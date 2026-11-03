#pragma once
#include <Windows.h>

// A free cursor belongs to menus/buy screens. Never inject gameplay input there.
inline bool gameplay_cursor_captured(DWORD flags,bool queried) {
    return queried && (flags & (CURSOR_SHOWING | CURSOR_SUPPRESSED))==0;
}
inline bool gameplay_cursor_captured() {
    CURSORINFO cursor{sizeof(CURSORINFO)};
    const bool queried=GetCursorInfo(&cursor)!=FALSE;
    return gameplay_cursor_captured(cursor.flags,queried);
}

inline ULONGLONG WINAPI input_clock_ms() {
    static const LONGLONG frequency=[] { LARGE_INTEGER value{}; QueryPerformanceFrequency(&value); return value.QuadPart; }();
    LARGE_INTEGER value{};
    if (!frequency || !QueryPerformanceCounter(&value)) return GetTickCount64();
    return static_cast<ULONGLONG>((value.QuadPart/frequency)*1000 + (value.QuadPart%frequency)*1000/frequency);
}

class PollTimer {
    HANDLE timer_=CreateWaitableTimerExW(nullptr,nullptr,CREATE_WAITABLE_TIMER_HIGH_RESOLUTION,TIMER_ALL_ACCESS);
public:
    PollTimer()=default;
    PollTimer(const PollTimer&)=delete;
    ~PollTimer() { if (timer_) CloseHandle(timer_); }
    void wait(DWORD milliseconds=2) {
        LARGE_INTEGER due{}; due.QuadPart=-static_cast<LONGLONG>(milliseconds)*10000; // independent of presentation/VSync.
        if (timer_ && SetWaitableTimer(timer_,&due,0,nullptr,nullptr,FALSE) && WaitForSingleObject(timer_,milliseconds+25)==WAIT_OBJECT_0) return;
        Sleep(milliseconds);
    }
};

// Production uses Win32. Tests supply a recording sink and deterministic clock.
struct InputApi {
    decltype(&SendInput) send = &SendInput;
    decltype(&GetAsyncKeyState) key = &GetAsyncKeyState;
    decltype(&GetTickCount64) now = &input_clock_ms;
};
