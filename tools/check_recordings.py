#!/usr/bin/env python3
"""Phase 157.2: read RadukCapture's recordings without RadukCapture's code.

Two modes:

    check_recordings.py --manifest DIR   files from tests/write_recordings.cpp:
                                         structure, every pixel, every stamp
    check_recordings.py --scan DIR       any recordings (a soak's, a night's):
                                         structure and stamps, no pixels

SER is read by the reader below, written from the SER v3 specification
(Grischa Hahn, 2014) and NOT from src/core. FITS is read by astropy, which
is imported only when a FITS file is met, so --scan over SER files needs
nothing beyond the standard library.

Exit code: 0 when every file passes, 1 otherwise. --json PATH writes the
same verdicts for a script to read.
"""

import argparse
import datetime as dt
import json
import os
import struct
import sys

# ── SER v3, from the specification ──────────────────────────────────────
#
#   offset size
#      0    14  FileID "LUCAM-RECORDER"
#     14     4  LuID
#     18     4  ColorID      0 MONO, 8-11 BAYER_RGGB/GRBG/GBRG/BGGR,
#                            16-19 BAYER_CYYM/YCMY/YMCY/MYYC, 100 RGB, 101 BGR
#     22     4  LittleEndian
#     26     4  ImageWidth
#     30     4  ImageHeight
#     34     4  PixelDepthPerPlane   1..16
#     38     4  FrameCount
#     42    40  Observer
#     82    40  Instrument
#    122    40  Telescope
#    162     8  DateTime      local, 100 ns ticks since 0001-01-01
#    170     8  DateTime_UTC  the same in UTC
#    178        image data: FrameCount frames of W*H*planes*bytes
#               then an optional trailer: FrameCount int64 UTC stamps
#
# The specification says LittleEndian=1 means little-endian samples.
# FireCapture, and so most readers, use 0 for little-endian, and RadukCapture
# writes 0 to match (see SERWriter.h). The flag is reported, not trusted: in
# --manifest mode the pixels decide, read as little-endian.

SER_HEADER = 178
SER_COLOUR = {0: "MONO", 8: "RGGB", 9: "GRBG", 10: "GBRG", 11: "BGGR",
              16: "CYYM", 17: "YCMY", 18: "YMCY", 19: "MYYC", 100: "RGB", 101: "BGR"}
TICKS_PER_SEC = 10_000_000
EPOCH = dt.datetime(1, 1, 1, tzinfo=dt.timezone.utc)
# A stamp outside these years is garbage, not a date.
PLAUSIBLE = (dt.datetime(2000, 1, 1, tzinfo=dt.timezone.utc),
             dt.datetime(2100, 1, 1, tzinfo=dt.timezone.utc))


