#pragma once
// ============================================================
// NightPlan.h -- Phase 163: a night of planets and stars
//
// The Passes tab plans a night of fast movers (SatPlan.h). This plans
// the slow ones: each planet or star ticked on the Tonight tab gets one
// visit, placed while it is in GOOD (45-60 deg) or PRIME (60 deg and up),
// the owner's windows from SkyConditions.h.
//
// ── Choosing ────────────────────────────────────────────────
//
// The night is cut into minutes. For each target, a minute is usable when
// the target is at 45 deg or more at both ends of it, and the Sun rules
// allow it (the Sun below NightPlanSunMaxDeg, and not up within
// NightPlanSunAvoidDeg of the target). It is PRIME when the target is at
// 60 deg or more at both ends. Positions come from the caller's sampler,
// which works them out for each minute: the mount plays no part.
//
// A visit is an overhead block (slew, lock, focus) and then the imaging.
// The overhead may fall while the target is still climbing; the imaging
// may not. Neither may touch another visit or a ticked pass.
//
// For one target on a timeline with some minutes taken, the best place
// is: the longest imaging stretch up to the visit length (so a full visit
// beats a shortened one), then the most PRIME minutes, then the earliest
// start. Below the shortest visit worth a slew it is not placed.
//
// The order targets are placed in matters, so with six or fewer every
// order is tried and the plan kept is the one that places the most
// targets, then has the most PRIME minutes, then the most imaging
// minutes. The first order to reach the best score wins, so the answer is
// the same every time. With more, the target with the fewest usable
// minutes goes first.
//
// ── Running ─────────────────────────────────────────────────
//
// Pure, as SatPlanRunner and MoonMapRunner are. Tick() returns a Step and
// the caller performs it. What a failure means:
//   - a slew refused, no lock, the runs refused: this target is skipped,
//     the plan goes on;
//   - a slew that never finishes: a fault in the telescope, which the next
//     target would meet again, so the plan fails;
//   - a person stopping the runs: the plan stops.
// ============================================================

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <functional>
#include <string>
#include <vector>

#include "SkyConditions.h"

namespace raduk::core {

// Where a target is at one moment, and where the Sun is.
struct NightSample {
    double altDeg    = -90.0;
    double sunAltDeg = -90.0;
    double sunSepDeg = 180.0;   // target to Sun
};

struct NightPlanOptions {
    double stepMin       = 1.0;
    double visitMin      = 20.0;   // imaging wanted a visit
    double minVisitMin   = 5.0;    // the shortest worth a slew
    double slewSec       = 90.0;
    double lockSec       = 180.0;
    double focusSec      = 120.0;
    double sunMaxAltDeg  = 90.0;   // 90: daylight allowed
    double sunAvoidDeg   = 30.0;   // never this close to a risen Sun
    double passMarginSec = 120.0;  // either side of a ticked pass
    int    exhaustiveMax = 6;      // try every order up to this many targets

    double OverheadSec() const { return slewSec + lockSec + focusSec; }
};

inline constexpr double NIGHT_GOOD_DEG  = 45.0;
inline constexpr double NIGHT_PRIME_DEG = 60.0;

struct NightTarget {
    std::string name;
    // Only planets and stars are planned. The caller says which is which,
    // and why not, so the plan can list it.
    bool        plannable = true;
    std::string notPlannable;
};

// A ticked pass, as given; the margin is added here.
struct NightBusy {
    double      startJd = 0.0, endJd = 0.0;
    std::string label;
};

struct NightVisit {
    int         target  = -1;
    std::string name;
    double      slewJd  = 0.0;    // the slew begins
    double      imageJd = 0.0;    // the first run may begin
    double      endJd   = 0.0;    // the visit ends, runs or not
    int         primeMin = 0;
    int         goodMin  = 0;
    double      peakAltDeg = 0.0; // during the imaging
    double      wantedMin  = 0.0;
    bool        shortened  = false;

    SkyWindow Window() const { return primeMin > 0 ? SkyWindow::Prime : SkyWindow::Good; }
    int  ImagingMin() const { return primeMin + goodMin; }

