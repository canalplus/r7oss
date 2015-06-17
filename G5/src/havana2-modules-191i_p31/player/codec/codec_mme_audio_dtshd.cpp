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

Source file name : codec_mme_audio_dtshd.cpp
Author :           Sylvain Barge

Implementation of the dtshd audio codec class for player 2.


Date        Modification                                    Name
----        ------------                                    --------
08-Jun-07   Ported from Player 1                            Sylvain Barge

************************************************************************/

////////////////////////////////////////////////////////////////////////////
/// \class Codec_MmeAudioDtshd_c
///
/// The DTSHD audio codec proxy.
///

// /////////////////////////////////////////////////////////////////////
//
//      Include any component headers

#define CODEC_TAG "DTSHD audio codec"
#include "codec_mme_audio_dtshd.h"
#include "dtshd.h"
#include "player_generic.h"

// /////////////////////////////////////////////////////////////////////////
//
// Locally defined constants
//

// /////////////////////////////////////////////////////////////////////////
//
// Locally defined structures
//

typedef struct DtshdAudioCodecStreamParameterContext_s
{
    CodecBaseStreamParameterContext_t   BaseContext;

    MME_LxAudioDecoderGlobalParams_t StreamParameters;
} DtshdAudioCodecStreamParameterContext_t;

//#if __KERNEL__
#if 0
#define BUFFER_DTSHD_AUDIO_CODEC_STREAM_PARAMETER_CONTEXT               "DtshdAudioCodecStreamParameterContext"
#define BUFFER_DTSHD_AUDIO_CODEC_STREAM_PARAMETER_CONTEXT_TYPE  {BUFFER_DTSHD_AUDIO_CODEC_STREAM_PARAMETER_CONTEXT, BufferDataTypeBase, AllocateFromDeviceMemory, 32, 0, true, true, sizeof(DtshdAudioCodecStreamParameterContext_t)}
#else
#define BUFFER_DTSHD_AUDIO_CODEC_STREAM_PARAMETER_CONTEXT               "DtshdAudioCodecStreamParameterContext"
#define BUFFER_DTSHD_AUDIO_CODEC_STREAM_PARAMETER_CONTEXT_TYPE  {BUFFER_DTSHD_AUDIO_CODEC_STREAM_PARAMETER_CONTEXT, BufferDataTypeBase, AllocateFromOSMemory, 32, 0, true, true, sizeof(DtshdAudioCodecStreamParameterContext_t)}
#endif

static BufferDataDescriptor_t            DtshdAudioCodecStreamParameterContextDescriptor = BUFFER_DTSHD_AUDIO_CODEC_STREAM_PARAMETER_CONTEXT_TYPE;

// --------

//#if __KERNEL__
#if 0
#define BUFFER_DTSHD_AUDIO_CODEC_DECODE_CONTEXT "DtshdAudioCodecDecodeContext"
#define BUFFER_DTSHD_AUDIO_CODEC_DECODE_CONTEXT_TYPE    {BUFFER_DTSHD_AUDIO_CODEC_DECODE_CONTEXT, BufferDataTypeBase, AllocateFromDeviceMemory, 32, 0, true, true, sizeof(DtshdAudioCodecDecodeContext_t)}
#else
#define BUFFER_DTSHD_AUDIO_CODEC_DECODE_CONTEXT "DtshdAudioCodecDecodeContext"
#define BUFFER_DTSHD_AUDIO_CODEC_DECODE_CONTEXT_TYPE    {BUFFER_DTSHD_AUDIO_CODEC_DECODE_CONTEXT, BufferDataTypeBase, AllocateFromOSMemory, 32, 0, true, true, sizeof(DtshdAudioCodecDecodeContext_t)}
#endif

static BufferDataDescriptor_t            DtshdAudioCodecDecodeContextDescriptor = BUFFER_DTSHD_AUDIO_CODEC_DECODE_CONTEXT_TYPE;

