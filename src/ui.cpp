#include "ui.hpp"
#include "conf.hpp"
#include "mem.hpp"
#include <imgui.h>
#include <iostream>
#include <cstdint>

using std::cout;

extern int last_rng_val;

void* get_scene_ptr() {
	const size_t offsets[] = { 0x59A94 + 0x400000, 0x268, 0 };
	return mem::ptr_from_offsets(offsets, sizeof(offsets) / 4);
}

int get_scene_id() {
	const size_t offsets[] = { 0x59A94 + 0x400000, 0x268, 0xA8 };
	return *(int*)mem::ptr_from_offsets(offsets, sizeof(offsets) / 4);
}

void* get_player_ptr(int s) {
	// W1, Tutorial, FB
	if (s == 3 || s == 36 || s == 46) {
		const size_t offsets[] = { 0x400000 + 0x00059A1C, 0x3FC, 0x28, 0, 0x7E8, 0x8D0, 0x80, 0 };
		return mem::ptr_from_offsets(offsets, sizeof(offsets) / 4);
	}
	// W2
	if (s == 5) {
		const size_t offsets[] = { 0x400000 + 0x59A9C, 0xA4, 0x7C0, 0xD0, 0x234, 0 };
		return mem::ptr_from_offsets(offsets, sizeof(offsets) / 4);
	}
	// W3, B8
	if (s == 8 || s == 26) {
		const size_t offsets[] = { mem::get_base("Lacewing.mfx") + 0x2D680, 0x208, 0x1C, 0x48, 0 };
		return mem::ptr_from_offsets(offsets, sizeof(offsets) / 4);
	}
	// W4-8, W11, B1, B3, B4, B5, B6, B7, B10, TELEPROOM
	if (s == 11 || s == 13 || s == 15 || s == 22 || s == 25 || s == 35 || s == 4 || s == 10 ||
		s == 12 || s == 14 || s == 17 || s == 24 || s == 32 || s == 29) {
		const size_t offsets[] = { mem::get_base("Lacewing.mfx") + 0x2D680, 0x208, 0x1C, 0x38, 0 };
		return mem::ptr_from_offsets(offsets, sizeof(offsets) / 4);
	}
	// W9
	if (s == 27) {
		const size_t offsets[] = { mem::get_base("Lacewing.mfx") + 0x2D680, 0x208, 0x1C, 0x88, 0 };
		return mem::ptr_from_offsets(offsets, sizeof(offsets) / 4);
	}
	// W10
	if (s == 31) {
		const size_t offsets[] = { mem::get_base("Lacewing.mfx") + 0x2D680, 0x234, 0x208, 0x1C, 0x68, 0 };
		return mem::ptr_from_offsets(offsets, sizeof(offsets) / 4);
	}
	// W11-cobrat
	if (s == 35) {
		const size_t offsets[] = { mem::get_base("Lacewing.mfx") + 0x2D680, 0x208, 0x1C, 0x480, 0xC, 0 };
		return mem::ptr_from_offsets(offsets, sizeof(offsets) / 4);
	}
	return nullptr;
}

void ui::draw() {
	if (!conf::menu)
		return;
	int ws[2];
	get_win_size(ws[0], ws[1]);
	ImGui::SetNextWindowPos(ImVec2((float)conf::pos[0], (float)conf::pos[1]));
	ImGui::SetNextWindowSize(ImVec2((float)conf::size[0], (float)conf::size[1]));
	if (ImGui::Begin("Boshyst", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoSavedSettings)) {
		// ImGui::Text("Boshyst by Pixelsuft");
		int scene_id = get_scene_id();
		uint8_t* pp = (uint8_t*)get_player_ptr(scene_id);
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
		ImGui::Text("Last new RNG value: %i", last_rng_val);
	}
	ImGui::End();
}
