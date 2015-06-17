#ifndef DSMCC_COMPRESS_H
#define DSMCC_COMPRESS_H

#include <stdbool.h>

#include "dsmcc-config.h"
#include "dsmcc-debug.h"

#ifdef HAVE_ZLIB
bool dsmcc_inflate_file(const char *filename);
#else
static inline bool dsmcc_inflate_file(const char *filename)
{
	(void) filename;
	DSMCC_ERROR("Compression support is disabled in this build");
	return false;
}
#endif

#endif /* DSMCC_COMPRESS_H */
