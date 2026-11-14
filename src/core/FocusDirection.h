#pragma once
// ============================================================
//  FocusDirection.h -- phase 139.0 to 139.2: a step number that means
//  one focus position
//
//  The EAF counts motor steps, not mirror position. On the 8SE it turns
//  the stock knob, which slides the primary mirror, and reversing takes
//  up the knob's slack before the mirror moves. So a step number reached
//  moving outward (rising numbers) and the same number reached moving
//  inward are two focus positions, a backlash apart. The owner saw it:
//  returning to the step where focus was sharp gave a blur.
//
//  With a slack of B steps, an outward move leaves the mirror B/2 behind
//  the count and an inward one B/2 ahead. A sweep's outward curve peaks
//  at focus + B/2, its inward curve at focus - B/2, and the average of
//  the two (what autofocus fitted since F9) at about focus itself: the
//  wrong step for a final approach made outward, by B/2, and flatter.
//
//  Here, all pure:
//    PeakWindow3        the vertex through the best point and its two
//                       neighbours (what autofocus already fitted).
//    FitDirections      the outward, inward and averaged peaks, and
//                       their difference: the backlash, mirror included.
//    OneSidedApproach   every move ends travelling outward by at least
//                       the backlash, so the slack is always taken up the
//                       same way (139.2).
//    ApproachSequencer  the same, as the two legs a focuser commands.
//    ResolveBacklash    which figure to use, and says where it came from.
//
//  Gated by tests/af_direction_fit.cpp.
// ============================================================

#include <algorithm>
#include <cstdint>
#include <cmath>
#include <mutex>
#include <random>
#include <vector>

#include "FocusCalibration.h"   // FitParabola (phase 161.4)

namespace raduk::core {

struct CurvePeak {
    bool   ok        = false;   // a concave peak inside the sweep
    bool   bracketed = false;   // the best point is not at either end
    double pos       = 0.0;     // the vertex, in steps
    double height    = 0.0;     // the best measured score
};

// Parabola through the best point and its neighbours, by divided
// differences (positions may be unevenly spaced).
inline CurvePeak PeakWindow3(const std::vector<int>& pos, const std::vector<float>& score) {
    CurvePeak r;
    const int n = (int)(std::min)(pos.size(), score.size());
    if (n < 3) return r;
    int pk = 0;
    for (int i = 1; i < n; i++) if (score[i] > score[pk]) pk = i;
    r.height = score[pk];
    if (pk == 0 || pk == n - 1) return r;
    r.bracketed = true;
    const double x1 = pos[pk-1], y1 = score[pk-1];
    const double x2 = pos[pk],   y2 = score[pk];
    const double x3 = pos[pk+1], y3 = score[pk+1];
    if (x2 - x1 < 1.0 || x3 - x2 < 1.0) return r;
    const double d1 = (y2 - y1) / (x2 - x1);
    const double d2 = (y3 - y2) / (x3 - x2);
    const double a  = (d2 - d1) / (x3 - x1);
    if (a >= -1e-12) return r;
    const double b = d1 - a * (x1 + x2);
    const double v = -b / (2.0 * a);
    if (v < x1 || v > x3) return r;
    r.ok = true;
    r.pos = v;
    return r;
}

struct DirectionFit {
    CurvePeak out, back, mean;
    bool   haveBacklash = false;
    double backlash     = 0.0;   // out peak minus back peak, in steps
};

// outScore[i] and backScore[i] were measured at pos[i], moving outward
// and inward. A score <= 0 means nothing was measured there.
inline DirectionFit FitDirections(const std::vector<int>& pos,
                                  const std::vector<float>& outScore,
                                  const std::vector<float>& backScore) {
    DirectionFit f;
    f.out  = PeakWindow3(pos, outScore);
    f.back = PeakWindow3(pos, backScore);
    std::vector<float> m(pos.size(), 0.0f);
    for (size_t i = 0; i < pos.size(); i++) {
        const float a = i < outScore.size() ? outScore[i] : 0.0f;
        const float b = i < backScore.size() ? backScore[i] : 0.0f;
        const int k = (a > 0) + (b > 0);
        m[i] = k ? (std::max(a, 0.0f) + std::max(b, 0.0f)) / k : 0.0f;
    }
    f.mean = PeakWindow3(pos, m);
    if (f.out.ok && f.back.ok) { f.haveBacklash = true; f.backlash = f.out.pos - f.back.pos; }
    return f;
}

// 139.2. Every move ends travelling the FINAL direction with the slack
// taken up: outward (rising step numbers, dir +1) by default, inward
// (dir -1) when rig.ini FocuserFinalDirection=in (phase 145.4). Which of
// the two pushes the 8SE's primary toward the corrector, against gravity,
// depends on how the EAF is mounted and its reverse setting, so the owner
// sets it once. A move is one leg when it already ends the final way with
// the slack taken up; otherwise it first goes a backlash past the target
// on the other side, and then comes back.
struct OneSidedApproach {
    int  backlash = 0;        // 0: off, every move in one leg
    int  dir      = 1;        // +1 finish outward, -1 finish inward
    bool loadedUp = false;    // the slack is taken up the final way right now

