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
/// \class Codec_MmeAudioSilence_c
///
/// A silence generating audio 'codec' to replace the main codec when licensing is insufficient.
///


#include "codec_mme_audio_silence.h"
#include "codec_mme_audio_dtshd.h"
#include "player_generic.h"
#include "player_playback.h"
#include "player_stream.h"

#undef TRACE_TAG
#define TRACE_TAG "Codec_MmeAudioSilence_c"

typedef struct SilentAudioCodecStreamParameterContext_s
{
    CodecBaseStreamParameterContext_t   BaseContext;
} SilentAudioCodecStreamParameterContext_t;

#define BUFFER_SILENT_AUDIO_CODEC_STREAM_PARAMETER_CONTEXT                "SilentAudioCodecStreamParameterContext"
#define BUFFER_SILENT_AUDIO_CODEC_STREAM_PARAMETER_CONTEXT_TYPE   {BUFFER_SILENT_AUDIO_CODEC_STREAM_PARAMETER_CONTEXT, BufferDataTypeBase, AllocateFromOSMemory, 32, 0, true, true, sizeof(SilentAudioCodecStreamParameterContext_t)}

static BufferDataDescriptor_t            SilentAudioCodecStreamParameterContextDescriptor = BUFFER_SILENT_AUDIO_CODEC_STREAM_PARAMETER_CONTEXT_TYPE;

// --------

typedef union SilentAudioFrameParameters_s
{
    void                       *OtherAudioFrameParameters;
    DtshdAudioFrameParameters_t DtshdAudioFrameParameters;
} SilentAudioFrameParameters_t;

typedef struct SilentAudioCodecDecodeContext_s
{
    CodecBaseDecodeContext_t            BaseContext;
    unsigned int                        TranscodeBufferIndex;
    SilentAudioFrameParameters_t        ContextFrameParameters;
} SilentAudioCodecDecodeContext_t;

#define BUFFER_SILENT_AUDIO_CODEC_DECODE_CONTEXT  "SilentAudioCodecDecodeContext"
#define BUFFER_SILENT_AUDIO_CODEC_DECODE_CONTEXT_TYPE     {BUFFER_SILENT_AUDIO_CODEC_DECODE_CONTEXT, BufferDataTypeBase, AllocateFromOSMemory, 32, 0, true, true, sizeof(SilentAudioCodecDecodeContext_t)}

static BufferDataDescriptor_t            SilentAudioCodecDecodeContextDescriptor = BUFFER_SILENT_AUDIO_CODEC_DECODE_CONTEXT_TYPE;

////////////////////////////////////////////////////////////////////////////
///
Codec_MmeAudioSilence_c::Codec_MmeAudioSilence_c(void)
    : Codec_MmeAudio_c()
{
    Configuration.CodecName                             = "Silence generator";
    Configuration.StreamParameterContextCount           = 1;
    Configuration.StreamParameterContextDescriptor      = &SilentAudioCodecStreamParameterContextDescriptor;
    Configuration.DecodeContextCount                    = 4;
    Configuration.DecodeContextDescriptor               = &SilentAudioCodecDecodeContextDescriptor;
    Configuration.TransformName[0]                      = "SILENCE_GENERATOR";
    Configuration.TransformName[1]                      = "SILENCE_GENERATOR";
    Configuration.AvailableTransformers                 = 2;
    Configuration.AddressingMode                        = CachedAddress;
    // silencegen only support 'transcoding' of DTS-HD (where transcode actually means trivial core extraction)
    Configuration.TranscodedFrameMaxSize                = DTSHD_FRAME_MAX_SIZE;
    Configuration.MaximumSampleCount                    = SILENCE_GENERATOR_MAX_SAMPLE_COUNT;

    ProtectTransformName = true;

    // TODO(pht) move FinalizeInit to a factory method
    InitializationStatus = FinalizeInit();
}

////////////////////////////////////////////////////////////////////////////
///
CodecStatus_t Codec_MmeAudioSilence_c::FinalizeInit(void)
{
    CodecStatus_t Status = GloballyVerifyMMECapabilities();
    if (CodecNoError != Status)
    {
        SE_ERROR("Silence generator not found (module not installed?)\n");
        return CodecError;
    }
    return CodecNoError;
}

////////////////////////////////////////////////////////////////////////////
///
Codec_MmeAudioSilence_c::~Codec_MmeAudioSilence_c(void)
{
    Halt();
}

////////////////////////////////////////////////////////////////////////////
///
/// Examine the capability structure returned by the firmware.
///
/// Unconditionally return success; the silence generator does not report
/// anything other than a version number.
///
CodecStatus_t   Codec_MmeAudioSilence_c::ParseCapabilities(unsigned int ActualTransformer)
{
    return CodecNoError;
}

CodecStatus_t   Codec_MmeAudioSilence_c::HandleCapabilities(unsigned int ActualTransformer)
{
    return CodecNoError;
}

