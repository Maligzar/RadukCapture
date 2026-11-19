// ============================================================
//  Capturing the Moon map, section after section (phase 162)
//
//  src/core/MoonMapRunner.h is the order of things for one section:
//  slew, settle, lock on the surface, focus if due, record, credit.
//  src/app/MoonMapController.h carries that out through hooks and keeps
//  the project. Both are driven here on a fake clock and a fake rig,
//  with the real grid and the real project file.
//
//  THE ONES THAT MATTER:
//
//   2. A mount asked "still slewing?" the instant after a GoTo says no.
//      Believing it records one patch of Moon under every section's name
//      and shows a finished map. (MosaicRunner met this first.)
//   5. A section goes green ONLY for a run that ended by itself with
//      frames in it. Not for an aborted run, not for an empty one.
//   9. The point of the whole feature: a project fills in over nights.
//      Half the Moon at first quarter, the rest later, and only then is
//      it READY FOR MOSAIC.
// ============================================================

#include "app/MoonMapController.h"

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

using namespace raduk::app;
using Runner = raduk::core::MoonMapRunner;
using A = Runner::Action;
namespace fs = std::filesystem;

static int g_fail = 0;
static void Check(bool ok, const std::string& what, const std::string& detail = "") {
    printf("  [%s] %-66s %s\n", ok ? "PASS" : "FAIL", what.c_str(), detail.c_str());
    if (!ok) ++g_fail;
}

// ── A rig for the runner alone ──────────────────────────────
struct Bench {
    Runner r;
    double now = 100.0;
    double slewStart = 0.6, slewLen = 3.0;   // it admits to slewing late
    double slewAt = -1.0;
    bool   stuckSlewing = false;
    bool   lockWorks = true;  double lockTakes = 5.0;  double lockAt = -1.0;
    bool   moonSeen = true;
    bool   focusing = false;  double focusUntil = 0.0;
    bool   recording = false; double recUntil = 0.0;   double recLen = 4.0;
    uint32_t frames = 0, framesPerRun = 500;
    std::vector<std::string> log;
    int credits = 0, stops = 0, focusAborts = 0;
    std::vector<int> credited;

    Runner::Config Cfg(int n) {
        Runner::Config c;
        c.sections = n; c.runSec = 4.0; c.settleSec = 4.0; c.lockLimitSec = 30.0;
        return c;
    }
    Runner::Inputs In() {
        Runner::Inputs in;
        const double t = now - slewAt;
        in.slewing = slewAt >= 0 && (stuckSlewing ? t >= slewStart : (t >= slewStart && t < slewStart + slewLen));
        in.locked = lockWorks && lockAt >= 0 && now - lockAt >= lockTakes;
        in.moonSeen = moonSeen;
        if (focusing && now >= focusUntil) focusing = false;
        in.focusing = focusing;
        if (recording && now >= recUntil) { recording = false; frames = framesPerRun; }
        in.recording = recording;
        in.frames = frames;
        return in;
    }
    Runner::Step Tick(double dt = 0.5) {
        now += dt;
        const Runner::Step s = r.Tick(now, In());
        switch (s.kind) {
            case A::Slew:       log.push_back("slew" + std::to_string(s.index)); slewAt = now; lockAt = -1.0; break;
            case A::StartLock:  log.push_back("lock" + std::to_string(s.index)); lockAt = now; break;
            case A::StartFocus: log.push_back("focus" + std::to_string(s.index)); focusing = true; focusUntil = now + 3.0; break;
            case A::AbortFocus: ++focusAborts; focusing = false; break;
            case A::StartRun:   log.push_back("run" + std::to_string(s.index)); recording = true; recUntil = now + recLen; frames = 0; break;
            case A::StopRun:    ++stops; recording = false; break;
            case A::Credit:     log.push_back("credit" + std::to_string(s.index)); ++credits; credited.push_back(s.index); break;
            case A::Skipped:    log.push_back("skip" + std::to_string(s.index)); break;
            default: break;
        }
        return s;
    }
    void Run(int maxTicks = 4000) { for (int i = 0; i < maxTicks && r.Running(); ++i) Tick(); Tick(); }
    std::string Joined() const { std::string o; for (const auto& l : log) o += l + " "; return o; }
};

