#ifndef DSMCC_SECTION_H
#define DSMCC_SECTION_H

#include <stdint.h>
#include <stdbool.h>

struct dsmcc_section
{
	uint16_t pid;
	uint8_t *data;
	int      length;
};

int dsmcc_parse_section(struct dsmcc_state *state, struct dsmcc_section *section);

#endif
