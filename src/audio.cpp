#include "btas.hpp"
#include "conf.hpp"
#include "fs.hpp"
#include "hook.hpp"
#include "mem.hpp"
#include "utils.hpp"
#include <Windows.h>
#include <algorithm>
#include <cstdint>
#include <dsound.h>
#include <iostream>
#include <string>
#include <vector>

using std::cout;
using std::string;

class CriticalSectionLock {
    CRITICAL_SECTION* m_cs;

public:
    CriticalSectionLock(CRITICAL_SECTION& cs) : m_cs(&cs) { EnterCriticalSection(m_cs); }
    ~CriticalSectionLock() { LeaveCriticalSection(m_cs); }
};

#pragma pack(push, 1)
struct WavHeader {
    char riff[4];
    uint32_t fileSize;
    char wave[4];
    char fmt[4];
    uint32_t fmtLen;
    uint16_t formatTag;
    uint16_t channels;
    uint32_t sampleRate;
    uint32_t byteRate;
    uint16_t blockAlign;
    uint16_t bitsPerSample;
    char data[4];
    uint32_t dataLen;

    WavHeader() {
        std::memcpy(riff, "RIFF", 4);
        fileSize = 0;
        std::memcpy(wave, "WAVE", 4);
        std::memcpy(fmt, "fmt ", 4);
        fmtLen = 16;
        formatTag = 1;
        channels = 0;
        sampleRate = 0;
        byteRate = 0;
        blockAlign = 0;
        bitsPerSample = 0;
        std::memcpy(data, "data", 4);
        dataLen = 0;
    }
};
#pragma pack(pop)

struct AudioEvent {
    ULONG timeOffset;
    LONG volume;
    DWORD frequency;
};

struct AudioCapture {
    std::vector<AudioEvent> events;
    uint64_t hash;
    ULONG startTime;
    ULONG endTime;

    AudioCapture() : startTime(0), endTime(0), hash(0) {}
};

class IDSBProxy;
static CRITICAL_SECTION g_audioCS;
static std::vector<AudioCapture> history;
static std::vector<IDSBProxy*> cache;
static ULONG last_time_offset;
static int last_uid;
static int fn_counter;

static uint64_t hash_vector(const std::vector<uint8_t>& vec) {
    const uint32_t c1 = 0xcc9e2d51;
    const uint32_t c2 = 0x1b873593;

    uint32_t h1 = 0x9e3779b9;

    const uint8_t* data = &vec[0];
    const size_t len = vec.size();
    const size_t nblocks = len / 4;

    const uint32_t* blocks = reinterpret_cast<const uint32_t*>(data);
    for (size_t i = 0; i < nblocks; ++i) {
        uint32_t k1 = blocks[i];

        k1 *= c1;
        k1 = (k1 << 15) | (k1 >> 17);
        k1 *= c2;

        h1 ^= k1;
        h1 = (h1 << 13) | (h1 >> 19);
        h1 = h1 * 5 + 0xe6546b64;
    }

    const uint8_t* tail = data + nblocks * 4;
    uint32_t k1 = 0;

    switch (len & 3) {
    case 3:
        k1 ^= static_cast<uint32_t>(tail[2]) << 16;
    case 2:
        k1 ^= static_cast<uint32_t>(tail[1]) << 8;
    case 1:
        k1 ^= static_cast<uint32_t>(tail[0]);
        k1 *= c1;
        k1 = (k1 << 15) | (k1 >> 17);
        k1 *= c2;
        h1 ^= k1;
    };

    h1 ^= len;
    h1 ^= h1 >> 16;
    h1 *= 0x85ebca6b;
    h1 ^= h1 >> 13;
    h1 *= 0xc2b2ae35;
    h1 ^= h1 >> 16;

    return static_cast<size_t>(h1);
}

inline unsigned long audio_get_time() { return btas::get_time() - last_time_offset; }

class IDSBProxy : public IDirectSoundBuffer {
    WavHeader header;
    AudioCapture cap;
    std::vector<uint8_t> audioBuffer;
    double virtualTimeAcc;
    IDirectSoundBuffer* pBuf;
    ULONG lastRealTime;
    DWORD currentFreq;
    DWORD originalFreq;
    bool inited;

