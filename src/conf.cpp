#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <cstring>
#include <string>
#include <fstream>
#include <iostream>
#include <algorithm>
#include "input.hpp"
#include "conf.hpp"
#include "ass.hpp"

using std::string;
using std::cout;

namespace conf {
    std::map<int, std::vector<InputEvent>> mb;
    string cap_cmd;
    int pos[2];
    int size[2];
    int cap_start;
    int cap_cnt;
    bool old_rec;
    bool no_vp;
    bool no_sh;
    bool god;
    bool menu;
    bool keep_save;
    bool no_cmove;
    bool draw_cursor;
    bool emu_mouse;
    bool cur_mouse_checked;
    bool allow_render;
    bool direct_render;
}

bool starts_with(const string& mainStr, const string& prefix) {
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
        else if (was_eq && !was_set && tolower(*it) == 't')
            return 1;
        else if (was_eq && !was_set && tolower(*it) == 'f')
            return 0;
    }
    return ret;
}

static void read_vec2_int(const string& line, const char* patt, int* buf) {
    ASS(sscanf(line.c_str(), patt, &buf[0], &buf[1]) == 2);
}

static void read_mouse_bind(const string& line) {
    int num;
    float x, y;
    ASS(sscanf(line.c_str(), "%i,%f,%f", &num, &x, &y) == 3);
    InputEvent ev;
    ev.x = x;
    ev.y = y;
    auto it = conf::mb.find(num);
    if (it == conf::mb.end()) {
        std::vector<InputEvent> temp_vec;
        temp_vec.push_back(std::move(ev));
        conf::mb[num] = std::move(temp_vec);
    }
    else {
        it->second.push_back(std::move(ev));
    }
}

void conf::read() {
    conf::cap_cmd = "";
    conf::cap_start = 0;
    conf::cap_cnt = 0;
    conf::god = conf::no_vp = conf::old_rec = conf::no_sh = conf::keep_save = conf::no_cmove = conf::draw_cursor = conf::emu_mouse = conf::allow_render = false;
    conf::direct_render = true;
	conf::cur_mouse_checked = false;
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
    int cap_end = -1;
    std::string line;
    while (std::getline(ifile, line)) {
        if (starts_with(line, "render_cmd")) {
            // Special case
            conf::cap_cmd = line.substr(10);
            while (conf::cap_cmd.size() > 0 && (isspace(conf::cap_cmd[0]) || conf::cap_cmd[0] == '='))
                conf::cap_cmd = conf::cap_cmd.substr(1);
        }
        line.erase(std::remove(line.begin(), line.end(), ' '), line.end());
        if (starts_with(line, "god"))
            conf::god = read_int(line) != 0;
        else if (starts_with(line, "disable_viewport"))
            conf::no_vp = read_int(line) != 0;
        else if (starts_with(line, "disable_shaders"))
            conf::no_sh = read_int(line) != 0;
        else if (starts_with(line, "menu"))
            conf::menu = read_int(line) != 0;
        else if (starts_with(line, "keep_save"))
            conf::keep_save = read_int(line) != 0;
        else if (starts_with(line, "no_mouse_move"))
            conf::no_cmove = read_int(line) != 0;
        else if (starts_with(line, "draw_cursor"))
            conf::draw_cursor = read_int(line) != 0;
        else if (starts_with(line, "simulate_mouse"))
            conf::emu_mouse = read_int(line) != 0;
        else if (starts_with(line, "allow_render"))
            conf::allow_render = read_int(line) != 0;
        else if (starts_with(line, "direct_render"))
            conf::direct_render = read_int(line) != 0;
        else if (starts_with(line, "old_render"))
            conf::old_rec = read_int(line) != 0;
        else if (starts_with(line, "render_start"))
            conf::cap_start = read_int(line);
        else if (starts_with(line, "render_count"))
            conf::cap_cnt = read_int(line);
        else if (starts_with(line, "render_end"))
            cap_end = read_int(line);
        else if (starts_with(line, "win_pos"))
            read_vec2_int(line, "win_pos=%i,%i", pos);
        else if (starts_with(line, "win_size"))
            read_vec2_int(line, "win_size=%i,%i", size);
        else if (starts_with(line, "mouse_bind"))
            read_mouse_bind(line.substr(11));
    }
    if (cap_end > 0) {
        conf::cap_cnt = cap_end - conf::cap_start;
    }
    ifile.close();
#if 0
    cout << "config: \n";
    cout << "menu: " << conf::menu << std::endl;
    cout << "god: " << conf::god << std::endl;
    cout << "no viewport: " << conf::no_vp << std::endl;
    cout << "keep save: " << conf::keep_save << std::endl;
    cout << "no cursor move: " << conf::no_cmove << std::endl;
    cout << "draw cursor: " << conf::draw_cursor << std::endl;
    cout << "emu mouse: " << conf::emu_mouse << std::endl;
#endif
}
