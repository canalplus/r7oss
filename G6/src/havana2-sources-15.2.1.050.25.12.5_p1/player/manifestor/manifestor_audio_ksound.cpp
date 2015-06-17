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

#include <ACC_Transformers/Audio_DecoderTypes.h>

#include "osinline.h"
#include "output_timer_audio.h"
#include "pcmplayer.h"
#include "ksound.h"
#include "mixer_mme.h"
#include "player_generic.h"
#include "player_playback.h"
#include "player_stream.h"
#include "auto_lock_mutex.h"
#include "timestamps.h"
#include "codec_mme_base.h"
#include "manifestor_audio_ksound.h"

#undef TRACE_TAG
#define TRACE_TAG   "Manifestor_AudioKsound_c"

#define TUNEABLE_NAME_MANIFESTOR_AUDIO_KSOUND_LOOK_AHEAD    "ManifestorLookAhead"

#define MAX_TIME_TO_GET_AUDIO_DELAY (500ull)
#define AUDIO_WA_SAMPlING_FREQUENCY 48000  //Force output to 48kHz

static const char *LookupMixerInputState(uint32_t x)
{
    switch (x)
    {
#define X(y) case y: return #y
        X(MIXER_INPUT_NOT_RUNNING);
        X(MIXER_INPUT_RUNNING);
        X(MIXER_INPUT_FADING_OUT);
        X(MIXER_INPUT_MUTED);
        X(MIXER_INPUT_PAUSED);
        X(MIXER_INPUT_FADING_IN);
#undef X
    default:
        return "UNKNOWN STATE";
    }
}

// /////////////////////////////////////////////////////////////////////////
//
// Certification Support
// Action : register tuneable debug controls
// Input /debug/havana/ManifestorLookAhead

////////////////////////////////////////////////////////////////////////////
uint32_t volatile Manifestor_AudioKsound_c::SamplesNeededForFadeOutAfterResampling = 128;

///
/// Initialize and reset the ksound manifestor.
///
/// Like pretty much all components the constructor does little work
/// and the constructed object has a very light memory footprint. This is
/// the almost all allocation is performed by methods called after
/// construction. In our case most if this work happens in
/// Manifestor_AudioKsound_c::OpenOutputSurface().
///
Manifestor_AudioKsound_c::Manifestor_AudioKsound_c()
    : Manifestor_Audio_c()
    , Mixer(NULL)
    , RegisteredWithMixer(false)
    , EnabledWithMixer(false)
    , DisplayTimeOfNextCommitMutex()
    , SamplesQueuedForDisplayAfterDisplayTimeOfNextCommit(0)
    , SamplesPlayedSinceDisplayTimeOfNextCommitWasCalculated(0)
    , SamplesToRemoveWhenPlaybackCommences(0)
    , ReleaseAllInputBuffersDuringUpdate(false)
    , ReleaseProcessingDecodedBuffer(false)
    , DisplayTimeOfNextCommit(INVALID_TIME)
    , LastDisplayTimeOfNextCommit(INVALID_TIME)
    , InputAudioParameters()
    , OutputChannelCount(2)
    , OutputSampleDepthInBytes(4)
    , LastActualSystemPlaybackTime(INVALID_TIME)
    , InputAudioModeHistory(ACC_MODE_ID)
    , InputAudioModeChangeCount(0)
    , CodedInput()
    , SamplesNeededForFadeOutBeforeResampling()
    , OutputState(STOPPED)
    , IsPlaying(false)
    , PageCount(0)
    , Dropped(0)
    , TranscodeRequested(true)
    , CompressedFrameRequested(true)
{
    if (InitializationStatus != ManifestorNoError)
    {
        SE_ERROR("Initialization status not valid - aborting init\n");
        return;
    }

    SetGroupTrace(group_manifestor_audio_ksound);

    Configuration.Capabilities = MANIFESTOR_CAPABILITY_DISPLAY;

    OS_InitializeMutex(&DisplayTimeOfNextCommitMutex);

    CodedInput[CodedDataInput_c::BYPASS_SPDIF_IDX].Reset(SPDIF);
    CodedInput[CodedDataInput_c::BYPASS_HDMI_IDX].Reset(HDMI);

    // Certification Support to update SamplesNeededForFadeOutAfterResampling
    OS_RegisterTuneable(TUNEABLE_NAME_MANIFESTOR_AUDIO_KSOUND_LOOK_AHEAD, (unsigned int *)&SamplesNeededForFadeOutAfterResampling);

    SamplesNeededForFadeOutBeforeResampling = SamplesNeededForFadeOutAfterResampling;
}

////////////////////////////////////////////////////////////////////////////
///
/// Trivial destruction of the ksound manifestor.
///
/// Unlike the constructor (which does very little allocation) the destructor
/// has far more work to do but this is almost entirely handled by the
/// Manifestor_AudioKsound_c::Halt() method.
///
Manifestor_AudioKsound_c::~Manifestor_AudioKsound_c(void)
{
    Halt();

    OS_UnregisterTuneable(TUNEABLE_NAME_MANIFESTOR_AUDIO_KSOUND_LOOK_AHEAD);

    // no useful error recovery can be performed...
    OS_TerminateMutex(&DisplayTimeOfNextCommitMutex);
}


