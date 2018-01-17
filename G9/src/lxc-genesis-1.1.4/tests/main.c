/**
 * @file
 * @brief Test launcher.
 */

/*
 * ANSI C/POSIX header files.
 */
#include <sys/types.h>
#include <sys/wait.h>

#include <getopt.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

/*
 * CUnit header files.
 */
#include <CUnit/CUnit.h>
#include <CUnit/Automated.h>
#include <CUnit/Basic.h>
#include <CUnit/Console.h>

/*
 * Test header files.
 */
#include "tests.h"

/**
 * @brief CUnit interface.
 */
typedef enum interface {
    INTERFACE_BASIC = 0,
    INTERFACE_CONSOLE,
    INTERFACE_CURSES,
    INTERFACE_AUTOMATED
} interface_t;

/**
 * @brief Verbosity level.
 */
typedef enum verbosity {
    VERBOSITY_SILENT = 0,
    VERBOSITY_NORMAL,
    VERBOSITY_VERBOSE
} verbosity_t;

/**
 * @brief The program's effective name.
 */
static const char *s_program = "unit_tests";

/**
 * @brief Indicate that help was requested.
 */
static bool s_option_help = false;

/**
 * @brief Indicate that a listing of tests was requested.
 */
static bool s_option_list = false;

/**
 * CUnit interface to run tests with.
 */
static interface_t s_option_interface = INTERFACE_BASIC;

/**
 * @brief Requested verbosity.
 */
static verbosity_t s_option_verbosity = VERBOSITY_NORMAL;

/**
 * @brief Parse the command line.
 *
 * @param[in] argc The number of arguments.
 * @param[in,out] argv The arguments. Arguments are reordered so that
 *   non-option arguments come last.
 * @param[out] arg_index The index of the first non-processed argument.
 *
 * @return
 * - true if the command line was successfully parsed.
 * - false otherwise.
 */
static bool parse_cmdline(int argc, char *argv[], int *arg_index)
{
    optind = 1;
    opterr = 0;

    while (true) {
        static struct option options[] = {
            { "help", no_argument, 0, 'h' },
            { "interface", required_argument, 0, 'i' },
            { "list", no_argument, 0, 'l' },
            { "silent", no_argument, 0, 's' },
            { "verbose", no_argument, 0, 'v' },
            { NULL, 0, NULL, 0 }
        };

        int c;
        int option_index = -1;

        c = getopt_long(argc, argv, "+:c:hi:lsv", options, &option_index);

        /* End of options. */
        if (c == -1)
            break;

        switch (c) {
            case 0:
                break;

            case 'h':
                s_option_help = true;
                break;

            case 'i':
                if (strcmp(optarg, "basic") == 0)
                    s_option_interface = INTERFACE_BASIC;
                else if (strcmp(optarg, "console") == 0)
                    s_option_interface = INTERFACE_CONSOLE;
                else if (strcmp(optarg, "automated") == 0)
                    s_option_interface = INTERFACE_AUTOMATED;
                else {
                    fprintf(stderr, "unknown interface '%s'\n", optarg);
                    return 0;
                }
                break;

            case 'l':
                s_option_list = true;
                break;

            case 's':
                s_option_verbosity = VERBOSITY_SILENT;
                break;

            case 'v':
                s_option_verbosity = VERBOSITY_VERBOSE;
                break;

            case '?':
                fprintf(stderr, "unknown option '%s'\n", argv[optind - 1]);
                return false;

            case ':':
                fprintf(stderr, "missing parameter for option '%s'\n",
                        argv[optind - 1]);
                return false;

            default:
                fprintf(stderr, "unexpected option '%c' (%#X)\n", c, c);
                return false;
        }
    }

    *arg_index = optind;

    return true;
}

/**
 * @brief Print the help.
 */