    // "PRIME 14 min, GOOD 6 min, peak 63 deg"
    std::string Why() const {
        char b[160];
        if (primeMin > 0 && goodMin > 0)
            std::snprintf(b, sizeof(b), "PRIME %d min, GOOD %d min, peak %.0f deg",
                          primeMin, goodMin, peakAltDeg);
        else if (primeMin > 0)
            std::snprintf(b, sizeof(b), "PRIME %d min, peak %.0f deg", primeMin, peakAltDeg);
        else
            std::snprintf(b, sizeof(b), "GOOD %d min, peak %.0f deg", goodMin, peakAltDeg);
        std::string s = b;
        if (shortened) {
            std::snprintf(b, sizeof(b), " (shortened from %.0f min)", wantedMin);
            s += b;
        }
        return s;
    }
};

struct NightSkip {
    int         target = -1;
    std::string name;
    std::string why;
};

struct NightPlan {
    double startJd = 0.0, endJd = 0.0;
    std::vector<NightVisit> visits;     // in time order
    std::vector<NightSkip>  skipped;    // in the order given
    std::vector<NightBusy>  busy;       // with the margin added
    int TotalPrimeMin() const { int n = 0; for (const auto& v : visits) n += v.primeMin; return n; }
    int TotalImagingMin() const { int n = 0; for (const auto& v : visits) n += v.ImagingMin(); return n; }
};

using NightSampler = std::function<NightSample(int target, double jd)>;

namespace nightplan_detail {

struct Track {
    std::vector<char>   usable, prime;   // per minute
    std::vector<double> alt;             // per sample, minutes + 1
    int    usableCount = 0;
    int    altGoodCount = 0;             // at 45 or more, Sun aside
    double peak = -90.0;
};

struct Placement {
    bool ok = false;
    int  imageAt = 0, len = 0, prime = 0;
};

// The best place for one target, given the minutes already taken.
inline Placement Place(const Track& t, const std::vector<char>& taken,
                       int overhead, int vmax, int vmin) {
    const int m = (int)taken.size();
    // Contiguous free-and-usable minutes from each minute on, and free
    // minutes from each minute on.
    std::vector<int> run(m + 1, 0), freeRun(m + 1, 0);
    for (int k = m - 1; k >= 0; --k) {
        freeRun[k] = taken[k] ? 0 : freeRun[k + 1] + 1;
        run[k] = (!taken[k] && t.usable[k]) ? run[k + 1] + 1 : 0;
    }
    std::vector<int> primeSum(m + 1, 0);
    for (int k = 0; k < m; ++k) primeSum[k + 1] = primeSum[k] + (t.prime[k] ? 1 : 0);

    Placement best;
    for (int i = overhead; i < m; ++i) {
        if (run[i] == 0) continue;
        if (overhead > 0 && freeRun[i - overhead] < overhead) continue;
        const int len = (std::min)(run[i], vmax);
        if (len < vmin) continue;
        const int pr = primeSum[i + len] - primeSum[i];
        if (!best.ok || len > best.len || (len == best.len && pr > best.prime)) {
            best.ok = true;
            best.imageAt = i;
            best.len = len;
            best.prime = pr;
        }
    }
    return best;
}

struct Score {
    int placed = 0, prime = 0, imaging = 0;
    bool Beats(const Score& o) const {
        if (placed != o.placed) return placed > o.placed;
        if (prime != o.prime) return prime > o.prime;
        return imaging > o.imaging;
    }
};

} // namespace nightplan_detail

inline NightPlan BuildNightPlan(const std::vector<NightTarget>& targets,
                                const NightSampler& sample,
                                double startJd, double endJd,
                                const std::vector<NightBusy>& busy,
                                const NightPlanOptions& o) {
    using namespace nightplan_detail;
    NightPlan plan;
    plan.startJd = startJd;
    plan.endJd   = endJd;
    const double margin = o.passMarginSec / 86400.0;
    for (const auto& b : busy)
        plan.busy.push_back({b.startJd - margin, b.endJd + margin, b.label});

    const double step = (o.stepMin > 0.0 ? o.stepMin : 1.0) / 1440.0;
    const int m = endJd > startJd ? (int)std::floor((endJd - startJd) / step) : 0;
    const int overhead = (int)std::ceil(o.OverheadSec() / 60.0 / (step * 1440.0) - 1e-9);
    const int vmax = (std::max)(1, (int)std::lround(o.visitMin / (step * 1440.0)));
    const int vmin = (std::max)(1, (std::min)(vmax, (int)std::ceil(o.minVisitMin / (step * 1440.0) - 1e-9)));

    // Minutes no visit may touch.
    std::vector<char> blocked(m, 0);
    for (int k = 0; k < m; ++k) {
        const double a = startJd + k * step, b = a + step;
        for (const auto& s : plan.busy)
            if (a < s.endJd && b > s.startJd) { blocked[k] = 1; break; }
    }

    auto sunOk = [&](const NightSample& s) {
        if (s.sunAltDeg > o.sunMaxAltDeg) return false;
        if (s.sunAltDeg > 0.0 && s.sunSepDeg < o.sunAvoidDeg) return false;
        return true;
    };

    // Every target's minutes.
    std::vector<Track> tracks(targets.size());
    std::vector<int> candidates;
    for (int ti = 0; ti < (int)targets.size(); ++ti) {
        const NightTarget& tg = targets[ti];
        if (!tg.plannable) {
            plan.skipped.push_back({ti, tg.name, tg.notPlannable.empty()
                                     ? std::string("not a planet or a star") : tg.notPlannable});
            continue;
        }
        Track& t = tracks[ti];
        t.usable.assign(m, 0);
        t.prime.assign(m, 0);
        t.alt.resize(m + 1);
        std::vector<char> sunGood(m + 1, 1);
        for (int k = 0; k <= m; ++k) {
            const NightSample s = sample(ti, startJd + k * step);
            t.alt[k] = s.altDeg;
            sunGood[k] = sunOk(s) ? 1 : 0;
            if (s.altDeg > t.peak) t.peak = s.altDeg;
        }
        for (int k = 0; k < m; ++k) {
            const double lo = (std::min)(t.alt[k], t.alt[k + 1]);
            if (lo < NIGHT_GOOD_DEG) continue;
            ++t.altGoodCount;
            if (!sunGood[k] || !sunGood[k + 1]) continue;
            t.usable[k] = 1;
            t.prime[k]  = lo >= NIGHT_PRIME_DEG ? 1 : 0;
            ++t.usableCount;
        }
        if (t.altGoodCount == 0) {
            char b[160];
            std::snprintf(b, sizeof(b), "never reaches 45 deg before the plan ends (peak %.0f deg)", t.peak);
            plan.skipped.push_back({ti, tg.name, b});
            continue;
        }
        if (t.usableCount < vmin) {
            plan.skipped.push_back({ti, tg.name, t.usableCount == 0
                ? "the Sun rules leave it no time: the Sun is above the limit, or up and too close"
                : "the Sun rules leave it too little time"});
            continue;
        }
        candidates.push_back(ti);
    }

    // Place the candidates in one order; returns the score and the visits.
    auto placeAll = [&](const std::vector<int>& order, std::vector<NightVisit>* out) {
        Score sc;
        std::vector<char> taken = blocked;
        for (int ti : order) {
            const Track& t = tracks[ti];
            const Placement p = Place(t, taken, overhead, vmax, vmin);
            if (!p.ok) continue;
            for (int k = p.imageAt - overhead; k < p.imageAt + p.len; ++k) taken[k] = 1;
            ++sc.placed;
            sc.prime += p.prime;
            sc.imaging += p.len;
            if (out) {
                NightVisit v;
                v.target  = ti;
                v.name    = targets[ti].name;
                v.slewJd  = startJd + (p.imageAt - overhead) * step;
                v.imageJd = startJd + p.imageAt * step;
                v.endJd   = startJd + (p.imageAt + p.len) * step;
                const double perStep = step * 1440.0;
                v.primeMin = (int)std::lround(p.prime * perStep);
                v.goodMin  = (int)std::lround((p.len - p.prime) * perStep);
                v.peakAltDeg = -90.0;
                for (int k = p.imageAt; k <= p.imageAt + p.len; ++k)
                    v.peakAltDeg = (std::max)(v.peakAltDeg, t.alt[k]);
                v.wantedMin = o.visitMin;
                v.shortened = p.len < vmax;
                out->push_back(v);
            }
        }
        return sc;
    };

    std::vector<int> bestOrder = candidates;
    if ((int)candidates.size() <= o.exhaustiveMax) {
        std::vector<int> order = candidates;    // ascending: next_permutation starts here
        Score best;
        bool have = false;
        do {
            const Score s = placeAll(order, nullptr);
            if (!have || s.Beats(best)) { best = s; bestOrder = order; have = true; }
        } while (std::next_permutation(order.begin(), order.end()));
    } else {
        std::stable_sort(bestOrder.begin(), bestOrder.end(), [&](int a, int b) {
            return tracks[a].usableCount < tracks[b].usableCount;
        });
    }

    placeAll(bestOrder, &plan.visits);
    std::sort(plan.visits.begin(), plan.visits.end(),
              [](const NightVisit& a, const NightVisit& b) { return a.slewJd < b.slewJd; });

    for (int ti : candidates) {
        bool placed = false;
        for (const auto& v : plan.visits) if (v.target == ti) { placed = true; break; }
        if (placed) continue;
        char b[200];
        std::snprintf(b, sizeof(b), "no free stretch of %.0f min or more in its window: "
                      "other visits or passes take it", o.minVisitMin);
        plan.skipped.push_back({ti, targets[ti].name, b});
    }
    std::stable_sort(plan.skipped.begin(), plan.skipped.end(),
                     [](const NightSkip& a, const NightSkip& b) { return a.target < b.target; });
    return plan;
}

// ── Running it ───────────────────────────────────────────────
class NightPlanRunner {
public:
    enum class Phase { Idle, Waiting, Slewing, Settling, Locking, Running, Complete, Aborted, Failed };

