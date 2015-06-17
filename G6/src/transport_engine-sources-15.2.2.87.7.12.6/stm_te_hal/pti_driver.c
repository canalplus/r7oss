/*****************************************************************************/
/* COPYRIGHT (C) 2009 STMicroelectronics - All Rights Reserved               */
/* ST makes no warranty express or implied including but not limited to,     */
/* any warranty of                                                           */
/*                                                                           */
/*   (i)  merchantability or fitness for a particular purpose and/or         */
/*   (ii) requirements, for a particular purpose in relation to the LICENSED */
/*        MATERIALS, which is provided "AS IS", WITH ALL FAULTS. ST does not */
/*        represent or warrant that the LICENSED MATERIALS provided here     */
/*        under is free of infringement of any third party patents,          */
/*        copyrights,trade secrets or other intellectual property rights.    */
/*        ALL WARRANTIES, CONDITIONS OR OTHER TERMS IMPLIED BY LAW ARE       */
/*        EXCLUDED TO THE FULLEST EXTENT PERMITTED BY LAW                    */
/*                                                                           */
/*****************************************************************************/
/**
   @file   pti_driver.c
   @brief  Main driver file.

   This file is the interface to HAL.  It contains the STPTI global variable for managing the STPTI
   devices, and references to the HAL objects.

   It is this file that would change should the driver need to support different STPTI architectures
   simulateously.

   It also contains the mechanism for relating the Device Name to a Device Handle, the device name
   being purely a STAPI concept (and hence abstracted away from the PTI HAL).

 */

#if 0
#define STPTI_PRINT
#endif

/* Includes ---------------------------------------------------------------- */
#include "linuxcommon.h"
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>

/* STAPI includes */

/* Includes from API level */
#include "pti_driver.h"
#include "pti_debug.h"
#include "pti_osal.h"
#include "stm_te_version.h"

/* Includes from the HAL / ObjMan level */
#include "objman/pti_object.h"
#include "objman/pti_container.h"
#include "tango/pti_pdevice.h"
#include "tango/pti_vdevice.h"
#include "tango/pti_session.h"
#include "stfe/pti_tsinput.h"
#include "tango/pti_swinjector.h"
#include "tango/pti_buffer.h"
#include "tango/pti_filter.h"
#include "tango/pti_signal.h"
#include "tango/pti_index.h"
#include "tango/pti_slot.h"
#include "tango/pti_dataentry.h"

/* Lite HAL includes */
#include "tango_lite/pti_pdevice_lite.h"
#include "tango_lite/pti_vdevice_lite.h"
#include "tango_lite/pti_session_lite.h"
#include "tango_lite/pti_swinjector_lite.h"
#include "tango_lite/pti_buffer_lite.h"
#include "tango_lite/pti_signal_lite.h"
#include "tango_lite/pti_slot_lite.h"

/* MACROS ------------------------------------------------------------------ */
#if !defined( STPTI_EVENT_STACK_SIZE )
#define STPTI_EVENT_STACK_SIZE      (1024*16)
#endif

#if !defined( STPTI_EVENT_TASK_PRIORITY )
#define STPTI_EVENT_TASK_PRIORITY   (MAX_USER_PRIORITY)
#endif


typedef struct {
	struct task_struct *EventTask_p;
	struct task_struct *LiveTask_p;
} stptiAPI_pDeviceAuxData_t;
/* Private Constants ------------------------------------------------------- */

/* Global Variables -------------------------------------------------------- */
STPTI_Driver_t STPTI_Driver;	/* Useful for debugging a single structure holding driver status */

static int thread_dmx_halevent[2] = { SCHED_NORMAL, 0 };
module_param_array_named(thread_DMX_HalEvent, thread_dmx_halevent, int, NULL, 0644);
MODULE_PARM_DESC(thread_DMX_HalEvent, "DMX-HalEvent thread:s(Mode),p(Priority)");

static unsigned int thread_dmx_hallive[2] = { 0, 0 };
module_param_array_named(thread_DMX_HalLive,thread_dmx_hallive, uint, NULL, 0644);
MODULE_PARM_DESC(thread_DMX_HalLive, "DMX-HalLive thread:s(Mode),p(Priority)");

/* Private Variables ------------------------------------------------------- */
/* Private Function Prototypes --------------------------------------------- */
/* Functions --------------------------------------------------------------- */
static int stfe_resume(struct device *dev);
static int tango_resume(struct device *dev);
static int tango_suspend(struct device *dev);
static int stfe_suspend(struct device *dev);

/**
 * @brief  Check access permission, block caller if not allowed and increment user count
 */
void STPTIHAL_entry(void)
{
	wait_event(STPTI_Driver.HALWaitQ, STPTI_Driver.HALAllow);
	atomic_inc(&STPTI_Driver.HALUsers);
}

int STPTIHAL_PowerState(void)
{
	return STPTI_Driver.Power_Enabled;
}

int TSHAL_PowerState(void)
{
	return STPTI_Driver.FE_Power_Enabled;
}
/**
 * @brief  Decrement user count and release suspend
 */
void STPTIHAL_exit(void)
{
	if (atomic_dec_and_test(&STPTI_Driver.HALUsers) && !STPTI_Driver.HALAllow) {
		complete(&STPTI_Driver.HALIdle);
	}
}

/**
 * @brief  Returns the number of pDevices registered with the driver
 *
 * @param NumPDevice - Pointer set to the current number of registered
 *                     pDevices
 *
 * @return A standard error type
 */
ST_ErrorCode_t stptiAPI_DriverGetNumberOfpDevices(int *NumPDevice)
{
	*NumPDevice = STPTI_Driver.NumberOfpDevices;
	return ST_NO_ERROR;
}

