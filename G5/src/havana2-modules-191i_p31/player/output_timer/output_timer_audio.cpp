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

Source file name : output_timer_audio.cpp
Author :           Nick

Implementation of the mpeg2 video codec class for player 2.


Date        Modification                                    Name
----        ------------                                    --------
26-Apr-07   Created (from output_timer_video.cpp)           Daniel

************************************************************************/

// /////////////////////////////////////////////////////////////////////
//
//	Include any component headers

#include "output_timer_audio.h"

// /////////////////////////////////////////////////////////////////////////
//
// Locally defined constants
//

#undef OUTPUT_TIMER_TAG
#define OUTPUT_TIMER_TAG "Audio output timer"

#define THRESHOLD_FOR_AUDIO_MANIFESTOR_INTERVENTION	-100000		// 100 ms - this a nick wild stab in the dark

// /////////////////////////////////////////////////////////////////////////
//
// Locally defined structures
//

// /////////////////////////////////////////////////////////////////////////
//
// 	Constructor function, fills in the video specific parameter values
//

OutputTimer_Audio_c::OutputTimer_Audio_c( void )
{
    Reset();
}


// /////////////////////////////////////////////////////////////////////////
//
// 	Destructor function, ensures a full halt and reset 
//	are executed for all levels of the class.
//

OutputTimer_Audio_c::~OutputTimer_Audio_c( void )
{
    Halt();
    Reset();
}


// /////////////////////////////////////////////////////////////////////////
//
//      The Halt function, give up access to any registered resources
//

OutputTimerStatus_t   OutputTimer_Audio_c::Halt(	void )
{

    return OutputTimer_Base_c::Halt();
}


// /////////////////////////////////////////////////////////////////////////
//
//      The Reset function release any resources, and reset all variables
//

OutputTimerStatus_t   OutputTimer_Audio_c::Reset(	void )
{

    ExtendSamplesForSynchronization		= false;
    SynchronizationCorrectionUnits		= 0;
    SynchronizationOneTenthCorrectionUnits	= 0;

    LastSeenDecodeIndex				= INVALID_INDEX;
    LastSeenDisplayIndex			= INVALID_INDEX;
    DecodeFrameCount				= 0;
    DisplayFrameCount				= 0;

    return OutputTimer_Base_c::Reset();
}


// /////////////////////////////////////////////////////////////////////////
//
// 	Function to setup the configuration record, after access has been
//	given to player exported data.
//

