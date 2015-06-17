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

#include "osinline.h"
#include "player_generic.h"
#include "player_playback.h"
#include "player_stream.h"
#include "codec_mme_video_divx_hd.h"

#undef TRACE_TAG
#define TRACE_TAG "Codec_MmeVideoDivxHd_c"


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

static BufferDataDescriptor_t           DivxCodecStreamParameterContextDescriptor = BUFFER_DIVX_CODEC_STREAM_PARAMETER_CONTEXT_TYPE;
static BufferDataDescriptor_t           DivxCodecDecodeContextDescriptor = BUFFER_DIVX_CODEC_DECODE_CONTEXT_TYPE;

Codec_MmeVideoDivxHd_c::Codec_MmeVideoDivxHd_c(void)
    : Codec_MmeVideo_c()
#if (MPEG4P2_MME_VERSION >= 20)
    , DivxTransformCapability()
#endif
    , DivxInitializationParameters()
    , DivXGlobalParamSequence()
    , ReturnParams()
    , DecodeParams()
    , MaxWidth(0)
    , MaxHeight(0)
    , IsDivX(false)
    , RasterOutput(false)
{
    Configuration.CodecName                             = "DivX HD Video";
    Configuration.StreamParameterContextCount           = 1;
    Configuration.StreamParameterContextDescriptor      = &DivxCodecStreamParameterContextDescriptor;
    Configuration.DecodeContextCount                    = 4;
    Configuration.DecodeContextDescriptor               = &DivxCodecDecodeContextDescriptor;
    Configuration.MaxDecodeIndicesPerBuffer             = 2;
    Configuration.SliceDecodePermitted                  = false;
    Configuration.DecimatedDecodePermitted              = true;
    Configuration.TransformName[0]                      = "MPEG4P2_TRANSFORMER0";
    Configuration.TransformName[1]                      = "MPEG4P2_TRANSFORMER1";
    Configuration.AvailableTransformers                 = 2;
#if (MPEG4P2_MME_VERSION >= 20)
    Configuration.SizeOfTransformCapabilityStructure    = sizeof(Divx_TransformerCapability_t);
    Configuration.TransformCapabilityStructurePointer   = (void *)(&DivxTransformCapability);
#endif
    // The video firmware violates the MME spec. and passes data buffer addresses
    // as parametric information. For this reason it requires physical addresses
    // to be used.
    Configuration.AddressingMode                        = PhysicalAddress;
    // We do not need the coded data after decode is complete
    Configuration.ShrinkCodedDataBuffersAfterDecode     = true;
    // Trick Mode Parameters
    //
    Configuration.TrickModeParameters.EmpiricalMaximumDecodeFrameRateShortIntegration   = 60;
    Configuration.TrickModeParameters.EmpiricalMaximumDecodeFrameRateLongIntegration    = 60;
    Configuration.TrickModeParameters.DecodeFrameRateShortIntegrationForIOnly       = 60;
    Configuration.TrickModeParameters.DecodeFrameRateLongIntegrationForIOnly        = 60;
    Configuration.TrickModeParameters.SubstandardDecodeSupported        = false;
    Configuration.TrickModeParameters.SubstandardDecodeRateIncrease     = 1;
    Configuration.TrickModeParameters.DefaultGroupSize          = 12;
    Configuration.TrickModeParameters.DefaultGroupReferenceFrameCount   = 4;
}

Codec_MmeVideoDivxHd_c::~Codec_MmeVideoDivxHd_c(void)
{
    Halt();
}

