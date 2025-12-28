#pragma once

namespace conf {
	extern int pos[2];
	extern int size[2];
	extern int menu_hotkey;
	extern bool no_vp;
	extern bool no_sh;
	extern bool old_rec;
	extern bool god;
	extern bool menu;
	extern bool keep_save;
	extern bool no_cmove;
	extern bool draw_cursor;
	extern bool emu_mouse;
	extern bool cur_mouse_checked;
	extern bool allow_render;
	extern bool direct_render;
	extern bool fix_white_render;
	extern bool tas_mode;
	extern bool first_run;
	extern bool tp_on_click;
	extern bool skip_msg;
	extern bool input_in_menu;

	void read();
}
