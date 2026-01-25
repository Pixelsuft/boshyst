#include <Windows.h>
#include <dsound.h>
#include "mem.hpp"
#include "ass.hpp"
#include "btas.hpp"
#include "hook.hpp"
#include "fs.hpp"
#include "utils.hpp"
#include "conf.hpp"
#include <iostream>
#include <map>
#include <string>
#include <vector>
#include <cstdint>

// TODO: fix frequency set

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
        memcpy(riff, "RIFF", 4);
        fileSize = 0;
        memcpy(wave, "WAVE", 4);
        memcpy(fmt, "fmt ", 4);
        fmtLen = 16;
        formatTag = 1;
        channels = 0;
        sampleRate = 0;
        byteRate = 0;
        blockAlign = 0;
        bitsPerSample = 0;
        memcpy(data, "data", 4);
        dataLen = 0;
    }
};
#pragma pack(pop)

struct AudioEvent {
    unsigned long timeOffset; // Relative to cap.startTime
    DWORD frequency;
    long volume;

    AudioEvent(unsigned long to, DWORD f, long v) : timeOffset(to), frequency(f), volume(v) {}
};

struct AudioCapture {
    bfs::File file;
    WavHeader h;
    unsigned long startTime;
    unsigned long endTime;
    uint32_t bytesWritten;
    int idx;
    std::vector<AudioEvent> events;
    DWORD lastFreq;
    DWORD sampleRateOrig;
    long lastVol;

    AudioCapture() : startTime(0), endTime(0), bytesWritten(0), idx(0), lastFreq(0), lastVol(0), sampleRateOrig(0) {}
    AudioCapture(string fn) : file(fn, 1), startTime(0), endTime(0), bytesWritten(0), idx(0), lastFreq(0), lastVol(0), sampleRateOrig(0) {}
};

static std::map<IDirectSoundBuffer*, AudioCapture> g_captures;
static std::vector<AudioCapture> g_history; // History for on_audio_destroy
static CRITICAL_SECTION g_audioCS;

typedef HRESULT(WINAPI* DirectSoundCreate_t)(LPCGUID, LPDIRECTSOUND*, LPUNKNOWN);
typedef HRESULT(STDMETHODCALLTYPE* CreateSoundBuffer_t)(IDirectSound*, LPCDSBUFFERDESC, LPDIRECTSOUNDBUFFER*, LPUNKNOWN);
typedef HRESULT(STDMETHODCALLTYPE* Unlock_t)(IDirectSoundBuffer*, LPVOID, DWORD, LPVOID, DWORD);
typedef int(__fastcall* tApplyFrequencyToBuffer)(IDirectSoundBuffer**, void*, DWORD);
typedef int(__fastcall* tApplyVolumeToBuffer)(IDirectSoundBuffer**, void*, long);
typedef int(__fastcall* tStopHardwareBuffer)(IDirectSoundBuffer**, void*);

// static BOOL (__stdcall* SetFileInformationByHandlePtr)(HANDLE, FILE_INFO_BY_HANDLE_CLASS, LPVOID, DWORD);
static DirectSoundCreate_t fpDirectSoundCreate = nullptr;
static CreateSoundBuffer_t fpCreateSoundBuffer = nullptr;
static tApplyFrequencyToBuffer fpApplyFrequencyToBuffer = nullptr;
static tApplyVolumeToBuffer fpApplyVolumeToBuffer = nullptr;
static Unlock_t fpUnlock = nullptr;
static tStopHardwareBuffer fpStopHardwareBuffer = nullptr;
static int last_uid = 0;
static unsigned long last_time = 0;

static int gen_uid(unsigned long mytime) {
    if (mytime != last_time) {
        last_time = mytime;
        last_uid = 0;
    }
    return last_uid++;
}

static unsigned long audio_get_time() {
    return btas::get_hg_time();
}

static void setup_fixed_header(WavHeader& h, uint16_t channels, uint16_t bits) {
    h.channels = channels;
    h.sampleRate = 48000;
    h.bitsPerSample = bits;
    h.blockAlign = (channels * bits) / 8;
    h.byteRate = h.sampleRate * h.blockAlign;
}

