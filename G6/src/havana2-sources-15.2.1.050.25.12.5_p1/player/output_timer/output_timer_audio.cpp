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

#include "output_timer_audio.h"
#include "mixer_mme.h"

#undef TRACE_TAG
#define TRACE_TAG "OutputTimer_Audio_c"

#define THRESHOLD_FOR_AUDIO_MANIFESTOR_INTERVENTION -100000     // 100 ms
#define AUDIO_WA_SAMPlING_FREQUENCY 48000  //Force output to 48kHz


//
OutputTimer_Audio_c::OutputTimer_Audio_c(void)
    : OutputTimer_Base_c()
    , mAudioConfiguration()
    , mSamplesInLastFrame(0)
    , mLastSampleRate(0)
    , mLastAdjustedSpeedAfterFrameDrop()
    , mCountMultiplier(1)
    , mSampleDurationTime()
    , mLastSeenDecodeIndex(0)
    , mLastSeenDisplayIndex(0)
    , mDecodeFrameCount(0)
    , mDisplayFrameCount(0)
{
}

//
OutputTimer_Audio_c::~OutputTimer_Audio_c(void)
{
    Halt();
}

//
OutputTimerStatus_t   OutputTimer_Audio_c::Halt(void)
{
    return OutputTimer_Base_c::Halt();
}

//
OutputTimerStatus_t   OutputTimer_Audio_c::Reset(void)
{
    mLastAdjustedSpeedAfterFrameDrop = 0;  // rational_c
    mLastSampleRate                  = 0;
    mCountMultiplier                 = 1;  // rational_c

    return OutputTimer_Base_c::Reset();
}

// /////////////////////////////////////////////////////////////////////////
//
//  Function to setup the configuration record, after access has been
//  given to player exported data.
//

OutputTimerStatus_t OutputTimer_Audio_c::InitializeConfiguration(void)
{
    // mConfiguration supposed initialized in base class

    mConfiguration.OutputTimerName               = "Audio";
    mConfiguration.StreamType                    = StreamTypeAudio;
    mConfiguration.AudioVideoDataParsedParametersType        = Player->MetaDataParsedAudioParametersType;
    mConfiguration.SizeOfAudioVideoDataParsedParameters      = sizeof(ParsedAudioParameters_t);
    //
    // Decode window controls, we assume that the
    // frame decode time is the maximum frame duration.
    // If it were greater than this we could not decode
    // a stream. We start with zero and increase this.
    // The Early decode porch, is a value representing
    // much we are willing to stretch the decode time
    // requirements to allow us to have some decoding
    // time in hand, this fairly 'wet finger in air'.
    // Both time values are in micro seconds.
    // Finally a value indicating the maximum wait for
    // entry into the decode window, this number is taken,
    // multiplied by the frame decode time, and then scaled
    // by the speed factor to get an actual maximum wait time.
    //
    mAudioConfiguration.MinimumManifestorLatencyInSamples   = MIXER_NUM_PERIODS * SND_PSEUDO_MIXER_MIN_GRAIN; // we really ought to ask the manifestor for this number
    mAudioConfiguration.MinimumManifestorSamplingFrequency  = 32000; // manifestor deploys resampler to assure this
    mConfiguration.FrameDecodeTime               = 0;
    mConfiguration.MinimumManifestorLatency      = LookupMinimumManifestorLatency(32000);
    mConfiguration.EarlyDecodePorch              = 100 * 1000;
    mConfiguration.MaximumDecodeTimesToWait      = 4;
    mConfiguration.ReverseEarlyDecodePorch       = 100 * 1000;  // Same as forward for audio
    //
    // Synchronization controls
    //      A threshold error in micro seconds (needs to be low if we are aiming for 3ms tolerance)
    //      A count of frames to integrate the error over
    //      A count of frames to be ignored to allow any correction to work through
    //
    mConfiguration.SynchronizationErrorThreshold         = 1000;
    mConfiguration.SynchronizationErrorThresholdForQuantizedSync = 1000;         // Unchanged for audio
    mConfiguration.SynchronizationIntegrationCount       = 8;
    //
    // Trick mode controls in general audio supports only a
    // limited range of trick modes, these values
    // specify those limitations
    //
    mConfiguration.ReversePlaySupported    = false;
    mConfiguration.MinimumSpeedSupported   = 1;
    mConfiguration.MaximumSpeedSupported   = Rational_t(200, 100);

    return OutputTimerNoError;
}


