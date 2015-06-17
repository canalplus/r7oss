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

#include "codec_mme_video_avs.h"
#include "avs.h"
#include "allocinline.h"

#undef TRACE_TAG
#define TRACE_TAG "Codec_MmeVideoAvs_c"

//{{{
typedef struct AvsCodecStreamParameterContext_s
{
    CodecBaseStreamParameterContext_t   BaseContext;

    MME_AVSSetGlobalParamSequence_t     StreamParameters;
} AvsCodecStreamParameterContext_t;

#define BUFFER_AVS_CODEC_STREAM_PARAMETER_CONTEXT             "AvsCodecStreamParameterContext"
#define BUFFER_AVS_CODEC_STREAM_PARAMETER_CONTEXT_TYPE        {BUFFER_AVS_CODEC_STREAM_PARAMETER_CONTEXT, BufferDataTypeBase, AllocateFromOSMemory, 32, 0, true, true, sizeof(AvsCodecStreamParameterContext_t)}

static BufferDataDescriptor_t  AvsCodecStreamParameterContextDescriptor = BUFFER_AVS_CODEC_STREAM_PARAMETER_CONTEXT_TYPE;

// --------

typedef struct AvsCodecDecodeContext_s
{
    CodecBaseDecodeContext_t            BaseContext;

    MME_AVSVideoDecodeParams_t          DecodeParameters;
    MME_AVSVideoDecodeReturnParams_t    DecodeStatus;
} AvsCodecDecodeContext_t;

#define BUFFER_AVS_CODEC_DECODE_CONTEXT       "AvsCodecDecodeContext"
#define BUFFER_AVS_CODEC_DECODE_CONTEXT_TYPE  {BUFFER_AVS_CODEC_DECODE_CONTEXT, BufferDataTypeBase, AllocateFromOSMemory, 32, 0, true, true, sizeof(AvsCodecDecodeContext_t)}

static BufferDataDescriptor_t  AvsCodecDecodeContextDescriptor = BUFFER_AVS_CODEC_DECODE_CONTEXT_TYPE;

//}}}

//
Codec_MmeVideoAvs_c::Codec_MmeVideoAvs_c(void)
    : Codec_MmeVideo_c()
    , DecodingWidth(AVS_MAXIMUM_PICTURE_WIDTH)
    , DecodingHeight(AVS_MAXIMUM_PICTURE_HEIGHT)
    , AvsTransformCapability()
    , AvsInitializationParameters()
    , IntraMbStructMemoryDevice(NULL)
    , MbStructMemoryDevice(NULL)
    , RasterOutput()
{
    Configuration.CodecName                             = "Avs video";
    Configuration.StreamParameterContextCount           = 1;
    Configuration.StreamParameterContextDescriptor      = &AvsCodecStreamParameterContextDescriptor;
    Configuration.DecodeContextCount                    = 4;
    Configuration.DecodeContextDescriptor               = &AvsCodecDecodeContextDescriptor;
    Configuration.MaxDecodeIndicesPerBuffer             = 2;
    Configuration.SliceDecodePermitted                  = false;
    Configuration.DecimatedDecodePermitted              = true;
    Configuration.TransformName[0]                      = AVSDECHD_MME_TRANSFORMER_NAME "0";
    Configuration.TransformName[1]                      = AVSDECHD_MME_TRANSFORMER_NAME "1";
    Configuration.AvailableTransformers                 = 2;
    Configuration.SizeOfTransformCapabilityStructure    = sizeof(AvsTransformCapability);
    Configuration.TransformCapabilityStructurePointer   = (void *)(&AvsTransformCapability);

    // The video firmware violates the MME spec. and passes data buffer addresses
    // as parametric information. For this reason it requires physical addresses
    // to be used.
    Configuration.AddressingMode                        = PhysicalAddress;

    // We do not need the coded data after decode is complete
    Configuration.ShrinkCodedDataBuffersAfterDecode     = true;

    // trick mode parameters
    Configuration.TrickModeParameters.EmpiricalMaximumDecodeFrameRateShortIntegration   = 60;
    Configuration.TrickModeParameters.EmpiricalMaximumDecodeFrameRateLongIntegration    = 60;
    Configuration.TrickModeParameters.DecodeFrameRateShortIntegrationForIOnly       = 60;
    Configuration.TrickModeParameters.DecodeFrameRateLongIntegrationForIOnly        = 60;
    Configuration.TrickModeParameters.SubstandardDecodeSupported        = false;
    Configuration.TrickModeParameters.SubstandardDecodeRateIncrease     = 1;
    Configuration.TrickModeParameters.DefaultGroupSize                  = 12;
    Configuration.TrickModeParameters.DefaultGroupReferenceFrameCount   = 4;
}

