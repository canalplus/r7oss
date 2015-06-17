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

#include <linux/module.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/init.h>    /* Initialisation support */
#include <linux/kernel.h>  /* Kernel support */
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/of.h>
#include <linux/suspend.h>
#include <linux/pm_runtime.h>
#include <linux/pm.h>

#include <mme.h>

#include "osdev_mem.h"

#include "hva_registers.h"
#include "h264_encode.h"
#include "h264_encode_hard_host_transformer.h"
#include "h264_encode_platform.h"

#include "H264ENCHW_VideoTransformerTypes.h"

#define MODULE_NAME "H264 encode hardware host transformer"

static char *mmeName = H264ENCHW_MME_TRANSFORMER_NAME;

MODULE_DESCRIPTION("H264 encode hardware cell platform driver");
MODULE_AUTHOR("STMicroelectronics");
MODULE_LICENSE("GPL");

module_param(mmeName , charp, S_IRUGO);
MODULE_PARM_DESC(mmeName, "Name to use for MME Transformer registration");

static void copyFrameParameters(H264EncodeHardFrameParams_t  *frameParamsOut,
                                H264EncodeHard_TransformParam_t *frameParamsIn);
static void copySequenceParameters(H264EncodeHardSequenceParams_t  *seqParamsOut,
                                   H264EncodeHard_SetGlobalParamSequence_t *seqParamsIn);
static int  HvaClkOn(void);
static void HvaClkOff(void);
static int  HvaPowerOn(void);
static void HvaPowerOff(void);
static int ReadHvaVersion(void);
static int RegisterHvaTransformer(void);
static int DeRegisterHvaTransformer(void);
static int stm_pm_hva_notifier_call(struct notifier_block *this, unsigned long event, void *ptr);

/* uncomment this to print CLK_HVA clock tree values */
//#define DISPLAY_CLKVALUES
static void DisplayClkValues(void);
static int HvaClockSetRate(struct device_node *of_node);

// Static variables
static H264EncodeHardFrameParams_t   frameParams;
static H264EncodeHardSequenceParams_t  sequenceParams;

//Global hva driver data
struct HvaDriverData *pDriverData;

//Global variables: to store platform specific information (SOC dependant)
HVAPlatformData_t platformData;
extern unsigned char h264EncInstanceNb;

extern unsigned int HvaRegisterBase;

static unsigned int HvaIPVersion;

// PM notifier callback
// At entry of low power, multicom is terminated in .freeze callback. When system exit from
// low power, multicom is initialized through pm_notifier, so we have to register HVA transformer
// again. After HVA pm_notifier, SE module notifier would be called in which other multicom apis
// are called.
// This callback is used to register the HVA transformer after low power exit
#define ENTRY(enum) case enum: return #enum
static inline const char *StringifyPmEvent(int aPmEvent)
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


static int HvaClockSetRate(struct device_node *of_node)
{
	int ret = 0;
	/* check incase 0 is set for max frequency property in DT */
	if (pDriverData->max_freq) {
		ret = clk_set_rate(pDriverData->clk, pDriverData->max_freq);
		if (ret) {
			dev_err(pDriverData->dev, "Error: setting max frequency failed (%d)\n", ret);
			return -EINVAL;
		}
		dev_info(pDriverData->dev, "Hva clock set to %u\n", pDriverData->max_freq);
	}
	return ret;
}

static int stm_pm_hva_notifier_call(struct notifier_block *this, unsigned long event, void *ptr)
{
	int ErrorCode;

	switch (event) {
	case PM_POST_SUSPEND:
	// fallthrough
	case PM_POST_HIBERNATION:
		dev_info(pDriverData->dev, "Hva notifier: %s\n", StringifyPmEvent(event));
		ErrorCode = RegisterHvaTransformer();
		if (ErrorCode != 0) {
			// Error returned by HVA, but this should not prevent the system exiting low power
			dev_err(pDriverData->dev, "Error: register module %s failed (%d)\n", MODULE_NAME, ErrorCode);
		}
		break;

	default:
		break;
	}

	return NOTIFY_DONE;
}

static int ReadHvaVersion(void)
{
	int ret = 0;
	if (HvaPowerOn()) {
		dev_err(pDriverData->dev, "Error: power on failed\n");
		return -EINVAL;
	}

	if (HvaClkOn()) {
		dev_err(pDriverData->dev, "Error: clock on failed\n");
		ret = -EINVAL;
		goto fail_clockon_ver;
	}

	HvaIPVersion = ReadHvaRegister(HVA_HIF_REG_VERSION);
	dev_info(pDriverData->dev, "Hva version is 0x%x\n", HvaIPVersion);
	HvaClkOff();
	HvaPowerOff();

	if (HvaIPVersion != HVA_HIF_REG_EXPECTED_VERSION) {
		dev_dbg(pDriverData->dev, "Incorrect hva version 0x%x, expected version 0x%x\n", HvaIPVersion,
		        HVA_HIF_REG_EXPECTED_VERSION);
	}

	return ret;

fail_clockon_ver:
	HvaPowerOff();
	return ret;
}

