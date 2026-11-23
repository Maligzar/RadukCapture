#!/usr/bin/env python3
"""guide_report.py - Phase 172 finding 5: what is moving the guide error?

    python tools/guide_report.py raduk_log.txt [--from HH:MM] [--to HH:MM]

Reads the PLANET LOCK's [GUIDE] lines (one every 2 s while it drives):

    21:47:03.512 [GUIDE] LOCKED err 1.5" (3, -7 px)  axis err az 0.8 alt -1.2"
                 rates az 15.64+0.24 alt -7.32-0.36 "/s

and reports, for each axis, over every LOCKED stretch:
  - the error: RMS, worst, and its autocorrelation at 2, 4 and 6 s;
  - the swing period, from the error's sign changes;
  - the correction against the error (how hard the loop leans, and whether
    the correction's swings follow the error's or run ahead of them).

The question finding 5 asks (2026-11-22: altitude swung +-15 px over 6-10 s
while azimuth held +-4 px): is it the SEEING the loop is chasing, or the
LOOP overshooting? They leave different marks at a 2 s sample:
  - seeing: the error is close to white at 2 s (autocorrelation near 0,
    no steady period), and the correction is just kp x error;
  - loop overshoot: the error swings with a steady period, its
    autocorrelation goes clearly negative at half that period, and the
    swings do not die away.
The verdict is a reading of those marks, labelled as such, not a proof:
the camera's own frame-to-frame motion (the seeing itself) is not in these
lines.

Standard library only; nothing online (rule 7).

    python tools/guide_report.py --self-test
"""
import math
import re
import sys

# CentroidTracker::GUIDE_DEADBAND_AS: under this total error the guider sends
# no proportional correction (only the integral), so lines inside it say
# nothing about how hard the loop leans.
DEADBAND_AS = 0.6

RE = re.compile(
    r'^(\d\d):(\d\d):(\d\d)(?:\.(\d+))?\s+\[GUIDE\]\s+(\w+)\s+err\s+([-\d.]+)"\s+\(([-\d.]+),\s*([-\d.]+)\s*px\)\s+'
    r'axis err az\s+([-\d.]+)\s+alt\s+([-\d.]+)"\s+rates az\s+([-\d.]+)([+-][\d.]+)\s+alt\s+([-\d.]+)([+-][\d.]+)')


def parse(lines, t_from=None, t_to=None):
    rows = []
    day = 0
    last = None
    for line in lines:
        m = RE.search(line.strip())
        if not m:
            continue
        h, mi, se = int(m.group(1)), int(m.group(2)), int(m.group(3))
        frac = float("0." + m.group(4)) if m.group(4) else 0.0
        t = h * 3600 + mi * 60 + se + frac
        if last is not None and t < last - 43200:   # past midnight
            day += 1
        last = t
        hm = h * 60 + mi
        if t_from is not None and t_to is not None and t_from > t_to:   # a window across midnight
            if t_to < hm < t_from:
                continue
        else:
            if t_from is not None and hm < t_from:
                continue
            if t_to is not None and hm > t_to:
                continue
        rows.append({
            "t": t + day * 86400, "stage": m.group(5), "err": float(m.group(6)),
            "px": (float(m.group(7)), float(m.group(8))),
            "az": float(m.group(9)), "alt": float(m.group(10)),
            "base_az": float(m.group(11)), "corr_az": float(m.group(12)),
            "base_alt": float(m.group(13)), "corr_alt": float(m.group(14)),
        })
    return rows


def stretches(rows, stage="LOCKED", max_gap=10.0):
    """Runs of consecutive lines in `stage`, split where the log goes quiet."""
    out, cur = [], []
    for r in rows:
        if r["stage"] != stage or (cur and r["t"] - cur[-1]["t"] > max_gap):
            if len(cur) >= 8:
                out.append(cur)
            cur = []
        if r["stage"] == stage:
            cur.append(r)
    if len(cur) >= 8:
        out.append(cur)
    return out


def autocorr(x, lag):
    n = len(x)
    if n <= lag + 2:
        return None
    m = sum(x) / n
    var = sum((v - m) ** 2 for v in x)
    if var <= 1e-12:
        return None
    return sum((x[i] - m) * (x[i + lag] - m) for i in range(n - lag)) / var


