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
/// \class Codec_MmeAudioMlp_c
///
/// The MLP audio codec proxy.
///

#include "codec_mme_audio_mlp.h"
#include "codec_capabilities.h"
#include "mlp.h"

#undef TRACE_TAG
#define TRACE_TAG "Codec_MmeAudioMlp_c"

typedef struct MlpAudioCodecStreamParameterContext_s
{
    CodecBaseStreamParameterContext_t   BaseContext;

    MME_LxAudioDecoderGlobalParams_t StreamParameters;
} MlpAudioCodecStreamParameterContext_t;

#define BUFFER_MLP_AUDIO_CODEC_STREAM_PARAMETER_CONTEXT        "MlpAudioCodecStreamParameterContext"
#define BUFFER_MLP_AUDIO_CODEC_STREAM_PARAMETER_CONTEXT_TYPE   {BUFFER_MLP_AUDIO_CODEC_STREAM_PARAMETER_CONTEXT, BufferDataTypeBase, AllocateFromOSMemory, 32, 0, true, true, sizeof(MlpAudioCodecStreamParameterContext_t)}

static BufferDataDescriptor_t MlpAudioCodecStreamParameterContextDescriptor = BUFFER_MLP_AUDIO_CODEC_STREAM_PARAMETER_CONTEXT_TYPE;

// --------
typedef struct
{
    MME_LxAudioDecoderFrameStatus_t             DecStatus;
    MME_PcmProcessingFrameExtCommonStatus_t     PcmStatus;
} MME_LxAudioDecoderFrameExtendedMlpStatus_t;

typedef struct MlpAudioCodecDecodeContext_s
{
    CodecBaseDecodeContext_t                    BaseContext;

    MME_LxAudioDecoderFrameParams_t             DecodeParameters;
    MME_LxAudioDecoderFrameExtendedMlpStatus_t  DecodeStatus;
} MlpAudioCodecDecodeContext_t;

#define BUFFER_MLP_AUDIO_CODEC_DECODE_CONTEXT          "MlpAudioCodecDecodeContext"
#define BUFFER_MLP_AUDIO_CODEC_DECODE_CONTEXT_TYPE     {BUFFER_MLP_AUDIO_CODEC_DECODE_CONTEXT, BufferDataTypeBase, AllocateFromOSMemory, 32, 0, true, true, sizeof(MlpAudioCodecDecodeContext_t)}

static BufferDataDescriptor_t MlpAudioCodecDecodeContextDescriptor = BUFFER_MLP_AUDIO_CODEC_DECODE_CONTEXT_TYPE;

///
Codec_MmeAudioMlp_c::Codec_MmeAudioMlp_c(void)
    : Codec_MmeAudio_c()
    , DecoderId(ACC_TRUEHD_ID)
{
    Configuration.CodecName                             = "MLP audio";
    Configuration.StreamParameterContextCount           = 1;
    Configuration.StreamParameterContextDescriptor      = &MlpAudioCodecStreamParameterContextDescriptor;
    Configuration.DecodeContextCount                    = 4;
    Configuration.DecodeContextDescriptor               = &MlpAudioCodecDecodeContextDescriptor;
    Configuration.MaximumSampleCount                    = MLP_MAX_DECODED_SAMPLE_COUNT;

    AudioDecoderTransformCapabilityMask.DecoderCapabilityFlags = (1 << ACC_DOLBY_TRUEHD_LOSSLESS);
}

///
Codec_MmeAudioMlp_c::~Codec_MmeAudioMlp_c(void)
{
    Halt();
}

