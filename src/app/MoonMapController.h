#pragma once
// ============================================================
//  MoonMapController -- Phase 162: the Moon map's wiring
//
//  Holds the projects (core/MoonMapProject.h), tonight's light on every
//  section (sky/MoonMap.h) and the runner (core/MoonMapRunner.h), and
//  carries the runner's steps out through hooks, as AutorunController
//  does for a sequence of runs:
//
//    Slew        the GoTo panel's own slew, as "Moon"
//    StartLock   PLANET LOCK, which on a frame the Moon fills guides on
//                the surface itself (phases 109 and 126)
//    StartFocus  autofocus, which has its own Moon path
//    StartRun    the RECORD path, filed under the section's name
//    Credit      the section goes green and the project is saved
//
//  Everything outside is a hook, so tests/moon_map_controller.cpp runs
//  the real project, grid and runner against a fake rig.
// ============================================================

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <functional>
#include <string>
#include <system_error>
#include <vector>

#include "../core/MoonMapProject.h"
#include "../core/MoonMapRunner.h"
#include "../sky/MoonMap.h"
#include "UiSink.h"

namespace raduk::app {

struct MoonMapHooks {
    std::function<raduk::sky::LunarView()>  view;            // the Moon now, from the site
    std::function<double()>                 jdNow;           // UT
    // Slew to a section's aim point. False with a reason when refused.
    std::function<bool(double raHours, double decDeg, const std::string& section, std::string& why)> slew;
    std::function<bool()>                   mountSlewing;
    std::function<bool(std::string& why)>   startLock;
    std::function<bool()>                   locked;
    std::function<bool()>                   moonSeen;        // something bright is in the frame
    std::function<void()>                   startFocus;
    std::function<void()>                   abortFocus;
    std::function<bool(const std::string& section)> startRecording;
    std::function<void()>                   stopRecording;
    std::function<std::string()>            lastFile;
    std::function<void(int& w, int& h)>     frameSize;
    std::function<void(const std::string&)> session;         // a Capture line in the session log
    std::function<void(ToastLevel, const std::string&)> notify;
    std::function<void()>                   changed;         // redraw; the state moved
    // Phase 162e: a section was credited with a recording; make its tile
    // (section, the recording, where the tile goes). Off this thread.
    std::function<void(const std::string&, const std::string&, const std::string&)> tileWanted;
};

class MoonMapController {
public:
    using Runner  = raduk::core::MoonMapRunner;
    using Project = raduk::core::MoonMapProject;
    Runner runner;

    explicit MoonMapController(IUiSink& ui) : m_ui(&ui) {}
    void SetHooks(MoonMapHooks h) { m_hooks = std::move(h); }
    void SetUi(IUiSink& u)        { m_ui = &u; }

    // ── Projects ──────────────────────────────────────────────
    // Where the .moonmap files live. Reads what is there, oldest project
    // first. The ACTIVE project is the one named in active.txt (phase
    // 162c): opening the app, or the map, always carries on with it until
    // the owner starts another with NEW. Without that file (a folder from
    // before 162c), the project last written to is the active one, which
    // was the old rule and is the right guess.
    static constexpr const char* kActiveFile = "active.txt";

    void Open(const std::string& dir) {
        m_dir = dir;
        m_files.clear();
        std::error_code ec;
        std::filesystem::create_directories(dir, ec);
        struct Found { double created; std::string path; std::filesystem::file_time_type written; };
        std::vector<Found> found;
        for (const auto& e : std::filesystem::directory_iterator(dir, ec)) {
            if (!e.is_regular_file(ec)) continue;
            if (e.path().extension().string() != Project::kExtension) continue;
            Project p;
            std::string why;
            const double created = p.Load(e.path().string(), why) ? p.CreatedJd() : 1e12;
            found.push_back({created, e.path().string(), e.last_write_time(ec)});
        }
        // By age, which is the order the archive is stepped through. Saving
        // a capture rewrites a file, so its write time is not its age.
        std::sort(found.begin(), found.end(), [](const Found& a, const Found& b) {
            return a.created != b.created ? a.created < b.created : a.path < b.path;
        });
        for (const auto& f : found) m_files.push_back(f.path);

        m_at = -1;
        const std::string named = ReadActiveName();
        for (size_t i = 0; i < m_files.size() && !named.empty(); ++i)
            if (std::filesystem::path(m_files[i]).filename().string() == named) m_at = (int)i;
        if (m_at < 0 && !found.empty()) {
            size_t newest = 0;
            for (size_t i = 1; i < found.size(); ++i)
                if (found[i].written > found[newest].written) newest = i;
            m_at = (int)newest;
        }
        LoadActive();
        if (m_project.Valid()) WriteActiveName();
    }

