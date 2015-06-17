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

#include "codec_mme_video_vc1.h"
#include "vc1.h"
#include "player_generic.h"
#include "player_playback.h"
#include "player_stream.h"

#undef TRACE_TAG
#define TRACE_TAG "Codec_MmeVideoVc1_c"

typedef struct Vc1CodecStreamParameterContext_s
{
    CodecBaseStreamParameterContext_t   BaseContext;
    //VC9_SetGlobalParamSequence_t      StreamParameters;
} Vc1CodecStreamParameterContext_t;

#define BUFFER_VC1_CODEC_STREAM_PARAMETER_CONTEXT               "Vc1CodecStreamParameterContext"
#define BUFFER_VC1_CODEC_STREAM_PARAMETER_CONTEXT_TYPE  {BUFFER_VC1_CODEC_STREAM_PARAMETER_CONTEXT, BufferDataTypeBase, AllocateFromOSMemory, 32, 0, true, true, sizeof(Vc1CodecStreamParameterContext_t)}

static BufferDataDescriptor_t           Vc1CodecStreamParameterContextDescriptor = BUFFER_VC1_CODEC_STREAM_PARAMETER_CONTEXT_TYPE;


// --------

typedef struct Vc1CodecDecodeContext_s
{
    CodecBaseDecodeContext_t            BaseContext;
    VC9_TransformParam_t                DecodeParameters;
    VC9_CommandStatus_t                 DecodeStatus;
} Vc1CodecDecodeContext_t;

#define BUFFER_VC1_CODEC_DECODE_CONTEXT         "Vc1CodecDecodeContext"
#define BUFFER_VC1_CODEC_DECODE_CONTEXT_TYPE    {BUFFER_VC1_CODEC_DECODE_CONTEXT, BufferDataTypeBase, AllocateFromOSMemory, 32, 0, true, true, sizeof(Vc1CodecDecodeContext_t)}

static BufferDataDescriptor_t                   Vc1CodecDecodeContextDescriptor = BUFFER_VC1_CODEC_DECODE_CONTEXT_TYPE;

Codec_MmeVideoVc1_c::Codec_MmeVideoVc1_c(void)
    : Codec_MmeVideo_c()
    , Vc1TransformCapability()
    , Vc1InitializationParameters()
    , RasterOutput(false)
{
    Configuration.CodecName                             = "Vc1 video";
    Configuration.StreamParameterContextCount           = 1;
    Configuration.StreamParameterContextDescriptor      = &Vc1CodecStreamParameterContextDescriptor;
    Configuration.DecodeContextCount                    = 4;
    Configuration.DecodeContextDescriptor               = &Vc1CodecDecodeContextDescriptor;
    Configuration.MaxDecodeIndicesPerBuffer             = 2;
    Configuration.SliceDecodePermitted                  = false;
    Configuration.DecimatedDecodePermitted              = true;
    Configuration.TransformName[0]                      = VC9_MME_TRANSFORMER_NAME "0";
    Configuration.TransformName[1]                      = VC9_MME_TRANSFORMER_NAME "1";
    Configuration.AvailableTransformers                 = 2;
#if 0
    // VC1 codec does not properly implement MME_GetTransformerCapability() and any attempt
    // to do so results in a MME_NOT_IMPLEMENTED error. For this reason it is better not to
    // ask.
    Configuration.SizeOfTransformCapabilityStructure    = sizeof(VC9_TransformerCapability_t);
    Configuration.TransformCapabilityStructurePointer   = (void *)(&Vc1TransformCapability);
#endif
    // The video firmware violates the MME spec. and passes data buffer addresses
    // as parametric information. For this reason it requires physical addresses
    // to be used.
    Configuration.AddressingMode                        = PhysicalAddress;
    // We do not need the coded data after decode is complete
    Configuration.ShrinkCodedDataBuffersAfterDecode     = true;
    // Some random trick mode parameters
    Configuration.TrickModeParameters.EmpiricalMaximumDecodeFrameRateShortIntegration   = 60;
    Configuration.TrickModeParameters.EmpiricalMaximumDecodeFrameRateLongIntegration    = 60;
    Configuration.TrickModeParameters.DecodeFrameRateShortIntegrationForIOnly       = 60;
    Configuration.TrickModeParameters.DecodeFrameRateLongIntegrationForIOnly        = 60;
    Configuration.TrickModeParameters.SubstandardDecodeSupported        = false;
    Configuration.TrickModeParameters.SubstandardDecodeRateIncrease     = 1;
    Configuration.TrickModeParameters.DefaultGroupSize                  = 12;
    Configuration.TrickModeParameters.DefaultGroupReferenceFrameCount   = 4;
}

Codec_MmeVideoVc1_c::~Codec_MmeVideoVc1_c(void)
{
    Halt();
}

