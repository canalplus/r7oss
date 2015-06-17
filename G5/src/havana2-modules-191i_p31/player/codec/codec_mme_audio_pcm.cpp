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

Source file name : codec_mme_audio_pcm.cpp
Author :           Julian

Implementation of the pcm audio codec class for player 2.


Date            Modification            Name
----            ------------            --------
12-Aug-09       Created                 Julian

************************************************************************/

////////////////////////////////////////////////////////////////////////////
/// \class Codec_MmeAudioPcm_c
///
///

// /////////////////////////////////////////////////////////////////////
//
//      Include any component headers

#include "codec_mme_audio_pcm.h"
#include "player_generic.h"

// /////////////////////////////////////////////////////////////////////////
//
// Locally defined constants
//
// /////////////////////////////////////////////////////////////////////////
//
// Locally defined structures
//

typedef struct PcmAudioCodecStreamParameterContext_s
{
    CodecBaseStreamParameterContext_t   BaseContext;

    MME_LxAudioDecoderGlobalParams_t StreamParameters;
} PcmAudioCodecStreamParameterContext_t;

#define BUFFER_PCM_AUDIO_CODEC_STREAM_PARAMETER_CONTEXT         "PcmAudioCodecStreamParameterContext"
#define BUFFER_PCM_AUDIO_CODEC_STREAM_PARAMETER_CONTEXT_TYPE    {BUFFER_PCM_AUDIO_CODEC_STREAM_PARAMETER_CONTEXT, BufferDataTypeBase, AllocateFromOSMemory, 32, 0, true, true, sizeof(PcmAudioCodecStreamParameterContext_t)}

static BufferDataDescriptor_t           PcmAudioCodecStreamParameterContextDescriptor   = BUFFER_PCM_AUDIO_CODEC_STREAM_PARAMETER_CONTEXT_TYPE;

typedef struct PcmAudioCodecDecodeContext_s
{
    CodecBaseDecodeContext_t            BaseContext;

    MME_LxAudioDecoderFrameParams_t     DecodeParameters;
    MME_LxAudioDecoderFrameStatus_t     DecodeStatus;
} PcmAudioCodecDecodeContext_t;

#define BUFFER_PCM_AUDIO_CODEC_DECODE_CONTEXT                   "PcmAudioCodecDecodeContext"
#define BUFFER_PCM_AUDIO_CODEC_DECODE_CONTEXT_TYPE              {BUFFER_PCM_AUDIO_CODEC_DECODE_CONTEXT, BufferDataTypeBase, AllocateFromOSMemory, 32, 0, true, true, sizeof(PcmAudioCodecDecodeContext_t)}

static BufferDataDescriptor_t           PcmAudioCodecDecodeContextDescriptor    = BUFFER_PCM_AUDIO_CODEC_DECODE_CONTEXT_TYPE;


//{{{  Constructor
////////////////////////////////////////////////////////////////////////////
///
/// Fill in the configuration parameters used by the super-class and reset everything.
///
Codec_MmeAudioPcm_c::Codec_MmeAudioPcm_c( void )
{
    Configuration.CodecName                             = "Pcm transcoder";

    Configuration.StreamParameterContextCount           = 1;
    Configuration.StreamParameterContextDescriptor      = &PcmAudioCodecStreamParameterContextDescriptor;

    Configuration.DecodeContextCount                    = 4;
    Configuration.DecodeContextDescriptor               = &PcmAudioCodecDecodeContextDescriptor;

    for (int i =0; i< CODEC_MAX_TRANSFORMERS; i++)
        Configuration.TransformName[i]                  = PCM_MME_TRANSFORMER_NAME;
    Configuration.AvailableTransformers                 = CODEC_MAX_TRANSFORMERS;

    Configuration.AddressingMode                        = CachedAddress;

    RestartTransformer                                  = ACC_MME_TRUE;

    Reset();
}
//}}}
//{{{  Destructor
////////////////////////////////////////////////////////////////////////////
///
///     Destructor function, ensures a full halt and reset 
///     are executed for all levels of the class.
///
Codec_MmeAudioPcm_c::~Codec_MmeAudioPcm_c( void )
{
    Halt();
    Reset();
}
//}}}
//{{{  HandleCapabilities
////////////////////////////////////////////////////////////////////////////
///
/// Examine the capability structure returned by the firmware.
///
/// Unconditionally return success; the silence generator does not report
/// anything other than a version number. 
///
CodecStatus_t   Codec_MmeAudioPcm_c::HandleCapabilities( void )
{
    return CodecNoError;
}

