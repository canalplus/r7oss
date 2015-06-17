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

Source file name : output_coordinator.h
Author :           Nick

Definition of the output coordinator class module for player 2.


Date        Modification                                    Name
----        ------------                                    --------
12-Mar-07   Created                                         Nick

************************************************************************/

#ifndef H_OUTPUT_COORDINATOR
#define H_OUTPUT_COORDINATOR

#include "player.h"

// ---------------------------------------------------------------------
//
// Local type definitions
//

enum
{
    OutputCoordinatorNoError				= PlayerNoError,
    OutputCoordinatorError				= PlayerError,

//

    OutputCoordinatorMappingNotEstablished		= BASE_OUTPUT_COORDINATOR,
    OutputCoordinatorAbandonedWait,
    OutputCoordinatorMappingAdjusted
};

typedef PlayerStatus_t	OutputCoordinatorStatus_t;

//

enum
{
    OutputCoordinatorFnRegisterStream			= BASE_OUTPUT_COORDINATOR,
    OutputCoordinatorFnDeRegisterStream,
    OutputCoordinatorFnSetPlaybackSpeed,
    OutputCoordinatorFnResetTimeMapping,
    OutputCoordinatorFnEstablishTimeMapping,
    OutputCoordinatorFnTranslatePlaybackTimeToSystem,
    OutputCoordinatorFnTranslateSystemTimeToPlayback,
    OutputCoordinatorFnSynchronizeStreams,
    OutputCoordinatorFnPerformEntryIntoDecodeWindowWait,
    OutputCoordinatorFnHandlePlaybackTimeDeltas,
    OutputCoordinatorFnCalculateOutputRateAdjustment,
    OutputCoordinatorFnAdjustMappingBase,
    OutputCoordinatorFnMappingBaseAdjustmentApplied,

    OutputCoordinatorFnSetModuleParameters
};

// ---------------------------------------------------------------------
//
// Context definition - Note this is an obfuscated 
// type you are not supposed to know what it is.
//

typedef struct OutputCoordinatorContext_s           *OutputCoordinatorContext_t;

#define PlaybackContext		NULL

// ---------------------------------------------------------------------
//
// Class definition
//

class OutputCoordinator_c : public BaseComponentClass_c
{
public:

    virtual OutputCoordinatorStatus_t   RegisterStream(		PlayerStream_t			  Stream,
								PlayerStreamType_t		  StreamType,
								OutputCoordinatorContext_t	 *Context ) = 0;

    virtual OutputCoordinatorStatus_t   DeRegisterStream(	OutputCoordinatorContext_t	  Context ) = 0;

    virtual OutputCoordinatorStatus_t   SetPlaybackSpeed(	OutputCoordinatorContext_t	  Context,
								Rational_t			  Speed,
								PlayDirection_t			  Direction ) = 0;

    virtual OutputCoordinatorStatus_t   ResetTimeMapping(	OutputCoordinatorContext_t	  Context ) = 0;

    virtual OutputCoordinatorStatus_t   EstablishTimeMapping(	OutputCoordinatorContext_t	  Context,
								unsigned long long		  NormalizedPlaybackTime,
								unsigned long long		  SystemTime = INVALID_TIME ) = 0;

    virtual OutputCoordinatorStatus_t   TranslatePlaybackTimeToSystem(
								OutputCoordinatorContext_t	  Context,
								unsigned long long		  NormalizedPlaybackTime,
								unsigned long long		 *SystemTime ) = 0;

    virtual OutputCoordinatorStatus_t   TranslateSystemTimeToPlayback(
								OutputCoordinatorContext_t	  Context,
								unsigned long long		  SystemTime,
								unsigned long long		 *NormalizedPlaybackTime ) = 0;

    virtual OutputCoordinatorStatus_t   SynchronizeStreams(	OutputCoordinatorContext_t	  Context,
								unsigned long long		  NormalizedPlaybackTime,
								unsigned long long		  NormalizedDecodeTime,
								unsigned long long		 *SystemTime ) = 0;

    virtual OutputCoordinatorStatus_t   PerformEntryIntoDecodeWindowWait(
								OutputCoordinatorContext_t	  Context,
								unsigned long long		  NormalizedDecodeTime,
								unsigned long long		  DecodeWindowPorch,
								unsigned long long		  MaximumAllowedSleepTime ) = 0;

    virtual OutputCoordinatorStatus_t   HandlePlaybackTimeDeltas(
								OutputCoordinatorContext_t	  Context,
								bool				  KnownJump,
								unsigned long long		  ExpectedPlaybackTime,
								unsigned long long		  ActualPlaybackTime ) = 0;

