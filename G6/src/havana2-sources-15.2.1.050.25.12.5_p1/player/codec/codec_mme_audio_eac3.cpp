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

#include "codec_mme_audio_eac3.h"
#include "codec_capabilities.h"
#include "eac3_audio.h"
#include "player_generic.h"
#include "player_playback.h"
#include "player_stream.h"

#include "st_relayfs_se.h"

#undef TRACE_TAG
#define TRACE_TAG "Codec_MmeAudioEAc3_c"

#define EAC3_TRANSCODE_STATUS_OK 2  // no error while transcoding
#define EAC3_DECODE_STATUS_OK    0  // no error while decoding
#define EAC3_SYNC_STATUS_OK      2  // sync status: still synchronized (no change in sync)
#define EAC3_SYNC_STATUS_2_OK    4  // sync status: just synchronized
#define EAC3_TRANSCODING_STATUS_OK   ((EAC3_DECODE_STATUS_OK << 16) | (EAC3_TRANSCODE_STATUS_OK << 8) | EAC3_SYNC_STATUS_OK)
#define EAC3_TRANSCODING_STATUS_2_OK ((EAC3_DECODE_STATUS_OK << 16) | (EAC3_TRANSCODE_STATUS_OK << 8) | EAC3_SYNC_STATUS_2_OK)
#define EAC3_TRANSCODING_DEC_STATUS_SHIFT  16

typedef struct EAc3AudioCodecStreamParameterContext_s
{
    CodecBaseStreamParameterContext_t   BaseContext;

    MME_LxAudioDecoderGlobalParams_t StreamParameters;
} EAc3AudioCodecStreamParameterContext_t;

#define BUFFER_EAC3_AUDIO_CODEC_STREAM_PARAMETER_CONTEXT                "EAc3AudioCodecStreamParameterContext"
#define BUFFER_EAC3_AUDIO_CODEC_STREAM_PARAMETER_CONTEXT_TYPE   {BUFFER_EAC3_AUDIO_CODEC_STREAM_PARAMETER_CONTEXT, BufferDataTypeBase, AllocateFromOSMemory, 32, 0, true, true, sizeof(EAc3AudioCodecStreamParameterContext_t)}

static BufferDataDescriptor_t  EAc3AudioCodecStreamParameterContextDescriptor = BUFFER_EAC3_AUDIO_CODEC_STREAM_PARAMETER_CONTEXT_TYPE;

// --------

typedef struct
{
    MME_LxAudioDecoderFrameStatus_t              DecStatus;
    MME_PcmProcessingFrameExtCommonStatus_t      PcmStatus;
    MME_LxAudioDecoderMixingMetadata_t           MixingMetadata;
} MME_LxAudioDecoderFrameExtendedEc3Status_t;

typedef struct EAc3AudioCodecDecodeContext_s
{
    CodecBaseDecodeContext_t                    BaseContext;

    MME_LxAudioDecoderFrameParams_t             DecodeParameters;
    MME_LxAudioDecoderFrameExtendedEc3Status_t  DecodeStatus;
    unsigned int                                TranscodeBufferIndex;
    unsigned int                                AuxBufferIndex;
} EAc3AudioCodecDecodeContext_t;

#define EAC3_MIN_MIXING_METADATA_SIZE       (sizeof(MME_LxAudioDecoderMixingMetadata_t) - sizeof(MME_MixingOutputConfiguration_t))
#define EAC3_MIN_MIXING_METADATA_FIXED_SIZE (EAC3_MIN_MIXING_METADATA_SIZE - (2 * sizeof(U32))) // same as above minus Id and StructSize

#define BUFFER_EAC3_AUDIO_CODEC_DECODE_CONTEXT  "EAc3AudioCodecDecodeContext"
#define BUFFER_EAC3_AUDIO_CODEC_DECODE_CONTEXT_TYPE     {BUFFER_EAC3_AUDIO_CODEC_DECODE_CONTEXT, BufferDataTypeBase, AllocateFromOSMemory, 32, 0, true, true, sizeof(EAc3AudioCodecDecodeContext_t)}