/**
   @brief  The Event Task

   This function calls in to the stptiHAL for handling events.  It will return only upon error,
   or HAL_pDeviceEventTaskAbort is called.

   It is useful to have the Event Task created and destroyed outside of the HAL, in the API layer.
   This is because the Event Task can cause re-entrancy issues, as the resulting event callbacks
   can be used to call STPTI5 APIs.  As a result when wanting to terminate  the task you will need
   to make sure that all locks are released (including the API lock) so that these can be APIs
   can be executed.

   So the physical device lifecycle (w.r.t. Event Task) is...

     1. Allocate pDevice
     2. Create Event Task
     3. Start Event Task
     4. Allocate vDevices
     5. Deallocate vDevices
     6. Abort Event Task (HAL_pDeviceEventTaskAbort)
     7. Make sure API locks are free (to allow Event Task callbacks to complete)
     8. Delete Event Task
     9. Deallocate pDevice

   @return                     None

 */
static void stptiAPI_EventNotifyTask(void *Param)
{
	FullHandle_t pDeviceHandle;

	pDeviceHandle.word = (U32) Param;

	if (ST_NO_ERROR != stptiHAL_call_unlocked(pDevice.HAL_pDeviceEventTask, pDeviceHandle)) {
		STPTI_PRINTF_ERROR("Failure to Start Event Task");
	}
}

/**
   @brief  The STFE monitor Task for hardware workaround

   @return                     None

 */
static void stptiAPI_LiveMonitorTask(void *Param)
{
	FullHandle_t pDeviceHandle;

	pDeviceHandle.word = (U32) Param;

	if (ST_NO_ERROR != stptiHAL_call_unlocked(pDevice.HAL_pDeviceLiveTask, pDeviceHandle)) {
		STPTI_PRINTF_ERROR("Failure to Start Live Monitor Task");
	}
}

