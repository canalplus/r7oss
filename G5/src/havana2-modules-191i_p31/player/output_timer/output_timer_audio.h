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

Source file name : output_timer_audio.h
Author :           Nick

Basic instance of the output timer class module for player 2.


Date        Modification                                    Name
----        ------------                                    --------
26-Apr-07   Created (from output_timer_video.h)             Daniel

************************************************************************/

#ifndef H_OUTPUT_TIMER_AUDIO
#define H_OUTPUT_TIMER_AUDIO

#include "output_timer_base.h"

// ---------------------------------------------------------------------
//
// Local type definitions
//

typedef struct OutputTimerConfiguration_Audio_s
{
    unsigned int		  MinimumManifestorLatencyInSamples;
    unsigned int                  MinimumManifestorSamplingFrequency;
} OutputTimerConfiguration_Audio_t;

// ---------------------------------------------------------------------
//
// Class definition
//

/// Audio specific output timer.
class OutputTimer_Audio_c : public OutputTimer_Base_c
{
protected:

    // Data

    OutputTimerConfiguration_Audio_t	  AudioConfiguration;
    
    AudioOutputSurfaceDescriptor_t	 *AudioOutputSurfaceDescriptor;

    unsigned int			  SamplesInLastFrame;
    unsigned int			  LastSampleRate;

    Rational_t				  LastAdjustedSpeedAfterFrameDrop;

    Rational_t				  CountMultiplier;
    Rational_t				  SampleDurationTime;
    Rational_t				  AccumulatedError;

    unsigned int			  ExtendSamplesForSynchronization;
    unsigned int			  SynchronizationCorrectionUnits;
    unsigned int			  SynchronizationOneTenthCorrectionUnits;

    unsigned int			  LastSeenDecodeIndex;				// Counters used to determing coded frame : decoded frame ratio in audio streaming codecs
    unsigned int			  LastSeenDisplayIndex;
    unsigned int			  DecodeFrameCount;
    unsigned int			  DisplayFrameCount;

public:

    //
    // Constructor/Destructor methods
    //

    OutputTimer_Audio_c(		void );
    ~OutputTimer_Audio_c(		void );

    //
    // Base component class overrides
    //

    OutputTimerStatus_t   Halt(		void );
    OutputTimerStatus_t   Reset(	void );

    //
    // Audio specific functions
    //

protected:

    OutputTimerStatus_t   InitializeConfiguration(	void );

    OutputTimerStatus_t   FrameDuration(		void			 *ParsedAudioVideoDataParameters,
							unsigned long long	 *Duration );

    OutputTimerStatus_t   FillOutFrameTimingRecord(	unsigned long long	  SystemTime,
							void			 *ParsedAudioVideoDataParameters,
							void			 *AudioVideoDataOutputTiming );

    OutputTimerStatus_t   CorrectSynchronizationError(	void );

    //
    // Audio specific functions
    //
    
    unsigned int	  LookupMinimumManifestorLatency(unsigned int SamplingFreqency);
};
#endif

