#pragma once

extern bool inited;
extern bool is_hourglass;
extern bool gr_hooked;

void try_to_init();
void init_temp_saves();
void try_to_hook_graphics();