static int stpti_tp_probe(struct platform_device *pdev)
{
	int err = 0;
	ST_ErrorCode_t hal_err;
	HAL_API_t *hal_api;
	FullHandle_t pdevice_handle;
	stptiAPI_pDeviceAuxData_t *aux_data;
	struct stpti_tp_config *pdata;
	struct sched_param param;

	pr_info("Probed %s device\n", pdev->name);

#ifdef CONFIG_OF
	if (pdev->dev.of_node) {
		err = stpti_dt_get_tp_pdata(pdev);
		if (err)
			return err;
	}
#endif
	pdata = pdev->dev.platform_data;

	pdata->id = STPTI_Driver.NumberOfpDevices;

	if (STPTI_Driver.cachedTPFW == NULL && pdata->firmware) {
		pr_info("Caching firmware %s\n", pdata->firmware);
		err = request_firmware(&STPTI_Driver.cachedTPFW, pdata->firmware, &pdev->dev);
		if (err) {
			pr_info("Could not cache TP firmware %s (%d)\n", pdata->firmware, err);
			return err;
		}
	}

	STPTI_Driver.HALAllow = true;

	init_waitqueue_head(&STPTI_Driver.HALWaitQ);
	init_completion(&STPTI_Driver.HALIdle);

	STPTI_Driver.ObjMan[pdata->id] = kzalloc(sizeof(ObjectManagerRoot_t), GFP_KERNEL);
	hal_api = STPTI_Driver.HALFunctionPool[pdata->id] = kzalloc(sizeof(HAL_API_t), GFP_KERNEL);
	if (!STPTI_Driver.HALFunctionPool[pdata->id] || !STPTI_Driver.ObjMan[pdata->id]) {
		err = -ENOMEM;
		goto error;
	}

	pr_debug("Registering Objects for TANGO, physical device %d", pdata->id);

	/* Assign function pointers to the to the HALFunctionPool */
	hal_api->pDevice = stptiHAL_pDeviceAPI;
	hal_api->vDevice = stptiHAL_vDeviceAPI;
	hal_api->Session = stptiHAL_SessionAPI;
	hal_api->SoftwareInjector = stptiHAL_SoftwareInjectorAPI;
	hal_api->Buffer = stptiHAL_BufferAPI;
	hal_api->Filter = stptiHAL_FilterAPI;
	hal_api->Signal = stptiHAL_SignalAPI;
	hal_api->Index = stptiHAL_IndexAPI;
	hal_api->Slot = stptiHAL_SlotAPI;
	hal_api->DataEntry = stptiHAL_DataEntryAPI;
	hal_api->Container = stptiHAL_ContainerAPI;

	/* Register all the objects for this physical device
	 * This involves supplying the size of the alloc, dealloc, assoc,
	 * disassoc function pointers */
	hal_err = stptiOBJMAN_RegisterObjectType(pdata->id,
						 OBJECT_TYPE_PDEVICE, sizeof(stptiHAL_pDevice_t),
						 &hal_api->pDevice.ObjectManagementFunctions);
	if (ST_NO_ERROR == hal_err)
		hal_err = stptiOBJMAN_RegisterObjectType(pdata->id,
							 OBJECT_TYPE_VDEVICE, sizeof(stptiHAL_vDevice_t),
							 &hal_api->vDevice.ObjectManagementFunctions);
	if (ST_NO_ERROR == hal_err)
		hal_err = stptiOBJMAN_RegisterObjectType(pdata->id,
							 OBJECT_TYPE_SESSION, sizeof(stptiHAL_Session_t),
							 &hal_api->Session.ObjectManagementFunctions);
	if (ST_NO_ERROR == hal_err)
		hal_err = stptiOBJMAN_RegisterObjectType(pdata->id,
							 OBJECT_TYPE_SOFTWARE_INJECTOR,
							 sizeof(stptiHAL_SoftwareInjector_t),
							 &hal_api->SoftwareInjector.ObjectManagementFunctions);
	if (ST_NO_ERROR == hal_err)
		hal_err = stptiOBJMAN_RegisterObjectType(pdata->id,
							 OBJECT_TYPE_BUFFER, sizeof(stptiHAL_Buffer_t),
							 &hal_api->Buffer.ObjectManagementFunctions);
	if (ST_NO_ERROR == hal_err)
		hal_err = stptiOBJMAN_RegisterObjectType(pdata->id,
							 OBJECT_TYPE_FILTER, sizeof(stptiHAL_Filter_t),
							 &hal_api->Filter.ObjectManagementFunctions);
	if (ST_NO_ERROR == hal_err)
		hal_err = stptiOBJMAN_RegisterObjectType(pdata->id,
							 OBJECT_TYPE_SIGNAL, sizeof(stptiHAL_Signal_t),
							 &hal_api->Signal.ObjectManagementFunctions);
	if (ST_NO_ERROR == hal_err)
		hal_err = stptiOBJMAN_RegisterObjectType(pdata->id,
							 OBJECT_TYPE_INDEX, sizeof(stptiHAL_Index_t),
							 &hal_api->Index.ObjectManagementFunctions);
	if (ST_NO_ERROR == hal_err)
		hal_err = stptiOBJMAN_RegisterObjectType(pdata->id,
							 OBJECT_TYPE_SLOT, sizeof(stptiHAL_Slot_t),
							 &hal_api->Slot.ObjectManagementFunctions);
	if (ST_NO_ERROR == hal_err)
		hal_err = stptiOBJMAN_RegisterObjectType(pdata->id,
							 OBJECT_TYPE_DATA_ENTRY,
							 sizeof(stptiHAL_DataEntry_t),
							 &hal_api->DataEntry.ObjectManagementFunctions);
	if (ST_NO_ERROR == hal_err)
		hal_err = stptiOBJMAN_RegisterObjectType(pdata->id,
							 OBJECT_TYPE_CONTAINER,
							 sizeof(stptiHAL_Container_t),
							 &hal_api->Container.ObjectManagementFunctions);

	if (ST_NO_ERROR != hal_err) {
		pr_err("Failed to register objects 0x%x\n", hal_err);
		err = -EINVAL;
		goto error;
	}

	/* Also setup access Write Lock for this physical device */
	stptiOBJMAN_InitWriteLock(pdata->id);

	/* Increment the number of pDevices otherwise the OBJMAN will fail */
	STPTI_Driver.NumberOfpDevices++;

	/* We allocate the pDevice object - we give its own handle as the
	 * parent since it is the root object */
	hal_err = stptiOBJMAN_AllocateObject(stptiOBJMAN_pDeviceObjectHandle(pdata->id),
					     OBJECT_TYPE_PDEVICE, &pdevice_handle, pdev);
	if (ST_NO_ERROR != hal_err) {
		pr_err("Failed to initialise TP device %d (0x%x)\n", pdata->id, hal_err);
		err = -ENODEV;
		goto error_post_pdevice;
	}

	/* Enable runtime power management */

	/* Start TP task (STFE task started in STFE probe) */
	aux_data = stptiOBJMAN_AllocateObjectAuxiliaryData(pdevice_handle, sizeof(stptiAPI_pDeviceAuxData_t));
	if (!aux_data) {
		err = -ENOMEM;
		goto error_post_pdevice;
	}
	memset(aux_data, 0, sizeof(stptiAPI_pDeviceAuxData_t));
	aux_data->EventTask_p = kthread_run((int (*)(void *))stptiAPI_EventNotifyTask,
					    (void *)pdevice_handle.word, "DMX-HalEvent-%d", pdata->id);
	if (IS_ERR(aux_data->EventTask_p)) {
		err = -ENOMEM;
		goto error_post_pm;
	}

	/* switch to real time scheduling (if requested) */
	if (thread_dmx_halevent[1]) {
		param.sched_priority = thread_dmx_halevent[1];
		if (0 != sched_setscheduler(aux_data->EventTask_p, thread_dmx_halevent[0], &param)) {
			pr_err("FAILED to set scheduling parameters to priority %d Mode :(%d)\n", \
			thread_dmx_halevent[1], thread_dmx_halevent[0]);
		}
	}
	STPTI_Driver.Power_Enabled = 1;
	/* if driver is power down by DT then
	 * resume the driver and suspend it to make sure
	 * hw if really off */
	/* Set Power enabled to false , i.e no more access
	 * to this hw */
	if (pdata->power_down == 1) {
		tango_resume(&pdev->dev);
		tango_suspend(&pdev->dev);
		STPTI_Driver.Power_Enabled = 0;
		return 0;
	}

	err = pm_runtime_set_active(&pdev->dev);
	if (err) {
		pr_err("pm_runtime_set_active error 0x%x\n", err);
		goto error_post_pm;
	}
	pm_suspend_ignore_children(&pdev->dev, 1);
	pm_runtime_enable(&pdev->dev);

	return 0;

error_post_pm:
	pm_runtime_disable(&pdev->dev);

error_post_pdevice:
	stptiOBJMAN_DeallocateObject(pdevice_handle, TRUE);
	STPTI_Driver.NumberOfpDevices--;

error:
	kfree(STPTI_Driver.ObjMan[pdata->id]);
	kfree(STPTI_Driver.HALFunctionPool[pdata->id]);

	if (STPTI_Driver.NumberOfpDevices == 0 && STPTI_Driver.cachedTPFW) {
		release_firmware(STPTI_Driver.cachedTPFW);
		STPTI_Driver.cachedTPFW = NULL;
	}

	return err;
}

