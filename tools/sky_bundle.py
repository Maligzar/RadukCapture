#!/usr/bin/env python3
"""Phases 156.3/156.4: a night's bundle, in field-results/<night>/.

Three kinds of night go through it the same way (the owner, 2026-11-21):

  sky-run    an unattended sky run: the app wrote results.json there
  imaging    a normal night of imaging: no results.json, the evidence is
             the recordings, Autorun's rankings and the validation log
  dash-test  a night with Dash's Playwright suite run against the app: as
             imaging, plus Playwright's report
  report     the troubleshooting button (phase 171): the owner's words and
             the app's state in report.json, written by the app with the
             site and secrets already removed, plus the log's last lines,
             rig.ini (redacted) and screenshots

Into the folder it writes:

  bundle.json           the kind and the night, read by sky_handoff.py
  summary.md            the night in plain words
  log_excerpt.txt       the night's lines from raduk_log.txt, kept to the
                        tags that tell the story (a sky run: from
                        "[SKYRUN] started" to the park)
  recordings.txt        each recording's path, from the log; no recordings
                        are copied (too large)
  validation_*.json     the night's validation log(s) (155.4)
  focus_history.jsonl   every autofocus sweep and the hand corrections after it (174.0)
  autorun_*.txt         Autorun's ranked runs for the night, if any
  dash-results.json     Playwright's report, for a dash-test night

Standard library only, offline (rule 7). Running it twice gives the same files.

Usage:
  python tools/sky_bundle.py field-results/<night>/results.json [--log L] [--logs D] [--captures C] [--dash R]
  python tools/sky_bundle.py --night <YYYYMMDD> --out field-results [--log L] [--logs D] [--captures C] [--dash R]
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


def excerpt(lines, sky_run=True):
    src = run_slice(lines) if sky_run else lines
    return [l for l in src if l.lstrip().startswith(TAGS)]


REC_RE = re.compile(r"\[STACK\] recording complete: .*path=(.+)$")


def recordings(lines, sky_run=True):
    out = []
    for l in (run_slice(lines) if sky_run else lines):
        m = REC_RE.search(l.rstrip())
        if m and m.group(1).strip() not in ("(empty)", ""):
            out.append(m.group(1).strip())
    return out


def night_of(stamp):
    """The night a YYYYMMDD_HHMMSS stamp belongs to: before noon is the night before
    (ValidationLog.h's NightOf)."""
    import datetime
    t = datetime.datetime.strptime(stamp[:15], "%Y%m%d_%H%M%S")
    if t.hour < 12:
        t -= datetime.timedelta(days=1)
    return t.strftime("%Y%m%d")


def dash_summary(path):
    """Playwright's JSON report: the counts, and each test that did not pass."""
    with open(path, encoding="utf-8") as f:
        rep = json.load(f)
    st = rep.get("stats", {})
    failed = []

    def walk(suite, trail):
        name = trail + ([suite["title"]] if suite.get("title") else [])
        for spec in suite.get("specs", []):
            for t in spec.get("tests", []):
                if t.get("status") == "unexpected" or any(r.get("status") in ("failed", "timedOut") for r in t.get("results", [])):
                    proj = t.get("projectName", "")
                    msg = ""
                    for r in t.get("results", []):
                        e = (r.get("error") or {}).get("message", "")
                        if e:
                            msg = e.strip().splitlines()[0][:200]
                    failed.append(f"{' > '.join(name + [spec.get('title', '')])} [{proj}]" + (f": {msg}" if msg else ""))
        for sub in suite.get("suites", []):
            walk(sub, name)

    for s_ in rep.get("suites", []):
        walk(s_, [])
    return st, failed


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


def night_summary(kind, night, recs, autoruns, validations, dash):
    title = {"imaging": "Imaging night", "dash-test": "Dash test night"}.get(kind, "Night")
    out = [f"# {title} {night}", ""]
    out.append(f"- {len(recs)} recording(s); paths in `recordings.txt` (on the capture PC)")
    out.append(f"- Autorun rankings: {', '.join(autoruns) if autoruns else 'none'}")
    out.append(f"- Validation log: {', '.join(validations) if validations else 'none found for this night'}")
    if dash is not None:
        st, failed = dash
        out.append(f"- Dash's Playwright suite: {st.get('expected', 0)} passed, {st.get('unexpected', 0)} failed, "
                   f"{st.get('skipped', 0)} skipped, {st.get('flaky', 0)} flaky")
        if failed:
            out.append("")
            out.append("## Dash tests that did not pass")
            out.append("")
            out.extend(f"- {f}" for f in failed)
    out.append("")
    out.append("No checklist item is marked from a night like this: only a sky run's results do that. "
               "The analysis says what the evidence shows, and what it does not settle.")
    return "\n".join(out) + "\n"


def _copy_night_files(out_dir, night, logs_dir, captures_dir, dash_path):
    written, validations, autoruns, dash = [], [], [], None
    if logs_dir and os.path.isdir(logs_dir) and night:
        for name in sorted(os.listdir(logs_dir)):
            if name.startswith(f"validation_{night}_") and name.endswith(".json"):
                shutil.copyfile(os.path.join(logs_dir, name), os.path.join(out_dir, name))
                written.append(name); validations.append(name)
        # Phase 174.0: the focus history, whole (every night's sweeps and the
        # hand corrections after them; small, and the history is the point).
        fh = os.path.join(logs_dir, "focus_history.jsonl")
        if os.path.isfile(fh):
            shutil.copyfile(fh, os.path.join(out_dir, "focus_history.jsonl"))
            written.append("focus_history.jsonl")
    if captures_dir and os.path.isdir(captures_dir) and night:
        for name in sorted(os.listdir(captures_dir)):
            if name.startswith("autorun_") and name.endswith(".txt"):
                try:
                    if night_of(name[len("autorun_"):-4]) != night:
                        continue
                except ValueError:
                    continue
                shutil.copyfile(os.path.join(captures_dir, name), os.path.join(out_dir, name))
                written.append(name); autoruns.append(name)
    if dash_path and os.path.isfile(dash_path):
        shutil.copyfile(dash_path, os.path.join(out_dir, "dash-results.json"))
        written.append("dash-results.json")
        dash = dash_summary(dash_path)
    return written, validations, autoruns, dash


def _read_log(log_path):
    if log_path and os.path.isfile(log_path):
        with open(log_path, encoding="utf-8", errors="replace") as f:
            return f.read().splitlines()
    return []


def bundle(results_path, log_path=None, logs_dir=None, captures_dir=None, dash_path=None):
    """A sky run's bundle, beside its results.json."""
    out_dir = os.path.dirname(os.path.abspath(results_path))
    with open(results_path, encoding="utf-8") as f:
        res = json.load(f)
    if res.get("format") != "raduk-sky-results":
        raise ValueError(f"{results_path} is not a sky run's results.json")
    night = res.get("night", "")
    lines = _read_log(log_path)
    ex = excerpt(lines)
    recs = recordings(lines)
    write_text(os.path.join(out_dir, "summary.md"), summary(res))
    write_text(os.path.join(out_dir, "log_excerpt.txt"),
               "\n".join(ex) + "\n" if ex else "No sky run found in the log given.\n")
    write_text(os.path.join(out_dir, "recordings.txt"), "\n".join(recs) + "\n" if recs else "No recordings in the run.\n")
    written = ["summary.md", "log_excerpt.txt", "recordings.txt"]
    more, _, _, _ = _copy_night_files(out_dir, night, logs_dir, captures_dir, dash_path)
    written += more
    write_text(os.path.join(out_dir, "bundle.json"),
               json.dumps({"format": "raduk-night-bundle", "version": 1, "kind": "sky-run", "night": night,
                           "emulator": bool(res.get("emulator"))}, indent=1) + "\n")
    return ["bundle.json"] + written


def bundle_night(night, out_root, log_path=None, logs_dir=None, captures_dir=None, dash_path=None):
    """An imaging night's bundle (or, with Playwright's report, a Dash test night's),
    in out_root/<night>/. Returns (folder, files written)."""
    if not (len(night) == 8 and night.isdigit()):
        raise ValueError(f"night must be YYYYMMDD, not {night!r}")
    out_dir = os.path.join(out_root, night)
    os.makedirs(out_dir, exist_ok=True)
    kind = "dash-test" if dash_path else "imaging"
    lines = _read_log(log_path)
    ex = excerpt(lines, sky_run=False)
    recs = recordings(lines, sky_run=False)
    more, validations, autoruns, dash = _copy_night_files(out_dir, night, logs_dir, captures_dir, dash_path)
    write_text(os.path.join(out_dir, "summary.md"), night_summary(kind, night, recs, autoruns, validations, dash))
    write_text(os.path.join(out_dir, "log_excerpt.txt"), "\n".join(ex) + "\n" if ex else "No log given, or nothing in it.\n")
    write_text(os.path.join(out_dir, "recordings.txt"), "\n".join(recs) + "\n" if recs else "No recordings in the log.\n")
    write_text(os.path.join(out_dir, "bundle.json"),
               json.dumps({"format": "raduk-night-bundle", "version": 1, "kind": kind, "night": night,
                           "emulator": False}, indent=1) + "\n")
    return out_dir, ["bundle.json", "summary.md", "log_excerpt.txt", "recordings.txt"] + more


def bundle_report(report_dir):
    """A troubleshooting report's bundle, in the folder the app wrote."""
    with open(os.path.join(report_dir, "report.json"), encoding="utf-8") as f:
        rep = json.load(f)
    if rep.get("format") != "raduk-problem-report":
        raise ValueError(f"{report_dir} is not a troubleshooting report")
    stamp = rep.get("stamp", os.path.basename(os.path.normpath(report_dir)))
    out = [f"# Problem report {stamp}", ""]
    out.append(f"- App {rep.get('app', '?')}" + (" (emulator)" if rep.get("emulator") else "") +
               f", night {rep.get('night', '?')}")
    out += ["", "## What the owner said", "", rep.get("description", "").strip() or "(nothing written)", ""]
    st = rep.get("state") or {}
    if st:
        out += ["## The app at the time", ""]
        out += [f"- {k}: {v}" for k, v in st.items()]
        out.append("")
    files = sorted(n for n in os.listdir(report_dir) if n not in ("bundle.json", "summary.md"))
    out += ["## In this folder", ""] + [f"- `{n}`" for n in files]
    out += ["", "The site, the Dash token and any key, secret or password were removed by the app before this was written."]
    write_text(os.path.join(report_dir, "summary.md"), "\n".join(out) + "\n")
    write_text(os.path.join(report_dir, "bundle.json"),
               json.dumps({"format": "raduk-night-bundle", "version": 1, "kind": "report", "night": rep.get("night", ""),
                           "stamp": stamp, "emulator": bool(rep.get("emulator"))}, indent=1) + "\n")
    return stamp, ["bundle.json", "summary.md"]


def self_test():
    fails = []

    def ok(cond, what, detail=""):
        print(f"  [{'PASS' if cond else 'FAIL'}] {what}" + (f"  {detail}" if detail and not cond else ""))
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
        ok(w == ["bundle.json", "summary.md", "log_excerpt.txt", "recordings.txt", "validation_20261121_200000.json"],
           "writes the kind, the summary, the excerpt, the recordings list and only tonight's validation log", str(w))
        ok(json.load(open(os.path.join(night_dir, "bundle.json")))["kind"] == "sky-run", "bundle.json says a sky run")
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
    # 2026-11-21: a normal imaging night, and a Dash test night.
    with tempfile.TemporaryDirectory() as d:
        log = """[ACQ] PLANET LOCK: locked
[SHARP] 40
[AUTORUN] run 1 of 25
[STACK] recording complete: normal=1  autoStack=0  path=D:\\cap\\Saturn_2015.ser
[STACK] recording complete: normal=1  autoStack=0  path=D:\\cap\\Saturn_2021.ser
"""
        lp = os.path.join(d, "raduk_log.txt")
        open(lp, "w").write(log)
        caps = os.path.join(d, "captures"); os.makedirs(caps)
        for n in ("autorun_20261121_221500.txt", "autorun_20261122_013000.txt", "autorun_20261122_200000.txt", "autorun_x.txt"):
            open(os.path.join(caps, n), "w").write("ranked")
        logs = os.path.join(d, "logs"); os.makedirs(logs)
        open(os.path.join(logs, "validation_20261121_190000.json"), "w").write("{}")
        out, w = bundle_night("20261121", os.path.join(d, "field-results"), lp, logs, caps)
        meta = json.load(open(os.path.join(out, "bundle.json")))
        ok(meta["kind"] == "imaging" and meta["night"] == "20261121", "an imaging night: bundle.json says so")
        ok("autorun_20261121_221500.txt" in w and "autorun_20261122_013000.txt" in w and "autorun_20261122_200000.txt" not in w,
           "Autorun's rankings for the night, after midnight included, the next night's left out", str(w))
        ok("validation_20261121_190000.json" in w, "the night's validation log")
        ok(open(os.path.join(out, "recordings.txt")).read().count(".ser") == 2, "both recordings listed, with no sky run in the log")
        sm = open(os.path.join(out, "summary.md")).read()
        ok(sm.startswith("# Imaging night 20261121") and "No checklist item is marked" in sm,
           "the summary says no checklist item is marked from it")
        ex = open(os.path.join(out, "log_excerpt.txt")).read()
        ok("[ACQ] PLANET LOCK: locked" in ex and "[SHARP]" not in ex, "the whole log's story lines, without a sky run's markers")
        rep = {"stats": {"expected": 38, "unexpected": 1, "skipped": 10, "flaky": 0}, "suites": [
            {"title": "park.spec.js", "specs": [], "suites": [{"title": "Dash: park", "specs": [
                {"title": "two taps send park", "tests": [{"projectName": "phone", "status": "unexpected",
                 "results": [{"status": "failed", "error": {"message": "expected PARK\nmore"}}]}]},
                {"title": "fine", "tests": [{"projectName": "desktop", "status": "expected", "results": [{"status": "passed"}]}]}]}]}]}
        rp = os.path.join(d, "dash-results.json")
        json.dump(rep, open(rp, "w"))
        out2, w2 = bundle_night("20261121", os.path.join(d, "fr2"), lp, logs, caps, rp)
        ok(json.load(open(os.path.join(out2, "bundle.json")))["kind"] == "dash-test" and "dash-results.json" in w2,
           "with Playwright's report: a Dash test night, the report kept")
        sm2 = open(os.path.join(out2, "summary.md")).read()
        ok("38 passed, 1 failed, 10 skipped" in sm2 and "park.spec.js > Dash: park > two taps send park [phone]: expected PARK" in sm2,
           "the counts, and each failed test with its first error line", sm2)
        try:
            bundle_night("tonight", d)
            ok(False, "a night that is not YYYYMMDD is refused")
        except ValueError:
            ok(True, "a night that is not YYYYMMDD is refused")
        ok(night_of("20261122_013000") == "20261121" and night_of("20261121_221500") == "20261121", "after midnight is the night before")
    # Phase 171: a troubleshooting report.
    with tempfile.TemporaryDirectory() as d:
        rd = os.path.join(d, "reports", "20261122_021500")
        os.makedirs(rd)
        json.dump({"format": "raduk-problem-report", "version": 1, "stamp": "20261122_021500", "night": "20261121",
                   "app": "3.3.44", "emulator": False, "description": "Autofocus took Saturn to a donut",
                   "state": {"target": "Saturn", "focuser": "168655"}, "files": ["log_tail.txt"]},
                  open(os.path.join(rd, "report.json"), "w"))
        open(os.path.join(rd, "log_tail.txt"), "w").write("[AF] x\n")
        stamp, w = bundle_report(rd)
        sm = open(os.path.join(rd, "summary.md")).read()
        ok(stamp == "20261122_021500" and json.load(open(os.path.join(rd, "bundle.json")))["kind"] == "report",
           "a report: bundle.json says so")
        ok("Autofocus took Saturn to a donut" in sm and "- target: Saturn" in sm and "`log_tail.txt`" in sm,
           "its summary: the owner's words, the state, the files")
        json.dump({"format": "x"}, open(os.path.join(rd, "report.json"), "w"))
        try:
            bundle_report(rd)
            ok(False, "a folder that is not a report is refused")
        except ValueError:
            ok(True, "a folder that is not a report is refused")
    print("\n" + ("SOME CHECKS FAILED" if fails else "ALL CHECKS PASSED"))
    return 1 if fails else 0


def main(argv):
    if "--self-test" in argv:
        return self_test()

    def opt(name):
        return argv[argv.index(name) + 1] if name in argv else None
    log, logs, caps, dash = opt("--log"), opt("--logs"), opt("--captures"), opt("--dash")
    if opt("--night"):
        out, written = bundle_night(opt("--night"), opt("--out") or "field-results", log, logs, caps, dash)
        print(f"sky_bundle: {out}: " + ", ".join(written))
        return 0
    if not argv or argv[0].startswith("-"):
        print(__doc__)
        return 2
    written = bundle(argv[0], log, logs, caps, dash)
    print("sky_bundle: wrote " + ", ".join(written))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