static int  HvaPowerOn(void)
{
	int ret = 0;
#ifdef CONFIG_PM_RUNTIME
	if (pm_runtime_get_sync(pDriverData->dev) < 0) {
		dev_err(pDriverData->dev, "Error: pm_runtime_get_sync failed\n");
		ret = -EINVAL;
	}
#endif

	return ret;
}

static void HvaPowerOff(void)
{
#ifdef CONFIG_PM_RUNTIME
	pm_runtime_mark_last_busy(pDriverData->dev);
	pm_runtime_put(pDriverData->dev);
#endif
}

MME_ERROR initTransformer(MME_UINT paramsSize, MME_GenericParams_t params, void **context)
{
	H264EncodeHardHandle_t      encodeHandle = NULL;
	H264EncodeHardInitParams_t *InitParams;
	H264EncodeHardStatus_t status;

	// We can ignore the "params" and give back any value for a handle.
	if (context == NULL) {
		return MME_INVALID_HANDLE;
	}

	InitParams = (H264EncodeHardInitParams_t *) params;
	dev_dbg(pDriverData->dev, "Retrieved memory profile %u (%s)\n", InitParams->MemoryProfile,
	        StringifyMemoryProfile(InitParams->MemoryProfile));


	status = H264HwEncodeInit(InitParams, &encodeHandle);

	*context = (void *)encodeHandle;

	switch (status) {
	case H264ENCHARD_NO_ERROR:
		return MME_SUCCESS;
	case H264ENCHARD_ERROR:
		return MME_INTERNAL_ERROR;
	case H264ENCHARD_NO_SDRAM_MEMORY:
		return MME_NOMEM;
	case H264ENCHARD_NO_HVA_ERAM_MEMORY:
		return MME_NOMEM;
	default:
		return MME_INTERNAL_ERROR;
	}
}

void FillMMEInfo(H264EncodeHard_AddInfo_CommandStatus_t *AddInfo_p, H264EncodeHardCodecContext_t    *h264CodecContext)
{
	AddInfo_p->StructSize          = sizeof(H264EncodeHard_AddInfo_CommandStatus_t);
	AddInfo_p->bitstreamSize       = h264CodecContext->statusBitstreamSize;
	AddInfo_p->removalTime         = h264CodecContext->statusRemovalTime;
	AddInfo_p->stuffingBits        = h264CodecContext->statusStuffingBits;
	AddInfo_p->nonVCLNALUSize      = h264CodecContext->statusNonVCLNALUSize;
	AddInfo_p->hwError             = h264CodecContext->hwError;
	AddInfo_p->transformStatus     = h264CodecContext->transformStatus;
	AddInfo_p->frameEncodeDuration = h264CodecContext->frameEncodeDuration;
}

// With asynchronous commands (Using the MME Deferred) mechanism
// We need to provide a Callback which will alert the host upon command completion
int HandleHVACompletionCallback(H264EncodeHardCodecContext_t *Context)
{
	MME_Command_t                *MMECmd = (MME_Command_t *) Context->CallbackHandle;

	MME_Event_t                   MMEEvent  = MME_COMMAND_COMPLETED_EVT;
	MME_ERROR                     MMEStatus = MME_SUCCESS;

	HvaClkOff();
	HvaPowerOff();

	if (Context->encodeStatus != H264ENCHARD_NO_ERROR) {
		dev_err(pDriverData->dev, "Error: hva encode failed 0x%x (%s)\n", Context->encodeStatus,
		        StringifyEncodeStatus(Context->encodeStatus));
	}

	switch (Context->encodeStatus) {
	case H264ENCHARD_NO_ERROR:
		MMEStatus = MME_SUCCESS;
		break;
	case H264ENCHARD_HW_ERROR:
		MMEStatus = MME_INTERNAL_ERROR;
		break;
	case H264ENCHARD_CMD_DISCARDED:
		MMEStatus = MME_COMMAND_STILL_EXECUTING; // KB: Really? This doesn't seem appropriate.
		break;
	default:
		MMEStatus = MME_INTERNAL_ERROR;
		break;
	}

	// Update the AdditionalInfo before returning the command to Multicom.
	FillMMEInfo(MMECmd->CmdStatus.AdditionalInfo_p, Context);

	return MME_NotifyHost(MMEEvent, MMECmd, MMEStatus);
}

