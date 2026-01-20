#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include "rec.hpp"
#include "ass.hpp"
#include "conf.hpp"
#include "utils.hpp"
#include "btas.hpp"
#include <vector>
#include <cstdlib>
#include <cstdio>
#include <memory>
#include <iostream>
#include <string>
#include <MinHook.h>
#include <d3d9.h>

using std::cout;

namespace conf {
    extern std::string cap_cmd;
    extern int cap_start;
    extern int cap_cnt;
}
extern bool last_reset;

extern HWND hwnd;
extern HWND mhwnd;
static long buffer_size = 0;
static HDC srcdc = nullptr;
static HDC memdc = nullptr;
static HBITMAP bmp = nullptr;
static HGDIOBJ old_bmp = nullptr;
static HANDLE hChildStdinRead = nullptr;
static HANDLE hChildStdinWrite = nullptr;
static LPDIRECT3DSURFACE9 pSysSurface = nullptr;
static PROCESS_INFORMATION pi;
static std::vector<BYTE> data_buffer;
static BITMAPINFO bmi;
static std::pair<int, int> ws;
bool next_white = false;
bool capturing = false;

extern BOOL(__stdcall* SetWindowTextAOrig)(HWND, LPCSTR);

static void get_buf_size(LPDIRECT3DDEVICE9 pDevice, int& w_buf, int& h_buf) {
    if (pDevice == nullptr) {
        // Get default sizes
        get_win_size(w_buf, h_buf);
        return;
    }
    // Get D3D9 output size
    LPDIRECT3DSURFACE9 pBackBuffer = nullptr;
    ASS(pDevice->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &pBackBuffer) == D3D_OK);
    D3DSURFACE_DESC desc;
    ASS(pBackBuffer->GetDesc(&desc) == D3D_OK);
    pBackBuffer->Release();
    w_buf = (int)desc.Width;
    h_buf = (int)desc.Height;
}

void rec::init(void* dev) {
    get_buf_size((LPDIRECT3DDEVICE9)dev, ws.first, ws.second);
    buffer_size = ws.first * ws.second * 4;
    data_buffer.resize(buffer_size);
    if (dev == nullptr) {
        // BitBlt/PrintWindow capture
        srcdc = GetDC(hwnd);
        ASS(srcdc != nullptr);
        memdc = CreateCompatibleDC(srcdc);
        ASS(memdc != nullptr);
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 32;
        bmi.bmiHeader.biCompression = BI_RGB;
        bmi.bmiHeader.biClrUsed = 0;
        bmi.bmiHeader.biClrImportant = 0;
        bmi.bmiHeader.biWidth = ws.first;
        bmi.bmiHeader.biHeight = -ws.second;
        bmp = CreateCompatibleBitmap(srcdc, ws.first, ws.second);
        ASS(bmp != nullptr);
        old_bmp = SelectObject(memdc, bmp);
    }
    else {
        // D3D9 capture
        LPDIRECT3DDEVICE9 pDevice = (LPDIRECT3DDEVICE9)dev;
        LPDIRECT3DSURFACE9 pBackBuffer = nullptr;
        ASS(pDevice->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &pBackBuffer) == D3D_OK);
        D3DSURFACE_DESC desc;
        ASS(pBackBuffer->GetDesc(&desc) == D3D_OK);
        ASS(pDevice->CreateOffscreenPlainSurface(
            desc.Width, desc.Height, desc.Format,
            D3DPOOL_SYSTEMMEM, &pSysSurface, nullptr
        ) == D3D_OK);
        pBackBuffer->Release();
    }
    std::string command = "";
    // Yea it's ugly
    while (conf::cap_cmd.size() > 0) {
        if (starts_with(conf::cap_cmd, "$SIZE")) {
            command += to_str(ws.first) + "x" + to_str(ws.second);
            conf::cap_cmd = conf::cap_cmd.substr(5);
            continue;
        }
        command += conf::cap_cmd[0];
        conf::cap_cmd = conf::cap_cmd.substr(1);
    }
    std::cout << command << '\n';
    hChildStdinRead = nullptr;
    hChildStdinWrite = nullptr;
    SECURITY_ATTRIBUTES saAttr;
    ZeroMemory(&saAttr, sizeof(saAttr));
    saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
    saAttr.bInheritHandle = TRUE;
    saAttr.lpSecurityDescriptor = nullptr;
    ASS(CreatePipe(&hChildStdinRead, &hChildStdinWrite, &saAttr, 0));
    ASS(SetHandleInformation(hChildStdinWrite, HANDLE_FLAG_INHERIT, 0));
    STARTUPINFOW si;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.hStdInput = hChildStdinRead;
    si.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    si.hStdError = GetStdHandle(STD_ERROR_HANDLE);
    si.dwFlags |= STARTF_USESTDHANDLES;
    ZeroMemory(&pi, sizeof(pi));
    wchar_t* w_buf = utf8_to_unicode(command);
    ASS(CreateProcessW(
        nullptr,
        w_buf,
        nullptr,
        nullptr,
        TRUE,
        0,
        nullptr,
        nullptr,
        &si,
        &pi
    ));
    std::free(w_buf);
    CloseHandle(hChildStdinRead);
}

