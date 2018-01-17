/**
 * @file
 * @brief Configuration file tests.
 */

/*
 * ANSI C/POSIX header files.
 */
#include <stdio.h>

/*
 * Package header files.
 */
#include "config.h"

/*
 * Test header files.
 */
#include "tests.h"

/**
 * @brief Description of expected parsing results.
 */
typedef struct {
    /**
     * @brief Indicate if parsing is expected to succeed.
     */
    bool must_succeed;

    /**
     * @brief Received entries. It must end with a @c CONFIG_ENTRY_MAX entry.
     */
    config_entry_t *entries;

    /**
     * @brief Next entry we are waiting for.
     */
    unsigned int next_entry;

} expected_results_t;

/**
 * @brief Configuration parsing callback.
 *
 * @param[in] entry Configuration entry.
 * @param[in] priv Opaque pointer to expected results.
 *
 * @return
 * - true on success.
 * - false to stop parsing.
 */
static bool test_config_callback(const config_entry_t *entry, void *priv)
{
    expected_results_t *results = (expected_results_t *)priv;
    config_entry_t *next_entry;

    next_entry = &results->entries[results->next_entry];
    if (next_entry->type == CONFIG_ENTRY_MAX) {
        TEST_FAILURE("[%u] unexpected entry (type %d)", results->next_entry,
                entry->type);
        return false;
    }

    if (entry->type != next_entry->type) {
        TEST_FAILURE("[%u] got entry of type %d instead of %d",
                results->next_entry, next_entry->type, entry->type);
        return false;
    }

    switch (entry->type) {
        case CONFIG_ENTRY_USER:
            if (strcmp(entry->u.user, next_entry->u.user) != 0) {
                TEST_FAILURE("[%u] got user '%s' instead of '%s'",
                        results->next_entry, entry->u.user, next_entry->u.user);
                return false;
            }
            break;

        case CONFIG_ENTRY_GROUP:
            if (strcmp(entry->u.group, next_entry->u.group) != 0) {
                TEST_FAILURE("[%u] got group '%s' instead of '%s'",
                        results->next_entry, entry->u.group,
                        next_entry->u.group);
                return false;
            }
            break;

        case CONFIG_ENTRY_UMASK:
            if (entry->u.umask != next_entry->u.umask) {
                TEST_FAILURE("[%u] got umask %#o instead of %#o",
                        results->next_entry, entry->u.umask,
                        next_entry->u.umask);
                return false;
            }
            break;

        case CONFIG_ENTRY_RLIMIT:
            if (strcmp(entry->u.rlimit.name, next_entry->u.rlimit.name) != 0 ||
                strcmp(entry->u.rlimit.current,
                    next_entry->u.rlimit.current) != 0 ||
                strcmp(entry->u.rlimit.maximum,
                    next_entry->u.rlimit.maximum) != 0) {
                TEST_FAILURE("[%u] got rlimit '%s' '%s' '%s' instead of '%s' "
                        "'%s' '%s'", results->next_entry, entry->u.rlimit.name,
                        entry->u.rlimit.current, entry->u.rlimit.maximum,
                        next_entry->u.rlimit.name,
                        next_entry->u.rlimit.current,
                        next_entry->u.rlimit.maximum);
                return false;
            }
            break;

        case CONFIG_ENTRY_ENV:
            if (strcmp(entry->u.env.name, next_entry->u.env.name) != 0 ||
                strcmp(entry->u.env.value, next_entry->u.env.value) != 0) {
                TEST_FAILURE("[%u] got environment variable '%s' '%s' instead "
                        "of '%s' '%s'", results->next_entry, entry->u.env.name,
                        entry->u.env.value, next_entry->u.env.name,
                        next_entry->u.env.value);
                return false;
            }
            break;

        default:
            TEST_FAILURE("[%u] unknown entry type", results->next_entry);
            return false;
    }

    results->next_entry++;

    return true;
}

/**
 * @brief Load a configuration file and compare with expected results.
 *
 * @param[in] file Path of file.
 * @param[in] results Expected results.
 *
 * @return
 * - true on success.
 * - false on failure.
 */