MME_ERROR processCommand(void *context, MME_Command_t *cmd)
{
	H264EncodeHard_AddInfo_CommandStatus_t *AddInfo_p = cmd->CmdStatus.AdditionalInfo_p;
	H264EncodeHardCodecContext_t    *h264CodecContext = (H264EncodeHardCodecContext_t *)context;
	H264EncodeHardStatus_t               encodeStatus = H264ENCHARD_ERROR;
	MME_ERROR status = MME_SUCCESS;

	//FIXME: switch to "deferred command" at mid term, will then issue events to be handled by client's callbacks
	switch (cmd->CmdCode) {
	case MME_TRANSFORM:
		status = MME_TRANSFORM_DEFERRED;
		copyFrameParameters(&frameParams, (H264EncodeHard_TransformParam_t *)cmd->Param_p);
		h264CodecContext->Callback       = HandleHVACompletionCallback;
		h264CodecContext->CallbackHandle = cmd;

		if (HvaPowerOn()) {
			dev_err(pDriverData->dev, "Error: power on failed\n");
			status = MME_INTERNAL_ERROR;
			goto bail_hvaencode;
		}

		if (HvaClkOn()) {
			dev_err(pDriverData->dev, "Error: clock on failed\n");
			status = MME_INTERNAL_ERROR;
			goto fail_clockon;
		}

		encodeStatus = H264HwEncodeEncodeFrame((H264EncodeHardHandle_t)context, &frameParams);

		if (encodeStatus != H264ENCHARD_ENCODE_DEFERRED) {
			//Fill AddInfo field of CmdStatus with output parameter of encode task
			FillMMEInfo(AddInfo_p, h264CodecContext);

			if (encodeStatus != H264ENCHARD_NO_ERROR) {
				dev_err(pDriverData->dev, "Error: encode frame failed 0x%x (%s)\n", encodeStatus, StringifyEncodeStatus(encodeStatus));
				switch (encodeStatus) {
				case H264ENCHARD_HW_ERROR:
					status =  MME_INTERNAL_ERROR;
					goto fail_hvaencode;

				case H264ENCHARD_CMD_DISCARDED:
					status =  MME_COMMAND_STILL_EXECUTING;
					goto fail_hvaencode;

				default:
					status =  MME_INTERNAL_ERROR;
					goto fail_hvaencode;
				}
			}
		}
		goto bail_hvaencode;

	case MME_SET_GLOBAL_TRANSFORM_PARAMS:
		copySequenceParameters(&sequenceParams, (H264EncodeHard_SetGlobalParamSequence_t *)cmd->Param_p);
		encodeStatus = H264HwEncodeSetSequenceParams((H264EncodeHardHandle_t)context, &sequenceParams);
		if (encodeStatus != H264ENCHARD_NO_ERROR) {
			dev_err(pDriverData->dev, "Error: set sequence parameters failed 0x%x (%s)\n", encodeStatus,
			        StringifyEncodeStatus(encodeStatus));
			status = MME_INTERNAL_ERROR;
		}
		goto bail_hvaencode;

	case MME_SEND_BUFFERS:
		status = MME_NOT_IMPLEMENTED;
		goto bail_hvaencode;

	default:
		status = MME_NOT_IMPLEMENTED;
		goto bail_hvaencode;
	}

fail_hvaencode:
	HvaClkOff();
fail_clockon:
	HvaPowerOff();
bail_hvaencode:
	/* At this point if Clock/Power is ON then it would be turned OFF inside HandleHVACompletionCallback */
	return status;
}

MME_ERROR abortCmd(void *context, MME_CommandId_t cmdId)
{
	return MME_NOT_IMPLEMENTED;
}

MME_ERROR getTransformerCapability(MME_TransformerCapability_t *capability)
{
	H264EncodeHard_TransformerCapability_t *pCap;

	capability->Version = 0x1;

	pCap = (H264EncodeHard_TransformerCapability_t *)capability->TransformerInfo_p;

	pCap->MaxXSize                    = H264_ENCODE_MAX_SIZE_X;
	pCap->MaxYSize                    = H264_ENCODE_MAX_SIZE_Y;
	pCap->BufferAlignment             = ENCODE_BUFFER_ALIGNMENT;
	pCap->areBFramesSupported         = false;
	pCap->MaxFrameRate                = 60;
	pCap->isT8x8Supported             = true;
	pCap->isSliceLossSupported        = false;
	pCap->isMultiSliceEncodeSupported = true;
	pCap->isIntraRefreshSupported     = true;

	capability->TransformerInfoSize = sizeof(H264EncodeHard_TransformerCapability_t);

	return MME_SUCCESS;
}

MME_ERROR termTransformer(void *context)
{
	H264EncodeHardStatus_t encodeStatus;

	encodeStatus = H264HwEncodeTerminate((H264EncodeHardHandle_t)context);

	dev_dbg(pDriverData->dev, "Delete one encode instance: termTransformer called\n");

	if (encodeStatus == H264ENCHARD_NO_ERROR) {
		return MME_SUCCESS;
	} else {
		return MME_INTERNAL_ERROR;
	}
}

