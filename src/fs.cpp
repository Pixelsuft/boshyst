#define WIN32_LEAN_AND_MEAN
#include "fs.hpp"
#include "ass.hpp"
#include <cstdlib>
#include <Windows.h>

using std::string;
using bfs::File;

extern wchar_t* utf8_to_unicode(const std::string& utf8);

File::File(const string& path, int mode) noexcept {
    wchar_t* w_path = utf8_to_unicode(path);
    handle = (void*)CreateFileW(
        w_path,
        mode == 1 ? GENERIC_WRITE : GENERIC_READ,
        mode == 1 ? 0 : FILE_SHARE_READ,
        nullptr,
        mode == 1 ? CREATE_ALWAYS : OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr
    );
    std::free(w_path);
}

File::File(File&& other) noexcept : handle(other.handle) {
    other.handle = INVALID_HANDLE_VALUE;
}

File& File::operator=(File&& other) noexcept {
    if (this != &other) {
        close();
        handle = other.handle;
        other.handle = INVALID_HANDLE_VALUE;
    }
    return *this;
}

bool File::is_open() {
	return handle != INVALID_HANDLE_VALUE;
}

bool File::read_line(std::string& line) {
    ASS(is_open());
    line.clear();
    line.reserve(128);
    char buffer;
    DWORD bytesRead;
    while (ReadFile(handle, &buffer, 1, &bytesRead, nullptr) && bytesRead > 0) {
        if (buffer == '\n') {
            if (line.size() == 0)
                continue;
            break;
        }
        if (buffer != '\r')
            line += buffer;
    }
    return line.size() > 0;
}

bool File::write(const void* buf, size_t size) {
    ASS(is_open());
    DWORD bytesWritten;
    return WriteFile(handle, buf, (DWORD)size, &bytesWritten, nullptr) && (DWORD)size == bytesWritten;
}

void File::close() {
    if (is_open()) {
        ASS(CloseHandle(handle) != 0);
        handle = INVALID_HANDLE_VALUE;
    }
}
