/*************************************************
*   This file is overriden with a load_config.   *
*   Read this only as an example.                *
**************************************************/

#ifndef _UDEV_RULES_HARDENING_WHITELIST_
#define _UDEV_RULES_HARDENING_WHITELIST_

#include "udev-hardening-utils.h"

static int ACCEPTED_RULE(const char *i, const char **w, int(*m)(const char *, const char *))
{
	const char **j;
	FOREACH(j, w)
		if (m(*j, i)) return 1;
	return 0;
}

int match(const char *a, const char *b) { return MATCH(a, b); }

#ifndef ACCEPTED_IMPORT_PROG
# define ACCEPTED_IMPORT_PROG(i) ACCEPTED_RULE(i, \
			__hardening_import_prog_whitelist, \
			match)
#endif

#ifndef ACCEPTED_RUN
# define ACCEPTED_RUN(i) ACCEPTED_RULE(i, \
			__hardening_run_whitelist, \
			match)
#endif

#ifndef ACCEPTED_IMPORT_BUILTIN
# define ACCEPTED_IMPORT_BUILTIN(i) ACCEPTED_RULE(i, \
			__hardening_import_builtin_whitelist, \
			match)
#endif

static const char * __hardening_import_builtin_whitelist[] = {
	NULL
};

static const char * __hardening_run_whitelist[] = {
	NULL
};

static const char * __hardening_import_prog_whitelist[] = {
	NULL
};

#endif /* _UDEV_RULES_HARDENING_WHITELIST_ */
