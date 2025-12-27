#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <cstdint>
#include <iostream>
#include "ass.hpp"
#include "mem.hpp"
#include "ui.hpp"

void ass::show_err(const char* text) {
	MessageBoxA(nullptr, text, "Boshyst error!", MB_ICONERROR);
}

static HANDLE hproc = GetCurrentProcess();
extern HWND hwnd;

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

int get_scene_id() {
	const size_t offsets[] = { 0x59A94 + 0x400000, 0x268, 0xA8 };
	return *(int*)mem::ptr_from_offsets(offsets, sizeof(offsets) / 4);
}

void* get_player_ptr(int s) {
	// Tutorial
	if (s == 36) {
		const size_t offsets[] = { mem::get_base("Lacewing.mfx") + 0x2D680, 0x208, 0x1C, 0xF0, 0 };
		return mem::ptr_from_offsets(offsets, sizeof(offsets) / 4);
	}
	// W2
	if (s == 5) {
		const size_t offsets[] = { mem::get_base("Lacewing.mfx") + 0x2D680, 0x208, 0x1C, 0x3B0, 0 };
		return mem::ptr_from_offsets(offsets, sizeof(offsets) / 4);
	}
	// MB1
	if (s == 6) {
		const size_t offsets[] = { mem::get_base("Lacewing.mfx") + 0x2D680, 0x208, 0x1C, 0xD8, 0 };
		return mem::ptr_from_offsets(offsets, sizeof(offsets) / 4);
	}
	// B2, MARIO SECRET
	if (s == 7 || s == 33) {
		const size_t offsets[] = { mem::get_base("Lacewing.mfx") + 0x2D680, 0x208, 0x1C, 0xC8, 0 };
		return mem::ptr_from_offsets(offsets, sizeof(offsets) / 4);
	}
	// W3, B8
	if (s == 8 || s == 26) {
		const size_t offsets[] = { mem::get_base("Lacewing.mfx") + 0x2D680, 0x208, 0x1C, 0x48, 0 };
		return mem::ptr_from_offsets(offsets, sizeof(offsets) / 4);
	}
	// W4-8, W11, B1, B3, B4, B5, B6, B7, B10, TELEPROOM
	if (s == 11 || s == 13 || s == 15 || s == 22 || s == 25 || s == 35 || s == 4 || s == 10 ||
		s == 12 || s == 14 || s == 17 || s == 24 || s == 32 || s == 29) {
		const size_t offsets[] = { mem::get_base("Lacewing.mfx") + 0x2D680, 0x208, 0x1C, 0x38, 0 };
		return mem::ptr_from_offsets(offsets, sizeof(offsets) / 4);
	}
	// W9
	if (s == 27) {
		const size_t offsets[] = { mem::get_base("Lacewing.mfx") + 0x2D680, 0x208, 0x1C, 0x88, 0 };
		return mem::ptr_from_offsets(offsets, sizeof(offsets) / 4);
	}
	// B9
	if (s == 59) {
		const size_t offsets[] = { mem::get_base("Lacewing.mfx") + 0x2D680, 0x208, 0x1C, 0x28, 0 };
		return mem::ptr_from_offsets(offsets, sizeof(offsets) / 4);
	}
	// W10
	if (s == 31) {
		const size_t offsets[] = { mem::get_base("Lacewing.mfx") + 0x2D680, 0x234, 0x208, 0x1C, 0x68, 0 };
		return mem::ptr_from_offsets(offsets, sizeof(offsets) / 4);
	}
	// W11-cobrat
	if (s == 35) {
		const size_t offsets[] = { mem::get_base("Lacewing.mfx") + 0x2D680, 0x208, 0x1C, 0x480, 0xC, 0 };
		return mem::ptr_from_offsets(offsets, sizeof(offsets) / 4);
	}
	// KAPPA
	if (s == 37) {
		const size_t offsets[] = { mem::get_base("Lacewing.mfx") + 0x2D680, 0x208, 0x1C, 0x110, 0 };
		return mem::ptr_from_offsets(offsets, sizeof(offsets) / 4);
	}
	// FINAL PATH
	if (s == 38) {
		const size_t offsets[] = { mem::get_base("Lacewing.mfx") + 0x2D680, 0x208, 0x1C, 0xB8, 0 };
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
	// BLIZZARD
	if (s == 53) {
		const size_t offsets[] = { mem::get_base("Lacewing.mfx") + 0x2D680, 0x208, 0x1C, 0xE8, 0 };
		return mem::ptr_from_offsets(offsets, sizeof(offsets) / 4);
	}
	// W1
	if (s == 3) {
		const size_t offsets[] = { mem::get_base("Lacewing.mfx") + 0x2D680, 0x208, 0x1C, 0xE0, 0 };
		return mem::ptr_from_offsets(offsets, sizeof(offsets) / 4);
	}
	// GASTLY
	if (s == 9) {
		const size_t offsets[] = { mem::get_base("Lacewing.mfx") + 0x2D680, 0x208, 0x1C, 0x130, 0 };
		return mem::ptr_from_offsets(offsets, sizeof(offsets) / 4);
	}
	// POKEWORLD
	if (s == 50) {
		const size_t offsets[] = { mem::get_base("Lacewing.mfx") + 0x2D680, 0x208, 0x1C, 0x78, 0 };
		return mem::ptr_from_offsets(offsets, sizeof(offsets) / 4);
	}
	// METAL GEAR
	if (s == 23) {
		const size_t offsets[] = { mem::get_base("Lacewing.mfx") + 0x2D680, 0x208, 0x1C, 0x418, 0 };
		return mem::ptr_from_offsets(offsets, sizeof(offsets) / 4);
	}
	// ELEVATOR
	if (s == 51) {
		const size_t offsets[] = { mem::get_base("Lacewing.mfx") + 0x2D680, 0x208, 0x1C, 0x138, 0 };
		return mem::ptr_from_offsets(offsets, sizeof(offsets) / 4);
	}
	// CHEETAHMEN
	if (s == 54) {
		const size_t offsets[] = { mem::get_base("Lacewing.mfx") + 0x2D680, 0x208, 0x1C, 0x70, 0 };
		return mem::ptr_from_offsets(offsets, sizeof(offsets) / 4);
	}
	return nullptr;
}
