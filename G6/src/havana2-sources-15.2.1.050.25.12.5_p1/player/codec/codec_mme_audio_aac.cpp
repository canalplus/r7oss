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
/// \class Codec_MmeAudioAac_c
///
/// The AAC audio codec proxy.
///

#include "codec_mme_audio_aac.h"
#include "codec_capabilities.h"
#include "aac_audio.h"
#include "frame_parser_audio_aac.h"

#undef TRACE_TAG
#define TRACE_TAG "Codec_MmeAudioAac_c"

const enum eAacFormatType HeaacFormatType[AAC_AUDIO_MAX_FORMAT_TYPE + 1] =
{
    AAC_AUTO_TYPE,      //  STM_SE_AAC_LC_TS_PROFILE,  /* Auto detect*/
    AAC_ADTS_FORMAT,    //  STM_SE_AAC_LC_ADTS_PROFILE,/* ADTS force */
    AAC_LOAS_FORMAT,    //  STM_SE_AAC_LC_LOAS_PROFILE,/* LOAS force */
    AAC_RAW_FORMAT,     //  STM_SE_AAC_LC_RAW_PROFILE, /* RAW  force */
    AAC_BSAC_RAW_FORMAT //  STM_SE_AAC_BSAC_PROFILE,   /* BSAC force */
};

const enum eMME_AAC_CompressionMode Heaac_DrcType[] =
{
    MME_AAC_LINE_OUT, // DRC Disabled
    MME_AAC_LINE_OUT, // CUSTOM0 : not supported
    MME_AAC_LINE_OUT, // CUSTOM1 : not supported
    MME_AAC_LINE_OUT, //
    MME_AAC_RF_MODE
};
#define AAC_COMPRESSED_FRAME_BUFFER_SCATTER_PAGES 2 // For MS1x it is required to send 2 scatter pages for the Compressed Frame
/*TODO How to handle "MS11_DUAL_DEC_MODE" : depend upon bug17223 */

#define BUFFER_AAC_AUDIO_CODEC_STREAM_PARAMETER_CONTEXT     "AacAudioCodecStreamParameterContext"
#define BUFFER_AAC_AUDIO_CODEC_STREAM_PARAMETER_CONTEXT_TYPE    {BUFFER_AAC_AUDIO_CODEC_STREAM_PARAMETER_CONTEXT, BufferDataTypeBase, AllocateFromOSMemory, 32, 0, true, true, sizeof(StreamAudioCodecStreamParameterContext_t)}

static BufferDataDescriptor_t        AacAudioCodecStreamParameterContextDescriptor = BUFFER_AAC_AUDIO_CODEC_STREAM_PARAMETER_CONTEXT_TYPE;

// --------
typedef struct
{
    MME_LxAudioDecoderFrameStatus_t              DecStatus;
    MME_PcmProcessingFrameExtCommonStatus_t      PcmStatus;
} MME_LxAudioDecoderFrameExtendedAacStatus_t;

typedef struct AacAudioCodecDecodeContext_s
{
    CodecBaseDecodeContext_t        BaseContext;

    MME_LxAudioDecoderFrameParams_t     DecodeParameters;
    MME_LxAudioDecoderFrameExtendedAacStatus_t     DecodeStatus;
} AacAudioCodecDecodeContext_t;

#define BUFFER_AAC_AUDIO_CODEC_DECODE_CONTEXT   "AacAudioCodecDecodeContext"
#define BUFFER_AAC_AUDIO_CODEC_DECODE_CONTEXT_TYPE  {BUFFER_AAC_AUDIO_CODEC_DECODE_CONTEXT, BufferDataTypeBase, AllocateFromOSMemory, 32, 0, true, true, sizeof(AacAudioCodecDecodeContext_t)}

static BufferDataDescriptor_t        AacAudioCodecDecodeContextDescriptor = BUFFER_AAC_AUDIO_CODEC_DECODE_CONTEXT_TYPE;

///
Codec_MmeAudioAac_c::Codec_MmeAudioAac_c(void)
    : Codec_MmeAudioStream_c()
    , DecoderId(ACC_MP4A_AAC_ID)
    , ApplicationConfig()
    , StreamBased((BaseComponentClass_c::EnableCoprocessorParsing == 0) ? false : true)
    , UpmixMono2Stereo((Codec_c::UpmixMono2Stereo == 0) ? false : true)
    , DPulseIdCnt(0) // Pulse ID Count
{
    Configuration.CodecName                         = "AAC audio";
    Configuration.StreamParameterContextCount       = 1;
    Configuration.StreamParameterContextDescriptor  = &AacAudioCodecStreamParameterContextDescriptor;
    Configuration.DecodeContextCount                = 4;
    Configuration.DecodeContextDescriptor           = &AacAudioCodecDecodeContextDescriptor;
    Configuration.TranscodedFrameMaxSize            = 2560; /* Corresponds to Max AC3 framesize at 48kHz -- 640kbps-- */
    Configuration.CompressedFrameMaxSize            = AAC_ADTS_MAX_FRAME_SIZE(AAC_MAX_NB_CHANNELS, AAC_ADTS_NB_MAX_RAW_DATA_BLOCK);
    Configuration.MaximumSampleCount                = AAC_MAX_NBSAMPLES_PER_FRAME;

    AudioDecoderTransformCapabilityMask.DecoderCapabilityFlags = (1 << ACC_MP4a_AAC);

    ApplicationConfig.aac_profile                   = STM_SE_AAC_LC_TS_PROFILE;
    ApplicationConfig.sbr_enable                    = true;
    ApplicationConfig.sbr_96k_enable                = false;
    ApplicationConfig.ps_enable                     = false;

    SendbufTriggerTransformCount                    = SENDBUF_TRIGGER_TRANSFORM_COUNT;    // AAC can start with 1 SEND_BUFFERS

    AudioParametersEvents.audio_coding_type         = STM_SE_STREAM_ENCODING_AUDIO_AAC;
}

///
Codec_MmeAudioAac_c::~Codec_MmeAudioAac_c(void)
{
    Halt();
}

