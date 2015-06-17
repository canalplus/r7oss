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

Source file name : codec_mme_video_vc1.cpp
Author :           Mark C

Implementation of the VC-1 video codec class for player 2.


Date        Modification                                    Name
----        ------------                                    --------
06-Mar-07   Created                                         Mark C

************************************************************************/

// /////////////////////////////////////////////////////////////////////
//
//      Include any component headers

#include "codec_mme_video_vc1.h"
#include "vc1.h"

// /////////////////////////////////////////////////////////////////////////
//
// Locally defined constants
//


// /////////////////////////////////////////////////////////////////////////
//
// Locally defined structures
//

typedef struct Vc1CodecStreamParameterContext_s
{
    CodecBaseStreamParameterContext_t   BaseContext;
    //VC9_SetGlobalParamSequence_t      StreamParameters;
} Vc1CodecStreamParameterContext_t;

#define BUFFER_VC1_CODEC_STREAM_PARAMETER_CONTEXT               "Vc1CodecStreamParameterContext"
#define BUFFER_VC1_CODEC_STREAM_PARAMETER_CONTEXT_TYPE  {BUFFER_VC1_CODEC_STREAM_PARAMETER_CONTEXT, BufferDataTypeBase, AllocateFromOSMemory, 32, 0, true, true, sizeof(Vc1CodecStreamParameterContext_t)}

static BufferDataDescriptor_t           Vc1CodecStreamParameterContextDescriptor = BUFFER_VC1_CODEC_STREAM_PARAMETER_CONTEXT_TYPE;

#define BUFFER_VC1_MBSTRUCT             "Vc1MbStruct"
#define BUFFER_VC1_MBSTRUCT_TYPE        {BUFFER_VC1_MBSTRUCT, BufferDataTypeBase, AllocateFromNamedDeviceMemory, 32, 0, false, true, NOT_SPECIFIED}

static BufferDataDescriptor_t           Vc1MbStructInitialDescriptor = BUFFER_VC1_MBSTRUCT_TYPE;


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



//{{{  Constructor
// /////////////////////////////////////////////////////////////////////////
//
//      Cosntructor function, fills in the codec specific parameter values
//

Codec_MmeVideoVc1_c::Codec_MmeVideoVc1_c( void )
{
        //printf( "Codec_MmeVideoVc1_c::Codec_MmeVideoVc1_c - decode context coming from system memory - needs fixing inbox\n" );

    Configuration.CodecName                             = "Vc1 video";

    Configuration.DecodeOutputFormat                    = FormatVideo420_MacroBlock;

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

    Vc1MbStructPool                                     = NULL;

    // We do not need the coded data after decode is complete
    Configuration.ShrinkCodedDataBuffersAfterDecode     = true;

    // Some random trick mode parameters

    Configuration.TrickModeParameters.EmpiricalMaximumDecodeFrameRateShortIntegration   = 60;
    Configuration.TrickModeParameters.EmpiricalMaximumDecodeFrameRateLongIntegration    = 60;

    Configuration.TrickModeParameters.SubstandardDecodeSupported        = false;
    Configuration.TrickModeParameters.SubstandardDecodeRateIncrease     = 1;

    Configuration.TrickModeParameters.DefaultGroupSize                  = 12;
    Configuration.TrickModeParameters.DefaultGroupReferenceFrameCount   = 4;

    Reset();
}
//}}}
//{{{  Destructor
// /////////////////////////////////////////////////////////////////////////
//
//      Destructor function, ensures a full halt and reset 
//      are executed for all levels of the class.
//

Codec_MmeVideoVc1_c::~Codec_MmeVideoVc1_c( void )
{
    Halt();
    Reset();
}
//}}}
//{{{  Reset
// /////////////////////////////////////////////////////////////////////////
//
//      Reset function for VC1 specific members.
//      
//

CodecStatus_t   Codec_MmeVideoVc1_c::Reset( void )
{
    if( Vc1MbStructPool != NULL )
    {
        BufferManager->DestroyPool( Vc1MbStructPool );
        Vc1MbStructPool = NULL;
    }

    return Codec_MmeVideo_c::Reset();
}
//}}}

