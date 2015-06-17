#ifndef DSMCC_DESCRIPTOR_H
#define DSMCC_DESCRIPTOR_H

#include <stdint.h>

struct dsmcc_descriptor_type
{
	char *text;
};

struct dsmcc_descriptor_name
{
	char *text;
};

struct dsmcc_descriptor_info
{
	char *lang_code;
	char *text;
};

struct dsmcc_descriptor_modlink
{
	uint8_t  position;
	uint16_t module_id;
};

struct dsmcc_descriptor_crc32
{
	uint32_t crc;
};

struct dsmcc_descriptor_location
{
	uint8_t location_tag;
};

struct dsmcc_descriptor_dltime
{
	uint32_t download_time;
};

struct dsmcc_descriptor_grouplink
{
	uint8_t  position;
	uint32_t group_id;
};

struct dsmcc_descriptor_compressed
{
	uint8_t  method;
	uint32_t original_size;
};

struct dsmcc_descriptor_label
{
	char *text;
};

struct dsmcc_descriptor_caching_priority
{
	uint8_t priority_value;
	uint8_t transparency_level;
};

struct dsmcc_descriptor_content_type
{
	char *text;
};

enum
{
	DSMCC_DESCRIPTOR_TYPE = 0,
	DSMCC_DESCRIPTOR_NAME,
	DSMCC_DESCRIPTOR_INFO,
	DSMCC_DESCRIPTOR_MODLINK,
	DSMCC_DESCRIPTOR_CRC32,
	DSMCC_DESCRIPTOR_LOCATION,
	DSMCC_DESCRIPTOR_DLTIME,
	DSMCC_DESCRIPTOR_GROUPLINK,
	DSMCC_DESCRIPTOR_COMPRESSED,
	DSMCC_DESCRIPTOR_LABEL,
	DSMCC_DESCRIPTOR_CACHING_PRIORITY,
	DSMCC_DESCRIPTOR_CONTENT_TYPE
};

struct dsmcc_descriptor
{
	int type;
	union
	{
		struct dsmcc_descriptor_type type;
		struct dsmcc_descriptor_name name;
		struct dsmcc_descriptor_info info;
		struct dsmcc_descriptor_modlink modlink;
		struct dsmcc_descriptor_crc32 crc32;
		struct dsmcc_descriptor_location location;
		struct dsmcc_descriptor_dltime dltime;
		struct dsmcc_descriptor_grouplink grouplink;
		struct dsmcc_descriptor_compressed compressed;
		struct dsmcc_descriptor_label label;
		struct dsmcc_descriptor_caching_priority caching_priority;
		struct dsmcc_descriptor_content_type content_type;
	} data;

	struct dsmcc_descriptor *next;
};

int dsmcc_parse_descriptors(struct dsmcc_descriptor **descriptors, uint8_t *data, int data_length);
struct dsmcc_descriptor *dsmcc_find_descriptor_by_type(struct dsmcc_descriptor *descriptors, int type);
void dsmcc_descriptors_free_all(struct dsmcc_descriptor *descriptors);

#endif