////////////////////////////////////////////////////////////////////////////
///
/// Update the AAC Config
///
///
void Codec_MmeAudioAac_c::UpdateConfig(unsigned int Update)
{
    if (Update & NEW_AAC_CONFIG)
    {
        PlayerStatus_t      PlayerStatus;
        PlayerStatus = Player->GetControl(Playback,
                                          Stream,
                                          STM_SE_CTRL_AAC_DECODER_CONFIG,
                                          &ApplicationConfig);
        if (PlayerStatus != PlayerNoError)
        {
            SE_ERROR("Failed to get AAC_DECODER_CONFIG control (%08x)\n", PlayerStatus);
        }
        else
        {
            SE_DEBUG(group_decoder_audio, "Applying new AAC config\n");
        }
    }

    // Update non AAC specific AudioConfig
    Codec_MmeAudio_c::UpdateConfig(Update);
}


////////////////////////////////////////////////////////////////////////////
///
/// Return the DPulse type as per the Policy.
///
///
unsigned int Codec_MmeAudioAac_c::GetDPulseMode(void)
{
    unsigned int dpulse = HEAAC_MODE;
    int ApplicationType = Player->PolicyValue(Playback, Stream, PolicyAudioApplicationType);
    PlayerStream_t SecondaryStream = Playback->GetSecondaryStream();

    switch (ApplicationType)
    {
    case PolicyValueAudioApplicationMS10:
        dpulse = MS10_MODE;
        break;

    case PolicyValueAudioApplicationMS11:
    case PolicyValueAudioApplicationMS12:
        if (NULL != SecondaryStream)
        {
            // So we are in MS11/MS12 mode and we have secondary Stream
            dpulse = MS11_DUAL_DEC_MODE;
        }
        else
        {
            // So we are in MS11/MS12 mode and we don't have secondary Stream
            dpulse = MS11_SINGLE_DEC_MODE;
        }
        break;

    default:
        break;
    }

    return dpulse;
}

