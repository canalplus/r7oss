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

#include "codec_mme_video_vp8.h"
#include "vp8.h"

#undef TRACE_TAG
#define TRACE_TAG "Codec_MmeVideoVp8_c"

//{{{
typedef struct Vp8CodecStreamParameterContext_s
{
    CodecBaseStreamParameterContext_t   BaseContext;
} Vp8CodecStreamParameterContext_t;

typedef struct Vp8CodecDecodeContext_s
{
    CodecBaseDecodeContext_t            BaseContext;
    VP8_TransformParam_t                DecodeParameters;
    VP8_ReturnParams_t                  DecodeStatus;
} Vp8CodecDecodeContext_t;

#define BUFFER_VP8_CODEC_STREAM_PARAMETER_CONTEXT               "Vp8CodecStreamParameterContext"
#define BUFFER_VP8_CODEC_STREAM_PARAMETER_CONTEXT_TYPE {BUFFER_VP8_CODEC_STREAM_PARAMETER_CONTEXT, BufferDataTypeBase, AllocateFromOSMemory, 32, 0, true, true, sizeof(Vp8CodecStreamParameterContext_t)}
static BufferDataDescriptor_t                   Vp8CodecStreamParameterContextDescriptor = BUFFER_VP8_CODEC_STREAM_PARAMETER_CONTEXT_TYPE;

#define BUFFER_VP8_CODEC_DECODE_CONTEXT         "Vp8CodecDecodeContext"
#define BUFFER_VP8_CODEC_DECODE_CONTEXT_TYPE    {BUFFER_VP8_CODEC_DECODE_CONTEXT, BufferDataTypeBase, AllocateFromOSMemory, 32, 0, true, true, sizeof(Vp8CodecDecodeContext_t)}
static BufferDataDescriptor_t                   Vp8CodecDecodeContextDescriptor        = BUFFER_VP8_CODEC_DECODE_CONTEXT_TYPE;

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

Codec_MmeVideoVp8_c::Codec_MmeVideoVp8_c(void)
    : Codec_MmeVideo_c()
    , Vp8TransformCapability()
    , Vp8InitializationParameters()
    , RestartTransformer(true)
    , RasterToOmega(false)
    , DecodingWidth(VP8_DEFAULT_PICTURE_WIDTH)
    , DecodingHeight(VP8_DEFAULT_PICTURE_HEIGHT)
{
    Configuration.CodecName                             = "Vp8 video";
    Configuration.StreamParameterContextCount           = 1;
    Configuration.StreamParameterContextDescriptor      = &Vp8CodecStreamParameterContextDescriptor;
    Configuration.DecodeContextCount                    = 4;
    Configuration.DecodeContextDescriptor               = &Vp8CodecDecodeContextDescriptor;
    Configuration.MaxDecodeIndicesPerBuffer             = 2;
    Configuration.SliceDecodePermitted                  = false;
    Configuration.DecimatedDecodePermitted              = false;
    Configuration.TransformName[0]                      = VP8DEC_MME_TRANSFORMER_NAME "0";
    Configuration.TransformName[1]                      = VP8DEC_MME_TRANSFORMER_NAME "1";
    Configuration.AvailableTransformers                 = 2;
#if 0
    // VP8 codec does not properly implement MME_GetTransformerCapability() and any attempt
    // to do so results in a MME_NOT_IMPLEMENTED error. For this reason it is better not to
    // ask.
    Configuration.SizeOfTransformCapabilityStructure    = 0;
    Configuration.TransformCapabilityStructurePointer   = NULL;
#endif
    Configuration.SizeOfTransformCapabilityStructure    = sizeof(VP8_CapabilityParams_t);
    Configuration.TransformCapabilityStructurePointer   = (void *)(&Vp8TransformCapability);
    Configuration.AddressingMode                        = CachedAddress;
    // We do not need the coded data after decode is complete
    Configuration.ShrinkCodedDataBuffersAfterDecode     = true;
}