    void push_event() {
        DWORD newFreq;
        LONG vol;
        pBuf->GetFrequency(&newFreq);
        pBuf->GetVolume(&vol);

        ULONG now = audio_get_time();

        if (cap.events.empty()) {
            cap.startTime = now;
            lastRealTime = now;
            virtualTimeAcc = 0.0;
            currentFreq = newFreq;
            originalFreq = newFreq;
        } else {
            ULONG deltaReal = now - lastRealTime;
            double scale =
                (originalFreq > 0.0) ? (static_cast<double>(currentFreq) / originalFreq) : 1.0;
            virtualTimeAcc += static_cast<double>(deltaReal) * scale;
            lastRealTime = now;
            currentFreq = newFreq;
        }

        ULONG offset = static_cast<ULONG>(virtualTimeAcc);
        if (!cap.events.empty()) {
            auto& last = cap.events.back();
            if (last.timeOffset == offset) {
                last.volume = vol;
                last.frequency = newFreq;
                return;
            }
            if (last.frequency == newFreq && last.volume == vol)
                return;
        }
        AudioEvent ev;
        ev.timeOffset = offset;
        ev.volume = vol;
        ev.frequency = newFreq;
        cap.events.push_back(ev);
    }

    void reinit_wav() {
        auto cur_time = audio_get_time();
        audioBuffer.clear();
        inited = true;
        cap.startTime = cap.endTime = cur_time;
        cap.events.clear();
        push_event();
    }

public:
    void finalize_wav() {
        if (inited) {
            inited = false;
            ULONG now = audio_get_time();
            double scale =
                (originalFreq > 0.0) ? (static_cast<double>(currentFreq) / originalFreq) : 1.0;
            virtualTimeAcc += static_cast<double>(now - lastRealTime) * scale;
            if (virtualTimeAcc <= 0.0) {
                // cout << "audio got virtualTimeAcc <= 0" << std::endl;
                return;
            }
            cap.endTime = cap.startTime + static_cast<ULONG>(virtualTimeAcc);
            if (cap.endTime > cap.startTime && !audioBuffer.empty()) {
                cap.hash = hash_vector(audioBuffer);
                size_t validSize = (audioBuffer.size() / header.blockAlign) * header.blockAlign;
                ASS(validSize == audioBuffer.size());
                header.fileSize = validSize + 36;
                header.dataLen = validSize;

                string path = "temp_audio\\a" + to_str(cap.hash) + ".wav";
                if (!file_exists(path)) {
                    bfs::File file(path, 1);
                    if (file.is_open()) {
                        file.write(&header, sizeof(WavHeader));
                        file.write(audioBuffer.data(), audioBuffer.size());
                        file.close();
                        history.push_back(cap);
                    }
                } else {
                    // cout << "audio skipped duplicate\n";
                    history.push_back(cap);
                }
                return;
            }
            // cout << "audio got endTime <= startTime" << std::endl;
        }
    }

    IDSBProxy(IDirectSoundBuffer* pReal, LPCDSBUFFERDESC desc) {
        pBuf = pReal;
        header.sampleRate = desc->lpwfxFormat->nSamplesPerSec;
        header.channels = desc->lpwfxFormat->nChannels;
        header.bitsPerSample = desc->lpwfxFormat->wBitsPerSample;
        header.blockAlign = desc->lpwfxFormat->nBlockAlign;
        header.byteRate = desc->lpwfxFormat->nAvgBytesPerSec;
        originalFreq = desc->lpwfxFormat->nSamplesPerSec;
        currentFreq = originalFreq;
        lastRealTime = 0;
        virtualTimeAcc = 0.0;
        inited = false;
        reinit_wav();
        CriticalSectionLock lock(g_audioCS);
        cache.push_back(this);
    }
    STDMETHOD(QueryInterface)(REFIID riid, void** ppvObj) override {
        return pBuf->QueryInterface(riid, ppvObj);
    }
    STDMETHOD_(ULONG, AddRef)() override { return pBuf->AddRef(); }
    STDMETHOD_(ULONG, Release)() override {
        ULONG count = pBuf->Release();
        if (count == 0) {
            // CriticalSectionLock lock(g_audioCS);
            // unsafe but otherwise softlocks
            auto it = std::find(cache.begin(), cache.end(), this);
            ASS(it != cache.end());
            cache.erase(it);
            delete this;
        }
        return count;
    }
    STDMETHOD(GetCaps)(LPDSBCAPS pDSBCaps) override { return pBuf->GetCaps(pDSBCaps); }
    STDMETHOD(GetCurrentPosition)(LPDWORD pdwCurrentPlayCursor,
                                  LPDWORD pdwCurrentWriteCursor) override {
        return pBuf->GetCurrentPosition(pdwCurrentPlayCursor, pdwCurrentWriteCursor);
    }
    STDMETHOD(GetFormat)(LPWAVEFORMATEX pwfxFormat, DWORD dwSizeAllocated,
                         LPDWORD pdwSizeWritten) override {
        return pBuf->GetFormat(pwfxFormat, dwSizeAllocated, pdwSizeWritten);
    }
    STDMETHOD(GetVolume)(LPLONG plVolume) override { return pBuf->GetVolume(plVolume); }
    STDMETHOD(GetPan)(LPLONG plPan) override { return pBuf->GetPan(plPan); }
    STDMETHOD(GetFrequency)(LPDWORD pdwFrequency) override {
        return pBuf->GetFrequency(pdwFrequency);
    }
    STDMETHOD(GetStatus)(LPDWORD pdwStatus) override { return pBuf->GetStatus(pdwStatus); }

