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

Source file name : codec_mme_audio_lpcm.cpp
Author :           Sylvain Barge

Implementation of the lpcm audio codec class for player 2.


Date        Modification                                    Name
----        ------------                                    --------
18-Jul-07   Created                                         Sylvain Barge

************************************************************************/


// /////////////////////////////////////////////////////////////////////
//
//      Include any component headers

#define CODEC_TAG "LPCM audio codec"
#include "codec_mme_audio_lpcm.h"
#include "lpcm.h"

// /////////////////////////////////////////////////////////////////////////
//
// Locally defined constants
//

const char LpcmModeLut[] = 
{
  ACC_LPCM_VIDEO,
  ACC_LPCM_AUDIO,
  ACC_LPCM_HD,
  ACC_LPCM_BD
};

const LpcmAudioStreamParameters_t  DefaultStreamParameters =
{
  TypeLpcmDVDVideo,
  ACC_MME_TRUE,
  ACC_MME_TRUE,
  LpcmWordSize16,
  LpcmWordSizeNone,
  LpcmSamplingFreq48,
  LpcmSamplingFreqNone,
  8,
  0,
  LPCM_DEFAULT_CHANNEL_ASSIGNMENT,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0
};

// this array is based on the LpcmSamplingFreq_t enum
static const int LpcmDVDSamplingFreq[LpcmSamplingFreqNone] = 
{
  48000, 96000, 192000, 0, 0, 0, 0, 0, 44100, 88200, 176400
};

// /////////////////////////////////////////////////////////////////////////
//
// Locally defined structures
//

typedef struct LpcmAudioCodecStreamParameterContext_s
{
    CodecBaseStreamParameterContext_t   BaseContext;

    MME_LxAudioDecoderGlobalParams_t StreamParameters;
} LpcmAudioCodecStreamParameterContext_t;

//#if __KERNEL__
#if 0
#define BUFFER_LPCM_AUDIO_CODEC_STREAM_PARAMETER_CONTEXT                "LpcmAudioCodecStreamParameterContext"
#define BUFFER_LPCM_AUDIO_CODEC_STREAM_PARAMETER_CONTEXT_TYPE   {BUFFER_LPCM_AUDIO_CODEC_STREAM_PARAMETER_CONTEXT, BufferDataTypeBase, AllocateFromDeviceMemory, 32, 0, true, true, sizeof(LpcmAudioCodecStreamParameterContext_t)}
#else
#define BUFFER_LPCM_AUDIO_CODEC_STREAM_PARAMETER_CONTEXT                "LpcmAudioCodecStreamParameterContext"
#define BUFFER_LPCM_AUDIO_CODEC_STREAM_PARAMETER_CONTEXT_TYPE   {BUFFER_LPCM_AUDIO_CODEC_STREAM_PARAMETER_CONTEXT, BufferDataTypeBase, AllocateFromOSMemory, 32, 0, true, true, sizeof(LpcmAudioCodecStreamParameterContext_t)}
#endif

static BufferDataDescriptor_t            LpcmAudioCodecStreamParameterContextDescriptor = BUFFER_LPCM_AUDIO_CODEC_STREAM_PARAMETER_CONTEXT_TYPE;

// --------

typedef struct LpcmAudioCodecDecodeContext_s
{
    CodecBaseDecodeContext_t            BaseContext;

    MME_LxAudioDecoderFrameParams_t     DecodeParameters;
    MME_LxAudioDecoderFrameStatus_t     DecodeStatus;
} LpcmAudioCodecDecodeContext_t;

//#if __KERNEL__
#if 0
#define BUFFER_LPCM_AUDIO_CODEC_DECODE_CONTEXT  "LpcmAudioCodecDecodeContext"
#define BUFFER_LPCM_AUDIO_CODEC_DECODE_CONTEXT_TYPE     {BUFFER_LPCM_AUDIO_CODEC_DECODE_CONTEXT, BufferDataTypeBase, AllocateFromDeviceMemory, 32, 0, true, true, sizeof(LpcmAudioCodecDecodeContext_t)}
#else
#define BUFFER_LPCM_AUDIO_CODEC_DECODE_CONTEXT  "LpcmAudioCodecDecodeContext"
#define BUFFER_LPCM_AUDIO_CODEC_DECODE_CONTEXT_TYPE     {BUFFER_LPCM_AUDIO_CODEC_DECODE_CONTEXT, BufferDataTypeBase, AllocateFromOSMemory, 32, 0, true, true, sizeof(LpcmAudioCodecDecodeContext_t)}
#endif