    struct Config {
        double settleSec      = 5.0;
        double slewMinSec     = 1.5;    // a GoTo is not "done" before it has started
        double slewTimeoutSec = 180.0;
        double lockLimitSec   = 300.0;  // centring can take 5 min in the afternoon
        double minVisitSec    = 300.0;  // less than this left: skip, do not start
        // After the lock, before RUNS: the frame has just gone from the
        // whole sensor to the profile's ROI, and until frames flow at the
        // new size there is no frame rate, so no run length.
        double afterLockSec   = 3.0;
    };

    // Stop: the plan was stopped; stopRuns or dropLock says what to undo.
    enum class Action { None, Slew, StartLock, StartRuns, Visited, Skipped, Stop };
    struct Step {
        Action kind  = Action::None;
        int    index = -1;          // into the visits
        bool   stopRuns = false;    // with Visited or Stop: a sequence is going
        bool   dropLock = false;    // with Skipped or Stop: an acquisition is running
    };

    struct Inputs {
        double nowJd       = 0.0;
        bool   inWindow    = false;  // the next visit's target is as high now as its plan puts it
        bool   mountBusy   = false;  // a pass, a mosaic, the Moon map, a recording
        bool   slewing     = false;
        bool   locked      = false;
        bool   runsRunning = false;
        bool   runsByPerson = false; // the sequence was stopped by someone
        bool   framesFlowing = true; // the camera has a frame rate at this size
    };

