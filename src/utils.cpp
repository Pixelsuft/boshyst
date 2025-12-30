#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <cstdint>
#include <iostream>
#include "ass.hpp"
#include "mem.hpp"
#include "fs.hpp"
#include "ui.hpp"
#include "utils.hpp"
#include <map>

using std::cout;

void ass::show_err(const char* text) {
	wchar_t* buf = utf8_to_unicode(text);
	MessageBoxW(nullptr, buf, L"Boshyst error!", MB_ICONERROR);
	std::free(buf);
}

static HANDLE hproc = GetCurrentProcess();
extern HWND hwnd;
extern HWND mhwnd;
extern BOOL(__stdcall* GetCursorPosOrig)(LPPOINT p);
extern SHORT(__stdcall* GetKeyStateOrig)(int k);
static std::map<int, bool> key_states;

bool MyKeyState(int k) {
	HWND fg = GetForegroundWindow();
	return (fg == hwnd || fg == mhwnd) && (GetKeyStateOrig(k) & 128);
}

int JustKeyState(int k) {
	auto it = key_states.find(k);
	auto st = MyKeyState(k);
	if (it == key_states.end()) {
		key_states[k] = st;
		return st ? 1 : 0;
	}
	if (st) {
		if (!it->second) {
			it->second = true;
			return 1;
		}
	}
	else if (it->second) {
		it->second = false;
		return -1;
	}
	return 0;
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
	if (!obj_name) {
		static auto def_ret = (size_t)GetModuleHandleW(nullptr);
		return def_ret;
	}
	size_t ret = (size_t)GetModuleHandleA(obj_name);
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

static void* get_player_by_id(int idx) {
	const size_t offsets[] = { mem::get_base("Lacewing.mfx") + 0x2D680, 0x208, 0x1C, (size_t)idx, 0 };
	return mem::ptr_from_offsets(offsets, sizeof(offsets) / 4);
}

void* get_player_ptr(int s) {
	// FIXME: W4, W5, B8
	switch (s) {
	// Tutorial
	case 36:
		return get_player_by_id(0x80);
	// W1, W3, METAL GEAR
	case 3:
	case 8:
	case 23:
		return get_player_by_id(0xE0);
	// MARIO SECRET
	case 33:
		return get_player_by_id(0x68);
	// B1, B5, B7, W8, POKEWORLD, CHEETAHMEN
	case 4:
	case 14:
	case 24:
	case 25:
	case 50:
	case 54:
		return get_player_by_id(0x70);
	// W2
	case 5:
		return get_player_by_id(0xA58);
	// MB1
	case 6:
		return get_player_by_id(0x148);
	// B2, GASTLY
	case 7:
	case 9:
		return get_player_by_id(0x130);
	// B3, B4, W11, B9
	case 10:
	case 12:
	case 35:
	case 59:
		return get_player_by_id(0x90);
	// W4
	case 11:
		return get_player_by_id(0x100);
	// W5
	case 13:
		return get_player_by_id(0x38);
	// W6
	case 15:
		return get_player_by_id(0x110);
	// B6
	case 17:
		return get_player_by_id(0xA0);
	// W7, Gardius
	case 22:
	case 39:
		return get_player_by_id(0x150);
	// B10, TELEPROOM
	case 32:
	case 29:
		return get_player_by_id(0x88);
	// B8
	case 26:
		return get_player_by_id(0x48);
	// W9, W10
	case 27:
	case 31:
		return get_player_by_id(0xB8);
	// KAPPA
	case 37:
		return get_player_by_id(0xB0);
	// FB
	case 46:
		return get_player_by_id(0xA8);
	// PRIZE ROOM
	case 49:
		return get_player_by_id(0x140);
	// BLIZZARD
	case 53:
		return get_player_by_id(0x78);
	// ELEVATOR
	case 51:
		return get_player_by_id(0x138);
	// FINAL PATH
	case 38:
		return get_player_by_id(0x58);
	default:
		return nullptr;
	}
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
	// cout << "save ret: " << ret << "\n";
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
	// cout << "load ret: " << ret << "\n";
	return true;
}
