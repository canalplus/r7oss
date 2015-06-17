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

#include "codec_mme_audio_lpcm.h"
#include "codec_capabilities.h"
#include "lpcm.h"

#undef TRACE_TAG
#define TRACE_TAG "Codec_MmeAudioLpcm_c"

const LpcmAudioStreamParameters_t  DefaultStreamParameters =
{
    TypeLpcmDVDVideo,
    ACC_MME_TRUE,
    ACC_MME_TRUE,
    LpcmWordSize16,
    LpcmWordSizeNone,
    LpcmSamplingFreq48,
    LpcmSamplingFreqNone,
    8,
    0,
    LPCM_DEFAULT_CHANNEL_ASSIGNMENT,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0
};

// this array is based on the LpcmSamplingFreq_t enum
static const int LpcmDVDSamplingFreq[LpcmSamplingFreqNone] =
{
    48000, 96000, 192000, 0, 0, 0, 0, 0, 44100, 88200, 176400
};

typedef struct LpcmAudioCodecStreamParameterContext_s
{
    CodecBaseStreamParameterContext_t   BaseContext;

    MME_LxAudioDecoderGlobalParams_t StreamParameters;
} LpcmAudioCodecStreamParameterContext_t;

#define BUFFER_LPCM_AUDIO_CODEC_STREAM_PARAMETER_CONTEXT                "LpcmAudioCodecStreamParameterContext"
#define BUFFER_LPCM_AUDIO_CODEC_STREAM_PARAMETER_CONTEXT_TYPE   {BUFFER_LPCM_AUDIO_CODEC_STREAM_PARAMETER_CONTEXT, BufferDataTypeBase, AllocateFromOSMemory, 32, 0, true, true, sizeof(LpcmAudioCodecStreamParameterContext_t)}

static BufferDataDescriptor_t LpcmAudioCodecStreamParameterContextDescriptor = BUFFER_LPCM_AUDIO_CODEC_STREAM_PARAMETER_CONTEXT_TYPE;

// --------
typedef struct
{
    MME_LxAudioDecoderFrameStatus_t              DecStatus;
    MME_PcmProcessingFrameExtCommonStatus_t      PcmStatus;
} MME_LxAudioDecoderFrameExtendedLpcmStatus_t;

typedef struct LpcmAudioCodecDecodeContext_s
{
    CodecBaseDecodeContext_t            BaseContext;

    MME_LxAudioDecoderFrameParams_t     DecodeParameters;
    MME_LxAudioDecoderFrameExtendedLpcmStatus_t     DecodeStatus;
} LpcmAudioCodecDecodeContext_t;

#define BUFFER_LPCM_AUDIO_CODEC_DECODE_CONTEXT  "LpcmAudioCodecDecodeContext"
#define BUFFER_LPCM_AUDIO_CODEC_DECODE_CONTEXT_TYPE     {BUFFER_LPCM_AUDIO_CODEC_DECODE_CONTEXT, BufferDataTypeBase, AllocateFromOSMemory, 32, 0, true, true, sizeof(LpcmAudioCodecDecodeContext_t)}

static BufferDataDescriptor_t LpcmAudioCodecDecodeContextDescriptor = BUFFER_LPCM_AUDIO_CODEC_DECODE_CONTEXT_TYPE;


static const char *LookupLpcmStreamType(LpcmStreamType_t t)
{
    switch (t)
    {
#define C(x) case TypeLpcm ## x: return #x
        C(DVDVideo);
        C(DVDAudio);
        C(DVDHD);
        C(DVDBD);
        C(SPDIFIN);
#undef C

    default:
        return "INVALID";
    }
}

static enum eAccLpcmMode LookupLpcmMode(LpcmStreamType_t type)
{
    switch (type)
    {
    case TypeLpcmDVDVideo:  return ACC_LPCM_VIDEO;
    case TypeLpcmDVDAudio:  return ACC_LPCM_AUDIO;
    case TypeLpcmDVDHD:     return ACC_LPCM_HD;
    case TypeLpcmDVDBD:     return ACC_LPCM_BD;
    // LPCM codec would not be invoked for SPDIFin
    // so consider it as invalid.
    case TypeLpcmSPDIFIN:
    case TypeLpcmInvalid:
    default:
        return ACC_RAW_PCM; // Invalid
    }
}

