// Shared helpers: where Dash is, the token, and opening it as [DASH] does.
const fs = require('fs');
const path = require('path');

const ROOT = path.resolve(__dirname, '..', '..', '..');
const DASH = process.env.RADUK_DASH || path.join(ROOT, 'RadukCaptureDashboard.html');

// WsToken: RADUK_WS_TOKEN, else the emulator build's rig.ini, else none.
function wsToken() {
  if (process.env.RADUK_WS_TOKEN) return process.env.RADUK_WS_TOKEN;
  const ini = process.env.RADUK_RIG_INI || path.join(ROOT, 'build_emu', 'Release', 'rig.ini');
  try {
    const m = fs.readFileSync(ini, 'utf8').match(/^\s*WsToken\s*=\s*([0-9a-fA-F]+)\s*$/m);
    return m ? m[1] : '';
  } catch { return ''; }
}

// file:///.../RadukCaptureDashboard.html#token=<hex>, as DashboardLaunch.h builds it.
function dashUrl(token) {
  const u = 'file:///' + DASH.replace(/\\/g, '/').replace(/^\/+/, '');
  return token ? `${u}#token=${token}` : u;
}

// Rule 7: no request may leave this PC. Records any that try, and refuses them.
async function blockInternet(page) {
  const tried = [];
  await page.route('**/*', route => {
    const url = route.request().url();
    const local = url.startsWith('file:') || /^(wss?|https?):\/\/(localhost|127\.0\.0\.1|\[::1\])(:|\/)/.test(url);
    if (local) return route.continue();
    tried.push(url);
    return route.abort();
  });
  return tried;
}

module.exports = { DASH, wsToken, dashUrl, blockInternet };
