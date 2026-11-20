#pragma once
// ============================================================
//  SkyWeatherClient -- phase 165.1: fetching the sky's weather
//
//  One background thread asks three services for the site's weather and
//  keeps the latest good reply of each:
//
//    Open-Meteo, current conditions   every 10 min, 2 after a failure
//    7Timer!, ASTRO forecast          every 60 min, 10 after a failure
//    NWS, active alerts (US only)     every 10 min, 30 after a failure
//
//  This file is pure C++: no windows.h and no socket. The request itself
//  is a function passed in, which in the app (165.2) is SatDesk::HttpGet,
//  the one place the offline rule is kept (phase 151): with rig.ini
//  Offline=1 nothing is asked of the internet. A test passes its own
//  function, so the whole client runs headless, under ASan and TSan.
//
//  Rules:
//    - No site in rig.ini, no request. Nothing asks for latitude 0,
//      longitude 0 (phase 151.4's mistake in Dash).
//    - The site is sent rounded to 0.01 degree (about 1 km), plenty for
//      a forecast and no more than one needs.
//    - Every good reply is written to a cache file, raw, with the time it
//      came. At start the cache is read back through the same parsers,
//      so after a restart offline there is something to show, and its
//      age says how old it is.
//    - A reply that does not parse leaves the last good one in place; it
//      is never half-applied.
//
//  tests/sky_weather_client.cpp gates it.
// ============================================================

#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <functional>
#include <limits>
#include <algorithm>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "SkyWeather.h"

namespace raduk::core::sky {

// ── Requests ─────────────────────────────────────────────────

enum class Source { OpenMeteo = 0, SevenTimer = 1, Nws = 2 };
inline constexpr int kSources = 3;
inline const char* SourceName(Source s) {
    switch (s) {
        case Source::OpenMeteo:  return "open-meteo";
        case Source::SevenTimer: return "7timer";
        case Source::Nws:        return "nws";
    }
    return "?";
}

struct Request { std::string host, path; };

// "%.2f", without a locale's decimal comma.
inline std::string Deg2(double v) {
    const double r = std::round(v * 100.0) / 100.0;
    char b[32];
    std::snprintf(b, sizeof(b), "%.2f", r == 0.0 ? 0.0 : r);   // no "-0.00"
    for (char* p = b; *p; ++p) if (*p == ',') *p = '.';
    return b;
}

inline Request RequestFor(Source s, double latDeg, double lonDeg) {
    const std::string la = Deg2(latDeg), lo = Deg2(lonDeg);
    switch (s) {
        case Source::OpenMeteo:
            return {"api.open-meteo.com",
                    "/v1/forecast?latitude=" + la + "&longitude=" + lo +
                    "&current=temperature_2m,relative_humidity_2m,wind_speed_10m,"
                    "wind_direction_10m,wind_gusts_10m,cloud_cover,pressure_msl,"
                    "visibility,dew_point_2m&timezone=UTC"};
        case Source::SevenTimer:
            return {"www.7timer.info",
                    "/bin/astro.php?lon=" + lo + "&lat=" + la +
                    "&ac=0&unit=metric&output=json&tzshift=0"};
        case Source::Nws:
            return {"api.weather.gov", "/alerts/active?point=" + la + "," + lo};
    }
    return {};
}

// How the request went, from the caller's fetch function.
enum class FetchResult { Ok, Failed, Offline };
using FetchFn = std::function<FetchResult(const std::string& host, const std::string& path,
                                          std::string& body)>;

// Seconds until the next try, after a success and after a failure.
struct Cadence { int okSec, failSec; };
inline Cadence CadenceOf(Source s) {
    switch (s) {
        case Source::OpenMeteo:  return {10 * 60, 2 * 60};
        case Source::SevenTimer: return {60 * 60, 10 * 60};
        case Source::Nws:        return {10 * 60, 30 * 60};
    }
    return {600, 120};
}

// A current reading older than this is not used for decisions (the
// same 30 minutes Leoben's readings had).
inline constexpr int64_t kCurrentFreshSec = 30 * 60;
inline constexpr int64_t kAlertsFreshSec  = 30 * 60;

// ── What the app reads ───────────────────────────────────────

enum class SourceState { Off, NoSite, Waiting, Ok, Failed, Offline, Unreadable };
inline const char* SourceStateText(SourceState s) {
    switch (s) {
        case SourceState::Off:        return "off";
        case SourceState::NoSite:     return "no site set in rig.ini";
        case SourceState::Waiting:    return "waiting";
        case SourceState::Ok:         return "ok";
        case SourceState::Failed:     return "not reachable";
        case SourceState::Offline:    return "offline";
        case SourceState::Unreadable: return "reply not readable";
    }
    return "?";
}

struct SourceStatus {
    SourceState state = SourceState::Off;
    int64_t     lastAttemptUnix = 0;
    int64_t     lastGoodUnix = 0;   // when the reply in use arrived (from the cache too)
    bool        fromCache = false;  // the reply in use was read back at start
    std::string detail;             // a parser's or service's reason, when there is one
};

struct SkySnapshot {
    SourceStatus status[kSources];
    OmCurrent    current;           // valid only if a reply has been read
    T7Forecast   sevenTimer;
    std::vector<NwsAlert> alerts;
    bool         alertsKnown = false;