// /////////////////////////////////////////////////////////////////////////
//
//      Function to deal with the returned capabilities
//      structure for a divx mme transformer.
//
CodecStatus_t   Codec_MmeVideoDivxHd_c::HandleCapabilities(void)
{
    // Default to using Omega2 unless Capabilities tell us otherwise
    BufferFormat_t  DisplayFormat = FormatVideo420_MacroBlock;
    // Default elements to produce in the buffers
    DecodeBufferComponentElementMask_t Elements = PrimaryManifestationElement |
                                                  DecimatedManifestationElement |
                                                  VideoMacroblockStructureElement;
    IsDivX = false;
    RasterOutput = false;
    SE_INFO(group_decoder_video, "MME Transformer '%s' capabilities are :-\n", Configuration.TransformName[0]);
    SE_DEBUG(group_decoder_video, "  mpeg4p2_mme_version %d\n", MPEG4P2_MME_VERSION);
#if (MPEG4P2_MME_VERSION >= 20)
    Divx_TransformerCapability_t *DivxTransformCapability_p = (Divx_TransformerCapability_t *)Configuration.TransformCapabilityStructurePointer;
#if (MPEG4P2_MME_VERSION >= 21)

    if (DivxTransformCapability_p->IsDivx)
    {
        SE_INFO(group_decoder_video, "  (DivX compatible)\n");
        IsDivX = DivxTransformCapability_p->IsDivx;
    }

#endif

    // Setting up the Display Format
    if (DivxTransformCapability_p->DisplayBufferFormat == DivX_RASTER)
    {
        // Display format : Raster
        DisplayFormat           = FormatVideo420_Raster2B;
        Elements                |= VideoDecodeCopyElement;
        RasterOutput            = true;
        SE_DEBUG(group_decoder_video, "  Raster Display Format supported\n");
    }
    else if (DivxTransformCapability_p->DisplayBufferFormat == DivX_OMEGA2)
    {
        //Display format : Omega 2
        DisplayFormat           = FormatVideo420_MacroBlock;
        SE_DEBUG(group_decoder_video, "  Omega2 Display Format supported\n");
    }
    else
    {
        //Display format : Omega3
        SE_DEBUG(group_decoder_video, "  Display format not yet supported in Manifestor\n");
    }

    // Setting up the Maximum resolution supported by the FW
    if ((DivxTransformCapability_p->MaxDecodeResolution == DivX_HD_1080i) || (DivxTransformCapability_p->MaxDecodeResolution == DivX_HD_1080p))
    {
        SE_INFO(group_decoder_video, "  MaxDecodeResolution 1920x1080\n");
        MaxWidth  = 1920;
        MaxHeight = 1088;
    }
    else if (DivxTransformCapability_p->MaxDecodeResolution == DivX_720p)
    {
        SE_INFO(group_decoder_video, "  MaxDecodeResolution 1280x720\n");
        MaxWidth  = 1280;
        MaxHeight = 736;
    }
    else
#endif
    {
        SE_INFO(group_decoder_video, "  MaxDecodeResolution 720x576\n");
        MaxWidth  = 720;
        MaxHeight = 576;
    }

    Stream->GetDecodeBufferManager()->FillOutDefaultList(DisplayFormat, Elements,
                                                         Configuration.ListOfDecodeBufferComponents);
    return CodecNoError;
}

///////////////////////////////////////////////////////////////////////////
///
///     Allocate the Divx macroblocks structure.
///
///     Due to the structure of the shared super-class, most codecs allocate
///     the majority of the resources when the player supplies it with an
///     output buffer.
///
CodecStatus_t   Codec_MmeVideoDivxHd_c::Connect(Port_c *Port)
{
    CodecStatus_t Status;
    Status = Codec_MmeVideo_c::Connect(Port);

    if (Status != CodecNoError)
    {
        return Status;
    }

    return CodecNoError;
}

// /////////////////////////////////////////////////////////////////////////
//
//      Function to extend the buffer request to incorporate the macroblock sizing
//

