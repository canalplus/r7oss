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

Source file name : codec_mme_audio_stream.cpp
Author :           Julian

Implementation of the basic stream based audio codec class for player 2.


Date        Modification                                    Name
----        ------------                                    --------
27-May-09   Created (from codec_mme_audio_wma.cpp)          Julian

************************************************************************/

//{{{  note
////////////////////////////////////////////////////////////////////////////
/// \class Codec_MmeAudioStream_c
///
/// The stream audio codec proxy.
///
/// When it is difficult for the host processor to efficiently determine the
/// frame boundaries the player makes no attempt to discover the frame boundaries.
/// There is not a one-to-one relationship between input buffers and output buffers
/// so the decoder is operated with streaming input (MME_SEND_BUFFERS) extracting
/// frame based output whenever we believe the decoder to be capable of
/// providing data (MME_TRANSFORM).
///
///
//}}}

// /////////////////////////////////////////////////////////////////////
//
//      Include any component headers

#include "codec_mme_audio_stream.h"
#include "ksound.h"

#ifdef __KERNEL__
extern "C"{void flush_cache_all();};
#endif

// /////////////////////////////////////////////////////////////////////////
//
// Locally defined constants
//

// /////////////////////////////////////////////////////////////////////////
//
// Locally defined structures
//

static OS_TaskEntry (TransformThreadStub)
{
    Codec_MmeAudioStream_c*     Codec = (Codec_MmeAudioStream_c*)Parameter;

    Codec->TransformThread();
    OS_TerminateThread();
    return NULL;
}

//{{{  Constructor
////////////////////////////////////////////////////////////////////////////
///
/// Fill in the configuration parameters used by the super-class and reset everything.
///
Codec_MmeAudioStream_c::Codec_MmeAudioStream_c( void )
{
    Configuration.CodecName                             = "Stream audio";       // This will be replaced by sub class

    TransformContextPool                                = NULL;

    TransformCodedFrameMemory[CachedAddress]            = NULL;
    TransformCodedFrameMemory[UnCachedAddress]          = NULL;
    TransformCodedFrameMemory[PhysicalAddress]          = NULL;
    TransformCodedFramePool                             = NULL;

    DecoderId                                           = ACC_LAST_DECODER_ID;  // Replaced by sub class

    TransformThreadRunning                              = false;
    TransformThreadId                                   = OS_INVALID_THREAD;

    Reset();

}
//}}}
//{{{  Destructor
////////////////////////////////////////////////////////////////////////////
///
///     Destructor function, ensures a full halt and reset
///     are executed for all levels of the class.
///
Codec_MmeAudioStream_c::~Codec_MmeAudioStream_c( void )
{
    Halt();
    Reset();
}
//}}}

//{{{  Halt
////////////////////////////////////////////////////////////////////////////
///
/// The halt function release any resources, and reset all variables
///
///
CodecStatus_t   Codec_MmeAudioStream_c::Halt(     void )
{
    CODEC_DEBUG("%s\n", __FUNCTION__);
    if (TransformThreadId != OS_INVALID_THREAD)
    {

        // notify the thread it should exit
        TransformThreadRunning          = false;

        // set any events the thread may be blocked waiting for
        OS_SetEvent (&IssueTransformCommandEvent);

        // wait for the thread to come to rest
        OS_WaitForEvent (&TransformThreadTerminated, OS_INFINITE);

        // tidy up
        OS_TerminateEvent (&TransformThreadTerminated);
        OS_TerminateEvent (&IssueTransformCommandEvent);
        OS_TerminateMutex (&DecodeContextPoolMutex);
        OS_TerminateMutex (&InputMutex);

        TransformThreadId               = OS_INVALID_THREAD;
    }

    return Codec_MmeAudio_c::Halt ();

}
//}}}
//{{{  Reset
////////////////////////////////////////////////////////////////////////////
///
/// The Reset function release any resources, and reset all variables
///
/// \todo This method is mismatched with the contructor; it frees events that it shouldn't
///
CodecStatus_t   Codec_MmeAudioStream_c::Reset(     void )
{

    // Release the decoded frame context buffer pool
    if (TransformContextPool != NULL)
    {
        BufferManager->DestroyPool (TransformContextPool);
        TransformContextPool                            = NULL;
    }

    // Release the coded frame buffer pool
    if (TransformCodedFramePool != NULL)
    {
        BufferManager->DestroyPool (TransformCodedFramePool);
        TransformCodedFramePool                          = NULL;
    }

    // Free the coded frame memory
    if (TransformCodedFrameMemory[CachedAddress] != NULL)
    {
#if __KERNEL__
        AllocatorClose (TransformCodedFrameMemoryDevice);
#endif

        TransformCodedFrameMemory[CachedAddress]        = NULL;
        TransformCodedFrameMemory[UnCachedAddress]      = NULL;
        TransformCodedFrameMemory[PhysicalAddress]      = NULL;
    }

    SendBuffersCommandsIssued                           = 0;
    SendBuffersCommandsCompleted                        = 0;
    TransformCommandsIssued                             = 0;
    TransformCommandsCompleted                          = 0;

    NeedToMarkStreamUnplayable                          = false;
    LastNormalizedPlaybackTime                          = UNSPECIFIED_TIME;

    InPossibleMarkerStallState                          = false;

    return Codec_MmeAudio_c::Reset();
}
//}}}

