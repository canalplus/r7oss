/************************************************************************
Copyright (C) 2008 STMicroelectronics. All Rights Reserved.

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

Source file name : codec_mme_video_divx.cpp
Author :           Chris

Implementation of the DivX video codec class for player 2.

************************************************************************/

#include "osinline.h"
#include "osdev_device.h"
#include "codec_mme_video_divx_hd.h"

typedef struct DivxCodecStreamParameterContext_s
{
		CodecBaseStreamParameterContext_t   BaseContext;
}
DivxCodecStreamParameterContext_t;

typedef struct DivxCodecDecodeContext_s
{
		CodecBaseDecodeContext_t            BaseContext;
}
DivxCodecDecodeContext_t;

#define BUFFER_DIVX_CODEC_STREAM_PARAMETER_CONTEXT             "DivxCodecStreamParameterContext"
#define BUFFER_DIVX_CODEC_STREAM_PARAMETER_CONTEXT_TYPE        {BUFFER_DIVX_CODEC_STREAM_PARAMETER_CONTEXT, BufferDataTypeBase, AllocateFromOSMemory, 32, 0, true, true, sizeof(DivxCodecStreamParameterContext_t)}

#define BUFFER_DIVX_CODEC_DECODE_CONTEXT       "DivxCodecDecodeContext"
#define BUFFER_DIVX_CODEC_DECODE_CONTEXT_TYPE  {BUFFER_DIVX_CODEC_DECODE_CONTEXT, BufferDataTypeBase, AllocateFromOSMemory, 32, 0, true, true, sizeof(DivxCodecDecodeContext_t)}

#define BUFFER_DIVX_MBSTRUCT             "DivxMbStruct"
#define BUFFER_DIVX_MBSTRUCT_TYPE        {BUFFER_DIVX_MBSTRUCT, BufferDataTypeBase, AllocateFromNamedDeviceMemory, 32, 0, false, true, NOT_SPECIFIED}

static BufferDataDescriptor_t           DivxMbStructInitialDescriptor = BUFFER_DIVX_MBSTRUCT_TYPE;
static BufferDataDescriptor_t           DivxCodecStreamParameterContextDescriptor = BUFFER_DIVX_CODEC_STREAM_PARAMETER_CONTEXT_TYPE;
static BufferDataDescriptor_t           DivxCodecDecodeContextDescriptor = BUFFER_DIVX_CODEC_DECODE_CONTEXT_TYPE;

Codec_MmeVideoDivxHd_c::Codec_MmeVideoDivxHd_c( void)
{
		Configuration.CodecName                             = "DivX HD Video";

		Configuration.DecodeOutputFormat                    = FormatVideo420_MacroBlock;
		Configuration.StreamParameterContextCount           = 1;
		Configuration.StreamParameterContextDescriptor      = &DivxCodecStreamParameterContextDescriptor;

		Configuration.DecodeContextCount                    = 4;
		Configuration.DecodeContextDescriptor               = &DivxCodecDecodeContextDescriptor;

		Configuration.MaxDecodeIndicesPerBuffer             = 2;
		Configuration.SliceDecodePermitted                  = false;
		Configuration.DecimatedDecodePermitted              = false;

		Configuration.TransformName[0]                      = "MPEG4P2_720P_TRANSFORMER0";
		Configuration.TransformName[1]                      = "MPEG4P2_720P_TRANSFORMER1";

		Configuration.AvailableTransformers                 = 2;

		Configuration.SizeOfTransformCapabilityStructure    = sizeof(DivxTransformCapability);
		Configuration.TransformCapabilityStructurePointer   = (void *)(&DivxTransformCapability);

		// The video firmware violates the MME spec. and passes data buffer addresses
		// as parametric information. For this reason it requires physical addresses
		// to be used.
		Configuration.AddressingMode                        = PhysicalAddress;

		DivxMbStructPool                                     = NULL;

		// We do not need the coded data after decode is complete
		Configuration.ShrinkCodedDataBuffersAfterDecode     = true;

		// Trick Mode Parameters
		//
		Configuration.TrickModeParameters.EmpiricalMaximumDecodeFrameRateShortIntegration	= 60;
		Configuration.TrickModeParameters.EmpiricalMaximumDecodeFrameRateLongIntegration	= 60;

		Configuration.TrickModeParameters.SubstandardDecodeSupported		= false;
		Configuration.TrickModeParameters.SubstandardDecodeRateIncrease		= 1;

		Configuration.TrickModeParameters.DefaultGroupSize			= 12;
		Configuration.TrickModeParameters.DefaultGroupReferenceFrameCount	= 4;

		MME_TransformerCapability_t      Capability;
		MME_ERROR                        MMEStatus;

		if( Configuration.SizeOfTransformCapabilityStructure != 0 )
		{
				memset( &Capability, 0, sizeof(MME_TransformerCapability_t) );
				memset( Configuration.TransformCapabilityStructurePointer, 0x00, Configuration.SizeOfTransformCapabilityStructure );

				Capability.StructSize           = sizeof(MME_TransformerCapability_t);
				Capability.TransformerInfoSize  = Configuration.SizeOfTransformCapabilityStructure;
				Capability.TransformerInfo_p    = Configuration.TransformCapabilityStructurePointer;

				MMEStatus                       = MME_GetTransformerCapability( Configuration.TransformName[0], &Capability );
				if( MMEStatus == MME_SUCCESS )
				{
						report (severity_info,"Found %s codec\n",Configuration.TransformName[0]);

				}
		}

		Reset();

}

