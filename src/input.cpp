#define WIN32_LEAN_AND_MEAN
#include "input.hpp"
#include "btas.hpp"
#include "conf.hpp"
#include "fs.hpp"
#include "ghidra_headers.h"
#include "hook.hpp"
#include "mem.hpp"
#include "ui.hpp"
#include "utils.hpp"
#include <Windows.h>
#include <iostream>
#include <map>
#include <vector>

using std::cout;

namespace config {
extern std::map<int, std::vector<InputEvent>> mb;
}
extern HWND hwnd;
extern HWND mhwnd;
LRESULT(__stdcall* SusProc)(HWND, UINT, WPARAM, LPARAM) = nullptr;

static int cur_x = -100;
static int cur_y = -100;

BOOL(__stdcall* GetCursorPosOrig)(LPPOINT p);
static BOOL __stdcall GetCursorPosHook(LPPOINT p) {
    if (is_btas) {
        int w, h;
        get_win_size(w, h, true);
        btas::my_mouse_pos(p->x, p->y);
        p->x = (long)(p->x * (float)w / 640.f);
        p->y = (long)(p->y * (float)h / 480.f);
        return ClientToScreen(hwnd, p);
    }
    if (!conf.emu_mouse && (!show_menu || conf.tas_mode))
        return GetCursorPosOrig(p);
    if (show_menu && !conf.emu_mouse) {
        p->x = -100;
        p->y = -100;
    } else {
        p->x = cur_x;
        p->y = cur_y;
    }
    return ClientToScreen(hwnd, p);
}

SHORT(__stdcall* GetKeyStateOrig)(int k);
static SHORT __stdcall GetKeyStateHook(int k) {
    if (is_btas)
        return btas::TasGetKeyState(k);
    if ((show_menu && !conf.tas_mode && !conf.input_in_menu) || b_loading_saving_state)
        return 0;
    return GetKeyStateOrig(k);
}

SHORT(__stdcall* GetAsyncKeyStateOrig)(int k);
static SHORT __stdcall GetAsyncKeyStateHook(int k) {
    if (is_btas)
        return btas::TasGetKeyState(k);
    if ((show_menu && !conf.tas_mode && !conf.input_in_menu) || b_loading_saving_state)
        return 0;
    return GetAsyncKeyStateOrig(k);
}

bool input_tick() {
    bool ret = false;
    if (is_btas)
        return ret;
    int w, h;
    get_win_size(w, h, true);
    // TODO: better way to handle??? (BTAS way?) (still need to be compatible with hourglass)
    for (auto it = config::mb.begin(); it != config::mb.end(); it++) {
        if (JustKeyState(it->first) == 1) {
            // cout << "sus_click\n";
            for (auto eit = it->second.begin(); eit != it->second.end(); eit++) {
                if (eit->type == eit->CLICK) {
                    cur_x = (int)(eit->click.x * (float)w / 640.f);
                    cur_y = (int)(eit->click.y * (float)h / 480.f);
                    if (cur_x < 0 || cur_y < 0)
                        continue;
                    SusProc(mhwnd, WM_LBUTTONDOWN, 0, 0);
                    SusProc(mhwnd, WM_LBUTTONUP, 0, 0);
                } else if (eit->type == eit->SAVE) {
                    bfs::File file(*eit->state.fn, 1);
                    if (file.is_open()) {
                        state_save(&file);
                        ret = true;
                    }
                } else if (eit->type == eit->LOAD) {
                    bfs::File file(*eit->state.fn, 0);
                    short prev_seed = conf.reset_rng
                                          ? (*(RunHeader**)(mem::get_base() + 0x59a9c))->RandomSeed
                                          : 0;
                    if (file.is_open()) {
                        b_loading_saving_state = true;
                        state_load(&file);
                        b_loading_saving_state = false;
                        if (conf.reset_rng) {
                            // cout << "reset rng\n";
                            (*(RunHeader**)(mem::get_base() + 0x59a9c))->RandomSeed = prev_seed;
                        }
                        ret = true;
                        // pState.rhNextFrame = 0x65;
                    }
                }
            }
        }
    }
    return ret;
}

void input_init() {
    SusProc = reinterpret_cast<decltype(SusProc)>(mem::get_base() + 0x41ba0);
    iat_hook("user32.dll", "GetCursorPos", GetCursorPosHook, &GetCursorPosOrig);
    iat_hook("user32.dll", "GetKeyState", GetKeyStateHook, &GetKeyStateOrig);
    iat_hook("user32.dll", "GetAsyncKeyState", GetAsyncKeyStateHook, &GetAsyncKeyStateOrig);
}
