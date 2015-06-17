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

#include "codec_mme_video_mpeg2.h"
#include "mpeg2.h"
#include "player_generic.h"
#include "player_playback.h"
#include "player_stream.h"

#undef TRACE_TAG
#define TRACE_TAG "Codec_MmeVideoMpeg2_c"

static QuantiserMatrix_t DefaultIntraQuantizationMatrix =
{
#if 0
    // Scan ordered
    0x08, 0x10, 0x10, 0x13, 0x10, 0x13, 0x16, 0x16,
    0x16, 0x16, 0x16, 0x16, 0x1A, 0x18, 0x1A, 0x1B,
    0x1B, 0x1B, 0x1A, 0x1A, 0x1A, 0x1A, 0x1B, 0x1B,
    0x1B, 0x1D, 0x1D, 0x1D, 0x22, 0x22, 0x22, 0x1D,
    0x1D, 0x1D, 0x1B, 0x1B, 0x1D, 0x1D, 0x20, 0x20,
    0x22, 0x22, 0x25, 0x26, 0x25, 0x23, 0x23, 0x22,
    0x23, 0x26, 0x26, 0x28, 0x28, 0x28, 0x30, 0x30,
    0x2E, 0x2E, 0x38, 0x38, 0x3A, 0x45, 0x45, 0x53
#else
    // Non-scan ordered
    0x08, 0x10, 0x13, 0x16, 0x1a, 0x1b, 0x1d, 0x22,
    0x10, 0x10, 0x16, 0x18, 0x1b, 0x1d, 0x22, 0x25,
    0x13, 0x16, 0x1a, 0x1b, 0x1d, 0x22, 0x22, 0x26,
    0x16, 0x16, 0x1a, 0x1b, 0x1d, 0x22, 0x25, 0x28,
    0x16, 0x1a, 0x1B, 0x1d, 0x20, 0x23, 0x28, 0x30,
    0x1a, 0x1b, 0x1d, 0x20, 0x23, 0x28, 0x30, 0x3a,
    0x1a, 0x1b, 0x1d, 0x22, 0x26, 0x2e, 0x38, 0x45,
    0x1b, 0x1d, 0x23, 0x26, 0x2e, 0x38, 0x45, 0x53
#endif
};

static QuantiserMatrix_t DefaultNonIntraQuantizationMatrix =
{
    0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10,
    0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10,
    0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10,
    0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10,
    0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10,
    0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10,
    0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10,
    0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10
};

typedef struct Mpeg2CodecStreamParameterContext_s
{
    CodecBaseStreamParameterContext_t   BaseContext;

    MPEG2_SetGlobalParamSequence_t      StreamParameters;
} Mpeg2CodecStreamParameterContext_t;

#define BUFFER_MPEG2_CODEC_STREAM_PARAMETER_CONTEXT             "Mpeg2CodecStreamParameterContext"
#define BUFFER_MPEG2_CODEC_STREAM_PARAMETER_CONTEXT_TYPE        {BUFFER_MPEG2_CODEC_STREAM_PARAMETER_CONTEXT, BufferDataTypeBase, AllocateFromOSMemory, 32, 0, true, true, sizeof(Mpeg2CodecStreamParameterContext_t)}

static BufferDataDescriptor_t            Mpeg2CodecStreamParameterContextDescriptor = BUFFER_MPEG2_CODEC_STREAM_PARAMETER_CONTEXT_TYPE;

// --------

typedef struct Mpeg2CodecDecodeContext_s
{
    CodecBaseDecodeContext_t            BaseContext;

    MPEG2_TransformParam_t              DecodeParameters;
    MPEG2_CommandStatus_t               DecodeStatus;
} Mpeg2CodecDecodeContext_t;

#define BUFFER_MPEG2_CODEC_DECODE_CONTEXT       "Mpeg2CodecDecodeContext"
#define BUFFER_MPEG2_CODEC_DECODE_CONTEXT_TYPE  {BUFFER_MPEG2_CODEC_DECODE_CONTEXT, BufferDataTypeBase, AllocateFromOSMemory, 32, 0, true, true, sizeof(Mpeg2CodecDecodeContext_t)}

static BufferDataDescriptor_t            Mpeg2CodecDecodeContextDescriptor = BUFFER_MPEG2_CODEC_DECODE_CONTEXT_TYPE;


Codec_MmeVideoMpeg2_c::Codec_MmeVideoMpeg2_c(void)
    : Codec_MmeVideo_c()
    , Mpeg2TransformCapability()
    , Mpeg2InitializationParameters()
    , RasterOutput(false)
{
    Configuration.CodecName                             = "Mpeg2 video";
    Configuration.StreamParameterContextCount           = 1;
    Configuration.StreamParameterContextDescriptor      = &Mpeg2CodecStreamParameterContextDescriptor;
    Configuration.DecodeContextCount                    = 4;
    Configuration.DecodeContextDescriptor               = &Mpeg2CodecDecodeContextDescriptor;
    Configuration.MaxDecodeIndicesPerBuffer             = 2;
    Configuration.SliceDecodePermitted                  = false;
    Configuration.DecimatedDecodePermitted              = true;
    Configuration.TransformName[0]                      = MPEG2_MME_TRANSFORMER_NAME "0";
    Configuration.TransformName[1]                      = MPEG2_MME_TRANSFORMER_NAME "1";
    Configuration.AvailableTransformers                 = 2;
    Configuration.SizeOfTransformCapabilityStructure    = sizeof(MPEG2_TransformerCapability_t);
    Configuration.TransformCapabilityStructurePointer   = (void *)(&Mpeg2TransformCapability);
    //
    // The video firmware violates the MME spec. and passes data buffer addresses
    // as parametric information. For this reason it requires physical addresses
    // to be used.
    //
    Configuration.AddressingMode                        = PhysicalAddress;
    //
    // We do not need the coded data after decode is complete
    //
    Configuration.ShrinkCodedDataBuffersAfterDecode     = true;
    //
    // My trick mode parameters
    //
    Configuration.TrickModeParameters.EmpiricalMaximumDecodeFrameRateShortIntegration   = 60;
    Configuration.TrickModeParameters.EmpiricalMaximumDecodeFrameRateLongIntegration    = 60;
    Configuration.TrickModeParameters.DecodeFrameRateShortIntegrationForIOnly       = 60;
    Configuration.TrickModeParameters.DecodeFrameRateLongIntegrationForIOnly        = 60;
    Configuration.TrickModeParameters.SubstandardDecodeSupported        = false;
    Configuration.TrickModeParameters.SubstandardDecodeRateIncrease = 1;
    Configuration.TrickModeParameters.DefaultGroupSize                  = 12;
    Configuration.TrickModeParameters.DefaultGroupReferenceFrameCount   = 4;
}

