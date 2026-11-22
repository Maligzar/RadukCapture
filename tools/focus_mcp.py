#!/usr/bin/env python3
"""focus_mcp.py - Phase 174.3: the focus history as a local MCP server.

    python tools/focus_mcp.py --history <logs>\\focus_history.jsonl

A Model Context Protocol server on stdin/stdout (JSON-RPC 2.0, one message
per line). It uses the standard library only and needs nothing online (rule 7).
It lets a model (Claude Code, or a small local model through an MCP tool
loop) read the focus history and write down proposals. It cannot move the
focuser or change a setting: the 174 principle is that the model proposes,
replay judges and the owner decides.

Tools, all read-only except the last:
  list_sweeps(night?, target?)          one line per sweep
  get_sweep(id)                         the sweep line and its followup, whole
  failure_summary()                     each failed or hand-corrected sweep, classified
  temperature_model(target?, filter?)   settled focus against focuser temperature
  replay()                              today's rules over the history (runs focus_replay.exe)
  propose(change, why)                  appends to focus_proposals.md beside the history

Claude Code, in .mcp.json or `claude mcp add`:
    claude mcp add focus-history -- python tools/focus_mcp.py --history D:\\RadukCapture\\logs\\focus_history.jsonl

    python tools/focus_mcp.py --self-test
"""
import datetime
import json
import os
import shutil
import subprocess
import sys

PROTOCOL = "2025-06-18"
SERVER = {"name": "raduk-focus-history", "version": "1.0"}

# The same thresholds as the app: core/FocusDirection.h (PassHasPeak) and
# core/FocusTempModel.h (Minimums). Kept in step by the self-test's cases.
PEAK_MIN_RISE = 1.15
MIN_POINTS, MIN_NIGHTS, MIN_SPAN_C = 6, 2, 2.0
CORRECTED_STEPS = 200      # a followup moving the result this far is a hand correction


# ── The history ─────────────────────────────────────────────────────────

def read_history(path):
    sweeps, followups, bad = [], {}, 0
    if not os.path.exists(path):
        return sweeps, followups, bad
    with open(path, encoding="utf-8", errors="replace") as f:
        for line in f:
            if not line.strip():
                continue
            try:
                j = json.loads(line)
            except ValueError:
                bad += 1
                continue
            if j.get("kind") == "sweep":
                sweeps.append(j)
            elif j.get("kind") == "followup":
                followups[j.get("sweep")] = j
    return sweeps, followups, bad


def night_of(unix):
    """The evening a sweep belongs to: 02:00 is the same night as 22:00 (as FocusTempModel.h)."""
    if not unix:
        return None
    d = datetime.datetime.fromtimestamp(unix - 12 * 3600, datetime.timezone.utc)
    return d.strftime("%Y%m%d")


def sweep_night(s):
    n = night_of(s.get("unix"))
    if n:
        return n
    sid = s.get("id", "")
    return sid.split("-")[1] if sid.startswith("log-") and sid.count("-") >= 2 else None


def pass_has_peak(sc):
    """As core/FocusDirection.h PassHasPeak: the best is inside the sweep and rises 1.15x over both ends."""
    if len(sc) < 3:
        return False
    i = max(range(len(sc)), key=lambda k: sc[k])
    if i == 0 or i == len(sc) - 1:
        return False
    return sc[i] >= PEAK_MIN_RISE * sc[0] and sc[i] >= PEAK_MIN_RISE * sc[-1]


def classify(s, fu):
    """Why a sweep went wrong, from what was recorded only. None if it did not."""
    c = s.get("coarse") or {}
    out, back = c.get("out") or [], c.get("back") or []
    ab = s.get("ab") or {}
    moved = abs(fu["settled"] - s.get("result", 0)) if fu else 0
    if s.get("success") and moved < CORRECTED_STEPS:
        return None
    reasons = []
    if s.get("sky_changed") or (ab.get("verdict") or "").startswith("start stands"):
        reasons.append("fade: the target dimmed during the sweep (cloud or haze)")
    peaks = [p for p in (out, back) if any(v for v in p)]
    if peaks and not any(pass_has_peak(p) for p in peaks):
        if any(p and max(range(len(p)), key=lambda k: p[k]) in (0, len(p) - 1) for p in peaks):
            reasons.append("edge: the best score was at an end of the sweep (focus not bracketed)")
        else:
            reasons.append("plateau: no focus peak in either pass")
    bm, bu = s.get("backlash_measured", -1), s.get("backlash_in_use", -1)
    if bm is not None and bu is not None and bm > 0 and bu > 0 and abs(bm - bu) > max(50, 0.5 * bu):
        reasons.append(f"backlash: measured {bm} steps against {bu} in use")
    if not reasons:
        reasons.append("unclassified: the record does not say why")
    return reasons


