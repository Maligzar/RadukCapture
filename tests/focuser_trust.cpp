// ============================================================
//  Phase 157.0e: the focuser's last position, kept and checked
//  (src/core/FocuserTrust.h). Pure; no focuser, no clock.
// ============================================================

#include "core/FocuserTrust.h"

#include <cstdio>
#include <filesystem>
#include <string>

using namespace raduk::core;
namespace fs = std::filesystem;

static int g_fail = 0;
static void Check(bool ok, const std::string& what, const std::string& detail = "") {
    printf("  [%s] %-62s %s\n", ok ? "PASS" : "FAIL", what.c_str(), detail.c_str());
    if (!ok) g_fail++;
}

int main() {
    printf("Focuser trust (phase 157.0e)\n");

    printf("The record\n");
    {
        FocuserMemory m; m.valid = true; m.serial = "a1b2"; m.position = 13785; m.maxStep = 600000;
        FocuserMemory back;
        Check(ParseFocuserMemory(FormatFocuserMemory(m), back) && back.valid && back.serial == "a1b2" &&
              back.position == 13785 && back.maxStep == 600000, "it reads back what it wrote");
        Check(ParseFocuserMemory("Position=5\r\n", back) && back.position == 5 && back.serial.empty() &&
              back.maxStep == 0, "Position alone is enough; CRLF is fine");
        Check(!ParseFocuserMemory("Serial=x\nMaxStep=10\n", back), "no Position: no record");
        Check(!ParseFocuserMemory("Position=12a\n", back), "a malformed number: no record");
        Check(!ParseFocuserMemory("Position=-3\n", back), "a negative one either");
        Check(!ParseFocuserMemory("", back), "an empty file is no record");
    }

    printf("Judging an open\n");
    {
        FocuserMemory s; s.valid = true; s.serial = "a1b2"; s.position = 13785; s.maxStep = 600000;
        Check(JudgeOpen(FocuserMemory{}, "a1b2", 1000, 600000) == TrustVerdict::NoRecord, "nothing saved: no record, trusted");
        Check(JudgeOpen(s, "a1b2", 13785, 600000) == TrustVerdict::Trusted, "the same number: trusted");
        Check(JudgeOpen(s, "a1b2", 13785 + kTrustToleranceSteps, 600000) == TrustVerdict::Trusted &&
              JudgeOpen(s, "a1b2", 13785 - kTrustToleranceSteps, 600000) == TrustVerdict::Trusted,
              "within the tolerance either way: trusted");
        Check(JudgeOpen(s, "a1b2", 13785 + kTrustToleranceSteps + 1, 600000) == TrustVerdict::Moved,
              "one step past it: not");
        Check(JudgeOpen(s, "a1b2", 1000, 600000) == TrustVerdict::Moved, "1000 against 13785 (the forum's case): not");
        Check(JudgeOpen(s, "a1b2", 0, 600000) == TrustVerdict::Moved, "0: not");
        Check(JudgeOpen(s, "ffff", 1000, 600000) == TrustVerdict::OtherFocuser, "another focuser: its own record");
        Check(JudgeOpen(s, "", 13785, 600000) == TrustVerdict::Trusted, "no serial reported: judged by the number");
        Check(JudgeOpen(s, "a1b2", 13785, 300000) == TrustVerdict::RangeChanged, "its range changed: not");
        Check(Trusting(TrustVerdict::Trusted) && Trusting(TrustVerdict::NoRecord) &&
              Trusting(TrustVerdict::OtherFocuser) && !Trusting(TrustVerdict::Moved) &&
              !Trusting(TrustVerdict::RangeChanged), "which verdicts are trusting");
    }

    printf("Held, through the owner's answer\n");
    {
        const fs::path dir = fs::temp_directory_path() / "raduk_focuser_trust";
        std::error_code ec; fs::remove_all(dir, ec); fs::create_directories(dir, ec);
        const std::string path = (dir / "focuser_position.ini").string();

        PositionTrust none;
        Check(none.OnOpen("", 1000, 600000) == TrustVerdict::Trusted && none.Trusted(),
              "no path: always trusted, nothing written");

        PositionTrust t; t.SetPath(path);
        Check(t.OnOpen("a1b2", 13785, 600000) == TrustVerdict::NoRecord && t.Trusted() && fs::exists(path),
              "the first open starts the record");
        t.OnArrived(14000);
        FocuserMemory m;
        const bool loaded = LoadFocuserMemory(path, m);
        Check(loaded && m.position == 14000 && m.serial == "a1b2",
              "a completed move is recorded", std::to_string(m.position));

        PositionTrust t2; t2.SetPath(path);
        Check(t2.OnOpen("a1b2", 14001, 600000) == TrustVerdict::Trusted && t2.Trusted(),
              "the next open, at the same number: trusted");

        PositionTrust t3; t3.SetPath(path);
        Check(t3.OnOpen("a1b2", 4761, 600000) == TrustVerdict::Moved && !t3.Trusted(),
              "the next, at 4761: untrusted");
        const std::string p = t3.Problem();
        Check(p.find("4761") != std::string::npos && p.find("14001") != std::string::npos,
              "the problem names both numbers", p);
        Check(t3.SavedPosition() == 14001 && t3.ReportedPosition() == 4761, "and holds them");
        t3.OnArrived(5000);
        Check(LoadFocuserMemory(path, m) && m.position == 14001,
              "a jog while untrusted does not overwrite the saved number", std::to_string(m.position));
        t3.Accept(5000);
        Check(t3.Trusted() && t3.Problem().empty() && LoadFocuserMemory(path, m) && m.position == 5000,
              "accepted: trusted, and the record follows the focuser");

        PositionTrust t4; t4.SetPath(path);
        t4.OnOpen("a1b2", 900, 600000);
        Check(!t4.Trusted(), "another mismatch");
        t4.Restored(5000);
        Check(t4.Trusted() && LoadFocuserMemory(path, m) && m.position == 5000,
              "the saved number put back: trusted, recorded");

        Check(!fs::exists(path + ".tmp"), "no half-written file left beside it");
        fs::remove_all(dir, ec);
    }

    printf("When to try a reconnect\n");
    {
        FocuserRecovery r;
        Check(!r.OnRead(true, 0.0) && !r.Failing(), "good reads: nothing to do");
        Check(!r.OnRead(false, 10.0) && r.Failing(), "the first failure: not yet");
        Check(!r.OnRead(false, 11.5), "under 2 s of failing: not yet");
        Check(r.OnRead(false, 12.0), "2 s of failing: try");
        Check(!r.OnRead(false, 13.0) && !r.OnRead(false, 14.9), "then not again inside 3 s");
        Check(r.OnRead(false, 15.0), "and again at 3 s");
        Check(!r.OnRead(true, 15.5) && !r.Failing(), "a good read ends it");
        Check(!r.OnRead(false, 16.0) && !r.OnRead(false, 17.0), "a fresh failure starts the 2 s again");
    }

    if (g_fail) { printf("focuser_trust: %d FAILED\n", g_fail); return 1; }
    printf("focuser_trust: all passed\n");
    return 0;
}