static BufferDataDescriptor_t EAc3AudioCodecDecodeContextDescriptor = BUFFER_EAC3_AUDIO_CODEC_DECODE_CONTEXT_TYPE;

const enum eDDCompMode DD_DrcType[] =
{
    DD_LINE_OUT, // DRC Disabled
    DD_CUSTOM_MODE_0,
    DD_CUSTOM_MODE_1,
    DD_LINE_OUT,
    DD_RF_MODE
};

///
Codec_MmeAudioEAc3_c::Codec_MmeAudioEAc3_c(void)
    : Codec_MmeAudio_c()
    , DecoderId(ACC_DDPLUS_ID)
    , isFwEac3Capable(false)
{
    Configuration.CodecName                             = "EAC3 audio";
    Configuration.StreamParameterContextCount           = 1;
    Configuration.StreamParameterContextDescriptor      = &EAc3AudioCodecStreamParameterContextDescriptor;
    Configuration.DecodeContextCount                    = 4;
    Configuration.DecodeContextDescriptor               = &EAc3AudioCodecDecodeContextDescriptor;
    Configuration.TranscodedFrameMaxSize                = EAC3_AC3_MAX_FRAME_SIZE;
    Configuration.MaximumSampleCount                    = EAC3_MAX_DECODED_SAMPLE_COUNT;

    AudioDecoderTransformCapabilityMask.DecoderCapabilityFlags = (1 << ACC_AC3);

    AudioParametersEvents.audio_coding_type  = STM_SE_STREAM_ENCODING_AUDIO_AC3;

    // TODO(pht) move FinalizeInit to a factory method
    InitializationStatus = FinalizeInit();
}

CodecStatus_t Codec_MmeAudioEAc3_c::FinalizeInit(void)
{
    CodecStatus_t Status = GloballyVerifyMMECapabilities();
    if (CodecNoError != Status)
    {
        SE_INFO(group_decoder_audio, "EAC3 not found\n");
        return CodecError;
    }

    isFwEac3Capable = (((AudioDecoderTransformCapability.DecoderCapabilityExtFlags[0]) >> (4 * ACC_AC3)) & (1 << ACC_DOLBY_DIGITAL_PLUS));
    SE_INFO(group_decoder_audio, "%s\n", isFwEac3Capable ?
            "Using extended AC3 decoder (DD+ streams will be correctly decoded)" :
            "Using standard AC3 decoder (DD+ streams will be unplayable)");

    return CodecNoError;
}

///
Codec_MmeAudioEAc3_c::~Codec_MmeAudioEAc3_c(void)
{
    Halt();
}