void rec::cap(void* dev) {
    if (dev == nullptr) {
        BOOL success;
        if (conf::old_rec) {
            success = BitBlt(
                memdc,
                0, 0,
                ws.first,
                ws.second,
                srcdc,
                0, 0,
                SRCCOPY | CAPTUREBLT
            );
        }
        else {
            success = PrintWindow(hwnd, memdc, PW_CLIENTONLY);
        }
        ASS(success);
        int bits = GetDIBits(
            memdc,
            bmp,
            0,
            ws.second,
            data_buffer.data(),
            &bmi,
            DIB_RGB_COLORS
        );
        ASS(bits == ws.second);
    }
    else if (!next_white) {
        LPDIRECT3DDEVICE9 pDevice = (LPDIRECT3DDEVICE9)dev;
        LPDIRECT3DSURFACE9 pBackBuffer = nullptr;
        ASS(pDevice->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &pBackBuffer) == D3D_OK);

        D3DSURFACE_DESC desc;
        ASS(pBackBuffer->GetDesc(&desc) == D3D_OK);

        ASS(pDevice->GetRenderTargetData(pBackBuffer, pSysSurface) == D3D_OK);
        D3DLOCKED_RECT lockedRect;
        auto temp_ret = pSysSurface->LockRect(&lockedRect, nullptr, D3DLOCK_READONLY);
        ASS(temp_ret == D3D_OK);
        if (temp_ret == D3D_OK) {
            ASS(data_buffer.size() == desc.Width * desc.Height * 4);
            unsigned char* pSrc = (unsigned char*)lockedRect.pBits;
            for (UINT y = 0; y < desc.Height; ++y) {
                memcpy(&data_buffer[y * desc.Width * 4], pSrc + (y * lockedRect.Pitch), desc.Width * 4);
            }
            pSysSurface->UnlockRect();
        }
        pBackBuffer->Release();
    }
    next_white = false;
    DWORD dwWritten;
    BOOL bSuccess = WriteFile(
        hChildStdinWrite,
        data_buffer.data(),
        data_buffer.size(),
        &dwWritten,
        nullptr
    );
    ASS(bSuccess);
    ASS(FlushFileBuffers(hChildStdinWrite));
}

void rec::stop(void* dev) {
    capturing = false;
    CloseHandle(hChildStdinWrite);
    WaitForSingleObject(pi.hProcess, INFINITE);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    ZeroMemory(&pi, sizeof(pi));
    if (dev == nullptr) {
        if (memdc && old_bmp) SelectObject(memdc, old_bmp);
        if (bmp) DeleteObject(bmp);
        if (memdc) DeleteDC(memdc);
        if (srcdc) ReleaseDC(nullptr, srcdc);
    }
    else {
        pSysSurface->Release();
    }
    hChildStdinWrite = nullptr;
    memdc = nullptr;
    old_bmp = nullptr;
    bmp = nullptr;
    memdc = nullptr;
    srcdc = nullptr;
}

void rec::rec_tick(void* dev) {
    conf::cur_mouse_checked = false;
    if (!conf::allow_render)
        return;
    if (conf::cap_start == 0 && conf::cap_cnt == 0) {
        // Special case
        char buf[32];
        int ret = GetWindowTextA(hwnd, buf, 32);
        ASS(ret > 0);
        buf[ret] = '\0';
        if (strcmp(buf, "I Wanna Be The Boshy R") == 0 && !capturing && (!is_btas || last_upd)) {
            capturing = true;
            rec::init(dev);
        }
        else if (strcmp(buf, "I Wanna Be The Boshy S") == 0 && capturing) {
            SetWindowTextAOrig(hwnd, "I Wanna Be The Boshy");
            capturing = false;
            rec::stop(dev);
        }
        if (capturing) {
            if (last_reset && strcmp(buf, "I Wanna Be The Boshy") == 0) {
                SetWindowTextAOrig(hwnd, "I Wanna Be The Boshy R");
                next_white = conf::fix_white_render;
            }
            if (!is_btas || last_upd)
                rec::cap(dev);
        }
        return;
    }
    // Legacy way
    if (is_btas && !last_upd)
        return;
    static int cur_total = 0;
    static int cur_cnt = 0;
    cur_total++;
    if (cur_total == conf::cap_start) {
        rec::init(dev);
        rec::cap(dev);
        cur_cnt++;
    }
    else if (cur_cnt > 0) {
        rec::cap(dev);
        cur_cnt++;
        if (cur_cnt == conf::cap_cnt) {
            rec::stop(dev);
            cur_cnt = 0;
        }
    }
}
