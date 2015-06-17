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

#include <asm/irq.h>
#include <asm/io.h>
#include <linux/interrupt.h>
#include <linux/delay.h>

#include "osdev_mem.h"

#include "vp8hard.h"
#include "defcfg.h"
#include "dwl.h"

/*------------------------------------------------------------------------------
    Function name   : VP8HwdAsicInit
    Description     :
    Return type     : void
    Argument        : VP8DecContainer_t * pDecCont
------------------------------------------------------------------------------*/
void VP8HwdAsicInit(VP8DecContainer_t *pDecCont)
{

	memset(pDecCont->vp8Regs, 0, sizeof(pDecCont->vp8Regs));

	SetDecRegister(pDecCont->vp8Regs, HWIF_DEC_MODE,
	               pDecCont->decMode == VP8HWD_VP7 ?  DEC_MODE_VP7 : DEC_MODE_VP8);

	/* these parameters are defined in deccfg.h */
	SetDecRegister(pDecCont->vp8Regs, HWIF_DEC_OUT_ENDIAN,
	               DEC_X170_OUTPUT_PICTURE_ENDIAN);

	SetDecRegister(pDecCont->vp8Regs, HWIF_DEC_IN_ENDIAN,
	               DEC_X170_INPUT_DATA_ENDIAN);

	SetDecRegister(pDecCont->vp8Regs, HWIF_DEC_STRENDIAN_E,
	               DEC_X170_INPUT_STREAM_ENDIAN);

	/*
	    SetDecRegister(pDecCont->vp8Regs, HWIF_DEC_OUT_TILED_E,
	                   DEC_X170_OUTPUT_FORMAT);
	                   */

	SetDecRegister(pDecCont->vp8Regs, HWIF_DEC_MAX_BURST,
	               DEC_X170_BUS_BURST_LENGTH);

	if ((DWLReadAsicID() >> 16) == 0x8170U) {
		SetDecRegister(pDecCont->vp8Regs, HWIF_PRIORITY_MODE,
		               DEC_X170_ASIC_SERVICE_PRIORITY);
	} else {
		SetDecRegister(pDecCont->vp8Regs, HWIF_DEC_SCMD_DIS,
		               DEC_X170_SCMD_DISABLE);
	}

	DEC_SET_APF_THRESHOLD(pDecCont->vp8Regs);

	SetDecRegister(pDecCont->vp8Regs, HWIF_DEC_LATENCY,
	               DEC_X170_LATENCY_COMPENSATION);

	SetDecRegister(pDecCont->vp8Regs, HWIF_DEC_DATA_DISC_E,
	               DEC_X170_DATA_DISCARD_ENABLE);

	SetDecRegister(pDecCont->vp8Regs, HWIF_DEC_OUTSWAP32_E,
	               DEC_X170_OUTPUT_SWAP_32_ENABLE);

	SetDecRegister(pDecCont->vp8Regs, HWIF_DEC_INSWAP32_E,
	               DEC_X170_INPUT_DATA_SWAP_32_ENABLE);

	SetDecRegister(pDecCont->vp8Regs, HWIF_DEC_STRSWAP32_E,
	               DEC_X170_INPUT_STREAM_SWAP_32_ENABLE);

#if( DEC_X170_HW_TIMEOUT_INT_ENA  != 0)
	SetDecRegister(pDecCont->vp8Regs, HWIF_DEC_TIMEOUT_E, 1);
#else
	SetDecRegister(pDecCont->vp8Regs, HWIF_DEC_TIMEOUT_E, 0);
#endif

#if( DEC_X170_INTERNAL_CLOCK_GATING != 0)
	SetDecRegister(pDecCont->vp8Regs, HWIF_DEC_CLK_GATE_E, 1);
#else
	SetDecRegister(pDecCont->vp8Regs, HWIF_DEC_CLK_GATE_E, 0);
#endif

#if( DEC_X170_USING_IRQ  == 0)
	SetDecRegister(pDecCont->vp8Regs, HWIF_DEC_IRQ_DIS, 1);
#else
	SetDecRegister(pDecCont->vp8Regs, HWIF_DEC_IRQ_DIS, 0);
#endif

	/* set AXI RW IDs */
	SetDecRegister(pDecCont->vp8Regs, HWIF_DEC_AXI_RD_ID,
	               (DEC_X170_AXI_ID_R & 0xFFU));
	SetDecRegister(pDecCont->vp8Regs, HWIF_DEC_AXI_WR_ID,
	               (DEC_X170_AXI_ID_W & 0xFFU));

}

irqreturn_t VP8InterruptHandler(int irq, void *data)
{
	DWLClearSwRegister(data);
	return IRQ_HANDLED;
}