////////////////////////////////////////////////////////////////////////////
///
/// Determine the frame duration in microseconds.
///
/// For some sampling freqencies (e.g. those from the 44.1KHz family) the
/// duration supplied will be by up to a micro second smaller than the
/// true value due to rounding.
///
OutputTimerStatus_t   OutputTimer_Audio_c::FrameDuration(void             *ParsedAudioVideoDataParameters,
                                                         unsigned long long    *Duration)
{
    ParsedAudioParameters_t  *ParsedAudioParameters = (ParsedAudioParameters_t *)ParsedAudioVideoDataParameters;

    if (0 == ParsedAudioParameters->Source.SampleRateHz)
    {
        // we haven't got enough information to determine the frame duration
        return OutputTimerUntimedFrame;
    }

    *Duration = ((unsigned long long) ParsedAudioParameters->SampleCount * 1000000ull) /
                (unsigned long long) ParsedAudioParameters->Source.SampleRateHz;

    return OutputTimerNoError;
}


////////////////////////////////////////////////////////////////////////////
///
/// Examine the frame parameters and determine the its display parameters.
///
/// Calculating the expected duration is rather problematic. For 44.1KHz audio
/// the expected duration (an integer number of microseconds) can never be
/// correct.
///
void  OutputTimer_Audio_c::FillOutManifestationTimings(Buffer_t Buffer)
{
    ParsedAudioParameters_t    *ParsedAudioParameters = (ParsedAudioParameters_t *) GetParsedAudioVideoDataParameters(Buffer);
    OutputTiming_t             *OutputTiming = GetOutputTiming(Buffer);

    OutputTimerStatus_t Status  = Stream->GetManifestationCoordinator()->GetSurfaceParameters(&mOutputSurfaceDescriptors, &mOutputSurfaceDescriptorsHighestIndex);
    if (Status != ManifestationCoordinatorNoError)
    {
        SE_ERROR("Stream 0x%p - %s - Failed to obtain the output surface descriptors from the Manifestation Coordinator\n", Stream, mConfiguration.OutputTimerName);
        return;
    }

    OS_LockMutex(&mLock);

    OutputTiming->HighestTimingIndex        = 0;

    for (unsigned int i = 0; i <= mOutputSurfaceDescriptorsHighestIndex; i++)
    {
        OutputSurfaceDescriptor_t *Surface  = mOutputSurfaceDescriptors[i];
        ManifestationTimingState_t *State   = &mManifestationTimingState[i];
        ManifestationOutputTiming_t *Timing = &OutputTiming->ManifestationTimings[i];

        if (State->Initialized && (Surface != NULL))
        {
            Timing->OutputRateAdjustment        = mOutputRateAdjustments[i];
            Timing->SystemClockAdjustment       = mSystemClockAdjustment;
            Timing->SystemPlaybackTime          = OutputTiming->BaseSystemPlaybackTime;
            Timing->ExpectedDurationTime        = INVALID_TIME;
            Timing->ActualSystemPlaybackTime    = INVALID_TIME;
            Timing->DisplayCount[0]             = ParsedAudioParameters->SampleCount;

            //
            // Here we have avsync corrections, if we are starting up just go for it,
            // if not we adjust by a maximum of 10% (sounds OK on pitch correction)
            // unless aggressive adjustment is requested
            //

            if (State->SynchronizationCorrectionUnits != 0)
            {
                unsigned int MaxAdjust = (State->SynchronizationAtStartup ||
                                          (Player->PolicyValue(Playback, Stream, PolicyLivePlayback) == PolicyValueApply)) ?
                                         ParsedAudioParameters->SampleCount :
                                         min((ParsedAudioParameters->SampleCount / 10), State->SynchronizationOneTenthCorrectionUnits);
                unsigned int Adjust    = min(MaxAdjust, State->SynchronizationCorrectionUnits);

                Timing->DisplayCount[0] += State->ExtendSamplesForSynchronization ? Adjust : -Adjust;

                State->SynchronizationCorrectionUnits -= Adjust;
            }

            //
            // Update derived timings, adjusting the decode time (for decode window porch control).
            //
            if (0 == ParsedAudioParameters->Source.SampleRateHz)
            {
                SE_INFO(group_output_timer, "Source.SampleRateHz 0; forcing default\n");
                ParsedAudioParameters->Source.SampleRateHz = AUDIO_WA_SAMPlING_FREQUENCY;
            }

            Rational_t Duration             = Rational_t(1000000, ParsedAudioParameters->Source.SampleRateHz) * Timing->DisplayCount[0];
            Timing->ExpectedDurationTime    = Duration.RoundedLongLongIntegerPart();
            //
            // Mark the timing as valid
            //
            Timing->TimingValid         = true;
            OutputTiming->HighestTimingIndex    = i;
        }
    }

    UpdateStreamParameters(ParsedAudioParameters);

    OS_UnLockMutex(&mLock);
}