static int __exit stpti_tp_remove(struct platform_device *pdev)
{
	ST_ErrorCode_t hal_err;
	FullHandle_t pdevice_handle;
	struct stpti_tp_config *pdata = pdev->dev.platform_data;

	pr_info("Removing Tango TP device %d\n", pdata->id);

	/* Disable power management for this device */

	pdevice_handle = stptiOBJMAN_pDeviceObjectHandle(pdata->id);

	/* Stop event task */
	hal_err = stptiHAL_call_unlocked(pDevice.HAL_pDeviceEventTaskAbort, pdevice_handle);
	if (ST_NO_ERROR == hal_err) {
		stptiAPI_pDeviceAuxData_t *aux_data = stptiOBJMAN_GetObjectAuxiliaryData_p(pdevice_handle);
		if (aux_data && aux_data->EventTask_p) {
			kthread_stop(aux_data->EventTask_p);
			pr_debug("Deleted Event Task %p", aux_data->EventTask_p);
			aux_data->EventTask_p = NULL;
		}
	} else {
		pr_err("Error stopping event task for pdevice %d\n", pdata->id);
		return -EINVAL;
	}

	hal_err = stptiOBJMAN_DeallocateObject(stptiOBJMAN_pDeviceObjectHandle(pdata->id), TRUE);
	if (ST_NO_ERROR != hal_err) {
		pr_err("Failed to deallocate pdevice %d\n", pdata->id);
		return -EINVAL;
	}

	/* Deallocate access Write Lock for this physical device */
	stptiOBJMAN_TermWriteLock(pdata->id);
	pm_runtime_disable(&pdev->dev);

	kfree(STPTI_Driver.ObjMan[pdata->id]);
	kfree(STPTI_Driver.HALFunctionPool[pdata->id]);

#ifdef CONFIG_OF
	if (pdev->dev.of_node)
		stpti_dt_put_tp_pdata(pdev);
#endif

	STPTI_Driver.NumberOfpDevices--;

	if (STPTI_Driver.NumberOfpDevices == 0 && STPTI_Driver.cachedTPFW) {
		release_firmware(STPTI_Driver.cachedTPFW);
		STPTI_Driver.cachedTPFW = NULL;
	}

	return 0;
}

static int tango_suspend(struct device *dev)
{
	int err = 0;
	struct platform_device *pdev = to_platform_device(dev);
	FullHandle_t pdevice_handle;
	FullHandle_t vdevice_handle[MAX_NUMBER_OF_VDEVICES];
	ST_ErrorCode_t hal_err;
	int vdevice_count;
	int i = 0;

	struct stpti_tp_config *pdata = pdev->dev.platform_data;

#ifdef CONFIG_PM_RUNTIME
	/* if dev->power.runtime_status=RPM_SUSPENDED then no need
	* to suspend again */
	if (pm_runtime_suspended(dev))
		return 0;
#endif
	STPTI_Driver.HALAllow = false;
	if (atomic_read(&STPTI_Driver.HALUsers) > 0) {
		pr_info("waiting for HAL to become idle...\n");
		wait_for_completion(&STPTI_Driver.HALIdle);
		pr_info("HAL is idle\n");
	}

	pdevice_handle = stptiOBJMAN_pDeviceObjectHandle(pdata->id);

	/* Power-down all child vDevices */
	vdevice_count = stptiOBJMAN_ReturnChildObjects(pdevice_handle,
						       vdevice_handle, MAX_NUMBER_OF_VDEVICES, OBJECT_TYPE_VDEVICE);
	for (i = 0; i < vdevice_count; i++) {
		hal_err = stptiHAL_call_unlocked(vDevice.HAL_vDevicePowerDown, vdevice_handle[i]);
		if (ST_NO_ERROR != hal_err) {
			pr_err("Unable to power-down child vDevice %d of pDevice %d (e=%x)\n", i, pdata->id, hal_err);
			err = -EIO;
			goto error;
		}
	}

	/* Put pDevice into sleep state */
	hal_err = stptiHAL_call_unlocked(pDevice.HAL_pDeviceSetPowerState, pdevice_handle, stptiHAL_PDEVICE_SLEEPING);
	if (ST_NO_ERROR != hal_err) {
		pr_err("Unable to suspend pDevice %d (e=%x)\n", pdata->id, hal_err);
		err = -EIO;
		goto error;
	}

	pr_info("pDevice %d suspended\n", pdata->id);

#ifdef CONFIG_ARCH_STI
	/* Power-down the pDevice once suspended */
	hal_err = stptiHAL_call_unlocked(pDevice.HAL_pDeviceSetPowerState, pdevice_handle, stptiHAL_PDEVICE_POWERDOWN);
	if (ST_NO_ERROR != hal_err) {
		pr_err("Unable to power-down pDevice %d (e=%x)\n", pdata->id, hal_err);
		err = -EIO;
		goto error;
	}

	if (!err)
		pr_info("pDevice %d frozen\n", pdata->id);
#endif
	return 0;

error:
	/* Restore vdevice power on error */
	for (i = 0; i < vdevice_count; i++) {
		stptiHAL_call_unlocked(vDevice.HAL_vDevicePowerUp, vdevice_handle[i]);
	}
	return err;
}

static int tango_resume(struct device *dev)
{
	int err = 0;
	struct platform_device *pdev = to_platform_device(dev);
	FullHandle_t pdevice_handle;
	FullHandle_t vdevice_handle[MAX_NUMBER_OF_VDEVICES];
	ST_ErrorCode_t hal_err;
	int vdevice_count;
	int i = 0;
	struct stpti_tp_config *pdata = pdev->dev.platform_data;

	STPTI_Driver.HALAllow = true;

	pdevice_handle = stptiOBJMAN_pDeviceObjectHandle(pdata->id);

#ifdef CONFIG_PM_RUNTIME
	/* if dev->power.runtime_status=RPM_SUSPENDED then don't
	* resume */
	if (pm_runtime_suspended(dev))
		return 0;
#endif
#ifdef CONFIG_ARCH_STI
	/* Power-up the pDevice to the sleeping state */
	hal_err = stptiHAL_call_unlocked(pDevice.HAL_pDeviceSetPowerState, pdevice_handle, stptiHAL_PDEVICE_SLEEPING);
	if (ST_NO_ERROR != hal_err) {
		pr_err("Unable to power-up pDevice %d (e=%x)\n", pdata->id, hal_err);
		err = -EIO;
		return err;
	}
#endif

	vdevice_count = stptiOBJMAN_ReturnChildObjects(pdevice_handle,
						       vdevice_handle, MAX_NUMBER_OF_VDEVICES, OBJECT_TYPE_VDEVICE);
	/* Resume pDevice if it has one or more child vDevices, otherwise it
	 * remains in sleep state until the first child vDevice is created */
	if (vdevice_count > 0) {
		hal_err = stptiHAL_call_unlocked(pDevice.HAL_pDeviceSetPowerState, pdevice_handle, stptiHAL_PDEVICE_STARTED);
		if (ST_NO_ERROR != hal_err) {
			pr_err("Unable to start pDevice %d (e=%x)\n", pdata->id, hal_err);
			err = -EIO;
		}
	} else {
		pr_info("pDevice %d has no child vDevices. Remains asleep\n", pdata->id);
	}

	/* Power-up all child vDevices */
	for (i = 0; i < vdevice_count && !err; i++) {
		hal_err = stptiHAL_call_unlocked(vDevice.HAL_vDevicePowerUp, vdevice_handle[i]);
		if (ST_NO_ERROR != hal_err) {
			pr_err("Unable to power-up child vDevice %d of pDevice %d (e=%x)\n", i, pdata->id, hal_err);
			/* Ignore vDevice power-up errors */
		}
	}

	wake_up_all(&STPTI_Driver.HALWaitQ);

	if (!err)
		pr_info("pDevice %d resumed\n", pdata->id);

	return err;
}

