// ============================================================
//  profile_per_target -- a profile for what the scope is on (2026-11-15)
//
//  A GoTo to Vega found no "Vega" profile, kept Jupiter's, and the status
//  bar said [Jupiter] with the scope on Vega; the note on the OUTPUT tab
//  was Jupiter's too. Now a target without a profile gets one. Pinned:
//    - CreateFor makes, saves and adds a profile with the target's name,
//      its own file and prefix, an empty note, the seed's settings and
//      the camera it was given;
//    - it never writes over another profile's file;
//    - LoadAll brings created profiles back (and skips backups such as
//      saturn.ini.working-2026-10-08);
//    - FindForTarget prefers the connected camera's profile, then one for
//      any camera.
//  Runs in a scratch folder under %TEMP%, never the real profiles.
// ============================================================

#include "profiles/ProfileManager.h"

#include <cstdio>
#include <string>

using namespace raduk::profiles;

static int g_fail = 0;
static void Check(bool ok, const std::string& what) {
    printf("  [%s] %s\n", ok ? "PASS" : "FAIL", what.c_str());
    if (!ok) g_fail++;
}

static void Wipe(const std::string& dir) {
    WIN32_FIND_DATAA fd{};
    HANDLE h = FindFirstFileA((dir + "\\*").c_str(), &fd);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
                DeleteFileA((dir + "\\" + fd.cFileName).c_str());
        } while (FindNextFileA(h, &fd));
        FindClose(h);
    }
    RemoveDirectoryA(dir.c_str());
}

int main() {
    printf("\n=== A profile per target ===\n\n");

    char tmp[MAX_PATH];
    GetTempPathA(MAX_PATH, tmp);
    const std::string dir = std::string(tmp) + "raduk_profile_per_target";
    Wipe(dir);

    ProfileManager pm;
    pm.Init(dir);
    const int standard = (int)pm.Profiles().size();
    Check(standard == 11, "the eleven standard profiles");

    Check(ProfileManager::FileNameFor("Vega") == "vega.ini", "Vega -> vega.ini");
    Check(ProfileManager::FileNameFor("M 31") == "m_31.ini", "M 31 -> m_31.ini");
    Check(ProfileManager::FileNameFor("Alpha Lyr / Vega") == "alpha_lyr_vega.ini",
          "punctuation folds to single underscores");
    Check(ProfileManager::FileNameFor("***") == "target.ini", "a name with no letters still gets a file");

    Check(pm.FindForTarget("Vega", "ZWO ASI678MC") < 0, "no Vega profile to start with");

    TargetProfile seed = *pm.Find("Star");
    const int vi = pm.CreateFor("Vega", seed, "ZWO ASI678MC");
    const TargetProfile& v = pm.Profiles()[vi];
    Check(v.name == "Vega", "created with the target's name");
    Check(v.filename == "vega.ini", "in its own file");
    Check(v.filePrefix == "vega_", "with its own file prefix");
    Check(v.notes.empty(), "with an empty note: the note is for this object");
    Check(v.gain == seed.gain && v.exposureMs == seed.exposureMs && v.targetType == "star",
          "with the Star profile's settings");
    Check(v.camera == "ZWO ASI678MC", "stamped with the connected camera");
    Check(GetFileAttributesA((dir + "\\vega.ini").c_str()) != INVALID_FILE_ATTRIBUTES,
          "saved to disk");
    Check(pm.FindForTarget("vega", "ZWO ASI678MC") == vi, "found again, case-insensitive");

    // A target whose file name would be a standard profile's.
    const int si = pm.CreateFor("Star!", seed, "");
    Check(pm.Profiles()[si].filename == "star_2.ini", "never over another profile's file");

    // A backup in the folder is not a profile.
    {
        FILE* f = nullptr;
        fopen_s(&f, (dir + "\\saturn.ini.working-2026-10-08").c_str(), "w");
        if (f) { fputs("[profile]\nname=Old Saturn\n", f); fclose(f); }
    }

    // A second camera's Vega.
    TargetProfile other = seed;
    other.gain = 123;
    const int oi = pm.CreateFor("Vega", other, "QHY5III715C");
    Check(pm.Profiles()[oi].filename == "vega_2.ini", "a second Vega gets its own file");

    // Reload from disk.
    ProfileManager pm2;
    pm2.Init(dir);
    Check((int)pm2.Profiles().size() == standard + 3, "LoadAll brings back the three created profiles");
    Check(pm2.Find("Old Saturn") == nullptr, "and skips the backup");
    const int a = pm2.FindForTarget("Vega", "ZWO ASI678MC");
    const int b = pm2.FindForTarget("Vega", "QHY5III715C");
    Check(a >= 0 && pm2.Profiles()[a].camera == "ZWO ASI678MC", "the ZWO camera gets the ZWO Vega");
    Check(b >= 0 && pm2.Profiles()[b].gain == 123, "the QHY camera gets the QHY Vega");
    Check(pm2.Profiles()[a].targetType == "star", "the target type survives the round trip");
    Check(pm2.Profiles()[a].notes.empty(), "and the empty note stays empty");
    // Neither camera named: one for any camera first, else the first.
    const int c = pm2.FindForTarget("Vega", "Some Other Cam");
    Check(c >= 0 && pm2.Profiles()[c].name == "Vega", "an unknown camera still gets a Vega");

    // A standard profile without a camera key keeps "any camera".
    Check(pm2.Find("Jupiter") && pm2.Find("Jupiter")->camera.empty(),
          "a profile that names no camera is for any camera");
    Check(pm2.FindForTarget("Jupiter", "ZWO ASI678MC") >= 0, "and is found for the connected one");

    Wipe(dir);
    printf("\n%s\n", g_fail ? "SOME CHECKS FAILED" : "ALL CHECKS PASSED");
    return g_fail ? 1 : 0;
}