VP8DecRet Vp8HardDeInit(const void *data)
{
	int ret;
	VP8DecRet status = VP8DEC_OK;
	free_irq(VP8_INTERRUPT_LINE, (void *)data);
	ret = DWLRelease(data);
	if (ret != DWL_OK) {
		pr_err("Error in DWL release operation (%d)\n", ret);
		status = VP8DEC_DWL_ERROR;
	}
	return status;
}

/*------------------------------------------------------------------------------
    Function name   : Vp8HardInit
    Description     :
    Return type     : VP8DecRet
    Argument        : VP8DecInst * pDecInst
                      unsigned int useVideoFreezeConcealment
------------------------------------------------------------------------------*/
VP8DecRet Vp8HardInit(Vp8HardInitParams_t *InitParams)
{
	unsigned int i;
	int ret;
	hX170dwl_config_t config;
	VP8DecContainer_t *pDecCont ;

	/* check that right shift on negative numbers is performed signed */
	/*lint -save -e* following check causes multiple lint messages */
#if (((-1) >> 1) != (-1))
#error Right bit-shifting (>>) does not preserve the sign
#endif

	/* check that decoding supported in HW */

	hX170dwl_config_t hwCfg;
	DWLReadAsicConfig(&hwCfg);
	if ((InitParams->decFormat == VP8DEC_VP8 || InitParams->decFormat == VP8DEC_WEBP) &&
	    !hwCfg.vp8Support) {
		pr_err("Error: %s VP8 not supported in HW\n", __func__);
		return VP8DEC_FORMAT_NOT_SUPPORTED;
	}

	/* allocate instance */
	pDecCont = &InitParams->decInstance;

	pDecCont->dwl = DWLInit(DWL_CLIENT_TYPE_VP8_DEC);

	/* request_irq: allocate a given interrupt line */
	ret = request_irq(VP8_INTERRUPT_LINE, VP8InterruptHandler, IRQF_DISABLED, "VP8_irq", (void *)pDecCont->dwl);
	if (ret < 0) {
		pr_err("Error: registering interrupt line failed (%d)\n", ret);
		DWLRelease(pDecCont->dwl);
		return VP8DEC_INITFAIL;
	}
	pr_debug("Call to request_irq is OK (%d)\n", ret);

	/* initial setup of instance */
	pDecCont->decStat = VP8DEC_INITIALIZED;
	pDecCont->checksum = (void *) pDecCont; /* save instance as a checksum */

	pDecCont->width = InitParams->CodedWidth;
	pDecCont->height = InitParams->CodedHeight;
	pDecCont->asicBuff[0].width = InitParams->CodedWidth;
	pDecCont->asicBuff[0].height = InitParams->CodedHeight;
	if (InitParams->numFrameBuffers > 16) {
		pr_err("Error: %s Warning : Too much frame buffers (forced to 16)\n", __func__);
		InitParams->numFrameBuffers = 16;
	}

	switch (InitParams->decFormat) {
	case VP8DEC_VP8:
		pDecCont->decMode = pDecCont->decoder.decMode = VP8HWD_VP8;
		if (InitParams->numFrameBuffers < 4) {
			InitParams->numFrameBuffers = 4;
		}
		break;
	case VP8DEC_WEBP:
		pDecCont->decMode = pDecCont->decoder.decMode = VP8HWD_VP8;
		pDecCont->intraOnly = HANTRO_TRUE;
		InitParams->numFrameBuffers = 1;
		break;
	default :
		break;
	}
	pDecCont->numBuffers = InitParams->numFrameBuffers;
	VP8HwdAsicInit(pDecCont);   /* Init ASIC */

	if (VP8HwdAsicAllocateMem(pDecCont) != 0) {
		pr_err("Error: %s ASIC Memory allocation failed\n", __func__);
		return VP8DEC_MEMFAIL;
	}

	DWLReadAsicConfig(&config);

	i = DWLReadAsicID() >> 16;
	pr_info("ASIC ID is : 0x%x\n", i);
	if (i == 0x8170U) {
		InitParams->useVideoFreezeConcealment = 0;
	}
	pDecCont->refBufSupport = config.refBufSupport;
	if (InitParams->referenceFrameFormat == DEC_REF_FRM_TILED_DEFAULT) {
		/* Assert support in HW before enabling.. */
		if (!config.tiledModeSupport) {
			pr_err("Error: %s Tiled reference picture format not supported in HW\n", __func__);
			return VP8DEC_FORMAT_NOT_SUPPORTED;
		}
		pDecCont->tiledModeSupport = config.tiledModeSupport;
	} else {
		pDecCont->tiledModeSupport = 0;
	}

	pDecCont->intraFreeze = InitParams->useVideoFreezeConcealment;
	pDecCont->pictureBroken = 0;
	pDecCont->decoder.refbuPredHits = 0;

	return VP8DEC_OK;
}


