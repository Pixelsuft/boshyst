#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include "ui.hpp"
#include "conf.hpp"
#include "mem.hpp"
#include "fs.hpp"
#include "input.hpp"
#include "ghidra_headers.h"
#include <imgui.h>
#include <iostream>
#include <unordered_map>
#include <cstdint>

using std::cout;

extern HWND hwnd;
extern int last_new_rand_val;

namespace conf {
	extern int cap_start;
	extern int cap_cnt;
}
extern int get_scene_id();
extern void* get_player_ptr(int s);
extern int JustKeyState(int k);
extern bool MyKeyState(int k);
extern std::string get_config_path();
extern bool state_save(bfs::File* file);
extern bool state_load(bfs::File* file);
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
extern void get_cursor_pos_orig(int& x_buf, int& y_buf);

extern int last_new_rand_val;
extern bool last_reset;
static int last_scene = 0;
static int cur_frames = 0;
static int cur_frames2 = 0;
static int need_save_state = 0;
static bool log_rng = false;
bool fix_rng = false;
float fix_rng_val = 0.f;
std::unordered_map<int, int> rng_map;
bool show_menu = true;

#undef min
#undef max
template<typename T>
inline T mclamp(T v, T minv, T maxv) {
	return std::max(std::min(v, maxv), minv);
}

void ui_register_rand(int maxval, int ret) {
	if (log_rng) {
		rng_map[maxval] = ret;
	}
}

static void post_draw() {
	if (conf::draw_cursor) {
		int x, y;
		get_cursor_pos(x, y);
		if (x >= 0 && y >= 0) {
			ImDrawList* draw_list = ImGui::GetBackgroundDrawList();
			draw_list->AddCircleFilled(ImVec2((float)x, (float)y), 3.f, IM_COL32(255, 0, 0, 255), 8);
		}
	}
}

static void draw_basic_text() {
	if (IsIconic(hwnd))
		return; // FIXME: WTF why it crashes without it
	static int ws[2];
	get_win_size(ws[0], ws[1]);
	int scene_id = get_scene_id();
	ObjectHeader* pp = (ObjectHeader*)get_player_ptr(scene_id);
	int inGameFrames = *(int*)(*(size_t*)(mem::get_base() + 0x59a9c) + 0xd0);
	if (pp != nullptr) {
		static int last_x = 0;
		static int last_y = 0;
		ImGui::Text("Pos: (%i, %i)", pp->xPos, pp->yPos);
		ImGui::Text("Delta: (%i, %i)", pp->xPos - last_x, pp->yPos - last_y);
		ImGui::Text("Align: %i", pp->xPos % 3);
		last_x = pp->xPos;
		last_y = pp->yPos;

		if (0) {
			auto anim = (AnimController*)((int)&pp->handle + pp->animControlleroffset);
			pp->redrawFlag = 1;
			// anim->currentSpeedOrLikelyCounter = 0;
			ImGui::Text("Test anim: %i", anim->currentSpeedOrLikelyCounter);
		}

		if (0) {
			for (size_t i = 0; i < 4000; i += 8) {
				const size_t offsets[] = { mem::get_base("Lacewing.mfx") + 0x2D680, 0x208, 0x1C, i, 0 };
				ObjectHeader* obj = (ObjectHeader*)mem::ptr_from_offsets(offsets, sizeof(offsets) / 4);
				if (!obj)
					break;
				if (std::abs(obj->xPos - pp->xPos) <= 10 && std::abs(obj->yPos - pp->yPos) <= 10) {
					if (ImGui::Button(std::to_string((long long)i).c_str())) {
						obj->xPos = obj->yPos = 10;
						obj->redrawFlag = 1;
					}
				}
			}
		}
	}
	ImGui::Text("Cur Frames: %i", cur_frames);
	ImGui::Text("Cur Frames 2: %i", cur_frames2);
	ImGui::Text("In-Game Frames: %i", inGameFrames);
	ImGui::Text("Scene ID: %i", scene_id);
	if (conf::draw_cursor) {
		int x, y;
		get_cursor_pos(x, y);
		ImGui::Text("Cursor Pos: (%i, %i)", x * 640 / ws[0], y * 480 / ws[1]);
		get_cursor_pos_orig(x, y);
		ImGui::Text("Orig Cursor Pos: (%i, %i)", x * 640 / ws[0], y * 480 / ws[1]);
	}
}

