# RadukCapture Help & Reference
### Built for the stars

---

## Read this first — safety

This telescope is shared with family, including children. These rules come
before everything else in this document.

**Never point the telescope at the Sun.** Not for a moment, not "just to
check", not with sunglasses on. An 8-inch telescope gathers roughly 800 times
more light than an eye does. Looking through it at the Sun destroys sight
instantly, permanently, and without any pain to warn you. It will also cook
the camera and can set the inside of the tube smouldering. There is no safe
way to do this with this equipment and no accessory here that makes it safe.
**If the Sun is above the horizon, the telescope stays capped and parked.**

**Wait until it is properly dark.** If the Sun is visible anywhere in the sky,
it is not time yet.

**Never push, pull or twist the telescope by hand while it is powered on.**
It moves itself with motors, and forcing it strips the gears — an expensive,
permanent repair. To move it, use the arrow keys or the hand controller.

**The stop key is `Space`.** `Esc` does the same thing. Either one halts all
telescope movement immediately, from any window, at any time. If anything at
all looks wrong — the telescope moving when it should not, a cable pulling
tight, a noise you do not recognise, or you are simply not sure — **press
`Space` first and work out why afterwards.** Nothing is damaged by stopping.
Plenty can be damaged by hesitating.

**Mind the cables.** Several cables run from the telescope to the computer. As
the telescope turns to follow the sky they can wrap around the mount or pull a
plug out mid-session. The app watches for this and warns you (see *Cord wrap*),
but look at the actual cables now and then. Never let one go taut.

**It is heavy and top-heavy.** The tube alone is about 6 kg on a tripod that
can tip if knocked. Do not lean on it, hang anything from it, or let anyone
swing on the tripod legs. Keep the legs where nobody will trip over them in
the dark.

**Use red light.** A white torch or a phone screen at full brightness ruins
everyone's night vision for about twenty minutes. Press `N` for the app's
night-vision mode.

---

## First night: a walkthrough for someone who has never done this

Written for someone who has never used a telescope. Follow it in order. You do
not need to understand any of the words to get a picture of a planet.

**What you are doing:** pointing a telescope at a planet, getting it sharp, and
recording a short video of it. Later, software stacks the good frames of that
video into one clean photograph. You are not taking a single photo — you are
collecting thousands of slightly-wobbly ones.

**Why it wobbles:** you are looking through 100 km of moving air. The planet
boils and shimmers. This is called *seeing*, and it is the reason for all the
machinery here. Nothing is broken when the image wobbles.

### 1. Put the telescope outside early

An hour before you want to look, carry the telescope outside and take the cap
off. Glass and metal that are warmer than the night air make heat ripples
inside the tube that blur everything. Nothing you do later fixes this, so give
it time. Forty-five minutes is a minimum; ninety is better.

### 2. Switch on and start the app

Power the mount, plug the camera and focuser into the computer, then start
RadukCapture. It finds the equipment by itself.

If something is missing it says so plainly and tells you which cable to check.
A list appearing is normal and not a failure — read it, fix what it names, and
press **Re-check now**. The list updates live, so you can stand at the
telescope and watch a line turn green as you plug something in.

### 3. Teach the mount where it is pointing

The telescope does not know which way it is facing when it wakes up. Until it
does, it cannot follow anything and **it will refuse to track**.

On the hand controller: **Align → Quick Align**, and accept the prompts. This
needs no stars and takes under a minute. If you skip this, everything later
appears to work and then quietly does not — this is the single most common
reason a night produces nothing.

### 4. Choose something to look at

Press `G` for the GoTo panel. It lists what is up tonight, brightest first,
and greys out anything below the horizon.

**Good first targets:** the Moon (enormous, bright, impossible to miss),
Jupiter, Saturn. **Not for a first night:** anything faint or fuzzy — galaxies
and nebulae need a different technique entirely.

Pick one and press **GoTo**. The telescope swings round. Let it finish.

### 5. Find it on the screen

Look at the preview. You may see the planet immediately, or you may see
nothing at all, which is normal — the camera sees a patch of sky about the
width of a pencil held at arm's length.

If the screen looks empty:

- Press `B` a couple of times for `BOOST ×2` and `×4`. This brightens the
  display so a dim smudge becomes visible. It does not change what is recorded.
- Nudge the telescope with the **arrow keys** and watch for something moving
  through the frame.
- Open `MOUNT CTRL` and watch the motion plot at the bottom while you
  press an arrow key. If the trail does not move, the telescope is not
  moving — which is a different problem from being pointed at the wrong
  patch of sky, and worth knowing before you hunt any further.
- Check that the lens cap is off. This catches everybody eventually.

A planet looks like a small bright blob at this stage, probably fuzzy. That is
expected — it is not focused yet.

Once it is somewhere on the screen, **`Ctrl` + click on it** to bring it to
the middle, rather than nudging it there with the arrow keys. See
**Centring by clicking** below for what it needs first.

### 6. Get it sharp

Press `A` for autofocus. The focuser steps through a range, measures sharpness
at each position, and drives to the best one. **It takes about a minute. Let it
finish.**

When it is done it reports what it found. If it says the focus got worse and it
has gone back to where it started, that is the app being honest rather than
failing — read what it suggests and try again. If it says focus was *not
bracketed*, the true focus lies outside the range it searched: focus roughly by
hand first, then run it again.

You can also focus by hand with `,` and `.` while watching the picture.

### 7. Watch before you record

Press `B` until the display says `STACK 10 BEST`. The app now keeps the
sharpest recent frames, lines them up, and averages them. The picture goes
quieter and detail that was lost in the shimmer appears — Saturn's rings
separating from the disc, cloud bands on Jupiter.

Two numbers appear top-right: `LIVE` for the single frame, `STACK` for the
combined one. **Higher means sharper.** Watch `STACK` for half a minute. If it
is drifting down, the air is getting worse, and waiting is better than
recording.

### 8. Record

Set a length — a minute or two is plenty — and press **RECORD**. Display
effects switch off automatically so the raw frames are saved untouched.

Then stop, and you are done. The video is in the captures folder, ready for
stacking software.

### If you get stuck

Everything in *Troubleshooting* below is written as a symptom you would
actually notice, not a technical fault name. Look for the one that sounds like
what you are seeing.

---

## What the numbers mean

A beginner will reasonably ask whether a number is good. Honest answers:

| Reading | What it is | What is good |
|---------|-----------|--------------|
| `LIVE` / `STACK` | Sharpness of one frame, and of the stacked frames. A gradient measure, corrected for noise and brightness. | **Relative only.** There is no universal good value. Watch whether it rises or falls as you change focus or as the night goes on. Higher is sharper. |
| `Seeing n/10` | How this moment compares with the best frame of the last few seconds | **Relative, not absolute (v1.2.2).** It touches 10 whenever a frame is the best seen recently, so it does NOT say whether tonight is a good night - it says whether right now is a good moment. Watch how far it DIPS: steady air stays near 9-10, poor air swings down to 5 or below. |
| `FPS` | Frames per second being captured | Higher is better — more frames to choose from. Shorter exposures raise it. |
| `DROPS` | Frames lost before reaching the disk | Should be **0**. Anything else means a USB or disk problem, and those frames are gone. |
| `TRK` | Whether the mount is following the sky | Must say **ON**. If it says OFF, the planet drifts out of frame within a minute or two. |
| Focuser position | Raw step count of the focus motor | Meaningless on its own. Only the number at best focus matters, and it changes with temperature. |
| Motion plot `"/s` | How fast the telescope is moving right now | Zero when parked with tracking off. Any non-zero value while you hold an arrow key means the command is getting through. |

**The most important thing to understand about sharpness numbers:** they are
only comparable to *other readings taken the same night on the same target*.
They are not a score out of anything, and they changed scale in v1.2.0, so
numbers remembered from earlier sessions no longer compare.

---

## Notes for the assistant

This file is loaded as reference context for the AI assistant. If you are that
assistant, helping someone who does not know this equipment:

**What you can see.** Alongside this file you are given a live session
snapshot: the time, the target and profile in use, the mount's RA/Dec/Alt/Az
and whether it is tracking, gain, exposure, FPS, the seeing score, the
**capture format and whether it is a crop**, the capture limit, **which
display modes are altering the image**, focuser position and temperature,
recording state, tonight's visible objects with their remaining imaging
windows, and the captures already recorded. The filter fitted to the camera
is in your identity block. Use those numbers rather than asking for them, and
do not state a number that is not in them.

**Safety is not negotiable.** If the Sun is up, or could be, refuse to help
point the telescope anywhere and say why. If the person describes a cable
pulling tight, the mount making a grinding noise, or movement they did not
ask for, tell them to press `Space` before anything else.

**Assume no vocabulary.** Say "the knob that moves the focus" rather than
"the EAF", "how steady the air is" rather than "seeing", unless they use the
term first. Explain a word the first time you need it, once.

**Check alignment early.** When someone reports the planet drifting out of
frame, that the mount will not track, or that `TRK` shows OFF, the cause is
almost always that Quick Align was never run. Ask before suggesting anything
more elaborate.

**Give one step at a time** and say what they should expect to see, so they can
tell whether it worked. "Press `A`. The focuser will hum and move for about a
minute — let it finish" is useful. A list of six steps is not.

**Do not invent numbers.** There is no good value for the sharpness readings.
If asked whether a reading is good, explain that it is relative and tell them
what to compare it against.

**Dim results are usually the sky, not the rig.** Poor seeing, a tube that has
not cooled, or a target low to the horizon explain most disappointing nights,
and no setting fixes them. Say so rather than suggesting changes that will not
help.

**Be straight about failure.** This app reports honestly when autofocus made
things worse or when a check did not pass. Relay that plainly instead of
softening it — a beginner who is told everything is fine when it is not learns
to distrust the whole thing.

**Know what the app cannot see.** It has no way to detect which filter is on
the camera, whether the dust cap is off, or whether the tube has cooled. The
filter is the one that silently changes a number: every dispersion figure
assumes the configured band. If someone says they changed or removed a
filter, tell them to set it in SETUP before trusting any ADC reading. Do not
guess a band from the target or the season.

**Autofocus is not a find-focus-from-anywhere button.** It searches ±3000
steps around wherever the focuser already is. If someone is far out of focus,
they need to get roughly sharp by hand first, or it will report `focus not
bracketed` — which is it working correctly, not failing.

**The display stretch is not an exposure change.** If someone says the image
looks too dark or too flat, the stretch (`L`, and the Black/White sliders) is
the first thing to try, because it costs nothing and changes no data. Raising
gain or exposure to fix how something *looks* makes the recording worse.

**Dark and flat correction change the preview, not the recording.** If
someone asks why their saved file looks noisier than the screen, that is why
and it is correct. A master only applies at the resolution and bit depth it
was taken at; the status line says so when it does not.

**Check the display line before blaming the camera.** Several modes change
only what the image *looks* like and nothing about what is captured: RAW
debayer makes it greyscale, night vision makes it red, single-channel makes
it a grey version of one colour, display FX brightens it, and a framing shot
makes it briefly bright and noisy. The snapshot lists whichever of these are
on, and says `Display: normal` when none are. So when someone reports a
picture that looks wrong, read that line first. If it says normal, the
display is not the cause and the cause is exposure, focus, or the sky. If it
lists something, name it and say which key turns it off — do not send them
hunting through the camera sliders for a fault that is not there.

**Do not promise what is not built.** There is no AVI output, no
polar-alignment assistant, no Bahtinov or multi-star FWHM focus aid, no image
annotation and no stellar photometry. Dark and flat correction exist but only
change the preview, so darks for the final image still belong in the stacker.
The section "What is not built yet" is the list. Say it plainly and say what
to do instead.

**Selecting a target sets the camera up.** Picking an object on the map or
in GoTo loads that target's profile: gain, exposure, resolution, bit depth,
focus position. So if someone asks "what settings should I use for Jupiter",
the answer is usually "pick Jupiter and it is already done". If the picture
then looks wrong, it is the profile that wants editing, not the sliders —
sliders changed by hand are lost on the next load unless they press SAVE.

**A crop is recoverable; say so.** The session snapshot tells you the format
and marks it `CROPPED` when it is narrower than the sensor, so you do not
have to ask. If someone says the view has gone narrow, or that they cropped
and cannot get back, the answer is the `FULL` button on the top bar. Do not
suggest restarting the app. If they ask about data rate or
file size, the controls are the resolution buttons and `8b`/`16b`; the USB
slider in the CAMERA section is a different thing and does not change what a
frame contains.

---

## Quick Key Reference

Keys work **whatever window or panel has focus**. Before v1.1.5 they only
reached the mount while the main window was focused, so clicking a text field
or opening Help silently killed them until you clicked the preview again.

Both `Esc` and `Space` stop the mount, and both work unconditionally. Until
v1.1.8 `Space` was listed here but bound to nothing, and `Esc` exited
fullscreen instead of stopping - so there was no stop key at all while
fullscreen.

**One exception: the command palette (`Ctrl+K`).** While it is open, keys
type into it. `Esc` closes it and does nothing else, because the emergency
stop also ends a recording, and dismissing the palette mid-run must not do
that. Press `Esc` again to stop. The palette closes as soon as you click
anywhere else, and the STOP buttons always work. Its first entry is the
emergency stop.

