/************************************************************************
Copyright (C) 2009 STMicroelectronics. All Rights Reserved.

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

Source file name : codec_mme_video_avs.cpp
Author :           Julian

Implementation of the avs video codec class for player 2.


Date        Modification                                    Name
----        ------------                                    --------
01-Jul-09   Created                                         Julian

************************************************************************/

// /////////////////////////////////////////////////////////////////////
//
//      Include any component headers

#include "codec_mme_video_avs.h"
#include "avs.h"
#include "allocinline.h"

// /////////////////////////////////////////////////////////////////////////
//
// Locally defined constants
//

//{{{  Locally defined structures
typedef struct AvsCodecStreamParameterContext_s
{
    CodecBaseStreamParameterContext_t   BaseContext;

    MME_AVSSetGlobalParamSequence_t     StreamParameters;
} AvsCodecStreamParameterContext_t;

//#if __KERNEL__
#if 0
#define BUFFER_AVS_CODEC_STREAM_PARAMETER_CONTEXT             "AvsCodecStreamParameterContext"
#define BUFFER_AVS_CODEC_STREAM_PARAMETER_CONTEXT_TYPE        {BUFFER_AVS_CODEC_STREAM_PARAMETER_CONTEXT, BufferDataTypeBase, AllocateFromDeviceMemory, 32, 0, true, true, sizeof(AvsCodecStreamParameterContext_t)}
#else
#define BUFFER_AVS_CODEC_STREAM_PARAMETER_CONTEXT             "AvsCodecStreamParameterContext"
#define BUFFER_AVS_CODEC_STREAM_PARAMETER_CONTEXT_TYPE        {BUFFER_AVS_CODEC_STREAM_PARAMETER_CONTEXT, BufferDataTypeBase, AllocateFromOSMemory, 32, 0, true, true, sizeof(AvsCodecStreamParameterContext_t)}
#endif

static BufferDataDescriptor_t            AvsCodecStreamParameterContextDescriptor = BUFFER_AVS_CODEC_STREAM_PARAMETER_CONTEXT_TYPE;

// --------

typedef struct AvsCodecDecodeContext_s
{
    CodecBaseDecodeContext_t            BaseContext;

    MME_AVSVideoDecodeParams_t          DecodeParameters;
    MME_AVSVideoDecodeReturnParams_t    DecodeStatus;
} AvsCodecDecodeContext_t;

//#if __KERNEL__
#if 0
#define BUFFER_AVS_CODEC_DECODE_CONTEXT       "AvsCodecDecodeContext"
#define BUFFER_AVS_CODEC_DECODE_CONTEXT_TYPE  {BUFFER_AVS_CODEC_DECODE_CONTEXT, BufferDataTypeBase, AllocateFromDeviceMemory, 32, 0, true, true, sizeof(AvsCodecDecodeContext_t)}
#else
#define BUFFER_AVS_CODEC_DECODE_CONTEXT       "AvsCodecDecodeContext"
#define BUFFER_AVS_CODEC_DECODE_CONTEXT_TYPE  {BUFFER_AVS_CODEC_DECODE_CONTEXT, BufferDataTypeBase, AllocateFromOSMemory, 32, 0, true, true, sizeof(AvsCodecDecodeContext_t)}
#endif

static BufferDataDescriptor_t            AvsCodecDecodeContextDescriptor = BUFFER_AVS_CODEC_DECODE_CONTEXT_TYPE;

#if defined (AVS_MBSTRUCT)
#define BUFFER_AVS_MBSTRUCT             "AvsMbStruct"
#define BUFFER_AVS_MBSTRUCT_TYPE        {BUFFER_AVS_MBSTRUCT, BufferDataTypeBase, AllocateFromNamedDeviceMemory, 32, 0, false, true, NOT_SPECIFIED}

static BufferDataDescriptor_t           AvsMbStructInitialDescriptor = BUFFER_AVS_MBSTRUCT_TYPE;
#endif

//}}}

//{{{  Constructor
// /////////////////////////////////////////////////////////////////////////
//
//      Cosntructor function, fills in the codec specific parameter values
//