static void copyFrameParameters(H264EncodeHardFrameParams_t  *frameParamsOut,
                                H264EncodeHard_TransformParam_t *frameParamsIn)
{
	//Same abstraction level between MME API and internal management of Frame parameters
	frameParamsOut->pictureCodingType          = (uint16_t)frameParamsIn->pictureCodingType;
	frameParamsOut->idrFlag                    = (uint16_t)frameParamsIn->idrFlag;
	frameParamsOut->frameNum                   = (uint32_t)frameParamsIn->frameNum;
	frameParamsOut->firstPictureInSequence     = (uint16_t)frameParamsIn->firstPictureInSequence;
	frameParamsOut->disableDeblockingFilterIdc = (uint16_t)frameParamsIn->disableDeblockingFilterIdc;
	frameParamsOut->sliceAlphaC0OffsetDiv2     = (int16_t) frameParamsIn->sliceAlphaC0OffsetDiv2;
	frameParamsOut->sliceBetaOffsetDiv2        = (int16_t) frameParamsIn->sliceBetaOffsetDiv2;
	frameParamsOut->addrSourceBuffer           = (uint32_t)frameParamsIn->addrSourceBuffer;
	frameParamsOut->addrOutputBitstreamStart   = (uint32_t)frameParamsIn->addrOutputBitstreamStart;
	frameParamsOut->addrOutputBitstreamEnd     = (uint32_t)frameParamsIn->addrOutputBitstreamEnd;
	frameParamsOut->bitstreamOffset            = (uint16_t)frameParamsIn->bitstreamOffset;
	frameParamsOut->seiRecoveryPtPresentFlag   = (uint8_t)(frameParamsIn->seiRecoveryPtPresentFlag ? 1 : 0);
	frameParamsOut->seiRecoveryFrameCnt        = (uint16_t) frameParamsIn->seiRecoveryFrameCnt;
	frameParamsOut->seiBrokenLinkFlag          = (uint8_t)(frameParamsIn->seiBrokenLinkFlag ? 1 : 0);
	frameParamsOut->seiUsrDataT35PresentFlag   = (uint8_t)(frameParamsIn->seiUsrDataT35PresentFlag ? 1 : 0);
	frameParamsOut->seiT35CountryCode          = (uint8_t)  frameParamsIn->seiT35CountryCode;
	frameParamsOut->seiAddrT35PayloadByte      = (uint32_t) frameParamsIn->seiAddrT35PayloadByte;
	frameParamsOut->seiT35PayloadSize          = (uint32_t) frameParamsIn->seiT35PayloadSize;
	frameParamsOut->log2MaxFrameNumMinus4      = (uint16_t)frameParamsIn->log2MaxFrameNumMinus4;
}

static void copySequenceParameters(H264EncodeHardSequenceParams_t  *seqParamsOut,
                                   H264EncodeHard_SetGlobalParamSequence_t *seqParamsIn)
{
	seqParamsOut->frameWidth                 = (uint16_t)seqParamsIn->frameWidth;
	seqParamsOut->frameHeight                = (uint16_t)seqParamsIn->frameHeight;
	seqParamsOut->picOrderCntType            = (uint16_t)seqParamsIn->picOrderCntType;
	seqParamsOut->log2MaxFrameNumMinus4      = (uint16_t)seqParamsIn->log2MaxFrameNumMinus4;
	seqParamsOut->useConstrainedIntraFlag    = (uint16_t)seqParamsIn->useConstrainedIntraFlag;
	seqParamsOut->maxSumNumBitsInNALU        = (uint32_t)seqParamsIn->maxSumNumBitsInNALU;
	seqParamsOut->intraRefreshType           = (uint16_t)seqParamsIn->intraRefreshType;
	seqParamsOut->irParamOption              = (uint16_t)seqParamsIn->irParamOption;
	seqParamsOut->brcType                    = (uint16_t)seqParamsIn->brcType;
	seqParamsOut->cpbBufferSize              = (uint32_t)seqParamsIn->cpbBufferSize;
	seqParamsOut->bitRate                    = (uint32_t)seqParamsIn->bitRate;
	seqParamsOut->framerateNum               = (uint16_t)seqParamsIn->framerateNum;
	seqParamsOut->framerateDen               = (uint16_t)seqParamsIn->framerateDen;
	seqParamsOut->encoderComplexity          = (uint16_t)seqParamsIn->encoderComplexity;
	seqParamsOut->quant                      = (uint16_t)seqParamsIn->vbrInitQp;
	seqParamsOut->samplingMode               = (uint16_t)seqParamsIn->samplingMode;
	seqParamsOut->delay                      = (uint16_t)seqParamsIn->cbrDelay;
	seqParamsOut->qpmin                      = (uint16_t)seqParamsIn->qpmin;
	seqParamsOut->qpmax                      = (uint16_t)seqParamsIn->qpmax;
	seqParamsOut->sliceNumber                = (uint16_t)seqParamsIn->sliceNumber;
	seqParamsOut->transformMode              = (uint16_t)HVA_ENCODE_NO_T8x8_ALLOWED;
	// Allow T8x8 for High Profile
	if ((seqParamsIn->profileIdc == HVA_ENCODE_SPS_PROFILE_IDC_HIGH)     ||
	    (seqParamsIn->profileIdc == HVA_ENCODE_SPS_PROFILE_IDC_HIGH_10) ||
	    (seqParamsIn->profileIdc == HVA_ENCODE_SPS_PROFILE_IDC_HIGH_422) ||
	    (seqParamsIn->profileIdc == HVA_ENCODE_SPS_PROFILE_IDC_HIGH_444)) {
		seqParamsOut->transformMode          = (uint16_t)seqParamsIn->transformMode;
	}

	seqParamsOut->profileIdc                 = (uint8_t) seqParamsIn->profileIdc;
	seqParamsOut->levelIdc                   = (uint8_t) seqParamsIn->levelIdc;

	seqParamsOut->vuiParametersPresentFlag   = (uint8_t)(seqParamsIn->vuiParametersPresentFlag ? 1 : 0);
	seqParamsOut->vuiAspectRatioIdc          = (uint8_t) seqParamsIn->vuiAspectRatioIdc;
	seqParamsOut->vuiSarWidth                = (uint16_t) seqParamsIn->vuiSarWidth;
	seqParamsOut->vuiSarHeight               = (uint16_t) seqParamsIn->vuiSarHeight;
	seqParamsOut->vuiOverscanAppropriateFlag = (uint8_t)(seqParamsIn->vuiOverscanAppropriateFlag ? 1 : 0);
	seqParamsOut->vuiVideoFormat             = (uint8_t)  seqParamsIn->vuiVideoFormat;
	seqParamsOut->vuiVideoFullRangeFlag      = (uint8_t)(seqParamsIn->vuiVideoFullRangeFlag ? 1 : 0);
	seqParamsOut->vuiColorStd                = (uint8_t)  seqParamsIn->vuiColorStd;
	seqParamsOut->seiPicTimingPresentFlag    = (uint8_t)(seqParamsIn->seiPicTimingPresentFlag ? 1 : 0);
	seqParamsOut->seiBufPeriodPresentFlag    = (uint8_t)(seqParamsIn->seiBufPeriodPresentFlag ? 1 : 0);
}

