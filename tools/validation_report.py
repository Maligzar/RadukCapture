#!/usr/bin/env python3
"""Phase 155.4: what the nights say about the UNVERIFIED constants.

The app writes validation_<night>_<hhmmss>.json into its logs folder through
every real night (src/core/ValidationLog.h). This joins any number of them
and writes one report: for each clear-night item it can speak to, what the
data shows, with a 95% confidence interval, and a recommendation only when
enough independent data agrees. Too little data is said, never filled in.

    python tools/validation_report.py <folder or files...>  [-o report.md]
    python tools/validation_report.py --self-test

Rules for a recommendation (deliberately strict; a constant that moves on
thin evidence is worse than one that waits):
  - plate scale (items 3, 15): at least 3 solves on at least 2 nights; the
    95% CI of measured/configured lies wholly outside +-0.5%, and its
    half-width is under 1%.
  - SHARP_CEIL (item 1): at least 3 nights and 5000 frames of one target.
    More than 1% of frames at the 100% cap says the ceiling is too low; a
    99th percentile under 40% says it is too high. Either way the report
    says by how much, not what to type.
  - Autorun's seeing bar (item 37): the share of the night above each bar
    (60-90% of the night's best 10 s median), so the bar is chosen knowing
    how often it would have held a run.
  - backlash (items 47, 58): at least 5 successful sweeps that measured
    one, on at least 2 nights; the median's 95% CI (by bootstrap) half-width
    under 15% of the median.
  - exposure (item 55): per target, the exposures that kept the highlight
    in 150-230 DN with under 0.1% clipped, against the profile's band.

Standard library only, and no network: it runs offline and in CI.
"""
import argparse
import glob
import json
import math
import os
import random
import statistics
import sys

Z95 = 1.959964


def load(paths):
    files = []
    for p in paths:
        if os.path.isdir(p):
            files += sorted(glob.glob(os.path.join(p, "validation_*.json")))
        else:
            files.append(p)
    runs = []
    for f in files:
        try:
            with open(f, encoding="utf-8") as fh:
                d = json.load(fh)
        except (OSError, ValueError) as e:
            print(f"skipped {f}: {e}", file=sys.stderr)
            continue
        if d.get("format") != "raduk-validation":
            print(f"skipped {f}: not a validation file", file=sys.stderr)
            continue
        d["_file"] = os.path.basename(f)
        runs.append(d)
    return runs


def mean_ci(xs):
    """Mean and the half-width of its 95% CI (normal approximation; n >= 2)."""
    m = statistics.fmean(xs)
    if len(xs) < 2:
        return m, float("inf")
    return m, Z95 * statistics.stdev(xs) / math.sqrt(len(xs))


def median_ci(xs, rounds=2000, seed=1):
    """Median and a bootstrap 95% CI (percentile method), reproducible."""
    rng = random.Random(seed)
    meds = sorted(statistics.median(rng.choices(xs, k=len(xs))) for _ in range(rounds))
    return statistics.median(xs), meds[int(0.025 * rounds)], meds[int(0.975 * rounds) - 1]


def hist_percentile(h, q):
    n = sum(h)
    if n == 0:
        return None
    target = q * n
    acc = 0
    for i, c in enumerate(h):
        acc += c
        if acc >= target:
            return i
    return len(h) - 1


def rolling_median(samples, window=10.0):
    """10 s rolling median of (t, v) samples, as SeeingMonitor judges."""
    out, lo = [], 0
    for i, (t, _) in enumerate(samples):
        while samples[lo][0] < t - window:
            lo += 1
        out.append(statistics.median(v for _, v in samples[lo:i + 1]))
    return out


# ── The sections ─────────────────────────────────────────────

def plate_scale(runs):
    rows = [(r["night"], s) for r in runs for s in r.get("solves", []) if s.get("configured", 0) > 0]
    out = ["## Plate scale (items 3 and 15)", ""]
    if not rows:
        return out + ["No solves recorded.", ""], None
    # The configured figure is per sensor pixel; a binned solve measures bin x it.
    ratios = [s["measured"] / max(1, s.get("bin", 1)) / s["configured"] for _, s in rows]
    nights = sorted({n for n, _ in rows})
    m, hw = mean_ci(ratios)
    conf = rows[-1][1]["configured"]
    out.append(f"{len(rows)} solves on {len(nights)} night(s). Measured / configured: "
               f"{m:.4f}, 95% CI +-{hw:.4f}. Configured now: {conf:.4f}\"/px.")
    rec = None
    if len(rows) >= 3 and len(nights) >= 2 and hw < 0.01 and (m - hw > 1.005 or m + hw < 0.995):
        rec = conf * m
        out.append(f"**Recommendation:** PlateScaleMeasuredArcsecPx = {rec:.4f} in rig.ini "
                   f"({(m - 1) * 100:+.2f}%; the whole interval is outside +-0.5%).")
    else:
        why = []
        if len(rows) < 3: why.append("fewer than 3 solves")
        if len(nights) < 2: why.append("one night only")
        if hw >= 0.01: why.append("the interval is wider than 1%")
        if not why: why.append("the configured figure is within 0.5%")
        out.append("No change recommended: " + "; ".join(why) + ".")
    return out + [""], rec