Codec_MmeVideoAvs_c::Codec_MmeVideoAvs_c( void )
{
    Configuration.CodecName                             = "Avs video";

    Configuration.DecodeOutputFormat                    = FormatVideo420_MacroBlock;

    Configuration.StreamParameterContextCount           = 1;
    Configuration.StreamParameterContextDescriptor      = &AvsCodecStreamParameterContextDescriptor;

    Configuration.DecodeContextCount                    = 4;
    Configuration.DecodeContextDescriptor               = &AvsCodecDecodeContextDescriptor;

    Configuration.MaxDecodeIndicesPerBuffer             = 2;
    Configuration.SliceDecodePermitted                  = false;

#if defined (TRANSFORMER_AVSDEC_HD)
    Configuration.TransformName[0]                      = AVSDECHD_MME_TRANSFORMER_NAME "0";
    Configuration.TransformName[1]                      = AVSDECHD_MME_TRANSFORMER_NAME "1";
#else
    Configuration.TransformName[0]                      = AVSDECSD_MME_TRANSFORMER_NAME "0";
    Configuration.TransformName[1]                      = AVSDECSD_MME_TRANSFORMER_NAME "1";
#endif
    Configuration.AvailableTransformers                 = 2;

    Configuration.SizeOfTransformCapabilityStructure    = sizeof(AvsTransformCapability);
    Configuration.TransformCapabilityStructurePointer   = (void *)(&AvsTransformCapability);

    //
    // The video firmware violates the MME spec. and passes data buffer addresses
    // as parametric information. For this reason it requires physical addresses
    // to be used.
    //

    Configuration.AddressingMode                        = PhysicalAddress;

    IntraMbStructMemoryDevice                           = NULL;
    MbStructMemoryDevice                                = NULL;

#if defined (AVS_MBSTRUCT)
    AvsMbStructPool                                     = NULL;
#endif

    DecodingWidth                                       = AVS_MAXIMUM_PICTURE_WIDTH;
    DecodingHeight                                      = AVS_MAXIMUM_PICTURE_HEIGHT;
    //
    // We do not need the coded data after decode is complete
    //

    Configuration.ShrinkCodedDataBuffersAfterDecode     = true;

    //
    // My trick mode parameters
    //

    Configuration.TrickModeParameters.EmpiricalMaximumDecodeFrameRateShortIntegration   = 60;
    Configuration.TrickModeParameters.EmpiricalMaximumDecodeFrameRateLongIntegration    = 60;

    Configuration.TrickModeParameters.SubstandardDecodeSupported        = false;
    Configuration.TrickModeParameters.SubstandardDecodeRateIncrease     = 1;

    Configuration.TrickModeParameters.DefaultGroupSize                  = 12;
    Configuration.TrickModeParameters.DefaultGroupReferenceFrameCount   = 4;

//

    Reset();
}
//}}}
//{{{  Destructor
// /////////////////////////////////////////////////////////////////////////
//
//      Destructor function, ensures a full halt and reset 
//      are executed for all levels of the class.
//

Codec_MmeVideoAvs_c::~Codec_MmeVideoAvs_c( void )
{
    Halt();
    Reset();
}
//}}}
//{{{  Reset
// /////////////////////////////////////////////////////////////////////////
//
//      Reset function for AVS specific members.
//      
//

CodecStatus_t   Codec_MmeVideoAvs_c::Reset( void )
{
    if (IntraMbStructMemoryDevice != NULL)
    {
        AllocatorClose (IntraMbStructMemoryDevice);
        IntraMbStructMemoryDevice       = NULL;
    }

    if (MbStructMemoryDevice != NULL)
    {
        AllocatorClose (MbStructMemoryDevice);
        MbStructMemoryDevice            = NULL;
    }

#if defined (AVS_MBSTRUCT)
    if (AvsMbStructPool != NULL)
    {
        BufferManager->DestroyPool (AvsMbStructPool);
        AvsMbStructPool                 = NULL;
    }
#endif

    return Codec_MmeVideo_c::Reset();
}
//}}}

//{{{  HandleCapabilities
// /////////////////////////////////////////////////////////////////////////
//
//      Function to deal with the returned capabilities
//      structure for an avs mme transformer.
//