/*------------------------------------------------------------------------------
    Function name   : VP8HwdAsicAllocateMem
    Description     :
    Return type     : int
    Argument        : VP8DecContainer_t * pDecCont
------------------------------------------------------------------------------*/
int VP8HwdAsicAllocateMem(VP8DecContainer_t *pDecCont)
{

//    const void *dwl = pDecCont->dwl;
//    int dwl_ret;
	DecAsicBuffers_t *pAsicBuff = pDecCont->asicBuff;
	unsigned long tempPhysAddr = 0;
	unsigned long tempVirtualAddr = 0;
	int numMbs = 0;
	int memorySize = 0;


	if (pAsicBuff->probTbl.busAddress || pAsicBuff->probTbl.virtualAddress) {
		OSDEV_AllignedFreeHwBuffer(pAsicBuff->probTbl.virtualAddress, BPA_PARTITION_NAME);
	}
	tempVirtualAddr = (unsigned int)OSDEV_AllignedMallocHwBuffer(PROB_TABLE_BUFFER_ALIGNMENT,
	                                                             PROB_TABLE_SIZE, BPA_PARTITION_NAME, &tempPhysAddr);

	if (tempVirtualAddr == 0) {
		pr_err("Error: %s Unable to allocate memory for PROB_TABLE\n", __func__);
		return -1;
	}

	pAsicBuff->probTbl.busAddress = (unsigned int) tempPhysAddr;
	pAsicBuff->probTbl.virtualAddress = (unsigned int *) tempVirtualAddr;
	pAsicBuff->probTbl.size = PROB_TABLE_SIZE;

	SetDecRegister(pDecCont->vp8Regs, HWIF_VP6HWPROBTBL_BASE,
	               pAsicBuff->probTbl.busAddress);
	/*Now Allocate Segmentation Map buffer*/

	if (pAsicBuff->segmentMap.busAddress ||  pAsicBuff->segmentMap.virtualAddress) {
		OSDEV_AllignedFreeHwBuffer(pAsicBuff->segmentMap.virtualAddress, BPA_PARTITION_NAME);
	}
	numMbs = (pDecCont->width >> 4) * (pDecCont->height >> 4);
	memorySize = (numMbs + 3) >> 2; /* We fit 4 MBs on data into every full byte */
	memorySize = 64 * ((memorySize + 63) >> 6); /* Round up to next multiple of 64 bytes */
	tempVirtualAddr = (unsigned int)OSDEV_AllignedMallocHwBuffer(SEGMENTATION_MAP_BUFFER_ALIGNMENT,
	                                                             memorySize, BPA_PARTITION_NAME, &tempPhysAddr);

	if (tempVirtualAddr == 0) {
		pr_err("Error: %s Unable to allocate memory for Segmentation Map buffer\n", __func__);
		OSDEV_AllignedFreeHwBuffer(pAsicBuff->probTbl.virtualAddress, BPA_PARTITION_NAME);
		return -1;
	}

	pAsicBuff->segmentMap.busAddress = (unsigned int) tempPhysAddr;
	pAsicBuff->segmentMap.virtualAddress = (unsigned int *) tempVirtualAddr;
	pAsicBuff->segmentMap.size = memorySize;
	pAsicBuff->segmentMapSize = memorySize;

	SetDecRegister(pDecCont->vp8Regs, HWIF_SEGMENT_BASE,
	               pAsicBuff->segmentMap.busAddress);
	return 0;

}

void VP8HwdAsicFreeMem(VP8DecContainer_t *pDecCont)
{
	int i;
	DecAsicBuffers_t *pAsicBuff = pDecCont->asicBuff;

	if (pAsicBuff->probTbl.busAddress || pAsicBuff->probTbl.virtualAddress) {
		OSDEV_AllignedFreeHwBuffer(pAsicBuff->probTbl.virtualAddress, BPA_PARTITION_NAME);
	}

	if (pAsicBuff->segmentMap.busAddress ||  pAsicBuff->segmentMap.virtualAddress) {
		OSDEV_AllignedFreeHwBuffer(pAsicBuff->segmentMap.virtualAddress, BPA_PARTITION_NAME);
	}

	for (i = 0; i < 16; i++) {
		if (pAsicBuff->pictures[i].virtualAddress) {
			OSDEV_AllignedFreeHwBuffer(pAsicBuff->pictures[i].virtualAddress, BPA_PARTITION_NAME);
		}
	}
}

