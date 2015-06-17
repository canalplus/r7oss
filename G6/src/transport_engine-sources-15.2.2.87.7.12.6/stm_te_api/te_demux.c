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

Source file name : te_demux.c

Defines demux object-specific functions
******************************************************************************/
#include <linux/pm_runtime.h>

#include "te_object.h"
#include "te_global.h"
#include "te_demux.h"
#include "te_output_filter.h"
#include "te_section_filter.h"
#include "te_internal_cfg.h"
#include "te_interface.h"
#include "te_hal_obj.h"
#include "te_sysfs.h"

#include <stm_te_dbg.h>
#include <pti_driver.h>
#include <pti_hal_api.h>

#include "te_object.h"
#include "te_global.h"
#include "te_demux.h"
#include "te_output_filter.h"
#include "te_section_filter.h"
#include "te_internal_cfg.h"
#include "te_interface.h"
#include "te_input_filter.h"
#include "te_pid_filter.h"
#include "st_relay.h"
#include <linux/of.h>

/* Place holder pdevices */
static struct pdevice_info_s {
	uint32_t pdevice_number;
	struct te_hal_obj *pdevice;
} pdevice_info;

/* Local function prototypes */
static int te_select_pdevice(void);
static int te_demux_hal_start(struct te_demux *dmx);
static int te_demux_hal_stop(struct te_demux *dmx);
static int te_hal_init(void);
static int te_hal_term(void);
static int te_demux_detach_autotarget(struct te_demux *demux, stm_te_pid_t pid,
					stm_te_object_h target);
static int te_demux_get_stat(struct te_obj *obj, stm_te_demux_stats_t *stat);
static int te_demux_get_actual_stat(struct te_demux *dmx,
					stm_te_demux_stats_t *stat);
static int te_demux_update_duplicate(struct te_demux *dmx);
static int te_hal_pm_resume(void);
static int te_hal_pm_suspend(void);

/*!
 * \brief return demux pointer from an obj
 */
struct te_demux *te_demux_from_obj(struct te_obj *obj)
{
	if (!obj || obj->type != TE_OBJ_DEMUX)
		return NULL;

	return container_of(obj, struct te_demux, obj);
}

/*!
 * \brief dmmy release for obj
 */
static void dummy_release_obj(struct kref *ref)
{
	pr_err("dummy release called!\n");
	BUG();
}

/*!
 * \brief stm_te select a pdevice
 *
 * returns the index of the pdevice to be used for Demux allocation
 *
 * \param none
 *
 * \retval index of the pdevice to be used
 */
static int te_select_pdevice(void)
{

	static int pdevice_index = 0;
	int selected_index = pdevice_index;

	pdevice_index++;
	if (pdevice_index >= pdevice_info.pdevice_number)
		pdevice_index = 0;

	return selected_index;
}

/*!
 * \brief stm_te demux object destructor
 *
 * Deletes an stm_te demux object and any resource allocated to it
 *
 * \param demux Pointer to the demux object to destroy. After this function
 * has returned this pointer should not be dereferenced
 *
 * \retval 0      Demux successfully deleted
 * \retval -EBUSY Demux busy - it cannot be deleted
 * \retval -EIO   HAL error freeing resources (demux is still deleted in this
 *                case since such error is not recoverable here)
 */
static int te_demux_delete(struct te_obj *obj)
{
	int err;
	struct te_demux *dmx = te_demux_from_obj(obj);
	struct te_autotarget *tgt, *tmp;

	/* delete stat from registry */
	err = te_sysfs_entries_remove(obj);
	if (err)
		pr_warn("Failed to remove sysfs entries\n");

	/* Return any error here immediately, since it likely means that the
	 * demux has connections or children and cannot be deleted */
	err = te_obj_deinit(&dmx->obj);
	if (err)
		return err;

	list_del(&dmx->obj.lh);
	/* Free demux HAL resources. Continue on error */
	err = te_demux_hal_stop(dmx);
	if (err)
		pr_warn("Failed to free demux HAL resources\n");

	/* Free demux TE-level resources */
	if (dmx->analysis_length)
		kfree(dmx->analysis);

	/* Stop the HAL if this is the last demux */
	if (list_empty(&te_global.demuxes))
		err = te_hal_term();

	/* delete the autotargets */
	list_for_each_entry_safe(tgt, tmp, &dmx->autotargets, entry) {
		list_del(&tgt->entry);
		kfree(tgt);
	}

	st_relayfs_freeindex(ST_RELAY_SOURCE_DEMUX_INJECTOR,
			dmx->injector_relayfs_index);

	kfree(dmx);

	return err;
}

/*!
 * \brief Retrieves stm_te control for an stm_te demux object
 *
 * \param obj     TE object for demux to read the control value from
 * \param control Control to get
 * \param buf     Buffer populated with the read value
 * \param size    Size of buf
 *
 * \retval 0       Success
 * \retval -ENOSYS Control not available
 * \retval -EINVAL Buffer size too small to read data
 */
static int te_demux_get_control(struct te_obj *obj, uint32_t control, void *buf,
		uint32_t size)
{
	int err = 0;
	struct te_demux *dmx = te_demux_from_obj(obj);

	switch (control) {
	case STM_TE_DEMUX_CNTRL_PACING_OUTPUT:
		err = GET_CONTROL(dmx->pacing_filter, buf, size);
		break;
	case STM_TE_DEMUX_CNTRL_INPUT_TYPE:
		err = GET_CONTROL(dmx->input_type, buf, size);
		break;
	case STM_TE_DEMUX_CNTRL_STATUS:
		err = te_demux_get_stat(obj, (stm_te_demux_stats_t *) buf);
		break;
	case STM_TE_DEMUX_CNTRL_DISCARD_DUPLICATE_PKTS:
		err = GET_CONTROL(dmx->discard_dupe_ts, buf, size);
		break;
	default:
		err = -ENOSYS;
	}
	return err;
}

/*!
 * \brief Stores stm_te control data for an stm_te demux object
 *
 * \param obj     TE object for the demux to set the control value on
 * \param control Control to set
 * \param buf     Buffer containing the control value to set
 * \param size    Size of the control value in buf
 *
 * \retval 0       Success
 * \retval -ENOSYS Control not available
 * \retval -EINVAL Buffer size too small for specified control
 */
static int te_demux_set_control(struct te_obj *obj, uint32_t control, const void *buf,
		uint32_t size)
{
	int err = 0;
	struct te_demux *dmx = te_demux_from_obj(obj);

	switch (control) {
	case STM_TE_DEMUX_CNTRL_PACING_OUTPUT:
		err = SET_CONTROL(dmx->pacing_filter, buf, size);
		break;
	case STM_TE_DEMUX_CNTRL_INPUT_TYPE:
		err = SET_CONTROL(dmx->input_type, buf, size);
		break;
	case STM_TE_DEMUX_CNTRL_DISCARD_DUPLICATE_PKTS:
		err = SET_CONTROL(dmx->discard_dupe_ts, buf, size);
		if (err == 0)
			err = te_demux_update_duplicate(dmx);
		break;
	default:
		err = -ENOSYS;
		break;
	}
	return err;
}

/*!
 * \brief Get global demux statistics
 *
 * Get statistics value, using offset from reset point reference.
 *
 * \param obj   demux object
 * \param stat	pointer to stat
 *
 * \retval 0 Success
 * \retval -EINVAL HAL error
 **/
static int te_demux_get_stat(struct te_obj *obj, stm_te_demux_stats_t *stat)
{
	int err;
	struct timespec ts;
	ktime_t	ktime;

	struct te_demux *dmx = te_demux_from_obj(obj);

	err = te_demux_get_actual_stat(dmx, stat);
	if (err) {
		pr_err("Failed to get actual stat\n");
		goto exit;
	}

	/* Recalculate based on reset point */
	stat->packet_count -= dmx->stat_upon_reset.packet_count;
	stat->cc_errors -= dmx->stat_upon_reset.cc_errors;
	stat->tei_count -= dmx->stat_upon_reset.tei_count;
	stat->input_packets -= dmx->stat_upon_reset.input_packets;
	stat->crc_errors -= dmx->stat_upon_reset.crc_errors;
	stat->buffer_overflows -= dmx->stat_upon_reset.buffer_overflows;
	getrawmonotonic(&ts);
	ktime = timespec_to_ktime(ts);
	stat->system_time = ktime_to_us(ktime);
exit:
	return err;
}

static int te_demux_attach(struct te_obj *obj, stm_object_h target)
{
	return -ENOSYS;
}

static int te_demux_detach(struct te_obj *obj, stm_object_h target)
{
	return -ENOSYS;
}

static struct te_obj_ops te_demux_ops = {
	.delete = te_demux_delete,
	.set_control = te_demux_set_control,
	.get_control = te_demux_get_control,
	.attach = te_demux_attach,
	.detach = te_demux_detach,
};

/*!
 * \brief stm_te demux object constructor
 *
 * Creates a new demux object. Initialises all the internal data structures in
 * the demux object to their default values
 *
 * \param name String name of the new demux object
 * \param new_demux Returned pointer to the new demux object
 *
 * \retval 0             Success
 * \retval -ENOMEM       No memory for new demux
 * \retval -EIO          Hardware error
 * \retval -ENAMETOOLONG String name too long
 */
