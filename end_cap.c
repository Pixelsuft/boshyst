#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

int main(int argc, char* argv[]) {
	HWND hwnd = FindWindowW(NULL, L"I Wanna Be The Boshy R");
	if (!hwnd)
		hwnd = FindWindowW(NULL, L"I Wanna Be The Boshy");
	SetWindowTextW(hwnd, L"I Wanna Be The Boshy S");
	return 0;
}