////////////////////////////////////////////////////////////////////////////
///
/// Populate the supplied structure with parameters for AAC audio.
///
///
CodecStatus_t Codec_MmeAudioAac_c::FillOutTransformerGlobalParameters(MME_LxAudioDecoderGlobalParams_t *GlobalParams_p)
{
    int BroadcastRegion = Player->PolicyValue(Playback, Stream, PolicyRegionType);
    SE_INFO(group_decoder_audio, "Initializing AAC audio decoder\n");
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
            SE_INFO(group_decoder_audio, "No DRC-MAIN parameters available at player level, using mixer parameters\n");
            main_drc_params.mode  = (stm_se_compression_mode_t) DRC.DRC_Type;
            main_drc_params.cut   = DRC.DRC_HDR;
            main_drc_params.boost = DRC.DRC_LDR;
        }
    }

    MME_LxAudioDecoderGlobalParams_t &GlobalParams = *GlobalParams_p;
    GlobalParams.StructSize = sizeof(MME_LxAudioDecoderGlobalParams_t);

    MME_AACConfig_t &Config     = *((MME_AACConfig_t *) GlobalParams.DecConfig);
    Config.DecoderId            = DecoderId;
    Config.StructSize           = sizeof(Config);
    Config.CRC_ENABLE           = ACC_MME_TRUE;                     // 8 bit  ! Activate CRC checking
    Config.DRC_ENABLE           = (STM_SE_NO_COMPRESSION != main_drc_params.mode);
    Config.TRANSCODE_FORMAT     = 0;                                // 2 bit ! Format of Transcoded O/P (0=32bitBE 1=32bitLE 2=16bitBE 3=16bitLE)
    Config.NB_DECODED_CHANS     = 0;                                // 4 bit ! No. of Chan to allocate in FW (0 means max supported by FW)
    Config.SBR_ENABLE           = ApplicationConfig.sbr_enable;     // 1 bit ! Enable SBR as set by application
    Config.SBR_ENABLE96K        = ApplicationConfig.sbr_96k_enable; // 1 bit ! Enable SBR96k as set by application
    Config.PS_ENABLE            = ApplicationConfig.ps_enable;      // 1 bit ! Enable PS Decoding (HE - AACv2) as set by application
    Config.LP_SBR_ENABLE        = ACC_MME_FALSE;                    // 1 bit ! Set Low Power SBR for multichan stream

    if (StreamBased)
    {
        unsigned int dpulse = GetDPulseMode();
        // We will enable transcode only if we are in MS10 Mode or in MS11 Single Decode Mode
        TranscodeEnable = (dpulse == MS10_MODE) || (dpulse == MS11_SINGLE_DEC_MODE);
        // We will disable transcoding if we are secondary too
        DisableTranscodingBasedOnProfile();
        // Enable the AuxBuffer based on MS11 profile
        EnableAuxBufferBasedOnProfile();
        Config.DPULSE_ENABLE        = dpulse;                       // 2 bit ! HE-AAC, DPulse MS10, DPulse MS11 single decode, DPulse MS11 dual decode
        Config.TRANSCODE_ENABLE     = TranscodeEnable
                                      && TranscodeNeeded;              // 1 bit ! Enable Transcoded O/P (DTS encoding default for AAC /DD encoding in Pulse)
        Config.TRANSCODE_FORMAT     = TRANS_FMT_16le;               // 2 bit ! Set Transcoding format for 16 bit LE
        CompressedFrameEnable       = true;
    }
    else
    {
        Config.DPULSE_ENABLE        = HEAAC_MODE;                   // 2 bit ! HE-AAC, DPulse MS10, DPulse MS11 single decode, DPulse MS11 dual decode
        Config.TRANSCODE_ENABLE     = ACC_MME_FALSE;                // 1 bit ! Enable Transcoded O/P (DTS encoding default for AAC /DD encoding in Pulse)
        TranscodeEnable             = false;
    }

    SE_DEBUG(group_decoder_audio, "DPULSE  Enable: %d, Transcode_Enable :%d\n",  Config.DPULSE_ENABLE, TranscodeEnable);
    Config.REPORT_MIXMDATA      = 1;                                // 1 bit ! Mixing metadata reporting enable
    Config.REPORT_STREAMMETADATA = 1;                               // 1 bit ! Decoder reports the input stream metadata in the Status structure

    if (StreamBased)
    {
        Config.AAC_FORMAT_TYPE      = (ApplicationConfig.aac_profile <= AAC_AUDIO_MAX_FORMAT_TYPE)
                                      ? HeaacFormatType[ApplicationConfig.aac_profile]
                                      : AAC_AUTO_TYPE;                  // 3 bit ! AAC encapsulation format (ADTS,LOAS,ADIF, MP4, Raw)
    }
    else
    {
        if ((ParsedFrameParameters != NULL) && (ParsedFrameParameters->FrameParameterStructure != NULL))
        {
            Config.AAC_FORMAT_TYPE  = ((AacAudioFrameParameters_s *)ParsedFrameParameters->FrameParameterStructure)->Type;
        }
        else
        {
            Config.AAC_FORMAT_TYPE  = AAC_ADTS_FORMAT;
        }
    }

    Config.AAC_FORCE_TYPE       = (Config.AAC_FORMAT_TYPE  != AAC_AUTO_TYPE); // 1 bit ! if 'true' Force the Format type else 'auto-detect'

    // 4 bit ! Sampling Frequency if stream is RAW AAC
    switch (RawAudioSamplingFrequency)
    {
    case  8000:     Config.RAW_FORMAT_FS   = AAC_FS8k;        break;

    case 11025:     Config.RAW_FORMAT_FS   = AAC_FS11k;       break;

    case 12000:     Config.RAW_FORMAT_FS   = AAC_FS12k;       break;

    case 16000:     Config.RAW_FORMAT_FS   = AAC_FS16k;       break;

    case 22050:     Config.RAW_FORMAT_FS   = AAC_FS22k;       break;

    case 24000:     Config.RAW_FORMAT_FS   = AAC_FS24k;       break;

    case 32000:     Config.RAW_FORMAT_FS   = AAC_FS32k;       break;

    case 44100:     Config.RAW_FORMAT_FS   = AAC_FS44k;       break;

    case 48000:     Config.RAW_FORMAT_FS   = AAC_FS48k;       break;

    case 64000:     Config.RAW_FORMAT_FS   = AAC_FS64k;       break;

    case 88200:     Config.RAW_FORMAT_FS   = AAC_FS88k;       break;

    case 96000:     Config.RAW_FORMAT_FS   = AAC_FS96k;       break;

    default:
        SE_ERROR("Frequency %d not supported - treating as 48k\n", RawAudioSamplingFrequency);
        Config.RAW_FORMAT_FS               = AAC_FS48k;
        break;
    }

    Config.DRC_CUT_FACTOR       = (STM_SE_NO_COMPRESSION != main_drc_params.mode) ? (main_drc_params.cut >> 1) :
                                  0x0;    // 8 bit ! DRC cut factor on main output (portion of the attenuation factor for high level signal)
    Config.DRC_BOOST_FACTOR     = (STM_SE_NO_COMPRESSION != main_drc_params.mode) ? (main_drc_params.boost >> 1) :
                                  0x0;    // 8 bit ! DRC boost factor on main output (portion of the gain factor for low level signal)
    /* Note : In Dolby Pluse HDR /LDR = 0 for minimum and = 127 for maximum */
    Config.DUAL_MONO2STEREO     = (unsigned int)UpmixMono2Stereo;    // 1 bit ! if set Force fully copy mono data to L,R in HEAAC mode (MPEG conformance certification requirenment)
    Config.REPORT_PCMMETA       = 1;                                 // 1 bit ! Report MS10 PCM Metadata
    Config.COMPRESSION_MODE     = Heaac_DrcType[main_drc_params.mode];           // 1 bit ! Set RF Mode (0 = Line/ 1 = RF ) on main output
    /* Note : In Dolby Pluse only line and rf mode is supported.*/
    Config.ARIB_ENABLE = (BroadcastRegion == PolicyValueRegionARIB) ? 1 :
                         0; // 1 bit ! ARIB type downmix is enabled in the FW when outmode in CMC is LoRo and ARIB_ENABLE is set to 1. When outmode is MODE_ID and ARIB_ENABLE is 1 then FW will export the ARIB dmix table
    /* TODO ARIB will be set as per profile */
    Config.TARGET_LEVEL         = 0;                                 // 5 bit ! RefLevel targeted when RF_MODE is selected.
    /* TODO TARGET Level will be set. */
    Config.ENABLE_PERFDRIVEN_DMIX = 0;                               // 1 bit ! If '1' and Sfreq != 48k , then downmix to stereo in order to minimise the cost of SFC
    Config.RESERVED2            = 0;                                 // 6 bit ! Must be 0
    /* TODO 2 bits of RESERVED2 in now used 1 for COMPRESSION_MODE on AUX and 1 for SBR_BYPASS will be set after API updation*/
    Config.BSAC_CH_CONFIG       = 0;                                 // 4 bit ! Signal Number of channels, 1 for stereo, 0 for mono
    Config.BSAC_SCAL_LAY_DEC    = 0;                                 // 6 bit ! BSAC highest scalability layer to be decoded, default 0
    /* TODO will be set while implementing BSAC */
    /* Note : name to be updated after API updation */
    Config.B_ASSOC_RESTRICTED   = 0;                                 // 1 bit ! In MS11 restrict associated audio to two channels
    /* TODO will be set while implementing  */
    Config.RESERVED3            = 0;                                 // 7 bit ! Must be 0
    SE_DEBUG(group_decoder_audio, "DRC params: enable=%d  MAIN[cut=%d - boost=%d]\n",
             Config.DRC_ENABLE ,
             Config.DRC_CUT_FACTOR, Config.DRC_BOOST_FACTOR);
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
CodecStatus_t   Codec_MmeAudioAac_c::FillOutTransformerInitializationParameters(void)
{
    CodecStatus_t Status;
    MME_LxAudioDecoderInitParams_t &Params = AudioDecoderInitializationParameters;
//
    MMEInitializationParameters.TransformerInitParamsSize = sizeof(Params);
    MMEInitializationParameters.TransformerInitParams_p = &Params;
//
    Status = Codec_MmeAudio_c::FillOutTransformerInitializationParameters();

    if (Status != CodecNoError)
    {
        return Status;
    }

//
    Params.BlockWise.Bits.StreamBase = StreamBased;  // we are playing AAC as a streambase decoding. For this we need to tell FW that run AAC in stream base.
    return FillOutTransformerGlobalParameters(&Params.GlobalParams);
}


