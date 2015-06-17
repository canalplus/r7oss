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

#include "codec_mme_video_vp6.h"
#include "vp6.h"

#undef TRACE_TAG
#define TRACE_TAG "Codec_MmeVideoVp6_c"

//{{{
typedef struct Vp6CodecStreamParameterContext_s
{
    CodecBaseStreamParameterContext_t   BaseContext;
} Vp6CodecStreamParameterContext_t;

typedef struct Vp6CodecDecodeContext_s
{
    CodecBaseDecodeContext_t            BaseContext;
    VP6_TransformParam_t                DecodeParameters;
    VP6_ReturnParams_t                  DecodeStatus;
} Vp6CodecDecodeContext_t;

#define BUFFER_VP6_CODEC_STREAM_PARAMETER_CONTEXT               "Vp6CodecStreamParameterContext"
#define BUFFER_VP6_CODEC_STREAM_PARAMETER_CONTEXT_TYPE {BUFFER_VP6_CODEC_STREAM_PARAMETER_CONTEXT, BufferDataTypeBase, AllocateFromOSMemory, 32, 0, true, true, sizeof(Vp6CodecStreamParameterContext_t)}
static BufferDataDescriptor_t                   Vp6CodecStreamParameterContextDescriptor = BUFFER_VP6_CODEC_STREAM_PARAMETER_CONTEXT_TYPE;

#define BUFFER_VP6_CODEC_DECODE_CONTEXT         "Vp6CodecDecodeContext"
#define BUFFER_VP6_CODEC_DECODE_CONTEXT_TYPE    {BUFFER_VP6_CODEC_DECODE_CONTEXT, BufferDataTypeBase, AllocateFromOSMemory, 32, 0, true, true, sizeof(Vp6CodecDecodeContext_t)}
static BufferDataDescriptor_t                   Vp6CodecDecodeContextDescriptor        = BUFFER_VP6_CODEC_DECODE_CONTEXT_TYPE;

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

Codec_MmeVideoVp6_c::Codec_MmeVideoVp6_c(void)
    : Codec_MmeVideo_c()
    , Vp6TransformCapability()
    , Vp6InitializationParameters()
    , RestartTransformer(true)
    , RasterToOmega(false)
    , DecodingWidth(VP6_DEFAULT_PICTURE_WIDTH)
    , DecodingHeight(VP6_DEFAULT_PICTURE_HEIGHT)
{
    Configuration.CodecName                             = "Vp6 video";
    Configuration.StreamParameterContextCount           = 1;
    Configuration.StreamParameterContextDescriptor      = &Vp6CodecStreamParameterContextDescriptor;
    Configuration.DecodeContextCount                    = 4;
    Configuration.DecodeContextDescriptor               = &Vp6CodecDecodeContextDescriptor;
    Configuration.MaxDecodeIndicesPerBuffer             = 2;
    Configuration.SliceDecodePermitted                  = false;
    Configuration.DecimatedDecodePermitted              = false;
    Configuration.TransformName[0]                      = VP6_MME_TRANSFORMER_NAME "0";
    Configuration.TransformName[1]                      = VP6_MME_TRANSFORMER_NAME "1";
    Configuration.AvailableTransformers                 = 2;
#if 0
    // VP6 codec does not properly implement MME_GetTransformerCapability() and any attempt
    // to do so results in a MME_NOT_IMPLEMENTED error. For this reason it is better not to
    // ask.
    Configuration.SizeOfTransformCapabilityStructure    = 0;
    Configuration.TransformCapabilityStructurePointer   = NULL;
#endif
    Configuration.SizeOfTransformCapabilityStructure    = sizeof(VP6_CapabilityParams_t);
    Configuration.TransformCapabilityStructurePointer   = (void *)(&Vp6TransformCapability);
    Configuration.AddressingMode                        = CachedAddress;
    // We do not need the coded data after decode is complete
    Configuration.ShrinkCodedDataBuffersAfterDecode     = true;

    // To support the Unrestricted Motion Vector feature of the Vp6 codec the buffers must be allocated as follows
    // For macroblock and planar buffers
    // Luma         (width + 32) * (height + 32)
    // Chroma       2 * ((width / 2) + 32) * ((height / 2) + 32)    = Luma / 2
}