static int tango_idle(struct device *dev)
{
	pr_info("pDevice idle \n");
	/* Nothing to do */
	return 0;
}

#ifndef CONFIG_ARCH_STI
static int tango_freeze(struct device *dev)
{
	int err = 0;
	struct platform_device *pdev = to_platform_device(dev);
	FullHandle_t pdevice_handle;
	ST_ErrorCode_t hal_err;
	struct stpti_tp_config *pdata = pdev->dev.platform_data;

	pdevice_handle = stptiOBJMAN_pDeviceObjectHandle(pdata->id);

	/* First suspend this pDevice */
	err = tango_suspend(dev);
	if (!err) {
		/* Power-down the pDevice once suspended */
		hal_err = stptiHAL_call_unlocked(pDevice.HAL_pDeviceSetPowerState, pdevice_handle, stptiHAL_PDEVICE_POWERDOWN);
		if (ST_NO_ERROR != hal_err) {
			pr_err("Unable to power-down pDevice %d (e=%x)\n", pdata->id, hal_err);
			err = -EIO;
		}
	}

	if (!err)
		pr_info("pDevice %d frozen\n", pdata->id);

	return err;
}

static int tango_restore(struct device *dev)
{
	int err = 0;
	struct platform_device *pdev = to_platform_device(dev);
	FullHandle_t pdevice_handle;
	ST_ErrorCode_t hal_err;
	struct stpti_tp_config *pdata = pdev->dev.platform_data;

	pdevice_handle = stptiOBJMAN_pDeviceObjectHandle(pdata->id);

	/* Power-up the pDevice to the sleeping state */
	hal_err = stptiHAL_call_unlocked(pDevice.HAL_pDeviceSetPowerState, pdevice_handle, stptiHAL_PDEVICE_SLEEPING);
	if (ST_NO_ERROR != hal_err) {
		pr_err("Unable to power-up pDevice %d (e=%x)\n", pdata->id, hal_err);
		err = -EIO;
	}

	if (!err) {
		pr_info("pDevice %d restored\n", pdata->id);
		/* Resume the pDevice from sleeping -> running
		 * Note: if the pDevice has no child vDevices it will remain
		 * sleeping */
		err = tango_resume(dev);
	}

	return err;
}
#endif