//{{{  HandleCapabilities
// /////////////////////////////////////////////////////////////////////////
//
//      Function to deal with the returned capabilities
//      structure for an vc1 mme transformer.
//
CodecStatus_t   Codec_MmeVideoVc1_c::HandleCapabilities( void )
{
    CODEC_TRACE("MME Transformer '%s' capabilities are :-\n", VC9_MME_TRANSFORMER_NAME);
    CODEC_TRACE("    Maximum field decoding latency %d\n", Vc1TransformCapability.MaximumFieldDecodingLatency90KHz);

    return CodecNoError;
}
//}}}
//{{{  RegisterOutputBufferRing
///////////////////////////////////////////////////////////////////////////
///
///     Allocate the VC1 macroblocks structure.
///
///     Due to the structure of the shared super-class, most codecs allocate
///     the majority of the resources when the player supplies it with an
///     output buffer.
///
CodecStatus_t   Codec_MmeVideoVc1_c::RegisterOutputBufferRing(   Ring_t                    Ring )
{
    CodecStatus_t Status;

    Status = Codec_MmeVideo_c::RegisterOutputBufferRing( Ring );
    if( Status != CodecNoError )
        return Status;

    //
    // Create the buffer type for the macroblocks structure
    //
    // Find the type if it already exists
    //

    Status      = BufferManager->FindBufferDataType( BUFFER_VC1_MBSTRUCT, &Vc1MbStructType );
    if( Status != BufferNoError )
    {
        //
        // It didn't already exist - create it
        //

        Status  = BufferManager->CreateBufferDataType( &Vc1MbStructInitialDescriptor, &Vc1MbStructType );
        if( Status != BufferNoError )
        {
                CODEC_ERROR ("Failed to create the %s buffer type.\n", Vc1MbStructInitialDescriptor.TypeName );
                return Status;
        }
    }

    //
    // Now create the pool
    //

    unsigned int                    Size;
    Size = (1920 >> 4) * (1088 >> 4) * 16;  // Allocate the maximum for now. FIX ME & save space on smaller streams.
    Status          = BufferManager->CreatePool( &Vc1MbStructPool, Vc1MbStructType, DecodeBufferCount, Size,
                                                 NULL, NULL, Configuration.AncillaryMemoryPartitionName );
    if( Status != BufferNoError )
    {
        CODEC_ERROR ("Failed to create macroblock structure pool.\n" );
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

CodecStatus_t   Codec_MmeVideoVc1_c::FillOutTransformerInitializationParameters( void )
{

    //
    // Fillout the command parameters
    //

    // Vc1InitializationParameters.InputBufferBegin     = 0x00000000;
    // Vc1InitializationParameters.InputBufferEnd       = 0xffffffff;

    CODEC_TRACE("FillOutTransformerInitializationParameters\n");

    //
    // Fillout the actual command
    //

    MMEInitializationParameters.TransformerInitParamsSize       = sizeof(VC9_InitTransformerParam_fmw_t);
    MMEInitializationParameters.TransformerInitParams_p         = (MME_GenericParams_t)(&Vc1InitializationParameters);

//

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

CodecStatus_t   Codec_MmeVideoVc1_c::FillOutSetStreamParametersCommand( void )
{
    //report (severity_info, "Codec_MmeVideoVc1_c::FillOutSetStreamParametersCommand\n");
    //
    // Fillout the actual command
    //

    // memset( &Context->BaseContext.MMECommand, 0x00, sizeof(MME_Command_t) );

    // Context->BaseContext.MMECommand.CmdStatus.AdditionalInfoSize     = 0;
    // Context->BaseContext.MMECommand.CmdStatus.AdditionalInfo_p               = NULL;
    // Context->BaseContext.MMECommand.ParamSize                                = sizeof(VC1_SetGlobalParamSequence_t);
    // Context->BaseContext.MMECommand.Param_p                          = (MME_GenericParams_t)(&Context->StreamParameters);

//

    StreamParameterContext->MMECommand.CmdStatus.AdditionalInfoSize    = sizeof(VC9_CommandStatus_t);
    StreamParameterContext->MMECommand.CmdStatus.AdditionalInfo_p      = (MME_GenericParams_t)&hack;

    return CodecNoError;
}
//}}}
//{{{  FillOutDecodeCommand
// /////////////////////////////////////////////////////////////////////////
//
//      Function to fill out the decode parameters
//      structure for an vc1 mme transformer.
//

CodecStatus_t   Codec_MmeVideoVc1_c::FillOutDecodeCommand(       void )
{
    Vc1CodecDecodeContext_t         *Context        = (Vc1CodecDecodeContext_t *)DecodeContext;
    Vc1FrameParameters_t            *Frame          = (Vc1FrameParameters_t *)ParsedFrameParameters->FrameParameterStructure;
    Vc1StreamParameters_t           *Stream         = (Vc1StreamParameters_t *)ParsedFrameParameters->StreamParameterStructure;

    VC9_TransformParam_t            *Param;
    VC9_DecodedBufferAddress_t      *Decode;
    VC9_RefPicListAddress_t         *RefList;
    VC9_EntryPointParam_t           *Entry;
    VC9_StartCodesParam_t           *StartCodes;
    VC9_IntensityComp_t             *Intensity;
    CodecStatus_t                   Status;
    unsigned int                    i;

    //
    // For vc1 we do not do slice decodes.
    //

    KnownLastSliceInFieldFrame                      = true;

    //
    // Fillout the straight forward command parameters
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
    if (Stream->SequenceHeader.profile == VC1_ADVANCED_PROFILE)
        Param->PictureStartAddrCompressedBuffer_p   = (VC9_CompressedData_t)(CodedData + 4);
    else
        Param->PictureStartAddrCompressedBuffer_p   = (VC9_CompressedData_t)CodedData;
    Param->PictureStopAddrCompressedBuffer_p    = (VC9_CompressedData_t)(CodedData + CodedDataLength + 32);

    // Two lines below changed in line with modified firmware for skipped pictures
    //Param->CircularBufferBeginAddr_p            = (VC9_CompressedData_t)0x00000000;
    //Param->CircularBufferEndAddr_p              = (VC9_CompressedData_t)0xffffffff;
    Param->CircularBufferBeginAddr_p            = Param->PictureStartAddrCompressedBuffer_p;
    Param->CircularBufferEndAddr_p              = Param->PictureStopAddrCompressedBuffer_p;

    Decode->Luma_p                              = (VC9_LumaAddress_t)BufferState[CurrentDecodeBufferIndex].BufferLumaPointer;
    Decode->Chroma_p                            = (VC9_ChromaAddress_t)BufferState[CurrentDecodeBufferIndex].BufferChromaPointer;
    // cjt

    if (Player->PolicyValue( Playback, this->Stream, PolicyDecimateDecoderOutput) != PolicyValueDecimateDecoderOutputDisabled)
    {
        Decode->LumaDecimated_p                 = (VC9_LumaAddress_t)BufferState[CurrentDecodeBufferIndex].DecimatedLumaPointer;
        Decode->ChromaDecimated_p               = (VC9_ChromaAddress_t)BufferState[CurrentDecodeBufferIndex].DecimatedChromaPointer;
    }
    else
    {
        Decode->LumaDecimated_p                 = NULL;
        Decode->ChromaDecimated_p               = NULL;
    }


    if (Frame->PictureHeader.first_field != 0)
    {
        // Get the macroblock structure buffer
        Buffer_t                Vc1MbStructBuffer;
        unsigned int            Size;

        Size = (1920 >> 4) * (1088 >> 4) * 16;  // Allocate the maximum for now. FIX ME & save space on smaller streams.

        Status  = Vc1MbStructPool->GetBuffer (&Vc1MbStructBuffer, Size);
        if( Status != BufferNoError )
        {
            CODEC_ERROR ("Failed to get macroblock structure buffer.\n" );
            return Status;
        }

        Vc1MbStructBuffer->ObtainDataReference (NULL, NULL, (void **)&Decode->MBStruct_p, PhysicalAddress);

        CurrentDecodeBuffer->AttachBuffer (Vc1MbStructBuffer);          // Attach to decode buffer (so it will be freed at the same time)
        Vc1MbStructBuffer->DecrementReferenceCount();                   // and release ownership of the buffer to the decode buffer

        // Remember the MBStruct pointer in case we have a second field to follow
        BufferState[CurrentDecodeBufferIndex].BufferMacroblockStructurePointer  = (unsigned char*)Decode->MBStruct_p;

        // Initialise the intensity compensation information (0xffffffff is no intensity com and is an invalid value)
        memset (&BufferState[CurrentDecodeBufferIndex].AppliedIntensityCompensation, 0xff, sizeof(CodecIntensityCompensation_t));
    }
    else
        Decode->MBStruct_p                      = (VC9_MBStructureAddress_t)BufferState[CurrentDecodeBufferIndex].BufferMacroblockStructurePointer;

    //{{{  Attach range mapping info if required
    if (1)
    {
        bool                       RangeMapLumaPresent          = false;
        unsigned int               RangeMapLuma                 = 0;
        bool                       RangeMapChromaPresent        = false;
        unsigned int               RangeMapChroma               = 0;

        if (Stream->SequenceHeader.profile == VC1_ADVANCED_PROFILE)
        {
            if (Stream->EntryPointHeader.range_mapy_flag == 1)        // Y[n] = CLIP((((Y[n]-128)*(RANGE_MAP_Y+9)+4)>>3)+128);
            {
                RangeMapLumaPresent         = true;
                RangeMapLuma                = Stream->EntryPointHeader.range_mapy;
            }
            if (Stream->EntryPointHeader.range_mapuv_flag == 1)
            {
                RangeMapChromaPresent       = true;
                RangeMapChroma              = Stream->EntryPointHeader.range_mapy;
            }
        }
        else if (Frame->PictureHeader.rangeredfrm == 1)   // implies that sequence->rangered == 1
        {
            RangeMapLumaPresent             = true;
            RangeMapLuma                    = 7;    // Y[n] = CLIP((Y[n]-128)*2+128);
            RangeMapChromaPresent           = true;
            RangeMapChroma                  = 7;    // so Luma and Chroma values should be set to 7 in above formula

        }
    #if 0
        report (severity_info, "%s: Luma (%d, %d), Chroma %d, %d)\n", __FUNCTION__,
                RangeMapLumaPresent, RangeMapLuma, RangeMapChromaPresent, RangeMapChroma);
    #endif
        if ((RangeMapLumaPresent || RangeMapChromaPresent) && (PostProcessControlBufferPool != NULL))
        {
            Buffer_t                            PostProcessBuffer;
            VideoPostProcessingControl_t*       PPControl;

            Status  = PostProcessControlBufferPool->GetBuffer (&PostProcessBuffer);
            if (Status != BufferNoError)
            {
                report( severity_error, "Codec_MmeVideoVc1_c::FillOutDecodeCommand - Failed to get post process control structure buffer.\n" );
                return Status;
            }
            PostProcessBuffer->ObtainDataReference (NULL, NULL, (void**)&PPControl);

            PPControl->RangeMapLumaPresent      = RangeMapLumaPresent;
            PPControl->RangeMapLuma             = RangeMapLuma;
            PPControl->RangeMapChromaPresent    = RangeMapChromaPresent;
            PPControl->RangeMapChroma           = RangeMapChroma;

            CurrentDecodeBuffer->AttachBuffer (PostProcessBuffer);      // Attach to decode buffer (so it will be freed at the same time)
            PostProcessBuffer->DecrementReferenceCount();               // and release ownership of the buffer to the decode buffer
        }
    }
    //}}}

    switch (Player->PolicyValue (Playback, this->Stream, PolicyDecimateDecoderOutput))
    {   
        case PolicyValueDecimateDecoderOutputDisabled:
        {
            // Normal Case
            Param->MainAuxEnable                = VC9_MAINOUT_EN;
            Param->HorizontalDecimationFactor   = VC9_HDEC_1;
            Param->VerticalDecimationFactor     = VC9_VDEC_1;
            break;
        }
        case PolicyValueDecimateDecoderOutputHalf:
        {
            Param->MainAuxEnable                = VC9_AUX_MAIN_OUT_EN;
            Param->HorizontalDecimationFactor   = VC9_HDEC_ADVANCED_2;
            if ((VC9_PictureSyntax_t)Frame->PictureHeader.fcm == VC9_PICTURE_SYNTAX_PROGRESSIVE)
                Param->VerticalDecimationFactor = VC9_VDEC_ADVANCED_2_PROG;
            else
                Param->VerticalDecimationFactor = VC9_VDEC_ADVANCED_2_INT;
            break;
        }
        case PolicyValueDecimateDecoderOutputQuarter:
        {
            Param->MainAuxEnable                = VC9_AUX_MAIN_OUT_EN;
            Param->HorizontalDecimationFactor   = VC9_HDEC_ADVANCED_4;
            Param->VerticalDecimationFactor     = VC9_VDEC_ADVANCED_2_INT;
            break;
        }
    }

    //Param->DecodingMode                         = VC9_NORMAL_DECODE_WITHOUT_ERROR_RECOVERY;
    Param->DecodingMode                         = VC9_NORMAL_DECODE;
    Param->AdditionalFlags                      = 0;
    switch (Stream->SequenceHeader.profile)
    {
        case VC1_SIMPLE_PROFILE:
            Param->AebrFlag                     = 0;
            Param->Profile                      = VC9_PROFILE_SIMPLE;
            break;
        case VC1_MAIN_PROFILE:
            Param->AebrFlag                     = 0;
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
    Param->FinterpFlag                          = Stream->SequenceHeader.finterpflag;
    Param->FrameHorizSize                       = ParsedVideoParameters->Content.Width;
    Param->FrameVertSize                        = ParsedVideoParameters->Content.Height;

    if (Frame->PictureHeader.first_field != 0)                          // Remember Picture syntax
        BufferState[CurrentDecodeBufferIndex].PictureSyntax     = (unsigned int)Param->PictureSyntax;

    memset (Intensity, 0xff, sizeof(VC9_IntensityComp_t) );

    Param->RndCtrl                              = Frame->PictureHeader.rndctrl;
    Param->NumeratorBFraction                   = Frame->PictureHeader.bfraction_numerator;
    Param->DenominatorBFraction                 = Frame->PictureHeader.bfraction_denominator;

    // Simple/Main profile flags
    Param->MultiresFlag                         = Stream->SequenceHeader.multires;
    // BOOL   HalfWidthFlag; /* in [0..1] */
    // BOOL   HalfHeightFlag; /* in [0..1] */
    Param->SyncmarkerFlag                       = Stream->SequenceHeader.syncmarker;
    Param->RangeredFlag                         = Stream->SequenceHeader.rangered;
    Param->Maxbframes                           = Stream->SequenceHeader.maxbframes;
    //Param->ForwardRangeredfrmFlag               = Frame->PictureHeader.rangered_frm_flag //of the forward reference picture.
    //                                       Shall be set to FALSE if it is not present in the bitstream, or if
    //                                       the current picture is a I/Bi picture */

    // Advanced profile settings here

    Param->PostprocFlag                         = Stream->SequenceHeader.postprocflag;
    Param->PulldownFlag                         = Stream->SequenceHeader.pulldown;
    Param->InterlaceFlag                        = Stream->SequenceHeader.interlace;
    Param->PsfFlag                              = Stream->SequenceHeader.psf;
    Param->TfcntrFlag                           = Stream->SequenceHeader.tfcntrflag;

    StartCodes->SliceCount                      = Frame->SliceHeaderList.no_slice_headers;

    for (i=0; i<StartCodes->SliceCount; i++)
    {
        StartCodes->SliceArray[i].SliceStartAddrCompressedBuffer_p      = (VC9_CompressedData_t)(CodedData + Frame->SliceHeaderList.slice_start_code[i]);
        StartCodes->SliceArray[i].SliceAddress                          = Frame->SliceHeaderList.slice_addr[i];
    }

    if(Stream->EntryPointHeader.refdist_flag)
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

    Entry->LoopfilterFlag                   = Stream->EntryPointHeader.loopfilter;
    Entry->FastuvmcFlag                     = Stream->EntryPointHeader.fastuvmc;
    Entry->ExtendedmvFlag                   = Stream->EntryPointHeader.extended_mv;
    Entry->Dquant                           = Stream->EntryPointHeader.dquant;
    Entry->VstransformFlag                  = Stream->EntryPointHeader.vstransform;
    Entry->OverlapFlag                      = Stream->EntryPointHeader.overlap;
    Entry->Quantizer                        = Stream->EntryPointHeader.quantizer;

    Entry->ExtendedDmvFlag                  = Stream->EntryPointHeader.extended_dmv;
    Entry->PanScanFlag                      = Stream->EntryPointHeader.panscan_flag;
    Entry->RefdistFlag                      = Stream->EntryPointHeader.refdist_flag;

    //Param->CurrentPicOrderCount             = 0;                    // FIX for trick modes
    //Param->ForwardPicOrderCount             = 0;                    // FIX for trick modes
    //Param->BackwardPicOrderCount            = 0;                    // FIX for trick modes

    //
    // Fillout the reference frame list stuff
    //

    if (ParsedFrameParameters->NumberOfReferenceFrameLists != 0)
    {
        if (DecodeContext->ReferenceFrameList[0].EntryCount > 0)
        {
            i                                           = DecodeContext->ReferenceFrameList[0].EntryIndicies[0];

            RefList->ForwardReferenceLuma_p             = (VC9_LumaAddress_t)BufferState[i].BufferLumaPointer;
            RefList->ForwardReferenceChroma_p           = (VC9_ChromaAddress_t)BufferState[i].BufferChromaPointer;
            RefList->ForwardReferencePictureSyntax      = (VC9_PictureSyntax_t)BufferState[i].PictureSyntax;

            if (Frame->PictureHeader.mvmode == VC1_MV_MODE_INTENSITY_COMP)
                SaveIntensityCompensationData (i, Intensity);

            GetForwardIntensityCompensationData (i, Intensity);
        }
        if (DecodeContext->ReferenceFrameList[0].EntryCount > 1)
        {
            i                                           = DecodeContext->ReferenceFrameList[0].EntryIndicies[1];
            RefList->BackwardReferenceLuma_p            = (VC9_LumaAddress_t)BufferState[i].BufferLumaPointer;
            RefList->BackwardReferenceChroma_p          = (VC9_ChromaAddress_t)BufferState[i].BufferChromaPointer;
            RefList->BackwardReferenceMBStruct_p        = (VC9_MBStructureAddress_t)BufferState[i].BufferMacroblockStructurePointer;
            RefList->BackwardReferencePictureSyntax     = (VC9_PictureSyntax_t)BufferState[i].PictureSyntax;

            GetBackwardIntensityCompensationData (i, Intensity);
        }
    }

    //{{{  Dump picture indexes
    #if 0
    {
        static unsigned int PictureNo  = 0;

        PictureNo++;
        if ((Param->IntensityComp.ForwTop_1 != 0xffffffff) || (Param->IntensityComp.ForwBot_1 != 0xffffffff) ||
            (Param->IntensityComp.ForwTop_2 != 0xffffffff) || (Param->IntensityComp.ForwBot_2 != 0xffffffff) ||
            (Param->IntensityComp.BackTop_1 != 0xffffffff) || (Param->IntensityComp.BackBot_1 != 0xffffffff) ||
            (Param->IntensityComp.BackTop_2 != 0xffffffff) || (Param->IntensityComp.BackBot_2 != 0xffffffff))
        {
            report (severity_info, "Codec: Decode %2d, Display %2d ", PictureNo-1, ParsedFrameParameters->DisplayFrameIndex);

            switch (Param->PictureType)
            {
                case VC9_PICTURE_TYPE_P:            report (severity_info, "P  ");            break;
                case VC9_PICTURE_TYPE_B:            report (severity_info, "B  ");            break;
                case VC9_PICTURE_TYPE_I:            report (severity_info, "I  ");            break;
                case VC9_PICTURE_TYPE_SKIP:         report (severity_info, "S  ");            break;
                case VC9_PICTURE_TYPE_BI:           report (severity_info, "BI ");            break;
            }
            switch (Param->PictureSyntax)
            {
                case VC9_PICTURE_SYNTAX_PROGRESSIVE:      report (severity_info, "Progressive\n");   break;
                case VC9_PICTURE_SYNTAX_INTERLACED_FRAME: report (severity_info, "Interlaced Frame (%s)\n", Param->TffFlag? "T":"B");   break;
                case VC9_PICTURE_SYNTAX_INTERLACED_FIELD: report (severity_info, "Interlaced Field (%d)\n",Frame->PictureHeader.first_field);   break;
            }
            if (Param->IntensityComp.ForwTop_1 != 0xffffffff)
                report (severity_info, "    : IntensityComp_ForwTop_1            = %08x\n", Param->IntensityComp.ForwTop_1);
            if (Param->IntensityComp.ForwBot_1 != 0xffffffff)
                report (severity_info, "    : IntensityComp_ForwBot_1            = %08x\n", Param->IntensityComp.ForwBot_1);
            if (Param->IntensityComp.ForwTop_2 != 0xffffffff)
                report (severity_info, "    : IntensityComp_ForwTop_2            = %08x\n", Param->IntensityComp.ForwTop_2);
            if (Param->IntensityComp.ForwBot_2 != 0xffffffff)
                report (severity_info, "    : IntensityComp_ForwBot_2            = %08x\n", Param->IntensityComp.ForwBot_2);
            if (Param->IntensityComp.BackTop_1 != 0xffffffff)
                report (severity_info, "    : IntensityComp_BackTop_1            = %08x\n", Param->IntensityComp.BackTop_1);
            if (Param->IntensityComp.BackBot_1 != 0xffffffff)
                report (severity_info, "    : IntensityComp_BackBot_1            = %08x\n", Param->IntensityComp.BackBot_1);
            if (Param->IntensityComp.BackTop_2 != 0xffffffff)
                report (severity_info, "    : IntensityComp_BackTop_2            = %08x\n", Param->IntensityComp.BackTop_2);
            if (Param->IntensityComp.BackBot_2 != 0xffffffff)
                report (severity_info, "    : IntensityComp_BackBot_2            = %08x\n", Param->IntensityComp.BackBot_2);
        }
    }
    #endif
    //}}}

    //
    // Fillout the actual command
    //

    memset( &Context->BaseContext.MMECommand, 0x00, sizeof(MME_Command_t) );

    Context->BaseContext.MMECommand.CmdStatus.AdditionalInfoSize    = sizeof(VC9_CommandStatus_t);
    Context->BaseContext.MMECommand.CmdStatus.AdditionalInfo_p      = (MME_GenericParams_t)(&Context->DecodeStatus);
    Context->BaseContext.MMECommand.ParamSize                       = sizeof(VC9_TransformParam_t);
    Context->BaseContext.MMECommand.Param_p                         = (MME_GenericParams_t)(&Context->DecodeParameters);

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
CodecStatus_t   Codec_MmeVideoVc1_c::ValidateDecodeContext( CodecBaseDecodeContext_t *Context )
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

CodecStatus_t   Codec_MmeVideoVc1_c::DumpSetStreamParameters(         void    *Parameters )
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
CodecStatus_t   Codec_MmeVideoVc1_c::DumpDecodeParameters(            void    *Parameters )
{
    unsigned int                        i;
    VC9_TransformParam_t*               FrameParams;
    VC9_DecodedBufferAddress_t*         Decode;
    VC9_EntryPointParam_t*              Entry;
    VC9_IntensityComp_t*                Intensity;

    FrameParams     = (VC9_TransformParam_t *)Parameters;
    Decode          = &FrameParams->DecodedBufferAddress;
    Entry           = &FrameParams->EntryPoint;
    Intensity       = &FrameParams->IntensityComp;

    StaticPicture++;

{
    report (severity_info, "********** Picture %d *********\n", StaticPicture-1);
    report (severity_info, "DDP -    Entry->LoopfilterFlag               %08x\n", Entry->LoopfilterFlag);
    report (severity_info, "DDP -    Entry->FastuvmcFlag                 %08x\n", Entry->FastuvmcFlag);
    report (severity_info, "DDP -    Entry->ExtendedmvFlag               %08x\n", Entry->ExtendedmvFlag);
    report (severity_info, "DDP -    Entry->Dquant                       %08x\n", Entry->Dquant);
    report (severity_info, "DDP -    Entry->VstransformFlag              %08x\n", Entry->VstransformFlag);
    report (severity_info, "DDP -    Entry->OverlapFlag                  %08x\n", Entry->OverlapFlag);
    report (severity_info, "DDP -    Entry->Quantizer                    %08x\n", Entry->Quantizer);
    report (severity_info, "DDP -    Entry->ExtendedDmvFlag              %08x\n", Entry->ExtendedDmvFlag);
    report (severity_info, "DDP -    Entry->PanScanFlag                  %08x\n", Entry->PanScanFlag);
    report (severity_info, "DDP -    Entry->RefdistFlag                  %08x\n", Entry->RefdistFlag);

    report (severity_info, "DDP -   Luma_p                             = %08x\n", Decode->Luma_p);
    report (severity_info, "DDP -   Chroma_p                           = %08x\n", Decode->Chroma_p);
    report (severity_info, "DDP -   MBStruct_p                         = %08x\n", Decode->MBStruct_p);

    report (severity_info, "DDP -   CircularBufferBeginAddr_p          = %08x\n", FrameParams->CircularBufferBeginAddr_p);
    report (severity_info, "DDP -   CircularBufferEndAddr_p            = %08x\n", FrameParams->CircularBufferEndAddr_p);

    report (severity_info, "DDP -   PictureStartAddrCompressedBuffer_p = %08x\n", FrameParams->PictureStartAddrCompressedBuffer_p);
    report (severity_info, "DDP -   PictureStopAddrCompressedBuffer_p  = %08x\n", FrameParams->PictureStopAddrCompressedBuffer_p);

    report (severity_info, "DDP -   BackwardReferenceLuma_p            = %08x\n", FrameParams->RefPicListAddress.BackwardReferenceLuma_p);
    report (severity_info, "DDP -   BackwardReferenceChroma_p          = %08x\n", FrameParams->RefPicListAddress.BackwardReferenceChroma_p);
    report (severity_info, "DDP -   BackwardReferenceMBStruct_p        = %08x\n", FrameParams->RefPicListAddress.BackwardReferenceMBStruct_p);
    report (severity_info, "DDP -   ForwardReferenceLuma_p             = %08x\n", FrameParams->RefPicListAddress.ForwardReferenceLuma_p);
    report (severity_info, "DDP -   ForwardReferenceChroma_p           = %08x\n", FrameParams->RefPicListAddress.ForwardReferenceChroma_p);
    report (severity_info, "DDP -   BackwardReferencePictureSyntax     = %08x\n", FrameParams->RefPicListAddress.BackwardReferencePictureSyntax);
    report (severity_info, "DDP -   ForwardReferencePictureSyntax      = %08x\n", FrameParams->RefPicListAddress.ForwardReferencePictureSyntax);
    report (severity_info, "DDP -   MainAuxEnable                      = %d\n", FrameParams->MainAuxEnable);
    report (severity_info, "DDP -   HorizontalDecimationFactor         = %d\n", FrameParams->HorizontalDecimationFactor);
    report (severity_info, "DDP -   VerticalDecimationFactor           = %d\n", FrameParams->VerticalDecimationFactor);
    report (severity_info, "DDP -   DecodingMode                       = %d\n", FrameParams->DecodingMode);
    report (severity_info, "DDP -   AebrFlag                           = %d\n", FrameParams->AebrFlag);
    report (severity_info, "DDP -   Profile                            = %d\n", FrameParams->Profile);
    report (severity_info, "DDP -   PictureSyntax                      = %d\n", FrameParams->PictureSyntax);
    report (severity_info, "DDP -   PictureType                        = %d\n", FrameParams->PictureType);
    report (severity_info, "DDP -   FinterpFlag                        = %d\n", FrameParams->FinterpFlag);
    report (severity_info, "DDP -   FrameHorizSize                     = %d\n", FrameParams->FrameHorizSize);
    report (severity_info, "DDP -   FrameVertSize                      = %d\n", FrameParams->FrameVertSize);
    report (severity_info, "DDP -   IntensityComp_ForwTop_1            = %d\n", FrameParams->IntensityComp.ForwTop_1);
    report (severity_info, "DDP -   IntensityComp_ForwTop_2            = %d\n", FrameParams->IntensityComp.ForwTop_2);
    report (severity_info, "DDP -   IntensityComp_ForwBot_1            = %d\n", FrameParams->IntensityComp.ForwBot_1);
    report (severity_info, "DDP -   IntensityComp_ForwBot_2            = %d\n", FrameParams->IntensityComp.ForwBot_2);
    report (severity_info, "DDP -   IntensityComp_BackTop_1            = %d\n", FrameParams->IntensityComp.BackTop_1);
    report (severity_info, "DDP -   IntensityComp_BackTop_2            = %d\n", FrameParams->IntensityComp.BackTop_2);
    report (severity_info, "DDP -   IntensityComp_BackBot_1            = %d\n", FrameParams->IntensityComp.BackBot_1);
    report (severity_info, "DDP -   IntensityComp_BackBot_2            = %d\n", FrameParams->IntensityComp.BackBot_2);
    report (severity_info, "DDP -   RndCtrl                            = %d\n", FrameParams->RndCtrl);
    report (severity_info, "DDP -   NumeratorBFraction                 = %d\n", FrameParams->NumeratorBFraction );
    report (severity_info, "DDP -   DenominatorBFraction               = %d\n", FrameParams->DenominatorBFraction  );

    report (severity_info, "DDP -   PostprocFlag                       = %d\n", FrameParams->PostprocFlag);
    report (severity_info, "DDP -   PulldownFlag                       = %d\n", FrameParams->PulldownFlag);
    report (severity_info, "DDP -   InterlaceFlag                      = %d\n", FrameParams->InterlaceFlag);
    report (severity_info, "DDP -   TfcntrFlag                         = %d\n", FrameParams->TfcntrFlag);
    report (severity_info, "DDP -   SliceCount                         = %d\n", FrameParams->StartCodes.SliceCount);
    for ( i = 0 ; i < FrameParams->StartCodes.SliceCount ; i++)
    {
        report (severity_info, "DDP -   SLICE %d\n", i+1);
        report (severity_info, "DDP -   SliceStartAddrCompressedBuffer_p = %d\n", FrameParams->StartCodes.SliceArray[i].SliceStartAddrCompressedBuffer_p);
        report (severity_info, "DDP -   SliceAddress = %d\n", FrameParams->StartCodes.SliceArray[i].SliceAddress);
    }
    report (severity_info, "DDP -   BackwardRefdist                    = %d\n", FrameParams->BackwardRefdist);
    report (severity_info, "DDP -   FramePictureLayerSize              = %d\n", FrameParams->FramePictureLayerSize );
    report (severity_info, "DDP -   TffFlag                            = %d\n", FrameParams->TffFlag  );
    report (severity_info, "DDP -   SecondFieldFlag                    = %d\n", FrameParams->SecondFieldFlag  );
    report (severity_info, "DDP -   RefDist                            = %d\n", FrameParams->RefDist  );
}

    return CodecNoError;
}
//}}}

//{{{  SaveIntensityCompenationData
void Codec_MmeVideoVc1_c::SaveIntensityCompensationData        (unsigned int            RefBufferIndex,
                                                                VC9_IntensityComp_t*    Intensity)
{
    Vc1FrameParameters_t*               Frame                   = (Vc1FrameParameters_t *)ParsedFrameParameters->FrameParameterStructure;
    CodecIntensityCompensation_t*       RefIntensityComp        = &BufferState[RefBufferIndex].AppliedIntensityCompensation;
    CodecIntensityCompensation_t*       OurIntensityComp        = &BufferState[CurrentDecodeBufferIndex].AppliedIntensityCompensation;

    if (Frame->PictureHeader.first_field == 1)
    // For first field intensity compensation is applied to the reference frame
    {
        if (Frame->PictureHeader.intensity_comp_field != VC1_INTENSITY_COMP_BOTTOM)
        {
            if (RefIntensityComp->Top1 == VC9_NO_INTENSITY_COMP)
                RefIntensityComp->Top1      = Frame->PictureHeader.intensity_comp_top;
            else
                RefIntensityComp->Top2      = Frame->PictureHeader.intensity_comp_top;
        }

        if (Frame->PictureHeader.intensity_comp_field != VC1_INTENSITY_COMP_TOP)
        {
            if (RefIntensityComp->Bottom1 == VC9_NO_INTENSITY_COMP)
                RefIntensityComp->Bottom1   = Frame->PictureHeader.intensity_comp_bottom;
            else
                RefIntensityComp->Bottom2   = Frame->PictureHeader.intensity_comp_bottom;
        }
    }
    else
    // For second field part is applied to the first field and part to the reference frame
    {
        if (Frame->PictureHeader.tff == 1)
        {
            if (Frame->PictureHeader.intensity_comp_field != VC1_INTENSITY_COMP_BOTTOM)
                OurIntensityComp->Top1          = Frame->PictureHeader.intensity_comp_top;

            if (Frame->PictureHeader.intensity_comp_field != VC1_INTENSITY_COMP_TOP)
            {
                if (RefIntensityComp->Bottom1 == VC9_NO_INTENSITY_COMP)
                    RefIntensityComp->Bottom1   = Frame->PictureHeader.intensity_comp_bottom;
                else
                    RefIntensityComp->Bottom2   = Frame->PictureHeader.intensity_comp_bottom;
            }
        }
        else
        {
            if (Frame->PictureHeader.intensity_comp_field != VC1_INTENSITY_COMP_TOP)
                RefIntensityComp->Bottom1       = Frame->PictureHeader.intensity_comp_bottom;

            if (Frame->PictureHeader.intensity_comp_field != VC1_INTENSITY_COMP_BOTTOM)
            {
                if (RefIntensityComp->Top1 == VC9_NO_INTENSITY_COMP)
                    RefIntensityComp->Top1      = Frame->PictureHeader.intensity_comp_top;
                else
                    RefIntensityComp->Top2      = Frame->PictureHeader.intensity_comp_top;
            }
        }
    }
}
//}}}
//{{{  GetForwardIntensityCompenationData
void Codec_MmeVideoVc1_c::GetForwardIntensityCompensationData  (unsigned int            RefBufferIndex,
                                                                VC9_IntensityComp_t*    Intensity)
{
    Vc1FrameParameters_t*               Frame                   = (Vc1FrameParameters_t *)ParsedFrameParameters->FrameParameterStructure;
    CodecIntensityCompensation_t*       RefIntensityComp        = &BufferState[RefBufferIndex].AppliedIntensityCompensation;
    CodecIntensityCompensation_t*       OurIntensityComp        = &BufferState[CurrentDecodeBufferIndex].AppliedIntensityCompensation;

    if ((Frame->PictureHeader.ptype != VC1_PICTURE_CODING_TYPE_P) && (Frame->PictureHeader.ptype != VC1_PICTURE_CODING_TYPE_B))
        return;

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
void Codec_MmeVideoVc1_c::GetBackwardIntensityCompensationData (unsigned int            RefBufferIndex,
                                                                VC9_IntensityComp_t*    Intensity)
{
    CodecIntensityCompensation_t*       RefIntensityComp        = &BufferState[RefBufferIndex].AppliedIntensityCompensation;

    Intensity->BackTop_1                = RefIntensityComp->Top1;
    Intensity->BackTop_2                = RefIntensityComp->Top2;
    Intensity->BackBot_1                = RefIntensityComp->Bottom1;
    Intensity->BackBot_2                = RefIntensityComp->Bottom2;

}
//}}}
//{{{  CheckCodecReturnParameters
// Convert the return code into human readable form.
static const char* LookupError (unsigned int Error)
{
#define E(e) case e: return #e
    switch(Error)
    {
        E(VC9_DECODER_ERROR_MB_OVERFLOW);
        E(VC9_DECODER_ERROR_RECOVERED);
        E(VC9_DECODER_ERROR_NOT_RECOVERED);
        E(VC9_DECODER_ERROR_TASK_TIMEOUT);
        default: return "VC9_DECODER_UNKNOWN_ERROR";
    }
#undef E
}
CodecStatus_t   Codec_MmeVideoVc1_c::CheckCodecReturnParameters( CodecBaseDecodeContext_t *Context )
{

    MME_Command_t*                              MMECommand              = (MME_Command_t*)(&Context->MMECommand);
    MME_CommandStatus_t*                        CmdStatus               = (MME_CommandStatus_t*)(&MMECommand->CmdStatus);
    VC9_CommandStatus_t*                        AdditionalInfo_p        = (VC9_CommandStatus_t*)CmdStatus->AdditionalInfo_p;

    if (AdditionalInfo_p != NULL)
    {
        if ((AdditionalInfo_p->ErrorCode != VC9_DECODER_NO_ERROR) && (AdditionalInfo_p->ErrorCode != VC9_DECODER_ERROR_MB_OVERFLOW))
            CODEC_TRACE("%s - %s  %x \n", __FUNCTION__, LookupError(AdditionalInfo_p->ErrorCode), AdditionalInfo_p->ErrorCode);
    }

    return CodecNoError;
}
//}}}
