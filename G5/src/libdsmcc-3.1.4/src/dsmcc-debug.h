#ifndef DSMCC_DEBUG_H
#define DSMCC_DEBUG_H

#include <dsmcc/dsmcc.h>
#include "dsmcc-config.h"

#ifdef DEBUG
#define DSMCC_DEBUG(format, args...) do { if (dsmcc_log_enabled(DSMCC_LOG_DEBUG)) dsmcc_log(DSMCC_LOG_DEBUG, __FILE__, __FUNCTION__, __LINE__, format, ##args); } while (0)
#else
#define DSMCC_DEBUG(format, args...) do { } while (0)
#endif
#define DSMCC_WARN(format, args...) do { if (dsmcc_log_enabled(DSMCC_LOG_WARN)) dsmcc_log(DSMCC_LOG_WARN, __FILE__, __FUNCTION__, __LINE__, format, ##args); } while (0)
#define DSMCC_ERROR(format, args...) do { if (dsmcc_log_enabled(DSMCC_LOG_ERROR)) dsmcc_log(DSMCC_LOG_ERROR, __FILE__, __FUNCTION__, __LINE__, format, ##args); } while (0)
int dsmcc_log_enabled(int severity);
void dsmcc_log(int severity, const char *filename, const char *function, int lineno, char *format, ...);

#endif /* DSMCC_DEBUG_H */
