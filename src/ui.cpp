#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include "ui.hpp"
#include "conf.hpp"
#include "mem.hpp"
#include "fs.hpp"
#include "init.hpp"
#include "input.hpp"
#include "btas.hpp"
#include "utils.hpp"
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
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

extern int last_new_rand_val;
extern bool last_reset;
extern int bullet_id;
extern int bullet_speed;
static int last_scene = 0;
int cur_frames = 0;
int cur_frames2 = 0;
int lock_rng_range = 0;
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
			// Draw cursor dot
			ImDrawList* draw_list = ImGui::GetBackgroundDrawList();
			draw_list->AddCircleFilled(ImVec2((float)x, (float)y), 3.f, IM_COL32(255, 0, 0, 255), 8);
		}
	}
}

static void draw_basic_text() {
	static int ws[2];
	get_win_size(ws[0], ws[1]);
	int scene_id = get_scene_id();
	ObjectHeader* pp = (ObjectHeader*)get_player_ptr(scene_id);
	int inGameFrames = *(int*)(*(size_t*)(mem::get_base() + 0x59a9c) + 0xd0);
	if (pp != nullptr && !is_btas) {
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
	}
	if (0) {
		RunHeader& pState = **(RunHeader**)(mem::get_base() + 0x59a9c);
		ImDrawList* draw_list = ImGui::GetBackgroundDrawList();
		const ImFont* font = ImGui::GetFont();
		for (int i = 0; i < pState.objectCount; i++) {
			ObjectHeader* obj = pState.objectList[i * 2];
			if (!obj)
				continue;
			int cur_x = obj->xPos;
			int cur_y = obj->yPos;
			if (cur_x <= pState.currentViewportX || cur_x >= (pState.currentViewportX + 640) || cur_y <= pState.currentViewportY || cur_y >= (pState.currentViewportY + 480))
				continue;
			char buf[16];
			_itoa((int)i, buf, 10);
			auto pos = ImVec2((float)(cur_x - pState.currentViewportX), (float)(cur_y - pState.currentViewportY));
			auto pos2 = ImVec2((float)(cur_x - pState.currentViewportX + 20), (float)(cur_y - pState.currentViewportY + 10));
			draw_list->AddRectFilled(pos, pos2, IM_COL32(255, 255, 255, 255));
			draw_list->AddText(font, ImGui::GetFontSize(), pos, IM_COL32(255, 0, 0, 255), buf);
		}
	}
	if (is_btas) {
		btas::draw_info();
	}
	else {
		ImGui::Text("Cur Frames: %i", cur_frames);
		ImGui::Text("Cur Frames 2: %i", cur_frames2);
		// ImGui::Text("In-Game Frames: %i", inGameFrames);
		ImGui::Text("Scene: %i (%s)", scene_id, get_scene_name());
	}
	if (conf::draw_cursor && ws[0] != 0 && ws[1] != 0) {
		int x, y;
		get_cursor_pos(x, y);
		ImGui::Text("Cursor Pos: (%i, %i)", x * 640 / ws[0], y * 480 / ws[1]);
		get_cursor_pos_orig(x, y);
		ImGui::Text("Orig Cursor Pos: (%i, %i)", x * 640 / ws[0], y * 480 / ws[1]);
	}
}

