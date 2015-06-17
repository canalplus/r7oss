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
/// \class Codec_MmeAudioMpeg_c
///
/// The MPEG audio codec proxy.
///

#include "codec_mme_audio_mpeg.h"
#include "codec_capabilities.h"
#include "mpeg_audio.h"

#undef TRACE_TAG
#define TRACE_TAG "Codec_MmeAudioMpeg_c"

typedef struct MpegAudioCodecStreamParameterContext_s
{
    CodecBaseStreamParameterContext_t   BaseContext;

    MME_LxAudioDecoderGlobalParams_t    StreamParameters;
} MpegAudioCodecStreamParameterContext_t;

#define BUFFER_MPEG_AUDIO_CODEC_STREAM_PARAMETER_CONTEXT                "MpegAudioCodecStreamParameterContext"
#define BUFFER_MPEG_AUDIO_CODEC_STREAM_PARAMETER_CONTEXT_TYPE   {BUFFER_MPEG_AUDIO_CODEC_STREAM_PARAMETER_CONTEXT, BufferDataTypeBase, AllocateFromOSMemory, 32, 0, true, true, sizeof(MpegAudioCodecStreamParameterContext_t)}

static BufferDataDescriptor_t MpegAudioCodecStreamParameterContextDescriptor = BUFFER_MPEG_AUDIO_CODEC_STREAM_PARAMETER_CONTEXT_TYPE;

// --------

typedef struct
{
    MME_LxAudioDecoderFrameStatus_t             DecStatus;
    MME_PcmProcessingFrameExtCommonStatus_t     PcmStatus;
} MME_LxAudioDecoderFrameExtendedMpegStatus_t;

typedef struct MpegAudioCodecDecodeContext_s
{
    CodecBaseDecodeContext_t                    BaseContext;

    MME_LxAudioDecoderFrameParams_t             DecodeParameters;
    MME_LxAudioDecoderFrameExtendedMpegStatus_t DecodeStatus;
} MpegAudioCodecDecodeContext_t;

#define BUFFER_MPEG_AUDIO_CODEC_DECODE_CONTEXT  "MpegAudioCodecDecodeContext"
#define BUFFER_MPEG_AUDIO_CODEC_DECODE_CONTEXT_TYPE     {BUFFER_MPEG_AUDIO_CODEC_DECODE_CONTEXT, BufferDataTypeBase, AllocateFromOSMemory, 32, 0, true, true, sizeof(MpegAudioCodecDecodeContext_t)}

static BufferDataDescriptor_t MpegAudioCodecDecodeContextDescriptor = BUFFER_MPEG_AUDIO_CODEC_DECODE_CONTEXT_TYPE;



///
Codec_MmeAudioMpeg_c::Codec_MmeAudioMpeg_c(void)
    : Codec_MmeAudio_c()
    , DecoderId(ACC_MP2A_ID)
{
    Configuration.CodecName                             = "MPEG audio";
    Configuration.StreamParameterContextCount           = 1;
    Configuration.StreamParameterContextDescriptor      = &MpegAudioCodecStreamParameterContextDescriptor;
    Configuration.DecodeContextCount                    = 4;
    Configuration.DecodeContextDescriptor               = &MpegAudioCodecDecodeContextDescriptor;
    Configuration.MaximumSampleCount                    = MPEG_MAX_DECODED_SAMPLE_COUNT;

    AudioDecoderTransformCapabilityMask.DecoderCapabilityFlags = (1 << ACC_MP2a) /*| (1 << ACC_MP3)*/;

    AudioParametersEvents.audio_coding_type = STM_SE_STREAM_ENCODING_AUDIO_MPEG2;
}

///
Codec_MmeAudioMpeg_c::~Codec_MmeAudioMpeg_c(void)
{
    Halt();
}

