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

Source file name : codec_mme_audio_aac.cpp
Author :           Adam

Implementation of the mpeg2 audio codec class for player 2.


Date        Modification                                    Name
----        ------------                                    --------
06-Jul-07   Created (from codec_mme_audio_mpeg.cpp)         Adam

************************************************************************/

////////////////////////////////////////////////////////////////////////////
/// \class Codec_MmeAudioAac_c
///
/// The AAC audio codec proxy.
///

// /////////////////////////////////////////////////////////////////////
//
//	Include any component headers

#include "codec_mme_audio_aac.h"
#include "aac_audio.h"

// /////////////////////////////////////////////////////////////////////////
//
// Locally defined constants
//

// /////////////////////////////////////////////////////////////////////////
//
// Locally defined structures
//

typedef struct AacAudioCodecStreamParameterContext_s
{
    CodecBaseStreamParameterContext_t	BaseContext;

    MME_LxAudioDecoderGlobalParams_t StreamParameters;
} AacAudioCodecStreamParameterContext_t;

//#if __KERNEL__
#if 0
#define BUFFER_AAC_AUDIO_CODEC_STREAM_PARAMETER_CONTEXT		"AacAudioCodecStreamParameterContext"
#define BUFFER_AAC_AUDIO_CODEC_STREAM_PARAMETER_CONTEXT_TYPE	{BUFFER_AAC_AUDIO_CODEC_STREAM_PARAMETER_CONTEXT, BufferDataTypeBase, AllocateFromDeviceMemory, 32, 0, true, true, sizeof(AacAudioCodecStreamParameterContext_t)}
#else
#define BUFFER_AAC_AUDIO_CODEC_STREAM_PARAMETER_CONTEXT		"AacAudioCodecStreamParameterContext"
#define BUFFER_AAC_AUDIO_CODEC_STREAM_PARAMETER_CONTEXT_TYPE	{BUFFER_AAC_AUDIO_CODEC_STREAM_PARAMETER_CONTEXT, BufferDataTypeBase, AllocateFromOSMemory, 32, 0, true, true, sizeof(AacAudioCodecStreamParameterContext_t)}
#endif

static BufferDataDescriptor_t		 AacAudioCodecStreamParameterContextDescriptor = BUFFER_AAC_AUDIO_CODEC_STREAM_PARAMETER_CONTEXT_TYPE;

// --------

typedef struct AacAudioCodecDecodeContext_s
{
    CodecBaseDecodeContext_t		BaseContext;

    MME_LxAudioDecoderFrameParams_t     DecodeParameters;
    MME_LxAudioDecoderFrameStatus_t     DecodeStatus;
} AacAudioCodecDecodeContext_t;

//#if __KERNEL__
#if 0
#define BUFFER_AAC_AUDIO_CODEC_DECODE_CONTEXT	"AacAudioCodecDecodeContext"
#define BUFFER_AAC_AUDIO_CODEC_DECODE_CONTEXT_TYPE	{BUFFER_AAC_AUDIO_CODEC_DECODE_CONTEXT, BufferDataTypeBase, AllocateFromDeviceMemory, 32, 0, true, true, sizeof(AacAudioCodecDecodeContext_t)}
#else
#define BUFFER_AAC_AUDIO_CODEC_DECODE_CONTEXT	"AacAudioCodecDecodeContext"
#define BUFFER_AAC_AUDIO_CODEC_DECODE_CONTEXT_TYPE	{BUFFER_AAC_AUDIO_CODEC_DECODE_CONTEXT, BufferDataTypeBase, AllocateFromOSMemory, 32, 0, true, true, sizeof(AacAudioCodecDecodeContext_t)}
#endif

static BufferDataDescriptor_t		 AacAudioCodecDecodeContextDescriptor = BUFFER_AAC_AUDIO_CODEC_DECODE_CONTEXT_TYPE;



////////////////////////////////////////////////////////////////////////////
///
/// Fill in the configuration parameters used by the super-class and reset everything.
///
Codec_MmeAudioAac_c::Codec_MmeAudioAac_c( void )
{
    Configuration.CodecName				= "AAC audio";

    Configuration.StreamParameterContextCount		= 1;
    Configuration.StreamParameterContextDescriptor	= &AacAudioCodecStreamParameterContextDescriptor;

    Configuration.DecodeContextCount			= 4;
    Configuration.DecodeContextDescriptor		= &AacAudioCodecDecodeContextDescriptor;

//

    AudioDecoderTransformCapabilityMask.DecoderCapabilityFlags = (1 << ACC_MP4a_AAC);

    DecoderId                                           = ACC_MP4A_AAC_ID;

    Reset();
}