Codec_MmeVideoMpeg2_c::~Codec_MmeVideoMpeg2_c(void)
{
    Halt();
}

// /////////////////////////////////////////////////////////////////////////
//
//      Function to deal with the returned capabilities
//      structure for an mpeg2 mme transformer.
//

CodecStatus_t   Codec_MmeVideoMpeg2_c::HandleCapabilities(void)
{
    // Default to using Omega2 unless Capabilities tell us otherwise
    BufferFormat_t  DisplayFormat = FormatVideo420_MacroBlock;
    // Default elements to produce in the buffers
    DecodeBufferComponentElementMask_t Elements = PrimaryManifestationElement | DecimatedManifestationElement;
    RasterOutput = false;
    SE_INFO(group_decoder_video, "MME Transformer '%s' capabilities are :-\n", MPEG2_MME_TRANSFORMER_NAME);
#if (MPEG2_MME_VERSION >= 46)

    switch (Mpeg2TransformCapability.DisplayBufferFormat)
    {
    case MPEG2_OMEGA2:
        SE_DEBUG(group_decoder_video, "  Omega2 Display Enabled\n");
        RasterOutput = false;
        break;

    case MPEG2_RASTER:
        SE_DEBUG(group_decoder_video, "  Raster Display Enabled\n");
        RasterOutput = true;
        break;

    case MPEG2_OMEGA3:
    default:
        SE_ERROR(" Unsupported Display Buffer format %d\n", Mpeg2TransformCapability.DisplayBufferFormat);
    }

    switch (Mpeg2TransformCapability.MaxDecodeResolution)
    {
    case MPEG2_VGA:
        SE_INFO(group_decoder_video, "  VGA Capable\n");
        break;

    case MPEG2_SD:
        SE_INFO(group_decoder_video, "  SD Capable\n");
        break;

    case MPEG2_720p:
        SE_INFO(group_decoder_video, "  720p Capable\n");
        break;

    case MPEG2_HD_1080i:
        SE_INFO(group_decoder_video, "  1080i Capable\n");
        break;

    case MPEG2_HD_1080p:
        SE_INFO(group_decoder_video, "  1080p Capable\n");
        break;

    default:
        break;
    }

    if (SE_IS_VERBOSE_ON(group_decoder_video))
    {
        SE_VERBOSE(group_decoder_video, "  IsDeblockingSupported             = %s\n", Mpeg2TransformCapability.IsDeblockingSupported ? "Yes" : "No");
        SE_VERBOSE(group_decoder_video, "  IsDeringingSupported              = %s\n", Mpeg2TransformCapability.IsDeringingSupported ? "Yes" : "No");
    }

#else

    if (SE_IS_VERBOSE_ON(group_decoder_video))
    {
        SE_VERBOSE(group_decoder_video, "  StructSize                        = %d bytes\n", Mpeg2TransformCapability.StructSize);
        SE_VERBOSE(group_decoder_video, "  MPEG1Capability                   = %d\n", Mpeg2TransformCapability.MPEG1Capability);
        SE_VERBOSE(group_decoder_video, "  SupportedFrameChromaFormat        = %d\n", Mpeg2TransformCapability.SupportedFrameChromaFormat);
        SE_VERBOSE(group_decoder_video, "  SupportedFieldChromaFormat        = %d\n", Mpeg2TransformCapability.SupportedFieldChromaFormat);
        SE_VERBOSE(group_decoder_video, "  MaximumFieldDecodingLatency90KHz  = %d\n", Mpeg2TransformCapability.MaximumFieldDecodingLatency90KHz);
    }

#if (MPEG2_MME_VERSION >= 44)
    {
        DeltaTop_TransformerCapability_t *DeltaTopCapabilities = (DeltaTop_TransformerCapability_t *) Configuration.DeltaTopCapabilityStructurePointer;

        if ((DeltaTopCapabilities != NULL) && (DeltaTopCapabilities->DisplayBufferFormat == DELTA_OUTPUT_RASTER))
        {
            SE_DEBUG(group_decoder_video, "  Raster Output Enabled\n");
            RasterOutput = true;
        }
    }
#endif
#endif

    if (RasterOutput)
    {
        DisplayFormat = FormatVideo420_Raster2B;
        Elements |= VideoDecodeCopyElement;
    }

    Stream->GetDecodeBufferManager()->FillOutDefaultList(DisplayFormat,  Elements,
                                                         Configuration.ListOfDecodeBufferComponents);
    return CodecNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Function to deal with the returned capabilities
//      structure for an mpeg2 mme transformer.
//

CodecStatus_t   Codec_MmeVideoMpeg2_c::FillOutTransformerInitializationParameters(void)
{
    //
    // Fill out the command parameters
    //
    Mpeg2InitializationParameters.InputBufferBegin      = 0x00000000;
    Mpeg2InitializationParameters.InputBufferEnd        = 0xffffffff;
    //
    // Fill out the actual command
    //
    MMEInitializationParameters.TransformerInitParamsSize       = sizeof(MPEG2_InitTransformerParam_t);
    MMEInitializationParameters.TransformerInitParams_p         = (MME_GenericParams_t)(&Mpeg2InitializationParameters);

    return CodecNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Function to fill out the stream parameters
//      structure for an mpeg2 mme transformer.
//

CodecStatus_t   Codec_MmeVideoMpeg2_c::FillOutSetStreamParametersCommand(void)
{
    Mpeg2StreamParameters_t                 *Parsed         = (Mpeg2StreamParameters_t *)ParsedFrameParameters->StreamParameterStructure;
    Mpeg2CodecStreamParameterContext_t      *Context        = (Mpeg2CodecStreamParameterContext_t *)StreamParameterContext;
    unsigned char                           *intra_quantizer_matrix;
    unsigned char                           *non_intra_quantizer_matrix;
    unsigned char                           *chroma_intra_quantizer_matrix;
    unsigned char                           *chroma_non_intra_quantizer_matrix;
    //
    // Fill out the command parameters
    //
    Context->StreamParameters.StructSize                = sizeof(MPEG2_SetGlobalParamSequence_t);
    Context->StreamParameters.MPEGStreamTypeFlag        = (Parsed->StreamType == MpegStreamTypeMpeg1) ? 0 : 1;
    Context->StreamParameters.horizontal_size           = Parsed->SequenceHeader.horizontal_size_value;
    Context->StreamParameters.vertical_size             = Parsed->SequenceHeader.vertical_size_value;
    Context->StreamParameters.progressive_sequence      = true;
    Context->StreamParameters.chroma_format             = MPEG2_CHROMA_4_2_0;

    if (Parsed->SequenceExtensionHeaderPresent)
    {
        Context->StreamParameters.horizontal_size       |= (Parsed->SequenceExtensionHeader.horizontal_size_extension << 12);
        Context->StreamParameters.vertical_size         |= (Parsed->SequenceExtensionHeader.vertical_size_extension << 12);
        Context->StreamParameters.progressive_sequence   = Parsed->SequenceExtensionHeader.progressive_sequence;
        Context->StreamParameters.chroma_format          = (MPEG2_ChromaFormat_t)Parsed->SequenceExtensionHeader.chroma_format;
    }

    Context->StreamParameters.MatrixFlags                = (Parsed->SequenceHeader.load_intra_quantizer_matrix ? MPEG2_LOAD_INTRA_QUANTIZER_MATRIX_FLAG : 0) |
                                                           (Parsed->SequenceHeader.load_non_intra_quantizer_matrix ? MPEG2_LOAD_NON_INTRA_QUANTIZER_MATRIX_FLAG : 0);
//
    intra_quantizer_matrix                               = Parsed->CumulativeQuantMatrices.load_intra_quantizer_matrix ?
                                                           Parsed->CumulativeQuantMatrices.intra_quantizer_matrix :
                                                           DefaultIntraQuantizationMatrix;
    non_intra_quantizer_matrix                           = Parsed->CumulativeQuantMatrices.load_non_intra_quantizer_matrix ?
                                                           Parsed->CumulativeQuantMatrices.non_intra_quantizer_matrix :
                                                           DefaultNonIntraQuantizationMatrix;
    chroma_intra_quantizer_matrix                        = Parsed->CumulativeQuantMatrices.load_chroma_intra_quantizer_matrix ?
                                                           Parsed->CumulativeQuantMatrices.chroma_intra_quantizer_matrix :
                                                           intra_quantizer_matrix;
    chroma_non_intra_quantizer_matrix                    = Parsed->CumulativeQuantMatrices.load_chroma_non_intra_quantizer_matrix ?
                                                           Parsed->CumulativeQuantMatrices.chroma_non_intra_quantizer_matrix :
                                                           non_intra_quantizer_matrix;
    memcpy(Context->StreamParameters.intra_quantiser_matrix, intra_quantizer_matrix, sizeof(QuantiserMatrix_t));
    memcpy(Context->StreamParameters.non_intra_quantiser_matrix, non_intra_quantizer_matrix, sizeof(QuantiserMatrix_t));
    memcpy(Context->StreamParameters.chroma_intra_quantiser_matrix, chroma_intra_quantizer_matrix, sizeof(QuantiserMatrix_t));
    memcpy(Context->StreamParameters.chroma_non_intra_quantiser_matrix, chroma_non_intra_quantizer_matrix, sizeof(QuantiserMatrix_t));
    //
    // Fill out the actual command
    //
    memset(&Context->BaseContext.MMECommand, 0, sizeof(MME_Command_t));
    Context->BaseContext.MMECommand.CmdStatus.AdditionalInfoSize        = 0;
    Context->BaseContext.MMECommand.CmdStatus.AdditionalInfo_p          = NULL;
    Context->BaseContext.MMECommand.ParamSize                           = sizeof(MPEG2_SetGlobalParamSequence_t);
    Context->BaseContext.MMECommand.Param_p                             = (MME_GenericParams_t)(&Context->StreamParameters);

    return CodecNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Function to fill out the decode parameters
//      structure for an mpeg2 mme transformer.
//

CodecStatus_t   Codec_MmeVideoMpeg2_c::FillOutDecodeCommand(void)
{
    Mpeg2CodecDecodeContext_t       *Context        = (Mpeg2CodecDecodeContext_t *)DecodeContext;
    Mpeg2FrameParameters_t          *Parsed         = (Mpeg2FrameParameters_t *)ParsedFrameParameters->FrameParameterStructure;
    MPEG2_TransformParam_t          *Param;
#if (MPEG2_MME_VERSION >= 44)
    MPEG2_DisplayBufferAddress_t    *Display;
#endif
    MPEG2_DecodedBufferAddress_t    *Decode;
    MPEG2_ParamPicture_t            *Picture;
    MPEG2_RefPicListAddress_t       *RefList;
    unsigned int                     Entry;
    Buffer_t             DecodeBuffer;
    Buffer_t             ReferenceBuffer;
    DecodeBufferComponentType_t  DisplayComponent;
    DecodeBufferComponentType_t  DecimatedComponent;
    DecodeBufferComponentType_t  DecodeComponent;
    //
    // For mpeg2 we do not do slice decodes.
    //
    KnownLastSliceInFieldFrame      = true;
    OS_LockMutex(&Lock);
    DecodeBuffer            = BufferState[CurrentDecodeBufferIndex].Buffer;
    DisplayComponent            = PrimaryManifestationComponent;
    DecimatedComponent          = DecimatedManifestationComponent;
    DecodeComponent         = RasterOutput ? VideoDecodeCopy : PrimaryManifestationComponent;
    //
    // Fill out the straight forward command parameters
    //
    memset(&Context->DecodeParameters, 0, sizeof(MPEG2_TransformParam_t));
    memset(&Context->DecodeStatus, 0xa5, sizeof(MPEG2_CommandStatus_t));
    Param                                       = &Context->DecodeParameters;
    Decode                                      = &Param->DecodedBufferAddress;
    Picture                                     = &Param->PictureParameters;
    RefList                                     = &Param->RefPicListAddress;
    Param->StructSize                           = sizeof(MPEG2_TransformParam_t);
    Param->PictureStartAddrCompressedBuffer_p   = (MPEG2_CompressedData_t)CodedData;
    Param->PictureStopAddrCompressedBuffer_p    = (MPEG2_CompressedData_t)(CodedData + CodedDataLength);
    Decode->StructSize                          = sizeof(MPEG2_DecodedBufferAddress_t);
    Decode->DecodedLuma_p                       = (MPEG2_LumaAddress_t)Stream->GetDecodeBufferManager()->Luma(DecodeBuffer, DecodeComponent);
    Decode->DecodedChroma_p                     = (MPEG2_ChromaAddress_t)Stream->GetDecodeBufferManager()->Chroma(DecodeBuffer, DecodeComponent);
    Decode->DecodedTemporalReferenceValue       = Parsed->PictureHeader.temporal_reference;
    Decode->MBDescr_p                           = NULL;
#if (MPEG2_MME_VERSION >= 44)
    Display                                     = &Param->DisplayBufferAddress;
    Display->StructSize                         = sizeof(MPEG2_DisplayBufferAddress_t);
    Display->DisplayLuma_p                      = (MPEG2_LumaAddress_t)Stream->GetDecodeBufferManager()->Luma(DecodeBuffer, DisplayComponent);
    Display->DisplayChroma_p                    = (MPEG2_ChromaAddress_t)Stream->GetDecodeBufferManager()->Chroma(DecodeBuffer, DisplayComponent);

    if (Stream->GetDecodeBufferManager()->ComponentPresent(DecodeBuffer, DecimatedComponent))
    {
        Display->DisplayDecimatedLuma_p             = (MPEG2_LumaAddress_t)Stream->GetDecodeBufferManager()->Luma(DecodeBuffer, DecimatedComponent);
        Display->DisplayDecimatedChroma_p           = (MPEG2_ChromaAddress_t)Stream->GetDecodeBufferManager()->Chroma(DecodeBuffer, DecimatedComponent);
    }
    else
    {
        Display->DisplayDecimatedLuma_p             = NULL;
        Display->DisplayDecimatedChroma_p           = NULL;
    }

#else

    if (Stream->GetDecodeBufferManager()->ComponentPresent(DecodeBuffer, DecimatedComponent))
    {
        Decode->DecimatedLuma_p                     = (MPEG2_LumaAddress_t)Stream->GetDecodeBufferManager()->Luma(DecodeBuffer, DecimatedComponent);
        Decode->DecimatedChroma_p                   = (MPEG2_ChromaAddress_t)Stream->GetDecodeBufferManager()->Chroma(DecodeBuffer, DecimatedComponent);
    }
    else
    {
        Decode->DecimatedLuma_p                     = NULL;
        Decode->DecimatedChroma_p                   = NULL;
    }

#endif

    //
    // Here we only support decimation by 2 in the vertical,
    // if the decimated buffer is present, then we decimate 2 vertical
    //

    if (!Stream->GetDecodeBufferManager()->ComponentPresent(DecodeBuffer, DecimatedComponent))
    {
        // Normal Case
#if (MPEG2_MME_VERSION >= 44)
        if (RasterOutput && ParsedFrameParameters->ReferenceFrame)
        {
            Param->MainAuxEnable = MPEG2_REF_MAIN_DISP_MAIN_EN;
        }
        else
        {
            Param->MainAuxEnable = MPEG2_DISP_MAIN_EN;
        }
#else
        Param->MainAuxEnable                        = MPEG2_MAINOUT_EN;
#endif
        Param->HorizontalDecimationFactor           = MPEG2_HDEC_1;
        Param->VerticalDecimationFactor             = MPEG2_VDEC_1;
    }
    else
    {
#if (MPEG2_MME_VERSION >= 44)
        if (RasterOutput && ParsedFrameParameters->ReferenceFrame)
        {
            Param->MainAuxEnable = MPEG2_REF_MAIN_DISP_MAIN_AUX_EN;
        }
        else
        {
            Param->MainAuxEnable = MPEG2_DISP_AUX_MAIN_EN;
        }
#else
        Param->MainAuxEnable                        = MPEG2_AUX_MAIN_OUT_EN;
#endif
        Param->HorizontalDecimationFactor           = (Stream->GetDecodeBufferManager()->DecimationFactor(DecodeBuffer, 0) == 2) ?
                                                      MPEG2_HDEC_ADVANCED_2 :
                                                      MPEG2_HDEC_ADVANCED_4;

        if (Parsed->PictureCodingExtensionHeader.progressive_frame)
        {
            Param->VerticalDecimationFactor             = MPEG2_VDEC_ADVANCED_2_PROG;
        }
        else
        {
            Param->VerticalDecimationFactor             = MPEG2_VDEC_ADVANCED_2_INT;
        }
    }

    Param->DecodingMode                         = MPEG2_NORMAL_DECODE;
//    Param->DecodingMode                               = MPEG2_NORMAL_DECODE_WITHOUT_ERROR_RECOVERY;
    Param->AdditionalFlags                      = MPEG2_ADDITIONAL_FLAG_NONE;
    Picture->StructSize                         = sizeof(MPEG2_ParamPicture_t);
    Picture->picture_coding_type                = (MPEG2_PictureCodingType_t)Parsed->PictureHeader.picture_coding_type;

    if (Parsed->PictureCodingExtensionHeaderPresent)
    {
        Picture->forward_horizontal_f_code      = Parsed->PictureCodingExtensionHeader.f_code[0][0];
        Picture->forward_vertical_f_code        = Parsed->PictureCodingExtensionHeader.f_code[0][1];
        Picture->backward_horizontal_f_code     = Parsed->PictureCodingExtensionHeader.f_code[1][0];
        Picture->backward_vertical_f_code       = Parsed->PictureCodingExtensionHeader.f_code[1][1];
        Picture->intra_dc_precision             = (MPEG2_IntraDCPrecision_t)Parsed->PictureCodingExtensionHeader.intra_dc_precision;
        Picture->picture_structure              = (MPEG2_PictureStructure_t)Parsed->PictureCodingExtensionHeader.picture_structure;
        Picture->mpeg_decoding_flags            = (Parsed->PictureCodingExtensionHeader.top_field_first                 << 0) |
                                                  (Parsed->PictureCodingExtensionHeader.frame_pred_frame_dct            << 1) |
                                                  (Parsed->PictureCodingExtensionHeader.concealment_motion_vectors      << 2) |
                                                  (Parsed->PictureCodingExtensionHeader.q_scale_type                    << 3) |
                                                  (Parsed->PictureCodingExtensionHeader.intra_vlc_format                << 4) |
                                                  (Parsed->PictureCodingExtensionHeader.alternate_scan                  << 5) |
                                                  (Parsed->PictureCodingExtensionHeader.progressive_frame               << 6);

        if (Picture->picture_structure == MPEG2_TOP_FIELD_TYPE)
            Param->AdditionalFlags              = (BufferState[CurrentDecodeBufferIndex].ParsedVideoParameters->TopFieldFirst) ?
                                                  MPEG2_ADDITIONAL_FLAG_FIRST_FIELD : MPEG2_ADDITIONAL_FLAG_SECOND_FIELD;
        else if (Picture->picture_structure == MPEG2_BOTTOM_FIELD_TYPE)
            Param->AdditionalFlags              = (BufferState[CurrentDecodeBufferIndex].ParsedVideoParameters->TopFieldFirst) ?
                                                  MPEG2_ADDITIONAL_FLAG_SECOND_FIELD : MPEG2_ADDITIONAL_FLAG_FIRST_FIELD;
    }
    else
    {
        Picture->forward_horizontal_f_code      = Parsed->PictureHeader.full_pel_forward_vector;
        Picture->forward_vertical_f_code        = Parsed->PictureHeader.forward_f_code;
        Picture->backward_horizontal_f_code     = Parsed->PictureHeader.full_pel_backward_vector;
        Picture->backward_vertical_f_code       = Parsed->PictureHeader.backward_f_code;
        Picture->intra_dc_precision             = MPEG2_INTRA_DC_PRECISION_8_BITS;
        Picture->picture_structure              = MPEG2_FRAME_TYPE;
        Picture->mpeg_decoding_flags            = MPEG_DECODING_FLAGS_TOP_FIELD_FIRST |
                                                  MPEG_DECODING_FLAGS_PROGRESSIVE_FRAME;
#if defined(CONFIG_STM_VIRTUAL_PLATFORM) /* VSOC WORKAROUND : This patch avoid delta model exits
                                            (MPEG1 : WARNING : DC coefficient out of range : 2992 in block 1 - Aborting) */
        /*  Frame_pred_frame_dct must be forced to 1 for mpeg1  */
        if (((Mpeg2StreamParameters_t *)ParsedFrameParameters->StreamParameterStructure)->StreamType == MpegStreamTypeMpeg1)
        {
            Picture->mpeg_decoding_flags       |= MPEG_DECODING_FLAGS_FRAME_PRED_FRAME_DCT;
        }
#endif
    }

    //
    // Fill out the reference frame list stuff
    //
    RefList->StructSize                         = sizeof(MPEG2_RefPicListAddress_t);

    if (ParsedFrameParameters->NumberOfReferenceFrameLists != 0)
    {
        if (DecodeContext->ReferenceFrameList[0].EntryCount > 0)
        {
            Entry       = DecodeContext->ReferenceFrameList[0].EntryIndicies[0];
            ReferenceBuffer             = BufferState[Entry].Buffer;
            RefList->ForwardReferenceLuma_p             = (MPEG2_LumaAddress_t)Stream->GetDecodeBufferManager()->Luma(ReferenceBuffer, DecodeComponent);
            RefList->ForwardReferenceChroma_p           = (MPEG2_ChromaAddress_t)Stream->GetDecodeBufferManager()->Chroma(ReferenceBuffer, DecodeComponent);
            RefList->ForwardTemporalReferenceValue      = Decode->DecodedTemporalReferenceValue - 1;
        }

        if (DecodeContext->ReferenceFrameList[0].EntryCount > 1)
        {
            Entry       = DecodeContext->ReferenceFrameList[0].EntryIndicies[1];
            ReferenceBuffer             = BufferState[Entry].Buffer;
            RefList->BackwardReferenceLuma_p            = (MPEG2_LumaAddress_t)Stream->GetDecodeBufferManager()->Luma(ReferenceBuffer, DecodeComponent);
            RefList->BackwardReferenceChroma_p          = (MPEG2_ChromaAddress_t)Stream->GetDecodeBufferManager()->Chroma(ReferenceBuffer, DecodeComponent);
            RefList->BackwardTemporalReferenceValue     = Decode->DecodedTemporalReferenceValue + 1;
        }
    }

    //
    // Fill out the actual command
    //
    memset(&Context->BaseContext.MMECommand, 0, sizeof(MME_Command_t));
    Context->BaseContext.MMECommand.CmdStatus.AdditionalInfoSize        = sizeof(MPEG2_CommandStatus_t);
    Context->BaseContext.MMECommand.CmdStatus.AdditionalInfo_p          = (MME_GenericParams_t)(&Context->DecodeStatus);
    Context->BaseContext.MMECommand.ParamSize                           = sizeof(MPEG2_TransformParam_t);
    Context->BaseContext.MMECommand.Param_p                             = (MME_GenericParams_t)(&Context->DecodeParameters);

    OS_UnLockMutex(&Lock);

    return CodecNoError;
}

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
CodecStatus_t   Codec_MmeVideoMpeg2_c::ValidateDecodeContext(CodecBaseDecodeContext_t *Context)
{
    return CodecNoError;
}

// /////////////////////////////////////////////////////////////////////////
//
//      Function to dump out the set stream
//      parameters from an mme command.
//

CodecStatus_t   Codec_MmeVideoMpeg2_c::DumpSetStreamParameters(void    *Parameters)
{
    MPEG2_SetGlobalParamSequence_t  *StreamParams;
//
    StreamParams    = (MPEG2_SetGlobalParamSequence_t *)Parameters;
    SE_VERBOSE(group_decoder_video, "AZA - STREAM PARAMS %p\n", StreamParams);
    SE_VERBOSE(group_decoder_video, "AZA -       StructSize                           = %d\n", StreamParams->StructSize);
    SE_VERBOSE(group_decoder_video, "AZA -       MPEGStreamTypeFlag                   = %d\n", StreamParams->MPEGStreamTypeFlag);
    SE_VERBOSE(group_decoder_video, "AZA -       horizontal_size                      = %d\n", StreamParams->horizontal_size);
    SE_VERBOSE(group_decoder_video, "AZA -       vertical_size                        = %d\n", StreamParams->vertical_size);
    SE_VERBOSE(group_decoder_video, "AZA -       progressive_sequence                 = %d\n", StreamParams->progressive_sequence);
    SE_VERBOSE(group_decoder_video, "AZA -       chroma_format                        = %d\n", StreamParams->chroma_format);
    SE_VERBOSE(group_decoder_video, "AZA -       MatrixFlags                          = %d\n", StreamParams->MatrixFlags);
    SE_VERBOSE(group_decoder_video, "AZA -       intra_quantiser_matrix               = %02x %02x %02x %02x %02x %02x %02x %02x\n",
               StreamParams->intra_quantiser_matrix[0], StreamParams->intra_quantiser_matrix[1], StreamParams->intra_quantiser_matrix[2], StreamParams->intra_quantiser_matrix[3],
               StreamParams->intra_quantiser_matrix[4], StreamParams->intra_quantiser_matrix[5], StreamParams->intra_quantiser_matrix[6], StreamParams->intra_quantiser_matrix[7]);
    SE_VERBOSE(group_decoder_video, "AZA -       non_intra_quantiser_matrix           = %02x %02x %02x %02x %02x %02x %02x %02x\n",
               StreamParams->non_intra_quantiser_matrix[0], StreamParams->non_intra_quantiser_matrix[1], StreamParams->non_intra_quantiser_matrix[2], StreamParams->non_intra_quantiser_matrix[3],
               StreamParams->non_intra_quantiser_matrix[4], StreamParams->non_intra_quantiser_matrix[5], StreamParams->non_intra_quantiser_matrix[6], StreamParams->non_intra_quantiser_matrix[7]);
    SE_VERBOSE(group_decoder_video, "AZA -       chroma_intra_quantiser_matrix        = %02x %02x %02x %02x %02x %02x %02x %02x\n",
               StreamParams->chroma_intra_quantiser_matrix[0], StreamParams->chroma_intra_quantiser_matrix[1], StreamParams->chroma_intra_quantiser_matrix[2], StreamParams->chroma_intra_quantiser_matrix[3],
               StreamParams->chroma_intra_quantiser_matrix[4], StreamParams->chroma_intra_quantiser_matrix[5], StreamParams->chroma_intra_quantiser_matrix[6], StreamParams->chroma_intra_quantiser_matrix[7]);
    SE_VERBOSE(group_decoder_video, "AZA -       chroma_non_intra_quantiser_matrix    = %02x %02x %02x %02x %02x %02x %02x %02x\n",
               StreamParams->chroma_non_intra_quantiser_matrix[0], StreamParams->chroma_non_intra_quantiser_matrix[1], StreamParams->chroma_non_intra_quantiser_matrix[2],
               StreamParams->chroma_non_intra_quantiser_matrix[3],
               StreamParams->chroma_non_intra_quantiser_matrix[4], StreamParams->chroma_non_intra_quantiser_matrix[5], StreamParams->chroma_non_intra_quantiser_matrix[6],
               StreamParams->chroma_non_intra_quantiser_matrix[7]);
    return CodecNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Function to dump out the decode
//      parameters from an mme command.
//

CodecStatus_t   Codec_MmeVideoMpeg2_c::DumpDecodeParameters(void    *Parameters)
{
    MPEG2_TransformParam_t *FrameParams     = (MPEG2_TransformParam_t *)Parameters;
    SE_VERBOSE(group_decoder_video, "AZA - FRAME PARAMS %p\n", FrameParams);
    SE_VERBOSE(group_decoder_video, "AZA -       StructSize                           = %d\n", FrameParams->StructSize);
    SE_VERBOSE(group_decoder_video, "AZA -       PictureStartAddrCompressedBuffer_p   = %p\n", FrameParams->PictureStartAddrCompressedBuffer_p);
    SE_VERBOSE(group_decoder_video, "AZA -       PictureStopAddrCompressedBuffer_p    = %p\n", FrameParams->PictureStopAddrCompressedBuffer_p);
    SE_VERBOSE(group_decoder_video, "AZA -       DecodedBufferAddress\n");
    SE_VERBOSE(group_decoder_video, "AZA -           StructSize                       = %d\n", FrameParams->DecodedBufferAddress.StructSize);
    SE_VERBOSE(group_decoder_video, "AZA -           DecodedLuma_p                    = %p\n", FrameParams->DecodedBufferAddress.DecodedLuma_p);
    SE_VERBOSE(group_decoder_video, "AZA -           DecodedChroma_p                  = %p\n", FrameParams->DecodedBufferAddress.DecodedChroma_p);
    SE_VERBOSE(group_decoder_video, "AZA -           DecodedTemporalReferenceValue    = %d\n",  FrameParams->DecodedBufferAddress.DecodedTemporalReferenceValue);
#if (MPEG2_MME_VERSION < 44)
    SE_VERBOSE(group_decoder_video, "AZA -           DecimatedLuma_p                  = %p\n", FrameParams->DecodedBufferAddress.DecimatedLuma_p);
    SE_VERBOSE(group_decoder_video, "AZA -           DecimatedChroma_p                = %p\n", FrameParams->DecodedBufferAddress.DecimatedChroma_p);
#else
    SE_VERBOSE(group_decoder_video, "AZA -           DisplayLuma_p                    = %p\n", FrameParams->DisplayBufferAddress.DisplayLuma_p);
    SE_VERBOSE(group_decoder_video, "AZA -           DisplayChroma_p                  = %p\n", FrameParams->DisplayBufferAddress.DisplayChroma_p);
    SE_VERBOSE(group_decoder_video, "AZA -           DisplayDecimatedLuma_p           = %p\n", FrameParams->DisplayBufferAddress.DisplayDecimatedLuma_p);
    SE_VERBOSE(group_decoder_video, "AZA -           DisplayDecimatedChroma_p         = %p\n", FrameParams->DisplayBufferAddress.DisplayDecimatedChroma_p);
#endif
    SE_VERBOSE(group_decoder_video, "AZA -           MBDescr_p                        = %p\n", FrameParams->DecodedBufferAddress.MBDescr_p);
    SE_VERBOSE(group_decoder_video, "AZA -       RefPicListAddress\n");
    SE_VERBOSE(group_decoder_video, "AZA -           StructSize                       = %d\n", FrameParams->RefPicListAddress.StructSize);
    SE_VERBOSE(group_decoder_video, "AZA -           BackwardReferenceLuma_p          = %p\n", FrameParams->RefPicListAddress.BackwardReferenceLuma_p);
    SE_VERBOSE(group_decoder_video, "AZA -           BackwardReferenceChroma_p        = %p\n", FrameParams->RefPicListAddress.BackwardReferenceChroma_p);
    SE_VERBOSE(group_decoder_video, "AZA -           BackwardTemporalReferenceValue   = %d\n", FrameParams->RefPicListAddress.BackwardTemporalReferenceValue);
    SE_VERBOSE(group_decoder_video, "AZA -           ForwardReferenceLuma_p           = %p\n", FrameParams->RefPicListAddress.ForwardReferenceLuma_p);
    SE_VERBOSE(group_decoder_video, "AZA -           ForwardReferenceChroma_p         = %p\n", FrameParams->RefPicListAddress.ForwardReferenceChroma_p);
    SE_VERBOSE(group_decoder_video, "AZA -           ForwardTemporalReferenceValue    = %d\n", FrameParams->RefPicListAddress.ForwardTemporalReferenceValue);
    SE_VERBOSE(group_decoder_video, "AZA -       MainAuxEnable                        = %08x\n", FrameParams->MainAuxEnable);
    SE_VERBOSE(group_decoder_video, "AZA -       HorizontalDecimationFactor           = %08x\n", FrameParams->HorizontalDecimationFactor);
    SE_VERBOSE(group_decoder_video, "AZA -       VerticalDecimationFactor             = %08x\n", FrameParams->VerticalDecimationFactor);
    SE_VERBOSE(group_decoder_video, "AZA -       DecodingMode                         = %d\n", FrameParams->DecodingMode);
    SE_VERBOSE(group_decoder_video, "AZA -       AdditionalFlags                      = %08x\n", FrameParams->AdditionalFlags);
    SE_VERBOSE(group_decoder_video, "AZA -       PictureParameters\n");
    SE_VERBOSE(group_decoder_video, "AZA -           StructSize                       = %d\n", FrameParams->PictureParameters.StructSize);
    SE_VERBOSE(group_decoder_video, "AZA -           picture_coding_type              = %d\n", FrameParams->PictureParameters.picture_coding_type);
    SE_VERBOSE(group_decoder_video, "AZA -           forward_horizontal_f_code        = %d\n", FrameParams->PictureParameters.forward_horizontal_f_code);
    SE_VERBOSE(group_decoder_video, "AZA -           forward_vertical_f_code          = %d\n", FrameParams->PictureParameters.forward_vertical_f_code);
    SE_VERBOSE(group_decoder_video, "AZA -           backward_horizontal_f_code       = %d\n", FrameParams->PictureParameters.backward_horizontal_f_code);
    SE_VERBOSE(group_decoder_video, "AZA -           backward_vertical_f_code         = %d\n", FrameParams->PictureParameters.backward_vertical_f_code);
    SE_VERBOSE(group_decoder_video, "AZA -           intra_dc_precision               = %d\n", FrameParams->PictureParameters.intra_dc_precision);
    SE_VERBOSE(group_decoder_video, "AZA -           picture_structure                = %d\n", FrameParams->PictureParameters.picture_structure);
    SE_VERBOSE(group_decoder_video, "AZA -           mpeg_decoding_flags              = %08x\n", FrameParams->PictureParameters.mpeg_decoding_flags);
    return CodecNoError;
}


// Convert the return code into human readable form.
static const char *LookupError(unsigned int Error)
{
#define E(e) case e: return #e

    switch (Error)
    {
        E(MPEG2_DECODER_ERROR_MB_OVERFLOW);
        E(MPEG2_DECODER_ERROR_RECOVERED);
        E(MPEG2_DECODER_ERROR_NOT_RECOVERED);
        E(MPEG2_DECODER_ERROR_TASK_TIMEOUT);

    default: return "MPEG2_DECODER_UNKNOWN_ERROR";
    }

#undef E
}
CodecStatus_t   Codec_MmeVideoMpeg2_c::CheckCodecReturnParameters(CodecBaseDecodeContext_t *Context)
{
    unsigned int                 i, j;
    unsigned int                 ErroneousMacroBlocks;
    unsigned int                 TotalMacroBlocks;
    Buffer_t                     DecodeBuffer;
    DecodeBufferComponentType_t  DecodeComponent;
    Mpeg2CodecDecodeContext_t   *Mpeg2Context     = (Mpeg2CodecDecodeContext_t *)Context;
    MME_Command_t               *MMECommand       = (MME_Command_t *)(&Context->MMECommand);
    MME_CommandStatus_t         *CmdStatus        = (MME_CommandStatus_t *)(&MMECommand->CmdStatus);
    MPEG2_CommandStatus_t       *AdditionalInfo_p = (MPEG2_CommandStatus_t *)CmdStatus->AdditionalInfo_p;

    if (AdditionalInfo_p != NULL)
    {
        if (AdditionalInfo_p->ErrorCode != MPEG2_DECODER_NO_ERROR)
        {
            SE_INFO(group_decoder_video, "%s  %x\n", LookupError(AdditionalInfo_p->ErrorCode), AdditionalInfo_p->ErrorCode);
            //
            // Calculate decode quality from error status fields
            //
            OS_LockMutex(&Lock);
            DecodeBuffer        = BufferState[Context->BufferIndex].Buffer;
            DecodeComponent     = RasterOutput ? VideoDecodeCopy : PrimaryManifestationComponent;
            TotalMacroBlocks        = (Stream->GetDecodeBufferManager()->ComponentDimension(DecodeBuffer, DecodeComponent, 0) *
                                       Stream->GetDecodeBufferManager()->ComponentDimension(DecodeBuffer, DecodeComponent, 1)) / 256;
            OS_UnLockMutex(&Lock);

            if (Mpeg2Context->DecodeParameters.PictureParameters.picture_structure != MPEG2_FRAME_TYPE)
            {
                TotalMacroBlocks  /= 2;
            }

            ErroneousMacroBlocks    = 0;

            for (i = 0; i < MPEG2_STATUS_PARTITION; i++)
                for (j = 0; j < MPEG2_STATUS_PARTITION; j++)
                {
                    ErroneousMacroBlocks  += Mpeg2Context->DecodeStatus.Status[i][j];
                }

            Context->DecodeQuality  = (ErroneousMacroBlocks < TotalMacroBlocks) ?
                                      Rational_t((TotalMacroBlocks - ErroneousMacroBlocks), TotalMacroBlocks) : 0;
#if 0
            SE_DEBUG(group_decoder_video, "AZAAZA - %4d %4d - %d.%09d\n", TotalMacroBlocks, ErroneousMacroBlocks,
                     Context->DecodeQuality.IntegerPart(),  Context->DecodeQuality.RemainderDecimal(9));

            for (unsigned int i = 0; i < MPEG2_STATUS_PARTITION; i++)
                SE_DEBUG(group_decoder_video, "  %02x %02x %02x %02x %02x %02x\n",
                         Mpeg2Context->DecodeStatus.Status[0][i], Mpeg2Context->DecodeStatus.Status[1][i],
                         Mpeg2Context->DecodeStatus.Status[2][i], Mpeg2Context->DecodeStatus.Status[3][i],
                         Mpeg2Context->DecodeStatus.Status[4][i], Mpeg2Context->DecodeStatus.Status[5][i]);

#endif

            switch (AdditionalInfo_p->ErrorCode)
            {
            case MPEG2_DECODER_ERROR_MB_OVERFLOW:
                Stream->Statistics().FrameDecodeMBOverflowError++;
                break;

            case MPEG2_DECODER_ERROR_RECOVERED:
                Stream->Statistics().FrameDecodeRecoveredError++;
                break;

            case MPEG2_DECODER_ERROR_NOT_RECOVERED:
                Stream->Statistics().FrameDecodeNotRecoveredError++;
                break;

            case MPEG2_DECODER_ERROR_TASK_TIMEOUT:
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