/* this API is for debug purpose to print clk values for stih407/410 board*/
static void DisplayClkValues()
{
#ifdef DISPLAY_CLKVALUES
	int err = 0;
	unsigned long clk_sysin_rate = 0;
	unsigned long clk_co_ref_rate = 0;
	unsigned long clk_co_pll1_rate = 0;
	unsigned long clk_hva_rate = 0;
	struct clk *clk_sysin = 0;
	struct clk *clk_co_ref = 0;
	struct clk *clk_co_pll1 = 0;
	struct clk *clk_hva = 0;

	dev_info(pDriverData->dev, "[Hva clock tree]\n");

	err = clk_add_alias("clk_sysin", NULL, "CLK_SYSIN", NULL);
	if (err) {
		dev_err(pDriverData->dev, "Error: add clock alias failed (%d)\n", err);
		return;
	}
	clk_sysin = devm_clk_get(pDriverData->dev, "clk_sysin");
	if (!clk_sysin) {
		dev_err(pDriverData->dev, "Error: get clock handle failed\n");
		return;
	}
	clk_sysin_rate = clk_get_rate(clk_sysin);
	dev_info(pDriverData->dev, "|-clk_sysin_rate   = %lu Hz\n", clk_sysin_rate);

	err = clk_add_alias("clk_co_ref", NULL, "CLK_C0_REF", NULL);
	if (err) {
		dev_err(pDriverData->dev, "Error: add clock alias failed (%d)\n", err);
		return;
	}
	clk_co_ref = devm_clk_get(pDriverData->dev, "clk_co_ref");
	if (!clk_sysin) {
		dev_err(pDriverData->dev, "Error: get clock handle failed\n");
		return;
	}
	clk_co_ref_rate = clk_get_rate(clk_co_ref);
	dev_info(pDriverData->dev, "|-clk_co_ref_rate  = %lu Hz\n", clk_co_ref_rate);

	err = clk_add_alias("clk_co_pll1", NULL, "CLK_C0_PLL1", NULL);
	if (err) {
		dev_err(pDriverData->dev, "Error: add clock alias failed (%d)\n", err);
		return;
	}
	clk_co_pll1 = devm_clk_get(pDriverData->dev, "clk_co_pll1");
	if (!clk_sysin) {
		dev_err(pDriverData->dev, "Error: get clock handle failed\n");
		return;
	}
	clk_co_pll1_rate = clk_get_rate(clk_co_pll1);
	dev_info(pDriverData->dev, "|-clk_co_pll1_rate = %lu Hz\n", clk_co_pll1_rate);

	err = clk_add_alias("clk_hva", NULL, "CLK_HVA", NULL);
	if (err) {
		dev_err(pDriverData->dev, "Error: add clock alias failed (%d)\n", err);
		return;
	}
	clk_hva = devm_clk_get(pDriverData->dev, "clk_hva");
	if (!clk_sysin) {
		dev_err(pDriverData->dev, "Error: get clock handle failed\n");
		return;
	}
	clk_hva_rate = clk_get_rate(clk_hva);
	dev_info(pDriverData->dev, "|-clk_hva_rate     = %lu Hz\n", clk_hva_rate);
	dev_info(pDriverData->dev, "\n");
#endif
}

