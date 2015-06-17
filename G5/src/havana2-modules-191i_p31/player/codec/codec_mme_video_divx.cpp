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

Source file name : codec_mme_video_divx.cpp
Author :           Chris

Implementation of the DivX video codec class for player 2.

************************************************************************/

#include "osinline.h"
#include "codec_mme_video_divx.h"

#ifndef DIVXDEC_MME_TRANSFORMER_NAME
# define DIVXDEC_MME_TRANSFORMER_NAME DIVX_MME_TRANSFORMER_NAME
#endif

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

#define BUFFER_DIVX_RASTERSTRUCT                     "DivxRasterStruct"
#define BUFFER_DIVX_RASTERSTRUCT_TYPE        {BUFFER_DIVX_RASTERSTRUCT, BufferDataTypeBase, AllocateFromNamedDeviceMemory, 32, 0, false, true, NOT_SPECIFIED}

#define BUFFER_DIVX_CODEC_STREAM_PARAMETER_CONTEXT             "DivxCodecStreamParameterContext"
#define BUFFER_DIVX_CODEC_STREAM_PARAMETER_CONTEXT_TYPE        {BUFFER_DIVX_CODEC_STREAM_PARAMETER_CONTEXT, BufferDataTypeBase, AllocateFromOSMemory, 32, 0, true, true, sizeof(DivxCodecStreamParameterContext_t)}

#define BUFFER_DIVX_CODEC_DECODE_CONTEXT       "DivxCodecDecodeContext"
#define BUFFER_DIVX_CODEC_DECODE_CONTEXT_TYPE  {BUFFER_DIVX_CODEC_DECODE_CONTEXT, BufferDataTypeBase, AllocateFromOSMemory, 32, 0, true, true, sizeof(DivxCodecDecodeContext_t)}

static BufferDataDescriptor_t           DivxRasterStructInitialDescriptor = BUFFER_DIVX_RASTERSTRUCT_TYPE;
static BufferDataDescriptor_t           DivxCodecStreamParameterContextDescriptor = BUFFER_DIVX_CODEC_STREAM_PARAMETER_CONTEXT_TYPE;
static BufferDataDescriptor_t           DivxCodecDecodeContextDescriptor = BUFFER_DIVX_CODEC_DECODE_CONTEXT_TYPE;

