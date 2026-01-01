#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <iostream>
#include "input.hpp"
#include "init.hpp"
#include "hook.hpp"
#include "conf.hpp"
#include "mem.hpp"
#include "ui.hpp"
#include "fs.hpp"
#include "utils.hpp"
#include "btas.hpp"

using std::cout;

namespace conf {
    extern std::map<int, std::vector<InputEvent>> mb;
}
extern HWND hwnd;
extern HWND mhwnd;
static LRESULT(__stdcall* SusProc)(HWND param_1, UINT param_2, WPARAM param_3, LPARAM param_4) = nullptr;

static int cur_x = -100;
static int cur_y = -100;

BOOL(__stdcall* GetCursorPosOrig)(LPPOINT p);
static BOOL __stdcall GetCursorPosHook(LPPOINT p) {
    if (is_btas) {
        p->x = cur_x;
        p->y = cur_y;
        return ClientToScreen(hwnd, p);
    }
    if (!conf::emu_mouse && (!show_menu || conf::tas_mode))
        return GetCursorPosOrig(p);
    if (show_menu && !conf::emu_mouse) {
        p->x = -100;
        p->y = -100;
    } 
    else {
        p->x = cur_x;
        p->y = cur_y;
    }
    return ClientToScreen(hwnd, p);
}

SHORT(__stdcall* GetKeyStateOrig)(int k);
static SHORT __stdcall GetKeyStateHook(int k) {
    if (is_btas)
        return btas::TasGetKeyState(k);
    if (show_menu && !conf::tas_mode && !conf::input_in_menu)
        return 0;
    return GetKeyStateOrig(k);
}

SHORT(__stdcall* GetAsyncKeyStateOrig)(int k);
static SHORT __stdcall GetAsyncKeyStateHook(int k) {
    if (is_btas)
        return btas::TasGetKeyState(k);
    if (show_menu && !conf::tas_mode && !conf::input_in_menu)
        return 0;
    return GetAsyncKeyStateOrig(k);
}

void input_tick() {
    if (is_btas)
        return;
    int w, h;
    get_win_size(w, h);
    // TODO: better way to handle??? (BTAS way?)
    for (auto it = conf::mb.begin(); it != conf::mb.end(); it++) {
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
                }
                else if (eit->type == eit->SAVE) {
                    bfs::File file(eit->state.fn, 1);
                    if (file.is_open())
                        state_save(&file);
                }
                else if (eit->type == eit->LOAD) {
                    bfs::File file(eit->state.fn, 0);
                    if (file.is_open())
                        state_load(&file);
                }
            }
        }
    }
}

void input_init() {
    SusProc = reinterpret_cast<decltype(SusProc)>(mem::get_base() + 0x41ba0);
    hook(mem::addr("GetCursorPos", "user32.dll"), GetCursorPosHook, &GetCursorPosOrig);
    hook(mem::addr("GetKeyState", "user32.dll"), GetKeyStateHook, &GetKeyStateOrig);
    hook(mem::addr("GetAsyncKeyState", "user32.dll"), GetAsyncKeyStateHook, &GetAsyncKeyStateOrig);
}
