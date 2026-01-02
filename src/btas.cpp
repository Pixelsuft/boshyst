#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <iostream>
#include <cstdio>
#include <imgui.h>
#include "conf.hpp"
#include "ass.hpp"
#include "init.hpp"
#include "fs.hpp"
#include "utils.hpp"
#include "btas.hpp"
#include "ui.hpp"
#include "mem.hpp"
#include "ghidra_headers.h"
#include <vector>
#include <algorithm>
#include <map>
#undef max
#undef min

using std::cout;
using std::string;

extern HWND hwnd;
extern SHORT(__stdcall* GetAsyncKeyStateOrig)(int k);
extern DWORD(__stdcall* timeGetTimeOrig)();
extern bool state_save(bfs::File* file);
extern bool state_load(bfs::File* file);
static UINT (__stdcall *pTimeBeginPeriod)(UINT uPeriod);
static UINT (__stdcall *pTimeEndPeriod)(UINT uPeriod);

struct BTasBind {
	union {
		struct {
			int k;
			int down_event;
		} mapper;
		struct {
			int slot;
		} state;
	};
	int key;
	int mod;
	uint8_t idx;
	bool down;

	BTasBind() {
		mapper.k = 0;
		mapper.down_event = -1;
		key = 0;
		mod = 0;
		idx = 0;
		down = false;
	}
};

struct BTasEvent {
	union {
		struct {
			int x;
			int y;
		} click;
		struct {
			int k;
		} key;
	};
	int frame;
	uint8_t idx;

	BTasEvent() {
		key.k = 0;
		frame = 0;
		idx = 0;
	}
};

struct BTasState {
	int scene;
	int frame;
	int sc_frame;
	int total;
	std::vector<BTasEvent> ev;

	BTasState() {
		frame = sc_frame = 0;
		scene = 0;
		total = 0;
	}
};

static std::vector<BTasBind> binds;
static std::vector<int> holding;
static string last_msg;
static BTasState st;
static DWORD last_time = 0;
static DWORD now = 0;
static unsigned long cur_time = 0;
static bool next_step = false;
static bool slowmo = false;

bool is_btas = false;
bool fast_forward = false;
bool is_paused = true;
// TODO: check it everywhere
bool is_replay = false;

void btas::read_setting(const string& line, const string& line_orig) {
	BTasBind bind;
	if (starts_with(line, "btas=map,")) {
		bind.idx = 0;
		ASS(sscanf(line.substr(9).c_str(), "%i,%i,%i", &bind.key, &bind.mod, &bind.mapper.k) == 3);
	}
	else if (starts_with(line, "btas=toggle_pause,")) {
		bind.idx = 1;
		ASS(sscanf(line.substr(18).c_str(), "%i,%i", &bind.key, &bind.mod) == 2);
	}
	else if (starts_with(line, "btas=toggle_fastforward,")) {
		bind.idx = 2;
		ASS(sscanf(line.substr(24).c_str(), "%i,%i", &bind.key, &bind.mod) == 2);
	}
	else if (starts_with(line, "btas=fastforward,")) {
		bind.idx = 3;
		ASS(sscanf(line.substr(17).c_str(), "%i,%i", &bind.key, &bind.mod) == 2);
	}
	else if (starts_with(line, "btas=step,")) {
		bind.idx = 4;
		ASS(sscanf(line.substr(10).c_str(), "%i,%i", &bind.key, &bind.mod) == 2);
	}
	else if (starts_with(line, "btas=slowmotion,")) {
		bind.idx = 5;
		ASS(sscanf(line.substr(16).c_str(), "%i,%i", &bind.key, &bind.mod) == 2);
	}
	else if (starts_with(line, "btas=save_state,")) {
		bind.idx = 6;
		ASS(sscanf(line.substr(16).c_str(), "%i,%i,%i", &bind.key, &bind.mod, &bind.state.slot) == 3);
	}
	else if (starts_with(line, "btas=load_state,")) {
		bind.idx = 7;
		ASS(sscanf(line.substr(16).c_str(), "%i,%i,%i", &bind.key, &bind.mod, &bind.state.slot) == 3);
	}
	else {
		ass::show_err((string("Unknown BTAS setting: ") + line_orig).c_str());
		ASS(false);
	}
	binds.push_back(bind);
}

