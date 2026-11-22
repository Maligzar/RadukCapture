#!/usr/bin/env python3
"""Phase 156.4: hand a sky run's results to Claude Code.

Three kinds of night, the same way (the owner, 2026-11-21):

  A sky run (the folder holds the app's results.json):
    python tools/sky_handoff.py <data root>/field-results/<night> [options]
  A normal imaging night, or one with Dash's Playwright suite run (--dash):
    python tools/sky_handoff.py <data root>/field-results/<night> --night <YYYYMMDD>
        --captures <capture folder> [--dash tests/web/results/dash-results.json] [options]

  A troubleshooting report (the app's REPORT button writes the folder):
    python tools/sky_handoff.py <data root>/reports/<stamp> [--logs <logs folder>] [--rig <rig.ini>]

  options: [--log <raduk_log.txt>] [--logs <logs folder>] [--rig <rig.ini>]
           [--no-claude] [--dry-run]

What it does, in order:
  1. Builds the bundle beside results.json (tools/sky_bundle.py).
  2. Refuses to go further for an emulator sky run: only the sky settles a
     checklist item. An imaging or Dash night marks no checklist item at
     all; Claude reads its evidence and says what it shows.
  3. In a separate git worktree (your own checkout and its local edits are
     never touched), makes the branch field/<night> from origin/master,
     copies the bundle into field-results/<night>/, marks the checklist
     (tools/sky_checklist_update.py), and commits both. The checklist is
     marked whatever happens next.
  4. Pushes the branch, unless rig.ini says Offline=1 (rule 7), in which case
     it stops with the commit local and says the analysis is owed.
  5. Starts Claude Code headless in the worktree with
     tools/sky_handoff_prompt.md. Claude writes findings.md, may propose
     register changes and small fixes, pushes, and opens a draft pull
     request. It is never allowed to merge: `gh pr merge` and pushes to
     master are outside the tools it is given.
  6. If Claude Code is missing or fails, the bundle and the marked checklist
     are already pushed, and it says so.

The flags were checked against Claude Code 2.1.330's --help: -p (print
mode), --permission-mode acceptEdits, --allowedTools.

  python tools/sky_handoff.py --self-test
"""
import json
import os
import shutil
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import sky_bundle  # noqa: E402

ALLOWED_TOOLS = ("Read Edit Write Grep Glob "
                 "Bash(git status*) Bash(git diff*) Bash(git log*) Bash(git add*) Bash(git commit*) "
                 "Bash(git push) Bash(git push origin field/*) Bash(gh pr create*) Bash(python *)")
BUNDLE_FILES = ("bundle.json", "results.json", "summary.md", "log_excerpt.txt", "recordings.txt", "dash-results.json")


def git(repo, *args, check=True):
    r = subprocess.run(["git", "-C", repo, *args], capture_output=True, text=True)
    if check and r.returncode != 0:
        raise RuntimeError(f"git {' '.join(args)}: {r.stderr.strip() or r.stdout.strip()}")
    return r.stdout.strip()


def offline(rig_path):
    if not rig_path or not os.path.isfile(rig_path):
        return False
    with open(rig_path, encoding="utf-8", errors="replace") as f:
        for line in f:
            k, _, v = line.partition("=")
            if k.strip().lower() == "offline" and v.strip() not in ("", "0"):
                return True
    return False