static bool test_config_load_and_check(const char *file,
        expected_results_t *results)
{
    bool success = false;
    FILE *f;
    unsigned int entry_count;

    f = fopen(file, "r");
    if (f == NULL) {
        CU_FAIL("cannot open configuration file");
        goto l_exit;
    }

    entry_count = 0;
    while (results->entries[entry_count].type != CONFIG_ENTRY_MAX)
        entry_count++;
    results->next_entry = 0;

    success = lxc_genesis_config_load(f, test_config_callback, results);
    if (success && !results->must_succeed) {
        CU_FAIL("parsing succeeded but must fail");
        goto l_exit;
    }
    else if (!success && results->must_succeed) {
        CU_FAIL("parsing failed but must succeed");
        goto l_exit;
    }

    if (results->next_entry < entry_count) {
        TEST_FAILURE("got %u entries returned instead of %u",
                results->next_entry, entry_count);
        goto l_exit;
    }

    success = true;

l_exit:
    if (f != NULL)
        fclose(f);

    return success;
}


/**
 * @brief Initialize the configuration file test suite.
 *
 * @return
 * - 0 on success.
 * - 1 on failure.
 */
static int test_config_init(void)
{
    return 0;
}

/**
 * @brief Clean up the configuration file test suite.
 *
 * @return
 * - 0 on success.
 * - 1 on failure.
 */
static int test_config_cleanup(void)
{
    return 0;
}

/**
 * @brief Test a valid configuration file.
 */
static void test_config_valid(void)
{
    config_entry_t entries[] = {
        { CONFIG_ENTRY_USER, { .user = "username" } },
        { CONFIG_ENTRY_GROUP, { .group = "groupname" } },
        { CONFIG_ENTRY_UMASK, { .umask = 0711 } },
        { CONFIG_ENTRY_RLIMIT, { .rlimit = { "RLIMIT_CORE", "6000000", "10000000" } } },
        { CONFIG_ENTRY_RLIMIT, { .rlimit = { "RLIMIT_CPU", "250", "1024" } } },
        { CONFIG_ENTRY_RLIMIT, { .rlimit = { "RLIMIT_DATA", "6000000", "10000000" } } },
        { CONFIG_ENTRY_ENV, { .env = { "ENV_VAR1", "ENV_VALUE1" } } },
        { CONFIG_ENTRY_ENV, { .env = { "ENV_VAR2", "ENV_VALUE2" } } },
        { CONFIG_ENTRY_ENV, { .env = { "ENV_VAR3", "ENV_VALUE3" } } },
        { CONFIG_ENTRY_MAX, { NULL } }
    };
    expected_results_t results = { true, entries, 0 };

    test_config_load_and_check("configs/valid.conf", &results);
}

/**
 * @brief Test comments.
 */
static void test_config_comments(void)
{
    config_entry_t entries[] = {
        { CONFIG_ENTRY_USER, { .user = "username" } },
        { CONFIG_ENTRY_GROUP, { .group = "groupname#" } },
        { CONFIG_ENTRY_MAX, { NULL } }
    };
    expected_results_t results = { true, entries, 0 };

    test_config_load_and_check("configs/comments.conf", &results);
}

/**
 * @brief "user" not followed by an end-of-line.
 */
static void test_config_user_no_end_of_line(void)
{
    config_entry_t entries[] = {
        { CONFIG_ENTRY_USER, { .user = "username" } },
        { CONFIG_ENTRY_MAX, { NULL } }
    };
    expected_results_t results = { true, entries, 0 };

    test_config_load_and_check("configs/user_no_end_of_line.conf", &results);
}

/**
 * @brief "user" with no assignment.
 */
static void test_config_user_no_assignment(void)
{
    config_entry_t entries[] = {
        { CONFIG_ENTRY_MAX, { NULL } }
    };
    expected_results_t results = { false, entries, 0 };

    test_config_load_and_check("configs/user_no_assignment.conf", &results);
}

/**
 * @brief "user" with no value.
 */
static void test_config_user_no_value(void)
{
    config_entry_t entries[] = {
        { CONFIG_ENTRY_MAX, { NULL } }
    };
    expected_results_t results = { false, entries, 0 };

    test_config_load_and_check("configs/user_no_value.conf", &results);
}

/**
 * @brief "user" with assignment on next line.
 */
static void test_config_user_assigment_next_line(void)
{
    config_entry_t entries[] = {
        { CONFIG_ENTRY_MAX, { NULL } }
    };
    expected_results_t results = { false, entries, 0 };

    test_config_load_and_check("configs/user_assignement_next_line.conf",
        &results);
}

/**
 * @brief "user" with multiple values.
 */
