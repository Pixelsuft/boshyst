#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include "ui.hpp"
#include "conf.hpp"
#include "mem.hpp"
#include "fs.hpp"
#include <imgui.h>
#include <iostream>
#include <cstdint>

using std::cout;

extern HWND hwnd;
extern int last_new_rand_val;

extern int get_scene_id();
extern void* get_player_ptr(int s);
extern SHORT(__stdcall* GetKeyStateOrig)(int k);
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
bool show_menu = true;

static bool MyKeyState(int k) {
	return GetForegroundWindow() == hwnd && (GetKeyStateOrig(k) & 128);
}

static void draw_basic_text() {
	int ws[2];
	get_win_size(ws[0], ws[1]);
	int scene_id = get_scene_id();
	uint8_t* pp = (uint8_t*)get_player_ptr(scene_id);
	ImGui::Text("Cur Frames: %i", cur_frames);
	ImGui::Text("Cur Frames 2: %i", cur_frames2);
	if (pp != nullptr) {
		static int last_x = 0;
		static int last_y = 0;
		int cur_x = *(int*)(pp + 0x4C);
		int cur_y = *(int*)(pp + 0x54);
		ImGui::Text("Pos: (%i, %i)", cur_x, cur_y);
		ImGui::Text("Delta: (%i, %i)", cur_x - last_x, cur_y - last_y);
		last_x = cur_x;
		last_y = cur_y;
	}
	ImGui::Text("Scene ID: %i", scene_id);
	if (conf::draw_cursor) {
		int x, y;
		get_cursor_pos(x, y);
		ImGui::Text("Cursor Pos: (%i, %i)", x * 640 / ws[0], y * 480 / ws[1]);
		get_cursor_pos_orig(x, y);
		ImGui::Text("Orig Cursor Pos: (%i, %i)", x * 640 / ws[0], y * 480 / ws[1]);
	}
}

void ui::pre_update() {
	static bool holds_insert = false;
	if (MyKeyState(conf::menu_hotkey)) {
		if (!holds_insert) {
			holds_insert = true;
			show_menu = !show_menu;
		}
	}
	else
		holds_insert = false;
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
	if (!show_menu)
		return;
	static bool holds_lmb = false;
	if (MyKeyState(VK_LBUTTON)) {
		if (!holds_lmb) {
			holds_lmb = true;
			ImGui_ImplWin32_WndProcHandler(hwnd, WM_LBUTTONDOWN, 0, 0);
		}
	}
	else if (holds_lmb) {
		holds_lmb = false;
		ImGui_ImplWin32_WndProcHandler(hwnd, WM_LBUTTONUP, 0, 0);
	}
	ImGui::SetNextWindowFocus();
	if (ImGui::Begin("Boshyst Menu")) {
		if (conf::first_run) {
			static std::string conf_path = get_config_path();
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
		}
		if (ImGui::CollapsingHeader("Gameplay")) {
			ImGui::Checkbox("God mode", &conf::god);
		}
		if (ImGui::CollapsingHeader("Random")) {
			ImGui::Text("Last rand() value: %i", last_new_rand_val);
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
		}
		if (ImGui::CollapsingHeader("Info")) {
			ImGui::Text("Created by Pixelsuft");
			// FIXME: roken with savestates
			// if (ImGui::Button("Reload Config")) conf::read();
		}
	}
	ImGui::End();
}

void ui::draw() {
	if (!conf::menu) {
		last_reset = false;
		return;
	}
	if (!conf::tas_mode) {
		last_reset = false;
		ui_menu_draw();
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
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
	ImGui::SetNextWindowPos(ImVec2((float)conf::pos[0], (float)conf::pos[1]));
	ImGui::SetNextWindowSize(ImVec2((float)conf::size[0], (float)conf::size[1]));
	if (ImGui::Begin("Boshyst Info", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoSavedSettings)) {
		cur_frames++;
		cur_frames2++;
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
		if (conf::draw_cursor) {
			int x, y;
			get_cursor_pos(x, y);
			ImDrawList* draw_list = ImGui::GetBackgroundDrawList();
			draw_list->AddCircleFilled(ImVec2((float)x, (float)y), 3.f, IM_COL32(255, 0, 0, 255), 8);
		}
	}
	ImGui::PopStyleVar();
	ImGui::End();
	last_reset = false;
}
