#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <cstdint>
#include "ass.hpp"
#include "mem.hpp"
#include "ui.hpp"

void ass::show_err(const char* text) {
	MessageBoxA(nullptr, text, "Boshyst error!", MB_ICONERROR);
}

static HANDLE hproc = GetCurrentProcess();
extern HWND hwnd;

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
