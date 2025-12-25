#define WIN32_LEAN_AND_MEAN
#define _CRT_SECURE_NO_WARNINGS
#include <Windows.h>
#include <shlwapi.h>
//#include <timeapi.h>
#include <iostream>
#include "hook.hpp"
#include "mem.hpp"
#include "conf.hpp"
#include "rec.hpp"
#include "ghidra_headers.h"
#pragma comment(lib, "Shlwapi.lib")

using std::cout;

extern void input_init();
extern HWND hwnd;
HWND mhwnd = nullptr;
int last_rng_val = 0;
bool last_reset = false;
static HANDLE(__stdcall* CreateFileOrig)(LPCSTR _fn, DWORD dw_access, DWORD share_mode, LPSECURITY_ATTRIBUTES sec_attr, DWORD cr_d, DWORD flags, HANDLE template_);

static short(__stdcall* DisplayRunObjectVPOrig)(void* pthis) = nullptr;
static short __stdcall DisplayRunObjectVPHook(void* pthis) {
    if (conf::no_vp)
        return 0;
    auto ret = DisplayRunObjectVPOrig(pthis);
    return ret;
}

static int(__cdecl* randOrig)() = nullptr;
static int __cdecl randHook() {
	int ret = randOrig();
    last_rng_val = ret;
	return ret;
}

static int(__cdecl* _stricmpOrig)(const char* s1, const char* s2) = nullptr;
static int __cdecl _stricmpHook(const char* s1, const char* s2) {
    if (conf::god && (strcmp(s2, "Die") == 0 || strcmp(s2, "die") == 0)) {
        return -1;
    }
    else if (conf::no_sh && (strcmp(s1, "CS_SinWave2.fx") == 0 || strcmp(s1, "DirBlur x3.fx") == 0 || strcmp(s1, "DropShadow.fx") == 0 || strcmp(s1, "FlipX.fx") == 0 || strcmp(s1, "Mosaic.fx") == 0 || strcmp(s1, "Outline.fx") == 0 || strcmp(s1, "PT_BlurAndAngle.fx") == 0)) {
        // shaders
        // Extra: Add, Invert, Sub, Mono, Blend, XOR, OR, AND
        // cout << "!!!: " << s1 << " " << s2 << '\n';
        // s1 = "Sub";
        // TODO: check FUN_00426f90
        return -1;
    }
    // menuChosen, NameTags, jump, doublejump, teleporting, save, shoot, restart, KillAll
    auto ret = _stricmpOrig(s1, s2);
    return ret;
}

static bool c_ends_with(const char* str, const char* end) {
    size_t sl = strlen(str);
    size_t el = strlen(end);
    if (el > sl)
        return false;
    return memcmp(str + sl - el, end, el) == 0;
}

static HANDLE __stdcall CreateFileHook(LPCSTR _fn, DWORD dw_access, DWORD share_mode, LPSECURITY_ATTRIBUTES sec_attr, DWORD cr_d, DWORD flags, HANDLE template_) {
    if (!conf::keep_save)
        return CreateFileOrig(_fn, dw_access, share_mode, sec_attr, cr_d, flags, template_);
    if (!_fn)
        return NULL;
    char buf[MAX_PATH];
    if (c_ends_with(_fn, ".ini")) {
        strcpy(buf, _fn);
        size_t l = strlen(buf);
        strcpy(buf + l - 4, ".tmp.ini");
        if ((dw_access & GENERIC_WRITE) || PathFileExistsA(buf)) {
            _fn = buf;
        }
        if (dw_access & GENERIC_READ) {
            // cout << "CreateFileA read: " << _fn << std::endl;
        }
        else if (dw_access & GENERIC_WRITE) {
            // cout << "CreateFileA write: " << _fn << std::endl;
        }
    }
    HANDLE ret = CreateFileOrig(_fn, dw_access, share_mode, sec_attr, cr_d, flags, template_);
    return ret;
}

static unsigned int __stdcall SetCursorYHook(void* param_1, int param_2, void* pshit)
{
    if (conf::no_cmove)
        return 0;
    BOOL uVar1;
    tagPOINT local_c;
    GetCursorPos(&local_c);
    uVar1 = SetCursorPos(param_2, local_c.y);
    return uVar1 & 0xffff0000;
}

static unsigned int __stdcall SetCursorXHook(void* param_1, int param_2, void* pshit)
{
    if (conf::no_cmove)
        return 0;
    BOOL uVar1;
    tagPOINT local_c;
    GetCursorPos(&local_c);
    uVar1 = SetCursorPos(local_c.x, param_2);
    return uVar1 & 0xffff0000;
}

static DWORD(__stdcall* timeGetTimeOrig)();
static DWORD __stdcall timeGetTimeHook() {
    cout << "time hook!\n";
    return timeGetTimeOrig();
}