CodecStatus_t   Codec_MmeAudioSilence_c::HandleCapabilities(void)
{
    return CodecNoError;
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
CodecStatus_t   Codec_MmeAudioSilence_c::FillOutTransformerInitializationParameters(void)
{
    MMEInitializationParameters.TransformerInitParamsSize = 0;
    MMEInitializationParameters.TransformerInitParams_p = NULL;
    return CodecNoError;
}


////////////////////////////////////////////////////////////////////////////
///
/// Populate the (non-existant) MME_SET_GLOBAL_TRANSFORMER_PARAMS parameters.
///
CodecStatus_t   Codec_MmeAudioSilence_c::FillOutSetStreamParametersCommand(void)
{
    if (ParsedAudioParameters == NULL)
    {
        SE_ERROR("(%s) - ParsedAudioParameters are NULL\n", Configuration.CodecName);
        return CodecError;
    }

    TranscodeEnable = Codec_MmeAudioDtshd_c::CapableOfTranscodeDtshdToDts(ParsedAudioParameters);
    //
    // Fill out the actual command
    //
    StreamParameterContext->MMECommand.CmdStatus.AdditionalInfoSize        = 0;
    StreamParameterContext->MMECommand.CmdStatus.AdditionalInfo_p          = NULL;
    StreamParameterContext->MMECommand.ParamSize                           = 0;
    StreamParameterContext->MMECommand.Param_p                             = NULL;
//
    return CodecNoError;
}

////////////////////////////////////////////////////////////////////////////
///
/// Populate the (non-existant) MME_TRANSFORM parameters.
///
CodecStatus_t   Codec_MmeAudioSilence_c::FillOutDecodeCommand(void)
{
    SilentAudioCodecDecodeContext_t  *Context        = (SilentAudioCodecDecodeContext_t *)DecodeContext;
    // export the frame parameters structure to the decode context (so that we can access them from the MME callback)
    memcpy(&Context->ContextFrameParameters, ParsedFrameParameters->FrameParameterStructure, sizeof(DtshdAudioFrameParameters_t));
    //
    // Fill out the actual command
    //
    DecodeContext->MMECommand.CmdStatus.AdditionalInfoSize        = 0;
    DecodeContext->MMECommand.CmdStatus.AdditionalInfo_p          = NULL;
    DecodeContext->MMECommand.ParamSize                           = 0;
    DecodeContext->MMECommand.Param_p                             = NULL;
    return CodecNoError;
}

////////////////////////////////////////////////////////////////////////////
///
/// Validate the (non-existant) reply from the transformer.
///
/// Unconditionally return success. Given the whole point of the transformer is
/// to mute the output it cannot 'successfully' fail in the way the audio firmware
/// does.
///
/// \return CodecNoError
///
CodecStatus_t   Codec_MmeAudioSilence_c::ValidateDecodeContext(CodecBaseDecodeContext_t *Context)
{
    memset(&AudioDecoderStatus, 0, sizeof(AudioDecoderStatus));     // SYSFS

    if (Context == NULL)
    {
        SE_ERROR("(%s) - CodecContext is NULL\n", Configuration.CodecName);
        return CodecError;
    }

    SilentAudioCodecDecodeContext_t *LocalDecodeContext = (SilentAudioCodecDecodeContext_t *) Context;

    /* To determine if we must perform a transcode,
     * let's check if a transcode buffer has been attached to the CodedDataBuffer*/
    Buffer_t codedDataBuffer;
    Context->DecodeContextBuffer->ObtainAttachedBufferReference(CodedFrameBufferType, &codedDataBuffer);
    if (codedDataBuffer != NULL)
    {
        Buffer_t TranscodeBuffer;
        codedDataBuffer->ObtainAttachedBufferReference(TranscodedFrameBufferType, &TranscodeBuffer);
        if (TranscodeBuffer != NULL)
        {
            SE_VERBOSE(group_decoder_audio, "OutputBuffer for transcoding is available => perform DTS-HD to DTS transcode\n");
            Codec_MmeAudioDtshd_c::TranscodeDtshdToDts(&LocalDecodeContext->BaseContext,
                                                       &LocalDecodeContext->ContextFrameParameters.DtshdAudioFrameParameters,
                                                       TranscodedBuffers + LocalDecodeContext->TranscodeBufferIndex);
            TranscodeBuffer = NULL;
        }

        codedDataBuffer = NULL;
    }

    return CodecNoError;
}

// /////////////////////////////////////////////////////////////////////////
//
//      Function to dump out the set stream
//      parameters from an mme command.
//

CodecStatus_t   Codec_MmeAudioSilence_c::DumpSetStreamParameters(void    *Parameters)
{
    SE_ERROR("Not implemented\n");
    return CodecNoError;
}

// /////////////////////////////////////////////////////////////////////////
//
//      Function to dump out the decode
//      parameters from an mme command.
//

CodecStatus_t   Codec_MmeAudioSilence_c::DumpDecodeParameters(void    *Parameters)
{
    SE_ERROR("Not implemented\n");
    return CodecNoError;
}

////////////////////////////////////////////////////////////////////////////
///
///  Set Default FrameBase style TRANSFORM command for AudioDecoder MT
///  with 1 Input Buffer and 1 Output Buffer.

void Codec_MmeAudioSilence_c::SetCommandIO(void)
{
    if (TranscodeEnable && TranscodeNeeded)
    {
        CodecStatus_t Status = GetTranscodeBuffer();

        if (Status != CodecNoError)
        {
            SE_ERROR("Error while requesting Transcoded buffer: %d. Disabling transcoding\n", Status);
            TranscodeEnable = false;
        }

        ((SilentAudioCodecDecodeContext_t *)DecodeContext)->TranscodeBufferIndex = CurrentTranscodeBufferIndex;
    }

    Codec_MmeAudio_c::SetCommandIO();
}