CodecStatus_t   Codec_MmeVideoAvs_c::HandleCapabilities( void )
{
#if defined (TRANSFORMER_AVSDEC_HD)
    CODEC_TRACE("MME Transformer '%s' capabilities are :-\n", AVSDECHD_MME_TRANSFORMER_NAME);
#else
    CODEC_TRACE("MME Transformer '%s' capabilities are :-\n", AVSDECSD_MME_TRANSFORMER_NAME);
#endif
    CODEC_TRACE("    Version                           = %x\n", AvsTransformCapability.api_version);

    return CodecNoError;
}
//}}}
#if defined (AVS_MBSTRUCT)
//{{{  RegisterOutputBufferRing
///////////////////////////////////////////////////////////////////////////
///
///     Allocate the VC1 macroblocks structure.
///
///     Due to the structure of the shared super-class, most codecs allocate
///     the majority of the resources when the player supplies it with an
///     output buffer.
///
CodecStatus_t   Codec_MmeVideoAvs_c::RegisterOutputBufferRing(   Ring_t                    Ring )
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

    Status      = BufferManager->FindBufferDataType( BUFFER_AVS_MBSTRUCT, &AvsMbStructType );
    if( Status != BufferNoError )
    {
        //
        // It didn't already exist - create it
        //

        Status  = BufferManager->CreateBufferDataType( &AvsMbStructInitialDescriptor, &AvsMbStructType );
        if( Status != BufferNoError )
        {
                CODEC_ERROR ("Failed to create the %s buffer type.\n", AvsMbStructInitialDescriptor.TypeName );
                return Status;
        }
    }

    //
    // Now create the pool
    //

    unsigned int                    Size;
    Size            = ((DecodingWidth + 15) / 16) * ((DecodingHeight + 15) / 16) * 6 * sizeof(unsigned int);
    Status          = BufferManager->CreatePool( &AvsMbStructPool, AvsMbStructType, DecodeBufferCount, Size,
                                                 NULL, NULL, Configuration.AncillaryMemoryPartitionName );
    if( Status != BufferNoError )
    {
        CODEC_ERROR ("Failed to create macroblock structure pool.\n" );
        return Status;
    }

    return CodecNoError;
}
//}}}
#endif
//{{{  FillOutTransformerInitializationParameters
// /////////////////////////////////////////////////////////////////////////
//
//      Function to deal with the returned capabilities
//      structure for an avs mme transformer.
//

CodecStatus_t   Codec_MmeVideoAvs_c::FillOutTransformerInitializationParameters( void )
{
    unsigned int                Size;
    allocator_status_t          AStatus;

    CODEC_TRACE("%s\n", __FUNCTION__);
    //
    // Fillout the command parameters
    //

    AvsInitializationParameters.CircularBufferBeginAddr_p       = (U32*)0x00000000;
    AvsInitializationParameters.CircularBufferEndAddr_p         = (U32*)0xffffffff;

    // Get the macroblock structure buffer
    if (IntraMbStructMemoryDevice != NULL)
    {
        AllocatorClose (IntraMbStructMemoryDevice);
        IntraMbStructMemoryDevice       = NULL;
    }
    Size                = ((DecodingWidth + 15) / 16) * ((DecodingHeight + 15) / 16) * sizeof(AVS_IntraMBstruct_t);
    CODEC_TRACE("Allocating %d bytes for intra mbstruct buffer:\n", Size);
    AStatus             = LmiAllocatorOpen (&IntraMbStructMemoryDevice, Size, true);
    if( AStatus != allocator_ok )
    {
        CODEC_ERROR ("Failed to allocate intra macroblock structure memory\n" );
        return CodecError;
    }
    AvsInitializationParameters.IntraMB_struct_ptr      = (AVS_IntraMBstruct_t*)AllocatorPhysicalAddress (IntraMbStructMemoryDevice);

    // Get the macroblock structure buffer
    if (MbStructMemoryDevice != NULL)
    {
        AllocatorClose (MbStructMemoryDevice);
        MbStructMemoryDevice            = NULL;
    }
    Size                = ((DecodingWidth + 15) / 16) * ((DecodingHeight + 15) / 16) * 6 * sizeof(unsigned int);
    CODEC_TRACE("Allocating %d bytes for mbstruct buffer:\n", Size);
    AStatus             = LmiAllocatorOpen (&MbStructMemoryDevice, Size, true);
    if( AStatus != allocator_ok )
    {
        CODEC_ERROR ("Failed to allocate macroblock structure memory\n" );
        return CodecError;
    }

    //
    // Fillout the actual command
    //

    MMEInitializationParameters.TransformerInitParamsSize       = sizeof(AvsInitializationParameters);
    MMEInitializationParameters.TransformerInitParams_p         = (MME_GenericParams_t)(&AvsInitializationParameters);

//

    return CodecNoError;
}
//}}}

//{{{  FillOutSetStreamParametersCommand
// /////////////////////////////////////////////////////////////////////////
//
//      Function to fill out the stream parameters 
//      structure for an avs mme transformer.
//

