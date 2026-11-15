// ============================================================
//  Phase 157.0: the emulator's fault list (src/core/EmuFault.h)
//    1. name@start and name@start+duration parse; junk is skipped.
//    2. In effect from the start, until the end or for ever.
//    3. Found by name; read from RADUK_EMU_FAULT.
// ============================================================

#include "core/EmuFault.h"

#include <cstdio>
#include <cstdlib>
#include <string>

using namespace raduk::core;

static int g_fail = 0;
static void Check(bool ok, const std::string& what, const std::string& detail = "") {
    printf("  [%s] %-62s %s\n", ok ? "PASS" : "FAIL", what.c_str(), detail.c_str());
    if (!ok) g_fail++;
}

int main() {
    printf("Emulator faults (phase 157.0)\n");
    auto f = ParseEmuFaults("camera-drop@20+5, eaf-stall@3,slew-stuck@0");
    Check(f.size() == 3, "three faults parse", std::to_string(f.size()));
    Check(f.size() == 3 && f[0].name == "camera-drop" && f[0].startSec == 20 && f[0].durationSec == 5,
          "name, start and duration");
    Check(f.size() == 3 && f[1].name == "eaf-stall" && f[1].durationSec < 0, "no duration: for ever");
    Check(ParseEmuFaults("nope, @3, a@, a@x, a@1+0, a@-2, b@1+x, ,").empty(), "junk is skipped");
    Check(ParseEmuFaults("").empty(), "an empty list is no faults");

    EmuFault d{"camera-drop", 20, 5};
    Check(!d.InEffect(19.9) && d.InEffect(20) && d.InEffect(24.9) && !d.InEffect(25), "in effect for its duration");
    EmuFault s{"eaf-stall", 3, -1};
    Check(!s.InEffect(2.9) && s.InEffect(3) && s.InEffect(1e6), "and for ever without one");

    EmuFault got;
    Check(FindEmuFault(f, "eaf-stall", got) && got.startSec == 3 && !FindEmuFault(f, "disk-full", got),
          "found by name, and only by name");
#ifdef _WIN32
    _putenv("RADUK_EMU_FAULT=slew-stuck@7");
#else
    setenv("RADUK_EMU_FAULT", "slew-stuck@7", 1);
#endif
    Check(ActiveEmuFault("slew-stuck", got) && got.startSec == 7 && !ActiveEmuFault("eaf-stall", got),
          "read from RADUK_EMU_FAULT");
#ifdef _WIN32
    _putenv("RADUK_EMU_FAULT=");
#else
    unsetenv("RADUK_EMU_FAULT");
#endif
    Check(!ActiveEmuFault("slew-stuck", got), "and nothing when it is unset");

    if (g_fail) { printf("emu_fault: %d FAILED\n", g_fail); return 1; }
    printf("emu_fault: all passed\n");
    return 0;
}