////////////////////////////////////////////////////////////////////////////
///
/// Populate the AUDIO_DECODER's MME_SET_GLOBAL_TRANSFORMER_PARAMS parameters for AAC audio.
///
CodecStatus_t   Codec_MmeAudioAac_c::FillOutSetStreamParametersCommand(void)
{
    if (StreamBased)
    {
        return Codec_MmeAudioStream_c::FillOutSetStreamParametersCommand();
    }
    else
    {
        CodecStatus_t Status;
        StreamAudioCodecStreamParameterContext_t    *Context = (StreamAudioCodecStreamParameterContext_t *)StreamParameterContext;
        //AacAudioStreamParameters_t *Parsed = (AacAudioStreamParameters_t *)ParsedFrameParameters->StreamParameterStructure;
        //
        // Examine the parsed stream parameters and determine what type of codec to instanciate
        //
        DecoderId = ACC_MP4A_AAC_ID;
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
        Context->BaseContext.MMECommand.CmdStatus.AdditionalInfoSize    = 0;
        Context->BaseContext.MMECommand.CmdStatus.AdditionalInfo_p      = NULL;
        Context->BaseContext.MMECommand.ParamSize               = sizeof(Context->StreamParameters);
        Context->BaseContext.MMECommand.Param_p             = (MME_GenericParams_t)(&Context->StreamParameters);
//
        return CodecNoError;
    }
}

