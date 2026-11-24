// clock_text: the clock on the status bar and the target panel (2026-11-24).
#include "core/ClockText.h"
#include <cstdio>
#include <cstdlib>

using namespace raduk::core;
static int fails = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); ++fails; } } while (0)

int main() {
    std::tm l{}, u{};
    l.tm_hour = 21; l.tm_min = 43; l.tm_sec = 7;
    u.tm_hour = 4;  u.tm_min = 43; u.tm_sec = 7;
    CHECK(ClockLineFrom(l, u) == L"21:43:07  ·  04:43 UTC");
    l.tm_hour = 0; l.tm_min = 5; l.tm_sec = 9;
    u.tm_hour = 7; u.tm_min = 5;
    CHECK(ClockLineFrom(l, u) == L"00:05:09  ·  07:05 UTC");
    // A real instant: 2026-11-24 04:43:07 UTC, in a zone 7 hours behind.
#ifndef _WIN32
    setenv("TZ", "UTC+7", 1); tzset();
    CHECK(ClockLine((std::time_t)1795495387) == L"21:43:07  ·  04:43 UTC");
#endif
    std::printf("clock_text: %s (%d failures)\n", fails ? "FAIL" : "PASS", fails);
    return fails ? 1 : 0;
}
