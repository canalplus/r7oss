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

Source file name : codec_mme_video_rmv.cpp
Author :           Mark C

Implementation of the VC-1 video codec class for player 2.


Date        Modification                                    Name
----        ------------                                    --------
06-Mar-07   Created                                         Mark C

************************************************************************/

// /////////////////////////////////////////////////////////////////////
//
//      Include any component headers

#include "codec_mme_video_rmv.h"
#include "rmv.h"

// /////////////////////////////////////////////////////////////////////////
//
// Locally defined constants
//


// /////////////////////////////////////////////////////////////////////////
//
// Locally defined structures
//

//{{{  Locally defined structures
typedef struct RmvCodecStreamParameterContext_s
{
    CodecBaseStreamParameterContext_t   BaseContext;
} RmvCodecStreamParameterContext_t;

typedef struct RmvCodecDecodeContext_s
{
    CodecBaseDecodeContext_t                    BaseContext;
    RV89Dec_TransformParams_t                   DecodeParameters;
    RV89Dec_TransformStatusAdditionalInfo_t     DecodeStatus;
} RmvCodecDecodeContext_t;

#define BUFFER_RMV_CODEC_STREAM_PARAMETER_CONTEXT               "RmvCodecStreamParameterContext"
#define BUFFER_RMV_CODEC_STREAM_PARAMETER_CONTEXT_TYPE {BUFFER_RMV_CODEC_STREAM_PARAMETER_CONTEXT, BufferDataTypeBase, AllocateFromOSMemory, 32, 0, true, true, sizeof(RmvCodecStreamParameterContext_t)}
static BufferDataDescriptor_t                   RmvCodecStreamParameterContextDescriptor = BUFFER_RMV_CODEC_STREAM_PARAMETER_CONTEXT_TYPE;

#define BUFFER_RMV_CODEC_DECODE_CONTEXT         "RmvCodecDecodeContext"
#define BUFFER_RMV_CODEC_DECODE_CONTEXT_TYPE    {BUFFER_RMV_CODEC_DECODE_CONTEXT, BufferDataTypeBase, AllocateFromOSMemory, 32, 0, true, true, sizeof(RmvCodecDecodeContext_t)}
static BufferDataDescriptor_t                   RmvCodecDecodeContextDescriptor         = BUFFER_RMV_CODEC_DECODE_CONTEXT_TYPE;

#define BUFFER_RMV_SEGMENT_LIST                 "RmvSegmentList"
#define BUFFER_RMV_SEGMENT_LIST_TYPE            {BUFFER_RMV_SEGMENT_LIST, BufferDataTypeBase, AllocateFromNamedDeviceMemory, 32, 0, false, true, NOT_SPECIFIED}
static BufferDataDescriptor_t                   RmvSegmentListInitialDescriptor         = BUFFER_RMV_SEGMENT_LIST_TYPE;

//}}}
static unsigned int                 PictureNo       = 0;

//{{{  C Callback stub
// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      C wrapper for the MME callback
//

typedef void (*MME_GenericCallback_t) (MME_Event_t Event, MME_Command_t * CallbackData, void *UserData);

static void MMECallbackStub(    MME_Event_t      Event,
                                MME_Command_t   *CallbackData,
                                void            *UserData )
{
Codec_MmeBase_c         *Self = (Codec_MmeBase_c *)UserData;

    Self->CallbackFromMME( Event, CallbackData );
    return;
}


//}}}

//{{{  Constructor
// /////////////////////////////////////////////////////////////////////////
//
//      Cosntructor function, fills in the codec specific parameter values
//

