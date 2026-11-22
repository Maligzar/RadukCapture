#pragma once
// ============================================================
// DiskBudget.h -- will this run's frames reach the disk?
// ============================================================
//
// 2026-11-22, 03:04 at the scope: a 1280x960 16-bit run at 114 fps is
// about 280 MB/s. It went to S:, which could not keep up; the 9.6 GB
// write buffer filled, frames were dropped, and the end of the run hung
// the app while it drained. Nothing said beforehand that the run was
// beyond that disk.
//
// Two parts, both pure:
//   - WriteSpeedMeter: what a disk has shown it can write, from the time
//     the writer spends inside its writes (busy time, not wall time, so a
//     run slower than the disk still measures the disk).
//   - JudgeDiskRate: at the start of a run, the run's rate against the
//     disk's, and how long the buffer lasts if the disk is slower.
// ============================================================

#include <cstdio>
#include <string>

namespace raduk::core {

class WriteSpeedMeter {
public:
    void Reset() { m_bytes = 0.0; m_busySec = 0.0; }
    void Add(double bytes, double busySec) {
        if (bytes > 0.0 && busySec >= 0.0) { m_bytes += bytes; m_busySec += busySec; }
    }
    double Bytes() const { return m_bytes; }
    // MB/s (1 MB = 1e6 bytes), or 0 until enough has been written to mean
    // anything: the first gigabytes can land in the OS cache at memory speed.
    double MBs(double minBytes = 2.0e9) const {
        if (m_bytes < minBytes || m_busySec <= 0.0) return 0.0;
        return m_bytes / m_busySec / 1.0e6;
    }
private:
    double m_bytes = 0.0, m_busySec = 0.0;
};

// A stored figure moves halfway to each new measurement, so one odd run
// does not swing it and a changed disk is learned in a few runs.
inline double BlendDiskSpeed(double storedMBs, double measuredMBs) {
    if (measuredMBs <= 0.0) return storedMBs;
    if (storedMBs <= 0.0) return measuredMBs;
    return 0.5 * storedMBs + 0.5 * measuredMBs;
}

struct DiskVerdict {
    double needMBs = 0.0;    // frame bytes x fps
    double haveMBs = 0.0;    // what this disk has shown, 0 when unknown
    double fillSec = -1.0;   // seconds until the buffer is full, -1 when it never fills
    bool   warn = false;
    std::string text;        // empty unless warn
};

// frameBytes and fps describe the run; bufferBytes is the write buffer.
// Warn when the disk is known and the run needs more than 95% of what it has
// shown: the figure is an average, so the margin is on the safe side.
inline DiskVerdict JudgeDiskRate(double frameBytes, double fps, double haveMBs,
                                 double bufferBytes, const std::string& volume) {
    DiskVerdict v;
    v.needMBs = frameBytes * fps / 1.0e6;
    v.haveMBs = haveMBs;
    if (haveMBs <= 0.0 || v.needMBs <= haveMBs * 0.95) return v;
    const double deficitMBs = v.needMBs - haveMBs;
    v.fillSec = deficitMBs > 0.0 ? bufferBytes / 1.0e6 / deficitMBs : -1.0;
    v.warn = true;
    char b[320];
    std::snprintf(b, sizeof(b),
                  "This run writes about %.0f MB/s; %s has managed %.0f MB/s. The write buffer "
                  "fills in about %.0f s, then frames are dropped and the end of the run waits "
                  "while the rest is written. A smaller frame (640), 8-bit, or a faster disk fits.",
                  v.needMBs, volume.empty() ? "this disk" : volume.c_str(), haveMBs,
                  v.fillSec > 0.0 ? v.fillSec : 0.0);
    v.text = b;
    return v;
}

// "S:" from "s:\\captures\\..." or "S:/x"; empty for anything else (a UNC
// path keeps no figure: its speed is the network's).
inline std::string VolumeOf(const std::string& path) {
    if (path.size() >= 2 && path[1] == ':' &&
        ((path[0] >= 'A' && path[0] <= 'Z') || (path[0] >= 'a' && path[0] <= 'z'))) {
        std::string v(1, (char)(path[0] & ~0x20));
        return v + ":";
    }
    return "";
}

} // namespace raduk::core
