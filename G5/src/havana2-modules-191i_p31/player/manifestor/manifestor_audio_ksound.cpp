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

Source file name : manifestor_audio_ksound.cpp
Author :           Daniel

Implementation of the in-kernel ALSA library manifestor


Date        Modification                                    Name
----        ------------                                    --------
14-May-07   Created (from manifestor_video_stmfb.cpp)       Daniel

************************************************************************/

#include <ACC_Transformers/Audio_DecoderTypes.h>
#include <ACC_Transformers/Mixer_ProcessorTypes.h>

#include "osinline.h"
#include "osdev_user.h"
#include "output_timer_audio.h"
#include "manifestor_audio_ksound.h"
#include "pcmplayer.h"
#include "ksound.h"
#include "mixer_mme.h"
#include "player_generic.h"

#include "../codec/codec_mme_base.h"

// /////////////////////////////////////////////////////////////////////////
//
// Tuneable constants
//

#define MAX_TIME_TO_GET_AUDIO_DELAY (500ull)

#undef  MANIFESTOR_TAG
#define MANIFESTOR_TAG          "Manifestor_AudioKsound_c::"

extern "C" unsigned int player_sysfs_get_err_threshold();

// /////////////////////////////////////////////////////////////////////////
//
// static definitions
//
static const char *LookupMixerInputState(unsigned int x)
{
    switch( x )
    {
#define X(y) case y: return #y
    X(MIXER_INPUT_NOT_RUNNING);
    X(MIXER_INPUT_RUNNING);
    X(MIXER_INPUT_FADING_OUT);
    X(MIXER_INPUT_MUTED);
    X(MIXER_INPUT_PAUSED);
    X(MIXER_INPUT_FADING_IN);
#undef X
    }

    return "UNKNOWN STATE";
}
////////////////////////////////////////////////////////////////////////////
///
/// Initialize and reset the ksound manifestor.
///
/// Like pretty much all player2 components the contructor does little work
/// and the constructed object has a very light memory footprint. This is
/// the almost all allocation is performed by methods called after
/// construction. In our case most if this work happens in
/// Manifestor_AudioKsound_c::OpenOutputSurface().
///
Manifestor_AudioKsound_c::Manifestor_AudioKsound_c  ()
{
    MANIFESTOR_DEBUG (">><<\n");
    
    if (InitializationStatus != ManifestorNoError)
    {
        MANIFESTOR_ERROR ("Initialization status not valid - aborting init\n");
        return;
    }

    if (OS_InitializeMutex (&DisplayTimeOfNextCommitMutex) != OS_NO_ERROR)
    {
        MANIFESTOR_ERROR ("Unable to create the mixer command mutex\n");
        InitializationStatus = ManifestorError;
        return;
    }
    
    Reset ();
}

////////////////////////////////////////////////////////////////////////////
///
/// Trivial destruction of the ksound manifestor.
///
/// Unlike the contructor (which does very little allocation) the destructor
/// has far more work to do but this is almost entirely handled by the
/// Manifestor_AudioKsound_c::Halt() method.
///
Manifestor_AudioKsound_c::~Manifestor_AudioKsound_c   (void)
{
    MANIFESTOR_DEBUG (">><<\n");

    (void) Manifestor_AudioKsound_c::Halt ();
    // no useful error recovery can be performed...
    
    OS_TerminateMutex(&DisplayTimeOfNextCommitMutex);
}


////////////////////////////////////////////////////////////////////////////
///
/// Shutdown, stop presenting and retrieving frames.
///
/// Blocks until complete. The primarily cause of blocking will be caused when
/// we wait for the manifestion thread, executing Manifestor_AudioKsound_c::PlaybackThread(),
/// to observe the request to terminate. 
///  
ManifestorStatus_t      Manifestor_AudioKsound_c::Halt (void)
{
    PlayerStatus_t PStatus;
    ManifestorStatus_t Status;
    
    if( EnabledWithMixer )
    {
        PStatus = Mixer->DisableManifestor( this );
        if( PStatus != ManifestorNoError )
        {
    	    MANIFESTOR_ERROR("Failed to disable manifestor\n");
    	    // non-fatal - it probably means that we were not already registered
        }
        EnabledWithMixer = false;
        OutputState = STOPPED;
    }

    Status = Manifestor_Audio_c::Halt();
    if( Status != ManifestorNoError )
    {
        MANIFESTOR_ERROR("Failed to halt parent - aborting\n");
        return Status;
    }
    
    CloseOutputSurface ();
    
    return ManifestorNoError;
}


////////////////////////////////////////////////////////////////////////////
///
/// Completely reset the class allowing it to be reused in a different
/// configuration.
///
/// This method, in fact, performs most of the work traditionally associated
/// with a constructor (although without being able to use initializers).
///
/// On of the important roles of this method is to pre-configure the complex
/// data associated with the mixer command. In particular it sets up the
/// MME_DataBuffer_t structures with a sufficient number of scatter pages to
/// linearize the mixer inputs. This simplifies the configuration of mixer
/// commands by reducing the amount of on-going setup that is required.
///
ManifestorStatus_t Manifestor_AudioKsound_c::Reset (void)
{
    MANIFESTOR_DEBUG (">><<\n");
    
    if (TestComponentState (ComponentRunning))
        Halt ();

    RegisteredWithMixer = false;
    EnabledWithMixer = false;
    
    SamplesQueuedForDisplayAfterDisplayTimeOfNextCommit = 0;
    SamplesPlayedSinceDisplayTimeOfNextCommitWasCalculated = 0;
    SamplesToRemoveWhenPlaybackCommences = 0;
    ReleaseAllInputBuffersDuringUpdate = false;
    DisplayTimeOfNextCommit = INVALID_TIME;
    LastDisplayTimeOfNextCommit = INVALID_TIME;
    
    memset( &InputAudioParameters, 0, sizeof( InputAudioParameters ) );

    OutputChannelCount = 2;
    OutputSampleDepthInBytes = 4;
    
    LastActualSystemPlaybackTime = INVALID_TIME;
    
    SamplesUntilNextCodedDataRepetitionPeriod = 0;
    FirstCodedDataBufferPartiallyConsumed = false;
    IsCodedDataPlaying = false;
    CurrentCodedDataEncoding = PcmPlayer_c::OUTPUT_IEC60958;
    CurrentCodedDataRepetitionPeriod = 1;
    LastCodedDataSyncOffset = 0;
    
    SamplesNeededForFadeOutBeforeResampling = SamplesNeededForFadeOutAfterResampling;
    
    OutputState = STOPPED;

    IsTranscoded = false;

    CodedFrameSampleCount = 0;

    errorFrameSeen = false;
    numGoodFrames = 0;
    digitalFlag = false;

//
    
    return Manifestor_Audio_c::Reset ();
}


////////////////////////////////////////////////////////////////////////////
///
/// Interfere with the module parameters in order to allocate over-sized decode buffers.
///
/// We currently use the host processor to implement an SRC-like approach to stretching
/// or compressing time. This requires extra space in the decode buffers to perform
/// in-place SRC.
///
ManifestorStatus_t      Manifestor_AudioKsound_c::SetModuleParameters (unsigned int   ParameterBlockSize,
                                                                 void*          ParameterBlock )
{
    ManifestorStatus_t Status;
    
    if( ParameterBlockSize == sizeof( ManifestorAudioParameterBlock_t ) )
    {
        ManifestorAudioParameterBlock_t *ManifestorParameters;
        
        ManifestorParameters = (ManifestorAudioParameterBlock_t *) ParameterBlock;
        
        if( ManifestorParameters->ParameterType == ManifestorAudioMixerConfiguration )
        {
            Mixer = (Mixer_Mme_c *) ManifestorParameters->Mixer;
            return ManifestorNoError;
        }
        else if( ManifestorParameters->ParameterType == ManifestorAudioSetEmergencyMuteState )
        {
            if( EnabledWithMixer )
            {
        	return Mixer->SetManifestorEmergencyMuteState( this, ManifestorParameters->EmergencyMute );
            }
            else
            {
        	MIXER_ERROR( "Cannot set mute state when not registered with the mixer\n" );
        	return ManifestorError;
            }
        }
    }
    
    if( ParameterBlockSize == sizeof( OutputTimerParameterBlock_t ) )
    {
	OutputTimerParameterBlock_t *OutputTimerParameters;
	
	OutputTimerParameters = (OutputTimerParameterBlock_t *) ParameterBlock;
	
	if( OutputTimerParameters->ParameterType == OutputTimerSetNormalizedTimeOffset )
	{
	    // if the manifestor is not yet running we can ignore this update (which would be unsafe because
	    // we might not yet have an output timer attached; it will be re-sent when we enable ourselves
	    // with the mixer
	    if (! TestComponentState( ComponentRunning ) )
	    {
		MANIFESTOR_DEBUG( "Ignoring sync offset update (manifestor is not running)\n");
		return ManifestorNoError;
	    }

	    MANIFESTOR_DEBUG( "Forwarding sync offset of %d\n", OutputTimerParameters->Offset.Value );
	    // delegate to the output timer (the mixer proxy doesn't have access to all the player
	    // components but it can poke the manifestor whenever the mixer settings are changed).
	    return OutputTimer->SetModuleParameters( ParameterBlockSize, ParameterBlock );
	}
    }
    if( ParameterBlockSize == sizeof( CodecParameterBlock_t ) )
    {
	CodecParameterBlock_t *CodecParameters;
	
	CodecParameters = (CodecParameterBlock_t *) ParameterBlock;
	
	if( CodecParameters->ParameterType == CodecSpecifyDRC )
	{
	    // as above we discard the update if the manifestor is not yet running
	    if (! TestComponentState( ComponentRunning ) )
	    {
		MANIFESTOR_DEBUG( "Ignoring DRC update (manifestor is not running)\n");
		return ManifestorNoError;
	    }
	    
	    MANIFESTOR_DEBUG( "Forwarding DRC info {Enable = %d / Type = %d / HDR = %d / LDR = %d } \n", CodecParameters->DRC.Enable, CodecParameters->DRC.Type, CodecParameters->DRC.HDR, CodecParameters->DRC.LDR );
	    return Codec->SetModuleParameters( ParameterBlockSize, ParameterBlock );
	}

	if( CodecParameters->ParameterType == CodecSpecifyDownmix )
	{
	    // as above we discard the update if the manifestor is not yet running
	    if (! TestComponentState( ComponentRunning ) )
	    {
		MANIFESTOR_DEBUG( "Ignoring downmix update (manifestor is not running)\n");
		return ManifestorNoError;
	    }
	    
	    MANIFESTOR_DEBUG( "Forwarding downmix promotion settings to codec\n" );
	    return Codec->SetModuleParameters( ParameterBlockSize, ParameterBlock );
	}
    } 
    
    Status = Manifestor_Audio_c::SetModuleParameters(ParameterBlockSize, ParameterBlock);
    return Status;
}


////////////////////////////////////////////////////////////////////////////
///
/// Allocate and initialize the decode buffer if required, and pass them to the caller.
/// 
/// \param Pool         Pointer to location for buffer pool pointer to hold
///                     details of the buffer pool holding the decode buffers.
/// \return             Succes or failure
///
ManifestorStatus_t      Manifestor_AudioKsound_c::GetDecodeBufferPool         (class BufferPool_c**   Pool)
{
ManifestorStatus_t Status;
PlayerStatus_t PStatus;

    Status = Manifestor_Audio_c::GetDecodeBufferPool(Pool);
    if( Status != ManifestorNoError )
    {
    	return Status;
    }
    
    // We are now ComponentRunning...
    
    if (!EnabledWithMixer)
    {
        memset( &InputAudioParameters, 0, sizeof( InputAudioParameters ) );

        PStatus = Mixer->EnableManifestor(this);
        if( PStatus != PlayerNoError )
        {
    	    MANIFESTOR_ERROR("Failed to enable manifestor\n");
    	    return ManifestorError;
        }
        EnabledWithMixer = true;
        OutputState = STARTING;
    }
    
    return ManifestorNoError;
}