| Key | Action |
|-----|--------|
| `Space` | Emergency stop all mount movement |
| `Esc` | Emergency stop (works in fullscreen too; `F11` exits fullscreen) |
| `Arrow keys` | Jog the mount (hold to slew, release to stop) |
| `?` | Toggle this Help window |
| `Ctrl+K` | **Command palette.** Type part of what you want ("hist", "night", "snap"), `Up`/`Down` to choose, `Enter` to run. Shows each command's own key. Anything that moves the telescope or focuser asks for `Enter` twice. |
| `A` | Toggle Autofocus (start / abort) |
| `B` | Cycle Display FX (Normal → Boost ×2 → Boost ×4 → Stack 3/5/10) |
| `C` | Toggle Clipping Warning Overlay |
| `F` / `Shift+F` | Focus step: fine / coarse (uses the profile's step sizes) |
| `Shift+S` | Framing shot — boost gain and exposure for 5s, then restore |
| `F11` | Toggle fullscreen |
| `G` | Open GoTo panel |
| `Shift+G` | **Recall a past capture** — pick its file and the telescope goes back to it. See **Going back to a previous capture**. |
| `Ctrl+Shift+G` | After a recall, put the target back where it sat **in that capture's frame**. |
| `H` | Cycle Channel Mode (All → Red → Green → Blue → All) |
| `M` | Open Map (Sky Map) |
| `F11` *(in the Sky Map window)* | Fill the display the map is on, with no title bar. `Esc` or `F11` again to come back. See **Putting the Sky Map on its own screen**. |
| `Ctrl+Shift+Home` | **Bring every window back to the main screen.** Works from anywhere, even with the app in the background. See **The multi-monitor workspace**. |
| `Ctrl+Shift+End` | **Save the current arrangement** as the layout to open with. |
| `Shift+H` | Close or reopen the **histogram window**. Open at startup. |
| `Shift+Q` | Close or reopen the **sharpness window**. Open at startup. |
| `Shift+D` | Close or reopen the **status window** — capture health and free space on every drive. |
| `Ctrl+Shift+R` | **Report a problem to Claude** — describe what happened, press SEND, and carry on; the app gathers the evidence and sends it to Claude Code on this PC (3.3.44). Also REPORT on the left rail. |
| `Shift+W` | Close or reopen the **weather window** — the whole reading (cloud, temperature, humidity, wind, gusts, dew, pressure, seeing, verdict), where it came from and how old, and ↻ REFRESH (3.3.45). `W` alone is still the advisories. |
| `Shift+R` | Close or reopen the **SKY RUN window** — start, watch and stop the unattended test night (3.3.45). `R` alone is still the reticle. |
| `Shift+A` | **ADC alignment window** — for dialling in a dispersion corrector. Inert unless *ADC fitted* is ticked on the READY screen. Until phase 132.7 this key also started or stopped autofocus. |
| `Shift+V` | **Share view** — the image alone, filling a second screen, for people who are not you. See **The share view**. |
| `Ctrl+Shift+V` | Share view in and out of a **movable window**. Works from the control screen, which is the point. |
| `Shift+M` | **A second sky map**, on the control screen, so the first can stay where other people can see it. |
| `N` | Night vision — red preview and HUD. Remembered between sessions |
| `W` | **W**atch — occasional advice from the local AI. Off by default |
| `L` | **L**evels — display stretch on/off, for comparing the two |
| `Shift+L` | Automatic display stretch on/off: the view stretched for the eye on the target (on by default). See [Display stretch](#display-stretch) |
| `P` | Pause / Resume recording |
| `Q` | Toggle Quality Bar Graph |
| `R` | Reticle: off → crosshair → circle → rings → box → off |
| `S` | Save Snapshot (PNG) |
| `U` | **Clear the view** — hide everything drawn over the image except the reticle. Remembered between sessions |
| `V` | Toggle RGB Align |
| `X` | Toggle Pixel Value Readout |
| `Z` | Cycle Zoom (FIT → 50% → 100% → 200%) |
| `K` | Stac**K** the last finished recording. A SER stacks hidden; a FITS file opens in AutoStakkert's window |
| `Shift+K` | Open the last finished recording in the program Windows has for that file type |
| `J` | **J**upiter: the central meridians now, the Red Spot's transits and the moons' transits, shadows, occultations and eclipses for the next two days, then the mutual events coming up for IMCCE's PHEMU campaign, all judged for this site. See *Jupiter's Red Spot, moons and shadows* and *Mutual events* below |
| `Shift+J` | Record the next mutual event, with the clock checked and the settings locked; again to cancel. Asks twice |
| `↑↓←→` | Jog mount (hold to slew) |
| `+` / `-` | Fine focus step |
| `[` / `]` | Coarse focus step |
| `Esc` | Close popup panels |
| Mouse wheel | Zoom preview in/out |
| `Ctrl` + click on the image | **Put that spot in the middle.** Needs a guide calibration first; it says so if there is none. See **Centring by clicking**. |
| `[HW]` toolbar button | **Connect or disconnect the camera, focuser and mount.** Shows how many are connected. See **Connecting hardware without restarting**. |

---

## Connecting hardware without restarting

The `[HW]` button on the toolbar says how many of the three devices are
talking to the app — `3/3`, `2/3`, `1/3`, or `OFF` — and it is bold whenever
anything is connected.

Press it and it does the sensible thing for the state you are in:

- **Not everything connected** → it connects what is missing, and leaves alone
  what is already working. This is the one you want when the mount was
  switched on late, or a cable was in the wrong socket, or the camera dropped
  off USB mid-session. You do not lose the camera to go and fetch the mount.
- **All three connected** → it disconnects all three, in the order that is safe:
  recording stopped first, then tracking, then the mount, the focuser and the
  camera. This is the end-of-session press, before you unplug anything.

If a capture is recording when you press it to disconnect, it asks first —
disconnecting closes the file where it is.

**Read the message, not the button.** Afterwards the notification bar says what
actually happened, and it names anything still missing: "Connected camera,
focuser — still no mount" means the telescope is not going to track, however
much of the rest came up. The bar is amber unless all three are connected, for
exactly that reason.

The mount is re-chosen each time rather than merely reopened, so a reconnect
can find CPWI if it has since been started, or the hand control on a different
COM port than last time — Windows moves those when a cable goes into a
different socket.

---

## Centring by clicking

Hold `Ctrl` and click anything on the image, and the telescope moves so that
spot is in the middle of the frame.

It is not a guess. The app learned how the image responds to the mount during
**guide calibration** — it pulsed the mount east, watched which way the picture
went, then pulsed north and watched again. That measurement already contains
everything awkward about the arrangement: which way round the camera sits in
the focuser, the mirror flip of the optical train, and the direction each axis
runs. A click can therefore be exact in a way that jogging by hand is not,
because a jog moves the mount and you have to work out for yourself which way
that pushes the picture.

**What it needs**

- A **guide calibration**. Run it once per session — it is part of the
  tracking workflow, and if there is none the notification says so and nothing
  moves.
- A **connected mount**.
- The tracker **not already centring**. If the tracking loop is working on the
  target, two things would be steering at once, so the click is refused.

**What it will not do**

- **Long moves.** Beyond about ten seconds of pulsing — roughly an arcminute —
  it refuses and says to slew instead. It does not send a shortened pulse,
  because that lands somewhere nobody asked for, and at this plate scale the
  target can leave the frame entirely.
- **Tiny moves.** A click within a few pixels of the middle does nothing.

Every attempt, including a refused one, is written to the session log with the
pixel clicked and the pulse lengths used, so a move you did not expect can be
read back afterwards.

---

## Panels Overview

### Toolbar (top strip)
Across the top of the preview window, in three groups: acquisition, view, and
capture format.

| Button | Key | Function |
|--------|-----|----------|
| `[CENTROID]` | — | **Acquire the planet**: find, calibrate, centre, lock. Press again to cancel. |
| `[LOCK]` | — | Manual override — force the calibration step |
| `[ROI]` | — | Crop the sensor around a locked planet (not a freehand ROI — drag a box on the preview for that) |
| `[COLOR]` | — | COLOR / RAW debayer. **RAW shows the raw greyscale sensor data** — useful for judging focus, but the image goes monochrome |
| `[MAP]` | `M` | Toggle Sky Map window |
| `[GOTO]` | `G` | Open GoTo panel |
| `[RETICLE]` | `R` | Toggle crosshair reticle overlay; lit, it names the shape |
| `[CLIP]` | `C` | Toggle clipping warning (red flash on saturated pixels) |
| `[GH]` | — | Ghost/frame-stack display FX |
| `[?]` | `?` | Toggle this Help window |
| `[AI]` | — | Toggle the AI assistant chat panel |
| `[FOLLOW]` | — | **ROI following.** While a target is locked on a crop, the app moves the ROI on the sensor to hold it, and the mount slowly brings the window back. The setting is saved. When the target fills the frame, as the Moon and the Sun do, it holds a patch of the surface instead of a disc, and says so. If the camera does not apply moves during capture, it stops and tells you |
| `[DEROT]` | — | **Derotation limit.** Each recording of a planet or the Sun is told how long it has before rotation blurs detail, from tonight's apparent size and the aperture. Jupiter on this telescope is about three minutes. `WARN` warns when that passes, `STOP` ends the run there. Saved. The Moon has no limit |
| `[RUNS]` | — | **A sequence of runs.** Repeats focus, record and a gap, so a night's runs do not each need a press of RECORD. Each run stops at the derotation window whatever `[DEROT]` says; a target with no window uses the frame or time limit. It focuses first and every third run, with AE on. Before each run it checks the target is in frame, above 20° and that the disk has room. Lit, it shows the run under way. Press it again, or RECORD, to stop; the file is closed. Auto-stack is skipped during a sequence. It also waits, up to 3 minutes, for the seeing to come within 80% of the best it has reached tonight, before a focus sweep and before a run; after that it goes anyway, because a poor run beats none. When it ends it writes `autorun_<date>_<time>.txt` beside the captures, listing the runs best first, so you know which to stack. On Jupiter (phase 144) a focus sweep that falls due while the Red Spot is within 50 minutes of the meridian, or a moon or its shadow is crossing the disc, waits until that ends, 30 minutes at most, while the runs go on; the run list shows what was on the disc for each run, in brackets. Set in `rig.ini`: `AutorunRuns`, `AutorunGapSec`, `AutorunRefocusEvery`, `AutorunMinAltDeg`, `AutorunSeeingPct` (0 never waits), `AutorunSeeingHoldSec`, `AutorunPrimeDeferMin` (0 never waits) |
| `[STACK]` | `K` | Stack the last finished recording. A SER runs hidden and exits; a FITS file opens in AutoStakkert's window |
| `[DASH]` | — | **Open the browser dashboard, already signed in.** It connects to this PC by itself; see **The browser dashboard** |
| `[STATS]` | — | **Every session at once.** Reads all session logs, capture records and the pointing map, and opens one page in the browser: totals, each target, each night, and whether the pointing map helps. Real nights and emulator sessions are shown apart |
| `FULL` | — | **Whole sensor, 3840x2160** - the widest field, and the way back from any crop |
| `1920` | — | 1920x1080 |
| `1280` | — | 1280x960 - what `[ROI]` crops to |
| `640` | — | 640x480 - smallest frame, highest frame rate |
| `8b` | — | 8-bit capture (RAW8). Half the data; the normal choice for planets |
| `16b` | — | 16-bit capture (RAW16). Headroom for stacking, twice the data rate |
| `EXIT` | — | Graceful shutdown (confirms if recording) |

Buttons highlight (bold) when their feature is active. The live resolution and
bit depth each carry a dot, so you can see at a glance what you are recording.

### Getting the field back (capture format)

`FULL` exists because a crop used to be a one-way door. `[ROI]` and a dragged
box both narrow the sensor read-out, and until v1.2.3 nothing widened it
again - the only way back to a full field was to restart the app. Press
`FULL` and you are back.

Resolution changes keep whatever is in the **middle** of the frame in the
middle of the frame, so widening the field to find a planet that has drifted
out does not also move the field somewhere else.

Changing either setting **closes the current recording**, because a SER file
cannot change format part-way through and the rest of it would be unreadable.
Set the format before you start, not during. If you were recording, a new
file is opened at the new format.

**Which bit depth?** 8-bit for planets: the frames are half the size, so the
disk and USB keep up at high frame rates, and stacking hundreds of frames
recovers the precision anyway. 16-bit when a single frame has to carry the
detail - long exposures, faint targets, or anything you intend to stretch
hard afterwards. The data rate doubles, so watch `LOST` in the status bar.

**Not the same as USB bandwidth.** That is a separate slider in the CAMERA
section of the control panel (40-100%), and it controls how hard the camera
pushes the USB bus, not how much data each frame holds. Lower it if frames
are dropping.

---

## Control Panel Sections

All sections are collapsible — click the section header to expand/collapse.

### IMAGE
- **DARK / FLAT / CLEAR** — Build or clear calibration masters. See [Dark and flat frames](#dark-and-flat-frames).
- **Black / White sliders** — Display stretch. See [Display stretch](#display-stretch).
- **ZOOM: FIT / FULLSCR** — Switch between fit-to-window and fullscreen preview.
- **RGB ALIGN** — Toggle atmospheric dispersion correction on the display. It works out the colour spread from the mount's altitude (the Edlen refractivity model, over your filter's band) and moves blue and red back onto green, together by exactly that spread (3.3.41; before, it moved them 1.5 times too far, which looked like a double image). It moves them along the zenith's direction in the frame, set by rig.ini `CameraZenithAngleDeg` (default 180: zenith toward the bottom). With `ADCFitted=1`, only the Manual slider applies, because the ADC is doing the correcting. Display only — SER data is unaffected.
- **Manual slider** — Fine-tune the RGB align offset ±20px on top of the auto value. Center position = 0px offset.
- HUD shows `RGB ALIGN +N px` in amber when active.
- **[FX] NORMAL** — Cycle Display FX modes (see Display FX section below). Also `B` key.
- **[N] NIGHT** — Toggle Night Vision Mode (red-tinted preview for dark adaptation). Also `N` key. HUD shows `NIGHT VISION` in red when active.
- **[H] CH: ALL** — Cycle single-channel display mode: All → Red → Green → Blue. Shows selected channel as grayscale. Display only — SER data unaffected. Also `H` key. HUD shows channel label in cyan when active.

### CAMERA
The **Exposure** slider covers **0.001 ms to 60 ms**, which spans planetary
work including the auto-exposure assistant's 50 ms ceiling. Longer exposures
are set through the profile editor, which accepts up to 60 s.

- **Gain** — ASI678MC gain (0–600). HCG threshold at 182 — above this gain, the camera switches to high-conversion-gain mode.
- **Exposure** — Exposure time in ms (0.1ms–10000ms).
- **Gamma** — Camera gamma (1–100, 50 is neutral). Greyed and marked `n/a` if the camera does not list the control.
- **WB Red / WB Blue** — White balance for the red and blue channels.
- **USB** — USB bandwidth limit (40–100%). Reduce if you see USB errors during recording.
- **AE OFF / AE ADV / AE AUTO** — Auto-Exposure modes:
  - `AE OFF` — Manual control only.
  - `AE ADV` — Advisory mode: suggests gain/exposure changes but does not apply them.
  - `AE AUTO` — Automatically adjusts gain and exposure to bring the planet's highlights to 185–215 DN. It raises exposure to about 20 ms, then gain, so a dim target keeps a short exposure. The sliders follow what it sets.
  - It measures the planet itself, not the whole frame (phase 140.1): the brightest part of whatever stands clear of the sky, and how much of *that* is clipped. It used to read the whole frame, which on a small planet is mostly sky, so it kept adding light. With nothing in the frame it holds and shows `AE: held for no target in the frame`.
  - Each planet has an **exposure band** (phase 140.2), a shortest and longest exposure. AUTO lengthens the exposure up to the band's longest before it adds gain, and to dim it lowers gain first and shortens the exposure no further than the band's shortest: gain brightens the picture, but only exposure collects light. Starting bands: Jupiter 4-12 ms, Saturn 8-25, Mars 3-10, Venus 1-5, Mercury 1-8, Uranus 10-50, Neptune 15-50, the Moon 1-10. They are starting points, not measurements; set your own in the profile's `.ini` under `[exposure_band]` (`floor_ms`, `ceiling_ms`) after the exposure ladder on a clear night.

Every slider has a box beside it. Type a value and press Enter; Escape restores the current value, and a value out of range clamps and shows the clamp.

AE holds during a recording. **AUTO FOCUS** turns AE to AUTO and opens this section, waits up to 20 seconds for AE to settle, holds it for the sweep, and leaves it on afterwards. All these settings are written to the record beside each capture.

### FOCUSER
- **Position readout** — Current EAF stepper position (0–500000).
- **`<< < STOP > >>`** — Step buttons: 1K / 10 / manual / 10 / 1K steps.
- **Input box** — Type a target position and click `GO TO` for a direct move.
- **TEMP** — Current EAF temperature in °C.
- **AUTO FOCUS** — Runs the built-in V-curve autofocus routine. Steps outward, measures sharpness at each position, fits a parabola, moves to the computed best focus. Abortable mid-run.
- **ADC: severity (N.N")** — Live atmospheric dispersion measurement. Computed from mount altitude using the Edlen 1966 model, with air pressure derived from your site elevation (T=288K). Shows NONE / MINOR / MODERATE / SIGNIFICANT / SEVERE. Use as a guide for when to apply an ADC or use RGB Align.
- **Filter: name (low–high nm)** — The filter the line above assumes is on the camera, and the wavelength band it computed the dispersion across. This is **configured, not detected** — the app has no way to see what is screwed onto the camera. If it is wrong, the ADC figure above is wrong with it, by up to 4×. Change it in **SETUP**. See [Filters](#filters).

### TELESCOPE
- **Mount name / COM port** — Celestron 8SE over the NexStar serial protocol
  (not ASCOM). The port is auto-detected; the one in use is shown here.
- **Az / Alt / RA / Dec** — Live mount coordinates. Azimuth was read from the
  mount and discarded before v1.1.6, so these showed `--`.
- **Cord wrap** — Appears beside the azimuth once the mount has turned more
  than 30° since the cables were dressed, e.g. `184.2°   wrap +210°`. See
  *Cord wrap* below.
- **CABLES OK** — Press after dressing the cables. Zeroes the wrap count, which
  every later warning is measured from.
- **SETUP** — Opens the rig setup view: a plan of where you stand relative to
  the mount, the keep-out sector and the wrap limit, drawn so the stored
  numbers can be checked against the actual garden.
- **READY** — Reopens the readiness check at any time. It appears by itself
  at startup only when something is wrong; this is how you get back to it
  after fixing whatever it flagged.
- **Acquisition status** — While acquiring, this line shows the stage and what
  is holding it up, e.g. `SEARCHING - planet too faint to detect (frame peak
  3%, needs above 8%)`. It updates continuously, not only when you press
  something.
- **MOUNT CTRL** — Opens the floating mount controller (always-on-top).
- **TRK ON/OFF** — Tracking state toggle.
- **PLANET LOCK** — Engages planetary centroid lock mode.
- **TGT / GOTO** — Type a target name (e.g. Jupiter) or coordinates and slew.
- **POINT SOLVE → SYNC** — Run ASTAP plate solve on the current frame and sync mount coordinates.

### CAPTURE
Holds the target profile and the recording settings together. They used to
be two sections, PROFILE and CAPTURE, which described the same thing from
opposite ends — and because the profile's copies were never pushed into the
boxes, the boxes silently won every time. One section, one set of values.

- **Target** — The profile, with `LOAD` / `SAVE` / `EDIT`. Picking an object
  on the sky map or in GoTo loads the matching profile automatically; see
  [Profiles](#profiles).
- **Prefix** — Base name for output files (e.g. `Jupiter_`). Filled in by a
  profile load, and read back by `SAVE`.
- **Type dropdown** — Frame type: `Light` / `Dark` / `Flat` / `Bias`. Inserted in filename.
- **Format dropdown** — Beside Type: `SER` or `FITS`. SER is the default and is
  what AutoStakkert stacks. FITS writes one cube per recording, with the
  observation details in its header and each frame's time and sharpness in a
  table after the image. The choice is saved, and a recording already running
  keeps the format it started with. `K` and `[STACK]` open a FITS file in
  AutoStakkert's own window, where you stack it by hand, because AutoStakkert's
  hidden run does not process FITS. Auto-stack is SER only. `Shift+K` opens the
  last recording in whatever program Windows has for that file type.
- **Output directory** — Where recordings are saved (default: `D:\RadukCapture\captures`).
- **TO SER** — At the end of the output-directory row. Converts FITS
  recordings to SER, for PIPP and AutoStakkert: pick one or several `.fits`
  files, and each gets a `.ser` beside it. The conversion runs in the
  background, and its progress shows on the notification bar.
  - Press **TO SER** again to stop. The file in progress is removed; files
    already done are kept.
  - It will not start during a recording. A recording started mid-conversion
    stops it, because both need the disk and the recording comes first.
  - A FITS recording that was never closed (the app went down mid-run) has a
    header claiming no frames. TO SER counts the whole frames in the file and
    keeps them.
  - Each SER is as large as its FITS, so check the free space first.
  - The same conversion is `build\Release\fits2ser.exe` on the command line.
- **PLAY** — Just before TO SER. Opens a `.ser` in the PLAY window, so you
  can look through a recording before PIPP or AutoStakkert see it. You can
  also drop a `.ser` on the main window or on PLAY itself.
  - The slider doubles as a quality trace. Every frame is scored in the
    background, using the same target finder and planet focus measure that
    autofocus uses. Tall bars are the sharp stretches. Red ticks along the
    bottom are frames with no target in them: the planet drifted out, cloud
    came over, or the camera recorded nothing.
  - **Space** play/pause · **←/→** step (Shift ×10, Ctrl ×100) · **Home/End** ·
    **↑/↓** speed · **B** best frame · **S** auto stretch on/off ·
    **C** colour/RAW · **O** open · mouse wheel zoom, drag to pan, double-click
    to fit · **F11** full screen · **Esc** close. Click or drag on the trace to
    scrub.
  - The arrow keys step frames here. They do not jog the mount while this
    window has focus.
  - It reads recordings the app never closed (it counts the frames from the
    file size) and 16-bit files from other programs whose byte-order flag is
    wrong. FITS recordings need converting with TO SER first.
- **Frames / Sec radio** — Capture limit mode: by frame count or by time. Also
  set by a profile load — a time-limited profile selects **Sec** for you,
  which it previously did not, so it loaded its seconds into the box and
  still stopped on the frame count.
- **MIN SEEING / GATE** — Quality gate: only record frames where seeing score ≥ threshold. Enable with the GATE checkbox. The setting survives collapsing the section — until v1.2.3 it did not, and collapsing CAPTURE silently switched the gate off.
- **▶ RECORD** — Start recording. Button turns red while recording. Press again to stop.
  - Press `P` while recording to **pause** (amber `⏸ PAUSED` state). Press `P` again to resume.

### STATUS
- **HDD** — Free disk space in GB.
- **BUF** — Write buffer fill level (slots used / total).
- **FPS** — Live frames per second.
- **LOST** — Frames dropped (write buffer overflow).

### HISTOGRAM and TEMPERATURE — moved to the preview (v1.2.4)

Both left the control panel. They are now permanent panels on the live
image itself, one either side of the target, and there is nothing to
expand — see **Side panels** under Preview Overlays.

They moved because they were the wrong shape for this panel: every
section here starts collapsed, so the two readouts you want to watch
continuously were the two you had to open first, and opening them
pushed everything else down. The dark sky either side of a planet was
going spare.

### NOTES  (saved with profile)
- Free-text notes for the current session. **Saved and restored with the active profile** — click SAVE on the profile to persist, click LOAD to restore. Notes are also editable in the profile EDIT dialog.

---

## The multi-monitor workspace (v1.2.4)

The app counts the displays at launch, loads the layout for that count,
and opens with each window already on the screen it belongs on. Nothing
to arrange by hand at the start of a session.

**Layouts are keyed by how many screens are attached**, not by one saved
arrangement. Five screens get the five-screen layout; plug in only the
laptop panel and it uses the one-screen layout instead. If there is no
layout for the exact number found, it steps *down* to the largest one
that fits, so a five-screen layout still does something sensible on
three and everything ultimately falls back to the one-screen layout.
This matters on a rig whose screens come from two graphics cards: the
set changes when a cable or a driver does, and a single remembered
arrangement would be wrong the moment it did.

The layout lives in **`workspace.ini`** next to the program, written on
first run so it can be edited rather than guessed at:

```ini
[monitors.5]
main   = \\.\DISPLAY5
skymap = \\.\DISPLAY4 maximized
goto   = \\.\DISPLAY2
mount  = \\.\DISPLAY1
help   = \\.\DISPLAY3
aichat = \\.\DISPLAY3
```

Each line is `role = display [maximized] [x,y,w,h]`. The sizes, if given,
are fractions of that screen, so a layout written on one monitor size
still means something on another. **A role with no size is moved, not
resized** — the mount and GoTo panels place their controls at fixed
positions, so stretching one would leave its buttons in a corner of an
empty window. Only the map and the preview actually scale.

**More than one window can name the same screen.** Five screens means
one for the main window and four to share between five panels, so two
of them double up — help and AI chat by default, both being panels you
read occasionally rather than watch. Change the file if you would
rather pair a different two.

### Arrange it by hand, then keep it

Drag the windows wherever you want them and press **`Ctrl+Shift+End`**.
That records exactly where everything is — to the pixel, including which
windows are maximised — and the app opens that way from then on.

This is the intended way to set the layout up. A generated default
spreads the windows one per screen because it has nothing better to go
on; it will never guess an arrangement you built deliberately. Saved
entries use the `@x,y,w,h` form, which is exact screen coordinates
rather than fractions, so what you get back is what you had.

The layout in use on this rig gives the histogram window DISPLAY2 and
the sharpness window DISPLAY1, with the map on DISPLAY4, the main window
and mount controller on DISPLAY5, and help, AI chat and GoTo sharing
DISPLAY3. A copy lives in `docs/workspace-5monitor.ini`.

### When a screen comes or goes (v1.2.6)

Unplug a monitor, let one sleep, change a resolution, or have a driver
reset the topology, and the app reapplies the whole saved layout by
itself. No restart, no keystroke.

It **re-places** rather than merely rescuing. A monitor waking from
sleep is by far the commonest case, and its windows should go back
where they belong rather than pile onto the main screen for you to sort
out by hand. Anything still unreachable afterwards is brought home and
the notification bar says so.

What it did goes to the session log, because this fires during exactly
the moments the rig is most fragile — driver installs, resolution
changes, a panel waking — and a 3am misbehaviour has to be diagnosable
the next morning.

A window dragged to a screen with a different scale factor is resized to
match it. Five panels of mixed vintage will not share a scale, and
without this a window keeps its old pixel size and either overflows the
smaller screen or shrinks on the larger one.

### Which GPU the preview is on

The rig drives five screens from two cards. The live preview presents
through whichever adapter drives the screen it is on, so moving it to a
screen hanging off the other card costs a cross-adapter copy on every
single frame — for nothing, since both show the same picture.

The app reads which card drives each screen at startup and names it in
the log:

```
[LAYOUT]   \.\DISPLAY5  1920x1080 at (0,0)  [primary]
[LAYOUT]       driven by NVIDIA GeForce RTX 2060
[LAYOUT]   \.\DISPLAY2  1920x1080 at (-562,1080)
[LAYOUT]       driven by NVIDIA Quadro P1000
```

If the preview ends up on a card other than the primary's, you get one
warning in the notification bar — at startup, and again if a display
change moves it. Once each, never per frame.

This is also why the generated default layout pins the main window to
the primary screen and never moves it. Every other window can go
anywhere; the preview is the one with a cost attached.

### Panels snap together (phase 134.1)

Drag one panel near another or near a screen edge and it snaps flush, so
panels line up without fiddling. This applies to Histogram, Sharpness,
Temperature, Status, ADC, both sky maps, GoTo and the SER player. Resizing
snaps the edge you drag in the same way. No panel goes smaller than 240 by
120. **Hold Shift** while dragging to place a panel freely. `PanelSnap=0` in
`rig.ini` turns snapping off.

**Panels that touch form a group** (phase 134.2). Drag any one of them and
the whole group moves, snapping as one block. Drag the edge two panels share
and both resize, like a splitter, and the joint stays closed. To take a panel
out, **hold Shift as you start dragging it**. Once it is no longer flush, it
has left. A group spread over two monitors with different scaling moves one
panel at a time for now.

**F11 on a panel in a group fills the monitor with the whole group**
(phase 134.3). The panels keep their proportions, lose their title bars, and
leave no gaps, so the group reads as one instrument. Press F11 again to put
every panel back where it was. Esc stays the emergency stop, as everywhere. F11 on a panel that is not in a group still
makes that one panel fullscreen.

**The dock window** (phase 134.4a) holds panels as one window. Right-click
a panel's title, or press Alt+Space with the panel focused, and choose
**Dock**. The dock opens where that panel was. Each panel you add takes half
the space beside or below the others, and you drag the bars between panels
to share the space differently. F11 on the dock, or on any panel in it,
fills the monitor; press F11 again to return. To take a panel out, choose
**Undock** from its title menu or **Shift-drag** it away. Closing the dock
returns every panel to where it was. The dock snaps to other panels like a
panel.

**Dragging onto the dock** (phase 134.4b). With the dock open, drag a panel
by its title over the dock. A highlight shows where it will go. Near an edge,
the panel goes on that side of everything already docked, taking half the
space. In the middle, it is added as **Dock** adds one. Let go to dock it, or
drag away again to cancel. This works for a panel dragged on its own, so
Shift-drag one out of a group first. The first panel still goes in through
**Dock** on its title menu, because the dock is closed while it is empty.

**Ctrl+Shift+End remembers the dock too** (phase 134.5): which panels are
in it, how the space is split, its size and place, and whether it was
fullscreen. A group left in group fullscreen is remembered the same way.
Both come back at the next start. A panel you closed before starting stays
closed, and the dock lays out without it. This is kept in `dock.ini`, beside
`workspace.ini`. Delete it to forget the dock.

### Why a window can be stuck

A window can be on screen, fully drawn, and still impossible to move,
because the only part you can drag is its title bar — and that can end
up somewhere you cannot click. Two ways it happens on a multi-monitor
desktop:

- **Under a taskbar.** Monitors can each have one.
- **In the gap between screens.** Monitors arranged at different
  vertical offsets leave bands that belong to no display at all. On this
  rig there is a 19-pixel band between the bottom of one row and the top
  of the next, and the GoTo panel's title bar landed exactly in it.

So placement does not ask "is this window on the desktop" — the desktop
as Windows reports it is the *bounding box* of all the monitors, dead
space included. It asks whether the **title bar** overlaps some
monitor's usable area, taskbars excluded. Anything that fails is put on
the main screen instead, and the log says so.

If one gets stuck anyway, `Ctrl+Shift+Home`.

### If a window goes missing

Press **`Ctrl+Shift+Home`**. Every window comes back to the main screen
at a usable size, and anything that was maximised is maximised again.

This exists because a window placed on a monitor that is no longer there
is not minimised and not closed — it is invisible, focusable, and still
taking keystrokes, which looks exactly like a feature that stopped
working. The shortcut is registered with Windows rather than handled by
the app's own window, so it works whatever has focus, including when the
window you would otherwise click on is the one that went missing.

Two safeguards usually prevent you needing it. Every position is checked
against the screens that actually exist before it is used, and anything
that would land off every screen goes to the main one instead. Screens
are matched by device name rather than by position in a list, because
that order changes when the graphics drivers re-enumerate.

**The preview stays on the main screen** and is not moved by any layout.
The live image is drawn by whichever card drives its monitor, and moving
it onto a screen belonging to the other card costs a copy on every
frame. Until the app knows which screens belong to which card, leaving
it alone is the safe default rather than a considered choice.

---

## Putting the Sky Map on its own screen (v1.2.4)

The map has always been a window you could drag to any display. What it
had no way to do was **fill** one, and the reason was two window flags
rather than anything to do with monitors: it was created as a tool
window, which suppresses the minimise and maximise buttons outright,
and without `WS_MAXIMIZEBOX`, so the system menu's Maximize was greyed
and double-clicking the title bar did nothing. There was no "make this
fill the screen" control anywhere because Windows had been told not to
offer one.

Three ways now, all of which act on **whichever display the window is
currently on** — drag it where you want it first:

| | What it does |
|---|---|
| **Maximise button**, or double-click the title bar | Fills that display, keeping the title bar and respecting the taskbar. |
| **`F11`** | Fills that display completely, with no title bar and over the taskbar. `Esc` or `F11` returns it. |
| **Drag the edges** | Still works; the map redraws at any size. |

The map is also now in the taskbar and Alt-Tab, which it was not before
— useful once it lives on a screen of its own rather than on top of the
capture window.

Measured on this rig's five displays: `F11` with the map on DISPLAY2 at
(-504,1088) filled that screen, not the primary.

**The other floating panels do not get this**, deliberately. Mount
Controller, GoTo and Object Info lay their controls out at fixed
positions and have no sizing border, so maximising one would strand its
buttons in a corner of an empty window. The map and the preview are the
two surfaces that actually scale.

---

## Mount Controller (Floating Panel)

Open with `MOUNT CTRL` button or from the TELESCOPE section. Always stays on top.

**Tracking without the hand control (3.3.43).** After power-on, let the hand
control finish **Quick Align** (it needs no stars); from then on the app does
the rest:

1. GoTo the target from the GoTo panel.
2. Centre it roughly and press **★ ALIGN**: it syncs and switches tracking on.
   (`T` or TRACKING also switches it on.)
3. Press **PLANET LOCK**. It measures the mount's own tracking, then drives the
   axes itself; the line reads **TRK: APP (lock)**.
4. Fine-tune with the arrow keys (or the controller's stick), never the hand
   control: the lock re-sends the axis rates every quarter second, so a hand
   control press is undone.

The **TRK** line: `ALT/AZ` (the hand control tracks), `APP (lock)` (PLANET LOCK
drives), `OFF`, or `?` (the mode is not confirmed yet). Some hand controls
acknowledge a tracking command but do not answer the mode query right after
it; the app then says "tracking sent, confirming" and the position poll settles
it within about a second, logging "confirmed by the position poll" or "NOT
RUNNING (mount likely unaligned)". Before 3.3.43 that case was reported as
"mount refused tracking", and `T` could then switch tracking off when you meant
on; `T` now asks for on whenever the mode is not known.

- **N / S / E / W** — Hold to slew. Release to stop.
- **9× / FAST / MED / FINE** — Slew rate selector. **This also sets the
  speed of the arrow keys**, and it is currently the only place the rate
  can be changed — the main panel has no rate row. Before v1.2.1 the two
  panels each kept their own copy of the rate and never told each other,
  so picking FINE here highlighted the button but left the arrow keys
  slewing at 9×. If you tried to slow the arrow keys down and nothing
  happened, that was why.
- **STOP** — Immediately halt all movement (also `Space` key).
- **★ ALIGN** — One-point align on a centred object: syncs the mount to the
  object's computed position **and** switches tracking on. Pick the target from
  the list; planets are offered first. The result is reported honestly —
  "Aligned ... tracking ALT-AZ", "Synced ... mount refused tracking", or
  "Mount refused the sync".
- **PARK** (beside ALIGN, 3.3.39) — Slews to the park position you set in
  `rig.ini` as `SkyParkAltAz=az:alt` (for example `180:20`), converted to
  RA/Dec for that moment and sent with the ordinary GoTo, then turns
  tracking off. The mount has no park of its own, so there is **no default
  position**: with the key unset, PARK refuses and says so. It also refuses
  outside the slew bounds (`SkySlewAltMax`, `SkySlewAz`), with the mount not
  connected or not aligned, or with no site set, and nothing moves. A park
  below your horizon profile is allowed, since that is for imaging over the
  trees. A slew that takes over 200 s is cancelled. Jogging or slewing more
  than a degree away ends the parked state. Dash's PARK (two taps) and the
  sky run park the same way.
  The button shows the state (3.3.45): **PARKING...** while it slews,
  **PARKED** once tracking is off, **PARK FAILED** (with the reason in a
  notice), or **PARK (unset)** when `SkyParkAltAz` is not set.
- **RA / Dec readout** — Live coordinates updated every 500ms.
- **Motion plot** (bottom) — see below.

### Motion plot (v1.2.1)

A trail at the bottom of the panel showing where the telescope has
actually moved over the last ten seconds, with the current speed:

```
CAMERA  4.8"/s   ±8"
```

The number turns green while something is moving. ±8" is the width of
the plot in arcseconds — it rescales itself, so read the scale before
judging how far the trail has travelled.

**What it is for.** At FINE rate a short press moves the telescope a few
arcseconds. Nothing on screen changes enough to notice, so a command that
did nothing and a command that did something small look identical. This
tells them apart: press an arrow key and either the trail moves or it
does not.

**The two sources are not equivalent, which is why the plot says which
one it is using:**

| Source | Where it comes from | Resolution |
|--------|--------------------|------------|
| `CAMERA` | The planet's position in the image, every analysed frame | Fractions of an arcsecond, effectively instant. Preferred. |
| `MOUNT` | Alt/az read back from the mount over the cable | Polled every 500ms, or 2s while recording — a short nudge appears as one jump, not a line. |

`CAMERA` is used whenever a planet is being detected; the plot falls back
to `MOUNT` otherwise. A switch to `MOUNT` is itself useful: it usually
means the planet has left the frame, which is worth knowing at once
rather than a minute later.

A trail drifting steadily with no input from you is normal when tracking
is off, and is exactly what tracking exists to cancel.

---

## GoTo Panel (`G` key)

- **Object list** — 110 Messier objects + 7 planets, sorted by altitude (highest first).
- **GOTO** — Slew to selected object.
- **Manual RA/Dec** — Enter coordinates directly and slew.
- **Plate Solve → Sync** — Runs ASTAP on the current frame, computes actual pointing, syncs mount.
- Objects below the horizon are shown greyed out.
- **Tonight tab** — Objects by imaging time left tonight. Tick planets and stars, then PLAN TONIGHT and RUN TONIGHT: see *A night of planets and stars*.

---

## Preview Overlays

### Clearing the view — `U` (v1.2.6)

`U` hides everything drawn over the image **except the reticle**: both
side panels, the sharpness graph, the ADC arrow, the caption, the zoom
hint, the mode labels, the pixel readout and the LIVE status line. Press
it again to bring them back. The setting is saved, so the app opens the
way you left it.

The reticle stays because it is the one overlay you look *through* the
image with rather than read off it. Clearing the view to judge focus or
contrast is exactly when you still want to know where centre is.

Two reasons to reach for it:

- **Judging the image.** Numbers next to an image change how you read
  the image. With five screens the readouts already have screens of
  their own, so on the preview they are a second copy sitting on top of
  the thing they describe.
- **Showing someone.** Handing over a view of Saturn is better without
  a HUD across it.

It is `U` for **u**ncluttered, and it takes either case — this is a key
you reach for in the dark, and caps lock is not a mode anyone tracks at
2am. It replaced an earlier `Shift+O` that only covered the side panels
and the sharpness graph, which meant pressing it left the caption and
HUD line still over the image and the view not actually cleared.

### Side panels — histogram and temperature (v1.2.4)

Always on, one either side of the image, low down and sharing a
bottom edge — just above the sharpness graph on the left and the ADC
readout on the right. `U` hides them along with the rest.

They were vertically centred until v1.2.6, which put them level with
the target: a planet is centred too, so at any real zoom the two panels
were the things closest to the thing being judged, one either side of
it. Low and bottom-aligned leaves the whole middle band of the image
clear and reads as one row of instruments rather than two floating
boxes. At a very short window they stop short rather than ride up into
the caption.

**Left — histogram.** One row per Bayer channel, R, G and B, each with
its own bar chart, a white tick at the channel mean and the **clipped
percentage** on the right. The percentage is the number to watch: it
turns amber past 0.1% and red past 1%. Below the rows is the **seeing
score** out of 10.

Two details worth knowing:

- Bins 0 and 1 are not drawn. On a planetary frame that is sky, and it
  is most of the pixels.
- **Counts are on a log scale.** A planetary frame holds orders of
  magnitude more sky pixels than planet pixels, so scaled linearly
  everything except the sky spike came out under one pixel tall and the
  rows read as empty. The control panel version had the same problem
  and it mattered less there, collapsed behind a header. Log scaling
  shows the distribution; the price is that read noise is visible too.

Because the three channels share a scale you can read the **colour
balance** straight off how far each row extends to the right — on
Jupiter, R reaches furthest and B stops soonest.

**Right — temperature.** Camera and focuser temperature over the last
120 samples, about a minute: camera in green, focuser in amber. The
range in use is printed bottom-left of the panel and the current
readings bottom-right. The vertical range never shrinks below 2°C, so
a steady night reads as a flat line rather than being magnified into
apparent swings.

Both panels hide themselves if the window is too narrow to hold them
without covering the target.

### The share view (`Shift+V`, v1.2.7)

For a screen other people are looking at.

The main window is built for you: controls, histogram, temperature,
sharpness, reticle, a status bar and a HUD. That is the right window
to work in and the wrong one to point at a crowd, because almost none
of it is the planet.

`Shift+V` opens a second window that is the planet and nothing else.
It fills whichever screen it is on, borderless, with the image centred
and never stretched. Along the bottom: what you are looking at, then
one quiet line of scope, camera, exposure, gain, frame rate and how
high the target is. A red **REC** dot in the corner while you are
recording, so nobody has to ask.

Nothing on it is clickable. It will not take focus from the control
screen when you open it, which matters — the arrow keys jog the
mount, and a window that quietly captured them would be a hazard
rather than an inconvenience.

**Getting it back into a movable window:** `Ctrl+Shift+V`, from the
control screen. It fills its screen borderlessly, so there is no
caption to drag and no close button, and it does not take focus on its
own — which is why the way out is a key you press where you are
standing rather than one you press over there. Once it is windowed it
has a normal caption and edges, so drag and resize it as usual, and
`Ctrl+Shift+V` again puts it back. Whichever state you leave it in is
the state it comes back in next time you open it.

A reminder of all this appears across the top for a few seconds
whenever the view goes fullscreen, then fades so the screen is clean.

If you click on the view it takes focus, and then `F11` fills and
unfills the screen and `C` hides the caption for photographing it.
Those two keys are the only ones this window takes — everything else,
`Escape` above all, still goes where it always goes.

**Where it opens.** On whatever screen your saved layout puts it — it
is an ordinary workspace role called `share`. Drag it where you want
it and press `Ctrl+Shift+End`. Until you do, it takes a generated
default, which on a five-screen rig is the first spare screen after
the sky map's.

**What it costs.** Close to nothing. It is handed the finished image
the preview has already built rather than working one out for itself,
it shows at most 30 frames a second however fast the camera runs, and
when the window is closed the whole feature is one branch per frame.
A star party is still an imaging session and the frames still matter.

**What it does not do.** It ignores the `Z` zoom — that is a tool for
inspecting pixels, and the share view always fits the whole frame to
the screen. For a 640×480 crop on a 1920×1080 monitor that is already
a 2.25× enlargement.

**If you want the moons in it,** you need the field of view, which
means the full sensor. Press **FULL** on the toolbar. At 1280×960 the
field is 4.3 arcmin and even Io falls outside it; the whole sensor is
about 14 arcmin and holds Io, Europa and Ganymede, with Callisto
drifting in and out as it swings around.

> **An earlier version of this page warned that the full sensor is
> slow to preview. That was wrong** and the measurement behind it was
> misread — the figure quoted was the camera's frame rate, not the
> preview's, and the cause was a cost that exists only in the
> emulator. On the real camera the whole sensor previews at the rate
> the loop runs. Crop for frame rate when you want frame rate, not
> because the display cannot keep up.

### Reading the histogram (v1.2.7)

Each row is one colour channel. The bars are the distribution of pixel
values, on a **log** scale — a planetary frame is mostly sky, so on a
linear scale the sky spike is the only thing tall enough to see and
the planet works out under one pixel high.

**The number on the right is the PEAK LEVEL**, as a percentage of full
scale. It is the brightness of the brightest real thing in that
channel, which on a planet is the planet. That is the figure you set
exposure by: wind the gain or exposure up until it reaches about three
quarters, and stop before any channel reaches the end.

The bright vertical mark on each bar sits at that same level, so the
picture and the number explain each other. The faint grey tick is 75%
— where you are aiming.

Colours, which match the advice line underneath exactly:

| Reading | Meaning |
|---|---|
| **Red** | Clipping. Pixels are stuck at the top and that detail is gone for good |
| **Amber, 84% and up** | At the limit. A moment of good seeing will clip it |
| **Green, 50–84%** | Good |
| **Amber, 12–50%** | Under-exposed. Usable, but you are giving away range |
| **Grey, under 12%** | Far under — the planet is near the noise floor |

Comparing the three numbers is how you read colour balance. On Jupiter
through an 8SE you would expect red highest and blue lowest; if blue
is far behind, that is atmospheric extinction and altitude, not the
camera.

> **It used to show something else.** Before v1.2.7 this column was the
> *clipped fraction* — the proportion of pixels stuck at 255. That is
> a good alarm and a useless readout, because on any correctly exposed
> frame it reads 0.0%, which is what three rows of a working rig looked
> like. Clipping has not been dropped: a clipped channel's peak is at
> the top by definition, so it reads 100% and turns red.

### The histogram window (`Shift+H`, v1.2.4)

The same per-channel histogram as the preview panel, in a window you can
put on a screen of its own. `F11` fills that screen, `Esc` closes it.

It is the identical display, not a second implementation — one piece of
code draws both, so the log scale, the skipped bins and the clip
colouring cannot drift apart between them. What the window adds is room:
rows a third of a screen tall instead of 24 pixels, where the shape of
each channel reads from across the room and the gap between R, G and B
is obvious rather than inferred.

The small panel on the preview stays as it is, for working on one
screen or with the preview full screen.

`Shift+H` rather than a letter of its own, the same way `Shift+S` is the
framing shot — and `H` already cycles the channel display mode, which is
the same question asked a different way.

### What the histogram window adds below the bars (v1.2.6)

Three single lines under the three bar charts. The copy drawn on the
preview has none of them — it is 104 px tall and already full — so this
is the stand-alone window only (`Shift+H`).

**SEEING n/10.** Unchanged.

**Exposure headroom.** How much more exposure the channel that clips
*first* could take, in stops. That channel is the one that decides the
exposure; the others have room to spare, and raising exposure to suit
them would clip it.

| Reads | Means |
|---|---|
| `CLIPPING in R - 4.2% of pixels at 255` | Already lost. The percentage, not a stop count — a clipped channel has lost the information needed to say how far past 255 it went, so any stop figure would be invented |
| `at the limit - any more clips` | Under a quarter stop left |
| `good - 0.6 stops left in R` | Where you want to be |
| `under by 2.0 stops` | Room to raise exposure |
| `far under - the planet is near the noise floor` | Over three stops under |

Stops rather than DN because a stop is the unit the adjustment is made
in: "half a stop under" tells you to move the exposure by a factor of
1.4, where "highlight at 181 DN" does not. It is measured from the
99.9th-percentile highlight, not the mean, for the same reason
auto-exposure stopped using the mean — on a planetary frame the mean is
sky and barely moves however the planet is exposed.

**Gr/Gb split.** The difference between the two greens, as a percentage.

This is the one number here that **cannot be seen in the bars above at
all**, by construction. A Bayer quad has two greens and the G row is
their mean, so a frame with Gr=20 and Gb=180 draws a G histogram
occupying one bin at 100 — identical to a frame with Gr=Gb=100. A
160-count split, completely invisible.

| Split | Reads |
|---|---|
| under 3% | `normal` |
| 3–10% | `slight - usually just crosstalk` |
| 10–25% | `high - suspect the flat, or the Bayer phase` |
| over 25% | `very high - wrong Bayer phase, or a sensor fault` |

Every colour sensor has some, from crosstalk differing between the
green next to red and the green next to blue, which is why a few percent
says `normal` rather than warning. A large one is a real signal: it is
what a bad flat, or a debayer applied to the wrong Bayer phase, looks
like.

Worth knowing while reading the rows: because G is the mean of two
samples and R and B are single samples, **the G row is about √2 quieter
than the other two for a reason that is not the sensor's colour
response.** Comparing how far each row extends is the point of having
three rows, and one of them is being quietly denoised relative to the
others. The split figure is there so the averaging is at least visible
rather than silent.


### The sharpness window (`Shift+Q`, v1.2.4)

The rolling sharpness graph on a screen of its own, and **not** just a
larger copy of the corner overlay.

The overlay keeps 200 samples, about five seconds. That is the right
amount for *is it sharp right now* while your hand is on the focuser.
A screen of its own can answer a different question — has the seeing
settled, is focus drifting as the tube cools, was that a gust or the
mount — and that needs minutes. So this window keeps its own history of
4000 samples, fed from the same stream: roughly 90 seconds at 40fps,
longer at lower frame rates.

It also draws **gridlines at 30% and 60%**, the two thresholds the bars
are coloured by, so a value can be read off rather than guessed. Those
same thresholds are what autofocus and the capture quality gate act on.

`F11` fills the screen it is on, `Esc` closes it.

**Every window opens at startup**, on the screen its layout names —
sharpness, histogram, the sky map, GoTo, help and AI chat. A readout you
have to remember to open is one you will not be looking at when it
matters, and on a five-screen desk there is no reason to hide any of
them. The shortcuts close them when a screen is wanted for something
else.

They open *without taking focus*, so the main window keeps it and the
first keystroke of a session goes where you expect.

One change worth knowing: the GoTo panel used to start hidden
deliberately, to stop a mis-click starting a slew while imaging. That
was a fair worry when it opened over the imaging controls; it now sits
on a screen of its own, well away from anything you touch during a
capture. Press `G` if you would rather have it shut.

### The ADC alignment window (`Shift+A`, v1.2.6)

For dialling in an atmospheric dispersion corrector. **Hidden, and
inert, unless *ADC fitted to the imaging train* is ticked on the READY
screen** — see below.

The ADC *advisor* (the FOCUSER readout and the arrow on the preview)
predicts how much dispersion the atmosphere is producing at the current
altitude. It is a model and it never looks at the image, so it cannot
tell you whether the corrector you fitted is doing its job. This window
measures the dispersion **actually present in the live frame**, which is
what is left after the corrector has done whatever it is currently
doing. Turn the prisms, watch the residual fall, stop when it stops
falling.

| What it shows | Meaning |
|---|---|
| **Measured residual** | Arcsec and pixels of red-to-blue displacement still in the image. Green below 0.15″, amber to 0.40″, red beyond |
| **Predicted, uncorrected** | What the model says would be there with no corrector, and what percentage of it is left. A small residual at 70° altitude proves nothing; a percentage is comparable between nights |
| **Guidance** | What to do with your hands — spread the prisms, back them off, or rotate the ADC body |
| **Dial** | Which way the residual points, against the zenith axis. A correctly *rotated* corrector leaves a residual at 0° or 180°; off that axis means the ADC body itself is turned wrong, which no prism angle will fix |
| **Residual over time** | Whether the last nudge helped. Seeing moves the current number by more than a good nudge does, so the trend is the honest answer |

Three things about how it measures, because they decide whether to
believe it:

- **It reads the raw sensor mosaic, not the preview.** The preview's
  debayer takes red from a 2×2 block's top-left and blue from its
  bottom-right, which bakes a one-pixel diagonal offset into the two
  channels. Measured against the preview buffer on a target with no
  dispersion at all, that read **1.70 px of residual that no corrector
  could ever remove**. Reading the mosaic exposes the sampling grid, so
  the offset is a known constant to subtract.
- **It matches structure, not centroids.** Jupiter's and Saturn's belts
  are red-brown, so red's centre of brightness sits elsewhere on the
  disc than blue's for reasons that have nothing to do with the
  atmosphere. A centroid difference read **0.63″ on a banded planet with
  no dispersion**, drifting as the planet rotated — a number that would
  have you chasing cloud belts with a corrector.
- **It matches on edges.** Same reason an experienced imager watches the
  *limb* rather than the disc: the limb is in both channels at the same
  place with the same shape, while the belts are in both at wildly
  different contrast. On the same banded target, matching brightness
  invented 0.24″; matching edges reads 0.04″.

Together those take a dispersion-free target from 0.63″ of phantom
residual to **0.01″**. The floor is about 0.05″, set by red and blue
each being sampled on half the sensor grid.

It keeps a rolling window of 45 frames and shows the median, not the
latest frame. A single frame's reading at this plate scale is dominated
by seeing — consecutive frames disagree by more than the residual being
hunted, and dialling a corrector against a number that jumps is
guesswork.

**Use a bright target at the altitude you intend to image at.** There is
no dispersion worth correcting near the zenith, so there is nothing to
dial against up there.

`F11` fills the screen it is on, `Esc` closes it.

#### "ADC fitted to the imaging train" (READY screen)

A checkbox at the bottom of the READY list, **off by default**. It is
not a readiness condition — not owning a corrector never blocks
Continue — it is a statement about what is bolted to the train, and it
is remembered between sessions.

With it off, the ADC arrow on the preview, the FOCUSER readout (which
reads `ADC: not fitted`) and `Shift+A` all stay dark. The figures would
still be *true* with no corrector fitted, because the atmosphere
disperses either way — but a readout about a part you do not own is
noise, and an alignment window showing a residual is an invitation to go
and adjust something that is not there.

It saves the moment you tick it rather than when you press Continue,
because `Esc` and the close box also end that dialog, and a setting that
only persists down one of three exits is one that sometimes silently
does not.

### The status window (`Shift+D`, v1.2.6)

Capture health and disk space, one bar per row.

| Row | Bar | Right |
|---|---|---|
| `REC` | How much of the capture drive's free space this recording has used | Live FPS |
| `BUF` | Ring-buffer slots in use — amber past 50%, red past 80% | Frames lost |
| `WX` | The weather (see **Weather**): cloud, wind, dew risk, the reading's age and its source. With Leoben's reading, its imaging score as the bar, green GO / amber CAUTION / red NO-GO; with RadukCapture's own, the bar shows how fresh the reading is. Hidden with `WeatherSource=off` | Air temperature, coloured by dew risk |
| one per drive | Space **used** | Percent **free** |

**`REC` is bytes written by the current recording**, not disk usage. It
was labelled `HDD` until v1.2.6 and scaled against a flat 500 GB, which
on a 2 TB drive meant nothing and, sitting directly above a list of real
drive capacities, read as a disk with 0.188 GB on it. It is now scaled
against the free space on the drive being written to, so the bar answers
the question actually being asked mid-run: how much of tonight's
headroom is gone.

**One row per fixed drive**, by letter, with the capture drive marked
`*`. The bar grows as a drive fills — the same direction as every other
bar in the app — but the number spelled out is **free** space, because
that is what decides whether tonight's capture completes. Free turns
amber under 100 GB and red under 20 GB. Those thresholds are in
gigabytes rather than percent on purpose: 2% is ample on a 4 TB drive
and nothing on a 256 GB one, and a night of SER files is tens of GB
whatever the drive's size.

Fixed drives only. A network share can hang far longer than a sleeping
disk, a CD-ROM's free space means nothing, and a RAM disk is not where
anyone keeps a night's data.

The free-space figure is the space available **to you**, which under a
disk quota is smaller than the raw free total — and the smaller number
is the one that decides whether the capture fits.

Space is re-read every five seconds **on a background thread**. A
sleeping external USB drive can take seconds to answer while it spins
up, and doing that on the UI thread would stall the preview and the
capture loop with it. A slow drive shows numbers a few seconds stale
instead.

`F11` fills the screen it is on, `Esc` closes it.

### Reticle (`R` key, or the `[+]` toolbar button)

One control, four shapes. Each press steps to the next and the fifth
turns it off, so a single press still just switches the reticle on:

**off → crosshair → circle → rings → box → off**

| Shape | Use |
|-------|-----|
| **Crosshair** | Centring a target before tracking. Spans the frame with a gap at the middle, so the target itself is never covered. |
| **Circle** | A single ring, for judging whether a disc sits centred. |
| **Rings** | Three concentric rings for **collimation** — defocus a star until it shows as a donut and check the donut is centred in these. |
| **Box** | Corner brackets framing a region, for deciding on a crop before committing to one. Brackets rather than a full rectangle so the edge does not hide the detail just inside it. |

All four keep a centre dot — the only part that marks an exact pixel, and
what you nudge the planet onto. The toolbar button names the live shape.

### Moving, sizing and rotating it

| Gesture | Effect |
|---------|--------|
| `Ctrl` + drag | Move it anywhere in the frame |
| `Ctrl` + wheel | Make it bigger or smaller |
| `Shift` + wheel | Rotate it, 5° a notch |

Modifiers rather than plain gestures on purpose: a plain drag is the ROI
rubber band and a plain wheel is the preview zoom, both used constantly. The
reticle takes the modifiers so neither is disturbed.

**Rotation only applies to crosshair and box.** A circle and three
concentric rings look identical rotated, so the app says so rather than
accepting the input and appearing to do nothing.

The position is stored as a fraction of the frame, so it stays where you put
it when the window is resized or you change zoom. **Cycling the reticle off
resets position, size and rotation**, which is how you recover from a stray
`Ctrl`+drag without hunting for a reset button.

### Clipping Warning (`C` key)
Any pixel where R, G, or B = 255 flashes red at ~2.5Hz. The `CLIP X.X%` label appears top-right in the HUD. Useful for dialing in exposure without clipping planetary detail.

### ADC Overlay
Shown automatically when mount altitude > 0°. Bottom-right corner box showing:
- Dispersion in arcseconds
- Severity level (color-coded)
- Blue upward arrow indicating the direction of dispersion

The number depends on which filter is fitted — see [Filters](#filters). The
FOCUSER section states the band in use directly beneath the ADC readout.

### Quality Bar Graph (`Q` key)
Rolling 200-frame sharpness history shown bottom-left, above the status bar.
- **Green bars** — Sharpness ≥ 60% (excellent seeing)
- **Amber bars** — Sharpness ≥ 30% (acceptable)
- **Red bars** — Sharpness < 30% (poor seeing / turbulence)
- **PK** — Peak sharpness in current window
- **AV** — Mean sharpness in current window

Watch for a burst of green bars during a good seeing cell — that's when to capture.

### RGB Align (`V` key)
Shifts blue channel UP and red channel DOWN during the display debayer pass to correct atmospheric dispersion. Auto value computed from mount altitude + Edlen model. Manual slider adds ±20px fine-tune.
- HUD shows `RGB ALIGN +N px` in amber when active
- Plate scale: 0.203 arcsec/px (8SE + ASI678MC at f/10)


### Pixel Value Readout (`X` key)
Toggle with `X`. When active, hover the cursor over any pixel in the preview to see a tooltip:
- **`R:NNN  G:NNN  B:NNN`** — raw 8-bit channel values from the debayered RGB buffer
- **`[fx, fy]`** — sensor pixel coordinates (accounts for zoom and pan)

**Click** to lock the readout to that pixel — tooltip turns green and `PIXEL LOCK` appears in the HUD top-right. Click again to unlock. Useful for:
- Checking exact exposure level on a planet's limb vs. disk center
- Confirming clipping (R/G/B = 255) on specific detail
- Verifying white balance by checking neutral gray stars

### Display FX (`B` key)
Cycles through six display modes — all display-only, SER data is always raw:

| Mode | Effect |
|------|--------|
| `NORMAL` | Unmodified debayered output |
| `BOOST ×2` | Each channel multiplied ×2, clamped at 255 |
| `BOOST ×4` | Each channel multiplied ×4, clamped at 255 |
| `STACK 3 BEST` | Aligned stack of the best 3 recent frames |
| `STACK 5 BEST` | Aligned stack of the best 5 recent frames |
| `STACK 10 BEST` | Aligned stack of the best 10 recent frames |

Active mode shown in amber HUD top-right (e.g. `BOOST ×2` or `STACK 5 BEST`). **FX automatically resets to NORMAL when recording starts** — you always capture raw frames. Also accessible via `[FX]` button in the IMAGE panel section.

Use Boost to find a dim target or check focus in poor seeing.

### The quick-look stacker (v1.2.0)

The three STACK modes are a **quick-look stacker**: they keep only the
sharpest recent frames, line them up on the planet, and average those.
Two numbers appear in the HUD:

```
LIVE 0.062  STACK 0.071  (10)
```

`LIVE` scores the current single frame, `STACK` the combined image, and
`(10)` is how many frames are in it. Both use the same measure over the
same area, so they are directly comparable. **Higher is sharper.**
While the first 32 frames are still being measured it shows
`learning...` instead of a threshold.

What this is for: deciding **at the telescope** whether what you are
capturing is sharp, instead of finding out the next morning. Watch
`STACK` while you nudge focus, and watch it over a few minutes to see
whether the air is improving or deteriorating.

Only about the top 15% of frames are admitted. Skipped frames are
normal and are the point — the whole value of lucky imaging is throwing
most frames away.

This is a **preview**, not a substitute for AutoStakkert. It aligns the
whole frame as one piece, where AutoStakkert corrects distortion
separately across dozens of small patches and will always produce a
better final image from the same video. Record the video regardless.

**Before v1.2.0 these modes averaged every recent frame at fixed pixel
positions.** Because the planet wanders 10-25 px with the seeing, that
smeared it — `STACK 10f` produced an image *softer* than a single frame
while the label implied otherwise. If you learned to avoid the stack
modes because they looked worse, that was why, and it is fixed.

### HUD Status Bar (bottom)
`LIVE 3840×2160 RAW8 38.3 FPS FRAMES: 2151 DROPS: 0`
- Click on the resolution readout to get a reset popup (reset to full sensor resolution).

---

## Autofocus Routine

1. Press `A` or click `★ AUTO FOCUS`.
2. The focuser sweeps 11 positions across ±3000 steps, averaging **8
   separate measurements** at each one.
3. A parabola is fitted to the peak sample and its two neighbours.
4. The focuser drives to the fitted position, approaching from one side
   to take up backlash.
5. Sharpness is measured again there and compared with where it started.
6. Press `A` again to abort mid-run.

**It takes about a minute.** That is expected. Each position waits for 8
genuinely new measurements at the analyser's own rate, which is what
makes the curve fittable rather than seeing noise.

### The fine pass, and which way moves end (phase 145)

After that coarse sweep, a **fine pass** narrows it. The coarse sweep measures
the planet's half-flux diameter, which finds focus from far out but barely
changes near focus on a big disc such as Jupiter. The fine pass sweeps 7 points
across the coarse curve's width and reads **edge contrast** instead
(Tenengrad: smoothed, with sensor noise gated out, and normalised so exposure
cancels). It scores about 20 frames at each point and keeps their 75th
percentile, which is the focus as it looks in the steadier moments, and each
point is approached the final way. If the best point lands at an end, it
sweeps once more centred there. If anything goes wrong, the coarse result
stands. **On the Moon**, or anything else that fills the frame, there is no
disc, so the fine pass runs on its own round where the focuser is. The fine pass reads its centre point again at the
end (phase 145.5): if the light changed by 10% to 40% while it swept (dew,
haze), the scores are corrected for it, and beyond 40% the coarse result
stands. On the Moon it scores the start first and returns there if the
result is really worse.
`AutofocusFineStage=0` in rig.ini turns the fine pass off.

**Every focuser move ends travelling one way**, by at least the backlash
(phase 139), so a step number means one focus position. On an SCT that last
travel should push the primary mirror **toward the corrector**, against
gravity, so it does not sag back as the scope slews. Which step direction
that is depends on how the EAF is mounted and on its reverse setting, so
check it once. Watch the focus knob while stepping to higher numbers, and
compare it with the knob direction your scope's manual gives for moving the
mirror forward. Then set `FocuserFinalDirection=out` (rising numbers, the
default) or `in` in rig.ini.

**Checking the backlash by hand.** Autofocus estimates the backlash from
every sweep (`Backlash measured on this sweep` in the log), but one sweep's
figure scatters widely. To check it:
1. Focus on a bright star, finishing the final way.
2. Step back the other way 10 steps at a time, watching the preview at 200%.
3. Count the steps until the image first changes: that is the slack.
4. Set `FocuserBacklashSteps` in rig.ini a little above it, and leave the
   EAF's own backlash at 0, which the app already does while connected.

**CALIBRATE FOCUS: measuring what a step does.** Hold `Ctrl` and click
**AUTO FOCUS** (or send the remote command `calibrate_focus`). Run autofocus
first: the calibration starts from where the focuser is and takes that as
focus. On a planet or a bright star, well up in the sky, not while
recording. It takes a few minutes and ends where it began. Auto-exposure
settles first, as for autofocus, and gets a turn before each wing. It
measures:
- **How well a position repeats:** the same step approached five times.
- **The knob's slack:** it moves out to where the image is clearly softer,
  then reverses in small moves until the image answers. That count is what
  belongs in `FocuserBacklashSteps`, and `FocuserOvershootSteps` must be
  larger than it.
- **Steps to defocus:** how fast the target's half-flux diameter grows on
  both sides of focus. With the pixel size, focal ratio and
  `CentralObstruction` from rig.ini, that is the focal-plane travel of one
  step, and so the width of the focus zone in steps.

The status line gives the result (for example "3.4 um a step, the focus zone
is 79 steps; slack 590 steps"), and a block is appended to
`focus_calibration.ini` in the data folder with the time, the EAF's
temperature, the altitude and every figure. Blocks are never overwritten:
run it again at another altitude or temperature and compare. A figure it
could not measure is left out and the status line says why; a target that
fills the frame (the Moon) gives the slack but not the steps to defocus,
which need a disc with sky round it. On a disc the steps-to-defocus figure
reads about 3% low, from the arithmetic of a blurred disc; a star reads
true.

**Tips:**
- Start near focus. The sweep covers ±3000 steps; if true focus is
  outside that it will say so rather than guess.
- Use a bright star or a planetary disk, never a faint fuzzy object.
- Check the EAF is connected (FOCUSER shows a position, not dashes).

### What it reports, and why you can trust it (v1.2.0)

Autofocus now checks its own work and says plainly when it did not help:

| Message | Meaning |
|---------|---------|
| `AF done: pos=… +24.0%` | Worked. Sharpness improved by that much. |
| `focus not bracketed — scan wider or recentre` | The sharpest point was at the edge of the sweep, so true focus is probably outside it. Focus roughly by hand, then re-run. |
| `Focus got worse (… → …) — reverted to start` | The fit landed somewhere worse, so it put the focuser back. Usually too little contrast to work with: centre the target, or wait for steadier air. |

Three faults were fixed in v1.2.0, and together they mean **results from
before that version were not meaningful**:

- Each of the 8 measurements per position was the *same* reading copied 8
  times, so every point on the curve carried full single-frame seeing
  noise instead of an average.
- The confirming measurement after the move collected nothing at all, so
  the reported sharpness was always `0.0%` and the improvement figure was
  meaningless.
- Success was reported unconditionally. A curve fitted to noise could
  drive the focuser somewhere clearly worse and still say `AF done`.

Separately, the sharpness measure itself was computing a red-minus-green
colour difference rather than detail, because it compared neighbouring
pixels that belong to different colours on the sensor. It barely
responded to focus at all. **Sharpness readings changed scale when this
was fixed, so numbers you remember from earlier sessions no longer
compare.**

---

## Recording Workflow

1. **Set target name** in the CAPTURE section file prefix box.
2. **Select frame type** (Light for normal captures, Dark/Flat/Bias for calibration frames).
3. **Set limit** — frames (e.g. 5000) or time (e.g. 120 sec).
4. **Optionally enable GATE** — set MIN SEEING threshold so only sharp frames are recorded.
5. **Click ▶ RECORD** — SER file opens immediately. Button turns red.
6. **Press `P`** to pause if seeing deteriorates. Press again to resume.
7. **Click ■ STOP RECORDING** or wait for the limit to be reached.

**Output filename format:**
`Jupiter_20260322_213045_Light_3840x2160_8bit_0001.ser`

`prefix _ timestamp _ frametype _ WxH _ bitdepth _ runnumber .ser`

When a Light finishes, the middle of the run in UT goes on the front so WinJUPOS can read it: `2026-03-22-2130_4-Jupiter_20260322_213045_Light_3840x2160_8bit_0001.ser`. The last digit is tenths of a minute. `WinJuposNames=0` in `rig.ini` turns this off.

A `_quality.txt` file is written alongside each SER with per-frame sharpness scores, mean, and peak.

### What protects a recording (phase 135)

- **A disk that fails or fills stops the run, and says so.**
  - A failed write ends the run with an error on screen: the frames still in memory are lost, so check the drive.
  - The free space is checked once a second. Below the floor (the larger of 256 MB and what is still waiting in memory), nothing more is accepted, and every frame already captured is still written.
- **A crash does not leave a file that says it is empty.** The frame count in the SER or FITS header is updated on disk once a second during the run. A crash loses the last second and the timestamps, not the frames.
- **A stalled camera recovers in about 1.5 s.** If no frame arrives for 1.5 s, or for 10 frame intervals on a long exposure, the camera is restarted. It used to take at least 10 s. Each restart writes a `Pipeline wedge` line in the log.
- **A camera that never comes back ends the run (phase 157.0).** If the camera sends nothing for 15 s, or for five frame intervals on a long exposure, the recording is stopped and the bar says so. Every frame it sent before is on disk with its record. A short USB drop that recovers inside that time does not stop the run. Before this, a run limited by time stayed open for ever, because the limit is only checked when a frame arrives.
- **A GoTo that never arrives is stopped (phase 157.0b).** If the mount still says it is slewing after 3 minutes, the GoTo panel stops it, says "GOTO DID NOT FINISH", and does not start acquiring. A survey whose slew does not finish in 90 s stops, instead of solving wherever the mount happened to be.
- **A run that records nothing leaves nothing behind.** No empty file, sidecar or record, and no line in the night's DeTeCt or submission lists.

### Where every frame went (phase 146)

At the end of each run, one line in the log, `[CAPTURE] Frames: ...`, says what happened to every frame the run did not keep. The same figures go into the observation record (`dropped_sdk`, `frames_dropped`, `skipped_by_choice`, `timestamp_gaps`, `frames_unexplained`, `median_interval_ms`, `effective_fps`, `rate_limit`, and `camera`, the camera's own name).

- **The camera or USB lost it** (`dropped_sdk`): the camera driver's own count, only this run's share.
- **Skipped on purpose** (`skipped_by_choice`): the quality gate was shut or the run was paused. Never a loss.
- **Our buffer was full** (`frames_dropped`).
- **Nobody reported it** (`frames_unexplained`): a gap in the frame times, more than 1.5 times the usual spacing, that none of the above accounts for. Anything above 0 here is worth knowing about.

The line also says whether the run was **exposure-limited** (each frame waited on its exposure) or **transfer-limited** (frames came as fast as USB, the camera or the disk allowed), and the effective frame rate.

**Any SER, after the fact:** `ser_drop_audit.exe <file.ser>` (built beside the app) reads the file's frame times and gives the same breakdown. With the run's `_observation.txt` beside it, it separates gate skips and camera drops from real losses; without one, every gap counts as unexplained, and it says so. It works on older recordings too.

**Measuring the camera itself (146.2).** About 30 minutes indoors; no sky needed, because frame rate over USB does not depend on what the sensor sees.
1. Cap the telescope, or take the camera off it with its cap on. Plug it into a USB 3 port on its own controller (this PC has two, Intel and ASMedia) with the cable you use at night.
2. FULL resolution, RAW8, high-speed mode on, the USB bandwidth control at its highest, exposure 0.1 ms, gain anywhere, the gate off.
3. Record three runs of 60 seconds. Note the `[CAPTURE] Frames:` line after each.
4. Then one 60-second run at 1280×960 RAW8, and one at FULL RAW16.
5. Send the five lines. If the first runs show camera or USB drops, try the other USB controller and a shorter cable before anything else, and say which you used.

If the full-sensor RAW8 rate is above what the disk can write (this PC's disk measured about 292 MB/s), the run will show our buffer dropping frames once it fills. That is the right answer, not a fault in the measurement.

### What goes beside each Jupiter run (phases 142 to 144)

- **The observation record** (`_observation.txt`) and **the FITS header** also carry:
  - the filter (`filter`, `FILTER`, from `FilterName`);
  - Jupiter's central meridians at mid-run (`cm_sys1_deg` to `cm_sys3_deg`, `CMSYS1` to `CMSYS3`);
  - whether the PC clock was checked (`clock_confirmed`, `clock_offset_ms`, `CLOCKOK`, `CLOCKOFF`), which only a mutual-event recording does;
  - what was on the disc as the run started (`jupiter_disc`, `JUPDISC`).
- **`detect_jupiter_<date>.txt`** in the capture folder lists each Jupiter Light run, with its start and length, for DeTeCt's impact-flash search. Run DeTeCt over the folder yourself. The log keeps the night's Jupiter minutes: empty hours count toward the impact statistics too.
- **`submission_<date>.txt`** gets one line per Light run with what a measurer or an archive asks for: the file, target, mid-run UTC, length, frames, meridians, telescope, camera, filter, site and clock.

---

## Night vision (`N`)

Turns the **preview and HUD** deep red: the image is reduced to luminance and
pushed into the red channel, which keeps detail visible while giving your eyes
far less to re-adapt from. **Remembered between sessions**, so if you finish a
night with it on, the next launch comes up red rather than blinding you.

**The whole interface goes red, not just the image.** The control panel, the
toolbar, the status bar, the floating windows and the dialogs all follow.
Measured by capturing the rendered windows: of the pixels sampled in the
control panel, 21% were blue- or green-dominant in day mode and **0%** with
night vision on; the status bar went from 77% to 0%.

The toolbar retains about 3% blue, and that is the anti-aliasing of the
button labels rather than any colour the app chooses — removing it would mean
changing how every font in the application is rendered.

**Accent colours converge.** Cyan, amber and green are distinguishable by
hue in day mode and all become the same red at night, because they have
almost the same brightness. That is unavoidable in a red-only palette: any
transform that kept them apart would have to put back the blue and green
that night vision exists to remove. Section headers still carry their names
in text.

One known gap: a few overlays drawn on top of the preview keep their day
colours if you switch night vision on **during** a session. Start with it on,
or press `N` twice, and they follow. The main surfaces are unaffected either
way.

Pair it with the `[FX]` brightness modes rather than raising gain if the
screen is still too bright.

---

## Framing shot (`Shift+S`, or the `[FRAME]` button)

A faint target is invisible at planetary settings, so checking whether it is
even in the frame used to mean winding gain and exposure up by hand and
remembering what they were.

One press saves your gain and exposure, goes to **gain 570 and a 2-second
exposure** for **five seconds**, then puts both back exactly as they were. The
preview is live throughout — nothing is captured to disk and the recording
settings are untouched.

**It refuses while recording**, and says so. Boosting gain mid-file would
bury unusable frames in the middle of a capture, and you would not find out
until you came to stack it.

The restore is automatic, so there is nothing to undo if you walk away. It is
`Shift+S` rather than `F` — a framing shot is a boosted snapshot, and `F` has
stepped the focuser since long before this existed.

---

## Snapshot (`S` key)

Saves the current debayered frame as a PNG to the captures directory. Uses the same prefix and output directory as the SER recorder.

**Output filename format:**
`Jupiter_20260322_213045_snap_0001.png`

---

## File Organization

All files go to the configured output directory (default: `D:\RadukCapture\captures`):

| File | Contents |
|------|----------|
| `*.ser` | Raw Bayer video frames, SER format |
| `*_quality.txt` | Per-frame sharpness scores + summary |
| `*_snap_*.png` | Single-frame PNG snapshots |
| `session_log.txt` | Session events, temperatures, status |

**The validation log** (from 3.3.37). Beside the session logs, in the logs
folder, each run writes `validation_<night>_<time>.json`. It holds the
night's evidence for the clear-night checklist: each plate solve's measured
scale, the real frames' sharpness by target, exposure and gain, the seeing
figure through the night, every autofocus result with the backlash it
measured and the fine pass's figures, and the target's brightness and clipping against the profile's
exposure band. It is written every 5 minutes and at exit, and a run that saw
nothing writes nothing.

To see what the nights add up to, run this from the repository:

```
python tools\validation_report.py <the logs folder>
```

The report gives each figure with a 95% confidence interval. It recommends a
new value (the measured plate scale, the backlash, whether `SHARP_CEIL` is too
low or too high) only when enough nights agree, and otherwise says what is
missing. It changes no setting.

---

## The browser dashboard (remote monitoring)

`RadukCaptureDashboard.html` connects to RadukCapture over a WebSocket on
port 8765 to show telemetry and send commands, including commands that
move the mount and focuser.

**It needs an access token.** RadukCapture refuses any connection that does
not present one. On first launch it generates a random token and stores it in
`rig.ini` next to the executable, as `WsToken` under `[Rig]`. The session log
says that a token was generated and where it is, and never shows the token
itself.

**The quick way, on the capture PC: press `[DASH]`** on the toolbar. It
opens the dashboard in your default browser with the token already in
it, switches to the RadukCapture tab and connects. Nothing to copy. The
token travels in the part of the link after `#`, which a browser never
sends anywhere, and the dashboard clears it from the address bar as
soon as it has read it.

From another machine, or by hand:

1. Open `rig.ini` on the capture PC and copy the `WsToken` value.
2. In the dashboard, paste it into the **access token** box under the URL.
3. Press **CONNECT**.

The dashboard keeps the token in that browser only. It is deliberately not
saved with the URL, because the URL is also synced to the dashboard's helper
server, where anything on the same WiFi could read it. Pasting a URL that ends
in `?token=…` works too: the token is moved into its own box.

**The preview picture.** Once connected, the dashboard's centre panel
shows the telescope's view, updated about twice a second. It is a small,
brightened copy for framing at a glance, not the recorded image: what
goes into the SER file is untouched. It costs the capture nothing
measurable, and it pauses when no dashboard is connected.

**Why a token, when the server only listens on this PC?** Browsers let any
web page open a WebSocket to `localhost`. Without a token, a page open in
another tab could connect and slew the telescope. The token is the one thing
such a page cannot know.

- **Connection refused, every time:** the token is missing or out of date.
  Copy it from `rig.ini` again.
- **The log says WsToken is not at least 32 hex characters:** the value in
  `rig.ini` was edited by hand into something too weak to trust. RadukCapture
  then refuses every connection rather than accept it. Delete the line and
  restart to have a new one generated.
- **Connecting from another machine:** see the next section.

### Reporting a problem to Claude (REPORT, `Ctrl+Shift+R`, 3.3.48)

When something goes wrong at the scope, press **REPORT** on the left rail (or `Ctrl+Shift+R`). Say what you did, what you saw and what you expected, and press **SEND**. The window closes straight away, so you can get back to the telescope.

Behind the scenes, the app:
1. **Saves a report** in `<data root>\reports\<date_time>\`:
   - your words;
   - what the app was doing: target, profile, exposure, focuser, mount, lock, recording;
   - the log's last 4000 lines;
   - your `rig.ini`;
   - screenshots of its windows, unless you untick that.
2. **Removes your site and anything secret before anything leaves the PC:** the site lines in the log and `rig.ini`, the Dash token, keys, passwords and Leoben's address.
3. **Sends it to Claude Code on this PC** (`tools\sky_handoff.py`). The report goes on its own branch, `report/<date_time>`, without the screenshots. Claude Code then looks into it, fixes the cause if the evidence is clear, and opens a draft pull request for you to review. It never merges.
4. **A notice says how it went:** sent; saved but not sent, and why; or kept on the PC if you're running with `Offline=1`.

Sending needs Python, git and Claude Code on this PC, and the app built from a clone of the repository (it finds the tools there). Without them, the report is still saved, and the notice says where.

### The weather window (`Shift+W`, 3.3.45)

The whole weather reading the app holds: cloud, temperature, humidity, wind and gusts, dew point and dew risk, sea-level pressure, 7Timer's seeing (1 is best of 8), and, from Leoben, the verdict and imaging score.
- **Units:** temperatures in °F with °C beside them, wind in mph with m/s, the same as Dash.
- **A missing value** shows as `--`, never a guess.
- **The last line** says where the reading came from (this PC, or Leoben) and how old it is.
- **With no reading,** the window says why: offline, not configured, or the last reading is stale.
- **↻ REFRESH** asks the app's own sources for a new reading. It's off when `WeatherSource` is `leoben` or `off`, or with `Offline=1`, because then the app fetches nothing.

The Status window keeps its one-line WX row, and Dash's weather card mirrors this window.

### The SKY RUN window (`Shift+R`, 3.3.45)

The unattended test night runs from the app's own window, not from Dash. Dash's card below mirrors it, for your phone or tablet.

- **The headline** says where the run is: not started, refused, waiting for the dark window, running, holding, parking, or done (and why it ended).
- **Now:** the step running, for example "autofocus from +700 (items 47, 58)".
- **Refused:** every reason it will not start, each one something to set or fix. Nothing moves.
- **PLAN:** each test group with its status and why, and under it each checklist item's own status, for example "items: 13 passed, 47 collected, 55 collected".
- **START** runs every group that applies. **STOP (parks)** ends the run and parks. Esc or Space, the emergency stop, ends it without a park.

The command palette lists it as "Sky run window".

**Sending a night to Claude Code: the bundle and the hand-off (3.3.45).** Any night can go to Claude Code: an unattended sky run, a normal night of imaging like a Saturn session, or a night with Dash's test suite run against the app. Run the command from the RadukCapture folder.

- **After a sky run,** the log's line starting `[SKYRUN] to hand off:` gives the exact command.
- **For a normal night,** the app writes a line starting `[NIGHT] to hand tonight to Claude:` when it closes. If you've started the app again since, use `raduk_log.prev.txt` as `--log`.

```bat
python tools\sky_handoff.py "<data root>\field-results\<night>" --night <night> --log "<raduk_log.txt>" --logs "<logs folder>" --captures "<capture folder>" --rig "<rig.ini>"
```

**After a Dash test night,** run Dash's tests against the running app first (`cd tests\web` then `npm test`). Then add `--dash tests\web\results\dash-results.json` to the command above.

What it does:
1. **Builds the bundle** in `field-results\<night>`:
   - `bundle.json`, saying which kind of night it was;
   - `summary.md`: a sky run's tests with each item's status; a normal night's recordings, Autorun rankings and validation log; a Dash night's test counts and each failure;
   - the night's log lines;
   - `recordings.txt` (paths only; no recordings are copied);
   - the night's `autorun_*.txt` rankings, including any written after midnight, and the validation log;
   - Playwright's report, for a Dash night.
2. **Commits it on its own branch** from `master`, in a separate worktree, so your checkout and its edits aren't touched. The branch is `field/<night>` for a sky run, `field/<night>-imaging` or `field/<night>-dash` for the others. Only a sky run marks the clear-night checklist. Running it again for the same night adds to the branch it pushed before.
3. **Pushes, and starts Claude Code headless.** Claude reads the evidence, writes `findings.md`, may propose UNVERIFIED changes and small fixes, and opens a **draft** pull request. It can't merge or push to `master`.

When it stops early:
- **An emulator sky run** is bundled, but not committed.
- **With `Offline=1`** the commit stays on this PC.
- **If Claude Code is missing or fails,** the bundle is still pushed, and it says the analysis is owed.

`--dry-run` builds the bundle and says what it would do; `--no-claude` pushes without the analysis.

### PARK and SKY RUN (3.3.39)

On the RADUKCAPTURE tab, under MOUNT JOG:

- **PARK** takes two taps (the first arms it for 4 seconds), so a phone in a pocket cannot move the scope. It shows parking, PARKED with tracking off, or why a park failed, and is greyed out until `SkyParkAltAz` is set.
- **SKY RUN** starts the unattended run. The app checks the rig first and lists every reason it will not go. Then it waits for the dark window (`SkyDarkSunAltDeg`), runs each test group that applies to what is in view, and parks at the end. The card shows the plan with each group's status, the step now running, and why the run ended. **STOP** ends the run and parks; ESC or the mount window's STOP ends it without a park. Results go to `field-results\<night>\results.json`, for `tools\sky_checklist_update.py`. Two groups are carried out so far: **tracking** (checklist 48 to 50) and, from 3.3.40, **any planet** (1, 13, 47, 55, 58). The others are listed as not built yet, never as failed.

**The planet group, step by step** (about 20 minutes): GoTo and PLANET LOCK; then two autofocus sweeps, one started 700 steps out and one 700 steps in, inside the focuser's travel guard (their difference is item 47's repeatability); then 60 seconds of recording at the profile's frame, which passes with no unexplained drops and the camera's own rate held (item 13); then the exposure ladder, 90 seconds at each of 2, 5, 10 and 15 ms with the gain found so the planet's highlight sits near 70% (item 55). Each item gets its own status: throughput passes or fails, while autofocus repeatability, the fine pass, the ladder and sharpness are COLLECTED, because what counts as good there is your eye's call. Your exposure, gain and AE setting are put back afterwards however the group ends, and a failed sweep sends the focuser back to where it started. Its recordings are never auto-stacked during the run.

### From another device on your home network

Leoben on Elminster, a tablet, a laptop -- anything on the same network
can use the dashboard, with nothing to type but the capture PC's name.

1. In `rig.ini` on the capture PC, under `[Rig]`, set `WsLan=1` (listen on
   the home network, not only this PC) and `WsLanOpen=1` (a device on the
   home network needs no token). Restart RadukCapture. The log confirms it:
   `WebSocket server on 0.0.0.0:8765 (home network ...) - private-network
   clients need no token`.
2. Let it through Windows Firewall, once, from an administrator PowerShell
   on the capture PC. The rule covers home networks (the **Private**
   profile) only:

   ```
   New-NetFirewallRule -DisplayName "RadukCapture dashboard (home network)" -Direction Inbound -Protocol TCP -LocalPort 8765 -Profile Private -Action Allow
   ```

   If Windows still blocks it, check the network itself is set to Private:
   `Get-NetConnectionProfile`.
3. Open the dashboard on the other device -- through Leoben (**Astro ->
   RadukCapture**), or the file itself -- and in the RadukCapture tab enter
   `ws://<capture PC's name or IP>:8765` once and press **CONNECT**.

From then on it **stays connected**: if RadukCapture is closed, or not
started yet, the dashboard waits and retries -- after 3 s, then 5, 10, 20
and every 30 -- and connects as soon as RadukCapture is up. Only
**STOP RETRYING** / **DISCONNECT** stops that. When Leoben serves the page,
the address is saved on Leoben, so every device that opens the dashboard
through it finds the rig without being told.

**What `WsLanOpen` costs, said plainly:** any web page open in any browser
on your network can then send the rig commands, including mount moves. That
was judged acceptable for a trusted home network. Two things stay closed
regardless: a browser on the capture PC itself still needs the token
(`[DASH]` supplies it), and an address from outside your network -- the
port forwarded by mistake -- still needs it too. Never forward port 8765.

It does not trouble a game on the same network: telemetry is a few hundred
bytes twice a second, and the preview picture about 11 KB a second, only
while a dashboard is open.

---

## Jupiter's Red Spot, moons and shadows (`J`, phases 137 and 144)

Which hour of a Jupiter night is worth the derotation window depends on what
faces you. `J` lists, for your site (Jupiter at least 15 degrees up, the Sun
below -6), the next two days of:
- the central meridians now (Systems I, II and III);
- the Red Spot's transits;
- each moon's transits, its shadow's transits, and its occultations and eclipses.

**The Red Spot** transits when System II reaches its longitude, and that
longitude drifts. Published figures disagree, and each degree moves the
transit by about 1.65 minutes. So it comes from rig.ini:
- `GrsLonSys2`: the longitude, degrees, System II;
- `GrsEpoch`: the date it was measured, `YYYY-MM-DD`;
- `GrsDriftDegPerMonth`: the drift.

The defaults are 91, 2026-06-01 and 1.75. Over 60 days old, the log asks for
a recent ALPO, BAA or JUPOS figure.

**Moon events** are fetched from JPL Horizons, three days ahead, and cached.
Shadow transits are worked out from the moons', the Sun's and the Earth's
positions. Without a connection, the central meridians and the Red Spot still
work; the moon events come from the cache. See [Without an internet
connection](#without-an-internet-connection).

**Using it.**
- **`[RUNS]` on Jupiter** puts a due focus sweep off while the Red Spot is within 50 minutes of the meridian or a moon or shadow is on the disc (up to `AutorunPrimeDeferMin`, default 30). The run list notes what was on the disc for each run.
- **The assistant** knows the next 8 hours.
- **The records** say what was on the disc (see [What goes beside each Jupiter run](#what-goes-beside-each-jupiter-run-phases-142-to-144)).

---

## Mutual events of Jupiter's moons (`J`, phase 141.0)

Around each Jupiter equinox, about every six years, the four large moons
pass in front of one another and through one another's shadows. Light
curves of these events, timed to better than a tenth of a second, refine
the moons' orbits, and IMCCE (Paris Observatory) collects them from
observers worldwide in its PHEMU campaign. This season runs from May 2026
to August 2027, with Jupiter's equinox on 16 December 2026 and most
eclipses between October and January.

**`J`** lists the events in the next seven days that can be seen from your
site: Jupiter at least 15 degrees up and the Sun below civil twilight.
Each line gives the middle of the event in UTC, which moon dims which,
how long it lasts, how deep the drop is predicted to be, and the window to
record in. The full list goes to the log; the notification shows the
next one. Half an hour before an event's recording window opens, a
reminder goes up.

**The recording window** starts at least ten minutes before the predicted
start and ends ten minutes after the predicted end, and for an event
longer than ten minutes by its own length. IMCCE warns that long events
can come five to ten minutes off the prediction, and the reduction needs
the flat light before and after the dip.

**What the cautions mean.** They are IMCCE's own advice on which events
to prefer: a drop under 10% is hard to measure; an event within three
Jupiter radii of the planet's limb sits in its glare; a grazing or
penumbral-only event may not happen as predicted. Occultations, and
events involving Callisto, are preferred.

**Where the predictions come from.** IMCCE's own service, fetched when
the app is online and kept in `phemu_jupiter_2026_27.txt` in the data
folder, so the list works offline afterwards. They are refreshed when a
month old. Nothing about your site is sent; visibility is worked out on
this machine. The file is IMCCE's material, kept for your use: do not
publish or share it.

**Recording one: `Shift+J`** (phase 141.1). It asks twice, like anything
that starts a sequence. It arms the next event visible in the coming 18
hours and then:

1. checks the PC clock against a time server (`TimeServer` in rig.ini,
   `pool.ntp.org` by default), because IMCCE needs every frame dated to
   UTC within 0.1 s and the frames are stamped from the PC clock;
2. switches off auto-exposure, autofocus and ROI-following, so nothing
   changes the brightness or the framing during the event. **Set gain and
   exposure before you arm it** so no moon clips: they will not change;
3. starts recording when the window opens and stops when it closes;
4. checks the clock again, writes IMCCE's observation sheet beside the SER
   as `<event ID>.txt`, and puts your settings back.

The sheet is IMCCE's own form, filled in: the event, your site
(`PhemuSite` in rig.ini names it), the telescope, the filter, the exposure
and gain, and both clock checks. Lines marked `[FILL IN]` need you, and
`[CHECK]` flags anything to look at, such as a clock that could not be
confirmed to 0.05 s. `Shift+J` again cancels.

**The light curve: `phemu_lightcurve`** (phase 141.2), a command-line tool
built beside the app. IMCCE wants the light curve as `<event ID>.dat`
next to the sheet. From the folder with the recording:

1. `phemu_lightcurve <event>.ser --list` lists the moons it finds on the
   first frame, with an index each. Jupiter and anything cut by the frame's
   edge are left out. A moon marked CLIPPED was saturated: IMCCE cannot use
   it, so lower the gain or exposure next time.
2. `phemu_lightcurve <event>.ser --pair-idx A B --ref-idx R --sheet <event ID>.txt`
   measures the two moons of the event together and a third as the
   reference, and writes `<event ID>.dat`.

The pair is measured together on purpose: during an occultation the two
moons merge into one blob, and IMCCE's own example measures them that way.
`--sheet` reads the clock offsets Shift+J measured and corrects every date
to UTC, then notes the `.dat` on the sheet. Without it the dates stay PC
times and the tool says so. Frames with a clipped pixel in an aperture are
dropped and counted, and the points are binned between 0.2 and 2 seconds
to stay within IMCCE's 10,000 lines. The "deepest point" it prints is a
quick look, not a fit: IMCCE does the fitting.

Send the `.txt` and the `.dat` together, after filling in the `[FILL IN]`
lines (IMCCE's address is at the top of the sheet).

Frame times are stamped from the PC clock and are not corrected; the sheet
gives the offset to add. The delay between the camera's exposure and the
PC's timestamp has not been measured, and the sheet says so. If UDP port
123 is blocked, the clock check fails and the sheet says that too.

Frame the moons, not Jupiter: both moons of the event in the frame, a
third as a reference if it fits, and Jupiter outside if you can.

## Weather

RadukCapture fetches the site's weather itself (from 3.3.35): current
conditions from Open-Meteo, 7Timer's seeing forecast, and NWS alerts inside
the US. Leoben, the assistant on Elminster, can still supply it instead.
`WeatherSource=` in `rig.ini` chooses:

| `WeatherSource` | Where the weather comes from |
|---|---|
| `internal` | RadukCapture's own fetch. The default when no `LeobenUrl` is set. |
| `leoben` | Leoben only, as before 3.3.35. |
| `both` | RadukCapture's own fetch, with Leoben standing in when it has no fresh reading. Every 10 minutes the session log gets a `WXCMP` line with the two readings side by side. The default when a `LeobenUrl` is set, so nobody loses weather on upgrade. |
| `off` | No weather. |

The log says which is in use at start: `[WX] Weather source: both`.

**RadukCapture's own fetch**

- **It needs a site in `rig.ini`.** With no site, nothing is asked for: the
  app never requests the weather at latitude 0, longitude 0. The site is
  sent rounded to 0.01 degree, about a kilometre.
- **It goes through the offline switch.** With `Offline=1`, nothing is asked
  of the internet and the `WX` row says `offline`.
- **Timing.** Current conditions every 10 minutes (2 after a failure), 7Timer
  hourly, and NWS alerts every 10 minutes. The log notes each source only
  when its state changes, for example `[SKYWX] open-meteo: ok` or
  `[SKYWX] 7timer: offline`.
- **The last good reply is kept.** It is also saved in `weather_cache.txt`
  beside the other data, so after a restart without internet the app still
  has the last weather, with its age.
- **A missing value stays missing.** A reply with no cloud figure gives an
  unknown cloud, not a guess.
- **No score yet.** The row shows the readings and the source, but no
  GO / CAUTION / NO-GO; a verdict made for planets is the next stage
  (165.3). Open-Meteo's data is licensed CC BY 4.0: weather data by
  [Open-Meteo.com](https://open-meteo.com/).

**Leoben** (`WeatherSource=leoben` or `both`)

- On Elminster, Leoben has to accept requests from the network: in
  `leoben.config.json`, `server.expose_lan` set to `true`, and a
  `LEOBEN_API_TOKEN` in its `.env`. Leoben refuses to listen on the network
  without both.
- On the capture PC, in `rig.ini`: `LeobenUrl=http://elminster:5000` (or
  Elminster's address) and `LeobenToken=` the same token.
- When Elminster is asleep or busy, RadukCapture asks again every 2 minutes.

**What the weather is used for**, from whichever source:

- **The ADC.** Dispersion scales with air pressure and temperature, and the
  model has used a standard atmosphere for your elevation and a fixed 15 C.
  With a fresh reading it uses the real air. Weather services report
  sea-level pressure; RadukCapture converts it to the pressure at your
  elevation before using it -- the sea-level figure would overstate
  dispersion by several percent. The log says when live air is in use:
  `[WX] ADC now uses live air (internal): ... hPa at the site`.
- **The status window's `WX` row** -- see **The status window**.
- **The dew risk** in the warning when the target fades.
- **The hourly `SEEING` line** pairs 7Timer's seeing forecast with the
  seeing measured that hour.
- **The observation record** written beside each capture keeps the cloud,
  temperature, humidity, wind, dew point and pressure at the start of the
  run, and the source.
- **The assistant** is told the measured conditions and where they came
  from, or is told plainly that it does not know.

After 30 minutes without a fresh reading, the ADC goes back to the standard
atmosphere and the record leaves the weather out rather than write a stale
sky.

**In the browser dashboard.** The SKY CONDITIONS panel shows the app's
weather when the dashboard is connected, and says where it came from. Not
connected, the dashboard asks Open-Meteo itself, but only once a location is
set in it. When the app has `Offline=1`, the dashboard asks the internet for
nothing, not even its forecast tab or an address lookup, and says
`NO FORECAST OFFLINE`. A value the reading does not give shows `--`. The ↻
beside the source asks the app for a fresh reading.

---

## Hardware Reference

| Device | Model | Connection |
|--------|-------|------------|
| Telescope | Celestron 8SE SCT | — |
| Camera | ZWO ASI678MC | USB3 |
| Focuser | ZWO EAF | USB |
| Mount | NexStar (8SE single-arm) | USB-serial, port auto-detected |
| Alignment | StarSense AutoAlign | — |

**Plate scale:** 0.203 arcsec/pixel (206.265 × 2.0µm ÷ 2032mm)
**Sensor:** Sony IMX678, 3840×2160, **2.0µm** pixel pitch
**Field of view:** 12.99' × 7.31' at full frame
**EAF safe test position:** 50000 (physical position currently 150000)
**HCG threshold:** gain 182 (above this = high-conversion-gain mode)
**Site:** auto-detected at startup (see *Observing site* below); stored in rig.ini
**ADC model:** Edlen 1966, pressure from site elevation, T=288K

---

## What is remembered between sessions

The app reopens where you left it. `rig.ini` keeps the live camera state and
capture format — gain, exposure, gamma, white balance, USB bandwidth,
resolution and bit depth — written whenever the format changes and on a
clean exit, and restored at launch.

Before v1.2.3 startup wrote fixed values over whatever the rig had been
doing (gain 100, 8 ms, full frame, 8-bit), so a night of tuning survived
only if it had been saved into a profile first, and the capture format did
not survive at all.

A first run still comes up at full frame, 8-bit. Values that cannot be used
are ignored individually rather than discarding the whole session, so one
bad number does not reset everything.

**This is not the same as a profile.** The session is simply where you left
off; a profile is a named set of settings for a target that you choose to
load. See [Profiles](#profiles).

Also remembered: night vision, and whether AI advice is on.

---

## Profiles

A profile is everything the rig should be for one target: gain, exposure,
gamma, white balance, USB bandwidth, resolution, bit depth, focus position,
focus step sizes, tracking mode, file prefix and capture limit.

Profile files name their pixel format in words (`pixel_format=raw8`) under
`format_version=2` (phase 149.0b). A profile saved by an older version, with
the camera maker's number in `img_type`, still loads, and is rewritten in the
new form the next time it is saved.

### Loading one

**Automatically.** Picking an object on the sky map, in the GoTo panel or in
the object info panel loads the matching profile before the slew starts, so
the camera is ready when the mount arrives. Matching is by name: `Jupiter`
finds the Jupiter profile, anything starting `M` and a digit finds `DSO`,
`Moon` finds `Moon`. No match leaves the current settings alone and says so
in the log.

**By hand.** Pick it under **Target** in the CAPTURE section and press
`LOAD`.

Either way it applies the camera settings, drives the focuser to the
profile's focus position, sets the tracking mode, sets the resolution and
bit depth, and fills in the prefix and capture limit — and the panel updates
to match, so what you see is what the camera is doing.

### Resolution: who wins

Three things can set resolution, and the last one to act wins:

| Action | Effect |
|--------|--------|
| Startup | Full sensor, 8-bit — always, whatever the active profile says |
| Loading a profile | The profile's resolution and bit depth |
| A toolbar button | What you pressed, overriding either of the above |

So the app always opens at the widest field, a target selection narrows it
to what that target wants, and `FULL` is always one press away. Loading a
profile does **not** move the ROI on the sensor — a profile says how wide to
read, not where to point; the mount and the tracker decide that.

### Saving one

`SAVE` records the **live** rig, not the stored profile: the camera's actual
gain, exposure, gamma, white balance, USB bandwidth, resolution and bit
depth, the focuser's actual position, and the prefix and limit as they stand
in the boxes. So a night of tuning is kept by pressing `SAVE`, and anything
you change in CAPTURE is part of the profile from then on.

**Worth knowing about your saved profiles.** Because `SAVE` records the live
resolution, and because the emulator used to force 1280x960 regardless of
what was asked, five of the eight profiles on disk — Jupiter, Saturn, Mars,
Venus and Moon — hold 1280x960 rather than the 640x480 (or 1920x1080 for
Moon) their defaults intend. That did nothing before, because loading a
profile ignored resolution. It does something now. If you want Jupiter back
at 640x480 for the frame rate, load it, press `640`, press `SAVE`.

### Focus steps

`+` / `-` move the focuser by the profile's **fine** step and `[` / `]` by
its **coarse** step (10 and 100 by default). Change them in the profile
editor for a target that wants finer control.

---

## Advice from the AI (`W`)

Separate from the chat panel, which answers when you ask. This one watches
and occasionally remarks — at most once every three minutes, never while
recording, and only when something has actually changed. **Off by default**;
`W` turns it on and it is remembered.

**The software decides what is worth saying, not the model.** It checks for
specific conditions — target below 30 degrees, frames dropping, mount not
tracking, sensor cropped small — and the model is only asked to put one of
them into a sentence. That restriction exists because the first version let
the model judge, and it confidently advised *"lowering gain for improved
dispersion visibility"*, which is nonsense. Advice that sounds like
expertise and is wrong is worse than no advice, so the model is no longer
allowed to invent a reason it was not handed.

It never operates anything. It produces a sentence in the notification bar
and nothing else — it has no connection to the camera, mount or focuser at
all.

If nothing is wrong, it says nothing and costs nothing: with no condition
found, the model is not called.

Needs Ollama running locally. If it is not, turning advice on says so.

---

## Display stretch

Two sliders in the IMAGE section, **Black** and **White**, that decide which
part of the brightness range fills the screen:

    shown = (value - black) × 255 / (white - black)

Raise **Black** until the background goes properly black. Lower **White**
until the target's detail fills the range. Anything below black shows as
black, anything above white as white.

**This changes only what you see.** Nothing recorded is affected, which is
the point — it is how a faint target becomes visible without touching gain
or exposure, both of which would change the data. `L` switches it off and on
so you can compare, which is the only reliable way to tell whether a stretch
is helping or just making the noise bigger.

Use the **histogram** below it to choose the points. The three rows are the
real red, green and blue channels, so a single channel about to clip is
visible on its own — before v1.2.3 all three rows drew the same luminance
figure, which looked like a colour histogram but could not show that.

**The automatic stretch** (`Shift+L`, phase 140.3, on by default). Short
planetary exposures look dark even when they are right. The automatic stretch
sets the two points from the target itself, twice a second:
- **black** just above the sky;
- **white** so the target's brightest part shows at about 90% of the screen.

It never darkens a bright target and never amplifies more than 8 times.
Touching either slider, or turning the stretch off with `L`, hands control
back to you. Whenever any stretch is on, the view carries an amber tag:
`DISPLAY STRETCHED (AUTO)`, or `DISPLAY STRETCHED` for your own. The
histogram, the analyser, auto-exposure and the recording still see the raw
data.

The status line reads `Stretch: 20-180` when it is doing something,
`Stretch: on, 0-255 (no effect)` when the window is open wide enough to
change nothing, and `Stretch: off (L)` when disabled. The sliders cannot be
crossed over; the window is held at least 4 levels wide.

---

## Dark and flat frames

Two corrections, both applied to **what you see** and not to what gets
recorded.

A **dark** records what the sensor reports with no light reaching it: thermal
signal, and hot pixels that sit at the same bright spot every frame. Subtract
it and the clutter goes, which matters most while focusing — a field of hot
pixels looks like detail.

A **flat** records how unevenly the optics deliver light: vignetting, and the
shadows of dust on the sensor window. Divide by it and the field evens out.

### Making them

Press **DARK** in the IMAGE section, **cover the telescope completely**, and
leave it. It averages 32 frames and saves the result. Press **FLAT** pointing
at something evenly lit — a white screen, or twilight sky through the scope
— again 32 frames. **CLEAR** deletes both.

It averages rather than taking one frame on purpose: a single dark carries
its own random noise into every frame you subtract it from, which can leave
you worse off than before.

Masters are saved next to your captures, so they are still there next launch.

### When a master stops being valid

A dark is only correct for the gain, exposure and temperature it was taken
at, because that is what determines the thermal signal. The status line says
which state it is in:

| Status | Meaning |
|--------|---------|
| `Cal: none` | Nothing built. |
| `Cal: dark ON` | Being applied, and nothing looks wrong. |
| `Cal: dark ON(taken at a different gain)` | **Still being applied**, but it will be imperfect. Rebuild it. |
| `Cal: dark off(wrong resolution)` | Loaded and **not** applied — you changed resolution since. Rebuild it. |

Resolution and bit depth are hard requirements: a master of the wrong shape
cannot be subtracted at all, so it is refused rather than misapplied. Gain
and exposure are warnings, because an imperfect dark usually still beats
none.

### What they do not touch

The recorded SER keeps the raw sensor data, and so does the sharpness
reading. That is deliberate: your stacker does this better with the whole
frame set, raw data stays useful if a master turns out to be wrong, and the
sharpness scale was calibrated against raw frames.

So for a real session, these are a **focusing and framing aid**. Take darks
and flats with your stacker for the final image.

---

## Filters

A filter is a piece of glass in front of the sensor that only lets certain
colours through. This rig normally has a **UV/IR cut** filter fitted — it
blocks the invisible light either side of what the eye can see, which the
camera is otherwise sensitive to and which arrives out of focus with
everything else.

The app **cannot detect** which filter is fitted. Nothing in the camera
reports it. So you tell it once, in **SETUP → Filter fitted to the camera**,
and it remembers in `rig.ini`. You only need to touch it when you physically
change filters, which on this rig means taking the camera off the scope.

### Why it matters

Atmospheric dispersion is the sky splitting a planet into a faint red and
blue fringe, worse the lower the target sits. How much depends entirely on
**which wavelengths reach the sensor** — so the same night, at the same
altitude, gives wildly different answers depending on the filter:

| Filter | 50° alt | 40° alt | 30° alt | Verdict at 40° |
|--------|---------|---------|---------|----------------|
| UV/IR cut (400–700nm) | 1.13″ | 1.61″ | 2.33″ | recommended |
| None (380–1000nm) | 1.59″ | 2.25″ | 3.28″ | **REQUIRED** |
| IR pass 685 (685–1000nm) | 0.29″ | 0.41″ | 0.60″ | not needed |
| Methane 889 (880–900nm) | 0.01″ | 0.02″ | 0.03″ | not needed |

A narrow band disperses almost nothing — that is part of why IR imaging
punches through poor seeing. A wide-open sensor with no filter disperses
most of all.

So a wrong setting is wrong in both directions that cost you something.
Told "UV/IR cut" when the camera is actually bare, the app under-warns at
40° — it says *recommended* where the truth crosses into *required*. Told
"UV/IR cut" when you are actually shooting IR-pass, it overstates by 4×,
and you would fit a corrector for a problem four times smaller than
reported.

### Where it shows up

- **FOCUSER section** — `Filter: name (low–high nm)`, directly under the ADC
  readout, so the assumption sits beside the figure it drives
- **SETUP** — the picker. An unrecognised band (because you hand-edited
  `rig.ini` with your own edges) leaves the box blank and is left alone
- **Readiness check** — a `Filter` row, listed as CHECK. It never makes the
  dialog appear on its own; it is there to be read on a night the list
  already opened for some other reason
- **AI assistant** — knows the fitted filter and that it is configured rather
  than detected

### Presets

| Name | Band | Use |
|------|------|-----|
| `UV/IR cut` | 400–700nm | Normal colour planetary. The default. |
| `None` | 380–1000nm | Bare sensor. Widest dispersion; an ADC matters most here. |
| `IR pass 685` | 685–1000nm | Cuts through poor seeing; mono-ish detail on Jupiter and Mars. |
| `Methane 889` | 880–900nm | Narrowband Jupiter/Saturn cloud structure. Very dim — long exposures. |

The upper edges are the practical limit of filter **and** sensor together.
The IMX678 still responds past 1000nm, but weakly enough that it moves the
dispersion extremes very little.

---

## Cord wrap

An alt-az fork turns in azimuth with the camera, focuser, power and serial
cables all hanging off the moving part. Enough rotation one way strains the
connectors and can pull the camera out mid-capture.

What matters is **cumulative travel, not azimuth**. 350° and 10° are next to
each other on a compass but 340° apart in travel if the mount went the long
way round, and a cable only cares how many turns have piled up.

- The readout appears next to Az once travel passes 30°, so a fresh session
  is not cluttered with `+0`.
- It is **signed**: 180° out and 180° back reads 0, because the cable is
  slack again.
- GoTo slews are excluded. A jump of more than 90° between readings is not
  continuous rotation, and counting it would make the total meaningless.
- It works **before alignment**, since it only needs the change between
  readings, not where the mount thinks it is pointing.
- Past the limit (`WrapLimitDeg` in rig.ini, default 360°) it warns once.

**Press CABLES OK whenever you free or re-dress the cables.** Nothing else
resets the count — the app cannot see a cable, so it has to be told.

### Before a slew

A GoTo is checked before it runs. If the result would pass your limit you get
the three numbers that matter and which way to jog to unwind:

```
  Travelled since CABLES OK:  +340°
  This slew adds:              +80°
  Would leave you at:         +420°   (limit 360°)
```

The mount chooses its own path for a GoTo — the protocol carries coordinates,
not a direction — so the forecast assumes the **shortest** turn. If even the
short way breaks the limit, the long way certainly does, so the warning is
never a false alarm, and a slew that happens to *unwind* stays silent.

Unwinding is left to you rather than driven automatically: shedding a turn
means rotating up to 360°, a long motion that can snag something by itself.
Jog in the direction given and watch the wrap figure beside Az fall.

Only slews you start are checked. The sky survey fires dozens unattended and
would otherwise raise a dialog on each.

---

## Keep-out sector

An optional arc of azimuth marking something you cannot point at — a wall, a
tree, the house. A GoTo whose target lands inside it asks first.

Direction is part of the definition and cannot be guessed: **300° to 60°** is
the 120° sector crossing north, while **60° to 300°** is the 240° sector the
other way round. The convention is **clockwise from start to end**. Equal
values mean no sector, which is the default.

Set it in the **SETUP** view, where it is drawn as a wedge so you can see
which sector you have actually described, or directly as `KeepOutStartDeg`
and `KeepOutEndDeg` in rig.ini.

---

## Before you start (readiness check)

Once the camera and mount are up, RadukCapture checks whether the rig can
actually image and writes one line to the log either way:

```
[READY] 7 ok, 0 warn, 1 fail, 0 unknown, 3 to check by hand
[READY]   Alignment  FAIL  mount reports NOT aligned - hand control,
          Align > Quick Align (needs no stars)
```

**A dialog appears only if something failed or warned.** A list that appeared
every launch would be dismissed unread within a week, so a ready rig gets
silence.

The check waits up to **two seconds** for the camera's first frame before it
runs. Without that it raced the capture thread it was checking, reported
`Camera FAIL — open but no frames have arrived` on a camera that was about to
work, and then blocked the launch with a modal dialog about it. A re-check you
ask for yourself does not wait, because by then no frames means no frames.

What it verifies for itself: camera streaming, dropped frames, focuser
present, mount connected, **mount aligned**, **tracking actually driving**,
observing site known, disk space, profile loaded. The two in bold are the
ones that will stop a night dead.

Three things it cannot see and must ask you: dust cap off, tube cooled to
ambient, cables dressed. These are reminders and **never** cause the dialog to
appear on their own. Ticking all three also resets the cord-wrap count, since
that is what confirming the cables means.

A fourth row, **Filter**, is stated rather than asked — it names the filter
the app believes is fitted and the band it uses for the ADC figure. It changes
only when the camera comes off the scope, so being asked about it nightly
would be noise; it is there to be read on a night the list already opened.
See [Filters](#filters).

While the dialog is open the rows **refresh every two seconds**. Walk to the
mount, run Quick Align, and watch the row turn green without restarting
anything. **Explain this** opens the Help section for the first failing row,
and **Continue** is always available — this is advice, not a gate.

**READY** in the TELESCOPE section reopens it whenever you want — unlike the
automatic appearance, that always shows the list whatever it says, because
you asked for it.

Set `ShowReadinessCheck=0` in rig.ini to suppress the automatic appearance.
The READY button still works.

---

## Stacking a finished capture

RadukCapture does not produce the final image. It records the video and
shows a quick-look stack so you can judge sharpness at the telescope;
**AutoStakkert** does the real work afterwards, and better than anything
here would.

`K` hands the last finished recording to AutoStakkert. Setting
`AutoStackOnStop=1` in `rig.ini` does it automatically whenever a capture
ends normally - you press stop, or it reaches its frame or time limit. An
emergency stop never triggers it.

**This used to say it did not work. It does — re-measured 2026-10-18.**
For a while this section carried a "known problem, unresolved" warning:
AutoStakkert started, parsed every option, and never began processing.
That was measured again on one 158 MB recording, three ways, with no
reinstall and no reboot in between — on its own from PowerShell, through
this app's own handoff, and through the handoff again with AutoStakkert
already open. All three stacked, in about 51 seconds each.

What originally caused it is unexplained and no longer reproducible.
`AutoStackOnStop` is still off by default, but for its own reason now:
stacking is minutes of CPU on the machine running the capture, and that
should be something you choose rather than something that happens to you
mid-session.

You can always run the command yourself:

```
AutoStakkert.exe "<file>.ser" /apgrid /process /hide /exit
                 /outputdirectory "<folder>"
```

If it ever does misbehave, AutoStakkert's own log at `debug_data\log.txt`
tells you which kind of failure you have, and the two look identical from
outside:

- **Switches logged, but no `Processing started`** — AutoStakkert received
  the instructions and did not begin. That is the failure this section used
  to describe.
- **No `command line ...` lines at all** — the switches never arrived. This
  is almost always a shell problem rather than an AutoStakkert one: run from
  **Git Bash or another MSYS shell**, `/apgrid` is rewritten into a path
  (`C:/Program Files/Git/apgrid`) before AutoStakkert sees it. Use PowerShell
  or `cmd`.

A working run prints `Processing started` about half a second after
`started...`.

### The `_quality.txt` file

Every recording gets one, listing a sharpness score per frame.

**AutoStakkert does not read it.** It measures quality itself, and does so
separately around each alignment point, so a single score per frame could
not drive its choices anyway. The file is for you: it records how the
seeing ran across a capture, and it is what tells you the next morning
which runs were worth keeping.

Before v1.2.2 it held whichever score the analyser happened to have last,
repeated across long runs of frames, and could not rank anything. It is now
measured per frame.

---

## Emulator test switches

The emulator build simulates camera, focuser and mount so the app can be
developed and tested with the telescope packed away. These environment
variables adjust what it simulates. They have no effect on the real
hardware build and are listed here so a test result can be reproduced.

| Variable | Default | Effect |
|----------|---------|--------|
| `RADUK_EMU_JITTER_AS` | 0.5 | Seeing wander amplitude, arcsec |
| `RADUK_EMU_DRIFT_AS` | 0.3 | Mount tracking error, arcsec/sec |
| `RADUK_EMU_ROTATION_DEG` | 0 | Camera angle, to prove the tracker measures its axis transform rather than assuming it |
| `RADUK_EMU_UNALIGNED` | off | Mount reports itself unaligned, reproducing the whole tracking-blocked path |
| `RADUK_EMU_TRUE_FOCUS` | 168000 | Focuser position at which the image is perfectly sharp |
| `RADUK_EMU_BLUR_PX_STEP` | 0.0083 | Blur added per step away from focus |
| `RADUK_EMU_FREEZE_SURFACE` | off | Stops planet rotation, turbulence and moons, so only focus varies |
| `RADUK_EMU_NOISE` | 0 | Per-pixel sensor noise, DN |
| `RADUK_EMU_SEEING_AS` | 0 | Per-frame atmospheric blur, arcsec. Without it every simulated frame is equally sharp, so the seeing score, the capture gate and the stacker's frame selection have nothing to choose between |
| `RADUK_EMU_PEAK_DN` | 0 | Exposes the simulated disc to this peak level. 0 keeps the plain gain/exposure behaviour; ~200 gives a realistically exposed frame with headroom, which is what anything measuring a noise floor needs |
| `RADUK_EMU_TARGET` | *active profile* | Which planet to render. Normally the emulator renders whatever profile is selected and follows every later switch; this pins one for bench work |
| `RADUK_EMU_REMOVE_AFTER_S` | off | Simulates the camera being unplugged after N seconds |

The emulated camera reports the real 3840x2160 sensor but will only **stream
up to 1280x960**. Larger requests are clamped and logged, the way a real
camera reports a mode it cannot deliver — and because every caller reads the
granted size back, the app follows along correctly. So `FULL` and `1920` both
land on 1280x960 in the emulator while `1280` and `640` are exact. Above that
cap the software render plus a 64-slot ring stops delivering frames inside the
capture timeout altogether, which is a dead emulator rather than a slow one.

### Why the focuser starts at 166500

The simulated EAF opens at **166500**, 1500 steps short of the 168000 default
true focus. Both numbers matter, and they are not arbitrary:

- The real rig focuses near **168187**, so the emulator starts in the same
  part of the range. It used to start at 50000, which was nowhere near
  anything real.
- Autofocus searches **±3000 steps** from wherever it starts. From 50000 it
  swept 47000–53000 — about 115,000 steps away from focus — so it could
  never bracket focus from a fresh launch, failed every time, and looked
  broken when it was behaving correctly.
- The 1500-step offset means the emulator opens slightly **soft**. Starting
  exactly at focus would make an autofocus run a no-op that proves nothing.

The focus, noise and seeing switches exist because the simulated planet was
previously rendered perfectly sharp at every focuser position and perfectly
clean in every frame. Nothing that depends on focus or on averaging frames
could be tested at all — an autofocus sweep was fitting a curve to
noise, and a stack of identical clean frames can only ever equal one
frame. Setting `RADUK_EMU_TRUE_FOCUS` gives a focus test a known
correct answer, which is what turns it into a real check.

---

## Numbers that have not met the sky (phase 155.2)

Some of the app's numbers were tuned on the emulator, worked out on paper, or
taken from a datasheet, and have never been checked against a real night.
Examples are the sharpness ceiling, the derived plate scale, the
auto-exposure target band, the 80% seeing bar and the autofocus fade bands.
Each one is marked in the source with a comment, `UNVERIFIED(item N)`, where
N is the item of the clear-night checklist that would confirm or replace it.

The full list, with each value and what would verify it, is
`docs/UNVERIFIED.md`. `tools/unverified_report.py` writes it from those
tags. The build checks that every tag names a real checklist item and that
the list is up to date, so a new untested number cannot slip in unmarked.
When a clear-night item has been run, the value is confirmed or changed and
its tag removed.

---

## Without an internet connection

**Nothing here needs the internet.** Capture, focus, guiding, plate solving
(ASTAP's local catalogues), the planets, Sun and Moon (computed here), the sky
map, the assistant (Ollama on this PC) and the dashboard (this PC or your home
network) all work in airplane mode. A few extras fetch data when they can, and
keep it:

| Extra | From | With no connection |
|---|---|---|
| Jupiter's moon events | JPL Horizons | the cache, three days ahead |
| Near-Earth objects on the map | JPL | none shown |
| Horizons targets (`FOLLOW`) | JPL Horizons | not available |
| Satellite elements | CelesTrak | the cache; positions grow less exact as it ages |
| Mutual events | IMCCE | the cached season, which is complete once fetched |
| The clock check before a mutual event | `TimeServer` (NTP) | the record says the clock was not confirmed |

**Going offline on purpose (phase 151.0).** Put `Offline=1` in rig.ini and the
app never tries the internet: each extra above keeps what it has and says
"offline (rig.ini Offline=1)" instead of reporting a failure. Without the
setting, the app notices by itself: the first time an internet name will not
resolve, it stops trying every internet fetch for ten
minutes, then tries once more, so a dark-site night does not stall on repeated
lookups and a connection that comes back is used again. A server's own trouble
(refusing, busy or broken) is not taken for a lost connection.

Machines on your own network are never treated as internet: Leoben, the
dashboard, Ollama, and a time server given as a LAN name (`elminster`,
`nas.local`) or a private address (`192.168.x.x`, `10.x.x.x`) are always tried.
So a GPS time server on the LAN still confirms the clock at a dark site.

The log says what happened, once per server per spell:
`[NET] skipped celestrak.org: offline (rig.ini Offline=1)`, or
`[NET] ssd.jpl.nasa.gov could not be found (WinHTTP 12007): no network ...`.

Still to come in phase 151: how old each cache is, shown where it is used, and
one action that fetches everything for a trip before you lose the connection.

---

## Troubleshooting

### No signal / black preview
- Check USB connection to ASI678MC
- Check that camera is not in use by another application (close FireCapture if open)
- Restart RadukCapture

### Mount not responding
The serial port is **auto-detected** at startup: RadukCapture probes every COM
port Windows reports as present and keeps the one that answers the NexStar echo
command. The port a USB-serial cable gets is assigned by Windows and changes
when the cable is plugged into a different USB socket, so it is never assumed.

The startup log line names what happened:
- `Mount:  OK (COM4 responded)` — hand control found on that port
- `Mount:  OK (COM4 owned by CPWI)` — port exists but CPWI holds it
- `Mount:  NOT FOUND (probed: COM3, COM4)` — nothing answered on any present port

If it is NOT FOUND:
- Check the hand control is powered on and the cable is seated at both ends
- Check the USB-serial adapter appears under Device Manager > Ports (COM & LPT)
  with no warning icon — a port listed there but not working needs its driver
- If CPWI is running, disconnect the mount in CPWI first, then RadukCapture
  will take the port
- Use `RELEASE` in the TELESCOPE section to hand the port back to CPWI

To pin a specific port and skip the scan, add it to `rig.ini` next to the
executable:

```ini
[Rig]
MountPort=COM4
```

A pinned port is tried first; if it does not answer, the scan continues anyway,
so a stale entry falls back to auto-detect instead of breaking the connection.

### Small moves seem to do nothing

- **Check the rate is what you think it is.** The rate buttons live only
  in the floating `MOUNT CTRL` panel, and they set the arrow keys too.
  Before v1.2.1 the two did not talk to each other, and the arrow keys
  stayed at 9× whatever the panel showed.
- **Watch the motion plot** at the bottom of `MOUNT CTRL` while pressing.
  A moving trail means the command is reaching the mount and the move is
  simply too small to see in the preview. A flat trail means it is not.
- At `FINE` a quarter-second press is a few arcseconds. That is intended
  — it is for centring a planet, not finding one. Use `9×` to travel.
- If the plot reads `MOUNT` rather than `CAMERA` it is only updating
  twice a second, so a brief nudge shows as one step rather than a smooth
  line. That is the readout being coarse, not the mount being stuck.

### Nothing is being recorded, or almost nothing

Check the **GATE** threshold first. The gate only records frames whose
seeing score beats it, and **the meaning of that number changed in v1.2.2**
- the score is now relative to the best recent frame rather than an
absolute scale. A threshold set before then will not behave as you remember.

Re-pick it against the live readout: watch the seeing number for a minute,
note how low it dips, and set the gate a little above that. Set too high it
records nothing at all, which looks exactly like a broken capture.

### Frames dropping (LOST > 0)
- Reduce USB bandwidth slider in CAMERA section
- Close other USB-intensive applications
- The write buffer is 512 slots — sustained drops mean disk write speed is insufficient

### Mount will not track (TRK stays OFF)
An alt-az mount computes its altitude and azimuth rates from **latitude and
local time**. Without them the calculation is underdetermined and the mount
declines to track, however often it is asked.

RadukCapture sends both on connect. Check the startup console:

```
[NexStarMount] Site  -> ... : accepted
[NexStarMount] Clock -> ... : accepted
```

If those are accepted and `TRK` still reads `OFF`, the mount is **not
aligned**. Tracking is gated behind the hand control's own alignment, and
nothing in this application can substitute for it:

> **Hand control -> Align -> Quick Align.** It needs **no stars** - level the
> tube, point north, confirm. GoTo accuracy will be mediocre; tracking will
> work, which is what matters if you find targets by hand.

RadukCapture notices an unaligned mount at startup and offers help with the
hard part of that instruction. "Point the tube north" is an abstraction in a
dark garden, so it asks one question - *standing where you normally sit, the
scope is to my ...?* - and answers in terms of what is actually around you:

> North is behind you. Point the tube back towards yourself - north is on your
> side of the mount, so the tube should face your desk.

The tube ending up aimed at you is correct and counter-intuitive, which is
why it says so plainly. The answer is saved to rig.ini
(`ObserverBearingDeg`), so you are asked once per rig, not once per session.
Dismiss it permanently with **Don't show again**, or set
`ShowAlignHelper=0`.

Afterwards the console should read `Mount reports ALIGNED - leaving its site
and clock untouched`: the app will not re-push a site to an aligned mount,
because changing time or location can invalidate a Celestron's model.

Note that `TRK` reflects a readback of the mount's actual state. A command the
mount merely acknowledges is not reported as success.

`TRK: UNKNOWN` is a third state, and means the mode could not be read at all
after several tries — a link problem, not a tracking problem. It used to be
reported as `OFF`, which sent at least one session chasing the wrong fault.

### Acquisition refuses to start
`[CENTROID]` runs the whole sequence: find the planet, calibrate, centre, lock.
If it cannot start it says why, in the notification bar and the log. The usual
cause is tracking:

- *"mount is not tracking"* - see above. With the mount undriven the planet
  drifts 15"/s, which is 74 px/s at this plate scale; the centring dwell window
  is 10 px, so it is crossed in 0.14 s and can never be satisfied.
- *"planet too faint to detect (frame peak 3%, needs above 8%)"* - it is in
  frame but under the detection threshold. Raise exposure or gain.
- *"no planet detected in frame"* - nothing bright enough is there at all.
  Go and find it first.

Those two used to read identically, which mattered because they need opposite
responses.

**On a small ROI the sequence widens the frame first.** A profile's ROI (the
Star profile's 640x480, say) is too small to find a target after a GoTo's
pointing error, and too small for calibration's moves of a few hundred pixels:
the target sat at the edge and calibration refused it. So when `[CENTROID]` or
PLANET LOCK starts the sequence on a frame smaller than the sensor, it switches
to the whole sensor, finds, calibrates and centres there, and goes back to the
profile's frame, centred on the target, once it is locked. It also goes back
when the sequence ends any other way (RESET, a failure), and right before any
recording starts, so a wide frame is never recorded. Changing the frame by hand
while it is wide leaves your frame as it is. The log says each switch
(`[ACQUIRE]`), and Dash's ROI row reads `WIDE` while it lasts. rig.ini
`AcquireWide=0` turns it off; `AcquireWideMaxW`/`AcquireWideMaxH` cap the wide
frame (0, the default, is the whole sensor).

**The frame never changes during an autofocus sweep.** A sweep compares
sharpness from one frame to the next, so if the lock arrives mid-sweep, the
return to the profile's frame waits until the sweep ends; a sequence started
during a sweep waits to widen the same way. `[RUNS]` pressed while acquisition
is still running waits for it to end (locked, or ended any other way) before
its first sweep or run, and the log says so once (`[AUTORUN] waiting for
acquisition`). If acquisition never ends, the sequence stops after 10 minutes
with *"acquisition did not finish"*.

`[ROI]` is stage 3 of that sequence, not a manual crop tool. To set an ROI by
hand, **drag a box on the preview** instead.

### Observing site wrong or missing
The site is resolved at startup in this order, and the first that answers wins:

1. `SiteLatDeg` / `SiteLonDeg` in rig.ini, if present
2. **Windows Location** - typically accurate to ~100 m, which is far better
   than the 5.6 km needed to land a target in the frame
3. A USB GPS receiver on any serial port (NMEA, passive listen)
4. The site the mount itself already holds
5. A dialog asking for it

If none answers, **no site is sent to the mount**. An unknown site is left
unknown rather than defaulted to 0,0 - which is a real place in the Gulf of
Guinea and would break tracking, the sky map and the ADC model while looking
perfectly configured.

Windows Location needs *Settings > Privacy > Location*, including **"Let
desktop apps access your location"**. If it is refused the log says so and
names the setting.

To pin the site by hand, set both coordinates and `SiteSource=manual` in
rig.ini; an entry marked manual is never replaced by an automatic estimate.

### Autofocus fails / doesn't converge
- **Check the log for `[SHARP] CEILING`.** After a few minutes of imaging the
  app judges its own sharpness scale and says so if it is wrong — a ceiling
  set too low clips the focus curve flat, and autofocus then has no peak to
  find. This is the single most likely reason a sweep returns something far
  from focus. It names the constant to change.
- Ensure EAF is connected (position shows a number, not dashes)
- **Centre the target first.** Sharpness is measured on a 256×256 crop that
  follows the detected planet; with nothing detected it falls back to frame
  centre and reads background noise.
- **Read what it reports.** Since v1.2.0 it tells you which way it failed:
  `focus not bracketed` means true focus is outside the ±3000-step sweep,
  so focus roughly by hand and re-run; `Focus got worse … reverted to
  start` means the fit landed somewhere worse and the focuser has been put
  back, usually for want of contrast.
- A `sharp=0.0%` result was a bug in versions before v1.2.0, not a
  diagnosis — the confirming measurement collected nothing. If you see it
  now, the target really is too faint.
- Check EAF is not at travel limits (safe zone: 10000–490000)
- **It took a nearly focused planet out of focus, to a donut** (3.3.44 fixes the part that let it stay there). Since 3.3.44, autofocus never keeps a result that measures a tenth or more worse than where it began without first going back and comparing the two under the same sky. If the start's half-flux diameter is a tenth better, the focuser returns to the start and says `HFD says the start is sharper` in the log. Before, it only checked when the score had **halved**. On a planet as big as Saturn, that doesn't happen until the image is already turning into a donut, the ring an SCT's secondary mirror makes of a defocused planet. If you see `Focus got worse … reverted to start`, or `the curve was the fade` (3.3.48: a passing cloud or haze while it swept), focus by hand or try again when the sky is steady. The `[AF]` lines show what the sweep saw: send them in, or send the night to Claude.

A move that makes no progress for 5 s is reported as stalled. Long moves are
fine - the sweep's first hop is 3000 steps and is allowed to take as long as
it needs.

### GoTo misses target
- Run `POINT SOLVE → SYNC` to correct mount model at current position
- Ensure StarSense alignment was performed before session
- After each locked target is plate solved, the log says how far off the mount
  was and what the pointing map would have left. Once the map has proved
  itself (the log says "Set PointingMapApply=..."), set that in the rig file and
  GoTos are corrected from it. It stays off until you do.

### The picture looks wrong — grey, red, too bright, monochrome

Before changing any camera setting, check whether a **display mode** is on.
These change only what you see; the recording is unaffected, and nothing in
the camera is broken.

| What you see | Likely mode | Turn it off with |
|--------------|-------------|------------------|
| Greyscale / monochrome | RAW debayer | `[COLOR]` button |
| Everything red | Night vision | `N` |
| Grey, but one colour's detail | Single channel (R/G/B) | `H` until it reads `CH: ALL` |
| Unnaturally bright or smeared | Display FX / stack | `B` until it reads `FX: NORMAL` |
| Briefly very bright and noisy | Framing shot — restores itself after 5s | wait |
| Circles or brackets drawn on it | Reticle | `R` until it turns off |

The AI assistant is given this same list of what is currently on, so asking
it "why does my image look like this" works.

If none of those are on, the cause is exposure, focus, or the sky — not the
display.

### The view has gone narrow / I cropped and cannot get back
Press **`FULL`** on the top bar. `[ROI]` and dragging a box on the preview
both narrow the sensor read-out, and before v1.2.3 nothing widened it again,
so restarting the app was the only way back. It no longer is.

If `FULL` gives you less than 3840x2160, the camera clamped the request and
the app is showing you what it actually got — in the emulator that is always
the case, by design.

### I cannot change the data rate / file size
Two controls, both on the top bar: the resolution buttons and `8b`/`16b`.
A SER file is uncompressed, so the rate is simply width x height x depth x
frame rate — there is no separate bitrate to set. The **USB Bandwidth**
slider in the CAMERA section is a different thing: it limits how hard the
camera pushes the bus, and is what to lower when `LOST` starts climbing.

Both the resolution and bit-depth buttons close the current recording, since
a SER file cannot change format part-way through. Set the format first.

### ADC reading seems wrong
- **Plate-solve once and read the log.** The ADC figure scales with the plate
  scale, and `PlateScaleArcsecPx` in `rig.ini` is calculated rather than
  measured. Every successful solve, from any of the solve buttons, logs
  `Plate scale: solved X"/px vs rig.ini Y"/px` with the focal length that
  implies, and flags anything more than 5% out. Correct it there and the ADC
  figure corrects with it. Use the full sensor at bin 1 for this.
- **Check the filter first.** The `Filter:` line directly under the ADC
  readout says which band the figure was computed over. If that does not match
  the glass actually on the camera, the number is wrong by as much as 4× —
  this is the most likely cause. Fix it in SETUP; see [Filters](#filters).
- ADC uses cached mount altitude — verify mount is tracking and RA/Dec updating in TELESCOPE section
- Ensure mount is polar-aligned (single-arm mount errors compound at low altitude)

---

## What is not built yet

Listed so nobody goes looking for it, and so the assistant does not promise
it. These are planned, not missing by accident.

**This list is derived from the `PLANNED` rows in
[`docs/PHASE-STATUS.md`](docs/PHASE-STATUS.md), and nothing else belongs in
it.** It is written down because the rule was broken: this section spent
some weeks denying four features that the rest of this same file documents
in full — click-to-recentre, the multi-monitor workspace, sensor analysis
and the ADC alignment window. The assistant reads this file, so it was
repeating those denials to whoever asked.

**Calibration frames are a preview aid only.** Dark and flat correction
exist and are applied to what you see, but the recorded SER stays raw — see
[Dark and flat frames](#dark-and-flat-frames). For the final image, take
darks and flats in your stacker.

**Output formats.** No AVI and no TIFF yet. Recordings are SER or a FITS cube,
and snapshots are PNG. Planned: uncompressed AVI recording (phase 147); TIFF
as a 16-bit snapshot, as a recording of one file per frame, and as a SER to
TIFF conversion (phase 148).

**Deep sky, and other cameras.** Planned, not built. The app is a planetary
imager today, tuned to the ASI678MC. Phase 149 will make it read every figure
from the camera itself, for whatever ZWO camera comes next: cooling, long
single exposures, a second camera for guiding, and the ZWO filter wheel. Phase
150 is the outline for deep-sky imaging: sequences of subs in FITS,
calibration frames, focus and guiding on stars, and dithering. With the alt-az
8SE, deep-sky subs stay short.

**Focus aids beyond autofocus.** No multi-star FWHM score and no Bahtinov
mask assistant. **Single-star HFD and FWHM do exist**, on the sharpness
readout.

**Polar alignment assistant.** Nothing guides a polar align. The align
helper covers mount orientation and the keep-out sector, which is a
different job.

**Alignment and framing helpers.** All of these exist, so do not deny them.
`Ctrl`+click the preview and the mount puts that spot in the middle — see
[Centring by clicking](#centring-by-clicking). The GoTo panel's `PUSH-TO`
button repeats a plate solve and says which way to push the scope. `Shift+G`
returns to a past capture's framing — see [Going back to a previous
capture](#going-back-to-a-previous-capture-shiftg-v21). So do the reticle
(four shapes, movable, sizeable, rotatable) and the framing shot — see the
**Reticle** and **Framing shot** sections.

**Display extras.** Nothing outstanding here worth listing. The
multi-monitor workspace exists and remembers where every window goes — see
[The multi-monitor workspace](#the-multi-monitor-workspace-v124); so do
night vision across the whole interface, the single-channel R/G/B view and
the display stretch — see [Display stretch](#display-stretch).

**Analysis extras.** No stellar photometry, no multi-session statistics
dashboard, and no image annotation overlay on the preview. **Sensor analysis
and the smart histogram do exist** — the Gr/Gb split, per-channel headroom in
stops, and which channel is limiting the exposure. **So does solar and lunar
framing**: the GoTo panel reports how a target fits the frame after a slew,
and `Ctrl+Shift+M` runs a mosaic across it. **Lunar features** are in the
GoTo search too: type `Copernicus` and slew to `Moon - Copernicus`. There are
99, and the log says whether each one is lit, past the limb, or how long until
sunrise there. The Moon and its features are pointed at from your site, so set
the site first: without it a lunar slew can be a degree off.

**The Moon map.** MOON on the left rail (under GOTO), MOON MAP on the GoTo panel beside GOTO SELECTED, or `Y` opens it; each closes it again. A Moon map is a project: the whole Moon, cut
into four quadrants (NE, NW, SW, SE, with Mare Crisium's side as east) and
each quadrant into sections, filled in over as many nights as it takes. When
every section is captured the project turns green and says READY FOR MOSAIC,
and the files to stitch are listed for you. Stitching is another program's
job.

- **One project at a time, until it is done.** Opening the map always opens
  the project you are working on, where the last night left it. The line
  under its name says which night of it tonight is, how many sections are
  captured and how many are ready now. Only **NEW PROJECT** (top right)
  changes the project.
- **NEW PROJECT** starts a project, named for the date. When the current map
  holds captures it asks first: START NEW MAP, or KEEP GOING (or Esc). The
  old map is kept in the archive. The sections are sized for the Moon
  profile's frame so that one fits at any camera angle: on the full sensor
  at 0.203"/px that is 5 x 5 a quadrant, 88 on the disc.
- **The picture:** under the grid is tonight's Moon, painted from NASA's
  LRO map (`data\moon_wac_4k.png`) and lit by tonight's Sun, so the
  terminator is where it is in the eyepiece. A place libration turns away
  from you tonight is dimmed.
- **The colours:** green is captured, and shows your own picture of the
  section once the app has made it from the recording (a few seconds after
  the run; kept as `moonmaps\<project>\tiles\<section>.png`). A recording
  with no lit surface in it (cloud, empty sky) makes no picture, and the
  cell stays plain green. An amber outline is wanted and wholly in sunlight now. Unlit or past the limb
  tonight: another night will reach it.
- **Zoom:** the mouse wheel zooms about the pointer, up to 8 times, a drag
  pans, and Home (or 0) shows the whole Moon again.
- **A section's name** counts from the disc's centre: `NE-A1` touches the
  centre, the letter is the row away from the equator, the number the column
  away from the central meridian.
- **The names on the Moon:** every section big enough on screen shows its
  largest named feature (maximise the window to see them on a fine grid),
  and picking a section lists its five largest in the log. They come from
  the IAU's gazetteer, all 9087 adopted names, kept in
  `data\moon_gazetteer.csv`; without that file the map falls back to the
  GoTo panel's 70.
- **START** (or `Ctrl+Shift+Y`) captures every section that is wanted and in
  sunlight, one after another: it slews, waits for the tube to settle, locks
  on the surface with PLANET LOCK, focuses when a sweep is due, records one
  run to the Moon profile's limit, and marks the section green. The same
  button stops it, and so do RECORD and the emergency stop. A run cut short
  or one that came back empty does not count.
- **By hand:** click a section, press **GOTO**, and the next RECORD is filed
  under that section and counted. **MARK** counts a section you captured
  some other way; **CLEAR** forgets a section whose recording was poor.
- **The files** go to `MoonMap_<project>` in the output folder, named
  `Moon_<section>_...`. When the project is complete,
  `<project>_mosaic.txt` in the `moonmaps` folder, beside `captures`, lists
  the newest recording of
  every section with the Sun's height, the colongitude and the libration it
  was taken under.
- **The archive:** the arrows beside NEW PROJECT show earlier maps. An
  archived map is only looked at: START, GOTO, MARK and CLEAR are refused
  on it, and **BACK** (or closing the map) returns to the one you are
  working on. A project made every month or two is how you see the Moon
  change in your own pictures.
- **Dash** has the same map and buttons under MOON MAP; its NEW PROJECT asks
  before it sets a map with captures aside.
- **rig.ini:** `MoonMapSectionsPerSide` (0 sizes it for the frame),
  `MoonMapLock`, `MoonMapRefocusEvery` (sections between focus sweeps, 10)
  and `MoonMapSettleSec` (8).

Sections taken on different nights are lit differently. A stitcher can blend
that, but for a seamless mosaic capture as much as you can in one night near
full Moon. And a section is where the mount pointed, not where a picture was
recognised: with a poorly aligned mount the map can be shifted on the Moon.
Check one section against an atlas before a long run.

**Satellite passes.** The Passes tab in the GoTo panel lists the next 24 hours
of passes from your site. UPDATE ELEMENTS fetches fresh orbits from CelesTrak.
TRACK SELECTED PASS slews to where the window opens, waits, follows the
satellite at up to 3 degrees a second, and records while it is on target.
Escape, any jog, or the stop button ends it at once. Tracking stops at 80
degrees, because the azimuth axis cannot follow near the zenith. Keep the PC
clock and the elements fresh. If the satellite is not on the sensor, `Ctrl+[`
and `Ctrl+]` slide the prediction 0.1 s earlier or later along its track. Once
it is, and if you took a guide calibration that night, the image steers the
pass and the status reads `image lock`.

**A night of satellites.** Tick the passes you want, or press PLAN THE NIGHT to
tick the best set from now on: it leaves out any it cannot slew to in time, and
prefers more targets, then higher and sunlit ones. RUN THE PLAN then flies them
in turn. A missed pass is skipped. A fault in the mount, or Escape, ends the
whole plan. Every target uses the same camera settings.

**A night of planets and stars.** On the Tonight tab, tick the planets and
stars you want and press PLAN TONIGHT. Each gets one visit while it is in GOOD
(45 to 60 degrees) or PRIME (60 degrees and up), worked out from its position
over the night, not from the mount. PRIME is preferred and GOOD is the
fallback. Before each visit comes time to slew, settle, lock and focus. The
slew is worked out from where the tube will be: the target before it, or for
the first visit where the mount points now. It is the bigger of the azimuth
and altitude moves at `SatSlewDegPerSec`, so a visit next to the last one gets
about 5.5 minutes and one across the sky about 6.5. After a pass, or with no
mount, it allows the longest slew. Then 20 minutes of imaging. Visits never overlap, and passes ticked on the Passes tab are kept
clear, with 2 minutes either side. The plan below the buttons says, for each
target, when it slews, when its runs start and end, which window, how many
minutes of each, and its peak height. A target left out says why: it never
reaches 45 degrees, the Sun rules leave it no time, it is not a planet or a
star, or other visits take its window. When a window is too short, a visit is
shortened to as little as 5 minutes and says so.

RUN TONIGHT takes the visits in time order: GoTo, which loads the target's own
profile; PLANET LOCK's full acquisition; then the RUNS sequence. It moves on
when the runs are done or the visit's time is up, and stops the runs if they
are still going. A visit starts early only when the one before finished early
and the target is already as high as its plan puts it. No lock in 5 minutes,
or a slew the cord refuses, skips that target and the plan goes on. STOP
TONIGHT, the emergency stop, or RECORD or RUNS pressed during a visit stops the
whole plan. A slew that never finishes stops it too. Every step and skip goes
to the session log with its reason.

PAUSE stops the plan's clock and nothing else: the runs, the lock and the
mount are left as they are, so you can stay on a target as long as you like,
or do something else. You can tick and untick targets while paused. CONTINUE
then keeps what is done (the target you paused on counts, if its runs had
started), and plans every ticked target not yet visited again from now, by
the same rules: the windows from the sky as it is then, and time to slew
from where the tube is then, lock and focus before each visit. The list keeps what is done above the new
plan.

The plan runs from now to astronomical dawn, or `NightPlanHours` from now if
that is sooner. Daylight is allowed by default, for bright planets and stars;
set `NightPlanSunMaxDeg` to -12, say, for dark sky only. Whatever that is set
to, a target is never visited while the Sun is up and within 30 degrees of it
(`NightPlanSunAvoidDeg`, 10 at the least). Other rig.ini keys:
`NightPlanVisitMin`, `NightPlanMinVisitMin`, `NightPlanSettleSec`,
`NightPlanLockSec`, `NightPlanFocusSec`, `NightPlanPassMarginSec`. Dash has the
same controls under NIGHT PLAN. Not yet tried on the sky: it is checked on the
bench and on two stars in the emulator.

**Anything JPL predicts.** Type a target into the box under the plan buttons:
an asteroid number with a semicolon (`99942;`), a comet (`C/2023 A3`) or a
spacecraft ID, and press FOLLOW. The app fetches 12 hours of positions from JPL
Horizons and flies the first stretch in the tracking band as it would a
satellite pass. Only the target's name and the time are sent, never your site.

**Anything in the frame.** For a rocket, or anything with no prediction, bring
it into the frame and press TRACK WHAT IS IN FRAME. The app follows it from the
image alone, using tonight's guide calibration, and records once it is held. If
it is lost for more than two seconds the mount stops.
This has
not been tried on a real mount, so read item 39 of the clear-night checklist
first.

**ADC alignment tool — built, and dark.** This is the one entry that is
neither "exists" nor "missing". The alignment window is built and tested
(`Shift+A`), and it stays dark until *ADC fitted* is ticked on the READY
screen — because there is no corrector in the train to align. The ADC
*advisor*, which tells you how much dispersion there is and whether a
corrector is worth fitting, works regardless (see [Filters](#filters)).

**Stacking.** Deliberately not built. The quick-look stacker is a preview so
you know that night whether a capture was soft; AutoStakkert does the real
work, and nothing here would beat it.

---

## rig.ini reference

Sits next to the executable, is never committed, and is the only place your
real site is stored. Most of it is written for you.

| Key | Meaning |
|-----|---------|
| `SiteLatDeg` / `SiteLonDeg` | Observing site. Auto-detected if absent. |
| `SiteElevationM` | Metres above sea level; sets the ADC pressure model. |
| `SiteSource` | `manual`, `windows`, `gps` or `mount`. A `manual` site is never overwritten by an automatic one. |
| `MountPort` | Pin a COM port. Empty means auto-detect. |
| `PlateScaleArcsecPx` | The image scale in use, written for you to read; no longer read back as a setting (phase 149.0). It is derived from the camera's pixel size and `FocalLengthMm` x `FocalReducer`, unless `PlateScaleMeasuredArcsecPx` is set. An older rig.ini whose figure differs from the derived one by more than 1% has that figure kept as the measured one. |
| `PlateScaleMeasuredArcsecPx` | The scale measured on the sky (clear-night item 3). Overrides the derived one when set; 0 = not measured. |
| `FocalLengthMm` / `FocalReducer` | The telescope's focal length (default 2032) and what sits in front of the camera: 1 for none, 0.63 for a reducer, 2 for a 2x Barlow. |
| `TelescopeName` | The telescope as files, records and the audience screen name it. Default `Celestron 8SE`. |
| `CameraName` / `CameraWidth` / `CameraHeight` / `CameraPixelUm` / `CameraColor` | The last camera that connected, as it described itself. Written for you when a camera connects; they let start-up and the emulator know the sensor before one does. |
| `GuideRateArcsecPerSec` | Assumed guide rate. Calibration measures the real one and warns if they disagree. |
| `AcquireWide` | `1` (default): an auto-acquire started on a frame smaller than the sensor finds, calibrates and locks on a wide frame, then returns to the profile's frame centred on the target (see *Acquisition refuses to start*). `0` acquires on the frame as it is. |
| `AcquireWideMaxW` / `AcquireWideMaxH` | The wide frame's largest size in pixels. `0` (default): the whole sensor. A smaller figure gives a faster frame on a slow USB link. |
| `WrapLimitDeg` | Cumulative azimuth travel before the cord-wrap warning. |
| `KeepOutStartDeg` / `KeepOutEndDeg` | Obstruction sector, clockwise from start to end. Equal values = none. |
| `ObserverBearingDeg` | Mount to observer, degrees true. Used by the align helper. |
| `ShowAlignHelper` | `0` suppresses the align helper. |
| `ShowReadinessCheck` | `0` suppresses the readiness dialog (the log line remains). |
| `FilterName` | Filter fitted to the camera. One of the presets, or your own label. |
| `NightVision` | `1` restores night vision at launch. |
| `AdvisoryEnabled` | `1` turns on occasional AI advice (`W`). |
| `AdvisoryIntervalS` | Seconds between advice checks; 30-3600, default 180. |
| `LastRoiWidth` / `LastRoiHeight` / `LastBitDepth` | Capture format to restore at launch. See **What is remembered between sessions**. |
| `LastGain` / `LastExposureMs` / `LastGamma` / `LastWbRed` / `LastWbBlue` / `LastUsb` | Camera state to restore. Each is ignored individually if out of range. |
| `WsToken` | The browser dashboard's access token. Generated on first launch; see **The browser dashboard**. Delete the line to have a new one made. Never paste it anywhere public. |
| `WsLan` | `1` = the dashboard server listens on the home network too, not only this PC. Off by default. See **From another device on your home network**. |
| `WsLanOpen` | `1` = a device on the home network (a private address) needs no token. Only with `WsLan=1`. This PC and outside addresses still need it. |
| `LeobenUrl` | Where Leoben is, as `http://name:port` -- for example `http://elminster:5000`. Empty = no weather from Leoben. See **Weather**. |
| `TimeServer` | The time server the PC clock is checked against before and after a mutual event recording (UDP 123). Default `pool.ntp.org`. A LAN name or private address is tried even when offline. |
| `Offline` | Phase 151.0. `1`: never try the internet (JPL, CelesTrak, IMCCE, an internet time server); each keeps its cache and says it is offline. `0` (default): try, and stop for ten minutes after a lookup fails. LAN machines are never affected. |
| `PhemuSite` | The site and telescope name on IMCCE's observation sheet, no spaces, different for each telescope. Default `RadukCapture_8SE`. |
| `LeobenToken` | Leoben's API token (its `LEOBEN_API_TOKEN`). Never logged. |
| `WeatherSource` | Where the weather comes from: `internal` (RadukCapture fetches it), `leoben`, `both` (internal, Leoben standing in, and a `WXCMP` comparison in the session log) or `off`. Unset: `both` with a `LeobenUrl`, otherwise `internal`. See **Weather**. |
| `SkyHorizon` | For the unattended sky run (phase 156): the lowest altitude it may use, by azimuth, as `az:alt` pairs, for example `0:15,90:25,180:20,270:15` for trees to the east. Interpolated between points; one point is a flat horizon. No default: the run refuses to start until it is set. |
| `SkySlewAltMax` | The highest altitude the sky run may slew to, for example `85`. No default. |
| `SkySlewAz` | The azimuth range the sky run may use, as `min..max`, for example `0..360` for no limit (set on purpose) or `300..60` for a range that wraps through north. No default. |
| `SkyStars` | The sky run's star-field group uses these stars, in your order of preference, for example `Vega,Arcturus,Capella`: the first inside the limits with a field around it. Empty: that group is not available. |
| `SkyDarkSunAltDeg` | The sky run works while the Sun is below this altitude, waits for it before starting, and parks when the Sun rises past it. Default `-10`. |
| `SkyParkAltAz` | Where PARK, Dash's park and the sky run send the tube, as `az:alt` in degrees (azimuth from north through east), for example `180:20`. It must sit inside the slew bounds. No default: unset, PARK refuses and the sky run will not start. |
| `CaptureFormat` | `ser` or `fits`, as the toolbar's selector leaves it. |
| `FocuserBacklashSteps` | The focuser's backlash in steps. `-1` (the default) uses the EAF's own figure, else 200. Set it once several autofocus runs' `Backlash measured` lines agree. |
| `FocuserOneSidedMoves` | `1` (default): every focuser move ends travelling one way by at least the backlash. `0` off. |
| `CentralObstruction` | The central obstruction as a fraction of the aperture's diameter: `0.34` (default, the 8SE). CALIBRATE FOCUS uses it to turn the growth of a defocused image into focal-plane travel per step. `0` for a refractor. |
| `CameraZenithAngleDeg` | For RGB ALIGN: where the zenith lies in the camera's frame, in degrees clockwise from the frame's top. Default `180` (toward the bottom). To measure it, jog up in altitude and watch which way the target moves; the zenith is the opposite way. If the colours still look doubled with RGB ALIGN on, this is the setting to change. |
| `FocuserOvershootSteps` | How far a one-sided focuser move goes past its target before coming back, in steps. It has only to be larger than the knob's real slack: too large costs a second or two a move, too small leaves the mirror inside the slack. `-1` (the default): three times the backlash figure, at least 1500. Unlike `FocuserBacklashSteps`, it does not have to be measured to be safe. |
| `FocuserMaxTravelSteps` | The furthest one focuser command may travel, in steps: `12000` (default), about two turns of the 8SE's knob. A longer move is refused and says why; the panel's GO TO asks for a second press and then makes it; autofocus stays within this of where it started. A move is also refused when the focuser's position cannot be read. `0` turns the guard off. |
| `FocuserFinalDirection` | `out` (default, rising step numbers) or `in`: which way every move ends. Set it so the last travel pushes the primary toward the corrector. See [Autofocus Routine](#autofocus-routine). |
| `AutofocusFineStage` | `1` (default): autofocus's fine pass on edge contrast after the coarse sweep, and on its own on the Moon. `0` off. |
| `GrsLonSys2` / `GrsEpoch` / `GrsDriftDegPerMonth` | The Red Spot's System II longitude, the date it was measured, and its drift a month. See [Jupiter's Red Spot](#jupiters-red-spot-moons-and-shadows-j-phases-137-and-144). |
| `AutorunPrimeDeferMin` | On Jupiter, how long `[RUNS]` puts a due focus sweep off for the Red Spot or a moon. Default 30; `0` never. |
| `PanelSnap` | `1` (default): panels snap together and to the screen edges. `0` off; Shift does it for one drag. |
| `DashboardAppWindow` | `1` (default): the dashboard opens as an app window. `0` uses your default browser. |
| `FilterLowNm` / `FilterHighNm` | Band edges in nanometres; these are what the ADC model actually uses. Edit them for a filter that is not a preset — the SETUP picker will then show blank rather than overwrite them. Both are reset to the UV/IR cut default if either falls outside 300–1200nm, or the band is narrower than 10nm, because a nonsense band would push the Edlen model outside its domain and return an authoritative-looking meaningless figure. |

---

## Going back to a previous capture (`Shift+G`, v2.1)

Every recording now leaves a third file beside the `.ser` and the
`_quality.txt`: **`<name>_observation.txt`**, the observation record. It
says what the capture was — target, profile, start and end time in UTC,
frames, format and ROI, exposure, gain, focuser position and
temperature, where the mount was pointing (RA/Dec and alt/az), where
the target sat in the frame, the altitude and airmass across the run,
and the site. It is plain text, one `key = value` per line. A value
the app could not read at the time is **left out**, never written as
0, so a missing line means "not known".

**`Shift+G`** opens a file picker on the capture folder. Pick any of
the three files of a capture. The app reads the record, then:

- If the record names a target the catalogue knows — a planet, the
  Moon, a Messier object, a bright star — it slews to where that target
  is **tonight**. A planet has moved since the night it was recorded,
  so the old coordinates would miss it.
- Otherwise (manual coordinates, or no target) it slews to the
  **recorded mount position**.

Before anything moves it checks, and refuses with the reason on the
notification bar if any fail:

| Check | Why |
|---|---|
| Mount connected and aligned (or plate-synced) | The same condition that enables the GoTo buttons. |
| Site set | Without it the horizon cannot be checked, so recall does not guess. |
| At least 15° up now | The Tonight list's own floor. |
| At least 20° from the Sun | A recall is a slew picked from a file. It will not go near the Sun, even for a solar capture. Recall a solar session by hand from the GoTo panel. |

Then it asks, **defaulting to No**, and shows where it is going. The
slew is the GoTo panel's own, so the keep-out sector and the cable-wrap
warning still apply, the matching profile is loaded, and acquisition
starts after the slew as it does for any GoTo.

**`Ctrl+Shift+G`** is for a capture framed off-centre on purpose — a
planet set to one side to take its moons in. Acquisition centres the
target after the slew; this nudges it back to where the record says it
sat, using the same guide pulses as `Ctrl`+click recentre, with the
same refusals: it needs a guide calibration, refuses while the tracker
is centring (stop it first), refuses a frame size different from the
recording's, and refuses a move longer than a 10 s pulse.

A capture recorded before v2.1 has no record, and recall says so.

---

## Phase History

**The current version is v3.1**; the title bar reads the exact build, such as
`3.1.0`. v2.0 and v3.0 match their tags, and v2.1 to v2.4 have no tag of
their own. Rows marked `pre-2.1` shipped before v2.1 under no number of their
own, and rows marked `3.0+` shipped after 3.0.0 without changing the number.
**From 3.1.0 the number moves with every pull request** (the owner's rule,
2026-11-12, written out in `src/Version.h` and checked by CI): each one bumps
the last part and adds its row here with that version, and the one that
completes a phase bumps the middle part. 3.1.0 is the catch-up for everything
merged after 3.0.0 that had not been given a number. This
table says what shipped, not what has been proven;
[`docs/PHASE-STATUS.md`](docs/PHASE-STATUS.md) says that. Elsewhere in this file, labels from v1.2.4 to v1.2.7 date
when a behaviour changed. They were never declared in `src/Version.h`,
which stayed at 1.2.3 from 2026-10-13 until it was set to 2.0.0 on
2026-10-25, but they are kept because they still put the changes in
order.

| Version | Phase | Feature |
|---------|-------|---------|
| v0.9 | 1–4 | Camera, EAF, NexStar — all devices communicating |
| v0.9 | 5 | Centroid tracking (3-stage: Search → Center → Lock) |
| v0.9 | 6 | GoTo panel (110 Messier + 7 planets, VSOP87, ASTAP) |
| v1.0 | UI | Full Win32 control panel + Direct2D preview |
| v1.0.1 | — | Floating mount controller |
| v1.0.2 | 15 | ADC Assistant (Edlen model, dispersion overlay) |
| v1.0.3 | 11 | Lucky imaging frame tagger (_quality.txt) |
| v1.0.4 | — | GetAltAz render-loop fix (moved to background thread) |
| v1.0.5 | 12 | Auto-Exposure Assistant (I-controller, HCG floor) |
| v1.0.6 | 21 | Clipping Warning Overlay (pixel flash + HUD) |
| v1.0.7 | 22 | Snapshot PNG (WIC encoder, S key) |
| v1.0.7 | 25a | Quality Bar Graph (200-frame rolling history) |
| v1.0.8 | 41 | Pause Capture (P key, amber button, SER stays open) |
| v1.0.9 | 44 | Frame Type Labels + App Icon (.rc embedded) |
| v1.0.10 | 23 | RGB Align Display Mode (V key, auto+manual) |
| v1.0.11 | 24 | Pixel Value Readout (X key, hover tooltip, click to lock) |
| v1.0.11 | — | Notes linked to profile (save/load with PROFILE section) |
| v1.0.12 | 25 | Image Boost/Frame Stack FX (B key, 6 modes, auto-reset on record) |
| v1.0.13 | 26 | Night Vision Mode (N key, red tint via Rec.709 luminance) |
| v1.0.14 | 38 | Single-Channel Display Mode (H key, R/G/B/All cycle, grayscale) |
| v1.0.15 | 49 | Help System (? key, floating window, live search filter) |
| v1.1.0 | — | Build version + compile timestamp in title bar |
| v1.1.1 | — | Hold last frame in preview (stop NO SIGNAL flashing) |
| v1.1.2 | — | Version in preview banner, dark title bar |
| v1.1.3 | — | Initial public release (MIT licence, README) |
| v1.1.4 | 60 | Hotkeys routed from the message pump — work whatever has focus |
| v1.1.4 | 60 | Arrow-key jog: auto-repeat suppressed, stop on app deactivation |
| v1.1.4 | 61 | Mouse reaches the preview — drag-box ROI and pixel readout |
| v1.1.4 | 61 | Sharpness crop follows the planet instead of the frame centre |
| v1.1.4 | 61 | Autofocus waits on progress, not a flat 10 s budget |
| v1.1.4 | 61 | ROI and LOCK state why they refuse instead of doing nothing |
| v1.1.4 | — | Mount serial port auto-detected (NexStar echo probe) |
| v1.1.4 | — | Profile SAVE captures live camera and focuser settings |
| v1.1.5 | 62 | Site and clock sent to the mount — makes alt/az tracking possible |
| v1.1.5 | 62 | ALIGN performs a one-point align and enables tracking |
| v1.1.5 | 63 | Sky map rebuilt as a true zenith-centred polar projection |
| v1.1.5 | 64 | Tracking verified by readback, not by command acknowledgement |
| v1.1.5 | 65 | One-button acquisition: find, calibrate, centre, lock |
| v1.1.5 | 65 | Calibration halved; guide rate measured instead of assumed |
| v1.1.5 | 66 | Observing site auto-detected (Windows Location, GPS, mount) |
| v1.1.5 | 66 | ADC pressure derived from site elevation |
| v1.1.6 | 69 | Acquisition says whether the planet is too faint or absent |
| v1.1.6 | 69 | Tracking reads UNKNOWN when unreadable, no longer OFF |
| v1.1.6 | 67 | Azimuth surfaced — it was read every poll and discarded |
| v1.1.6 | 67 | Cord-wrap tracking as cumulative travel, with CABLES OK reset |
| v1.1.6 | 67 | Quick Align helper — "point north" in terms of your landmarks |
| v1.1.7 | 67 | Pre-slew cord-wrap forecast and warning |
| v1.1.7 | 67 | Keep-out sector — warn before slewing into a wall or tree |
| v1.1.7 | 67 | Rig setup view — check the stored bearings against reality |
| v1.1.7 | 68 | Readiness check: verified at startup, shown only on a failure |
| v1.1.7 | 68 | Live-refreshing rows and "Explain this" into the Help file |
| v1.1.7 | 70 | Acquisition stage and its blocker shown continuously |
| v1.1.8 | 71 | Esc stops the mount unconditionally, including in fullscreen |
| v1.1.8 | 71 | Space (emergency stop) and A (autofocus) actually bound |
| v1.1.8 | 71 | Exposure slider: one unit system, ceiling 10 ms -> 60 ms |
| v1.1.8 | 71 | PLANET LOCK indicator restored when acquisition ends |
| v1.1.9 | 72 | READY button — reopen the readiness check without relaunching |
| v1.2.0 | 73 | Autofocus: real per-position averaging, honest reporting, reverts a bad fit |
| v1.2.0 | 73 | Sharpness metric fixed — was measuring colour, not focus |
| v1.2.0 | 73 | Emulator models defocus and sensor noise, so focus work can be tested |
| v1.2.0 | 74 | Quick-look stacker — aligned, quality-gated preview stack with a score |
| v1.2.1 | 75 | Jog rate now reaches the arrow keys; both rate selectors stay in step |
| v1.2.1 | 75 | Motion plot on the Mount Controller — camera or mount source, labelled |
| v1.2.2 | 76 | Per-frame quality scores; one shared sharpness definition; a ceiling that survives sensor noise |
| v1.2.2 | 77 | AutoStakkert handoff — `K` and AutoStackOnStop. CLI and in-app launch both measured working 2026-10-18 |
| v1.2.2 | 78 | Emulator jog rates follow the real NexStar curve |
| v1.2.2 | 79 | Capture gate fixed; emulator can vary frame quality |
| v1.2.3 | 80 | Both configurations build from one script — the hardware build had silently been broken for four phases |
| v1.2.3 | 80 | Emulator exposure control (`RADUK_EMU_PEAK_DN`); the noise-floor theory tested and disproved |
| v1.2.3 | 80 | Review pass: eight confirmed defects fixed |
| v1.2.3 | 80 | Control panel opens with every section collapsed |
| v1.2.3 | 80 | Focuser GoTo field can be typed into again — a periodic refresh was overwriting it mid-keystroke |
| v1.2.3 | 81 | Emulator renders the selected target, and the focuser starts where the real rig actually focuses |
| v1.2.3 | 81 | Autofocus pass 2: sample vectors cleared before arming, not after, closing a race |
| v1.2.3 | 82 | ADC dispersion computed across the fitted filter's band instead of a hardcoded 450–700nm |
| v1.2.3 | 83 | Filter stated in the focuser panel, the readiness list and the assistant's context; picker in SETUP |
| v1.2.3 | 84 | Resolution and bit-depth buttons on the toolbar — a crop is recoverable, and 16-bit is reachable |
| v1.2.3 | 84 | One shared capture-format path replaces three divergent copies; ROI placed by centre, not origin |
| v1.2.3 | 84 | Startup readiness check no longer races the first frame and false-fails the camera |
| v1.2.3 | 84 | Emulator honours requested ROI up to 1280x960; its FrameSize() no longer ignores its arguments |
| v1.2.3 | 85 | Loading a profile applies its resolution, bit depth and capture limit, and the panel updates to match |
| v1.2.3 | 85 | CAPTURE and PROFILE merged into one section — they held the same values and disagreed |
| v1.2.3 | 85 | Profile focus step sizes reach the `+`/`-` and `[`/`]` keys for the first time |
| v1.2.3 | 86 | The assistant is told the capture format, whether it is a crop, and the real target and profile names |
| v1.2.3 | 26 | Night vision remembered between sessions (the mode itself already shipped) |
| v1.2.3 | 45 | Multi-reticle: crosshair / circle / rings / box, cycled by one control |
| v1.2.3 | 39 | Framing shot — `Shift+S` boosts gain and exposure for 5s, then restores |
| v1.2.3 | 87 | The assistant is told which display modes are altering the image, so "why does it look wrong" starts in the right place |
| v1.2.3 | 88 | Startup restores the last camera state and capture format instead of overwriting them |
| v1.2.3 | 12b | Occasional AI advice (`W`) — conditions detected in code, the model only phrases them |
| v1.2.3 | 27 | Master dark: 32-frame average, subtracted from the preview |
| v1.2.3 | 28 | Master flat: normalised per Bayer position so vignetting is corrected without shifting colour |
| v1.2.3 | 37 | Display stretch (`L`), and the histogram shows the real R/G/B channels for the first time |
| v1.2.3 | 45 | Reticle moves, sizes and rotates (Ctrl+drag, Ctrl+wheel, Shift+wheel) |
| v1.2.3 | 26 | Night vision reddens the whole interface, not just the preview |
| pre-2.1 | 89 | Share view for a second screen (`Shift+V`) |
| pre-2.1 | 90 | A second sky map (`Shift+M`) |
| pre-2.1 | 91 | The emulator gets a field of view, up to the full sensor |
| pre-2.1 | 92 | Object panel, on the knowledge database |
| pre-2.1 | 46 | `[HW]` connects or disconnects all hardware; `MountProxy` |
| pre-2.1 | 47 | `Ctrl`+click recentres on the guide calibration |
| v2.1 | 48 | Observation record beside every capture; `Shift+G` recalls a past capture's framing |
| v2.2 | B3 | The browser dashboard shows the telescope's view, about twice a second |
| v2.3 | — | `[DASH]` opens the browser dashboard already signed in |
| v2.4 | — | The dashboard from any device on the home network, staying connected; weather and sky from Leoben into the ADC, the status window, the observation record and the assistant |
| v3.0 | 30 | Push-to assistant in the GoTo panel |
| v3.0 | 47b | Coordinates under the cursor and a direction rose, from a plate solve |
| v3.0 | 93 | Variable-rate slewing; the preview only produces pixels when the frame changed |
| v3.0 | 94 | Emulator stars, planet sizes from the ephemeris, and a Moon with a terminator |
| v3.0 | 31/32 | Single-star HFD and FWHM |
| v3.0 | 96 | Visual system built to survive night vision |
| v3.0 | 97 | Lunar and solar sizes, tracking rates, framing report, mosaics (`Ctrl+Shift+M`) |
| v3.0 | 98 | Camera panel review: log exposure slider, typed exposure, AE fixes, recording timer wired |
| v3.0 | 99, 100 | Dead code sweep, docking deleted, exposure rails enforced |
| v3.0 | 101 | Panels keep their saved open or closed state |
| v3.0 | 102 | POINT SOLVE → SYNC and PLANET LOCK wired; emergency stop disengages the lock |
| v3.0 | 103, 104 | Object panel and share view text stay inside their windows |
| v3.0 | 105 | The recording timer counts on frame-limited runs |
| v3.0 | 106 | FITS cube output, `[STACK]`, `Shift+K` |
| v3.0 | 107 | ROI following, `[FOLLOW]` |
| v3.0 | 108 | Derotation limit, `[DEROT]`, WinJUPOS file names |
| v3.0 | 109 | Surface lock for a target that fills the frame |
| v3.0 | 110 | Plate scale check on every solve |
| v3.0 | 111 | The toolbar's two rows |
| v3.0 | 112 | Camera controls review: typed values, sliders show the camera, AE hold, light ladder |
| v3.0 | 113 | Autorun, `[RUNS]` |
| v3.0 | 114 | Seeing-aware runs: hold for the sky, rank the runs |
| 3.0+ | 115 | Lunar feature targeting, and parallax on every lunar slew |
| 3.0+ | 116 | Satellite passes and high-rate mount control, on the Passes tab |
| 3.0+ | 117 | Image lock on a satellite pass, and a time nudge |
| 3.0+ | 118 | A night plan of satellites, run in turn |
| 3.0+ | 121 | Follow anything JPL Horizons predicts: asteroids, comets, spacecraft |
| 3.0+ | 122 | Track whatever is in the frame, from the image alone |
| 3.0+ | 123 | The pointing map graded on every solve, and applied when it helps |
| 3.0+ | 124 | `[STATS]`: every session at once, rig and emulator apart |
| 3.0+ | 125 | Scenarios: the emulator build tests itself. Fixed PLANET LOCK after a GoTo, recording limits and names from any tab, and Escape leaving the lock lit |
| 3.0+ | 126 | PLANET LOCK works on a Moon that fills the frame: it guides on the surface itself |
| 3.0+ | 127 | SKY SURVEY works again; satellite recordings are named for the satellite and run the whole pass; a calibration with one dead axis is refused |
| 3.0+ | 128 | Near-Earth objects on the sky map: orange diamonds with their magnitude and an hour's motion; click one to FOLLOW it. `NeoMagLimit` in rig.ini sets how faint |
| 3.0+ | 129 | Snapshots save their true colours (red and blue were swapped); Escape in a camera box shows the camera's value again |
| 3.0+ | 130 | The sky map shows the bright satellites and rocket bodies (squares) live, with a line to where each will be in 30 s; the ones you can see are bright. Every NEO and satellite is named, and the site panel shows the date and the time to the second (local and UT). The line under the map counts them and says how many NEOs `NeoMagLimit` let through |
| 3.0+ | 131 | Click a satellite on the sky map and press FOLLOW to fly its next pass, as TRACK SELECTED PASS does. A pass already under way is met where the satellite will be when the mount arrives |
| 3.1.0 | 132 | The instrument look: one palette shared with the browser pages, night vision that emits red and nothing else, custom-drawn buttons, lists and tabs, dark scrollbars, the Ctrl+K command palette, optional audio cues, the session stats page in the app's look, and the dashboard carried in the exe and opened as its own window, restyled and following night vision live |
| 3.1.0 | 133 | Fixes from the 2026-11-07 night: the target found by area rather than one hot pixel, NexStar commands sent one at a time, tracking kept across stops, a warning when the target fades, and files named for the panel's target |
| 3.1.0 | — | `[PLAY]` looks through a SER with a quality trace; `[TO SER]` and `fits2ser` convert FITS cubes and rescue an unclosed one |
| 3.1.0 | 134 | Panels dock together: they snap, form groups that move and resize together, fill a monitor with F11, go into a dock window by menu or by dragging, and are remembered by Ctrl+Shift+End |
| 3.1.0 | 135 | Recording that survives the disk and the process: a failed write ends the run, the disk is watched, the header counts frames during the run, a stalled camera recovers in about 1.5 s, an empty run leaves no file, and the threads are named |
| 3.1.0 | 136, 137 | Satellite elements as OMM (six-digit catalogue numbers); the Galilean moons' events and the Red Spot's transits |
| 3.1.0 | 139 | Autofocus that takes up the focuser's backlash, and says NOT NOW when focus cannot work |
| 3.1.0 | 140 | Auto-exposure meters the planet within a per-planet band, and the live view can be stretched for the eye |
| 3.1.0 | 141 | Galilean mutual events: predictions, a recording mode with the clock checked, and the light curve for IMCCE |
| 3.1.0 | 142, 143 | Jupiter runs listed for DeTeCt impact searches, and records with central meridians and a submission sheet |
| 3.1.1 | 138 | Once an hour the session log gets a `SEEING` line: the seeing forecast Leoben passed on (7Timer's class, the jet stream at 200 and 250 hPa) beside the seeing measured that hour, to learn which forecast, if any, predicts this site |
| 3.2.0 | 144 | Jupiter's events put to work: on Jupiter, Autorun puts a due focus sweep off while the Red Spot is near the meridian or a moon or shadow is on the disc (30 min at most, `AutorunPrimeDeferMin`), the run list says what was on the disc for each run, and the assistant knows the next 8 hours |
| 3.2.1 | 144.1 | A Jupiter run's observation record (`jupiter_disc`) and FITS header (`JUPDISC`) say what was on the disc as it started: the Red Spot near the meridian, moons and shadows crossing |
| 3.2.1 | B1 | The object panel (Shift+I) on a star no longer paints its description across the field diagram: the text goes beside the diagram or below it, whichever fits larger, and a long line under the figures keeps its second row |
| 3.3.0 | 145 | Autofocus in two stages: the coarse sweep on the half-flux diameter, then a fine pass on edge contrast (20 frames a point, the 75th percentile) that sharpens a big disc's focus and can focus on the Moon; `FocuserFinalDirection` sets which way every move ends, so the last travel can push the mirror against gravity |
| 3.3.0 | 145.5 | The fine pass reads its centre point again at the end: a target that dimmed or brightened while it swept is corrected for, and one that changed by more than 40% keeps the coarse result. On the Moon, autofocus compares the result with where it started and goes back if the start was really better |
| 3.3.1 | 146-152 | Planned: the real camera's throughput measured indoors, uncompressed AVI recording, TIFF (a 16-bit snapshot, sequence recording, SER to TIFF), readiness for any ZWO camera (cooling, long exposures, a second camera, the filter wheel), an outline for deep sky and stars, airplane mode (no internet ever required), and the AI assistant revisited with a preferred and a fallback model |
| 3.3.2 | 153-155 | Planned, from a review of the design: the wiring taken out of main.cpp into a tested application core, the hand-written network code fuzzed and decided on, and the unverified constants measured by the app itself and tested against real recordings |
| 3.3.3 | 146.0, 146.1 | Every run says where each frame it did not keep went: the camera or USB, skipped by the gate or a pause, our buffer, or a gap in the frame times nobody reported; exposure- or transfer-limited; and `ser_drop_audit` gives the same for any SER |
| 3.3.4 | 149.0b | Frames and profiles use the app's own pixel format instead of the camera maker's; profile files carry a format version and older ones still load; CI keeps the maker's names inside its own code |
| 3.3.5 | 149.0 | The camera describes itself: the plate scale is derived from its pixel size and the focal length (or the measured figure), and the full-sensor size, file and record names, the audience screen, the sky map's field circle and the assistant all follow whatever camera is connected. New settings `FocalLengthMm`, `FocalReducer`, `PlateScaleMeasuredArcsecPx` and `TelescopeName` |
| 3.3.6 | 155.1, 155.2 | Every number never checked against the sky carries an `UNVERIFIED(item N)` tag naming the clear-night item that would check it (20 so far), and `docs/UNVERIFIED.md` lists them. `tools/unverified_report.py` writes the list, and the build fails if a tag names an item the checklist lacks or the list is stale |
| 3.3.7 | 155 hygiene | Every message from the capture, mount, focuser, writer and solver code now reaches `raduk_log.txt`, time-stamped, instead of a console window nobody sees: 170 `printf` calls in 27 files go through `CoreLog`, and the build refuses a new one. Autofocus's own lines are written once, not twice. Three tracker functions nothing called were removed. When a target fades during autofocus, the final check after a fine pass now compares the start and the result on the edge metric, measured back to back under the same sky: the coarse score is too flat near focus to tell them apart, and could undo a good result |
| 3.3.8 | 151.0 | Airplane mode, proven rather than assumed: rig.ini `Offline=1` stops every internet fetch, and without it the first failed lookup stops them for ten minutes at a time. The satellite, NEO, Jupiter and mutual-event data say "offline" and keep their caches instead of reporting an error; LAN machines are never affected; the log names each skipped server once |
| 3.3.9 | 156-160 | Planned: phases 157 (fault injection, a soak, independent file checks, a qualification report), 158 (focus through the night), 159 and 160 (QHY and Player One cameras), and 149.6 (RGB on a mono camera); releases now come as milestones, 3.4 reliability, 3.5 planetary, 3.6 hardware, 3.7 science, then 4.0, with deep sky after 4.0. And phase 156: an unattended test run under a real sky. It skips targets outside your horizon limits, records failures and moves on, stops on clouds, a lost target or a stalled device, and parks at the end; then it marks each clear-night checklist item passed, failed or skipped, and hands the night's results to Claude Code on this PC, which writes up its findings and opens a draft pull request |
| 3.3.10 | 153.0 | Nothing changes on screen. The camera, focuser, mount link and frame pipeline now live in one application core that a test can build without a window, and a new test runs connect, focus and record against the emulators in groups. Planned for phase 156: tests in groups run for whatever you are observing, and in the emulator every group on a synthetic sky time |
| 3.3.11 | 153.1 | Nothing changes on screen. Autofocus's wiring (the press-twice when it is not ready, waiting for auto-exposure, the Moon's fine pass, its messages) moved out of the main program into a controller with its own tests |
| 3.3.12 | 153.2 | Nothing changes on screen. Recording's wiring (starting, the one way a run stops, the quality gate, the disk, limit and derotation stops, and whether a finished run is auto-stacked) moved into a controller with its own tests |
| 3.3.13 | 153.3 | Nothing changes on screen. A sequence of runs (`[RUNS]`) is driven by a controller with its own tests: when to focus and record, each run noted, the seeing hold and the Jupiter deferral announced, and the ranked list at the end |
| 3.3.14 | 161 plan, 161.0a to 161.3 | The focuser no longer moves from a position it could not read (a failed read used to count as step 0), and no single command travels further than `FocuserMaxTravelSteps` (12,000 by default): a longer one is refused and the focus status line says why, and GO TO asks for a second press. A second key press during a move adds to where that move ends. Autofocus checks that every move was accepted and arrived, stops the motor when one fails, and says "Aborted" for an abort. A stop cannot be overtaken: once STOP, an abort or the emergency stop has been pressed, no move of a running sweep can start, and the panel's STOP now ends a sweep. `Ctrl`+click on AUTO FOCUS runs CALIBRATE FOCUS, which measures the knob's slack, how well a position repeats, and the focal-plane travel of one step (so the focus zone in steps), and appends them to `focus_calibration.ini`. Focuser moves now go past by `FocuserOvershootSteps` (1,500 by default) before coming back, apart from the backlash figure, so an unmeasured backlash no longer leaves the mirror inside the knob's slack. The preview can no longer step back to an older frame when the camera pauses, and autofocus only counts a measurement from a frame taken after the focuser had settled. Auto-exposure, which already settled before a sweep and was held through it, now also gets a turn between the coarse sweep and the fine pass, at the coarse focus, so an exposure set on a defocused disc cannot clip the focused one. With it, the plan for phase 161 from a first-principles review of the focuser and autofocus. |
| 3.3.15 | 157.0 | A camera that stops sending for 15 s (or five frames' worth on a long exposure) now ends the recording and says so; it used to leave a time-limited run open for ever. Behind it, the emulator can be told to fail (`RADUK_EMU_FAULT`: a focuser stall, a camera drop short or for good, a GoTo that never arrives), and a new test checks the app's safe answer to each |
| 3.3.16 | 157.0b | A GoTo still slewing after 3 minutes is stopped and reported, instead of showing SLEWING for ever; a survey whose slew does not finish stops rather than solving in the wrong place |
| 3.3.17 | Testing | Nothing changes in the app. `docs/TESTING.md` is the one guide to testing it: the bench tests, Dash tested in a browser with Playwright (offline, and live against the running app, on a desktop and a phone-sized screen), the scenarios, and the sky. Every control at the telescope is to be reachable from Dash and tested through it, and the tests run by themselves so Claude only reviews their results. The first Dash test found that Dash asks the internet for a weather forecast, owed as 151.4 |
| 3.3.18 | profile per target, focus verdict | A GoTo or an ALIGN now switches the active profile to the target's own, and makes one when there is none (`profilesega.ini` for Vega), started from the Star profile for a star, DSO for a Messier/NGC/IC object, and otherwise the profile in use, with an empty note of its own. The status bar's `[name]`, the OUTPUT tab's profile and its notes therefore follow what the scope is pointed at; before, a GoTo to Vega kept Jupiter's. A lunar feature uses the Moon's profile. Profiles carry the camera they were made for (EDIT → Camera, offered the connected camera; blank means any), and a target with profiles for two cameras gets the one for the camera connected. Under the histogram, SHARP / PK / AV is replaced by a focus verdict: FOCUSED, SOFT or OUT against the live sharpness just after the last successful autofocus on this target, the figure as a percentage of that, and RUN AF when there is no reference yet. |
| 3.3.19 | 157.1, 157.2 | Nothing changes in the app. Every build now writes SER and FITS test recordings with the app's own writers and reads them back with readers that share none of its code (astropy for FITS, a SER reader written from the specification), checking every pixel, the header, the frame count against the file size, the time cards and the timestamps, and a SER from a run that crashed mid-recording. `tests
| 3.3.20 | 157.0c | PLANET LOCK and `[CENTROID]` on a small ROI now work after a GoTo. On the Star profile's 640x480 a GoTo's pointing error left the star at the frame's edge, calibration refused it, and the first soak made 0 locks in 7. The sequence now switches to the whole sensor to find, calibrate and centre, then goes back to the profile's frame centred on the target once locked; also when the sequence ends any other way, and right before a recording starts, so a wide frame is never recorded. rig.ini `AcquireWide` (on), `AcquireWideMaxW`/`AcquireWideMaxH` (0: whole sensor). Dash's ROI row says `WIDE` while it lasts. A RESET part way through a sequence now ends it: it used to leave the sequence marked as running until the next acquire. Dash's older telemetry strip (TARGET, GAIN, ROI, ...) now shows: a misspelt flag had kept it hidden since the first release. A 30-minute soak made 4 locks in 4 cycles, none refused. The frame no longer changes during an autofocus sweep: `[RUNS]` waits for acquisition to end before its first sweep or run, and a lock that arrives mid-sweep goes back to the profile's frame when the sweep ends. The first 4-hour soak found 28 sweeps in 30 with the frame changed part way through. |
un-soak.ps1` runs the emulator for hours through GoTo, lock, autofocus and runs, and fails on a leak (handles, memory, threads), a timestamp that does not move forward, or a frame lost without a reason. In the emulator, a GoTo to a star the emulator does not know by name (Kochab, Pherkad) now shows a star instead of Jupiter, and a star well out of focus is the doughnut an SCT makes, bright enough to find and focus on, where it used to fade to nothing; Mars has its dark markings and a south polar cap and turns at its own rate; Mercury, Venus and Mars are drawn at tonight's phase, lit from the Sun's side, so Venus is a crescent when it is one; and Mercury, Uranus and Neptune are drawn as themselves |
| 3.3.21 | 162 | The Moon map (`Y`). A project maps the whole Moon by quadrant and section, sized so a section fits the frame at any camera angle (5 x 5 a quadrant, 88 sections, on this rig's full sensor), and fills in over as many nights as it takes: green is captured, amber is wanted and in sunlight now. START (`Ctrl+Shift+Y`) captures every wanted section in sunlight in turn, with the slew, PLANET LOCK on the surface, autofocus and RECORD the app already had; GOTO and RECORD do one by hand. When every section is green the project says READY FOR MOSAIC and lists the files, with the light each was taken in, for a stitching program. Dash has the same map under MOON MAP. rig.ini `MoonMapSectionsPerSide`, `MoonMapLock`, `MoonMapRefocusEvery`, `MoonMapSettleSec`. Not yet run on the Moon: the bench tests, the scenario and the Dash test pass in the emulator. |
| 3.3.22 | 157.3 | At connect, the session log now names each device: an `Identity:` line with model, firmware, SDK and serial for the camera, the focuser and the mount (the EAF's firmware, the NexStar hand control's version; ZWO cameras do not report a firmware version, so it reads "not reported"). `docs/QUALIFICATION.md` lists the configurations actually run, with measured figures only, each with its date and source. So far: the ASI678MC (SDK 1.41), Celestron 8SE and EAF (SDK 1.7.7) on 2026-11-07, where three Uranus runs held 51.1 and 37.0 fps (exposure-limited) with no frame dropped and no gap in 20,374 frames; and the emulator's write path and soaks. The camera's full-sensor rate over USB, autofocus repeatability, pointing error and firmware versions are marked not yet measured. |
| 3.3.23 | 162b | The Moon map has buttons: MOON on the left rail, under GOTO, and MOON MAP beside GOTO SELECTED on the GoTo panel. Before, only `Y` and the command palette opened it. The rail's buttons are 50 px tall instead of 56, so eleven fit. |
| 3.3.24 | 163 | A night plan for planets and stars, on the GoTo panel's Tonight tab. Tick targets and press PLAN TONIGHT: each gets one visit while it is in GOOD (45-60 degrees) or PRIME (60 and up), PRIME first, worked out from its computed position over the night, with time to slew (from where the tube will be), lock and focus before it, never overlapping another visit or a pass ticked on the Passes tab. The plan says when and why for each target, and why any was left out. RUN TONIGHT takes them in turn: GoTo with the target's profile, PLANET LOCK, then RUNS; it moves on when the runs are done or the visit's time is up. PAUSE holds the plan, to stay longer on a target or change the ticks; CONTINUE plans what is left again from now. No lock in 5 minutes skips a target; STOP TONIGHT, the emergency stop, or a person stopping the runs stops the plan. Never within 30 degrees of a risen Sun. Dash has the same controls under NIGHT PLAN. rig.ini `NightPlanVisitMin`, `NightPlanMinVisitMin`, `NightPlanSettleSec`, `NightPlanLockSec`, `NightPlanFocusSec`, `NightPlanSunMaxDeg`, `NightPlanSunAvoidDeg`, `NightPlanPassMarginSec`, `NightPlanHours`. Not yet tried on the sky. |
| 3.3.25 | 157.0 (camera replug) | A camera whose USB cable is pulled and plugged back in now comes back by itself, at the format, gain and exposure it had. Before, the capture loop retried a stop and start on the old connection every second, which cannot revive an unplugged ZWO camera: its handle is dead. Now it closes it, finds the same camera again by model and serial (it may come back under a new ID), opens it and replays every setting. The emulator's camera now dies on an unplug the way the SDK's does, so `fault_injection camera-blip` proves the re-open; `camera_replug` tests the replay. Clear-night item 61 checks it on the real camera. |
| 3.3.26 | 154.0a | Dash without the access token can watch and stop, nothing else. With `WsLanOpen=1`, a device on the home network could connect with no token and send every command, and nothing checked which web page opened the connection, so any page in any browser on the network could try to drive the mount. Now a tokenless client gets the telemetry and the preview and may send only STOP (mount, recording, autofocus, the Moon map, the night plan); anything else is refused, and Dash says to enter the token. A browser page from the internet is not admitted at all. With the token, nothing changes. Also in this release: the plans from the 2026-11-17 review (157.0d, 157.0e, Phase 164, the assistant's advise-only rule). |
| 3.3.27 | 157.0d | A disk that falls behind during a recording is now said while it happens: the first frame the write buffer cannot take puts "Frames dropped: the disk is not keeping up with the camera" on the bar, once a run, and the run goes on. Before, it was said only at the end, as "reduce USB BW or exposure", which blamed the wrong thing: these drops are the disk's, and USB drops are counted separately in the run's record. Closing the app while recording ends the run through the same code as every other stop, so the SER's header counts its frames, the observation record is written, and nothing is stacked. In the emulator, `RADUK_EMU_FAULT=disk-slow@T+D` makes every write take 200 ms for D seconds from T seconds into each run. `fault_injection` has three new groups: `disk-slow-fits` (a 60-frame run loses nothing), `disk-slow-overflow` (74 frames lost, all 74 counted, 0 gaps unexplained) and `quit-mid-run`. |
| 3.3.29 | 162c | The Moon map is one long project. Opening it always carries on with the project you are working on, which is now remembered (`moonmaps\active.txt`) instead of guessed from the newest file, and the line under its name says which night of it tonight is, how many sections are captured and how many are ready now. The main row is only what a night needs: GOTO, MARK, CLEAR, START. NEW PROJECT, at the top, asks first when the current map holds captures (START NEW MAP or KEEP GOING); the old map goes to the archive. The arrows beside it show earlier maps without making them current: on an archived map START, GOTO, MARK and CLEAR are refused, and BACK or closing the map returns. Dash's NEW PROJECT asks the same, and its PREV and NEXT are gone, since Dash always shows the current map. Gated by `moon_map_run` section 11 and `tests/scenarios/moon-map-resume.scn`. |
| 3.3.30 | 162d | The Moon map knows every named place on the Moon: the IAU/USGS gazetteer, 9087 adopted names with their size and what each name honours, is kept in `data\moon_gazetteer.csv`. Each section big enough on screen shows its largest feature whose name fits whole (named craters before lettered ones), and picking a section lists its five largest. Also kept: NASA's LRO WAC map of the whole Moon (`data\moon_wac_4k.png`, 4096 x 2048 grey, from the SVS CGI Moon Kit), which 162e will draw under the grid, and a WIC image reader for it. `data\SOURCE-moon.txt` says where both came from. The section line is shorter: `NE-B4  21°N 64°E  in sunlight, Sun 17°  not captured`. Gated by `lunar_gazetteer` (33 checks: Copernicus, Tycho and Mare Crisium in the sections worked out by hand; the map the right way round) and `tests/scenarios/moon-map-names.scn`. |
| 3.3.31 | 157.0e | The focuser's position is remembered and checked. It is saved to `data\focuser_position.ini` after every completed move and when the focuser closes, and compared on every open. ZWO EAFs are reported losing their number over a power loss (reading 1000 against 13785, or 0, 2 or about 4761); when the number disagrees with the saved one by more than 5 steps, or the focuser's range changed, the bar says so with both numbers, and every absolute move is refused (GO TO, a profile's focus position, autofocus, a refocus, CALIBRATE) until you answer. Pressing AUTO FOCUS, CALIBRATE or GO TO asks: Yes puts the saved number back (EAFResetPostion; nothing moves), No keeps the focuser's number, Cancel decides later. Dash shows a POSITION IN DOUBT card in the EAF panel with both numbers and the same two answers, PUT BACK and KEEP. Relative steps (the jog buttons) still work, because a step means the same whatever the number. A focuser that stops answering is re-opened by itself, by serial, after 2 s and every 3 s, and its number judged again; nothing moves as part of it. In the emulator, `RADUK_EMU_FAULT=eaf-drop@T+D` unplugs the focuser at T s and brings it back D s later at 4761. |
| 3.3.32 | 162e | The Moon map shows the Moon. Under the grid is tonight's Moon: NASA's LRO map lit by tonight's Sun, from the same arithmetic that decides which sections are in sunlight, so the picture and the colours cannot disagree. A captured section shows your own picture of it: a few seconds after a run (or a GOTO and RECORD) credits it, the sharpest of 24 frames spread through the recording is cropped, scaled to 256 x 256 and kept beside the project, off the thread that draws. Over the nights the map becomes your photograph of the Moon. A recording with no lit surface in it (no frame sampled above 4% of full scale) makes no picture, so cloud never leaves a black hole in the map. CLEAR takes the picture with the capture. The wheel zooms about the pointer, a drag pans, Home shows the whole Moon. An archived map is drawn fully lit. Gated by `moon_render` (18 checks: the terminator at first quarter on the central meridian, and on 88 sections the picture agrees with the map), `moon_tile` (20: the sharp frame picked over blurred ones, from a SER written by the test; empty sky makes no tile), `moon_map_run` section 12, and `tests/scenarios/moon-map-picture.scn`. |
| 3.3.33 | 165 (plan) | A plan to stop depending on Leoben for the weather. RadukCapture will fetch and score the sky itself (Open-Meteo, 7Timer, NWS), always through the offline switch, so a night at the scope no longer needs the gaming PC awake. Site and focal length come from `rig.ini`; a missing value stays missing rather than being guessed. Leoben stays available as a source, side by side for a few nights, until the readings agree. No code yet: see `docs/Phase165-Plan-Sky-Weather-Without-Leoben.md`. |
| 3.3.34 | 165.0a, 165.1 | The weather, read and fetched by RadukCapture itself, not yet switched on. `core/SkyWeather.h` reads Open-Meteo's current conditions, 7Timer's astronomy forecast and NWS alerts, and keeps Leoben's dew, wind, cloud-layer and dust thresholds in metric. A value that is missing or unreadable stays unknown: no cloud figure is not 15% cloud, and a clear night's 24 km visibility is a number (Leoben's weather broke on it). 7Timer's seeing is now read from the key 7Timer actually sends, and its wind is a class, not knots. `core/SkyWeatherClient.h` asks the three services on their own cadences through the offline switch, never without a site, sends the site rounded to about 1 km, keeps each source's last good reply, and caches them so a restart offline still has the last weather with its age. Gated by `sky_weather` (75 checks) and `sky_weather_client` (42), on replies captured for public places. 165.2 connects it to the app. |
| 3.3.35 | 165.2 | RadukCapture now fetches the weather itself. `WeatherSource=` in `rig.ini` picks `internal`, `leoben`, `both` or `off`; with it unset, a rig with a `LeobenUrl` runs on `both` and one without on `internal`. In `both`, Leoben stands in whenever the internal fetch has no fresh reading, and every 10 minutes a `WXCMP` line in the session log compares the two, for a few parity nights. Everything that used Leoben's weather now reads whichever source is chosen: the ADC's live air, the dew risk on a fade, the hourly `SEEING` line (7Timer's forecast now reaches it; Leoben's never did), the observation record, the status window's `WX` row (which now names the source) and the assistant. Nothing is fetched without a site or with `Offline=1`. Gated by `site_weather` (40 checks). |
| 3.3.36 | 165.4 | The browser dashboard shows the app's weather. Connected, its SKY CONDITIONS panel takes the reading the app uses and names the source (RadukCapture's own fetch or Leoben), with no request of its own. Not connected, it asks Open-Meteo only once a location is set (it used to ask for latitude 0, longitude 0 on every load). With `Offline=1` in the app, it asks the internet nothing at all, forecast tab and address lookups included. A value not given shows `--`. Several made-up numbers are gone: 15% cloud when a reply had none, AQI 42, transparency 8.2/10 and seeing 2.8" that never came from any source, and an imaging score of 50 with no reading. New ↻ (`weather_refresh`) asks the app to fetch now. Open-Meteo is credited under the panel. Closes 151.4. Gated by `specs/weather.spec.js` and `offline.spec.js` (Playwright, desktop and phone) and `telemetry_night`. |
| 3.3.37 | 155.4a | The app keeps a night's evidence. Through every session it records, in `logs\validation_<night>_<time>.json`, what the clear-night checklist needs to settle the constants still marked unverified: each plate solve's scale against the configured one, how sharp the real frames are by target, exposure and gain (and how many sit at the 100% cap), the seeing figure through the night, every autofocus result with the backlash it measured on that sweep and the fine pass's own figures (where it moved focus from the coarse result, its fade check, its curve, and how noisy the edge metric was at each point), and the target's brightness and clipping against the profile's exposure band. Frames during an autofocus sweep or a slew are left out. `python tools\validation_report.py logs` joins any number of nights into one report, and recommends a new value only when enough nights agree, with a confidence interval; otherwise it says what is missing. Nothing is changed by it. Gated by `validation_log` and the report's self-test on CI. |
| 3.3.38 | 156 (rules) | The rules of the unattended sky run, ahead of the run itself. New `rig.ini` limits with no defaults, so the run refuses to start until they are set: `SkyHorizon` (a horizon profile by azimuth), `SkySlewAltMax` and `SkySlewAz`. Before starting it also checks the site, an aligned mount, the camera and room on the disk, and lists every reason it will not go. Tests come in five groups (any planet, Jupiter, the Moon, a star field, tracking); a group runs only on a target it fits and with the devices it needs, and a target below your horizon or setting too soon is skipped, not failed. Through the night: a failure is recorded and passed over, a lost target fails that test, clouds hold the run and the test is tried again when the sky clears, a stalled device stops the run and parks, and it parks at the end of the dark window or the list. `python tools\sky_checklist_update.py field-results\<night>\results.json` then marks the clear-night checklist with what passed, failed or was skipped, and never from an emulator run. A group whose result needs your eye (comparing stacks, judging focus by eye) is marked COLLECTED, never PASSED; the star-field group uses your own `SkyStars` list; the run works while the Sun is below `SkyDarkSunAltDeg` (-10 by default). Gated by `sky_run` (65 checks) and the updater's self-test on CI. The run itself, through Dash, comes next. |
| 3.3.39 | 156.2a | The sky run's first group, and park. **PARK** is a new button in the mount window and in Dash: it slews to `rig.ini` `SkyParkAltAz=az:alt` and turns tracking off, with no default position, refusing (and saying why) outside the slew bounds, unaligned or with no site. Dash gets a **SKY RUN** card: start, the plan with each group's status, the step running, STOP (parks). The app runs the night itself, so a closed browser does not stop it. The **tracking group** runs on any bright target: a box change from full to a third to a sixth and back with the lock held (item 48), a 200-pixel jog re-centred within 25 s (49), and the lock made in a small box, then a calibration refused near an edge with "centre it roughly" (50). The other groups say not built yet. ESC stops the run without a park. Gated by `sky_run_controller`, `park_controller`, `sky_run`, and the Dash specs `park` and `skyrun`. |
| 3.3.40 | 156.2b | The sky run's **planet group**. On any planet in view, after the lock: autofocus from +700 and from -700 steps (item 47's repeatability, and the fine pass for item 58), a 60-second throughput recording judged on unexplained drops and the camera's own rate (item 13), and the exposure ladder at 2, 5, 10 and 15 ms with the gain chosen so the highlight sits near 70% (item 55). The gain is found by halving the camera's own range, so it works on any make. **Each checklist item now gets its own status**, in `results.json` and in `sky_checklist_update.py`, so the ladder is never marked passed because throughput was. The camera's exposure, gain and AE are put back afterwards; the run's recordings are not auto-stacked. Gated by `sky_run_controller` (61 checks), `sky_run` and the updater's self-test. |
| 3.3.41 | 168 | **RGB ALIGN fixed.** It moved blue and red apart by one and a half times the colour spread it computed, so the preview looked doubled at the scope. Blue and red now move together by exactly that spread, split around green, along the zenith's direction in the frame (rig.ini `CameraZenithAngleDeg`, default 180 as before). With an ADC fitted, it adds nothing of its own. Display only; recordings were never affected. Gated by `rgb_align`. |
| 3.3.43 | 172 (fix) | **Tracking is no longer reported as refused when the mount accepted it.** On 2026-11-21 every tracking-on command was acknowledged and the mode check right after it failed, all night, while the mount tracked; the app said "mount refused tracking" and `T` then switched tracking off. Now that case reads "tracking sent, confirming" and TRK: ? until the position poll confirms it (or reports it not running); `T` and TRACKING ask for on when the mode is unknown. The mount window says **TRK: APP (lock)** while PLANET LOCK drives the axes (it said OFF). **An Xbox controller** (Bluetooth or USB, nothing to install): stick or D-pad jog through the arrow keys' path (the lock pauses and re-centres), B is ESC, A is PLANET LOCK, X is RECORD, LB/RB the jog rate; Y (autofocus) and the triggers (focus) are off until phases 167 and 169. Gated by `tracking_confirm`, `tracking_state` and `pad_map`; plan and the night's findings in `docs/Phase172-Plan-Gamepad-And-Night-Findings.md`. |
| 3.3.44 | 167 | **Autofocus no longer keeps a result that's worse than where it began.** At the scope on 2026-11-21 it took a nearly focused Saturn out to a donut and stayed there. The check that should have caught it only stepped in once the focus score had halved, and on a disc as large as Saturn the score is still over half while the image is already turning into a donut (shown on a synthetic Saturn defocused through an SCT, `focus_donut`). Now any result a tenth or more worse is compared with the start under the same sky. If the start's half-flux diameter is a tenth better, it goes back there, whatever the edge score says. The cause of the bad sweep itself is still to be read from that night's `[AF]` lines. Gated by `focus_donut`, `af_keep` and `autofocus_saturn`. |
| 3.3.45 | 166.0, 156.2c, 166.1, 166.2, 166.3, 156.4 | **The SKY RUN window, in the app (`Shift+R`).** It shows where the run is, the step running, every reason it won't start, and the plan with each group's and each item's status, with START and STOP (STOP parks). Dash's card now mirrors it. Your rule is written down and checked: the app is the instrument, and Dash is the remote for an iPhone or iPad and the test harness. CI reads every command Dash can send and fails if one has no control in the app itself, unless an open stage owes it; none is owed now. **The weather window (`Shift+W`, 166.1):** the whole reading, in °F and mph with °C and m/s, its source and age, and ↻ REFRESH, which Dash's button now mirrors. **Dash no longer makes things up (166.3):** the NASA / EVENTS and FAA / WILDFIRE panels (fixed lists written into the page, not data), the data-source lights for sources nothing asked, the wind score's deductions written for one telescope, and the forecast's assumed 10 mph wind are gone; a calm night's wind score is now 100. **Park's state on the mount window's PARK button (166.2):** PARKING..., PARKED, PARK FAILED (with the reason) or PARK (unset); before this, only Dash showed it. **Sending a night to Claude (156.4):** `tools\sky_handoff.py` takes any night: a sky run (the checklist marked from its results), a normal imaging night, or a Dash test night with Playwright's report. It bundles the summary, log lines, recordings list, Autorun rankings and validation log, commits them on the night's own branch in a separate worktree, pushes, and starts Claude Code to write the findings and open a draft pull request. It never merges. The log gives the exact command after a sky run and when the app closes. Also fixed: the sky run's log lines said `[SKYRUN]` twice. Gated by `sky_run_view`, `weather_view`, Dash's `dash-honest.spec.js`, `command_palette`, the parity check, the bundle and hand-off self-tests (a throwaway git repo and a stand-in for Claude), and `tests/scenarios/sky-run-refused.scn`, `sky-run-window.scn` and `weather-window.scn`. |
| 3.3.46 | 167.5 | **Autofocus checks its own sweep before trusting it.** At the scope on 2026-11-22 Saturn faded about fourfold in haze halfway through a sweep; the return pass read the same low score everywhere, and autofocus moved 2,700 steps away from a start that was already in focus. Now when the outward and return passes disagree at the same positions (by over 1.5 times, as a median), or the scores show no peak, it goes back to where it started and says why in the log (`[AF] Sweep check`). If the start itself has faded to under half when it is re-measured, the start stands. A sweep kept only because the sky changed no longer sets the focus readout's reference, which had made a focused planet read OUT. The status bar and Dash now say **APP (lock)** while PLANET LOCK drives the mount, as the mount window does. Gated by `af_sweep_judge`. |
| 3.3.47 | 172 (fix) | **PLANET LOCK no longer overshoots and gives up after a long centring.** On 2026-11-22 a lock started with Saturn 480 px off centre on the full frame: while both axes ran at their correction limits, the guider's integral term kept adding up the large error, so at the centre it was still pushing hard, carried the planet 146 px past and abandoned the lock ("corrections were making it worse"). The integral now holds still while an axis is at its limit, and works as before once near the centre, where it absorbs a tracking rate that has gone stale over a long lock. Gated by `guide_windup`, which replays that night's numbers through a model of the mount. Also: the status bar said `RAW8` whatever the bit depth; it now shows what the camera is set to and records (the files were always right). |
| 3.3.48 | 171 | **REPORT: tell Claude what went wrong, from the app.** A **REPORT** button on the left rail (above HELP), `Ctrl+Shift+R`, or the command palette opens a small window: describe the problem and press SEND, and the window closes at once. The app saves your words, what it was doing, its windows' screenshots, the log's last 4,000 lines and your `rig.ini`, with the site, the Dash token, keys, passwords and Leoben's address removed, and hands it to Claude Code on this PC (`tools\sky_handoff.py`, its own `report/<time>` branch, screenshots never committed, a draft PR, never merged). A notice says whether it went. The rail buttons are 44 px tall (were 50) to fit it. Also written: the plans for phase 169 (manual focus, one behaviour from every control, including absolute moves dropped while the focuser moves) and 170 (auto-exposure settles on its own path). Gated by `problem_report`, `toolbar_layout`, `command_palette` and the hand-off's self-test. |
| 3.3.47 | 172 (fix) | **PLANET LOCK no longer overshoots and gives up after a long centring.** On 2026-11-22 a lock started with Saturn 480 px off centre on the full frame: while both axes ran at their correction limits, the guider's integral term kept adding up the large error, so at the centre it was still pushing hard, carried the planet 146 px past and abandoned the lock ("corrections were making it worse"). The integral now holds still while an axis is at its limit, and works as before once near the centre, where it absorbs a tracking rate that has gone stale over a long lock. Gated by `guide_windup`, which replays that night's numbers through a model of the mount. Also: the status bar said `RAW8` whatever the bit depth; it now shows what the camera is set to and records (the files were always right). |
| 3.3.46 | 167.5 | **Autofocus no longer keeps a fit when the target faded during the sweep.** At the scope on 2026-11-22 Saturn faded about fourfold in haze halfway through a sweep, and autofocus moved 2,700 steps away from a start that was already in focus: comparing start and result afterwards, both read the same faded score. Now, if the start re-measures at under half what it read before the sweep, the start stands, and the log says `The target faded`. A sweep kept only because the sky changed no longer sets the focus readout's reference, which had made a focused planet read OUT. Each sweep also logs how far its outward and return passes disagreed (`[AF] Sweep check`), for tuning; it does not act on it yet. The status bar and Dash now say **APP (lock)** while PLANET LOCK drives the mount. Gated by `af_sweep_judge`. |
| 3.3.47 | 172 (fix) | **PLANET LOCK no longer overshoots and gives up after a long centring.** On 2026-11-22 a lock started with Saturn 480 px off centre on the full frame: while both axes ran at their correction limits, the guider's integral term kept adding up the large error, so at the centre it was still pushing hard, carried the planet 146 px past and abandoned the lock ("corrections were making it worse"). The integral now holds still while an axis is at its limit, and works as before once near the centre, where it absorbs a tracking rate that has gone stale over a long lock. Gated by `guide_windup`, which replays that night's numbers through a model of the mount. Also: the status bar said `RAW8` whatever the bit depth; it now shows what the camera is set to and records (the files were always right). |
| 3.3.48 | 167.5 | **Autofocus no longer keeps a fit made from a fade.** At the scope on 2026-11-22 Saturn faded about fourfold in haze halfway through a sweep, and autofocus moved 2,700 steps away from a start that was already in focus. Now, when the start re-measures at under half what it read before the sweep **and** the sweep's outward and return passes disagreed (they did by 3 times that night), the start stands and the log says `the curve was the fade`. A steady fade through a sweep that stays consistent still keeps its result, which is right. A sweep kept only because the sky changed no longer sets the focus readout's reference, which had made a focused planet read OUT. The status bar and Dash now say **APP (lock)** while PLANET LOCK drives the mount. Gated by `af_sweep_judge`. |

---

*RadukCapture Help v3.1 — the Phase History gains a row with every pull request*
