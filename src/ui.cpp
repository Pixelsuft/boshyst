#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include "ui.hpp"
#include "conf.hpp"
#include "mem.hpp"
#include <imgui.h>
#include <iostream>
#include <cstdint>

using std::cout;

extern HWND hwnd;
extern int last_rng_val;

extern int get_scene_id();
extern void* get_player_ptr(int s);

extern bool last_reset;
int last_scene = 0;

void ui::pre_update() {

}

void ui::draw() {
	if (!conf::menu) {
		last_reset = false;
		return;
	}
	int ws[2];
	get_win_size(ws[0], ws[1]);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
	ImGui::SetNextWindowPos(ImVec2((float)conf::pos[0], (float)conf::pos[1]));
	ImGui::SetNextWindowSize(ImVec2((float)conf::size[0], (float)conf::size[1]));
	if (ImGui::Begin("Boshyst", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoSavedSettings)) {
		// ImGui::Text("Boshyst by Pixelsuft");
		static int cur_frames = 0;
		static int cur_frames2 = 0;
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
		uint8_t* pp = (uint8_t*)get_player_ptr(scene_id);
		ImGui::Text("Cur Frames: %i", cur_frames);
		ImGui::Text("Cur Frames 2: %i", cur_frames2);
		if (pp != nullptr) {
			static int last_x = 0;
			static int last_y = 0;
			int cur_x = *(int*)(pp + 0x4C);
			int cur_y = *(int*)(pp + 0x54);
			// ImGui::Text("Pointer: %p", pp);
			// ImGui::Text("Pointer 2: %p", get_scene_ptr());
			ImGui::Text("Pos: (%i, %i)", cur_x, cur_y);
			ImGui::Text("Delta: (%i, %i)", cur_x - last_x, cur_y - last_y);
			last_x = cur_x;
			last_y = cur_y;
			float tx = (float)(cur_x % 640) * (float)ws[0] / 640.f;
			float ty = (float)(cur_y % 480) * (float)ws[1] / 480.f;
			ImDrawList* draw_list = ImGui::GetBackgroundDrawList();
			// draw_list->AddCircleFilled(ImVec2(tx, ty), 5.f, IM_COL32(255, 0, 0, 255), 12);
		}
		ImGui::Text("Scene ID: %i", scene_id);
		if (conf::draw_cursor) {
			ImDrawList* draw_list = ImGui::GetBackgroundDrawList();
			int x, y;
			get_cursor_pos(x, y);
			ImGui::Text("Cursor Pos: (%i, %i)", x, y);
			// cout << x << " " << y << std::endl;
			draw_list->AddCircleFilled(ImVec2((float)x, (float)y), 3.f, IM_COL32(255, 0, 0, 255), 8);
		}
		if (0) {
			// Display all object IDs
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
				// cout << i << ": " << cur_x << ", " << cur_y << std::endl;
				char buf[100];
				// sprintf(buf, "%i, %i", cur_x, cur_y);
				_itoa((int)i, buf, 10);
				draw_list->AddRectFilled(ImVec2((float)(cur_x % 640), (float)(cur_y % 480)), ImVec2((float)(cur_x % 640 + 20), (float)(cur_y % 480 + 10)), IM_COL32(255, 255, 255, 255));
				draw_list->AddText(font, ImGui::GetFontSize(), ImVec2((float)(cur_x % 640), (float)(cur_y % 480)), IM_COL32(255, 0, 0, 255), buf);
				// draw_list->AddCircleFilled(ImVec2((float)(cur_x % 640), (float)(cur_y % 480)), 2.f, IM_COL32(255, 0, 0, 255), 4);
			}
		}
		// ImGui::Text("Last new RNG value: %i", last_rng_val);
	}
	ImGui::PopStyleVar();
	ImGui::End();
	last_reset = false;
}