static int stpti_tango_lite_probe(struct platform_device *pdev)
{
	int err = 0;
	ST_ErrorCode_t hal_err;
	HAL_API_t *hal_api;
	FullHandle_t pdevice_handle;
	stptiAPI_pDeviceAuxData_t *aux_data;

	pr_info("Probed %s device\n", pdev->name);

	STPTI_Driver.HALAllow = true;
	STPTI_Driver.Power_Enabled = 1;

	init_waitqueue_head(&STPTI_Driver.HALWaitQ);
	init_completion(&STPTI_Driver.HALIdle);

	/* Only support a single tango lite device */
	if (STPTI_Driver.NumberOfpDevices != 0) {
		dev_err(&pdev->dev, "Only a single tango lite device is supported\n");
		return -EINVAL;
	}

	STPTI_Driver.ObjMan[0] = kzalloc(sizeof(ObjectManagerRoot_t), GFP_KERNEL);
	hal_api = STPTI_Driver.HALFunctionPool[0] = kzalloc(sizeof(HAL_API_t), GFP_KERNEL);
	if (!STPTI_Driver.HALFunctionPool[0] || !STPTI_Driver.ObjMan[0]) {
		err = -ENOMEM;
		goto error;
	}

	/* Assign function pointers to the to the HALFunctionPool */
	hal_api->pDevice = stptiHAL_pDeviceAPI_Lite;
	hal_api->vDevice = stptiHAL_vDeviceAPI_Lite;
	hal_api->Session = stptiHAL_SessionAPI;
	hal_api->SoftwareInjector = stptiHAL_SoftwareInjectorAPI_Lite;
	hal_api->Buffer = stptiHAL_BufferAPI_Lite;
	hal_api->Signal = stptiHAL_SignalAPI_Lite;
	hal_api->Slot = stptiHAL_SlotAPI_Lite;

	/* Register all the objects for this physical device
	 * This involves supplying the size of the alloc, dealloc, assoc,
	 * disassoc function pointers */
	hal_err = stptiOBJMAN_RegisterObjectType(0,
						 OBJECT_TYPE_PDEVICE,
						 sizeof(stptiHAL_pDevice_lite_t),
						 &hal_api->pDevice.ObjectManagementFunctions);
	if (ST_NO_ERROR == hal_err)
		hal_err = stptiOBJMAN_RegisterObjectType(0,
							 OBJECT_TYPE_VDEVICE,
							 sizeof(stptiHAL_vDevice_lite_t),
							 &hal_api->vDevice.ObjectManagementFunctions);
	if (ST_NO_ERROR == hal_err)
		hal_err = stptiOBJMAN_RegisterObjectType(0,
							 OBJECT_TYPE_SESSION,
							 sizeof(stptiHAL_Session_lite_t),
							 &hal_api->Session.ObjectManagementFunctions);
	if (ST_NO_ERROR == hal_err)
		hal_err = stptiOBJMAN_RegisterObjectType(0,
							 OBJECT_TYPE_SOFTWARE_INJECTOR,
							 sizeof(stptiHAL_SoftwareInjector_lite_t),
							 &hal_api->SoftwareInjector.ObjectManagementFunctions);
	if (ST_NO_ERROR == hal_err)
		hal_err = stptiOBJMAN_RegisterObjectType(0,
							 OBJECT_TYPE_BUFFER,
							 sizeof(stptiHAL_Buffer_lite_t),
							 &hal_api->Buffer.ObjectManagementFunctions);
	if (ST_NO_ERROR == hal_err)
		hal_err = stptiOBJMAN_RegisterObjectType(0,
							 OBJECT_TYPE_SIGNAL,
							 sizeof(stptiHAL_Signal_lite_t),
							 &hal_api->Signal.ObjectManagementFunctions);
	if (ST_NO_ERROR == hal_err)
		hal_err = stptiOBJMAN_RegisterObjectType(0,
							 OBJECT_TYPE_SLOT,
							 sizeof(stptiHAL_Slot_lite_t),
							 &hal_api->Slot.ObjectManagementFunctions);
	if (ST_NO_ERROR != hal_err) {
		pr_err("Failed to register objects 0x%x\n", hal_err);
		err = -EINVAL;
		goto error;
	}

	/* Also setup access Write Lock for this physical device */
	stptiOBJMAN_InitWriteLock(0);

	/* Increment the number of pDevices otherwise the OBJMAN will fail */
	STPTI_Driver.NumberOfpDevices++;

	/* We allocate the pDevice object - we give its own handle as the
	 * parent since it is the root object */
	hal_err = stptiOBJMAN_AllocateObject(stptiOBJMAN_pDeviceObjectHandle(0),
					     OBJECT_TYPE_PDEVICE, &pdevice_handle, pdev);
	if (ST_NO_ERROR != hal_err) {
		pr_err("Failed to initialise TP device %d (0x%x)\n", 0, hal_err);
		err = -ENODEV;
		goto error_post_pdevice;
	}

	/* Enable runtime power management */
	err = pm_runtime_set_active(&pdev->dev);
	if (err) {
		pr_err("pm_runtime_set_active error 0x%x\n", err);
		goto error_post_pdevice;
	}
	pm_suspend_ignore_children(&pdev->dev, 1);
	pm_runtime_enable(&pdev->dev);

	/* Allocate auxiliary data for this pDevice */
	aux_data = stptiOBJMAN_AllocateObjectAuxiliaryData(pdevice_handle, sizeof(stptiAPI_pDeviceAuxData_t));
	if (!aux_data) {
		err = -ENOMEM;
		goto error_post_pm;
	}
	return 0;

error_post_pm:
	pm_runtime_disable(&pdev->dev);

error_post_pdevice:
	stptiOBJMAN_DeallocateObject(pdevice_handle, TRUE);
	STPTI_Driver.NumberOfpDevices--;

error:
	kfree(STPTI_Driver.ObjMan[0]);
	kfree(STPTI_Driver.HALFunctionPool[0]);

	return err;
}

static int __exit stpti_tango_lite_remove(struct platform_device *pdev)
{
	ST_ErrorCode_t hal_err;

	pr_info("Removing %s device\n", pdev->name);

	/* Disable power management for this device */
	pm_runtime_put_sync(&pdev->dev);
	pm_runtime_disable(&pdev->dev);

	hal_err = stptiOBJMAN_DeallocateObject(stptiOBJMAN_pDeviceObjectHandle(0), TRUE);
	if (ST_NO_ERROR != hal_err) {
		pr_err("Failed to deallocate pdevice %d\n", 0);
		return -EINVAL;
	}

	/* Deallocate access Write Lock for this physical device */
	stptiOBJMAN_TermWriteLock(0);

	kfree(STPTI_Driver.ObjMan[0]);
	kfree(STPTI_Driver.HALFunctionPool[0]);

	STPTI_Driver.NumberOfpDevices--;
	return 0;
}

