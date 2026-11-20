// ============================================================
//  Phase 165.1: fetching the sky's weather (core/SkyWeatherClient.h)
//
//  The fetch is a function the test passes in, serving the replies under
//  tests/data/sky_weather, so nothing here touches the network:
//    1. No site in rig.ini: no request at all, and every source says why.
//    2. The requests: the site rounded to 0.01 degree, HTTPS hosts, UTC.
//    3. A first pass fetches all three; each comes due again on its own
//       cadence, sooner after a failure.
//    4. Offline: the source says "offline", not "not reachable", and the
//       last good reply stays in use.
//    5. A reply that does not parse leaves the last good one in place.
//    6. Freshness: the dew, wind and dust readings come from fresh data
//       only, and say "unknown" when it is stale.
//    7. The cache: written after each good reply, read back at start
//       with its age; a damaged tail is dropped.
//    8. The thread: starts, fetches, refreshes on request and stops,
//       with no data race (this file also runs under TSan).
// ============================================================

#include "core/SkyWeatherClient.h"

#include <atomic>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <thread>

using namespace raduk::core::sky;

static int g_fail = 0;
static void Check(bool ok, const std::string& what, const std::string& detail = "") {
    printf("  [%s] %-62s %s\n", ok ? "PASS" : "FAIL", what.c_str(), detail.c_str());
    if (!ok) g_fail++;
}

