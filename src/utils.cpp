#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <cstdint>
#include <iostream>
#include "ass.hpp"
#include "mem.hpp"
#include "fs.hpp"
#include "ui.hpp"

using std::cout;

void ass::show_err(const char* text) {
	MessageBoxA(nullptr, text, "Boshyst error!", MB_ICONERROR);
}

static HANDLE hproc = GetCurrentProcess();
extern HWND hwnd;
extern HWND mhwnd;
extern BOOL(__stdcall* GetCursorPosOrig)(LPPOINT p);
extern SHORT(__stdcall* GetKeyStateOrig)(int k);

bool MyKeyState(int k) {
	HWND fg = GetForegroundWindow();
	return (fg == hwnd || fg == mhwnd) && (GetKeyStateOrig(k) & 128);
}

wchar_t* utf8_to_unicode(const std::string& utf8) {
	int size_needed = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), static_cast<int>(utf8.length()), nullptr, 0);
	ASS(utf8.size() == 0 || size_needed > 0);
	wchar_t* ret = (wchar_t*)std::malloc((size_t)size_needed * sizeof(wchar_t) + 2);
	ASS(ret != nullptr);
	int chars_converted = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), static_cast<int>(utf8.length()), ret, size_needed);
	ASS(chars_converted == size_needed);
	ret[size_needed] = L'\0';
	return ret;
}

std::string unicode_to_utf8(wchar_t* buf, bool autofree) {
	int size_needed = WideCharToMultiByte(CP_UTF8, 0, buf, -1, nullptr, 0, nullptr, nullptr);
	ASS(size_needed > 0);
	std::string ret(size_needed - 1, 0);
	int chars_converted = WideCharToMultiByte(CP_UTF8, 0, buf, -1, &ret[0], size_needed, nullptr, nullptr);
	ASS(chars_converted == size_needed);
	if (autofree)
		std::free(buf);
	return ret;
}

size_t mem::get_base(const char* obj_name) {
	auto ret = (size_t)GetModuleHandleA(obj_name);
	ASS(ret != 0);
	return (size_t)ret;
}

void* mem::addr(const char* func_name, const char* obj_name) {
	auto obj = GetModuleHandleA(obj_name);
	ASS(obj != nullptr);
	auto ptr = GetProcAddress(obj, func_name);
	ASS(ptr != nullptr);
	return ptr;
}

static uint8_t* read_ptr(const uint8_t* addr) {
	SIZE_T bt_read;
	uint8_t* buf = nullptr;
	auto ret = ReadProcessMemory(hproc, (LPCVOID)addr, &buf, 4, &bt_read);
	return buf;
}

void* mem::ptr_from_offsets(const size_t* offsets, size_t n) {
	uint8_t* temp_addr = read_ptr((uint8_t*)offsets[0]);
	uint8_t* ptr = nullptr;
	for (size_t i = 1; i < n; i++) {
		ptr = temp_addr + offsets[i];
		temp_addr = read_ptr(ptr);
	}
	return ptr;
}

void get_win_size(int& w_buf, int& h_buf) {
	RECT rect;
	memset(&rect, 0, sizeof(rect));
	GetClientRect(hwnd, &rect);
	w_buf = rect.right;
	h_buf = rect.bottom;
}

void get_cursor_pos(int& x_buf, int& y_buf) {
	POINT point;
	memset(&point, 0, sizeof(point));
	GetCursorPos(&point);
	ScreenToClient(hwnd, &point);
	x_buf = point.x;
	y_buf = point.y;
}

void get_cursor_pos_orig(int& x_buf, int& y_buf) {
	POINT point;
	memset(&point, 0, sizeof(point));
	GetCursorPosOrig(&point);
	ScreenToClient(hwnd, &point);
	x_buf = point.x;
	y_buf = point.y;
}

int get_scene_id() {
	const size_t offsets[] = { 0x59A94 + 0x400000, 0x268, 0xA8 };
	return *(int*)mem::ptr_from_offsets(offsets, sizeof(offsets) / 4);
}

