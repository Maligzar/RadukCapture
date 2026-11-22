# Phase 168: RGB ALIGN corrects the dispersion it computes, no more

Planned 2026-11-22, from the owner at the scope: "RGB align does not seem to be working. The image looks like I am seeing double and they are not aligned." Status: `WRITTEN` 2026-11-22 (3.3.41). Display only (the recordings are not touched), so turning it off is a complete workaround.

## The defects (`PreviewRenderer.cpp`, Phase 23)
1. **It corrects by 1.5 times the dispersion.**
   - Blue moves the whole computed shift one way, and red half of it the other way.
   - So the blue-red separation changes by 1.5 x what `ADCAssistant` computed.
   - At +9 px that leaves about 4.5 px of fringe of the opposite sign: the double image.
   - **The fix:** split one total between the two. The correction moves blue and red apart by exactly the separation, with green as the reference: blue by its share, red by the rest, in proportion to the refractive index differences `ADCAssistant` already uses.
2. **It shifts along sensor rows only.**
   - Atmospheric dispersion lies along the vertical circle, toward the zenith.
   - Where that falls in the frame depends on the camera's rotation, and on whether the optics mirror or invert the image (row order, item 15).
   - **The fix:** shift along the zenith direction in the frame. On an alt-az mount that is fixed to the camera, so it is one measured angle (`CameraZenithAngleDeg`, measured once by jogging in altitude and watching the target move), with a sub-pixel shift along it.
3. **It knows nothing of a physical ADC.**
   - With an ADC in the train and dialled in, a software shift adds dispersion back.
   - **The fix:** with an ADC configured as fitted, the automatic part is 0 and only the manual offset applies, said on the HUD.
4. **A comment names a camera and a plate scale** (`0.203 arcsec/px for 8SE+ASI678MC`), against Phase 149. Removed; the plate scale comes from `RigConfig`, as the code already does.

## Tests
- A pure function, so it is tested on Linux.
- After correction, synthetic R, G and B discs displaced by a known dispersion line up within 0.5 px:
  - at any camera angle;
  - with and without an inverted image;
  - with an ADC fitted, the automatic part is 0.
- The existing render path calls it.

## As built (3.3.41)
- **`core/RgbAlign.h`** (pure):
  - `BlueShareOfDispersion`: Edlen at the band's ends and middle; about 0.71 for 400-700 nm;
  - `ComputeRgbAlign`: one sampling offset each for blue and red. Together they move by exactly the separation, along the zenith's direction.
- **`PreviewRenderer.cpp`** applies the two offsets in x and y.
- **rig.ini `CameraZenithAngleDeg`:** degrees clockwise from the frame's top. The default 180 keeps the old direction (blue up, red down).
- **With `ADCFitted=1`** only the manual offset applies.
- **`tests/rgb_align.cpp`:**
  - 3, 9 and 17.5 px at every 15 degrees, within rounding;
  - the old correction's 4 px fringe, for the record;
  - the ADC case;
  - the directions.
- **Owed:**
  - the owner's look on the sky;
  - if colours still double after this, the zenith angle is not 180 on this camera. Jog up in altitude: the zenith is opposite the way the target moves in the frame. Set `CameraZenithAngleDeg` to that.
