/************************************************************************
Copyright (C) 2003-2014 STMicroelectronics. All Rights Reserved.

This file is part of the Streaming Engine.

Streaming Engine is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

Streaming Engine is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with Streaming Engine; see the file COPYING.  If not, write to the Free
Software Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307,
USA.

The Streaming Engine Library may alternatively be licensed under a
proprietary license from ST.
************************************************************************/

/************************************************************************
MME Host transformer mapping to Hades hardware
Hades is the HEVC decoder

Restrictions: This driver supports only one hades device
************************************************************************/

#include <linux/module.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/platform_device.h>
#include <linux/init.h>    /* Initialisation support */
#include <linux/kernel.h>  /* Kernel support */
#include <linux/err.h>
#include <linux/suspend.h>
#include <linux/pm_runtime.h>
#include <linux/pm.h>
#include <linux/firmware.h> /*for request_firmware() */
#include <linux/of.h>
#include <linux/clk.h>

#include <mme.h>

// upstream interface
#include <hevc_video_transformer_types.h>
// downstream interface
#include "hades_api.h"
#include "hades_cmd_types.h"

#include "osdev_mem.h"
#include "osdev_cache.h"

#include "st_relayfs_se.h"

#include "hevc_hard_host_transformer.h"
#include "hades_memory_map.h"

#include "checker.h"
#include "crcdriver.h"
#include "picture_dumper.h"
#include "memtest.h"

#define MODULE_NAME "Hades HEVC host transformer"
#define DEVICE_NAME "hades"
#define CRC_DEVICE_NAME "hades_crc"

MODULE_DESCRIPTION("Hades HEVC decode hardware cell platform driver");
MODULE_AUTHOR("STMicroelectronics");
MODULE_LICENSE("GPL");

// defined in MME API, must match name in header of Hades firmware file
static char *mmeName  = HEVC_MME_TRANSFORMER_NAME "0";
module_param(mmeName , charp, S_IRUGO);
MODULE_PARM_DESC(mmeName, "Name to use for MME Transformer 0 registration");
static char *mmeName_extra  = HEVC_MME_TRANSFORMER_NAME "1";
module_param(mmeName_extra , charp, S_IRUGO);
MODULE_PARM_DESC(mmeName_extra, "Name to use for MME Transformer 1 registration");

static char *partition  = "hades-l3"; // must match a BPA2 partition in bootargs in SoC memory map or tlm_run_* (VSOC)
module_param(partition, charp, S_IRUGO);
MODULE_PARM_DESC(partition, "BPA2 partition for shared memory with Hades device");

#ifdef HEVC_HADES_CUT1
static char *firmwareName = "hades_fw_cut1.bin";
#else
static char *firmwareName = "hades_fw.bin";
#endif
module_param(firmwareName, charp, S_IRUGO);
MODULE_PARM_DESC(firmwareName, "Path to HADES firmware file");

