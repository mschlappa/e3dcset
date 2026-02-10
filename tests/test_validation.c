/*
 * Tests for input validation functions
 */

#include "test_framework.h"
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>

/* Simple date validation function (to be implemented in e3dcset.cpp for Issue #7) */
static bool validate_date_string(const char* dateStr) {
    if (strcmp(dateStr, "today") == 0) {
        return true;
    }
    
    /* Parse YYYY-MM-DD format */
    int year, month, day;
    if (sscanf(dateStr, "%d-%d-%d", &year, &month, &day) != 3) {
        return false;
    }
    
    /* Validate ranges */
    if (year < 1970 || year > 2100) {
        return false;
    }
    
    if (month < 1 || month > 12) {
        return false;
    }
    
    if (day < 1 || day > 31) {
        return false;
    }
    
    /* Month-specific day validation */
    int daysInMonth[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    
    /* Leap year check for February */
    if (month == 2) {
        if ((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0)) {
            daysInMonth[1] = 29;
        }
    }
    
    if (day > daysInMonth[month - 1]) {
        return false;
    }
    
    return true;
}

/* Validate power value within bounds */
static bool validate_power(int power, int min, int max) {
    return (power >= min && power <= max);
}

/* Validate battery charge amount */
static bool validate_charge_amount(int amount, int min, int max) {
    return (amount >= min && amount <= max);
}

TEST(validate_date_today) {
    bool result = validate_date_string("today");
    ASSERT(result == true, "Should accept 'today'");
    PASS();
}

TEST(validate_date_valid) {
    bool result = validate_date_string("2024-11-20");
    ASSERT(result == true, "Should accept valid date");
    PASS();
}

TEST(validate_date_invalid_format) {
    bool result = validate_date_string("20-11-2024");
    ASSERT(result == false, "Should reject invalid date format");
    PASS();
}

TEST(validate_date_invalid_month) {
    bool result = validate_date_string("2024-13-01");
    ASSERT(result == false, "Should reject month > 12");
    PASS();
}

TEST(validate_date_invalid_day) {
    bool result = validate_date_string("2024-11-32");
    ASSERT(result == false, "Should reject day > 31");
    PASS();
}

TEST(validate_date_february_leap_year) {
    bool result = validate_date_string("2024-02-29");
    ASSERT(result == true, "Should accept Feb 29 in leap year");
    PASS();
}

TEST(validate_date_february_non_leap) {
    bool result = validate_date_string("2023-02-29");
    ASSERT(result == false, "Should reject Feb 29 in non-leap year");
    PASS();
}

TEST(validate_date_year_too_old) {
    bool result = validate_date_string("1969-12-31");
    ASSERT(result == false, "Should reject year < 1970");
    PASS();
}

TEST(validate_date_year_too_new) {
    bool result = validate_date_string("2101-01-01");
    ASSERT(result == false, "Should reject year > 2100");
    PASS();
}

TEST(validate_power_valid) {
    bool result = validate_power(2000, 100, 5000);
    ASSERT(result == true, "Should accept valid power value");
    PASS();
}

TEST(validate_power_min_boundary) {
    bool result = validate_power(100, 100, 5000);
    ASSERT(result == true, "Should accept min boundary value");
    PASS();
}

TEST(validate_power_max_boundary) {
    bool result = validate_power(5000, 100, 5000);
    ASSERT(result == true, "Should accept max boundary value");
    PASS();
}

TEST(validate_power_too_low) {
    bool result = validate_power(99, 100, 5000);
    ASSERT(result == false, "Should reject value below min");
    PASS();
}

TEST(validate_power_too_high) {
    bool result = validate_power(5001, 100, 5000);
    ASSERT(result == false, "Should reject value above max");
    PASS();
}

TEST(validate_charge_valid) {
    bool result = validate_charge_amount(5000, 0, 20000);
    ASSERT(result == true, "Should accept valid charge amount");
    PASS();
}

TEST(validate_charge_zero) {
    bool result = validate_charge_amount(0, 0, 20000);
    ASSERT(result == true, "Should accept zero (stop charging)");
    PASS();
}

TEST(validate_charge_negative) {
    bool result = validate_charge_amount(-100, 0, 20000);
    ASSERT(result == false, "Should reject negative charge amount");
    PASS();
}

TEST(validate_charge_too_high) {
    bool result = validate_charge_amount(20001, 0, 20000);
    ASSERT(result == false, "Should reject charge amount above max");
    PASS();
}

int main(void) {
    printf("\n=== Testing Input Validation ===\n");
    
    RUN_TEST(validate_date_today);
    RUN_TEST(validate_date_valid);
    RUN_TEST(validate_date_invalid_format);
    RUN_TEST(validate_date_invalid_month);
    RUN_TEST(validate_date_invalid_day);
    RUN_TEST(validate_date_february_leap_year);
    RUN_TEST(validate_date_february_non_leap);
    RUN_TEST(validate_date_year_too_old);
    RUN_TEST(validate_date_year_too_new);
    
    RUN_TEST(validate_power_valid);
    RUN_TEST(validate_power_min_boundary);
    RUN_TEST(validate_power_max_boundary);
    RUN_TEST(validate_power_too_low);
    RUN_TEST(validate_power_too_high);
    
    RUN_TEST(validate_charge_valid);
    RUN_TEST(validate_charge_zero);
    RUN_TEST(validate_charge_negative);
    RUN_TEST(validate_charge_too_high);
    
    TEST_SUMMARY();
}
