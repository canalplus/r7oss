#define _GNU_SOURCE
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "dsmcc.h"
#include "dsmcc-descriptor.h"
#include "dsmcc-biop-ior.h"
#include "dsmcc-biop-module.h"
#include "dsmcc-biop-message.h"
#include "dsmcc-cache-module.h"
#include "dsmcc-cache-file.h"
#include "dsmcc-util.h"
#include "dsmcc-carousel.h"
#include "dsmcc-gii.h"

struct dsmcc_section_header
{
	uint8_t  table_id;
	uint16_t table_id_extension;
	uint16_t length;
};

struct dsmcc_message_header
{
	uint16_t message_id;
	uint32_t transaction_id;
	uint16_t message_length;
};

struct dsmcc_data_header
{
	uint32_t download_id;
	uint16_t message_length;
};

/**
  * returns number of bytes to skip to get to next data or -1 on error
  */
static int parse_section_header(struct dsmcc_section_header *header, uint8_t *data, int data_length)
{
	int off = 0;
	int section_syntax_indicator;
	int private_indicator;
	int crc;

	if (!dsmcc_getbyte(&header->table_id, data, off, data_length))
		return -1;
	off++;

	if (!dsmcc_getshort(&header->length, data, off, data_length))
		return -1;
	off += 2;
	section_syntax_indicator = ((header->length & 0x8000) != 0);
	private_indicator = ((header->length & 0x4000) != 0);
	header->length &= 0xFFF;

	/* Check CRC is set and private_indicator is set to its complement, else skip packet */
	if (!(section_syntax_indicator ^ private_indicator))
	{
		DSMCC_ERROR("Invalid section header: section_syntax_indicator and private_indicator flags are not complements (%d/%d)", section_syntax_indicator, private_indicator);
		return -1;
	}

	/* Check CRC */
	crc = dsmcc_crc32(data, header->length + off);
	if (crc != 0)
	{
		DSMCC_ERROR("Dropping corrupt section (Got CRC 0x%08x)", crc);
		return -1;
	}

	if (!dsmcc_getshort(&header->table_id_extension, data, off, data_length))
		return -1;
	off += 2;

	/* skip unused fields */
	off += 3;

	/* adjust section length for header fields after length field (table ID extension and unused fields */
	header->length -= 5;

	return off;
}

/*
 * returns number of bytes to skip to get to next data or -1 on error
 * ETSI TR 101 202 Table A.1
 */
static int parse_message_header(struct dsmcc_message_header *header, uint8_t *data, int data_length)
{
	int off = 0;
	uint8_t protocol, type, adaptation_length;

	if (!dsmcc_getbyte(&protocol, data, off, data_length))
		return -1;
	off++;
	if (protocol != 0x11)
	{
		DSMCC_ERROR("Message Header: invalid protocol 0x%02hhx (expected 0x11)", protocol);
		return -1;
	}

	if (!dsmcc_getbyte(&type, data, off, data_length))
		return -1;
	off++;
	if (type != 0x3)
	{
		DSMCC_ERROR("Message Header: invalid type 0x%02hhx (expected 0x03)", protocol);
		return -1;
	}

	if (!dsmcc_getshort(&header->message_id, data, off, data_length))
		return -1;
	off += 2;
	DSMCC_DEBUG("Message Header: MessageID 0x%hx", header->message_id);

	if (!dsmcc_getlong(&header->transaction_id, data, off, data_length))
		return -1;
	off += 4;
	DSMCC_DEBUG("Message Header: TransactionID 0x%x", header->transaction_id);

	/* skip reserved byte */
	off += 1;

	if (!dsmcc_getbyte(&adaptation_length, data, off, data_length))
		return -1;
	off++;
	DSMCC_DEBUG("Message Header: Adaptation Length %hhu", adaptation_length);

	if (!dsmcc_getshort(&header->message_length, data, off, data_length))
		return -1;
	off += 2;
	header->message_length -= adaptation_length;
	DSMCC_DEBUG("Message Header: Message Length %hu (excluding adaption header)", header->message_length);

	/* skip adaptation header */
	off += adaptation_length;

	return off;
}

/*
 * ETSI TR 101 202 Table 4.15
 */