////////////////////////////////////////////////////////////////////////////
///
/// Populate the supplied structure with parameters for MLP audio.
///
///
CodecStatus_t Codec_MmeAudioMlp_c::FillOutTransformerGlobalParameters(MME_LxAudioDecoderGlobalParams_t *GlobalParams_p)
{
    SE_INFO(group_decoder_audio, "Initializing MLP audio decoder\n");

    MME_LxAudioDecoderGlobalParams_t &GlobalParams = *GlobalParams_p;
    int NbAccessUnits = 0;

    if ((ParsedFrameParameters != NULL) && (ParsedFrameParameters->StreamParameterStructure != NULL))
    {
        MlpAudioStreamParameters_t *Parsed = (MlpAudioStreamParameters_t *)ParsedFrameParameters->StreamParameterStructure;
        NbAccessUnits = Parsed->AccumulatedFrameNumber;
    }

    GlobalParams.StructSize = sizeof(MME_LxAudioDecoderGlobalParams_t);

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

    return Codec_MmeAudio_c::FillOutTransformerGlobalParameters(GlobalParams_p);
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
CodecStatus_t   Codec_MmeAudioMlp_c::FillOutTransformerInitializationParameters(void)
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
/// Populate the AUDIO_DECODER's MME_SET_GLOBAL_TRANSFORMER_PARAMS parameters for MLP audio.
///
CodecStatus_t   Codec_MmeAudioMlp_c::FillOutSetStreamParametersCommand(void)
{
    MlpAudioCodecStreamParameterContext_t  *Context = (MlpAudioCodecStreamParameterContext_t *)StreamParameterContext;
    //
    // Examine the parsed stream parameters and determine what type of codec to instanciate
    //
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
/// Populate the AUDIO_DECODER's MME_TRANSFORM parameters for MLP audio.
///
CodecStatus_t   Codec_MmeAudioMlp_c::FillOutDecodeCommand(void)
{
    MlpAudioCodecDecodeContext_t   *Context        = (MlpAudioCodecDecodeContext_t *)DecodeContext;
//MlpAudioFrameParameters_t    *Parsed         = (MlpAudioFrameParameters_t *)ParsedFrameParameters->FrameParameterStructure;
    //
    // Initialize the frame parameters (we don't actually have much to say here)
    //
    memset(&Context->DecodeParameters, 0, sizeof(Context->DecodeParameters));

    bool mute = false;
    Player->GetControl(Playback, Stream, STM_SE_CTRL_PLAY_STREAM_MUTE, &mute);
    Context->DecodeParameters.Cmd = mute ? ACC_CMD_MUTE : ACC_CMD_PLAY;

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
CodecStatus_t   Codec_MmeAudioMlp_c::ValidateDecodeContext(CodecBaseDecodeContext_t *Context)
{
    MlpAudioCodecDecodeContext_t    *DecodeContext;
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

    DecodeContext = (MlpAudioCodecDecodeContext_t *) Context;
    Status        = &(DecodeContext->DecodeStatus.DecStatus);

    if (Status->DecStatus)
    {
        SE_WARNING("MLP audio decode error (muted frame): 0x%x\n", Status->DecStatus);
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

    AudioParameters->Source.BitsPerSample = ((AUDIODEC_GET_OUTPUT_WS((&AudioDecoderInitializationParameters)) == ACC_WS32) ? 32 : 16);
    //AudioParameters->Source.BitsPerSample = AudioOutputSurface->BitsPerSample;
    AudioParameters->Source.ChannelCount =  AudioOutputSurface->ChannelCount;
    //AudioParameters->Source.ChannelCount =  Codec_MmeAudio_c::GetNumberOfChannelsFromAudioConfiguration((enum eAccAcMode) Status.AudioMode);
    AudioParameters->Organisation = Status->AudioMode;
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

CodecStatus_t   Codec_MmeAudioMlp_c::DumpSetStreamParameters(void    *Parameters)
{
    SE_ERROR("Not implemented\n");
    return CodecNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Function to dump out the decode
//      parameters from an mme command.
//

CodecStatus_t   Codec_MmeAudioMlp_c::DumpDecodeParameters(void    *Parameters)
{
    SE_ERROR("Not implemented\n");
    return CodecNoError;
}

////////////////////////////////////////////////////////////////////////////
///
///  Public static function to fill Dolby TrueHD codec capabilities
///  to expose it through STM_SE_CTRL_GET_CAPABILITY_AUDIO_DECODE control.
///
void Codec_MmeAudioMlp_c::GetCapabilities(stm_se_audio_dec_capability_t::audio_dec_dolby_truehd_capability_s *cap,
                                          const MME_LxAudioDecoderHDInfo_t decInfo)
{
    const int ExtFlags = Codec_Capabilities_c::ExtractAudioExtendedFlags(decInfo, ACC_TRUEHD);
    cap->common.capable = (decInfo.DecoderCapabilityFlags     & (1 << ACC_TRUEHD)
                           & SE_AUDIO_DEC_CAPABILITIES        & (1 << ACC_TRUEHD)
                          ) ? true : false;
    cap->dthd_DVD_Audio = cap->common.capable                           ? true : false;
    cap->dthd_BD        = (ExtFlags & (1 << ACC_DOLBY_TRUEHD_LOSSLESS)) ? true : false;
    cap->dthd_MAT       = (ExtFlags & (1 << ACC_DOLBY_TRUEHD_MATENC))   ? true : false;
}
