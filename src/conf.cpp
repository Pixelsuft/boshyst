#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <cstring>
#include <string>
#include <iostream>
#include <algorithm>
#include "input.hpp"
#include "init.hpp"
#include "conf.hpp"
#include "ass.hpp"
#include "btas.hpp"
#include "fs.hpp"
#undef max
#undef min

using std::string;
using std::cout;

namespace conf {
    std::map<int, std::vector<InputEvent>> mb;
    string cap_cmd;
    int pos[2];
    int size[2];
    int full_size[2];
    int force_size[2];
    int cap_start;
    int cap_cnt;
    int rapid_bind;
    int menu_hotkey;
    int hitbox_level;
    float font_scale;
    bool old_rec;
    bool no_vp;
    bool no_ps;
    bool cap_au;
    bool no_au;
    bool au_mth;
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
    bool tas_mode;
    bool first_run;
    bool tp_on_click;
    bool skip_msg;
    bool input_in_menu;
    bool no_trans;
    bool force_gdi;
    bool pixel_filter;
    bool hg_instant;
    bool tas_no_info;
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
    if (mainStr.length() < prefix.length())
        return false;
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
        else if (was_eq && *it == '/')
            return ret;
        else if (was_eq && !was_set && tolower(*it) == 't')
            return 1;
        else if (was_eq && !was_set && tolower(*it) == 'f')
            return 0;
    }
    return ret;
}

static float read_float(const string& line) {
    for (int i = 0; i < (int)line.size(); i++) {
        if (line[i] == '=')
            return (float)atof(line.substr(i + 1).c_str());
    }
    return 1.f;
}

static void read_vec2_int(const string& line, const char* patt, int* buf) {
    ASS(sscanf(line.c_str(), patt, &buf[0], &buf[1]) == 2);
}

