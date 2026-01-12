#pragma once
#include <MinHook.h>
#include "ass.hpp"

template<typename A>
inline void hook(A pTarget, LPVOID pDetour) {
	ASS(MH_CreateHook(reinterpret_cast<LPVOID>(pTarget), pDetour, nullptr) == MH_OK);
}

template<typename A, typename T>
inline void hook(A pTarget, LPVOID pDetour, T* ppOriginal) {
	ASS(MH_CreateHook(reinterpret_cast<LPVOID>(pTarget), pDetour, reinterpret_cast<LPVOID*>(ppOriginal)) == MH_OK);
}

inline void enable_hook() {
	ASS(MH_EnableHook(MH_ALL_HOOKS) == MH_OK);
}

template<typename T>
inline void enable_hook(T target) {
	ASS(MH_EnableHook(reinterpret_cast<LPVOID>(target)) == MH_OK);
}
