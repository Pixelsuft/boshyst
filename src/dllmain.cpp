#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <iostream>
#include <cstdio>
#include <MinHook.h>
#include <kiero.h>
#include <backends/imgui_impl_win32.h>
#include <backends/imgui_impl_dx9.h>
#include <d3d9.h>
#include "conf.hpp"
#include "ass.hpp"
#include "init.hpp"
#include "ui.hpp"
#include "rec.hpp"
#include "utils.hpp"
#include "btas.hpp"
#define SHOW_STAGES 0

using std::cout;

HWND hwnd = nullptr;
HWND mhwnd = nullptr;
bool inited = false;
bool gr_hooked = false;
bool is_hourglass = false;

static DWORD WINAPI app_entry(LPVOID lpParameter) {
    ASS(MH_Initialize() == MH_OK);
    init_game_loop();
    return 0;
}

typedef long(__stdcall* Reset)(LPDIRECT3DDEVICE9, D3DPRESENT_PARAMETERS*);
static Reset oReset = nullptr;

typedef long(__stdcall* EndScene)(LPDIRECT3DDEVICE9);
static EndScene oEndScene = nullptr;

static long __stdcall hkReset(LPDIRECT3DDEVICE9 pDevice, D3DPRESENT_PARAMETERS* pPresentationParameters)
{
    // cout << "dev reset\n";
    ImGui_ImplDX9_InvalidateDeviceObjects();
    long result = oReset(pDevice, pPresentationParameters);
    ImGui_ImplDX9_CreateDeviceObjects();

    return result;
}

static long __stdcall hkEndScene(LPDIRECT3DDEVICE9 pDevice)
{
    if (!inited)
    {
        D3DDEVICE_CREATION_PARAMETERS params;
        pDevice->GetCreationParameters(&params);
        hwnd = params.hFocusWindow;
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        if (conf::tas_mode)
            io.ConfigFlags |= ImGuiConfigFlags_NoMouse;
        io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
        io.IniFilename = nullptr;
        ImGui_ImplWin32_Init(hwnd);
        ImGui_ImplDX9_Init(pDevice);
        inited = true;
#if SHOW_STAGES
        cout << "graphics inited\n";
#endif
    }
    rec::rec_tick(conf::direct_render ? pDevice : nullptr);

    ImGui_ImplDX9_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
    ui::draw();
    ImGui::EndFrame();
    ImGui::Render();
    ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
    auto ret = oEndScene(pDevice);
    return ret;
}

extern void init_simple_hacks();

void try_to_hook_graphics() {
    if (gr_hooked)
        return;
    if (is_hourglass) {
        int cur_scene = get_scene_id();
        if (cur_scene < 2 || cur_scene > 60)
            return;
    }
    gr_hooked = true;
#if SHOW_STAGES
    cout << "graphics hooking 3\n";
#endif
    ASS(kiero::init(kiero::RenderType::Auto) == kiero::Status::Success);
#if SHOW_STAGES
    cout << "graphics hooking 4\n";
#endif
    ASS(kiero::bind(16, (void**)&oReset, hkReset) == kiero::Status::Success);
#if SHOW_STAGES
    cout << "graphics hooking 5\n";
#endif
    ASS(kiero::bind(42, (void**)&oEndScene, hkEndScene) == kiero::Status::Success);
#if SHOW_STAGES
    cout << "graphics hooking 6\n";
#endif
    ASS(MH_EnableHook(MH_ALL_HOOKS) == MH_OK);
#if SHOW_STAGES
    cout << "hooks enabled\n";
#endif
}

void try_to_init() {
#if SHOW_STAGES
    cout << "before hooking 1\n";
#endif
    hwnd = FindWindowA(nullptr, "I Wanna Be The Boshy");
    mhwnd = FindWindowExA(hwnd, nullptr, "Mf2EditClassTh", nullptr);
    ASS(mhwnd != nullptr);
#if SHOW_STAGES
    cout << "game hooks start 2\n";
#endif
    init_simple_hacks();
    ASS(MH_EnableHook(MH_ALL_HOOKS) == MH_OK);
}

extern "C" __declspec(dllexport) void dummy_func() {}

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
    DisableThreadLibraryCalls(hModule);
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        is_hourglass = GetModuleHandleA("wintasee.dll") != nullptr;
        is_btas = !is_hourglass && GetModuleHandleA("Viewport.mfx") == nullptr; // Hacky
        conf::read();
#if defined(_DEBUG)
        if (true) {
#else
        if (conf::tas_mode) {
#endif
            AllocConsole();
            freopen_s((FILE**)stdout, "CONOUT$", "w", stdout);
        }
        app_entry(nullptr);
        break;
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

