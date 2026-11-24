// recording_cap: RECORD sets the limit to what the derotation window will
// really allow (2026-11-24), so the countdown is right.
#include "core/RecordingCap.h"
#include <cstdio>

using namespace raduk::core;
static int fails = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); ++fails; } } while (0)

int main() {
    // The owner's example: 100,000 asked, a 163 s window at 300 fps.
    RecordingCap c = CapForWindow(true, 100000, 60, 163.0, 300.0, "Saturn");
    std::printf("  %s\n", c.why.c_str());
    CHECK(c.capped && c.frames == 48900);
    CHECK(c.why.find("48900 frames, not 100000") != std::string::npos && c.why.find("2:43") != std::string::npos);
    CHECK(c.why.find("on Saturn") != std::string::npos);

    // A limit inside the window is left alone.
    c = CapForWindow(true, 20000, 60, 163.0, 300.0);
    CHECK(!c.capped && c.frames == 20000 && c.why.empty());
    // Exactly at the window: left alone.
    CHECK(!CapForWindow(true, 48900, 60, 163.0, 300.0).capped);
    // No window (a star, Light off, the stop off): left alone.
    CHECK(!CapForWindow(true, 100000, 60, 0.0, 300.0).capped);
    // No rate yet: no frame figure; the window still stops it.
    c = CapForWindow(true, 100000, 60, 163.0, 0.0);
    CHECK(!c.capped && c.frames == 100000);
    // A fractional rate rounds down: never promise a frame past the window.
    CHECK(CapForWindow(true, 100000, 60, 100.0, 123.456).frames == 12345);

    // Time limits.
    c = CapForWindow(false, 1000, 600, 163.7, 300.0, "Jupiter");
    std::printf("  %s\n", c.why.c_str());
    CHECK(c.capped && c.sec == 163.0 && c.why.find("163 s, not 600 s") != std::string::npos);
    CHECK(!CapForWindow(false, 1000, 120, 163.0, 300.0).capped);
    c = CapForWindow(false, 1000, 0, 163.0, 300.0);
    CHECK(c.capped && c.sec == 163.0);   // no time limit set: the window is the limit

    std::printf("recording_cap: %s (%d failures)\n", fails ? "FAIL" : "PASS", fails);
    return fails ? 1 : 0;
}
