/************************************************************************
Copyright (C) 2007 STMicroelectronics. All Rights Reserved.

This file is part of the Player2 Library.

Player2 is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License version 2 as published by the
Free Software Foundation.

Player2 is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with player2; see the file COPYING.  If not, write to the Free Software
Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

The Player2 Library may alternatively be licensed under a proprietary
license from ST.

Source file name : mpeg2_hard_host_transformer.c

MME Host transformer mapping to MPEG2 hardware on STx71XX

Date        Modification                                    Name
----        ------------                                    --------
04-Jun-07   Created                                         Chris

************************************************************************/

#include <linux/sched.h>
#include <linux/syscalls.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kthread.h>
#include <asm/uaccess.h>

#include "mme.h"
#include "osdev_device.h"
#include "registers.h"
#include "mpeg2hard.h"
#include "stddefs.h"
#include "MPEG2_VideoTransformerTypes.h"

#define MODULE_NAME     "STx71XX MPEG2 hardware host transformer"

static int  __init      StmLoadModule (void);
static void __exit      StmUnloadModule (void);
static char* mmeName = MPEG2_MME_TRANSFORMER_NAME;

module_init             (StmLoadModule);
module_exit             (StmUnloadModule);

MODULE_DESCRIPTION      ("Host transformer driver for STB71XX MPEG2 hardware cell.");
MODULE_AUTHOR           ("Chris");
MODULE_LICENSE          ("GPL");

module_param( mmeName , charp, 0000);
MODULE_PARM_DESC(mmeName, "Name to use for MME Transformer registration");

static MME_ERROR abortCmd(void *context, MME_CommandId_t cmdId);
static MME_ERROR getTransformerCapability(MME_TransformerCapability_t *capability);
static MME_ERROR initTransformer(MME_UINT paramsSize, MME_GenericParams_t params, void **context);
static MME_ERROR processCommand(void *context, MME_Command_t *cmd);
static MME_ERROR termTransformer(void* context);

void copyFrameParameters(Mpeg2HardFrameParams_t  *frameParamsOut, MPEG2_TransformParam_t* frameParamsIn);
void copySequenceParameters(Mpeg2HardSequenceParams_t* seqParamsOut, MPEG2_SetGlobalParamSequence_t* seqParamsIn);


static int quantizationMatrixZigZagOrder[QUANTISER_MATRIX_SIZE] =
{	
	 0,  1,  5,  6, 14, 15, 27, 28, 
	 2,  4,  7, 13, 16, 26, 29, 42,
	 3,  8, 12, 17, 25, 30, 41, 43,
	 9, 11, 18, 24, 31, 40, 44, 53,
	10, 19, 23, 32, 39, 45, 52, 54,
	20, 22, 33, 38, 46, 51, 55, 60,
	21, 34, 37, 47, 50, 56, 59, 61,
	35, 36,	48, 49, 57, 58, 62, 63
};

unsigned char* intraQuantizerMatrix;
unsigned char* nonIntraQuantizerMatrix;

static int __init StmLoadModule (void)
{
	MME_ERROR status = MME_SUCCESS;
	
	// Now do the MME Init	
	status =  MME_RegisterTransformer(
					  mmeName,
					  abortCmd,
					  getTransformerCapability,
					  initTransformer,
					  processCommand,
					  termTransformer);
	
	if (status == MME_SUCCESS)
		OSDEV_Print("%s loaded\n",MODULE_NAME);
	else
		OSDEV_Print("Error registering %s with MME (%d)\n",MODULE_NAME,status);
	
	return status;
}

static MME_ERROR abortCmd(void *context, MME_CommandId_t cmdId)
{
	return MME_SUCCESS;
}

MPEG2_TransformerCapability_t cap;

static MME_ERROR getTransformerCapability(MME_TransformerCapability_t *capability)
{	
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
	
	capability->Version = 0xBEEFF00D;
	cap.MPEG1Capability = 0x1;
	cap.StructSize = sizeof(MPEG2_TransformerCapability_t);
	capability->TransformerInfo_p = &cap;
	capability->TransformerInfoSize = sizeof(MPEG2_TransformerCapability_t);
	
	return MME_SUCCESS;
}

static MME_ERROR initTransformer(MME_UINT paramsSize, MME_GenericParams_t params, void **context)
{
	Mpeg2HardHandle_t      Context;
	Mpeg2HardInitParams_t*  InitParams = 0;
	
	Mpeg2HardStatus_t status;
	status = Mpeg2HardInit("wibble",InitParams,&Context);

	intraQuantizerMatrix    = (unsigned char*)OSDEV_Malloc(sizeof(unsigned char) *64);
	nonIntraQuantizerMatrix = (unsigned char*)OSDEV_Malloc(sizeof(unsigned char) *64);
	
	Context = OSDEV_Malloc(sizeof(Mpeg2HardCodecContext_t));
	
	*context = (void*)Context;
	
	return MME_SUCCESS;
}

static Mpeg2HardFrameParams_t   frameParams;
static Mpeg2HardSequenceParams_t  sequenceParams;


