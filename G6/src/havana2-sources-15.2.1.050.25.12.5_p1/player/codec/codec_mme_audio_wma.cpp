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
/// \class Codec_MmeAudioWma_c
///
/// The WMA audio codec proxy.
///
/// It is difficult for the host processor to efficiently determine the WMA
/// frame boundaries (the stream must be huffman decoded before the frame
/// boundaries are apparent). For this reason the player makes no attempt
/// to discover the frame boundaries. Unfortunately this makes the WMA codec
/// proxy implementation particularly complex. For this class there is not a
/// one-to-one relationship between input buffers and output buffers requiring
/// us to operate the decoder with streaming input (MME_SEND_BUFFERS) extracting
/// frame based output whenever we believe the decoder to be capable of
/// providing data (MME_TRANSFORM).
///
/// To assist with this we introduce a thread whose job is to manage the
/// issue of MME_TRANSFORM commands. This thread is woken every time a
/// previous transform is complete and every time an MME_SEND_BUFFERS command
/// is issued.
///
/// \todo We need to move more into the thread to eliminate the race conditions.
///

#include "codec_mme_audio_wma.h"
#include "codec_capabilities.h"
#include "wma_properties.h"
#include "wma_audio.h"
#include "ksound.h"

#undef TRACE_TAG
#define TRACE_TAG "Codec_MmeAudioWma_c"

//#define NUM_SEND_BUFFERS_COMMANDS 4
#define MAX_ASF_PACKET_SIZE 16384

#define MAXIMUM_STALL_PERIOD    250000          // 1/4 second

#define BUFFER_WMA_AUDIO_CODEC_STREAM_PARAMETER_CONTEXT         "WmaAudioCodecStreamParameterContext"
#define BUFFER_WMA_AUDIO_CODEC_STREAM_PARAMETER_CONTEXT_TYPE    {BUFFER_WMA_AUDIO_CODEC_STREAM_PARAMETER_CONTEXT, BufferDataTypeBase, AllocateFromOSMemory, 32, 0, true, true, sizeof(StreamAudioCodecStreamParameterContext_t)}

static BufferDataDescriptor_t            WmaAudioCodecStreamParameterContextDescriptor = BUFFER_WMA_AUDIO_CODEC_STREAM_PARAMETER_CONTEXT_TYPE;

// --------


#define BUFFER_WMA_AUDIO_CODEC_DECODE_CONTEXT   "WmaAudioCodecDecodeContext"
#define BUFFER_WMA_AUDIO_CODEC_DECODE_CONTEXT_TYPE      {BUFFER_WMA_AUDIO_CODEC_DECODE_CONTEXT, BufferDataTypeBase, AllocateFromOSMemory, 32, 0, true, true, sizeof(StreamAudioCodecDecodeContext_t)}

static BufferDataDescriptor_t           WmaAudioCodecDecodeContextDescriptor    = BUFFER_WMA_AUDIO_CODEC_DECODE_CONTEXT_TYPE;

///
Codec_MmeAudioWma_c::Codec_MmeAudioWma_c(void)
    : Codec_MmeAudioStream_c()
{
    Configuration.CodecName                             = "WMA audio";
    Configuration.StreamParameterContextCount           = 1;
    Configuration.StreamParameterContextDescriptor      = &WmaAudioCodecStreamParameterContextDescriptor;
    Configuration.DecodeContextCount                    = SENDBUF_DECODE_CONTEXT_COUNT; //4;
    Configuration.DecodeContextDescriptor               = &WmaAudioCodecDecodeContextDescriptor;
    // WMA decoder itself decodes up to 4k samples at a time...
    // Request here a limitation to 1k samples to minimize the buffer sizes.
    Configuration.MaximumSampleCount                    = WMA_BLOCK_WISE_SAMPLE_COUNT; //Fixed to 1024 for WMA blockwise;

    AudioDecoderTransformCapabilityMask.DecoderCapabilityFlags = (1 << ACC_WMAPROLSL);
    DecoderId                                           = ACC_WMAPROLSL_ID;
    SendbufTriggerTransformCount                        = 1;
}

///
Codec_MmeAudioWma_c::~Codec_MmeAudioWma_c(void)
{
    Halt();
}

