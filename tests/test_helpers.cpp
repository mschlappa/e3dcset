#include "catch.hpp"
#include <cstring>
#include <cstdio>
#include <map>
#include <string>

// ============================================================================
// Pure functions extracted from e3dcset.cpp for unit testing
// Keep in sync with e3dcset.cpp!
// ============================================================================

// --- getDaysInMonth (exact copy from e3dcset.cpp) ---
static int getDaysInMonth(int month, int year) {
    int daysInMonth[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (month < 1 || month > 12) return 31;
    int days = daysInMonth[month - 1];
    if (month == 2) {
        if ((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0)) {
            days = 29;
        }
    }
    return days;
}

// --- isRequestTag (exact copy from e3dcset.cpp) ---
static bool isRequestTag(uint32_t tag) {
    uint8_t secondByte = (tag >> 16) & 0xFF;
    return secondByte < 0x80;
}

// --- interpretValue (adapted: uses passed map instead of global) ---
static const char* interpretValue(uint32_t tag, int64_t value,
                                  const std::map<std::string, std::string>& interpretations) {
    char key[64];
    snprintf(key, sizeof(key), "0x%08X:%lld", tag, (long long)value);
    auto it = interpretations.find(std::string(key));
    if (it != interpretations.end()) {
        return it->second.c_str();
    }
    uint8_t secondByte = (tag >> 16) & 0xFF;
    if (secondByte >= 0x80) {
        uint32_t requestTag = (tag & 0xFF00FFFF) | (((secondByte & 0x7F) << 16));
        snprintf(key, sizeof(key), "0x%08X:%lld", requestTag, (long long)value);
        it = interpretations.find(std::string(key));
        if (it != interpretations.end()) {
            return it->second.c_str();
        }
    }
    return NULL;
}

// ============================================================================
// getDaysInMonth Tests
// ============================================================================

TEST_CASE("getDaysInMonth - all months in non-leap year", "[helpers]") {
    int expected[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    for (int m = 1; m <= 12; m++) {
        REQUIRE(getDaysInMonth(m, 2023) == expected[m-1]);
    }
}

TEST_CASE("getDaysInMonth - all months in leap year", "[helpers]") {
    int expected[] = {31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    for (int m = 1; m <= 12; m++) {
        REQUIRE(getDaysInMonth(m, 2024) == expected[m-1]);
    }
}

TEST_CASE("getDaysInMonth - leap year rules for February", "[helpers]") {
    REQUIRE(getDaysInMonth(2, 2024) == 29);  // divisible by 4
    REQUIRE(getDaysInMonth(2, 2023) == 28);  // not divisible by 4
    REQUIRE(getDaysInMonth(2, 1900) == 28);  // divisible by 100, not 400
    REQUIRE(getDaysInMonth(2, 2000) == 29);  // divisible by 400
    REQUIRE(getDaysInMonth(2, 2100) == 28);  // divisible by 100, not 400
    REQUIRE(getDaysInMonth(2, 2400) == 29);  // divisible by 400
}

TEST_CASE("getDaysInMonth - invalid months return 31", "[helpers]") {
    REQUIRE(getDaysInMonth(0, 2024) == 31);
    REQUIRE(getDaysInMonth(13, 2024) == 31);
    REQUIRE(getDaysInMonth(-1, 2024) == 31);
    REQUIRE(getDaysInMonth(100, 2024) == 31);
}

// ============================================================================
// isRequestTag Tests
// ============================================================================

TEST_CASE("isRequestTag - request tags return true", "[helpers]") {
    REQUIRE(isRequestTag(0x01000001) == true);   // EMS_REQ_POWER_PV
    REQUIRE(isRequestTag(0x01000008) == true);   // EMS_REQ_BAT_SOC
    REQUIRE(isRequestTag(0x03000001) == true);   // BAT_REQ_*
    REQUIRE(isRequestTag(0x03040000) == true);   // BAT_REQ_DCB_*
    REQUIRE(isRequestTag(0x00000000) == true);   // Zero tag
}

TEST_CASE("isRequestTag - response tags return false", "[helpers]") {
    REQUIRE(isRequestTag(0x01800001) == false);  // EMS response
    REQUIRE(isRequestTag(0x03800001) == false);  // BAT response
    REQUIRE(isRequestTag(0x01FF0000) == false);  // Max response byte
}

TEST_CASE("isRequestTag - boundary at 0x80", "[helpers]") {
    REQUIRE(isRequestTag(0x007F0000) == true);   // 0x7F → request
    REQUIRE(isRequestTag(0x00800000) == false);  // 0x80 → response
}

// ============================================================================
// interpretValue Tests
// ============================================================================

TEST_CASE("interpretValue - exact match found", "[helpers]") {
    std::map<std::string, std::string> interp;
    interp["0x01000009:2"] = "AC-gekoppelt";

    REQUIRE(std::string(interpretValue(0x01000009, 2, interp)) == "AC-gekoppelt");
}

TEST_CASE("interpretValue - no match returns NULL", "[helpers]") {
    std::map<std::string, std::string> interp;
    interp["0x01000009:2"] = "AC-gekoppelt";

    REQUIRE(interpretValue(0x01000009, 3, interp) == NULL);
    REQUIRE(interpretValue(0x01000008, 2, interp) == NULL);
}

TEST_CASE("interpretValue - response tag falls back to request tag", "[helpers]") {
    std::map<std::string, std::string> interp;
    interp["0x01000009:2"] = "AC-gekoppelt";

    // Response 0x01800009 → strips bit 23 → finds 0x01000009
    const char* result = interpretValue(0x01800009, 2, interp);
    REQUIRE(result != NULL);
    REQUIRE(std::string(result) == "AC-gekoppelt");
}

TEST_CASE("interpretValue - empty map returns NULL", "[helpers]") {
    std::map<std::string, std::string> empty;
    REQUIRE(interpretValue(0x01000009, 2, empty) == NULL);
}

TEST_CASE("interpretValue - negative values", "[helpers]") {
    std::map<std::string, std::string> interp;
    interp["0x01000001:-1"] = "Fehler";

    REQUIRE(std::string(interpretValue(0x01000001, -1, interp)) == "Fehler");
}

TEST_CASE("interpretValue - zero value", "[helpers]") {
    std::map<std::string, std::string> interp;
    interp["0x01000001:0"] = "Aus";

    REQUIRE(std::string(interpretValue(0x01000001, 0, interp)) == "Aus");
}

// ============================================================================
// JsonModuleDumpBuffer tests (CLI-based to avoid link dependency)
// ============================================================================

// These tests verify JSON module dump output via the CLI test script
// (see tests/test_json_module_dump.sh)
