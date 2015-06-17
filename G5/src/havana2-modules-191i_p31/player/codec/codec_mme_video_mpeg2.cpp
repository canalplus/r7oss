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

Source file name : codec_mme_video_mpeg2.cpp
Author :           Nick

Implementation of the mpeg2 video codec class for player 2.


Date        Modification                                    Name
----        ------------                                    --------
25-Jan-06   Created                                         Nick

************************************************************************/

// /////////////////////////////////////////////////////////////////////
//
//      Include any component headers

#include "codec_mme_video_mpeg2.h"
#include "mpeg2.h"

// /////////////////////////////////////////////////////////////////////////
//
// Locally defined constants
//

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
    0x10, 0x10, 0x16, 0x18, 0x1b, 0x1c, 0x22, 0x25,
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

// /////////////////////////////////////////////////////////////////////////
//
// Locally defined structures
//

typedef struct Mpeg2CodecStreamParameterContext_s
{
    CodecBaseStreamParameterContext_t   BaseContext;

    MPEG2_SetGlobalParamSequence_t      StreamParameters;
} Mpeg2CodecStreamParameterContext_t;

//#if __KERNEL__
#if 0
#define BUFFER_MPEG2_CODEC_STREAM_PARAMETER_CONTEXT             "Mpeg2CodecStreamParameterContext"
#define BUFFER_MPEG2_CODEC_STREAM_PARAMETER_CONTEXT_TYPE        {BUFFER_MPEG2_CODEC_STREAM_PARAMETER_CONTEXT, BufferDataTypeBase, AllocateFromDeviceMemory, 32, 0, true, true, sizeof(Mpeg2CodecStreamParameterContext_t)}
#else
#define BUFFER_MPEG2_CODEC_STREAM_PARAMETER_CONTEXT             "Mpeg2CodecStreamParameterContext"
#define BUFFER_MPEG2_CODEC_STREAM_PARAMETER_CONTEXT_TYPE        {BUFFER_MPEG2_CODEC_STREAM_PARAMETER_CONTEXT, BufferDataTypeBase, AllocateFromOSMemory, 32, 0, true, true, sizeof(Mpeg2CodecStreamParameterContext_t)}
#endif

static BufferDataDescriptor_t            Mpeg2CodecStreamParameterContextDescriptor = BUFFER_MPEG2_CODEC_STREAM_PARAMETER_CONTEXT_TYPE;

// --------

typedef struct Mpeg2CodecDecodeContext_s
{
    CodecBaseDecodeContext_t            BaseContext;

    MPEG2_TransformParam_t              DecodeParameters;
    MPEG2_CommandStatus_t               DecodeStatus;
} Mpeg2CodecDecodeContext_t;

//#if __KERNEL__
#if 0
#define BUFFER_MPEG2_CODEC_DECODE_CONTEXT       "Mpeg2CodecDecodeContext"
#define BUFFER_MPEG2_CODEC_DECODE_CONTEXT_TYPE  {BUFFER_MPEG2_CODEC_DECODE_CONTEXT, BufferDataTypeBase, AllocateFromDeviceMemory, 32, 0, true, true, sizeof(Mpeg2CodecDecodeContext_t)}
#else
#define BUFFER_MPEG2_CODEC_DECODE_CONTEXT       "Mpeg2CodecDecodeContext"
#define BUFFER_MPEG2_CODEC_DECODE_CONTEXT_TYPE  {BUFFER_MPEG2_CODEC_DECODE_CONTEXT, BufferDataTypeBase, AllocateFromOSMemory, 32, 0, true, true, sizeof(Mpeg2CodecDecodeContext_t)}
#endif

static BufferDataDescriptor_t            Mpeg2CodecDecodeContextDescriptor = BUFFER_MPEG2_CODEC_DECODE_CONTEXT_TYPE;



// /////////////////////////////////////////////////////////////////////////
//
//      Cosntructor function, fills in the codec specific parameter values
//