// /////////////////////////////////////////////////////////////////////////
//
//      Destructor function, ensures a full halt and reset
//      are executed for all levels of the class.
//

Codec_MmeVideoDivxHd_c::~Codec_MmeVideoDivxHd_c( void )
{
		Halt();
		Reset();
}

// /////////////////////////////////////////////////////////////////////////
//
//      Function to deal with the returned capabilities
//      structure for a DivX MME transformer.
//

CodecStatus_t Codec_MmeVideoDivxHd_c::Reset( void )
{
		if( DivxMbStructPool != NULL )
		{
				BufferManager->DestroyPool( DivxMbStructPool );
				DivxMbStructPool = NULL;
		}

		return Codec_MmeVideo_c::Reset();
}

// /////////////////////////////////////////////////////////////////////////
//
//      Function to deal with the returned capabilities
//      structure for a divx mme transformer.
//
CodecStatus_t   Codec_MmeVideoDivxHd_c::HandleCapabilities( void )
{
		report( severity_info, "MME Transformer '%s' capabilities are :-\n", Configuration.TransformName[0]);

		// Should never be called since we did not set the size of the capabilities structure.
		return CodecError;
}

///////////////////////////////////////////////////////////////////////////
///
///     Allocate the Divx macroblocks structure.
///
///     Due to the structure of the shared super-class, most codecs allocate
///     the majority of the resources when the player supplies it with an
///     output buffer.
///
CodecStatus_t   Codec_MmeVideoDivxHd_c::RegisterOutputBufferRing(   Ring_t                    Ring )
{
		CodecStatus_t Status;

		Status = Codec_MmeVideo_c::RegisterOutputBufferRing( Ring );
		if( Status != CodecNoError )
				return Status;

		//
		// Create the buffer type for the macroblocks structure
		//
		// Find the type if it already exists
		//

		Status      = BufferManager->FindBufferDataType( BUFFER_DIVX_MBSTRUCT, &DivxMbStructType );
		if( Status != BufferNoError )
		{
				//
				// It didn't already exist - create it
				//

				Status  = BufferManager->CreateBufferDataType( &DivxMbStructInitialDescriptor, &DivxMbStructType );
				if( Status != BufferNoError )
				{
						report( severity_error, "<Blah Blah> - Failed to create the %s buffer type.\n", DivxMbStructInitialDescriptor.TypeName );
						return Status;
				}
		}

		//
		// Now create the pool
		//

		unsigned int                    Size;
		Size = (1280 >> 4) * (736 >> 4) * 32;  // Allocate the maximum for now. FIX ME & save space on smaller streams.
		Status          = BufferManager->CreatePool( &DivxMbStructPool, DivxMbStructType, DecodeBufferCount, Size,
													 NULL, NULL, Configuration.AncillaryMemoryPartitionName );
		if( Status != BufferNoError )
		{
				report( severity_error, "<Blah Blah> - Failed to create macroblock structure pool.\n" );
				return Status;
		}

		return CodecNoError;
}