////////////////////////////////////////////////////////////////////////////
///
/// Open the ALSA output device or devices and initialize the mixer transformer.
///
/// It is in this method that the bulk of the memory allocation will be performed.
///
ManifestorStatus_t Manifestor_AudioKsound_c::OpenOutputSurface ()
{
PlayerStatus_t Status;
    
    MANIFESTOR_DEBUG (">><<\n");

    Status = Mixer->RegisterManifestor( this );
    if( Status != PlayerNoError )
    {
        MANIFESTOR_ERROR( "Cannot register with the mixer\n");
        return ManifestorError;
    }
    RegisteredWithMixer = true;
    
    return ManifestorNoError;
}


////////////////////////////////////////////////////////////////////////////
///
/// Release all ALSA and MME resources.
///
ManifestorStatus_t Manifestor_AudioKsound_c::CloseOutputSurface (void)
{
PlayerStatus_t Status;

    MANIFESTOR_DEBUG (">><<\n");
    
    MANIFESTOR_ASSERT( !EnabledWithMixer );
    
    if( RegisteredWithMixer )
    {
        Status = Mixer->DeRegisterManifestor( this );
        if( Status != PlayerNoError )
        {
            MANIFESTOR_ERROR( "Failed to deregister with the mixer\n" );
            // no recovery possible
        }
        RegisteredWithMixer = false;
    }

    return ManifestorNoError;
}

////////////////////////////////////////////////////////////////////////////
///
/// Estimate the time that the next frame enqueued will be displayed.
///
/// If we are called before the first frame has been queued for manifestation we
/// don't know what our natural sample rate is. We therefore must report time assuming
/// the latency of a worst case (lowest sampling frequency) startup and a ALSA buffer
/// length of 3 mixer granules.
///
/// Once started (but before performing the first write to an ALSA device) we don't
/// have an estimate for the display time of the next commit. Thus we still have to guess
/// the ALSA delay (as 3 mixer granules) but do at least know what the presentation rate
/// will be.
///
/// In all other states we have both an estimate of the display time of the next commit and
/// an accurate count of the number of samples to be presented after that point. Providing
/// ALSA itselt never starves (i.e. we stuff silence in on input starvation) the calculated
/// value will be valid.
///
ManifestorStatus_t Manifestor_AudioKsound_c::GetNextQueuedManifestationTime (unsigned long long* Time)
{
unsigned long long DisplayTime;
unsigned long long WallTime;

    WallTime = OS_GetTimeInMicroSeconds();
    
    // must use switch (or copy OututState) otherwise read may not be atomic
    switch (OutputState)
    {
    case STARTING:
    	// we don't know the sampling frequency so the worst case is triple buffered
    	// output(s) at 32KHz
    	DisplayTime = WallTime + Mixer->GetWorstCaseStartupDelayInMicroSeconds();
    	break;
    
    case MUTED:
    case STARVED:
    case PLAYING:
    	// by combining the calculated display time of the next commit with the number of
    	// samples in the buffer queue we can calculate

    	OS_LockMutex(&DisplayTimeOfNextCommitMutex);

    	if (DisplayTimeOfNextCommit < WallTime)
    	{
    	    // when the manifestor is functioning correctly taking this branch in impossible
    	    // (hence the prominent error report). unfortunately we currently are deliberately
    	    // running a 'broken' manifestor without silence injection on starvation making
    	    // the use of this code inevitable.
    	    MANIFESTOR_ERROR("DisplayTimeOfNextCommit has illegal (historic) value, deploying workaround\n");
    	    DisplayTimeOfNextCommit = WallTime;
	    if( InputAudioParameters.Source.SampleRateHz )
		DisplayTimeOfNextCommit += ( (MIXER_NUM_PERIODS * 1000000ull * Mixer->GetMixerGranuleSize()) /
                                        InputAudioParameters.Source.SampleRateHz );
     	}
	if(  InputAudioParameters.Source.SampleRateHz == 0 )
	{
	    DisplayTime = DisplayTimeOfNextCommit + Mixer->GetWorstCaseStartupDelayInMicroSeconds();
	}else 
    	    DisplayTime = DisplayTimeOfNextCommit +
    	          ((((unsigned long long) SamplesQueuedForDisplayAfterDisplayTimeOfNextCommit) * 1000000ull) /
    	            ((unsigned long long) InputAudioParameters.Source.SampleRateHz));
    	            
    	OS_UnLockMutex(&DisplayTimeOfNextCommitMutex);
    	break;
    
    default:
    	MANIFESTOR_ERROR("Unexpected OutputState %s\n", LookupState());
    	return ManifestorError;
    }
    
    MANIFESTOR_DEBUG("At %llx (%s) next queued manifestation time was %llx (decimal delta %lld)\n",
                     WallTime, LookupState(), DisplayTime, DisplayTime - WallTime);
    *Time = DisplayTime;
    return ManifestorNoError;
}


////////////////////////////////////////////////////////////////////////////
///
/// Handle a super-class notification that a buffer is about to be queued.
///
ManifestorStatus_t Manifestor_AudioKsound_c::QueueBuffer        (unsigned int                    BufferIndex)
{
    ManifestorStatus_t Status;
    ParsedFrameParameters_t *FrameParameters = StreamBuffer[BufferIndex].FrameParameters;

    /* Hysterisis:
     * 1. Stop queuing samples on decode error.
     * 2. Keep counting good frames on no decode error.
     * 3. When the good frame count reaches threshold start queuing.
     */

    if ( !digitalFlag )
    {
        ParsedAudioParameters_t *AudioParameters = StreamBuffer[BufferIndex].AudioParameters;
        if ( AudioParameters->decErrorStatus )
        {
            MANIFESTOR_ERROR( "Skipping bad frame\n");
            errorFrameSeen = true;
            numGoodFrames  = 0;
            return ManifestorError;
        }
        else
        {
            ++numGoodFrames;
            if ( numGoodFrames > player_sysfs_get_err_threshold())
            {
                errorFrameSeen = false;
            }
            else if (errorFrameSeen)
            {
                MANIFESTOR_ERROR( "Skipping good frames %d \n", numGoodFrames );
                return ManifestorError;
            }
        }
    }

    //
    // Dump the first four samples of the buffer (assuming it to be a ten channel buffer)
    //
    if (0) {
        unsigned char *Data = StreamBuffer[BufferIndex].Data;
        ParsedAudioParameters_t *AudioParameters = StreamBuffer[BufferIndex].AudioParameters;
        unsigned int *pcm = (unsigned int *) Data;
        report( severity_info, "Manifestor_AudioKsound_c::QueueDecodeBuffer - Queueing %4d - Normalized playback time %lluus\n",
	                   FrameParameters->DisplayFrameIndex, FrameParameters->NormalizedPlaybackTime );
        for (unsigned int i=0; i<4; i++) {
	    unsigned int j = AudioParameters->Source.ChannelCount * i;
	    report (severity_info, "Sample[%3d] (24-bits): %06x %06x %06x %06x %06x %06x %06x %06x %06x %06x\n", i,
	                           pcm[j+0]>>8, pcm[j+1]>>8, pcm[j+2]>>8, pcm[j+3]>>8, pcm[j+4]>>8,
	                           pcm[j+5]>>8, pcm[j+6]>>8, pcm[j+7]>>8, pcm[j+8]>>8, pcm[j+9]>>8);
        }
    }

    //
    // Maintain the number of queued samples (used by GetNextQueuedManifestationTime)
    //

    OS_LockMutex(&DisplayTimeOfNextCommitMutex);
    SamplesQueuedForDisplayAfterDisplayTimeOfNextCommit +=
    		    StreamBuffer[BufferIndex].AudioParameters->SampleCount;
    OS_UnLockMutex(&DisplayTimeOfNextCommitMutex);

    //
    // If we haven't set the input parameters then peek at the parameters before
    // enqueuing them. 
    //
    
    if( 0 == InputAudioParameters.Source.SampleRateHz )
    {
        Status = UpdateAudioParameters( BufferIndex );
        if( Status == ManifestorNoError )
        {
            StreamBuffer[BufferIndex].UpdateAudioParameters = false;
        }
        else
        {
            MANIFESTOR_ERROR("Failed to peek at the audio parameters\n");
        }
    }

    return ManifestorNoError;
}


////////////////////////////////////////////////////////////////////////////
///
/// Handle a super-class notification that a buffer is about to be released.
///
ManifestorStatus_t Manifestor_AudioKsound_c::ReleaseBuffer(unsigned int BufferIndex)
{
	
    //
    // Maintain the number of queued samples (used by GetNextQueuedManifestationTime)
    //
    
    OS_LockMutex(&DisplayTimeOfNextCommitMutex);
    SamplesQueuedForDisplayAfterDisplayTimeOfNextCommit -=
    		    StreamBuffer[BufferIndex].AudioParameters->SampleCount;
    OS_UnLockMutex(&DisplayTimeOfNextCommitMutex);

    return ManifestorNoError;
}

////////////////////////////////////////////////////////////////////////////
///
/// Soft mute and lock the output frequency synthesizers.
///
/// This method may appear to be unimplemented but 'oh no sirree Bob'. If
/// the mixer starves it will soft mute and latch the output of the frequency
/// synthesizers all by itself.
///
ManifestorStatus_t      Manifestor_AudioKsound_c::QueueNullManifestation (void)
{
    MANIFESTOR_DEBUG(">><<\n");

    return ManifestorNoError;
}


////////////////////////////////////////////////////////////////////////////
///
/// Flushes the display queue so buffers not yet manifested are returned.
///
/// \todo Not implemented. Should set a flag that the playback thread will observe
///       and act upon, then wait for an event.
/// 
ManifestorStatus_t      Manifestor_AudioKsound_c::FlushDisplayQueue (void)
{
    MANIFESTOR_DEBUG(">><<\n");

    return ManifestorNoError;
}


