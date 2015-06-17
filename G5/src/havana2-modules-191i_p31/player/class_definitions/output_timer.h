/************************************************************************
Copyright (C) 2006 STMicroelectronics. All Rights Reserved.

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

Source file name : output_timer.h
Author :           Nick

Definition of the output timer class module for player 2.


Date        Modification                                    Name
----        ------------                                    --------
01-Nov-06   Created                                         Nick

************************************************************************/

#ifndef H_OUTPUT_TIMER
#define H_OUTPUT_TIMER

#include "player.h"

// ---------------------------------------------------------------------
//
// Local type definitions
//

enum
{
    OutputTimerNoError				= PlayerNoError,
    OutputTimerError				= PlayerError,

    OutputTimerAlreadyPastDecodeWindow		= BASE_OUTPUT_TIMER,
    OutputTimerDecodeWindowTooFarInFuture,

    // Note this bunch must be at end, it is the generic code, 
    // if greater than this then drop the frame.

    OutputTimerDropFrame			= (BASE_OUTPUT_TIMER+0x800),
    OutputTimerDropFrameSingleGroupPlayback,
    OutputTimerDropFrameKeyFramesOnly,
    OutputTimerDropFrameOutsidePresentationInterval,
    OutputTimerDropFrameZeroDuration,
    OutputTimerUntimedFrame,

    OutputTimerTrickModeNotSupportedDropFrame,
    OutputTimerTrickModeDropFrame,
    OutputTimerLateDropFrame,

    OutputTimerDropFrameTooLateForManifestation
};

typedef PlayerStatus_t	OutputTimerStatus_t;

//

enum
{
    OutputTimerFnRegisterOutputCoordinator	= BASE_OUTPUT_TIMER,
    OutputTimerFnResetTimeMapping,
    OutputTimerFnAwaitEntryIntoDecodeWindow,
    OutputTimerFnTestForFrameDrop,
    OutputTimerFnGenerateFrameTiming,
    OutputTimerFnRecordActualFrameTiming,

    OutputTimerFnSetModuleParameters
};

//

typedef enum
{
    OutputTimerBeforeDecodeWindow	= 0,
    OutputTimerBeforeDecode,
    OutputTimerBeforeOutputTiming,
    OutputTimerBeforeManifestation
} OutputTimerTestPoint_t;


// ---------------------------------------------------------------------
//
// Class definition
//

class OutputTimer_c : public BaseComponentClass_c
{
public:

    virtual OutputTimerStatus_t   RegisterOutputCoordinator(	OutputCoordinator_t	  OutputCoordinator ) = 0;

    virtual OutputTimerStatus_t   ResetTimeMapping(		void ) = 0;

    virtual OutputTimerStatus_t   AwaitEntryIntoDecodeWindow(   Buffer_t		  Buffer ) = 0;

    virtual OutputTimerStatus_t   TestForFrameDrop(		Buffer_t		  Buffer,
								OutputTimerTestPoint_t	  TestPoint ) = 0;

    virtual OutputTimerStatus_t   GenerateFrameTiming(		Buffer_t		  Buffer ) = 0;

    virtual OutputTimerStatus_t   RecordActualFrameTiming(	Buffer_t		  Buffer ) = 0;

    virtual OutputTimerStatus_t   GetStreamStartDelay(		unsigned long long	 *Delay ) = 0;
};

// ---------------------------------------------------------------------
//
// Documentation
//

/*! \class OutputTimer_c
    \brief The OutputTimer class is responsible for taking decisions related to output timing
    
    This class is used to generate output timing information for decoded buffers, handling avsync 
    (through use of the OutputCoordinator), for managing frame drop due to late decode, trick mode 
    execution and playlist presentation intervals. 

    The partial list of meta data types used by this class :-

    Attached to input buffers :-

     - ParsedFrameParameters Describes the frame to be manifested.
     - Parsed[Video|Audio|Data]Parameters providing input to the 
       allow generation of stream type specific timing input
       to the manifestation process. 

    Added to output buffers :-

     - [Video|Audio|Data]OutputTiming Describes the output timing 
       to be applied to manifestation of the frame.
*/

/*! \fn OutputTimerStatus_t OutputTimer_c::RegisterOutputCoordinator(OutputCoordinator_t OutputCoordinator);
    \brief Specify the output coordinator.

    This call provides the OutputCoordinator to the OutputTimer, the timer makes use
    of the OutputCoordinator to tie together the presentation times of the separate 
    streams of a playback.

    Calling this function provokes critical actions within the OutputTimer, it will 
    register itself with the OutputCoordinator, and is expected to be in a running 
    state after the call, that is it should be ready to accept buffers for timing, 
    and other function calls.

    \param OutputCoordinator A pointer to an instance of an OutputCoordinator_c class.

    \return OutputTimer status code, OutputTimerNoError indicates success.
*/

