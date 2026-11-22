# Phase 174: a focus history that learns

Written 2026-11-22 at the owner's request, imaging through cloud: "a big learning opportunity for my autofocus so I want to make sure that I capture the meta data in a way that I can build a history of focusing so that my autofocuser starts to learn ... maybe we even build a skill, an MCP server, and allow an LLM or SLM to get involved with the auto focusing." Status: 174.0 `WRITTEN` (3.3.54); 174.2 `WRITTEN` (3.3.56); 174.1 `WRITTEN` (3.3.58); 174.3 `WRITTEN` (3.3.59).

## The principle
**The model reads the history and proposes; the deterministic routine focuses; replay judges every proposal against real sweeps before it reaches the sky.**

A language model in the focus loop itself would be slow, would not answer the same way twice, and would read pixels no better than the metrics already do. Where it is good:
- reading hundreds of sweeps for patterns;
- explaining a failure;
- proposing a rule, which replay then tests.

The one live decision it could own is **when** to refocus (from temperature, time and the sharpness trend), which is slow, low-risk and explainable. That is 174.3's optional stage, behind the owner's approval.

## 174.0: record everything, as data (`WRITTEN` 3.3.54)
`<logs>\focus_history.jsonl` holds one JSON line per event (`core/FocusHistory.h`, gated by `focus_history`). It is in the night bundle.

**`"kind":"sweep"`** records:
- **The conditions just before the sweep:** target, filter, profile, exposure, gain, frame, bit depth, focuser and sensor temperatures, altitude, seeing and live sharpness.
- **The result:** start, result, success, message, and backlash measured and in use.
- **The coarse pass:** each position's score **going out and coming back** and the planet's **peak brightness at each step**. A fade shows in the peak before the score: on 2026-11-22 it went from about 200 to about 55. Also the coarse peak and the out/back disagreement.
- **The fine pass:** positions, scores, frame spread and the fade check.
- **The same-sky A/B:** start before, start now, fit, both edge scores and the verdict.
- **Whether the sky changed.**

**`"kind":"followup"`** records the 15 minutes after a sweep:
- each place the owner brought the focuser to rest by hand (still for 5 s), with the best sharpness read there;
- where it settled, and how far that was from autofocus's result.

A new sweep or the app closing ends it. Leaving the result alone is recorded too. The settled position is the ground truth. On 2026-11-22 it was 168,893 against autofocus's 165,984.

## 174.1: a temperature model (`WRITTEN` 3.3.58, first half)
**What is written.**
- `core/FocusTempModel.h` joins each sweep to its followup and makes a point: the focuser's temperature and where the owner settled. A sweep with no followup or no temperature gives no point, so settled focus is never guessed.
- The fit is per target and filter. The slope is the median of pairwise slopes, taken only across at least 0.5 C. The intercept is the median of the points' intercepts, and the scatter is 1.4826 x the median absolute residual. One bad sweep does not bend it.
- Before anything may act on the fit, it needs **6 points, 2 nights and 2 C of range**. Below that it still fits and reports what it lacks.
- `RefocusSuggested(fit, tempAtFocus, tempNow, zoneSteps)` gives true only for a confident fit and a known zone.
- `focus_replay <history> --temp` prints the fit for every target and filter.
- The app logs `[FOCUSTEMP]` beside every sweep: the model's prediction against the sweep's result. That puts the evidence into the history before the model is used.
- Gated by `focus_temp_model`: a -120 steps/C fit over three nights with an outlier, the one-night case, missing data, flat temperature, separate keys, and the night boundary.

**Still to do.** Start autofocus at the predicted position with a narrower sweep, and raise "refocus suggested". Both wait for the model to be confident on real nights and for a critical focus zone in steps (no code holds one yet).