    STDMETHOD(Initialize)(LPDIRECTSOUND pDirectSound, LPCDSBUFFERDESC pcDSBufferDesc) override {
        return pBuf->Initialize(pDirectSound, pcDSBufferDesc);
    }
    STDMETHOD(Lock)(DWORD dwOffset, DWORD dwBytes, LPVOID* ppvAudioPtr1, LPDWORD pdwAudioBytes1,
                    LPVOID* ppvAudioPtr2, LPDWORD pdwAudioBytes2, DWORD dwFlags) override {
        return pBuf->Lock(dwOffset, dwBytes, ppvAudioPtr1, pdwAudioBytes1, ppvAudioPtr2,
                          pdwAudioBytes2, dwFlags);
    }
    STDMETHOD(Play)(DWORD dwReserved1, DWORD dwPriority, DWORD dwFlags) override {
        // cout << "DirectSoundBuffer::Play\n";
        return pBuf->Play(dwReserved1, dwPriority, dwFlags);
    }
    STDMETHOD(SetCurrentPosition)(DWORD dwNewPosition) override {
        auto ret = pBuf->SetCurrentPosition(dwNewPosition);
        // cout << "DirectSoundBuffer::SetCurrentPosition\n";
        return ret;
    }
    STDMETHOD(SetFormat)(LPCWAVEFORMATEX pcfxFormat) override {
        return pBuf->SetFormat(pcfxFormat);
    }
    STDMETHOD(SetFrequency)(DWORD f) override {
        HRESULT hr = pBuf->SetFrequency(f);
        CriticalSectionLock lock(g_audioCS);
        push_event();
        return hr;
    }
    STDMETHOD(SetVolume)(LONG v) override {
        HRESULT hr = pBuf->SetVolume(v);
        CriticalSectionLock lock(g_audioCS);
        push_event();
        return hr;
    }
    STDMETHOD(SetPan)(LONG p) override { return pBuf->SetPan(p); }
    STDMETHOD(Stop)() override {
        CriticalSectionLock lock(g_audioCS);
        finalize_wav();
        // cout << "DirectSoundBuffer::Stop\n";
        return pBuf->Stop();
    }
    STDMETHOD(Unlock)(LPVOID pv1, DWORD db1, LPVOID pv2, DWORD db2) override {
        CriticalSectionLock lock(g_audioCS);
        if (!inited) {
            reinit_wav();
            return pBuf->Unlock(pv1, db1, pv2, db2);
        }
        if (pv1 && db1 > 0)
            audioBuffer.insert(audioBuffer.end(), reinterpret_cast<uint8_t*>(pv1),
                               reinterpret_cast<uint8_t*>(pv1) + db1);
        if (pv2 && db2 > 0)
            audioBuffer.insert(audioBuffer.end(), reinterpret_cast<uint8_t*>(pv2),
                               reinterpret_cast<uint8_t*>(pv2) + db2);
        return pBuf->Unlock(pv1, db1, pv2, db2);
    }
    STDMETHOD(Restore)() override { return pBuf->Restore(); }
};

