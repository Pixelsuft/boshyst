#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include "ui.hpp"
#include "conf.hpp"
#include "rec.hpp"
#include "mem.hpp"
#include <imgui.h>
#include <iostream>
#include <cstdint>

using std::cout;

namespace conf {
	extern int cap_start;
	extern int cap_cnt;
}

extern HWND hwnd;
extern int last_rng_val;

static void* get_scene_ptr() {
	const size_t offsets[] = { 0x59A94 + 0x400000, 0x268, 0 };
	return mem::ptr_from_offsets(offsets, sizeof(offsets) / 4);
}

static int get_scene_id() {
	const size_t offsets[] = { 0x59A94 + 0x400000, 0x268, 0xA8 };
	return *(int*)mem::ptr_from_offsets(offsets, sizeof(offsets) / 4);
}

static void* get_player_ptr(int s) {
	// Tutorial
	if (s == 36) {
		const size_t offsets[] = { mem::get_base("Lacewing.mfx") + 0x2D680, 0x208, 0x1C, 0xF0, 0 };
		return mem::ptr_from_offsets(offsets, sizeof(offsets) / 4);
	}
	// W2
	if (s == 5) {
		const size_t offsets[] = { mem::get_base("Lacewing.mfx") + 0x2D680, 0x208, 0x1C, 0x3B0, 0 };
		return mem::ptr_from_offsets(offsets, sizeof(offsets) / 4);
	}
	// MB1
	if (s == 6) {
		const size_t offsets[] = { mem::get_base("Lacewing.mfx") + 0x2D680, 0x208, 0x1C, 0xD8, 0 };
		return mem::ptr_from_offsets(offsets, sizeof(offsets) / 4);
	}
	// B2, MARIO SECRET
	if (s == 7 || s == 33) {
		const size_t offsets[] = { mem::get_base("Lacewing.mfx") + 0x2D680, 0x208, 0x1C, 0xC8, 0 };
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
	// B9
	if (s == 59) {
		const size_t offsets[] = { mem::get_base("Lacewing.mfx") + 0x2D680, 0x208, 0x1C, 0x28, 0 };
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
	// KAPPA
	if (s == 37) {
		const size_t offsets[] = { mem::get_base("Lacewing.mfx") + 0x2D680, 0x208, 0x1C, 0x110, 0 };
		return mem::ptr_from_offsets(offsets, sizeof(offsets) / 4);
	}
	// FINAL PATH
	if (s == 38) {
		const size_t offsets[] = { mem::get_base("Lacewing.mfx") + 0x2D680, 0x208, 0x1C, 0xB8, 0 };
		return mem::ptr_from_offsets(offsets, sizeof(offsets) / 4);
	}
	// FB
	if (s == 46) {
		const size_t offsets[] = { mem::get_base("Lacewing.mfx") + 0x2D680, 0x208, 0x1C, 0xa8, 0 };
		return mem::ptr_from_offsets(offsets, sizeof(offsets) / 4);
	}
	// PRIZE ROOM
	if (s == 49) {
		const size_t offsets[] = { mem::get_base("Lacewing.mfx") + 0x2D680, 0x208, 0x1C, 0x140, 0 };
		return mem::ptr_from_offsets(offsets, sizeof(offsets) / 4);
	}
	// BLIZZARD
	if (s == 53) {
		const size_t offsets[] = { mem::get_base("Lacewing.mfx") + 0x2D680, 0x208, 0x1C, 0xE8, 0 };
		return mem::ptr_from_offsets(offsets, sizeof(offsets) / 4);
	}
	// W1, CHEETAH
	if (s == 3 || s == 54) {
		const size_t offsets[] = { mem::get_base("Lacewing.mfx") + 0x2D680, 0x208, 0x1C, 0xE0, 0 };
		return mem::ptr_from_offsets(offsets, sizeof(offsets) / 4);
	}
	// GASTLY
	if (s == 9) {
		const size_t offsets[] = { mem::get_base("Lacewing.mfx") + 0x2D680, 0x208, 0x1C, 0x130, 0 };
		return mem::ptr_from_offsets(offsets, sizeof(offsets) / 4);
	}
	// POKEWORLD
	if (s == 50) {
		const size_t offsets[] = { mem::get_base("Lacewing.mfx") + 0x2D680, 0x208, 0x1C, 0x78, 0 };
		return mem::ptr_from_offsets(offsets, sizeof(offsets) / 4);
	}
	// METAL GEAR
	if (s == 23) {
		const size_t offsets[] = { mem::get_base("Lacewing.mfx") + 0x2D680, 0x208, 0x1C, 0x418, 0 };
		return mem::ptr_from_offsets(offsets, sizeof(offsets) / 4);
	}
	// ELEVATOR
	if (s == 51) {
		const size_t offsets[] = { mem::get_base("Lacewing.mfx") + 0x2D680, 0x208, 0x1C, 0x138, 0 };
		return mem::ptr_from_offsets(offsets, sizeof(offsets) / 4);
	}
	return nullptr;
}

void ui::draw() {
	conf::cur_mouse_checked = false;
	if (conf::allow_render) {
		static int cur_total = 0;
		static int cur_cnt = 0;
		cur_total++;
		if (cur_total == conf::cap_start) {
			rec::init();
			rec::cap();
			cur_cnt++;
		}
		else if (cur_cnt > 0) {
			rec::cap();
			cur_cnt++;
			if (cur_cnt == conf::cap_cnt) {
				rec::stop();
				cur_cnt = 0;
			}
		}
	}
	if (!conf::menu)
		return;
	int ws[2];
	get_win_size(ws[0], ws[1]);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
	ImGui::SetNextWindowPos(ImVec2((float)conf::pos[0], (float)conf::pos[1]));
	ImGui::SetNextWindowSize(ImVec2((float)conf::size[0], (float)conf::size[1]));
	if (ImGui::Begin("Boshyst", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoSavedSettings)) {
		// ImGui::Text("Boshyst by Pixelsuft");
		static int last_scene = 0;
		static int cur_frames = 0;
		int scene_id = get_scene_id();
		if (scene_id != last_scene) {
			last_scene = scene_id;
			cur_frames = 0;
		}
		cur_frames++;
		uint8_t* pp = (uint8_t*)get_player_ptr(scene_id);
		ImGui::Text("Cur Frames: %i", cur_frames);
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
}
