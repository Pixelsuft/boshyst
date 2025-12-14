#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

int main(int argc, char* argv[]) {
	HWND hwnd = FindWindowW(NULL, L"I Wanna Be The Boshy");
	if (!hwnd)
		hwnd = FindWindowW(NULL, L"I Wanna Be The Boshy S");
	SetWindowTextW(hwnd, L"I Wanna Be The Boshy R");
	return 0;
}
