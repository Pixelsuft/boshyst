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

extern HANDLE hproc;
extern HWND hwnd;
extern SHORT(__stdcall* GetAsyncKeyStateOrig)(int k);
extern DWORD(__stdcall* timeGetTimeOrig)();
extern bool state_save(bfs::File* file);
extern bool state_load(bfs::File* file);
extern int(__stdcall* UpdateGameFrameOrig)();
static UINT (__stdcall *pTimeBeginPeriod)(UINT uPeriod);
static UINT (__stdcall *pTimeEndPeriod)(UINT uPeriod);
void(__cdecl* ExecuteTriggeredEvent)(unsigned int p);

struct BTasBind {
	union {
		struct {
			int k;
		} mapper;
		struct {
			int slot;
		} state;
		struct {
			int x;
			int y;
		} dummy;
	};
	int key;
	int mod;
	uint8_t idx;
	bool down;

	BTasBind() {
		mapper.k = 0;
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
		struct {
			int val;
		} hash;
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
	int cur_pos[2];
	int last_pos[2];
	std::vector<BTasEvent> ev;
	std::vector<int> prev;

	BTasState() {
		frame = sc_frame = 0;
		scene = 0;
		total = 0;
		cur_pos[0] = cur_pos[1] = last_pos[0] = last_pos[1] = 0;
	}
};

static std::vector<BTasBind> binds;
static std::vector<int> holding;
static std::vector<int> repl_holding;
static string last_msg;
static BTasState st;
static DWORD last_time = 0;
static DWORD now = 0;
static unsigned long cur_time = 0;
static int need_scene_state_slot = -1;
static bool next_step = false;
static bool slowmo = false;
static bool last_upd = false;
static bool reset_on_replay = false;
static int repl_index = 0;

bool is_btas = false;
bool fast_forward = false;
bool is_paused = true;
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
	ExecuteTriggeredEvent = (decltype(ExecuteTriggeredEvent))(mem::get_base() + 0x47cb0);
	last_time = now = timeGetTimeOrig();
	const uint8_t buf[] = { 0x90, 0x90, 0x90, 0x90, 0x90 };
	DWORD bW;
	// Disable timers when moving window to prevent desync
	ASS(WriteProcessMemory(hproc, (LPVOID)(mem::get_base() + 0x4b74), buf, 5, &bW) != 0 && bW == 5);
	ASS(WriteProcessMemory(hproc, (LPVOID)(mem::get_base() + 0x4b6d), buf, 5, &bW) != 0 && bW == 5);
}

template<typename T>
static void write_bin(bfs::File& f, const std::vector<T>& data) {
	size_t size = data.size();
	ASS(f.write(&size, sizeof(size_t)));
	if (size == 0)
		return;
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
	if (size == 0) {
		data.clear();
		return;
	}
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
	write_bin(f, st.cur_pos[0]);
	write_bin(f, st.cur_pos[1]);
	write_bin(f, st.last_pos[0]);
	write_bin(f, st.last_pos[1]);
	write_bin(f, st.prev);
	write_bin(f, st.ev);
	state_save(&f);
	last_msg = string("State ") + to_str(slot) + " saved";
}

