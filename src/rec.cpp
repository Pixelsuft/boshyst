#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include "rec.hpp"
#include "ass.hpp"
#include <vector>
#include <cstdlib>
#include <cstdio>
#include <memory>
#include <string>

extern HWND hwnd;
extern HWND mhwnd;
static long buffer_size = 0;
static HDC srcdc = nullptr;
static HDC memdc = nullptr;
static HBITMAP bmp = nullptr;
static HGDIOBJ old_bmp = nullptr;
static FILE* proc_stdin = nullptr;
static std::vector<BYTE> data_buffer;
static BITMAPINFO bmi;
static std::pair<int, int> ws;

extern void get_win_size(int& w_buf, int& h_buf);

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
    std::string command = std::string("ffmpeg -y -f rawvideo") +
        " -vcodec rawvideo" +
        " -s " + std::to_string((long long)ws.first) + "x" + std::to_string((long long)ws.second) +
        " -pix_fmt rgb32" +
        " -r 50" +
        " -i - -an -vcodec mpeg4" +
        " -b 8000k" +
        " output.mp4";
        //" > nul 2>&1";
    proc_stdin = _popen(command.c_str(), "wb");
    ASS(proc_stdin != nullptr);
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
    fwrite(data_buffer.data(), 1, buffer_size, proc_stdin);
    fflush(proc_stdin);
}

void rec::stop() {
    if (proc_stdin) {
        _pclose(proc_stdin);
    }
    if (memdc && old_bmp) SelectObject(memdc, old_bmp);
    if (bmp) DeleteObject(bmp);
    if (memdc) DeleteDC(memdc);
    if (srcdc) ReleaseDC(NULL, srcdc);
    proc_stdin = nullptr;
    memdc = nullptr;
    old_bmp = nullptr;
    bmp = nullptr;
    memdc = nullptr;
    srcdc = nullptr;
}