class IDSProxy : public IDirectSound {
    IDirectSound* pDev;

public:
    IDSProxy(IDirectSound* pReal) : pDev(pReal) {}
    virtual ~IDSProxy() {}
    STDMETHOD(QueryInterface)(REFIID riid, void** ppvObj) override {
        return pDev->QueryInterface(riid, ppvObj);
    }
    STDMETHOD_(ULONG, AddRef)() override { return pDev->AddRef(); }
    STDMETHOD_(ULONG, Release)() override {
        ULONG count = pDev->Release();
        // Again, IDSProxy should be deleted manually
        if (count == 0)
            delete this;
        return count;
    }
    STDMETHOD(CreateSoundBuffer)(LPCDSBUFFERDESC pcDSBufferDesc, LPDIRECTSOUNDBUFFER* ppDSBuffer,
                                 LPUNKNOWN pUnkOuter) override {
        HRESULT hr = pDev->CreateSoundBuffer(pcDSBufferDesc, ppDSBuffer, pUnkOuter);
        if (SUCCEEDED(hr) && ppDSBuffer && *ppDSBuffer && pcDSBufferDesc->lpwfxFormat) {
            // cout << "wrapping IDirectSoundBuffer into IDSBProxy\n";
            *ppDSBuffer = new IDSBProxy(*ppDSBuffer, pcDSBufferDesc);
        }
        return hr;
    }
    STDMETHOD(GetCaps)(LPDSCAPS pDSCaps) override { return pDev->GetCaps(pDSCaps); }
    STDMETHOD(DuplicateSoundBuffer)(LPDIRECTSOUNDBUFFER pDSBufferOriginal,
                                    LPDIRECTSOUNDBUFFER* ppDSBufferDuplicate) override {
        ASS(false);
        return DSERR_OUTOFMEMORY;
    }
    STDMETHOD(SetCooperativeLevel)(HWND hwnd, DWORD dwLevel) override {
        return pDev->SetCooperativeLevel(hwnd, dwLevel);
    }
    STDMETHOD(Compact)() override { return pDev->Compact(); }
    STDMETHOD(GetSpeakerConfig)(LPDWORD pdwSpeakerConfig) override {
        return pDev->GetSpeakerConfig(pdwSpeakerConfig);
    }
    STDMETHOD(SetSpeakerConfig)(DWORD dwSpeakerConfig) override {
        return pDev->SetSpeakerConfig(dwSpeakerConfig);
    }
    STDMETHOD(Initialize)(LPCGUID pcGuidDevice) override { return pDev->Initialize(pcGuidDevice); }
};

static HRESULT(WINAPI* DirectSoundCreateOrig)(LPCGUID guid, LPDIRECTSOUND* ds, LPUNKNOWN unk);
static HRESULT WINAPI DirectSoundCreateHook(LPCGUID guid, LPDIRECTSOUND* ds, LPUNKNOWN unk) {
    if (conf.no_au)
        return DSERR_NODRIVER;
    HRESULT hr = DirectSoundCreateOrig(guid, ds, unk);
    if (SUCCEEDED(hr) && ds && *ds) {
        cout << "wrapping IDirectSound into IDSProxy\n";
        *ds = new IDSProxy(*ds);
        ASS(*ds != nullptr);
    }
    return hr;
}

void audio_init() {
    if (!conf.cap_au && !conf.no_au)
        return;
    last_time_offset = 0;
    fn_counter = 0;
    InitializeCriticalSection(&g_audioCS);
    // Let's use minhook here because the game uses ordinal import
    hook(mem::addr("DirectSoundCreate", "dsound.dll"), DirectSoundCreateHook,
         &DirectSoundCreateOrig);
}

