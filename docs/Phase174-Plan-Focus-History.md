# Phase 174: a focus history that learns

Written 2026-11-22 at the owner's request, imaging through cloud: "a big learning opportunity for my autofocus so I want to make sure that I capture the meta data in a way that I can build a history of focusing so that my autofocuser starts to learn ... maybe we even build a skill, an MCP server, and allow an LLM or SLM to get involved with the auto focusing." Status: 174.0 `WRITTEN` (3.3.54); 174.1 to 174.3 `PLANNED`.

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

## 174.1: a temperature model (`PLANNED`)
1. **The fit.** Fit settled focus against focuser temperature, per target and filter, robustly: the median slope over pairs (Theil-Sen), with its spread.
2. **Using it.** Autofocus starts at the predicted position with a narrower sweep when the model is confident. A temperature change worth more than the critical focus zone (Phase 161's focus calibration) raises "refocus suggested".
3. **The amount of data needed.** It reports how many sweeps and nights it rests on, and acts only above a minimum.

## 174.2: replay (`PLANNED`)
`tools/focus_replay` feeds each recorded sweep's coarse and fine readings through the routine's decision code: the fit, the sweep judge and the A/B. It reports what each rule version would have chosen against the followup's settled position.

- **Tonight's fade becomes a permanent case.**
- **The gate for any rule change,** whether hand-written or model-proposed: it must not make a single recorded sweep end further from where the owner settled.

## 174.3: an MCP server for focus history (`PLANNED`)
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