#define BUFFER_TRANSCODED_FRAME_BUFFER        "TranscodedFrameBuffer"
#define BUFFER_TRANSCODED_FRAME_BUFFER_TYPE   {BUFFER_TRANSCODED_FRAME_BUFFER, BufferDataTypeBase, AllocateFromSuppliedBlock, 4, 64, false, false, 0}

static BufferDataDescriptor_t            InitialTranscodedFrameBufferDescriptor = BUFFER_TRANSCODED_FRAME_BUFFER_TYPE;

////////////////////////////////////////////////////////////////////////////
///
/// Fill in the configuration parameters used by the super-class and reset everything.
///
Codec_MmeAudioDtshd_c::Codec_MmeAudioDtshd_c( bool isLbrStream) : IsLbrStream(isLbrStream)
{
    CodecStatus_t Status;

    if (isLbrStream)
        Configuration.CodecName                     = "DTS-HD LBR audio";
    else
        Configuration.CodecName                     = "DTS(-HD) audio";

    Configuration.StreamParameterContextCount           = 1;
    Configuration.StreamParameterContextDescriptor      = &DtshdAudioCodecStreamParameterContextDescriptor;

    Configuration.DecodeContextCount                    = 4;
    Configuration.DecodeContextDescriptor               = &DtshdAudioCodecDecodeContextDescriptor;

//

    AudioDecoderTransformCapabilityMask.DecoderCapabilityFlags = (1 << (isLbrStream?ACC_DTS_LBR:ACC_DTS));

    DecoderId                                                  = ACC_DTS_ID;

    CurrentTranscodeBufferIndex = 0;
    
    TranscodedFramePool = NULL;
    
    TranscodedFrameMemory[CachedAddress]      = NULL;
    TranscodedFrameMemory[UnCachedAddress]    = NULL;
    TranscodedFrameMemory[PhysicalAddress]    = NULL;
    
    Reset();

//

#ifdef CONFIG_CPU_SUBTYPE_STX7200
    // This is a pretty gross hack to allow the pre-startup checks to work (the ones that would have
    // us fall back the silence generator. The basic problem is that AUDIO_DECODER may refer to
    // either #3 or #4 (and may changed based on the victor of boot time races) and that, for some BD-ROM
    // deployments, #4 doesn't support DTS. This results in the eroneous adoption of the silence generator.
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
}


////////////////////////////////////////////////////////////////////////////
///
/// Reset method to manage the transcoded frame pool
///
CodecStatus_t Codec_MmeAudioDtshd_c::Reset( void )
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
Codec_MmeAudioDtshd_c::~Codec_MmeAudioDtshd_c( void )
{
    Halt();
    Reset();
}

