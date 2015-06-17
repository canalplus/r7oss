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

#include "output_timer_video.h"
#include "player_stream.h"

#undef TRACE_TAG
#define TRACE_TAG   "OutputTimer_Video_c"

// An essential parameter for percussive adjustments is the propagation time :
// an adjustment is indeed applied when the corresponding "adjusted buffer" reaches
// the end of the pipeline.
// The "time" to wait is directly linked to the number of decoded buffers already
// decoded.
// The best and worse case of decoded buffer count will be used to clamp the
// waiting time needed for an adjutment to be taken into account
// 16 is the absolute worst case of decoded buffer count : H264 DPB size
// 2 is a reasonable low limit to avoid performing too frequent corrections
#define WORST_CASE_DECODED_BUFFER_COUNT 16
#define BEST_CASE_DECODED_BUFFER_COUNT  2

// Initial error initialized to 1 for FRC so that as soon as we have a portion
// of frame to display, we'll display it
#define OUTPUT_TIMER_RESIDUE_N (0x7FFFFFFF-1)
#define OUTPUT_TIMER_RESIDUE_D 0x7FFFFFFF
#define RESIDUE Rational_t(OUTPUT_TIMER_RESIDUE_N,OUTPUT_TIMER_RESIDUE_D)

// the following value moves the percussive adjustment threshold further from
// the cliffedge limit (1/2 vsync) to remain stable when the presentation error
// get close to this limit when the adjustment granularity is a full vsync
// (case interlaced frames)
#define DISTANCE_FROM_EDGE_IN_PERCENT 10

OutputTimer_Video_c::OutputTimer_Video_c(void)
    : OutputTimer_Base_c()
    , mFirstFrame(true)
    , mThreeTwoPullDownInputFrameRate(0)
    , mEnteredThreeTwoPulldown(false)
    , mThreeTwoPulldownSequenceState(PULLDOWN_UNDEFINED)
    , mLastAdjustedSpeedAfterFrameDrop(0)
    , mPreviousFrameRate(0)
    , mPreviousContentProgressive(false)
    , mSourceFieldDurationTime(0)
{
}

//
OutputTimer_Video_c::~OutputTimer_Video_c(void)
{
    Halt();
}

//
OutputTimerStatus_t   OutputTimer_Video_c::Halt(void)
{
    return OutputTimer_Base_c::Halt();
}

//
OutputTimerStatus_t   OutputTimer_Video_c::Reset(void)
{
    mFirstFrame                      = true;
    mThreeTwoPulldownSequenceState   = PULLDOWN_UNDEFINED;
    mLastAdjustedSpeedAfterFrameDrop  = 0;  // rational
    mPreviousFrameRate                = 0; // rational
    mPreviousContentProgressive       = false;
    mSourceFieldDurationTime          = 0;

    return OutputTimer_Base_c::Reset();
}

OutputTimerStatus_t   OutputTimer_Video_c::ResetOnStreamSwitch(void)
{
    mFirstFrame = true;
    return OutputTimer_Base_c::ResetOnStreamSwitch();
}

// /////////////////////////////////////////////////////////////////////////
//
//      Function to setup the configuration record, after access has been
//      given to player exported data.
//

