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

////////////////////////////////////////////////////////////////////////////
/// \class Codec_MmeAudioPcm_c
///
///

#include "codec_mme_audio_pcm.h"
#include "player_generic.h"
#include "player_playback.h"
#include "player_stream.h"

#undef TRACE_TAG
#define TRACE_TAG "Codec_MmeAudioPcm_c"

typedef struct PcmAudioCodecStreamParameterContext_s
{
    CodecBaseStreamParameterContext_t   BaseContext;

    MME_LxAudioDecoderGlobalParams_t StreamParameters;
} PcmAudioCodecStreamParameterContext_t;

#define BUFFER_PCM_AUDIO_CODEC_STREAM_PARAMETER_CONTEXT         "PcmAudioCodecStreamParameterContext"
#define BUFFER_PCM_AUDIO_CODEC_STREAM_PARAMETER_CONTEXT_TYPE    {BUFFER_PCM_AUDIO_CODEC_STREAM_PARAMETER_CONTEXT, BufferDataTypeBase, AllocateFromOSMemory, 32, 0, true, true, sizeof(PcmAudioCodecStreamParameterContext_t)}

static BufferDataDescriptor_t  PcmAudioCodecStreamParameterContextDescriptor = BUFFER_PCM_AUDIO_CODEC_STREAM_PARAMETER_CONTEXT_TYPE;

typedef struct
{
    MME_LxAudioDecoderFrameStatus_t             DecStatus;
    MME_PcmProcessingFrameExtCommonStatus_t     PcmStatus;
} MME_LxAudioDecoderFrameExtendedPcmStatus_t;

typedef struct PcmAudioCodecDecodeContext_s
{
    CodecBaseDecodeContext_t                    BaseContext;

    MME_LxAudioDecoderFrameParams_t             DecodeParameters;
    MME_LxAudioDecoderFrameExtendedPcmStatus_t  DecodeStatus;
} PcmAudioCodecDecodeContext_t;

#define BUFFER_PCM_AUDIO_CODEC_DECODE_CONTEXT                   "PcmAudioCodecDecodeContext"
#define BUFFER_PCM_AUDIO_CODEC_DECODE_CONTEXT_TYPE              {BUFFER_PCM_AUDIO_CODEC_DECODE_CONTEXT, BufferDataTypeBase, AllocateFromOSMemory, 32, 0, true, true, sizeof(PcmAudioCodecDecodeContext_t)}

static BufferDataDescriptor_t  PcmAudioCodecDecodeContextDescriptor = BUFFER_PCM_AUDIO_CODEC_DECODE_CONTEXT_TYPE;

Codec_MmeAudioPcm_c::Codec_MmeAudioPcm_c(void)
    : Codec_MmeAudio_c()
    , RestartTransformer(ACC_MME_TRUE)
{
    Configuration.CodecName                             = "Pcm transcoder";
    Configuration.StreamParameterContextCount           = 1;
    Configuration.StreamParameterContextDescriptor      = &PcmAudioCodecStreamParameterContextDescriptor;
    Configuration.DecodeContextCount                    = 4;
    Configuration.DecodeContextDescriptor               = &PcmAudioCodecDecodeContextDescriptor;
    Configuration.TransformName[0]                      = PCM_MME_TRANSFORMER_NAME;
    Configuration.TransformName[1]                      = PCM_MME_TRANSFORMER_NAME;
    Configuration.AvailableTransformers                 = 2;
    Configuration.AddressingMode                        = CachedAddress;
    Configuration.MaximumSampleCount                    = PCM_TRANSCODER_MAX_DECODED_SAMPLE_COUNT;
}

Codec_MmeAudioPcm_c::~Codec_MmeAudioPcm_c(void)
{
    Halt();
}

//{{{  HandleCapabilities
////////////////////////////////////////////////////////////////////////////
///
/// Examine the capability structure returned by the firmware.
///
/// Unconditionally return success; the pcm decoder does not report
/// anything other than a version number.
///
CodecStatus_t   Codec_MmeAudioPcm_c::ParseCapabilities(unsigned int ActualTransformer)
{
    return CodecNoError;
}