static BufferDataDescriptor_t            LpcmAudioCodecDecodeContextDescriptor = BUFFER_LPCM_AUDIO_CODEC_DECODE_CONTEXT_TYPE;


static const char *LookupLpcmStreamType(LpcmStreamType_t t)
{
	switch(t) {
#define C(x) case TypeLpcm ## x: return #x
	C(DVDVideo);
	C(DVDAudio);
	C(DVDHD);
	C(DVDBD);
	C(SPDIFIN);
#undef C
	default:
		return "INVALID";
	}
}


////////////////////////////////////////////////////////////////////////////
///
/// The LPCM audio codec proxy.
///
/// Fill in the configuration parameters used by the super-class and reset everything.
///
/// \todo Set the audio capability mask...
///
Codec_MmeAudioLpcm_c::Codec_MmeAudioLpcm_c( void )
{
    Configuration.CodecName                             = "LPCM audio";

    Configuration.StreamParameterContextCount           = 1;
    Configuration.StreamParameterContextDescriptor      = &LpcmAudioCodecStreamParameterContextDescriptor;

    Configuration.DecodeContextCount                    = 4;
    Configuration.DecodeContextDescriptor               = &LpcmAudioCodecDecodeContextDescriptor;

//

    DecoderId = ACC_LPCM_ID;

    Reset();
}


////////////////////////////////////////////////////////////////////////////
///
///     Destructor function, ensures a full halt and reset 
///     are executed for all levels of the class.
///
Codec_MmeAudioLpcm_c::~Codec_MmeAudioLpcm_c( void )
{
    Halt();
    Reset();
}

////////////////////////////////////////////////////////////////////////////
///
/// Populate the supplied structure with parameters for LPCM audio.
///
CodecStatus_t Codec_MmeAudioLpcm_c::FillOutTransformerGlobalParameters( MME_LxAudioDecoderGlobalParams_t *GlobalParams_p )
{
  CodecStatus_t Status;

//

  LpcmAudioStreamParameters_t    *Parsed;
  MME_LxAudioDecoderGlobalParams_t &GlobalParams = *GlobalParams_p;
  GlobalParams.StructSize = sizeof(MME_LxAudioDecoderGlobalParams_t);

  if (ParsedFrameParameters == NULL)
    {
      // At transformer init, stream properties might be unknown...
      Parsed = (LpcmAudioStreamParameters_t*) &DefaultStreamParameters;
    }
  else
    {
      Parsed = (LpcmAudioStreamParameters_t *)ParsedFrameParameters->StreamParameterStructure;
    }

  CODEC_TRACE("Init LPCM Decoder (as %s)\n", LookupLpcmStreamType(Parsed->Type));

  
//

  MME_LxLpcmConfig_t &Config = *((MME_LxLpcmConfig_t *) GlobalParams.DecConfig);
  Config.DecoderId = ACC_LPCM_ID;
  Config.StructSize = sizeof(MME_LxLpcmConfig_t);
  Config.Config[LPCM_MODE] = LpcmModeLut[Parsed->Type];
  Config.Config[LPCM_DRC_CODE] = Parsed->DrcCode;
  bool DrcEnable;
  if ((Parsed->Type == TypeLpcmDVDBD) || (Parsed->DrcCode == LPCM_DRC_VALUE_DISABLE))
  {
    DrcEnable = ACC_MME_FALSE;
  }
  else
  {
    DrcEnable = ACC_MME_TRUE;
  }
  Config.Config[LPCM_DRC_ENABLE] = DrcEnable;
  Config.Config[LPCM_MUTE_FLAG] = Parsed->MuteFlag;
  Config.Config[LPCM_EMPHASIS_FLAG] = Parsed->EmphasisFlag;
  Config.Config[LPCM_NB_CHANNELS] = Parsed->NumberOfChannels;
  Config.Config[LPCM_WS_CH_GR1] = Parsed->WordSize1;
  Config.Config[LPCM_WS_CH_GR2] = (Parsed->WordSize2 == LpcmWordSizeNone)?LPCM_DVD_AUDIO_NO_CH_GR2:Parsed->WordSize2;
  Config.Config[LPCM_FS_CH_GR1] = Parsed->SamplingFrequency1;
  Config.Config[LPCM_FS_CH_GR2] = (Parsed->SamplingFrequency2 == LpcmSamplingFreqNone)?LPCM_DVD_AUDIO_NO_CH_GR2:Parsed->SamplingFrequency2;
  Config.Config[LPCM_BIT_SHIFT_CH_GR2] = Parsed->BitShiftChannel2;
  Config.Config[LPCM_CHANNEL_ASSIGNMENT] = Parsed->ChannelAssignment;
  Config.Config[LPCM_MIXING_PHASE] = 0;
  Config.Config[LPCM_NB_ACCESS_UNITS] = Parsed->NbAccessUnits;
  // force resampling according to the manifestor target sampling frequency
  unsigned char Resampling = ACC_LPCM_AUTO_RSPL;
  if ( Parsed->Type != TypeLpcmDVDAudio )
    {
      unsigned int StreamSamplingFreq = LpcmDVDSamplingFreq[Parsed->SamplingFrequency1];
      if ( (StreamSamplingFreq == 48000) && 
	   (AudioOutputSurface->SampleRateHz == 96000) )
	{
	  Resampling = ACC_LPCM_RSPL_48;
	}
      else if ( (StreamSamplingFreq == 96000) && 
		(AudioOutputSurface->SampleRateHz == 48000) )
	{
	  Resampling = ACC_LPCM_RSPL_96;	  
	}
    }

  Config.Config[LPCM_OUT_RESAMPLING] = Resampling;
  Config.Config[LPCM_NB_SAMPLES] = Parsed->NumberOfSamples;
  
  //

  Status = Codec_MmeAudio_c::FillOutTransformerGlobalParameters( GlobalParams_p );
  if( Status != CodecNoError )
    {
      return Status;
    }
  
  //
  
  unsigned char *PcmParams_p = ((unsigned char *) &Config) + Config.StructSize;
  MME_LxPcmProcessingGlobalParams_Subset_t &PcmParams =
    *((MME_LxPcmProcessingGlobalParams_Subset_t *) PcmParams_p);
  
  // downmix must be disabled for LPCM
  MME_DMixGlobalParams_t DMix = PcmParams.DMix;
  DMix.Apply = ACC_MME_DISABLED;
  
  return CodecNoError;
}


