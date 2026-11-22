# Phase 167: autofocus that a donut cannot fool

Planned 2026-11-22, from the owner's report at the scope with the real camera:

> "My auto focus thinks a donut is in focus and a focused Saturn is 31% in focus only."

Autofocus started from a Saturn that was nearly in focus and finished entirely out of focus, on a defocused ring: the "donut" an 8SE makes, because its secondary mirror shadows the middle of a defocused image. Status: `PARTIAL`: **167.2's checks `WRITTEN` 2026-11-22 (3.3.44)**; the cause of the bad sweep itself waits on the night's `[AF]` lines. **Urgent:** autofocus should not be used on the sky until this is fixed. `AutorunRefocusEvery=0` keeps `[RUNS]` from calling it.

## Why the tests never saw it
- **The emulator blurs a defocused planet with a box blur** (`CameraEmulator.h`, Phase 73). Only a star gets the doughnut.
- **So every autofocus test ran against a sky that cannot make a donut of a planet,** and passed. The real sky makes one at every defocus beyond a few hundred steps.

## Where it can go wrong (to be confirmed from the night's `[AF]` lines)
1. **The coarse score, 1000/HFD, measured inside an aperture sized from the target's own extent** (`FocusMetric.h::MeasurePlanetFocus`: `ap = max(48, 1.8 * radius + 24)`, with the radius taken from the cells above the threshold).
   - A donut's dark hole is not counted in that extent.
   - So the aperture is too small, and the ring's outer light falls outside it.
   - The half-flux diameter of what is left can then read smaller than a focused Saturn's (disc and rings).
   - The result: HFD is not monotonic far from focus.
2. **The fine pass's edge score** (Tenengrad, `MeasureEdgeFocus`). A donut's two ring edges are crisp; a focused planet's detail is low-contrast. When the coarse curve is flat, the fine span is wide enough to reach donuts, and the edge score prefers them.
3. **The final check only reverts if the score halves,** and it uses the same score. When the score is fooled, the check is fooled too.
4. **The displayed 31%** is the Laplacian sharpness, not what autofocus uses. It is the readout that showed the owner the truth.

## What the first build found (2026-11-22)

`tests/focus_donut.cpp` draws a Saturn-like globe and ring, defocused through an SCT: an annulus with a 0.34 hole, noise, and the globe saturating. It measures them with the app's own scores.
- **Neither score is fooled by the donut as such.** HFD and the edge metric both fall at every step out from focus.
- **But HFD falls slowly on a disc this size.** With the middle already darkening it still scores 54% of focus, so the final check, which reverted only when the score halved, stayed silent until the donut was deep. **That gap is fixed (167.2).**
- **So the donut didn't fool the metrics in the model.** Something in the real frames did, or the sweep ended far out for another reason. The likeliest:
  - a flat or noisy coarse curve ending "not bracketed", which drives to the end of the ±3000-step sweep (the failure of 2026-11-07);
  - noise at gain 230;
  - haze.

  The night's `[AF]` lines decide which. Until then 167.0 and 167.1 (the emulator's donut and a donut detector) stay planned, not built, rather than fixing a guess.

## Stages

### 167.0: a donut in the emulator (size S-M)
- **Defocus a planet the way a Schmidt-Cassegrain does.** Convolve with an annulus whose outer diameter is the defocus blur, with a hole of `CentralObstruction` (rig.ini, Phase 161.3) times that.
- **No telescope in code (Phase 149).** The emulator's own `SCT_OBSTRUCTION` constant gives way to the rig value.
- **A test:**
  - far from focus, Saturn's image has a dark centre;
  - the current autofocus, started near focus with a wide scan, finishes on a donut (the failure reproduced);
  - every later stage turns that test green.

### 167.1: a donut is never "in focus" (size S)
- **The rule:** in `FrameAnalyzer`/`FocusMetric`, a target whose centre is markedly darker than its brightest ring is flagged `donut`, and its focus score is set to 0 for autofocus.
- **The measure:** the median brightness in a disc at the centroid, against the brightest annulus.
- **What it must not catch:**
  - a focused planet (a bright disc);
  - Saturn's ring gap;
  - the Moon, which fills the frame (it takes the fine-only path).
- **Tested on:** the emulator's donut and its sharp targets, plus a synthetic Saturn with Cassini's division.

