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
/// \class Codec_MmeAudioRma_c
///
/// The RMA audio codec proxy.
///

#include "codec_mme_audio_rma.h"
#include "codec_capabilities.h"
#include "rma_audio.h"

#undef TRACE_TAG
#define TRACE_TAG "Codec_MmeAudioRma_c"

static inline unsigned int BE2LE(unsigned int Value)
{
    return (((Value & 0xff) << 24) | ((Value & 0xff00) << 8) | ((Value >> 8) & 0xff00) | ((Value >> 24) & 0xff));
}

typedef struct RmaAudioCodecStreamParameterContext_s
{
    CodecBaseStreamParameterContext_t   BaseContext;

    MME_LxAudioDecoderGlobalParams_t StreamParameters;
} RmaAudioCodecStreamParameterContext_t;

#define BUFFER_RMA_AUDIO_CODEC_STREAM_PARAMETER_CONTEXT         "RmaAudioCodecStreamParameterContext"
#define BUFFER_RMA_AUDIO_CODEC_STREAM_PARAMETER_CONTEXT_TYPE    {BUFFER_RMA_AUDIO_CODEC_STREAM_PARAMETER_CONTEXT, BufferDataTypeBase, AllocateFromOSMemory, 32, 0, true, true, sizeof(RmaAudioCodecStreamParameterContext_t)}

static BufferDataDescriptor_t           RmaAudioCodecStreamParameterContextDescriptor   = BUFFER_RMA_AUDIO_CODEC_STREAM_PARAMETER_CONTEXT_TYPE;

typedef struct
{
    MME_LxAudioDecoderFrameStatus_t             DecStatus;
    MME_PcmProcessingFrameExtCommonStatus_t     PcmStatus;
} MME_LxAudioDecoderFrameExtendedRmaStatus_t;

typedef struct RmaAudioCodecDecodeContext_s
{
    CodecBaseDecodeContext_t            BaseContext;

    MME_LxAudioDecoderFrameParams_t     DecodeParameters;
    MME_LxAudioDecoderFrameExtendedRmaStatus_t     DecodeStatus;
} RmaAudioCodecDecodeContext_t;

#define BUFFER_RMA_AUDIO_CODEC_DECODE_CONTEXT                   "RmaAudioCodecDecodeContext"
#define BUFFER_RMA_AUDIO_CODEC_DECODE_CONTEXT_TYPE              {BUFFER_RMA_AUDIO_CODEC_DECODE_CONTEXT, BufferDataTypeBase, AllocateFromOSMemory, 32, 0, true, true, sizeof(RmaAudioCodecDecodeContext_t)}

static BufferDataDescriptor_t           RmaAudioCodecDecodeContextDescriptor    = BUFFER_RMA_AUDIO_CODEC_DECODE_CONTEXT_TYPE;

Codec_MmeAudioRma_c::Codec_MmeAudioRma_c(void)
    : Codec_MmeAudio_c()
    , DecoderId(ACC_REALAUDIO_ID)
    , RestartTransformer(ACC_MME_FALSE)
{
    Configuration.CodecName                             = "Rma audio";
    Configuration.StreamParameterContextCount           = 1;
    Configuration.StreamParameterContextDescriptor      = &RmaAudioCodecStreamParameterContextDescriptor;
    Configuration.DecodeContextCount                    = 4;
    Configuration.DecodeContextDescriptor               = &RmaAudioCodecDecodeContextDescriptor;
    Configuration.MaximumSampleCount                    = RMA_MAX_DECODED_SAMPLE_COUNT;

    AudioDecoderTransformCapabilityMask.DecoderCapabilityFlags = (1 << ACC_REAL_AUDIO);
}

Codec_MmeAudioRma_c::~Codec_MmeAudioRma_c(void)
{
    Halt();
}