//function to enable HVA clock
static int HvaClkOn()
{
#ifdef CONFIG_STM_VIRTUAL_PLATFORM
	return 0;
#else

	struct clk *clk;
	unsigned long flags;
	int ret = 0;
	bool need_reinit = false;

	spin_lock_irqsave(&pDriverData->clk_lock, flags);
	clk = pDriverData->clk;
	ret = clk_enable(clk);
	if (ret) {
		spin_unlock_irqrestore(&pDriverData->clk_lock, flags);
		dev_err(pDriverData->dev, "Error: clock enable failed (%d)\n", ret);
		goto end_hvaclk;
	}
	pDriverData->clk_ref_count++;
	if (1 == pDriverData->clk_ref_count) {
		need_reinit = true;
	}
	spin_unlock_irqrestore(&pDriverData->clk_lock, flags);

	if (need_reinit) {
		DisplayClkValues();
		H264HwEncodeSetRegistersConfig();
	}

end_hvaclk:
	return ret;
#endif
}

//function to disable HVA clock
static void HvaClkOff()
{
#ifdef CONFIG_STM_VIRTUAL_PLATFORM

#else
	unsigned long flags;
	struct clk *clk;

	spin_lock_irqsave(&pDriverData->clk_lock, flags);
	clk = pDriverData->clk;
	if (pDriverData->clk_ref_count) {
		pDriverData->clk_ref_count--;
	}

	clk_disable(clk);
	spin_unlock_irqrestore(&pDriverData->clk_lock, flags);
#endif
}


#ifdef CONFIG_PM
// suspend callback, being called on Host Passive Standby entry (HPS entry)
static int HvaPmSuspend(struct device *dev)
{
	dev_info(pDriverData->dev, "Hva pm suspend\n");

	// Here we assume that there are no more jobs in progress.
	// This is ensured at SE level, which has already entered "low power state", in PM notifier callback of player2 module (on PM_SUSPEND_PREPARE event).
	// nothing to be done here, clk On/Off is managed dynamically at each task level
	return 0;
}

// resume callback, being called on Host Passive Standby exit (HPS exit)
static int HvaPmResume(struct device *dev)
{
	int ret;
	dev_info(pDriverData->dev, "Hva pm resume\n");

	ret = HvaClockSetRate(dev->of_node);
	if (ret) {
		dev_err(pDriverData->dev, "Error: setting maximum clock frequency failed (%d)\n", ret);
		return -EINVAL;
	}

	// Here we assume that no new jobs have been posted yet.
	// This is ensured at SE level, which will exit "low power state" later on, in PM notifier callback of player2 module (on PM_POST_SUSPEND event).
	// nothing to be done here, clk On/Off is managed dynamically at each task level
	return 0;
}

// freeze callback, being called on Controller Passive Standby entry (CPS entry)
static int HvaPmFreeze(struct device *dev)
{
	dev_info(pDriverData->dev, "Hva pm freeze\n");

	// Entering this API  we assume that there are no more jobs in progress.
	// This is ensured at SE level, which has already entered "low power state", in PM notifier callback of player2 module (on PM_HIBERNATION_PREPARE event).
	// nothing to be done here, clk On/Off is managed dynamically at each task level
	return 0;
}

// restore callback, being called on Controller Passive Standby exit (CPS exit)
static int HvaPmRestore(struct device *dev)
{
	int ret;
	dev_info(pDriverData->dev, "Hva pm restore\n");

	ret = HvaClockSetRate(dev->of_node);
	if (ret) {
		dev_err(pDriverData->dev, "Error: setting maximum clock frequency failed (%d)\n", ret);
		return -EINVAL;
	}

	// Entering this API we assume that no new jobs have been posted yet.
	// This is ensured at SE level, which will exit "low power state" later on, in PM notifier callback of player2 module (on PM_POST_HIBERNATION event).
	// nothing to be done here, clk On/Off is managed dynamically at each task level
	return 0;
}

#ifdef CONFIG_PM_RUNTIME
// runtime_suspend callback for Active Standby
static int HvaPmRuntimeSuspend(struct device *dev)
{
	dev_dbg(pDriverData->dev, "Hva pm runtime suspend\n");

	// Nothing to do, since HVA clocks are managed dynamically
	return 0;
}

// runtime_resume callback for Active Standby
static int HvaPmRuntimeResume(struct device *dev)
{
	dev_dbg(pDriverData->dev, "Hva pm runtime resume\n");

	// Nothing to do, since HVA clocks are managed dynamically
	return 0;
}
#endif // CONFIG_PM_RUNTIME
#endif // CONFIG_PM