static unsigned int softcrc = 0;
module_param(softcrc, uint, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(softcrc, "Set to 1 to use software CRCs instead of hardware ones");

static unsigned int outputdecodingtime = 0;
module_param(outputdecodingtime, uint, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(outputdecodingtime, "Set to 1 to output decoding time for each frame via strelay");

// memtest parameter:
//    n >= 0 : skip first n watchpoints of each image
//    n == -1: rotate thru watchpoints
//    n <= -2: do not check the memory
static int memtest = -3;
module_param(memtest, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(memtest, "Set to 0 to activate memory checking");

static int HadesPmRuntimeSuspend(struct device *dev);
static int HadesPmRuntimeResume(struct device *dev);
static int HadesPmSuspend(struct device *dev);
static int HadesPmResume(struct device *dev);
static int HadesPmFreeze(struct device *dev);
static int HadesPmRestore(struct device *dev);
static struct dev_pm_ops HadesPmOps = {
	.suspend = HadesPmSuspend,
	.resume  = HadesPmResume,
	.freeze  = HadesPmFreeze,
	.restore = HadesPmRestore,
	.runtime_suspend  = HadesPmRuntimeSuspend,
	.runtime_resume   = HadesPmRuntimeResume,
};

static int HadesProbe(struct platform_device *pdev);
static int HadesRemove(struct platform_device *pdev);
#ifdef CONFIG_OF
static struct of_device_id stm_hades_match[] = {
	{
		.compatible = "st,se-hades",
	},
	{},
};
//MODULE_DEVICE_TABLE(of, stm_hades_match);
#endif
static struct platform_driver HadesDriver = {
	.driver = {
		.name = DEVICE_NAME,
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(stm_hades_match),
		.pm = &HadesPmOps,
	},
	.probe = HadesProbe,
	.remove = HadesRemove,
};

module_platform_driver(HadesDriver);

typedef struct {
	HadesHandle_t                   hadesHandle;     // to communicate with the HADES hardware
	unsigned int                    instanceId;      // gives minor number of CRC device
	CheckerHandle_t                 checkerHandle;   // CRC checker instance
#ifdef HEVC_HADES_CUT1
	HostAddress                     hadesCmdAndStatusAddr;  // transform host address
	hevcdecpix_transform_param_t   *hades_cmd;       // copy of the current command, virtual address
	hevcdecpix_status_t            *hades_status;    //  status returned by Hades, virtual address
#else
	HostAddress                     hadesCmdAndStatusAddr;  // transform host address
	hades_cmd_t                    *hades_cmd;       // copy of the current command, virtual address
	hades_status_t                 *hades_status;    //  status returned by Hades, virtual address
#endif
} HevcTransformerContext_t;

struct HevcDeviceContext_t {
	struct device     *iDev;
	HadesInitParams_t  hades_params;
	fw_info_t         fw_info;            // Used in full static FW loading mode
	struct clk        *clk_hwpe;
	struct clk        *clk_fc;
};

static HevcTransformerContext_t *AllocateContext(void);
static void FreeContext(HevcTransformerContext_t *context);
static int HevcClockSetRate(struct device_node *of_node);

typedef struct { // Copied from HEVC Codec class
	// Parser info
	uint32_t DecodeIndex;
	uint32_t IDR;
	uint32_t poc;
	// Buffer cached addresses
	uint8_t *omega_luma;
	uint8_t *omega_chroma;
	uint32_t omega_luma_size;
	uint8_t *ppb;
	uint32_t ppb_size;
	uint8_t *raster_luma;
	uint8_t *raster_chroma;
	uint32_t raster_stride;
	uint32_t raster_width;
	uint32_t raster_height;
	uint8_t *raster_decimated_luma;
	uint8_t *raster_decimated_chroma;
	uint32_t raster_decimated_stride;
	uint32_t raster_decimated_width;
	uint32_t raster_decimated_height;
} HevcCodecExtraCRCInfo_t;

static HadesError_t copyTransform(hevcdecpix_transform_param_t *frameParamsOut,
                                  hevcdecpix_transform_param_t *frameParamsIn);
static void         processCRC(MME_Command_t *cmd, CheckerHandle_t checkerHandle);
static void         dumpPicture(MME_Command_t *cmd);
static void         doMemTest(MME_Command_t *cmd, HostAddress *payload, HostAddress *commandAndStatusAddr);
static void         stopMemTest(void);
static int          LoadHadesDevice(void);
static int          UnloadHadesDevice(void);
static int          RegisterHadesTransformer(void);
static int          DeRegisterHadesTransformer(void);
static int          stm_pm_hades_notifier_call(struct notifier_block *this, unsigned long event, void *ptr);
static int          HadesPowerOn(void);
static void         HadesPowerOff(void);
static int          HadesDynamicPmOn(void);
static void         HadesDynamicPmOff(void);

// Global variables
static int   HevcInstanceNumber = 0;   // counts the number of initialized transformers for Power management
static char *firmwareContents = NULL;  // firmware file copied into kernel memory
static int   firmwareSize = 0;
static HadesHandle_t HadesHandle = NULL;
static struct HevcDeviceContext_t *HevcDeviceContext;

extern unsigned int decode_time;

// PM notifier callback
// At entry of low power, multicom is terminated in .freeze callback. When system exit from
// low power, multicom is initialized through pm_notifier, so we have to register Hades transformer
// again. After Hades pm_notifier, SE module notifier would be called in which other multicom apis
// are called.
// This callback is used to register the Hades transformer after low power exit
#define ENTRY(enum) case enum: return #enum
inline const char *StringifyPmEvent(int aPmEvent)
{
	switch (aPmEvent) {
		ENTRY(PM_HIBERNATION_PREPARE);
		ENTRY(PM_POST_HIBERNATION);
		ENTRY(PM_SUSPEND_PREPARE);
		ENTRY(PM_POST_SUSPEND);
		ENTRY(PM_RESTORE_PREPARE);
		ENTRY(PM_POST_RESTORE);
	default: return "<unknown pm event>";
	}
}

static int stm_pm_hades_notifier_call(struct notifier_block *this, unsigned long event, void *ptr)
{
	int ErrorCode;

	switch (event) {
	case PM_POST_SUSPEND:
	//fallthrough
	case PM_POST_HIBERNATION:
		pr_info("Hades notifier: %s\n", StringifyPmEvent(event));

		ErrorCode = RegisterHadesTransformer();
		if (ErrorCode != 0) {
			//Error returned by Hades, but this should not prevent the system exiting low power
			pr_err("Error: %s RegisterHadesTransformer failed (%d)\n", __func__, ErrorCode);
		}
		break;

	default:
		break;
	}

	return NOTIFY_DONE;
}

static int HevcClockSetRate(struct device_node *of_node)
{
	int ret = 0;
	unsigned int max_freq = 0;
	if (!of_property_read_u32_index(of_node, "clock-frequency", 0, &max_freq)) {
		if (max_freq) {
			ret = clk_set_rate(HevcDeviceContext->clk_hwpe, max_freq);
			if (ret) {
				pr_err("Error: Failed to set max frequency for clk_hwpe_hades clock (%d)\n", ret);
				return -EINVAL;
			}
		}
	}
	pr_info("For CLK_HWPE_HADES ret value : 0x%x max frequency value : %u\n", (unsigned int)ret, max_freq);

	max_freq = 0;
	if (!of_property_read_u32_index(of_node, "clock-frequency", 1, &max_freq)) {
		if (max_freq) {
			ret = clk_set_rate(HevcDeviceContext->clk_fc, max_freq);
			if (ret) {
				pr_err("Error: Failed to set max frequency for clk_fc clock (%d)\n", ret);
				return -EINVAL;
			}
		}
	}
	pr_info("For CLK_FC_HADES ret value : 0x%x max frequency value : %u\n", (unsigned int)ret, max_freq);
	return ret;
}

static int HadesPowerOn(void)
{
	int ret = 0;
#ifdef CONFIG_PM_RUNTIME
	if (pm_runtime_get_sync(HevcDeviceContext->iDev) < 0) {
		ret = -EINVAL;
	}
#endif
	return ret;
}

static void HadesPowerOff(void)
{
#ifdef CONFIG_PM_RUNTIME
	pm_runtime_mark_last_busy(HevcDeviceContext->iDev);
	pm_runtime_put(HevcDeviceContext->iDev);
#endif
}

static int HadesDynamicPmOn()
{
	int ret;
	ret = HadesPowerOn();
	if (ret) {
		pr_err("Error: %s - failed to power on\n", __func__);
		return ret;
	}

#ifdef CONFIG_ARCH_STI
	// Enable clock (Only clk_hwpe is managed from host!)
	ret = clk_enable(HevcDeviceContext->clk_hwpe);
	if (ret) {
		pr_err("Error: %s - failed to clk on\n", __func__);
		HadesPowerOff();
	}
#endif

	return ret;
}

static void HadesDynamicPmOff()
{
#ifdef CONFIG_ARCH_STI
	// Disable clock (Only clk_hwpe is managed from host!)
	clk_disable(HevcDeviceContext->clk_hwpe);
#endif
	HadesPowerOff();
}

//
//
// Multicom Host Transformer functions
//
//

static HevcTransformerContext_t *AllocateContext(void)
{
	HevcTransformerContext_t *context;

	context = OSDEV_Malloc(sizeof(*context));
	if (context == NULL) {
		pr_err("Error: %s context allocation failure\n", __func__);
		return NULL;
	}
	memset(context, 0, sizeof(HevcTransformerContext_t));

	if (HADES_Malloc(&context->hadesCmdAndStatusAddr,
	                 sizeof(*context->hades_cmd) + sizeof(*context->hades_status)) == HADES_NO_ERROR) {
#ifdef HEVC_HADES_CUT1
		context->hades_cmd = (hevcdecpix_transform_param_t *) context->hadesCmdAndStatusAddr.virtual;
		context->hades_status = (hevcdecpix_status_t *)(context->hadesCmdAndStatusAddr.virtual + sizeof(*context->hades_cmd));
#else
		context->hades_cmd = (hades_cmd_t *) context->hadesCmdAndStatusAddr.virtual;
		context->hades_status = (hades_status_t *)(context->hadesCmdAndStatusAddr.virtual + sizeof(*context->hades_cmd));
#endif

	} else {
		FreeContext(context);
		return NULL;
	}
	return context;
}

static void FreeContext(HevcTransformerContext_t *context)
{
	if (context != NULL) {
		if (context->hades_cmd != NULL) {
			HADES_Free(&context->hadesCmdAndStatusAddr);
		}
		OSDEV_Free(context);
	}
}

MME_ERROR initTransformer(MME_UINT paramsSize, MME_GenericParams_t params, void **context)
{
	Hevc_InitTransformerParam_fmw_t *InitTransformerParam = (Hevc_InitTransformerParam_fmw_t *)params;
	HevcTransformerContext_t        *HevcContext;

	HEVC_VERBOSE(">>> initTransformer\n");

	if (context == NULL) {
		return MME_INVALID_HANDLE;
	}

	/* HadesHandle should not be NULL when we enter initTransformer */
	if (HadesHandle == NULL) {
		pr_err("Error: %s hades handle null\n", __func__);
		return MME_INTERNAL_ERROR;
	}

	// Allocate a new context
	HevcContext = AllocateContext();
	if (HevcContext == NULL) {
		pr_err("Error: %s - Unable to allocate Hevc Context in %s\n", __func__, MODULE_NAME);
		return MME_NOMEM;
	}

	HevcContext->hadesHandle = HadesHandle;
	HevcContext->instanceId = InitTransformerParam->InstanceId;
	CRCDriver_GetChecker(HevcContext->instanceId,
	                     &HevcContext->checkerHandle); // the checker must be open before the stream starts to play
	pr_info("%s: CRC check started with handle %p\n", mmeName, (void *)(HevcContext->checkerHandle));
	*context = (void *)HevcContext;
	++ HevcInstanceNumber;
	pr_info("%s: transformer init'ed\n", mmeName);

	return MME_SUCCESS;
}


MME_ERROR processCommand(void *context, MME_Command_t *cmd)
{
	HadesError_t Status = HADES_NO_ERROR;
	HevcTransformerContext_t *HevcContext = (HevcTransformerContext_t *)context;
	HostAddress HadesAttrAddr;
	HadesAttr_t *HadesAttr;
	int error_code = 0;
	MME_ERROR mme_status = MME_SUCCESS;
	FrameCRC_t ComputedCrc;

	switch (cmd->CmdCode) {
	case MME_TRANSFORM:
#ifdef HEVC_HADES_CUT1
		Status = copyTransform(HevcContext->hades_cmd, (hevcdecpix_transform_param_t *)(cmd->Param_p));
#else
		HevcContext->hades_cmd->command_type = HEVC;
		Status = copyTransform(&HevcContext->hades_cmd->command.hevc_mme_cmd, (hevcdecpix_transform_param_t *)(cmd->Param_p));
#endif
		if (Status != HADES_NO_ERROR) {
			pr_err("Error: %s failed to prepare transform parameters\n", mmeName);
			return MME_INTERNAL_ERROR;
		}
		HADES_Flush(&HevcContext->hadesCmdAndStatusAddr);

		if (HADES_Malloc(&HadesAttrAddr, sizeof(*HadesAttr)) == HADES_NO_ERROR) {
			HadesAttr = (HadesAttr_t *) HadesAttrAddr.virtual;
		} else {
			return MME_NOMEM;
		}

		HadesAttr->hades_cmd_struct_p    = (uintptr_t) HADES_HostToFabricAddress(&HevcContext->hadesCmdAndStatusAddr);
		HadesAttr->hades_status_struct_p = HadesAttr->hades_cmd_struct_p + sizeof(*HevcContext->hades_cmd);

		// Reset decode return status.
		// Ihis is necesaary as in case of firmware/hang crash, this
		// information remains unchanged so codec class might get stale
		// information of previous correct decode.
		memset(HevcContext->hades_status, 0x0, sizeof(*HevcContext->hades_status));
#ifdef HEVC_HADES_CUT1
		HevcContext->hades_status->error_code = HEVC_DECODER_ERROR_NOT_RECOVERED;
#else
		HevcContext->hades_status->command_type = HEVC;
		HevcContext->hades_status->status.hevc_mme_status.error_code = HEVC_DECODER_ERROR_NOT_RECOVERED;
#endif
		HADES_Flush(&HadesAttrAddr);

		if (memtest >= -1) {
			doMemTest(cmd, &HadesAttrAddr, &HevcContext->hadesCmdAndStatusAddr);
		}
		if (HadesDynamicPmOn()) {
			pr_err("Error: %s - unable to dynamic power on\n", __func__);
			HADES_Free(&HadesAttrAddr);
			return MME_INTERNAL_ERROR;
		}

		Status = HADES_ProcessCommand(HevcContext->hadesHandle, HADES_HostToFabricAddress(&HadesAttrAddr)); // blocking call

		HADES_GetHWPE_Debug(&ComputedCrc); // Get Hades CRC
		if (ComputedCrc.os_r2b_tx == 0xFFFFFFFFU) {
			pr_err("Error: %s HEVC Firmware Crash. Resetting board only option now to decode HEVC properly.\n",
			       __func__);
			mme_status = MME_COMMAND_TIMEOUT;
			Status = HADES_ERROR;
		}

		if (Status != HADES_NO_ERROR) {
			pr_err("Error: %s HADES_ProcessCommand failed:%d\n", __func__, Status);
			switch (Status) {
			case HADES_ERROR:
				if (mme_status != MME_COMMAND_TIMEOUT) {
					mme_status = MME_INTERNAL_ERROR;
				}
				break;
			case HADES_INVALID_HANDLE:
				mme_status = MME_INVALID_HANDLE;
				break;
			case HADES_NO_MEMORY:
				mme_status = MME_NOMEM;
				break;
			default:
				mme_status = MME_INTERNAL_ERROR;
				break;
			}
		}

		if (HevcContext->checkerHandle != NULL) {
			processCRC(cmd, HevcContext->checkerHandle);
		}

		HadesDynamicPmOff();
		HADES_Invalidate(&HevcContext->hadesCmdAndStatusAddr);
#ifdef HEVC_HADES_CUT1
		memcpy(cmd->CmdStatus.AdditionalInfo_p, HevcContext->hades_status, sizeof(*HevcContext->hades_status));
#else
		memcpy(cmd->CmdStatus.AdditionalInfo_p, &HevcContext->hades_status->status.hevc_mme_status,
		       sizeof(HevcContext->hades_status->status.hevc_mme_status));
#endif

		if (outputdecodingtime) {
			HevcCodecExtraCRCInfo_t *info = (HevcCodecExtraCRCInfo_t *)(cmd->DataBuffers_p[0]->ScatterPages_p[0].Page_p);
			st_relayfs_print_se(ST_RELAY_TYPE_HEVC_HW_DECODING_TIME, ST_RELAY_SOURCE_SE,
			                    "decoder,%d,%d,%d,%d\n",
			                    0,
			                    info->DecodeIndex,
			                    decode_time,
			                    ((hevcdecpix_status_t *)(cmd->CmdStatus.AdditionalInfo_p))->error_code);
			pr_info("decoding_time,decoder,%d,%d,%d,%d\n",
			        0,
			        info->DecodeIndex,
			        decode_time,
			        ((hevcdecpix_status_t *)(cmd->CmdStatus.AdditionalInfo_p))->error_code);
		}
		if (memtest >= -1) {
			stopMemTest();
		}

		// For debug; dump some or all pictures
		if (0) {
			HevcCodecExtraCRCInfo_t *info = (HevcCodecExtraCRCInfo_t *)(cmd->DataBuffers_p[0]->ScatterPages_p[0].Page_p);
			if (info->DecodeIndex >= 459 && info->DecodeIndex <= 459) {
				dumpPicture(cmd);
			}
		}

		HADES_Invalidate(&HevcContext->hadesCmdAndStatusAddr);
#ifdef HEVC_HADES_CUT1
		memcpy(cmd->CmdStatus.AdditionalInfo_p, HevcContext->hades_status, sizeof(*HevcContext->hades_status));
#else
		memcpy(cmd->CmdStatus.AdditionalInfo_p, &HevcContext->hades_status->status.hevc_mme_status,
		       sizeof(HevcContext->hades_status->status.hevc_mme_status));
#endif
		error_code = ((hevcdecpix_status_t *)(cmd->CmdStatus.AdditionalInfo_p))->error_code;
		if (error_code != 0) {
			pr_err("Error: %s processCommand: HEVC decode status is %d\n", __func__, error_code);
		}
		HADES_Free(&HadesAttrAddr);
		break;

	case MME_SET_GLOBAL_TRANSFORM_PARAMS:
		pr_err("Error: %s SET_GLOBAL_TRANSFORM_PARAMS not supported %s\n", __func__, mmeName);
		return MME_NOT_IMPLEMENTED;

	case MME_SEND_BUFFERS:
		return MME_NOT_IMPLEMENTED;

	default:
		return MME_NOT_IMPLEMENTED;
	}

	return mme_status;
}

MME_ERROR abortCmd(void *context, MME_CommandId_t cmdId)
{
	return MME_NOT_IMPLEMENTED;
}

MME_ERROR getTransformerCapability(MME_TransformerCapability_t *capability)
{
	pr_err("Error: %s %s getTransformerCapability not yet supported\n", __func__, mmeName);
	return MME_NOT_IMPLEMENTED;
}

MME_ERROR termTransformer(void *context)
{
	HevcTransformerContext_t *HevcContext = (HevcTransformerContext_t *)context;

	HEVC_VERBOSE(">>> termTransformer\n");

	if (HevcContext == NULL) {
		pr_err("Error: %s - HevcContext is Null, returning without terminating transformer in %s\n", __func__, mmeName);
		return MME_INVALID_HANDLE;
	}

	-- HevcInstanceNumber;
	if (HevcContext->checkerHandle != NULL) {
		CRCDriver_ReleaseChecker(HevcContext->instanceId);
		CHK_ShutdownComputed(HevcContext->checkerHandle);
	}

	FreeContext(HevcContext);

	HEVC_VERBOSE(">>> transformer has been terminated\n");

	return MME_SUCCESS;
}

static HadesError_t copyTransform(hevcdecpix_transform_param_t  *frameParamsOut,
                                  hevcdecpix_transform_param_t *frameParamsIn)
{
	uintptr_t fabric_addr_p;
	int       referenceIndex;
	int       listIndex;
	bool      converted[HEVC_MAX_REFERENCE_PICTURES];

	memcpy(frameParamsOut, frameParamsIn, sizeof(*frameParamsOut));

#define TRANSLATE(ADDR) 								\
    if (frameParamsOut->ADDR != 0)							\
    {											\
	    fabric_addr_p = HADES_PhysicalToFabricAddress((void*)frameParamsOut->ADDR);	\
	    if (fabric_addr_p == 0) 							\
	    {										\
		   pr_err( "Error: %s - HADES API: unable to translate " #ADDR " = %p into fabric address\n",	\
				__func__, (void*)(frameParamsOut->ADDR));						\
		   return HADES_ERROR;									\
	    }										\
	    frameParamsOut->ADDR = (uint32_t)fabric_addr_p;				\
    }											\
 
	// Adresses must be translated into the HADES memory space

	// Intermediate buffer
	TRANSLATE(intermediate_buffer.slice_table_base_addr)
	TRANSLATE(intermediate_buffer.ctb_table_base_addr)
	TRANSLATE(intermediate_buffer.slice_headers_base_addr)
	TRANSLATE(intermediate_buffer.ctb_commands.base_addr)
	TRANSLATE(intermediate_buffer.ctb_residuals.base_addr)

	// Reference pictures
	for (referenceIndex = 0; referenceIndex < HEVC_MAX_REFERENCE_PICTURES; referenceIndex++) {
		converted[referenceIndex] = false;
	}
	for (listIndex = 0; listIndex < frameParamsOut->num_reference_pictures; listIndex++) {
		referenceIndex = frameParamsOut->initial_ref_pic_list_l0[listIndex];
		if (! converted[referenceIndex]) {
			TRANSLATE(ref_picture_buffer[referenceIndex].ppb_offset)
			TRANSLATE(ref_picture_buffer[referenceIndex].pixels.luma_offset)
			TRANSLATE(ref_picture_buffer[referenceIndex].pixels.chroma_offset)
			converted[referenceIndex] = true;
		}
		referenceIndex = frameParamsOut->initial_ref_pic_list_l1[listIndex];
		if (! converted[referenceIndex]) {
			TRANSLATE(ref_picture_buffer[referenceIndex].ppb_offset)
			TRANSLATE(ref_picture_buffer[referenceIndex].pixels.luma_offset)
			TRANSLATE(ref_picture_buffer[referenceIndex].pixels.chroma_offset)
			converted[referenceIndex] = true;
		}
	}


	// JLX: patch for PPB: the Hades model needs a valid non-null PPB pointer even when nothing is written inside
	if (frameParamsOut->curr_picture_buffer.ppb_offset == 0) { // non-ref picture
		frameParamsOut->curr_picture_buffer.ppb_offset = frameParamsOut->curr_display_buffer.pixels.luma_offset;
	}

	// Current picture for future reference
	TRANSLATE(curr_picture_buffer.ppb_offset)
	TRANSLATE(curr_picture_buffer.pixels.luma_offset)
	TRANSLATE(curr_picture_buffer.pixels.chroma_offset)

	// Display pictures
	TRANSLATE(curr_display_buffer.pixels.luma_offset)
	TRANSLATE(curr_display_buffer.pixels.chroma_offset)
	TRANSLATE(curr_resize_buffer.pixels.luma_offset)
	TRANSLATE(curr_resize_buffer.pixels.chroma_offset)

	return HADES_NO_ERROR;
}

static void processCRC(MME_Command_t *cmd, CheckerHandle_t checkerHandle)
{
	FrameCRC_t computed_crc;
	HevcCodecExtraCRCInfo_t *info = (HevcCodecExtraCRCInfo_t *)(cmd->DataBuffers_p[0]->ScatterPages_p[0].Page_p);
	hevcdecpix_transform_param_t *transform_param = (hevcdecpix_transform_param_t *) cmd->Param_p;

	computed_crc.decode_index = info->DecodeIndex;
	computed_crc.idr          = info->IDR;
	computed_crc.poc          = info->poc;
	computed_crc.mask = (1llu << CRC_BIT_DECODE_INDEX) | (1llu << CRC_BIT_IDR) | (1llu << CRC_BIT_POC);

	HADES_GetHWPE_Debug(&computed_crc); // hardware registers

	if (softcrc) {
		if (info->omega_luma != NULL && info->omega_chroma != NULL) {
			OSDEV_InvCacheRange(info->omega_luma,
			                    (phys_addr_t) transform_param->curr_picture_buffer.pixels.luma_offset,
			                    info->omega_luma_size);
			computed_crc.omega_luma    = crc_1d(info->omega_luma, info->omega_luma_size);
			OSDEV_InvCacheRange(info->omega_chroma,
			                    (phys_addr_t) transform_param->curr_picture_buffer.pixels.chroma_offset,
			                    info->omega_luma_size / 2);
			computed_crc.omega_chroma  = crc_1d(info->omega_chroma, info->omega_luma_size / 2);
			computed_crc.mask |= (1llu << CRC_BIT_OMEGA_LUMA) | (1llu << CRC_BIT_OMEGA_CHROMA);
		}
		OSDEV_InvCacheRange(info->raster_luma,
		                    (phys_addr_t) transform_param->curr_display_buffer.pixels.luma_offset,
		                    info->raster_stride * info->raster_height);
		computed_crc.raster_luma   = crc_2d(info->raster_luma,   info->raster_stride, info->raster_width, info->raster_height);
		OSDEV_InvCacheRange(info->raster_chroma,
		                    (phys_addr_t) transform_param->curr_display_buffer.pixels.chroma_offset,
		                    info->raster_stride * info->raster_height / 2);
		computed_crc.raster_chroma = crc_2d(info->raster_chroma, info->raster_stride, info->raster_width,
		                                    info->raster_height / 2);
		computed_crc.mask |= (1llu << CRC_BIT_RASTER_LUMA) | (1llu << CRC_BIT_RASTER_CHROMA);
		if (info->raster_decimated_luma != NULL && info->raster_decimated_chroma != NULL) {
			OSDEV_InvCacheRange(info->raster_decimated_luma,
			                    (phys_addr_t) transform_param->curr_resize_buffer.pixels.luma_offset,
			                    info->raster_decimated_stride * info->raster_decimated_height);
			computed_crc.raster_decimated_luma   = crc_2d(info->raster_decimated_luma, info->raster_decimated_stride,
			                                              info->raster_decimated_width, info->raster_decimated_height);
			OSDEV_InvCacheRange(info->raster_decimated_chroma,
			                    (phys_addr_t) transform_param->curr_resize_buffer.pixels.chroma_offset,
			                    info->raster_decimated_stride * info->raster_decimated_height / 2);
			computed_crc.raster_decimated_chroma = crc_2d(info->raster_decimated_chroma, info->raster_decimated_stride,
			                                              info->raster_decimated_width, info->raster_decimated_height / 2);
			computed_crc.mask |= (1llu << CRC_BIT_RASTER_DECIMATED_LUMA) | (1llu << CRC_BIT_RASTER_DECIMATED_CHROMA);
		}
	}

	CHK_CheckDecoderCRC(checkerHandle, &computed_crc);
}

static void dumpPicture(MME_Command_t *cmd)
{
	HevcCodecExtraCRCInfo_t *info = (HevcCodecExtraCRCInfo_t *)(cmd->DataBuffers_p[0]->ScatterPages_p[0].Page_p);
	hevcdecpix_transform_param_t *transform_param = (hevcdecpix_transform_param_t *) cmd->Param_p;

	if (info->omega_luma != NULL && info->omega_chroma != NULL) {
		OSDEV_InvCacheRange(info->omega_luma,
		                    (phys_addr_t) transform_param->curr_picture_buffer.pixels.luma_offset,
		                    info->omega_luma_size);
		OSDEV_InvCacheRange(info->omega_chroma,
		                    (phys_addr_t) transform_param->curr_picture_buffer.pixels.chroma_offset,
		                    info->omega_luma_size / 2);
		O4_dump("/mnt/host/prj/SHARK_Validation/users/lachauxj/VSOC/Cannes/Pictures", info->DecodeIndex,
		        info->omega_luma, info->omega_chroma, info->omega_luma_size, transform_param->pic_width_in_luma_samples,
		        transform_param->pic_height_in_luma_samples);
	}

	OSDEV_InvCacheRange(info->raster_luma,
	                    (phys_addr_t) transform_param->curr_display_buffer.pixels.luma_offset,
	                    info->raster_stride * info->raster_height);
	OSDEV_InvCacheRange(info->raster_chroma,
	                    (phys_addr_t) transform_param->curr_display_buffer.pixels.chroma_offset,
	                    info->raster_stride * info->raster_height / 2);
	R2B_dump("/mnt/host/prj/SHARK_Validation/users/lachauxj/VSOC/Cannes/Pictures", 0, info->DecodeIndex,
	         info->raster_luma, info->raster_chroma, info->raster_width, info->raster_height, info->raster_stride);

	if (info->raster_decimated_luma != NULL && info->raster_decimated_chroma != NULL) {
		OSDEV_InvCacheRange(info->raster_decimated_luma,
		                    (phys_addr_t) transform_param->curr_resize_buffer.pixels.luma_offset,
		                    info->raster_decimated_stride * info->raster_decimated_height);
		OSDEV_InvCacheRange(info->raster_decimated_chroma,
		                    (phys_addr_t) transform_param->curr_resize_buffer.pixels.chroma_offset,
		                    info->raster_decimated_stride * info->raster_decimated_height / 2);
		R2B_dump("/mnt/host/prj/SHARK_Validation/users/lachauxj/VSOC/Cannes/Pictures", 1, info->DecodeIndex,
		         info->raster_decimated_luma, info->raster_decimated_chroma, info->raster_decimated_width, info->raster_decimated_height,
		         info->raster_decimated_stride);
	}
}

static void doMemTest(MME_Command_t *cmd, HostAddress *payload, HostAddress *commandAndStatusAddr)
{
	static unsigned int skip = 0;

	hevcdecpix_transform_param_t *transform = (hevcdecpix_transform_param_t *) cmd->Param_p;
	HevcCodecExtraCRCInfo_t *info = (HevcCodecExtraCRCInfo_t *)(cmd->DataBuffers_p[0]->ScatterPages_p[0].Page_p);

	MT_AddressRange_t ranges[MT_MAX_RANGES];
	int range = 0;
	int ref;

	uint32_t ppb_size = info->ppb_size;
	uint32_t omega_luma_size   = info->omega_luma_size;
	uint32_t raster_luma_size  = info->raster_stride * info->raster_height;
	uint32_t decimated_luma_size  = info->raster_decimated_stride * info->raster_decimated_height;

	MT_OpenMonitoring("CTBE", info->DecodeIndex);

#define ADD(NAME, START, LENGTH)		\
	{					\
		ranges[range].name = NAME;	\
		ranges[range].start = START;	\
		ranges[range].length = LENGTH;	\
		++range;			\
	}

	/////////////////////////////////////////////////
	//                  HWPE                       //
	/////////////////////////////////////////////////

	// Intermediate buffer
	range = 0;
	ADD("Slice Table", transform->intermediate_buffer.slice_table_base_addr,
	    transform->intermediate_buffer.slice_table_length);
	ADD("CTB table", transform->intermediate_buffer.ctb_table_base_addr, transform->intermediate_buffer.ctb_table_length);
	ADD("Slice headers", transform->intermediate_buffer.slice_headers_base_addr,
	    transform->intermediate_buffer.slice_headers_length);
	ADD("CTB commands", transform->intermediate_buffer.ctb_commands.base_addr,
	    transform->intermediate_buffer.ctb_commands.length);
	ADD("CTB residuals", transform->intermediate_buffer.ctb_residuals.base_addr,
	    transform->intermediate_buffer.ctb_residuals.length);

	// Reconstruction + PPB out
	if (transform->curr_picture_buffer.enable_flag) {
		ADD("O4 luma",  transform->curr_picture_buffer.pixels.luma_offset, omega_luma_size);
		ADD("O4 chroma", transform->curr_picture_buffer.pixels.chroma_offset, omega_luma_size / 2);
		if (transform->curr_picture_buffer.ppb_offset != 0 && ppb_size != 0) {
			ADD("PPB out", transform->curr_picture_buffer.ppb_offset, ppb_size);
		}
	}

	if (transform->curr_display_buffer.enable_flag) {
		ADD("R2B luma", transform->curr_display_buffer.pixels.luma_offset, raster_luma_size);
		ADD("R2B chroma", transform->curr_display_buffer.pixels.chroma_offset, raster_luma_size / 2);
	}

	if (transform->curr_resize_buffer.enable_flag) {
		ADD("R2Bdec luma", transform->curr_resize_buffer.pixels.luma_offset, decimated_luma_size);
		ADD("R2Bdec chroma", transform->curr_resize_buffer.pixels.chroma_offset, decimated_luma_size / 2);
	}

	// MME command & status
	ADD("MME", (uint32_t)(commandAndStatusAddr->physical), commandAndStatusAddr->size);

	// PPB read
	for (ref = 0; ref < transform->num_reference_pictures; ref++) {
		ADD("PPB in", transform->ref_picture_buffer[ref].ppb_offset, ppb_size);
	}

	MT_MonitorBuffers(MT_SOURCE_DATA_HWPE, MT_SOURCE_MONO_MASK, range,
	                  ranges); // All HEVC buffers (read IB, read/write PPB, write RCN)

	/////////////////////////////////////////////////
	//                 FIRMWARE                    //
	/////////////////////////////////////////////////

	range = 0;
	ADD("Hades L3", 0x41000000, 0x0E600000);   // BPA2 partition
	ADD("base+reloc", 0x78000000, 0x04000000); // reloc zone + base RT
	MT_MonitorBuffers(MT_SOURCE_CODE_ID, MT_SOURCE_CODE_MASK, range, ranges); // Firmware code sections

	range = 0;
	ADD("RUN payload", (uint32_t)(payload->physical), payload->size);
	MT_MonitorBuffers(MT_SOURCE_DATA_FIRMWARE, MT_SOURCE_MONO_MASK, 1, ranges); // Firmware data

	/////////////////////////////////////////////////
	//               XPRED - MCC                   //
	/////////////////////////////////////////////////

	for (ref = 0; ref < transform->num_reference_pictures; ref++) {
		if (ref * 2 + 1 >= MT_MAX_RANGES) {
			pr_info("Hades: Memtest: skipping remaining refs\n");
			break;
		}
		ranges[ref * 2].name = "ref luma";
		ranges[ref * 2].start =  transform->ref_picture_buffer[ref].pixels.luma_offset;
		ranges[ref * 2].length = omega_luma_size;
		ranges[ref * 2 + 1].name = "ref chroma";
		ranges[ref * 2 + 1].start =  transform->ref_picture_buffer[ref].pixels.chroma_offset;
		ranges[ref * 2 + 1].length = omega_luma_size / 2;
	}
	MT_MonitorBuffers(MT_SOURCE_XPRED, MT_SOURCE_MONO_MASK, ref * 2, ranges);


	if (memtest >= 0) {
		MT_StartMonitoring(memtest);
	} else {
		MT_StartMonitoring(skip++);
	}
}

static void stopMemTest(void)
{
	MT_StopMonitoring();
}

//
//
// Linux Module & Driver functions
//
//

//
// Loads firmware from user space filesystem into kernel memory
// Caller is responsible for freeing the returned buffer
//
#ifndef VSOC_MODEL
static char *LoadFirmware(const char *firmwareName, struct device *dev, int *fw_size)
{
	const struct firmware *fw_entry;
	char                  *firmware_p;
	int                    result;

	// Get the firmware
	if ((result = request_firmware(&fw_entry, firmwareName, dev)) != 0) {
		pr_err("Error: %s loading firmware into kernel space %s (%d)\n", __func__, MODULE_NAME, result);
		return NULL;
	}

	// allocate kernel memory to hold firmware
	firmware_p = OSDEV_Malloc(fw_entry->size);
	if (firmware_p == NULL) {
		pr_err("Error: %s allocating memory for firmware\n", __func__);
		release_firmware(fw_entry);
		return NULL;
	}

	memcpy(firmware_p, fw_entry->data, fw_entry->size);
	*fw_size = fw_entry->size;

	release_firmware(fw_entry);

	return firmware_p;
}
#endif // VSOC_MODEL

static int LoadHadesDevice()
{
	HadesError_t                hades_status = HADES_NO_ERROR;
	HadesTransformerParams_t    InitParams;
	int ret = 0;

	if (HadesHandle != NULL) {
		pr_err("Error: %s hades handle is not null\n", __func__);
		return -EINVAL;
	}

	ret = HadesDynamicPmOn();
	if (ret) {
		pr_err("Error: %s - unable to dynamic power on\n", __func__);
		return ret;
	}

	hades_status = HADES_Init(& HevcDeviceContext->hades_params);
	if (hades_status != HADES_NO_ERROR) {
		pr_err("Error: %s initializing HADES API (%d)\n", __func__, hades_status);
		ret = -EINVAL;
		goto hades_pmoff;
	}

	pr_info("%s: Hades API initialized with registers base %p, IRQ base %d, BPA2 partition %s\n",
	        __func__, (void *)(HevcDeviceContext->hades_params.HadesBaseAddress),
	        HevcDeviceContext->hades_params.InterruptNumber, HevcDeviceContext->hades_params.partition);

	// Boots the firmware
	strncpy(HevcDeviceContext->fw_info.Name, firmwareName, PATH_LENGTH);
	HevcDeviceContext->fw_info.Name[sizeof(HevcDeviceContext->fw_info.Name) - 1] = '\0';

	hades_status = HADES_Boot(&HevcDeviceContext->fw_info, (uintptr_t)firmwareContents, firmwareSize);
	if (hades_status != HADES_NO_ERROR) {
		pr_err("Error: %s booting firmware %s (%d)\n", __func__, MODULE_NAME, hades_status);
		ret = -EINVAL;
		goto fail_loadhades;
	}
	pr_notice("%s: HADES FW name: %s, size: %d\n", __func__, firmwareName, HevcDeviceContext->fw_info.Size);

	strncpy(InitParams.TransformerName, HADES_HEVC_TRANSFORMER_NAME, sizeof(InitParams.TransformerName));
	InitParams.TransformerName[sizeof(InitParams.TransformerName) - 1] = '\0';

	InitParams.FWImageAddr = (uintptr_t)firmwareContents;
	InitParams.FWImageSize = firmwareSize;

	hades_status = HADES_InitTransformer(&InitParams, &HadesHandle);
	if (hades_status != HADES_NO_ERROR) {
		pr_err("Error: %s initializing HADES transformer %s (%d)\n", __func__, MODULE_NAME, hades_status);
		ret = -EINVAL;
		goto fail_loadhades;
	}

	goto hades_pmoff;

fail_loadhades:
	HADES_Term();
hades_pmoff:
	HadesDynamicPmOff();
	return ret;
}

static int UnloadHadesDevice()
{
	int ret = 0;
	HadesError_t Status = HADES_NO_ERROR;

	if (HadesHandle == NULL) {
		pr_err("Error: %s hades handle is null\n", __func__);
		return -EINVAL;
	}

	ret = HadesDynamicPmOn();
	if (ret) {
		pr_err("Error: %s - unable to dynamic power on\n", __func__);
		goto fail_hadesdynamicpoweron;
	}

	Status = HADES_TermTransformer(HadesHandle);
	if (Status != HADES_NO_ERROR) {
		pr_err("Error: %s - error %d while terminating %s MME transformer\n", __func__, Status, mmeName);
	}

	HadesDynamicPmOff();

fail_hadesdynamicpoweron:
	HadesHandle = NULL;

	return ret;
}

static void hades_unprep_clk(struct HevcDeviceContext_t *hades_ctx)
{
#ifdef CONFIG_ARCH_STI
	clk_unprepare(hades_ctx->clk_fc);
	clk_unprepare(hades_ctx->clk_hwpe);
#endif //CONFIG_ARCH_STI
}

static int hades_get_of_pdata(struct platform_device *pdev, struct HevcDeviceContext_t *hades_ctx)
{
	int ret = 0;
#ifdef CONFIG_ARCH_STI
	hades_ctx->clk_hwpe = devm_clk_get(&pdev->dev, "clk_hwpe_hades");
	if (IS_ERR(hades_ctx->clk_hwpe)) {
		pr_err("Error: %s failed to get hades HWPE clock\n", __func__);
		return -EINVAL;
	}

	ret = clk_prepare(hades_ctx->clk_hwpe);
	if (ret) {
		pr_err("Error: %s failed to prepare hades HWPE clock (%d)\n", __func__, ret);
		return ret;
	}

	hades_ctx->clk_fc = devm_clk_get(&pdev->dev, "clk_fc_hades");
	if (IS_ERR(hades_ctx->clk_fc)) {
		pr_err("Error: %s failed to get hades FC clock\n", __func__);
		ret = -EINVAL;
		goto fail_clk_hwpe;
	}

	ret = HevcClockSetRate(pdev->dev.of_node);
	if (ret) {
		pr_err("Error: failed to set max frequencies for HADES clock (%d)\n", ret);
		return -EINVAL;
	}

	ret = clk_prepare(hades_ctx->clk_fc);
	if (ret) {
		pr_err("Error: %s failed to prepare hades FC clock (%d)\n", __func__, ret);
		goto fail_clk_hwpe;
	}

	return 0;

fail_clk_hwpe:
	clk_unprepare(hades_ctx->clk_hwpe);
#endif //CONFIG_ARCH_STI

	return ret;
}

// Power management callbacks
static struct notifier_block stm_pm_hades_notifier = {
	.notifier_call = stm_pm_hades_notifier_call,
};

static int RegisterHadesTransformer()
{
	MME_ERROR mme_status;
	int ret = 0;

	// Register transformer
	mme_status =  MME_RegisterTransformer(mmeName,
	                                      abortCmd,
	                                      getTransformerCapability,
	                                      initTransformer,
	                                      processCommand,
	                                      termTransformer);

	if (mme_status != MME_SUCCESS) {
		pr_err("Error: %s Registration of transformer %s failed\n", __func__, mmeName);
		ret = -ENODEV;
	}

	// Register transformer_extra
	mme_status =  MME_RegisterTransformer(mmeName_extra,
	                                      abortCmd,
	                                      getTransformerCapability,
	                                      initTransformer,
	                                      processCommand,
	                                      termTransformer);


	if (mme_status != MME_SUCCESS) {
		pr_err("Error: %s Registration of transformer %s failed\n", __func__, mmeName_extra);
		mme_status = MME_DeregisterTransformer(mmeName);
		ret = -(ENODEV);
	}

	return ret;
}

static int DeRegisterHadesTransformer()
{
	MME_ERROR status = MME_SUCCESS;
	int ret = 0;

	status = MME_DeregisterTransformer(mmeName);
	if (status != MME_SUCCESS) {
		pr_err("Error: %s Deregistration of transformer %s failed\n", __func__, mmeName);
		ret = -ENODEV;
	}

	status = MME_DeregisterTransformer(mmeName_extra);
	if (status != MME_SUCCESS) {
		pr_err("Error: %s Deregistration of transformer %s failed\n", __func__, mmeName_extra);
		ret = -ENODEV;
	}
	return ret;
}

//
// driver probe is called by kernel when the platform devices are added by the player2 "platform" module
//
static int HadesProbe(struct platform_device *pdev)
{
	struct device     *dev;
	struct resource   *pResource;
	int                ret = 0;

	BUG_ON(!pdev);
	dev = &pdev->dev;

	HevcDeviceContext = devm_kzalloc(&pdev->dev, sizeof(*HevcDeviceContext), GFP_KERNEL);
	if (!HevcDeviceContext) {
		pr_err("Error: %s alloc failed\n", __func__);
		ret = -ENOMEM;
		goto fail_hades;
	}

	/**
	 * Saving pointer to main struct to be able to retrieve it in pm_runtime
	 * callbacks for example (it is *NOT* platform device data, just *driver* data)
	 */
	platform_set_drvdata(pdev, HevcDeviceContext);
	HevcDeviceContext->iDev = &pdev->dev;

	if (pdev->dev.of_node) {
		pr_info("HadesProbe: Probing device with DT\n");
		ret = hades_get_of_pdata(pdev, HevcDeviceContext);
		if (ret) {
			pr_err("Error: %s hades failed to retrieve driver data\n", __func__);
			goto fail_hades;
		}
	} else {
		pr_info("Error: %s hades no DT entry\n", __func__);
	}

#ifdef CONFIG_PM_RUNTIME
	// Power management
	/* Clear the device's 'power.runtime_error' flag and set the device's runtime PM status to 'suspended' */
	pm_runtime_set_suspended(&pdev->dev);
	pm_suspend_ignore_children(&pdev->dev, 1);
	pm_runtime_enable(&pdev->dev);
#endif

	// Platform info: get register base address and IRQ number
	pResource = platform_get_resource_byname(pdev, IORESOURCE_MEM, "hades_registers");
	if (!pResource) {
		pr_err("Error: %s platform_get_resource_byname failed\n", __func__);
		ret = -ENODEV;
		goto fail_hadesprobe;
	}

	HevcDeviceContext->hades_params.HadesBaseAddress = pResource->start - HADES_FABRIC_OFFSET;
	HevcDeviceContext->hades_params.HadesSize = pResource->end - HevcDeviceContext->hades_params.HadesBaseAddress + 1;
	pr_debug("%s: registers base address is %p\n", __func__, (void *)HevcDeviceContext->hades_params.HadesBaseAddress);

	ret = platform_get_irq_byname(pdev, "hades_gen_irq");
	if (ret <= 0) {
		pr_err("Error: %s - platform_get_irq_byname failed %d\n", __func__, ret);
		goto fail_hadesprobe;
	} else {
		pr_debug("%s - irq number is %d\n", __func__, ret);
	}

	HevcDeviceContext->hades_params.InterruptNumber = ret;
	pr_debug("%s - hades request IRQ %d\n", __func__, HevcDeviceContext->hades_params.InterruptNumber);

#ifdef CONFIG_ARCH_STI
	ret = clk_enable(HevcDeviceContext->clk_fc);
	if (ret) {
		pr_err("Error: %s - Enabling FC clock failed (ret:%d)\n", __func__, ret);
		goto fail_hadesprobe;
	}
#endif
	//TODO hades_err_irq

#ifndef VSOC_MODEL
	// Loads the firmware into kernel memory
	firmwareContents = LoadFirmware(firmwareName, dev, &firmwareSize);
	if (firmwareContents == NULL) {
		pr_err("Error: %s - loading firmware %s\n", __func__, firmwareName);
		pr_err("Error: %s - ... for debug purposes, ignore error\n", __func__); // FIXME
                goto fail_hadesdevice;
	}
#endif

	// Initialize HADES API
	HevcDeviceContext->hades_params.partition = partition;

	ret = LoadHadesDevice();
	if (ret) {
		pr_err("Error: %s LoadHadesDevice failed %d\n", __func__, ret);
		goto fail_hadesdevice;
	}

	// Register transformer
	ret =  RegisterHadesTransformer();
	if (ret) {
		pr_err("Error: %s RegisterHadesTransformer failed %d\n", __func__, ret);
		goto fail_hadesTransformer;
	}

	// Register with PM notifiers (for HPS/CPS support)
	register_pm_notifier(&stm_pm_hades_notifier);

	if (CRCDriver_Init(CRC_DEVICE_NAME) != 0) {
		pr_err("Error: %s - CRCDriver_Init failed\n", __func__);
		// FIXME: keep going?
	}

	OSDEV_Dump_MemCheckCounters(__func__);
	pr_info("%s probe done ok\n", MODULE_NAME);

	return 0;

fail_hadesTransformer:
	UnloadHadesDevice();
	HADES_Term();
fail_hadesdevice:
#ifndef VSOC_MODEL
	OSDEV_Free(firmwareContents);
#endif
#ifdef CONFIG_ARCH_STI
	clk_disable(HevcDeviceContext->clk_fc);
#endif
fail_hadesprobe:
	hades_unprep_clk(HevcDeviceContext);
#ifdef CONFIG_PM_RUNTIME
	pm_runtime_disable(&pdev->dev);
#endif
fail_hades:
	OSDEV_Dump_MemCheckCounters(__func__);
	pr_err("Error: %s probe failed\n", MODULE_NAME);

	return ret;
}

static int HadesRemove(struct platform_device *pdev)
{
	UnloadHadesDevice();

	// Terminate MME & HADES API
	DeRegisterHadesTransformer();

	unregister_pm_notifier(&stm_pm_hades_notifier);

#ifndef VSOC_MODEL
	// The firmware won't be used in further InitTransformer()s
	OSDEV_Free(firmwareContents);
#endif

	HADES_Term();
#ifdef CONFIG_ARCH_STI
	clk_disable(HevcDeviceContext->clk_fc);
#endif
	hades_unprep_clk(HevcDeviceContext);
#ifdef CONFIG_PM_RUNTIME
	// Prevent use of PM runtime
	pm_runtime_disable(&pdev->dev);
#endif
	CRCDriver_Term();

	OSDEV_Dump_MemCheckCounters(__func__);
	pr_info("%s remove done\n", MODULE_NAME);

	return 0;
}

static int HadesPmSuspend(struct device *dev)
{
	pr_info("Hades suspend\n");
	HadesHandle = NULL;
	HADES_Term();
	return 0;
}

// resume callback, being called on Host Passive Standby exit (HPS exit)
static int HadesPmResume(struct device *dev)
{
	int ret = 0;
	pr_info("Hades resume\n");

	ret = HevcClockSetRate(dev->of_node);
	if (ret) {
		pr_err("Error: failed to set max frequencies for HADES clock (%d)\n", ret);
		return -EINVAL;
	}

	ret = LoadHadesDevice();
	if (ret) {
		pr_err("Error: %s - booting hades, status(%d)\n", __func__, ret);
		ret = -EINVAL;
	}
	return ret;
}

// freeze callback, being called on Controller Passive Standby entry (CPS entry)
static int HadesPmFreeze(struct device *dev)
{
	pr_info("Hades freeze\n");
	HadesHandle = NULL;
	HADES_Term();
	return 0;
}

// restore callback, being called on Controller Passive Standby exit (CPS exit)
static int HadesPmRestore(struct device *dev)
{
	int ret = 0;
	pr_info("Hades restore\n");

	ret = HevcClockSetRate(dev->of_node);
	if (ret) {
		pr_err("Error: failed to set max frequencies for HADES clock (%d)\n", ret);
		return -EINVAL;
	}

	ret = LoadHadesDevice();
	if (ret) {
		pr_err("Error: %s - booting hades, status(%d)\n", __func__, ret);
		ret = -EINVAL;
	}
	return ret;
}

// Hades runtime_suspend callback: relates to active profile standby
static int HadesPmRuntimeSuspend(struct device *dev)
{
	pr_info("HadesPmRuntime: Suspend ..\n");

	//Check no existing encoder instance to allow callback execution
	if (HevcInstanceNumber != 0) {
		pr_info("HadesPmRuntime: still %d instances running ..\n", HevcInstanceNumber);

		// Signal PM core there is still a user for the device: otherwise, runtime_suspend will never be called any more (see bug 22377)
		pm_runtime_get(dev);

		// warning, error code should be 0, -EBUSY or -EAGAIN to enable future callback execution
		return -EBUSY;
	}

	return 0;
}

// Hades runtime_resume callback
static int HadesPmRuntimeResume(struct device *dev)
{
	pr_info("HadesPmRuntime: Resume ..\n");

	return 0;
}

