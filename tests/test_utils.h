#ifndef LLTD_TEST_UTILS_H
#define LLTD_TEST_UTILS_H

#include <stdbool.h>
#include <stdio.h>

#define TEST_ASSERT(cond) do { \
    if (!(cond)) { \
        fprintf(stderr, "Assertion failed: %s (%s:%d)\n", #cond, __FILE__, __LINE__); \
        return false; \
    } \
} while (0)

#define TEST_ASSERT_EQ_INT(actual, expected) do { \
    if ((actual) != (expected)) { \
        fprintf(stderr, "Assertion failed: %s == %s (got %d, expected %d) (%s:%d)\n", \
            #actual, #expected, (int)(actual), (int)(expected), __FILE__, __LINE__); \
        return false; \
    } \
} while (0)

#define TEST_ASSERT_EQ_SIZE(actual, expected) do { \
    if ((actual) != (expected)) { \
        fprintf(stderr, "Assertion failed: %s == %s (got %zu, expected %zu) (%s:%d)\n", \
            #actual, #expected, (size_t)(actual), (size_t)(expected), __FILE__, __LINE__); \
        return false; \
    } \
} while (0)

struct test_case {
    const char *name;
    bool (*fn)(void);
};

static inline int run_tests(const struct test_case *cases, size_t count) {
    size_t failed = 0;
    for (size_t i = 0; i < count; i++) {
        if (!cases[i].fn()) {
            fprintf(stderr, "Test failed: %s\n", cases[i].name);
            failed++;
        }
    }
    if (failed > 0) {
        fprintf(stderr, "%zu/%zu tests failed\n", failed, count);
        return 1;
    }
    return 0;
}

#endif
