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
/// \class Codec_MmeAudioDtshd_c
///
/// The DTSHD audio codec proxy.
///

#include "codec_mme_audio_dtshd.h"
#include "codec_capabilities.h"
#include "dtshd.h"
#include "player_generic.h"
#include "player_playback.h"
#include "player_stream.h"
#include "st_relayfs_se.h"


#undef TRACE_TAG
#define TRACE_TAG "Codec_MmeAudioDtshd_c"

typedef struct DtshdAudioCodecStreamParameterContext_s
{
    CodecBaseStreamParameterContext_t   BaseContext;

    MME_LxAudioDecoderGlobalParams_t StreamParameters;
} DtshdAudioCodecStreamParameterContext_t;

#define BUFFER_DTSHD_AUDIO_CODEC_STREAM_PARAMETER_CONTEXT        "DtshdAudioCodecStreamParameterContext"
#define BUFFER_DTSHD_AUDIO_CODEC_STREAM_PARAMETER_CONTEXT_TYPE  {BUFFER_DTSHD_AUDIO_CODEC_STREAM_PARAMETER_CONTEXT, BufferDataTypeBase, AllocateFromOSMemory, 32, 0, true, true, sizeof(DtshdAudioCodecStreamParameterContext_t)}

static BufferDataDescriptor_t DtshdAudioCodecStreamParameterContextDescriptor = BUFFER_DTSHD_AUDIO_CODEC_STREAM_PARAMETER_CONTEXT_TYPE;

// --------

#define BUFFER_DTSHD_AUDIO_CODEC_DECODE_CONTEXT "DtshdAudioCodecDecodeContext"
#define BUFFER_DTSHD_AUDIO_CODEC_DECODE_CONTEXT_TYPE    {BUFFER_DTSHD_AUDIO_CODEC_DECODE_CONTEXT, BufferDataTypeBase, AllocateFromOSMemory, 32, 0, true, true, sizeof(DtshdAudioCodecDecodeContext_t)}

static BufferDataDescriptor_t DtshdAudioCodecDecodeContextDescriptor = BUFFER_DTSHD_AUDIO_CODEC_DECODE_CONTEXT_TYPE;

////////////////////////////////////////////////////////////////////////////
///
/// Fill in the configuration parameters used by the super-class and reset everything.
///
Codec_MmeAudioDtshd_c::Codec_MmeAudioDtshd_c(bool isLbrStream)
    : Codec_MmeAudio_c()
    , DecoderId(ACC_DTS_ID)
    , IsLbrStream(isLbrStream)
{
    if (isLbrStream)
    {
        Configuration.CodecName                     = "DTS-HD LBR audio";
    }
    else
    {
        Configuration.CodecName                     = "DTS(-HD) audio";
    }

    Configuration.StreamParameterContextCount           = 1;
    Configuration.StreamParameterContextDescriptor      = &DtshdAudioCodecStreamParameterContextDescriptor;
    Configuration.DecodeContextCount                    = 4;
    Configuration.DecodeContextDescriptor               = &DtshdAudioCodecDecodeContextDescriptor;
    Configuration.TranscodedFrameMaxSize                = DTSHD_FRAME_MAX_SIZE;
    Configuration.MaximumSampleCount                    = DTSHD_MAX_DECODED_SAMPLE_COUNT;

    AudioDecoderTransformCapabilityMask.DecoderCapabilityFlags = (1 << (isLbrStream ? ACC_DTS_LBR : ACC_DTS));

    // TODO(pht) move FinalizeInit to a factory method
    InitializationStatus = FinalizeInit();
}

CodecStatus_t Codec_MmeAudioDtshd_c::FinalizeInit(void)
{
    CodecStatus_t Status = GloballyVerifyMMECapabilities();
    if (CodecNoError != Status)
    {
        SE_INFO(group_decoder_audio, "DTSHD not found\n");
        return CodecError;
    }
    return CodecNoError;
}

Codec_MmeAudioDtshd_c::~Codec_MmeAudioDtshd_c(void)
{
    Halt();
}

