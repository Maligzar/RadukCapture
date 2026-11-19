#pragma once
// ============================================================
//  FocuserTrust.h -- phase 157.0e: a focuser that comes back with a
//  different number
//
//  Owners report the ZWO EAF losing its position over a power loss: it
//  reads 1000 against a last known 13785, or 0, 2 or about 4761. Before
//  this, the app saved the position nowhere, so it could not even notice;
//  every absolute number after that (a profile's focus position, the
//  autofocus sweep, a GO TO) would have meant a different place.
//
//  Here, pure (no SDK, no Windows), shared by the EAF wrapper and the
//  emulator:
//    FocuserMemory    the last position and the focuser's range, with the
//                     serial it belongs to, saved after every completed move
//    JudgeOpen        on every open (startup or a reconnect): does the
//                     focuser's number agree with the saved one?
//    PositionTrust    the verdict, held: while untrusted no absolute move
//                     is made, until the owner either accepts the
//                     focuser's number or puts the saved one back. Never
//                     synced silently. Relative steps still mean what they
//                     say, so jogging by hand still works.
//    FocuserRecovery  when to try re-opening a focuser whose reads fail
//
//  Gated by tests/focuser_trust.cpp and fault_injection's eaf-drop group.
// ============================================================

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <sstream>
#include <string>
#include <system_error>

namespace raduk::core {

// The focuser's number agrees with the saved one within this. A completed
// move ends within kArrivalToleranceSteps (2) of where it was sent; this
// leaves room for that and a step or two of reading.
inline constexpr int kTrustToleranceSteps = 5;

struct FocuserMemory {
    bool        valid = false;
    std::string serial;      // empty: the device does not report one (the emulator)
    int         position = 0;
    int         maxStep = 0;
};

inline std::string FormatFocuserMemory(const FocuserMemory& m) {
    std::ostringstream o;
    o << "# RadukCapture: the focuser's last known position (phase 157.0e)\n"
      << "Serial=" << m.serial << "\n"
      << "Position=" << m.position << "\n"
      << "MaxStep=" << m.maxStep << "\n";
    return o.str();
}

// Needs Position; Serial and MaxStep may be missing. Anything malformed: false.
inline bool ParseFocuserMemory(const std::string& text, FocuserMemory& out) {
    FocuserMemory m;
    bool havePos = false;
    std::istringstream in(text);
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty() || line[0] == '#') continue;
        const size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        const std::string k = line.substr(0, eq), v = line.substr(eq + 1);
        auto num = [&](int& dst) {
            char* end = nullptr;
            const long x = std::strtol(v.c_str(), &end, 10);
            if (v.empty() || !end || *end != '\0' || x < 0 || x > 100000000L) return false;
            dst = (int)x;
            return true;
        };
        if (k == "Serial") m.serial = v;
        else if (k == "Position") { if (!num(m.position)) return false; havePos = true; }
        else if (k == "MaxStep")  { if (!num(m.maxStep)) return false; }
    }
    if (!havePos) return false;
    m.valid = true;
    out = m;
    return true;
}

inline bool LoadFocuserMemory(const std::string& path, FocuserMemory& out) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    std::ostringstream ss;
    ss << f.rdbuf();
    return ParseFocuserMemory(ss.str(), out);
}

// Written beside and renamed over, so a crash mid-write leaves the old
// record, never half a file.
inline bool SaveFocuserMemory(const std::string& path, const FocuserMemory& m) {
    const std::string tmp = path + ".tmp";
    {
        std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
        if (!f) return false;
        f << FormatFocuserMemory(m);
        if (!f) return false;
    }
    std::error_code ec;
    std::filesystem::rename(tmp, path, ec);
    if (ec) { std::filesystem::remove(tmp, ec); return false; }
    return true;
}

enum class TrustVerdict {
    Trusted,        // the number agrees with the saved one
    NoRecord,       // nothing saved yet: trusted, and recorded from now
    OtherFocuser,   // a different serial: its own record starts now
    Moved,          // the number disagrees: untrusted
    RangeChanged,   // the focuser's maximum changed: untrusted
};

inline bool Trusting(TrustVerdict v) {
    return v == TrustVerdict::Trusted || v == TrustVerdict::NoRecord || v == TrustVerdict::OtherFocuser;
}

inline TrustVerdict JudgeOpen(const FocuserMemory& saved, const std::string& serial,
                              int reported, int maxStep, int tolerance = kTrustToleranceSteps) {
    if (!saved.valid) return TrustVerdict::NoRecord;
    if (!saved.serial.empty() && !serial.empty() && saved.serial != serial) return TrustVerdict::OtherFocuser;
    if (saved.maxStep > 0 && maxStep > 0 && saved.maxStep != maxStep) return TrustVerdict::RangeChanged;
    if (std::abs(reported - saved.position) > tolerance) return TrustVerdict::Moved;
    return TrustVerdict::Trusted;
}

