#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <iostream>
#include "input.hpp"
#include "init.hpp"
#include "hook.hpp"
#include "conf.hpp"
#include "mem.hpp"

using std::cout;

namespace conf {
    extern std::map<int, std::vector<InputEvent>> mb;
}
extern HWND hwnd;
extern HWND mhwnd;
extern void get_win_size(int& w_buf, int& h_buf);
static LRESULT(__stdcall* SusProc)(HWND param_1, UINT param_2, WPARAM param_3, LPARAM param_4) = nullptr;

static int cur_x = -100;
static int cur_y = -100;
static BOOL __stdcall GetCursorPosHook(LPPOINT p) {
    p->x = cur_x;
    p->y = cur_y;
    return ClientToScreen(hwnd, p);
}

static SHORT(__stdcall* GetKeyStateOrig)(int k);
static SHORT __stdcall GetKeyStateHook(int k) {
    if (k == VK_LBUTTON && !conf::cur_mouse_checked) {
        conf::cur_mouse_checked = true;
        int w, h;
        get_win_size(w, h);
        for (auto it = conf::mb.begin(); it != conf::mb.end(); it++) {
            bool pressed = (GetKeyStateOrig(it->first) & 128) != 0;
            if (pressed) {
                cout << "sus_click\n";
                for (auto eit = it->second.begin(); eit != it->second.end(); eit++) {
                    cur_x = (int)(eit->x * (float)w / 640.f);
                    cur_y = (int)(eit->y * (float)h / 480.f);
                    if (cur_x < 0 || cur_y < 0)
                        continue;
                    SusProc(mhwnd, WM_LBUTTONDOWN, 0, 0);
                    SusProc(mhwnd, WM_LBUTTONUP, 0, 0);
                }
            }
        }
    }
    return GetKeyStateOrig(k);
}

void input_init() {
    if (!conf::emu_mouse)
        return;
    SusProc = reinterpret_cast<decltype(SusProc)>(mem::get_base() + 0x41ba0);
    hook(mem::addr("GetCursorPos", "user32.dll"), GetCursorPosHook);
    hook(mem::addr("GetKeyState", "user32.dll"), GetKeyStateHook, &GetKeyStateOrig);
}
