#ifndef UTILS_H_D81AC48E_DE9A_4D75_87BD_85530517BE2B
#define UTILS_H_D81AC48E_DE9A_4D75_87BD_85530517BE2B

/**
 * @file
 * @brief Common utilities.
 */

#include <stdio.h>

#ifdef DEBUG
/**
 * @brief Log an information message.
 *
 * @param[in] format printf-like format string.
 * @param ... Format arguments.
 */
# define LOG_INFO(format,...) do { \
    (void)printf(format, ## __VA_ARGS__); \
} while(0)

/**
 * @brief Log an error message.
 *
 * @param[in] format printf-like format string.
 * @param ... Format arguments.
 */
# define LOG_ERROR(format,...) do { \
    (void)fprintf(stderr, "ERROR: "); \
    (void)fprintf(stderr, format, ## __VA_ARGS__); \
} while(0)
#else
/**
 * @brief Do nothing.
 *
 * @param[in] format printf-like format string.
 * @param ... Format arguments.
 */
# define LOG_INFO(format,...) do {} while(0)

/**
 * @brief Do nothing.
 *
 * @param[in] format printf-like format string.
 * @param ... Format arguments.
 */
# define LOG_ERROR(format,...) do {} while(0)
#endif

/**
 * @brief Duplicate a string and abort program on failure.
 *
 * @param[in] s A string or @c NULL.
 *
 * @return
 * - @c NULL if @p s i NULL.
 * - A copy of the string otherwise. Call @c free on it when you don't need it
 *   anymore.
 */
void *lxc_genesis_strdup(const char *s);

/**
 * @brief Free allocated memory.
 *
 * If the the pointer is @c NULL, nothing is done. This is to avoid warnings
 * with debugging tools that may complain when freeing @c NULL pointers.
 */
void lxc_genesis_free(void *p);

#endif /* file inclusion guard */