// ── A rig for the controller ────────────────────────────────
struct Night {
    RecordingUiSink ui;
    MoonMapController c{ui};
    fs::path dir;
    double now = 1000.0, jd = 2461000.5;
    int light = 0;                 // 0 full, 1 east half lit, 2 new
    bool mountSlewing = false; int slewTicks = 0;
    bool locked = false;  int lockTicks = -1;
    bool recording = false; int recTicks = 0;
    bool focusing = false; int focusTicks = 0;
    uint32_t frames = 0;
    bool slewOk = true, recordOk = true, pressingSeen = false;
    std::vector<std::string> slewedTo, recorded;
    std::map<std::string, std::pair<double, double>> coords;
    std::vector<std::pair<ToastLevel, std::string>> toasts;
    int changes = 0, locks = 0, focusStarts = 0;
    std::string lastFile;

    explicit Night(const fs::path& d, bool wipe) : dir(d) {
        std::error_code ec;
        if (wipe) fs::remove_all(dir, ec);
        MoonMapHooks h;
        h.view = [this] {
            raduk::sky::LunarView v = raduk::sky::LunarViewAt(jd, raduk::sky::LunarObserver{});
            if (light == 0) v.sunDir = (v.centre * -1.0).Unit();
            if (light == 1) v.sunDir = v.axisY;
            if (light == 2) v.sunDir = v.centre.Unit();
            return v;
        };
        h.jdNow = [this] { return jd; };
        h.slew = [this](double ra, double dec, const std::string& id, std::string& why) {
            if (!slewOk) { why = "cord wrap"; return false; }
            slewedTo.push_back(id); coords[id] = {ra, dec};
            mountSlewing = true; slewTicks = 3; locked = false; lockTicks = -1;
            return true;
        };
        h.mountSlewing = [this] { return mountSlewing; };
        h.startLock = [this](std::string&) { ++locks; lockTicks = 3; return true; };
        h.locked = [this] { return locked; };
        h.moonSeen = [] { return true; };
        h.startFocus = [this] { ++focusStarts; focusing = true; focusTicks = 3; };
        h.abortFocus = [this] { focusing = false; };
        h.startRecording = [this](const std::string& id) {
            pressingSeen = c.Pressing();
            if (!recordOk) return false;
            recorded.push_back(id); recording = true; recTicks = 4; frames = 0;
            lastFile = (dir / "caps" / (MoonMapController::PrefixFor(id) + "x.ser")).string();
            return true;
        };
        h.stopRecording = [this] { recording = false; };
        h.lastFile = [this] { return lastFile; };
        h.frameSize = [](int& w, int& hgt) { w = 3840; hgt = 2160; };
        h.notify = [this](ToastLevel l, const std::string& s) { toasts.push_back({l, s}); };
        h.changed = [this] { ++changes; };
        c.SetHooks(h);
        c.Open(dir.string());
    }
    void Step() {
        now += 0.5; jd += 0.5 / 86400.0;
        if (mountSlewing && --slewTicks <= 0) mountSlewing = false;
        if (lockTicks > 0 && --lockTicks == 0) locked = true;
        if (focusing && --focusTicks <= 0) focusing = false;
        if (recording) { frames += 100; if (--recTicks <= 0) recording = false; }
        c.Tick(now, recording, focusing, frames);
    }
    bool Start(std::string& why, int refocus = 0) {
        Runner::Config cfg;
        cfg.runSec = 2.0; cfg.settleSec = 1.0; cfg.slewMinSec = 0.5; cfg.lockLimitSec = 20.0; cfg.refocusEvery = refocus;
        return c.Start(cfg, now, why);
    }
    void RunToEnd(int maxTicks = 20000) { for (int i = 0; i < maxTicks && c.runner.Running(); ++i) Step(); Step(); }
    bool Toasted(const std::string& s) const {
        for (const auto& t : toasts) if (t.second.find(s) != std::string::npos) return true;
        return false;
    }
};

