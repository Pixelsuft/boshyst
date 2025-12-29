#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <iostream>
#include <cstdio>
#include <d3d9.h>
#include "conf.hpp"
#include "ass.hpp"
#include "init.hpp"
#include "fs.hpp"
#include "utils.hpp"
#include "btas.hpp"
#include "mem.hpp"
#include "ghidra_headers.h"
#include <vector>
#include <map>

using std::cout;
using std::string;

extern SHORT(__stdcall* GetAsyncKeyStateOrig)(int k);
extern DWORD(__stdcall* timeGetTimeOrig)();
extern bool state_save(bfs::File* file);
extern bool state_load(bfs::File* file);

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
};

struct BTasState {
	int frames;
	int scene;
	unsigned long time;
	std::vector<int> key_state;
	std::vector<BTasEvent> ev;

	BTasState() {
		frames = 0;
		scene = 0;
		time = 0;
	}
};

static DWORD real_last_time = 0;
extern int cur_frames;
extern int cur_frames2;
static BTasState st;
bool is_btas = false;
bool fast_forward = false;
bool is_paused = true;
bool is_replay = false; // TODO

static void to_file(const string& fn) {
	bfs::File out(fn, 1);
	ASS(out.write(&st, sizeof(int) * 3));
	int temp = (int)st.key_state.size();
	ASS(out.write(&temp, sizeof(int)));
	for (auto it = st.key_state.begin(); it != st.key_state.end(); it++) {
		ASS(out.write(&*it, sizeof(int)));
	}
	temp = (int)st.ev.size();
	ASS(out.write(&temp, sizeof(int)));
	for (auto it = st.ev.begin(); it != st.ev.end(); it++) {
		ASS(out.write(&*it, sizeof(BTasEvent)));
	}
	state_save(&out);
}

static void from_file(const string& fn) {
	bfs::File file(fn, 0);
	ASS(file.read(&st, sizeof(int) * 3));
	int temp = 0;
	ASS(file.read(&temp, sizeof(int)));
	st.key_state.resize((size_t)temp);
	for (int i = 0; i < temp; i++) {
		ASS(file.read(&st.key_state[i], sizeof(int)));
	}
	ASS(file.read(&temp, sizeof(int)));
	st.ev.resize((size_t)temp);
	for (int i = 0; i < temp; i++) {
		ASS(file.read(&st.ev[i], sizeof(BTasEvent)));
	}
	state_load(&file);
}

void btas::init() {
	cout << "btas init\n";
	real_last_time = timeGetTimeOrig();
	st.time = 0;
	st.frames = 0;
	st.scene = 0;
	if (is_replay) {
		from_file("test.bin");
		st.key_state.clear();
		st.frames = 0;
		st.scene = 0;
		st.time = 0;
	}
}

short btas::TasGetKeyState(int k) {
	return std::find(st.key_state.begin(), st.key_state.end(), k) == st.key_state.end() ? 0 : -32767;
	// return GetAsyncKeyStateOrig(k);
}

bool btas::on_before_update() {
	RunHeader* pState = *(RunHeader**)(mem::get_base() + 0x59a9c);
	if (JustKeyState(VK_F1) == 1) {
		to_file("test.bin");
	}
	if (JustKeyState(VK_F2) == 1) {
		from_file("test.bin");
	}
	if (JustKeyState(VK_PAUSE) == 1)
		is_paused = !is_paused;
	fast_forward = MyKeyState(VK_TAB);
	int temp = JustKeyState(VK_SPACE);
	if (temp == 1)
		is_paused = false;
	else if (temp == -1)
		is_paused = true;
	temp = JustKeyState('V');
	if (temp == 1)
		is_paused = false;
	else if (MyKeyState('V'))
		is_paused = true;

	if (is_paused) {
		pState->isPaused = true;
		real_last_time = timeGetTimeOrig();
		return true;
	}
	pState->isPaused = false;
	pState->subTickStep = 1;
	// st.time = timeGetTimeOrig();

	if (is_replay) {
		static size_t last_index = 0;
		for (; last_index < st.ev.size(); last_index++) {
			if (st.ev[last_index].frame > st.frames)
				break;
			BTasEvent& ev = st.ev[last_index];
			if (ev.idx == 0) {
				st.key_state.push_back(ev.key.k);
			}
			else if (ev.idx == 1) {
				auto it = std::find(st.key_state.begin(), st.key_state.end(), ev.key.k);
				if (it != st.key_state.end())
					st.key_state.erase(it);
			}
		}
		st.frames++;
		st.time += 20;
	}
	else {
		const int keys[] = { VK_LEFT, VK_RIGHT, VK_RETURN, 'Z', 'X' };
		for (int i = 0; i < sizeof(keys) / sizeof(int); i++) {
			int k = keys[i];
			temp = JustKeyState(k); // TODO: don't use in production, may break shit
			if (temp == 1) {
				st.key_state.push_back(k);
				BTasEvent ev;
				ev.key.k = k;
				ev.frame = st.frames;
				ev.idx = 0;
				st.ev.push_back(ev);
			}
			else if (temp == -1) {
				auto it = std::find(st.key_state.begin(), st.key_state.end(), k);
				if (it != st.key_state.end())
					st.key_state.erase(it);
				BTasEvent ev;
				ev.key.k = k;
				ev.frame = st.frames;
				ev.idx = 1;
				st.ev.push_back(ev);
			}
		}
		st.frames++;
		st.time += 20;
	}

	DWORD now = timeGetTimeOrig();
	if (!fast_forward && now < (real_last_time + 20))
		Sleep((DWORD)((long long)20 - (long long)now + (long long)real_last_time));
	if (MyKeyState(VK_SPACE) || MyKeyState('V'))
		Sleep(20);
	real_last_time = timeGetTimeOrig();
	return false;
}

unsigned long btas::get_time() {
	return st.time;
}
