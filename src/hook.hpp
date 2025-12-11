#pragma once
#include <MinHook.h>
#include "ass.hpp"

template<typename A>
void hook(A pTarget, LPVOID pDetour) {
	ASS(MH_CreateHook(reinterpret_cast<LPVOID>(pTarget), pDetour, nullptr) == MH_OK);
}

template<typename A, typename T>
void hook(A pTarget, LPVOID pDetour, T* ppOriginal) {
	ASS(MH_CreateHook(reinterpret_cast<LPVOID>(pTarget), pDetour, reinterpret_cast<LPVOID*>(ppOriginal)) == MH_OK);
}