    enum class Outcome { Pending, Active, Done, Skipped };

    bool Start(const NightPlan& plan, const Config& c, double nowJd) {
        m_visits = plan.visits;
        m_busy   = plan.busy;
        m_cfg    = c;
        m_at     = 0;
        m_done = m_skipped = 0;
        m_reason.clear();
        m_note.clear();
        m_out.assign(m_visits.size(), Outcome::Pending);
        m_notes.assign(m_visits.size(), std::string());
        m_stopRunsPending = m_dropLockPending = false;
        if (m_visits.empty()) { m_phase = Phase::Failed; m_reason = "the plan has no visits"; return false; }
        Enter(Phase::Waiting, nowJd);
        return true;
    }

    void Abort(const char* why) {
        if (!Running()) return;
        if (m_phase == Phase::Running) m_stopRunsPending = true;
        if (m_phase == Phase::Locking || m_phase == Phase::Settling) m_dropLockPending = true;
        if (m_at < (int)m_out.size() && m_out[m_at] == Outcome::Active) {
            m_out[m_at] = Outcome::Skipped;
            m_notes[m_at] = why ? why : "stopped";
        }
        m_phase  = Phase::Aborted;
        m_reason = why ? why : "stopped";
    }

    // The caller could not do what it was told. Each costs this target.
    void SlewRefused(const char* why, double nowJd) { SkipNow(why ? why : "the slew was refused", nowJd); }
    void LockRefused(const char* why, double nowJd) {
        SkipNow(std::string("no lock: ") + (why ? why : "acquisition refused"), nowJd);
    }
    void RunsRefused(const char* why, double nowJd) {
        SkipNow(std::string("the runs would not start: ") + (why ? why : "refused"), nowJd);
    }