//{{{  RegisterOutputBufferRing
// /////////////////////////////////////////////////////////////////////////
//
//      The register output ring function
//
CodecStatus_t   Codec_MmeAudioStream_c::RegisterOutputBufferRing       (Ring_t          Ring)
{
    PlayerStatus_t      Status;

    Status              = Codec_MmeAudio_c::RegisterOutputBufferRing( Ring );
    if (Status != CodecNoError)
        return Status;

    //{{{  Create transform thread and mutexes
    // Create Transform thread and events and mutexes it uses
    // If TransformThreadId != OS_INVALID_THREAD all threads, events and mutexes are valid
    if (TransformThreadId == OS_INVALID_THREAD)
    {
        if (OS_InitializeMutex (&InputMutex) != OS_NO_ERROR)
        {
            CODEC_ERROR( "Unable to create the InputMutex\n");
            return CodecError;
        }

        if (OS_InitializeMutex (&DecodeContextPoolMutex) != OS_NO_ERROR)
        {
            CODEC_ERROR( "Unable to create the DecodeContextPoolMutex\n");
            OS_TerminateMutex (&InputMutex);
            return CodecError;
        }

        if (OS_InitializeEvent (&IssueTransformCommandEvent) != OS_NO_ERROR)
        {
            CODEC_ERROR( "Unable to create the SendBuffersCommandCompleted event\n" );
            OS_TerminateMutex (&DecodeContextPoolMutex);
            OS_TerminateMutex (&InputMutex);
            return CodecError;
        }

        if (OS_InitializeEvent( &TransformThreadTerminated ) != OS_NO_ERROR)
        {
            CODEC_ERROR( "Unable to create the Transform thread terminated event\n" );
            OS_TerminateEvent (&IssueTransformCommandEvent);
            OS_TerminateMutex (&DecodeContextPoolMutex);
            OS_TerminateMutex (&InputMutex);
            return CodecError;
        }

        TransformThreadRunning              = true;
        if (OS_CreateThread( &TransformThreadId, TransformThreadStub, this, "Stream audio transform thread", OS_MID_PRIORITY+9 ) != OS_NO_ERROR)
        {
            CODEC_ERROR( "Unable to create Transform playback thread\n");
            TransformThreadId               = OS_INVALID_THREAD;
            TransformThreadRunning          = false;
            OS_TerminateEvent (&TransformThreadTerminated);
            OS_TerminateEvent (&IssueTransformCommandEvent);
            OS_TerminateMutex (&DecodeContextPoolMutex);
            OS_TerminateMutex (&InputMutex);
            return CodecError;
        }
    }
    //}}}

    // Create the coded frame buffer pool if necessary
    if (TransformCodedFramePool == NULL)
    {
        BufferType_t    CodedFrameBufferType;
        unsigned int    CodedFrameCount;
        unsigned int    CodedMemorySize;

        CodedFrameBufferPool->GetType (&CodedFrameBufferType);
        CodedFrameBufferPool->GetPoolUsage (&CodedFrameCount, NULL, &CodedMemorySize, NULL, NULL);

        // Get the memory and Create the pool with it
#if __KERNEL__
        Status          = PartitionAllocatorOpen       (&TransformCodedFrameMemoryDevice,
                                                        Configuration.TranscodedMemoryPartitionName,
                                                        CodedMemorySize,
                                                        true);
        if (Status != allocator_ok)
        {
            CODEC_ERROR("(%s) - Failed to allocate memory\n", Configuration.CodecName);
            return PlayerInsufficientMemory;
        }

        TransformCodedFrameMemory[CachedAddress]        = AllocatorUserAddress (TransformCodedFrameMemoryDevice);
        TransformCodedFrameMemory[UnCachedAddress]      = AllocatorUncachedUserAddress (TransformCodedFrameMemoryDevice);
        TransformCodedFrameMemory[PhysicalAddress]      = AllocatorPhysicalAddress (TransformCodedFrameMemoryDevice);
#else
        static unsigned char    FrameMemory[4*1024*1024];

        TransformCodedFrameMemory[CachedAddress]        = FrameMemory;
        TransformCodedFrameMemory[UnCachedAddress]      = FrameNULL;
        TransformCodedFrameMemory[PhysicalAddress]      = FrameMemory;
        //Configuration.CodedMemorySize                   = 4*1024*1024;
#endif

        Status          = BufferManager->CreatePool (&TransformCodedFramePool, CodedFrameBufferType,
                                                     CODEC_MAX_DECODE_BUFFERS, CodedMemorySize, TransformCodedFrameMemory);
        if (Status != BufferNoError)
        {
            CODEC_ERROR("(%s) - Failed to create the coded frame pool.\n", Configuration.CodecName);
            return PlayerInsufficientMemory;
        }

        Status          = TransformCodedFramePool->AttachMetaData (Player->MetaDataParsedFrameParametersType);
        if (Status != PlayerNoError)
        {
            CODEC_ERROR("(%s) - Failed to attach ParsedFrameParameters to the coded buffer pool.\n", Configuration.CodecName);
            return Status;
        }

        Status          = TransformCodedFramePool->AttachMetaData (Player->MetaDataSequenceNumberType);
        if (Status != PlayerNoError)
        {
            CODEC_ERROR("(%s) - Failed to attach sequence numbers to the coded buffer pool.\n", Configuration.CodecName);
            return Status;
        }
    }

    // Create the decode context buffers
    Player->GetBufferManager (&BufferManager);

    if (TransformContextPool == NULL)
    {
        Status          = BufferManager->CreatePool (&TransformContextPool, DecodeContextType, 4);
        if (Status != BufferNoError)
        {
            CODEC_ERROR("(%s) - Failed to create a pool of decode context buffers.\n", Configuration.CodecName);
            return Status;
        }

        //Set this up here too - this will be stamped onto the ParsedFrameParameters of the CodedFrames
        CurrentDecodeFrameIndex         = 0;
    }

    TransformContextBuffer              = NULL;

    return CodecNoError;
}
//}}}
//{{{  Input
// /////////////////////////////////////////////////////////////////////////
//
//      The Input function - receive chunks of data parsed by the frame parser
//
CodecStatus_t   Codec_MmeAudioStream_c::Input(     Buffer_t          CodedBuffer )
{
    CodecStatus_t               Status;
    ParsedFrameParameters_t*    ParsedFrameParameters;
    PlayerSequenceNumber_t*     SequenceNumberStructure;

    OS_LockMutex (&InputMutex );

    //! get the coded frame params
    Status              = CodedBuffer->ObtainMetaDataReference (Player->MetaDataParsedFrameParametersType, (void**)(&ParsedFrameParameters));
    if (Status != PlayerNoError)
    {
        CODEC_ERROR("(%s) No frame params on coded frame.\n", Configuration.CodecName);
        OS_UnLockMutex (&InputMutex);
        return Status;
    }
    memcpy (&SavedParsedFrameParameters, ParsedFrameParameters, sizeof(ParsedFrameParameters_t));

    Status              = CodedBuffer->ObtainMetaDataReference (Player->MetaDataSequenceNumberType, (void**)(&SequenceNumberStructure));
    if (Status != PlayerNoError)
    {
        CODEC_ERROR("(%s) Unable to obtain the meta data \"SequenceNumber\" - Implementation error\n", Configuration.CodecName);
        OS_UnLockMutex (&InputMutex);
        return Status;
    }
    memcpy (&SavedSequenceNumberStructure, SequenceNumberStructure, sizeof(PlayerSequenceNumber_t));


    // Perform base operations, on return we may need to mark the stream as unplayable
    Status      = Codec_MmeAudio_c::Input (CodedBuffer);

    OS_UnLockMutex (&InputMutex);

    if (NeedToMarkStreamUnplayable)
    {
        CODEC_ERROR("(%s) Marking stream unplayable\n", Configuration.CodecName);
        Player->MarkStreamUnPlayable (Stream);
        NeedToMarkStreamUnplayable      = false;
    }

    return Status;
}
//}}}