//Store platform specific information in platformData
static int HvaGetPlatformInfo(struct platform_device *pdev)
{
	struct resource *pResource;

	if (pdev == NULL) {
		dev_err(pDriverData->dev, "Error: incorrect platform device pointer\n");
		return -EINVAL;
	}
	if (!pdev->name) {
		dev_err(pDriverData->dev, "Error: incorrect platform device name\n");
		return -EINVAL;
	}
	//retrieve HVA register base address from platform file
	pResource = platform_get_resource_byname(pdev, IORESOURCE_MEM, "hva_registers");
	if (pResource) {
		platformData.BaseAddress[0] = pResource->start;
		platformData.Size[0] = (pResource->end - pResource->start + 1) ;
		dev_dbg(pDriverData->dev, "Hva registers base address is 0x%x and size is 0x%x\n", platformData.BaseAddress[0],
		        platformData.Size[0]);
	} else {
		dev_err(pDriverData->dev, "Error: can't retrieve hva register base address and size\n");
		return -EINVAL;
	}

	//retrieve ESRAM base address from platform file
	pResource = platform_get_resource_byname(pdev, IORESOURCE_MEM, "hva_esram");
	if (pResource) {
		platformData.BaseAddress[1] = pResource->start;
		platformData.Size[1] = (pResource->end - pResource->start + 1) ;
		dev_dbg(pDriverData->dev, "Hva esram base address is 0x%x and size is 0x%x\n", platformData.BaseAddress[1],
		        platformData.Size[1]);
	} else {
		dev_err(pDriverData->dev, "Error: can't retrieve hva esram base address\n");
		return -EINVAL;
	}

	//retrieve HVA ITS_IRQ interrupt number from platform file
	platformData.Interrupt[0] = platform_get_irq_byname(pdev, "hva_its_irq");
	if (platformData.Interrupt[0]) {
		dev_dbg(pDriverData->dev, "Hva interrupt number is %d\n", platformData.Interrupt[0]);
	} else {
		dev_err(pDriverData->dev, "Error: can't retrieve hva status interrupt number\n");
		return -EINVAL;
	}

	//retrieve HVA ERR_IRQ interrupt number from platform file
	platformData.Interrupt[1] = platform_get_irq_byname(pdev, "hva_err_irq");
	if (platformData.Interrupt[1]) {
		dev_dbg(pDriverData->dev, "Hva error interrupt number is %d\n", platformData.Interrupt[1]);
	} else {
		dev_err(pDriverData->dev, "Error: can't retrieve hva error interrupt number\n");
		return -EINVAL;
	}

	// We must have a lock per HVA device.
	spin_lock_init(&platformData.hw_lock);

	return 0;
}

#ifdef CONFIG_OF
/**
 * Get platform data from Device Tree
 */
static int hva_get_of_pdata(struct platform_device *pdev,
                            struct HvaDriverData *pDriverData)
{
#ifndef CONFIG_ARCH_STI
	struct device_node *np = pdev->dev.of_node;
	const char *clkName;

	of_property_read_string_index(np, "st,dev_clk", 0, &clkName);
	//map clk virtual name to clock specific platform HW name
	clk_add_alias("clk_hva", NULL, (char *)clkName, NULL);
#endif
	return 0;
}
#else
int hva_get_of_pdata(struct platform_device *pdev,
                     struct HvaDriverData *pDriverData)
{
	return 0;
}
#endif //CONFIG_OF

// Power management callbacks
static struct notifier_block stm_pm_hva_notifier = {
	.notifier_call = stm_pm_hva_notifier_call,
};

static int RegisterHvaTransformer()
{
	MME_ERROR status;
	int ret = 0;

	status =  MME_RegisterTransformer(
	                  mmeName,
	                  abortCmd,
	                  getTransformerCapability,
	                  initTransformer,
	                  processCommand,
	                  termTransformer);

	if (status != MME_SUCCESS) {
		ret = -ENODEV;
	}
	return ret;
}

static int DeRegisterHvaTransformer()
{
	MME_ERROR status = MME_SUCCESS;
	int ret = 0;

	status = MME_DeregisterTransformer(mmeName);
	if (status != MME_SUCCESS) {
		ret = -ENODEV;
	}
	return ret;
}

