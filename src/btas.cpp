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
	int scene;
	int frames;
	int total;
	std::vector<BTasEvent> ev;

	BTasState() {
		frames = 0;
		scene = 0;
		total = 0;
	}
};

static DWORD real_last_time = 0;
static BTasState st;
static unsigned long cur_time = 0;

bool is_btas = false;
bool fast_forward = false;
bool is_paused = true;
bool is_replay = false;

void btas::read_setting(const std::string& line, const std::string& line_orig) {
	if (0) {
		std::string fn = line;
		while (fn.size() > 0 && (isspace(fn[0]) || std::find(fn.begin(), fn.end(), ',') != fn.end()))
			fn = fn.substr(1);
	}
}

void btas::init() {
	cout << "btas init\n";
	cur_time = 0;
	real_last_time = timeGetTimeOrig();
}

short btas::TasGetKeyState(int k) {
	return 0 ? 0 : -32767;
}

bool btas::on_before_update() {
	DWORD now = timeGetTimeOrig();
	RunHeader* pState = *(RunHeader**)(mem::get_base() + 0x59a9c);
	pState->isPaused = false;
	pState->subTickStep = 1;
	st.frames++;
	cur_time += 20;

	if (!fast_forward && now < (real_last_time + 20))
		Sleep((DWORD)((long long)20 - (long long)now + (long long)real_last_time));
	real_last_time = timeGetTimeOrig();
	return false;
}

unsigned long btas::get_time() {
	return cur_time;
}

void btas::on_key(int k, bool pressed, bool repeat) {

}

void btas::draw_info() {
	ImGui::Text("TODO: BTAS info");
}

void btas::draw_tab() {
	if (ImGui::CollapsingHeader("BTas")) {
		ImGui::Text("TODO");
	}
}
