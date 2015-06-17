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

#include "codec_mme_video_flv1.h"
#include "h263.h"

#undef TRACE_TAG
#define TRACE_TAG "Codec_MmeVideoFlv1_c"

//{{{
typedef struct Flv1CodecStreamParameterContext_s
{
    CodecBaseStreamParameterContext_t   BaseContext;
} Flv1CodecStreamParameterContext_t;

typedef struct Flv1CodecDecodeContext_s
{
    CodecBaseDecodeContext_t            BaseContext;
    FLV1_TransformParam_t               DecodeParameters;
    FLV1_ReturnParams_t                 DecodeStatus;
} Flv1CodecDecodeContext_t;

#define BUFFER_FLV1_CODEC_STREAM_PARAMETER_CONTEXT               "Flv1CodecStreamParameterContext"
#define BUFFER_FLV1_CODEC_STREAM_PARAMETER_CONTEXT_TYPE {BUFFER_FLV1_CODEC_STREAM_PARAMETER_CONTEXT, BufferDataTypeBase, AllocateFromOSMemory, 32, 0, true, true, sizeof(Flv1CodecStreamParameterContext_t)}
static BufferDataDescriptor_t                   Flv1CodecStreamParameterContextDescriptor = BUFFER_FLV1_CODEC_STREAM_PARAMETER_CONTEXT_TYPE;

#define BUFFER_FLV1_CODEC_DECODE_CONTEXT        "Flv1CodecDecodeContext"
#define BUFFER_FLV1_CODEC_DECODE_CONTEXT_TYPE   {BUFFER_FLV1_CODEC_DECODE_CONTEXT, BufferDataTypeBase, AllocateFromOSMemory, 32, 0, true, true, sizeof(Flv1CodecDecodeContext_t)}
static BufferDataDescriptor_t                   Flv1CodecDecodeContextDescriptor        = BUFFER_FLV1_CODEC_DECODE_CONTEXT_TYPE;


//}}}

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

Codec_MmeVideoFlv1_c::Codec_MmeVideoFlv1_c(void)
    : Codec_MmeVideo_c()
    , Flv1InitializationParameters()
    , RestartTransformer(true)
    , RasterToOmega(false)
    , DecodingWidth(H263_WIDTH_CIF)
    , DecodingHeight(H263_HEIGHT_CIF)
{
    Configuration.CodecName                             = "Flv1 video";
    Configuration.StreamParameterContextCount           = 1;
    Configuration.StreamParameterContextDescriptor      = &Flv1CodecStreamParameterContextDescriptor;
    Configuration.DecodeContextCount                    = 4;
    Configuration.DecodeContextDescriptor               = &Flv1CodecDecodeContextDescriptor;
    Configuration.MaxDecodeIndicesPerBuffer             = 2;
    Configuration.SliceDecodePermitted                  = false;
    Configuration.DecimatedDecodePermitted              = false;
    Configuration.TransformName[0]                      = FLV1_MME_TRANSFORMER_NAME "0";
    Configuration.TransformName[1]                      = FLV1_MME_TRANSFORMER_NAME "1";
    Configuration.AvailableTransformers                 = 2;
    // FLV1 codec does not properly implement MME_GetTransformerCapability() and any attempt
    // to do so results in a MME_NOT_IMPLEMENTED error. For this reason it is better not to
    // ask.
    Configuration.SizeOfTransformCapabilityStructure    = 0;
    Configuration.TransformCapabilityStructurePointer   = NULL;
    Configuration.AddressingMode                        = CachedAddress;
    // We do not need the coded data after decode is complete
    Configuration.ShrinkCodedDataBuffersAfterDecode     = true;
}

Codec_MmeVideoFlv1_c::~Codec_MmeVideoFlv1_c(void)
{
    Halt();
}