///
/// \todo Set the audio capability mask...
///
Codec_MmeAudioLpcm_c::Codec_MmeAudioLpcm_c(void)
    : Codec_MmeAudio_c()
    , DecoderId(ACC_LPCM_ID)
{
    Configuration.CodecName                             = "LPCM audio";
    Configuration.StreamParameterContextCount           = 1;
    Configuration.StreamParameterContextDescriptor      = &LpcmAudioCodecStreamParameterContextDescriptor;
    Configuration.DecodeContextCount                    = 4;
    Configuration.DecodeContextDescriptor               = &LpcmAudioCodecDecodeContextDescriptor;
    Configuration.MaximumSampleCount                    = LPCM_MAXIMUM_SAMPLE_COUNT;
}

///
Codec_MmeAudioLpcm_c::~Codec_MmeAudioLpcm_c(void)
{
    Halt();
}

////////////////////////////////////////////////////////////////////////////
///
/// Populate the supplied structure with parameters for LPCM audio.
///
CodecStatus_t Codec_MmeAudioLpcm_c::FillOutTransformerGlobalParameters(MME_LxAudioDecoderGlobalParams_t *GlobalParams_p)
{
    LpcmAudioStreamParameters_t    *Parsed;
    MME_LxAudioDecoderGlobalParams_t &GlobalParams = *GlobalParams_p;
    GlobalParams.StructSize = sizeof(MME_LxAudioDecoderGlobalParams_t);

    if ((ParsedFrameParameters == NULL) || (ParsedFrameParameters->StreamParameterStructure == NULL))
    {
        // At transformer init, stream properties might be unknown...
        Parsed = (LpcmAudioStreamParameters_t *) &DefaultStreamParameters;
    }
    else
    {
        Parsed = (LpcmAudioStreamParameters_t *)ParsedFrameParameters->StreamParameterStructure;
    }

    SE_INFO(group_decoder_audio, "Init LPCM Decoder (as %s)\n", LookupLpcmStreamType(Parsed->Type));

    if (AudioOutputSurface == NULL)
    {
        SE_ERROR("(%s) - AudioOutputSurface is NULL\n", Configuration.CodecName);
        return CodecError;
    }

    MME_LxLpcmConfig_t &Config = *((MME_LxLpcmConfig_t *) GlobalParams.DecConfig);
    Config.DecoderId = ACC_LPCM_ID;
    Config.StructSize = sizeof(MME_LxLpcmConfig_t);
    Config.Config[LPCM_MODE]     = (U32) LookupLpcmMode(Parsed->Type);
    Config.Config[LPCM_DRC_CODE] = Parsed->DrcCode;
    bool DrcEnable;

    if ((Parsed->Type == TypeLpcmDVDBD) || (Parsed->DrcCode == LPCM_DRC_VALUE_DISABLE))
    {
        DrcEnable = ACC_MME_FALSE;
    }
    else
    {
        DrcEnable = ACC_MME_TRUE;
    }

    Config.Config[LPCM_DRC_ENABLE] = DrcEnable;
    Config.Config[LPCM_MUTE_FLAG] = Parsed->MuteFlag;
    Config.Config[LPCM_EMPHASIS_FLAG] = Parsed->EmphasisFlag;
    Config.Config[LPCM_NB_CHANNELS] = Parsed->NumberOfChannels;
    Config.Config[LPCM_WS_CH_GR1] = Parsed->WordSize1;
    Config.Config[LPCM_WS_CH_GR2] = (Parsed->WordSize2 == LpcmWordSizeNone) ? LPCM_DVD_AUDIO_NO_CH_GR2 : Parsed->WordSize2;
    Config.Config[LPCM_FS_CH_GR1] = Parsed->SamplingFrequency1;
    Config.Config[LPCM_FS_CH_GR2] = (Parsed->SamplingFrequency2 == LpcmSamplingFreqNone) ? LPCM_DVD_AUDIO_NO_CH_GR2 : Parsed->SamplingFrequency2;
    Config.Config[LPCM_BIT_SHIFT_CH_GR2] = Parsed->BitShiftChannel2;
    Config.Config[LPCM_CHANNEL_ASSIGNMENT] = Parsed->ChannelAssignment;
    Config.Config[LPCM_MIXING_PHASE] = 0;
    Config.Config[LPCM_NB_ACCESS_UNITS] = Parsed->NbAccessUnits;
    // force resampling according to the manifestor target sampling frequency
    unsigned char Resampling = ACC_LPCM_AUTO_RSPL;

    if (Parsed->Type != TypeLpcmDVDAudio)
    {
        unsigned int StreamSamplingFreq = LpcmDVDSamplingFreq[Parsed->SamplingFrequency1];

        if ((StreamSamplingFreq == 48000) &&
            (AudioOutputSurface->SampleRateHz == 96000))
        {
            Resampling = ACC_LPCM_RSPL_48;
        }
        else if ((StreamSamplingFreq == 96000) &&
                 (AudioOutputSurface->SampleRateHz == 48000))
        {
            Resampling = ACC_LPCM_RSPL_96;
        }
    }

    Config.Config[LPCM_OUT_RESAMPLING] = Resampling;
    Config.Config[LPCM_NB_SAMPLES] = Parsed->NumberOfSamples;

    CodecStatus_t Status = Codec_MmeAudio_c::FillOutTransformerGlobalParameters(GlobalParams_p);
    if (Status != CodecNoError)
    {
        return Status;
    }

    unsigned char *PcmParams_p = ((unsigned char *) &Config) + Config.StructSize;
    MME_LxPcmProcessingGlobalParams_Subset_t &PcmParams =
        *((MME_LxPcmProcessingGlobalParams_Subset_t *) PcmParams_p);
    // downmix must be disabled for LPCM
    MME_DMixGlobalParams_t &DMix = PcmParams.DMix;
    DMix.Apply = ACC_MME_DISABLED;

    return CodecNoError;
}