////////////////////////////////////////////////////////////////////////////
///
/// Extract buffers from the queue and encode them as scatter pages.
///
/// This method is ultimately responsible for applying percussive adjustments.
/// We do this by constructing a buffer queue that contains all the samples
/// in the original buffers and then processing it prior to issuing it to
/// remove samples from or inject samples into it.
///
/// The following are the means by which samples may be injected or removed:
///
/// - Throw away samples at a single point (reduce previous buffer size) and
///   de-click using MIXER_FOFI.
/// - Throw away samples at the end of the buffer by issuing a MIXER_PAUSE on
///   the final sample of the buffer This method can only be deployed when in the
///   PLAYING output state.
/// - Throw away samples at the beginning of the buffer. This method can only
///   be deployed when the output state is silent (MUTED or STARVING)
/// - Inject silent samples at the end of a buffer by issuing MIXER_PAUSE at a
///   point before the natural end of the buffer. This method can only be used
///   when the stream is in the PLAYING state.
/// - Inject silent samples at the beginning of a buffer by issuing MIXER_PLAY
///   at a point before the natural start of the buffer. This method can only be
///   used when the stream is in a silent state (MUTED or STARVING).
///
/// \param DataBuffer         The MME data buffer structure to be populated with input buffers.
/// \param MixerFrameParams   The mixer command structure associated with the stream. 
/// \param SamplesToDescatter The number of samples required (post-mix) to cleanly output sound (not including
///                           any de-pop buffer).
/// \param RescaleFactor      The pre-mix resampling factor. This can be used to calculate how many samples
///                           of input buffer will be consumed during the mix.
/// \param FinalBuffer        True the the buffer we are populating is the last buffer before end of stream.
///
/// \return Manifestor status code, ManifestorNoError indicates success.
///
ManifestorStatus_t Manifestor_AudioKsound_c::FillOutInputBuffer(
                unsigned int SamplesToDescatter,
                Rational_c RescaleFactor,
                bool FinalBuffer,
                MME_DataBuffer_t *DataBuffer, tMixerFrameParams *MixerFrameParams,
                MME_DataBuffer_t *CodedDataBuffer, tMixerFrameParams *CodedMixerFrameParams,
                PcmPlayer_c::OutputEncoding *OutputEncoding,
                BypassPhysicalChannel_t BypassChannel )
{   
    ManifestorStatus_t Status = ManifestorNoError;
    unsigned int TimeToFillInSamples;
    unsigned int TimeCurrentlyEnqueuedInSamples; // the actual time the enqueued samples represent
    unsigned int NumSamplesCurrentlyEnqueued; // the number of actual samples enqueued (if this differs from
                                              // TimeCurrentlyEnqueuedInSamples then a percussive adjustment
    					      // is required).
    unsigned int IdealisedStartOffset = 0; // Prefered point of discontinuity
    unsigned int BufferIndex;
    unsigned int EndOffsetAfterResampling = SamplesToDescatter;
    unsigned int EndOffsetBeforeResampling = (RescaleFactor*EndOffsetAfterResampling).RoundedUpIntegerPart();

    // rescale SamplesToDescatter and set SamplesNeededForFadeOutBeforeResampling based on the
    // resampling currently applied
    TimeToFillInSamples = (RescaleFactor * SamplesToDescatter).RoundedUpIntegerPart();
    SamplesNeededForFadeOutBeforeResampling =
	(RescaleFactor * SamplesNeededForFadeOutAfterResampling).RoundedUpIntegerPart();
    
    // ensure we descatter sufficient samples to perform a fade out
    MANIFESTOR_DEBUG( "TimeToFillInSamples %d + %d = %d\n",
	              TimeToFillInSamples, SamplesNeededForFadeOutBeforeResampling,
	              TimeToFillInSamples + SamplesNeededForFadeOutBeforeResampling );
    TimeToFillInSamples += SamplesNeededForFadeOutBeforeResampling;
    
    MixerFrameParams->PTS = 0;
    MixerFrameParams->PTSflags = 0;

    //
    // Examine the already-descattered buffers
    //
    
    MANIFESTOR_ASSERT( InputAudioParameters.Source.ChannelCount );
    MANIFESTOR_ASSERT( InputAudioParameters.Source.BitsPerSample );
    
    NumSamplesCurrentlyEnqueued = BytesToSamples( DataBuffer->TotalSize );
    MANIFESTOR_ASSERT( NumSamplesCurrentlyEnqueued >= SamplesToRemoveWhenPlaybackCommences );
    TimeCurrentlyEnqueuedInSamples = NumSamplesCurrentlyEnqueued - SamplesToRemoveWhenPlaybackCommences;
    SamplesToRemoveWhenPlaybackCommences = 0;

    //
    // Dequeue buffers until the input buffer is complete
    //
    
    while( TimeCurrentlyEnqueuedInSamples < TimeToFillInSamples )
    {
        int SamplesToInjectIntoThisBuffer; // Number of samples that must be removed from the buffer
        
        //
        // Try to exit early without applying output timing to buffers we're not going to play
        //
        
	if( FinalBuffer )
        {
            if( OutputState == PLAYING && TimeCurrentlyEnqueuedInSamples >= SamplesNeededForFadeOutBeforeResampling )
            {
                MANIFESTOR_DEBUG( "Fading out ready for disconnection\n" );
                MixerFrameParams->StartOffset = 128;
            }
            else
            {
                MANIFESTOR_DEBUG( "Naturally ready for disconnection (already silent)\n" );
                MixerFrameParams->StartOffset = 0;
            }
            
            MixerFrameParams->Command = MIXER_PAUSE;
            OutputState = MUTED;

            ReleaseAllInputBuffersDuringUpdate = true;
            
            return FillOutCodedDataBuffer( DataBuffer, MixerFrameParams,
                                           CodedDataBuffer, CodedMixerFrameParams, 
                                           OutputEncoding, BypassChannel);
        }
        
        //
    	// Non-blocking dequeue of buffer (with silence stuffing in reaction to error)
    	//
        
    	if( DataBuffer->NumberOfScatterPages < MIXER_AUDIO_PAGES_PER_BUFFER )
    	{
    	    Status = DequeueBuffer(&BufferIndex, true);
            
            if( Status == ManifestorNoError )
            {
                //
                // Calculate the time this buffer will actually hit the display and the number of
                // samples we must offset the start by.
                //

                HandleAnticipatedOutputTiming(BufferIndex, TimeCurrentlyEnqueuedInSamples);
                SamplesToInjectIntoThisBuffer = HandleRequestedOutputTiming(BufferIndex);

                //
                // It is not possible for SamplestoInjectIntoThisBuffer to be too small (there are stoppers
                // to prevent too many samples being removed). It is however possible for its value to be
                // too large. In such a case the silence to inject extends beyond the end of this mixer frame.
                // In such a case, a buffer with a presentation time a long way in the future, we need to
                // push back the buffer, mute the entire input frame and wait for the next one.
                //
                
                if( ( SamplesToInjectIntoThisBuffer > 0 ) &&
                    ( ((TimeCurrentlyEnqueuedInSamples - NumSamplesCurrentlyEnqueued) + SamplesToInjectIntoThisBuffer) >
                      ((int) EndOffsetBeforeResampling - SamplesNeededForFadeOutBeforeResampling) ) )
                {
                    PushBackBuffer( BufferIndex );
                    Status = ManifestorError;
                }
            }
    	}
    	else
    	{
    	    MANIFESTOR_ERROR("No scatter pages remain to describe input - injecting silence\n");
    	    Status = ManifestorError;
    	}

    	if( Status != ManifestorNoError )
    	{
    	    if (OutputState == PLAYING)
            {
                if (NumSamplesCurrentlyEnqueued < SamplesNeededForFadeOutBeforeResampling)
                {
                    MANIFESTOR_ERROR("NumSamplesCurrentlyEnqueued is insufficient to de-pop (have %d, need %d)\n",
                                     NumSamplesCurrentlyEnqueued, SamplesNeededForFadeOutBeforeResampling);
                }              

    	    	MixerFrameParams->Command = MIXER_PAUSE;
    	    	if (NumSamplesCurrentlyEnqueued >= EndOffsetBeforeResampling)
    	    	    MixerFrameParams->StartOffset = EndOffsetAfterResampling;
    	    	else
    	    	    MixerFrameParams->StartOffset =
    	    			(NumSamplesCurrentlyEnqueued / RescaleFactor).RoundedIntegerPart();

                if (Status == ManifestorWouldBlock)
                {
                    MANIFESTOR_ERROR("Mixer has starved - injecting silence (sta:0x%x)\n",Status);
                    OutputState = STARVED;
                }
                else
                {
                    MANIFESTOR_DEBUG("Deliberately muting mixer - injecting silence\n");
                    OutputState = MUTED;
                }
            }
            else
            {
                MANIFESTOR_DEBUG("Injecting further silence (whilst %s)\n", LookupState());
		// still starving (or muted and not yet ready to start-up) so don't play any samples
    	    	MixerFrameParams->Command = MIXER_PAUSE;
    	    	MixerFrameParams->StartOffset = 0;
    	    }
            
            // We must release all input buffers when the mixer command completes otherwise we'll have
            // a glitch as the old samples are played during an unmute
            ReleaseAllInputBuffersDuringUpdate = true;

            return FillOutCodedDataBuffer( DataBuffer, MixerFrameParams,
                                           CodedDataBuffer, CodedMixerFrameParams, 
                                           OutputEncoding, BypassChannel );
    	}

        //
        // Update the manifestation parameters if required
        //
        
        if( StreamBuffer[BufferIndex].UpdateAudioParameters )
        {
            Status = UpdateAudioParameters(BufferIndex);
            if( Status != ManifestorNoError )
            {
                MANIFESTOR_ERROR("Ignored failure to update audio parameters\n");
            }
        }
     	    	
        //
        // Update the scatter page
        //
            
    	MME_ScatterPage_t *CurrentPage = DataBuffer->ScatterPages_p + DataBuffer->NumberOfScatterPages;
        CurrentPage->Page_p = StreamBuffer[BufferIndex].Data;
        CurrentPage->Size = StreamBuffer[BufferIndex].AudioParameters->SampleCount *
                            StreamBuffer[BufferIndex].AudioParameters->Source.ChannelCount *
                            (StreamBuffer[BufferIndex].AudioParameters->Source.BitsPerSample / 8);
        CurrentPage->FlagsIn = 0;
        
    	unsigned int *BufferIndexPtr = ((unsigned int *) DataBuffer->UserData_p) +
    	                               DataBuffer->NumberOfScatterPages;
    	*BufferIndexPtr = BufferIndex;
        
         DataBuffer->NumberOfScatterPages++;
         DataBuffer->TotalSize += CurrentPage->Size;
         
         NumSamplesCurrentlyEnqueued += StreamBuffer[BufferIndex].AudioParameters->SampleCount;
         TimeCurrentlyEnqueuedInSamples += StreamBuffer[BufferIndex].AudioParameters->SampleCount +
                                      SamplesToInjectIntoThisBuffer;
                                      
         if( 0 == IdealisedStartOffset &&
             NumSamplesCurrentlyEnqueued != TimeCurrentlyEnqueuedInSamples )
         {
             IdealisedStartOffset = TimeCurrentlyEnqueuedInSamples;
         }
    }

    MANIFESTOR_DEBUG( "Initial DataBuffer->TotalSize %d (%d samples)\n",
                      DataBuffer->TotalSize, BytesToSamples(DataBuffer->TotalSize) );
                     
    if( NumSamplesCurrentlyEnqueued == TimeCurrentlyEnqueuedInSamples )
    {
        MixerFrameParams->Command = MIXER_PLAY;
        MixerFrameParams->StartOffset = 0;
        OutputState = PLAYING;
        
        Status = ManifestorNoError;
    }
    else if( NumSamplesCurrentlyEnqueued > TimeCurrentlyEnqueuedInSamples )
    {
        unsigned int SamplesToRemoveBeforeResampling = NumSamplesCurrentlyEnqueued - TimeCurrentlyEnqueuedInSamples;
        
        Status = ShortenDataBuffer( DataBuffer, MixerFrameParams, SamplesToRemoveBeforeResampling,
        	                    IdealisedStartOffset, EndOffsetAfterResampling, RescaleFactor );
    }
    else
    {
        unsigned int SamplesToInjectBeforeResampling = TimeCurrentlyEnqueuedInSamples - NumSamplesCurrentlyEnqueued;
        
        Status = ExtendDataBuffer( DataBuffer, MixerFrameParams, SamplesToInjectBeforeResampling,
        	                   EndOffsetAfterResampling, RescaleFactor );
    }
    
    MANIFESTOR_DEBUG( "Massaged DataBuffer->TotalSize %d (%d samples)\n",
                      DataBuffer->TotalSize, BytesToSamples(DataBuffer->TotalSize) );

    // Error recovery is important at this stage. The player gracefully handles failure to deploy percussive
    // adjustment but our direct caller has to employ fairly drastic measures if we refuse to fill out the
    // buffer.
    if( Status != ManifestorNoError )
    {
	MANIFESTOR_DEBUG( "Recovered from error during percussive adjustment (adjustment will be ignored\n" );
        MixerFrameParams->Command = MIXER_PLAY;
        MixerFrameParams->StartOffset = 0;
        OutputState = PLAYING;
    }

    if( Status == ManifestorNoError )
        return FillOutCodedDataBuffer( DataBuffer, MixerFrameParams,
                                       CodedDataBuffer, CodedMixerFrameParams, 
                                       OutputEncoding, BypassChannel );
    return Status;
}