int te_demux_new(char *name, struct te_obj **new_demux)
{
	struct te_demux *dmx;
	int err;
	bool started_hal = false;

	/* Sanity check name. The registry will do this for us later anyway.
	 * This check is so that we can comply with our documented return code
	 * of -ENAMETOOLONG */
	if (strnlen(name, STM_REGISTRY_MAX_TAG_SIZE) >
			STM_REGISTRY_MAX_TAG_SIZE - 1)
		return -ENAMETOOLONG;

	dmx = kzalloc(sizeof(struct te_demux), GFP_KERNEL);
	if (!dmx) {
		pr_err("No memory for new demux\n");
		return -ENOMEM;
	}

	err = te_obj_init(&dmx->obj, TE_OBJ_DEMUX, name, NULL,
			&te_global.demux_class);
	if (err)
		goto err0;
	dmx->obj.ops = &te_demux_ops;

	/* Init demux-specific data */
	INIT_LIST_HEAD(&dmx->in_filters);
	INIT_LIST_HEAD(&dmx->out_filters);
	INIT_LIST_HEAD(&dmx->autotargets);
	INIT_LIST_HEAD(&dmx->linked_pids);

	dmx->stm_fe_bc_pid_set = NULL;
	dmx->stm_fe_bc_pid_clear = NULL;
	dmx->stream_id = TE_NULL_STREAM_ID;
	dmx->ts_tag = TE_NULL_STREAM_ID;
	dmx->input_type = STM_TE_INPUT_TYPE_UNKNOWN;

	dmx->use_timer_tag = FALSE;

	/* Start the HAL if this is the first demux */
	if (list_empty(&te_global.demuxes)) {
		err = te_hal_init();
		if (err)
			goto err1;
		started_hal = true;
	}

	/* Allocate HAL resources for this demux*/
	err = te_demux_hal_start(dmx);
	if (err)
		goto err2;

	/* Add statistics registry */
	err = te_sysfs_entries_add(&dmx->obj);
	if (err) {
		pr_err("Failed to add sysfs entries for demux %p %s\n", dmx,
			name);
		err = -ENOMEM;
		goto err2;
	}

	err = stm_registry_add_attribute(&dmx->obj, "st_relayfs_id",
			STM_REGISTRY_UINT32, &dmx->injector_relayfs_index,
			sizeof(dmx->injector_relayfs_index));
	if (err) {
		pr_warn("Failed to add st_relayfs_id to obj %s (%d)\n", name,
				err);
		goto err2;
	}
	dmx->injector_relayfs_index = st_relayfs_getindex(
			ST_RELAY_SOURCE_DEMUX_INJECTOR);

	err = stm_registry_add_attribute(&dmx->obj,
			TE_SYSFS_NAME_HAL_HDL "vdevice",
			TE_SYSFS_TAG_HAL_HDL,
			&dmx->hal_vdevice,
			sizeof(struct te_hal_obj *));
	if (err) {
		pr_warn("Failed to add HAL vdevice handle to obj %s (%d)\n",
				name, err);
		goto err2;
	}
	err = stm_registry_add_attribute(&dmx->obj,
			TE_SYSFS_NAME_HAL_HDL "session",
			TE_SYSFS_TAG_HAL_HDL,
			&dmx->hal_session,
			sizeof(struct te_hal_obj *));
	if (err) {
		pr_warn("Failed to add HAL session handle to obj %s (%d)\n",
				name, err);
		goto err2;
	}
	err = stm_registry_add_attribute(&dmx->obj,
			TE_SYSFS_NAME_HAL_HDL "injector",
			TE_SYSFS_TAG_HAL_HDL,
			&dmx->hal_injector,
			sizeof(struct te_hal_obj *));
	if (err) {
		pr_warn("Failed to add HAL injector handle to obj %s (%d)\n",
				name, err);
		goto err2;
	}
	err = stm_registry_add_attribute(&dmx->obj,
			TE_SYSFS_NAME_HAL_HDL "signal",
			TE_SYSFS_TAG_HAL_HDL,
			&dmx->hal_signal,
			sizeof(struct te_hal_obj *));
	if (err) {
		pr_warn("Failed to add HAL signal handle to obj %s (%d)\n",
				name, err);
		goto err2;
	}

	list_add_tail(&dmx->obj.lh, &te_global.demuxes);
	rwlock_init(&dmx->out_flt_rw_lock);
	mutex_init(&dmx->of_sec_lock);
	*new_demux = &dmx->obj;

	return 0;

err2:
	if (started_hal)
		te_hal_term();
err1:
	te_obj_deinit(&dmx->obj);

err0:
	kfree(dmx);
	return err;
}

/*!
 * \brief Allocates the HAL resources associated with a new TE demux object
 *
 * \param dmx TE demux object to allocate HAL resource for
 *
 * \retval 0    Success
 * \retval -EIO Required HAL resources not available
 */
static int te_demux_hal_start(struct te_demux *dmx)
{
	int err = 0;
	ST_ErrorCode_t hal_err;

	stptiHAL_vDeviceAllocationParams_t vdev_params = {0};
	stptiHAL_SoftwareInjectorConfigParams_t swi_params = {0};
	stptiHAL_pDeviceConfigStatus_t pdev_caps = {0};
	int pdev_index = te_select_pdevice();

	/* Detemine the vDevice allocation parameters from the pDevice
	 * capabilties */
	hal_err = stptiHAL_call(pDevice.HAL_pDeviceGetCapability,
			stptiOBJMAN_pDeviceObjectHandle(pdev_index),
			&pdev_caps);
	if (hal_err) {
		pr_err("pDevice %d GetCapability error (0x%x)\n",
				pdev_index, hal_err);
		return te_hal_err_to_errno(hal_err);
	}
	vdev_params.NumberOfSlots =
		TE_SLOTS_PER_VDEVICE(pdev_caps.NumberSlots,
					pdev_caps.NumbervDevice);
	vdev_params.NumberOfSectionFilters =
		pdev_caps.NumberHWSectionFilters / pdev_caps.NumbervDevice;

	/* Allocate vdevice */
	vdev_params.StreamID = dmx->stream_id;
	vdev_params.ForceDiscardSectionOnCRCError =
		TE_DEFAULT_DISCARD_ON_CRC_ERROR;
	vdev_params.PacketSize = DVB_PACKET_SIZE;
	vdev_params.TSProtocol = stptiHAL_DVB_TS_PROTOCOL;

	err = te_hal_obj_alloc(&dmx->hal_vdevice,
				&pdevice_info.pdevice[pdev_index],
				OBJECT_TYPE_VDEVICE, &vdev_params);

	if (err) {
		pr_err("Failed to allocate vDevice (err %d)\n", err);
		goto error;
	}

	/* Allocate session */
	err = te_hal_obj_alloc(&dmx->hal_session, dmx->hal_vdevice,
				OBJECT_TYPE_SESSION, NULL);
	if (err) {
		pr_err("failed to allocate session (err %d)\n", err);
		goto error;
	}

	/* Enable indexes on the vdevice */
	hal_err = stptiHAL_call(vDevice.HAL_vDeviceIndexesEnable,
			dmx->hal_vdevice->hdl, TRUE);
	if (ST_NO_ERROR != hal_err) {
		pr_err("failed to enable indexes on vdevice (err 0x%x)\n",
				hal_err);
		err = te_hal_err_to_errno(hal_err);
		goto error;
	}

	/* Allocate software injector */
	swi_params.MaxNumberOfNodes = TE_MAX_INJECTOR_NODES;
	err = te_hal_obj_alloc(&dmx->hal_injector, dmx->hal_session,
				OBJECT_TYPE_SOFTWARE_INJECTOR, &swi_params);
	if (err) {
		pr_err("failed to allocate injector (err 0x%x\n", hal_err);
		goto error;
	}

	/* Allocate signal buffer work queue */
	err = te_hal_obj_alloc(&dmx->hal_signal, dmx->hal_session,
				OBJECT_TYPE_SIGNAL, NULL);
	if (err) {
		pr_err("failed to allocate signal (err 0x%x)\n", hal_err);
		goto error;
	}

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 36))
	{
		char wq_name[16];
		snprintf(wq_name, sizeof(wq_name), "DMX-WQ-%01d.%02d",
			dmx->hal_vdevice->hdl.member.pDevice,
			dmx->hal_vdevice->hdl.member.vDevice);
		dmx->work_que = create_workqueue(wq_name);
	}
#else
	dmx->work_que = alloc_workqueue("DMX-WQ-%01d.%02d", WQ_UNBOUND, 0,
			dmx->hal_vdevice->hdl.member.pDevice,
			dmx->hal_vdevice->hdl.member.vDevice);
#endif
	if (!dmx->work_que) {
		pr_err("failed to create workqueue\n");
		err = -ENOMEM;
		goto error;
	}

	err = te_bwq_create(&dmx->bwq, dmx->hal_signal, dmx->work_que);
	if (err != 0) {
		pr_err("failed to create buffer work queue\n");
		goto error;
	}

	return 0;
error:
	te_demux_hal_stop(dmx);
	return err;
}

/*!
 * \brief Deallocates all HAL resources owned by a TE demux object
 *
 * \param dmx TE demux object whose HAL resource to free
 *
 * Note that if HAL resources are allocated against this demux, this function
 * will return 0 (success)
 *
 * \retval 0    Success
 * \retval -EIO HAL error deallocating HAL resources
 */
static int te_demux_hal_stop(struct te_demux *dmx)
{
	int err = 0;

	pr_debug("Deallocating HAL resources for %s\n", dmx->obj.name);

	if (dmx->bwq)
		err = te_bwq_destroy(dmx->bwq);

	dmx->bwq = NULL;

	if (err != 0)
		pr_warning("unable to stop buffer work queue\n");

	if (dmx->work_que)
		destroy_workqueue(dmx->work_que);

	if (dmx->hal_injector)
		te_hal_obj_dec(dmx->hal_injector);

	dmx->hal_injector = NULL;

	if (dmx->hal_signal)
		te_hal_obj_dec(dmx->hal_signal);

	dmx->hal_signal = NULL;

	if (dmx->hal_session)
		te_hal_obj_dec(dmx->hal_session);

	dmx->hal_session = NULL;

	if (dmx->hal_vdevice)
		if (te_hal_obj_dec(dmx->hal_vdevice))
			dmx->hal_vdevice = NULL;

	if ( dmx->hal_vdevice != NULL ) {
		pr_warning("**** MEMORY LEAK: demux objects remain post stop ****\n");
		te_hal_obj_dump_tree(dmx->hal_vdevice);
	}

	dmx->hal_vdevice = NULL;

	return err;
}

/*!
 * \brief Initialises the TE HAL layer
 */
