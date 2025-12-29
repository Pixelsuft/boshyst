#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <iostream>
#include <cstdio>
#include <d3d9.h>
#include "conf.hpp"
#include "ass.hpp"
#include "init.hpp"
#include "fs.hpp"
#include "utils.hpp"
#include "btas.hpp"
#include "mem.hpp"
#include "ghidra_headers.h"

using std::cout;

extern SHORT(__stdcall* GetAsyncKeyStateOrig)(int k);
extern DWORD(__stdcall* timeGetTimeOrig)();

unsigned long tas_time = 0;
static DWORD real_last_time = 0;
bool is_btas = false;
bool fast_forward = false;
bool is_paused = true;

void btas::init() {
	cout << "init\n";
	real_last_time = timeGetTimeOrig();
	tas_time = 0;
}

short btas::TasGetKeyState(int k) {
	if (k == VK_LEFT)
		return 0;
	return GetAsyncKeyStateOrig(k);
}

bool btas::on_before_update() {
	RunHeader* pState = *(RunHeader**)(mem::get_base() + 0x59a9c);
	if (JustKeyState(VK_PAUSE) == 1)
		is_paused = !is_paused;
	fast_forward = MyKeyState(VK_TAB);
	int temp = JustKeyState(VK_SPACE);
	if (temp == 1)
		is_paused = false;
	else if (temp == -1)
		is_paused = true;
	temp = JustKeyState('V');
	if (temp == 1)
		is_paused = false;
	else if (MyKeyState('V'))
		is_paused = true;

	if (is_paused) {
		pState->isPaused = true;
		real_last_time = timeGetTimeOrig();
		return true;
	}
	pState->isPaused = false;
	DWORD now = timeGetTimeOrig();
	pState->subTickStep = 1;
	// tas_time = timeGetTimeOrig();
	tas_time += 20; // 50 FPS
	if (!fast_forward && now < (real_last_time + 20))
		Sleep((DWORD)((long long)20 - (long long)now + (long long)real_last_time));
	if (MyKeyState(VK_SPACE) || MyKeyState('V'))
		Sleep(20);
	real_last_time = timeGetTimeOrig();
	return false;
}