int main() {
    printf("\n1. One section after another, in order\n");
    {
        Bench b;
        Runner::Config c = b.Cfg(3);
        c.refocusEvery = 2;
        Check(b.r.Start(c, b.now), "a run of three starts");
        b.Run();
        Check(b.r.State() == Runner::Phase::Complete, "and completes", b.r.Reason());
        Check(b.Joined() == "slew0 lock0 focus0 run0 credit0 slew1 lock1 run1 credit1 slew2 lock2 focus2 run2 credit2 ",
              "slew, lock, focus when due, record, credit", b.Joined());
        Check(b.r.Credited() == 3 && b.r.Skipped() == 0 && b.r.Unlocked() == 0, "three credited, none skipped");
    }

    printf("\n2. A slew is not over before it has begun\n");
    {
        Bench b;
        b.r.Start(b.Cfg(2), b.now);
        Check(b.Tick(0.1).kind == A::Slew, "the first thing asked for is the slew");
        b.Tick(0.1); b.Tick(0.1);     // the mount still says "not slewing"
        Check(b.r.State() == Runner::Phase::Slewing, "0.3 s on, with the mount not yet moving, it is still slewing");
        b.Run();
        Check(b.r.Credited() == 2 && b.log[2].rfind("run0", 0) == 0, "and the run starts only after it arrived", b.Joined());

        Bench stuck;
        stuck.stuckSlewing = true;
        stuck.r.Start(stuck.Cfg(2), stuck.now);
        stuck.Run();
        Check(stuck.r.State() == Runner::Phase::Failed && stuck.r.Reason() == "a slew did not finish",
              "a mount that never stops slewing fails the run", stuck.r.Reason());
        Check(stuck.credits == 0, "with nothing credited");
    }

    printf("\n3. A section that will not lock\n");
    {
        Bench b;
        b.lockWorks = false;
        b.r.Start(b.Cfg(2), b.now);
        b.Run();
        Check(b.r.State() == Runner::Phase::Complete && b.r.Credited() == 2 && b.r.Unlocked() == 2,
              "with the Moon in the frame it is recorded all the same, and counted", b.Joined());

        Bench dark;
        dark.lockWorks = false; dark.moonSeen = false;
        dark.r.Start(dark.Cfg(5), dark.now);
        dark.Run();
        Check(dark.r.State() == Runner::Phase::Failed && dark.credits == 0, "with nothing in the frame, nothing is recorded");
        Check(dark.Joined() == "slew0 lock0 skip0 slew1 lock1 skip1 slew2 lock2 ",
              "two sections are left wanted and the third ends the run", dark.Joined());
        Check(dark.r.Reason().find("is the Moon there") != std::string::npos, "and it asks the right question", dark.r.Reason());

        Bench off;
        Runner::Config c = off.Cfg(2);
        c.lock = false;
        off.r.Start(c, off.now);
        off.Run();
        Check(off.Joined() == "slew0 run0 credit0 slew1 run1 credit1 ", "with the lock switched off it is not asked for", off.Joined());
    }

    printf("\n4. Runs that come back empty\n");
    {
        Bench b;
        b.framesPerRun = 0;
        b.r.Start(b.Cfg(4), b.now);
        b.Run();
        Check(b.r.State() == Runner::Phase::Failed && b.credits == 0 && b.r.Empty() == 2,
              "two empty runs in a row end it, and neither is credited", b.r.Reason());
    }

    printf("\n5. Stopped mid-run: the file is closed and the section stays wanted\n");
    {
        Bench b;
        b.r.Start(b.Cfg(3), b.now);
        while (b.r.State() != Runner::Phase::Recording) b.Tick();
        b.Tick();
        b.r.Abort("stopped by the operator");
        const Runner::Step s = b.Tick();
        Check(s.kind == A::StopRun && b.stops == 1, "one last StopRun is owed");
        for (int i = 0; i < 20; ++i) b.Tick();
        Check(b.credits == 0 && b.r.State() == Runner::Phase::Aborted, "and no credit follows", b.Joined());

        Bench hung;
        hung.recLen = 1e9;
        hung.r.Start(hung.Cfg(2), hung.now);
        hung.Run();
        Check(hung.r.State() == Runner::Phase::Failed && hung.stops == 1 && hung.credits == 0,
              "a run that never ends is stopped and fails it", hung.r.Reason());

        Bench foc;
        Runner::Config c = foc.Cfg(2);
        c.refocusEvery = 1;
        foc.r.Start(c, foc.now);
        while (foc.r.State() != Runner::Phase::Focusing) foc.Tick();
        foc.r.Abort("stopped");
        foc.Tick();
        Check(foc.focusAborts == 1, "stopped mid-sweep, the sweep is aborted");
    }

    printf("\n6. What it will not start on\n");
    {
        Runner r;
        Runner::Config c;
        c.sections = 0; c.runSec = 10;
        Check(!r.Start(c, 0) && r.Reason().find("sunlight") != std::string::npos, "nothing wanted and lit", r.Reason());
        c.sections = 3; c.runSec = 0;
        Check(!r.Start(c, 0) && r.Reason().find("limit") != std::string::npos, "no run length", r.Reason());
        Check(!r.Running(), "and it is not running");
        Check(!raduk::core::MOON_MAP_RUN_PROVEN_ON_SKY, "the runner does not claim a real mount");
    }

    const fs::path dir = fs::temp_directory_path() / "raduk_moon_map_run";

    printf("\n7. A project, made and kept\n");
    {
        Night n(dir, true);
        std::string why;
        Check(!n.c.HasProject(), "an empty folder has no project");
        Check(!n.Start(why) && why.find("no project") != std::string::npos, "and nothing to run", why);
        Check(n.c.NewProject("Moon 2026-11", 2, why), "a new project is made", why);
        Check(n.c.Active().Total() == 16 && n.c.HaveSky(), "two a side is sixteen sections, with tonight's light");
        Check(fs::exists(dir / "Moon_2026-11.moonmap"), "and is on disk at once");
        Check(!n.c.NewProject("Moon 2026-11", 2, why), "the same name again is refused", why);
        Check(!n.c.NewProject("x", 0, why), "and a grid of nothing", why);
    }

    printf("\n8. A whole map in one full-Moon night\n");
    {
        Night n(dir, false);
        std::string why;
        Check(n.c.HasProject() && n.c.Active().Name() == "Moon 2026-11", "the project is found again on opening");
        Check(n.c.CapturableNow() == 16, "at full Moon all sixteen can be taken");
        Check(n.Start(why, 8), "the run starts", why);
        Check(!n.c.GoTo("NE-A1", why), "a hand GoTo is refused while it runs", why);
        n.RunToEnd();
        Check(n.c.runner.State() == Runner::Phase::Complete, "and completes", n.c.runner.Reason());
        Check(n.slewedTo.size() == 16 && n.recorded.size() == 16 && n.locks == 16, "sixteen slews, locks and recordings");
        Check(n.slewedTo == n.recorded && n.slewedTo.front() == n.c.Active().Sections().front().id,
              "each recording is of the section just slewed to, in grid order");
        Check(n.focusStarts == 2, "focus before the first and again after eight", std::to_string(n.focusStarts));
        Check(n.pressingSeen, "its RECORD press is told apart from a person's");
        Check(n.c.Active().Complete(), "every section is captured");
        const auto* cap = n.c.Active().Latest("SW-B2");
        Check(cap && cap->file.find("Moon_SW-B2_") != std::string::npos && cap->frames == 400 && cap->frameW == 3840,
              "a capture names its section's file, its frames and its frame", cap ? cap->file : "");
        Check(MoonMapController::PrefixFor("NE-A1") == "Moon_NE-A1_", "the section is in the file's name");
        Check(n.Toasted("ready for mosaic") && n.ui.Saw("READY FOR MOSAIC"), "and it says READY FOR MOSAIC");
        Check(fs::exists(n.c.ManifestPath()), "the stitcher's list is written", n.c.ManifestPath());
        std::ifstream mf(n.c.ManifestPath());
        std::stringstream ms; ms << mf.rdbuf();
        Check(ms.str().find("Moon_NW-A1_") != std::string::npos && ms.str().find("MISSING") == std::string::npos,
              "with a file for every section");
        // Different places on the sky, all of them on the Moon.
        const auto a = n.coords["NE-B2"], b = n.coords["SW-B2"];
        Check(std::fabs(a.second - b.second) > 0.2 && std::fabs(a.second - b.second) < 0.6,
              "opposite corners are aimed about 0.4 degrees apart", std::to_string(a.second - b.second));
        Check(!n.Start(why) && why.find("complete") != std::string::npos, "a complete project has nothing left to run", why);
    }

    printf("\n9. Over two nights: half at first quarter, the rest later\n");
    {
        Night q(dir, true);
        std::string why;
        q.light = 1;
        q.c.NewProject("Two nights", 2, why);
        const int lit = q.c.CapturableNow();
        Check(lit > 0 && lit < 16, "at first quarter only part of the map is in sunlight", std::to_string(lit));
        Check(q.Start(why), "the run starts on those", why);
        q.RunToEnd();
        Check(q.c.Active().Captured() == lit && !q.c.Active().Complete(), "they are captured and the project is not complete");
        bool east = true;
        for (const auto& id : q.recorded) if (id[1] != 'E') east = false;
        Check(east, "every one of them on the east side");
        Check(!q.Toasted("ready for mosaic") && !fs::exists(q.c.ManifestPath()), "nothing says ready yet");
        Check(!q.Start(why), "and a second run the same night has nothing to do", why);

        Night f(dir, false);          // another night, the app started again
        Check(f.c.Active().Name() == "Two nights" && f.c.Active().Captured() == lit, "the project remembers the first night");
        f.light = 0;
        f.c.RefreshSky();
        Check(f.c.CapturableNow() == 16 - lit, "at full Moon the rest are wanted, and only the rest");
        Check(f.Start(why), "the second night's run starts", why);
        f.RunToEnd();
        Check((int)f.recorded.size() == 16 - lit, "it records only what was missing");
        Check(f.c.Active().Complete() && f.Toasted("ready for mosaic"), "and now it is READY FOR MOSAIC");
    }

    printf("\n10. By hand, and when things are refused\n");
    {
        Night n(dir, true);
        std::string why;
        n.light = 2;
        n.c.NewProject("By hand", 2, why);
        Check(!n.Start(why) && why.find("sunlight") != std::string::npos, "at new Moon a run has nothing in sunlight", why);
        n.light = 0;
        n.c.Select("NE-A1");
        Check(n.c.Selected() == "NE-A1", "a section is picked");
        n.c.Select("nowhere");
        Check(n.c.Selected().empty(), "a name that is not a section picks nothing");
        Check(n.c.GoTo("NE-A1", why) && n.slewedTo.back() == "NE-A1", "GoTo slews to a section", why);
        Check(n.c.Mark("NE-A1", "D:\\hand.ser", 250) && n.c.Active().IsCaptured("NE-A1"), "and it can be marked captured");
        Check(!n.c.Mark("ZZ-A1"), "but not a section that does not exist");
        Check(n.c.Clear("NE-A1") && !n.c.Active().IsCaptured("NE-A1"), "and cleared again");
        Night again(dir, false);
        Check(!again.c.Active().IsCaptured("NE-A1") && again.c.Active().Name() == "By hand", "both were saved");

        n.slewOk = false;
        Check(n.Start(why), "a run starts", why);
        n.RunToEnd();
        Check(n.c.runner.State() == Runner::Phase::Failed && n.c.runner.Reason() == "cord wrap",
              "and a refused slew ends it with the reason", n.c.runner.Reason());
        n.slewOk = true; n.recordOk = false;
        n.Start(why);
        n.RunToEnd();
        Check(n.c.runner.State() == Runner::Phase::Failed && n.c.Active().Captured() == 0,
              "a recording that will not start ends it, with nothing credited", n.c.runner.Reason());

        // Two projects: the new one is active, and PREV only looks back.
        n.recordOk = true;
        Check(n.c.NewProject("Later", 2, why) && n.c.ProjectCount() == 2 && n.c.Active().Name() == "Later", "a second project is the active one");
        Check(n.c.Step(-1) && n.c.Shown().Name() == "By hand" && n.c.Active().Name() == "Later",
              "PREV shows the earlier one without making it active");
        Check(n.c.Step(1) && !n.c.ViewingArchive() && n.c.Shown().Name() == "Later", "and NEXT comes back");
    }

    printf("\n11. One long project: it resumes, and only NEW changes it (162c)\n");
    {
        Night n(dir, true);
        std::string why;
        n.c.NewProject("First map", 2, why);
        Check(n.c.NightNumber() == 1, "an empty map is on its first night");
        Check(!n.c.NewNeedsConfirm(), "and NEW on it needs no confirmation: nothing would be set aside");
        n.c.Mark("NE-A1");
        Check(n.c.NightNumber() == 1 && n.c.Active().Nights() == 1, "a capture tonight keeps it night 1");
        n.jd += 1.0;
        Check(n.c.NightNumber() == 2, "the next evening is night 2");
        n.c.Mark("NE-A2");
        n.jd += 0.1;
        n.c.Mark("NW-A1");
        Check(n.c.Active().Nights() == 2, "captures hours apart are one night, a day apart two", std::to_string(n.c.Active().Nights()));
        Check(n.c.NewNeedsConfirm(), "now NEW must be confirmed");

        n.jd += 2.0;
        Check(n.c.NewProject("Second map", 2, why) && n.c.Active().Name() == "Second map", "a second map is made (CONFIRM's path)", why);
        Check(n.c.Step(-1) && n.c.ViewingArchive() && n.c.Shown().Name() == "First map", "the first is in the archive");
        Check(!n.Start(why) && why.find("archived") != std::string::npos, "START is refused while it is shown", why);
        Check(!n.c.GoTo("NE-A1", why) && why.find("archived") != std::string::npos, "so is GOTO", why);
        Check(!n.c.Mark("SW-A1") && !n.c.Clear("NE-A1"), "and MARK and CLEAR");
        Check(n.c.Shown().IsCaptured("NE-A1") && !n.c.Active().IsCaptured("SW-A1"), "and nothing changed in either map");
        n.c.Select("NE-A1");
        Check(n.c.Selected() == "NE-A1", "a section of the archived map can be picked, to look at");
        Check(n.c.ViewActive() && !n.c.ViewingArchive() && n.c.Selected().empty(), "BACK returns to the active map");
        Check(!n.c.ViewActive(), "and BACK there does nothing");

        // Reopen after the FIRST map's file is rewritten (newer on disk):
        // the active map is still the second.
        { std::ofstream touch(dir / "First_map.moonmap", std::ios::app); touch << "# touched\n"; }
        Night again(dir, false);
        Check(again.c.Active().Name() == "Second map", "reopening carries on with the active map, not the newest file",
              again.c.Active().Name());
        Check(again.c.ProjectCount() == 2 && !again.c.ViewingArchive(), "and opens on it, not on the archive");
        Check(fs::exists(dir / MoonMapController::kActiveFile), "the active map is named in a file of its own");

        // Without that file (a folder from before 162c) the newest file is the guess.
        fs::remove(dir / MoonMapController::kActiveFile);
        Night old(dir, false);
        Check(old.c.Active().Name() == "First map", "without it, the map last written to is active", old.c.Active().Name());

        // A NEW made by mistake and discarded hands back the map it was pressed over.
        old.c.NewProject("Oops", 2, why);
        Check(old.c.DiscardEmpty(why) && old.c.Active().Name() == "First map", "DISCARD returns to the map before NEW", why);

        // Arming: the controller holds it until something takes it back.
        old.c.ArmNew();
        Check(old.c.NewArmed(), "NEW can be armed");
        old.c.DisarmNew();
        Check(!old.c.NewArmed() && old.c.Active().Name() == "First map", "and disarmed with nothing changed");
    }

    std::error_code ec;
    fs::remove_all(dir, ec);
    printf("\n%s\n", g_fail == 0 ? "ALL PASSED" : "FAILURES");
    return g_fail == 0 ? 0 : 1;
}
