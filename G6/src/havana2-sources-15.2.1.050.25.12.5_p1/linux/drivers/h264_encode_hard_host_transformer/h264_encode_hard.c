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

#include <linux/of.h>
#include <linux/sysfs.h>
#include <linux/stat.h>
#include <linux/platform_device.h>
#include <linux/semaphore.h>

#include "osdev_time.h"
#include "osdev_cache.h"
#include "osdev_mem.h"
#include "osdev_int.h"

#include "hva_registers.h"
#include "h264_encode.h"
#include "h264_encode_platform.h"
#include "utilisation.h"

// Mme instance is limited to 32 by multicom
// Encode stream is limited to 6 instances for HLS use case
// As encode * encode stream = mme instances, encoder is limited to 5 instances
// => hva mme driver is limited to 30 instances
#define MAX_HVA_ENCODE_INSTANCE 30U

// HVA_COMMAND_TIMEOUT_MS is the maximum time to wait before aborting sending
// a new command to the HVA in the event that the CMD Fifo is full.
// An encode usually takes about 8 to 10 ms, so setting at 1 second
// is an arbitrarily large timeout that will only occur in event of the HVA
// becoming locked up.
#define HVA_COMMAND_TIMEOUT_MS                 (1000)

static DEFINE_MUTEX(H264EncoderGlobalLock);

struct semaphore                      h264EncInstances;
unsigned char                         h264EncInstanceNb = 0;

unsigned int HvaRegisterBase;

struct semaphore                      h264CmdFifoDepth;

//Record Global number of frame encoded: same format as HW taskId
static unsigned short                 globalTaskId              = 0;
//Manage list of clients connected and resource available
static H264EncodeHardCodecContext_t   *ClientContextList[MAX_HVA_ENCODE_INSTANCE];

//Temporary storage of HW states (during Interrupt handler) before storage in Context
static H264EncodeHard_InterruptError_t errorInterruptStatus     = H264ENCHARD_NO_HW_ERROR;

/* Mono instance buffer created at transformer init (no context to manage for those buffers)*/
/* SRAM buffer addresses */

static unsigned int searchWindowBufPhysAddress = 0;
static unsigned int localRecBufPhysAddress = 0;
static unsigned int contextMBBufPhysAddress = 0;
static unsigned int cabacContextBufPhysAddress = 0;

/*Pointer to manage free SRAM memory (no allocator here as HVA is only customer)*/
/* only written during first HW init: no concurrency on accesses) */
static unsigned int HvaSramBase = 0;
static unsigned int newHvaSramFreePhysAddress = 0;
static unsigned int HvaSramSize = 0;

extern HVAPlatformData_t platformData;
extern struct HvaDriverData *pDriverData;

#define SYSFS_DECLARE_UINT( VarName )                                                              \
static unsigned int VarName = 0;                                                                   \
static ssize_t SysfsShow_##VarName( struct device *dev, struct device_attribute *attr, char *buf ) \
{                                                                                                  \
    return sprintf(buf, "%d\n", VarName);                                                          \
}                                                                                                  \
                                                                                                   \
