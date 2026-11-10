#!/usr/bin/env python3
"""Run the actual StatusBar timer methods with deterministic Win32/time doubles.

Only rendering and window creation are excluded. No Windows runtime or Discord
credentials are needed. Run from any directory with Python 3 and a native g++.
"""
from pathlib import Path
import os
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def method(source, signature):
    start = source.index(signature)
    body = source.index("{", start)
    depth = 1
    end = body + 1
    while depth:
        if source[end] == "{":
            depth += 1
        elif source[end] == "}":
            depth -= 1
        end += 1
    return source[start:end]


header = (ROOT / "src/windows/StatusBar.hpp").read_text()
# Retain the real class layout and declarations; expose private methods to tests.
header = "\n".join(line for line in header.splitlines()
                   if not line.startswith(("#include", "#pragma")))
header = header.replace("private:", "public:")
source = (ROOT / "src/windows/StatusBar.cpp").read_text()
program = r'''
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <ctime>
#include <map>
#include <string>
#include <vector>
#include "models/Snowflake.hpp"
#include "state/TypingExpiry.hpp"
#define CALLBACK
#define TRUE 1
#define WM_TIMER 0x113
#define TYPING_INTERVAL 10000
using HWND = void*;
using UINT = unsigned int;
using UINT_PTR = uintptr_t;
using DWORD = unsigned long;
using BOOL = int;
struct RECT { int left, top, right, bottom; };
struct DRAWITEMSTRUCT;
using LPDRAWITEMSTRUCT = DRAWITEMSTRUCT*;
static time_t now = 1700000000;
static time_t fakeTime(time_t*) { return now; }
#define time fakeTime
static std::map<UINT_PTR, int> timers;
static int creates = 0, paints = 0;
static UINT_PTR SetTimer(HWND, UINT_PTR id, int ms, void (*)(HWND, UINT, UINT_PTR, DWORD)) {
    assert(ms > 0); ++creates; timers[id] = ms; return id;
}
static void KillTimer(HWND, UINT_PTR id) { timers.erase(id); }
static void InvalidateRect(HWND, const RECT*, BOOL) { ++paints; }
static BOOL DestroyWindow(HWND) { return 1; }
'''
program += header
program += "\nvoid StatusBar::TimerProc(HWND, UINT, UINT_PTR, DWORD) {}\n"
for name in ["AddTypingName", "RemoveTypingName", "ClearTypers",
             "RemoveTypingNameInternal", "SetExpiryTimerIn", "StartAnimation",
             "StopAnimation", "OnExpiryTick", "SetAnimationVisible",
             "OnAnimationTick"]:
    program += method(source, "void StatusBar::" + name + "(") + "\n"
program += method(source, "int StatusBar::GetNextExpiryTime()") + "\n"
program += method(source, "StatusBar::~StatusBar()") + "\n"
program += r'''
int main() {
    {
        StatusBar bar;
        bar.m_hwnd = reinterpret_cast<HWND>(1);
        bar.AddTypingName(1, now, "Alice");
        assert(timers.count(IDT_EXPIRY) == 1 && timers.count(IDT_ANIMATION) == 0);
        bar.SetAnimationVisible(true);
        assert(timers.count(IDT_ANIMATION) == 1);
        int count = creates;
        bar.StartAnimation();
        assert(creates == count); // no timer recreation for every typing update
        now += 5;
        bar.AddTypingName(2, now, "Bob");
        now += 1;
        bar.OnExpiryTick();
        assert(bar.m_typingUsers.size() == 2 && timers[IDT_EXPIRY] == 4000);
        bar.SetAnimationVisible(false); // hidden or minimized
        assert(timers.count(IDT_ANIMATION) == 0 && bar.m_timerAnimEvent == 0);
        int oldPaints = paints;
        bar.OnAnimationTick(); // queued callback after KillTimer
        assert(paints == oldPaints);
        now += 4;
        bar.OnExpiryTick();
        assert(bar.m_typingUsers.size() == 1 && bar.m_typingUsers[0].m_key == 2);
        bar.SetAnimationVisible(true);
        assert(timers.count(IDT_ANIMATION) == 1);
        bar.RemoveTypingName(2);
        assert(timers.empty() && bar.m_typingUsers.empty());
        bar.AddTypingName(3, now, "Carol");
        now += 3600; // delayed expiry after sleep or a busy UI
        bar.OnExpiryTick();
        assert(timers.empty() && bar.m_typingUsers.empty());
        bar.AddTypingName(4, now, "Dave");
        assert(!timers.empty());
    }
    assert(timers.empty()); // destruction cancels both timers
    { StatusBar failedCreation; } // no initialized window or timers
}
'''
with tempfile.TemporaryDirectory(prefix="dm-status-timers-") as tmp:
    cpp = Path(tmp) / "test.cpp"
    binary = Path(tmp) / "test"
    cpp.write_text(program)
    subprocess.run([os.environ.get("CXX", "g++"), "-std=c++11", "-Wall", "-Wextra",
                    "-I" + str(ROOT / "src/core"), str(cpp), "-o", str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
print("PASS: typing expiry, hidden/visible timers, queued ticks, removal, sleep, destruction")
