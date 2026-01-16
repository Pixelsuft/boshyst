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
#include <mutex>
#include <vector>
#include <cstdint>

using std::cout;
using std::string;

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
    uint32_t bytesWritten;
    unsigned long startTime;
    
    AudioCapture() {
        bytesWritten = 0;
        startTime = 0;
    }

    AudioCapture(string fn) : file(fn, 1) {
        bytesWritten = 0;
        startTime = 0;
    }
};

static std::map<IDirectSoundBuffer*, AudioCapture> g_captures;
static std::mutex g_audioMutex;

typedef HRESULT(WINAPI* DirectSoundCreate_t)(LPCGUID, LPDIRECTSOUND*, LPUNKNOWN);
typedef HRESULT(STDMETHODCALLTYPE* CreateSoundBuffer_t)(IDirectSound*, LPCDSBUFFERDESC, LPDIRECTSOUNDBUFFER*, LPUNKNOWN);
typedef HRESULT(STDMETHODCALLTYPE* Unlock_t)(IDirectSoundBuffer*, LPVOID, DWORD, LPVOID, DWORD);
typedef int(__fastcall* tApplyFrequencyToBuffer)(IDirectSoundBuffer**, void*, DWORD);
typedef int(__fastcall* tStopHardwareBuffer)(IDirectSoundBuffer**, void*);
typedef int(__fastcall* tResetBufferPosition)(IDirectSoundBuffer**, void*);

static DirectSoundCreate_t fpDirectSoundCreate = nullptr;
static CreateSoundBuffer_t fpCreateSoundBuffer = nullptr;
static tApplyFrequencyToBuffer fpApplyFrequencyToBuffer = nullptr;
static Unlock_t fpUnlock = nullptr;
static tStopHardwareBuffer fpStopHardwareBuffer = nullptr;
static tResetBufferPosition fpResetBufferPosition = nullptr;

