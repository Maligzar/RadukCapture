#pragma once
// ============================================================
// DarkPlan.h -- which darks tonight's lights need (Phase 173)
// ============================================================
//
// The owner, 2026-11-22: "at the end of the night I click run darks and it
// runs my darks based on what is in the capture folders for the night."
//
// A dark matches a light when the sensor read it the same way: the same
// frame (width, height, bin), bit depth, exposure and gain. Those come
// from each run's observation record. Lights are grouped by that key; a
// key already covered by a dark taken tonight is left out. Pure: the
// caller reads the records and runs the sets (app/DarkRunController.h).
//
// Not matched, by design for now: sensor temperature (the record does not
// carry it; for planetary exposures of milliseconds the dark current it
// changes is small next to the read pattern the dark is for). Logged when
// the darks run, so a large difference is visible.
// ============================================================

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "ObservationRecord.h"

namespace raduk::core {

struct DarkKey {
    int    width = 0, height = 0, bitDepth = 0, bin = 1;
    double exposureMs = 0.0;
    double gain = 0.0;

    // Exposure to within 0.5% (the camera rounds to its own step), gain exactly.
    bool Matches(const DarkKey& o) const {
        return width == o.width && height == o.height && bitDepth == o.bitDepth &&
               bin == o.bin && std::fabs(gain - o.gain) < 0.5 &&
               std::fabs(exposureMs - o.exposureMs) <= 0.005 * (std::max)(exposureMs, o.exposureMs) + 1e-6;
    }
};

struct DarkSet {
    DarkKey key;
    int lights = 0;                     // runs that need it
    std::vector<std::string> targets;   // what they were of
    std::string folder;                 // where the first of its lights was written
};

// Nothing about a record is guessed: one without a frame, bit depth,
// exposure or gain is not planned for, and the caller is told how many.
inline bool DarkKeyOf(const ObservationRecord& r, DarkKey& k) {
    if (r.width <= 0 || r.height <= 0 || r.bitDepth <= 0) return false;
    if (!ObsKnown(r.exposureMs) || !ObsKnown(r.gain) || r.exposureMs <= 0.0) return false;
    k.width = r.width; k.height = r.height; k.bitDepth = r.bitDepth;
    k.bin = r.bin > 0 ? r.bin : 1;
    k.exposureMs = r.exposureMs; k.gain = r.gain;
    return true;
}

// One record and the folder its SER was written to.
struct FoundRecord {
    ObservationRecord rec;
    std::string folder;
};

struct DarkPlanResult {
    std::vector<DarkSet> sets;   // in the order their first light was taken
    int lightsRead = 0;
    int lightsUnusable = 0;      // a record without the settings a dark needs
    int setsAlreadyDark = 0;     // covered by a dark taken tonight
};

// recs: every record found; sinceUnix: the night's start (records before
// it are another night's).
inline DarkPlanResult PlanDarks(std::vector<FoundRecord> recs, int64_t sinceUnix) {
    DarkPlanResult out;
    std::stable_sort(recs.begin(), recs.end(), [](const FoundRecord& a, const FoundRecord& b) {
        return a.rec.startUnix < b.rec.startUnix;
    });
    std::vector<DarkKey> darks;
    for (const auto& f : recs) {
        if (f.rec.startUnix < sinceUnix) continue;
        if (f.rec.frameType == "Dark") {
            DarkKey k;
            if (DarkKeyOf(f.rec, k)) darks.push_back(k);
        }
    }
    for (const auto& f : recs) {
        if (f.rec.startUnix < sinceUnix) continue;
        if (!(f.rec.frameType.empty() || f.rec.frameType == "Light")) continue;
        ++out.lightsRead;
        DarkKey k;
        if (!DarkKeyOf(f.rec, k)) { ++out.lightsUnusable; continue; }
        auto it = std::find_if(out.sets.begin(), out.sets.end(),
                               [&](const DarkSet& s) { return s.key.Matches(k); });
        if (it == out.sets.end()) {
            out.sets.push_back({k, 0, {}, f.folder});
            it = out.sets.end() - 1;
        }
        ++it->lights;
        if (!f.rec.target.empty() &&
            std::find(it->targets.begin(), it->targets.end(), f.rec.target) == it->targets.end())
            it->targets.push_back(f.rec.target);
    }
    const size_t before = out.sets.size();
    out.sets.erase(std::remove_if(out.sets.begin(), out.sets.end(), [&](const DarkSet& s) {
        return std::any_of(darks.begin(), darks.end(), [&](const DarkKey& d) { return d.Matches(s.key); });
    }), out.sets.end());
    out.setsAlreadyDark = (int)(before - out.sets.size());
    return out;
}

// "Dark_9.44ms_g324_640x480_16bit": the file says which lights it is for.
inline std::string DarkPrefix(const DarkKey& k) {
    char b[96];
    std::snprintf(b, sizeof(b), "Dark_%.4gms_g%.0f_%dx%d_%dbit%s", k.exposureMs, k.gain,
                  k.width, k.height, k.bitDepth, k.bin > 1 ? (k.bin == 2 ? "_bin2" : "_binN") : "");
    return b;
}

// Seconds to record `frames` darks: one exposure each, at least the
// transfer time the camera needs (fpsCap frames a second at most).
inline double DarkSetSeconds(const DarkKey& k, int frames, double fpsCap) {
    const double perFrame = (std::max)(k.exposureMs / 1000.0, fpsCap > 0.0 ? 1.0 / fpsCap : 0.0);
    return frames * perFrame;
}

inline std::string DescribeDarkSet(const DarkSet& s) {
    std::string t;
    for (size_t i = 0; i < s.targets.size(); ++i) t += (i ? ", " : "") + s.targets[i];
    char b[200];
    std::snprintf(b, sizeof(b), "%dx%d %d-bit, %.4g ms, gain %.0f (%d run%s%s%s)",
                  s.key.width, s.key.height, s.key.bitDepth, s.key.exposureMs, s.key.gain,
                  s.lights, s.lights == 1 ? "" : "s", t.empty() ? "" : ": ", t.c_str());
    return b;
}

} // namespace raduk::core