void btas::init() {
	cout << "btas init\n";
	std::sort(binds.begin(), binds.end(), [](BTasBind a, BTasBind b) {
		return a.key > b.key;
	});
	cur_time = 0;
	next_step = slowmo = false;
	auto h = GetModuleHandleW(L"winmm.dll");
	ASS(h != nullptr);
	pTimeBeginPeriod = (decltype(pTimeBeginPeriod))GetProcAddress(h, "timeBeginPeriod");
	pTimeEndPeriod = (decltype(pTimeEndPeriod))GetProcAddress(h, "timeEndPeriod");
	ASS(pTimeBeginPeriod != nullptr && pTimeEndPeriod != nullptr);
	last_msg = "None";
	last_time = now = timeGetTimeOrig();
}

template<typename T>
static void write_bin(bfs::File& f, const std::vector<T>& data) {
	size_t size = data.size();
	ASS(f.write(&size, sizeof(size_t)));
	ASS(f.write(data.data(), size * sizeof(T)));
}

template<typename T>
static void write_bin(bfs::File& f, T data) {
	ASS(f.write(&data, sizeof(T)));
}

template<typename T>
static void load_bin(bfs::File& f, std::vector<T>& data) {
	size_t size;
	ASS(f.read(&size, sizeof(size_t)));
	data.resize(size);
	ASS(f.read(&data[0], size * sizeof(T)));
}

template<typename T>
static void load_bin(bfs::File& f, T& data) {
	ASS(f.read(&data, sizeof(T)));
}

static void b_state_save(int slot) {
	string path = string("state") + std::to_string((long long)slot) + ".bstate";
	bfs::File f(path, 1);
	if (!f.is_open()) {
		last_msg = "Failed to open file for writing to save state " + std::to_string((long long)slot);
		return;
	}
	ASS(f.write("btas", 4));
	write_bin(f, st.scene);
	write_bin(f, st.frame);
	write_bin(f, st.sc_frame);
	write_bin(f, st.total);
	write_bin(f, st.ev);
	state_save(&f);
	last_msg = string("State ") + std::to_string((long long)slot) + " saved";
}

static void b_state_load(int slot) {
	string path = string("state") + std::to_string((long long)slot) + ".bstate";
	bfs::File f(path, 0);
	if (!f.is_open()) {
		last_msg = "Failed to open file for reading to load state " + std::to_string((long long)slot);
		return;
	}
	char buf[4];
	ASS(f.read(buf, 4));
	ASS(memcmp(buf, "btas", 4) == 0);
	int scene_id;
	load_bin(f, scene_id);
	if (scene_id != get_scene_id()) {
		last_msg = string("Scene ID mismatch for state ") + std::to_string((long long)slot);
		return;
	}
	st.scene = scene_id;
	load_bin(f, st.frame);
	load_bin(f, st.sc_frame);
	load_bin(f, st.total);
	load_bin(f, st.ev);
	state_load(&f);
	last_msg = string("State ") + std::to_string((long long)slot) + " loaded";
}

short btas::TasGetKeyState(int k) {
	auto eit = std::find(holding.begin(), holding.end(), k);
	return (eit == holding.end()) ? 0 : -32767;
}

bool btas::on_before_update() {
	now = timeGetTimeOrig();

	RunHeader* pState = *(RunHeader**)(mem::get_base() + 0x59a9c);
	int cur_scene = get_scene_id();
	if (cur_scene != st.scene)
		st.sc_frame = 0;
	st.scene = cur_scene;
	pState->subTickStep = 1;
	if (is_paused && !next_step) {
		pState->isPaused = true;
		return true;
	}
	next_step = false;
	pState->isPaused = false;
	// cout << "before\n";
	return false;
}