//
Codec_MmeVideoAvs_c::~Codec_MmeVideoAvs_c(void)
{
    Halt();

    AllocatorClose(&IntraMbStructMemoryDevice);

    AllocatorClose(&MbStructMemoryDevice);
}

//{{{  HandleCapabilities
// /////////////////////////////////////////////////////////////////////////
//
//      Function to deal with the returned capabilities
//      structure for an avs mme transformer.
//

CodecStatus_t   Codec_MmeVideoAvs_c::HandleCapabilities(void)
{
    // Default to using Omega2 unless Capabilities tell us otherwise
    BufferFormat_t  DisplayFormat = FormatVideo420_MacroBlock;
    // Default elements to produce in the buffers
    DecodeBufferComponentElementMask_t Elements = PrimaryManifestationElement | DecimatedManifestationElement | VideoMacroblockStructureElement;
    DeltaTop_TransformerCapability_t *DeltaTopCapabilities = (DeltaTop_TransformerCapability_t *) Configuration.DeltaTopCapabilityStructurePointer;
    SE_INFO(group_decoder_video, "MME Transformer '%s' capabilities are :-\n", AVSDECHD_MME_TRANSFORMER_NAME);
    SE_DEBUG(group_decoder_video, "    Version                           = %x\n", AvsTransformCapability.api_version);
    RasterOutput = false;

#if (AVS_HD_MME_VERSION >= 18)
    if ((DeltaTopCapabilities != NULL) && (DeltaTopCapabilities->DisplayBufferFormat == DELTA_OUTPUT_RASTER))
    {
        RasterOutput = true;
    }
#endif

    if (RasterOutput)
    {
        DisplayFormat = FormatVideo420_Raster2B;
        Elements |= VideoDecodeCopyElement;
    }

    Stream->GetDecodeBufferManager()->FillOutDefaultList(DisplayFormat, Elements,
                                                         Configuration.ListOfDecodeBufferComponents);
    return CodecNoError;
}
//}}}
//{{{  FillOutTransformerInitializationParameters
// /////////////////////////////////////////////////////////////////////////
//
//      Function to deal with the returned capabilities
//      structure for an avs mme transformer.
//

CodecStatus_t   Codec_MmeVideoAvs_c::FillOutTransformerInitializationParameters(void)
{
    SE_VERBOSE(group_decoder_video, "\n");
    //
    // Fill out the command parameters
    //
    AvsInitializationParameters.CircularBufferBeginAddr_p       = (U32 *)0x00000000;
    AvsInitializationParameters.CircularBufferEndAddr_p         = (U32 *)0xffffffff;
    AvsInitializationParameters.IntraMB_struct_ptr      = NULL;
    //
    // Fill out the actual command
    //
    MMEInitializationParameters.TransformerInitParamsSize       = sizeof(AvsInitializationParameters);
    MMEInitializationParameters.TransformerInitParams_p         = (MME_GenericParams_t)(&AvsInitializationParameters);

    return CodecNoError;
}
//}}}

//{{{  FillOutSetStreamParametersCommand
// /////////////////////////////////////////////////////////////////////////
//
//      Function to fill out the stream parameters
//      structure for an avs mme transformer.
//

