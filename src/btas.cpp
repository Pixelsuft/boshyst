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
#define STATE_VER 1

using std::cout;
using std::string;

extern HANDLE hproc;
extern HWND hwnd;
extern HWND mhwnd;
extern SHORT(__stdcall* GetAsyncKeyStateOrig)(int k);
extern DWORD(__stdcall* timeGetTimeOrig)();
extern int(__stdcall* UpdateGameFrameOrig)();
extern LRESULT(__stdcall* SusProc)(HWND, UINT, WPARAM, LPARAM);
static UINT (__stdcall *pTimeBeginPeriod)(UINT uPeriod);
static UINT (__stdcall *pTimeEndPeriod)(UINT uPeriod);
static void(__cdecl* DestroyObject)(int handleIndex, int destroyMode);
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
		struct {
			int range;
			int val;
		} rng;
		struct {
			int val;
		} tm;
	};
	int frame;
	uint8_t idx;

	BTasEvent() {
		click.x = click.y = 0;
		frame = 0;
		idx = 0;
	}
};

struct BTasState {
	std::vector<BTasEvent> ev;
	std::vector<BTasEvent> temp_ev;
	std::vector<IntPair> rng_buf;
	std::vector<IntPair> timer_conds;
	std::vector<int> prev;
	int cur_pos[2];
	int last_pos[2];
	int m_pos[2];
	int scene;
	int frame;
	int sc_frame;
	int total;
	int time;
	int c1;
	short seed;

	BTasState() {
		clear();
	}

	void clear_arr() {
		ev.clear();
		temp_ev.clear();
		rng_buf.clear();
		prev.clear();
		timer_conds.clear();
	}

	void clear() {
		time = 0;
		frame = sc_frame = 0;
		scene = 0;
		total = 0;
		cur_pos[0] = cur_pos[1] = last_pos[0] = last_pos[1] = 0;
		m_pos[0] = m_pos[1] = -100;
		seed = 0;
		c1 = 0;
	}
};

static std::vector<BTasBind> binds;
static std::vector<int> holding;
static std::vector<int> repl_holding;
static string last_msg;
static BTasState st;
static DWORD last_time = 0;
static DWORD now = 0;
static int need_scene_state_slot = -1;
static bool timers_fix = true;
static bool next_step = false;
static bool slowmo = false;
bool last_upd = false;
static bool reset_on_replay = false;
static int repl_index = 0;
static char export_buf[MAX_PATH];
static bool export_hash = true;

bool is_btas = false;
bool fast_forward = false;
bool fast_forward_skip = false;
bool is_paused = true;
bool is_replay = false;
bool b_loading_saving_state = false;

static RunHeader& get_state() {
	return **(RunHeader**)(mem::get_base() + 0x59a9c);
}

static GlobalStats& get_stats() {
	return **(GlobalStats**)(mem::get_base() + 0x59a98);
}

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
	else if (starts_with(line, "btas=toggle_fastforward_skip,")) {
		bind.idx = 8;
		ASS(sscanf(line.substr(29).c_str(), "%i,%i", &bind.key, &bind.mod) == 2);
	}
	else if (starts_with(line, "btas=fastforward_skip,")) {
		bind.idx = 9;
		ASS(sscanf(line.substr(22).c_str(), "%i,%i", &bind.key, &bind.mod) == 2);
	}
	else {
		ass::show_err((string("Unknown BTAS setting: ") + line_orig).c_str());
		ASS(false);
	}
	binds.push_back(bind);
}

#define WPM(addr, data, size) do { ASS(WriteProcessMemory(hproc, (LPVOID)(addr), data, size, &bW) != 0 && bW == size); } while (0)

