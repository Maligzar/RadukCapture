// ============================================================
//  Phase 155.4: a night's evidence, as data (core/ValidationLog.h)
//
//    1. The night: before noon belongs to the previous date, across a
//       month, a year and a leap day.
//    2. Frames: sharpness binned by exposure and gain, the cap in its own
//       bin; seeing and exposure thinned to one a second; frames with no
//       target or no signal left out.
//    3. Solves and autofocus runs kept; an impossible scale refused.
//    4. The file: valid JSON (read back with core/MiniJson.h), with what
//       tools/validation_report.py reads, and names escaped.
//    5. The caps hold: a long night stays bounded.
// ============================================================

#include "core/ValidationLog.h"
#include "core/MiniJson.h"

#include <cstdio>
#include <string>

using namespace raduk::core::validation;
using raduk::json::JVal;

static int g_fail = 0;
static void Check(bool ok, const std::string& what, const std::string& detail = "") {
    printf("  [%s] %-62s %s\n", ok ? "PASS" : "FAIL", what.c_str(), detail.c_str());
    if (!ok) g_fail++;
}

static Frame F(int64_t ms, const char* tgt, double sharp, double see = 50, double exp = 5, double gain = 300) {
    Frame f;
    f.ms = ms; f.target = tgt; f.sharpnessPct = sharp; f.highlightDN = 200;
    f.normalisedSeeing = see; f.targetHighlightDN = 190; f.targetClipPct = 0.01; f.targetFramePct = 3;
    f.exposureMs = exp; f.gain = gain; f.bandFloorMs = 4; f.bandCeilMs = 9;
    return f;
}