// /////////////////////////////////////////////////////////////////////////
//
//      Function to deal with the returned capabilities
//      structure for a divx mme transformer.
//

CodecStatus_t Codec_MmeVideoDivxHd_c::FillOutTransformerInitializationParameters( void )
{
		Mpeg4VideoStreamParameters_t  *Parsed  = (Mpeg4VideoStreamParameters_t *)ParsedFrameParameters->StreamParameterStructure;

		//	report(severity_error,"%s :: %d\n",__FUNCTION__,__LINE__);

		// This may not be needed may be able to go back to frame parser defaulting to 500
		if (Parsed->VolHeader.version == 100)
				Parsed->VolHeader.version = 500;

		if ( (Parsed->VolHeader.width > 1280) ||
			 (Parsed->VolHeader.height > 720) )
		{
				report(severity_error,"Stream dimensions too large for playback (%d x %d)\n",Parsed->VolHeader.width,Parsed->VolHeader.height);
				Player->MarkStreamUnPlayable( Stream );
				return CodecError;
		}

		// temp until we have fully working firmware
	/*
	if (Parsed->VolHeader.version < 400 )
	{
		report (severity_error,"DivX Version %d - Interlaced %d\n",Parsed->VolHeader.version,Parsed->VolHeader.interlaced);
		report(severity_error,"Current DivX HD firmware does not support anything but progressive Divx 5 content\n");
		Player->MarkStreamUnPlayable( Stream );
		return CodecError;
	}
	*/

		DivxInitializationParameters.width                          = Parsed->VolHeader.width;
		DivxInitializationParameters.height                         = Parsed->VolHeader.height;
		DivxInitializationParameters.codec_version                  = Parsed->VolHeader.version;

		StreamWidth                                                 = Parsed->VolHeader.width;
		StreamHeight                                                = Parsed->VolHeader.height;

		DivxInitializationParameters.CircularBufferBeginAddr_p      = (DivXHD_CompressedData_t)0x00000000;
		DivxInitializationParameters.CircularBufferEndAddr_p        = (DivXHD_CompressedData_t)0xfffffff8;

		MMEInitializationParameters.TransformerInitParamsSize       = sizeof(DivxInitializationParameters);
		MMEInitializationParameters.TransformerInitParams_p         = (MME_GenericParams_t)(&DivxInitializationParameters);

		return CodecNoError;
}

static void MMECallbackStub(    MME_Event_t      Event,
								MME_Command_t   *CallbackData,
								void            *UserData )
{
		//	report(severity_error,"%s :: %d\n",__FUNCTION__,__LINE__);

		Codec_MmeBase_c         *Self = (Codec_MmeBase_c *)UserData;

		Self->CallbackFromMME( Event, CallbackData );
		return;
}

// /////////////////////////////////////////////////////////////////////////
// // We need to initialise the transformer once we actually have the stream
// parameters, so for now we do nothing on the base class call and do the
// work later on in the set stream parameters
//
CodecStatus_t   Codec_MmeVideoDivxHd_c::InitializeMMETransformer(      void )
{
		return CodecNoError;
}

// /////////////////////////////////////////////////////////////////////////
//
//      Function to fill out the stream parameters
//      structure for a DivX MME transformer.
//