////////////////////////////////////////////////////////////////////////////
///
/// Populate the AUDIO_DECODER's initialization parameters for LPCM audio.
///
/// When this method completes Codec_MmeAudio_c::AudioDecoderInitializationParameters
/// will have been filled out with valid values sufficient to initialize an
/// LPCM audio decoder.
///
CodecStatus_t   Codec_MmeAudioLpcm_c::FillOutTransformerInitializationParameters(void)
{
    MME_LxAudioDecoderInitParams_t &Params = AudioDecoderInitializationParameters;

    MMEInitializationParameters.TransformerInitParamsSize = sizeof(Params);
    MMEInitializationParameters.TransformerInitParams_p = &Params;

    CodecStatus_t Status = Codec_MmeAudio_c::FillOutTransformerInitializationParameters();
    if (Status != CodecNoError)
    {
        return Status;
    }

    return FillOutTransformerGlobalParameters(&Params.GlobalParams);
}


////////////////////////////////////////////////////////////////////////////
///
/// Populate the AUDIO_DECODER's MME_SET_GLOBAL_TRANSFORMER_PARAMS parameters for LPCM audio.
///
CodecStatus_t   Codec_MmeAudioLpcm_c::FillOutSetStreamParametersCommand(void)
{
    LpcmAudioCodecStreamParameterContext_t  *Context = (LpcmAudioCodecStreamParameterContext_t *)StreamParameterContext;
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
CodecStatus_t   Codec_MmeAudioLpcm_c::FillOutDecodeCommand(void)
{
    LpcmAudioCodecDecodeContext_t   *Context        = (LpcmAudioCodecDecodeContext_t *)DecodeContext;
    LpcmAudioFrameParameters_t    *Parsed         = (LpcmAudioFrameParameters_t *)ParsedFrameParameters->FrameParameterStructure;
    //
    // Initialize the frame parameters
    //
    memset(&Context->DecodeParameters, 0, sizeof(Context->DecodeParameters));

    bool mute = false;
    Player->GetControl(Playback, Stream, STM_SE_CTRL_PLAY_STREAM_MUTE, &mute);
    Context->DecodeParameters.Cmd = mute ? ACC_CMD_MUTE : ACC_CMD_PLAY;

    Context->DecodeParameters.FrameParams[ACC_LPCM_FRAME_PARAMS_NBSAMPLES_INDEX] = Parsed->NumberOfSamples;
    Context->DecodeParameters.FrameParams[ACC_LPCM_FRAME_PARAMS_DRC_INDEX] = Parsed->DrcCode;
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
/// Dispite the squawking this method unconditionally returns success. This is
/// because the firmware will already have concealed the decode problems by
/// performing a soft mute.
///
/// \return CodecSuccess
///
CodecStatus_t   Codec_MmeAudioLpcm_c::ValidateDecodeContext(CodecBaseDecodeContext_t *Context)
{
    LpcmAudioCodecDecodeContext_t   *DecodeContext;
    MME_LxAudioDecoderFrameStatus_t *Status;
    ParsedAudioParameters_t *AudioParameters;
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

    DecodeContext = (LpcmAudioCodecDecodeContext_t *) Context;
    Status        = &(DecodeContext->DecodeStatus.DecStatus);

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

    // TODO: these values should be extracted from the codec's reply
    AudioParameters->Source.BitsPerSample = /* AudioOutputSurface->BitsPerSample;*/ ((AUDIODEC_GET_OUTPUT_WS((&AudioDecoderInitializationParameters)) == ACC_WS32) ? 32 : 16);
    AudioParameters->Source.ChannelCount =  AudioOutputSurface->ChannelCount;/*Codec_MmeAudio_c::GetNumberOfChannelsFromAudioConfiguration((enum eAccAcMode) Status.AudioMode);*/
    AudioParameters->Organisation = Status->AudioMode;
    // lpcm can be resampled directly in the codec to avoid having them resampled by the mixer
    // so get the sampling frequency from the codec
    AudioParameters->SampleCount = Status->NbOutSamples;
    enum eAccFsCode SamplingFreqCode = Status->SamplingFreq;

    if (SamplingFreqCode < ACC_FS_reserved)
    {
        AudioParameters->Source.SampleRateHz = StmSeTranslateDiscreteSamplingFrequencyToInteger(SamplingFreqCode);
    }
    else
    {
        AudioParameters->Source.SampleRateHz = 0;
        SE_ERROR("LPCM audio decode bad sampling freq returned: 0x%x\n", SamplingFreqCode);
    }

    OS_UnLockMutex(&Lock);
    // Validate the extended status (without propagating errors)
    (void) ValidatePcmProcessingExtendedStatus(Context,
                                               (MME_PcmProcessingFrameExtStatus_t *) &DecodeContext->DecodeStatus.PcmStatus);
    //SYSFS
    SetAudioCodecDecAttributes();
    return CodecNoError;
}

// /////////////////////////////////////////////////////////////////////////
//
//      Function to dump out the set stream
//      parameters from an mme command.
//

CodecStatus_t   Codec_MmeAudioLpcm_c::DumpSetStreamParameters(void    *Parameters)
{
    SE_ERROR("Not implemented\n");
    return CodecNoError;
}

// /////////////////////////////////////////////////////////////////////////
//
//      Function to dump out the decode
//      parameters from an mme command.
//

CodecStatus_t   Codec_MmeAudioLpcm_c::DumpDecodeParameters(void    *Parameters)
{
    SE_ERROR("Not implemented\n");
    return CodecNoError;
}

////////////////////////////////////////////////////////////////////////////
///
///  Public static function to fill PCM codec capabilities
///  to expose it through STM_SE_CTRL_GET_CAPABILITY_AUDIO_DECODE control.
///
void Codec_MmeAudioLpcm_c::GetCapabilities(stm_se_audio_dec_capability_t::audio_dec_pcm_capability_s *cap,
                                           const MME_LxAudioDecoderHDInfo_t decInfo)
{
    const int ExtFlags = Codec_Capabilities_c::ExtractAudioExtendedFlags(decInfo, ACC_LPCM);
    cap->common.capable = (decInfo.DecoderCapabilityFlags     & (1 << ACC_LPCM)
                           & SE_AUDIO_DEC_CAPABILITIES        & (1 << ACC_LPCM)
                          ) ? true : false;
    cap->lpcm_DVDvideo = (ExtFlags & (1 << ACC_LPCM_DVD_VIDEO)) ? true : false;
    cap->lpcm_DVDaudio = (ExtFlags & (1 << ACC_LPCM_DVD_AUDIO)) ? true : false;
    cap->lpcm_CDDA     = (ExtFlags & (1 << ACC_LPCM_CDDA))      ? true : false;
    cap->lpcm_8bits    = (ExtFlags & (1 << ACC_LPCM_8BIT))      ? true : false;

    if (decInfo.DecoderCapabilityFlags     & (1 << ACC_PCM)
        & SE_AUDIO_DEC_CAPABILITIES        & (1 << ACC_PCM))
    {
        /* Firmware does not expose these capabilities extensions */
        cap->ALaw      = true;
        cap->MuLaw     = true;
    }
}
