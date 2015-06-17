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

#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/syscalls.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kthread.h>
#include <asm/uaccess.h>

#include <linux/fs.h>
#include <linux/ioport.h>

#include <mme.h>


#include "VP8_hard_VideoTransformer.h"
#include "VP8_hard_VideoTransformerTypes.h"
#include "vp8hard.h"
#include "vp8hard_decode.h"

#define MODULE_NAME     "VP8 hardware host transformer"

MODULE_DESCRIPTION("VP8 decode hardware cell platform driver");
MODULE_AUTHOR("STMicroelectronics");
MODULE_LICENSE("Proprietary");

//#define DEBUG_VIDEO 1

static int  __init      StmLoadModule(void);
static void __exit      StmUnloadModule(void);
static char *mmeName = VP8DECHW_MME_TRANSFORMER_NAME;

module_init(StmLoadModule);
module_exit(StmUnloadModule);

module_param(mmeName , charp, 0000);
MODULE_PARM_DESC(mmeName, "Name to use for MME Transformer registration");

/*static Vp8HardSequenceParams_t  *sequenceParams;*/

static int __init StmLoadModule(void)
{
	MME_ERROR status;
	int ret = 0;

	ret = hx170_check_chip_id();
	if (ret) {
		pr_err("Error: %s Failed to check chip_id\n", __func__);
		status = MME_INTERNAL_ERROR;
		goto fail_chip;
	}

	// Now do the MME Init
	status =  MME_RegisterTransformer(
	                  mmeName,
	                  VP8DEC_Abort,
	                  VP8DEC_GetTransformerCapability,
	                  VP8DEC_InitTransformer,
	                  VP8DEC_ProcessCommand,
	                  VP8DEC_TermTransformer);

	if (status == MME_SUCCESS) {
		pr_info("%s loaded\n", MODULE_NAME);
	} else {
		pr_err("Error registering %s with MME (%d)\n", MODULE_NAME, status);
	}
fail_chip:
	return status;
}

MME_ERROR VP8DEC_Abort(void *context, MME_CommandId_t cmdId)
{
	return MME_SUCCESS;
}

MME_ERROR VP8DEC_GetTransformerCapability(MME_TransformerCapability_t *capability)
{
	VP8_CapabilityParams_t *cap;

	/*
	typedef struct MME_TransformerCapability_t {
	            MME_UINT             StructSize;
	            MME_UINT             Version;
	            MME_DataFormat_t     InputType;
	            MME_DataFormat_t     OutputType;
	            MME_UINT             TransformerInfoSize;
	            MME_GenericParams_t  TransformerInfo_p;
	} MME_TransformerCapability_t;
	*/

	cap = (VP8_CapabilityParams_t *)capability->TransformerInfo_p;

	capability->Version = 0xF000F000;

	/*TO DO : add a check on the hardware version (to be sure that google IP exist on SOC)*/
	cap->IsHantroG1Here = 1;
	capability->TransformerInfoSize = sizeof(VP8_CapabilityParams_t);

	return MME_SUCCESS;
}

MME_ERROR VP8DEC_InitTransformer(MME_UINT paramsSize, MME_GenericParams_t params, void **context)
{
	Vp8HardInitParams_t  *HardInitParams_p;
	VP8_InitTransformerParam_t *initparams = (VP8_InitTransformerParam_t *)params;
	Vp8HardStatus_t status;

	/* allocate the memory for the HardwareInitParams structure ... */
	HardInitParams_p = kzalloc(sizeof(Vp8HardInitParams_t), GFP_KERNEL);
	if (!HardInitParams_p) {
		pr_err("Error: %s alloc failed\n", __func__);
		return MME_NOMEM;
	}

	HardInitParams_p->decFormat = VP8DEC_VP8;
	HardInitParams_p->numFrameBuffers = 4;
	HardInitParams_p->referenceFrameFormat = DEC_REF_FRM_RASTER_SCAN;
	HardInitParams_p->useVideoFreezeConcealment = 0;
	HardInitParams_p->CodedWidth = initparams->CodedWidth;
	HardInitParams_p->CodedHeight = initparams->CodedHeight;

	status = Vp8HardInit(HardInitParams_p);
	if (status != VP8HARD_NO_ERROR) {
		pr_err("Error: %s Vp8HardInit failed\n", __func__);
	} else {
		pr_debug("Succeed to initialize the VP8 Hardware Decoder");
	}

	*context = (void *)HardInitParams_p;
	return MME_SUCCESS;
}