    virtual OutputCoordinatorStatus_t   CalculateOutputRateAdjustment(
								OutputCoordinatorContext_t	  Context,
								unsigned long long		  ExpectedDuration,
								unsigned long long		  ActualDuration,
								long long			  CurrentError,
								Rational_t			 *OutputRateAdjustment,
								Rational_t			 *SystemClockAdjustment ) = 0;

    virtual OutputCoordinatorStatus_t   RestartOutputRateIntegration(
								OutputCoordinatorContext_t	  Context ) = 0;

    virtual OutputCoordinatorStatus_t   AdjustMappingBase( 	OutputCoordinatorContext_t	  Context,
								long long			  Adjustment ) = 0;

    virtual OutputCoordinatorStatus_t   MappingBaseAdjustmentApplied( 
								OutputCoordinatorContext_t	  Context ) = 0;

    virtual OutputCoordinatorStatus_t   GetStreamStartDelay(	OutputCoordinatorContext_t	  Context,
								unsigned long long		 *Delay ) = 0;

    virtual OutputCoordinatorStatus_t   MonitorVsyncOffset( 	OutputCoordinatorContext_t	  Context, 
								unsigned long long		  RequestedOutputTime, 
								unsigned long long 		  ActualOutputTime ) = 0;

    virtual OutputCoordinatorStatus_t   ClockRecoveryInitialize(PlayerTimeFormat_t		  SourceTimeFormat ) = 0;

    virtual OutputCoordinatorStatus_t   ClockRecoveryDataPoint(	unsigned long long		  SourceTime,
								unsigned long long		  LocalTime ) = 0;

    virtual OutputCoordinatorStatus_t   ClockRecoveryEstimate(	unsigned long long		 *SourceTime,
								unsigned long long		 *LocalTime ) = 0;

};

// ---------------------------------------------------------------------
//
// Documentation
//

/*! \class OutputCoordinator_c
    \brief The OutputCoordinator class is responsible for coordinating the output timing of streams in a playback
    
    This class is used to tie together the mappings between system time and playback time for the 
    streams that make up a playback. It handles the establishment of that mapping whether by an imposed
    external setting (application level), or by synchronizing the start of a number of streams.

    This class uses a context to identify the specific stream, and to hold the mapping in force on each stream.

    This class makes no use of buffers or of meta data attached to buffers.

    Some notes on time mappings.

    Time mappings global mappings for a playback, but have local, stream specific, aspects. A playback will move 
    from one time mnapping to another one when a discontinuity is encounterred within a playback. Since different 
    streams within a playback will have the discontinuity pass thorugh timing functions at different (though closely 
    spaced) times, different streams will transition between time mappings at different times. Through careful mangement 
    of the context associated with a stream, and the implementation of the synchronize function, the output coordinator 
    must manage these phased transitions. In addition it is necessary for the functions that translate between time domains
    to know which stream the translation is for (since the same playback time may generate completely different system 
    times, depending on which mapping is in force).
    
*/

/*! \fn OutputCoordinatorStatus_t OutputCoordinator_c::RegisterStream( 	PlayerStream_t			  Stream,
									PlayerStreamType_t		  StreamType,
                                                                	OutputCoordinatorContext_t       *Context );
    \brief Register a stream to the OutputCoordinator.

    This call informs the coordinator of a stream that is to be coordinated, and of the nature of that 
    stream, and obtains a context used to identify stream specific coordination information. 

    Different policies, that may be in force in the player, affect the way in which different stream types 
    (audio/video/data) are handled in terms of synchronization etc... It is important then that the coordinator 
    is aware of the type of a stream.

    \param Stream The stream to be coordinated.
    \param StreamType An indicator of the nature of the stream.
    \param Context A pointer to a returned context identifier

    \return OutputCoordinator status code, OutputCoordinatorNoError indicates success.
*/

/*! \fn OutputCoordinatorStatus_t OutputCoordinator_c::DeRegisterStream(OutputCoordinatorContext_t	Context );
    \brief Remove a stream from consideration by the OutputCoordinator.

    Calling this function will remove the specified stream, identified by Context,  from consideration by the OutputCoordinator.

    \param Context Identifier for the context of the stream to be removed from consideration.

    \return OutputCoordinator status code, OutputCoordinatorNoError indicates success.
*/