void btas::on_after_update() {
	RunHeader* pState = *(RunHeader**)(mem::get_base() + 0x59a9c);
	if (!pState->isPaused) {
		st.frame++;
		st.sc_frame++;
		st.total = std::max(st.total, st.frame);
		cur_time += 20;
	}
	if (is_hourglass) {
		Sleep(20);
		return;
	}
	DWORD advance = slowmo ? 100 : 20;
	// TODO: less performance eating way
	while (!fast_forward && now < (last_time + advance))
		now = timeGetTimeOrig();
	if (IsIconic(hwnd))
		Sleep(100);
	last_time = now;
	// cout << "after " << GetCurrentThreadId() << std::endl;
}

unsigned long btas::get_time() {
	return cur_time;
}

void btas::on_key(int k, bool pressed) {
	auto it = std::lower_bound(binds.begin(), binds.end(), k, [](const BTasBind& b, int key) {
		return b.key > key;
	});
	int current_mod = (MyKeyState(VK_CONTROL) ? 1 : 0) | (MyKeyState(VK_SHIFT) ? 2 : 0);
	while (it != binds.end()) {
		BTasBind& bind = *it;
		if (bind.key != k)
			break;
		if ((pressed && (bind.mod != current_mod)) || (!pressed && !bind.down))
			continue;
		switch (bind.idx) {
		case 0: {
			if ((bind.down && pressed) || is_replay)
				break;
			auto eit = std::find(holding.begin(), holding.end(), bind.mapper.k);
			if (pressed) {
				if (show_menu)
					break;
				// cout << "down " << GetCurrentThreadId() << std::endl;
				ASS(eit == holding.end());
				// if (eit == holding.end())
				bind.mapper.down_event = (int)st.ev.size();
				BTasEvent ev;
				ev.idx = 1;
				ev.frame = st.frame;
				ev.key.k = bind.mapper.k;
				st.ev.push_back(ev);
				holding.push_back(bind.mapper.k);
			}
			else if (eit != holding.end()) {
				// cout << "up\n";
				ASS(bind.mapper.down_event != -1);
				ASS(bind.mapper.down_event < (int)st.ev.size());
				if (st.ev[bind.mapper.down_event].frame == st.frame) {
					// TODO: remove from state
					st.ev.erase(st.ev.begin() + bind.mapper.down_event);
				}
				else {
					BTasEvent ev;
					ev.idx = 2;
					ev.frame = st.frame;
					ev.key.k = bind.mapper.k;
					st.ev.push_back(ev);
				}
				bind.mapper.down_event = -1;
				// if (eit != holding.end())
				holding.erase(eit);
			}
			break;
		}
		case 1: {
			if (pressed && !show_menu)
				is_paused = !is_paused;
			break;
		}
		case 2: {
			if (pressed && !show_menu)
				fast_forward = !fast_forward;
			break;
		}
		case 3: {
			if (!show_menu)
				fast_forward = pressed;
			break;
		}
		case 4: {
			if (!show_menu && pressed) {
				next_step = true;
				is_paused = true;
			}
			break;
		}
		case 5: {
			is_paused = !pressed;
			slowmo = pressed;
			break;
		}
		case 6: {
			if (!show_menu && pressed && !bind.down)
				b_state_save(bind.state.slot);
			break;
		}
		case 7: {
			if (!show_menu && pressed && !bind.down)
				b_state_load(bind.state.slot);
			break;
		}
		}
		bind.down = pressed;
		it++;
	}
}

void btas::draw_info() {
	ImGui::Text("Frames: %i / %i", st.frame, st.total);
	ImGui::Text("Message: %s", last_msg.c_str());
}

void btas::draw_tab() {
	if (ImGui::CollapsingHeader("BTas")) {
		RunHeader* pState = *(RunHeader**)(mem::get_base() + 0x59a9c);
		ImGui::Text("Random seed: %i", (unsigned int)(unsigned short)(pState->SystemTimeInMSFromSaveOrSeed));
	}
}