// Thread-safe: the focuser's command lock is not always held round it.
class PositionTrust {
public:
    // Where the record lives. Empty (the default, and every test that does
    // not ask): no record, and every open is trusted.
    void SetPath(const std::string& p) { std::lock_guard<std::mutex> lk(m_mx); m_path = p; }
    std::string Path() const { std::lock_guard<std::mutex> lk(m_mx); return m_path; }

    // The record as it stands, for an emulator that "remembers" its number.
    bool Saved(FocuserMemory& out) const {
        std::lock_guard<std::mutex> lk(m_mx);
        return !m_path.empty() && LoadFocuserMemory(m_path, out);
    }

    // Every open, before any move. Returns the verdict and holds it.
    TrustVerdict OnOpen(const std::string& serial, int reported, int maxStep) {
        std::lock_guard<std::mutex> lk(m_mx);
        m_serial = serial; m_maxStep = maxStep; m_reported = reported;
        if (m_path.empty()) { m_verdict = TrustVerdict::Trusted; m_trusted = true; return m_verdict; }
        FocuserMemory saved;
        const bool have = LoadFocuserMemory(m_path, saved);
        m_saved = have ? saved.position : -1;
        m_verdict = JudgeOpen(have ? saved : FocuserMemory{}, serial, reported, maxStep);
        m_trusted = Trusting(m_verdict);
        if (m_trusted) SaveLocked(reported);   // the record follows this focuser from now
        return m_verdict;
    }

    bool Trusted() const { std::lock_guard<std::mutex> lk(m_mx); return m_trusted; }
    TrustVerdict Verdict() const { std::lock_guard<std::mutex> lk(m_mx); return m_verdict; }
    int SavedPosition() const    { std::lock_guard<std::mutex> lk(m_mx); return m_saved; }
    int ReportedPosition() const { std::lock_guard<std::mutex> lk(m_mx); return m_reported; }

    // Empty while trusted; else both numbers, and what to do.
    std::string Problem() const {
        std::lock_guard<std::mutex> lk(m_mx);
        if (m_trusted) return {};
        char b[320];
        if (m_verdict == TrustVerdict::RangeChanged)
            std::snprintf(b, sizeof(b), "the focuser's range changed since it was last used (it reports %d, "
                          "it was last at %d): accept its number or put %d back before any automatic move",
                          m_reported, m_saved, m_saved);
        else
            std::snprintf(b, sizeof(b), "the focuser reports %d but was last at %d: accept its number "
                          "or put %d back before any automatic move", m_reported, m_saved, m_saved);
        return b;
    }

    // A move finished here: the record follows it. Not while untrusted,
    // so the number the owner may want back is never overwritten.
    void OnArrived(int position) {
        std::lock_guard<std::mutex> lk(m_mx);
        if (m_trusted) SaveLocked(position);
    }

    // The owner's two answers. Neither moves the motor.
    void Accept(int reported) {
        std::lock_guard<std::mutex> lk(m_mx);
        m_trusted = true; m_verdict = TrustVerdict::Trusted; m_reported = reported;
        SaveLocked(reported);
    }
    void Restored(int position) {   // the focuser was set to the saved number
        std::lock_guard<std::mutex> lk(m_mx);
        m_trusted = true; m_verdict = TrustVerdict::Trusted; m_reported = position;
        SaveLocked(position);
    }

private:
    void SaveLocked(int position) {
        if (m_path.empty()) return;
        FocuserMemory m;
        m.valid = true; m.serial = m_serial; m.position = position; m.maxStep = m_maxStep;
        SaveFocuserMemory(m_path, m);
    }
    mutable std::mutex m_mx;
    std::string  m_path, m_serial;
    int          m_maxStep = 0, m_saved = -1, m_reported = 0;
    bool         m_trusted = true;
    TrustVerdict m_verdict = TrustVerdict::Trusted;
};

// When to try re-opening a focuser whose position reads fail: once they
// have failed for kFailingSec, then every kRetrySec. Never moves anything.
// Single-threaded (the app's 500 ms tick).
class FocuserRecovery {
public:
    static constexpr double kFailingSec = 2.0;
    static constexpr double kRetrySec = 3.0;
    // A read, and its outcome. Returns whether to try a reconnect now.
    bool OnRead(bool ok, double now) {
        if (ok) { m_failing = false; return false; }
        if (!m_failing) { m_failing = true; m_since = now; m_lastTry = -1e9; return false; }
        if (now - m_since < kFailingSec || now - m_lastTry < kRetrySec) return false;
        m_lastTry = now;
        return true;
    }
    bool Failing() const { return m_failing; }
    void Reset() { m_failing = false; }
private:
    bool   m_failing = false;
    double m_since = 0.0, m_lastTry = -1e9;
};

}  // namespace raduk::core
