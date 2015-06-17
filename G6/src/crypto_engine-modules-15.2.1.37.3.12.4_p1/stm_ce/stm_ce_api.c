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

Source file name : stm_ce_api.c

Defines stm_ce top-level API functions
************************************************************************/

#include <linux/pm_runtime.h>

#include "stm_ce.h"
#include "stm_ce_osal.h"
#include "stm_registry.h"
#include "stm_ce_objects.h"
#include "ce_hal.h"
#include "helpers.h"
#include "stm_ce_version.h"
#include "stm_ce_fuse.h"

/* Global stm_ce_t pointer*/
stm_ce_t *stm_ce_global;

/* String containing global host version string */
static char stm_ce_host_version[] = STM_CE_VERSION;

/*
 * Local function prototypes
 */
static int init_ce_global(stm_ce_t *ce);
static int registry_ce_add(stm_ce_t *ce);
static int registry_ce_del(stm_ce_t *ce);

/* Initializes the global stm_ce structure */
static int init_ce_global(stm_ce_t *ce)
{
	/* Set the transform and session debug handlers */
	ce->session_type.debug.print_handler = &ce_session_print;
	ce->transform_type.debug.print_handler = &ce_transform_print;

	/* Initialize the session and transform counts */
	OS_atomic_set(ce->session_type.count, 0);
	OS_atomic_set(ce->transform_type.count, 0);

	/* Initialize the global stm_ce lock */
	OS_lock_init(ce->lock);

	/* Initialize the session_rules fields */
	memset(ce->session_rules.rules_name, 0, RULES_NAME_MAX_LEN);
	memset(&ce->session_rules.rules, 0, sizeof(stm_ce_rule_set_t));

	return 0;
}

/* Deinitializes the global stm_ce structure */
static int uninit_ce_global(stm_ce_t *ce)
{
	/* Delete the global stm_ce lock */
	OS_lock_del(ce->lock);

	return  0;
}

/**
 * Adds global stm_ce registry objects
 *
 * Types added
 * - TYPES/STM_CE_TYPE_NAME
 * - TYPES/STM_CE_SESSION_TYPE_NAME
 * - TYPES/STM_CE_TRANSFORM_TYPE_NAME
 *
 * Instances added
 * - INSTANCES/STM_CE_INSTANCE_NAME
 * - INSTANCES/STM_CE_INSTANCE_NAME/STM_CE_SESSION_INSTANCE_NAME
 *
 * Data types added
 * - STM_CE_SESSION_PRINT_TYPE
 * - STM_CE_TRANSFORM_PRINT_TYPE
 */
static int registry_ce_add(stm_ce_t *ce)
{
	int err;

	/* Add Type objects to stm_registry */
	err = stm_registry_add_object(STM_REGISTRY_TYPES, STM_CE_TYPE_NAME,
			&ce->ce_type);
	if (0 != err) {
		CE_ERROR("Failed to add stm_ce type object to registry "\
				"(%d)\n", err);
		goto clean;
	}
	err = stm_registry_add_object(&ce->ce_type, STM_CE_SESSION_TYPE_NAME,
			&ce->session_type);
	if (0 != err) {
		CE_ERROR("Failed to add session type object to registry "\
				"(%d)\n", err);
		goto clean;
	}
	err = stm_registry_add_object(&ce->ce_type, STM_CE_TRANSFORM_TYPE_NAME,
			&ce->transform_type);
	if (0 != err) {
		CE_ERROR("Failed to add transform type object to registry "\
				"(%d)\n", err);
		goto clean;
	}

	/* Add debug print registry types */
	err = stm_registry_add_data_type(STM_CE_SESSION_PRINT_TYPE,
			&ce->session_type.debug);
	if (0 != err)
		CE_ERROR("Failed to add session print data type (%d)\n", err);

	err = stm_registry_add_data_type(STM_CE_TRANSFORM_PRINT_TYPE,
			&ce->transform_type.debug);
	if (0 != err)
		CE_ERROR("Failed to add transform print data type (%d)\n", err);

	/* Add stm_ce object instance. This will be the root object for all
	 * stm_ce object instances.
	 * The session_list object is added as a child of this object, to
	 * contain all session object instances */
	err = stm_registry_add_instance(STM_REGISTRY_INSTANCES,
			&ce->ce_type, STM_CE_INSTANCE_NAME, ce);
	if (0 != err) {
		CE_ERROR("Failed to add stm_ce global instance to registry "\
				"(%d)\n", err);
		goto clean;
	}
	err = stm_registry_add_object(ce, STM_CE_SESSION_INSTANCE_NAME,
			&ce->session_list);
	if (0 != err) {
		CE_ERROR("Failed to add session_list to registry (%d)\n", err);
		goto clean;
	}

	return 0;

clean:
	registry_ce_del(ce);
	return err;
}

/**
 * Removes registry entries added by registry_ce_add
 **/
