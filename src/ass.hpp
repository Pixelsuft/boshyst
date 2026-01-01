#pragma once
#include <cassert>

#ifdef _DEBUG
#define ASS(expr) assert(expr)
#else
#include <cstdio>

#define TO_STRING(x) #x
#define ASS(expr) if (!(expr)) { ass::show_err("ASSERTION FAILED"); printf("ASSERTION FAILED AT %s:%i (%s)\n", __FILE__, (int)__LINE__, TO_STRING(expr)); }
#endif

namespace ass {
	void show_err(const char* text);
}
