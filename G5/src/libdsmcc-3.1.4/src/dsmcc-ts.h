#ifndef DSMCC_TS_H
#define DSMCC_TS_H

#include <stdint.h>

#define DSMCC_TSPARSER_BUFFER_SIZE 8192

struct dsmcc_tsparser_buffer
{
	uint16_t pid;
	int      si_seen;
	int      in_section;
	int      cont;
	uint8_t  data[DSMCC_TSPARSER_BUFFER_SIZE];

	struct dsmcc_tsparser_buffer *next;
};

#endif
