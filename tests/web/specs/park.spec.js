// Phase 156.2a: Dash's PARK. Park is a slew to rig.ini SkyParkAltAz, then
// tracking off (app/ParkController.h). Dash shows the control only for an
// app that reports park, disables it when no position is set, takes two taps
// to send, and shows what the app says the park is doing. Needs no
// RadukCapture running: the page is fed the app's telemetry and its sender is
// recorded.
const { test, expect } = require('@playwright/test');
const { dashUrl, blockInternet } = require('./dash');

const base = { rc_running: true, mount_tracking: 'ALT_AZ', park_state: 'idle', park_reason: '', park_set: true };

test.describe('Dash: park', () => {
  test.beforeEach(async ({ page }) => {
    await blockInternet(page);
    await page.goto(dashUrl(''));
    await expect(page.locator('#rcHeaderStatus')).toHaveText('DISCONNECTED');
    await page.locator('#tab-fc').click();   // the RADUKCAPTURE tab, where MOUNT JOG is
    await page.evaluate(() => {
      window.__sent = [];
      window.rcSendCmd = (cmd, arg) => window.__sent.push([cmd, arg]);
    });
  });

  test('no control for an app that does not report park', async ({ page }) => {
    const errors = [];
    page.on('pageerror', e => errors.push(String(e)));
    const older = { ...base };
    delete older.park_state; delete older.park_set; delete older.park_reason;
    await page.evaluate(d => rcHandleData(d), older);
    await expect(page.locator('#rcParkRow')).toBeHidden();
    expect(errors, 'script errors').toEqual([]);
  });

  test('no position set: disabled, and says which key', async ({ page }) => {
    await page.evaluate(d => rcHandleData(d), { ...base, park_set: false });
    await expect(page.locator('#rcParkRow')).toBeVisible();
    await expect(page.locator('#rcParkBtn')).toBeDisabled();
    await expect(page.locator('#rcParkState')).toContainText('SkyParkAltAz');
  });

  test('two taps send park; one tap sends nothing', async ({ page }) => {
    const errors = [];
    page.on('pageerror', e => errors.push(String(e)));
    await page.evaluate(d => rcHandleData(d), base);
    const btn = page.locator('#rcParkBtn');
    await expect(btn).toBeEnabled();
    await btn.click();
    await expect(btn).toHaveText('TAP AGAIN TO PARK');
    expect(await page.evaluate(() => window.__sent)).toEqual([]);
    await btn.click();
    await expect(btn).toHaveText('PARK');
    expect(await page.evaluate(() => window.__sent)).toEqual([['park', '']]);
    expect(errors, 'script errors').toEqual([]);
  });

  test('one tap disarms itself after 4 s', async ({ page }) => {
    await page.evaluate(d => rcHandleData(d), base);
    const btn = page.locator('#rcParkBtn');
    await btn.click();
    await expect(btn).toHaveText('TAP AGAIN TO PARK');
    await expect(btn).toHaveText('PARK', { timeout: 6000 });
    await btn.click();   // arms again, does not send
    expect(await page.evaluate(() => window.__sent)).toEqual([]);
  });

  test('shows slewing, parked and a failure with its reason', async ({ page }) => {
    const st = page.locator('#rcParkState');
    await page.evaluate(d => rcHandleData(d), { ...base, park_state: 'slewing' });
    await expect(st).toContainText('slewing');
    await expect(page.locator('#rcParkBtn')).toBeDisabled();
    await page.evaluate(d => rcHandleData(d), { ...base, park_state: 'parked', mount_tracking: 'OFF' });
    await expect(st).toHaveText('PARKED, tracking off');
    await expect(page.locator('#rcParkBtn')).toBeEnabled();
    await page.evaluate(d => rcHandleData(d), { ...base, park_state: 'failed',
      park_reason: 'the park slew did not finish in 200 s, so it was cancelled' });
    await expect(st).toContainText('Park failed: the park slew did not finish in 200 s');
  });
});