// probe function called on module load
static int HvaProbe(struct platform_device *pdev)
{
	int ret = 0;

	dev_dbg(&pdev->dev, "Run hva probe function\n");

	//NB: Memory allocated with devm_kzalloc() is automatically freed on driver detach
	pDriverData = devm_kzalloc(&pdev->dev, sizeof(*pDriverData), GFP_KERNEL);
	if (!pDriverData) {
		dev_err(&pdev->dev, "Error: memory allocation failed\n");
		ret = -ENOMEM;
		goto fail_out;
	}

	// Save hva device pointer
	pDriverData->dev = &pdev->dev;

	ret = RegisterHvaTransformer();
	if (ret != 0) {
		dev_err(pDriverData->dev, "Error: RegisterHvaTransformer failed\n");
		goto fail_out;
	}

	//Retrieve platform specific parameters from platform file & store in platformData
	HvaGetPlatformInfo(pdev);

	//Store driver data: easy access then
	platform_set_drvdata(pdev, pDriverData);

	/* Check DT usage */
	if (pdev->dev.of_node) {
		ret = hva_get_of_pdata(pdev, pDriverData);
		if (ret) {
			dev_err(pDriverData->dev, "Error: hva probing device with device tree failed (%d)\n", ret);
			goto fail_hvaprobe;
		}
	} else {
		dev_err(pDriverData->dev, "Error: device tree not working\n");
		goto fail_hvaprobe;
	}

	spin_lock_init(&pDriverData->clk_lock);
	pDriverData->clk_ref_count = 0;
	pDriverData->spurious_irq_count = 0;
	pDriverData->max_freq = 0;
	if (of_property_read_u32(pdev->dev.of_node, "clock-frequency", &pDriverData->max_freq)) {
		dev_dbg(pDriverData->dev, "Maximum hva clock frequency not retrieved from DT\n");
	}

	HvaRegisterBase = (unsigned int)devm_ioremap_nocache(pDriverData->dev, platformData.BaseAddress[0],
	                                                     platformData.Size[0]);
	if (!HvaRegisterBase) {
		dev_err(pDriverData->dev, "Error: ioremap failed\n");
		ret = -EIO;
		goto fail_hvaprobe;
	}

	//clk_get() is a costly processing: interesting to perform it one shot & record clocks structure as device driver data
	pDriverData->clk = devm_clk_get(&pdev->dev, "clk_hva");
	if (IS_ERR(pDriverData->clk)) {
		ret = PTR_ERR(pDriverData->clk);
		dev_err(pDriverData->dev, "Error: get hva clock handle failed (%d)\n", ret);
		goto fail_hvaprobe;
	}

	ret = HvaClockSetRate(pdev->dev.of_node);
	if (ret) {
		dev_err(pDriverData->dev, "Error: setting maximum clock frequency failed (%d)\n", ret);
		goto fail_hvaprobe;
	}

	ret = clk_prepare(pDriverData->clk);
	if (ret) {
		dev_err(pDriverData->dev, "Error: prepare hva clock failed (%d)\n", ret);
		goto fail_hvaprobe;
	}

#ifdef CONFIG_PM_RUNTIME
	/* Clear the device's 'power.runtime_error' flag and set the device's runtime PM status to 'suspended' */
	pm_runtime_set_suspended(&pdev->dev);
	pm_suspend_ignore_children(&pdev->dev, 1);
	//decrement the device's 'power.disable_depth' field: if that field is equal to zero, the runtime PM helper functions can execute
	pm_runtime_enable(&pdev->dev);
#endif

	ret = ReadHvaVersion();
	if (ret) {
		goto hva_fail_unprep;
	}

	// Pass the initialisation stage to the h264_encode_hard layer
	ret = H264HwEncodeProbe(&pdev->dev);
	if (ret) {
		goto hva_fail_unprep;
	}

	// Register with PM notifiers (for HPS/CPS support)
	register_pm_notifier(&stm_pm_hva_notifier);

	OSDEV_Dump_MemCheckCounters(__func__);
	dev_info(pDriverData->dev, "%s probe done ok\n", MODULE_NAME);

	return 0;

hva_fail_unprep:
	clk_unprepare(pDriverData->clk);
#ifdef CONFIG_PM_RUNTIME
	pm_runtime_disable(&pdev->dev);
#endif
fail_hvaprobe:
	DeRegisterHvaTransformer();
fail_out:
	OSDEV_Dump_MemCheckCounters(__func__);
	dev_err(&pdev->dev, "Error: %s probe failed\n", MODULE_NAME);

	return ret;
}

// remove function called on module unload
static int HvaRemove(struct platform_device *pdev)
{
	dev_dbg(pDriverData->dev, "Run hva remove function\n");

	pDriverData = dev_get_drvdata(&pdev->dev);

	H264HwEncodeRemove(&pdev->dev);

	clk_unprepare(pDriverData->clk);

	unregister_pm_notifier(&stm_pm_hva_notifier);

#ifdef CONFIG_PM_RUNTIME
	pm_runtime_disable(&pdev->dev);
#endif

	// Remove HVA device pointer
	pDriverData->dev = 0;

	DeRegisterHvaTransformer();

	OSDEV_Dump_MemCheckCounters(__func__);
	pr_info("%s remove done\n", MODULE_NAME);

	return 0;
}

// Power management callbacks
#ifdef CONFIG_PM
static struct dev_pm_ops HvaPmOps = {
	.suspend = HvaPmSuspend,
	.resume  = HvaPmResume,
	.freeze  = HvaPmFreeze,
	.restore = HvaPmRestore,
#ifdef CONFIG_PM_RUNTIME
	.runtime_suspend  = HvaPmRuntimeSuspend,
	.runtime_resume   = HvaPmRuntimeResume,
#endif
};
#endif // CONFIG_PM

#ifdef CONFIG_OF
static struct of_device_id stm_hva_match[] = {
	{
		.compatible = "st,se-hva",
	},
	{},
};
//MODULE_DEVICE_TABLE(of, stm_hva_match);
#endif

static struct platform_driver HvaDriver = {
	.driver = {
		.name = "h264encoder",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(stm_hva_match),
#ifdef CONFIG_PM
		.pm = &HvaPmOps,
#endif
	},
	.probe = HvaProbe,
	.remove = HvaRemove,
};

module_platform_driver(HvaDriver);