void ui::pre_update() {;
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
	ImGui::SetNextWindowSize(ImVec2(440.f, 400.f), ImGuiCond_Once);
	ImGui::SetNextWindowFocus();
	if (ImGui::Begin("Boshyst Menu", nullptr, ImGuiWindowFlags_NoSavedSettings)) {
		static std::string conf_path = get_config_path();
		if (conf::first_run) {
			ImGui::Text("First run info:");
			ImGui::Text("Use 'Insert' key to toggle this menu");
			ImGui::Text("You can edit config at \"%s\"", conf_path.c_str());
		}
		if (is_btas)
			btas::draw_tab();
		if (ImGui::CollapsingHeader("Game Info")) {
			draw_basic_text();
		}
		if (ImGui::CollapsingHeader("Visual")) {
			if (ImGui::SliderFloat("Font scale", &conf::font_scale, 0.01f, 10.f))
				conf::font_scale = mclamp(conf::font_scale, 0.01f, 10.f);
			ImGui::Checkbox("Pixel filter", &conf::pixel_filter);
			ImGui::Checkbox("No viewport", &conf::no_vp);
			ImGui::Checkbox("No perspective", &conf::no_ps);
			ImGui::Checkbox("No shaders", &conf::no_sh);
			ImGui::Checkbox("No transitions", &conf::no_trans);
			if (ImGui::InputInt("Show hitbox level (BETA)", &conf::hitbox_level))
				conf::hitbox_level = std::max(conf::hitbox_level, 0);
		}
		if (!is_btas && ImGui::CollapsingHeader("Gameplay")) {
			ImGui::Checkbox("God mode", &conf::god);
			ImGui::Checkbox("Teleport with mouse", &conf::tp_on_click);
			/*
			pState->rhNextFrame
			1 - next
			2 - prev
			3 - set (pState->rhNextFrameData = need_frame | 0x8000
			4 - reset game
			5 - reset audio
			7/9 - load state sus
			*/
			RunHeader* pState = *(RunHeader**)(mem::get_base() + 0x59a9c);
			static int next_scene_id = 0;
			static int prev_sc_id = 0;
			if (prev_sc_id != get_scene_id())
				next_scene_id = prev_sc_id = get_scene_id();
			if (ImGui::Button("Previous scene") && prev_sc_id != 0) {
				if (prev_sc_id == 55) {
					pState->rhNextFrame = 3;
					pState->rhNextFrameData = 53 | 0x8000;
				}
				else
					pState->rhNextFrame = 2;
			}
			if (ImGui::Button("Next scene") && prev_sc_id != 60) {
				if (prev_sc_id == 53) {
					pState->rhNextFrame = 3;
					pState->rhNextFrameData = 55 | 0x8000;
				}
				else
					pState->rhNextFrame = 1;
			}
			ImGui::InputInt("Next scene ID", &next_scene_id);
			next_scene_id = mclamp(next_scene_id, 0, 60);
			if (next_scene_id == 54)
				next_scene_id = 55;
			if (ImGui::Button("Set scene")) {
				pState->rhNextFrame = 3;
				pState->rhNextFrameData = next_scene_id | 0x8000;
			}
			if (ImGui::Button("Reset game"))
				pState->rhNextFrame = 4;
			if (ImGui::Button("Reset audio"))
				pState->rhNextFrame = 5;
			ImGui::Text("Be careful here!");
			if (ImGui::InputInt("Bullet object ID", &bullet_id))
				bullet_id = std::max(bullet_id, 0);
			if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
				bullet_id = 106;
			if (ImGui::InputInt("Bullet speed", &bullet_speed))
				bullet_speed = std::max(bullet_speed, 0);
			if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
				bullet_speed = 70;
		}
		if (ImGui::CollapsingHeader("Random")) {
			ImGui::Text("Last rand() value: %i / %i", last_new_rand_val, (int)RAND_MAX);
			if (!is_btas) {
				ImGui::Checkbox("Fixed MMF2_Random() value %", &fix_rng);
				if (ImGui::SliderFloat("Value##MMF2_Random()", &fix_rng_val, 0.f, 100.f))
					fix_rng_val = mclamp(fix_rng_val, 0.f, 100.f);
				if (ImGui::InputInt("Only specific range", &lock_rng_range))
					lock_rng_range = mclamp(lock_rng_range, 0, 65535);
			}
			if (ImGui::Checkbox("Log MMF2_Random() map", &log_rng) && !log_rng)
				rng_map.clear();
			if (log_rng) {
				ImGui::Text("Max range: Value");
				for (auto it = rng_map.begin(); it != rng_map.end(); it++)
					ImGui::Text("%i: %i", it->first, it->second);
			}
		}
		if (!is_btas && ImGui::CollapsingHeader("State")) {
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
			if (ImGui::Checkbox("Allow render", &conf::allow_render) && !conf::allow_render && capturing)
				conf::allow_render = true;
			if (!capturing)
				ImGui::Checkbox("Use Direct3D9 render", &conf::direct_render);
			ImGui::Checkbox("Old render (BitBlt)", &conf::old_rec);
			if (conf::allow_render && !capturing && ImGui::Button("Start recording")) {
				conf::cap_start = conf::cap_cnt = 0; // Hack
				SetWindowTextA(hwnd, "I Wanna Be The Boshy R");
			}
			if (conf::allow_render && capturing && ImGui::Button("Stop recording")) {
				SetWindowTextA(hwnd, "I Wanna Be The Boshy S");
			}
			if (conf::cap_au && !conf::no_au && ImGui::Button("Stop audio capture")) {
				on_audio_destroy();
			}
		}
		if (ImGui::CollapsingHeader("Info")) {
			ImGui::Text("Created by Pixelsuft");
			ImGui::Text("Config path: %s", conf_path.c_str());
			// FIXME: broken with savestates
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
	ImGuiIO& io = ImGui::GetIO();
	io.FontGlobalScale = conf::font_scale;
	if (is_btas) {
		if (!is_hourglass)
			ui_menu_draw();
	}
	else {
		int scene_id = get_scene_id();
		if (scene_id != last_scene) {
			last_scene = scene_id;
			cur_frames2 = 0;
			// cout << "New scene (" << scene_id << ")" << std::endl;
		}
		if (last_reset) {
			cur_frames = -1;
			// cout << "Scene reset (" << scene_id << ")" << std::endl;
		}
		cur_frames++;
		cur_frames2++;
		if (!conf::tas_mode) {
			last_reset = false;
			ui_menu_draw();
			post_draw();
			return;
		}
	}
	if (conf::tas_no_info)
		return;
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
	ImGui::SetNextWindowPos(ImVec2((float)conf::pos[0], (float)conf::pos[1]), ImGuiCond_Once);
	ImGui::SetNextWindowSize(ImVec2((float)conf::size[0], (float)conf::size[1]), ImGuiCond_Once);
	auto flags = ImGuiWindowFlags_NoTitleBar | (is_btas ? 0 : (ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoInputs)) |
		ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings;
	if (ImGui::Begin("Boshyst Info", nullptr, flags))
		draw_basic_text();
	ImGui::PopStyleVar();
	ImGui::End();
	last_reset = false;
	post_draw();
}
