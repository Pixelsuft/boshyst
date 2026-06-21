#include "btas.hpp"
#include "conf.hpp"
#include "fs.hpp"
#include "hook.hpp"
#include "mem.hpp"
#include "utils.hpp"
#include <Windows.h>
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
    uint64_t timeOffset;
    LONG volume;
    DWORD frequency;
};

struct AudioCapture {
    uint64_t startTime;
    uint64_t endTime;
    std::vector<AudioEvent> events;
    int idx;

    AudioCapture() : startTime(0), endTime(0), idx(0) {}
};

class IDSBProxy;
static CRITICAL_SECTION g_audioCS;
static std::vector<AudioCapture> history;
static std::vector<IDSBProxy*> cache;
static uint64_t last_time;
static int last_uid;

static int gen_uid(uint64_t mytime) {
    if (mytime != last_time) {
        last_time = mytime;
        last_uid = 0;
    }
    return last_uid++;
}

inline unsigned long audio_get_time() { return btas::get_time(); }

inline string audio_get_fn(uint64_t a, int b) {
    return "a_" + to_str(a) + "_" + to_str(b) + ".wav";
}

class IDSBProxy : public IDirectSoundBuffer {
    AudioCapture cap;
    WavHeader header;
    uint64_t lastRealTime;
    double virtualTimeAcc;
    bfs::File file;
    IDirectSoundBuffer* pBuf;
    uint32_t bytesWritten;
    DWORD currentFreq;
    DWORD originalFreq;

    void push_event() {
        DWORD newFreq;
        LONG vol;
        pBuf->GetFrequency(&newFreq);
        pBuf->GetVolume(&vol);

        uint64_t now = audio_get_time();

        if (cap.events.empty()) {
            cap.startTime = now;
            lastRealTime = now;
            virtualTimeAcc = 0;
            currentFreq = newFreq;
            originalFreq = newFreq;
        } else {
            uint64_t deltaReal = now - lastRealTime;
            double scale =
                (originalFreq > 0.0) ? (static_cast<double>(currentFreq) / originalFreq) : 1.0;
            virtualTimeAcc += static_cast<double>(deltaReal) * scale;
            lastRealTime = now;
            currentFreq = newFreq;
        }

        uint64_t offset = static_cast<uint64_t>(virtualTimeAcc);
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
        auto idx = gen_uid(cur_time);
        file = bfs::File(string("temp_audio\\") + audio_get_fn(cur_time, idx), 1);
        file.write(&header, sizeof(WavHeader));
        bytesWritten = 0;
        cap.startTime = cap.endTime = cur_time;
        cap.idx = idx;
        cap.events.clear();
        push_event();
    }

public:
    void finalize_wav() {
        if (file.is_open()) {
            uint64_t now = audio_get_time();
            double scale = static_cast<double>(currentFreq) / static_cast<double>(originalFreq);
            virtualTimeAcc += static_cast<double>(now - lastRealTime) * scale;
            if (virtualTimeAcc <= 0.0) {
                cout << "audio got virtualTimeAcc <= 0" << std::endl;
                file.close();
                auto rem_ret =
                    remove_file(string("temp_audio\\") + audio_get_fn(cap.startTime, cap.idx));
                ASS(rem_ret);
                return;
            }
            cap.endTime = cap.startTime + static_cast<uint64_t>(virtualTimeAcc);
            uint32_t finalFileSize = bytesWritten + 36;
            file.seek(4, bfs::SeekBegin);
            file.write(&finalFileSize, 4);
            file.seek(24, bfs::SeekBegin);
            file.write(&header.sampleRate, 4);
            file.seek(28, bfs::SeekBegin);
            file.write(&header.byteRate, 4);
            file.seek(40, bfs::SeekBegin);
            file.write(&bytesWritten, 4);
            file.close();
            if (cap.endTime > cap.startTime) {
                history.push_back(cap);
                return;
            }
            cout << "audio got endTime <= startTime" << std::endl;
            auto rem_ret =
                remove_file(string("temp_audio\\") + audio_get_fn(cap.startTime, cap.idx));
            ASS(rem_ret);
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
        bytesWritten = 0;
        lastRealTime = 0;
        virtualTimeAcc = 0.0;
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
        if (!file.is_open()) {
            reinit_wav();
            return pBuf->Unlock(pv1, db1, pv2, db2);
        }
        if (pv1 && db1 > 0) {
            file.write(pv1, db1);
            bytesWritten += db1;
        }
        if (pv2 && db2 > 0) {
            file.write(pv2, db2);
            bytesWritten += db2;
        }
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
            cout << "wrapping IDirectSoundBuffer into IDSBProxy\n";
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

void audio_reinit_capture() {
    last_uid = 0;
    last_time = 0;
}

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
    InitializeCriticalSection(&g_audioCS);
    // Let's use minhook here because the game uses ordinal import
    hook(mem::addr("DirectSoundCreate", "dsound.dll"), DirectSoundCreateHook,
         &DirectSoundCreateOrig);
}

void on_audio_destroy() {
    if (!conf.cap_au)
        return;
    conf.cap_au = false;
    CriticalSectionLock lock(g_audioCS);
    for (auto it = cache.begin(); it != cache.end(); it++)
        (*it)->finalize_wav();
    if (history.empty())
        return;

    bfs::File filters("temp_audio\\audio_filters.txt", 1);
    string mix = "";
    size_t count = 0;

    for (auto it = history.begin(); it != history.end(); it++) {
        auto& c = *it;
        string finalLabel = "[f" + to_str(count) + "]";
        double totalDuration = static_cast<double>(c.endTime - c.startTime) / 1000.0;
        ASS(!c.events.empty());
        size_t numSegs = c.events.size();
        filters.write("amovie=" + audio_get_fn(c.startTime, c.idx) + ",asplit=" + to_str(numSegs));
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
    filters.write(mix + "amix=inputs=" + to_str(count) + ":normalize=0[out]");
    bfs::File bat("audio_merge.bat", 1);
    bat.write_line("@echo off");
    bat.write_line("cd temp_audio");
    bat.write_line(
        "ffmpeg -y -/filter_complex audio_filters.txt -map \"[out]\" -ar 48000 ../output.wav");
    bat.write_line("echo Waiting to delete wav cache...");
    bat.write_line("pause");
    bat.write_line("del a_*.wav");
    bat.write_line("del audio_filters.txt");
    history.clear();
}
