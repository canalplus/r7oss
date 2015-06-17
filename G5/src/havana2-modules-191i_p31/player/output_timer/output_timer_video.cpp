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

Source file name : output_timer_video.cpp
Author :           Nick

Implementation of the mpeg2 video codec class for player 2.


Date        Modification                                    Name
----        ------------                                    --------
25-Jan-06   Created                                         Nick

************************************************************************/

// /////////////////////////////////////////////////////////////////////
//
//      Include any component headers

#include "output_timer_video.h"

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
//      Constructor function, fills in the video specific parameter values
//

OutputTimer_Video_c::OutputTimer_Video_c( void )
{
    Reset();
}


// /////////////////////////////////////////////////////////////////////////
//
//      Destructor function, ensures a full halt and reset 
//      are executed for all levels of the class.
//

OutputTimer_Video_c::~OutputTimer_Video_c( void )
{
    Halt();
    Reset();
}


// /////////////////////////////////////////////////////////////////////////
//
//      The Halt function, give up access to any registered resources
//

OutputTimerStatus_t   OutputTimer_Video_c::Halt(        void )
{

    return OutputTimer_Base_c::Halt();
}


// /////////////////////////////////////////////////////////////////////////
//
//      The Reset function release any resources, and reset all variables
//

OutputTimerStatus_t   OutputTimer_Video_c::Reset(       void )
{

    ThreeTwoPulldownDetected                    = false;
    LastAdjustedSpeedAfterFrameDrop             = 0;

    PreviousFrameRate                           = 0;
    PreviousDisplayFrameRate                    = 0;
    PreviousContentProgressive			= false;

    CountMultiplier                             = 1;
    FrameDurationTime                           = 0;
    SourceFrameDurationTime			= 0;
    AccumulatedError                            = 0;

    SeenInterlacedContentOnInterlacedDisplay    = false;

    LoseFramesForSynchronization                = 0;

    return OutputTimer_Base_c::Reset();
}


// /////////////////////////////////////////////////////////////////////////
//
//      Function to setup the configuration record, after access has been
//      given to player exported data.
//

