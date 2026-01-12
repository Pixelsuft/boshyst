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
    uint32_t bytesWritten;
    unsigned long startTime;
    uint32_t byteRate;
    uint32_t currentFrequency;
    
    AudioCapture() {
        currentFrequency = 0;
        bytesWritten = 0;
        startTime = 0;
        byteRate = 0;
    }

    AudioCapture(string fn) : file(fn, 1) {
        currentFrequency = 0;
        bytesWritten = 0;
        startTime = 0;
        byteRate = 0;
    }
};

static std::map<IDirectSoundBuffer*, AudioCapture> g_captures;
static std::mutex g_audioMutex;

typedef HRESULT(WINAPI* DirectSoundCreate_t)(LPCGUID, LPDIRECTSOUND*, LPUNKNOWN);
typedef HRESULT(STDMETHODCALLTYPE* CreateSoundBuffer_t)(IDirectSound*, LPCDSBUFFERDESC, LPDIRECTSOUNDBUFFER*, LPUNKNOWN);
typedef HRESULT(STDMETHODCALLTYPE* Unlock_t)(IDirectSoundBuffer*, LPVOID, DWORD, LPVOID, DWORD);
typedef int(__fastcall* tApplyFrequencyToBuffer)(IDirectSoundBuffer**, void*, DWORD);

static DirectSoundCreate_t fpDirectSoundCreate = nullptr;
static CreateSoundBuffer_t fpCreateSoundBuffer = nullptr;
tApplyFrequencyToBuffer fpApplyFrequencyToBuffer = nullptr;
static Unlock_t fpUnlock = nullptr;

static void finalize_wav(AudioCapture& cap) {
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
        cap.file.seek(24);
        cap.file.write((char*)&cap.currentFrequency, 4);
        cap.file.seek(28);
        cap.file.write((char*)&cap.byteRate, 4);
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
    if (it != g_captures.end() && freq != 0 && freq != it->second.currentFrequency) {
        // std::cout << "Freq hook: " << it->second.currentFrequency << " -> " << freq << std::endl;
        it->second.currentFrequency = freq;
    }

    return fpApplyFrequencyToBuffer(pThis, edx, freq);
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
            WavHeader h;
            h.channels = desc->lpwfxFormat->nChannels;
            h.sampleRate = desc->lpwfxFormat->nSamplesPerSec;
            h.bitsPerSample = desc->lpwfxFormat->wBitsPerSample;
            h.byteRate = desc->lpwfxFormat->nAvgBytesPerSec;
            h.blockAlign = desc->lpwfxFormat->nBlockAlign;
            cap.file.write((char*)&h, sizeof(h));
            cap.startTime = timestamp;
            cap.byteRate = h.byteRate;
            cap.currentFrequency = h.sampleRate;
            g_captures[*buffer] = std::move(cap);
        }
        if (fpApplyFrequencyToBuffer == nullptr) {
            cout << "enabling hooks\n";
            void* target = (*(void***)*buffer)[19]; // Unlock
            hook(target, DetourUnlock, &fpUnlock);
            enable_hook(target);
            target = (void*)(mem::get_base("mmfs2.dll") + 0x451d0);
            hook(target, hkApplyFrequencyToBuffer, &fpApplyFrequencyToBuffer);
            enable_hook(target);
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
            enable_hook(target);
        }
    }
    return hr;
}

void audio_init() {
    if (!conf::cap_au)
        return;
    LoadLibraryW(L"dsound.dll");
    hook(mem::addr("DirectSoundCreate", "dsound.dll"), DetourDirectSoundCreate, &fpDirectSoundCreate);
    enable_hook();
}