def sharpness(runs):
    out = ["## Sharpness and SHARP_CEIL (item 1)", ""]
    pooled, nights = {}, {}
    for r in runs:
        for tgt, t in r.get("targets", {}).items():
            for h in t.get("sharpness", {}).values():
                acc = pooled.setdefault(tgt, [0] * len(h))
                for i, c in enumerate(h):
                    acc[i] += c
                nights.setdefault(tgt, set()).add(r["night"])
    if not pooled:
        return out + ["No frames recorded.", ""], {}
    recs = {}
    out += ["| Target | Frames | Nights | p50 | p95 | p99 | At the cap |", "|---|---|---|---|---|---|---|"]
    for tgt in sorted(pooled):
        h = pooled[tgt]
        n = sum(h)
        cap = h[-1] / n if n else 0
        p50, p95, p99 = (hist_percentile(h, q) for q in (0.5, 0.95, 0.99))
        out.append(f"| {tgt} | {n} | {len(nights[tgt])} | {p50}% | {p95}% | {p99}% | {cap * 100:.2f}% |")
        if len(nights[tgt]) >= 3 and n >= 5000:
            if cap > 0.01:
                recs[tgt] = f"{cap * 100:.1f}% of {tgt}'s frames sit at the cap: SHARP_CEIL is too low for it"
            elif p99 is not None and p99 < 40:
                recs[tgt] = (f"{tgt}'s 99th percentile is {p99}%: SHARP_CEIL could come down by about "
                             f"{100 / max(p99, 1):.1f}x and the best frames would still not clip")
    out.append("")
    if recs:
        out += ["**Recommendations:**"] + [f"- {v}." for v in recs.values()]
    else:
        out.append("No change recommended: a target needs 3 nights and 5000 frames before the ceiling is judged.")
    return out + [""], recs


def seeing_bar(runs, bars=(60, 70, 80, 90)):
    out = ["## Autorun's seeing bar (item 37)", "",
           "The share of each night's samples whose 10 s median was at or above the bar, "
           "as a percentage of that night's best 10 s median.", "",
           "| Night | Target | Samples | " + " | ".join(f"{b}%" for b in bars) + " |",
           "|---|---|---|" + "---|" * len(bars)]
    any_row = False
    table = {}
    for r in runs:
        for tgt, t in r.get("targets", {}).items():
            s = [(a, b) for a, b in t.get("seeing", []) if b and b > 0]
            if len(s) < 20:
                continue
            med = rolling_median(s)
            best = max(med)
            shares = [sum(1 for v in med if v >= best * b / 100) / len(med) for b in bars]
            table[(r["night"], tgt)] = shares
            out.append(f"| {r['night']} | {tgt} | {len(s)} | " +
                       " | ".join(f"{x * 100:.0f}%" for x in shares) + " |")
            any_row = True
    if not any_row:
        out = out[:2] + ["Not enough seeing samples (20 a target a night)."]
    return out + [""], table


