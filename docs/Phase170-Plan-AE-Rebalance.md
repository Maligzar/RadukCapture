# Phase 170: auto-exposure settles on its own path, not only on the right brightness

Raised by the owner at the scope, 2026-11-21: "Why is AE not doing this automatically?", about recording Saturn at 25 ms and gain 230 when shorter exposures would give more frames. Status: `PARTIAL`: 170.0 `WRITTEN` (3.3.66); 170.1 `PLANNED`.

## Correction (2026-11-23, 3.3.69): the band, not the path
The analysis below and 170.0's example missed the profile's exposure band (Phase 140.2).
- **How the band changes the ladder.** `main.cpp`'s `setAERails` gives AE `TargetProfile::AERail()`, which for a profile with a band narrows the exposure rail to it. It puts the exposure knee at the ceiling and the gain knee at the HCG floor.
- **Saturn's starting band is 8 to 25 ms.** On that ladder, AE uses exposure all the way to 25 ms before it adds gain. So 25 ms at gain 230 was **on** Saturn's path, and 170.0's rebalance leaves it there (`ae_rebalance` 4b).
- **The 20 ms at gain 249 example** holds only on the default ladder, with no band.
- **What answers the owner's question.** "Why not shorter exposures for more frames?" is a band question:
  - **The ceiling is unmeasured.** Saturn's 25 ms is a starting value from amateur practice for an 8-inch at f/10, never measured here.
  - **The trade-off is real.** A lower ceiling gives more frames and more frozen seeing, but at a higher gain and more noise per frame. 140.2 chose exposure-first because only exposure collects light.
- **170.1 is item 55.** The mechanism already exists (the knee follows the band's ceiling); the exposure ladder measures where detail peaks per planet and sets `[exposure_band] ceiling_ms`. Until then the owner can lower `ceiling_ms` in `saturn.ini` by hand.

## Why it doesn't
- **The path.** AE walks one path through exposure and gain (`LightLadder.h`): gain to the HCG floor (182), exposure to the knee (20 ms), gain to its knee (350), then exposure, then gain. **But it only moves when the brightness is wrong.**
- **What happened tonight.** A setting off that path, from a profile or a hand change, stays put once the highlight sits in AE's band. 25 ms at gain 230 is off the path; the path would be about 20 ms at gain 250.
- **The knee is unmeasured.** The 20 ms knee is a starting value (UNVERIFIED items 35 and 55). The best exposure per planet is what the exposure ladder (item 55, and the sky run's planet group) measures.

## Stages
1. **170.0, rebalance (`WRITTEN` 3.3.66).**
   - **The functions.** `LightLadder.h`:
     - `LightStops` (the light at a setting, in stops above (expMin, 0));
     - `OnPath` (the path point with that light);
     - `OffPathStops`;
     - `RebalanceStep`: one step at constant light, at most 1 stop of exposure, with gain the other way by the same amount. It does nothing within 0.15 stop of the path, or when the light is beyond the path's ends.
   - **The night's setting, on the default ladder.** 25 ms at gain 230 is 0.32 stop off the default path and goes to 20 ms at gain 249 in one step. With Saturn's band it is on the path and stays (see the correction above).
   - **In the assistant.** `AutoExposureAssistant::Tick` uses it when the highlight is in band and nothing holds AE; recordings and focus sweeps already hold it. It logs `AE: = rebalance, same light: ...`.
   - **Settled.** A rebalancing tick is not settled, so autofocus waits for the settings to stop changing.
   - **The switch.** `Config::rebalanceMaxStops` (1.0; 0 turns it off).
   - **Tests.** Gated by `ae_rebalance`. `camera_controls`' settle check now feeds a setting on the path, and its simulated camera still settles every case inside its rails, a dim planet in 31 ticks.
   - **The original plan:** When the brightness is in band, the setting is off the path, and no recording or sweep is running, AE slides along its own path to the equivalent point. The light stays the same, measured in stops; a rebalance moves at most a stop per step, and the log says so. Tested on `LightLadder`'s pure functions.
2. **170.1, the knee from the ladder.** Once item 55 has been run on a planet, its profile's `[exposure_band]` holds the measured ceiling, and AE's knee follows it (140.2's band). Until then the default stays and is labelled a starting value.
3. **Not here:** exposure that follows the seeing (phase 164) waits on 170.1's measurements.
