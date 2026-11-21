#!/usr/bin/env python3
"""Phase 156.3/156.4: the night's results bundle, in field-results/<night>/.

The app writes results.json there at the end of a sky run. This adds what a
person, or Claude Code, needs to read the night without the PC that ran it:

  summary.md            the night in plain words: each test, its status and
                        why, each checklist item's own status, the measures
  log_excerpt.txt       the run's lines from raduk_log.txt, from
                        "[SKYRUN] started" to the park (or the abort), kept
                        to the tags that tell the story
  recordings.txt        each recording's path, from the log; no recordings
                        are copied (too large)
  validation_*.json     the night's validation log(s), copied (155.4)

Standard library only, offline (rule 7). Running it twice gives the same files.

Usage:
  python tools/sky_bundle.py field-results/<night>/results.json \
      [--log <raduk_log.txt>] [--logs <logs folder>]
  python tools/sky_bundle.py --self-test
"""
import json
import os
import re
import shutil
import sys
import tempfile

# The tags a night's story is told in. [SHARP] and [MOTION] (twice a
# second) and the like would bury it.
TAGS = ("[SKYRUN]", "[PARK]", "[AF]", "[ACQ]", "[ACQUIRE]", "[CAMERA]", "[FADE]", "[DEROT]",
        "[SAFETY]", "[FOCUS]", "[STACK]", "[RECOVER]", "[WX]", "[CAL]", "[GOTO]", "[SLEW]")

LABEL = {
    "passed": "PASSED", "failed": "FAILED", "collected": "COLLECTED (your eye owed)",
    "clouded": "CLOUDED", "aborted-stall": "ABORTED (a device stalled)",
    "not-reached": "NOT REACHED", "skipped-horizon": "SKIPPED (below the horizon limit)",
    "not-available": "NOT AVAILABLE", "pending": "PENDING", "running": "RUNNING",
}


def run_slice(lines):
    """The last sky run's lines: from its last 'started' to its park or abort."""
    start = None
    for i, l in enumerate(lines):
        if "[SKYRUN] started" in l:
            start = i
    if start is None:
        return []
    end = len(lines)
    for j in range(start + 1, len(lines)):
        if "[SKYRUN] parked;" in lines[j] or "[SKYRUN] aborted" in lines[j]:
            end = j + 1
            break
    return lines[start:end]


def excerpt(lines):
    return [l for l in run_slice(lines) if l.lstrip().startswith(TAGS)]


REC_RE = re.compile(r"\[STACK\] recording complete: .*path=(.+)$")


def recordings(lines):
    out = []
    for l in run_slice(lines):
        m = REC_RE.search(l.rstrip())
        if m and m.group(1).strip() not in ("(empty)", ""):
            out.append(m.group(1).strip())
    return out


def summary(res):
    night = res.get("night", "?")
    out = [f"# Sky run {night}", ""]
    out.append(f"- App {res.get('app', '?')}" + (", **emulator run**: settles no checklist item" if res.get("emulator") else ""))
    out.append(f"- Ended: {res.get('stopped') or 'not recorded'}; parked: {'yes' if res.get('parked') else 'no'}")
    tests = res.get("tests", [])
    ran = [t for t in tests if t.get("status") not in ("not-available", "pending")]
    out.append(f"- {len(ran)} of {len(tests)} planned tests ran or were tried")
    out.append("")
    out.append("| Group | Target | Status | Why |")
    out.append("|---|---|---|---|")
    for t in tests:
        detail = (t.get("detail") or "").replace("|", "/")
        out.append(f"| {t.get('group', '?')} | {t.get('target', '')} | {LABEL.get(t.get('status'), t.get('status', '?'))} | {detail} |")
    out.append("")
    for t in tests:
        items = t.get("item_status") or {}
        meas = t.get("measures") or []
        if not items and not meas:
            continue
        out.append(f"## {t.get('group', '?')} on {t.get('target', '')}")
        out.append("")
        for n in t.get("items", []):
            st = items.get(str(n), t.get("status", "?"))
            out.append(f"- Item {n}: {LABEL.get(st, st)}")
        for m in meas:
            v = m.get("value")
            vs = f"{v:g}" if isinstance(v, (int, float)) else str(v)
            exp = m.get("expected")
            es = f" against {exp:g}" if isinstance(exp, (int, float)) else ""
            out.append(f"- Item {m.get('item', '?')}, {m.get('what', '')}: {vs}{m.get('unit', '')}{es}")
        out.append("")
    out.append("Recordings are not in the bundle: `recordings.txt` lists their paths on the capture PC.")
    return "\n".join(out) + "\n"


def write_text(path, text):
    with open(path, "w", encoding="utf-8", newline="\n") as f:
        f.write(text)