static void print_help()
{
    printf(
        "usage : %s [options] [<suite> <tests> . <suite> <tests>...]\n"
        "\n"
        "The tests to be run can be specified on the command line, by "
        "indicating  the suite name followed by a list of test names ended by "
        "a dot (.) character or the end of the argument list. Several suites "
        "can be indicated in this way. For example, the following call will run "
        "test1 and test3 of suite1, followed by test2 and test4 of suite3, and "
        "all the tests of suite4:\n"
        "\n"
        "$ %s suite1 test1 test3 . suite3 test2 test4 . suite4\n"
        "\n"
        "If suites or tests cannot be found, a warning is printed but no error "
        "is returned.\n"
        "\n"
        "If the tests are executed with AUTOMATED_TEST_FILE environment "
        "variable set and an empty command line, the tests are executed in "
        "automated mode, i.e. an XML containing the test report is generated.\n"
        "\n"
        "Available options:\n"
        "\n"
        "\t-h,--help\n"
        "\t\tprint this help.\n"
        "\n"
        "\t-i,--interface <interface>\n"
        "\t\tset the CUnit interface to run tests with: basic (default), "
        "console, automated.\n"
        "\n"
        "\t-l,--list\n"
        "\t\tlist all tests.\n"
        "\n"
        "\t-s, --silent\n"
        "\t\tenter silent mode\n"
        "\n" "\t-v, --verbose\n" "\t\tenter verbose mode\n",
        s_program, s_program);
}

/**
 * @brief Find a suite by name.
 *
 * @param[in] name Suite name.
 *
 * @return
 * - NULL if no suite with this name exists.
 * - A suite descriptor otherwise.
 */
static test_suite_desc_t *find_suite(const char *name)
{
    test_suite_desc_t **suite_ptr;

    for (suite_ptr = test_suites; *suite_ptr != NULL; ++suite_ptr) {
        test_suite_desc_t *suite = (*suite_ptr);

        if (strcmp(suite->name, name) == 0)
            return suite;
    }

    return NULL;
}

/**
 * @brief Find a test in a suite by name.
 *
 * @param[in] suite Suite descriptor.
 * @param[in] name Test name.
 *
 * @return
 * - NULL if no test with this name exists.
 * - A test descriptor otherwise.
 */
static test_desc_t *find_test(test_suite_desc_t * suite, const char *name)
{
    test_desc_t *test;

    for (test = suite->tests; test->name != NULL; ++test) {
        if (strcmp(test->name, name) == 0)
            return test;
    }

    return NULL;
}

/**
 * @brief Register tests of a suite.
 *
 * @param[in] suite_name Name of suite.
 * @param[in] test_names Array of test names.
 * @param[in] test_count Number of test names.
 */
static void register_tests_of_suite(const char *suite_name, char *test_names[],
        int test_count)
{
    test_suite_desc_t *suite;

    /* Find the corresponding suite. */
    suite = find_suite(suite_name);
    if (suite == NULL) {
        if (s_option_verbosity >= VERBOSITY_NORMAL)
            fprintf(stderr, "no suite '%s'\n", suite_name);
        return;
    }

    /* Register the suite. */
    suite->suite = CU_add_suite(suite->name, suite->init_func,
            suite->cleanup_func);
    if (suite->suite == NULL) {
        if (s_option_verbosity >= VERBOSITY_NORMAL)
            fprintf(stderr, "cannot register suite %s: %s\n", suite->name,
                    CU_get_error_msg());
        return;
    }

    if (s_option_verbosity >= VERBOSITY_VERBOSE)
        printf("registered suite %s\n", suite->name);

    /* Test names specified. */
    if (test_count > 0) {
        int i;

        /* Register the specified tests. */
        for (i = 0; i < test_count; ++i) {
            test_desc_t *test;
            const char *test_name = test_names[i];

            /* Find the corresponding test. */
            test = find_test(suite, test_name);
            if (test == NULL) {
                if (s_option_verbosity >= VERBOSITY_NORMAL)
                    fprintf(stderr, "no test '%s' in suite '%s'", test_name,
                            suite_name);
                continue;
            }

            /* Register this test. */
            test->test = CU_add_test(suite->suite, test->name, test->test_func);
            if (test->test == NULL) {
                if (s_option_verbosity >= VERBOSITY_NORMAL)
                    fprintf(stderr, "cannot register test %s of suite %s: %s\n",
                            test->name, suite->name, CU_get_error_msg());
            } else if (s_option_verbosity >= VERBOSITY_VERBOSE)
                printf("registered test %s\n", test->name);
        }
    }
    /* No test specified. */
    else {
        test_desc_t *test;

        /* Register all tests. */
        for (test = suite->tests; test->name != NULL; ++test) {
            test->test = CU_add_test(suite->suite, test->name, test->test_func);
            if (test->test == NULL) {
                if (s_option_verbosity >= VERBOSITY_NORMAL)
                    fprintf(stderr, "cannot register test %s of suite %s: %s\n",
                            test->name, suite->name, CU_get_error_msg());
            } else if (s_option_verbosity >= VERBOSITY_VERBOSE)
                printf("registered test %s\n", test->name);
        }
    }
}