/*! \fn OutputCoordinatorStatus_t OutputCoordinator_c::SetPlaybackSpeed(OutputCoordinatorContext_t	Context,
									Rational_t			Speed,
									PlayDirection_t         	Direction );
    \brief Inform the OutputCoordinator of a new playback speed and direction.

    The playback speed affects the relative rate at which playback time passes with respect 
    to system time, and directly affects the mapping of playback times to system times. In 
    addition the setting of a new playback speed will abort any attempt to synchronize streams 
    for startup. Initially only the PlaybackContext will be allowed as the Context identifier, 
    if we ever get to understand the meaning of having different speeds for different streams we may allow 
    stream specific speed setting.

    \param Context Identifier for the context of the stream.
    \param Speed A scalar value representing the speed of playback. FIX(1) represents normal speed.
    \param Direction The direction of playback (forward/reverse).

    \return OutputCoordinator status code, OutputCoordinatorNoError indicates success.
*/

/*! \fn OutputCoordinatorStatus_t OutputCoordinator_c::ResetTimeMapping( OutputCoordinatorContext_t	Context )
    \brief Invalidate the playback/system time mapping.

    This function invalidates the mapping between system and playback times, for one stream, or for thew whole playback. 
    It makes it impossible 
    to obtain translations between system and playback times until a new mapping has beeen established.

    A new mapping may be established by synchronizing the streams of a playback, or by 
    imposing a mapping from the application.

    \param Context Stream context identifier.

    \return OutputCoordinator status code, OutputCoordinatorNoError indicates success.
*/

/*! \fn OutputCoordinatorStatus_t OutputCoordinator_c::EstablishTimeMapping(    OutputCoordinatorContext_t        Context,
                                                                unsigned long long                NormalizedPlaybackTime,
                                                                unsigned long long                SystemTime = INVALID_TIME );
    \brief Impose a playback/system time mapping.
    
    This function allows the external application to impose a system/playback time mapping by 
    identifying a system time and playback time that are synchronous. If the system time is 
    specified as 'INVALID_TIME', then the current system time is used.

    It is anticipated that Context will be PlaybackContext.

    It should be noted that the specification of a synchronous point forms only one part of 
    the mapping, the complete mapping is specified by the identification of a synchronous point,
    plus a scaling factor which is the Speed of playback (or its' inverse).

    Due to the nature of elapsing time, in order to guarantee a specific mapping it is important 
    to specify the speed before specifying the synchronous point.

    \param Context Stream context identifier.
    \param NormalizedPlaybackTime A playback time.
    \param SystemTime The system time with which it is synchronous.

    \return OutputCoordinator status code, OutputCoordinatorNoError indicates success.
*/

/*! \fn OutputCoordinatorStatus_t OutputCoordinator_c::TranslatePlaybackTimeToSystem(
								OutputCoordinatorContext_t        Context,
                                                                unsigned long long                NormalizedPlaybackTime,
                                                                unsigned long long               *SystemTime );
    \brief Translate a playback time to a system time.
    
    Apply the playback to system time mapping to the specified playback time.

    \param Context Stream context identifier.
    \param NormalizedPlaybackTime A playback time.
    \param SystemTime A pointer to a variable to hold the mapped playback time.

    \return OutputCoordinator status code, OutputCoordinatorNoError indicates success, 
	    OutputCoordinatorMappingNotEstablished indicates that no mapping has yet been established.
*/

/*! \fn OutputCoordinatorStatus_t OutputCoordinator_c::TranslateSystemTimeToPlayback(
								OutputCoordinatorContext_t        Context,
                                                                unsigned long long                SystemTime,
                                                                unsigned long long               *NormalizedPlaybackTime );
    \brief Translate a system time to a playback time.
    
    Apply the inverse of playback to system time mapping to the specified system time.

    \param Context Stream context identifier.
    \param SystemTime A system time.
    \param NormalizedPlaybackTime A pointer to a variable to hold the mapped system time.

    \return OutputCoordinator status code, OutputCoordinatorNoError indicates success, 
	    OutputCoordinatorMappingNotEstablished indicates that no mapping has yet been established.
*/