    // The first leg's target, or `to` when one leg will do.
    int ViaFor(int from, int to) const {
        if (backlash <= 0 || to == from) return to;
        const int d = (to - from) * (dir < 0 ? -1 : 1);
        if (d > 0 && (loadedUp || d >= backlash)) return to;
        return dir < 0 ? to + backlash : (std::max)(0, to - backlash);
    }
    // Record a leg that was commanded and completed.
    void Moved(int from, int to) {
        const int d = (to - from) * (dir < 0 ? -1 : 1);
        if (d > 0) { if (loadedUp || d >= backlash) loadedUp = true; }
        else if (d < 0) loadedUp = false;
    }
};

// The legs a focuser commands. Thread-safe: autofocus's thread and the
// UI both poll the focuser.
class ApproachSequencer {
public:
    void SetBacklash(int b) { std::lock_guard<std::mutex> lk(m_mx); m_a.backlash = (std::max)(0, b); }
    int  Backlash() const   { std::lock_guard<std::mutex> lk(m_mx); return m_a.backlash; }
    // Phase 145.4: +1 finish outward, -1 inward. A change forgets the slack.
    void SetDirection(int d) {
        std::lock_guard<std::mutex> lk(m_mx);
        const int nd = d < 0 ? -1 : 1;
        if (nd != m_a.dir) { m_a.dir = nd; m_a.loadedUp = false; }
    }
    int  Direction() const  { std::lock_guard<std::mutex> lk(m_mx); return m_a.dir; }