////////////////////////////////////////////////////////////////////////////
///
/// Populate the AUDIO_DECODER's MME_TRANSFORM parameters for MPEG audio.
///
CodecStatus_t   Codec_MmeAudioAac_c::FillOutDecodeCommand(void)
{
    if (StreamBased)
    {
        return Codec_MmeAudioStream_c::FillOutDecodeCommand();
    }
    else
    {
        AacAudioCodecDecodeContext_t    *Context    = (AacAudioCodecDecodeContext_t *)DecodeContext;
        unsigned int                     PRL        = (unsigned int)Player->PolicyValue(Playback, Stream, PolicyAudioProgramReferenceLevel);

        //Limit prl to valid range if it is outside the supported range
        PRL = (PRL > 31) ? 31 : PRL;

        //AacAudioFrameParameters_t *Parsed     = (AacAudioFrameParameters_t *)ParsedFrameParameters->FrameParameterStructure;
        //
        // Initialize the frame parameters (we don't actually have much to say here)
        //
        memset(&Context->DecodeParameters, 0, sizeof(Context->DecodeParameters));

        bool mute = false;
        Player->GetControl(Playback, Stream, STM_SE_CTRL_PLAY_STREAM_MUTE, &mute);
        Context->DecodeParameters.Cmd = mute ? ACC_CMD_MUTE : ACC_CMD_PLAY;

        Context->DecodeParameters.PtsFlags.Bits.DialogNorm = PRL;

        //
        // Zero the reply structure
        //
        memset(&Context->DecodeStatus, 0, sizeof(Context->DecodeStatus));
        //
        // Fill out the actual command
        //
        Context->BaseContext.MMECommand.CmdStatus.AdditionalInfoSize    = sizeof(Context->DecodeStatus);
        Context->BaseContext.MMECommand.CmdStatus.AdditionalInfo_p      = (MME_GenericParams_t)(&Context->DecodeStatus);
        Context->BaseContext.MMECommand.ParamSize               = sizeof(Context->DecodeParameters);
        Context->BaseContext.MMECommand.Param_p             = (MME_GenericParams_t)(&Context->DecodeParameters);
        return CodecNoError;
    }
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
CodecStatus_t   Codec_MmeAudioAac_c::ValidateDecodeContext(CodecBaseDecodeContext_t *Context)
{
    MME_Command_t                            *MMECommand ;
    MME_CommandStatus_t                      *CmdStatus;
    MME_LxAudioDecoderFrameExtendedStatus_t  *extStatus;
    unsigned int                              sz;
    AssertComponentState(ComponentRunning);

    if (Context == NULL)
    {
        SE_ERROR("(%s) - CodecContext is NULL\n", Configuration.CodecName);
        return CodecError;
    }

    MMECommand = (MME_Command_t *)(&Context->MMECommand);
    CmdStatus  = (MME_CommandStatus_t *)(&MMECommand->CmdStatus);
    extStatus  = (MME_LxAudioDecoderFrameExtendedStatus_t *)(CmdStatus->AdditionalInfo_p);
    sz         = CmdStatus->AdditionalInfoSize;
    FillAACSpecificStatistics(extStatus, sz);

    if (StreamBased)
    {
        StreamAudioCodecTransformContext_t *TransformContext   = (StreamAudioCodecTransformContext_t *)Context;
        MME_LxAudioDecoderFrameStatus_t     &Status            =  TransformContext->DecodeStatus.DecStatus;

        if (ACC_AAC_RESERVED_PROFILE == (Status.DecStatus & ACC_AAC_DECODE_FRAME_ERROR))
        {
            SE_ERROR("Requested AAC profile not supported\n");
        }

        return Codec_MmeAudioStream_c::ValidateDecodeContext(Context);
    }
    else
    {
        AacAudioCodecDecodeContext_t    *DecodeContext;
        MME_LxAudioDecoderFrameStatus_t *Status;
        Buffer_t                 TheCurrentDecodeBuffer;
        ParsedAudioParameters_t *AudioParameters;
        ParsedFrameParameters_t *DecodedFrameParsedFrameParameters;
        SE_DEBUG(group_decoder_audio, "\n");

        if (AudioOutputSurface == NULL)
        {
            SE_ERROR("(%s) - AudioOutputSurface is NULL\n", Configuration.CodecName);
            return CodecError;
        }

        DecodeContext = (AacAudioCodecDecodeContext_t *) Context;
        Status        = &(DecodeContext->DecodeStatus.DecStatus);

        if (Status->DecStatus != ACC_MPEG2_OK)
        {
            SE_WARNING("AAC audio decode error (muted frame): 0x%08X\n", Status->DecStatus);
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

        AudioParameters->Source.BitsPerSample = AudioOutputSurface->BitsPerSample;
        AudioParameters->Source.ChannelCount = AudioOutputSurface->ChannelCount;
        AudioParameters->Organisation = Status->AudioMode;

        Codec_MmeAudioAac_c::FillStreamMetadata(AudioParameters,  Status);

        //
        //  The AAC frame parser assumes SBR is enabled, and doubles the samples and sample frequency.
        //  We check to see if the audio sample value returned from the codec matches this assumed value,
        //  in which case SBR has been deployed and the audio playback time brought forward to
        //  compensate for the delay of 962 samples (20ms).
        //

        if (AudioParameters->SampleCount - Status->NbOutSamples == 0)
        {
            SE_DEBUG(group_decoder_audio, "HE-AAC / DPulse decoded stream detected: compensating for SBR delay\n");
            TheCurrentDecodeBuffer = BufferState[DecodeContext->BaseContext.BufferIndex].Buffer;
            TheCurrentDecodeBuffer->ObtainMetaDataReference(Player->MetaDataParsedFrameParametersReferenceType, (void **)(&DecodedFrameParsedFrameParameters));
            SE_ASSERT(DecodedFrameParsedFrameParameters != NULL);

            AudioParameters->OriginalEncoding = FrameParser_AudioAac_c::GetHeAacOriginalEncodingFromSampleCount(AudioParameters->SampleCount / 2);

            DecodedFrameParsedFrameParameters->NormalizedPlaybackTime -= 20000;
        }

        AudioParameters->SampleCount = Status->NbOutSamples;
        enum eAccFsCode SamplingFreqCode = Status->SamplingFreq;

        if (SamplingFreqCode < ACC_FS_reserved)
        {
            AudioParameters->Source.SampleRateHz = StmSeTranslateDiscreteSamplingFrequencyToInteger(SamplingFreqCode);
        }
        else
        {
            AudioParameters->Source.SampleRateHz = 0;
            SE_ERROR("AAC audio decode bad sampling freq returned: 0x%x\n", (int)SamplingFreqCode);
        }

        OS_UnLockMutex(&Lock);
        // Validate the extended status (without propagating errors)
        (void) ValidatePcmProcessingExtendedStatus(Context,
                                                   (MME_PcmProcessingFrameExtStatus_t *) &DecodeContext->DecodeStatus.PcmStatus);
        //SYSFS
        SetAudioCodecDecAttributes();
        // Check if a new AudioParameter event occurs
        PlayerEventRecord_t       myEvent;
        stm_se_play_stream_audio_parameters_t   newAudioParametersValues;
        memset(&newAudioParametersValues, 0, sizeof(stm_se_play_stream_audio_parameters_t));
        // no codec specific parameters => do not need to update myEvent and newAudioParametersValues
        CheckAudioParameterEvent(Status, (MME_PcmProcessingFrameExtStatus_t *) &DecodeContext->DecodeStatus.PcmStatus, &newAudioParametersValues, &myEvent);
        return CodecNoError;
    }
}

// /////////////////////////////////////////////////////////////////////////
//
//  Function to dump out the set stream
//  parameters from an mme command.
//

CodecStatus_t   Codec_MmeAudioAac_c::DumpSetStreamParameters(void   *Parameters)
{
    SE_ERROR("Not implemented\n");
    return CodecNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//  Function to dump out the decode
//  parameters from an mme command.
//

CodecStatus_t   Codec_MmeAudioAac_c::DumpDecodeParameters(void  *Parameters)
{
    SE_ERROR("Not implemented\n");
    return CodecNoError;
}

///
CodecStatus_t   Codec_MmeAudioAac_c::Halt(void)
{
    if (StreamBased)
    {
        return Codec_MmeAudioStream_c::Halt();
    }
    else
    {
        return Codec_MmeAudio_c::Halt();
    }
}

//{{{  Connect
// /////////////////////////////////////////////////////////////////////////
//
//      Connect output port
//
CodecStatus_t   Codec_MmeAudioAac_c::Connect(Port_c *Port)
{
    if (StreamBased)
    {
        return Codec_MmeAudioStream_c::Connect(Port);
    }
    else
    {
        return Codec_MmeAudio_c::Connect(Port);
    }
}

//}}}
//{{{  Input
// /////////////////////////////////////////////////////////////////////////
//
//      The Input function - receive chunks of data parsed by the frame parser
//
CodecStatus_t   Codec_MmeAudioAac_c::Input(Buffer_t          CodedBuffer)
{
    if (StreamBased)
    {
        return Codec_MmeAudioStream_c::Input(CodedBuffer);
    }
    else
    {
        return Codec_MmeAudio_c::Input(CodedBuffer);
    }
}

//{{{  FillOutDecodeContext
////////////////////////////////////////////////////////////////////////////
///
/// Override the superclass version to suit MME_SEND_BUFFERS.
///
/// Populate the DecodeContext structure with parameters for a single buffer
///
CodecStatus_t Codec_MmeAudioAac_c::FillOutDecodeContext(void)
{
    if (StreamBased)
    {
        return Codec_MmeAudioStream_c::FillOutDecodeContext();
    }
    else
    {
        return Codec_MmeAudio_c::FillOutDecodeContext();
    }
}

//}}}
//{{{  SendMMEDecodeCommand
CodecStatus_t   Codec_MmeAudioAac_c::SendMMEDecodeCommand(void)
{
    if (StreamBased)
    {
        return Codec_MmeAudioStream_c::SendMMEDecodeCommand();
    }
    else
    {
        return Codec_MmeBase_c::SendMMEDecodeCommand();
    }
}
//{{{  FinishedDecode
////////////////////////////////////////////////////////////////////////////
///
/// Clear up - do nothing, as actual decode done elsewhere
///
void Codec_MmeAudioAac_c::FinishedDecode(void)
{
    if (StreamBased)
    {
        return Codec_MmeAudioStream_c::FinishedDecode();
    }
    else
    {
        return Codec_MmeAudio_c::FinishedDecode();
    }
}

//{{{  DiscardQueuedDecodes
// /////////////////////////////////////////////////////////////////////////
//
//      When discarding queued decodes, poke the monitor task
//

CodecStatus_t   Codec_MmeAudioAac_c::DiscardQueuedDecodes(void)
{
    if (StreamBased)
    {
        return Codec_MmeAudioStream_c::DiscardQueuedDecodes();
    }
    else
    {
        return Codec_MmeBase_c::DiscardQueuedDecodes();
    }
}

//{{{  CallbackFromMME
// /////////////////////////////////////////////////////////////////////////
//
//      Callback function from MME
//
//

void   Codec_MmeAudioAac_c::CallbackFromMME(MME_Event_t Event, MME_Command_t *CallbackData)
{
    if (StreamBased)
    {
        return Codec_MmeAudioStream_c::CallbackFromMME(Event, CallbackData);
    }
    else
    {
        return Codec_MmeBase_c::CallbackFromMME(Event, CallbackData);
    }
}

//{{{  OutputPartialDecodeBuffers
////////////////////////////////////////////////////////////////////////////
///
/// We override CodecMmeBase version as Drain() calls this, and for
/// audio_stream we need to protect against TransformThread as well
/// this is currenly achieved using InputMutex
///
CodecStatus_t    Codec_MmeAudioAac_c::OutputPartialDecodeBuffers(void)
{
    if (StreamBased)
    {
        return Codec_MmeAudioStream_c::OutputPartialDecodeBuffers();
    }
    else
    {
        return Codec_MmeBase_c::OutputPartialDecodeBuffers();
    }
}

////////////////////////////////////////////////////////////////////////////
///
///  Set Default StreamBase style TRANSFORM command IOs
///  Populate TransformContext with 0 Input and 2 output Buffers
///  Populate I/O MME_DataBuffers
void Codec_MmeAudioAac_c::PresetIOBuffers(void)
{
    if (StreamBased)
    {
        Codec_MmeAudioStream_c::PresetIOBuffers();

        if (TranscodeEnable && TranscodeNeeded)
        {
            BufferState[CurrentDecodeBufferIndex].ParsedAudioParameters->OriginalEncoding  = AudioOriginalEncodingDPulse;
            // plumbing
            TransformContext->MMEBufferList[STREAM_BASE_TRANSCODE_BUFFER_INDEX] = &TransformContext->MMEBuffers[STREAM_BASE_TRANSCODE_BUFFER_INDEX];
            memset(&TransformContext->MMEBuffers[STREAM_BASE_TRANSCODE_BUFFER_INDEX], 0, sizeof(MME_DataBuffer_t));
            TransformContext->MMEBuffers[STREAM_BASE_TRANSCODE_BUFFER_INDEX].StructSize           = sizeof(MME_DataBuffer_t);
            TransformContext->MMEBuffers[STREAM_BASE_TRANSCODE_BUFFER_INDEX].NumberOfScatterPages = 1;
            TransformContext->MMEBuffers[STREAM_BASE_TRANSCODE_BUFFER_INDEX].ScatterPages_p       = &TransformContext->MMEPages[STREAM_BASE_TRANSCODE_BUFFER_INDEX];
            memset(&TransformContext->MMEPages[STREAM_BASE_TRANSCODE_BUFFER_INDEX], 0, sizeof(MME_ScatterPage_t));
            TransformContext->MMEBuffers[STREAM_BASE_TRANSCODE_BUFFER_INDEX].Flags     = 0xCD; // 0xCD is for Compressed buffer this will trigger transcoding in the firmware....
            TransformContext->MMEBuffers[STREAM_BASE_TRANSCODE_BUFFER_INDEX].TotalSize = TranscodedBuffers[CurrentTranscodeBufferIndex].BufferLength;
            TransformContext->MMEPages[STREAM_BASE_TRANSCODE_BUFFER_INDEX].Page_p      = TranscodedBuffers[CurrentTranscodeBufferIndex].BufferPointer;
            TransformContext->MMEPages[STREAM_BASE_TRANSCODE_BUFFER_INDEX].Size        = TranscodedBuffers[CurrentTranscodeBufferIndex].BufferLength;
        }

        if (CompressedFrameEnable)
        {
            // When the transcoding is enabled along with CompressedFrame then CompressedFrame index will be TranscodeBufferIndex + 1.
            int32_t CompressedFrameScatterPageIndex = (TranscodeEnable && TranscodeNeeded) ? STREAM_BASE_TRANSCODE_BUFFER_INDEX + 1 : STREAM_BASE_TRANSCODE_BUFFER_INDEX;
            // plumbing
            TransformContext->MMEBufferList[CompressedFrameScatterPageIndex] = &TransformContext->MMEBuffers[CompressedFrameScatterPageIndex];
            memset(&TransformContext->MMEBuffers[CompressedFrameScatterPageIndex], 0, sizeof(MME_DataBuffer_t));
            TransformContext->MMEBuffers[CompressedFrameScatterPageIndex].StructSize           = sizeof(MME_DataBuffer_t);
            TransformContext->MMEBuffers[CompressedFrameScatterPageIndex].NumberOfScatterPages = NoOfCompressedFrameBuffers;
            TransformContext->MMEBuffers[CompressedFrameScatterPageIndex].ScatterPages_p       = &TransformContext->MMEPages[CompressedFrameScatterPageIndex];
            TransformContext->MMEBuffers[CompressedFrameScatterPageIndex].Flags                = 0xC3; // 0xC3 is to trigger the FW to output compressed frame for bypass
            TransformContext->MMEBuffers[CompressedFrameScatterPageIndex].TotalSize            = 0;

            for (uint32_t i = 0; i < NoOfCompressedFrameBuffers; i++)
            {
                memset(&TransformContext->MMEPages[CompressedFrameScatterPageIndex + i], 0, sizeof(MME_ScatterPage_t));
                TransformContext->MMEPages[CompressedFrameScatterPageIndex + i].Page_p      = CompressedFrameBuffers[i][CurrentCompressedFrameBufferIndex[i]].BufferPointer;
                TransformContext->MMEPages[CompressedFrameScatterPageIndex + i].Size        = CompressedFrameBuffers[i][CurrentCompressedFrameBufferIndex[i]].BufferLength;
                TransformContext->MMEBuffers[CompressedFrameScatterPageIndex].TotalSize  += TransformContext->MMEPages[CompressedFrameScatterPageIndex + i].Size;
            }
        }

        if (AuxOutputEnable)
        {
            int32_t AuxFrameBufferIndex = (TranscodeEnable && TranscodeNeeded) ? STREAM_BASE_TRANSCODE_BUFFER_INDEX + 1 : STREAM_BASE_TRANSCODE_BUFFER_INDEX;
            AuxFrameBufferIndex = CompressedFrameEnable ? AuxFrameBufferIndex + 1 : AuxFrameBufferIndex;

            // The scatter page index will not be the same as buffer index when compressed o/p is enabled
            int32_t AuxFrameScatterPageIndex = CompressedFrameEnable ? AuxFrameBufferIndex + NoOfCompressedFrameBuffers : AuxFrameBufferIndex;

            TransformContext->MMEBufferList[AuxFrameBufferIndex] = &TransformContext->MMEBuffers[AuxFrameBufferIndex];
            memset(&TransformContext->MMEBuffers[AuxFrameBufferIndex], 0, sizeof(MME_DataBuffer_t));

            TransformContext->MMEBuffers[AuxFrameBufferIndex].StructSize           = sizeof(MME_DataBuffer_t);
            TransformContext->MMEBuffers[AuxFrameBufferIndex].NumberOfScatterPages = 1;
            TransformContext->MMEBuffers[AuxFrameBufferIndex].ScatterPages_p       = &TransformContext->MMEPages[AuxFrameScatterPageIndex];
            memset(&TransformContext->MMEPages[AuxFrameScatterPageIndex], 0, sizeof(MME_ScatterPage_t));
            TransformContext->MMEBuffers[AuxFrameBufferIndex].Flags      = 0xA1; // For the FW to consider this as an Aux buffer
            TransformContext->MMEBuffers[AuxFrameBufferIndex].TotalSize  = AuxBuffers[CurrentAuxBufferIndex].BufferLength;
            TransformContext->MMEPages[AuxFrameScatterPageIndex].Page_p  = AuxBuffers[CurrentAuxBufferIndex].BufferPointer;
            TransformContext->MMEPages[AuxFrameScatterPageIndex].Size    = AuxBuffers[CurrentAuxBufferIndex].BufferLength;
        }
    }
}

////////////////////////////////////////////////////////////////////////////
///
///  Set Default StreamBase style TRANSFORM command for AudioDecoder MT
///  with 0 Input Buffer and 2 Output Buffer.

void Codec_MmeAudioAac_c::SetCommandIO(void)
{
    if (StreamBased)
    {
        if (TranscodeEnable && TranscodeNeeded)
        {
            CodecStatus_t Status = GetTranscodeBuffer();

            if (Status != CodecNoError)
            {
                SE_ERROR("Error while requesting Transcoded buffer: %d. Disabling transcoding\n", Status);
                TranscodeEnable = false;
            }

            ((StreamAudioCodecDecodeContext_t *)TransformContext)->TranscodeBufferIndex = CurrentTranscodeBufferIndex;
        }

        if (CompressedFrameEnable)
        {
            CodecStatus_t Status = GetCompressedFrameBuffer(AAC_COMPRESSED_FRAME_BUFFER_SCATTER_PAGES);

            if (Status != CodecNoError)
            {
                SE_ERROR("Error while requesting CompressedFrame buffer: %d. Problem in the bypass of AAC over SPDIF/HDMI\n", Status);
                CompressedFrameEnable = false;
            }
            else
            {
                for (uint32_t i = 0; i < NoOfCompressedFrameBuffers; i++)
                {
                    ((StreamAudioCodecDecodeContext_t *)TransformContext)->CompressedFrameBufferIndex[i] = CurrentCompressedFrameBufferIndex[i];
                }
            }
        }

        if (AuxOutputEnable)
        {
            CodecStatus_t Status = GetAuxBuffer();

            if (Status != CodecNoError)
            {
                SE_ERROR("Error while requesting Auxiliary buffer: %d. Disabling AuxBufferEnable flag..\n", Status);
                AuxOutputEnable = false;
            }
            ((StreamAudioCodecDecodeContext_t *)TransformContext)->AuxBufferIndex = CurrentAuxBufferIndex;
        }

        PresetIOBuffers();
        Codec_MmeAudioStream_c::SetCommandIO();

        if (TranscodeEnable && TranscodeNeeded)
        {
            // StreamBase Transformer :: 0 Input Buffer / N+1 Output Buffer sent through same MME_TRANSFORM
            TransformContext->MMECommand.NumberOutputBuffers += 1;
        }

        if (CompressedFrameEnable)
        {
            // StreamBase Transformer :: 0 Input Buffer / N+1 Output Buffer sent through same MME_TRANSFORM
            TransformContext->MMECommand.NumberOutputBuffers += 1;
        }

        if (AuxOutputEnable)
        {
            // StreamBase Transformer :: 0 Input Buffer / N+1 Output Buffer sent through same MME_TRANSFORM
            TransformContext->MMECommand.NumberOutputBuffers += 1;
        }

    }
    else
    {
        Codec_MmeAudio_c::SetCommandIO();
    }
}

CodecStatus_t Codec_MmeAudioAac_c::FillAACSpecificStatistics(MME_LxAudioDecoderFrameExtendedStatus_t    *extDecStatus, unsigned int size)
{
    MME_PcmProcessingFrameExtStatus_t    *pcmStatus = (MME_PcmProcessingFrameExtStatus_t *)(&extDecStatus->PcmStatus);
    MME_PcmProcessingStatusTemplate_t    *SpecificStatus;
    int          id;
    unsigned int BytesLeft;
    BytesLeft = size - (extDecStatus->DecStatus.StructSize + sizeof(pcmStatus->BytesUsed));
    BytesLeft = (pcmStatus->BytesUsed > BytesLeft) ? BytesLeft : pcmStatus->BytesUsed;
    SpecificStatus = (MME_PcmProcessingStatusTemplate_t *)(&pcmStatus->PcmStatus);

    while (BytesLeft > sizeof(MME_PcmProcessingStatusTemplate_t))
    {
        if (SpecificStatus->StructSize < 8 ||
            SpecificStatus->StructSize > (unsigned) BytesLeft)
        {
            SE_ERROR("PCM extended status is too %s - Id %x  StructSize %d",
                     (SpecificStatus->StructSize < 8 ? "small" : "large"),
                     SpecificStatus->Id, SpecificStatus->StructSize);
            return CodecError;
        }

        id = ACC_PCMPROC_ID(SpecificStatus->Id);

        switch (id)
        {
        case ACC_MP4A_AAC_ID :
        {
            tADFrameStatus_t          *FrameStatus = (tADFrameStatus_t *)SpecificStatus;
            MME_AACExtStatus_t     *AACextStatus = (MME_AACExtStatus_t *)(&(FrameStatus->ExtDecStatus));

            if (AACextStatus->DolbyPulseId)
            {
                DPulseIdCnt += 1;
            };

            Stream->Statistics().DolbyPulseIDCount      = DPulseIdCnt;

            Stream->Statistics().DolbyPulseSBRPresent   = AACextStatus->SBRPresent;

            Stream->Statistics().DolbyPulsePSPresent    = AACextStatus->PSPresent;

            Stream->Statistics().CodecFrameLength       = AACextStatus->AACFrameLenFlag == 1 ? 960 : 1024;

            Stream->Statistics().CodecNumberOfOutputChannels  = DecodedNumberOfChannels(&FrameStatus->ChannelMap);
        }
        break;

        default:
            SE_DEBUG(group_decoder_audio, "SpecificCodecStatus not found , skip Status-ID[%d] over %d bytes\n", id, SpecificStatus->StructSize);
            break;
        }

        BytesLeft -= SpecificStatus->StructSize;
        SpecificStatus = (MME_PcmProcessingStatusTemplate_t *)
                         (((char *) SpecificStatus) + SpecificStatus->StructSize);
    }

    return CodecNoError;
}

unsigned int  Codec_MmeAudioAac_c::DecodedNumberOfChannels(MME_ChannelMapInfo_u  *ChannelMap)
{
    unsigned int nbchans = 0;
    nbchans = (ChannelMap->bits.LeftRightPair) ? (nbchans + 2) : nbchans;
    nbchans = (ChannelMap->bits.Center) ? (nbchans + 1) : nbchans;
    nbchans = (ChannelMap->bits.Lfe) ? (nbchans + 1) : nbchans;
    nbchans = (ChannelMap->bits.SurroundPair) ? (nbchans + 2) : nbchans;
    nbchans = (ChannelMap->bits.CenterSurround) ? (nbchans + 1) : nbchans;
    nbchans = (ChannelMap->bits.FrontHighPair) ? (nbchans + 2) : nbchans;
    nbchans = (ChannelMap->bits.RearSurroundPair) ? (nbchans + 2) : nbchans;
    nbchans = (ChannelMap->bits.HighCenter) ? (nbchans + 1) : nbchans;
    nbchans = (ChannelMap->bits.TopSurround) ? (nbchans + 1) : nbchans;
    nbchans = (ChannelMap->bits.CenterLRPair) ? (nbchans + 2) : nbchans;
    nbchans = (ChannelMap->bits.FrontWidePair) ? (nbchans + 2) : nbchans;
    nbchans = (ChannelMap->bits.SideSurroundPair) ? (nbchans + 2) : nbchans;
    nbchans = (ChannelMap->bits.Lfe2) ? (nbchans + 1) : nbchans;
    nbchans = (ChannelMap->bits.HighSurroundPair) ? (nbchans + 2) : nbchans;
    nbchans = (ChannelMap->bits.CenterHighRear) ? (nbchans + 1) : nbchans;
    nbchans = (ChannelMap->bits.HighRearPair) ? (nbchans + 2) : nbchans;
    nbchans = (ChannelMap->bits.CenterLowFront) ? (nbchans + 1) : nbchans;
    nbchans = (ChannelMap->bits.FrontLowPair) ? (nbchans + 2) : nbchans;
    nbchans = (ChannelMap->bits.LtRtPair) ? (nbchans + 2) : nbchans;
    nbchans = (ChannelMap->bits.SideDirectsPair) ? (nbchans + 2) : nbchans;
    nbchans = (ChannelMap->bits.LsExRsExPair) ? (nbchans + 2) : nbchans;
    nbchans = (ChannelMap->bits.LbinRbinPair) ? (nbchans + 2) : nbchans;
    return nbchans;
}
////////////////////////////////////////////////////////////////////////////
///
///  Public static function to fill AAC codec capabilities
///  to expose it through STM_SE_CTRL_GET_CAPABILITY_AUDIO_DECODE control.
///
void Codec_MmeAudioAac_c::GetCapabilities(stm_se_audio_dec_capability_t::audio_dec_aac_capability_s *cap,
                                          const MME_LxAudioDecoderHDInfo_t decInfo)
{
    const int ExtFlags = Codec_Capabilities_c::ExtractAudioExtendedFlags(decInfo, ACC_MP4a_AAC);
    cap->common.capable = (decInfo.DecoderCapabilityFlags     & (1 << ACC_MP4a_AAC)
                           & SE_AUDIO_DEC_CAPABILITIES        & (1 << ACC_MP4a_AAC)
                          ) ? true : false;
    cap->aac_BSAC       = (ExtFlags & (1 << ACC_AAC_BSAC))    ? true : false;
    cap->aac_DolbyPulse = (ExtFlags & (1 << ACC_DOLBY_PULSE)) ? true : false;
    cap->aac_SBR        = (ExtFlags & (1 << ACC_AAC_SBR))     ? true : false;
    cap->aac_PS         = (ExtFlags & (1 << ACC_AAC_PS))      ? true : false;
}

void Codec_MmeAudioAac_c::FillStreamMetadata(ParsedAudioParameters_t *AudioParameters, MME_LxAudioDecoderFrameStatus_t *Status)
{
    StreamMetadata_t *Metadata = &AudioParameters->StreamMetadata;
    tMME_BufferFlags *Flags = (tMME_BufferFlags *) &Status->PTSflag;

    // direct mapping
    Metadata->FrontMatrixEncoded = Flags->FrontMatrixEncoded;
    Metadata->RearMatrixEncoded  = Flags->RearMatrixEncoded;
    Metadata->DialogNorm = Flags->DialogNorm;
}