static void on_audio_auto_destroy() {
    CriticalSectionLock lock(g_audioCS);
    if (history.empty())
        return;

    bfs::File filters("temp_audio\\audio_filters" + to_str(fn_counter) + ".txt", 1);
    ASS(filters.is_open());
    string mix = "";
    size_t count = 0;

    for (auto it = history.begin(); it != history.end(); it++) {
        auto& c = *it;
        string finalLabel = "[f" + to_str(count) + "]";
        double totalDuration = static_cast<double>(c.endTime - c.startTime) / 1000.0;
        ASS(!c.events.empty());
        size_t numSegs = c.events.size();
        filters.write("amovie=a" + to_str(c.hash) + ".wav,asplit=" + to_str(numSegs));
        for (size_t e = 0; e < numSegs; e++)
            filters.write("[b" + to_str(count) + "s" + to_str(e) + "]");
        filters.write_line(";");
        string segmentLabels = "";
        for (size_t e = 0; e < c.events.size(); e++) {
            string branchIn = "[b" + to_str(count) + "s" + to_str(e) + "]";
            string branchOut = "[p" + to_str(count) + "s" + to_str(e) + "]";

            double start = static_cast<double>(c.events[e].timeOffset) / 1000.0;
            double end = (e + 1 < c.events.size())
                             ? static_cast<double>(c.events[e + 1].timeOffset) / 1000.0
                             : totalDuration;
            double volLinear = std::pow(10.0, static_cast<double>(c.events[e].volume) / 2000.0);

            filters.write_line(branchIn + "atrim=start=" + to_str(start) + ":end=" + to_str(end) +
                               ",asetrate=" + to_str(c.events[e].frequency) + ",volume=" +
                               to_str(volLinear) + ",aresample=48000" + branchOut + ";");

            segmentLabels += branchOut;
        }
        filters.write_line(segmentLabels + "concat=n=" + to_str(numSegs) +
                           ":v=0:a=1,adelay=" + to_str(c.startTime) + ":all=1" + finalLabel + ";");
        mix += finalLabel;
        count++;
    }
    filters.write(mix + "amix=inputs=" + to_str(count) + ":normalize=0[mixed];");
    // Fill audio with nothing to exactly match needed duration
    unsigned long cur_time = audio_get_time();
    string target_duration = to_str(static_cast<double>(cur_time) / 1000.0);
    filters.write_line("[mixed]apad=whole_dur=" + target_duration + "[out]");
    filters.close();
    bfs::File bat("temp_audio\\audio_merge" + to_str(fn_counter) + ".bat", 1);
    bat.write_line("@echo off");
    // IDK why it requires trimming (-t) when audio restarting is disabled
    bat.write_line("ffmpeg -y -/filter_complex audio_filters" + to_str(fn_counter) +
                   ".txt -map \"[out]\" -ac 2 -ar 48000 -t " + target_duration + " output" +
                   to_str(fn_counter) + ".wav");
    history.clear();
    last_time_offset += cur_time;
    fn_counter++;
#ifdef _DEBUG
    cout << "audio was split\n";
#endif
}

void on_audio_destroy() {
    if (!conf.cap_au)
        return;
    if (1) {
        CriticalSectionLock lock(g_audioCS);
        for (auto it = cache.begin(); it != cache.end(); it++)
            (*it)->finalize_wav();
    }
    on_audio_auto_destroy();
    is_paused = true;
    conf.cap_au = false;
    bfs::File inputs("temp_audio\\audio_inputs.txt", 1);
    ASS(inputs.is_open());
    for (int i = 0; i < fn_counter; i++)
        inputs.write_line("file 'output" + to_str(i) + ".wav'");
    inputs.close();
    bfs::File bat("audio_merge.bat", 1);
    ASS(bat.is_open());
    bat.write_line("@echo off");
    bat.write_line("cd temp_audio");
    for (int i = 0; i < fn_counter; i++)
        bat.write_line("call audio_merge" + to_str(i) + ".bat");
    bat.write_line("ffmpeg -f concat -i audio_inputs.txt -c copy ../output.wav");
    bat.write_line("cd ..");
    bat.write_line("echo Waiting to delete cache...");
    bat.write_line("pause");
    bat.write_line("del temp_audio\\audio_inputs.txt");
    bat.write_line("del temp_audio\\audio_filters*.txt");
    bat.write_line("del temp_audio\\audio_merge*.bat");
    bat.write_line("del temp_audio\\a*.wav");
    bat.write_line("del temp_audio\\output*.wav");
}

void on_audio_change() {
    if (!conf.cap_au)
        return;
    ASS(cache.size() == 0);
    if (cache.size() == 0)
        on_audio_auto_destroy();
#ifdef _DEBUG
    else
        cout << "WARN: audio was not split because cache was not empty: " << cache.size()
             << std::endl;
#endif
}