void btas::pre_init() {
	cout << "btas pre-init\n";
	const uint8_t buf[] = { 0x90, 0x90, 0x90, 0x90, 0x90, 0x90 };
	const uint8_t joy_patch[] = {
		0x31, 0xC0, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90,
		0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x85, 0xC0, 0xEB, 0x00
	};
	uint8_t temp = 0xeb;
	DWORD bW;
	// Disable automatic frame rendering
	WPM(mem::get_base() + 0x1dcc1, buf, 5);
	// Disable timers when moving window to prevent desync
	WPM(mem::get_base() + 0x4b74, buf, 5);
	WPM(mem::get_base() + 0x4b6d, buf, 5);
	WPM(mem::get_base() + 0x4c29, &temp, 1);
	// Disable CRun_SyncFrameRate, MsgWaitForMultipleObjects and some other sync stuff when needed
	// WPM(mem::get_base() + 0x365db, buf, 5);
	// WPM(mem::get_base() + 0x36630, buf, 5);
	WPM(mem::get_base() + 0x4659f, buf, 5);
	WPM(mem::get_base() + 0x2a74, buf, 2);
	WPM(mem::get_base() + 0x2a49, &temp, 1);
	WPM(mem::get_base() + 0x2a53, &temp, 1); // No throttling
	// Disable controller options menu
	WPM(mem::get_base() + 0x43036, buf, 5);
	WPM(mem::get_base() + 0x4304e, buf, 5);
	WPM(mem::get_base() + 0x43056, buf, 5);
	// Joystick stuff
	WPM(mem::get_base() + 0x44e3, buf, 5);
	WPM(mem::get_base() + 0xb245, buf, 5);
	WPM(mem::get_base() + 0x2013b, joy_patch, 17);
	// Keyboard stuff (Disable TAB logic)
	WPM(mem::get_base() + 0x332df, &temp, 1);
	WPM(mem::get_base() + 0x3eaab, buf, 5);
	WPM(mem::get_base() + 0x3ea96, buf, 5);
	// Disable replay mode (WTF?)
	WPM(mem::get_base() + 0x1d93e, buf, 5);
	WPM(mem::get_base() + 0x1d931, buf, 5);
	// RNG disable SavedSeed use
	WPM(mem::get_base() + 0x36241, buf, 2);
	// Clipboard
	WPM(mem::get_base() + 0xe1fa, &temp, 1);
	WPM(mem::get_base() + 0xe2cd, &temp, 1);
	// No drag/drop
	WPM(mem::get_base() + 0x41985, &temp, 1);
	WPM(mem::get_base() + 0x425a1, &temp, 1);
	WPM(mem::get_base() + 0x436b4, &temp, 1);
	// Disable some dialogs (WTF?)
	const uint8_t cent_patch[] = { 0x66, 0xe9, 0xca };
	WPM(mem::get_base() + 0x431a2, cent_patch, 3);
	// Disable window centering
	// WPM(mem::get_base() + 0x330eb, &temp, 1);
	// Patch save state bug in original code (rcBoundaryBottom instead of doubled rcBoundaryRight)
	// TODO: move to hacks.cpp?
	temp = 0x24;
	WPM(mem::get_base() + 0x386fb, &temp, 1);
	WPM(mem::get_base() + 0x3a4e9, &temp, 1);
}

void btas::init() {
	cout << "btas init\n";
	const uint8_t buf[] = { 0x90, 0x90, 0x90, 0x90, 0x90 };
	uint8_t temp = 0xeb;
	DWORD bW;
	// No Sleep
	WPM(mem::get_base("mmfs2.dll") + 0xc3f3, buf, 2);
	WPM(mem::get_base("mmfs2.dll") + 0xc3a5, &temp, 1);
	WPM(mem::get_base("mmfs2.dll") + 0x2dd58, &temp, 1);
	if (GetModuleHandleA("mmf2d3d9.dll") != nullptr)
		WPM(mem::get_base("mmf2d3d9.dll") + 0x270c, &temp, 1);
	// WPM(mem::get_base("ctrlx.mfx") + 0x4378, buf, 5);
	// WPM(mem::get_base("kcfloop.mfx") + 0x10f3, &temp, 1); // Right??
	// No multimedia timers (I hope it doesn't crash)
	WPM(mem::get_base("mmfs2.dll") + 0x4459f, &temp, 1);
	WPM(mem::get_base("mmfs2.dll") + 0x41af4, &temp, 1);
	// No threads, Lacewing.mfx!
	WPM(mem::get_base("Lacewing.mfx") + 0xb209, &temp, 1);
	WPM(mem::get_base("Lacewing.mfx") + 0x88cb, buf, 5);
	WPM(mem::get_base("Lacewing.mfx") + 0xb340, buf, 5);

	std::sort(binds.begin(), binds.end(), [](BTasBind a, BTasBind b) {
		return a.key > b.key;
	});
	st.time = 0;
	next_step = slowmo = false;
	auto h = GetModuleHandleW(L"winmm.dll");
	ASS(h != nullptr);
	pTimeBeginPeriod = (decltype(pTimeBeginPeriod))GetProcAddress(h, "timeBeginPeriod");
	pTimeEndPeriod = (decltype(pTimeEndPeriod))GetProcAddress(h, "timeEndPeriod");
	ASS(pTimeBeginPeriod != nullptr && pTimeEndPeriod != nullptr);
	last_msg = "None";
	strcpy(export_buf, "replay");
	DestroyObject = (decltype(DestroyObject))(mem::get_base() + 0x1e710);
	ExecuteTriggeredEvent = (decltype(ExecuteTriggeredEvent))(mem::get_base() + 0x47cb0);
	st.clear();
	st.clear_arr();
	last_time = now = timeGetTimeOrig();
}