    Step Tick(const Inputs& in) {
        Step s;
        if (m_stopRunsPending) { m_stopRunsPending = false; s.kind = Action::Stop; s.index = m_at; s.stopRuns = true; return s; }
        if (m_dropLockPending) { m_dropLockPending = false; s.kind = Action::Stop; s.index = m_at; s.dropLock = true; return s; }
        if (!Running()) return s;

        const double t = in.nowJd;
        const double secIn = (t - m_enteredJd) * 86400.0;
        const NightVisit& v = m_visits[m_at];
        s.index = m_at;

        switch (m_phase) {
        case Phase::Waiting: {
            const double left = (v.endJd - t) * 86400.0;
            if (left < m_cfg.minVisitSec) {
                const std::string why = in.mountBusy
                    ? "its slot passed while the mount was busy"
                    : "its slot passed before it could start";
                return Skip(why, t, false);
            }
            if (in.mountBusy) return s;
            const bool due = t >= v.slewJd;
            const bool early = !due && in.inWindow && !BusyBetween(t, v.endJd);
            if (!due && !early) return s;
            m_out[m_at] = Outcome::Active;
            m_note = early ? "early: it is already as high as its plan puts it"
                           : "on time";
            Enter(Phase::Slewing, t);
            s.kind = Action::Slew;
            return s;
        }
        case Phase::Slewing:
            if (secIn < m_cfg.slewMinSec) return s;
            if (secIn > m_cfg.slewTimeoutSec) {
                char b[160];
                std::snprintf(b, sizeof(b), "the slew to %s did not finish in %.0f s",
                              v.name.c_str(), m_cfg.slewTimeoutSec);
                m_out[m_at] = Outcome::Skipped;
                m_notes[m_at] = b;
                m_phase = Phase::Failed;
                m_reason = b;
                return s;
            }
            if (!in.slewing) Enter(Phase::Settling, t);
            return s;
        case Phase::Settling:
            if (secIn < m_cfg.settleSec) return s;
            Enter(Phase::Locking, t);
            s.kind = Action::StartLock;
            return s;
        case Phase::Locking:
            if (in.locked) {
                if (m_lockedJd <= 0.0) m_lockedJd = t;
                if ((t - m_lockedJd) * 86400.0 >= m_cfg.afterLockSec && in.framesFlowing) {
                    m_lockedJd = 0.0;
                    Enter(Phase::Running, t);
                    s.kind = Action::StartRuns;
                    return s;
                }
            } else {
                m_lockedJd = 0.0;
            }
            if (t >= v.endJd) return Skip("its slot ended before it locked", t, true);
            if (secIn > m_cfg.lockLimitSec) {
                char b[96];
                std::snprintf(b, sizeof(b), "no lock in %.0f s", m_cfg.lockLimitSec);
                return Skip(b, t, true);
            }
            return s;
        case Phase::Running:
            if (secIn >= m_cfg.slewMinSec && !in.runsRunning) {
                if (in.runsByPerson) {
                    m_out[m_at] = Outcome::Skipped;
                    m_notes[m_at] = "the runs were stopped by hand";
                    m_phase = Phase::Aborted;
                    m_reason = "the runs were stopped by hand";
                    return s;
                }
                return Visit("its runs are done", t, false);
            }
            if (t >= v.endJd) return Visit("its slot ended", t, true);
            return s;
        default:
            return s;
        }
    }