////////////////////////////////////////////////////////////////////////////
///
/// Populate the supplied structure with parameters for DTSHD audio.
///
CodecStatus_t Codec_MmeAudioDtshd_c::FillOutTransformerGlobalParameters(MME_LxAudioDecoderGlobalParams_t *GlobalParams_p)
{
    MME_LxAudioDecoderGlobalParams_t &GlobalParams = *GlobalParams_p;
    GlobalParams.StructSize = sizeof(MME_LxAudioDecoderGlobalParams_t);

    MME_LxDtsConfig_t &Config = *((MME_LxDtsConfig_t *) GlobalParams.DecConfig);
    memset(&Config, 0, sizeof(MME_LxDtsConfig_t));
    Config.DecoderId = ACC_DTS_ID;
    Config.StructSize = sizeof(MME_LxDtsConfig_t);
    // set the common fields
    Config.Config[DTS_CRC_ENABLE] = ACC_MME_FALSE;
    Config.Config[DTS_LFE_ENABLE] = ACC_MME_TRUE;
    Config.Config[DTS_DRC_ENABLE] = ACC_MME_FALSE;
    Config.Config[DTS_ES_ENABLE]  = ACC_MME_TRUE;
    Config.Config[DTS_96K_ENABLE] = ACC_MME_TRUE;
    Config.Config[DTS_NBBLOCKS_PER_TRANSFORM] = ACC_MME_TRUE;
    // DTS HD specific parameters... (no impact on dts decoder)
    Config.Config[DTS_XBR_ENABLE] = ACC_MME_TRUE;
    Config.Config[DTS_XLL_ENABLE] = ACC_MME_TRUE;
    Config.Config[DTS_MIX_LFE]    = ACC_MME_FALSE;
    Config.Config[DTS_LBR_ENABLE] = (IsLbrStream) ? ACC_MME_TRUE : ACC_MME_FALSE;
#define DTSHD_DRC_PERCENT 0 // set to 0 due to a bug in BL_25_10, normally to be set to 100...
    Config.PostProcessing.DRC = DTSHD_DRC_PERCENT;
    // In case of LBR, don't do any resampling
    Config.PostProcessing.SampleFreq = ACC_FS_ID;
    Config.PostProcessing.DialogNorm = ACC_MME_TRUE;
    Config.PostProcessing.Features = 0;
#if DRV_MME_DTS_DECODER_VERSION >= 0x110401
    Config.PostProcessing.DisableNavFadeIn = 0;
#endif
    Config.FirstByteEncSamples = 0;
    Config.Last4ByteEncSamples = 0;
    Config.DelayLossLess = 0;

    CodecStatus_t Status = Codec_MmeAudio_c::FillOutTransformerGlobalParameters(GlobalParams_p);
    if (Status != CodecNoError)
    {
        return Status;
    }

    unsigned char *PcmParams_p = ((unsigned char *) &Config) + Config.StructSize;
    MME_LxPcmProcessingGlobalParams_Subset_t &PcmParams =
        *((MME_LxPcmProcessingGlobalParams_Subset_t *) PcmParams_p);
    // downmix must be disabled for DTSHD
    MME_DMixGlobalParams_t &DMix = PcmParams.DMix;
    DMix.Apply = ACC_MME_DISABLED;
    return CodecNoError;
}