/*! \fn OutputCoordinatorStatus_t OutputCoordinator_c::SynchronizeStreams( 	OutputCoordinatorContext_t	  Context,
										unsigned long long		  NormalizedPlaybackTime,
										unsigned long long		  NormalizedDecodeTime,
										unsigned long long		 *SystemTime );
    \brief Establish a time mapping based on stream content.
    
    This function is responsible for establishing a time mapping and performing startup 
    synchronization for all of the currently registered streams. It waits until all the 
    streams have enterred this function before establishing a mapping based on the stream 
    with the earliest playback time. 

    Also on entry each stream is delayed to allow sufficient frames to be decoded to allow 
    smooth playback on return from this function.

    The function can establish a mapping based on a subset of the streams, if not all 
    streams have eneterred the function within a reasonable time period, currently 
    defined as a constant.

    \param Context Stream context identifier.
    \param NormalizedPlaybackTime The normalized playback time that it is attempting to synchronize.
    \param NormalizedDecodeTime The normalized decode time of the frame that it is attempting to synchronize.
    \param SystemTime A pointer to a variable to hold the mapped playback time, when the mapping is established.

    \return OutputCoordinator status code, OutputCoordinatorNoError indicates success.
*/

/*! \fn OutputCoordinatorStatus_t   OutputCoordinator_c::PerformEntryIntoDecodeWindowWait(
								OutputCoordinatorContext_t	  Context,
								unsigned long long		  NormalizedDecodeTime,
								unsigned long long		  DecodeWindowPorch,
								unsigned long long		  MaximumAllowedSleepTime );
    \brief Wait until we enter the decode window for a frame.
    
    This function is responsible for performing the wait until entry into the decode 
    window, this will commence
    a time 'DecodeWindowPorch' micro seconds before the system time
    associated with the 'NormalizedDecodeTime', applying a sensible maximum
    wait time defined by 'MaximumAllowedSleepTime' scaled by the speed at
    which we are playing. The wait for entry can be aborted on one of
    several conditions including a loss of time mapping (in which case there
    is no system time associated with the decode time) or a change of speed
    (in which case the maximum sleep time changes meaning).

    \param Context Stream context identifier.
    \param NormalizedDecodeTime The normalized decode time of the window we will await entry to.
    \param DecodeWindowPorch The time, in micro seconds, before the decode time at which the window starts.
    \param MaximumAllowedSleepTime A limit on how long we may wait, expressed in playback time.

    \return OutputCoordinator status code, OutputCoordinatorNoError indicates success.
*/

/*! \fn OutputCoordinatorStatus_t   OutputCoordinator_c::HandlePlaybackTimeDeltas(
								OutputCoordinatorContext_t	  Context,
								bool				  KnownJump,
								unsigned long long		  ExpectedPlaybackTime,
								unsigned long long		  ActualPlaybackTime );
    \brief Spot, and handle, jumps in PTS values.
    
    This function is responsible for monitoring the playback times, and detecting large jumps in them.
    It will adjust the system time base when a jump is detected in a stream, and check that all other 
    streams also replicate the jump at an appropriate point. If a stream fails to replicate the jump, 
    then synchronization will be invalidated, and all streams will be invited to re-synchronize.

    \param Context Stream context identifier.
    \param KnownJump flag indicating that jump detection is not necessary, a jump is known to have occurred.
    \param ExpectedPlaybackTime The normalized playback time that is expected.
    \param ActualPlaybackTime The actual playback time seen.

    \return OutputCoordinator status code, OutputCoordinatorNoError indicates success.
*/

/*! \fn OutputCoordinatorStatus_t   OutputCoordinator_c::CalculateOutputRateAdjustment(
								OutputCoordinatorContext_t	  Context,
								unsigned long long		  ExpectedDuration,
								unsigned long long		  ActualDuration,
								long long			  CurrentError,
								Rational_t			 *OutputRateAdjustment,
								Rational_t			 *SystemClockAdjustment );
    \brief Calculate output rate adjustment.
    
    This function is responsible for calculating the necessary adjustments to the output rate
    to keep the output in synchronization with master clock. If this is context refers to the master 
    clock, this function also calculates the internally held adjustment to the system clock to
    keep it synchronized with the master output device.

    This function will (within the supplied context) maintain a history of expected durations and 
    actual durations and will generate clock adjustments that are meant to lead to these being zero 
    on an ongoing basis. Large differences relating to jitter or avsync issues will be ignored for 
    this purpose.

    \param Context Stream context identifier.
    \param ExpectedDuration The time, in system clock ticks, that this frame was expected to take to manifest.
    \param ActualDuration The actual time, in system clock ticks, that this frame took to manifest.
    \param CurrentError The current error, in system clock ticks, between when this frame expected to manifest, and when it actually manifested.
    \param OutputRateAdjustment The value to be used as an output rate adjustment.
    \param SystemClockAdjustment The current value of the system clock adjustment factor.

    \return OutputCoordinator status code, OutputCoordinatorNoError indicates success.
*/

