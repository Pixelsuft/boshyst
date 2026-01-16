#pragma once

extern bool show_menu;

void get_win_size(int& w_buf, int& h_buf);
void get_cursor_pos(int& x_buf, int& y_buf);
void ui_register_rand(int maxval, int ret);

namespace ui {
	void pre_update();
	void draw();
}
