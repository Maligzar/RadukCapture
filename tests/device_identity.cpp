// ============================================================
//  Phase 157.3: what a device says it is (src/core/DeviceIdentity.h)
//    1. All four fields always appear; an empty one reads "not reported".
//    2. An SDK's 8-byte serial as hex; all zero is no serial.
//    3. ZWO's "1, 41, 0" reads 1.41.0; three bytes read 3.0.4.
// ============================================================

#include "core/DeviceIdentity.h"

#include <cstdio>
#include <string>

using namespace raduk::core;

static int g_fail = 0;
static void Check(bool ok, const std::string& what, const std::string& detail = "") {
    printf("  [%s] %-62s %s\n", ok ? "PASS" : "FAIL", what.c_str(), detail.c_str());
    if (!ok) g_fail++;
}

int main() {
    printf("Device identity (phase 157.3)\n");

    DeviceIdentity full{"ZWO EAF", "3.0.4", "1.7.7", "0a1b2c3d4e5f6071"};
    const std::string f = DescribeIdentity(full);
    Check(f == "ZWO EAF; firmware 3.0.4; SDK 1.7.7; serial 0a1b2c3d4e5f6071", "every field written", f);

    DeviceIdentity cam{"ZWO ASI678MC", "", "1.41.0", ""};
    const std::string c = DescribeIdentity(cam);
    Check(c == "ZWO ASI678MC; firmware not reported; SDK 1.41.0; serial not reported",
          "an empty field says not reported, never left out", c);
    Check(DescribeIdentity({}) ==
          "not reported; firmware not reported; SDK not reported; serial not reported",
          "nothing known still gives all four");

    const unsigned char sn[8] = {0x0a, 0x1b, 0x2c, 0x3d, 0x4e, 0x5f, 0x60, 0x71};
    Check(SerialHex(sn, 8) == "0a1b2c3d4e5f6071", "serial as lower-case hex", SerialHex(sn, 8));
    const unsigned char zero[8] = {};
    Check(SerialHex(zero, 8).empty(), "an all-zero serial is no serial");
    Check(SerialHex(nullptr, 8).empty() && SerialHex(sn, 0).empty(), "no buffer, no serial");

    Check(TidyVersion("1, 41, 0") == "1.41.0", "ZWO's \"1, 41, 0\" reads 1.41.0", TidyVersion("1, 41, 0"));
    Check(TidyVersion("1.7.7") == "1.7.7", "an already tidy version is kept");
    Check(TidyVersion(nullptr).empty(), "no version string, nothing");
    Check(VersionString(3, 0, 4) == "3.0.4", "three bytes read 3.0.4");

    printf(g_fail ? "FAILED: %d\n" : "All passed\n", g_fail);
    return g_fail ? 1 : 0;
}