static DEVICE_ATTR(VarName, S_IRUGO, SysfsShow_##VarName, NULL)

// For displaying values in Semaphores. For monitoring and Debug use only
#define SYSFS_DECLARE_SEMAPHORE( VarName )                                                         \
static ssize_t SysfsShow_##VarName( struct device *dev, struct device_attribute *attr, char *buf ) \
{                                                                                                  \
    unsigned int count;                                                                            \
    unsigned long flags;                                                                           \
                                                                                                   \
    raw_spin_lock_irqsave( &VarName.lock, flags );                                                \
    count = VarName.count;                                                                        \
    raw_spin_unlock_irqrestore( &VarName.lock, flags );                                           \
                                                                                                   \
    return sprintf(buf, "%d\n", count);                                                            \
}                                                                                                  \
                                                                                                   \
static DEVICE_ATTR(VarName, S_IRUGO, SysfsShow_##VarName, NULL)


SYSFS_DECLARE_UINT(h264_frames_queued);
SYSFS_DECLARE_UINT(h264_frames_completed);
SYSFS_DECLARE_UINT(hw_encode_duration);
SYSFS_DECLARE_UINT(cpu_encode_duration);
SYSFS_DECLARE_UINT(WakeUpLatency);
SYSFS_DECLARE_UINT(WakeUpLatencyAvg);
SYSFS_DECLARE_UINT(WakeUpLatencyMax);

SYSFS_DECLARE_UTILISATION(hw_utilisation);
SYSFS_DECLARE_UTILISATION(sw_utilisation);

SYSFS_DECLARE_SEMAPHORE(h264EncInstances);
SYSFS_DECLARE_SEMAPHORE(h264CmdFifoDepth);

int H264HwEncodeProbe(struct device *dev)
{
	unsigned int maxInstance = UINT_MAX;
	int result;

	if (dev->of_node) {
		if (of_property_read_u32(dev->of_node, "st,max_enc", &maxInstance)) {
			dev_dbg(pDriverData->dev, "st,max_enc property not found => no restriction on the number of encode instance\n");
			maxInstance = UINT_MAX;
		}
	} else {
		dev_err(pDriverData->dev, "Error: device tree not working\n");
		return -1;
	}

	//create the attribute files
	result = device_create_file(dev, &dev_attr_h264_frames_queued);
	if (result) {
		dev_err(pDriverData->dev, "Error: device_create_file failed for h264_frames_queued (%d)\n", result);
		goto devcreate_bail;
	}

	result = device_create_file(dev, &dev_attr_h264_frames_completed);
	if (result) {
		dev_err(pDriverData->dev, "Error: device_create_file failed for h264_frames_completed (%d)\n", result);
		goto devcreate_fail1;
	}

	result = device_create_file(dev, &dev_attr_hw_encode_duration);
	if (result) {
		dev_err(pDriverData->dev, "Error: device_create_file failed for hw_encode_duration (%d)\n", result);
		goto devcreate_fail2;
	}

	result = device_create_file(dev, &dev_attr_cpu_encode_duration);
	if (result) {
		dev_err(pDriverData->dev, "Error: device_create_file failed for cpu_encode_duration (%d)\n", result);
		goto devcreate_fail3;
	}

	result = device_create_file(dev, &dev_attr_WakeUpLatency);
	if (result) {
		dev_err(pDriverData->dev, "Error: device_create_file failed for WakeUpLatency (%d)\n", result);
		goto devcreate_fail4;
	}

	result = device_create_file(dev, &dev_attr_WakeUpLatencyAvg);
	if (result) {
		dev_err(pDriverData->dev, "Error: device_create_file failed for WakeUpLatencyAvg (%d)\n", result);
		goto devcreate_fail5;
	}

	result = device_create_file(dev, &dev_attr_WakeUpLatencyMax);
	if (result) {
		dev_err(pDriverData->dev, "Error: device_create_file failed for WakeUpLatencyMax (%d)\n", result);
		goto devcreate_fail6;
	}
	result = device_create_file(dev, &dev_attr_hw_utilisation);
	if (result) {
		dev_err(pDriverData->dev, "Error: device_create_file failed for hw_utilisation (%d)\n", result);
		goto devcreate_fail7;
	}
	result = device_create_file(dev, &dev_attr_sw_utilisation);
	if (result) {
		dev_err(pDriverData->dev, "Error: device_create_file failed for sw_utilisation (%d)\n", result);
		goto devcreate_fail8;
	}

	maxInstance = min(MAX_HVA_ENCODE_INSTANCE, maxInstance);
	dev_info(pDriverData->dev, "Setting hva max instance number to %d\n", maxInstance);
	sema_init(&h264EncInstances, maxInstance);

	sema_init(&h264CmdFifoDepth, HVA_CMD_FIFO_DEPTH);

	// These must be created AFTER the semaphores are initialized
	result = device_create_file(dev, &dev_attr_h264EncInstances);
	if (result) {
		dev_err(pDriverData->dev, "Error: device_create_file failed for h264EncInstances (%d)\n", result);
		goto devcreate_fail9;
	}
	result = device_create_file(dev, &dev_attr_h264CmdFifoDepth);
	if (result) {
		dev_err(pDriverData->dev, "Error: device_create_file failed for h264CmdFifoDepth (%d)\n", result);
		goto devcreate_fail10;
	}

	goto devcreate_bail;

devcreate_fail10:
	device_remove_file(dev, &dev_attr_h264EncInstances);
devcreate_fail9:
	device_remove_file(dev, &dev_attr_sw_utilisation);
devcreate_fail8:
	device_remove_file(dev, &dev_attr_hw_utilisation);
devcreate_fail7:
	device_remove_file(dev, &dev_attr_WakeUpLatencyMax);
devcreate_fail6:
	device_remove_file(dev, &dev_attr_WakeUpLatencyAvg);
devcreate_fail5:
	device_remove_file(dev, &dev_attr_WakeUpLatency);
devcreate_fail4:
	device_remove_file(dev, &dev_attr_cpu_encode_duration);
devcreate_fail3:
	device_remove_file(dev, &dev_attr_hw_encode_duration);
devcreate_fail2:
	device_remove_file(dev, &dev_attr_h264_frames_completed);
devcreate_fail1:
	device_remove_file(dev, &dev_attr_h264_frames_queued);
devcreate_bail:
	return result;
}

void H264HwEncodeRemove(struct device *dev)
{
	device_remove_file(dev, &dev_attr_h264CmdFifoDepth);
	device_remove_file(dev, &dev_attr_h264EncInstances);

	device_remove_file(dev, &dev_attr_hw_utilisation);
	device_remove_file(dev, &dev_attr_sw_utilisation);

	//remove the attribute files
	device_remove_file(dev, &dev_attr_h264_frames_queued);
	device_remove_file(dev, &dev_attr_h264_frames_completed);
	device_remove_file(dev, &dev_attr_hw_encode_duration);
	device_remove_file(dev, &dev_attr_cpu_encode_duration);
	device_remove_file(dev, &dev_attr_WakeUpLatency);
	device_remove_file(dev, &dev_attr_WakeUpLatencyAvg);
	device_remove_file(dev, &dev_attr_WakeUpLatencyMax);
}

#define LA_SAMPLES 240 // Number of latency samples to average. Time period = LA_SAMPLES/number of frames encoded per sec
static unsigned long long LatencyTotal = 0;
static unsigned int LatencyFrame = 0;


// ////////////////////////////////////////////////////////////////////////////////
//
//    Prototypes

static OSDEV_InterruptHandlerEntrypoint(H264HwEncodeInterruptHandler);
static OSDEV_InterruptHandlerEntrypoint(H264HwEncodeErrorInterruptHandler);

static void H264HwEncodeLoadStaticParameters(H264EncodeHardCodecContext_t  *Context);

static H264EncodeHardStatus_t H264HwEncodeInterruptInstall(void);
static void                   H264HwEncodeInterruptUnInstall(void);
static void H264HwEncodeCompletionHandler(struct work_struct *work);
static H264EncodeHardStatus_t H264HwEncodeAllocateSingletonHwBuffers(void);
static void                   H264HwEncodeFreeSingletonHwBuffers(void);
static uint32_t H264HwEncodeParamOutSize(uint32_t MaxFrameWidth, uint32_t MaxFrameHeight);
static H264EncodeHardStatus_t H264HwEncodeAllocateContextHwBuffers(H264EncodeHardCodecContext_t  *Context ,
                                                                   uint32_t MaxFrameWidth, uint32_t MaxFrameHeight);
static void                   H264HwEncodeFreeContextHwBuffers(H264EncodeHardCodecContext_t  *Context);
static void H264HwEncodeLoadStreamContext(H264EncodeHardCodecContext_t  *Context);
static void H264HwEncodeDisplayTaskDescriptor(H264EncodeHardTaskDescriptor_t  *pTaskDescriptor, uint8_t clientId);
static unsigned int H264HwEncodeBuildHVACommand(HVACmdType_t cmdType, unsigned char clientId, unsigned short taskId);
static H264EncodeHardStatus_t H264HwEncodeLaunchEncodeTask(H264EncodeHardCodecContext_t  *Context);
static H264EncodeHardStatus_t H264HwEncodeCompletion(H264EncodeHardCodecContext_t *Context);
static void H264HwEncodeUpdateContextAtEncodeEnd(H264EncodeHardCodecContext_t  *Context);
static void H264HwEncodeHVAReset(void);
static void H264HwEncodeHVAStaticConfig(void);
static void H264HwEncodeCheckFramerate(H264EncodeHardCodecContext_t  *Context);
static void H264HwEncodeCheckBitrateAndCpbSize(H264EncodeHardCodecContext_t  *Context);
static void H264HwEncodeGetFrameDimensionFromProfile(H264EncodeMemoryProfile_t memoryProfile, uint32_t *maxFrameWidth,
                                                     uint32_t *maxFrameHeight);

#ifndef CONFIG_STM_VIRTUAL_PLATFORM /* VSOC WORKAROUND : inlining not currently supported in VSOC environment */
inline unsigned int HVA_ALIGNED_256(unsigned int address);
inline unsigned int HVA_SET_BUFFER_AS_SDRAM(unsigned int address);
inline unsigned int HVA_SET_BUFFER_AS_ESRAM(unsigned int address);
#else
unsigned int HVA_ALIGNED_256(unsigned int address);
unsigned int HVA_SET_BUFFER_AS_SDRAM(unsigned int address);
unsigned int HVA_SET_BUFFER_AS_ESRAM(unsigned int address);
#endif

//{{{  H264HwEncodeInit
//{{{  doxynote
/// \brief      Initialize HVA HW & allocate requested internal buffers
///
/// \return     H264EncodeHardStatus_t
//}}}
H264EncodeHardStatus_t H264HwEncodeInit(H264EncodeHardInitParams_t    *InitParams,
                                        H264EncodeHardHandle_t        *Handle)
{
	H264EncodeHardCodecContext_t  *Context;
	H264EncodeHardStatus_t status = H264ENCHARD_NO_ERROR;
	uint32_t i;
	uint8_t clientId;
	uint32_t maxFrameWidth;
	uint32_t maxFrameHeight;

	dev_dbg(pDriverData->dev, "Hva encode init\n");

	mutex_lock(&H264EncoderGlobalLock);

	if (down_timeout(&h264EncInstances, msecs_to_jiffies(1000)) != 0) {
		dev_err(pDriverData->dev, "Error: max hva encoder client reached\n");
		status = H264ENCHARD_ERROR;
		goto endinit;
	}

	//Retrieve frame max dimension from Profile
	H264HwEncodeGetFrameDimensionFromProfile(InitParams->MemoryProfile, &maxFrameWidth, &maxFrameHeight);

	//perform "one shot" init like HW init if required
	if (h264EncInstanceNb == 0) {
		dev_dbg(pDriverData->dev, "Hva esram physical address is 0x%x and size is %u bytes\n", platformData.BaseAddress[1],
		        platformData.Size[1]);
		HvaSramBase = platformData.BaseAddress[1];
		newHvaSramFreePhysAddress = platformData.BaseAddress[1];
		HvaSramSize = platformData.Size[1];

		// Install interrupt handler
		status = H264HwEncodeInterruptInstall();
		if (status != H264ENCHARD_NO_ERROR) {
			dev_err(pDriverData->dev, "Error: installation of interrupt handler failed\n");
			goto fail_inth;
		}

		// Allocate mono instantiated buffers (no context required)
		// Physically contiguous Addresses required!!!
		status = H264HwEncodeAllocateSingletonHwBuffers();
		if (status != H264ENCHARD_NO_ERROR) {
			dev_err(pDriverData->dev, "Error: allocation of singleton buffers failed\n");
			goto fail_allocateshw;
		}

		// initialize client context to NULL
		for (i = 0; i < MAX_HVA_ENCODE_INSTANCE; i++) {
			ClientContextList[i] = NULL;
		}
	} else { dev_dbg(pDriverData->dev, "Hva encode already initialized\n"); }

	for (clientId = 0; clientId < MAX_HVA_ENCODE_INSTANCE; clientId++) {
		if (ClientContextList[clientId] == NULL) {
			break;
		}
	}

	// Check for error condition
	if (clientId == MAX_HVA_ENCODE_INSTANCE) {
		dev_err(pDriverData->dev, "Error: unable to find free client id\n");
		status = H264ENCHARD_ERROR;
		goto endinitup;  // assuming that in this case h264EncInstanceNb != 0
	}

	// Obtain an encoder context
	Context     = (H264EncodeHardCodecContext_t *)OSDEV_Malloc(sizeof(H264EncodeHardCodecContext_t));

	if (Context == NULL) {
		dev_err(pDriverData->dev, "Error: memory allocation for context structure failed\n");
		status = H264ENCHARD_NO_SDRAM_MEMORY;
		if (h264EncInstanceNb != 0) {
			goto endinitup;
		} else {
			goto fail_instance;
		}
	}

	// Initialize encoder context
	memset(Context, 0, sizeof(H264EncodeHardCodecContext_t));
	*Handle     = (void *)Context;

	ClientContextList[clientId] = Context;
	Context->clientId           = clientId;

	// Initialise the CompletionHandler
	INIT_WORK(&Context->CompletionWork, H264HwEncodeCompletionHandler);

	//Allocate multi instance internal buffers: 1 set per encode instance
	//Design choice to allocate the max size for internal buffers at init, bypassing need to reallocate for each size update
	status = H264HwEncodeAllocateContextHwBuffers(Context, maxFrameWidth, maxFrameHeight);
	if (status != H264ENCHARD_NO_ERROR) {
		dev_err(pDriverData->dev, "Error: allocation of multi instance buffers failed\n");
		OSDEV_Free(Context);
		ClientContextList[clientId] = NULL;
		if (h264EncInstanceNb != 0) {
			goto endinitup;
		} else {
			goto fail_instance;
		}
	}

	//Fill task descriptor static parameters
	H264HwEncodeLoadStaticParameters(Context);

	h264EncInstanceNb++;

	//Initialize past bitstream size
	for (i = 0; i < 4; i++) {
		Context->PastBitstreamSize[i] = 0;
	}

	goto endinit;

fail_instance:
	H264HwEncodeFreeSingletonHwBuffers();
fail_allocateshw:
	H264HwEncodeInterruptUnInstall();
fail_inth:
endinitup:
	up(&h264EncInstances);

endinit:
	mutex_unlock(&H264EncoderGlobalLock);

	return status;
}

//{{{  H264HwEncodeSetSequenceParams
//{{{  doxynote
/// \brief      Provide sequence related encode parameters
///
/// \return     H264EncodeHardStatus_t
//}}}
H264EncodeHardStatus_t H264HwEncodeSetSequenceParams(
        H264EncodeHardHandle_t                 Handle,
        H264EncodeHardSequenceParams_t        *SequenceParams)
{

	H264EncodeHardCodecContext_t  *Context = (H264EncodeHardCodecContext_t *)Handle;
	H264EncodeNVCLContext_t       *nVCLContext = &Context->nVCLContext;

	memcpy(&(Context->globalParameters), SequenceParams, sizeof(H264EncodeHardSequenceParams_t));

	// Initialize reference frame rate as set sequence params
	Context->framerateNumRef = Context->globalParameters.framerateNum;
	Context->framerateDenRef = Context->globalParameters.framerateDen;
	// Initialize reference bitrate as set sequence params, will be used in future
	Context->bitrateRef      = Context->globalParameters.bitRate;
	Context->cpbBufferSizeRef = Context->globalParameters.cpbBufferSize;
	H264HwEncodeCheckBitrateAndCpbSize(Context);

	dev_dbg(pDriverData->dev, "[Sequence parameters]\n");
	dev_dbg(pDriverData->dev, "|-frameWidth              = %u pixels\n", Context->globalParameters.frameWidth);
	dev_dbg(pDriverData->dev, "|-frameHeight             = %u pixels\n", Context->globalParameters.frameHeight);
	dev_dbg(pDriverData->dev, "|-log2MaxFrameNumMinus4   = %u\n", Context->globalParameters.log2MaxFrameNumMinus4);
	dev_dbg(pDriverData->dev, "|-useConstrainedIntraFlag = %u\n", Context->globalParameters.useConstrainedIntraFlag);
	dev_dbg(pDriverData->dev, "|-MaxSumNumBitsInNALU     = %u bits\n", Context->globalParameters.maxSumNumBitsInNALU);
	dev_dbg(pDriverData->dev, "|-intraRefreshType        = %u (%s)\n", Context->globalParameters.intraRefreshType,
	        StringifyIntraRefreshType(Context->globalParameters.intraRefreshType));
	if (Context->globalParameters.irParamOption == HVA_ENCODE_DISABLE_INTRA_REFRESH) {
		dev_dbg(pDriverData->dev, "|-irParamOption           = %u\n", Context->globalParameters.irParamOption);
	} else {
		dev_dbg(pDriverData->dev, "|-irParamOption           = %u %s\n", Context->globalParameters.irParamOption,
		        (Context->globalParameters.intraRefreshType == HVA_ENCODE_ADAPTIVE_INTRA_REFRESH) ? "macroblocks/frame refreshed" :
		        "frames (refresh period)");
	}
	dev_dbg(pDriverData->dev, "|-brcType                 = %u (%s)\n", Context->globalParameters.brcType,
	        StringifyBrcType(Context->globalParameters.brcType));
	dev_dbg(pDriverData->dev, "|-bitRate                 = %u bps\n", Context->globalParameters.bitRate);
	dev_dbg(pDriverData->dev, "|-cpbBufferSize           = %u bits\n", Context->globalParameters.cpbBufferSize);
	dev_dbg(pDriverData->dev, "|-framerateNum            = %u\n", Context->globalParameters.framerateNum);
	dev_dbg(pDriverData->dev, "|-framerateDen            = %u\n", Context->globalParameters.framerateDen);
	dev_dbg(pDriverData->dev, "|-TransformMode           = %u (%s)\n", Context->globalParameters.transformMode,
	        StringifyTransformMode(Context->globalParameters.transformMode));
	dev_dbg(pDriverData->dev, "|-encoderComplexity       = %u (%s)\n", Context->globalParameters.encoderComplexity,
	        StringifyEncoderComplexity(Context->globalParameters.encoderComplexity));
	dev_dbg(pDriverData->dev, "|-samplingMode            = %u (%s)\n", Context->globalParameters.samplingMode,
	        StringifySamplingMode(Context->globalParameters.samplingMode));
	dev_dbg(pDriverData->dev, "|-delay                   = %u ms\n", Context->globalParameters.delay);
	dev_dbg(pDriverData->dev, "|-QPmin                   = %u\n", Context->globalParameters.qpmin);
	dev_dbg(pDriverData->dev, "|-QPmax                   = %u\n", Context->globalParameters.qpmax);
	dev_dbg(pDriverData->dev, "\n");

	// Initialize vcl context parameter
	Context->mvToggle = 1;
	Context->lastIdrPicId = -1;
	Context->idrToggle = 1;

	H264HwEncodeInitParamsNAL(nVCLContext, SequenceParams, true);

	// Reload the StreamContext values.
	H264HwEncodeLoadStreamContext(Context);

	return H264ENCHARD_NO_ERROR;
}


//{{{  H264HwEncodeLoadStreamContext
//{{{  doxynote
/// \brief      Restore HVA task descriptor with a managed context
///
//}}}
static void H264HwEncodeLoadStreamContext(H264EncodeHardCodecContext_t  *Context)
{

	H264EncodeHardTaskDescriptor_t *pTaskDescriptor = (H264EncodeHardTaskDescriptor_t *)Context->taskDescriptorHandler;

	dev_dbg(pDriverData->dev, "Reload stream context for client %d\n", Context->clientId);

	//No specific HW registers to set but task descriptor vs global sequence parameters & static parameters
	pTaskDescriptor->frameWidth              = (((Context->globalParameters.frameWidth + 15) >> 4) << 4) &
	                                           FRAME_DIMENSION_BITS_MASK;
	pTaskDescriptor->frameHeight             = (((Context->globalParameters.frameHeight + 15) >> 4) << 4) &
	                                           FRAME_DIMENSION_BITS_MASK;
	pTaskDescriptor->windowWidth             = pTaskDescriptor->frameWidth;
	pTaskDescriptor->windowHeight            = pTaskDescriptor->frameHeight;
	pTaskDescriptor->windowHorizontalOffset  = 0;
	pTaskDescriptor->windowVerticalOffset    = 0;
	/*pTaskDescriptor->pictureCodingType is frame parameter
	  pTaskDescriptor->idrFlag is not used by hva
	  pTaskDescriptor->frameNum is not used by hva */
	pTaskDescriptor->picOrderCntType         = Context->globalParameters.picOrderCntType & PIC_ORDER_CNT_BITS_MASK;
	/*pTaskDescriptor->FirstPictureInSequence is frame parameter */
	pTaskDescriptor->useConstrainedIntraFlag = Context->globalParameters.useConstrainedIntraFlag & INTRA_FLAG_BITS_MASK;
	//FIXME: slice management to clarify
	//pTaskDescriptor->sliceSizeType           = HVA_ENCODE_SLICE_BYTE_SIZE_LIMIT; /*Fixed value and use sliceByteSize*/
	//pTaskDescriptor->sliceByteSize           = HVA_ENCODE_RECOMMENDED_SLICE_SIZE;
	pTaskDescriptor->sliceSizeType           = HVA_ENCODE_NO_CONSTRAINT_TO_CLOSE_SLICE;
	//Number of MB per slice not used with HVA_ENCODE_SLICE_BYTE_SIZE_LIMIT sliceSizeType
	/*pTaskDescriptor->sliceMbSize             = (((pTaskDescriptor->frameWidth / 16) * (pTaskDescriptor->frameHeight / 16)) / Context->globalParameters.sliceNumber) & SLICE_MB_SIZE_BITS_MASK;*/
	pTaskDescriptor->intraRefreshType        = Context->globalParameters.intraRefreshType;
	pTaskDescriptor->irParamOption           = Context->globalParameters.irParamOption & IR_PARAM_BITS_MASK;
	/*pTaskDescriptor->disableDeblockingFilterIdc
	pTaskDescriptor->sliceAlphaC0OffsetDiv2
	pTaskDescriptor->sliceBetaOffsetDiv2 are frame parameters */
	pTaskDescriptor->brcType                 = Context->globalParameters.brcType;
	/*pTaskDescriptor->lastBPAUts
	pTaskDescriptor->NALfinalArrivalTime are not used by hva */
	//pTaskDescriptor->nonVCLNALUSize is frame parameter
	pTaskDescriptor->cpbBufferSize           = Context->globalParameters.cpbBufferSize;
	pTaskDescriptor->bitRate                 = Context->globalParameters.bitRate;
	//pTaskDescriptor->timestamp is not used by hva
	pTaskDescriptor->samplingMode            = Context->globalParameters.samplingMode;
	pTaskDescriptor->transformMode           = Context->globalParameters.transformMode;
	pTaskDescriptor->encoderComplexity       = Context->globalParameters.encoderComplexity;
	pTaskDescriptor->quant                   = Context->globalParameters.quant;
	H264HwEncodeCheckFramerate(Context);
	pTaskDescriptor->delay                   = Context->globalParameters.delay;
	pTaskDescriptor->strictHRDCompliancy     = HVA_ENCODE_USE_STRICT_HRD_COMPLIANCY;
	pTaskDescriptor->qpmin                   = Context->globalParameters.qpmin & QP_BITS_MASK;
	pTaskDescriptor->qpmax                   = Context->globalParameters.qpmax & QP_BITS_MASK;

	//Buffers addresses: only deal here with instance static buffers (full static programmed at task desc creation and frame related at encode frame stage)
	//WARNING: Reference and Reconstructed addresses have to be swapped after each frame encoding as REC(N) will be REF(N+1)

	/* pTaskDescriptor->addrSourceBuffer is a frame parameter */
	pTaskDescriptor->addrFwdRefBuffer        = Context->referenceFrameBufPhysAddress;
	pTaskDescriptor->addrRecBuffer           = Context->reconstructedFrameBufPhysAddress;
	/*pTaskDescriptor->addrOutputBitstreamStart
	  pTaskDescriptor->addrOutputBitstreamEnd
	  pTaskDescriptor->bitstreamOffset              are frame parameters */
	/*pTaskDescriptor->addrLctx                     is static parameter */
	pTaskDescriptor->addrParamInout          = Context->paramOutPhysAddress;
	/*pTaskDescriptor->addrExternalSw
	  pTaskDescriptor->addrLocalRecBuffer
	  pTaskDescriptor->addrExternalCwi              are static params */
	//Swap spatial and temporal context (replace mvToggle)
	if (Context->mvToggle == 0) {
		pTaskDescriptor->addrSpatialContext = Context->spatialContextBufPhysAddress;
		pTaskDescriptor->addrTemporalContext = Context->temporalContextBufPhysAddress;
	} else {
		pTaskDescriptor->addrSpatialContext = Context->temporalContextBufPhysAddress;
		pTaskDescriptor->addrTemporalContext = Context->spatialContextBufPhysAddress;
	}

	pTaskDescriptor->addrBrcInOutParameter  = Context->brcInOutBufPhysAddress;
	pTaskDescriptor->addrSliceHeader = Context->sliceHeaderBufPhysAddress;
	pTaskDescriptor->maxSliceNumber = 1;
	pTaskDescriptor->brcNoSkip = 0;

	//From here, instance's task descriptor is in line with latest provided global parameters for a given MME instance AND current context
	//Only Frame related parameters missing

	//Flush cache to ensure task descriptor consistency in memory
	OSDEV_FlushCacheRange((void *) pTaskDescriptor, Context->taskDescriptorPhysAddress,
	                      sizeof(H264EncodeHardTaskDescriptor_t));
}

// ////////////////////////////////////////////////////////////////////////////////
//
//    Local function to load static context: update related instance task descriptor

static void H264HwEncodeLoadStaticParameters(H264EncodeHardCodecContext_t  *Context)
{

	H264EncodeHardTaskDescriptor_t *pTaskDescriptor = (H264EncodeHardTaskDescriptor_t *)Context->taskDescriptorHandler;

	//Full static params: static among MME instance
	pTaskDescriptor->addrExternalSw         = searchWindowBufPhysAddress;
	pTaskDescriptor->addrLocalRecBuffer     = localRecBufPhysAddress;
	pTaskDescriptor->addrLctx               = contextMBBufPhysAddress;
	pTaskDescriptor->addrCabacContextBuffer     = cabacContextBufPhysAddress;

	//Flush cache to ensure task descriptor consistency in memory
	OSDEV_FlushCacheRange((void *) pTaskDescriptor, Context->taskDescriptorPhysAddress,
	                      sizeof(H264EncodeHardTaskDescriptor_t));

}

//{{{  H264HwEncodeLoadStreamContext
//{{{  doxynote
/// \brief      Encode frame function
///
/// \return     H264EncodeHardStatus_t
//}}}
H264EncodeHardStatus_t H264HwEncodeEncodeFrame(H264EncodeHardHandle_t                 Handle,
                                               H264EncodeHardFrameParams_t           *FrameParams)
{
	H264EncodeHardCodecContext_t  *Context = (H264EncodeHardCodecContext_t *)Handle;
	H264EncodeHardTaskDescriptor_t *pTaskDescriptor = (H264EncodeHardTaskDescriptor_t *)Context->taskDescriptorHandler;
	volatile H264EncodeHardParamOut_t *paramOut = (H264EncodeHardParamOut_t *) Context->paramOutHandler;
	FLEXIBLE_SLICE_HEADER flexible_slice_header;
	H264EncodeHardSequenceParams_t *SequenceParams = &Context->globalParameters;
	H264EncodeNVCLContext_t *nVCLContext = &Context->nVCLContext;
	ESBufferContext_t *ESContext = &Context->ESContext;
	uint8_t *streamBuf_p;
	uint32_t addrOutputBitstreamStartByteAlign;
	uint32_t addrOutputBitstreamStart;
	uint32_t addrOutputBitstreamEnd;
	uint32_t bitstreamOffset;
	uint32_t estSEISize;
	uint32_t ESSize;
	bool isNVclPresent;

	dev_dbg(pDriverData->dev, "Encode frame for client %d\n", Context->clientId);

	// We need to store a copy of the FrameParams for use by the Completion Context.
	Context->FrameParams = *FrameParams;

	// Fill the StreamContext descriptor and flush cache ready for hardware access.
	H264HwEncodeLoadStreamContext(Context);

	H264HwEncodeCheckFramerate(Context);

	/* NVCL NALU Generation */
	estSEISize                = 0;
	ESContext->offset         = 0;
	ESContext->overEstSEISize = 0;

	H264HwEncodeInitParamsNAL(nVCLContext, SequenceParams, false);
	ESSize = ((int32_t)FrameParams->addrOutputBitstreamEnd - (int32_t)FrameParams->addrOutputBitstreamStart);
	/* Map physical to virtual address */
	ESContext->ESStart_p = (uint8_t *)ioremap_cache(FrameParams->addrOutputBitstreamStart, ESSize);
	if (ESContext->ESStart_p == NULL) {
		dev_err(pDriverData->dev,
		        "Error: output bitstream physical address (0x%08x) cannot be re-mapped to virtual address (%p)\n",
		        FrameParams->addrOutputBitstreamStart, ESContext->ESStart_p);
		return H264ENCHARD_ERROR;
	}

	ESContext->offset += FrameParams->bitstreamOffset;
#if (ENABLE_H264ENC_ACCESS_UNIT_DELIMITER == 1)
	streamBuf_p  = ESContext->ESStart_p + (ESContext->offset >> 3);
	ESContext->offset += H264HwEncodeCreateAccessUnitDelimiterNAL(nVCLContext, FrameParams, streamBuf_p);
#endif
#if (ENABLE_H264ENC_SPS_PPS_AT_EACH_IDR == 1)
	isNVclPresent = (FrameParams->idrFlag == 1) || (FrameParams->firstPictureInSequence == 1);
#else
	isNVclPresent = (FrameParams->firstPictureInSequence == 1);
#endif
	if (isNVclPresent) {
		streamBuf_p  = ESContext->ESStart_p + (ESContext->offset >> 3);
		ESContext->offset += H264HwEncodeCreateSeqParamSetNAL(nVCLContext, SequenceParams, streamBuf_p);

		streamBuf_p  = ESContext->ESStart_p + (ESContext->offset >> 3);
		ESContext->offset += H264HwEncodeCreatePicParamSetNAL(nVCLContext, FrameParams, streamBuf_p);
	}
	if (SequenceParams->brcType != HVA_ENCODE_NO_BRC &&
	    (SequenceParams->seiBufPeriodPresentFlag || SequenceParams->seiPicTimingPresentFlag)) {
		/* We need to estimate the SEI Size because the timing parameters are only available after encoding */
		estSEISize = H264HwEncodeEstimateSEISize(nVCLContext, FrameParams);
		/* We overestimate SEI size for emulation start code prediction *3/8 */
		ESContext->overEstSEISize  = estSEISize + ((estSEISize * 3) >> 3);
	}

	Context->mvToggle = !Context->mvToggle;
	dev_dbg(pDriverData->dev, "mvToggle = %u => %u\n", !Context->mvToggle, Context->mvToggle);

	Context->idrToggle = !Context->idrToggle;
	if ((FrameParams->pictureCodingType == HVA_ENCODE_I_FRAME) && (FrameParams->idrFlag == 1) &&
	    (Context->lastIdrPicId == Context->idrToggle)) {
		Context->idrToggle = !Context->idrToggle;
	} else {
		dev_dbg(pDriverData->dev, "idrToggle = %u => %u\n", !Context->idrToggle, Context->idrToggle);
	}

	memset(&flexible_slice_header, 0, sizeof(FLEXIBLE_SLICE_HEADER));
	flexible_slice_header.frame_num = FrameParams->frameNum;
	flexible_slice_header.buffer = (uint8_t *)Context->sliceHeaderBufHandler;
	FrameParams->idrPicId = Context->idrToggle;
	pTaskDescriptor->SliceHeaderSizeInBits = H264HwEncodeFillFlexibleSliceHeader(&flexible_slice_header, FrameParams,
	                                                                             nVCLContext);
	pTaskDescriptor->SliceHeaderOffset0 = flexible_slice_header.offset0_bitsize;
	pTaskDescriptor->SliceHeaderOffset1 = flexible_slice_header.offset1_bitsize;;
	pTaskDescriptor->SliceHeaderOffset2 = 0;
	pTaskDescriptor->entropyCodingMode = nVCLContext->entropyCodingMode & ENTROPY_CODING_MODE_MASK;
	pTaskDescriptor->chromaQpIndexOffset = nVCLContext->chromaQpIndexOffset & CHROMA_QP_INDEX_OFFSET_MASK;

	/* update address for HVA to write */
	/* If we overestimate the SEI size to provide for emulation 3 bytes, we can shift the VCL NALU */
	addrOutputBitstreamStartByteAlign = FrameParams->addrOutputBitstreamStart + ((ESContext->offset +
	                                                                              ESContext->overEstSEISize) >> 3);
	addrOutputBitstreamStart          = (addrOutputBitstreamStartByteAlign & 0xfffffff0); // 128 bit alignment
	bitstreamOffset                   = (addrOutputBitstreamStartByteAlign & 0xF) << 3; /* offset in bits */
	addrOutputBitstreamEnd            = FrameParams->addrOutputBitstreamEnd;//addrOutputBitstreamStart + ESSize;

	//Fill task descriptor with suitable Frame parameters
	pTaskDescriptor->pictureCodingType           = FrameParams->pictureCodingType           & PICTURE_CODING_TYPE_MASK;
	pTaskDescriptor->firstPictureInSequence      = FrameParams->firstPictureInSequence      & FIRST_PICT_MASK;
	pTaskDescriptor->disableDeblockingFilterIdc  = FrameParams->disableDeblockingFilterIdc  & DISABLE_DEBLOCKING_MASK;
	pTaskDescriptor->sliceAlphaC0OffsetDiv2      = FrameParams->sliceAlphaC0OffsetDiv2      & SLICE_ALPHA_MASK;
	pTaskDescriptor->sliceBetaOffsetDiv2         = FrameParams->sliceBetaOffsetDiv2         & SLICE_BETA_MASK;
	pTaskDescriptor->nonVCLNALUSize              = (ESContext->offset - FrameParams->bitstreamOffset) +
	                                               estSEISize; /* SEI exact size configured for task descriptor */
	pTaskDescriptor->addrSourceBuffer            = FrameParams->addrSourceBuffer;
	pTaskDescriptor->addrOutputBitstreamStart    = addrOutputBitstreamStart;
	pTaskDescriptor->addrOutputBitstreamEnd      = addrOutputBitstreamEnd;
	pTaskDescriptor->bitstreamOffset             = bitstreamOffset &
	                                               BITSTREAM_OFFSET_MASK; //FrameParams->bitstreamOffset             & BITSTREAM_OFFSET_MASK;

	//For debug, monitor task descriptor before launching task
	H264HwEncodeDisplayTaskDescriptor(pTaskDescriptor, Context->clientId);

	//Flush cache to ensure task descriptor consistency in memory
	OSDEV_FlushCacheRange((void *) pTaskDescriptor, Context->taskDescriptorPhysAddress,
	                      sizeof(H264EncodeHardTaskDescriptor_t));

	//Flush cache to ensure reseted Paramout value in memory
	OSDEV_FlushCacheRange((void *) paramOut, Context->paramOutPhysAddress, Context->paramOutSize);

	Context->frameEncodeStart = OSDEV_GetTimeInMicroSeconds();

	//Launch an encode task
	if (H264HwEncodeLaunchEncodeTask(Context) != H264ENCHARD_NO_ERROR) {
		//specific management for early discarded command where no callback will be received
		iounmap((void *)ESContext->ESStart_p);
		dev_err(pDriverData->dev, "Error: command discarded due to risk of fifo overflow\n");
		return H264ENCHARD_ERROR;
	}

	// Return early. We will notify the completion through the Context->Callback()
	return H264ENCHARD_ENCODE_DEFERRED;
}

//{{{  H264HwEncodeCompletion
//{{{  doxynote
/// \brief      Provide any work following an encode frame completion event.
///
/// \return     H264EncodeHardStatus_t
//}}}
static H264EncodeHardStatus_t H264HwEncodeCompletion(H264EncodeHardCodecContext_t *Context)
{
	H264EncodeHardSequenceParams_t *SequenceParams = &Context->globalParameters;
	H264EncodeNVCLContext_t *nVCLContext = &Context->nVCLContext;
	ESBufferContext_t *ESContext = &Context->ESContext;
	H264EncodeHardFrameParams_t *FrameParams = &Context->FrameParams;
	H264EncodeHardParamOut_t encParamOut;
	uint8_t *streamBuf_p;
	H264EncodeHardParamOut_t *paramOut = (H264EncodeHardParamOut_t *) Context->paramOutHandler;
	uint32_t i;
	uint32_t ESSize;
	uint32_t ESBufferSize;

	Context->frameEncodeEnd = OSDEV_GetTimeInMicroSeconds();

	WakeUpLatency = Context->frameEncodeEnd - Context->LastInterruptTime;

	if (WakeUpLatency > WakeUpLatencyMax) {
		WakeUpLatencyMax = WakeUpLatency;
	}

	// Running Total
	LatencyTotal += WakeUpLatency;

	if (++LatencyFrame % LA_SAMPLES == 0) {
		// WakeUpLatencyAvg =  LatencyTotal / LA_SAMPLES;
		do_div(LatencyTotal, LA_SAMPLES);
		WakeUpLatencyAvg = (unsigned int) LatencyTotal;
		LatencyTotal = 0;
		WakeUpLatencyMax = 0;
	}

	// Invalidate cache to retrieve proper Paramout from memory
	OSDEV_InvCacheRange((void *)paramOut, Context->paramOutPhysAddress, Context->paramOutSize);

	dev_dbg(pDriverData->dev, "Hva command completed\n");

	// Store hva output parameters in context
	Context->statusBitstreamSize = paramOut->bitstreamSize;
	Context->statusRemovalTime   = paramOut->removalTime;
	Context->statusStuffingBits  = paramOut->stuffingBits;
	Context->frameEncodeDuration = (unsigned int)(Context->frameEncodeEnd - Context->frameEncodeStart);
	Context->hvcEncodeDuration   = (unsigned int)(paramOut->hvcStopTime - paramOut->hvcStartTime);

	// Clear hva output parameters
	memset((void *)Context->paramOutHandler, 0, Context->paramOutSize);

	dev_dbg(pDriverData->dev, "[Hva output parameters]\n");
	dev_dbg(pDriverData->dev, "|-statusBitstreamSize = %d bytes\n", Context->statusBitstreamSize);
	dev_dbg(pDriverData->dev, "|-statusStuffingBits  = %d bits\n", Context->statusStuffingBits);
	dev_dbg(pDriverData->dev, "|-statusRemovalTime   = %d\n", Context->statusRemovalTime);
	dev_dbg(pDriverData->dev, "|-hvcEncodeDuration   = %d hva ticks\n", Context->hvcEncodeDuration);
	dev_dbg(pDriverData->dev, "|-frameEncodeDuration = %d us\n", Context->frameEncodeDuration);
	dev_dbg(pDriverData->dev, "|-transformStatus     = %d (%s)\n", Context->transformStatus,
	        StringifyTransformStatus(Context->transformStatus));
	dev_dbg(pDriverData->dev, "\n");

	// Report frame statistics through Sysfs
	cpu_encode_duration = Context->frameEncodeDuration;
	hw_encode_duration  = Context->hvcEncodeDuration;

	// Calculate Utilisation Statistics
	utilisation_update(&hw_utilisation, paramOut->hvcStartTime, paramOut->hvcStopTime);
	utilisation_update(&sw_utilisation, Context->frameEncodeStart, Context->frameEncodeEnd);

	// Check if any HW error
	// FIXME: what would be the return channel for such error to MME client?
	if (Context->hwError != H264ENCHARD_NO_HW_ERROR) {
		dev_err(pDriverData->dev, "Error: hva error interrupt status is %08x (%s)\n", Context->hwError,
		        StringifyInterruptError(Context->hwError));

		iounmap((void *)ESContext->ESStart_p);

		switch (Context->hwError) {
		case H264ENCHARD_HW_LMI_ERR:
		case H264ENCHARD_HW_EMI_ERR:
			return H264ENCHARD_HW_ERROR;
		case H264ENCHARD_HW_SFL_OVERFLOW:
			return H264ENCHARD_CMD_DISCARDED;
		default:
			return H264ENCHARD_HW_ERROR;
		}
	} else {
		//manage context / task desc update at process end like mv_toggle, rec/ref buffer swap
		H264HwEncodeUpdateContextAtEncodeEnd(Context);

		encParamOut.bitstreamSize = Context->statusBitstreamSize;
		encParamOut.removalTime   = Context->statusRemovalTime;
		encParamOut.stuffingBits  = Context->statusStuffingBits;

		if ((SequenceParams->brcType != HVA_ENCODE_NO_BRC) &&
		    (Context->transformStatus == HVA_ENCODE_OK) &&
		    (SequenceParams->seiBufPeriodPresentFlag || SequenceParams->seiPicTimingPresentFlag)) {
			uint32_t SEISize = 0;
			streamBuf_p = ESContext->ESStart_p + (ESContext->offset >> 3);

			SEISize = H264HwEncodeCreateSEINAL(nVCLContext, FrameParams, &encParamOut, streamBuf_p);
			/* The SEI size will be different from estimation if there is some emulation code inserted */
			if (SEISize != ESContext->overEstSEISize) {
				dev_dbg(pDriverData->dev,
				        "Different length between current sei size (%d bytes) and over estimated sei size (%d bytes)\n", SEISize,
				        ESContext->overEstSEISize);
				if (SEISize < ESContext->overEstSEISize) {
					uint8_t *ESStartVCL_p; // Hardware Bitstream Buffer Virtual Address Pointer
					ESStartVCL_p = ESContext->ESStart_p + ((ESContext->offset + ESContext->overEstSEISize) >> 3);

					streamBuf_p = ESContext->ESStart_p + ((ESContext->offset + SEISize) >> 3); /* start address */
					memcpy((void *)streamBuf_p, (void *)ESStartVCL_p, (size_t)(encParamOut.bitstreamSize));
				} else {
					/* If we underestimate the SEI size, error will be generated */
					dev_err(pDriverData->dev, "Error: current sei size (%d) is larger than over estimated sei size (%d)\n", SEISize,
					        ESContext->overEstSEISize);
					iounmap((void *)ESContext->ESStart_p);
					return H264ENCHARD_HW_ERROR;
				}
			}
			ESContext->offset += SEISize;
		}
		ESBufferSize = (FrameParams->addrOutputBitstreamEnd - FrameParams->addrOutputBitstreamStart);
		ESSize = (ESContext->offset + encParamOut.bitstreamSize + (encParamOut.stuffingBits >> 3) + FILLER_DATA_HEADER_SIZE);
		dev_dbg(pDriverData->dev, "ESSize = %u, ESBufferSize = %u\n", ESSize, ESBufferSize);
		if (ESSize <= ESBufferSize) {
			if (encParamOut.stuffingBits > 0) {
				streamBuf_p = ESContext->ESStart_p + (ESContext->offset >> 3) + encParamOut.bitstreamSize;
				ESContext->offset += H264HwEncodeCreateFillerNAL(nVCLContext, &encParamOut, streamBuf_p);
			}
		} else {
			dev_warn(pDriverData->dev, "Warning: buffer too small to add filler data\n");
		}
		dev_dbg(pDriverData->dev, "[Frame composition]\n");
		dev_dbg(pDriverData->dev, "|-AU   = %d bytes\n",
		        (((ESContext->offset - FrameParams->bitstreamOffset) >> 3) - 1) + encParamOut.bitstreamSize +
		        (encParamOut.stuffingBits >> 3) + FILLER_DATA_HEADER_SIZE);
		dev_dbg(pDriverData->dev, "|-NVCL = %d bytes\n", ((ESContext->offset - FrameParams->bitstreamOffset) >> 3) - 1);
		dev_dbg(pDriverData->dev, "|-VCL  = %d bytes\n", encParamOut.bitstreamSize);
		dev_dbg(pDriverData->dev, "|-FD   = %d bytes\n", (encParamOut.stuffingBits > 0 &&
		                                                  ESSize <= ESBufferSize) ? (encParamOut.stuffingBits >> 3) + FILLER_DATA_HEADER_SIZE : 0);
		dev_dbg(pDriverData->dev, "\n");
		Context->statusNonVCLNALUSize = ((ESContext->offset - FrameParams->bitstreamOffset) >> 3);

		/* Unmap virtual address */
		iounmap((void *)ESContext->ESStart_p);

		for (i = 3; i > 0; i--) {
			Context->PastBitstreamSize[i] = Context->PastBitstreamSize[i - 1];
		}
		Context->PastBitstreamSize[0] = encParamOut.bitstreamSize;

		return H264ENCHARD_NO_ERROR;
	}
}


//{{{  H264HwEncodeTerminate
//{{{  doxynote
/// \brief      Terminate one encoder instance
///
/// \return     H264EncodeHardStatus_t
//}}}
H264EncodeHardStatus_t       H264HwEncodeTerminate(H264EncodeHardHandle_t                    Handle)
{
	H264EncodeHardCodecContext_t  *Context = (H264EncodeHardCodecContext_t *)Handle;

	mutex_lock(&H264EncoderGlobalLock);

	h264EncInstanceNb--;
	up(&h264EncInstances);

	//Free instance related Buffers

	H264HwEncodeFreeContextHwBuffers(Context);

	ClientContextList[Context->clientId] = NULL;

	//if last instance of encode, should free all resources like mono instances buffers
	if (h264EncInstanceNb == 0) {
		H264HwEncodeInterruptUnInstall();

		H264HwEncodeFreeSingletonHwBuffers();
	}

	OSDEV_Free(Context);

	mutex_unlock(&H264EncoderGlobalLock);

	return H264ENCHARD_NO_ERROR;
}

///////////////////////////////////////////////////////////////
//    The interrupt handler installation function

static H264EncodeHardStatus_t H264HwEncodeInterruptInstall(void)
{
	if (request_irq(platformData.Interrupt[0], H264HwEncodeInterruptHandler, IRQF_DISABLED, "h264enchw", NULL) != 0) {
		dev_err(pDriverData->dev, "Error: hva interrupt request failed\n");
		return H264ENCHARD_ERROR;
	}
	if (request_irq(platformData.Interrupt[1], H264HwEncodeErrorInterruptHandler, IRQF_DISABLED, "h264enchw", NULL) != 0) {
		dev_err(pDriverData->dev, "Error: hva error interrupt request failed\n");
		return H264ENCHARD_ERROR;
	}

	return H264ENCHARD_NO_ERROR;
}


// ///////////////////////////////////////////////////////////////////////////////
//
//    The interrupt handler removal function

static void H264HwEncodeInterruptUnInstall(void)
{
	dev_dbg(pDriverData->dev, "Free hva interrupts (CURRENTLY DISABLED)\n");
	free_irq(platformData.Interrupt[0], NULL);
	free_irq(platformData.Interrupt[1], NULL);
}

static void H264HwEncodeCompletionHandler(struct work_struct *work)
{
	H264EncodeHardCodecContext_t *Context = container_of(work, H264EncodeHardCodecContext_t, CompletionWork);

	// Complete the Encode
	Context->encodeStatus = H264HwEncodeCompletion(Context);

	// Once the completion is done, we can notify the callback.
	// This will update the host based on the status stored in Context->encodeStatus;
	if (Context->Callback) {
		Context->Callback(Context);
	} else {
		dev_err(pDriverData->dev, "Error: context callback undefined, failed to notify host\n");
	}

	// Increment global frame counter for Sysfs Statistics
	h264_frames_completed++;
}

// ///////////////////////////////////////////////////////////////////////////////
//
//    The interrupt handler code
OSDEV_InterruptHandlerEntrypoint(H264HwEncodeInterruptHandler)
{
	unsigned int fifoLevel;
	unsigned int fifoStatus = 0;

	unsigned long flags;

	H264EncodeHardCodecContext_t   *Context = NULL;
	unsigned int CommandStatus;
	unsigned int TaskId;
	unsigned int ClientId;

	spin_lock_irqsave(&platformData.hw_lock, flags);

	fifoLevel = ReadHvaRegister(HVA_HIF_REG_SFL);
	if ((fifoLevel & 0xF) > 0) { //at least one element in status fifo
		fifoStatus = ReadHvaRegister(HVA_HIF_FIFO_STS);

		// Hva periodically raises an interrupt unless it receives an acknowledge.
		// Acknowledge hva only if fifo level is greater than 0 and do it as quick
		// as possible to limit interrupt reply.
		WriteHvaRegister(HVA_HIF_REG_IT_ACK, 0x1);

		// We have read the FIFO register, and therefore removed an entry from the queue...
		up(&h264CmdFifoDepth);

		CommandStatus = (fifoStatus) & 0xFF;
		ClientId = (fifoStatus >> 8) & 0xFF;
		TaskId = (fifoStatus >> 16) & 0xFFFF;

		if (ClientId < MAX_HVA_ENCODE_INSTANCE) {
			Context = ClientContextList[ClientId];

			// Assert that we have obtained the correct Context
			if (ClientId != Context->clientId) {
				dev_err(pDriverData->dev, "Error: context id do not match (%u != %u)\n", ClientId, Context->clientId);
			}
		} else {
			dev_err(pDriverData->dev, "Error: ClientId %u >= MAX_HVA_ENCODE_INSTANCE %u\n", ClientId, MAX_HVA_ENCODE_INSTANCE);
		}

		//Too intrusive: only to debug issue
		dev_dbg(pDriverData->dev, "TaskId = %u , ClientId = %u, CommandStatus = 0x%x\n", TaskId, ClientId, CommandStatus);
	} else {
		pDriverData->spurious_irq_count++;
		dev_warn(pDriverData->dev, "Warning: hva fifo is empty, fifoLevel = %u, spurious_irq_count = %d\n", fifoLevel,
		         pDriverData->spurious_irq_count);
	}

	// Unlock as soon as possible to limit time of disabled interrupt
	spin_unlock_irqrestore(&platformData.hw_lock, flags);

	if (Context) {
		// We can update our Context Specifics ... before completing the frame
		Context->transformStatus = (unsigned char) CommandStatus;
		Context->hwError         = errorInterruptStatus;

		// Reset globally stored interrupt status, now that it has been 'claimed' by the Context.
		errorInterruptStatus         = H264ENCHARD_NO_HW_ERROR;

		Context->LastInterruptTime = OSDEV_GetTimeInMicroSeconds();

		// Ask our Completion work to be executed.
		// This can't be done in interrupt context, as it involves Calling Multicom
		// and unmapping memory.
		schedule_work(&Context->CompletionWork);
	} else {
		dev_warn(pDriverData->dev, "Warning: context not found, fifoLevel = %u, fifoStatus = %u\n", fifoLevel, fifoStatus);
	}

	return IRQ_HANDLED;
}


// ///////////////////////////////////////////////////////////////////////////////
//
//    The interrupt handler code
OSDEV_InterruptHandlerEntrypoint(H264HwEncodeErrorInterruptHandler)
{
	volatile unsigned int LMIError;
	volatile unsigned int EMIError;
	volatile unsigned int fifoLevel;

	unsigned long flags;

	spin_lock_irqsave(&platformData.hw_lock, flags);

	dev_err(pDriverData->dev, "Error: hva error interrupt raised\n");

	//Track local memory interface error
	LMIError = ReadHvaRegister(HVA_HIF_REG_LMI_ERR);
	if (LMIError != 0) {
		errorInterruptStatus = H264ENCHARD_HW_LMI_ERR;
	}
	dev_dbg(pDriverData->dev, "HVA_HIF_REG_LMI_ERR = 0x%x\n", LMIError);
	//Track external memory interface error
	EMIError = ReadHvaRegister(HVA_HIF_REG_EMI_ERR);
	if (EMIError != 0) {
		errorInterruptStatus = H264ENCHARD_HW_EMI_ERR;
	}
	dev_dbg(pDriverData->dev, "HVA_HIF_REG_EMI_ERR = 0x%x\n", EMIError);

	//Track fifo command overflow
	fifoLevel = ReadHvaRegister(HVA_HIF_REG_SFL);
	if (fifoLevel == HVA_STATUS_HW_FIFO_SIZE) {
		errorInterruptStatus = H264ENCHARD_HW_SFL_OVERFLOW;
	}
	dev_dbg(pDriverData->dev, "HVA_HIF_REG_SFL = 0x%x\n", fifoLevel);
	dev_dbg(pDriverData->dev, "HVA_STATUS_HW_FIFO_SIZE = %d\n", HVA_STATUS_HW_FIFO_SIZE);

	//Acknowledge HW error interrupt
	WriteHvaRegister(HVA_HIF_REG_ERR_IT_ACK, 0x1);

	//FIXME: error management to be addressed

	spin_unlock_irqrestore(&platformData.hw_lock, flags);

	return IRQ_HANDLED;
}

/////////////////////////////////////////////////////////////////////////////////
//
//Allocate here all mono instance HVA related buffers
//Physical contiguous addresses are required

static H264EncodeHardStatus_t H264HwEncodeAllocateSingletonHwBuffers(void)
{
	unsigned int misalignment = 0;

	dev_dbg(pDriverData->dev, "[Allocate singleton buffers]\n");

	//
	//SRAM buffers: single instance buffer as no context required
	//
	dev_dbg(pDriverData->dev, "|-Hva esram start address is 0x%x\n", newHvaSramFreePhysAddress);

	//search window buffer
	//Window in reference picture in YUV 420 MB-tiled format with size=(4*MBx+6)*256*3/2
	searchWindowBufPhysAddress = HVA_ALIGNED_256(newHvaSramFreePhysAddress);
	misalignment = searchWindowBufPhysAddress - newHvaSramFreePhysAddress;
	dev_dbg(pDriverData->dev, "|-searchWindow buffer misalignment is 0x%x bytes\n", misalignment);
	//Update free memory address pool
	newHvaSramFreePhysAddress += (SEARCH_WINDOW_BUFFER_MAX_SIZE + misalignment);
	if (newHvaSramFreePhysAddress > (HvaSramBase + HvaSramSize)) {
		return H264ENCHARD_NO_HVA_ERAM_MEMORY;
	}
	dev_dbg(pDriverData->dev, "|-searchWindowBufPhysAddress = 0x%x\n", searchWindowBufPhysAddress);
	dev_dbg(pDriverData->dev, "|-SEARCH_WINDOW_BUFFER_MAX_SIZE = %u bytes\n", SEARCH_WINDOW_BUFFER_MAX_SIZE);

	//local_rec_buffer
	//4 lines of pixels (in Luma, Chroma blue and Chroma red) of top MB for deblocking with size=4*16*MBx*2
	localRecBufPhysAddress = HVA_ALIGNED_256(newHvaSramFreePhysAddress);
	misalignment = localRecBufPhysAddress - newHvaSramFreePhysAddress;
	dev_dbg(pDriverData->dev, "|-localRec buffer misalignment is 0x%x bytes\n", misalignment);
	//Update free memory address pool
	newHvaSramFreePhysAddress += (LOCAL_RECONSTRUCTED_BUFFER_MAX_SIZE + misalignment);
	if (newHvaSramFreePhysAddress > (HvaSramBase + HvaSramSize)) {
		return H264ENCHARD_NO_HVA_ERAM_MEMORY;
	}
	dev_dbg(pDriverData->dev, "|-localRecBufPhysAddress = 0x%x\n", localRecBufPhysAddress);
	dev_dbg(pDriverData->dev, "|-LOCAL_RECONSTRUCTED_BUFFER_MAX_SIZE = %u bytes\n", LOCAL_RECONSTRUCTED_BUFFER_MAX_SIZE);

	//context MXB data in Raw format
	contextMBBufPhysAddress = HVA_ALIGNED_256(newHvaSramFreePhysAddress);
	misalignment = contextMBBufPhysAddress - newHvaSramFreePhysAddress;
	dev_dbg(pDriverData->dev, "|-ctxMB buffer misalignment is 0x%x bytes\n", misalignment);
	//Update free memory address pool
	newHvaSramFreePhysAddress += (CTX_MB_BUFFER_MAX_SIZE + misalignment);
	if (newHvaSramFreePhysAddress > (HvaSramBase + HvaSramSize)) {
		return H264ENCHARD_NO_HVA_ERAM_MEMORY;
	}
	dev_dbg(pDriverData->dev, "|-contextMBBufPhysAddress = 0x%x\n", contextMBBufPhysAddress);
	dev_dbg(pDriverData->dev, "|-CTX_MB_BUFFER_MAX_SIZE = %u bytes\n", CTX_MB_BUFFER_MAX_SIZE);

	//Cabac context
	cabacContextBufPhysAddress = HVA_ALIGNED_256(newHvaSramFreePhysAddress);
	misalignment = cabacContextBufPhysAddress - newHvaSramFreePhysAddress;
	dev_dbg(pDriverData->dev, "|-cabacContext buffer misalignment is 0x%x bytes\n", misalignment);
	//Update free memory address pool
	newHvaSramFreePhysAddress += (CABAC_CONTEXT_BUFFER_MAX_SIZE + misalignment);
	if (newHvaSramFreePhysAddress > (HvaSramBase + HvaSramSize)) {
		return H264ENCHARD_NO_HVA_ERAM_MEMORY;
	}
	dev_dbg(pDriverData->dev, "|-cabacContextBufPhysAddress = 0x%x\n", cabacContextBufPhysAddress);
	dev_dbg(pDriverData->dev, "|-CABAC_CONTEXT_BUFFER_MAX_SIZE = %u bytes\n", CABAC_CONTEXT_BUFFER_MAX_SIZE);
	dev_dbg(pDriverData->dev, "|-Hva esram end address is 0x%x\n", newHvaSramFreePhysAddress);
	dev_dbg(pDriverData->dev, "\n");

	if (newHvaSramFreePhysAddress > HvaSramBase + HvaSramSize) {
		dev_err(pDriverData->dev,
		        "Error: esram overflow => newHvaSramFreePhysAddress 0x%x > 0x%x (HvaSramBase 0x%x + HvaSramSize %u bytes)\n",
		        newHvaSramFreePhysAddress, HvaSramBase + HvaSramSize, HvaSramBase, HvaSramSize);
	}

	return H264ENCHARD_NO_ERROR;
}

/////////////////////////////////////////////////////////////////////////////////
//
//Free here all mono instance HVA related buffers
//Physical contiguous addresses are required

static void H264HwEncodeFreeSingletonHwBuffers(void)
{
	//
	//reset SRAM buffers base address
	//
	newHvaSramFreePhysAddress = platformData.BaseAddress[1];
}

static uint32_t H264HwEncodeParamOutSize(uint32_t MaxFrameWidth, uint32_t MaxFrameHeight)
{
	uint32_t paramOutSize = 0;
	paramOutSize = sizeof(H264EncodeHardParamOut_t) + sizeof(H264EncodeHardParamOutSlice_t) * MaxFrameWidth / 16 *
	               MaxFrameHeight / 16;
	return paramOutSize;
}
/////////////////////////////////////////////////////////////////////////////////
//
//Allocate here all multi instance HVA related buffers
//Physical contiguous addresses are required

static H264EncodeHardStatus_t H264HwEncodeAllocateContextHwBuffers(H264EncodeHardCodecContext_t  *Context ,
                                                                   uint32_t MaxFrameWidth, uint32_t MaxFrameHeight)
{
	/*Temporary SDRAM buffer addresses (multi instance only) used for HVA internal processing before storage in instance context */
	unsigned int referenceFrameBufAddress = 0;
	unsigned int reconstructedFrameBufAddress = 0;
	unsigned int spatialContextBufAddress = 0;
	unsigned int temporalContextBufAddress = 0;
	unsigned int brcInOutBufAddress = 0;
	unsigned long spatialContextBufPhysAddress = 0;
	unsigned long temporalContextBufPhysAddress = 0;
	unsigned long brcInOutBufPhysAddress = 0;
	unsigned int paramOutAddress = 0;
	unsigned int taskDescAddress = 0;
	unsigned long referenceFrameBufPhysAddress = 0;
	unsigned long reconstructedFrameBufPhysAddress = 0;
	unsigned long paramOutPhysAddress = 0;
	unsigned long taskDescPhysAddress = 0;
	unsigned int sliceHeaderBufAddress = 0;
	unsigned long sliceHeaderBufPhysAddress = 0;
	char EncodePartitionName[MAX_PARTITION_NAME_SIZE];

	dev_dbg(pDriverData->dev, "Allocate hva context buffers for client %u\n", Context->clientId);

	snprintf(EncodePartitionName, sizeof(EncodePartitionName), "%s-%d", HVA_MME_PARTITION, ((Context->clientId) % 2));
	EncodePartitionName[sizeof(EncodePartitionName) - 1] = '\0';

	//Record frame max dimension information in Context
	Context->maxFrameWidth = MaxFrameWidth;
	Context->maxFrameHeight = MaxFrameHeight;
	dev_dbg(pDriverData->dev, "Max frame dimension: width = %u pixels, height = %u pixels\n", MaxFrameWidth,
	        MaxFrameHeight);

	dev_dbg(pDriverData->dev, "[Hva context buffers]\n");

	//Reference Frame with size=MBx*MBy*256*3/2 (addr_fw_ref_buffer) => Multi instance
	Context->referenceFrameBufSize = MaxFrameWidth * MaxFrameHeight * 3 / 2;
	referenceFrameBufAddress = (unsigned int) OSDEV_AllignedMallocHwBuffer(ENCODE_BUFFER_ALIGNMENT,
	                                                                       Context->referenceFrameBufSize, EncodePartitionName, &referenceFrameBufPhysAddress);
	if (referenceFrameBufAddress == 0) {
		dev_err(pDriverData->dev, "Error: reference frame buffer allocation failed\n");
		return H264ENCHARD_NO_SDRAM_MEMORY;
	}
	Context->referenceFrameBufHandler = referenceFrameBufAddress;
	Context->referenceFrameBufPhysAddress = (unsigned int) referenceFrameBufPhysAddress;
	Context->referenceFrameBufPhysAddress = HVA_SET_BUFFER_AS_SDRAM(Context->referenceFrameBufPhysAddress);
	dev_dbg(pDriverData->dev, "|-referenceFrameBufHandler         = 0x%x\n", Context->referenceFrameBufHandler);
	dev_dbg(pDriverData->dev, "|-referenceFrameBufSize            = %u bytes\n", Context->referenceFrameBufSize);
	dev_dbg(pDriverData->dev, "|-referenceFrameBufPhysAddress     = 0x%x\n", Context->referenceFrameBufPhysAddress);

	Context->spatialContextBufSize = H264_ENCODE_SPATIAL_DATA_SIZE;
	spatialContextBufAddress = (unsigned int) OSDEV_AllignedMallocHwBuffer(ENCODE_BUFFER_ALIGNMENT,
	                                                                       Context->spatialContextBufSize, EncodePartitionName, &spatialContextBufPhysAddress);
	if (spatialContextBufAddress == 0) {
		dev_err(pDriverData->dev, "Error: spatial context buffer allocation failed\n");
		goto spatial_context_alloc_fail;
	}
	Context->temporalContextBufSize = H264_ENCODE_TEMPORAL_DATA_SIZE;
	temporalContextBufAddress = (unsigned int) OSDEV_AllignedMallocHwBuffer(ENCODE_BUFFER_ALIGNMENT,
	                                                                        Context->temporalContextBufSize, EncodePartitionName, &temporalContextBufPhysAddress);
	if (temporalContextBufAddress == 0) {
		dev_err(pDriverData->dev, "Error: temporal context buffer allocation failed\n");
		goto temporal_context_alloc_fail;
	}
	Context->brcInOutBufSize = H264_ENCODE_BRC_DATA_SIZE;
	brcInOutBufAddress = (unsigned int) OSDEV_AllignedMallocHwBuffer(ENCODE_BUFFER_ALIGNMENT, Context->brcInOutBufSize,
	                                                                 EncodePartitionName, &brcInOutBufPhysAddress);
	if (brcInOutBufAddress == 0) {
		dev_err(pDriverData->dev, "Error: brc in/out buffer allocation failed\n");
		goto brc_in_out_alloc_fail;
	}
	Context->spatialContextBufHandler = spatialContextBufAddress;
	Context->spatialContextBufPhysAddress = (unsigned int) spatialContextBufPhysAddress;
	Context->temporalContextBufHandler = temporalContextBufAddress;
	Context->temporalContextBufPhysAddress = (unsigned int) temporalContextBufPhysAddress;
	Context->brcInOutBufHandler = brcInOutBufAddress;
	Context->brcInOutBufPhysAddress = (unsigned int) brcInOutBufPhysAddress;
	//Init Buffer content with '0'
	memset((void *)spatialContextBufAddress, 0, Context->spatialContextBufSize);
	memset((void *)temporalContextBufAddress, 0, Context->temporalContextBufSize);
	memset((void *)brcInOutBufAddress, 0, Context->brcInOutBufSize);
	Context->spatialContextBufPhysAddress = HVA_SET_BUFFER_AS_SDRAM(Context->spatialContextBufPhysAddress);
	Context->temporalContextBufPhysAddress = HVA_SET_BUFFER_AS_SDRAM(Context->temporalContextBufPhysAddress);
	Context->brcInOutBufPhysAddress = HVA_SET_BUFFER_AS_SDRAM(Context->brcInOutBufPhysAddress);
	dev_dbg(pDriverData->dev, "|-spatialContextBufHandler         = 0x%x\n", Context->spatialContextBufHandler);
	dev_dbg(pDriverData->dev, "|-spatialContextBufSize            = %u bytes\n", Context->spatialContextBufSize);
	dev_dbg(pDriverData->dev, "|-spatialContextBufPhysAddress     = 0x%x\n", Context->spatialContextBufPhysAddress);
	dev_dbg(pDriverData->dev, "|-temporalContextBufHandler        = 0x%x\n", Context->temporalContextBufHandler);
	dev_dbg(pDriverData->dev, "|-temporalContextBufSize           = %u bytes\n", Context->temporalContextBufSize);
	dev_dbg(pDriverData->dev, "|-temporalContextBufPhysAddress    = 0x%x\n", Context->temporalContextBufPhysAddress);
	dev_dbg(pDriverData->dev, "|-brcInOutBufHandler               = 0x%x\n", Context->brcInOutBufHandler);
	dev_dbg(pDriverData->dev, "|-brcInOutBufSize                  = %u bytes\n", Context->brcInOutBufSize);
	dev_dbg(pDriverData->dev, "|-brcInOutBufPhysAddress           = 0x%x\n", Context->brcInOutBufPhysAddress);

	//ParamOut buffer with size=5x4 (addr_param_in_out) => Multi instance
	Context->paramOutSize = H264HwEncodeParamOutSize(MaxFrameWidth, MaxFrameHeight);
	paramOutAddress = (unsigned int) OSDEV_AllignedMallocHwBuffer(ENCODE_BUFFER_ALIGNMENT, Context->paramOutSize,
	                                                              EncodePartitionName, &paramOutPhysAddress);
	if (paramOutAddress == 0) {
		dev_err(pDriverData->dev, "Error: param out buffer allocation failed\n");
		goto param_out_alloc_fail;
	}
	Context->paramOutHandler = paramOutAddress;
	Context->paramOutPhysAddress = (unsigned int) paramOutPhysAddress;
	Context->paramOutPhysAddress = HVA_SET_BUFFER_AS_SDRAM(Context->paramOutPhysAddress);
	memset((void *)paramOutAddress, 0, Context->paramOutSize);
	dev_dbg(pDriverData->dev, "|-paramOutHandler                  = 0x%x\n", Context->paramOutHandler);
	dev_dbg(pDriverData->dev, "|-paramOutSize                     = %u bytes\n", Context->paramOutSize);
	dev_dbg(pDriverData->dev, "|-paramOutPhysAddress              = 0x%x\n", Context->paramOutPhysAddress);

	//HVA Task descriptor that embeds all task programming
	Context->taskDescriptorSize = sizeof(H264EncodeHardTaskDescriptor_t);
	taskDescAddress = (unsigned int) OSDEV_AllignedMallocHwBuffer(ENCODE_BUFFER_ALIGNMENT, Context->taskDescriptorSize,
	                                                              EncodePartitionName, &taskDescPhysAddress);
	if (taskDescAddress == 0) {
		dev_err(pDriverData->dev, "Error: task descriptor buffer allocation failed\n");
		goto task_desc_alloc_fail;
	}
	Context->taskDescriptorHandler = taskDescAddress;
	Context->taskDescriptorPhysAddress = (unsigned int) taskDescPhysAddress;
	Context->taskDescriptorPhysAddress = HVA_SET_BUFFER_AS_SDRAM(Context->taskDescriptorPhysAddress);
	memset((void *)taskDescAddress, 0, Context->taskDescriptorSize);
	dev_dbg(pDriverData->dev, "|-taskDescriptorHandler            = 0x%x\n", Context->taskDescriptorHandler);
	dev_dbg(pDriverData->dev, "|-taskDescriptorSize               = %u bytes\n", Context->taskDescriptorSize);
	dev_dbg(pDriverData->dev, "|-taskDescriptorPhysAddress        = 0x%x\n", Context->taskDescriptorPhysAddress);

	//Reconstructed Frame should also multi instantiated to enable reference by
	//Reconstructed Frame with size=MBx*MBy*256*3/2 (addr_fw_ref_buffer) => Multi instance
	Context->reconstructedFrameBufSize = MaxFrameWidth * MaxFrameHeight * 3 / 2;
	reconstructedFrameBufAddress = (unsigned int) OSDEV_AllignedMallocHwBuffer(ENCODE_BUFFER_ALIGNMENT,
	                                                                           Context->reconstructedFrameBufSize, EncodePartitionName, &reconstructedFrameBufPhysAddress);
	if (reconstructedFrameBufAddress == 0) {
		dev_err(pDriverData->dev, "Error: recontructed frame buffer allocation failed\n");
		goto rec_buf_alloc_fail;
	}
	Context->reconstructedFrameBufHandler = reconstructedFrameBufAddress;
	Context->reconstructedFrameBufPhysAddress = (unsigned int) reconstructedFrameBufPhysAddress;
	Context->reconstructedFrameBufPhysAddress = HVA_SET_BUFFER_AS_SDRAM(Context->reconstructedFrameBufPhysAddress);
	dev_dbg(pDriverData->dev, "|-reconstructedFrameBufHandler     = 0x%x\n", Context->reconstructedFrameBufHandler);
	dev_dbg(pDriverData->dev, "|-reconstructedFrameBufSize        = %u bytes\n", Context->reconstructedFrameBufSize);
	dev_dbg(pDriverData->dev, "|-reconstructedFrameBufPhysAddress = 0x%x\n", Context->reconstructedFrameBufPhysAddress);

	//Slice header
	Context->sliceHeaderBufSize = 4 * 16;
	sliceHeaderBufAddress = (unsigned int) OSDEV_AllignedMallocHwBuffer(ENCODE_BUFFER_ALIGNMENT,
	                                                                    Context->sliceHeaderBufSize, EncodePartitionName, &sliceHeaderBufPhysAddress);
	if (sliceHeaderBufAddress == 0) {
		dev_err(pDriverData->dev, "Error: slice header buffer allocation failed\n");
		goto slice_header_alloc_fail;
	}
	Context->sliceHeaderBufHandler = sliceHeaderBufAddress;
	Context->sliceHeaderBufPhysAddress = (unsigned int) sliceHeaderBufPhysAddress;
	Context->sliceHeaderBufPhysAddress = HVA_SET_BUFFER_AS_SDRAM(Context->sliceHeaderBufPhysAddress);
	dev_dbg(pDriverData->dev, "|-sliceHeaderBufHandler            = 0x%x\n", Context->sliceHeaderBufHandler);
	dev_dbg(pDriverData->dev, "|-sliceHeaderBufSize               = %u bytes\n", Context->sliceHeaderBufSize);
	dev_dbg(pDriverData->dev, "|-sliceHeaderBufPhysAddress        = 0x%x\n", Context->sliceHeaderBufPhysAddress);

	//TODO add scaling matrix buffers
	//TODO add region of interest buffer
	//TODO add motion vector map buffer

	dev_dbg(pDriverData->dev, "\n");

	return H264ENCHARD_NO_ERROR;

slice_header_alloc_fail:
	OSDEV_AllignedFreeHwBuffer((void *)reconstructedFrameBufAddress, EncodePartitionName);
rec_buf_alloc_fail:
	OSDEV_AllignedFreeHwBuffer((void *)taskDescAddress, EncodePartitionName);
task_desc_alloc_fail:
	OSDEV_AllignedFreeHwBuffer((void *)paramOutAddress, EncodePartitionName);
param_out_alloc_fail:
	OSDEV_AllignedFreeHwBuffer((void *)brcInOutBufAddress, EncodePartitionName);
brc_in_out_alloc_fail:
	OSDEV_AllignedFreeHwBuffer((void *)temporalContextBufAddress, EncodePartitionName);
temporal_context_alloc_fail:
	OSDEV_AllignedFreeHwBuffer((void *)spatialContextBufAddress, EncodePartitionName);
spatial_context_alloc_fail:
	OSDEV_AllignedFreeHwBuffer((void *)referenceFrameBufAddress, EncodePartitionName);

	return H264ENCHARD_NO_SDRAM_MEMORY;
}

/////////////////////////////////////////////////////////////////////////////////
//
//Free here all multi instance HVA related buffers

static void H264HwEncodeFreeContextHwBuffers(H264EncodeHardCodecContext_t  *Context)
{
	char EncodePartitionName[MAX_PARTITION_NAME_SIZE];

	snprintf(EncodePartitionName, sizeof(EncodePartitionName), "%s-%d", HVA_MME_PARTITION, ((Context->clientId) % 2));
	EncodePartitionName[sizeof(EncodePartitionName) - 1] = '\0';

	dev_dbg(pDriverData->dev, "Free hva context buffers for client %u\n", Context->clientId);

	dev_dbg(pDriverData->dev, "Free reference frame buffer at virtual address 0x%x\n",
	        (unsigned int)Context->referenceFrameBufHandler);
	OSDEV_AllignedFreeHwBuffer((void *)Context->referenceFrameBufHandler, EncodePartitionName);
	dev_dbg(pDriverData->dev, "Free reconstructed frame buffer at virtual address 0x%x\n",
	        (unsigned int)Context->reconstructedFrameBufHandler);
	OSDEV_AllignedFreeHwBuffer((void *)Context->reconstructedFrameBufHandler, EncodePartitionName);
	dev_dbg(pDriverData->dev, "Free spatial context buffer at virtual address 0x%x\n",
	        (unsigned int)Context->spatialContextBufHandler);
	OSDEV_AllignedFreeHwBuffer((void *)Context->spatialContextBufHandler, EncodePartitionName);
	dev_dbg(pDriverData->dev, "Free temporal context buffer at virtual address 0x%x\n",
	        (unsigned int)Context->temporalContextBufHandler);
	OSDEV_AllignedFreeHwBuffer((void *)Context->temporalContextBufHandler, EncodePartitionName);
	dev_dbg(pDriverData->dev, "Free brc in/out buffer at virtual address 0x%x\n",
	        (unsigned int)Context->brcInOutBufHandler);
	OSDEV_AllignedFreeHwBuffer((void *)Context->brcInOutBufHandler, EncodePartitionName);
	dev_dbg(pDriverData->dev, "Free output parameter buffer at virtual address 0x%x\n",
	        (unsigned int)Context->paramOutHandler);
	OSDEV_AllignedFreeHwBuffer((void *)Context->paramOutHandler, EncodePartitionName);
	dev_dbg(pDriverData->dev, "Free task descriptor buffer at virtual address 0x%x\n",
	        (unsigned int)Context->taskDescriptorHandler);
	OSDEV_AllignedFreeHwBuffer((void *)Context->taskDescriptorHandler, EncodePartitionName);
	dev_dbg(pDriverData->dev, "Free slice header buffer at virtual address 0x%x\n",
	        (unsigned int)Context->sliceHeaderBufHandler);
	OSDEV_AllignedFreeHwBuffer((void *)Context->sliceHeaderBufHandler, EncodePartitionName);
}

/////////////////////////////////////////////////////////////////////////////////
//
//For debug puprose, display instance task descriptor

static void H264HwEncodeDisplayTaskDescriptor(H264EncodeHardTaskDescriptor_t  *pTaskDescriptor, uint8_t clientId)
{
	dev_dbg(pDriverData->dev, "[Task descriptor for client %d]\n", clientId);
	dev_dbg(pDriverData->dev, "|-frameWidth                 = %u pixels\n", pTaskDescriptor->frameWidth);
	dev_dbg(pDriverData->dev, "|-frameHeight                = %u pixels\n", pTaskDescriptor->frameHeight);
	dev_dbg(pDriverData->dev, "|-windowWidth                = %u pixels\n", pTaskDescriptor->windowWidth);
	dev_dbg(pDriverData->dev, "|-windowHeight               = %u pixels\n", pTaskDescriptor->windowHeight);
	dev_dbg(pDriverData->dev, "|-windowHorizontalOffset     = %u pixels\n", pTaskDescriptor->windowHorizontalOffset);
	dev_dbg(pDriverData->dev, "|-windowVerticalOffset       = %u pixels\n", pTaskDescriptor->windowVerticalOffset);
	dev_dbg(pDriverData->dev, "|-pictureCodingType          = %u (%s)\n", pTaskDescriptor->pictureCodingType,
	        StringifyPictureCodingType(pTaskDescriptor->pictureCodingType));
	dev_dbg(pDriverData->dev, "|-reserved0                  = %u\n", pTaskDescriptor->reserved0);
	dev_dbg(pDriverData->dev, "|-reserved1                  = %u\n", pTaskDescriptor->reserved1);
	dev_dbg(pDriverData->dev, "|-picOrderCntType            = %u\n", pTaskDescriptor->picOrderCntType);
	dev_dbg(pDriverData->dev, "|-firstPictureInSequence     = %u\n", pTaskDescriptor->firstPictureInSequence);
	dev_dbg(pDriverData->dev, "|-useConstrainedIntraFlag    = %u\n", pTaskDescriptor->useConstrainedIntraFlag);
	dev_dbg(pDriverData->dev, "|-sliceSizeType              = %u\n", pTaskDescriptor->sliceSizeType);
	dev_dbg(pDriverData->dev, "|-sliceByteSize              = %u bytes\n", pTaskDescriptor->sliceByteSize);
	dev_dbg(pDriverData->dev, "|-sliceMbSize                = %u macroblocks\n", pTaskDescriptor->sliceMbSize);
	dev_dbg(pDriverData->dev, "|-intraRefreshType           = %u (%s)\n", pTaskDescriptor->intraRefreshType,
	        StringifyIntraRefreshType(pTaskDescriptor->intraRefreshType));
	if (pTaskDescriptor->intraRefreshType == HVA_ENCODE_DISABLE_INTRA_REFRESH) {
		dev_dbg(pDriverData->dev, "|-irParamOption              = %u\n", pTaskDescriptor->irParamOption);
	} else {
		dev_dbg(pDriverData->dev, "|-irParamOption              = %u %s\n", pTaskDescriptor->irParamOption,
		        (pTaskDescriptor->intraRefreshType == HVA_ENCODE_ADAPTIVE_INTRA_REFRESH) ? "macroblocks/frame refreshed" :
		        "frames (refresh period)");
	}
	dev_dbg(pDriverData->dev, "|-reserved2                  = %u\n", pTaskDescriptor->reserved2);
	dev_dbg(pDriverData->dev, "|-disableDeblockingFilterIdc = %u (%s)\n", pTaskDescriptor->disableDeblockingFilterIdc,
	        StringifyDeblocking(pTaskDescriptor->disableDeblockingFilterIdc));
	dev_dbg(pDriverData->dev, "|-sliceAlphaC0OffsetDiv2     = %d\n", pTaskDescriptor->sliceAlphaC0OffsetDiv2);
	dev_dbg(pDriverData->dev, "|-sliceBetaOffsetDiv2        = %d\n", pTaskDescriptor->sliceBetaOffsetDiv2);
	dev_dbg(pDriverData->dev, "|-brcType                    = %d (%s)\n", pTaskDescriptor->brcType,
	        StringifyBrcType(pTaskDescriptor->brcType));
	dev_dbg(pDriverData->dev, "|-NonVCLNALUSize             = %u bits\n", pTaskDescriptor->nonVCLNALUSize);
	dev_dbg(pDriverData->dev, "|-cpbBufferSize              = %u bits\n", pTaskDescriptor->cpbBufferSize);
	dev_dbg(pDriverData->dev, "|-bitRate                    = %u bps\n", pTaskDescriptor->bitRate);
	dev_dbg(pDriverData->dev, "|-samplingMode               = %u (%s)\n", pTaskDescriptor->samplingMode,
	        StringifySamplingMode(pTaskDescriptor->samplingMode));
	dev_dbg(pDriverData->dev, "|-transformMode              = %u (%s)\n", pTaskDescriptor->transformMode,
	        StringifyTransformMode(pTaskDescriptor->transformMode));
	dev_dbg(pDriverData->dev, "|-encoderComplexity          = %u (%s)\n", pTaskDescriptor->encoderComplexity,
	        StringifyEncoderComplexity(pTaskDescriptor->encoderComplexity));
	dev_dbg(pDriverData->dev, "|-quant                      = %u\n", pTaskDescriptor->quant);
	dev_dbg(pDriverData->dev, "|-framerateNum               = %u\n", pTaskDescriptor->framerateNum);
	dev_dbg(pDriverData->dev, "|-framerateDen               = %u\n", pTaskDescriptor->framerateDen);
	dev_dbg(pDriverData->dev, "|-delay                      = %u ms\n", pTaskDescriptor->delay);
	dev_dbg(pDriverData->dev, "|-strictHRDCompliancy        = %u\n", pTaskDescriptor->strictHRDCompliancy);
	dev_dbg(pDriverData->dev, "|-qpmin                      = %u\n", pTaskDescriptor->qpmin);
	dev_dbg(pDriverData->dev, "|-qpmax                      = %u\n", pTaskDescriptor->qpmax);
	dev_dbg(pDriverData->dev, "|-addrSourceBuffer           = 0x%x\n", pTaskDescriptor->addrSourceBuffer);
	dev_dbg(pDriverData->dev, "|-addrFwdRefBuffer           = 0x%x\n", pTaskDescriptor->addrFwdRefBuffer);
	dev_dbg(pDriverData->dev, "|-addrRecBuffer              = 0x%x\n", pTaskDescriptor->addrRecBuffer);
	dev_dbg(pDriverData->dev, "|-addrOutputBitstreamStart   = 0x%x\n", pTaskDescriptor->addrOutputBitstreamStart);
	dev_dbg(pDriverData->dev, "|-addrOutputBitstreamEnd     = 0x%x\n", pTaskDescriptor->addrOutputBitstreamEnd);
	dev_dbg(pDriverData->dev, "|-bitstreamOffset            = %u bits\n", pTaskDescriptor->bitstreamOffset);
	dev_dbg(pDriverData->dev, "|-addrLctx                   = 0x%x\n", pTaskDescriptor->addrLctx);
	dev_dbg(pDriverData->dev, "|-addrParamInout             = 0x%x\n", pTaskDescriptor->addrParamInout);
	dev_dbg(pDriverData->dev, "|-addrExternalSw             = 0x%x\n", pTaskDescriptor->addrExternalSw);
	dev_dbg(pDriverData->dev, "|-addrLocalRecBuffer         = 0x%x\n", pTaskDescriptor->addrLocalRecBuffer);
	dev_dbg(pDriverData->dev, "|-addrSpatialContext         = 0x%x\n", pTaskDescriptor->addrSpatialContext);
	dev_dbg(pDriverData->dev, "|-addrTemporalContext        = 0x%x\n", pTaskDescriptor->addrTemporalContext);
	dev_dbg(pDriverData->dev, "|-addrBrcInOutParameter      = 0x%x\n", pTaskDescriptor->addrBrcInOutParameter);
	dev_dbg(pDriverData->dev, "|-chromaQpIndexOffset        = %d\n", pTaskDescriptor->chromaQpIndexOffset);
	dev_dbg(pDriverData->dev, "|-entropyCodingMode          = %u (%s)\n", pTaskDescriptor->entropyCodingMode,
	        StringifyEntropyCodingMode(pTaskDescriptor->entropyCodingMode));
	dev_dbg(pDriverData->dev, "|-addrScalingMatrix          = 0x%x\n", pTaskDescriptor->addrScalingMatrix);
	dev_dbg(pDriverData->dev, "|-addrScalingMatrixDir       = 0x%x\n", pTaskDescriptor->addrScalingMatrixDir);
	dev_dbg(pDriverData->dev, "|-addrCabacContextBuffer     = 0x%x\n", pTaskDescriptor->addrCabacContextBuffer);
	dev_dbg(pDriverData->dev, "|-reserved3                  = %u\n", pTaskDescriptor->reserved3);
	dev_dbg(pDriverData->dev, "|-reserved4                  = %u\n", pTaskDescriptor->reserved4);
	dev_dbg(pDriverData->dev, "|-GmvX                       = %d pixels\n", pTaskDescriptor->GmvX);
	dev_dbg(pDriverData->dev, "|-GmvY                       = %d pixels\n", pTaskDescriptor->GmvY);
	dev_dbg(pDriverData->dev, "|-addrRoi                    = 0x%x\n", pTaskDescriptor->addrRoi);
	dev_dbg(pDriverData->dev, "|-addrSliceHeader            = 0x%x\n", pTaskDescriptor->addrSliceHeader);
	dev_dbg(pDriverData->dev, "|-SliceHeaderSizeInBits      = %u bits\n", pTaskDescriptor->SliceHeaderSizeInBits);
	dev_dbg(pDriverData->dev, "|-SliceHeaderOffset0         = %u bits\n", pTaskDescriptor->SliceHeaderOffset0);
	dev_dbg(pDriverData->dev, "|-SliceHeaderOffset1         = %u bits\n", pTaskDescriptor->SliceHeaderOffset1);
	dev_dbg(pDriverData->dev, "|-SliceHeaderOffset2         = %u bits\n", pTaskDescriptor->SliceHeaderOffset2);
	dev_dbg(pDriverData->dev, "|-reserved5                  = 0x%x\n", pTaskDescriptor->reserved5);
	dev_dbg(pDriverData->dev, "|-reserved6                  = 0x%x\n", pTaskDescriptor->reserved6);
	dev_dbg(pDriverData->dev, "|-reserved7                  = %u\n", pTaskDescriptor->reserved7);
	dev_dbg(pDriverData->dev, "|-reserved8                  = %u\n", pTaskDescriptor->reserved8);
	dev_dbg(pDriverData->dev, "|-sliceSynchroEnable         = %u\n", pTaskDescriptor->sliceSynchroEnable);
	dev_dbg(pDriverData->dev, "|-maxSliceNumber             = %u\n", pTaskDescriptor->maxSliceNumber);
	dev_dbg(pDriverData->dev, "|-rgb2YuvYCoeff              = 0x%x\n", pTaskDescriptor->rgb2YuvYCoeff);
	dev_dbg(pDriverData->dev, "|-rgb2YuvUCoeff              = 0x%x\n", pTaskDescriptor->rgb2YuvUCoeff);
	dev_dbg(pDriverData->dev, "|-rgb2YuvVCoeff              = 0x%x\n", pTaskDescriptor->rgb2YuvVCoeff);
	dev_dbg(pDriverData->dev, "|-maxAirIntraMbNb            = %u intra macroblocks/frame\n",
	        pTaskDescriptor->maxAirIntraMbNb);
	dev_dbg(pDriverData->dev, "|-brcNoSkip                  = %u\n", pTaskDescriptor->brcNoSkip);
	dev_dbg(pDriverData->dev, "\n");
}

inline unsigned int HVA_ALIGNED_256(unsigned int address)
{
	address = ((address + 0xff) & 0xffffff00);
	return address;
}

inline unsigned int HVA_SET_BUFFER_AS_SDRAM(unsigned int address)
{
	return address;
}

inline unsigned int HVA_SET_BUFFER_AS_ESRAM(unsigned int address)
{
	address += 1;
	return address;
}

static unsigned int H264HwEncodeBuildHVACommand(HVACmdType_t cmdType, unsigned char clientId, unsigned short taskId)
{
	unsigned int cmd;

	cmd = (unsigned int)(cmdType & 0xFF);
	cmd |= (clientId << 8);
	cmd |= (taskId << 16);

	dev_dbg(pDriverData->dev, "[Launch hva command]\n");
	dev_dbg(pDriverData->dev, "|-cmdType = 0x%x (%s)\n", cmdType, StringifyHvaCmdType(cmdType));
	dev_dbg(pDriverData->dev, "|-clientId = %u\n", clientId);
	dev_dbg(pDriverData->dev, "|-taskId = %u\n", taskId);
	dev_dbg(pDriverData->dev, "|-cmd = 0x%x\n", cmd);
	dev_dbg(pDriverData->dev, "\n");

	return cmd;
}

static H264EncodeHardStatus_t H264HwEncodeLaunchEncodeTask(H264EncodeHardCodecContext_t  *Context)
{
	volatile unsigned int fifoLevel = 0;
	unsigned int  hvaCmd = 0;
	unsigned int status = 0;

	//Build HVA command
	hvaCmd = H264HwEncodeBuildHVACommand(HVA_H264_ENC, Context->clientId, globalTaskId);
	globalTaskId++;
	(Context->instanceTaskId)++;

	status = down_timeout(&h264CmdFifoDepth, msecs_to_jiffies(HVA_COMMAND_TIMEOUT_MS));
	if (status == 0) {
		// Take our hardware lock for register access to the Fifo Queue.
		unsigned long flags;
		spin_lock_irqsave(&platformData.hw_lock, flags);

		// Assert no risk to overflow HVA command queue
		fifoLevel = ReadHvaRegister(HVA_HIF_REG_CFL);

		if ((fifoLevel & LEVEL_SFL_BITS_MASK) < HVA_CMD_FIFO_DEPTH) {
			//First write cmd value
			WriteHvaRegister(HVA_HIF_FIFO_CMD, hvaCmd);
			//Then write task descriptor address
			WriteHvaRegister(HVA_HIF_FIFO_CMD, Context->taskDescriptorPhysAddress);

			h264_frames_queued++; // Record statistics
		} else {
			//TODO Remove this statement as it is unreachable
			// The hardware indicates the semaphore was incorrect
			dev_dbg(pDriverData->dev, "HW Fifo Level indicates invalid entry into command queue\n");

			// We don't release this semaphore here, as we have discovered
			// that we don't actually have enough resource ... so we become
			// ' Self - Limiting ' rather than simply discarding frames.
			// up(&h264CmdFifoDepth);

			status = -EINTR; // Enter the Error Handling below.
		}

		// Release our Hardware Lock
		spin_unlock_irqrestore(&platformData.hw_lock, flags);
	}

	/* This is not an if/else due to the error handling in the above statement */
	if (status != 0) {
		// Failed to insert a command on the hardware queue.
		Context->discardCommandCounter++;
		dev_err(pDriverData->dev, "Error: claim h264CmdFifoDepth semaphore failed\n");
		return H264ENCHARD_CMD_DISCARDED;
	}

	return H264ENCHARD_NO_ERROR;
}

int32_t ComputeBufferFullness(H264EncodeHardCodecContext_t  *Context)
{
	uint32_t pictureSize = 0;
	H264EncodeHardRational_t depletion;
	H264EncodeHardFps_t framerate;
	H264EncodeHardRational_t firstPictDpl;
	H264EncodeHardTaskDescriptor_t *pTaskDescriptor = (H264EncodeHardTaskDescriptor_t *)Context->taskDescriptorHandler;

	dev_dbg(pDriverData->dev, "[Buffer fullnesss input]\n");
	dev_dbg(pDriverData->dev, "|-bitRate             = %u bps\n", pTaskDescriptor->bitRate);
	dev_dbg(pDriverData->dev, "|-framerateNum        = %u\n", pTaskDescriptor->framerateNum);
	dev_dbg(pDriverData->dev, "|-framerateDen        = %u\n", pTaskDescriptor->framerateDen);
	dev_dbg(pDriverData->dev, "|-statusRemovalTime   = %u\n", Context->statusRemovalTime);
	dev_dbg(pDriverData->dev, "|-statusBitstreamSize = %u bytes\n", Context->statusBitstreamSize);
	dev_dbg(pDriverData->dev, "|-nonVCLNALUSize      = %u bytes\n", pTaskDescriptor->nonVCLNALUSize / 8);
	dev_dbg(pDriverData->dev, "\n");

	pictureSize = 8 * Context->statusBitstreamSize + pTaskDescriptor->nonVCLNALUSize + 8 * Context->statusStuffingBits;
	framerate.num = pTaskDescriptor->framerateNum;
	framerate.den = pTaskDescriptor->framerateDen;
	BRC_SET_DEPLETION(depletion, framerate, pTaskDescriptor->bitRate);
	if (pTaskDescriptor->firstPictureInSequence) {
		firstPictDpl.base = depletion.base;
		firstPictDpl.div = depletion.div;
		firstPictDpl.rem = depletion.rem;
		BRC_A_TIMES_N(firstPictDpl, Context->statusRemovalTime);
		BRC_SUB_VALUE_FROM_A(firstPictDpl, pictureSize);
		Context->bufferFullness.base = firstPictDpl.base;
		Context->bufferFullness.div = firstPictDpl.div;
		Context->bufferFullness.rem = firstPictDpl.rem;
		Context->lastRemovalTime = Context->statusRemovalTime;
	} else {
		do {
			BRC_ADD_B_TO_A(Context->bufferFullness, depletion);
			Context->lastRemovalTime++;
		} while (Context->lastRemovalTime < Context->statusRemovalTime);
		BRC_SUB_VALUE_FROM_A(Context->bufferFullness, pictureSize);
	}

	dev_dbg(pDriverData->dev, "Estimated buffer fullness is %d bits\n", BRC_GET_INTEGER_PART(Context->bufferFullness));

	if (Context->transformStatus == HVA_ENCODE_FRAME_SKIPPED) {
		BRC_ADD_VALUE_FROM_A(Context->bufferFullness, pictureSize);
	}

	return BRC_GET_INTEGER_PART(Context->bufferFullness);
}

/////////////////////////////////////////////////////////////////////////////////
//
//Update current context / task descriptor at the end of the encode task
//  *  mv_toggle parameter: should toggle between 0-1 between each frame encode (whatever encode status)
//  *  swap reconstructed and reference buffer if previous encode successful (in particular , no frame skipped)

static void H264HwEncodeUpdateContextAtEncodeEnd(H264EncodeHardCodecContext_t  *Context)
{
	unsigned int tempAddress = 0;
	H264EncodeHardTaskDescriptor_t *pTaskDescriptor = (H264EncodeHardTaskDescriptor_t *)Context->taskDescriptorHandler;

	dev_dbg(pDriverData->dev, "Update context at encode end\n");
	dev_dbg(pDriverData->dev, "idrToggle = %u, mvToggle = %u, lastIdrPicId = %d\n", Context->idrToggle, Context->mvToggle,
	        Context->lastIdrPicId);

	// CODED PICTURE BUFFER FULLNESS
	// If first frame has been encoded in vbr with a coded picture
	// buffer overflow, it could lead to encoding quality drop.
	// TODO re-encode frame by increasing qp until 51
	if (pTaskDescriptor->firstPictureInSequence) {
		int32_t buffer_fullness = ComputeBufferFullness(Context);
		if (buffer_fullness < 0) {
			dev_err(pDriverData->dev, "Error: cpb overflow, buffer fullness is %d bits, qp is %u, cpb size is %u bits\n",
			        buffer_fullness, pTaskDescriptor->quant, pTaskDescriptor->cpbBufferSize);
		} else {
			dev_dbg(pDriverData->dev, "Buffer fullness is %d bits\n", buffer_fullness);
		}
	} else {
		dev_dbg(pDriverData->dev, "Buffer fullness is %d bits\n", ComputeBufferFullness(Context));
	}

	if (Context->transformStatus == HVA_ENCODE_OK) {
		if (Context->FrameParams.pictureCodingType == HVA_ENCODE_I_FRAME) {
			Context->lastIdrPicId = Context->idrToggle;
		} else {
			Context->lastIdrPicId = -1;
		}
	}

	//should swap buffers in task descriptor IF frame not skipped:
	//reconstructed and ref frame Phys address for a given instance!
	if (Context->transformStatus == HVA_ENCODE_OK) {
		dev_dbg(pDriverData->dev, "[Swap rec <=> ref buffers]\n");
		tempAddress = Context->referenceFrameBufPhysAddress;
		Context->referenceFrameBufPhysAddress     = Context->reconstructedFrameBufPhysAddress;
		Context->reconstructedFrameBufPhysAddress = tempAddress;
		dev_dbg(pDriverData->dev, "|-referenceFrameBufPhysAddress     = 0x%x => 0x%x\n",
		        Context->reconstructedFrameBufPhysAddress, Context->referenceFrameBufPhysAddress);
		dev_dbg(pDriverData->dev, "|-reconstructedFrameBufPhysAddress = 0x%x => 0x%x\n", Context->referenceFrameBufPhysAddress,
		        Context->reconstructedFrameBufPhysAddress);
		dev_dbg(pDriverData->dev, "\n");
		pTaskDescriptor->addrFwdRefBuffer         = Context->referenceFrameBufPhysAddress;
		pTaskDescriptor->addrRecBuffer            = Context->reconstructedFrameBufPhysAddress;
	}

	//Flush cache to ensure task descriptor consistency in memory
	OSDEV_FlushCacheRange((void *) pTaskDescriptor, Context->taskDescriptorPhysAddress,
	                      sizeof(H264EncodeHardTaskDescriptor_t));
}

static void H264HwEncodeHVAReset(void)
{
	//volatile unsigned char stBusPlugResetAck=0;
	//volatile unsigned char hvaResetAck=0;

	// Hva soft reset
	dev_dbg(pDriverData->dev, "Perform hva soft reset\n");
	WriteHvaRegister(HVA_HIF_REG_RST, 0x1);
	/*
	do
	{
	     stBusPlugResetAck=ReadHvaRegister(HVA_HIF_REG_RST_ACK);
	}
	while ((stBusPlugResetAck&0x1)!=1);
	dev_dbg(pDriverData->dev, "Receive HVA STBusPlug Reset Ack\n");
	*/
	//FIXME: Got acknowledge for first reset but not after
	//add delay instead of Acknowledge for this IP revision
	udelay(2000);

	// Hva hard reset
	dev_dbg(pDriverData->dev, "Perform hva hard reset\n");
	WriteHvaRegister(HVA_HIF_REG_RST, 0x2);
	udelay(2000); // Add delay as no acknowledge on hva hard reset
}

static void H264HwEncodeHVAStaticConfig(void)
{
	dev_dbg(pDriverData->dev, "Disable clock gating\n");
	WriteHvaRegister(HVA_HIF_REG_CLK_GATING, HVA_HIF_REG_CLK_GATING_HVC_CLK_EN |
	                 HVA_HIF_REG_CLK_GATING_HEC_CLK_EN |
	                 HVA_HIF_REG_CLK_GATING_HJE_CLK_EN);

	dev_dbg(pDriverData->dev, "Define max opcode size and max message size for esram and ddr\n");
	WriteHvaRegister(HVA_HIF_REG_MIF_CFG, HVA_HIF_MIF_CFG_RECOMMENDED_VALUE);
}

// Function to set all hva static registers configuration
void H264HwEncodeSetRegistersConfig(void)
{
	dev_dbg(pDriverData->dev, "Configure hva registers\n");
	H264HwEncodeHVAReset();
	H264HwEncodeHVAStaticConfig();
}

//{{{  H264HwEncodeCheckFramerate
//{{{  doxynote
/// \brief      Check and correct frame rate parameters to prevent overflow (Bug 23112)
///
/// \return     H264EncodeHardStatus_t
//}}}

static void H264HwEncodeCheckFramerate(H264EncodeHardCodecContext_t  *Context)
{
	H264EncodeHardTaskDescriptor_t *pTaskDescriptor = (H264EncodeHardTaskDescriptor_t *)Context->taskDescriptorHandler;
	uint32_t maxBitstreamSize;
	uint32_t i;
	uint32_t BitstreamSize;
	uint32_t framerateNum;
	uint32_t framerateDen;
	uint32_t maxframerateNum;
	uint64_t bitrateNum;
	uint64_t maxbitrateNum;
	uint32_t framerateNumOpt;
	uint32_t framerateDenOpt;
	uint32_t framerateNumRef;
	uint32_t framerateDenRef;
	bool overflowNum, overflowDen;
	uint64_t bitrateDen;
	uint64_t maxbitrateDen;

	// Previous frame rate to limit the frame rate correction frequency
	framerateNum = (uint32_t)Context->globalParameters.framerateNum;
	framerateDen = (uint32_t)Context->globalParameters.framerateDen;

	// Ref frame rate to compute the new optimal frame rate so that we do not lose accuracy in previous rounding
	framerateNumRef = (uint32_t)Context->framerateNumRef;
	framerateDenRef = (uint32_t)Context->framerateDenRef;

	overflowNum = false;
	overflowDen = false;

	dev_dbg(pDriverData->dev, "Framerate is (%d/%d) fps\n", framerateNum, framerateDen);
	dev_dbg(pDriverData->dev, "Reference framerate is (%d/%d) fps\n", framerateNumRef, framerateDenRef);

	// Checking overflow for framerateDen
	bitrateDen = (uint64_t) Context->globalParameters.bitRate * framerateDen;
	maxbitrateDen = 1;
	maxbitrateDen <<= OVERFLOW_LIMIT_DEN;
	maxbitrateDen -= 1;
	if (bitrateDen >= maxbitrateDen) {
		dev_dbg(pDriverData->dev, "Overflow bitrateDen %llu (bitRate %d * framerateDen %d) >= maxbitrateDen %llu\n",
		        bitrateDen, Context->globalParameters.bitRate, framerateDen, maxbitrateDen);
		overflowDen = true;
	}

	if (overflowDen) {

		// optimal accuracy
		framerateDenOpt = (int32_t)maxbitrateDen / (Context->globalParameters.bitRate);
		framerateNumOpt = ((framerateNumRef * framerateDenOpt) + (framerateDenRef >> 1)) / framerateDenRef;

		dev_dbg(pDriverData->dev, "Optimal framerate num is %d\n", framerateNumOpt);
		dev_dbg(pDriverData->dev, "Optimal framerate den is %d\n", framerateDenOpt);
		Context->globalParameters.framerateNum = (uint16_t)framerateNumOpt;
		Context->globalParameters.framerateDen = (uint16_t)framerateDenOpt;

		framerateNum = framerateNumOpt;
		framerateDen = framerateDenOpt;
	}

	// Checking overflow for framerateNum
	maxBitstreamSize = Context->PastBitstreamSize[0];
	for (i = 1; i < 4; i++) {
		if (Context->PastBitstreamSize[i] > maxBitstreamSize) {
			maxBitstreamSize = Context->PastBitstreamSize[i];
		}
	}

	//if(maxBitstreamSize>(1<<22))
	//    dev_err(pDriverData->dev, "Error: Max picture ES size exceeded: maxBitstreamSize %d\n", maxBitstreamSize );

	bitrateNum = (uint64_t) maxBitstreamSize * framerateNum;
	maxbitrateNum = 1;
	maxbitrateNum <<= OVERFLOW_LIMIT_NUM;
	maxbitrateNum -= 1;
	if (bitrateNum >= maxbitrateNum) {
		dev_dbg(pDriverData->dev, "Overflow bitrateNum %llu (maxBitstreamSize %d * framerateNum %d) >= maxbitrateNum %llu\n",
		        bitrateNum, maxBitstreamSize, framerateNum, maxbitrateNum);
		overflowNum = true;
	}

	if (!overflowDen && !overflowNum) {
		pTaskDescriptor->framerateNum = Context->globalParameters.framerateNum;
		pTaskDescriptor->framerateDen = Context->globalParameters.framerateDen;
		return;
	}

	if (overflowNum) {

		// compute number of bit used bitstream
		// simplify to prevent a division > 32 bits
		BitstreamSize = maxBitstreamSize;
		i = 0;
		while (BitstreamSize > 0) {
			BitstreamSize >>= 1;
			i++;
		}
		maxframerateNum = (1 << ((int32_t)OVERFLOW_LIMIT_NUM - i)) - 1;
		//maxframerateNum = maxbitrateNum;
		//do_div(maxframerateNum, maxBitstreamSize);

		dev_dbg(pDriverData->dev, "Max bitstream size is %d (%d bits representation)\n", maxBitstreamSize, i);
		dev_dbg(pDriverData->dev, "Max framerate num is %d vs %d (%d/%d))\n", maxframerateNum,
		        (uint32_t)maxbitrateNum / maxBitstreamSize,
		        (uint32_t)maxbitrateNum, maxBitstreamSize);

		// optimal accuracy
		framerateDenOpt = (framerateDenRef * maxframerateNum) / framerateNumRef;
		framerateNumOpt = ((framerateNumRef * framerateDenOpt) + (framerateDenRef >> 1)) / framerateDenRef;

		dev_dbg(pDriverData->dev, "Optimal framerate num is %d\n", framerateNumOpt);
		dev_dbg(pDriverData->dev, "Optimal framerate den is %d\n", framerateDenOpt);
		Context->globalParameters.framerateNum = (uint16_t)framerateNumOpt;
		Context->globalParameters.framerateDen = (uint16_t)framerateDenOpt;
	}

	pTaskDescriptor->framerateNum = Context->globalParameters.framerateNum;
	pTaskDescriptor->framerateDen = Context->globalParameters.framerateDen;
}

static void H264HwEncodeCheckBitrateAndCpbSize(H264EncodeHardCodecContext_t  *Context)
{
	value_scale_deconstruction_t deconstructed_parameter;
	uint32_t reconstructed_parameter;

	if (Context->globalParameters.vuiParametersPresentFlag) {
		// Compute bit rate
		deconstructed_parameter = H264HwEncodeDeconstructBitrate(Context->bitrateRef);
		reconstructed_parameter = H264HwEncodeReconstructBitrate(deconstructed_parameter.value, deconstructed_parameter.scale);
		dev_dbg(pDriverData->dev, "Bitrate is %d bps, reconstructed bitrate is %d bps\n", Context->bitrateRef,
		        reconstructed_parameter);
		if (reconstructed_parameter != Context->bitrateRef) {
			Context->globalParameters.bitRate = reconstructed_parameter;
			dev_dbg(pDriverData->dev, "Bitrate is %d bps, reconstructed bitrate is %d bps\n", Context->bitrateRef,
			        reconstructed_parameter);
		}

		// Compute cpb buffer size
		deconstructed_parameter = H264HwEncodeDeconstructCpbSize(Context->cpbBufferSizeRef);
		reconstructed_parameter = H264HwEncodeReconstructCpbSize(deconstructed_parameter.value, deconstructed_parameter.scale);
		dev_dbg(pDriverData->dev, "Cpb size is %d bits, reconstructed cpb size is %d bits\n", Context->cpbBufferSizeRef,
		        reconstructed_parameter);
		if (reconstructed_parameter != Context->cpbBufferSizeRef) {
			Context->globalParameters.cpbBufferSize = reconstructed_parameter;
			dev_dbg(pDriverData->dev, "Cpb size is %d bits, reconstructed cpb size is %d bits\n", Context->cpbBufferSizeRef,
			        reconstructed_parameter);
		}
	}
}

static void H264HwEncodeGetFrameDimensionFromProfile(H264EncodeMemoryProfile_t memoryProfile, uint32_t *maxFrameWidth,
                                                     uint32_t *maxFrameHeight)
{
	switch (memoryProfile) {
	case HVA_ENCODE_PROFILE_CIF:
		*maxFrameWidth  = 352;
		*maxFrameHeight = 288;
		break;
	case HVA_ENCODE_PROFILE_SD:
		*maxFrameWidth  = 720;
		*maxFrameHeight = 576;
		break;
	case HVA_ENCODE_PROFILE_720P:
		*maxFrameWidth  = 1280;
		*maxFrameHeight = 720;
		break;
	case HVA_ENCODE_PROFILE_HD:
		*maxFrameWidth  = 1920;
		*maxFrameHeight = 1088;
		break;
	default:
		dev_err(pDriverData->dev, "Error: unknown memory profile, use 1080p frame size\n");
		*maxFrameWidth  = 1920;
		*maxFrameHeight = 1088;
		break;
	}
}