CodecStatus_t   Codec_MmeVideoAvs_c::FillOutSetStreamParametersCommand(void)
{
    AvsStreamParameters_t              *Parsed          = (AvsStreamParameters_t *)ParsedFrameParameters->StreamParameterStructure;
    AvsCodecStreamParameterContext_t   *Context         = (AvsCodecStreamParameterContext_t *)StreamParameterContext;
    //
    // Fill out the command parameters
    //
    DecodingWidth                                       = Parsed->SequenceHeader.horizontal_size;
    DecodingHeight                                      = Parsed->SequenceHeader.vertical_size;
    Context->StreamParameters.Width                     = DecodingWidth;
    Context->StreamParameters.Height                    = DecodingHeight;
    Context->StreamParameters.Progressive_sequence      = (AVS_SeqSyntax_t)Parsed->SequenceHeader.progressive_sequence;
    //
    // Fill out the actual command
    //
    memset(&Context->BaseContext.MMECommand, 0, sizeof(MME_Command_t));
    Context->BaseContext.MMECommand.CmdStatus.AdditionalInfoSize        = 0;
    Context->BaseContext.MMECommand.CmdStatus.AdditionalInfo_p          = NULL;
    Context->BaseContext.MMECommand.ParamSize                           = sizeof(Context->StreamParameters);
    Context->BaseContext.MMECommand.Param_p                             = (MME_GenericParams_t)(&Context->StreamParameters);
    return CodecNoError;
}
//}}}
//{{{  FillOutDecodeCommand
// /////////////////////////////////////////////////////////////////////////
//
//      Function to fill out the decode parameters
//      structure for an avs mme transformer.
//

