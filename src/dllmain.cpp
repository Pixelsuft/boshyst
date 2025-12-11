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

using std::cout;

extern void inp_pre_init();

HWND hwnd;
bool inited;
bool gr_hooked;

static DWORD WINAPI app_entry(LPVOID lpParameter) {
#if 0
    AllocConsole();
    freopen_s((FILE**)stdout, "CONOUT$", "w", stdout);
    conf::read();
#endif
    ASS(MH_Initialize() == MH_OK);
    // inp_pre_init();
#ifndef _DEBUG
    // Sleep(500);
#endif
    try_hook_gr();
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
        io.ConfigFlags |= ImGuiConfigFlags_NoMouse;
        io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
        ImGui_ImplWin32_Init(hwnd);
        ImGui_ImplDX9_Init(pDevice);
        inited = true;
        cout << "graphics inited\n";
    }

    ImGui_ImplDX9_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
    ui::draw();
    ImGui::EndFrame();
    ImGui::Render();
    ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());

    return oEndScene(pDevice);
}

extern void init_simple_hacks();
void try_hook_gr() {
    if (gr_hooked)
        return;
    gr_hooked = true;
    cout << "graphics hooking 1\n";
    hwnd = nullptr;
#ifdef _DEBUG
    hwnd = FindWindowA(nullptr, "I Wanna Be The Boshy");
#else
    do {
        hwnd = FindWindowA(nullptr, "I Wanna Be The Boshy");
        Sleep(1000);
    } while (hwnd == nullptr);
#endif
    cout << "graphics hooking 2\n";
    init_simple_hacks();
    ASS(MH_EnableHook(MH_ALL_HOOKS) == MH_OK);
    cout << "graphics hooking 3\n";
    ASS(kiero::init(kiero::RenderType::Auto) == kiero::Status::Success);
    cout << "graphics hooking 4\n";
    ASS(kiero::bind(16, (void**)&oReset, hkReset) == kiero::Status::Success);
    cout << "graphics hooking 5\n";
    ASS(kiero::bind(42, (void**)&oEndScene, hkEndScene) == kiero::Status::Success);
    cout << "graphics hooking 6\n";
    ASS(MH_EnableHook(MH_ALL_HOOKS) == MH_OK);
    cout << "graphics hooked\n";
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
        AllocConsole();
        freopen_s((FILE**)stdout, "CONOUT$", "w", stdout);
        conf::read();
        CreateThread(nullptr, 0, (LPTHREAD_START_ROUTINE)app_entry, nullptr, 0, nullptr);
        // app_entry(nullptr);
        break;
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