Codec_MmeVideoDivx_c::Codec_MmeVideoDivx_c( void)
{
		const unsigned int Width = ((960 + 32) + 63) & ~0x3F;
		const unsigned int Height = ((576 + 32) + 31) & ~0x1F;
		// This is the maximum size of a frame... currently PAL which is 720x576.  aligned to 1kb boundry
		MaxBytesPerFrame = (((Width * Height) + ((Width * Height) >> 1)) + 1023) & ~0x3FF;

		CurrentWidth   = 720;
		CurrentHeight  = 576;
		CurrentVersion = 500;

		Configuration.CodecName                             = "DivX video";

		Configuration.DecodeOutputFormat                    = FormatVideo420_PairedMacroBlock;

		Configuration.StreamParameterContextCount           = 1;
		Configuration.StreamParameterContextDescriptor      = &DivxCodecStreamParameterContextDescriptor;

		Configuration.DecodeContextCount                    = 4;
		Configuration.DecodeContextDescriptor               = &DivxCodecDecodeContextDescriptor;

		Configuration.MaxDecodeIndicesPerBuffer             = 2;
		Configuration.SliceDecodePermitted                  = false;
		Configuration.DecimatedDecodePermitted              = false;

		Configuration.TransformName[0]                      = MPEG4P2_MME_TRANSFORMER_NAME "0";
		Configuration.TransformName[1]                      = MPEG4P2_MME_TRANSFORMER_NAME "1";
		Configuration.TransformName[2]                      = DIVXDEC_MME_TRANSFORMER_NAME "0";
		Configuration.TransformName[3]                      = DIVXDEC_MME_TRANSFORMER_NAME "1";

		Configuration.AvailableTransformers                 = 2;

		Configuration.SizeOfTransformCapabilityStructure    = sizeof(Divx_TransformerCapability_t);
		Configuration.TransformCapabilityStructurePointer   = (void *)(&DivxTransformCapability);

		// The video firmware violates the MME spec. and passes data buffer addresses
		// as parametric information. For this reason it requires physical addresses
		// to be used.
		Configuration.AddressingMode                        = UnCachedAddress;

		// We do not need the coded data after decode is complete
		Configuration.ShrinkCodedDataBuffersAfterDecode     = true;

		Configuration.TrickModeParameters.EmpiricalMaximumDecodeFrameRateShortIntegration	= 60;
		Configuration.TrickModeParameters.EmpiricalMaximumDecodeFrameRateLongIntegration	= 60;

		Configuration.TrickModeParameters.SubstandardDecodeSupported		= false;
		Configuration.TrickModeParameters.SubstandardDecodeRateIncrease		= 1;

		Configuration.TrickModeParameters.DefaultGroupSize			= 12;
		Configuration.TrickModeParameters.DefaultGroupReferenceFrameCount	= 4;

		DivxRasterStructPool = NULL;
		DivxFirmware = false;
		restartTransformer = true;

		Reset();

		// Check which decoders we have available
		//
		for (int i = 0 ; i < 4 ; ++i)
		{

				MME_TransformerCapability_t      Capability;
				MME_ERROR                        MMEStatus;

				if( Configuration.SizeOfTransformCapabilityStructure != 0 )
				{
						memset( &Capability, 0, sizeof(MME_TransformerCapability_t) );
						memset( Configuration.TransformCapabilityStructurePointer, 0x00, Configuration.SizeOfTransformCapabilityStructure );

						Capability.StructSize           = sizeof(MME_TransformerCapability_t);
						Capability.TransformerInfoSize  = Configuration.SizeOfTransformCapabilityStructure;
						Capability.TransformerInfo_p    = Configuration.TransformCapabilityStructurePointer;

						MMEStatus                       = MME_GetTransformerCapability( Configuration.TransformName[i], &Capability );
						if( MMEStatus == MME_SUCCESS )
						{
								report (severity_info,"Found transformer %s",Configuration.TransformName[i]);

								if (((Divx_TransformerCapability_t*)Capability.TransformerInfo_p)->supports_311 == 1)
								{
										report( severity_info," (DivX compatible)");
										DivxFirmware = true;
								}
								report(severity_info,"\n");
						}
				}
		}

}

Codec_MmeVideoDivx_c::~Codec_MmeVideoDivx_c( void )
{
		Halt();
		Reset();
}

CodecStatus_t Codec_MmeVideoDivx_c::FillOutTransformerInitializationParameters( void )
{
		DivxInitializationParameters.width         = CurrentWidth;
		DivxInitializationParameters.height        = CurrentHeight;
		DivxInitializationParameters.codec_version = CurrentVersion;

		MMEInitializationParameters.TransformerInitParamsSize       = sizeof(Divx_InitTransformerParam_t);
		MMEInitializationParameters.TransformerInitParams_p         = (MME_GenericParams_t)(&DivxInitializationParameters);

		//report (severity_info, "Init Params - width: %d height: %d version: %d\n", CurrentWidth, CurrentHeight, CurrentVersion);

		return CodecNoError;
}

static void MMECallbackStub(    MME_Event_t      Event,
								MME_Command_t   *CallbackData,
								void            *UserData )
{
		Codec_MmeBase_c         *Self = (Codec_MmeBase_c *)UserData;

		Self->CallbackFromMME( Event, CallbackData );
		//    report (severity_error, "MME Callback !! \n");
		return;
}

CodecStatus_t   Codec_MmeVideoDivx_c::InitializeMMETransformer(      void )
{
        HandleCapabilities();
		return CodecNoError;
}

