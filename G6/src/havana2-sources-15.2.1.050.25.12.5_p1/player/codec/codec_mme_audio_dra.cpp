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
/// \class Codec_MmeAudioDra_c
///
/// The DRA audio codec proxy.
///

#include "codec_mme_audio_dra.h"
#include "codec_capabilities.h"
#include "dra_audio.h"

#undef TRACE_TAG
#define TRACE_TAG "Codec_MmeAudioDra_c"

typedef struct DraAudioCodecStreamParameterContext_s
{
    CodecBaseStreamParameterContext_t   BaseContext;

    MME_LxAudioDecoderGlobalParams_t    StreamParameters;
} DraAudioCodecStreamParameterContext_t;

#define BUFFER_DRA_AUDIO_CODEC_STREAM_PARAMETER_CONTEXT         "DraAudioCodecStreamParameterContext"
#define BUFFER_DRA_AUDIO_CODEC_STREAM_PARAMETER_CONTEXT_TYPE    {BUFFER_DRA_AUDIO_CODEC_STREAM_PARAMETER_CONTEXT, BufferDataTypeBase, AllocateFromOSMemory, 32, 0, true, true, sizeof(DraAudioCodecStreamParameterContext_t)}

static BufferDataDescriptor_t DraAudioCodecStreamParameterContextDescriptor   = BUFFER_DRA_AUDIO_CODEC_STREAM_PARAMETER_CONTEXT_TYPE;

typedef struct
{
    MME_LxAudioDecoderFrameStatus_t             DecStatus;
    MME_PcmProcessingFrameExtCommonStatus_t     PcmStatus;
} MME_LxAudioDecoderFrameExtendedDraStatus_t;

//
typedef struct DraAudioCodecDecodeContext_s
{
    CodecBaseDecodeContext_t                    BaseContext;

    MME_LxAudioDecoderFrameParams_t             DecodeParameters;
    MME_LxAudioDecoderFrameExtendedDraStatus_t  DecodeStatus;
} DraAudioCodecDecodeContext_t;

#define BUFFER_DRA_AUDIO_CODEC_DECODE_CONTEXT                   "DraAudioCodecDecodeContext"
#define BUFFER_DRA_AUDIO_CODEC_DECODE_CONTEXT_TYPE              {BUFFER_DRA_AUDIO_CODEC_DECODE_CONTEXT, BufferDataTypeBase, AllocateFromOSMemory, 32, 0, true, true, sizeof(DraAudioCodecDecodeContext_t)}

static BufferDataDescriptor_t DraAudioCodecDecodeContextDescriptor    = BUFFER_DRA_AUDIO_CODEC_DECODE_CONTEXT_TYPE;

///
/// Fill in the configuration parameters used by the super-class and reset everything.
///
Codec_MmeAudioDra_c::Codec_MmeAudioDra_c(void)
    : Codec_MmeAudio_c()
    , DecoderId(ACC_DRA_ID)
{
    Configuration.CodecName                             = "DRA audio";
    Configuration.StreamParameterContextCount           = 1;
    Configuration.StreamParameterContextDescriptor      = &DraAudioCodecStreamParameterContextDescriptor;
    Configuration.DecodeContextCount                    = 4;
    Configuration.DecodeContextDescriptor               = &DraAudioCodecDecodeContextDescriptor;
    Configuration.MaximumSampleCount                    = DRA_MAX_DECODED_SAMPLE_COUNT;

    AudioDecoderTransformCapabilityMask.DecoderCapabilityFlags = (1 << ACC_DRA);
}


////////////////////////////////////////////////////////////////////////////
///
///     Destructor function, ensures a full halt and reset
///     are executed for all levels of the class.
///
Codec_MmeAudioDra_c::~Codec_MmeAudioDra_c(void)
{
    Halt();
}

////////////////////////////////////////////////////////////////////////////
///
/// Populate the supplied structure with parameters for DRA audio.
///

CodecStatus_t Codec_MmeAudioDra_c::FillOutTransformerGlobalParameters(MME_LxAudioDecoderGlobalParams_t *GlobalParams_p)
{
    SE_INFO(group_decoder_audio, "Initializing DRA %s audio decoder\n",
            ((DecoderId == ACC_DRA_ID) ? "DRA decoder" :
             "unknown"));

    MME_LxAudioDecoderGlobalParams_t &GlobalParams = *GlobalParams_p;
    GlobalParams.StructSize = sizeof(MME_LxAudioDecoderGlobalParams_t);

    MME_LxDRAConfig_t &Config = *((MME_LxDRAConfig_t *) GlobalParams.DecConfig);
    Config.StructSize                      = sizeof(MME_LxDRAConfig_t);
    Config.DecoderId                       = DecoderId;
    Config.Config[DRA_LFE_ENABLE]          = (U8) ACC_MME_TRUE;

    return Codec_MmeAudio_c::FillOutTransformerGlobalParameters(GlobalParams_p);
}


