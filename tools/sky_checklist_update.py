#!/usr/bin/env python3
"""Phase 156.3b: mark the clear-night checklist from a sky run's results.

    python tools/sky_checklist_update.py field-results/<night>/results.json
    python tools/sky_checklist_update.py <results.json> --checklist docs/CLEAR-NIGHT-CHECKLIST.md
    python tools/sky_checklist_update.py --self-test

For each checklist item the night touched, it writes one status line under
the item, replacing that item's previous sky-run line, so re-running never
stacks duplicates:

    **Sky run 20261121: PASSED** (planet on Saturn: fine pass +12)

and a "Last sky run" table near the top, between markers, listing every item
the night tested. A passed item is marked, never deleted: removing an item
and its UNVERIFIED tags stays a reviewed change.

It refuses an emulator run: only the sky settles a clear-night item. The
owner asked for this on 2026-11-13; it does not depend on Claude Code.
Standard library only, no network.
"""
import argparse
import json
import re
import sys

START = "<!-- sky-run:start -->"
END = "<!-- sky-run:end -->"
LABEL = {
    "passed": "PASSED", "collected": "COLLECTED, your eye owed", "failed": "FAILED", "skipped-horizon": "SKIPPED, outside the horizon limits or slew bounds",
    "clouded": "CLOUDED", "aborted-stall": "ABORTED, a device stalled", "not-reached": "NOT REACHED",
    "not-available": "NOT AVAILABLE",
}
# The line's headline when several tests touched one item: the most telling.
# A failure leads; then a pass, since a group that could not run (clouded,
# not reached, skipped, not available) tested nothing; the line still lists
# every test, so a skipped half of an item stays in view.
# Collected (the owner, 2026-11-21) leads a pass: it asks something of you.
RANK = ["failed", "aborted-stall", "collected", "passed", "clouded", "not-reached", "skipped-horizon", "not-available"]
HEADING = re.compile(r"^## (\d+)\.\s")
NUMBERED = re.compile(r"^(\d+)\.\s+\*\*")
SKYLINE = re.compile(r"^\s*\*\*Sky run [0-9]{8}: ")


def item_starts(lines):
    """Line index of each item's first line: '## N.' for 1-19, 'N. **' from 20."""
    starts = {}
    for i, l in enumerate(lines):
        m = HEADING.match(l)
        if m:
            starts.setdefault(int(m.group(1)), i)
            continue
        m = NUMBERED.match(l)
        if m and int(m.group(1)) >= 20:
            starts.setdefault(int(m.group(1)), i)
    return starts


def item_end(lines, starts, n):
    """The line after item n's block: the next item's start or a '---' / '#' heading."""
    s = starts[n]
    later = sorted(v for v in starts.values() if v > s)
    stop = later[0] if later else len(lines)
    for i in range(s + 1, stop):
        if lines[i].startswith("---") or (lines[i].startswith("#") and not lines[i].startswith("####")):
            return i
    return stop


def per_item(results):
    """item -> list of (status, text) from every test that touched it."""
    by = {}
    tests = results.get("tests", [])
    # A clouded attempt that was tried again is replaced by the retry.
    retried = set()
    for i, t in enumerate(tests):
        if t.get("status") == "clouded" and any(
                u.get("group") == t.get("group") and u.get("target") == t.get("target") and u.get("status") != "clouded"
                for u in tests[i + 1:]):
            retried.add(i)
    for i, t in enumerate(tests):
        if i in retried:
            continue
        what = t.get("group", "?") + (f" on {t['target']}" if t.get("target") else "")
        detail = t.get("detail", "")
        meas = [m for m in t.get("measures", [])]
        for item in t.get("items", []):
            extra = "; ".join(
                f"{m['what']} {m['value']:g}{m.get('unit', '')}" + (f" against {m['expected']:g}" if m.get("expected") is not None else "")
                for m in meas if m.get("item") == item and isinstance(m.get("value"), (int, float)))
            text = what + (": " + detail if detail else "") + (f" ({extra})" if extra else "")
            # 156.2b: a group that settles several items may say how each came out.
            status = (t.get("item_status") or {}).get(str(item), t.get("status", "?"))
            by.setdefault(item, []).append((status, text))
    return by


def status_line(night, entries, indent):
    worst = min(entries, key=lambda e: RANK.index(e[0]) if e[0] in RANK else len(RANK))[0]
    return f"{indent}**Sky run {night}: {LABEL.get(worst, worst.upper())}** (" + "; ".join(
        f"{LABEL.get(s, s).split(',')[0].lower()}: {t}" if len(entries) > 1 else t for s, t in entries) + ")"