static void finalize_wav(AudioCapture& cap) {
    if (cap.file.is_open()) {
        unsigned long currentTime = btas::get_time();
        unsigned long elapsedMs = (currentTime > cap.startTime) ? (currentTime - cap.startTime) : 0;
        if (cap.bytesWritten == 0 || elapsedMs == 0) {
            // Delete empty file
            FILE_DISPOSITION_INFO disInfo;
            ZeroMemory(&disInfo, sizeof(FILE_DISPOSITION_INFO));
            disInfo.DeleteFile = TRUE;
            ASS(SetFileInformationByHandle((HANDLE)cap.file.get_handle(), FileDispositionInfo, &disInfo, sizeof(disInfo)));
            cap.file.close();
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
        cap.file.seek(4);
        cap.file.write((char*)&finalFileSize, 4);
        cap.file.seek(24);
        cap.file.write((char*)&cap.h.sampleRate, 4);
        cap.file.seek(28);
        cap.file.write((char*)&cap.h.byteRate, 4);
        cap.file.seek(40);
        cap.file.write((char*)&cap.bytesWritten, 4);
        cap.file.close();
    }
}

void audio_on_reset() {
    std::lock_guard<std::mutex> lock(g_audioMutex);
    for (auto& pair : g_captures) {
        finalize_wav(pair.second);
    }
    g_captures.clear();
}

void audio_stop() {
    if (!conf::cap_au)
        return;
    audio_on_reset();
}

static int __fastcall hkApplyFrequencyToBuffer(IDirectSoundBuffer** pThis, void* edx, DWORD freq) {
    std::lock_guard<std::mutex> lock(g_audioMutex);
    auto it = g_captures.find(*pThis);
    if (it != g_captures.end() && freq != 0 && freq != it->second.h.sampleRate) {
        // std::cout << "Freq hook: " << it->second.h.sampleRate << " -> " << freq << std::endl;
        it->second.h.sampleRate = freq;
    }

    return fpApplyFrequencyToBuffer(pThis, edx, freq);
}

static int __fastcall hkStopHardwareBuffer(IDirectSoundBuffer** pThis, void* edx) {
    std::lock_guard<std::mutex> lock(g_audioMutex);
    auto it = g_captures.find(*pThis);
    if (it != g_captures.end()) {
        // cout << "hkStopHardwareBuffer\n";
        finalize_wav(it->second);
    }
    return fpStopHardwareBuffer(pThis, edx);
}

static void __fastcall hkReleaseHardwareBuffer(IDirectSoundBuffer** pThis, void* edx) {
    std::lock_guard<std::mutex> lock(g_audioMutex);
    auto it = g_captures.find(*pThis);
    if (it != g_captures.end()) {
        finalize_wav(it->second);
        g_captures.erase(it);
    }
    *pThis = nullptr;
}

static int __fastcall hkResetBufferPosition(IDirectSoundBuffer** pThis, void* edx) {
    // return fpResetBufferPosition(pThis, edx);
    // FIXME
    std::lock_guard<std::mutex> lock(g_audioMutex);
    auto it = g_captures.find(*pThis);
    auto cur_time = btas::get_time();
    if (it != g_captures.end() && it->second.bytesWritten > 0 && it->second.startTime < cur_time) {
        cout << "hkResetBufferPosition " << it->second.bytesWritten << "\n";
        finalize_wav(it->second);
        string filename = "audio_" + to_str(cur_time) + "_" + to_str((size_t)*pThis) + ".wav";
        it->second.file = bfs::File(filename, 1);
        it->second.file.write((char*)&it->second.h, sizeof(WavHeader));
        it->second.bytesWritten = 0;
        it->second.startTime = cur_time;
    }
    return fpResetBufferPosition(pThis, edx);
}

static HRESULT STDMETHODCALLTYPE DetourUnlock(IDirectSoundBuffer* pThis, LPVOID pv1, DWORD db1, LPVOID pv2, DWORD db2) {
    std::lock_guard<std::mutex> lock(g_audioMutex);
    auto it = g_captures.find(pThis);
    if (it != g_captures.end() && it->second.file.is_open()) {
        if (pv1 && db1 > 0) { it->second.file.write((char*)pv1, db1); it->second.bytesWritten += db1; }
        if (pv2 && db2 > 0) { it->second.file.write((char*)pv2, db2); it->second.bytesWritten += db2; }
    }
    return fpUnlock(pThis, pv1, db1, pv2, db2);
}

static HRESULT STDMETHODCALLTYPE DetourCreateSoundBuffer(IDirectSound* pThis, LPCDSBUFFERDESC desc, LPDIRECTSOUNDBUFFER* buffer, LPUNKNOWN unk) {
    HRESULT hr = fpCreateSoundBuffer(pThis, desc, buffer, unk);
    if (SUCCEEDED(hr) && buffer && *buffer && desc->lpwfxFormat) {
        std::lock_guard<std::mutex> lock(g_audioMutex);
        unsigned long timestamp = btas::get_time();
        string filename = "audio_" + to_str(timestamp) + "_" + to_str((size_t)*buffer) + ".wav";
        AudioCapture cap(filename);
        if (cap.file.is_open()) {
            cap.h.channels = desc->lpwfxFormat->nChannels;
            cap.h.sampleRate = desc->lpwfxFormat->nSamplesPerSec;
            cap.h.bitsPerSample = desc->lpwfxFormat->wBitsPerSample;
            cap.h.byteRate = desc->lpwfxFormat->nAvgBytesPerSec;
            cap.h.blockAlign = desc->lpwfxFormat->nBlockAlign;
            cap.file.write((char*)&cap.h, sizeof(WavHeader));
            cap.startTime = timestamp;
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
    HRESULT hr = fpDirectSoundCreate(guid, ds, unk);
    if (SUCCEEDED(hr) && ds && *ds && fpCreateSoundBuffer == nullptr) {
        cout << "audio enabling hooks 1\n";
        void* target = (*(void***)*ds)[3];
        hook(target, DetourCreateSoundBuffer, &fpCreateSoundBuffer);
        enable_hook(target);
        // Somewhy hooking freq from dsound doesnt work
        target = (void*)(mem::get_base("mmfs2.dll") + 0x451d0);
        hook(target, hkApplyFrequencyToBuffer, &fpApplyFrequencyToBuffer);
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
        // TODO: mmfs2.dll + 0x448d3 for disabling event?
    }
    return hr;
}

void audio_init() {
    if (!conf::cap_au)
        return;
    hook(mem::addr("DirectSoundCreate", "dsound.dll"), DetourDirectSoundCreate, &fpDirectSoundCreate);
}
