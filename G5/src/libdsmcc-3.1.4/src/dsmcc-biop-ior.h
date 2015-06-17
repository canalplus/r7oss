#ifndef DSMCC_BIOP_IOR_H
#define DSMCC_BIOP_IOR_H

#include <stdint.h>

struct biop_dsm_conn_binder
{
	uint16_t assoc_tag;
	uint32_t transaction_id;
	uint32_t timeout;
};

struct biop_obj_location
{
	uint32_t carousel_id;
	uint16_t module_id;
	uint32_t key;
	uint32_t key_mask;
};

struct biop_profile_body
{
	struct biop_obj_location    obj_loc;
	struct biop_dsm_conn_binder conn_binder;
};

enum
{
	IOR_TYPE_DSM_DIRECTORY = 0,
	IOR_TYPE_DSM_FILE,
	IOR_TYPE_DSM_STREAM,
	IOR_TYPE_DSM_SERVICE_GATEWAY,
	IOR_TYPE_DSM_STREAM_EVENT
};

struct biop_ior
{
	int                      type;
	struct biop_profile_body profile_body;
};

int dsmcc_biop_parse_ior(struct biop_ior *ior, uint8_t *data, int data_length);
const char *dsmcc_biop_get_ior_type_str(int type);

#endif /* DSMCC_BIOP_IOR_H */