////////////////////////////////////////////////////////////////////////////
///
/// Populate the AUDIO_DECODER's initialization parameters for DRA audio.
///
/// When this method completes Codec_MmeAudio_c::AudioDecoderInitializationParameters
/// will have been filled out with valid values sufficient to initialize a
/// DRA audio decoder (defaults to MPEG Layer II but can be updated by new
/// stream parameters).
///
CodecStatus_t   Codec_MmeAudioDra_c::FillOutTransformerInitializationParameters(void)
{
    MME_LxAudioDecoderInitParams_t &Params = AudioDecoderInitializationParameters;

    MMEInitializationParameters.TransformerInitParamsSize = sizeof(Params);
    MMEInitializationParameters.TransformerInitParams_p   = &Params;

    CodecStatus_t Status = Codec_MmeAudio_c::FillOutTransformerInitializationParameters();
    if (Status != CodecNoError)
    {
        return Status;
    }

    return FillOutTransformerGlobalParameters(&Params.GlobalParams);
}


////////////////////////////////////////////////////////////////////////////
///
/// Populate the AUDIO_DECODER's MME_SET_GLOBAL_TRANSFORMER_PARAMS parameters for DRA audio.
///
CodecStatus_t   Codec_MmeAudioDra_c::FillOutSetStreamParametersCommand(void)
{
    CodecStatus_t Status;
    DraAudioCodecStreamParameterContext_t  *Context = (DraAudioCodecStreamParameterContext_t *)StreamParameterContext;

    memset(&(Context->StreamParameters), 0, sizeof(Context->StreamParameters));
    Status = FillOutTransformerGlobalParameters(&(Context->StreamParameters));

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

////////////////////////////////////////////////////////////////////////////
///
/// Populate the AUDIO_DECODER's MME_TRANSFORM parameters for DRA.
///
CodecStatus_t   Codec_MmeAudioDra_c::FillOutDecodeCommand(void)
{
    DraAudioCodecDecodeContext_t *Context = (DraAudioCodecDecodeContext_t *)DecodeContext;

    memset(&Context->DecodeParameters, 0, sizeof(Context->DecodeParameters));

    bool mute = false;
    Player->GetControl(Playback, Stream, STM_SE_CTRL_PLAY_STREAM_MUTE, &mute);
    Context->DecodeParameters.Cmd = mute ? ACC_CMD_MUTE : ACC_CMD_PLAY;

    memset(&Context->DecodeStatus, 0, sizeof(Context->DecodeStatus));

    // Fill out the actual command
    Context->BaseContext.MMECommand.CmdStatus.AdditionalInfoSize = sizeof(Context->DecodeStatus);
    Context->BaseContext.MMECommand.CmdStatus.AdditionalInfo_p   = (MME_GenericParams_t)(&Context->DecodeStatus);
    Context->BaseContext.MMECommand.ParamSize                    = sizeof(Context->DecodeParameters);
    Context->BaseContext.MMECommand.Param_p                      = (MME_GenericParams_t)(&Context->DecodeParameters);

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
CodecStatus_t   Codec_MmeAudioDra_c::ValidateDecodeContext(CodecBaseDecodeContext_t *Context)
{
    DraAudioCodecDecodeContext_t    *DecodeContext;
    MME_LxAudioDecoderFrameStatus_t *Status ;
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

    DecodeContext = (DraAudioCodecDecodeContext_t *) Context;
    Status        = &(DecodeContext->DecodeStatus.DecStatus);
    // SYSFS
    AudioDecoderStatus = *Status;
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
        SE_ERROR("DRA audio decode bad sampling freq returned: 0x%x\n", SamplingFreqCode);
    }

    OS_UnLockMutex(&Lock);
    // Validate the extended status (without propagating errors)
    (void) ValidatePcmProcessingExtendedStatus(Context,
                                               (MME_PcmProcessingFrameExtStatus_t *) &DecodeContext->DecodeStatus.PcmStatus);
    return CodecNoError;
}

// /////////////////////////////////////////////////////////////////////////
//
//      Function to dump out the set stream
//      parameters from an mme command.
//

CodecStatus_t   Codec_MmeAudioDra_c::DumpSetStreamParameters(void *Parameters)
{
    SE_ERROR("Not implemented\n");
    return CodecNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Function to dump out the decode
//      parameters from an mme command.
//

CodecStatus_t   Codec_MmeAudioDra_c::DumpDecodeParameters(void *Parameters)
{
    SE_ERROR("Not implemented\n");
    return CodecNoError;
}

////////////////////////////////////////////////////////////////////////////
///
///  Public static function to fill DRA codec capabilities
///  to expose it through STM_SE_CTRL_GET_CAPABILITY_AUDIO_DECODE control.
///
void Codec_MmeAudioDra_c::GetCapabilities(stm_se_audio_dec_capability_t::audio_dec_dra_capability_s *cap,
                                          const MME_LxAudioDecoderHDInfo_t decInfo)
{
    cap->common.capable = (decInfo.DecoderCapabilityFlags     & (1 << ACC_DRA)
                           & SE_AUDIO_DEC_CAPABILITIES        & (1 << ACC_DRA)
                          ) ? true : false;
}