/**
 * @brief Register selected suites and tests.
 *
 * @param[in] selectors Suite and test name selectors.
 * @param[in] count Number of selectors.
 */
static void register_tests(char *selectors[], int count)
{
    int selector_idx;
    int suite_name_idx = -1;

    if (count == 0) {
        test_suite_desc_t **suite_ptr;

        for (suite_ptr = test_suites; *suite_ptr != NULL; ++suite_ptr)
            register_tests_of_suite((*suite_ptr)->name, NULL, 0);
        return;
    }

    for (selector_idx = 0; selector_idx < count; ++selector_idx) {
        if (strcmp(selectors[selector_idx], ".") == 0) {
            if (suite_name_idx != -1) {
                register_tests_of_suite(selectors[suite_name_idx],
                        selectors + suite_name_idx + 1,
                        selector_idx - suite_name_idx - 1);
                suite_name_idx = -1;
            }
        } else if (suite_name_idx == -1)
            suite_name_idx = selector_idx;
    }

    if (suite_name_idx != -1) {
        register_tests_of_suite(selectors[suite_name_idx],
                selectors + suite_name_idx + 1, count - suite_name_idx - 1);
    }
}

/**
 * @brief List registered tests.
 */
static void list_tests(void)
{
    test_suite_desc_t **suite_ptr;

    for (suite_ptr = test_suites; *suite_ptr != NULL; ++suite_ptr) {
        test_suite_desc_t *suite = (*suite_ptr);
        test_desc_t *test;

        printf("%s\n", suite->name);

        for (test = suite->tests; test->name != NULL; ++test)
            printf("\t%s\n", test->name);
    }
}

int main(int argc, char *argv[])
{
    bool success = false;
    int arg_index;
    bool registry_initialized = false;
    CU_ErrorCode cu_error;
    const char * s;

    /* Set the program name. */
    if (argc > 0) {
        const char *sep = strrchr(argv[0], '/');
        if (sep) s_program = sep + 1;
        else sep = argv[0];
    }

    /* Run in automated mode if executed through Jenkins */
    if ((s = getenv("AUTOMATED_TEST_FILE")) != NULL && s[0] != '\0') {
        s_option_interface = INTERFACE_AUTOMATED;
    }

    /* Parse the command line. */
    if (!parse_cmdline(argc, argv, &arg_index))
        goto l_exit;

    /* Print help. */
    if (s_option_help) {
        print_help();
        success = true;
        goto l_exit;
    }

    /* Listing of tests requested. */
    if (s_option_list) {
        list_tests();
        success = true;
        goto l_exit;
    }

    /* Initialize the test registry. */
    cu_error = CU_initialize_registry();
    if (cu_error != CUE_SUCCESS) {
        fprintf(stderr, "cannot initialize CUnit: %s\n", CU_get_error_msg());
        goto l_exit;
    }
    registry_initialized = false;

    /* Register tests. */
    register_tests(argv + optind, (argc - optind));

    /* Run tests. */
    switch (s_option_interface) {

        case INTERFACE_CONSOLE:
            CU_console_run_tests();
            break;

        case INTERFACE_AUTOMATED:
            CU_set_output_filename(s_program);
            CU_automated_run_tests();
            break;

        case INTERFACE_BASIC:
        default:
            switch (s_option_verbosity) {
                case VERBOSITY_SILENT:
                    CU_basic_set_mode(CU_BRM_SILENT);
                    break;
                case VERBOSITY_VERBOSE:
                    CU_basic_set_mode(CU_BRM_VERBOSE);
                    break;
                case VERBOSITY_NORMAL:
                default:
                    CU_basic_set_mode(CU_BRM_NORMAL);
                    break;
            }
            CU_basic_run_tests();
            break;
    }

    /* Check if all tests passed. */
    success = (CU_get_number_of_suites_failed() == 0 &&
            CU_get_number_of_tests_failed() == 0 &&
            CU_get_number_of_failures() == 0);

l_exit:
    if (registry_initialized)
        CU_cleanup_registry();
    return (success ? EXIT_SUCCESS : EXIT_FAILURE);
}