////////////////////////////////////////////////////////////////////////////
///
/// Release all decode buffers that are no longer contributing samples to the MME data buffer.
///
/// Additionally this function will update the first scatter page to reflect
/// any samples from it that we consumed by the previous mix command.
///
/// This function should not be called while a mix command is being processed.
///
/// \todo Review the management of SamplesPlayedSinceDisplayTimeOfNextCommitWasCalculated
///       it looks like this will give jitter due to miscalculation of consumption...
///
ManifestorStatus_t Manifestor_AudioKsound_c::UpdateInputBuffer( MME_DataBuffer_t *DataBuffer,
                                                                MME_MixerInputStatus_t *InputStatus,
                                                                MME_DataBuffer_t *CodedDataBuffer,
                                                                MME_MixerInputStatus_t *CodedInputStatus )
{
    unsigned int i;
    unsigned int CompletedPages;
    
    int BytesUsed = InputStatus->BytesUsed;
    
    //
    // Perform wholesale destruction if this was sought
    //
    
    if( ReleaseAllInputBuffersDuringUpdate )
    {
        ReleaseAllInputBuffersDuringUpdate = false;

        return FlushInputBuffer( DataBuffer, InputStatus, CodedDataBuffer, CodedInputStatus );
    }

    //
    // Work out which buffers can be marked as completed (and do so)
    //
    
    for (i=0; i<DataBuffer->NumberOfScatterPages; i++)
    {
    	MME_ScatterPage_t *CurrentPage = DataBuffer->ScatterPages_p + i;
	unsigned int &BufferIndex = ((unsigned int*) DataBuffer->UserData_p)[i];
	MANIFESTOR_ASSERT(BufferIndex != INVALID_BUFFER_ID);

	BytesUsed -= CurrentPage->Size;
	
	if( BytesUsed >= 0 )
	{
	    // this scatter page was entirely consumed
	    SamplesPlayedSinceDisplayTimeOfNextCommitWasCalculated +=
	    		   StreamBuffer[BufferIndex].AudioParameters->SampleCount;
	    if( StreamBuffer[BufferIndex].EventPending )
		ServiceEventQueue( BufferIndex );
	    OutputRing->Insert( (unsigned int) StreamBuffer[BufferIndex].Buffer );
            OS_LockMutex(&BufferQueueLock);
            NotQueuedBufferCount--;
            OS_UnLockMutex(&BufferQueueLock);

	    BufferIndex = INVALID_BUFFER_ID;
	}
	else
	{
	    // this scatter page was partially consumed so update the page structure...
	    unsigned int BytesRemaining = -BytesUsed;
	    
	    CurrentPage->Page_p = ((unsigned char *) CurrentPage->Page_p) + CurrentPage->Size - BytesRemaining;
	    CurrentPage->Size = BytesRemaining;
	    
	    break;
	}
    }
    CompletedPages = i;
    
    //
    // Shuffle everything downwards
    //
    
    int RemainingPages = DataBuffer->NumberOfScatterPages - CompletedPages;
    unsigned int FirstRemainingPage = DataBuffer->NumberOfScatterPages - RemainingPages;
    if( RemainingPages > 0 )
    {
    	memmove(DataBuffer->ScatterPages_p,
    	        DataBuffer->ScatterPages_p + FirstRemainingPage,
    	        RemainingPages * sizeof(MME_ScatterPage_t));
    	memmove(DataBuffer->UserData_p,
    	        ((unsigned int *) DataBuffer->UserData_p) + FirstRemainingPage,
    	        RemainingPages * sizeof(unsigned int));
	
	DataBuffer->NumberOfScatterPages = RemainingPages;
    }
    else
    {
    	DataBuffer->NumberOfScatterPages = 0;
    	
    	if( RemainingPages < 0 )
    	{
    	    MANIFESTOR_ERROR("Firmware consumed more data than it was supplied with!!!\n");
    	}
    }

    if (InputStatus->BytesUsed > DataBuffer->TotalSize) {
        MANIFESTOR_ERROR("InputStatus->BytesUsed > DataBuffer->TotalSize (%d > %d), BUG!!!!\n", DataBuffer->TotalSize, InputStatus->BytesUsed);
        DataBuffer->TotalSize = 0;
    } else
         DataBuffer->TotalSize -= InputStatus->BytesUsed;
    
    //
    // Handle other bits of status (mostly the play state)
    //
    
    if( OutputState == PLAYING )
    {
        if( InputStatus->State != MIXER_INPUT_RUNNING )
        {
            MANIFESTOR_ERROR( "Unexpected mixer mode %s whilst %s\n",
                              LookupMixerInputState(InputStatus->State), LookupState() );
        }

        if( DataBuffer->TotalSize < SamplesToBytes( SamplesNeededForFadeOutBeforeResampling ) )
        {
            MANIFESTOR_ERROR( "Insufficient samples remain for de-pop (got %d, want %d)\n",
                              BytesToSamples(DataBuffer->TotalSize), SamplesNeededForFadeOutBeforeResampling );
        }
    }
    else
    { 
        if( InputStatus->State == MIXER_INPUT_RUNNING )
        {
            MANIFESTOR_ERROR( "Unexpected mixer mode %s whilst %s\n", 
                              LookupMixerInputState(InputStatus->State), LookupState() );
        }        
    }

    //MANIFESTOR_DEBUG("DataBuffer->TotalSize %d DataBuffer->NumberOfScatterPages %d\n",
    //                 DataBuffer->TotalSize, DataBuffer->NumberOfScatterPages);
    if( CodedDataBuffer )
        return UpdateCodedDataBuffer( CodedDataBuffer, CodedInputStatus );
    return ManifestorNoError;
}


////////////////////////////////////////////////////////////////////////////
///
/// Release all decode buffers associated with an MME data buffer.
///
/// After running this function all buffers will be placed on the output ring
/// (whether they we displayed or not) a
///
/// This function should not be called while a mix command is being processed.
///
ManifestorStatus_t Manifestor_AudioKsound_c::FlushInputBuffer( MME_DataBuffer_t *DataBuffer,
                                                               MME_MixerInputStatus_t *InputStatus,
                                                               MME_DataBuffer_t *CodedDataBuffer,
                                                               MME_MixerInputStatus_t *CodedInputStatus )
{
    int BytesUsed = InputStatus->BytesUsed;

    MANIFESTOR_ASSERT( OutputState != PLAYING );

    //
    // Work out which buffers can be marked as completed (and do so)
    //
    
    for (unsigned int i = 0; i < DataBuffer->NumberOfScatterPages; i++ )
    {
        MME_ScatterPage_t *CurrentPage = DataBuffer->ScatterPages_p + i;
        unsigned int &BufferIndex = ((unsigned int*) DataBuffer->UserData_p)[i];
        MANIFESTOR_ASSERT(BufferIndex != INVALID_BUFFER_ID);

        BytesUsed -= CurrentPage->Size;
        
        if( BytesUsed >= 0 )
        {
            // this scatter page was entirely consumed (so we update the samples played business)
            SamplesPlayedSinceDisplayTimeOfNextCommitWasCalculated +=
                           StreamBuffer[BufferIndex].AudioParameters->SampleCount;
        }
        else
        {
            // this buffer was not played and is being released in an unusual way
            (void) ReleaseBuffer( BufferIndex );                       
        }
        
	if( StreamBuffer[BufferIndex].EventPending )
	    ServiceEventQueue( BufferIndex );
        OutputRing->Insert( (unsigned int) StreamBuffer[BufferIndex].Buffer );
        OS_LockMutex(&BufferQueueLock);
        NotQueuedBufferCount--;
        OS_UnLockMutex(&BufferQueueLock);
        BufferIndex = INVALID_BUFFER_ID;
    }

    //
    // Tidy up the data buffer to make clear that we really did flush everything.
    //
    
    DataBuffer->NumberOfScatterPages = 0;
    DataBuffer->TotalSize = 0;
    DataBuffer->StartOffset = 0;
    
    // zero-length, null pointer, no flags
    memset( &DataBuffer->ScatterPages_p[0], 0, sizeof( DataBuffer->ScatterPages_p[0] ) );

    if( CodedDataBuffer )
        return FlushCodedDataBuffer( CodedDataBuffer );
    return ManifestorNullQueued;
}


////////////////////////////////////////////////////////////////////////////
///
/// Update the manifestors view of the progression of time.
///
void Manifestor_AudioKsound_c::UpdateDisplayTimeOfNextCommit(unsigned long long Time)
{
    // we must hold this lock from the point we touch SamplesQueuedForDisplayAfterDisplayTimeOfNextCommit to
    // the point we update DisplayTimeOfNextCommit.
    OS_AutoLockMutex automatic( &DisplayTimeOfNextCommitMutex );
    
    SamplesQueuedForDisplayAfterDisplayTimeOfNextCommit -=
                    SamplesPlayedSinceDisplayTimeOfNextCommitWasCalculated;
    SamplesPlayedSinceDisplayTimeOfNextCommitWasCalculated = 0;

    DisplayTimeOfNextCommit = Time;

    //MANIFESTOR_DEBUG("DisplayTimeOfNextCommit %llu (delta %llu)\n",
    //                 DisplayTimeOfNextCommit, DisplayTimeOfNextCommit - LastDisplayTimeOfNextCommit);
    LastDisplayTimeOfNextCommit = DisplayTimeOfNextCommit;
    
    if( OutputState == STARTING )
    {
        MANIFESTOR_DEBUG( "Switching from STARTING to MUTED after display time update\n" );
        OutputState = MUTED;
    }
}


////////////////////////////////////////////////////////////////////////////
/// 
/// Make the best effort possible to honour the requested output timing.
///
/// At present this is acheived by dicking about rampantly with the sample count.
/// See Manifestor_AudioKsound_c::SetModuleParameters to see how we ensure space exists
/// to do this in.
///
/// \param BufferIndex Index (into the StreamBuffer table) of the buffer to be timed.
/// \return Number of silent samples to inject (+ve) or number of existing samples to remove (-ve).
///
int Manifestor_AudioKsound_c::HandleRequestedOutputTiming(unsigned int BufferIndex)
{
    Rational_c PreciseAdjustment = StreamBuffer[BufferIndex].AudioOutputTiming->OutputRateAdjustment;
    unsigned int SampleCount = StreamBuffer[BufferIndex].AudioParameters->SampleCount;
    //unsigned int ChannelCount = StreamBuffer[BufferIndex].AudioParameters->Source.ChannelCount;
    unsigned int DisplayCount = StreamBuffer[BufferIndex].AudioOutputTiming->DisplayCount;
    unsigned long long SystemPlaybackTime = StreamBuffer[BufferIndex].AudioOutputTiming->SystemPlaybackTime;
    unsigned long long ActualSystemPlaybackTime = StreamBuffer[BufferIndex].AudioOutputTiming->ActualSystemPlaybackTime; 
    unsigned int SamplingFrequency = StreamBuffer[BufferIndex].AudioParameters->Source.SampleRateHz;
    
    int SamplesRequiredToHonourDisplayCount;
    long long TimeRequiredToHonourSystemPlaybackTime;
    int SamplesRequiredToHonourSystemPlaybackTime;
    int SamplesRequiredToHonourEverything;
    int Adjustment;
    
    
    //
    // Send the tuning parameters to the frequency synthesiser
    //
    
    DerivePPMValueFromOutputRateAdjustment( PreciseAdjustment, &Adjustment );
    Mixer->SetOutputRateAdjustment( Adjustment );
    
    //
    // Calculate how man samples to inject or remove
    //
    
    SamplesRequiredToHonourDisplayCount = DisplayCount - SampleCount;
    if (SamplesRequiredToHonourDisplayCount)
        MANIFESTOR_DEBUG("Deploying percussive adjustment due to ::DisplayCount %u  SampleCount %u  Delta %d\n",
                         DisplayCount, SampleCount, DisplayCount - SampleCount);

#if 0
    TimeRequiredToHonourDisplayCount = (SamplesRequiredToHonourDisplayCount * 1000000) / SamplingFrequency;
    ActualSystemPlaybackTime += TimeRequiredToHonourDisplayCount;
#endif
    
    //
    // Figure out what actions would have to be taken in order to honour the system playback time.
    //
    
    if( (SystemPlaybackTime == INVALID_TIME) || (SystemPlaybackTime == UNSPECIFIED_TIME) )
    {
        SamplesRequiredToHonourSystemPlaybackTime = 0;
    }
    else
    {
        TimeRequiredToHonourSystemPlaybackTime = SystemPlaybackTime - ActualSystemPlaybackTime;
        
        if( TimeRequiredToHonourSystemPlaybackTime )
        {
	    // Nick changed the following to 10 seconds - this is to match the maximum synchronization window in the output coordinator
            if (TimeRequiredToHonourSystemPlaybackTime > 10000000ll)
            {
                MANIFESTOR_ERROR("Playback time is more than 10 seconds in the future (%lld) - ignoring\n",
                	         TimeRequiredToHonourSystemPlaybackTime);
                TimeRequiredToHonourSystemPlaybackTime = 0;
            }
            else if( TimeRequiredToHonourSystemPlaybackTime < 3000ll &&
                     TimeRequiredToHonourSystemPlaybackTime > -3000ll )
            {
                MANIFESTOR_DEBUG("Suppressing small adjustment required to meet true SystemPlaybackTime (%d)\n",
                                 TimeRequiredToHonourSystemPlaybackTime);
                TimeRequiredToHonourSystemPlaybackTime = 0;
            }
            else if( TimeRequiredToHonourSystemPlaybackTime < 0ll)
            {
                // the sync engine should be using the DisplayCount to avoid this
                MANIFESTOR_DEBUG("Playback time is in the past (%lld) - ignoring\n",
                                 TimeRequiredToHonourSystemPlaybackTime);
                TimeRequiredToHonourSystemPlaybackTime = 0;
            }
            else
            {
                MANIFESTOR_DEBUG("Deploying percussive adjustment due to ::SystemPlaybackTime %lld  Actual %lld  Delta %lld\n",
                                 SystemPlaybackTime, ActualSystemPlaybackTime, TimeRequiredToHonourSystemPlaybackTime);
            }
        }

        SamplesRequiredToHonourSystemPlaybackTime = (TimeRequiredToHonourSystemPlaybackTime * (long long) SamplingFrequency) / 1000000ll;
    }

    SamplesRequiredToHonourEverything = SamplesRequiredToHonourDisplayCount +
                                        SamplesRequiredToHonourSystemPlaybackTime;

    if (SamplesRequiredToHonourEverything < (-1*(int)SampleCount) ||
        SamplesRequiredToHonourDisplayCount > (int) SampleCount)
    {
        MANIFESTOR_ERROR("Found BUG (SamplesRequiredToHonourEverything %d) - deployed automated fly swat\n",
                         SamplesRequiredToHonourEverything);
        SamplesRequiredToHonourEverything = 0;
    }

    return SamplesRequiredToHonourEverything;
}