def bundle(results_path, log_path=None, logs_dir=None):
    out_dir = os.path.dirname(os.path.abspath(results_path))
    with open(results_path, encoding="utf-8") as f:
        res = json.load(f)
    if res.get("format") != "raduk-sky-results":
        raise ValueError(f"{results_path} is not a sky run's results.json")
    written = []
    write_text(os.path.join(out_dir, "summary.md"), summary(res))
    written.append("summary.md")
    lines = []
    if log_path and os.path.isfile(log_path):
        with open(log_path, encoding="utf-8", errors="replace") as f:
            lines = f.read().splitlines()
    ex = excerpt(lines)
    write_text(os.path.join(out_dir, "log_excerpt.txt"),
               "\n".join(ex) + "\n" if ex else "No sky run found in the log given.\n")
    written.append("log_excerpt.txt")
    recs = recordings(lines)
    write_text(os.path.join(out_dir, "recordings.txt"), "\n".join(recs) + "\n" if recs else "No recordings in the run.\n")
    written.append("recordings.txt")
    night = res.get("night", "")
    if logs_dir and os.path.isdir(logs_dir) and night:
        for name in sorted(os.listdir(logs_dir)):
            if name.startswith(f"validation_{night}_") and name.endswith(".json"):
                shutil.copyfile(os.path.join(logs_dir, name), os.path.join(out_dir, name))
                written.append(name)
    return written


def self_test():
    fails = []

    def ok(cond, what):
        print(f"  [{'PASS' if cond else 'FAIL'}] {what}")
        if not cond:
            fails.append(what)

    res = {"format": "raduk-sky-results", "version": 1, "night": "20261121", "app": "3.3.41", "emulator": False,
           "stopped": "the end of the list", "parked": True, "tests": [
               {"group": "planet", "target": "Saturn", "status": "collected", "detail": "4 of 4 ladder recordings",
                "items": [1, 13, 47, 55, 58], "item_status": {"1": "collected", "13": "passed", "47": "collected",
                                                               "55": "collected", "58": "collected"},
                "measures": [{"item": 13, "what": "frame rate", "value": 98.2, "expected": 100, "unit": " fps"},
                             {"item": 47, "what": "repeatability", "value": 12, "expected": None, "unit": " steps"}]},
               {"group": "moon", "target": "Moon", "status": "not-available", "detail": "its steps are not built yet",
                "items": [18, 58], "item_status": {}, "measures": []}]}
    log = """[SHARP] 41
[SKYRUN] started: 2 groups planned
[MOTION] x
[SKYRUN] group planet on Saturn
[AF] Complete pos=10006
[STACK] recording complete: normal=1  autoStack=0  path=D:\\cap\\Saturn_1.ser
[SHARP] 42
[STACK] recording complete: normal=1  autoStack=0  path=(empty)
[SKYRUN] parked; the end of the list
[SKYRUN] started: an older run after this one would win
"""
    with tempfile.TemporaryDirectory() as d:
        night_dir = os.path.join(d, "field-results", "20261121")
        os.makedirs(night_dir)
        rp = os.path.join(night_dir, "results.json")
        with open(rp, "w") as f:
            json.dump(res, f)
        lp = os.path.join(d, "raduk_log.txt")
        logs = os.path.join(d, "logs")
        os.makedirs(logs)
        for n in ("validation_20261121_200000.json", "validation_20261120_200000.json"):
            with open(os.path.join(logs, n), "w") as f:
                f.write("{}")
        # One run in this log; the last-started rule is checked on its own below.
        with open(lp, "w") as f:
            f.write(log.replace("[SKYRUN] started: an older run after this one would win\n", ""))
        w = bundle(rp, lp, logs)
        ok(w == ["summary.md", "log_excerpt.txt", "recordings.txt", "validation_20261121_200000.json"],
           "writes the summary, the excerpt, the recordings list and only tonight's validation log")
        s = open(os.path.join(night_dir, "summary.md")).read()
        ok("| planet | Saturn | COLLECTED (your eye owed) | 4 of 4 ladder recordings |" in s, "the table: status in words, and why")
        ok("- Item 13: PASSED" in s and "- Item 55: COLLECTED (your eye owed)" in s, "each item with its own status")
        ok("- Item 13, frame rate: 98.2 fps against 100" in s and "- Item 47, repeatability: 12 steps" in s,
           "the measures, with the expected value where there is one")
        ok("## moon" not in s, "a group with nothing to report gets no section")
        ex = open(os.path.join(night_dir, "log_excerpt.txt")).read()
        ok("[SKYRUN] group planet on Saturn" in ex and "[AF] Complete" in ex and "[SHARP]" not in ex and "[MOTION]" not in ex,
           "the excerpt keeps the story's tags and drops the readouts")
        ok(open(os.path.join(night_dir, "recordings.txt")).read() == "D:\\cap\\Saturn_1.ser\n",
           "recordings listed by path, an empty path left out")
        again = bundle(rp, lp, logs)
        ok(again == w and open(os.path.join(night_dir, "summary.md")).read() == s, "running it twice gives the same files")
        lines = (log).splitlines()
        ok(run_slice(lines)[0] == "[SKYRUN] started: an older run after this one would win",
           "the last run started is the one bundled")
        bad = os.path.join(d, "other.json")
        with open(bad, "w") as f:
            json.dump({"format": "something-else"}, f)
        try:
            bundle(bad)
            ok(False, "a file that is not results.json is refused")
        except ValueError:
            ok(True, "a file that is not results.json is refused")
    print("\n" + ("SOME CHECKS FAILED" if fails else "ALL CHECKS PASSED"))
    return 1 if fails else 0


def main(argv):
    if "--self-test" in argv:
        return self_test()
    if not argv or argv[0].startswith("-"):
        print(__doc__)
        return 2
    log = logs = None
    if "--log" in argv:
        log = argv[argv.index("--log") + 1]
    if "--logs" in argv:
        logs = argv[argv.index("--logs") + 1]
    written = bundle(argv[0], log, logs)
    print("sky_bundle: wrote " + ", ".join(written))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
