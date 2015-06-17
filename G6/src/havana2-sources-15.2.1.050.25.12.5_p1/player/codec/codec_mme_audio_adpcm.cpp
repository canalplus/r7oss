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

#include "codec_mme_audio_adpcm.h"
#include "frame_parser_audio_adpcm.h"
#include "adpcm.h"

#undef TRACE_TAG
#define TRACE_TAG "Codec_MmeAudioAdpcm_c"

// Define the default stream parameters for the first codec initialization.
const AdpcmAudioStreamParameters_t  DefaultStreamParameters =
{
    TypeImaAdpcm,
    48000,
    2,
    2,
    0,
    0,
    0,
    0,
    0,
    0,
    0

};

typedef struct AdpcmAudioCodecStreamParameterContext_s
{
    CodecBaseStreamParameterContext_t   BaseContext;
    MME_LxAudioDecoderGlobalParams_t StreamParameters;
} AdpcmAudioCodecStreamParameterContext_t;

#define BUFFER_ADPCM_AUDIO_CODEC_STREAM_PARAMETER_CONTEXT                "AdpcmAudioCodecStreamParameterContext"
#define BUFFER_ADPCM_AUDIO_CODEC_STREAM_PARAMETER_CONTEXT_TYPE   {BUFFER_ADPCM_AUDIO_CODEC_STREAM_PARAMETER_CONTEXT, BufferDataTypeBase, AllocateFromOSMemory, 32, 0, true, true, sizeof(AdpcmAudioCodecStreamParameterContext_t)}

static BufferDataDescriptor_t AdpcmAudioCodecStreamParameterContextDescriptor = BUFFER_ADPCM_AUDIO_CODEC_STREAM_PARAMETER_CONTEXT_TYPE;



typedef struct AdpcmAudioCodecDecodeContext_s
{
    CodecBaseDecodeContext_t            BaseContext;
    MME_LxAudioDecoderFrameParams_t     DecodeParameters;
    MME_LxAudioDecoderFrameStatus_t     DecodeStatus;
} AdpcmAudioCodecDecodeContext_t;

#define BUFFER_ADPCM_AUDIO_CODEC_DECODE_CONTEXT  "AdpcmAudioCodecDecodeContext"
#define BUFFER_ADPCM_AUDIO_CODEC_DECODE_CONTEXT_TYPE     {BUFFER_ADPCM_AUDIO_CODEC_DECODE_CONTEXT, BufferDataTypeBase, AllocateFromOSMemory, 32, 0, true, true, sizeof(AdpcmAudioCodecDecodeContext_t)}

static BufferDataDescriptor_t AdpcmAudioCodecDecodeContextDescriptor = BUFFER_ADPCM_AUDIO_CODEC_DECODE_CONTEXT_TYPE;

///
Codec_MmeAudioAdpcm_c::Codec_MmeAudioAdpcm_c(void)
    : Codec_MmeAudio_c()
    , DecoderId(ACC_LPCM_ID)
{
    Configuration.CodecName                             = "LPCM audio";
    Configuration.StreamParameterContextCount           = 1;
    Configuration.StreamParameterContextDescriptor      = &AdpcmAudioCodecStreamParameterContextDescriptor;
    Configuration.DecodeContextCount                    = 4;
    Configuration.DecodeContextDescriptor               = &AdpcmAudioCodecDecodeContextDescriptor;
    Configuration.MaximumSampleCount                    = ADPCM_MAXIMUM_SAMPLE_COUNT;
}

///
Codec_MmeAudioAdpcm_c::~Codec_MmeAudioAdpcm_c(void)
{
    Halt();
}

