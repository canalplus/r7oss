#ifndef DSMCC_BIOP_MODULE_H
#define DSMCC_BIOP_MODULE_H

#include <stdint.h>
#include "dsmcc-biop-ior.h"

struct biop_module_info
{
	uint32_t mod_timeout;
	uint32_t block_timeout;
	uint32_t min_blocktime;

	uint16_t assoc_tag;

	struct dsmcc_descriptor *descriptors;
};

int dsmcc_biop_parse_module_info(struct biop_module_info *module, uint8_t *data, int data_length);
void dsmcc_biop_free_module_info(struct biop_module_info *module);

#endif