def backlash(runs):
    out = ["## Backlash (items 47 and 58)", ""]
    rows = [(r["night"], f) for r in runs for f in r.get("focus", [])]
    if not rows:
        return out + ["No autofocus runs recorded.", ""], None
    ok = [(n, f) for n, f in rows if f.get("success")]
    meas = [(n, f["backlash_measured"]) for n, f in ok if f.get("backlash_measured", 0) > 0]
    out.append(f"{len(rows)} runs, {len(ok)} succeeded, {len(meas)} measured the backlash.")
    rec = None
    if meas:
        vals = [v for _, v in meas]
        nights = {n for n, _ in meas}
        med, lo, hi = median_ci(vals)
        inuse = ok[-1][1].get("backlash_in_use", 0)
        out.append(f"Measured: median {med:.0f} steps, 95% CI {lo:.0f}-{hi:.0f}, on {len(nights)} night(s); "
                   f"in use: {inuse}.")
        if len(vals) >= 5 and len(nights) >= 2 and (hi - lo) / 2 < 0.15 * med:
            rec = med
            out.append(f"**Recommendation:** backlash = {med:.0f} steps (the interval is within 15% of it).")
        else:
            out.append("No change recommended: needs 5 measured sweeps on 2 nights, and an interval within 15%.")
    by = {}
    for n, f in ok:
        by.setdefault((n, f.get("target", "")), []).append(f["best"])
    rep = [(k, v) for k, v in by.items() if len(v) >= 3]
    if rep:
        out += ["", "Repeatability (3 or more successful sweeps of a target in a night):", "",
                "| Night | Target | Sweeps | Best positions SD (steps) |", "|---|---|---|---|"]
        for (n, t), v in sorted(rep):
            out.append(f"| {n} | {t} | {len(v)} | {statistics.stdev(v):.0f} |")
    return out + [""], rec


def unimodal(scores):
    """The fine curve rises and falls once: one interior local maximum, and
    it is the highest point."""
    if len(scores) < 3:
        return False
    peaks = [i for i in range(1, len(scores) - 1) if scores[i] > scores[i - 1] and scores[i] > scores[i + 1]]
    best = max(range(len(scores)), key=lambda i: scores[i])
    return len(peaks) == 1 and peaks[0] == best


def fine_pass(runs):
    out = ["## The fine pass (items 47 and 58)", ""]
    rows = [(r["night"], f) for r in runs for f in r.get("focus", []) if f.get("fine_passes", 0) > 0]
    if not rows:
        return out + ["No fine passes recorded (files from before 3.3.37 do not carry them).", ""], {}
    used = [f for _, f in rows if f.get("fine_used")]
    offs = [f["fine_peak"] - f["coarse_peak"] for f in used if f.get("coarse_peak", -1) >= 0 and f.get("fine_peak", -1) >= 0]
    fades = [f for _, f in rows if f.get("fade_action")]
    curves = [f for _, f in rows if len(f.get("fine_score", [])) >= 3]
    uni = [f for f in curves if unimodal(f["fine_score"])]
    spreads = [x for f in curves for x in f.get("fine_spread", []) if x is not None and x >= 0]
    out.append(f"{len(rows)} sweeps ran a fine pass; its result was used in {len(used)}.")
    res = {"used": len(used), "runs": len(rows)}
    if offs:
        med, lo, hi = median_ci(offs)
        out.append(f"Fine minus coarse: median {med:+.0f} steps (95% CI {lo:+.0f} to {hi:+.0f}), "
                   f"range {min(offs):+d} to {max(offs):+d}.")
        res["offset"] = med
    if fades:
        acts = {a: sum(1 for f in fades if f["fade_action"] == a) for a in ("steady", "corrected", "gave up")}
        ratios = [f["fade_ratio"] for f in fades if f.get("fade_ratio")]
        out.append(f"Fade check: {acts['steady']} steady, {acts['corrected']} corrected, {acts['gave up']} gave up"
                   + (f"; ratio median x{statistics.median(ratios):.2f}, range x{min(ratios):.2f}-x{max(ratios):.2f}" if ratios else "")
                   + ". On a clear, steady night a ratio far from 1 means the target moved in the frame, not a fade.")
        res["fades"] = acts
    if curves:
        out.append(f"The last fine curve rose and fell once in {len(uni)} of {len(curves)} sweeps.")
        res["unimodal"] = (len(uni), len(curves))
    if spreads:
        noise = statistics.median(spreads)
        contrast = [(max(f["fine_score"]) - min(f["fine_score"])) / statistics.median(f["fine_score"])
                    for f in curves if statistics.median(f["fine_score"]) > 0]
        line = f"Edge metric noise floor: a point's frames spread by {noise * 100:.1f}% of their median (median over {len(spreads)} points)"
        if contrast:
            c = statistics.median(contrast)
            line += f"; a fine curve's own contrast is {c * 100:.0f}%, {c / noise:.1f}x the noise" if noise > 0 else ""
            res["snr"] = c / noise if noise > 0 else None
        out.append(line + ".")
        res["noise"] = noise
    return out + [""], res