def update(text, results):
    if results.get("format") != "raduk-sky-results":
        raise ValueError("not a sky run's results.json")
    if results.get("emulator"):
        raise ValueError("an emulator run: only the sky settles a clear-night item, so the checklist is not marked")
    night = results.get("night", "?")
    lines = text.split("\n")
    by = per_item(results)
    starts = item_starts(lines)
    missing = sorted(i for i in by if i not in starts)
    # Bottom-up, so earlier indices stay valid.
    for n in sorted((i for i in by if i in starts), key=lambda i: starts[i], reverse=True):
        s, e = starts[n], item_end(lines, starts, n)
        block = [i for i in range(s + 1, e) if SKYLINE.match(lines[i])]
        for i in reversed(block):
            del lines[i]
        heading = lines[s].startswith("## ")
        line = status_line(night, by[n], "" if heading else "    ")
        if heading:
            # Whatever blank lines follow the heading become exactly: a blank,
            # the status line, a blank. So a second run changes nothing.
            k = s + 1
            while k < len(lines) and lines[k].strip() == "":
                k += 1
            lines[s + 1:k] = ["", line, ""]
        else:
            lines.insert(s + 1, line)
    # The table, between its markers, after the title and before the first rule.
    rows = [f"Last sky run: **{night}**" + (f", stopped at {results['stopped']}" if results.get("stopped") else "")
            + (", parked" if results.get("parked") else ", NOT parked") + ".", "",
            "| Item | Status | What ran |", "|---|---|---|"]
    for n in sorted(by):
        worst = min(by[n], key=lambda e: RANK.index(e[0]) if e[0] in RANK else len(RANK))[0]
        rows.append(f"| {n} | {LABEL.get(worst, worst).split(',')[0]} | " +
                    "; ".join(t.replace("|", "\\|") for _, t in by[n]) + " |")
    if missing:
        rows += ["", "Items in the results that this checklist does not have: " + ", ".join(map(str, missing)) + "."]
    block = [START] + rows + [END]
    text2 = "\n".join(lines)
    if START in text2 and END in text2:
        a, b = text2.index(START), text2.index(END) + len(END)
        text2 = text2[:a] + "\n".join(block) + text2[b:]
    else:
        lines = text2.split("\n")
        at = next((i for i, l in enumerate(lines) if l.startswith("---")), 1)
        lines[at:at] = block + [""]
        text2 = "\n".join(lines)
    return text2, sorted(by), missing


# ── Self-test (run by CI) ─────────────────────────────────────

SAMPLE = """# What needs a clear night

Intro.

---

## 1. `SHARP_CEIL = 1000` — the one that biases everything

**File:** `src/core/Sharpness.h`

Body.

---

## 3. Plate scale

Body three.

---

## Items from later phases

47. **Autofocus, on the sky** (F9). Defocus and run it.
    More text.

58. **The fine pass** (145).
    More.
"""


def _res(tests, emulator=False, night="20261121"):
    return {"format": "raduk-sky-results", "night": night, "emulator": emulator, "stopped": "the end of the list",
            "parked": True, "tests": tests}