void OutputTimer_Audio_c::UpdateStreamParameters(ParsedAudioParameters_t *ParsedAudioParameters)
{
    // Update recorded values that cover the whole stream
    mSamplesInLastFrame          = ParsedAudioParameters->SampleCount;

    if (0 == ParsedAudioParameters->Source.SampleRateHz)
    {
        SE_INFO(group_output_timer, "Source.SampleRateHz 0; forcing default\n");
        ParsedAudioParameters->Source.SampleRateHz = AUDIO_WA_SAMPlING_FREQUENCY;
    }

    if (ParsedAudioParameters->Source.SampleRateHz != mLastSampleRate)
    {
        mLastSampleRate          = ParsedAudioParameters->Source.SampleRateHz;
        mConfiguration.MinimumManifestorLatency = LookupMinimumManifestorLatency(mLastSampleRate);
    }

    // Apply an update to the frame decode time,
    // based on the native decode duration,
    // rather than the adjusted for speed decode duration
    Rational_t Duration = Rational_t(1000000, ParsedAudioParameters->Source.SampleRateHz) * mSamplesInLastFrame ;
    unsigned long long FrameDecodeTime = RoundedLongLongIntegerPart(Duration);

    if (mConfiguration.FrameDecodeTime != FrameDecodeTime)
    {
        SE_INFO(group_output_timer, "Update FrameDecodeTime :: Samples %d: %lld ==> %lld\n", mSamplesInLastFrame , mConfiguration.FrameDecodeTime , FrameDecodeTime);
        mConfiguration.FrameDecodeTime =  FrameDecodeTime;
    }

    // Apply an update to the next expected playback time, used to spot PTS jumps
    if (ValidTime(mNextExpectedPlaybackTime))
    {
        Rational_t          Speed;
        PlayDirection_t     Direction;

        Player->GetPlaybackSpeed(Playback, &Speed, &Direction);

        unsigned long long PlaybackTimeIncrement = Duration.RoundedLongLongIntegerPart();

        mNextExpectedPlaybackTime    = (Direction == PlayBackward) ?
                                       (mNextExpectedPlaybackTime - PlaybackTimeIncrement) :
                                       (mNextExpectedPlaybackTime + PlaybackTimeIncrement);
    }
}

OutputTimerStatus_t   OutputTimer_Audio_c::FillOutFrameTimingRecord(
    unsigned long long   SystemTime,
    void                *ParsedAudioVideoDataParameters,
    OutputTiming_t      *OutputTiming)
{
    OutputTiming->BaseSystemPlaybackTime    = SystemTime;

    return OutputTimerNoError;
}


