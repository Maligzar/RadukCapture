# Phase 169: manual focus, one behaviour from every control (QA and rebuild)

Raised by the owner at the scope, 2026-11-21: "The focuser manual commands need a QA; they do not seem to behave the same way. Maybe the features have passed the functionality and we need to rebuild the manual focus." Tech debt, after the open branches merge. Status: `PLANNED`.

## Why they differ (found by reading the code, 2026-11-22)
Each manual control reaches the focuser by its own path, and the paths have collected different rules over phases 85, 139, 157 and 161:

| Control | Path | What it does differently |
|---|---|---|
| Keyboard fine/coarse and the panel's IN/OUT | `main.cpp` `profileStep` → `MoveRelative(±stepFine / stepCoarse)` | step size from the active profile (10/100 by default, set per profile); relative; adds to a move in flight |
| The focus panel's position box | `ControlPanel.cpp` → `MoveTo(pos)`, then `MoveToFar` after a second press | absolute; ends travelling outward with the backlash (139.2); the travel guard can refuse it (161.0a), and a second press overrides |
| Dash `move_eaf` | `main.cpp` → `MoveTo(pos)` | absolute, outward finish; never overrides the guard (a remote cannot be asked twice) |
| Dash `move_eaf_rel` | `main.cpp` → `MoveRelative(steps)` | relative, with Dash's own step sizes, not the profile's |
| A profile load | `MoveTo(profile focusPosition)` | absolute, outward finish, on every GoTo that loads a profile |
| Autofocus and calibration | `MoveToDirect` and their own sequences | one leg, both directions on purpose |

So a relative step and an absolute move to the same place can end at different mirror positions (the outward finish adds the backlash). The keyboard and Dash step by different amounts. One path can override the guard and another can't. And a profile load can move the focuser when the owner did not ask it to.

## Stages
1. **169.0, the QA.** Each manual control is driven on the emulator focuser, which has slack and a guard, and its logged moves are recorded. Measure the end position, the direction of the final approach, the step size, and the refusal behaviour. Do the same on the real EAF indoors. The result is a table of what each actually does, against what the Help says.
2. **169.1, one rule.** One `ManualFocus` controller (`src/app/`) that every manual control calls:
   - one step size per profile (fine and coarse), shared by the keyboard, the panel and Dash;
   - one approach rule for every move that ends somewhere (outward finish with the backlash), relative steps included;
   - one guard rule: the app's controls may override after asking twice, Dash never;
   - one log line per move.
   - The app's controls come first, Dash mirrors them (the owner's rule, phase 166).
3. **169.2, profile loads.** A profile load moves the focuser only if the owner has said so (a setting). It never moves it mid-recording or mid-sweep, and it says so when it does move it.
4. **Tests:**
   - a pure test of the controller's rules;
   - a scenario pressing each control in the emulator;
   - a Dash spec mirroring them;
   - the parity check (166.0) keeps Dash's focuser commands tied to app controls.
