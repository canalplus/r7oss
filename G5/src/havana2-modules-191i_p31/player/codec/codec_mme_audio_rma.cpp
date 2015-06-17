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

Source file name : codec_mme_audio_rma.cpp
Author :           Julian

Implementation of the Real Media Audio codec class for player 2.


Date            Modification            Name
----            ------------            --------
28-Jan-09       Created                 Julian

************************************************************************/

////////////////////////////////////////////////////////////////////////////
/// \class Codec_MmeAudioRma_c
///
/// The RMA audio codec proxy.
///

// /////////////////////////////////////////////////////////////////////
//
//      Include any component headers

#include "codec_mme_audio_rma.h"
#include "rma_audio.h"

// /////////////////////////////////////////////////////////////////////////
//
// Locally defined constants
//

static inline unsigned int BE2LE (unsigned int Value)
{
    return (((Value&0xff)<<24) | ((Value&0xff00)<<8) | ((Value>>8)&0xff00) | ((Value>>24)&0xff));
}


// /////////////////////////////////////////////////////////////////////////
//
// Locally defined structures
//

typedef struct RmaAudioCodecStreamParameterContext_s
{
    CodecBaseStreamParameterContext_t   BaseContext;

    MME_LxAudioDecoderGlobalParams_t StreamParameters;
} RmaAudioCodecStreamParameterContext_t;

#define BUFFER_RMA_AUDIO_CODEC_STREAM_PARAMETER_CONTEXT         "RmaAudioCodecStreamParameterContext"
#define BUFFER_RMA_AUDIO_CODEC_STREAM_PARAMETER_CONTEXT_TYPE    {BUFFER_RMA_AUDIO_CODEC_STREAM_PARAMETER_CONTEXT, BufferDataTypeBase, AllocateFromOSMemory, 32, 0, true, true, sizeof(RmaAudioCodecStreamParameterContext_t)}

static BufferDataDescriptor_t           RmaAudioCodecStreamParameterContextDescriptor   = BUFFER_RMA_AUDIO_CODEC_STREAM_PARAMETER_CONTEXT_TYPE;

typedef struct RmaAudioCodecDecodeContext_s
{
    CodecBaseDecodeContext_t            BaseContext;

    MME_LxAudioDecoderFrameParams_t     DecodeParameters;
    MME_LxAudioDecoderFrameStatus_t     DecodeStatus;
} RmaAudioCodecDecodeContext_t;

#define BUFFER_RMA_AUDIO_CODEC_DECODE_CONTEXT                   "RmaAudioCodecDecodeContext"
#define BUFFER_RMA_AUDIO_CODEC_DECODE_CONTEXT_TYPE              {BUFFER_RMA_AUDIO_CODEC_DECODE_CONTEXT, BufferDataTypeBase, AllocateFromOSMemory, 32, 0, true, true, sizeof(RmaAudioCodecDecodeContext_t)}

static BufferDataDescriptor_t           RmaAudioCodecDecodeContextDescriptor    = BUFFER_RMA_AUDIO_CODEC_DECODE_CONTEXT_TYPE;