static MME_ERROR processCommand(void *context, MME_Command_t *cmd)
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
	
	sequenceParams.intraQuantizerMatrix = intraQuantizerMatrix;
	sequenceParams.nonIntraQuantizerMatrix = nonIntraQuantizerMatrix;

	switch (cmd->CmdCode) 
	{
	case MME_TRANSFORM:
		copyFrameParameters(&frameParams,(MPEG2_TransformParam_t*)cmd->Param_p);
		Mpeg2HardDecodeFrame( (Mpeg2HardHandle_t)context, &frameParams );
		break;
			
	case MME_SET_GLOBAL_TRANSFORM_PARAMS:
		copySequenceParameters(&sequenceParams, (MPEG2_SetGlobalParamSequence_t*) cmd->Param_p);
		Mpeg2HardSetSequenceParams( (Mpeg2HardHandle_t)context, &sequenceParams );
		break;
		
	case MME_SEND_BUFFERS:
		break;
		
	default:
		break;
	}
		
	return MME_SUCCESS;
}

static MME_ERROR termTransformer(void* context)
{
	OSDEV_Free(intraQuantizerMatrix);
	OSDEV_Free(nonIntraQuantizerMatrix);
	OSDEV_Free(context);
	
	return MME_SUCCESS;
}

void copyFrameParameters(Mpeg2HardFrameParams_t  *frameParamsOut, MPEG2_TransformParam_t* frameParamsIn)
{

//	OSDEV_Print("copyFrameParameters\n");	
/*
 U32                          StructSize;
 MPEG2_CompressedData_t       PictureStartAddrCompressedBuffer_p;
 MPEG2_CompressedData_t       PictureStopAddrCompressedBuffer_p;
 MPEG2_DecodedBufferAddress_t DecodedBufferAddress;
 MPEG2_RefPicListAddress_t    RefPicListAddress;
 MPEG2_MainAuxEnable_t        MainAuxEnable;
 MPEG2_HorizontalDeciFactor_t HorizontalDecimationFactor;
 MPEG2_VerticalDeciFactor_t   VerticalDecimationFactor;
 MPEG2_DecodingMode_t         DecodingMode;
 MPEG2_AdditionalFlags_t      AdditionalFlags;
 MPEG2_ParamPicture_t         PictureParameters;
 */ 

/* Mpeg2HardFrameParams_t
 unsigned char               *compressedDataFrame;
 unsigned int                 compressedDataSize;
 unsigned char               *lumaDecodeFramebuffer;                 // 32 bit aligned
 unsigned char               *chromaDecodeFramebuffer;
 unsigned char               *lumaBackwardReferenceFrame;            // 32 bit aligned
 unsigned char               *chromaBackwardReferenceFrame;
 unsigned char               *lumaForwardReferenceFrame;             // 32 bit aligned
 unsigned char               *chromaForwardReferenceFrame;
 unsigned char                pictureCodingType;                     // 3 bits
 unsigned char                forwardHorizontalMotionVector;         // 4 bits
 unsigned char                forwardVerticalMotionVector;           // 4 bits
 unsigned char                backwardHorizontalMotionVector;        // 4 bits
 unsigned char                backwardVerticalMotionVector;          // 4 bits
 unsigned char                intraDCPrecision;                      // 2 bits
 unsigned char                pictureStructure;                      // 2 bits
 unsigned char                decodingFlags;                         // 5 bits
*/ 

	frameParamsOut->compressedDataFrame = (unsigned char*)frameParamsIn->PictureStartAddrCompressedBuffer_p;
	frameParamsOut->compressedDataSize = (unsigned int)frameParamsIn->PictureStopAddrCompressedBuffer_p - 
					      (unsigned int)frameParamsIn->PictureStartAddrCompressedBuffer_p;
	
	frameParamsOut->lumaDecodeFramebuffer    = (unsigned char*)frameParamsIn->DecodedBufferAddress.DecodedLuma_p;
	frameParamsOut->chromaDecodeFramebuffer  = (unsigned char*)frameParamsIn->DecodedBufferAddress.DecodedChroma_p;

	frameParamsOut->horizontalDecimationFactor = 1;
	frameParamsOut->verticalDecimationFactor = 1;
		
	if ((frameParamsIn->HorizontalDecimationFactor != MPEG2_HDEC_1) || (frameParamsIn->VerticalDecimationFactor != MPEG2_VDEC_1))
	{
		
		switch (frameParamsIn->HorizontalDecimationFactor)
		{	
			case MPEG2_HDEC_1:	
			        frameParamsOut->horizontalDecimationFactor = 1;
			        break;		
		        case MPEG2_HDEC_2:
		        case MPEG2_HDEC_ADVANCED_2:
			        frameParamsOut->horizontalDecimationFactor = 2;
			        break;
		        case MPEG2_HDEC_4:
		        case MPEG2_HDEC_ADVANCED_4:
			        frameParamsOut->horizontalDecimationFactor = 4;
			        break;
		        default:
			        frameParamsOut->horizontalDecimationFactor = 1;
			        break;					
		}

		switch (frameParamsIn->VerticalDecimationFactor)
		{	
			case MPEG2_VDEC_1:	
			        frameParamsOut->verticalDecimationFactor = 1;
			        break;		
		        case MPEG2_VDEC_2_INT:
		        case MPEG2_VDEC_2_PROG:
		        case MPEG2_VDEC_ADVANCED_2_INT:
		        case MPEG2_VDEC_ADVANCED_2_PROG:
			        frameParamsOut->verticalDecimationFactor = 2;
			        break;
		        default:
			        frameParamsOut->verticalDecimationFactor = 1;
			        break;		
		}
					
		frameParamsOut->decimatedLumaDecodeFramebuffer    = (unsigned char*)frameParamsIn->DecodedBufferAddress.DecimatedLuma_p;
		frameParamsOut->decimatedChromaDecodeFramebuffer  = (unsigned char*)frameParamsIn->DecodedBufferAddress.DecimatedChroma_p;	
	}
	
	frameParamsOut->lumaBackwardReferenceFrame   = (unsigned char*)frameParamsIn->RefPicListAddress.BackwardReferenceLuma_p;
	frameParamsOut->chromaBackwardReferenceFrame = (unsigned char*)frameParamsIn->RefPicListAddress.BackwardReferenceChroma_p;		
	frameParamsOut->lumaForwardReferenceFrame    = (unsigned char*)frameParamsIn->RefPicListAddress.ForwardReferenceLuma_p;
	frameParamsOut->chromaForwardReferenceFrame  = (unsigned char*)frameParamsIn->RefPicListAddress.ForwardReferenceChroma_p;
	
	frameParamsOut->pictureCodingType = (unsigned char)frameParamsIn->PictureParameters.picture_coding_type;
		
	frameParamsOut->forwardHorizontalMotionVector  = (unsigned char)frameParamsIn->PictureParameters.forward_horizontal_f_code;
	frameParamsOut->forwardVerticalMotionVector    = (unsigned char)frameParamsIn->PictureParameters.forward_vertical_f_code;		
	frameParamsOut->backwardHorizontalMotionVector = (unsigned char)frameParamsIn->PictureParameters.backward_horizontal_f_code;
	frameParamsOut->backwardVerticalMotionVector   = (unsigned char)frameParamsIn->PictureParameters.backward_vertical_f_code;
		
	frameParamsOut->intraDCPrecision = (unsigned char)frameParamsIn->PictureParameters.intra_dc_precision;	
	frameParamsOut->pictureStructure = (unsigned char)frameParamsIn->PictureParameters.picture_structure;	
	frameParamsOut->decodingFlags	 = (unsigned char)frameParamsIn->PictureParameters.mpeg_decoding_flags;
		
}
	
