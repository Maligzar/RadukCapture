#pragma once
// DarkRunController.h - Phase 173: RUN DARKS (the owner, 2026-11-22).
//
// "At the end of the night I click run darks and it runs my darks based on
// what is in the capture folders for the night." The plan is
// core/DarkPlan.h; this runs it, one set after another:
//   apply the frame and bit depth, the exposure and gain, AE off;
//   settle (the camera's first frames after a change are not the new ones);
//   record `frames` frames as frame type Dark into the Darks folder, named
//   for the set; wait for the file to close; next set.
// At the end, or on Stop, the camera is put back exactly as it was.
//
// Two presses, like PARK: the first shows the plan and asks for the cap,
// the second runs it. A cap the app cannot see is the owner's to fit.
//
// Pure: the camera and the recorder are hooks, tested on Linux
// (tests/dark_plan.cpp).

#include <functional>
#include <string>
#include <vector>

#include "../core/DarkPlan.h"

namespace raduk::app {

struct DarkCameraState {
    int    width = 0, height = 0, bitDepth = 0;
    double exposureMs = 0.0, gain = 0.0;
    bool   aeEnabled = false;   // AE on at all
    bool   aeAuto = false;      // and applying its own settings
};

struct DarkRunHooks {
    std::function<DarkCameraState()> capture;                    // what the camera is set to now
    std::function<bool(int w, int h, int bits)> applyFormat;     // false: the camera refused
    std::function<void(double ms)> setExposure;
    std::function<void(double gain)> setGain;
    std::function<void(bool on)> setAeAuto;
    std::function<bool(const std::string& prefix, const std::string& folder, int frames)> startDarkRecording;
    std::function<bool()> recording;
    std::function<void(const DarkCameraState&)> restore;
    std::function<void(const std::string&)> log;                 // "[DARKS] ..."
};

class DarkRunController {
public:
    enum class Phase { Idle, Armed, Settling, Recording, Done, Stopped, Failed };

    explicit DarkRunController(DarkRunHooks h) : m_h(std::move(h)) {}

    // First press: keep the plan, say what it will do. Returns the text.
    std::string Arm(const raduk::core::DarkPlanResult& plan, int frames, double fpsCap, double now) {
        m_plan = plan; m_frames = frames; m_fpsCap = fpsCap;
        m_armedAt = now;
        if (plan.sets.empty()) {
            m_phase = Phase::Idle;
            return plan.lightsRead == 0
                ? "RUN DARKS: no lights recorded tonight in the capture folders."
                : "RUN DARKS: every set tonight already has its darks.";
        }
        double secs = 0.0;
        for (const auto& s : plan.sets) secs += raduk::core::DarkSetSeconds(s.key, frames, fpsCap) + kSettleSec;
        m_phase = Phase::Armed;
        char b[200];
        snprintf(b, sizeof(b), "RUN DARKS: %d set%s, %d frames each, about %.0f s. Cap the scope, then run it again within %.0f s.",
                 (int)plan.sets.size(), plan.sets.size() == 1 ? "" : "s", frames, secs, kArmSec);
        Log(b);
        for (const auto& s : plan.sets) Log("  " + raduk::core::DescribeDarkSet(s));
        return b;
    }

    // Second press within kArmSec: run. Returns false if not armed.
    bool Confirm(double now) {
        if (m_phase != Phase::Armed) return false;
        if (now - m_armedAt > kArmSec) { m_phase = Phase::Idle; Log("the plan was not confirmed in time; press again"); return false; }
        m_saved = m_h.capture ? m_h.capture() : DarkCameraState{};
        if (m_h.setAeAuto) m_h.setAeAuto(false);
        m_index = 0;
        m_done = 0;
        return StartSet(now);
    }

    void Stop(const char* why) {
        if (m_phase == Phase::Idle || m_phase == Phase::Done || m_phase == Phase::Stopped ||
            m_phase == Phase::Failed) return;
        const bool wasRunning = m_phase != Phase::Armed;
        m_phase = Phase::Stopped;
        m_reason = why;
        Log(std::string("stopped: ") + why);
        if (wasRunning) Restore();
    }