static void record_event(AudioCapture& cap, DWORD freq, long vol) {
    unsigned long cur = audio_get_time();
    unsigned long offset = (cur > cap.startTime) ? (cur - cap.startTime) : 0;
    if (freq != cap.lastFreq || vol != cap.lastVol) {
        cap.events.push_back(AudioEvent(offset, freq, vol));
        cap.lastFreq = freq;
        cap.lastVol = vol;
    }
}

static void write_original_raw(AudioCapture& cap, const char* data, uint32_t len) {
    cap.file.write(data, len);
    cap.bytesWritten += len;
}

static void finalize_wav(AudioCapture& cap) {
    if (cap.file.is_open()) {
        cap.endTime = audio_get_time();
        uint32_t finalFileSize = cap.bytesWritten + 36;

        cap.file.seek(4, bfs::SeekBegin);
        cap.file.write((char*)&finalFileSize, 4);
        cap.file.seek(24, bfs::SeekBegin);
        cap.file.write((char*)&cap.h.sampleRate, 4); // Write ORIGINAL rate
        cap.file.seek(28, bfs::SeekBegin);
        cap.file.write((char*)&cap.h.byteRate, 4);   // Write ORIGINAL byte rate
        cap.file.seek(40, bfs::SeekBegin);
        cap.file.write((char*)&cap.bytesWritten, 4);
        cap.file.close();

        g_history.push_back(std::move(cap));
    }
}

void on_audio_destroy() {
    if (!conf::cap_au)
        return;
    CriticalSectionLock lock(g_audioCS);
    for (auto it = g_captures.begin(); it != g_captures.end(); it++)
        finalize_wav(it->second);
    g_captures.clear();

    if (g_history.empty()) return;

    bfs::File filterFile("temp_filters.txt", 1);
    bfs::File batFile("amerge.bat", 1);

    std::string filters = "";
    std::string mix = "";

    for (size_t i = 0; i < g_history.size(); ++i) {
        auto& c = g_history[i];
        std::string fn = "audio_" + to_str(c.startTime) + "_" + to_str(c.idx) + ".wav";
        std::string finalLabel = "[final" + to_str(i) + "]";
        double totalDuration = (c.endTime > c.startTime) ? (double)(c.endTime - c.startTime) / 1000.0 : 0.0;

        if (c.events.empty()) {
            // Case 1: Simple file
            filters += "amovie=" + fn + ",atrim=duration=" + to_str(totalDuration) +
                ",aresample=48000,adelay=" + to_str(c.startTime) + ":all=1" + finalLabel + ";\n";
        }
        else {
            // Case 2: Frequency segments
            int numSegs = (int)c.events.size();

            // 1. Load and split source
            filters += "amovie=" + fn + ",asplit=" + to_str(numSegs);
            for (int e = 0; e < numSegs; ++e) {
                filters += "[b" + to_str(i) + "s" + to_str(e) + "]";
            }
            filters += ";\n";

            // 2. Process each split branch
            std::string segmentLabels = "";
            for (size_t e = 0; e < c.events.size(); ++e) {
                std::string branchIn = "[b" + to_str(i) + "s" + to_str(e) + "]";
                std::string branchOut = "[p" + to_str(i) + "s" + to_str(e) + "]";

                double start = (double)c.events[e].timeOffset / 1000.0;
                double end = (e + 1 < c.events.size()) ? (double)c.events[e + 1].timeOffset / 1000.0 : totalDuration;
                double volLinear = pow(10.0, (double)c.events[e].volume / 2000.0);

                filters += branchIn + "atrim=start=" + to_str(start) + ":end=" + to_str(end) +
                    ",asetrate=" + to_str(c.events[e].frequency) +
                    ",volume=" + to_str(volLinear) +
                    ",aresample=48000" + branchOut + ";\n";

                segmentLabels += branchOut;
            }

            // 3. Concat branches and apply delay
            filters += segmentLabels + "concat=n=" + to_str(numSegs) + ":v=0:a=1,adelay=" +
                to_str(c.startTime) + ":all=1" + finalLabel + ";\n";
        }
        mix += finalLabel;
    }

    filters += mix + "amix=inputs=" + to_str(g_history.size()) + ":normalize=0[out]";

    // FFmpeg 2026 syntax: -/filter_complex reads from file
    std::string batContent = "@echo off\nffmpeg -y -/filter_complex temp_filters.txt -map \"[out]\" -ar 48000 output.wav\npause";

    filterFile.write(filters.c_str(), filters.size());
    batFile.write(batContent.c_str(), batContent.size());

    g_history.clear();
}