OutputTimerStatus_t   OutputTimer_Audio_c::InitializeConfiguration(  void )
{
    memset( &Configuration, 0x00, sizeof(OutputTimerConfiguration_t) );

    Configuration.OutputTimerName				= "Audio";
    Configuration.StreamType					= StreamTypeAudio;

    Configuration.AudioVideoDataParsedParametersType		= Player->MetaDataParsedAudioParametersType;
    Configuration.SizeOfAudioVideoDataParsedParameters		= sizeof(ParsedAudioParameters_t);

    Configuration.AudioVideoDataOutputTimingType		= Player->MetaDataAudioOutputTimingType;
    Configuration.SizeOfAudioVideoDataOutputTiming		= sizeof(AudioOutputTiming_t);

    Configuration.AudioVideoDataOutputSurfaceDescriptorPointer	= (void**)(&AudioOutputSurfaceDescriptor);

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

    AudioConfiguration.MinimumManifestorLatencyInSamples        = 2 * 1536; // we really ought to ask the manifestor for this number
    AudioConfiguration.MinimumManifestorSamplingFrequency       = 32000; // manifestor deploys resampler to assure this
    
    Configuration.FrameDecodeTime				= 0;
    Configuration.MinimumManifestorLatency                      = LookupMinimumManifestorLatency(32000);
    Configuration.EarlyDecodePorch				= 100 * 1000;

    Configuration.MaximumDecodeTimesToWait			= 4;

    //
    // Synchronization controls
    //	    A threshhold error in micro seconds (needs to be low if we are aiming for 3ms tolerance)
    //	    A count of frames to integrate the error over
    //	    A count of frames to be ignored to allow any correction to work through
    //

    Configuration.SynchronizationErrorThreshold			= 1000;
    Configuration.SynchronizationErrorThresholdForExternalSync	= 1000;			// Unchanged for audio
    Configuration.SynchronizationIntegrationCount		= 8;
    Configuration.SynchronizationWorkthroughCount		= 64;			// Never need be greater than the number of decode buffers

    //
    // Trick mode controls in general audio supports only a 
    // limited range of trick modes, these values 
    // specify those limitations
    //

    Configuration.ReversePlaySupported				= false;
    Configuration.MinimumSpeedSupported				= 1;			// Rational_t( 75,100);
    Configuration.MaximumSpeedSupported				= 1;			// Rational_t(125,100);

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
OutputTimerStatus_t   OutputTimer_Audio_c::FrameDuration(	void			  *ParsedAudioVideoDataParameters,
								unsigned long long	  *Duration )
{
ParsedAudioParameters_t		 *ParsedAudioParameters	= (ParsedAudioParameters_t *)ParsedAudioVideoDataParameters;

//

    *Duration = ( (unsigned long long) ParsedAudioParameters->SampleCount * 1000000ull ) /
                (unsigned long long) ParsedAudioParameters->Source.SampleRateHz;

//

    return OutputTimerNoError;
}


////////////////////////////////////////////////////////////////////////////
///
/// Examine the frame parameters and determine the its display parameters.
///
/// This (free-running) implementation consistantly sets the requested display
/// time as invalid. The manifestor will ignore it without adjusting anything.
///
/// Calculating the expected duration is rather problematic. For 44.1KHz audio
/// the expected duration (an integer number of microseconds) can never be
/// correct.
/// 
OutputTimerStatus_t   OutputTimer_Audio_c::FillOutFrameTimingRecord(
							unsigned long long	  SystemTime,
							void			 *ParsedAudioVideoDataParameters,
							void			 *AudioVideoDataOutputTiming )
{
ParsedFrameParameters_t		 *ParsedFrameParameters	= PostDecodeParsedFrameParameters;
ParsedAudioParameters_t		 *ParsedAudioParameters	= (ParsedAudioParameters_t *)ParsedAudioVideoDataParameters;
AudioOutputTiming_t		 *AudioOutputTiming	= (AudioOutputTiming_t *)AudioVideoDataOutputTiming;
Rational_t			  Duration;
unsigned long long  		  BlockDecodeTime;
Rational_t			  SampleCount;
Rational_t	 		  SystemTimeError;
unsigned int			  MaxAdjust;
unsigned int			  Adjust;

//

    AudioOutputTiming->SystemPlaybackTime 	= SystemTime;
    AudioOutputTiming->ExpectedDurationTime 	= INVALID_TIME;
    AudioOutputTiming->ActualSystemPlaybackTime = INVALID_TIME;

    //
    // Set the duration of the frame in samples,
    // and record values for use in avsync corrections
    //

    AudioOutputTiming->DisplayCount	= ParsedAudioParameters->SampleCount;
    SamplesInLastFrame			= ParsedAudioParameters->SampleCount;

    //
    // Do we need to re-calculate the sample count multiplier
    //

    if( (AdjustedSpeedAfterFrameDrop != LastAdjustedSpeedAfterFrameDrop) ||
	(ParsedAudioParameters->Source.SampleRateHz != LastSampleRate) )
    {
	//
	// Re-calculate count multiplier
	// NOTE we do not reset the accumulated error, we 
	// leave it accumulating, except when we transit to a mode 
	// where there is no error IE when the multiplier is 1.
	//

	CountMultiplier			= 1 / AdjustedSpeedAfterFrameDrop;

	if( CountMultiplier == 1 )
	    AccumulatedError		= 0;

	SampleDurationTime		= Rational_t(1000000, ParsedAudioParameters->Source.SampleRateHz);

	LastAdjustedSpeedAfterFrameDrop	= AdjustedSpeedAfterFrameDrop;
	LastSampleRate			= ParsedAudioParameters->Source.SampleRateHz;
	CodedFrameRate			= Rational_t((1000000ULL * ParsedAudioParameters->SampleCount), ParsedAudioParameters->Source.SampleRateHz );
	Configuration.MinimumManifestorLatency = LookupMinimumManifestorLatency(LastSampleRate);

#if 0
report( severity_info, "OutputTimer_Audio_c::FillOutFrameTimingRecord - SampleRate = %d, CountMultiplier %d.%06d, CodedFrameRate  %d.%06d\n",
		LastSampleRate,
		CountMultiplier.IntegerPart(), CountMultiplier.RemainderDecimal(),
		CodedFrameRate.IntegerPart(), CodedFrameRate.RemainderDecimal() );
#endif
    }

    //
    // Jitter the output time by the accumulated error
    //

    if( SystemTime != UNSPECIFIED_TIME ) 
    {
	SystemTimeError			= AccumulatedError * SampleDurationTime;
	SystemTime			= SystemTime - SystemTimeError.LongLongIntegerPart();
    }

    //
    // Calculate the sample count
    //

    SampleCount				= (CountMultiplier * AudioOutputTiming->DisplayCount) + AccumulatedError;
    AudioOutputTiming->DisplayCount	= SampleCount.IntegerPart();
    AccumulatedError			= SampleCount.Remainder();

    //
    // Here we have avsync corrections, if we are starting up just go for it,
    // if not we adjust by a maximum of 10% (sounds ok on pitch correction)
    //

    if( SynchronizationCorrectionUnits != 0 )
    {
	MaxAdjust				= SynchronizationAtStartup ?
							SamplesInLastFrame :
							min( (SamplesInLastFrame / 10), SynchronizationOneTenthCorrectionUnits );

	Adjust					= min( MaxAdjust, SynchronizationCorrectionUnits );

	AudioOutputTiming->DisplayCount		+= ExtendSamplesForSynchronization ? Adjust : -Adjust;
	SynchronizationCorrectionUnits		-= Adjust;
    }

//

#if 0
if( SamplesInLastFrame != AudioOutputTiming->DisplayCount )
report( severity_info, "Timing  - ORA = %d.%06d - Samples = %4d - DisplayCount = %4d\n", 
	AudioOutputTiming->OutputRateAdjustment.IntegerPart(), AudioOutputTiming->OutputRateAdjustment.RemainderDecimal(),
	SamplesInLastFrame, AudioOutputTiming->DisplayCount );
#endif

    //
    // Update derived timings, adjusting the decode time (for decode window porch control).
    //

    Duration					= Rational_t( 1000000, ParsedAudioParameters->Source.SampleRateHz ) * AudioOutputTiming->DisplayCount;
    AudioOutputTiming->ExpectedDurationTime	= Duration.RoundedLongLongIntegerPart();

    //
    // Update the count of coded and decoded frames
    //

    if( LastSeenDecodeIndex != ParsedFrameParameters->DecodeFrameIndex )
	DecodeFrameCount++;

    LastSeenDecodeIndex				= ParsedFrameParameters->DecodeFrameIndex;

//

    if( LastSeenDisplayIndex != ParsedFrameParameters->DisplayFrameIndex )
	DisplayFrameCount++;

    LastSeenDisplayIndex			= ParsedFrameParameters->DisplayFrameIndex;

    //
    // Apply an update to the frame decode time, 
    // based on the native decode duration, 
    // rather than the adjusted for speed decode duration
    // NOTE this is now adjusted to take account the fact
    //      that we decode a coded frame, which may contain 
    //	    several decode frames in the case of streaming audio.
    //

    Duration					= Rational_t( 1000000, ParsedAudioParameters->Source.SampleRateHz ) * ParsedAudioParameters->SampleCount;
    BlockDecodeTime				= (DisplayFrameCount > DecodeFrameCount) ? 
							RoundedLongLongIntegerPart( Duration * Rational_t(DisplayFrameCount, DecodeFrameCount) ) : 
							RoundedLongLongIntegerPart( Duration );
    Configuration.FrameDecodeTime		= max( BlockDecodeTime, Configuration.FrameDecodeTime);

    //
    // Apply an update to the next expected playback time, used to spot PTS jumps
    //

    PlaybackTimeIncrement			= Duration.RoundedLongLongIntegerPart();

//

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
OutputTimerStatus_t   OutputTimer_Audio_c::CorrectSynchronizationError(	void )
{
long long	ErrorSign;
long long	CorrectionUnitSize;
long long	CorrectionUnits;

//

    ErrorSign		= (SynchronizationError < 0) ? -1 : 1;

    //
    // We can correct in units of a single sample.
    //

    CorrectionUnitSize	= (1000000 + (LastSampleRate / 2))/ LastSampleRate;

    CorrectionUnits	= (SynchronizationError + (ErrorSign * (CorrectionUnitSize / 2))) / CorrectionUnitSize;

    //
    // Now, if we need to gain samples at a level greater than the trigger delay 
    // level (where the manifestor will just wait for a while), we ignore the 
    // correction, alternatively we either gain or lose samples.
    //

    SynchronizationCorrectionUnits		= 0;

    if( (CorrectionUnitSize * CorrectionUnits) > THRESHOLD_FOR_AUDIO_MANIFESTOR_INTERVENTION )
    {
	ExtendSamplesForSynchronization		= CorrectionUnits < 0;
	SynchronizationCorrectionUnits		= ExtendSamplesForSynchronization ? -CorrectionUnits : CorrectionUnits;

    	//
    	// Now we limit the counts when we are not in startup mode
    	//

	if( !SynchronizationAtStartup )
	    SynchronizationCorrectionUnits	= min( SamplesInLastFrame, SynchronizationCorrectionUnits );

	SynchronizationOneTenthCorrectionUnits	= (SynchronizationCorrectionUnits + 9)/10;
    }

//

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

    // keep doubling the sampling rate until it is above the minimum
    while (SamplingFrequency && SamplingFrequency < AudioConfiguration.MinimumManifestorSamplingFrequency)
	    SamplingFrequency *= 2;

    numerator =  AudioConfiguration.MinimumManifestorLatencyInSamples * (1000000 >> no_overflow);
    denominator = SamplingFrequency >> no_overflow;
    
    return numerator / denominator;
}
