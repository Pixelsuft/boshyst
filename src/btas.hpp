#pragma once

extern bool is_btas;

namespace btas {
	short TasGetKeyState(int k);
	bool on_before_update();
	unsigned long get_time();
	void init();
}
