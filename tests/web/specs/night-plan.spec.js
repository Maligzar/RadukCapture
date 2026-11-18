// Phase 163: the night plan for planets and stars, from Dash. A target
// picked from the list is ticked and shows as a chip, PLAN lists it with
// its window and minutes, RUN starts the plan and a second press stops it,
// and the ticks the rig had before are put back.
// Needs the app running (launch_emulator.bat). Tagged @live. A whole run,
// slew, lock and RUNS on two stars, is tests\scenarios\night-plan-stars.scn's.
const { test, expect } = require('@playwright/test');
const { dashUrl, wsToken, blockInternet } = require('./dash');

test.describe('@live Dash: the night plan', () => {
  test.skip(!wsToken(), 'no WsToken: set RADUK_WS_TOKEN or RADUK_RIG_INI');

  test('tick, plan, run and stop', async ({ page }) => {
    await blockInternet(page);   // refused, not asserted: the forecast fetch is owed as 151.4
    await page.goto(dashUrl(wsToken()));
    await expect(page.locator('#rcHeaderStatus')).not.toHaveText('DISCONNECTED', { timeout: 20_000 });

    const state = page.locator('#rcNightState');
    await expect(state).not.toHaveText('--', { timeout: 20_000 });   // telemetry has arrived
    await expect(state).not.toHaveText(/^RUNNING/);

    // What was ticked before, to put back at the end.
    const before = await page.locator('#rcNightTicked button').evaluateAll(
      bs => bs.map(b => b.dataset.name));

    await page.locator('#rcNightClear').click();
    await expect(page.locator('#rcNightTicked button')).toHaveCount(0, { timeout: 10_000 });

    // Something reaches 45 deg before dawn from any site, any night.
    const pick = page.locator('#rcNightPick');
    await expect(pick.locator('option').first()).not.toHaveAttribute('value', '', { timeout: 20_000 });
    const name = await pick.locator('option').first().getAttribute('value');
    await pick.selectOption(name);
    await page.locator('#rcNightTick').click();
    const chip = page.locator('#rcNightTicked button', { hasText: name });
    await expect(chip).toHaveCount(1, { timeout: 10_000 });

    // PLAN: one row for it, planned or with the reason it is not.
    await page.locator('#rcNightPlan').click();
    await expect(state).toHaveText(/^PLANNED: \d+ VISITS/, { timeout: 15_000 });
    const row = page.locator(`#rcNightRows div[data-name="${name}"]`);
    await expect(row).toHaveCount(1);
    const rowState = await row.getAttribute('data-state');
    expect(['planned', 'not planned']).toContain(rowState);
    if (rowState === 'planned') {
      await expect(row).toContainText(/(PRIME|GOOD) \d+ min/);

      // RUN, then the same button stops it.
      await page.locator('#rcNightRun').click();
      await expect(state).toHaveText(/^RUNNING 1\/\d+/, { timeout: 10_000 });
      await expect(page.locator('#rcNightRun')).toHaveText(/STOP/);

      // Phase 163b: PAUSE holds the plan where it is; CONTINUE plans the
      // rest again from now and runs on.
      const pause = page.locator('#rcNightPause');
      await expect(pause).toHaveText('PAUSE');
      await pause.click();
      await expect(state).toHaveText(/^PAUSED (ON|BEFORE) /, { timeout: 10_000 });
      await expect(pause).toHaveText('CONTINUE');
      await expect(page.locator('#rcNightSection')).toHaveAttribute('data-paused', '1');
      await pause.click();
      await expect(state).toHaveText(/^(RUNNING|DONE)/, { timeout: 15_000 });
      await expect(pause).toHaveText('PAUSE');
      if (/^RUNNING/.test(await state.textContent())) {
        await page.locator('#rcNightRun').click();
      }
      await expect(state).toHaveText(/^(STOPPED|DONE)/, { timeout: 15_000 });
      await expect(page.locator('#rcNightRun')).toHaveText(/RUN/);
    }

    // A tick of a name the app does not know is refused, and nothing changes.
    await page.evaluate(() => rcSendCmd('nightplan', 'TICK No Such Star'));
    await page.waitForTimeout(1500);
    await expect(page.locator('#rcNightTicked button')).toHaveCount(1);

    // Leave the rig as it was.
    await page.locator('#rcNightClear').click();
    await expect(page.locator('#rcNightTicked button')).toHaveCount(0, { timeout: 10_000 });
    for (const n of before) await page.evaluate(t => rcSendCmd('nightplan', 'TICK ' + t), n);
    await expect(page.locator('#rcNightTicked button')).toHaveCount(before.length, { timeout: 10_000 });
  });
});
