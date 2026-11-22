// focus_replay: Phase 174.2. Recorded sweeps through today's rules, scored
// against where the owner settled. The two sweeps that shaped the fade
// guard are held here as history lines:
//   - 2026-11-22 23:10, Saturn through haze: autofocus kept 165,984 (the old
//     rule), the owner settled on 168,893 by hand;
//   - autofocus_saturn case 3 (the target fades through the sweep, focus
//     169,000), whose fit 168,952 is right.
#include "core/FocusReplay.h"
#include <cstdio>

using namespace raduk::core::replay;
static int fails = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); ++fails; } } while (0)

static const char* kHistory =
R"JSONL({"kind":"sweep","format":1,"id":"night-2310","unix":1795300223,"app":"3.3.49","at":{"target":"Saturn","filter":"UV/IR cut (400-700nm)","profile":"","exposure_ms":25,"gain":230,"width":640,"height":480,"bit_depth":16,"focuser_temp_c":14,"sensor_temp_c":null,"alt_deg":null,"seeing":9.7,"sharpness_pct":14.3},"success":true,"fine_only":false,"start":168665,"result":165984,"start_sharp":14.6,"result_sharp":4.6,"improvement":0,"message":"conditions changed during the sweep (target faded) - focus kept from the out-and-back fit","backlash_measured":-4665,"backlash_in_use":200,"coarse":{"pos":[165665,166265,166865,167465,168065,168665,169265,169865,170465,171065,171665],"out":[14.6,14.8,14.7,14.6,14.6,14.3,14.0,8.3,6.3,5.1,4.8],"back":[4.6,4.6,4.5,4.7,4.6,4.6,4.6,4.6,4.6,4.8,4.5],"peak_out":[200,207,209,201,201,202,194,108,83,70,64],"peak_back":[49,53,55,56,54,59,54,51,60,63,61],"peak":166286,"out_back_ratio":3.11},"fine":{"used":true,"peak":165984,"passes":1,"pos":[165786,165986,166186,166386,166586,166786,166986],"score":[0.2119,0.2209,0.2198,0.2086,0.223,0.2238,0.2027],"spread":[],"fade_ratio":1.13,"fade_action":"corrected"},"ab":{"ran":true,"start_before":14.6,"start_now":4.6,"fit":4.6,"edge_start":0.1881,"edge_fit":0.1917,"verdict":"kept fit"},"sky_changed":true}
{"kind":"followup","format":1,"sweep":"night-2310","sweep_unix":1795300223,"sweep_result":165984,"rests":[{"unix":1795300500,"pos":168893,"sharpness_pct":99}],"settled":168893,"moved_from_result":2909,"ended_by":"window"}
{"kind":"sweep","format":1,"id":"ci-case3","unix":1795320000,"app":"3.3.55","at":{"target":"Saturn"},"success":true,"fine_only":false,"start":167500,"result":168952,"coarse":{"pos":[164500,165100,165700,166300,166900,167500,168100,168700,169300,169900,170500],"out":[19.3,20.1,22.6,26.5,30.4,34.5,41.3,43.3,38.2,27.0,17.3],"back":[1.0,1.4,2.1,3.2,5.0,7.8,12.6,17.2,20.0,19.0,17.1],"peak_out":[],"peak_back":[],"peak":168848,"out_back_ratio":4.45},"fine":{"used":true},"ab":{"ran":true,"start_before":141.1,"start_now":0.5,"fit":1.1,"edge_start":0,"edge_fit":0,"verdict":"kept fit"},"sky_changed":true}
{"kind":"followup","format":1,"sweep":"ci-case3","sweep_unix":1795320000,"sweep_result":168952,"rests":[],"settled":169000,"moved_from_result":48,"ended_by":"window"}
this line is not JSONL
)JSONL";

int main() {
    const ReplayReport rep = ReplayHistory(kHistory);
    CHECK(rep.linesRead == 5 && rep.linesBad == 1);
    CHECK(rep.sweeps.size() == 2);
    for (const auto& r : rep.sweeps)
        std::printf("  %-11s start %d  recorded %d (%s)  replayed %d (%s)  settled %d  error %d -> %d  coarse fit %d\n",
                    r.id.c_str(), r.start, r.recorded, r.recordedVerdict.c_str(), r.replayed,
                    r.replayedVerdict.c_str(), r.settled, r.errRecorded, r.errReplayed, r.coarseFit);

    // The night: today's rules keep the start, 228 steps from where the owner
    // settled, where the old rule kept a fit 2,909 steps away.
    const Replayed& n = rep.sweeps[0];
    CHECK(n.replayedVerdict == "start stands (fade)");
    CHECK(n.replayed == 168665);
    CHECK(n.settledKnown && n.errRecorded == 2909 && n.errReplayed == 228);

    // Case 3: the fit stays (it peaked both ways), right where focus is.
    const Replayed& c = rep.sweeps[1];
    CHECK(c.replayedVerdict == "kept fit");
    CHECK(c.replayed == 168952 && c.errReplayed == 48);

    // No recorded sweep ends further from where the owner settled.
    CHECK(Regressions(rep).empty());

    // A rule that wrongly sent case 3 back to its start would show here.
    ReplayReport bad = rep;
    bad.sweeps[1].replayed = bad.sweeps[1].start;
    bad.sweeps[1].errReplayed = std::abs(bad.sweeps[1].start - bad.sweeps[1].settled);
    const auto reg = Regressions(bad);
    CHECK(reg.size() == 1 && reg[0].id == "ci-case3");

    std::printf("focus_replay: %s (%d failures)\n", fails ? "FAIL" : "PASS", fails);
    return fails ? 1 : 0;
}