static std::string Data(const char* name) {
    std::filesystem::path p = std::filesystem::path(__FILE__).parent_path() / "data" / "sky_weather" / name;
    std::ifstream f(p, std::ios::binary);
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

// A fake network: a reply (or a result) per host, and a log of requests.
struct FakeNet {
    std::map<std::string, std::string> body;
    std::map<std::string, FetchResult> result;
    std::vector<std::string> asked;
    std::mutex mu;
    FetchFn Fn() {
        return [this](const std::string& h, const std::string& p, std::string& out) {
            std::lock_guard<std::mutex> lk(mu);
            asked.push_back(h + p);
            auto r = result.count(h) ? result[h] : FetchResult::Ok;
            if (r == FetchResult::Ok) out = body[h];
            return r;
        };
    }
    void Serve() {
        body["api.open-meteo.com"] = Data("open_meteo_current_greenwich.json");
        body["www.7timer.info"]    = Data("seventimer_astro_layout.json");
        body["api.weather.gov"]    = Data("nws_alerts_alaska.json");
    }
};

// The captured Open-Meteo reading is 2026-11-19 20:15 UTC.
static const int64_t T0 = UnixFromUtc(2026, 11, 19, 20, 20);

static SkyWeatherClient::Options Site(const std::string& cache = "") {
    SkyWeatherClient::Options o;
    o.siteSet = true;
    o.latDeg = 51.4779;
    o.lonDeg = -0.0015;
    o.cachePath = cache;
    return o;
}

static void NoSite() {
    printf("\n1. No site\n");
    FakeNet net;
    SkyWeatherClient c;
    SkyWeatherClient::Options o;   // siteSet false, 0,0
    c.Configure(o, net.Fn());
    c.Step(T0);
    Check(net.asked.empty(), "nothing is asked for 0,0");
    Check(c.Get().Of(Source::OpenMeteo).state == SourceState::NoSite, "and the reason is the site",
          SourceStateText(c.Get().Of(Source::OpenMeteo).state));
}

static void Requests() {
    printf("\n2. The requests\n");
    const Request om = RequestFor(Source::OpenMeteo, 51.4779, -0.0015);
    Check(om.host == "api.open-meteo.com", "Open-Meteo's host");
    Check(om.path.find("latitude=51.48&longitude=0.00&") != std::string::npos,
          "the site rounded to 0.01 degree, no -0.00", om.path.substr(0, 45));
    Check(om.path.find("timezone=UTC") != std::string::npos, "times asked for in UTC");
    const Request t7 = RequestFor(Source::SevenTimer, 33.4567, -112.3456);
    Check(t7.host == "www.7timer.info" && t7.path.find("lon=-112.35&lat=33.46") != std::string::npos,
          "7Timer: lon first, rounded", t7.path);
    const Request nws = RequestFor(Source::Nws, 33.4567, -112.3456);
    Check(nws.path == "/alerts/active?point=33.46,-112.35", "NWS: a point, rounded", nws.path);
}

static void Cadences() {
    printf("\n3. The first pass, then each on its own cadence\n");
    FakeNet net;
    net.Serve();
    SkyWeatherClient c;
    std::vector<std::string> log;
    c.Configure(Site(), net.Fn(), [&](const std::string& l) { log.push_back(l); }, [] { return T0; });
    const int64_t wait = c.Step(T0);
    Check(net.asked.size() == 3, "all three asked on the first pass", std::to_string(net.asked.size()));
    auto s = c.Get();
    Check(s.Of(Source::OpenMeteo).state == SourceState::Ok && s.Of(Source::SevenTimer).state == SourceState::Ok &&
          s.Of(Source::Nws).state == SourceState::Ok, "all three ok");
    Check(s.current.valid && s.sevenTimer.valid && s.alerts.size() == 13, "and read");
    Check(wait == 600, "next due in 10 minutes", std::to_string(wait));
    Check(log.size() == 3, "one log line per change of state", log.empty() ? "" : log[0]);

    net.asked.clear();
    c.Step(T0 + 599);
    Check(net.asked.empty(), "nothing before it is due");
    c.Step(T0 + 600);
    Check(net.asked.size() == 2, "at 10 minutes: Open-Meteo and NWS, not 7Timer", std::to_string(net.asked.size()));
    net.asked.clear();
    c.Step(T0 + 3600);
    Check(net.asked.size() == 3, "at an hour: 7Timer as well");

    // A failure brings it back sooner.
    net.result["api.open-meteo.com"] = FetchResult::Failed;
    net.asked.clear();
    c.Step(T0 + 4200);
    s = c.Get();
    Check(s.Of(Source::OpenMeteo).state == SourceState::Failed, "a failure is said",
          SourceStateText(s.Of(Source::OpenMeteo).state));
    Check(s.current.valid, "and the last good reading stays");
    net.asked.clear();
    c.Step(T0 + 4200 + 120);
    Check(net.asked.size() == 1, "tried again two minutes later");
}

static void Offline() {
    printf("\n4. Offline\n");
    FakeNet net;
    net.Serve();
    SkyWeatherClient c;
    c.Configure(Site(), net.Fn());
    c.Step(T0);
    net.result["api.open-meteo.com"] = FetchResult::Offline;
    net.result["www.7timer.info"] = FetchResult::Offline;
    net.result["api.weather.gov"] = FetchResult::Offline;
    c.Step(T0 + 3600);
    const auto s = c.Get();
    Check(s.Of(Source::OpenMeteo).state == SourceState::Offline, "the source says offline",
          SourceStateText(s.Of(Source::OpenMeteo).state));
    Check(s.current.valid && s.Of(Source::OpenMeteo).lastGoodUnix == T0, "the last good reply stays, with its time");
    Check(!s.CurrentFresh(T0 + 3600), "but an hour on it is no longer fresh");
}

static void Unreadable() {
    printf("\n5. A reply that does not parse\n");
    FakeNet net;
    net.Serve();
    SkyWeatherClient c;
    c.Configure(Site(), net.Fn());
    c.Step(T0);
    net.body["api.open-meteo.com"] = Data("open_meteo_daily_limit.json");
    c.Step(T0 + 600);
    const auto s = c.Get();
    Check(s.Of(Source::OpenMeteo).state == SourceState::Unreadable, "it is called unreadable");
    Check(s.Of(Source::OpenMeteo).detail.find("Daily API request limit") != std::string::npos,
          "with the service's reason", s.Of(Source::OpenMeteo).detail);
    Check(s.current.valid && s.Of(Source::OpenMeteo).lastGoodUnix == T0, "and the good reading is kept");
}

static void Freshness() {
    printf("\n6. Derived readings use fresh data only\n");
    FakeNet net;
    net.Serve();
    SkyWeatherClient c;
    c.Configure(Site(), net.Fn());
    c.Step(T0);
    const auto s = c.Get();
    // 8.8 C over a 5.9 C dew point: a 2.9 C spread, under 4.4: elevated.
    Check(s.Dew(T0) == DewRisk::Elevated, "dew from the reading", DewRiskText(s.Dew(T0)));
    Check(s.Wind(T0).base == 100, "wind from the reading");
    Check(s.Dust(T0).observed && s.Dust(T0).category == 0, "dust observed, clear");
    const int64_t late = T0 + kCurrentFreshSec + 1;
    Check(s.Dew(late) == DewRisk::Unknown && s.Wind(late).base == -1 && !s.Dust(late).observed,
          "past 30 minutes: all unknown, not the old values");
    Check(s.SeeingNow(UnixFromUtc(2026, 11, 19, 15, 10)) != nullptr, "7Timer's timepoint for the hour");
}

static void Cache() {
    printf("\n7. The cache\n");
    namespace fs = std::filesystem;
    std::error_code ec;
    const fs::path dir = fs::temp_directory_path(ec) / "raduk_sky_weather_client";
    fs::remove_all(dir, ec);
    fs::create_directories(dir, ec);
    const std::string path = (dir / "weather_cache.txt").string();
    {
        FakeNet net;
        net.Serve();
        SkyWeatherClient c;
        c.Configure(Site(path), net.Fn());
        c.Step(T0);
    }
    Check(fs::exists(path), "written after the good replies");
    Check(!fs::exists(path + ".tmp"), "and no temporary file left");

    FakeNet none;   // a restart with no network
    SkyWeatherClient c2;
    c2.Configure(Site(path), none.Fn());
    auto s = c2.Get();
    Check(s.current.valid && s.sevenTimer.valid && s.alertsKnown, "read back at start, through the parsers");
    Check(s.Of(Source::OpenMeteo).fromCache && s.Of(Source::OpenMeteo).lastGoodUnix == T0,
          "marked as from the cache, with the time it came");
    Check(s.CurrentFresh(T0 + 60) && !s.CurrentFresh(T0 + 3600), "fresh only as long as its age allows");

    // Round trip, and a damaged tail.
    std::vector<CacheEntry> es = {{Source::Nws, 5, "{}"}, {Source::SevenTimer, 6, "a\nb"}};
    const std::string text = SerializeCache(es);
    auto back = ParseCache(text);
    Check(back.size() == 2 && back[1].body == "a\nb" && back[1].fetchedUnix == 6, "entries round-trip, newlines and all");
    back = ParseCache(text.substr(0, text.size() - 2));
    Check(back.size() == 1, "a cut-off last entry is dropped, the first kept");
    Check(ParseCache("garbage\n").empty(), "a file that is not a cache gives nothing");
    fs::remove_all(dir, ec);
}

static void Thread() {
    printf("\n8. The thread\n");
    FakeNet net;
    net.Serve();
    SkyWeatherClient c;
    c.Start(Site(), net.Fn());
    auto waitFor = [&](size_t n) {
        for (int i = 0; i < 400; i++) {
            { std::lock_guard<std::mutex> lk(net.mu); if (net.asked.size() >= n) return true; }
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        return false;
    };
    Check(waitFor(3), "the thread fetches all three at start");
    for (int i = 0; i < 50; i++) (void)c.Get();   // reads beside the thread
    c.RefreshNow();
    Check(waitFor(6), "RefreshNow fetches again at once");
    c.Stop();
    Check(true, "and it stops");
    SkyWeatherClient none;
    SkyWeatherClient::Options o;
    none.Start(o, net.Fn());
    Check(true, "with no site, no thread is started, and Stop is safe");
}

int main() {
    printf("sky_weather_client (phase 165.1)\n");
    NoSite();
    Requests();
    Cadences();
    Offline();
    Unreadable();
    Freshness();
    Cache();
    Thread();
    printf("\n%s\n", g_fail ? "SOME CHECKS FAILED" : "ALL CHECKS PASSED");
    return g_fail ? 1 : 0;
}