CodecStatus_t   Codec_MmeAudioPcm_c::HandleCapabilities(unsigned int ActualTransformer)
{
    return CodecNoError;
}

CodecStatus_t   Codec_MmeAudioPcm_c::HandleCapabilities(void)
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
CodecStatus_t Codec_MmeAudioPcm_c::FillOutTransformerGlobalParameters(MME_LxAudioDecoderGlobalParams_t *GlobalParams_p)
{
    MME_LxAudioDecoderGlobalParams_t   &GlobalParams    = *GlobalParams_p;
    SE_INFO(group_decoder_audio, "Initializing PCM audio decoder\n");
    GlobalParams.StructSize             = sizeof(MME_LxAudioDecoderGlobalParams_t);
    MME_LxPcmAudioConfig_t &Config      = *((MME_LxPcmAudioConfig_t *)GlobalParams.DecConfig);
    Config.DecoderId                    = ACC_PCM_ID;
    Config.StructSize                   = sizeof(Config);

    if ((ParsedFrameParameters != NULL) && (ParsedFrameParameters->StreamParameterStructure != NULL))
    {
        PcmAudioStreamParameters_s     *StreamParams    = (PcmAudioStreamParameters_s *)ParsedFrameParameters->StreamParameterStructure;
        Config.ChannelCount             = StreamParams->ChannelCount;
        Config.SampleRate               = StreamParams->SampleRate;
        Config.BytesPerSecond           = StreamParams->BytesPerSecond;
        Config.BlockAlign               = StreamParams->BlockAlign;
        Config.BitsPerSample            = StreamParams->BitsPerSample;

        switch (StreamParams->CompressionCode)
        {
        case PCM_COMPRESSION_CODE_ALAW:
            Config.DataEndianness         = PCM_ALAW;
            break;

        case PCM_COMPRESSION_CODE_MULAW:
            Config.DataEndianness         = PCM_MULAW;
            break;

        case PCM_COMPRESSION_CODE_PCM:
        default:
            Config.DataEndianness         = StreamParams->DataEndianness;
            break;
        }
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

    if (SE_IS_DEBUG_ON(group_decoder_audio))
    {
        SE_DEBUG(group_decoder_audio, "DecoderId                  %d\n", Config.DecoderId);
        SE_DEBUG(group_decoder_audio, "StructSize                 %d\n", Config.StructSize);
        SE_DEBUG(group_decoder_audio, "Config.ChannelCount        %d\n", Config.ChannelCount);
        SE_DEBUG(group_decoder_audio, "Config.SampleRate          %d\n", Config.SampleRate);
        SE_DEBUG(group_decoder_audio, "Config.BytesPerSecond      %d\n", Config.BytesPerSecond);
        SE_DEBUG(group_decoder_audio, "Config.BlockAlign          %d\n", Config.BlockAlign);
        SE_DEBUG(group_decoder_audio, "Config.BitsPerSample       %d\n", Config.BitsPerSample);
        SE_DEBUG(group_decoder_audio, "Config.DataEndianness      %d\n", Config.DataEndianness);
    }

    return Codec_MmeAudio_c::FillOutTransformerGlobalParameters(GlobalParams_p);
}
//}}}
//{{{  FillOutTransformerInitializationParameters
////////////////////////////////////////////////////////////////////////////
///
/// Populate the AUDIO_DECODER's initialization parameters for PCM audio.
///
CodecStatus_t   Codec_MmeAudioPcm_c::FillOutTransformerInitializationParameters(void)
{
    CodecStatus_t                       Status;
    MME_LxAudioDecoderInitParams_t     &Params                  = AudioDecoderInitializationParameters;
    MMEInitializationParameters.TransformerInitParamsSize       = sizeof(Params);
    MMEInitializationParameters.TransformerInitParams_p         = &Params;
    Status                                                      = Codec_MmeAudio_c::FillOutTransformerInitializationParameters();

    if (Status != CodecNoError)
    {
        return Status;
    }

    return FillOutTransformerGlobalParameters(&Params.GlobalParams);
}
//}}}
//{{{  FillOutSetStreamParametersCommand
////////////////////////////////////////////////////////////////////////////
///
/// Populate the AUDIO_DECODER's MME_SET_GLOBAL_TRANSFORMER_PARAMS parameters for PCM audio.
///
CodecStatus_t   Codec_MmeAudioPcm_c::FillOutSetStreamParametersCommand(void)
{
    CodecStatus_t                               Status;
    PcmAudioCodecStreamParameterContext_t      *Context = (PcmAudioCodecStreamParameterContext_t *)StreamParameterContext;
    SE_VERBOSE(group_decoder_audio, "\n");
    // Fill out the structure
    memset(&(Context->StreamParameters), 0, sizeof(Context->StreamParameters));
    Status              = FillOutTransformerGlobalParameters(&(Context->StreamParameters));

    if (Status != CodecNoError)
    {
        return Status;
    }

    // Fill out the actual command
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
CodecStatus_t   Codec_MmeAudioPcm_c::FillOutDecodeCommand(void)
{
    PcmAudioCodecDecodeContext_t    *Context                            = (PcmAudioCodecDecodeContext_t *)DecodeContext;
    SE_VERBOSE(group_decoder_audio, "Initializing decode params\n");
    // Initialize the frame parameters
    memset(&Context->DecodeParameters, 0, sizeof(Context->DecodeParameters));

    bool mute = false;
    Player->GetControl(Playback, Stream, STM_SE_CTRL_PLAY_STREAM_MUTE, &mute);
    Context->DecodeParameters.Cmd = mute ? ACC_CMD_MUTE : ACC_CMD_PLAY;

    Context->DecodeParameters.Restart           = RestartTransformer;
    RestartTransformer                          = ACC_MME_FALSE;
    // Zero the reply structure
    memset(&Context->DecodeStatus, 0, sizeof(Context->DecodeStatus));
    // Fill out the actual command
    Context->BaseContext.MMECommand.CmdStatus.AdditionalInfoSize        = sizeof(Context->DecodeStatus);
    Context->BaseContext.MMECommand.CmdStatus.AdditionalInfo_p          = (MME_GenericParams_t)(&Context->DecodeStatus);
    Context->BaseContext.MMECommand.ParamSize                           = sizeof(Context->DecodeParameters);
    Context->BaseContext.MMECommand.Param_p                             = (MME_GenericParams_t)(&Context->DecodeParameters);
    SE_DEBUG(group_decoder_audio, "Restart                    %d\n", Context->DecodeParameters.Restart);
    SE_DEBUG(group_decoder_audio, "AdditionalInfoSize         %d\n", Context->BaseContext.MMECommand.CmdStatus.AdditionalInfoSize);
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
CodecStatus_t   Codec_MmeAudioPcm_c::ValidateDecodeContext(CodecBaseDecodeContext_t *Context)
{
    PcmAudioCodecDecodeContext_t       *DecodeContext ;
    MME_LxAudioDecoderFrameStatus_t    *Status;
    ParsedAudioParameters_t            *AudioParameters;
    AssertComponentState(ComponentRunning);

    if (Context == NULL)
    {
        SE_ERROR("(%s) - CodecContext is NULL\n", Configuration.CodecName);
        return CodecError;
    }

    if (AudioOutputSurface == NULL)
    {
        SE_ERROR("(%s) - AudioOutputSurface is NULL\n", Configuration.CodecName);
        return CodecError;
    }

    DecodeContext   = (PcmAudioCodecDecodeContext_t *)Context;
    Status          = &(DecodeContext->DecodeStatus.DecStatus);

    if (Status->DecStatus != ACC_HXR_OK)
    {
        SE_WARNING("PCM audio decode error (muted frame): %d\n", Status->DecStatus);
        // don't report an error to the higher levels (because the frame is muted)
    }

    // SYSFS
    AudioDecoderStatus = *Status;
    SetAudioCodecDecStatistics();
    //
    // Attach any codec derived metadata to the output buffer (or verify the
    // frame analysis if the frame analyser already filled everything in for
    // us).
    //
    OS_LockMutex(&Lock);
    AudioParameters                             = BufferState[DecodeContext->BaseContext.BufferIndex].ParsedAudioParameters;

    if (AudioParameters == NULL)
    {
        SE_ERROR("(%s) - AudioParameters are NULL\n", Configuration.CodecName);
        OS_UnLockMutex(&Lock);
        return CodecError;
    }

    AudioParameters->Source.BitsPerSample       = AudioOutputSurface->BitsPerSample;
    AudioParameters->Source.ChannelCount        = AudioOutputSurface->ChannelCount;
    AudioParameters->Organisation               = Status->AudioMode;
    AudioParameters->SampleCount                = Status->NbOutSamples;
    enum eAccFsCode SamplingFreqCode            = Status->SamplingFreq;

    if (SamplingFreqCode < ACC_FS_reserved)
    {
        AudioParameters->Source.SampleRateHz    = StmSeTranslateDiscreteSamplingFrequencyToInteger(SamplingFreqCode);
    }
    else
    {
        AudioParameters->Source.SampleRateHz    = 0;
        SE_ERROR("PCM audio decode bad sampling freq returned: 0x%x\n", SamplingFreqCode);
    }

    if (SE_IS_DEBUG_ON(group_decoder_audio))
    {
        SE_DEBUG(group_decoder_audio, "BitsPerSample                %d\n", AudioParameters->Source.BitsPerSample);
        SE_DEBUG(group_decoder_audio, "ChannelCount                 %d\n", AudioParameters->Source.ChannelCount);
        SE_DEBUG(group_decoder_audio, "Organisation                 %d\n", AudioParameters->Organisation);
        SE_DEBUG(group_decoder_audio, "SampleCount                  %d\n", AudioParameters->SampleCount);
        SE_DEBUG(group_decoder_audio, "Source.SampleRateHz          %d\n", AudioParameters->Source.SampleRateHz);
    }

    OS_UnLockMutex(&Lock);
    // Validate the extended status (without propagating errors)
    (void) ValidatePcmProcessingExtendedStatus(Context,
                                               (MME_PcmProcessingFrameExtStatus_t *) &DecodeContext->DecodeStatus.PcmStatus);
    //SYSFS
    SetAudioCodecDecAttributes();
    return CodecNoError;
}
//}}}
//{{{  DumpSetStreamParameters
// /////////////////////////////////////////////////////////////////////////
//
//      Function to dump out the set stream
//      parameters from an mme command.
//

CodecStatus_t   Codec_MmeAudioPcm_c::DumpSetStreamParameters(void    *Parameters)
{
    SE_ERROR("Not implemented\n");
    return CodecNoError;
}
//}}}
//{{{  DumpDecodeParameters
// /////////////////////////////////////////////////////////////////////////
//
//      Function to dump out the decode
//      parameters from an mme command.
//

CodecStatus_t   Codec_MmeAudioPcm_c::DumpDecodeParameters(void    *Parameters)
{
    SE_VERBOSE(group_decoder_audio, "TotalSize[0]                  %d\n", DecodeContext->MMEBuffers[0].TotalSize);
    SE_VERBOSE(group_decoder_audio, "Page_p[0]                     %p\n", DecodeContext->MMEPages[0].Page_p);
    SE_VERBOSE(group_decoder_audio, "TotalSize[1]                  %d\n", DecodeContext->MMEBuffers[1].TotalSize);
    SE_VERBOSE(group_decoder_audio, "Page_p[1]                     %p\n", DecodeContext->MMEPages[1].Page_p);
    return CodecNoError;
}
//}}}
