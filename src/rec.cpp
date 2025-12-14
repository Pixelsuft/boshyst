#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include "rec.hpp"
#include "ass.hpp"
#include "conf.hpp"
#include <vector>
#include <cstdlib>
#include <cstdio>
#include <memory>
#include <iostream>
#include <string>
#include <MinHook.h>

using std::cout;

namespace conf {
    extern std::string cap_cmd;
    extern int cap_start;
    extern int cap_cnt;
}

extern HWND hwnd;
extern HWND mhwnd;
static long buffer_size = 0;
static HDC srcdc = nullptr;
static HDC memdc = nullptr;
static HBITMAP bmp = nullptr;
static HGDIOBJ old_bmp = nullptr;
static HANDLE hChildStdinRead = nullptr;
static HANDLE hChildStdinWrite = nullptr;
static PROCESS_INFORMATION pi;
static std::vector<BYTE> data_buffer;
static BITMAPINFO bmi;
static std::pair<int, int> ws;

extern void get_win_size(int& w_buf, int& h_buf);
extern bool starts_with(const std::string& mainStr, const std::string& prefix);

void rec::pre_hook() {

}

void rec::init() {
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
    get_win_size(ws.first, ws.second);
    bmi.bmiHeader.biWidth = ws.first;
    bmi.bmiHeader.biHeight = -ws.second;
    buffer_size = ws.first * ws.second * 4;
    data_buffer.resize(buffer_size);
    bmp = CreateCompatibleBitmap(srcdc, ws.first, ws.second);
    ASS(bmp != nullptr);
    old_bmp = SelectObject(memdc, bmp);
    std::string command = "";
    // Yea it's ugly
    while (conf::cap_cmd.size() > 0) {
        if (starts_with(conf::cap_cmd, "$SIZE")) {
            command += std::to_string((long long)ws.first) + "x" + std::to_string((long long)ws.second);
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
    STARTUPINFOA si;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.hStdInput = hChildStdinRead;
    si.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    si.hStdError = GetStdHandle(STD_ERROR_HANDLE);
    si.dwFlags |= STARTF_USESTDHANDLES;
    ZeroMemory(&pi, sizeof(pi));
    std::vector<char> cmd_chars(command.begin(), command.end());
    cmd_chars.push_back('\0');
    ASS(CreateProcessA(
        nullptr,
        cmd_chars.data(),
        nullptr,
        nullptr,
        TRUE,
        0,
        nullptr,
        nullptr,
        &si,
        &pi
    ));
    CloseHandle(hChildStdinRead);
}

void rec::cap() {
#if 1
    BOOL success = PrintWindow(hwnd, memdc, PW_CLIENTONLY);
#else
    BOOL success = BitBlt(
        memdc,
        0, 0,
        ws.first,
        ws.second,
        srcdc,
        0, 0,
        SRCCOPY | CAPTUREBLT
    );
#endif
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

void rec::stop() {
    CloseHandle(hChildStdinWrite);
    WaitForSingleObject(pi.hProcess, INFINITE);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    ZeroMemory(&pi, sizeof(pi));
    if (memdc && old_bmp) SelectObject(memdc, old_bmp);
    if (bmp) DeleteObject(bmp);
    if (memdc) DeleteDC(memdc);
    if (srcdc) ReleaseDC(nullptr, srcdc);
    hChildStdinWrite = nullptr;
    memdc = nullptr;
    old_bmp = nullptr;
    bmp = nullptr;
    memdc = nullptr;
    srcdc = nullptr;
}

void rec::rec_tick() {
    conf::cur_mouse_checked = false;
    if (!conf::allow_render)
        return;
    if (conf::cap_start == 0 && conf::cap_cnt == 0) {
        // Special case
        static bool capturing = false;
        char buf[32];
        int ret = GetWindowTextA(hwnd, buf, 32);
        ASS(ret > 0);
        buf[ret] = '\0';
        if (strcmp(buf, "I Wanna Be The Boshy R") == 0 && !capturing) {
            capturing = true;
            rec::init();
        }
        else if (strcmp(buf, "I Wanna Be The Boshy S") == 0 && capturing) {
            capturing = false;
            rec::stop();
        }
        if (capturing) {
            rec::cap();
        }
        return;
    }
    static int cur_total = 0;
    static int cur_cnt = 0;
    cur_total++;
    if (cur_total == conf::cap_start) {
        rec::init();
        rec::cap();
        cur_cnt++;
    }
    else if (cur_cnt > 0) {
        rec::cap();
        cur_cnt++;
        if (cur_cnt == conf::cap_cnt) {
            rec::stop();
            cur_cnt = 0;
        }
    }
}