////////////////////////////////////////////////////////////////////////////
///
/// Populate the supplied structure with parameters for EAC3 audio.
///
CodecStatus_t Codec_MmeAudioEAc3_c::FillOutTransformerGlobalParameters(MME_LxAudioDecoderGlobalParams_t *GlobalParams_p)
{
    // DRC parameters for MAIN
    //  1- first try to get DRC parameters at player level (stream or playback)
    //  2- if not available, the uses DRC settings from mixer (which can be modified through ALSA)
    stm_se_drc_t main_drc_params;

    unsigned int AudioApplicationType = Player->PolicyValue(Playback, Stream, PolicyAudioApplicationType);
    if (AudioApplicationType == PolicyValueAudioApplicationMS12)
    {
        main_drc_params.mode  = (stm_se_compression_mode_t)STM_SE_NO_COMPRESSION;
        main_drc_params.cut   = 0;
        main_drc_params.boost = 0;

    }
    else
    {
        if (PlayerNoError != Player->GetControl(Playback, Stream, STM_SE_CTRL_AUDIO_DYNAMIC_RANGE_COMPRESSION, &main_drc_params))
        {
            SE_DEBUG(group_decoder_audio, "No DRC-MAIN parameters available at player level, using mixer parameters\n");
            main_drc_params.mode  = (stm_se_compression_mode_t) DRC.DRC_Type;
            main_drc_params.cut   = DRC.DRC_HDR;
            main_drc_params.boost = DRC.DRC_LDR;
        }
    }

    MME_LxAudioDecoderGlobalParams_t &GlobalParams = *GlobalParams_p;
    GlobalParams.StructSize = sizeof(MME_LxAudioDecoderGlobalParams_t);

    MME_LxDDPConfig_t &Config = *((MME_LxDDPConfig_t *) GlobalParams.DecConfig);
    memset(&Config, 0, sizeof(MME_LxDDPConfig_t));
    Config.DecoderId     = isFwEac3Capable ? ACC_DDPLUS_ID : ACC_AC3_ID;
    Config.StructSize    = sizeof(MME_LxDDPConfig_t);
#if DRV_MULTICOM_DOLBY_DIGITAL_DECODER_VERSION < 0x110401
    Config.Config[DD_CRC_ENABLE] = ACC_MME_TRUE;
    Config.Config[DD_LFE_ENABLE] = ACC_MME_TRUE;
    Config.Config[DD_COMPRESS_MODE] = DD_DrcType[DRC.Type];
    Config.Config[DD_HDR] = (DRC.Enable) ? DRC.HDR : 0xFF;
    Config.Config[DD_LDR] = (DRC.Enable) ? DRC.LDR : 0xFF;
    Config.Config[DD_HIGH_COST_VCR] = ACC_MME_FALSE; // give better quality 2 channel output but uses more CPU
    Config.Config[DD_VCR_REQUESTED] = ACC_MME_FALSE;

    // DDplus specific parameters
    if (isFwEac3Capable)
    {
        uDDPfeatures ddp;
        Config.Config[DDP_FRAMEBASED_ENABLE] = ACC_MME_TRUE;
        // by default we will leave LFE downmix enabled
        bool DisableLfeDMix = false;

        // Dolby require that LFE downmix is disabled in RF mode (certification requirement)
        if (DD_RF_MODE == Config.Config[DD_COMPRESS_MODE])
        {
            DisableLfeDMix = true;
        }

        ddp.ddp_output_settings         = 0; // reset all controls.
        ddp.ddp_features.Upsample       = 2; // never upsample;
        ddp.ddp_features.AC3Endieness   = 1; // LittleEndian
        ddp.ddp_features.DitherAlgo     = 0; // set to 0 for correct transcoding
        ddp.ddp_features.DisableLfeDmix = DisableLfeDMix;
        Config.Config[DDP_OUTPUT_SETTING] = ddp.ddp_output_settings;
    }

#else
    Config.CRC_ENABLE         = ACC_MME_TRUE;
    Config.LFE_ENABLE         = ACC_MME_TRUE;
    Config.MAIN_COMPRESS_MODE = (STM_SE_NO_COMPRESSION != main_drc_params.mode) ? DD_DrcType[main_drc_params.mode] : DD_LINE_OUT ;
    Config.HDR                = (STM_SE_NO_COMPRESSION != main_drc_params.mode) ? main_drc_params.cut : 0;
    Config.LDR                = (STM_SE_NO_COMPRESSION != main_drc_params.mode) ? main_drc_params.boost : 0;
    Config.HIGH_COST_VCR      = ACC_MME_FALSE; // give better quality 2 channel output but uses more CPU
    SE_DEBUG(group_decoder_audio, "DRC params: MAIN[mode=%d - cut=%d - boost=%d]\n",
             Config.MAIN_COMPRESS_MODE, Config.HDR, Config.LDR);

    // DDplus specific parameters
    if (isFwEac3Capable)
    {
        // by default we will leave LFE downmix enabled
        bool DisableLfeDMix = false;

        // Dolby require that LFE downmix is disabled in RF mode (certification requirement)
        if (DD_RF_MODE == Config.MAIN_COMPRESS_MODE)
        {
            DisableLfeDMix = true;
        }

        Config.Upsample       = 2; // never upsample;
        Config.AC3Endieness   = 1; // LittleEndian
        Config.DitherAlgo     = 0; // set to 0 for correct transcoding
        Config.DisableLfeDmix = DisableLfeDMix;
        Config.DisablePgrmScl = 1; //Disable by default
        Config.SelSubstreamID = Player->PolicyValue(Playback, Stream, PolicyAudioSubstreamId);
    }

#endif
    Config.PcmScale    = ACC_UNITY;
    Config.PcmScaleAux = ACC_UNITY; // Initialize scaling coeff (applied in freq domain)for aux output
//

    CodecStatus_t Status = Codec_MmeAudio_c::FillOutTransformerGlobalParameters(GlobalParams_p);
    if (Status != CodecNoError)
    {
        return Status;
    }

    unsigned char *PcmParams_p = ((unsigned char *) &Config) + Config.StructSize;
    MME_LxPcmProcessingGlobalParams_Subset_t &PcmParams =
        *((MME_LxPcmProcessingGlobalParams_Subset_t *) PcmParams_p);
    // downmix must be disabled for EAC3
    MME_DMixGlobalParams_t &DMix = PcmParams.DMix;
    DMix.Apply = ACC_MME_DISABLED;
    return CodecNoError;
}


