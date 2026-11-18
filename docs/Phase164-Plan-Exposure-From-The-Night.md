# Phase 164 plan: does the night choose the exposure?

Added 2026-11-17 from the ChatGPT review's "adaptive exposure and gain based on measured seeing", steelmanned. Status: `PLANNED`, **exploratory**: 164.0 decides whether anything gets built. See [`PHASE-STATUS.md`](PHASE-STATUS.md). Release 3.5 (planetary).

## The case for it, at its strongest

Lucky imaging trades two things within one frame:
- A longer exposure lifts the signal above read noise.
- A shorter one freezes the seeing. Blur inside a frame is set by how fast the air changes (the coherence time, a few ms in visible light, and different every night).

On a steady night a longer exposure costs nothing and buys signal. On a turbulent night the same exposure smears every frame, and no frame selection can unsmear it. A fixed per-planet band (140.2, set once by clear-night item 55) is right for the night it was measured on. If the best exposure moves with the seeing, a fixed band is wrong on every other night, in a way the operator cannot see at the eyepiece and that stacking cannot repair. The app already measures the seeing (`SeeingMonitor`, 114) and nothing connects that to the exposure.

## Why it might not be worth building

- The best exposure may hardly move. If item 55 run on three nights of different seeing picks the same exposure each time, a fixed band is enough and this phase is dropped.
- The live figures are not yet trusted to judge it. Sharpness rises with noise (the `SHARP_CEIL` history, clear-night item 1), so a naive metric would prefer the noisiest exposure. Any judge has to be noise-robust: 161.5a's noise gate, or the edge score from 145.
- Changing exposure between runs is fine, but a ladder costs about a minute of sky before each run.

## Stages

- **164.0 Find out, by hand. No code.** Clear-night item 55 repeated on at least three nights whose `SEEING` lines (138) differ, the same target each time. Stack each night's rungs in AutoStakkert with identical settings; the owner picks the best rung by eye. Alongside, from the SERs offline: the top-decile frame score per rung, on the noise-robust metric.
  - **Drop the phase** if the best rung is the same every night.
  - **Drop 164.1 and keep 164.2** if it moves but the metric's choice disagrees with the owner's on most nights.
  - **Go on** if it moves and the metric agrees with the owner.
- **164.1 The ladder, automatic.** Before an Autorun run (113), with the target locked and focused, 8 to 10 s at each of three or four exposures inside the planet's band, with gain set per rung to keep the planet's peak where 140.1 puts it. Pick the rung with the best noise-robust top-decile score and record the run there. The ladder's figures go in the observation record and the run list, so 164.0's comparison keeps running for free.
- **164.2 The advice, if the automation is not trusted.** Exposure stays manual. The run list and the advisory loop say "on tonight's seeing the 5 ms rung beat the 10 ms one by X", from a ladder the owner starts with one key.

## Rules

Phase 149's rules hold: no camera model, plate scale or bit depth in code; nothing needs the internet. The ladder never moves the mount or the focuser.

## Owes the sky

Everything. The emulator's blur does not depend on exposure the way air does, so the bench can gate the mechanism (rungs, gain per rung, the choice, the record) but not the premise.

## Versioning

Every PR bumps PATCH; MINOR marks the milestone (3.5).