CodecStatus_t  Codec_MmeVideoDivxHd_c::FillOutSetStreamParametersCommand( void )
{
        DivxCodecStreamParameterContext_t      *Context        = (DivxCodecStreamParameterContext_t *)StreamParameterContext;
		Mpeg4VideoStreamParameters_t  *Parsed  = (Mpeg4VideoStreamParameters_t *)ParsedFrameParameters->StreamParameterStructure;

		//	report(severity_error,"%s :: %d\n",__FUNCTION__,__LINE__);
		if (!MMEInitialized)
		{
				// now that we have all the stream parameters we can start the transformer
				//
				CodecStatus_t                    Status;
				MME_ERROR                        MMEStatus;

				//

				MMECommandPreparedCount     = 0;
				MMECommandAbortedCount      = 0;
				MMECommandCompletedCount    = 0;

				//
				// Handle the transformer initialization
				//

				memset( &MMEInitializationParameters, 0x00, sizeof(MME_TransformerInitParams_t) );

				MMEInitializationParameters.Priority                        = MME_PRIORITY_NORMAL;
				MMEInitializationParameters.StructSize                      = sizeof(MME_TransformerInitParams_t);
				MMEInitializationParameters.Callback                        = &MMECallbackStub;
				MMEInitializationParameters.CallbackUserData                = this;

				Status      = FillOutTransformerInitializationParameters();
				if( Status != CodecNoError )
				{
						report( severity_error, "Codec_MmeBase_c::InitializeMMETransformer(%s) - Failed to fill out transformer initialization parameters (%08x).\n", Configuration.CodecName, Status );
						return Status;
				}

				MMEStatus   = MME_InitTransformer( Configuration.TransformName[SelectedTransformer], &MMEInitializationParameters, &MMEHandle );
				if( MMEStatus != MME_SUCCESS )
				{
						report( severity_error, "Codec_MmmBase_c::InitializeMMETransformer(%s) - Failed to initialize mme transformer (%08x).\n", Configuration.CodecName, MMEStatus );
						return CodecError;
				}

				MMEInitialized      = true;
				MMECallbackPriorityBoosted = false;

		}

		//	if ( (Parsed->VolHeader.width != StreamWidth) ||
		if ( (Parsed->VolHeader.width > 1280) ||
			 (Parsed->VolHeader.height > 720) )
		{
				report(severity_error,"Stream Dimensions too large %d x %d greater than 720p\n",Parsed->VolHeader.width,Parsed->VolHeader.height);
				Player->MarkStreamUnPlayable( Stream );
				return CodecError;
		}

		// This may not be needed may be able to go back to frame parser defaulting to 500
		if (Parsed->VolHeader.version == 100)
				Parsed->VolHeader.version = 500;
/*
	DivXGlobalParamSequence.CodecVersion = Parsed->VolHeader.version;
	DivXGlobalParamSequence.height = Parsed->VolHeader.height;
	DivXGlobalParamSequence.width = Parsed->VolHeader.width;
	DivXGlobalParamSequence.newpred_enable = 0; // HE
 */

		DivXGlobalParamSequence.shape = Parsed->VolHeader.shape;
		DivXGlobalParamSequence.time_increment_resolution = Parsed->VolHeader.time_increment_resolution;
		DivXGlobalParamSequence.interlaced = Parsed->VolHeader.interlaced;
		DivXGlobalParamSequence.sprite_usage = Parsed->VolHeader.sprite_usage;
		DivXGlobalParamSequence.quant_type = Parsed->VolHeader.quant_type;
		DivXGlobalParamSequence.load_intra_quant_matrix = Parsed->VolHeader.load_intra_quant_matrix;
		DivXGlobalParamSequence.load_nonintra_quant_matrix = Parsed->VolHeader.load_non_intra_quant_matrix;
		DivXGlobalParamSequence.quarter_pixel = Parsed->VolHeader.quarter_pixel;

		// required for GMC support
		//	DivXGlobalParamSequence.sprite_warping_accuracy = Parsed->VolHeader.sprite_warping_accuracy;
		DivXGlobalParamSequence.data_partitioning = Parsed->VolHeader.data_partitioning;
		DivXGlobalParamSequence.reversible_vlc = Parsed->VolHeader.reversible_vlc;
		DivXGlobalParamSequence.resync_marker_disable = Parsed->VolHeader.resync_marker_disable;

		DivXGlobalParamSequence.short_video_header = 0;
		DivXGlobalParamSequence.num_mb_in_gob = 0;
		DivXGlobalParamSequence.num_gobs_in_vop = 0;

		memcpy(DivXGlobalParamSequence.intra_quant_matrix,Parsed->VolHeader.intra_quant_matrix,(sizeof(unsigned char) * 64));
		memcpy(DivXGlobalParamSequence.nonintra_quant_matrix,Parsed->VolHeader.non_intra_quant_matrix,(sizeof(unsigned char) * 64));

		memset( &Context->BaseContext.MMECommand, 0x00, sizeof(MME_Command_t) );

		Context->BaseContext.MMECommand.CmdStatus.AdditionalInfoSize        = 0;
		Context->BaseContext.MMECommand.CmdStatus.AdditionalInfo_p          = NULL;
		Context->BaseContext.MMECommand.ParamSize                           = sizeof(DivXGlobalParamSequence);
		Context->BaseContext.MMECommand.Param_p                             = (MME_GenericParams_t)(&DivXGlobalParamSequence);

		//	report(severity_error,"%s :: %d\n",__FUNCTION__,__LINE__);

		return CodecNoError;
}

