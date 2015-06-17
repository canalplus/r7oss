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

Source file name : stm_ce_objects.h

Declares stm_ce internal object types and names
************************************************************************/

#ifndef __STM_CE_OBJECTS_H
#define __STM_CE_OBJECTS_H

#include "stm_registry.h"
#include "stm_ce.h"
#include "stm_ce_osal.h"
#include "stm_ce_key_rules.h"
#include "stm_data_interface.h"
#include "ce_dt.h"

/* Type names, used with the stm_registry */
#define STM_CE_TYPE_NAME "stm_ce_class"
#define STM_CE_SESSION_TYPE_NAME "session_class"
#define STM_CE_TRANSFORM_TYPE_NAME "transform_class"

/* Instance names, used with the stm_registry */
#define STM_CE_INSTANCE_NAME "stm_ce"
#define STM_CE_SESSION_INSTANCE_NAME "session"
#define STM_CE_TRANSFORM_INSTANCE_NAME "transform"

/* Print handler names, used with stm_registry */
#define STM_CE_SESSION_PRINT_TYPE "stm_ce.session.print"
#define STM_CE_SESSION_PRINT_NAME "debug"
#define STM_CE_TRANSFORM_PRINT_TYPE "stm_ce.transform.print"
#define STM_CE_TRANSFORM_PRINT_NAME "debug"

/* Name used for stm_ce session platform device for loading key rules */
#define STM_CE_SESSION_PDEV_NAME "stm_ce_session"

/* Default session scheme name */
#include <stm_ce_sp/scheme.h>

/* Maximum number of global version strings (includes all HAL/slave/firmware
 * versions) */
#define STM_CE_MAX_VERSIONS 6

#define STM_CE_SESSION_MAGIC (0xCAFECAFE)
/* Struct used to store instance attributes for session objects */
struct ce_session {
	/* Fixed value. Identifies this as a session object. */
	uint32_t magic;

	/* Points to itself. Used to check validity */
	struct ce_session *self;

	/* Pointer to HAL API for this session */
	struct ce_session_api *api;

	/* Pointer to HAL-specific data (opaque to API layer) */
	struct ce_session_data *data;

	/* Platform device (used for firmware loading for this session */
	struct platform_device pdev;
	char pdev_name[STM_REGISTRY_MAX_TAG_SIZE];

	/* stm_ce HAL used by this session */
	struct ce_hal *hal;

	/* Session string identifier, uesd in stm_registry */
	char tag[STM_REGISTRY_MAX_TAG_SIZE];

	/* Key rules for this session */
	stm_ce_rule_set_t rules;

	/* Registry object; the parent of all child transforms for this
	 * session */
	stm_object_h transform_list; /* Registry object; parent of child
					transforms */
	/* Lock for this session */
	OS_lock_t lock;
};

/* Struct used to store additional info for memory transforms */
struct ce_mem_transform {
	/* Downstream object handle */
	stm_object_h sink;

	/* Upstream object handle */
	stm_object_h src;

	/* Downstream sink interface */
	union {
		stm_data_interface_push_sink_t push;
		stm_data_interface_pull_sink_t pull;
	} sink_u;

	/* Buffer for sending data to downstream push sink. If NULL, the
	 * downstrean sink must be a pull sink */
	uint8_t *push_buffer;
	uint32_t push_buffer_size;

	/* Downstream buffer list for both push and pull buffers */
	stm_ce_buffer_t *dest_buf;
	uint32_t dest_count;
	uint32_t dest_total_size;
	uint32_t bytes_written;
	uint32_t bytes_available;

	/* Mutex for writing data. Held by the task performing DMAs */
	struct mutex wlock;

	/* Completion to indicate destination buffer is available for writing
	 * I.e. reader has completed */
	struct completion dest_writeable;

	/* Completion to indicate destination buffer is available for reading
	 * I.e. writer has completed */
	struct completion dest_readable;

	struct task_struct *push_task;
};

#define STM_CE_TRANSFORM_MAGIC (0xBEEFBEEF)

/* Struct used to store instance attributes for transform objects */
struct ce_transform {
	/* Fixed value. Identifies this as a transform object. */
	uint32_t magic;

	/* Points to itself. Used to check validity */
	struct ce_transform *self;

	/* Pointer to parent session object */
	struct ce_session *parent;

	/* Pointer to HAL API for this transform */
	struct ce_transform_api *api;

	/* Pointer to HAL-specific data (opaque to API layer) */
	struct ce_transform_data *data;

	/* Transform string name, used in the stm_registry */
	char tag[STM_REGISTRY_MAX_TAG_SIZE];

	/* Transform type */
	stm_ce_transform_type_t type;

	/* Lock for this transform */
	OS_lock_t lock;

	/* Indicates this transform is being deleted. Used to avoid deadlocks
	 * in disconnection callbacks */
	bool delete;

	/* Number of cipher stages for this transform */
	uint32_t stages;

	/* Array of "stages" cipher configurations */
	stm_ce_transform_config_t *config;

	/* Additional info for memory transforms */
	struct ce_mem_transform mem;
};

/* Struct used to store static attributes for session objects.
 * I.e. data that is common to all session objects */
typedef struct {
	OS_atomic_t count;
	stm_registry_type_def_t debug;
} stm_ce_session_static_t;

/* Struct used to store static attributes for transform objects.
 * I.e. data that is common to all transform objects */
typedef struct {
	OS_atomic_t count;
	stm_registry_type_def_t debug;
} stm_ce_transform_static_t;

/* Struct used to store global stm_ce data. Only one instance of this struct
 * should exist. */
typedef struct {
	OS_lock_t lock;
	stm_object_h ce_type;
	stm_ce_session_static_t session_type;
	stm_ce_transform_static_t transform_type;
	stm_object_h session_list;
	stm_ce_version_t version[STM_CE_MAX_VERSIONS];
	uint32_t n_versions;
#define RULES_NAME_MAX_LEN	20
	struct {
		char rules_name[RULES_NAME_MAX_LEN];
		stm_ce_rule_set_t rules;
	} session_rules;
	struct platform_device *pdev;
	uint32_t initialized;
} stm_ce_t;

/* Global pointer to single instance of the stm_ce_t struct */
extern stm_ce_t *stm_ce_global;

/* Checks is stm_ce is initialized */
int ce_check_initialized(void);

/* Object utility function prototypes */
int ce_transform_from_handle(stm_ce_handle_t transform,
		struct ce_transform **object);
int ce_session_from_handle(stm_ce_handle_t session,
		struct ce_session **object);
int ce_transform_get_session(struct ce_transform *transform,
		struct ce_session **session);

/* Object pretty-print handlers */
int ce_session_print(stm_object_h object, char *buf, size_t size,
		char *user_buf, size_t user_size);
int ce_transform_print(stm_object_h object, char *buf, size_t size,
		char *user_buf, size_t user_size);

/* Memory transform helpers */
int ce_transform_attach_pull_sink(struct ce_transform *tfm, stm_object_h sink);
int ce_transform_attach_push_sink(struct ce_transform *tfm, stm_object_h sink);
int ce_transform_detach_pull_sink(struct ce_transform *tfm, stm_object_h sink);
int ce_transform_detach_push_sink(struct ce_transform *tfm, stm_object_h sink);

/* Transform data push interface */
extern stm_data_interface_push_sink_t ce_data_push_interface;

/* checks any active sessions, if exists return TRUE */
int ce_is_active(void);

#endif
