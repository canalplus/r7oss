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
/// \class Codec_MmeAudioSpdifIn_c
///
/// The SpdifIn audio codec proxy.
///

#include "codec_mme_audio_spdifin.h"
#include "codec_mme_audio_eac3.h"
#include "codec_mme_audio_dtshd.h"
#include "lpcm.h"
#include "codec_capabilities.h"
#include "spdifin_audio.h"

#undef TRACE_TAG
#define TRACE_TAG "Codec_MmeAudioSpdifin_c"

/* SE_MIN_SYSTEM_LATENCY_GRAINS defines the minimum latency in grains of the
 * streaming engine from HDMI input to HDMI output:
 * that is 1 grain  for HDMI input
 * and     2 grains for HDMI output
 * considering that to achieve minimum latency HDMI in and HDMI out grains
 * shall be equal
*/
#define SE_MIN_SYSTEM_LATENCY_GRAINS  ( 1 + 2 )

/* LOOKAHEAD_MAX_BUFFERSIZE_SPDIFIN(in samples) defines the initial buffering to be
 * done in the firmware before start of decode.The current value is set to 2048
 * as this is the least buffering required for majority of the codecs supported
 * by SPDIFIn. This value needs to be tweaked once this feature is fully
 * supported in the fw.
 * */
#define LOOKAHEAD_MAX_BUFFERSIZE_SPDIFIN 2048

typedef struct
{
    int             kHz;
    enum eAccFsCode Code;
} SfreqConversion_t;

#define BUFFER_SPDIFIN_AUDIO_CODEC_STREAM_PARAMETER_CONTEXT         "SpdifinAudioCodecStreamParameterContext"
#define BUFFER_SPDIFIN_AUDIO_CODEC_STREAM_PARAMETER_CONTEXT_TYPE    {BUFFER_SPDIFIN_AUDIO_CODEC_STREAM_PARAMETER_CONTEXT, BufferDataTypeBase, AllocateFromOSMemory, 32, 0, true, true, sizeof(StreamAudioCodecStreamParameterContext_t)}

static BufferDataDescriptor_t SpdifinAudioCodecStreamParameterContextDescriptor   = BUFFER_SPDIFIN_AUDIO_CODEC_STREAM_PARAMETER_CONTEXT_TYPE;

#define BUFFER_SPDIFIN_AUDIO_CODEC_DECODE_CONTEXT                   "SpdifinAudioCodecDecodeContext"
#define BUFFER_SPDIFIN_AUDIO_CODEC_DECODE_CONTEXT_TYPE              {BUFFER_SPDIFIN_AUDIO_CODEC_DECODE_CONTEXT, BufferDataTypeBase, AllocateFromOSMemory, 32, 0, true, true, sizeof(StreamAudioCodecDecodeContext_t)}

static BufferDataDescriptor_t SpdifinAudioCodecDecodeContextDescriptor = BUFFER_SPDIFIN_AUDIO_CODEC_DECODE_CONTEXT_TYPE;


/// \todo Correctly setup AudioDecoderTransformCapabilityMask
///

Codec_MmeAudioSpdifin_c::Codec_MmeAudioSpdifin_c(void)
    : Codec_MmeAudioStream_c()
    , NbSamplesPerTransform(0)
    , HdmiRxMode(PolicyValueHdmiRxModeDisabled)
    , SpdifStatus()
    , DecodeErrors(0)
    , NumberOfSamplesProcessed(0)
{
    Configuration.CodecName                        = "SPDIFIN audio";
    // for SPDIFin we know that the incoming data is never longer than 1024 samples giving us a fairly
    // small maximum frame size (reducing the maximum frame size allows us to make more efficient use of
    /// the coded frame buffer)
    Configuration.StreamParameterContextCount      = 10;
    Configuration.StreamParameterContextDescriptor = &SpdifinAudioCodecStreamParameterContextDescriptor;
    // Send up to 10 frames for look-ahead
    Configuration.DecodeContextCount               = 10;
    Configuration.DecodeContextDescriptor          = &SpdifinAudioCodecDecodeContextDescriptor;
    Configuration.TranscodedFrameMaxSize           = SPDIFIN_MAX_TRANSCODED_FRAME_SIZE;
    Configuration.CompressedFrameMaxSize           = SPDIFIN_MAX_COMPRESSED_FRAME_SIZE;

    AudioDecoderTransformCapabilityMask.DecoderCapabilityFlags = (1 << ACC_SPDIFIN);

    DecoderId                         = ACC_SPDIFIN_ID;

    Configuration.MaximumSampleCount  = SPDIFIN_MAXIMUM_SAMPLE_COUNT;
    SendbufTriggerTransformCount      = 1;    // Spdifin can start with 1 SEND_BUFFERS

    SpdifStatus.State                 = SPDIFIN_STATE_PCM_BYPASS;
    SpdifStatus.StreamType            = SPDIF_RESERVED;
    SpdifStatus.PlayedSamples         = 0;
}

