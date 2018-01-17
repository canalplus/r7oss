#ifndef TESTS_H_C23234A9_19C2_4805_A708_875D79A4AF85
#define TESTS_H_C23234A9_19C2_4805_A708_875D79A4AF85

/**
 * @file
 * @brief Description and declarations of tests.
 */

/*
 * ANSI C/POSIX header files.
 */
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

/*
 * CUnit header files.
 */
#include <CUnit/CUnit.h>

/**
 * @brief Record a test failure.
 *
 * @param[in] format printf-like format string.
 * @param[in] ... Format arguments.
 */
#define TEST_FAILURE(format,...) do { \
    char __msg[1024]; \
    snprintf(__msg, sizeof(__msg), format, ## __VA_ARGS__); \
    CU_assertImplementation(CU_FALSE, __LINE__, __msg, __FILE__, "", CU_FALSE); \
} while(false)

/**
 * @brief Test descriptor.
 */
typedef struct test_desc {

    /**
     * @brief Test name.
     */
    const char * name;

    /**
     * @brief Test function.
     */
    CU_TestFunc test_func;

    /**
     * @brief CUnit test. It's @c NULL if not registered yet.
     */
    CU_Test * test;

} test_desc_t;

/**
 * @brief Test descriptor initializer.
 *
 * @param[in] name Test name.
 * @param[in] test_func Test function.
 */
#define TEST_DESC_INIT(name, test_func) { (name), (test_func), NULL }

/**
 * @brief Empty test descriptor initializer.
 */
#define TEST_DESC_EMPTY { NULL, NULL, NULL }

/**
 * @brief Test suite descriptor.
 */
typedef struct test_suite_desc {

    /**
     * @brief Suite name.
     */
    const char * name;

    /**
     * @brief Suite initialization function.
     */
    CU_InitializeFunc init_func;

    /**
     * @brief Suite cleanup function.
     */
    CU_CleanupFunc cleanup_func;

    /**
     * @brief List of tests.
     */
    test_desc_t * tests;

    /**
     * @brief CUnit suite. It's @c NULL if not registered yet.
     */
    CU_Suite * suite;

} test_suite_desc_t;

/**
 * @brief Test suite descriptor initializer.
 *
 * @param[in] name Suite name.
 * @param[in] init_func Initialization function.
 * @param[in] cleanup_func Cleanup function.
 * @param[in] tests List of tests.
 */
#define TEST_SUITE_DESC_INIT(name, init_func, cleanup_func, tests) \
    { (name), (init_func), (cleanup_func), (tests), NULL }

/**
 * @brief Empty test suite descriptor initializer.
 */
#define TEST_SUITE_DESC_EMPTY { NULL, NULL, NULL, NULL, NULL }

/**
 * @brief Configuration file test suite.
 */
extern test_suite_desc_t test_config_suite;

/**
 * @brief List of all test suites.
 *
 * It is NULL-terminated.
 */
extern test_suite_desc_t * test_suites[];

#endif /* file inclusion guard */