static int te_hal_init(void)
{
	ST_ErrorCode_t Error = ST_NO_ERROR;
	uint32_t pdevice_number = 0;
	uint32_t i;

	Error = stptiAPI_DriverGetNumberOfpDevices(&pdevice_number);
	if (Error != ST_NO_ERROR) {
		pr_err("stptiAPI_DriverGetNumberOfpDevices FAILED error 0x%x\n",
				Error);
		return -ENODEV;
	}

	if (pdevice_number == 0) {
		pr_err("No TE HAL pDevices registered. Cannot create any demux objects\n");
		return -ENODEV;
	}

	pdevice_info.pdevice = kzalloc(sizeof(struct te_hal_obj) * pdevice_number, GFP_KERNEL);
	if (!pdevice_info.pdevice) {
		pr_err("couldn't allocate pdevice list\n");
		return -ENOMEM;
	}

	pdevice_info.pdevice_number = pdevice_number;

	for (i = 0; i < pdevice_number; i++) {
		pdevice_info.pdevice[i].hdl.member.pDevice = i;
		pdevice_info.pdevice[i].hdl.member.vDevice = STPTI_HANDLE_INVALID_VDEVICE_NUMBER;
		pdevice_info.pdevice[i].hdl.member.Session = STPTI_HANDLE_INVALID_SESSION_NUMBER;
		pdevice_info.pdevice[i].hdl.member.ObjectType = OBJECT_TYPE_PDEVICE;
		pdevice_info.pdevice[i].hdl.member.Object = STPTI_HANDLE_INVALID_OBJECT_NUMBER;
		pdevice_info.pdevice[i].release = dummy_release_obj;

		/* Initialise the static pdevice hal wrapper */
		INIT_LIST_HEAD(&pdevice_info.pdevice[i].children);
		INIT_LIST_HEAD(&pdevice_info.pdevice[i].c_entry);
		kref_init(&pdevice_info.pdevice[i].refcount);
	}
	if(Error == ST_NO_ERROR) {
		Error = te_hal_pm_resume();
		if(Error != ST_NO_ERROR)
			kfree(pdevice_info.pdevice);
	}

	return te_hal_err_to_errno(Error);
}

/*!
 * \brief Terminates the TE HAL layer
 */
static int te_hal_term(void)
{
	ST_ErrorCode_t Error = ST_NO_ERROR;
	Error = te_hal_pm_suspend();
	if (Error)
		pr_warn("Failed to do pm_suspend \n");

	kfree(pdevice_info.pdevice);
	return te_hal_err_to_errno(Error);
}

/* TE PM runtime resume */
static int te_hal_pm_resume()
{
	ST_ErrorCode_t hal_err = 0;

#ifdef CONFIG_PM_RUNTIME
	struct platform_device *pdev;
	/* This has to be fixed for more than one pdevice */
	int pdev_index = te_select_pdevice();

	/* retrieve the pdev from the pDevice structure*/
	hal_err = stptiHAL_call_unlocked(pDevice.HAL_pDeviceGetPlatformDevice,
			stptiOBJMAN_pDeviceObjectHandle(pdev_index),
			&pdev);
	if (hal_err) {
		pr_err("pDevice %d GetPlatformDevice error (0x%x)\n",
				pdev_index, hal_err);
		return hal_err;
	}
	/* Already write locked */
	hal_err = pm_runtime_get_sync(&pdev->dev);
	if (hal_err) {
		pr_err("pDevice %d pm_runtime_get_sync error (0x%x)\n",
				pdev_index, hal_err);
		return hal_err;
	}
	/* stfe resume */
	pdev = NULL;

	hal_err = stptiTSHAL_call(TSInput.TSHAL_TSInputGetPlatformDevice,
					0, &pdev);
	if (hal_err) {
		pr_err("TSInput GetPlatformDevice error (0x%x)\n",
				hal_err);
		return hal_err;
	}
	hal_err = pm_runtime_get_sync(&pdev->dev);
	if (hal_err) {
		pr_err("TSInput pm_runtime_get_sync error (0x%x)\n",
				hal_err);
		return hal_err;
	}
#endif
	return hal_err;
}
/* TE PM runtime suspend */

static int te_hal_pm_suspend()
{
	ST_ErrorCode_t hal_err = 0;

#ifdef CONFIG_PM_RUNTIME
	struct platform_device *pdev;
	/* This has to be fixed for more than one pdevice */
	int pdev_index = te_select_pdevice();

	/* retrieve the pdev from the pDevice structure*/
	hal_err = stptiHAL_call_unlocked(pDevice.HAL_pDeviceGetPlatformDevice,
			stptiOBJMAN_pDeviceObjectHandle(pdev_index),
			&pdev);
	if (hal_err) {
		pr_err("pDevice %d GetPlatformDevice error (0x%x)\n",
				pdev_index, hal_err);
		return hal_err;
	}
	hal_err = pm_runtime_put_sync(&pdev->dev);
	if (hal_err) {
		pr_err("pDevice %d pm_runtime_put_sync error (0x%x)\n",
				pdev_index, hal_err);
		return hal_err;
	}
	/* stfe suspend */
	pdev = NULL;

	hal_err = stptiTSHAL_call(TSInput.TSHAL_TSInputGetPlatformDevice,
					0, &pdev);
	if (hal_err) {
		pr_err("TSInput GetPlatformDevice error (0x%x)\n",
				hal_err);
		return hal_err;
	}
	hal_err = pm_runtime_put_sync(&pdev->dev);
	if (hal_err) {
		pr_err("TSInput pm_runtime_put_sync error (0x%x)\n",
				hal_err);
		return hal_err;
	}
#endif
	return hal_err;
}

/*!
 * \brief Configures the vdevice underlying a te demux object to connect it to
 * the correct TSIN
 *
 * \param demux TE demux object to connect
 * \param tsin  TSIN number to connect to
 *
 * \retval 0    Success
 * \retval -EIO HAL error
 **/
static int te_demux_hal_update_tsin(struct te_demux *demux, uint32_t tsin)
{
	ST_ErrorCode_t hal_err;
	int err = 0;

	hal_err = stptiHAL_call(vDevice.HAL_vDeviceSetStreamID,
			demux->hal_vdevice->hdl, tsin, demux->use_timer_tag);
	if (ST_NO_ERROR != hal_err) {
		pr_err("could not attach demux %s to tsin 0x%x (0x%x)\n",
				demux->obj.name, tsin, hal_err);
		err = te_hal_err_to_errno(hal_err);
	}
	return err;
}

/*!
 * \brief Starts a te demux object
 *
 * A demux object is "started" by connecting the correct TSIN and stopped by
 * setting the "NULL" TSIN
 *
 * \param obj TE obj for demux to start
 *
 * \retval 0    Success
 * \retval -EIO Hardware error
 */
int te_demux_start(struct te_obj *obj)
{
	int err = 0;
	struct te_demux *dmx = te_demux_from_obj(obj);

	/* Note: It is not an error to start a started demux */
	if (TE_OBJ_STOPPED == obj->state) {
		err = te_demux_hal_update_tsin(dmx, dmx->ts_tag);
		if (!err)
			obj->state = TE_OBJ_STARTED;
	}
	return err;
}

/*!
 * \brief Stops a te demux object
 *
 * A demux object is "stopped" by setting the "NULL" TSIN
 *
 * \param obj TE obj for demux to start
 *
 * \retval 0    Success
 * \retval -EIO Hardware error
 */
int te_demux_stop(struct te_obj *obj)
{
	int err = 0;
	struct te_demux *dmx = te_demux_from_obj(obj);

	/* Note: It is not an error to stop a stopped demux */
	if (TE_OBJ_STOPPED != obj->state) {
		err = te_demux_hal_update_tsin(dmx, TE_NULL_STREAM_ID);
		if (!err)
			obj->state = TE_OBJ_STOPPED;
	}
	return err;
}

/*!
 * \brief Configures or resets the HAL specified TSIN
 *
 * \param tsin_number TSIN number to configure
 * \param ts_tag      TS tag
 * \param tsconfig    Pointer to TSIN configuration paramters. If NULL, the
 * TSIN will be reset
 *
 * \return 0 on success, -EINVAL on error
 **/
static int te_tsin_configure(uint32_t tsin_number, uint32_t ts_tag,
		stm_te_tsinhw_config_t *tsconfig)
{
	stptiTSHAL_TSInputConfigParams_t tsin_parm = {0};
	ST_ErrorCode_t hal_err = ST_NO_ERROR;

	/* Configure this TSIN */
	tsin_parm.StreamID = tsin_number;
	if (tsconfig) {
		if (tsconfig->ts_merged && ((tsin_number & 0x2000)==0))
			tsin_parm.PacketLength = TE_TAGGED_PACKET_SIZE;
		else
			tsin_parm.PacketLength = DVB_PACKET_SIZE;
		tsin_parm.SyncLDEnable = true;
		tsin_parm.SerialNotParallel = tsconfig->serial_not_parallel;
		tsin_parm.AsyncNotSync = tsconfig->async_not_sync;
		tsin_parm.AlignByteSOP = false;
		tsin_parm.InvertTSClk = tsconfig->invert_ts_clk;
		tsin_parm.IgnoreErrorInByte = false;
		tsin_parm.IgnoreErrorInPkt = false;
		tsin_parm.IgnoreErrorAtSOP = false;
		tsin_parm.InputBlockEnable = true;
		/* As ts_error pins are float, we ignore ts_error signal by doing this */
		if (of_machine_is_compatible("st,custom001303"))
		{
			tsin_parm.IgnoreErrorInByte = true;
			tsin_parm.IgnoreErrorInPkt = true;
			tsin_parm.IgnoreErrorAtSOP = true;
		}
		tsin_parm.MemoryPktNum = 128;
		tsin_parm.ClkRvSrc = 1;
		tsin_parm.Routing = (tsconfig->ts_thru_cablecard == true) ?
				stptiTSHAL_TSINPUT_DEST_TSOUT0 :
				stptiTSHAL_TSINPUT_DEST_DEFAULT;
		tsin_parm.InputTagging = ((tsconfig->ts_merged == true) && ((tsin_number & 0x2000) == 0)) ?
				stptiTSHAL_TSINPUT_TAGS_STFE :
				stptiTSHAL_TSINPUT_TAGS_NONE;
		tsin_parm.OutputPacingRate = 0;
		tsin_parm.OutputPacingClkSrc = 0;
		tsin_parm.CIMode = false;
		tsin_parm.OutputSerialNotParallel = false;
		tsin_parm.TSTag = ts_tag;

		pr_debug("Configuring TSIN %u with ts_tag %u\n",
				tsin_number, ts_tag);
		pr_debug("\tSerialNotParallel: %u\n",
				tsconfig->serial_not_parallel);
		pr_debug("\tInvertTSClk: %u\n", tsconfig->invert_ts_clk);
		pr_debug("\tAsyncNotSync: %u\n", tsconfig->async_not_sync);
	} else {
		pr_debug("Resetting TSIN %u with ts_tag %u\n",
				tsin_number, ts_tag);
	}

	/* Resources are mapped at start in the driver init */
	hal_err = stptiTSHAL_call(TSInput.TSHAL_TSInputConfigure, 0,
		&tsin_parm);

	if (ST_NO_ERROR != hal_err) {
		pr_err("TSIN %d with ts_tag %d config failed, error 0x%x\n",
				tsin_number, ts_tag, hal_err);
	}

	return te_hal_err_to_errno(hal_err);
}