    bool HasProject() const { return m_project.Valid(); }
    // The project every run, GOTO, MARK and CLEAR acts on.
    const Project& Active() const { return m_project; }
    int  ProjectCount() const { return (int)m_files.size(); }
    int  ProjectIndex() const { return m_at; }
    const std::string& Dir() const { return m_dir; }

    // ── The archive (phase 162c) ──────────────────────────────
    // PREV and NEXT look at older projects without making them active:
    // the only thing that changes the active project is NEW. While one is
    // on screen, Shown() is it, and every command that would change a
    // project or move the mount is refused.
    bool ViewingArchive() const { return m_viewing >= 0 && m_viewing != m_at; }
    const Project& Shown() const { return ViewingArchive() ? m_archived : m_project; }
    int  ShownIndex() const { return ViewingArchive() ? m_viewing : m_at; }
    std::string ArchiveRefusal() const {
        return "viewing an archived map: BACK returns to '" + m_project.Name() + "'";
    }

    // The next or the previous project, by age. Stepping onto the active
    // one is the same as BACK.
    bool Step(int dir) {
        if (runner.Running() || m_files.size() < 2) return false;
        const int n = (int)m_files.size();
        const int to = ((ShownIndex() + dir) % n + n) % n;
        if (to == m_at) return ViewActive();
        Project p;
        std::string why;
        if (!p.Load(m_files[(size_t)to], why)) {
            Line("[MOONMAP] " + m_files[(size_t)to] + " was not read: " + why + "\n");
            return false;
        }
        m_archived = p;
        m_viewing = to;
        m_selected.clear();
        DisarmNew();
        char b[64];
        std::snprintf(b, sizeof(b), " (%d of %d)", to + 1, n);
        Line("[MOONMAP] archive: '" + p.Name() + "'" + b + ", " + p.Summary() +
             "; the active map is still '" + m_project.Name() + "'\n");
        Changed();
        return true;
    }

    // Back to the active project. False when it is already on screen.
    bool ViewActive() {
        if (!ViewingArchive()) { m_viewing = -1; return false; }
        m_viewing = -1;
        m_archived = Project{};
        m_selected.clear();
        Line("[MOONMAP] back to the active map '" + m_project.Name() + "': " + m_project.Summary() + "\n");
        Changed();
        return true;
    }

    // ── NEW asks first (phase 162c) ───────────────────────────
    // A project with captures in it is weeks of nights. NEW on it arms a
    // confirmation the window shows; CONFIRM then makes the new project
    // (the old one stays, in the archive). Anything else disarms it.
    bool NewNeedsConfirm() const { return m_project.Valid() && !m_project.Captures().empty(); }
    bool NewArmed() const { return m_newArmed; }
    void ArmNew()    { m_newArmed = true; Changed(); }
    void DisarmNew() { if (m_newArmed) { m_newArmed = false; Changed(); } }

    // Which night of the active project tonight is: the nights it already
    // has, plus one when tonight has not yet added to it.
    int NightNumber() const {
        if (!m_project.Valid()) return 0;
        const int had = m_project.Nights();
        if (had == 0) return 1;
        return had + (Jd() - m_project.LastCaptureJd() >= Project::kNightGapDays ? 1 : 0);
    }

    // A new project, which becomes the active one. False with a reason:
    // a name already used, a grid that cannot be, a sequence under way.
    bool NewProject(const std::string& name, int perSide, std::string& why) {
        m_newArmed = false;
        if (runner.Running()) { why = "a map run is going"; return false; }
        if (perSide < raduk::sky::MOONMAP_MIN_SIDE || perSide > raduk::sky::MOONMAP_MAX_SIDE) {
            why = "no frame yet, so no section size";
            return false;
        }
        const std::string path = PathFor(name);
        std::error_code ec;
        if (std::filesystem::exists(path, ec)) { why = "a project called '" + name + "' exists"; return false; }
        Project p;
        p.Init(name, perSide, Jd());
        if (!p.Save(path)) { why = "could not write " + path; return false; }
        m_before = (m_at >= 0 && m_at < (int)m_files.size()) ? m_files[(size_t)m_at] : std::string();
        m_project = p;
        m_files.push_back(path);
        m_at = (int)m_files.size() - 1;
        m_viewing = -1;
        m_archived = Project{};
        m_selected.clear();
        m_wasComplete = false;
        WriteActiveName();
        RefreshSky();
        Line("[MOONMAP] new project '" + name + "': " + std::to_string(p.Total()) + " sections\n");
        Changed();
        return true;
    }