CodecStatus_t   Codec_MmeVideoDivxHd_c::FillOutDecodeBufferRequest(DecodeBufferRequest_t    *Request)
{
    //
    // Fill out the standard fields first
    //
    Codec_MmeVideo_c::FillOutDecodeBufferRequest(Request);
    //
    // and now the extended field
    //
    Request->MacroblockSize                             = 256;
    Request->PerMacroblockMacroblockStructureSize   = 32;
    Request->PerMacroblockMacroblockStructureFifoSize   = 512; // Fifo length is 256 bytes for 7108 but 512 are requested for MPE42c1, let's use widest value it's easier
    return CodecNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Function to deal with the returned capabilities
//      structure for a divx mme transformer.
//

CodecStatus_t Codec_MmeVideoDivxHd_c::FillOutTransformerInitializationParameters(void)
{
    Mpeg4VideoStreamParameters_t  *Parsed  = (Mpeg4VideoStreamParameters_t *)ParsedFrameParameters->StreamParameterStructure;

    // This may not be needed may be able to go back to frame parser defaulting to 500
    if (Parsed->VolHeader.version == 100)
    {
        Parsed->VolHeader.version = 500;
    }

    if ((Parsed->VolHeader.width > MaxWidth) ||
        (Parsed->VolHeader.height > MaxHeight))
    {
        SE_ERROR("Stream dimensions too large for playback (%d x %d) > (%d x %d)\n",
                 Parsed->VolHeader.width, Parsed->VolHeader.height, MaxWidth, MaxHeight);
        Stream->MarkUnPlayable();
        return CodecError;
    }

    DivxInitializationParameters.width                          = Parsed->VolHeader.width;
    DivxInitializationParameters.height                         = Parsed->VolHeader.height;
    DivxInitializationParameters.codec_version                  = Parsed->VolHeader.version;
    DivxInitializationParameters.CircularBufferBeginAddr_p      = (DivX_CompressedData_t)0x00000000;
    DivxInitializationParameters.CircularBufferEndAddr_p        = (DivX_CompressedData_t)0xfffffff8;
    MMEInitializationParameters.TransformerInitParamsSize       = sizeof(DivxInitializationParameters);
    MMEInitializationParameters.TransformerInitParams_p         = (MME_GenericParams_t)(&DivxInitializationParameters);
    return CodecNoError;
}

static void MMECallbackStub(MME_Event_t      Event,
                            MME_Command_t   *CallbackData,
                            void            *UserData)
{
    Codec_MmeBase_c         *Self = (Codec_MmeBase_c *)UserData;
    Self->CallbackFromMME(Event, CallbackData);
}

// /////////////////////////////////////////////////////////////////////////
// // We need to initialise the transformer once we actually have the stream
// parameters, so for now we do nothing on the base class call and do the
// work later on in the set stream parameters
//
CodecStatus_t   Codec_MmeVideoDivxHd_c::InitializeMMETransformer(void)
{
    return Codec_MmeBase_c::VerifyMMECapabilities(0);
}

// /////////////////////////////////////////////////////////////////////////
//
//      Function to fill out the stream parameters
//      structure for a DivX MME transformer.
//

CodecStatus_t  Codec_MmeVideoDivxHd_c::FillOutSetStreamParametersCommand(void)
{
    DivxCodecStreamParameterContext_t      *Context        = (DivxCodecStreamParameterContext_t *)StreamParameterContext;
    Mpeg4VideoStreamParameters_t  *Parsed  = (Mpeg4VideoStreamParameters_t *)ParsedFrameParameters->StreamParameterStructure;
#if (MPEG4P2_MME_VERSION >= 21)

    if ((Parsed->VolHeader.version < 500) && (Parsed->VolHeader.version > 100) && !IsDivX)
    {
        SE_ERROR("DivX version 3.11 / 4.12 not available with transformer %s\n", Configuration.TransformName[SelectedTransformer]);
        Stream->MarkUnPlayable();
        return CodecError;
    }

#endif

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
        memset(&MMEInitializationParameters, 0, sizeof(MME_TransformerInitParams_t));
        MMEInitializationParameters.Priority                        = MME_PRIORITY_NORMAL;
        MMEInitializationParameters.StructSize                      = sizeof(MME_TransformerInitParams_t);
        MMEInitializationParameters.Callback                        = &MMECallbackStub;
        MMEInitializationParameters.CallbackUserData                = this;
        Status      = FillOutTransformerInitializationParameters();

        if (Status != CodecNoError)
        {
            SE_ERROR("(%s) - Failed to fill out transformer initialization parameters (%08x)\n", Configuration.CodecName, Status);
            return Status;
        }

        MMEStatus   = MME_InitTransformer(Configuration.TransformName[SelectedTransformer], &MMEInitializationParameters, &MMEHandle);

        if (MMEStatus != MME_SUCCESS)
        {
            SE_ERROR("(%s) - Failed to initialize mme transformer (%08x)\n", Configuration.CodecName, MMEStatus);
            return CodecError;
        }

        MMEInitialized      = true;
        MMECallbackPriorityBoosted = false;
    }

    if ((Parsed->VolHeader.width > MaxWidth) ||
        (Parsed->VolHeader.height > MaxHeight))
    {
        SE_ERROR("Stream Dimensions too large %d x %d greater than Capabilities %d x %d\n",
                 Parsed->VolHeader.width, Parsed->VolHeader.height, MaxWidth, MaxHeight);
        Stream->MarkUnPlayable();
        return CodecError;
    }

    // This may not be needed may be able to go back to frame parser defaulting to 500
    if (Parsed->VolHeader.version == 100)
    {
        Parsed->VolHeader.version = 500;
    }

    DivXGlobalParamSequence.shape = Parsed->VolHeader.shape;
    DivXGlobalParamSequence.time_increment_resolution = Parsed->VolHeader.time_increment_resolution;
    DivXGlobalParamSequence.interlaced = Parsed->VolHeader.interlaced;
    DivXGlobalParamSequence.sprite_usage = Parsed->VolHeader.sprite_usage;
    DivXGlobalParamSequence.quant_type = Parsed->VolHeader.quant_type;
    DivXGlobalParamSequence.load_intra_quant_matrix = Parsed->VolHeader.load_intra_quant_matrix;
    DivXGlobalParamSequence.load_nonintra_quant_matrix = Parsed->VolHeader.load_non_intra_quant_matrix;
    DivXGlobalParamSequence.quarter_pixel = Parsed->VolHeader.quarter_pixel;
    // required for GMC support
    //  DivXGlobalParamSequence.sprite_warping_accuracy = Parsed->VolHeader.sprite_warping_accuracy;
    DivXGlobalParamSequence.data_partitioning = Parsed->VolHeader.data_partitioning;
    DivXGlobalParamSequence.reversible_vlc = Parsed->VolHeader.reversible_vlc;
    DivXGlobalParamSequence.resync_marker_disable = Parsed->VolHeader.resync_marker_disable;
    DivXGlobalParamSequence.short_video_header = 0;
    DivXGlobalParamSequence.num_mb_in_gob = 0;
    DivXGlobalParamSequence.num_gobs_in_vop = 0;
    memcpy(DivXGlobalParamSequence.intra_quant_matrix, Parsed->VolHeader.intra_quant_matrix, (sizeof(unsigned char) * 64));
    memcpy(DivXGlobalParamSequence.nonintra_quant_matrix, Parsed->VolHeader.non_intra_quant_matrix, (sizeof(unsigned char) * 64));
    memset(&Context->BaseContext.MMECommand, 0, sizeof(MME_Command_t));
    Context->BaseContext.MMECommand.CmdStatus.AdditionalInfoSize        = 0;
    Context->BaseContext.MMECommand.CmdStatus.AdditionalInfo_p          = NULL;
    Context->BaseContext.MMECommand.ParamSize                           = sizeof(DivXGlobalParamSequence);
    Context->BaseContext.MMECommand.Param_p                             = (MME_GenericParams_t)(&DivXGlobalParamSequence);
    return CodecNoError;
}