/*!
 * \brief Connects a TSIN input to a demux object
 *
 * \param demux    TE demux to attach the TSIN to
 * \param tsconfig TSIN configuration parameters
 *
 * \retval 0       Success
 * \retval -EINVAL Error configuring TSIN
 * \retval -EINTR  Call interrupted
 */
static int te_demux_connect_tsin(struct te_demux *demux,
		stm_te_tsinhw_config_t *tsconfig)
{
	int err = 0;
	bool tsin_configured = false;
	uint32_t tsin_number = tsconfig->tsin_number;
	uint32_t ts_tag = tsconfig->ts_tag;
	struct te_demux *demux_other;

	if (mutex_lock_interruptible(&te_global.lock) != 0)
		return -EINTR;

	/* ts_tag are egual to tsin_number when:
	 *  - input on the MIB ((tsin_number & 0x2000)) or
	 *  - path IB->DEMUX  ((!tsconfig->ts_thru_cablecard || !tsconfig->ts_merged))*/
	if ( (tsin_number & 0x2000) || (!tsconfig->ts_thru_cablecard && !tsconfig->ts_merged))
		ts_tag = tsin_number;

	/* Configure the additional IB->MSCC path for a merged stream
	 * going to cablecard */
	if (tsconfig->ts_thru_cablecard && tsconfig->ts_merged) {
		/* ts_tag must have the MIB bit set for all merged inputs */
		ts_tag |= 0x2000;
		/* Check if the IB->MSCC path has already been configured */
		list_for_each_entry(demux_other, &te_global.demuxes, obj.lh) {
			if (demux_other->stream_id == tsconfig->tsin_number)
				tsin_configured = true;
		}
		if (!tsin_configured) {
			pr_debug("Configuring tsin %u for %s\n",
				tsconfig->tsin_number, demux->obj.name);
			/* We use tsin_number for the ts_tag parameter to avoid
			 * problems with clashes with the MIB path, but
			 * it doesn't matter as this route turns off tagging */
			err = te_tsin_configure(tsin_number, tsin_number,
					tsconfig);
			if (err)
				goto error;

		}
		/* Now we turn off the cablecard flag for the MIB->DEMUX path */
		tsconfig->ts_thru_cablecard = false;
		/* The MIB->DEMUX path needs to be in parallel mode */
		tsconfig->serial_not_parallel =	(bool)0;
		/* The MIB->DEMUX path needs to not invert clock */
		tsconfig->invert_ts_clk = (bool)0;
		/* The MIB->DEMUX path needs to use the ts_tag with MIB bit set
		 * for the tsin_number */
		tsin_number = ts_tag;
		tsin_configured = false;

		/* Check if this ts stream id is connected to another demux */
		list_for_each_entry(demux_other, &te_global.demuxes, obj.lh) {
			if (demux_other->ts_tag == ts_tag)
				tsin_configured = true;
		}
	} else {
		/* Check if this ts stream id is connected to another demux */
		list_for_each_entry(demux_other, &te_global.demuxes, obj.lh) {
			if (demux_other->stream_id == tsin_number)
				tsin_configured = true;
		}
	}

	if (!tsin_configured) {
		pr_debug("Configuring tsin %u with tag %u for demux %s\n",
			tsin_number, ts_tag, demux->obj.name);
		err = te_tsin_configure(tsin_number, ts_tag, tsconfig);
		if (err) {
			pr_err("Failed to configure tsin error 0x%x\n", err);
			goto error;
		}
	}

	/* Must save original tsconfig->tsin_number as local
	 * variable is overwritten for the merged_IB->MSCC
	 * scenario */
	demux->stream_id = tsconfig->tsin_number;
	demux->ts_tag = ts_tag;

	if (TE_OBJ_STARTED == demux->obj.state) {
		err = te_demux_hal_update_tsin(demux, ts_tag);
		if (err)
			pr_err("Failed to update tsin error 0x%x\n", err);
	}
error:
	mutex_unlock(&te_global.lock);
	return err;
}

/*!
 * \brief Disconnects any existing TSIN from a TE demux object
 *
 * \param demux TE demux to disconnect
 *
 * \retval 0       Success
 * \retval -EINVAL Error disconnecting TSIN
 * \retval -EINTR  Call Interrupted
 */
int te_demux_disconnect_tsin(struct te_demux *demux)
{
	int err = 0;
	struct te_demux *demux_other;
	bool tsin_stop = true;
	uint32_t ts_tag;
	uint32_t tsin_number;

	if (mutex_lock_interruptible(&te_global.lock) != 0)
		return -EINTR;

	/* Set this demux to use the default stream id */
	ts_tag = demux->ts_tag;
	tsin_number = demux->stream_id;
	demux->stream_id = TE_NULL_STREAM_ID;
	demux->ts_tag = TE_NULL_STREAM_ID;

	/* Update the TSIN feeding this demux back to the default stream id */
	err = te_demux_hal_update_tsin(demux, TE_NULL_STREAM_ID);
	if (err)
		goto error;

	/* Check if this TSIN is connected to another demux Check the ts_tag
	 * value first as this is unique except when routing a merged stream
	 * through cable-card */
	list_for_each_entry(demux_other, &te_global.demuxes, obj.lh) {
		if (demux_other->ts_tag == ts_tag)
			tsin_stop = false;
	}

	/* Stop the TSIN if no other demux is using it*/
	if (tsin_stop) {
		/* If route is merged_IB->MSCC->MIB->DEMUX then the tsin_number
		 * will be different to ts_tag AND ts_tag will indicate a MIB
		 */
		if ((tsin_number != ts_tag) && (ts_tag & 0x2000)) {
			/* First close down the MIB->DEMUX path */
			err = te_tsin_configure(ts_tag, ts_tag, NULL);
			if (err)
				goto error;
		}
		/* Now check if there are any other demuxes using the
		 * same tsin_number i.e. any more substreams on the
		 * merged input
		 * This is done when merged_IB is used
		 */
		list_for_each_entry(demux_other, &te_global.demuxes,
				obj.lh) {
			if (demux_other->stream_id == tsin_number)
				tsin_stop = false;
		}
		if (tsin_stop) {
			/* Close down the IB->MSCC path */
			err = te_tsin_configure(tsin_number,
					tsin_number, NULL);
		}
	}

error:
	mutex_unlock(&te_global.lock);
	return err;
}

/*!
 * \brief Injects a single scatter list of blocks to the demux
 *
 * \param demux Demux object
 * \param page  scatterlist block
 * \param inject_param injection parameters
 *
 * \retval 0    Success
 * \retval -EIO HAL error
 */
static int te_demux_inject_blocks(struct te_demux *demux,
		struct scatterlist *blocks, uint32_t nblocks, uint32_t inject_parm)
{
	ST_ErrorCode_t hal_err;
	int i=0;
	struct scatterlist *page;
	int res=0;

	te_hal_obj_inc(demux->hal_injector);

	if (!( nblocks > 0 && nblocks <= TE_MAX_INJECTOR_NODES )){
		res = -EIO;
		pr_err("Injection invalid number of blocks. Aborting injection\n");
		goto error;
	}

	for_each_sg(blocks, page, nblocks, i) {
		hal_err = stptiHAL_call(
				SoftwareInjector.HAL_SoftwareInjectorAddInjectNode,
				demux->hal_injector->hdl,
				(uint8_t *)sg_dma_address(page),
				sg_dma_len(page),
				inject_parm);

		if (ST_NO_ERROR != hal_err) {
			pr_err("HAL_SoftwareInjectorAddInjectNode err 0x%x\n", hal_err);
			goto err_abort;
		}
	}

	hal_err = stptiHAL_call(SoftwareInjector.HAL_SoftwareInjectorStart,
			demux->hal_injector->hdl);

	if (ST_NO_ERROR != hal_err) {
		pr_err("HAL_SoftwareInjectorStart err 0x%x\n", hal_err);
		goto err_abort;
	}

	/* Wait up to 1000ms for injection to complete */
	hal_err = stptiHAL_call(
			SoftwareInjector.HAL_SoftwareInjectorWaitForCompletion,
			demux->hal_injector->hdl, 1000);
	if (ST_ERROR_TIMEOUT == hal_err) {
		pr_err("Injection timeout. Aborting injection\n");
		goto err_abort;
	}

error:
	te_hal_obj_dec(demux->hal_injector);
	return res;

err_abort:
	/* Attempt to abort injection */
	stptiHAL_call(SoftwareInjector.HAL_SoftwareInjectorAbort,
			demux->hal_injector->hdl);

	res = te_hal_err_to_errno(hal_err);
	return res;
}

/*!
 * \brief Retrieve a byte from a scatter list
 *
 * \param offset      byte offset to retreive
 * \param byte        location to store byte
 * \param blocks  scatterlist blocks
 * \param nblock blocks in list
 *
 * \retval 0 on success
 * \retval -1 on error
 */
static uint8_t get_sg_byte(uint32_t offset, uint8_t *byte,
		struct scatterlist *blocks, uint32_t nblock)
{
	uint32_t scanned = 0;
	uint8_t res = -1;
	struct scatterlist *page;
	int i;

	pr_debug("offset %d\n", offset);