////////////////////////////////////////////////////////////////////////////
///
/// Shutdown, stop presenting and retrieving frames.
///
/// Blocks until complete. The primarily cause of blocking will be caused when
/// we wait for the manifestation thread, executing Manifestor_AudioKsound_c::PlaybackThread(),
/// to observe the request to terminate.
///
ManifestorStatus_t      Manifestor_AudioKsound_c::Halt(void)
{
    SE_DEBUG(group_manifestor_audio_ksound, ">: %s\n", LookupState());

    if (EnabledWithMixer)
    {
        PlayerStatus_t PStatus = Mixer->SendDisableManifestorRequest(this);
        if (PStatus != ManifestorNoError)
        {
            SE_ERROR("Failed to disable manifestor\n");
            // non-fatal - it probably means that we were not already registered
        }

        EnabledWithMixer = false;
        OutputState = STOPPED;
    }

    ManifestorStatus_t Status = Manifestor_Audio_c::Halt();
    if (Status != ManifestorNoError)
    {
        SE_ERROR("Failed to halt parent - aborting\n");
        return Status;
    }

    CloseOutputSurface();

    SE_DEBUG(group_manifestor_audio_ksound, "<: %s\n", LookupState());

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
ManifestorStatus_t Manifestor_AudioKsound_c::Reset(void)
{
    SE_DEBUG(group_manifestor_audio_ksound, ">: %s\n", LookupState());

    if (TestComponentState(ComponentRunning))
    {
        Halt();
    }

    RegisteredWithMixer = false;
    EnabledWithMixer = false;
    SamplesQueuedForDisplayAfterDisplayTimeOfNextCommit = 0;
    SamplesPlayedSinceDisplayTimeOfNextCommitWasCalculated = 0;
    SamplesToRemoveWhenPlaybackCommences = 0;
    ReleaseAllInputBuffersDuringUpdate = false;
    ReleaseProcessingDecodedBuffer      = false;
    DisplayTimeOfNextCommit = INVALID_TIME;
    LastDisplayTimeOfNextCommit = INVALID_TIME;

    memset(&InputAudioParameters, 0, sizeof(InputAudioParameters));  // TODO(pht) change

    OutputChannelCount = 2;
    OutputSampleDepthInBytes = 4;
    LastActualSystemPlaybackTime = INVALID_TIME;
    InputAudioModeHistory = ACC_MODE_ID;
    InputAudioModeChangeCount = 0;
    CodedInput[CodedDataInput_c::BYPASS_SPDIF_IDX].Reset(SPDIF);
    CodedInput[CodedDataInput_c::BYPASS_HDMI_IDX].Reset(HDMI);
    TranscodeRequested = true;
    CompressedFrameRequested = true;

    // Certification Support to update SamplesNeededForFadeOutAfterResampling
    SamplesNeededForFadeOutBeforeResampling = SamplesNeededForFadeOutAfterResampling;

    OutputState = STOPPED;
    IsPlaying   = false;
    PageCount   = 0;
    Dropped     = 0;

    return Manifestor_Audio_c::Reset();
}


////////////////////////////////////////////////////////////////////////////
///
/// Interfere with the module parameters in order to allocate over-sized decode buffers.
///
/// We currently use the host processor to implement an SRC-like approach to stretching
/// or compressing time. This requires extra space in the decode buffers to perform
/// in-place SRC.
///
ManifestorStatus_t      Manifestor_AudioKsound_c::SetModuleParameters(unsigned int   ParameterBlockSize,
                                                                      void          *ParameterBlock)
{
    ManifestorStatus_t Status;

    //SE_DEBUG(group_manifestor_audio_ksound, ">\n");
    if (ParameterBlockSize == sizeof(ManifestorAudioParameterBlock_t))
    {
        ManifestorAudioParameterBlock_t *ManifestorParameters;
        ManifestorParameters = (ManifestorAudioParameterBlock_t *) ParameterBlock;

        if (ManifestorParameters->ParameterType == ManifestorAudioMixerConfiguration)
        {
            Mixer = static_cast<Mixer_Mme_c *>(ManifestorParameters->Mixer);
            SE_DEBUG(group_manifestor_audio_ksound, "@: ManifestorAudioMixerConfiguration\n");
            return ManifestorNoError;
        }
        else if (ManifestorParameters->ParameterType == ManifestorAudioSetEmergencyMuteState)
        {
            if (EnabledWithMixer)
            {
                SE_DEBUG(group_manifestor_audio_ksound, "@: ManifestorAudioSetEmergencyMuteState\n");
                return Mixer->SetManifestorEmergencyMuteState(this, ManifestorParameters->EmergencyMute);
            }
            else
            {
                SE_ERROR("Cannot set mute state when not registered with the mixer\n");
                return ManifestorError;
            }
        }
    }

    if (ParameterBlockSize == sizeof(OutputTimerParameterBlock_t))
    {
        OutputTimerParameterBlock_t *OutputTimerParameters;
        OutputTimerParameters = (OutputTimerParameterBlock_t *) ParameterBlock;

        if (OutputTimerParameters->ParameterType == OutputTimerSetNormalizedTimeOffset)
        {
            SE_DEBUG(group_manifestor_audio_ksound, "@: OutputTimerSetNormalizedTimeOffset\n");

            // if the manifestor is not yet running we can ignore this update (which would be unsafe because
            // we might not yet have an output timer attached; it will be re-sent when we enable ourselves
            // with the mixer
            if (! TestComponentState(ComponentRunning))
            {
                SE_DEBUG(group_manifestor_audio_ksound,  "Ignoring sync offset update (manifestor is not running)\n");
                return ManifestorNoError;
            }

            SE_DEBUG(group_manifestor_audio_ksound,  "Forwarding sync offset of %lld\n", OutputTimerParameters->Offset.Value);
            // delegate to the output timer (the mixer proxy doesn't have access to all the player
            // components but it can poke the manifestor whenever the mixer settings are changed).
            return Stream->GetOutputTimer()->SetModuleParameters(ParameterBlockSize, ParameterBlock);
        }
    }

    if (ParameterBlockSize == sizeof(CodecParameterBlock_t))
    {
        CodecParameterBlock_t *CodecParameters;
        CodecParameters = (CodecParameterBlock_t *) ParameterBlock;

        if (CodecParameters->ParameterType == CodecSpecifyDownmix)
        {
            SE_DEBUG(group_manifestor_audio_ksound, "@: CodecSpecifyDownmix\n");

            // as above we discard the update if the manifestor is not yet running
            if (! TestComponentState(ComponentRunning))
            {
                SE_DEBUG(group_manifestor_audio_ksound,  "Ignoring downmix update (manifestor is not running)\n");
                return ManifestorNoError;
            }

            SE_DEBUG(group_manifestor_audio_ksound,  "Forwarding downmix promotion settings to codec\n");
            return Stream->GetCodec()->SetModuleParameters(ParameterBlockSize, ParameterBlock);
        }
    }

    Status = Manifestor_Audio_c::SetModuleParameters(ParameterBlockSize, ParameterBlock);
    //SE_DEBUG(group_manifestor_audio_ksound, "<\n");
    return Status;
}


////////////////////////////////////////////////////////////////////////////
///
/// connect the output port, and initialize our decode buffer associated context
///
/// \param Port         Port to connect to.
/// \return             Success or failure
///
ManifestorStatus_t      Manifestor_AudioKsound_c::Connect(Port_c *Port)
{
    ManifestorStatus_t Status;
    Status = Manifestor_Audio_c::Connect(Port);

    if (Status != ManifestorNoError)
    {
        return Status;
    }

    // We are now ComponentRunning...

    if (!EnabledWithMixer)
    {
        memset(&InputAudioParameters, 0, sizeof(InputAudioParameters)); // TODO(pht) change

        PlayerStatus_t PStatus = Mixer->SendEnableManifestorRequest(this);
        if (PStatus != PlayerNoError)
        {
            SE_ERROR("Failed to enable manifestor\n");
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
ManifestorStatus_t Manifestor_AudioKsound_c::OpenOutputSurface(class HavanaStream_c *Stream,
                                                               stm_se_sink_input_port_t input_port)
{
    PlayerStatus_t Status;
    SE_DEBUG(group_manifestor_audio_ksound, "> input_port:%d\n", input_port);
    Status = Mixer->SendRegisterManifestorRequest(this , Stream, input_port);

    if (Status != PlayerNoError)
    {
        SE_ERROR("Cannot register with the mixer\n");
        return ManifestorError;
    }

    RegisteredWithMixer = true;
    SE_DEBUG(group_manifestor_audio_ksound, "<\n");
    return ManifestorNoError;
}

///////////////////////////////////////////////////////////////////////////////
// WaitForProcessingBufRingRelease
//
// This method blocks the caller until Mixer thread releases the buffers
// that are currently processed by MixerTransformer.
//
// It returns a ManifestorError if after 3 MixerThread loop executions
// the processing buffer ring is still not empty.
//
ManifestorStatus_t Manifestor_AudioKsound_c::WaitForProcessingBufRingRelease()
{
    if (!RegisteredWithMixer || Mixer == NULL)
    {
        SE_DEBUG(group_manifestor_audio_ksound, "Not registered with Mixer\n");
        return ManifestorNoError;
    }

    // Wait for 3 Mixer transforms maximum: 1 ongoing Transform + 1 FadeOut transform + 1 margin = 3
    int maxTry = 3;
    int processingBufCount;
    GetProcessingRingBufCount(&processingBufCount);
    while (processingBufCount > 0 && maxTry-- > 0)
    {
        SE_INFO(GetGroupTrace(), "Wait for the Mixer thread to release %d processing buf\n", processingBufCount);
        Mixer->SendGetProcessingRingBufCountRequest(this, &processingBufCount);
    }

    if (processingBufCount > 0)
    {
        SE_ERROR("Failed to release %d Processing Buffers still owned by MixerThread\n", processingBufCount);
        return ManifestorError;
    }

    return ManifestorNoError;
}

///////////////////////////////////////////////////////////////////////////////
// GetProcessingRingBufCount
//
// This public method fills bufCountPtr pointer to integer argument with
// the number of buffers stored in ProcessingBufRing.
//
void Manifestor_AudioKsound_c::GetProcessingRingBufCount(int *bufCountPtr)
{
    if (ProcessingBufRing == NULL)
    {
        *bufCountPtr = 0;
    }
    else
    {
        *bufCountPtr = ProcessingBufRing->NbOfEntries();
    }
    SE_DEBUG(GetGroupTrace(), "NbProcessingBuffers:%d\n", *bufCountPtr);
}

////////////////////////////////////////////////////////////////////////////
///
/// Release all ALSA and MME resources.
///
ManifestorStatus_t Manifestor_AudioKsound_c::CloseOutputSurface(void)
{
    SE_DEBUG(group_manifestor_audio_ksound, ">\n");
    SE_ASSERT(!EnabledWithMixer);

    if (RegisteredWithMixer)
    {
        PlayerStatus_t Status;
        Status = Mixer->SendDeRegisterManifestorRequest(this);

        if (Status != PlayerNoError)
        {
            SE_ERROR("Failed to deregister with the mixer\n");
            // no recovery possible
        }

        RegisteredWithMixer = false;
    }

    SE_DEBUG(group_manifestor_audio_ksound, "<\n");
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
ManifestorStatus_t Manifestor_AudioKsound_c::GetNextQueuedManifestationTime(unsigned long long *Time, unsigned int *NumTimes)
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
    {
        uint32_t sfreq = InputAudioParameters.Source.SampleRateHz;
        uint32_t SamplesToManifest = InputAudioParameters.SampleCount;

        /* Bug 26979 :The DisplayTime is calculated for the time of next decoded frame to be manifested.
        And "SamplesQueuedForDisplayAfterDisplayTimeOfNextCommit" is dependent upon the SampleCount of the "Decoded frame"
        so it is logical to use the sampling frequency of the InPut to calculate the Displaytime.
        If input sampling frequency is not set for the case when mixer is already playing from audio-generators,
        as soon as we  start a play-stream, it attempts to get the sfreq from the playstream which is not yet available
        at mixer(although mixer is already in PLAYING state) see bug18311. Then we have no option to use
        MixerGranuleSize /    MixerSamplingFrequency */

        if (InputAudioParameters.Source.SampleRateHz == 0)
        {
            SamplesToManifest = Mixer->GetMixerGranuleSize();
            sfreq =  Mixer->GetMixerSamplingFrequency();
            SE_DEBUG(group_manifestor_audio_ksound,
                     " Using the SamplingFrequency and MixerGranuleSize of the mixer to calculate the time for next frame for manifestation\n");
        }

        // by combining the calculated display time of the next commit with the number of
        // samples in the buffer queue we can calculate
        OS_LockMutex(&DisplayTimeOfNextCommitMutex);
        SE_ASSERT(0 != sfreq);

        if (DisplayTimeOfNextCommit < WallTime)
        {
            // when the manifestor is functioning correctly taking this branch in impossible
            // (hence the prominent error report). unfortunately we currently are deliberately
            // running a 'broken' manifestor without silence injection on starvation making
            // the use of this code inevitable.
            SE_ERROR("DisplayTimeOfNextCommit has illegal (historic) value, deploying workaround\n");
            DisplayTimeOfNextCommit = WallTime +
                                      ((MIXER_NUM_PERIODS * 1000000ull * SamplesToManifest) / sfreq);
        }

        DisplayTime = DisplayTimeOfNextCommit +
                      ((((unsigned long long) SamplesQueuedForDisplayAfterDisplayTimeOfNextCommit) * 1000000ull) /
                       ((unsigned long long) sfreq));
        OS_UnLockMutex(&DisplayTimeOfNextCommitMutex);
        break;
    }

    default:
        SE_ERROR("Unexpected OutputState %s\n", LookupState());
        return ManifestorError;
    }

    SE_DEBUG(group_manifestor_audio_ksound, "At %llx (%s) next queued manifestation time was %llx (decimal delta %lld)\n",
             WallTime, LookupState(), DisplayTime, DisplayTime - WallTime);
    *Time = DisplayTime;
    *NumTimes = 1;
    return ManifestorNoError;
}


////////////////////////////////////////////////////////////////////////////
///
/// Handle a super-class notification that a buffer is about to be queued.
///
ManifestorStatus_t Manifestor_AudioKsound_c::QueueBuffer(AudioStreamBuffer_t *StreamBufPtr)
{
    //
    // Maintain the number of queued samples (used by GetNextQueuedManifestationTime)
    //
    OS_LockMutex(&DisplayTimeOfNextCommitMutex);
    SamplesQueuedForDisplayAfterDisplayTimeOfNextCommit += StreamBufPtr->AudioParameters->SampleCount;
    OS_UnLockMutex(&DisplayTimeOfNextCommitMutex);

    //
    // If we haven't set the input parameters then peek at the parameters before
    // enqueuing them.
    //

    if (0 == InputAudioParameters.Source.SampleRateHz)
    {
        ManifestorStatus_t Status;
        Status = UpdateAudioParameters(StreamBufPtr, true);

        if (Status != ManifestorNoError)
        {
            SE_ERROR("Failed to peek at the audio parameters\n");
        }
    }

    return ManifestorNoError;
}


////////////////////////////////////////////////////////////////////////////
///
/// Handle a super-class notification that a buffer is about to be released.
///
ManifestorStatus_t Manifestor_AudioKsound_c::ReleaseBuffer(AudioStreamBuffer_t *StreamBufPtr)
{
    SE_DEBUG(group_manifestor_audio_ksound, "> %d\n", StreamBufPtr->BufferIndex);
    //
    // Maintain the number of queued samples (used by GetNextQueuedManifestationTime)
    //
    OS_LockMutex(&DisplayTimeOfNextCommitMutex);
    SamplesQueuedForDisplayAfterDisplayTimeOfNextCommit -= StreamBufPtr->AudioParameters->SampleCount;
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
ManifestorStatus_t      Manifestor_AudioKsound_c::QueueNullManifestation(void)
{
    SE_DEBUG(group_manifestor_audio_ksound, "\n");
    return ManifestorNoError;
}


////////////////////////////////////////////////////////////////////////////
///
/// Flushes the display queue so buffers not yet manifested are returned.
///
/// \todo Not implemented. Should set a flag that the playback thread will observe
///       and act upon, then wait for an event.
///
ManifestorStatus_t      Manifestor_AudioKsound_c::FlushDisplayQueue(void)
{
    SE_DEBUG(group_manifestor_audio_ksound, "\n");
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
ManifestorStatus_t Manifestor_AudioKsound_c::FillOutInputBuffer(uint32_t SamplesToDescatter,
                                                                Rational_c RescaleFactor,
                                                                bool FinalBuffer,
                                                                MME_DataBuffer_t *DataBuffer,
                                                                tMixerFrameParams *MixerFrameParams,
                                                                bool Muted,
                                                                MME_DataBuffer_t CodedMixerDataBufferArray[],
                                                                tMixerFrameParams CodedMixerFrameParamsArray[],
                                                                bool BypassChannelSPDIF,
                                                                bool BypassChannelHDMI,
                                                                bool BypassSDChannelSPDIF,
                                                                bool BypassSDChannelHDMI,
                                                                PcmPlayer_c::OutputEncoding *OutputEncodingSPDIF,
                                                                PcmPlayer_c::OutputEncoding *OutputEncodingHDMI)
{
    ManifestorStatus_t Status = ManifestorNoError;
    bool     InputAudioModeChange = false;
    uint32_t InputAudioMode = InputAudioParameters.Organisation;
    uint32_t TimeToFillInSamples;
    uint32_t TimeCurrentlyEnqueuedInSamples; // the actual time the enqueued samples represent
    uint32_t NumSamplesCurrentlyEnqueued; // the number of actual samples enqueued (if this differs from
    // TimeCurrentlyEnqueuedInSamples then a percussive adjustment
    // is required).
    uint32_t IdealisedStartOffset = 0; // Preferred point of discontinuity
    uint32_t EndOffsetAfterResampling = SamplesToDescatter;
    uint32_t EndOffsetBeforeResampling = (RescaleFactor * EndOffsetAfterResampling).RoundedUpIntegerPart();
    SE_DEBUG(group_manifestor_audio_ksound, ">:%s DataBuffer: %p CodedDataBufferArray: %p BypassChannelSPDIF: %s BypassChannelHDMI: %s\n",
             LookupState(),
             DataBuffer,
             CodedMixerDataBufferArray,
             (true == BypassChannelSPDIF) ? "true" : "false",
             (true == BypassChannelHDMI) ? "true" : "false");
    // rescale SamplesToDescatter and set SamplesNeededForFadeOutBeforeResampling based on the
    // resampling currently applied
    TimeToFillInSamples = (RescaleFactor * SamplesToDescatter).RoundedUpIntegerPart();
    SamplesNeededForFadeOutBeforeResampling =
        (RescaleFactor * SamplesNeededForFadeOutAfterResampling).RoundedUpIntegerPart();
    // ensure we descatter sufficient samples to perform a fade out
    SE_DEBUG(group_manifestor_audio_ksound,  "TimeToFillInSamples %d + %d = %d\n",
             TimeToFillInSamples, SamplesNeededForFadeOutBeforeResampling,
             TimeToFillInSamples + SamplesNeededForFadeOutBeforeResampling);
    TimeToFillInSamples += SamplesNeededForFadeOutBeforeResampling;
    MixerFrameParams->PTS = 0;
    uMME_BufferFlags flags;
    flags.U32                      = 0;
    flags.Bits.FrontMatrixEncoded  = InputAudioParameters.StreamMetadata.FrontMatrixEncoded;
    flags.Bits.RearMatrixEncoded   = InputAudioParameters.StreamMetadata.RearMatrixEncoded;
    flags.Bits.MixLevel            = InputAudioParameters.StreamMetadata.MixLevel;
    flags.Bits.DialogNorm          = InputAudioParameters.StreamMetadata.DialogNorm;
    MixerFrameParams->PTSflags.U32  = flags.U32;
    //
    // Examine the already-descattered buffers
    //
    SE_ASSERT(InputAudioParameters.Source.ChannelCount);
    SE_ASSERT(InputAudioParameters.Source.BitsPerSample);
    NumSamplesCurrentlyEnqueued = BytesToSamples(DataBuffer->TotalSize);
    if (NumSamplesCurrentlyEnqueued < SamplesToRemoveWhenPlaybackCommences)
    {
        SE_WARNING("Illegal values: NumSamplesCurrentlyEnqueued = %d < SamplesToRemoveWhenPlaybackCommences = %d\n", NumSamplesCurrentlyEnqueued, SamplesToRemoveWhenPlaybackCommences);

        // This may happens on long zap test (bug 50320) when SamplesToRemoveWhenPlaybackCommences is
        // taking a large value of SamplesToRemoveBeforeResampling while issuing a MIXER_PAUSE (rare comparing
        // to MIXER_FOFI) and NumSamplesCurrentlyEnqueued is insufficient so remove all samples.
        TimeCurrentlyEnqueuedInSamples = 0;
    }
    else
    {
        TimeCurrentlyEnqueuedInSamples = NumSamplesCurrentlyEnqueued - SamplesToRemoveWhenPlaybackCommences;
        SE_DEBUG(group_manifestor_audio_ksound, "SamplesToRemoveWhenPlaybackCommences: -> %d\n", SamplesToRemoveWhenPlaybackCommences);
    }

    SamplesToRemoveWhenPlaybackCommences = 0;

    //
    // Initialise InputAudioModeHistory with InputAudioMode for first frame case
    //
    if (InputAudioModeHistory == ACC_MODE_ID)
    {
        InputAudioModeHistory = InputAudioParameters.Organisation;
    }

    //
    // Try to exit early without applying output timing to buffers we're not going to play
    //

    if (FinalBuffer)
    {
        SE_DEBUG(group_manifestor_audio_ksound, "Final Buffer, OutputState:%d; TimeCurrentlyEnqueuedInSamples:%d, SamplesNeededForFadeOutBeforeResampling:%d",
                 OutputState, TimeCurrentlyEnqueuedInSamples, SamplesNeededForFadeOutBeforeResampling);
        if (OutputState == PLAYING && TimeCurrentlyEnqueuedInSamples >= SamplesNeededForFadeOutBeforeResampling)
        {
            SE_DEBUG(group_manifestor_audio_ksound,  "Fading out ready for disconnection\n");
            MixerFrameParams->StartOffset = 128;
        }
        else
        {
            SE_DEBUG(group_manifestor_audio_ksound,  "Naturally ready for disconnection (already silent)\n");
            MixerFrameParams->StartOffset = 0;
        }

        MixerFrameParams->Command = MIXER_PAUSE;
        OutputState = MUTED;
        ReleaseAllInputBuffersDuringUpdate = true;
        SE_DEBUG(group_manifestor_audio_ksound,  "ReleaseAllInputBuffersDuringUpdate -> %d\n", ReleaseAllInputBuffersDuringUpdate);
        return FillOutCodedDataBuffer(DataBuffer,
                                      MixerFrameParams,
                                      CodedMixerDataBufferArray,
                                      CodedMixerFrameParamsArray,
                                      BypassChannelSPDIF,
                                      BypassChannelHDMI,
                                      BypassSDChannelSPDIF,
                                      BypassSDChannelHDMI,
                                      OutputEncodingSPDIF,
                                      OutputEncodingHDMI);
    }

    //
    // Dequeue buffers until the input buffer is complete
    //

    // Avoid concurent access to QueuedBuffer Ring. When extracting buffers to build Mixer's MME command.
    LockBuffferQueue();
    while (TimeCurrentlyEnqueuedInSamples < TimeToFillInSamples)
    {
        AudioStreamBuffer_t *StreamBufPtr;
        int SamplesToInjectIntoThisBuffer; // Number of samples that must be removed from the buffer

        //
        // Non-blocking dequeue of buffer (with silence stuffing in reaction to error)
        //

        if (DataBuffer->NumberOfScatterPages < MIXER_AUDIO_PAGES_PER_BUFFER)
        {

            if (PeekBufferUnderLock(&StreamBufPtr) == RingNoError)
            {
                //
                // Calculate the time this buffer will actually hit the display and the number of
                // samples we must offset the start by.
                //
                HandleAnticipatedOutputTiming(StreamBufPtr, TimeCurrentlyEnqueuedInSamples);
                SamplesToInjectIntoThisBuffer = HandleRequestedOutputTiming(StreamBufPtr);
                SE_DEBUG(group_manifestor_audio_ksound,  "SamplesToInjectIntoThisBuffer: %d\n", SamplesToInjectIntoThisBuffer);
                //
                // It is not possible for SamplestoInjectIntoThisBuffer to be too small (there are stoppers
                // to prevent too many samples being removed). It is however possible for its value to be
                // too large. In such a case the silence to inject extends beyond the end of this mixer frame.
                // In such a case, a buffer with a presentation time a long way in the future, we need to
                // push back the buffer, mute the entire input frame and wait for the next one.
                //

                if ((SamplesToInjectIntoThisBuffer > 0) &&
                    (((TimeCurrentlyEnqueuedInSamples - NumSamplesCurrentlyEnqueued) + SamplesToInjectIntoThisBuffer) >
                     ((int) EndOffsetBeforeResampling - SamplesNeededForFadeOutBeforeResampling)))
                {
                    Status = ManifestorError;
                }
                else if (SamplesToInjectIntoThisBuffer < - (int)(StreamBufPtr->AudioParameters->SampleCount))
                {
                    // this buffer is too late for presentation, drop it.
                    Status = DropNextQueuedBufferUnderLock();

                    if (Status == ManifestorNoError)
                    {
                        // peek the next buffer
                        continue;
                    }
                }
                else
                {
                    Status = DequeueBufferUnderLock(&StreamBufPtr);
                }
            }
            else
            {
                Status = ManifestorError;
            }
        }
        else
        {
            ReleaseAllInputBuffersDuringUpdate = true;
            SE_DEBUG(group_manifestor_audio_ksound, "No scatter pages remain to describe input - release all input pages during update\n");
            Status = ManifestorError;
        }

        if (Status != ManifestorNoError)
        {
            if (OutputState == PLAYING)
            {
                if (NumSamplesCurrentlyEnqueued < SamplesNeededForFadeOutBeforeResampling)
                {
                    SE_ERROR("NumSamplesCurrentlyEnqueued is insufficient to de-pop (have %d, need %d)\n",
                             NumSamplesCurrentlyEnqueued, SamplesNeededForFadeOutBeforeResampling);
                }

                MixerFrameParams->Command = MIXER_PAUSE;

                if (NumSamplesCurrentlyEnqueued >= EndOffsetBeforeResampling)
                {
                    MixerFrameParams->StartOffset = EndOffsetAfterResampling;
                }
                else
                    MixerFrameParams->StartOffset =
                        (NumSamplesCurrentlyEnqueued / RescaleFactor).RoundedIntegerPart();

                if (Status == ManifestorWouldBlock)
                {
                    SE_DEBUG(group_manifestor_audio_ksound, "@:%s: Mixer has starved - injecting silence (sta:0x%x)\n", LookupState(), Status);
                    OutputState = STARVED;
                    Stream->Statistics().ManifestorAudioMixerStarved++;
                }
                else
                {
                    SE_DEBUG(group_manifestor_audio_ksound, "Deliberately muting mixer - injecting silence\n");
                    OutputState = MUTED;
                }
            }
            else
            {
                SE_DEBUG(group_manifestor_audio_ksound, "@:%s: Injecting further silence\n", LookupState());
                // still starving (or muted and not yet ready to start-up) so don't play any samples
                MixerFrameParams->Command = MIXER_PAUSE;
                MixerFrameParams->StartOffset = 0;
            }

            SE_DEBUG(group_manifestor_audio_ksound, "@:%s: MixerFrameParams: %p->Command -> %s MixerFrameParams->StartOffset -> %d\n",
                     LookupState(), MixerFrameParams, Mixer_Mme_c::LookupMixerCommand((enum eMixerCommand) MixerFrameParams->Command), MixerFrameParams->StartOffset);
#if 0   // In case of PCM data input being paused, AND coded data input being also paused,
            // avoid flushing coded data after mixer operation completion.
            // We must release all input buffers when the mixer command completes otherwise we'll have
            // a glitch as the old samples are played during an unmute
            ReleaseAllInputBuffersDuringUpdate = true;
            SE_DEBUG(group_manifestor_audio_ksound,  "@:%s: ReleaseAllInputBuffersDuringUpdate -> %d\n", LookupState(), ReleaseAllInputBuffersDuringUpdate);
#endif
            UnLockBuffferQueue();
            return FillOutCodedDataBuffer(DataBuffer,
                                          MixerFrameParams,
                                          CodedMixerDataBufferArray,
                                          CodedMixerFrameParamsArray,
                                          BypassChannelSPDIF,
                                          BypassChannelHDMI,
                                          BypassSDChannelSPDIF,
                                          BypassSDChannelHDMI,
                                          OutputEncodingSPDIF,
                                          OutputEncodingHDMI);
        }

        //
        // Update the scatter page
        //
        MME_ScatterPage_t *CurrentPage = &DataBuffer->ScatterPages_p[DataBuffer->NumberOfScatterPages];
        CurrentPage->Page_p = Stream->GetDecodeBufferManager()->ComponentBaseAddress(StreamBufPtr->Buffer, PrimaryManifestationComponent, CachedAddress);
        CurrentPage->Size = StreamBufPtr->AudioParameters->SampleCount *
                            StreamBufPtr->AudioParameters->Source.ChannelCount *
                            (StreamBufPtr->AudioParameters->Source.BitsPerSample / 8);
        CurrentPage->FlagsIn = 0;

        /* Hold AudioStreamBuffer_t pointer into UserData for post mixing access */
        ((AudioStreamBuffer_t **) DataBuffer->UserData_p)[DataBuffer->NumberOfScatterPages] = StreamBufPtr;

        // TODO: should check --/++ operation
        DataBuffer->NumberOfScatterPages++;
        DataBuffer->TotalSize += CurrentPage->Size;
        NumSamplesCurrentlyEnqueued += StreamBufPtr->AudioParameters->SampleCount;
        TimeCurrentlyEnqueuedInSamples += StreamBufPtr->AudioParameters->SampleCount +
                                          SamplesToInjectIntoThisBuffer;

        if (0 == IdealisedStartOffset &&
            NumSamplesCurrentlyEnqueued != TimeCurrentlyEnqueuedInSamples)
        {
            IdealisedStartOffset = TimeCurrentlyEnqueuedInSamples;
        }

        // Update the manifestation parameters if required
        //
        if (SamplesNeededForFadeOutBeforeResampling == 0) // No lookahead done for accumulation. Lookahead need to check if any AudioParameters Update
        {
            RingStatus_t RingStatus = PeekBufferUnderLock(&StreamBufPtr);
            if (RingStatus != RingNoError)
            {
                SE_WARNING("Failed to Peek from QueuedBufRing for AudioParameters update lookahead\n");
            }
        }

        // Set TakeMixerClientLock parameter to false cause mixer client lock already taken.
        Status = UpdateAudioParameters(StreamBufPtr, false);

        if (Status != ManifestorNoError)
        {
            SE_ERROR("Ignored failure to update audio parameters\n");
        }

        //
        // If acmod of input has changed then apply mute
        //
        InputAudioMode = InputAudioParameters.Organisation;
        if (InputAudioModeHistory != InputAudioMode)
        {
            SE_DEBUG(group_manifestor_audio_ksound, "Mixer Audio Mode Change Detected : 0x%04X -> 0x%04X\n", InputAudioModeHistory, InputAudioMode);
            InputAudioModeChange = true;
        }
    }
    UnLockBuffferQueue();

    SE_DEBUG(group_manifestor_audio_ksound,  "Initial DataBuffer->TotalSize %d (%d samples)\n",
             DataBuffer->TotalSize, BytesToSamples(DataBuffer->TotalSize));

    if (NumSamplesCurrentlyEnqueued == TimeCurrentlyEnqueuedInSamples)
    {
        MixerFrameParams->Command = MIXER_PLAY;
        OutputState = PLAYING;

        //
        // If acmod of input has changed then apply mute to two consecutive blocks.
        // This is needed as we may face that within short span of time acmod has
        // changed. This may results in audio artifacts. Hence muting these blocks
        // is better option.
        //
#define MUTE_THRESHOLD 2
        if (InputAudioModeChangeCount >= MUTE_THRESHOLD)
        {
            InputAudioModeHistory = InputAudioMode;
            InputAudioModeChangeCount = 0;
        }
        else if ((InputAudioModeChange) || (InputAudioModeChangeCount != 0))
        {
            InputAudioModeChangeCount++;

            MixerFrameParams->Command = MIXER_MUTE;
            OutputState = MUTED;
        }


        if (Muted)
        {
            MixerFrameParams->Command = MIXER_MUTE;
            OutputState = MUTED;
        }

        MixerFrameParams->StartOffset = 0;
        Status = ManifestorNoError;
    }
    else if (NumSamplesCurrentlyEnqueued > TimeCurrentlyEnqueuedInSamples)
    {
        uint32_t SamplesToRemoveBeforeResampling = NumSamplesCurrentlyEnqueued - TimeCurrentlyEnqueuedInSamples;
        Status = ShortenDataBuffer(DataBuffer, MixerFrameParams, SamplesToRemoveBeforeResampling,
                                   IdealisedStartOffset, EndOffsetAfterResampling, RescaleFactor, Muted);
    }
    else
    {
        uint32_t SamplesToInjectBeforeResampling = TimeCurrentlyEnqueuedInSamples - NumSamplesCurrentlyEnqueued;
        Status = ExtendDataBuffer(DataBuffer, MixerFrameParams, SamplesToInjectBeforeResampling,
                                  EndOffsetAfterResampling, RescaleFactor, Muted);
    }

    SE_DEBUG(group_manifestor_audio_ksound,  "Massaged DataBuffer->TotalSize %d (%d samples)\n",
             DataBuffer->TotalSize, BytesToSamples(DataBuffer->TotalSize));

    // Error recovery is important at this stage. The player gracefully handles failure to deploy percussive
    // adjustment but our direct caller has to employ fairly drastic measures if we refuse to fill out the
    // buffer.
    if (Status != ManifestorNoError)
    {
        SE_DEBUG(group_manifestor_audio_ksound,  "Recovered from error during percussive adjustment (adjustment will be ignored)\n");
        MixerFrameParams->Command = MIXER_PLAY;
        OutputState = PLAYING;

        if (Muted)
        {
            MixerFrameParams->Command = MIXER_MUTE;
            OutputState = MUTED;
        }

        MixerFrameParams->StartOffset = 0;
    }

    if (Status == ManifestorNoError)
    {
        return FillOutCodedDataBuffer(DataBuffer,
                                      MixerFrameParams,
                                      CodedMixerDataBufferArray,
                                      CodedMixerFrameParamsArray,
                                      BypassChannelSPDIF,
                                      BypassChannelHDMI,
                                      BypassSDChannelSPDIF,
                                      BypassSDChannelHDMI,
                                      OutputEncodingSPDIF,
                                      OutputEncodingHDMI);
    }

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
ManifestorStatus_t Manifestor_AudioKsound_c::UpdateInputBuffer(MME_DataBuffer_t *DataBuffer,
                                                               MME_MixerInputStatus_t *InputStatus,
                                                               MME_DataBuffer_t CodedDataBufferArray[],
                                                               MME_MixerInputStatus_t CodedInputStatusArray[],
                                                               bool ForceReleaseProcessingDecodedBuffer)
{
    uint32_t i;
    uint32_t CompletedPages;
    int BytesUsed = InputStatus->BytesUsed;
    SE_DEBUG(group_manifestor_audio_ksound, ">: DataBuffer: %p (%d) InputStatus->BytesUsed: %d CodedDataBufferArray: %p\n",
             DataBuffer, DataBuffer->NumberOfScatterPages, InputStatus->BytesUsed, CodedDataBufferArray);

    if (NULL != CodedDataBufferArray)
    {
        SE_DEBUG(group_manifestor_audio_ksound, "@: &CodedDataBufferArray[0]: %p CodedInputStatusArray[0].BytesUsed: %d\n",
                 &CodedDataBufferArray[Mixer_Mme_c::MIXER_CODED_DATA_INPUT_SPDIF_IDX],
                 CodedInputStatusArray ? CodedInputStatusArray[Mixer_Mme_c::MIXER_CODED_DATA_INPUT_SPDIF_IDX].BytesUsed : 0);
        SE_DEBUG(group_manifestor_audio_ksound, "@: &CodedDataBufferArray[1]: %p CodedInputStatusArray[1].BytesUsed: %d\n",
                 &CodedDataBufferArray[Mixer_Mme_c::MIXER_CODED_DATA_INPUT_HDMI_IDX],
                 CodedInputStatusArray ? CodedInputStatusArray[Mixer_Mme_c::MIXER_CODED_DATA_INPUT_HDMI_IDX].BytesUsed : 0);
    }
    if (ForceReleaseProcessingDecodedBuffer)
    {
        SE_DEBUG(group_manifestor_audio_ksound, "@: Force release of the processing decode buffer\n");
        ReleaseProcessingDecodedBuffer = ForceReleaseProcessingDecodedBuffer;
    }
    SE_DEBUG(group_manifestor_audio_ksound, "@: ReleaseAllInputBuffersDuringUpdate: %d\n", ReleaseAllInputBuffersDuringUpdate);

    //
    // Perform wholesale destruction if this was sought
    //
    if (ReleaseProcessingDecodedBuffer)
    {
        if (OutputState == PLAYING) // When the output state is playing buffer can't be freed. As we are freeing the buffer so setting OutputState = MUTED
        {
            OutputState = MUTED;
        }

        ReleaseProcessingDecodedBuffer = false;
        ReleaseAllInputBuffersDuringUpdate = true;
    }

    if (ReleaseAllInputBuffersDuringUpdate)
    {
        ReleaseAllInputBuffersDuringUpdate = false;
        SE_DEBUG(group_manifestor_audio_ksound,  "@: ReleaseAllInputBuffersDuringUpdate -> %d\n", ReleaseAllInputBuffersDuringUpdate);
        return FlushInputBuffer(DataBuffer, InputStatus, CodedDataBufferArray, CodedInputStatusArray);
    }

    //
    // Work out which buffers can be marked as completed (and do so)
    //

    for (i = 0; i < DataBuffer->NumberOfScatterPages; i++)
    {
        MME_ScatterPage_t *CurrentPage = &DataBuffer->ScatterPages_p[i];
        AudioStreamBuffer_t *StreamBufPtr = ((AudioStreamBuffer_t **) DataBuffer->UserData_p)[i];
        SE_ASSERT(StreamBufPtr != NULL);
        BytesUsed -= CurrentPage->Size;

        if (BytesUsed >= 0)
        {
            // update playback info statistics
            Stream->Statistics().Pts              = StreamBufPtr->FrameParameters->NativePlaybackTime;
            Stream->Statistics().PresentationTime = StreamBufPtr->AudioOutputTiming->ActualSystemPlaybackTime;
            Stream->Statistics().SystemTime       = OS_GetTimeInMicroSeconds();
            if (ValidTime(StreamBufPtr->AudioOutputTiming->SystemPlaybackTime))
            {
                Stream->Statistics().SyncError    = StreamBufPtr->AudioOutputTiming->SystemPlaybackTime
                                                    - StreamBufPtr->AudioOutputTiming->ActualSystemPlaybackTime;
            }

            // the PTS of the next buffer to be played will be current PTS + Duration of Buffer in PTS units
            {
                uint32_t NumberOfSamples =  StreamBufPtr->AudioParameters->SampleCount;
                uint32_t SamplingFrequency = StreamBufPtr->AudioParameters->Source.SampleRateHz;

                if (0 == SamplingFrequency)
                {
                    SE_INFO(group_manifestor_audio_ksound, "samplingfreq 0; forcing default\n"); // shall replace with assert eventually
                    SamplingFrequency = AUDIO_WA_SAMPlING_FREQUENCY;
                }

                unsigned long long FrameDurationInusec = RoundedLongLongIntegerPart(Rational_t(1000000, SamplingFrequency) * NumberOfSamples);
                unsigned long long FrameDurationInPts = TimeStamp_c(FrameDurationInusec, TIME_FORMAT_US).PtsValue();
                PtsToDisplay = Stream->Statistics().Pts + FrameDurationInPts;
            }
            // this scatter page was entirely consumed
            SamplesPlayedSinceDisplayTimeOfNextCommitWasCalculated += StreamBufPtr->AudioParameters->SampleCount;

            if (StreamBufPtr->EventPending)
            {
                ServiceEventQueue(StreamBufPtr->BufferIndex);
            }

            ExtractAndReleaseProcessingBuf();

            // Increment statistic counter for manifested frames as
            // frame (Decoded Buffer) corresponding to scatter page is consumed.
            Stream->Statistics().FrameCountManifested++;
        }
        else
        {
            // this scatter page was partially consumed so update the page structure...
            unsigned int BytesRemaining = -BytesUsed;
            CurrentPage->Page_p = ((unsigned char *) CurrentPage->Page_p) + CurrentPage->Size - BytesRemaining;
            CurrentPage->Size = BytesRemaining;
            SE_DEBUG(group_manifestor_audio_ksound, "@: BytesRemaining: %d\n", BytesRemaining);
            break;
        }
    }

    CompletedPages = i;
    //
    // Shuffle everything downwards
    //
    int RemainingPages = DataBuffer->NumberOfScatterPages - CompletedPages;
    unsigned int FirstRemainingPage = DataBuffer->NumberOfScatterPages - RemainingPages;

    if (RemainingPages > 0)
    {
        memmove(&DataBuffer->ScatterPages_p[0],
                &DataBuffer->ScatterPages_p[FirstRemainingPage],
                RemainingPages * sizeof(DataBuffer->ScatterPages_p[0]));
        memmove(&((AudioStreamBuffer_t **) DataBuffer->UserData_p)[0],
                &((AudioStreamBuffer_t **) DataBuffer->UserData_p)[FirstRemainingPage],
                RemainingPages * sizeof(AudioStreamBuffer_t *));
        DataBuffer->NumberOfScatterPages = RemainingPages;
    }
    else
    {
        DataBuffer->NumberOfScatterPages = 0;

        if (RemainingPages < 0)
        {
            SE_ERROR("Firmware consumed more data than it was supplied with!!!\n");
        }
    }

    DataBuffer->TotalSize -= InputStatus->BytesUsed;

    //
    // Handle other bits of status (mostly the play state)
    //
    if ((OutputState == PLAYING) || (OutputState == MUTED))
    {
        IsPlaying = true;
        SE_VERBOSE(group_manifestor_audio_ksound,  "mixer-fw input status  %s whilst mixer-driver status %s\n",
                   LookupMixerInputState(InputStatus->State), LookupState());

        if ((OutputState == PLAYING) && (DataBuffer->TotalSize < SamplesToBytes(SamplesNeededForFadeOutBeforeResampling)))
        {
            SE_DEBUG(group_manifestor_audio_ksound,  "Insufficient samples remain for de-pop (got %d, want %d)\n",
                     BytesToSamples(DataBuffer->TotalSize), SamplesNeededForFadeOutBeforeResampling);
        }
    }
    else
    {
        if (InputStatus->State == MIXER_INPUT_RUNNING)
        {
            SE_INFO(group_manifestor_audio_ksound,  "Unexpected mixer mode MIXER_INPUT_RUNNING whilst %s\n", LookupState());
        }
    }

    //SE_DEBUG(group_manifestor_audio_ksound, "DataBuffer->TotalSize %d DataBuffer->NumberOfScatterPages %d\n",
    //                 DataBuffer->TotalSize, DataBuffer->NumberOfScatterPages);
    if (NULL != CodedDataBufferArray)
    {
        if (true == CodedInput[CodedDataInput_c::BYPASS_SPDIF_IDX].IsBypassPossible())
        {
            UpdateCodedDataBuffer(&CodedDataBufferArray[Mixer_Mme_c::MIXER_CODED_DATA_INPUT_SPDIF_IDX],
                                  &CodedInputStatusArray[Mixer_Mme_c::MIXER_CODED_DATA_INPUT_SPDIF_IDX],
                                  CodedInput[CodedDataInput_c::BYPASS_SPDIF_IDX]);
        }
        else
        {
            CodedInput[CodedDataInput_c::BYPASS_SPDIF_IDX].Reset(SPDIF);
        }

        if (true == CodedInput[CodedDataInput_c::BYPASS_HDMI_IDX].IsBypassPossible())
        {
            UpdateCodedDataBuffer(&CodedDataBufferArray[Mixer_Mme_c::MIXER_CODED_DATA_INPUT_HDMI_IDX],
                                  &CodedInputStatusArray[Mixer_Mme_c::MIXER_CODED_DATA_INPUT_HDMI_IDX],
                                  CodedInput[CodedDataInput_c::BYPASS_HDMI_IDX]);
        }
        else
        {
            CodedInput[CodedDataInput_c::BYPASS_HDMI_IDX].Reset(HDMI);
        }

        // The job is correctly done so let return.
    }
    else
    {
        CodedInput[CodedDataInput_c::BYPASS_SPDIF_IDX].Reset(SPDIF);
        CodedInput[CodedDataInput_c::BYPASS_HDMI_IDX].Reset(HDMI);
    }

    CodedInput[CodedDataInput_c::BYPASS_SPDIF_IDX].DebugDump();
    CodedInput[CodedDataInput_c::BYPASS_HDMI_IDX].DebugDump();
    return ManifestorNoError;
}


////////////////////////////////////////////////////////////////////////////
///
/// Release all decode buffers associated with an MME data buffer.
///
/// After running this function all buffers will be placed on the output port
/// (whether they we displayed or not) a
///
/// This function should not be called while a mix command is being processed.
///
ManifestorStatus_t Manifestor_AudioKsound_c::FlushInputBuffer(MME_DataBuffer_t *DataBuffer,
                                                              MME_MixerInputStatus_t *InputStatus,
                                                              MME_DataBuffer_t CodedDataBufferArray[],
                                                              MME_MixerInputStatus_t CodedInputStatusArray[])
{
    int BytesUsed = InputStatus->BytesUsed;
    SE_ASSERT(OutputState != PLAYING);
    SE_DEBUG(group_manifestor_audio_ksound, ">:%s: DataBuffer: %p CodedDataBufferArray: %p\n", LookupState(), DataBuffer, CodedDataBufferArray);

    //
    // Work out which buffers can be marked as completed (and do so)
    //

    for (unsigned int i = 0; i < DataBuffer->NumberOfScatterPages; i++)
    {
        MME_ScatterPage_t *CurrentPage = &DataBuffer->ScatterPages_p[i];
        AudioStreamBuffer_t *StreamBufPtr = ((AudioStreamBuffer_t **) DataBuffer->UserData_p)[i];
        SE_ASSERT(StreamBufPtr != NULL);
        BytesUsed -= CurrentPage->Size;

        if (BytesUsed >= 0)
        {
            // this scatter page was entirely consumed (so we update the samples played business)
            SamplesPlayedSinceDisplayTimeOfNextCommitWasCalculated += StreamBufPtr->AudioParameters->SampleCount;
        }
        else
        {
            // this buffer was not played and is being released in an unusual way
            ReleaseBuffer(StreamBufPtr);
        }

        if (StreamBufPtr->EventPending)
        {
            ServiceEventQueue(StreamBufPtr->BufferIndex);
        }

        ExtractAndReleaseProcessingBuf();
    }

    //
    // Tidy up the data buffer to make clear that we really did flush everything.
    //
    DataBuffer->Flags = 0;
    DataBuffer->NumberOfScatterPages = 0;
    DataBuffer->TotalSize = 0;
    DataBuffer->StartOffset = 0;

    // zero-length, null pointer, no flags
    memset(&DataBuffer->ScatterPages_p[0], 0, sizeof(DataBuffer->ScatterPages_p[0]));

    if (CodedDataBufferArray)
    {
        FlushCodedDataBuffer(&CodedDataBufferArray[Mixer_Mme_c::MIXER_CODED_DATA_INPUT_SPDIF_IDX], CodedInput[CodedDataInput_c::BYPASS_SPDIF_IDX]);
        FlushCodedDataBuffer(&CodedDataBufferArray[Mixer_Mme_c::MIXER_CODED_DATA_INPUT_HDMI_IDX], CodedInput[CodedDataInput_c::BYPASS_HDMI_IDX]);
    }

    CodedInput[CodedDataInput_c::BYPASS_SPDIF_IDX].Reset(SPDIF);
    CodedInput[CodedDataInput_c::BYPASS_HDMI_IDX].Reset(HDMI);
    CodedInput[CodedDataInput_c::BYPASS_SPDIF_IDX].DebugDump();
    CodedInput[CodedDataInput_c::BYPASS_HDMI_IDX].DebugDump();

    // Extrapolated PTS invalidated as we have flushed the queue.
    PtsToDisplay = INVALID_TIME;
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
    AutoLockMutex automatic(&DisplayTimeOfNextCommitMutex);
    SamplesQueuedForDisplayAfterDisplayTimeOfNextCommit -=
        SamplesPlayedSinceDisplayTimeOfNextCommitWasCalculated;
    SamplesPlayedSinceDisplayTimeOfNextCommitWasCalculated = 0;
    DisplayTimeOfNextCommit = Time;
    //SE_DEBUG(group_manifestor_audio_ksound, "DisplayTimeOfNextCommit %llu (delta %llu)\n",
    //                 DisplayTimeOfNextCommit, DisplayTimeOfNextCommit - LastDisplayTimeOfNextCommit);
    LastDisplayTimeOfNextCommit = DisplayTimeOfNextCommit;

    if (OutputState == STARTING)
    {
        SE_DEBUG(group_manifestor_audio_ksound,  "Switching from STARTING to MUTED after display time update\n");
        OutputState = MUTED;
    }
}


////////////////////////////////////////////////////////////////////////////
///
/// Make the best effort possible to honour the requested output timing.
///
/// At present this is achieved by dicking about rampantly with the sample count.
/// See Manifestor_AudioKsound_c::SetModuleParameters to see how we ensure space exists
/// to do this in.
///
/// \param StreamBufPtr  Pointer to the buffer to be timed.
/// \return Number of silent samples to inject (+ve) or number of existing samples to remove (-ve).
///
int Manifestor_AudioKsound_c::HandleRequestedOutputTiming(AudioStreamBuffer_t *StreamBufPtr)
{
    Rational_c PreciseAdjustment = StreamBufPtr->AudioOutputTiming->OutputRateAdjustment;
    unsigned int SampleCount = StreamBufPtr->AudioParameters->SampleCount;
    //unsigned int ChannelCount = StreamBufPtr->AudioParameters->Source.ChannelCount;
    unsigned int DisplayCount = StreamBufPtr->AudioOutputTiming->DisplayCount[0];
    unsigned long long SystemPlaybackTime = StreamBufPtr->AudioOutputTiming->SystemPlaybackTime;
    unsigned long long ActualSystemPlaybackTime = StreamBufPtr->AudioOutputTiming->ActualSystemPlaybackTime;
    unsigned int SamplingFrequency = StreamBufPtr->AudioParameters->Source.SampleRateHz;
    int SamplesRequiredToHonourDisplayCount;
    int SamplesRequiredToHonourSystemPlaybackTime;
    int SamplesRequiredToHonourEverything;
    int Adjustment;
    //
    // Send the tuning parameters to the frequency synthesiser
    //
    DerivePPMValueFromOutputRateAdjustment(PreciseAdjustment, &Adjustment, 0);
    Mixer->SetOutputRateAdjustment(Adjustment);
    //
    // Calculate how many samples to inject or remove
    //
    SamplesRequiredToHonourDisplayCount = DisplayCount - SampleCount;

    SE_VERBOSE(group_avsync, "Expected PlaybackTime=%lld ActualSystemPlaybackTime=%lld\n", SystemPlaybackTime, ActualSystemPlaybackTime);

    if (SamplesRequiredToHonourDisplayCount)
        SE_DEBUG(group_manifestor_audio_ksound, "Deploying percussive adjustment due to ::DisplayCount %u  SampleCount %u  Delta %d\n",
                 DisplayCount, SampleCount, DisplayCount - SampleCount);

    if (0 == SamplingFrequency)
    {
        SE_INFO(group_manifestor_audio_ksound, "samplingfreq 0; forcing default\n"); // shall replace with assert eventually
        SamplingFrequency = AUDIO_WA_SAMPlING_FREQUENCY;
    }

#if 0
    TimeRequiredToHonourDisplayCount = (SamplesRequiredToHonourDisplayCount * 1000000) / SamplingFrequency;
    ActualSystemPlaybackTime += TimeRequiredToHonourDisplayCount;
#endif

    //
    // Figure out what actions would have to be taken in order to honour the system playback time.
    //

    if (NotValidTime(SystemPlaybackTime))
    {
        SamplesRequiredToHonourSystemPlaybackTime = 0;
    }
    else
    {
        long long TimeRequiredToHonourSystemPlaybackTime(SystemPlaybackTime - ActualSystemPlaybackTime);

        if (TimeRequiredToHonourSystemPlaybackTime)
        {
            // 10 seconds - this is to match the maximum synchronization window in the output coordinator
            if (TimeRequiredToHonourSystemPlaybackTime > 10000000ll)
            {
                SE_ERROR("Playback time is more than 10 seconds in the future (%lld) - ignoring\n",
                         TimeRequiredToHonourSystemPlaybackTime);
                TimeRequiredToHonourSystemPlaybackTime = 0;
            }
            else if (TimeRequiredToHonourSystemPlaybackTime < 1000ll &&
                     TimeRequiredToHonourSystemPlaybackTime > -1000ll)
            {
                SE_DEBUG(group_manifestor_audio_ksound, "Suppressing small adjustment required to meet true SystemPlaybackTime (%lld)\n",
                         TimeRequiredToHonourSystemPlaybackTime);
                TimeRequiredToHonourSystemPlaybackTime = 0;
            }
            else if (TimeRequiredToHonourSystemPlaybackTime < 0ll)
            {
                // the sync engine should be using the DisplayCount to avoid this. If not then we do here.
                if (SamplesRequiredToHonourDisplayCount != 0)
                {
                    SE_DEBUG(group_manifestor_audio_ksound, "Playback time is in the past (%lld) - ignoring\n", TimeRequiredToHonourSystemPlaybackTime);
                    TimeRequiredToHonourSystemPlaybackTime = 0;
                }
                else
                {
                    SE_DEBUG(group_manifestor_audio_ksound, "Playback time is in past (%lld). Sync engine not using DisplayCount to avoid this. Deploying percussive adjustment\n", TimeRequiredToHonourSystemPlaybackTime);
                }
            }
            else
            {
                SE_DEBUG(group_manifestor_audio_ksound, "Deploying percussive adjustment due to ::SystemPlaybackTime %lld  Actual %lld  Delta %lld\n",
                         SystemPlaybackTime, ActualSystemPlaybackTime, TimeRequiredToHonourSystemPlaybackTime);
            }
        }

        SamplesRequiredToHonourSystemPlaybackTime = (TimeRequiredToHonourSystemPlaybackTime * (long long) SamplingFrequency) / 1000000ll;
    }

    SamplesRequiredToHonourEverything = SamplesRequiredToHonourDisplayCount +
                                        SamplesRequiredToHonourSystemPlaybackTime;

    if (!inrange(SamplesRequiredToHonourEverything, (int) - SampleCount, (int) SampleCount))
    {
        SE_VERBOSE(group_manifestor_audio_ksound, "SamplesRequiredToHonourEverything(%d) is larger than the buffer itself (%d samples) \n", SamplesRequiredToHonourEverything, SampleCount);
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
/// \param StreamBufPtr         Pointer to the buffer to be timed.
/// \param SamplesAlreadyPlayed Number of samples by which to offset the display time.
///
void Manifestor_AudioKsound_c::HandleAnticipatedOutputTiming(
    AudioStreamBuffer_t *StreamBufPtr, unsigned int SamplesAlreadyPlayed)
{
    unsigned int SamplingFrequency = StreamBufPtr->AudioParameters->Source.SampleRateHz;

    //
    // Calculate and record the output timings
    //

    if (NotValidTime(DisplayTimeOfNextCommit))
    {
        // this is perhaps a little optimistic but it better than nothing
        DisplayTimeOfNextCommit = OS_GetTimeInMicroSeconds();
    }

    if (0 == SamplingFrequency)
    {
        SE_INFO(group_manifestor_audio_ksound, "samplingfreq 0; forcing default\n"); // shall replace with assert eventually
        SamplingFrequency = AUDIO_WA_SAMPlING_FREQUENCY;
    }

    Rational_c TimeForSamplesAlreadyPlayedInSeconds(SamplesAlreadyPlayed, SamplingFrequency);

    if (StreamBufPtr->AudioOutputTiming->SystemClockAdjustment != 0)
    {
        TimeForSamplesAlreadyPlayedInSeconds        /=  StreamBufPtr->AudioOutputTiming->SystemClockAdjustment;
    }

    StreamBufPtr->AudioOutputTiming->ActualSystemPlaybackTime =
        DisplayTimeOfNextCommit + IntegerPart(1000000 * TimeForSamplesAlreadyPlayedInSeconds);
    SE_DEBUG(group_manifestor_audio_ksound, "ActualPlaybackTime %llu (delta %llu) @ offset %u\n",
             StreamBufPtr->AudioOutputTiming->ActualSystemPlaybackTime,
             StreamBufPtr->AudioOutputTiming->ActualSystemPlaybackTime -
             LastActualSystemPlaybackTime,
             SamplesAlreadyPlayed);

    // squirrel away the last claimed playback time so we can display the delta
    LastActualSystemPlaybackTime = StreamBufPtr->AudioOutputTiming->ActualSystemPlaybackTime;
}

ManifestorStatus_t Manifestor_AudioKsound_c::ShortenDataBuffer(MME_DataBuffer_t *DataBuffer,
                                                               tMixerFrameParams *MixerFrameParams,
                                                               uint32_t SamplesToRemoveBeforeResampling,
                                                               uint32_t IdealisedStartOffset,
                                                               uint32_t EndOffsetAfterResampling,
                                                               Rational_c RescaleFactor,
                                                               bool Muted)
{
    unsigned int EndOffsetBeforeResampling = (RescaleFactor * EndOffsetAfterResampling).RoundedUpIntegerPart();
    uint32_t TotalSamples = BytesToSamples(DataBuffer->TotalSize);
    unsigned int MaximumToRemove = TotalSamples - EndOffsetBeforeResampling;
    uint32_t BytesToRemove = SamplesToBytes(SamplesToRemoveBeforeResampling);
    uint32_t PageOffset;
    MME_ScatterPage_t *Pages = &DataBuffer->ScatterPages_p[0];

    if (SamplesToRemoveBeforeResampling > MaximumToRemove)
    {
        SE_ERROR("SamplesToRemoveBeforeResampling is too large (%d, max %d)\n",
                 SamplesToRemoveBeforeResampling, MaximumToRemove);
        return ManifestorError;
    }

    Dropped += SamplesToRemoveBeforeResampling;

    if (OutputState != PLAYING)
    {
        //
        // Output is silent, must remove samples from the front of the buffer
        //
        MixerFrameParams->Command = MIXER_PLAY;
        OutputState = PLAYING;

        if (Muted)
        {
            MixerFrameParams->Command = MIXER_MUTE;
            OutputState = MUTED;
        }

        MixerFrameParams->StartOffset = 0;
        SE_DEBUG(group_manifestor_audio_ksound, "@:%s MixerFrameParams: %p->Command: %s ->StartOffset: %d SamplesToRemoveBeforeResampling: %d\n",
                 LookupState(), MixerFrameParams, Mixer_Mme_c::LookupMixerCommand((enum eMixerCommand) MixerFrameParams->Command), MixerFrameParams->StartOffset, SamplesToRemoveBeforeResampling);
        // start trimming the buffers from the front of the first page
        PageOffset = 0;
    }
    else
    {
        //
        // Output is playing, can choose between MIXER_FOFI and MIXER_PAUSE
        //
        int ApplyChangeAt;
        //
        // Search for the first seam that is at least 128 samples into the buffer
        //
        PageOffset = 0;
        uint32_t AccumulatedOffset = BytesToSamples(Pages[PageOffset].Size);

        while (AccumulatedOffset < SamplesNeededForFadeOutBeforeResampling)
        {
            AccumulatedOffset += BytesToSamples(Pages[++PageOffset].Size);
        }

        ApplyChangeAt = AccumulatedOffset - SamplesToRemoveBeforeResampling;

        if (ApplyChangeAt < (int) SamplesNeededForFadeOutBeforeResampling)
        {
            ApplyChangeAt = SamplesNeededForFadeOutBeforeResampling;
        }

        if (ApplyChangeAt <= (int)(EndOffsetBeforeResampling - SamplesNeededForFadeOutBeforeResampling))
        {
            //
            // Deploy MIXER_FOFI
            //
            MixerFrameParams->Command = MIXER_FOFI;
            MixerFrameParams->StartOffset = (ApplyChangeAt / RescaleFactor).RoundedIntegerPart();
            // OutputState is already correct;
            SE_DEBUG(group_manifestor_audio_ksound,  "Removing %d samples with MIXER_FOFI at %d\n",
                     SamplesToRemoveBeforeResampling, MixerFrameParams->StartOffset);
            uint32_t SamplesRemoved = AccumulatedOffset - ApplyChangeAt;
            uint32_t BytesRemoved = SamplesToBytes(SamplesRemoved);
            BytesToRemove -= BytesRemoved;
            SamplesToRemoveBeforeResampling -= SamplesRemoved;
            DataBuffer->TotalSize -= BytesRemoved;
            Pages[PageOffset].Size -= BytesRemoved;

            if (0 == SamplesToRemoveBeforeResampling)
            {
                return ManifestorNoError;
            }

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
            SE_DEBUG(group_manifestor_audio_ksound,  "@: %s Carrying over %d samples with MIXER_PAUSE at %d\n",
                     LookupState(), SamplesToRemoveBeforeResampling, MixerFrameParams->StartOffset);
            SamplesToRemoveWhenPlaybackCommences = SamplesToRemoveBeforeResampling;
            SE_DEBUG(group_manifestor_audio_ksound, "SamplesToRemoveWhenPlaybackCommences: -> %d\n", SamplesToRemoveWhenPlaybackCommences);
            // Don't fall through into the sample destruction code below
            return ManifestorNoError;
        }
    }

    // completely remove whole pages at the front
    while (Pages[PageOffset].Size < BytesToRemove)
    {
        BytesToRemove -= Pages[PageOffset].Size;
        SamplesToRemoveBeforeResampling -= BytesToSamples(Pages[PageOffset].Size);
        DataBuffer->TotalSize -= Pages[PageOffset].Size;
        Pages[PageOffset].Size = 0;
        PageOffset++;

        if (PageOffset >= DataBuffer->NumberOfScatterPages)
        {
            SE_ERROR("PageOffset overflowed - probable implementation bug\n");
            return ManifestorError;
        }

        SE_ASSERT(BytesToRemove == SamplesToBytes(SamplesToRemoveBeforeResampling));
    }

    unsigned char *DataPtr = (unsigned char *) Pages[PageOffset].Page_p;
    Pages[PageOffset].Page_p = (void *)(DataPtr + BytesToRemove);
    Pages[PageOffset].Size -= BytesToRemove;
    DataBuffer->TotalSize -= BytesToRemove;
    return ManifestorNoError;
}

ManifestorStatus_t Manifestor_AudioKsound_c::ExtendDataBuffer(MME_DataBuffer_t *DataBuffer,
                                                              tMixerFrameParams *MixerFrameParams,
                                                              uint32_t SamplesToInjectBeforeResampling,
                                                              uint32_t EndOffsetAfterResampling,
                                                              Rational_c RescaleFactor,
                                                              bool Muted)
{
    uint32_t EndOffsetBeforeResampling = (RescaleFactor * EndOffsetAfterResampling).RoundedUpIntegerPart();
    uint32_t SamplesToInjectAfterResampling =
        (SamplesToInjectBeforeResampling / RescaleFactor).RoundedIntegerPart();

    if (SamplesToInjectBeforeResampling > EndOffsetBeforeResampling)
    {
        SE_ERROR("SamplesToInjectBeforeResampling is too large (%d, max %d)\n", SamplesToInjectBeforeResampling, EndOffsetBeforeResampling);
        return ManifestorError;
    }

    if (OutputState == PLAYING)
    {
        MixerFrameParams->Command = MIXER_PAUSE;
        MixerFrameParams->StartOffset = EndOffsetAfterResampling - SamplesToInjectAfterResampling;
        OutputState = MUTED;
        SE_DEBUG(group_manifestor_audio_ksound,  "Injecting %d samples with MIXER_PAUSE at %d whilst %s\n",
                 SamplesToInjectBeforeResampling, MixerFrameParams->StartOffset, LookupState());
    }
    else
    {
        SE_DEBUG(group_manifestor_audio_ksound,  "Injecting %d samples  whilst %s\n", SamplesToInjectBeforeResampling, LookupState());
        MixerFrameParams->Command = MIXER_PLAY;
        OutputState = PLAYING;

        if (Muted)
        {
            MixerFrameParams->Command = MIXER_MUTE;
            OutputState = MUTED;
        }

        MixerFrameParams->StartOffset = SamplesToInjectAfterResampling;
    }

    SE_DEBUG(group_manifestor_audio_ksound, "<:%s MixerFrameParams: %p->Command: %s\n", LookupState(), MixerFrameParams, Mixer_Mme_c::LookupMixerCommand((enum eMixerCommand) MixerFrameParams->Command));
    return ManifestorNoError;
}


////////////////////////////////////////////////////////////////////////////
///
/// Update the manifestation parameters (if required)
///
ManifestorStatus_t Manifestor_AudioKsound_c::UpdateAudioParameters(AudioStreamBuffer_t *StreamBufPtr,
                                                                   bool TakeMixerClientLock)
{
    ManifestorStatus_t Status;
    ParsedAudioParameters_t *AudioParameters;
    ParsedAudioParameters_t  CurrentAudioParameters;
    //
    //  Check if there are new audio parameters (i.e. change of sample rate etc.)
    //  excluding sample-count,PaOffSet and note this
    //
    memcpy(&CurrentAudioParameters, &InputAudioParameters, sizeof(InputAudioParameters));
    CurrentAudioParameters.SampleCount = StreamBufPtr->AudioParameters->SampleCount;
    CurrentAudioParameters.BackwardCompatibleProperties.SampleCount = StreamBufPtr->AudioParameters->BackwardCompatibleProperties.SampleCount;
    CurrentAudioParameters.SpdifInProperties.PaOffSetInCompressedBuffer = StreamBufPtr->AudioParameters->SpdifInProperties.PaOffSetInCompressedBuffer;

    if (0 != memcmp(&CurrentAudioParameters, StreamBufPtr->AudioParameters,
                    sizeof(CurrentAudioParameters)))
    {
        SE_DEBUG(group_manifestor_audio_ksound, "Updating audio parameters\n");
        AudioParameters = StreamBufPtr->AudioParameters;

        SE_ASSERT(AudioParameters->Source.ChannelCount);
        SE_ASSERT(AudioParameters->Source.BitsPerSample);

        if (AudioParameters->Source.ChannelCount != SurfaceDescriptor.ChannelCount)
        {
            SE_ERROR("Codec did not honour the surface's channel interleaving (%d instead of %d)\n",
                     AudioParameters->Source.ChannelCount, SurfaceDescriptor.ChannelCount);
            // this error is non-fatal (providing the codec doesn't run past the end of its buffers)
        }

        //
        // Memorise the new audio parameters.
        //

        InputAudioParameters = *AudioParameters;
        Status = Mixer->UpdateManifestorParameters(this, AudioParameters, TakeMixerClientLock);
        if (Status != PlayerNoError)
        {
            SE_ERROR("Cannot update the mixer parameters\n");
            return ManifestorError;
        }
    }

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
PcmPlayer_c::OutputEncoding Manifestor_AudioKsound_c::LookupCodedDtsDataBufferOutputEncoding(const ParsedAudioParameters_t *ParsedAudioParameters,
                                                                                             uint32_t CodedDataBufferSize,
                                                                                             uint32_t *RepetitionPeriod) const
{
    uint32_t CompatibleSampleCount = ParsedAudioParameters->BackwardCompatibleProperties.SampleCount;
    uint32_t Iec60958Size = CompatibleSampleCount * 4; /* stereo, 16-bit */
    uint32_t MaxIec61937Size = Iec60958Size - 8; /* PA/PB/PC/PD @ 16-bits each */

    // in case of dtshd stream without compatible core (i.e. XLL only stream),
    // the transcoded buffer will be void, so fallback to PCM
    if (CodedDataBufferSize == 0)
    {
        *RepetitionPeriod = 1;
        return PcmPlayer_c::OUTPUT_IEC60958;
    }
    else if (CodedDataBufferSize > MaxIec61937Size)
    {
        if (CodedDataBufferSize == Iec60958Size)
        {
            return PcmPlayer_c::BYPASS_DTS_CDDA;
        }

        return PcmPlayer_c::OUTPUT_IEC60958;
    }

    *RepetitionPeriod = CompatibleSampleCount;

    switch (CompatibleSampleCount)
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
/// Study a coded data buffer and identify what output encoding to use (AAC case)
///
/// \param CodedDataBufferSize The caller usually has this to hand so we can avoid another lookup
/// \param RepetitionPeriod Pointer to where the repetition period of the coded data should be stored
/// \return Output encoding to be used.
///
PcmPlayer_c::OutputEncoding Manifestor_AudioKsound_c::LookupCodedAacDataBufferOutputEncoding(Buffer_c *CodedFrameBuffer,
                                                                                             const ParsedAudioParameters_t *ParsedAudioParameters,
                                                                                             const ParsedFrameParameters_t *ParsedFrameParameters,
                                                                                             uint32_t *CodedFrameSampleCount,
                                                                                             uint32_t *RepetitionPeriod) const
{
    switch (ParsedAudioParameters->OriginalEncoding)
    {
    case AudioOriginalEncodingAAC:
    {
        if (ParsedFrameParameters == NULL)
        {
            SE_ERROR("NULL AAC Frame Parameters -> Cannot bypass, fallback to PCM\n");
            return PcmPlayer_c::OUTPUT_IEC60958;
        }
        if (ParsedFrameParameters->SizeofFrameParameterStructure != sizeof(AacAudioFrameParameters_t))
        {
            SE_ERROR("Invalid AAC Frame Parameters -> Cannot bypass, fallback to PCM\n");
            return PcmPlayer_c::OUTPUT_IEC60958;
        }

        AacAudioFrameParameters_t *AacParams = (AacAudioFrameParameters_t *) ParsedFrameParameters->FrameParameterStructure;

        if (AacParams->Type != AAC_ADTS_FORMAT)
        {
            SE_DEBUG(group_manifestor_audio_ksound,
                     "Only ADTS AAC bypass is supported (Current type:%d) fallback to PCM\n", AacParams->Type);
            return PcmPlayer_c::OUTPUT_IEC60958;
        }

        *CodedFrameSampleCount = (ParsedAudioParameters->BackwardCompatibleProperties.SampleCount) ?
                                 ParsedAudioParameters->BackwardCompatibleProperties.SampleCount :
                                 ParsedAudioParameters->SampleCount;

        *RepetitionPeriod = 1024;
        return PcmPlayer_c::BYPASS_AAC;
    }

    // Today [February 2015] HE-AAC bypass is not supported.
    //
    // Note that unless there is an explicit request through sbr_96k_enable in STM_SE_CTRL_AAC_DECODER_CONFIG
    // HE-AAC 96k/48k is not considered as HE-AAC by Decoder but it is considered as AAC 48kHz
    // Thus in this case, original encoding is AudioOriginalEncodingAAC, and bypass remains possible.
    //
    case AudioOriginalEncodingHEAAC_960:
    case AudioOriginalEncodingHEAAC_1024:
    case AudioOriginalEncodingHEAAC_1920:
    case AudioOriginalEncodingHEAAC_2048:
        SE_VERBOSE(group_manifestor_audio_ksound, "HE-AAC bypass is not supported, Fallback to PCM\n");
        *RepetitionPeriod = 1;
        return PcmPlayer_c::OUTPUT_IEC60958;

    default:
        SE_ASSERT("Invalid Encoding type\n");
        return PcmPlayer_c::OUTPUT_IEC60958;
    }
}

////////////////////////////////////////////////////////////////////////////
///
/// Study a coded data buffer and identify what output encoding to use.
///
/// \param CodedDataBuffer Coded data buffer to extract the meta data from
/// \param CodedDataBufferSize The caller usually has this to hand so we can avoid another lookup
/// \param RepetitionPeriod Pointer to where the repetition period of the coded data should be stored
/// \return Output encoding to be used.
///
PcmPlayer_c::OutputEncoding Manifestor_AudioKsound_c::LookupCodedDataBufferOutputEncoding(Buffer_c *CodedFrameBuffer,
                                                                                          const ParsedAudioParameters_t *ParsedAudioParameters,
                                                                                          const ParsedFrameParameters_t *ParsedFrameParameters,
                                                                                          uint32_t CodedDataBufferSize,
                                                                                          uint32_t *RepetitionPeriod,
                                                                                          uint32_t *Oversampling,
                                                                                          uint32_t *CodedFrameSampleCount,
                                                                                          BypassPhysicalChannel_t BypassChannel,
                                                                                          bool bypassSD)
{
    SE_ASSERT(ParsedAudioParameters != NULL);

    SE_DEBUG(group_manifestor_audio_ksound, ">: %d %u %u %u %u\n",
             ParsedAudioParameters->OriginalEncoding,
             ParsedAudioParameters->Source.BitsPerSample,
             ParsedAudioParameters->Source.ChannelCount,
             ParsedAudioParameters->Source.SampleRateHz,
             ParsedAudioParameters->SampleCount);
    *CodedFrameSampleCount = ParsedAudioParameters->SampleCount;
    *Oversampling          = 1;
    *RepetitionPeriod      = *CodedFrameSampleCount;

    switch (ParsedAudioParameters->OriginalEncoding)
    {
    case AudioOriginalEncodingUnknown:
    case AudioOriginalEncodingMax: // added only to avoid warning for not handled value in switch
        *RepetitionPeriod = 1;
        return PcmPlayer_c::OUTPUT_IEC60958;

    case AudioOriginalEncodingSPDIFIn_Compressed:
        *RepetitionPeriod = ParsedAudioParameters->SampleCount;
        if (ParsedAudioParameters->SpdifInProperties.SpdifInStreamType == SPDIF_DDPLUS)
        {
            *Oversampling     = 4;
        }
        return PcmPlayer_c::BYPASS_SPDIFIN_COMPRESSED;

    case AudioOriginalEncodingSPDIFIn_Pcm:
        *RepetitionPeriod = ParsedAudioParameters->SampleCount;
        return PcmPlayer_c::BYPASS_SPDIFIN_PCM;

    case AudioOriginalEncodingAc3:
        return PcmPlayer_c::BYPASS_AC3;

    case AudioOriginalEncodingDdplus:
        if (BypassChannel == SPDIF)
        {
            // by this point we've already located the transcoded buffer
            return PcmPlayer_c::BYPASS_AC3;
        }
        else if (BypassChannel == HDMI && bypassSD)
        {
            // by this point we've already located the transcoded buffer
            SE_DEBUG(group_manifestor_audio_ksound, "AudioOriginalEncodingDdplus BypassSD => Force HDMI to use AC3 transcoded buffer\n");
            return PcmPlayer_c::BYPASS_AC3;
        }
        else
        {
            *RepetitionPeriod = 6144;
            *Oversampling     = 4;
            return PcmPlayer_c::BYPASS_DDPLUS;
        }

    case AudioOriginalEncodingDts:
        return LookupCodedDtsDataBufferOutputEncoding(ParsedAudioParameters, CodedDataBufferSize, RepetitionPeriod);

    case AudioOriginalEncodingDtshd:
    case AudioOriginalEncodingDtshdMA:
        if (BypassChannel == SPDIF)
        {
            // a transcoded buffer is available...
            return LookupCodedDtsDataBufferOutputEncoding(ParsedAudioParameters, CodedDataBufferSize, RepetitionPeriod);
        }
        else if (BypassChannel == HDMI && bypassSD)
        {
            // a transcoded buffer is available...
            return LookupCodedDtsDataBufferOutputEncoding(ParsedAudioParameters, CodedDataBufferSize, RepetitionPeriod);
        }
        else
        {
            if (ParsedAudioParameters->OriginalEncoding == AudioOriginalEncodingDtshd)
            {
                *Oversampling     = 2;
                *RepetitionPeriod = 2048;
                return PcmPlayer_c::BYPASS_DTSHD_HR;
            }
            else
            {
                // repetition frame is 8k Samples Whatever the number of samples in the dtshd frame.
                *Oversampling     = SCALABLE_BYPASS_OVERSAMPLING;
                *RepetitionPeriod = 8192;
                return PcmPlayer_c::BYPASS_DTSHD_MA;
            }
        }

    case AudioOriginalEncodingDtshdLBR:
        if (BypassChannel == SPDIF)
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
        if (BypassChannel == SPDIF)
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

    case AudioOriginalEncodingAAC:
    case AudioOriginalEncodingHEAAC_960:
    case AudioOriginalEncodingHEAAC_1024:
    case AudioOriginalEncodingHEAAC_1920:
    case AudioOriginalEncodingHEAAC_2048:
        return LookupCodedAacDataBufferOutputEncoding(CodedFrameBuffer,
                                                      ParsedAudioParameters,
                                                      ParsedFrameParameters,
                                                      CodedFrameSampleCount,
                                                      RepetitionPeriod);

    case AudioOriginalEncodingDPulse:
        if (BypassChannel == SPDIF)
        {
            // There is AC3 transcode operation @48 kHz so set number of sample count for coded to 1536.
            *Oversampling = SCALABLE_BYPASS_OVERSAMPLING;
            *CodedFrameSampleCount = 1536;
            *RepetitionPeriod = 1536;
            SE_DEBUG(group_manifestor_audio_ksound, "<: AudioOriginalEncodingDPulse -> %s with %u %u\n",
                     PcmPlayer_c::LookupOutputEncoding(PcmPlayer_c::BYPASS_AC3),
                     *CodedFrameSampleCount,
                     *RepetitionPeriod);
            return PcmPlayer_c::BYPASS_AC3; /* DolbyPulse do transcoding in AC3 */
        }
        else
        {
            *RepetitionPeriod     = 1024;   /* For HDMI output bypass AAC compressed frame */
            SE_DEBUG(group_manifestor_audio_ksound, "<: AudioOriginalEncodingDPulse -> %s with %u %u\n",
                     PcmPlayer_c::LookupOutputEncoding(PcmPlayer_c::BYPASS_AAC),
                     *CodedFrameSampleCount,
                     *RepetitionPeriod);
            return PcmPlayer_c::BYPASS_AAC;
        }
    }

    SE_ERROR("Unexpected audio coded data format (%d)\n", ParsedAudioParameters->OriginalEncoding);
    *RepetitionPeriod = 1;
    return PcmPlayer_c::OUTPUT_IEC60958;
}

////////////////////////////////////////////////////////////////////////////
///
/// Dequeue NPages coded buffers from PageIdx page and release them.
///
/// There is no error checking here in order to prevent callers having to check the return code then they
/// 'know' it will be successful. This does, of course, make the caller responsible for checking that the
/// call is safe.
///
/// \param CodedMmeDataBuffer The coded data buffer to dequeue
/// \param PageIdx            Offset of the first page to dequeue
/// \param NPages             Number of pages to dequeue
/// \param ThisCodedInput     Source of Coded data
///
void Manifestor_AudioKsound_c::DequeueCodedDataBuffer(MME_DataBuffer_t *CodedMmeDataBuffer, uint32_t PageIdx , uint32_t NPages, CodedDataInput_c ThisCodedInput)
{
    uint32_t i;
    SE_DEBUG(group_manifestor_audio_ksound, ">: %p NbScatterPages(%d) PageIdx:%d NPages:%d\n",
             CodedMmeDataBuffer, CodedMmeDataBuffer->NumberOfScatterPages, PageIdx, NPages);

    if (NPages == 0)
    {
        return;
    }

    SE_ASSERT(CodedMmeDataBuffer->NumberOfScatterPages >= PageIdx + NPages);

    if (PageIdx == 0)
    {
        // Set StartOffset to 0 because first page is dequeued
        CodedMmeDataBuffer->StartOffset = 0;
        ThisCodedInput.SetFirstBufferPartiallyConsumed(false);
    }

    for (i = 0; i < NPages; i++)
    {
        Buffer_c *CodedFrameBuffer = (Buffer_c *)(((unsigned int *)CodedMmeDataBuffer->UserData_p)[PageIdx + i]);
        CodedFrameBuffer->DecrementReferenceCount(IdentifierManifestor);

        CodedMmeDataBuffer->TotalSize -= CodedMmeDataBuffer->ScatterPages_p[PageIdx + i].Size;
        // TODO: should check --/++ operation
        CodedMmeDataBuffer->NumberOfScatterPages--;
    }

    memmove(&CodedMmeDataBuffer->ScatterPages_p[PageIdx],
            &CodedMmeDataBuffer->ScatterPages_p[PageIdx + NPages],
            (CodedMmeDataBuffer->NumberOfScatterPages - PageIdx) * sizeof(CodedMmeDataBuffer->ScatterPages_p[0]));
    memmove(&((unsigned int *) CodedMmeDataBuffer->UserData_p)[PageIdx],
            &((unsigned int *) CodedMmeDataBuffer->UserData_p)[PageIdx + NPages],
            (CodedMmeDataBuffer->NumberOfScatterPages - PageIdx) * sizeof(unsigned int));
}

////////////////////////////////////////////////////////////////////////////
///
/// Examine the queued compressed data buffers and determine the exact lengths (in sample frames).
///
uint32_t Manifestor_AudioKsound_c::LookupCodedDataBufferLength(MME_DataBuffer_t *CodedDataBuffer,
                                                               CodedDataInput_c &ThisCodedInput)
{
    uint32_t Length = 0;

    if (PcmPlayer_c::BYPASS_SPDIFIN_COMPRESSED == ThisCodedInput.GetEncoding())
    {
        //SpdifIn Compressed buffers are not frame align.
        Length = BytesToSpdifSamples(CodedDataBuffer->TotalSize);
        if (ThisCodedInput.IsFirstBufferPartiallyConsumed())
        {
            Length -= BytesToSpdifSamples(CodedDataBuffer->StartOffset);
        }
    }
    else
    {
        // calculate the 'fundamental' length
        for (unsigned int CodedScatterPageIdx(0); CodedScatterPageIdx < CodedDataBuffer->NumberOfScatterPages; CodedScatterPageIdx++)
        {
            Length += AUDIOMIXER_GET_REPETITION_PERIOD(&CodedDataBuffer->ScatterPages_p[CodedScatterPageIdx]);
        }

        // add in any sample frames pending at the mixer
        Length += ThisCodedInput.GetSamplesUntilNextRepetitionPeriod();

        // remove the head of the queue if it is partial consumed (that was already accounted for by the above
        // addition)
        if (ThisCodedInput.IsFirstBufferPartiallyConsumed())
        {
            Length -= AUDIOMIXER_GET_REPETITION_PERIOD(&CodedDataBuffer->ScatterPages_p[0]);
        }
    }

    SE_DEBUG(group_manifestor_audio_ksound, "<: %p %d\n", &ThisCodedInput, Length);
    return Length;
}


bool Manifestor_AudioKsound_c::DoesTranscodedBufferExist(BypassPhysicalChannel_t BypassChannel,
                                                         const ParsedAudioParameters_t *AudioParameters,
                                                         bool bypassSD)
{
    // local table to prevent from being accessed apart from this method
    // this table tells whether a transcoded buffer exists according to the
    // stream format and output type
    const static bool IsTranscodedMatrix[BypassPhysicalChannelCount][AudioOriginalEncodingMax] =
    {
        // None   AC3    DD+    DTS    DTSHD  DTSHDMA  DTSHDLBR  TrueHD  AAC    HEAAC                       DPulse  over SPDIF  SpdifIn Compressed SpdifIn PCM
        { false,  false, true,  false, true,  true,    false,    false,  false, false, false, false, false, true               , true              , false},
        // None   AC3    DD+    DTS    DTSHD  DTSHDMA  DTSHDLBR  TrueHD  AAC    HEAAC                       DPulse  over HDMI   SpdifIn Compressed SpdifIn PCM
        { false,  false, false, false, false, false,   false,    true,   false, false, false, false, false, false              , false             , false}
    };
    const static bool IsTranscodedMatrixSD[BypassPhysicalChannelCount][AudioOriginalEncodingMax] =
    {
        // None   AC3    DD+    DTS    DTSHD  DTSHDMA  DTSHDLBR  TrueHD  AAC    HEAAC                       DPulse  over SPDIF  SpdifIn Compressed SpdifIn PCM
        { false,  false, true,  false, true,  true,    false,    false,  false, false, false, false, false, true               , true              , false},
        // None   AC3    DD+    DTS    DTSHD  DTSHDMA  DTSHDLBR  TrueHD  AAC    HEAAC                       DPulse  over HDMI   SpdifIn Compressed SpdifIn PCM
        { false,  false, true,  false, true,  true,    false,    true,   false, false, false, false, false, false              , false             , false}
    };
    AudioOriginalEncoding_t Encoding = AudioParameters->OriginalEncoding;
    SE_DEBUG(group_manifestor_audio_ksound, ">: %d\n", Encoding);

    if ((BypassChannel < BypassPhysicalChannelCount) &&
        (Encoding < AudioOriginalEncodingMax))
    {
        bool TranscodedBufferExists = bypassSD ? IsTranscodedMatrixSD[BypassChannel][Encoding]
                                      : IsTranscodedMatrix[BypassChannel][Encoding];

        // special case of DTS-HD: a dts core might not be present...
        if ((Encoding == AudioOriginalEncodingDtshdMA) ||
            (Encoding == AudioOriginalEncodingDtshd))
        {
            if (!AudioParameters->BackwardCompatibleProperties.SampleRateHz || !AudioParameters->BackwardCompatibleProperties.SampleCount)
            {
                // no core is present, so transcoding is not possible
                TranscodedBufferExists = false;
            }
        }

        SE_DEBUG(group_manifestor_audio_ksound, "<: %s %d %s\n",
                 (BypassChannel == SPDIF) ? "SPDIF" : "HDMI",
                 Encoding,
                 (true == TranscodedBufferExists) ? "true" : "false");
        return TranscodedBufferExists;
    }
    else
    {
        SE_ERROR("Wrong Bypass channel (%d) or encoding (%d)\n", BypassChannel, Encoding);
        return false;
    }
}

bool Manifestor_AudioKsound_c::DoesCompressedFrameBufferExist(BypassPhysicalChannel_t BypassChannel, const ParsedAudioParameters_t *AudioParameters)
{
    AudioOriginalEncoding_t Encoding = AudioParameters->OriginalEncoding;

    if (BaseComponentClass_c::EnableCoprocessorParsing == 1) // CompressedFrameBuffer available only in the stream based decoding
    {
        if ((Encoding >=  AudioOriginalEncodingAAC) && (Encoding <=  AudioOriginalEncodingDPulse)) //CompressedFrameBuffer available for all AAC format
        {
            SE_DEBUG(group_manifestor_audio_ksound, "<: %s %d true\n", (BypassChannel == SPDIF) ? "SPDIF" : "HDMI", Encoding);
            return true;
        }
    }
    if ((AudioParameters->OriginalEncoding == AudioOriginalEncodingSPDIFIn_Compressed) || (AudioParameters->OriginalEncoding == AudioOriginalEncodingSPDIFIn_Pcm))
    {
        SE_DEBUG(group_manifestor_audio_ksound, "<: %s %d true\n", (BypassChannel == SPDIF) ? "SPDIF" : "HDMI", Encoding);
        return true;
    }
    SE_DEBUG(group_manifestor_audio_ksound,
             "Compressed Frame Buffer not Exist for <: %d %s Encoding\n", Encoding, (BypassChannel == SPDIF) ? "SPDIF" : "HDMI");
    return false;
}

////////////////////////////////////////////////////////////////////////////
///
/// Manifestor_AudioKsound_c::FillOutCodedMmeBufferArray
///
/// \param in  CodedDataBufferArray  array of MME buffer that has to be filled
/// \param in  CodedFrameBuffer      pointer to CodedFrameBuffer generated by Decoder
/// \param in  curStreamBuf          pointer to current StreamBuffer entry
/// \param in  TranscodedFrameBuffer pointer to TranscodedFrameBuffer generated by Decoder
/// \param in  CompressedFrameBuffer pointer to CompressedFrameBuffer generated by Decoder
/// \param in  BypassChannelSPDIF    true if SPDIF bypass is enabled
/// \param in  BypassChannelHDMI     true if HDMI bypass is enabled
/// \param in  BypassSDChannelSPDIF  true if SPDIF SD bypass is enabled
/// \param in  BypassSDChannelHDMI   true if HDMI SD bypass is enabled
///
/// \return    ManifestorStatus_t    ManifestorNoError in case of success,
///                                  appropriate ManifestorStatus_t otherwise
///
ManifestorStatus_t Manifestor_AudioKsound_c::FillOutCodedMmeBufferArray(MME_DataBuffer_t CodedDataBufferArray[BypassPhysicalChannelCount],
                                                                        Buffer_c *CodedFrameBuffer,
                                                                        AudioStreamBuffer_t *curStreamBuf,
                                                                        Buffer_c *TranscodedFrameBuffer,
                                                                        Buffer_c *CompressedFrameBuffer,
                                                                        bool BypassChannelSPDIF,
                                                                        bool BypassChannelHDMI,
                                                                        bool BypassSDChannelSPDIF,
                                                                        bool BypassSDChannelHDMI)
{
    BufferStatus_t              Status          = BufferNoError;
    Buffer_c                   *Buffer          = curStreamBuf->Buffer;

    bool                        bypassSD;
    bool                        IsTranscoded;
    bool                        IsCompressed;
    PcmPlayer_c::OutputEncoding CurrentBufferEncoding;
    uint32_t                    CurrentBufferRepetitionPeriod;
    BypassPhysicalChannel_t     bypassChannel;

    for (uint32_t CodedIndex = 0; CodedIndex < CodedDataInput_c::BYPASS_CHANNEL_MAX_NUM; CodedIndex++)
    {
        MME_DataBuffer_t *CurrentCodedMmeDataBuffer;
        Buffer_c *CurrentCodedFrameBuffer = CodedFrameBuffer;
        bypassChannel = (Manifestor_AudioKsound_c::BypassPhysicalChannel_t) CodedInput[CodedIndex].GetBypassChannel();
        bypassSD = false;
        bool DoFill = false;

        if (CodedDataInput_c::BYPASS_SPDIF_IDX == CodedIndex)
        {
            if (true == BypassChannelSPDIF)
            {
                CurrentCodedMmeDataBuffer = &CodedDataBufferArray[Mixer_Mme_c::MIXER_CODED_DATA_INPUT_SPDIF_IDX];
                bypassSD = BypassSDChannelSPDIF;
                DoFill = true;
            }
        }
        else if (CodedDataInput_c::BYPASS_HDMI_IDX == CodedIndex)
        {
            if (true == BypassChannelHDMI)
            {
                CurrentCodedMmeDataBuffer = &CodedDataBufferArray[Mixer_Mme_c::MIXER_CODED_DATA_INPUT_HDMI_IDX];
                bypassSD = BypassSDChannelHDMI;
                DoFill = true;
            }
        }

        if (DoFill == false)
        {
            // This coded input is out of interest.
            continue;
        }

        IsTranscoded = DoesTranscodedBufferExist(bypassChannel, curStreamBuf->AudioParameters, bypassSD);

        if (IsTranscoded && (TranscodedFrameBuffer == NULL))// Give the Preference to the transcoded buffer over the compressed buffer
        {
            SE_DEBUG(group_manifestor_audio_ksound, "Transcoded frame buffer is null\n");
            continue;
        }
        else if (IsTranscoded)
        {
            CurrentCodedFrameBuffer = TranscodedFrameBuffer;
        }
        else
        {
            IsCompressed = DoesCompressedFrameBufferExist(bypassChannel, curStreamBuf->AudioParameters);

            if (IsCompressed && (CompressedFrameBuffer == NULL))
            {
                SE_DEBUG(group_manifestor_audio_ksound, "Compressed frame buffer is null\n");
                continue;
            }
            if (BypassChannelHDMI || BypassSDChannelHDMI)
            {
                CompressedFrameRequested = true;
            }
            else
            {
                CompressedFrameRequested = false;
            }

            CurrentCodedFrameBuffer = IsCompressed ? CompressedFrameBuffer : CodedFrameBuffer;
        }

        ((unsigned int *) CurrentCodedMmeDataBuffer->UserData_p)[CurrentCodedMmeDataBuffer->NumberOfScatterPages] = (unsigned int)CodedFrameBuffer;
        MME_ScatterPage_t *CurrentPage = &CurrentCodedMmeDataBuffer->ScatterPages_p[CurrentCodedMmeDataBuffer->NumberOfScatterPages];
        Status = (CurrentCodedFrameBuffer)->ObtainDataReference(NULL, &(CurrentPage->Size), &(CurrentPage->Page_p));
        SE_DEBUG(group_manifestor_audio_ksound, "CurrentPage->Size %d\n", CurrentPage->Size);

        if (BufferNoError != Status)
        {
            if (PcmPlayer_c::OUTPUT_IEC60958 != CodedInput[CodedIndex].GetEncoding())
            {
                SE_DEBUG(group_manifestor_audio_ksound,  "Detected coded data format change on (%s -> NONE)\n",
                         PcmPlayer_c::LookupOutputEncoding(CodedInput[CodedIndex].GetEncoding()));
            }

            // either we are already in OUTPUT_IEC60958 mode (in which case we want to exit the loop
            // immediately) or we need to flush out the coded data already queued up (in which case we
            // want to exit the loop immediately).
            break;
        }
        {
            uint32_t Oversampling(0);
            uint32_t CodedFrameSampleCount(0);
            // check that the buffer encodings all match nicely
            CurrentBufferEncoding = LookupCodedDataBufferOutputEncoding(Buffer,
                                                                        curStreamBuf->AudioParameters,
                                                                        curStreamBuf->FrameParameters,
                                                                        CurrentPage->Size,
                                                                        &CurrentBufferRepetitionPeriod,
                                                                        &Oversampling,
                                                                        &CodedFrameSampleCount,
                                                                        bypassChannel,
                                                                        bypassSD);
            // Update code data input accordingly as far oversampling is concerned.
            CodedInput[CodedIndex].SetBypassOversampling(Oversampling);
            // Update code data input accordingly as far frame sample count is concerned.
            CodedInput[CodedIndex].SetFrameSampleCount(CodedFrameSampleCount);
        }

        // Check if encoding for this page fit encoding
        if ((CodedInput[CodedIndex].GetEncoding() != CurrentBufferEncoding) ||
            (CodedInput[CodedIndex].GetRepetitionPeriod() != CurrentBufferRepetitionPeriod))
        {
            if (PcmPlayer_c::OUTPUT_IEC60958 == CodedInput[CodedIndex].GetEncoding())
            {
                CodedInput[CodedIndex].SetEncoding(CurrentBufferEncoding);
                CodedInput[CodedIndex].SetRepetitionPeriod(CurrentBufferRepetitionPeriod);
            }
            else
            {
                SE_DEBUG(group_manifestor_audio_ksound,  "Detected coded data format change[%d] (%s[%d] -> %s[%d])\n",
                         CodedIndex,
                         PcmPlayer_c::LookupOutputEncoding(CodedInput[CodedIndex].GetEncoding()),
                         CodedInput[CodedIndex].GetRepetitionPeriod(),
                         PcmPlayer_c::LookupOutputEncoding(CurrentBufferEncoding),
                         CurrentBufferRepetitionPeriod);

                if (CodedInput[CodedIndex].GetEncoding() != CurrentBufferEncoding)
                {
                    // we can allow a format change of compaitible types DD <-> DD+ and SPDIFIN_PCM <-> SPDIFIN_COMPRESSED
                    if (CodedInput[CodedIndex].IsCompatibleEncodingChange(CurrentBufferEncoding))
                    {
                        CodedInput[CodedIndex].SetEncoding(CurrentBufferEncoding);
                        CodedInput[CodedIndex].SetRepetitionPeriod(CurrentBufferRepetitionPeriod);
                    }
                    else
                    {
                        // We still don't allow format change for others as of now
                        break;
                    }
                }
                else
                {
                    CodedInput[CodedIndex].SetPeriodChange(true);
                }
            }
        }
        PageCount++;

        if (CurrentPage->Size == 0)
        {
            SE_DEBUG(group_manifestor_audio_ksound,  "Attach an empty transcoded coded data buffer (%d)\n", Status);
        }
        else
        {
            uint32_t SampleCount = (true == CodedInput[CodedIndex].IsBypassRatioVariable())
                                   ? CurrentBufferRepetitionPeriod
                                   : curStreamBuf->AudioParameters->SampleCount * CodedInput[CodedIndex].GetBypassOversampling();

            //SE_DEBUG(group_manifestor_audio_ksound,  "SampleCount: %d\n", SampleCount);
            if (PcmPlayer_c::BYPASS_SPDIFIN_COMPRESSED == CodedInput[CodedIndex].GetEncoding())
            {
                //SpdifIn Compressed buffers are not frame align. Setting PaOffSet will enable to apply pause at correct location
                AUDIOMIXER_SET_PA_OFFSET(CurrentPage, curStreamBuf->AudioParameters->SpdifInProperties.PaOffSetInCompressedBuffer);
            }
            else if (CurrentPage->Size == sizeof(int))
            {
                SE_DEBUG(group_manifestor_audio_ksound,  "%d: Attach a pause-request coded data buffer (%d) of %d samples\n",
                         PageCount, Status, SampleCount);
                AUDIOMIXER_SET_REPETITION_PERIOD(CurrentPage, curStreamBuf->AudioParameters->SampleCount);
            }
            else if (SampleCount != CurrentBufferRepetitionPeriod)
            {
                if (true == CodedInput[CodedIndex].IsBypassPossible())
                {
                    SE_INFO(group_manifestor_audio_ksound,  "%d: Attach a buffer to bypass that do not correspond to the grain %d : replace with a pause burst of %d samples\n",
                            PageCount, CurrentBufferRepetitionPeriod, SampleCount);
                }

                CurrentPage->Size = sizeof(int);
                AUDIOMIXER_SET_REPETITION_PERIOD(CurrentPage, SampleCount);
            }
            else
            {
                AUDIOMIXER_SET_REPETITION_PERIOD(CurrentPage, CurrentBufferRepetitionPeriod);
            }
        }

        if (false == CodedInput[CodedIndex].IsBypassPossible())
        {
            // there is no point whatsoever in queuing up data we can't play...
            break;
        }

        // ensure the buffer remains valid for as long as we need
        CodedFrameBuffer->IncrementReferenceCount(IdentifierManifestor);

        // update the master descriptor (until the point no matter how much we adjust the current page
        // it will have no effect because MME will never read it)
        CurrentCodedMmeDataBuffer->NumberOfScatterPages++;
        CurrentCodedMmeDataBuffer->TotalSize += CurrentPage->Size;
        SE_DEBUG(group_manifestor_audio_ksound,  "Added buffer %p/%p (%d bytes) to the coded data queue [%d]\n",
                 Buffer, CurrentCodedFrameBuffer, CurrentPage->Size, CodedIndex);
        SE_DEBUG(group_manifestor_audio_ksound,  "Coded data queue [%d] has %d scatter pages\n", CodedIndex,
                 CurrentCodedMmeDataBuffer->NumberOfScatterPages);
    }
    return ManifestorNoError;
}

////////////////////////////////////////////////////////////////////////////
///
/// Manifestor_AudioKsound_c::EvaluateCodedDataBufferAdjustement
///
/// This method evaluates if a percussive adjusment needs to be performed
/// on input CodedDataBuffer. This method is responsible to set pDelta,
/// pMuteRequestAsserted and pPauseRequestAsserted values required to
/// perform potential percussive adjustment on Mixer's coded inputs.
///
/// \param in  CodedMmeDataBuffer    MME buffer that holds mixer coded input data
/// \param in  SamplesInPcmBuffer    number of samples available in PCM buffer
/// \param in  CodedIndex            Current Coded Input index
/// \param out pDelta                pointer to fill with Buffer adjustment value in samples
/// \param out pMuteRequestAsserted  pointer to fill with true if mute is necessary for adjustement
/// \param out pPauseRequestAsserted pointer to fill with true if pause is necessary for adjustement
///
/// \return    void
//
void Manifestor_AudioKsound_c::EvaluateCodedDataBufferAdjustement(MME_DataBuffer_t *CodedMmeDataBuffer,
                                                                  int SamplesInPcmBuffer,
                                                                  uint32_t CodedIndex,
                                                                  int  *pDelta,
                                                                  bool *pMuteRequestAsserted,
                                                                  bool *pPauseRequestAsserted)
{
    int Delta = 0;
    bool MuteRequestAsserted = false;
    bool PauseRequestAsserted = false;

    uint32_t RatioCompressedVsPcm = CodedInput[CodedIndex].GetBypassOversampling();

    // Protection against division by 0
    if (!RatioCompressedVsPcm)
    {
        SE_ERROR("RatioCompressedVsPcm is 0 => risk of division by 0 => set it to 1\n");
        RatioCompressedVsPcm = 1;
    }

    if (true == CodedInput[CodedIndex].IsBypassPossible())
    {
        int SamplesInCodedMmeDataBuffer = LookupCodedDataBufferLength(CodedMmeDataBuffer, CodedInput[CodedIndex]);
        Delta = (CodedInput[CodedIndex].IsPeriodChange() == false) ? (SamplesInCodedMmeDataBuffer / RatioCompressedVsPcm) - SamplesInPcmBuffer : 0;

        SE_DEBUG(group_manifestor_audio_ksound,  "@: %d: SamplesInPcmBuffer %d  SamplesInCodedMmeDataBuffer %d  Delta %d\n",
                 CodedIndex, SamplesInPcmBuffer, SamplesInCodedMmeDataBuffer, Delta);
        AdjustCodedDataBufferAndReturnRemaingDelta(CodedMmeDataBuffer,
                                                   &Delta,
                                                   CodedIndex,
                                                   RatioCompressedVsPcm);

        if (Delta)
        {
            int repetition = CodedInput[CodedIndex].GetRepetitionPeriod();
            // force a stop of the compress-bypass so that on next round we can release buffers
            MuteRequestAsserted  = ((Delta >= 0) && (Delta >= repetition)) ? true : false;
            // Need to pause since CompressedData is ahead of PCM data.
            PauseRequestAsserted = (Delta <  0) ? true : false;
        }
    }
    else
    {
        SE_DEBUG(group_manifestor_audio_ksound,  "@: %d: Good LPCM/compressed data delta (%d)\n", CodedIndex, Delta);
    }

    *pDelta = Delta;
    *pMuteRequestAsserted = MuteRequestAsserted;
    *pPauseRequestAsserted = PauseRequestAsserted;
}

////////////////////////////////////////////////////////////////////////////
///
/// Manifestor_AudioKsound_c::PrepareCodedDataBufferMixCommand
///
/// This method fills MME command parameters of CodedFrameParams
///
/// \param in CodedMmeDataBuffer       MME buffer that holds mixer coded input data
/// \param in CodedFrameParams         Mixer Coded input MME parameters
/// \param in PcmFrameParams           Mixer PCM input MME parameters
/// \param in CodedIndex               Current Coded Input index
/// \param in Delta                    Value in samples of adjustement to apply to this coded input
/// \param in PlayerMuted              true if the output AudioPlayer is in mute state.
/// \param in MuteRequestAsserted      true if this coded input has to be muted
/// \param in PauseRequestAsserted     true if this coded input has to be paused
///
/// \return   void
///
void Manifestor_AudioKsound_c::PrepareCodedDataBufferMixCommand(MME_DataBuffer_t  *CodedMmeDataBuffer,
                                                                tMixerFrameParams *CodedFrameParams,
                                                                tMixerFrameParams *PcmFrameParams,
                                                                uint32_t CodedIndex,
                                                                int  Delta,
                                                                bool PlayerMuted,
                                                                bool MuteRequestAsserted,
                                                                bool PauseRequestAsserted)
{
    //
    // Generate the command to perform upon the coded data queue
    //
    // copy the PCM command (which we won't change unless we are performing a corrective action)
    *CodedFrameParams = *PcmFrameParams;

    // Handle the Mute request on a specific audio-player
    if (PlayerMuted == true)
    {
        CodedFrameParams->Command = MIXER_MUTE;
    }

    SE_DEBUG(group_manifestor_audio_ksound, "@: %d: CodedFrameParams: %p->Command: -> %s\n",
             CodedIndex, CodedFrameParams,
             Mixer_Mme_c::LookupMixerCommand((enum eMixerCommand) CodedFrameParams->Command));

    if (true == CodedInput[CodedIndex].IsBypassPossible())
    {
        CodedMmeDataBuffer->Flags = BUFFER_TYPE_CODED_DATA_IO;

        if (true == CodedInput[CodedIndex].IsPlaying())
        {
            if (MuteRequestAsserted /*|| CodedFrameParams->Command == MIXER_PAUSE*/)
            {
                CodedFrameParams->Command = MIXER_STOP;
                // TODO: we could do better than this if we knew the size of the mixer granule (i.e.
                //       StartOffset = MixerGranule - CurrentCodedDataRepetitionPeriod
                CodedFrameParams->StartOffset = 0;
                SE_DEBUG(group_manifestor_audio_ksound,
                         "@: %d: CodedFrameParams: %p->Command: -> %s (cause PCM MuteRequestAsserted)\n",
                         CodedIndex, CodedFrameParams,
                         Mixer_Mme_c::LookupMixerCommand((enum eMixerCommand) CodedFrameParams->Command));
            }
            else if (PauseRequestAsserted)
            {
                CodedFrameParams->Command = MIXER_PLAY;
                CodedFrameParams->StartOffset = -Delta;
                SE_DEBUG(group_manifestor_audio_ksound,
                         "@: %d: CodedFrameParams: %p->Command:%s StartOffset of %d (cause PCM PauseRequestAsserted)\n",
                         CodedIndex, CodedFrameParams,
                         Mixer_Mme_c::LookupMixerCommand((enum eMixerCommand) CodedFrameParams->Command),
                         CodedFrameParams->StartOffset);
            }
            else if (CodedFrameParams->Command == MIXER_FOFI)
            {
                CodedFrameParams->Command = MIXER_PLAY;
                CodedFrameParams->StartOffset = 0;
                SE_DEBUG(group_manifestor_audio_ksound,
                         "@: %d: CodedFrameParams: %p->Command: -> %s (cause PCM is MIXER_FOFI)\n",
                         CodedIndex, CodedFrameParams,
                         Mixer_Mme_c::LookupMixerCommand((enum eMixerCommand) CodedFrameParams->Command));
            }
            else
            {
                // leave everything the same as the LPCM Q
                // Note that if PCM data input is pause, corresponding coded data input is also paused.
            }
        }
        else if (Delta < 0 && CodedFrameParams->Command == MIXER_PLAY)
        {
            // Delta gives us the number of samples to insert to equalize the length of the queues
            CodedFrameParams->StartOffset = -Delta;
        }
    }
    else
    {
        SE_DEBUG(group_manifestor_audio_ksound, "@: %d: No bypass for CodedMmeDataBuffer %p\n",
                 CodedIndex, CodedMmeDataBuffer);
        CodedMmeDataBuffer->Flags = 0;
        // make no noise...
        CodedFrameParams->Command = MIXER_STOP;
        CodedFrameParams->StartOffset = 0;
        SE_DEBUG(group_manifestor_audio_ksound, "@: %d: CodedFrameParams: %p->Command: -> %s (going to flush)\n",
                 CodedIndex, CodedFrameParams,
                 Mixer_Mme_c::LookupMixerCommand((enum eMixerCommand) CodedFrameParams->Command));
        // clear out the buffers (if there are any), once we have stopped nothing else will consume them
        FlushCodedDataBuffer(CodedMmeDataBuffer, CodedInput[CodedIndex]);
        {
            int BypassChannel(CodedInput[CodedIndex].GetBypassChannel());
            CodedInput[CodedIndex].Reset(BypassChannel);
        }
    }
}

////////////////////////////////////////////////////////////////////////////
///
/// Examine the queued LPCM buffers and create a partner coded data queue.
///
/// \todo Account for resampling...
///
ManifestorStatus_t Manifestor_AudioKsound_c::FillOutCodedDataBuffer(MME_DataBuffer_t *PcmBuffer,
                                                                    tMixerFrameParams *PcmFrameParams,
                                                                    MME_DataBuffer_t CodedDataBufferArray[BypassPhysicalChannelCount],
                                                                    tMixerFrameParams CodedFrameParamsArray[BypassPhysicalChannelCount],
                                                                    bool BypassChannelSPDIF,
                                                                    bool BypassChannelHDMI,
                                                                    bool BypassSDChannelSPDIF,
                                                                    bool BypassSDChannelHDMI,
                                                                    PcmPlayer_c::OutputEncoding *OutputEncodingSPDIF,
                                                                    PcmPlayer_c::OutputEncoding *OutputEncodingHDMI)
{
    ManifestorStatus_t Error;
    AudioStreamBuffer_t *curStreamBuf;
    //
    // Locate any new buffers in the PCM queue and append them to the coded queue.
    //
    SE_DEBUG(group_manifestor_audio_ksound,
             ">: PcmBuffer: %p (%d)  CodedDataBufferArray: %p  BypassChannelSPDIF: %s, BypassChannelHDMI: %s BypassSDChannelSPDIF: %s, BypassSDChannelHDMI: %s,"
             "MuteSpdif: %s , MuteHDMI: %s \n",
             PcmBuffer,
             PcmBuffer->NumberOfScatterPages,
             CodedDataBufferArray,
             BypassChannelSPDIF   ? "true" : "false",
             BypassChannelHDMI    ? "true" : "false",
             BypassSDChannelSPDIF ? "true" : "false",
             BypassSDChannelHDMI  ? "true" : "false",
             (OutputEncodingSPDIF && (*OutputEncodingSPDIF == PcmPlayer_c::BYPASS_MUTE)) ? "true" : "false",
             (OutputEncodingHDMI  && (*OutputEncodingHDMI  == PcmPlayer_c::BYPASS_MUTE)) ? "true" : "false");

    // bug25332: do not reset even if coded data are not actually being played.
    //           Will be done in case of flush, or bypass requested to be ended.
    if ((NULL == CodedDataBufferArray) || (false == BypassChannelSPDIF))
    {
        CodedInput[CodedDataInput_c::BYPASS_SPDIF_IDX].Reset(SPDIF);
    }

    if ((NULL == CodedDataBufferArray) || (false == BypassChannelHDMI))
    {
        CodedInput[CodedDataInput_c::BYPASS_HDMI_IDX].Reset(HDMI);
    }

    if (CodedDataBufferArray != NULL)
    {
        if (BypassChannelSPDIF == false && BypassSDChannelSPDIF == false)
        {
            FlushCodedDataBuffer(&CodedDataBufferArray[Mixer_Mme_c::MIXER_CODED_DATA_INPUT_SPDIF_IDX],
                                 CodedInput[CodedDataInput_c::BYPASS_SPDIF_IDX]);
            CodedInput[CodedDataInput_c::BYPASS_SPDIF_IDX].Reset(SPDIF);
        }
        if (BypassChannelHDMI == false && BypassSDChannelHDMI == false)
        {
            FlushCodedDataBuffer(&CodedDataBufferArray[Mixer_Mme_c::MIXER_CODED_DATA_INPUT_HDMI_IDX],
                                 CodedInput[CodedDataInput_c::BYPASS_HDMI_IDX]);
            CodedInput[CodedDataInput_c::BYPASS_HDMI_IDX].Reset(HDMI);
        }
    }

    // Analyze PCM data pages so as to define encoding types according to
    // the requested bypass channels and original format.

    for (unsigned int PcmScatterPageIdx(0); PcmScatterPageIdx < PcmBuffer->NumberOfScatterPages; PcmScatterPageIdx++)
    {
        curStreamBuf = ((AudioStreamBuffer_t **) PcmBuffer->UserData_p)[PcmScatterPageIdx];
        SE_ASSERT(curStreamBuf != NULL);

        if (curStreamBuf->QueueAsCodedData)
        {
            Buffer_c *Buffer = curStreamBuf->Buffer;
            Buffer_c *CodedFrameBuffer(NULL), * TranscodedFrameBuffer(NULL), * CompressedFrameBuffer(NULL);
            // Check if transcoded data exist for all possible bypass channels.
            bool IsTranscodedForSpdif = DoesTranscodedBufferExist(SPDIF, curStreamBuf->AudioParameters, BypassSDChannelSPDIF);
            bool IsTranscodedForHdmi = DoesTranscodedBufferExist(HDMI, curStreamBuf->AudioParameters, BypassSDChannelHDMI);
            bool IsTranscoded(IsTranscodedForSpdif || IsTranscodedForHdmi);

            if (BypassChannelSPDIF || BypassSDChannelSPDIF || BypassSDChannelHDMI)
            {
                TranscodeRequested = true;
            }
            else
            {
                TranscodeRequested = false;
            }

            bool IsCompressedForSpdif = DoesCompressedFrameBufferExist(SPDIF, curStreamBuf->AudioParameters);
            bool IsCompressedForHdmi = DoesCompressedFrameBufferExist(HDMI, curStreamBuf->AudioParameters);
            bool IsCompressed(IsCompressedForSpdif || IsCompressedForHdmi);

            if (CodedDataBufferArray && (CodedDataBufferArray[SPDIF].NumberOfScatterPages >= MIXER_AUDIO_PAGES_PER_BUFFER))
            {
                SE_ERROR("%p No scatter pages remain to describe input - ignoring\n", &CodedDataBufferArray[SPDIF]);
                break;
            }

            if (CodedDataBufferArray && (CodedDataBufferArray[HDMI].NumberOfScatterPages >= MIXER_AUDIO_PAGES_PER_BUFFER))
            {
                SE_ERROR("%p No scatter pages remain to describe input - ignoring\n", &CodedDataBufferArray[HDMI]);
                break;
            }

            curStreamBuf->QueueAsCodedData = false;
            Buffer->ObtainAttachedBufferReference(Stream->GetCodedFrameBufferType(), &CodedFrameBuffer);
            SE_ASSERT(CodedFrameBuffer != NULL);

            if (IsTranscoded)
            {
                SE_DEBUG(group_manifestor_audio_ksound,  "Dealing with transcoded buffer\n");
                CodedFrameBuffer->ObtainAttachedBufferReference(Stream->TranscodedFrameBufferType,
                                                                &TranscodedFrameBuffer);
                if (TranscodedFrameBuffer == NULL)
                {
                    // Transcoded Buffer may have been switched off even if this stream may be transcodable. So. Continue as if no transcoded buffer present
                    SE_DEBUG(group_manifestor_audio_ksound,
                             "No Transcoded frame buffer, will compensate with Pause insertion of %d samples\n",
                             curStreamBuf->AudioParameters->SampleCount);
                    // there could be 2 reasons for absence of transcoded buffer :
                    // 1. transcoding has been turned off (decoding of AudioDescription , decoding for grab only)
                    // 2. codec is not present so transcoding cannot be performed.
                }

                SE_DEBUG(group_manifestor_audio_ksound, "@: CodedFrameBuffer: %p TranscodedFrameBuffer: %p\n", CodedFrameBuffer, TranscodedFrameBuffer);
            }

            if (IsCompressed)
            {
                SE_DEBUG(group_manifestor_audio_ksound,  "Dealing with Compressed frame buffer\n");
                CodedFrameBuffer->ObtainAttachedBufferReference(Stream->CompressedFrameBufferType, &CompressedFrameBuffer);
                if (CompressedFrameBuffer == NULL)
                {
                    // Compressed frame Buffer may have been switched off ?
                    SE_DEBUG(group_manifestor_audio_ksound,
                             "No Compressed frame buffer, will compensate with Pause insertion of %d samples\n",
                             curStreamBuf->AudioParameters->SampleCount);
                }
            }

            // skip the coded data handling
            if (CodedDataBufferArray == NULL)
            {
                continue;
            }

            Error = FillOutCodedMmeBufferArray(CodedDataBufferArray,
                                               CodedFrameBuffer,
                                               curStreamBuf,
                                               TranscodedFrameBuffer,
                                               CompressedFrameBuffer,
                                               BypassChannelSPDIF,
                                               BypassChannelHDMI,
                                               BypassSDChannelSPDIF,
                                               BypassSDChannelHDMI);
            if (ManifestorNoError != Error)
            {
                return Error;
            }
        }
        else
        {
            SE_DEBUG(group_manifestor_audio_ksound,  "@: Stream %p has no encoded buffer\n", curStreamBuf);
            // The current page has no encoded data. Processing will restart with next pages if some
            // or possible FlushCodedDataBuffer() are going to follow.
        }
    }

    if (CodedDataBufferArray == NULL)
    {
        SE_DEBUG(group_manifestor_audio_ksound, "<\n");
        return ManifestorNoError;
    }

    //
    // Is any additional percussive action required upon the compressed buffers?
    //
    int Delta;
    int SamplesInPcmBuffer = BytesToSamples(PcmBuffer->TotalSize);

    for (uint32_t CodedIndex = 0; CodedIndex < CodedDataInput_c::BYPASS_CHANNEL_MAX_NUM; CodedIndex++)
    {
        MME_DataBuffer_t *CurrentCodedMmeDataBuffer;
        tMixerFrameParams *CurrentCodedFrameParams;
        bool PlayerMuted = false;
        bool MuteRequestAsserted = false;
        bool PauseRequestAsserted = false;
        bool DoFill = false;

        if (CodedDataInput_c::BYPASS_SPDIF_IDX == CodedIndex)
        {
            if (true == BypassChannelSPDIF)
            {
                CurrentCodedMmeDataBuffer = &CodedDataBufferArray[Mixer_Mme_c::MIXER_CODED_DATA_INPUT_SPDIF_IDX];
                CurrentCodedFrameParams = &CodedFrameParamsArray[Mixer_Mme_c::MIXER_CODED_DATA_INPUT_SPDIF_IDX];
                PlayerMuted = (*OutputEncodingSPDIF == PcmPlayer_c::BYPASS_MUTE);
                DoFill = true;
            }
        }
        else if (CodedDataInput_c::BYPASS_HDMI_IDX == CodedIndex)
        {
            if (true == BypassChannelHDMI)
            {
                CurrentCodedMmeDataBuffer = &CodedDataBufferArray[Mixer_Mme_c::MIXER_CODED_DATA_INPUT_HDMI_IDX];
                CurrentCodedFrameParams = &CodedFrameParamsArray[Mixer_Mme_c::MIXER_CODED_DATA_INPUT_HDMI_IDX];
                PlayerMuted = (*OutputEncodingHDMI == PcmPlayer_c::BYPASS_MUTE);
                DoFill = true;
            }
        }

        if (DoFill == false)
        {
            // This coded input is out of interest.
            continue;
        }

        EvaluateCodedDataBufferAdjustement(CurrentCodedMmeDataBuffer,
                                           SamplesInPcmBuffer,
                                           CodedIndex,
                                           &Delta,
                                           &MuteRequestAsserted,
                                           &PauseRequestAsserted);

        PrepareCodedDataBufferMixCommand(CurrentCodedMmeDataBuffer,
                                         CurrentCodedFrameParams,
                                         PcmFrameParams,
                                         CodedIndex,
                                         Delta,
                                         PlayerMuted,
                                         MuteRequestAsserted,
                                         PauseRequestAsserted);
    }

    CodedInput[CodedDataInput_c::BYPASS_SPDIF_IDX].DebugDump();
    CodedInput[CodedDataInput_c::BYPASS_HDMI_IDX].DebugDump();
    if (OutputEncodingSPDIF != NULL)
    {
        *OutputEncodingSPDIF = CodedInput[CodedDataInput_c::BYPASS_SPDIF_IDX].GetEncoding();
    }
    if (OutputEncodingHDMI != NULL)
    {
        *OutputEncodingHDMI = CodedInput[CodedDataInput_c::BYPASS_HDMI_IDX].GetEncoding();
    }

    SE_DEBUG(group_manifestor_audio_ksound, "<: OutputEncoding SPDIF:%s HDMI:%s \n",
             PcmPlayer_c::LookupOutputEncoding(CodedInput[CodedDataInput_c::BYPASS_SPDIF_IDX].GetEncoding()),
             PcmPlayer_c::LookupOutputEncoding(CodedInput[CodedDataInput_c::BYPASS_HDMI_IDX].GetEncoding()));

    return ManifestorNoError;
}

////////////////////////////////////////////////////////////////////////////
///
/// Retire any already played buffers.
///
/// Release the references for any buffers that have been completely consumed and
/// update the MME descriptor accordingly.
///
void Manifestor_AudioKsound_c::UpdateCodedDataBuffer(MME_DataBuffer_t *CodedDataBuffer,
                                                     MME_MixerInputStatus_t *CodedInputStatus,
                                                     CodedDataInput_c &ThisCodedInput)
{
    uint32_t CompletedPages;
    int BytesUsed = CodedInputStatus->BytesUsed + CodedDataBuffer->StartOffset;
    SE_DEBUG(group_manifestor_audio_ksound, ">: CodedDataBuffer: %p %s BytesUsed: %d NbInSplNextTransform: %d\n",
             CodedDataBuffer,
             LookupMixerInputState(CodedInputStatus->State),
             CodedInputStatus->BytesUsed,
             CodedInputStatus->NbInSplNextTransform);
    //
    // Work out which buffers can be marked as completed (and do so)
    //

    if (BytesUsed > 0)
    {
        uint32_t i;

        for (i = 0; i < CodedDataBuffer->NumberOfScatterPages; i++)
        {
            MME_ScatterPage_t *CurrentPage = &CodedDataBuffer->ScatterPages_p[i];
            Buffer_c *CodedFrameBuffer = (Buffer_c *)(((unsigned int *)CodedDataBuffer->UserData_p)[i]);
            BytesUsed -= CurrentPage->Size;

            if (BytesUsed >= 0)
            {
                CodedFrameBuffer->DecrementReferenceCount(IdentifierManifestor);

                SE_DEBUG(group_manifestor_audio_ksound,  "@: Removing whole of a %d byte buffer from the queue\n", CurrentPage->Size);
                CodedDataBuffer->TotalSize -= CurrentPage->Size;

                if (BytesUsed == 0)
                {
                    CodedDataBuffer->StartOffset = 0;
                    ThisCodedInput.SetFirstBufferPartiallyConsumed(false);
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
                SE_DEBUG(group_manifestor_audio_ksound,  "@: Removing %d of a %d byte buffer from the queue (new length %d)\n",
                         CurrentPage->Size - BytesRemaining, CurrentPage->Size, BytesRemaining);
                // the coded data input is 'special' - it wants the whole page back because its using the
                // size field to work out how much padding to insert. in exchange for its odd behavior it
                // promises to give sum the BytesUsed itself (i.e. we don't have to remember that we didn't
                // nibble anything out of the buffer).
                //CurrentPage->Page_p = ((unsigned char *) CurrentPage->Page_p) + CurrentPage->Size - BytesRemaining;
                //CurrentPage->Size = BytesRemaining;
                CodedDataBuffer->StartOffset = CurrentPage->Size + BytesUsed; // BytesUsed is -ve
                ThisCodedInput.SetFirstBufferPartiallyConsumed(true);
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

    if (RemainingPages > 0)
    {
        if (FirstRemainingPage > 0)
        {
            memmove(&CodedDataBuffer->ScatterPages_p[0],
                    &CodedDataBuffer->ScatterPages_p[FirstRemainingPage],
                    RemainingPages * sizeof(CodedDataBuffer->ScatterPages_p[0]));
            memmove(&((unsigned int *) CodedDataBuffer->UserData_p)[0],
                    &((unsigned int *) CodedDataBuffer->UserData_p)[FirstRemainingPage],
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

        if (RemainingPages < 0)
        {
            SE_ERROR("Firmware consumed more data than it was supplied with!!!\n");
        }
    }

    //
    // Update the state based on the reply from the firmware
    //
    // In case of PCM data input being paused, corresponding coded data input is also paused, and coded data are considered as playing.
    ThisCodedInput.SetPlaying((CodedInputStatus->State == MIXER_INPUT_RUNNING) || (CodedInputStatus->State == MIXER_INPUT_PAUSED));

    if (true == ThisCodedInput.IsPlaying())
    {
        ThisCodedInput.SetSamplesUntilNextRepetitionPeriod(CodedInputStatus->NbInSplNextTransform);

        if (ThisCodedInput.GetSamplesUntilNextRepetitionPeriod() > ThisCodedInput.GetRepetitionPeriod())
        {
            SE_ERROR("@: Firmware is talking nonsense about remaining repetition period (%d of %d)\n",
                     ThisCodedInput.GetSamplesUntilNextRepetitionPeriod(),
                     ThisCodedInput.GetRepetitionPeriod());
            ThisCodedInput.SetSamplesUntilNextRepetitionPeriod(0); // firmware value cannot be trusted
        }

        // DTS/CDDA is DTS encoded at a fixed bit rate (one that exactly matches CD audio) and
        // therefore, there is a direct translation between coded size and frames(1 frame is 4 bytes).
        // This is fortunate since the firmware doesn't know when the frame will end (due to historic
        // DTS encoder bugs) meaning the value of SamplesUntilNextCodedDataRepetitionPeriod needs to
        // be corrected by the driver.
        if ((ThisCodedInput.GetEncoding() == PcmPlayer_c::BYPASS_DTS_CDDA) &&
            (0 == ThisCodedInput.GetSamplesUntilNextRepetitionPeriod()) &&
            (0 != CodedDataBuffer->StartOffset))
        {
            ThisCodedInput.SetSamplesUntilNextRepetitionPeriod(ThisCodedInput.GetRepetitionPeriod() \
                                                               - (CodedDataBuffer->StartOffset / 4));
        }

        SE_DEBUG(group_manifestor_audio_ksound, "@: Retired %d whole buffers, buffers partially consumed: %s (%d frames remaining)\n",
                 CompletedPages,
                 ThisCodedInput.IsFirstBufferPartiallyConsumed() ? "true" : "false",
                 ThisCodedInput.GetSamplesUntilNextRepetitionPeriod());
    }
    else
    {
        if (false != ThisCodedInput.IsFirstBufferPartiallyConsumed())
        {
            SE_ERROR("Partially consumed buffer remains when %s\n",
                     LookupMixerInputState(CodedInputStatus->State));
            CodedDataBuffer->StartOffset = 0;
            ThisCodedInput.SetFirstBufferPartiallyConsumed(false);
            DequeueCodedDataBuffer(CodedDataBuffer  , 0 , 1, ThisCodedInput);
        }

        ThisCodedInput.SetSamplesUntilNextRepetitionPeriod(0);

        //
        // Determine if the queue is totally empty (empty queue and mixer has nothing pended in current
        // repetition period.
        //
        if ((0 == CodedDataBuffer->NumberOfScatterPages) && (0 == ThisCodedInput.GetSamplesUntilNextRepetitionPeriod()))
        {
            // the queue to longer has a type
            int BypassChannel(ThisCodedInput.GetBypassChannel());
            ThisCodedInput.Reset(BypassChannel);
        }
    }

    SE_DEBUG(group_manifestor_audio_ksound, "<: %p-> Flags:%u StreamNumber: %u TotalSize: %u StartOffset: %u\n",
             CodedDataBuffer,
             CodedDataBuffer->Flags,
             CodedDataBuffer->StreamNumber,
             CodedDataBuffer->TotalSize,
             CodedDataBuffer->StartOffset);
}


////////////////////////////////////////////////////////////////////////////
///
/// Remove Samples from coded data buffer.
/// Shorten the first page upto PaOffset
/// Remove samples till PaOffset from the last page
/// Dequeue the pages between the page with FirstPaOffSet and Last PaOffSet
/// \param CodedMmeDataBuffer          The coded data buffer to dequeue, remove the sample
/// \param FirstPageIdxWithPaOffset    First page with PaOffset
/// \param PaOffsetOfFirstPageToUpdate PaOffset of first page
/// \param LastPageIdxWithPaOffset     Last page with PaOffset
/// \param PaOffsetOfLastPageToUpdate  PaOffset of last page
///
void Manifestor_AudioKsound_c::RemoveSamplesFromCodedDataBuffer(MME_DataBuffer_t *CodedMmeDataBuffer,
                                                                int32_t FirstPageIdxWithPaOffset,
                                                                uint16_t PaOffsetOfFirstPageToUpdate,
                                                                int32_t LastPageIdxWithPaOffset,
                                                                uint16_t PaOffsetOfLastPageToUpdate,
                                                                uint32_t CodedIndex)
{
    if ((FirstPageIdxWithPaOffset < 0) || (SPDIFIN_NO_PA_IN_PAGE == PaOffsetOfFirstPageToUpdate) || (LastPageIdxWithPaOffset <= FirstPageIdxWithPaOffset) ||
        (SPDIFIN_NO_PA_IN_PAGE == PaOffsetOfLastPageToUpdate))
    {
        return;
    }

    SE_ASSERT(CodedMmeDataBuffer->NumberOfScatterPages > (LastPageIdxWithPaOffset - FirstPageIdxWithPaOffset));
    SE_ASSERT(CodedMmeDataBuffer->NumberOfScatterPages > FirstPageIdxWithPaOffset);
    SE_ASSERT(CodedMmeDataBuffer->NumberOfScatterPages > LastPageIdxWithPaOffset);

    //Update the FirstPagewithPa (Size of page to set upto PaOffset of first page)
    if ((SpdifSamplesToBytes(PaOffsetOfFirstPageToUpdate) < CodedMmeDataBuffer->ScatterPages_p[FirstPageIdxWithPaOffset].Size) && (PaOffsetOfFirstPageToUpdate > 0))
    {
        if ((FirstPageIdxWithPaOffset == 0) && (CodedInput[CodedIndex].IsFirstBufferPartiallyConsumed()) && (CodedMmeDataBuffer->StartOffset >= SpdifSamplesToBytes(PaOffsetOfFirstPageToUpdate)))
        {
            FirstPageIdxWithPaOffset--; // Remove this page completely
        }
        else
        {
            CodedMmeDataBuffer->TotalSize -= CodedMmeDataBuffer->ScatterPages_p[FirstPageIdxWithPaOffset].Size  - SpdifSamplesToBytes(PaOffsetOfFirstPageToUpdate);
            CodedMmeDataBuffer->ScatterPages_p[FirstPageIdxWithPaOffset].Size = SpdifSamplesToBytes(PaOffsetOfFirstPageToUpdate);
            AUDIOMIXER_SET_PA_OFFSET(&CodedMmeDataBuffer->ScatterPages_p[FirstPageIdxWithPaOffset], SPDIFIN_NO_PA_IN_PAGE); // Now no Pa in this page.
        }
    }
    else
    {
        FirstPageIdxWithPaOffset--; // Remove this page completely
    }


    //Update the LastPagewithPa (Update the start pointer)
    if (SpdifSamplesToBytes(LastPageIdxWithPaOffset) < CodedMmeDataBuffer->ScatterPages_p[LastPageIdxWithPaOffset].Size)
    {
        CodedMmeDataBuffer->TotalSize -= SpdifSamplesToBytes(PaOffsetOfLastPageToUpdate);
        CodedMmeDataBuffer->ScatterPages_p[LastPageIdxWithPaOffset].Page_p = ((unsigned char *) CodedMmeDataBuffer->ScatterPages_p[LastPageIdxWithPaOffset].Page_p) + SpdifSamplesToBytes(
                                                                                 PaOffsetOfLastPageToUpdate);
        CodedMmeDataBuffer->ScatterPages_p[LastPageIdxWithPaOffset].Size = CodedMmeDataBuffer->ScatterPages_p[LastPageIdxWithPaOffset].Size - SpdifSamplesToBytes(PaOffsetOfLastPageToUpdate);
        AUDIOMIXER_SET_PA_OFFSET(&CodedMmeDataBuffer->ScatterPages_p[LastPageIdxWithPaOffset], 0); // Now this page start with 0 PaOffset
    }
    else
    {
        LastPageIdxWithPaOffset++; // Remove this page completely
    }
    // Remove the pages till buffer with last Pa
    DequeueCodedDataBuffer(CodedMmeDataBuffer, FirstPageIdxWithPaOffset + 1, (LastPageIdxWithPaOffset - FirstPageIdxWithPaOffset - 1), CodedInput[CodedIndex]);

}


void Manifestor_AudioKsound_c::AdjustCodedDataBufferAndReturnRemaingDelta(MME_DataBuffer_t *CodedMmeDataBuffer,
                                                                          int  *pDelta,
                                                                          uint32_t CodedIndex,
                                                                          uint32_t  RatioCompressedVsPcm)
{
    int Delta = *pDelta;

    // Delta is -ve when we are starving (there may be enough samples to complete this frame but this
    // is not guaranteed unless Delta is +ve). Delta would also be -ve when a pause had been previously requested on PCM and Bypass input
    // but because of iec framing constraints, pause was delayed after the completion of the bypass of the frame ...
    // If Delta exceeds the sync threshold then we must take corrective action.
    // The sync threshold must be greater than the longest fixed size pause burst
    // mandated by IEC61937 (MPEG2, low sampling frequency fixes pause bursts at 64 IEC60938 frames) and may
    // optionally include a small fudge factor.
    if (Delta < 0 || Delta > (64 + 16))
    {
        if (PcmPlayer_c::BYPASS_SPDIFIN_COMPRESSED == CodedInput[CodedIndex].GetEncoding())
        {
            int32_t i = 0;
            int32_t StartOffset = 0;
            int32_t FirstPageIdxWithPaOffset = -1;
            uint16_t PaOffsetOfFirstPageToUpdate = SPDIFIN_NO_PA_IN_PAGE;
            int32_t LastPageIdxWithPaOffset = -1;
            uint16_t PaOffsetOfLastPageToUpdate =  SPDIFIN_NO_PA_IN_PAGE;
            while ((Delta > 0) && (i < CodedMmeDataBuffer->NumberOfScatterPages))
            {
                uint16_t PaOffsetOfCurrentPage = AUDIOMIXER_GET_PA_OFFSET(&CodedMmeDataBuffer->ScatterPages_p[i]);

                StartOffset = ((i == 0) && (CodedInput[CodedIndex].IsFirstBufferPartiallyConsumed())) ? CodedMmeDataBuffer->StartOffset : 0;
                Delta -= BytesToSpdifSamples(CodedMmeDataBuffer->ScatterPages_p[i++].Size - StartOffset) / RatioCompressedVsPcm; // Samples in current page
                if (SPDIFIN_NO_PA_IN_PAGE != PaOffsetOfCurrentPage) // We have a PaOffset in CurrentPage
                {
                    if (FirstPageIdxWithPaOffset < 0)
                    {
                        FirstPageIdxWithPaOffset = i - 1;
                        PaOffsetOfFirstPageToUpdate = PaOffsetOfCurrentPage;
                    }
                    else
                    {
                        LastPageIdxWithPaOffset      = i - 1;
                        PaOffsetOfLastPageToUpdate   = PaOffsetOfCurrentPage;
                    }
                }
                // Round the delta to a multiple of 4 samples
                Delta &= 0xFFFFFFFC;
                if (Delta >= 0)
                {
                    SE_DEBUG(group_manifestor_audio_ksound, "Search next page for PaOffset FirstPageWithPaOffset = %d LastPageWithPaOffset = %d Delta= %d\n", FirstPageIdxWithPaOffset, LastPageIdxWithPaOffset, Delta);
                }
                else
                {
                    Delta = 0; // Don't take any action...
                }
            }
            if (LastPageIdxWithPaOffset > 0)
            {
                RemoveSamplesFromCodedDataBuffer(CodedMmeDataBuffer, FirstPageIdxWithPaOffset, PaOffsetOfFirstPageToUpdate, LastPageIdxWithPaOffset, PaOffsetOfLastPageToUpdate, CodedIndex);
            }
        }
        else
        {
            // If Current Page is being played , then start skipping from first page
            uint32_t first_page_to_dequeue = (CodedInput[CodedIndex].GetSamplesUntilNextRepetitionPeriod() == 0) ? 0 : 1;
            uint32_t page_idx              = first_page_to_dequeue;
            uint32_t nb_pages_to_dequeue   = 0;
            int32_t page_repetition;

            SE_DEBUG(group_manifestor_audio_ksound,  "@: %d: Bad LPCM/compressed data delta (%d), taking corrective action\n",
                     CodedIndex, Delta);

            // Keep removing buffers from the compressed data queue until the delta is negative
            // (i.e. until the CD Q is shorter than the LPCM Q).
            // bug 27900 : only remove buffers if Delta is larger than one repetition frame
            while (Delta > 0 && CodedMmeDataBuffer->NumberOfScatterPages > (first_page_to_dequeue + nb_pages_to_dequeue))
            {
                page_repetition = ((AUDIOMIXER_GET_REPETITION_PERIOD(&CodedMmeDataBuffer->ScatterPages_p[page_idx])) / RatioCompressedVsPcm);
                Delta -= page_repetition;
                // bug23870 : AudioFW doesn't support odd repetition period (and actually processes samples by group of 4)
                // so round the delta to a multiple of 4 samples
                Delta &= 0xFFFFFFFC;

                if (Delta >= 0)
                {
                    SE_DEBUG(group_manifestor_audio_ksound, "drop %d compressed samples\n", page_repetition);
                    nb_pages_to_dequeue++;
                    Dropped -= page_repetition;
                }
                else
                {
                    Delta = 0; // Don't take any action...
                }

                page_idx++;
            }

            DequeueCodedDataBuffer(CodedMmeDataBuffer, first_page_to_dequeue, nb_pages_to_dequeue, CodedInput[CodedIndex]);
            // a correctly synchronized start offset will be calculated in PrepareCodedDataBufferMixCommand()
        }
    }
    else
    {
        Delta = 0;
    }
    *pDelta = Delta;
}


////////////////////////////////////////////////////////////////////////////
///
/// Drop the references for any buffer in the coded data queue.
///
ManifestorStatus_t Manifestor_AudioKsound_c::FlushCodedDataBuffer(MME_DataBuffer_t *CodedMmeDataBuffer, CodedDataInput_c ThisCodedInput)
{
    SE_DEBUG(group_manifestor_audio_ksound, ">: %p (%d)\n", CodedMmeDataBuffer, CodedMmeDataBuffer->NumberOfScatterPages);

    // release any references to the old buffers
    for (unsigned int i = 0; i < CodedMmeDataBuffer->NumberOfScatterPages; i++)
    {
        Buffer_c *CodedFrameBuffer = (Buffer_c *)(((unsigned int *)CodedMmeDataBuffer->UserData_p)[i]);
        ((unsigned int *) CodedMmeDataBuffer->UserData_p)[i] = INVALID_BUFFER_ID;
        CodedFrameBuffer->DecrementReferenceCount(IdentifierManifestor);

        SE_DEBUG(group_manifestor_audio_ksound,  "@: Flushed buffer %p from the queue\n", CodedFrameBuffer);
    }

    CodedMmeDataBuffer->Flags = 0;
    CodedMmeDataBuffer->NumberOfScatterPages = 0;

    // zero-length, null pointer, no flags
    memset(&CodedMmeDataBuffer->ScatterPages_p[0], 0, sizeof(CodedMmeDataBuffer->ScatterPages_p[0]));

    CodedMmeDataBuffer->TotalSize = 0;
    CodedMmeDataBuffer->StartOffset = 0;

    ThisCodedInput.SetFirstBufferPartiallyConsumed(false);

    SE_DEBUG(group_manifestor_audio_ksound,  "<:\n");

    return ManifestorNullQueued;
}
////////////////////////////////////////////////////////////////////////////
///
/// Release any buffers queued within the StreamBuffer structure.
/// Done by parent class  Manifestor_Audio_c::ReleaseQueuedDecodeBuffers
///
/// Trigger to release non queued decoded buffer (buffer send to FW)
/// These will be released when Updated of the input buffer called during callback.
///

ManifestorStatus_t   Manifestor_AudioKsound_c::ReleaseQueuedDecodeBuffers()
{
    ReleaseProcessingDecodedBuffer = true;
    return Manifestor_Audio_c::ReleaseQueuedDecodeBuffers();
}

ManifestorStatus_t  Manifestor_AudioKsound_c::GetChannelConfiguration(enum eAccAcMode *AcMode)
{
    Mixer->GetChannelConfiguration(AcMode);
    return ManifestorNoError;
}

bool Manifestor_AudioKsound_c::IsTranscodeNeeded()
{
    SE_DEBUG(group_manifestor_audio_ksound, "TranscodeRequested:%s\n", TranscodeRequested ? "true" : "false");
    return TranscodeRequested;
}

bool Manifestor_AudioKsound_c::IsCompressedFrameNeeded()
{
    SE_DEBUG(group_manifestor_audio_ksound, "CompressedFrameRequested:%s\n", CompressedFrameRequested ? "true" : "false");
    return CompressedFrameRequested;
}

ManifestorStatus_t  Manifestor_AudioKsound_c::GetDRCParams(DRCParams_t *DRC)
{
    Mixer->GetDRCParameters(DRC);
    return ManifestorNoError;
}