int main() {
    printf("validation_log (phase 155.4)\n");

    printf("\n1. The night\n");
    Check(NightOf(2026, 11, 21, 21) == "20261121", "the evening is its own date");
    Check(NightOf(2026, 11, 22, 3) == "20261121", "after midnight is still the night before");
    Check(NightOf(2026, 12, 1, 2) == "20261130", "across a month");
    Check(NightOf(2027, 1, 1, 0) == "20261231", "across a year");
    Check(NightOf(2028, 3, 1, 5) == "20280229", "a leap day");
    Check(NightOf(2027, 3, 1, 5) == "20270228", "and not in a common year");
    Check(NightOf(2026, 11, 21, 12) == "20261121", "noon starts the new night");

    printf("\n2. Frames\n");
    Recorder r;
    r.Begin("20261121", "3.3.37", 1795300000);
    Check(r.Empty() && !r.Dirty(), "empty at the start");
    for (int i = 0; i < 20; i++) r.AddFrame(F(i * 500, "Jupiter", 40));   // 2 Hz for 10 s
    r.AddFrame(F(10000, "Jupiter", 100));
    r.AddFrame(F(10500, "Jupiter", 130));                                 // over the cap: clamps to 100
    r.AddFrame(F(11000, "Jupiter", 40, 50, 8, 300));                      // another exposure
    r.AddFrame(F(11500, "", 40));                                         // no target
    Frame dark = F(12000, "Jupiter", 40);
    dark.highlightDN = 0;
    r.AddFrame(dark);                                                     // no signal
    const auto& t = r.Targets().at("Jupiter");
    Check(t.framesSeen == 23, "every frame counted, the empty ones not", std::to_string(t.framesSeen));
    Check(t.sharp.size() == 2 && t.sharp.count("5.00|300") && t.sharp.count("8.00|300"),
          "sharpness kept by exposure and gain");
    const auto& h = t.sharp.at("5.00|300");
    Check(h.size() == (size_t)kSharpBins && h[40] == 20 && h[100] == 2, "binned by percent, the cap in bin 100");
    Check(t.seeing.size() == 12, "seeing thinned to one a second", std::to_string(t.seeing.size()));
    Check(t.exposure.size() == 12, "exposure too");
    Check(t.bandFloorMs == 4 && t.bandCeilMs == 9, "the profile's band kept");
    Check(r.Dirty(), "and marked for writing");

    printf("\n3. Solves and autofocus\n");
    Solve s;
    s.unixSec = 1795300100; s.measured = 0.2041; s.configured = 0.203; s.frameW = 3840; s.frameH = 2160; s.target = "Jupiter";
    r.AddSolve(s);
    Solve bad = s;
    bad.measured = 0;
    r.AddSolve(bad);
    Check(r.Solves().size() == 1, "a solve kept, an impossible scale refused");
    FocusRun fr;
    fr.unixSec = 1795300200; fr.target = "Jupiter"; fr.success = true; fr.start = 167000; fr.best = 168010;
    fr.bestSharp = 0.71; fr.improvement = 0.3; fr.backlashMeasured = 152; fr.backlashInUse = 200;
    fr.message = "Complete: \"peak\" at 168010";
    fr.coarsePeak = 167980; fr.finePeak = 168010; fr.fineUsed = true; fr.finePasses = 1;
    fr.fadesCorrected = 0; fr.fadeRatio = 1.03; fr.fadeAction = "steady";
    fr.finePos = {167900, 167950, 168000, 168050, 168100};
    fr.fineScore = {0.21, 0.33, 0.41, 0.34, 0.22};
    fr.fineSpread = {0.03, 0.02, 0.025, 0.02, 0.03};
    r.AddFocus(fr);
    Check(r.Focus().size() == 1, "an autofocus run kept");

    printf("\n4. The file\n");
    const std::string j = r.ToJson();
    JVal root;
    raduk::json::JsonReader rd(j);
    Check(rd.Parse(root) && root.kind == JVal::Obj, "valid JSON");
    const JVal* fmt = root.Get("format");
    Check(fmt && fmt->s == "raduk-validation" && root.Get("night")->s == "20261121", "its format and night");
    const JVal* solves = root.Get("solves");
    Check(solves && solves->a.size() == 1 && solves->a[0].Get("measured")->n == 0.2041 &&
          solves->a[0].Get("configured")->n == 0.203, "the solve, measured and configured");
    const JVal* focus = root.Get("focus");
    Check(focus && focus->a.size() == 1 && focus->a[0].Get("backlash_measured")->n == 152 &&
          focus->a[0].Get("message")->s == "Complete: \"peak\" at 168010", "the run, its message escaped and back");
    const JVal* f0 = focus && !focus->a.empty() ? &focus->a[0] : nullptr;
    Check(f0 && f0->Get("coarse_peak")->n == 167980 && f0->Get("fine_peak")->n == 168010 &&
          f0->Get("fine_used")->b && f0->Get("fade_action")->s == "steady" && f0->Get("fade_ratio")->n == 1.03,
          "155.4b: the fine pass's peaks and fade check");
    Check(f0 && f0->Get("fine_pos")->a.size() == 5 && f0->Get("fine_score")->a[2].n == 0.41 &&
          f0->Get("fine_spread")->a.size() == 5, "155.4b: the fine curve and its spreads");
    const JVal* tj = root.Get("targets") ? root.Get("targets")->Get("Jupiter") : nullptr;
    Check(tj && tj->Get("sharpness")->Get("5.00|300")->a.size() == (size_t)kSharpBins, "the histogram, all 101 bins");
    Check(tj && tj->Get("seeing")->a.size() == 12 && tj->Get("seeing")->a[0].a[0].n == 0.0,
          "seeing as [seconds, value], from 0");
    Check(tj && tj->Get("exposure")->a[0].a.size() == 6, "exposure rows have their six fields");
    r.MarkWritten();
    Check(!r.Dirty(), "written: not dirty until something new arrives");

    printf("\n5. The caps\n");
    Recorder big;
    big.Begin("20261121", "x", 0);
    for (int64_t i = 0; i < (int64_t)kMaxSamplesPerTarget + 500; i++) big.AddFrame(F(i * 1000, "Moon", 30));
    Check(big.Targets().at("Moon").seeing.size() == kMaxSamplesPerTarget, "seeing stops at the cap");
    Check(big.Targets().at("Moon").framesSeen == kMaxSamplesPerTarget + 500, "but frames are still counted");
    for (size_t i = 0; i < kMaxSolves + 10; i++) big.AddSolve(s);
    Check(big.Solves().size() == kMaxSolves, "solves stop at the cap");

    printf("\n%s\n", g_fail ? "SOME CHECKS FAILED" : "ALL CHECKS PASSED");
    return g_fail ? 1 : 0;
}
