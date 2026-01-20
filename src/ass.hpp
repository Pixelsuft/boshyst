#pragma once
#include <cassert>

#ifdef _DEBUG
#define ASS(expr) assert(expr)
#else
#include <cstdio>

#define TO_STRING(x) #x
#define ASS(expr) do { if (!(expr)) { printf("ASSERTION FAILED AT %s:%i (%s)\n", __FILE__, (int)__LINE__, TO_STRING(expr)); ass::show_err("ASSERTION FAILED AT " __FILE__ " (" __func__ ")"); } } while (0)
#endif

namespace ass {
	void show_err(const char* text);
}