static int parse_service_gateway_info(struct biop_ior *gateway_ior, uint8_t *data, int data_length)
{
	int off = 0, ret;
	uint8_t tmp;

	ret = dsmcc_biop_parse_ior(gateway_ior, data + off, data_length - off);
	if (ret < 0)
		return -1;
	off += ret;

	if (gateway_ior->type != IOR_TYPE_DSM_SERVICE_GATEWAY)
	{
		DSMCC_ERROR("Service Gateway: Expected an IOR of type DSM:ServiceGateway, but got \"%s\"", dsmcc_biop_get_ior_type_str(gateway_ior->type));
		return -1;
	}

	/* Download Taps count, should be 0 */
	if (!dsmcc_getbyte(&tmp, data, off, data_length))
		return -1;
	off++;
	if (tmp != 0)
	{
		DSMCC_ERROR("Service Gateway: Download Taps count should be 0 but is %hhu", tmp);
		return -1;
	}

	/* Service Context List count, should be 0 */
	if (!dsmcc_getbyte(&tmp, data, off, data_length))
		return -1;
	off++;
	if (tmp != 0)
	{
		DSMCC_ERROR("Service Gateway: Service Context List count should be 0 but is %hhu", tmp);
		return -1;
	}

	/* TODO parse descriptors in user_data, for now just skip it */
	if (!dsmcc_getbyte(&tmp, data, off, data_length))
		return -1;
	off++;
	off += tmp;

	return off;
}

/*
 * ETSI TR 101 202 Table A.3
 */
static int parse_section_dsi(struct dsmcc_object_carousel *carousel, uint8_t *data, int data_length)
{
        int off = 0, ret;
	uint16_t i, dsi_data_length;
	struct biop_ior gateway_ior;
	struct dsmcc_stream *stream = NULL;

	/* skip unused Server ID */
	/* 0-19 Server id = 20 * 0xFF */
	off += 20;

	/* compatibility descriptor length, should be 0 */
	if (!dsmcc_getshort(&i, data, off, data_length))
		return -1;
	off += 2;
	if (i != 0)
	{
		DSMCC_ERROR("DSI: Compatibility descriptor length should be 0 but is %hu", i);
		return -1;
	}

	if (!dsmcc_getshort(&dsi_data_length, data, off, data_length))
		return -1;
	off += 2;
	DSMCC_DEBUG("DSI: Data Length %hu", dsi_data_length);

	if(carousel->type == DSMCC_OBJECT_CAROUSEL)
	{
		DSMCC_DEBUG("DSI: Processing BIOP::ServiceGatewayInfo...");
		memset(&gateway_ior, 0, sizeof(struct biop_ior));
		ret = parse_service_gateway_info(&gateway_ior, data + off, dsmcc_min(data_length - off, dsi_data_length));
		if (ret < 0)
			return -1;
		off += dsi_data_length;

		/* Check if carousel was updated */
		if (carousel->dsi_transaction_id != 0xFFFFFFFF)
		{
			if (carousel->cid != gateway_ior.profile_body.obj_loc.carousel_id)
			{
				DSMCC_ERROR("DSI: Ignoring gateway info, wrong carousel ID (got 0x%x but expected 0x%x)", gateway_ior.profile_body.obj_loc.carousel_id, carousel->cid);
				return -1;
			}

			dsmcc_stream_queue_remove(carousel, DSMCC_QUEUE_ENTRY_DII);
		}
		else
		{
			/* Set carousel ID and DSI Transaction ID */
			carousel->cid = gateway_ior.profile_body.obj_loc.carousel_id;
		}

		/* Queue entry for DII */
		stream = dsmcc_stream_queue_add(carousel,
				DSMCC_STREAM_SELECTOR_ASSOC_TAG, gateway_ior.profile_body.conn_binder.assoc_tag,
				DSMCC_QUEUE_ENTRY_DII, gateway_ior.profile_body.conn_binder.transaction_id);

	}
	else // DSMCC_DATA_CAROUSEL
	{
		struct dsmcc_group_list *groups, *pg;
		ret = dsmcc_group_info_indication_parse(&groups, data + off, dsmcc_min(data_length - off, dsi_data_length));
		if(ret < 0)
			return -1;

		dsmcc_stream_queue_remove(carousel, DSMCC_QUEUE_ENTRY_DII);
		dsmcc_stream_queue_remove(carousel, DSMCC_QUEUE_ENTRY_DDB);

		pg = groups;
		while(pg)
		{
			DSMCC_DEBUG("datacarousel: add group %d\n",pg->id);
			stream = dsmcc_stream_queue_add(carousel, DSMCC_STREAM_SELECTOR_PID, carousel->requested_pid, DSMCC_QUEUE_ENTRY_DII, pg->id);
			pg = pg->next;
		}
		dsmcc_cache_remove_unneeded_modules_by_group(carousel, groups);
		dsmcc_group_info_indication_free(carousel->group_list);
		carousel->group_list = groups;
		off += dsi_data_length;
	}


	/* add section filter on stream for DII (table_id == 0x3B, table_id_extension != 0x0000 or 0x0001) */
	if (carousel->state->callbacks.add_section_filter && stream)
	{
		uint8_t pattern[3]  = { 0x3B, 0x00, 0x00 };
		uint8_t equal[3]    = { 0xff, 0x00, 0x00 };
		uint8_t notequal[3] = { 0x00, 0xff, 0xfe };
		(*carousel->state->callbacks.add_section_filter)(carousel->state->callbacks.add_section_filter_arg, stream->pid, pattern, equal, notequal, 3);
	}
	/* update timeouts */
	dsmcc_timeout_remove(carousel, DSMCC_TIMEOUT_DSI, 0);
	dsmcc_timeout_set(carousel, DSMCC_TIMEOUT_DII, 0, gateway_ior.profile_body.conn_binder.timeout);

	return off;
}