/*! \fn OutputTimerStatus_t OutputTimer_c::ResetTimeMapping( void )
    \brief Invalidate the playback/system time mapping.

    This function invalidates the mapping between system and playback times, for the stream. 
    It makes it impossible 
    to obtain translations between system and playback times on the stream until a new mapping has beeen established.

    A new mapping may be established by synchronizing the streams of a playback, or by 
    imposing a mapping from the application.

    \return OutputTimer status code, OutputTimerNoError indicates success.
*/

/*! \fn OutputTimerStatus_t OutputTimer_c::AwaitEntryIntoDecodeWindow(Buffer_t   Buffer)
    \brief perform a process wait until entry to the decode window.

    Calling this function will perform a process sleep until entry into a decode window 
    for the specified buffer, if certain conditions are met. The conditions are first that 
    a decode window is specified, or can be deduced, second that the window has no already 
    been enterred and third that the window is not an unreasonable distance in the future.

    A decode window is specified if the 'NormalizedDecodeTime' field in the 'parsedFrameParameters', 
    associated with the buffer, is not 'INVALID_TIME'. It can be deduced when this is not the case, 
    only if the buffer contains a non-reference frame, by applying an offset to the 
    'NormalizedPlaybackTime' if it is not INVALID_TIME' (which it shopuld not be at this point 
    for a non-reference frame).

    The test for an unreasonable distance in the future, will involve comparing the distance to 
    the decode window agains a value involving (a constant X frame duration)/Speed.

    Any wait will abort should the output timer be halted, or the speed changed.

    \param Buffer A pointer to a buffer instance whose decode window we are waiting for.

    \return OutputTimer status code, OutputTimerNoError indicates success.
*/

/*! \fn OutputTimerStatus_t OutputTimer_c::TestForFrameDrop(Buffer_t Buffer, OutputTimerTestPoint_t TestPoint)
    \brief Test a buffer to see if it should be dropped

    During play, a buffer may be dropped at one of several points, this function 
    is responsible for making the decision to drop a frame.

    Before decode, a buffer can only be be dropped if it is a non-reference frame and :-

	- The current trick mode will not manifest it.
	- The current time is close to or after the presentation time, so it will not decode 
	  in time to present it. 

    After decode, reference frames also can be dropped (though this will only drop the presentation), 
    the reasons why a frame may be dropped are as above.

    \param Buffer A pointer to a buffer instance containing the coded frame (before decode), 
	   or decoded frame (after decode) under consideration.
    \param TestPoint An enumerated value indicating the point at which the drop is being tested.

    \return OutputTimer status code, OutputTimerNoError indicates do not drop the frame, 
            other statuses may indicate a fault, or the reason the frame is to be dropped.
*/

/*! \fn OutputTimerStatus_t OutputTimer_c::GenerateFrameTiming( Buffer_t Buffer )
    \brief Generate the output timing information for a frame.

    This function takes a frame and fills out the [Video|Audio|Data]OutputTiming structure 
    attached to the buffer. 

    This function makes use of the OutputCoordinator to obtain the mapping between playback time 
    and system time, when no mapping has been established, this function may block while waiting
    for the mapping to be established, this block will occur within the OutputCoordinator_c::SynchronizeStreams().

    \param Buffer A pointer to a buffer instance containing the 
           decoded frame under consideration.

    \return OutputTimer status code, OutputTimerNoError indicates success.
*/

/*! \fn OutputTimerStatus_t OutputTimer_c::RecordActualFrameTiming( Buffer_t Buffer )
    \brief Record the actual time a frame was manifested.
    
    This function is responsible for recording the actual time a frame was manifested, and
    the length of time it spent being manifested. It is by comparing these times with the 
    calculated times and adjusting future calculations that avsync will be achieved.

    \return OutputTimer status code, OutputTimerNoError indicates success.
*/

/*! \fn     OutputTimerStatus_t   OutputTimer_c::GetStreamStartDelay( unsigned long long               *Delay );
    \brief Find the delay between the start of this stream and the earliest starting stream in the playback.

    This function is provided to support throttling of data injection into the player.

    \param Delay   The deduced delay value.

    \return OutputTimer status code, OutputTimerNoError should be returned if the delay value is valid.
*/
#endif