/*! \fn OutputCoordinatorStatus_t   RestartOutputRateIntegration( OutputCoordinatorContext_t	  Context );
    \brief Restart the output rate integration.
    
    Restart the output rate integration, with an ignore period first. This function is provided 
    to reset the integration when an avsync operation is performed.

    \param Context Stream context identifier.

    \return OutputCoordinator status code, OutputCoordinatorNoError indicates success.
*/

/*! \fn     OutputCoordinatorStatus_t   OutputCoordinator_c::AdjustMappingBase( 	OutputCoordinatorContext_t	  Context,
								long long			  Adjustment );
    \brief Adjust the base system time for playback/system time mappings.
    
    This function is provided to support audio/video synchronization, in order to hone the sync to 
    within 3 ms (a period well inside the limit of control for a video display), it is necessary 
    for a video stream to adjust the base system time, to force the audio to apply its finer grained 
    controls to close sub field/frame synchronization gaps.

    \param Context Stream context identifier.
    \param Adjustment the adjustment, in system clock ticks, to be applied.

    \return OutputCoordinator status code, OutputCoordinatorNoError indicates success.
*/

/*! \fn     OutputCoordinatorStatus_t   OutputCoordinator_c::MappingBaseAdjustmentApplied( 	OutputCoordinatorContext_t	  Context );
    \brief Check if the base system time for playback/system time mappings has been modified.
    
    This function is provided to support audio/video synchronization, allowing an 
    output timer to find out if any adjustments to the base system time have been 
    applied since the last call to this function.

    \param Context Stream context identifier.

    \return OutputCoordinator status code, OutputCoordinatorNoError indicates no adjustment has been applied,
	OutputCoordinatorMappingAdjusted indicates that a change has occurred.
*/

/*! \fn     OutputCoordinatorStatus_t   OutputCoordinator_c::GetStreamStartDelay( 	OutputCoordinatorContext_t	  Context,
											unsigned long long		 *Delay );
    \brief Find the delay between the start of this stream and the earliest starting stream in the playback.
    
    This function is provided to support throttling of data injection into the player.

    \param Context Stream context identifier.
    \param Delay   The deduced delay value.

    \return OutputCoordinator status code, OutputCoordinatorNoError should be returned if the delay value is valid.
*/

/*! \fn     OutputCoordinatorStatus_t   OutputCoordinator_c::MonitorVsyncOffset( 	OutputCoordinatorContext_t	  Context, 
											unsigned long long		  RequestedOutputTime, 
											unsigned long long 		  ActualOutputTime );
    \brief Monitor vsync offset for reporting via event signal.
    
    This function is provided to support dvp control when not using vsync locking, in addition to 
    monitoring the vsync offset, the function will adjust the time mapping to eliminate it.

    \param Context Stream context identifier.
    \param RequestedOutputTime   When we wanted the frame to display.
    \param RequestedOutputTime   When the frame actually displayed.

    \return OutputCoordinator status code, OutputCoordinatorNoError should be returned.
*/

/*! \fn OutputCoordinatorStatus_t   OutputCoordinator_c::ClockRecoveryInitialize(
							OutputCoordinatorTimeFormat_t	  SourceTimeFormat )
\brief Initialize clock recovery

\param SourceTimeFormat the format of source times

\return OutputCoordinator status code, OutputCoordinatorNoError should be returned.
*/

/*! \fn OutputCoordinatorStatus_t   OutputCoordinator_c::ClockRecoveryDataPoint(
							unsigned long long	  SourceTime,
							unsigned long long	  LocalTime )
\brief Specify a clock recovery data point

A function used to mark a clock recovery data point, specifies a source clock value, 
and a local time (monotonic local time), at which the packet transmitted at source 
time arrived at the box. 

\param SourceTime The clock value at the source
\param LocalTime Arrival time of the packet containing the source clock value

\return OutputCoordinator status code, OutputCoordinatorNoError should be returned.
*/

/*! \fn OutputCoordinatorStatus_t   OutputCoordinator_c::ClockRecoveryEstimate(	
							unsigned long long	 *SourceTime,
							unsigned long long	 *LocalTime )
\brief Request a clock recovery estimate

\param SourceTime The value of source time (in the appropriate format) when the function is called.
\param LocalTime A copy of the monotonic clock at the time this function was called.

\return OutputCoordinator status code, OutputCoordinatorNoError should be returned.
*/
#endif
