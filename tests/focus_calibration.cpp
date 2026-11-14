// ============================================================
//  CALIBRATE FOCUS (phase 161.3)
//
//  What one focuser step does: the growth of the half-flux diameter with
//  defocus (and so the focal-plane travel per step and the focus zone in
//  steps), the knob's slack, and how well a position repeats.
//
//  1. The arithmetic (FocusCalibration.h), on figures worked by hand.
//  2. The real routine on the emulated focuser, with a mirror that has a
//     slack the routine is not told and an image whose blur per step is
//     known: it must give both back, and end where it began.
//  3. What it says when it cannot measure.
// ============================================================

#define RADUK_EMULATOR 1
#include "core/AutofocusRoutine.h"

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <random>
#include <string>
#include <thread>

using namespace raduk::core;

static int g_fail = 0;
static void Check(bool ok, const char* what, const std::string& detail = "") {
    std::printf("  [%s] %-66s %s\n", ok ? "PASS" : "FAIL", what, detail.c_str());
    if (!ok) ++g_fail;
}
static std::string N(double v, int dp = 0) { char b[40]; std::snprintf(b, 40, "%.*f", dp, v); return b; }
static void Ms(int ms) { std::this_thread::sleep_for(std::chrono::milliseconds(ms)); }
static bool Near(double a, double b, double frac) { return std::fabs(a - b) <= frac * std::fabs(b); }

static constexpr int    kHome = 166500;     // where the emulator opens: in focus
static constexpr double kH0   = 60.0;       // the target's half-flux diameter in focus, px
static constexpr double kHfdPerStep = 0.12; // its growth far out, px a step

struct Rig { int slack = 600; bool hfd = true; bool edge = true; double seeing = 0.015; int overshoot = 1500; };
struct Out { bool done = false; AFResult res; FocusCalibration cal; int endMirror = 0, endStep = 0; double sec = 0; };

static Out Calibrate(const Rig& rig) {
    EAFFocuser f; f.Open(0);
    f.SetSimulatedSlack(rig.slack);
    // In focus as an outward approach leaves it: the mirror half a slack
    // behind the count. The image is sharp where the mirror is NOW.
    f.MoveTo(kHome - 3000); while (f.IsMoving()) Ms(5);
    f.MoveTo(kHome);        while (f.IsMoving()) Ms(5);
    const int focusMirror = f.GetOpticalPosition();

    AutofocusRoutine af;
    af.SetFocuser(&f);
    AutofocusParams p;
    p.settleMs = 30;
    p.backlash = 200;          // what the app believes
    p.overshoot = rig.overshoot;
    p.fineFrames = 12;
    af.SetParams(p);
    af.SetLogger([](const char* s) { std::printf("%s", s); });
    Out out;
    std::atomic<bool> done{false}, feed{true};
    af.SetOnComplete([&](AFResult r) { out.res = r; done.store(true); });
    std::mt19937 rng(rig.slack + 3);
    std::thread feeder([&] {
        std::normal_distribution<double> seeing(1.0, rig.seeing);
        while (feed.load()) {
            const double d = f.GetOpticalPosition() - focusMirror;
            if (rig.hfd) {
                const double h = std::sqrt(kH0 * kH0 + kHfdPerStep * kHfdPerStep * d * d) * seeing(rng);
                af.SubmitSharpness((float)(1000.0 / h), true, 120);
            }
            for (int k = 0; k < 3; ++k) {
                if (rig.edge && af.WantsFastScores()) {
                    const double e = (f.GetOpticalPosition() - focusMirror) / 350.0;
                    af.SubmitFastScore((float)((0.5 + 60.0 / (1.0 + e * e)) * seeing(rng)));
                }
                Ms(4);
            }
        }
    });
    const auto t0 = std::chrono::steady_clock::now();
    af.StartCalibration(f.GetPosition());
    for (int i = 0; i < 60000 && !done.load(); ++i) Ms(10);
    out.sec = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    out.done = done.load();
    feed.store(false);
    feeder.join();
    out.cal = af.Calibration();
    out.endMirror = f.GetOpticalPosition() - focusMirror;
    out.endStep = f.GetPosition();
    f.Close();
    return out;
}