////////////////////////////////////////////////////////////////////////////
///
/// 	Destructor function, ensures a full halt and reset 
///	are executed for all levels of the class.
///
Codec_MmeAudioAac_c::~Codec_MmeAudioAac_c( void )
{
    Halt();
    Reset();
}

////////////////////////////////////////////////////////////////////////////
///
/// Populate the supplied structure with parameters for AAC audio.
///
///
CodecStatus_t Codec_MmeAudioAac_c::FillOutTransformerGlobalParameters( MME_LxAudioDecoderGlobalParams_t *GlobalParams_p )
{

//

    CODEC_TRACE("Initializing AAC audio decoder\n");

//

    MME_LxAudioDecoderGlobalParams_t &GlobalParams = *GlobalParams_p;
    GlobalParams.StructSize = sizeof(MME_LxAudioDecoderGlobalParams_t);

//
    MME_LxMp2aConfig_t &Config = *((MME_LxMp2aConfig_t *) GlobalParams.DecConfig);
    Config.DecoderId = DecoderId;
    Config.StructSize = sizeof(Config);
    Config.Config[AAC_CRC_ENABLE] = ACC_MME_TRUE;
    Config.Config[AAC_DRC_ENABLE] = ACC_MME_FALSE;// dynamic range compression
    Config.Config[AAC_SBR_ENABLE] = ACC_MME_TRUE; // interpret sbr when possible
    if (ParsedFrameParameters != NULL)
    {
        Config.Config[AAC_FORMAT_TYPE] = ((AacAudioFrameParameters_s *)ParsedFrameParameters->FrameParameterStructure)->Type;
    }
    else
    {
        Config.Config[AAC_FORMAT_TYPE] = AAC_ADTS_FORMAT;
    }
//

    return Codec_MmeAudio_c::FillOutTransformerGlobalParameters( GlobalParams_p );
}


////////////////////////////////////////////////////////////////////////////
///
/// Populate the AUDIO_DECODER's initialization parameters for MPEG audio.
///
/// When this method completes Codec_MmeAudio_c::AudioDecoderInitializationParameters
/// will have been filled out with valid values sufficient to initialize an
/// MPEG audio decoder (defaults to MPEG Layer II but can be updated by new
/// stream parameters).
///
CodecStatus_t   Codec_MmeAudioAac_c::FillOutTransformerInitializationParameters( void )
{
CodecStatus_t Status;
MME_LxAudioDecoderInitParams_t &Params = AudioDecoderInitializationParameters;

//

    MMEInitializationParameters.TransformerInitParamsSize = sizeof(Params);
    MMEInitializationParameters.TransformerInitParams_p = &Params;

//

    Status = Codec_MmeAudio_c::FillOutTransformerInitializationParameters();
    if (Status != CodecNoError)
        return Status;

//

    return FillOutTransformerGlobalParameters( &Params.GlobalParams );
}


////////////////////////////////////////////////////////////////////////////
///
/// Populate the AUDIO_DECODER's MME_SET_GLOBAL_TRANSFORMER_PARAMS parameters for AAC audio.
///
CodecStatus_t   Codec_MmeAudioAac_c::FillOutSetStreamParametersCommand( void )
{
CodecStatus_t Status;
AacAudioCodecStreamParameterContext_t	*Context = (AacAudioCodecStreamParameterContext_t *)StreamParameterContext;
//AacAudioStreamParameters_t *Parsed = (AacAudioStreamParameters_t *)ParsedFrameParameters->StreamParameterStructure;
    
    //
    // Examine the parsed stream parameters and determine what type of codec to instanciate
    //
    
    DecoderId = ACC_MP4A_AAC_ID;
    
    //
    // Now fill out the actual structure
    //     
    
    memset( &(Context->StreamParameters), 0, sizeof(Context->StreamParameters) );
    Status = FillOutTransformerGlobalParameters( &(Context->StreamParameters) );
    if( Status != CodecNoError )
        return Status;

    //
    // Fillout the actual command
    //

    Context->BaseContext.MMECommand.CmdStatus.AdditionalInfoSize	= 0;
    Context->BaseContext.MMECommand.CmdStatus.AdditionalInfo_p		= NULL;
    Context->BaseContext.MMECommand.ParamSize				= sizeof(Context->StreamParameters);
    Context->BaseContext.MMECommand.Param_p				= (MME_GenericParams_t)(&Context->StreamParameters);

//

    return CodecNoError;
}

