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

Source file name : codec_mme_audio_eac3.cpp
Author :           Sylvain Barge

Implementation of the eac3 audio codec class for player 2.


Date        Modification                                    Name
----        ------------                                    --------
24-May-07   Created (from codec_mme_audio_ac3.cpp)        Sylvain Barge

************************************************************************/

////////////////////////////////////////////////////////////////////////////
/// \class Codec_MmeAudioEAc3_c
///
/// The EAC3 audio codec proxy.
///

// /////////////////////////////////////////////////////////////////////
//
//      Include any component headers

#define CODEC_TAG "EAC3 audio codec"
#include "codec_mme_audio_eac3.h"
#include "eac3_audio.h"
#include "player_generic.h"

// /////////////////////////////////////////////////////////////////////////
//
// Locally defined constants
//
#define EAC3_TRANSCODE_STATUS_OK 2  // no error while transcoding
#define EAC3_DECODE_STATUS_OK    0  // no error while decoding
#define EAC3_SYNC_STATUS_OK      2  // sync status: still synchronized (no change in sync)
#define EAC3_SYNC_STATUS_2_OK    4  // sync status: just synchronized
#define EAC3_TRANSCODING_STATUS_OK   ((EAC3_DECODE_STATUS_OK << 16) | (EAC3_TRANSCODE_STATUS_OK << 8) | EAC3_SYNC_STATUS_OK)
#define EAC3_TRANSCODING_STATUS_2_OK ((EAC3_DECODE_STATUS_OK << 16) | (EAC3_TRANSCODE_STATUS_OK << 8) | EAC3_SYNC_STATUS_2_OK)
#define EAC3_TRANSCODING_DEC_STATUS_SHIFT  16

// /////////////////////////////////////////////////////////////////////////
//
// Locally defined structures
//

typedef struct EAc3AudioCodecStreamParameterContext_s
{
    CodecBaseStreamParameterContext_t   BaseContext;

    MME_LxAudioDecoderGlobalParams_t StreamParameters;
} EAc3AudioCodecStreamParameterContext_t;

//#if __KERNEL__
#if 0
#define BUFFER_EAC3_AUDIO_CODEC_STREAM_PARAMETER_CONTEXT                "EAc3AudioCodecStreamParameterContext"
#define BUFFER_EAC3_AUDIO_CODEC_STREAM_PARAMETER_CONTEXT_TYPE   {BUFFER_EAC3_AUDIO_CODEC_STREAM_PARAMETER_CONTEXT, BufferDataTypeBase, AllocateFromDeviceMemory, 32, 0, true, true, sizeof(EAc3AudioCodecStreamParameterContext_t)}
#else
#define BUFFER_EAC3_AUDIO_CODEC_STREAM_PARAMETER_CONTEXT                "EAc3AudioCodecStreamParameterContext"
#define BUFFER_EAC3_AUDIO_CODEC_STREAM_PARAMETER_CONTEXT_TYPE   {BUFFER_EAC3_AUDIO_CODEC_STREAM_PARAMETER_CONTEXT, BufferDataTypeBase, AllocateFromOSMemory, 32, 0, true, true, sizeof(EAc3AudioCodecStreamParameterContext_t)}
#endif

static BufferDataDescriptor_t            EAc3AudioCodecStreamParameterContextDescriptor = BUFFER_EAC3_AUDIO_CODEC_STREAM_PARAMETER_CONTEXT_TYPE;

// --------

typedef struct
{
	U32                                    BytesUsed;  // Amount of this structure already filled
	MME_LxAudioDecoderMixingMetadata_t  MixingMetadata;
} MME_MixMetadataAc3FrameStatus_t;

typedef struct
{
	MME_LxAudioDecoderFrameStatus_t  DecStatus;  
	MME_MixMetadataAc3FrameStatus_t  PcmStatus;
} MME_LxAudioDecoderAc3FrameMixMetadataStatus_t;

