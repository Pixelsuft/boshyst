#define WIN32_LEAN_AND_MEAN
#define _CRT_SECURE_NO_WARNINGS
#include <Windows.h>
#include <shlwapi.h>
#include <iostream>
#include <imgui.h>
#include "hook.hpp"
#include "mem.hpp"
#include "conf.hpp"
#include "rec.hpp"
#include "ui.hpp"
#include "input.hpp"
#include "init.hpp"
#include "utils.hpp"
#include "btas.hpp"
#include "ghidra_headers.h"
#pragma comment(lib, "Shlwapi.lib")

using std::cout;
extern HWND hwnd;
extern HWND mhwnd;
extern bool capturing;
extern bool show_menu;
extern bool next_white;
extern int lock_rng_range;
extern bool fix_rng;
extern float fix_rng_val;
int last_new_rand_val = 0;
bool last_reset = false;
static HANDLE(__stdcall* CreateFileOrig)(LPCSTR _fn, DWORD dw_access, DWORD share_mode, LPSECURITY_ATTRIBUTES sec_attr, DWORD cr_d, DWORD flags, HANDLE template_);
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

static short(__stdcall* DisplayRunObjectVPOrig)(void* pthis) = nullptr;
static short __stdcall DisplayRunObjectVPHook(void* pthis) {
    if (conf::no_vp)
        return 0;
    auto ret = DisplayRunObjectVPOrig(pthis);
    return ret;
}

static int(__cdecl* randOrig)() = nullptr;
static int __cdecl randHook() {
    if (is_btas)
        return 0; // TODO
    int ret;
    if (fix_rng && (lock_rng_range == 0 || lock_rng_range == (RAND_MAX + 1)))
        ret = (unsigned int)((float)RAND_MAX * fix_rng_val / 100.f);
    else
	    ret = randOrig();
    last_new_rand_val = ret;
	return ret;
}