void copySequenceParameters(Mpeg2HardSequenceParams_t  *seqParamsOut, MPEG2_SetGlobalParamSequence_t* seqParamsIn)
{
	unsigned int i;
//	OSDEV_Print("copySequenceParameters\n");	
	
/* MPEG2_SetGlobalParamSequence_t	
U32                  StructSize;
BOOL                 MPEGStreamTypeFlag;
U32                  horizontal_size;
U32                  vertical_size;
U32                  progressive_sequence;           
MPEG2_ChromaFormat_t chroma_format;                  
U32                  MatrixFlags;
U8                   intra_quantiser_matrix[64];
U8                   non_intra_quantiser_matrix[64];
U8                   chroma_intra_quantiser_matrix[64];
U8                   chroma_non_intra_quantiser_matrix[64];
*/ 
	
/*
 *     MpegStreamType_t             mpegStreamType;
 *     unsigned int                 horizontalSize;
 *     unsigned int                 verticalSize;
 *     unsigned int                 chromaFormat;
 *     unsigned char               *intraQuantizerMatrix;
 *     unsigned char               *nonIntraQuantizerMatrix;
*/ 
	
	seqParamsOut->mpegStreamType = seqParamsIn->MPEGStreamTypeFlag;
        seqParamsOut->horizontalSize = seqParamsIn->horizontal_size;	
        seqParamsOut->verticalSize   = seqParamsIn->vertical_size;	
//        seqParamsOut->chromaFormat   = (unsigned int)seqParamsIn->chroma_format;
        seqParamsOut->chromaFormat   = 0;
	
	for (i = 0 ; i < 64 ; i++)
	{	
		seqParamsOut->intraQuantizerMatrix[quantizationMatrixZigZagOrder[i]] = seqParamsIn->intra_quantiser_matrix[i];
		seqParamsOut->nonIntraQuantizerMatrix[quantizationMatrixZigZagOrder[i]] = seqParamsIn->non_intra_quantiser_matrix[i];
	}
	
}

static void __exit StmUnloadModule (void)
{

	Mpeg2InterruptUninstall();	
	UnMapRegisters();
	
	OSDEV_Print("Module unloaded\n");
}
