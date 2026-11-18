// Phase 157.0c: an auto-acquire started from Dash widens the frame, Dash's
// ROI cell says WIDE while it is, and RESET puts the profile's frame back.
// Needs the emulator running (launch_emulator.bat) on a frame smaller than
// the sensor, and rig.ini AcquireWide on (the default). Tagged @live.
const { test, expect } = require('@playwright/test');
const { dashUrl, wsToken, blockInternet } = require('./dash');

test.describe('@live Dash: acquisition on a wide frame', () => {
  test.skip(!wsToken(), 'no WsToken: set RADUK_WS_TOKEN or RADUK_RIG_INI');

  test('CENTROID widens the frame, RESET restores it', async ({ page }) => {
    await blockInternet(page);
    await page.goto(dashUrl(wsToken()));
    await expect(page.locator('#rcHeaderStatus')).not.toHaveText('DISCONNECTED', { timeout: 20_000 });

    const roi = page.locator('#rcROI');   // the remote panel's ROI row

    // A stage left over from an earlier test would make CENTROID do nothing.
    await page.locator('button.rc-ctrl-btn', { hasText: /^RESET$/ }).click();
    // The emulator starts on the whole sensor, where there is nothing to
    // widen: the Star profile's 640x480 first. Dash's profile picker does
    // not reach the app (it relabels Dash only), so this goes through
    // Dash's own command channel.
    await page.evaluate(() => rcSendCmd('load_profile', 'Star'));
    await expect(roi).toHaveText('640×480', { timeout: 20_000 });
    const before = '640×480';

    await page.locator('button.rc-ctrl-btn', { hasText: /^CENTROID$/ }).click();
    await expect(roi).toContainText('WIDE', { timeout: 15_000 });
    const wide = (await roi.textContent()).trim();
    expect(wide.startsWith(before + ' '), `wide frame ${wide} differs from ${before}`).toBe(false);

    await page.locator('button.rc-ctrl-btn', { hasText: /^RESET$/ }).click();
    await expect(roi).toHaveText(before, { timeout: 15_000 });
  });
});