///
Codec_MmeAudioSpdifin_c::~Codec_MmeAudioSpdifin_c(void)
{
    Halt();
}

const static SfreqConversion_t
LpcmSpdifin2ACC[] =
{

    // DVD Video Supported Frequencies
    {  48 , ACC_FS48k},
    {  96 , ACC_FS96k},
    { 192 , ACC_FS192k},
    {   0 , ACC_FS_reserved},
    {  32 , ACC_FS32k},
    {  16 , ACC_FS16k},
    {  22 , ACC_FS22k},
    {  24 , ACC_FS24k},
    // DVD Audio Supported Frequencies
    {  44 , ACC_FS44k},
    {  88 , ACC_FS88k},
    { 176 , ACC_FS176k},
    {   0 , ACC_FS_reserved},
    {   0 , ACC_FS_reserved},
    {   0 , ACC_FS_reserved},
    {   0 , ACC_FS_reserved},
    {   0 , ACC_FS_reserved},

    // SPDIFIN Supported frequencies
    {   0 , ACC_FS_reserved},
    {   0 , ACC_FS_reserved},
    {   0 , ACC_FS_reserved},
    {   0 , ACC_FS_reserved},
    {   0 , ACC_FS_reserved},
    {   0 , ACC_FS_reserved},
    {   0 , ACC_FS_reserved},
    {   0 , ACC_FS_reserved},
    {   0 , ACC_FS_reserved},
    {   0 , ACC_FS_reserved}
};

const int FreqToRange[] =
{
    0, 1 , 2, 3, -2, -1, 4, -3
};

static const LpcmAudioStreamParameters_t  DefaultStreamParameters =
{
    TypeLpcmSPDIFIN,
    ACC_MME_FALSE,          // MuteFlag
    ACC_MME_FALSE,          // EmphasisFlag
    LpcmWordSize32,
    LpcmWordSizeNone,
    LpcmSamplingFreq48,
    LpcmSamplingFreqNone,
    2,                      // NbChannels
    0,
    LPCM_DEFAULT_CHANNEL_ASSIGNMENT, // derived from NbChannels.

};

