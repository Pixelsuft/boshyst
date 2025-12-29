#pragma once

extern bool inited;
extern bool is_hourglass;
extern bool is_btas;
extern bool gr_hooked;

void try_to_init();
void try_to_hook_graphics();
