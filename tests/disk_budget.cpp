// disk_budget: will this run's frames reach the disk? 2026-11-22, 03:04:
// 1280x960 16-bit at 114 fps (~280 MB/s) to S:, which could not keep up.
#include "core/DiskBudget.h"
#include <cstdio>
#include <cmath>

using namespace raduk::core;
static int fails = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); ++fails; } } while (0)

int main() {
    // 1. The meter: busy time, and nothing until enough is written.
    {
        WriteSpeedMeter m;
        m.Add(1.0e9, 5.0);
        CHECK(m.MBs() == 0.0);                    // 1 GB: could be the OS cache
        m.Add(2.0e9, 15.0);
        CHECK(std::fabs(m.MBs() - 150.0) < 0.01); // 3 GB in 20 s busy
        m.Reset();
        CHECK(m.MBs() == 0.0);
    }
    // 2. The night: 2,457,600 bytes a frame at 114 fps against a 95 MB/s
    //    disk with a 9.6 GB buffer: warned, and the buffer lasts ~53 s.
    {
        auto v = JudgeDiskRate(2457600.0, 114.0, 95.0, 4096.0 * 2457600.0, "S:");
        std::printf("  night: need %.0f MB/s, have %.0f, fills in %.0f s\n", v.needMBs, v.haveMBs, v.fillSec);
        CHECK(v.warn);
        CHECK(std::fabs(v.needMBs - 280.2) < 1.0);
        CHECK(v.fillSec > 40.0 && v.fillSec < 70.0);
        CHECK(v.text.find("S:") != std::string::npos);
    }
    // 3. Run 2 at 640x480 16-bit, 105 fps (~64.5 MB/s) to a disk that did it: no warning.
    {
        auto v = JudgeDiskRate(614400.0, 105.0, 120.0, 4096.0 * 614400.0, "D:");
        CHECK(!v.warn);
        CHECK(v.text.empty());
    }
    // 4. An unknown disk says nothing (it is measured on this run).
    CHECK(!JudgeDiskRate(2457600.0, 114.0, 0.0, 1e10, "S:").warn);
    // 5. A 5% margin: a run needing up to 95% of the disk's figure is not
    //    warned; one needing more is (the figure is an average, not a floor).
    CHECK(!JudgeDiskRate(1.0e6, 94.0, 100.0, 1e9, "D:").warn);
    CHECK(JudgeDiskRate(1.0e6, 96.0, 100.0, 1e9, "D:").warn);
    // 6. Blending a stored figure.
    CHECK(BlendDiskSpeed(0.0, 120.0) == 120.0);
    CHECK(BlendDiskSpeed(100.0, 0.0) == 100.0);
    CHECK(BlendDiskSpeed(100.0, 140.0) == 120.0);
    // 7. Volumes.
    CHECK(VolumeOf("s:\\captures\\Jupiter.ser") == "S:");
    CHECK(VolumeOf("D:/x") == "D:");
    CHECK(VolumeOf("\\\\nas\\share\\x").empty());
    CHECK(VolumeOf("").empty());

    std::printf("disk_budget: %s (%d failures)\n", fails ? "FAIL" : "PASS", fails);
    return fails ? 1 : 0;
}