Codec_MmeVideoVp6_c::~Codec_MmeVideoVp6_c(void)
{
    Halt();
}

//{{{  HandleCapabilities
// /////////////////////////////////////////////////////////////////////////
//
//      Function to deal with the returned capabilities
//      structure for an Vp6 mme transformer.
//
CodecStatus_t   Codec_MmeVideoVp6_c::HandleCapabilities(void)
{
    BufferFormat_t OutputFormat                 = FormatVideo420_Planar;
    SE_INFO(group_decoder_video, "MME Transformer '%s' capabilities are :-\n", VP6_MME_TRANSFORMER_NAME);
    SE_INFO(group_decoder_video, "  display_buffer_size  %d\n", Vp6TransformCapability.display_buffer_size);
    SE_DEBUG(group_decoder_video, "  packed_buffer_size   %d\n", Vp6TransformCapability.packed_buffer_size);
    SE_DEBUG(group_decoder_video, "  packed_buffer_total  %d\n", Vp6TransformCapability.packed_buffer_total);
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
    Configuration.ListOfDecodeBufferComponents[0].DefaultAddressMode    = CachedAddress;
    Configuration.ListOfDecodeBufferComponents[0].ComponentBorders[0]   = 32;
    Configuration.ListOfDecodeBufferComponents[0].ComponentBorders[1]   = 32;
    Configuration.ListOfDecodeBufferComponents[1].DataType              = RasterToOmega ? FormatVideo420_Planar : FormatVideo420_MacroBlock;
    Configuration.ListOfDecodeBufferComponents[1].DefaultAddressMode    = CachedAddress;
    Configuration.ListOfDecodeBufferComponents[1].ComponentBorders[0]   = 32;
    Configuration.ListOfDecodeBufferComponents[1].ComponentBorders[1]   = 32;
    return CodecNoError;
}
//}}}
//{{{  FillOutTransformerInitializationParameters
// /////////////////////////////////////////////////////////////////////////
//
//      Function to deal with the returned capabilities
//      structure for an Vp6 mme transformer.
//
CodecStatus_t   Codec_MmeVideoVp6_c::FillOutTransformerInitializationParameters(void)
{
    // Fill out the command parameters
    Vp6InitializationParameters.CodedWidth                      = DecodingWidth;
    Vp6InitializationParameters.CodedHeight                     = DecodingHeight;
    Vp6InitializationParameters.CodecVersion                    = VP6f;
    SE_INFO(group_decoder_video, "DecodingWidth              %6u\n", DecodingWidth);
    SE_INFO(group_decoder_video, "DecodingHeight             %6u\n", DecodingHeight);
    // Fill out the actual command
    MMEInitializationParameters.TransformerInitParamsSize       = sizeof(VP6_InitTransformerParam_t);
    MMEInitializationParameters.TransformerInitParams_p         = (MME_GenericParams_t)(&Vp6InitializationParameters);
    return CodecNoError;
}
//}}}
//{{{  SendMMEStreamParameters
CodecStatus_t Codec_MmeVideoVp6_c::SendMMEStreamParameters(void)
{
    CodecStatus_t       CodecStatus     = CodecNoError;
    unsigned int        MMEStatus       = MME_SUCCESS;

    // There are no set stream parameters for Vp6 decoder so the transformer is
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
//      structure for an Vp6 mme transformer.
//
CodecStatus_t   Codec_MmeVideoVp6_c::FillOutSetStreamParametersCommand(void)
{
    Vp6StreamParameters_t              *Parsed  = (Vp6StreamParameters_t *)ParsedFrameParameters->StreamParameterStructure;

    if ((Parsed->SequenceHeader.encoded_width != DecodingWidth) || (Parsed->SequenceHeader.encoded_height != DecodingHeight))
    {
        DecodingWidth           = Parsed->SequenceHeader.encoded_width;
        DecodingHeight          = Parsed->SequenceHeader.encoded_height;
        RestartTransformer      = true;
    }

    return CodecNoError;
}
//}}}
//{{{  FillOutDecodeCommand
// /////////////////////////////////////////////////////////////////////////
//
//      Function to fill out the decode parameters
//      structure for an Vp6 mme transformer.
//

CodecStatus_t   Codec_MmeVideoVp6_c::FillOutDecodeCommand(void)
{
    Vp6CodecDecodeContext_t            *Context        = (Vp6CodecDecodeContext_t *)DecodeContext;
    Vp6FrameParameters_t               *Frame          = (Vp6FrameParameters_t *)ParsedFrameParameters->FrameParameterStructure;
    Vp6StreamParameters_t              *vp6StreamParams         = (Vp6StreamParameters_t *)ParsedFrameParameters->StreamParameterStructure;
    VP6_TransformParam_t               *Param;
    VP6_ParamPicture_t                 *Picture;
    unsigned int                        i;
    Buffer_t                DecodeBuffer;
    //
    // Invert the usage of the buffers when we change display mode
    //
    DecodeBufferComponentType_t RasterComponent        = RasterToOmega ? VideoDecodeCopy : PrimaryManifestationComponent;
    DecodeBufferComponentType_t OmegaComponent         = RasterToOmega ? PrimaryManifestationComponent : VideoDecodeCopy;
    //
    // For Vp6 we do not do slice decodes.
    //
    KnownLastSliceInFieldFrame                  = true;
    OS_LockMutex(&Lock);
    DecodeBuffer                = BufferState[CurrentDecodeBufferIndex].Buffer;
    //
    // Fill out the straight forward command parameters
    //
    Param                                       = &Context->DecodeParameters;
    Picture                                     = &Param->PictureParameters;
    Picture->StructSize                         = sizeof(VP6_ParamPicture_t);
    Picture->KeyFrame                           = Frame->PictureHeader.ptype == VP6_PICTURE_CODING_TYPE_I;
    Picture->SampleVarianceThreshold            = vp6StreamParams->SequenceHeader.variance_threshold;
    Picture->Deblock_Filtering                  = Frame->PictureHeader.deblock_filtering;
    Picture->Quant                              = Frame->PictureHeader.pquant;
    Picture->MaxVectorLength                    = vp6StreamParams->SequenceHeader.max_vector_length;
    Picture->FilterMode                         = vp6StreamParams->SequenceHeader.filter_mode;
    Picture->FilterSelection                    = vp6StreamParams->SequenceHeader.filter_selection;
    // The following three items are retrieved directly from the range decoder in the frame parser
    Picture->high                               = Frame->PictureHeader.high;
    Picture->bits                               = Frame->PictureHeader.bits;
    Picture->code_word                          = Frame->PictureHeader.code_word;
    Picture->PictureStartOffset                 = Frame->PictureHeader.offset;
    Picture->PictureStopOffset                  = CodedDataLength;
    //
    // Fill out the actual command
    //
    memset(&DecodeContext->MMECommand, 0, sizeof(MME_Command_t));
    DecodeContext->MMECommand.CmdStatus.AdditionalInfoSize      = sizeof(VP6_ReturnParams_t);
    DecodeContext->MMECommand.CmdStatus.AdditionalInfo_p        = (MME_GenericParams_t *)&Context->DecodeStatus;
    DecodeContext->MMECommand.StructSize                        = sizeof(MME_Command_t);
    DecodeContext->MMECommand.CmdCode                           = MME_TRANSFORM;
    DecodeContext->MMECommand.CmdEnd                            = MME_COMMAND_END_RETURN_NOTIFY;
    DecodeContext->MMECommand.DueTime                           = (MME_Time_t)0;
    DecodeContext->MMECommand.NumberInputBuffers                = VP6_NUM_MME_INPUT_BUFFERS;
    DecodeContext->MMECommand.NumberOutputBuffers               = VP6_NUM_MME_OUTPUT_BUFFERS;
    DecodeContext->MMECommand.DataBuffers_p                     = (MME_DataBuffer_t **)DecodeContext->MMEBufferList;
    DecodeContext->MMECommand.ParamSize                         = sizeof(VP6_TransformParam_t);
    DecodeContext->MMECommand.Param_p                           = (MME_GenericParams_t)Param;

    //{{{  Fill in details for all buffers
    for (i = 0; i < VP6_NUM_MME_BUFFERS; i++)
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
    DecodeContext->MMEBuffers[VP6_MME_CODED_DATA_BUFFER].ScatterPages_p[0].Page_p              = (void *)CodedData;
    DecodeContext->MMEBuffers[VP6_MME_CODED_DATA_BUFFER].TotalSize                             = CodedDataLength;
    //}}}
    //{{{  Current output decoded buffer
    DecodeContext->MMEBuffers[VP6_MME_CURRENT_FRAME_BUFFER_RASTER].ScatterPages_p[0].Page_p    = (void *)Stream->GetDecodeBufferManager()->ComponentBaseAddress(DecodeBuffer, RasterComponent);
    DecodeContext->MMEBuffers[VP6_MME_CURRENT_FRAME_BUFFER_RASTER].TotalSize                   = Stream->GetDecodeBufferManager()->ComponentSize(DecodeBuffer, RasterComponent);

    //}}}
    //{{{  Previous decoded buffer - get its planar buffer
    if (DecodeContext->ReferenceFrameList[0].EntryCount > 0)
    {
        unsigned int    Entry   = DecodeContext->ReferenceFrameList[0].EntryIndicies[0];
        Buffer_t    ReferenceBuffer = BufferState[Entry].Buffer;
        DecodeContext->MMEBuffers[VP6_MME_REFERENCE_FRAME_BUFFER].ScatterPages_p[0].Page_p     = (void *)Stream->GetDecodeBufferManager()->ComponentBaseAddress(ReferenceBuffer, RasterComponent);
        DecodeContext->MMEBuffers[VP6_MME_REFERENCE_FRAME_BUFFER].TotalSize                    = Stream->GetDecodeBufferManager()->ComponentSize(ReferenceBuffer, RasterComponent);
    }

    //}}}
    //{{{  Golden frame decoded buffer - get its planar buffer
    if (DecodeContext->ReferenceFrameList[0].EntryCount == 2)
    {
        unsigned int    Entry   = DecodeContext->ReferenceFrameList[0].EntryIndicies[1];
        Buffer_t    ReferenceBuffer = BufferState[Entry].Buffer;
        DecodeContext->MMEBuffers[VP6_MME_GOLDEN_FRAME_BUFFER].ScatterPages_p[0].Page_p        = (void *)Stream->GetDecodeBufferManager()->ComponentBaseAddress(ReferenceBuffer, RasterComponent);
        DecodeContext->MMEBuffers[VP6_MME_GOLDEN_FRAME_BUFFER].TotalSize                       = Stream->GetDecodeBufferManager()->ComponentSize(ReferenceBuffer, RasterComponent);
    }

    //}}}
    //{{{  Current output macroblock buffer luma
    DecodeContext->MMEBuffers[VP6_MME_CURRENT_FRAME_BUFFER_LUMA].ScatterPages_p[0].Page_p      = (void *)Stream->GetDecodeBufferManager()->Luma(DecodeBuffer, OmegaComponent);
    DecodeContext->MMEBuffers[VP6_MME_CURRENT_FRAME_BUFFER_LUMA].TotalSize                     = Stream->GetDecodeBufferManager()->LumaSize(DecodeBuffer, OmegaComponent);
    //}}}
    //{{{  Current output macroblock buffer chroma
    DecodeContext->MMEBuffers[VP6_MME_CURRENT_FRAME_BUFFER_CHROMA].ScatterPages_p[0].Page_p    = (void *)Stream->GetDecodeBufferManager()->Chroma(DecodeBuffer, OmegaComponent);
    DecodeContext->MMEBuffers[VP6_MME_CURRENT_FRAME_BUFFER_CHROMA].TotalSize                   = Stream->GetDecodeBufferManager()->ChromaSize(DecodeBuffer, OmegaComponent);
    //}}}

    //{{{  Initialise remaining scatter page values
    for (i = 0; i < VP6_NUM_MME_BUFFERS; i++)
    {
        // Only one scatterpage, so size = totalsize
        DecodeContext->MMEBuffers[i].ScatterPages_p[0].Size         = DecodeContext->MMEBuffers[i].TotalSize;
        DecodeContext->MMEBuffers[i].ScatterPages_p[0].BytesUsed    = 0;
        DecodeContext->MMEBuffers[i].ScatterPages_p[0].FlagsIn      = 0;
        DecodeContext->MMEBuffers[i].ScatterPages_p[0].FlagsOut     = 0;
    }

    //}}}

    if (SE_IS_VERBOSE_ON(group_decoder_video))
    {
        for (i = 0; i < (VP6_NUM_MME_BUFFERS); i++)
        {
            SE_VERBOSE(group_decoder_video, "Buffer List      (%d)  %p\n", i, DecodeContext->MMEBufferList[i]);
            SE_VERBOSE(group_decoder_video, "Struct Size      (%d)  %x\n", i, DecodeContext->MMEBuffers[i].StructSize);
            SE_VERBOSE(group_decoder_video, "User Data p      (%d)  %p\n", i, DecodeContext->MMEBuffers[i].UserData_p);
            SE_VERBOSE(group_decoder_video, "Flags            (%d)  %x\n", i, DecodeContext->MMEBuffers[i].Flags);
            SE_VERBOSE(group_decoder_video, "Stream Number    (%d)  %x\n", i, DecodeContext->MMEBuffers[i].StreamNumber);
            SE_VERBOSE(group_decoder_video, "No Scatter Pages (%d)  %x\n", i, DecodeContext->MMEBuffers[i].NumberOfScatterPages);
            SE_VERBOSE(group_decoder_video, "Scatter Pages p  (%d)  %p\n", i, DecodeContext->MMEBuffers[i].ScatterPages_p[0].Page_p);
            SE_VERBOSE(group_decoder_video, "Start Offset     (%d)  %x\n\n", i, DecodeContext->MMEBuffers[i].StartOffset);
        }

        SE_VERBOSE(group_decoder_video, "Additional Size  %x\n", DecodeContext->MMECommand.CmdStatus.AdditionalInfoSize);
        SE_VERBOSE(group_decoder_video, "Additional p     %p\n", DecodeContext->MMECommand.CmdStatus.AdditionalInfo_p);
        SE_VERBOSE(group_decoder_video, "Param Size       %x\n", DecodeContext->MMECommand.ParamSize);
        SE_VERBOSE(group_decoder_video, "Param p          %p\n", DecodeContext->MMECommand.Param_p);
    }

    OS_UnLockMutex(&Lock);
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
CodecStatus_t   Codec_MmeVideoVp6_c::ValidateDecodeContext(CodecBaseDecodeContext_t *Context)
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
CodecStatus_t   Codec_MmeVideoVp6_c::DumpSetStreamParameters(void    *Parameters)
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

unsigned int Vp6StaticPicture;
CodecStatus_t   Codec_MmeVideoVp6_c::DumpDecodeParameters(void    *Parameters)
{
    SE_VERBOSE(group_decoder_video, "Picture %d %p\n", Vp6StaticPicture++, Parameters);
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
        E(VP6_DECODER_ERROR_TASK_TIMEOUT);
    default: return "VP6_DECODER_UNKNOWN_ERROR";
    }

#undef E
}
CodecStatus_t   Codec_MmeVideoVp6_c::CheckCodecReturnParameters(CodecBaseDecodeContext_t *Context)
{
    MME_Command_t       *MMECommand       = (MME_Command_t *)(&Context->MMECommand);
    MME_CommandStatus_t *CmdStatus        = (MME_CommandStatus_t *)(&MMECommand->CmdStatus);
    VP6_ReturnParams_t  *AdditionalInfo_p = (VP6_ReturnParams_t *)CmdStatus->AdditionalInfo_p;

    if (AdditionalInfo_p != NULL)
    {
        if (AdditionalInfo_p->Errortype != VP6_NO_ERROR)
        {
            // Set the decode quality to worst because there is a decoding error and there is no emulation.
            // This is done to allow flexibity for not displaying the corrupt output, using player policy, after encountering the decoding error.
            Context->DecodeQuality = 0;
            SE_INFO(group_decoder_video, "%s  0x%x\n", LookupError(AdditionalInfo_p->Errortype), AdditionalInfo_p->Errortype);
            switch (AdditionalInfo_p->Errortype)
            {
            case VP6_DECODER_ERROR_TASK_TIMEOUT:
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