CodecStatus_t Codec_MmeVideoDivxHd_c::FillOutDecodeCommand(void)
{
    unsigned int  Entry;
    Buffer_t      DecodeBuffer;
    Buffer_t      ReferenceBuffer;
    DecodeBufferComponentType_t   DisplayComponent;
    DecodeBufferComponentType_t   DecimatedComponent;
    DecodeBufferComponentType_t   DecodeComponent;
    KnownLastSliceInFieldFrame = true;
    Mpeg4VideoFrameParameters_t  *Frame  = (Mpeg4VideoFrameParameters_t *)ParsedFrameParameters->FrameParameterStructure;
    OS_LockMutex(&Lock);
    DecodeBuffer           = BufferState[CurrentDecodeBufferIndex].Buffer;
    DisplayComponent       = PrimaryManifestationComponent;
    DecimatedComponent     = DecimatedManifestationComponent;
    DecodeComponent        = RasterOutput ? VideoDecodeCopy : PrimaryManifestationComponent;
    DecodeParams.PictureStartAddr_p = (DivX_CompressedData_t)(CodedData);
    DecodeParams.PictureEndAddr_p = (DivX_CompressedData_t)(CodedData + CodedDataLength);

    if (Player->PolicyValue(Playback, Stream, PolicyDecimateDecoderOutput) != PolicyValueDecimateDecoderOutputDisabled)
    {
        DecodeParams.DecodedBufferAddr.LumaDecimated_p                     = (DivX_LumaAddress_t)Stream->GetDecodeBufferManager()->Luma(DecodeBuffer, DecimatedComponent);
        DecodeParams.DecodedBufferAddr.ChromaDecimated_p                   = (DivX_ChromaAddress_t)Stream->GetDecodeBufferManager()->Chroma(DecodeBuffer, DecimatedComponent);
    }
    else
    {
        DecodeParams.DecodedBufferAddr.LumaDecimated_p                     = NULL;
        DecodeParams.DecodedBufferAddr.ChromaDecimated_p                   = NULL;
    }

#if (MPEG4P2_MME_VERSION >= 18)
    DecodeParams.DecodingMode = DIVX_NORMAL_DECODE;
    DecodeParams.AdditionalFlags = DIVX_ADDITIONAL_FLAG_NONE;
#endif
    DecodeParams.DecodedBufferAddr.Luma_p = (unsigned int *)Stream->GetDecodeBufferManager()->Luma(DecodeBuffer, DecodeComponent);
    DecodeParams.DecodedBufferAddr.Chroma_p = (unsigned int *)Stream->GetDecodeBufferManager()->Chroma(DecodeBuffer, DecodeComponent);

    if (ParsedFrameParameters->ReferenceFrame)
    {
        DecodeParams.DecodedBufferAddr.MBStruct_p = (unsigned int *)Stream->GetDecodeBufferManager()->ComponentBaseAddress(DecodeBuffer, VideoMacroblockStructure);
    }

#if (MPEG4P2_MME_VERSION >= 14)
    DecodeParams.DecodedBufferAddr.DisplayLuma_p   = (unsigned int *)Stream->GetDecodeBufferManager()->Luma(DecodeBuffer, DisplayComponent);
    DecodeParams.DecodedBufferAddr.DisplayChroma_p = (unsigned int *)Stream->GetDecodeBufferManager()->Chroma(DecodeBuffer, DisplayComponent);
#endif

    switch (Player->PolicyValue(Playback, Stream, PolicyDecimateDecoderOutput))
    {
    case PolicyValueDecimateDecoderOutputDisabled:
    {
        // Normal Case
#if (MPEG4P2_MME_VERSION >= 14)
        if (RasterOutput && ParsedFrameParameters->ReferenceFrame)
        {
            DecodeParams.MainAuxEnable = DIVX_REF_MAIN_DISP_MAIN_EN;
        }
        else
        {
            DecodeParams.MainAuxEnable = DIVX_DISP_MAIN_EN;
        }
#else
        DecodeParams.MainAuxEnable                        = DIVX_MAINOUT_EN;
#endif
        DecodeParams.HorizontalDecimationFactor           = DIVX_HDEC_1;
        DecodeParams.VerticalDecimationFactor             = DIVX_VDEC_1;
        break;
    }

    case PolicyValueDecimateDecoderOutputHalf:
    {
#if (MPEG4P2_MME_VERSION >= 14)
        if (RasterOutput && ParsedFrameParameters->ReferenceFrame)
        {
            DecodeParams.MainAuxEnable = DIVX_REF_MAIN_DISP_MAIN_AUX_EN;
        }
        else
        {
            DecodeParams.MainAuxEnable = DIVX_DISP_AUX_MAIN_EN;
        }
#else
        DecodeParams.MainAuxEnable = DIVX_AUX_MAIN_OUT_EN;
#endif
        DecodeParams.HorizontalDecimationFactor           = DIVX_HDEC_ADVANCED_2;
        DecodeParams.VerticalDecimationFactor             = DIVX_VDEC_ADVANCED_2_PROG;
        break;
    }

    case PolicyValueDecimateDecoderOutputQuarter:
    {
#if (MPEG4P2_MME_VERSION >= 14)
        if (RasterOutput && ParsedFrameParameters->ReferenceFrame)
        {
            DecodeParams.MainAuxEnable = DIVX_REF_MAIN_DISP_MAIN_AUX_EN;
        }
        else
        {
            DecodeParams.MainAuxEnable = DIVX_DISP_AUX_MAIN_EN;
        }
#else
        DecodeParams.MainAuxEnable = DIVX_AUX_MAIN_OUT_EN;
#endif
        DecodeParams.HorizontalDecimationFactor           = DIVX_HDEC_ADVANCED_4;
        DecodeParams.VerticalDecimationFactor             = DIVX_VDEC_ADVANCED_2_PROG;
        break;
    }
    }

    // Setting up the default for the reference buffer
    if (ParsedFrameParameters->NumberOfReferenceFrameLists != 0)
    {
        if (DecodeContext->ReferenceFrameList[0].EntryCount == 1)
        {
            Entry       = DecodeContext->ReferenceFrameList[0].EntryIndicies[0];
            ReferenceBuffer = BufferState[Entry].Buffer;
            DecodeParams.RefPicListAddr.BackwardRefLuma_p = (U32 *)Stream->GetDecodeBufferManager()->Luma(ReferenceBuffer, DecodeComponent);
            DecodeParams.RefPicListAddr.BackwardRefChroma_p = (U32 *)Stream->GetDecodeBufferManager()->Chroma(ReferenceBuffer, DecodeComponent);
        }

        if (DecodeContext->ReferenceFrameList[0].EntryCount == 2)
        {
            unsigned int ForEntry       = DecodeContext->ReferenceFrameList[0].EntryIndicies[0];
            ReferenceBuffer         = BufferState[ForEntry].Buffer;
            DecodeParams.RefPicListAddr.ForwardRefLuma_p = (U32 *)Stream->GetDecodeBufferManager()->Luma(ReferenceBuffer, DecodeComponent);
            DecodeParams.RefPicListAddr.ForwardRefChroma_p = (U32 *)Stream->GetDecodeBufferManager()->Chroma(ReferenceBuffer, DecodeComponent);
            unsigned int BackEntry      = DecodeContext->ReferenceFrameList[0].EntryIndicies[1];
            ReferenceBuffer         = BufferState[BackEntry].Buffer;
            DecodeParams.RefPicListAddr.BackwardRefLuma_p = (U32 *)Stream->GetDecodeBufferManager()->Luma(ReferenceBuffer, DecodeComponent);
            DecodeParams.RefPicListAddr.BackwardRefChroma_p = (U32 *)Stream->GetDecodeBufferManager()->Chroma(ReferenceBuffer, DecodeComponent);
        }
    }
    else if (Frame->VopHeader.prediction_type != PREDICTION_TYPE_I)
    {
        SE_ERROR("unexpected wrong prediction type (expected PREDICTION_TYPE_I)\n");
    }

    OS_UnLockMutex(&Lock);
    DecodeParams.bit_skip_no = Frame->bit_skip_no;
    DecodeParams.use_intra_dc_vlc = Frame->VopHeader.intra_dc_vlc_thr ? 0 : 1;
    DecodeParams.shape_coding_type = 0;
    DecodeParams.trb_trd = Frame->VopHeader.trb_trd;
    DecodeParams.trb_trd_trd = Frame->VopHeader.trb_trd_trd;
    DecodeParams.prediction_type = (DivX_PictureType_t)Frame->VopHeader.prediction_type;
    DecodeParams.quantizer = Frame->VopHeader.quantizer;
    DecodeParams.rounding_type = Frame->VopHeader.rounding_type ;
    DecodeParams.fcode_for = Frame->VopHeader.fcode_forward;
    DecodeParams.fcode_back = Frame->VopHeader.fcode_backward;
    DecodeParams.vop_coded = Frame->VopHeader.vop_coded;
    DecodeParams.intra_dc_vlc_thr = Frame->VopHeader.intra_dc_vlc_thr;
    DecodeParams.alternate_vertical_scan_flag = Frame->VopHeader.alternate_vertical_scan_flag;
    DecodeParams.top_field_first = Frame->VopHeader.top_field_first;
    memset(&(DecodeContext->MMECommand.CmdStatus), 0, sizeof(MME_CommandStatus_t));
    DecodeContext->MMECommand.CmdStatus.AdditionalInfoSize = sizeof(ReturnParams);
    DecodeContext->MMECommand.CmdStatus.AdditionalInfo_p   = (MME_GenericParams_t *)&ReturnParams;
    DecodeContext->MMECommand.StructSize = sizeof(MME_Command_t);
    DecodeContext->MMECommand.CmdCode = MME_TRANSFORM;
    DecodeContext->MMECommand.CmdEnd = MME_COMMAND_END_RETURN_NOTIFY;
    DecodeContext->MMECommand.DueTime = (MME_Time_t)0;
    DecodeContext->MMECommand.DataBuffers_p = NULL;
    DecodeContext->MMECommand.NumberInputBuffers = 0;
    DecodeContext->MMECommand.NumberOutputBuffers = 0;
    DecodeContext->MMECommand.ParamSize = sizeof(DecodeParams);
    DecodeContext->MMECommand.Param_p = (MME_GenericParams_t *)&DecodeParams;
    return CodecNoError;
}

