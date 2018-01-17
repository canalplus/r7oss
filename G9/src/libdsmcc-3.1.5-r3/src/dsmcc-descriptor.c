#include <stdlib.h>
#include <string.h>

#include "dsmcc.h"
#include "dsmcc-descriptor.h"
#include "dsmcc-util.h"

struct dsmcc_descriptor *dsmcc_find_descriptor_by_type(struct dsmcc_descriptor *descriptors, int type)
{
	while (descriptors)
	{
		if (descriptors->type == type)
			break;
		descriptors = descriptors->next;
	}
	return descriptors;
}

void dsmcc_descriptors_free_all(struct dsmcc_descriptor *descriptors)
{
	while (descriptors)
	{
		struct dsmcc_descriptor *next = descriptors->next;
		/* free additional memory for some descriptor types */
		switch (descriptors->type) {
			case DSMCC_DESCRIPTOR_TYPE:
				free(descriptors->data.type.text);
				break;
			case DSMCC_DESCRIPTOR_NAME:
				free(descriptors->data.name.text);
				break;
			case DSMCC_DESCRIPTOR_INFO:
				free(descriptors->data.info.lang_code);
				free(descriptors->data.info.text);
				break;
			case DSMCC_DESCRIPTOR_LABEL:
				free(descriptors->data.label.text);
				break;
			case DSMCC_DESCRIPTOR_CONTENT_TYPE:
				free(descriptors->data.content_type.text);
				break;
		}
		free(descriptors);
		descriptors = next;
	}
}

static int parse_type_descriptor(struct dsmcc_descriptor *desc, uint8_t *data, int length)
{
	struct dsmcc_descriptor_type *type = &desc->data.type;

	desc->type = DSMCC_DESCRIPTOR_TYPE;
	if (!dsmcc_strdup(&type->text, length, data, 0, length))
		return -1;

	DSMCC_DEBUG("Type descriptor, text='%s'", type->text);
	return length;
}

static int parse_name_descriptor(struct dsmcc_descriptor *desc, uint8_t *data, int length)
{
	struct dsmcc_descriptor_name *name = &desc->data.name;

	desc->type = DSMCC_DESCRIPTOR_NAME;
	if (!dsmcc_strdup(&name->text, length, data, 0, length))
		return -1;

	DSMCC_DEBUG("Name descriptor, text='%s'", name->text);
	return length;
}

static int parse_info_descriptor(struct dsmcc_descriptor *desc, uint8_t *data, int length)
{
	struct dsmcc_descriptor_info *info = &desc->data.info;

	desc->type = DSMCC_DESCRIPTOR_INFO;
	if (!dsmcc_strdup(&info->lang_code, 3, data, 0, length))
		return -1;
	if (!dsmcc_strdup(&info->text, length - 3, data, 3, length))
	{
		free(info->lang_code);
		return -1;
	}

	DSMCC_DEBUG("Info descriptor, lang_code='%s' text='%s'", info->lang_code, info->text);
	return length;
}

static int parse_modlink_descriptor(struct dsmcc_descriptor *desc, uint8_t *data, int length)
{
	struct dsmcc_descriptor_modlink *modlink = &desc->data.modlink;

	desc->type = DSMCC_DESCRIPTOR_MODLINK;
	if (!dsmcc_getbyte(&modlink->position, data, 0, length))
		return -1;
	if (!dsmcc_getshort(&modlink->module_id, data, 1, length))
		return -1;

	DSMCC_DEBUG("Modlink descriptor, position=%hhu module_id=0x%04hx", modlink->position, modlink->module_id);
	return 3;
}

static int parse_crc32_descriptor(struct dsmcc_descriptor *desc, uint8_t *data, int length)
{
	struct dsmcc_descriptor_crc32 *crc32 = &desc->data.crc32;

	desc->type = DSMCC_DESCRIPTOR_CRC32;
	if (!dsmcc_getlong(&crc32->crc, data, 0, length))
		return -1;

	DSMCC_DEBUG("CRC32 descriptor, crc=0x%lx", crc32->crc);
	return 4;
}

static int parse_location_descriptor(struct dsmcc_descriptor *desc, uint8_t *data, int length)
{
	struct dsmcc_descriptor_location *location = &desc->data.location;

	desc->type = DSMCC_DESCRIPTOR_LOCATION;
	if (!dsmcc_getbyte(&location->location_tag, data, 0, length))
		return -1;

	DSMCC_DEBUG("Location descriptor, location_tag=%hhu", location->location_tag);
	return 1;
}

static int parse_dltime_descriptor(struct dsmcc_descriptor *desc, uint8_t *data, int length)
{
	struct dsmcc_descriptor_dltime *dltime = &desc->data.dltime;

	desc->type = DSMCC_DESCRIPTOR_DLTIME;
	if (!dsmcc_getlong(&dltime->download_time, data, 0, length))
		return -1;

	DSMCC_DEBUG("Dltime descriptor, download_time=%u", dltime->download_time);
	return 4;
}

static int parse_grouplink_descriptor(struct dsmcc_descriptor *desc, uint8_t *data, int length)
{
	struct dsmcc_descriptor_grouplink *grouplink = &desc->data.grouplink;

	desc->type = DSMCC_DESCRIPTOR_GROUPLINK;
	if (!dsmcc_getbyte(&grouplink->position, data, 0, length))
		return -1;
	if (!dsmcc_getlong(&grouplink->group_id, data, 1, length))
		return -1;

	DSMCC_DEBUG("Grouplink descriptor, position=%hhu group_id=0x%04hx", grouplink->position, grouplink->group_id);
	return 5;
}