CodecStatus_t Codec_MmeVideoDivxHd_c::FillOutDecodeCommand( void )
{
		CodecStatus_t Status;
		unsigned int  Entry;
		unsigned int  Size = (1280 >> 4) * (736 >> 4) * 32;
		Buffer_t      DivxMbStructBuffer;

		KnownLastSliceInFieldFrame = true;

		Mpeg4VideoFrameParameters_t  *Frame  = (Mpeg4VideoFrameParameters_t *)ParsedFrameParameters->FrameParameterStructure;

		if (ParsedFrameParameters->ReferenceFrame)
		{
				Status  = DivxMbStructPool->GetBuffer (&DivxMbStructBuffer, Size);
				if( Status != BufferNoError )
				{
						report( severity_error, "Codec_MmeVideoDivxHd_c::FillOutDecodeCommand - Failed to get macroblock structure buffer.\n" );
						return Status;
				}

				DivxMbStructBuffer->ObtainDataReference (NULL, NULL, (void **)&DecodeParams.DecodedBufferAddr.MBStruct_p, PhysicalAddress);

				CurrentDecodeBuffer->AttachBuffer (DivxMbStructBuffer);          // Attach to decode buffer (so it will be freed at the same time)
				DivxMbStructBuffer->DecrementReferenceCount();                   // and release ownership of the buffer to the decode buffer

				// Remember the MBStruct pointer in case we have a second field to follow
				BufferState[CurrentDecodeBufferIndex].BufferMacroblockStructurePointer  = (unsigned char*)DecodeParams.DecodedBufferAddr.MBStruct_p;
		}

		memset(&(DecodeContext->MMECommand.CmdStatus), 0, sizeof(MME_CommandStatus_t));
		DecodeContext->MMECommand.CmdStatus.AdditionalInfoSize = sizeof(ReturnParams);
		DecodeContext->MMECommand.CmdStatus.AdditionalInfo_p   = (MME_GenericParams_t*)&ReturnParams;
		DecodeContext->MMECommand.StructSize = sizeof (MME_Command_t);
		DecodeContext->MMECommand.CmdCode = MME_TRANSFORM;
		DecodeContext->MMECommand.CmdEnd = MME_COMMAND_END_RETURN_NOTIFY;
		DecodeContext->MMECommand.DueTime = (MME_Time_t)0;

		DecodeContext->MMECommand.DataBuffers_p = NULL;
		DecodeContext->MMECommand.NumberInputBuffers = 0;
		DecodeContext->MMECommand.NumberOutputBuffers = 0;
		DecodeContext->MMECommand.ParamSize = sizeof(DecodeParams);
		DecodeContext->MMECommand.Param_p = (MME_GenericParams_t *)&DecodeParams;

		DecodeParams.PictureStartAddr_p = (unsigned int*)(CodedData);
		DecodeParams.PictureEndAddr_p = (unsigned int*)(CodedData + CodedDataLength);

		DecodeParams.DecodedBufferAddr.Luma_p = (unsigned int*)BufferState[CurrentDecodeBufferIndex].BufferLumaPointer;
		DecodeParams.DecodedBufferAddr.Chroma_p = (unsigned int*)BufferState[CurrentDecodeBufferIndex].BufferChromaPointer;

		if( ParsedFrameParameters->NumberOfReferenceFrameLists != 0 )
		{
				if( DecodeContext->ReferenceFrameList[0].EntryCount == 1 )
				{
						Entry       = DecodeContext->ReferenceFrameList[0].EntryIndicies[0];
						DecodeParams.RefPicListAddr.BackwardRefLuma_p = (U32*)BufferState[Entry].BufferLumaPointer;
						DecodeParams.RefPicListAddr.BackwardRefChroma_p = (U32*)BufferState[Entry].BufferChromaPointer;
				}
				if( DecodeContext->ReferenceFrameList[0].EntryCount == 2 )
				{
						unsigned int ForEntry       = DecodeContext->ReferenceFrameList[0].EntryIndicies[0];
						DecodeParams.RefPicListAddr.ForwardRefLuma_p = (U32*)BufferState[ForEntry].BufferLumaPointer;
						DecodeParams.RefPicListAddr.ForwardRefChroma_p = (U32*)BufferState[ForEntry].BufferChromaPointer;

						unsigned int BackEntry      = DecodeContext->ReferenceFrameList[0].EntryIndicies[1];
						DecodeParams.RefPicListAddr.BackwardRefLuma_p = (U32*)BufferState[BackEntry].BufferLumaPointer;
						DecodeParams.RefPicListAddr.BackwardRefChroma_p = (U32*)BufferState[BackEntry].BufferChromaPointer;
				}
		}
		else if (Frame->VopHeader.prediction_type != PREDICTION_TYPE_I)
				report(severity_error,"What's happened to my reference frame list ??\n");

		DecodeParams.bit_skip_no = Frame->bit_skip_no;

		DecodeParams.use_intra_dc_vlc = Frame->VopHeader.intra_dc_vlc_thr?0:1;
		DecodeParams.shape_coding_type = 0;

        DecodeParams.UserData_p = DecodeParams.DecodedBufferAddr.Luma_p;

		//	DecodeParams.trbi = Frame->VopHeader.trbi;
		//	DecodeParams.trdi = Frame->VopHeader.trdi;
		DecodeParams.trb_trd = Frame->VopHeader.trb_trd;
		DecodeParams.trb_trd_trd = Frame->VopHeader.trb_trd_trd;
		//	DecodeParams.trd = Frame->VopHeader.trd;
		//	DecodeParams.trb = Frame->VopHeader.trb;

		DecodeParams.prediction_type = Frame->VopHeader.prediction_type;

		DecodeParams.quantizer = Frame->VopHeader.quantizer;
		DecodeParams.rounding_type = Frame->VopHeader.rounding_type ;
		DecodeParams.fcode_for = Frame->VopHeader.fcode_forward;
		DecodeParams.fcode_back = Frame->VopHeader.fcode_backward;

		DecodeParams.vop_coded = Frame->VopHeader.vop_coded;
		DecodeParams.intra_dc_vlc_thr = Frame->VopHeader.intra_dc_vlc_thr;
		//	DecodeParams.time_inc = Frame->VopHeader.time_inc;

		//	DecodeParams.top_field_first = Frame->VopHeader.top_field_first;
		DecodeParams.alternate_vertical_scan_flag = Frame->VopHeader.alternate_vertical_scan_flag;

		//	report(severity_error,"%s :: %d\n",__FUNCTION__,__LINE__);

		return CodecNoError;
}

CodecStatus_t Codec_MmeVideoDivxHd_c::ValidateDecodeContext( CodecBaseDecodeContext_t *Context )
{
		return CodecNoError;
}

CodecStatus_t Codec_MmeVideoDivxHd_c::DumpSetStreamParameters( void *Parameters )
{
		return CodecNoError;
}

CodecStatus_t  Codec_MmeVideoDivxHd_c::DumpDecodeParameters( void *Parameters )
{
		return CodecNoError;
}