CodecStatus_t Codec_MmeVideoDivxHd_c::ValidateDecodeContext(CodecBaseDecodeContext_t *Context)
{
    return CodecNoError;
}

#if (MPEG4P2_MME_VERSION >= 13)
// Convert the return code into human readable form.
static const char *LookupError(unsigned int Error)
{
#define E(e) case e: return #e

    switch (Error)
    {
        E(DIVX_HEADER_DAMAGED);
        E(DIVX_NEW_PRED_NOT_SUPPORTED);
        E(DIVX_SCALABILITY_NOT_SUPPORTED);
        E(DIVX_VOP_SHAPE_NOT_SUPPORTED);
        E(DIVX_STATIC_SPRITE_NOT_SUPPORTED);
        E(DIVX_GMC_NOT_SUPPORTED);
        E(DIVX_QPEL_NOT_SUPPORTED);
        E(DIVX_FIRMWARE_TIME_OUT_ENCOUNTERED);
        E(DIVX_ILLEGAL_MB_NUM_PCKT);
        E(DIVX_CBPY_DAMAGED);
        E(DIVX_CBPC_DAMAGED);
        E(DIVX_ILLEGAL_MB_TYPE);
        E(DIVX_DC_DAMAGED);
        E(DIVX_AC_DAMAGED);
        E(DIVX_MISSING_MARKER_BIT);
        E(DIVX_INVALID_PICTURE_SIZE);
        E(DIVX_ERROR_RECOVERED);
        E(DIVX_ERROR_NOT_RECOVERED);

    default: return "DIVX_DECODER_UNKNOWN_ERROR";
    }

#undef E
}

