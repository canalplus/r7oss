#ifndef CONFIG_H_5472C715_BA87_4389_9FD4_EA198E3DE888
#define CONFIG_H_5472C715_BA87_4389_9FD4_EA198E3DE888

/**
 * @file
 * @brief Configuration file parsing.
 */

#include <stdbool.h>
#include <stdio.h>

/**
 * @brief Configuration entry type.
 */
typedef enum config_entry_type {
    CONFIG_ENTRY_UNKNOWN = 0,
    CONFIG_ENTRY_USER,
    CONFIG_ENTRY_GROUP,
    CONFIG_ENTRY_UMASK,
    CONFIG_ENTRY_RLIMIT,
    CONFIG_ENTRY_ENV,
    CONFIG_ENTRY_MAX
} config_entry_type_t;

/**
 * @brief Configuration entry.
 */
typedef struct config_entry {
    config_entry_type_t type;
    union {
        char *user;
        char *group;
        int umask;
        struct {
            char *name;
            char *current;
            char *maximum;
        } rlimit;
        struct {
            char *name;
            char *value;
        } env;
    } u;
} config_entry_t;

/**
 * @brief Callback triggered on each entry.
 *
 * @param[in] entry Configuration entry.
 * @param[in] priv Callback private data.
 *
 * @return
 * - true on success.
 * - false to stop parsing.
 */
typedef bool config_callback_t(const config_entry_t *entry, void *priv);

/**
 * @brief Load a configuration file.
 *
 * @param[in] f Input file stream.
 * @param[in] cb Callback triggered on each entry.
 * @param[in] priv Callback private data.
 *
 * @return
 * - true if loading succeeded.
 * - false if loading failed.
 */
bool lxc_genesis_config_load(FILE *f, config_callback_t *cb, void *priv);

#endif /* file inclusion guard */