    // Throw away the active project, which is allowed only while it holds
    // nothing: one made by mistake, or by a test. A project with a single
    // capture in it is somebody's night and is never removed from here.
    bool DiscardEmpty(std::string& why) {
        if (runner.Running()) { why = "a map run is going"; return false; }
        if (!m_project.Valid()) { why = "no project"; return false; }
        if (!m_project.Captures().empty()) { why = "the project holds captures"; return false; }
        const std::string name = m_project.Name();
        std::error_code ec;
        std::filesystem::remove(m_files[(size_t)m_at], ec);
        m_files.erase(m_files.begin() + m_at);
        // The map a mistaken NEW was pressed over is active again; failing
        // that, the newest.
        m_at = (int)m_files.size() - 1;
        for (size_t i = 0; i < m_files.size(); ++i) if (m_files[i] == m_before) m_at = (int)i;
        m_before.clear();
        LoadActive();
        if (m_project.Valid()) WriteActiveName();
        else std::filesystem::remove(std::filesystem::path(m_dir) / kActiveFile, ec);
        Line("[MOONMAP] empty project '" + name + "' discarded\n");
        Changed();
        return true;
    }

    // ── Tonight ───────────────────────────────────────────────
    // Sunlight and visibility for every section, now. Cheap enough to
    // call every few seconds; the terminator moves half a degree an hour.
    void RefreshSky() {
        m_sky.clear();
        if (!m_project.Valid() || !m_hooks.view) return;
        m_view = m_hooks.view();
        m_haveView = true;
        for (const auto& s : m_project.Sections())
            m_sky.push_back(raduk::sky::SectionSkyAt(m_view, s));
    }
    bool HaveSky() const { return m_haveView && m_sky.size() == m_project.Sections().size(); }
    const raduk::sky::SectionSky& Sky(int i) const { return m_sky[(size_t)i]; }
    const raduk::sky::LunarView&  View() const { return m_view; }

    // Wanted and fully lit, so a run tonight would add it.
    int CapturableNow() const {
        if (!HaveSky()) return 0;
        int n = 0;
        const auto& secs = m_project.Sections();
        for (size_t i = 0; i < secs.size(); ++i)
            if (!m_project.IsCaptured(secs[i].id) && m_sky[i].Capturable()) ++n;
        return n;
    }

    // ── The selection, and marking by hand ────────────────────
    const std::string& Selected() const { return m_selected; }
    void Select(const std::string& id) {
        m_selected = Shown().Find(id) ? id : std::string();
        Changed();
    }

    // A section recorded by hand, or outside the app.
    bool Mark(const std::string& id, const std::string& file = std::string(), uint32_t frames = 0) {
        if (ViewingArchive()) return false;
        const int i = IndexOf(id);
        if (i < 0) return false;
        RefreshSky();
        m_project.AddCapture(CaptureFor(i, file, frames));
        Line("[MOONMAP] " + id + " marked captured: " + m_project.Summary() + "\n");
        Saved();
        WantTile(id, file);
        return true;
    }
    bool Clear(const std::string& id) {
        if (ViewingArchive() || !m_project.Clear(id)) return false;
        std::error_code ec;
        std::filesystem::remove(TilePath(m_project, id), ec);   // the picture goes with the capture
        Line("[MOONMAP] " + id + " cleared: " + m_project.Summary() + "\n");
        Saved();
        return true;
    }

    // Slew to one section, outside a run. False with a reason.
    bool GoTo(const std::string& id, std::string& why) {
        const int i = IndexOf(id);
        if (ViewingArchive()) { why = ArchiveRefusal(); return false; }
        if (i < 0) { why = "no such section"; return false; }
        if (runner.Running()) { why = "a map run is going"; return false; }
        RefreshSky();
        if (!HaveSky()) { why = "the Moon's position is not known"; return false; }
        if (!m_sky[(size_t)i].visible) { why = id + " is past the limb tonight"; return false; }
        if (!m_hooks.slew) { why = "no mount"; return false; }
        return m_hooks.slew(m_sky[(size_t)i].raHours, m_sky[(size_t)i].decDeg, id, why);
    }

