/************************************************************************
Copyright (C) 2011,2012 STMicroelectronics. All Rights Reserved.

This file is part of the Crypto_engine Library.

Crypto_engine is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License version 2 as published by the
Free Software Foundation.

Crypto_engine is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with Crypto_engine; see the file COPYING.  If not, write to the Free Software
Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

The Crypto_engine Library may alternatively be licensed under a proprietary
license from ST.

Source file name : ce_hal.h

Declares stm_ce HAL APIs and HAL management functions
************************************************************************/
#ifndef __CE_HAL_H
#define __CE_HAL_H

#include "stm_ce.h"
#include "stm_ce_objects.h"

/* Struct holding a ce session internal api */
struct ce_session_api {
	int (*new)(struct ce_session *session);
	int (*del)(struct ce_session *session);
	int (*get_control)(struct ce_session *session,
			const stm_ce_session_ctrl_t selector,
			uint32_t *value);
	int (*set_control)(struct ce_session *session,
			const stm_ce_session_ctrl_t selector,
			const uint32_t value);
	int (*set_compound_control)(struct ce_session *session,
			const uint32_t selector, void *args);
	int (*get_capabilities)(struct ce_session *session,
			stm_ce_session_caps_t *caps);
	int (*set_rule_data)(struct ce_session *session,
			const int32_t key_rule_id,
			const stm_ce_buffer_t *buffers);
	int (*print)(struct ce_session *session, char *print_buffer,
			uint32_t buffer_size);
};

/* Struct holding a ce transform internal api */
struct ce_transform_api {
	int (*new)(struct ce_transform *transform, stm_ce_transform_type_t type,
			const uint32_t n_stages);
	int (*del)(struct ce_transform *transform);
	int (*get_control)(struct ce_transform *transform,
			const stm_ce_transform_ctrl_t selector,
			uint32_t *value);
	int (*set_control)(struct ce_transform *transform,
			const stm_ce_transform_ctrl_t selector,
			const uint32_t value);
	int (*set_compound_control)(struct ce_transform *transform,
			const uint32_t selector, void *args);
	int (*get_capabilities)(struct ce_transform *transform,
			stm_ce_transform_caps_t *caps);
	int (*set_config)(struct ce_transform *transform,
			const uint32_t stage,
			const stm_ce_transform_config_t *config);
	int (*set_key)(struct ce_transform *transform,
			const uint32_t stage,
			const stm_ce_buffer_t *key,
			const int32_t key_number);
	int (*set_key_Ex)(struct ce_transform *transform,
				const uint32_t stage,
				const stm_ce_buffer_t *key,
				const int32_t key_number,
				const stm_ce_key_operation_t keyop);
	int (*set_iv)(struct ce_transform *transform,
			const uint32_t stage,
			const stm_ce_buffer_t *iv,
			const int32_t key_number);
	int (*dma)(struct ce_transform *transform,
			const stm_ce_buffer_t *src, const uint32_t n_src,
			const stm_ce_buffer_t *dst, const uint32_t n_dst);
	int (*print)(struct ce_transform *transform, char *print_buffer,
			size_t buffer_size);
};

/* Opaque (to API layer) struct containing HAL-specific data */
struct ce_hal_data;

/* Struct containing a single HAL API */
struct ce_hal {
	uint32_t id;
	char name[STM_REGISTRY_MAX_TAG_SIZE];
	int8_t initialized;
	int (*reg)(struct ce_hal *hal);
	int (*unreg)(struct ce_hal *hal);
	int (*init)(struct ce_hal *hal);
	int (*term)(struct ce_hal *hal);
	int (*get_version)(struct ce_hal *hal,
			stm_ce_version_t *versions,
			uint32_t max_versions, uint32_t *n_versions);
	struct ce_session_api *session_api;
	struct ce_transform_api *transform_api;

	stm_object_h session_type;
	stm_object_h transform_type;

	/* Pointer to HAL-specific data */
	struct ce_hal_data *hal_data;
};

/* HAL management functions */
int ce_hal_register_all(void);
int ce_hal_get_initialized(uint32_t id, struct ce_hal **hal);
int ce_hal_unregister_all(void);
int ce_hal_get_version(stm_ce_version_t *version, uint32_t max_versions,
		uint32_t *n_versions);

#endif
