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

struct AudioCapture {
    bfs::File file;
    WavHeader h;
    double resampleRatio;
    double sampleAcc;
    unsigned long startTime;
    long volume;
    uint32_t bytesWritten;
    DWORD currentSrcFreq;
    int idx;
    
    AudioCapture() {
        resampleRatio = 1.0;
        sampleAcc = 0.0;
        currentSrcFreq = 0;
        bytesWritten = 0;
        startTime = 0;
        idx = 0;
        volume = 0;
    }

    AudioCapture(string fn) : file(fn, 1) {
        resampleRatio = 1.0;
        sampleAcc = 0.0;
        currentSrcFreq = 0;
        bytesWritten = 0;
        startTime = 0;
        idx = 0;
        volume = 0;
    }
};

static std::map<IDirectSoundBuffer*, AudioCapture> g_captures;
static CRITICAL_SECTION g_audioCS;

typedef HRESULT(WINAPI* DirectSoundCreate_t)(LPCGUID, LPDIRECTSOUND*, LPUNKNOWN);
typedef HRESULT(STDMETHODCALLTYPE* CreateSoundBuffer_t)(IDirectSound*, LPCDSBUFFERDESC, LPDIRECTSOUNDBUFFER*, LPUNKNOWN);
typedef HRESULT(STDMETHODCALLTYPE* Unlock_t)(IDirectSoundBuffer*, LPVOID, DWORD, LPVOID, DWORD);
typedef int(__fastcall* tApplyFrequencyToBuffer)(IDirectSoundBuffer**, void*, DWORD);
typedef int(__fastcall* tApplyVolumeToBuffer)(IDirectSoundBuffer**, void*, long);
typedef int(__fastcall* tStopHardwareBuffer)(IDirectSoundBuffer**, void*);
typedef int(__fastcall* tResetBufferPosition)(IDirectSoundBuffer**, void*);

// static BOOL (__stdcall* SetFileInformationByHandlePtr)(HANDLE, FILE_INFO_BY_HANDLE_CLASS, LPVOID, DWORD);
static DirectSoundCreate_t fpDirectSoundCreate = nullptr;
static CreateSoundBuffer_t fpCreateSoundBuffer = nullptr;
static tApplyFrequencyToBuffer fpApplyFrequencyToBuffer = nullptr;
static tApplyVolumeToBuffer fpApplyVolumeToBuffer = nullptr;
static Unlock_t fpUnlock = nullptr;
static tStopHardwareBuffer fpStopHardwareBuffer = nullptr;
static tResetBufferPosition fpResetBufferPosition = nullptr;
static int last_uid = 0;
static unsigned long last_time = 0;

static int gen_uid(unsigned long mytime) {
    if (mytime != last_time) {
        last_time = mytime;
        last_uid = 0;
    }
    return last_uid++;
}

static void setup_fixed_header(WavHeader& h, uint16_t channels, uint16_t bits) {
    h.channels = channels;
    h.sampleRate = 48000;
    h.bitsPerSample = bits;
    h.blockAlign = (channels * bits) / 8;
    h.byteRate = h.sampleRate * h.blockAlign;
}

static void write_resampled(AudioCapture& cap, const char* data, uint32_t len) {
    if (cap.h.bitsPerSample != 16) {
        cap.file.write(data, len);
        cap.bytesWritten += len;
        return;
    }
    int16_t* src = (int16_t*)data;
    int numSamples = len / cap.h.blockAlign;
    int chans = cap.h.channels;

    std::vector<int16_t> outputBuffer;
    outputBuffer.reserve((int)(numSamples * cap.resampleRatio) * chans);

    while (cap.sampleAcc < numSamples) {
        int srcIdx = (int)cap.sampleAcc;
        for (int c = 0; c < chans; c++) {
            outputBuffer.push_back(src[srcIdx * chans + c]);
        }
        cap.sampleAcc += (1.0 / cap.resampleRatio);
    }
    cap.sampleAcc -= numSamples;
    if (!outputBuffer.empty()) {
        uint32_t outSize = outputBuffer.size() * sizeof(int16_t);
        cap.file.write((char*)outputBuffer.data(), outSize);
        cap.bytesWritten += outSize;
    }
}

