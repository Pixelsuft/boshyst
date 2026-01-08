#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include "ass.hpp"
#ifdef _MSC_VER
#pragma comment(lib, "ntdll.lib")
#endif

#ifndef NTAPI
#define NTAPI __stdcall
#endif
#ifndef NTSYSAPI
#define NTSYSAPI declspec(dllimport)
#endif

#define LOAD_FUNC_ORD(func_name, func_ord) do { \
	win_shit.func_name = reinterpret_cast<func_name##_t>(GetProcAddress(win_shit.uxtheme_handle, MAKEINTRESOURCEA(func_ord))); \
	if (win_shit.func_name == NULL) { \
		ass::show_err("WTF failed to load uxtheme.dll func " #func_name); \
		FreeLibrary(win_shit.uxtheme_handle); \
		return true; \
	} \
} while (0)

typedef LONG WIN_NTDLL_NTSTATUS;

typedef struct {
	ULONG dwOSVersionInfoSize;
	ULONG dwMajorVersion;
	ULONG dwMinorVersion;
	ULONG dwBuildNumber;
	ULONG dwPlatformId;
	WCHAR szCSDVersion[128];
	USHORT wServicePackMajor;
	USHORT wServicePackMinor;
	USHORT wSuiteMask;
	UCHAR wProductType;
	UCHAR wReserved;
} WIN_NTDLL_OSVERSIONINFOEXW;

typedef enum {
	WIN_APPMODE_DEFAULT,
	WIN_APPMODE_ALLOW_DARK,
	WIN_APPMODE_FORCE_DARK,
	WIN_APPMODE_FORCE_LIGHT,
	WIN_APPMODE_MAX
} WinPreferredAppMode;

enum WINDOWCOMPOSITIONATTRIB {
	WCA_UNDEFINED = 0,
	WCA_NCRENDERING_ENABLED = 1,
	WCA_NCRENDERING_POLICY = 2,
	WCA_TRANSITIONS_FORCEDISABLED = 3,
	WCA_ALLOW_NCPAINT = 4,
	WCA_CAPTION_BUTTON_BOUNDS = 5,
	WCA_NONCLIENT_RTL_LAYOUT = 6,
	WCA_FORCE_ICONIC_REPRESENTATION = 7,
	WCA_EXTENDED_FRAME_BOUNDS = 8,
	WCA_HAS_ICONIC_BITMAP = 9,
	WCA_THEME_ATTRIBUTES = 10,
	WCA_NCRENDERING_EXILED = 11,
	WCA_NCADORNMENTINFO = 12,
	WCA_EXCLUDED_FROM_LIVEPREVIEW = 13,
	WCA_VIDEO_OVERLAY_ACTIVE = 14,
	WCA_FORCE_ACTIVEWINDOW_APPEARANCE = 15,
	WCA_DISALLOW_PEEK = 16,
	WCA_CLOAK = 17,
	WCA_CLOAKED = 18,
	WCA_ACCENT_POLICY = 19,
	WCA_FREEZE_REPRESENTATION = 20,
	WCA_EVER_UNCLOAKED = 21,
	WCA_VISUAL_OWNER = 22,
	WCA_HOLOGRAPHIC = 23,
	WCA_EXCLUDED_FROM_DDA = 24,
	WCA_PASSIVEUPDATEMODE = 25,
	WCA_USEDARKMODECOLORS = 26,
	WCA_LAST = 27
};

typedef struct {
	WINDOWCOMPOSITIONATTRIB Attrib;
	PVOID pvData;
	SIZE_T cbData;
} WINDOWCOMPOSITIONATTRIBDATA;

typedef bool (WINAPI* ShouldAppsUseDarkMode_t)(void);
typedef void (WINAPI* AllowDarkModeForWindow_t)(HWND, bool);
typedef void (WINAPI* AllowDarkModeForApp_t)(bool);
typedef void (WINAPI* FlushMenuThemes_t)(void);
typedef void (WINAPI* RefreshImmersiveColorPolicyState_t)(void);
typedef bool (WINAPI* IsDarkModeAllowedForWindow_t)(HWND);
typedef bool (WINAPI* ShouldSystemUseDarkMode_t)(void);
typedef WinPreferredAppMode(WINAPI* SetPreferredAppMode_t)(WinPreferredAppMode);
typedef bool (WINAPI* IsDarkModeAllowedForApp_t)(void);

typedef BOOL(WINAPI* SetWindowCompositionAttribute_t)(HWND, const WINDOWCOMPOSITIONATTRIBDATA*);

