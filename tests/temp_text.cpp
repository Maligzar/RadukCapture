// temp_text: temperatures shown in both units (2026-11-24).
#include "core/TempText.h"
#include <cstdio>

using namespace raduk::core;
static int fails = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); ++fails; } } while (0)

int main() {
    CHECK(std::fabs(CToF(0) - 32) < 1e-9 && std::fabs(CToF(100) - 212) < 1e-9 && std::fabs(CToF(-40) + 40) < 1e-9);
    CHECK(std::fabs(FToC(62.6) - 17.0) < 1e-9);
    CHECK(TempCF(17.0) == L"17.0°C / 62.6°F");
    CHECK(TempCF(-5.25, 0) == L"-5°C / 23°F");
    CHECK(TempCFShort(24.8) == L"24.8°C 76.6°F");
    CHECK(TempCF(NAN) == L"--" && TempCFShort(INFINITY) == L"--");
    std::printf("temp_text: %s (%d failures)\n", fails ? "FAIL" : "PASS", fails);
    return fails ? 1 : 0;
}
