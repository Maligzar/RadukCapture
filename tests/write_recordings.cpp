// ============================================================
//  Phase 157.2: recordings for an independent reader to check
//
//  Writes a small set of SER and FITS files through the app's own
//  writers, with pixels from a formula and timestamps on a fixed
//  interval, and a manifest.json that says what each file should hold.
//  tools/check_recordings.py then reads them WITHOUT this code: SER
//  with a reader written from the SER v3 specification, FITS with
//  astropy. A file that only our own reader can read fails there.
//
//      write_recordings.exe <out dir>
//
//  The pixel formula is the contract between the two halves. It is
//  restated in check_recordings.py, never shared:
//
//      n       = x*7 + y*13 + f*29          (x, y in px; f = frame index)
//      8-bit:  n & 0xFF
//      16-bit: (n*257 + f) & 0xFFFF         (reaches the top bit, so a
//                                            FITS BZERO slip shows)
//
//  The last file is a crash: a SER whose writer is never closed, the
//  process ended by TerminateProcess mid-run. Its frames are 1.1 s
//  apart on the frame clock, so every frame patches the header's count
//  (phase 135.2), and the reader must find exactly that count of whole
//  frames and no trailer.
// ============================================================

#include "core/FITSWriter.h"
#include "core/SERWriter.h"

#include <cstdint>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

using namespace raduk::core;

namespace {

struct Case {
    const char* file;
    bool        fits;
    int         width, height, bits;
    bool        bayer;
    int         frames;
    uint64_t    intervalUs;
    double      exposureMs;
    bool        complete;   // false: the crash, never closed
};

// Odd sizes on purpose: a reader that assumes even rows, or a width
// that is a multiple of 4, gets them wrong.
const Case kCases[] = {
    { "mono8.ser",   false, 67, 41, 8,  false, 12, 10000, 5.0, true },
    { "mono16.ser",  false, 53, 37, 16, false, 9,  12500, 8.0, true },
    { "rggb8.ser",   false, 64, 48, 8,  true,  7,  20000, 15.0, true },
    { "rggb16.ser",  false, 32, 24, 16, true,  5,  33333, 30.0, true },
    { "mono8.fits",  true,  67, 41, 8,  false, 12, 10000, 5.0, true },
    { "mono16.fits", true,  53, 37, 16, false, 9,  12500, 8.0, true },
    { "rggb16.fits", true,  32, 24, 16, true,  5,  33333, 30.0, true },
};

const Case kCrash = { "crash.ser", false, 40, 30, 16, false, 6, 1100000, 1.0, false };

std::vector<uint8_t> Frame(const Case& c, int f) {
    const size_t bpp = c.bits == 16 ? 2 : 1;
    std::vector<uint8_t> out((size_t)c.width * c.height * bpp);
    size_t i = 0;
    for (int y = 0; y < c.height; ++y) {
        for (int x = 0; x < c.width; ++x) {
            const uint32_t n = (uint32_t)(x * 7 + y * 13 + f * 29);
            if (c.bits == 8) {
                out[i++] = (uint8_t)(n & 0xFF);
            } else {
                const uint16_t v = (uint16_t)((n * 257u + (uint32_t)f) & 0xFFFF);
                out[i++] = (uint8_t)(v & 0xFF);       // little-endian, as
                out[i++] = (uint8_t)(v >> 8);         // the camera delivers
            }
        }
    }
    return out;
}

std::unique_ptr<ICaptureWriter> WriterFor(const Case& c) {
    if (c.fits) return std::make_unique<FITSWriter>();
    return std::make_unique<SERWriter>();
}

bool WriteFrames(ICaptureWriter& w, const Case& c) {
    const uint64_t t0 = Timestamp::Now();
    for (int f = 0; f < c.frames; ++f) {
        const std::vector<uint8_t> px = Frame(c, f);
        if (!w.WriteFrame(px.data(), px.size(), t0 + (uint64_t)f * c.intervalUs,
                          (float)(10 + f))) {
            printf("  FAIL  %s: frame %d refused\n", c.file, f);
            return false;
        }
    }
    return true;
}

bool OpenFor(ICaptureWriter& w, const Case& c, const std::string& path) {
    if (!w.Open(path, c.width, c.height, c.bits, c.bayer)) {
        printf("  FAIL  %s: did not open\n", c.file);
        return false;
    }
    ObservationRecord r;
    r.file       = c.file;
    r.target     = "Test pattern";
    r.frameType  = "Light";
    r.appVersion = "write_recordings";
    r.width      = c.width;
    r.height     = c.height;
    r.bitDepth   = c.bits;
    r.colour     = c.bayer ? "RGGB" : "MONO";
    r.exposureMs = c.exposureMs;
    r.gain       = 100;
    w.NoteObservation(r);
    return true;
}

void ManifestEntry(FILE* m, const Case& c, bool last) {
    fprintf(m,
            "    {\"file\": \"%s\", \"format\": \"%s\", \"width\": %d, \"height\": %d, "
            "\"bits\": %d, \"colour\": \"%s\", \"frames\": %d, \"interval_us\": %llu, "
            "\"exposure_ms\": %.3f, \"complete\": %s}%s\n",
            c.file, c.fits ? "fits" : "ser", c.width, c.height, c.bits,
            c.bayer ? "RGGB" : "MONO", c.frames, (unsigned long long)c.intervalUs,
            c.exposureMs, c.complete ? "true" : "false", last ? "" : ",");
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        printf("usage: write_recordings <out dir>\n");
        return 2;
    }
    std::string dir = argv[1];
    if (!dir.empty() && dir.back() != '\\' && dir.back() != '/') dir += '\\';
    CreateDirectoryA(dir.c_str(), nullptr);

    printf("Writing test recordings to %s\n", dir.c_str());
    int fail = 0;
    for (const Case& c : kCases) {
        std::unique_ptr<ICaptureWriter> w = WriterFor(c);
        if (!OpenFor(*w, c, dir + c.file) || !WriteFrames(*w, c)) { ++fail; continue; }
        if (!w->Close()) { printf("  FAIL  %s: close\n", c.file); ++fail; continue; }
        printf("  wrote %s: %dx%d %d-bit %s, %d frames\n", c.file, c.width, c.height,
               c.bits, c.bayer ? "RGGB" : "mono", c.frames);
    }

    // The manifest is closed before the crash, which ends the process.
    const std::string mpath = dir + "manifest.json";
    FILE* m = nullptr;
    if (fopen_s(&m, mpath.c_str(), "wb") != 0 || !m) {
        printf("  FAIL  cannot write %s\n", mpath.c_str());
        return 1;
    }
    fprintf(m, "{\n  \"pixel_formula\": \"n = x*7 + y*13 + f*29; 8-bit n&0xFF; 16-bit (n*257+f)&0xFFFF\",\n");
    fprintf(m, "  \"files\": [\n");
    for (const Case& c : kCases) ManifestEntry(m, c, false);
    ManifestEntry(m, kCrash, true);
    fprintf(m, "  ]\n}\n");
    fclose(m);

    if (fail) return 1;

    // ── The crash ──────────────────────────────────────────────
    // Leaked on purpose: no destructor may close it.
    SERWriter* crash = new SERWriter();
    if (!OpenFor(*crash, kCrash, dir + kCrash.file) || !WriteFrames(*crash, kCrash)) return 1;
    printf("  wrote %s and ended the process without closing it\n", kCrash.file);
    fflush(stdout);
    TerminateProcess(GetCurrentProcess(), 0);
    return 0;   // not reached
}