CodecStatus_t   Codec_MmeVideoAvs_c::FillOutDecodeCommand(void)
{
    AvsCodecDecodeContext_t            *Context         = (AvsCodecDecodeContext_t *)DecodeContext;
    AvsFrameParameters_t               *Parsed          = (AvsFrameParameters_t *)ParsedFrameParameters->FrameParameterStructure;
    AvsVideoPictureHeader_t            *PictureHeader   = &Parsed->PictureHeader;
    MME_AVSVideoDecodeParams_t         *Param;
    AVS_DecodedBufferAddress_t         *Decode;
    AVS_RefPicListAddress_t            *RefList;
#if (AVS_HD_MME_VERSION >= 18)
    AVS_DisplayBufferAddress_t          *Display;
#endif
    Buffer_t                            DecodeBuffer;
    Buffer_t                            ReferenceBuffer;
    DecodeBufferComponentType_t         DisplayComponent;
    DecodeBufferComponentType_t         DecimatedComponent;
    DecodeBufferComponentType_t         DecodeComponent;
    unsigned int                        Entry;
    SE_VERBOSE(group_decoder_video, "\n");
    // For avs we do not do slice decodes.
    KnownLastSliceInFieldFrame                  = true;
    OS_LockMutex(&Lock);
    DecodeBuffer                = BufferState[CurrentDecodeBufferIndex].Buffer;
    DisplayComponent            = PrimaryManifestationComponent;
    DecimatedComponent          = DecimatedManifestationComponent;
    DecodeComponent             = RasterOutput ? VideoDecodeCopy : PrimaryManifestationComponent;
    Param                                       = &Context->DecodeParameters;
    Decode                                      = &Param->DecodedBufferAddr;
    RefList                                     = &Param->RefPicListAddr;
#if defined (TRANSFORMER_AVSDEC_HD)
    AVS_StartCodecsParam_t *StartCodes          = &Param->StartCodecs;
#endif
    Decode->Luma_p                              = (AVS_LumaAddress_t)Stream->GetDecodeBufferManager()->Luma(DecodeBuffer, DecodeComponent);
    Decode->Chroma_p                            = (AVS_ChromaAddress_t)Stream->GetDecodeBufferManager()->Chroma(DecodeBuffer, DecodeComponent);
#if (AVS_HD_MME_VERSION >= 18)
    Display                                     = &Param->DisplayBufferAddr;
    Display->StructSize                         = sizeof(AVS_DisplayBufferAddress_t);
    Display->DisplayLuma_p                      = (AVS_LumaAddress_t)Stream->GetDecodeBufferManager()->Luma(DecodeBuffer, DisplayComponent);
    Display->DisplayChroma_p                    = (AVS_ChromaAddress_t)Stream->GetDecodeBufferManager()->Chroma(DecodeBuffer, DisplayComponent);

    if (Stream->GetDecodeBufferManager()->ComponentPresent(DecodeBuffer, DecimatedComponent))
    {
        Display->DisplayDecimatedLuma_p             = (AVS_LumaAddress_t)Stream->GetDecodeBufferManager()->Luma(DecodeBuffer, DecimatedComponent);
        Display->DisplayDecimatedChroma_p           = (AVS_ChromaAddress_t)Stream->GetDecodeBufferManager()->Chroma(DecodeBuffer, DecimatedComponent);
    }
    else
    {
        Display->DisplayDecimatedLuma_p             = NULL;
        Display->DisplayDecimatedChroma_p           = NULL;
    }

#else

    if (Stream->GetDecodeBufferManager()->ComponentPresent(DecodeBuffer, DecimatedComponent))
    {
        Decode->LumaDecimated_p                     = (AVS_LumaAddress_t)Stream->GetDecodeBufferManager()->Luma(DecodeBuffer, DecimatedComponent);
        Decode->ChromaDecimated_p                   = (AVS_ChromaAddress_t)Stream->GetDecodeBufferManager()->Chroma(DecodeBuffer, DecimatedComponent);
    }
    else
    {
        Decode->LumaDecimated_p                     = NULL;
        Decode->ChromaDecimated_p                   = NULL;
    }

#endif
    Decode->MBStruct_p = (unsigned int *)Stream->GetDecodeBufferManager()->ComponentAddress(VideoMacroblockStructure, PhysicalAddress);

    Param->PictureStartAddr_p = (AVS_CompressedData_t)(CodedData + PictureHeader->top_field_offset);
    Param->PictureEndAddr_p   = (AVS_CompressedData_t)(CodedData + CodedDataLength);

    //{{{  Fill out the reference frame lists
    if (ParsedFrameParameters->NumberOfReferenceFrameLists != 0)
    {
        if (DecodeContext->ReferenceFrameList[0].EntryCount > 0)
        {
            Entry                                       = DecodeContext->ReferenceFrameList[0].EntryIndicies[0];
            ReferenceBuffer                             = BufferState[Entry].Buffer;
            RefList->BackwardRefLuma_p                  = (AVS_LumaAddress_t)Stream->GetDecodeBufferManager()->Luma(ReferenceBuffer, DecodeComponent);
            RefList->BackwardRefChroma_p                = (AVS_ChromaAddress_t)Stream->GetDecodeBufferManager()->Chroma(ReferenceBuffer, DecodeComponent);
        }

        if (DecodeContext->ReferenceFrameList[0].EntryCount > 1)
        {
            Entry                                      = DecodeContext->ReferenceFrameList[0].EntryIndicies[1];
            ReferenceBuffer                            = BufferState[Entry].Buffer;
            RefList->ForwardRefLuma_p                  = (AVS_LumaAddress_t)Stream->GetDecodeBufferManager()->Luma(ReferenceBuffer, DecodeComponent);
            RefList->ForwardRefChroma_p                = (AVS_ChromaAddress_t)Stream->GetDecodeBufferManager()->Chroma(ReferenceBuffer, DecodeComponent);
        }
    }

    //}}}
    OS_UnLockMutex(&Lock);
    //{{{  Fill in remaining fields
    Param->Progressive_frame                                            = (AVS_FrameSyntax_t)PictureHeader->progressive_frame;
#if (AVS_HD_MME_VERSION >= 18)
    // build main in omega2 (REF) format instead of raster for now for raster capable transformers else we'll get unexpected raster content in reference frame buffers
    // to be modified as soon as we get 3 distinct buffers to store both ref omega2 + raster display + raster decimated display
    if (RasterOutput && ParsedFrameParameters->ReferenceFrame)
    {
        Param->MainAuxEnable = AVS_REF_MAIN_EN;
    }
    else
    {
        Param->MainAuxEnable = AVS_DISP_MAIN_EN;
    }
#else
    Param->MainAuxEnable                        = AVS_MAINOUT_EN;
#endif
    Param->HorizontalDecimationFactor                                   = AVS_HDEC_1;
    Param->VerticalDecimationFactor                                     = AVS_VDEC_1;
    Param->AebrFlag                                                     = 1;
    Param->Picture_structure                                            = (AVS_PicStruct_t)PictureHeader->picture_structure;
    Param->Picture_structure_bwd                                        = (AVS_PicStruct_t)PictureHeader->picture_structure;
    Param->Fixed_picture_qp                                             = (MME_UINT)PictureHeader->fixed_picture_qp;
    Param->Picture_qp                                                   = (MME_UINT)PictureHeader->picture_qp;
    Param->Skip_mode_flag                                               = (AVS_SkipMode_t)PictureHeader->skip_mode_flag;
    Param->Loop_filter_disable                                          = (MME_UINT)PictureHeader->loop_filter_disable;
    Param->alpha_offset                                                 = (S32)PictureHeader->alpha_c_offset;
    Param->beta_offset                                                  = (S32)PictureHeader->beta_offset;
    Param->Picture_ref_flag                                             = (AVS_PicRef_t)PictureHeader->picture_reference_flag;
    Param->tr                                                           = (S32)PictureHeader->tr;
    Param->imgtr_next_P                                                 = (S32)PictureHeader->imgtr_next_P;
    Param->imgtr_last_P                                                 = (S32)PictureHeader->imgtr_last_P;
    Param->imgtr_last_prev_P                                            = (S32)PictureHeader->imgtr_last_prev_P;
    // To do
    Param->field_flag                                                   = (AVS_FieldSyntax_t)0;
    Param->topfield_pos                                                 = (U32)PictureHeader->top_field_offset;
    Param->botfield_pos                                                 = (U32)PictureHeader->bottom_field_offset;
    Param->DecodingMode                                                 = AVS_NORMAL_DECODE;
    Param->AdditionalFlags                                              = (MME_UINT)0;
    Param->FrameType                                                    = (AVS_PictureType_t)PictureHeader->picture_coding_type;

    if (!Stream->GetDecodeBufferManager()->ComponentPresent(DecodeBuffer, DecimatedComponent))
    {
        // Normal Case
#if (AVS_HD_MME_VERSION >= 18)
        if (RasterOutput && ParsedFrameParameters->ReferenceFrame)
        {
            Param->MainAuxEnable = AVS_REF_MAIN_DISP_MAIN_EN;
        }
        else
        {
            Param->MainAuxEnable = AVS_DISP_MAIN_EN;
        }
#else
        Param->MainAuxEnable                        = AVS_MAINOUT_EN;
#endif
        Param->HorizontalDecimationFactor           = AVS_HDEC_1;
        Param->VerticalDecimationFactor             = AVS_VDEC_1;
    }
    else
    {
#if (AVS_HD_MME_VERSION >= 18)
        if (RasterOutput && ParsedFrameParameters->ReferenceFrame)
        {
            Param->MainAuxEnable = AVS_REF_MAIN_DISP_MAIN_AUX_EN;
        }
        else
        {
            Param->MainAuxEnable = AVS_DISP_AUX_MAIN_EN;
        }
#else
        Param->MainAuxEnable                        = AVS_AUX_MAIN_OUT_EN;
#endif
        Param->HorizontalDecimationFactor           = (Stream->GetDecodeBufferManager()->DecimationFactor(DecodeBuffer, 0) == 2) ?
                                                      AVS_HDEC_ADVANCED_2 :
                                                      AVS_HDEC_ADVANCED_4;

        if (Param->Progressive_frame)
        {
            Param->VerticalDecimationFactor             = AVS_VDEC_ADVANCED_2_PROG;
        }
        else
        {
            Param->VerticalDecimationFactor             = AVS_VDEC_ADVANCED_2_INT;
        }
    }

#if (AVS_HD_MME_VERSION >= 18)
    Param->MainAuxEnable        = AVS_REF_MAIN_DISP_MAIN_EN;
#endif
    //{{{  Fill out slice list if HD decode
#if defined (TRANSFORMER_AVSDEC_HD)
    StartCodes->SliceCount                                              = Parsed->SliceHeaderList.no_slice_headers;

    for (unsigned int i = 0; i < StartCodes->SliceCount; i++)
    {
        StartCodes->SliceArray[i].SliceStartAddrCompressedBuffer_p      = (AVS_CompressedData_t)(CodedData + Parsed->SliceHeaderList.slice_array[i].slice_offset);
        StartCodes->SliceArray[i].SliceAddress                          = Parsed->SliceHeaderList.slice_array[i].slice_start_code;
    }
#endif
    //}}}
    // Fill out the actual command
    memset(&Context->BaseContext.MMECommand, 0, sizeof(MME_Command_t));
    Context->BaseContext.MMECommand.CmdStatus.AdditionalInfoSize        = sizeof(Context->DecodeStatus);
    Context->BaseContext.MMECommand.CmdStatus.AdditionalInfo_p          = (MME_GenericParams_t)(&Context->DecodeStatus);
    Context->BaseContext.MMECommand.ParamSize                           = sizeof(Context->DecodeParameters);
    Context->BaseContext.MMECommand.Param_p                             = (MME_GenericParams_t)(&Context->DecodeParameters);
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
CodecStatus_t   Codec_MmeVideoAvs_c::ValidateDecodeContext(CodecBaseDecodeContext_t *Context)
{
    SE_VERBOSE(group_decoder_video, "\n");
    return CodecNoError;
}
//}}}
//{{{  DumpSetStreamParameters
// /////////////////////////////////////////////////////////////////////////
//
//      Function to dump out the set stream
//      parameters from an mme command.
//

CodecStatus_t   Codec_MmeVideoAvs_c::DumpSetStreamParameters(void    *Parameters)
{
    MME_AVSSetGlobalParamSequence_t    *StreamParameters        = (MME_AVSSetGlobalParamSequence_t *)Parameters;
    SE_VERBOSE(group_decoder_video, "Progressive  %6d\n", StreamParameters->Progressive_sequence);
    return CodecNoError;
}
//}}}
//{{{  DumpDecodeParameters
// /////////////////////////////////////////////////////////////////////////
//
//      Function to dump out the decode
//      parameters from an mme command.
//

CodecStatus_t   Codec_MmeVideoAvs_c::DumpDecodeParameters(void    *Parameters)
{
    MME_AVSVideoDecodeParams_t         *Param           = (MME_AVSVideoDecodeParams_t *)Parameters;
    AVS_DecodedBufferAddress_t         *Decode;
    AVS_RefPicListAddress_t            *RefList;
    Decode                              = &Param->DecodedBufferAddr;
    RefList                             = &Param->RefPicListAddr;
    SE_VERBOSE(group_decoder_video, "  Param->PictureStartAddr_p                  %p\n", Param->PictureStartAddr_p);
    SE_VERBOSE(group_decoder_video, "  Param->PictureEndAddr_p                    %p\n", Param->PictureEndAddr_p);
    SE_VERBOSE(group_decoder_video, "  Decode->Luma_p                             %p\n", Decode->Luma_p);
    SE_VERBOSE(group_decoder_video, "  Decode->Chroma_p                           %p\n", Decode->Chroma_p);
#if (AVS_HD_MME_VERSION < 18)
    SE_VERBOSE(group_decoder_video, "  Decode->LumaDecimated_p                    %p\n", Decode->LumaDecimated_p);
    SE_VERBOSE(group_decoder_video, "  Decode->ChromaDecimated_p                  %p\n", Decode->ChromaDecimated_p);
#endif
    SE_VERBOSE(group_decoder_video, "  Decode->MBStruct__p                        %p\n", Decode->MBStruct_p);
    SE_VERBOSE(group_decoder_video, "  RefList->ForwardRefLuma_p                  %p\n", RefList->ForwardRefLuma_p);
    SE_VERBOSE(group_decoder_video, "  RefList->ForwardRefChroma_p                %p\n", RefList->ForwardRefChroma_p);
    SE_VERBOSE(group_decoder_video, "  RefList->BackwardRefLuma_p                 %p\n", RefList->BackwardRefLuma_p);
    SE_VERBOSE(group_decoder_video, "  RefList->BackwardRefChroma_p               %p\n", RefList->BackwardRefChroma_p);
    SE_VERBOSE(group_decoder_video, "  Param->Progressive_frame                   %x\n", Param->Progressive_frame);
    SE_VERBOSE(group_decoder_video, "  Param->MainAuxEnable                       %x\n", Param->MainAuxEnable);
    SE_VERBOSE(group_decoder_video, "  Param->HorizontalDecimationFactor          %x\n", Param->HorizontalDecimationFactor);
    SE_VERBOSE(group_decoder_video, "  Param->VerticalDecimationFactor            %x\n", Param->VerticalDecimationFactor);
    SE_VERBOSE(group_decoder_video, "  Param->AebrFlag                            %x\n", Param->AebrFlag);
    SE_VERBOSE(group_decoder_video, "  Param->Picture_structure                   %x\n", Param->Picture_structure);
    SE_VERBOSE(group_decoder_video, "  Param->Picture_structure_bwd               %x\n", Param->Picture_structure_bwd);
    SE_VERBOSE(group_decoder_video, "  Param->Fixed_picture_qp                    %x\n", Param->Fixed_picture_qp);
    SE_VERBOSE(group_decoder_video, "  Param->Picture_qp                          %x\n", Param->Picture_qp);
    SE_VERBOSE(group_decoder_video, "  Param->Skip_mode_flag                      %x\n", Param->Skip_mode_flag);
    SE_VERBOSE(group_decoder_video, "  Param->Loop_filter_disable                 %x\n", Param->Loop_filter_disable);
    SE_VERBOSE(group_decoder_video, "  Param->alpha_offset                        %x\n", Param->alpha_offset);
    SE_VERBOSE(group_decoder_video, "  Param->beta_offset                         %x\n", Param->beta_offset);
    SE_VERBOSE(group_decoder_video, "  Param->Picture_ref_flag                    %x\n", Param->Picture_ref_flag);
    SE_VERBOSE(group_decoder_video, "  Param->tr                                  %x\n", Param->tr);
    SE_VERBOSE(group_decoder_video, "  Param->imgtr_next_P                        %x\n", Param->imgtr_next_P);
    SE_VERBOSE(group_decoder_video, "  Param->imgtr_last_P                        %x\n", Param->imgtr_last_P);
    SE_VERBOSE(group_decoder_video, "  Param->imgtr_last_prev_P                   %x\n", Param->imgtr_last_prev_P);
    SE_VERBOSE(group_decoder_video, "  Param->field_flag                          %x\n", Param->field_flag);
    SE_VERBOSE(group_decoder_video, "  Param->DecodingMode                        %x\n", Param->DecodingMode);
    SE_VERBOSE(group_decoder_video, "  Param->AdditionalFlags                     %x\n", Param->AdditionalFlags);
    SE_VERBOSE(group_decoder_video, "  Param->FrameType                           %x\n", Param->FrameType);
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
        E(AVS_DECODER_ERROR_MB_OVERFLOW);
        E(AVS_DECODER_ERROR_RECOVERED);
        E(AVS_DECODER_ERROR_NOT_RECOVERED);
        E(AVS_DECODER_ERROR_TASK_TIMEOUT);

    default: return "AVS_DECODER_UNKNOWN_ERROR";
    }

#undef E
}
CodecStatus_t   Codec_MmeVideoAvs_c::CheckCodecReturnParameters(CodecBaseDecodeContext_t *Context)
{
    MME_Command_t                    *MMECommand       = (MME_Command_t *)(&Context->MMECommand);
    MME_CommandStatus_t              *CmdStatus        = (MME_CommandStatus_t *)(&MMECommand->CmdStatus);
    MME_AVSVideoDecodeReturnParams_t *AdditionalInfo_p = (MME_AVSVideoDecodeReturnParams_t *)CmdStatus->AdditionalInfo_p;

    if (AdditionalInfo_p != NULL)
    {
        if (AdditionalInfo_p->Errortype != AVS_DECODER_NO_ERROR)
        {
            // Set the decode quality to worst because there is a decoding error and there is no emulation.
            // This is done to allow flexibity for not displaying the corrupt output, using player policy, after encountering the decoding error.
            Context->DecodeQuality = 0;
            SE_INFO(group_decoder_video, "%s  0x%x\n", LookupError(AdditionalInfo_p->Errortype), AdditionalInfo_p->Errortype);

            switch (AdditionalInfo_p->Errortype)
            {
            case AVS_DECODER_ERROR_MB_OVERFLOW:
                Stream->Statistics().FrameDecodeMBOverflowError++;
                break;

            case AVS_DECODER_ERROR_RECOVERED:
                Stream->Statistics().FrameDecodeRecoveredError++;
                break;

            case AVS_DECODER_ERROR_NOT_RECOVERED:
                Stream->Statistics().FrameDecodeNotRecoveredError++;
                break;

            case AVS_DECODER_ERROR_TASK_TIMEOUT:
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
