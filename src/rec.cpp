#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include "rec.hpp"
#include "ass.hpp"
#include <vector>
#include <chrono>
#include <cstdlib>
#include <cstdio>
#include <memory>
#include <string>

extern HWND hwnd;
static long buffer_size = 0;
static HDC srcdc = nullptr;
static HDC memdc = nullptr;
static HBITMAP bmp = nullptr;
static HGDIOBJ old_bmp = nullptr;
static FILE* proc_stdin = nullptr;
static std::vector<BYTE> data_buffer;
static BITMAPINFO bmi;
static std::pair<int, int> ws;

static std::pair<int, int> win_size() {
    RECT rect_buf = { 0, 0, 0, 0 };
    if (GetClientRect(hwnd, &rect_buf)) {
        return { rect_buf.right, rect_buf.bottom };
    }
    return { 0, 0 };
}

static std::pair<int, int> get_win_top_left() {
    POINT point_buf = { 0, 0 };
    if (ClientToScreen(hwnd, &point_buf)) {
        return { point_buf.x, point_buf.y };
    }
    return { 0, 0 };
}

void rec::init() {
    srcdc = GetWindowDC(NULL);
    ASS(srcdc != nullptr);
    memdc = CreateCompatibleDC(srcdc);
    ASS(memdc != nullptr);
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    bmi.bmiHeader.biClrUsed = 0;
    bmi.bmiHeader.biClrImportant = 0;
    ws = ::win_size();
    bmi.bmiHeader.biWidth = ws.first;
    bmi.bmiHeader.biHeight = -ws.second;
    buffer_size = ws.first * ws.second * 4;
    data_buffer.reserve(buffer_size);
    bmp = CreateCompatibleBitmap(srcdc, ws.first, ws.second);
    ASS(bmp != nullptr);
    old_bmp = SelectObject(memdc, bmp);
    ASS(old_bmp != nullptr);
    std::string command = std::string("ffmpeg -y -f rawvideo") +
        " -vcodec rawvideo" +
        " -s " + std::to_string(ws.first) + "x" + std::to_string(ws.second) +
        " -pix_fmt rgb32" +
        " -r 50" +
        " -i - -an -vcodec mpeg4" +
        " -b 5000k" +
        " output.mp4";
        //" > nul 2>&1";
    proc_stdin = _popen(command.c_str(), "wb");
    ASS(proc_stdin != nullptr);
}

void rec::cap() {
    std::pair<int, int> real_pos = get_win_top_left();
    int real_x = real_pos.first;
    int real_y = real_pos.second;
    BOOL bitblt_success = BitBlt(
        memdc,                  // dest DC
        0, 0,                   // dest x, y
        ws.first,         // width
        ws.second,        // height
        srcdc,                  // source DC (screen)
        real_x, real_y,         // source x, y (screen coords of client top left)
        SRCCOPY | CAPTUREBLT    // Raster operation
    );
    ASS(bitblt_success);
    int bits = GetDIBits(
        memdc,                  // hdc
        bmp,                    // hbmp
        0,                      // start scan line
        ws.second,        // number of scan lines
        data_buffer.data(),     // lpbits
        &bmi,                   // lpbi
        DIB_RGB_COLORS          // usage
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