static void trim_current_state() {
	st.total = st.frame;
	// Hacky
	while (!st.ev.empty() && (st.ev.end() - 1)->frame > st.frame)
		st.ev.erase(st.ev.end() - 1);
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

static void fill_timers_fix() {
	// TODO: why so many timers if used much less?
	if (!timers_fix)
		return;
	GlobalStats& gStats = get_stats();
	EventGroup* eventPtr = gStats.pEventGroups;
	while (eventPtr->length != 0) {
		ConditionHeader* cond = (ConditionHeader*)&eventPtr->condStart;
		for (int i = (int)eventPtr->eventCount; i != 0; i--) {
			if (cond->condID == -4 && cond->conditionType == 13 && cond->size == 30) {
				// cout << "savin " << cond->currentTimer << " " << cond->interval << std::endl;
				st.timer_conds.push_back(IntPair(cond->currentTimer, cond->interval));
			}
			cond = (ConditionHeader*)((size_t)cond + (size_t)cond->size);
		}
		eventPtr = (EventGroup*)
			((size_t)eventPtr - (size_t)eventPtr->length);
	}
}

static void import_timers_fix() {
	if (!timers_fix) {
		st.timer_conds.clear();
		return;
	}
	if (st.timer_conds.empty()) {
		last_msg = "Timer fix was not enabled";
		return;
	}
	auto it = st.timer_conds.begin();
	GlobalStats& gStats = get_stats();
	EventGroup* eventPtr = gStats.pEventGroups;
	while (eventPtr->length != 0) {
		ConditionHeader* cond = (ConditionHeader*)&eventPtr->condStart;
		for (int i = (int)eventPtr->eventCount; i != 0; i--) {
			if (cond->condID == -4 && cond->conditionType == 13 && cond->size == 30) {
				// cout << "loadin " << cond->currentTimer << " " << cond->interval << std::endl;
				if (it == st.timer_conds.end()) {
					last_msg = "Not enough data to re-fill timer conds (WTF?)";
					st.timer_conds.clear();
					return;
				}
				if (it->b != cond->interval) {
					last_msg = "Timer conds got fucked (WTF?)";
					st.timer_conds.clear();
					return;
				}
				cond->currentTimer = it->a;
				it++;
			}
			cond = (ConditionHeader*)((size_t)cond + (size_t)cond->size);
		}
		eventPtr = (EventGroup*)
			((size_t)eventPtr - (size_t)eventPtr->length);
	}
	st.timer_conds.clear();
}

static void b_state_save(int slot) {
	string path = string("state") + std::to_string((long long)slot) + ".bstate";
	bfs::File f(path, 1);
	if (!f.is_open()) {
		last_msg = "Failed to open file for writing to save state " + std::to_string((long long)slot);
		return;
	}
	RunHeader& pState = get_state();
	ASS(f.write("btas", 4));
	write_bin(f, (int)STATE_VER);
	write_bin(f, st.scene);
	write_bin(f, st.frame);
	write_bin(f, st.sc_frame);
	write_bin(f, st.total);
	write_bin(f, st.cur_pos[0]);
	write_bin(f, st.cur_pos[1]);
	write_bin(f, st.last_pos[0]);
	write_bin(f, st.last_pos[1]);
	write_bin(f, st.m_pos[0]);
	write_bin(f, st.m_pos[1]);
	write_bin(f, st.time);
	write_bin(f, st.c1);
	write_bin(f, st.seed);
	write_bin(f, st.rng_buf);
	fill_timers_fix();
	write_bin(f, st.timer_conds);
	// cout << "saved " << st.timer_conds.size() << std::endl;
	st.timer_conds.clear();
	write_bin(f, st.prev);
	write_bin(f, st.temp_ev);
	write_bin(f, st.ev);
	b_loading_saving_state = true;
	state_save(&f);
	b_loading_saving_state = false;
	last_msg = string("State ") + to_str(slot) + " saved";
}

static void b_state_load(int slot, bool from_loop) {
	RunHeader& pState = get_state();
	string path = string("state") + to_str(slot) + ".bstate";
	bfs::File f(path, 0);
	if (!f.is_open()) {
		last_msg = "Failed to open file for reading to load state " + to_str(slot);
		return;
	}
	if (is_replay && reset_on_replay && !from_loop && st.frame != 0) {
		st.prev.clear();
		st.ev.clear();
		repl_holding.clear();
		need_scene_state_slot = slot;
		last_msg = "Restarting game";
		RunHeader& pState = get_state();
		pState.rhNextFrame = 4;
		pState.rhNextFrameData = 0;
		st.frame = st.sc_frame = 0;
		st.seed = 0;
		st.time = 0;
		ExecuteTriggeredEvent(0xfffefffd);
		return;
	}
	char buf[4];
	ASS(f.read(buf, 4));
	ASS(memcmp(buf, "btas", 4) == 0);
	int st_ver;
	load_bin(f, st_ver);
	if (st_ver != STATE_VER) {
		last_msg = "Wrong state version";
		return;
	}
	int scene_id;
	load_bin(f, scene_id);
	if (!is_replay && scene_id != get_scene_id()) {
		cout << "preparing change\n";
		need_scene_state_slot = slot;
		// last_msg = string("Scene ID mismatch (") + to_str(scene_id) + " instead of " + to_str(get_scene_id()) + ")";
		last_msg = string("Loading scene ") + to_str(scene_id);
		RunHeader& pState = get_state();
		pState.rhNextFrame = 3;
		pState.rhNextFrameData = scene_id | 0x8000;
		ExecuteTriggeredEvent(0xfffefffd);
		return;
	}
	last_msg = "";
	if (is_replay) {
		if (reset_on_replay) {
			st.clear();
			st.clear_arr();
		}
		int dummy;
		load_bin(f, dummy); // frame
		load_bin(f, dummy); // sc frame
		load_bin(f, st.total);
		load_bin(f, dummy); // cur pos
		load_bin(f, dummy);
		load_bin(f, dummy); // last_pos
		load_bin(f, dummy);
		load_bin(f, dummy); // m_pos
		load_bin(f, dummy);
		load_bin(f, dummy); // time
		load_bin(f, dummy); // c1
		short dummy3;
		load_bin(f, dummy3); // seed
		std::vector<int> dummy2;
		std::vector<IntPair> dummy4;
		load_bin(f, dummy4); // rng_buf
		load_bin(f, dummy4); // timer_conds
		load_bin(f, dummy2); // prev
		load_bin(f, dummy2); // temp_ev
		load_bin(f, st.ev);
		if (reset_on_replay) {
			repl_holding.clear();
			init_temp_saves();
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
		load_bin(f, st.m_pos[0]);
		load_bin(f, st.m_pos[1]);
		load_bin(f, st.time);
		load_bin(f, st.c1);
		load_bin(f, st.seed);
		load_bin(f, st.rng_buf);
		load_bin(f, st.timer_conds);
		load_bin(f, st.prev);
		load_bin(f, st.temp_ev);
		load_bin(f, st.ev);
	}
	// pState.RandomSeed = st.seed;
	if (!is_replay) {
		b_loading_saving_state = true;
		state_load(&f);
		b_loading_saving_state = false;
		trim_current_state();
		// Fix internal state load bug
		// TODO: also do this in normal mode?
		for (int i = 0; i < pState.activeObjectCount; i++) {
			ObjectHeader* obj = pState.objectList[i * 2];
			if (!obj || !(obj->flags) || !obj->spriteHandle)
				continue;
			// Force mask recalculation
			obj->spriteHandle->flags |= 1;
		}
		import_timers_fix();
		pState.lastFrameScore = st.c1;
		// pState.RandomSeed = st.seed;
	}
	// pState.rhNextFrame = 0;
	// cout << "state loaded\n";
	if (last_msg.empty())
		last_msg = string("State ") + to_str(slot) + " loaded";
	else
		last_msg = string("State ") + to_str(slot) + " loaded (" + last_msg + ")";
}

static void export_replay(const std::string& path) {
	bfs::File f(path, 1);
	if (!f.is_open()) {
		last_msg = "Failed to open file for writing replay";
		return;
	}
	ASS(f.write_line("brep"));
	ASS(f.write_line(string("total: ") + to_str(st.total)));
	ASS(f.write_line("data: "));
	for (auto it = st.ev.begin(); it != st.ev.end(); it++) {
		BTasEvent& ev = *it;
		if (!export_hash && (ev.idx == 3 || ev.idx == 4))
			continue;
		if (ev.idx == 1 || ev.idx == 2 || ev.idx == 3 || ev.idx == 4 || ev.idx == 7 || ev.idx == 8) {
			ASS(f.write_line(to_str((int)ev.idx) + "," + to_str(ev.frame) + "," + to_str(ev.click.x)));
		}
		else {
			ASS(f.write_line(to_str((int)ev.idx) + "," + to_str(ev.frame) + "," + to_str(ev.click.x) + "," + to_str(ev.click.y)));
		}
	}
	last_msg = "Replay exported";
}

static void import_replay(const std::string& path) {
	bfs::File f(path, 0);
	if (!f.is_open()) {
		last_msg = "Failed to open file for reading replay";
		return;
	}
	string line;
	ASS(f.read_line(line) && line == "brep");
	st.clear_arr();
	st.clear();
	while (f.read_line(line)) {
		if (starts_with(line, "total: "))
			st.total = std::atoi(line.substr(7).c_str());
		else if (starts_with(line, "data: "))
			break;
	}
	while (f.read_line(line)) {
		int idx = atoi(line.c_str());
		BTasEvent ev;
		if (idx == 1 || idx == 2 || idx == 3 || idx == 4 || idx == 7 || idx == 8)
			ASS(sscanf(line.c_str(), "%i,%i,%i", &idx, &ev.frame, &ev.click.x) == 3);
		else
			ASS(sscanf(line.c_str(), "%i,%i,%i,%i", &idx, &ev.frame, &ev.click.x, &ev.click.y) == 4);
		ev.idx = (uint8_t)idx;
		// cout << idx << ' ' << ev.frame << ' ' << ev.click.x << ' ' << ev.click.y << std::endl;
		st.ev.push_back(ev);
	}
	is_replay = true;
	last_msg = "Replay imported";
}

void btas::reg_obj(int handle) {
	// unused
	if (b_loading_saving_state && 0) {
		cout << "bullet reg\n";
		RunHeader& pState = get_state();
		auto obj = pState.objectList[handle * 2];
	}
	if (0) {
		RunHeader& pState = get_state();
		auto& obj = *pState.objectList[handle * 2];
		auto& s = *obj.spriteHandle;
		cout << "launch " << handle << " " << s.flags << " " << s.layer << " "
			<< s.numChildren << " " << s.textureHandleA << " " << s.textureHandleB << " "
			<< s.zOrder << " "
			<< std::endl;
		// obj->movementType = 13
		//void(__cdecl * MMF2_RefreshObject)(ObjectHeader*);
		//MMF2_RefreshObject = (decltype(MMF2_RefreshObject))0x401870;
		//MMF2_RefreshObject(obj);
		// obj->nextSelectedHandle = -1;
		// TODO: bullet warn!!!!!!!! 4479e8
		// TODO: research
		// should trigger destruction at 0411ed1

		// renderGroup = 2
		// upd cb 41EA60
		// important 401eb6 RefreshObjectVisuals DestroyObjectInstance(refs)
		// 40211e for showing boshyhitbox
		// TODO: 420aa6 ui set to remove anim
		// 042e530 - player coll with objs
	}
}

void btas::unreg_obj(int handle) {
	// unused
}

unsigned int btas::get_rng(unsigned int maxv) {
	auto it = std::lower_bound(st.rng_buf.begin(), st.rng_buf.end(), (int)maxv, [](const IntPair& a, int range) {
		return a.a > range;
	});
	if (it == st.rng_buf.end() || it->a != (int)maxv)
		return maxv;
	auto ret = it->b;
	st.rng_buf.erase(it);
	return ret;
}

short btas::TasGetKeyState(int k) {
	if (!last_upd)
		return 0;
	if (is_replay) {
		auto eit = std::find(repl_holding.begin(), repl_holding.end(), k);
		return (eit == repl_holding.end()) ? 0 : -32767;
	}
	auto eit = std::find(holding.begin(), holding.end(), k);
	return (eit == holding.end()) ? 0 : -32767;
}

static void exec_event(BTasEvent& ev) {
	RunHeader& pState = get_state();
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
		int comp_val = st.cur_pos[0] ^ st.cur_pos[1];
		if (comp_val != ev.hash.val) {
			last_msg = string("Hash check (POS) failed on frame ~") + to_str(st.frame) + ", resetting";
			ev.hash.val = comp_val;
		}
		break;
	}
	case 4: {
		int comp_val = st.seed;
		if (comp_val != ev.hash.val) {
			last_msg = string("Hash check (RNG) failed on frame ~") + to_str(st.frame) + ", resetting";
			ev.hash.val = comp_val;
		}
		break;
	}
	case 5: {
		st.m_pos[0] = ev.click.x;
		st.m_pos[1] = ev.click.y;
		if (ev.click.x < 0 || ev.click.y < 0)
			break;
		SusProc(mhwnd, WM_LBUTTONDOWN, 0, 0);
		SusProc(mhwnd, WM_LBUTTONUP, 0, 0);
		break;
	}
	case 6: {
		st.rng_buf.push_back(IntPair(ev.rng.range, ev.rng.val));
		std::sort(st.rng_buf.begin(), st.rng_buf.end(), [](const IntPair& a, const IntPair& b) {
			return a.a > b.a;
		});
		break;
	}
	case 7: {
		// TODO: optimize?
		while (1) {
			auto it = std::lower_bound(st.rng_buf.begin(), st.rng_buf.end(), ev.rng.range, [](const IntPair& a, int range) {
				return a.a > range;
				});
			if (it == st.rng_buf.end() || it->a != ev.rng.range)
				break;
			st.rng_buf.erase(it);
		}
		break;
	}
	case 8: {
		st.time = ev.tm.val;
		break;
	}
	}
}

