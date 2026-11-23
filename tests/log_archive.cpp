// log_archive: every run's log kept by date (2026-11-23: the night of the
// 22nd was gone from both raduk_log.txt and .prev.txt after a relaunch).
#include "core/LogArchive.h"
#include <cstdio>

using namespace raduk::core::logarchive;
static int fails = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); ++fails; } } while (0)

int main() {
    CHECK(ArchiveName(2026, 11, 22, 3, 10, 5) == "raduk_log_20261122-031005.txt");
    CHECK(IsArchiveName("raduk_log_20261122-031005.txt"));
    CHECK(!IsArchiveName("raduk_log.prev.txt"));
    CHECK(!IsArchiveName("raduk_log_2026112-031005.txt"));
    CHECK(!IsArchiveName("raduk_log_20261122_031005.txt"));
    CHECK(!IsArchiveName("raduk_log_20261122-031005.log"));
    CHECK(!IsArchiveName("focus_history.jsonl"));

    // 35 runs over five nights: the oldest 5 go, nothing else is touched.
    std::vector<std::string> names;
    for (int d = 18; d <= 22; ++d)
        for (int r = 0; r < 7; ++r) names.push_back(ArchiveName(2026, 11, d, 18 + r, 0, 0));
    names.push_back("notes.txt");
    names.push_back("raduk_log.prev.txt");
    const auto del = ToDelete(names, 30);
    CHECK(del.size() == 5);
    for (const auto& n : del) CHECK(n.rfind("raduk_log_20261118-", 0) == 0);
    for (const auto& n : del) CHECK(n != "notes.txt" && n != "raduk_log.prev.txt");
    // Sorted by time even across a month and a year.
    const auto y = ToDelete({ArchiveName(2027, 1, 1, 0, 0, 0), ArchiveName(2026, 12, 31, 23, 59, 59),
                             ArchiveName(2026, 2, 1, 0, 0, 0)}, 1);
    CHECK(y.size() == 2 && y[0] == ArchiveName(2026, 2, 1, 0, 0, 0) && y[1] == ArchiveName(2026, 12, 31, 23, 59, 59));
    // Under the limit, or a nonsense limit: nothing beyond what it must.
    CHECK(ToDelete(names, 100).empty());
    CHECK(ToDelete(names, -3).size() == 35);

    std::printf("log_archive: %s (%d failures)\n", fails ? "FAIL" : "PASS", fails);
    return fails ? 1 : 0;
}
