#!/usr/bin/env python3
"""af_log_to_history.py - Phase 174.2: autofocus sweeps from a raduk_log into
focus_history.jsonl lines, for nights before the app wrote the history itself
(3.3.54).

    python tools/af_log_to_history.py raduk_log.prev.txt --night 20261121 \
        --settled 23:10=168893 >> logs/focus_history.jsonl

Each sweep in the log becomes one "sweep" line: the start, the coarse passes
(position, score and the planet's peak at each step, out and back), the
same-sky A/B, the verdict and the result. A log has times of day but no date,
so --night names the evening (YYYYMMDD) and the sweep ids are
"log-<night>-<HHMMSS>". --settled HH:MM=pos (repeatable) adds a followup
saying where the owner settled after the sweep that started at that minute,
the answer tools/focus_replay scores against. Conditions the log does not
carry (exposure, gain, temperatures) are written as null, never guessed.
"""
import json
import re
import sys

RE_TIME = re.compile(r"^(\d\d):(\d\d):(\d\d)\.\d+ ")
RE_FROM = re.compile(r"\[AF\] AE settled .*V-curve scan from pos=(\d+)|\[AF\] Start pos=(\d+)")
RE_START = re.compile(r"\[AF\] Autofocus starting")
RE_STEP = re.compile(r"\[AF\] Step \d+/\d+ \((out|back)\)\s+pos=(\d+)\s+sharp=([\d.]+)%.*?peak (-?\d+)(?:-(\d+))?")
RE_REMEAS = re.compile(r"\[AF\] Re-measured start pos=(\d+) now: ([\d.]+) \(was ([\d.]+)\); fit pos=(\d+): ([\d.]+)")
RE_EDGE = re.compile(r"\[AF\] Same sky, edge metric: start pos=\d+ ([\d.]+), fit pos=\d+ ([\d.]+)")
RE_KEEP = re.compile(r"\[AF\] The sky changed, not the focus: keeping pos=(\d+)")
RE_BACK = re.compile(r"returning to start|the start stands|start is sharper")
RE_END = re.compile(r"\[AF\] (SUCCESS|FAILED): (?:pos=(\d+))?(.*)")
RE_FINEPEAK = re.compile(r"\[AF\] Fine pass: (\d+) \(coarse (\d+)")


def parse(lines, night):
    sweeps, cur = [], None
    last_time = "000000"
    for line in lines:
        m = RE_TIME.match(line)
        if m:
            last_time = m.group(1) + m.group(2) + m.group(3)
        if RE_START.search(line) or (cur is None and RE_FROM.search(line)):
            if cur is None or cur.get("steps"):
                cur = {"time": last_time, "steps": [], "start": None, "ab": None, "verdict": "", "result": None,
                       "success": None, "message": "", "fine_peak": -1}
                sweeps.append(cur)
            elif RE_START.search(line) and m:
                cur["time"] = last_time   # the AE line before it carries no time
        if cur is None:
            continue
        m = RE_FROM.search(line)
        if m:
            cur["start"] = int(m.group(1) or m.group(2))
        m = RE_STEP.search(line)
        if m:
            peak = int(m.group(5) or m.group(4))
            cur["steps"].append((m.group(1), int(m.group(2)), float(m.group(3)), peak))
        m = RE_REMEAS.search(line)
        if m:
            cur["start"] = cur["start"] or int(m.group(1))
            cur["ab"] = {"ran": True, "start_now": float(m.group(2)), "start_before": float(m.group(3)),
                         "fit": float(m.group(5)), "edge_start": None, "edge_fit": None, "verdict": ""}
        m = RE_EDGE.search(line)
        if m and cur["ab"]:
            cur["ab"]["edge_start"], cur["ab"]["edge_fit"] = float(m.group(1)), float(m.group(2))
        if RE_KEEP.search(line) and cur["ab"]:
            cur["ab"]["verdict"] = "kept fit"
        elif RE_BACK.search(line) and cur["ab"] and not cur["ab"]["verdict"]:
            cur["ab"]["verdict"] = "start stands"
        m = RE_FINEPEAK.search(line)
        if m:
            cur["fine_peak"] = int(m.group(1))
        m = RE_END.search(line)
        if m and cur["success"] is None:
            cur["success"] = m.group(1) == "SUCCESS"
            cur["result"] = int(m.group(2)) if m.group(2) else cur["start"]
            cur["message"] = m.group(3).strip()
    out = []
    for s in sweeps:
        if s["success"] is None or not s["steps"]:
            continue
        pos = sorted({p for _, p, _, _ in s["steps"]})
        idx = {p: i for i, p in enumerate(pos)}
        out_s, back_s = [0.0] * len(pos), [0.0] * len(pos)
        out_p, back_p = [-1] * len(pos), [-1] * len(pos)
        for d, p, sc, pk in s["steps"]:
            if d == "out":
                out_s[idx[p]], out_p[idx[p]] = sc, pk
            else:
                back_s[idx[p]], back_p[idx[p]] = sc, pk
        ab = s["ab"] or {"ran": False}
        sid = f"log-{night}-{s['time']}"
        out.append({
            "kind": "sweep", "format": 1, "id": sid, "unix": None, "app": "log", "source": "raduk_log",
            "at": {"target": None, "filter": None, "exposure_ms": None, "gain": None, "focuser_temp_c": None},
            "success": s["success"], "fine_only": False, "start": s["start"] or 0, "result": s["result"] or 0,
            "message": s["message"],
            "coarse": {"pos": pos, "out": out_s, "back": back_s, "peak_out": out_p, "peak_back": back_p},
            "fine": {"peak": s["fine_peak"]},
            "ab": {k: ab.get(k) for k in ("ran", "start_before", "start_now", "fit", "edge_start", "edge_fit", "verdict")},
            "time_of_day": s["time"],
        })
    return out