int main() {
    std::printf("CALIBRATE FOCUS (phase 161.3)\n");
    CoreLogHook() = [](const char*) {};

    std::printf("\n1. The arithmetic\n");
    {
        Check(Near(AnnulusHfdFactor(0.0), 0.7071, 1e-3) && Near(AnnulusHfdFactor(0.34), 0.7469, 1e-3),
              "an annulus's half-flux diameter: 0.707 of it unobstructed, 0.747 at 34%");
        // 0.127 px of HFD a step, 2.0 um pixels, f/10, 34% obstruction:
        // blur 0.170 px a step = 0.340 um of blur = 3.40 um of focal plane.
        Check(Near(UmPerStep(0.127, 2.0, 10.0, 0.34), 3.40, 0.01), "0.127 px a step is 3.40 um a step at f/10",
              N(UmPerStep(0.127, 2.0, 10.0, 0.34), 3));
        Check(Near(CriticalZoneUm(10.0), 268.4, 1e-3), "the focus zone at f/10 and 550 nm is 268 um",
              N(CriticalZoneUm(10.0), 1));
        FocusCalibration c; c.haveScale = true; c.hfdPxPerStep = 0.127;
        const FocusScale s = ScaleFrom(c, 2.0, 10.0, 0.34);
        Check(Near(s.zoneSteps, 78.9, 0.01), "which is 79 steps", N(s.zoneSteps, 1));
        Check(UmPerStep(0.0, 2.0, 10.0, 0.34) == 0.0 && UmPerStep(0.1, 0.0, 10.0, 0.34) == 0.0,
              "nothing is made of a missing figure");
    }
    {
        // A hyperbola, sampled as the routine samples it.
        const double h0 = 60, k = 0.12; const int x0 = 168000, R = 800;
        std::vector<int> pos; std::vector<float> hfd;
        pos.push_back(x0); hfd.push_back((float)h0);
        for (double f : {1.0, 4.0 / 3, 5.0 / 3, 2.0})
            for (int sgn : {-1, 1}) {
                const int x = x0 + sgn * (int)std::lround(R * f);
                pos.push_back(x);
                hfd.push_back((float)std::sqrt(h0 * h0 + k * k * (double)(x - x0) * (x - x0)));
            }
        const WingFit w = FitWings(pos, hfd);
        Check(w.ok && Near(w.hfdPxPerStep, k, 0.005), "the wings of a hyperbola give its asymptote back",
              N(w.hfdPxPerStep, 5));
        Check(std::fabs(w.crossing - x0) < 2.0, "and where focus is", N(w.crossing - x0, 1));
        Check(w.leftSlope < k && w.rightSlope < k, "where a straight line through them reads low",
              N(w.leftSlope, 5));
        // One wing only.
        std::vector<int> p1; std::vector<float> h1;
        for (size_t i = 0; i < pos.size(); ++i) if (pos[i] >= x0) { p1.push_back(pos[i]); h1.push_back(hfd[i]); }
        Check(!FitWings(p1, h1).ok, "one wing alone is refused");
        // Wings that disagree.
        std::vector<float> h2 = hfd;
        for (size_t i = 0; i < pos.size(); ++i) if (pos[i] > x0) h2[i] *= 2.2f;
        const WingFit bad = FitWings(pos, h2);
        Check(!bad.ok, "wings whose slopes disagree by more than 40% are refused", bad.why);
        // Not far enough out.
        std::vector<float> h3 = hfd;
        for (float& v : h3) v = (float)(h0 * 1.2);
        h3[0] = (float)h0;
        Check(!FitWings(pos, h3).ok, "points where the blur has not outgrown the target are not wings");
    }
    {
        // Base 20, flat for 590 steps of reversal, then climbing 0.05 a step.
        std::vector<int> t; std::vector<float> s;
        for (int x = 10; x <= 800; x += 10) { t.push_back(x); s.push_back(x <= 590 ? 20.0f : 20.0f + 0.05f * (x - 590)); }
        const SlackFit f = SlackFromProbes(t, s, 20.0f, 1.6f, 0.05);
        Check(f.found && std::fabs(f.steps - 590) <= 10, "a reversal that answers after 590 steps: 590",
              N(f.steps));
        std::vector<float> flat(t.size(), 20.0f);
        const SlackFit none = SlackFromProbes(t, flat, 20.0f, 1.6f, 0.05);
        Check(!none.found && none.probedTo == 800, "no answer: not found, and how far it looked");
        std::vector<float> spike = flat; spike[20] = 30.0f;
        Check(!SlackFromProbes(t, spike, 20.0f, 1.6f, 0.05).found, "one reading out of line is seeing, not the mirror");
    }

    std::printf("\n2. The routine, on a focuser with 600 steps of slack it is not told\n");
    {
        const Out r = Calibrate(Rig{});
        Check(r.done && r.res.success && r.cal.ok, "it finishes", r.cal.message);
        Check(r.cal.haveScale && Near(r.cal.hfdPxPerStep, kHfdPerStep, 0.08),
              "HFD's growth per step, within 8%", N(r.cal.hfdPxPerStep, 4) + " against " + N(kHfdPerStep, 4));
        Check(r.cal.haveSlack && std::fabs(r.cal.slackSteps - 600) <= 60,
              "the slack, within 60 steps", N(r.cal.slackSteps));
        Check(r.cal.haveRepeat && r.cal.repeats == 5 && r.cal.repeatScatterPct < 15.0,
              "five approaches to one step agree", N(r.cal.repeatScatterPct, 1) + "%");
        Check(r.endStep == kHome && std::abs(r.endMirror) <= 2, "the focuser ends where it began, mirror and count",
              N(r.endMirror));
        Check(r.cal.returnRatio > 0.85 && r.cal.returnRatio < 1.15, "as sharp as it was",
              N(r.cal.returnRatio, 2));
        const FocusScale s = ScaleFrom(r.cal, 2.0, 10.0, 0.34);
        const std::string block = FormatCalibration(r.cal, s, "2026-11-14T20:00:00Z", 9.5f, true, 52.0,
                                                    "Saturn", 2.0, 10.0, 0.34);
        Check(block.find("[calibration 2026-11-14T20:00:00Z]") == 0 &&
              block.find("um_per_step=") != std::string::npos &&
              block.find("critical_zone_steps=") != std::string::npos &&
              block.find("slack_steps=") != std::string::npos &&
              block.find("focuser_temp_c=9.5") != std::string::npos,
              "and the block for focus_calibration.ini says so");
        std::printf("         %s\n         took %.0f s here (the emulated motor is quick; moves dominate on the rig)\n",
                    CalibrationSummary(r.cal, s).c_str(), r.sec);
    }

    std::printf("\n3. What it says when it cannot measure\n");
    {
        Rig moon; moon.hfd = false;          // a target that fills the frame: no half-flux diameter
        const Out r = Calibrate(moon);
        Check(r.done && r.cal.haveSlack && !r.cal.haveScale, "no disc: the slack still, the scale not",
              r.cal.message);
        Check(r.cal.message.find("fills the frame") != std::string::npos, "and it says why");
        Check(r.endStep == kHome, "back where it began");
        const FocusScale s = ScaleFrom(r.cal, 2.0, 10.0, 0.34);
        const std::string block = FormatCalibration(r.cal, s, "t", 0, false, -99, "", 2.0, 10.0, 0.34);
        Check(block.find("um_per_step") == std::string::npos && block.find("focuser_temp_c") == std::string::npos &&
              block.find("altitude_deg") == std::string::npos,
              "what was not measured is left out of the block, not written as 0");
    }
    {
        Rig blind; blind.edge = false;       // nothing to read at all
        const Out r = Calibrate(blind);
        Check(r.done && !r.res.success && !r.cal.ok, "no edge score at the start: it stops and says so",
              r.res.message);
        Check(std::abs(r.endStep - kHome) <= 2, "without having gone anywhere");
    }
    {
        // A slack of 2,500 with the overshoot set to 800: the reversal
        // probes three overshoots (2,400) and gets no answer.
        Rig wide; wide.slack = 2500; wide.overshoot = 800;
        const Out r = Calibrate(wide);
        Check(r.done && !r.cal.haveSlack && r.cal.slackAtLeast >= 2300 && r.cal.slackAtLeast <= 2500,
              "a slack beyond the reversal's reach: not guessed, and how far it looked",
              N(r.cal.slackAtLeast));
        Check(r.cal.message.find("more than the reversal probed") != std::string::npos,
              "said in words too", r.cal.message);
        Check(r.endStep == kHome, "back where it began");
    }

    std::printf("\n%s\n", g_fail ? "FAILED" : "focus_calibration: all passed");
    return g_fail ? 1 : 0;
}