    // ── A run over every section wanted and lit ───────────────
    // `c.sections` is filled in here. False with the reason.
    bool Start(Runner::Config c, double nowSec, std::string& why) {
        if (runner.Running()) { why = "one is already going"; return false; }
        if (ViewingArchive()) { why = ArchiveRefusal(); return false; }
        if (!m_project.Valid()) { why = "no project: make one first"; return false; }
        RefreshSky();
        if (!HaveSky()) { why = "the Moon's position is not known"; return false; }
        m_order.clear();
        const auto& secs = m_project.Sections();
        for (size_t i = 0; i < secs.size(); ++i)
            if (!m_project.IsCaptured(secs[i].id) && m_sky[i].Capturable()) m_order.push_back((int)i);
        c.sections = (int)m_order.size();
        if (m_project.Complete()) { why = "the project is complete: every section is captured"; return false; }
        if (!runner.Start(c, nowSec)) { why = runner.Reason(); return false; }
        m_shown = true;
        char b[200];
        std::snprintf(b, sizeof(b), "[MOONMAP] run started on '%s': %d sections wanted and in sunlight, "
                      "runs of %.0f s, lock %s, focus every %d\n", m_project.Name().c_str(),
                      c.sections, c.runSec, c.lock ? "on" : "off", c.refocusEvery);
        Line(b);
        std::snprintf(b, sizeof(b), "Moon map: %d sections of '%s'", c.sections, m_project.Name().c_str());
        Session(b);
        Changed();
        return true;
    }

    void Stop(const char* why) { runner.Abort(why); }

    // The section the run is on, or "" when none is.
    std::string CurrentSection() const {
        if (!runner.Running()) return std::string();
        return SectionOfStep(runner.Index());
    }
    bool Pressing() const { return m_pressing; }

    // From the 500 ms readout.
    void Tick(double nowSec, bool recording, bool focusing, uint32_t frames) {
        if (!runner.Running() && !m_shown) return;

        Runner::Inputs in;
        in.recording = recording;
        in.focusing  = focusing;
        in.frames    = frames;
        // Asked only while a slew is awaited: it is a query down the
        // mount's cable, and during a recording that cable is kept quiet.
        in.slewing   = runner.State() == Runner::Phase::Slewing &&
                       m_hooks.mountSlewing && m_hooks.mountSlewing();
        in.locked    = m_hooks.locked && m_hooks.locked();
        in.moonSeen  = m_hooks.moonSeen && m_hooks.moonSeen();

        const Runner::Phase before = runner.State();
        const int unlockedBefore = runner.Unlocked();
        const Runner::Step s = runner.Tick(nowSec, in);
        const std::string id = SectionOfStep(s.index);
        char b[256];
        std::string why;

        switch (s.kind) {
        case Runner::Action::Slew: {
            // Where the section is NOW: the Moon has moved since the last
            // one, and that is the whole reason a fixed plan does not close.
            RefreshSky();
            const int i = SectionIndexOfStep(s.index);
            if (i < 0 || !HaveSky()) { runner.SlewRefused("the Moon's position is not known"); break; }
            const auto& k = m_sky[(size_t)i];
            std::snprintf(b, sizeof(b), "[MOONMAP] section %s (%d of %d): slewing, Sun %.0f deg up there\n",
                          id.c_str(), s.index + 1, runner.Total(), k.sunAltDeg);
            Line(b);
            if (!m_hooks.slew || !m_hooks.slew(k.raHours, k.decDeg, id, why))
                runner.SlewRefused(why.empty() ? "the slew was refused" : why.c_str());
            break;
        }
        case Runner::Action::StartLock:
            Line("[MOONMAP] section " + id + ": locking on the surface\n");
            if (!m_hooks.startLock || !m_hooks.startLock(why)) {
                Line("[MOONMAP] section " + id + ": lock refused: " + why + "\n");
                runner.LockRefused(nowSec);
            }
            break;
        case Runner::Action::StartFocus:
            Line("[MOONMAP] section " + id + ": focus before the run\n");
            if (m_hooks.startFocus) m_hooks.startFocus();
            break;
        case Runner::Action::AbortFocus:
            if (m_hooks.abortFocus) m_hooks.abortFocus();
            break;
        case Runner::Action::StartRun: {
            m_pressing = true;
            const bool ok = m_hooks.startRecording && m_hooks.startRecording(id);
            m_pressing = false;
            if (ok) Line("[MOONMAP] section " + id + ": recording\n");
            else    runner.RunRefused();
            break;
        }
        case Runner::Action::StopRun:
            m_pressing = true;
            if (m_hooks.stopRecording) m_hooks.stopRecording();
            m_pressing = false;
            break;
        case Runner::Action::Credit: {
            const int i = SectionIndexOfStep(s.index);
            if (i >= 0) {
                RefreshSky();
                m_project.AddCapture(CaptureFor(i, m_hooks.lastFile ? m_hooks.lastFile() : std::string(), frames));
                std::snprintf(b, sizeof(b), "[MOONMAP] section %s captured, %u frames: %s\n",
                              id.c_str(), frames, m_project.Summary().c_str());
                Line(b);
                Session("Moon map: " + id + " captured");
                Saved();
                WantTile(id, m_project.Latest(id) ? m_project.Latest(id)->file : std::string());
            }
            break;
        }
        case Runner::Action::Skipped:
            Line("[MOONMAP] section " + id + " left for another try: " + runner.Note() + "\n");
            break;
        default:
            break;
        }

        if (runner.Unlocked() > unlockedBefore)
            Line("[MOONMAP] section " + id + ": no lock, recording on the mount's own tracking\n");
        if (runner.State() != before) Changed();
        const bool ended = m_shown && !runner.Running();
        if (ended) { m_shown = false; Ended(); }
    }