typedef struct EAc3AudioCodecDecodeContext_s
{
    CodecBaseDecodeContext_t            BaseContext;

    MME_LxAudioDecoderFrameParams_t                DecodeParameters;
    MME_LxAudioDecoderAc3FrameMixMetadataStatus_t  DecodeStatus;
    unsigned int                        TranscodeBufferIndex;
} EAc3AudioCodecDecodeContext_t;

#define EAC3_MIN_MIXING_METADATA_SIZE       (sizeof(MME_LxAudioDecoderMixingMetadata_t) - sizeof(MME_MixingOutputConfiguration_t))
#define EAC3_MIN_MIXING_METADATA_FIXED_SIZE (EAC3_MIN_MIXING_METADATA_SIZE - (2 * sizeof(U32))) // same as above minus Id and StructSize

//#if __KERNEL__
#if 0
#define BUFFER_EAC3_AUDIO_CODEC_DECODE_CONTEXT  "EAc3AudioCodecDecodeContext"
#define BUFFER_EAC3_AUDIO_CODEC_DECODE_CONTEXT_TYPE     {BUFFER_EAC3_AUDIO_CODEC_DECODE_CONTEXT, BufferDataTypeBase, AllocateFromDeviceMemory, 32, 0, true, true, sizeof(EAc3AudioCodecDecodeContext_t)}
#else
#define BUFFER_EAC3_AUDIO_CODEC_DECODE_CONTEXT  "EAc3AudioCodecDecodeContext"
#define BUFFER_EAC3_AUDIO_CODEC_DECODE_CONTEXT_TYPE     {BUFFER_EAC3_AUDIO_CODEC_DECODE_CONTEXT, BufferDataTypeBase, AllocateFromOSMemory, 32, 0, true, true, sizeof(EAc3AudioCodecDecodeContext_t)}
#endif

static BufferDataDescriptor_t            EAc3AudioCodecDecodeContextDescriptor = BUFFER_EAC3_AUDIO_CODEC_DECODE_CONTEXT_TYPE;


#define BUFFER_TRANSCODED_FRAME_BUFFER        "TranscodedFrameBuffer"
#define BUFFER_TRANSCODED_FRAME_BUFFER_TYPE   {BUFFER_TRANSCODED_FRAME_BUFFER, BufferDataTypeBase, AllocateFromSuppliedBlock, 4, 64, false, false, 0}

static BufferDataDescriptor_t            InitialTranscodedFrameBufferDescriptor = BUFFER_TRANSCODED_FRAME_BUFFER_TYPE;

////////////////////////////////////////////////////////////////////////////
///
/// Fill in the configuration parameters used by the super-class and reset everything.
///
Codec_MmeAudioEAc3_c::Codec_MmeAudioEAc3_c( void )
{
    CodecStatus_t Status;

    Configuration.CodecName                             = "EAC3 audio";

    Configuration.StreamParameterContextCount           = 1;
    Configuration.StreamParameterContextDescriptor      = &EAc3AudioCodecStreamParameterContextDescriptor;

    Configuration.DecodeContextCount                    = 4;
    Configuration.DecodeContextDescriptor               = &EAc3AudioCodecDecodeContextDescriptor;

//

    AudioDecoderTransformCapabilityMask.DecoderCapabilityFlags = (1 << ACC_AC3);

    DecoderId                                           = ACC_DDPLUS_ID;

    CurrentTranscodeBufferIndex = 0;
    
    TranscodedFramePool = NULL;
    
    TranscodedFrameMemory[CachedAddress]      = NULL;
    TranscodedFrameMemory[UnCachedAddress]    = NULL;
    TranscodedFrameMemory[PhysicalAddress]    = NULL;
    
    Reset();
    
#ifdef CONFIG_CPU_SUBTYPE_STX7200
    // This is a pretty gross hack to allow the pre-startup checks to work (the ones that would have
    // us fall back the silence generator. The basic problem is that AUDIO_DECODER may refer to
    // either #3 or #4 (and may changed based on the victor of boot time races) and that, for some
    // deployments, #4 doesn't support AC3. This results in the eroneous adoption of the silence generator.
    // Our solution is to temporarily change the transformer names to allow the codec to initialize. We then
    // switch them back again in order to prevent the system breaking during a later call to
    // SetModuleParameters(CodecSpecifyTransformerPostFix...).
    strcat(TransformName[1],"3");
    strcat(TransformName[2],"4");
#endif
    Status = GloballyVerifyMMECapabilities();
    if( CodecNoError != Status )
    {
	InitializationStatus = PlayerNotSupported;
	return;
    }
#ifdef CONFIG_CPU_SUBTYPE_STX7200
    strcpy(TransformName[1], AUDIO_DECODER_TRANSFORMER_NAME);
    strcpy(TransformName[2], AUDIO_DECODER_TRANSFORMER_NAME);
#endif

    isFwEac3Capable = (((AudioDecoderTransformCapability.DecoderCapabilityExtFlags[0]) >> (4 * ACC_AC3)) & (1 << ACC_DOLBY_DIGITAL_PLUS));
    CODEC_TRACE("%s\n", isFwEac3Capable ?
			"Using extended AC3 decoder (DD+ streams will be correctly decoded)" :
			"Using standard AC3 decoder (DD+ streams will be unplayable)");
}

