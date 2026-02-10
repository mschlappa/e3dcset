/*
 * Minimal C Test Framework
 * No external dependencies
 */

#ifndef TEST_FRAMEWORK_H
#define TEST_FRAMEWORK_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int test_count = 0;
static int test_passed = 0;
static int test_failed = 0;

#define TEST(name) \
    static void test_##name(void); \
    static void run_test_##name(void) { \
        test_count++; \
        printf("  Running: " #name "... "); \
        fflush(stdout); \
        test_##name(); \
    } \
    static void test_##name(void)

#define ASSERT(condition, message) \
    do { \
        if (!(condition)) { \
            printf("FAIL\n    %s:%d: %s\n", __FILE__, __LINE__, message); \
            test_failed++; \
            return; \
        } \
    } while(0)

#define ASSERT_EQ(expected, actual, message) \
    do { \
        if ((expected) != (actual)) { \
            printf("FAIL\n    %s:%d: %s (expected: %d, got: %d)\n", \
                   __FILE__, __LINE__, message, (int)(expected), (int)(actual)); \
            test_failed++; \
            return; \
        } \
    } while(0)

#define ASSERT_STR_EQ(expected, actual, message) \
    do { \
        if (strcmp((expected), (actual)) != 0) { \
            printf("FAIL\n    %s:%d: %s (expected: \"%s\", got: \"%s\")\n", \
                   __FILE__, __LINE__, message, (expected), (actual)); \
            test_failed++; \
            return; \
        } \
    } while(0)

#define PASS() \
    do { \
        printf("PASS\n"); \
        test_passed++; \
    } while(0)

#define RUN_TEST(name) run_test_##name()

#define TEST_SUMMARY() \
    do { \
        printf("\n=== Test Summary ===\n"); \
        printf("Total:  %d\n", test_count); \
        printf("Passed: %d\n", test_passed); \
        printf("Failed: %d\n", test_failed); \
        if (test_failed == 0) { \
            printf("\nAll tests passed! ✓\n"); \
        } else { \
            printf("\nSome tests failed! ✗\n"); \
        } \
        return (test_failed == 0) ? 0 : 1; \
    } while(0)

#endif /* TEST_FRAMEWORK_H */
