#pragma once
// ============================================================
// FocusReplay.h -- recorded sweeps through today's rules (Phase 174.2)
// ============================================================
//
// The owner, 2026-11-22: "a history of focusing so that my autofocuser
// starts to learn." Learning safely means a rule change is judged against
// every real sweep before it reaches the sky. This reads focus_history.jsonl
// (core/FocusHistory.h), and for each sweep re-runs the decisions that can
// be made again from what was recorded:
//   - the coarse fit (FitDirections over the recorded out and back passes);
//   - the same-sky A/B's verdict (SweepSpoiledByFade, KeepAfterAB), when it
//     ran: keep the fit, or go back to the start.
// The fine pass cannot be re-run without its frames, so a replayed result
// is the recorded result or the start, as the A/B now decides.
//
// Where the owner's followup says where focus settled, each sweep is
// scored: how far the recorded result was, and how far the replayed one is.
// A rule change that leaves any sweep further from where the owner settled
// is a regression (Regressions()).
//
// Pure, no dependencies: a small JSON reader for the history's own format.
// ============================================================

#include <cmath>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>

#include "FocusDirection.h"

namespace raduk::core::replay {

// ── A small JSON reader ────────────────────────────────────────────────

struct J {
    enum T { Null, Bool, Num, Str, Arr, Obj } t = Null;
    bool b = false; double n = NAN; std::string s;
    std::vector<J> a; std::vector<std::pair<std::string, J>> o;
    const J& operator[](const char* k) const {
        static const J none;
        for (const auto& kv : o) if (kv.first == k) return kv.second;
        return none;
    }
    double num(double dflt = NAN) const { return t == Num ? n : dflt; }
    int    integer(int dflt = 0) const { return t == Num ? (int)std::lround(n) : dflt; }
    bool   truth() const { return t == Bool && b; }
};

class Reader {
public:
    explicit Reader(const std::string& s) : m_s(s) {}
    bool Parse(J& out) { m_i = 0; bool ok = Value(out); Ws(); return ok && m_i == m_s.size(); }
private:
    void Ws() { while (m_i < m_s.size() && (m_s[m_i] == ' ' || m_s[m_i] == '\t' || m_s[m_i] == '\r' || m_s[m_i] == '\n')) ++m_i; }
    bool Lit(const char* w) { size_t n = strlen(w); if (m_s.compare(m_i, n, w) != 0) return false; m_i += n; return true; }
    bool String(std::string& out) {
        if (m_s[m_i] != '"') return false;
        ++m_i;
        while (m_i < m_s.size() && m_s[m_i] != '"') {
            char c = m_s[m_i++];
            if (c == '\\' && m_i < m_s.size()) {
                char e = m_s[m_i++];
                switch (e) {
                    case 'n': out += '\n'; break; case 't': out += '\t'; break; case 'r': out += '\r'; break;
                    case 'u': if (m_i + 4 <= m_s.size()) { out += (char)strtol(m_s.substr(m_i, 4).c_str(), nullptr, 16); m_i += 4; } break;
                    default: out += e;
                }
            } else out += c;
        }
        if (m_i >= m_s.size()) return false;
        ++m_i;
        return true;
    }
    bool Value(J& v) {
        Ws();
        if (m_i >= m_s.size()) return false;
        const char c = m_s[m_i];
        if (c == '{') {
            v.t = J::Obj; ++m_i; Ws();
            if (m_s[m_i] == '}') { ++m_i; return true; }
            for (;;) {
                Ws(); std::string k; if (!String(k)) return false;
                Ws(); if (m_s[m_i] != ':') return false; ++m_i;
                J x; if (!Value(x)) return false;
                v.o.emplace_back(std::move(k), std::move(x));
                Ws(); if (m_s[m_i] == ',') { ++m_i; continue; }
                if (m_s[m_i] == '}') { ++m_i; return true; }
                return false;
            }
        }
        if (c == '[') {
            v.t = J::Arr; ++m_i; Ws();
            if (m_s[m_i] == ']') { ++m_i; return true; }
            for (;;) {
                J x; if (!Value(x)) return false;
                v.a.push_back(std::move(x));
                Ws(); if (m_s[m_i] == ',') { ++m_i; continue; }
                if (m_s[m_i] == ']') { ++m_i; return true; }
                return false;
            }
        }
        if (c == '"') { v.t = J::Str; return String(v.s); }
        if (Lit("true"))  { v.t = J::Bool; v.b = true; return true; }
        if (Lit("false")) { v.t = J::Bool; v.b = false; return true; }
        if (Lit("null"))  { v.t = J::Null; return true; }
        char* end = nullptr;
        const double d = strtod(m_s.c_str() + m_i, &end);
        if (end == m_s.c_str() + m_i) return false;
        v.t = J::Num; v.n = d; m_i = (size_t)(end - m_s.c_str());
        return true;
    }
    const std::string& m_s;
    size_t m_i = 0;
};

inline std::vector<float> Floats(const J& a) {
    std::vector<float> v;
    for (const auto& x : a.a) v.push_back(x.t == J::Num ? (float)x.n : 0.0f);
    return v;
}
inline std::vector<int> Ints(const J& a) {
    std::vector<int> v;
    for (const auto& x : a.a) v.push_back(x.integer());
    return v;
}

// ── Replay ─────────────────────────────────────────────────────────────

struct Replayed {
    std::string id, target;
    int start = 0, recorded = 0, replayed = 0;
    int coarseFit = -1;              // today's FitDirections on the recorded passes
    std::string recordedVerdict, replayedVerdict;
    bool settledKnown = false;
    int settled = 0;
    int errRecorded = 0, errReplayed = 0;   // |result - settled|, when settled is known
};

// One sweep line (kind "sweep").
inline Replayed ReplaySweep(const J& s) {
    Replayed r;
    r.id = s["id"].s;
    r.target = s["at"]["target"].s;
    r.start = s["start"].integer();
    r.recorded = s["result"].integer();
    r.replayed = r.recorded;
    const J& c = s["coarse"];
    const std::vector<int> pos = Ints(c["pos"]);
    const std::vector<float> out = Floats(c["out"]), back = Floats(c["back"]);
    if (!pos.empty() && pos.size() == out.size() && pos.size() == back.size()) {
        const DirectionFit f = FitDirections(pos, out, back);
        if (f.mean.ok) r.coarseFit = (int)std::lround(f.mean.pos);
    }
    const J& ab = s["ab"];
    r.recordedVerdict = ab["verdict"].s;
    if (ab["ran"].truth()) {
        const float before = (float)ab["start_before"].num(0), now = (float)ab["start_now"].num(0),
                    fit = (float)ab["fit"].num(0), es = (float)ab["edge_start"].num(0),
                    ef = (float)ab["edge_fit"].num(0);
        const bool spoiled = SweepSpoiledByFade(now, before, out, back);
        const bool keep = !spoiled && KeepAfterAB(now, fit, es > 0.0f && ef > 0.0f, es, ef);
        r.replayedVerdict = keep ? "kept fit" : (spoiled ? "start stands (fade)" : "start stands");
        if (!keep) r.replayed = r.start;
        else if (r.recordedVerdict.rfind("start stands", 0) == 0) {
            // The routine went back to the start, so the fit it would have kept
            // is the coarse one (the fine result was never driven to): best known.
            if (r.coarseFit > 0) r.replayed = r.coarseFit;
        }
    } else {
        r.replayedVerdict = "no A/B";
    }
    return r;
}

struct ReplayReport {
    std::vector<Replayed> sweeps;
    int linesRead = 0, linesBad = 0;
};

// The whole file's text: sweeps, each joined to its followup's settled position.
inline ReplayReport ReplayHistory(const std::string& text) {
    ReplayReport rep;
    std::map<std::string, int> settled;
    std::vector<J> sweeps;
    size_t p = 0;
    while (p < text.size()) {
        size_t nl = text.find('\n', p);
        if (nl == std::string::npos) nl = text.size();
        const std::string line = text.substr(p, nl - p);
        p = nl + 1;
        if (line.find_first_not_of(" \t\r") == std::string::npos) continue;
        ++rep.linesRead;
        J j;
        if (!Reader(line).Parse(j) || j.t != J::Obj) { ++rep.linesBad; continue; }
        if (j["kind"].s == "sweep") sweeps.push_back(std::move(j));
        else if (j["kind"].s == "followup") settled[j["sweep"].s] = j["settled"].integer();
    }
    for (const auto& s : sweeps) {
        Replayed r = ReplaySweep(s);
        auto it = settled.find(r.id);
        if (it != settled.end()) {
            r.settledKnown = true; r.settled = it->second;
            r.errRecorded = std::abs(r.recorded - r.settled);
            r.errReplayed = std::abs(r.replayed - r.settled);
        }
        rep.sweeps.push_back(r);
    }
    return rep;
}

// Sweeps today's rules leave further from where the owner settled than the
// recorded result was, by more than `slack` steps.
inline std::vector<Replayed> Regressions(const ReplayReport& rep, int slack = 50) {
    std::vector<Replayed> out;
    for (const auto& r : rep.sweeps)
        if (r.settledKnown && r.errReplayed > r.errRecorded + slack) out.push_back(r);
    return out;
}

} // namespace raduk::core::replay
