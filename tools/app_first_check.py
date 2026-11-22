#!/usr/bin/env python3
"""Phase 166.0: the app first, Dash second (the owner's rule, 2026-11-21).

Dash is a remote for an iPhone or iPad and a test harness; it is never used
in place of the app. So every command Dash can send must have a control in
the app itself. This check reads every remote command main.cpp accepts
(`rc.cmd == "..."`) and the parity table in docs/Phase166-Plan-App-First.md,
and fails when:

  - a remote command has no row in the table (a new command must say where
    its native control is);
  - a row names a command the app no longer accepts (the table is stale);
  - a row is `owed` but its stage is already marked WRITTEN in
    docs/PHASE-STATUS.md (the control was promised by a stage that is done).

Usage:
  python tools/app_first_check.py            # check the repository
  python tools/app_first_check.py --self-test
"""
import os
import re
import sys

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
MAIN = os.path.join(ROOT, "src", "main.cpp")
PLAN = os.path.join(ROOT, "docs", "Phase166-Plan-App-First.md")
STATUS = os.path.join(ROOT, "docs", "PHASE-STATUS.md")

CMD_RE = re.compile(r'rc\.cmd\s*==\s*"([A-Za-z0-9_]+)"')


def commands(main_text):
    return sorted(set(CMD_RE.findall(main_text)))


def table(plan_text):
    """command -> (native, stage) from the plan's parity table."""
    rows = {}
    if "## Parity table" not in plan_text:
        raise ValueError("the plan has no '## Parity table' section")
    body = plan_text.split("## Parity table", 1)[1]
    for line in body.splitlines():
        cells = [c.strip() for c in line.strip().strip("|").split("|")]
        if len(cells) != 3 or not line.strip().startswith("|"):
            continue
        cmd, native, stage = cells
        if cmd in ("command", "") or set(cmd) <= set("-: "):
            continue
        rows[cmd] = (native, stage)
    return rows


def written_stages(status_text):
    """Stage numbers PHASE-STATUS marks WRITTEN, e.g. '156.2c'."""
    return set(re.findall(r"(\d{2,3}\.\d+[a-z]?)\s*`WRITTEN`", status_text))


def check(main_text, plan_text, status_text):
    problems, owed = [], []
    cmds = commands(main_text)
    rows = table(plan_text)
    done = written_stages(status_text)
    for c in cmds:
        if c not in rows:
            problems.append(f'remote command "{c}" has no row in the parity table: say where its control in the app is')
            continue
        native, stage = rows[c]
        if native.lower() == "owed":
            owed.append((c, stage))
            if stage in done:
                problems.append(f'"{c}" is still owed, but stage {stage} is marked WRITTEN: its control in the app was due there')
        elif not native:
            problems.append(f'"{c}": the native control is empty')
    for c in rows:
        if c not in cmds:
            problems.append(f'the parity table lists "{c}", which the app no longer accepts')
    return problems, owed


def main_check():
    with open(MAIN, encoding="utf-8", errors="replace") as f:
        m = f.read()
    with open(PLAN, encoding="utf-8") as f:
        p = f.read()
    with open(STATUS, encoding="utf-8") as f:
        s = f.read()
    problems, owed = check(m, p, s)
    n = len(commands(m))
    for c, st in owed:
        print(f"  owed: {c} (native control due in {st})")
    if problems:
        for x in problems:
            print("  FAIL: " + x)
        print(f"app_first_check: {len(problems)} problem(s) across {n} remote commands")
        return 1
    print(f"app_first_check: {n} remote commands, each with a control in the app or owed by an open stage ({len(owed)} owed)")
    return 0


def self_test():
    fails = []

    def ok(cond, what):
        print(f"  [{'PASS' if cond else 'FAIL'}] {what}")
        if not cond:
            fails.append(what)

    main = 'if (rc.cmd == "jog") {} else if (rc.cmd == "park") {} else if (rc.cmd=="skyrun_start") {}'
    plan = """# x

## Parity table (read by 166.0's check)

| command | native | stage |
|---|---|---|
| jog | mount window D-pad | done |
| park | PARK | done |
| skyrun_start | owed | 156.2c |
"""
    ok(commands(main) == ["jog", "park", "skyrun_start"], "reads every rc.cmd, spacing aside")
    p, o = check(main, plan, "")
    ok(p == [] and o == [("skyrun_start", "156.2c")], "a full table passes; an owed row is listed")
    p, _ = check(main + ' rc.cmd == "new_thing"', plan, "")
    ok(any("new_thing" in x and "no row" in x for x in p), "a new remote command without a row fails")
    p, _ = check(main.replace('rc.cmd == "jog"', 'rc.cmd == "x"'), plan + "| x | X button | done |\n", "")
    ok(any('"jog", which the app no longer accepts' in x for x in p), "a row for a command that is gone fails")
    p, _ = check(main, plan, "| 156 | ... **156.2c `WRITTEN` (3.3.41)** ... |")
    ok(any("still owed" in x for x in p), "owed past its stage being written fails")
    ok(written_stages("**166.0 `WRITTEN` (3.3.41)** and 155.4a `WRITTEN` x") == {"166.0", "155.4a"}, "stage numbers read from PHASE-STATUS")
    try:
        table("no table here")
        ok(False, "a plan without the table is refused")
    except ValueError:
        ok(True, "a plan without the table is refused")
    print("\n" + ("SOME CHECKS FAILED" if fails else "ALL CHECKS PASSED"))
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(self_test() if "--self-test" in sys.argv else main_check())
