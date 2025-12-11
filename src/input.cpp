#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include "init.hpp"
#include "hook.hpp"
#include "mem.hpp"

static SHORT (__stdcall *GetKeyStateOrig)(int vk);
static SHORT __stdcall GetKeyStateHook(int vk) {
	if (!gr_hooked) {
		try_hook_gr();
	}
	return GetKeyStateOrig(vk);
}

void inp_pre_init() {
	hook(mem::addr("GetKeyState", "user32.dll"), GetKeyStateHook, &GetKeyStateOrig);
	MH_EnableHook(GetKeyStateHook);
}