	for_each_sg(blocks, page, nblock, i) {
		if (scanned + sg_dma_len(page) > offset) {
			*byte = ((uint8_t *)sg_virt(page))[offset - scanned];
			res = 0;
			break;
		}
		scanned += sg_dma_len(page);
	}
	return res;
}

/*!
 * \brief look for sync bytes in an SG list
 *
 * \param start     starting offset into SG list
 * \param pkt_size  packet size trying to sync to
 * \param blocks    SG list
 * \param nblock    number of elements in SG list
 *
 * \return 0        on failure
 * \return pkt_size on success
 */
static int te_demux_sync(int start, int pkt_size, struct scatterlist *blocks,
		uint32_t nblock)
{
	u8 byte;
	int i;
	for (i = 1; i < TE_ANALYSIS_PKTS; i++) {

		get_sg_byte(start + (pkt_size * i),
			&byte, blocks, nblock);

		pr_debug("input[%d] == 0x%02x (%d %d)\n",
			start + (pkt_size * i), byte, start, i);

		if (byte != DVB_SYNC_BYTE) {
			/* No 0x47 at expected location, no point continuing */
			break;
		}
	}
	if (i == TE_ANALYSIS_PKTS) {
		pr_info("determined %d byte packets\n",	pkt_size);
		return pkt_size;
	} else {
		pr_debug("not %d byte packets\n", pkt_size);
	}
	return 0;
}

/*!
 * \brief Analyse the input data to determine packet size
 *
 * \param demux Demux object
 * \param blocks  scatterlist blocks
 * \param nblock blocks in list
 *
 * \retval STM_TE_INPUT_TYPE_UNKNOWN Not enough data to analyse - feed more
 * \retval STM_TE_INPUT_TYPE_TTS 192 byte packets
 * \retval STM_TE_INPUT_TYPE_DVB 188 byte packets
 */
static int te_demux_analyse_input(struct te_demux *demux,
	struct scatterlist *blocks,	uint32_t nblock)
{
	uint32_t bytes_rx = 0;
	uint8_t res = STM_TE_INPUT_TYPE_UNKNOWN;
	struct scatterlist *page;
	struct scatterlist *tmp = NULL;
	uint8_t *buf = NULL;
	int i;
	int start = 0;
	uint8_t byte;

	for_each_sg(blocks, page, nblock, i)
		bytes_rx += sg_dma_len(page);

	pr_debug("demux %s received %d bytes\n", demux->obj.name, bytes_rx);

	if (demux->analysis_length) {

		/*
		 * Create a temporary scatter list
		 * prepended with stored data
		 */

		nblock++;
		pr_debug("creating temp list of %d entried\n", nblock);

		tmp = kzalloc(sizeof(*tmp) * nblock, GFP_KERNEL);
		if (!tmp) {
			pr_err("couldn't allocate temporary scatter list\n");
			goto error;
		}

		sg_init_table(tmp, nblock);

		sg_set_buf(&tmp[0], demux->analysis, demux->analysis_length);

		for_each_sg(blocks, page, nblock - 1, i)
			sg_set_page(&tmp[i + 1], sg_page(page), page->length,
					page->offset);
	} else {
		tmp = blocks;
	}

	if (demux->analysis_length + bytes_rx
		>= TE_ANALYSIS_PKTS * TE_DLNA_PACKET_SIZE) {


		/* Search through the biggest packet length and attempt sync */
		while (start < TE_DLNA_PACKET_SIZE) {

			/* look for a 0x47 */
			for (; start < TE_DLNA_PACKET_SIZE; start++) {
				get_sg_byte(start, &byte, tmp, nblock);
				if (byte == DVB_SYNC_BYTE) {
					pr_debug("found possible sync @ %d\n",
						start);
					break;
				}
			}

			if (start >= TE_DLNA_PACKET_SIZE) {
				/* No 0x47 in sight, someone fed us garbage */
				goto error;
			}

			/* Look for sync at DVB_PACKET_SIZE intervals */
			if (te_demux_sync(start, DVB_PACKET_SIZE, tmp,
						nblock)) {
				res = STM_TE_INPUT_TYPE_DVB;
				break; /* sync OK */
			}

			/*
			 * didn't sync, so look for sync
			 * at TE_DLNA_PACKET_SIZE intervals
			 */
			if (te_demux_sync(start, TE_DLNA_PACKET_SIZE,
				tmp, nblock)) {
				res = STM_TE_INPUT_TYPE_TTS;
				break; /* sync OK */
			}
			start++;
		}

		if (res != STM_TE_INPUT_TYPE_UNKNOWN) {
			/* Search successful */
			if (demux->analysis_length) {
				/*
				 * Inject stored data
				 * (rest of scatter list will be
				 * injected by caller)
				 */
				stptiHAL_InjectionFlags_t parm = { 0 };

				pr_debug("injecting stored data length %d\n",
					demux->analysis_length);

				if (res == STM_TE_INPUT_TYPE_TTS)
					parm =
					stptiHAL_PKT_INCLUDES_DNLA_TTS_TAGS;

				dma_map_sg(NULL, &tmp[0], 1, DMA_TO_DEVICE);
				te_demux_inject_blocks(demux, &tmp[0], 1, parm);
				dma_unmap_sg(NULL, &tmp[0], 1, DMA_TO_DEVICE);

			}
			/* Clean up buffers not an error */
			goto error;
		} else {
			pr_debug("Input type unknown start %d bytes\n", start);
			goto error;
		}
	}

	/* Got here? need to save everything for later */

	pr_debug("storing %d bytes\n", demux->analysis_length + bytes_rx);

	buf = kzalloc(demux->analysis_length + bytes_rx, GFP_KERNEL | GFP_DMA);
	if (!buf) {
		pr_err("couldn't allocate analysis buffer\n");
		goto error;
	}

	sg_copy_to_buffer(tmp, nblock, buf, demux->analysis_length + bytes_rx);

	if (demux->analysis_length) {
		kfree(tmp);
		kfree(demux->analysis);
		demux->analysis = NULL;
	}

	demux->analysis = buf;
	demux->analysis_length = demux->analysis_length + bytes_rx;

	return res;
error:
	/* Dump everything, start over */
	if (demux->analysis_length) {
		kfree(tmp);
		kfree(demux->analysis);
		demux->analysis = NULL;
		demux->analysis_length = 0;
	}
	return res;
}

static int te_demux_inject_data(struct te_demux *demux, struct scatterlist *blocks,
		uint32_t nblock, uint32_t *injected_count)
{
	int err = 0;
	int free_space = -1;
	unsigned int bufsz , bytes_in_buffer;
	struct scatterlist *out, *page;
	uint32_t n_out, i;
	ktime_t starttime, endtime;
	struct timespec ts_start, ts_end;
	stptiHAL_InjectionFlags_t inject_parm = {0};

	if (demux->input_type == STM_TE_INPUT_TYPE_UNKNOWN) {
		/* Continue analysis */
		demux->input_type = te_demux_analyse_input(demux, blocks,
				nblock);
		pr_debug("set input_type %d\n", demux->input_type);
	}

	if (demux->input_type == STM_TE_INPUT_TYPE_UNKNOWN) {
		/* We're still analysing, this isn't an error */
		return 0;
	}

	if (demux->input_type == STM_TE_INPUT_TYPE_TTS)
		inject_parm = stptiHAL_PKT_INCLUDES_DNLA_TTS_TAGS;

	while (nblock > 0) {

		/* Wait until buffer has space */
		if (demux->pacing_filter) {
			pr_debug("Pacing using output filter: %p\n",
				demux->pacing_filter);

			err = te_out_filter_get_sizes(demux->pacing_filter,
						&bufsz, &bytes_in_buffer,
						&free_space);
			if (err != 0)
				free_space = -1;

			if (demux->sink_interface.mode == STM_IOMODE_BLOCKING_IO){
				err = te_out_filter_wait(demux->pacing_filter);
			} else if ((err == 0) &&
				(((bytes_in_buffer*100) / bufsz) > 75)) {
				/*This will return with last updated injected_count */
				return err;
			}

		} else {
			free_space = -1;
		}

		out = blocks;
		n_out = nblock;

		/* Work out how many pages to inject */
		if (free_space != -1) {
			uint32_t total = 0;
			for_each_sg(out, page, n_out, i) {
				if (sg_dma_len(page) > (bufsz >> 2))
					pr_warn_ratelimited("Overflow possible! "
						"Input buffer size (%u bytes) "
						"exceedes 1/4 pacing filter "
						"buffer size (%u bytes)\n",
						 sg_dma_len(page), bufsz);
				if (total + sg_dma_len(page) <= free_space) {
					total += sg_dma_len(page);
				} else {
					break;
				}
			}

			/* Always inject at least one */
			n_out = (i == 0 ? 1 : i);
			pr_debug("injecting: %u (%u into %d)\n",
					n_out, total, free_space);
		}

		nblock -= n_out;
		blocks += n_out;

		getrawmonotonic(&ts_start);
		starttime = timespec_to_ktime(ts_start);
		/* Inject up to TE_MAX_INJECTOR_NODES at a time */
		while (n_out > 0 && !err) {
			uint32_t nb_to_inject = min_t(uint32_t, n_out,
					TE_MAX_INJECTOR_NODES);

			err = te_demux_inject_blocks(demux, out, nb_to_inject,
					inject_parm);
			n_out -= nb_to_inject;
			out += nb_to_inject;
			*injected_count += nb_to_inject;
		}
		getrawmonotonic(&ts_end);
		endtime = timespec_to_ktime(ts_end);

		demux->push_time_stats.inject_time_tp
			= ktime_to_us(ktime_sub(endtime, starttime));
	}
	return err;
}

/*
 * Demux push data interface
 */
