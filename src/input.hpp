#pragma once
#include <vector>
#include <map>

struct InputEvent {
	float x;
	float y;
};

void input_tick();
void input_init();