static void read_bind(const string& line_orig, const string& line) {
    int num;
    if (starts_with(line, "bind=click,")) {
        float x, y;
        ASS(sscanf(line.substr(11).c_str(), "%i,%f,%f", &num, &x, &y) == 3);
        InputEvent ev;
        ev.click.x = x;
        ev.click.y = y;
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
    else if (starts_with(line, "bind=save,") || starts_with(line, "bind=load,")) {
        if (is_hourglass || is_btas)
            return;
        std::string fn = line;
        ASS(sscanf(line.substr(10).c_str(), "%i", &num) == 1);
        // Ugly.
        while (fn.size() > 0 && (isspace(fn[0]) || std::find(fn.begin(), fn.end(), ',') != fn.end()))
            fn = fn.substr(1);
        InputEvent ev(fn, starts_with(line, "bind=save,") ? InputEvent::SAVE : InputEvent::LOAD);
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
    else if (starts_with(line, "bind=rapid,")) {
        if (is_hourglass || is_btas)
            return;
        ASS(sscanf(line.substr(11).c_str(), "%i", &num) == 1);
        conf::rapid_bind = num;
    }
    else {
        ass::show_err("Invalid bind");
        ASS(false);
    }
}

static void create_default_config(const string& path) {
    bfs::File file(path, 1);
    ASS(file.is_open());
    ASS(file.write_line("menu = 1 // Show menu window"));
    ASS(file.write_line("hourglass_instant = 0 // Instant mod load in hourglass, may cause softlock"));
    ASS(file.write_line("tas_mode = 0 // Use small info window, useful for TASing (automatically enabled when using Hourglass or BTAS)"));
    ASS(file.write_line("win_pos = 0, 0 // Info window position"));
    ASS(file.write_line("win_size = 200, 100 // Info window size"));
    ASS(file.write_line("menu_hotkey = 45 // Default menu toggle hotkey, VK_INSERT"));
    ASS(file.write_line("fullscreen_size = 0, 0 // Leave defaults for fullscreen mode (also use -1, -1 for desktop resolution)"));
    ASS(file.write_line(""));
    ASS(file.write_line("god = 0 // God mode"));
    ASS(file.write_line("teleport_with_mouse = 0 // Teleport player using mouse"));
    ASS(file.write_line("hitbox_level = 0 // Can be changed to 1 or 2 to show default player"));
    ASS(file.write_line("pixel_filter = 0 // Make textures look pixelated instead of blurry"));
    ASS(file.write_line("font_scale = 1.0 // Here you can make UI bigger"));
    ASS(file.write_line("disable_viewport = 0 // Disable camera manipulation"));
    ASS(file.write_line("disable_shaders = 0 // Disable shaders"));
    ASS(file.write_line("disable_transitions = 0 // Disable transition when using teleporter"));
    ASS(file.write_line("disable_perspective = 0 // Disable background distortion"));
    ASS(file.write_line("skip_messageboxes = 0 // Don't show message boxes from the game"));
    ASS(file.write_line("keep_save = 0 // Prevent overriding save files (use temporary ini files instead)"));
    ASS(file.write_line(""));
    ASS(file.write_line("tas_force_size = 0, 0 // You can force window size to be higher than you monitor resolution"));
    ASS(file.write_line("tas_force_gdi = 0 // Force software renderer (doesnt support UI)"));
    ASS(file.write_line("tas_disable_audio = 0 // Disable audio output"));
    ASS(file.write_line("tas_audio_capture = 0 // Capture audio in tas mode"));
    ASS(file.write_line("tas_audio_main_thread = 0 // Force audio processing on main thread for stability"));
    ASS(file.write_line("tas_replay_mode = 0 // Replay mode by default"));
    ASS(file.write_line("tas_no_info = 0 // Hide info window"));
    ASS(file.write_line(""));
    ASS(file.write_line("allow_render = 0 // Allow video capturing"));
    ASS(file.write_line("direct_render = 1 // Capture video directly using Direct3D 9 instead of making screenshots of the window using Win32 API"));
    ASS(file.write_line("old_render = 0 // Turn on this if you have rendering issues (only when not using direct render; likely will cause window content overriding)"));
    ASS(file.write_line("render_start = 0 // Start frame (not recommended to use, see readme)"));
    ASS(file.write_line("render_count = 0 // Frame count (not recommended to use, see readme)"));
    ASS(file.write_line("# render_end = 0 // End frame"));
    ASS(file.write_line("render_cmd = ffmpeg -y -f:v rawvideo -s $SIZE -pix_fmt rgb32 -r 50 -i - -an -vcodec libx264 -b:v 10000k output.mp4"));
    ASS(file.write_line(""));
    ASS(file.write_line("no_mouse_move = 1 // Prevent mouse cursor from moving to kill the player"));
    ASS(file.write_line("draw_cursor = 1 // Draw virtual cursor pos on screen (kinda pointless)"));
    ASS(file.write_line("simulate_mouse = 0 // Allow simulating mouse via keyboard (disables real mouse input)"));
    ASS(file.write_line("# Mouse on keyboard bindings (useful for hourglass)"));
    ASS(file.write_line("# Multible mouse binds to one key are supported (order is important)"));
    ASS(file.write_line("# Boshy selection example (via 'K' key) from F3 menu"));
    ASS(file.write_line("bind = click, 75, 84.0, 276.0 // Virtual Key (here is K), X pos in [0;640), Y pos in [0;480) (move cursor and click on Quadrick)"));
    ASS(file.write_line("bind = click, 75, 315.0, 406.0 // Click 'Select' button"));
    ASS(file.write_line("bind = click, 75, -100, -100 // Move mouse outside (so not clicking, just move) of the window"));
    ASS(file.write_line("# State saving/loading (useful for training)"));
    ASS(file.write_line("bind = load, 70, example_state.mfs // 'F' to load state"));
    ASS(file.write_line("bind = save, 71, example_state.mfs // 'G' to save state"));
    ASS(file.write_line("bind = rapid, 66 // 'B' for rapid fire hack"));
    ASS(file.write_line(""));
    ASS(file.write_line("# Map BTAS keys: map, keyboard_key, mod (0 - None, 1 - Ctrl, 2 - Shift), ingame_key"));
    ASS(file.write_line("btas = map, 82, 0, 82 // 'R'"));
    ASS(file.write_line("btas = map, 90, 0, 90 // 'Z'"));
    ASS(file.write_line("btas = map, 88, 0, 88 // 'X'"));
    ASS(file.write_line("btas = map, 67, 0, 67 // 'C'"));
    ASS(file.write_line("btas = map, 83, 0, 83 // 'S'"));
    ASS(file.write_line("btas = map, 37, 0, 37 // Left"));
    ASS(file.write_line("btas = map, 39, 0, 39 // Right"));
    ASS(file.write_line("btas = map, 40, 0, 40 // Down"));
    ASS(file.write_line("btas = map, 73, 0, 113 // F2 on 'I'"));
    ASS(file.write_line("btas = map, 79, 0, 114 // F3 on 'O'"));
    ASS(file.write_line("btas = map, 13, 0, 13 // Enter"));
    ASS(file.write_line("btas = map, 27, 0, 27 // Escape"));
    ASS(file.write_line("# BTAS states: function, keyboard_key, mod, state index"));
    ASS(file.write_line("# F1 - F10 to save"));
    ASS(file.write_line("btas = save_state, 112, 0, 1"));
    ASS(file.write_line("btas = save_state, 113, 0, 2"));
    ASS(file.write_line("btas = save_state, 114, 0, 3"));
    ASS(file.write_line("btas = save_state, 115, 0, 4"));
    ASS(file.write_line("btas = save_state, 116, 0, 5"));
    ASS(file.write_line("btas = save_state, 117, 0, 6"));
    ASS(file.write_line("btas = save_state, 118, 0, 7"));
    ASS(file.write_line("btas = save_state, 119, 0, 8"));
    ASS(file.write_line("btas = save_state, 120, 0, 9"));
    ASS(file.write_line("btas = save_state, 121, 0, 10"));
    ASS(file.write_line("# '1' - '9', '0' to load"));
    ASS(file.write_line("btas = load_state, 49, 0, 1"));
    ASS(file.write_line("btas = load_state, 50, 0, 2"));
    ASS(file.write_line("btas = load_state, 51, 0, 3"));
    ASS(file.write_line("btas = load_state, 52, 0, 4"));
    ASS(file.write_line("btas = load_state, 53, 0, 5"));
    ASS(file.write_line("btas = load_state, 54, 0, 6"));
    ASS(file.write_line("btas = load_state, 55, 0, 7"));
    ASS(file.write_line("btas = load_state, 56, 0, 8"));
    ASS(file.write_line("btas = load_state, 57, 0, 9"));
    ASS(file.write_line("btas = load_state, 48, 0, 10"));
    ASS(file.write_line("# BTAS binds: function, keyboard_key, mod"));
    ASS(file.write_line("btas = toggle_pause, 19, 0 // PAUSE to pause/resume"));
    ASS(file.write_line("btas = fastforward, 9, 0 // Hold TAB for fastforward, you can use toggle_fastforward as well!"));
    ASS(file.write_line("btas = fastforward_skip, 66, 0 // Hold 'B' for fastforward without drawing frames (you might need toggle_fastforward_skip)"));
    ASS(file.write_line("btas = step, 86, 0 // Play single frame on 'V'"));
    ASS(file.write_line("btas = slowmotion, 32, 0 // Slow-mo on Space"));
}

void conf::read() {
    cap_cmd = "";
    cap_start = 0;
    cap_cnt = 0;
    rapid_bind = -1;
    first_run = false;
    tas_mode = skip_msg = god = no_vp = old_rec = no_au = force_gdi =
        no_sh = keep_save = no_trans = no_ps = au_mth = cap_au = tas_no_info =
        no_cmove = draw_cursor = emu_mouse = allow_render = hg_instant = false;
    direct_render = true;
	cur_mouse_checked = false;
    tp_on_click = input_in_menu = false;
    font_scale = 1.f;
    menu_hotkey = 45;
    menu = true;
    pos[0] = pos[1] = 0;
    size[0] = 200;
    size[1] = 100;
    string file_path = get_config_path();
    bfs::File ifile(file_path, 0);
    if (!ifile.is_open()) {
        create_default_config(file_path);
        ifile = bfs::File(file_path, 0);
        ASS(ifile.is_open());
        first_run = true;
    }
    int cap_end = -1;
    string line;
    while (ifile.read_line(line)) {
        string line_orig = line;
        line.erase(std::remove(line.begin(), line.end(), ' '), line.end());
        if (line.size() < 3 || (line[0] == '/' && line[1] == '/') || line[0] == '#')
            continue;
        // cout << "line: " << line << std::endl;
        // cout << "orig: " << line_orig << std::endl;
        if (starts_with(line, "god="))
            god = read_int(line) != 0;
        else if (starts_with(line, "teleport_with_mouse="))
            tp_on_click = read_int(line) != 0;
        else if (starts_with(line, "tas_mode="))
            tas_mode = read_int(line) != 0;
        else if (starts_with(line, "hourglass_instant="))
            hg_instant = read_int(line) != 0;
        else if (starts_with(line, "disable_viewport="))
            no_vp = read_int(line) != 0;
        else if (starts_with(line, "disable_shaders="))
            no_sh = read_int(line) != 0;
        else if (starts_with(line, "disable_transitions="))
            no_trans = read_int(line) != 0;
        else if (starts_with(line, "disable_perspective="))
            no_ps = read_int(line) != 0;
        else if (starts_with(line, "pixel_filter="))
            pixel_filter = read_int(line) != 0;
        else if (starts_with(line, "skip_messageboxes="))
            skip_msg = read_int(line) != 0;
        else if (starts_with(line, "hitbox_level="))
            hitbox_level = std::max(read_int(line), 0);
        else if (starts_with(line, "menu_hotkey="))
            menu_hotkey = read_int(line);
        else if (starts_with(line, "menu="))
            menu = read_int(line) != 0;
        else if (starts_with(line, "keep_save="))
            keep_save = read_int(line) != 0;
        else if (starts_with(line, "no_mouse_move="))
            no_cmove = read_int(line) != 0;
        else if (starts_with(line, "draw_cursor="))
            draw_cursor = read_int(line) != 0;
        else if (starts_with(line, "simulate_mouse="))
            emu_mouse = read_int(line) != 0;
        else if (starts_with(line, "tas_force_gdi="))
            force_gdi = read_int(line) != 0;
        else if (starts_with(line, "tas_disable_audio="))
            no_au = read_int(line) != 0;
        else if (starts_with(line, "tas_audio_capture="))
            cap_au = read_int(line) != 0;
        else if (starts_with(line, "tas_audio_main_thread="))
            au_mth = read_int(line) != 0;
        else if (starts_with(line, "tas_replay_mode="))
            is_replay = read_int(line) != 0;
        else if (starts_with(line, "tas_no_info="))
            tas_no_info = read_int(line) != 0;
        else if (starts_with(line, "allow_render="))
            allow_render = read_int(line) != 0;
        else if (starts_with(line, "direct_render="))
            direct_render = read_int(line) != 0;
        else if (starts_with(line, "old_render="))
            old_rec = read_int(line) != 0;
        else if (starts_with(line, "render_start="))
            cap_start = read_int(line);
        else if (starts_with(line, "render_count="))
            cap_cnt = read_int(line);
        else if (starts_with(line, "render_end="))
            cap_end = read_int(line);
        else if (starts_with(line, "font_scale="))
            font_scale = read_float(line);
        else if (starts_with(line, "win_pos="))
            read_vec2_int(line, "win_pos=%i,%i", pos);
        else if (starts_with(line, "win_size"))
            read_vec2_int(line, "win_size=%i,%i", size);
        else if (starts_with(line, "fullscreen_size"))
            read_vec2_int(line, "fullscreen_size=%i,%i", full_size);
        else if (starts_with(line, "tas_force_size"))
            read_vec2_int(line, "tas_force_size=%i,%i", force_size);
        else if (starts_with(line, "bind="))
            read_bind(line_orig, line);
        else if (starts_with(line, "btas="))
            btas::read_setting(line, line_orig);
        else if (starts_with(line, "render_cmd=")) {
            cap_cmd = line_orig.substr(10);
            while (cap_cmd.size() > 0 && (isspace(cap_cmd[0]) || cap_cmd[0] == '='))
                cap_cmd = cap_cmd.substr(1);
        }
        else {
            ass::show_err((string("Unknown setting: ") + line_orig).c_str());
            ASS(false);
        }
    }
    if (font_scale <= 0.f)
        font_scale = 1.f;
    if (cap_end > 0)
        cap_cnt = cap_end - cap_start;
}