static int registry_ce_del(stm_ce_t *ce)
{
	int err;

	/* Remove object instances */
	err = stm_registry_remove_object(&ce->session_list);
	if (0 != err) {
		CE_ERROR("Failed to remove session_list from registry (%d)\n",
				err);
	}
	err = stm_registry_remove_object(ce);
	if (0 != err) {
		CE_ERROR("Failed to remove stm_ce global instance from "\
				"registry (%d)\n", err);
	}

	/* Remove print data types */
	err = stm_registry_remove_data_type(STM_CE_TRANSFORM_PRINT_TYPE);
	if (0 != err) {
		CE_ERROR("Failed to remove transform print data type (%d)\n",
				err);
	}
	err = stm_registry_remove_data_type(STM_CE_SESSION_PRINT_TYPE);
	if (0 != err) {
		CE_ERROR("Failed to remove session print data type (%d)\n",
				err);
	}

	/* Remove stm_ce type objects */
	err = stm_registry_remove_object(&ce->transform_type);
	if (0 != err) {
		CE_ERROR("Failed to remove transform type from registry "\
				"(%d)\n", err);
	}
	err = stm_registry_remove_object(&ce->session_type);
	if (0 != err) {
		CE_ERROR("Failed to remove session type from registry "\
				"(%d)\n", err);
	}
	err = stm_registry_remove_object(&ce->ce_type);
	if (0 != err) {
		CE_ERROR("Failed to remove stm_ce type from registry "\
				"(%d)\n", err);
	}

	return err;
}

/* Helper function, used by stm_ce API functions to check if stm_ce has been
 * initialized */
int ce_check_initialized(void)
{
	if (!stm_ce_global->pdev)
		return -ENODEV;
	return 0;
}

/*
 * API function definitions
 */
int stm_ce_init(void)
{
	int err = 0;

	CE_API_ENTRY();

	/* Is stm_ce initialized? */
	if (stm_ce_global->initialized) {
		CE_ERROR("Already initialized.\n");
		return -EALREADY;
	}

	/* Initialize the global object */
	err = init_ce_global(stm_ce_global);
	if (0 != err)
		goto clean;

	/* Add global stm_ce registry objects */
	err = registry_ce_add(stm_ce_global);
	if (0 != err)
		goto clean;

#ifdef CONFIG_PM_RUNTIME
	/* Ensure the device is woken up */
	pm_runtime_get_sync(&stm_ce_global->pdev->dev);
#endif
	/* Register available stm ce HALs */
	err = ce_hal_register_all();
	if (0 != err)
		goto clean;
#ifdef CONFIG_PM_RUNTIME
	/* We don't need anymore the device */
	pm_runtime_put(&stm_ce_global->pdev->dev);
#endif

	stm_ce_global->initialized = 1;
	CE_API_EXIT("%d\n", 0);
	return 0;
clean:
	/* Remove registry entries */
	registry_ce_del(stm_ce_global);

	/* Free stm_ce_global */
	uninit_ce_global(stm_ce_global);

	CE_API_EXIT("%d\n", err);
	return err;
}
EXPORT_SYMBOL(stm_ce_init);

int stm_ce_term(void)
{
	int err = 0;

	CE_API_ENTRY();

	/* Is stm_ce initialized? */
	if (!stm_ce_global->initialized)
		return -ENODEV;

	/* Get global stm_ce lock */
	OS_wlock(stm_ce_global->lock);

	/* Delete all session objects */
	foreach_child_object(&stm_ce_global->session_list,
			(int (*)(void *))&stm_ce_session_delete);

#ifdef CONFIG_PM_RUNTIME
	/* Ensure the device is woken up */
	pm_runtime_get_sync(&stm_ce_global->pdev->dev);
#endif

	/* Unregister all available stm ce HALs */
	err = ce_hal_unregister_all();

#ifdef CONFIG_PM_RUNTIME
	/* We don't need anymore the device */
	pm_runtime_put(&stm_ce_global->pdev->dev);
#endif
	/* Remove registry entries */
	registry_ce_del(stm_ce_global);

	/* Release global stm_ce lock */
	OS_wunlock(stm_ce_global->lock);

	/* Free stm_ce_global */
	uninit_ce_global(stm_ce_global);
	stm_ce_global->initialized = 0;

	CE_API_EXIT("%d\n", err);
	return err;
}
EXPORT_SYMBOL(stm_ce_term);

int stm_ce_get_capabilities(stm_ce_caps_t *caps)
{
	int err = 0;
	CE_API_ENTRY();

	err = ce_check_initialized();
	if (err)
		goto clean;

	/* Basic parameter check */
	if (NULL == caps)
		return -EINVAL;

	OS_wlock(stm_ce_global->lock);

	/* Set version info for host code */
	stm_ce_global->version[0].name = stm_ce_host_version;
	stm_ce_global->n_versions = 1;

	/* Load HAL version strings */
	err = ce_hal_get_version(
			&stm_ce_global->version[stm_ce_global->n_versions],
			STM_CE_MAX_VERSIONS - stm_ce_global->n_versions,
			&stm_ce_global->n_versions);

	caps->version = stm_ce_global->version;
	caps->num_versions = stm_ce_global->n_versions;

	OS_wunlock(stm_ce_global->lock);

clean:
	CE_API_EXIT("%d\n", err);
	return err;
}
EXPORT_SYMBOL(stm_ce_get_capabilities);