void* get_player_ptr(int s) {
	// Tutorial
	if (s == 36) {
		const size_t offsets[] = { mem::get_base("Lacewing.mfx") + 0x2D680, 0x208, 0x1C, 0x80, 0 };
		return mem::ptr_from_offsets(offsets, sizeof(offsets) / 4);
	}
	// W1
	if (s == 3) {
		const size_t offsets[] = { mem::get_base("Lacewing.mfx") + 0x2D680, 0x208, 0x1C, 0xE0, 0 };
		return mem::ptr_from_offsets(offsets, sizeof(offsets) / 4);
	}
	// MARIO SECRET
	if (s == 33) {
		const size_t offsets[] = { mem::get_base("Lacewing.mfx") + 0x2D680, 0x208, 0x1C, 0x68, 0 };
		return mem::ptr_from_offsets(offsets, sizeof(offsets) / 4);
	}
	// B1, B5, B7
	if (s == 4 || s == 14 || s == 24) {
		const size_t offsets[] = { mem::get_base("Lacewing.mfx") + 0x2D680, 0x208, 0x1C, 0x70, 0 };
		return mem::ptr_from_offsets(offsets, sizeof(offsets) / 4);
	}
	// W2
	if (s == 5) {
		const size_t offsets[] = { mem::get_base("Lacewing.mfx") + 0x2D680, 0x208, 0x1C, 0xa58, 0 };
		return mem::ptr_from_offsets(offsets, sizeof(offsets) / 4);
	}
	// MB1
	if (s == 6) {
		const size_t offsets[] = { mem::get_base("Lacewing.mfx") + 0x2D680, 0x208, 0x1C, 0x148, 0 };
		return mem::ptr_from_offsets(offsets, sizeof(offsets) / 4);
	}
	// B2
	if (s == 7) {
		const size_t offsets[] = { mem::get_base("Lacewing.mfx") + 0x2D680, 0x208, 0x1C, 0x130, 0 };
		return mem::ptr_from_offsets(offsets, sizeof(offsets) / 4);
	}
	// B2
	if (s == 7) {
		const size_t offsets[] = { mem::get_base("Lacewing.mfx") + 0x2D680, 0x208, 0x1C, 0xC8, 0 };
		return mem::ptr_from_offsets(offsets, sizeof(offsets) / 4);
	}
	// W3
	if (s == 8) {
		const size_t offsets[] = { mem::get_base("Lacewing.mfx") + 0x2D680, 0x208, 0x1C, 0xE0, 0 };
		return mem::ptr_from_offsets(offsets, sizeof(offsets) / 4);
	}
	// B3, B4, B9, W11
	if (s == 10 || s == 12 || s == 59 || s == 35) {
		const size_t offsets[] = { mem::get_base("Lacewing.mfx") + 0x2D680, 0x208, 0x1C, 0x90, 0 };
		return mem::ptr_from_offsets(offsets, sizeof(offsets) / 4);
	}
	// W4
	if (s == 11) {
		const size_t offsets[] = { mem::get_base("Lacewing.mfx") + 0x2D680, 0x208, 0x1C, 0x100, 0 };
		return mem::ptr_from_offsets(offsets, sizeof(offsets) / 4);
	}
	// W5, W8
	if (s == 13 || s == 25) {
		const size_t offsets[] = { mem::get_base("Lacewing.mfx") + 0x2D680, 0x208, 0x1C, 0x38, 0 };
		return mem::ptr_from_offsets(offsets, sizeof(offsets) / 4);
	}
	// W6
	if (s == 15) {
		const size_t offsets[] = { mem::get_base("Lacewing.mfx") + 0x2D680, 0x208, 0x1C, 0x110, 0 };
		return mem::ptr_from_offsets(offsets, sizeof(offsets) / 4);
	}
	// B6
	if (s == 17) {
		const size_t offsets[] = { mem::get_base("Lacewing.mfx") + 0x2D680, 0x208, 0x1C, 0xA0, 0 };
		return mem::ptr_from_offsets(offsets, sizeof(offsets) / 4);
	}
	// W7
	if (s == 22) {
		const size_t offsets[] = { mem::get_base("Lacewing.mfx") + 0x2D680, 0x208, 0x1C, 0x150, 0 };
		return mem::ptr_from_offsets(offsets, sizeof(offsets) / 4);
	}
	// B10, TELEPROOM
	if (s == 32 || s == 29) {
		const size_t offsets[] = { mem::get_base("Lacewing.mfx") + 0x2D680, 0x208, 0x1C, 0x88, 0 };
		return mem::ptr_from_offsets(offsets, sizeof(offsets) / 4);
	}
	// B8
	if (s == 26) {
		const size_t offsets[] = { mem::get_base("Lacewing.mfx") + 0x2D680, 0x208, 0x1C, 0x48, 0 };
		return mem::ptr_from_offsets(offsets, sizeof(offsets) / 4);
	}
	// W9, W10
	if (s == 27 || s == 31) {
		const size_t offsets[] = { mem::get_base("Lacewing.mfx") + 0x2D680, 0x208, 0x1C, 0xB8, 0 };
		return mem::ptr_from_offsets(offsets, sizeof(offsets) / 4);
	}
	// B9
	if (s == 59) {
		const size_t offsets[] = { mem::get_base("Lacewing.mfx") + 0x2D680, 0x208, 0x1C, 0x28, 0 };
		return mem::ptr_from_offsets(offsets, sizeof(offsets) / 4);
	}
	// KAPPA
	if (s == 37) {
		const size_t offsets[] = { mem::get_base("Lacewing.mfx") + 0x2D680, 0x208, 0x1C, 0xB0, 0 };
		return mem::ptr_from_offsets(offsets, sizeof(offsets) / 4);
	}
	// FB
	if (s == 46) {
		const size_t offsets[] = { mem::get_base("Lacewing.mfx") + 0x2D680, 0x208, 0x1C, 0xa8, 0 };
		return mem::ptr_from_offsets(offsets, sizeof(offsets) / 4);
	}
	// PRIZE ROOM
	if (s == 49) {
		const size_t offsets[] = { mem::get_base("Lacewing.mfx") + 0x2D680, 0x208, 0x1C, 0x140, 0 };
		return mem::ptr_from_offsets(offsets, sizeof(offsets) / 4);
	}
	// GASTLY
	if (s == 9) {
		const size_t offsets[] = { mem::get_base("Lacewing.mfx") + 0x2D680, 0x208, 0x1C, 0x130, 0 };
		return mem::ptr_from_offsets(offsets, sizeof(offsets) / 4);
	}
	// POKEWORLD
	if (s == 50) {
		const size_t offsets[] = { mem::get_base("Lacewing.mfx") + 0x2D680, 0x208, 0x1C, 0x70, 0 };
		return mem::ptr_from_offsets(offsets, sizeof(offsets) / 4);
	}
	// BLIZZARD
	if (s == 53) {
		const size_t offsets[] = { mem::get_base("Lacewing.mfx") + 0x2D680, 0x208, 0x1C, 0x78, 0 };
		return mem::ptr_from_offsets(offsets, sizeof(offsets) / 4);
	}
	// METAL GEAR
	if (s == 23) {
		const size_t offsets[] = { mem::get_base("Lacewing.mfx") + 0x2D680, 0x208, 0x1C, 0xE0, 0 };
		return mem::ptr_from_offsets(offsets, sizeof(offsets) / 4);
	}
	// ELEVATOR
	if (s == 51) {
		const size_t offsets[] = { mem::get_base("Lacewing.mfx") + 0x2D680, 0x208, 0x1C, 0x138, 0 };
		return mem::ptr_from_offsets(offsets, sizeof(offsets) / 4);
	}
	// FINAL PATH
	if (s == 38) {
		const size_t offsets[] = { mem::get_base("Lacewing.mfx") + 0x2D680, 0x208, 0x1C, 0x58, 0 };
		return mem::ptr_from_offsets(offsets, sizeof(offsets) / 4);
	}
	// CHEETAHMEN
	if (s == 54) {
		const size_t offsets[] = { mem::get_base("Lacewing.mfx") + 0x2D680, 0x208, 0x1C, 0x70, 0 };
		return mem::ptr_from_offsets(offsets, sizeof(offsets) / 4);
	}
	// Gardius
	if (s == 39) {
		const size_t offsets[] = { mem::get_base("Lacewing.mfx") + 0x2D680, 0x208, 0x1C, 0x150, 0 };
		return mem::ptr_from_offsets(offsets, sizeof(offsets) / 4);
	}
	return nullptr;
}

bool state_save(bfs::File* file) {
	bool(__cdecl * SaveFunc)(HANDLE);
	SaveFunc = reinterpret_cast<decltype(SaveFunc)>(mem::get_base() + 0x37dc0);
	if (file == nullptr) {
		// TODO: actually make save/load dialog
		SaveFunc(INVALID_HANDLE_VALUE);
		return true;
	}
	// TODO: write needed info
	auto ret = SaveFunc(file->get_handle());
	cout << "save ret: " << ret << "\n";
	return true;
}

bool state_load(bfs::File* file) {
	int outver = 0;
	int(__cdecl * LoadFunc)(HANDLE, int*);
	LoadFunc = reinterpret_cast<decltype(LoadFunc)>(mem::get_base() + 0x39780);
	if (file == nullptr) {
		LoadFunc(INVALID_HANDLE_VALUE, &outver);
		return true;
	}
	// TODO: read needed info
	auto ret = LoadFunc(file->get_handle(), &outver);
	cout << "load ret: " << ret << "\n";
	return true;
}