/*
 * ETSI TR 101 202 Table A.4
 */
static int parse_section_dii(struct dsmcc_object_carousel *carousel, uint8_t *data, int data_length, uint32_t dii_transaction_id)
{
	int off = 0, ret;
	uint16_t i, number_modules;
	uint32_t download_id;
	uint16_t block_size;
	struct dsmcc_module_id *modules_id;
	struct dsmcc_module_info *modules_info;
	uint16_t *assoc_tags;
	uint32_t total_size;

	if (!dsmcc_getlong(&download_id, data, off, data_length))
		return -1;
	off += 4;
	DSMCC_DEBUG("DII: Download ID 0x%x", download_id);

	if (!dsmcc_getshort(&block_size, data, off, data_length))
		return -1;
	off += 2;
	DSMCC_DEBUG("DII: Block Size %hu", block_size);

	/* skip unused fields */
	off += 10;

	/* ignore compatibility descriptor */
	if (!dsmcc_getshort(&i, data, off, data_length))
		return -1;
	off += 2 + i;

	if (!dsmcc_getshort(&number_modules, data, off, data_length))
		return -1;
	off += 2;
	DSMCC_DEBUG("DII: Number of modules %hu", number_modules);

	modules_id = calloc(number_modules, sizeof(struct dsmcc_module_id));
	modules_info = calloc(number_modules, sizeof(struct dsmcc_module_info));
	assoc_tags = calloc(number_modules, sizeof(uint16_t));
	for (i = 0; i < number_modules; i++)
	{
		struct biop_module_info *bmi;
		struct dsmcc_descriptor *desc;
		uint8_t module_info_length;

		modules_id[i].download_id = download_id;
		modules_id[i].dii_transaction_id = dii_transaction_id;
		modules_info[i].block_size = block_size;

		if (!dsmcc_getshort(&modules_id[i].module_id, data, off, data_length))
			goto error;
		off += 2;
		if (!dsmcc_getlong(&modules_info[i].module_size, data, off, data_length))
			goto error;
		off += 4;
		if (!dsmcc_getbyte(&modules_id[i].module_version, data, off, data_length))
			goto error;
		off++;
		if (!dsmcc_getbyte(&module_info_length, data, off, data_length))
			goto error;
		off++;

		DSMCC_DEBUG("DII: Module ID 0x%04hx Size %u Version 0x%02hhx", modules_id[i].module_id, modules_info[i].module_size, modules_id[i].module_version);

		if(carousel->type == DSMCC_OBJECT_CAROUSEL)
		{
			bmi = malloc(sizeof(struct biop_module_info));
			ret = dsmcc_biop_parse_module_info(bmi, data + off, dsmcc_min(data_length - off, module_info_length));
			if (ret < 0)
			{
				dsmcc_biop_free_module_info(bmi);
				goto error;
			}
			off += module_info_length;

			modules_info[i].mod_timeout = bmi->mod_timeout;
			modules_info[i].block_timeout = bmi->block_timeout;

			assoc_tags[i] = bmi->assoc_tag;
			desc = dsmcc_find_descriptor_by_type(bmi->descriptors, DSMCC_DESCRIPTOR_COMPRESSED);
			if (desc)
			{
				modules_info[i].compressed = 1;
				modules_info[i].compress_method = desc->data.compressed.method;
				modules_info[i].uncompressed_size = desc->data.compressed.original_size;
			}
			dsmcc_biop_free_module_info(bmi);
		}
		else
		{
			if(module_info_length)
				DSMCC_WARN("skiping module info");
			off += module_info_length;
			modules_info[i].mod_timeout = 0xFFFFFFFF;
			modules_info[i].block_timeout = 0xFFFFFFFF;

		}

	}

	/* skip private_data */
	if (!dsmcc_getshort(&i, data, off, data_length))
		goto error;
	off += 2;
	DSMCC_DEBUG("DII: Private Data Length %hhu", i);
	off += i;

	if(carousel->type == DSMCC_OBJECT_CAROUSEL)
	{
		/* remove outdated modules and files */
		dsmcc_stream_queue_remove(carousel, DSMCC_QUEUE_ENTRY_DDB);
		dsmcc_filecache_clear_all(carousel);
		dsmcc_cache_remove_unneeded_modules(carousel, modules_id, number_modules);
	}

	/* add modules info to module cache */
	total_size = 0;
	for (i = 0; i < number_modules; i++)
	{
		struct dsmcc_stream *stream;

		total_size += modules_info[i].module_size;
		if (!dsmcc_cache_add_module_info(carousel, &modules_id[i], &modules_info[i]))
		{
			/* Queue entry for DDBs */
			if(carousel->type == DSMCC_OBJECT_CAROUSEL)
			{
				stream = dsmcc_stream_queue_add(carousel,
						DSMCC_STREAM_SELECTOR_ASSOC_TAG, assoc_tags[i],
						DSMCC_QUEUE_ENTRY_DDB, download_id);
			}
			else
			{
				stream = dsmcc_stream_queue_add(carousel,
						DSMCC_STREAM_SELECTOR_PID, carousel->requested_pid,
						DSMCC_QUEUE_ENTRY_DDB, download_id);
			}
			/* add section filter on stream for DDB (table_id == 0x3C, table_id_extension == module_id, version_number == module_version % 32) */
			if (carousel->state->callbacks.add_section_filter)
			{
				uint8_t pattern[4];
				uint8_t equal[4]    = { 0xff, 0xff, 0xff, 0x3e }; /* bits 2-6 */
				uint8_t notequal[4] = { 0x00, 0x00, 0x00, 0x00 };
				pattern[0] = 0x3C;
				pattern[1] = (modules_id[i].module_id >> 8) & 0xff;
				pattern[2] = modules_id[i].module_id & 0xff;
				pattern[3] = (modules_id[i].module_version & 0x1f) << 1;
				(*carousel->state->callbacks.add_section_filter)(carousel->state->callbacks.add_section_filter_arg, stream->pid, pattern, equal, notequal, 4);
			}

			/* add module timeout */
			dsmcc_timeout_set(carousel, DSMCC_TIMEOUT_MODULE, modules_id[i].module_id, modules_info[i].mod_timeout);
		}
	}

	/* remove DII timeout */
	dsmcc_timeout_remove(carousel, DSMCC_TIMEOUT_DII, 0);

	free(modules_id);
	free(modules_info);
	free(assoc_tags);

	return off;

error:
	free(modules_id);
	free(modules_info);
	free(assoc_tags);
	return -1;
}