////////////////////////////////////////////////////////////////////////////
///
/// Populate the AUDIO_DECODER's initialization parameters for EAC3 audio.
///
/// When this method completes Codec_MmeAudio_c::AudioDecoderInitializationParameters
/// will have been filled out with valid values sufficient to initialize an
/// EAC3 audio decoder.
///
CodecStatus_t   Codec_MmeAudioEAc3_c::FillOutTransformerInitializationParameters(void)
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
/// Populate the AUDIO_DECODER's MME_SET_GLOBAL_TRANSFORMER_PARAMS parameters for EAC3 audio.
///
CodecStatus_t   Codec_MmeAudioEAc3_c::FillOutSetStreamParametersCommand(void)
{
//EAc3AudioStreamParameters_t *Parsed = (EAc3AudioStreamParameters_t *)ParsedFrameParameters->StreamParameterStructure;
    EAc3AudioCodecStreamParameterContext_t  *Context = (EAc3AudioCodecStreamParameterContext_t *)StreamParameterContext;

    if (ParsedAudioParameters == NULL)
    {
        SE_ERROR("(%s) - ParsedAudioParameters are NULL\n", Configuration.CodecName);
        return CodecError;
    }

    // if the stream is dd+, then the transcoding is required
    TranscodeEnable = (ParsedAudioParameters->OriginalEncoding == AudioOriginalEncodingDdplus);
    // The below function will disable the Transcoding based on the stream and system profile
    DisableTranscodingBasedOnProfile();
    // Enable the AuxBuffer based on profile
    EnableAuxBufferBasedOnProfile();
    SE_DEBUG(group_decoder_audio, "Transcode_Enable : %d TranscodeNeeded:%d\n", TranscodeEnable, TranscodeNeeded);

    //
    // Examine the parsed stream parameters and determine what type of codec to instanciate
    //
    DecoderId = ACC_DDPLUS_ID;
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
CodecStatus_t   Codec_MmeAudioEAc3_c::FillOutDecodeCommand(void)
{
    EAc3AudioCodecDecodeContext_t   *Context        = (EAc3AudioCodecDecodeContext_t *)DecodeContext;

    memset(&Context->DecodeParameters, 0, sizeof(Context->DecodeParameters));

    bool mute = false;
    Player->GetControl(Playback, Stream, STM_SE_CTRL_PLAY_STREAM_MUTE, &mute);
    Context->DecodeParameters.Cmd = mute ? ACC_CMD_MUTE : ACC_CMD_PLAY;

    memset(&Context->DecodeStatus, 0, sizeof(Context->DecodeStatus));

    // Fill out the actual command
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
CodecStatus_t   Codec_MmeAudioEAc3_c::ValidateDecodeContext(CodecBaseDecodeContext_t *Context)
{
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

    EAc3AudioCodecDecodeContext_t   *DecodeContext = (EAc3AudioCodecDecodeContext_t *) Context;
    MME_LxAudioDecoderFrameStatus_t *Status        = &(DecodeContext->DecodeStatus.DecStatus);
    AudioDecoderStatus           = *Status;
    AudioDecoderStatus.DecStatus =  Status->DecStatus >> EAC3_TRANSCODING_DEC_STATUS_SHIFT;

    if (AudioDecoderStatus.DecStatus != ACC_DDPLUS_OK)
    {
        SE_WARNING("Decode error (muted frame): 0x%x\n", Status->DecStatus);
        // don't report an error to the higher levels (because the frame is muted)
    }

    bool TranscodeOutputBufAvailable = false;
    /* To determine if a transcoded buffer must be handled,
     * let's check if a transcode buffer has been attached to the CodedDataBuffer */
    Buffer_t codedDataBuffer;
    Context->DecodeContextBuffer->ObtainAttachedBufferReference(CodedFrameBufferType, &codedDataBuffer);
    if (codedDataBuffer != NULL)
    {
        Buffer_t TranscodeBuffer;
        codedDataBuffer->ObtainAttachedBufferReference(TranscodedFrameBufferType, &TranscodeBuffer);
        if (TranscodeBuffer != NULL)
        {
            SE_VERBOSE(group_decoder_audio, "Transcode output buffer is available\n");
            TranscodeOutputBufAvailable = true;
        }
    }

    int TranscodedBufferSize = 0;
    if (TranscodeOutputBufAvailable)
    {
        MME_Command_t *Cmd = &DecodeContext->BaseContext.MMECommand;
        int PauseBufferSize = sizeof(short) * 2; //corresponds to the pause duration in the transcoded output when FW is unable to do transcoding
        TranscodedBufferSize = Cmd->DataBuffers_p[EAC3_TRANSCODE_BUFFER_INDEX]->ScatterPages_p[0].BytesUsed;

        if ((Status->NbOutSamples) && (TranscodedBufferSize == 0)) // Firmware is not able to transcode
        {
            short *data_p = (short *)Cmd->DataBuffers_p[EAC3_TRANSCODE_BUFFER_INDEX]->ScatterPages_p[0].Page_p;
            TranscodedBufferSize = PauseBufferSize; //update the size of transcoded bufffer with the pause burst size
            Cmd->DataBuffers_p[EAC3_TRANSCODE_BUFFER_INDEX]->ScatterPages_p[0].BytesUsed = TranscodedBufferSize; //update byte used
            data_p[0] = (Status->NbOutSamples); //update duration for spdif pause burst.
            data_p[1] = 0;
        }

        if ((TranscodedBufferSize == 0) || (TranscodedBufferSize > EAC3_AC3_MAX_FRAME_SIZE))
        {
            SE_ERROR("Erroneous transcoded buffer size: %d\n", TranscodedBufferSize);
        }
        else if (TranscodedBufferSize != PauseBufferSize)
        {
            st_relayfs_write_se(ST_RELAY_TYPE_AUDIO_TRANSCODE, ST_RELAY_SOURCE_SE,
                                (unsigned char *) Cmd->DataBuffers_p[EAC3_TRANSCODE_BUFFER_INDEX]->ScatterPages_p[0].Page_p,
                                TranscodedBufferSize, 0);
        }
    }

    if (AuxOutputEnable)
    {
        MME_Command_t *Cmd = &DecodeContext->BaseContext.MMECommand;
        unsigned int AuxFrameBufferIndex = (TranscodeOutputBufAvailable) ? EAC3_TRANSCODE_BUFFER_INDEX + 1 : EAC3_TRANSCODE_BUFFER_INDEX;
        AuxFrameBufferIndex = (AuxFrameBufferIndex <= Cmd->NumberOutputBuffers) ? AuxFrameBufferIndex : AuxFrameBufferIndex - 1;
        int32_t   AuxBufferSize = Cmd->DataBuffers_p[AuxFrameBufferIndex]->ScatterPages_p[0].BytesUsed;

        st_relayfs_write_se(ST_RELAY_TYPE_DECODED_AUDIO_AUX_BUFFER0 + RelayfsIndex, ST_RELAY_SOURCE_SE,
                            (uint8_t *) Cmd->DataBuffers_p[AuxFrameBufferIndex]->ScatterPages_p[0].Page_p,
                            AuxBufferSize, 0);
    }


    if (Status->NbOutSamples != EAC3_NBSAMPLES_NEEDED)
    {
        ///  TODO: in case of MS1x , signal frames that wouldn't return 1 full converted frame.
        SE_DEBUG(group_decoder_audio, "Unexpected number of output samples (%d)\n", Status->NbOutSamples);
    }

    // SYSFS
    SetAudioCodecDecStatistics();
    //
    // Attach any codec derived metadata to the output buffer (or verify the
    // frame analysis if the frame analyser already filled everything in for
    // us).
    //
    OS_LockMutex(&Lock);
    ParsedAudioParameters_t *AudioParameters = BufferState[DecodeContext->BaseContext.BufferIndex].ParsedAudioParameters;

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
    enum eAccFsCode  SamplingFreqCode = Status->SamplingFreq;

    if (SamplingFreqCode < ACC_FS_reserved)
    {
        AudioParameters->Source.SampleRateHz = StmSeTranslateDiscreteSamplingFrequencyToInteger(SamplingFreqCode);
    }
    else
    {
        AudioParameters->Source.SampleRateHz = 0;
        SE_ERROR("Decode bad sampling freq returned: 0x%x\n", SamplingFreqCode);
    }

    // Fill the parsed parameters with the AC3 stream metadata
    Codec_MmeAudioEAc3_c::FillStreamMetadata(AudioParameters, Status);
    OS_UnLockMutex(&Lock);
    // Validate the extended status (without propagating errors)
    (void) ValidatePcmProcessingExtendedStatus(Context,
                                               (MME_PcmProcessingFrameExtStatus_t *) &DecodeContext->DecodeStatus.PcmStatus);

    if (TranscodeOutputBufAvailable)
    {
        TranscodedBuffers[DecodeContext->TranscodeBufferIndex].Buffer->SetUsedDataSize(TranscodedBufferSize);
    }

    //SYSFS
    SetAudioCodecDecAttributes();
    // Check if a new AudioParameter event occurs
    PlayerEventRecord_t       myEvent;
    stm_se_play_stream_audio_parameters_t   newAudioParametersValues;
    memset(&newAudioParametersValues, 0, sizeof(stm_se_play_stream_audio_parameters_t));

    // no codec specific parameters => do not need to update myEvent and newAudioParametersValues
    CheckAudioParameterEvent(Status,
                             (MME_PcmProcessingFrameExtStatus_t *) &DecodeContext->DecodeStatus.PcmStatus,
                             &newAudioParametersValues,
                             &myEvent);

    return CodecNoError;
}


////////////////////////////////////////////////////////////////////////////
///
/// Extract mixing metadata and stuff it into the audio parameters.
///
/// \todo Can we make this code common between EAC3 and DTSHD handling.
///
void Codec_MmeAudioEAc3_c::HandleMixingMetadata(CodecBaseDecodeContext_t *Context,
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

    if (MixingMetadata->MinStruct.StructSize < sizeof(MixingMetadata->MinStruct))
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

// /////////////////////////////////////////////////////////////////////////
//
//      Function to dump out the set stream
//      parameters from an mme command.
//

CodecStatus_t   Codec_MmeAudioEAc3_c::DumpSetStreamParameters(void    *Parameters)
{
    SE_ERROR("Not implemented\n");
    return CodecNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Function to dump out the decode
//      parameters from an mme command.
//

CodecStatus_t   Codec_MmeAudioEAc3_c::DumpDecodeParameters(void    *Parameters)
{
    SE_ERROR("Not implemented\n");
    return CodecNoError;
}

////////////////////////////////////////////////////////////////////////////
///
///  Set Default FrameBase style TRANSFORM command IOs
///  Populate DecodeContext with 1 Input and 2 output Buffers
///  Populate I/O MME_DataBuffers
void Codec_MmeAudioEAc3_c::PresetIOBuffers(void)
{
    Codec_MmeAudio_c::PresetIOBuffers();

    if (TranscodeEnable & TranscodeNeeded)
    {
        // plumbing
        DecodeContext->MMEBufferList[EAC3_TRANSCODE_BUFFER_INDEX] = &DecodeContext->MMEBuffers[EAC3_TRANSCODE_BUFFER_INDEX];
        memset(&DecodeContext->MMEBuffers[EAC3_TRANSCODE_BUFFER_INDEX], 0, sizeof(MME_DataBuffer_t));
        DecodeContext->MMEBuffers[EAC3_TRANSCODE_BUFFER_INDEX].StructSize           = sizeof(MME_DataBuffer_t);
        DecodeContext->MMEBuffers[EAC3_TRANSCODE_BUFFER_INDEX].NumberOfScatterPages = 1;
        DecodeContext->MMEBuffers[EAC3_TRANSCODE_BUFFER_INDEX].ScatterPages_p       = &DecodeContext->MMEPages[EAC3_TRANSCODE_BUFFER_INDEX];
        memset(&DecodeContext->MMEPages[EAC3_TRANSCODE_BUFFER_INDEX], 0, sizeof(MME_ScatterPage_t));
        DecodeContext->MMEBuffers[EAC3_TRANSCODE_BUFFER_INDEX].Flags     = 0xCD; // this will trigger transcoding in the firmware....(magic number not exported in the firmware header files, sorry)
        DecodeContext->MMEBuffers[EAC3_TRANSCODE_BUFFER_INDEX].TotalSize = TranscodedBuffers[CurrentTranscodeBufferIndex].BufferLength;
        DecodeContext->MMEPages[EAC3_TRANSCODE_BUFFER_INDEX].Page_p      = TranscodedBuffers[CurrentTranscodeBufferIndex].BufferPointer;
        DecodeContext->MMEPages[EAC3_TRANSCODE_BUFFER_INDEX].Size        = TranscodedBuffers[CurrentTranscodeBufferIndex].BufferLength;
    }

    if (AuxOutputEnable)
    {
        unsigned int AuxFrameBufferIndex = (TranscodeEnable & TranscodeNeeded) ? EAC3_TRANSCODE_BUFFER_INDEX + 1 : EAC3_TRANSCODE_BUFFER_INDEX;

        DecodeContext->MMEBufferList[AuxFrameBufferIndex] = &DecodeContext->MMEBuffers[AuxFrameBufferIndex];
        memset(&DecodeContext->MMEBuffers[AuxFrameBufferIndex], 0, sizeof(MME_DataBuffer_t));

        DecodeContext->MMEBuffers[AuxFrameBufferIndex].StructSize           = sizeof(MME_DataBuffer_t);
        DecodeContext->MMEBuffers[AuxFrameBufferIndex].NumberOfScatterPages = 1;
        DecodeContext->MMEBuffers[AuxFrameBufferIndex].ScatterPages_p       = &DecodeContext->MMEPages[AuxFrameBufferIndex];
        memset(&DecodeContext->MMEPages[AuxFrameBufferIndex], 0, sizeof(MME_ScatterPage_t));
        DecodeContext->MMEBuffers[AuxFrameBufferIndex].Flags      = 0xA1; // For the FW to consider this as an Aux buffer
        DecodeContext->MMEBuffers[AuxFrameBufferIndex].TotalSize  = AuxBuffers[CurrentAuxBufferIndex].BufferLength;
        DecodeContext->MMEPages[AuxFrameBufferIndex].Page_p  = AuxBuffers[CurrentAuxBufferIndex].BufferPointer;
        DecodeContext->MMEPages[AuxFrameBufferIndex].Size    = AuxBuffers[CurrentAuxBufferIndex].BufferLength;
    }
}

////////////////////////////////////////////////////////////////////////////
///
///  Set Default FrameBase style TRANSFORM command for AudioDecoder MT
///  with 1 Input Buffer and 2 Output Buffer.

void Codec_MmeAudioEAc3_c::SetCommandIO(void)
{
    if (TranscodeEnable && TranscodeNeeded)
    {
        CodecStatus_t Status = GetTranscodeBuffer();

        if (Status != CodecNoError)
        {
            SE_ERROR("Error while requesting Transcoded buffer: %d. Disabling transcoding\n", Status);
            TranscodeEnable = false;
        }

        ((EAc3AudioCodecDecodeContext_t *)DecodeContext)->TranscodeBufferIndex = CurrentTranscodeBufferIndex;
    }

    if (AuxOutputEnable)
    {
        CodecStatus_t Status = GetAuxBuffer();

        if (Status != CodecNoError)
        {
            SE_ERROR("Error while requesting Auxiliary buffer: %d. Disabling AuxBufferEnable flag..\n", Status);
            AuxOutputEnable = false;
        }
        ((EAc3AudioCodecDecodeContext_t *)DecodeContext)->AuxBufferIndex = CurrentAuxBufferIndex;
    }

    PresetIOBuffers();
    Codec_MmeAudio_c::SetCommandIO();

    if (TranscodeEnable && TranscodeNeeded)
    {
        // FrameBase Transformer :: 1 Input Buffer / 2 Output Buffer sent through same MME_TRANSFORM
        DecodeContext->MMECommand.NumberOutputBuffers = 2;
    }
    if (AuxOutputEnable)
    {
        // FrameBase Transformer :: 1 Input Buffer / 2 Output Buffer sent through same MME_TRANSFORM
        DecodeContext->MMECommand.NumberOutputBuffers += 1;
    }
}


// /////////////////////////////////////////////////////////////////////////
//
//      Function to set the stream metadata according to what is contained
//      in the steam bitstream (returned by the codec firmware status)
//

void Codec_MmeAudioEAc3_c::FillStreamMetadata(ParsedAudioParameters_t *AudioParameters, MME_LxAudioDecoderFrameStatus_t *Status)
{
    // this code is very fatpipe dependent, so maybe not in the right place...
    StreamMetadata_t *Metadata = &AudioParameters->StreamMetadata;
    tMME_BufferFlags *Flags = (tMME_BufferFlags *) &Status->PTSflag;
    // direct mapping
    Metadata->FrontMatrixEncoded = Flags->FrontMatrixEncoded;
    Metadata->RearMatrixEncoded  = Flags->RearMatrixEncoded;
    // according to fatpipe specs
    Metadata->MixLevel  = (Flags->AudioProdie) ? (Flags->MixLevel + 80) : 0;
    // no dialog norm processing is performed within or after the ac3 decoder,
    // so the box will apply the value from the stream metadata
    Metadata->DialogNorm = Flags->DialogNorm;
    Metadata->LfeGain = 10;
}
////////////////////////////////////////////////////////////////////////////
///
///  Public static function to fill AC3/EAC3 codec capabilities
///  to expose it through STM_SE_CTRL_GET_CAPABILITY_AUDIO_DECODE control.
///
void Codec_MmeAudioEAc3_c::GetCapabilities(stm_se_audio_dec_capability_t::audio_dec_ac3_capability_s *cap,
                                           const MME_LxAudioDecoderHDInfo_t decInfo)
{
    const int ExtFlags = Codec_Capabilities_c::ExtractAudioExtendedFlags(decInfo, ACC_AC3);
    cap->common.capable      = (decInfo.DecoderCapabilityFlags     & (1 << ACC_AC3)
                                & SE_AUDIO_DEC_CAPABILITIES        & (1 << ACC_AC3)
                               ) ? true : false;
    cap->DolyDigitalEx       = (ExtFlags & (1 << ACC_DOLBY_DIGITAL_Ex))      ? true : false;
    cap->DolyDigitalPlus     = (ExtFlags & (1 << ACC_DOLBY_DIGITAL_PLUS))    ? true : false;
    cap->DolyDigitalPlus_7_1 = (ExtFlags & (1 << ACC_DOLBY_DIGITAL_PLUS_71)) ? true : false;
}
