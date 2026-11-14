// ============================================================
//  The fine pass, refitted (phase 161.4)
//
//  1. The order its points are visited: centre-out and alternating, so
//     a slow change in the light falls on both sides of the curve.
//  2. The fit: a parabola through the logarithm of every point within
//     half the top's height, when there are five; else three points.
//  3. An error bar, from resampling each point's frames.
//  4. The real routine: it reports the error bar, lands on focus, and a
//     fade through the pass no longer needs correcting.
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

static double Curve(double p, double focus, double width) {
    const double d = (p - focus) / width;
    return 0.5 + 60.0 * std::exp(-0.5 * d * d);
}

static constexpr int kFocus = 167300;
struct Out { bool done = false; AFResult res; int end = 0; };
static Out Focus(double edgeFadePerSec, unsigned seed) {
    EAFFocuser f; f.Open(0);
    AutofocusRoutine af;
    af.SetFocuser(&f);
    AutofocusParams p;
    p.settleMs = 30;
    p.backlash = 0;
    p.fineStage = true;
    af.SetParams(p);
    af.SetLogger([](const char*) {});
    Out out;
    std::atomic<bool> done{false}, feed{true};
    af.SetOnComplete([&](AFResult r) { out.res = r; done.store(true); });
    std::mt19937 rng(seed);
    const auto t0 = std::chrono::steady_clock::now();
    std::thread feeder([&] {
        std::normal_distribution<double> seeing(1.0, 0.08);
        while (feed.load()) {
            const double el = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
            const double d = (f.GetOpticalPosition() - kFocus) / 1800.0;
            af.SubmitSharpness((float)((6.0 + 235.0 / (1.0 + d * d)) * (1.0 + 0.02 * (seeing(rng) - 1.0) / 0.08)), true, 120);
            for (int k = 0; k < 3; ++k) {
                if (af.WantsFastScores())
                    af.SubmitFastScore((float)(Curve(f.GetOpticalPosition(), kFocus, 250.0) * seeing(rng) *
                                               std::exp(-edgeFadePerSec * el)));
                Ms(5);
            }
        }
    });
    af.Start(f.GetPosition());
    for (int i = 0; i < 12000 && !done.load(); ++i) Ms(10);
    out.done = done.load();
    feed.store(false);
    feeder.join();
    out.end = f.GetOpticalPosition();
    f.Close();
    return out;
}

