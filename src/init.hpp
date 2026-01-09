#pragma once

extern bool inited;
extern bool is_hourglass;
extern bool gr_hooked;

bool fix_win32_theme();
void audio_init();
void try_to_init();
void init_temp_saves();
void try_to_hook_graphics();