static int te_demux_push_connect(stm_object_h src_object,
		stm_object_h sink_object)
{
	int err = 0;
	struct te_obj *dmx_obj;
	struct te_demux *dmx;
	uint32_t	if_notify_size;
	char type_tag[STM_REGISTRY_MAX_TAG_SIZE];
	stm_object_h target_type;
	uint32_t if_size;

	stm_te_trace_in();

	err = te_demux_from_hdl(sink_object, &dmx_obj);
	if (err) {
		pr_err("Bad demux handle %p\n", sink_object);
		goto out;
	}
	dmx = te_demux_from_obj(dmx_obj);

	if (dmx->upstream_obj) {
		pr_err("Demux %s already connected to by %p\n",
				dmx_obj->name, dmx->upstream_obj);
		return -EPERM;
	}

	if (mutex_lock_interruptible(&dmx_obj->lock) != 0) {
		err = -EINTR;
		goto out;
	}

	dmx->upstream_obj = src_object;

	/*Iomode and notify support : check dmx_obj if it has been
	* updated for iomode from src_obj If yes , check if we have
	* notify function defined with src_obj */

	if (0 == stm_registry_get_attribute(dmx_obj,
				STM_DATA_INTERFACE_PUSH,
				STM_REGISTRY_ADDRESS,
				sizeof(dmx->sink_interface),
				&dmx->sink_interface,
				&if_size)) {

		if (0 != stm_registry_get_object_type(src_object, &target_type)) {
			pr_err("unable to get type of src object %p\n",
				src_object);
			return err;
		}
		err = stm_registry_get_attribute(target_type,
				STM_DATA_INTERFACE_PUSH_NOTIFY,
				type_tag,
				sizeof(stm_data_interface_push_notify_t),
				&dmx->push_notify,
				&if_notify_size);
		if (0 != err)
			pr_warn("no push_notify attribute\n");

	}

	/* Reset the demux input type */
	dmx->input_type = STM_TE_INPUT_TYPE_UNKNOWN;
	mutex_unlock(&dmx_obj->lock);

out:
	stm_te_trace_out_result(err);
	return err;
}

static int te_demux_push_disconnect(stm_object_h src_object,
		stm_object_h sink_object)
{
	struct te_obj *dmx_obj;
	struct te_demux *dmx;
	int err = 0;

	stm_te_trace_in();

	err = te_demux_from_hdl(sink_object, &dmx_obj);
	if (err) {
		pr_err("Bad demux handle %p\n", sink_object);
		return err;
	}
	dmx = te_demux_from_obj(dmx_obj);

	if (src_object != dmx->upstream_obj) {
		pr_err("Demux %s not connected to %p (upstream_obj=%p)\n",
				dmx_obj->name, dmx->upstream_obj, src_object);
		return -EPERM;
	}

	dmx->disconnect_pending = true;

	if (mutex_lock_interruptible(&dmx_obj->lock) != 0) {
		err = -EINTR;
		goto error;
	}

	dmx->upstream_obj = NULL;
	dmx->disconnect_pending = false;

        memset(&dmx->sink_interface, 0, sizeof(dmx->sink_interface));
	memset(&dmx->push_notify, 0, sizeof(dmx->push_notify));

        if (dmx->analysis_length) {
		kfree(dmx->analysis);
		dmx->analysis = NULL;
		dmx->analysis_length = 0;
	}
	mutex_unlock(&dmx_obj->lock);

error:
	stm_te_trace_out();
	return err;
}

static int te_demux_push_data(stm_object_h demux_h,
		struct stm_data_block *block_list, uint32_t block_count,
		uint32_t *data_blocks)
{
	struct scatterlist *sglist = NULL;
	uint32_t i;
	int err;
	struct te_obj *dmx_obj;
	struct te_demux *dmx;
	uint32_t injected_count = 0 ;
	struct timespec ts;
	ktime_t current_time;
	*data_blocks = 0;

	err = te_demux_from_hdl(demux_h, &dmx_obj);
	if (err) {
		pr_err("Bad demux handle %p\n", demux_h);
		return err;
	}

	if (mutex_lock_interruptible(&dmx_obj->lock) != 0)
		return -EINTR;

	dmx = te_demux_from_obj(dmx_obj);

	getrawmonotonic(&ts);
	current_time = timespec_to_ktime(ts);
	dmx->push_time_stats.inject_input_latency = ktime_to_us(ktime_sub(
						current_time, dmx->last_push));

	dmx->last_push = current_time;

	/* Only allow data push if we have an upstream connection */
	if (!dmx->upstream_obj) {
		pr_err("Demux not connected to upstream object\n");
		err = -EPERM;
		goto out;
	}

	/* Construct a scatterlist and map it for reading by the device in
	 * order to flush cache aliases */
	sglist = kmalloc(sizeof(struct scatterlist) * block_count, GFP_KERNEL);
	if (!sglist) {
		*data_blocks = 0;
		err = -ENOMEM;
		goto out;
	}
	sg_init_table(sglist, block_count);

	for (i = 0; i < block_count; i++) {
		sg_set_buf(&sglist[i], block_list[i].data_addr,
				block_list[i].len);
		st_relayfs_write(ST_RELAY_TYPE_DEMUX_INJECTOR +
				dmx->injector_relayfs_index,
				ST_RELAY_SOURCE_DEMUX_INJECTOR +
				dmx->injector_relayfs_index,
				block_list[i].data_addr,
				block_list[i].len, 0);
	}

	dma_map_sg(NULL, sglist, block_count, DMA_TO_DEVICE);

	err = te_demux_inject_data(dmx, sglist,
			block_count, &injected_count);

	if (!err && dmx->input_type == STM_TE_INPUT_TYPE_UNKNOWN)
		*data_blocks = block_count;
	else if (!err)
		*data_blocks = injected_count;
out:
	mutex_unlock(&dmx_obj->lock);
	if (sglist) {
		dma_unmap_sg(NULL, sglist, block_count, DMA_TO_DEVICE);
		kfree(sglist);
	}
	return err;
}

stm_data_interface_push_sink_t stm_te_data_push_interface = {
	te_demux_push_connect,
	te_demux_push_disconnect,
	te_demux_push_data,
	KERNEL,
	STM_IOMODE_BLOCKING_IO,
	0, /*aligment not used*/
	0, /* max transfer field not used*/
	0
};

/*
 * Demux frontend sink interface
 */
static int stm_demod_te_connect_handler(stm_object_h fe_object,
		stm_object_h demux_h, stm_te_tsinhw_config_t *tsconfig)
{
	int err = 0;
	struct te_obj *dmx_obj;
	struct te_demux *dmx;
	struct te_in_filter *in_filter = NULL;

	stm_te_trace_in();

	err = te_demux_from_hdl(demux_h, &dmx_obj);
	if (err) {
		pr_err("Bad demux handle %p\n", demux_h);
		goto out;
	}
	dmx = te_demux_from_obj(dmx_obj);

	if (dmx->upstream_obj) {
		pr_err("Demux %s already connected to by %p\n",
				dmx_obj->name, dmx->upstream_obj);
		err = -EPERM;
		goto out;
	}

	dmx->use_timer_tag = tsconfig->use_timer_tag;

	if (mutex_lock_interruptible(&dmx_obj->lock) != 0) {
		err = -EINTR;
		goto out;
	}

	err = te_demux_connect_tsin(dmx, tsconfig);
	if (!err)
		dmx->upstream_obj = fe_object;

	if (tsconfig->stm_fe_bc_pid_set != NULL) {
		dmx->stm_fe_bc_pid_set = tsconfig->stm_fe_bc_pid_set;
		dmx->stm_fe_bc_pid_clear = tsconfig->stm_fe_bc_pid_clear;

		list_for_each_entry(in_filter, &dmx->in_filters, obj.lh) {
			dmx->stm_fe_bc_pid_set(fe_object, demux_h, in_filter->pid);
		}
	}

	mutex_unlock(&dmx_obj->lock);

out:
	stm_te_trace_out_result(err);
	return err;
}

static int stm_demod_te_disconnect_handler(stm_object_h fe_object,
		stm_object_h demux_h)
{
	int err = 0;
	struct te_obj *dmx_obj;
	struct te_demux *dmx;
	struct te_in_filter *in_filter = NULL;

	stm_te_trace_in();

	err = te_demux_from_hdl(demux_h, &dmx_obj);
	if (err) {
		pr_err("Bad demux handle %p\n", demux_h);
		goto out;
	}
	dmx = te_demux_from_obj(dmx_obj);

	if (fe_object != dmx->upstream_obj) {
		pr_err("Demux %s not connected to %p (upstream_obj=%p)\n",
				dmx_obj->name, fe_object, dmx->upstream_obj);
		err = -EPERM;
		goto out;
	}

	dmx->disconnect_pending = true;
	dmx->use_timer_tag = FALSE;

	if (mutex_lock_interruptible(&dmx_obj->lock) != 0) {
		err = -EINTR;
		goto out;
	}

	err = te_demux_disconnect_tsin(dmx);
	if (!err)
		dmx->upstream_obj = NULL;
	dmx->disconnect_pending = false;

	if (dmx->stm_fe_bc_pid_clear != 0) {
		list_for_each_entry(in_filter, &dmx->in_filters, obj.lh) {
			dmx->stm_fe_bc_pid_clear(fe_object, demux_h, in_filter->pid);
		}
		dmx->stm_fe_bc_pid_set = NULL;
		dmx->stm_fe_bc_pid_clear = NULL;
	}

	mutex_unlock(&dmx_obj->lock);
out:
	stm_te_trace_out_result(err);
	return err;
}

stm_fe_te_sink_interface_t fe_te_sink_interface = {
	stm_demod_te_connect_handler,
	stm_demod_te_disconnect_handler
};

/*
 * Demux ip frontend sink interface
 */
static int stm_ip_te_connect_handler(stm_object_h fe_object,
				     stm_object_h demux_h,
				     stm_te_ip_config_t *ipconfig)
{
	struct te_obj *dmx_obj;
	struct te_demux *dmx;
	int err;

	stm_te_trace_in();

	err = te_demux_from_hdl(demux_h, &dmx_obj);
	if (err) {
		pr_err("Bad demux handle %p\n", demux_h);
		return err;
	}
	dmx = te_demux_from_obj(dmx_obj);

	if (dmx->upstream_obj) {
		pr_err("Demux %s already connected to by %p\n", dmx_obj->name,
				dmx->upstream_obj);
		return -EPERM;
	}

	if (mutex_lock_interruptible(&dmx_obj->lock) != 0) {
		err = -EINTR;
		goto out;
	}

	dmx->upstream_obj = fe_object;
	mutex_unlock(&dmx_obj->lock);

out:
	stm_te_trace_out();
	return err;
}

