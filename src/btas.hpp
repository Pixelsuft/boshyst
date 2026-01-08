#pragma once
#include <string>

extern bool is_btas;
extern bool last_upd;
extern bool b_loading_state;
void launch_bullet(int x, int y, int dir);

namespace btas {
	short TasGetKeyState(int k);
	bool on_before_update();
	void on_after_update();
	unsigned long get_time();
	void read_setting(const std::string& line, const std::string& line_orig);
	void pre_init();
	void init();
	void draw_info();
	void draw_tab();
	void on_key(int k, bool pressed);
	void reg_obj(int handle);
	void fix_bullets();
	void my_mouse_pos(long& x, long& y);
	unsigned int get_rng(unsigned int maxv);
}
