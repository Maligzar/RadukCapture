# Night {NIGHT}: read the bundle and report

You are Claude Code, started headless by `tools/sky_handoff.py` after a night at the telescope (Phase 156.4). Your working directory is a clean worktree of RadukCapture, on the branch the bundle was committed to. `field-results/{NIGHT}/bundle.json` says which kind of night it was:

- **`sky-run`:** an unattended sky run. `results.json` holds each test's status, and the checklist has already been marked from it by `tools/sky_checklist_update.py`.
- **`imaging`:** a normal night of imaging. There is no `results.json` and no checklist mark. The evidence is the recordings, Autorun's rankings (`autorun_*.txt`) and the validation log. Say what that evidence shows about the clear-night checklist's items and the UNVERIFIED values. Don't mark any item: only a sky run's machine-checked results do that.
- **`dash-test`:** as imaging, plus Dash's Playwright report (`dash-results.json`; the failures are listed in `summary.md`). For each failure, find whether Dash, the app or the test is wrong, and fix it if the evidence is clear. Dash is the remote, so a Dash failure that hides an app fault is the app's to fix.

The recordings aren't in the bundle, but this is the capture PC, so the paths in `recordings.txt` and each recording's `.json` observation record beside it can be read.

## What to read
- `field-results/{NIGHT}/summary.md`: each test, its status and why, each checklist item's own status, and the measures.
- `field-results/{NIGHT}/results.json` (a sky run only): the same data, as written by the app.
- `field-results/{NIGHT}/autorun_*.txt`: Autorun's runs, ranked best first, with what was on Jupiter's disc.
- `field-results/{NIGHT}/dash-results.json` (a Dash night): Playwright's report.
- `field-results/{NIGHT}/log_excerpt.txt`: the run's log lines.
- `field-results/{NIGHT}/validation_*.json`: the night's validation log (sharpness, focus runs, exposure). `python tools/validation_report.py field-results/{NIGHT}` summarises it.
- `field-results/{NIGHT}/recordings.txt`: the recordings' paths on the capture PC. The recordings themselves are not here.
- `docs/UNVERIFIED.md` and `docs/CLEAR-NIGHT-CHECKLIST.md`: what each item and each UNVERIFIED value is waiting for.

## What to write
1. **`field-results/{NIGHT}/findings.md`.**
   - For each test that ran: what it measured, against what was expected, and what that means for its checklist items and UNVERIFIED tags.
   - Say plainly what the night did not settle.
   - Label anything you infer as inferred.
2. **Proposed register changes,** only where the night's evidence confirms or replaces a value.
   - Make each change in `docs/UNVERIFIED.md`, citing the measure and the night.
   - Never remove an item or a tag on a COLLECTED result: that status means the owner's eye is still owed.
3. **Small fixes,** only where the evidence clearly supports one, with a test.
   - For example, a threshold the night measured as wrong.
   - Anything larger goes in `findings.md` as a proposal, not a change.

## Rules
- The app is the instrument; Dash is a remote and a test harness (the owner's rule, 2026-11-21). Don't propose anything that makes Dash do what the app can't.
- No camera model, resolution or plate scale in code (Phase 149).
- Nothing may need the internet (rule 7).
- Every pull request bumps the PATCH in `src/Version.h` and adds its row to the Phase History in `RadukCapture-Help.md`. Do both only if you change code.
- **Commit on the branch you are on, and push it.** Then open a **draft** pull request against `master`, with `findings.md` as its body: run `gh pr create --draft --base master --title "Night {NIGHT}: findings" --body-file field-results/{NIGHT}/findings.md` if `gh` is available. Otherwise push, and say in your last message that the pull request still needs opening.
- **Never merge, approve, or push to `master`.** The owner reviews and merges.
- If the bundle is empty or the run never started, write that in `findings.md`, push it, and stop.