bool btas::on_before_update() {
	now = timeGetTimeOrig();

	RunHeader& pState = get_state();
	if (need_scene_state_slot != -1) {
		// cout << "load state\n";
		b_state_load(need_scene_state_slot, true);
		repl_index = 0;
		need_scene_state_slot = -1;
	}
	int cur_scene = get_scene_id();
	if (cur_scene != st.scene)
		st.sc_frame = 0;
	st.scene = cur_scene;
	pState.frameSkipAccumulator = 0;
	pState.subTickStep = 1;
	// cout << pState.frameStatus << std::endl;
	if (is_paused && !next_step) {
		pState.isPaused = true;
		return true;
	}
	pState.isPaused = false;
	if (is_replay) {
		for (; repl_index < (int)st.ev.size(); repl_index++) {
			BTasEvent& ev = st.ev[repl_index];
			if (ev.frame > st.frame)
				break;
			if (ev.frame < st.frame)
				continue;
			exec_event(ev);
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
		for (auto it = st.temp_ev.begin(); it != st.temp_ev.end(); it++) {
			exec_event(*it);
			st.ev.push_back(*it);
		}
		st.temp_ev.clear();
		if (st.frame % 40 == 0) {
			BTasEvent ev;
			if (st.frame % 80 == 0) {
				ev.hash.val = st.cur_pos[0] ^ st.cur_pos[1];
				ev.idx = 3;
			}
			else {
				ev.hash.val = (int)st.seed;
				ev.idx = 4;
			}
			ev.frame = st.frame;
			if (ev.hash.val != 0)
				st.ev.push_back(ev);
			// cout << "Hashing frame " << st.frame << std::endl;
		}
	}
	ushort temp_seed = (ushort)st.seed;
	pState.RandomSeed = *(short*)&temp_seed;
	last_upd = true;
	st.prev = is_replay ? repl_holding : holding;
	next_step = false;

	return false;
}

void btas::on_after_update() {
	RunHeader& pState = get_state();
	if (last_upd) {
		last_upd = false;
		st.c1 = pState.lastFrameScore;
		st.seed = (int)*(ushort*)&pState.RandomSeed;
		st.frame++;
		// cout << st.frame << " " << st.seed << std::endl;
		st.sc_frame++;
		st.total = std::max(st.total, st.frame);
		st.time += 20;

		ObjectHeader* pp = (ObjectHeader*)get_player_ptr(get_scene_id());
		if (pp != nullptr) {
			st.last_pos[0] = st.cur_pos[0];
			st.last_pos[1] = st.cur_pos[1];
			st.cur_pos[0] = pp->xPos;
			st.cur_pos[1] = pp->yPos;
		}
		else {
			st.cur_pos[0] = st.cur_pos[1] = st.last_pos[0] = st.last_pos[1] = 0;
		}

		if (is_replay && st.frame == st.total && st.frame > 0) {
			is_replay = false;
			is_paused = true;
			repl_holding.clear();
			last_msg = "Switched to recording";
		}
	}
	if (is_hourglass) {
		Sleep(20);
		return;
	}
	DWORD advance = slowmo ? 100 : 20;
	// TODO: less performance eating way
	while (!fast_forward && !fast_forward_skip && now < (last_time + advance))
		now = timeGetTimeOrig();
	if (IsIconic(hwnd))
		Sleep(100);
	last_time = now;
	// cout << "after " << GetCurrentThreadId() << std::endl;
}

unsigned long btas::get_time() {
	return st.time;
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
		if ((pressed && ((bind.mod != current_mod) || b_loading_saving_state)) || (!pressed && !bind.down)) {
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
		case 8: {
			if (pressed && !show_menu)
				fast_forward_skip = !fast_forward_skip;
			break;
		}
		case 9: {
			if (!show_menu)
				fast_forward_skip = pressed;
			break;
		}
		}
		bind.down = pressed;
		it++;
	}
}