Codec_MmeVideoVp8_c::~Codec_MmeVideoVp8_c(void)
{
    Halt();
}

//{{{  HandleCapabilities
// /////////////////////////////////////////////////////////////////////////
//
//      Function to deal with the returned capabilities
//      structure for an Vp8 mme transformer.
//
CodecStatus_t   Codec_MmeVideoVp8_c::HandleCapabilities(void)
{
    BufferFormat_t OutputFormat = FormatVideo420_Raster2B;
    SE_INFO(group_decoder_video, "MME Transformer '%s' capabilities are :-\n", VP8DEC_MME_TRANSFORMER_NAME);
    SE_INFO(group_decoder_video, "  display_buffer_size  %d\n", Vp8TransformCapability.display_buffer_size);
    SE_DEBUG(group_decoder_video, "  packed_buffer_size   %d\n", Vp8TransformCapability.packed_buffer_size);
    SE_DEBUG(group_decoder_video, "  packed_buffer_total  %d\n", Vp8TransformCapability.packed_buffer_total);

    //
    // We can query the firmware here to determine if we can/should convert to
    // an Omega Surface format output
    //

    if (Vp8TransformCapability.IsHantroG1Here)
    {
        Stream->GetDecodeBufferManager()->FillOutDefaultList(OutputFormat,
                                                             PrimaryManifestationElement,
                                                             Configuration.ListOfDecodeBufferComponents);
        Configuration.AddressingMode                        = PhysicalAddress;
        RasterToOmega      = false;
        Configuration.ListOfDecodeBufferComponents[0].DefaultAddressMode    = PhysicalAddress;
        Configuration.ListOfDecodeBufferComponents[0].ComponentBorders[0]   = 16;
        Configuration.ListOfDecodeBufferComponents[0].ComponentBorders[1]   = 16;
        Configuration.ListOfDecodeBufferComponents[1].DataType              = RasterToOmega ? FormatVideo420_MacroBlock : FormatVideo420_Raster2B ;
        Configuration.ListOfDecodeBufferComponents[1].DefaultAddressMode    = PhysicalAddress;
        Configuration.ListOfDecodeBufferComponents[1].ComponentBorders[0]   = 16;
        Configuration.ListOfDecodeBufferComponents[1].ComponentBorders[1]   = 16;
    }
    else
    {
        RasterToOmega      = true;
        OutputFormat       = FormatVideo420_MacroBlock;
        Stream->GetDecodeBufferManager()->FillOutDefaultList(OutputFormat,
                                                             PrimaryManifestationElement | VideoDecodeCopyElement,
                                                             Configuration.ListOfDecodeBufferComponents);
        Configuration.ListOfDecodeBufferComponents[0].DefaultAddressMode    = CachedAddress;
        Configuration.ListOfDecodeBufferComponents[0].ComponentBorders[0]   = 32;
        Configuration.ListOfDecodeBufferComponents[0].ComponentBorders[1]   = 32;
        Configuration.ListOfDecodeBufferComponents[1].DataType              = RasterToOmega ? FormatVideo420_MacroBlock : FormatVideo420_Raster2B ;
        Configuration.ListOfDecodeBufferComponents[1].DefaultAddressMode    = CachedAddress;
        Configuration.ListOfDecodeBufferComponents[1].ComponentBorders[0]   = 32;
        Configuration.ListOfDecodeBufferComponents[1].ComponentBorders[1]   = 32;
    }

    SE_INFO(group_decoder_video, "  packed_buffer_total  %d\n", Vp8TransformCapability.packed_buffer_total);
    return CodecNoError;
}
//}}}
//{{{  FillOutTransformerInitializationParameters
// /////////////////////////////////////////////////////////////////////////
//
//      Function to deal with the returned capabilities
//      structure for an Vp8 mme transformer.
//
CodecStatus_t   Codec_MmeVideoVp8_c::FillOutTransformerInitializationParameters(void)
{
    // Fill out the command parameters
    Vp8InitializationParameters.CodedWidth                      = DecodingWidth;
    Vp8InitializationParameters.CodedHeight                     = DecodingHeight;
    SE_INFO(group_decoder_video, "DecodingWidth              %6u\n", DecodingWidth);
    SE_INFO(group_decoder_video, "DecodingHeight             %6u\n", DecodingHeight);
    // Fill out the actual command
    MMEInitializationParameters.TransformerInitParamsSize       = sizeof(VP8_InitTransformerParam_t);
    MMEInitializationParameters.TransformerInitParams_p         = (MME_GenericParams_t)(&Vp8InitializationParameters);
    return CodecNoError;
}
//}}}
//{{{  SendMMEStreamParameters
CodecStatus_t Codec_MmeVideoVp8_c::SendMMEStreamParameters(void)
{
    CodecStatus_t       CodecStatus     = CodecNoError;
    unsigned int        MMEStatus       = MME_SUCCESS;

    // There are no set stream parameters for Vp8 decoder so the transformer is
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
        MMEStatus = MME_InitTransformer(Configuration.TransformName[SelectedTransformer],
                                        &MMEInitializationParameters, &MMEHandle);

        if (MMEStatus ==  MME_SUCCESS)
        {
            SE_DEBUG(group_decoder_video, "New Stream Params %dx%d\n", DecodingWidth, DecodingHeight);
            CodecStatus                                         = CodecNoError;
            RestartTransformer                                  = false;
            ParsedFrameParameters->NewStreamParameters          = false;
            MMEInitialized                                      = true;
        }
        else
        {
            CodecStatus = CodecError;
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
//      structure for an Vp8 mme transformer.
//
CodecStatus_t   Codec_MmeVideoVp8_c::FillOutSetStreamParametersCommand(void)
{
    Vp8StreamParameters_t              *Parsed  = (Vp8StreamParameters_t *)ParsedFrameParameters->StreamParameterStructure;

    if ((Parsed->SequenceHeader.decode_width != DecodingWidth) || (Parsed->SequenceHeader.decode_height != DecodingHeight))
    {
        //DecodingWidth           = Parsed->SequenceHeader.encoded_width;
        //DecodingHeight          = Parsed->SequenceHeader.encoded_height;
        DecodingWidth           = Parsed->SequenceHeader.decode_width;
        DecodingHeight          = Parsed->SequenceHeader.decode_height;
        RestartTransformer      = true;
    }

    return CodecNoError;
}
//}}}
//{{{  FillOutDecodeCommand
// /////////////////////////////////////////////////////////////////////////
//
//      Function to fill out the decode parameters
//      structure for an Vp8 mme transformer.
//

CodecStatus_t   Codec_MmeVideoVp8_c::FillOutDecodeCommand(void)
{
    Vp8CodecDecodeContext_t *Context               = (Vp8CodecDecodeContext_t *)DecodeContext;
    Vp8FrameParameters_t    *ParsedPicture         = (Vp8FrameParameters_t *) ParsedFrameParameters->FrameParameterStructure;
    Vp8VideoPicture_t        ParsedPictureHeader   = (Vp8VideoPicture_t)ParsedPicture->PictureHeader;
    Vp8StreamParameters_t   *ParsedSequence        = (Vp8StreamParameters_t *)ParsedFrameParameters->StreamParameterStructure;
    VP8_TransformParam_t               *Param;
    VP8_ParamPicture_t                 *Picture;
    unsigned int                        i;
    Buffer_t                            DecodeBuffer;
    unsigned int                        Entry;
    Buffer_t                            ReferenceBuffer;
    //
    // Invert the usage of the buffers when we change display mode
    //
    DecodeBufferComponentType_t RasterComponent        = RasterToOmega ? VideoDecodeCopy : PrimaryManifestationComponent;
    DecodeBufferComponentType_t OmegaComponent         = RasterToOmega ? PrimaryManifestationComponent : VideoDecodeCopy;
    //
    // For Vp8 we do not do slice decodes.
    //
    KnownLastSliceInFieldFrame  = true;
    OS_LockMutex(&Lock);
    DecodeBuffer = BufferState[CurrentDecodeBufferIndex].Buffer;
    //
    // Fill out the straight forward command parameters
    //
    Param                            = &Context->DecodeParameters;
    Picture                          = &Param->PictureParameters;
    Picture->PictureStartOffset      = 0;//Frame->PictureHeader.offset;
    Picture->PictureStopOffset       = CodedDataLength + 32;
    Picture->KeyFrame                = ParsedFrameParameters->KeyFrame;
    Picture->ShowFrame               = ParsedPictureHeader.show_frame;
    Picture->vpVersion               = ParsedSequence->SequenceHeader.version;
    Picture->offsetToDctParts        = ParsedPictureHeader.first_part_size;
    Picture->encoded_width           = ParsedSequence->SequenceHeader.encoded_width;
    Picture->encoded_height          = ParsedSequence->SequenceHeader.encoded_height;
    Picture->decode_width            = ParsedSequence->SequenceHeader.decode_width;
    Picture->decode_height           = ParsedSequence->SequenceHeader.decode_height;
    Picture->horizontal_scale        = ParsedSequence->SequenceHeader.horizontal_scale;
    Picture->vertical_scale          = ParsedSequence->SequenceHeader.vertical_scale;
    Picture->colour_space            = ParsedSequence->SequenceHeader.colour_space;
    Picture->clamping                = ParsedSequence->SequenceHeader.clamping;
    Picture->filter_type             = ParsedPictureHeader.filter_type;
    Picture->loop_filter_level       = ParsedPictureHeader.loop_filter_level;
    Picture->sharpness_level         = ParsedPictureHeader.sharpness_level;
    //
    // Fill out the actual command
    //
    memset(&DecodeContext->MMECommand, 0, sizeof(MME_Command_t));
    DecodeContext->MMECommand.CmdStatus.AdditionalInfoSize      = sizeof(VP8_ReturnParams_t);
    DecodeContext->MMECommand.CmdStatus.AdditionalInfo_p        = (MME_GenericParams_t *)&Context->DecodeStatus;
    DecodeContext->MMECommand.StructSize                        = sizeof(MME_Command_t);
    DecodeContext->MMECommand.CmdCode                           = MME_TRANSFORM;
    DecodeContext->MMECommand.CmdEnd                            = MME_COMMAND_END_RETURN_NOTIFY;
    DecodeContext->MMECommand.DueTime                           = (MME_Time_t)0;
    DecodeContext->MMECommand.NumberInputBuffers                = VP8_NUM_MME_INPUT_BUFFERS;
    DecodeContext->MMECommand.NumberOutputBuffers               = VP8_NUM_MME_OUTPUT_BUFFERS;
    DecodeContext->MMECommand.DataBuffers_p                     = (MME_DataBuffer_t **)DecodeContext->MMEBufferList;
    DecodeContext->MMECommand.ParamSize                         = sizeof(VP8_TransformParam_t);
    DecodeContext->MMECommand.Param_p                           = (MME_GenericParams_t)Param;

    //{{{  Fill in details for all buffers
    for (i = 0; i < VP8_NUM_MME_BUFFERS; i++)
    {
        DecodeContext->MMEBufferList[i]                             = &DecodeContext->MMEBuffers[i];
        DecodeContext->MMEBuffers[i].StructSize                     = sizeof(MME_DataBuffer_t);
        DecodeContext->MMEBuffers[i].UserData_p                     = NULL;
        DecodeContext->MMEBuffers[i].Flags                          = 0;
        DecodeContext->MMEBuffers[i].StreamNumber                   = 0;

        if (Vp8TransformCapability.IsHantroG1Here)
        {
            DecodeContext->MMEBuffers[i].NumberOfScatterPages           = 0;
        }
        else
        {
            DecodeContext->MMEBuffers[i].NumberOfScatterPages           = 1;
        }

        DecodeContext->MMEBuffers[i].ScatterPages_p                 = &DecodeContext->MMEPages[i];
        DecodeContext->MMEBuffers[i].StartOffset                    = 0;
    }

    //}}}
    // Then overwrite bits specific to other buffers
    //{{{  Input compressed data buffer - need pointer to coded data and its length

    if (Vp8TransformCapability.IsHantroG1Here)
    {
        Param->CodedData = (unsigned int) CodedData;
        Param->CodedData_Cp = (unsigned int) CodedData_Cp;
        Param->CodedDataSize = CodedDataLength ;
        Param->CurrentFB_Luma_p = (unsigned int)Stream->GetDecodeBufferManager()->Luma(DecodeBuffer, RasterComponent);
        Param->CurrentFB_Chroma_p = (unsigned int)Stream->GetDecodeBufferManager()->Chroma(DecodeBuffer, RasterComponent);
        Param->FrameBuffer_Size = (unsigned int)Stream->GetDecodeBufferManager()->ComponentSize(DecodeBuffer, RasterComponent);

        if (DecodeContext->ReferenceFrameList[0].EntryCount > 0)
        {
            Entry   = DecodeContext->ReferenceFrameList[0].EntryIndicies[0];
            ReferenceBuffer = BufferState[Entry].Buffer;
            Param->LastRefFB_Luma_p = (unsigned int)Stream->GetDecodeBufferManager()->Luma(ReferenceBuffer, RasterComponent);
            Param->LastRefFB_Chroma_p = (unsigned int)Stream->GetDecodeBufferManager()->Chroma(ReferenceBuffer, RasterComponent);
        }

        if (DecodeContext->ReferenceFrameList[1].EntryCount > 0)
        {
            Entry   = DecodeContext->ReferenceFrameList[1].EntryIndicies[0];
            ReferenceBuffer = BufferState[Entry].Buffer;
            Param->GoldenFB_Luma_p = (unsigned int)Stream->GetDecodeBufferManager()->Luma(ReferenceBuffer, RasterComponent);
            Param->GoldenFB_Chroma_p = (unsigned int)Stream->GetDecodeBufferManager()->Chroma(ReferenceBuffer, RasterComponent);
        }

        if (DecodeContext->ReferenceFrameList[2].EntryCount > 0)
        {
            Entry   = DecodeContext->ReferenceFrameList[2].EntryIndicies[0];
            ReferenceBuffer = BufferState[Entry].Buffer;
            Param->AlternateFB_Luma_p = (unsigned int)Stream->GetDecodeBufferManager()->Luma(ReferenceBuffer, RasterComponent);
            Param->AlternateFB_Chroma_p = (unsigned int)Stream->GetDecodeBufferManager()->Chroma(ReferenceBuffer, RasterComponent);
        }
    }
    else /*Vp8TransformCapability.IsHantroG1Here*/
    {
        DecodeContext->MMEBuffers[VP8_MME_CODED_DATA_BUFFER].ScatterPages_p[0].Page_p              = (void *)CodedData;
        DecodeContext->MMEBuffers[VP8_MME_CODED_DATA_BUFFER].TotalSize                             = CodedDataLength;
        //}}}
        //{{{  Current output decoded buffer
        DecodeContext->MMEBuffers[VP8_MME_CURRENT_FRAME_BUFFER_RASTER].ScatterPages_p[0].Page_p    = (void *)Stream->GetDecodeBufferManager()->ComponentBaseAddress(DecodeBuffer, RasterComponent);
        DecodeContext->MMEBuffers[VP8_MME_CURRENT_FRAME_BUFFER_RASTER].TotalSize                   = Stream->GetDecodeBufferManager()->ComponentSize(DecodeBuffer, RasterComponent);

        //}}}
        //{{{  Previous decoded buffer - get its planar buffer
        if (DecodeContext->ReferenceFrameList[0].EntryCount > 0)
        {
            Entry   = DecodeContext->ReferenceFrameList[0].EntryIndicies[0];
            ReferenceBuffer = BufferState[Entry].Buffer;
            DecodeContext->MMEBuffers[VP8_MME_REFERENCE_FRAME_BUFFER].ScatterPages_p[0].Page_p     = (void *)Stream->GetDecodeBufferManager()->ComponentBaseAddress(ReferenceBuffer, RasterComponent);
            DecodeContext->MMEBuffers[VP8_MME_REFERENCE_FRAME_BUFFER].TotalSize                    = Stream->GetDecodeBufferManager()->ComponentSize(ReferenceBuffer, RasterComponent);
        }

        //}}}
        //{{{  Golden frame decoded buffer - get its planar buffer
        if (DecodeContext->ReferenceFrameList[1].EntryCount > 0)
        {
            Entry   = DecodeContext->ReferenceFrameList[1].EntryIndicies[0];
            ReferenceBuffer = BufferState[Entry].Buffer;
            DecodeContext->MMEBuffers[VP8_MME_GOLDEN_FRAME_BUFFER].ScatterPages_p[0].Page_p        = (void *)Stream->GetDecodeBufferManager()->ComponentBaseAddress(ReferenceBuffer, RasterComponent);
            DecodeContext->MMEBuffers[VP8_MME_GOLDEN_FRAME_BUFFER].TotalSize                       = Stream->GetDecodeBufferManager()->ComponentSize(ReferenceBuffer, RasterComponent);
        }

        //}}}
        //{{{  Alternate reference frame decoded buffer - get its planar buffer
        if (DecodeContext->ReferenceFrameList[2].EntryCount > 0)
        {
            Entry   = DecodeContext->ReferenceFrameList[2].EntryIndicies[0];
            ReferenceBuffer = BufferState[Entry].Buffer;
            DecodeContext->MMEBuffers[VP8_MME_ALTERNATE_FRAME_BUFFER].ScatterPages_p[0].Page_p      = (void *)Stream->GetDecodeBufferManager()->ComponentBaseAddress(ReferenceBuffer, RasterComponent);
            DecodeContext->MMEBuffers[VP8_MME_ALTERNATE_FRAME_BUFFER].TotalSize                     = Stream->GetDecodeBufferManager()->ComponentSize(ReferenceBuffer, RasterComponent);
        }

        //}}}
        //{{{  Current output macroblock buffer luma
        DecodeContext->MMEBuffers[VP8_MME_CURRENT_FRAME_BUFFER_LUMA].ScatterPages_p[0].Page_p      = (void *)Stream->GetDecodeBufferManager()->Luma(DecodeBuffer, OmegaComponent);
        DecodeContext->MMEBuffers[VP8_MME_CURRENT_FRAME_BUFFER_LUMA].TotalSize                     = Stream->GetDecodeBufferManager()->LumaSize(DecodeBuffer, OmegaComponent);
        //}}}
        //{{{  Current output macroblock buffer chroma
        DecodeContext->MMEBuffers[VP8_MME_CURRENT_FRAME_BUFFER_CHROMA].ScatterPages_p[0].Page_p    = (void *)Stream->GetDecodeBufferManager()->Chroma(DecodeBuffer, OmegaComponent);
        DecodeContext->MMEBuffers[VP8_MME_CURRENT_FRAME_BUFFER_CHROMA].TotalSize                   = Stream->GetDecodeBufferManager()->ChromaSize(DecodeBuffer, OmegaComponent);

        //}}}
        //{{{  Initialise remaining scatter page values
        for (i = 0; i < VP8_NUM_MME_BUFFERS; i++)
        {
            // Only one scatterpage, so size = totalsize
            DecodeContext->MMEBuffers[i].ScatterPages_p[0].Size         = DecodeContext->MMEBuffers[i].TotalSize;
            DecodeContext->MMEBuffers[i].ScatterPages_p[0].BytesUsed    = 0;
            DecodeContext->MMEBuffers[i].ScatterPages_p[0].FlagsIn      = 0;
            DecodeContext->MMEBuffers[i].ScatterPages_p[0].FlagsOut     = 0;
        }

        //}}}
    }/*Vp8TransformCapability.IsHantroG1Here*/

    if (SE_IS_VERBOSE_ON(group_decoder_video))
    {
        for (i = 0; i < (VP8_NUM_MME_BUFFERS); i++)
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

//
// Low power enter method
// For CPS mode, all MME transformers must be terminated
//
CodecStatus_t Codec_MmeVideoVp8_c::LowPowerEnter(void)
{
    CodecStatus_t status = CodecNoError;

    if (DecodeContext)
    {
        status = TerminateMMETransformer();
    }

    return status;
}

//
// Low power exit method
// For CPS mode, all MME transformers must be re-initialized
//
CodecStatus_t Codec_MmeVideoVp8_c::LowPowerExit(void)
{
    CodecStatus_t status = CodecNoError;

    if (DecodeContext)
    {
        status = InitializeMMETransformer();
    }

    return status;
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
CodecStatus_t   Codec_MmeVideoVp8_c::ValidateDecodeContext(CodecBaseDecodeContext_t *Context)
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
CodecStatus_t   Codec_MmeVideoVp8_c::DumpSetStreamParameters(void    *Parameters)
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

unsigned int Vp8StaticPicture;
CodecStatus_t   Codec_MmeVideoVp8_c::DumpDecodeParameters(void    *Parameters)
{
    SE_VERBOSE(group_decoder_video, "Picture %d %p\n", Vp8StaticPicture, Parameters);
    Vp8StaticPicture++;
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
        E(VP8_DECODER_ERROR_TASK_TIMEOUT); /* Decoder task timeout has occurred */
#if (VP8DEC_MME_VERSION >= 12)
        E(VP8_CORRUPT_PACKET); /* Decoder received corrupt/incomplete packet */
        E(VP8_INVALID_PICTURE_SIZE); /* Picture width\height are corrupt/not supported */
        E(VP8_INVALID_FRAME_SYNC_CODE); /* Invalid frame sync code */
        E(VP8_MEM_ERROR); /* Invalid memory pointer\ memory allocation error */
        E(G1VP8_PARAM_ERROR);
        E(G1VP8_STRM_ERROR);
        E(G1VP8_NOT_INITIALIZED);
        E(G1VP8_MEMFAIL);
        E(G1VP8_INITFAIL);
        E(G1VP8_HDRS_NOT_RDY);
        E(G1VP8_STREAM_NOT_SUPPORTED);
        E(G1VP8_HW_RESERVED);
        E(G1VP8_HW_TIMEOUT);
        E(G1VP8_HW_BUS_ERROR);
        E(G1VP8_SYSTEM_ERROR);
        E(G1VP8_DWL_ERROR);
        E(G1VP8_EVALUATION_LIMIT_EXCEEDED);
        E(G1VP8_FORMAT_NOT_SUPPORTED);
#endif
    default: return "VP8_DECODER_UNKNOWN_ERROR";
    }

#undef E
}
CodecStatus_t   Codec_MmeVideoVp8_c::CheckCodecReturnParameters(CodecBaseDecodeContext_t *Context)
{
    MME_Command_t       *MMECommand       = (MME_Command_t *)(&Context->MMECommand);
    MME_CommandStatus_t *CmdStatus        = (MME_CommandStatus_t *)(&MMECommand->CmdStatus);
    VP8_ReturnParams_t  *AdditionalInfo_p = (VP8_ReturnParams_t *)CmdStatus->AdditionalInfo_p;

    if (AdditionalInfo_p != NULL)
    {
        if (AdditionalInfo_p->Errortype != VP8_NO_ERROR)
        {
            // Set the decode quality to worst because there is a decoding error and there is no emulation.
            // This is done to allow flexibity for not displaying the corrupt output, using player policy, after encountering the decoding error.
            Context->DecodeQuality = 0;
            SE_INFO(group_decoder_video, "%s  0x%x\n", LookupError(AdditionalInfo_p->Errortype), AdditionalInfo_p->Errortype);
            switch (AdditionalInfo_p->Errortype)
            {
            case VP8_DECODER_ERROR_TASK_TIMEOUT:
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
