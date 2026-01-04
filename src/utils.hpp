#pragma once
#include <string>

inline std::string to_str(int x) {
#if _MSC_VER >= 1900
	return std::to_string((long long)x);
#else
	return std::to_string(x);
#endif
}

static bool c_ends_with(const char* str, const char* end) {
    size_t sl = strlen(str);
    size_t el = strlen(end);
    if (el > sl)
        return false;
    return memcmp(str + sl - el, end, el) == 0;
}

bool MyKeyState(int k);
int JustKeyState(int k);
wchar_t* utf8_to_unicode(const std::string& utf8);
std::string unicode_to_utf8(wchar_t* buf, bool autofree);
void get_win_size(int& w_buf, int& h_buf);
void get_cursor_pos(int& x_buf, int& y_buf);
void get_cursor_pos_orig(int& x_buf, int& y_buf);
int get_scene_id();
void* get_player_ptr(int s);
void init_game_loop();
bool starts_with(const std::string& mainStr, const std::string& prefix);
std::string get_config_path();
