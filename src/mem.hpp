#pragma once
#include <cstddef>
#include "ass.hpp"

namespace mem {
	size_t get_base(const char* obj_name = nullptr);
	void* addr(const char* func_name, const char* obj_name = nullptr);
	void* ptr_from_offsets(const size_t* offsets, size_t n);
}
