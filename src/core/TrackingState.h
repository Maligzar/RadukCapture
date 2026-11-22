#pragma once
// ============================================================
// TrackingState.h -- who is moving the mount to follow the sky
// ============================================================
//
// 2026-11-22 at the scope: the mount window said "TRK: OFF" while Saturn
// held to 1.5" for an hour. It was true and misleading. PLANET LOCK's
// guider turns the hand control's tracking off (`T 0`) and drives both
// axes itself at the measured rates (CentroidTracker::_RunCalibration),
// so the hand control rightly reports OFF while the mount follows the
// sky better than it would alone.
//
// The answer to "is the mount tracking" is therefore not the hand
// control's mode alone. This header combines the two, and says when the
// mode could not be read (Phase 69: UNKNOWN is not OFF).
//
// Display and gating only. Nothing here changes what the guider sends.
// ============================================================

namespace raduk::core {

enum class Steering {
    NotConnected,   // no mount
    Unknown,        // the hand control's mode could not be read
    Off,            // nobody is following the sky
    HandControl,    // the hand control's own tracking
    AppLock,        // PLANET LOCK drives the axes; the hand control is OFF on purpose
};

// modeOff: the hand control reports tracking OFF.
// appDriving: CentroidTracker has taken the axes (IsAppDriving()).
inline Steering WhoSteers(bool connected, bool modeKnown, bool modeOff, bool appDriving) {
    if (!connected) return Steering::NotConnected;
    // The app's own drive is known without asking the hand control, and
    // it is the one that matters while it runs.
    if (appDriving) return Steering::AppLock;
    if (!modeKnown) return Steering::Unknown;
    return modeOff ? Steering::Off : Steering::HandControl;
}

// The sky is being followed, by either.
inline bool IsFollowingSky(Steering s) {
    return s == Steering::HandControl || s == Steering::AppLock;
}

// For gates that refuse "the mount is not tracking": refuse only a
// mount known to be standing still. Unknown is not refused here; the
// caller that needs certainty checks Unknown itself.
inline bool KnownStill(Steering s) { return s == Steering::Off; }

inline const char* SteeringName(Steering s) {
    switch (s) {
        case Steering::NotConnected: return "not connected";
        case Steering::Unknown:      return "unknown";
        case Steering::Off:          return "off";
        case Steering::HandControl:  return "hand control";
        case Steering::AppLock:      return "app (lock)";
    }
    return "unknown";
}

// The mount window's line. modeName is the hand control's mode as
// shown before ("ALT/AZ", "EQ N", "EQ S"); used for HandControl only.
inline const wchar_t* TrkLabel(Steering s, const wchar_t* modeName) {
    switch (s) {
        case Steering::NotConnected: return L"TRK: OFF";
        case Steering::Unknown:      return L"TRK: ?";          // no reply from the hand control
        case Steering::Off:          return L"TRK: OFF";
        case Steering::AppLock:      return L"TRK: APP (lock)";
        case Steering::HandControl:  return modeName;
    }
    return L"TRK: ?";
}

} // namespace raduk::core