static void b_state_load(int slot, bool from_loop) {
	string path = string("state") + to_str(slot) + ".bstate";
	bfs::File f(path, 0);
	if (!f.is_open()) {
		last_msg = "Failed to open file for reading to load state " + to_str(slot);
		return;
	}
	char buf[4];
	ASS(f.read(buf, 4));
	ASS(memcmp(buf, "btas", 4) == 0);
	int scene_id;
	load_bin(f, scene_id);
	if (is_replay && reset_on_replay && !from_loop) {
		need_scene_state_slot = slot;
		last_msg = "Restarting game";
		RunHeader* pState = *(RunHeader**)(mem::get_base() + 0x59a9c);
		pState->rhNextFrame = 4;
		pState->rhNextFrameData = scene_id | 0x8000;
		st.frame = st.sc_frame = 0;
		ExecuteTriggeredEvent(0xfffefffd);
		return;
	}
	if (!is_replay && scene_id != get_scene_id()) {
		need_scene_state_slot = slot;
		// last_msg = string("Scene ID mismatch (") + to_str(scene_id) + " instead of " + to_str(get_scene_id()) + ")";
		last_msg = string("Loading scene ") + to_str(scene_id);
		RunHeader* pState = *(RunHeader**)(mem::get_base() + 0x59a9c);
		pState->rhNextFrame = 3;
		pState->rhNextFrameData = scene_id | 0x8000;
		ExecuteTriggeredEvent(0xfffefffd);
		return;
	}
	if (is_replay) {
		int dummy;
		load_bin(f, dummy); // frame
		load_bin(f, dummy); // sc frame
		load_bin(f, st.total);
		load_bin(f, dummy); // cur pos
		load_bin(f, dummy);
		load_bin(f, dummy); // last_pos
		load_bin(f, dummy);
		std::vector<int> dummy2; // prev
		load_bin(f, dummy2);
		if (reset_on_replay) {
			st.frame = st.sc_frame = 0;
			cur_time = 0;
		}
	}
	else {
		st.scene = scene_id;
		load_bin(f, st.frame);
		load_bin(f, st.sc_frame);
		load_bin(f, st.total);
		load_bin(f, st.cur_pos[0]);
		load_bin(f, st.cur_pos[1]);
		load_bin(f, st.last_pos[0]);
		load_bin(f, st.last_pos[1]);
		load_bin(f, st.prev);
	}
	load_bin(f, st.ev);
	if (!is_replay)
		state_load(&f);
	last_msg = string("State ") + to_str(slot) + " loaded";
}

unsigned int btas::get_rng(unsigned int maxv) {
	// TODO
	return maxv;
}

short btas::TasGetKeyState(int k) {
	if (is_replay) {
		auto eit = std::find(repl_holding.begin(), repl_holding.end(), k);
		return (eit == repl_holding.end()) ? 0 : -32767;
	}
	auto eit = std::find(holding.begin(), holding.end(), k);
	return (eit == holding.end()) ? 0 : -32767;
}

bool btas::on_before_update() {
	now = timeGetTimeOrig();

	RunHeader* pState = *(RunHeader**)(mem::get_base() + 0x59a9c);
	if (need_scene_state_slot != -1) {
		b_state_load(need_scene_state_slot, true);
		repl_index = 0;
		need_scene_state_slot = -1;
	}
	int cur_scene = get_scene_id();
	if (cur_scene != st.scene)
		st.sc_frame = 0;
	st.scene = cur_scene;
	pState->subTickStep = 1;
	// cout << pState->frameStatus << std::endl;
	if (is_paused && !next_step) {
		pState->isPaused = true;
		return true;
	}
	if (is_replay) {
		for (; repl_index < (int)st.ev.size(); repl_index++) {
			BTasEvent& ev = st.ev[repl_index];
			if (ev.frame > st.frame)
				break;
			if (ev.frame < st.frame)
				continue;
			switch (ev.idx) {
			case 1: {
				repl_holding.push_back(ev.key.k);
				break;
			}
			case 2: {
				auto it = std::find(repl_holding.begin(), repl_holding.end(), ev.key.k);
				ASS(it != repl_holding.end());
				repl_holding.erase(it);
				break;
			}
			case 3: {
				// cout << "hash check " << ev.frame << std::endl;
				int comp_val = st.cur_pos[0] ^ st.cur_pos[1] ^ (int)pState->SystemTimeInMSFromSaveOrSeed;
				if (comp_val != ev.hash.val)
					last_msg = string("Hash check failed on frame ") + to_str(st.frame);
				break;
			}
			}
		}
		if (st.frame == st.total) {
			//is_replay = false;
			is_paused = true;
			//st.prev = repl_holding;
			//last_msg = "Switched to recording";
		}
	}
	else {
		for (auto it = holding.begin(); it != holding.end(); it++) {
			auto pit = std::find(st.prev.begin(), st.prev.end(), *it);
			if (pit == st.prev.end()) {
				BTasEvent ev;
				ev.key.k = *it;
				ev.frame = st.frame;
				ev.idx = 1;
				st.ev.push_back(ev);
			}
		}
		for (auto pit = st.prev.begin(); pit != st.prev.end(); pit++) {
			auto it = std::find(holding.begin(), holding.end(), *pit);
			if (it == holding.end()) {
				BTasEvent ev;
				ev.key.k = *pit;
				ev.frame = st.frame;
				ev.idx = 2;
				st.ev.push_back(ev);
			}
		}
		if (st.frame % 20 == 0) {
			BTasEvent ev;
			ev.hash.val = st.cur_pos[0] ^ st.cur_pos[1] ^ (int)pState->SystemTimeInMSFromSaveOrSeed;
			ev.frame = st.frame;
			ev.idx = 3;
			st.ev.push_back(ev);
			// cout << "Hashing frame " << st.frame << std::endl;
		}
	}
	last_upd  = true;
	st.prev = holding;
	next_step = false;
	pState->isPaused = false;
	return false;
}

