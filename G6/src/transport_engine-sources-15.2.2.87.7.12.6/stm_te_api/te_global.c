/******************************************************************************
Copyright (C) 2011, 2012 STMicroelectronics. All Rights Reserved.

This file is part of the Transport Engine Library.

Transport Engine is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License version 2 as published by
the Free Software Foundation.

Transport Engine is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with Transport Engine; see the file COPYING.  If not, write to the Free
Software Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

The Transport Engine Library may alternatively be licensed under a proprietary
license from ST.

Source file name : te_global.c

Defines te global operations
******************************************************************************/

#include <stm_te_dbg.h>
#include <stm_registry.h>
#include <te_global.h>
#include <te_interface.h>
#include <te_internal_cfg.h>
#include <te_sysfs.h>
#include <pti_driver.h>
#include <pti_hal_api.h>
#include <te_hal_obj.h>
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 37))
#include <asm/atomic.h>
#else
#include <linux/atomic.h>
#endif

struct te_global te_global;

static int te_unregister_classes(void)
{
	int err;
	int result = 0;

	/* Remove output filter class */
	err = stm_registry_remove_object(&te_global.out_filter_class);
	if (err) {
		result = err;
		pr_warn("Failed to remove output filter class\n");
	}
	err = stm_registry_remove_object(&te_global.in_filter_class);
	if (err) {
		result = err;
		pr_warn("Failed to remove input filter class\n");
	}
	err = stm_registry_remove_object(&te_global.tsg_filter_class);
	if (err) {
		result = err;
		pr_warn("Failed to remove tsg filter class\n");
	}
	err = stm_registry_remove_object(&te_global.tsg_index_filter_class);
	if (err) {
		result = err;
		pr_warn("Failed to remove tsg index filter class\n");
	}
	err = stm_registry_remove_object(&te_global.demux_class);
	if (err) {
		result = err;
		pr_warn("Failed to remove demux class\n");
	}
	err = stm_registry_remove_object(&te_global.tsmux_class);
	if (err) {
		result = err;
		pr_warn("Failed to remove tsmux class\n");
	}
	return result;
}

static int te_register_classes(void)
{
	int err;

	/* Add demux class */
	err = stm_registry_add_object(STM_REGISTRY_TYPES, "te_demux",
			&te_global.demux_class);
	if (err) {
		pr_err("Failed to register demux class\n");
		goto error;
	}
	err = stm_registry_add_attribute(&te_global.demux_class,
			STM_FE_TE_SINK_INTERFACE, STM_REGISTRY_ADDRESS,
			&fe_te_sink_interface, sizeof(fe_te_sink_interface));
	if (err) {
		pr_err("Failed to register interface %s\n",
				STM_FE_TE_SINK_INTERFACE);
		goto error;
	}
	err = stm_registry_add_attribute(&te_global.demux_class,
			STM_FE_IP_TE_SINK_INTERFACE, STM_REGISTRY_ADDRESS,
			&ip_te_sink_interface, sizeof(ip_te_sink_interface));
	if (err) {
		pr_err("Failed to register interface %s\n",
				STM_FE_IP_TE_SINK_INTERFACE);
		goto error;
	}
	err = stm_registry_add_attribute(&te_global.demux_class,
			STM_DATA_INTERFACE_PUSH, STM_REGISTRY_ADDRESS,
			&stm_te_data_push_interface,
			sizeof(stm_te_data_push_interface));
	if (err) {
		pr_err("Failed to register interface %s\n",
				STM_DATA_INTERFACE_PUSH);
		goto error;
	}

	/* Add input filter class */
	err = stm_registry_add_object(STM_REGISTRY_TYPES, "te_input_filter",
			&te_global.in_filter_class);
	if (err) {
		pr_err("Failed to register input filter class\n");
		goto error;
	}
	err = stm_registry_add_attribute(&te_global.in_filter_class,
			STM_PID_FILTER_INTERFACE, STM_REGISTRY_ADDRESS,
			&te_in_filter_pid_interface,
			sizeof(stm_te_pid_interface_t));
	if (err) {
		pr_err("Failed to register interface %s\n",
				STM_PID_FILTER_INTERFACE);
		goto error;
	}

	/* Add output filter class */
	err = stm_registry_add_object(STM_REGISTRY_TYPES, "te_output_filter",
			&te_global.out_filter_class);
	if (err) {
		pr_err("Failed to register output filter class\n");
		goto error;
	}

	/* Add tsmux class */
	err = stm_registry_add_object(STM_REGISTRY_TYPES, "te_tsmux",
			&te_global.tsmux_class);
	if (err) {
		pr_err("Failed to register tsmux class\n");
		goto error;
	}
	/* Add tsg filter class */
	err = stm_registry_add_object(STM_REGISTRY_TYPES, "te_tsg_filter",
			&te_global.tsg_filter_class);
	if (err) {
		pr_err("Failed to register tsg filter class\n");
		goto error;
	}
	err = stm_registry_add_attribute(&te_global.tsg_filter_class,
			STM_TE_ASYNC_DATA_INTERFACE, STM_REGISTRY_ADDRESS,
			&stm_te_tsg_data_interface,
			sizeof(stm_te_tsg_data_interface));
	if (err) {
		pr_err("Failed to register interface %s\n",
				STM_TE_ASYNC_DATA_INTERFACE);
		goto error;
	}

	/* Add tsg index filter class */
	err = stm_registry_add_object(STM_REGISTRY_TYPES, "te_tsg_index_filter",
			&te_global.tsg_index_filter_class);
	if (err) {
		pr_err("Failed to register tsg index filter class\n");
		goto error;
	}


	return 0;
error:
	te_unregister_classes();
	return err;
}

