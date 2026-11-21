#pragma once
// ParkController.h - Phase 156.2a: park, as the owner chose (2026-11-21).
//
// The mount (NexStar) has no park of its own, so park is a slew to a set
// position: rig.ini SkyParkAltAz=az:alt, converted to RA/Dec for this
// moment and sent with the ordinary GoTo, then tracking off once the slew
// ends. There is no default position: a park that slews somewhere the
// owner never chose is worse than no park, so an unset key refuses.
//
// One controller, three callers: the PARK button in the mount window,
// Dash's park command, and the sky run (at the end, on a stall, on the
// dark window closing, on STOP). They all park the same way.
//
// Pure: the mount, the rig and the sky are hooks, so the rules are tested
// on Linux with a simulated mount (tests/park_controller.cpp).

#include <cstdio>
#include <functional>
#include <string>

#include "../core/SkyRun.h"

namespace raduk::app {

struct ParkHooks {
    // rig.ini, read fresh each time so an edited key takes effect.
    std::function<std::string()>          parkText;   // SkyParkAltAz
    std::function<bool(raduk::core::skyrun::Limits&, std::string&)> limits;
    std::function<bool()>                 connected;
    std::function<bool()>                 aligned;
    // Alt/Az now -> RA (hours), Dec (degrees), for the configured site.
    std::function<bool(double altDeg, double azDeg, double& raH, double& decDeg)> toRADec;
    std::function<void()>                 beforeSlew;  // stop what would fight it: the tracker, Autorun
    std::function<bool(double raH, double decDeg)> gotoRADec;
    std::function<bool()>                 slewing;
    std::function<void()>                 cancelSlew;
    std::function<bool()>                 trackingOff;
    std::function<void(const std::string&)> log;       // "[PARK] ..."
};

class ParkController {
public:
    enum class State { Idle, Slewing, Parked, Failed };

    static constexpr double kSlewLimitS = 200.0;  // SlewWatch's 180 s, and a margin

    explicit ParkController(ParkHooks h) : m_h(std::move(h)) {}

    // Start a park. False with the reason when it cannot start; nothing
    // moves in that case.
    bool Request(double now, std::string& why) {
        namespace sr = raduk::core::skyrun;
        why.clear();
        if (m_state == State::Slewing) { why = "a park is already under way"; return false; }
        if (!m_h.connected || !m_h.connected()) return Refuse("the mount is not connected", why);
        if (m_h.aligned && !m_h.aligned())
            return Refuse("the mount is not aligned, and a park is a GoTo", why);

        sr::ParkPosition p;
        std::string w;
        if (!sr::ParsePark(m_h.parkText ? m_h.parkText() : std::string(), p, w))
            return Refuse(w + " (rig.ini SkyParkAltAz=az:alt)", why);
        sr::Limits lim;
        if (!m_h.limits || !m_h.limits(lim, w)) return Refuse("the slew bounds: " + w, why);
        if (!sr::ParkWithinBounds(p, lim, &w))
            return Refuse("the park position is outside the slew bounds: " + w, why);

        double ra = 0, dec = 0;
        if (!m_h.toRADec || !m_h.toRADec(p.altDeg, p.azDeg, ra, dec))
            return Refuse("the site is not set, so az:alt cannot be turned into RA/Dec", why);

        if (m_h.beforeSlew) m_h.beforeSlew();
        if (!m_h.gotoRADec || !m_h.gotoRADec(ra, dec)) return Refuse("the mount refused the GoTo", why);

        m_state = State::Slewing;
        m_started = now;
        m_target = p;
        m_reason.clear();
        char b[128];
        std::snprintf(b, sizeof b, "slewing to az %.1f alt %.1f (RA %.3fh Dec %+.2f)",
                      p.azDeg, p.altDeg, ra, dec);
        Log(b);
        return true;
    }

    void Tick(double now) {
        if (m_state != State::Slewing) return;
        const bool moving = m_h.slewing && m_h.slewing();
        if (moving) {
            if (now - m_started > kSlewLimitS) {
                if (m_h.cancelSlew) m_h.cancelSlew();
                Fail("the park slew did not finish in 200 s, so it was cancelled");
            }
            return;
        }
        if (m_h.trackingOff && !m_h.trackingOff()) {
            Fail("at the park position, but the mount refused tracking off");
            return;
        }
        m_state = State::Parked;
        Log("parked, tracking off");
    }

    // A jog, a GoTo or a STOP moves the scope off the park (or abandons
    // the slew); the state must not go on saying parked.
    void Unpark(const char* why) {
        if (m_state == State::Idle) return;
        if (m_state == State::Slewing && m_h.cancelSlew) m_h.cancelSlew();
        m_state = State::Idle;
        Log(std::string("no longer parked: ") + why);
    }

    // While parked, the mount's own alt/az. A move by anything (the
    // paddle, the hand controller, a GoTo from another panel) takes it
    // off the park; more than a degree from the position counts. Azimuth
    // differences wrap at 360.
    void WatchPosition(double altDeg, double azDeg) {
        if (m_state != State::Parked) return;
        double dAz = azDeg - m_target.azDeg;
        while (dAz > 180) dAz -= 360;
        while (dAz < -180) dAz += 360;
        const double dAlt = altDeg - m_target.altDeg;
        if (dAlt > 1.0 || dAlt < -1.0 || dAz > 1.0 || dAz < -1.0)
            Unpark("the mount moved off the park position");
    }

    State state() const { return m_state; }
    bool  parked() const { return m_state == State::Parked; }
    const std::string& reason() const { return m_reason; }

    static const char* Name(State s) {
        switch (s) {
        case State::Idle:    return "idle";
        case State::Slewing: return "slewing";
        case State::Parked:  return "parked";
        case State::Failed:  return "failed";
        }
        return "?";
    }

private:
    bool Refuse(const std::string& w, std::string& why) {
        why = w;
        Log("refused: " + w);
        return false;
    }
    void Fail(const std::string& w) {
        m_state = State::Failed;
        m_reason = w;
        Log("failed: " + w);
    }
    void Log(const std::string& s) { if (m_h.log) m_h.log("[PARK] " + s); }

    ParkHooks   m_h;
    State       m_state = State::Idle;
    double      m_started = 0;
    raduk::core::skyrun::ParkPosition m_target;
    std::string m_reason;
};

} // namespace raduk::app
