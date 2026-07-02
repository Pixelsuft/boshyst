#define WIN32_LEAN_AND_MEAN
#define _CRT_SECURE_NO_WARNINGS
#include "btas.hpp"
#include "conf.hpp"
#include "ghidra_headers.h"
#include "hook.hpp"
#include "init.hpp"
#include "input.hpp"
#include "mem.hpp"
#include "rec.hpp"
#include "ui.hpp"
#include "utils.hpp"
#include <Windows.h>
#include <ctime>
#include <imgui.h>
#include <iostream>
#include <mmsystem.h>
#include <shlwapi.h>
#pragma comment(lib, "Shlwapi.lib")

using std::cout;
extern HANDLE hproc;
extern HWND hwnd;
extern HWND mhwnd;
extern int lock_rng_range;
extern bool fix_rng;
extern float fix_rng_val;
static bool next_our_bullet = false;
static int next_bullet_x = 0;
static int next_bullet_y = 0;
static uint next_bullet_dir = 0;
static bool audio_timer_hooked = false;
static bool allow_hitbox_fix = false;
static char temp_path[MAX_PATH];
int bullet_id = 106;
int bullet_speed = 70;
int last_new_rand_val = 0;
bool last_reset = false;
static void(__cdecl* SuperINI_Crypt)(char*, ulong, char*, ulong);
static void(__stdcall* AudioTimerCallback)(UINT, UINT, DWORD_PTR, DWORD_PTR, DWORD_PTR);
static void (*ProcessFrameRendering)(void);
static void(__cdecl* ExecuteObjectAction)(ActionHeader* action);
static HANDLE(__stdcall* CreateFileOrig)(LPCSTR _fn, DWORD dw_access, DWORD share_mode,
                                         LPSECURITY_ATTRIBUTES sec_attr, DWORD cr_d, DWORD flags,
                                         HANDLE template_);
static void(__stdcall* Ordinal_78)(void* hMainEngine, SpriteHandle* hSprite, BOOL bShow);
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam,
                                                             LPARAM lParam);

struct my_timeb {
    __time32_t time;
    unsigned short millitm;
    short timezone;
    short dstflag;
};

static short(__stdcall* DisplayRunObjectVPOrig)(void* pthis) = nullptr;
static short __stdcall DisplayRunObjectVPHook(void* pthis) {
    // Viewport.mfx display hook
    if (conf.no_vp)
        return 0;
    auto ret = DisplayRunObjectVPOrig(pthis);
    return ret;
}

static short(__stdcall* DisplayRunObjectPOrig)(void* pthis) = nullptr;
static short __stdcall DisplayRunObjectPHook(void* pthis) {
    // Perspective.mfx display hook
    if (conf.no_ps)
        return 0;
    auto ret = DisplayRunObjectPOrig(pthis);
    return ret;
}

static int(__cdecl* randOrig)() = nullptr;
static int __cdecl randHook() {
    // For MMKRandomPool.mfx (used very rarely)
    int ret;
    if (is_btas) {
        ret = btas::get_rng(RAND_MAX);
        // if (ret != RAND_MAX)
        //    return ret;
        // I don't think it's good to call orig rand
#ifdef _DEBUG
        cout << "warn: game used rand(): " << ret << "/" << RAND_MAX << "\n";
#endif
        return ret;
    }
    if (fix_rng && (lock_rng_range == 0 || lock_rng_range == (RAND_MAX + 1)))
        ret = (unsigned int)((float)RAND_MAX * fix_rng_val / 100.f);
    else
        ret = randOrig();
    last_new_rand_val = ret;
    return ret;
}

static int(__cdecl* _stricmpOrig)(const char* s1, const char* s2) = nullptr;
static int __cdecl _stricmpHook(const char* s1, const char* s2) {
    // DirBlur x3.fx fucks up collision for some reason
    if (conf.no_sh && (strcmp(s1, "CS_SinWave2.fx") == 0 || strcmp(s1, "DirBlur x3.fx") == 0 ||
                       strcmp(s1, "DropShadow.fx") == 0 || strcmp(s1, "FlipX.fx") == 0 ||
                       strcmp(s1, "Mosaic.fx") == 0 || strcmp(s1, "Outline.fx") == 0 ||
                       strcmp(s1, "PT_BlurAndAngle.fx") == 0)) {
        // shaders
        // Extra: Add, Invert, Sub, Mono, Blend, XOR, OR, AND
        // cout << "!!!: " << s1 << " " << s2 << '\n';
        // s1 = "Sub";
        // TODO: check FUN_00426f90
        return -1;
    } else if (conf.god && (strcmp(s1, "Die") == 0 || strcmp(s1, "die") == 0)) {
        // god mode
        return -1;
    } else if (conf.no_trans && strcmp(s1, "teleporting") == 0 && get_scene_id() != 10) {
        // no teleport effects (broken in world 4)
        return -1;
    }
    /*else if (conf.show_hitbox && strlen(s1) > 1) {
        cout << "test str: " << s1 << '\n';
    }*/
    // menuChosen, NameTags, jump, doublejump, teleporting, save, Save, shoot, shooot, restart,
    // KillAll, killboss
    auto ret = _stricmpOrig(s1, s2);
    return ret;
}