//{{{  FillOutTransformerGlobalParameters
////////////////////////////////////////////////////////////////////////////
///
/// Populate the supplied structure with parameters for RMA audio.
///
///
CodecStatus_t Codec_MmeAudioRma_c::FillOutTransformerGlobalParameters(MME_LxAudioDecoderGlobalParams_t *GlobalParams_p)
{
    MME_LxAudioDecoderGlobalParams_t   &GlobalParams    = *GlobalParams_p;
    SE_INFO(group_decoder_audio, "Initializing RMA audio decoder\n");
    GlobalParams.StructSize             = sizeof(MME_LxAudioDecoderGlobalParams_t);
    MME_LxRealAudioConfig_t &Config     = *((MME_LxRealAudioConfig_t *)GlobalParams.DecConfig);
    Config.DecoderId                    = DecoderId;
    Config.StructSize                   = sizeof(Config);
#if DRV_MULTICOM_AUDIO_DECODER_VERSION >= 0x090128

    if ((ParsedFrameParameters != NULL) && (ParsedFrameParameters->StreamParameterStructure != NULL))
    {
        RmaAudioStreamParameters_s     *StreamParams    = (RmaAudioStreamParameters_s *)ParsedFrameParameters->StreamParameterStructure;
        Config.CodecType                = BE2LE(StreamParams->CodecId);
        Config.usNumChannels            = StreamParams->ChannelCount;
        Config.usFlavorIndex            = StreamParams->CodecFlavour;
        Config.ulSampleRate             = StreamParams->SampleRate;
        Config.ulBitsPerFrame           = StreamParams->SubPacketSize << 3;
        Config.ulGranularity            = StreamParams->FrameSize;
        Config.ulGranularity            = StreamParams->SubPacketSize;
        Config.ulOpaqueDataSize         = StreamParams->CodecOpaqueDataLength;
    }

    Config.NbSample2Conceal             = 0;
    Config.Features                     = 0;
    RestartTransformer                  = ACC_MME_TRUE;

    if (SE_IS_DEBUG_ON(group_decoder_audio))
    {
        SE_DEBUG(group_decoder_audio, " DecoderId                  %d\n", Config.DecoderId);
        SE_DEBUG(group_decoder_audio, " StructSize                 %d\n", Config.StructSize);
        SE_DEBUG(group_decoder_audio, " CodecType                %.4s\n", (unsigned char *)&Config.CodecType);
        SE_DEBUG(group_decoder_audio, " usNumChannels              %d\n", Config.usNumChannels);
        SE_DEBUG(group_decoder_audio, " usFlavorIndex              %d\n", Config.usFlavorIndex);
        SE_DEBUG(group_decoder_audio, " NbSample2Conceal           %d\n", Config.NbSample2Conceal);
        SE_DEBUG(group_decoder_audio, " ulSampleRate               %d\n", Config.ulSampleRate);
        SE_DEBUG(group_decoder_audio, " ulBitsPerFrame             %d\n", Config.ulBitsPerFrame);
        SE_DEBUG(group_decoder_audio, " ulGranularity              %d\n", Config.ulGranularity);
        SE_DEBUG(group_decoder_audio, " ulOpaqueDataSize           %d\n", Config.ulOpaqueDataSize);
        SE_DEBUG(group_decoder_audio, " Features                   %d\n", Config.Features);
    }

#endif // DRV_MULTICOM_AUDIO_DECODER_VERSION >= 0x090128
    return Codec_MmeAudio_c::FillOutTransformerGlobalParameters(GlobalParams_p);
}
//}}}
//{{{  FillOutTransformerInitializationParameters
////////////////////////////////////////////////////////////////////////////
///
/// Populate the AUDIO_DECODER's initialization parameters for MPEG audio.
///
/// When this method completes Codec_MmeAudio_c::AudioDecoderInitializationParameters
/// will have been filled out with valid values sufficient to initialize an
/// MPEG audio decoder (defaults to MPEG Layer II but can be updated by new
/// stream parameters).
///
CodecStatus_t   Codec_MmeAudioRma_c::FillOutTransformerInitializationParameters(void)
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
/// Populate the AUDIO_DECODER's MME_SET_GLOBAL_TRANSFORMER_PARAMS parameters for RMA audio.
///
CodecStatus_t   Codec_MmeAudioRma_c::FillOutSetStreamParametersCommand(void)
{
    CodecStatus_t                               Status;
    RmaAudioCodecStreamParameterContext_t      *Context = (RmaAudioCodecStreamParameterContext_t *)StreamParameterContext;
    DecoderId           = ACC_REALAUDIO_ID;
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
/// Populate the AUDIO_DECODER's MME_TRANSFORM parameters for Real audio.
///
CodecStatus_t   Codec_MmeAudioRma_c::FillOutDecodeCommand(void)
{
    RmaAudioCodecDecodeContext_t    *Context                            = (RmaAudioCodecDecodeContext_t *)DecodeContext;
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
CodecStatus_t   Codec_MmeAudioRma_c::ValidateDecodeContext(CodecBaseDecodeContext_t *Context)
{
    RmaAudioCodecDecodeContext_t       *DecodeContext;
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

    DecodeContext   = (RmaAudioCodecDecodeContext_t *)Context;
    Status          = &(DecodeContext->DecodeStatus.DecStatus);

    if (Status->DecStatus != ACC_HXR_OK)
    {
        SE_WARNING("RMA audio decode error (muted frame): %d\n", Status->DecStatus);
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
        SE_ERROR("RMA audio decode bad sampling freq returned: 0x%x\n", SamplingFreqCode);
    }

    if (SE_IS_DEBUG_ON(group_decoder_audio))
    {
        SE_DEBUG(group_decoder_audio, "Source.BitsPerSample         %d\n", AudioParameters->Source.BitsPerSample);
        SE_DEBUG(group_decoder_audio, "Source.ChannelCount          %d\n", AudioParameters->Source.ChannelCount);
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

CodecStatus_t   Codec_MmeAudioRma_c::DumpSetStreamParameters(void    *Parameters)
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

CodecStatus_t   Codec_MmeAudioRma_c::DumpDecodeParameters(void    *Parameters)
{
    SE_VERBOSE(group_decoder_audio, " : TotalSize[0]                  %d\n", DecodeContext->MMEBuffers[0].TotalSize);
    SE_VERBOSE(group_decoder_audio, " : Page_p[0]                     %p\n", DecodeContext->MMEPages[0].Page_p);
    SE_VERBOSE(group_decoder_audio, " : TotalSize[1]                  %d\n", DecodeContext->MMEBuffers[1].TotalSize);
    SE_VERBOSE(group_decoder_audio, " : Page_p[1]                     %p\n", DecodeContext->MMEPages[1].Page_p);
    return CodecNoError;
}

//}}}
//{{{ GetCapabilities
////////////////////////////////////////////////////////////////////////////
///
///  Public static function to fill REALAUDIO codec capabilities
///  to expose it through STM_SE_CTRL_GET_CAPABILITY_AUDIO_DECODE control.
///
void Codec_MmeAudioRma_c::GetCapabilities(stm_se_audio_dec_capability_t::audio_dec_realaudio_capability_s *cap,
                                          const MME_LxAudioDecoderHDInfo_t decInfo)
{
    const int ExtFlags = Codec_Capabilities_c::ExtractAudioExtendedFlags(decInfo, ACC_REAL_AUDIO);
    cap->common.capable = (decInfo.DecoderCapabilityFlags     & (1 << ACC_REAL_AUDIO)
                           & SE_AUDIO_DEC_CAPABILITIES        & (1 << ACC_REAL_AUDIO)
                          ) ? true : false;
    cap->ra_COOK   = (ExtFlags & (1 << ACC_RA_COOK))   ? true : false;
    cap->ra_LSD    = (ExtFlags & (1 << ACC_RA_LSD))    ? true : false;
    cap->ra_AAC    = (ExtFlags & (1 << ACC_RA_AAC))    ? true : false;
    cap->ra_DEPACK = (ExtFlags & (1 << ACC_RA_DEPACK)) ? true : false;
}

//}}}