static void finalize_wav(AudioCapture& cap) {
    if (cap.file.is_open()) {
        unsigned long currentTime = btas::get_time();
        unsigned long elapsedMs = (currentTime > cap.startTime) ? (currentTime - cap.startTime) : 0;
        if (cap.bytesWritten == 0 || elapsedMs == 0) {
            // Delete empty file
            string old_filename = "audio_" + to_str(cap.startTime) + "_0_" + to_str(cap.idx) + ".wav";
            /*
            FILE_DISPOSITION_INFO disInfo;
            ZeroMemory(&disInfo, sizeof(FILE_DISPOSITION_INFO));
            disInfo.DeleteFile = TRUE;
            if (SetFileInformationByHandlePtr)
                ASS(SetFileInformationByHandlePtr((HANDLE)cap.file.get_handle(), FileDispositionInfo, &disInfo, sizeof(disInfo)));
            */
            cap.file.close();
            ASS(DeleteFileA(old_filename.c_str()));
            return;
        }
        // Exact byte count required for this duration
        uint32_t targetBytes = (uint32_t)((uint64_t)elapsedMs * cap.h.byteRate / 1000);
        if (cap.bytesWritten > targetBytes) {
            // TRIM: The game produced too much audio (ahead of TAS time)
            cap.bytesWritten = targetBytes;
        }
        else if (cap.bytesWritten < targetBytes) {
            // PAD: The game lagged/produced too little audio (behind TAS time)
            uint32_t padding = targetBytes - cap.bytesWritten;
            std::vector<char> silence(padding, 0);
            cap.file.write(silence.data(), padding);
            cap.bytesWritten += padding;
        }
        // Finalize headers and physically truncate the file
        uint32_t finalFileSize = cap.bytesWritten + 36;
        cap.file.seek(4, bfs::SeekBegin);
        cap.file.write((char*)&finalFileSize, 4);
        cap.file.seek(24, bfs::SeekBegin);
        cap.file.write((char*)&cap.h.sampleRate, 4);
        cap.file.seek(28, bfs::SeekBegin);
        cap.file.write((char*)&cap.h.byteRate, 4);
        cap.file.seek(40, bfs::SeekBegin);
        cap.file.write((char*)&cap.bytesWritten, 4);
        cap.file.close();
        if (cap.volume == 0)
            return;
        string old_filename = "audio_" + to_str(cap.startTime) + "_0_" + to_str(cap.idx) + ".wav";
        string new_filename = "audio_" + to_str(cap.startTime) + "_" + to_str(cap.volume) + "_" + to_str(cap.idx) + ".wav";
        ASS(MoveFileA(old_filename.c_str(), new_filename.c_str())); // No UNICODE anyway
    }
}

static void reinit_wav(AudioCapture& cap) {
    // cout << "reinit\n";
    auto cur_time = btas::get_time();
    auto idx = gen_uid(cur_time);
    string filename = "audio_" + to_str(cur_time) + "_0_" + to_str(idx) + ".wav";
    cap.file = bfs::File(filename, 1);
    cap.file.write((char*)&cap.h, sizeof(WavHeader));
    cap.bytesWritten = 0;
    cap.startTime = cur_time;
    cap.idx = idx;
    // cout << "reinit " << cap.h.byteRate << "\n";
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
    if (it != g_captures.end() && freq != 0) {
        it->second.currentSrcFreq = freq;
        it->second.resampleRatio = 48000.0 / (double)freq;
    }

    return fpApplyFrequencyToBuffer(pThis, edx, freq);
}

static int __fastcall hkApplyVolumeToBuffer(IDirectSoundBuffer** pThis, void* edx, long volume) {
    CriticalSectionLock lock(g_audioCS);
    auto it = g_captures.find(*pThis);
    if (it != g_captures.end()) {
        // cout << "set volume: " << (long)volume << "\n";
        // Only remember last volume
        it->second.volume = volume;
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

static int __fastcall hkResetBufferPosition(IDirectSoundBuffer** pThis, void* edx) {
    // return fpResetBufferPosition(pThis, edx);
    // FIXME
    CriticalSectionLock lock(g_audioCS);
    auto it = g_captures.find(*pThis);
    auto cur_time = btas::get_time();
    if (it != g_captures.end() && it->second.bytesWritten > 0 && it->second.startTime < cur_time) {
        // cout << "hkResetBufferPosition " << it->second.bytesWritten << "\n";
        finalize_wav(it->second);
        reinit_wav(it->second);
    }
    return fpResetBufferPosition(pThis, edx);
}

static HRESULT STDMETHODCALLTYPE DetourUnlock(IDirectSoundBuffer* pThis, LPVOID pv1, DWORD db1, LPVOID pv2, DWORD db2) {
    CriticalSectionLock lock(g_audioCS);
    auto it = g_captures.find(pThis);
    if (it != g_captures.end()) {
        if (!it->second.file.is_open()) {
            reinit_wav(it->second);
            return fpUnlock(pThis, pv1, db1, pv2, db2);
        }
        if (pv1 && db1 > 0) write_resampled(it->second, (char*)pv1, db1);
        if (pv2 && db2 > 0) write_resampled(it->second, (char*)pv2, db2);
    }
    return fpUnlock(pThis, pv1, db1, pv2, db2);
}

static HRESULT STDMETHODCALLTYPE DetourCreateSoundBuffer(IDirectSound* pThis, LPCDSBUFFERDESC desc, LPDIRECTSOUNDBUFFER* buffer, LPUNKNOWN unk) {
    HRESULT hr = fpCreateSoundBuffer(pThis, desc, buffer, unk);
    if (SUCCEEDED(hr) && buffer && *buffer && desc->lpwfxFormat) {
        CriticalSectionLock lock(g_audioCS);
        unsigned long timestamp = btas::get_time();
        // create wav file for each sound to be joined later via script
        auto idx = gen_uid(timestamp);
        string filename = "audio_" + to_str(timestamp) + "_0_" + to_str(idx) + ".wav";
        AudioCapture cap(filename);
        if (cap.file.is_open()) {
            setup_fixed_header(cap.h, desc->lpwfxFormat->nChannels, desc->lpwfxFormat->wBitsPerSample);
            cap.currentSrcFreq = desc->lpwfxFormat->nSamplesPerSec;
            cap.resampleRatio = 48000.0 / (double)cap.currentSrcFreq;
            cap.file.write((char*)&cap.h, sizeof(WavHeader));
            cap.startTime = timestamp;
            cap.idx = idx;
            g_captures[*buffer] = std::move(cap);
        }
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
        // Does not help anyway
        //target = (void*)(mem::get_base("mmfs2.dll") + 0x45070);
        //hook(target, hkResetBufferPosition, &fpResetBufferPosition);
        //enable_hook(target);
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
