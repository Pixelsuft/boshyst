#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <cstring>
#include <string>
#include <fstream>
#include <iostream>
#include <algorithm>
#include "conf.hpp"
#include "ass.hpp"

using std::string;
using std::cout;

namespace conf {
    int pos[2];
    int size[2];
    bool no_vp;
    bool god;
    bool menu;
    bool keep_save;
    bool no_cmove;
}

static bool starts_with(const string& mainStr, const string& prefix) {
    if (mainStr.length() < prefix.length()) {
        return false;
    }
    return mainStr.compare(0, prefix.length(), prefix) == 0;
}

static int read_int(const string& line) {
    bool was_eq = false;
    bool was_set = false;
    int ret = 0;
    for (auto it = line.begin(); it != line.end(); it++) {
        if (*it == '=')
            was_eq = true;
        else if (was_eq && isdigit(*it)) {
            ret = (ret * 10) + (*it - '0');
            was_set = true;
        }
        else if (was_eq && !was_set && *it == 't')
            return 1;
        else if (was_eq && !was_set && *it == 'f')
            return 0;
    }
    return ret;
}

static void read_vec2_int(const string& line, const char* patt, int* buf) {
    ASS(sscanf(line.c_str(), patt, &buf[0], &buf[1]) == 2);
}

void conf::read() {
    conf::god = conf::no_vp = conf::keep_save = conf::no_cmove = false;
    conf::menu = true;
    pos[0] = pos[1] = 0;
    size[0] = 200;
    size[1] = 100;
    char path_buf[MAX_PATH + 1];
    memset(path_buf, 0, sizeof(path_buf));
    auto cwd_ret = GetCurrentDirectoryA(MAX_PATH, path_buf);
    ASS(cwd_ret > 0);
    path_buf[cwd_ret] = '\0';
    std::ifstream ifile;
    ifile.open(string(path_buf) + "\\boshyst.conf");
    ASS(ifile.is_open());
    if (!ifile.is_open()) {
        ass::show_err("Failed to open boshyst config");
        return;
    }
    std::string line;
    while (std::getline(ifile, line)) {
        line.erase(std::remove(line.begin(), line.end(), ' '), line.end());
        if (starts_with(line, "god"))
            conf::god = read_int(line) != 0;
        else if (starts_with(line, "disable_viewport"))
            conf::no_vp = read_int(line) != 0;
        else if (starts_with(line, "menu"))
            conf::menu = read_int(line) != 0;
        else if (starts_with(line, "keep_save"))
            conf::keep_save = read_int(line) != 0;
        else if (starts_with(line, "no_mouse_move"))
            conf::no_cmove = read_int(line) != 0;
        else if (starts_with(line, "win_pos"))
            read_vec2_int(line, "win_pos=%i,%i", pos);
        else if (starts_with(line, "win_size"))
            read_vec2_int(line, "win_size=%i,%i", size);
    }
    ifile.close();
#if 1
    cout << "config: \n";
    cout << "menu: " << conf::menu << std::endl;
    cout << "god: " << conf::god << std::endl;
    cout << "no viewport: " << conf::no_vp << std::endl;
    cout << "keep save: " << conf::keep_save << std::endl;
    cout << "no cursor move: " << conf::no_cmove << std::endl;
#endif
}
