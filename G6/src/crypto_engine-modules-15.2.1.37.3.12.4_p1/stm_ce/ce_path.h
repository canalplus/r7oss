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

Source file name : ce_path.h

Declarations for tango HAL
************************************************************************/
#ifndef __CE_PATH_H
#define __CE_PATH_H

#include "stm_ce_objects.h"
#include "ce_hal.h"
#include "stm_registry.h"
#include "stm_ce_osal.h"

#include "stm_te_if_ce.h"
#include "mme.h"
#include "stm_ce_mme_share.h"

/* Initial path ID value */
#define CE_PATH_ID_NONE 0

/* Registry HAL-specific type object names */
#define STM_CE_PATH_SESSION_TYPE_NAME "tango_session"
#define STM_CE_PATH_TRANSFORM_TYPE_NAME "tango_transform"

/* stm_ce KTD platform device */
#define STM_CE_KTD_DEV_NAME "stm_ce-ktd"

/**
 * struct ce_hal_data - Stores HAL-specific static data
 */
struct ce_hal_data {
	MME_TransformerHandle_t mme_transformer;
	struct platform_device ktd_pdev;
};

/**
 * struct ce_session_data - Stores path HAL-specific data for a session
 * object
 */
struct ce_session_data {
};

/**
 * struct ce_transform_data - Stores path HAL-specific data for a transform
 * object
 */
struct ce_transform_data {
	/* List of Pid filter connections to this transform */
	struct list_head te_conn_list;
	uint32_t path_id;
};

/**
 * struct ce_te_conn - Stores information about a single connection between a
 * transform and a pid filter
 * @te_object:	stm_te PID filter object
 * @te_if:	stm_te pid interface for te_object
 * @lh:		List node (to link int parent transform's connection
 *		list)
 */
struct ce_te_conn {
	struct list_head lh;
	stm_object_h te_object;
	stm_te_pid_interface_t te_if;
	char tag[STM_REGISTRY_MAX_TAG_SIZE];
};

/* HAL registration, init, term functions */
int ce_path_hal_register(struct ce_hal *hal);
int ce_path_hal_unregister(struct ce_hal *hal);
int ce_path_hal_init(struct ce_hal *hal);
int ce_path_hal_term(struct ce_hal *hal);
int ce_path_hal_get_version(struct ce_hal *hal,
		stm_ce_version_t *versions, uint32_t max_versions,
		uint32_t *n_versions);

/* HAL APIs */
extern struct ce_session_api ce_path_session_api;
extern struct ce_transform_api ce_path_transform_api;

/* HAL interfaces */
extern stm_ce_path_interface_t ce_path_interface;

#endif