CodecStatus_t Codec_MmeVideoDivx_c::SendMMEStreamParameters( void )
{

		// no real set stream parameters for DivX decoder
		// we restart the transformer if width, height or version have changed.
		// so we need to write our own little version
		//
		CodecStatus_t status = CodecNoError;
        unsigned int TransformerID = 0;

		if (restartTransformer)
		{
				TerminateMMETransformer();

				memset( &MMEInitializationParameters, 0x00, sizeof(MME_TransformerInitParams_t) );

				MMEInitializationParameters.Priority                        = MME_PRIORITY_NORMAL;
				MMEInitializationParameters.StructSize                      = sizeof(MME_TransformerInitParams_t);
				MMEInitializationParameters.Callback                        = &MMECallbackStub;
				MMEInitializationParameters.CallbackUserData                = this;

				FillOutTransformerInitializationParameters();

				if ( (CurrentVersion > 100) && DivxFirmware )
				{
						TransformerID = SelectedTransformer+2;
				}
				else
				{
						TransformerID = SelectedTransformer;
						CurrentVersion = 500;
				}

				report(severity_info,"Using transformer %s - version %d - %s\n",
					   Configuration.TransformName[TransformerID],
					   CurrentVersion,
					   (DivxFirmware && (TransformerID> 1)) ?"DivX":"MPEG4p2");

				status  = MME_InitTransformer( Configuration.TransformName[TransformerID], &MMEInitializationParameters, &MMEHandle );
		}

		if (status ==  MME_SUCCESS)
		{
				status = CodecNoError;
				restartTransformer = false;
				report (severity_info,"New Stream Params w:%d h:%d ver:%d\n",CurrentWidth,CurrentHeight,CurrentVersion);
				ParsedFrameParameters->NewStreamParameters = false;
				MMEInitialized  = true;
		}
		else
		{
				report(severity_error,"Failed to initialise transformer MME error 0x%x\n",status);
				Player->MarkStreamUnPlayable( Stream );
		}

		//
		// The base class has very helpfully acquired a stream
		// parameters context for us which we must release.
		// But only if everything went well, otherwise the callers
		// will helpfully release it as well (Nick).
		//

		if( status == CodecNoError )
				StreamParameterContextBuffer->DecrementReferenceCount ();
		//

		return status;
}

CodecStatus_t  Codec_MmeVideoDivx_c::FillOutSetStreamParametersCommand( void )
{
		Mpeg4VideoStreamParameters_t  *Parsed  = (Mpeg4VideoStreamParameters_t *)ParsedFrameParameters->StreamParameterStructure;

		if ( (Parsed->VolHeader.width > 960) ||  (Parsed->VolHeader.height > 576) )
		{
				report(severity_error,"Stream dimensions too large for playback (%d x %d)\n",Parsed->VolHeader.width,Parsed->VolHeader.height);
				Player->MarkStreamUnPlayable( Stream );
				return CodecError;
		}

		// It seems there is still a bug somewhere, we get 0xe4000 for some strange reason to be investiagted....
		if ( !(DivxTransformCapability.supports_311 == 0 || DivxTransformCapability.supports_311 == 1))
				report(severity_error,"*** CAP *** = %d\n",DivxTransformCapability.supports_311);

		if ( (Parsed->VolHeader.version < 500) && (Parsed->VolHeader.version > 100) && !DivxFirmware)
		{
				report(severity_error,"DivX version 3.11 / 4.12 not available with transformer %s\n", Configuration.TransformName[SelectedTransformer]);
				Player->MarkStreamUnPlayable( Stream );
				return CodecError;
		}

		if ( (Parsed->VolHeader.width   != CurrentWidth ) ||
			 (Parsed->VolHeader.height  != CurrentHeight) ||
			 (Parsed->VolHeader.version != CurrentVersion) )
		{
				CurrentWidth   = Parsed->VolHeader.width;
				CurrentHeight  = Parsed->VolHeader.height;
				CurrentVersion = Parsed->VolHeader.version;
				restartTransformer = true;
		}

		return CodecNoError;
}