void btas::draw_info() {
	ImGui::Text("Frames: %i / %i, %i, %i", st.frame, st.total, st.sc_frame, get_state().frameCount);

	ImGui::Text("Pos: (%i, %i)", st.cur_pos[0], st.cur_pos[1]);
	ImGui::Text("Delta: (%i, %i)", st.cur_pos[0] - st.last_pos[0], st.cur_pos[1] - st.last_pos[1]);
	ImGui::Text("Align: %i", st.cur_pos[0] % 3);

	ImGui::Text("Scene: %i (%s)", get_scene_id(), get_scene_name());
	// ImGui::Text("Time: %u", cur_time);
	ImGui::Text("Message: %s", last_msg.c_str());
	if (fast_forward || fast_forward_skip)
		return;
	string cur_keys = "";
	for (auto it = st.prev.cbegin(); it != st.prev.cend(); it++) {
		switch (*it) {
		case 'Q':
		case 'R':
		case 'S':
		case 'Z':
		case 'X':
		case 'C':
			cur_keys += ", ";
			cur_keys += (char)*it;
			break;
		case VK_LEFT:
			cur_keys += ", <-";
			break;
		case VK_RIGHT:
			cur_keys += ", ->";
			break;
		case VK_DOWN:
			cur_keys += ", \\/";
			break;
		case VK_UP:
			cur_keys += ", ^";
			break;
		case VK_F2:
			cur_keys += ", F2";
			break;
		case VK_F3:
			cur_keys += ", F3";
			break;
		case VK_F5:
			cur_keys += ", F5";
			break;
		case VK_ESCAPE:
			cur_keys += ", ESC";
			break;
		}
	}
	ImGui::Text("Keys: %s", cur_keys.size() > 1 ? cur_keys.substr(2).c_str() : "");
}