static void reinit_wav(AudioCapture& cap) {
    auto cur_time = audio_get_time();
    auto idx = gen_uid(cur_time);
    string fn = "audio_" + to_str(cur_time) + "_" + to_str(idx) + ".wav";
    cap.file = bfs::File(fn, 1);
    cap.file.write((char*)&cap.h, sizeof(WavHeader));
    cap.bytesWritten = 0;
    cap.startTime = cur_time;
    cap.idx = idx;
    cap.events.clear();
    cap.events.push_back(AudioEvent(0, cap.lastFreq, cap.lastVol)); // Hacky
    // cout << "reinit " << cap.h.sampleRate << " " << cap.lastVol << "\n";
}

void audio_stop() {
    // User stops audio or scene changes/reset
    if (!conf::cap_au)
        return;
    CriticalSectionLock lock(g_audioCS);
    for (auto it = g_captures.begin(); it != g_captures.end(); it++)
        finalize_wav(it->second);
    g_captures.clear();
}

static int __fastcall hkApplyFrequencyToBuffer(IDirectSoundBuffer** pThis, void* edx, DWORD freq) {
    // IDK but hooking dsound directly doesnt work
    CriticalSectionLock lock(g_audioCS);
    auto it = g_captures.find(*pThis);
    if (it != g_captures.end()) {
        DWORD targetFreq = freq;
        if (targetFreq == 0) {
            targetFreq = it->second.sampleRateOrig;
            // cout << "reset to " << targetFreq << "\n";
        }
        if (targetFreq >= 100 && targetFreq <= 100000) {
            record_event(it->second, targetFreq, it->second.lastVol);
        }
    }

    return fpApplyFrequencyToBuffer(pThis, edx, freq);
}

static int __fastcall hkApplyVolumeToBuffer(IDirectSoundBuffer** pThis, void* edx, long volume) {
    CriticalSectionLock lock(g_audioCS);
    auto it = g_captures.find(*pThis);
    if (it != g_captures.end()) {
        record_event(it->second, it->second.lastFreq, volume);
    }
    return fpApplyVolumeToBuffer(pThis, edx, volume);
}

static int __fastcall hkStopHardwareBuffer(IDirectSoundBuffer** pThis, void* edx) {
    CriticalSectionLock lock(g_audioCS);
    auto it = g_captures.find(*pThis);
    if (it != g_captures.end()) {
        // cout << "hkStopHardwareBuffer\n";
        finalize_wav(it->second);
        // cout << "stop\n";
    }
    return fpStopHardwareBuffer(pThis, edx);
}

static void __fastcall hkReleaseHardwareBuffer(IDirectSoundBuffer** pThis, void* edx) {
    CriticalSectionLock lock(g_audioCS);
    auto it = g_captures.find(*pThis);
    if (it != g_captures.end()) {
        finalize_wav(it->second);
        // cout << "release\n";
        g_captures.erase(it);
    }
    *pThis = nullptr;
}

