// Phase 157.0e: Dash's focuser-trust card. When the focuser comes back with a
// number that disagrees with the saved one, the card shows both and sends the
// owner's answer (focuser_trust restore|accept). Needs no RadukCapture running:
// the page is fed the app's telemetry and its sender is recorded, because a
// focuser in doubt cannot be produced on the live app without unplugging one
// (fault_injection eaf-drop covers that end).
const { test, expect } = require('@playwright/test');
const { dashUrl, blockInternet } = require('./dash');

const doubt = {
  rc_running: true,
  eaf_position: 4761,
  eaf_temp: 18.3,
  eaf_moving: false,
  eaf_trusted: false,
  eaf_saved_position: 166700,
  eaf_trust_problem: 'the focuser reports 4761 but was last at 166700: accept its number or put 166700 back before any automatic move',
};

test.describe('Dash: the focuser-trust card', () => {
  test.beforeEach(async ({ page }) => {
    await blockInternet(page);
    await page.goto(dashUrl(''));
    await expect(page.locator('#rcHeaderStatus')).toHaveText('DISCONNECTED');
    await page.locator('#tab-fc').click();   // the RADUKCAPTURE tab, where the EAF panel is
    // Record what the buttons would send, as the app would receive it.
    await page.evaluate(() => {
      window.__sent = [];
      window.rcSendCmd = (cmd, arg) => window.__sent.push([cmd, arg]);
    });
  });

  test('hidden while the position is trusted, and for an app that does not say', async ({ page }) => {
    const errors = [];
    page.on('pageerror', e => errors.push(String(e)));
    const card = page.locator('#rcEAFTrust');
    await expect(card).toBeHidden();
    await page.evaluate(d => rcHandleData(d), { ...doubt, eaf_trusted: true, eaf_trust_problem: '' });
    await expect(card).toBeHidden();
    const older = { ...doubt };
    delete older.eaf_trusted;
    await page.evaluate(d => rcHandleData(d), older);
    await expect(card).toBeHidden();
    expect(errors, 'script errors').toEqual([]);
  });

  test('in doubt: both numbers, and each answer sends its command', async ({ page }) => {
    const errors = [];
    page.on('pageerror', e => errors.push(String(e)));
    await page.evaluate(d => rcHandleData(d), doubt);
    const card = page.locator('#rcEAFTrust');
    await expect(card).toBeVisible();
    await expect(page.locator('#rcEAFTrustSaved')).toHaveText((166700).toLocaleString('en-US'));
    await expect(page.locator('#rcEAFTrustNow')).toHaveText((4761).toLocaleString('en-US'));
    await expect(page.locator('#rcEAFTrustText')).toContainText('was last at 166700');
    const restore = page.locator('#rcEAFTrustRestore');
    const accept = page.locator('#rcEAFTrustAccept');
    await expect(restore).toContainText('PUT BACK');
    await expect(restore).toContainText(/166.?700/);
    await expect(accept).toContainText(/KEEP 4.?761/);

    await restore.click();
    await accept.click();
    expect(await page.evaluate(() => window.__sent)).toEqual([
      ['focuser_trust', 'restore'],
      ['focuser_trust', 'accept'],
    ]);

    // Answered: the next telemetry says trusted, and the card goes.
    await page.evaluate(d => rcHandleData(d), { ...doubt, eaf_trusted: true, eaf_trust_problem: '' });
    await expect(card).toBeHidden();
    // And it goes when RadukCapture stops running, too.
    await page.evaluate(d => rcHandleData(d), doubt);
    await expect(card).toBeVisible();
    await page.evaluate(d => rcHandleData(d), { ...doubt, rc_running: false });
    await expect(card).toBeHidden();
    expect(errors, 'script errors').toEqual([]);
  });
});