Codec_MmeVideoRmv_c::Codec_MmeVideoRmv_c( void )
{
    Configuration.CodecName                             = "Rmv video";

    Configuration.DecodeOutputFormat                    = FormatVideo420_MacroBlock;

    Configuration.StreamParameterContextCount           = 1;
    Configuration.StreamParameterContextDescriptor      = &RmvCodecStreamParameterContextDescriptor;

    Configuration.DecodeContextCount                    = 4;
    Configuration.DecodeContextDescriptor               = &RmvCodecDecodeContextDescriptor;

    Configuration.MaxDecodeIndicesPerBuffer             = 2;
    Configuration.SliceDecodePermitted                  = false;
    Configuration.DecimatedDecodePermitted              = false;

    Configuration.TransformName[0]                      = RV89DEC_MME_TRANSFORMER_NAME "0";
    Configuration.TransformName[1]                      = RV89DEC_MME_TRANSFORMER_NAME "1";
    Configuration.AvailableTransformers                 = 2;

    // RV89Dec codec does not properly implement MME_GetTransformerCapability() and any attempt
    // to do so results in a MME_NOT_IMPLEMENTED error. For this reason it is better not to
    // ask.
    Configuration.SizeOfTransformCapabilityStructure    = 0;
    Configuration.TransformCapabilityStructurePointer   = NULL;

    // The video firmware violates the MME spec. and passes data buffer addresses
    // as parametric information. For this reason it requires physical addresses
    // to be used.
    Configuration.AddressingMode                        = PhysicalAddress;

    // We do not need the coded data after decode is complete
    Configuration.ShrinkCodedDataBuffersAfterDecode     = true;

    InitializationParameters.MaxWidth                   = RMV_DEFAULT_PICTURE_WIDTH;
    InitializationParameters.MaxHeight                  = RMV_DEFAULT_PICTURE_HEIGHT;
    InitializationParameters.StreamFormatIdentifier     = RV89DEC_FID_REALVIDEO30;
    InitializationParameters.isRV8                      = 1;            // default to RV8
    InitializationParameters.NumRPRSizes                = 0;

    #if (RV89DEC_MME_VERSION > 11)
        InitializationParameters.BFrameDeblockingMode   = RV89DEC_BFRAME_REGULAR_DEBLOCK;
    #endif   

    RestartTransformer                                  = false;

    SegmentListPool                                     = NULL;

    PictureNo                                           = 0;
    Reset();
}
//}}}
//{{{  Destructor
// /////////////////////////////////////////////////////////////////////////
//
//      Destructor function, ensures a full halt and reset 
//      are executed for all levels of the class.
//

Codec_MmeVideoRmv_c::~Codec_MmeVideoRmv_c( void )
{
    Halt();
    Reset();
}
//}}}
//{{{  Reset
// /////////////////////////////////////////////////////////////////////////
//
//      Reset function for Rmv specific members.
//      
//

CodecStatus_t   Codec_MmeVideoRmv_c::Reset( void )
{
    if (SegmentListPool != NULL)
    {
        BufferManager->DestroyPool (SegmentListPool);
        SegmentListPool         = NULL;
    }
    return Codec_MmeVideo_c::Reset();
}
//}}}