Codec_MmeVideoMpeg2_c::Codec_MmeVideoMpeg2_c( void )
{
    Configuration.CodecName                             = "Mpeg2 video";

    Configuration.DecodeOutputFormat                    = FormatVideo420_MacroBlock;

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

    Configuration.TrickModeParameters.EmpiricalMaximumDecodeFrameRateShortIntegration	= 60;
    Configuration.TrickModeParameters.EmpiricalMaximumDecodeFrameRateLongIntegration	= 60;

    Configuration.TrickModeParameters.SubstandardDecodeSupported        = false;
    Configuration.TrickModeParameters.SubstandardDecodeRateIncrease	= 1;

    Configuration.TrickModeParameters.DefaultGroupSize                  = 12;
    Configuration.TrickModeParameters.DefaultGroupReferenceFrameCount   = 4;

//

    Reset();
}


// /////////////////////////////////////////////////////////////////////////
//
//      Destructor function, ensures a full halt and reset 
//      are executed for all levels of the class.
//

Codec_MmeVideoMpeg2_c::~Codec_MmeVideoMpeg2_c( void )
{
    Halt();
    Reset();
}


// /////////////////////////////////////////////////////////////////////////
//
//      Function to deal with the returned capabilities 
//      structure for an mpeg2 mme transformer.
//