static int stm_ip_te_disconnect_handler(stm_object_h fe_object,
					stm_object_h demux_h)
{
	struct te_obj *dmx_obj;
	struct te_demux *dmx;
	int err;

	stm_te_trace_in();

	err = te_demux_from_hdl(demux_h, &dmx_obj);
	if (err) {
		pr_err("Bad demux handle %p\n", demux_h);
		return err;
	}
	dmx = te_demux_from_obj(dmx_obj);

	if (fe_object != dmx->upstream_obj) {
		pr_err("Demux %s not connected to %p (upstream_obj=%p)\n",
				dmx_obj->name, fe_object, dmx->upstream_obj);
		return -EPERM;
	}

	dmx->disconnect_pending = true;

	if (mutex_lock_interruptible(&dmx_obj->lock) != 0) {
		err = -EINTR;
		goto out;
	}

	dmx->upstream_obj = NULL;
	dmx->disconnect_pending = false;
	memset(&dmx->sink_interface, 0, sizeof(dmx->sink_interface));
	memset(&dmx->push_notify, 0, sizeof(dmx->push_notify));
	mutex_unlock(&dmx_obj->lock);

out:
	stm_te_trace_out_result(err);
	return err;
}

stm_ip_te_sink_interface_t ip_te_sink_interface = {
	stm_ip_te_connect_handler,
	stm_ip_te_disconnect_handler
};

int te_demux_get_device_time(struct te_obj *object, uint64_t *dev_time,
		uint64_t *sys_time)
{
	ST_ErrorCode_t Error;
	struct te_demux *demux;
	U32 StreamID = 0;
	BOOL UseTimerTag;
	stptiTSHAL_TimerValue_t TimerValue = { 0, 0, 0, 0, 0 };
	U32 vdevice_timervalue = 0;

	demux = te_demux_from_obj(object);

	if (demux == NULL)
		return -ENODEV;

	Error = stptiHAL_call(vDevice.HAL_vDeviceGetTimer,
				demux->hal_vdevice->hdl,
				&vdevice_timervalue,
				sys_time);
	if (ST_NO_ERROR == Error) {
		pr_debug("Timer read %x", vdevice_timervalue);
		*dev_time = ((uint64_t)vdevice_timervalue);
	} else if (ST_ERROR_FEATURE_NOT_SUPPORTED == Error) {
		Error = stptiHAL_call(vDevice.HAL_vDeviceGetStreamID,
					demux->hal_vdevice->hdl,
					&StreamID,
					&UseTimerTag);
		if (ST_NO_ERROR == Error) {
			Error = stptiTSHAL_call(
				TSInput.TSHAL_TSInputStfeGetTimer,
				TE_DEFAULT_DEVICE_INDEX, StreamID,
				&TimerValue,
				NULL);

			if (ST_NO_ERROR == Error) {
				*dev_time = ((uint64_t)
					(TimerValue.Clk27MHzDiv300Bit31to0));
				*dev_time |= ((uint64_t)
					(TimerValue.Clk27MHzDiv300Bit32)) << 32;
				*sys_time = TimerValue.SystemTime;
			}
		}
	}

	return te_hal_err_to_errno(Error);
}

int te_demux_register_pid_autotarget(stm_te_object_h dmx_h, stm_te_pid_t pid,
		stm_te_object_h target)
{
	int res = 0;
	stm_object_h target_obj_type;
	struct te_in_filter *input;
	struct te_obj *dmx_o;
	struct te_demux *demux;
	struct te_autotarget *autotarget;

	res = te_obj_from_hdl(dmx_h, &dmx_o);
	if (res != 0)
		goto out;

	demux = te_demux_from_obj(dmx_o);
	if (demux == NULL) {
		res = -ENODEV;
		goto out;
	}

	list_for_each_entry(autotarget, &demux->autotargets, entry) {
		if (autotarget->pid == pid) {
			pr_err("pid 0x%04x already registered\n", pid);
			res = -EINVAL;
			goto out;
		}
	}

	res = stm_registry_get_object_type(target, &target_obj_type);
	if (res != 0) {
		pr_err("bad target\n");
		goto out;
	}

	autotarget = kzalloc(sizeof(*autotarget), GFP_KERNEL);
	if (autotarget == NULL) {
		pr_err("couldn't allocate autotarget object\n");
		res = -ENOMEM;
		goto out;
	}

	autotarget->pid = pid;
	autotarget->target = target;
	INIT_LIST_HEAD(&autotarget->entry);
	list_add(&autotarget->entry, &demux->autotargets);

	pr_debug("searching for Pid filter objects\n");

	list_for_each_entry(input, &demux->in_filters, obj.lh) {
		if (input->obj.type == TE_OBJ_PID_FILTER && input->pid == pid) {
			pr_debug("found pid filter 0x%p\n", &input->obj);
			res = input->obj.ops->attach(&input->obj, target);
			if (res != 0)
				goto out;
		}
	}
out:
	return res;
}

static int te_demux_detach_autotarget(struct te_demux *demux, stm_te_pid_t pid,
					stm_te_object_h target)
{
	int res = 0;
	struct te_in_filter *input;

	list_for_each_entry(input, &demux->in_filters, obj.lh) {
		if (input->obj.type == TE_OBJ_PID_FILTER && input->pid == pid) {
			pr_debug("found pid filter 0x%p\n", &input->obj);
			res = input->obj.ops->detach(&input->obj, target);
			if (res != 0)
				pr_debug("ignoring detach error %d\n", res);
		}
	}

	return 0;
}

int te_demux_unregister_pid_autotarget(stm_te_object_h dmx_h,
					stm_te_pid_t pid)
{
	int res = 0;
	struct te_obj *dmx_o;
	struct te_demux *demux;
	struct te_autotarget *tgt, *tmp;

	res = te_obj_from_hdl(dmx_h, &dmx_o);
	if (res != 0)
		goto out;

	demux = te_demux_from_obj(dmx_o);
	if (demux == NULL) {
		res = -ENODEV;
		goto out;
	}

	/*
	 * Search through transforms, if pid matches,
	 * search input filters and detach
	 */
	list_for_each_entry_safe(tgt, tmp, &demux->autotargets, entry) {
		if (tgt->pid == pid) {
			te_demux_detach_autotarget(demux, pid, tgt->target);
			list_del(&tgt->entry);
			kfree(tgt);
		}
	}

out:
	return res;
}

int te_demux_pid_announce(struct te_in_filter *input)
{
	struct te_demux *demux;
	struct te_autotarget *tgt;
	struct te_linkedpid *link;
	struct te_pid_filter_secondary_data sdata;

	int res = 0;

	if (input->obj.type != TE_OBJ_PID_FILTER) {
		pr_err("not a PID filter\n");
		return -EINVAL;
	}

	demux = te_demux_from_obj(input->obj.parent);
	if (demux == NULL) {
		pr_err("No parent demux\n");
		return -ENODEV;
	}

	pr_debug("searching for transform for PID 0x%04x\n", input->pid);

	/* Search through transforms, if pid matches, attach */
	list_for_each_entry(tgt, &demux->autotargets, entry) {
		if (tgt->pid == input->pid) {
			pr_debug("found transform for PID 0x%04x\n", input->pid);
			res = input->obj.ops->attach(&input->obj, tgt->target);
			if (res != 0) {
				pr_err("couldn't attach transform\n");
				return res;
			}
		}
	}

	/* Search through links for matching primary or secondary PIDs */
	list_for_each_entry(link, &demux->linked_pids, entry) {
		if (link->p_pid == input->pid && !link->p_filter)
			link->p_filter = &input->obj;
		if (link->s_pid == input->pid && !link->s_filter)
			link->s_filter = &input->obj;

		if (link->p_filter && link->s_filter) {
			pr_debug("Linking pri %s (pid:0x%x) to sec %s (pid:0x%x)\n",
					link->p_filter->name, link->p_pid,
					link->s_filter->name, link->s_pid);
			sdata.filter = link->p_filter;
			sdata.mode = link->mode;
			res = input->obj.ops->set_control(link->s_filter,
					STM_TE_PID_FILTER_CONTROL_SECONDARY_PID,
					&sdata, sizeof(sdata));
		}
		pr_debug("Link: pri=0x%x (pf=%p) sec=0x%x (pf=%p)\n",
				link->p_pid, link->p_filter,
				link->s_pid, link->s_filter);
	}

	return 0;
}