////////////////////////////////////////////////////////////////////////////
///
/// Reset method to manage the transcoded frame pool
///
CodecStatus_t Codec_MmeAudioEAc3_c::Reset( void )
{
    //
    // Release the coded frame buffer pool
    //
    
    if( TranscodedFramePool != NULL )
    {
        BufferManager->DestroyPool( TranscodedFramePool );
        TranscodedFramePool  = NULL;
    }
    
    if( TranscodedFrameMemory[CachedAddress] != NULL )
    {
#if __KERNEL__
        AllocatorClose( TranscodedFrameMemoryDevice );
#endif
	
        TranscodedFrameMemory[CachedAddress]      = NULL;
        TranscodedFrameMemory[UnCachedAddress]    = NULL;
        TranscodedFrameMemory[PhysicalAddress]    = NULL;
    }
    
    //	PreviousTranscodeBuffer = NULL;
    CurrentTranscodeBuffer = NULL;

    TranscodeEnable = false;
    
    //!     
    return Codec_MmeAudio_c::Reset();
}


////////////////////////////////////////////////////////////////////////////
///
///     Destructor function, ensures a full halt and reset 
///     are executed for all levels of the class.
///
Codec_MmeAudioEAc3_c::~Codec_MmeAudioEAc3_c( void )
{
    Halt();
    Reset();
}