static int te_register_types(void)
{
	int err;

	/* This will be used to register TE-specific data types with the
	 * registry */
	err = te_sysfs_data_type_add();
	if (err)
		pr_err("Failed to add dataypes\n");

	return err;
}

static int te_unregister_types(void)
{
	int err;

	/* This will be used to unregister TE-specific data types from the
	 * registry */
	err = te_sysfs_data_type_remove();
	if (err)
		pr_err("Failed to remove dataypes\n");

	return 0;
}

/**
 * @brief Set the TE global parameters based on the HAL's reported capabilities
 *
 * @param global - Pointer to te_global struct to configure
 *
 * @return 0 on success or negative errno on failure
 */
static int te_global_set_from_hal(struct te_global *g)
{
	int pdev_num = 0;
	int i;
	ST_ErrorCode_t hal_err;
	stptiHAL_pDeviceConfigStatus_t pdev_caps = {0};

	hal_err = stptiAPI_DriverGetNumberOfpDevices(&pdev_num);
	if (hal_err) {
		pr_err("Unable to get number of pDevices from HAL (0x%x)\n",
				hal_err);
		return te_hal_err_to_errno(hal_err);
	}

	/* Set object maximums from pdevice capabilities */
	g->demux_class.max = 0;
	g->in_filter_class.max = 0;
	g->out_filter_class.max = 0;

	for (i = 0; i < pdev_num; i++) {
		hal_err = stptiHAL_call_unlocked(pDevice.HAL_pDeviceGetCapability,
				stptiOBJMAN_pDeviceObjectHandle(i),
				&pdev_caps);
		if (hal_err) {
			pr_err("pDevice %d GetCapability error (0x%x)\n",
					i, hal_err);
			return te_hal_err_to_errno(hal_err);
		}

		g->demux_class.max += pdev_caps.NumbervDevice;
		g->out_filter_class.max += pdev_caps.NumberDMAs;
		g->in_filter_class.max += pdev_caps.NumberSlots;
	}

	return 0;
}

int te_global_init(void)
{
	int err = 0;

	INIT_LIST_HEAD(&te_global.demuxes);
	INIT_LIST_HEAD(&te_global.tsmuxes);
	mutex_init(&te_global.lock);

	INIT_LIST_HEAD(&te_global.index_filters);
	INIT_LIST_HEAD(&te_global.shared_sinks);

	/* Initialise demux class objects */
	te_global.demux_class.class = TE_OBJ_CLASS_DEMUX;
	atomic_set(&te_global.demux_class.count, 0);

	te_global.in_filter_class.class = TE_OBJ_CLASS_INPUT_FILTER;
	atomic_set(&te_global.in_filter_class.count, 0);

	te_global.out_filter_class.class = TE_OBJ_CLASS_OUTPUT_FILTER;
	atomic_set(&te_global.out_filter_class.count, 0);

	/* Get demux class maximums by querying the HAL */
	err = te_global_set_from_hal(&te_global);
	if (err)
		goto error;

	/* Initiailise tsmux class objects */
	te_global.tsmux_class.class = TE_OBJ_CLASS_TSMUX;
	te_global.tsmux_class.max = TE_MAX_TSMUXES;
	atomic_set(&te_global.tsmux_class.count, 0);

	te_global.tsg_filter_class.class = TE_OBJ_CLASS_TSG_FILTER;
	te_global.tsg_filter_class.max = TE_MAX_TSG_FILTERS;
	atomic_set(&te_global.tsg_filter_class.count, 0);

	te_global.tsg_index_filter_class.class = TE_OBJ_CLASS_TSG_INDEX_FILTER;
	te_global.tsg_index_filter_class.max = TE_MAX_TSG_FILTERS;
	atomic_set(&te_global.tsg_index_filter_class.count, 0);

	/* Register class objects in registry */
	err = te_register_classes();
	if (err)
		goto error;

	/* Register data types in registry */
	err = te_register_types();
	if (err)
		goto error;

	return err;
error:
	te_unregister_types();
	te_unregister_classes();
	return err;
}

int te_global_term(void)
{
	/* Unregister data types from registry */
	te_unregister_types();

	/* Unregister class objects from registry */
	te_unregister_classes();

	/* Sanity checks when shutting down */
	if (!list_empty(&te_global.demuxes))
		pr_warn("State error: TE demuxes still exist\n");
	if (!list_empty(&te_global.tsmuxes))
		pr_warn("State error: TE tsmuxes still exist\n");

	if (atomic_read(&te_global.demux_class.count))
		pr_warn("State error: TE demux count non-zero\n");
	if (atomic_read(&te_global.tsmux_class.count))
		pr_warn("State error: TE tsmux count non-zero\n");
	if (atomic_read(&te_global.out_filter_class.count))
		pr_warn("State error: TE output filter count non-zero\n");
	if (atomic_read(&te_global.in_filter_class.count))
		pr_warn("State error: TE input filter count non-zero\n");

	mutex_destroy(&te_global.lock);
	return 0;
}