### The original plan
1. **The fit.** Fit settled focus against focuser temperature, per target and filter, robustly: the median slope over pairs (Theil-Sen), with its spread.
2. **Using it.** Autofocus starts at the predicted position with a narrower sweep when the model is confident. A temperature change worth more than the critical focus zone (Phase 161's focus calibration) raises "refocus suggested".
3. **The amount of data needed.** It reports how many sweeps and nights it rests on, and acts only above a minimum.

## 174.2: replay (`WRITTEN` 3.3.56)
- **`core/FocusReplay.h`** reads `focus_history.jsonl`. It includes a small JSON reader, so it has no dependencies. For each sweep it re-runs the decisions that can be repeated from what was recorded:
  - the coarse fit (`FitDirections` on the recorded passes);
  - the same-sky A/B (`SweepSpoiledByFade`, `KeepAfterAB`).

  The fine pass cannot be re-run without its frames, so a replayed result is the recorded one or the start, as the A/B now decides. Each sweep with a followup is scored as recorded and replayed distance from where the owner settled. `Regressions()` lists any sweep that ends more than 50 steps further away.
- **`tools/focus_replay.exe <logs>\focus_history.jsonl`** prints the table and exits 1 on a regression. It is built beside the app.
- **`tools/af_log_to_history.py`** turns a `raduk_log` from before 3.3.54 into history lines. The night and the settled positions are given on the command line (`--night 20261121 --settled 23:10=168893`); a log carries neither.
- **Gated by `focus_replay`**, which holds two sweeps:
  - **The night of 2026-11-22.** The recorded result was 2,909 steps from where the owner settled; replayed today, the start stands, 228 steps away.
  - **CI's case 3.** The fit stays.

  It also checks that a wrong rule is caught as a regression. The converter's self-test runs in CI.

## 174.3: an MCP server for focus history (`WRITTEN` 3.3.59)
**What is written.** `tools/focus_mcp.py`: newline-delimited JSON-RPC on stdin/stdout, standard library only.
- **`list_sweeps(night?, target?)`, `get_sweep(id)`.** Each sweep comes with where the owner settled.
- **`failure_summary`.** Every failed or hand-corrected sweep (moved 200 or more steps) is classified from the record:
  - fade (sky changed, or the start stood);
  - plateau (no peak, by `PassHasPeak`'s 1.15x rule);
  - edge (best at an end);
  - backlash (measured against in use);
  - otherwise unclassified.

  Donut and seeing are not in the record yet, so they are not guessed.
- **`temperature_model(target?, filter?)`.** The same Theil-Sen fit and minimums as `core/FocusTempModel.h`.
- **`replay()`.** Runs the built `focus_replay` over the history. A rule change is a code change: rebuild, then replay. No free-form `rule_change` argument is accepted, because one could not be checked.
- **`propose(change, why)`.** The only writer. It appends to `focus_proposals.md` beside the history with `Status: proposed`.
- **There is no tool that moves the focuser or changes a setting.**

**Connecting it:**
```
claude mcp add focus-history -- python tools/focus_mcp.py --history <logs>\focus_history.jsonl
```

**The self-test runs in CI.** It covers every tool on the night's record, and the real stdio loop end to end.

### The original plan
`tools/focus_mcp.py` is a local MCP server, standard library only, needing nothing online (rule 7).

**Read-only tools:**
- `list_sweeps(night?, target?)`
- `get_sweep(id)`
- `failure_summary`, which classifies each failed or corrected sweep as fade, plateau (no peak), donut, backlash, edge of sweep or seeing
- `temperature_model(target, filter)`
- `replay(rule_change)`

**The one writing tool:** `propose(change, why)` appends to `focus_proposals.md` for the owner to accept. Nothing moves the focuser or changes a setting by itself.

It connects to Claude Code, or to Leoben's existing MCP tool loop and local Ollama model, so an SLM can work on the history on the capture PC. Optionally, and later: a refocus advisor that suggests "refocus now", with its reason, as a notice. It is never an actuator.

## Owes
- **174.0:** one night's file read back. Check that the sweep lines carry the coarse curve and the followups catch a hand correction.
- **174.1 to 174.3:** as above.
