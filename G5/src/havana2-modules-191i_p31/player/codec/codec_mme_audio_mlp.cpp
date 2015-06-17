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

Source file name : codec_mme_audio_mlp.cpp
Author :           Sylvain

Implementation of the mlp audio codec class for player 2.


Date        Modification                                    Name
----        ------------                                    --------
11-Oct-07   Created                                         Sylvain

************************************************************************/

////////////////////////////////////////////////////////////////////////////
/// \class Codec_MmeAudioMlp_c
///
/// The MLP audio codec proxy.
///

// /////////////////////////////////////////////////////////////////////
//
//      Include any component headers

#include "codec_mme_audio_mlp.h"
#include "mlp.h"

// /////////////////////////////////////////////////////////////////////////
//
// Locally defined constants
//

#ifdef ENABLE_CODEC_DEBUG
#undef ENABLE_CODEC_DEBUG
#define ENABLE_CODEC_DEBUG 0
#endif

// /////////////////////////////////////////////////////////////////////////
//
// Locally defined structures
//

typedef struct MlpAudioCodecStreamParameterContext_s
{
    CodecBaseStreamParameterContext_t   BaseContext;

    MME_LxAudioDecoderGlobalParams_t StreamParameters;
} MlpAudioCodecStreamParameterContext_t;

//#if __KERNEL__
#if 0
#define BUFFER_MLP_AUDIO_CODEC_STREAM_PARAMETER_CONTEXT        "MlpAudioCodecStreamParameterContext"
#define BUFFER_MLP_AUDIO_CODEC_STREAM_PARAMETER_CONTEXT_TYPE   {BUFFER_MLP_AUDIO_CODEC_STREAM_PARAMETER_CONTEXT, BufferDataTypeBase, AllocateFromDeviceMemory, 32, 0, true, true, sizeof(MlpAudioCodecStreamParameterContext_t)}
#else
#define BUFFER_MLP_AUDIO_CODEC_STREAM_PARAMETER_CONTEXT        "MlpAudioCodecStreamParameterContext"
#define BUFFER_MLP_AUDIO_CODEC_STREAM_PARAMETER_CONTEXT_TYPE   {BUFFER_MLP_AUDIO_CODEC_STREAM_PARAMETER_CONTEXT, BufferDataTypeBase, AllocateFromOSMemory, 32, 0, true, true, sizeof(MlpAudioCodecStreamParameterContext_t)}
#endif

static BufferDataDescriptor_t            MlpAudioCodecStreamParameterContextDescriptor = BUFFER_MLP_AUDIO_CODEC_STREAM_PARAMETER_CONTEXT_TYPE;

// --------

typedef struct MlpAudioCodecDecodeContext_s
{
    CodecBaseDecodeContext_t            BaseContext;

    MME_LxAudioDecoderFrameParams_t     DecodeParameters;
    MME_LxAudioDecoderFrameStatus_t     DecodeStatus;
} MlpAudioCodecDecodeContext_t;

//#if __KERNEL__
#if 0
#define BUFFER_MLP_AUDIO_CODEC_DECODE_CONTEXT          "MlpAudioCodecDecodeContext"
#define BUFFER_MLP_AUDIO_CODEC_DECODE_CONTEXT_TYPE     {BUFFER_MLP_AUDIO_CODEC_DECODE_CONTEXT, BufferDataTypeBase, AllocateFromDeviceMemory, 32, 0, true, true, sizeof(MlpAudioCodecDecodeContext_t)}
#else
#define BUFFER_MLP_AUDIO_CODEC_DECODE_CONTEXT          "MlpAudioCodecDecodeContext"
#define BUFFER_MLP_AUDIO_CODEC_DECODE_CONTEXT_TYPE     {BUFFER_MLP_AUDIO_CODEC_DECODE_CONTEXT, BufferDataTypeBase, AllocateFromOSMemory, 32, 0, true, true, sizeof(MlpAudioCodecDecodeContext_t)}
#endif

static BufferDataDescriptor_t            MlpAudioCodecDecodeContextDescriptor = BUFFER_MLP_AUDIO_CODEC_DECODE_CONTEXT_TYPE;