def self_test():
    fails = []

    def check(ok, what):
        print(f"  [{'PASS' if ok else 'FAIL'}] {what}")
        if not ok:
            fails.append(what)

    tests = [
        {"group": "planet", "target": "Saturn", "status": "passed", "detail": "fine pass +12", "items": [1, 47, 58],
         "measures": [{"item": 1, "what": "sharpness p99", "value": 61, "expected": None, "unit": "%"}]},
        {"group": "moon", "target": "", "status": "skipped-horizon", "detail": "Moon at 12 deg, below the horizon limit",
         "items": [58], "measures": []},
        {"group": "solve", "target": "Vega", "status": "failed", "detail": "no solve", "items": [3, 99], "measures": []},
    ]
    out, touched, missing = update(SAMPLE, _res(tests))
    check(touched == [1, 3, 47, 58, 99] and missing == [99], "every item touched; one the checklist lacks is reported")
    check("**Sky run 20261121: PASSED** (planet on Saturn: fine pass +12 (sharpness p99 61%))" in out,
          "a heading item gets its line, with the measure")
    check("    **Sky run 20261121: PASSED** (planet on Saturn: fine pass +12)" in out, "a numbered item gets an indented line")
    check("**Sky run 20261121: PASSED** (passed: planet on Saturn: fine pass +12; skipped: moon: Moon at 12 deg" in out,
          "two groups on one item: the pass leads, the skip is listed")
    check("**Sky run 20261121: FAILED** (solve on Vega: no solve)" in out, "a failure is marked as such")
    check(out.count(START) == 1 and "| 58 | PASSED |" in out and "| 1 | PASSED |" in out, "the Last sky run table")
    check(out.index(START) < out.index("## 1."), "the table sits above the items")
    check("Body." in out and "More text." in out, "nothing else is touched")
    again, _, _ = update(out, _res(tests))
    check(again == out, "running it twice changes nothing more")
    later, _, _ = update(out, _res([{"group": "planet", "target": "Jupiter", "status": "failed", "detail": "x",
                                     "items": [47], "measures": []}], night="20261125"))
    check("Sky run 20261125: FAILED" in later and "Sky run 20261121: PASSED** (planet on Saturn: fine pass +12)\n" not in later.split("47.")[1].split("58.")[0],
          "a later night replaces the item's line, not adds to it")
    check(later.count(START) == 1 and "Last sky run: **20261125**" in later, "and replaces the table")
    retry = [{"group": "jupiter", "target": "Jupiter", "status": "clouded", "detail": "clouds", "items": [47], "measures": []},
             {"group": "jupiter", "target": "Jupiter", "status": "passed", "detail": "ran", "items": [47], "measures": []}]
    out3, _, _ = update(SAMPLE, _res(retry))
    check("**Sky run 20261121: PASSED** (jupiter on Jupiter: ran)" in out3, "a clouded attempt tried again is replaced by the retry")
    mixed = [{"group": "planet", "target": "Saturn", "status": "passed", "detail": "ok", "items": [58], "measures": []},
             {"group": "moon", "target": "", "status": "skipped-horizon", "detail": "low", "items": [58], "measures": []}]
    out4, _, _ = update(SAMPLE, _res(mixed))
    check("Sky run 20261121: PASSED** (passed: planet on Saturn: ok; skipped: moon: low)" in out4,
          "a pass leads a skip on one item, and both are listed")
    col = [{"group": "planet", "target": "Saturn", "status": "collected", "detail": "4 ladder recordings", "items": [47], "measures": []},
           {"group": "tracking", "target": "Saturn", "status": "passed", "detail": "lock held", "items": [47], "measures": []}]
    out5, _, _ = update(SAMPLE, _res(col))
    check("**Sky run 20261121: COLLECTED, your eye owed** (collected: planet on Saturn: 4 ladder recordings; passed: tracking" in out5,
          "collected leads a pass on one item: your eye is owed")
    check("| 47 | COLLECTED |" in out5, "and reads COLLECTED in the table")
    per = [{"group": "planet", "target": "Saturn", "status": "collected", "detail": "throughput and ladder",
            "items": [1, 47], "item_status": {"1": "passed", "47": "collected"}, "measures": []}]
    out6, _, _ = update(SAMPLE, _res(per))
    check("| 1 | PASSED |" in out6 and "| 47 | COLLECTED |" in out6,
          "156.2b: each item takes its own status, not the group's")
    try:
        update(SAMPLE, _res(tests, emulator=True))
        check(False, "an emulator run is refused")
    except ValueError as e:
        check("emulator" in str(e), "an emulator run is refused")
    try:
        update(SAMPLE, {"format": "other"})
        check(False, "a file that is not results.json is refused")
    except ValueError:
        check(True, "a file that is not results.json is refused")
    print("\n" + ("ALL CHECKS PASSED" if not fails else f"{len(fails)} CHECK(S) FAILED"))
    return 1 if fails else 0


def main(argv):
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("results", nargs="?")
    ap.add_argument("--checklist", default="docs/CLEAR-NIGHT-CHECKLIST.md")
    ap.add_argument("--self-test", action="store_true")
    a = ap.parse_args(argv)
    if a.self_test:
        return self_test()
    if not a.results:
        ap.error("give the night's results.json")
    with open(a.results, encoding="utf-8") as fh:
        results = json.load(fh)
    with open(a.checklist, encoding="utf-8") as fh:
        text = fh.read()
    try:
        out, touched, missing = update(text, results)
    except ValueError as e:
        print(f"not marked: {e}", file=sys.stderr)
        return 2
    with open(a.checklist, "w", encoding="utf-8", newline="\n") as fh:
        fh.write(out)
    print(f"marked {len(touched)} item(s) in {a.checklist}" + (f"; not in the checklist: {missing}" if missing else ""))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