def main(argv):
    if len(argv) >= 2 and argv[1] == "--self-test":
        return self_test()
    if len(argv) < 2:
        print(__doc__)
        return 2
    path, night, settled = argv[1], "unknown", {}
    i = 2
    while i < len(argv):
        if argv[i] == "--night":
            night = argv[i + 1]; i += 2
        elif argv[i] == "--settled":
            hm, pos = argv[i + 1].split("=")
            settled[hm.replace(":", "")] = int(pos); i += 2
        else:
            print(f"unknown argument {argv[i]}", file=sys.stderr); return 2
    with open(path, encoding="utf-8", errors="replace") as f:
        sweeps = parse(f.readlines(), night)
    for s in sweeps:
        print(json.dumps(s))
        hm = s["time_of_day"][:4]
        if hm in settled:
            print(json.dumps({"kind": "followup", "format": 1, "sweep": s["id"], "sweep_unix": None,
                              "sweep_result": s["result"], "rests": [], "settled": settled[hm],
                              "moved_from_result": settled[hm] - s["result"], "ended_by": "given"}))
    print(f"{len(sweeps)} sweep(s) read from {path}", file=sys.stderr)
    return 0


def self_test():
    log = """[AF] AE settled after 15.9s; V-curve scan from pos=168665
23:10:23.705 [AF] Autofocus starting...
[AF] Step 1/4 (out)  pos=165665  sharp=14.6%  (4 samples, 4 distinct, centroid 4/4 ok, peak 193-200)
[AF] Step 2/4 (out)  pos=166265  sharp=14.8%  (4 samples, 4 distinct, centroid 4/4 ok, peak 198-207)
[AF] Step 3/4 (back)  pos=166265  sharp=4.6%  (4 samples, 4 distinct, centroid 4/4 ok, peak 51-53)
[AF] Step 4/4 (back)  pos=165665  sharp=4.6%  (4 samples, 4 distinct, centroid 4/4 ok, peak 47-49)
[AF] Fine pass: 165984 (coarse 166386, -402)
[AF] Re-measured start pos=168665 now: 4.6 (was 14.6); fit pos=165984: 4.6
[AF] Same sky, edge metric: start pos=168665 0.1881, fit pos=165984 0.1917
[AF] The sky changed, not the focus: keeping pos=165984
23:12:44.097 [AF] SUCCESS: pos=165984  sharp=4.6%  improvement=+0.0%
"""
    fails = 0
    def ok(c, what):
        nonlocal fails
        print(("  ok    " if c else "  FAIL  ") + what)
        if not c: fails += 1
    s = parse(log.splitlines(), "20261121")
    ok(len(s) == 1, "one sweep")
    s = s[0]
    ok(s["start"] == 168665 and s["result"] == 165984, "start and result")
    ok(s["coarse"]["pos"] == [165665, 166265], "positions, sorted")
    ok(s["coarse"]["out"] == [14.6, 14.8] and s["coarse"]["back"] == [4.6, 4.6], "out and back scores by position")
    ok(s["coarse"]["peak_out"] == [200, 207] and s["coarse"]["peak_back"] == [49, 53], "the peak at each step")
    ok(s["ab"]["ran"] and s["ab"]["start_before"] == 14.6 and s["ab"]["start_now"] == 4.6, "the A/B")
    ok(s["ab"]["edge_start"] == 0.1881 and s["ab"]["verdict"] == "kept fit", "its edge scores and verdict")
    ok(s["id"] == "log-20261121-231023" and s["at"]["gain"] is None, "id from the night and time; unknowns null")
    ok(json.loads(json.dumps(s)) == s, "round-trips as JSON")
    print("ALL CHECKS PASSED" if fails == 0 else f"{fails} CHECK(S) FAILED")
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