OutputTimerStatus_t   OutputTimer_Video_c::InitializeConfiguration(void)
{
    // mConfiguration supposed initialized in base class

    mConfiguration.OutputTimerName                               = "Video";
    mConfiguration.StreamType                                    = StreamTypeVideo;
    mConfiguration.AudioVideoDataParsedParametersType            = Player->MetaDataParsedVideoParametersType;
    mConfiguration.SizeOfAudioVideoDataParsedParameters          = sizeof(ParsedVideoParameters_t);
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
    mConfiguration.FrameDecodeTime                   = 0;
    mConfiguration.EarlyDecodePorch                  = 120 * 1000;
    mConfiguration.MinimumManifestorLatency          = 25 * 1000;                    // To allow for double buffered registers
    mConfiguration.MaximumDecodeTimesToWait          = 4;
    mConfiguration.ReverseEarlyDecodePorch           = 4000 * 1000;                  // 4 seconds
    //
    // Synchronization controls
    //      A threshold error in micro seconds (needs to be low if we are aiming for 3ms tolerance)
    //      A count of frames to integrate the error over
    //      A count of frames to be ignored to allow any correction to work through
    //
    mConfiguration.SynchronizationErrorThreshold                 = 1000;
    mConfiguration.SynchronizationErrorThresholdForQuantizedSync = 25000;        // Video cannot manipulate sync by less than a whole field
    // when it is unable to adjust the time mapping
    mConfiguration.SynchronizationIntegrationCount               = 4;
    //
    // Trick mode controls in general video supports a
    // wide range of trick modes, these values specify
    // the limitations
    //
    mConfiguration.ReversePlaySupported                          = true;
    mConfiguration.MinimumSpeedSupported                         = Rational_t(1, FRACTIONAL_MINIMUM_SPEED_SUPPORTED);
    mConfiguration.MaximumSpeedSupported                         = MAXIMUM_SPEED_SUPPORTED;
//
    return OutputTimerNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Function to calculate the frame duration in Normalized time
//      note since this is in normalized time we do not need to adjust for speed
//

OutputTimerStatus_t   OutputTimer_Video_c::FrameDuration(void                      *ParsedAudioVideoDataParameters,
                                                         unsigned long long        *Duration)
{
    ParsedVideoParameters_t  *ParsedVideoParameters = (ParsedVideoParameters_t *)ParsedAudioVideoDataParameters;
    Rational_t                TimePerFrame;
    Rational_t                TotalTime;

    //
    // Calculated as :-
    //          Time per frame 1000000/frame rate
    //          Number of frames = count 0 + count 1 divided by 2 if not a progressive sequence
    //
    if (ParsedVideoParameters->Content.FrameRate == 0)
    {
        SE_INFO(group_output_timer, "FrameRate 0; forcing TimePerFrame 0\n");
        TimePerFrame = 0;
    }
    else
    {
        TimePerFrame        = (1000000 / ParsedVideoParameters->Content.FrameRate);
    }

    TotalTime           = TimePerFrame * (ParsedVideoParameters->DisplayCount[0] + ParsedVideoParameters->DisplayCount[1]);

    if (!ParsedVideoParameters->Content.Progressive)
    {
        TotalTime       = TotalTime / 2;
    }

    *Duration            = TotalTime.RoundedLongLongIntegerPart();
//
    return OutputTimerNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Function to setup the configuration record, after access has been
//      given to player exported data.
//
//      Major pain, the fixed point support is just not accurate enough for
//      the calculations in here, we get a drift of around 1us per second
//      which kills us in the end, it is because of this that we convert
//      to using integers part way through the function (just before we add
//      speed into the equation).
//


OutputTimerStatus_t OutputTimer_Video_c::HandleFrameParameters(
    void *ParsedAudioVideoDataParameters,
    OutputTiming_t *OutputTiming,
    unsigned long long *SystemTime)
{
    ParsedVideoParameters_t          *ParsedVideoParameters = (ParsedVideoParameters_t *)ParsedAudioVideoDataParameters;
    Rational_t                        ThreeTwoJitter;

    //
    // Adjust data for a non-paired field
    //

    if (ParsedVideoParameters->PictureStructure == StructureEmpty)
    {
        SE_ERROR("Empty picture structure\n");
        OutputTiming->BaseSystemPlaybackTime    = *SystemTime;
        return OutputTimerError;
    }

    bool PartialFrame   = ParsedVideoParameters->PictureStructure != StructureFrame;

    if (PartialFrame)
    {
        SE_VERBOSE(group_output_timer, "Partial Frame\n");
        ParsedVideoParameters->Content.Progressive  = false;
        ParsedVideoParameters->InterlacedFrame      = true;
        ParsedVideoParameters->TopFieldFirst        = ParsedVideoParameters->PictureStructure == StructureTopField;
        ParsedVideoParameters->DisplayCount[0]      = 1;    // Whole frame

        if (ParsedVideoParameters->PanScanCount > 0)
        {
            ParsedVideoParameters->PanScanCount             = 1;
            ParsedVideoParameters->PanScan[0].DisplayCount  = 1;
        }
    }
    //
    // Validate the incoming frame rate, if it isn't reasonable
    // then take the output rate as a reasonable stab at it.
    //

    SE_DEBUG(group_output_timer, "DisplayCount[0]=%d DisplayCount[1]=%d InterlacedFrame=%d Progressive=%d FrameRate=%d.%06d\n"
             , ParsedVideoParameters->DisplayCount[0]
             , ParsedVideoParameters->DisplayCount[1]
             , ParsedVideoParameters->InterlacedFrame
             , ParsedVideoParameters->Content.Progressive
             , ParsedVideoParameters->Content.FrameRate.IntegerPart()
             , ParsedVideoParameters->Content.FrameRate.RemainderDecimal()
            );

    if (!inrange(ParsedVideoParameters->Content.FrameRate, 1, 120))
    {
        SE_WARNING("invalid frame rate => defaulting to 24fps\n");

        ParsedVideoParameters->Content.FrameRate = 24;

        if (!ParsedVideoParameters->Content.Progressive)
        {
            ParsedVideoParameters->Content.FrameRate = ParsedVideoParameters->Content.FrameRate / 2;
        }
    }

    //
    // Check for and if appropriate remove 3:2 pulldown.
    // We use a very simple 3:2 detection algorithm,
    // if we ever see a repeated field, for a progressive frame,
    // but in a non-progressive sequence, we assume 3:2 pulldown
    // is in operation.
    // This may need modifying later, but for now we go with it.
    //
    // multi timing change previously only remove if inrange and then :-
    //    ((OutputSurfaceDescriptor->FrameRate < 51) || (Speed != 1))
    //    but now removing in all cases as output surfaces may be many
    //

    if (mThreeTwoPulldownSequenceState == PULLDOWN_UNDEFINED)
    {
        if (
            inrange(ParsedVideoParameters->Content.FrameRate, 29, 31) &&
            !ParsedVideoParameters->Content.Progressive     &&
            !ParsedVideoParameters->InterlacedFrame         &&
            (ParsedVideoParameters->DisplayCount[0] == 2))
        {
            SE_INFO(group_output_timer, "Stepping in 3:2 pulldown mode\n");
            mThreeTwoPulldownSequenceState  = PULLDOWN_THREE;
            mThreeTwoPullDownInputFrameRate = ParsedVideoParameters->Content.FrameRate;
            mEnteredThreeTwoPulldown        = true;
        }
    }
    else
    {
        if (mThreeTwoPulldownSequenceState == PULLDOWN_TWO)
        {
            if (ParsedVideoParameters->DisplayCount[0] == 2 && ParsedVideoParameters->DisplayCount[1] == 1)
            {
                mThreeTwoPulldownSequenceState = PULLDOWN_THREE;
            }
            else if (ParsedVideoParameters->DisplayCount[0] == 1 && ParsedVideoParameters->DisplayCount[1] == 1)
            {
                mThreeTwoPulldownSequenceState = PULLDOWN_UNDEFINED;
                SE_INFO(group_output_timer, "Stepping out of 3:2 pulldown mode\n");
            }
        }
        else if (mThreeTwoPulldownSequenceState == PULLDOWN_THREE)
        {
            if (ParsedVideoParameters->DisplayCount[0] == 1 && ParsedVideoParameters->DisplayCount[1] == 1)
            {
                mThreeTwoPulldownSequenceState = PULLDOWN_TWO;
            }
            else
            {
                SE_WARNING("Inconsistency detected in 3:2 pulldown sequence : DisplayCount[0]=%d DisplayCount[1]=%d\n"
                           , ParsedVideoParameters->DisplayCount[0]
                           , ParsedVideoParameters->DisplayCount[1]
                          );
                mThreeTwoPulldownSequenceState = PULLDOWN_UNDEFINED;
            }
        }
    }

    if (mThreeTwoPulldownSequenceState != PULLDOWN_UNDEFINED)
    {
        ParsedVideoParameters->Content.FrameRate = (ParsedVideoParameters->Content.FrameRate * 4) / 5;  // e.g. 29.970 -> 23.976

        SE_DEBUG(group_output_timer, "3:2 pulldown mode : Adjusted FrameRate=%d.%06d\n"
                 , ParsedVideoParameters->Content.FrameRate.IntegerPart()
                 , ParsedVideoParameters->Content.FrameRate.RemainderDecimal());

        if (mThreeTwoPulldownSequenceState == PULLDOWN_THREE)
        {
            //
            // Remove the extra field, and also adjust the system
            // time to remove the jitter added by 3:2 pulldown
            //
            ParsedVideoParameters->DisplayCount[0] = 1;

            if (ValidTime(*SystemTime))
            {
                ThreeTwoJitter  = 200000 / ParsedVideoParameters->Content.FrameRate;
                *SystemTime     = *SystemTime + ThreeTwoJitter.RoundedLongLongIntegerPart();
            }
            SE_DEBUG(group_output_timer, "Removed the extra field, adjusted System time by %lld : %lld\n"
                     , ThreeTwoJitter.RoundedLongLongIntegerPart()
                     , *SystemTime);

            //
            // Do we also need to remove the 3:2 pulldown affects from the pan scan vectors
            //
            if ((ParsedVideoParameters->PanScanCount == 3) && (ParsedVideoParameters->PanScan[1].DisplayCount == 1))
            {
                memcpy(&ParsedVideoParameters->PanScan[1], &ParsedVideoParameters->PanScan[2], sizeof(PanScan_t));
                ParsedVideoParameters->PanScanCount = 2;
            }
            else if (ParsedVideoParameters->PanScanCount != 0)
            {
                ParsedVideoParameters->PanScan[0].DisplayCount--;
            }
        }
    }

    // Update our synchronization error threshold based on frame rate
    Rational_t Margin = Rational_t(1000000 * DISTANCE_FROM_EDGE_IN_PERCENT, 100);
    Rational_t HalfVsync = Rational_t(1000000, 2);
    Rational_t Tmp = (Margin + HalfVsync) / ParsedVideoParameters->Content.FrameRate;
    mConfiguration.SynchronizationErrorThresholdForQuantizedSync = Tmp.LongLongIntegerPart();

    return OutputTimerNoError;
}

// /////////////////////////////////////////////////////////////////////////
//
//  This function set the timing record in the specific case of I-only decode
//
//  In I-only injection, the GOP structure cannot be known, and the FRC algorithm is not appropriate.
//  The preferred strategy here is to keep the top field and reset the bottom field count.
//  This will lead to an issue in speed accuracy when the I-frame rate will be higher than the display framerate
//  in very high playback speeds and very high I-frame ratio in the GOPs.
//

void   OutputTimer_Video_c::FillMasterSurfaceTimingRecord_IOnly(
    unsigned long long  SystemTime,
    ParsedVideoParameters_t *ParsedVideoParameters,
    ManifestationOutputTiming_t *Timing,
    unsigned long long FieldDurationTime)
{
    Timing->DisplayCount[0]          = ParsedVideoParameters->DisplayCount[0];
    Timing->DisplayCount[1]          = 0;
    Timing->SystemPlaybackTime       = SystemTime;
    Timing->ActualSystemPlaybackTime = INVALID_TIME;
    Timing->Interlaced               = ParsedVideoParameters->InterlacedFrame;
    Timing->TopFieldFirst            = ParsedVideoParameters->TopFieldFirst;
    Timing->ActualSystemPlaybackTime = INVALID_TIME;
    Timing->Interlaced               = ParsedVideoParameters->InterlacedFrame;
    Timing->TopFieldFirst            = ParsedVideoParameters->TopFieldFirst;
    Timing->TimingValid              = true;
    Timing->ExpectedDurationTime     = FieldDurationTime * Timing->DisplayCount[0];

    SE_VERBOSE(group_output_timer, "Stream 0x%p In I-Only, DisplayCount is not updated. DisplayCount[0]=%d DisplayCount[1]=%d FieldDurationTime=%lld ExpectedDurationTime=%lld \n",
               Stream,
               Timing->DisplayCount[0], Timing->DisplayCount[1],
               FieldDurationTime, Timing->ExpectedDurationTime);
}

void   OutputTimer_Video_c::FillMasterSurfaceTimingRecord(
    unsigned long long  SystemTime,
    ParsedVideoParameters_t *ParsedVideoParameters,
    OutputTiming_t *OutputTiming,
    unsigned int TimingIndex,
    Rational_t FrameRate)
{
    SE_VERBOSE(group_avsync, "SystemTime=%lld\n", SystemTime);

    if (TimingIndex > OutputTiming->HighestTimingIndex)
    {
        OutputTiming->HighestTimingIndex  = TimingIndex;
    }

    OutputSurfaceDescriptor_t    *Surface;
    ManifestationTimingState_t   *State;
    Surface     = mOutputSurfaceDescriptors[TimingIndex];
    State       = &mManifestationTimingState[TimingIndex];

    if (Surface->InheritRateAndTypeFromSource)
    {
        Surface->Progressive = (ParsedVideoParameters->Content.Progressive) || (!ParsedVideoParameters->InterlacedFrame);
        Surface->FrameRate   = FrameRate * (Surface->Progressive ? 1 : 2);
        //Framerate of a surface is really vertical re-fresh rate
    }

    //
    // Do we need to re-calculate the field/frame count multiplier
    //

    if ((mAdjustedSpeedAfterFrameDrop != mLastAdjustedSpeedAfterFrameDrop) ||
        (FrameRate != mPreviousFrameRate) ||
        (Surface->FrameRate != State->PreviousDisplayFrameRate) ||
        (ParsedVideoParameters->Content.Progressive != mPreviousContentProgressive))
    {
        UpdateCountMultiplier(FrameRate, Surface, State, ParsedVideoParameters->Content.Progressive);

        SE_DEBUG2(group_output_timer, group_frc,
                  "TimingIndex=%d DisplayFrameRate %d.%06d (was %d.%06d), ContentFrameRate %d.%06d (was %d.%06d) , AdjustedSpeed=%d.%06d (was %d.%06d) CountMultiplier %d.%06d (was %d.%06d) DisplayCount[0]=%d DisplayCount[1]=%d AccumulatedError=%d.%06d workthough=%d\n",
                  TimingIndex,
                  Surface->FrameRate.IntegerPart(), Surface->FrameRate.RemainderDecimal(),
                  State->PreviousDisplayFrameRate.IntegerPart(), State->PreviousDisplayFrameRate.RemainderDecimal(),
                  FrameRate.IntegerPart(), FrameRate.RemainderDecimal(),
                  mPreviousFrameRate.IntegerPart(), mPreviousFrameRate.RemainderDecimal(),
                  mAdjustedSpeedAfterFrameDrop.IntegerPart(), mAdjustedSpeedAfterFrameDrop.RemainderDecimal(),
                  mLastAdjustedSpeedAfterFrameDrop.IntegerPart(), mLastAdjustedSpeedAfterFrameDrop.RemainderDecimal(),
                  State->CountMultiplier.IntegerPart(), State->CountMultiplier.RemainderDecimal(),
                  mLastCountMultiplier.IntegerPart(), mLastCountMultiplier.RemainderDecimal(),
                  ParsedVideoParameters->DisplayCount[0],
                  ParsedVideoParameters->DisplayCount[1],
                  State->AccumulatedError.IntegerPart(),
                  State->AccumulatedError.RemainderDecimal(),
                  State->FrameWorkthroughFinishCount
                 );
    }

    ManifestationOutputTiming_t  *Timing;
    Timing      = &OutputTiming->ManifestationTimings[TimingIndex];

    Timing->ActualSystemPlaybackTime    = INVALID_TIME;
    Timing->Interlaced          = ParsedVideoParameters->InterlacedFrame;
    Timing->TopFieldFirst       = ParsedVideoParameters->TopFieldFirst;

    if (Player->PolicyValue(Playback, Stream, PolicyTrickModeDomain) == PolicyValueTrickModeDecodeKeyFrames
        || mTrickModeDomain == PolicyValueTrickModeDecodeKeyFrames
        || Player->PolicyValue(Playback, Stream, PolicyStreamOnlyKeyFrames) == PolicyValueApply
        || Player->PolicyValue(Playback, Stream, PolicyStreamDiscardFrames) == PolicyValueKeyFramesOnly)
    {
        return FillMasterSurfaceTimingRecord_IOnly(SystemTime, ParsedVideoParameters, Timing, State->FieldDurationTime);
    }

    //
    // Jitter the output time by the accumulated error
    //
    if (ValidTime(SystemTime))
    {
        Rational_t SystemTimeError      = State->AccumulatedError * State->FieldDurationTime;
        Timing->SystemPlaybackTime      = SystemTime - SystemTimeError.LongLongIntegerPart();
        SE_EXTRAVERB(group_output_timer, "Corrected SystemPlaybackTime = %llu SystemTime = %llu Error = %lld\n",
                     Timing->SystemPlaybackTime,
                     SystemTime,
                     SystemTimeError.LongLongIntegerPart());
    }
    else
    {
        Timing->SystemPlaybackTime      = SystemTime;
    }

    //
    // Calculate the field counts
    //
    Rational_t FrameCount;
    FrameCount                  = (State->CountMultiplier * ParsedVideoParameters->DisplayCount[0]) + State->AccumulatedError;
    Timing->DisplayCount[0]     = (FrameCount + RESIDUE).IntegerPart();
    State->AccumulatedError     = FrameCount - Timing->DisplayCount[0];

    if (!ParsedVideoParameters->Content.Progressive)
    {
        FrameCount                  = (State->CountMultiplier * ParsedVideoParameters->DisplayCount[1]) + State->AccumulatedError;
        Timing->DisplayCount[1]     = (FrameCount + RESIDUE).IntegerPart();
        State->AccumulatedError     = FrameCount - Timing->DisplayCount[1];
    }
    else
    {
        Timing->DisplayCount[1]     = 0;
    }

    //
    // Are we in the process of performing an avsync correction
    //
    if ((State->LoseFieldsForSynchronization != 0) && ((Timing->DisplayCount[0] + Timing->DisplayCount[1]) != 0))
    {

        unsigned int Lose_0        = min(State->LoseFieldsForSynchronization, Timing->DisplayCount[0]);
        State->LoseFieldsForSynchronization -= Lose_0;
        Timing->DisplayCount[0]     -= Lose_0;
        unsigned int Lose_1        = min(State->LoseFieldsForSynchronization, Timing->DisplayCount[1]);
        State->LoseFieldsForSynchronization -= Lose_1;
        Timing->DisplayCount[1]     -= Lose_1;

        SE_DEBUG2(group_avsync, group_frc, "Percussive adjustment : dropping (%d,%d) fields. DisplayCount[0]=%d DisplayCount[1]=%d AccumulatedError=%d.%06d, WorkThrough=%d\n",
                  Lose_0,
                  Lose_1,
                  Timing->DisplayCount[0],
                  Timing->DisplayCount[1],
                  State->AccumulatedError.IntegerPart(), State->AccumulatedError.RemainderDecimal(),
                  State->FrameWorkthroughFinishCount);
    }

    //
    // After all of that wonderful calculation, we now
    // override the lot of it if we are single stepping.
    //
    Rational_t                        Speed;
    PlayDirection_t                   Direction;
    Player->GetPlaybackSpeed(Playback, &Speed, &Direction);

    if (Speed == 0)
    {
        Timing->DisplayCount[0]     = 1;

        if (!ParsedVideoParameters->Content.Progressive)
        {
            Timing->DisplayCount[1]   = 1;
        }
    }

    //
    // Fill in the other timing parameters, adjusting the decode time (for decode window porch control).
    //
    Timing->ExpectedDurationTime    = State->FieldDurationTime * (Timing->DisplayCount[0] + Timing->DisplayCount[1]);

    //
    // If we are running in reverse and an interlaced frame, then reverse the fields
    // This also involves reversing the display counts, and the pan scan arrays.
    //

    if (!ParsedVideoParameters->Content.Progressive &&
        Direction == PlayBackward)
    {
        unsigned int    Tmp;
        Timing->TopFieldFirst   = !Timing->TopFieldFirst;
        Tmp         = Timing->DisplayCount[0];
        Timing->DisplayCount[0] = Timing->DisplayCount[1];
        Timing->DisplayCount[1] = Tmp;
    }

    //
    // Finally, in trickmode if we have a zero first field count we move the second field count up,
    // and invert the top field first flag.
    //

    if (Speed > 1)
    {
        if (Timing->DisplayCount[0] == 0)
        {
            Timing->DisplayCount[0]     = Timing->DisplayCount[1];
            Timing->DisplayCount[1]     = 0;
            Timing->TopFieldFirst       = !Timing->TopFieldFirst;
        }
    }

    SE_VERBOSE(group_output_timer, "Stream 0x%p topfirst=%d TimingIndex=%d DisplayCount[0]=%d DisplayCount[1]=%d FieldDurationTime=%lld ExpectedDurationTime=%lld AccumulatedError=%d.%06d\n",
               Stream,
               Timing->TopFieldFirst,
               TimingIndex,
               Timing->DisplayCount[0], Timing->DisplayCount[1],
               State->FieldDurationTime, Timing->ExpectedDurationTime,
               State->AccumulatedError.IntegerPart(), State->AccumulatedError.RemainderDecimal());

    Timing->TimingValid         = true;

    if (TimingIndex > OutputTiming->HighestTimingIndex)
    {
        OutputTiming->HighestTimingIndex  = TimingIndex;
    }

    if (mFirstFrame
        && Timing->DisplayCount[0]
        && Player->PolicyValue(Playback, Stream, PolicyManifestFirstFrameEarly) == PolicyValueApply)
    {
        SE_INFO(group_output_timer, "Stream 0x%p TimingIndex %d Manifesting First Frame Early\n", Stream, TimingIndex);

        // display only the top field as the frame will stay on display
        Timing->DisplayCount[1]     = 0;
        // set expected presentation time to unspecified so that it is displayed asap
        Timing->SystemPlaybackTime  = UNSPECIFIED_TIME;
    }

}

//
// This function re-calculates frame rate conversion parameters
// It also updates the number of frames to wait until an applied correction is effective.
// This is to avoid rebounds in percussive adjustments (case of too short window) and oversized latencies
// case of too long window)
//
//
void   OutputTimer_Video_c::UpdateCountMultiplier(
    Rational_t FrameRate, OutputSurfaceDescriptor_t *Surface,
    ManifestationTimingState_t *State,
    bool Progressive)
{
    // To avoid a division by 0, we ignore the rest of this function if we are updating count multiplier
    // when Speed == 0
    if (mAdjustedSpeedAfterFrameDrop == 0 || FrameRate == 0)
    {
        State->CountMultiplier = 1;
        return;
    }

    // Tmp variable is used here to prevent overflow on rationals
    //  if multiplication and division are done at once
    //
    mLastCountMultiplier = State->CountMultiplier;
    Rational_t Tmp = mAdjustedSpeedAfterFrameDrop * FrameRate;
    State->CountMultiplier = Surface->FrameRate / Tmp;

    if (!Progressive)
    {
        State->CountMultiplier  = State->CountMultiplier / 2;
    }

    Tmp                         = 1000000 / Surface->FrameRate;
    State->FieldDurationTime    = Tmp.LongLongIntegerPart();

    // Workthough counter is updated once for every source frame

    unsigned int AccumulatedFrames    = (FrameRate * PLAYER_LIMITED_EARLY_MANIFESTATION_WINDOW / 1000000).RoundedUpIntegerPart();

    unsigned int DelayedFramesDueToNotification = ((State->PreviousDisplayFrameRate / Surface->FrameRate)).RoundedUpIntegerPart();

    unsigned long long DelayUntilConsumed = ((unsigned long long)(AccumulatedFrames + DelayedFramesDueToNotification + 1 /*margin */) * 1000000 / Surface->FrameRate *
                                             (Surface->Progressive ? 1 : 2)).LongLongIntegerPart();

    // How long until the applied correction is propagated until manifestation ? We need a worst case,
    // but as little as possible to be reactive if a new correction is needed (typically < 1s)
    State->FrameWorkthroughFinishCount      = (DelayUntilConsumed * FrameRate / 1000000).RoundedUpIntegerPart();
    SE_INFO(group_avsync, "Accumulated = %d Delayed = %d DelayUntilConsumed = %lld FinishCount = %d\n", AccumulatedFrames, DelayedFramesDueToNotification, DelayUntilConsumed,
            State->FrameWorkthroughFinishCount);

    if (State->FieldDurationTime)
    {
        if (!inrange(State->FrameWorkthroughFinishCount, BEST_CASE_DECODED_BUFFER_COUNT, WORST_CASE_DECODED_BUFFER_COUNT))
        {
            State->FrameWorkthroughFinishCount = State->FrameWorkthroughFinishCount < BEST_CASE_DECODED_BUFFER_COUNT ?
                                                 BEST_CASE_DECODED_BUFFER_COUNT : WORST_CASE_DECODED_BUFFER_COUNT;
        }
    }
    else
    {
        State->FrameWorkthroughFinishCount = 16;
    }

    SE_VERBOSE(group_output_timer, "Stream 0x%p Workthrough frame count for error management : %d (frame duration : %lld window : %d)\n"
               , Stream
               , State->FrameWorkthroughFinishCount
               , State->FieldDurationTime
               , PLAYER_LIMITED_EARLY_MANIFESTATION_WINDOW
              );

    mConfiguration.SynchronizationIntegrationCount = State->FrameWorkthroughFinishCount;

    State->PreviousDisplayFrameRate = Surface->FrameRate;

    // Count multiplier update requires a fast adjutment
    // Adjustment size will then not be clamped and recovery will be as quick as possible
    // Note : this flag is read from another thread, which might be in the process of doing
    // a percussive adjustment, even though this is unlikely.
    // But in this case the only side effect is that the startup condition will not be met
    // and subsequent adjustment will be smoother but slower.
    State->SynchronizationAtStartup = true;
}

void   OutputTimer_Video_c::FillSlavedSurfacesTimingRecords(
    OutputSurfaceDescriptor_t *MasterSurface,
    ManifestationOutputTiming_t *MasterTiming,
    OutputTiming_t  *OutputTiming)
{
    for (unsigned k = 0; k <= mOutputSurfaceDescriptorsHighestIndex; k++)
    {
        ManifestationOutputTiming_t    *SlavedTiming  =  &OutputTiming->ManifestationTimings[k];
        OutputSurfaceDescriptor_t      *SlavedSurface =  mOutputSurfaceDescriptors[k];
        ManifestationTimingState_t     *SlavedState   =  &mManifestationTimingState[k];

        if (!SlavedState->Initialized || SlavedSurface == NULL
            || SlavedSurface->MasterSurface != MasterSurface) { continue; }

        SlavedTiming->SystemPlaybackTime        = MasterTiming->SystemPlaybackTime;
        SlavedTiming->ExpectedDurationTime      = MasterTiming->ExpectedDurationTime;
        SlavedTiming->ActualSystemPlaybackTime  = MasterTiming->ActualSystemPlaybackTime;
        SlavedTiming->Interlaced        = MasterTiming->Interlaced;
        SlavedTiming->TopFieldFirst             = MasterTiming->TopFieldFirst;
        SlavedTiming->TimingValid               = true;

        if (k > OutputTiming->HighestTimingIndex)
        {
            OutputTiming->HighestTimingIndex = k;
        }
    }
}


OutputTimerStatus_t   OutputTimer_Video_c::FillOutFrameTimingRecord(
    unsigned long long       SystemTime,
    void                *ParsedAudioVideoDataParameters,
    OutputTiming_t          *OutputTiming)
{
    SE_DEBUG(group_output_timer, "\n");

    OutputTimerStatus_t Status = HandleFrameParameters(ParsedAudioVideoDataParameters, OutputTiming, &SystemTime);
    if (Status != OutputTimerNoError) { return OutputTimerNoError; }

    OutputTiming->BaseSystemPlaybackTime    = SystemTime;

    return OutputTimerNoError;
}

void OutputTimer_Video_c::UpdateStreamParameters(
    ParsedVideoParameters_t *ParsedVideoParameters)
{
    mCodedFrameRate                      = ParsedVideoParameters->Content.FrameRate;
    mPreviousFrameRate                  = ParsedVideoParameters->Content.FrameRate;
    mPreviousContentProgressive         = ParsedVideoParameters->Content.Progressive;
    mLastAdjustedSpeedAfterFrameDrop    = mAdjustedSpeedAfterFrameDrop;
    mEnteredThreeTwoPulldown            = false;

    // Apply an update to the frame decode time,
    // based on the native decode duration,
    // rather than the adjusted for speed decode duration
    Rational_t Tmp = 1000000 / ParsedVideoParameters->Content.FrameRate;
    if (!ParsedVideoParameters->Content.Progressive)
    {
        Tmp = Tmp / 2;
    }

    mSourceFieldDurationTime     = Tmp.LongLongIntegerPart();
    unsigned long long NormalSpeedExpectedDurationTime = mSourceFieldDurationTime * (ParsedVideoParameters->DisplayCount[0] + ParsedVideoParameters->DisplayCount[1]);
    mConfiguration.FrameDecodeTime   = max(NormalSpeedExpectedDurationTime, mConfiguration.FrameDecodeTime);

    // Apply an update to the next expected playback time, used to spot PTS jumps
    if (ValidTime(mNextExpectedPlaybackTime))
    {
        Rational_t          Speed;
        PlayDirection_t     Direction;

        Player->GetPlaybackSpeed(Playback, &Speed, &Direction);

        unsigned long long PlaybackTimeIncrement = NormalSpeedExpectedDurationTime;

        mNextExpectedPlaybackTime    = (Direction == PlayBackward) ?
                                       (mNextExpectedPlaybackTime - PlaybackTimeIncrement) :
                                       (mNextExpectedPlaybackTime + PlaybackTimeIncrement);
    }
}

void OutputTimer_Video_c::FixThreeTwoPulldownAccumulatedError(
    ManifestationTimingState_t *State,
    OutputSurfaceDescriptor_t *Surface)
{
    //
    // You are going to love this, because we are now entering 3:2 pulldown
    // the accumulated error value that we have is incorrect, on the previous
    // frame (before entry) we will have added to the error a value of (Fo/Fi - 1)
    // where Fo and Fi are the output and input frame rates respectively. But
    // we should have added (Fo/(4Fi/5) - 1), so now we need to correct the error
    // by adding 1/4(Fo/Fi).
    //
    // Here we fudge, in most cases we know that the error will currently equal
    // the initial value (Fo - Fi)/Fi, in order to make the rational not expand
    // the denominator, we multiply by 4/4 making 4(Fo-Fi)/4Fi when we add Fo/4Fi
    // this means we have a common denominator
    //

    if (!Surface->InheritRateAndTypeFromSource)
    {
        State->AccumulatedError = State->AccumulatedError * Rational_t(4, 4);
        State->AccumulatedError = State->AccumulatedError + (Surface->FrameRate / (4 * mThreeTwoPullDownInputFrameRate));

        SE_VERBOSE(group_output_timer, "Updating AccumulatedError=%d.06%d\n"
                   , State->AccumulatedError.IntegerPart(), State->AccumulatedError.RemainderDecimal());
    }
}

void OutputTimer_Video_c::FillOutManifestationTimings(Buffer_t Buffer)
{
    ParsedVideoParameters_t    *ParsedVideoParameters = (ParsedVideoParameters_t *)GetParsedAudioVideoDataParameters(Buffer);
    OutputTiming_t             *OutputTiming = GetOutputTiming(Buffer);

    OutputTimerStatus_t Status  = Stream->GetManifestationCoordinator()->GetSurfaceParameters(&mOutputSurfaceDescriptors, &mOutputSurfaceDescriptorsHighestIndex);
    if (Status != ManifestationCoordinatorNoError)
    {
        SE_ERROR("Stream 0x%p - %s - Failed to obtain the output surface descriptors from the Manifestation Coordinator\n", Stream, mConfiguration.OutputTimerName);
        return;
    }

    OS_LockMutex(&mLock);

    OutputTiming->HighestTimingIndex        = 0;

    if (mOutputSurfaceDescriptorsHighestIndex == 0)
    {
        SE_VERBOSE(group_output_timer, "Stream 0x%p No display attached !\n", Stream);
    }
    for (unsigned int i = 0; i <= mOutputSurfaceDescriptorsHighestIndex; i++)
    {
        OutputSurfaceDescriptor_t      *Surface = mOutputSurfaceDescriptors[i];
        ManifestationTimingState_t     *State   = &mManifestationTimingState[i];
        ManifestationOutputTiming_t    *Timing  = &OutputTiming->ManifestationTimings[i];

        if (!State->Initialized || Surface == NULL) { continue; }

        if (mEnteredThreeTwoPulldown)
        {
            FixThreeTwoPulldownAccumulatedError(State, Surface);
        }

        Timing->OutputRateAdjustment    = mOutputRateAdjustments[i];
        Timing->SystemClockAdjustment   = mSystemClockAdjustment;

        if (!Surface->IsSlavedSurface)
        {
            FillMasterSurfaceTimingRecord(OutputTiming->BaseSystemPlaybackTime, ParsedVideoParameters, OutputTiming, i, ParsedVideoParameters->Content.FrameRate);
            FillSlavedSurfacesTimingRecords(Surface, Timing, OutputTiming);
        }
    }
    // Timings filled, first frame flag reset
    mFirstFrame = false;

    UpdateStreamParameters(ParsedVideoParameters);

    OS_UnLockMutex(&mLock);
}


// /////////////////////////////////////////////////////////////////////////
//
//      Function to correct a synchronization error.
//
//      Two salient pices of information are avaialable, the magnitude
//      of the error in the variable "SynchronizationError", plus a boolean
//      flag "SynchronizationAtStartup" informing us whether or not we are
//      in the startup sphase. This latter variable, when set, may encourage
//      us to be more enthusiatic in our correction.
//

OutputTimerStatus_t   OutputTimer_Video_c::CorrectSynchronizationError(Buffer_t Buffer, unsigned int TimingIndex)
{
    ManifestationTimingState_t   *State = &mManifestationTimingState[TimingIndex];
    long long             ErrorSign;
    long long             CorrectionUnitSize;
    long long             CorrectionUnits;
    unsigned int          CorrectionFrameLimit;

    ErrorSign = (State->SynchronizationError < 0) ? -1 : 1;

    ParsedVideoParameters_t    *ParsedVideoParameters = (ParsedVideoParameters_t *)GetParsedAudioVideoDataParameters(Buffer);
    if (0 == State->FieldDurationTime)
    {
        SE_INFO(group_output_timer, "FieldDurationTime 0\n");
        return OutputTimerError;
    }

    CorrectionUnitSize  = State->FieldDurationTime;

    CorrectionUnits     = (State->SynchronizationError + (ErrorSign * (CorrectionUnitSize / 2))) / CorrectionUnitSize;
    //
    // Adjust the accumulated error for frame rate conversion by the spare change.
    //
    OS_LockMutex(&mLock);

    long long Adjustment  = State->SynchronizationError - (CorrectionUnits * CorrectionUnitSize);

    State->AccumulatedError -= Rational_t(Adjustment, CorrectionUnitSize);

    OS_UnLockMutex(&mLock);

    //
    // Set any whole units to be incorporated by the frame timing calculations
    // Note we lose negative values because the manifestor should delay
    // until the appropriate time is reached.
    //
    if (CorrectionUnits > 0)
    {

        //
        // When FRC pattern is not doing any polarity inversion (1/CountMultiplier is integer)
        // we will correct in frames to avoid introducing polarity inversion.
        // Otherwise we correct in fields
        //
        State->LoseFieldsForSynchronization = CorrectionUnits;

        if (ParsedVideoParameters->InterlacedFrame && State->CountMultiplier != 0)
        {
            Rational_t InvMultiplier = Rational_t((int)State->CountMultiplier.GetDenominator(), (int)State->CountMultiplier.GetNumerator());
            if (InvMultiplier.RemainderDecimal() == 0)
            {
                State->LoseFieldsForSynchronization = CorrectionUnits * 2;
                SE_INFO2(group_avsync, group_frc, "CorrectionUnits=%lld LoseFieldsForSynchronization=%d\n", CorrectionUnits, State->LoseFieldsForSynchronization);
            }
        }


        // Unless this at startup, we limit the correction to 2 frames
        // unless we are adopting rapid re-synchronization when we limit the change
        // a maximum of 2 seconds worth per attempt.
        //
        if (!State->SynchronizationAtStartup)
        {
            CorrectionFrameLimit        = (Player->PolicyValue(Playback, Stream, PolicyLivePlayback) == PolicyValueApply) ?
                                          (2000000 / CorrectionUnitSize) : 4;
            State->LoseFieldsForSynchronization = min(CorrectionFrameLimit, State->LoseFieldsForSynchronization);
        }

        SE_DEBUG2(group_avsync, group_frc, "Stream 0x%p startup : %d LoseFieldsForSynchronization=%d CorrectionUnitSize=%lld FrameDurationTime=%lld SynchronizationAtStartup=%d\n",
                  Stream, State->SynchronizationAtStartup, State->LoseFieldsForSynchronization, CorrectionUnitSize, State->FieldDurationTime, State->SynchronizationAtStartup);
    }
    else
    {
        State->LoseFieldsForSynchronization   = 0;
    }

    return OutputTimerNoError;
}