////////////////////////////////////////////////////////////////////////////
///
/// Populate the supplied structure with parameters for SPDIFIN audio.
///
CodecStatus_t Codec_MmeAudioSpdifin_c::FillOutTransformerGlobalParameters(MME_LxAudioDecoderGlobalParams_t *GlobalParams_p)
{
    LpcmAudioStreamParameters_t       *Parsed;
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

    MME_SpdifinConfig_t  &Config       = *((MME_SpdifinConfig_t *) GlobalParams.DecConfig);
    Config.DecoderId                   = ACC_SPDIFIN_ID;
    Config.StructSize                  = sizeof(MME_SpdifinConfig_t);
    // Setup default IEC config
    Config.Config[IEC_SFREQ]           = LpcmSpdifin2ACC[Parsed->SamplingFrequency1].Code; // should be 48 by default.
    Config.Config[IEC_NBSAMPLES]       = Parsed->NumberOfSamples;
    NbSamplesPerTransform              = Parsed->NumberOfSamples;

    MME_SpdifInFlags_u   SpdifInFlag;
    SpdifInFlag.U32                    = 0;
    SpdifInFlag.Bits.Emphasis          = Parsed->EmphasisFlag;
    SpdifInFlag.Bits.EnableAAC         = 1;
    SpdifInFlag.Bits.ForceLookAhead    = 1;
    SpdifInFlag.Bits.UserDecCfg        = 0;
    SpdifInFlag.Bits.HDMIAudioMode     = 0;
    if (Parsed->SpdifInProperties.SpdifInLayout == STM_SE_AUDIO_STREAM_TYPE_IEC)
    {
        if (Parsed->SpdifInProperties.ChannelCount > 2)
        {
            SpdifInFlag.Bits.Layout  = HDMI_LAYOUT1;
            SpdifInFlag.Bits.HDMIAudioMode     = Parsed->SpdifInProperties.Organisation;
        }
        else
        {
            SpdifInFlag.Bits.Layout  = HDMI_LAYOUT0;
        }
    }
    else if (Parsed->SpdifInProperties.SpdifInLayout == STM_SE_AUDIO_STREAM_TYPE_HBR)
    {
        SpdifInFlag.Bits.Layout  = STM_SE_AUDIO_STREAM_TYPE_HBR;
    }
    else
    {
        SE_WARNING("Unknown or unsupported Layout: %d  Using layout0 \n", Parsed->SpdifInProperties.SpdifInLayout);
        SpdifInFlag.Bits.Layout  = HDMI_LAYOUT0;
    }
    SpdifInFlag.Bits.ReportCodecStatus = 0;
    SpdifInFlag.Bits.DisableDetection  = 0;
    SpdifInFlag.Bits.DisableFade       = 0;
    CompressedFrameEnable       = true;
    if (HdmiRxMode == PolicyValueHdmiRxModeRepeater)
    {
        SpdifInFlag.Bits.EnableRepeaterMode = 1;
        /* Setup lookahead to the 1 input grain for repeater mode.
           Because repeater mode expects minimum latency so the maximum lookahead we can do to achieve minimum latency is 1 grain.*/
        Config.Config[IEC_LOOKAHEAD] = (FreqToRange[LpcmSpdifin2ACC[Parsed->SamplingFrequency1].Code >> 2] == ACC_FSRANGE_192k) ? NbSamplesPerTransform / 4 : NbSamplesPerTransform;
        SE_INFO(group_decoder_audio, "SPDIFIn @ %d kHz Set NSpl %d LookAhead repeater mode %d\n",
                LpcmSpdifin2ACC[Parsed->SamplingFrequency1].kHz,
                NbSamplesPerTransform, Config.Config[IEC_LOOKAHEAD]);
    }
    else
    {
        TranscodeEnable             = true; // Transcoding possible only in non repeater mode
        // Setup lookahead for non repeater mode
        Config.Config[IEC_LOOKAHEAD] = LOOKAHEAD_MAX_BUFFERSIZE_SPDIFIN;
        SE_INFO(group_decoder_audio, "SPDIFIn @ %d kHz Set NSpl %d LookAhead Non repeater mode %d\n",
                LpcmSpdifin2ACC[Parsed->SamplingFrequency1].kHz,
                NbSamplesPerTransform, Config.Config[IEC_LOOKAHEAD]);
    }
    Config.Config[IEC_FLAGS]           = SpdifInFlag.U32;

    // Setup default DD+ decoder config
    memset(&Config.DecConfig[0], 0, sizeof(Config.DecConfig));
    Config.DecConfig[DD_CRC_ENABLE]    = ACC_MME_TRUE;
    Config.DecConfig[DD_LFE_ENABLE]    = ACC_MME_TRUE;
    Config.DecConfig[DD_COMPRESS_MODE] = DD_LINE_OUT;
    Config.DecConfig[DD_HDR]           = 0xFF;
    Config.DecConfig[DD_LDR]           = 0xFF;

    CodecStatus_t Status = Codec_MmeAudio_c::FillOutTransformerGlobalParameters(GlobalParams_p);
    if (Status != CodecNoError)
    {
        return Status;
    }

    unsigned char *PcmParams_p = ((unsigned char *) &Config) + Config.StructSize;
    MME_LxPcmProcessingGlobalParams_Subset_t &PcmParams =
        *((MME_LxPcmProcessingGlobalParams_Subset_t *) PcmParams_p);
    MME_Resamplex2GlobalParams_t &resamplex2 = PcmParams.Resamplex2;
    // Id already set
    // StructSize already set
    resamplex2.Apply = ACC_MME_AUTO;
    resamplex2.Range = ACC_FSRANGE_48k;
#if PCMPROCESSINGS_API_VERSION >= 0x101122
    resamplex2.SfcEnable       = ACC_MME_FALSE;
    resamplex2.OutFs           = 0;//for 48k
    resamplex2.SfcFilterSelect = 0;
#endif

    return CodecNoError;
}


////////////////////////////////////////////////////////////////////////////
///
/// Populate the AUDIO_DECODER's initialization parameters for SPDIFIN audio.
///
/// When this method completes Codec_MmeAudioStream_c::FillOutTransformerInitializationParameters
/// will have been filled out with valid values sufficient to initialize an
/// SPDIFIN audio decoder.
///
CodecStatus_t   Codec_MmeAudioSpdifin_c::FillOutTransformerInitializationParameters(void)
{
    // Set priority of the decoder according to the application of the playback
    // Note : the application type has to be applied to the playback because this
    // is the only way to know it at the time of creation of the transformer.
    HdmiRxMode = Player->PolicyValue(Playback, Stream, PolicyHdmiRxMode);

    if (HdmiRxMode != PolicyValueHdmiRxModeDisabled)
    {
        MMEInitializationParameters.Priority = MME_PRIORITY_HIGHEST;
    }
    else
    {
        MMEInitializationParameters.Priority = MME_PRIORITY_NORMAL;
    }
    return Codec_MmeAudioStream_c::FillOutTransformerInitializationParameters();
}