CodecStatus_t   Codec_MmeVideoDivxHd_c::CheckCodecReturnParameters(CodecBaseDecodeContext_t *Context)
{
    MME_Command_t                     *MMECommand       = (MME_Command_t *)(&Context->MMECommand);
    MME_CommandStatus_t               *CmdStatus        = (MME_CommandStatus_t *)(&MMECommand->CmdStatus);
    MME_DivXVideoDecodeReturnParams_t *AdditionalInfo_p = (MME_DivXVideoDecodeReturnParams_t *)CmdStatus->AdditionalInfo_p;

    if (AdditionalInfo_p != NULL)
    {
        if (AdditionalInfo_p->Errortype != DIVX_NO_ERROR)
        {
            // Set the decode quality to worst because there is a decoding error and there is no emulation.
            // This is done to allow flexibity for not displaying the corrupt output, using player policy, after encountering the decoding error.
            Context->DecodeQuality = 0;
            SE_INFO(group_decoder_video, "Error: %s  0x%x\n", LookupError(AdditionalInfo_p->Errortype), AdditionalInfo_p->Errortype);

            switch (AdditionalInfo_p->Errortype)
            {
            case DIVX_FIRMWARE_TIME_OUT_ENCOUNTERED:
                Stream->Statistics().FrameDecodeErrorTaskTimeOutError++;
                break;

            case DIVX_ERROR_RECOVERED:
                Stream->Statistics().FrameDecodeRecoveredError++;
                break;

            case DIVX_ERROR_NOT_RECOVERED:
                Stream->Statistics().FrameDecodeNotRecoveredError++;
                break;

            case DIVX_HEADER_DAMAGED:
            case DIVX_NEW_PRED_NOT_SUPPORTED:
            case DIVX_SCALABILITY_NOT_SUPPORTED:
            case DIVX_VOP_SHAPE_NOT_SUPPORTED:
            case DIVX_STATIC_SPRITE_NOT_SUPPORTED:
            case DIVX_GMC_NOT_SUPPORTED:
            case DIVX_QPEL_NOT_SUPPORTED:
            case DIVX_ILLEGAL_MB_NUM_PCKT:
            case DIVX_CBPY_DAMAGED:
            case DIVX_CBPC_DAMAGED:
            case DIVX_ILLEGAL_MB_TYPE:
            case DIVX_DC_DAMAGED:
            case DIVX_AC_DAMAGED:
            case DIVX_MISSING_MARKER_BIT:
            case DIVX_INVALID_PICTURE_SIZE:
            default:
                Stream->Statistics().FrameDecodeError++;
                break;
            }
        }
    }

    return CodecNoError;
}
#endif

