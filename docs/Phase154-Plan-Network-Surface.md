# Phase 154 plan: the hand-written network code

From the owner's weakness review, 2026-11-13. Status: `PLANNED`.

## The problem
- The dashboard and telemetry run on a hand-written RFC 6455 WebSocket server (`src/net/WebSocketServer.cpp`), with a hand-written frame parser (`WsProtocol.h`) and hand-written JSON.
- The reason was "no vcpkg". The cost is attack surface and maintenance: this is the only network-facing C++ in the project, and it accepts commands that move a telescope.
- **What mitigates it today** (P5.6, 2026-10-25):
  - loopback only by default (`WsLan` opens it to the network);
  - a token on every connection;
  - the frame parser fuzzed (`tests/ws_fuzz.cpp`, AddressSanitizer);
  - a 500 ms send timeout on every socket.
- **What is not covered:** the HTTP upgrade parsing and the JSON command parsing get only partial fuzzing, and the fuzzer is a random-input loop, not coverage-guided.

## Stages
- **154.0 Measure the surface.**
  - List every byte path from a socket to a decision: the upgrade request, the frame header, masking, fragmentation and continuation, close and ping, the JSON command, the command dispatch.
  - For each, record whether a test or fuzzer reaches it.
- **154.1 Coverage-guided fuzzing.**
  - Build `WsProtocol.h`, the upgrade parser and the JSON command parser as libFuzzer targets with clang on Linux: the parsers are pure, so they build without Windows.
  - Run each for a fixed time in CI, as a separate job so the `windows` gate is not slowed.
  - Keep a corpus in the repo.
  - Every crash becomes a regression test.
- **154.2 Decide what to replace, with the fuzzing in hand.** Options, with no dependency manager in either case:
  - **The JSON:** vendor a small, well-fuzzed header-only parser into `third_party/`. A single header with a compatible licence, pinned to a version and checked in, works offline (rule 7). Or keep ours if 154.1 finds nothing.
  - **The WebSocket server:** Windows' own HTTP server (`http.sys`) with its WebSocket Protocol Component API takes the framing out of our code. That this API pair does the whole handshake and framing is recalled, not confirmed; 154.2 confirms it with a prototype before deciding. The cost is a rewrite of the server's I/O and a dependency on Windows behaviour that cannot be fuzzed.
  - **Keep both, hardened**, if 154.1 runs clean for a long stretch and the prototype is not clearly simpler.

  The decision and its evidence go into this file. The owner's direction (the dashboard on the home network, airplane mode) is a constraint either way.
- **154.0a The hardware-command token, pulled forward to release 3.4** (added 2026-11-17 from the ChatGPT review; moved into 3.4 by the owner the same day; written as 3.3.26, see "154.0a, as built" below). 154.3 below plans this. It is pulled out because of what the code and the rig show today:
  - The rig's `build/Release/rig.ini` has `WsLan=1` and `WsLanOpen=1`.
  - `WebSocketServer::HandleHandshake` admits any private-address peer without a token, and nothing checks the `Origin` header.
  - A tokenless client gets every command, including slews and the focuser.
  - So **any web page open in a browser on any device on the home network can try `ws://<imaging-pc>:8765`** and drive the telescope (cross-site WebSocket hijacking). It needs the PC's address, which a page can scan for, and the port, which is fixed.
  - Chrome reportedly prompts before a public page opens a WebSocket to a private address from about Chrome 147 (April 2026) ([Chrome's Local Network Access guide](https://developer.chrome.com/docs/modern-web-guidance/guides/security/local-network-access), [blink-dev](https://groups.google.com/a/chromium.org/g/blink-dev/c/O6GMKt44Ups)). That is one browser, behind one click, and the version is from secondary sources.

  The change, small and on its own:
  - Without the token, a `WsLanOpen` client gets telemetry and the preview. Every command that moves or changes hardware (mount, focuser, camera settings, recording) is refused with a reason Dash shows.
  - Dash asks for the token once and keeps it in that browser, as it already can (`rcUrlWithToken`).
  - A handshake whose `Origin` is a web origin other than Dash's own is refused even with `WsLanOpen`.
  - A log line goes out for every hardware command, with its source address (154.3's fourth item, brought along).

  Gated by a `ws_protocol` case and a `tests/web` spec: a tokenless LAN client is refused a slew and allowed telemetry. STOP stays allowed without the token: stopping the telescope is never the dangerous direction.
- **154.3 Defence in depth, whatever 154.2 decides:**
  - a size cap on every message;
  - a rate limit on mount commands;
  - commands that move hardware refused unless the connection authenticated with the token, even on `WsLanOpen` (today `WsLanOpen` exempts private addresses);
  - a log line for every command that moves hardware, with its source address.

## 154.0a, as built (2026-11-17, 3.3.26)
- **`src/net/WsProtocol.h`**, pure:
  - `CommandAllowedWithoutToken` lists what a tokenless client may send: `mount_stop`, `stop_capture`, `autofocus_abort`, `moonmap` with `STOP` alone, and `nightplan` with `STOP` alone (not its PAUSE: one button that also continues a paused plan). Everything else is refused, including a command nobody has listed yet.
  - `OriginAllowedWithoutToken` allows no Origin, `"null"` (Dash as a file), and pages served from this network (localhost, loopback or private IPv4, a name with no dot, `.local`, `.lan`, `.home.arpa`). It refuses every other http(s) origin, every other scheme, and IPv6 literals.
  - The handshake now reads `Origin`.
  - `RefusalMessage` echoes only command-name characters.
- **`WebSocketServer`**:
  - Each client records whether it presented the token.
  - A tokenless handshake from a refused origin gets 403.
  - A tokenless client's message is parsed with `APEXCommand::Parse`, the parser the app executes with, so the gate and the executor cannot disagree. Anything outside the list is answered with the refusal, logged with the peer's address, and never reaches the command queue.
  - A client with the token is unchanged.
- **Dash** shows the refusal ("needs the access token … without it Dash can watch and STOP"). Before, it would have fed the message to the telemetry handler and shown the app as not running.
- **Tests:**
  - `ws_protocol` covers the rules: every command the app dispatches, look-alike origins, the echo.
  - `ws_server_loopback` section 6 dials the server on the machine's own private IPv4, so the server sees a real LAN peer. An internet origin gets 403. Dash as a file is admitted, its jog is refused and never reaches the app, and its STOP does. With the token, a jog goes through as before. It passed on the owner's PC over 192.168.1.35, and is skipped where there is no private IPv4.
  - No Playwright spec. Playwright's browser connects over loopback, which needs the token anyway, so it cannot be the tokenless LAN client.
- **Not changed:** the `WsLanOpen` decision itself, the token for loopback, and 154.3's size caps and rate limits, which stay with 154.3.

## Versioning
Every PR bumps PATCH; the PR that completes the phase bumps MINOR ([`ROADMAP-Next.md`](ROADMAP-Next.md#next-in-order)).
