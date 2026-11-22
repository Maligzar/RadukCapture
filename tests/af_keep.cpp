// ============================================================
//  Phase 167: the same-sky A/B's verdict (core/FocusDirection.h
//  KeepAfterAB)
//
//  When autofocus's result measures worse than where it began, the start
//  and the result are scored back to back under one sky, and this
//  decides. The owner at the scope, 2026-11-21: autofocus took a nearly
//  focused Saturn to a donut and kept it.
//    1. HFD a tenth better at the start: return, whatever the edge score.
//    2. Near focus (HFD within a tenth): the edge metric judges.
//    3. No edge scores: HFD within 5% keeps the result.
//    4. No reading at the start: never keep blind.
// ============================================================

#include "core/FocusDirection.h"

#include <cstdio>
#include <string>

using namespace raduk::core;

static int g_fail = 0;
static void Check(bool ok, const std::string& what) {
    printf("  [%s] %s\n", ok ? "PASS" : "FAIL", what.c_str());
    if (!ok) g_fail++;
}

int main() {
    printf("af_keep (phase 167)\n");
    // The donut: HFD clearly better at the start, the edge score misled.
    Check(!KeepAfterAB(16.9f, 9.2f, true, 4.0f, 6.0f),
          "1. HFD 84% better at the start, edge score says the donut: return to the start");
    Check(!KeepAfterAB(16.9f, 15.0f, true, 4.0f, 9.0f),
          "1. HFD 13% better at the start: return, even with the edge score for the result");
    Check(KeepAfterAB(16.9f, 15.6f, true, 4.0f, 9.0f),
          "2. HFD within a tenth, edge score better at the result: keep (near focus HFD is flat)");
    Check(!KeepAfterAB(16.9f, 15.6f, true, 9.0f, 4.0f),
          "2. HFD within a tenth, edge score better at the start: return");
    Check(KeepAfterAB(16.0f, 15.5f, false, 0, 0), "3. no edge scores, HFD within 5%: keep");
    Check(!KeepAfterAB(16.0f, 15.0f, false, 0, 0), "3. no edge scores, HFD 7% better at the start: return");
    Check(!KeepAfterAB(0.0f, 15.0f, true, 1.0f, 9.0f), "4. no reading at the start: return rather than keep blind");
    printf("\n%s\n", g_fail ? "SOME CHECKS FAILED" : "ALL CHECKS PASSED");
    return g_fail ? 1 : 0;
}
