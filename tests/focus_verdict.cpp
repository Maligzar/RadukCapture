// ============================================================
//  focus_verdict -- is the target in focus? (2026-11-15)
//
//  The readout under the histogram used to say SHARP / PK 2% / AV 2%,
//  an uncalibrated figure, beside a histogram that looked fine; nobody
//  could tell from it whether the scope was in focus. core/FocusVerdict.h
//  judges the live figure against the one measured just after a good
//  autofocus. Pinned here: no reference means NO REF (never a guess),
//  the reference is the mean of the readings after autofocus, the bands
//  are 85% and 60% of it, and a target change drops it.
// ============================================================

#include "core/FocusVerdict.h"

#include <cmath>
#include <cstdio>
#include <string>

using namespace raduk::core;

static int g_fail = 0;
static void Check(bool ok, const std::string& what) {
    printf("  [%s] %s\n", ok ? "PASS" : "FAIL", what.c_str());
    if (!ok) g_fail++;
}

int main() {
    printf("\n=== Focus verdict ===\n\n");

    FocusReference f;
    Check(f.State() == FocusState::NoReference, "nothing measured: NO REF");
    for (int i = 0; i < 20; ++i) f.Submit(2.0f);
    Check(f.State() == FocusState::NoReference, "readings but no autofocus: still NO REF, never a guess");
    Check(std::fabs(f.Now() - 2.0f) < 1e-4f, "Now is the mean of the recent readings");

    f.ArmAfterAutofocus();
    Check(f.State() == FocusState::Measuring, "just after autofocus: measuring");
    // Five 42s and five 38s: a mean of exactly 40.
    for (int i = 0; i < FocusReference::kRefSamples - 1; ++i) f.Submit(i % 2 ? 38.0f : 42.0f);
    Check(f.State() == FocusState::Measuring, "one reading short of the reference: still measuring");
    f.Submit(38.0f);
    Check(f.HasReference(), "the reference is set after kRefSamples readings");
    Check(std::fabs(f.Reference() - 40.0f) < 1e-3f, "the reference is their mean (40%)");

    for (int i = 0; i < FocusReference::kNowSamples; ++i) f.Submit(38.0f);
    Check(f.State() == FocusState::Focused, "95% of the reference: FOCUSED");
    for (int i = 0; i < FocusReference::kNowSamples; ++i) f.Submit(34.0f);
    Check(f.State() == FocusState::Focused, "85% exactly: FOCUSED");
    for (int i = 0; i < FocusReference::kNowSamples; ++i) f.Submit(30.0f);
    Check(f.State() == FocusState::Soft, "75%: SOFT");
    for (int i = 0; i < FocusReference::kNowSamples; ++i) f.Submit(10.0f);
    Check(f.State() == FocusState::Out, "25%: OUT");

    f.Submit(std::nanf(""));
    f.Submit(-1.0f);
    Check(std::fabs(f.Now() - 10.0f) < 1e-4f, "NaN and negative readings are ignored");

    f.Reset();
    Check(f.State() == FocusState::NoReference && !f.HasReference(),
          "a new target drops the reference");
    Check(f.Now() == 0.0f, "and the recent readings");

    // A reset during measuring abandons it.
    f.ArmAfterAutofocus();
    for (int i = 0; i < 3; ++i) f.Submit(50.0f);
    f.Reset();
    for (int i = 0; i < 20; ++i) f.Submit(50.0f);
    Check(!f.HasReference(), "a target change mid-measurement leaves no reference");

    Check(std::string(FocusStateLabel(FocusState::Focused)) == "FOCUSED" &&
          std::string(FocusStateLabel(FocusState::NoReference)) == "NO REF",
          "the labels the readout draws");

    printf("\n%s\n", g_fail ? "SOME CHECKS FAILED" : "ALL CHECKS PASSED");
    return g_fail ? 1 : 0;
}