////////////////////////////////////////////////////////////////////////////
///
/// Populate the supplied structure with parameters for LPCM audio codec.
///
CodecStatus_t Codec_MmeAudioAdpcm_c::FillOutTransformerGlobalParameters(MME_LxAudioDecoderGlobalParams_t *GlobalParams_p)
{
    //
    // Local variables.
    // - Status: the Status to return.
    // - accFreqCode: the audio frequency expressed in acc code.
    // - StreamParameters: the stream parameters
    // - GlobalParams_p: the global parameters.
    //
    CodecStatus_t                    Status = CodecNoError;
    enum eAccFsCode                  accFreqCode;
    AdpcmAudioStreamParameters_t     *StreamParameters;
    MME_LxAudioDecoderGlobalParams_t &GlobalParams = *GlobalParams_p;
    GlobalParams.StructSize          = sizeof(MME_LxAudioDecoderGlobalParams_t);
    MME_LxLpcmConfig_t               &Config = *((MME_LxLpcmConfig_t *) GlobalParams.DecConfig);

    //
    // Get the stream parameters (filled out in the ParsedFrameParameters->StreamParameterStructure
    // by the frame parser.
    // If ParsedFrameParameters is NULL (at transformer init) use the default stream parameters.
    //
    if ((ParsedFrameParameters == NULL) || (ParsedFrameParameters->StreamParameterStructure == NULL))
    {
        StreamParameters = (AdpcmAudioStreamParameters_t *) &DefaultStreamParameters;
    }
    else
    {
        StreamParameters = (AdpcmAudioStreamParameters_t *)ParsedFrameParameters->StreamParameterStructure;
    }

    //
    // Fill out the codec configuration.
    // Some values are set here and some come from the stream parameters that
    // the frame parser has filled out.
    //
    Config.DecoderId                       = ACC_LPCM_ID;
    Config.StructSize                      = sizeof(MME_LxLpcmConfig_t);

    switch (StreamParameters->Type)
    {
    case TypeMsAdpcm:
        Config.Config[LPCM_MODE] = ACC_LPCM_MS_ADPCM;
        break;

    case TypeImaAdpcm:
        Config.Config[LPCM_MODE] = ACC_LPCM_IMA_ADPCM;
        break;

    default:
        break;
    }

    if (Codec_MmeAudio_c::ConvertFreqToAccCode(StreamParameters->SamplingFrequency1, accFreqCode) == CodecError)
    {
        Status = CodecError;
    }
    else
    {
        Config.Config[LPCM_DRC_CODE]            = ADPCM_DRC_VALUE_DISABLE;
        Config.Config[LPCM_DRC_ENABLE]          = ACC_MME_FALSE;
        Config.Config[LPCM_MUTE_FLAG]           = 0;
        Config.Config[LPCM_EMPHASIS_FLAG]       = 0;
        Config.Config[LPCM_NB_CHANNELS]         = StreamParameters->NumberOfChannels;
        Config.Config[LPCM_WS_CH_GR1]           = 0;
        Config.Config[LPCM_WS_CH_GR2]           = 15;
        Config.Config[LPCM_FS_CH_GR1]           = 0;
        Config.Config[LPCM_FS_CH_GR1]           = accFreqCode;
        Config.Config[LPCM_FS_CH_GR2]           = 15;
        Config.Config[LPCM_BIT_SHIFT_CH_GR2]    = StreamParameters->NbOfCoefficients;
        Config.Config[LPCM_CHANNEL_ASSIGNMENT]  = 0x000000FF;
        Config.Config[LPCM_MIXING_PHASE]        = 0;
        Config.Config[LPCM_OUT_RESAMPLING]      = ACC_LPCM_NO_RSPL;
        Config.Config[LPCM_NB_ACCESS_UNITS]     = StreamParameters->NbOfBlockPerBufferToDecode;
        Config.Config[LPCM_NB_SAMPLES]          = StreamParameters->NbOfSamplesOverAllBlocks;
        //
        // Fill out the transformer global parameter.
        //
        Status = Codec_MmeAudio_c::FillOutTransformerGlobalParameters(GlobalParams_p);

        if (Status == CodecNoError)
        {
            unsigned char *PcmParams_p = ((unsigned char *) &Config) + Config.StructSize;
            MME_LxPcmProcessingGlobalParams_Subset_t &PcmParams =
                *((MME_LxPcmProcessingGlobalParams_Subset_t *) PcmParams_p);
            PcmParams.CMC.Features.PcmDownScaled = 0;
            // downmix must be disabled for LPCM
            //MME_DMixGlobalParams_t &DMix = PcmParams.DMix;
            //XXXXXXXXXXXDMix.Apply = ACC_MME_DISABLED;
        }
    }

    return Status;
}