////////////////////////////////////////////////////////////////////////////
///
/// Fill in the configuration parameters used by the super-class and reset everything.
///
Codec_MmeAudioMlp_c::Codec_MmeAudioMlp_c( void )
{
    Configuration.CodecName                             = "MLP audio";

    Configuration.StreamParameterContextCount           = 1;
    Configuration.StreamParameterContextDescriptor      = &MlpAudioCodecStreamParameterContextDescriptor;

    Configuration.DecodeContextCount                    = 4;
    Configuration.DecodeContextDescriptor               = &MlpAudioCodecDecodeContextDescriptor;
//

    AudioDecoderTransformCapabilityMask.DecoderCapabilityFlags = (1 << ACC_DOLBY_TRUEHD_LOSSLESS);

    DecoderId = ACC_TRUEHD_ID;

    Reset();
}


////////////////////////////////////////////////////////////////////////////
///
///     Destructor function, ensures a full halt and reset 
///     are executed for all levels of the class.
///
Codec_MmeAudioMlp_c::~Codec_MmeAudioMlp_c( void )
{
    Halt();
    Reset();
}

////////////////////////////////////////////////////////////////////////////
///
/// Populate the supplied structure with parameters for MLP audio.
///
///
CodecStatus_t Codec_MmeAudioMlp_c::FillOutTransformerGlobalParameters( MME_LxAudioDecoderGlobalParams_t *GlobalParams_p )
{

//

    CODEC_TRACE("Initializing MLP audio decoder\n");

//

    MME_LxAudioDecoderGlobalParams_t &GlobalParams = *GlobalParams_p;
    int NbAccessUnits = 0;

    if (ParsedFrameParameters != NULL)
    {
      MlpAudioStreamParameters_t *Parsed = (MlpAudioStreamParameters_t *)ParsedFrameParameters->StreamParameterStructure;
      NbAccessUnits = Parsed->AccumulatedFrameNumber;
    }

    GlobalParams.StructSize = sizeof(MME_LxAudioDecoderGlobalParams_t);

//

    MME_LxTruehdConfig_t &Config = *((MME_LxTruehdConfig_t *) GlobalParams.DecConfig);
    Config.DecoderId = DecoderId;
    Config.StructSize = sizeof(Config);
    Config.Config[TRUEHD_BITFIELD_FEATURES] = 0; // future use
    Config.Config[TRUEHD_DRC_ENABLE] = ACC_MME_TRUE;  // dynamic range compression
    Config.Config[TRUEHD_PP_ENABLE] = ACC_MME_FALSE;
    Config.Config[TRUEHD_DIALREF] = MLP_CODEC_DIALREF_DEFAULT_VALUE;
    Config.Config[TRUEHD_LDR] = MLP_CODEC_LDR_DEFAULT_VALUE;
    Config.Config[TRUEHD_HDR] = MLP_CODEC_HDR_DEFAULT_VALUE;
    Config.Config[TRUEHD_SERIAL_ACCESS_UNITS] = NbAccessUnits;

//

    return Codec_MmeAudio_c::FillOutTransformerGlobalParameters( GlobalParams_p );
}


////////////////////////////////////////////////////////////////////////////
///
/// Populate the AUDIO_DECODER's initialization parameters for MLP audio.
///
/// When this method completes Codec_MmeAudio_c::AudioDecoderInitializationParameters
/// will have been filled out with valid values sufficient to initialize an
/// MLP audio decoder (defaults to MLP Layer II but can be updated by new
/// stream parameters).
///
CodecStatus_t   Codec_MmeAudioMlp_c::FillOutTransformerInitializationParameters( void )
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
/// Populate the AUDIO_DECODER's MME_SET_GLOBAL_TRANSFORMER_PARAMS parameters for MLP audio.
///
CodecStatus_t   Codec_MmeAudioMlp_c::FillOutSetStreamParametersCommand( void )
{
CodecStatus_t Status;
MlpAudioCodecStreamParameterContext_t  *Context = (MlpAudioCodecStreamParameterContext_t *)StreamParameterContext;

    //
    // Examine the parsed stream parameters and determine what type of codec to instanciate
    //


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

    Context->BaseContext.MMECommand.CmdStatus.AdditionalInfoSize        = 0;
    Context->BaseContext.MMECommand.CmdStatus.AdditionalInfo_p          = NULL;
    Context->BaseContext.MMECommand.ParamSize                           = sizeof(Context->StreamParameters);
    Context->BaseContext.MMECommand.Param_p                             = (MME_GenericParams_t)(&Context->StreamParameters);

//

    return CodecNoError;
}