def handoff(night_dir, repo, log=None, logs=None, rig=None, use_claude=True, dry_run=False, claude="claude",
            say=print, work_root=None, night=None, captures=None, dash=None):
    """Returns 'emulator', 'offline', 'pushed' (Claude missing or failed), 'handed-off' or 'dry-run'."""
    results = os.path.join(night_dir, "results.json")
    stamp = ""
    if os.path.isfile(os.path.join(night_dir, "report.json")):
        # Phase 171: the troubleshooting button's report.
        stamp, written = sky_bundle.bundle_report(night_dir)
        with open(os.path.join(night_dir, "bundle.json"), encoding="utf-8") as f:
            night = json.load(f).get("night", "") or night or ""
        kind = "report"
        if logs:
            for name in sorted(os.listdir(logs)) if os.path.isdir(logs) else []:
                if night and name.startswith(f"validation_{night}_") and name.endswith(".json"):
                    shutil.copyfile(os.path.join(logs, name), os.path.join(night_dir, name))
                    written.append(name)
    elif os.path.isfile(results):
        with open(results, encoding="utf-8") as f:
            res = json.load(f)
        night = res.get("night") or os.path.basename(os.path.normpath(night_dir))
        kind = "sky-run"
        written = sky_bundle.bundle(results, log, logs, captures, dash)
        if res.get("emulator"):
            say(f"[HANDOFF] bundle: {', '.join(written)}")
            say("[HANDOFF] an emulator run: bundled, but no branch, no checklist marks and no analysis")
            return "emulator"
    else:
        night = night or os.path.basename(os.path.normpath(night_dir))
        out_root = os.path.dirname(os.path.normpath(night_dir))
        night_dir, written = sky_bundle.bundle_night(night, out_root, log, logs, captures, dash)
        kind = "dash-test" if dash else "imaging"
    say(f"[HANDOFF] {kind} {night}: bundle: {', '.join(written)}")
    # One branch per night and kind; a rerun builds on what it pushed before.
    branch = (f"report/{stamp}" if kind == "report" else
              f"field/{night}" + {"sky-run": "", "imaging": "-imaging", "dash-test": "-dash"}[kind])
    if dry_run:
        say(f"[HANDOFF] dry run: would make {branch} from origin/master, commit the bundle and the checklist, push, and start Claude Code")
        return "dry-run"

    git(repo, "fetch", "origin", "master")
    again = subprocess.run(["git", "-C", repo, "fetch", "origin", f"{branch}:refs/remotes/origin/{branch}"],
                           capture_output=True, text=True).returncode == 0
    base = f"origin/{branch}" if again else "origin/master"
    work = os.path.join(work_root or tempfile.mkdtemp(prefix="sky-handoff-"), branch.replace("/", "-"))
    if os.path.exists(work):
        git(repo, "worktree", "remove", "--force", work, check=False)
        shutil.rmtree(work, ignore_errors=True)
    git(repo, "worktree", "prune", check=False)
    git(repo, "worktree", "add", "-B", branch, work, base)
    if again:
        say(f"[HANDOFF] {branch} was pushed before: this run adds to it")
    dest = os.path.join(work, "field-results", "reports", stamp) if kind == "report" else \
        os.path.join(work, "field-results", night)
    os.makedirs(dest, exist_ok=True)
    if kind == "report":
        # The screenshots stay in the worktree for Claude to look at, and out
        # of every commit (its own too), so the repository does not grow by images.
        with open(os.path.join(dest, ".gitignore"), "w", encoding="utf-8") as f:
            f.write("*.png\n")
    for name in os.listdir(night_dir):
        if kind == "report" or name in BUNDLE_FILES or \
                (name.startswith(("validation_", "autorun_")) and name.endswith((".json", ".txt"))):
            shutil.copyfile(os.path.join(night_dir, name), os.path.join(dest, name))
    if kind == "sky-run":
        upd = subprocess.run([sys.executable, os.path.join(work, "tools", "sky_checklist_update.py"),
                              os.path.join(dest, "results.json"),
                              "--checklist", os.path.join(work, "docs", "CLEAR-NIGHT-CHECKLIST.md")],
                             capture_output=True, text=True)
        say("[HANDOFF] checklist: " + (upd.stdout.strip().splitlines() or ["(no output)"])[-1])
        msg = f"Sky run {night}: the bundle, and the checklist marked from it"
    else:
        say("[HANDOFF] checklist: not marked (only a sky run's results mark it)")
        msg = ({"dash-test": "Dash test night", "report": "Problem report"}.get(kind, "Imaging night") +
               f" {stamp or night}: the bundle")
    git(work, "add", "-A")

    if not git(work, "status", "--porcelain"):
        say("[HANDOFF] nothing new to commit: the bundle is as it was")
    else:
        git(work, "commit", "-m", msg)
    say(f"[HANDOFF] committed on {branch} in {work}")

    if offline(rig):
        say("[HANDOFF] rig.ini Offline=1: the commit stays local, nothing pushed; the analysis is owed")
        return "offline"
    git(work, "push", "-u", "origin", branch)
    say(f"[HANDOFF] pushed {branch}")

    if not use_claude or not shutil.which(claude):
        say("[HANDOFF] Claude Code not run (" + ("--no-claude" if not use_claude else "not installed") +
            "): the bundle is pushed" + (" with the checklist marked" if kind == "sky-run" else "") + "; the analysis is owed")
        return "pushed"
    # One line: on Windows `claude` is an npm .cmd shim, and cmd.exe ends an
    # argument at its first newline, so the prompt itself stays in the file.
    if kind == "report":
        prompt = (f"Read tools/sky_handoff_prompt.md and do what it says for the problem report {stamp}: "
                  f"its folder is field-results/reports/{stamp}, and {{NIGHT}} in it means {night}.")
    else:
        prompt = (f"Read tools/sky_handoff_prompt.md and do what it says for the night {night}, a {kind} night: "
                  f"{{NIGHT}} in it means {night}.")
    r = subprocess.run([shutil.which(claude), "-p", prompt, "--permission-mode", "acceptEdits",
                        "--allowedTools", ALLOWED_TOOLS], cwd=work, capture_output=True, text=True)
    with open(os.path.join(night_dir, "handoff.log"), "w", encoding="utf-8") as f:
        f.write(r.stdout + ("\n[stderr]\n" + r.stderr if r.stderr else ""))
    if r.returncode != 0:
        say(f"[HANDOFF] Claude Code failed (exit {r.returncode}): see handoff.log; the bundle is pushed, the analysis is owed")
        return "pushed"
    say("[HANDOFF] Claude Code finished: its findings and draft pull request are on " + branch)
    return "handed-off"


