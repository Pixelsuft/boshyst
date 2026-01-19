#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <Psapi.h>
#include <cstdint>
#include <iostream>
#include "ass.hpp"
#include "mem.hpp"
#include "fs.hpp"
#include "ui.hpp"
#include "utils.hpp"
#include "ghidra_headers.h"
#include <vector>

using std::cout;

void ass::show_err(const char* text) {
	wchar_t* buf = utf8_to_unicode(text);
	MessageBoxW(nullptr, buf, L"Boshyst error!", MB_ICONERROR);
	std::free(buf);
}

HANDLE hproc = GetCurrentProcess();
extern HWND hwnd;
extern HWND mhwnd;
extern BOOL(__stdcall* GetCursorPosOrig)(LPPOINT p);
extern SHORT(__stdcall* GetKeyStateOrig)(int k);
static std::vector<int> key_states;

bool MyKeyState(int k) {
	HWND fg = GetForegroundWindow();
	return (fg == hwnd || fg == mhwnd) && (GetKeyStateOrig(k) & 128);
}

int JustKeyState(int k) {
	auto it = std::find(key_states.begin(), key_states.end(), k);
	auto st = MyKeyState(k);
	if (it == key_states.end()) {
		if (st) {
			key_states.push_back(k);
			return 1;
		}
		return 0;
	}
	if (!st) {
		key_states.erase(it);
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

static HMODULE GetSxSModuleHandle(const char* targetPart) {
	HMODULE hMods[1024];
	HANDLE hProcess = GetCurrentProcess();
	DWORD cbNeeded;

	if (EnumProcessModules(hProcess, hMods, sizeof(hMods), &cbNeeded)) {
		for (unsigned int i = 0; i < (cbNeeded / sizeof(HMODULE)); i++) {
			char szModName[MAX_PATH];
			if (GetModuleFileNameExA(hProcess, hMods[i], szModName, MAX_PATH)) {
				if (c_ends_with(szModName, targetPart)) {
					return hMods[i];
				}
			}
		}
	}
	return NULL;
}

size_t mem::get_base() {
	static auto def_ret = (size_t)GetModuleHandleW(nullptr);
	return def_ret;
}

size_t mem::get_base(const char* obj_name) {
	size_t ret = (size_t)GetModuleHandleA(obj_name);
	ASS(ret != 0);
	return (size_t)ret;
}

void* mem::addr(const char* func_name, const char* obj_name) {
	// cout << "trying to load " << obj_name << std::endl;
	auto obj = (strcmp(obj_name, "MSVCR90.dll") == 0) ? GetSxSModuleHandle(obj_name) : GetModuleHandleA(obj_name);
	if (!obj && 0) {
		cout << "trying to fix " << obj_name << " for " << func_name << std::endl;
		LoadLibraryA(obj_name);
		obj = GetModuleHandleA(obj_name);
	}
	ASS(obj != nullptr);
	if (!obj)
		return nullptr;
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

const char* get_scene_name() {
	// TODO: return mem::get_base()?
	GlobalStats& gStats = **(GlobalStats**)(0x459a98);
	return (gStats.sceneName && *gStats.sceneName) ? gStats.sceneName : "Unknown";
}

int get_scene_id() {
	// TODO: return mem::get_base()?
	RunApp& gState = **(RunApp**)0x0459a94;
	return gState.rhCurrentFrame;
}

static void* get_player_by_id(int idx) {
	const size_t offsets[] = { mem::get_base("Lacewing.mfx") + 0x2D680, 0x208, 0x1C, (size_t)idx, 0 };
	return mem::ptr_from_offsets(offsets, sizeof(offsets) / 4);
}

static int get_player_handle(int s) {
	switch (s) {
	case 2: return 28;
	case 3: return 14;
	case 4: return 331;
	case 5: return 41;
	case 6: return 38;
	case 7: return 28;
	case 8: return 25;
	case 9: return 18;
	case 10: return 614;
	case 11: return 18;
	case 12: return 519;
	case 13: return 14;
	case 14: return 34;
	case 16: return 20;
	case 21: return 42;
	case 22: return 28;
	case 23: return 14;
	case 24: return 14;
	case 25: return 76;
	case 26: return 23;
	case 27: return 14;
	case 28: return 17;
	case 29: return 13;
	case 30: return 23;
	case 31: return 17;
	case 32: return 13;
	case 33: return 13;
	case 34: return 18;
	case 35: return 16;
	case 36: return 22;
	case 37: return 11;
	case 38: return 13;
	case 39: return 13;
	case 40: return 50;
	case 43: return 50;
	case 44: return 50;
	case 45: return 21;
	case 47: return 680;
	case 48: return 40;
	case 49: return 14;
	case 50: return 38;
	case 51: return 13;
	case 52: return 15;
	case 53: return 14;
	case 58: return 18;
	case 59: return 15;
	case 60: return 13;
	default: return -1;
	}
}

void* get_player_ptr(int s) {
	int handle = get_player_handle(s);
	RunHeader& pState = **(RunHeader**)(mem::get_base() + 0x59a9c);
	if (handle != -1 && handle < pState.activeObjectCount)
		return pState.objectList[handle * 2];
	return nullptr;
	/*
	// FIXME: return W4, W5, B8
	switch (s) {
	// Tutorial
	case 35:
		return get_player_by_id(0x80);
	// W1, W3, METAL GEAR
	case 2:
	case 7:
	case 22:
		return get_player_by_id(0xE0);
	// MARIO SECRET
	case 32:
		return get_player_by_id(0x68);
	// B1, B5, B7, W8, POKEWORLD, CHEETAHMEN
	case 3:
	case 13:
	case 23:
	case 24:
	case 49:
	case 53:
		return get_player_by_id(0x70);
	// W2
	case 4:
		return get_player_by_id(0xA58);
	// MB1
	case 5:
		return get_player_by_id(0x148);
	// B2, GASTLY
	case 6:
	case 8:
		return get_player_by_id(0x130);
	// B3, B4, W11, B9
	case 9:
	case 11:
	case 34:
	case 58:
		return get_player_by_id(0x90);
	// W4
	case 10:
		return get_player_by_id(0x100);
	// W5
	case 12:
		return get_player_by_id(0x38);
	// W6
	case 14:
		return get_player_by_id(0x110);
	// B6
	case 16:
		return get_player_by_id(0xA0);
	// W7, Gardius
	case 21:
	case 38:
		return get_player_by_id(0x150);
	// B10, TELEPROOM
	case 31:
	case 28:
		return get_player_by_id(0x88);
	// B8
	case 25:
		return get_player_by_id(0x48);
	// W9, W10
	case 26:
	case 30:
		return get_player_by_id(0xB8);
	// KAPPA
	case 36:
		return get_player_by_id(0xB0);
	// FB
	case 45:
		return get_player_by_id(0xA8);
	// PRIZE ROOM
	case 48:
		return get_player_by_id(0x140);
	// BLIZZARD
	case 52:
		return get_player_by_id(0x78);
	// ELEVATOR
	case 50:
		return get_player_by_id(0x138);
	// FINAL PATH
	case 37:
		return get_player_by_id(0x58);
	}
	if (s == 0 || s == 1 || s == 20) {
		player_handle = -1;
		return nullptr;
	}
	RunHeader& pState = **(RunHeader**)(mem::get_base() + 0x59a9c);
	if (player_handle != -1 && player_handle < pState.activeObjectCount)
		return pState.objectList[player_handle * 2];
	player_handle = -1;
	return nullptr;*/
}

bool state_save(bfs::File* file) {
	bool(__cdecl * SaveFunc)(HANDLE);
	SaveFunc = reinterpret_cast<decltype(SaveFunc)>(mem::get_base() + 0x37dc0);
	if (file == nullptr) {
		// TODO: actually make save/load dialog
		SaveFunc(INVALID_HANDLE_VALUE);
		return true;
	}
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
	auto ret = LoadFunc(file->get_handle(), &outver);
	// cout << "load ret: " << ret << "\n";
	return true;
}
