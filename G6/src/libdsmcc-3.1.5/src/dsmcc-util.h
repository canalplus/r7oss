#ifndef DSMCC_UTIL_H
#define DSMCC_UTIL_H

#include <stdint.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/time.h>

#include "dsmcc.h"

uint32_t dsmcc_crc32(uint8_t *data, uint32_t len);

bool dsmcc_file_copy(const char *dstfile, const char *srcfile, int offset, int length);
bool dsmcc_file_link(const char *dstfile, const char *srcfile, int length);
bool dsmcc_file_write_block(const char *dstfile, int offset, uint8_t *data, int length);

static inline int dsmcc_min(int a, int b)
{
	if (a < b)
		return a;
	else
		return b;
}

static inline bool dsmcc_getlong(uint32_t *dst, const uint8_t *data, int offset, int length)
{
	data += offset;
	length -= offset;
	if (length < 4)
	{
		DSMCC_ERROR("size error");
		return 0;
	}
	*dst = (data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3];
	return 1;
}

static inline bool dsmcc_getshort(uint16_t *dst, const uint8_t *data, int offset, int length)
{
	data += offset;
	length -= offset;
	if (length < 2)
	{
		DSMCC_ERROR("size error");
		return 0;
	}
	*dst = (data[0] << 8) | data[1];
	return 1;
}

static inline bool dsmcc_getbyte(uint8_t *dst, const uint8_t *data, int offset, int length)
{
	data += offset;
	length -= offset;
	if (length < 1)
	{
		DSMCC_ERROR("size error");
		return 0;
	}
	*dst = data[0];
	return 1;
}

static inline bool dsmcc_getkey(uint32_t *dstkey, uint32_t *dstkey_mask, int key_length, const uint8_t *data, int offset, int length)
{
	uint32_t key = 0, key_mask = 0;
	uint8_t tmp;
	int i;

	if (key_length < 0 || key_length > 4)
		return 0;

	for (i = 0; i < key_length; i++)
	{
		if (!dsmcc_getbyte(&tmp, data, offset, length))
			return 0;
		offset++;
		key = (key << 8) | tmp;
		key_mask = (key_mask << 8) | 0xff;
	}

	*dstkey = key;
	*dstkey_mask = key_mask;

	return 1;
}

static inline bool dsmcc_strdup(char **dst, int dstlength, const uint8_t *data, int offset, int length)
{
	data += offset;
	length -= offset;
	if (length < dstlength)
		return 0;
	if (dstlength > 0)
	{
		*dst = malloc(dstlength);
		strncpy(*dst, (const char *)data, dstlength);
	}
	else
		*dst = NULL;
	return 1;
}

static inline bool dsmcc_memdup(uint8_t **dst, int dstlength, const uint8_t *data, int offset, int length)
{
	data += offset;
	length -= offset;
	if (length < dstlength)
		return 0;
	if (dstlength > 0)
	{
		*dst = malloc(dstlength);
		memcpy(*dst, data, dstlength);
	}
	else
		*dst = NULL;
	return 1;
}

#endif
