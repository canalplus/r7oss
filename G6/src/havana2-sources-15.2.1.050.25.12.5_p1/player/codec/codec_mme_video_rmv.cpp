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

#include "codec_mme_video_rmv.h"
#include "rmv.h"
#include "player_generic.h"
#include "player_playback.h"
#include "player_stream.h"

#undef TRACE_TAG
#define TRACE_TAG "Codec_MmeVideoRmv_c"

//{{{
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
//}}}
static unsigned int                 PictureNo       = 0;

//{{{  C Callback stub
// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      C wrapper for the MME callback
//

typedef void (*MME_GenericCallback_t)(MME_Event_t Event, MME_Command_t *CallbackData, void *UserData);

static void MMECallbackStub(MME_Event_t      Event,
                            MME_Command_t   *CallbackData,
                            void            *UserData)
{
    Codec_MmeBase_c         *Self = (Codec_MmeBase_c *)UserData;
    Self->CallbackFromMME(Event, CallbackData);
}

//}}}

Codec_MmeVideoRmv_c::Codec_MmeVideoRmv_c(void)
    : Codec_MmeVideo_c()
    , InitializationParameters()
    , RestartTransformer(true)
{
    Configuration.CodecName                             = "Rmv video";
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
}

Codec_MmeVideoRmv_c::~Codec_MmeVideoRmv_c(void)
{
    Halt();
}