////////////////////////////////////////////////////////////////////////////
/// 
/// Calculate the time at which we anticipate the buffer will be output.
///
/// The basis of this calculation is that the manifestor is notified (before
/// Manifestor_AudioKsound_c::FillOutInputBuffer() is ever called) of the display
/// time that the first sample of the input buffer will be rendered,
/// Manifestor_AudioKsound_c::DisplayTimeOfNextCommit. When
/// we enter Manifestor_Ksound_c::FillOutInputBuffer() we determine how many
/// samples remain in the input buffer from any previous playback. This gives
/// us an offset (in samples) to the first sample that must be filled. When
/// a buffer is dequeued this method is called to calculate the display time
/// using this offset, SamplesAlreadyPlayed, and
/// Manifestor_AudioKsound_c::DisplayTimeOfNextCommit.
///
/// \param BufferIndex Index (into the StreamBuffer table) of the buffer to be timed.
/// \param SamplesAlreadyPlayed Number of samples by which to offset the display time.
///
void Manifestor_AudioKsound_c::HandleAnticipatedOutputTiming(
		unsigned int BufferIndex, unsigned int SamplesAlreadyPlayed)
{
    unsigned int SamplingFrequency = StreamBuffer[BufferIndex].AudioParameters->Source.SampleRateHz;
	
    //
    // Calculate and record the output timings
    //

    if( DisplayTimeOfNextCommit == INVALID_TIME )
    {
        // this is perhaps a little optimistic but it better than nothing
        DisplayTimeOfNextCommit = OS_GetTimeInMicroSeconds();
    }

    //
    // Nick changed this calculation to improve the accuracy
    //

    Rational_c TimeForSamplesAlreadyPlayedInSeconds(SamplesAlreadyPlayed, SamplingFrequency);
    TimeForSamplesAlreadyPlayedInSeconds	/=  StreamBuffer[BufferIndex].AudioOutputTiming->SystemClockAdjustment;

    StreamBuffer[BufferIndex].AudioOutputTiming->ActualSystemPlaybackTime =
            DisplayTimeOfNextCommit + IntegerPart( 1000000 * TimeForSamplesAlreadyPlayedInSeconds );

    MANIFESTOR_DEBUG("ActualPlaybackTime %llu (delta %llu) @ offset %u\n",
                     StreamBuffer[BufferIndex].AudioOutputTiming->ActualSystemPlaybackTime,
                     StreamBuffer[BufferIndex].AudioOutputTiming->ActualSystemPlaybackTime -
                             LastActualSystemPlaybackTime,
                     SamplesAlreadyPlayed);
#if 0
{
#define COUNT 512
static unsigned int C = 0;
static unsigned long long  DT[COUNT], PL[COUNT], SPT[COUNT];

    DT[C]	= DisplayTimeOfNextCommit;
    PL[C]	= IntegerPart( 1000000 * TimeForSamplesAlreadyPlayedInSeconds );
    SPT[C++]	= StreamBuffer[BufferIndex].AudioOutputTiming->ActualSystemPlaybackTime;

    if( C==COUNT )
    {
        for( C=1; C<COUNT; C++ )
            report( severity_info, "%4d - Vals %016llx %08llx %016llx - Deltas %6lld  %6lld %6lld\n", C,
                                DT[C], PL[C], SPT[C], DT[C]-DT[C-1], PL[C]-PL[C-1], SPT[C]-SPT[C-1] );

        report( severity_fatal, "Thats all folks\n" );
    }
}
#endif

    // squirrel away the last claimed playback time so we can display the delta
    LastActualSystemPlaybackTime = StreamBuffer[BufferIndex].AudioOutputTiming->ActualSystemPlaybackTime;
}

ManifestorStatus_t Manifestor_AudioKsound_c::ShortenDataBuffer( MME_DataBuffer_t *DataBuffer,
                                          tMixerFrameParams *MixerFrameParams,
                                          unsigned int SamplesToRemoveBeforeResampling,
                                          unsigned int IdealisedStartOffset,
                                          unsigned int EndOffsetAfterResampling,
                                          Rational_c RescaleFactor )
{
    unsigned int EndOffsetBeforeResampling = (RescaleFactor*EndOffsetAfterResampling).RoundedUpIntegerPart();
    unsigned int TotalSamples = BytesToSamples(DataBuffer->TotalSize);
    unsigned int MaximumToRemove = TotalSamples - EndOffsetBeforeResampling;
    unsigned int BytesToRemove = SamplesToBytes( SamplesToRemoveBeforeResampling );
    
    unsigned int PageOffset;
    
    MME_ScatterPage_t *Pages = DataBuffer->ScatterPages_p;
    
    if (SamplesToRemoveBeforeResampling > MaximumToRemove)
    {
        MANIFESTOR_ERROR( "SamplesToRemoveBeforeResampling is too large (%d, max %d)\n",
        	          SamplesToRemoveBeforeResampling, MaximumToRemove );
        return ManifestorError;
    }
    
    if( OutputState != PLAYING )
    {
        //
        // Output is silent, must remove samples from the front of the buffer
        //
        
        MixerFrameParams->Command = MIXER_PLAY;
        MixerFrameParams->StartOffset = 0;
        OutputState = PLAYING;
        
        MIXER_DEBUG( "Removing %d samples with MIXER_PLAY at %d\n",
        	SamplesToRemoveBeforeResampling, MixerFrameParams->StartOffset );
                     
        // start trimming the buffers from the front of the first page
        PageOffset = 0;
    }
    else
    {
        //
        // Output is playing, can choose between MIXER_FOFI and MIXER_PAUSE
        //

        unsigned int AccumulatedOffset = 0;
        int ApplyChangeAt;

        //
        // Search for the first seam that is at least 128 samples into the buffer
        //
        
        PageOffset = 0;
        AccumulatedOffset = BytesToSamples( Pages[PageOffset].Size );
        while (AccumulatedOffset < SamplesNeededForFadeOutBeforeResampling)
                AccumulatedOffset += BytesToSamples( Pages[++PageOffset].Size );
        
        ApplyChangeAt = AccumulatedOffset - SamplesToRemoveBeforeResampling;
        if (ApplyChangeAt < (int) SamplesNeededForFadeOutBeforeResampling)
            ApplyChangeAt = SamplesNeededForFadeOutBeforeResampling;
        
        if( ApplyChangeAt <= (int) (EndOffsetBeforeResampling - SamplesNeededForFadeOutBeforeResampling) )
        {
            //
            // Deploy MIXER_FOFI
            //
            
            MixerFrameParams->Command = MIXER_FOFI;
            MixerFrameParams->StartOffset = (ApplyChangeAt / RescaleFactor).RoundedIntegerPart();
            // OutputState is already correct;

            MIXER_DEBUG( "Removing %d samples with MIXER_FOFI at %d\n",
                         SamplesToRemoveBeforeResampling, MixerFrameParams->StartOffset );
            
            unsigned int SamplesRemoved = AccumulatedOffset - ApplyChangeAt;
            unsigned int BytesRemoved = SamplesToBytes( SamplesRemoved );
            
            BytesToRemove -= BytesRemoved;            
            SamplesToRemoveBeforeResampling -= SamplesRemoved;
            DataBuffer->TotalSize -= BytesRemoved;
            Pages[PageOffset].Size -= BytesRemoved;

            if( 0 == SamplesToRemoveBeforeResampling )
                return ManifestorNoError;

            // start trimming the buffers from the front of the second page
            PageOffset++;
        }
        else
        {
            //
            // Deploy MIXER_PAUSE
            //
            
            MixerFrameParams->Command = MIXER_PAUSE;
            MixerFrameParams->StartOffset = EndOffsetAfterResampling;
            OutputState = MUTED;
            
            MIXER_DEBUG( "Carrying over %d samples with MIXER_PAUSE at %d\n",
                         SamplesToRemoveBeforeResampling, MixerFrameParams->StartOffset );
                         
            SamplesToRemoveWhenPlaybackCommences = SamplesToRemoveBeforeResampling;
            
            // Don't fall through into the sample destuction code below
            return ManifestorNoError;
        }
    }

    // completely remove whole pages at the front
    while( Pages[PageOffset].Size < BytesToRemove )
    {
        BytesToRemove -= Pages[PageOffset].Size;
        SamplesToRemoveBeforeResampling -= BytesToSamples(Pages[PageOffset].Size);
        DataBuffer->TotalSize -= Pages[PageOffset].Size;
        Pages[PageOffset].Size = 0;

        PageOffset++;
        if( PageOffset >= DataBuffer->NumberOfScatterPages )
        {
            MANIFESTOR_ERROR( "PageOffset overflowed - probable implementation bug\n");
            return ManifestorError;
        }
            
        MANIFESTOR_ASSERT(BytesToRemove == SamplesToBytes( SamplesToRemoveBeforeResampling ));
    }
    
 
        
    unsigned char *DataPtr = (unsigned char *) Pages[PageOffset].Page_p;
    Pages[PageOffset].Page_p = (void *) (DataPtr + BytesToRemove);
    Pages[PageOffset].Size -= BytesToRemove;
    DataBuffer->TotalSize -= BytesToRemove;

         
    return ManifestorNoError;
}