////////////////////////////////////////////////////////////////////////////
///
/// Populate the AUDIO_DECODER's initialization parameters for LPCM audio.
///
/// When this method completes Codec_MmeAudio_c::AudioDecoderInitializationParameters
/// will have been filled out with valid values sufficient to initialize an
/// LPCM audio decoder.
///
CodecStatus_t   Codec_MmeAudioAdpcm_c::FillOutTransformerInitializationParameters(void)
{
    //
    // Local variables.
    // - Status: the status to return.
    // - Params: the parameters to initialize the decoder.
    //
    MME_LxAudioDecoderInitParams_t &Params = AudioDecoderInitializationParameters;
    //
    // Fill out the initialization parameters.
    //
    MMEInitializationParameters.TransformerInitParamsSize = sizeof(Params);
    MMEInitializationParameters.TransformerInitParams_p = &Params;
    CodecStatus_t Status = Codec_MmeAudio_c::FillOutTransformerInitializationParameters();
    if (Status == CodecNoError)
    {
        Status = FillOutTransformerGlobalParameters(&Params.GlobalParams);
    }

    return Status;
}


////////////////////////////////////////////////////////////////////////////
///
/// Populate the AUDIO_DECODER's MME_SET_GLOBAL_TRANSFORMER_PARAMS parameters for LPCM audio.
///
CodecStatus_t   Codec_MmeAudioAdpcm_c::FillOutSetStreamParametersCommand(void)
{
    //
    // Local variables
    // - Status: the status to return.
    // - Context: the structure that contains the MME command and the context buffer.
    //
    AdpcmAudioCodecStreamParameterContext_t  *Context = (AdpcmAudioCodecStreamParameterContext_t *)StreamParameterContext;
    //
    // Examine the parsed stream parameters and determine what type of codec to instanciate
    //
    DecoderId = ACC_LPCM_ID;
    //
    // Now fill out the actual structure
    //
    memset(&(Context->StreamParameters), 0, sizeof(Context->StreamParameters));
    CodecStatus_t Status = FillOutTransformerGlobalParameters(&(Context->StreamParameters));
    if (Status != CodecNoError)
    {
        return Status;
    }

    //
    // Fill out the actual command
    //
    Context->BaseContext.MMECommand.CmdStatus.AdditionalInfoSize        = 0;
    Context->BaseContext.MMECommand.CmdStatus.AdditionalInfo_p          = NULL;
    Context->BaseContext.MMECommand.ParamSize                           = sizeof(Context->StreamParameters);
    Context->BaseContext.MMECommand.Param_p                             = (MME_GenericParams_t)(&Context->StreamParameters);
    return CodecNoError;
}