////////////////////////////////////////////////////////////////////////////
///
///  Set Default StreamBase style TRANSFORM command IOs
///  Populate TransformContext with 0 Input and 1 + 1 if(TranscodeEnable and TranscodeNeeded) + 1 if (CompressedFrameEnable and CompressedFrameNeeded) output Buffers
///  Populate I/O MME_DataBuffers
void Codec_MmeAudioSpdifin_c::PresetIOBuffers(void)
{
    Codec_MmeAudioStream_c::PresetIOBuffers();

    if (TranscodeEnable && TranscodeNeeded)
    {
        MME_DataBuffer_t *TranscodeBuffer = &TransformContext->MMEBuffers[STREAM_BASE_TRANSCODE_BUFFER_INDEX];
        TransformContext->MMEBufferList[STREAM_BASE_TRANSCODE_BUFFER_INDEX] = TranscodeBuffer;
        memset(TranscodeBuffer, 0, sizeof(MME_DataBuffer_t));
        TranscodeBuffer->StructSize           = sizeof(MME_DataBuffer_t);
        TranscodeBuffer->NumberOfScatterPages = 1;
        TranscodeBuffer->ScatterPages_p       = &TransformContext->MMEPages[STREAM_BASE_TRANSCODE_BUFFER_INDEX];
        memset(&TransformContext->MMEPages[STREAM_BASE_TRANSCODE_BUFFER_INDEX], 0, sizeof(MME_ScatterPage_t));
        TranscodeBuffer->Flags               = BUFFER_TYPE_COMPRESSED_DATA; // BUFFER_TYPE_COMPRESSED_DATA is to trigger transcoding in the firmware
        TranscodeBuffer->TotalSize           = TranscodedBuffers[CurrentTranscodeBufferIndex].BufferLength;
        TransformContext->MMEPages[STREAM_BASE_TRANSCODE_BUFFER_INDEX].Page_p      = TranscodedBuffers[CurrentTranscodeBufferIndex].BufferPointer;
        TransformContext->MMEPages[STREAM_BASE_TRANSCODE_BUFFER_INDEX].Size        = TranscodedBuffers[CurrentTranscodeBufferIndex].BufferLength;
    }
    if (CompressedFrameEnable && CompressedFrameNeeded)
    {
        // When the transcoding is enabled along with CompressedFrame then CompressedFrame index will be TranscodeBufferIndex + 1.
        int32_t CompressedFrameScatterPageIndex = (TranscodeEnable && TranscodeNeeded) ? STREAM_BASE_TRANSCODE_BUFFER_INDEX + 1 : STREAM_BASE_TRANSCODE_BUFFER_INDEX;
        MME_DataBuffer_t *CompressedBuffer = &TransformContext->MMEBuffers[CompressedFrameScatterPageIndex];
        TransformContext->MMEBufferList[CompressedFrameScatterPageIndex] = CompressedBuffer;
        memset(CompressedBuffer, 0, sizeof(MME_DataBuffer_t));
        CompressedBuffer->StructSize           = sizeof(MME_DataBuffer_t);
        CompressedBuffer->NumberOfScatterPages = NoOfCompressedFrameBuffers;
        CompressedBuffer->ScatterPages_p       = &TransformContext->MMEPages[CompressedFrameScatterPageIndex];
        CompressedBuffer->Flags                = BUFFER_TYPE_CODED_DATA_IO + ACC_MIX_HDMIOUT; // BUFFER_TYPE_CODED_DATA_IO + ACC_MIX_HDMIOUT is to trigger the FW to output compressed frame for HDMI bypass
        CompressedBuffer->TotalSize            = 0;
        for (uint32_t i = 0; i < NoOfCompressedFrameBuffers; i++)
        {
            memset(&TransformContext->MMEPages[CompressedFrameScatterPageIndex + i], 0, sizeof(MME_ScatterPage_t));
            TransformContext->MMEPages[CompressedFrameScatterPageIndex + i].Page_p      = CompressedFrameBuffers[i][CurrentCompressedFrameBufferIndex[i]].BufferPointer;
            TransformContext->MMEPages[CompressedFrameScatterPageIndex + i].Size        = CompressedFrameBuffers[i][CurrentCompressedFrameBufferIndex[i]].BufferLength;
            CompressedBuffer->TotalSize  += TransformContext->MMEPages[CompressedFrameScatterPageIndex + i].Size;
        }
    }
}