//{{{  RegisterOutputBufferRing
///////////////////////////////////////////////////////////////////////////
///
///     Allocate the RMV segment lists.
///
///     Due to the structure of the shared super-class, most codecs allocate
///     the majority of the resources when the player supplies it with an
///     output buffer.
///
CodecStatus_t   Codec_MmeVideoRmv_c::RegisterOutputBufferRing(   Ring_t                    Ring )
{
    CodecStatus_t Status;

    Status = Codec_MmeVideo_c::RegisterOutputBufferRing( Ring );
    if( Status != CodecNoError )
        return Status;

    //
    // Create the buffer type for the segment list structure
    //
    // Find the type if it already exists
    //

    Status             = BufferManager->FindBufferDataType (BUFFER_RMV_SEGMENT_LIST, &SegmentListType);
    if (Status != BufferNoError )
    {
        // It didn't already exist - create it
        Status         = BufferManager->CreateBufferDataType (&RmvSegmentListInitialDescriptor, &SegmentListType);
        if (Status != BufferNoError)
        {
            CODEC_ERROR ("Failed to create the %s buffer type.\n", RmvSegmentListInitialDescriptor.TypeName);
            return Status;
        }
    }

    // Now create the pool
    Status              = BufferManager->CreatePool (&SegmentListPool, SegmentListType,
                                                     Configuration.DecodeContextCount, (sizeof(RV89Dec_Segment_Info))*RMV_MAX_SEGMENTS,
                                                     NULL, NULL, Configuration.AncillaryMemoryPartitionName );
    if (Status != BufferNoError)
    {
        CODEC_ERROR ("Failed to create segment list pool.\n" );
        return Status;
    }

    return CodecNoError;
}
//}}}
//{{{  HandleCapabilities
// /////////////////////////////////////////////////////////////////////////
//
//      Function to deal with the returned capabilities
//      structure for an Rmv mme transformer.
//
CodecStatus_t   Codec_MmeVideoRmv_c::HandleCapabilities( void )
{
    CODEC_TRACE ("MME Transformer '%s' capabilities are :-\n", RV89DEC_MME_TRANSFORMER_NAME);

    // Should never be called since we did not set the size of the capabilities structure.
    return CodecError;
}
//}}}
//{{{  FillOutTransformerInitializationParameters
// /////////////////////////////////////////////////////////////////////////
//
//      Function to deal with the returned capabilities
//      structure for an Rmv mme transformer.
//
CodecStatus_t   Codec_MmeVideoRmv_c::FillOutTransformerInitializationParameters( void )
{
    unsigned int        i;
    // Fillout the actual command
    MMEInitializationParameters.TransformerInitParamsSize      = sizeof(RV89Dec_InitTransformerParam_t);
    MMEInitializationParameters.TransformerInitParams_p        = (MME_GenericParams_t)(&InitializationParameters);

    CODEC_TRACE("FillOutTransformerInitializationParameters\n");
    CODEC_TRACE("  MaxWidth              %6u\n", InitializationParameters.MaxWidth);
    CODEC_TRACE("  MaxHeight             %6u\n", InitializationParameters.MaxHeight);
    CODEC_TRACE("  FormatId              %6u\n", InitializationParameters.StreamFormatIdentifier);
    CODEC_TRACE("  isRV8                 %6u\n", InitializationParameters.isRV8);
    CODEC_TRACE("  NumRPRSizes           %6u\n", InitializationParameters.NumRPRSizes);
    for (i=0; i<(InitializationParameters.NumRPRSizes*2); i+=2)
    {
        CODEC_DEBUG("  RPRSize[%d]           %6u\n", i, InitializationParameters.RPRSize[i]);
        CODEC_DEBUG("  RPRSize[%d]           %6u\n", i+1, InitializationParameters.RPRSize[i+1]);
    }

    return CodecNoError;
}
//}}}
//{{{  SendMMEStreamParameters
CodecStatus_t Codec_MmeVideoRmv_c::SendMMEStreamParameters (void)
{
    CodecStatus_t       CodecStatus     = CodecNoError;
    unsigned int        MMEStatus       = MME_SUCCESS;


    // There are no set stream parameters for Rmv decoder so the transformer is
    // terminated and restarted when required (i.e. if width or height change).

    if (RestartTransformer)
    {
        TerminateMMETransformer();

        memset (&MMEInitializationParameters, 0x00, sizeof(MME_TransformerInitParams_t));

        MMEInitializationParameters.Priority                    = MME_PRIORITY_NORMAL;
        MMEInitializationParameters.StructSize                  = sizeof(MME_TransformerInitParams_t);
        MMEInitializationParameters.Callback                    = &MMECallbackStub;
        MMEInitializationParameters.CallbackUserData            = this;

        FillOutTransformerInitializationParameters ();

        MMEStatus               = MME_InitTransformer (Configuration.TransformName[SelectedTransformer],
                                                       &MMEInitializationParameters, &MMEHandle);

        if (MMEStatus ==  MME_SUCCESS)
        {
            CODEC_DEBUG ("New Stream Params %dx%d\n", InitializationParameters.MaxWidth, InitializationParameters.MaxHeight);
            CodecStatus                                         = CodecNoError;
            RestartTransformer                                  = false;
            ParsedFrameParameters->NewStreamParameters          = false;

            MMEInitialized                                      = true;
        }
    }

    //
    // The base class has very helpfully acquired a stream 
    // parameters context for us which we must release.
    // But only if everything went well, otherwise the callers
    // will helpfully release it as well (Nick).
    //

    if (CodecStatus == CodecNoError)
        StreamParameterContextBuffer->DecrementReferenceCount ();

//

    return CodecStatus;


}
//}}}
//{{{  FillOutSetStreamParametersCommand
// /////////////////////////////////////////////////////////////////////////
//
//      Function to fill out the stream parameters
//      structure for an Rmv mme transformer.
//
CodecStatus_t   Codec_MmeVideoRmv_c::FillOutSetStreamParametersCommand( void )
{
    RmvStreamParameters_t*      Parsed          = (RmvStreamParameters_t*)ParsedFrameParameters->StreamParameterStructure;
    RmvVideoSequence_t*         SequenceHeader  = &Parsed->SequenceHeader;
    unsigned int                MaxWidth;
    unsigned int                MaxHeight;
    RV89Dec_fid_t               FormatId;
    int                         IsRV8           = 1;
    unsigned int                NumRPRSizes;
    unsigned int                i;

    MaxWidth                    = SequenceHeader->MaxWidth;
    MaxHeight                   = SequenceHeader->MaxHeight;

    if ((SequenceHeader->BitstreamVersion == RV9_BITSTREAM_VERSION) &&
        (SequenceHeader->BitstreamMinorVersion == RV9_BITSTREAM_MINOR_VERSION))
    {
        FormatId                = RV89DEC_FID_REALVIDEO30;
        IsRV8                   = 0;
    }
    else if ((SequenceHeader->BitstreamVersion == RV8_BITSTREAM_VERSION) &&
             (SequenceHeader->BitstreamMinorVersion == RV8_BITSTREAM_MINOR_VERSION))
    {
        FormatId                = RV89DEC_FID_REALVIDEO30;
        IsRV8                   = 1;
    }
    else if (SequenceHeader->BitstreamMinorVersion == RV89_RAW_BITSTREAM_MINOR_VERSION)
    {
        FormatId                = RV89DEC_FID_RV89COMBO;
        if (SequenceHeader->BitstreamVersion == RV8_BITSTREAM_VERSION)
            IsRV8               = 1;
    }
    else
    {
        CODEC_ERROR ("Invalid Bitstream versions (%d, %d)\n",
                      SequenceHeader->BitstreamVersion, SequenceHeader->BitstreamMinorVersion);
        return CodecError;
    }
    NumRPRSizes                 = IsRV8 ? SequenceHeader->NumRPRSizes : 0;

#if 0
    if ((MaxWidth    != InitializationParameters.MaxWidth)          ||
        (MaxHeight   != InitializationParameters.MaxHeight)         ||
        (FormatId    != InitializationParameters.StreamFormatIdentifier) ||
        (IsRV8       != InitializationParameters.isRV8)             ||
        (NumRPRSizes != InitializationParameters.NumRPRSizes))
    {
#endif
    InitializationParameters.MaxWidth               = MaxWidth;
    InitializationParameters.MaxHeight              = MaxHeight;
    InitializationParameters.StreamFormatIdentifier = FormatId;
    InitializationParameters.isRV8                  = IsRV8;
    InitializationParameters.NumRPRSizes            = NumRPRSizes;
    for (i=0; i<(NumRPRSizes*2); i+=2)
    {
        InitializationParameters.RPRSize[i]         = SequenceHeader->RPRSize[i];
        InitializationParameters.RPRSize[i+1]       = SequenceHeader->RPRSize[i+1];
    }
    InitializationParameters.pIntraMBInfo           = NULL;

    RestartTransformer      = true;

    return CodecNoError;

}
//}}}
//{{{  FillOutDecodeCommand
// /////////////////////////////////////////////////////////////////////////
//
//      Function to fill out the decode parameters
//      structure for an rmv mme transformer.
//

//#define RV89_INTERFACE_V0_0_4
CodecStatus_t   Codec_MmeVideoRmv_c::FillOutDecodeCommand(       void )
{
    RmvCodecDecodeContext_t*            Context         = (RmvCodecDecodeContext_t*)DecodeContext;
    RmvFrameParameters_t*               Frame           = (RmvFrameParameters_t*)ParsedFrameParameters->FrameParameterStructure;

    RV89Dec_TransformParams_t*          Param;
    RmvVideoSegmentList_t*              SegmentList;
    Buffer_t                            SegmentInfoBuffer;
    RV89Dec_Segment_Info*               SegmentInfo;

    Buffer_t                            RasterBuffer;
    BufferStructure_t                   RasterBufferStructure;
    unsigned char*                      RasterBufferBase;

    CodecStatus_t                       Status;
    unsigned int                        i;

    // For rmv we do not do slice decodes.
    KnownLastSliceInFieldFrame                  = true;

    Param                                       = &Context->DecodeParameters;
    SegmentList                                 = &Frame->SegmentList;

    // Fillout the straight forward command parameters
#if defined (RV89_INTERFACE_V0_0_4)
    Param->InBuffer.pCompressedData             = (unsigned char*)CodedData;
    Param->InBuffer.CompressedDataBufferSize    = CodedDataLength;
#elif defined (SMALL_CIRCULAR_BUFFER)
    // The first two assume a circular buffer arrangement
    Param->InBuffer.pStartPtr                   = (unsigned char*)CodedData;
    Param->InBuffer.pEndPtr                     = Param->InBuffer.pStartPtr + CodedDataLength + 4096;
    Param->InBuffer.PictureOffset               = 0;
    Param->InBuffer.PictureSize                 = CodedDataLength;
#else
    // The first two assume a circular buffer arrangement
    Param->InBuffer.pStartPtr                   = (unsigned char*)0x00;
    Param->InBuffer.pEndPtr                     = (unsigned char*)0xffffffff;
    Param->InBuffer.PictureOffset               = (unsigned int)CodedData;
    Param->InBuffer.PictureSize                 = CodedDataLength;
#endif

    // Get the segment list buffer
    Status                                      = SegmentListPool->GetBuffer (&SegmentInfoBuffer,
                                                                              (sizeof(RV89Dec_Segment_Info))*(SegmentList->NumSegments+1));
    if (Status != BufferNoError)
    {
        CODEC_ERROR ("Failed to get segment info buffer.\n" );
        return Status;
    }

    // Copy segment list
    //Param->InBuffer.NumSegments                 = SegmentList->NumSegments;
    Param->InBuffer.NumSegments                 = SegmentList->NumSegments;
    SegmentInfoBuffer->ObtainDataReference (NULL, NULL, (void**)&SegmentInfo, UnCachedAddress);
    for (i=0; i<SegmentList->NumSegments; i++)
    {
        SegmentInfo[i].is_valid                 = 1;
        SegmentInfo[i].offset                   = SegmentList->Segment[i].Offset;
    }
    SegmentInfo[i].is_valid                     = 0;
    SegmentInfo[i].offset                       = CodedDataLength;

    // Tell far side how to find list
    SegmentInfoBuffer->ObtainDataReference (NULL, NULL, (void **)&Param->InBuffer.pSegmentInfo, PhysicalAddress);

    // Attach allocated segment list buffer to decode context and let go of it
    DecodeContextBuffer->AttachBuffer (SegmentInfoBuffer);
    SegmentInfoBuffer->DecrementReferenceCount ();

    Status                                      = FillOutDecodeBufferRequest (&RasterBufferStructure);
    if (Status != BufferNoError)
    {
        report (severity_error, "Codec_MmeVideoRmv_c::FillOutDecodeCommand - Failed to fill out a buffer request structure.\n");
        return Status;
    }
    // Override the format so we get one sized for raster rather than macroblock
    RasterBufferStructure.Format                = FormatVideo420_Planar;
    RasterBufferStructure.ComponentBorder[0]    = 16;
    RasterBufferStructure.ComponentBorder[1]    = 16;
    // Ask the manifestor for a buffer of the new format
    Status                                      = Manifestor->GetDecodeBuffer (&RasterBufferStructure, &RasterBuffer);
    if (Status != BufferNoError)
    {
        report (severity_error, "Codec_MmeVideoRmv_c::FillOutDecodeCommand - Failed to obtain a decode buffer from the manifestor.\n");
        return Status;
    }
    RasterBuffer->ObtainDataReference (NULL, NULL, (void **)&RasterBufferBase, PhysicalAddress);

    // Fill in all buffer luma and chroma pointers
    Param->Outbuffer.pLuma                      = (RV89Dec_LumaAddress_t)BufferState[CurrentDecodeBufferIndex].BufferLumaPointer;
    Param->Outbuffer.pChroma                    = (RV89Dec_ChromaAddress_t)BufferState[CurrentDecodeBufferIndex].BufferChromaPointer;

#if defined (RV89_INTERFACE_V0_0_4)
    // Move pointer to first byte inside border
    RasterBufferStructure.ComponentOffset[0]    = RasterBufferStructure.Dimension[0] * 16 + 16;
    RasterBufferStructure.ComponentOffset[1]   += RasterBufferStructure.Dimension[0] * 8 + 8;
#endif

#if 0
    // Initialise decode buffers to bright pink
    unsigned char*      LumaBuffer;
    unsigned char*      ChromaBuffer;
    unsigned int        LumaSize        = InitializationParameters.MaxWidth*InitializationParameters.MaxHeight;
    CurrentDecodeBuffer->ObtainDataReference( NULL, NULL, (void**)&LumaBuffer, UnCachedAddress);
    ChromaBuffer                        = LumaBuffer+LumaSize;
    memset (LumaBuffer,   0x00, LumaSize);
    memset (ChromaBuffer, 0x80, LumaSize/2);
    RasterBuffer->ObtainDataReference( NULL, NULL, (void**)&LumaBuffer, UnCachedAddress);
    LumaSize                            = RasterBufferStructure.ComponentOffset[1];
    ChromaBuffer                        = LumaBuffer+LumaSize;
    memset (LumaBuffer,   0xff, LumaSize);
    memset (ChromaBuffer, 0xff, LumaSize/2);
#endif

    Param->CurrDecFrame.pLuma                   = (RV89Dec_LumaAddress_t)(RasterBufferBase + RasterBufferStructure.ComponentOffset[0]);
    Param->CurrDecFrame.pChroma                 = (RV89Dec_ChromaAddress_t)(RasterBufferBase + RasterBufferStructure.ComponentOffset[1]);

    // Attach planar buffer to decode buffer and let go of it
    CurrentDecodeBuffer->AttachBuffer (RasterBuffer);
    RasterBuffer->DecrementReferenceCount ();

    // Preserve raster buffer pointers for later use as reference frames
    BufferState[CurrentDecodeBufferIndex].BufferRasterPointer                   = Param->CurrDecFrame.pLuma;
    BufferState[CurrentDecodeBufferIndex].BufferMacroblockStructurePointer      = Param->CurrDecFrame.pChroma;

    // Fillout the reference frame lists - default to self if not present
    if ((ParsedFrameParameters->NumberOfReferenceFrameLists == 0) || (DecodeContext->ReferenceFrameList[0].EntryCount == 0))
    {
        Param->PrevRefFrame.pLuma               = Param->CurrDecFrame.pLuma;
        Param->PrevRefFrame.pChroma             = Param->CurrDecFrame.pChroma;
        Param->PrevMinusOneRefFrame.pLuma       = Param->CurrDecFrame.pLuma;
        Param->PrevMinusOneRefFrame.pChroma     = Param->CurrDecFrame.pChroma;
    }
    else
    {
        i                                       = DecodeContext->ReferenceFrameList[0].EntryIndicies[0];
        Param->PrevRefFrame.pLuma               = (RV89Dec_LumaAddress_t)BufferState[i].BufferRasterPointer;
        Param->PrevRefFrame.pChroma             = (RV89Dec_ChromaAddress_t)BufferState[i].BufferMacroblockStructurePointer;
        i                                       = DecodeContext->ReferenceFrameList[0].EntryIndicies[1];
        Param->PrevMinusOneRefFrame.pLuma       = (RV89Dec_LumaAddress_t)BufferState[i].BufferRasterPointer;
        Param->PrevMinusOneRefFrame.pChroma     = (RV89Dec_ChromaAddress_t)BufferState[i].BufferMacroblockStructurePointer;
    }

    //{{{  DEBUG
    {

        report( severity_info,  "Codec Picture No %d, Picture type %d\n", PictureNo++, Frame->PictureHeader.PictureCodingType);
    #if 0
        report( severity_info,  "Codec Picture No %d, Picture type %d\n", PictureNo++, Frame->PictureHeader.PictureCodingType);
        report( severity_info,  "      InBuffer.pCompressedData             = %08x\n", Param->InBuffer.pCompressedData);
        report( severity_info,  "      InBuffer.CompressedDataBufferSize    = %d\n",   Param->InBuffer.CompressedDataBufferSize);
        report( severity_info,  "      InBuffer.NumSegments                 = %d\n",   Param->InBuffer.NumSegments);
        report( severity_info,  "      InBuffer.pSegmentInfo                = %08x\n", Param->InBuffer.pSegmentInfo);
        for (i=0; i<Param->InBuffer.NumSegments+1; i++)
        {
            report( severity_info,  "      InBuffer.SegmentInfo[%d]             = %d, %d\n", i, SegmentInfo[i].is_valid, SegmentInfo[i].offset);
        }
        report( severity_info,  "      CurrDecFrame.pLuma                   = %08x\n", Param->CurrDecFrame.pLuma);
        report( severity_info,  "      CurrDecFrame.pChroma                 = %08x\n", Param->CurrDecFrame.pChroma);
        report( severity_info,  "      Outbuffer.pLuma                      = %08x\n", Param->Outbuffer.pLuma);
        report( severity_info,  "      Outbuffer.pChroma                    = %08x\n", Param->Outbuffer.pChroma);
        report( severity_info,  "      PrevRefFrame.pLuma                   = %08x\n", Param->PrevRefFrame.pLuma);
        report( severity_info,  "      PrevRefFrame.pChroma                 = %08x\n", Param->PrevRefFrame.pChroma);
        report( severity_info,  "      PrevMinusOneRefFrame.pLuma           = %08x\n", Param->PrevMinusOneRefFrame.pLuma);
        report( severity_info,  "      PrevMinusOneRefFrame.pChroma         = %08x\n", Param->PrevMinusOneRefFrame.pChroma);
    #endif
    }
    //}}}

    // Fillout the actual command
    memset( &Context->BaseContext.MMECommand, 0x00, sizeof(MME_Command_t) );

    Context->BaseContext.MMECommand.CmdStatus.AdditionalInfoSize    = sizeof(RV89Dec_TransformStatusAdditionalInfo_t);
    Context->BaseContext.MMECommand.CmdStatus.AdditionalInfo_p      = (MME_GenericParams_t)(&Context->DecodeStatus);
    Context->BaseContext.MMECommand.ParamSize                       = sizeof(RV89Dec_TransformParams_t);
    Context->BaseContext.MMECommand.Param_p                         = (MME_GenericParams_t)(&Context->DecodeParameters);

    return CodecNoError;
}
//}}}
//{{{  FillOutDecodeBufferRequest
// /////////////////////////////////////////////////////////////////////////
//
//      The specific video function used to fill out a buffer structure
//      request.
//

CodecStatus_t   Codec_MmeVideoRmv_c::FillOutDecodeBufferRequest(   BufferStructure_t        *Request )
{
    Codec_MmeVideo_c::FillOutDecodeBufferRequest(Request);

    //Request->ComponentBorder[0]         = 16;
    //Request->ComponentBorder[1]         = 16;

    return CodecNoError;
}
//}}}
//{{{  ValidateDecodeContext
////////////////////////////////////////////////////////////////////////////
///
/// Unconditionally return success.
/// 
/// Success and failure codes are located entirely in the generic MME structures
/// allowing the super-class to determine whether the decode was successful. This
/// means that we have no work to do here.
///
/// \return CodecNoError
///
CodecStatus_t   Codec_MmeVideoRmv_c::ValidateDecodeContext( CodecBaseDecodeContext_t *Context )
{
    return CodecNoError;
}
//}}}
//{{{  DumpSetStreamParameters
// /////////////////////////////////////////////////////////////////////////
//
//      Function to dump out the set stream
//      parameters from an mme command.
//

CodecStatus_t   Codec_MmeVideoRmv_c::DumpSetStreamParameters(         void    *Parameters )
{

    report (severity_info, "Stream Params:\n");
    report (severity_info, "  MaxWidth              %6u\n", InitializationParameters.MaxWidth);
    report (severity_info, "  MaxHeight             %6u\n", InitializationParameters.MaxHeight);
    report (severity_info, "  DecoderInterface      %6u\n", InitializationParameters.StreamFormatIdentifier);
    report (severity_info, "  isRV8                 %6u\n", InitializationParameters.isRV8);
    report (severity_info, "  NumRPRSizes           %6u\n", InitializationParameters.NumRPRSizes);
    report (severity_info, "  RetsartTransformer    %6u\n", RestartTransformer);

    return CodecNoError;
}
//}}}
//{{{  DumpDecodeParameters
// /////////////////////////////////////////////////////////////////////////
//
//      Function to dump out the decode
//      parameters from an mme command.
//

CodecStatus_t   Codec_MmeVideoRmv_c::DumpDecodeParameters      (void*   Parameters)
{
    RV89Dec_TransformParams_t*  FrameParams     = (RV89Dec_TransformParams_t*)Parameters;

    report (severity_info,  "Frame Params\n");
#if defined (RV89_INTERFACE_V0_0_4)
    report (severity_info,  "      InBuffer.pCompressedData             = %08x\n", FrameParams->InBuffer.pCompressedData);
    report (severity_info,  "      InBuffer.CompressedDataBufferSize    = %d\n",   FrameParams->InBuffer.CompressedDataBufferSize);
#else
    report (severity_info,  "      InBuffer.pStartPtr                   = %08x\n", FrameParams->InBuffer.pStartPtr);
    report (severity_info,  "      InBuffer.PictureSize                 = %d\n",   FrameParams->InBuffer.PictureSize);
#endif
    report (severity_info,  "      InBuffer.NumSegments                 = %d\n",   FrameParams->InBuffer.NumSegments);
    report (severity_info,  "      InBuffer.pSegmentInfo                = %08x\n", FrameParams->InBuffer.pSegmentInfo);
    report (severity_info,  "      CurrDecFrame.pLuma                   = %08x\n", FrameParams->CurrDecFrame.pLuma);
    report (severity_info,  "      CurrDecFrame.pChroma                 = %08x\n", FrameParams->CurrDecFrame.pChroma);
    report (severity_info,  "      PrevRefFrame.pLuma                   = %08x\n", FrameParams->PrevRefFrame.pLuma);
    report (severity_info,  "      PrevRefFrame.pChroma                 = %08x\n", FrameParams->PrevRefFrame.pChroma);
    report (severity_info,  "      PrevMinusOneRefFrame.pLuma           = %08x\n", FrameParams->PrevMinusOneRefFrame.pLuma);
    report (severity_info,  "      PrevMinusOneRefFrame.pChroma         = %08x\n", FrameParams->PrevMinusOneRefFrame.pChroma);

    return CodecNoError;
}
//}}}
//{{{  CheckCodecReturnParameters
// Convert the return code into human readable form.
static const char* LookupError (unsigned int Error)
{
#define E(e) case e: return #e
    switch(Error)
    {
        E(RV89DEC_TIMEOUT_ERROR);
        E(RV89DEC_RUN_TIME_INVALID_PARAMS);
        E(RV89DEC_FEATURE_NOT_IMPLEMENTED);
        E(RV89DEC_MEMORY_UNDERFLOW_ERROR);
        E(RV89DEC_MEMORY_TRANSLATION_ERROR);
        E(RV89DEC_TASK_CREATION_ERROR);
        E(RV89DEC_UNKNOWN_ERROR);
        default: return "RV89DEC_UNKNOWN_ERROR";
    }
#undef E
}
CodecStatus_t   Codec_MmeVideoRmv_c::CheckCodecReturnParameters( CodecBaseDecodeContext_t *Context )
{

    MME_Command_t*                              MMECommand              = (MME_Command_t*)(&Context->MMECommand);
    MME_CommandStatus_t*                        CmdStatus               = (MME_CommandStatus_t*)(&MMECommand->CmdStatus);
    RV89Dec_TransformStatusAdditionalInfo_t*    AdditionalInfo_p        = (RV89Dec_TransformStatusAdditionalInfo_t*)CmdStatus->AdditionalInfo_p;

    if (AdditionalInfo_p != NULL)
    {
        if (AdditionalInfo_p->ErrorCode != RV89DEC_NO_ERROR)
            CODEC_TRACE("%s - %s  %x \n", __FUNCTION__, LookupError(AdditionalInfo_p->ErrorCode), AdditionalInfo_p->ErrorCode );
    }

    return CodecNoError;
}
//}}}