BOOL(__stdcall* SetWindowTextAOrig)(HWND, LPCSTR);
static BOOL __stdcall SetWindowTextAHook(HWND hwnd, LPCSTR cap) {
    if (hwnd != ::hwnd)
        return SetWindowTextAOrig(hwnd, cap);
    last_reset = true;
    return SetWindowTextAOrig(hwnd, cap);
}

extern int get_scene_id();
extern void* get_player_ptr(int s);
static int(__stdcall* UpdateGameFrameOrig)();
static int __stdcall UpdateGameFrameHook() {
    void* pState = *(void**)(mem::get_base() + 0x59a9c);
    short* next_frame = (short*)((size_t)pState + 0x30);
    if ((GetKeyState('F') & 128) != 0) {
        HANDLE hFile = CreateFileOrig(
            "example.txt",
            GENERIC_READ,
            FILE_SHARE_READ,
            nullptr,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            nullptr
        );
        ASS(hFile != nullptr);
        int outver = 0;
        int(__cdecl * LoadFunc)(HANDLE, int*);
        LoadFunc = reinterpret_cast<decltype(LoadFunc)>(mem::get_base() + 0x39780);
        auto ret = LoadFunc(hFile, &outver);
        cout << "load " << ret << " " << outver << '\n';
        CloseHandle(hFile);
    }
    if ((GetKeyState('G') & 128) != 0) {
        HANDLE hFile = CreateFileOrig(
            "example.txt",
            GENERIC_WRITE,
            0,
            nullptr,
            CREATE_ALWAYS,
            FILE_ATTRIBUTE_NORMAL,
            nullptr
        );
        ASS(hFile != nullptr);
        bool(__cdecl * SaveFunc)(HANDLE);
        SaveFunc = reinterpret_cast<decltype(SaveFunc)>(mem::get_base() + 0x37dc0);
        auto ret = SaveFunc(hFile);
        cout << "save" << ret << '\n';
        CloseHandle(hFile);
    }

    // My Stuff
    ObjectHeader* player = (ObjectHeader*)get_player_ptr(get_scene_id());
    if (player != nullptr && 0) {
       // player->xPos = 100;
       // player->yPos = 100;
        // player->redrawFlag = 1;
    }

    auto ret = UpdateGameFrameOrig();
    // cout << "Hook!\n";
    return ret;
}

static void(__cdecl* TriggerFrameTransitionOrig)(void*);
static void __cdecl TriggerFrameTransitionHook(void* v) {
    //cout << "trans hook " << v << "\n";
    // 00459a94
    void* st = *(void**)0x00459a94;

    if (1)
        TriggerFrameTransitionOrig(v);
    return;
}

static unsigned int __cdecl RandomHook(unsigned int maxv) {
    // cout << "bless rng\n";
    return 0;
}

void init_simple_hacks() {
    if (!mhwnd) {
        mhwnd = FindWindowExA(hwnd, nullptr, "Mf2EditClassTh", nullptr);
        ASS(mhwnd != nullptr);
    }
    input_init();
    hook(mem::addr("DisplayRunObject", "Viewport.mfx"), DisplayRunObjectVPHook, &DisplayRunObjectVPOrig);
    hook(mem::addr("rand", "msvcrt.dll"), randHook, &randOrig);
    hook(mem::addr("_stricmp", "msvcrt.dll"), _stricmpHook, &_stricmpOrig);
    // hook(mem::addr("timeGetTime", "winmm.dll"), timeGetTimeHook, &timeGetTimeOrig);
    hook(mem::addr("CreateFileA", "kernel32.dll"), CreateFileHook, &CreateFileOrig);
    hook(mem::addr("SetWindowTextA", "user32.dll"), SetWindowTextAHook, &SetWindowTextAOrig);
    hook(mem::get_base("kcmouse.mfx") + 0x1103, SetCursorYHook);
    hook(mem::get_base("kcmouse.mfx") + 0x1125, SetCursorXHook);
    // hook(mem::get_base() + 0x147c0, SusSceneHook, &SusSceneOrig);
    hook(mem::get_base() + 0x365a0, UpdateGameFrameHook, &UpdateGameFrameOrig);
    hook(mem::get_base() + 0x47cb0, TriggerFrameTransitionHook, &TriggerFrameTransitionOrig);
    hook(mem::get_base() + 0x1f890, RandomHook);
    DeleteFileA("onlineLicense.tmp.ini");
    DeleteFileA("animation.tmp.ini");
    DeleteFileA("options.tmp.ini");
    DeleteFileA("saveFile.tmp.ini");
    DeleteFileA("SaveFile1.tmp.ini");
    DeleteFileA("SaveFile2.tmp.ini");
    DeleteFileA("SaveFile3.tmp.ini");
}
