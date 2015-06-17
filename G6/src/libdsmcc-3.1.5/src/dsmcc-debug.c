#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <pthread.h>

#include "dsmcc-debug.h"

static dsmcc_logger_t *g_logger = NULL;
static int g_severity = -1;

void dsmcc_set_logger(dsmcc_logger_t *logger, int severity)
{
	g_logger = logger;
	g_severity = severity;
}

int dsmcc_log_enabled(int severity)
{
	return g_severity <= severity;
}

void dsmcc_log(int severity, const char *filename, const char *function, int lineno, char *format, ...)
{
	int i;
	char buffer[1024];

	if (!g_logger || g_severity > severity)
		return;

	i = snprintf(buffer, 1024, "%08lx:%s:%d %s ", pthread_self(), filename, lineno, function);
	va_list va;
	va_start(va, format);
	vsnprintf(buffer + i, 1024 - i, format, va);
	va_end(va);
	(*g_logger)(severity, buffer);
}