typedef struct {
	HMODULE uxtheme_handle;
	HMODULE user32_handle;
	ShouldAppsUseDarkMode_t ShouldAppsUseDarkMode;
	AllowDarkModeForWindow_t AllowDarkModeForWindow;
	AllowDarkModeForApp_t AllowDarkModeForApp;
	FlushMenuThemes_t FlushMenuThemes;
	RefreshImmersiveColorPolicyState_t RefreshImmersiveColorPolicyState;
	IsDarkModeAllowedForWindow_t IsDarkModeAllowedForWindow;
	ShouldSystemUseDarkMode_t ShouldSystemUseDarkMode;
	SetPreferredAppMode_t SetPreferredAppMode;
	IsDarkModeAllowedForApp_t IsDarkModeAllowedForApp;
	SetWindowCompositionAttribute_t SetWindowCompositionAttribute;
	DWORD build_num;
} win_shit_type;

extern HWND hwnd;
extern HWND mhwnd;
static win_shit_type win_shit;

// Fuck you, Microshit!!!
extern "C" NTSYSAPI WIN_NTDLL_NTSTATUS NTAPI RtlGetVersion(WIN_NTDLL_OSVERSIONINFOEXW* ver_info);

bool fix_win32_theme() {
	win_shit.uxtheme_handle = NULL;
	win_shit.user32_handle = NULL;
	WIN_NTDLL_OSVERSIONINFOEXW os_ver;
	os_ver.dwOSVersionInfoSize = sizeof(WIN_NTDLL_OSVERSIONINFOEXW);
	RtlGetVersion(&os_ver);
	win_shit.build_num = os_ver.dwBuildNumber;
	win_shit.build_num &= ~0xF0000000;
	if (win_shit.build_num < 17763)
		return true; // Lol ur windoes is too old;
	win_shit.uxtheme_handle = LoadLibraryExW(L"uxtheme.dll", NULL, LOAD_IGNORE_CODE_AUTHZ_LEVEL | LOAD_LIBRARY_SEARCH_SYSTEM32);
	if (win_shit.uxtheme_handle == NULL) {
		ass::show_err("WTF failed to load uxtheme.dll");
		return false;
	};
	win_shit.user32_handle = LoadLibraryExW(L"user32.dll", NULL, LOAD_IGNORE_CODE_AUTHZ_LEVEL | LOAD_LIBRARY_SEARCH_SYSTEM32);
	if (win_shit.user32_handle == NULL) {
		FreeLibrary(win_shit.uxtheme_handle);
		ass::show_err("WTF failed to load user32.dll");
		return false;
	};
	win_shit.SetWindowCompositionAttribute = (SetWindowCompositionAttribute_t)GetProcAddress(win_shit.user32_handle, "SetWindowCompositionAttribute");
	LOAD_FUNC_ORD(ShouldAppsUseDarkMode, 132);
	LOAD_FUNC_ORD(AllowDarkModeForWindow, 133);
	if (win_shit.build_num < 18362) {
		win_shit.SetPreferredAppMode = NULL;
		LOAD_FUNC_ORD(AllowDarkModeForApp, 135);
	}
	else {
		win_shit.AllowDarkModeForApp = NULL;
		LOAD_FUNC_ORD(SetPreferredAppMode, 135);
	}
	LOAD_FUNC_ORD(FlushMenuThemes, 136);
	LOAD_FUNC_ORD(RefreshImmersiveColorPolicyState, 104);
	LOAD_FUNC_ORD(IsDarkModeAllowedForWindow, 137);
	if (win_shit.build_num >= 18290)
		LOAD_FUNC_ORD(ShouldSystemUseDarkMode, 138);
	else
		win_shit.ShouldSystemUseDarkMode = NULL;
	if (win_shit.build_num >= 18334)
		LOAD_FUNC_ORD(IsDarkModeAllowedForApp, 139);
	else
		win_shit.IsDarkModeAllowedForApp = NULL;
	// Let's begin our magic
	if (win_shit.AllowDarkModeForApp != NULL)
		win_shit.AllowDarkModeForApp(true);
	if (win_shit.SetPreferredAppMode != NULL)
		win_shit.SetPreferredAppMode(WIN_APPMODE_ALLOW_DARK);
	win_shit.RefreshImmersiveColorPolicyState();
	win_shit.AllowDarkModeForWindow(hwnd, true);
	bool enable_dark = win_shit.ShouldAppsUseDarkMode();
	BOOL win_dark = enable_dark ? TRUE : FALSE;
	if (win_shit.build_num < 18362) {
		SetPropW(hwnd, L"UseImmersiveDarkModeColors", reinterpret_cast<HANDLE>(static_cast<INT_PTR>(win_dark)));
	}
	else if (win_shit.SetWindowCompositionAttribute != NULL) {
		WINDOWCOMPOSITIONATTRIBDATA data = { WCA_USEDARKMODECOLORS, &win_dark, sizeof(win_dark) };
		win_shit.SetWindowCompositionAttribute(hwnd, &data);
	}
	FreeLibrary(win_shit.user32_handle);
	FreeLibrary(win_shit.uxtheme_handle);
	return true;
}
#endif
