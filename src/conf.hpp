#pragma once

class Config {
public:
    float font_scale;
    int pos[2];
    int size[2];
    int full_size[2];
    int force_size[2];
    int menu_hotkey;
    int rapid_bind;
    int hitbox_level;
    int cap_start;
    int cap_cnt;
    int save_slot_fix;
    bool no_vp;
    bool no_sh;
    bool no_ps;
    bool cap_au;
    bool no_au;
    bool au_mth;
    bool old_rec;
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
    bool pixel_filter;
    bool force_gdi;
    bool hg_instant;
    bool tas_no_info;
    bool rng_patches;
    bool reset_rng;
    bool no_encryption;
    bool no_save_object_spamming;
};

extern Config conf;

namespace config {

void read();
} // namespace config