////////////////////////////////////////////////////////////////////////////
///
/// Populate the AUDIO_DECODER's initialization parameters for DTSHD audio.
///
/// When this method completes Codec_MmeAudio_c::AudioDecoderInitializationParameters
/// will have been filled out with valid values sufficient to initialize an
/// DTSHD audio decoder.
///
CodecStatus_t   Codec_MmeAudioDtshd_c::FillOutTransformerInitializationParameters(void)
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
/// Populate the AUDIO_DECODER's MME_SET_GLOBAL_TRANSFORMER_PARAMS parameters for DTSHD audio.
///
CodecStatus_t   Codec_MmeAudioDtshd_c::FillOutSetStreamParametersCommand(void)
{
//DtshdAudioStreamParameters_t *Parsed = (DtshdAudioStreamParameters_t *)ParsedFrameParameters->StreamParameterStructure;
    DtshdAudioCodecStreamParameterContext_t *Context = (DtshdAudioCodecStreamParameterContext_t *)StreamParameterContext;

    if (ParsedAudioParameters == NULL)
    {
        SE_ERROR("(%s) - ParsedAudioParameters are NULL\n", Configuration.CodecName);
        return CodecError;
    }

    if ((ParsedAudioParameters->OriginalEncoding != AudioOriginalEncodingDtshdLBR) && IsLbrStream)
    {
        SE_ERROR("This stream does not contain any lbr extension!\n");
        return CodecError;
    }

    TranscodeEnable = CapableOfTranscodeDtshdToDts(ParsedAudioParameters);

    //
    // Examine the parsed stream parameters and determine what type of codec to instanciate
    //
    DecoderId = ACC_DTS_ID;
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
/// Populate the AUDIO_DECODER's MME_TRANSFORM parameters for MPEG audio.
///
CodecStatus_t   Codec_MmeAudioDtshd_c::FillOutDecodeCommand(void)
{
    DtshdAudioCodecDecodeContext_t  *Context        = (DtshdAudioCodecDecodeContext_t *)DecodeContext;
//DtshdAudioFrameParameters_t   *Parsed         = (DtshdAudioFrameParameters_t *)ParsedFrameParameters->FrameParameterStructure;
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
    // report this CoreSize value
    DecodeContext->MMEPages[0].FlagsIn = ((DtshdAudioFrameParameters_t *)ParsedFrameParameters->FrameParameterStructure)->CoreSize;
    // export the frame parameters structure to the decode context (so that we can access them from the MME callback)
    memcpy(&Context->ContextFrameParameters, ParsedFrameParameters->FrameParameterStructure, sizeof(DtshdAudioFrameParameters_t));
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
CodecStatus_t   Codec_MmeAudioDtshd_c::ValidateDecodeContext(CodecBaseDecodeContext_t *Context)
{
// MME_LxAudioDecoderInitParams_t &Params = AudioDecoderInitializationParameters;
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

    DtshdAudioCodecDecodeContext_t  *LocalDecodeContext = (DtshdAudioCodecDecodeContext_t *) Context;
    MME_LxAudioDecoderFrameStatus_t *Status             = &(LocalDecodeContext->DecodeStatus.DecStatus);
    if (Status->DecStatus != MME_SUCCESS)
    {
        SE_WARNING("DTSHD audio decode error (muted frame): 0x%x\n", Status->DecStatus);
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
    ParsedAudioParameters_t *AudioParameters = BufferState[LocalDecodeContext->BaseContext.BufferIndex].ParsedAudioParameters;

    if (AudioParameters == NULL)
    {
        SE_ERROR("(%s) - AudioParameters are NULL\n", Configuration.CodecName);
        OS_UnLockMutex(&Lock);
        return CodecError;
    }

    AudioParameters->Source.BitsPerSample = AudioOutputSurface->BitsPerSample; /*(((Params.BlockWise >> 4) & 0xF) == ACC_WS32)?32:16;*/
    AudioParameters->Source.ChannelCount =  AudioOutputSurface->ChannelCount; /* ACC_AcMode2ChannelCount(Status.AudioMode) */
    AudioParameters->Organisation = Status->AudioMode;
    {
        int expected_spl = AudioParameters->SampleCount, firmware_spl = Status->NbOutSamples;
        int ratio_spl = 1, ratio_freq = 1;
        int expected_freq = AudioParameters->Source.SampleRateHz, firmware_freq = StmSeTranslateDiscreteSamplingFrequencyToInteger(Status->SamplingFreq);

        // check the firmware status against what we parsed
        if (expected_spl != firmware_spl)
        {
            ratio_spl = (expected_spl > firmware_spl) ? (expected_spl / firmware_spl) : (firmware_spl / expected_spl);
        }

        if (Status->SamplingFreq > ACC_FS_reserved)
        {
            SE_ERROR("DTSHD audio decode wrong sampling freq returned: %d\n",
                     firmware_freq);
        }

        if (firmware_freq != expected_freq)
        {
            ratio_freq = (expected_freq > firmware_freq) ? (expected_freq / firmware_freq) : (firmware_freq / expected_freq);
        }

        // the ratio between the sampling freq and the number of samples should be the same
        // (a core substream can contain extension such a DTS96)
        if ((ratio_freq != ratio_spl) && (Status->DecStatus == MME_SUCCESS))
        {
            SE_ERROR("DTSHD: Wrong ratio between expected and parsed frame porperties: nb samples: %d (expected %d), freq %d (expected %d)\n",
                     firmware_spl, expected_spl, firmware_freq, expected_freq);
        }
        else
        {
            // the firmware output is true, and is what needs to be sent to the manifestor
            AudioParameters->SampleCount = firmware_spl;
            AudioParameters->Source.SampleRateHz = firmware_freq;
        }

        unsigned int period = (AudioParameters->SampleCount * 1000) / AudioParameters->Source.SampleRateHz;

        if ((Status->ElapsedTime / 1000) > period)
        {
            SE_INFO(group_decoder_audio, "MME command took a lot of time (%d vs %d)\n",
                    Status->ElapsedTime, period);
        }
    }
    // Fill the parsed parameters with the DTS stream metadata
    Codec_MmeAudioDtshd_c::FillStreamMetadata(AudioParameters, Status);
    OS_UnLockMutex(&Lock);
    // Validate the extended status (without propagating errors)
    (void) ValidatePcmProcessingExtendedStatus(Context,
                                               (MME_PcmProcessingFrameExtStatus_t *) &LocalDecodeContext->DecodeStatus.PcmStatus);
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
            TranscodeDtshdToDts(&LocalDecodeContext->BaseContext,
                                &LocalDecodeContext->ContextFrameParameters,
                                TranscodedBuffers + LocalDecodeContext->TranscodeBufferIndex);
        }
    }

    // sysfs
    SetAudioCodecDecAttributes();
    return CodecNoError;
}


////////////////////////////////////////////////////////////////////////////
///
/// Extract mixing metadata and stuff it into the audio parameters.
///
/// \todo Can we make this code common between EAC3 and DTSHD handling.
///
void Codec_MmeAudioDtshd_c::HandleMixingMetadata(CodecBaseDecodeContext_t *Context,
                                                 MME_PcmProcessingStatusTemplate_t *PcmStatus)
{
    ParsedAudioParameters_t *AudioParameters;
    MME_LxAudioDecoderMixingMetadata_t *MixingMetadata = (MME_LxAudioDecoderMixingMetadata_t *) PcmStatus;
    int NbMixConfigurations;

    if (Context == NULL)
    {
        SE_ERROR("(%s) - CodecContext is NULL\n", Configuration.CodecName);
        return;
    }

    OS_LockMutex(&Lock);
    AudioParameters = BufferState[Context->BufferIndex].ParsedAudioParameters;

    if (AudioParameters == NULL)
    {
        SE_ERROR("(%s) - AudioParameters are NULL\n", Configuration.CodecName);
        OS_UnLockMutex(&Lock);
        return;
    }

    //
    // Validation
    //
    SE_ASSERT(MixingMetadata->MinStruct.Id == ACC_MIX_METADATA_ID);   // already checked by framework

    if (MixingMetadata->MinStruct.StructSize < DTSHD_MIN_MIXING_METADATA_SIZE)
    {
        SE_ERROR("Mixing metadata is too small (%d)\n", MixingMetadata->MinStruct.StructSize);
        OS_UnLockMutex(&Lock);
        return;
    }

    NbMixConfigurations = MixingMetadata->MinStruct.NbOutMixConfig;

    if (NbMixConfigurations > MAX_MIXING_OUTPUT_CONFIGURATION)
    {
        SE_INFO(group_decoder_audio, "Number of mix out configs is gt 3 (%d)!\n", NbMixConfigurations);
        NbMixConfigurations = MAX_MIXING_OUTPUT_CONFIGURATION;
    }

    //
    // Action
    //
    memset(&AudioParameters->MixingMetadata, 0, sizeof(AudioParameters->MixingMetadata));
    AudioParameters->MixingMetadata.IsMixingMetadataPresent = true;
    AudioParameters->MixingMetadata.PostMixGain = MixingMetadata->MinStruct.PostMixGain;
    AudioParameters->MixingMetadata.NbOutMixConfig = MixingMetadata->MinStruct.NbOutMixConfig;

    for (int i = 0; i < NbMixConfigurations; i++)
    {
        MME_MixingOutputConfiguration_t &In  = MixingMetadata->MixOutConfig[i];
        MixingOutputConfiguration_t &Out = AudioParameters->MixingMetadata.MixOutConfig[i];
        Out.AudioMode = In.AudioMode;

        for (int j = 0; j < MAX_NB_CHANNEL_COEFF; j++)
        {
            Out.PrimaryAudioGain[j] = In.PrimaryAudioGain[j];
            Out.SecondaryAudioPanCoeff[j] = In.SecondaryAudioPanCoeff[j];
        }
    }

    OS_UnLockMutex(&Lock);
}


////////////////////////////////////////////////////////////////////////////
///
/// Static method to check whether "transcode" is possible.
///
bool Codec_MmeAudioDtshd_c::CapableOfTranscodeDtshdToDts(ParsedAudioParameters_t *ParsedAudioParameters)
{
    // if the stream is dtshd, then the "transcoding" might be required
    if ((ParsedAudioParameters->OriginalEncoding == AudioOriginalEncodingDtshdMA) ||
        (ParsedAudioParameters->OriginalEncoding == AudioOriginalEncodingDtshd))
    {
        if (ParsedAudioParameters->BackwardCompatibleProperties.SampleRateHz &&
            ParsedAudioParameters->BackwardCompatibleProperties.SampleCount)
        {
            // a core is present, so transcode is possible
            return true;
        }
    }

    return false;
}


////////////////////////////////////////////////////////////////////////////
///
/// Static method that "transcodes" dtshd to dts
///
/// The transcoding consists in copying the dts core compatible substream
/// the the transcoded buffer.
///
void    Codec_MmeAudioDtshd_c::TranscodeDtshdToDts(CodecBaseDecodeContext_t     *BaseContext,
                                                   DtshdAudioFrameParameters_t *FrameParameters,
                                                   AdditionalBufferState_t      *TranscodeBuffer)
{
    MME_Command_t *Cmd     = &BaseContext->MMECommand;
    unsigned char *SrcPtr  = (unsigned char *)Cmd->DataBuffers_p[0]->ScatterPages_p[0].Page_p;
    unsigned char *DestPtr = (unsigned char *)TranscodeBuffer->BufferPointer;
    unsigned int CoreSize  = FrameParameters->CoreSize;

    memcpy(DestPtr, SrcPtr + FrameParameters->BcCoreOffset, CoreSize);
    TranscodeBuffer->Buffer->SetUsedDataSize(CoreSize);

    // revert the substream core sync to the backward dts core sync
    if (FrameParameters->IsSubStreamCore && CoreSize)
    {
        unsigned int *SyncPtr = (unsigned int *) DestPtr;
        unsigned int SyncWord = *SyncPtr;

        // it is possible at this point that the firmware has already modified the input buffer
        // and done the conversion...
        if (SyncWord == __swapbw(DTSHD_START_CODE_SUBSTREAM_CORE))
        {
            *SyncPtr = __swapbw(DTSHD_START_CODE_CORE);
        }
        else if (SyncWord != __swapbw(DTSHD_START_CODE_CORE))
        {
            SE_ERROR("Wrong Core Substream Sync (0x%x) Implementation error?\n", SyncWord);
        }
    }

    st_relayfs_write_se(ST_RELAY_TYPE_AUDIO_TRANSCODE, ST_RELAY_SOURCE_SE, DestPtr, CoreSize, 0);
}

// /////////////////////////////////////////////////////////////////////////
//
//      Function to dump out the set stream
//      parameters from an mme command.
//

CodecStatus_t   Codec_MmeAudioDtshd_c::DumpSetStreamParameters(void    *Parameters)
{
    SE_ERROR("Not implemented\n");
    return CodecNoError;
}

// /////////////////////////////////////////////////////////////////////////
//
//      Function to dump out the decode
//      parameters from an mme command.
//

CodecStatus_t   Codec_MmeAudioDtshd_c::DumpDecodeParameters(void    *Parameters)
{
    SE_ERROR("Not implemented\n");
    return CodecNoError;
}

////////////////////////////////////////////////////////////////////////////
///
///  Set Default FrameBase style TRANSFORM command for AudioDecoder MT
///  with 1 Input Buffer and 1 Output Buffer.

void Codec_MmeAudioDtshd_c::SetCommandIO(void)
{
    if (TranscodeEnable && TranscodeNeeded)
    {
        CodecStatus_t Status = GetTranscodeBuffer();
        if (Status != CodecNoError)
        {
            SE_ERROR("Error while requesting Transcoded buffer: %d. Disabling transcoding\n", Status);
            TranscodeEnable = false;
        }

        ((DtshdAudioCodecDecodeContext_t *)DecodeContext)->TranscodeBufferIndex = CurrentTranscodeBufferIndex;
    }

    Codec_MmeAudio_c::SetCommandIO();
}


// /////////////////////////////////////////////////////////////////////////
//
//      Function to set the stream metadata according to what is contained
//      in the steam bitstream (returned by the codec firmware status)
//

void Codec_MmeAudioDtshd_c::FillStreamMetadata(ParsedAudioParameters_t *AudioParameters, MME_LxAudioDecoderFrameStatus_t *Status)
{
    // this code is very fatpipe dependent, so maybe not in the right place...
    StreamMetadata_t *Metadata = &AudioParameters->StreamMetadata;
    tMME_BufferFlags *Flags = (tMME_BufferFlags *) &Status->PTSflag;
    int Temp;

    // according to fatpipe specs...
    if (Status->DecAudioMode == ACC_MODE20)
    {
        Temp = 1;
    }
    else if (Status->DecAudioMode == ACC_MODE20t)
    {
        Temp = 2;
    }
    else
    {
        Temp = 0;
    }

    Metadata->FrontMatrixEncoded = Temp;
    // the rearmatrix ecncoded flags from the status returns in fact the DTS ES flag...
    Metadata->RearMatrixEncoded  = Flags->RearMatrixEncoded ? 2 : 1;
    // according to fatpipe specs
    Metadata->MixLevel  = 0;
    // surely 0
    Metadata->DialogNorm  = Flags->DialogNorm;
    Metadata->LfeGain = 10;
}

////////////////////////////////////////////////////////////////////////////
///
///  Public static function to fill DTS codec capabilities
///  to expose it through STM_SE_CTRL_GET_CAPABILITY_AUDIO_DECODE control.
///

void Codec_MmeAudioDtshd_c::GetCapabilities(stm_se_audio_dec_capability_t::audio_dec_dts_capability_s *cap,
                                            const MME_LxAudioDecoderHDInfo_t decInfo)
{
    const int ExtFlags = Codec_Capabilities_c::ExtractAudioExtendedFlags(decInfo, ACC_DTS);
    cap->common.capable = (decInfo.DecoderCapabilityFlags     & (1 << ACC_DTS)
                           & SE_AUDIO_DEC_CAPABILITIES          & (1 << ACC_DTS)
                          ) ? true : false;
    cap->dts_ES  = (ExtFlags & (1 << ACC_DTS_ES))  ? true : false;
    cap->dts_96K = (ExtFlags & (1 << ACC_DTS_96K)) ? true : false;
    cap->dts_HD  = (ExtFlags & (1 << ACC_DTS_HD))  ? true : false;
    cap->dts_XLL = (ExtFlags & (1 << ACC_DTS_XLL)) ? true : false;
}