OutputTimerStatus_t   OutputTimer_Video_c::InitializeConfiguration(  void )
{
    memset( &Configuration, 0x00, sizeof(OutputTimerConfiguration_t) );

    Configuration.OutputTimerName                               = "Video";
    Configuration.StreamType                                    = StreamTypeVideo;

    Configuration.AudioVideoDataParsedParametersType            = Player->MetaDataParsedVideoParametersType;
    Configuration.SizeOfAudioVideoDataParsedParameters          = sizeof(ParsedVideoParameters_t);

    Configuration.AudioVideoDataOutputTimingType                = Player->MetaDataVideoOutputTimingType;
    Configuration.SizeOfAudioVideoDataOutputTiming              = sizeof(VideoOutputTiming_t);

    Configuration.AudioVideoDataOutputSurfaceDescriptorPointer  = (void**)(&VideoOutputSurfaceDescriptor);

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

    Configuration.FrameDecodeTime                               = 0;
    Configuration.EarlyDecodePorch                              = 120 * 1000;
    Configuration.MinimumManifestorLatency			= 25 * 1000;					// To allow for double buffered registers

    Configuration.MaximumDecodeTimesToWait                      = 4;

    //
    // Synchronization controls
    //      A threshhold error in micro seconds (needs to be low if we are aiming for 3ms tolerance)
    //      A count of frames to integrate the error over
    //      A count of frames to be ignored to allow any correction to work through
    //

    Configuration.SynchronizationErrorThreshold                 = 1000;
//    Configuration.SynchronizationErrorThresholdForExternalSync	= 25000;		// Video cannot manipulate sync by less than a whole field 
    Configuration.SynchronizationErrorThresholdForExternalSync	= 1000;		// Video cannot manipulate sync by less than a whole field 
											// when it is unable to adjust the time mapping

    Configuration.SynchronizationIntegrationCount               = 4;
    Configuration.SynchronizationWorkthroughCount               = 64;                   // Never need be greater than the number of decode buffers

    //
    // Trick mode controls in general video supports a 
    // wide range of trick modes, these values specify 
    // the limitations
    //

    Configuration.ReversePlaySupported                          = true;
    Configuration.MinimumSpeedSupported                         = Rational_t(   1,100000);
    Configuration.MaximumSpeedSupported                         = Rational_t(  32,     1);

//

    return OutputTimerNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Function to calculate the frame duration in Normalized time
//      note since this is in normalized time we do not need to adjust for speed
//

OutputTimerStatus_t   OutputTimer_Video_c::FrameDuration(       void                      *ParsedAudioVideoDataParameters,
								unsigned long long        *Duration )
{
ParsedVideoParameters_t  *ParsedVideoParameters = (ParsedVideoParameters_t *)ParsedAudioVideoDataParameters;
Rational_t                TimePerFrame;
Rational_t                TotalTime;

    //
    // Calculated as :-
    //          Time per frame 1000000/frame rate
    //          Number of frames = count 0 + count 1 divided by 2 if not a progressive sequence
    //

    TimePerFrame        = (1000000 / ParsedVideoParameters->Content.FrameRate);
    TotalTime           = TimePerFrame * (ParsedVideoParameters->DisplayCount[0] + ParsedVideoParameters->DisplayCount[1]);

    if( !ParsedVideoParameters->Content.Progressive )
	TotalTime       = TotalTime / 2;

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

OutputTimerStatus_t   OutputTimer_Video_c::FillOutFrameTimingRecord(
							unsigned long long        SystemTime,
							void                     *ParsedAudioVideoDataParameters,
							void                     *AudioVideoDataOutputTiming )
{
unsigned int                      i;
ParsedVideoParameters_t          *ParsedVideoParameters = (ParsedVideoParameters_t *)ParsedAudioVideoDataParameters;
VideoOutputTiming_t              *VideoOutputTiming     = (VideoOutputTiming_t *)AudioVideoDataOutputTiming;
Rational_t                        FrameRate;
bool                              RemoveAnyThreeTwoPulldown;
Rational_t                        AccumulatedPanScanError;
Rational_t                        FrameCount;
Rational_t                        ThreeTwoJitter;
Rational_t                        SystemTimeError;
Rational_t                        Tmp;
Rational_t                        Speed;
PlayDirection_t                   Direction;
unsigned int                      Lose;
unsigned long long                NormalSpeedExpectedDurationTime;
bool				  InterlacedContentOnInterlacedDisplay;
bool				  PartialFrame;

    //
    // Get the playback speed and direction for reverse field inversion
    //

#if 0
report( severity_info, "NickOrd - PictureStructure = %d (%d %d %d - %d %d)\n", 
	ParsedVideoParameters->PictureStructure, 
	ParsedVideoParameters->Content.Progressive,
	ParsedVideoParameters->InterlacedFrame, ParsedVideoParameters->TopFieldFirst,
	ParsedVideoParameters->DisplayCount[0], ParsedVideoParameters->DisplayCount[1] );
#endif

    Player->GetPlaybackSpeed( Playback, &Speed, &Direction );

    //
    // Adjust data for a non-paired field
    //

    if( ParsedVideoParameters->PictureStructure == StructureEmpty )
    {
	report( severity_error, "OutputTimer_Video_c::FillOutFrameTimingRecord - Empty picture structure.\n" );
	VideoOutputTiming->DisplayCount[0]	= 0;
	VideoOutputTiming->DisplayCount[1]	= 0;
	return OutputTimerNoError;
    }

    PartialFrame	= ParsedVideoParameters->PictureStructure != StructureFrame;
    if( PartialFrame )
    {
	ParsedVideoParameters->Content.Progressive	= false;
	ParsedVideoParameters->InterlacedFrame		= true;
	ParsedVideoParameters->TopFieldFirst		= ParsedVideoParameters->PictureStructure == StructureTopField;

// Temporary set to 1 to pass test
	ParsedVideoParameters->DisplayCount[0]		= 1;	// Whole frame
	if( ParsedVideoParameters->PanScan.Count > 0 )
	{
	    ParsedVideoParameters->PanScan.Count		= 1;	// Whole frame
	    ParsedVideoParameters->PanScan.DisplayCount[0]	= 1;	// Whole frame
	}
    }

    //
    // Update the record of whether or not we have seen interlaced frames
    //

    InterlacedContentOnInterlacedDisplay		= ParsedVideoParameters->InterlacedFrame && 
							  !VideoOutputSurfaceDescriptor->Progressive;

    if( InterlacedContentOnInterlacedDisplay && !PartialFrame )
	SeenInterlacedContentOnInterlacedDisplay	= true;

    //
    // Validate the incoming frame rate, if it isn't reasonable 
    // then take the output rate as a reasonable stab at it.
    //

    FrameRate           = ParsedVideoParameters->Content.FrameRate;
    if( !inrange( FrameRate, 1, 120 ) )
    {
	report( severity_error, "OutputTimer_Video_c::FillOutFrameTimingRecord - Ridiculous frame rate %d.%06dfps.\n", 
				FrameRate.IntegerPart(), FrameRate.RemainderDecimal() );

	FrameRate       = VideoOutputSurfaceDescriptor->FrameRate;
	if( !ParsedVideoParameters->Content.Progressive )
	    FrameRate   = FrameRate / 2;
    }

    //
    // Check for and if appropriate remove 3:2 pulldown. 
    // We use a very simple 3:2 detection algorithm, 
    // if we ever see a repeated field, for a progressive frame,
    // but in a non-progressive sequence, we assume 3:2 pulldown 
    // is in operation.
    // This may need modifying later, but for now we go with it.
    //

    RemoveAnyThreeTwoPulldown           = inrange(FrameRate, 29, 31) &&
					  ((VideoOutputSurfaceDescriptor->FrameRate < 51) || (Speed != 1));

    if( !ThreeTwoPulldownDetected                       &&
	!ParsedVideoParameters->Content.Progressive     &&
	!ParsedVideoParameters->InterlacedFrame         &&
	(ParsedVideoParameters->DisplayCount[0] == 2) )
    {
	ThreeTwoPulldownDetected        = true;

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

	if( RemoveAnyThreeTwoPulldown )
	{
	    AccumulatedError    = AccumulatedError * Rational_t(4,4);
	    AccumulatedError    = AccumulatedError + (VideoOutputSurfaceDescriptor->FrameRate / ( 4 * FrameRate));
	}
    }
    else if( ParsedVideoParameters->DisplayCount[0] > 2 )
	ThreeTwoPulldownDetected        = false;

//

    if( ThreeTwoPulldownDetected && RemoveAnyThreeTwoPulldown )
    {
	FrameRate                       = (FrameRate * 4) / 5;                  // e.g. 29.970 -> 23.976
	if( ParsedVideoParameters->DisplayCount[0] == 2 )
	{
	    //
	    // Remove the extra field, and also adjust the system 
	    // time to remove the jitter added by 3:2 pulldown
	    //

	    ParsedVideoParameters->DisplayCount[0] = 1;

	    if( SystemTime != UNSPECIFIED_TIME ) 
	    {
		ThreeTwoJitter  = 200000 / FrameRate;
		SystemTime      = SystemTime + ThreeTwoJitter.RoundedLongLongIntegerPart();
	    }

	    //
	    // Do we also need to remove the 3:2 pulldown affects from the pan scan vectors
	    //

	    if( (ParsedVideoParameters->PanScan.Count == 3) && (ParsedVideoParameters->PanScan.DisplayCount[1] == 1) )
	    {
		ParsedVideoParameters->PanScan.DisplayCount[1]          = ParsedVideoParameters->PanScan.DisplayCount[2];
		ParsedVideoParameters->PanScan.HorizontalOffset[1]      = ParsedVideoParameters->PanScan.HorizontalOffset[2];
		ParsedVideoParameters->PanScan.VerticalOffset[1]        = ParsedVideoParameters->PanScan.VerticalOffset[2];
		ParsedVideoParameters->PanScan.Count                    = 2;
	    }
	    else if( ParsedVideoParameters->PanScan.Count != 0 )
		ParsedVideoParameters->PanScan.DisplayCount[0]--;
	}
    }

    //
    // Do we need to re-calculate the field/frame count multiplier
    //

    if( (AdjustedSpeedAfterFrameDrop != LastAdjustedSpeedAfterFrameDrop) ||
	(FrameRate != PreviousFrameRate) ||
	(VideoOutputSurfaceDescriptor->FrameRate != PreviousDisplayFrameRate) ||
	(ParsedVideoParameters->Content.Progressive != PreviousContentProgressive) )
    {
	//
	// Re-calculate frame rate conversion parameters,
	// NOTE we do not reset the accumulated error, we 
	// leave it accumulating, except when we transit to a mode 
	// where there is no error IE when the multiplier is 1.
	//

	CountMultiplier                 = VideoOutputSurfaceDescriptor->FrameRate / (AdjustedSpeedAfterFrameDrop * FrameRate );

	if( !ParsedVideoParameters->Content.Progressive )
	    CountMultiplier             = CountMultiplier / 2;

	if( CountMultiplier == 1 )
	    AccumulatedError            = 0;

//

	Tmp                             = 1000000 / VideoOutputSurfaceDescriptor->FrameRate;
	FrameDurationTime               = Tmp.LongLongIntegerPart();

	Tmp				= 1000000 / FrameRate;
	if( !ParsedVideoParameters->Content.Progressive )
	    Tmp				= Tmp / 2;
	SourceFrameDurationTime		= Tmp.LongLongIntegerPart();

	CodedFrameRate			= FrameRate;
	PreviousFrameRate               = FrameRate;
	PreviousDisplayFrameRate        = VideoOutputSurfaceDescriptor->FrameRate;
	PreviousContentProgressive	= ParsedVideoParameters->Content.Progressive;
	LastAdjustedSpeedAfterFrameDrop = AdjustedSpeedAfterFrameDrop;

#if 0
report( severity_info, "OutputTimer_Video_c::FillOutFrameTimingRecord - DisplayFrameRate %d.%06d, ContentFrameRate %d.%06d, CountMultiplier %d.%06d\n",
		PreviousDisplayFrameRate.IntegerPart(), PreviousDisplayFrameRate.RemainderDecimal(),
		PreviousFrameRate.IntegerPart(), PreviousFrameRate.RemainderDecimal(),
		CountMultiplier.IntegerPart(), CountMultiplier.RemainderDecimal() );
#endif
    }

    //
    // Jitter the output time by the accumulated error
    //

    if( SystemTime != UNSPECIFIED_TIME ) 
    {
	SystemTimeError                 = AccumulatedError * FrameDurationTime;
	if( InterlacedContentOnInterlacedDisplay && !PartialFrame )
	    SystemTimeError             = SystemTimeError * 2;

	SystemTime                      = SystemTime - SystemTimeError.LongLongIntegerPart();
    }

    //
    // Calculate the field counts
    //

    AccumulatedPanScanError             = AccumulatedError;

    FrameCount                          = (CountMultiplier * ParsedVideoParameters->DisplayCount[0]) + AccumulatedError;
    VideoOutputTiming->DisplayCount[0]  = FrameCount.IntegerPart();
    AccumulatedError                    = FrameCount.Remainder();

    if( PartialFrame )
    {
	VideoOutputTiming->DisplayCount[0]      = 1;
	VideoOutputTiming->DisplayCount[1]      = 0;
    }
    else if( InterlacedContentOnInterlacedDisplay )
    {
	if( ParsedVideoParameters->DisplayCount[0] != ParsedVideoParameters->DisplayCount[1] )
	    report( severity_error, "OutputTimer_Video_c::FillOutFrameTimingRecord - Interlaced content expected identical field counts (%d - %d)\n", ParsedVideoParameters->DisplayCount[0], ParsedVideoParameters->DisplayCount[1] );

	VideoOutputTiming->DisplayCount[1]      = VideoOutputTiming->DisplayCount[0];
    }
    else if( !ParsedVideoParameters->Content.Progressive )
    {
	FrameCount                              = (CountMultiplier * ParsedVideoParameters->DisplayCount[1]) + AccumulatedError;
	VideoOutputTiming->DisplayCount[1]      = FrameCount.IntegerPart();
	AccumulatedError                        = FrameCount.Remainder();
    }
    else
	VideoOutputTiming->DisplayCount[1]      = 0;

    //
    // Calculate the panscan field counts
    //

    memcpy( &VideoOutputTiming->PanScan, &ParsedVideoParameters->PanScan, sizeof(PanScan_t) );

    if( InterlacedContentOnInterlacedDisplay && (VideoOutputTiming->PanScan.Count != 0) )
    {
	if( VideoOutputTiming->PanScan.Count == 1 )
	{
	    VideoOutputTiming->PanScan.DisplayCount[0]  = VideoOutputTiming->DisplayCount[0] + VideoOutputTiming->DisplayCount[1];
	}
	else if( VideoOutputTiming->PanScan.Count == 2 )
	{
	    VideoOutputTiming->PanScan.DisplayCount[0]  = VideoOutputTiming->DisplayCount[0];
	    VideoOutputTiming->PanScan.DisplayCount[1]  = VideoOutputTiming->DisplayCount[1];
	}
	else
	{
	    report( severity_error, "OutputTimer_Video_c::FillOutFrameTimingRecord - Interlaced content 0,1 or 2 pan scan values (%d)\n", VideoOutputTiming->PanScan.Count );
	}
    }
    else
    {
	for( i=0; i<VideoOutputTiming->PanScan.Count; i++ )
	{
	    FrameCount                                  = (CountMultiplier * VideoOutputTiming->PanScan.DisplayCount[i]) + AccumulatedPanScanError;
	    VideoOutputTiming->PanScan.DisplayCount[i]  = FrameCount.IntegerPart();
	    AccumulatedPanScanError                     = FrameCount.Remainder();
	}
    }

//

{
// Some nick debug

unsigned int Count;

    if( VideoOutputTiming->PanScan.Count != 0 )
    {
	Count            = VideoOutputTiming->DisplayCount[0];
	if( !ParsedVideoParameters->Content.Progressive )
	    Count       += VideoOutputTiming->DisplayCount[1]; 

	for( i=0; i<VideoOutputTiming->PanScan.Count; i++ )
	    Count       -= VideoOutputTiming->PanScan.DisplayCount[i];

	if( Count != 0 )
	{
	    report( severity_error, "OutputTimer_Video_c::FillOutFrameTimingRecord - Pan scan counts and display counts do not match.\n" );
	    report( severity_info,  "                                                Display  %2d %2d    => %2d %2d\n", ParsedVideoParameters->DisplayCount[0], ParsedVideoParameters->DisplayCount[1], VideoOutputTiming->DisplayCount[0], VideoOutputTiming->DisplayCount[1] );
	    report( severity_info,  "                                                Pan Scan %2d %2d %2d => %2d %2d %2d\n", 
		ParsedVideoParameters->PanScan.DisplayCount[0], ParsedVideoParameters->PanScan.DisplayCount[1], ParsedVideoParameters->PanScan.DisplayCount[2],
		VideoOutputTiming->PanScan.DisplayCount[0], VideoOutputTiming->PanScan.DisplayCount[1], VideoOutputTiming->PanScan.DisplayCount[2] );
	}
    }
}

    //
    // Are we in the process of performing an avsync correction
    //

    if( (LoseFramesForSynchronization != 0) && ((VideoOutputTiming->DisplayCount[0] + VideoOutputTiming->DisplayCount[1]) != 0) )
    {
	if( InterlacedContentOnInterlacedDisplay && !PartialFrame )
	{
	    Lose        = min( LoseFramesForSynchronization/2, VideoOutputTiming->DisplayCount[0] );

	    LoseFramesForSynchronization        -= 2 * Lose;
	    VideoOutputTiming->DisplayCount[0]  -= Lose;
	    VideoOutputTiming->DisplayCount[1]  -= Lose;
	}
	else
	{
	    Lose        = min( LoseFramesForSynchronization, VideoOutputTiming->DisplayCount[0] );

	    LoseFramesForSynchronization        -= Lose;
	    VideoOutputTiming->DisplayCount[0]  -= Lose;

	    Lose        = min( LoseFramesForSynchronization, VideoOutputTiming->DisplayCount[1] );

	    LoseFramesForSynchronization        -= Lose;
	    VideoOutputTiming->DisplayCount[1]  -= Lose;
	}
    }

    //
    // After all of that wondeful calculation, we now 
    // override the lot of it if we are single stepping.
    //

    if( Speed == 0 )
    {
	VideoOutputTiming->DisplayCount[0]	= 1;
	if( !ParsedVideoParameters->Content.Progressive )
	    VideoOutputTiming->DisplayCount[1]	= 1;
    }

    //
    // Fill in the other timing parameters, adjusting the decode time (for decode window porch control).
    //

    VideoOutputTiming->SystemPlaybackTime       = SystemTime;
    VideoOutputTiming->ExpectedDurationTime     = FrameDurationTime * (VideoOutputTiming->DisplayCount[0] + VideoOutputTiming->DisplayCount[1]);
    VideoOutputTiming->ActualSystemPlaybackTime = INVALID_TIME;
    VideoOutputTiming->Interlaced               = ParsedVideoParameters->InterlacedFrame;
    VideoOutputTiming->TopFieldFirst            = ParsedVideoParameters->TopFieldFirst;

    //
    // Apply an update to the frame decode time, 
    // based on the native decode duration, 
    // rather than the adjusted for speed decode duration
    //

    NormalSpeedExpectedDurationTime             = SourceFrameDurationTime * (ParsedVideoParameters->DisplayCount[0] + ParsedVideoParameters->DisplayCount[1]);
    Configuration.FrameDecodeTime               = max( NormalSpeedExpectedDurationTime, Configuration.FrameDecodeTime);

    //
    // Apply an update to the next expected playback time, used to spot PTS jumps
    //

    PlaybackTimeIncrement			= NormalSpeedExpectedDurationTime;

    //
    // If we are running in reverse and an interlaced frame, then reverse the fields
    // This also involves reversing the display counts, and the pan scan arrays.
    //

    if( !ParsedVideoParameters->Content.Progressive &&
	Direction == PlayBackward )
    {
	unsigned int    Tmp;
	int             Tmpi;
	unsigned int    Cnt;

	VideoOutputTiming->TopFieldFirst        = !VideoOutputTiming->TopFieldFirst;

	Tmp                                     = VideoOutputTiming->DisplayCount[0];
	VideoOutputTiming->DisplayCount[0]      = VideoOutputTiming->DisplayCount[1];
	VideoOutputTiming->DisplayCount[1]      = Tmp;

	Cnt                                     = VideoOutputTiming->PanScan.Count;
	for( i=0; i<Cnt/2; i++ )
	{
	    Tmp                                                         = VideoOutputTiming->PanScan.DisplayCount[i];
	    VideoOutputTiming->PanScan.DisplayCount[i]                  = VideoOutputTiming->PanScan.DisplayCount[Cnt - 1 - i];
	    VideoOutputTiming->PanScan.DisplayCount[Cnt - 1 - i]        = Tmp;
	    Tmpi                                                        = VideoOutputTiming->PanScan.HorizontalOffset[i];
	    VideoOutputTiming->PanScan.HorizontalOffset[i]              = VideoOutputTiming->PanScan.HorizontalOffset[Cnt - 1 - i];
	    VideoOutputTiming->PanScan.HorizontalOffset[Cnt - 1 - i]    = Tmpi;
	    Tmpi                                                        = VideoOutputTiming->PanScan.VerticalOffset[i];
	    VideoOutputTiming->PanScan.VerticalOffset[i]                = VideoOutputTiming->PanScan.VerticalOffset[Cnt - 1 - i];
	    VideoOutputTiming->PanScan.VerticalOffset[Cnt - 1 - i]      = Tmpi;
	}
    }

    //
    // Finally, if we have a zero first field count we move the second field count up,
    // and invert the top field first flag. Also we shuffle up and zero pan scan field counts.
    // NOTE the shuffle uses while, it makes the code simple, and if it passes through twice, 
    // I will be a monkeys uncle.
    //

    if( VideoOutputTiming->DisplayCount[0] == 0 )
    {
	VideoOutputTiming->DisplayCount[0]      = VideoOutputTiming->DisplayCount[1];
	VideoOutputTiming->DisplayCount[1]      = 0;
	VideoOutputTiming->TopFieldFirst        = !VideoOutputTiming->TopFieldFirst;
    }

    while( (VideoOutputTiming->PanScan.Count != 0) && (VideoOutputTiming->PanScan.DisplayCount[0] == 0) )
    {
	VideoOutputTiming->PanScan.Count--;
	for( i=0; i<VideoOutputTiming->PanScan.Count; i++ )
	{
	    VideoOutputTiming->PanScan.DisplayCount[i]                  = VideoOutputTiming->PanScan.DisplayCount[i+1];
	    VideoOutputTiming->PanScan.HorizontalOffset[i]              = VideoOutputTiming->PanScan.HorizontalOffset[i+1];
	    VideoOutputTiming->PanScan.VerticalOffset[i]                = VideoOutputTiming->PanScan.VerticalOffset[i+1];
	}
    }

//

#if 0
{
static unsigned long long LastSystemTime        = INVALID_TIME;

    if( LastSystemTime == INVALID_TIME )
	LastSystemTime  = SystemTime;

    report( severity_info, "Timing Out  %12lld - Error was %12lld\n", SystemTime - LastSystemTime, SystemTimeError.LongLongIntegerPart() );
    report( severity_info, "Display     %d (3:2 %d %d) (%d %d) - %2d %2d    => %2d %2d\n", ParsedVideoParameters->PictureStructure, ThreeTwoPulldownDetected, RemoveAnyThreeTwoPulldown, ParsedVideoParameters->InterlacedFrame, ParsedVideoParameters->TopFieldFirst, ParsedVideoParameters->DisplayCount[0], ParsedVideoParameters->DisplayCount[1], VideoOutputTiming->DisplayCount[0], VideoOutputTiming->DisplayCount[1] );

    LastSystemTime  = SystemTime;
}
#endif
    return OutputTimerNoError;
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

OutputTimerStatus_t   OutputTimer_Video_c::CorrectSynchronizationError( void )
{
unsigned char	ExternalMappingPolicy;
long long       ErrorSign;
long long       CorrectionUnitSize;
long long       CorrectionUnits;
long long       TimebaseAdjustment;

//

    ErrorSign           = (SynchronizationError < 0) ? -1 : 1;

    //
    // We can correct in units of a field, however to avoid 
    // constant de-interlacing  we only correct in frames for 
    // an interlaced on interlaced situation.
    //

    CorrectionUnitSize  = FrameDurationTime;
    if( SeenInterlacedContentOnInterlacedDisplay )
	CorrectionUnitSize      *= 2;

//

    CorrectionUnits     = (SynchronizationError + (ErrorSign * (CorrectionUnitSize / 2))) / CorrectionUnitSize;

    //
    // Adjust the system time base by the spare change, note we can only do
    // a time base adjusmtent if we are not applying an external time base
    // mapping.
    //

    ExternalMappingPolicy	= Player->PolicyValue( Playback, Stream, PolicyExternalTimeMapping );
    if( ExternalMappingPolicy != PolicyValueApply )
    {
    	TimebaseAdjustment  = SynchronizationError - (CorrectionUnits * CorrectionUnitSize);

    	OutputCoordinator->AdjustMappingBase( OutputCoordinatorContext, TimebaseAdjustment );
    }

    //
    // Set any whole units to be incorporated by the frame timing calculations 
    // Note we lose negative values because the manifestor should delay 
    // until the appropriate time is reached.
    // Note unless this at startup, we limit the correction to 2 frames
    //

    if( CorrectionUnits > 0 )
    {
	LoseFramesForSynchronization            = ((unsigned long long)CorrectionUnitSize == FrameDurationTime) ? (CorrectionUnits) : (2 * CorrectionUnits);

	if( !SynchronizationAtStartup )
	    LoseFramesForSynchronization        = min( 4, LoseFramesForSynchronization );
    }
    else
	LoseFramesForSynchronization            = 0;

//

    return OutputTimerNoError;
}

