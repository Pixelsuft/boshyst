#pragma once
#include "ass.hpp"
#include "mem.hpp"
#include <MinHook.h>

extern bool is_btas;
extern bool is_hourglass;

template <typename A> inline void hook(A pTarget, LPVOID pDetour) {
    ASS(MH_CreateHook(reinterpret_cast<LPVOID>(pTarget), pDetour, nullptr) == MH_OK);
}

template <typename A, typename T> inline void hook(A pTarget, LPVOID pDetour, T* ppOriginal) {
    ASS(MH_CreateHook(reinterpret_cast<LPVOID>(pTarget), pDetour,
                      reinterpret_cast<LPVOID*>(ppOriginal)) == MH_OK);
}

inline void enable_hook() { ASS(MH_EnableHook(MH_ALL_HOOKS) == MH_OK); }

template <typename T> inline void enable_hook(T target) {
    ASS(MH_EnableHook(reinterpret_cast<LPVOID>(target)) == MH_OK);
}

void _reg_iat(const char* dll, const char* func_name, void* pNewFunc, void** ppOriginal);

inline void iat_hook(const char* dll, const char* func_name, LPVOID pDetour) {
    if (!is_btas && !is_hourglass) {
        hook(mem::addr(func_name, dll), pDetour);
        return;
    }
    _reg_iat(dll, func_name, pDetour, nullptr);
}

template <typename T>
inline void iat_hook(const char* dll, const char* func_name, LPVOID pDetour, T* ppOriginal) {
    if (!is_btas && !is_hourglass) {
        hook(mem::addr(func_name, dll), pDetour, ppOriginal);
        return;
    }
    _reg_iat(dll, func_name, pDetour, reinterpret_cast<LPVOID*>(ppOriginal));
}

void enable_iat();
