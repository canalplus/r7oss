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

#ifndef H_OUTPUT_TIMER_AUDIO
#define H_OUTPUT_TIMER_AUDIO

#include "output_timer_base.h"

#undef TRACE_TAG
#define TRACE_TAG "OutputTimer_Audio_c"

typedef struct OutputTimerConfiguration_Audio_s
{
    unsigned int MinimumManifestorLatencyInSamples;
    unsigned int MinimumManifestorSamplingFrequency;
} OutputTimerConfiguration_Audio_t;

/// Audio specific output timer.
class OutputTimer_Audio_c : public OutputTimer_Base_c
{
public:
    OutputTimer_Audio_c(void);
    ~OutputTimer_Audio_c(void);

    //
    // Base component class overrides
    //

    OutputTimerStatus_t   Halt(void);
    OutputTimerStatus_t   Reset(void);

    //
    // Audio specific functions
    //

protected:
    OutputTimerConfiguration_Audio_t  mAudioConfiguration;

    unsigned int              mSamplesInLastFrame;
    unsigned int              mLastSampleRate;

    Rational_t                mLastAdjustedSpeedAfterFrameDrop;

    Rational_t                mCountMultiplier;
    Rational_t                mSampleDurationTime;

    unsigned int              mLastSeenDecodeIndex;              // Counters used to determing coded frame : decoded frame ratio in audio streaming codecs
    unsigned int              mLastSeenDisplayIndex;
    unsigned int              mDecodeFrameCount;
    unsigned int              mDisplayFrameCount;


    OutputTimerStatus_t   InitializeConfiguration(void);

    OutputTimerStatus_t   FrameDuration(void                 *ParsedAudioVideoDataParameters,
                                        unsigned long long   *Duration);

    void   FillOutManifestationTimings(Buffer_t Buffer);

    void UpdateStreamParameters(ParsedAudioParameters_t *ParsedAudioParameters);

    OutputTimerStatus_t   FillOutFrameTimingRecord(unsigned long long    SystemTime,
                                                   void                 *ParsedAudioVideoDataParameters,
                                                   OutputTiming_t       *OutputTiming);

    OutputTimerStatus_t   CorrectSynchronizationError(Buffer_t Buffer, unsigned int       TimingIndex);

    //
    // Audio specific functions
    //

    unsigned int      LookupMinimumManifestorLatency(unsigned int SamplingFreqency);
};

#endif