static HANDLE __stdcall CreateFileHook(LPCSTR _fn, DWORD dw_access, DWORD share_mode,
                                       LPSECURITY_ATTRIBUTES sec_attr, DWORD cr_d, DWORD flags,
                                       HANDLE template_) {
    if (!conf.keep_save)
        return CreateFileOrig(_fn, dw_access, share_mode, sec_attr, cr_d, flags, template_);
    if (!_fn)
        return NULL;
    char buf[MAX_PATH];
    if (c_ends_with(_fn, ".ini")) {
        // cout << "file open: " << _fn << ((dw_access & GENERIC_WRITE) ? " (write)" : " (read)") <<
        // std::endl; Use temp files when needed
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

static int(__cdecl* CreateObjectOrig)(ushort parentHandle, ushort objectInfoID, int posX, int posY,
                                      void* creationParam, ushort creationFlags, uint initialDir,
                                      int layerIndex);
static int __cdecl CreateObjectHook(ushort parentHandle, ushort objectInfoID, int posX, int posY,
                                    void* creationParam, ushort creationFlags, uint initialDir,
                                    int layerIndex) {
    if (objectInfoID == 243) {
        // Somewhy game spams this (kinda INI++) object
        // blocking it reduces ammount of crashes
        // cout << "save object spam prevented" << '\n';
        return -1;
    } else if ((objectInfoID == 269 || objectInfoID == 105) && conf.show_hitbox) {
        // Nyan cat trail, meat boy trail
        return -1;
    } else if (0) {
        // 7105 269 6756x1231
        cout << "create object " << parentHandle << ' ' << objectInfoID << ' ' << posX << 'x'
             << posY << '\n';
    }
    auto ret = CreateObjectOrig(parentHandle, objectInfoID, posX, posY, creationParam,
                                creationFlags, initialDir, layerIndex);
    // if (is_btas && objectInfoID == 106 && ret != -1)
    //    btas::reg_obj(ret);
    if (parentHandle == 28 && ret != -1) {
        // cout << "player h " << ret << std::endl;
    }
    return ret;
}

static void(__cdecl* LaunchObjectActionOrig)(ActionHeader* action, ObjectHeader* obj, int x, int y,
                                             uint direction);
static void __cdecl LaunchObjectActionHook(ActionHeader* action, ObjectHeader* obj, int x, int y,
                                           uint direction) {
    if (next_our_bullet) {
        action->objectToLaunchID = 106;
        obj = (ObjectHeader*)get_player_ptr(get_scene_id());
        x = next_bullet_x;
        y = next_bullet_y;
        direction = next_bullet_dir;
        // cout << "BULLET EVENT " << x << " " << y << " " << direction << std::endl;
        next_our_bullet = false;
    }
    if (action->objectToLaunchID == 106 && action->launchSpeed == 70) {
        action->objectToLaunchID = bullet_id;
        action->launchSpeed = bullet_speed;
        LaunchObjectActionOrig(action, obj, x, y, direction);
        action->objectToLaunchID = 106;
        action->launchSpeed = 70;
        return;
    }
    LaunchObjectActionOrig(action, obj, x, y, direction);
}

void launch_bullet(int x, int y, int dir) {
    auto obj = (ObjectHeader*)get_player_ptr(get_scene_id());
    if (!obj)
        return;
    ActionHeader action = {0};
    action.actionID = 0x1D;
    action.launchSpeed = 70;
    action.objectToLaunchID = 106;
    action.creatorID = 28;
    // action.size = 0;
    // action.eventCode = 1;
    if (dir == -1) {
        next_bullet_x = obj->xPos + (obj->hoCurrentDirection == 0 ? 8 : -8);
        next_bullet_y = obj->yPos - 10;
        next_bullet_dir = obj->hoCurrentDirection;
    } else {
        next_bullet_x = x;
        next_bullet_y = y;
        next_bullet_dir = (uint)dir;
    }
    // cout << "launching\n";
    next_our_bullet = true;
    ExecuteObjectAction(&action);
}

static unsigned int __stdcall SetCursorYHook(void* param_1, int param_2, void* pshit) {
    if (conf.no_cmove)
        return 0;
    BOOL uVar1;
    tagPOINT local_c;
    GetCursorPos(&local_c);
    uVar1 = SetCursorPos(param_2, local_c.y);
    return uVar1 & 0xffff0000;
}

static unsigned int __stdcall SetCursorXHook(void* param_1, int param_2, void* pshit) {
    if (conf.no_cmove)
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
    if (strcmp(cap, "I Wanna Be The Boshy") == 0) {
        // This happens only when chaning/resetting scene lul
        if (capturing)
            return FALSE;
    }
    return SetWindowTextAOrig(hwnd, cap);
}

static void(__cdecl* ActOrig)(ActionHeader* act);
static void __cdecl ActHook(ActionHeader* act) {
    // unused
    auto act2 = act;
    RunHeader* pState = *(RunHeader**)(mem::get_base() + 0x59a9c);
    // *(ushort*)(pState.currentExecutingEvent + 4) &= ~0x1e;
    auto cnt = (uint) * (byte*)(pState->currentExecutingEvent + 3);
    auto c = act->eventCode;
    // if (c >= 0 && c != 2 && c != 32 && c != 33 && c != 34 && c != 36 && c != 41 && c != 58 && c
    // != 61 && c != 57)
    //     cout << 'a' << act->eventCode << std::endl;
    auto a = act->actionID;
    if (c == 33 && a == 94 && cnt > 4) {
        cout << "begin " << act << std::endl;

        for (uint i = 0u; i < cnt; i++) {
            c = act->eventCode;
            a = act->actionID;
            if (c == -216 || c == -192) {
                act->eventCode = 0;
                act->actionID = 0;
            }
            c = act->eventCode;
            a = act->actionID;
            cout << act->creatorID << " " << act->launchSpeed << std::endl;
            act++;
        }
        // act2 += 1;
        // *(byte*)(pState->currentExecutingEvent + 3) -= 9;
        ActOrig(act2);
        return;
    }
    // 415740
    // 415330
    ActOrig(act2);
}

static bool hooks_inited = false;
static int(__stdcall* UpdateGameFrameOrig)() = nullptr;
static int __stdcall UpdateGameFrameHook() {
    // Main update hook
    if (!hooks_inited) {
        // Good point for init
        hooks_inited = true;
        try_to_init();
        if (is_btas)
            btas::init();
    }
    try_to_hook_graphics();

    if (is_btas && btas::on_before_update()) {
        // Paused, need to manually render
        auto ret = UpdateGameFrameOrig();
        ProcessFrameRendering();
        btas::on_after_update(false);
        if (!conf.direct_render)
            rec::rec_tick(nullptr);
        last_upd2 = false;
        return ret;
    }

    if (input_tick() && 0)
        return 0;
    ui::pre_update();

    if (conf.rapid_bind != -1 && get_scene_id() > 1 && MyKeyState(conf.rapid_bind))
        launch_bullet(-1, -1, -1);

    static int spawn_x = 0;
    static int spawn_y = 0;
    if (0) {
        // Used, used to get player spawn pos
        auto pp = (ObjectHeader*)get_player_ptr(get_scene_id());
        if (pp) {
            cout << get_scene_id() << ": (" << pp->xPos << ", " << pp->yPos << ")" << std::endl;
            spawn_x = pp->xPos;
            spawn_y = pp->yPos;
        }
    }

    auto ret = UpdateGameFrameOrig();

    if (ret != 0)
        allow_hitbox_fix = conf.show_hitbox;

    if (audio_timer_hooked) {
        // TODO: conf::tas_better_precise_audio
        if (1) {
            AudioTimerCallback(1337228, 0, 0, 0, 0);
        } else {
            static int audio_fake_timer = 0;
            audio_fake_timer += 20;
            if (audio_fake_timer >= 50) {
                audio_fake_timer -= 50;
                btas::offset_time(-audio_fake_timer);
                AudioTimerCallback(1337228, 0, 0, 0, 0);
                btas::offset_time(audio_fake_timer);
            }
        }
    }

    if (!is_btas && !show_menu && conf.tp_on_click && MyKeyState(VK_LBUTTON)) {
        // Teleport player with mouse
        int scene_id = get_scene_id();
        auto player = (ObjectHeader*)get_player_ptr(scene_id);
        if (player) {
            int x, y, w, h;
            get_win_size(w, h, true);
            get_cursor_pos_orig(x, y);
            // TODO: how to map cursor pos into game properly (scaling) (need to hook
            // Viewport.mfx?)?
            RunHeader* pState = *(RunHeader**)(mem::get_base() + 0x59a9c);
            player->xPos = pState->currentViewportX + x * 640 / w;
            player->yPos = pState->currentViewportY + y * 480 / h;
            if (0) {
                if (MyKeyState('A')) {
                    player->xPos = spawn_x;
                    player->yPos = spawn_y;
                }
            }
            player->redrawFlag = 1;
        }
    }

    if (is_btas)
        btas::on_after_update(false);
#ifndef _DEBUG
    ASS(!is_btas || !last_upd2 || conf.force_gdi);
#endif
    if (is_btas && last_upd2 && !conf.force_gdi) {
        // FIXME
        // 13.breplay: breaks on frame 114->115
        cout << "NO DRAW WTF!!!!!!!!!! ret=" << ret << '\n';
        // ProcessFrameRendering();
    }

    if (!conf.direct_render)
        rec::rec_tick(nullptr);

    processed_first = true;
    last_upd2 = false;
    return ret;
}

unsigned int(__cdecl* RandomOrig)(unsigned int maxv);
static unsigned int __cdecl RandomHook(unsigned int maxv) {
    // Main random func
    unsigned int ret;
    ASS(maxv != RAND_MAX);
    if (is_btas)
        ret = btas::get_rng(maxv);
    else if (fix_rng && (lock_rng_range == 0 || lock_rng_range == (int)maxv)) {
        if (fix_rng_val == 100.f)
            ret = (maxv != 0) ? (maxv - 1) : 0;
        else
            ret = (unsigned int)((float)maxv * fix_rng_val / 100.f);
    } else
        ret = RandomOrig(maxv);
    ui_register_rand(maxv, ret);
    return ret;
}

static int(__stdcall* MessageBoxAOrig)(HWND hWnd, LPCSTR lpText, LPCSTR lpCaption, UINT uType);
static int __stdcall MessageBoxAHook(HWND hWnd, LPCSTR lpText, LPCSTR lpCaption, UINT uType) {
    if (!conf.skip_msg)
        return MessageBoxAOrig(hWnd, lpText, lpCaption, uType);
    // cout << lpText << std::endl;
    return IDNO;
}

static LRESULT(__stdcall* MainWindowProcOrig)(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
static LRESULT __stdcall MainWindowProcHook(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    if (1) {
        if (is_btas && uMsg == WM_DROPFILES)
            return 0;
        if (is_btas && uMsg == WM_GETMINMAXINFO &&
            (conf.force_size[0] != 0 || conf.force_size[1] != 0)) {
            MINMAXINFO* mmi = (MINMAXINFO*)lParam;
            mmi->ptMaxTrackSize.x = conf.force_size[0] + 32;
            mmi->ptMaxTrackSize.y = conf.force_size[1] + 64;
            return 0;
        }
        if (uMsg == WM_KEYDOWN) {
            if (wParam == (WPARAM)conf.menu_hotkey)
                show_menu = !show_menu;
        }
        if (is_btas && (uMsg == WM_KEYDOWN || uMsg == WM_KEYUP)) {
            // cout << "1 " << (uMsg == WM_KEYDOWN) << std::endl;
            btas::on_key((int)wParam, uMsg == WM_KEYDOWN);
        }
        if (!b_loading_saving_state)
            ImGui_ImplWin32_WndProcHandler(::hwnd, uMsg, wParam, lParam);
    }
    auto ret = MainWindowProcOrig(hWnd, uMsg, wParam, lParam);
    return ret;
}

static LRESULT(__stdcall* EditWindowProcOrig)(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
static LRESULT __stdcall EditWindowProcHook(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    if (1) {
        if (is_btas && uMsg == WM_DROPFILES)
            return 0;
        if (is_btas && uMsg == WM_GETMINMAXINFO &&
            (conf.force_size[0] != 0 || conf.force_size[1] != 0)) {
            MINMAXINFO* mmi = (MINMAXINFO*)lParam;
            mmi->ptMaxTrackSize.x = conf.force_size[0] + 32;
            mmi->ptMaxTrackSize.y = conf.force_size[1] + 64;
            return 0;
        }
        if (uMsg == WM_KEYDOWN) {
            if (wParam == (WPARAM)conf.menu_hotkey)
                show_menu = !show_menu;
        }
        if (is_btas && (uMsg == WM_KEYDOWN || uMsg == WM_KEYUP)) {
            // cout << "2 " << (uMsg == WM_KEYDOWN) << std::endl;
            btas::on_key((int)wParam, uMsg == WM_KEYDOWN);
        }
        if (!b_loading_saving_state)
            ImGui_ImplWin32_WndProcHandler(::mhwnd, uMsg, wParam, lParam);
    }
    if (is_btas && uMsg > WM_MOUSEFIRST && uMsg < WM_MOUSELAST)
        return 0;
    auto ret = EditWindowProcOrig(hWnd, uMsg, wParam, lParam);
    return ret;
}

static void __stdcall FlushInputQueueHook(void) {
    // cout << "queue\n";
}

static BOOL __stdcall InternetGetConnectedStateHook(LPDWORD lpdwFlags, DWORD dwReserved) {
    // For sure
    *lpdwFlags = 0x20;
    return FALSE;
}

static HRESULT __stdcall DirectDrawCreateHook(void* lpGUID, void* lplpDD, void* pUnkOuter) {
    // We support only D3D9
    cout << "Failing DirectDrawCreateHook\n";
    return 0x8007000E;
}

static BOOL __stdcall GetUserNameAHook(LPSTR lpBuffer, LPDWORD pcbBuffer) {
    // For sure
    // cout << "GetUserNameAHook\n";
    strcpy(lpBuffer, "BTAS");
    return TRUE;
}

static int(__stdcall* GetSystemMetricsOrig)(int nIndex);
static int __stdcall GetSystemMetricsHook(int nIndex) {
    // Still don't know why window size differs between Windows XP and newer
    switch (nIndex) {
        /*
        case SM_CXSCREEN:
        case SM_CXVIRTUALSCREEN:
            return 3840;
        case SM_CYSCREEN:
        case SM_CYVIRTUALSCREEN:
            return 2160;
            */
    case SM_CMONITORS:
        return 1;
    case SM_SAMEDISPLAYFORMAT:
        return 1;
    case SM_CXVSCROLL:
    case SM_CYHSCROLL:
    case SM_CYCAPTION:
    case SM_CYSIZE:
    case SM_CXFRAME:
    case SM_CYFRAME:
    case SM_CYVSCROLL:
    case SM_CXHSCROLL:
        return 0;
    default:
        return GetSystemMetricsOrig(nIndex);
    }
}

static int(__stdcall* FindBestModeCallbackOrig)(int* candidate, DisplaySearchCriteria* best);
static int __stdcall FindBestModeCallbackHook(int* candidate, DisplaySearchCriteria* best) {
    auto ret = FindBestModeCallbackOrig(candidate, best);
    if (conf.full_size[0] < 0)
        best->targetWidth = best->bestMatchedWidth = GetSystemMetricsOrig(0);
    else if (conf.full_size[0] > 0)
        best->targetWidth = best->bestMatchedWidth = conf.full_size[0];
    if (conf.full_size[1] < 0)
        best->targetHeight = best->bestMatchedHeight = GetSystemMetricsOrig(1);
    else if (conf.full_size[1] > 0)
        best->targetHeight = best->bestMatchedHeight = conf.full_size[1];
    return 0;
}

static int WINAPI WSAStartupHook(WORD wVersionRequired, void* lpWSAData) {
    // cout << "Failing WSAStartup\n";
    return 10092L;
}

static HMODULE(__stdcall* LoadLibraryAOrig)(LPCSTR lpLibFileName);
static HMODULE __stdcall LoadLibraryAHook(LPCSTR lpLibFileName) {
    if (is_btas && c_ends_with(lpLibFileName, "mmf2d3d8.dll")) {
        cout << "Failing to load mmf2d3d8.dll\n";
        return nullptr;
    } else if (c_ends_with(lpLibFileName, "uxtheme.dll")) {
        // Keep modern theming
        return nullptr;
    }
    HMODULE ret = LoadLibraryAOrig(lpLibFileName);
    // Disable extra threads for performance
    uint8_t temp = 0xeb;
    const uint8_t buf[] = {0x90, 0x90, 0x90, 0x90, 0x90, 0x90};
    DWORD bW;
    if (is_btas && c_ends_with(lpLibFileName, "mmfs2.dll")) {
        // hook(mem::addr("DirectDrawCreate", "ddraw.dll"), DirectDrawCreateHook);
        // TODO: hook mmfs2 dll funcs directly here
        audio_init();
        enable_hook();
    } else if (is_btas && c_ends_with(lpLibFileName, "Lacewing.mfx")) {
        // Disable thread creation as early as possible (for better stability)
        ASS(WriteProcessMemory(hproc, (LPVOID)(mem::get_base("Lacewing.mfx") + 0xb202), buf, 5,
                               &bW) != 0 &&
            bW == 5);
        ASS(WriteProcessMemory(hproc, (LPVOID)(mem::get_base("Lacewing.mfx") + 0xb209), &temp, 1,
                               &bW) != 0 &&
            bW == 1);
        iat_hook("ws2_32.dll", "WSAStartup", WSAStartupHook);
    } else if (is_btas && c_ends_with(lpLibFileName, "Yaso.mfx")) {
        iat_hook("wininet.dll", "InternetGetConnectedState", InternetGetConnectedStateHook);
        iat_hook("advapi32.dll", "GetUserNameA", GetUserNameAHook);
    }
    enable_iat();
    return ret;
}

static HMODULE(__stdcall* LoadLibraryWOrig)(LPCWSTR lpLibFileName);
static HMODULE __stdcall LoadLibraryWHook(LPCWSTR lpLibFileName) {
    // std::wcout << L"load w hook: " << lpLibFileName << L'\n';
    HMODULE ret = LoadLibraryWOrig(lpLibFileName);
    enable_iat();
    return ret;
}

static HINSTANCE __stdcall ShellExecuteAHook(HWND hwnd, LPCSTR lpOperation, LPCSTR lpFile,
                                             LPCSTR lpParameters, LPCSTR lpDirectory,
                                             INT nShowCmd) {
    // No need to open URLs
    return nullptr;
}

static HWND __stdcall GetActiveWindowHook() {
    // In BTAS, we are always focused!
    return ::hwnd;
}

static HWND __stdcall SetFocusHook(HWND hWnd) {
    // In BTAS, we are always focused!
    return ::hwnd;
}

static DWORD __stdcall GetTickCountHook() { return (DWORD)btas::get_time(); }

static time_t __cdecl timeHook(time_t* tloc) {
    if (tloc)
        *tloc = (time_t)btas::get_time();
    return (time_t)btas::get_time();
}

static void __cdecl _ftimeHook(struct my_timeb* timeptr) {
    if (timeptr) {
        timeptr->time = (__time32_t)btas::get_time() / 1000;
        timeptr->millitm = (unsigned short)(btas::get_time() % 1000);
        timeptr->timezone = 0;
        timeptr->dstflag = 0;
    }
}

BOOL(__stdcall* QueryPerformanceFrequencyOrig)(LARGE_INTEGER* ret) = QueryPerformanceFrequency;
static BOOL __stdcall QueryPerformanceFrequencyHook(LARGE_INTEGER* ret) {
    ret->QuadPart = 1000;
    return TRUE;
}

BOOL(__stdcall* QueryPerformanceCounterOrig)(LARGE_INTEGER* ret) = QueryPerformanceCounter;
static BOOL __stdcall QueryPerformanceCounterHook(LARGE_INTEGER* ret) {
    if (!hooks_inited)
        return QueryPerformanceCounterOrig(ret);
    ret->QuadPart = (LONGLONG)btas::get_time();
    return TRUE;
}

static void __stdcall GetSystemTimeAsFileTimeHook(LPFILETIME tm) {
    ((LARGE_INTEGER*)tm)->QuadPart = (LONGLONG)btas::get_time();
}

static BOOL __stdcall GetProcessTimesHook(HANDLE hProcess, LPFILETIME lpCreationTime,
                                          LPFILETIME lpExitTime, LPFILETIME lpKernelTime,
                                          LPFILETIME lpUserTime) {
    ((LARGE_INTEGER*)lpCreationTime)->QuadPart = 0;
    ((LARGE_INTEGER*)lpKernelTime)->QuadPart = 0;
    ((LARGE_INTEGER*)lpUserTime)->QuadPart = (LONGLONG)btas::get_time();
    ((LARGE_INTEGER*)lpExitTime)->QuadPart = (LONGLONG)btas::get_time();
    return TRUE;
}

static MMRESULT(__stdcall* timeSetEventOrig)(UINT, UINT, LPTIMECALLBACK, DWORD_PTR, UINT);
static MMRESULT __stdcall timeSetEventHook(UINT uDelay, UINT uResolution, LPTIMECALLBACK lpTimeProc,
                                           DWORD_PTR dwUser, UINT fuEvent) {
    if (uDelay == 50 && uResolution == 10) {
        // Audio processing timer
        // Hacky (i think no need to check for AudioTimerCallback
        ASS(!audio_timer_hooked);
        audio_timer_hooked = true;
        AudioTimerCallback =
            reinterpret_cast<decltype(AudioTimerCallback)>(mem::get_base("mmfs2.dll") + 0x42940);
        return 1337228;
    }
    return timeSetEventOrig(uDelay, uResolution, lpTimeProc, dwUser, fuEvent);
}

static MMRESULT(__stdcall* timeKillEventOrig)(UINT);
static MMRESULT __stdcall timeKillEventHook(UINT uTimerID) {
    if (uTimerID == 1337228) {
        ASS(audio_timer_hooked);
        audio_timer_hooked = false;
        return TIMERR_NOERROR;
    }
    return timeKillEventOrig(uTimerID);
}

static void(__cdecl* DestroyObjectOrig)(int handle);
static void __cdecl DestroyObjectHook(int handle) { DestroyObjectOrig(handle); }

static int(__cdecl* GetCollidingObjectListOrig)(ObjectHeader*, uint, uint, float, float, int, int,
                                                ObjectHeader***, int);
static int __cdecl GetCollidingObjectListHook(ObjectHeader* obj, uint angle, uint scale,
                                              float scaleX, float scaleY, int x, int y,
                                              ObjectHeader*** outList, int filterGroup) {
    // bullet created by player for sure
    if (obj && obj->parentID == 28 && obj->oiHandle == 106 && obj->spriteHandle) {
        // cout << obj->oiHandle << " " << obj->spriteHandle->flags << std::endl;
        // Bullet fix (check for SF_INACTIVE => means bullet is broken)
        if (obj->spriteHandle->flags == 0x20000008) {
            // cout << "fixed\n";
            obj->spriteHandle->flags &= ~0x8; // remove SF_INACTIVE
            obj->spriteHandle->flags |= 0x40; // add SF_RECALC
            obj->spriteHandle->flags |= 0x1;  // add SF_RECREATEMASK
        }
    }
    auto ret =
        GetCollidingObjectListOrig(obj, angle, scale, scaleX, scaleY, x, y, outList, filterGroup);
    return ret;
}

static DWORD __stdcall GetTempPathAHook(DWORD nBufferLength, LPSTR lpBuffer) {
    // Why just not to use %GAME_PATH%/temp folder for temp files?
    if (!CreateDirectoryA(temp_path, nullptr) && GetLastError() != ERROR_ALREADY_EXISTS)
        return 0;
    strcpy(lpBuffer, temp_path);
    return 4;
}

static void __stdcall DragAcceptFilesHook(HWND hWnd, BOOL fAccept) {
    // cout << "DragAcceptFilesHook\n";
}

static HWND(__stdcall* CreateWindowExAOrig)(DWORD, LPCSTR, LPCSTR, DWORD, int, int, int, int, HWND,
                                            HMENU, HINSTANCE, LPVOID);

static HWND __stdcall CreateWindowExAHook(DWORD dwExStyle, LPCSTR lpClassName, LPCSTR lpWindowName,
                                          DWORD dwStyle, int X, int Y, int nWidth, int nHeight,
                                          HWND hWndParent, HMENU hMenu, HINSTANCE hInstance,
                                          LPVOID lpParam) {
    if (reinterpret_cast<size_t>(lpClassName) <= 0xFFFF)
        return CreateWindowExAOrig(dwExStyle, lpClassName, lpWindowName, dwStyle, X, Y, nWidth,
                                   nHeight, hWndParent, hMenu, hInstance, lpParam);
    if (strcmp(lpClassName, "Mf2MainClassTh") == 0) {
        HWND ret = CreateWindowExAOrig(dwExStyle, lpClassName, lpWindowName, dwStyle, X, Y, nWidth,
                                       nHeight, hWndParent, hMenu, hInstance, lpParam);
        ::hwnd = ret;
        return ret;
    } else if (strcmp(lpClassName, "Mf2EditClassTh") == 0) {
        HWND ret = CreateWindowExAOrig(dwExStyle, lpClassName, lpWindowName, dwStyle, X, Y, nWidth,
                                       nHeight, hWndParent, hMenu, hInstance, lpParam);
        ::mhwnd = ret;
        return ret;
    } else if (!is_replay &&
               (strcmp(lpClassName, "EDIT") == 0 || strcmp(lpClassName, "COMBOBOX") == 0 ||
                strcmp(lpClassName, "LISTBOX") == 0 ||
                strcmp(lpClassName, "omgwtfbbqColorButton") == 0 ||
                strcmp(lpClassName, "omgwtfbbqColorSelector") == 0)) {
        // cout << "CreateWindowExAHook " << lpClassName << " -> STATIC\n";
        lpClassName = "STATIC";
    }
    // cout << "create " << lpClassName << "\n";
    HWND ret = CreateWindowExAOrig(dwExStyle, lpClassName, lpWindowName, dwStyle, X, Y, nWidth,
                                   nHeight, hWndParent, hMenu, hInstance, lpParam);
    return ret;
}

static void(__cdecl* HideObjectIfNeededOrig)(ObjectHeader* obj);
static void __cdecl HideObjectIfNeededHook(ObjectHeader* obj) {
    // Ugly shit for showing hitbox (original player)
    int mvtOffset = obj->hoAdpOffset;
    ushort* statusFlags = (ushort*)((int)&obj->eventTriggerTable + mvtOffset);
    if (conf.show_hitbox && obj && obj == get_player_ptr(get_scene_id())) {
        RunHeader* pState = *(RunHeader**)(mem::get_base() + 0x59a9c);
        obj->runtimeFlags = obj->runtimeFlags & 0xdf;
        obj->isDirty = 1;
        obj->animFinished = 0;
        obj->collisionFlags = 0;
        Ordinal_78(pState->hMainEngine, obj->spriteHandle, 1);
        if (!allow_hitbox_fix)
            return;
        int count = 2;
        for (int i = pState->activeObjectCount - 1; i > 20; i--) {
            ObjectHeader* ptr = pState->objectList[i * 2];
            if (!ptr || obj->handle == ptr->handle || obj->flags != 58164 ||
                std::abs(obj->xPos - ptr->xPos) > 0 || std::abs(obj->yPos - ptr->yPos) > 0)
                continue;
            statusFlags = (ushort*)((int)&ptr->eventTriggerTable + mvtOffset);
            *statusFlags &= ~1;
            HideObjectIfNeededOrig(ptr);
            allow_hitbox_fix = false;
            count--;
            if (count == 0)
                break;
        }
        return;
    }
    HideObjectIfNeededOrig(obj);
}

BOOL(__stdcall* GetClientRectOrig)(HWND hWnd, LPRECT lpRect) = GetClientRect;
BOOL __stdcall GetClientRectHook(HWND hWnd, LPRECT lpRect) {
    if (hWnd == hwnd && (conf.force_size[0] != 0 || conf.force_size[1] != 0)) {
        lpRect->left = 0;
        lpRect->top = 0;
        lpRect->right = conf.force_size[0];
        lpRect->bottom = conf.force_size[1];
        return TRUE;
    }
    return GetClientRectOrig(hWnd, lpRect);
}

static BOOL(__stdcall* AdjustWindowRectExOrig)(LPRECT, DWORD, BOOL, DWORD);
static BOOL __stdcall AdjustWindowRectExHook(LPRECT lpRect, DWORD dwStyle, BOOL bMenu,
                                             DWORD dwExStyle) {
    if (conf.force_size[0] != 0 || conf.force_size[1] != 0) {
        lpRect->left = 0;
        lpRect->top = 0;
        lpRect->right = conf.force_size[0];
        lpRect->bottom = conf.force_size[1];
    }
    return AdjustWindowRectExOrig(lpRect, dwStyle, bMenu, dwExStyle);
}

static bool is_encrypted_ini(char* data) {
    if (c_starts_with(data, "[Options]") || c_starts_with(data, "[License]") ||
        c_starts_with(data, "[Achievements]") || c_starts_with(data, "[Check]"))
        return false;
    return true;
}

static void __cdecl SuperINI_CryptChecked(char* inp, ulong inplen, char* key, ulong keylen) {
    if (inplen < 16 || is_encrypted_ini(inp))
        SuperINI_Crypt(inp, inplen, key, keylen);
}

void init_game_loop() {
    // Executed as early as possible
    ProcessFrameRendering =
        reinterpret_cast<decltype(ProcessFrameRendering)>(mem::get_base() + 0x1ebf0);
    if (!UpdateGameFrameOrig)
        hook(mem::get_base() + 0x365a0, UpdateGameFrameHook, &UpdateGameFrameOrig);
    iat_hook("kernel32.dll", "LoadLibraryA", LoadLibraryAHook, &LoadLibraryAOrig);
    iat_hook("kernel32.dll", "LoadLibraryW", LoadLibraryWHook, &LoadLibraryWOrig);
    if (is_btas) {
        if (is_hourglass && 0)
            timeGetTimeOrig = (decltype(timeGetTimeOrig))mem::addr("timeGetTime", "winmm.dll");
        else {
            iat_hook("winmm.dll", "timeGetTime", timeGetTimeHook, &timeGetTimeOrig);
            iat_hook("msvcrt.dll", "time", timeHook);
            iat_hook("msvcrt.dll", "_ftime", _ftimeHook);
            iat_hook("kernel32.dll", "GetTickCount", GetTickCountHook);
        }
        iat_hook("shell32.dll", "DragAcceptFiles", DragAcceptFilesHook);
        iat_hook("shell32.dll", "ShellExecuteA", ShellExecuteAHook);
        iat_hook("user32.dll", "GetClientRect", GetClientRectHook, &GetClientRectOrig);
        iat_hook("user32.dll", "AdjustWindowRectEx", AdjustWindowRectExHook,
                 &AdjustWindowRectExOrig);
        iat_hook("user32.dll", "SetFocus", SetFocusHook);
        iat_hook("user32.dll", "GetActiveWindow", GetActiveWindowHook);
        iat_hook("user32.dll", "GetFocus", GetActiveWindowHook);
        iat_hook("user32.dll", "CreateWindowExA", CreateWindowExAHook, &CreateWindowExAOrig);
        iat_hook("user32.dll", "GetSystemMetrics", GetSystemMetricsHook, &GetSystemMetricsOrig);
        // Ok this might be overkill
        // iat_hook("kernel32.dll", "QueryPerformanceFrequency",
        // QueryPerformanceFrequencyHook, &QueryPerformanceFrequencyOrig);
        // iat_hook("kernel32.dll", "QueryPerformanceCounter", QueryPerformanceCounterHook,
        // &QueryPerformanceCounterOrig);
        hook(mem::get_base() + 0x40720, FlushInputQueueHook);
        // hook(mem::get_base() + 0x1f730, DestroyObjectHook, &DestroyObjectOrig);
        // hook(mem::get_base() + 0x485d0, ActHook, &ActOrig);
        // hook(mem::get_base() + 0x15740, EvaluateCondition, &EvaluateConditionO);
        auto cwd_len = GetCurrentDirectoryA(MAX_PATH, temp_path);
        ASS(cwd_len > 0);
        strcpy(temp_path + cwd_len, "\\temp");
        iat_hook("kernel32.dll", "GetTempPathA", GetTempPathAHook);
        btas::pre_init();
    }
    if (conf.force_gdi) {
        // Force software renderer if needed somewhy
        *(short*)(mem::get_base() + 0x59a28) = 1;
        *(short*)(mem::get_base() + 0x59a2a) = 8;
    }
    if ((conf.tas_mode || is_btas) && conf.au_mth) {
        iat_hook("winmm.dll", "timeSetEvent", timeSetEventHook, &timeSetEventOrig);
        iat_hook("winmm.dll", "timeKillEvent", timeKillEventHook, &timeKillEventOrig);
    }
    // Actually might be useful for normal mod menu
    if (!is_btas && !is_hourglass)
        hook(mem::get_base() + 0x47140, GetCollidingObjectListHook, &GetCollidingObjectListOrig);
    enable_hook();
    enable_iat();
}

void init_temp_saves() {
    // Cleanup temp files
    DeleteFileA("onlineLicense.tmp.ini");
    DeleteFileA("animation.tmp.ini");
    DeleteFileA("options.tmp.ini");
    DeleteFileA("saveFile.tmp.ini");
    DeleteFileA("SaveFile1.tmp.ini");
    DeleteFileA("SaveFile2.tmp.ini");
    DeleteFileA("SaveFile3.tmp.ini");
}

static short __stdcall HandleRunObjectIniHook(void* pthis) {
    size_t me = (size_t)pthis;
    if (JustKeyState('U') == 1) {
        cout << "d\n";
        /*
        short(__stdcall * actionOpenDialog)(void*, long, long) =
            decltype(actionOpenDialog)(mem::get_base("INI++.mfx") + 0xae32);
        int(__stdcall * doBox)(void*) = decltype(doBox)(mem::get_base("INI++.mfx") + 0xaddb);
        doBox(pthis);
        */
    }
    return 0;
}

static int(__stdcall* CreateRunObjectIniOrig)(size_t pObject, size_t pEditData, int dummy);
static int __stdcall CreateRunObjectIniHook(size_t pObject, size_t pEditData, int dummy) {
    if (!b_loading_saving_state)
        return CreateRunObjectIniOrig(pObject, pEditData, dummy);
    bool fixed = false;
    std::string def_fp = (char*)(pEditData + 0x16);
    if (def_fp != "onlineLicense.ini" && def_fp != "options.ini") {
        // cout << "fixing save slot\n";
        // cout << "started fix\n";
        RunApp* gState = *(RunApp**)(mem::get_base() + 0x59a94);
        // cout << "fixing: " << (char*)gState->pRawStringTable[0] << std::endl;
        fixed = *(uint8_t*)(pEditData + 0x14) == 0;
        *(uint8_t*)(pEditData + 0x14) = 1;
        strcpy((char*)(pEditData + 0x16), (char*)gState->pRawStringTable[0]);
    }
    auto ret = CreateRunObjectIniOrig(pObject, pEditData, dummy);
    // size_t ini = *(size_t*)(pObject + 0x168);
    if (fixed)
        *(uint8_t*)(pEditData + 0x14) = 0;
    // cout << "ini create " << (int)*(uint8_t*)(pEditData + 0x14) << '\n';
    return ret;
}

void init_simple_hacks() {
    // First-frame init
    input_init();
    if (conf.cap_au)
        ASS(CreateDirectoryW(L"temp_audio", nullptr) != 0 ||
            GetLastError() == ERROR_ALREADY_EXISTS);
    if (!is_hourglass && (is_btas || !conf.tas_mode)) {
        // hook(mem::get_base() + 0x43e30, MainWindowProcHook, &MainWindowProcOrig);
        // hook(mem::get_base() + 0x41ba0, EditWindowProcHook, &EditWindowProcOrig);
        MainWindowProcOrig =
            (WNDPROC)SetWindowLongPtrA(::hwnd, GWLP_WNDPROC, (LONG)MainWindowProcHook);
        EditWindowProcOrig =
            (WNDPROC)SetWindowLongPtrA(::mhwnd, GWLP_WNDPROC, (LONG)EditWindowProcHook);
    }
    if (is_btas && (!is_hourglass || 1)) {
        iat_hook("kernel32.dll", "GetSystemTimeAsFileTime", GetSystemTimeAsFileTimeHook);
        iat_hook("kernel32.dll", "GetProcessTimes", GetProcessTimesHook);
    }
    ExecuteObjectAction = (decltype(ExecuteObjectAction))(mem::get_base() + 0x15180);
    Ordinal_78 = (decltype(Ordinal_78))(mem::get_base("mmfs2.dll") + 0x116e0);
    // cout << std::hex << (mem::get_base("ForEach.mfx")) << std::endl;
    // hook(mem::get_base("INI++.mfx") + 0x15681, SuperINI_CryptHook);
    hook(mem::addr("DisplayRunObject", "Viewport.mfx"), DisplayRunObjectVPHook,
         &DisplayRunObjectVPOrig);
    hook(mem::addr("DisplayRunObject", "Perspective.mfx"), DisplayRunObjectPHook,
         &DisplayRunObjectPOrig);
    iat_hook("msvcrt.dll", "rand", randHook, &randOrig);
    iat_hook("msvcrt.dll", "_stricmp", _stricmpHook, &_stricmpOrig);
    iat_hook("kernel32.dll", "CreateFileA", CreateFileHook, &CreateFileOrig);
    iat_hook("user32.dll", "SetWindowTextA", SetWindowTextAHook, &SetWindowTextAOrig);
    iat_hook("user32.dll", "MessageBoxA", MessageBoxAHook, &MessageBoxAOrig);
    hook(mem::get_base("kcmouse.mfx") + 0x1103, SetCursorYHook);
    hook(mem::get_base("kcmouse.mfx") + 0x1125, SetCursorXHook);
    hook(mem::get_base() + 0x1f890, RandomHook, &RandomOrig);
    if (!is_btas)
        hook(mem::get_base() + 0x10ac0, LaunchObjectActionHook, &LaunchObjectActionOrig);
    if (conf.no_save_object_spamming)
        hook(mem::get_base() + 0x1e2d0, CreateObjectHook, &CreateObjectOrig);
    hook(mem::get_base() + 0x20f0, HideObjectIfNeededHook, &HideObjectIfNeededOrig);
    hook(mem::get_base() + 0x3f550, FindBestModeCallbackHook, &FindBestModeCallbackOrig);
    // hook(mem::addr("HandleRunObject", "INI++.mfx"), HandleRunObjectIniHook);
    hook(mem::addr("CreateRunObject", "INI++.mfx"), CreateRunObjectIniHook,
         &CreateRunObjectIniOrig);

    // hook(mem::get_base("INI++.mfx") + 0x153e0, Ini_Item_Compare);
    // hook(mem::get_base("INI++.mfx") + 0x1d980, IniState_GetValueHook, &IniState_GetValueOrig);
    // Get_CStr = (decltype(Get_CStr))(mem::get_base("INI++.mfx") + 0x32f0);
    // Patch to support loading unencrypted save files
    if (1) {
        SuperINI_Crypt = (decltype(SuperINI_Crypt))(mem::get_base("INI++.mfx") + 0x15681);
        DWORD bW;
        size_t addr = (size_t)((uint64_t)SuperINI_CryptChecked -
                               ((uint64_t)(mem::get_base("INI++.mfx") + 0x1a5f7) + 5));
        ASS(WriteProcessMemory(hproc, (void*)(mem::get_base("INI++.mfx") + 0x1a5f7 + 1), &addr, 4,
                               &bW) &&
            bW == 4);
        update_save_encryption();
    }
    if (!is_btas)
        init_temp_saves();
    refresh_admin_mode();
    enable_iat();
}