////////////////////////////////////////////////////////////////////////////
///
/// Populate the AUDIO_DECODER's initialization parameters for LPCM audio.
///
/// When this method completes Codec_MmeAudio_c::AudioDecoderInitializationParameters
/// will have been filled out with valid values sufficient to initialize an
/// LPCM audio decoder.
///
CodecStatus_t   Codec_MmeAudioLpcm_c::FillOutTransformerInitializationParameters( void )
{
CodecStatus_t Status;
MME_LxAudioDecoderInitParams_t &Params = AudioDecoderInitializationParameters;

//

    MMEInitializationParameters.TransformerInitParamsSize = sizeof(Params);
    MMEInitializationParameters.TransformerInitParams_p = &Params;

//

    Status = Codec_MmeAudio_c::FillOutTransformerInitializationParameters();
    if (Status != CodecNoError)
	return Status;

//

    return FillOutTransformerGlobalParameters( &Params.GlobalParams );
}


////////////////////////////////////////////////////////////////////////////
///
/// Populate the AUDIO_DECODER's MME_SET_GLOBAL_TRANSFORMER_PARAMS parameters for LPCM audio.
///
CodecStatus_t   Codec_MmeAudioLpcm_c::FillOutSetStreamParametersCommand( void )
{
CodecStatus_t Status;
//LpcmAudioStreamParameters_t *Parsed = (LpcmAudioStreamParameters_t *)ParsedFrameParameters->StreamParameterStructure;
LpcmAudioCodecStreamParameterContext_t  *Context = (LpcmAudioCodecStreamParameterContext_t *)StreamParameterContext;

    //
    // Examine the parsed stream parameters and determine what type of codec to instanciate
    //

    DecoderId = ACC_LPCM_ID;

    //
    // Now fill out the actual structure
    //     

    memset( &(Context->StreamParameters), 0, sizeof(Context->StreamParameters) );
    Status = FillOutTransformerGlobalParameters( &(Context->StreamParameters) );
    if( Status != CodecNoError )
	return Status;

    //
    // Fillout the actual command
    //

    Context->BaseContext.MMECommand.CmdStatus.AdditionalInfoSize        = 0;
    Context->BaseContext.MMECommand.CmdStatus.AdditionalInfo_p          = NULL;
    Context->BaseContext.MMECommand.ParamSize                           = sizeof(Context->StreamParameters);
    Context->BaseContext.MMECommand.Param_p                             = (MME_GenericParams_t)(&Context->StreamParameters);

//

    return CodecNoError;
}

