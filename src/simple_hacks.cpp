#define WIN32_LEAN_AND_MEAN
#define _CRT_SECURE_NO_WARNINGS
#include <Windows.h>
#include <shlwapi.h>
#include <iostream>
#include "hook.hpp"
#include "mem.hpp"
#include "conf.hpp"
#include "rec.hpp"
#pragma comment(lib, "Shlwapi.lib")

using std::cout;

extern void input_init();
extern HWND hwnd;
HWND mhwnd = nullptr;
int last_rng_val = 0;

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
    else if (conf::no_sh && strcmp(s1, "CS_SinWave2.fx") == 0 || strcmp(s1, "DirBlur x3.fx") == 0 || strcmp(s1, "DropShadow.fx") == 0 || strcmp(s1, "FlipX.fx") == 0 || strcmp(s1, "Mosaic.fx") == 0 || strcmp(s1, "Outline.fx") == 0 || strcmp(s1, "PT_BlurAndAngle.fx") == 0) {
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

static HANDLE (__stdcall *CreateFileOrig)(LPCSTR _fn, DWORD dw_access, DWORD share_mode, LPSECURITY_ATTRIBUTES sec_attr, DWORD cr_d, DWORD flags, HANDLE template_);
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

void init_simple_hacks() {
    if (!mhwnd) {
        mhwnd = FindWindowExA(hwnd, nullptr, "Mf2EditClassTh", nullptr);
        ASS(mhwnd != nullptr);
    }
    input_init();
    hook(mem::addr("DisplayRunObject", "Viewport.mfx"), DisplayRunObjectVPHook, &DisplayRunObjectVPOrig);
    hook(mem::addr("rand", "msvcrt.dll"), randHook, &randOrig);
    hook(mem::addr("_stricmp", "msvcrt.dll"), _stricmpHook, &_stricmpOrig);
    hook(mem::addr("CreateFileA", "kernel32.dll"), CreateFileHook, &CreateFileOrig);
    hook(mem::get_base("kcmouse.mfx") + 0x1103, SetCursorYHook);
    hook(mem::get_base("kcmouse.mfx") + 0x1125, SetCursorXHook);
    DeleteFileA("onlineLicense.tmp.ini");
    DeleteFileA("animation.tmp.ini");
    DeleteFileA("options.tmp.ini");
    DeleteFileA("saveFile.tmp.ini");
    DeleteFileA("SaveFile1.tmp.ini");
    DeleteFileA("SaveFile2.tmp.ini");
    DeleteFileA("SaveFile3.tmp.ini");
}