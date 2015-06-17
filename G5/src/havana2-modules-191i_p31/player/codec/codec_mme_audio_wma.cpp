/************************************************************************
Copyright (C) 2007 STMicroelectronics. All Rights Reserved.

This file is part of the Player2 Library.

Player2 is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License version 2 as published by the
Free Software Foundation.

Player2 is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with player2; see the file COPYING.  If not, write to the Free Software
Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

The Player2 Library may alternatively be licensed under a proprietary
license from ST.

Source file name : codec_mme_audio_wma.cpp
Author :           Adam

Implementation of the wma audio codec class for player 2.


Date        Modification                                    Name
----        ------------                                    --------
11-Sep-07   Created (from codec_mme_audio_mpeg.cpp)         Adam

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
/// previous transform is complete and everytime an MME_SEND_BUFFERS command
/// is issued.
///
/// \todo We need to move more into the thread to eliminate the race conditions.
///

// /////////////////////////////////////////////////////////////////////
//
//      Include any component headers

#include "codec_mme_audio_wma.h"
#include "wma_properties.h"
#include "wma_audio.h"
#include "ksound.h"

//! AWTODO
#ifdef __KERNEL__
extern "C"{void flush_cache_all();};
#endif
//!

// /////////////////////////////////////////////////////////////////////////
//
// Locally defined constants
//
//#define NUM_SEND_BUFFERS_COMMANDS 4
#define MAX_ASF_PACKET_SIZE 16384

#define MAXIMUM_STALL_PERIOD    250000          // 1/4 second

// /////////////////////////////////////////////////////////////////////////
//
// Locally defined structures
//

typedef struct WmaAudioCodecStreamParameterContext_s
{
    CodecBaseStreamParameterContext_t   BaseContext;

    MME_LxAudioDecoderGlobalParams_t StreamParameters;
} WmaAudioCodecStreamParameterContext_t;

//#if __KERNEL__
#if 0
#define BUFFER_WMA_AUDIO_CODEC_STREAM_PARAMETER_CONTEXT         "WmaAudioCodecStreamParameterContext"
#define BUFFER_WMA_AUDIO_CODEC_STREAM_PARAMETER_CONTEXT_TYPE    {BUFFER_WMA_AUDIO_CODEC_STREAM_PARAMETER_CONTEXT, BufferDataTypeBase, AllocateFromDeviceMemory, 32, 0, true, true, sizeof(WmaAudioCodecStreamParameterContext_t)}
#else
#define BUFFER_WMA_AUDIO_CODEC_STREAM_PARAMETER_CONTEXT         "WmaAudioCodecStreamParameterContext"
#define BUFFER_WMA_AUDIO_CODEC_STREAM_PARAMETER_CONTEXT_TYPE    {BUFFER_WMA_AUDIO_CODEC_STREAM_PARAMETER_CONTEXT, BufferDataTypeBase, AllocateFromOSMemory, 32, 0, true, true, sizeof(WmaAudioCodecStreamParameterContext_t)}
#endif

static BufferDataDescriptor_t            WmaAudioCodecStreamParameterContextDescriptor = BUFFER_WMA_AUDIO_CODEC_STREAM_PARAMETER_CONTEXT_TYPE;

// --------

typedef struct WmaAudioCodecDecodeContext_s
{
    CodecBaseDecodeContext_t            BaseContext;

    MME_StreamingBufferParams_t         DecodeParameters;
    MME_LxAudioDecoderFrameStatus_t     DecodeStatus;
} WmaAudioCodecDecodeContext_t;

//#if __KERNEL__
#if 0
#define BUFFER_WMA_AUDIO_CODEC_DECODE_CONTEXT   "WmaAudioCodecDecodeContext"
#define BUFFER_WMA_AUDIO_CODEC_DECODE_CONTEXT_TYPE      {BUFFER_WMA_AUDIO_CODEC_DECODE_CONTEXT, BufferDataTypeBase, AllocateFromDeviceMemory, 32, 0, true, true, sizeof(WmaAudioCodecDecodeContext_t)}
#else
#define BUFFER_WMA_AUDIO_CODEC_DECODE_CONTEXT   "WmaAudioCodecDecodeContext"
#define BUFFER_WMA_AUDIO_CODEC_DECODE_CONTEXT_TYPE      {BUFFER_WMA_AUDIO_CODEC_DECODE_CONTEXT, BufferDataTypeBase, AllocateFromOSMemory, 32, 0, true, true, sizeof(WmaAudioCodecDecodeContext_t)}
#endif

static BufferDataDescriptor_t           WmaAudioCodecDecodeContextDescriptor    = BUFFER_WMA_AUDIO_CODEC_DECODE_CONTEXT_TYPE;


//{{{  Constructor
////////////////////////////////////////////////////////////////////////////
///
/// Fill in the configuration parameters used by the super-class and reset everything.
///
Codec_MmeAudioWma_c::Codec_MmeAudioWma_c( void )
{
    CODEC_DEBUG("%s\n", __FUNCTION__);
    Configuration.CodecName                             = "WMA audio";

    Configuration.StreamParameterContextCount           = 1;
    Configuration.StreamParameterContextDescriptor      = &WmaAudioCodecStreamParameterContextDescriptor;

    Configuration.DecodeContextCount                    = SENDBUF_DECODE_CONTEXT_COUNT; //4;
    Configuration.DecodeContextDescriptor               = &WmaAudioCodecDecodeContextDescriptor;

    Configuration.MaximumSampleCount                    = 4096;

//

    AudioDecoderTransformCapabilityMask.DecoderCapabilityFlags = (1 << ACC_WMAPROLSL);

    DecoderId                                           = ACC_WMAPROLSL_ID;

    Reset();

    SendbufTriggerTransformCount                        = 1;

}
//}}}  
//{{{  Destructor
////////////////////////////////////////////////////////////////////////////
///
///     Destructor function, ensures a full halt and reset
///     are executed for all levels of the class.
///
Codec_MmeAudioWma_c::~Codec_MmeAudioWma_c( void )
{
    CODEC_DEBUG("%s\n", __FUNCTION__);
    Halt();
    Reset();

}
//}}}  

//{{{  FillOutTransformerGlobalParameters
////////////////////////////////////////////////////////////////////////////
///
/// Populate the supplied structure with parameters for WMA audio.
///
///
CodecStatus_t Codec_MmeAudioWma_c::FillOutTransformerGlobalParameters( MME_LxAudioDecoderGlobalParams_t *GlobalParams_p)
{

    MME_LxAudioDecoderGlobalParams_t &GlobalParams = *GlobalParams_p;
    GlobalParams.StructSize = sizeof(MME_LxAudioDecoderGlobalParams_t);
//    MME_LxWmaConfig_t &Config = *((MME_LxWmaConfig_t *) GlobalParams.DecConfig);
    MME_LxWmaProLslConfig_t &Config = *((MME_LxWmaProLslConfig_t *) GlobalParams.DecConfig);

    CODEC_TRACE("Initializing WMA audio decoder\n");

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
        WmaAudioStreamParameters_t*     StreamParams    = (WmaAudioStreamParameters_t *)ParsedFrameParameters->StreamParameterStructure;

        Config.NbSamplesOut = StreamParams->SamplesPerFrame ? StreamParams->SamplesPerFrame : 2048;
        CODEC_TRACE("%s - StreamParams->SamplesPerFrame %d\n", __FUNCTION__, StreamParams->SamplesPerFrame );
        CODEC_TRACE("%s - Config.NbSamplesOut %d\n", __FUNCTION__, Config.NbSamplesOut );

        if (0 == StreamParams->StreamNumber)
        {
            // zero is an illegal stream number
            CODEC_ERROR("ILLEGAL STREAM NUMBER\n" );
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
            CODEC_TRACE("%s - streamInfo.nSamplesPerBlock %d\n", __FUNCTION__, streamInfo.nSamplesPerBlock);
            streamInfo.dwChannelMask       = StreamParams->ChannelMask;
            streamInfo.nBitsPerSample      = StreamParams->BitsPerSample;
            streamInfo.wValidBitsPerSample = StreamParams->ValidBitsPerSample;
            streamInfo.wStreamId           = StreamParams->StreamNumber;
            CODEC_TRACE("%s INFO : streamInfo.nSamplesPerSec %d \n", __FUNCTION__, streamInfo.nSamplesPerSec);

            // HACK: see ValidateCompletedCommand()
            //NumChannels = StreamParams.NumberOfChannels;
        }
    }
    else
    {
        CODEC_ERROR("No Params\n" );
        //no params, no set-up
        Config.NewAudioStreamInfo = ACC_MME_FALSE;
        Config.StructSize = 2 * sizeof(U32) ; // only transmit the ID and StructSize (all other params are irrelevant)
    }
    // This is not what the ACC headers make it look like but this is what
    // the firmware actually expects (tightly packed against the config structure)
    // unsigned char *pcmParams = ((unsigned char *) &Config) + Config.StructSize;
    // FillPcmProcessingGlobalParams((void *) pcmParams);

    return Codec_MmeAudio_c::FillOutTransformerGlobalParameters( GlobalParams_p );

    //  return CodecNoError;
}
//}}}  