static int parse_section_control(struct dsmcc_stream *stream, uint8_t *data, int data_length)
{
	struct dsmcc_message_header header;
	int off = 0, ret;
	struct dsmcc_object_carousel *carousel;
	struct dsmcc_group_list *grp;

	ret = parse_message_header(&header, data, data_length);
	if (ret < 0)
		return -1;
	off += ret;

	switch (header.message_id)
	{
		case 0x1006:
			DSMCC_DEBUG("Processing Download-ServerInitiate message for stream with PID 0x%hx", stream->pid);
			carousel = dsmcc_stream_queue_find(stream, DSMCC_QUEUE_ENTRY_DSI, header.transaction_id);
			if (carousel)
			{
				if (carousel->dsi_transaction_id != header.transaction_id)
				{
					ret = parse_section_dsi(carousel, data + off, data_length - off);
					if (ret < 0)
						return -1;
					carousel->dsi_transaction_id = header.transaction_id;
				}
				else
					DSMCC_DEBUG("Ignoring duplicate DSI with Transaction ID 0x%x", header.transaction_id);
			}
			else
				DSMCC_DEBUG("Skipping unrequested DSI");
			break;
		case 0x1002:
			DSMCC_DEBUG("Processing Download-InfoIndication message for stream with PID 0x%hx", stream->pid);
			carousel = dsmcc_stream_queue_find(stream, DSMCC_QUEUE_ENTRY_DII, header.transaction_id);
			if (carousel)
			{
				if(carousel->type == DSMCC_OBJECT_CAROUSEL)
				{
					if (carousel->dii_transaction_id != header.transaction_id)
					{
						ret = parse_section_dii(carousel, data + off, data_length - off, header.transaction_id);
						if (ret < 0)
							return -1;
						carousel->dii_transaction_id = header.transaction_id;
					}
					else
						DSMCC_DEBUG("Ignoring duplicate DII with Transaction ID 0x%x", header.transaction_id);
				}
				else
				{
					for (grp = carousel->group_list; grp; grp = grp->next)
					{
						if (header.transaction_id == grp->id)
						{
							if(grp->parsed)
							{
								DSMCC_DEBUG("Ignoring duplicate DII with Transaction ID 0x%x", header.transaction_id);
							}
							else
							{
								ret = parse_section_dii(carousel, data + off, data_length - off, header.transaction_id);
								if (ret < 0)
									return -1;
								grp->parsed = 1;
							}

							break;
						}

					}

					if(!grp)
						DSMCC_DEBUG("Can't find DII with Transaction ID 0x%x", header.transaction_id);

				}
			}
			else
				DSMCC_DEBUG("Skipping unrequested DII");
			break;
		default:
			DSMCC_ERROR("Unknown message ID (0x%x)", header.message_id);
			return -1;
	}

	off += header.message_length;

	return off;
}

