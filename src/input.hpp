#pragma once
#include <vector>
#include <map>

static const int keys_to_check[] = {
    VK_LCONTROL, VK_RCONTROL, VK_RETURN, '0', '1', '2',
    '3', '4', '5', '6', '7', '8', '9', VK_DECIMAL, VK_OEM_COMMA, VK_OEM_PERIOD,
    VK_LEFT, VK_RIGHT, VK_UP, VK_DOWN, VK_LSHIFT, VK_RSHIFT, VK_BACK, VK_SPACE, VK_DELETE
};

struct InputEvent {
	float x;
	float y;
};

void input_tick();
void input_init();