int te_demux_pid_announce_change(struct te_in_filter *input,
		stm_te_pid_t old_pid, stm_te_pid_t new_pid)
{
	int res = 0;
	struct te_demux *demux;
	struct te_autotarget *tgt;
	struct te_linkedpid *link;
	struct te_pid_filter_secondary_data sdata = {0};

	if (input->obj.type != TE_OBJ_PID_FILTER) {
		pr_err("not a PID filter\n");
		return -EINVAL;
	}

	demux = te_demux_from_obj(input->obj.parent);
	if (demux == NULL) {
		pr_err("No parent demux\n");
		res = -ENODEV;
		goto out;
	}

	/* Search through transforms, if pid matches, detach */
	list_for_each_entry(tgt, &demux->autotargets, entry) {
		if (tgt->pid == old_pid) {
			res = input->obj.ops->detach(&input->obj, tgt->target);
			if (res != 0)
				pr_debug("ignoring detach error\n");
		}
	}

	/* Search through links and unlink any links that are using this
	 * filter.
	 * Since there may be other PID filters with the same PID we then
	 * search for another PID filter to link to */
	list_for_each_entry(link, &demux->linked_pids, entry) {
		struct te_in_filter *new_pf;
		bool relink = false;

		if (link->p_pid == old_pid && link->p_filter == &input->obj) {
			/* Unlink filters */
			if (link->s_filter) {
				pr_debug("Unlinking pri %s (pid:0x%x) from sec %s (pid:0x%x)\n",
						link->p_filter->name,
						link->p_pid,
						link->s_filter->name,
						link->s_pid);
				sdata.filter = NULL;
				link->s_filter->ops->set_control(link->s_filter,
					STM_TE_PID_FILTER_CONTROL_SECONDARY_PID,
					&sdata, sizeof(sdata));
			}
			link->p_filter = NULL;

			/* Search for new primary filter for link */
			list_for_each_entry(new_pf, &demux->in_filters,
					obj.lh) {
				if (new_pf->obj.type == TE_OBJ_PID_FILTER
						&& new_pf->pid == link->p_pid
						&& new_pf != input) {
					relink = true;
					link->p_filter = &input->obj;
					break;
				}
			}
		}

		if (link->s_pid == old_pid && link->s_filter == &input->obj) {
			/* Unlink filters */
			if (link->p_filter) {
				pr_debug("Unlinking pri %s (pid:0x%x) from sec %s (pid:0x%x)\n",
						link->p_filter->name,
						link->p_pid,
						link->s_filter->name,
						link->s_pid);
				sdata.filter = NULL;
				link->s_filter->ops->set_control(link->s_filter,
					STM_TE_PID_FILTER_CONTROL_SECONDARY_PID,
					&sdata, sizeof(sdata));
			}
			link->s_filter = NULL;

			/* Search for new secondary filter for link */
			list_for_each_entry(new_pf, &demux->in_filters,
					obj.lh) {
				if (new_pf->obj.type == TE_OBJ_PID_FILTER
						&& new_pf->pid == link->s_pid
						&& new_pf != input) {
					relink = true;
					link->s_filter = &input->obj;
					break;
				}
			}
		}
		if (relink) {
			/* New primary/secondary pair of filters found. So
			 * link them */
			pr_debug("Linking pri %s (pid:0x%x) to sec %s (pid:0x%x)\n",
					link->p_filter->name, link->p_pid,
					link->s_filter->name, link->s_pid);
			sdata.filter = link->p_filter;
			sdata.mode = link->mode;
			res = input->obj.ops->set_control(link->s_filter,
					STM_TE_PID_FILTER_CONTROL_SECONDARY_PID,
					&sdata, sizeof(sdata));
		}
	}

	/* Just incase we changed to a registered pid... */
	if (STM_TE_PID_NONE != new_pid)
		res = te_demux_pid_announce(input);
out:
	return res;
}

/*!
 * \brief Get global demux statistics
 *
 * Get actual value of statistics, without any offset from reset point
 * reference.
 *
 * \param obj   demux object
 * \param stat	pointer to stat
 *
 * \retval 0 Success
 * \retval -EINVAL HAL error
 **/
static int te_demux_get_actual_stat(struct te_demux *dmx,
					stm_te_demux_stats_t *stat)
{
	ST_ErrorCode_t hal_err;
	int err = 0;
	stptiHAL_vDeviceStreamStatistics_t hal_stat = {0};
	struct te_obj *in_filter_obj, *out_filter_obj;
	stm_te_input_filter_stats_t in_filter_stat;
	stm_te_output_filter_stats_t out_filter_stat;

	memset(stat, 0, sizeof(stm_te_demux_stats_t));

	hal_err = stptiHAL_call(vDevice.HAL_vDeviceGetStreamStatistics,
				HAL_HDL(dmx->hal_vdevice->hdl), &hal_stat);
	if (ST_NO_ERROR != hal_err) {
		pr_err("could not get stream statistics demux %p"
			"(0x%x)\n", dmx, hal_err);
		err = -EINVAL;
		goto exit;
	}

	/* Stats that are retrieved directly from HAL */
	stat->packet_count = hal_stat.PacketCount;
	stat->cc_errors = hal_stat.CCErrorCount;
	stat->tei_count = hal_stat.TransportErrorCount;
	stat->buffer_overflows = hal_stat.BufferOverflowCount;
	stat->utilization = hal_stat.Utilization;

	/* Sum up all input filters stats belonged to this demux */
	list_for_each_entry(in_filter_obj, &dmx->in_filters, lh) {
		err = in_filter_obj->ops->get_control(in_filter_obj,
				STM_TE_INPUT_FILTER_CONTROL_STATUS,
				&in_filter_stat,
				sizeof(stm_te_input_filter_stats_t));
		if (err) {
			pr_err("Failed to get input filter stat\n");
			goto exit;
		}

		stat->input_packets += in_filter_stat.packet_count;
	}

	read_lock(&dmx->out_flt_rw_lock);
	/* Sum up all output filters stat belonged to this demux */
	list_for_each_entry(out_filter_obj, &dmx->out_filters, lh) {
		read_unlock(&dmx->out_flt_rw_lock);
		err = out_filter_obj->ops->get_control(out_filter_obj,
				STM_TE_OUTPUT_FILTER_CONTROL_STATUS,
				&out_filter_stat,
				sizeof(stm_te_output_filter_stats_t));
		if (err) {
			pr_err("Failed to get out filter stat\n");
			goto exit;
		}

		stat->crc_errors += out_filter_stat.crc_errors;
		stat->output_packets += out_filter_stat.packet_count;

		/* don't sum up overflow count. Vdevice will do this and overflow
		count not supported on output_filter stat */
		read_lock(&dmx->out_flt_rw_lock);
	}
	read_unlock(&dmx->out_flt_rw_lock);
exit:
	return err;
}

/*!
 * \brief Reset global demux statistics
 *
 * This function will store current statistics into a reference variable.
 * The reference will later be used as to offset the stats.
 *
 * \param demux    Pointer to demux
 *
 * \retval 0       Success
 * \retval -EINVAL Bad handle
 * \retval -EIO    Hardware error
 */
int te_demux_reset_stat(struct te_obj *obj)
{
	int err;
	struct te_demux *demux = te_demux_from_obj(obj);
	stm_te_demux_stats_t stat;

	err = te_demux_get_actual_stat(demux, &stat);
	if (err) {
		pr_err("Failed to get actual stat\n");
		goto exit;
	}

	memcpy(&demux->stat_upon_reset, &stat, sizeof(stm_te_demux_stats_t));

exit:
	return err;
}

int te_demux_link_secondary_pid(stm_te_object_h dmx_h, stm_te_pid_t pri,
				stm_te_pid_t sec,
				stm_te_secondary_pid_mode_t mode)
{
	struct te_linkedpid *link;
	int res = 0;
	struct te_obj *dmx_o;
	struct te_demux *demux;
	struct te_in_filter *input;
	struct te_pid_filter_secondary_data sdata;

	res = te_obj_from_hdl(dmx_h, &dmx_o);
	if (res != 0)
		goto out;

	demux = te_demux_from_obj(dmx_o);
	if (demux == NULL) {
		res = -ENODEV;
		goto out;
	}

	/* Check these pids haven't already been linked */
	list_for_each_entry(link, &demux->linked_pids, entry) {
		if (link->p_pid == pri || link->s_pid == pri
			|| link->p_pid == sec || link->s_pid == sec) {
			pr_err("pid 0x%04x already linked to 0x%04x\n",
				link->p_pid, link->s_pid);
			res = -EINVAL;
			goto out;
		}
	}

	link = kzalloc(sizeof(*link), GFP_KERNEL);
	if (link == NULL) {
		pr_err("couldn't allocate link object\n");
		res = -ENOMEM;
		goto out;
	}

	link->p_pid = pri;
	link->s_pid = sec;
	link->mode = mode;

	INIT_LIST_HEAD(&link->entry);
	list_add(&link->entry, &demux->linked_pids);

	pr_debug("linked pid 0x%04x -> 0x%04x\n", pri, sec);

	list_for_each_entry(input, &demux->in_filters, obj.lh) {
		if (input->obj.type == TE_OBJ_PID_FILTER && input->pid == pri
				&& !link->p_filter)
			link->p_filter = &input->obj;
		if (input->obj.type == TE_OBJ_PID_FILTER && input->pid == sec
				&& !link->s_filter)
			link->s_filter = &input->obj;

		if (link->p_filter && link->s_filter) {
			pr_debug("Linking pri %s (pid:0x%x) to sec %s (pid:0x%x)\n",
					link->p_filter->name, link->p_pid,
					link->s_filter->name, link->s_pid);
			sdata.filter = link->p_filter;
			sdata.mode = mode;
			res = input->obj.ops->set_control(link->s_filter,
					STM_TE_PID_FILTER_CONTROL_SECONDARY_PID,
					&sdata, sizeof(sdata));
		}
	}
out:
	return res;
}

int te_demux_unlink_secondary_pid(stm_te_object_h dmx_h, stm_te_pid_t pid)
{
	int res = 0;
	struct te_linkedpid *link, *tmp;
	struct te_obj *dmx_o;
	struct te_demux *demux;
	struct te_pid_filter_secondary_data sdata;

	res = te_obj_from_hdl(dmx_h, &dmx_o);
	if (res != 0)
		goto out;

	demux = te_demux_from_obj(dmx_o);
	if (demux == NULL) {
		res = -ENODEV;
		goto out;
	}

	list_for_each_entry_safe(link, tmp, &demux->linked_pids, entry) {
		if (link->p_pid == pid) {
			pr_debug("unlinked pid 0x%04x -> 0x%04x\n",
					link->p_pid, link->s_pid);
			if (link->p_filter && link->s_filter) {
				pr_debug("Unlinking pri %s (pid:0x%x) from sec %s (pid:0x%x)\n",
						link->p_filter->name,
						link->p_pid,
						link->s_filter->name,
						link->s_pid);
				sdata.filter = NULL;
				link->s_filter->ops->set_control(link->s_filter,
					STM_TE_PID_FILTER_CONTROL_SECONDARY_PID,
					&sdata, sizeof(sdata));
			}
			list_del(&link->entry);
			kfree(link);
			goto out;
		}
	}

	pr_err("PID 0x%04x not linked\n", pid);
	res = -EINVAL;

out:
	return res;
}

static int te_demux_update_duplicate(struct te_demux *dmx)
{
	ST_ErrorCode_t hal_err;
	BOOL val = dmx->discard_dupe_ts == 0 ? FALSE : TRUE;

	pr_debug("setting discard duplicate: %d\n", val);

	hal_err = stptiHAL_call(vDevice.HAL_vDeviceFeatureEnable,
				dmx->hal_vdevice->hdl,
				stptiHAL_VDEVICE_DISCARD_DUPLICATE_PKTS,
				val);

	return te_hal_err_to_errno(hal_err);
}
