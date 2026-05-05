#pragma once
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <chrono>
#include <algorithm>
using std::max;
using std::min;
// millis() — returns current tick in sim context
// Override this to return g_currentTick for deterministic simulation
extern int g_currentTick;
inline unsigned long millis() { return (unsigned long)g_currentTick; }

// Stub out Serial
struct SerialStub_t
{
    template <typename... Args>
    void printf(const char *fmt, Args... args) { ::printf(fmt, args...); }
    void println(const char *s) { ::printf("%s\n", s); }
} SerialStub;
#define Serial SerialStub