### 167.2: HFD has the last word, measured fairly (size S) — the checks `WRITTEN` 2026-11-22 (3.3.44)
- **A fair aperture:** sized from the target's outer edge, hole included, so a donut's light is not clipped.
- **HFD can veto the fine pass:** its result is rejected if its HFD is worse than the coarse peak's, beyond the measurement's own spread.
- **The final check (WRITTEN, 3.3.44):** a result a tenth or more worse than the start always takes the same-sky A/B; halving was too loose. In the A/B, HFD a tenth better at the start wins outright (`KeepAfterAB` in `FocusDirection.h`, tested by `af_keep`); the edge metric judges only near focus.
- **The fine span:** limited to the coarse curve's width near its bottom, never the whole scan.

### 167.3: on the sky, then trusted again (owed, item 47)
- Autofocus from near focus, from +700 and from -700, on Saturn or Jupiter.
- It ends where the eye and the Laplacian readout agree.
- The validation log records it.
- Until then the AUTO FOCUS button warns that autofocus is unconfirmed on this rig. The sky run's planet group (156.2b), which sweeps from ±700, does not run.

### 167.4: the focus readout's reference comes only from a trusted sweep (size S)
The owner's screenshot at the scope (2026-11-22): **`OUT 31% of AF 44%`** under a nearly focused Saturn.
- **What it means:** the focus readout (`FocusReference`) takes its reference from the next live readings after any autofocus the routine calls a success. The 44% came from the donut the failed sweep ended on, so the readout now calls good focus OUT.
- **The Laplacian is fooled too:** the readout's sharpness (Laplacian) also rated the donut (44%) above focus (31%). No score in the app tonight told the truth. Only the eye did.
- **The fix:**
  - the reference is armed only after a sweep that passes 167.1 and 167.2 (no donut, and an HFD no worse than the start);
  - a reference taken on a donut-flagged frame is discarded;
  - a CLEAR on the readout lets the owner drop a bad reference by hand.

### 167.5: judge the sweep before fitting it — `WRITTEN` 2026-11-22 (3.3.48)
The owner at the scope, 2026-11-22 at 23:10, with the guard of 3.3.44 built: autofocus "did not give me a donut but messed up the image and I had to correct it".

**What the log showed:**
- **The outward pass started flat.** It read 14.6% at seven positions, with the planet's peak at about 200.
- **Saturn then faded about fourfold in haze.** The score fell to 8.3, 6.3, 5.1 and 4.8%, and the peak to about 55.
- **The return pass read 4.5 to 4.8% everywhere,** including 168665, which had read 14.3% a minute before.
- **The fit chose 165984.** That was 2,681 steps inward of a start that was in focus (the owner's correction: 168,893).
- **The same-sky A/B could not tell start from fit.** Both read 4.6, so it kept the fit: "The sky changed, not the focus".
- **The focus reference was then armed from the faded frames.** The readout called the focused planet "OUT 28% of AF".

**Built** (`FocusDirection.h`, gated by `af_sweep_judge`, which replays the night's own numbers):
- **`SweepSpoiledByFade`, acted on in the same-sky A/B.** The start stands when it re-measures at under half its earlier score **and** the sweep's outward and return passes disagreed by a median of over 1.5x. The night had both: 4.6 against 14.6, and passes 3.11x apart. Either fact alone is not enough:
  - **A fade with a consistent sweep:** `autofocus_saturn`'s fading cases (the 2026-11-08 kind) re-measure at 46-49% with passes 1.21x and 1.23x apart, and their fit is right.
  - **Disagreement without a fade:** a large backlash makes the passes differ 2.0x (`af_overshoot`).

  The first build used the fade alone, and CI sent those fading cases back to a defocused start.
- **The focus reference is not armed** after a sweep that kept a fit only because the sky changed (`AFDiagnostics::skyChanged`). That closes part of 167.4.
- **`[AF] Sweep check` logs both figures for every sweep.** A rule on the disagreement or spread alone failed good sweeps in CI: a planet's flat coarse score (`app_headless`, 3%) and a large backlash.

**Owes:** a clean sweep on the sky (167.3, item 47); the emulator's `autofocus_saturn` passing with the check in place (CI).

## Evidence wanted from the night of 2026-11-21
The `[AF]` lines of the failed sweep:
- `Scan:`;
- each coarse point;
- `Direction fits:`;
- `Fine pass: X (coarse Y, +N)`;
- `Complete` or `failed`.

`Select-String -Path raduk_log.txt -Pattern "\[AF\]"` on the capture PC, or the night's hand-off bundle.