//{{{  Connect
///////////////////////////////////////////////////////////////////////////
///
///     Allocate the RMV segment lists.
///
///     Due to the structure of the shared super-class, most codecs allocate
///     the majority of the resources when the player supplies it with an
///     output buffer.
///
CodecStatus_t   Codec_MmeVideoRmv_c::Connect(Port_c *Port)
{
    CodecStatus_t Status;
    Status = Codec_MmeVideo_c::Connect(Port);

    if (Status != CodecNoError)
    {
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
CodecStatus_t   Codec_MmeVideoRmv_c::HandleCapabilities(void)
{
    SE_INFO(group_decoder_video, "MME Transformer '%s' capabilities are :-\n", RV89DEC_MME_TRANSFORMER_NAME);
    //
    // Create the buffer type including a raster copy for reference purposes
    //
    Stream->GetDecodeBufferManager()->FillOutDefaultList(FormatVideo420_MacroBlock,
                                                         PrimaryManifestationElement | VideoDecodeCopyElement | AdditionalMemoryBlockElement,
                                                         Configuration.ListOfDecodeBufferComponents);
    Configuration.ListOfDecodeBufferComponents[1].DataType      = FormatVideo420_Planar;
    Configuration.ListOfDecodeBufferComponents[1].ComponentBorders[0]   = 16;
    Configuration.ListOfDecodeBufferComponents[1].ComponentBorders[1]   = 16;
    return CodecNoError;
}
//}}}
//{{{  FillOutTransformerInitializationParameters
// /////////////////////////////////////////////////////////////////////////
//
//      Function to deal with the returned capabilities
//      structure for an Rmv mme transformer.
//
CodecStatus_t   Codec_MmeVideoRmv_c::FillOutTransformerInitializationParameters(void)
{
    unsigned int        i;
    // Fill out the actual command
    MMEInitializationParameters.TransformerInitParamsSize      = sizeof(RV89Dec_InitTransformerParam_t);
    MMEInitializationParameters.TransformerInitParams_p        = (MME_GenericParams_t)(&InitializationParameters);
    SE_INFO(group_decoder_video, "Transformer Initialization Parameters\n");
    SE_INFO(group_decoder_video, "  MaxWidth              %6u\n", InitializationParameters.MaxWidth);
    SE_INFO(group_decoder_video, "  MaxHeight             %6u\n", InitializationParameters.MaxHeight);
    SE_DEBUG(group_decoder_video, "  FormatId              %6u\n", InitializationParameters.StreamFormatIdentifier);
    SE_DEBUG(group_decoder_video, "  isRV8                 %6u\n", InitializationParameters.isRV8);
    SE_DEBUG(group_decoder_video, "  NumRPRSizes           %6u\n", InitializationParameters.NumRPRSizes);

    for (i = 0; i < (InitializationParameters.NumRPRSizes * 2); i += 2)
    {
        SE_DEBUG(group_decoder_video,  "  RPRSize[%d]           %6u\n", i, InitializationParameters.RPRSize[i]);
        SE_DEBUG(group_decoder_video,  "  RPRSize[%d]           %6u\n", i + 1, InitializationParameters.RPRSize[i + 1]);
    }

    return CodecNoError;
}
//}}}
//{{{  SendMMEStreamParameters
CodecStatus_t Codec_MmeVideoRmv_c::SendMMEStreamParameters(void)
{
    CodecStatus_t       CodecStatus     = CodecNoError;
    unsigned int        MMEStatus       = MME_SUCCESS;

    // There are no set stream parameters for Rmv decoder so the transformer is
    // terminated and restarted when required (i.e. if width or height change).

    if (RestartTransformer)
    {
        TerminateMMETransformer();
        memset(&MMEInitializationParameters, 0, sizeof(MME_TransformerInitParams_t));
        MMEInitializationParameters.Priority                    = MME_PRIORITY_NORMAL;
        MMEInitializationParameters.StructSize                  = sizeof(MME_TransformerInitParams_t);
        MMEInitializationParameters.Callback                    = &MMECallbackStub;
        MMEInitializationParameters.CallbackUserData            = this;
        FillOutTransformerInitializationParameters();
        MMEStatus               = MME_InitTransformer(Configuration.TransformName[SelectedTransformer],
                                                      &MMEInitializationParameters, &MMEHandle);

        if (MMEStatus ==  MME_SUCCESS)
        {
            SE_DEBUG(group_decoder_video, "New Stream Params %dx%d\n", InitializationParameters.MaxWidth, InitializationParameters.MaxHeight);
            CodecStatus                                         = CodecNoError;
            RestartTransformer                                  = false;
            ParsedFrameParameters->NewStreamParameters          = false;
            MMEInitialized                                      = true;
        }
    }

    // The base class has very helpfully acquired a stream
    // parameters context for us which we must release.
    // But only if everything went well, otherwise the callers
    // will helpfully release it as well

    if (CodecStatus == CodecNoError)
    {
        StreamParameterContextBuffer->DecrementReferenceCount();
    }

    return CodecStatus;
}
//}}}
//{{{  FillOutSetStreamParametersCommand
// /////////////////////////////////////////////////////////////////////////
//
//      Function to fill out the stream parameters
//      structure for an Rmv mme transformer.
//
CodecStatus_t   Codec_MmeVideoRmv_c::FillOutSetStreamParametersCommand(void)
{
    RmvStreamParameters_t      *Parsed          = (RmvStreamParameters_t *)ParsedFrameParameters->StreamParameterStructure;
    RmvVideoSequence_t         *SequenceHeader  = &Parsed->SequenceHeader;
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
        {
            IsRV8               = 1;
        }
    }
    else
    {
        SE_ERROR("Invalid Bitstream versions (%d, %d)\n",
                 SequenceHeader->BitstreamVersion, SequenceHeader->BitstreamMinorVersion);
        return CodecError;
    }

    NumRPRSizes                 = IsRV8 ? SequenceHeader->NumRPRSizes : 0;

    InitializationParameters.MaxWidth               = MaxWidth;
    InitializationParameters.MaxHeight              = MaxHeight;
    InitializationParameters.StreamFormatIdentifier = FormatId;
    InitializationParameters.isRV8                  = IsRV8;
    InitializationParameters.NumRPRSizes            = NumRPRSizes;

    for (i = 0; i < (NumRPRSizes * 2); i += 2)
    {
        InitializationParameters.RPRSize[i]         = SequenceHeader->RPRSize[i];
        InitializationParameters.RPRSize[i + 1]       = SequenceHeader->RPRSize[i + 1];
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
CodecStatus_t   Codec_MmeVideoRmv_c::FillOutDecodeCommand(void)
{
    RmvCodecDecodeContext_t            *Context         = (RmvCodecDecodeContext_t *)DecodeContext;
    RmvFrameParameters_t               *Frame           = (RmvFrameParameters_t *)ParsedFrameParameters->FrameParameterStructure;
    RV89Dec_TransformParams_t          *Param;
    RmvVideoSegmentList_t              *SegmentList;
    RV89Dec_Segment_Info               *SegmentInfo;
    unsigned int                        i;
    Buffer_t                DecodeBuffer;
    Buffer_t                ReferenceBuffer;
    // For rmv we do not do slice decodes.
    KnownLastSliceInFieldFrame                  = true;
    OS_LockMutex(&Lock);
    DecodeBuffer                = BufferState[CurrentDecodeBufferIndex].Buffer;
    Param                       = &Context->DecodeParameters;
    SegmentList                 = &Frame->SegmentList;

    // Fill out the straight forward command parameters
    // The first two assume a circular buffer arrangement
    Param->InBuffer.pStartPtr                   = (unsigned char *)0x00;
    Param->InBuffer.pEndPtr                     = (unsigned char *)0xffffffff;
    Param->InBuffer.PictureOffset               = (unsigned int)CodedData;
    Param->InBuffer.PictureSize                 = CodedDataLength;
    // Copy segment list
    Param->InBuffer.NumSegments                 = SegmentList->NumSegments;

    SegmentInfo = (RV89Dec_Segment_Info *)Stream->GetDecodeBufferManager()->ComponentBaseAddress(DecodeBuffer, AdditionalMemoryBlock, CachedAddress);

    for (i = 0; i < SegmentList->NumSegments; i++)
    {
        SegmentInfo[i].is_valid                 = 1;
        SegmentInfo[i].offset                   = SegmentList->Segment[i].Offset;
    }

    SegmentInfo[i].is_valid                     = 0;
    SegmentInfo[i].offset                       = CodedDataLength;
    // Tell far side how to find list
    Param->InBuffer.pSegmentInfo        = (RV89Dec_Segment_Info *)Stream->GetDecodeBufferManager()->ComponentBaseAddress(DecodeBuffer, AdditionalMemoryBlock);
    // Fill in all buffer luma and chroma pointers
    Param->Outbuffer.pLuma                      = (RV89Dec_LumaAddress_t)Stream->GetDecodeBufferManager()->Luma(DecodeBuffer, PrimaryManifestationComponent);
    Param->Outbuffer.pChroma                    = (RV89Dec_ChromaAddress_t)Stream->GetDecodeBufferManager()->Chroma(DecodeBuffer, PrimaryManifestationComponent);
    Param->CurrDecFrame.pLuma                   = (RV89Dec_LumaAddress_t)Stream->GetDecodeBufferManager()->Luma(DecodeBuffer, VideoDecodeCopy);
    Param->CurrDecFrame.pChroma                 = (RV89Dec_ChromaAddress_t)Stream->GetDecodeBufferManager()->Chroma(DecodeBuffer, VideoDecodeCopy);

    // Fill out the reference frame lists - default to self if not present
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
        ReferenceBuffer             = BufferState[i].Buffer;
        Param->PrevRefFrame.pLuma               = (RV89Dec_LumaAddress_t)Stream->GetDecodeBufferManager()->Luma(ReferenceBuffer, VideoDecodeCopy);
        Param->PrevRefFrame.pChroma             = (RV89Dec_ChromaAddress_t)Stream->GetDecodeBufferManager()->Chroma(ReferenceBuffer, VideoDecodeCopy);
        i                                       = DecodeContext->ReferenceFrameList[0].EntryIndicies[1];
        ReferenceBuffer             = BufferState[i].Buffer;
        Param->PrevMinusOneRefFrame.pLuma       = (RV89Dec_LumaAddress_t)Stream->GetDecodeBufferManager()->Luma(ReferenceBuffer, VideoDecodeCopy);
        Param->PrevMinusOneRefFrame.pChroma     = (RV89Dec_ChromaAddress_t)Stream->GetDecodeBufferManager()->Chroma(ReferenceBuffer, VideoDecodeCopy);
    }

    //{{{  DEBUG
    if (SE_IS_VERBOSE_ON(group_decoder_video))
    {
        SE_VERBOSE(group_decoder_video, "Codec Picture No %d, Picture type %d\n", PictureNo++, Frame->PictureHeader.PictureCodingType);

        for (i = 0; i < Param->InBuffer.NumSegments + 1; i++)
        {
            SE_VERBOSE(group_decoder_video, "      InBuffer.SegmentInfo[%d]             = %d, %d\n", i, SegmentInfo[i].is_valid, SegmentInfo[i].offset);
        }
    }

    //}}}
    // Fill out the actual command
    memset(&Context->BaseContext.MMECommand, 0, sizeof(MME_Command_t));
    Context->BaseContext.MMECommand.CmdStatus.AdditionalInfoSize    = sizeof(RV89Dec_TransformStatusAdditionalInfo_t);
    Context->BaseContext.MMECommand.CmdStatus.AdditionalInfo_p      = (MME_GenericParams_t)(&Context->DecodeStatus);
    Context->BaseContext.MMECommand.ParamSize                       = sizeof(RV89Dec_TransformParams_t);
    Context->BaseContext.MMECommand.Param_p                         = (MME_GenericParams_t)(&Context->DecodeParameters);
    OS_UnLockMutex(&Lock);
    return CodecNoError;
}
//}}}
//{{{  FillOutDecodeBufferRequest
// /////////////////////////////////////////////////////////////////////////
//
//      The specific video function used to fill out a buffer structure
//      request.
//
CodecStatus_t   Codec_MmeVideoRmv_c::FillOutDecodeBufferRequest(DecodeBufferRequest_t     *Request)
{
    RmvFrameParameters_t    *Frame = (RmvFrameParameters_t *)ParsedFrameParameters->FrameParameterStructure;
    //
    // Fill out the standard fields first
    //
    Codec_MmeVideo_c::FillOutDecodeBufferRequest(Request);
    //
    // and now the extended field
    //
    Request->AdditionalMemoryBlockSize  = (Frame->SegmentList.NumSegments + 1) * sizeof(RV89Dec_Segment_Info);
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
CodecStatus_t   Codec_MmeVideoRmv_c::ValidateDecodeContext(CodecBaseDecodeContext_t *Context)
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
CodecStatus_t   Codec_MmeVideoRmv_c::DumpSetStreamParameters(void     *Parameters)
{
    SE_VERBOSE(group_decoder_video, "Stream Params:\n");
    SE_VERBOSE(group_decoder_video, "  MaxWidth              %6u\n", InitializationParameters.MaxWidth);
    SE_VERBOSE(group_decoder_video, "  MaxHeight             %6u\n", InitializationParameters.MaxHeight);
    SE_VERBOSE(group_decoder_video, "  DecoderInterface      %6u\n", InitializationParameters.StreamFormatIdentifier);
    SE_VERBOSE(group_decoder_video, "  isRV8                 %6u\n", InitializationParameters.isRV8);
    SE_VERBOSE(group_decoder_video, "  NumRPRSizes           %6u\n", InitializationParameters.NumRPRSizes);
    SE_VERBOSE(group_decoder_video, "  RetsartTransformer    %6u\n", RestartTransformer);
    return CodecNoError;
}
//}}}
//{{{  DumpDecodeParameters
// /////////////////////////////////////////////////////////////////////////
//
//      Function to dump out the decode
//      parameters from an mme command.
//
CodecStatus_t   Codec_MmeVideoRmv_c::DumpDecodeParameters(void    *Parameters)
{
    RV89Dec_TransformParams_t  *FrameParams     = (RV89Dec_TransformParams_t *)Parameters;
    SE_VERBOSE(group_decoder_video, "  Frame Params\n");
    SE_VERBOSE(group_decoder_video, "      InBuffer.pStartPtr                   = %p\n", FrameParams->InBuffer.pStartPtr);
    SE_VERBOSE(group_decoder_video, "      InBuffer.PictureSize                 = %d\n", FrameParams->InBuffer.PictureSize);
    SE_VERBOSE(group_decoder_video, "      InBuffer.NumSegments                 = %d\n", FrameParams->InBuffer.NumSegments);
    SE_VERBOSE(group_decoder_video, "      InBuffer.pSegmentInfo                = %p\n", FrameParams->InBuffer.pSegmentInfo);
    SE_VERBOSE(group_decoder_video, "      CurrDecFrame.pLuma                   = %p\n", FrameParams->CurrDecFrame.pLuma);
    SE_VERBOSE(group_decoder_video, "      CurrDecFrame.pChroma                 = %p\n", FrameParams->CurrDecFrame.pChroma);
    SE_VERBOSE(group_decoder_video, "      PrevRefFrame.pLuma                   = %p\n", FrameParams->PrevRefFrame.pLuma);
    SE_VERBOSE(group_decoder_video, "      PrevRefFrame.pChroma                 = %p\n", FrameParams->PrevRefFrame.pChroma);
    SE_VERBOSE(group_decoder_video, "      PrevMinusOneRefFrame.pLuma           = %p\n", FrameParams->PrevMinusOneRefFrame.pLuma);
    SE_VERBOSE(group_decoder_video, "      PrevMinusOneRefFrame.pChroma         = %p\n", FrameParams->PrevMinusOneRefFrame.pChroma);
    return CodecNoError;
}
//}}}
//{{{  CheckCodecReturnParameters
// Convert the return code into human readable form.
static const char *LookupError(unsigned int Error)
{
#define E(e) case e: return #e

    switch (Error)
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
CodecStatus_t   Codec_MmeVideoRmv_c::CheckCodecReturnParameters(CodecBaseDecodeContext_t *Context)
{
    MME_Command_t                           *MMECommand       = (MME_Command_t *)(&Context->MMECommand);
    MME_CommandStatus_t                     *CmdStatus        = (MME_CommandStatus_t *)(&MMECommand->CmdStatus);
    RV89Dec_TransformStatusAdditionalInfo_t *AdditionalInfo_p = (RV89Dec_TransformStatusAdditionalInfo_t *)CmdStatus->AdditionalInfo_p;

    if (AdditionalInfo_p != NULL)
    {
        if (AdditionalInfo_p->ErrorCode != RV89DEC_NO_ERROR)
        {
            SE_INFO(group_decoder_video, "%s  %x\n", LookupError(AdditionalInfo_p->ErrorCode), AdditionalInfo_p->ErrorCode);
            Stream->Statistics().FrameDecodeError++;
        }
    }

    return CodecNoError;
}
//}}}
