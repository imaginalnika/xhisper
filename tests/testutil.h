/*
 * testutil.h — minimal test helper macros, no external dependencies
 *
 * Usage:
 *   #include "testutil.h"
 *
 *   void test_something(int *failed) {
 *     ASSERT_EQ(expected, actual, "description", failed);
 *   }
 *
 *   int main(void) {
 *     int failed = 0;
 *     TEST_SUITE_BEGIN("my module");
 *     test_something(&failed);
 *     TEST_SUITE_END(failed);
 *   }
 */

#ifndef TESTUTIL_H
#define TESTUTIL_H

#include <stdio.h>
#include <string.h>

#define TEST_SUITE_BEGIN(name) \
    printf("\nRunning %s tests...\n", name); \
    printf("----------------------------------------\n")

#define TEST_SUITE_END(failed) \
    printf("----------------------------------------\n"); \
    if ((failed) == 0) { \
        printf("All tests passed.\n\n"); \
    } else { \
        printf("%d test(s) failed.\n\n", (failed)); \
    } \
    return (failed) > 0 ? 1 : 0

#define ASSERT_TRUE(cond, desc, failed) do { \
    if (cond) { \
        printf("  PASS: %s\n", desc); \
    } else { \
        printf("  FAIL: %s\n", desc); \
        (*failed)++; \
    } \
} while (0)

#define ASSERT_FALSE(cond, desc, failed) \
    ASSERT_TRUE(!(cond), desc, failed)

#define ASSERT_EQ(expected, actual, desc, failed) do { \
    if ((expected) == (actual)) { \
        printf("  PASS: %s\n", desc); \
    } else { \
        printf("  FAIL: %s (expected %d, got %d)\n", desc, (int)(expected), (int)(actual)); \
        (*failed)++; \
    } \
} while (0)

#define ASSERT_STR_EQ(expected, actual, desc, failed) do { \
    if (strcmp((expected), (actual)) == 0) { \
        printf("  PASS: %s\n", desc); \
    } else { \
        printf("  FAIL: %s (expected \"%s\", got \"%s\")\n", desc, expected, actual); \
        (*failed)++; \
    } \
} while (0)

#endif /* TESTUTIL_H */
