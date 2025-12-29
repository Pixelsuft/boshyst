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

using std::cout;

extern SHORT(__stdcall* GetAsyncKeyStateOrig)(int k);

unsigned long tas_time = 5000000;
bool is_btas = false;

void btas::init() {

}

short btas::TasGetKeyState(int k) {
	if (k == VK_LEFT)
		return 0;
	return GetAsyncKeyStateOrig(k);
}

void btas::on_before_update() {
	// cout << "before update\n";
	tas_time += 1000 / 50;
}