//{{{  HandleCapabilities
// /////////////////////////////////////////////////////////////////////////
//
//      Function to deal with the returned capabilities
//      structure for an vc1 mme transformer.
//
CodecStatus_t   Codec_MmeVideoVc1_c::HandleCapabilities(void)
{
    // Default to using Omega2 unless Capabilities tell us otherwise
    BufferFormat_t  DisplayFormat = FormatVideo420_MacroBlock;
    // Default elements to produce in the buffers
    DecodeBufferComponentElementMask_t Elements = PrimaryManifestationElement | DecimatedManifestationElement |
                                                  VideoMacroblockStructureElement | VideoPostProcessControlElement;
    SE_INFO(group_decoder_video, "MME Transformer '%s' capabilities are :-\n", VC9_MME_TRANSFORMER_NAME);
// Noting from the constructor, this will not be used
#ifdef DELTATOP_MME_VERSION
#if (VC9_MME_VERSION >= 19)
    DeltaTop_TransformerCapability_t *DeltaTopCapabilities = (DeltaTop_TransformerCapability_t *) Configuration.DeltaTopCapabilityStructurePointer;

    if ((DeltaTopCapabilities != NULL) && (DeltaTopCapabilities->DisplayBufferFormat == DELTA_OUTPUT_RASTER))
    {
        RasterOutput = true;
    }

#endif
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
//{{{  Connect
///////////////////////////////////////////////////////////////////////////
///
///     Allocate the VC1 macroblocks structure.
///
///     Due to the structure of the shared super-class, most codecs allocate
///     the majority of the resources when the player supplies it with an
///     output buffer.
///
CodecStatus_t   Codec_MmeVideoVc1_c::Connect(Port_c *Port)
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
//{{{  FillOutTransformerInitializationParameters
// /////////////////////////////////////////////////////////////////////////
//
//      Function to deal with the returned capabilities
//      structure for an vc1 mme transformer.
//

CodecStatus_t   Codec_MmeVideoVc1_c::FillOutTransformerInitializationParameters(void)
{
    //
    // Fill out the command parameters
    //
    // Vc1InitializationParameters.InputBufferBegin     = 0x00000000;
    // Vc1InitializationParameters.InputBufferEnd       = 0xffffffff;
    SE_VERBOSE(group_decoder_video, "\n");
    //
    // Fill out the actual command
    //
    MMEInitializationParameters.TransformerInitParamsSize       = sizeof(VC9_InitTransformerParam_fmw_t);
    MMEInitializationParameters.TransformerInitParams_p         = (MME_GenericParams_t)(&Vc1InitializationParameters);

    return CodecNoError;
}
//}}}

//{{{  FillOutSetStreamParametersCommand
// /////////////////////////////////////////////////////////////////////////
//
//      Function to fill out the stream parameters
//      structure for an vc1 mme transformer.
//


VC9_CommandStatus_t hack;

CodecStatus_t   Codec_MmeVideoVc1_c::FillOutSetStreamParametersCommand(void)
{
    //
    // Fill out the actual command
    //
    // memset( &Context->BaseContext.MMECommand, 0, sizeof(MME_Command_t) );
    // Context->BaseContext.MMECommand.CmdStatus.AdditionalInfoSize     = 0;
    // Context->BaseContext.MMECommand.CmdStatus.AdditionalInfo_p               = NULL;
    // Context->BaseContext.MMECommand.ParamSize                                = sizeof(VC1_SetGlobalParamSequence_t);
    // Context->BaseContext.MMECommand.Param_p                          = (MME_GenericParams_t)(&Context->StreamParameters);

    StreamParameterContext->MMECommand.CmdStatus.AdditionalInfoSize    = sizeof(VC9_CommandStatus_t);
    StreamParameterContext->MMECommand.CmdStatus.AdditionalInfo_p      = (MME_GenericParams_t)&hack;
    return CodecNoError;
}

//}}}
//{{{  FillOutDecodeBufferRequest
// /////////////////////////////////////////////////////////////////////////
//
//      The specific video function used to fill out a buffer structure
//      request.
//

CodecStatus_t   Codec_MmeVideoVc1_c::FillOutDecodeBufferRequest(DecodeBufferRequest_t    *Request)
{
    //
    // Fill out the standard fields first
    //
    Codec_MmeVideo_c::FillOutDecodeBufferRequest(Request);
    //
    // and now the extended field
    //
    Request->MacroblockSize                             = 256;
    Request->PerMacroblockMacroblockStructureSize   = 16;
    Request->PerMacroblockMacroblockStructureFifoSize   = 512; // Fifo length is 256 bytes for 7108 but 512 are requested for MPE42c1, let's use widest value it's easier
    return CodecNoError;
}

//}}}
//{{{  FillOutDecodeCommand
// /////////////////////////////////////////////////////////////////////////
//
//      Function to fill out the decode parameters
//      structure for an vc1 mme transformer.
//

CodecStatus_t   Codec_MmeVideoVc1_c::FillOutDecodeCommand(void)
{
    Vc1CodecDecodeContext_t         *Context        = (Vc1CodecDecodeContext_t *)DecodeContext;
    Vc1FrameParameters_t            *Frame          = (Vc1FrameParameters_t *)ParsedFrameParameters->FrameParameterStructure;
    Vc1StreamParameters_t           *vc1StreamParams         = (Vc1StreamParameters_t *)ParsedFrameParameters->StreamParameterStructure;
    VC9_TransformParam_t            *Param;
    VC9_DecodedBufferAddress_t      *Decode;
    VC9_RefPicListAddress_t         *RefList;
    VC9_EntryPointParam_t           *Entry;
    VC9_StartCodesParam_t           *StartCodes;
    VC9_IntensityComp_t             *Intensity;
    unsigned int                    i;
    Buffer_t                DecodeBuffer;
    Buffer_t                ReferenceBuffer;
    DecodeBufferComponentType_t     DisplayComponent;
    DecodeBufferComponentType_t     DecimatedComponent;
    DecodeBufferComponentType_t     DecodeComponent;

    if (ParsedVideoParameters == NULL)
    {
        SE_ERROR("(%s) - ParsedVideoParameters are NULL\n", Configuration.CodecName);
        return CodecError;
    }

    //
    // For vc1 we do not do slice decodes.
    //
    KnownLastSliceInFieldFrame                      = true;
    OS_LockMutex(&Lock);
    DecodeBuffer                = BufferState[CurrentDecodeBufferIndex].Buffer;
    DisplayComponent                = PrimaryManifestationComponent;
    DecimatedComponent              = DecimatedManifestationComponent;
    DecodeComponent             = RasterOutput ? VideoDecodeCopy : PrimaryManifestationComponent;
    //
    // Fill out the straight forward command parameters
    //
    Param                                       = &Context->DecodeParameters;
    Decode                                      = &Param->DecodedBufferAddress;
    RefList                                     = &Param->RefPicListAddress;
    Intensity                                   = &Param->IntensityComp;
    Entry                                       = &Param->EntryPoint;
    StartCodes                                  = &Param->StartCodes;

    // The vc1 decoder requires a pointer to the first data byte after the frame header.
    // so the Compressed buffer is adjusted by the size of a standard 4 byte header for
    // Advanced profile only (Main and simple profiles do not have headers).
    if (vc1StreamParams->SequenceHeader.profile == VC1_ADVANCED_PROFILE)
    {
        Param->PictureStartAddrCompressedBuffer_p   = (VC9_CompressedData_t)(CodedData + 4);
    }
    else
    {
        // RP227 compliant VC1 SP/MP streams have Sequence Start code and Frame Start Code
        if (vc1StreamParams->StreamType ==  Vc1StreamTypeVc1Rp227SpMp)
        {
            Param->PictureStartAddrCompressedBuffer_p   = (VC9_CompressedData_t)(CodedData + 4);
        }
        else // Stream->GetStreamType() ==  Vc1StreamTypeWMV
        {
            Param->PictureStartAddrCompressedBuffer_p   = (VC9_CompressedData_t)CodedData;
        }
    }

    Param->PictureStopAddrCompressedBuffer_p    = (VC9_CompressedData_t)(CodedData + CodedDataLength + 32);
    // Two lines below changed in line with modified firmware for skipped pictures
    //Param->CircularBufferBeginAddr_p            = (VC9_CompressedData_t)0x00000000;
    //Param->CircularBufferEndAddr_p              = (VC9_CompressedData_t)0xffffffff;
    Param->CircularBufferBeginAddr_p            = Param->PictureStartAddrCompressedBuffer_p;
    Param->CircularBufferEndAddr_p              = Param->PictureStopAddrCompressedBuffer_p;
    Decode->Luma_p                              = (VC9_LumaAddress_t)Stream->GetDecodeBufferManager()->Luma(DecodeBuffer, DecodeComponent);
    Decode->Chroma_p                            = (VC9_ChromaAddress_t)Stream->GetDecodeBufferManager()->Chroma(DecodeBuffer, DecodeComponent);
    Decode->MBStruct_p              = (VC9_MBStructureAddress_t)Stream->GetDecodeBufferManager()->ComponentBaseAddress(DecodeBuffer, VideoMacroblockStructure);
#if (VC9_MME_VERSION >= 19)
    Decode->DisplayLuma_p           = (VC9_LumaAddress_t)Stream->GetDecodeBufferManager()->Luma(DecodeBuffer, DisplayComponent);
    Decode->DisplayChroma_p         = (VC9_ChromaAddress_t)Stream->GetDecodeBufferManager()->Chroma(DecodeBuffer, DisplayComponent);
#endif

    // cjt

    if (Stream->GetDecodeBufferManager()->ComponentPresent(DecodeBuffer, DecimatedComponent))
    {
        Decode->LumaDecimated_p                 = (VC9_LumaAddress_t)Stream->GetDecodeBufferManager()->Luma(DecodeBuffer, DecimatedComponent);
        Decode->ChromaDecimated_p               = (VC9_ChromaAddress_t)Stream->GetDecodeBufferManager()->Chroma(DecodeBuffer, DecimatedComponent);
    }
    else
    {
        Decode->LumaDecimated_p                 = NULL;
        Decode->ChromaDecimated_p               = NULL;
    }

    if (Frame->PictureHeader.first_field != 0)
    {
        // Initialise the intensity compensation information (0xffffffff is no intensity com and is an invalid value)
        memset(&BufferState[CurrentDecodeBufferIndex].AppliedIntensityCompensation, 0xff, sizeof(CodecIntensityCompensation_t));
    }

    //{{{  Attach range mapping info if required
    if (1)
    {
        VideoPostProcessingControl_t       *PPControl;
        PPControl   = (VideoPostProcessingControl_t *)Stream->GetDecodeBufferManager()->ComponentBaseAddress(DecodeBuffer, VideoPostProcessControl);
        memset(PPControl, 0, sizeof(VideoPostProcessingControl_t));

        if (vc1StreamParams->SequenceHeader.profile == VC1_ADVANCED_PROFILE)
        {
            if (vc1StreamParams->EntryPointHeader.range_mapy_flag == 1)        // Y[n] = CLIP((((Y[n]-128)*(RANGE_MAP_Y+9)+4)>>3)+128);
            {
                PPControl->RangeMapLumaPresent         = true;
                PPControl->RangeMapLuma                = vc1StreamParams->EntryPointHeader.range_mapy;
            }

            if (vc1StreamParams->EntryPointHeader.range_mapuv_flag == 1)
            {
                PPControl->RangeMapChromaPresent       = true;
                PPControl->RangeMapChroma              = vc1StreamParams->EntryPointHeader.range_mapy;
            }
        }
        else if (Frame->PictureHeader.rangeredfrm == 1)   // implies that sequence->rangered == 1
        {
            PPControl->RangeMapLumaPresent             = true;
            PPControl->RangeMapLuma                    = 7;    // Y[n] = CLIP((Y[n]-128)*2+128);
            PPControl->RangeMapChromaPresent           = true;
            PPControl->RangeMapChroma                  = 7;    // so Luma and Chroma values should be set to 7 in above formula
        }

        SE_VERBOSE(group_decoder_video, "Luma (%d, %d), Chroma %d, %d)\n",
                   PPControl->RangeMapLumaPresent, PPControl->RangeMapLuma, PPControl->RangeMapChromaPresent, PPControl->RangeMapChroma);
    }

    //}}}

    if (!Stream->GetDecodeBufferManager()->ComponentPresent(DecodeBuffer, DecimatedComponent))
    {
        // Normal Case
#if (VC9_MME_VERSION >= 19)
        if (RasterOutput && ParsedFrameParameters->ReferenceFrame)
        {
            Param->MainAuxEnable = VC9_REF_MAIN_DISP_MAIN_EN;
        }
        else
        {
            Param->MainAuxEnable = VC9_DISP_MAIN_EN;
        }
#else
        Param->MainAuxEnable                        = VC9_MAINOUT_EN;
#endif
        Param->HorizontalDecimationFactor           = VC9_HDEC_1;
        Param->VerticalDecimationFactor             = VC9_VDEC_1;
    }
    else
    {
#if (VC9_MME_VERSION >= 19)
        if (RasterOutput && ParsedFrameParameters->ReferenceFrame)
        {
            Param->MainAuxEnable = VC9_REF_MAIN_DISP_MAIN_AUX_EN;
        }
        else
        {
            Param->MainAuxEnable = VC9_DISP_AUX_MAIN_EN;
        }
#else
        Param->MainAuxEnable                        = VC9_AUX_MAIN_OUT_EN;
#endif
        Param->HorizontalDecimationFactor           = (Stream->GetDecodeBufferManager()->DecimationFactor(DecodeBuffer, 0) == 2) ?
                                                      VC9_HDEC_ADVANCED_2 :
                                                      VC9_HDEC_ADVANCED_4;

        if ((VC9_PictureSyntax_t)Frame->PictureHeader.fcm == VC9_PICTURE_SYNTAX_PROGRESSIVE)
        {
            Param->VerticalDecimationFactor         = VC9_VDEC_ADVANCED_2_PROG;
        }
        else
        {
            Param->VerticalDecimationFactor         = VC9_VDEC_ADVANCED_2_INT;
        }
    }

    Param->DecodingMode                         = VC9_NORMAL_DECODE;
#if VC9_MME_VERSION >= 17
    Param->AdditionalFlags                      = VC9_ADDITIONAL_FLAG_NONE;
#else
    Param->AdditionalFlags                      = 0;
#endif

    switch (vc1StreamParams->SequenceHeader.profile)
    {
    case VC1_SIMPLE_PROFILE:
        Param->AebrFlag                     = (vc1StreamParams->StreamType ==  Vc1StreamTypeVc1Rp227SpMp) ? 1 : 0;
        Param->Profile                      = VC9_PROFILE_SIMPLE;
        break;

    case VC1_MAIN_PROFILE:
        Param->AebrFlag                     = (vc1StreamParams->StreamType ==  Vc1StreamTypeVc1Rp227SpMp) ? 1 : 0;
        Param->Profile                      = VC9_PROFILE_MAIN;
        break;

    case VC1_ADVANCED_PROFILE:
    default:
        Param->AebrFlag                     = 1;
        Param->Profile                      = VC9_PROFILE_ADVANCE;
        break;
    }

    Param->PictureSyntax                        = (VC9_PictureSyntax_t)Frame->PictureHeader.fcm;        // Prog/Field/Frame
    Param->PictureType                          = (VC9_PictureType_t)Frame->PictureHeader.ptype;        // I/P/B/BI/Skip
    Param->FinterpFlag                          = vc1StreamParams->SequenceHeader.finterpflag;
    Param->FrameHorizSize                       = ParsedVideoParameters->Content.Width;
    Param->FrameVertSize                        = ParsedVideoParameters->Content.Height;

    if (Frame->PictureHeader.first_field != 0)                          // Remember Picture syntax
    {
        BufferState[CurrentDecodeBufferIndex].PictureSyntax     = (unsigned int)Param->PictureSyntax;
    }

    memset(Intensity, 0xff, sizeof(VC9_IntensityComp_t));
    Param->RndCtrl                              = Frame->PictureHeader.rndctrl;
    Param->NumeratorBFraction                   = Frame->PictureHeader.bfraction_numerator;
    Param->DenominatorBFraction                 = Frame->PictureHeader.bfraction_denominator;

    if (0 == Param->DenominatorBFraction)
    {
        SE_WARNING("DenominatorBFraction 0\n");
    }

    // Simple/Main profile flags
    Param->MultiresFlag                         = vc1StreamParams->SequenceHeader.multires;
    // BOOL   HalfWidthFlag; /* in [0..1] */
    // BOOL   HalfHeightFlag; /* in [0..1] */
    Param->SyncmarkerFlag                       = vc1StreamParams->SequenceHeader.syncmarker;
    Param->RangeredFlag                         = vc1StreamParams->SequenceHeader.rangered;
    Param->Maxbframes                           = vc1StreamParams->SequenceHeader.maxbframes;
    //Param->ForwardRangeredfrmFlag               = Frame->PictureHeader.rangered_frm_flag //of the forward reference picture.
    //                                       Shall be set to FALSE if it is not present in the bitstream, or if
    //                                       the current picture is a I/Bi picture */
    // Advanced profile settings here
    Param->PostprocFlag                         = vc1StreamParams->SequenceHeader.postprocflag;
    Param->PulldownFlag                         = vc1StreamParams->SequenceHeader.pulldown;
    Param->InterlaceFlag                        = vc1StreamParams->SequenceHeader.interlace;
    Param->PsfFlag                              = vc1StreamParams->SequenceHeader.psf;
    Param->TfcntrFlag                           = vc1StreamParams->SequenceHeader.tfcntrflag;
    StartCodes->SliceCount                      = Frame->SliceHeaderList.no_slice_headers;

    for (i = 0; i < StartCodes->SliceCount; i++)
    {
        StartCodes->SliceArray[i].SliceStartAddrCompressedBuffer_p      = (VC9_CompressedData_t)(CodedData + Frame->SliceHeaderList.slice_start_code[i]);
        StartCodes->SliceArray[i].SliceAddress                          = Frame->SliceHeaderList.slice_addr[i];
    }

    if (vc1StreamParams->EntryPointHeader.refdist_flag)
    {
        Param->RefDist                      = Frame->PictureHeader.refdist;
        Param->BackwardRefdist              = Frame->PictureHeader.backward_refdist;
    }
    else
    {
        Param->RefDist                      = 0;
        Param->BackwardRefdist              = 0;
    }

    Param->FramePictureLayerSize            = Frame->PictureHeader.picture_layer_size;
    Param->TffFlag                          = Frame->PictureHeader.tff;
    Param->SecondFieldFlag                  = Frame->PictureHeader.first_field  == 0 ? 1 : 0;
    Entry->LoopfilterFlag                   = vc1StreamParams->EntryPointHeader.loopfilter;
    Entry->FastuvmcFlag                     = vc1StreamParams->EntryPointHeader.fastuvmc;
    Entry->ExtendedmvFlag                   = vc1StreamParams->EntryPointHeader.extended_mv;
    Entry->Dquant                           = vc1StreamParams->EntryPointHeader.dquant;
    Entry->VstransformFlag                  = vc1StreamParams->EntryPointHeader.vstransform;
    Entry->OverlapFlag                      = vc1StreamParams->EntryPointHeader.overlap;
    Entry->Quantizer                        = vc1StreamParams->EntryPointHeader.quantizer;
    Entry->ExtendedDmvFlag                  = vc1StreamParams->EntryPointHeader.extended_dmv;
    Entry->PanScanFlag                      = vc1StreamParams->EntryPointHeader.panscan_flag;
    Entry->RefdistFlag                      = vc1StreamParams->EntryPointHeader.refdist_flag;

    //Param->CurrentPicOrderCount             = 0;                    // FIX for trick modes
    //Param->ForwardPicOrderCount             = 0;                    // FIX for trick modes
    //Param->BackwardPicOrderCount            = 0;                    // FIX for trick modes

    //
    // Fill out the reference frame list stuff
    //

    if (ParsedFrameParameters->NumberOfReferenceFrameLists != 0)
    {
        if (DecodeContext->ReferenceFrameList[0].EntryCount > 0)
        {
            i                                           = DecodeContext->ReferenceFrameList[0].EntryIndicies[0];
            ReferenceBuffer             = BufferState[i].Buffer;
            RefList->ForwardReferenceLuma_p             = (VC9_LumaAddress_t)Stream->GetDecodeBufferManager()->Luma(ReferenceBuffer, DecodeComponent);
            RefList->ForwardReferenceChroma_p           = (VC9_ChromaAddress_t)Stream->GetDecodeBufferManager()->Chroma(ReferenceBuffer, DecodeComponent);
            RefList->ForwardReferencePictureSyntax      = (VC9_PictureSyntax_t)BufferState[i].PictureSyntax;

            if (Frame->PictureHeader.mvmode == VC1_MV_MODE_INTENSITY_COMP)
            {
                SaveIntensityCompensationData(i, Intensity);
            }

            GetForwardIntensityCompensationData(i, Intensity);
        }

        if (DecodeContext->ReferenceFrameList[0].EntryCount > 1)
        {
            i                                           = DecodeContext->ReferenceFrameList[0].EntryIndicies[1];
            ReferenceBuffer             = BufferState[i].Buffer;
            RefList->BackwardReferenceLuma_p            = (VC9_LumaAddress_t)Stream->GetDecodeBufferManager()->Luma(ReferenceBuffer, DecodeComponent);
            RefList->BackwardReferenceChroma_p          = (VC9_ChromaAddress_t)Stream->GetDecodeBufferManager()->Chroma(ReferenceBuffer, DecodeComponent);
            RefList->BackwardReferenceMBStruct_p        = (VC9_MBStructureAddress_t)Stream->GetDecodeBufferManager()->ComponentBaseAddress(ReferenceBuffer, VideoMacroblockStructure);
            RefList->BackwardReferencePictureSyntax     = (VC9_PictureSyntax_t)BufferState[i].PictureSyntax;
            GetBackwardIntensityCompensationData(i, Intensity);
        }
    }

    //{{{  Dump picture indexes
    if (SE_IS_DEBUG_ON(group_decoder_video))
    {
        static unsigned int PictureNo  = 0;
        PictureNo++;

        if ((Param->IntensityComp.ForwTop_1 != 0xffffffff) || (Param->IntensityComp.ForwBot_1 != 0xffffffff) ||
            (Param->IntensityComp.ForwTop_2 != 0xffffffff) || (Param->IntensityComp.ForwBot_2 != 0xffffffff) ||
            (Param->IntensityComp.BackTop_1 != 0xffffffff) || (Param->IntensityComp.BackBot_1 != 0xffffffff) ||
            (Param->IntensityComp.BackTop_2 != 0xffffffff) || (Param->IntensityComp.BackBot_2 != 0xffffffff))
        {
            SE_DEBUG(group_decoder_video, "Codec: Decode %2d, Display %2d ", PictureNo - 1, ParsedFrameParameters->DisplayFrameIndex);

            switch (Param->PictureType)
            {
            case VC9_PICTURE_TYPE_P:                 SE_DEBUG(group_decoder_video, "P  ");            break;

            case VC9_PICTURE_TYPE_B:                 SE_DEBUG(group_decoder_video, "B  ");            break;

            case VC9_PICTURE_TYPE_I:                 SE_DEBUG(group_decoder_video, "I  ");            break;

            case VC9_PICTURE_TYPE_SKIP:              SE_DEBUG(group_decoder_video, "S  ");            break;

            case VC9_PICTURE_TYPE_BI:                SE_DEBUG(group_decoder_video, "BI ");            break;

            default/*VC9_PICTURE_TYPE_NONE*/:        SE_ERROR("Invalid Picture Type ");               break;
            }

            switch (Param->PictureSyntax)
            {
            case VC9_PICTURE_SYNTAX_PROGRESSIVE:      SE_DEBUG(group_decoder_video, "Progressive\n");   break;

            case VC9_PICTURE_SYNTAX_INTERLACED_FRAME: SE_DEBUG(group_decoder_video, "Interlaced Frame (%s)\n", Param->TffFlag ? "T" : "B");   break;

            case VC9_PICTURE_SYNTAX_INTERLACED_FIELD: SE_DEBUG(group_decoder_video, "Interlaced Field (%d)\n", Frame->PictureHeader.first_field);   break;
            }

            if (Param->IntensityComp.ForwTop_1 != 0xffffffff)
            {
                SE_DEBUG(group_decoder_video, "    : IntensityComp_ForwTop_1            = %08x\n", Param->IntensityComp.ForwTop_1);
            }

            if (Param->IntensityComp.ForwBot_1 != 0xffffffff)
            {
                SE_DEBUG(group_decoder_video, "    : IntensityComp_ForwBot_1            = %08x\n", Param->IntensityComp.ForwBot_1);
            }

            if (Param->IntensityComp.ForwTop_2 != 0xffffffff)
            {
                SE_DEBUG(group_decoder_video, "    : IntensityComp_ForwTop_2            = %08x\n", Param->IntensityComp.ForwTop_2);
            }

            if (Param->IntensityComp.ForwBot_2 != 0xffffffff)
            {
                SE_DEBUG(group_decoder_video, "    : IntensityComp_ForwBot_2            = %08x\n", Param->IntensityComp.ForwBot_2);
            }

            if (Param->IntensityComp.BackTop_1 != 0xffffffff)
            {
                SE_DEBUG(group_decoder_video, "    : IntensityComp_BackTop_1            = %08x\n", Param->IntensityComp.BackTop_1);
            }

            if (Param->IntensityComp.BackBot_1 != 0xffffffff)
            {
                SE_DEBUG(group_decoder_video, "    : IntensityComp_BackBot_1            = %08x\n", Param->IntensityComp.BackBot_1);
            }

            if (Param->IntensityComp.BackTop_2 != 0xffffffff)
            {
                SE_DEBUG(group_decoder_video, "    : IntensityComp_BackTop_2            = %08x\n", Param->IntensityComp.BackTop_2);
            }

            if (Param->IntensityComp.BackBot_2 != 0xffffffff)
            {
                SE_DEBUG(group_decoder_video, "    : IntensityComp_BackBot_2            = %08x\n", Param->IntensityComp.BackBot_2);
            }
        }
    }

    //}}}
    //
    // Fill out the actual command
    //
    memset(&Context->BaseContext.MMECommand, 0, sizeof(MME_Command_t));
    Context->BaseContext.MMECommand.CmdStatus.AdditionalInfoSize    = sizeof(VC9_CommandStatus_t);
    Context->BaseContext.MMECommand.CmdStatus.AdditionalInfo_p      = (MME_GenericParams_t)(&Context->DecodeStatus);
    Context->BaseContext.MMECommand.ParamSize                       = sizeof(VC9_TransformParam_t);
    Context->BaseContext.MMECommand.Param_p                         = (MME_GenericParams_t)(&Context->DecodeParameters);
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
CodecStatus_t   Codec_MmeVideoVc1_c::ValidateDecodeContext(CodecBaseDecodeContext_t *Context)
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

CodecStatus_t   Codec_MmeVideoVc1_c::DumpSetStreamParameters(void    *Parameters)
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

unsigned int StaticPicture;
CodecStatus_t   Codec_MmeVideoVc1_c::DumpDecodeParameters(void    *Parameters)
{
    unsigned int                        i;
    VC9_TransformParam_t               *FrameParams;
    VC9_DecodedBufferAddress_t         *Decode;
    VC9_EntryPointParam_t              *Entry;
    FrameParams     = (VC9_TransformParam_t *)Parameters;
    Decode          = &FrameParams->DecodedBufferAddress;
    Entry           = &FrameParams->EntryPoint;
    // VC9_IntensityComp_t *Intensity       = &FrameParams->IntensityComp;
    StaticPicture++;
    SE_VERBOSE(group_decoder_video, "Picture %d\n", StaticPicture - 1);
    SE_VERBOSE(group_decoder_video, "DDP -    Entry->LoopfilterFlag               %08x\n", Entry->LoopfilterFlag);
    SE_VERBOSE(group_decoder_video, "DDP -    Entry->FastuvmcFlag                 %08x\n", Entry->FastuvmcFlag);
    SE_VERBOSE(group_decoder_video, "DDP -    Entry->ExtendedmvFlag               %08x\n", Entry->ExtendedmvFlag);
    SE_VERBOSE(group_decoder_video, "DDP -    Entry->Dquant                       %08x\n", Entry->Dquant);
    SE_VERBOSE(group_decoder_video, "DDP -    Entry->VstransformFlag              %08x\n", Entry->VstransformFlag);
    SE_VERBOSE(group_decoder_video, "DDP -    Entry->OverlapFlag                  %08x\n", Entry->OverlapFlag);
    SE_VERBOSE(group_decoder_video, "DDP -    Entry->Quantizer                    %08x\n", Entry->Quantizer);
    SE_VERBOSE(group_decoder_video, "DDP -    Entry->ExtendedDmvFlag              %08x\n", Entry->ExtendedDmvFlag);
    SE_VERBOSE(group_decoder_video, "DDP -    Entry->PanScanFlag                  %08x\n", Entry->PanScanFlag);
    SE_VERBOSE(group_decoder_video, "DDP -    Entry->RefdistFlag                  %08x\n", Entry->RefdistFlag);
    SE_VERBOSE(group_decoder_video, "DDP -    DecodedBufferAddress\n");
    SE_VERBOSE(group_decoder_video, "DDP -        Luma_p                             = %p\n", Decode->Luma_p);
    SE_VERBOSE(group_decoder_video, "DDP -        Chroma_p                           = %p\n", Decode->Chroma_p);
    SE_VERBOSE(group_decoder_video, "DDP -        MBStruct_p                         = %p\n", Decode->MBStruct_p);
#if (VC9_MME_VERSION >= 19)
    SE_VERBOSE(group_decoder_video, "DDP -        DisplayLuma_p                      = %p\n", Decode->DisplayLuma_p);
    SE_VERBOSE(group_decoder_video, "DDP -        DisplayChroma_p                    = %p\n", Decode->DisplayChroma_p);
#endif
    SE_VERBOSE(group_decoder_video, "DDP -        LumaDecimated_p                    = %p\n", Decode->LumaDecimated_p);
    SE_VERBOSE(group_decoder_video, "DDP -        ChromaDecimated_p                  = %p\n", Decode->ChromaDecimated_p);
    SE_VERBOSE(group_decoder_video, "DDP -    CircularBufferBeginAddr_p          = %p\n", FrameParams->CircularBufferBeginAddr_p);
    SE_VERBOSE(group_decoder_video, "DDP -    CircularBufferEndAddr_p            = %p\n", FrameParams->CircularBufferEndAddr_p);
    SE_VERBOSE(group_decoder_video, "DDP -    PictureStartAddrCompressedBuffer_p = %p\n", FrameParams->PictureStartAddrCompressedBuffer_p);
    SE_VERBOSE(group_decoder_video, "DDP -    PictureStopAddrCompressedBuffer_p  = %p\n", FrameParams->PictureStopAddrCompressedBuffer_p);
    SE_VERBOSE(group_decoder_video, "DDP -    ReferencePicList\n");
    SE_VERBOSE(group_decoder_video, "DDP -        BackwardReferenceLuma_p            = %p\n", FrameParams->RefPicListAddress.BackwardReferenceLuma_p);
    SE_VERBOSE(group_decoder_video, "DDP -        BackwardReferenceChroma_p          = %p\n", FrameParams->RefPicListAddress.BackwardReferenceChroma_p);
    SE_VERBOSE(group_decoder_video, "DDP -        BackwardReferenceMBStruct_p        = %p\n", FrameParams->RefPicListAddress.BackwardReferenceMBStruct_p);
    SE_VERBOSE(group_decoder_video, "DDP -        ForwardReferenceLuma_p             = %p\n", FrameParams->RefPicListAddress.ForwardReferenceLuma_p);
    SE_VERBOSE(group_decoder_video, "DDP -        ForwardReferenceChroma_p           = %p\n", FrameParams->RefPicListAddress.ForwardReferenceChroma_p);
    SE_VERBOSE(group_decoder_video, "DDP -        BackwardReferencePictureSyntax     = %08x\n", FrameParams->RefPicListAddress.BackwardReferencePictureSyntax);
    SE_VERBOSE(group_decoder_video, "DDP -        ForwardReferencePictureSyntax      = %08x\n", FrameParams->RefPicListAddress.ForwardReferencePictureSyntax);
    SE_VERBOSE(group_decoder_video, "DDP -    MainAuxEnable                      = %d\n", FrameParams->MainAuxEnable);
    SE_VERBOSE(group_decoder_video, "DDP -    HorizontalDecimationFactor         = %d\n", FrameParams->HorizontalDecimationFactor);
    SE_VERBOSE(group_decoder_video, "DDP -    VerticalDecimationFactor           = %d\n", FrameParams->VerticalDecimationFactor);
    SE_VERBOSE(group_decoder_video, "DDP -    DecodingMode                       = %d\n", FrameParams->DecodingMode);
    SE_VERBOSE(group_decoder_video, "DDP -    AebrFlag                           = %d\n", FrameParams->AebrFlag);
    SE_VERBOSE(group_decoder_video, "DDP -    Profile                            = %d\n", FrameParams->Profile);
    SE_VERBOSE(group_decoder_video, "DDP -    PictureSyntax                      = %d\n", FrameParams->PictureSyntax);
    SE_VERBOSE(group_decoder_video, "DDP -    PictureType                        = %d\n", FrameParams->PictureType);
    SE_VERBOSE(group_decoder_video, "DDP -    FinterpFlag                        = %d\n", FrameParams->FinterpFlag);
    SE_VERBOSE(group_decoder_video, "DDP -    FrameHorizSize                     = %d\n", FrameParams->FrameHorizSize);
    SE_VERBOSE(group_decoder_video, "DDP -    FrameVertSize                      = %d\n", FrameParams->FrameVertSize);
    SE_VERBOSE(group_decoder_video, "DDP -    IntensityComp_ForwTop_1            = %d\n", FrameParams->IntensityComp.ForwTop_1);
    SE_VERBOSE(group_decoder_video, "DDP -    IntensityComp_ForwTop_2            = %d\n", FrameParams->IntensityComp.ForwTop_2);
    SE_VERBOSE(group_decoder_video, "DDP -    IntensityComp_ForwBot_1            = %d\n", FrameParams->IntensityComp.ForwBot_1);
    SE_VERBOSE(group_decoder_video, "DDP -    IntensityComp_ForwBot_2            = %d\n", FrameParams->IntensityComp.ForwBot_2);
    SE_VERBOSE(group_decoder_video, "DDP -    IntensityComp_BackTop_1            = %d\n", FrameParams->IntensityComp.BackTop_1);
    SE_VERBOSE(group_decoder_video, "DDP -    IntensityComp_BackTop_2            = %d\n", FrameParams->IntensityComp.BackTop_2);
    SE_VERBOSE(group_decoder_video, "DDP -    IntensityComp_BackBot_1            = %d\n", FrameParams->IntensityComp.BackBot_1);
    SE_VERBOSE(group_decoder_video, "DDP -    IntensityComp_BackBot_2            = %d\n", FrameParams->IntensityComp.BackBot_2);
    SE_VERBOSE(group_decoder_video, "DDP -    RndCtrl                            = %d\n", FrameParams->RndCtrl);
    SE_VERBOSE(group_decoder_video, "DDP -    NumeratorBFraction                 = %d\n", FrameParams->NumeratorBFraction);
    SE_VERBOSE(group_decoder_video, "DDP -    DenominatorBFraction               = %d\n", FrameParams->DenominatorBFraction);
    SE_VERBOSE(group_decoder_video, "DDP -    PostprocFlag                       = %d\n", FrameParams->PostprocFlag);
    SE_VERBOSE(group_decoder_video, "DDP -    PulldownFlag                       = %d\n", FrameParams->PulldownFlag);
    SE_VERBOSE(group_decoder_video, "DDP -    InterlaceFlag                      = %d\n", FrameParams->InterlaceFlag);
    SE_VERBOSE(group_decoder_video, "DDP -    TfcntrFlag                         = %d\n", FrameParams->TfcntrFlag);
    SE_VERBOSE(group_decoder_video, "DDP -    SliceCount                         = %d\n", FrameParams->StartCodes.SliceCount);

    for (i = 0 ; i < FrameParams->StartCodes.SliceCount ; i++)
    {
        SE_VERBOSE(group_decoder_video, "DDP -   SLICE %d\n", i + 1);
        SE_VERBOSE(group_decoder_video, "DDP -   SliceStartAddrCompressedBuffer_p = %p\n", FrameParams->StartCodes.SliceArray[i].SliceStartAddrCompressedBuffer_p);
        SE_VERBOSE(group_decoder_video, "DDP -   SliceAddress = %d\n", FrameParams->StartCodes.SliceArray[i].SliceAddress);
    }

    SE_VERBOSE(group_decoder_video, "DDP -   BackwardRefdist                    = %d\n", FrameParams->BackwardRefdist);
    SE_VERBOSE(group_decoder_video, "DDP -   FramePictureLayerSize              = %d\n", FrameParams->FramePictureLayerSize);
    SE_VERBOSE(group_decoder_video, "DDP -   TffFlag                            = %d\n", FrameParams->TffFlag);
    SE_VERBOSE(group_decoder_video, "DDP -   SecondFieldFlag                    = %d\n", FrameParams->SecondFieldFlag);
    SE_VERBOSE(group_decoder_video, "DDP -   RefDist                            = %d\n", FrameParams->RefDist);
    return CodecNoError;
}
//}}}

//{{{  SaveIntensityCompenationData
//
//      This function must be mutex-locked by caller
//
void Codec_MmeVideoVc1_c::SaveIntensityCompensationData(unsigned int            RefBufferIndex,
                                                        VC9_IntensityComp_t    *Intensity)
{
    Vc1FrameParameters_t               *Frame                   = (Vc1FrameParameters_t *)ParsedFrameParameters->FrameParameterStructure;
    CodecIntensityCompensation_t       *RefIntensityComp;
    CodecIntensityCompensation_t       *OurIntensityComp;
    OS_AssertMutexHeld(&Lock);
    RefIntensityComp        = &BufferState[RefBufferIndex].AppliedIntensityCompensation;
    OurIntensityComp        = &BufferState[CurrentDecodeBufferIndex].AppliedIntensityCompensation;

    if (Frame->PictureHeader.first_field == 1)
        // For first field intensity compensation is applied to the reference frame
    {
        if (Frame->PictureHeader.intensity_comp_field != VC1_INTENSITY_COMP_BOTTOM)
        {
            if (RefIntensityComp->Top1 == VC9_NO_INTENSITY_COMP)
            {
                RefIntensityComp->Top1      = Frame->PictureHeader.intensity_comp_top;
            }
            else
            {
                RefIntensityComp->Top2      = Frame->PictureHeader.intensity_comp_top;
            }
        }

        if (Frame->PictureHeader.intensity_comp_field != VC1_INTENSITY_COMP_TOP)
        {
            if (RefIntensityComp->Bottom1 == VC9_NO_INTENSITY_COMP)
            {
                RefIntensityComp->Bottom1   = Frame->PictureHeader.intensity_comp_bottom;
            }
            else
            {
                RefIntensityComp->Bottom2   = Frame->PictureHeader.intensity_comp_bottom;
            }
        }
    }
    else
        // For second field part is applied to the first field and part to the reference frame
    {
        if (Frame->PictureHeader.tff == 1)
        {
            if (Frame->PictureHeader.intensity_comp_field != VC1_INTENSITY_COMP_BOTTOM)
            {
                OurIntensityComp->Top1          = Frame->PictureHeader.intensity_comp_top;
            }

            if (Frame->PictureHeader.intensity_comp_field != VC1_INTENSITY_COMP_TOP)
            {
                if (RefIntensityComp->Bottom1 == VC9_NO_INTENSITY_COMP)
                {
                    RefIntensityComp->Bottom1   = Frame->PictureHeader.intensity_comp_bottom;
                }
                else
                {
                    RefIntensityComp->Bottom2   = Frame->PictureHeader.intensity_comp_bottom;
                }
            }
        }
        else
        {
            if (Frame->PictureHeader.intensity_comp_field != VC1_INTENSITY_COMP_TOP)
            {
                RefIntensityComp->Bottom1       = Frame->PictureHeader.intensity_comp_bottom;
            }

            if (Frame->PictureHeader.intensity_comp_field != VC1_INTENSITY_COMP_BOTTOM)
            {
                if (RefIntensityComp->Top1 == VC9_NO_INTENSITY_COMP)
                {
                    RefIntensityComp->Top1      = Frame->PictureHeader.intensity_comp_top;
                }
                else
                {
                    RefIntensityComp->Top2      = Frame->PictureHeader.intensity_comp_top;
                }
            }
        }
    }
}
//}}}
//{{{  GetForwardIntensityCompenationData
//
//      This function must be mutex-locked by caller
//
void Codec_MmeVideoVc1_c::GetForwardIntensityCompensationData(unsigned int            RefBufferIndex,
                                                              VC9_IntensityComp_t    *Intensity)
{
    Vc1FrameParameters_t               *Frame                   = (Vc1FrameParameters_t *)ParsedFrameParameters->FrameParameterStructure;
    CodecIntensityCompensation_t       *RefIntensityComp;
    CodecIntensityCompensation_t       *OurIntensityComp;
    OS_AssertMutexHeld(&Lock);
    RefIntensityComp        = &BufferState[RefBufferIndex].AppliedIntensityCompensation;
    OurIntensityComp        = &BufferState[CurrentDecodeBufferIndex].AppliedIntensityCompensation;

    if ((Frame->PictureHeader.ptype != VC1_PICTURE_CODING_TYPE_P) && (Frame->PictureHeader.ptype != VC1_PICTURE_CODING_TYPE_B))
    {
        return;
    }

    if (Frame->PictureHeader.first_field == 1) //|| (Frame->PictureHeader.ptype == VC1_PICTURE_CODING_TYPE_B))
        // For first field intensity compensation is obtained from that already used for the reference frame
    {
        Intensity->ForwTop_1            = RefIntensityComp->Top1;
        Intensity->ForwTop_2            = RefIntensityComp->Top2;
        Intensity->ForwBot_1            = RefIntensityComp->Bottom1;
        Intensity->ForwBot_2            = RefIntensityComp->Bottom2;
    }
    else if (Frame->PictureHeader.ptype != VC1_PICTURE_CODING_TYPE_B)
        // Second field of B frame uses first field as reference so cannot have intensity compensation applied
        // For second field of P frame part is obtained from the first field and part from the reference frame
    {
        if (Frame->PictureHeader.tff == 1)
        {
            Intensity->ForwTop_1        = OurIntensityComp->Top1;
            Intensity->ForwTop_2        = OurIntensityComp->Top2;
            Intensity->ForwBot_1        = RefIntensityComp->Bottom1;
            Intensity->ForwBot_2        = RefIntensityComp->Bottom2;
        }
        else
        {
            Intensity->ForwTop_1        = RefIntensityComp->Top1;
            Intensity->ForwTop_2        = RefIntensityComp->Top2;
            Intensity->ForwBot_1        = OurIntensityComp->Bottom1;
            Intensity->ForwBot_2        = OurIntensityComp->Bottom2;
        }
    }
}
//}}}
//{{{  GetBackwardIntensityCompenationData
//
//      This function must be mutex-locked by caller
//
void Codec_MmeVideoVc1_c::GetBackwardIntensityCompensationData(unsigned int            RefBufferIndex,
                                                               VC9_IntensityComp_t    *Intensity)
{
    CodecIntensityCompensation_t       *RefIntensityComp;
    OS_AssertMutexHeld(&Lock);
    RefIntensityComp                    = &BufferState[RefBufferIndex].AppliedIntensityCompensation;
    Intensity->BackTop_1                = RefIntensityComp->Top1;
    Intensity->BackTop_2                = RefIntensityComp->Top2;
    Intensity->BackBot_1                = RefIntensityComp->Bottom1;
    Intensity->BackBot_2                = RefIntensityComp->Bottom2;
}
//}}}
//{{{  CheckCodecReturnParameters
// Convert the return code into human readable form.
static const char *LookupError(unsigned int Error)
{
#define E(e) case e: return #e

    switch (Error)
    {
        E(VC9_DECODER_ERROR_MB_OVERFLOW);
        E(VC9_DECODER_ERROR_RECOVERED);
        E(VC9_DECODER_ERROR_NOT_RECOVERED);
        E(VC9_DECODER_ERROR_TASK_TIMEOUT);

    default: return "VC9_DECODER_UNKNOWN_ERROR";
    }

#undef E
}
CodecStatus_t   Codec_MmeVideoVc1_c::CheckCodecReturnParameters(CodecBaseDecodeContext_t *Context)
{
    MME_Command_t       *MMECommand       = (MME_Command_t *)(&Context->MMECommand);
    MME_CommandStatus_t *CmdStatus        = (MME_CommandStatus_t *)(&MMECommand->CmdStatus);
    VC9_CommandStatus_t *AdditionalInfo_p = (VC9_CommandStatus_t *)CmdStatus->AdditionalInfo_p;

    if (AdditionalInfo_p != NULL)
    {
        if (AdditionalInfo_p->ErrorCode != VC9_DECODER_NO_ERROR)
        {
            // Set the decode quality to worst because there is a decoding error and there is no emulation.
            // This is done to allow flexibity for not displaying the corrupt output, using player policy, after encountering the decoding error.
            Context->DecodeQuality = 0;
            SE_INFO(group_decoder_video, "%s  0x%x\n", LookupError(AdditionalInfo_p->ErrorCode), AdditionalInfo_p->ErrorCode);

            switch (AdditionalInfo_p->ErrorCode)
            {
            case VC9_DECODER_ERROR_MB_OVERFLOW:
                Stream->Statistics().FrameDecodeMBOverflowError++;
                break;

            case VC9_DECODER_ERROR_RECOVERED:
                Stream->Statistics().FrameDecodeRecoveredError++;
                break;

            case VC9_DECODER_ERROR_NOT_RECOVERED:
                Stream->Statistics().FrameDecodeNotRecoveredError++;
                break;

            case VC9_DECODER_ERROR_TASK_TIMEOUT:
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
