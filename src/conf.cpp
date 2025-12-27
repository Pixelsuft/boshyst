#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <cstring>
#include <string>
#include <iostream>
#include <algorithm>
#include "input.hpp"
#include "conf.hpp"
#include "ass.hpp"
#include "fs.hpp"

using std::string;
using std::cout;

namespace conf {
    std::map<int, std::vector<InputEvent>> mb;
    string cap_cmd;
    int pos[2];
    int size[2];
    int cap_start;
    int cap_cnt;
    int menu_hotkey;
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
    bool fix_white_render;
    bool tas_mode;
    bool first_run;
    bool tp_on_click;
}

extern std::string unicode_to_utf8(wchar_t* buf, bool autofree);

std::string get_config_path() {
    wchar_t path_buf[MAX_PATH + 1];
    memset(path_buf, 0, sizeof(path_buf));
    auto cwd_ret = GetCurrentDirectoryW(MAX_PATH, path_buf);
    ASS(cwd_ret > 0);
    path_buf[cwd_ret] = L'\0';
    return unicode_to_utf8(path_buf, false) + "\\boshyst.conf";
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

static void create_default_config(const string& path) {
    bfs::File file(path, 1);
    ASS(file.is_open());
    ASS(file.write_line("menu = 1 // Show menu window"));
    ASS(file.write_line("tas_mode = 0 // Use small info window, useful for TASing"));
    ASS(file.write_line("win_pos = 0, 0 // Info window position"));
    ASS(file.write_line("win_size = 200, 100 // Info window size"));
    ASS(file.write_line("menu_hotkey = 45 // Default menu toggle hotkey, VK_INSERT"));
    ASS(file.write_line(""));
    ASS(file.write_line("god = 0 // God mode"));
    ASS(file.write_line("disable_viewport = 0 // Disable camera manipulation"));
    ASS(file.write_line("disable_shaders = 0 // Disable shaders"));
    ASS(file.write_line("keep_save = 0 // Prevent overriding save files (use temporary ini files instead)"));
    ASS(file.write_line(""));
    ASS(file.write_line("allow_render = 0 // Allow video capturing"));
    ASS(file.write_line("direct_render = 1 // Capture video directly using Direct3D 9 instead of making screenshots of the window using Win32 API"));
    ASS(file.write_line("fix_white_render = 1 // Fix white screen when using direct render"));
    ASS(file.write_line("old_render = 0 // Turn on this if you have rendering issues (only when not using direct render; likely will cause window content overriding)"));
    ASS(file.write_line("render_start = 0 // Start frame (not recommended to use, see readme)"));
    ASS(file.write_line("render_count = 0 // Frame count (not recommended to use, see readme)"));
    ASS(file.write_line("# render_end = 0 // End frame"));
    ASS(file.write_line("render_cmd = ffmpeg -y -f:v rawvideo -s $SIZE -pix_fmt rgb32 -r 50 -i - -an -v:c libx264 -b:v 10000k output.mp4"));
    ASS(file.write_line(""));
    ASS(file.write_line("no_mouse_move = 1 // Prevent mouse cursor from moving to kill the player"));
    ASS(file.write_line("draw_cursor = 1 // Draw virtual cursor pos on screen (kinda pointless)"));
    ASS(file.write_line("simulate_mouse = 0 // Allow simulating mouse via keyboard (disables real mouse input)"));
    ASS(file.write_line("# Multible mouse binds to one key are supported (order is important)"));
    ASS(file.write_line("# Boshy selection example (via 'K' key) from F3 menu"));
    ASS(file.write_line("mouse_bind = 75, 84.0, 276.0 // Virtual Key (here is K), X pos in [0;640), Y pos in [0;480) (move cursor and click on Quadrick)"));
    ASS(file.write_line("mouse_bind = 75, 315.0, 406.0 // Click 'Select' button"));
    ASS(file.write_line("mouse_bind = 75, -100, -100 // Move mouse outside (so not clicking, just move) of the window"));
}

void conf::read() {
    conf::cap_cmd = "";
    conf::cap_start = 0;
    conf::cap_cnt = 0;
    conf::first_run = false;
    conf::tas_mode = conf::god = conf::no_vp = conf::old_rec = conf::no_sh = conf::keep_save = conf::no_cmove = conf::draw_cursor = conf::emu_mouse = conf::allow_render = false;
    conf::direct_render = conf::fix_white_render = true;
	conf::cur_mouse_checked = false;
    conf::tp_on_click = false;
    conf::menu_hotkey = 45;
    conf::menu = true;
    pos[0] = pos[1] = 0;
    size[0] = 200;
    size[1] = 100;
    string file_path = get_config_path();
    bfs::File ifile(file_path, 0);
    if (!ifile.is_open()) {
        create_default_config(file_path);
        ifile = bfs::File(file_path, 0);
        ASS(ifile.is_open());
        conf::first_run = true;
    }
    int cap_end = -1;
    string line;
    while (ifile.read_line(line)) {
        string line_orig = line;
        line.erase(std::remove(line.begin(), line.end(), ' '), line.end());
        // cout << "line: " << line << std::endl;
        // cout << "orig: " << line_orig << std::endl;
        if (starts_with(line, "god="))
            conf::god = read_int(line) != 0;
        else if (starts_with(line, "tas_mode="))
            conf::tas_mode = read_int(line) != 0;
        else if (starts_with(line, "disable_viewport="))
            conf::no_vp = read_int(line) != 0;
        else if (starts_with(line, "disable_shaders="))
            conf::no_sh = read_int(line) != 0;
        else if (starts_with(line, "menu_hotkey="))
            conf::menu_hotkey = read_int(line);
        else if (starts_with(line, "menu="))
            conf::menu = read_int(line) != 0;
        else if (starts_with(line, "keep_save="))
            conf::keep_save = read_int(line) != 0;
        else if (starts_with(line, "no_mouse_move="))
            conf::no_cmove = read_int(line) != 0;
        else if (starts_with(line, "draw_cursor="))
            conf::draw_cursor = read_int(line) != 0;
        else if (starts_with(line, "simulate_mouse="))
            conf::emu_mouse = read_int(line) != 0;
        else if (starts_with(line, "allow_render="))
            conf::allow_render = read_int(line) != 0;
        else if (starts_with(line, "direct_render="))
            conf::direct_render = read_int(line) != 0;
        else if (starts_with(line, "old_render="))
            conf::old_rec = read_int(line) != 0;
        else if (starts_with(line, "fix_white_render="))
            conf::fix_white_render = read_int(line) != 0;
        else if (starts_with(line, "render_start="))
            conf::cap_start = read_int(line);
        else if (starts_with(line, "render_count="))
            conf::cap_cnt = read_int(line);
        else if (starts_with(line, "render_end="))
            cap_end = read_int(line);
        else if (starts_with(line, "win_pos="))
            read_vec2_int(line, "win_pos=%i,%i", pos);
        else if (starts_with(line, "win_size"))
            read_vec2_int(line, "win_size=%i,%i", size);
        else if (starts_with(line, "mouse_bind="))
            read_mouse_bind(line.substr(11));
        else if (starts_with(line, "render_cmd=")) {
            conf::cap_cmd = line_orig.substr(10);
            while (conf::cap_cmd.size() > 0 && (isspace(conf::cap_cmd[0]) || conf::cap_cmd[0] == '='))
                conf::cap_cmd = conf::cap_cmd.substr(1);
        }
    }
    if (cap_end > 0)
        conf::cap_cnt = cap_end - conf::cap_start;
}