static int parse_compressed_descriptor(struct dsmcc_descriptor *desc, uint8_t *data, int length)
{
	struct dsmcc_descriptor_compressed *compressed = &desc->data.compressed;

	desc->type = DSMCC_DESCRIPTOR_COMPRESSED;
	if (!dsmcc_getbyte(&compressed->method, data, 0, length))
		return -1;
	if (!dsmcc_getlong(&compressed->original_size, data, 1, length))
		return -1;

	DSMCC_DEBUG("Compressed descriptor, method=%hhu original_size=%u", compressed->method, compressed->original_size);
	return 5;
}

static int parse_label_descriptor(struct dsmcc_descriptor *desc, uint8_t *data, int length)
{
	struct dsmcc_descriptor_label *label = &desc->data.label;

	desc->type = DSMCC_DESCRIPTOR_LABEL;
	if (!dsmcc_strdup(&label->text, length, data, 0, length))
		return -1;

	DSMCC_DEBUG("Label descriptor, text='%s'", label->text);
	return length;
}

static int parse_caching_priority_descriptor(struct dsmcc_descriptor *desc, uint8_t *data, int length)
{
	struct dsmcc_descriptor_caching_priority *caching_priority = &desc->data.caching_priority;

	desc->type = DSMCC_DESCRIPTOR_CACHING_PRIORITY;
	if (!dsmcc_getbyte(&caching_priority->priority_value, data, 0, length))
		return -1;
	if (!dsmcc_getbyte(&caching_priority->transparency_level, data, 1, length))
		return -1;

	DSMCC_DEBUG("Caching priority descriptor, priority_value=%hhu, transparency_level=%hhu", caching_priority->priority_value, caching_priority->transparency_level);
	return 2;
}

static int parse_content_type_descriptor(struct dsmcc_descriptor *desc, uint8_t *data, int length)
{
	struct dsmcc_descriptor_content_type *content_type = &desc->data.content_type;

	desc->type = DSMCC_DESCRIPTOR_LABEL;
	if (!dsmcc_strdup(&content_type->text, length, data, 0, length))
		return -1;

	DSMCC_DEBUG("Content type descriptor, text='%s'", content_type->text);
	return length;
}

static int parse_descriptor(struct dsmcc_descriptor **descriptor, uint8_t *data, int data_length)
{
	int off = 0, ret;
	struct dsmcc_descriptor *desc;
	unsigned char tag, length;

	desc = malloc(sizeof(struct dsmcc_descriptor));
	if (!dsmcc_getbyte(&tag, data, off, data_length))
		goto error;
	off++;
	if (!dsmcc_getbyte(&length, data, off, data_length))
		goto error;
	off++;

	if (length > data_length - off)
	{
		DSMCC_ERROR("Buffer overflow while parsing descriptors (got %d bytes but need %d)", data_length - off, length);
		goto error;
	}

	switch(tag) {
		case 0x01:
			ret = parse_type_descriptor(desc, data + off, length);
			break;
		case 0x02:
			ret = parse_name_descriptor(desc, data + off, length);
			break;
		case 0x03:
			ret = parse_info_descriptor(desc, data + off, length);
			break;
		case 0x04:
			ret = parse_modlink_descriptor(desc, data + off, length);
			break;
		case 0x05:
			ret = parse_crc32_descriptor(desc, data + off, length);
			break;
		case 0x06:
			ret = parse_location_descriptor(desc, data + off, length);
			break;
		case 0x07:
			ret = parse_dltime_descriptor(desc, data + off, length);
			break;
		case 0x08:
			ret = parse_grouplink_descriptor(desc, data + off, length);
			break;
		case 0x09:
			ret = parse_compressed_descriptor(desc, data + off, length);
			break;
		case 0x70:
			ret = parse_label_descriptor(desc, data + off, length);
			break;
		case 0x71:
			ret = parse_caching_priority_descriptor(desc, data + off, length);
			break;
		case 0x72:
			ret = parse_content_type_descriptor(desc, data + off, length);
			break;
		default:
			DSMCC_WARN("Unknown/Unhandled descriptor, Tag 0x%02x Length %d", tag, length);
			ret = length;
			free(desc);
			desc = NULL;
			break;
	}

	if (ret < 0)
		goto error;

	if (ret < length)
		DSMCC_WARN("Trailing data after descriptor (Tag 0x%02x Length %d, parsed %d bytes)", tag, length, ret);

	off += length;
	*descriptor = desc;
	return off;
error:
	free(desc);
	desc = NULL;
	return -1;
}

int dsmcc_parse_descriptors(struct dsmcc_descriptor **descriptors, uint8_t *data, int data_length)
{
	int off = 0, ret;
	struct dsmcc_descriptor *list_head, *list_tail;

	list_head = list_tail = NULL;

	while (off < data_length)
	{
		struct dsmcc_descriptor *desc = NULL;
		ret = parse_descriptor(&desc, data + off, data_length - off);
		if (ret < 0)
		{
			dsmcc_descriptors_free_all(list_head);
			return -1;
		}
		off += ret;

		if (desc)
		{
			desc->next = NULL;
			if (!list_tail)
			{
				list_tail = desc;
				list_head = list_tail;
			}
			else
			{
				list_tail->next = desc;
				list_tail = desc;
			}
		}
	}

	if (off < data_length)
		DSMCC_WARN("Trailing data after descriptors (%d bytes remaining)", data_length - off);

	*descriptors = list_head;
	return off;
}