void ui::pre_update() {;
	if (!conf::tas_mode && JustKeyState(conf::menu_hotkey) == 1)
		show_menu = !show_menu;
	if (need_save_state == 1) {
		need_save_state = 0;
		state_save(nullptr);
	}
	else if (need_save_state == 2) {
		need_save_state = 0;
		state_load(nullptr);
	}
}

static void ui_menu_draw() {
	if (!show_menu) {
		post_draw();
		return;
	}
	auto temp_state = JustKeyState(VK_LBUTTON);
	if (temp_state == 1)
		ImGui_ImplWin32_WndProcHandler(hwnd, WM_LBUTTONDOWN, 0, 0);
	else if (temp_state == -1)
		ImGui_ImplWin32_WndProcHandler(hwnd, WM_LBUTTONUP, 0, 0);
	for (int i = 0; i < sizeof(keys_to_check) / sizeof(int); i++) {
		int k = keys_to_check[i];
		temp_state = JustKeyState(k);
		if (temp_state == 1) {
			ImGui_ImplWin32_WndProcHandler(hwnd, WM_KEYDOWN, k, 0);
			if (k >= '0' && k <= '9')
				ImGui_ImplWin32_WndProcHandler(hwnd, WM_CHAR, k, 0);
			else if (k == VK_DECIMAL || k == VK_OEM_COMMA || k == VK_OEM_PERIOD)
				ImGui_ImplWin32_WndProcHandler(hwnd, WM_CHAR, '.', 0);
		}
		else if (temp_state == -1)
			ImGui_ImplWin32_WndProcHandler(hwnd, WM_KEYUP, k, 0);
	}
	ImGui::SetNextWindowSize(ImVec2(400.f, 400.f), ImGuiCond_Once);
	ImGui::SetNextWindowFocus();
	if (ImGui::Begin("Boshyst Menu", nullptr, ImGuiWindowFlags_NoSavedSettings)) {
		static std::string conf_path = get_config_path();
		if (conf::first_run) {
			ImGui::Text("First run info:");
			ImGui::Text("Use 'Insert' key to toggle this menu");
			ImGui::Text("You can edit config at \"%s\"", conf_path.c_str());
		}
		if (ImGui::CollapsingHeader("Game Info")) {
			draw_basic_text();
		}
		if (ImGui::CollapsingHeader("Visual")) {
			ImGui::Checkbox("No viewport", &conf::no_vp);
			ImGui::Checkbox("No shaders", &conf::no_sh);
			ImGui::Checkbox("No transitions", &conf::no_trans);
		}
		if (ImGui::CollapsingHeader("Gameplay")) {
			ImGui::Checkbox("God mode", &conf::god);
			ImGui::Checkbox("Teleport with mouse (BETA)", &conf::tp_on_click);
		}
		if (ImGui::CollapsingHeader("Random")) {
			ImGui::Text("Last rand() value: %i", last_new_rand_val);
			ImGui::Checkbox("Fixed MMF2_Random() value %", &fix_rng);
			if (ImGui::SliderFloat("Value##MMF2_Random()", &fix_rng_val, 0.f, 100.f)) {
				fix_rng_val = mclamp(fix_rng_val, 0.f, 100.f);
			}
			if (ImGui::Checkbox("Log MMF2_Random() map", &log_rng)) {
				if (!log_rng)
					rng_map.clear();
			}
			if (log_rng) {
				ImGui::Text("Max range: Value");
				for (auto it = rng_map.begin(); it != rng_map.end(); it++)
					ImGui::Text("%i: %i", it->first, it->second);
			}
		}
		if (ImGui::CollapsingHeader("State")) {
			if (ImGui::Button("Save"))
				need_save_state = 1;
			ImGui::SameLine();
			if (ImGui::Button("Load"))
				need_save_state = 2;
		}
		if (ImGui::CollapsingHeader("System")) {
			ImGui::Checkbox("Keep save", &conf::keep_save);
			ImGui::Checkbox("No mouse move", &conf::no_cmove);
			ImGui::Checkbox("Draw cursor", &conf::draw_cursor);
			ImGui::Checkbox("Simulate mouse", &conf::emu_mouse);
			ImGui::Checkbox("Skip message boxes", &conf::skip_msg);
			ImGui::Checkbox("Allow in-game keyboard in menu", &conf::input_in_menu);
		}
		if (ImGui::CollapsingHeader("Recording")) {
			ImGui::Checkbox("Allow render", &conf::allow_render);
			ImGui::Checkbox("Use Direct3D9 render", &conf::direct_render);
			ImGui::Checkbox("Fix white screen (Direct3D9)", &conf::fix_white_render);
			ImGui::Checkbox("Old render (BitBlt)", &conf::old_rec);
			if (ImGui::Button("Start recording")) {
				conf::cap_start = conf::cap_cnt = 0; // Hack
				SetWindowTextA(hwnd, "I Wanna Be The Boshy R");
			}
			if (ImGui::Button("Stop recording")) {
				SetWindowTextA(hwnd, "I Wanna Be The Boshy S");
			}
		}
		if (ImGui::CollapsingHeader("Info")) {
			ImGui::Text("Created by Pixelsuft");
			ImGui::Text("Config path: %s", conf_path.c_str());
			// FIXME: roken with savestates
			// if (ImGui::Button("Reload Config")) conf::read();
		}
	}
	ImGui::End();
	post_draw();
}