    // A one-sided move from `from` to `to`: returns the leg to command now.
    int Begin(int from, int to) {
        std::lock_guard<std::mutex> lk(m_mx);
        const int via = m_a.ViaFor(from, to);
        m_pending = (via != to) ? to : -1;
        m_a.Moved(from, via);
        return via;
    }
    // A plain move (autofocus's sweep): no second leg.
    void Direct(int from, int to) {
        std::lock_guard<std::mutex> lk(m_mx);
        m_pending = -1;
        m_a.Moved(from, to);
    }
    // The focuser has stopped at `at`: the next leg to command, or -1.
    int NextLeg(int at) {
        std::lock_guard<std::mutex> lk(m_mx);
        if (m_pending < 0) return -1;
        const int t = m_pending;
        m_pending = -1;
        m_a.Moved(at, t);
        return t;
    }
    // Stopped part way: nothing pending, and the slack's state unknown.
    void Cancel() {
        std::lock_guard<std::mutex> lk(m_mx);
        m_pending = -1;
        m_a.loadedUp = false;
    }
    bool Pending() const { std::lock_guard<std::mutex> lk(m_mx); return m_pending >= 0; }

private:
    mutable std::mutex m_mx;
    OneSidedApproach   m_a;
    int                m_pending = -1;
};

// ── Phase 145.3: the fine stage's arithmetic ────────────────────
//
// The coarse sweep (HFD) finds roughly where focus is and how wide its
// curve is; the fine pass then sweeps a narrow span on the edge metric,
// every point approached the final way, many frames a point.

// The value at fraction p (0..1) of the sorted scores: the 75th
// percentile is the focus as it looks in the steadier moments, which is
// what lucky imaging keeps. 0 for no scores.
inline float ScorePercentile(std::vector<float> v, float p) {
    if (v.empty()) return 0.0f;
    std::sort(v.begin(), v.end());
    const float k = (std::max)(0.0f, (std::min)(1.0f, p)) * (float)(v.size() - 1);
    const size_t i = (size_t)k;
    if (i + 1 >= v.size()) return v.back();
    return v[i] + (v[i + 1] - v[i]) * (k - (float)i);
}

// The fine span: the coarse curve's width where it is within 10% of its
// peak, at least 2 backlashes and 120 steps (or 7 points sit inside the
// slack and seeing), at most two coarse steps (it was bracketed by them).
inline int FineSpanFrom(const std::vector<int>& pos, const std::vector<float>& score, int backlash) {
    const int n = (int)(std::min)(pos.size(), score.size());
    if (n < 2) return 0;
    int lo = pos[0], hi = pos[0];
    for (int i = 1; i < n; ++i) { lo = (std::min)(lo, pos[i]); hi = (std::max)(hi, pos[i]); }
    const int coarseStep = (hi - lo) / (n - 1);
    float pk = score[0];
    for (int i = 1; i < n; ++i) pk = (std::max)(pk, score[i]);
    int wlo = INT32_MAX, whi = INT32_MIN;
    for (int i = 0; i < n; ++i)
        if (score[i] >= 0.9f * pk) { wlo = (std::min)(wlo, pos[i]); whi = (std::max)(whi, pos[i]); }
    int span = (whi > wlo) ? whi - wlo : 2 * coarseStep;
    const int floorSpan = (std::max)(2 * backlash, 120);
    const int ceilSpan  = (std::max)(floorSpan, 2 * coarseStep);
    return (std::max)(floorSpan, (std::min)(span, ceilSpan));
}

// The fine pass's positions in the order they are visited: `points`
// across `span` centred on `center`, travelling the final way (rising for
// dir +1), kept within [minStep, maxStep].
inline std::vector<int> FineSweepPositions(int center, int span, int points, int dir,
                                           int minStep, int maxStep) {
    std::vector<int> out;
    if (points < 3 || span <= 0) return out;
    int lo = center - span / 2, hi = lo + span;
    if (lo < minStep) { hi += minStep - lo; lo = minStep; }
    if (hi > maxStep) { lo -= hi - maxStep; hi = maxStep; }
    lo = (std::max)(lo, minStep);
    for (int i = 0; i < points; ++i)
        out.push_back(lo + (int)std::lround((double)(hi - lo) * i / (points - 1)));
    if (dir < 0) std::reverse(out.begin(), out.end());
    return out;
}

// ── Phase 145.5: a fade during the fine pass ────────────────────
//
// The fine pass visits its points one way only, so a target dimming as
// it goes (dew, haze: 2026-11-08) lowers the later points and pulls the
// fit toward the first. The coarse sweep cancels that by averaging out
// and back; the fine pass instead scores its centre point again at the
// end, and the ratio of the two readings says how much the light moved.

// Second centre reading over the first; 1 when either is missing.
inline float FadeRatio(float firstCentre, float againCentre) {
    if (!(firstCentre > 0.0f) || !(againCentre > 0.0f)) return 1.0f;
    return againCentre / firstCentre;
}

enum class FadeAction { None, Correct, GiveUp };

// Within 10%: seeing alone does that. Under 60%: too much changed to fit.
inline FadeAction FadeVerdict(float ratio) {
    // UNVERIFIED(item 58): the fine-pass fade-check ratio on a clear, steady night
    if (ratio >= 0.9f && ratio <= 1.0f / 0.9f) return FadeAction::None;
    // UNVERIFIED(item 58): a real fade (dew or cloud) during a fine pass, and whether correcting helped
    if (ratio >= 0.6f && ratio <= 1.0f / 0.6f) return FadeAction::Correct;
    return FadeAction::GiveUp;
}

// Undo a fade taken as linear in time. The centre was read at tFirst and
// again at tEnd, `ratio` apart, so the light at time t relative to tFirst
// is 1 + (ratio - 1) * (t - tFirst) / (tEnd - tFirst); every score is
// brought to the light at tFirst.
inline std::vector<float> FadeCorrect(const std::vector<float>& score, const std::vector<double>& t,
                                      float ratio, double tFirst, double tEnd) {
    std::vector<float> out(score);
    if (!(tEnd > tFirst)) return out;
    for (size_t i = 0; i < out.size() && i < t.size(); ++i) {
        const double f = 1.0 + ((double)ratio - 1.0) * (t[i] - tFirst) / (tEnd - tFirst);
        if (f > 0.05) out[i] = (float)(out[i] / f);
    }
    return out;
}

// Phase 161.2: how far a one-sided move goes past its target before it
// comes back. Unlike the backlash figure this has only to be LARGER than
// the slack; too large costs a second or two a move, too small leaves the
// mirror somewhere inside the slack and a step number stops meaning one
// focus position. rigSteps < 0 means rig.ini does not set it: three times
// the backlash figure, and not less than 1,500 steps (a quarter of a turn
// of the 8SE's knob at the EAF's 5,760 steps a turn).
// UNVERIFIED(item 59): the default overshoot exceeds the 8SE's real slack (reverse in 10-step moves until the image changes)
inline constexpr int kMinDefaultOvershootSteps = 1500;
inline int ResolveOvershoot(int rigSteps, int backlash, const char** source) {
    if (rigSteps >= 0) { if (source) *source = "rig.ini FocuserOvershootSteps"; return rigSteps; }
    if (source) *source = "3 x the backlash figure, at least 1500, not measured";
    return (std::max)(3 * (std::max)(0, backlash), kMinDefaultOvershootSteps);
}

// ── Phase 161.4: the fine pass, refitted ────────────────────────
//
// Three changes to how the fine pass turns its points into a position,
// from the review of 2026-11-14.
//
// 1. The order the points are VISITED. One way across, a slow change in
//    the light or the seeing tilts the whole curve and moves its peak;
//    145.5 measured that by reading the centre twice and "corrected" it
//    with a straight line, on one noisy ratio. Visited centre-out, the
//    same slow change falls about equally on both sides: it narrows the
//    curve a little and leaves the peak alone. The side that goes first
//    swaps with each ring (above, below; below, above; ...): with the
//    same side always first, that side would always be the brighter by
//    one visit. What is left is the pair beside the centre, one visit
//    apart: a 20% fade across a seven-point pass moves the peak 2.8
//    steps of a 100-step spacing, where one way across moved it 7.5.
//    Every point is approached the final way, so the order costs only
//    the reversals.
inline std::vector<int> FineVisitOrder(int n) {
    std::vector<int> order;
    if (n <= 0) return order;
    const int c = n / 2;
    order.push_back(c);
    for (int k = 1; (int)order.size() < n; ++k) {
        const int first = (k % 2) ? c + k : c - k, second = (k % 2) ? c - k : c + k;
        if (first >= 0 && first < n)   order.push_back(first);
        if (second >= 0 && second < n) order.push_back(second);
    }
    return order;
}

// 2. The fit. A parabola through the best point and its two neighbours
//    uses three of the points. When five or more lie within half the
//    top's height, round the best, a parabola through the LOGARITHM of
//    all of them (a Gaussian) uses them all: on a simulated pass with 8%
//    noise a point it cut the scatter of the result from about 25 steps
//    to 5. When the curve is wider than the span that fit is poorly
//    conditioned (it was worse than three points), which is why it is
//    taken only with five points inside half height and a vertex within
//    one and a half spacings of the best point. Otherwise, three points.
struct FinePeak {
    bool   ok        = false;   // a position was fitted
    bool   bracketed = false;   // the best point is not at an end
    double pos       = 0.0;
    int    used      = 0;       // points in the fit
    int    bestIndex = 0;
};

// pos ascending.
inline FinePeak FitFinePeak(const std::vector<int>& pos, const std::vector<float>& score) {
    FinePeak r;
    const int n = (int)(std::min)(pos.size(), score.size());
    if (n < 3) return r;
    int pk = 0;
    for (int i = 1; i < n; ++i) if (score[i] > score[pk]) pk = i;
    r.bestIndex = pk;
    if (pk == 0 || pk == n - 1) return r;
    r.bracketed = true;
    const float top = score[pk];
    if (top > 0.0f) {
        int lo = pk, hi = pk;
        while (lo > 0 && score[lo - 1] > 0.0f && score[lo - 1] >= 0.5f * top) --lo;
        while (hi < n - 1 && score[hi + 1] > 0.0f && score[hi + 1] >= 0.5f * top) ++hi;
        if (hi - lo + 1 >= 5 && lo < pk && pk < hi) {
            std::vector<double> x, y;
            for (int i = lo; i <= hi; ++i) { x.push_back((double)pos[i] - pos[pk]); y.push_back(std::log((double)score[i])); }
            double A = 0, B = 0, C = 0;
            if (FitParabola(x, y, A, B, C) && A < 0.0) {
                const double v = -B / (2.0 * A);
                const double spacing = (std::max)((double)(pos[pk] - pos[pk - 1]), (double)(pos[pk + 1] - pos[pk]));
                if (std::fabs(v) <= 1.5 * spacing) {
                    r.ok = true; r.pos = pos[pk] + v; r.used = hi - lo + 1;
                    return r;
                }
            }
        }
    }
    const CurvePeak c = PeakWindow3(pos, score);
    if (c.ok) { r.ok = true; r.pos = c.pos; r.used = 3; }
    return r;
}

// 3. An error bar. Each point's score is a percentile of its frames; the
//    frames are resampled with replacement, the percentile and the fit
//    redone, a few hundred times. Half the spread between the 16th and
//    84th percentiles of the fitted positions is the figure. It is a
//    LOWER bound: frames a few tens of milliseconds apart share their
//    seeing, so they are not as independent as the resampling assumes,
//    and nothing here knows about the mirror's own repeatability
//    (CALIBRATE FOCUS measures that). A fixed seed, so a run reads the
//    same twice.
struct FineError {
    bool   ok    = false;
    double sigma = 0.0;   // steps
    int    fits  = 0;
};
inline FineError BootstrapFinePeak(const std::vector<int>& pos,
                                   const std::vector<std::vector<float>>& frames,
                                   float percentile, int rounds = 200, unsigned seed = 16141) {
    FineError e;
    const size_t n = (std::min)(pos.size(), frames.size());
    if (n < 3 || rounds < 20) return e;
    for (size_t i = 0; i < n; ++i) if (frames[i].empty()) return e;
    std::mt19937 rng(seed);
    std::vector<double> got;
    std::vector<float> sc(n), pick;
    for (int r = 0; r < rounds; ++r) {
        for (size_t i = 0; i < n; ++i) {
            const size_t m = frames[i].size();
            pick.resize(m);
            std::uniform_int_distribution<size_t> d(0, m - 1);
            for (size_t k = 0; k < m; ++k) pick[k] = frames[i][d(rng)];
            sc[i] = ScorePercentile(pick, percentile);
        }
        const FinePeak p = FitFinePeak(pos, sc);
        if (p.ok) got.push_back(p.pos);
    }
    e.fits = (int)got.size();
    if (e.fits < rounds / 2) return e;   // most resamples did not even bracket: no figure
    std::sort(got.begin(), got.end());
    const double lo = got[(size_t)(0.16 * (got.size() - 1))];
    const double hi = got[(size_t)(0.84 * (got.size() - 1))];
    e.sigma = 0.5 * (hi - lo);
    e.ok = true;
    return e;
}

// Which backlash figure to use until the sky measures one. rigSteps < 0
// means rig.ini does not set it.
inline int ResolveBacklash(int rigSteps, int firmwareSteps, const char** source) {
    if (rigSteps >= 0)     { if (source) *source = "rig.ini FocuserBacklashSteps"; return rigSteps; }
    if (firmwareSteps > 0) { if (source) *source = "the EAF's own setting"; return firmwareSteps; }
    if (source) *source = "the default 200, not measured";
    return 200;
}

}  // namespace raduk::core