////////////////////////////////////////////////////////////////////////////
///
/// Populate the AUDIO_DECODER's MME_TRANSFORM parameters for LPCM audio.
///
CodecStatus_t   Codec_MmeAudioAdpcm_c::FillOutDecodeCommand(void)
{
    AdpcmAudioCodecDecodeContext_t   *Context        = (AdpcmAudioCodecDecodeContext_t *)DecodeContext;
    AdpcmAudioFrameParameters_t      *StreamParameters         = (AdpcmAudioFrameParameters_t *)ParsedFrameParameters->FrameParameterStructure;
    //
    // Initialize the frame parameters
    //
    memset(&Context->DecodeParameters, 0, sizeof(Context->DecodeParameters));

    bool mute = false;
    Player->GetControl(Playback, Stream, STM_SE_CTRL_PLAY_STREAM_MUTE, &mute);
    Context->DecodeParameters.Cmd = mute ? ACC_CMD_MUTE : ACC_CMD_PLAY;

    Context->DecodeParameters.FrameParams[ACC_LPCM_FRAME_PARAMS_NBSAMPLES_INDEX] = StreamParameters->NbOfSamplesOverAllBlocks;
    Context->DecodeParameters.FrameParams[ACC_LPCM_FRAME_PARAMS_DRC_INDEX] = StreamParameters->DrcCode;
    Context->DecodeParameters.FrameParams[ACC_LPCM_FRAME_PARAMS_NB_ACCESS_UNITS] = StreamParameters->NbOfBlockPerBufferToDecode;
    //
    // Zero the reply structure
    //
    memset(&Context->DecodeStatus, 0, sizeof(Context->DecodeStatus));
    //
    // Fill out the actual command
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
/// Despite the squawking this method unconditionally returns success. This is
/// because the firmware will already have concealed the decode problems by
/// performing a soft mute.
///
/// \return CodecSuccess
///
CodecStatus_t   Codec_MmeAudioAdpcm_c::ValidateDecodeContext(CodecBaseDecodeContext_t *Context)
{
    //
    // Local variables.
    //
    AdpcmAudioCodecDecodeContext_t   *DecodeContext;
    MME_LxAudioDecoderFrameStatus_t  *Status;
    ParsedAudioParameters_t          *AudioParameters;
    SE_VERBOSE(group_decoder_audio, ">><<\n");
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

    DecodeContext = (AdpcmAudioCodecDecodeContext_t *) Context;
    Status        = &(DecodeContext->DecodeStatus);
    if (Status->DecStatus)
    {
        SE_WARNING("LPCM audio decode error (muted frame): %d\n", Status->DecStatus);
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
    AudioParameters = BufferState[DecodeContext->BaseContext.BufferIndex].ParsedAudioParameters;

    if (AudioParameters == NULL)
    {
        SE_ERROR("(%s) - AudioParameters are NULL\n", Configuration.CodecName);
        OS_UnLockMutex(&Lock);
        return CodecError;
    }

    AudioParameters->Source.BitsPerSample = /* AudioOutputSurface->BitsPerSample;*/ ((AUDIODEC_GET_OUTPUT_WS((&AudioDecoderInitializationParameters)) == ACC_WS32) ? 32 : 16);
    AudioParameters->Source.ChannelCount =  AudioOutputSurface->ChannelCount;/*Codec_MmeAudio_c::GetNumberOfChannelsFromAudioConfiguration((enum eAccAcMode) Status.AudioMode);*/
    AudioParameters->Organisation = Status->AudioMode;
    AudioParameters->SampleCount = Status->NbOutSamples;

    if (Status->SamplingFreq < ACC_FS_reserved)
    {
        AudioParameters->Source.SampleRateHz = StmSeTranslateDiscreteSamplingFrequencyToInteger(Status->SamplingFreq);
    }
    else
    {
        AudioParameters->Source.SampleRateHz = 0;
        SE_ERROR("LPCM audio decode (use for ADPCM) bad sampling freq returned: 0x%x\n", Status->SamplingFreq);
    }

    OS_UnLockMutex(&Lock);
    return CodecNoError;
}

// /////////////////////////////////////////////////////////////////////////
//
//      Function to dump out the set stream
//      parameters from an mme command.
//

CodecStatus_t   Codec_MmeAudioAdpcm_c::DumpSetStreamParameters(void    *Parameters)
{
    SE_ERROR("Not implemented\n");
    return CodecNoError;
}

// /////////////////////////////////////////////////////////////////////////
//
//      Function to dump out the decode
//      parameters from an mme command.
//

CodecStatus_t   Codec_MmeAudioAdpcm_c::DumpDecodeParameters(void    *Parameters)
{
    SE_ERROR("Not implemented\n");
    return CodecNoError;
}