static void test_config_user_multiple_values(void)
{
    config_entry_t entries[] = {
        { CONFIG_ENTRY_MAX, { NULL } }
    };
    expected_results_t results = { false, entries, 0 };

    test_config_load_and_check("configs/user_multiple_values.conf",
        &results);
}

/**
 * @brief "group" not followed by an end-of-line.
 */
static void test_config_group_no_end_of_line(void)
{
    config_entry_t entries[] = {
        { CONFIG_ENTRY_GROUP, { .group = "groupname" } },
        { CONFIG_ENTRY_MAX, { NULL } }
    };
    expected_results_t results = { true, entries, 0 };

    test_config_load_and_check("configs/group_no_end_of_line.conf", &results);
}

/**
 * @brief "group" with no assignment.
 */
static void test_config_group_no_assignment(void)
{
    config_entry_t entries[] = {
        { CONFIG_ENTRY_MAX, { NULL } }
    };
    expected_results_t results = { false, entries, 0 };

    test_config_load_and_check("configs/group_no_assignment.conf", &results);
}

/**
 * @brief "group" with no value.
 */
static void test_config_group_no_value(void)
{
    config_entry_t entries[] = {
        { CONFIG_ENTRY_MAX, { NULL } }
    };
    expected_results_t results = { false, entries, 0 };

    test_config_load_and_check("configs/group_no_value.conf", &results);
}

/**
 * @brief "group" with assignment on next line.
 */
static void test_config_group_assigment_next_line(void)
{
    config_entry_t entries[] = {
        { CONFIG_ENTRY_MAX, { NULL } }
    };
    expected_results_t results = { false, entries, 0 };

    test_config_load_and_check("configs/group_assignement_next_line.conf",
        &results);
}

/**
 * @brief "group" with multiple values.
 */
static void test_config_group_multiple_values(void)
{
    config_entry_t entries[] = {
        { CONFIG_ENTRY_MAX, { NULL } }
    };
    expected_results_t results = { false, entries, 0 };

    test_config_load_and_check("configs/group_multiple_values.conf",
        &results);
}

/**
 * @brief "umask" with an invalid value.
 */
static void test_config_umask_invalid_value(void)
{
    config_entry_t entries[] = {
        { CONFIG_ENTRY_MAX, { NULL } }
    };
    expected_results_t results = { false, entries, 0 };

    test_config_load_and_check("configs/umask_invalid_value.conf", &results);
}

/**
 * @brief "umask" not followed by an end-of-line.
 */
static void test_config_umask_no_end_of_line(void)
{
    config_entry_t entries[] = {
        { CONFIG_ENTRY_UMASK, { .umask = 0711 } },
        { CONFIG_ENTRY_MAX, { NULL } }
    };
    expected_results_t results = { true, entries, 0 };

    test_config_load_and_check("configs/umask_no_end_of_line.conf", &results);
}

/**
 * @brief "umask" with no assignment.
 */
static void test_config_umask_no_assignment(void)
{
    config_entry_t entries[] = {
        { CONFIG_ENTRY_MAX, { NULL } }
    };
    expected_results_t results = { false, entries, 0 };

    test_config_load_and_check("configs/umask_no_assignment.conf", &results);
}

/**
 * @brief "umask" with no value.
 */
static void test_config_umask_no_value(void)
{
    config_entry_t entries[] = {
        { CONFIG_ENTRY_MAX, { NULL } }
    };
    expected_results_t results = { false, entries, 0 };

    test_config_load_and_check("configs/umask_no_value.conf", &results);
}

/**
 * @brief "umask" with assignment on next line.
 */
static void test_config_umask_assigment_next_line(void)
{
    config_entry_t entries[] = {
        { CONFIG_ENTRY_MAX, { NULL } }
    };
    expected_results_t results = { false, entries, 0 };

    test_config_load_and_check("configs/umask_assignement_next_line.conf",
        &results);
}

/**
 * @brief "umask" with multiple values.
 */
static void test_config_umask_multiple_values(void)
{
    config_entry_t entries[] = {
        { CONFIG_ENTRY_MAX, { NULL } }
    };
    expected_results_t results = { false, entries, 0 };

    test_config_load_and_check("configs/umask_multiple_values.conf",
        &results);
}

/**
 * @brief "rlimit" with an invalid value.
 */
static void test_config_rlimit_invalid_value(void)
{
    config_entry_t entries[] = {
        { CONFIG_ENTRY_MAX, { NULL } }
    };
    expected_results_t results = { false, entries, 0 };

    test_config_load_and_check("configs/rlimit_invalid_value.conf", &results);
}