    void Tick(double now) {
        switch (m_phase) {
        case Phase::Armed:
            if (now - m_armedAt > kArmSec) { m_phase = Phase::Idle; }
            break;
        case Phase::Settling:
            if (now - m_phaseAt >= kSettleSec) {
                const auto& s = m_plan.sets[m_index];
                const std::string prefix = raduk::core::DarkPrefix(s.key);
                if (!m_h.startDarkRecording ||
                    !m_h.startDarkRecording(prefix, DarksFolder(s.folder), m_frames)) {
                    Fail("the recording for " + prefix + " would not start");
                    return;
                }
                Log("recording " + prefix + " (" + std::to_string(m_frames) + " frames) into " + DarksFolder(s.folder));
                m_phase = Phase::Recording;
                m_phaseAt = now;
            }
            break;
        case Phase::Recording:
            if (m_h.recording && !m_h.recording()) {
                ++m_done;
                ++m_index;
                if (m_index >= m_plan.sets.size()) {
                    m_phase = Phase::Done;
                    Log("done: " + std::to_string(m_done) + " dark set(s) recorded");
                    Restore();
                } else {
                    StartSet(now);
                }
            } else if (now - m_phaseAt > RecordLimitSec()) {
                Fail("a dark recording ran far past its expected length");
            }
            break;
        default: break;
        }
    }

    Phase State() const { return m_phase; }
    bool Running() const { return m_phase == Phase::Settling || m_phase == Phase::Recording; }
    bool Armed() const { return m_phase == Phase::Armed; }
    int SetsDone() const { return m_done; }
    const std::string& Reason() const { return m_reason; }

    static std::string DarksFolder(const std::string& lightsFolder) {
        if (lightsFolder.empty()) return "Darks";
        const char last = lightsFolder.back();
        return lightsFolder + ((last == '\\' || last == '/') ? "" : "\\") + "Darks";
    }

    static constexpr double kSettleSec = 3.0;   // frames already in flight use the old settings
    static constexpr double kArmSec = 60.0;     // the confirm must follow the plan

private:
    bool StartSet(double now) {
        const auto& k = m_plan.sets[m_index].key;
        if (m_h.applyFormat && !m_h.applyFormat(k.width, k.height, k.bitDepth)) {
            Fail("the camera would not take " + std::to_string(k.width) + "x" + std::to_string(k.height) +
                 " " + std::to_string(k.bitDepth) + "-bit");
            return false;
        }
        if (m_h.setExposure) m_h.setExposure(k.exposureMs);
        if (m_h.setGain) m_h.setGain(k.gain);
        Log("set " + std::to_string(m_index + 1) + " of " + std::to_string(m_plan.sets.size()) + ": " +
            raduk::core::DescribeDarkSet(m_plan.sets[m_index]));
        m_phase = Phase::Settling;
        m_phaseAt = now;
        return true;
    }

    double RecordLimitSec() const {
        const auto& k = m_plan.sets[m_index].key;
        return 3.0 * raduk::core::DarkSetSeconds(k, m_frames, m_fpsCap) + 60.0;
    }

    void Fail(const std::string& why) {
        m_phase = Phase::Failed;
        m_reason = why;
        Log("failed: " + why);
        Restore();
    }

    void Restore() {
        if (m_h.restore) m_h.restore(m_saved);
        Log("camera settings put back");
    }

    void Log(const std::string& s) { if (m_h.log) m_h.log("[DARKS] " + s); }

    DarkRunHooks m_h;
    raduk::core::DarkPlanResult m_plan;
    int m_frames = 200;
    double m_fpsCap = 100.0;
    double m_armedAt = 0.0, m_phaseAt = 0.0;
    size_t m_index = 0;
    int m_done = 0;
    DarkCameraState m_saved;
    Phase m_phase = Phase::Idle;
    std::string m_reason;
};

} // namespace raduk::app