CodecStatus_t Codec_MmeVideoDivx_c::FillOutDecodeCommand( void )
{
		CodecStatus_t Status;
		unsigned int i = 0;
		unsigned int Entry;
		Buffer_t DivxRasterStructBuffer;
		Buffer_t ForRefDivxRasterStructBuffer;
		Buffer_t RevRefDivxRasterStructBuffer;

		KnownLastSliceInFieldFrame = true;

		DecodeContext->MMECommand.CmdStatus.AdditionalInfoSize = sizeof(MME_DivXVideoDecodeReturnParams_t);
		DecodeContext->MMECommand.CmdStatus.AdditionalInfo_p   = (MME_GenericParams_t*)&ReturnParams;
		DecodeContext->MMECommand.StructSize = sizeof (MME_Command_t);
		DecodeContext->MMECommand.CmdCode = MME_TRANSFORM;
		DecodeContext->MMECommand.CmdEnd = MME_COMMAND_END_RETURN_NOTIFY;
		DecodeContext->MMECommand.DueTime = (MME_Time_t)0;
		DecodeContext->MMECommand.NumberInputBuffers = NUM_MME_INPUT_BUFFERS;
		DecodeContext->MMECommand.NumberOutputBuffers = NUM_MME_OUTPUT_BUFFERS;

		DecodeContext->MMECommand.DataBuffers_p = (MME_DataBuffer_t**)DecodeContext->MMEBufferList;

		DecodeContext->MMECommand.ParamSize = sizeof(MME_DivXVideoDecodeParams_t);

		AdditionalParams.StartOffset = 0;   // Only for STAPI circular buffer mode
		AdditionalParams.EndOffset = 0;     // Only for STAPI circular buffer mode
		DecodeContext->MMECommand.Param_p = (MME_GenericParams_t *)&AdditionalParams;

		for(i = 0; i < (NUM_MME_DIVX_BUFFERS); i++)
		{
				DecodeContext->MMEBufferList[i] = &DecodeContext->MMEBuffers[i];
				DecodeContext->MMEBuffers[i].StructSize = sizeof (MME_DataBuffer_t);
				DecodeContext->MMEBuffers[i].UserData_p = (void*)CodedData;
				DecodeContext->MMEBuffers[i].Flags = 0;
				DecodeContext->MMEBuffers[i].StreamNumber = 0;
				DecodeContext->MMEBuffers[i].NumberOfScatterPages = 1;
				DecodeContext->MMEBuffers[i].ScatterPages_p = &DecodeContext->MMEPages[i];
				DecodeContext->MMEBuffers[i].StartOffset = 0;
				DecodeContext->MMEBuffers[i].UserData_p = (void*)CodedData;
		}

		DecodeContext->MMEBuffers[0].ScatterPages_p[0].Page_p = (void*)CodedData;
		DecodeContext->MMEBuffers[0].TotalSize = CodedDataLength;

		//      report(severity_error,"Data %02x %02x %02x %02x %02x\n",CodedData[0],CodedData[1],CodedData[2],CodedData[3],CodedData[4]);
		//
		//      report (severity_error,"%d Coded Data Length\n", CodedDataLength);
		//
		DecodeContext->MMEBuffers[4].ScatterPages_p[0].Page_p = (void*)BufferState[CurrentDecodeBufferIndex].BufferLumaPointer;
		DecodeContext->MMEBuffers[4].TotalSize = 0x1fe000 / 2;
		DecodeContext->MMEBuffers[5].ScatterPages_p[0].Page_p = (void*)(BufferState[CurrentDecodeBufferIndex].BufferChromaPointer);
		DecodeContext->MMEBuffers[5].TotalSize = 0x1fe000 / 2;
/*
	report(severity_error,"%x %x %x\n",
	       CodedData,
	       BufferState[CurrentDecodeBufferIndex].BufferLumaPointer,
	       BufferState[CurrentDecodeBufferIndex].BufferChromaPointer);
*/
		// set the defaults
		// MME doesn't like NULL pointers so just use CodedData instead
		// apparently NULLs can cause a mem leak.
		DecodeContext->MMEBuffers[2].ScatterPages_p[0].Page_p = (void*)CodedData;
		DecodeContext->MMEBuffers[2].TotalSize = 0;
		DecodeContext->MMEBuffers[3].ScatterPages_p[0].Page_p = (void*)CodedData;
		DecodeContext->MMEBuffers[3].TotalSize = 0;

		//      if( DecodeContext->ReferenceFrameList[0].EntryCount ==  0 )
		//              report( severity_error, "I Frame\n");
		//
		if( DecodeContext->ReferenceFrameList[0].EntryCount ==  1 )
		{
				//              report( severity_error, "P Frame\n");
				Entry       = DecodeContext->ReferenceFrameList[0].EntryIndicies[0];
				DecodeContext->MMEBuffers[3].TotalSize = MaxBytesPerFrame;
				Status  = BufferState[Entry].Buffer->ObtainAttachedBufferReference( DivxRasterStructType, &ForRefDivxRasterStructBuffer );

				if( Status != BufferNoError)
				{
						report( severity_error, "Codec_MmeVideoDivx_c::FillOutDecodeCommand - Failed to obtain raster frame buffer for forward ref frame (entry %d).\n",Entry );
						return Status;
				}

				ForRefDivxRasterStructBuffer->ObtainDataReference( NULL, NULL, (void **)&DecodeContext->MMEBuffers[3].ScatterPages_p[0].Page_p, UnCachedAddress );
		}
		else if( DecodeContext->ReferenceFrameList[0].EntryCount > 1 )
		{
				//report(severity_error,"Decoding B Frame\n");
				//            report( severity_error, "B Frame %d (For:%d Rev:%d)\n", DecodeContext->ReferenceFrameList[0].EntryCount, DecodeContext->ReferenceFrameList[0].EntryIndicies[1], DecodeContext->ReferenceFrameList[0].EntryIndicies[0]);
				Entry       = DecodeContext->ReferenceFrameList[0].EntryIndicies[0];
				DecodeContext->MMEBuffers[2].TotalSize = MaxBytesPerFrame;
				Status  = BufferState[Entry].Buffer->ObtainAttachedBufferReference( DivxRasterStructType, &RevRefDivxRasterStructBuffer );

				if( Status != BufferNoError)
				{
						report( severity_error, "Codec_MmeVideoDivx_c::FillOutDecodeCommand - Failed to obtain raster frame buffer for reverse ref frame. (Entry %d)\n",Entry );
						return Status;
				}

				RevRefDivxRasterStructBuffer->ObtainDataReference( NULL, NULL, (void **)&DecodeContext->MMEBuffers[2].ScatterPages_p[0].Page_p, UnCachedAddress );

				Entry       = DecodeContext->ReferenceFrameList[0].EntryIndicies[1];
				DecodeContext->MMEBuffers[3].TotalSize = MaxBytesPerFrame;
				Status  = BufferState[Entry].Buffer->ObtainAttachedBufferReference( DivxRasterStructType, &ForRefDivxRasterStructBuffer );

				if( Status != BufferNoError)
				{
						report( severity_error, "Codec_MmeVideoDivx_c::FillOutDecodeCommand - Failed to obtain raster frame buffer for forward ref frame (entry %d).\n",Entry );
						return Status;
				}

				ForRefDivxRasterStructBuffer->ObtainDataReference( NULL, NULL, (void **)&DecodeContext->MMEBuffers[3].ScatterPages_p[0].Page_p, UnCachedAddress );
		}

		if (CurrentDecodeBufferIndex != INVALID_FRAME_INDEX)
		{
				Status  = DivxRasterStructPool->GetBuffer( &DivxRasterStructBuffer, MaxBytesPerFrame );
				if( Status != BufferNoError )
				{
						report( severity_error, "Codec_MmeVideoDivx_c::FillOutDecodeCommand - Failed to get raster buffer pool.\n" );
						return Status;
				}

				CurrentDecodeBuffer->AttachBuffer( DivxRasterStructBuffer );
				DivxRasterStructBuffer->ObtainDataReference( NULL, NULL, (void **)&DecodeContext->MMEBuffers[1].ScatterPages_p[0].Page_p, UnCachedAddress );
				DivxRasterStructBuffer->DecrementReferenceCount();                   // Release ownership of the buffer to the decode buffer

				DecodeContext->MMEBuffers[1].TotalSize = MaxBytesPerFrame;
		}
		else
		{
				report( severity_error, "******* output buffer MUST have a valid index\n");
		}

		for(i = 0; i < (NUM_MME_DIVX_BUFFERS); i++)
		{
				DecodeContext->MMEPages[i].Size = DecodeContext->MMEBuffers[i].TotalSize; // Only one scatterpage, so size = totalsize
				DecodeContext->MMEPages[i].BytesUsed = 0;
				DecodeContext->MMEPages[i].FlagsIn = 0;
				DecodeContext->MMEPages[i].FlagsOut = 0;
		}

		//      report (severity_error , "Filled out decode command\n");
		//
#if 0

		for(i = 0; i < (NUM_MME_DIVX_BUFFERS); i++)
		{
				MME_ScatterPage_t* page = (MME_ScatterPage_t*)DecodeContext->MMEBuffers[i].ScatterPages_p[0].Page_p;

				report(severity_error, "Buffer List      (%d)  %x\n",i,DecodeContext->MMEBufferList[i]);
				report(severity_error, "Struct Size      (%d)  %x\n",i,DecodeContext->MMEBuffers[i].StructSize);
				report(severity_error, "User Data p      (%d)  %x\n",i,DecodeContext->MMEBuffers[i].UserData_p);
				report(severity_error, "Flags            (%d)  %x\n",i,DecodeContext->MMEBuffers[i].Flags);
				report(severity_error, "Stream Number    (%d)  %x\n",i,DecodeContext->MMEBuffers[i].StreamNumber);
				report(severity_error, "No Scatter Pages (%d)  %x\n",i,DecodeContext->MMEBuffers[i].NumberOfScatterPages);
				report(severity_error, "Scatter Pages p  (%d)  %x\n",i,page->Page_p);
				report(severity_error, "Start Offset     (%d)  %x\n\n",i,DecodeContext->MMEBuffers[i].StartOffset);
		}

		report(severity_error, "Additional Size  %x\n",DecodeContext->MMECommand.CmdStatus.AdditionalInfoSize);
		report(severity_error, "Additional p     %x\n",DecodeContext->MMECommand.CmdStatus.AdditionalInfo_p);
		report(severity_error, "Param Size       %x\n",DecodeContext->MMECommand.ParamSize);
		report(severity_error, "Param p          %x\n",DecodeContext->MMECommand.Param_p);
		report(severity_error, "DataBuffer p     %x\n",DecodeContext->MMECommand.DataBuffers_p);

#endif

		return CodecNoError;
}