/**
 * @brief "rlimit" not followed by an end-of-line.
 */
static void test_config_rlimit_no_end_of_line(void)
{
    config_entry_t entries[] = {
        { CONFIG_ENTRY_RLIMIT,
            { .rlimit = { .name = "RLIMIT_CORE", "6000000", "10000000" } } },
        { CONFIG_ENTRY_MAX, { NULL } }
    };
    expected_results_t results = { true, entries, 0 };

    test_config_load_and_check("configs/rlimit_no_end_of_line.conf", &results);
}

/**
 * @brief "rlimit" with no assignment.
 */
static void test_config_rlimit_no_assignment(void)
{
    config_entry_t entries[] = {
        { CONFIG_ENTRY_MAX, { NULL } }
    };
    expected_results_t results = { false, entries, 0 };

    test_config_load_and_check("configs/rlimit_no_assignment.conf", &results);
}

/**
 * @brief "rlimit" with no value.
 */
static void test_config_rlimit_no_value(void)
{
    config_entry_t entries[] = {
        { CONFIG_ENTRY_MAX, { NULL } }
    };
    expected_results_t results = { false, entries, 0 };

    test_config_load_and_check("configs/rlimit_no_value.conf", &results);
}

/**
 * @brief "rlimit" with no current parameter.
 */
static void test_config_rlimit_no_current(void)
{
    config_entry_t entries[] = {
        { CONFIG_ENTRY_MAX, { NULL } }
    };
    expected_results_t results = { false, entries, 0 };

    test_config_load_and_check("configs/rlimit_no_current.conf", &results);
}

/**
 * @brief "rlimit" with no maximum parameter.
 */
static void test_config_rlimit_no_maximum(void)
{
    config_entry_t entries[] = {
        { CONFIG_ENTRY_MAX, { NULL } }
    };
    expected_results_t results = { false, entries, 0 };

    test_config_load_and_check("configs/rlimit_no_maximum.conf", &results);
}

/**
 * @brief "rlimit" with assignment on next line.
 */
static void test_config_rlimit_assigment_next_line(void)
{
    config_entry_t entries[] = {
        { CONFIG_ENTRY_MAX, { NULL } }
    };
    expected_results_t results = { false, entries, 0 };

    test_config_load_and_check("configs/rlimit_assignement_next_line.conf",
        &results);
}

/**
 * @brief "rlimit" with multiple values.
 */
static void test_config_rlimit_multiple_values(void)
{
    config_entry_t entries[] = {
        { CONFIG_ENTRY_MAX, { NULL } }
    };
    expected_results_t results = { false, entries, 0 };

    test_config_load_and_check("configs/rlimit_multiple_values.conf",
        &results);
}

/**
 * @brief "env" with all valid characters in name.
 */
static void test_config_env_valid_name(void)
{
    config_entry_t entries[] = {
        { CONFIG_ENTRY_ENV, { .env = { "!\"#$%&'()*+,-./0123456789:;<>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~", "value" } } },
        { CONFIG_ENTRY_MAX, { NULL } }
    };
    expected_results_t results = { true, entries, 0 };

    test_config_load_and_check("configs/env_valid_name.conf", &results);
}

/**
 * @brief "env" with all valid characters in value.
 */
static void test_config_env_valid_value(void)
{
    config_entry_t entries[] = {
        { CONFIG_ENTRY_ENV, { .env = { "name", "!\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~" } } },
        { CONFIG_ENTRY_MAX, { NULL } }
    };
    expected_results_t results = { true, entries, 0 };

    test_config_load_and_check("configs/env_valid_value.conf", &results);
}

/**
 * @brief "env" not followed by an end-of-line.
 */
static void test_config_env_no_end_of_line(void)
{
    config_entry_t entries[] = {
        { CONFIG_ENTRY_ENV, { .env = { "MY_VAR", "value" } } },
        { CONFIG_ENTRY_MAX, { NULL } }
    };
    expected_results_t results = { true, entries, 0 };

    test_config_load_and_check("configs/env_no_end_of_line.conf", &results);
}

/**
 * @brief "env" with no assignment.
 */
static void test_config_env_no_assignment(void)
{
    config_entry_t entries[] = {
        { CONFIG_ENTRY_MAX, { NULL } }
    };
    expected_results_t results = { false, entries, 0 };

    test_config_load_and_check("configs/env_no_assignment.conf", &results);
}