////////////////////////////////////////////////////////////////////////////
///
/// Populate the AUDIO_DECODER's MME_TRANSFORM parameters for MPEG audio.
///
CodecStatus_t   Codec_MmeAudioAac_c::FillOutDecodeCommand(       void )
{
AacAudioCodecDecodeContext_t	*Context 	= (AacAudioCodecDecodeContext_t *)DecodeContext;
//AacAudioFrameParameters_t	*Parsed		= (AacAudioFrameParameters_t *)ParsedFrameParameters->FrameParameterStructure;

    //
    // Initialize the frame parameters (we don't actually have much to say here)
    //
    
    memset( &Context->DecodeParameters, 0, sizeof(Context->DecodeParameters) );

    //
    // Zero the reply structure
    //
    
    memset( &Context->DecodeStatus, 0, sizeof(Context->DecodeStatus) );

    //
    // Fillout the actual command
    //

    Context->BaseContext.MMECommand.CmdStatus.AdditionalInfoSize	= sizeof(Context->DecodeStatus);
    Context->BaseContext.MMECommand.CmdStatus.AdditionalInfo_p		= (MME_GenericParams_t)(&Context->DecodeStatus);
    Context->BaseContext.MMECommand.ParamSize				= sizeof(Context->DecodeParameters);
    Context->BaseContext.MMECommand.Param_p				= (MME_GenericParams_t)(&Context->DecodeParameters);

    return CodecNoError;
}

////////////////////////////////////////////////////////////////////////////
///
/// Validate the ACC status structure and squawk loudly if problems are found.
/// 
/// Dispite the squawking this method unconditionally returns success. This is
/// because the firmware will already have concealed the decode problems by
/// performing a soft mute.
///
/// \return CodecSuccess
///
CodecStatus_t   Codec_MmeAudioAac_c::ValidateDecodeContext( CodecBaseDecodeContext_t *Context )
{
AacAudioCodecDecodeContext_t *DecodeContext = (AacAudioCodecDecodeContext_t *) Context;
MME_LxAudioDecoderFrameStatus_t &Status = DecodeContext->DecodeStatus;
ParsedAudioParameters_t *AudioParameters;


    CODEC_DEBUG(">><<\n");
 
    if (ENABLE_CODEC_DEBUG) {
        //DumpCommand(bufferIndex);
    }
    
    if (Status.DecStatus != ACC_MPEG2_OK) {
        CODEC_ERROR("AAC audio decode error (muted frame): %d\n", Status.DecStatus);
        //DumpCommand(bufferIndex);
        // don't report an error to the higher levels (because the frame is muted)
    }

    // SYSFS
    AudioDecoderStatus = Status;

    //
    // Attach any codec derived metadata to the output buffer (or verify the
    // frame analysis if the frame analyser already filled everything in for
    // us).
    //

    AudioParameters = BufferState[DecodeContext->BaseContext.BufferIndex].ParsedAudioParameters;
    
    AudioParameters->Source.BitsPerSample = AudioOutputSurface->BitsPerSample;
    AudioParameters->Source.ChannelCount = AudioOutputSurface->ChannelCount;
    AudioParameters->Organisation = Status.AudioMode;
    
    AudioParameters->SampleCount = Status.NbOutSamples;

    int SamplingFreqCode = Status.SamplingFreq;
    if (SamplingFreqCode < ACC_FS_reserved)
	{
		AudioParameters->Source.SampleRateHz = ACC_SamplingFreqLUT[SamplingFreqCode];
	}
    else
	{
		AudioParameters->Source.SampleRateHz = 0;
        CODEC_ERROR("DTSHD audio decode bad sampling freq returned: 0x%x\n", SamplingFreqCode);
	}
	

    return CodecNoError;
}

// /////////////////////////////////////////////////////////////////////////
//
// 	Function to dump out the set stream 
//	parameters from an mme command.
//

CodecStatus_t   Codec_MmeAudioAac_c::DumpSetStreamParameters(		void	*Parameters )
{
    CODEC_ERROR("Not implemented\n");  
    return CodecNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
// 	Function to dump out the decode
//	parameters from an mme command.
//

CodecStatus_t   Codec_MmeAudioAac_c::DumpDecodeParameters( 		void	*Parameters )
{
    CODEC_ERROR("Not implemented\n");  
    return CodecNoError;
}