int main() {
    std::printf("The fine pass, refitted (phase 161.4)\n");
    CoreLogHook() = [](const char*) {};

    std::printf("\n1. The order of the visits\n");
    {
        Check(FineVisitOrder(7) == std::vector<int>({3, 4, 2, 1, 5, 6, 0}), "seven points: centre, then out, the leading side swapping");
        Check(FineVisitOrder(4) == std::vector<int>({2, 3, 1, 0}) && FineVisitOrder(1) == std::vector<int>({0}) &&
              FineVisitOrder(0).empty(), "four, one and none");
        // The 145.5 case: a peak at 300, the light falling 20% across the pass.
        std::vector<int> pos; for (int k = 0; k < 7; ++k) pos.push_back(k * 100);
        auto light = [](double visit) { return 1.0 - 0.2 * visit / 6.0; };
        std::vector<float> oneWay(7), centreOut(7);
        for (int k = 0; k < 7; ++k) oneWay[(size_t)k] = (float)(Curve(pos[(size_t)k], 300, 150) * light(k));
        const std::vector<int> order = FineVisitOrder(7);
        for (int v = 0; v < 7; ++v) {
            const int i = order[(size_t)v];
            centreOut[(size_t)i] = (float)(Curve(pos[(size_t)i], 300, 150) * light(v));
        }
        const FinePeak a = FitFinePeak(pos, oneWay), b = FitFinePeak(pos, centreOut);
        Check(a.ok && a.pos < 300 - 4, "visited one way, a 20% fade pulls the peak toward the first points",
              N(a.pos - 300, 1));
        // Not zero: the two points beside the centre are still one visit
        // apart, and here they are the whole fit. 3% of the spacing.
        Check(b.ok && std::fabs(b.pos - 300) < 3.5 && std::fabs(b.pos - 300) < std::fabs(a.pos - 300) / 2,
              "visited centre-out, the same fade moves it under half as far, uncorrected", N(b.pos - 300, 1));
    }

    {
        // The centre read twice, and what is done about it.
        Check(RepeatVerdict(0.95f) == RepeatAction::None && RepeatVerdict(1.08f) == RepeatAction::None,
              "within 10%: nothing");
        Check(RepeatVerdict(0.5f) == RepeatAction::Detrend && RepeatVerdict(2.5f) == RepeatAction::Detrend,
              "up to a factor of three: detrended, not given up");
        Check(RepeatVerdict(0.3f) == RepeatAction::GiveUp && RepeatVerdict(3.5f) == RepeatAction::GiveUp &&
              RepeatVerdict(0.0f) == RepeatAction::GiveUp, "beyond it, or no reading: given up");
        // A pass in which the light halves, steadily. Nine points, visited
        // centre-out; the centre read first (visit 0) and again (visit 9).
        std::vector<int> pos; for (int k = 0; k < 9; ++k) pos.push_back(k * 60);
        const double focus = 255.0, rate = std::log(2.0) / 9.0;
        const std::vector<int> order = FineVisitOrder(9);
        std::vector<float> raw(9); std::vector<double> t(9);
        for (int v = 0; v < 9; ++v) {
            const int i = order[(size_t)v];
            t[(size_t)i] = v;
            raw[(size_t)i] = (float)(Curve(pos[(size_t)i], focus, 120) * std::exp(-rate * v));
        }
        auto fitWith = [&](float ratio) {
            std::vector<float> sc = raw;
            for (int i = 0; i < 9; ++i) sc[(size_t)i] *= (float)DetrendFactor(ratio, t[(size_t)i], 0.0, 9.0);
            return FitFinePeak(pos, sc).pos - focus;
        };
        const double none = FitFinePeak(pos, raw).pos - focus;
        const double right = fitWith(0.5f), low = fitWith(0.43f), high = fitWith(0.58f), bad = fitWith(0.35f);
        Check(std::fabs(right) < 1.0, "the light halving through the pass, detrended: on the peak", N(right, 2));
        Check(std::fabs(none) < 8.0, "left alone it is not far off either: the order does most of the work",
              N(none, 2));
        Check(std::fabs(low) < 3.0 && std::fabs(high) < 3.0,
              "a ratio misread by 15% either way: still within 3 steps", N(low, 2) + " and " + N(high, 2));
        Check(std::fabs(bad) > std::fabs(none),
              "a ratio misread by a third does more harm than good: why nothing is done inside 10%",
              N(bad, 2) + " against " + N(none, 2) + " untouched");
    }

    std::printf("\n2. The fit\n");
    {
        std::vector<int> pos; for (int k = 0; k < 9; ++k) pos.push_back(1000 + k * 60);
        for (double focus : {1240.0, 1255.0, 1269.0}) {
            std::vector<float> sc;
            for (int p : pos) sc.push_back((float)(60.0 * std::exp(-0.5 * std::pow((p - focus) / 120.0, 2))));
            const FinePeak f = FitFinePeak(pos, sc);
            const CurvePeak three = PeakWindow3(pos, sc);
            Check(f.ok && std::fabs(f.pos - focus) < 0.5 && std::fabs(f.pos - focus) <= std::fabs(three.pos - focus) + 1e-6,
                  "a peak between the points: to half a step, and never worse than three points",
                  N(f.pos - focus, 2) + " with " + N(f.used) + " points (three points: " + N(three.pos - focus, 2) + ")");
        }
        // Noise: the scatter of the result, both ways.
        std::mt19937 rng(5);
        std::normal_distribution<double> noise(1.0, 0.08);
        double s3 = 0, sAll = 0; int n3 = 0, nAll = 0;
        for (int r = 0; r < 2000; ++r) {
            std::vector<float> sc;
            for (int p : pos) sc.push_back((float)(60.0 * std::exp(-0.5 * std::pow((p - 1255.0) / 120.0, 2)) * noise(rng)));
            const CurvePeak three = PeakWindow3(pos, sc);
            const FinePeak f = FitFinePeak(pos, sc);
            if (three.ok) { s3 += (three.pos - 1255) * (three.pos - 1255); ++n3; }
            if (f.ok)     { sAll += (f.pos - 1255) * (f.pos - 1255); ++nAll; }
        }
        const double r3 = std::sqrt(s3 / n3), rAll = std::sqrt(sAll / nAll);
        Check(rAll < 0.7 * r3, "with 8% noise a point, its scatter is well under three points'",
              N(rAll, 1) + " steps against " + N(r3, 1));
        // A curve too wide for the span: fewer than five points inside half
        // height on each side is impossible here, so make it narrow instead.
        std::vector<float> narrow;
        for (int p : pos) narrow.push_back((float)(60.0 * std::exp(-0.5 * std::pow((p - 1250.0) / 35.0, 2)) + 0.01));
        const FinePeak fn = FitFinePeak(pos, narrow);
        Check(fn.ok && fn.used == 3, "a curve with three points inside half height: three points are used",
              N(fn.pos - 1250, 1));
        std::vector<float> rising;
        for (int p : pos) rising.push_back((float)p);
        const FinePeak fr = FitFinePeak(pos, rising);
        Check(!fr.bracketed && !fr.ok && fr.bestIndex == 8, "the best point at an end: not bracketed, and which end");
    }

    std::printf("\n3. The error bar\n");
    {
        std::vector<int> pos; for (int k = 0; k < 9; ++k) pos.push_back(1000 + k * 60);
        std::mt19937 rng(11);
        std::normal_distribution<double> noise(1.0, 0.15);
        // Many passes: the scatter of their results against the bar each reports.
        double sumSq = 0, sumBar = 0; int n = 0;
        for (int r = 0; r < 150; ++r) {
            std::vector<std::vector<float>> frames(9);
            std::vector<float> sc(9);
            for (int i = 0; i < 9; ++i) {
                for (int k = 0; k < 20; ++k)
                    frames[(size_t)i].push_back((float)(60.0 * std::exp(-0.5 * std::pow((pos[(size_t)i] - 1255.0) / 120.0, 2)) * noise(rng)));
                sc[(size_t)i] = ScorePercentile(frames[(size_t)i], 0.75f);
            }
            const FinePeak f = FitFinePeak(pos, sc);
            const FineError e = BootstrapFinePeak(pos, frames, 0.75f, 100, 100 + (unsigned)r);
            if (f.ok && e.ok) { sumSq += (f.pos - 1255) * (f.pos - 1255); sumBar += e.sigma; ++n; }
        }
        const double actual = std::sqrt(sumSq / n), bar = sumBar / n;
        Check(n > 140, "nearly every pass gets a bar", N(n));
        Check(bar > 0.5 * actual && bar < 2.0 * actual,
              "independent frames: the bar is the real scatter, within a factor of two",
              "bar " + N(bar, 1) + ", scatter " + N(actual, 1) + " steps");
        const std::vector<std::vector<float>> empty(9);
        Check(!BootstrapFinePeak(pos, empty, 0.75f).ok, "no frames, no bar");
        // Same seed, same answer.
        std::vector<std::vector<float>> fr(9);
        for (int i = 0; i < 9; ++i)
            for (int k = 0; k < 20; ++k)
                fr[(size_t)i].push_back((float)(60.0 * std::exp(-0.5 * std::pow((pos[(size_t)i] - 1255.0) / 120.0, 2)) * noise(rng)));
        Check(BootstrapFinePeak(pos, fr, 0.75f).sigma == BootstrapFinePeak(pos, fr, 0.75f).sigma,
              "the same pass reads the same twice");
    }

    std::printf("\n4. The routine\n");
    {
        double worst = 0, worstBar = 0; int bars = 0;
        for (unsigned seed : {1u, 2u, 3u}) {
            const Out r = Focus(0.0, seed);
            if (!r.done || !r.res.success) { Check(false, "autofocus finishes", r.res.message); continue; }
            worst = (std::max)(worst, (double)std::abs(r.end - kFocus));
            if (r.res.errorSteps >= 0.0f) { ++bars; worstBar = (std::max)(worstBar, (double)r.res.errorSteps); }
        }
        Check(bars == 3, "three runs: each result carries an error bar", N(bars));
        Check(worst <= 40, "each lands on focus", "worst " + N(worst) + " steps");
        Check(worstBar > 0 && worstBar < 60, "and the bar is a sane number of steps", "largest " + N(worstBar, 1));
    }
    {
        double worst = 0;
        for (unsigned seed : {4u, 5u}) {
            const Out r = Focus(0.06, seed);     // the edge score falling 6% a second
            if (!r.done || !r.res.success) { Check(false, "autofocus finishes through a fade", r.res.message); continue; }
            worst = (std::max)(worst, (double)std::abs(r.end - kFocus));
        }
        Check(worst <= 60, "a 6% a second fade through the fine pass: on focus, nothing corrected",
              "worst " + N(worst) + " steps");
    }

    std::printf("\n%s\n", g_fail ? "FAILED" : "af_fine_pass: all passed");
    return g_fail ? 1 : 0;
}