    const SourceStatus& Of(Source s) const { return status[(int)s]; }
    bool CurrentFresh(int64_t now) const {
        const auto& st = Of(Source::OpenMeteo);
        return current.valid && st.lastGoodUnix > 0 && now >= st.lastGoodUnix &&
               now - st.lastGoodUnix <= kCurrentFreshSec;
    }
    bool AlertsFresh(int64_t now) const {
        const auto& st = Of(Source::Nws);
        return alertsKnown && st.lastGoodUnix > 0 && now >= st.lastGoodUnix &&
               now - st.lastGoodUnix <= kAlertsFreshSec;
    }
    // 7Timer's timepoint for now, or none.
    const T7Point* SeeingNow(int64_t now) const {
        return sevenTimer.valid ? SevenTimerAt(sevenTimer, now) : nullptr;
    }
    // The derived readings, from fresh data only; unknown otherwise.
    DewRisk Dew(int64_t now) const {
        if (!CurrentFresh(now)) return DewRisk::Unknown;
        return DewRiskFromSpread(current.tempC, current.dewPointC);
    }
    WindTier Wind(int64_t now) const {
        if (!CurrentFresh(now)) return WindTier{};
        return WindTierFrom(current.windMs, current.gustMs);
    }
    DustReading Dust(int64_t now) const {
        static const std::vector<NwsAlert> none;
        const bool cur = CurrentFresh(now);
        return DustFrom(cur ? current.visibilityM : Opt{}, cur ? current.gustMs : Opt{},
                        AlertsFresh(now) ? alerts : none);
    }
};

// ── The cache file ───────────────────────────────────────────
//
// Text: for each source with a good reply, one header line
//   "raduk-sky-cache 1 <source> <fetchedUnix> <bytes>"
// then exactly <bytes> of the raw reply and a newline. The replies are
// read back through the same parsers, so the cache needs no format of
// its own for the readings.

struct CacheEntry { Source source; int64_t fetchedUnix; std::string body; };

inline std::string SerializeCache(const std::vector<CacheEntry>& es) {
    std::string out;
    for (const auto& e : es) {
        out += "raduk-sky-cache 1 " + std::string(SourceName(e.source)) + " " +
               std::to_string(e.fetchedUnix) + " " + std::to_string(e.body.size()) + "\n";
        out += e.body;
        out += "\n";
    }
    return out;
}

// What reads; a damaged tail is dropped, not guessed at.
inline std::vector<CacheEntry> ParseCache(const std::string& text) {
    std::vector<CacheEntry> out;
    size_t p = 0;
    while (p < text.size()) {
        const size_t nl = text.find('\n', p);
        if (nl == std::string::npos) break;
        const std::string head = text.substr(p, nl - p);
        char name[32] = {};
        long long t = 0, n = 0;
        int ver = 0;
        if (std::sscanf(head.c_str(), "raduk-sky-cache %d %31s %lld %lld", &ver, name, &t, &n) != 4
            || ver != 1 || t <= 0 || n < 0 || n > 4 * 1024 * 1024)
            break;
        const size_t start = nl + 1;
        if (start + (size_t)n + 1 > text.size() || text[start + (size_t)n] != '\n') break;
        Source s;
        const std::string nm = name;
        if (nm == "open-meteo") s = Source::OpenMeteo;
        else if (nm == "7timer") s = Source::SevenTimer;
        else if (nm == "nws") s = Source::Nws;
        else { p = start + (size_t)n + 1; continue; }   // a source this build does not know
        out.push_back({s, (int64_t)t, text.substr(start, (size_t)n)});
        p = start + (size_t)n + 1;
    }
    return out;
}

// ── The client ───────────────────────────────────────────────

class SkyWeatherClient {
public:
    struct Options {
        bool        siteSet = false;
        double      latDeg = 0, lonDeg = 0;
        bool        askNws = true;          // US sites; elsewhere NWS has nothing
        std::string cachePath;              // empty: no cache
    };
    using LogFn   = std::function<void(const std::string&)>;
    using ClockFn = std::function<int64_t()>;