static HRESULT STDMETHODCALLTYPE DetourUnlock(IDirectSoundBuffer* pThis, LPVOID pv1, DWORD db1, LPVOID pv2, DWORD db2) {
    CriticalSectionLock lock(g_audioCS);
    auto it = g_captures.find(pThis);
    if (it != g_captures.end()) {
        if (!it->second.file.is_open()) {
            reinit_wav(it->second);
            return fpUnlock(pThis, pv1, db1, pv2, db2);
        }
        if (pv1 && db1 > 0) write_original_raw(it->second, (char*)pv1, db1);
        if (pv2 && db2 > 0) write_original_raw(it->second, (char*)pv2, db2);
    }
    return fpUnlock(pThis, pv1, db1, pv2, db2);
}

static HRESULT STDMETHODCALLTYPE DetourCreateSoundBuffer(IDirectSound* pThis, LPCDSBUFFERDESC desc, LPDIRECTSOUNDBUFFER* buffer, LPUNKNOWN unk) {
    HRESULT hr = fpCreateSoundBuffer(pThis, desc, buffer, unk);
    if (SUCCEEDED(hr) && buffer && *buffer && desc->lpwfxFormat) {
        CriticalSectionLock lock(g_audioCS);
        AudioCapture cap;
        cap.sampleRateOrig = desc->lpwfxFormat->nSamplesPerSec;
        cap.h.sampleRate = desc->lpwfxFormat->nSamplesPerSec;
        cap.h.channels = desc->lpwfxFormat->nChannels;
        cap.h.bitsPerSample = desc->lpwfxFormat->wBitsPerSample;
        cap.h.blockAlign = desc->lpwfxFormat->nBlockAlign;
        cap.h.byteRate = desc->lpwfxFormat->nAvgBytesPerSec;
        cap.lastFreq = cap.h.sampleRate;
        reinit_wav(cap);
        g_captures[*buffer] = std::move(cap);
        if (fpUnlock == nullptr) {
            cout << "audio enabling hooks 2\n";
            void* target = (*(void***)*buffer)[19]; // Unlock
            hook(target, DetourUnlock, &fpUnlock);
            enable_hook(target);
        }
    }
    return hr;
}

static HRESULT WINAPI DetourDirectSoundCreate(LPCGUID guid, LPDIRECTSOUND* ds, LPUNKNOWN unk) {
    if (conf::no_au)
        return DSERR_NODRIVER;
    HRESULT hr = fpDirectSoundCreate(guid, ds, unk);
    if (SUCCEEDED(hr) && ds && *ds && fpCreateSoundBuffer == nullptr) {
        cout << "audio enabling hooks 1\n";
        void* target = (*(void***)*ds)[3];
        hook(target, DetourCreateSoundBuffer, &fpCreateSoundBuffer);
        enable_hook(target);
        // Somewhy hooking freq from dsound doesnt work
        // Hooking more mmf2 funcs directly to avoid vtable shit
        target = (void*)(mem::get_base("mmfs2.dll") + 0x451d0);
        hook(target, hkApplyFrequencyToBuffer, &fpApplyFrequencyToBuffer);
        enable_hook(target);
        target = (void*)(mem::get_base("mmfs2.dll") + 0x45190);
        hook(target, hkApplyVolumeToBuffer, &fpApplyVolumeToBuffer);
        enable_hook(target);
        target = (void*)(mem::get_base("mmfs2.dll") + 0x45060);
        hook(target, hkStopHardwareBuffer, &fpStopHardwareBuffer);
        enable_hook(target);
        target = (void*)(mem::get_base("mmfs2.dll") + 0x45050);
        hook(target, hkReleaseHardwareBuffer);
        enable_hook(target);
    }
    return hr;
}

void audio_init() {
    if (!conf::cap_au && !conf::no_au)
        return;
    InitializeCriticalSection(&g_audioCS);
    /*
    auto handle = GetModuleHandleW(L"kernel32.dll");
    if (handle)
        SetFileInformationByHandlePtr = (decltype(SetFileInformationByHandlePtr))GetProcAddress(handle, "SetFileInformationByHandle");
    else
        SetFileInformationByHandlePtr = nullptr;
    */
    hook(mem::addr("DirectSoundCreate", "dsound.dll"), DetourDirectSoundCreate, &fpDirectSoundCreate);
}