/**
 * @brief "env" with no value.
 */
static void test_config_env_no_value(void)
{
    config_entry_t entries[] = {
        { CONFIG_ENTRY_MAX, { NULL } }
    };
    expected_results_t results = { false, entries, 0 };

    test_config_load_and_check("configs/env_no_value.conf", &results);
}

/**
 * @brief "env" with assignment on next line.
 */
static void test_config_env_assigment_next_line(void)
{
    config_entry_t entries[] = {
        { CONFIG_ENTRY_MAX, { NULL } }
    };
    expected_results_t results = { false, entries, 0 };

    test_config_load_and_check("configs/env_assignement_next_line.conf",
        &results);
}

/**
 * @brief "env" with multiple values.
 */
static void test_config_env_multiple_values(void)
{
    config_entry_t entries[] = {
        { CONFIG_ENTRY_MAX, { NULL } }
    };
    expected_results_t results = { false, entries, 0 };

    test_config_load_and_check("configs/env_multiple_values.conf",
        &results);
}

/**
 * @brief List of tests.
 */
static test_desc_t test_config_tests[] = {
    /* global */
    TEST_DESC_INIT("valid", test_config_valid),
    TEST_DESC_INIT("comments", test_config_comments),

    /* user */
    TEST_DESC_INIT("user_no_end_of_line", test_config_user_no_end_of_line),
    TEST_DESC_INIT("user_no_assignment", test_config_user_no_assignment),
    TEST_DESC_INIT("user_no_value", test_config_user_no_value),
    TEST_DESC_INIT("user_assignement_next_line",
        test_config_user_assigment_next_line),
    TEST_DESC_INIT("user_multiple_values", test_config_user_multiple_values),

    /* group */
    TEST_DESC_INIT("group_no_end_of_line", test_config_group_no_end_of_line),
    TEST_DESC_INIT("group_no_assignment", test_config_group_no_assignment),
    TEST_DESC_INIT("group_no_value", test_config_group_no_value),
    TEST_DESC_INIT("group_assignement_next_line",
        test_config_group_assigment_next_line),
    TEST_DESC_INIT("group_multiple_values", test_config_group_multiple_values),

    /* umask */
    TEST_DESC_INIT("umask_invalid_value", test_config_umask_invalid_value),
    TEST_DESC_INIT("umask_no_end_of_line", test_config_umask_no_end_of_line),
    TEST_DESC_INIT("umask_no_assignment", test_config_umask_no_assignment),
    TEST_DESC_INIT("umask_no_value", test_config_umask_no_value),
    TEST_DESC_INIT("umask_assignement_next_line",
        test_config_umask_assigment_next_line),
    TEST_DESC_INIT("umask_multiple_values", test_config_umask_multiple_values),

    /* rlimit */
    TEST_DESC_INIT("rlimit_invalid_name", test_config_rlimit_invalid_value),
    TEST_DESC_INIT("rlimit_no_end_of_line", test_config_rlimit_no_end_of_line),
    TEST_DESC_INIT("rlimit_no_assignment", test_config_rlimit_no_assignment),
    TEST_DESC_INIT("rlimit_no_value", test_config_rlimit_no_value),
    TEST_DESC_INIT("rlimit_no_current", test_config_rlimit_no_current),
    TEST_DESC_INIT("rlimit_no_maximum", test_config_rlimit_no_maximum),
    TEST_DESC_INIT("rlimit_assignement_next_line",
        test_config_rlimit_assigment_next_line),
    TEST_DESC_INIT("rlimit_multiple_values", test_config_rlimit_multiple_values),

    /* environment variable */
    TEST_DESC_INIT("env_valid_name", test_config_env_valid_name),
    TEST_DESC_INIT("env_valid_value", test_config_env_valid_value),
    TEST_DESC_INIT("env_no_end_of_line", test_config_env_no_end_of_line),
    TEST_DESC_INIT("env_no_assignment", test_config_env_no_assignment),
    TEST_DESC_INIT("env_no_value", test_config_env_no_value),
    TEST_DESC_INIT("env_assignement_next_line",
        test_config_env_assigment_next_line),
    TEST_DESC_INIT("env_multiple_values", test_config_env_multiple_values),

    /* end of tests */
    TEST_DESC_EMPTY
};

test_suite_desc_t test_config_suite = TEST_SUITE_DESC_INIT(
    "config",
    test_config_init,
    test_config_cleanup,
    test_config_tests
);
