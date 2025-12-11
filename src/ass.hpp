#pragma once
#include <cassert>

#ifdef _DEBUG
#define ASS(expr) assert(expr)
#else
#include <cstdio>
#define ASS(expr) if (!(expr)) { ass::show_err("ASSERTION FAILED"); printf("ASSERTION FAILED AT %s:%i\n", __FILE__, (int)__LINE__); }
#endif

namespace ass {
	void show_err(const char* text);
}
