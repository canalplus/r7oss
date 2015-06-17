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

#ifndef H_OUTPUT_TIMER_VIDEO
#define H_OUTPUT_TIMER_VIDEO

#include "output_timer_base.h"

#undef TRACE_TAG
#define TRACE_TAG   "OutputTimer_Video_c"

class OutputTimer_Video_c : public OutputTimer_Base_c
{
public:
    OutputTimer_Video_c(void);
    ~OutputTimer_Video_c(void);

    //
    // Base component class overrides
    //

    OutputTimerStatus_t   Halt(void);
    OutputTimerStatus_t   Reset(void);

    OutputTimerStatus_t   ResetOnStreamSwitch(void);

private:
    bool            mFirstFrame;

    Rational_t      mThreeTwoPullDownInputFrameRate;
    bool            mEnteredThreeTwoPulldown;

    enum
    {
        PULLDOWN_UNDEFINED,
        PULLDOWN_THREE,
        PULLDOWN_TWO,
    } mThreeTwoPulldownSequenceState;

    Rational_t      mLastAdjustedSpeedAfterFrameDrop;

    Rational_t      mPreviousFrameRate;
    bool            mPreviousContentProgressive;

    unsigned long long   mSourceFieldDurationTime;

    void FixThreeTwoPulldownAccumulatedError(ManifestationTimingState_t *State, OutputSurfaceDescriptor_t *Surface);

    OutputTimerStatus_t HandleFrameParameters(
        void *ParsedAudioVideoDataParameters, OutputTiming_t *OutputTiming,
        unsigned long long *SystemTime);

    void FillMasterSurfaceTimingRecord(
        unsigned long long  SystemTime, ParsedVideoParameters_t *ParsedVideoParameters,
        OutputTiming_t *OutputTiming, unsigned int TimingIndex, Rational_t FrameRate);
    void FillMasterSurfaceTimingRecord_IOnly(
        unsigned long long  SystemTime, ParsedVideoParameters_t *ParsedVideoParameters,
        ManifestationOutputTiming_t *Timing, unsigned long long FieldDurationTime);
    void UpdateCountMultiplier(
        Rational_t FrameRate, OutputSurfaceDescriptor_t *Surface, ManifestationTimingState_t *State, bool Progressive);

    void FillSlavedSurfacesTimingRecords(
        OutputSurfaceDescriptor_t *MasterSurface, ManifestationOutputTiming_t *MasterTiming, OutputTiming_t *OutputTiming);

    OutputTimerStatus_t   InitializeConfiguration(void);

    OutputTimerStatus_t   FrameDuration(void                 *ParsedAudioVideoDataParameters,
                                        unsigned long long       *Duration);

    void UpdateStreamParameters(ParsedVideoParameters_t *ParsedVideoParameters);

    void FillOutManifestationTimings(Buffer_t Buffer);

    OutputTimerStatus_t   FillOutFrameTimingRecord(unsigned long long         SystemTime,
                                                   void                 *ParsedAudioVideoDataParameters,
                                                   OutputTiming_t           *OutputTiming);

    OutputTimerStatus_t   CorrectSynchronizationError(Buffer_t Buffer, unsigned int            TimingIndex);
};

#endif