def exposure(runs, lo_dn=150, hi_dn=230, max_clip=0.1):
    out = ["## Exposure and the band (item 55)", "",
           f"Per target, the exposures that kept the target's highlight in {lo_dn}-{hi_dn} DN with under "
           f"{max_clip}% clipped, against the profile's band.", ""]
    agg = {}
    for r in runs:
        for tgt, t in r.get("targets", {}).items():
            a = agg.setdefault(tgt, {"good": [], "all": 0, "band": (0, 0)})
            if t.get("band_ceil_ms", 0) > t.get("band_floor_ms", 0) > 0:
                a["band"] = (t["band_floor_ms"], t["band_ceil_ms"])
            for row in t.get("exposure", []):
                _, exp_ms, _gain, hl, clip, _fp = row
                a["all"] += 1
                if lo_dn <= hl <= hi_dn and clip < max_clip:
                    a["good"].append(exp_ms)
    if not agg or all(a["all"] == 0 for a in agg.values()):
        return out + ["No exposure samples recorded.", ""], {}
    out += ["| Target | Samples | In range | Exposure in range (ms), 5th-95th | Band now (ms) |", "|---|---|---|---|---|"]
    res = {}
    for tgt in sorted(agg):
        a = agg[tgt]
        if a["all"] == 0:
            continue
        g = sorted(a["good"])
        rng = f"{g[int(0.05 * (len(g) - 1))]:.2f}-{g[int(0.95 * (len(g) - 1))]:.2f}" if len(g) >= 2 else "--"
        band = f"{a['band'][0]:.2f}-{a['band'][1]:.2f}" if a["band"][1] > 0 else "none"
        out.append(f"| {tgt} | {a['all']} | {len(g)} | {rng} | {band} |")
        res[tgt] = (len(g), a["all"])
    return out + [""], res


def report(runs):
    nights = sorted({r["night"] for r in runs})
    lines = ["# Validation report", "",
             f"{len(runs)} file(s), {len(nights)} night(s): {', '.join(nights) or 'none'}.", ""]
    for section in (plate_scale, sharpness, seeing_bar, backlash, fine_pass, exposure):
        part, _ = section(runs)
        lines += part
    lines.append("A recommendation is evidence for the owner to act on; nothing here changes a setting.")
    return "\n".join(lines) + "\n"


# ── Self-test (run by CI) ─────────────────────────────────────

def _run(night, solves=(), focus=(), targets=None):
    return {"format": "raduk-validation", "night": night, "solves": list(solves),
            "focus": list(focus), "targets": targets or {}}


