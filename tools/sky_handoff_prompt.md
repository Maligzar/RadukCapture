# Sky run {NIGHT}: read the bundle and report

You are Claude Code, started headless by `tools/sky_handoff.py` after an unattended sky run (Phase 156.4). Your working directory is a clean worktree of RadukCapture on the branch `field/{NIGHT}`. The bundle is already committed on that branch, along with the checklist marks `tools/sky_checklist_update.py` made from it.

## What to read
- `field-results/{NIGHT}/summary.md`: each test, its status and why, each checklist item's own status, and the measures.
- `field-results/{NIGHT}/results.json`: the same data, as written by the app.
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
- **Commit on `field/{NIGHT}` and push it.** Then open a **draft** pull request against `master`, with `findings.md` as its body: run `gh pr create --draft --base master --title "Sky run {NIGHT}: findings" --body-file field-results/{NIGHT}/findings.md` if `gh` is available. Otherwise push, and say in your last message that the pull request still needs opening.
- **Never merge, approve, or push to `master`.** The owner reviews and merges.
- If the bundle is empty or the run never started, write that in `findings.md`, push it, and stop.