////////////////////////////////////////////////////////////////////////////
///
/// Populate the AUDIO_DECODER's MME_TRANSFORM parameters for LPCM audio.
///
CodecStatus_t   Codec_MmeAudioLpcm_c::FillOutDecodeCommand(       void )
{
LpcmAudioCodecDecodeContext_t   *Context        = (LpcmAudioCodecDecodeContext_t *)DecodeContext;
LpcmAudioFrameParameters_t    *Parsed         = (LpcmAudioFrameParameters_t *)ParsedFrameParameters->FrameParameterStructure;

    //
    // Initialize the frame parameters
    //

    memset( &Context->DecodeParameters, 0, sizeof(Context->DecodeParameters) );

    Context->DecodeParameters.FrameParams[ACC_LPCM_FRAME_PARAMS_NBSAMPLES_INDEX] = Parsed->NumberOfSamples;
    Context->DecodeParameters.FrameParams[ACC_LPCM_FRAME_PARAMS_DRC_INDEX] = Parsed->DrcCode;

    //
    // Zero the reply structure
    //

    memset( &Context->DecodeStatus, 0, sizeof(Context->DecodeStatus) );

    //
    // Fillout the actual command
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
CodecStatus_t   Codec_MmeAudioLpcm_c::ValidateDecodeContext( CodecBaseDecodeContext_t *Context )
{
LpcmAudioCodecDecodeContext_t *DecodeContext = (LpcmAudioCodecDecodeContext_t *) Context;
MME_LxAudioDecoderFrameStatus_t &Status = DecodeContext->DecodeStatus;
ParsedAudioParameters_t *AudioParameters;


    CODEC_DEBUG(">><<\n");

    if (ENABLE_CODEC_DEBUG) 
      {
	//DumpCommand(bufferIndex);
      }

    if (Status.DecStatus) 
      {
	CODEC_ERROR("LPCM audio decode error (muted frame): %d\n", Status.DecStatus);
	//DumpCommand(bufferIndex);
	// don't report an error to the higher levels (because the frame is muted)
      }

    // SYSFS
    AudioDecoderStatus = Status;

    //
    // Attach any codec derived metadata to the output buffer (or verify the
    // frame analysis if the frame analyser already filled everything in for
    // us).
    //

    AudioParameters = BufferState[DecodeContext->BaseContext.BufferIndex].ParsedAudioParameters;

    // TODO: these values should be extracted from the codec's reply
    AudioParameters->Source.BitsPerSample = /* AudioOutputSurface->BitsPerSample;*/ ((AUDIODEC_GET_OUTPUT_WS((&AudioDecoderInitializationParameters))== ACC_WS32)?32:16);
    AudioParameters->Source.ChannelCount =  AudioOutputSurface->ChannelCount;/*Codec_MmeAudio_c::GetNumberOfChannelsFromAudioConfiguration((enum eAccAcMode) Status.AudioMode);*/
    AudioParameters->Organisation = Status.AudioMode;

    // lpcm can be resampled directly in the codec to avoid having them resampled by the mixer
    // so get the sampling frequency from the codec
    AudioParameters->SampleCount = Status.NbOutSamples;

    enum eAccFsCode SamplingFreqCode = (enum eAccFsCode) Status.SamplingFreq;
    if (SamplingFreqCode < ACC_FS_reserved)
      {
	AudioParameters->Source.SampleRateHz = Codec_MmeAudio_c::ConvertCodecSamplingFreq(SamplingFreqCode);
      }
    else
      {
	AudioParameters->Source.SampleRateHz = 0;
        CODEC_ERROR("LPCM audio decode bad sampling freq returned: 0x%x\n", SamplingFreqCode);
      }

    return CodecNoError;
}

// /////////////////////////////////////////////////////////////////////////
//
//      Function to dump out the set stream 
//      parameters from an mme command.
//

CodecStatus_t   Codec_MmeAudioLpcm_c::DumpSetStreamParameters(          void    *Parameters )
{
    CODEC_ERROR("Not implemented\n");  
    return CodecNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Function to dump out the decode
//      parameters from an mme command.
//

CodecStatus_t   Codec_MmeAudioLpcm_c::DumpDecodeParameters(             void    *Parameters )
{
    CODEC_ERROR("Not implemented\n");  
    return CodecNoError;
}
