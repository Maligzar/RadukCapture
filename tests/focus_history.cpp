// focus_history: Phase 174.0. Every sweep and the hand corrections after it,
// as JSON lines a later tool (and an LLM) can read. Replays 2026-11-22 23:10:
// autofocus left 165,984; the owner settled on 168,893.
#include "core/FocusHistory.h"
#include <cstdio>
#include <cstring>

using namespace raduk::core::focushist;
static int fails = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); ++fails; } } while (0)

// Enough of a JSON check for one line: balanced braces and brackets outside
// strings, no raw newline, and the keys asked for.
static bool LooksLikeJson(const std::string& s) {
    int b = 0, k = 0; bool inStr = false, esc = false;
    for (char c : s) {
        if (c == '\n') return false;
        if (inStr) { if (esc) esc = false; else if (c == '\\') esc = true; else if (c == '"') inStr = false; continue; }
        if (c == '"') inStr = true;
        else if (c == '{') ++b; else if (c == '}') --b;
        else if (c == '[') ++k; else if (c == ']') --k;
        if (b < 0 || k < 0) return false;
    }
    return b == 0 && k == 0 && !inStr && s.front() == '{' && s.back() == '}';
}

int main() {
    // 1. The night's sweep as a record.
    Sweep s;
    s.unixSec = 1795300223; s.id = "1795300223-168665"; s.appVersion = "3.3.49";
    s.at.target = "Saturn"; s.at.filter = "UV/IR cut (400-700nm)"; s.at.exposureMs = 25.0; s.at.gain = 230;
    s.at.width = 640; s.at.height = 480; s.at.bitDepth = 16; s.at.focuserTempC = 14.0;
    s.success = true; s.start = 168665; s.result = 165984; s.message = "conditions changed during the sweep \"faded\"";
    s.coarsePos = {165665, 166265, 166865}; s.coarseOut = {14.6, 14.8, 14.7}; s.coarseBack = {4.6, 4.6, 4.5};
    s.coarsePeakOut = {200, 207, 209}; s.coarsePeakBack = {49, 53, 55};
    s.sweepRatio = 3.11; s.abRan = true; s.abStartBefore = 14.6; s.abStartNow = 4.6; s.abFit = 4.6;
    s.abVerdict = "kept fit"; s.skyChanged = true;
    const std::string line = SweepJson(s);
    std::printf("  %zu bytes\n", line.size());
    CHECK(LooksLikeJson(line));
    CHECK(line.find("\"kind\":\"sweep\"") != std::string::npos);
    CHECK(line.find("\"peak_back\":[49,53,55]") != std::string::npos);
    CHECK(line.find("\\\"faded\\\"") != std::string::npos);          // quotes escaped
    CHECK(line.find("\"alt_deg\":null") != std::string::npos);        // unknown is null, not 0
    CHECK(line.find("\"out_back_ratio\":3.11") != std::string::npos);

    // 2. The followup: the owner moves the focuser by hand and rests twice.
    FollowupTracker t;
    Followup f;
    const int64_t t0 = s.unixSec;
    t.SweepEnded(s.id, t0, s.result);
    CHECK(!t.Tick(t0 + 1, false, 165984, 4.6, f));
    CHECK(!t.Tick(t0 + 20, false, 167000, 20.0, f));     // moving
    CHECK(!t.Tick(t0 + 22, false, 168000, 60.0, f));     // still moving
    CHECK(!t.Tick(t0 + 28, false, 168000, 80.0, f));     // rested 6 s: a rest
    CHECK(!t.Tick(t0 + 29, false, 168000, 85.0, f));     // the best reading there is kept
    CHECK(!t.Tick(t0 + 40, false, 168893, 90.0, f));
    CHECK(!t.Tick(t0 + 50, false, 168893, 99.0, f));     // rested: the second rest
    CHECK(!t.Tick(t0 + 60, false, 168893, 98.0, f));
    CHECK(t.Tick(t0 + FollowupTracker::kWindowSec, false, 168893, 98.0, f));   // the window ends
    CHECK(f.rests.size() == 2);
    CHECK(f.rests[0].pos == 168000 && f.rests[0].sharpnessPct == 85.0);
    CHECK(f.rests[1].pos == 168893 && f.rests[1].sharpnessPct == 99.0);
    CHECK(f.endedBy == "window");
    const std::string fl = FollowupJson(f);
    CHECK(LooksLikeJson(fl));
    CHECK(fl.find("\"settled\":168893") != std::string::npos);
    CHECK(fl.find("\"moved_from_result\":2909") != std::string::npos);
    CHECK(!t.Open());

    // 3. A new autofocus closes the followup; leaving the result alone is a label too.
    t.SweepEnded("x", 100, 5000);
    CHECK(!t.Tick(110, false, 5000, 50.0, f));
    CHECK(t.Tick(120, true, 5000, 50.0, f));
    CHECK(f.endedBy == "autofocus" && f.rests.empty());
    CHECK(FollowupJson(f).find("\"moved_from_result\":0") != std::string::npos);

    // 4. Nothing open: ticks do nothing.
    FollowupTracker idle;
    CHECK(!idle.Tick(1, false, 1, 1.0, f));

    std::printf("focus_history: %s (%d failures)\n", fails ? "FAIL" : "PASS", fails);
    return fails ? 1 : 0;
}