def self_test():
    fails = []

    def ok(cond, what, detail=""):
        print(f"  [{'PASS' if cond else 'FAIL'}] {what}" + (f"  {detail}" if detail else ""))
        if not cond:
            fails.append(what)

    repo_root = os.path.normpath(os.path.join(HERE, ".."))
    with tempfile.TemporaryDirectory() as d:
        # A bare "origin" with master holding the tools and the checklist.
        origin = os.path.join(d, "origin.git")
        seed = os.path.join(d, "seed")
        subprocess.run(["git", "init", "-q", "--bare", "-b", "master", origin], check=True)
        subprocess.run(["git", "init", "-q", "-b", "master", seed], check=True)
        for rel in ("tools/sky_checklist_update.py", "tools/sky_handoff_prompt.md", "docs/CLEAR-NIGHT-CHECKLIST.md"):
            os.makedirs(os.path.join(seed, os.path.dirname(rel)), exist_ok=True)
            shutil.copyfile(os.path.join(repo_root, rel), os.path.join(seed, rel))
        env_git = ["-c", "user.name=t", "-c", "user.email=t@example.invalid"]
        subprocess.run(["git", "-C", seed, *env_git, "add", "-A"], check=True)
        subprocess.run(["git", "-C", seed, *env_git, "commit", "-qm", "seed"], check=True)
        subprocess.run(["git", "-C", seed, "remote", "add", "origin", origin], check=True)
        subprocess.run(["git", "-C", seed, "push", "-q", "origin", "master"], check=True)
        # The owner's checkout, with a local edit that must survive.
        clone = os.path.join(d, "clone")
        subprocess.run(["git", "clone", "-q", origin, clone], check=True)
        for k, v in (("user.name", "t"), ("user.email", "t@example.invalid")):
            subprocess.run(["git", "-C", clone, "config", k, v], check=True)
        with open(os.path.join(clone, "docs", "CLEAR-NIGHT-CHECKLIST.md"), "a") as f:
            f.write("\nlocal edit\n")

        def night_dir(emulator=False):
            nd = os.path.join(d, "data", "field-results", "20261121")
            os.makedirs(nd, exist_ok=True)
            res = {"format": "raduk-sky-results", "version": 1, "night": "20261121", "app": "3.3.42",
                   "emulator": emulator, "stopped": "the end of the list", "parked": True,
                   "tests": [{"group": "tracking", "target": "Saturn", "status": "passed", "detail": "lock held",
                              "items": [48, 49, 50], "item_status": {}, "measures": []}]}
            with open(os.path.join(nd, "results.json"), "w") as f:
                json.dump(res, f)
            return nd

        said = []
        r = handoff(night_dir(emulator=True), clone, say=said.append, use_claude=False, work_root=d)
        ok(r == "emulator" and not git(clone, "branch", "-a", "--list", "*field*", check=False),
           "an emulator run: bundled, no branch made")

        rig = os.path.join(d, "rig.ini")
        with open(rig, "w") as f:
            f.write("[Rig]\nOffline=1\n")
        said = []
        r = handoff(night_dir(), clone, rig=rig, say=said.append, use_claude=False, work_root=d)
        ok(r == "offline", "Offline=1: committed locally, nothing pushed", said[-1])
        remote_branches = subprocess.run(["git", "-C", origin, "branch", "--list", "field/*"], capture_output=True, text=True).stdout
        ok("field/20261121" not in remote_branches, "and the remote has no field branch")

        said = []
        r = handoff(night_dir(), clone, say=said.append, use_claude=False, work_root=d)
        ok(r == "pushed", "online, no Claude: pushed, the analysis owed", said[-1])
        remote_branches = subprocess.run(["git", "-C", origin, "branch", "--list", "field/*"], capture_output=True, text=True).stdout
        ok("field/20261121" in remote_branches, "the branch is on the remote")
        files = subprocess.run(["git", "-C", origin, "ls-tree", "-r", "--name-only", "field/20261121"],
                               capture_output=True, text=True).stdout.split()
        ok("field-results/20261121/results.json" in files and "field-results/20261121/summary.md" in files,
           "the bundle is committed on it")
        chk = subprocess.run(["git", "-C", origin, "show", "field/20261121:docs/CLEAR-NIGHT-CHECKLIST.md"],
                             capture_output=True, text=True).stdout
        ok("Sky run 20261121" in chk, "and the checklist is marked on it")
        local = open(os.path.join(clone, "docs", "CLEAR-NIGHT-CHECKLIST.md")).read()
        ok(local.endswith("local edit\n") and "Sky run 20261121" not in local,
           "the owner's own checkout and its local edit are untouched")

        # A stand-in for Claude Code that records how it was called and fails.
        # Python behind a .cmd on Windows (as npm installs claude), a shebang elsewhere.
        bindir = os.path.join(d, "bin")
        os.makedirs(bindir)
        py = os.path.join(bindir, "claude_stub.py")
        with open(py, "w") as f:
            f.write("import os, sys\n"
                    "open(os.path.join(os.getcwd(), '..', 'claude_args.txt'), 'w').write('\\n'.join(sys.argv[1:]))\n"
                    "sys.exit(3)\n")
        if os.name == "nt":
            stub = os.path.join(bindir, "claude.cmd")
            with open(stub, "w") as f:
                f.write(f'@"{sys.executable}" "{py}" %*\r\n')
        else:
            stub = os.path.join(bindir, "claude")
            with open(stub, "w") as f:
                f.write(f"#!{sys.executable}\n" + open(py).read())
            os.chmod(stub, 0o755)
        said = []
        r = handoff(night_dir(), clone, say=said.append, claude=stub, work_root=d)
        ok(r == "pushed" and "failed (exit 3)" in said[-1], "Claude Code failing: still pushed, and said so", said[-1])
        args = open(os.path.join(d, "claude_args.txt")).read()
        ok("-p" in args and "acceptEdits" in args and "sky_handoff_prompt.md" in args and "20261121" in args,
           "Claude Code is started headless, pointed at the prompt for this night")
        ok(len(args.split("\n")) == 6,
           "six arguments: the prompt is one line (a Windows .cmd shim would cut it at a newline)", str(len(args.split("\n"))))
        ok("gh pr merge" not in args and "Bash(git push origin field/*)" in args and "push origin master" not in args,
           "its tools cannot merge or push to master")
        ok(os.path.isfile(os.path.join(night_dir(), "handoff.log")), "its output is kept in handoff.log")
        ok(handoff(night_dir(), clone, dry_run=True, say=said.append) == "dry-run", "--dry-run changes nothing")

        # A normal imaging night: no results.json, so no checklist marks.
        img = os.path.join(d, "data", "field-results", "20261122")
        logp = os.path.join(d, "raduk_log.txt")
        with open(logp, "w") as f:
            f.write("[ACQ] PLANET LOCK: locked\n[STACK] recording complete: normal=1  autoStack=0  path=D:\\c\\Saturn.ser\n")
        caps = os.path.join(d, "caps")
        os.makedirs(caps)
        with open(os.path.join(caps, "autorun_20261122_230000.txt"), "w") as f:
            f.write("ranked")
        said = []
        r = handoff(img, clone, log=logp, captures=caps, night="20261122", say=said.append, use_claude=False, work_root=d)
        ok(r == "pushed", "an imaging night: bundled and pushed", said[-1] if said else "")
        files = subprocess.run(["git", "-C", origin, "ls-tree", "-r", "--name-only", "field/20261122-imaging"],
                               capture_output=True, text=True).stdout.split()
        ok("field-results/20261122/bundle.json" in files and "field-results/20261122/autorun_20261122_230000.txt" in files,
           "on its own branch, field/<night>-imaging, with Autorun's rankings")
        chk = subprocess.run(["git", "-C", origin, "show", "field/20261122-imaging:docs/CLEAR-NIGHT-CHECKLIST.md"],
                             capture_output=True, text=True).stdout
        ok("Sky run 20261122" not in chk, "and no checklist item marked")
        said = []
        r = handoff(img, clone, log=logp, captures=caps, night="20261122", say=said.append, use_claude=False, work_root=d)
        ok(r == "pushed" and any("pushed before" in x for x in said), "run again: it builds on the branch it pushed, no rejected push")

        # Phase 171: a troubleshooting report from the app's REPORT button.
        rep = os.path.join(d, "data", "reports", "20261122_021500")
        os.makedirs(rep)
        with open(os.path.join(rep, "report.json"), "w") as f:
            json.dump({"format": "raduk-problem-report", "version": 1, "stamp": "20261122_021500",
                       "night": "20261121", "app": "3.3.44", "description": "Autofocus took Saturn to a donut",
                       "state": {"target": "Saturn"}, "files": ["log_tail.txt", "screen_main.png"]}, f)
        with open(os.path.join(rep, "log_tail.txt"), "w") as f:
            f.write("[AF] Fine pass\n")
        with open(os.path.join(rep, "screen_main.png"), "wb") as f:
            f.write(b"\x89PNG fake")
        said = []
        r = handoff(rep, clone, say=said.append, use_claude=False, work_root=d)
        ok(r == "pushed", "a report: bundled and pushed", said[-1] if said else "")
        files = subprocess.run(["git", "-C", origin, "ls-tree", "-r", "--name-only", "report/20261122_021500"],
                               capture_output=True, text=True).stdout.split()
        ok("field-results/reports/20261122_021500/report.json" in files and
           "field-results/reports/20261122_021500/summary.md" in files, "on its own branch, report/<stamp>")
        ok(not any(f.endswith(".png") for f in files), "the screenshots are not committed")
        ok(os.path.isfile(os.path.join(d, "report-20261122_021500", "field-results", "reports", "20261122_021500",
                                       "screen_main.png")), "but they are in the worktree for Claude to look at")
    print("\n" + ("SOME CHECKS FAILED" if fails else "ALL CHECKS PASSED"))
    return 1 if fails else 0


def main(argv):
    if "--self-test" in argv:
        return self_test()
    if not argv or argv[0].startswith("-"):
        print(__doc__)
        return 2

    def opt(name):
        return argv[argv.index(name) + 1] if name in argv else None
    repo = os.path.normpath(os.path.join(HERE, ".."))
    r = handoff(argv[0], repo, log=opt("--log"), logs=opt("--logs"), rig=opt("--rig"),
                use_claude="--no-claude" not in argv, dry_run="--dry-run" in argv,
                night=opt("--night"), captures=opt("--captures"), dash=opt("--dash"))
    return 0 if r in ("handed-off", "pushed", "offline", "emulator", "dry-run") else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