    SkyWeatherClient() = default;
    SkyWeatherClient(const SkyWeatherClient&) = delete;
    SkyWeatherClient& operator=(const SkyWeatherClient&) = delete;
    ~SkyWeatherClient() { Stop(); }

    // Sets up and reads the cache, without starting the thread. A test
    // drives it with Step(); the app calls Start().
    void Configure(const Options& o, FetchFn fetch, LogFn log = {}, ClockFn clock = {}) {
        Stop();
        std::lock_guard<std::mutex> lk(m_mu);
        m_opt = o;
        m_fetch = std::move(fetch);
        m_log = std::move(log);
        m_clock = clock ? std::move(clock) : ClockFn([] { return (int64_t)std::time(nullptr); });
        m_snap = SkySnapshot{};
        m_cache.clear();
        for (int i = 0; i < kSources; i++) {
            m_due[i] = 0;
            m_snap.status[i].state = SourceState::Waiting;
        }
        if (!m_opt.askNws) m_snap.status[(int)Source::Nws].state = SourceState::Off;
        if (!m_opt.siteSet)
            for (auto& s : m_snap.status) s.state = SourceState::NoSite;
        ReadCacheLocked();
    }

    void Start(const Options& o, FetchFn fetch, LogFn log = {}) {
        Configure(o, std::move(fetch), std::move(log));
        if (!m_opt.siteSet) {
            if (m_log) m_log("[SKYWX] No site in rig.ini: the weather is not fetched");
            return;
        }
        m_stop = false;
        m_thread = std::thread([this] { Run(); });
    }

    void Stop() {
        {
            std::lock_guard<std::mutex> lk(m_waitMu);
            m_stop = true;
        }
        m_cv.notify_all();
        if (m_thread.joinable()) m_thread.join();
    }

    // Asks the thread to fetch everything now (Dash's weather_refresh).
    void RefreshNow() {
        {
            std::lock_guard<std::mutex> lk(m_mu);
            for (auto& d : m_due) d = 0;
        }
        { std::lock_guard<std::mutex> lk(m_waitMu); m_kick = true; }
        m_cv.notify_all();
    }

    SkySnapshot Get() const {
        std::lock_guard<std::mutex> lk(m_mu);
        return m_snap;
    }

    // Does whatever is due at `now`. Returns the seconds until the next
    // thing is due. The thread calls it; a test calls it directly.
    int64_t Step(int64_t now) {
        for (int i = 0; i < kSources; i++) {
            Request req;
            {
                std::lock_guard<std::mutex> lk(m_mu);
                if (!m_opt.siteSet) return 3600;
                if (i == (int)Source::Nws && !m_opt.askNws) continue;
                if (now < m_due[i]) continue;
                req = RequestFor((Source)i, m_opt.latDeg, m_opt.lonDeg);
            }
            std::string body;
            const FetchResult r = m_fetch ? m_fetch(req.host, req.path, body) : FetchResult::Failed;
            Apply((Source)i, r, body, now);
        }
        std::lock_guard<std::mutex> lk(m_mu);
        int64_t next = (std::numeric_limits<int64_t>::max)();
        for (int i = 0; i < kSources; i++) {
            if (i == (int)Source::Nws && !m_opt.askNws) continue;
            next = (std::min)(next, m_due[i] - now);
        }
        return next == (std::numeric_limits<int64_t>::max)() ? 3600 : (std::max<int64_t>)(1, next);
    }

private:
    void Apply(Source s, FetchResult r, const std::string& body, int64_t now) {
        const int i = (int)s;
        std::string logLine;
        {
            std::lock_guard<std::mutex> lk(m_mu);
            SourceStatus& st = m_snap.status[i];
            const SourceState before = st.state;
            st.lastAttemptUnix = now;
            st.detail.clear();
            bool good = false;
            if (r == FetchResult::Offline)      st.state = SourceState::Offline;
            else if (r == FetchResult::Failed)  st.state = SourceState::Failed;
            else {
                good = ReadLocked(s, body, now, st.detail);
                st.state = good ? SourceState::Ok : SourceState::Unreadable;
                if (good) {
                    st.lastGoodUnix = now;
                    st.fromCache = false;
                    StoreCacheLocked(s, now, body);
                }
            }
            const Cadence c = CadenceOf(s);
            m_due[i] = now + (good ? c.okSec : c.failSec);
            if (st.state != before) {
                logLine = std::string("[SKYWX] ") + SourceName(s) + ": " + SourceStateText(st.state);
                if (!st.detail.empty()) logLine += " (" + st.detail + ")";
            }
        }
        // Changes of state only: a night offline would otherwise log the
        // same line every two minutes.
        if (!logLine.empty() && m_log) m_log(logLine);
    }