//}}}

//{{{  FillOutTransformerGlobalParameters
////////////////////////////////////////////////////////////////////////////
///
/// Populate the supplied structure with parameters for PCM audio.
///
///
CodecStatus_t Codec_MmeAudioPcm_c::FillOutTransformerGlobalParameters( MME_LxAudioDecoderGlobalParams_t *GlobalParams_p )
{
    MME_LxAudioDecoderGlobalParams_t   &GlobalParams    = *GlobalParams_p;

    CODEC_TRACE ("Initializing PCM audio decoder\n");

    GlobalParams.StructSize             = sizeof(MME_LxAudioDecoderGlobalParams_t);

    MME_LxPcmAudioConfig_t &Config      = *((MME_LxPcmAudioConfig_t*)GlobalParams.DecConfig);

    Config.DecoderId                    = ACC_PCM_ID;
    Config.StructSize                   = sizeof(Config);

    if (ParsedFrameParameters != NULL)
    {
        PcmAudioStreamParameters_s*     StreamParams    = (PcmAudioStreamParameters_s*)ParsedFrameParameters->StreamParameterStructure;

        Config.ChannelCount             = StreamParams->ChannelCount;
        Config.SampleRate               = StreamParams->SampleRate;
        Config.BytesPerSecond           = StreamParams->BytesPerSecond;
        Config.BlockAlign               = StreamParams->BlockAlign;
        Config.BitsPerSample            = StreamParams->BitsPerSample;
        Config.DataEndianness           = StreamParams->DataEndianness;
    }
    else
    {
        Config.ChannelCount             = 2;
        Config.SampleRate               = 44100;
        Config.BytesPerSecond           = 88200;
        Config.BlockAlign               = 4;
        Config.BitsPerSample            = 16;
        Config.DataEndianness           = PCM_LITTLE_ENDIAN;
    }
    RestartTransformer                  = ACC_MME_TRUE;

    CODEC_TRACE("DecoderId                  %d (%x)\n", Config.DecoderId, Config.DecoderId);
    CODEC_TRACE("StructSize                 %d (%x)\n", Config.StructSize, Config.StructSize);
    CODEC_TRACE("Config.ChannelCount        %d (%x)\n", Config.ChannelCount, Config.ChannelCount);
    CODEC_TRACE("Config.SampleRate          %d (%x)\n", Config.SampleRate, Config.SampleRate);
    CODEC_TRACE("Config.BytesPerSecond      %d (%x)\n", Config.BytesPerSecond, Config.BytesPerSecond);
    CODEC_TRACE("Config.BlockAlign          %d (%x)\n", Config.BlockAlign, Config.BlockAlign);
    CODEC_TRACE("Config.BitsPerSample       %d (%x)\n", Config.BitsPerSample, Config.BitsPerSample);
    CODEC_TRACE("Config.DataEndianness      %d (%x)\n", Config.DataEndianness, Config.DataEndianness);

    return Codec_MmeAudio_c::FillOutTransformerGlobalParameters (GlobalParams_p);
}
//}}}
//{{{  FillOutTransformerInitializationParameters
////////////////////////////////////////////////////////////////////////////
///
/// Populate the AUDIO_DECODER's initialization parameters for PCM audio.
///
CodecStatus_t   Codec_MmeAudioPcm_c::FillOutTransformerInitializationParameters( void )
{
    CodecStatus_t                       Status;
    MME_LxAudioDecoderInitParams_t     &Params                  = AudioDecoderInitializationParameters;

    MMEInitializationParameters.TransformerInitParamsSize       = sizeof(Params);
    MMEInitializationParameters.TransformerInitParams_p         = &Params;

    Status                                                      = Codec_MmeAudio_c::FillOutTransformerInitializationParameters();
    if (Status != CodecNoError)
        return Status;

    return FillOutTransformerGlobalParameters (&Params.GlobalParams);
}
//}}}
//{{{  FillOutSetStreamParametersCommand
////////////////////////////////////////////////////////////////////////////
///
/// Populate the AUDIO_DECODER's MME_SET_GLOBAL_TRANSFORMER_PARAMS parameters for PCM audio.
///
CodecStatus_t   Codec_MmeAudioPcm_c::FillOutSetStreamParametersCommand( void )
{
    CodecStatus_t                               Status;
    PcmAudioCodecStreamParameterContext_t*      Context = (PcmAudioCodecStreamParameterContext_t *)StreamParameterContext;

    CODEC_TRACE("\n");
    // Fill out the structure
#if 0
    // There are no set stream parameters for Vp6 decoder so the transformer is
    // terminated and restarted when required (i.e. if width or height change).

    if (RestartTransformer)
    {
        TerminateMMETransformer();

        memset (&MMEInitializationParameters, 0x00, sizeof(MME_TransformerInitParams_t));

        MMEInitializationParameters.Priority                    = MME_PRIORITY_NORMAL;
        MMEInitializationParameters.StructSize                  = sizeof(MME_TransformerInitParams_t);
        MMEInitializationParameters.Callback                    = &MMECallbackStub;
        MMEInitializationParameters.CallbackUserData            = this;

        FillOutTransformerInitializationParameters ();

        MMEStatus               = MME_InitTransformer (Configuration.TransformName[SelectedTransformer],
                                                       &MMEInitializationParameters, &MMEHandle);

        if (MMEStatus ==  MME_SUCCESS)
        {
            CODEC_DEBUG ("New Stream Params %dx%d\n", DecodingWidth, DecodingHeight);
            CodecStatus                                         = CodecNoError;
            RestartTransformer                                  = eAccFalse;
            ParsedFrameParameters->NewStreamParameters          = false;

            MMEInitialized                                      = true;
        }
    }
#endif

    memset (&(Context->StreamParameters), 0, sizeof(Context->StreamParameters));
    Status              = FillOutTransformerGlobalParameters (&(Context->StreamParameters));
    if (Status != CodecNoError)
        return Status;

    // Fillout the actual command
    Context->BaseContext.MMECommand.CmdStatus.AdditionalInfoSize        = 0;
    Context->BaseContext.MMECommand.CmdStatus.AdditionalInfo_p          = NULL;
    Context->BaseContext.MMECommand.ParamSize                           = sizeof(Context->StreamParameters);
    Context->BaseContext.MMECommand.Param_p                             = (MME_GenericParams_t)(&Context->StreamParameters);

    return CodecNoError;
}
//}}}
//{{{  FillOutDecodeCommand
////////////////////////////////////////////////////////////////////////////
///
/// Populate the AUDIO_DECODER's MME_TRANSFORM parameters for PCM audio.
///
CodecStatus_t   Codec_MmeAudioPcm_c::FillOutDecodeCommand(       void )
{
    PcmAudioCodecDecodeContext_t    *Context                            = (PcmAudioCodecDecodeContext_t *)DecodeContext;

    CODEC_DEBUG ("%s: Initializing decode params\n", __FUNCTION__);

    // Initialize the frame parameters
    memset (&Context->DecodeParameters, 0, sizeof(Context->DecodeParameters));
    Context->DecodeParameters.Restart           = RestartTransformer;

    RestartTransformer                          = ACC_MME_FALSE;

    // Zero the reply structure
    memset( &Context->DecodeStatus, 0, sizeof(Context->DecodeStatus) );

    // Fillout the actual command
    Context->BaseContext.MMECommand.CmdStatus.AdditionalInfoSize        = sizeof(Context->DecodeStatus);
    Context->BaseContext.MMECommand.CmdStatus.AdditionalInfo_p          = (MME_GenericParams_t)(&Context->DecodeStatus);
    Context->BaseContext.MMECommand.ParamSize                           = sizeof(Context->DecodeParameters);
    Context->BaseContext.MMECommand.Param_p                             = (MME_GenericParams_t)(&Context->DecodeParameters);

    CODEC_DEBUG("Restart                    %d\n", Context->DecodeParameters.Restart);
    CODEC_DEBUG("AdditionalInfoSize         %d\n", Context->BaseContext.MMECommand.CmdStatus.AdditionalInfoSize);

    return CodecNoError;
}
//}}}
//{{{  ValidateDecodeContext
////////////////////////////////////////////////////////////////////////////
///
/// Validate the ACC status structure and squawk loudly if problems are found.
/// 
/// \return CodecSuccess
///
CodecStatus_t   Codec_MmeAudioPcm_c::ValidateDecodeContext( CodecBaseDecodeContext_t *Context )
{
    PcmAudioCodecDecodeContext_t*       DecodeContext   = (PcmAudioCodecDecodeContext_t*)Context;
    MME_LxAudioDecoderFrameStatus_t    &Status          = DecodeContext->DecodeStatus;
    ParsedAudioParameters_t*            AudioParameters;


    //CODEC_TRACE ("%s: DecStatus %d\n", __FUNCTION__, Status.DecStatus);

    if (ENABLE_CODEC_DEBUG)
    {
        //DumpCommand(bufferIndex);
    }

    if (Status.DecStatus != ACC_HXR_OK)
    {
        CODEC_ERROR("PCM audio decode error (muted frame): %d\n", Status.DecStatus);
        //DumpCommand(bufferIndex);
        // don't report an error to the higher levels (because the frame is muted)
    }

    // SYSFS
    AudioDecoderStatus                          = Status;

    //
    // Attach any codec derived metadata to the output buffer (or verify the
    // frame analysis if the frame analyser already filled everything in for
    // us).
    //

    AudioParameters                             = BufferState[DecodeContext->BaseContext.BufferIndex].ParsedAudioParameters;

    AudioParameters->Source.BitsPerSample       = AudioOutputSurface->BitsPerSample;
    AudioParameters->Source.ChannelCount        = AudioOutputSurface->ChannelCount;
    AudioParameters->Organisation               = Status.AudioMode;

    AudioParameters->SampleCount                = Status.NbOutSamples;

    int SamplingFreqCode                        = Status.SamplingFreq;
    if (SamplingFreqCode < ACC_FS_reserved)
    {
        AudioParameters->Source.SampleRateHz    = ACC_SamplingFreqLUT[SamplingFreqCode];
    }
    else
    {
        AudioParameters->Source.SampleRateHz    = 0;
        CODEC_ERROR("PCM audio decode bad sampling freq returned: 0x%x\n", SamplingFreqCode);
    }

    CODEC_DEBUG("AudioParameters->Source.BitsPerSample         %d\n", AudioParameters->Source.BitsPerSample);
    CODEC_DEBUG("AudioParameters->Source.ChannelCount          %d\n", AudioParameters->Source.ChannelCount);
    CODEC_DEBUG("AudioParameters->Organisation                 %d\n", AudioParameters->Organisation);
    CODEC_DEBUG("AudioParameters->SampleCount                  %d\n", AudioParameters->SampleCount);
    CODEC_DEBUG("AudioParameters->Source.SampleRateHz          %d\n", AudioParameters->Source.SampleRateHz);

    return CodecNoError;
}
//}}}
//{{{  DumpSetStreamParameters
// /////////////////////////////////////////////////////////////////////////
//
//      Function to dump out the set stream 
//      parameters from an mme command.
//

CodecStatus_t   Codec_MmeAudioPcm_c::DumpSetStreamParameters(           void    *Parameters )
{
    CODEC_ERROR("Not implemented\n");
    return CodecNoError;
}
//}}}
//{{{  DumpDecodeParameters
// /////////////////////////////////////////////////////////////////////////
//
//      Function to dump out the decode
//      parameters from an mme command.
//

CodecStatus_t   Codec_MmeAudioPcm_c::DumpDecodeParameters(              void    *Parameters )
{
    CODEC_TRACE("%s: TotalSize[0]                  %d\n", __FUNCTION__, DecodeContext->MMEBuffers[0].TotalSize);
    CODEC_TRACE("%s: Page_p[0]                     %p\n", __FUNCTION__, DecodeContext->MMEPages[0].Page_p);
    CODEC_TRACE("%s: TotalSize[1]                  %d\n", __FUNCTION__, DecodeContext->MMEBuffers[1].TotalSize);
    CODEC_TRACE("%s: Page_p[1]                     %p\n", __FUNCTION__, DecodeContext->MMEPages[1].Page_p);

    return CodecNoError;
}
//}}}
