#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <shlwapi.h>
#include <iostream>
#include <cstdio>
#include <kiero.h>
#include <backends/imgui_impl_win32.h>
#include <backends/imgui_impl_dx9.h>
#include <d3d9.h>
#include "conf.hpp"
#include "ass.hpp"
#include "mem.hpp"
#include "init.hpp"
#include "ui.hpp"
#include "rec.hpp"
#include "utils.hpp"
#include "btas.hpp"
#include "hook.hpp"
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

typedef long(__stdcall* SetSamplerState)(LPDIRECT3DDEVICE9, DWORD, D3DSAMPLERSTATETYPE, DWORD);
static SetSamplerState oSetSamplerState = nullptr;

typedef long(__stdcall* StretchRect)(LPDIRECT3DDEVICE9, IDirect3DSurface9*, const RECT*, IDirect3DSurface9*, const RECT*, D3DTEXTUREFILTERTYPE);
static StretchRect oStretchRect = nullptr;

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
        // Init imgui
        D3DDEVICE_CREATION_PARAMETERS params;
        pDevice->GetCreationParameters(&params);
        if (!is_btas)
            hwnd = params.hFocusWindow;
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        if (conf::tas_mode && !is_btas)
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
    // Render before we draw our GUI
    if (conf::direct_render)
        rec::rec_tick(pDevice);

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

static long __stdcall hkSetSamplerState(LPDIRECT3DDEVICE9 pDevice, DWORD Sampler, D3DSAMPLERSTATETYPE Type, DWORD Value) {
    if (conf::pixel_filter && Sampler == 0 && (Type == D3DSAMP_MAGFILTER || Type == D3DSAMP_MINFILTER))
        return oSetSamplerState(pDevice, Sampler, Type, D3DTEXF_POINT);
    return oSetSamplerState(pDevice, Sampler, Type, Value);
}

static long __stdcall hkStretchRect(LPDIRECT3DDEVICE9 pDevice, IDirect3DSurface9* pSrc, const RECT* pSrcR, IDirect3DSurface9* pDst, const RECT* pDstR, D3DTEXTUREFILTERTYPE Filter) {
    return oStretchRect(pDevice, pSrc, pSrcR, pDst, pDstR, conf::pixel_filter ? D3DTEXF_POINT : Filter);
}

void try_to_hook_graphics() {
    if (gr_hooked)
        return;
    if (is_hourglass && !conf::hg_instant) {
        // Somewhy hourglass freezes after logo when initing during logo scene
        int cur_scene = get_scene_id();
        if (cur_scene < 1 || cur_scene > 59)
            return;
    }
    gr_hooked = true;
    if (GetModuleHandleA("mmf2d3d9.dll") == nullptr) {
        conf::direct_render = false;
        ass::show_err("Boshyst menu only supports Direct3D 9 mode, you are using a different one");
        show_menu = false;
        // ASS(false);
        return;
    }
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
    ASS(kiero::bind(69, (void**)&oSetSamplerState, hkSetSamplerState) == kiero::Status::Success);
    ASS(kiero::bind(34, (void**)&oStretchRect, hkStretchRect) == kiero::Status::Success);
#if SHOW_STAGES
    cout << "graphics hooking 6\n";
#endif
    enable_hook();
#if SHOW_STAGES
    cout << "hooks enabled\n";
#endif
}

void try_to_init() {
#if SHOW_STAGES
    cout << "before hooking 1\n";
#endif
    // These values might be already set in BTAS mode
    // TODO: search in windows of the current process
    if (!hwnd)
        hwnd = FindWindowA(nullptr, "I Wanna Be The Boshy");
    if (!mhwnd)
        mhwnd = FindWindowExA(hwnd, nullptr, "Mf2EditClassTh", nullptr);
    ASS(mhwnd != nullptr);
#if SHOW_STAGES
    cout << "game hooks start 2\n";
#endif
    init_simple_hacks();
    enable_hook();
    if (!is_hourglass)
        fix_win32_theme();
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
        if (mem::get_base() != 0x400000) {
            ass::show_err("Invalid process. You should inject Boshyst only in the IWBTB process!");
            break;
        }
        is_hourglass = GetModuleHandleA("wintasee.dll") != nullptr;
        is_btas = !is_hourglass && GetModuleHandleA("Viewport.mfx") == nullptr;
        // Hacky if needed to work under hourglass
        if (PathFileExistsA("is_btas.txt"))
            is_btas = true;
#if defined(_DEBUG)
        if (true) {
#else
        if ((is_btas || is_hourglass) && 0) {
#endif
            // Alloc console as early as possible
            AllocConsole();
            freopen_s((FILE**)stdout, "CONOUT$", "w", stdout);
        }
        conf::read();
        if (is_btas || is_hourglass)
            conf::tas_mode = true;
        app_entry(nullptr);
        break;
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