//{{{  HandleCapabilities
// /////////////////////////////////////////////////////////////////////////
//
//      Function to deal with the returned capabilities
//      structure for an Flv1 mme transformer.
//
CodecStatus_t   Codec_MmeVideoFlv1_c::HandleCapabilities(void)
{
    BufferFormat_t OutputFormat                 = FormatVideo420_Planar;
    SE_INFO(group_decoder_video, "MME Transformer '%s' capabilities are :-\n", FLV1_MME_TRANSFORMER_NAME);
    //
    // We can query the firmware here to determine if we can/should convert to
    // an Omega Surface format output
    //
    RasterToOmega      = true;

    if (RasterToOmega)
    {
        SE_DEBUG(group_decoder_video, "  OmegaOutput Conversion enabled\n");
        OutputFormat = FormatVideo420_MacroBlock;
    }

    Stream->GetDecodeBufferManager()->FillOutDefaultList(OutputFormat,
                                                         PrimaryManifestationElement | VideoDecodeCopyElement,
                                                         Configuration.ListOfDecodeBufferComponents);
    //
    // Configure the Buffers for UnCached Addresses
    //
    Configuration.ListOfDecodeBufferComponents[0].DefaultAddressMode    = CachedAddress;
    Configuration.ListOfDecodeBufferComponents[0].ComponentBorders[0]   = 16;
    Configuration.ListOfDecodeBufferComponents[0].ComponentBorders[1]   = 16;
    Configuration.ListOfDecodeBufferComponents[1].DataType              = RasterToOmega ? FormatVideo420_Planar : FormatVideo420_MacroBlock;
    Configuration.ListOfDecodeBufferComponents[1].DefaultAddressMode    = CachedAddress;
    Configuration.ListOfDecodeBufferComponents[1].ComponentBorders[0]   = 16;
    Configuration.ListOfDecodeBufferComponents[1].ComponentBorders[1]   = 16;
    return CodecNoError;
}
//}}}
//{{{  InitializeMMETransformer
///////////////////////////////////////////////////////////////////////////
///
/// Verify that the transformer exists
///
CodecStatus_t   Codec_MmeVideoFlv1_c::InitializeMMETransformer(void)
{
    MME_ERROR                        MMEStatus;
    MME_TransformerCapability_t      Capability;
    memset(&Capability, 0, sizeof(MME_TransformerCapability_t));
    memset(Configuration.TransformCapabilityStructurePointer, 0, Configuration.SizeOfTransformCapabilityStructure);
    Capability.StructSize           = sizeof(MME_TransformerCapability_t);
    Capability.TransformerInfoSize  = Configuration.SizeOfTransformCapabilityStructure;
    Capability.TransformerInfo_p    = Configuration.TransformCapabilityStructurePointer;
    MMEStatus                       = MME_GetTransformerCapability(Configuration.TransformName[SelectedTransformer], &Capability);

    if (MMEStatus != MME_SUCCESS)
    {
        if (MMEStatus == MME_UNKNOWN_TRANSFORMER)
        {
            SE_ERROR("(%s) - Transformer %s not found\n", Configuration.CodecName, Configuration.TransformName[SelectedTransformer]);
        }
        else
        {
            SE_ERROR("(%s:%s) - Unable to read capabilities (%08x)\n", Configuration.CodecName, Configuration.TransformName[SelectedTransformer], MMEStatus);
        }

        return CodecError;
    }

    return HandleCapabilities();
}
//}}}
//{{{  FillOutTransformerInitializationParameters
// /////////////////////////////////////////////////////////////////////////
//
//      Function to deal with the returned capabilities
//      structure for an Flv1 mme transformer.
//
CodecStatus_t   Codec_MmeVideoFlv1_c::FillOutTransformerInitializationParameters(void)
{
    // Fill out the command parameters
    Flv1InitializationParameters.codec_id                       = FLV1;
    Flv1InitializationParameters.CodedWidth                     = DecodingWidth;
    Flv1InitializationParameters.CodedHeight                    = DecodingHeight;
    SE_INFO(group_decoder_video, "DecodingWidth              %6u\n", DecodingWidth);
    SE_INFO(group_decoder_video, "DecodingHeight             %6u\n", DecodingHeight);
    // Fill out the actual command
    MMEInitializationParameters.TransformerInitParamsSize       = sizeof(FLV1_InitTransformerParam_t);
    MMEInitializationParameters.TransformerInitParams_p         = (MME_GenericParams_t)(&Flv1InitializationParameters);
    return CodecNoError;
}
//}}}
//{{{  SendMMEStreamParameters
CodecStatus_t Codec_MmeVideoFlv1_c::SendMMEStreamParameters(void)
{
    CodecStatus_t       CodecStatus     = CodecNoError;
    unsigned int        MMEStatus       = MME_SUCCESS;

    // There are no set stream parameters for Flv1 decoder so the transformer is
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
            SE_DEBUG(group_decoder_video, "New Stream Params %dx%d\n", DecodingWidth, DecodingHeight);
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
//      structure for an Flv1 mme transformer.
//
CodecStatus_t   Codec_MmeVideoFlv1_c::FillOutSetStreamParametersCommand(void)
{
    H263StreamParameters_t                     *Parsed  = (H263StreamParameters_t *)ParsedFrameParameters->StreamParameterStructure;

    if ((Parsed->SequenceHeader.width != DecodingWidth) || (Parsed->SequenceHeader.height != DecodingHeight))
    {
        DecodingWidth           = Parsed->SequenceHeader.width;
        DecodingHeight          = Parsed->SequenceHeader.height;
        RestartTransformer      = true;
    }

    return CodecNoError;
}
//}}}
//{{{  FillOutDecodeCommand
// /////////////////////////////////////////////////////////////////////////
//
//      Function to fill out the decode parameters
//      structure for an Flv1 mme transformer.
//

CodecStatus_t   Codec_MmeVideoFlv1_c::FillOutDecodeCommand(void)
{
    Flv1CodecDecodeContext_t           *Context        = (Flv1CodecDecodeContext_t *)DecodeContext;
    H263FrameParameters_t              *Frame          = (H263FrameParameters_t *)ParsedFrameParameters->FrameParameterStructure;
    //Flv1StreamParameters_t*             Stream         = (Flv1StreamParameters_t *)ParsedFrameParameters->StreamParameterStructure;
    FLV1_TransformParam_t              *Param;
    FLV1_ParamPicture_t                *Picture;
    unsigned int                        i;
    Buffer_t                DecodeBuffer;
    //
    // Invert the usage of the buffers when we change display mode
    //
    DecodeBufferComponentType_t RasterComponent        = RasterToOmega ? VideoDecodeCopy : PrimaryManifestationComponent;
    DecodeBufferComponentType_t OmegaComponent         = RasterToOmega ? PrimaryManifestationComponent : VideoDecodeCopy;
    //
    // For Flv1 we do not do slice decodes.
    //
    KnownLastSliceInFieldFrame                  = true;
    OS_LockMutex(&Lock);
    DecodeBuffer                = BufferState[CurrentDecodeBufferIndex].Buffer;
    //
    // Fill out the straight forward command parameters
    //
    Param                                       = &Context->DecodeParameters;
    Picture                                     = &Param->PictureParameters;
    Picture->StructSize                         = sizeof(FLV1_ParamPicture_t);
#if (FLV1_MME_VERSION < 10)
    Picture->EnableDeblocking                   = Frame->PictureHeader.dflag;
#endif
    Picture->PicType                            = (Frame->PictureHeader.ptype == H263_PICTURE_CODING_TYPE_I) ? FLV1_DECODE_PICTURE_TYPE_I :
                                                  FLV1_DECODE_PICTURE_TYPE_P;
    Picture->H263Flv                            = Frame->PictureHeader.version + 1;             // Flv1 -> 2
    Picture->ChromaQscale                       = Frame->PictureHeader.pquant;
    Picture->Qscale                             = Frame->PictureHeader.pquant;
    Picture->Dropable                           = Frame->PictureHeader.ptype > H263_PICTURE_CODING_TYPE_P;
    Picture->index                              = Frame->PictureHeader.bit_offset;
    Picture->PictureStartOffset                 = 0;                                            // Picture starts at beginning of buffer
    Picture->PictureStopOffset                  = CodedDataLength + 32;
    Picture->size_in_bits                       = CodedDataLength * 8;
    //
    // Fill out the actual command
    //
    memset(&DecodeContext->MMECommand, 0, sizeof(MME_Command_t));
    DecodeContext->MMECommand.CmdStatus.AdditionalInfoSize      = sizeof(FLV1_ReturnParams_t);
    DecodeContext->MMECommand.CmdStatus.AdditionalInfo_p        = (MME_GenericParams_t *)&Context->DecodeStatus;
    DecodeContext->MMECommand.StructSize                        = sizeof(MME_Command_t);
    DecodeContext->MMECommand.CmdCode                           = MME_TRANSFORM;
    DecodeContext->MMECommand.CmdEnd                            = MME_COMMAND_END_RETURN_NOTIFY;
    DecodeContext->MMECommand.DueTime                           = (MME_Time_t)0;
    DecodeContext->MMECommand.NumberInputBuffers                = FLV1_NUM_MME_INPUT_BUFFERS;
    DecodeContext->MMECommand.NumberOutputBuffers               = FLV1_NUM_MME_OUTPUT_BUFFERS;
    DecodeContext->MMECommand.DataBuffers_p                     = (MME_DataBuffer_t **)DecodeContext->MMEBufferList;
    DecodeContext->MMECommand.ParamSize                         = sizeof(FLV1_TransformParam_t);
    DecodeContext->MMECommand.Param_p                           = (MME_GenericParams_t)Param;

    //{{{  Fill in details for all buffers
    for (i = 0; i < FLV1_NUM_MME_BUFFERS; i++)
    {
        DecodeContext->MMEBufferList[i]                             = &DecodeContext->MMEBuffers[i];
        DecodeContext->MMEBuffers[i].StructSize                     = sizeof(MME_DataBuffer_t);
        DecodeContext->MMEBuffers[i].UserData_p                     = NULL;
        DecodeContext->MMEBuffers[i].Flags                          = 0;
        DecodeContext->MMEBuffers[i].StreamNumber                   = 0;
        DecodeContext->MMEBuffers[i].NumberOfScatterPages           = 1;
        DecodeContext->MMEBuffers[i].ScatterPages_p                 = &DecodeContext->MMEPages[i];
        DecodeContext->MMEBuffers[i].StartOffset                    = 0;
    }

    //}}}
    // Then overwrite bits specific to other buffers
    //{{{  Input compressed data buffer - need pointer to coded data and its length
    DecodeContext->MMEBuffers[FLV1_MME_CODED_DATA_BUFFER].ScatterPages_p[0].Page_p              = (void *)CodedData;
    DecodeContext->MMEBuffers[FLV1_MME_CODED_DATA_BUFFER].TotalSize                             = CodedDataLength;
    //}}}
    //{{{  Current output decoded buffer
    DecodeContext->MMEBuffers[FLV1_MME_CURRENT_FRAME_BUFFER_RASTER].ScatterPages_p[0].Page_p    = (void *)Stream->GetDecodeBufferManager()->ComponentBaseAddress(DecodeBuffer, RasterComponent);
    DecodeContext->MMEBuffers[FLV1_MME_CURRENT_FRAME_BUFFER_RASTER].TotalSize                   = Stream->GetDecodeBufferManager()->ComponentSize(DecodeBuffer, RasterComponent);

    //}}}
    //{{{  Previous decoded buffer - get its planar buffer
    if (DecodeContext->ReferenceFrameList[0].EntryCount == 1)
    {
        unsigned int    Entry   = DecodeContext->ReferenceFrameList[0].EntryIndicies[0];
        Buffer_t    ReferenceBuffer = BufferState[Entry].Buffer;
        DecodeContext->MMEBuffers[FLV1_MME_REFERENCE_FRAME_BUFFER].ScatterPages_p[0].Page_p     = (void *)Stream->GetDecodeBufferManager()->ComponentBaseAddress(ReferenceBuffer, RasterComponent);
        DecodeContext->MMEBuffers[FLV1_MME_REFERENCE_FRAME_BUFFER].TotalSize                    = Stream->GetDecodeBufferManager()->ComponentSize(ReferenceBuffer, RasterComponent);
    }

    //}}}
    //{{{  Current output macroblock buffer luma
    DecodeContext->MMEBuffers[FLV1_MME_CURRENT_FRAME_BUFFER_LUMA].ScatterPages_p[0].Page_p      = (void *)Stream->GetDecodeBufferManager()->Luma(DecodeBuffer, OmegaComponent);
    DecodeContext->MMEBuffers[FLV1_MME_CURRENT_FRAME_BUFFER_LUMA].TotalSize                     = Stream->GetDecodeBufferManager()->LumaSize(DecodeBuffer, OmegaComponent);
    //}}}
    //{{{  Current output macroblock buffer chroma
    DecodeContext->MMEBuffers[FLV1_MME_CURRENT_FRAME_BUFFER_CHROMA].ScatterPages_p[0].Page_p    = (void *)Stream->GetDecodeBufferManager()->Chroma(DecodeBuffer, OmegaComponent);
    DecodeContext->MMEBuffers[FLV1_MME_CURRENT_FRAME_BUFFER_CHROMA].TotalSize                   = Stream->GetDecodeBufferManager()->ChromaSize(DecodeBuffer, OmegaComponent);
    //}}}

    //{{{  Initialise remaining scatter page values
    for (i = 0; i < FLV1_NUM_MME_BUFFERS; i++)
    {
        // Only one scatterpage, so size = totalsize
        DecodeContext->MMEBuffers[i].ScatterPages_p[0].Size         = DecodeContext->MMEBuffers[i].TotalSize;
        DecodeContext->MMEBuffers[i].ScatterPages_p[0].BytesUsed    = 0;
        DecodeContext->MMEBuffers[i].ScatterPages_p[0].FlagsIn      = 0;
        DecodeContext->MMEBuffers[i].ScatterPages_p[0].FlagsOut     = 0;
    }

    //}}}
    OS_UnLockMutex(&Lock);

    if (SE_IS_VERBOSE_ON(group_decoder_video))
    {
        for (i = 0; i < (FLV1_NUM_MME_BUFFERS); i++)
        {
            SE_VERBOSE(group_decoder_video, "  Buffer List      (%d)  %p\n", i, DecodeContext->MMEBufferList[i]);
            SE_VERBOSE(group_decoder_video, "  Struct Size      (%d)  %x\n", i, DecodeContext->MMEBuffers[i].StructSize);
            SE_VERBOSE(group_decoder_video, "  User Data p      (%d)  %p\n", i, DecodeContext->MMEBuffers[i].UserData_p);
            SE_VERBOSE(group_decoder_video, "  Flags            (%d)  %x\n", i, DecodeContext->MMEBuffers[i].Flags);
            SE_VERBOSE(group_decoder_video, "  Stream Number    (%d)  %x\n", i, DecodeContext->MMEBuffers[i].StreamNumber);
            SE_VERBOSE(group_decoder_video, "  No Scatter Pages (%d)  %x\n", i, DecodeContext->MMEBuffers[i].NumberOfScatterPages);
            SE_VERBOSE(group_decoder_video, "  Scatter Pages p  (%d)  %p\n", i, DecodeContext->MMEBuffers[i].ScatterPages_p[0].Page_p);
            SE_VERBOSE(group_decoder_video, "  Start Offset     (%d)  %x\n\n", i, DecodeContext->MMEBuffers[i].StartOffset);
        }

        SE_VERBOSE(group_decoder_video, "  Additional Size  %x\n", DecodeContext->MMECommand.CmdStatus.AdditionalInfoSize);
        SE_VERBOSE(group_decoder_video, "  Additional p     %p\n", DecodeContext->MMECommand.CmdStatus.AdditionalInfo_p);
        SE_VERBOSE(group_decoder_video, "  Param Size       %x\n", DecodeContext->MMECommand.ParamSize);
        SE_VERBOSE(group_decoder_video, "  Param p          %p\n", DecodeContext->MMECommand.Param_p);
    }

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
CodecStatus_t   Codec_MmeVideoFlv1_c::ValidateDecodeContext(CodecBaseDecodeContext_t *Context)
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
CodecStatus_t   Codec_MmeVideoFlv1_c::DumpSetStreamParameters(void    *Parameters)
{
    return CodecNoError;
}
//}}}
//{{{  DumpDecodeParameters
// /////////////////////////////////////////////////////////////////////////
//
//      Function to dump out the decode
//      parameters from an mme command.
//

unsigned int Flv1StaticPicture;
CodecStatus_t   Codec_MmeVideoFlv1_c::DumpDecodeParameters(void    *Parameters)
{
    Flv1StaticPicture++;
    SE_VERBOSE(group_decoder_video, "Picture %d %p\n", Flv1StaticPicture - 1, Parameters);
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
#if (FLV1_MME_VERSION >= 12)
        E(FLV1_DECODER_ERROR_RECOVERED);
        E(FLV1_DECODER_ERROR_NOT_RECOVERED);
#endif
        E(FLV1_DECODER_ERROR_TASK_TIMEOUT);

    default: return "FLV1_DECODER_UNKNOWN_ERROR";
    }

#undef E
}
CodecStatus_t   Codec_MmeVideoFlv1_c::CheckCodecReturnParameters(CodecBaseDecodeContext_t *Context)
{
    MME_Command_t       *MMECommand       = (MME_Command_t *)(&Context->MMECommand);
    MME_CommandStatus_t *CmdStatus        = (MME_CommandStatus_t *)(&MMECommand->CmdStatus);
    FLV1_ReturnParams_t *AdditionalInfo_p = (FLV1_ReturnParams_t *)CmdStatus->AdditionalInfo_p;

    if (AdditionalInfo_p != NULL)
    {
        if (AdditionalInfo_p->Errortype != FLV1_NO_ERROR)
        {
            // Set the decode quality to worst because there is a decoding error and there is no emulation.
            // This is done to allow flexibity for not displaying the corrupt output, using player policy, after encountering the decoding error.
            Context->DecodeQuality = 0;
            SE_INFO(group_decoder_video, "%s  0x%x\n", LookupError(AdditionalInfo_p->Errortype), AdditionalInfo_p->Errortype);

            switch (AdditionalInfo_p->Errortype)
            {
#if (FLV1_MME_VERSION >= 12)
            case FLV1_DECODER_ERROR_RECOVERED:
                Stream->Statistics().FrameDecodeRecoveredError++;
                break;

            case FLV1_DECODER_ERROR_NOT_RECOVERED:
                Stream->Statistics().FrameDecodeNotRecoveredError++;
                break;
#endif
            case FLV1_DECODER_ERROR_TASK_TIMEOUT:
                Stream->Statistics().FrameDecodeErrorTaskTimeOutError++;
                break;

            default:
                Stream->Statistics().FrameDecodeError++;
                break;
            }
        }
    }

    return CodecNoError;
}
//}}}