////////////////////////////////////////////////////////////////////////////
///
/// Populate the supplied structure with parameters for EAC3 audio.
///
CodecStatus_t Codec_MmeAudioEAc3_c::FillOutTransformerGlobalParameters( MME_LxAudioDecoderGlobalParams_t *GlobalParams_p )
{
CodecStatus_t Status;

//

    MME_LxAudioDecoderGlobalParams_t &GlobalParams = *GlobalParams_p;
    GlobalParams.StructSize = sizeof(MME_LxAudioDecoderGlobalParams_t);

//

    MME_LxDDPConfig_t &Config = *((MME_LxDDPConfig_t *) GlobalParams.DecConfig);
    Config.DecoderId = isFwEac3Capable ? ACC_DDPLUS_ID : ACC_AC3_ID;
    Config.StructSize = sizeof(MME_LxDDPConfig_t);
    Config.Config[DD_CRC_ENABLE] = ACC_MME_TRUE;
    Config.Config[DD_LFE_ENABLE] = ACC_MME_TRUE;
    Config.Config[DD_COMPRESS_MODE] = (DRC.Enable) ? DRC.Type : DD_LINE_OUT ;
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
		bool EnableLfeDMix = true;

		// Dolby require that LFE downmix is disabled in RF mode (certification requirement)
		if (DD_RF_MODE == Config.Config[DD_COMPRESS_MODE])
			EnableLfeDMix = false;

		ddp.ddp_output_settings         = 0; // reset all controls.
		ddp.ddp_features.Upsample       = 2; // never upsample;
		ddp.ddp_features.AC3Endieness   = 1; // LittleEndian
		ddp.ddp_features.DitherAlgo     = 0; // set to 0 for correct transcoding
		ddp.ddp_features.DisableLfeDmix = (DRC.Type == DD_RF_MODE) ? ACC_MME_TRUE : ACC_MME_FALSE;

		Config.Config[DDP_OUTPUT_SETTING] = ddp.ddp_output_settings;

    }

    Config.PcmScale = 0x7fff;

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

    // downmix must be disabled for EAC3
    MME_DMixGlobalParams_t DMix = PcmParams.DMix;
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
CodecStatus_t   Codec_MmeAudioEAc3_c::FillOutTransformerInitializationParameters( void )
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
/// Populate the AUDIO_DECODER's MME_SET_GLOBAL_TRANSFORMER_PARAMS parameters for EAC3 audio.
///
CodecStatus_t   Codec_MmeAudioEAc3_c::FillOutSetStreamParametersCommand( void )
{
CodecStatus_t Status;
//EAc3AudioStreamParameters_t *Parsed = (EAc3AudioStreamParameters_t *)ParsedFrameParameters->StreamParameterStructure;
EAc3AudioCodecStreamParameterContext_t  *Context = (EAc3AudioCodecStreamParameterContext_t *)StreamParameterContext;

    // if the stream is dd+, then the transcoding is required
    TranscodeEnable = (ParsedAudioParameters->OriginalEncoding == AudioOriginalEncodingDdplus);
    
    //
    // Examine the parsed stream parameters and determine what type of codec to instanciate
    //

    DecoderId = ACC_DDPLUS_ID;

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
/// Populate the AUDIO_DECODER's MME_TRANSFORM parameters for MPEG audio.
///
CodecStatus_t   Codec_MmeAudioEAc3_c::FillOutDecodeCommand(       void )
{
EAc3AudioCodecDecodeContext_t   *Context        = (EAc3AudioCodecDecodeContext_t *)DecodeContext;
//EAc3AudioFrameParameters_t    *Parsed         = (EAc3AudioFrameParameters_t *)ParsedFrameParameters->FrameParameterStructure;

    //
    // Initialize the frame parameters (we don't actually have much to say here)
    //

    memset( &Context->DecodeParameters, 0, sizeof(Context->DecodeParameters) );

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
CodecStatus_t   Codec_MmeAudioEAc3_c::ValidateDecodeContext( CodecBaseDecodeContext_t *Context )
{
EAc3AudioCodecDecodeContext_t *DecodeContext = (EAc3AudioCodecDecodeContext_t *) Context;
MME_LxAudioDecoderFrameStatus_t &Status      = DecodeContext->DecodeStatus.DecStatus;
ParsedAudioParameters_t *AudioParameters;
int TranscodedBufferSize = 0;
bool decode_error = false;

    CODEC_DEBUG(">><<\n");

    if (ENABLE_CODEC_DEBUG) 
	{
		//DumpCommand(bufferIndex);
    }

    if ( (Status.DecStatus != ACC_DDPLUS_OK) && ((Status.DecStatus >> EAC3_TRANSCODING_DEC_STATUS_SHIFT) != ACC_DDPLUS_OK) )
    {    
        CODEC_ERROR("Decode error (muted frame): 0x%x\n", Status.DecStatus);
        //DumpCommand(bufferIndex);
        // don't report an error to the higher levels (because the frame is muted)
        decode_error = true;
    }

    // if transcoding is required, check the transcoded buffer size...
    if (TranscodeEnable)
    {
        MME_Command_t * Cmd = &DecodeContext->BaseContext.MMECommand;
        TranscodedBufferSize = Cmd->DataBuffers_p[EAC3_TRANSCODE_SCATTER_PAGE_INDEX]->ScatterPages_p[0].BytesUsed;
        
        if ((TranscodedBufferSize == 0) || (TranscodedBufferSize > EAC3_AC3_MAX_FRAME_SIZE))
        {
            CODEC_ERROR("Erroneous transcoded buffer size: %d\n", TranscodedBufferSize);
        }
    }

    if (Status.NbOutSamples != EAC3_NBSAMPLES_NEEDED) 
	{ // TODO: manifest constant
		CODEC_ERROR("Unexpected number of output samples (%d)\n", Status.NbOutSamples);
		//DumpCommand(bufferIndex);
    }

    // SYSFS
    AudioDecoderStatus = Status;

    //
    // Attach any codec derived metadata to the output buffer (or verify the
    // frame analysis if the frame analyser already filled everything in for
    // us).
    //

    AudioParameters = BufferState[DecodeContext->BaseContext.BufferIndex].ParsedAudioParameters;

    AudioParameters->Source.BitsPerSample = AudioOutputSurface->BitsPerSample;
    AudioParameters->Source.ChannelCount = AudioOutputSurface->ChannelCount;
    AudioParameters->Organisation = Status.AudioMode;

    AudioParameters->SampleCount = Status.NbOutSamples;
    AudioParameters->decErrorStatus = decode_error;

    int SamplingFreqCode = Status.SamplingFreq;

    if (SamplingFreqCode < ACC_FS_reserved)
    {
        AudioParameters->Source.SampleRateHz = ACC_SamplingFreqLUT[SamplingFreqCode];
    }
    else
    {
        AudioParameters->Source.SampleRateHz = 0;
        CODEC_ERROR("Decode bad sampling freq returned: 0x%x\n", SamplingFreqCode);
    }

    // Fill the parsed parameters with the AC3 stream metadata
    Codec_MmeAudioEAc3_c::FillStreamMetadata(AudioParameters, (MME_LxAudioDecoderFrameStatus_t*)&Status);
    
    // Validate the extended status (without propagating errors)
    (void) ValidatePcmProcessingExtendedStatus(Context,
	    (MME_PcmProcessingFrameExtStatus_t *) &DecodeContext->DecodeStatus.PcmStatus);

    if (TranscodeEnable)
    {
        TranscodedBuffers[DecodeContext->TranscodeBufferIndex].Buffer->SetUsedDataSize(TranscodedBufferSize);
    }		

    return CodecNoError;
}


////////////////////////////////////////////////////////////////////////////
///
/// Extract mixing metadata and stuff it into the audio parameters.
///
/// \todo Can we make this code common between EAC3 and DTSHD handling.
///
void Codec_MmeAudioEAc3_c::HandleMixingMetadata( CodecBaseDecodeContext_t *Context,
	                                          MME_PcmProcessingStatusTemplate_t *PcmStatus )
{
    ParsedAudioParameters_t *AudioParameters = BufferState[Context->BufferIndex].ParsedAudioParameters;
    MME_LxAudioDecoderMixingMetadata_t *MixingMetadata = (MME_LxAudioDecoderMixingMetadata_t *) PcmStatus;
    int NbMixConfigurations;
    
    //
    // Validation
    //
    
    CODEC_ASSERT( MixingMetadata->MinStruct.Id == ACC_MIX_METADATA_ID ); // already checked by framework
    if( MixingMetadata->MinStruct.StructSize < sizeof(MixingMetadata->MinStruct) )
    {
	CODEC_ERROR("Mixing metadata is too small (%d)\n", MixingMetadata->MinStruct.StructSize);
	return;
    }

    NbMixConfigurations = MixingMetadata->MinStruct.NbOutMixConfig;
    if( NbMixConfigurations > MAX_MIXING_OUTPUT_CONFIGURATION )
    {
        CODEC_TRACE("Number of mix out configs is gt 3 (%d)!\n", NbMixConfigurations);
        NbMixConfigurations = MAX_MIXING_OUTPUT_CONFIGURATION;
    }

    //
    // Action
    //
    
    memset(&AudioParameters->MixingMetadata, 0, sizeof(AudioParameters->MixingMetadata));

    AudioParameters->MixingMetadata.IsMixingMetadataPresent = true;
    
    AudioParameters->MixingMetadata.PostMixGain = MixingMetadata->MinStruct.PostMixGain;
    AudioParameters->MixingMetadata.NbOutMixConfig = MixingMetadata->MinStruct.NbOutMixConfig;

    for (int i=0; i<NbMixConfigurations; i++) {
        MME_MixingOutputConfiguration_t &In  = MixingMetadata->MixOutConfig[i];
	MixingOutputConfiguration_t &Out = AudioParameters->MixingMetadata.MixOutConfig[i];
        
	Out.AudioMode = In.AudioMode;
        
        for (int j=0; j<MAX_NB_CHANNEL_COEFF; j++) {
	    Out.PrimaryAudioGain[j] = In.PrimaryAudioGain[j];
	    Out.SecondaryAudioPanCoeff[j] = In.SecondaryAudioPanCoeff[j];
	}
    }
}

// /////////////////////////////////////////////////////////////////////////
//
//      Function to dump out the set stream 
//      parameters from an mme command.
//

CodecStatus_t   Codec_MmeAudioEAc3_c::DumpSetStreamParameters(          void    *Parameters )
{
    CODEC_ERROR("Not implemented\n");  
    return CodecNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Function to dump out the decode
//      parameters from an mme command.
//

CodecStatus_t   Codec_MmeAudioEAc3_c::DumpDecodeParameters(             void    *Parameters )
{
    CODEC_ERROR("Not implemented\n");  
    return CodecNoError;
}

////////////////////////////////////////////////////////////////////////////
///
///  Set Default FrameBase style TRANSFORM command IOs
///  Populate DecodeContext with 1 Input and 2 output Buffers
///  Populate I/O MME_DataBuffers
void Codec_MmeAudioEAc3_c::PresetIOBuffers( void )
{
    Codec_MmeAudio_c::PresetIOBuffers();
    
    if (TranscodeEnable)
    {
        // plumbing
        DecodeContext->MMEBufferList[EAC3_TRANSCODE_SCATTER_PAGE_INDEX] = &DecodeContext->MMEBuffers[EAC3_TRANSCODE_SCATTER_PAGE_INDEX];
	
        memset( &DecodeContext->MMEBuffers[EAC3_TRANSCODE_SCATTER_PAGE_INDEX], 0, sizeof(MME_DataBuffer_t) );

        DecodeContext->MMEBuffers[EAC3_TRANSCODE_SCATTER_PAGE_INDEX].StructSize           = sizeof(MME_DataBuffer_t);
        DecodeContext->MMEBuffers[EAC3_TRANSCODE_SCATTER_PAGE_INDEX].NumberOfScatterPages = 1;
        DecodeContext->MMEBuffers[EAC3_TRANSCODE_SCATTER_PAGE_INDEX].ScatterPages_p       = &DecodeContext->MMEPages[EAC3_TRANSCODE_SCATTER_PAGE_INDEX];
	
        memset( &DecodeContext->MMEPages[EAC3_TRANSCODE_SCATTER_PAGE_INDEX], 0, sizeof(MME_ScatterPage_t) );
	
        DecodeContext->MMEBuffers[EAC3_TRANSCODE_SCATTER_PAGE_INDEX].Flags     = 0xCD; // this will trigger transcoding in the firmware....(magic number not exported in the firmware header files, sorry)
        DecodeContext->MMEBuffers[EAC3_TRANSCODE_SCATTER_PAGE_INDEX].TotalSize = TranscodedBuffers[CurrentTranscodeBufferIndex].BufferLength;
        DecodeContext->MMEPages[EAC3_TRANSCODE_SCATTER_PAGE_INDEX].Page_p      = TranscodedBuffers[CurrentTranscodeBufferIndex].BufferPointer;
        DecodeContext->MMEPages[EAC3_TRANSCODE_SCATTER_PAGE_INDEX].Size        = TranscodedBuffers[CurrentTranscodeBufferIndex].BufferLength;
    }
}

////////////////////////////////////////////////////////////////////////////
///
///  Set Default FrameBase style TRANSFORM command for AudioDecoder MT
///  with 1 Input Buffer and 2 Output Buffer.

void Codec_MmeAudioEAc3_c::SetCommandIO( void )
{	
    if (TranscodeEnable)
    {
        CodecStatus_t Status = GetTranscodeBuffer();
	
        if (Status != CodecNoError)
        {            
            CODEC_ERROR("Error while requesting Transcoded buffer: %d. Disabling transcoding...\n", Status);
            TranscodeEnable = false;
        }
        ((EAc3AudioCodecDecodeContext_t *)DecodeContext)->TranscodeBufferIndex = CurrentTranscodeBufferIndex;
    }
    
    PresetIOBuffers();
    
    Codec_MmeAudio_c::SetCommandIO();
    
    if (TranscodeEnable)
    {
        // FrameBase Transformer :: 1 Input Buffer / 2 Output Buffer sent through same MME_TRANSFORM
        DecodeContext->MMECommand.NumberOutputBuffers = 2;
    }
}


// /////////////////////////////////////////////////////////////////////////
//
//      Function to obtain a new decode buffer.
//

CodecStatus_t   Codec_MmeAudioEAc3_c::GetTranscodeBuffer( void )
{
    PlayerStatus_t           Status;
    BufferPool_t             Tfp;
    //Buffer        Structure_t        BufferStructure;
    
    //
    // Get a buffer
    //
    
    Status = GetTranscodedFrameBufferPool( &Tfp );
    
    if( Status != CodecNoError )
    {
        CODEC_ERROR("GetTranscodeBuffer(%s) - Failed to obtain the transcoded buffer pool instance.\n", Configuration.CodecName );
        return Status;
    }
    
    Status  = Tfp->GetBuffer(&CurrentTranscodeBuffer, IdentifierCodec, EAC3_FRAME_MAX_SIZE, false);
    
    if( Status != BufferNoError )
    {
        CODEC_ERROR("GetTranscodeBuffer(%s) - Failed to obtain a transcode buffer from the transcoded buffer pool.\n", Configuration.CodecName );
        Tfp->Dump(DumpPoolStates | DumpBufferStates);
        
        return Status;
    }

    //
    // Map it and initialize the mapped entry.
    //

    CurrentTranscodeBuffer->GetIndex( &CurrentTranscodeBufferIndex );

    if( CurrentTranscodeBufferIndex >= EAC3_TRANSCODE_BUFFER_COUNT )
        CODEC_ERROR("GetTranscodeBuffer(%s) - Transcode buffer index >= EAC3_TRANSCODE_BUFFER_COUNT - Implementation error.\n", Configuration.CodecName );
    
    memset( &TranscodedBuffers[CurrentTranscodeBufferIndex], 0x00, sizeof(CodecBufferState_t) );
    
    TranscodedBuffers[CurrentTranscodeBufferIndex].Buffer                        = CurrentTranscodeBuffer;
    TranscodedBuffers[CurrentTranscodeBufferIndex].OutputOnDecodesComplete       = false;
    TranscodedBuffers[CurrentTranscodeBufferIndex].DecodesInProgress             = 0;
    
    //
    // Obtain the interesting references to the buffer
    //
    
    CurrentTranscodeBuffer->ObtainDataReference( &TranscodedBuffers[CurrentTranscodeBufferIndex].BufferLength,
                                                 NULL,
                                                 (void **)(&TranscodedBuffers[CurrentTranscodeBufferIndex].BufferPointer),
                                                 Configuration.AddressingMode );
//

    return CodecNoError;
}

////////////////////////////////////////////////////////////////////////////
///
/// Attach a coded buffer to the deocded buffer
/// In case of transcoding is required, attach also
/// the transcoded buffer to the original coded buffer
///
void Codec_MmeAudioEAc3_c::AttachCodedFrameBuffer( void )
{
    Codec_MmeAudio_c::AttachCodedFrameBuffer();

    if (TranscodeEnable)
    {
        Buffer_t CodedDataBuffer;
        BufferStatus_t Status;
	
        Status = CurrentDecodeBuffer->ObtainAttachedBufferReference( CodedFrameBufferType, &CodedDataBuffer );
	
        if (Status != BufferNoError)
        {
            CODEC_ERROR("Could not get the attached coded data buffer (%d)\n", Status);
            return;
        }
        
        CodedDataBuffer->AttachBuffer( CurrentTranscodeBuffer );
        CurrentTranscodeBuffer->DecrementReferenceCount();
        // the trnascoded buffer is now only referenced by its attachement to the coded buffer
    }
}


// /////////////////////////////////////////////////////////////////////////
//
//      Function to set the stream metadata according to what is contained 
//      in the steam bitstream (returned by the codec firmware status)
//

void Codec_MmeAudioEAc3_c::FillStreamMetadata(ParsedAudioParameters_t * AudioParameters, MME_LxAudioDecoderFrameStatus_t * Status)
{
    // this code is very fatpipe dependent, so maybe not in the right place...
    StreamMetadata_t * Metadata = &AudioParameters->StreamMetadata;
    tMME_BufferFlags * Flags = (tMME_BufferFlags *) &Status->PTSflag;

    // direct mapping
    Metadata->FrontMatrixEncoded = Flags->FrontMatrixEncoded;
    Metadata->RearMatrixEncoded  = Flags->RearMatrixEncoded;
    // according to fatpipe specs
    Metadata->MixLevel  = (Flags->AudioProdie)?(Flags->MixLevel + 80):0;

    // no dialog norm processing is performed within or after the ac3 decoder, 
    // so the box will apply the value from the stream metadata
    Metadata->DialogNorm = Flags->DialogNorm;
    
    Metadata->LfeGain = 10;
}


// /////////////////////////////////////////////////////////////////////////
//
//      The get coded frame buffer pool fn
//

CodecStatus_t   Codec_MmeAudioEAc3_c::GetTranscodedFrameBufferPool( BufferPool_t * Tfp )
{
	PlayerStatus_t          Status;
#ifdef __KERNEL__
	allocator_status_t      AStatus;
#endif

    //
    // If we haven't already created the buffer pool, do it now.
    //

    if( TranscodedFramePool == NULL )
    {

        //
        // Coded frame buffer type
        //  
        Status      = InitializeDataType( &InitialTranscodedFrameBufferDescriptor, &TranscodedFrameBufferType, &TranscodedFrameBufferDescriptor );
        if( Status != PlayerNoError )
            return Status;
         
        //
        // Get the memory and Create the pool with it
        //
	
        Player->GetBufferManager( &BufferManager );
        
#if __KERNEL__
        AStatus = PartitionAllocatorOpen( &TranscodedFrameMemoryDevice, Configuration.TranscodedMemoryPartitionName, EAC3_FRAME_MAX_SIZE * EAC3_TRANSCODE_BUFFER_COUNT, true );
        if( AStatus != allocator_ok )
        {
            CODEC_ERROR("Failed to allocate memory\n", Configuration.CodecName );
            return PlayerInsufficientMemory;
        }
	
        TranscodedFrameMemory[CachedAddress]         = AllocatorUserAddress( TranscodedFrameMemoryDevice );
        TranscodedFrameMemory[UnCachedAddress]       = AllocatorUncachedUserAddress( TranscodedFrameMemoryDevice );
        TranscodedFrameMemory[PhysicalAddress]       = AllocatorPhysicalAddress( TranscodedFrameMemoryDevice );
#else
        static unsigned char    Memory[4*1024*1024];
	
        TranscodedFrameMemory[CachedAddress]         = Memory;
        TranscodedFrameMemory[UnCachedAddress]       = NULL;
        TranscodedFrameMemory[PhysicalAddress]       = Memory;
//        Configuration.CodedMemorySize           = 4*1024*1024;
#endif
        
        //
        
        Status  = BufferManager->CreatePool( &TranscodedFramePool, 
                                             TranscodedFrameBufferType, 
                                             EAC3_TRANSCODE_BUFFER_COUNT, 
                                             EAC3_FRAME_MAX_SIZE * EAC3_TRANSCODE_BUFFER_COUNT, 
                                             TranscodedFrameMemory );
        if( Status != BufferNoError )
        {
            CODEC_ERROR( "GetTranscodedFrameBufferPool(%s) - Failed to create the pool.\n", Configuration.CodecName );
            return PlayerInsufficientMemory;
        }

        ((PlayerStream_s *)Stream)->TranscodedFrameBufferType = TranscodedFrameBufferType;
    }
    
    *Tfp = TranscodedFramePool;
    
    return CodecNoError;
}