void btas::on_after_update() {
	RunHeader* pState = *(RunHeader**)(mem::get_base() + 0x59a9c);
	if (last_upd) {
		last_upd = false;
		st.frame++;
		st.sc_frame++;
		st.total = std::max(st.total, st.frame);
		cur_time += 20;

		ObjectHeader* pp = (ObjectHeader*)get_player_ptr(get_scene_id());
		if (pp != nullptr) {
			st.last_pos[0] = st.cur_pos[0];
			st.last_pos[1] = st.cur_pos[1];
			st.cur_pos[0] = pp->xPos;
			st.cur_pos[1] = pp->yPos;
		}
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
		if ((pressed && (bind.mod != current_mod)) || (!pressed && !bind.down)) {
			it++;
			continue;
		}
		switch (bind.idx) {
		case 0: {
			if (bind.down && pressed)
				break;
			auto eit = std::find(holding.begin(), holding.end(), bind.mapper.k);
			if (pressed) {
				if (show_menu)
					break;
				ASS(eit == holding.end()); // Single binds are allowed only
				holding.push_back(bind.mapper.k);
			}
			else if (eit != holding.end()) {				
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
				b_state_load(bind.state.slot, false);
			break;
		}
		}
		bind.down = pressed;
		it++;
	}
}

void btas::draw_info() {
	int inGameFrames = *(int*)(*(size_t*)(mem::get_base() + 0x59a9c) + 0xd0);
	ImGui::Text("Frames: %i / %i, %i, %i", st.frame, st.total, st.sc_frame, inGameFrames);

	ImGui::Text("Pos: (%i, %i)", st.cur_pos[0], st.cur_pos[1]);
	ImGui::Text("Delta: (%i, %i)", st.cur_pos[0] - st.last_pos[0], st.cur_pos[1] - st.last_pos[1]);
	ImGui::Text("Align: %i", st.cur_pos[0] % 3);

	ImGui::Text("Scene ID: %i", get_scene_id());
	// ImGui::Text("Time: %u", cur_time);
	ImGui::Text("Message: %s", last_msg.c_str());
}

void btas::draw_tab() {
	if (ImGui::CollapsingHeader("BTas", ImGuiTreeNodeFlags_DefaultOpen)) {
		RunHeader* pState = *(RunHeader**)(mem::get_base() + 0x59a9c);
		ImGui::Text("Random seed: %i", (unsigned int)(unsigned short)(pState->SystemTimeInMSFromSaveOrSeed));
		if (ImGui::Checkbox("Replay mode", &is_replay)) {
			if (is_replay && st.frame != 0 && !reset_on_replay)
				last_msg = "Running replay not from start, may desync!";
			if (is_replay)
				repl_holding.clear();
			else {
				st.prev = repl_holding;
				st.total = st.frame;
				st.ev.resize(repl_index);
			}
			repl_index = 0;
		}
		ImGui::Checkbox("Reset game on replay", &reset_on_replay);
	}
}