    // Parses a reply into the snapshot. On failure the last good one stays.
    bool ReadLocked(Source s, const std::string& body, int64_t, std::string& why) {
        switch (s) {
            case Source::OpenMeteo: {
                OmCurrent c;
                if (!ParseOpenMeteoCurrent(body, c)) { why = c.error; return false; }
                m_snap.current = c;
                return true;
            }
            case Source::SevenTimer: {
                T7Forecast f;
                if (!ParseSevenTimerAstro(body, f)) { why = f.error; return false; }
                m_snap.sevenTimer = f;
                return true;
            }
            case Source::Nws: {
                std::vector<NwsAlert> a;
                if (!ParseNwsAlerts(body, a, why)) return false;
                m_snap.alerts = std::move(a);
                m_snap.alertsKnown = true;
                return true;
            }
        }
        return false;
    }

    void ReadCacheLocked() {
        if (m_opt.cachePath.empty()) return;
        std::ifstream f(m_opt.cachePath, std::ios::binary);
        if (!f) return;
        std::stringstream ss;
        ss << f.rdbuf();
        for (const auto& e : ParseCache(ss.str())) {
            std::string why;
            if (!ReadLocked(e.source, e.body, e.fetchedUnix, why)) continue;
            SourceStatus& st = m_snap.status[(int)e.source];
            st.lastGoodUnix = e.fetchedUnix;
            st.fromCache = true;
            m_cache.push_back(e);
        }
    }

    // Keeps the latest good reply of each source and rewrites the file:
    // to a temporary name first, then renamed over, so a crash mid-write
    // leaves the old cache rather than half a new one.
    void StoreCacheLocked(Source s, int64_t now, const std::string& body) {
        if (m_opt.cachePath.empty()) return;
        bool replaced = false;
        for (auto& e : m_cache)
            if (e.source == s) { e.fetchedUnix = now; e.body = body; replaced = true; }
        if (!replaced) m_cache.push_back({s, now, body});
        namespace fs = std::filesystem;
        const fs::path dst(m_opt.cachePath);
        fs::path tmp = dst;
        tmp += ".tmp";
        {
            std::ofstream o(tmp, std::ios::binary | std::ios::trunc);
            if (!o) return;
            o << SerializeCache(m_cache);
            if (!o) return;
        }
        std::error_code ec;
        fs::rename(tmp, dst, ec);
        if (ec) fs::remove(tmp, ec);
    }

    void Run() {
        for (;;) {
            const int64_t wait = Step(m_clock());
            std::unique_lock<std::mutex> lk(m_waitMu);
            if (m_cv.wait_for(lk, std::chrono::seconds(wait),
                              [this] { return m_stop.load() || m_kick; })) {
                if (m_stop) return;
                m_kick = false;
            }
        }
    }

    mutable std::mutex      m_mu;          // guards m_opt, m_snap, m_due, m_cache
    Options                 m_opt;
    FetchFn                 m_fetch;
    LogFn                   m_log;
    ClockFn                 m_clock;
    SkySnapshot             m_snap;
    int64_t                 m_due[kSources] = {0, 0, 0};
    std::vector<CacheEntry> m_cache;

    std::thread             m_thread;
    std::atomic<bool>       m_stop{false};
    std::mutex              m_waitMu;
    std::condition_variable m_cv;
    bool                    m_kick = false;   // under m_waitMu
};

} // namespace raduk::core::sky