/*
 * ETSI TR 101 202 Table A.2
 */
static int parse_data_header(struct dsmcc_data_header *header, uint8_t *data, int data_length)
{
	int off = 0;
	uint8_t protocol, type, adaptation_length;
	uint16_t message_id;

	if (!dsmcc_getbyte(&protocol, data, off, data_length))
		return -1;
	off++;
	if (protocol != 0x11)
	{
		DSMCC_ERROR("Data Header: invalid protocol 0x%hhx (expected 0x%x)", protocol, 0x11);
		return -1;
	}

	if (!dsmcc_getbyte(&type, data, off, data_length))
		return -1;
	off++;
	if (type != 0x3)
	{
		DSMCC_ERROR("Data Header: invalid type 0x%hhx (expected 0x%x)", type, 0x3);
		return -1;
	}

	if (!dsmcc_getshort(&message_id, data, off, data_length))
		return -1;
	off += 2;
	if (message_id != 0x1003)
	{
		DSMCC_ERROR("Data Header: invalid message ID 0x%hx (expected 0x%x)", message_id, 0x1003);
		return -1;
	}

	if (!dsmcc_getlong(&header->download_id, data, off, data_length))
		return -1;
	off += 4;
	DSMCC_DEBUG("Data Header: Download ID 0x%x", header->download_id);

	/* skip reserved byte */
	off += 1;

	if (!dsmcc_getbyte(&adaptation_length, data, off, data_length))
		return -1;
	off++;
	DSMCC_DEBUG("Data Header: Adaptation Length %hhu", adaptation_length);

	if (!dsmcc_getshort(&header->message_length, data, off, data_length))
		return -1;
	off += 2;
	header->message_length -= adaptation_length;
	DSMCC_DEBUG("Data Header: Message Length %hu (excluding adaption header)", header->message_length);

	/* skip adaptation header */
	off += adaptation_length;

	return off;
}