MME_ERROR VP8DEC_ProcessCommand(void *handle, MME_Command_t *CmdInfo_p)
{
	/*
	typedef struct MME_Command_t {
	            MME_UINT                  StructSize;
	            MME_CommandCode_t         CmdCode;
	            MME_CommandEndType_t      CmdEnd;
	            MME_Time_t                DueTime;
	            MME_UINT                  NumberInputBuffers;
	            MME_UINT                  NumberOutputBuffers;
	            MME_DataBuffer_t**        DataBuffers_p;
	            MME_CommandStatus_t       CmdStatus;
	            MME_UINT                  ParamSize;
	            MME_GenericParams_t       Param_p;
	} MME_Command_t;
	*/
	char *VP8_BitBufferStart; /* Pointer to the start of  Bit Buffer containing the compressed frames */
	char *VP8_BitStreamPointer; /* Pointer to the Current frame's compressed data inside  Bit Buffer */

	unsigned int  VP8_BitBufferSize;        /* total length of the Bit Buffer */

	Vp8HardInitParams_t  *HardInitParams_p;
	VP8DecInput *pInput;

	VP8_TransformParam_t         *Transformer_Struct_p;
	VP8_ParamPicture_t           Picture_Data_Struct_p;

	MME_CommandStatus_t *CmdStatus = &CmdInfo_p->CmdStatus;

	CmdStatus->State = MME_COMMAND_EXECUTING;

	HardInitParams_p = (Vp8HardInitParams_t *)handle;
	pInput = &HardInitParams_p->decIn;

	switch (CmdInfo_p->CmdCode) {
	case MME_TRANSFORM:
		HardInitParams_p  = (Vp8HardInitParams_t *)handle;
		Transformer_Struct_p = (VP8_TransformParam_t *)CmdInfo_p->Param_p;
		Picture_Data_Struct_p = Transformer_Struct_p->PictureParameters;
		/* Pointer to the Bit Buffer */
		VP8_BitBufferStart = (char *) Transformer_Struct_p->CodedData_Cp;
		VP8_BitBufferSize  = Transformer_Struct_p->CodedDataSize ;
		/* VP8_BitBufferEnd  = (char*)((unsigned int)VP8_BitBufferStart + VP8_BitBufferSize - 1); */

		/* Pointer to the compressed frame */
		VP8_BitStreamPointer = (char *)VP8_BitBufferStart;
		pInput->dataLen = VP8_BitBufferSize ;
		pInput->streamBusAddress = (unsigned int) Transformer_Struct_p->CodedData;
		pInput->pStream = (char *)VP8_BitStreamPointer;

#if defined(DEBUG_VIDEO)

		/* Frame Buffer where to decode */
		pInput->DataBuffers.picBufferBusAddressY = (unsigned int)Transformer_Struct_p->CurrentFB_Luma_p;
		pInput->DataBuffers.pPicBufferY = (unsigned int *)ioremap_nocache((unsigned int)
		                                                                  pInput->DataBuffers.picBufferBusAddressY,
		                                                                  Transformer_Struct_p->FrameBuffer_Size);

		pInput->DataBuffers.pPicBufferC = (unsigned int *)(((unsigned int)pInput->DataBuffers.pPicBufferY) +
		                                                   (HardInitParams_p->CodedWidth * HardInitParams_p->CodedHeight));
		pInput->DataBuffers.picBufferBusAddressC = Transformer_Struct_p->CurrentFB_Chroma_p;

		/* previous reference frame */
		pInput->DataBuffers.LastpicBufferBusAddressY = (unsigned int)Transformer_Struct_p->LastRefFB_Luma_p;
		pInput->DataBuffers.pLastPicBufferY = (unsigned int *)ioremap_nocache((unsigned int)
		                                                                      pInput->DataBuffers.LastpicBufferBusAddressY, Transformer_Struct_p->FrameBuffer_Size);

		pInput->DataBuffers.pLastPicBufferC = (unsigned int *) 0;
		pInput->DataBuffers.LastpicBufferBusAddressC = Transformer_Struct_p->LastRefFB_Chroma_p;

		/* golden reference frame  */
		pInput->DataBuffers.GoldpicBufferBusAddressY = Transformer_Struct_p->GoldenFB_Luma_p;
		pInput->DataBuffers.pGoldPicBufferY = (unsigned int *)ioremap_nocache((unsigned int)
		                                                                      pInput->DataBuffers.GoldpicBufferBusAddressY, Transformer_Struct_p->FrameBuffer_Size);

		pInput->DataBuffers.pGoldPicBufferC = (unsigned int *) 0;
		pInput->DataBuffers.GoldpicBufferBusAddressC = Transformer_Struct_p->GoldenFB_Chroma_p;

		/* alternate reference frame */
		pInput->DataBuffers.AltpicBufferBusAddressY = Transformer_Struct_p->AlternateFB_Luma_p;
		pInput->DataBuffers.pAltPicBufferY = (unsigned int *)ioremap_nocache((unsigned int)
		                                                                     pInput->DataBuffers.AltpicBufferBusAddressY, Transformer_Struct_p->FrameBuffer_Size);

		pInput->DataBuffers.pAltPicBufferC = (unsigned int *) 0;
		pInput->DataBuffers.AltpicBufferBusAddressC = Transformer_Struct_p->AlternateFB_Chroma_p;

#else/*defined(DEBUG_VIDEO)*/

		/* Frame Buffer where to decode */
		pInput->DataBuffers.picBufferBusAddressY = (unsigned int)Transformer_Struct_p->CurrentFB_Luma_p;
		pInput->DataBuffers.pPicBufferY = (unsigned int *)
		                                  Transformer_Struct_p->CurrentFB_Luma_p; /*Put the physical @ : HW don't use the mapped @, so we will not map it*/

		pInput->DataBuffers.pPicBufferC = (unsigned int *)
		                                  Transformer_Struct_p->CurrentFB_Chroma_p; /*Put the physical @ : HW don't use the mapped @, so we will not map it*/
		pInput->DataBuffers.picBufferBusAddressC = Transformer_Struct_p->CurrentFB_Chroma_p;

		/* previous reference frame */
		pInput->DataBuffers.LastpicBufferBusAddressY = (unsigned int)Transformer_Struct_p->LastRefFB_Luma_p;
		pInput->DataBuffers.pLastPicBufferY = (unsigned int *)
		                                      Transformer_Struct_p->LastRefFB_Luma_p; /*Put the physical @ : HW don't use the mapped @, so we will not map it*/

		pInput->DataBuffers.pLastPicBufferC = (unsigned int *) 0;
		pInput->DataBuffers.LastpicBufferBusAddressC =
		        Transformer_Struct_p->LastRefFB_Chroma_p;/*will not be used by the HW : Only the Luma pointer is set in the swRegs*/

		/* golden reference frame  */
		pInput->DataBuffers.GoldpicBufferBusAddressY = Transformer_Struct_p->GoldenFB_Luma_p;
		pInput->DataBuffers.pGoldPicBufferY = (unsigned int *)
		                                      Transformer_Struct_p->GoldenFB_Luma_p; /*Put the physical @ : HW don't use the mapped @, so we will not map it*/

		pInput->DataBuffers.pGoldPicBufferC = (unsigned int *) 0;
		pInput->DataBuffers.GoldpicBufferBusAddressC =
		        Transformer_Struct_p->GoldenFB_Chroma_p;/*will not be used by the HW : Only the Luma pointer is set in the swRegs*/

		/* alternate reference frame */
		pInput->DataBuffers.AltpicBufferBusAddressY = Transformer_Struct_p->AlternateFB_Luma_p;
		pInput->DataBuffers.pAltPicBufferY = (unsigned int *)
		                                     Transformer_Struct_p->AlternateFB_Luma_p; /*Put the physical @ : HW don't use the mapped @, so we will not map it*/

		pInput->DataBuffers.pAltPicBufferC = (unsigned int *) 0;
		pInput->DataBuffers.AltpicBufferBusAddressC =
		        Transformer_Struct_p->AlternateFB_Chroma_p; /*will not be used by the HW : Only the Luma pointer is set in the swRegs*/
#endif/*defined(DEBUG_VIDEO)*/

		/* DecResult =*/ VP8DecDecode(&HardInitParams_p->decInstance, &Picture_Data_Struct_p, pInput);

#if defined(DEBUG_VIDEO)&&defined(DUMP_PICTURES)
		/*Dump pics after each decode*/
		unsigned int header[6];
		unsigned int width = HardInitParams_p->CodedWidth;
		unsigned int height = HardInitParams_p->CodedHeight;
		unsigned int format = 0xb0;
		unsigned int widthAligned;

		header[1] = (unsigned int)format & 0xffff;
		header[2] = width;
		header[3] = height;

		header[0] = (0x420F << 16) | 0x6;
		header[1] |= 0x07110000;
		widthAligned = ((width + 31) >> 5) << 5;
		header[2] |= (widthAligned << 16);

		header[4] = width * height;
		header[5] = width * height * 0.5;

		struct file *filp;
		filp = filp_open("decoded_picture.gam", O_CREAT | O_RDWR, 0644);
		if (IS_ERR(filp)) {
			pr_alert("Oops ! filp_open failed\n");
		} else {
			mm_segment_t old_fs = get_fs();
			set_fs(get_ds());

			filp->f_op->write(filp, (char *) header, 24, &filp->f_pos);
			filp->f_op->write(filp, pInput->DataBuffers.pPicBufferY, header[4], &filp->f_pos);
			filp->f_op->write(filp, pInput->DataBuffers.pPicBufferC, header[5], &filp->f_pos);

			set_fs(old_fs);
			filp_close(filp, NULL);
			pr_alert("File Operations END\n");
		}
#endif /*defined(DEBUG_VIDEO)*/

#if defined(DEBUG_VIDEO)
		/*Now Unmap used Buffers*/
		iounmap((void *)pInput->DataBuffers.pPicBufferY);
		iounmap((void *)pInput->DataBuffers.pLastPicBufferY);
		iounmap((void *)pInput->DataBuffers.pGoldPicBufferY);
		iounmap((void *)pInput->DataBuffers.pAltPicBufferY);
#endif
		break;

	case MME_SET_GLOBAL_TRANSFORM_PARAMS:
		pr_err("Error: %s Should not receive a global transform command for VP8 \n", __func__);

		break;

	case MME_SEND_BUFFERS:
		break;

	default:
		break;
	}

	return MME_SUCCESS;
}

MME_ERROR VP8DEC_TermTransformer(void *context)
{
	Vp8HardStatus_t status;
	Vp8HardInitParams_t *param = (Vp8HardInitParams_t *)context;

	VP8HwdAsicFreeMem(&param->decInstance);

	status = Vp8HardDeInit(param->decInstance.dwl);
	if (status != VP8HARD_NO_ERROR) {
		pr_err("Error: %s Vp8HardDeInit failed\n", __func__);
	} else {
		pr_debug("Succeed to deinitialize the VP8 Hardware Decoder\n");
	}

	kfree(context);
	context = NULL;

	return MME_SUCCESS;
}

static void __exit StmUnloadModule(void)
{
	MME_ERROR status = MME_SUCCESS;

	status = MME_DeregisterTransformer(mmeName);
	if (status != MME_SUCCESS) {
		pr_err("Error unregistering %s with MME (%d)\n", MODULE_NAME, status);
	}
	pr_info("Module unloaded\n");
}