CodecStatus_t Codec_MmeVideoDivxHd_c::DumpSetStreamParameters(void *Parameters)
{
    return CodecNoError;
}

CodecStatus_t  Codec_MmeVideoDivxHd_c::DumpDecodeParameters(void *Parameters)
{
    MME_DivXVideoDecodeParams_t *Param = (MME_DivXVideoDecodeParams_t *) Parameters;
    SE_VERBOSE(group_decoder_video, "-----------------------------------------\n");
    SE_VERBOSE(group_decoder_video, "RasterOutput          %d\n",   RasterOutput);
    SE_VERBOSE(group_decoder_video, "PictureStartAddr_p    %p\n", Param->PictureStartAddr_p);
    SE_VERBOSE(group_decoder_video, "PictureEndAddr_p      %p\n", Param->PictureEndAddr_p);
    SE_VERBOSE(group_decoder_video, "DecodedBufferAddr\n");
    SE_VERBOSE(group_decoder_video, " Luma_p              %p\n", Param->DecodedBufferAddr.Luma_p);
    SE_VERBOSE(group_decoder_video, " Chroma_p            %p\n", Param->DecodedBufferAddr.Chroma_p);
#if (MPEG4P2_MME_VERSION >= 14)
    SE_VERBOSE(group_decoder_video, " DisplayLuma_p       %p\n", Param->DecodedBufferAddr.DisplayLuma_p);
    SE_VERBOSE(group_decoder_video, " DisplayChroma_p     %p\n", Param->DecodedBufferAddr.DisplayChroma_p);
#endif
    SE_VERBOSE(group_decoder_video, " MBStruct_p          %p\n", Param->DecodedBufferAddr.MBStruct_p);
    SE_VERBOSE(group_decoder_video, "RefPicListAddr\n");
    SE_VERBOSE(group_decoder_video, " BackwardRefLuma_p   %p\n", Param->RefPicListAddr.BackwardRefLuma_p);
    SE_VERBOSE(group_decoder_video, " BackwardRefChroma_p %p\n", Param->RefPicListAddr.BackwardRefChroma_p);
    SE_VERBOSE(group_decoder_video, " ForwardRefLuma_p    %p\n", Param->RefPicListAddr.ForwardRefLuma_p);
    SE_VERBOSE(group_decoder_video, " ForwardRefChroma_p  %p\n", Param->RefPicListAddr.ForwardRefChroma_p);
    SE_VERBOSE(group_decoder_video, "MainAuxEnable              0x%x\n", Param->MainAuxEnable);
    SE_VERBOSE(group_decoder_video, "HorizontalDecimationFactor 0x%x\n", Param->HorizontalDecimationFactor);
    SE_VERBOSE(group_decoder_video, "VerticalDecimationFactor   0x%x\n", Param->VerticalDecimationFactor);
    SE_VERBOSE(group_decoder_video, "ReferenceFrame %d\n",    ParsedFrameParameters->ReferenceFrame);
    SE_VERBOSE(group_decoder_video, "-----------------------------------------\n");
    return CodecNoError;
}
