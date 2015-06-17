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

Source file name : codec_mme_audio_silence.cpp
Author :           Daniel

Implementation of the mpeg2 audio codec class for player 2.


Date        Modification                                    Name
----        ------------                                    --------
19-Mar-08   Created (from codec_mme_audio_mpeg.cpp)         Daniel

************************************************************************/

////////////////////////////////////////////////////////////////////////////
/// \class Codec_MmeAudioSilence_c
///
/// A silence generating audio 'codec' to replace the main codec when licensing is insufficient.
///

// /////////////////////////////////////////////////////////////////////
//
//      Include any component headers

#include "codec_mme_audio_silence.h"
#include "codec_mme_audio_dtshd.h"
#include "player_generic.h"

// /////////////////////////////////////////////////////////////////////////
//
// Locally defined constants
//
// /////////////////////////////////////////////////////////////////////////
//
// Locally defined structures
//

// /////////////////////////////////////////////////////////////////////////
//
// Locally defined structures
//

typedef struct SilentAudioCodecStreamParameterContext_s
{
    CodecBaseStreamParameterContext_t   BaseContext;
} SilentAudioCodecStreamParameterContext_t;

//#if __KERNEL__
#if 0
#define BUFFER_SILENT_AUDIO_CODEC_STREAM_PARAMETER_CONTEXT                "SilentAudioCodecStreamParameterContext"
#define BUFFER_SILENT_AUDIO_CODEC_STREAM_PARAMETER_CONTEXT_TYPE   {BUFFER_SILENT_AUDIO_CODEC_STREAM_PARAMETER_CONTEXT, BufferDataTypeBase, AllocateFromDeviceMemory, 32, 0, true, true, sizeof(SilentAudioCodecStreamParameterContext_t)}
#else
#define BUFFER_SILENT_AUDIO_CODEC_STREAM_PARAMETER_CONTEXT                "SilentAudioCodecStreamParameterContext"
#define BUFFER_SILENT_AUDIO_CODEC_STREAM_PARAMETER_CONTEXT_TYPE   {BUFFER_SILENT_AUDIO_CODEC_STREAM_PARAMETER_CONTEXT, BufferDataTypeBase, AllocateFromOSMemory, 32, 0, true, true, sizeof(SilentAudioCodecStreamParameterContext_t)}
#endif

static BufferDataDescriptor_t            SilentAudioCodecStreamParameterContextDescriptor = BUFFER_SILENT_AUDIO_CODEC_STREAM_PARAMETER_CONTEXT_TYPE;

// --------

typedef union SilentAudioFrameParameters_s
{
    void*                       OtherAudioFrameParameters;
    DtshdAudioFrameParameters_t DtshdAudioFrameParameters;
} SilentAudioFrameParameters_t;

typedef struct SilentAudioCodecDecodeContext_s
{
    CodecBaseDecodeContext_t            BaseContext;
    unsigned int                        TranscodeBufferIndex;
    SilentAudioFrameParameters_t        ContextFrameParameters;
} SilentAudioCodecDecodeContext_t;

//#if __KERNEL__
#if 0
#define BUFFER_SILENT_AUDIO_CODEC_DECODE_CONTEXT  "SilentAudioCodecDecodeContext"
#define BUFFER_SILENT_AUDIO_CODEC_DECODE_CONTEXT_TYPE     {BUFFER_SILENT_AUDIO_CODEC_DECODE_CONTEXT, BufferDataTypeBase, AllocateFromDeviceMemory, 32, 0, true, true, sizeof(SilentAudioCodecDecodeContext_t)}
#else
#define BUFFER_SILENT_AUDIO_CODEC_DECODE_CONTEXT  "SilentAudioCodecDecodeContext"
#define BUFFER_SILENT_AUDIO_CODEC_DECODE_CONTEXT_TYPE     {BUFFER_SILENT_AUDIO_CODEC_DECODE_CONTEXT, BufferDataTypeBase, AllocateFromOSMemory, 32, 0, true, true, sizeof(SilentAudioCodecDecodeContext_t)}
#endif