def swing_period(x, dt):
    """Twice the mean time between sign changes about the mean; None if too
    few, or if the crossings are irregular (noise, not a swing)."""
    m = sum(x) / len(x)
    cross = [i for i in range(1, len(x)) if (x[i - 1] - m) * (x[i] - m) < 0]
    if len(cross) < 3:
        return None
    gaps = [cross[i] - cross[i - 1] for i in range(1, len(cross))]
    mean = sum(gaps) / len(gaps)
    # A swing is steady when its half-periods are alike; noise crosses at
    # irregular intervals and has no period to report.
    sd = math.sqrt(sum((g - mean) ** 2 for g in gaps) / len(gaps))
    if mean <= 0 or sd / mean > 0.5:
        return None
    return 2.0 * dt * mean


def axis_report(st, key, corr_key):
    err = [r[key] for r in st]
    corr = [r[corr_key] for r in st]
    dts = [st[i]["t"] - st[i - 1]["t"] for i in range(1, len(st))]
    dt = sorted(dts)[len(dts) // 2] if dts else 2.0
    rms = math.sqrt(sum(e * e for e in err) / len(err))
    ac = {lag: autocorr(err, lag) for lag in (1, 2, 3)}
    per = swing_period(err, dt)
    # How hard the loop leans: the slope of correction on error, from the
    # lines outside the deadband only (inside it the proportional term is
    # off, which would read as a weak loop).
    me = sum(err) / len(err)
    inside = sum(1 for r in st if r["err"] <= DEADBAND_AS)
    act = [(r[key], r[corr_key]) for r in st if r["err"] > DEADBAND_AS]
    gain = None
    if len(act) >= 4:
        ae = sum(e for e, _ in act) / len(act)
        ac_ = sum(c for _, c in act) / len(act)
        see = sum((e - ae) ** 2 for e, _ in act)
        if see > 1e-12:
            gain = sum((e - ae) * (c - ac_) for e, c in act) / see
    # Does the error's swing grow or fade across the stretch?
    half = len(err) // 2
    a1 = math.sqrt(sum((e - me) ** 2 for e in err[:half]) / max(1, half))
    a2 = math.sqrt(sum((e - me) ** 2 for e in err[half:]) / max(1, len(err) - half))
    return {"n": len(err), "dt": dt, "rms": rms, "worst": max(abs(e) for e in err),
            "ac": ac, "period": per, "gain": gain, "trend": (a2 / a1) if a1 > 1e-9 else None,
            "deadband_share": inside / len(st), "active": len(act)}


def verdict(r):
    """A reading of the marks, not a proof."""
    if r["rms"] < 0.5:
        return "quiet: nothing to explain"
    ac1, ac2 = r["ac"].get(1), r["ac"].get(2)
    per = r["period"]
    oscillating = per is not None and 4.0 <= per <= 20.0 and (
        (ac2 is not None and ac2 < -0.25) or (ac1 is not None and ac1 < -0.25))
    if oscillating:
        grow = r["trend"] is not None and r["trend"] > 1.2
        return ("looks like the LOOP: a steady %.0f s swing with negative autocorrelation at half of it%s"
                " - soften this axis's gain" % (per, ", growing" if grow else ""))
    if ac1 is not None and abs(ac1) < 0.3:
        return "looks like SEEING: close to white at %.0f s, no steady swing" % r["dt"]
    if ac1 is not None and ac1 > 0.6:
        if r.get("deadband_share", 0.0) >= 0.4:
            return ("slow wander inside the guider's %.1f\" deadband (%.0f%% of lines), where only the integral "
                    "acts: drift until the error crosses it, then a push back - working as designed"
                    % (DEADBAND_AS, 100.0 * r["deadband_share"]))
        return "slow wander (strong positive autocorrelation): a rate or drift, not seeing or overshoot"
    return "mixed: neither mark is clear"


def report(rows):
    out = []
    sts = stretches(rows)
    if not sts:
        return "no LOCKED stretch of 8 or more [GUIDE] lines found"
    for i, st in enumerate(sts, 1):
        t0, t1 = st[0]["t"] % 86400, st[-1]["t"] % 86400
        out.append("LOCKED stretch %d: %s to %s, %d lines" % (
            i, "%02d:%02d:%02d" % (t0 // 3600, t0 % 3600 // 60, t0 % 60),
            "%02d:%02d:%02d" % (t1 // 3600, t1 % 3600 // 60, t1 % 60), len(st)))
        for name, key, ck in (("az", "az", "corr_az"), ("alt", "alt", "corr_alt")):
            r = axis_report(st, key, ck)
            ac = "  ".join("%ds %s" % (int(l * r["dt"]), "%+.2f" % v if v is not None else "-")
                           for l, v in r["ac"].items())
            out.append("  %-3s err RMS %.2f\" worst %.1f\"  autocorr %s  swing %s  loop gain %s  trend %s" % (
                name, r["rms"], r["worst"], ac,
                "%.0f s" % r["period"] if r["period"] else "-",
                ("%.2f /s" % r["gain"]) if r["gain"] is not None else "- (too few lines outside the deadband)",
                "x%.2f" % r["trend"] if r["trend"] is not None else "-"))
            out.append("      %.0f%% of lines inside the %.1f\" deadband; the loop gain is from the other %d"
                       % (100.0 * r["deadband_share"], DEADBAND_AS, r["active"]))
            out.append("      %s" % verdict(r))
    out.append("(a reading of the marks in the [GUIDE] lines, not a proof: the seeing itself is not logged)")
    return "\n".join(out)


def diagnose(lines, t_from=None, t_to=None):
    """Why nothing was found: no [GUIDE] lines, lines in another format,
    all outside the window, or no LOCKED stage among them."""
    guide = [l.rstrip("\n") for l in lines if "[GUIDE]" in l]
    status = [l for l in guide if " err " in l and "axis err" in l]
    parsed_all = parse(lines)
    parsed_win = parse(lines, t_from, t_to)
    out = ["why: %d [GUIDE] line(s) in the file, %d of them status lines, %d read, %d inside the window"
           % (len(guide), len(status), len(parsed_all), len(parsed_win))]
    if not guide:
        out.append("  no [GUIDE] lines at all: PLANET LOCK did not drive in this log (try the other log file)")
    elif status and not parsed_all:
        out.append("  the status lines are in a format this tool does not read; first one:")
        out.append("    " + status[0][:200])
    elif parsed_all and not parsed_win:
        def hm(t):
            t %= 86400
            return "%02d:%02d" % (t // 3600, t % 3600 // 60)
        out.append("  every line is outside the window; the log's [GUIDE] lines run %s to %s"
                   % (hm(parsed_all[0]["t"]), hm(parsed_all[-1]["t"])))
    elif parsed_win:
        stages = {}
        for r in parsed_win:
            stages[r["stage"]] = stages.get(r["stage"], 0) + 1
        out.append("  stages seen: " + ", ".join("%s %d" % kv for kv in sorted(stages.items()))
                   + " (a stretch needs 8 or more LOCKED lines with no gap over 10 s)")
    return "\n".join(out)


def main(argv):
    if len(argv) >= 2 and argv[1] == "--self-test":
        return self_test()
    if len(argv) < 2:
        print(__doc__)
        return 2
    t_from = t_to = None
    i = 2
    while i < len(argv):
        if argv[i] in ("--from", "--to") and i + 1 < len(argv):
            h, m = argv[i + 1].split(":")
            v = int(h) * 60 + int(m)
            if argv[i] == "--from":
                t_from = v
            else:
                t_to = v
            i += 2
        else:
            print("unknown argument " + argv[i], file=sys.stderr)
            return 2
    with open(argv[1], encoding="utf-8", errors="replace") as f:
        lines = f.readlines()
    rows = parse(lines, t_from, t_to)
    print(report(rows))
    if not stretches(rows):
        print(diagnose(lines, t_from, t_to))
    return 0


def _line(t, stage, az, alt, corr_az, corr_alt):
    h, m, s = int(t // 3600) % 24, int(t % 3600 // 60), t % 60
    return ('%02d:%02d:%06.3f [GUIDE] %s err %.1f" (%.0f, %.0f px)  axis err az %.1f alt %.1f"  '
            'rates az 15.64%+.2f alt -7.32%+.2f "/s' % (h, m, s, stage, math.hypot(az, alt), az * 5, alt * 5,
                                                        az, alt, corr_az, corr_alt))


def self_test():
    import random
    fails = 0

    def ok(c, what):
        nonlocal fails
        print(("  ok    " if c else "  FAIL  ") + what)
        if not c:
            fails += 1

    rng = random.Random(3)
    lines = ["21:00:00.000 [CAL] something else", "noise"]
    t = 21 * 3600.0
    # Azimuth: seeing (white, small). Altitude: an 8 s loop swing of 3".
    for i in range(120):
        az = rng.gauss(0, 0.6)
        alt = 3.0 * math.sin(2 * math.pi * (i * 2.0) / 8.0 + 0.4) + rng.gauss(0, 0.3)
        lines.append(_line(t, "LOCKED", az, alt, 0.3 * az, 0.3 * alt + 0.5))
        t += 2.0
    lines.append(_line(t + 30, "CENTERING", 20, 20, 9, 9))   # a gap and a stage change end the stretch
    rows = parse(lines)
    ok(len(rows) == 121, "every [GUIDE] status line read, others ignored")
    one = parse(['21:47:03.512 [GUIDE] LOCKED err 1.5" (3, -7 px)  axis err az 0.8 alt -1.2"  '
                 'rates az 15.64+0.24 alt -7.32-0.36 "/s'])
    ok(len(one) == 1 and one[0]["az"] == 0.8 and one[0]["alt"] == -1.2 and one[0]["base_az"] == 15.64
       and one[0]["corr_az"] == 0.24 and one[0]["base_alt"] == -7.32 and one[0]["corr_alt"] == -0.36,
       "a line as the app writes it, signed corrections included")
    sts = stretches(rows)
    ok(len(sts) == 1 and len(sts[0]) == 120, "one LOCKED stretch")
    az = axis_report(sts[0], "az", "corr_az")
    alt = axis_report(sts[0], "alt", "corr_alt")
    ok(abs(az["ac"][1]) < 0.3, "azimuth is near white at 2 s")
    ok("SEEING" in verdict(az), "azimuth reads as seeing")
    ok(az["period"] is None, "noise has no swing period")
    ok(alt["period"] is not None and 6.5 <= alt["period"] <= 9.5, "altitude's swing period found (8 s)")
    ok(alt["ac"][2] is not None and alt["ac"][2] < -0.5, "negative autocorrelation at half the period")
    ok("LOOP" in verdict(alt), "altitude reads as the loop")
    ok(alt["gain"] is not None and abs(alt["gain"] - 0.3) < 0.05, "the loop's lean (kp) recovered")
    txt = report(rows)
    ok("LOCKED stretch 1" in txt and "not a proof" in txt, "the report names the stretch and its limits")
    # Window filter, across midnight.
    late = [_line(23 * 3600 + 59 * 60 + 50 + 2 * i, "LOCKED", 0.1, 0.1, 0, 0) for i in range(20)]
    ok(len(parse(late, 23 * 60 + 50, 0 * 60 + 10)) == 20, "a window across midnight keeps both sides")
    ok(stretches(parse(late))[0][-1]["t"] > stretches(parse(late))[0][0]["t"], "time keeps rising past midnight")
    ok("no LOCKED stretch" in report(parse(["nothing"])), "an empty log says so")
    # 2026-11-23 21:44 (the owner's run): altitude wandered slowly round
    # 0.5", mostly inside the 0.6" deadband, and the lean read 0.10 instead
    # of the loop's 0.3 because the proportional term was off for those lines.
    wl = []
    tt = 21 * 3600 + 44 * 60.0
    alt_v = 0.0
    for i in range(40):
        alt_v = 0.85 * alt_v + rng.gauss(0, 0.25) + (0.08 if i < 20 else -0.05)
        tot = math.hypot(0.1, alt_v)
        p_on = tot > DEADBAND_AS
        c_alt = (0.3 * alt_v if p_on else 0.0) + 0.02
        wl.append('%02d:%02d:%06.3f [GUIDE] LOCKED err %.1f" (0, %.0f px)  axis err az 0.1 alt %.2f"  '
                  'rates az 15.64+0.03 alt -7.32%+.3f "/s' % (int(tt // 3600), int(tt % 3600 // 60), tt % 60,
                                                              tot, alt_v * 5, alt_v, c_alt))
        tt += 2.0
    wr = axis_report(stretches(parse(wl))[0], "alt", "corr_alt")
    ok(wr["deadband_share"] > 0.3, "the deadband share is reported")
    ok(wr["gain"] is None or abs(wr["gain"] - 0.3) < 0.08, "the lean is the loop's 0.3, not diluted by the deadband")
    ok("no [GUIDE] lines at all" in diagnose(["nothing"]), "it says when there are no guide lines")
    ok("outside the window" in diagnose(lines, 23 * 60, 23 * 60 + 30) and "21:00" in diagnose(lines, 23 * 60, 23 * 60 + 30),
       "it says when the window misses them, and when they run")
    odd = ['21:00:00 [GUIDE] LOCKED err 1.5 px  axis err az 0.8 alt -1.2 (other format)']
    ok("format" in diagnose(odd), "it shows a line it cannot read")
    cen = [_line(21 * 3600 + 2 * i, "CENTERING", 3, 3, 1, 1) for i in range(20)]
    ok("CENTERING 20" in diagnose(cen), "it names the stages when none is LOCKED")
    print("ALL CHECKS PASSED" if fails == 0 else "%d CHECK(S) FAILED" % fails)
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