ManifestorStatus_t Manifestor_AudioKsound_c::ExtendDataBuffer( MME_DataBuffer_t *DataBuffer,
                                         tMixerFrameParams *MixerFrameParams,
                                         unsigned int SamplesToInjectBeforeResampling,
                                         unsigned int EndOffsetAfterResampling,
                                         Rational_c RescaleFactor )
{
    unsigned int EndOffsetBeforeResampling = (RescaleFactor*EndOffsetAfterResampling).RoundedUpIntegerPart();
    unsigned int SamplesToInjectAfterResampling =
	    (SamplesToInjectBeforeResampling / RescaleFactor).RoundedIntegerPart();
    
    if( SamplesToInjectBeforeResampling > EndOffsetBeforeResampling )
    {
        MANIFESTOR_ERROR("SamplesToInjectBeforeResampling is too large (%d, max %d)\n", SamplesToInjectBeforeResampling, EndOffsetBeforeResampling);
        return ManifestorError;
    }
    
    if( OutputState == PLAYING )
    {
        MixerFrameParams->Command = MIXER_PAUSE;
        MixerFrameParams->StartOffset = EndOffsetAfterResampling - SamplesToInjectAfterResampling;
        OutputState = MUTED;
        
        MANIFESTOR_DEBUG( "Injecting %d samples with MIXER_PAUSE at %d\n",
                     SamplesToInjectBeforeResampling, MixerFrameParams->StartOffset );
    }
    else
    {
        MANIFESTOR_DEBUG( "Injecting %d samples with MIXER_PLAY whilst %s\n", SamplesToInjectBeforeResampling, LookupState() );

        MixerFrameParams->Command = MIXER_PLAY;
        MixerFrameParams->StartOffset = SamplesToInjectAfterResampling;
        OutputState = PLAYING;
    }
    
    return ManifestorNoError;
}


////////////////////////////////////////////////////////////////////////////
/// 
/// Update the manifestation parameters (if required)
///
ManifestorStatus_t Manifestor_AudioKsound_c::UpdateAudioParameters(unsigned int BufferIndex)
{
    ManifestorStatus_t Status;
    ParsedAudioParameters_t *AudioParameters;
    
    if (!StreamBuffer[BufferIndex].UpdateAudioParameters)
    {
    	MANIFESTOR_ERROR( "Audio parameters do not require updating at frame %d\n",
                          StreamBuffer[BufferIndex].FrameParameters->DisplayFrameIndex );
    	return ManifestorError;
    }
    
//

    MANIFESTOR_DEBUG("Updating audio parameters\n");  
    AudioParameters = StreamBuffer[BufferIndex].AudioParameters;
    
    MANIFESTOR_ASSERT(AudioParameters->Source.ChannelCount);
    MANIFESTOR_ASSERT(AudioParameters->Source.BitsPerSample);
    
    if (AudioParameters->Source.ChannelCount != SurfaceDescriptor.ChannelCount)
    {
    	MANIFESTOR_ERROR("Codec did not honour the surface's channel interleaving (%d instead of %d)\n",
    	                 AudioParameters->Source.ChannelCount, SurfaceDescriptor.ChannelCount);
    	// this error is non-fatal (providing the codec doesn't run past the end of its buffers)
    }

//

    //
    // Memorise the new audio parameters.
    //
    
    InputAudioParameters = *AudioParameters;
    Status = Mixer->UpdateManifestorParameters( this, AudioParameters );
    if( Status != PlayerNoError )
    {
        MANIFESTOR_ERROR( "Cannot update the mixer parameters\n" );
        return ManifestorError;
    }

//

    StreamBuffer[BufferIndex].UpdateAudioParameters = false;

//

    return ManifestorNoError;
}


////////////////////////////////////////////////////////////////////////////
/// 
/// Study a coded data buffer and identify what output encoding to use (dts case)
///
/// \param CodedDataBufferSize The caller usually has this to hand so we can avoid another lookup
/// \param RepetitionPeriod Pointer to where the repetition period of the coded data should be stored
/// \return Output encoding to be used.
///
PcmPlayer_c::OutputEncoding Manifestor_AudioKsound_c::LookupCodedDtsDataBufferOutputEncoding(
    ParsedAudioParameters_t *ParsedAudioParameters,
    unsigned int CodedDataBufferSize,
    unsigned int *RepetitionPeriod )
{
    unsigned int CompatibleSampleCount = ParsedAudioParameters->BackwardCompatibleProperties.SampleCount;
    unsigned int Iec60958Size = CompatibleSampleCount * 4; /* stereo, 16-bit */
    unsigned int MaxIec61937Size = Iec60958Size - 8; /* PA/PB/PC/PD @ 16-bits each */
    
    // in case of dtshd stream without compatible core (i.e. XLL only stream), 
    // the transcoded buffer will be void, so fallback to PCM
    if ( CodedDataBufferSize == 0 )
    {
        *RepetitionPeriod = 1;
        return PcmPlayer_c::OUTPUT_IEC60958;
    }
    else if ( CodedDataBufferSize > MaxIec61937Size )
    {
        if( CodedDataBufferSize == Iec60958Size )
            return PcmPlayer_c::BYPASS_DTS_CDDA;
        
        return PcmPlayer_c::OUTPUT_IEC60958;
    }
    
    *RepetitionPeriod = CompatibleSampleCount;
    
    switch( CompatibleSampleCount )
    {
        case 512:
            return PcmPlayer_c::BYPASS_DTS_512;
            
        case 1024:
            return PcmPlayer_c::BYPASS_DTS_1024;
            
        case 2048:
            return PcmPlayer_c::BYPASS_DTS_2048;
    }
    
    *RepetitionPeriod = 1;
    return PcmPlayer_c::OUTPUT_IEC60958;
}

////////////////////////////////////////////////////////////////////////////
/// 
/// Study a coded data buffer and identify what output encoding to use.
///
/// \param CodedDataBuffer Coded data buffer to extrac the meta data from
/// \param CodedDataBufferSize The caller usually has this to hand so we can avoid another lookup
/// \param RepetitionPeriod Pointer to where the repetition period of the coded data should be stored
/// \return Output encoding to be used.
///
PcmPlayer_c::OutputEncoding Manifestor_AudioKsound_c::LookupCodedDataBufferOutputEncoding(
                                                Buffer_c *CodedFrameBuffer, unsigned int CodedDataBufferSize,
                                                unsigned int *RepetitionPeriod,
                                                BypassPhysicalChannel_t BypassChannel )
{
BufferStatus_t Status;
ParsedAudioParameters_t *ParsedAudioParameters;

    //

    Status  = CodedFrameBuffer->ObtainMetaDataReference( Player->MetaDataParsedAudioParametersType,
                                                         (void **)&ParsedAudioParameters );
    if( BufferNoError != Status  )
    {
        MANIFESTOR_ERROR( "Cannot obtain meta data that ought to be attached to our coded data\n" );
        return PcmPlayer_c::OUTPUT_IEC60958;
    }     


    CodedFrameSampleCount = ParsedAudioParameters->SampleCount;
    *RepetitionPeriod = CodedFrameSampleCount;

    switch( ParsedAudioParameters->OriginalEncoding )
    {
    case AudioOriginalEncodingUnknown:
        *RepetitionPeriod = 1;
        return PcmPlayer_c::OUTPUT_IEC60958;    
        
    case AudioOriginalEncodingAc3:
        return PcmPlayer_c::BYPASS_AC3;

    case AudioOriginalEncodingDdplus:
        if ( BypassChannel == SPDIF )
        {
             // by this point we've already located the transcoded buffer
            return PcmPlayer_c::BYPASS_AC3;
        }
        else
        {
            *RepetitionPeriod = 6144;
            return PcmPlayer_c::BYPASS_DDPLUS;
        }
    case AudioOriginalEncodingDts:
        return (Manifestor_AudioKsound_c::LookupCodedDtsDataBufferOutputEncoding(ParsedAudioParameters, CodedDataBufferSize, RepetitionPeriod));

   case AudioOriginalEncodingDtshd:
   case AudioOriginalEncodingDtshdMA:
       if ( BypassChannel == SPDIF )
       {
           // a transcoded buffer is available...
           return (Manifestor_AudioKsound_c::LookupCodedDtsDataBufferOutputEncoding(ParsedAudioParameters, CodedDataBufferSize, RepetitionPeriod));
       }
       else
       {
           if (ParsedAudioParameters->OriginalEncoding == AudioOriginalEncodingDtshd)
           {
               *RepetitionPeriod = 2048;
               return PcmPlayer_c::BYPASS_DTSHD_HR;
           }
           else
           {
               *RepetitionPeriod = 8192;
               return PcmPlayer_c::BYPASS_DTSHD_MA;
           }
       }

   case AudioOriginalEncodingDtshdLBR:
       if ( BypassChannel == SPDIF )
        {
            *RepetitionPeriod = 1;
            return PcmPlayer_c::OUTPUT_IEC60958;    
        }
        else
        {
            *RepetitionPeriod = 4096;
            return PcmPlayer_c::BYPASS_DTSHD_LBR;
        }

   case AudioOriginalEncodingTrueHD:
       if ( BypassChannel == SPDIF )
        {
            *RepetitionPeriod = 1;
            return PcmPlayer_c::OUTPUT_IEC60958;    
        }
        else
        {
#ifdef BUG_4952
            *RepetitionPeriod = 15360;
            return PcmPlayer_c::BYPASS_TRUEHD;
#else
            *RepetitionPeriod = 1;
            return PcmPlayer_c::OUTPUT_IEC60958;
#endif
        }
    }
    
    MANIFESTOR_ERROR( "Unexpected audio coded data format (%d)\n", ParsedAudioParameters->OriginalEncoding );
    *RepetitionPeriod = 1;
    return PcmPlayer_c::OUTPUT_IEC60958;      
}

////////////////////////////////////////////////////////////////////////////
/// 
/// Dequeue the first coded buffer and release it.
///
/// There is no error checking here in order to prevent callers having to check the return code then they
/// 'know' it will be successful. This does, of course, make the caller responsible for checking that the
/// call is safe.
///
void Manifestor_AudioKsound_c::DequeueFirstCodedDataBuffer( MME_DataBuffer_t *CodedMmeDataBuffer )
{
    //Do atleast the basic check
    if(CodedMmeDataBuffer->NumberOfScatterPages > 0)
    {
        CodedMmeDataBuffer->TotalSize -= CodedMmeDataBuffer->ScatterPages_p->Size;
        CodedMmeDataBuffer->NumberOfScatterPages--;

        Buffer_c *CodedFrameBuffer = ((Buffer_c **) CodedMmeDataBuffer->UserData_p)[0];

        if (IsTranscoded)
            ReleaseTranscodedDataBuffer( CodedFrameBuffer );

        BufferStatus_t Status = CodedFrameBuffer->DecrementReferenceCount( IdentifierManifestor );
        if( BufferNoError != Status  )
        {
            MANIFESTOR_ERROR( "Cannot decrement coded data buffer reference count (%d)\n", Status );
            // no error recovery possible
        }
                
        memmove( CodedMmeDataBuffer->ScatterPages_p,
             CodedMmeDataBuffer->ScatterPages_p + 1,
             CodedMmeDataBuffer->NumberOfScatterPages * sizeof(MME_ScatterPage_t) );
        memmove( CodedMmeDataBuffer->UserData_p,
             ((unsigned int *) CodedMmeDataBuffer->UserData_p) + 1,
             CodedMmeDataBuffer->NumberOfScatterPages * sizeof(unsigned int) );
    }
}

////////////////////////////////////////////////////////////////////////////
/// 
/// Examine the queued compressed data buffers and determine the exact lengths (in sample frames).
///
unsigned int Manifestor_AudioKsound_c::LookupCodedDataBufferLength( MME_DataBuffer_t *CodedDataBuffer )
{
    unsigned int Length;

    //

    // calculate the 'fundamental' length
    Length = CodedDataBuffer->NumberOfScatterPages * CurrentCodedDataRepetitionPeriod;
    
    // add in any sample frames pending at the mixer
    Length += SamplesUntilNextCodedDataRepetitionPeriod;
    
    // remove the head of the queue if it is partial consumed (that was already accounted for by the above
    // addition)
    if( FirstCodedDataBufferPartiallyConsumed )
        Length -=  CurrentCodedDataRepetitionPeriod;
        
    return Length;
}