void ui::draw() {
	if (!conf::menu) {
		last_reset = false;
		return;
	}
	int scene_id = get_scene_id();
	if (scene_id != last_scene) {
		last_scene = scene_id;
		cur_frames2 = 0;
		cout << "New scene (" << scene_id << ")" << std::endl;
	}
	if (last_reset) {
		cur_frames = -1;
		cout << "Scene reset (" << scene_id << ")" << std::endl;
	}
	cur_frames++;
	cur_frames2++;
	if (!conf::tas_mode) {
		last_reset = false;
		ui_menu_draw();
		return;
	}
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
	ImGui::SetNextWindowPos(ImVec2((float)conf::pos[0], (float)conf::pos[1]));
	ImGui::SetNextWindowSize(ImVec2((float)conf::size[0], (float)conf::size[1]));
	if (ImGui::Begin("Boshyst Info", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoSavedSettings)) {
		draw_basic_text();
		if (0) {
			// Display all object IDs
			uint8_t* pp;
			ImDrawList* draw_list = ImGui::GetBackgroundDrawList();
			const ImFont* font = ImGui::GetFont();
			for (size_t i = 0; i < 2000; i += 8) {
				const size_t offsets[] = { mem::get_base("Lacewing.mfx") + 0x2D680, 0x208, 0x1C, i, 0 };
				pp = (uint8_t*)mem::ptr_from_offsets(offsets, sizeof(offsets) / 4);
				if (!pp)
					break;
				int cur_x = *(int*)(pp + 0x4C);
				int cur_y = *(int*)(pp + 0x54);
				if (cur_x <= 0 || cur_x >= 4257664 || cur_y <= 0 || cur_y >= 4257664)
					continue;
				char buf[16];
				_itoa((int)i, buf, 10);
				draw_list->AddRectFilled(ImVec2((float)(cur_x % 640), (float)(cur_y % 480)), ImVec2((float)(cur_x % 640 + 20), (float)(cur_y % 480 + 10)), IM_COL32(255, 255, 255, 255));
				draw_list->AddText(font, ImGui::GetFontSize(), ImVec2((float)(cur_x % 640), (float)(cur_y % 480)), IM_COL32(255, 0, 0, 255), buf);
			}
		}
	}
	ImGui::PopStyleVar();
	ImGui::End();
	last_reset = false;
	post_draw();
}