def self_test():
    fails = []

    def check(ok, what):
        print(f"  [{'PASS' if ok else 'FAIL'}] {what}")
        if not ok:
            fails.append(what)

    s = lambda m: {"measured": m, "configured": 0.200}
    # Plate scale: consistent 2% high on two nights -> recommended.
    out, rec = plate_scale([_run("20261120", [s(0.204), s(0.2041)]), _run("20261121", [s(0.2039)])])
    check(rec is not None and abs(rec - 0.204) < 0.0005, "plate scale: 2% off on 2 nights is recommended")
    out, rec = plate_scale([_run("20261120", [s(0.204), s(0.2041), s(0.2039)])])
    check(rec is None and any("one night only" in l for l in out), "plate scale: one night is not enough, and says so")
    out, rec = plate_scale([_run("20261120", [s(0.2004), s(0.2005)]), _run("20261121", [s(0.2003)])])
    check(rec is None, "plate scale: within 0.5% is left alone")
    sb = {"measured": 0.408, "configured": 0.200, "bin": 2}
    out, rec = plate_scale([_run("20261120", [sb, sb]), _run("20261121", [sb])])
    check(rec is not None and abs(rec - 0.204) < 0.0005, "plate scale: a bin-2 solve is halved before comparing")
    out, rec = plate_scale([])
    check(rec is None and "No solves recorded." in out, "plate scale: no data says no data")

    # Sharpness: 2% at the cap on 3 nights with enough frames -> too low.
    h = [0] * 101
    h[50] = 1960
    h[100] = 40
    t = {"Jupiter": {"sharpness": {"5.00|300": h}}}
    _, recs = sharpness([_run("2026112%d" % i, targets=t) for i in range(3)])
    check("Jupiter" in recs and "too low" in recs["Jupiter"], "SHARP_CEIL: frames at the cap say too low")
    _, recs = sharpness([_run("20261120", targets=t)])
    check(not recs, "SHARP_CEIL: one night is not judged")
    h2 = [0] * 101
    h2[20] = 2000
    _, recs = sharpness([_run("2026112%d" % i, targets={"Saturn": {"sharpness": {"9|300": h2}}}) for i in range(3)])
    check("Saturn" in recs and "come down" in recs["Saturn"], "SHARP_CEIL: a low 99th percentile says too high")

    # Seeing: a night half good, half at 50% of best.
    samples = [[i, 100.0 if i < 100 else 50.0] for i in range(200)]
    _, table = seeing_bar([_run("20261120", targets={"Jupiter": {"seeing": samples}})])
    sh = table[("20261120", "Jupiter")]
    check(abs(sh[2] - 0.5) < 0.06 and sh[0] >= sh[3], "seeing bar: half the night above 80%, and shares fall as the bar rises")

    # Backlash: 6 sweeps on 2 nights, tight -> recommended; 2 sweeps -> not.
    f = lambda b, best=1000: {"success": True, "backlash_measured": b, "backlash_in_use": 200, "best": best, "target": "Jupiter"}
    _, rec = backlash([_run("20261120", focus=[f(150), f(152), f(149)]), _run("20261121", focus=[f(151), f(150), f(148)])])
    check(rec is not None and abs(rec - 150) <= 1, "backlash: 6 agreeing sweeps on 2 nights are recommended")
    _, rec = backlash([_run("20261120", focus=[f(150), f(180)])])
    check(rec is None, "backlash: 2 sweeps are not enough")

    # Exposure: in-range samples counted.
    ex = {"Jupiter": {"exposure": [[0, 5.0, 300, 200, 0.0, 3], [1, 8.0, 300, 250, 0.5, 3], [2, 6.0, 300, 180, 0.05, 3]],
                      "band_floor_ms": 4, "band_ceil_ms": 9}}
    _, res = exposure([_run("20261120", targets=ex)])
    check(res.get("Jupiter") == (2, 3), "exposure: two of three samples in range")

    # The fine pass: offsets, fades, curve shape, noise.
    fp = lambda coarse, fine, scores, spread, act="steady", ratio=1.02: {
        "success": True, "fine_passes": 1, "fine_used": fine >= 0, "coarse_peak": coarse, "fine_peak": fine,
        "fade_action": act, "fade_ratio": ratio, "fine_score": scores, "fine_spread": spread, "best": fine}
    up_down = [0.2, 0.3, 0.45, 0.5, 0.44, 0.31, 0.2]
    zigzag = [0.2, 0.4, 0.3, 0.5, 0.35, 0.45, 0.2]
    _, res = fine_pass([_run("20261120", focus=[fp(1000, 1030, up_down, [0.02] * 7),
                                                fp(1000, 1040, zigzag, [0.03] * 7, "corrected", 0.85),
                                                fp(1000, -1, up_down, [0.02] * 7)])])
    check(res["runs"] == 3 and res["used"] == 2, "fine pass: runs and uses counted")
    check(abs(res["offset"] - 35) <= 5, "fine pass: the offset from the coarse peak")
    check(res["fades"] == {"steady": 2, "corrected": 1, "gave up": 0}, "fine pass: the fade checks counted")
    check(res["unimodal"] == (2, 3), "fine pass: a zigzag curve is not counted as rising and falling once")
    check(abs(res["noise"] - 0.02) < 1e-9 and res["snr"] > 10, "fine pass: the noise floor and the curve's contrast over it")
    out, res = fine_pass([_run("20261120", focus=[{"success": True, "best": 1}])])
    check(res == {} and "before 3.3.37" in out[2], "fine pass: old files say they carry none")

    text = report([_run("20261120")])
    check(text.startswith("# Validation report") and "nothing here changes a setting" in text, "the report runs on an empty night")
    print("\n" + ("ALL CHECKS PASSED" if not fails else f"{len(fails)} CHECK(S) FAILED"))
    return 1 if fails else 0


def main(argv):
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("paths", nargs="*")
    ap.add_argument("-o", "--out")
    ap.add_argument("--self-test", action="store_true")
    a = ap.parse_args(argv)
    if a.self_test:
        return self_test()
    if not a.paths:
        ap.error("give a folder of validation_*.json files, or the files")
    runs = load(a.paths)
    if not runs:
        print("no validation files found", file=sys.stderr)
        return 1
    text = report(runs)
    if a.out:
        with open(a.out, "w", encoding="utf-8", newline="\n") as fh:
            fh.write(text)
        print(f"wrote {a.out}")
    else:
        sys.stdout.write(text)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