////////////////////////////////////////////////////////////////////////////
///
///  Set Default StreamBase style TRANSFORM command for AudioDecoder MT
///  with 0 Input Buffer and 1 + 1 if(TranscodeEnable and TranscodeNeeded) + 1 if (CompressedFrameEnable and CompressedFrameNeeded) output Buffers.

void Codec_MmeAudioSpdifin_c::SetCommandIO(void)
{
    if (TranscodeEnable && TranscodeNeeded)
    {
        CodecStatus_t Status = GetTranscodeBuffer();
        if (Status != CodecNoError)
        {
            SE_ERROR("Error while requesting Transcoded buffer: %d. Disabling transcoding\n", Status);
            TranscodeEnable = false;
        }
        ((StreamAudioCodecTransformContext_t *)TransformContext)->TranscodeBufferIndex = CurrentTranscodeBufferIndex;
    }
    if (CompressedFrameEnable && CompressedFrameNeeded)
    {
        CodecStatus_t Status = GetCompressedFrameBuffer(1); /* 1 Compressed frame */
        if (Status != CodecNoError)
        {
            SE_ERROR("Error while requesting CompressedFrame buffer: %d. Problem in the bypass of compressed frames over HDMI\n", Status);
            CompressedFrameEnable = false;
        }
        else
        {
            for (uint32_t i = 0; i < NoOfCompressedFrameBuffers; i++)
            {
                ((StreamAudioCodecTransformContext_t *)TransformContext)->CompressedFrameBufferIndex[i] = CurrentCompressedFrameBufferIndex[i];
            }
        }
    }
    PresetIOBuffers();
    Codec_MmeAudioStream_c::SetCommandIO();

    if (TranscodeEnable && TranscodeNeeded)
    {
        // StreamBase Transformer :: 0 Input Buffer / N+1 Output Buffer sent through same MME_TRANSFORM
        TransformContext->MMECommand.NumberOutputBuffers += 1;
    }

    if (CompressedFrameEnable && CompressedFrameNeeded)
    {
        // StreamBase Transformer :: 0 Input Buffer / N+1 Output Buffer sent through same MME_TRANSFORM
        TransformContext->MMECommand.NumberOutputBuffers += 1;
    }
}


////////////////////////////////////////////////////////////////////////////
///
/// Status Information display

#define SPDIFIN_TEXT(x) case x: return #x

static inline const char *reportStreamType(enum eMulticomSpdifPC type)
{
    switch (type)
    {
        SPDIFIN_TEXT(SPDIF_NULL_DATA_BURST);
        SPDIFIN_TEXT(SPDIF_AC3);
        SPDIFIN_TEXT(SPDIF_DDPLUS);
        SPDIFIN_TEXT(SPDIF_PAUSE_BURST);
        SPDIFIN_TEXT(SPDIF_MP1L1);
        SPDIFIN_TEXT(SPDIF_MP1L2L3);
        SPDIFIN_TEXT(SPDIF_MP2MC);
        SPDIFIN_TEXT(SPDIF_MP2AAC);
        SPDIFIN_TEXT(SPDIF_MP2L1LSF);
        SPDIFIN_TEXT(SPDIF_MP2L2LSF);
        SPDIFIN_TEXT(SPDIF_MP2L3LSF);
        SPDIFIN_TEXT(SPDIF_DTS1);
        SPDIFIN_TEXT(SPDIF_DTS2);
        SPDIFIN_TEXT(SPDIF_DTS3);
        SPDIFIN_TEXT(SPDIF_ATRAC);
        SPDIFIN_TEXT(SPDIF_ATRAC2_3);
        SPDIFIN_TEXT(SPDIF_DTS14);
        SPDIFIN_TEXT(SPDIF_DTS16);
        SPDIFIN_TEXT(SPDIF_RESERVED);
    default:
        return (type < SPDIF_RESERVED) ? "Lookup-Failed" : "UNKNOWN";
    }
}