CodecStatus_t Codec_MmeVideoDivx_c::Reset( void )
{

		if( DivxRasterStructPool != NULL )
		{
				BufferManager->DestroyPool( DivxRasterStructPool );
				DivxRasterStructPool = NULL;
		}

		restartTransformer = true;
		return Codec_MmeVideo_c::Reset();
}

CodecStatus_t Codec_MmeVideoDivx_c::HandleCapabilities( void )
{
		CodecStatus_t Status = CodecNoError;

		//
		// Create the buffer type for the raster image structure
		//
		// Find the type if it already exists
		//

		Status      = BufferManager->FindBufferDataType( BUFFER_DIVX_RASTERSTRUCT, &DivxRasterStructType );

		if( Status != BufferNoError )
		{

				//
				// It didn't already exist - create it
				//

				Status  = BufferManager->CreateBufferDataType( &DivxRasterStructInitialDescriptor, &DivxRasterStructType );

				if( Status != BufferNoError )
				{
						report( severity_error, "%s - Failed to create the %s buffer type.\n",__FUNCTION__, DivxRasterStructInitialDescriptor.TypeName );
				}
		}

		//
		// Now create the pool
		//
		if( Status == BufferNoError )
		{

				Status          = BufferManager->CreatePool( &DivxRasterStructPool, DivxRasterStructType, DecodeBufferCount, MaxBytesPerFrame,
															 NULL, NULL, Configuration.AncillaryMemoryPartitionName );

				if( Status != BufferNoError )
				{
						report( severity_error, "Codec_MmeVideoDivx_c::HandleCapabilities - Failed to create raster frame buffer pool.\n" );
				}
		}

		return Status;
}

CodecStatus_t Codec_MmeVideoDivx_c::ValidateDecodeContext( CodecBaseDecodeContext_t *Context )
{
		return CodecNoError;
}

CodecStatus_t Codec_MmeVideoDivx_c::DumpSetStreamParameters( void *Parameters )
{
		return CodecNoError;
}

CodecStatus_t  Codec_MmeVideoDivx_c::DumpDecodeParameters( void *Parameters )
{
		return CodecNoError;
}