CodecStatus_t   Codec_MmeVideoAvs_c::FillOutSetStreamParametersCommand( void )
{
    AvsStreamParameters_t*              Parsed          = (AvsStreamParameters_t*)ParsedFrameParameters->StreamParameterStructure;
    AvsCodecStreamParameterContext_t*   Context         = (AvsCodecStreamParameterContext_t*)StreamParameterContext;

    //
    // Fillout the command parameters
    //

    DecodingWidth                                       = Parsed->SequenceHeader.horizontal_size;
    DecodingHeight                                      = Parsed->SequenceHeader.vertical_size;

    Context->StreamParameters.Width                     = DecodingWidth;
    Context->StreamParameters.Height                    = DecodingHeight;
    Context->StreamParameters.Progressive_sequence      = (AVS_SeqSyntax_t)Parsed->SequenceHeader.progressive_sequence;

    //
    // Fillout the actual command
    //

    memset( &Context->BaseContext.MMECommand, 0x00, sizeof(MME_Command_t) );

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

CodecStatus_t   Codec_MmeVideoAvs_c::FillOutDecodeCommand(       void )
{
    AvsCodecDecodeContext_t*            Context         = (AvsCodecDecodeContext_t *)DecodeContext;
    AvsFrameParameters_t*               Parsed          = (AvsFrameParameters_t *)ParsedFrameParameters->FrameParameterStructure;
    AvsVideoPictureHeader_t*            PictureHeader   = &Parsed->PictureHeader;
    MME_AVSVideoDecodeParams_t*         Param;
    AVS_StartCodecsParam_t*             StartCodes;
    AVS_DecodedBufferAddress_t*         Decode;
    AVS_RefPicListAddress_t*            RefList;

    unsigned int                        Entry;

    CODEC_DEBUG("%s\n", __FUNCTION__);

    // For avs we do not do slice decodes.
    KnownLastSliceInFieldFrame                  = true;

    Param                                       = &Context->DecodeParameters;
    Decode                                      = &Param->DecodedBufferAddr;
    RefList                                     = &Param->RefPicListAddr;
    StartCodes                                  = &Param->StartCodecs;

#if defined (TRANSFORMER_AVSDEC_HD)
    Param->PictureStartAddr_p                   = (AVS_CompressedData_t)(CodedData + PictureHeader->top_field_offset);
#else
    Param->PictureStartAddr_p                   = (AVS_CompressedData_t)CodedData;
#endif
    Param->PictureEndAddr_p                     = (AVS_CompressedData_t)(CodedData + CodedDataLength);

    Decode->Luma_p                              = (AVS_LumaAddress_t)BufferState[CurrentDecodeBufferIndex].BufferLumaPointer;
    Decode->Chroma_p                            = (AVS_ChromaAddress_t)BufferState[CurrentDecodeBufferIndex].BufferChromaPointer;
    if (Player->PolicyValue( Playback, this->Stream, PolicyDecimateDecoderOutput) != PolicyValueDecimateDecoderOutputDisabled)
    {
        Decode->LumaDecimated_p                 = (AVS_LumaAddress_t)BufferState[CurrentDecodeBufferIndex].DecimatedLumaPointer;
        Decode->ChromaDecimated_p               = (AVS_ChromaAddress_t)BufferState[CurrentDecodeBufferIndex].DecimatedChromaPointer;
    }
    else
    {
        Decode->LumaDecimated_p                 = NULL;
        Decode->ChromaDecimated_p               = NULL;
    }

    //{{{  Obtain MBStruct buffer
    #if defined (AVS_MBSTRUCT)
    if (PictureHeader->ReversePlay)
    {
        if (PictureHeader->picture_coding_type != AVS_PICTURE_CODING_TYPE_B)
        {
            // Get the macroblock structure buffer
            Buffer_t                AvsMbStructBuffer;
            unsigned int            Size;
            CodecStatus_t           Status;

            Size            = ((DecodingWidth + 15) / 16) * ((DecodingHeight + 15) / 16) * 6 * sizeof(unsigned int);

            Status  = AvsMbStructPool->GetBuffer (&AvsMbStructBuffer, Size);
            if( Status != BufferNoError )
            {
                CODEC_ERROR ("Failed to get macroblock structure buffer.\n" );
                return Status;
            }

            AvsMbStructBuffer->ObtainDataReference (NULL, NULL, (void **)&Decode->MBStruct_p, PhysicalAddress);

            CurrentDecodeBuffer->AttachBuffer (AvsMbStructBuffer);          // Attach to decode buffer (so it will be freed at the same time)
            AvsMbStructBuffer->DecrementReferenceCount();                   // and release ownership of the buffer to the decode buffer

            // Remember the MBStruct pointer in case we have a second field to follow
            BufferState[CurrentDecodeBufferIndex].BufferMacroblockStructurePointer  = (unsigned char*)Decode->MBStruct_p;
        }
    }
    else
        Decode->MBStruct_p                      = (U32*)AllocatorPhysicalAddress (MbStructMemoryDevice);
    #else
    Decode->MBStruct_p                          = (U32*)AllocatorPhysicalAddress (MbStructMemoryDevice);
    #endif
    //}}}

    //{{{  Initialise decode buffers to bright pink
    #if 0
    unsigned int        LumaSize        = DecodingWidth*DecodingHeight;
    unsigned char*      LumaBuffer;
    unsigned char*      ChromaBuffer;
    static unsigned int Colour;
    CurrentDecodeBuffer->ObtainDataReference( NULL, NULL, (void**)&LumaBuffer, UnCachedAddress);
    ChromaBuffer                        = LumaBuffer+LumaSize;
    memset (LumaBuffer,   0xff, LumaSize);
    memset (ChromaBuffer, Colour++ & 0xff, LumaSize/2);
    #endif
    //}}}

    //{{{  Fillout the reference frame lists
    if (ParsedFrameParameters->NumberOfReferenceFrameLists != 0)
    {
        if (DecodeContext->ReferenceFrameList[0].EntryCount > 0)
        {
            Entry                                                       = DecodeContext->ReferenceFrameList[0].EntryIndicies[0];
            RefList->BackwardRefLuma_p                                  = (AVS_LumaAddress_t)BufferState[Entry].BufferLumaPointer;
            RefList->BackwardRefChroma_p                                = (AVS_ChromaAddress_t)BufferState[Entry].BufferChromaPointer;
            //Param->Picture_structure_bwd                                = (AVS_PicStruct_t)BufferState[i].PictureSyntax);
    #if defined (AVS_MBSTRUCT)
            if (PictureHeader->ReversePlay)
                Decode->MBStruct_p                                      = (U32*)BufferState[Entry].BufferMacroblockStructurePointer;
    #endif
        }
        if( DecodeContext->ReferenceFrameList[0].EntryCount > 1 )
        {
            Entry                                                       = DecodeContext->ReferenceFrameList[0].EntryIndicies[1];
            RefList->ForwardRefLuma_p                                   = (AVS_LumaAddress_t)BufferState[Entry].BufferLumaPointer;
            RefList->ForwardRefChroma_p                                 = (AVS_ChromaAddress_t)BufferState[Entry].BufferChromaPointer;
    #if defined (AVS_MBSTRUCT)
            if ((PictureHeader->ReversePlay) && (PictureHeader->picture_coding_type == AVS_PICTURE_CODING_TYPE_P))
                Decode->MBStruct_p                                      = (U32*)BufferState[Entry].BufferMacroblockStructurePointer;
    #endif
        }
    }
    //}}}

    //{{{  Fill in remaining fields
    Param->Progressive_frame                                            = (AVS_FrameSyntax_t)PictureHeader->progressive_frame;

    Param->MainAuxEnable                                                = AVS_MAINOUT_EN;
    Param->HorizontalDecimationFactor                                   = AVS_HDEC_1;
    Param->VerticalDecimationFactor                                     = AVS_VDEC_1;

    Param->AebrFlag                                                     = 0;
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
    //}}}
    //{{{  Fill in decimation values if required
    switch (Player->PolicyValue (Playback, this->Stream, PolicyDecimateDecoderOutput))
    {   
        case PolicyValueDecimateDecoderOutputDisabled:
        {
            // Normal Case
            Param->MainAuxEnable                = AVS_MAINOUT_EN;
            Param->HorizontalDecimationFactor   = AVS_HDEC_1;
            Param->VerticalDecimationFactor     = AVS_VDEC_1;
            break;
        }
        case PolicyValueDecimateDecoderOutputHalf:
        {
            Param->MainAuxEnable                = AVS_AUX_MAIN_OUT_EN;
            Param->HorizontalDecimationFactor   = AVS_HDEC_ADVANCED_2;
            if (Param->Progressive_frame)
                Param->VerticalDecimationFactor = AVS_VDEC_ADVANCED_2_PROG;
            else
                Param->VerticalDecimationFactor = AVS_VDEC_ADVANCED_2_INT;
            break;
        }
        case PolicyValueDecimateDecoderOutputQuarter:
        {
            Param->MainAuxEnable                = AVS_AUX_MAIN_OUT_EN;
            Param->HorizontalDecimationFactor   = AVS_HDEC_ADVANCED_4;
            Param->VerticalDecimationFactor     = AVS_VDEC_ADVANCED_2_INT;
            break;
        }
    }
    //}}}

    //{{{  Fill out slice list if HD decode
    #if defined (TRANSFORMER_AVSDEC_HD)
    StartCodes->SliceCount                                              = Parsed->SliceHeaderList.no_slice_headers;
    for (unsigned int i=0; i<StartCodes->SliceCount; i++)
    {
        StartCodes->SliceArray[i].SliceStartAddrCompressedBuffer_p      = (AVS_CompressedData_t)(CodedData + Parsed->SliceHeaderList.slice_array[i].slice_offset);
        StartCodes->SliceArray[i].SliceAddress                          = Parsed->SliceHeaderList.slice_array[i].slice_start_code;
    }
    #endif
    //}}}

    //{{{  Set up raster buffers if SD decode
    #if !defined (TRANSFORMER_AVSDEC_HD)
    {
        // AVS SD uses both raster and Omega 2 buffers so obtain a raster buffer from the pool as well.
        // Pass details to the firmware in the scatterpages
        BufferStatus_t                  Status;
        Buffer_t                        RasterBuffer;
        BufferStructure_t               RasterBufferStructure;
        unsigned char*                  RasterBufferBase;

        Status                                                          = FillOutDecodeBufferRequest (&RasterBufferStructure);
        if (Status != BufferNoError)
        {
            CODEC_ERROR("%s - Failed to fill out a buffer request structure.\n", __FUNCTION__);
            return Status;
        }
        // Override the format so we get one sized for raster rather than macroblock
        RasterBufferStructure.Format                                    = FormatVideo420_PlanarAligned;
        RasterBufferStructure.ComponentBorder[0]                        = 32;
        RasterBufferStructure.ComponentBorder[1]                        = 32;
        // Ask the manifestor for a buffer of the new format
        Status                                                          = Manifestor->GetDecodeBuffer (&RasterBufferStructure, &RasterBuffer);
        if (Status != BufferNoError)
        {
            CODEC_ERROR("%s - Failed to obtain a decode buffer from the manifestor.\n", __FUNCTION__);
            return Status;
        }
        RasterBuffer->ObtainDataReference (NULL, NULL, (void **)&RasterBufferBase, UnCachedAddress);

        //{{{  Fill in details for all buffers
        for (int i = 0; i < AVS_NUM_MME_BUFFERS; i++)
        {
            DecodeContext->MMEBufferList[i]                             = &DecodeContext->MMEBuffers[i];
            DecodeContext->MMEBuffers[i].StructSize                     = sizeof (MME_DataBuffer_t);
            DecodeContext->MMEBuffers[i].UserData_p                     = NULL;
            DecodeContext->MMEBuffers[i].Flags                          = 0;
            DecodeContext->MMEBuffers[i].StreamNumber                   = 0;
            DecodeContext->MMEBuffers[i].NumberOfScatterPages           = 1;
            DecodeContext->MMEBuffers[i].ScatterPages_p                 = &DecodeContext->MMEPages[i];
            DecodeContext->MMEBuffers[i].StartOffset                    = 0;
        }
        //}}}

        // Then overwrite bits specific to other buffers
        DecodeContext->MMEBuffers[AVS_MME_CURRENT_FRAME_BUFFER].ScatterPages_p[0].Page_p        = RasterBufferBase + RasterBufferStructure.ComponentOffset[0];
        DecodeContext->MMEBuffers[AVS_MME_CURRENT_FRAME_BUFFER].TotalSize                       = RasterBufferStructure.Size;

        // Preserve raster buffer pointers for later use as reference frames
        BufferState[CurrentDecodeBufferIndex].BufferRasterPointer                               = RasterBufferBase + RasterBufferStructure.ComponentOffset[0];

        if (ParsedFrameParameters->NumberOfReferenceFrameLists != 0)
        {
            if (DecodeContext->ReferenceFrameList[0].EntryCount > 0)
            {
                Entry                                                                                       = DecodeContext->ReferenceFrameList[0].EntryIndicies[0];
                DecodeContext->MMEBuffers[AVS_MME_BACKWARD_REFERENCE_FRAME_BUFFER].ScatterPages_p[0].Page_p = BufferState[Entry].BufferRasterPointer;
                DecodeContext->MMEBuffers[AVS_MME_BACKWARD_REFERENCE_FRAME_BUFFER].TotalSize                = RasterBufferStructure.Size;
            }
            if (DecodeContext->ReferenceFrameList[0].EntryCount > 1)
            {
                Entry                                                                                       = DecodeContext->ReferenceFrameList[0].EntryIndicies[1];
                DecodeContext->MMEBuffers[AVS_MME_FORWARD_REFERENCE_FRAME_BUFFER].ScatterPages_p[0].Page_p  = BufferState[Entry].BufferRasterPointer;
                DecodeContext->MMEBuffers[AVS_MME_FORWARD_REFERENCE_FRAME_BUFFER].TotalSize                 = RasterBufferStructure.Size;
            }
        }
        //{{{  Initialise remaining scatter page values
        for (int i = 0; i < AVS_NUM_MME_BUFFERS; i++)
        {                                                                   // Only one scatterpage, so size = totalsize
            DecodeContext->MMEBuffers[i].ScatterPages_p[0].Size         = DecodeContext->MMEBuffers[i].TotalSize;
            DecodeContext->MMEBuffers[i].ScatterPages_p[0].BytesUsed    = 0;
            DecodeContext->MMEBuffers[i].ScatterPages_p[0].FlagsIn      = 0;
            DecodeContext->MMEBuffers[i].ScatterPages_p[0].FlagsOut     = 0;
        }
        //}}}

        // Attach planar buffer to decode buffer and let go of it
        CurrentDecodeBuffer->AttachBuffer (RasterBuffer);
        RasterBuffer->DecrementReferenceCount ();

        //{{{  Initialise raster decode buffers to bright pink
        #if 0
        {
            unsigned int        LumaSize        = (DecodingWidth+32)*(DecodingHeight+32);
            unsigned char*      LumaBuffer      = (unsigned char*)DecodeContext->MMEBuffers[AVS_MME_CURRENT_FRAME_BUFFER].ScatterPages_p[0].Page_p;
            unsigned char*      ChromaBuffer    = &LumaBuffer[LumaSize];
            memset (LumaBuffer,   0xff, LumaSize);
            memset (ChromaBuffer, 0xff, LumaSize/2);
        }
        #endif
        //}}}
    }
    #endif
    //}}}

    // Fillout the actual command
    memset( &Context->BaseContext.MMECommand, 0x00, sizeof(MME_Command_t) );

    Context->BaseContext.MMECommand.CmdStatus.AdditionalInfoSize        = sizeof(Context->DecodeStatus);
    Context->BaseContext.MMECommand.CmdStatus.AdditionalInfo_p          = (MME_GenericParams_t)(&Context->DecodeStatus);
    Context->BaseContext.MMECommand.ParamSize                           = sizeof(Context->DecodeParameters);
    Context->BaseContext.MMECommand.Param_p                             = (MME_GenericParams_t)(&Context->DecodeParameters);

#if !defined (TRANSFORMER_AVSDEC_HD)
    DecodeContext->MMECommand.NumberInputBuffers                        = AVS_NUM_MME_INPUT_BUFFERS;
    DecodeContext->MMECommand.NumberOutputBuffers                       = AVS_NUM_MME_OUTPUT_BUFFERS;
    DecodeContext->MMECommand.DataBuffers_p                             = (MME_DataBuffer_t**)DecodeContext->MMEBufferList;
#endif

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
CodecStatus_t   Codec_MmeVideoAvs_c::ValidateDecodeContext( CodecBaseDecodeContext_t *Context )
{
    CODEC_DEBUG("%s\n", __FUNCTION__);
    return CodecNoError;
}
//}}}
//{{{  DumpSetStreamParameters
// /////////////////////////////////////////////////////////////////////////
//
//      Function to dump out the set stream 
//      parameters from an mme command.
//

CodecStatus_t   Codec_MmeVideoAvs_c::DumpSetStreamParameters(         void    *Parameters )
{
    MME_AVSSetGlobalParamSequence_t*    StreamParameters        = (MME_AVSSetGlobalParamSequence_t*)Parameters;

    CODEC_TRACE("%s\n", __FUNCTION__);
    CODEC_TRACE("     Progressive           %6d\n", StreamParameters->Progressive_sequence);

    return CodecNoError;
}
//}}}
//{{{  DumpDecodeParameters
// /////////////////////////////////////////////////////////////////////////
//
//      Function to dump out the decode
//      parameters from an mme command.
//

CodecStatus_t   Codec_MmeVideoAvs_c::DumpDecodeParameters(            void    *Parameters )
{
    MME_AVSVideoDecodeParams_t*         Param           = (MME_AVSVideoDecodeParams_t *)Parameters;
    AVS_DecodedBufferAddress_t*         Decode;
    AVS_RefPicListAddress_t*            RefList;

    Decode                              = &Param->DecodedBufferAddr;
    RefList                             = &Param->RefPicListAddr;

    CODEC_TRACE("%s\n", __FUNCTION__);
    CODEC_TRACE("    Param->PictureStartAddr_p                  %x\n", Param->PictureStartAddr_p);
    CODEC_TRACE("    Param->PictureEndAddr_p                    %x\n", Param->PictureEndAddr_p);
    CODEC_TRACE("    Decode->Luma_p                             %x\n", Decode->Luma_p);
    CODEC_TRACE("    Decode->Chroma_p                           %x\n", Decode->Chroma_p);
    CODEC_TRACE("    Decode->LumaDecimated_p                    %x\n", Decode->LumaDecimated_p);
    CODEC_TRACE("    Decode->ChromaDecimated_p                  %x\n", Decode->ChromaDecimated_p);
    CODEC_TRACE("    Decode->MBStruct__p                        %x\n", Decode->MBStruct_p);
    CODEC_TRACE("    RefList->ForwardRefLuma_p                  %x\n", RefList->ForwardRefLuma_p);
    CODEC_TRACE("    RefList->ForwardRefChroma_p                %x\n", RefList->ForwardRefChroma_p);
    CODEC_TRACE("    RefList->BackwardRefLuma_p                 %x\n", RefList->BackwardRefLuma_p);
    CODEC_TRACE("    RefList->BackwardRefChroma_p               %x\n", RefList->BackwardRefChroma_p);
    CODEC_TRACE("    Param->Progressive_frame                   %x\n", Param->Progressive_frame);
    CODEC_TRACE("    Param->MainAuxEnable                       %x\n", Param->MainAuxEnable);
    CODEC_TRACE("    Param->HorizontalDecimationFactor          %x\n", Param->HorizontalDecimationFactor);
    CODEC_TRACE("    Param->VerticalDecimationFactor            %x\n", Param->VerticalDecimationFactor);
    CODEC_TRACE("    Param->AebrFlag                            %x\n", Param->AebrFlag);
    CODEC_TRACE("    Param->Picture_structure                   %x\n", Param->Picture_structure);
    CODEC_TRACE("    Param->Picture_structure_bwd               %x\n", Param->Picture_structure_bwd);
    CODEC_TRACE("    Param->Fixed_picture_qp                    %x\n", Param->Fixed_picture_qp);
    CODEC_TRACE("    Param->Picture_qp                          %x\n", Param->Picture_qp);
    CODEC_TRACE("    Param->Skip_mode_flag                      %x\n", Param->Skip_mode_flag);
    CODEC_TRACE("    Param->Loop_filter_disable                 %x\n", Param->Loop_filter_disable);
    CODEC_TRACE("    Param->alpha_offset                        %x\n", Param->alpha_offset);
    CODEC_TRACE("    Param->beta_offset                         %x\n", Param->beta_offset);
    CODEC_TRACE("    Param->Picture_ref_flag                    %x\n", Param->Picture_ref_flag);
    CODEC_TRACE("    Param->tr                                  %x\n", Param->tr);
    CODEC_TRACE("    Param->imgtr_next_P                        %x\n", Param->imgtr_next_P);
    CODEC_TRACE("    Param->imgtr_last_P                        %x\n", Param->imgtr_last_P);
    CODEC_TRACE("    Param->imgtr_last_prev_P                   %x\n", Param->imgtr_last_prev_P);
    CODEC_TRACE("    Param->field_flag                          %x\n", Param->field_flag);
    CODEC_TRACE("    Param->DecodingMode                        %x\n", Param->DecodingMode);
    CODEC_TRACE("    Param->AdditionalFlags                     %x\n", Param->AdditionalFlags);
    CODEC_TRACE("    Param->FrameType                           %x\n", Param->FrameType);
#if !defined (TRANSFORMER_AVSDEC_HD)
    CODEC_TRACE("    Param->topfield_pos                        %x\n", Param->topfield_pos);
    CODEC_TRACE("    Param->botfield_pos                        %x\n", Param->botfield_pos);
    //CODEC_TRACE("    Param->CEH_returnvalue_flag                %x\n", Param->CEH_returnvalue_flag);
    CODEC_TRACE("    MMEBuffers[Current]...Page_p               %x\n", DecodeContext->MMEBuffers[AVS_MME_CURRENT_FRAME_BUFFER].ScatterPages_p[0].Page_p);
    CODEC_TRACE("    MMEBuffers[Current]...TotalSize            %d\n", DecodeContext->MMEBuffers[AVS_MME_CURRENT_FRAME_BUFFER].TotalSize);
    CODEC_TRACE("    MMEBuffers[Forward]...Page_p               %x\n", DecodeContext->MMEBuffers[AVS_MME_FORWARD_REFERENCE_FRAME_BUFFER].ScatterPages_p[0].Page_p);
    CODEC_TRACE("    MMEBuffers[Backward]...Page_p              %x\n", DecodeContext->MMEBuffers[AVS_MME_BACKWARD_REFERENCE_FRAME_BUFFER].ScatterPages_p[0].Page_p);
#endif

    return CodecNoError;
}
//}}}
