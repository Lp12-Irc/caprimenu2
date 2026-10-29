// language: C++20, file: anti_debug.hpp, target: Windows 11 x64, MSVC
// silent exit — no message box, no log
#pragma once
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <intrin.h>

namespace anti_debug {

inline bool debugger_present() {
    return IsDebuggerPresent() != FALSE;
}

inline bool remote_debugger() {
    BOOL r = FALSE;
    CheckRemoteDebuggerPresent(GetCurrentProcess(), &r);
    return r != FALSE;
}

inline bool nt_global_flag() {
    // NtGlobalFlag in PEB — set to 0x70 under debugger
    auto* peb = reinterpret_cast<BYTE*>(__readgsqword(0x60));
    return (*(DWORD*)(peb + 0xBC) & 0x70) != 0;
}

inline bool heap_flag() {
    auto* peb      = reinterpret_cast<BYTE*>(__readgsqword(0x60));
    auto  heap_ptr = *reinterpret_cast<BYTE**>(peb + 0x30);
    // Flags at +0x40, ForceFlags at +0x44 (64-bit heap header)
    DWORD flags        = *reinterpret_cast<DWORD*>(heap_ptr + 0x70);
    DWORD force_flags  = *reinterpret_cast<DWORD*>(heap_ptr + 0x74);
    return (flags & ~0x2) || force_flags;
}

inline bool timing_attack() {
    // rdtsc delta across a Sleep(1) — massively inflated under single-step
    DWORD64 t1 = __rdtsc();
    volatile int x = 0;
    for (int i = 0; i < 1000; ++i) x += i; // tiny loop, ~microseconds native
    DWORD64 t2 = __rdtsc();
    return (t2 - t1) > 2'000'000ULL; // >2M cycles → likely suspended
}

inline void run() {
    if (debugger_present() || remote_debugger() ||
        nt_global_flag()   || heap_flag()       || timing_attack()) {
        ExitProcess(0);
    }
}

// Call every N seconds from a watchdog thread
inline void watchdog_tick() {
    if (debugger_present() || remote_debugger()) ExitProcess(0);
}

} // namespace anti_debug
