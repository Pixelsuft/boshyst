#include <windows.h>
#include <dsound.h>
#include "mem.hpp"
#include "ass.hpp"
#include "btas.hpp"
#include "hook.hpp"
#include "fs.hpp"
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
    char riff[4] = { 'R', 'I', 'F', 'F' };
    uint32_t fileSize = 0;
    char wave[4] = { 'W', 'A', 'V', 'E' };
    char fmt[4] = { 'f', 'm', 't', ' ' };
    uint32_t fmtLen = 16;
    uint16_t formatTag = 1; // PCM
    uint16_t channels = 0;
    uint32_t sampleRate = 0;
    uint32_t byteRate = 0;
    uint16_t blockAlign = 0;
    uint16_t bitsPerSample = 0;
    char data[4] = { 'd', 'a', 't', 'a' };
    uint32_t dataLen = 0;
};
#pragma pack(pop)

struct AudioCapture {
    bfs::File file;
    uint32_t bytesWritten = 0;
    unsigned long startTime = 0;
    uint32_t byteRate = 0;
    
    AudioCapture() {}
    AudioCapture(string fn) : file(fn, 1) {}
};

static std::map<IDirectSoundBuffer*, AudioCapture> g_captures;
static std::mutex g_audioMutex;
extern bool is_paused;

typedef HRESULT(WINAPI* DirectSoundCreate_t)(LPCGUID, LPDIRECTSOUND*, LPUNKNOWN);
typedef HRESULT(STDMETHODCALLTYPE* CreateSoundBuffer_t)(IDirectSound*, LPCDSBUFFERDESC, LPDIRECTSOUNDBUFFER*, LPUNKNOWN);
typedef HRESULT(STDMETHODCALLTYPE* Unlock_t)(IDirectSoundBuffer*, LPVOID, DWORD, LPVOID, DWORD);

static DirectSoundCreate_t fpDirectSoundCreate = nullptr;
static CreateSoundBuffer_t fpCreateSoundBuffer = nullptr;
static Unlock_t fpUnlock = nullptr;

// TRIMS or PADS audio to match TAS ResetTime - StartTime
void finalize_wav(AudioCapture& cap) {
    if (cap.file.is_open()) {
        unsigned long currentTime = btas::get_time();
        unsigned long elapsedMs = (currentTime > cap.startTime) ? (currentTime - cap.startTime) : 0;

        // Exact byte count required for this duration
        uint32_t targetBytes = (uint32_t)((uint64_t)elapsedMs * cap.byteRate / 1000);

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
    audio_on_reset();
    is_paused = true;
}

static HRESULT STDMETHODCALLTYPE DetourUnlock(IDirectSoundBuffer* pThis, LPVOID pv1, DWORD db1, LPVOID pv2, DWORD db2) {
    std::lock_guard<std::mutex> lock(g_audioMutex);
    auto it = g_captures.find(pThis);
    if (it != g_captures.end() && it->second.file.is_open()) {
        // Only write if we haven't exceeded real-time TAS duration yet
        // (Prevents files from growing indefinitely if Unlock is called excessively)
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
        string filename = "audio_" + std::to_string(timestamp) + "_" + std::to_string((uintptr_t)*buffer) + ".wav";
        AudioCapture cap(filename);
        if (cap.file.is_open()) {
            WavHeader h;
            h.channels = desc->lpwfxFormat->nChannels;
            h.sampleRate = desc->lpwfxFormat->nSamplesPerSec;
            h.bitsPerSample = desc->lpwfxFormat->wBitsPerSample;
            h.byteRate = desc->lpwfxFormat->nAvgBytesPerSec;
            h.blockAlign = desc->lpwfxFormat->nBlockAlign;
            cap.file.write((char*)&h, sizeof(h));
            cap.startTime = timestamp;
            cap.byteRate = h.byteRate;
            g_captures[*buffer] = std::move(cap);
        }
        void* target = (*(void***)*buffer)[19]; // Unlock
        if (fpUnlock == nullptr) {
            hook(target, DetourUnlock, &fpUnlock);
            MH_EnableHook(target);
        }
    }
    return hr;
}

static HRESULT WINAPI DetourDirectSoundCreate(LPCGUID guid, LPDIRECTSOUND* ds, LPUNKNOWN unk) {
    HRESULT hr = fpDirectSoundCreate(guid, ds, unk);
    if (SUCCEEDED(hr) && ds && *ds) {
        void* target = (*(void***)*ds)[3]; // CreateSoundBuffer
        if (fpCreateSoundBuffer == nullptr) {
            hook(target, DetourCreateSoundBuffer, &fpCreateSoundBuffer);
            MH_EnableHook(target);
        }
    }
    return hr;
}

void audio_init() {
    LoadLibraryW(L"dsound.dll");
    hook(mem::addr("DirectSoundCreate", "dsound.dll"), DetourDirectSoundCreate, &fpDirectSoundCreate);
    MH_EnableHook(MH_ALL_HOOKS);
}