static BufferDataDescriptor_t            SilentAudioCodecDecodeContextDescriptor = BUFFER_SILENT_AUDIO_CODEC_DECODE_CONTEXT_TYPE;

#define BUFFER_TRANSCODED_FRAME_BUFFER        "TranscodedFrameBuffer"
#define BUFFER_TRANSCODED_FRAME_BUFFER_TYPE   {BUFFER_TRANSCODED_FRAME_BUFFER, BufferDataTypeBase, AllocateFromSuppliedBlock, 4, 64, false, false, 0}

static BufferDataDescriptor_t            InitialTranscodedFrameBufferDescriptor = BUFFER_TRANSCODED_FRAME_BUFFER_TYPE;

////////////////////////////////////////////////////////////////////////////
///
/// Fill in the configuration parameters used by the super-class and reset everything.
///
/// \todo This uses default values for SizeOfTransformCapabilityStructure and
///       TransformCapabilityStructurePointer. This is wrong (but harmless).
///
Codec_MmeAudioSilence_c::Codec_MmeAudioSilence_c( void )
{
    CodecStatus_t Status;
    int i;

    Configuration.CodecName                             = "Silence generator";

    Configuration.StreamParameterContextCount           = 1;
    Configuration.StreamParameterContextDescriptor      = &SilentAudioCodecStreamParameterContextDescriptor;

    Configuration.DecodeContextCount                    = 4;
    Configuration.DecodeContextDescriptor               = &SilentAudioCodecDecodeContextDescriptor;

    for (i =0; i< CODEC_MAX_TRANSFORMERS; i++)
	Configuration.TransformName[i]                  = "SILENCE_GENERATOR";
    Configuration.AvailableTransformers                 = CODEC_MAX_TRANSFORMERS;

    Configuration.AddressingMode                        = CachedAddress;

//
    
    CurrentTranscodeBufferIndex = 0;
    
    TranscodedFramePool = NULL;
    
    TranscodedFrameMemory[CachedAddress]      = NULL;
    TranscodedFrameMemory[UnCachedAddress]    = NULL;
    TranscodedFrameMemory[PhysicalAddress]    = NULL;
    
    Reset();
    
    ProtectTransformName				= true;
    
    Status = GloballyVerifyMMECapabilities();
    if( CodecNoError != Status )
    {
	CODEC_ERROR( "Silence generator not found (module not installed?)\n" );
	InitializationStatus = PlayerNotSupported;
	return;
    }
}


////////////////////////////////////////////////////////////////////////////
///
/// Reset method to manage the transcoded frame pool
///
CodecStatus_t Codec_MmeAudioSilence_c::Reset( void )
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
Codec_MmeAudioSilence_c::~Codec_MmeAudioSilence_c( void )
{
    Halt();
    Reset();
}