static int(__cdecl* _stricmpOrig)(const char* s1, const char* s2) = nullptr;
static int __cdecl _stricmpHook(const char* s1, const char* s2) {
    if (conf::no_sh && (strcmp(s1, "CS_SinWave2.fx") == 0 || strcmp(s1, "DirBlur x3.fx") == 0 || strcmp(s1, "DropShadow.fx") == 0 || strcmp(s1, "FlipX.fx") == 0 || strcmp(s1, "Mosaic.fx") == 0 || strcmp(s1, "Outline.fx") == 0 || strcmp(s1, "PT_BlurAndAngle.fx") == 0)) {
        // shaders
        // Extra: Add, Invert, Sub, Mono, Blend, XOR, OR, AND
        // cout << "!!!: " << s1 << " " << s2 << '\n';
        // s1 = "Sub";
        // TODO: check FUN_00426f90
        return -1;
    }
    else if (conf::god && (strcmp(s2, "Die") == 0 || strcmp(s2, "die") == 0)) {
        return -1;
    }
    else if (conf::no_trans && strcmp(s2, "teleporting") == 0) {
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

DWORD(__stdcall* timeGetTimeOrig)();
static DWORD __stdcall timeGetTimeHook() {
    if (!is_btas)
        return timeGetTimeOrig();
    // cout << "time hook!\n";
    // tas_time += 1;
    return btas::get_time();
}

BOOL(__stdcall* SetWindowTextAOrig)(HWND, LPCSTR);
static BOOL __stdcall SetWindowTextAHook(HWND hwnd, LPCSTR cap) {
    if (hwnd != ::hwnd)
        return SetWindowTextAOrig(hwnd, cap);
    last_reset = true;
    if (capturing && strcmp(cap, "I Wanna Be The Boshy") == 0) {
        next_white = true;
        return FALSE;
    }
    return SetWindowTextAOrig(hwnd, cap);
}

static int(__stdcall* UpdateGameFrameOrig)() = nullptr;
static int __stdcall UpdateGameFrameHook() {
    static bool hooks_inited = false;
    if (!hooks_inited) {
        hooks_inited = true;
        try_to_init();
        if (is_btas)
            btas::init();
    }
    try_to_hook_graphics();
    if (is_btas && btas::on_before_update()) {
        auto ret = UpdateGameFrameOrig();
        void (*ProcessFrameRendering)(void);
        ProcessFrameRendering = reinterpret_cast<decltype(ProcessFrameRendering)>(mem::get_base() + 0x1ebf0);
        ProcessFrameRendering();
        return ret;
    }

    input_tick();
    ui::pre_update();

    auto ret = UpdateGameFrameOrig();
    if (!show_menu && conf::tp_on_click && MyKeyState(VK_LBUTTON)) {
        int scene_id = get_scene_id();
        auto player = (ObjectHeader*)get_player_ptr(scene_id);
        if (player) {
            int x, y, w, h;
            get_win_size(w, h);
            get_cursor_pos_orig(x, y);
            // TODO: how to map cursor pos into game properly (scaling)?
            RunHeader* pState = *(RunHeader**)(mem::get_base() + 0x59a9c);
            player->xPos = pState->currentViewportX + x * 640 / w;
            player->yPos = pState->currentViewportY + y * 480 / h;
            player->redrawFlag = 1;
        }
    }

    // cout << "Hook!\n";
    return ret;
}

static unsigned int(__cdecl* RandomOrig)(unsigned int maxv);
static unsigned int __cdecl RandomHook(unsigned int maxv) {
    unsigned int ret;
    if (fix_rng && (lock_rng_range == 0 || lock_rng_range == (int)maxv)) {
        if (fix_rng_val == 100.f)
            ret = maxv - 1;
        else
            ret = (unsigned int)((float)maxv * fix_rng_val / 100.f);
    }
    else
        ret = RandomOrig(maxv);
    ui_register_rand(maxv, ret);
    return ret;
}

static int(__stdcall* MessageBoxAOrig)(HWND hWnd, LPCSTR lpText, LPCSTR lpCaption, UINT uType);
static int __stdcall MessageBoxAHook(HWND hWnd, LPCSTR lpText, LPCSTR lpCaption, UINT uType) {
    if (!conf::skip_msg)
        return MessageBoxAOrig(hWnd, lpText, lpCaption, uType);
    // cout << lpText << std::endl;
    return IDNO;
}

static LRESULT(__stdcall* MainWindowProcOrig)(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
static LRESULT __stdcall MainWindowProcHook(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    if (1) {
        if (uMsg == WM_KEYDOWN) {
            if (wParam == (WPARAM)conf::menu_hotkey)
                show_menu = !show_menu;
        }
        if (is_btas && (uMsg == WM_KEYDOWN || uMsg == WM_KEYUP))
            btas::on_key((int)wParam, uMsg == WM_KEYDOWN, (uMsg == WM_KEYDOWN) && (HIWORD(lParam) & KF_REPEAT));
        ImGui_ImplWin32_WndProcHandler(::hwnd, uMsg, wParam, lParam);
    }
    auto ret = MainWindowProcOrig(hWnd, uMsg, wParam, lParam);
    return ret;
}

static LRESULT(__stdcall* EditWindowProcOrig)(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
static LRESULT __stdcall EditWindowProcHook(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    if (1) {
        if (uMsg == WM_KEYDOWN) {
            if (wParam == (WPARAM)conf::menu_hotkey)
                show_menu = !show_menu;
        }
        if (is_btas && (uMsg == WM_KEYDOWN || uMsg == WM_KEYUP))
            btas::on_key((int)wParam, uMsg == WM_KEYDOWN, (uMsg == WM_KEYDOWN) && (HIWORD(lParam) & KF_REPEAT));
        ImGui_ImplWin32_WndProcHandler(::mhwnd, uMsg, wParam, lParam);
    }
    if (is_btas && uMsg > WM_MOUSEFIRST && uMsg < WM_MOUSELAST)
        return 0;
    auto ret = EditWindowProcOrig(hWnd, uMsg, wParam, lParam);
    return ret;
}

void init_game_loop() {
    if (!UpdateGameFrameOrig)
        hook(mem::get_base() + 0x365a0, UpdateGameFrameHook, &UpdateGameFrameOrig);
    if (is_btas)
        hook(mem::addr("timeGetTime", "winmm.dll"), timeGetTimeHook, &timeGetTimeOrig);
    ASS(MH_EnableHook(MH_ALL_HOOKS) == MH_OK);
}

void init_simple_hacks() {
    input_init();
    if (!is_hourglass && (is_btas || !conf::tas_mode)) {
        // hook(mem::get_base() + 0x43e30, MainWindowProcHook, &MainWindowProcOrig);
        // hook(mem::get_base() + 0x41ba0, EditWindowProcHook, &EditWindowProcOrig);
        MainWindowProcOrig = (WNDPROC)SetWindowLongPtrA(::hwnd, GWLP_WNDPROC, (LONG)MainWindowProcHook);
        EditWindowProcOrig = (WNDPROC)SetWindowLongPtrA(::mhwnd, GWLP_WNDPROC, (LONG)EditWindowProcHook);
    }
    hook(mem::addr("DisplayRunObject", "Viewport.mfx"), DisplayRunObjectVPHook, &DisplayRunObjectVPOrig);
    hook(mem::addr("rand", "msvcrt.dll"), randHook, &randOrig);
    hook(mem::addr("_stricmp", "msvcrt.dll"), _stricmpHook, &_stricmpOrig);
    hook(mem::addr("CreateFileA", "kernel32.dll"), CreateFileHook, &CreateFileOrig);
    hook(mem::addr("SetWindowTextA", "user32.dll"), SetWindowTextAHook, &SetWindowTextAOrig);
    hook(mem::addr("MessageBoxA", "user32.dll"), MessageBoxAHook, &MessageBoxAOrig);
    hook(mem::get_base("kcmouse.mfx") + 0x1103, SetCursorYHook);
    hook(mem::get_base("kcmouse.mfx") + 0x1125, SetCursorXHook);
    hook(mem::get_base() + 0x1f890, RandomHook, &RandomOrig);
    DeleteFileA("onlineLicense.tmp.ini");
    DeleteFileA("animation.tmp.ini");
    DeleteFileA("options.tmp.ini");
    DeleteFileA("saveFile.tmp.ini");
    DeleteFileA("SaveFile1.tmp.ini");
    DeleteFileA("SaveFile2.tmp.ini");
    DeleteFileA("SaveFile3.tmp.ini");
}