CodecStatus_t   Codec_MmeVideoMpeg2_c::HandleCapabilities( void )
{
    report( severity_info, "MME Transformer '%s' capabilities are :-\n", MPEG2_MME_TRANSFORMER_NAME );
    report( severity_info, "    StructSize                        = %d bytes\n", Mpeg2TransformCapability.StructSize );
    report( severity_info, "    MPEG1Capability                   = %d\n", Mpeg2TransformCapability.MPEG1Capability );
    report( severity_info, "    SupportedFrameChromaFormat        = %d\n", Mpeg2TransformCapability.SupportedFrameChromaFormat );
    report( severity_info, "    SupportedFieldChromaFormat        = %d\n", Mpeg2TransformCapability.SupportedFieldChromaFormat );
    report( severity_info, "    MaximumFieldDecodingLatency90KHz  = %d\n", Mpeg2TransformCapability.MaximumFieldDecodingLatency90KHz );

    return CodecNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Function to deal with the returned capabilities 
//      structure for an mpeg2 mme transformer.
//

CodecStatus_t   Codec_MmeVideoMpeg2_c::FillOutTransformerInitializationParameters( void )
{

    //
    // Fillout the command parameters
    //

    Mpeg2InitializationParameters.InputBufferBegin      = 0x00000000;
    Mpeg2InitializationParameters.InputBufferEnd        = 0xffffffff;

    //
    // Fillout the actual command
    //

    MMEInitializationParameters.TransformerInitParamsSize       = sizeof(MPEG2_InitTransformerParam_t);
    MMEInitializationParameters.TransformerInitParams_p         = (MME_GenericParams_t)(&Mpeg2InitializationParameters);

//

    return CodecNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Function to fill out the stream parameters 
//      structure for an mpeg2 mme transformer.
//

CodecStatus_t   Codec_MmeVideoMpeg2_c::FillOutSetStreamParametersCommand( void )
{
Mpeg2StreamParameters_t                 *Parsed         = (Mpeg2StreamParameters_t *)ParsedFrameParameters->StreamParameterStructure;
Mpeg2CodecStreamParameterContext_t      *Context        = (Mpeg2CodecStreamParameterContext_t *)StreamParameterContext;
unsigned char                           *intra_quantizer_matrix;
unsigned char                           *non_intra_quantizer_matrix;
unsigned char                           *chroma_intra_quantizer_matrix;
unsigned char                           *chroma_non_intra_quantizer_matrix;

    //
    // Fillout the command parameters
    //

    Context->StreamParameters.StructSize                = sizeof(MPEG2_SetGlobalParamSequence_t);

    Context->StreamParameters.MPEGStreamTypeFlag        = (Parsed->StreamType == MpegStreamTypeMpeg1) ? 0 : 1;

    Context->StreamParameters.horizontal_size           = Parsed->SequenceHeader.horizontal_size_value;
    Context->StreamParameters.vertical_size             = Parsed->SequenceHeader.vertical_size_value;
    Context->StreamParameters.progressive_sequence      = true;
    Context->StreamParameters.chroma_format             = MPEG2_CHROMA_4_2_0;

    if( Parsed->SequenceExtensionHeaderPresent )
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

    memcpy( Context->StreamParameters.intra_quantiser_matrix, intra_quantizer_matrix, sizeof(QuantiserMatrix_t) );
    memcpy( Context->StreamParameters.non_intra_quantiser_matrix, non_intra_quantizer_matrix, sizeof(QuantiserMatrix_t) );
    memcpy( Context->StreamParameters.chroma_intra_quantiser_matrix, chroma_intra_quantizer_matrix, sizeof(QuantiserMatrix_t) );
    memcpy( Context->StreamParameters.chroma_non_intra_quantiser_matrix, chroma_non_intra_quantizer_matrix, sizeof(QuantiserMatrix_t) );

    //
    // Fillout the actual command
    //

    memset( &Context->BaseContext.MMECommand, 0x00, sizeof(MME_Command_t) );

    Context->BaseContext.MMECommand.CmdStatus.AdditionalInfoSize        = 0;
    Context->BaseContext.MMECommand.CmdStatus.AdditionalInfo_p          = NULL;
    Context->BaseContext.MMECommand.ParamSize                           = sizeof(MPEG2_SetGlobalParamSequence_t);
    Context->BaseContext.MMECommand.Param_p                             = (MME_GenericParams_t)(&Context->StreamParameters);

//

    return CodecNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Function to fill out the decode parameters 
//      structure for an mpeg2 mme transformer.
//

CodecStatus_t   Codec_MmeVideoMpeg2_c::FillOutDecodeCommand(       void )
{
Mpeg2CodecDecodeContext_t       *Context        = (Mpeg2CodecDecodeContext_t *)DecodeContext;
Mpeg2FrameParameters_t          *Parsed         = (Mpeg2FrameParameters_t *)ParsedFrameParameters->FrameParameterStructure;
MPEG2_TransformParam_t          *Param;
MPEG2_DecodedBufferAddress_t    *Decode;
MPEG2_ParamPicture_t            *Picture;
MPEG2_RefPicListAddress_t       *RefList;
unsigned int                     Entry;

    //
    // For mpeg2 we do not do slice decodes.
    //

    KnownLastSliceInFieldFrame                  = true;

    //
    // Fillout the straight forward command parameters
    //

    Param                                       = &Context->DecodeParameters;
    Decode                                      = &Param->DecodedBufferAddress;
    Picture                                     = &Param->PictureParameters;
    RefList                                     = &Param->RefPicListAddress;

    Param->StructSize                           = sizeof(MPEG2_TransformParam_t);

    Param->PictureStartAddrCompressedBuffer_p   = (MPEG2_CompressedData_t)CodedData;
    Param->PictureStopAddrCompressedBuffer_p    = (MPEG2_CompressedData_t)(CodedData + CodedDataLength);

    Decode->StructSize                          = sizeof(MPEG2_DecodedBufferAddress_t);
    Decode->DecodedLuma_p                       = (MPEG2_LumaAddress_t)BufferState[CurrentDecodeBufferIndex].BufferLumaPointer;
    Decode->DecodedChroma_p                     = (MPEG2_ChromaAddress_t)BufferState[CurrentDecodeBufferIndex].BufferChromaPointer;
    Decode->DecodedTemporalReferenceValue       = Parsed->PictureHeader.temporal_reference;
    Decode->MBDescr_p                           = NULL;

    if (Player->PolicyValue( Playback, Stream, PolicyDecimateDecoderOutput) != PolicyValueDecimateDecoderOutputDisabled)
    {
         Decode->DecimatedLuma_p                     = (MPEG2_LumaAddress_t)BufferState[CurrentDecodeBufferIndex].DecimatedLumaPointer;
         Decode->DecimatedChroma_p                   = (MPEG2_ChromaAddress_t)BufferState[CurrentDecodeBufferIndex].DecimatedChromaPointer;
    }
    else
    {
         Decode->DecimatedLuma_p                     = NULL;
         Decode->DecimatedChroma_p                   = NULL;              
    }
   
    switch (Player->PolicyValue( Playback, Stream, PolicyDecimateDecoderOutput) )
    {	
         case PolicyValueDecimateDecoderOutputDisabled:
	 {  	    
	    // Normal Case
	    Param->MainAuxEnable                        = MPEG2_MAINOUT_EN;
	    Param->HorizontalDecimationFactor           = MPEG2_HDEC_1;
	    Param->VerticalDecimationFactor             = MPEG2_VDEC_1;
	    break;
	 }       
         case PolicyValueDecimateDecoderOutputHalf:
         {	    
	    Param->MainAuxEnable                        = MPEG2_AUX_MAIN_OUT_EN;
	    Param->HorizontalDecimationFactor           = MPEG2_HDEC_ADVANCED_2;
	    
	    if (Parsed->PictureCodingExtensionHeader.progressive_frame)
	      Param->VerticalDecimationFactor             = MPEG2_VDEC_ADVANCED_2_PROG;
	    else
	      Param->VerticalDecimationFactor             = MPEG2_VDEC_ADVANCED_2_INT;
	      
	    break;
	 }       
         case PolicyValueDecimateDecoderOutputQuarter:
	 {  
	    Param->MainAuxEnable                        = MPEG2_AUX_MAIN_OUT_EN;
	    Param->HorizontalDecimationFactor           = MPEG2_HDEC_ADVANCED_4;
	    
	    if (Parsed->PictureCodingExtensionHeader.progressive_frame)
	      Param->VerticalDecimationFactor             = MPEG2_VDEC_ADVANCED_2_PROG;
	    else
	      Param->VerticalDecimationFactor             = MPEG2_VDEC_ADVANCED_2_INT;
	    break;
	 }       
    }
      
    Param->DecodingMode                         = MPEG2_NORMAL_DECODE;
//    Param->DecodingMode                               = MPEG2_NORMAL_DECODE_WITHOUT_ERROR_RECOVERY;
    Param->AdditionalFlags                      = MPEG2_ADDITIONAL_FLAG_NONE;
    Picture->StructSize                         = sizeof(MPEG2_ParamPicture_t);
    Picture->picture_coding_type                = (MPEG2_PictureCodingType_t)Parsed->PictureHeader.picture_coding_type;

    if( Parsed->PictureCodingExtensionHeaderPresent )
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

	if( Picture->picture_structure == MPEG2_TOP_FIELD_TYPE )
	    Param->AdditionalFlags              = (BufferState[CurrentDecodeBufferIndex].ParsedVideoParameters->TopFieldFirst) ?
						   MPEG2_ADDITIONAL_FLAG_FIRST_FIELD : MPEG2_ADDITIONAL_FLAG_SECOND_FIELD;
	else if( Picture->picture_structure == MPEG2_BOTTOM_FIELD_TYPE )
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
    }


    //
    // Fillout the referece frame list stuff
    //

    RefList->StructSize                         = sizeof(MPEG2_RefPicListAddress_t);
    if( ParsedFrameParameters->NumberOfReferenceFrameLists != 0 )
    {
	if( DecodeContext->ReferenceFrameList[0].EntryCount > 0 )
	{
	    Entry       = DecodeContext->ReferenceFrameList[0].EntryIndicies[0];
	    RefList->ForwardReferenceLuma_p             = (MPEG2_LumaAddress_t)BufferState[Entry].BufferLumaPointer;
	    RefList->ForwardReferenceChroma_p           = (MPEG2_ChromaAddress_t)BufferState[Entry].BufferChromaPointer;
	    RefList->ForwardTemporalReferenceValue      = Decode->DecodedTemporalReferenceValue - 1;
	}
	if( DecodeContext->ReferenceFrameList[0].EntryCount > 1 )
	{
	    Entry       = DecodeContext->ReferenceFrameList[0].EntryIndicies[1];
	    RefList->BackwardReferenceLuma_p            = (MPEG2_LumaAddress_t)BufferState[Entry].BufferLumaPointer;
	    RefList->BackwardReferenceChroma_p          = (MPEG2_ChromaAddress_t)BufferState[Entry].BufferChromaPointer;
	    RefList->BackwardTemporalReferenceValue     = Decode->DecodedTemporalReferenceValue + 1;
	}
    }

    //
    // Fillout the actual command
    //

    memset( &Context->BaseContext.MMECommand, 0x00, sizeof(MME_Command_t) );

    Context->BaseContext.MMECommand.CmdStatus.AdditionalInfoSize        = sizeof(MPEG2_CommandStatus_t);
    Context->BaseContext.MMECommand.CmdStatus.AdditionalInfo_p          = (MME_GenericParams_t)(&Context->DecodeStatus);
    Context->BaseContext.MMECommand.ParamSize                           = sizeof(MPEG2_TransformParam_t);
    Context->BaseContext.MMECommand.Param_p                             = (MME_GenericParams_t)(&Context->DecodeParameters);

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
CodecStatus_t   Codec_MmeVideoMpeg2_c::ValidateDecodeContext( CodecBaseDecodeContext_t *Context )
{
    return CodecNoError;
}

// /////////////////////////////////////////////////////////////////////////
//
//      Function to dump out the set stream 
//      parameters from an mme command.
//

CodecStatus_t   Codec_MmeVideoMpeg2_c::DumpSetStreamParameters(         void    *Parameters )
{
MPEG2_SetGlobalParamSequence_t  *StreamParams;

//

    StreamParams    = (MPEG2_SetGlobalParamSequence_t *)Parameters;

    report( severity_info,  "AZA - STREAM PARAMS %08x\n", StreamParams );
    report( severity_info,  "AZA -       StructSize                           = %d\n", StreamParams->StructSize );
    report( severity_info,  "AZA -       MPEGStreamTypeFlag                   = %d\n", StreamParams->MPEGStreamTypeFlag );
    report( severity_info,  "AZA -       horizontal_size                      = %d\n", StreamParams->horizontal_size );
    report( severity_info,  "AZA -       vertical_size                        = %d\n", StreamParams->vertical_size );
    report( severity_info,  "AZA -       progressive_sequence                 = %d\n", StreamParams->progressive_sequence );
    report( severity_info,  "AZA -       chroma_format                        = %d\n", StreamParams->chroma_format );
    report( severity_info,  "AZA -       MatrixFlags                          = %d\n", StreamParams->MatrixFlags );
    report( severity_info,  "AZA -       intra_quantiser_matrix               = %02x %02x %02x %02x %02x %02x %02x %02x\n",
		   StreamParams->intra_quantiser_matrix[0], StreamParams->intra_quantiser_matrix[1], StreamParams->intra_quantiser_matrix[2], StreamParams->intra_quantiser_matrix[3],
		   StreamParams->intra_quantiser_matrix[4], StreamParams->intra_quantiser_matrix[5], StreamParams->intra_quantiser_matrix[6], StreamParams->intra_quantiser_matrix[7] );
    report( severity_info,  "AZA -       non_intra_quantiser_matrix           = %02x %02x %02x %02x %02x %02x %02x %02x\n",
		   StreamParams->non_intra_quantiser_matrix[0], StreamParams->non_intra_quantiser_matrix[1], StreamParams->non_intra_quantiser_matrix[2], StreamParams->non_intra_quantiser_matrix[3],
		   StreamParams->non_intra_quantiser_matrix[4], StreamParams->non_intra_quantiser_matrix[5], StreamParams->non_intra_quantiser_matrix[6], StreamParams->non_intra_quantiser_matrix[7] );
    report( severity_info,  "AZA -       chroma_intra_quantiser_matrix        = %02x %02x %02x %02x %02x %02x %02x %02x\n",
		   StreamParams->chroma_intra_quantiser_matrix[0], StreamParams->chroma_intra_quantiser_matrix[1], StreamParams->chroma_intra_quantiser_matrix[2], StreamParams->chroma_intra_quantiser_matrix[3],
		   StreamParams->chroma_intra_quantiser_matrix[4], StreamParams->chroma_intra_quantiser_matrix[5], StreamParams->chroma_intra_quantiser_matrix[6], StreamParams->chroma_intra_quantiser_matrix[7] );
    report( severity_info,  "AZA -       chroma_non_intra_quantiser_matrix    = %02x %02x %02x %02x %02x %02x %02x %02x\n",
		   StreamParams->chroma_non_intra_quantiser_matrix[0], StreamParams->chroma_non_intra_quantiser_matrix[1], StreamParams->chroma_non_intra_quantiser_matrix[2], StreamParams->chroma_non_intra_quantiser_matrix[3],
		   StreamParams->chroma_non_intra_quantiser_matrix[4], StreamParams->chroma_non_intra_quantiser_matrix[5], StreamParams->chroma_non_intra_quantiser_matrix[6], StreamParams->chroma_non_intra_quantiser_matrix[7] );

    return CodecNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Function to dump out the decode
//      parameters from an mme command.
//

CodecStatus_t   Codec_MmeVideoMpeg2_c::DumpDecodeParameters(            void    *Parameters )
{
MPEG2_TransformParam_t          *FrameParams;

//

    FrameParams     = (MPEG2_TransformParam_t *)Parameters;

    report( severity_info,  "AZA - FRAME PARAMS %08x\n", FrameParams );
    report( severity_info,  "AZA -       StructSize                           = %d\n", FrameParams->StructSize );
    report( severity_info,  "AZA -       PictureStartAddrCompressedBuffer_p   = %08x\n", FrameParams->PictureStartAddrCompressedBuffer_p );
    report( severity_info,  "AZA -       PictureStopAddrCompressedBuffer_p    = %08x\n", FrameParams->PictureStopAddrCompressedBuffer_p );
    report( severity_info,  "AZA -       DecodedBufferAddress\n" );
    report( severity_info,  "AZA -           StructSize                       = %d\n", FrameParams->DecodedBufferAddress.StructSize );
    report( severity_info,  "AZA -           DecodedLuma_p                    = %08x\n", FrameParams->DecodedBufferAddress.DecodedLuma_p );
    report( severity_info,  "AZA -           DecodedChroma_p                  = %08x\n", FrameParams->DecodedBufferAddress.DecodedChroma_p );
    report( severity_info,  "AZA -           DecodedTemporalReferenceValue    = %d\n",  FrameParams->DecodedBufferAddress.DecodedTemporalReferenceValue );
    report( severity_info,  "AZA -           DecimatedLuma_p                  = %08x\n", FrameParams->DecodedBufferAddress.DecimatedLuma_p );
    report( severity_info,  "AZA -           DecimatedChroma_p                = %08x\n", FrameParams->DecodedBufferAddress.DecimatedChroma_p );
    report( severity_info,  "AZA -           MBDescr_p                        = %08x\n", FrameParams->DecodedBufferAddress.MBDescr_p );
    report( severity_info,  "AZA -       RefPicListAddress\n" );
    report( severity_info,  "AZA -           StructSize                       = %d\n", FrameParams->RefPicListAddress.StructSize );
    report( severity_info,  "AZA -           BackwardReferenceLuma_p          = %08x\n", FrameParams->RefPicListAddress.BackwardReferenceLuma_p );
    report( severity_info,  "AZA -           BackwardReferenceChroma_p        = %08x\n", FrameParams->RefPicListAddress.BackwardReferenceChroma_p );
    report( severity_info,  "AZA -           BackwardTemporalReferenceValue   = %d\n", FrameParams->RefPicListAddress.BackwardTemporalReferenceValue );
    report( severity_info,  "AZA -           ForwardReferenceLuma_p           = %08x\n", FrameParams->RefPicListAddress.ForwardReferenceLuma_p );
    report( severity_info,  "AZA -           ForwardReferenceChroma_p         = %08x\n", FrameParams->RefPicListAddress.ForwardReferenceChroma_p );
    report( severity_info,  "AZA -           ForwardTemporalReferenceValue    = %d\n", FrameParams->RefPicListAddress.ForwardTemporalReferenceValue );
    report( severity_info,  "AZA -       MainAuxEnable                        = %08x\n", FrameParams->MainAuxEnable );
    report( severity_info,  "AZA -       HorizontalDecimationFactor           = %08x\n", FrameParams->HorizontalDecimationFactor );
    report( severity_info,  "AZA -       VerticalDecimationFactor             = %08x\n", FrameParams->VerticalDecimationFactor );
    report( severity_info,  "AZA -       DecodingMode                         = %d\n", FrameParams->DecodingMode );
    report( severity_info,  "AZA -       AdditionalFlags                      = %08x\n", FrameParams->AdditionalFlags );
    report( severity_info,  "AZA -       PictureParameters\n" );
    report( severity_info,  "AZA -           StructSize                       = %d\n", FrameParams->PictureParameters.StructSize );
    report( severity_info,  "AZA -           picture_coding_type              = %d\n", FrameParams->PictureParameters.picture_coding_type );
    report( severity_info,  "AZA -           forward_horizontal_f_code        = %d\n", FrameParams->PictureParameters.forward_horizontal_f_code );
    report( severity_info,  "AZA -           forward_vertical_f_code          = %d\n", FrameParams->PictureParameters.forward_vertical_f_code );
    report( severity_info,  "AZA -           backward_horizontal_f_code       = %d\n", FrameParams->PictureParameters.backward_horizontal_f_code );
    report( severity_info,  "AZA -           backward_vertical_f_code         = %d\n", FrameParams->PictureParameters.backward_vertical_f_code );
    report( severity_info,  "AZA -           intra_dc_precision               = %d\n", FrameParams->PictureParameters.intra_dc_precision );
    report( severity_info,  "AZA -           picture_structure                = %d\n", FrameParams->PictureParameters.picture_structure );
    report( severity_info,  "AZA -           mpeg_decoding_flags              = %08x\n", FrameParams->PictureParameters.mpeg_decoding_flags );

    return CodecNoError;
}

CodecStatus_t   Codec_MmeVideoMpeg2_c::CheckCodecReturnParameters( CodecBaseDecodeContext_t *Context )
{
  
  MME_Command_t             *MMECommand              = (MME_Command_t *)        (&Context->MMECommand);
  MME_CommandStatus_t       *CmdStatus               = (MME_CommandStatus_t *)  (&MMECommand->CmdStatus);
  MPEG2_CommandStatus_t     *AdditionalInfo_p        = (MPEG2_CommandStatus_t *)  CmdStatus->AdditionalInfo_p;
  
  if ( AdditionalInfo_p != NULL) {

    if( AdditionalInfo_p->ErrorCode != MPEG2_DECODER_NO_ERROR ) {

      switch ( AdditionalInfo_p->ErrorCode )
      {
	case MPEG2_DECODER_ERROR_MB_OVERFLOW:
	{
	  report( severity_info,  "Codec_MmeVideoMpeg2_c::CheckCodecReturnParameters - MPEG2_DECODER_ERROR_MB_OVERFLOW   %x \n" , AdditionalInfo_p->ErrorCode );
	  break;
	}
        case MPEG2_DECODER_ERROR_RECOVERED:
	{
	  report( severity_info,  "Codec_MmeVideoMpeg2_c::CheckCodecReturnParameters - MPEG2_DECODER_ERROR_RECOVERED     %x \n" , AdditionalInfo_p->ErrorCode );
	  break;
	}
        case MPEG2_DECODER_ERROR_NOT_RECOVERED:
	{
	  report( severity_info,  "Codec_MmeVideoMpeg2_c::CheckCodecReturnParameters - MPEG2_DECODER_ERROR_NOT_RECOVERED %x \n" , AdditionalInfo_p->ErrorCode );
	  break;
	}
        case MPEG2_DECODER_ERROR_TASK_TIMEOUT:
	{
	  report( severity_info,  "Codec_MmeVideoMpeg2_c::CheckCodecReturnParameters - MPEG2_DECODER_ERROR_TASK_TIMEOUT  %x \n" , AdditionalInfo_p->ErrorCode );
	  break;
	}
      }
    }
  }

  return CodecNoError;
}
