#pragma once
#include <string>

extern bool is_btas;

namespace btas {
	short TasGetKeyState(int k);
	bool on_before_update();
	void on_after_update();
	unsigned long get_time();
	void read_setting(const std::string& line, const std::string& line_orig);
	void init();
	void draw_info();
	void draw_tab();
	void on_key(int k, bool pressed);
}