static inline const char *reportState(enum eMulticomSpdifinState state)
{
    switch (state)
    {
        SPDIFIN_TEXT(SPDIFIN_STATE_RESET);
        SPDIFIN_TEXT(SPDIFIN_STATE_PCM_BYPASS);
        SPDIFIN_TEXT(SPDIFIN_STATE_COMPRESSED_BYPASS);
        SPDIFIN_TEXT(SPDIFIN_STATE_UNDERFLOW);

    default:
        SPDIFIN_TEXT(SPDIFIN_STATE_INVALID);
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
CodecStatus_t   Codec_MmeAudioSpdifin_c::ValidateDecodeContext(CodecBaseDecodeContext_t *Context)
{
    enum eMulticomSpdifinState NewState;
    enum eMulticomSpdifinState OldState = SpdifStatus.State;
    enum eMulticomSpdifPC      NewPC;
    enum eMulticomSpdifPC      OldPC    = SpdifStatus.StreamType;

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

    StreamAudioCodecTransformContext_t *TransformContext   = (StreamAudioCodecTransformContext_t *)Context;
    MME_LxAudioDecoderFrameStatus_t  *Status = &(TransformContext->DecodeStatus.DecStatus);
    tMMESpdifinStatus                *FrameStatus   = (tMMESpdifinStatus *) & (Status->FrameStatus[0]);
    NewState = (enum eMulticomSpdifinState) FrameStatus->CurrentState;
    NewPC    = (enum eMulticomSpdifPC) FrameStatus->PC;
    bool StatusChange = (AudioDecoderStatus.SamplingFreq != Status->SamplingFreq) ||
                        (AudioDecoderStatus.DecAudioMode != Status->DecAudioMode);

    if ((OldState != NewState) || (OldPC != NewPC) || StatusChange)
    {
        SpdifStatus.State      = NewState;
        SpdifStatus.StreamType = NewPC;
        SE_INFO(group_decoder_audio, "New State      :: %s after %d samples\n", reportState(NewState) ,  SpdifStatus.PlayedSamples);
        SE_INFO(group_decoder_audio, "New StreamType :: [%d] %s after %d samples\n", NewPC, reportStreamType(NewPC), SpdifStatus.PlayedSamples);
        // END SYSFS
    }

    SpdifStatus.PlayedSamples += Status->NbOutSamples;
    NumberOfSamplesProcessed  += Status->NbOutSamples;  // SYSFS
    SE_DEBUG(group_decoder_audio, "Transform Cmd returned\n");

    if (Status->DecStatus)
    {
        SE_WARNING("SPDIFIN audio decode error (muted frame): %d\n", Status->DecStatus);
        DecodeErrors++;
        // don't report an error to the higher levels (because the frame is muted)
    }

    //
    // Attach any codec derived metadata to the output buffer (or verify the
    // frame analysis if the frame analyser already filled everything in for
    // us).
    //
    OS_LockMutex(&Lock);
    ParsedAudioParameters_t  *AudioParameters = BufferState[TransformContext->BaseContext.BufferIndex].ParsedAudioParameters;
    if (AudioParameters == NULL)
    {
        SE_ERROR("(%s) - AudioParameters are NULL\n", Configuration.CodecName);
        OS_UnLockMutex(&Lock);
        return CodecError;
    }

    if (SpdifStatus.StreamType == SPDIF_AC3)
    {
        Codec_MmeAudioEAc3_c::FillStreamMetadata(AudioParameters, Status);
    }
    else if (((SpdifStatus.StreamType >= SPDIF_DTS1) && ((SpdifStatus.StreamType <= SPDIF_DTS3))) ||
             (SpdifStatus.StreamType == SPDIF_DTS14) || (SpdifStatus.StreamType == SPDIF_DTS16))
    {
        Codec_MmeAudioDtshd_c::FillStreamMetadata(AudioParameters, Status);
    }
    else
    {
        // do nothing, the AudioParameters are zeroed by FrameParser_Audio_c::Input() which is
        // appropriate (i.e. OriginalEncoding is AudioOriginalEncodingUnknown)
    }
    if (FrameStatus->CompressedDataInCompressedBuffer)
    {
        AudioParameters->OriginalEncoding  = AudioOriginalEncodingSPDIFIn_Compressed;
        AudioParameters->SpdifInProperties.PaOffSetInCompressedBuffer = FrameStatus->PaOffSetInCompressedBuffer;
    }
    else
    {
        AudioParameters->OriginalEncoding  = AudioOriginalEncodingSPDIFIn_Pcm;
    }
    AudioParameters->SpdifInProperties.SpdifInStreamType = SpdifStatus.StreamType;

    OS_UnLockMutex(&Lock);

    //SYSFS
    SetAudioCodecDecAttributes();

    return Codec_MmeAudioStream_c::ValidateDecodeContext(Context);
}


// /////////////////////////////////////////////////////////////////////////
//
//      Function to dump out the set stream
//      parameters from an mme command.
//

CodecStatus_t   Codec_MmeAudioSpdifin_c::DumpSetStreamParameters(void    *Parameters)
{
    SE_ERROR("Not implemented\n");
    return CodecNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Function to dump out the decode
//      parameters from an mme command.
//

CodecStatus_t   Codec_MmeAudioSpdifin_c::DumpDecodeParameters(void    *Parameters)
{
    SE_ERROR("Not implemented\n");
    return CodecNoError;
}

CodecStatus_t Codec_MmeAudioSpdifin_c::GetAttribute(const char *Attribute, PlayerAttributeDescriptor_t *Value)
{
    if (0 == strcmp(Attribute, "input_format"))
    {
        Value->Id = SYSFS_ATTRIBUTE_ID_CONSTCHARPOINTER;
#define C(x) case SPDIF_ ## x: Value->u.ConstCharPointer = #x; return CodecNoError

        switch (SpdifStatus.StreamType)
        {
            C(NULL_DATA_BURST);
            C(AC3);
            C(PAUSE_BURST);
            C(MP1L1);
            C(MP1L2L3);
            C(MP2MC);
            C(MP2AAC);
            C(MP2L1LSF);
            C(MP2L2LSF);
            C(MP2L3LSF);
            C(DTS1);
            C(DTS2);
            C(DTS3);
            C(ATRAC);
            C(ATRAC2_3);
            C(DDPLUS);
        case SPDIF_PCM:
            Value->u.ConstCharPointer = "PCM";
            return CodecNoError;
            C(DTS14);
            C(DTS16);

        default:
            SE_ERROR("This input_format does not exist\n");
            return CodecError;
        }

#undef C
    }
    else if (0 == strcmp(Attribute, "decode_errors"))
    {
        Value->Id   = SYSFS_ATTRIBUTE_ID_INTEGER;
        Value->u.Int    = DecodeErrors;
        return CodecNoError;
    }
    else if (0 == strcmp(Attribute, "supported_input_format"))
    {
        MME_LxAudioDecoderInfo_t &Capability = AudioDecoderTransformCapability;
        Value->Id                = SYSFS_ATTRIBUTE_ID_BOOL;

        switch (SpdifStatus.StreamType)
        {
        case SPDIF_AC3:
        case SPDIF_DDPLUS:
            Value->u.Bool = Capability.DecoderCapabilityExtFlags[0] & 0x8; // ACC_SPDIFIN_DD
            return CodecNoError;

        case SPDIF_DTS1:
        case SPDIF_DTS2:
        case SPDIF_DTS3:
        case SPDIF_DTS14:
        case SPDIF_DTS16:
            Value->u.Bool = Capability.DecoderCapabilityExtFlags[0] & 0x10; // ACC_SPDIFIN_DTS
            return CodecNoError;

        case SPDIF_MP2AAC:
            Value->u.Bool = Capability.DecoderCapabilityExtFlags[0] & 0x20; // ACC_SPDIFIN_MPG to be renamed ACC_SPDIFIN_AAC

        case SPDIF_PCM:
        case SPDIF_NULL_DATA_BURST:
        case SPDIF_PAUSE_BURST:
            Value->u.Bool = true;
            return CodecNoError;

        case SPDIF_MP1L1:
        case SPDIF_MP1L2L3:
        case SPDIF_MP2MC:
        case SPDIF_MP2L1LSF:
        case SPDIF_MP2L2LSF:
        case SPDIF_MP2L3LSF:
        case SPDIF_ATRAC:
        case SPDIF_ATRAC2_3:
        default:
            Value->u.Bool = false;
            return CodecNoError;
        }
    }
    else if (0 == strcmp(Attribute, "number_of_samples_processed"))
    {
        Value->Id   = SYSFS_ATTRIBUTE_ID_UNSIGNEDLONGLONGINT;
        Value->u.UnsignedLongLongInt = NumberOfSamplesProcessed;
        return CodecNoError;
    }
    else
    {
        CodecStatus_t Status;
        Status = Codec_MmeAudio_c::GetAttribute(Attribute, Value);

        if (Status != CodecNoError)
        {
            SE_ERROR("This attribute does not exist\n");
            return CodecError;
        }
    }

    return CodecNoError;
}

CodecStatus_t Codec_MmeAudioSpdifin_c::SetAttribute(const char *Attribute,  PlayerAttributeDescriptor_t *Value)
{
    if (0 == strcmp(Attribute, "decode_errors"))
    {
        DecodeErrors    = Value->u.Int;
        return CodecNoError;
    }
    else
    {
        SE_ERROR("This attribute cannot be set\n");
        return CodecError;
    }

    return CodecNoError;
}

void  Codec_MmeAudioSpdifin_c::SetAudioCodecDecAttributes()
{
    PlayerAttributeDescriptor_t *Value;
    /* set common attributes*/
    /* set "input_format" */
    Value = &Stream->Attributes().input_format;
    Value->Id = SYSFS_ATTRIBUTE_ID_CONSTCHARPOINTER;
#define C(x) case SPDIFIN_ ## x: Value->u.ConstCharPointer = #x; break

    switch (SpdifStatus.StreamType)
    {
        C(NULL_DATA_BURST);
        C(AC3);
        C(PAUSE_BURST);
        C(MP1L1);
        C(MP1L2L3);
        C(MP2MC);
        C(MP2AAC);
        C(MP2L1LSF);
        C(MP2L2LSF);
        C(MP2L3LSF);
        C(DTS1);
        C(DTS2);
        C(DTS3);
        C(ATRAC);
        C(ATRAC2_3);
        C(DDPLUS);
    case SPDIFIN_IEC60958:
        Value->u.ConstCharPointer = "PCM";
        break;
        C(IEC60958_DTS14);
        C(IEC60958_DTS16);

    default:
        SE_ERROR("This input_format does not exist\n");
    }

#undef Codec
    /* set "decode_errors" */
    Value = &Stream->Attributes().decode_errors;
    Value->Id     = SYSFS_ATTRIBUTE_ID_INTEGER;
    Value->u.Int  = DecodeErrors;
    /* set  "supported_input_format" */
    Value = &Stream->Attributes().supported_input_format;
    MME_LxAudioDecoderInfo_t &Capability = AudioDecoderTransformCapability;
    Value->Id      = SYSFS_ATTRIBUTE_ID_BOOL;

    switch (SpdifStatus.StreamType)
    {
    case SPDIFIN_AC3:
    case SPDIF_DDPLUS:
        Value->u.Bool = Capability.DecoderCapabilityExtFlags[0] & 0x8; // ACC_SPDIFIN_DD
        break;

    case SPDIFIN_DTS1:
    case SPDIFIN_DTS2:
    case SPDIFIN_DTS3:
    case SPDIFIN_IEC60958_DTS14:
    case SPDIFIN_IEC60958_DTS16:
        Value->u.Bool = Capability.DecoderCapabilityExtFlags[0] & 0x10; // ACC_SPDIFIN_DTS
        break;

    case SPDIFIN_MP2AAC:
        Value->u.Bool = Capability.DecoderCapabilityExtFlags[0] & 0x20; // ACC_SPDIFIN_MPG to be renamed ACC_SPDIFIN_AAC

    case SPDIFIN_IEC60958:
    case SPDIFIN_NULL_DATA_BURST:
    case SPDIFIN_PAUSE_BURST:
        Value->u.Bool = true;
        break;

    case SPDIFIN_MP1L1:
    case SPDIFIN_MP1L2L3:
    case SPDIFIN_MP2MC:
    case SPDIFIN_MP2L1LSF:
    case SPDIFIN_MP2L2LSF:
    case SPDIFIN_MP2L3LSF:
    case SPDIFIN_ATRAC:
    case SPDIFIN_ATRAC2_3:
    default:
        Value->u.Bool = false;
    }

    /* set "number_of_samples_processed" */
    Value = &Stream->Attributes().number_of_samples_processed;
    Value->Id  = SYSFS_ATTRIBUTE_ID_UNSIGNEDLONGLONGINT;
    Value->u.UnsignedLongLongInt = NumberOfSamplesProcessed;
}
////////////////////////////////////////////////////////////////////////////
///
///  Public static function to fill IEC61937 codec capabilities
///  to expose it through STM_SE_CTRL_GET_CAPABILITY_AUDIO_DECODE control.
///
void Codec_MmeAudioSpdifin_c::GetCapabilities(stm_se_audio_dec_capability_t::audio_dec_iec61937_capability_s *cap,
                                              const MME_LxAudioDecoderHDInfo_t decInfo)
{
    const int ExtFlags = Codec_Capabilities_c::ExtractAudioExtendedFlags(decInfo, ACC_SPDIFIN);
    cap->common.capable = (decInfo.DecoderCapabilityFlags     & (1 << ACC_SPDIFIN)
                           & SE_AUDIO_DEC_CAPABILITIES        & (1 << ACC_SPDIFIN)
                          ) ? true : false;
    cap->iec61937_DD  = (ExtFlags & (1 << ACC_SPDIFIN_DD))  ? true : false;
    cap->iec61937_DTS = (ExtFlags & (1 << ACC_SPDIFIN_DTS)) ? true : false;
    cap->iec61937_MPG = (ExtFlags & (1 << ACC_SPDIFIN_MPG)) ? true : false;
    /* Firmware does not expose this capability extension */
    cap->iec61937_AAC = cap->common.capable               ? true : false;
}
