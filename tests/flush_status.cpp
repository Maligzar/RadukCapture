// flush_status: the capture panel's line while a stopped recording still
// drains from RAM to disk (2026-11-24: the app froze on STOP).
#include "core/FlushStatus.h"
#include <cstdio>

using namespace raduk::core;
static int fails = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); ++fails; } } while (0)

int main() {
    CHECK(Thousands(0) == L"0" && Thousands(999) == L"999" && Thousands(1000) == L"1,000" &&
          Thousands(12345678) == L"12,345,678");
    // Just stopped: the count and 0%, no estimate yet.
    std::wstring l = FlushLine(20000, 20000, 0.2);
    CHECK(l == L"FLUSHING  20,000 frames  0%");
    // 15 s in, a quarter written: 45 s left.
    l = FlushLine(15000, 20000, 15.0);
    std::printf("  %ls\n", l.c_str());
    CHECK(l == L"FLUSHING  15,000 frames  25%  ~0:45 left");
    // Nothing gone yet after a while: no estimate rather than infinity.
    CHECK(FlushLine(20000, 20000, 30.0).find(L"left") == std::wstring::npos);
    // Minutes.
    CHECK(FlushLine(90000, 100000, 60.0).find(L"~9:00 left") != std::wstring::npos);
    // No start count known: just the count.
    CHECK(FlushLine(500, 0, 5.0) == L"FLUSHING  500 frames");
    std::printf("flush_status: %s (%d failures)\n", fails ? "FAIL" : "PASS", fails);
    return fails ? 1 : 0;
}