////////////////////////////////////////////////////////////////////////////
///
/// Examine the capability structure returned by the firmware.
///
/// Unconditionally return success; the silence generator does not report
/// anything other than a version number. 
///
CodecStatus_t   Codec_MmeAudioSilence_c::HandleCapabilities( void )
{
    return CodecNoError;
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
CodecStatus_t   Codec_MmeAudioSilence_c::FillOutTransformerInitializationParameters( void )
{
    MMEInitializationParameters.TransformerInitParamsSize = 0;
    MMEInitializationParameters.TransformerInitParams_p = NULL;
    
    return CodecNoError;
}


////////////////////////////////////////////////////////////////////////////
///
/// Populate the (non-existant) MME_SET_GLOBAL_TRANSFORMER_PARAMS parameters.
///
CodecStatus_t   Codec_MmeAudioSilence_c::FillOutSetStreamParametersCommand( void )
{

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
    // Fillout the actual command
    //

    StreamParameterContext->MMECommand.CmdStatus.AdditionalInfoSize        = 0;
    StreamParameterContext->MMECommand.CmdStatus.AdditionalInfo_p          = NULL;
    StreamParameterContext->MMECommand.ParamSize                           = 0;
    StreamParameterContext->MMECommand.Param_p                             = NULL;

//

    return CodecNoError;
}

////////////////////////////////////////////////////////////////////////////
///
/// Populate the (non-existant) MME_TRANSFORM parameters.
///
CodecStatus_t   Codec_MmeAudioSilence_c::FillOutDecodeCommand(       void )
{

    SilentAudioCodecDecodeContext_t  *Context        = (SilentAudioCodecDecodeContext_t *)DecodeContext;

    // export the frame parameters structure to the decode context (so that we can access them from the MME callback)
    memcpy(&Context->ContextFrameParameters, ParsedFrameParameters->FrameParameterStructure, sizeof(DtshdAudioFrameParameters_t));

    //
    // Fillout the actual command
    //

    DecodeContext->MMECommand.CmdStatus.AdditionalInfoSize        = 0;
    DecodeContext->MMECommand.CmdStatus.AdditionalInfo_p          = NULL;
    DecodeContext->MMECommand.ParamSize                           = 0;
    DecodeContext->MMECommand.Param_p                             = NULL;

    return CodecNoError;
}

////////////////////////////////////////////////////////////////////////////
///
/// Validate the (non-existant) reply from the transformer.
/// 
/// Unconditionally return success. Given the whole point of the transformer is
/// to mute the output it cannot 'successfully' fail in the way the audio firmware
/// does.
///
/// \return CodecNoError
///
CodecStatus_t   Codec_MmeAudioSilence_c::ValidateDecodeContext( CodecBaseDecodeContext_t *Context )
{

	SilentAudioCodecDecodeContext_t *LocalDecodeContext = (SilentAudioCodecDecodeContext_t *) Context;

    memset( &AudioDecoderStatus, 0, sizeof(AudioDecoderStatus));    // SYSFS
    
    if (TranscodeEnable)
    {
        Codec_MmeAudioDtshd_c::TranscodeDtshdToDts(&LocalDecodeContext->BaseContext,
                                                   LocalDecodeContext->TranscodeBufferIndex,
                                                   &LocalDecodeContext->ContextFrameParameters.DtshdAudioFrameParameters,
                                                   TranscodedBuffers);
    }

    return CodecNoError;
}

// /////////////////////////////////////////////////////////////////////////
//
//      Function to dump out the set stream 
//      parameters from an mme command.
//

CodecStatus_t   Codec_MmeAudioSilence_c::DumpSetStreamParameters(          void    *Parameters )
{
    CODEC_ERROR("Not implemented\n");  
    return CodecNoError;
}

// /////////////////////////////////////////////////////////////////////////
//
//      Function to dump out the decode
//      parameters from an mme command.
//

CodecStatus_t   Codec_MmeAudioSilence_c::DumpDecodeParameters(             void    *Parameters )
{
    CODEC_ERROR("Not implemented\n");  
    return CodecNoError;
}

////////////////////////////////////////////////////////////////////////////
///
///  Set Default FrameBase style TRANSFORM command for AudioDecoder MT
///  with 1 Input Buffer and 1 Output Buffer.

void Codec_MmeAudioSilence_c::SetCommandIO( void )
{	
    if (TranscodeEnable)
    {
        CodecStatus_t Status = GetTranscodeBuffer();
	
        if (Status != CodecNoError)
        {            
            CODEC_ERROR("Error while requesting Transcoded buffer: %d. Disabling transcoding...\n", Status);
            TranscodeEnable = false;
        }
        ((SilentAudioCodecDecodeContext_t *)DecodeContext)->TranscodeBufferIndex = CurrentTranscodeBufferIndex;
    }

    Codec_MmeAudio_c::SetCommandIO();
}

// /////////////////////////////////////////////////////////////////////////
//
//      Function to obtain a new decode buffer.
//

CodecStatus_t   Codec_MmeAudioSilence_c::GetTranscodeBuffer( void )
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
void Codec_MmeAudioSilence_c::AttachCodedFrameBuffer( void )
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

CodecStatus_t   Codec_MmeAudioSilence_c::GetTranscodedFrameBufferPool( BufferPool_t * Tfp )
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
//        Configuration.CodedMemorySize           = 4*1024*1024;
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