void btas::draw_tab() {
	if (!is_replay)
		is_paused = true; // TODO: configure that
	RECT test;
	GetClientRect(::hwnd, &test);
	//cout << test.right << "x" << test.bottom << '\n';
	if (ImGui::CollapsingHeader("BTas", ImGuiTreeNodeFlags_DefaultOpen)) {
		RunHeader& pState = get_state();
		ImGui::Checkbox("Paused", &is_paused);
		ImGui::Checkbox("Fast forward", &fast_forward);
		if (ImGui::Checkbox("Replay mode", &is_replay)) {
			if (is_replay && st.frame != 0 && !reset_on_replay)
				last_msg = "Running replay not from start, may desync!";
			repl_holding.clear();
			if (is_replay) {
				st.temp_ev.clear();
			}
			else {
				trim_current_state();
				st.ev.resize(repl_index);
			}
			repl_index = 0;
		}
		ImGui::Checkbox("Reset game on replay (BETA)", &reset_on_replay);
		ImGui::InputText("Replay name", export_buf, MAX_PATH);
		ImGui::Checkbox("Export hash checks", &export_hash);
		if (ImGui::Button("Export"))
			export_replay(string(export_buf) + ".breplay");
		if (st.frame == 0)
			ImGui::SameLine();
		if (st.frame == 0 && ImGui::Button("Import"))
			import_replay(string(export_buf) + ".breplay");
		ImGui::Checkbox("Timer conditions fix", &timers_fix);
		static int rval[3] = { 0, 0, 0 };
		ImGui::Text("Random seed: %i", st.seed);
		ImGui::InputInt("RNG value", &rval[0]);
		if (ImGui::InputInt("RNG range", &rval[1]))
			rval[1] = std::max(rval[1], 1);
		if (ImGui::InputInt("RNG repeat (0 - clear range)", &rval[2]))
			rval[2] = std::max(rval[2], 0);
		if (ImGui::Button("Push RNG")) {
			BTasEvent ev;
			ev.rng.val = rval[0];
			ev.rng.range = rval[1];
			ev.frame = st.frame;
			ev.idx = rval[2] == 0 ? 7 : 6;
			for (int i = 0; i < rval[2]; i++)
				st.temp_ev.push_back(ev);
		}
		static int mpos[2] = { 0, 0 };
		ImGui::InputInt2("Mouse pos for click", mpos);
		if (ImGui::Button("Push mouse click")) {
			BTasEvent ev;
			ev.click.x = mpos[0];
			ev.click.y = mpos[1];
			ev.frame = st.frame;
			ev.idx = 5;
			st.temp_ev.push_back(ev);
		}
		static bool show_rng = false;
		ImGui::Checkbox("Show RNG state (range: values)", &show_rng);
		if (show_rng && !st.rng_buf.empty()) {
			int cur_range = -1;
			string cur_str;
			for (auto it = st.rng_buf.begin(); it != st.rng_buf.end(); it++) {
				if (it->a != cur_range) {
					if (cur_range != -1)
						ImGui::TextUnformatted(cur_str.substr(0, cur_str.size() - 2).c_str());
					cur_range = it->a;
					cur_str = to_str(cur_range) + ": ";
				}
				cur_str += to_str(it->b) + ", ";
			}
			ImGui::TextUnformatted(cur_str.substr(0, cur_str.size() - 2).c_str());
		}
		static int time_val = 0;
		if (ImGui::InputInt("New time", &time_val))
			time_val = std::max(time_val, 0);
		if (ImGui::Button("Push time")) {
			BTasEvent ev;
			ev.tm.val = time_val;
			ev.frame = st.frame;
			ev.idx = 8;
			st.temp_ev.push_back(ev);
		}
		ImGui::Text("Temp event queue size: %i", (int)st.temp_ev.size());
		if (ImGui::Button("Clear temp events"))
			st.temp_ev.clear();
	}
}

void btas::my_mouse_pos(long& x, long& y) {
	x = (long)st.m_pos[0];
	y = (long)st.m_pos[1];
}