bool Manifestor_AudioKsound_c::DoesTranscodedBufferExist(BypassPhysicalChannel_t BypassChannel, ParsedAudioParameters_t * AudioParameters)
{
    // local table to prevent from being accessed appart from this method
    // this table tells whether a transcoded buffer exists according to the 
    // stream format and output type
    const static bool IsTranscodedMatrix[BypassPhysicalChannelCount][AudioOriginalEncodingTrueHD + 1] = 
    {
        // None   AC3    DD+   DTS     DTSHD  DTSHDMA  DTSHDLBR  TrueHD  over SPDIF
        { false,  false, true, false,  true,  true,    false,    false},
        // None   AC3    DD+    DTS    DTSHD  DTSHDMA  DTSHDLBR  TrueHD  over HDMI
        { false,  false, false, false, false, false,   false,    true}
    };

    AudioOriginalEncoding_t Encoding = AudioParameters->OriginalEncoding;

    if ((BypassChannel < BypassPhysicalChannelCount) && 
        (Encoding <= AudioOriginalEncodingTrueHD))
    {
        bool TranscodedBufferExists = IsTranscodedMatrix[BypassChannel][Encoding];

        // special case of DTS-HD: a dts core might not be present...
        if ( (Encoding == AudioOriginalEncodingDtshdMA) || 
             (Encoding == AudioOriginalEncodingDtshd) )
        {
            if (!AudioParameters->BackwardCompatibleProperties.SampleRateHz || !AudioParameters->BackwardCompatibleProperties.SampleCount)
            {
                // no core is present, so transcoding is not possible  
                TranscodedBufferExists = false;
            }
        }
        return TranscodedBufferExists;
    }
    else
    {
        MANIFESTOR_ERROR("Wrong Bypass channel (%d) or encoding (%d)\n", BypassChannel, Encoding);
        return false;
    }
}

////////////////////////////////////////////////////////////////////////////
/// 
/// Examine the queued LPCM buffers and create a partner coded data queue.
///
/// \todo Account for resampling...
///
ManifestorStatus_t Manifestor_AudioKsound_c::FillOutCodedDataBuffer( MME_DataBuffer_t *PcmBuffer,
                                                                     tMixerFrameParams *PcmFrameParams,
                                                                     MME_DataBuffer_t *CodedMmeDataBuffer,
                                                                     tMixerFrameParams *MixerFrameParams,
                                                                     PcmPlayer_c::OutputEncoding *OutputEncoding,
                                                                     BypassPhysicalChannel_t BypassChannel )
{
BufferStatus_t Status;
bool MuteRequestAsserted = false;

    //
    // Locate any new buffers in the PCM queue and append them to the coded queue.
    //
    
    for (unsigned int i=0; i<PcmBuffer->NumberOfScatterPages; i++)
    {
        unsigned int &BufferIndex = ((unsigned int*) PcmBuffer->UserData_p)[i];
        MANIFESTOR_ASSERT(BufferIndex != INVALID_BUFFER_ID);
        
        if( StreamBuffer[BufferIndex].QueueAsCodedData )
        {
            Buffer_c* Buffer = StreamBuffer[BufferIndex].Buffer;
            Buffer_c* CodedFrameBuffer, * TranscodedFrameBuffer;
            PcmPlayer_c::OutputEncoding CurrentBufferEncoding;
            unsigned int CurrentBufferRepetitionPeriod;
            bool IsTranscoded = DoesTranscodedBufferExist(BypassChannel, StreamBuffer[BufferIndex].AudioParameters);
            
            if( CodedMmeDataBuffer && (CodedMmeDataBuffer->NumberOfScatterPages >= MIXER_AUDIO_PAGES_PER_BUFFER) )
            {
                MANIFESTOR_ERROR("No scatter pages remain to describe input - ignoring\n");
                break;
            }
            
            StreamBuffer[BufferIndex].QueueAsCodedData = false;
             
            Status = Buffer->ObtainAttachedBufferReference( Stream->CodedFrameBufferType, &CodedFrameBuffer );
            if( Status != BufferNoError )
            {
                MANIFESTOR_ERROR( "Cannot fully populate the coded data buffer (%d)\n", Status );
                return ManifestorError;
            }

            if (IsTranscoded)
            {
                Status = CodedFrameBuffer->ObtainAttachedBufferReference( Stream->TranscodedFrameBufferType, &TranscodedFrameBuffer );

                if( Status != BufferNoError )
                {
                    MANIFESTOR_ERROR( "Cannot fully populate the transcoded coded data buffer (%d)\n", Status );
                    return ManifestorError;
                }

                if( CodedMmeDataBuffer == NULL )
                {
                    // bypass is not required, simply detach the transcoded data buffer if any
                    CodedFrameBuffer->DetachBuffer(TranscodedFrameBuffer);
                }
            }

            // skip the coded data handling
            if( CodedMmeDataBuffer == NULL )
                continue;
            
            ((void **) CodedMmeDataBuffer->UserData_p)[CodedMmeDataBuffer->NumberOfScatterPages] =
                        CodedFrameBuffer;
            MME_ScatterPage_t *CurrentPage = CodedMmeDataBuffer->ScatterPages_p + CodedMmeDataBuffer->NumberOfScatterPages;
	    
            Status = (IsTranscoded?TranscodedFrameBuffer:CodedFrameBuffer)->ObtainDataReference( NULL, &(CurrentPage->Size),
                                                                                                 &(CurrentPage->Page_p), UnCachedAddress );

            if( BufferNoError != Status  )
            {
                if( PcmPlayer_c::OUTPUT_IEC60958 != CurrentCodedDataEncoding )
                {
                    MANIFESTOR_DEBUG( "Detected coded data format change (%s -> NONE)\n",
                                      PcmPlayer_c::LookupOutputEncoding( CurrentCodedDataEncoding ) );
                }
                
                // either we are already in OUTPUT_IEC60958 mode (in which case we want to exit the loop
                // immediately) or we need to flush out the coded data already queued up (in which case we
                // want to exit the loop immediately).
                break;
            }
            
            // check that the buffer encodings all match nicely
            CurrentBufferEncoding = LookupCodedDataBufferOutputEncoding( Buffer, CurrentPage->Size,
                                                                         &CurrentBufferRepetitionPeriod,
                                                                         BypassChannel );
            
            if( CurrentCodedDataEncoding != CurrentBufferEncoding ||
                CurrentCodedDataRepetitionPeriod != CurrentBufferRepetitionPeriod )
            {
                if ( PcmPlayer_c::OUTPUT_IEC60958 == CurrentCodedDataEncoding )
                {
                    CurrentCodedDataEncoding = CurrentBufferEncoding;
                    CurrentCodedDataRepetitionPeriod = CurrentBufferRepetitionPeriod;
                }
                else
                {
                    MANIFESTOR_DEBUG( "Detected coded data format change (%s -> %s)\n",
                                      PcmPlayer_c::LookupOutputEncoding( CurrentCodedDataEncoding ),
                                      PcmPlayer_c::LookupOutputEncoding( CurrentBufferEncoding ) );
                    // get out... we can't allow a hetrogeneous queue
                    break;
                }
            }
           
            digitalFlag = true; 
            if( !PcmPlayer_c::IsOutputBypassed( CurrentCodedDataEncoding ) )
            {
                // there is no point whatsoever in queuing up data we can't play...
                break;
            }

            ParsedAudioParameters_t *AudioParameters = StreamBuffer[BufferIndex].AudioParameters;
            if ( AudioParameters->decErrorStatus )
            {
                MANIFESTOR_ERROR( "Skipping bad frame");
                errorFrameSeen = true;
                numGoodFrames  = 0;
                continue;
            }
            else
            {
                ++numGoodFrames;
                if ( numGoodFrames > player_sysfs_get_err_threshold())
                {
                    errorFrameSeen = false;
                }
                else if (errorFrameSeen)
                {
                    MANIFESTOR_ERROR( "Skipping good frame %d \n", numGoodFrames );
                    continue;
                }
            }

            // ensure the buffer remains valid for as long as we need
            Status = CodedFrameBuffer->IncrementReferenceCount( IdentifierManifestor );
            if( BufferNoError != Status  )
            {
                MANIFESTOR_ERROR( "Cannot increment coded data buffer reference count (%d)\n", Status );
                return ManifestorError;
            }

            // update the master descriptor (until the point no matter how much we adjust the current page
            // it will have no effect because MME will never read it)

            CodedMmeDataBuffer->NumberOfScatterPages++;
            CodedMmeDataBuffer->TotalSize += CurrentPage->Size;
            
            MANIFESTOR_DEBUG( "Added buffer %p/%p (%d bytes) to the coded data queue\n",
                              Buffer, CodedFrameBuffer, CurrentPage->Size );

            MANIFESTOR_DEBUG( "Coded data queue has %d scatter pages\n",
                              CodedMmeDataBuffer->NumberOfScatterPages );
        }
    }
    
    if (CodedMmeDataBuffer == NULL)
        return ManifestorNoError;
        
    //
    // Is any additional percussive action required upon the compressed buffers?
    //
    
    int Delta = 0;
    int RatioCompressedVsPcm = CodedFrameSampleCount?(CurrentCodedDataRepetitionPeriod/CodedFrameSampleCount):1;
    
    if( PcmPlayer_c::IsOutputBypassed( CurrentCodedDataEncoding ) )
    {
        int SamplesInPcmBuffer = BytesToSamples( PcmBuffer->TotalSize );
        int SamplesInCodedMmeDataBuffer = LookupCodedDataBufferLength( CodedMmeDataBuffer );
        
        Delta = (SamplesInCodedMmeDataBuffer/RatioCompressedVsPcm) - SamplesInPcmBuffer;
        
        MANIFESTOR_DEBUG( "SamplesInPcmBuffer %d  SamplesInCodedMmeDataBuffer %d  Delta %d\n",
                          SamplesInPcmBuffer, SamplesInCodedMmeDataBuffer, Delta );
    
        // Delta is -ve when we are starving (there may be enough samples to complete this frame but this cannot
        // is not guaranteed unless Delta is +ve). If Delta is exceeds the sync threshhold then we must take
        // corrective action. The sync threshhold is must be greater than the longest fixed size pause burst
        // mandated by IEC61937 (MPEG2, low sampling frequency fixes pause bursts at 64 IEC60938 frames) and may
        // optionally include a small fudge factor.
        if ( Delta < 0 || Delta > (64 + 16) )
        {
            MANIFESTOR_DEBUG( "Bad LPCM/compressed data delta (%d), taking corrective action\n",
                              Delta );
                          
            if( IsCodedDataPlaying ) {
                MuteRequestAsserted = true;
            }
            else
            {
                // keep removing buffers from the compressed data queue until the delta is negative (i.e. until
                // the CD Q is shorter than the LPCM Q). it is not terribly efficient to dequeue a single element
                // at a time but, dash it all, we should only really be removing one at once.
                // Planted a parsnip, 27 March 2008
                while( Delta > 0 && CodedMmeDataBuffer->NumberOfScatterPages > 0 )
                {
                    DequeueFirstCodedDataBuffer( CodedMmeDataBuffer );
                             
                    Delta -= (CurrentCodedDataRepetitionPeriod / RatioCompressedVsPcm);
                }
                
                // a correctly syncrhonized start offset will be calculated in the next section
            }
        }
        else
        {
            MANIFESTOR_DEBUG( "Good LPCM/compressed data delta (%d)\n", Delta );
        }
    }
    
    //
    // Generate the command to perform upon the coded data queue
    //
    
    // copy the PCM command (which we won't change unless we are performing a corrective action)
    *MixerFrameParams = *PcmFrameParams; 
    
    if( PcmPlayer_c::IsOutputBypassed( CurrentCodedDataEncoding ) ) {
        CodedMmeDataBuffer->Flags = ACC_MIXER_DATABUFFER_FLAG_COMPRESSED_DATA;
        
        if( IsCodedDataPlaying )
        {
            if( MuteRequestAsserted || MixerFrameParams->Command == MIXER_PAUSE )
            {
                MixerFrameParams->Command = MIXER_STOP;
                // TODO: we could do better than this if we knew the size of the mixer granule (i.e.
                //       StartOffset = MixerGranule - CurrentCodedDataRepetitionPeriod
                MixerFrameParams->StartOffset = 0;
            }
            else if( MixerFrameParams->Command == MIXER_FOFI )
            {
                MixerFrameParams->Command = MIXER_PLAY;
                MixerFrameParams->StartOffset = 0;
            }
            else
            {
                // leave everything the same as the LPCM Q
            }
        }
        else
        {
            if( Delta < 0 && MixerFrameParams->Command == MIXER_PLAY )
            {
                // Delta gives us the number of samples to insert to equalize the length of the queues
                MixerFrameParams->StartOffset = -Delta;
            }
        }
    }
    else
    {
        CodedMmeDataBuffer->Flags = 0;
        
        // make no noise...
        MixerFrameParams->Command = MIXER_STOP;
        MixerFrameParams->StartOffset = 0;
        LastCodedDataSyncOffset = 0;
        
        // clear out the buffers (if there are any), once we have stopped nothing else will consume them
        FlushCodedDataBuffer( CodedMmeDataBuffer );
    }
    
    *OutputEncoding = CurrentCodedDataEncoding;
    return ManifestorNoError;
}

