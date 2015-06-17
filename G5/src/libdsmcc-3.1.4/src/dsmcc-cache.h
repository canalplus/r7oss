#ifndef DSMCC_CACHE_H
#define DSMCC_CACHE_H

#include <stdint.h>

struct dsmcc_module_id
{
	uint32_t download_id;
	uint16_t module_id;
	uint8_t  module_version;
	uint32_t dii_transaction_id;
};

struct dsmcc_object_id
{
	uint16_t module_id;
	uint32_t key;
	uint32_t key_mask;
};

#endif /* DSMCC_CACHE_H */