////////////////////////////////////////////////////////////////////////////
///
/// Populate the supplied structure with parameters for DTSHD audio.
///
CodecStatus_t Codec_MmeAudioDtshd_c::FillOutTransformerGlobalParameters( MME_LxAudioDecoderGlobalParams_t *GlobalParams_p )
{
CodecStatus_t Status;

//

    MME_LxAudioDecoderGlobalParams_t &GlobalParams = *GlobalParams_p;
    GlobalParams.StructSize = sizeof(MME_LxAudioDecoderGlobalParams_t);

//


    MME_LxDtsConfig_t &Config = *((MME_LxDtsConfig_t *) GlobalParams.DecConfig);
    Config.DecoderId = ACC_DTS_ID;
    Config.StructSize = sizeof(MME_LxDtsConfig_t);
    memset(&Config.Config, 0, sizeof(MME_LxDtsConfig_t));
    // set the common fields
    Config.Config[DTS_CRC_ENABLE] = ACC_MME_FALSE;
    Config.Config[DTS_LFE_ENABLE] = ACC_MME_TRUE;
    Config.Config[DTS_DRC_ENABLE] = ACC_MME_FALSE;
    Config.Config[DTS_ES_ENABLE]  = ACC_MME_TRUE;
    Config.Config[DTS_96K_ENABLE] = ACC_MME_TRUE;
    Config.Config[DTS_NBBLOCKS_PER_TRANSFORM] = ACC_MME_TRUE;
    // DTS HD specifc parameters... (no impact on dts decoder)
    Config.Config[DTS_XBR_ENABLE] = ACC_MME_TRUE;
    Config.Config[DTS_XLL_ENABLE] = ACC_MME_TRUE;
    Config.Config[DTS_MIX_LFE]    = ACC_MME_FALSE;
    Config.Config[DTS_LBR_ENABLE] = (IsLbrStream)?ACC_MME_TRUE:ACC_MME_FALSE;
#define DTSHD_DRC_PERCENT 0 // set to 0 due to a bug in BL_25_10, normally to be set to 100...
    Config.PostProcessing.DRC = DTSHD_DRC_PERCENT;
    // In case of LBR, don't do any resampling
    Config.PostProcessing.SampleFreq = ACC_FS_ID;
    Config.PostProcessing.DialogNorm = ACC_MME_TRUE;
    Config.PostProcessing.Features = 0;
    Config.FirstByteEncSamples = 0;
    Config.Last4ByteEncSamples = 0;
    Config.DelayLossLess = 0;

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

    // downmix must be disabled for DTSHD
    MME_DMixGlobalParams_t DMix = PcmParams.DMix;
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
CodecStatus_t   Codec_MmeAudioDtshd_c::FillOutTransformerInitializationParameters( void )
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
/// Populate the AUDIO_DECODER's MME_SET_GLOBAL_TRANSFORMER_PARAMS parameters for DTSHD audio.
///
CodecStatus_t   Codec_MmeAudioDtshd_c::FillOutSetStreamParametersCommand( void )
{
CodecStatus_t Status;
//DtshdAudioStreamParameters_t *Parsed = (DtshdAudioStreamParameters_t *)ParsedFrameParameters->StreamParameterStructure;
DtshdAudioCodecStreamParameterContext_t *Context = (DtshdAudioCodecStreamParameterContext_t *)StreamParameterContext;

    if ((ParsedAudioParameters->OriginalEncoding != AudioOriginalEncodingDtshdLBR) && IsLbrStream)
    {
        CODEC_TRACE("This stream does not contain any lbr extension!\n");
        return CodecError;
    }

    // if the stream is dtshd, then the "transcoding" might be required
    if ( (ParsedAudioParameters->OriginalEncoding == AudioOriginalEncodingDtshdMA) || 
         (ParsedAudioParameters->OriginalEncoding == AudioOriginalEncodingDtshd) )
    {
        if (ParsedAudioParameters->BackwardCompatibleProperties.SampleRateHz && ParsedAudioParameters->BackwardCompatibleProperties.SampleCount)
        {
            // a core is present, so transcode is possible
            TranscodeEnable = true;
        }
        else
        {
            TranscodeEnable = false;
        }
    }
    else
    {
        TranscodeEnable = false;
    }
    //
    // Examine the parsed stream parameters and determine what type of codec to instanciate
    //

    DecoderId = ACC_DTS_ID;

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
CodecStatus_t   Codec_MmeAudioDtshd_c::FillOutDecodeCommand(       void )
{
DtshdAudioCodecDecodeContext_t  *Context        = (DtshdAudioCodecDecodeContext_t *)DecodeContext;
//DtshdAudioFrameParameters_t   *Parsed         = (DtshdAudioFrameParameters_t *)ParsedFrameParameters->FrameParameterStructure;

    //
    // Initialize the frame parameters (we don't actually have much to say here)
    //

    memset( &Context->DecodeParameters, 0, sizeof(Context->DecodeParameters) );

    //
    // Zero the reply structure
    //

    memset( &Context->DecodeStatus, 0, sizeof(Context->DecodeStatus) );

    // report this CoreSize value
    DecodeContext->MMEPages[0].FlagsIn = ((DtshdAudioFrameParameters_t *)ParsedFrameParameters->FrameParameterStructure)->CoreSize;

    // export the frame parameters structure to the decode context (so that we can access them from the MME callback)
    memcpy(&Context->ContextFrameParameters, ParsedFrameParameters->FrameParameterStructure, sizeof(DtshdAudioFrameParameters_t));

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
CodecStatus_t   Codec_MmeAudioDtshd_c::ValidateDecodeContext( CodecBaseDecodeContext_t *Context )
{
DtshdAudioCodecDecodeContext_t *LocalDecodeContext = (DtshdAudioCodecDecodeContext_t *) Context;
MME_LxAudioDecoderFrameStatus_t &Status       = LocalDecodeContext->DecodeStatus.DecStatus;
ParsedAudioParameters_t *AudioParameters;

//MME_LxAudioDecoderInitParams_t &Params = AudioDecoderInitializationParameters;


    CODEC_DEBUG(">><<\n");

    if (ENABLE_CODEC_DEBUG) 
    {
        //DumpCommand(bufferIndex);
    }

    if (Status.DecStatus != MME_SUCCESS) 
    {
        CODEC_ERROR("DTSHD audio decode error (muted frame): 0x%x\n", Status.DecStatus);
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

    AudioParameters = BufferState[LocalDecodeContext->BaseContext.BufferIndex].ParsedAudioParameters;

    AudioParameters->Source.BitsPerSample = AudioOutputSurface->BitsPerSample; /*(((Params.BlockWise >> 4) & 0xF) == ACC_WS32)?32:16;*/
    AudioParameters->Source.ChannelCount =  AudioOutputSurface->ChannelCount; /* ACC_AcMode2ChannelCount(Status.AudioMode) */ 
    AudioParameters->Organisation = Status.AudioMode;

    
    {
        int expected_spl = AudioParameters->SampleCount, firmware_spl = Status.NbOutSamples;
        int ratio_spl = 1, ratio_freq = 1;
        int expected_freq = AudioParameters->Source.SampleRateHz, firmware_freq = ACC_SamplingFreqLUT[Status.SamplingFreq];

        // check the firmware status against what we parsed
        if (expected_spl != firmware_spl)
        {
            ratio_spl = (expected_spl > firmware_spl)?(expected_spl/firmware_spl):(firmware_spl/expected_spl);
        }
        
        if (Status.SamplingFreq > ACC_FS_reserved)
        {
            CODEC_ERROR("DTSHD audio decode wrong sampling freq returned: %d\n", 
                        firmware_freq);
        }
        
        if (firmware_freq != expected_freq)
        {
            ratio_freq = (expected_freq > firmware_freq)?(expected_freq/firmware_freq):(firmware_freq/expected_freq);
        }

        // the ratio between the sampling freq and the number of samples should be the same 
        // (a core substream can contain extension such a DTS96)
        if ((ratio_freq != ratio_spl) && (Status.DecStatus == MME_SUCCESS))
        {
            CODEC_ERROR("DTSHD: Wrong ratio between expected and parsed frame porperties: nb samples: %d (expected %d), freq %d (expected %d)\n", 
                        firmware_spl, expected_spl, firmware_freq, expected_freq);
        }
        else
        {
            // the firmware output is true, and is what needs to be sent to the manifestor
            AudioParameters->SampleCount = firmware_spl;
            AudioParameters->Source.SampleRateHz = firmware_freq;
        }
        
        unsigned int period = (AudioParameters->SampleCount * 1000) / AudioParameters->Source.SampleRateHz;
        
        if( (Status.ElapsedTime/1000) > period )
        {
            CODEC_TRACE( "MME command took a lot of time (%d vs %d)\n",
                         Status.ElapsedTime, period );
        }
    }

    // Fill the parsed parameters with the DTS stream metadata
    Codec_MmeAudioDtshd_c::FillStreamMetadata(AudioParameters, (MME_LxAudioDecoderFrameStatus_t*)&Status);

    // Validate the extended status (without propagating errors)
    (void) ValidatePcmProcessingExtendedStatus(Context,
	    (MME_PcmProcessingFrameExtStatus_t *) &LocalDecodeContext->DecodeStatus.PcmStatus);

    if (TranscodeEnable)
    {
        TranscodeDtshdToDts(&LocalDecodeContext->BaseContext, 
                            LocalDecodeContext->TranscodeBufferIndex,
                            &LocalDecodeContext->ContextFrameParameters,
                            TranscodedBuffers);
    }		

    return CodecNoError;
}


////////////////////////////////////////////////////////////////////////////
///
/// Extract mixing metadata and stuff it into the audio parameters.
///
/// \todo Can we make this code common between EAC3 and DTSHD handling.
///
void Codec_MmeAudioDtshd_c::HandleMixingMetadata( CodecBaseDecodeContext_t *Context,
	                                          MME_PcmProcessingStatusTemplate_t *PcmStatus )
{
    ParsedAudioParameters_t *AudioParameters = BufferState[Context->BufferIndex].ParsedAudioParameters;
    MME_LxAudioDecoderMixingMetadata_t *MixingMetadata = (MME_LxAudioDecoderMixingMetadata_t *) PcmStatus;
    int NbMixConfigurations;
    
    //
    // Validation
    //
    
    CODEC_ASSERT( MixingMetadata->MinStruct.Id == ACC_MIX_METADATA_ID ); // already checked by framework
    if( MixingMetadata->MinStruct.StructSize < DTSHD_MIN_MIXING_METADATA_SIZE )
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


////////////////////////////////////////////////////////////////////////////
///
/// Static method that "transcodes" dtshd to dts
/// 
/// The transcoding consists in copying the dts core compatible substream 
/// the the transcoded buffer.
///
void    Codec_MmeAudioDtshd_c::TranscodeDtshdToDts( CodecBaseDecodeContext_t *    BaseContext,
                                                    unsigned int                  TranscodeBufferIndex,
                                                    DtshdAudioFrameParameters_t * FrameParameters,
                                                    CodecBufferState_t *          TranscodedBuffers )
{
    MME_Command_t * Cmd     = &BaseContext->MMECommand;
    unsigned char * SrcPtr  = (unsigned char *)Cmd->DataBuffers_p[0]->ScatterPages_p[0].Page_p;
    unsigned char * DestPtr = TranscodedBuffers[TranscodeBufferIndex].BufferPointer;
    unsigned int CoreSize   = FrameParameters->CoreSize;
    memcpy(DestPtr, SrcPtr + FrameParameters->BcCoreOffset, CoreSize);
    
    TranscodedBuffers[TranscodeBufferIndex].Buffer->SetUsedDataSize(CoreSize);
    
    // revert the substream core sync to the backward dts core sync
    if (FrameParameters->IsSubStreamCore && CoreSize)
    {
        unsigned int * SyncPtr = (unsigned int *) DestPtr;
        unsigned int SyncWord = *SyncPtr;
        
        // it is possible at this point that the firmware has already modified the input buffer
        // and done the conversion...
        if (SyncWord == __swapbw(DTSHD_START_CODE_SUBSTREAM_CORE))
        {
            *SyncPtr = __swapbw(DTSHD_START_CODE_CORE);
        }
        else if (SyncWord != __swapbw(DTSHD_START_CODE_CORE))
        {
            CODEC_ERROR("Wrong Core Substream Sync (0x%x) Implementation error?\n", SyncWord);
        }
    }
}

// /////////////////////////////////////////////////////////////////////////
//
//      Function to dump out the set stream 
//      parameters from an mme command.
//

CodecStatus_t   Codec_MmeAudioDtshd_c::DumpSetStreamParameters(         void    *Parameters )
{
    CODEC_ERROR("Not implemented\n");  
    return CodecNoError;
}

// /////////////////////////////////////////////////////////////////////////
//
//      Function to dump out the decode
//      parameters from an mme command.
//

CodecStatus_t   Codec_MmeAudioDtshd_c::DumpDecodeParameters(            void    *Parameters )
{
    CODEC_ERROR("Not implemented\n");  
    return CodecNoError;
}

////////////////////////////////////////////////////////////////////////////
///
///  Set Default FrameBase style TRANSFORM command for AudioDecoder MT
///  with 1 Input Buffer and 1 Output Buffer.

void Codec_MmeAudioDtshd_c::SetCommandIO( void )
{	
    if (TranscodeEnable)
    {
        CodecStatus_t Status = GetTranscodeBuffer();
	
        if (Status != CodecNoError)
        {            
            CODEC_ERROR("Error while requesting Transcoded buffer: %d. Disabling transcoding...\n", Status);
            TranscodeEnable = false;
        }
        ((DtshdAudioCodecDecodeContext_t *)DecodeContext)->TranscodeBufferIndex = CurrentTranscodeBufferIndex;
    }

    Codec_MmeAudio_c::SetCommandIO();
}

// /////////////////////////////////////////////////////////////////////////
//
//      Function to obtain a new decode buffer.
//

CodecStatus_t   Codec_MmeAudioDtshd_c::GetTranscodeBuffer( void )
{
    PlayerStatus_t           Status;
    BufferPool_t             Tfp;
    
    //
    // Get a buffer
    //
    
    Status = GetTranscodedFrameBufferPool( &Tfp );
    
    if( Status != CodecNoError )
    {
        CODEC_ERROR("GetTranscodeBuffer(%s) - Failed to obtain the transcoded buffer pool instance.\n", Configuration.CodecName );
        return Status;
    }
    
    Status  = Tfp->GetBuffer(&CurrentTranscodeBuffer, IdentifierCodec, DTSHD_FRAME_MAX_SIZE, false);
    
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

    if( CurrentTranscodeBufferIndex >= DTSHD_TRANSCODE_BUFFER_COUNT )
        CODEC_ERROR("GetTranscodeBuffer(%s) - Transcode buffer index >= DTSHD_TRANSCODE_BUFFER_COUNT - Implementation error.\n", Configuration.CodecName );
    
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
void Codec_MmeAudioDtshd_c::AttachCodedFrameBuffer( void )
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
        // the transcoded buffer is now only referenced by its attachement to the coded buffer
    }
}

// /////////////////////////////////////////////////////////////////////////
//
//      The get coded frame buffer pool fn
//

CodecStatus_t   Codec_MmeAudioDtshd_c::GetTranscodedFrameBufferPool( BufferPool_t * Tfp )
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
        AStatus = PartitionAllocatorOpen( &TranscodedFrameMemoryDevice, Configuration.TranscodedMemoryPartitionName, DTSHD_FRAME_MAX_SIZE * DTSHD_TRANSCODE_BUFFER_COUNT, true );
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
        //Configuration.CodedMemorySize           = 4*1024*1024;
#endif
        
        //
        
        Status  = BufferManager->CreatePool( &TranscodedFramePool, 
                                             TranscodedFrameBufferType, 
                                             DTSHD_TRANSCODE_BUFFER_COUNT, 
                                             DTSHD_FRAME_MAX_SIZE * DTSHD_TRANSCODE_BUFFER_COUNT, 
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

// /////////////////////////////////////////////////////////////////////////
//
//      Function to set the stream metadata according to what is contained 
//      in the steam bitstream (returned by the codec firmware status)
//

void Codec_MmeAudioDtshd_c::FillStreamMetadata(ParsedAudioParameters_t * AudioParameters, MME_LxAudioDecoderFrameStatus_t * Status)
{
    // this code is very fatpipe dependent, so maybe not in the right place...
    StreamMetadata_t * Metadata = &AudioParameters->StreamMetadata;
    tMME_BufferFlags * Flags = (tMME_BufferFlags *) &Status->PTSflag;
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
    Metadata->RearMatrixEncoded  = Flags->RearMatrixEncoded?2:1;
    // according to fatpipe specs
    Metadata->MixLevel  = 0;
    // surely 0
    Metadata->DialogNorm  = Flags->DialogNorm;
    
    Metadata->LfeGain = 10;
}