////////////////////////////////////////////////////////////////////////////
/// 
/// Retire any already played buffers.
///
/// Release the references for any buffers that have been completely consumed and
/// update the MME descriptor accordingly.
///
ManifestorStatus_t Manifestor_AudioKsound_c::UpdateCodedDataBuffer( MME_DataBuffer_t *CodedDataBuffer,
                                                                    MME_MixerInputStatus_t *CodedInputStatus )
{
    BufferStatus_t Status;
    unsigned int i;
    unsigned int CompletedPages;
    
    int BytesUsed = CodedInputStatus->BytesUsed + CodedDataBuffer->StartOffset;
    
    //
    // Work out which buffers can be marked as completed (and do so)
    //
    
    if (BytesUsed > 0)
    {
        for( i=0; i<CodedDataBuffer->NumberOfScatterPages; i++ )
        {
            MME_ScatterPage_t *CurrentPage = CodedDataBuffer->ScatterPages_p + i;
            Buffer_c *CodedFrameBuffer = ((Buffer_c **) CodedDataBuffer->UserData_p)[i];
        
            BytesUsed -= CurrentPage->Size;
        
            if( BytesUsed >= 0 )
            {
                // this scatter page was entirely consumed
                if (IsTranscoded)
                    ReleaseTranscodedDataBuffer( CodedFrameBuffer );

                Status = CodedFrameBuffer->DecrementReferenceCount( IdentifierManifestor );
                if( BufferNoError != Status  )
                {
                    MANIFESTOR_ERROR( "Cannot decrement coded data buffer reference count (%d)\n", Status );
                    // no error recovery possible
                }
            
                MANIFESTOR_DEBUG( "Removing whole of a %d byte buffer from the queue\n", CurrentPage->Size );

                CodedDataBuffer->TotalSize -= CurrentPage->Size;
            
                if( BytesUsed == 0 )
                {
                    CodedDataBuffer->StartOffset = 0;
                    FirstCodedDataBufferPartiallyConsumed = false;
                
                    // ensure this buffer is included in the shuffle and break immediately (to ensure the value
                    // of FirstCodedDataBufferPartiallyConsumed remains unchanged)
                    i++;       
                    break;
                }
            }
            else
            {
                // this scatter page was partially consumed so update the page structure...
                unsigned int BytesRemaining = -BytesUsed;
            
                MANIFESTOR_DEBUG( "Removing %d of a %d byte buffer from the queue (new length %d)\n", 
                                  CurrentPage->Size - BytesRemaining, CurrentPage->Size, BytesRemaining );
                                  
                // the coded data input is 'special' - it wants the whole page back because its using the
                // size field to work out how much padding to insert. in exchange for its odd behavior it
                // promises to give sum the BytesUsed itself (i.e. we don't have to remember that we didn't
                // nibble anything out of the buffer).
                //CurrentPage->Page_p = ((unsigned char *) CurrentPage->Page_p) + CurrentPage->Size - BytesRemaining;
                //CurrentPage->Size = BytesRemaining;
                CodedDataBuffer->StartOffset = CurrentPage->Size + BytesUsed; // BytesUsed is -ve
            
                FirstCodedDataBufferPartiallyConsumed = true;
                break;
            }
        }
        
        CompletedPages = i;
    }
    else
    {
        CompletedPages = 0;
    }
    
    //
    // Shuffle everything downwards
    //

    int RemainingPages = CodedDataBuffer->NumberOfScatterPages - CompletedPages;
    unsigned int FirstRemainingPage = CodedDataBuffer->NumberOfScatterPages - RemainingPages;
    if( RemainingPages > 0 )
    {
        if( FirstRemainingPage > 0 )
        {
            memmove(CodedDataBuffer->ScatterPages_p,
                    CodedDataBuffer->ScatterPages_p + FirstRemainingPage,
                    RemainingPages * sizeof(MME_ScatterPage_t));
            memmove(CodedDataBuffer->UserData_p,
                    ((unsigned int *) CodedDataBuffer->UserData_p) + FirstRemainingPage,
                    RemainingPages * sizeof(unsigned int));
        
            CodedDataBuffer->NumberOfScatterPages = RemainingPages;
            
            // see above comments regarding how 'special' the coded data input is
            //CodedDataBuffer->TotalSize -= CodedInputStatus->BytesUsed;
        }
    }
    else
    {
        CodedDataBuffer->NumberOfScatterPages = 0;
        CodedDataBuffer->TotalSize = 0;
        
        if( RemainingPages < 0 )
        {
            MANIFESTOR_ERROR("Firmware consumed more data than it was supplied with!!!\n");
        }
    }
    

    //
    // Update the state based on the reply from the firmware
    //
    
    IsCodedDataPlaying = CodedInputStatus->State == MIXER_INPUT_RUNNING;
    //    IsCodedDataPlaying = CodedInputStatus->State != MIXER_INPUT_NOT_RUNNING;
    if ( IsCodedDataPlaying )
    {
        SamplesUntilNextCodedDataRepetitionPeriod = CodedInputStatus->NbInSplNextTransform;

        if( SamplesUntilNextCodedDataRepetitionPeriod > CurrentCodedDataRepetitionPeriod )
        {
            MANIFESTOR_ERROR( "Firmware is talking nonsense about remaining repetition period (%d of %d)\n",
                              SamplesUntilNextCodedDataRepetitionPeriod, CurrentCodedDataRepetitionPeriod );
            SamplesUntilNextCodedDataRepetitionPeriod = 0; // firmware value cannot be trusted
        }

        // DTS/CDDA is DTS encoded at a fixed bit rate (one that exactly matches CD audio) and
        // therefore, there is a direct translation between coded size and frames(1 frame is 4 bytes).
        // This is fortunate since the firmware doesn't know when the frame will end (due to historic
        // DTS encoder bugs) meaning the value of SamplesUntilNextCodedDataRepetitionPeriod needs to
        // be corrected by the driver.
        if( CurrentCodedDataEncoding == PcmPlayer_c::BYPASS_DTS_CDDA &&
            0 == SamplesUntilNextCodedDataRepetitionPeriod &&
            0 != CodedDataBuffer->StartOffset )
        {
            SamplesUntilNextCodedDataRepetitionPeriod = CurrentCodedDataRepetitionPeriod -
                                                        (CodedDataBuffer->StartOffset / 4);
        }
        
        MANIFESTOR_DEBUG("Retired %d whole buffers, %d buffers partially consumed (%d frames remaining)\n",
                         CompletedPages, FirstCodedDataBufferPartiallyConsumed,
                         SamplesUntilNextCodedDataRepetitionPeriod);
        
    }
    else
    {
        if( false != FirstCodedDataBufferPartiallyConsumed )
        {
            MANIFESTOR_ERROR( "Partially consumed buffer remains when %s\n",
                              LookupMixerInputState(CodedInputStatus->State) );

            CodedDataBuffer->StartOffset = 0;                              
            FirstCodedDataBufferPartiallyConsumed = false;
            DequeueFirstCodedDataBuffer( CodedDataBuffer );
            
        }
        SamplesUntilNextCodedDataRepetitionPeriod = 0;
    }

    //
    // Determine if the queue is totally empty (empty queue and mixer has nothing pended in current
    // repetition period.
    //

    if ( 0 == CodedDataBuffer->NumberOfScatterPages && 0 == SamplesUntilNextCodedDataRepetitionPeriod )
    {
        // the queue to longer has a type
        CurrentCodedDataEncoding = PcmPlayer_c::OUTPUT_IEC60958;
        CurrentCodedDataRepetitionPeriod = 1;
    }
    
    return ManifestorNoError;
}

////////////////////////////////////////////////////////////////////////////
/// 
/// Drop the references for any buffer in the coded data queue.
///
ManifestorStatus_t Manifestor_AudioKsound_c::FlushCodedDataBuffer( MME_DataBuffer_t *CodedMmeDataBuffer )
{
BufferStatus_t Status;
Buffer_c *CodedFrameBuffer;

    // release any references to the old buffers
    for( unsigned int i=0; i<CodedMmeDataBuffer->NumberOfScatterPages; i++ )
    {
        CodedFrameBuffer = ((Buffer_c **) CodedMmeDataBuffer->UserData_p)[i];
        ((unsigned int *) CodedMmeDataBuffer->UserData_p)[i] = INVALID_BUFFER_ID;
        
        if (IsTranscoded)
            ReleaseTranscodedDataBuffer( CodedFrameBuffer );
        
        Status = CodedFrameBuffer->DecrementReferenceCount( IdentifierManifestor );
        if( BufferNoError != Status  )
        {
            MANIFESTOR_ERROR( "Cannot decrement coded data buffer reference count (%d)\n", Status );
            return ManifestorError;
        }
        
        MANIFESTOR_DEBUG( "Flushed buffer %p from the queue\n", CodedFrameBuffer );                                         
    }
    
    CodedMmeDataBuffer->NumberOfScatterPages = 0;
    CodedMmeDataBuffer->TotalSize = 0;
    
    // the queue to longer has a type
    CurrentCodedDataEncoding = PcmPlayer_c::OUTPUT_IEC60958;
    CurrentCodedDataRepetitionPeriod = 1;
    
    return ManifestorNullQueued;
}

///////////////////////////////////////////////////////////////////////////
/// 
/// Release any transcoded data buufer attached to the coded data buffer
///
ManifestorStatus_t Manifestor_AudioKsound_c::ReleaseTranscodedDataBuffer( Buffer_c *CodedDataBuffer )
{
    Buffer_c * TranscodedDataBuffer;
    ManifestorStatus_t Status;
    
    Status = CodedDataBuffer->ObtainAttachedBufferReference( Stream->TranscodedFrameBufferType, &TranscodedDataBuffer );
    
    if (Status != BufferNoError)
    {
        MANIFESTOR_ERROR("Could not get the attached transcoded data buffer (%d)\n", Status);
        return ManifestorError;
    }
    
    TranscodedDataBuffer->Dump();
    
    Status = CodedDataBuffer->DetachBuffer( TranscodedDataBuffer );
    
    if (Status != BufferNoError)
    {
        MANIFESTOR_ERROR("Could not detach the transcoded buffer (%d)\n", Status);
        return ManifestorError;
    }
    
    return ManifestorNoError;
}