    // Where the stitcher's list goes: beside the project file.
    std::string ManifestPath() const {
        return m_dir + "\\" + Project::SafeName(m_project.Name()) + "_mosaic.txt";
    }
    // Where a project's recordings go, under the output folder.
    std::string CaptureDir(const std::string& outputDir) const {
        return outputDir + "\\MoonMap_" + Project::SafeName(m_project.Name());
    }
    // "Moon_NE-A1_": the section is in the file's name.
    static std::string PrefixFor(const std::string& section) { return "Moon_" + section + "_"; }

    // Phase 162e: a captured section's picture, beside the project file:
    // moonmaps\<project>\tiles\<section>.png. Found by name, so the
    // project file needs nothing new and an older app is not upset.
    std::string TilePath(const Project& p, const std::string& section) const {
        return m_dir + "\\" + Project::SafeName(p.Name()) + "\\tiles\\" + section + ".png";
    }

private:
    void WantTile(const std::string& id, const std::string& file) {
        if (file.empty() || !m_hooks.tileWanted) return;
        m_hooks.tileWanted(id, file, TilePath(m_project, id));
    }

    void Ended() {
        const bool ok = runner.State() == Runner::Phase::Complete;
        char b[320];
        std::snprintf(b, sizeof(b), "[MOONMAP] run ended: %d captured (%d without a lock), %d left for another try, %d empty: %s\n",
                      runner.Credited(), runner.Unlocked(), runner.Skipped(), runner.Empty(), runner.Reason().c_str());
        Line(b);
        Line("[MOONMAP] " + m_project.Summary() + "\n");
        std::snprintf(b, sizeof(b), "Moon map run ended: %d captured: %s", runner.Credited(), runner.Reason().c_str());
        Session(b);
        std::snprintf(b, sizeof(b), "Moon map: %d of %d sections%s", m_project.Captured(), m_project.Total(),
                      m_project.Complete() ? " - READY FOR MOSAIC" : "");
        Notify(ok ? ToastLevel::Info : ToastLevel::Warn,
               ok ? std::string(b) : std::string(b) + " (" + runner.Reason() + ")");
        Changed();
    }

    // Save, and when that completes the map, write the stitcher's list.
    void Saved() {
        if (!m_files.empty() && m_at >= 0 && !m_project.Save(m_files[(size_t)m_at]))
            Line("[MOONMAP] could not save " + m_files[(size_t)m_at] + "\n");
        if (m_project.Complete()) {
            const std::string path = ManifestPath();
            FILE* f = nullptr;
#ifdef _WIN32
            if (fopen_s(&f, path.c_str(), "w") != 0) f = nullptr;
#else
            f = std::fopen(path.c_str(), "w");
#endif
            if (f) {
                std::fputs(m_project.Manifest().c_str(), f);
                std::fclose(f);
                Line("[MOONMAP] '" + m_project.Name() + "' is complete: READY FOR MOSAIC, files listed in " + path + "\n");
            } else {
                Line("[MOONMAP] could not write " + path + "\n");
            }
            if (!m_wasComplete) Notify(ToastLevel::Info, "Moon map '" + m_project.Name() + "' is complete - ready for mosaic");
        }
        m_wasComplete = m_project.Complete();
        Changed();
    }