static int stpti_stfe_probe(struct platform_device *pdev)
{
	int err = 0;
	int i;
	ST_ErrorCode_t hal_err;
	struct stpti_stfe_config *stfe_config;
	struct sched_param param;

	pr_info("Probed %s device\n", pdev->name);

#ifdef CONFIG_OF
	if (pdev->dev.of_node) {
		err = stpti_dt_get_stfe_pdata(pdev);
		if (err)
			return err;
	}
#endif
	stfe_config = pdev->dev.platform_data;
	STPTI_Driver.FE_Power_Enabled = 1;

	if (stfe_config->firmware) {
		pr_info("Caching firmware %s\n", stfe_config->firmware);
		err = request_firmware(&STPTI_Driver.cachedSTFEFW, stfe_config->firmware, &pdev->dev);
		if (err) {
			pr_info("Could not cache TP firmware %s (%d)\n", stfe_config->firmware, err);
			return err;
		}
	}

	STPTI_Driver.TSHALFunctionPool = kzalloc(sizeof(TSHAL_API_t), GFP_KERNEL);
	if (!STPTI_Driver.TSHALFunctionPool) {
		err = -ENOMEM;
		goto error;
	}

	STPTI_Driver.TSHALFunctionPool[0].TSInput = stptiTSHAL_TSInputAPI;

	/* Initialise the TSHAL device */
	hal_err = stptiTSHAL_call(TSInput.TSHAL_TSInputMapHW, 0, pdev);
	if (ST_NO_ERROR != hal_err) {
		pr_err("Failure to initialise TSInput (0x%x)\n", hal_err);
		err = -ENODEV;
		goto error;
	}

	/* Enable runtime power management */

	/* Start 1 LiveMonitor task per TP device (STFE_V1 only)
	 * Note that this obviously requires that all TP devices are probed
	 * BEFORE the STFE device */
	for (i = 0; STFE_V1 == stfe_config->stfe_version && i < STPTI_Driver.NumberOfpDevices; i++) {
		FullHandle_t pDevice = stptiOBJMAN_pDeviceObjectHandle(i);
		stptiAPI_pDeviceAuxData_t *aux_data = stptiOBJMAN_GetObjectAuxiliaryData_p(pDevice);
		if (!aux_data) {
			pr_err("Failed to get aux data for TP %d\n", i);
			err = -ENODEV;
			goto error_post_pm;
		}

		aux_data->LiveTask_p = kthread_run((int (*)(void *))stptiAPI_LiveMonitorTask,
						   (void *)pDevice.word, "DMX-HalLive-%d", i);
		if (IS_ERR(aux_data->LiveTask_p)) {
			pr_err("Failed to create live monitor task for TP %d\n", i);
			err = -ENOMEM;
			aux_data->LiveTask_p = NULL;
			goto error_post_pm;
		}

		/* switch to real time scheduling (if requested) */
		if (thread_dmx_hallive[1]) {
			param.sched_priority = thread_dmx_hallive[1];
			if (0 != sched_setscheduler(aux_data->LiveTask_p, thread_dmx_hallive[0], &param)) {
				pr_err("FAILED to set scheduling parameters to priority %d Mode :(%d)\n", \
				thread_dmx_hallive[1], thread_dmx_hallive[0]);
			}
		}

	}
	/* if driver is power down by DT then
	 * resume the driver and suspend it to make sure
	 * hw if really off */
	/* Set Power enabled to false , i.e no more access
	 * to this hw */
	if (stfe_config->power_down) {
		stfe_resume(&pdev->dev);
		stfe_suspend(&pdev->dev);
		STPTI_Driver.FE_Power_Enabled = 0;
		return 0;
	}
	err = pm_runtime_set_active(&pdev->dev);
	if (err) {
		pr_err("pm_runtime_set_active error 0x%x\n", err);
		goto error_post_maphw;
	}
	pm_suspend_ignore_children(&pdev->dev, 1);
	pm_runtime_enable(&pdev->dev);

	return 0;

error_post_pm:
	pm_runtime_disable(&pdev->dev);

error_post_maphw:
	for (i = 0; i < STPTI_Driver.NumberOfpDevices; i++) {
		FullHandle_t pDevice = stptiOBJMAN_pDeviceObjectHandle(i);
		stptiAPI_pDeviceAuxData_t *aux_data = stptiOBJMAN_GetObjectAuxiliaryData_p(pDevice);
		if (aux_data && aux_data->LiveTask_p) {
			kthread_stop(aux_data->LiveTask_p);
			aux_data->LiveTask_p = NULL;
		}
	}

	stptiTSHAL_call(TSInput.TSHAL_TSInputUnMapHW, 0);

error:
	kfree(STPTI_Driver.TSHALFunctionPool);
	if (STPTI_Driver.cachedSTFEFW) {
		release_firmware(STPTI_Driver.cachedSTFEFW);
		STPTI_Driver.cachedSTFEFW = NULL;
	}
	return err;
}

static int __exit stpti_stfe_remove(struct platform_device *pdev)
{
	int err = 0;
	int i;
	ST_ErrorCode_t hal_err;
	struct stpti_stfe_config *stfe_config;
	stfe_config = pdev->dev.platform_data;

	/* Disable power management for this device */

	/* Stop 1 LiveMonitor task per TP device
	 * Note that this expects that the STFE device is removed BEFORE
	 * any TP devices*/
	for (i = 0; i < STPTI_Driver.NumberOfpDevices; i++) {
		FullHandle_t pDevice = stptiOBJMAN_pDeviceObjectHandle(i);
		stptiAPI_pDeviceAuxData_t *aux_data = stptiOBJMAN_GetObjectAuxiliaryData_p(pDevice);
		if (aux_data && aux_data->LiveTask_p) {
			kthread_stop(aux_data->LiveTask_p);
			aux_data->LiveTask_p = NULL;
		}
	}

	pr_info("Removing %s device\n", pdev->name);
	hal_err = stptiTSHAL_call(TSInput.TSHAL_TSInputUnMapHW, 0);
	if (ST_NO_ERROR != hal_err) {
		pr_err("Failure to remove TSInput (0x%x)\n", hal_err);
		err = -ENODEV;
	}

	kfree(STPTI_Driver.TSHALFunctionPool);
	if (STPTI_Driver.cachedSTFEFW) {
		release_firmware(STPTI_Driver.cachedSTFEFW);
		STPTI_Driver.cachedSTFEFW = NULL;
	}
	pm_runtime_disable(&pdev->dev);

#ifdef CONFIG_OF
	if (pdev->dev.of_node)
		stpti_dt_put_stfe_pdata(pdev);
#endif

	return err;
}

static int stfe_suspend(struct device *dev)
{
	ST_ErrorCode_t hal_err;
	int err = 0;
#ifdef CONFIG_ARCH_STI
	struct platform_device *pdev = to_platform_device(dev);
#endif

#ifdef CONFIG_PM_RUNTIME
	/* if dev->power.runtime_status=RPM_SUSPENDED then no need
	* to suspend again */
	if (pm_runtime_suspended(dev))
		return 0;
#endif
	hal_err = stptiTSHAL_call(TSInput.TSHAL_TSInputSetPowerState, 0, stptiTSHAL_TSINPUT_SLEEPING);
	if (ST_NO_ERROR != hal_err) {
		pr_err("Failed to suspend TSInput (e=%x)\n", hal_err);
		err = -EIO;
	}

	if (!err)
		pr_info("Suspend TSInput %d\n", 0);

#ifdef CONFIG_ARCH_STI
	if (!err) {
		/* Power-down TSInput device */
		hal_err = stptiTSHAL_call(TSInput.TSHAL_TSInputSetPowerState, 0, stptiTSHAL_TSINPUT_POWERDOWN);
		if (ST_NO_ERROR != hal_err) {
			pr_err("Failed to resume TSInput (e=%x)\n", hal_err);
			err = -EIO;
		} else {
			pr_info("Powered-down TSInput %d\n", pdev->id);
		}
	}
#endif

	return err;
}