///////////////////////////////////////////////////////////////////////////
///
/// Function to correct a synchronization error.
///
/// Two salient pices of information are avaialable, the magnitude
/// of the error in the variable "SynchronizationError", plus a boolean
/// flag "SynchronizationAtStartup" informing us whether or not we are
/// in the startup sphase. This latter variable, when set, may encourage
/// us to be more enthusiatic in our correction.
///
OutputTimerStatus_t   OutputTimer_Audio_c::CorrectSynchronizationError(Buffer_t Buffer, unsigned int   TimingIndex)
{
    ManifestationTimingState_t   *State = &mManifestationTimingState[TimingIndex];
    long long             ErrorSign;
    long long             CorrectionUnitSize;
    long long             CorrectionUnits;
    long long             CorrectionUnitLimit;

    ErrorSign = (State->SynchronizationError < 0) ? -1 : 1;

    //
    // We can correct in units of a single sample.
    //
    if (0 == mLastSampleRate)
    {
        SE_INFO(group_output_timer, "mLastSampleRate 0\n");
        return OutputTimerError;
    }

    CorrectionUnitSize  = (1000000 + (mLastSampleRate / 2)) / mLastSampleRate;
    CorrectionUnits = (State->SynchronizationError + (ErrorSign * (CorrectionUnitSize / 2))) / CorrectionUnitSize;
    //
    // Now, if we need to gain samples at a level greater than the trigger delay
    // level (where the manifestor will just wait for a while), we ignore the
    // correction, alternatively we either gain or lose samples.
    //
    OS_LockMutex(&mLock);
    State->SynchronizationCorrectionUnits       = 0;

    if ((CorrectionUnitSize * CorrectionUnits) > THRESHOLD_FOR_AUDIO_MANIFESTOR_INTERVENTION)
    {
        State->ExtendSamplesForSynchronization  = CorrectionUnits < 0;
        State->SynchronizationCorrectionUnits   = State->ExtendSamplesForSynchronization ? -CorrectionUnits : CorrectionUnits;

        //
        // Now we limit the counts when we are not in startup mode
        //

        if (!State->SynchronizationAtStartup)
        {
            CorrectionUnitLimit             = (Player->PolicyValue(Playback, Stream, PolicyLivePlayback) == PolicyValueApply) ?
                                              (2000000 / CorrectionUnitSize) : mSamplesInLastFrame;
            State->SynchronizationCorrectionUnits   = min(CorrectionUnitLimit, State->SynchronizationCorrectionUnits);
        }

        State->SynchronizationOneTenthCorrectionUnits   = (State->SynchronizationCorrectionUnits + 9) / 10;
    }

    OS_UnLockMutex(&mLock);

    return OutputTimerNoError;
}

///////////////////////////////////////////////////////////////////////////
///
/// Estimate the latency imposed by the manifesor.
///
/// For audio the minimum manifestor latency varies wildly (4x) with the mixer's
/// native sampling frequency. This means a fixed value for the
/// MinimumManifestorLatency is likely to either reject frames to frequently
/// or fail to reject obviously broken frames. This method therefore uses the
/// current sampling rate to estimate the latency imposed by the manifestor.
///
/// We are forced to estimate (and will get it wrong if the mixer's max freq
/// logic kicks in) because we get the information before the manifestor
/// does. In other words, we can't get the manifestor to tell us the actual answer.
///
unsigned int OutputTimer_Audio_c::LookupMinimumManifestorLatency(unsigned int SamplingFrequency)
{
    const int no_overflow = 4;
    unsigned int numerator, denominator;

    // assume worst case latencies if SamplingFrequency is unknown
    if (0 == SamplingFrequency)
    {
        SE_INFO(group_output_timer, "Replacing zero s/freq with minimal default (%d)\n",
                mAudioConfiguration.MinimumManifestorSamplingFrequency);
        SamplingFrequency = mAudioConfiguration.MinimumManifestorSamplingFrequency;
    }

    // keep doubling the sampling rate until it is above the minimum
    while (SamplingFrequency < mAudioConfiguration.MinimumManifestorSamplingFrequency)
    {
        SamplingFrequency *= 2;
    }

    numerator =  mAudioConfiguration.MinimumManifestorLatencyInSamples * (1000000 >> no_overflow);
    denominator = SamplingFrequency >> no_overflow;
    return numerator / denominator;
}