////////////////////////////////////////////////////////////////////////////
///
/// Populate the supplied structure with parameters for MPEG audio.
///
///
CodecStatus_t Codec_MmeAudioMpeg_c::FillOutTransformerGlobalParameters(MME_LxAudioDecoderGlobalParams_t *GlobalParams_p)
{
    int application_type = Player->PolicyValue(Playback, Stream, PolicyAudioApplicationType);

    SE_INFO(group_decoder_audio, "Initializing MPEG layer %s audio decoder\n",
            (DecoderId == ACC_MP2A_ID ? "I/II" :
             DecoderId == ACC_MP3_ID  ? "III" :
             "unknown"));

    // check for firmware decoder existence in case of SET_GLOBAL only
    // (we don't know the frame type at init time)

    if (ParsedFrameParameters)
    {
        MpegAudioStreamParameters_t *Parsed = (MpegAudioStreamParameters_t *)ParsedFrameParameters->StreamParameterStructure;

        if (Parsed && Parsed->Layer)
        {
            int mask = (DecoderId == ACC_MP2A_ID) ? ACC_MP2a : ACC_MP3;

            if (!(AudioDecoderTransformCapability.DecoderCapabilityFlags & (1 << mask)))
            {
                CodecStatus_t CodecStatus;
                SE_ERROR("This type of MPEG Layer is not supported by the selected firmware!\n");
                // We've been that far because we thought the codec was supported and maybe it is but
                // maybe it is only supported by one of the audio CPUs.
                // terminate the current transformer
                CodecStatus = TerminateMMETransformer();

                // no sucess... then abort!
                if (CodecStatus != CodecNoError)
                {
                    Stream->MarkUnPlayable();
                    return CodecStatus; // sub-routine puts errors to console
                }

                // and check the capability again wrt the precise layer (will automatically change the Selected CPU if required).
                AudioDecoderTransformCapabilityMask.DecoderCapabilityFlags = (1 << mask);
                CodecStatus = GloballyVerifyMMECapabilities();

                if (CodecStatus == CodecNoError)
                {
                    // if the codec is indeed supported , reinitialise the transformer based on the new Selected CPU.
                    CodecStatus = InitializeMMETransformer();
                }

                // no sucess... then abort!
                if (CodecStatus != CodecNoError)
                {
                    Stream->MarkUnPlayable();
                    return CodecStatus; // sub-routine puts errors to console
                }
            }

            // Update AudioParametersEvents
            AudioParametersEvents.audio_coding_type  = (DecoderId == ACC_MP2A_ID) ? STM_SE_STREAM_ENCODING_AUDIO_MPEG2 : STM_SE_STREAM_ENCODING_AUDIO_MP3;
            AudioParametersEvents.codec_specific.mpeg_params.layer = Parsed->Layer;
        }
    }

    MME_LxAudioDecoderGlobalParams_t &GlobalParams = *GlobalParams_p;
    GlobalParams.StructSize = sizeof(MME_LxAudioDecoderGlobalParams_t);

    MME_LxMp2aConfig_t &Config = *((MME_LxMp2aConfig_t *) GlobalParams.DecConfig);
    Config.DecoderId = DecoderId;
    Config.StructSize = sizeof(Config);
    Config.Config[MP2a_CRC_ENABLE] = ACC_MME_TRUE;
    Config.Config[MP2a_LFE_ENABLE] = ACC_MME_FALSE; // low frequency effects
#if DRV_MME_MP2a_DECODER_VERSION >= 0x130302
    {
        uMp2aDRC     dynamic_range;
        dynamic_range.u32               = 0;
        // MP2 streams encoded in DVD may carry DRC info ... nothing can be inferred for other application types.
        // because there is no way for the decoder to verify the decoded info is indeed a DRC info,
        // it is preferred to condition its application to DVD streams only.
        dynamic_range.bits.DRC_Enable   = ((DRC.DRC_Enable) && (application_type == STM_SE_CTRL_VALUE_AUDIO_APPLICATION_DVD)) ?
                                          ACC_MME_TRUE : ACC_MME_FALSE;
        // If set to MP2A_PROFILE_DVB (legacy) the mp2a decoder would report DialogNorm=-23dBFS
        // unless the host specifies a non 0 value in the frame params...
        // If set to MP2A_PROFILE_ISO, it will precisely report the value given in the frame params that is it will report 0 dB
        // if the frame params says 0dB.
        dynamic_range.bits.Profile      = (application_type == STM_SE_CTRL_VALUE_AUDIO_APPLICATION_DVB) ? MP2A_PROFILE_DVB : MP2A_PROFILE_ISO;
        Config.Config[MP2a_DRC_ENABLE]  = (unsigned char) dynamic_range.u32;
    }
#else
    Config.Config[MP2a_DRC_ENABLE] = ACC_MME_FALSE; // dynamic range compression
#endif
    // enabling multi-channel decode can cause a firmware crash when decoding
    // MPEG-1 audio (without multi-channel).
    // The only known source of mpeg2 multichannel stream is DVD streams.
    // So enable MC decoding only for DVD streams and if the requested  output speaker configuration is larger than stereo.
    Config.Config[MP2a_MC_ENABLE] = ((OutmodeMain >= ACC_MODE20) && (application_type == STM_SE_CTRL_VALUE_AUDIO_APPLICATION_DVD)) ?
                                    ACC_MME_TRUE : ACC_MME_FALSE;
    Config.Config[MP2a_NIBBLE_ENABLE] = ACC_MME_FALSE; // mpeg frame parser doesn't lock on 4bits aligned frames.
    Config.Config[MP3_FREE_FORMAT]    = ACC_MME_TRUE;  // "free bit rate streams"
#if DRV_MME_MP2a_DECODER_VERSION >= 0x130129
    {
        uMp2aActionOnError action_on_error;
        action_on_error.u32                     = 0;
        action_on_error.MuteOnError             = ACC_MME_TRUE;
        action_on_error.bits.LayerSwitchTrigger = 8;
        Config.Config[MP3_MUTE_ON_ERROR]        = (unsigned char) action_on_error.u32;
    }
#else
    Config.Config[MP3_MUTE_ON_ERROR] = ACC_MME_TRUE;
#endif
    Config.Config[MP3_DECODE_LAST_FRAME] = ACC_MME_TRUE;

    return Codec_MmeAudio_c::FillOutTransformerGlobalParameters(GlobalParams_p);
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
CodecStatus_t   Codec_MmeAudioMpeg_c::FillOutTransformerInitializationParameters(void)
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
/// Populate the AUDIO_DECODER's MME_SET_GLOBAL_TRANSFORMER_PARAMS parameters for MPEG audio.
///
CodecStatus_t   Codec_MmeAudioMpeg_c::FillOutSetStreamParametersCommand(void)
{
    CodecStatus_t Status;
    MpegAudioStreamParameters_t *Parsed = (MpegAudioStreamParameters_t *)ParsedFrameParameters->StreamParameterStructure;
    MpegAudioCodecStreamParameterContext_t  *Context = (MpegAudioCodecStreamParameterContext_t *)StreamParameterContext;

// Parsed may be NULL if call to this function results from an ALSA parameter update.
    if (Parsed)
    {
        //
        // Examine the parsed stream parameters and determine what type of codec to instanciate
        //
        DecoderId = Parsed->Layer == 3 ? ACC_MP3_ID : ACC_MP2A_ID;
    }

    //
    // Now fill out the actual structure
    //
    memset(&(Context->StreamParameters), 0, sizeof(Context->StreamParameters));
    Status = FillOutTransformerGlobalParameters(&(Context->StreamParameters));

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
/// Populate the AUDIO_DECODER's MME_TRANSFORM parameters for MPEG audio.
///
CodecStatus_t   Codec_MmeAudioMpeg_c::FillOutDecodeCommand(void)
{
    MpegAudioCodecDecodeContext_t   *Context        = (MpegAudioCodecDecodeContext_t *)DecodeContext;
//MpegAudioFrameParameters_t    *Parsed         = (MpegAudioFrameParameters_t *)ParsedFrameParameters->FrameParameterStructure;
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
CodecStatus_t   Codec_MmeAudioMpeg_c::ValidateDecodeContext(CodecBaseDecodeContext_t *Context)
{
    MpegAudioCodecDecodeContext_t   *DecodeContext;
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

    DecodeContext = (MpegAudioCodecDecodeContext_t *) Context;
    Status        = &(DecodeContext->DecodeStatus.DecStatus);

    if (Status->DecStatus != ACC_MPEG2_OK && Status->DecStatus != ACC_MPEG_MC_CRC_ERROR)
    {
        SE_WARNING("MPEG audio decode error (muted frame): %d\n", Status->DecStatus);
        // don't report an error to the higher levels (because the frame is muted)
    }

    OS_LockMutex(&Lock);
    AudioParameters = BufferState[DecodeContext->BaseContext.BufferIndex].ParsedAudioParameters;

    if (AudioParameters == NULL)
    {
        SE_ERROR("(%s) - AudioParameters are NULL\n", Configuration.CodecName);
        OS_UnLockMutex(&Lock);
        return CodecError;
    }

    if (mTempoSetRatio != 0 && mTempoSetRatio != -100)
    {
        // Trickmode: Number of samples depends on playback speed
        int expectedSamples = AudioParameters->SampleCount * 100;
        expectedSamples    /= (100 + mTempoSetRatio);

        // We authorize the range [-2;+2] around the computed value as
        // the number of samples return by tempo Processing control can vary wihtin this range.
        if ((Status->NbOutSamples < (expectedSamples - 2)) || (Status->NbOutSamples > (expectedSamples + 2)))
        {
            SE_ERROR("Unexpected number of output samples:%d  input SampleCount:%d ExpectedSamples:%d Ratio:%d\n",
                     Status->NbOutSamples, AudioParameters->SampleCount, expectedSamples, mTempoSetRatio);
        }
    }
    else if ((unsigned int)Status->NbOutSamples != AudioParameters->SampleCount)   // TODO: manifest constant
    {
        // 288/144 aren't valid values (but testing showed that it was emited for some
        // of the MPEG2 streams for example WrenTesting/MPEG2_LIII/MPEG2_LIII_80kbps_24khz_s.mp3)
        // so we continue to output a message if this is observed but only if diagnostics are
        // enabled.
        if (Status->NbOutSamples == 288 || Status->NbOutSamples == 144)
        {
            SE_DEBUG(group_decoder_audio, "Unexpected number of output samples (%d)\n", Status->NbOutSamples);
        }
        else
        {
            SE_ERROR("Unexpected number of output samples (%d)\n", Status->NbOutSamples);
        }
    }

    // SYSFS
    AudioDecoderStatus = *Status;
    SetAudioCodecDecStatistics();
    //
    // Attach any codec derived metadata to the output buffer (or verify the
    // frame analysis if the frame analyser already filled everything in for
    // us).
    //
    AudioParameters->Source.BitsPerSample = AudioOutputSurface->BitsPerSample;
    AudioParameters->Source.ChannelCount = AudioOutputSurface->ChannelCount;
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
        SE_WARNING("MPEG audio decode bad sampling freq returned: 0x%x\n", SamplingFreqCode);
    }

    // Fill the parsed parameters with the mpeg stream metadata
    Codec_MmeAudioMpeg_c::FillStreamMetadata(AudioParameters, Status);
    OS_UnLockMutex(&Lock);
    // Validate the extended status (without propagating errors)
    (void) ValidatePcmProcessingExtendedStatus(Context,
                                               (MME_PcmProcessingFrameExtStatus_t *) &DecodeContext->DecodeStatus.PcmStatus);
    //SYSFS
    SetAudioCodecDecAttributes();
    // Check if a new AudioParameter event occurs
    PlayerEventRecord_t                 myEvent;
    stm_se_play_stream_audio_parameters_t   newAudioParametersValues;
    memset(&newAudioParametersValues, 0, sizeof(stm_se_play_stream_audio_parameters_t));
    // fill specific codec parameters: myEvent (for event notification) and newAudioParametersValues (for comparison)
    myEvent.ExtraValue[0].UnsignedInt =  AudioParametersEvents.codec_specific.mpeg_params.layer;
    newAudioParametersValues.codec_specific.mpeg_params.layer = AudioParametersEvents.codec_specific.mpeg_params.layer;
    CheckAudioParameterEvent(Status, (MME_PcmProcessingFrameExtStatus_t *) &DecodeContext->DecodeStatus.PcmStatus, &newAudioParametersValues, &myEvent);
    return CodecNoError;
}

// /////////////////////////////////////////////////////////////////////////
//
//      Function to dump out the set stream
//      parameters from an mme command.
//

CodecStatus_t   Codec_MmeAudioMpeg_c::DumpSetStreamParameters(void    *Parameters)
{
    SE_ERROR("Not implemented\n");
    return CodecNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Function to dump out the decode
//      parameters from an mme command.
//

CodecStatus_t   Codec_MmeAudioMpeg_c::DumpDecodeParameters(void    *Parameters)
{
    SE_ERROR("Not implemented\n");
    return CodecNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Function to set the stream metadata according to what is contained
//      in the steam bitstream (returned by the codec firmware status)
//

void Codec_MmeAudioMpeg_c::FillStreamMetadata(ParsedAudioParameters_t *AudioParameters, MME_LxAudioDecoderFrameStatus_t *Status)
{
    // this code is very fatpipe dependent, so maybe not in the right place...
    StreamMetadata_t *Metadata = &AudioParameters->StreamMetadata;
    tMME_BufferFlags *Flags = (tMME_BufferFlags *) &Status->PTSflag;
    // direct mapping
    Metadata->FrontMatrixEncoded = Flags->FrontMatrixEncoded;
    Metadata->RearMatrixEncoded  = Flags->RearMatrixEncoded;
    Metadata->DialogNorm         = Flags->DialogNorm;
    // undefined for mpeg.
    Metadata->MixLevel           = 0;
    Metadata->LfeGain            = 0;
}

////////////////////////////////////////////////////////////////////////////
///
///  Public static function to fill MPEG2A codec capabilities
///  to expose it through STM_SE_CTRL_GET_CAPABILITY_AUDIO_DECODE control.
///

void Codec_MmeAudioMpeg_c::GetCapabilities(stm_se_audio_dec_capability_t::audio_dec_mp2a_capability_s *cap,
                                           const MME_LxAudioDecoderHDInfo_t decInfo)
{
    const int ExtFlags = Codec_Capabilities_c::ExtractAudioExtendedFlags(decInfo, ACC_MP2a);
    cap->common.capable = (decInfo.DecoderCapabilityFlags     & (1 << ACC_MP2a)
                           & SE_AUDIO_DEC_CAPABILITIES        & (1 << ACC_MP2a)
                          ) ? true : false;
    cap->mp2_stereo_L1 = cap->common.capable             ? true : false;
    cap->mp2_stereo_L2 = cap->common.capable             ? true : false;
    cap->mp2_51_L1     = false;
    cap->mp2_51_L2     = (ExtFlags & (1 << ACC_MP2_MC_L2)) ? true : false;
    cap->mp2_71_L2     = (ExtFlags & (1 << ACC_MP2_71_L2)) ? true : false;
    cap->mp2_DAB       = (ExtFlags & (1 << ACC_MP2_DAB))   ? true : false;
}

////////////////////////////////////////////////////////////////////////////
///
///  Public static function to fill MP3 codec capabilities
///  to expose it through STM_SE_CTRL_GET_CAPABILITY_AUDIO_DECODE control.
///
void Codec_MmeAudioMpeg_c::GetCapabilities(stm_se_audio_dec_capability_t::audio_dec_mp3_capability_s *cap,
                                           const MME_LxAudioDecoderHDInfo_t decInfo)
{
    const int ExtFlags = Codec_Capabilities_c::ExtractAudioExtendedFlags(decInfo, ACC_MP3);
    cap->common.capable = (decInfo.DecoderCapabilityFlags     & (1 << ACC_MP3)
                           & SE_AUDIO_DEC_CAPABILITIES        & (1 << ACC_MP3)
                          ) ? true : false;
    cap->mp3_PRO      = (ExtFlags & (1 << ACC_MP3_PRO))      ? true : false;
    cap->mp3_SURROUND = (ExtFlags & (1 << ACC_MP3_SURROUND)) ? true : false;
    cap->mp3_BINAURAL = (ExtFlags & (1 << ACC_MP3_BINAURAL)) ? true : false;
    cap->mp3_HD       = (ExtFlags & (1 << ACC_MP3_HD))       ? true : false;
}