static int stfe_resume(struct device *dev)
{
	ST_ErrorCode_t hal_err;
	int err = 0;

#ifdef CONFIG_PM_RUNTIME
	/* if dev->power.runtime_status=RPM_SUSPENDED then don't
	* resume */
	if (pm_runtime_suspended(dev))
		return 0;
#endif

#ifdef CONFIG_ARCH_STI
	/* Power-up TSInput device to sleeping state */
	hal_err = stptiTSHAL_call(TSInput.TSHAL_TSInputSetPowerState, 0, stptiTSHAL_TSINPUT_SLEEPING);
	if (ST_NO_ERROR != hal_err) {
		pr_err("Failed to resume TSInput (e=%x)\n", hal_err);
		err = -EIO;
	}

#endif
	if (!err) {
		pr_info("Resumed TSInput\n");
		hal_err = stptiTSHAL_call(TSInput.TSHAL_TSInputSetPowerState, 0, stptiTSHAL_TSINPUT_RUNNING);
		if (ST_NO_ERROR != hal_err) {
			pr_err("Failed to resume TSInput (e=%x)\n", hal_err);
			err = -EIO;
		}
	}
	return err;
}

static int stfe_idle(struct device *dev)
{
	pr_info("stfe idle \n");
	/* Nothing to do */
	return 0;
}
#ifndef CONFIG_ARCH_STI
static int stfe_freeze(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	int err = 0;
	ST_ErrorCode_t hal_err;

	/* Put TSInput device into suspend first */
	err = stfe_suspend(dev);

	if (!err) {
		/* Power-down TSInput device */
		hal_err = stptiTSHAL_call(TSInput.TSHAL_TSInputSetPowerState, 0, stptiTSHAL_TSINPUT_POWERDOWN);
		if (ST_NO_ERROR != hal_err) {
			pr_err("Failed to resume TSInput (e=%x)\n", hal_err);
			err = -EIO;
		} else {
			pr_info("Powered-down TSInput %d\n", pdev->id);
		}
	}

	return err;
}

static int stfe_restore(struct device *dev)
{
	int err = 0;
	ST_ErrorCode_t hal_err;

	/* Power-up TSInput device to sleeping state */
	hal_err = stptiTSHAL_call(TSInput.TSHAL_TSInputSetPowerState, 0, stptiTSHAL_TSINPUT_SLEEPING);
	if (ST_NO_ERROR != hal_err) {
		pr_err("Failed to resume TSInput (e=%x)\n", hal_err);
		err = -EIO;
	}

	/* Resume TSInput device */
	if (!err) {
		pr_info("Restored TSInput\n");
		err = stfe_resume(dev);
	}

	return 0;
}
#endif

static struct platform_driver stpti_tp_driver = {
	.driver = {
		   .name = "stm-tango-tp",
		   .owner = THIS_MODULE,
		   .pm = &(struct dev_pm_ops){
					      .suspend = tango_suspend,
					      .resume = tango_resume,
#ifndef CONFIG_ARCH_STI
					      .freeze = tango_freeze,
					      .thaw = tango_restore,
					      .restore = tango_restore,
#endif
					      .runtime_suspend = tango_suspend,
					      .runtime_resume = tango_resume,
					      .runtime_idle = tango_idle,
					      },
#if defined(CONFIG_OF)
		   .of_match_table = of_match_ptr(stpti_tp_match),
#endif
		   },
	.probe = stpti_tp_probe,
	.remove = stpti_tp_remove,
};

static struct platform_driver stpti_tango_lite_driver = {
	.driver = {
		   .name = "stm-tango-dummy",
		   .owner = THIS_MODULE,
		   .pm = &(struct dev_pm_ops){
					      /* Since the power callbacks use the stptiHAL for
					       * abstraction we can use the same power callbacks for
					       * the tango lite as the tango */
					      .suspend = tango_suspend,
					      .resume = tango_resume,
#ifndef CONFIG_ARCH_STI
					      .freeze = tango_freeze,
					      .thaw = tango_restore,
					      .restore = tango_restore,
#endif
					      .runtime_suspend = tango_suspend,
					      .runtime_resume = tango_resume,
					      .runtime_idle = tango_idle,
					      },
#if defined(CONFIG_OF)
		   .of_match_table = of_match_ptr(stpti_tango_lite_match),
#endif
		   },
	.probe = stpti_tango_lite_probe,
	.remove = stpti_tango_lite_remove,
};

static struct platform_driver stpti_stfe_driver = {
	.driver = {
		   .name = "stm-tango-stfe",
		   .owner = THIS_MODULE,
		   .pm = &(struct dev_pm_ops){
					      .suspend = stfe_suspend,
					      .resume = stfe_resume,
#ifndef CONFIG_ARCH_STI
					      .freeze = stfe_freeze,
					      .thaw = stfe_restore,
					      .restore = stfe_restore,
#endif
					      .runtime_suspend = stfe_suspend,
					      .runtime_resume = stfe_resume,
					      .runtime_idle = stfe_idle,
					      },
#if defined(CONFIG_OF)
		   .of_match_table = of_match_ptr(stpti_stfe_match),
#endif
		   },
	.probe = stpti_stfe_probe,
	.remove = stpti_stfe_remove,
};

int stptiAPI_register_drivers(void)
{
	int err = platform_driver_register(&stpti_tp_driver);
	if (!err)
		err = platform_driver_register(&stpti_tango_lite_driver);
	if (!err)
		err = platform_driver_register(&stpti_stfe_driver);
	return err;
}

void stptiAPI_unregister_drivers(void)
{
	platform_driver_unregister(&stpti_stfe_driver);
	platform_driver_unregister(&stpti_tango_lite_driver);
	platform_driver_unregister(&stpti_tp_driver);
}