def ticks_to_dt(t):
    return EPOCH + dt.timedelta(microseconds=t // 10)


def plausible(t):
    if t <= 0:
        return False
    try:
        return PLAUSIBLE[0] <= ticks_to_dt(t) < PLAUSIBLE[1]
    except OverflowError:
        return False


def expected_sample(x, y, f, bits):
    """The pixel formula, restated from write_recordings.cpp's comment."""
    n = x * 7 + y * 13 + f * 29
    return n & 0xFF if bits == 8 else (n * 257 + f) & 0xFFFF


class Verdict:
    def __init__(self, path):
        self.file = os.path.basename(path)
        self.problems = []
        self.notes = []

    def bad(self, msg):
        self.problems.append(msg)

    def note(self, msg):
        self.notes.append(msg)

    @property
    def ok(self):
        return not self.problems

    def as_dict(self):
        return {"file": self.file, "ok": self.ok, "problems": self.problems, "notes": self.notes}


def read_ser(path, v, want=None):
    """Reads one SER. `want` is its manifest entry, or None in --scan mode."""
    size = os.path.getsize(path)
    if size < SER_HEADER:
        v.bad(f"{size} bytes, shorter than the {SER_HEADER}-byte header")
        return
    with open(path, "rb") as fh:
        hdr = fh.read(SER_HEADER)
        (file_id, _lu, colour_id, le_flag, w, h, depth, count) = struct.unpack_from("<14s7i", hdr, 0)
        dt_local, dt_utc = struct.unpack_from("<qq", hdr, 162)

        if file_id != b"LUCAM-RECORDER":
            v.bad(f"FileID is {file_id!r}, not LUCAM-RECORDER")
            return
        if colour_id not in SER_COLOUR:
            v.bad(f"ColorID {colour_id} is not in the specification")
            return
        if not (1 <= depth <= 16):
            v.bad(f"PixelDepthPerPlane {depth} is outside 1..16")
            return
        if w <= 0 or h <= 0 or count < 0:
            v.bad(f"size {w}x{h}, FrameCount {count}")
            return
        planes = 3 if colour_id >= 100 else 1
        bpp = planes * (1 if depth <= 8 else 2)
        frame_bytes = w * h * bpp
        v.note(f"{w}x{h} {depth}-bit {SER_COLOUR[colour_id]}, {count} frames, LittleEndian={le_flag}")

        if not plausible(dt_utc):
            v.bad(f"header DateTime_UTC {dt_utc} is not a date")

        body = size - SER_HEADER
        image = count * frame_bytes
        trailer_bytes = body - image
        complete = want["complete"] if want else None

        if trailer_bytes < 0:
            v.bad(f"FrameCount {count} needs {image} bytes of image, the file holds {body}")
            return
        if trailer_bytes == 0:
            have_trailer = False
            if complete:
                v.bad("no timestamp trailer on a closed recording")
            elif want is None and count > 0:
                v.note("no trailer: a run that was not closed, or a writer that keeps none")
        elif trailer_bytes == 8 * count:
            have_trailer = True
            if complete is False:
                v.bad("a trailer on a recording that was never closed")
        elif complete is False and trailer_bytes < frame_bytes:
            # A crash between the count's patch and the next frame's bytes.
            have_trailer = False
            v.note(f"{trailer_bytes} bytes of a frame the count does not include")
        else:
            v.bad(f"{trailer_bytes} bytes after {count} frames: neither nothing nor an "
                  f"{8 * count}-byte trailer")
            return

        if want:
            for key, got in (("width", w), ("height", h), ("bits", depth), ("frames", count)):
                if want[key] != got:
                    v.bad(f"{key} is {got}, the writer was given {want[key]}")
            if SER_COLOUR[colour_id] != want["colour"]:
                v.bad(f"ColorID says {SER_COLOUR[colour_id]}, the writer was given {want['colour']}")
            if not v.ok:
                return
            check_ser_pixels(fh, v, w, h, depth, count, frame_bytes)

        if have_trailer and count:
            fh.seek(SER_HEADER + image)
            stamps = struct.unpack(f"<{count}q", fh.read(8 * count))
            check_ser_stamps(v, stamps, dt_utc, want)


def check_ser_pixels(fh, v, w, h, depth, count, frame_bytes):
    fh.seek(SER_HEADER)
    wrong = 0
    first = None
    for f in range(count):
        raw = fh.read(frame_bytes)
        if depth <= 8:
            samples = raw
        else:
            samples = struct.unpack(f"<{w * h}H", raw)
        i = 0
        for y in range(h):
            for x in range(w):
                if samples[i] != expected_sample(x, y, f, depth):
                    wrong += 1
                    if first is None:
                        first = (f, x, y, samples[i], expected_sample(x, y, f, depth))
                i += 1
    if wrong:
        f, x, y, got, exp = first
        v.bad(f"{wrong} pixels wrong; first at frame {f} ({x},{y}): {got}, expected {exp}")
    else:
        v.note(f"all {count * w * h} pixels as written")


def check_ser_stamps(v, stamps, header_utc, want):
    bad_dates = [i for i, t in enumerate(stamps) if not plausible(t)]
    if bad_dates:
        v.bad(f"{len(bad_dates)} trailer stamps are not dates (first: frame {bad_dates[0]})")
        return
    back = [i for i in range(1, len(stamps)) if stamps[i] <= stamps[i - 1]]
    if back:
        i = back[0]
        v.bad(f"{len(back)} stamps not after the one before; first at frame {i} "
              f"({stamps[i - 1]} then {stamps[i]})")
    # The header is stamped at Open, before any frame: a first frame more
    # than a minute before it means one of the two clocks is wrong.
    if stamps[0] < header_utc - 60 * TICKS_PER_SEC:
        v.bad(f"first frame {ticks_to_dt(stamps[0])} is before the file was opened "
              f"({ticks_to_dt(header_utc)})")
    if want:
        step = want["interval_us"] * 10
        off = [i for i in range(1, len(stamps)) if abs(stamps[i] - stamps[i - 1] - step) > 1]
        if off:
            i = off[0]
            v.bad(f"{len(off)} intervals not {want['interval_us']} us; first at frame {i}: "
                  f"{(stamps[i] - stamps[i - 1]) / 10:.1f} us")
    span = (stamps[-1] - stamps[0]) / TICKS_PER_SEC
    v.note(f"{len(stamps)} stamps, increasing, {span:.3f} s first to last, "
           f"from {ticks_to_dt(stamps[0]).isoformat()}")


# ── FITS, through astropy ───────────────────────────────────────────────

def read_fits(path, v, want=None):
    try:
        import numpy as np
        from astropy.io import fits
        from astropy.time import Time
    except ImportError as e:
        v.bad(f"astropy is not installed ({e}); pip install astropy")
        return

    try:
        with fits.open(path, mode="readonly", memmap=False) as hdul:
            hdul.verify("exception")
            prim = hdul[0]
            hd = prim.header
            data = prim.data
            if hd.get("NAXIS") != 3 or data is None or data.ndim != 3:
                v.bad(f"primary HDU is not a cube (NAXIS={hd.get('NAXIS')})")
                return
            n, h, w = data.shape
            bits = 8 if hd["BITPIX"] == 8 else 16
            v.note(f"{w}x{h} BITPIX {hd['BITPIX']} BZERO {hd.get('BZERO', 0)}, {n} frames, "
                   f"dtype {data.dtype}")
            if hd["BITPIX"] == 16 and data.dtype != np.uint16:
                v.bad(f"16-bit data reads as {data.dtype}, not uint16: BZERO/BSCALE wrong")

            # The time cards.
            if n:
                if hd.get("TIMESYS") != "UTC":
                    v.bad(f"TIMESYS is {hd.get('TIMESYS')!r}, not UTC")
                times = {}
                for key in ("DATE-OBS", "DATE-AVG", "DATE-END"):
                    if key not in hd:
                        if key == "DATE-AVG" or "EXPTIME" in hd:
                            v.bad(f"{key} missing")
                        continue
                    try:
                        times[key] = Time(hd[key], format="isot", scale="utc")
                    except ValueError as e:
                        v.bad(f"{key} {hd[key]!r} does not parse: {e}")
                if {"DATE-OBS", "DATE-AVG", "DATE-END"} <= times.keys():
                    a, m, b = times["DATE-OBS"], times["DATE-AVG"], times["DATE-END"]
                    if not (a < m < b) and n > 1:
                        v.bad(f"DATE-OBS {a.isot}, DATE-AVG {m.isot}, DATE-END {b.isot} out of order")
                for key, t in times.items():
                    if not (2000 <= t.datetime.year < 2100):
                        v.bad(f"{key} {t.isot} is not a plausible date")

            # The FRAMES table.
            table = None
            for hdu in hdul[1:]:
                if hdu.header.get("EXTNAME") == "FRAMES":
                    table = hdu
            if table is None:
                v.bad("no FRAMES table")
                mjd = None
            else:
                mjd = np.asarray(table.data["MJD_MID"], dtype=np.float64)
                if len(mjd) != n:
                    v.bad(f"FRAMES has {len(mjd)} rows for {n} frames")
                elif n > 1 and not np.all(np.diff(mjd) > 0):
                    i = int(np.argmax(np.diff(mjd) <= 0)) + 1
                    v.bad(f"MJD_MID not increasing at frame {i}")
                else:
                    v.note(f"FRAMES: {len(mjd)} rows, MJD_MID increasing")

            if want:
                check_fits_against(v, hd, data, mjd, times if n else {}, want, w, h, n, bits, np)
    except Exception as e:  # astropy raises many kinds; each is a failure here
        v.bad(f"astropy could not read it: {type(e).__name__}: {e}")


def check_fits_against(v, hd, data, mjd, times, want, w, h, n, bits, np):
    for key, got in (("width", w), ("height", h), ("bits", bits), ("frames", n)):
        if want[key] != got:
            v.bad(f"{key} is {got}, the writer was given {want[key]}")
    pat = hd.get("BAYERPAT")
    if want["colour"] == "MONO" and pat is not None:
        v.bad(f"BAYERPAT {pat!r} on a mono file")
    if want["colour"] != "MONO" and pat != want["colour"]:
        v.bad(f"BAYERPAT is {pat!r}, the writer was given {want['colour']}")
    if not v.ok:
        return

    f, y, x = np.meshgrid(np.arange(n), np.arange(h), np.arange(w), indexing="ij")
    nn = x * 7 + y * 13 + f * 29
    exp = (nn & 0xFF) if bits == 8 else ((nn * 257 + f) & 0xFFFF)
    wrong = int(np.count_nonzero(data.astype(np.int64) != exp))
    if wrong:
        idx = tuple(int(i[0]) for i in np.nonzero(data.astype(np.int64) != exp))
        v.bad(f"{wrong} pixels wrong; first at frame {idx[0]} ({idx[2]},{idx[1]}): "
              f"{int(data[idx])}, expected {int(exp[idx])}")
    else:
        v.note(f"all {data.size} pixels as written")

    step_d = want["interval_us"] / 86400e6
    if mjd is not None and len(mjd) == n and n > 1:
        err_us = np.abs(np.diff(mjd) - step_d) * 86400e6
        # A double MJD near 60000 resolves about 1 us.
        if err_us.max() > 5:
            v.bad(f"MJD_MID intervals off by up to {err_us.max():.1f} us from {want['interval_us']} us")
    if {"DATE-OBS", "DATE-END"} <= times.keys():
        span_s = (times["DATE-END"] - times["DATE-OBS"]).sec
        exp_s = (n - 1) * want["interval_us"] / 1e6 + want["exposure_ms"] / 1e3
        if abs(span_s - exp_s) > 0.002:
            v.bad(f"DATE-END - DATE-OBS is {span_s:.4f} s, expected {exp_s:.4f} s")
        if mjd is not None and len(mjd):
            first_mid = times["DATE-OBS"].mjd + want["exposure_ms"] / 2e3 / 86400
            if abs(mjd[0] - first_mid) * 86400 > 0.002:
                v.bad("MJD_MID[0] is not DATE-OBS plus half an exposure")
    exptime = hd.get("EXPTIME")
    if exptime is None or abs(exptime - want["exposure_ms"] / 1000) > 1e-6:
        v.bad(f"EXPTIME is {exptime}, the writer was given {want['exposure_ms'] / 1000} s")


# ── Driving it ──────────────────────────────────────────────────────────

def check_file(path, want=None):
    v = Verdict(path)
    ext = os.path.splitext(path)[1].lower()
    if ext == ".ser":
        read_ser(path, v, want)
    elif ext in (".fits", ".fit", ".fts"):
        read_fits(path, v, want)
    else:
        v.bad(f"unknown extension {ext}")
    return v


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    g = ap.add_mutually_exclusive_group(required=True)
    g.add_argument("--manifest", metavar="DIR", help="check write_recordings' files against its manifest.json")
    g.add_argument("--scan", metavar="DIR", help="check every .ser/.fits under DIR")
    ap.add_argument("--json", metavar="PATH", help="write the verdicts here")
    args = ap.parse_args()

    verdicts = []
    if args.manifest:
        with open(os.path.join(args.manifest, "manifest.json"), encoding="utf-8") as fh:
            manifest = json.load(fh)
        for want in manifest["files"]:
            path = os.path.join(args.manifest, want["file"])
            if not os.path.exists(path):
                v = Verdict(path)
                v.bad("missing")
                verdicts.append(v)
                continue
            verdicts.append(check_file(path, want))
    else:
        for dirpath, _dirs, files in os.walk(args.scan):
            for name in sorted(files):
                if os.path.splitext(name)[1].lower() in (".ser", ".fits", ".fit", ".fts"):
                    verdicts.append(check_file(os.path.join(dirpath, name)))

    failed = [v for v in verdicts if not v.ok]
    for v in verdicts:
        print(f"{'ok  ' if v.ok else 'FAIL'}  {v.file}")
        for p in v.problems:
            print(f"        problem: {p}")
        for n in v.notes:
            print(f"        {n}")
    print(f"{len(verdicts) - len(failed)} of {len(verdicts)} files pass")
    if not verdicts:
        print("no recordings found")
    if args.json:
        with open(args.json, "w", encoding="utf-8") as fh:
            json.dump({"files": [v.as_dict() for v in verdicts],
                       "passed": len(verdicts) - len(failed), "failed": len(failed)}, fh, indent=2)
    return 1 if failed or (args.manifest and not verdicts) else 0


if __name__ == "__main__":
    sys.exit(main())
