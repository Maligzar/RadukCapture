// Phase 162: the Moon map from Dash. A new project shows its grid, a click
// picks a section, MARK turns it green and counts it, CLEAR takes the green
// away, and the empty project is discarded so the rig is left as found.
// Needs the app running (launch_emulator.bat). Tagged @live. The run itself
// (START) moves the mount for minutes and is tests\scenarios\moon-map.scn's.
const { test, expect } = require('@playwright/test');
const { dashUrl, wsToken, blockInternet } = require('./dash');

test.describe('@live Dash: the Moon map', () => {
  test.skip(!wsToken(), 'no WsToken: set RADUK_WS_TOKEN or RADUK_RIG_INI');

  test('a new project, a section marked and cleared', async ({ page }) => {
    await blockInternet(page);
    await page.goto(dashUrl(wsToken()));
    await expect(page.locator('#rcHeaderStatus')).not.toHaveText('DISCONNECTED', { timeout: 20_000 });

    const count = page.locator('#rcMoonCount');
    const name  = page.locator('#rcMoonName');
    await expect(name).not.toHaveText('--', { timeout: 20_000 });   // telemetry has arrived
    const before = (await name.textContent()).trim();

    // Phase 162c: when the active map holds captures, NEW PROJECT asks first.
    page.on('dialog', d => d.accept());
    await page.locator('#rcMoonNew').click();
    await expect(count).toHaveText(/^0 \/ \d+$/, { timeout: 15_000 });
    await expect(name).not.toHaveText(before);
    const total = Number((await count.textContent()).split('/')[1]);
    expect(total).toBeGreaterThan(3);

    // One square a section, and none of them captured.
    const cells = page.locator('#rcMoonGrid div[data-state]');
    await expect(cells).toHaveCount(total);
    await expect(page.locator('#rcMoonGrid div[data-state="C"]')).toHaveCount(0);
    await expect(page.locator('#rcMoonState')).not.toHaveText('READY FOR MOSAIC');

    // Pick the section nearest the middle of the disc on the north-east side.
    const pick = page.locator('#rcMoon_NE-A1');
    await pick.click();
    await expect(page.locator('#rcMoonPicked')).toHaveText('NE-A1', { timeout: 10_000 });

    await page.locator('#rcMoonMark').click();
    await expect(pick).toHaveAttribute('data-state', 'C', { timeout: 10_000 });
    await expect(count).toHaveText(`1 / ${total}`);

    await page.locator('#rcMoonClear').click();
    await expect(pick).not.toHaveAttribute('data-state', 'C', { timeout: 10_000 });
    await expect(count).toHaveText(`0 / ${total}`);

    // Leave the rig as it was: the empty project goes, the one before returns.
    await page.evaluate(() => rcSendCmd('moonmap', 'DISCARD'));
    await expect(name).toHaveText(before, { timeout: 10_000 });
  });
});
