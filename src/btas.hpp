#pragma once

extern bool is_btas;
extern unsigned long tas_time;

namespace btas {
	short TasGetKeyState(int k);
	void on_before_update();
	void init();
}