//{{{  FillOutTransformerInitializationParameters
////////////////////////////////////////////////////////////////////////////
///
/// Populate the AUDIO_DECODER's initialization parameters for Stream audio.
///
/// When this method completes Codec_MmeAudio_c::AudioDecoderInitializationParameters
/// will have been filled out with valid values sufficient to initialize an
/// Stream audio decoder
///
CodecStatus_t   Codec_MmeAudioStream_c::FillOutTransformerInitializationParameters (void)
{
    CodecStatus_t                       Status;
    MME_LxAudioDecoderInitParams_t     &Params  = AudioDecoderInitializationParameters;

    MMEInitializationParameters.TransformerInitParamsSize       = sizeof(Params);
    MMEInitializationParameters.TransformerInitParams_p         = &Params;


    Status              = Codec_MmeAudio_c::FillOutTransformerInitializationParameters();
    if (Status != CodecNoError)
        return Status;

    return FillOutTransformerGlobalParameters (&Params.GlobalParams);
}
//}}}
//{{{  FillOutSetStreamParametersCommand
////////////////////////////////////////////////////////////////////////////
///
/// Populate the AUDIO_DECODER's MME_SET_GLOBAL_TRANSFORMER_PARAMS parameters for stream based audio.
///
CodecStatus_t   Codec_MmeAudioStream_c::FillOutSetStreamParametersCommand( void )
{
    CodecStatus_t                               Status;
    StreamAudioCodecStreamParameterContext_t*   Context = (StreamAudioCodecStreamParameterContext_t*)StreamParameterContext;

    CODEC_DEBUG("%s\n", __FUNCTION__);

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

//{{{  FillOutDecodeContext
////////////////////////////////////////////////////////////////////////////
///
/// Override the superclass version to suit MME_SEND_BUFFERS.
///
/// Populate the DecodeContext structure with parameters for a single buffer
///
CodecStatus_t Codec_MmeAudioStream_c::FillOutDecodeContext( void )
{
    //
    // Provide default values for the input and output buffers (the sub-class can change this if it wants to).
    //

    memset (&DecodeContext->MMECommand, 0x00, sizeof(MME_Command_t));

    DecodeContext->MMECommand.NumberInputBuffers                = 1;
    DecodeContext->MMECommand.NumberOutputBuffers               = 0;
    DecodeContext->MMECommand.DataBuffers_p = DecodeContext->MMEBufferList;

    // plumbing
    DecodeContext->MMEBufferList[0]                             = &DecodeContext->MMEBuffers[0];

    memset (&DecodeContext->MMEBuffers[0], 0, sizeof(MME_DataBuffer_t));
    DecodeContext->MMEBuffers[0].StructSize                     = sizeof(MME_DataBuffer_t);
    DecodeContext->MMEBuffers[0].NumberOfScatterPages           = 1;
    DecodeContext->MMEBuffers[0].ScatterPages_p                 = &DecodeContext->MMEPages[0];

    memset (&DecodeContext->MMEPages[0], 0, sizeof(MME_ScatterPage_t));

    // input
    DecodeContext->MMEBuffers[0].TotalSize                      = CodedDataLength;
    DecodeContext->MMEPages[0].Page_p                           = CodedData;
    DecodeContext->MMEPages[0].Size                             = CodedDataLength;

    return CodecNoError;
}
//}}}
//{{{  FillOutDecodeCommand
////////////////////////////////////////////////////////////////////////////
///
/// Populate the AUDIO_DECODER's -=<< !! MME_SEND_BUFFERS !! >>=- parameters for Stream audio.
///1572864
CodecStatus_t   Codec_MmeAudioStream_c::FillOutDecodeCommand(       void )
{
    StreamAudioCodecDecodeContext_t*    Context = (StreamAudioCodecDecodeContext_t*)DecodeContext;

    Context->BaseContext.MMECommand.CmdCode                             = MME_SEND_BUFFERS;

    // Initialize the frame parameters
    memset (&Context->DecodeParameters, 0, sizeof(Context->DecodeParameters));

    // Zero the reply structure
    memset (&Context->DecodeStatus, 0, sizeof(Context->DecodeStatus) );

    // Fillout the actual command
    Context->BaseContext.MMECommand.CmdStatus.AdditionalInfoSize        = sizeof(Context->DecodeStatus);
    Context->BaseContext.MMECommand.CmdStatus.AdditionalInfo_p          = (MME_GenericParams_t)(&Context->DecodeStatus);
    Context->BaseContext.MMECommand.ParamSize                           = sizeof(Context->DecodeParameters);
    Context->BaseContext.MMECommand.Param_p                             = (MME_GenericParams_t)(&Context->DecodeParameters);

    return CodecNoError;
}
//}}}
//{{{  SendMMEDecodeCommand
CodecStatus_t   Codec_MmeAudioStream_c::SendMMEDecodeCommand   (void)
{
    CodecStatus_t                       Status;
    StreamAudioCodecDecodeContext_t*    Context         = (StreamAudioCodecDecodeContext_t*)DecodeContext;
    Buffer_t                            AttachedCodedDataBuffer;
    ParsedFrameParameters_t*            ParsedFrameParams;
    unsigned int                        PTSFlag         = 0;
    unsigned long long int              PTS             = ACC_NO_PTS_DTS;

    OS_AutoLockMutex mutex (&DecodeContextPoolMutex);

    //DecodeContext->DecodeContextBuffer->Dump (DumpAll);
    // pass the PTS to the firmware...
    Status              = DecodeContext->DecodeContextBuffer->ObtainAttachedBufferReference (CodedFrameBufferType, &AttachedCodedDataBuffer);
    if (Status == BufferNoError)
    {
        Status          = AttachedCodedDataBuffer->ObtainMetaDataReference (Player->MetaDataParsedFrameParametersType,
                                                                           (void**)(&ParsedFrameParams));
        //CODEC_TRACE ("%s (%llx,%llx)\n", __FUNCTION__, ParsedFrameParams->NormalizedPlaybackTime, ParsedFrameParams->NativePlaybackTime);
        if (Status == BufferNoError)
        {
            if (ValidTime (ParsedFrameParams->NormalizedPlaybackTime))
            {
                PTS                                             = ParsedFrameParams->NativePlaybackTime;
                // inform the firmware a pts is present
                PTSFlag                                         = ACC_PTS_PRESENT;
            }
            //else
            //    CODEC_ERROR("(%s) PTS = INVALID_TIME\n", Configuration.CodecName);
        }
        else
            CODEC_ERROR("(%s) No meta data reference\n", Configuration.CodecName);
    }
    else
        CODEC_ERROR("(%s) No attached buffer reference\n", Configuration.CodecName);

    //CODEC_TRACE("%s PTSFlag %x, PTS %llx\n", __FUNCTION__, PTSFlag, PTS);

#if DRV_MULTICOM_AUDIO_DECODER_VERSION >= 0x090128
    Context->DecodeParameters.BufferFlags.Bits.PTS_DTS_FLAG     = PTSFlag;
    Context->DecodeParameters.BufferFlags.Bits.PTS_Bit32        = PTS >> 32;
    Context->DecodeParameters.PTS                               = PTS;
#else
     MME_ADBufferParams_t *      BufferParams                   = (MME_ADBufferParams_t *) &(Context->DecodeParameters.BufferParams[0]);
     BufferParams->PTSflags.Bits.PTS_DTS_FLAG                   = PTSFlag;
     BufferParams->PTSflags.Bits.PTS_Bit32                      = PTS >> 32;
     BufferParams->PTS                                          = PTS;
#endif

    SendBuffersCommandsIssued++;
    OS_SetEvent (&IssueTransformCommandEvent);

    Status              = Codec_MmeBase_c::SendMMEDecodeCommand();

    return Status;
}
//}}}
//{{{  FinishedDecode
////////////////////////////////////////////////////////////////////////////
///
/// Clear up - do nothing, as actual decode done elsewhere
///
void Codec_MmeAudioStream_c::FinishedDecode( void )
{
    // We have NOT finished decoding into this buffer
    // This is to replace the cleanup after an MME_TRANSFORM from superclass Input function
    // But as we over-ride this with an MME_SEND_BUFFERS, and do the transform from a seperate thread
    // we don't want to clean up anything here

    return;
}
//}}}
//{{{  ValidateDecodeContext
////////////////////////////////////////////////////////////////////////////
///
/// Validate the ACC status structure and squawk loudly if problems are found.
/// in spite of the messages this method unconditionally returns success. This is
/// because the firmware will already have concealed the decode problems by
/// performing a soft mute.
/// 
/// \return CodecSuccess
///
CodecStatus_t   Codec_MmeAudioStream_c::ValidateDecodeContext (CodecBaseDecodeContext_t* Context)
{
    StreamAudioCodecDecodeContext_t*    DecodeContext   = (StreamAudioCodecDecodeContext_t*)Context;
    MME_LxAudioDecoderFrameStatus_t    &Status          = DecodeContext->DecodeStatus;
    ParsedAudioParameters_t*            AudioParameters;
    Buffer_t                            TheCurrentDecodeBuffer;
    PlayerStatus_t                      PlayerStatus;
    ParsedFrameParameters_t*            DecodedFrameParsedFrameParameters;

    CODEC_DEBUG("%s: DecStatus %d\n", __FUNCTION__, Status.DecStatus);

    if (ENABLE_CODEC_DEBUG)
    {
        //DumpCommand(bufferIndex);
    }

    if (Status.DecStatus != ACC_HXR_OK)
    {
        CODEC_ERROR("(%s) - Audio decode error (muted frame): %d\n", Configuration.CodecName, Status.DecStatus);
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

    if (AudioOutputSurface == NULL)
    {
        CODEC_ERROR("(%s) - AudioOutputSurface is NULL\n", Configuration.CodecName);
        return CodecError;
    }

    AudioParameters                             = BufferState[DecodeContext->BaseContext.BufferIndex].ParsedAudioParameters;
    if (AudioParameters == NULL)
    {
        CODEC_ERROR("(%s) - AudioParameters are NULL\n", Configuration.CodecName);
        return CodecError;
    }

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
        CODEC_ERROR("(%s) - Audio decode bad sampling freq returned: 0x%x\n", Configuration.CodecName, SamplingFreqCode);
    }

    CODEC_DEBUG("AudioParameters                               %p\n", AudioParameters);
    CODEC_DEBUG("AudioParameters->Source.BitsPerSample         %d\n", AudioParameters->Source.BitsPerSample);
    CODEC_DEBUG("AudioParameters->Source.ChannelCount          %d\n", AudioParameters->Source.ChannelCount);
    CODEC_DEBUG("AudioParameters->Organisation                 %d\n", AudioParameters->Organisation);
    CODEC_DEBUG("AudioParameters->SampleCount                  %d\n", AudioParameters->SampleCount);
    CODEC_DEBUG("AudioParameters->Source.SampleRateHz          %d\n", AudioParameters->Source.SampleRateHz);

    //! This is probably the right time to synthesise a PTS
    TheCurrentDecodeBuffer                      = BufferState[DecodeContext->BaseContext.BufferIndex].Buffer; //= CurrentDecodeBuffer;
    PlayerStatus                                = TheCurrentDecodeBuffer->ObtainMetaDataReference (Player->MetaDataParsedFrameParametersReferenceType, (void**)(&DecodedFrameParsedFrameParameters));
    if (PlayerStatus == PlayerNoError)
    {
        long long       CalculatedDelta         = ((unsigned long long)AudioParameters->SampleCount * 1000000ull) /
                                                  ((unsigned long long)AudioParameters->Source.SampleRateHz);

        // post-decode the DTS isn't interesting (especially given we had to fiddle with it in the
        // frame parser) so we discard it entirely rather then extracting it from the codec's reply.

        DecodedFrameParsedFrameParameters->NativeDecodeTime     = INVALID_TIME;
        DecodedFrameParsedFrameParameters->NormalizedDecodeTime = INVALID_TIME;

        //
        // suck out the PTS from the codec's reply
        //

        if (ACC_isPTS_PRESENT(Status.PTSflag.Bits.PTS_DTS_FLAG))
        {
            unsigned long long  Temp    = (unsigned long long)Status.PTS + (unsigned long long)(((unsigned long long)Status.PTSflag.Bits.PTS_Bit32) << 32);
            DecodedFrameParsedFrameParameters->NativePlaybackTime       = Temp;
            DecodedFrameParsedFrameParameters->NormalizedPlaybackTime   = ((Temp*1000000)/90000);

            CODEC_DEBUG("Status.PTS %llx\n", Temp);
        }
        else if (LastNormalizedPlaybackTime != UNSPECIFIED_TIME )
        {
            // synthesise a PTS
            unsigned long long  Temp    = LastNormalizedPlaybackTime + CalculatedDelta;
            DecodedFrameParsedFrameParameters->NativePlaybackTime       = (Temp * 90000)/100000;
            DecodedFrameParsedFrameParameters->NormalizedPlaybackTime   = Temp;
            CODEC_DEBUG("Synthesized PTS %llx\n", Temp);
        }
        else
        {
            DecodedFrameParsedFrameParameters->NativePlaybackTime       = 0;
            DecodedFrameParsedFrameParameters->NormalizedPlaybackTime   = 0;
        }

        // Squawk if time does not progress quite as expected.
        if (LastNormalizedPlaybackTime != UNSPECIFIED_TIME)
        {
            long long   RealDelta                       = DecodedFrameParsedFrameParameters->NormalizedPlaybackTime - LastNormalizedPlaybackTime;
            long long   DeltaDelta                      = RealDelta - CalculatedDelta;
            long long   PtsJitterTollerenceThreshold    = 1000;      // Changed to 1ms by nick, because some file formats specify pts times to 1 ms accuracy

            // Check that the predicted and actual times deviate by no more than the threshold
            if (DeltaDelta < -PtsJitterTollerenceThreshold || DeltaDelta > PtsJitterTollerenceThreshold)
            {
                CODEC_ERROR( "(%s)Unexpected change in playback time. Expected %lldus, got %lldus (deltas: exp. %lld  got %lld )\n",
                             Configuration.CodecName,
                             LastNormalizedPlaybackTime + CalculatedDelta,
                             DecodedFrameParsedFrameParameters->NormalizedPlaybackTime,
                             CalculatedDelta, RealDelta);

            }     
        }

        LastNormalizedPlaybackTime      = DecodedFrameParsedFrameParameters->NormalizedPlaybackTime;

    }
    else
        CODEC_ERROR ("(%s) - ObtainMetaDataReference failed\n", Configuration.CodecName);

    return CodecNoError;
}
//}}}

//{{{  DiscardQueuedDecodes
// /////////////////////////////////////////////////////////////////////////
//
//      When discarding queued decodes, poke the monitor task
//

CodecStatus_t   Codec_MmeAudioStream_c::DiscardQueuedDecodes( void )
{
    CodecStatus_t       Status;

    OS_LockMutex (&InputMutex);
    Status              = Codec_MmeBase_c::DiscardQueuedDecodes();
    OS_UnLockMutex (&InputMutex);

    OS_SetEvent (&IssueTransformCommandEvent);

    return Status;
}
//}}}
//{{{  CheckForMarkerFrameStall
// /////////////////////////////////////////////////////////////////////////
//
//      This function checks for a marker frame stall, where we have 1 
//      or more coded frame buffers in the transformer, and 1 or more 
//      decode buffers, and a marker frame waiting, but nothing is going 
//      in or out of the transform.
//

CodecStatus_t   Codec_MmeAudioStream_c::CheckForMarkerFrameStall( void )
{
    Buffer_t                    LocalMarkerBuffer;
    unsigned long long          Now;

    if (MarkerBuffer != NULL)
    {
        Now     = OS_GetTimeInMicroSeconds();

        if (!InPossibleMarkerStallState ||
            (SendBuffersCommandsIssued  != StallStateSendBuffersCommandsIssued) ||
            (TransformCommandsCompleted != StallStateTransformCommandsCompleted))
        {
            TimeOfEntryToPossibleMarkerStallState       = Now;
            StallStateSendBuffersCommandsIssued         = SendBuffersCommandsIssued;
            StallStateTransformCommandsCompleted        = TransformCommandsCompleted;
            InPossibleMarkerStallState                  = true;
        }
        else if ((Now - TimeOfEntryToPossibleMarkerStallState) >= MAXIMUM_STALL_PERIOD)
        {
            LocalMarkerBuffer                           = TakeMarkerBuffer();
            if (LocalMarkerBuffer)
                OutputRing->Insert( (unsigned int)LocalMarkerBuffer );

            InPossibleMarkerStallState                  = false;
        }
    }
    else
    {
        InPossibleMarkerStallState      = false;
    }

    return CodecNoError;
}
//}}}
//{{{  AbortMMECommands
////////////////////////////////////////////////////////////////////////////
/// 
/// Abort any pending MME_TRANSFORM commands.
/// 
CodecStatus_t Codec_MmeAudioStream_c::AbortMMECommands (BufferPool_t CommandContextPool)
{
    BufferStatus_t              Status;
    Buffer_t                    AllocatedBuffers[DEFAULT_COMMAND_CONTEXT_COUNT] = { 0 };
    CodecBaseDecodeContext_t*   OutstandingCommandContexts[DEFAULT_COMMAND_CONTEXT_COUNT] = { 0 };
    unsigned int                NumOutstandingCommands = 0;
    unsigned long long          TimeOut;

    // Scan the local decode context pool to cancel MME_TRANSFORM commands
    if (CommandContextPool == NULL)
    {
        CODEC_TRACE("(%s) No context pool - no commands to abort\n", Configuration.CodecName);
        return CodecNoError;
    }

    // Scan the decode context pool to cancel MME_SEND_BUFFERS commands
    Status                      = CommandContextPool->GetAllUsedBuffers (DEFAULT_COMMAND_CONTEXT_COUNT, AllocatedBuffers, 0);
    if (Status != BufferNoError)
    {
        CODEC_ERROR ("(%s) Could not get handles for in-use buffers\n", Configuration.CodecName);
        return CodecError;
    }

    for (int i = 0; i < DEFAULT_COMMAND_CONTEXT_COUNT; i++)
    {
        if (AllocatedBuffers[i])
        {
            Status              = AllocatedBuffers[i]->ObtainDataReference (NULL, NULL, (void**)&OutstandingCommandContexts[i]);
            if ( Status != BufferNoError)
            {
                CODEC_ERROR ("(%s) Could not get data reference for in-use buffer at %p\n", Configuration.CodecName,
                             AllocatedBuffers[i] );
                return CodecError;
            }

            CODEC_TRACE("Selected command %08x\n", OutstandingCommandContexts[i]->MMECommand.CmdStatus.CmdId);
            NumOutstandingCommands++;
        }
    }

    CODEC_TRACE ("Waiting for %d MME command(s) to abort\n", NumOutstandingCommands);

    TimeOut             = OS_GetTimeInMicroSeconds() + 5000000;

    while (NumOutstandingCommands && OS_GetTimeInMicroSeconds() < TimeOut)
    {
        // Scan for any completed commands
        for (unsigned int i = 0; i < NumOutstandingCommands; /* no iterator */ )
        {
            MME_Command_t      &Command = OutstandingCommandContexts[i]->MMECommand;

            CODEC_TRACE ("Command %08x  State %d\n", Command.CmdStatus.CmdId, Command.CmdStatus.State);

            // It might, perhaps, looks a little odd to check for a zero command identifier here. Basically
            // the callback action calls ReleaseDecodeContext() which will zero the structures. We really
            // ought to use a better techinque to track in-flight commands (and possible move the call to
            // ReleaseDecodeContext() into the Stream playback thread.
            if (0 == Command.CmdStatus.CmdId ||
                MME_COMMAND_COMPLETED == Command.CmdStatus.State ||
                MME_COMMAND_FAILED == Command.CmdStatus.State)
            {
                CODEC_TRACE ("Retiring command %08x\n", Command.CmdStatus.CmdId);
                OutstandingCommandContexts[i]           = OutstandingCommandContexts[NumOutstandingCommands - 1];
                NumOutstandingCommands--;
            }
            else
            {
                i++;
            }
        }

        // Issue the aborts to the co-processor
        for (unsigned int i = 0; i < NumOutstandingCommands; i++)
        {
            MME_Command_t      &Command = OutstandingCommandContexts[i]->MMECommand;

            CODEC_TRACE("Aborting command %08x\n", Command.CmdStatus.CmdId);
            MME_ERROR   Error   = MME_AbortCommand (MMEHandle, Command.CmdStatus.CmdId);
            if (MME_SUCCESS != Error)
            {
                if (MME_INVALID_ARGUMENT &&
                    ((Command.CmdStatus.State == MME_COMMAND_COMPLETED) || (Command.CmdStatus.State == MME_COMMAND_FAILED)))
                    CODEC_TRACE( "Ignored error during abort (command %08x already complete)\n",
                                 Command.CmdStatus.CmdId );
                else
                    CODEC_ERROR ("(%s) Cannot issue abort on command %08x (%d)\n", Configuration.CodecName,
                                 Command.CmdStatus.CmdId, Error );
            }
        }

        // Allow a little time for the co-processor to react
        OS_SleepMilliSeconds(100);
    }


    if (NumOutstandingCommands > 0)
    {
        CODEC_ERROR ("(%s) Timed out waiting for %d MME_SEND_BUFFERS commands to abort\n", Configuration.CodecName, NumOutstandingCommands );
        return CodecError;
    }

    return CodecNoError;
}
//}}}

//{{{  DumpSetStreamParameters
// /////////////////////////////////////////////////////////////////////////
//
//      Function to dump out the set stream
//      parameters from an mme command.
//

CodecStatus_t   Codec_MmeAudioStream_c::DumpSetStreamParameters(           void    *Parameters )
{
    CODEC_ERROR("(%s) Not implemented\n", Configuration.CodecName);
    return CodecNoError;
}
//}}}
//{{{  DumpDecodeParameters
// /////////////////////////////////////////////////////////////////////////
//
//      Function to dump out the decode
//      parameters from an mme command.
//

CodecStatus_t   Codec_MmeAudioStream_c::DumpDecodeParameters(              void    *Parameters )
{
    CODEC_TRACE("%s: TotalSize[0]                  %d\n", __FUNCTION__, DecodeContext->MMEBuffers[0].TotalSize);
    CODEC_TRACE("%s: Page_p[0]                     %p\n", __FUNCTION__, DecodeContext->MMEPages[0].Page_p);
    CODEC_TRACE("%s: TotalSize[1]                  %d\n", __FUNCTION__, DecodeContext->MMEBuffers[1].TotalSize);
    CODEC_TRACE("%s: Page_p[1]                     %p\n", __FUNCTION__, DecodeContext->MMEPages[1].Page_p);

    return CodecNoError;
}
//}}}

//{{{  CallbackFromMME
// /////////////////////////////////////////////////////////////////////////
//
//      Callback function from MME
//
//

void   Codec_MmeAudioStream_c::CallbackFromMME( MME_Event_t Event, MME_Command_t *CallbackData )
{
    CodecBaseDecodeContext_t*           DecodeContext;

    CODEC_DEBUG("Callback!  CmdId %x  CmdCode %s  State %d  Error %d\n",
                CallbackData->CmdStatus.CmdId,
                CallbackData->CmdCode == MME_SET_GLOBAL_TRANSFORM_PARAMS ? "MME_SET_GLOBAL_TRANSFORM_PARAMS" :
                CallbackData->CmdCode == MME_SEND_BUFFERS ? "MME_SEND_BUFFERS" :
                CallbackData->CmdCode == MME_TRANSFORM ? "MME_TRANSFORM" : "UNKNOWN",
                CallbackData->CmdStatus.State,
                CallbackData->CmdStatus.Error);

    Codec_MmeBase_c::CallbackFromMME( Event, CallbackData );

    //
    // Switch to perform appropriate actions per command
    //

    switch( CallbackData->CmdCode )
    {
        case MME_SET_GLOBAL_TRANSFORM_PARAMS:
            break;

        case MME_TRANSFORM:
            if (Event == MME_COMMAND_COMPLETED_EVT)
            {
                TransformCommandsCompleted++;

                OS_SetEvent (&IssueTransformCommandEvent);
            }
            break;

        case MME_SEND_BUFFERS:
            DecodeContext               = (CodecBaseDecodeContext_t *)CallbackData;

            ReleaseDecodeContext (DecodeContext);
            SendBuffersCommandsCompleted++;

            break;

        default:
            break;
    }
}
//}}}
/// Functions below are all used by the transform thread to init, issue and abort
/// MME_TRANSFORM commands.  They all correspond to similar functions used for
/// managing MME_SEND_BUFFERS commands above or in super classes.

//{{{  FillOutTransformContext
////////////////////////////////////////////////////////////////////////////
/// 
/// Fill out class contextual variable required to issue an MME_TRANSFORM command.
///
/// This is the corollary of FillOutDecodeContext although it has significantly more code
/// in order to deal with the Stream buffer pool.
/// 
CodecStatus_t   Codec_MmeAudioStream_c::FillOutTransformContext( void )
{
    CodecStatus_t               Status;
    CodecBufferState_t*         State;
    PlayerSequenceNumber_t*     SequenceNumberStructure;

    // Obtain a new buffer if needed
    if (CurrentDecodeBufferIndex == INVALID_INDEX)
    {
        Status          = TransformCodedFramePool->GetBuffer (&CodedFrameBuffer, UNSPECIFIED_OWNER, 4, false);
        if (Status != BufferNoError)
        {
            CODEC_ERROR("(%s) Failed to get a coded buffer.\n", Configuration.CodecName);
            //CurrentDecodeBuffer->DecrementReferenceCount();
            ReleaseDecodeContext (TransformContext);
            return Status;
        }

        // Set up base parsed frame params pointer to point at our coded frame buffer
        Status          = CodedFrameBuffer->ObtainMetaDataReference (Player->MetaDataParsedFrameParametersType, (void**)(&ParsedFrameParameters));
        if (Status != PlayerNoError)
        {
            CODEC_ERROR("(%s) Failed to get ParsedFrameParameters on new coded frame\n", Configuration.CodecName);
            ReleaseDecodeContext (TransformContext);
            return Status;
        }
        memcpy (ParsedFrameParameters, &SavedParsedFrameParameters, sizeof(ParsedFrameParameters_t));

        Status          = GetDecodeBuffer ();
        CodedFrameBuffer->DecrementReferenceCount ();           // Now attached to decode buffer or failed
        if (Status != CodecNoError)
        {
            CODEC_ERROR("(%s) Failed to get decode buffer.\n", Configuration.CodecName);
            ReleaseDecodeContext (TransformContext);
            return Status;
        }

        // attach coded frame (with copied params) to decode buffer
        Status          = TransformContextBuffer->AttachBuffer (CodedFrameBuffer);
        if (Status != CodecNoError)
        {
            CODEC_ERROR("(%s) Failed to attach new coded frame to decode buffer\n", Configuration.CodecName);
            ReleaseDecodeContext (TransformContext);
            return Status;
        }

        ParsedFrameParameters->DisplayFrameIndex        = CurrentDecodeFrameIndex++;

        // copy the sequence structure across here
        Status          = CodedFrameBuffer->ObtainMetaDataReference (Player->MetaDataSequenceNumberType, (void**)(&SequenceNumberStructure));
        if (Status != PlayerNoError)
        {
            CODEC_ERROR("(%s) Unable to obtain the meta data \"SequenceNumber\" - Implementation    error\n", Configuration.CodecName);
            ReleaseDecodeContext (TransformContext);
            return Status;
        }
        memcpy (SequenceNumberStructure, &SavedSequenceNumberStructure, sizeof(PlayerSequenceNumber_t));

    }
    else
    {
        CODEC_ERROR("(%s) Already have valid CurrentDecodeBufferIndex\n", Configuration.CodecName);
        ReleaseDecodeContext( TransformContext );
        return CodecError;
    }

    // Now back as the corollary of FillOutDecodeContext (the code above is the buffer pool management)

    // Record the buffer being used in the decode context
    TransformContext->BufferIndex                       = CurrentDecodeBufferIndex;

    memset (&TransformContext->MMECommand, 0x00, sizeof(MME_Command_t));

    TransformContext->MMECommand.NumberInputBuffers     = 0;
    TransformContext->MMECommand.NumberOutputBuffers    = 1;
    TransformContext->MMECommand.DataBuffers_p          = TransformContext->MMEBufferList;

    // plumbing
    TransformContext->MMEBufferList[0]                  = &TransformContext->MMEBuffers[0];

    memset (&TransformContext->MMEBuffers[0], 0, sizeof(MME_DataBuffer_t));
    TransformContext->MMEBuffers[0].StructSize          = sizeof(MME_DataBuffer_t);
    TransformContext->MMEBuffers[0].NumberOfScatterPages= 1;
    TransformContext->MMEBuffers[0].ScatterPages_p      = &TransformContext->MMEPages[0];

    memset (&TransformContext->MMEPages[0], 0, sizeof(MME_ScatterPage_t));

     // output
    State                                               = &BufferState[CurrentDecodeBufferIndex];
    if (State->BufferStructure->ComponentCount != 1)
        CODEC_ERROR("(%s) Decode buffer structure contains unsupported number of components (%d).\n", Configuration.CodecName, State->BufferStructure->ComponentCount);

     TransformContext->MMEBuffers[0].TotalSize          = State->BufferLength - State->BufferStructure->ComponentOffset[0];
     TransformContext->MMEPages[0].Page_p               = State->BufferPointer + State->BufferStructure->ComponentOffset[0];
     TransformContext->MMEPages[0].Size                 = State->BufferLength - State->BufferStructure->ComponentOffset[0];

    OS_LockMutex (&Lock);
    TransformContext->DecodeInProgress                                  = true;
    BufferState[CurrentDecodeBufferIndex].DecodesInProgress++;
    BufferState[CurrentDecodeBufferIndex].OutputOnDecodesComplete       = true;
    OS_UnLockMutex (&Lock);

    return CodecNoError;
}
//}}}
//{{{  FillOutTransformCommand
////////////////////////////////////////////////////////////////////////////
/// 
/// Populate an MME_TRANSFORM command.
///
/// This is the corollary of Codec_MmeAudioStream_c::FillOutDecodeCommand .
/// 
CodecStatus_t   Codec_MmeAudioStream_c::FillOutTransformCommand( void )
{
    StreamAudioCodecDecodeContext_t*            Context = (StreamAudioCodecDecodeContext_t*)TransformContext;

    // Initialize the frame parameters (we don't actually have much to say here)
    memset (&Context->DecodeParameters, 0, sizeof(Context->DecodeParameters));

    // Zero the reply structure
    memset (&Context->DecodeStatus, 0, sizeof(Context->DecodeStatus));

    // Fillout the actual command
    Context->BaseContext.MMECommand.StructSize                          = sizeof(MME_Command_t);
    Context->BaseContext.MMECommand.CmdCode                             = MME_TRANSFORM;
    Context->BaseContext.MMECommand.CmdEnd                              = MME_COMMAND_END_RETURN_NOTIFY;
    Context->BaseContext.MMECommand.DueTime                             = 0;

    Context->BaseContext.MMECommand.CmdStatus.AdditionalInfoSize        = sizeof(Context->DecodeStatus);
    Context->BaseContext.MMECommand.CmdStatus.AdditionalInfo_p          = (MME_GenericParams_t)(&Context->DecodeStatus);
    Context->BaseContext.MMECommand.ParamSize                           = sizeof(Context->DecodeParameters);
    Context->BaseContext.MMECommand.Param_p                             = (MME_GenericParams_t)(&Context->DecodeParameters);

    return CodecNoError;
}
//}}}
//{{{  SendMMETransformCommand
////////////////////////////////////////////////////////////////////////////
/// 
/// Issue an MME_TRANSFORM command.
///
/// This is the corollary of Codec_MmeBase_c::SendMMEDecodeCommand from
/// which a substantial quantity of code has been copied.
///
/// \todo Verify the correct action if the component is halted
/// 
CodecStatus_t   Codec_MmeAudioStream_c::SendMMETransformCommand( void )
{
    CodecStatus_t       Status;

    MMECommandPreparedCount++;

    // Check that we have not commenced shutdown.
    if (TestComponentState(ComponentHalted))
    {
        CODEC_ERROR("(%s) Attempting to send Stream transform command when compontent is halted\n", Configuration.CodecName);
        MMECommandAbortedCount++;
        // XXX: This code was refactored from Codec_MmeAudioStream_c::StreamThread(), it used to cause the
        //      imediate death of the thread (instant return), this felt wrong so instead report success and
        //      rely on other components to examine the component state. Once this path has been tested
        //      the CODEC_ERROR() can probably be removed.
        return CodecNoError;
    }

#ifdef DUMP_COMMANDS
    DumpMMECommand (&TransformContext->MMECommand);
#endif

    // Perform the mme transaction - Note at this point we invalidate
    // our pointer to the context buffer, after the send we invalidate
    // out pointer to the context data.

#ifdef __KERNEL__
    flush_cache_all();
#endif

    TransformContextBuffer                      = NULL;
    TransformContext->DecodeCommenceTime        = OS_GetTimeInMicroSeconds();

    Status                                      = MME_SendCommand (MMEHandle, &TransformContext->MMECommand);
    if (Status != MME_SUCCESS)
    {
        CODEC_ERROR("(%s) Unable to send decode command (%08x).\n", Configuration.CodecName, Status);
        Status                                  = CodecError; //return
    }
    else
    {
        TransformContext                        = NULL;
        Status                                  = CodecNoError;
        TransformCommandsIssued++;
    }

    return Status;
}
//}}}
//{{{  FinishedTransform
////////////////////////////////////////////////////////////////////////////
/// 
/// Tidy up the contextual variables after issuing an MME_TRANSFORM command.
///
/// This is the corollary of Codec_MmeAudio_c::FinishedDecode .
/// 
void Codec_MmeAudioStream_c::FinishedTransform( void )
{
    //
    // We have finished decoding into this buffer
    //

    CurrentDecodeBufferIndex        = INVALID_INDEX;
    CurrentDecodeIndex              = INVALID_INDEX;
}
//}}}

//{{{  TransformThread
////////////////////////////////////////////////////////////////////////////
/// 
/// Stream transform management thread.
///
/// \b WARNING: This method is public only to allow it to be called from a
///             C linkage callback. Do not call it directly.
///
/// This is basically an adaptation of Codec_MmeAudio_c::Input to dispatch
/// the TRANSFORM commands to match the SEND_BUFFERS done by the main code
/// that will be driven directly by the Codec_MmeAudio_c::Input.
/// Various items called by Codec_MmeAudio_c::Input are duplicated here to
/// be customised to the requirements or to detach them from member variables
/// that we have duplicated for this half of the process.
///
void Codec_MmeAudioStream_c::TransformThread (void)
{
    CodecStatus_t       Status;
    unsigned int        DecodeContextSize;
    unsigned int        HighWatermarkOfDiscardDecodesUntil = 0;
    int                 BuffersAvailable        = 0;
    int                 TransformsActive        = 0;

    //unsigned int        ContextsInTransformPool, TransformContextsInUse;
    //unsigned int        ContextsInDecodePool, DecodeContextsInUse;

    while (TransformThreadRunning)
    {
        OS_WaitForEvent (&IssueTransformCommandEvent, 1000);
        OS_ResetEvent (&IssueTransformCommandEvent);

        //TransformContextPool->GetPoolUsage (&ContextsInTransformPool, &TransformContextsInUse, NULL, NULL, NULL);
        //DecodeContextPool->GetPoolUsage    (&ContextsInDecodePool, &DecodeContextsInUse, NULL, NULL, NULL);

        //CODEC_TRACE("SendBuffers %d, %d, Contexts %d, %d Transforms %d, %d, Contexts %d, %d\n",
        //         SendBuffersCommandsIssued, SendBuffersCommandsCompleted,
        //         ContextsInDecodePool, DecodeContextsInUse,
        //         TransformCommandsIssued, TransformCommandsCompleted,
        //         ContextsInTransformPool, TransformContextsInUse);

        OS_LockMutex (&InputMutex);

        //{{{  Check if need to discard current data.
        if (DiscardDecodesUntil > HighWatermarkOfDiscardDecodesUntil)
        {
                CODEC_TRACE("Commands prepared %d, CommandsCompleted %d)\n", MMECommandPreparedCount, MMECommandCompletedCount);
                CODEC_TRACE("SendBuffers %d, %d, Transforms %d, %d, \n",
                         SendBuffersCommandsIssued, SendBuffersCommandsCompleted,
                         TransformCommandsIssued, TransformCommandsCompleted);

                HighWatermarkOfDiscardDecodesUntil      = DiscardDecodesUntil;

                Status          = AbortMMECommands (TransformContextPool);
                if( Status != CodecNoError )
                {
                     CODEC_ERROR("(%s) Could not abort all pending MME_TRANSFORM comamnds\n", Configuration.CodecName);
                     // no recovery possible
                }

                //Status      = AbortSendBuffersCommands();
                OS_LockMutex (&DecodeContextPoolMutex);
                Status          = AbortMMECommands (DecodeContextPool);
                OS_UnLockMutex (&DecodeContextPoolMutex);
                if( Status != CodecNoError )
                {
                     CODEC_ERROR("(%s) Could not abort all pending MME_SEND_BUFFERS comamnds\n", Configuration.CodecName);
                     // no recovery possible
                }

                // Stream transform gets confused over PTS generation when we abort send buffers
                // So we terminate and restart the mme transform, we also tidy up state to avoid
                // any negative repercusions of this act.

                CODEC_TRACE("Terminating transformer\n");
                Status          = TerminateMMETransformer();
                if( Status == CodecNoError )
                    Status      = InitializeMMETransformer();

                if( Status != CodecNoError )
                {
                     CODEC_ERROR("(%s) Could not terminate and restart the Stream transformer\n", Configuration.CodecName);
                     // no recovery possible
                }

                ForceStreamParameterReload          = true;
                HighWatermarkOfDiscardDecodesUntil  = 0;
                DiscardDecodesUntil                 = 0;

                // We now expect a jump in the pts time, so we avoid any unsavoury errors, by invalidating our record
                LastNormalizedPlaybackTime          = UNSPECIFIED_TIME;
        }
        //}}}

        //{{{  Conditionally issue a new MME_TRANSFORM command
        BuffersAvailable            = SendBuffersCommandsIssued - SendBuffersCommandsCompleted;
        TransformsActive            = TransformCommandsIssued - TransformCommandsCompleted;
        while (((BuffersAvailable - TransformsActive) >= SendbufTriggerTransformCount) &&
               (!TestComponentState(ComponentHalted)) && TransformThreadRunning &&
               TransformCodedFramePool)
        {
            if (TransformContextBuffer != NULL)
            {
                CODEC_ERROR("(%) Already have a decode context.\n", Configuration.CodecName);
                break;
            }

            Status          = TransformContextPool->GetBuffer (&TransformContextBuffer);
            if (Status != BufferNoError)
            {
                CODEC_ERROR("(%s) Fail to get decode context.\n", Configuration.CodecName);
                break;
            }

            TransformContextBuffer->ObtainDataReference (&DecodeContextSize, NULL, (void**)&TransformContext);
            memset (TransformContext, 0x00, DecodeContextSize);

            TransformContext->DecodeContextBuffer   = TransformContextBuffer;

            Status          = FillOutTransformContext();
            if (Status != CodecNoError)
            {
                CODEC_ERROR("(%s) Cannot fill out transform context\n", Configuration.CodecName);
                break;
            }

            Status          = FillOutTransformCommand();
            if (Status != CodecNoError)
            {
                CODEC_ERROR("(%s) Cannot fill out transform command\n", Configuration.CodecName);
                break;
            }

            // Ensure that the coded frame will be available throughout the 
            // life of the decode by attaching the coded frame to the decode
            // context prior to launching the decode.
            TransformContextBuffer->AttachBuffer (CodedFrameBuffer);

            Status          = SendMMETransformCommand ();
            if (Status != CodecNoError)
            {
                CODEC_ERROR("(%s) Failed to send a transform command.\n", Configuration.CodecName);
                ReleaseDecodeContext (TransformContext);
                //! NULL them here, as ReleaseDecodeContext will only do for the MME_SEND_BUFFERS done with DecodeContext/etc
                TransformContext                    = NULL;
                TransformContextBuffer              = NULL;
                break;
            }

            FinishedTransform();

            BuffersAvailable                        = SendBuffersCommandsIssued - SendBuffersCommandsCompleted;
            TransformsActive                        = TransformCommandsIssued - TransformCommandsCompleted;
        }
        //}}}

        CheckForMarkerFrameStall();

        OS_UnLockMutex (&InputMutex);

    } //while (TransformThreadRunning)

    // About to terminate, make sure there are no pending commands.
    //Status      = AbortTransformCommands();
    Status      = AbortMMECommands (TransformContextPool);
    if (Status != CodecNoError)
        CODEC_ERROR("(%s)Could not abort all pending MME_TRANSFORM comamnds (resources will leak)\n", Configuration.CodecName);


    //Status      = AbortSendBuffersCommands();
    OS_LockMutex (&DecodeContextPoolMutex);
    Status      = AbortMMECommands (DecodeContextPool);
    OS_UnLockMutex (&DecodeContextPoolMutex);
    if (Status != CodecNoError)
        CODEC_ERROR("(%s)Could not abort all pending MME_SEND_BUFFERS comamnds (resources will leak)\n", Configuration.CodecName);

    CODEC_TRACE("Terminating transform thread\n");
    OS_SetEvent (&TransformThreadTerminated);
}
//}}}

#if 0
// Should be deleted when everything stable
//{{{  AbortTransformCommands
////////////////////////////////////////////////////////////////////////////
/// 
/// Abort any pending MME_TRANSFORM commands.
///
CodecStatus_t Codec_MmeAudioStream_c::AbortTransformCommands( void )
{
    Buffer_t                    ArrayOfBuffers[2] = { 0, 0 };
    unsigned int                DecodeContextSize;
    CodecBaseDecodeContext_t*   HaltDecodeContext1 = 0;
    CodecBaseDecodeContext_t*   HaltDecodeContext2 = 0;

    // Scan the local decode context pool to cancel MME_TRANSFORM commands
    if (TransformContextPool == NULL)
    {
        CODEC_TRACE("(%s) No transform context pool - no commands to abort\n", Configuration.CodecName);
        return CodecNoError;
    }

    TransformContextPool->GetAllUsedBuffers (2, ArrayOfBuffers, 0);

    if (ArrayOfBuffers[0])
    {
        ArrayOfBuffers[0]->ObtainDataReference (&DecodeContextSize, NULL, (void**)&HaltDecodeContext1);
    }

    if(ArrayOfBuffers[1])
    {
        ArrayOfBuffers[1]->ObtainDataReference (&DecodeContextSize, NULL, (void**)&HaltDecodeContext2);
    }

    //check if we should swap order of inspection
    if (HaltDecodeContext1 && HaltDecodeContext2)
    {
        if (HaltDecodeContext1->MMECommand.CmdStatus.CmdId < HaltDecodeContext2->MMECommand.CmdStatus.CmdId)
        {
            CodecBaseDecodeContext_t*   Temp    = HaltDecodeContext1;
            HaltDecodeContext1                  = HaltDecodeContext2;
            HaltDecodeContext2                  = Temp;
        }
    }

    //now complete/cancel them, newest first
    if (HaltDecodeContext1)
    {
        int delay = 20;
        CODEC_TRACE("waiting for MME_TRANSFORM to complete\n" );
        while (delay-- && (HaltDecodeContext1->MMECommand.CmdStatus.State < MME_COMMAND_COMPLETED))
        {
            OS_SleepMilliSeconds (10);
        }
        while (HaltDecodeContext1->MMECommand.CmdStatus.State < MME_COMMAND_COMPLETED)
        {
            CODEC_TRACE("waiting for MME_TRANSFORM to abort\n" );
            MME_AbortCommand (MMEHandle, HaltDecodeContext1->MMECommand.CmdStatus.CmdId );
            OS_SleepMilliSeconds (10);
        }
    }

    if (HaltDecodeContext2)
    {
        int delay = 20;
        CODEC_TRACE("waiting for MME_TRANSFORM to complete\n" );
        while (delay-- && (HaltDecodeContext2->MMECommand.CmdStatus.State < MME_COMMAND_COMPLETED))
        {
            OS_SleepMilliSeconds (10);
        }

        while(HaltDecodeContext2->MMECommand.CmdStatus.State < MME_COMMAND_COMPLETED)
        {
            CODEC_TRACE("waiting for MME_TRANSFORM to abort\n");
            MME_AbortCommand (MMEHandle, HaltDecodeContext2->MMECommand.CmdStatus.CmdId );
            OS_SleepMilliSeconds (10);
        }
    }

    return CodecNoError;
}
//}}}
//{{{  AbortSendBuffersCommands
////////////////////////////////////////////////////////////////////////////
/// 
/// Abort any pending MME_SEND_BUFFERS commands.
/// 
CodecStatus_t Codec_MmeAudioStream_c::AbortSendBuffersCommands( void )
{
    BufferStatus_t              Status;
    Buffer_t                    AllocatedDecodeBuffers[DEFAULT_SENDBUF_DECODE_CONTEXT_COUNT] = { 0 };
    CodecBaseDecodeContext_t*   OutstandingDecodeCommandContexts[DEFAULT_SENDBUF_DECODE_CONTEXT_COUNT] = { 0 };
    unsigned int                NumOutstandingCommands = 0;
    unsigned long long          TimeOut;


    OS_AutoLockMutex mutex (&DecodeContextPoolMutex);

    if (DecodeContextPool == NULL)
    {
        CODEC_TRACE("(%s) No decode context pool - no commands to abort\n", Configuration.CodecName);
        return CodecNoError;
    }

    // Scan the decode context pool to cancel MME_SEND_BUFFERS commands
    Status                      = DecodeContextPool->GetAllUsedBuffers (DEFAULT_SENDBUF_DECODE_CONTEXT_COUNT, AllocatedDecodeBuffers, 0);
    if (Status != BufferNoError)
    {
        CODEC_ERROR ("(%s) Could not get handles for in-use buffers\n", Configuration.CodecName);
        return CodecError;
    }

    for (int i = 0; i < DEFAULT_SENDBUF_DECODE_CONTEXT_COUNT; i++)
    {
        if (AllocatedDecodeBuffers[i])
        {
            Status              = AllocatedDecodeBuffers[i]->ObtainDataReference (NULL, NULL, (void**)&OutstandingDecodeCommandContexts[i]);
            if ( Status != BufferNoError)
            {
                CODEC_ERROR ("(%s) Could not get data reference for in-use buffer at %p\n", Configuration.CodecName,
                             AllocatedDecodeBuffers[i] );
                return CodecError;
            }

            CODEC_TRACE("Selected command %08x\n", OutstandingDecodeCommandContexts[i]->MMECommand.CmdStatus.CmdId);
            NumOutstandingCommands++;
        }
    }

    CODEC_TRACE ("Waiting for %d MME_SEND_BUFFERS commands to abort\n", NumOutstandingCommands);

    TimeOut             = OS_GetTimeInMicroSeconds() + 5000000;

    while (NumOutstandingCommands && OS_GetTimeInMicroSeconds() < TimeOut)
    {
        // Scan for any completed commands
        for (unsigned int i = 0; i < NumOutstandingCommands; /* no iterator */ )
        {
            MME_Command_t      &Command = OutstandingDecodeCommandContexts[i]->MMECommand;

            CODEC_TRACE ("Command %08x  State %d\n", Command.CmdStatus.CmdId, Command.CmdStatus.State);

            // It might, perhaps, looks a little odd to check for a zero command identifier here. Basically
            // the callback action calls ReleaseDecodeContext() which will zero the structures. We really
            // ought to use a better techinque to track in-flight commands (and possible move the call to
            // ReleaseDecodeContext() into the Stream playback thread.
            if (0 == Command.CmdStatus.CmdId ||
                MME_COMMAND_COMPLETED == Command.CmdStatus.State ||
                MME_COMMAND_FAILED == Command.CmdStatus.State)
            {
                CODEC_TRACE ("Retiring command %08x\n", Command.CmdStatus.CmdId);
                OutstandingDecodeCommandContexts[i]     = OutstandingDecodeCommandContexts[NumOutstandingCommands - 1];
                NumOutstandingCommands--;
            }
            else
            {
                i++;
            }
        }

        // Issue the aborts to the co-processor
        for (unsigned int i = 0; i < NumOutstandingCommands; i++)
        {
            MME_Command_t      &Command = OutstandingDecodeCommandContexts[i]->MMECommand;

            CODEC_TRACE("Aborting command %08x\n", Command.CmdStatus.CmdId);

            MME_ERROR   Error   = MME_AbortCommand (MMEHandle, Command.CmdStatus.CmdId );
            if (MME_SUCCESS != Error)
            {
                if (MME_INVALID_ARGUMENT &&
                    ((Command.CmdStatus.State == MME_COMMAND_COMPLETED) || (Command.CmdStatus.State == MME_COMMAND_FAILED)))
                    CODEC_TRACE( "Ignored error during abort (command %08x already complete)\n",
                                 Command.CmdStatus.CmdId );
                else
                    CODEC_ERROR ("(%s) Cannot issue abort on command %08x (%d)\n", Configuration.CodecName,
                                 Command.CmdStatus.CmdId, Error );
            }
        }

        // Allow a little time for the co-processor to react
        OS_SleepMilliSeconds(10);
    }


    if (NumOutstandingCommands > 0)
    {
        CODEC_ERROR ("(%s) Timed out waiting for %d MME_SEND_BUFFERS commands to abort\n", Configuration.CodecName, NumOutstandingCommands );
        return CodecError;
    }

    return CodecNoError;
}
//}}}
#endif
