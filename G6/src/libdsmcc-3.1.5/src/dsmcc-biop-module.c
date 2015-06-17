#include <stdlib.h>
#include <string.h>

#include "dsmcc-biop-module.h"
#include "dsmcc-biop-ior.h"
#include "dsmcc-biop-tap.h"
#include "dsmcc-descriptor.h"
#include "dsmcc-debug.h"
#include "dsmcc-util.h"

int dsmcc_biop_parse_module_info(struct biop_module_info *module_info, uint8_t *data, int data_length)
{
	int off = 0, ret;
	unsigned char userinfo_len;
	struct biop_tap *tap = NULL;

	memset(module_info, 0, sizeof(struct biop_module_info));

	if (!dsmcc_getlong(&module_info->mod_timeout, data, off, data_length))
		return -1;
	off += 4;
	DSMCC_DEBUG("Mod Timeout = %lu", module_info->mod_timeout);

	if (!dsmcc_getlong(&module_info->block_timeout, data, off, data_length))
		return -1;
	off += 4;
	DSMCC_DEBUG("Block Timeout = %lu", module_info->block_timeout);

	if (!dsmcc_getlong(&module_info->min_blocktime, data, off, data_length))
		return -1;
	off += 4;
	DSMCC_DEBUG("Min Block Timeout = %lu", module_info->min_blocktime);

	ret = dsmcc_biop_parse_taps_keep_only_first(&tap, BIOP_OBJECT_USE, data + off, data_length - off);
	if (ret < 0)
	{
		dsmcc_biop_free_tap(tap);
		return -1;
	}
	off += ret;
	module_info->assoc_tag = tap->assoc_tag;
	dsmcc_biop_free_tap(tap);

	if (!dsmcc_getbyte(&userinfo_len, data, off, data_length))
		return -1;
	off++;
	DSMCC_DEBUG("UserInfo Len = %hhu", userinfo_len);

	if (userinfo_len > 0)
	{
		ret = dsmcc_parse_descriptors(&module_info->descriptors, data + off, userinfo_len);
		if (ret < 0)
		{
			dsmcc_biop_free_module_info(module_info);
			return -1;
		}
		off += userinfo_len;
	}
	else
	{
		module_info->descriptors = NULL;
	}

	return off;
}

void dsmcc_biop_free_module_info(struct biop_module_info *module_info)
{
	if (module_info == NULL)
		return;

	dsmcc_descriptors_free_all(module_info->descriptors);
	module_info->descriptors = NULL;

	free(module_info);
}