//{{{  FillOutTransformerGlobalParameters
////////////////////////////////////////////////////////////////////////////
///
/// Populate the supplied structure with parameters for WMA audio.
///
///
CodecStatus_t Codec_MmeAudioWma_c::FillOutTransformerGlobalParameters(MME_LxAudioDecoderGlobalParams_t *GlobalParams_p)
{
    MME_LxAudioDecoderGlobalParams_t &GlobalParams = *GlobalParams_p;
    GlobalParams.StructSize = sizeof(MME_LxAudioDecoderGlobalParams_t);
//    MME_LxWmaConfig_t &Config = *((MME_LxWmaConfig_t *) GlobalParams.DecConfig);
    MME_LxWmaProLslConfig_t &Config = *((MME_LxWmaProLslConfig_t *) GlobalParams.DecConfig);
    SE_INFO(group_decoder_audio, " Initializing WMA audio decoder\n");
//    MME_LxWmaProLslConfig_t &config = ((MME_LxWmaProLslConfig_t *) globalParams.DecConfig)[0];
    memset(&Config, 0, sizeof(Config));
    Config.DecoderId = ACC_WMAPROLSL_ID;
    /*
    Audio_DecoderTypes.h:   ACC_WMA9_ID,
    Audio_DecoderTypes.h:   ACC_WMAPROLSL_ID,
    Audio_DecoderTypes.h:   ACC_WMA_ST_FILE,
    Audio_EncoderTypes.h:   ACC_WMAE_ID,
    */
    Config.StructSize = sizeof(Config);
#if 0
    config.MaxNbPages = NUM_SEND_BUFFERS_COMMANDS;
#else
    // In BL012_5 there are problems in the WMA decoder that can cause it to mismanage
    // the page arrays. Picking a 'large' number for MaxNbPages ensures that there are
    // a few unused page structures lying around which makes running of the end of the
    // page arrays slightly less likely
    Config.MaxNbPages = 16;
#endif
    Config.MaxPageSize = MAX_ASF_PACKET_SIZE; // default if no other is specified

    if ((ParsedFrameParameters != NULL) && (ParsedFrameParameters->StreamParameterStructure != NULL))
    {
        WmaAudioStreamParameters_t     *StreamParams    = (WmaAudioStreamParameters_t *)ParsedFrameParameters->StreamParameterStructure;
        // bug22980 : fix the number of output samples instead of using StreamParams->SamplesPerFrame to keep control of memory footprint
        Config.NbSamplesOut = 2048;
        SE_INFO(group_decoder_audio, "StreamParams->SamplesPerFrame %d\n", StreamParams->SamplesPerFrame);
        SE_INFO(group_decoder_audio, "Config.NbSamplesOut %d\n", Config.NbSamplesOut);

        if (0 == StreamParams->StreamNumber)
        {
            // zero is an illegal stream number
            SE_ERROR("ILLEGAL STREAM NUMBER\n");
            Config.NewAudioStreamInfo       = ACC_MME_FALSE;
            NeedToMarkStreamUnplayable      = true;
            return CodecError;
        }
        else
        {
            Config.NewAudioStreamInfo = ACC_MME_TRUE;
            //MME_WmaAudioStreamInfo_t &streamInfo = Config.AudioStreamInfo;
            MME_WmaProLslAudioStreamInfo_t &streamInfo = Config.AudioStreamInfo;
            streamInfo.nVersion            = (StreamParams->FormatTag == WMA_VERSION_2_9) ? 2 :     // WMA V2
                                             (StreamParams->FormatTag == WMA_VERSION_9_PRO) ? 3 :   // WMA Pro
                                             (StreamParams->FormatTag == WMA_LOSSLESS) ? 4 : 1;     // WMA lossless - Default to WMA version1?
            streamInfo.wFormatTag          = StreamParams->FormatTag;
            streamInfo.nSamplesPerSec      = StreamParams->SamplesPerSecond;
            streamInfo.nAvgBytesPerSec     = StreamParams->AverageNumberOfBytesPerSecond;
            streamInfo.nBlockAlign         = StreamParams->BlockAlignment;
            streamInfo.nChannels           = StreamParams->NumberOfChannels;
            streamInfo.nEncodeOpt          = StreamParams->EncodeOptions;
            streamInfo.nSamplesPerBlock    = StreamParams->SamplesPerBlock;
            SE_INFO(group_decoder_audio, "streamInfo.nSamplesPerBlock %d\n", streamInfo.nSamplesPerBlock);
            streamInfo.dwChannelMask       = StreamParams->ChannelMask;
            streamInfo.nBitsPerSample      = StreamParams->BitsPerSample;
            streamInfo.wValidBitsPerSample = StreamParams->ValidBitsPerSample;
            streamInfo.wStreamId           = StreamParams->StreamNumber;
            SE_INFO(group_decoder_audio, "streamInfo.nSamplesPerSec %d\n", streamInfo.nSamplesPerSec);
            // HACK: see ValidateCompletedCommand()
            //NumChannels = StreamParams.NumberOfChannels;
        }
    }
    else
    {
        SE_ERROR("No Params\n");
        //no params, no set-up
        Config.NewAudioStreamInfo = ACC_MME_FALSE;
        Config.StructSize = 2 * sizeof(U32) ; // only transmit the ID and StructSize (all other params are irrelevant)
    }

    // This is not what the ACC headers make it look like but this is what
    // the firmware actually expects (tightly packed against the config structure)
    // unsigned char *pcmParams = ((unsigned char *) &Config) + Config.StructSize;
    // FillPcmProcessingGlobalParams((void *) pcmParams);
    return Codec_MmeAudio_c::FillOutTransformerGlobalParameters(GlobalParams_p);
    //  return CodecNoError;
}
//}}}
//{{{ GetCapabilities
////////////////////////////////////////////////////////////////////////////
///
///  Public static function to fill WMA codec capabilities
///  to expose it through STM_SE_CTRL_GET_CAPABILITY_AUDIO_DECODE control.
///
void Codec_MmeAudioWma_c::GetCapabilities(stm_se_audio_dec_capability_t::audio_dec_wma_capability_s *cap,
                                          const MME_LxAudioDecoderHDInfo_t decInfo)
{
    const int ExtFlags = Codec_Capabilities_c::ExtractAudioExtendedFlags(decInfo, ACC_WMAPROLSL);
    cap->common.capable = (decInfo.DecoderCapabilityFlags     & (1 << ACC_WMAPROLSL)
                           & SE_AUDIO_DEC_CAPABILITIES        & (1 << ACC_WMAPROLSL)
                          ) ? true : false;
    cap->wma_STD      = (ExtFlags & (1 << ACC_WMA_STD))      ? true : false;
    cap->wma_PRO      = (ExtFlags & (1 << ACC_WMA_PRO))      ? true : false;
    cap->wma_LOSSLESS = (ExtFlags & (1 << ACC_WMA_LOSSLESS)) ? true : false;
    cap->wma_VOICE    = (ExtFlags & (1 << ACC_WMA_VOICE))    ? true : false;
}
//}}}