    bool Running() const {
        return m_phase == Phase::Waiting || m_phase == Phase::Slewing ||
               m_phase == Phase::Settling || m_phase == Phase::Locking ||
               m_phase == Phase::Running;
    }
    Phase State()   const { return m_phase; }
    int   Index()   const { return m_at; }
    int   Total()   const { return (int)m_visits.size(); }
    int   Visited() const { return m_done; }
    int   Skipped() const { return m_skipped; }
    const std::string& Reason() const { return m_reason; }
    // Why the last visit started, ended or was skipped.
    const std::string& Note() const { return m_note; }
    Outcome OutcomeOf(int i) const { return (i >= 0 && i < (int)m_out.size()) ? m_out[i] : Outcome::Pending; }
    const std::string& NoteOf(int i) const { static const std::string e; return (i >= 0 && i < (int)m_notes.size()) ? m_notes[i] : e; }
    const std::vector<NightVisit>& Visits() const { return m_visits; }

    static const char* PhaseName(Phase p) {
        switch (p) {
        case Phase::Idle:     return "idle";
        case Phase::Waiting:  return "waiting";
        case Phase::Slewing:  return "slewing";
        case Phase::Settling: return "settling";
        case Phase::Locking:  return "locking";
        case Phase::Running:  return "runs";
        case Phase::Complete: return "complete";
        case Phase::Aborted:  return "stopped";
        default:              return "failed";
        }
    }

private:
    void Enter(Phase p, double jd) { m_phase = p; m_enteredJd = jd; if (p != Phase::Locking) m_lockedJd = 0.0; }

    bool BusyBetween(double a, double b) const {
        for (const auto& s : m_busy) if (a < s.endJd && b > s.startJd) return true;
        return false;
    }

    void Next(double jd) {
        ++m_at;
        if (m_at >= (int)m_visits.size()) {
            m_phase = Phase::Complete;
            char b[96];
            std::snprintf(b, sizeof(b), "%d visited, %d skipped", m_done, m_skipped);
            m_reason = b;
        } else {
            Enter(Phase::Waiting, jd);
        }
    }

    Step Skip(const std::string& why, double jd, bool dropLock) {
        Step s;
        s.kind = Action::Skipped;
        s.index = m_at;
        s.dropLock = dropLock;
        m_out[m_at] = Outcome::Skipped;
        m_notes[m_at] = why;
        m_note = why;
        ++m_skipped;
        Next(jd);
        return s;
    }
    void SkipNow(const std::string& why, double jd) {
        if (!Running()) return;
        m_out[m_at] = Outcome::Skipped;
        m_notes[m_at] = why;
        m_note = why;
        ++m_skipped;
        Next(jd);
    }
    Step Visit(const std::string& why, double jd, bool stopRuns) {
        Step s;
        s.kind = Action::Visited;
        s.index = m_at;
        s.stopRuns = stopRuns;
        m_out[m_at] = Outcome::Done;
        m_notes[m_at] = why;
        m_note = why;
        ++m_done;
        Next(jd);
        return s;
    }

    std::vector<NightVisit> m_visits;
    std::vector<NightBusy>  m_busy;
    std::vector<Outcome>    m_out;
    std::vector<std::string> m_notes;
    Config      m_cfg;
    Phase       m_phase = Phase::Idle;
    double      m_enteredJd = 0.0;
    double      m_lockedJd  = 0.0;   // when the lock was first seen
    int         m_at = 0, m_done = 0, m_skipped = 0;
    bool        m_stopRunsPending = false, m_dropLockPending = false;
    std::string m_reason, m_note;
};

inline constexpr bool NIGHT_PLAN_PROVEN_ON_SKY = false;

} // namespace raduk::core
