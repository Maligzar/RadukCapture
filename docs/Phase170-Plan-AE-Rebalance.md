# Phase 170: auto-exposure settles on its own path, not only on the right brightness

Raised by the owner at the scope, 2026-11-21: "Why is AE not doing this automatically?", about recording Saturn at 25 ms and gain 230 when shorter exposures would give more frames. Status: `PLANNED`.

## Why it doesn't
- **The path.** AE walks one path through exposure and gain (`LightLadder.h`): gain to the HCG floor (182), exposure to the knee (20 ms), gain to its knee (350), then exposure, then gain. **But it only moves when the brightness is wrong.**
- **What happened tonight.** A setting off that path, from a profile or a hand change, stays put once the highlight sits in AE's band. 25 ms at gain 230 is off the path; the path would be about 20 ms at gain 250.
- **The knee is unmeasured.** The 20 ms knee is a starting value (UNVERIFIED items 35 and 55). The best exposure per planet is what the exposure ladder (item 55, and the sky run's planet group) measures.

## Stages
1. **170.0, rebalance.** When the brightness is in band, the setting is off the path, and no recording or sweep is running, AE slides along its own path to the equivalent point. The light stays the same, measured in stops; a rebalance moves at most a stop per step, and the log says so. Tested on `LightLadder`'s pure functions.
2. **170.1, the knee from the ladder.** Once item 55 has been run on a planet, its profile's `[exposure_band]` holds the measured ceiling, and AE's knee follows it (140.2's band). Until then the default stays and is labelled a starting value.
3. **Not here:** exposure that follows the seeing (phase 164) waits on 170.1's measurements.