    raduk::core::MoonMapCapture CaptureFor(int i, const std::string& file, uint32_t frames) const {
        raduk::core::MoonMapCapture c;
        c.section = m_project.Sections()[(size_t)i].id;
        c.jdUt    = Jd();
        c.file    = file;
        c.frames  = frames;
        if (HaveSky()) {
            c.sunAltDeg      = m_sky[(size_t)i].sunAltDeg;
            c.colongitudeDeg = m_view.colongitudeDeg;
            c.librLonDeg     = m_view.subEarthLonDeg;
            c.librLatDeg     = m_view.subEarthLatDeg;
        }
        if (m_hooks.frameSize) m_hooks.frameSize(c.frameW, c.frameH);
        return c;
    }

    std::string ReadActiveName() const {
        std::ifstream f(std::filesystem::path(m_dir) / kActiveFile);
        std::string s;
        if (f) std::getline(f, s);
        while (!s.empty() && (s.back() == '\r' || s.back() == '\n' || s.back() == ' ')) s.pop_back();
        return s;
    }
    void WriteActiveName() const {
        if (m_at < 0 || m_at >= (int)m_files.size()) return;
        std::ofstream f(std::filesystem::path(m_dir) / kActiveFile, std::ios::trunc);
        if (f) f << std::filesystem::path(m_files[(size_t)m_at]).filename().string() << "\n";
    }

    void LoadActive() {
        m_project = Project{};
        m_archived = Project{};
        m_viewing = -1;
        m_newArmed = false;
        m_selected.clear();
        m_haveView = false;
        m_sky.clear();
        if (m_at < 0 || m_at >= (int)m_files.size()) return;
        std::string why;
        if (!m_project.Load(m_files[(size_t)m_at], why)) {
            Line("[MOONMAP] " + m_files[(size_t)m_at] + " was not read: " + why + "\n");
            m_project = Project{};
            return;
        }
        m_wasComplete = m_project.Complete();
        RefreshSky();
    }

    int IndexOf(const std::string& id) const {
        const auto& secs = m_project.Sections();
        for (size_t i = 0; i < secs.size(); ++i) if (secs[i].id == id) return (int)i;
        return -1;
    }
    int SectionIndexOfStep(int step) const {
        return (step >= 0 && step < (int)m_order.size()) ? m_order[(size_t)step] : -1;
    }
    std::string SectionOfStep(int step) const {
        const int i = SectionIndexOfStep(step);
        return i >= 0 ? m_project.Sections()[(size_t)i].id : std::string();
    }
    std::string PathFor(const std::string& name) const {
        return m_dir + "\\" + Project::SafeName(name) + Project::kExtension;
    }
    double Jd() const { return m_hooks.jdNow ? m_hooks.jdNow() : 0.0; }

    void Line(const std::string& s)    { m_ui->Line(s); }
    void Session(const std::string& s) { if (m_hooks.session) m_hooks.session(s); }
    void Notify(ToastLevel l, const std::string& s) { if (m_hooks.notify) m_hooks.notify(l, s); }
    void Changed()                     { if (m_hooks.changed) m_hooks.changed(); }

    IUiSink*     m_ui;
    MoonMapHooks m_hooks;
    std::string  m_dir;
    std::vector<std::string> m_files;     // oldest first
    int          m_at = -1;               // the active project
    int          m_viewing = -1;          // an archived one on screen, or -1
    Project      m_project;
    Project      m_archived;
    bool         m_newArmed = false;
    std::string  m_before;                // the active file before the last NEW
    std::string  m_selected;
    raduk::sky::LunarView m_view;
    bool         m_haveView = false;
    std::vector<raduk::sky::SectionSky> m_sky;
    std::vector<int> m_order;             // this run's sections, as indices
    bool         m_shown = false;
    bool         m_pressing = false;
    bool         m_wasComplete = false;
};

} // namespace raduk::app