# ── The temperature model (as core/FocusTempModel.h) ────────────────────

def median(v):
    v = sorted(v)
    n = len(v)
    if not n:
        return None
    return v[n // 2] if n % 2 else 0.5 * (v[n // 2 - 1] + v[n // 2])


def temperature_points(sweeps, followups):
    pts = []
    for s in sweeps:
        fu = followups.get(s.get("id"))
        at = s.get("at") or {}
        t = at.get("focuser_temp_c")
        if fu is None or not isinstance(t, (int, float)) or not -60 <= t <= 80:
            continue
        pts.append({"key": f"{at.get('target') or ''}|{at.get('filter') or ''}", "temp": float(t),
                    "settled": fu["settled"], "night": sweep_night(s)})
    return pts


def fit(pts, key):
    mine = [p for p in pts if p["key"] == key]
    r = {"key": key, "points": len(mine), "nights": len({p["night"] for p in mine}), "ok": False, "confident": False}
    if mine:
        r["temp_range_c"] = [min(p["temp"] for p in mine), max(p["temp"] for p in mine)]
    slopes = [(b["settled"] - a["settled"]) / (b["temp"] - a["temp"])
              for i, a in enumerate(mine) for b in mine[i + 1:] if abs(b["temp"] - a["temp"]) >= 0.5]
    if slopes:
        m = median(slopes)
        c = median([p["settled"] - m * p["temp"] for p in mine])
        r.update(ok=True, steps_per_c=round(m, 2), intercept=round(c, 1),
                 scatter_steps=round(1.4826 * median([abs(p["settled"] - (c + m * p["temp"])) for p in mine]), 1))
    span = r["temp_range_c"][1] - r["temp_range_c"][0] if mine else 0.0
    if not r["ok"]:
        r["why_not"] = f"{len(mine)} point(s), no temperature spread to fit"
    elif r["points"] < MIN_POINTS:
        r["why_not"] = f"{r['points']} of {MIN_POINTS} points"
    elif r["nights"] < MIN_NIGHTS:
        r["why_not"] = f"{r['nights']} of {MIN_NIGHTS} nights"
    elif span < MIN_SPAN_C:
        r["why_not"] = f"{span:.1f} of {MIN_SPAN_C:.1f} C of temperature range"
    else:
        r["confident"] = True
    return r


# ── The tools ───────────────────────────────────────────────────────────

class Tools:
    def __init__(self, history, replay_exe=None):
        self.history = history
        self.proposals = os.path.join(os.path.dirname(os.path.abspath(history)), "focus_proposals.md")
        self.replay_exe = replay_exe

    def load(self):
        return read_history(self.history)

    def list_sweeps(self, night=None, target=None):
        sweeps, fus, bad = self.load()
        rows = []
        for s in sweeps:
            at = s.get("at") or {}
            if night and sweep_night(s) != str(night):
                continue
            if target and (at.get("target") or "").lower() != str(target).lower():
                continue
            fu = fus.get(s.get("id"))
            rows.append({"id": s.get("id"), "night": sweep_night(s), "target": at.get("target"),
                         "filter": at.get("filter"), "focuser_temp_c": at.get("focuser_temp_c"),
                         "success": s.get("success"), "start": s.get("start"), "result": s.get("result"),
                         "verdict": (s.get("ab") or {}).get("verdict"),
                         "settled": fu["settled"] if fu else None,
                         "moved_from_result": fu.get("moved_from_result") if fu else None})
        return {"history": self.history, "sweeps": rows, "unreadable_lines": bad}

    def get_sweep(self, id):
        sweeps, fus, _ = self.load()
        for s in sweeps:
            if s.get("id") == id:
                return {"sweep": s, "followup": fus.get(id)}
        raise ValueError(f"no sweep with id {id!r}")

    def failure_summary(self):
        sweeps, fus, _ = self.load()
        out, counts = [], {}
        for s in sweeps:
            fu = fus.get(s.get("id"))
            why = classify(s, fu)
            if why is None:
                continue
            for w in why:
                k = w.split(":")[0]
                counts[k] = counts.get(k, 0) + 1
            out.append({"id": s.get("id"), "target": (s.get("at") or {}).get("target"),
                        "success": s.get("success"), "result": s.get("result"),
                        "settled": fu["settled"] if fu else None, "reasons": why})
        return {"sweeps_read": len(sweeps), "went_wrong": len(out), "by_reason": counts, "sweeps": out,
                "note": "Classified from the recorded curves and verdicts only. Donut and seeing are not in the record yet."}

    def temperature_model(self, target=None, filter=None):
        sweeps, fus, _ = self.load()
        pts = temperature_points(sweeps, fus)
        keys = sorted({p["key"] for p in pts})
        if target is not None:
            keys = [k for k in keys if k.split("|")[0].lower() == str(target).lower()]
        if filter is not None:
            keys = [k for k in keys if k.split("|", 1)[1].lower() == str(filter).lower()]
        return {"points": len(pts), "minimums": {"points": MIN_POINTS, "nights": MIN_NIGHTS, "span_c": MIN_SPAN_C},
                "fits": [fit(pts, k) for k in keys]}

    def replay(self):
        exe = self.replay_exe or shutil.which("focus_replay") or shutil.which("focus_replay.exe")
        if not exe:
            here = os.path.dirname(os.path.abspath(__file__))
            for cand in ("../build/Release/focus_replay.exe", "../build/focus_replay", "../build/Release/focus_replay"):
                p = os.path.normpath(os.path.join(here, cand))
                if os.path.exists(p):
                    exe = p
                    break
        if not exe:
            raise ValueError("focus_replay is not built (build_all.bat builds it as build\\Release\\focus_replay.exe)")
        r = subprocess.run([exe, self.history], capture_output=True, text=True, timeout=120)
        return {"exit_code": r.returncode, "regression": r.returncode == 1, "report": r.stdout + r.stderr,
                "note": "Replays the rules compiled into this build. A rule change is a code change: rebuild, then replay."}

    def propose(self, change, why):
        change, why = str(change).strip(), str(why).strip()
        if not change or not why:
            raise ValueError("a proposal needs both a change and a reason")
        stamp = datetime.datetime.now().strftime("%Y-%m-%d %H:%M")
        new = not os.path.exists(self.proposals)
        with open(self.proposals, "a", encoding="utf-8") as f:
            if new:
                f.write("# Focus proposals\n\nWritten by a model reading the focus history. "
                        "Nothing here is applied until the owner accepts it and replay shows no regression.\n")
            f.write(f"\n## {stamp}: {change}\n\n{why}\n\nStatus: proposed\n")
        return {"written_to": self.proposals, "change": change}


TOOL_DEFS = [
    {"name": "list_sweeps", "description": "List autofocus sweeps from the focus history, one row each: night, target, filter, focuser temperature, start, result, the A/B verdict and where the owner settled by hand afterwards.",
     "inputSchema": {"type": "object", "properties": {"night": {"type": "string", "description": "YYYYMMDD of the evening"},
                                                         "target": {"type": "string"}}}},
    {"name": "get_sweep", "description": "One sweep's full record (the coarse curve out and back, the fine pass, the A/B, conditions) and its followup.",
     "inputSchema": {"type": "object", "properties": {"id": {"type": "string"}}, "required": ["id"]}},
    {"name": "failure_summary", "description": "Every sweep that failed or that the owner corrected by hand, classified from the record: fade, plateau, edge, backlash or unclassified.",
     "inputSchema": {"type": "object", "properties": {}}},
    {"name": "temperature_model", "description": "Settled focus against focuser temperature per target and filter (Theil-Sen), with the data it rests on and whether it is confident enough to use.",
     "inputSchema": {"type": "object", "properties": {"target": {"type": "string"}, "filter": {"type": "string"}}}},
    {"name": "replay", "description": "Run today's autofocus rules (the built focus_replay) over the whole history and report any sweep they would now end further from where the owner settled.",
     "inputSchema": {"type": "object", "properties": {}}},
    {"name": "propose", "description": "Write a proposed change to autofocus, with the reason and evidence, into focus_proposals.md for the owner. The only tool that writes; it never changes a setting or moves the focuser.",
     "inputSchema": {"type": "object", "properties": {"change": {"type": "string"}, "why": {"type": "string"}},
                     "required": ["change", "why"]}},
]


# ── The protocol ────────────────────────────────────────────────────────

def handle(tools, msg):
    """One JSON-RPC message in, the reply out (None for a notification)."""
    mid, method = msg.get("id"), msg.get("method")
    if mid is None:
        return None
    try:
        if method == "initialize":
            ver = (msg.get("params") or {}).get("protocolVersion") or PROTOCOL
            result = {"protocolVersion": ver, "capabilities": {"tools": {}}, "serverInfo": SERVER}
        elif method == "ping":
            result = {}
        elif method == "tools/list":
            result = {"tools": TOOL_DEFS}
        elif method == "tools/call":
            p = msg.get("params") or {}
            name, args = p.get("name"), p.get("arguments") or {}
            if name not in {t["name"] for t in TOOL_DEFS}:
                return {"jsonrpc": "2.0", "id": mid, "error": {"code": -32602, "message": f"unknown tool {name}"}}
            try:
                data = getattr(tools, name)(**args)
                result = {"content": [{"type": "text", "text": json.dumps(data, indent=1)}], "isError": False}
            except (ValueError, TypeError, OSError, subprocess.SubprocessError) as e:
                result = {"content": [{"type": "text", "text": str(e)}], "isError": True}
        else:
            return {"jsonrpc": "2.0", "id": mid, "error": {"code": -32601, "message": f"method not found: {method}"}}
    except Exception as e:  # never die on one bad message
        return {"jsonrpc": "2.0", "id": mid, "error": {"code": -32603, "message": str(e)}}
    return {"jsonrpc": "2.0", "id": mid, "result": result}


def serve(tools):
    for line in sys.stdin:
        if not line.strip():
            continue
        try:
            msg = json.loads(line)
        except ValueError:
            sys.stdout.write(json.dumps({"jsonrpc": "2.0", "id": None,
                                         "error": {"code": -32700, "message": "parse error"}}) + "\n")
            sys.stdout.flush()
            continue
        reply = handle(tools, msg)
        if reply is not None:
            sys.stdout.write(json.dumps(reply) + "\n")
            sys.stdout.flush()
    return 0


def main(argv):
    if len(argv) >= 2 and argv[1] == "--self-test":
        return self_test()
    history, exe = None, None
    i = 1
    while i < len(argv):
        if argv[i] == "--history" and i + 1 < len(argv):
            history = argv[i + 1]; i += 2
        elif argv[i] == "--replay-exe" and i + 1 < len(argv):
            exe = argv[i + 1]; i += 2
        else:
            print(__doc__, file=sys.stderr); return 2
    if not history:
        print(__doc__, file=sys.stderr); return 2
    return serve(Tools(history, exe))


# ── Self-test ───────────────────────────────────────────────────────────

def self_test():
    import tempfile
    fails = 0

    def ok(c, what):
        nonlocal fails
        print(("  ok    " if c else "  FAIL  ") + what)
        if not c:
            fails += 1

    d = tempfile.mkdtemp()
    hist = os.path.join(d, "focus_history.jsonl")
    lines = []
    # The night of 2026-11-22: faded, no peak either way, owner moved 2,909 steps.
    night = {"kind": "sweep", "id": "1795300223-168665", "unix": 1795300223,
             "at": {"target": "Saturn", "filter": "UV/IR cut", "focuser_temp_c": 14.0},
             "success": True, "start": 168665, "result": 165984, "backlash_measured": -1, "backlash_in_use": 120,
             "coarse": {"pos": [165665, 168665, 171665], "out": [14.6, 14.8, 4.8], "back": [4.6, 4.7, 4.5]},
             "ab": {"ran": True, "verdict": "kept fit"}, "sky_changed": True}
    lines.append(night)
    lines.append({"kind": "followup", "sweep": night["id"], "settled": 168893, "moved_from_result": 2909})
    # A good sweep, left alone.
    good = {"kind": "sweep", "id": "g1", "unix": 1795310000, "at": {"target": "Saturn", "filter": "UV/IR cut", "focuser_temp_c": 13.0},
            "success": True, "start": 168000, "result": 169000,
            "coarse": {"pos": [1, 2, 3], "out": [5, 14, 5], "back": [5, 13, 6]}, "ab": {"ran": False}}
    lines += [good, {"kind": "followup", "sweep": "g1", "settled": 169010}]
    # Temperature points over three nights at -120 steps/C.
    for i, (t, n) in enumerate([(16, 0), (15, 0), (14, 1), (12, 1), (11, 2), (10, 2)]):
        sid = f"t{i}"
        lines.append({"kind": "sweep", "id": sid, "unix": 1795400000 + n * 86400 + i * 600,
                      "at": {"target": "Jupiter", "filter": "UV/IR cut", "focuser_temp_c": t}, "success": True,
                      "start": 0, "result": 170000 - 120 * (t - 10), "coarse": {"out": [1, 5, 1], "back": [1, 5, 1]}, "ab": {}})
        lines.append({"kind": "followup", "sweep": sid, "settled": 170000 - 120 * (t - 10)})
    with open(hist, "w") as f:
        for l in lines:
            f.write(json.dumps(l) + "\n")
        f.write("not json\n")
    tools = Tools(hist)

    r = handle(tools, {"jsonrpc": "2.0", "id": 1, "method": "initialize", "params": {"protocolVersion": "2025-06-18"}})
    ok(r["result"]["capabilities"] == {"tools": {}} and r["result"]["serverInfo"]["name"], "initialize")
    ok(handle(tools, {"jsonrpc": "2.0", "method": "notifications/initialized"}) is None, "a notification gets no reply")
    r = handle(tools, {"jsonrpc": "2.0", "id": 2, "method": "tools/list"})
    names = [t["name"] for t in r["result"]["tools"]]
    ok(names == ["list_sweeps", "get_sweep", "failure_summary", "temperature_model", "replay", "propose"], "six tools listed")

    def call(name, **args):
        rr = handle(tools, {"jsonrpc": "2.0", "id": 9, "method": "tools/call", "params": {"name": name, "arguments": args}})
        res = rr["result"]
        return res["isError"], (res["content"][0]["text"] if res["isError"] else json.loads(res["content"][0]["text"]))

    err, ls = call("list_sweeps", target="saturn")
    ok(not err and len(ls["sweeps"]) == 2 and ls["unreadable_lines"] == 1, "list_sweeps filters by target, counts bad lines")
    ok(ls["sweeps"][0]["night"] == "20261121" and ls["sweeps"][0]["settled"] == 168893, "the night and where the owner settled")
    err, g = call("get_sweep", id="g1")
    ok(not err and g["followup"]["settled"] == 169010, "get_sweep returns the sweep and its followup")
    err, msg = call("get_sweep", id="nope")
    ok(err and "no sweep" in msg, "an unknown id is a tool error, not a crash")

    err, fs = call("failure_summary")
    ok(not err and fs["went_wrong"] == 1 and fs["sweeps"][0]["id"] == night["id"], "only the corrected sweep went wrong")
    rs = " ".join(fs["sweeps"][0]["reasons"])
    ok("fade" in rs and "plateau" in rs and "edge" not in rs, "the night: fade, and no peak in either pass")
    ok(pass_has_peak([5, 14, 5]) and not pass_has_peak([14.6, 14.6, 4.8]) and not pass_has_peak([4, 5]),
       "peak test as core/FocusDirection.h")

    err, tm = call("temperature_model", target="Jupiter")
    f = tm["fits"][0]
    ok(not err and f["confident"] and abs(f["steps_per_c"] + 120) < 1 and f["nights"] == 3, "Jupiter: -120 steps/C, confident")
    err, tm = call("temperature_model", target="Saturn")
    ok(not tm["fits"][0]["confident"] and "points" in tm["fits"][0]["why_not"], "Saturn: two points, not confident")

    err, msg = call("replay")
    ok(("not built" in msg) if err else ("report" in msg), "replay runs focus_replay, or says it is not built")

    err, p = call("propose", change="Narrow the sweep to 1500 steps on Jupiter", why="six sweeps settled within 300 steps of the model")
    ok(not err and os.path.exists(p["written_to"]), "propose writes focus_proposals.md")
    text = open(p["written_to"]).read()
    ok("Status: proposed" in text and "Nothing here is applied" in text, "with its status and the warning")
    err, msg = call("propose", change="x", why="")
    ok(err, "a proposal without a reason is refused")

    r = handle(tools, {"jsonrpc": "2.0", "id": 3, "method": "tools/call", "params": {"name": "move_focuser", "arguments": {}}})
    ok("error" in r, "there is no tool that moves anything")
    r = handle(tools, {"jsonrpc": "2.0", "id": 4, "method": "resources/list"})
    ok(r["error"]["code"] == -32601, "unknown methods answer method-not-found")

    # The real stdio loop, end to end.
    proc = subprocess.run([sys.executable, os.path.abspath(__file__), "--history", hist],
                          input='{"jsonrpc":"2.0","id":1,"method":"initialize","params":{}}\n'
                                '{"jsonrpc":"2.0","method":"notifications/initialized"}\n'
                                'garbage\n'
                                '{"jsonrpc":"2.0","id":2,"method":"tools/list"}\n',
                          capture_output=True, text=True, timeout=30)
    replies = [json.loads(l) for l in proc.stdout.splitlines() if l.strip()]
    ok(len(replies) == 3 and replies[1]["error"]["code"] == -32700 and len(replies[2]["result"]["tools"]) == 6,
       "over stdio: initialize, a parse error, tools/list")

    print("ALL CHECKS PASSED" if fails == 0 else f"{fails} CHECK(S) FAILED")
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
