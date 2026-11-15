# Autofocus

A V-curve scan of the EAF with a local three-point parabola fit, a backlash
approach, and — the part that makes it trustworthy — a re-measurement
afterwards that reverts the move if it made focus worse.

[`src/core/AutofocusRoutine.h`](https://github.com/Maligzar/Raduk-Capture/blob/master/src/core/AutofocusRoutine.h)

---

## Since v3.0: backlash, and two stages

> The sections below describe the coarse sweep as built in v1.2.0. Two
> phases have built on it since.

**Phase 139, backlash.**
- The EAF counts motor steps, not mirror position. On the 8SE the knob's slack puts a step number reached outward and the same number reached inward a backlash apart.
- So every move now ends travelling one way by at least the backlash (`FocusDirection.h`), and the EAF's own backlash compensation is set to 0 while connected.
- The sweep visits each point out and back. The average cancels a fading target, and the difference between the outward and inward fits is the backlash measured on the sky, logged every run.
- `AfReadiness.h` turns the button into AUTO FOCUS: NOT NOW, with the reason, when focus cannot work.

**Phase 145, two stages.** HFD (1000/HFD) is the coarse metric: wide capture range, and noise and exposure cancel out of it. But near focus on a large disc it barely moves, and on a frame-filling Moon there is no disc. On synthetic scenes:
- near focus, HFD separated blur 0 from blur 3 px by 0.1 to 0.2 frame-to-frame scatters, against edge contrast's 2.2 to 2.3;
- far out, HFD separated blur 48 from blur 72 px by 44 scatters, against edge contrast's 12.

So after the coarse sweep, a fine pass:
- 7 points across the coarse curve's width, each approached the final way;
- each scored on the edge metric, `MeasureEdgeFocus` in `FocusMetric.h` (Tenengrad on luminance superpixels, Gaussian-smoothed, gated at 3× the measured noise in the gradient's own units, normalised by the mean squared);
- 20 frames a point, about 30 a second, keeping the 75th percentile;
- the centre is re-read at the end, and a 10% to 40% fade is corrected for;
- if anything fails, the coarse result stands.

On a flat-topped, Jupiter-like curve in `tests/autofocus_saturn.cpp`, the coarse stage alone ended 187 to 547 steps from focus, and with the fine pass 2 to 34. A frame-filling Moon runs the fine pass alone. `FocuserFinalDirection` sets which way every move ends, so the last travel can push the mirror against gravity.

Plan: [`Phase145-Plan-Two-Stage-Focus.md`](https://github.com/Maligzar/Raduk-Capture/blob/master/docs/Phase145-Plan-Two-Stage-Focus.md). None of it has run on the sky yet: clear-night items 47 and 58.

## Since v3.3.12: phase 161, safe, calibrated, with an error bar

From a first-principles review on 2026-11-14. Plan:
[`Phase161-Plan-Focus-Safety-And-Calibration.md`](https://github.com/Maligzar/Raduk-Capture/blob/master/docs/Phase161-Plan-Focus-Safety-And-Calibration.md).
Where this page's older sections disagree with this one, this one is current.

**The focuser cannot be sent somewhere by accident (161.0a).** One pure rule,
`src/core/FocuserGuard.h`, used by the EAF wrapper and the emulator alike:
- no move from a position that could not be read (a failed read used to come
  back as 0, and the next relative step, autofocus start or profile save used
  it);
- none outside the focuser's range, and none longer than rig.ini
  `FocuserMaxTravelSteps` (12,000) in one command; the panel's GO TO asks for a
  second press past it, and a profile load or remote move is refused;
- autofocus stays within the guard of where it started, and checks every move
  (accepted, finished, within 2 steps), stopping the motor and ending the sweep
  with the reason when one fails;
- a relative step during a move adds to where that move ends.

**Stop wins (161.0b).** Each focuser holds one command lock round every move,
stop, second leg and SDK call, and `Stop()` advances a stop generation under
it. Autofocus sends every move with the generation it started with
(`IFocuser::MoveInSequence`), so once any stop has returned (the abort, the
panel's STOP, the emergency stop) no move of that sweep can start. The panel's
STOP now ends a sweep.

**Overshoot apart from backlash (161.2).** How far a one-sided move goes past
before coming back is rig.ini `FocuserOvershootSteps` (default three times the
backlash figure, at least 1,500). It only has to exceed the knob's slack;
`FocuserBacklashSteps` keeps its one job, the coarse result's half-backlash
shift. With a slack the app is not told about, the old 200-step overshoot left
one step number two mirror positions 800 steps apart; 1,500 left them the same.

**Auto-exposure is part of autofocus.** It settles before the sweep and is
held inside each stage, as since phase 112. New: after the coarse sweep the
routine goes to the coarse result, releases AE there for up to 10 s, and holds
it again for the fine pass, because an exposure set on a defocused (dim) disc
can clip the focused one.

**The fine pass, refitted (161.4a).**
- Points are visited centre-out, the leading side swapping each ring, each
  approached the final way. A 20% fade across a seven-point pass moved the
  peak 7.5 steps one way across, 2.8 centre-out.
- The centre is read first and last. Within 10% nothing is done; up to a
  factor of three a steady change is taken out, in the logarithm; beyond that
  the pass is given up.
- The position comes from a parabola through the logarithm of every point
  within half the top's height when there are five, else three points: at 8%
  noise a point, 11 steps of scatter against 18.5.
- The result has an error bar from resampling the frames: the status line
  reads `AF done: pos=168214 ±9`. A lower bound on the sky.
- The fine region is sized from the target's half-flux diameter at the coarse
  best point (161.5b), and waits for a size rather than falling to 48 px.
  rig.ini `AutofocusChannel=green` scores only the green Bayer sites, which
  may focus better low in the sky where dispersion splits the colours.

**CALIBRATE FOCUS (161.3).** `Ctrl`+click AUTO FOCUS (or remote
`calibrate_focus`) after an autofocus. It measures how well a position repeats,
the knob's slack (a reversal from the slope of the edge score), and the growth
of the half-flux diameter on both wings; `src/core/FocusCalibration.h` turns
that into micrometres per step and the focus zone in steps, using rig.ini's
pixel size, focal ratio and `CentralObstruction`, and appends a block to
`focus_calibration.ini`. This is the measurement the **Parameters** section
below was never based on: the arithmetic (5,760 steps a turn, a 0.75 to
1.06 mm thread, a moving primary's 26x) gives about 3.4 to 4.8 um a step and
a focus zone of about 60 to 85 steps, against the "2,000 steps per mm" in the
code's note. Clear-night item 59 runs it.

**Threads (161.1).** The completion no longer runs hooks on the scan thread:
the routine leaves its result with `AutofocusController`, and `Deliver()`,
called from the frame loop, runs them on the GUI thread. `Start()` reads as
running at once; each sample carries its frame's capture time and a collection
window takes only frames taken after it opened.

Tests: `focuser_safety`, `af_exposure_window`, `af_sample_window`,
`af_overshoot`, `focus_calibration`, `af_fine_pass`, `ring_monotonic`, and
cases in `af_controller`, `app_feed` and `focus_metric`.

## The algorithm

```
1. Sweep the EAF across SCAN_RANGE steps in N_STEPS positions
2. At each position: settle, then average FRAMES_PER_STEP sharpness samples
3. Fit a parabola to the peak sample and its two neighbours
   (fall back to a global least-squares fit, then to raw maximum)
4. Drive to the fitted peak, approaching from one side to take up backlash
5. Re-measure sharpness there and compare with the start
6. If it got clearly worse, go back to the start and report failure
```

```mermaid
stateDiagram-v2
    [*] --> Idle
    Idle --> Scanning: Start(currentPosition)
    Scanning --> Failed: peak < minSharpness
    Scanning --> Moving: fit produced a position
    Moving --> Complete: final sharpness acceptable
    Moving --> Failed: final < 50% of start - reverted
    Scanning --> Failed: Abort()
    Moving --> Failed: Abort()
    Complete --> Idle
    Failed --> Idle
```

---

## Parameters

```cpp
struct AutofocusParams {
    int   scanRange     = 6000;   // total steps swept (+/-3000 from start)
    int   nSteps        = 11;     // sample positions (odd preferred)
    int   framesPerStep = 8;      // sharpness samples averaged per position
    int   settleMs      = 600;    // wait after the EAF stops moving
    int   backlash      = 200;    // steps of compensation on the final move
    float minSharpness  = 2.0f;   // abort if the peak never exceeds this
    int   maxStepBound  = 600000; // EAF physical range
    int   minStepBound  = 0;
};
```

> **These are v1.2.0's defaults.** Since phase 161 the struct also carries
> `overshoot`, `exposureWindowMs` and the fine-pass fields, and the bounds are
> narrowed at each start to the focuser's range and the travel guard. The
> reasoning below has not been checked against the sky; CALIBRATE FOCUS
> measures it (see above).
>
> **Why these numbers.** Tuned for the ZWO EAF on a Celestron 8SE at f/10.
> The 8SE has ~2000 steps/mm of focus travel at this f-ratio, and at typical
> planetary seeing the focus zone is ±500–1000 steps. A ±3000-step sweep
> with 11 positions gives 600-step spacing — coarse enough to cover the
> range, fine enough to find the peak.

**It takes about a minute, and that is correct.** Each position waits for 8
*genuinely new* measurements at the analyzer's own ~2 Hz rate, which is
what makes the curve fittable rather than seeing noise.

---

## The fit

### Preferred: three-point parabola around the peak

```cpp
int bestPos = FitPeakWindow();           // local, preferred
if (bestPos < 0) bestPos = FindBestPosition();   // global least squares
if (bestPos < 0) { /* fall back to raw maximum sample */ }
```

> **Why local beats global here.** A defocus curve is **not a parabola**: it
> has a sharp peak and long shallow tails, and a global least-squares fit
> across all 11 samples is dragged by the tails. Measured against a known
> true focus of 51,500, the global fit returned **52,224 — 724 steps out,
> against a sample spacing of 600.** Three points around the peak use only
> the region where a parabola is actually a good local model.

The fit uses divided differences, which tolerate the slightly uneven
spacing that integer position rounding leaves:

```cpp
const double d1 = (y2 - y1) / (x2 - x1);
const double d2 = (y3 - y2) / (x3 - x2);
const double a  = (d2 - d1) / (x3 - x1);
const double b      = d1 - a * (x1 + x2);
const double vertex = -b / (2.0 * a);
```

### Three conditions that reject the local fit

Each returns `-1` and falls back, and each is logged with a reason:

| Condition | Meaning |
|-----------|---------|
| Peak at either end of the sweep | **Focus is not bracketed at all.** `m_notBracketed` is set and the user is told to scan wider or recentre |
| `a >= -1e-12` (not concave at the peak) | That is noise, not a peak |
| Vertex outside the bracketing interval `[x1, x3]` | The samples are internally inconsistent |

A fit that refuses to answer is more useful than one that always answers,
because the fallback path is strictly better informed than a confident
wrong number.

---

## Backlash approach

```cpp
if (m_params.backlash > 0) {
    int approachFrom = finalPos - m_params.backlash;   // clamped to minStepBound
    m_focuser->MoveTo(approachFrom);
    WaitForMove();
    Sleep(300);
}
m_focuser->MoveTo(finalPos);
```

Always approach from **below**, in the same direction, for repeatability.
Mechanical backlash means the position reached depends on the direction of
travel; approaching consistently removes that as a variable.

---

## The revert, which is the point

```cpp
const float REGRESSION_FRAC = 0.50f;
if (startSharpness > 0.0f && finalSharp < startSharpness * REGRESSION_FRAC) {
    m_focuser->MoveTo(m_startPosition);
    Finish(false, m_startPosition, startSharpness, startSharpness,
           "Focus got worse (… -> …) — reverted to start; "
           "try a wider sweep or check the target");
    return;
}
```

> **Why it exists.** `Finish(true, …)` used to be unconditional:
> `improvement` was computed, logged, and then ignored — so a parabola
> fitted to noise could drive the focuser somewhere visibly worse and still
> report "AF done". **Observed on the bench: start 11.3%, final 0.0%,
> reported as success.** `minSharpness` does not catch this, because it
> tests the sweep's *peak*, not the outcome.

### Why the margin is 50% and not tighter

> `startSharpness` and `finalSharp` are measured **45–60 seconds apart** —
> the whole sweep sits between them — and seeing varies on exactly that
> timescale. A tight threshold would revert a genuinely better focus just
> because the sky got worse meanwhile. Requiring the sharpness to **halve**
> catches a fit that landed somewhere bad while leaving ordinary seeing
> variation alone.
>
> A fair same-moment A/B would need a re-measure back at the start
> position; that is a larger change and has not been made.

This is a good example of a threshold whose looseness is a deliberate
statement about measurement error, not a guess.

---

## What it reports

| Message | Meaning |
|---------|---------|
| `AF done: pos=… +24.0%` | Worked. Sharpness improved by that much |
| `focus not bracketed — scan wider or recentre` | The sharpest point was at the edge of the sweep, so true focus is probably outside it. Focus roughly by hand, then re-run |
| `Focus got worse (… → …) — reverted to start` | The fit landed somewhere worse, so the focuser went back. Usually too little contrast: centre the target, or wait for steadier air |
| `No usable star/planet — sharpness too low` | The sweep's peak never exceeded `minSharpness` |

---

## Three faults fixed in v1.2.0

Worth recording because together they mean **autofocus results from before
v1.2.0 were not meaningful:**

1. **Each of the 8 measurements per position was the same reading copied 8
   times.** Every point on the curve carried full single-frame seeing noise
   instead of an average, which is precisely the thing averaging was meant
   to remove.
2. **The confirming measurement after the move collected nothing at all.**
   `finalSharp` was always `0.0`, so the reported improvement was always
   about −100% — and ignored, which is why nobody noticed.
3. **Success was reported unconditionally.** See the revert, above.

Plus, separately, the sharpness metric itself was computing a
red-minus-green colour difference rather than detail. See
[Image Analysis](Image-Analysis#sharpness). **Sharpness readings changed
scale when this was fixed, so numbers remembered from earlier sessions no
longer compare.**

### The sample-collection bug, in detail

```cpp
// Gate on m_samplesNeeded, not on state == Scanning. The final
// verification after the move runs in state Moving (set before it),
// so a Scanning-only gate collected NOTHING there.
AFState st = m_state.load();
if (st != AFState::Scanning && st != AFState::Moving) return;
int needed = m_samplesNeeded.load();
if (needed <= 0) return;
```

`m_samplesNeeded` is the real signal — it is non-zero only inside a
collection window — so the state test was both redundant during the sweep
and wrong afterwards. The current code accepts both `Scanning` and
`Moving`, and gates on the counter.

Phase 81 closed a related race: the sample vectors are now cleared
**before** arming `m_samplesNeeded`, not after.

---

## Thread model

```cpp
af.SetFocuser(&focuser);
af.SetOnComplete([](AFResult r) { /* NOT on the GUI thread */ });
af.SetLogger(mainLogFunction);
af.Start(currentPosition);                 // returns immediately

// each render tick:
af.SubmitSharpness(lastAnalysis.sharpnessPct,
                   lastAnalysis.centroidValid,
                   lastAnalysis.centroidPeak);
```

The scan runs on a dedicated thread; **samples come from the main thread**
via `SubmitSharpness()`. Progress and state are atomics, so the UI polls
`State()` / `StatusText()` without locking.

The completion callback fires **on the scan thread**. Since phase 161.1 it
only hands the result to `AutofocusController::OnComplete`, which keeps it;
`Deliver()`, called from the frame loop, runs the panel, session-log and
Autorun hooks on the GUI thread, and `Busy()` stays true until it has. Before
that the hooks set window text from the scan thread.

### Joinable, not detached

> Phase 59 (review G4): the scan thread was detached, so at shutdown it
> could outlive `RunApp()`'s locals and invoke a destroyed `std::function`
> or `ControlPanel`. The thread is now joinable, the destructor waits for
> it, and `Abort()` makes that wait short. `Start()` also reaps the
> previous finished thread before spawning a new one.

### The diagnostic sink

```cpp
void SetLogger(std::function<void(const char*)> f);
```

> **Why.** The per-step V-curve used to go only to `printf`, which this app
> sends to an `AllocConsole` window — so the one piece of data needed to
> tell a good focus run from a bad one **vanished when the console
> closed**, and could not be read back from any log. Pointed at
> `main.cpp`'s `Log()`, the whole curve lands in `raduk_log.txt`.

The `centroidValid` / `centroidPeak` arguments to `SubmitSharpness` are
diagnostic only: the sharpness crop follows the centroid, so a step where
the centroid kept dropping out measured something other than focus, and
the log says so.

---

## Operating tips

- **Start near focus.** The sweep covers ±3000 steps; if true focus is
  outside that it will say `not bracketed` rather than guess. It also never
  goes further than `FocuserMaxTravelSteps` from where it started.
- **Calibrate once on a clear night** (`Ctrl`+click AUTO FOCUS after a
  focus): the slack it measures belongs in `FocuserBacklashSteps`, and
  `FocuserOvershootSteps` must be larger than it.
- **Use a bright star or a planetary disc**, never a faint fuzzy object.
- **Check the EAF is connected** — FOCUSER shows a position, not dashes.
- **Press `A` again to abort** mid-run.

### Testing it in the emulator

The emulator models defocus (Phase 73), which is what makes autofocus
testable indoors:

| Variable | Default | Role |
|----------|---------|------|
| `RADUK_EMU_TRUE_FOCUS` | 168000 | The position at which the image is perfectly sharp — **gives a focus test a known correct answer** |
| `RADUK_EMU_BLUR_PX_STEP` | 0.0083 | Blur added per step away from focus |
| `RADUK_EMU_FREEZE_SURFACE` | off | Stops planet rotation, turbulence and moons, so **only focus varies** |
| `RADUK_EMU_NOISE` | 0 | Per-pixel sensor noise, DN |

> **Why the emulated focuser opens at 166,500.** Both numbers matter. The
> real rig focuses near 168,187, so the emulator starts in the same part of
> the range — it used to start at 50,000, which was nowhere near anything
> real, and since autofocus searches ±3000 steps from wherever it starts,
> from 50,000 it swept 47,000–53,000, about **115,000 steps away from
> focus**. It could never bracket focus from a fresh launch, failed every
> time, and looked broken when it was behaving correctly. The 1,500-step
> offset from true focus means the emulator opens slightly **soft** —
> starting exactly at focus would make an autofocus run a no-op that proves
> nothing.

See [Emulator and Testing](Emulator-and-Testing).

---

**Next:** [Image Analysis](Image-Analysis) · [Advisors](Advisors) · [Emulator and Testing](Emulator-and-Testing)