//{{{  Constructor
////////////////////////////////////////////////////////////////////////////
///
/// Fill in the configuration parameters used by the super-class and reset everything.
///
Codec_MmeAudioRma_c::Codec_MmeAudioRma_c( void )
{
    Configuration.CodecName                             = "Rma audio";

    Configuration.StreamParameterContextCount           = 1;
    Configuration.StreamParameterContextDescriptor      = &RmaAudioCodecStreamParameterContextDescriptor;

    Configuration.DecodeContextCount                    = 4;
    Configuration.DecodeContextDescriptor               = &RmaAudioCodecDecodeContextDescriptor;

    AudioDecoderTransformCapabilityMask.DecoderCapabilityFlags = (1 << ACC_REAL_AUDIO);

    DecoderId                                           = ACC_REALAUDIO_ID;
    RestartTransformer                                  = ACC_MME_FALSE;

    Reset();
}
//}}}  
//{{{  Destructor
////////////////////////////////////////////////////////////////////////////
///
///     Destructor function, ensures a full halt and reset 
///     are executed for all levels of the class.
///
Codec_MmeAudioRma_c::~Codec_MmeAudioRma_c( void )
{
    Halt();
    Reset();
}
//}}}  
//{{{  FillOutTransformerGlobalParameters
////////////////////////////////////////////////////////////////////////////
///
/// Populate the supplied structure with parameters for RMA audio.
///
///
CodecStatus_t Codec_MmeAudioRma_c::FillOutTransformerGlobalParameters( MME_LxAudioDecoderGlobalParams_t *GlobalParams_p )
{
    MME_LxAudioDecoderGlobalParams_t   &GlobalParams    = *GlobalParams_p;

    CODEC_TRACE ("Initializing RMA audio decoder\n");

    GlobalParams.StructSize             = sizeof(MME_LxAudioDecoderGlobalParams_t);

    MME_LxRealAudioConfig_t &Config     = *((MME_LxRealAudioConfig_t*)GlobalParams.DecConfig);

    Config.DecoderId                    = DecoderId;
    Config.StructSize                   = sizeof(Config);

#if DRV_MULTICOM_AUDIO_DECODER_VERSION >= 0x090128

    if (ParsedFrameParameters != NULL)
    {
        RmaAudioStreamParameters_s*     StreamParams    = (RmaAudioStreamParameters_s*)ParsedFrameParameters->StreamParameterStructure;

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

    CODEC_TRACE("DecoderId                  %d\n", Config.DecoderId);
    CODEC_TRACE("StructSize                 %d\n", Config.StructSize);
    CODEC_TRACE("CodecType                %.4s\n", (unsigned char*)&Config.CodecType);
    CODEC_TRACE("usNumChannels              %d\n", Config.usNumChannels);
    CODEC_TRACE("usFlavorIndex              %d\n", Config.usFlavorIndex);
    CODEC_TRACE("NbSample2Conceal           %d\n", Config.NbSample2Conceal);
    CODEC_TRACE("ulSampleRate               %d\n", Config.ulSampleRate);
    CODEC_TRACE("ulBitsPerFrame             %d\n", Config.ulBitsPerFrame);
    CODEC_TRACE("ulGranularity              %d\n", Config.ulGranularity);
    CODEC_TRACE("ulOpaqueDataSize           %d\n", Config.ulOpaqueDataSize);
    CODEC_TRACE("Features                   %d\n", Config.Features);

#endif // DRV_MULTICOM_AUDIO_DECODER_VERSION >= 0x090128

    return Codec_MmeAudio_c::FillOutTransformerGlobalParameters (GlobalParams_p);
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
CodecStatus_t   Codec_MmeAudioRma_c::FillOutTransformerInitializationParameters( void )
{
    CodecStatus_t                       Status;
    MME_LxAudioDecoderInitParams_t     &Params                  = AudioDecoderInitializationParameters;

    MMEInitializationParameters.TransformerInitParamsSize       = sizeof(Params);
    MMEInitializationParameters.TransformerInitParams_p         = &Params;

    Status                                                      = Codec_MmeAudio_c::FillOutTransformerInitializationParameters();
    if (Status != CodecNoError)
        return Status;

    return FillOutTransformerGlobalParameters (&Params.GlobalParams);
}
//}}}  
//{{{  FillOutSetStreamParametersCommand
////////////////////////////////////////////////////////////////////////////
///
/// Populate the AUDIO_DECODER's MME_SET_GLOBAL_TRANSFORMER_PARAMS parameters for RMA audio.
///
CodecStatus_t   Codec_MmeAudioRma_c::FillOutSetStreamParametersCommand( void )
{
    CodecStatus_t                               Status;
    RmaAudioCodecStreamParameterContext_t*      Context = (RmaAudioCodecStreamParameterContext_t *)StreamParameterContext;

    DecoderId           = ACC_REALAUDIO_ID;

    // Fill out the structure
    memset (&(Context->StreamParameters), 0, sizeof(Context->StreamParameters));
    Status              = FillOutTransformerGlobalParameters (&(Context->StreamParameters));
    if (Status != CodecNoError)
        return Status;

    // Fillout the actual command
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
CodecStatus_t   Codec_MmeAudioRma_c::FillOutDecodeCommand(       void )
{
    RmaAudioCodecDecodeContext_t    *Context                            = (RmaAudioCodecDecodeContext_t *)DecodeContext;

    CODEC_DEBUG ("%s: Initializing decode params\n", __FUNCTION__);

    // Initialize the frame parameters
    memset (&Context->DecodeParameters, 0, sizeof(Context->DecodeParameters));
    Context->DecodeParameters.Restart           = RestartTransformer;

    RestartTransformer                          = ACC_MME_FALSE;

    // Zero the reply structure
    memset( &Context->DecodeStatus, 0, sizeof(Context->DecodeStatus) );

    // Fillout the actual command
    Context->BaseContext.MMECommand.CmdStatus.AdditionalInfoSize        = sizeof(Context->DecodeStatus);
    Context->BaseContext.MMECommand.CmdStatus.AdditionalInfo_p          = (MME_GenericParams_t)(&Context->DecodeStatus);
    Context->BaseContext.MMECommand.ParamSize                           = sizeof(Context->DecodeParameters);
    Context->BaseContext.MMECommand.Param_p                             = (MME_GenericParams_t)(&Context->DecodeParameters);

    CODEC_DEBUG("Restart                    %d\n", Context->DecodeParameters.Restart);
    CODEC_DEBUG("AdditionalInfoSize         %d\n", Context->BaseContext.MMECommand.CmdStatus.AdditionalInfoSize);

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
CodecStatus_t   Codec_MmeAudioRma_c::ValidateDecodeContext( CodecBaseDecodeContext_t *Context )
{
    RmaAudioCodecDecodeContext_t*       DecodeContext   = (RmaAudioCodecDecodeContext_t*)Context;
    MME_LxAudioDecoderFrameStatus_t    &Status          = DecodeContext->DecodeStatus;
    ParsedAudioParameters_t*            AudioParameters;


    //CODEC_TRACE ("%s: DecStatus %d\n", __FUNCTION__, Status.DecStatus);

    if (ENABLE_CODEC_DEBUG)
    {
        //DumpCommand(bufferIndex);
    }

    if (Status.DecStatus != ACC_HXR_OK)
    {
        CODEC_ERROR("RMA audio decode error (muted frame): %d\n", Status.DecStatus);
        //DumpCommand(bufferIndex);
        // don't report an error to the higher levels (because the frame is muted)
    }

    // SYSFS
    AudioDecoderStatus                          = Status;

    //
    // Attach any codec derived metadata to the output buffer (or verify the
    // frame analysis if the frame analyser already filled everything in for
    // us).
    //

    AudioParameters                             = BufferState[DecodeContext->BaseContext.BufferIndex].ParsedAudioParameters;

    AudioParameters->Source.BitsPerSample       = AudioOutputSurface->BitsPerSample;
    AudioParameters->Source.ChannelCount        = AudioOutputSurface->ChannelCount;
    AudioParameters->Organisation               = Status.AudioMode;

    AudioParameters->SampleCount                = Status.NbOutSamples;

    int SamplingFreqCode                        = Status.SamplingFreq;
    if (SamplingFreqCode < ACC_FS_reserved)
    {
        AudioParameters->Source.SampleRateHz    = ACC_SamplingFreqLUT[SamplingFreqCode];
    }
    else
    {
        AudioParameters->Source.SampleRateHz    = 0;
        CODEC_ERROR("RMA audio decode bad sampling freq returned: 0x%x\n", SamplingFreqCode);
    }

    CODEC_DEBUG("AudioParameters->Source.BitsPerSample         %d\n", AudioParameters->Source.BitsPerSample);
    CODEC_DEBUG("AudioParameters->Source.ChannelCount          %d\n", AudioParameters->Source.ChannelCount);
    CODEC_DEBUG("AudioParameters->Organisation                 %d\n", AudioParameters->Organisation);
    CODEC_DEBUG("AudioParameters->SampleCount                  %d\n", AudioParameters->SampleCount);
    CODEC_DEBUG("AudioParameters->Source.SampleRateHz          %d\n", AudioParameters->Source.SampleRateHz);

    return CodecNoError;
}
//}}}  
//{{{  DumpSetStreamParameters
// /////////////////////////////////////////////////////////////////////////
//
//      Function to dump out the set stream 
//      parameters from an mme command.
//

CodecStatus_t   Codec_MmeAudioRma_c::DumpSetStreamParameters(           void    *Parameters )
{
    CODEC_ERROR("Not implemented\n");
    return CodecNoError;
}
//}}}  
//{{{  DumpDecodeParameters
// /////////////////////////////////////////////////////////////////////////
//
//      Function to dump out the decode
//      parameters from an mme command.
//

CodecStatus_t   Codec_MmeAudioRma_c::DumpDecodeParameters(              void    *Parameters )
{
    CODEC_TRACE("%s: TotalSize[0]                  %d\n", __FUNCTION__, DecodeContext->MMEBuffers[0].TotalSize);
    CODEC_TRACE("%s: Page_p[0]                     %p\n", __FUNCTION__, DecodeContext->MMEPages[0].Page_p);
    CODEC_TRACE("%s: TotalSize[1]                  %d\n", __FUNCTION__, DecodeContext->MMEBuffers[1].TotalSize);
    CODEC_TRACE("%s: Page_p[1]                     %p\n", __FUNCTION__, DecodeContext->MMEPages[1].Page_p);

    return CodecNoError;
}
//}}}  