////////////////////////////////////////////////////////////////////////////
///
/// Populate the AUDIO_DECODER's MME_TRANSFORM parameters for MLP audio.
///
CodecStatus_t   Codec_MmeAudioMlp_c::FillOutDecodeCommand(       void )
{
MlpAudioCodecDecodeContext_t   *Context        = (MlpAudioCodecDecodeContext_t *)DecodeContext;
//MlpAudioFrameParameters_t    *Parsed         = (MlpAudioFrameParameters_t *)ParsedFrameParameters->FrameParameterStructure;

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

    Context->BaseContext.MMECommand.CmdStatus.AdditionalInfoSize        = sizeof(Context->DecodeStatus);
    Context->BaseContext.MMECommand.CmdStatus.AdditionalInfo_p          = (MME_GenericParams_t)(&Context->DecodeStatus);
    Context->BaseContext.MMECommand.ParamSize                           = sizeof(Context->DecodeParameters);
    Context->BaseContext.MMECommand.Param_p                             = (MME_GenericParams_t)(&Context->DecodeParameters);

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
CodecStatus_t   Codec_MmeAudioMlp_c::ValidateDecodeContext( CodecBaseDecodeContext_t *Context )
{
MlpAudioCodecDecodeContext_t *DecodeContext = (MlpAudioCodecDecodeContext_t *) Context;
MME_LxAudioDecoderFrameStatus_t &Status = DecodeContext->DecodeStatus;
ParsedAudioParameters_t *AudioParameters;


    CODEC_DEBUG(">><<\n");

    if (ENABLE_CODEC_DEBUG) 
    {
        //DumpCommand(bufferIndex);
    }

    if (Status.DecStatus) 
    {
        CODEC_ERROR("MLP audio decode error (muted frame): 0x%x\n", Status.DecStatus);
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

    AudioParameters->Source.BitsPerSample = ((AUDIODEC_GET_OUTPUT_WS((&AudioDecoderInitializationParameters))== ACC_WS32)?32:16);
    //AudioParameters->Source.BitsPerSample = AudioOutputSurface->BitsPerSample;
    AudioParameters->Source.ChannelCount =  AudioOutputSurface->ChannelCount;
    //AudioParameters->Source.ChannelCount =  Codec_MmeAudio_c::GetNumberOfChannelsFromAudioConfiguration((enum eAccAcMode) Status.AudioMode);
    AudioParameters->Organisation = Status.AudioMode;
    AudioParameters->SampleCount = Status.NbOutSamples;

    enum eAccFsCode SamplingFreqCode = (enum eAccFsCode) Status.SamplingFreq;
    if (SamplingFreqCode < ACC_FS_reserved)
    {
      AudioParameters->Source.SampleRateHz = Codec_MmeAudio_c::ConvertCodecSamplingFreq(SamplingFreqCode);
    }
    else
    {
      AudioParameters->Source.SampleRateHz = 0;
      CODEC_ERROR("LPCM audio decode bad sampling freq returned: 0x%x\n", SamplingFreqCode);
    }

    return CodecNoError;
}

// /////////////////////////////////////////////////////////////////////////
//
//      Function to dump out the set stream 
//      parameters from an mme command.
//

CodecStatus_t   Codec_MmeAudioMlp_c::DumpSetStreamParameters(          void    *Parameters )
{
    CODEC_ERROR("Not implemented\n");  
    return CodecNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Function to dump out the decode
//      parameters from an mme command.
//

CodecStatus_t   Codec_MmeAudioMlp_c::DumpDecodeParameters(             void    *Parameters )
{
    CODEC_ERROR("Not implemented\n");  
    return CodecNoError;
}