/*
 * ETSI TR 101 202 Table A.5
 */
static int parse_section_ddb(struct dsmcc_object_carousel *carousel, struct dsmcc_data_header *header, uint8_t *data, int data_length)
{
	int off = 0;
	struct dsmcc_module_id module_id;
	uint16_t block_number;
	int length;

	module_id.download_id = header->download_id;

	if (!dsmcc_getshort(&module_id.module_id, data, off, data_length))
		return -1;
	off += 2;
	DSMCC_DEBUG("DDB: Module ID 0x%04hx", module_id.module_id);

	if (!dsmcc_getbyte(&module_id.module_version, data, off, data_length))
		return -1;
	off++;
	DSMCC_DEBUG("DDB: Module Version %u", module_id.module_version);

	/* skip reserved byte */
	off++;

	if (!dsmcc_getshort(&block_number, data, off, data_length))
		return -1;
	off += 2;
	DSMCC_DEBUG("DDB: Block Number %u", block_number);

	length = header->message_length - off;
	DSMCC_DEBUG("DDB: Block Length %d", length);

	/* Check that we have enough data in buffer */
	if (length > data_length - off)
	{
		DSMCC_ERROR("DDB: Data buffer overflow (need %d bytes but only got %d)", length, data_length - off);
		return -1;
	}

	dsmcc_cache_save_module_data(carousel, &module_id, block_number, data + off, length);
	off += length;

	return off;
}

static int parse_section_data(struct dsmcc_stream *stream, uint8_t *data, int data_length)
{
	struct dsmcc_data_header header;
	int off = 0, ret;
	struct dsmcc_object_carousel *carousel;

	DSMCC_DEBUG("Parsing DDB section for stream with PID 0x%hx", stream->pid);

	ret = parse_data_header(&header, data, data_length);
	if (ret < 0)
		return -1;
	off += ret;

	carousel = dsmcc_stream_queue_find(stream, DSMCC_QUEUE_ENTRY_DDB, header.download_id);
	if (carousel)
	{
		ret = parse_section_ddb(carousel, &header, data + off, data_length - off);
		if (ret < 0)
			return -1;
	}
	else
		DSMCC_DEBUG("Skipping unrequested DDB");

	off += header.message_length;

	return off;
}

int dsmcc_parse_section(struct dsmcc_state *state, struct dsmcc_section *section)
{
	int off = 0, ret;
	struct dsmcc_section_header header;
	struct dsmcc_stream *stream;

	stream = dsmcc_stream_find_by_pid(state, section->pid);
	if (!stream)
	{
		DSMCC_WARN("Skipping section for unknown PID 0x%hx", section->pid);
		return 0;
	}

	ret = parse_section_header(&header, section->data, section->length);
	if (ret < 0)
		return 0;
	off += ret;

	if (header.length > section->length - off)
	{
		DSMCC_ERROR("Data buffer overflow (need %hu bytes but only got %d)", header.length, section->length - off);
		return 0;
	}

	DSMCC_DEBUG("Processing section: PID 0x%hx length %hu", section->pid, header.length);

	switch (header.table_id)
	{
		case 0x3B:
			DSMCC_DEBUG("DSI/DII Section");
			ret = parse_section_control(stream, section->data + off, header.length);
			if (ret < 0)
				return 0;
			break;
		case 0x3C:
			DSMCC_DEBUG("DDB Section");
			ret = parse_section_data(stream, section->data + off, header.length);
			if (ret < 0)
				return 0;
			break;
		default:
			DSMCC_ERROR("Unknown section (table ID is 0x%02x)", header.table_id);
			return 0;
	}

	return 1;
}

