/************************************************************************
Copyright (C) 2002, 2005, 2006 STMicroelectronics. All Rights Reserved.

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

Source file name : player.h
Author :           Nick

Definition of the pure virtual class defining the interface to a player 2
module.


Date        Modification                                    Name
----        ------------                                    --------
22-Jul-02   Created Player1 header                          Nick
01-Nov-06   Re-wrote player 2                               Nick

************************************************************************/

#ifndef H_PLAYER
#define H_PLAYER

// /////////////////////////////////////////////////////////////////////
//
//      Include the numerous consituent header files in a reasonable 
//      order.

#include "osinline.h"
#include "player_types.h"

//

#include "base_component_class.h"

#include "buffer.h"

#include "predefined_metadata_types.h"
#include "owner_identifiers.h"

//

#include "demultiplexor.h"
#include "collator.h"
#include "frame_parser.h"
#include "codec.h"
#include "manifestor.h"
#include "output_coordinator.h"
#include "output_timer.h"

//

// /////////////////////////////////////////////////////////////////////////
//
// The class definition
//

class Player_c
{
public:

    //
    // Globally visible data items
    //

    PlayerStatus_t              InitializationStatus;

    BufferType_t        MetaDataInputDescriptorType;
    BufferType_t        MetaDataCodedFrameParametersType;    
    BufferType_t                MetaDataStartCodeListType;
    BufferType_t                MetaDataParsedFrameParametersType;
    BufferType_t                MetaDataParsedFrameParametersReferenceType;
    BufferType_t                MetaDataParsedVideoParametersType;
    BufferType_t                MetaDataParsedAudioParametersType;
    BufferType_t                MetaDataVideoOutputTimingType;
    BufferType_t                MetaDataAudioOutputTimingType;
    BufferType_t                MetaDataVideoOutputSurfaceDescriptorType;
    BufferType_t                MetaDataAudioOutputSurfaceDescriptorType;
    BufferType_t                MetaDataBufferStructureType;

    BufferType_t                BufferVideoPostProcessingControlType;
    BufferType_t                BufferAudioPostProcessingControlType;

    BufferType_t                MetaDataSequenceNumberType;     //made available so WMA can make it's private CodedFrameBufferPool properly

    //
    // Force destructors to work
    //

    virtual ~Player_c( void ) {};

    //
    // Mechanisms for registering global items
    //

    virtual PlayerStatus_t   RegisterBufferManager(     BufferManager_t           BufferManager ) = 0;

    virtual PlayerStatus_t   RegisterDemultiplexor(     Demultiplexor_t           Demultiplexor ) = 0;

    //
    // Mechanisms relating to event retrieval
    //

    virtual PlayerStatus_t   SpecifySignalledEvents(    PlayerPlayback_t          Playback,
							PlayerStream_t            Stream,
							PlayerEventMask_t         Events,
							void                     *UserData              = NULL ) = 0;

    virtual PlayerStatus_t   SetEventSignal(            PlayerPlayback_t          Playback,
							PlayerStream_t            Stream,
							PlayerEventMask_t         Events,
							OS_Event_t               *Event ) = 0;

    virtual PlayerStatus_t   GetEventRecord(            PlayerPlayback_t          Playback,
							PlayerStream_t            Stream,
							PlayerEventMask_t         Events,
							PlayerEventRecord_t      *Record,
							bool                      NonBlocking           = false ) = 0;

    //
    // Mechanisms for policy management
    //

    virtual PlayerStatus_t   SetPolicy(                 PlayerPlayback_t          Playback,
							PlayerStream_t            Stream,
							PlayerPolicy_t            Policy,
							unsigned char             PolicyValue           = PolicyValueApply ) = 0;

    //
    // Mechanisms for managing playbacks
    //

    virtual PlayerStatus_t   CreatePlayback(            OutputCoordinator_t       OutputCoordinator,
							PlayerPlayback_t         *Playback,
							bool                      SignalEvent           = false,
							void                     *EventUserData         = NULL ) = 0;

    virtual PlayerStatus_t   TerminatePlayback(         PlayerPlayback_t          Playback,
							bool                      SignalEvent           = false,
							void                     *EventUserData         = NULL ) = 0;

    virtual PlayerStatus_t   AddStream(                 PlayerPlayback_t          Playback,
							PlayerStream_t           *Stream,
							PlayerStreamType_t        StreamType,
							Collator_t                Collator,
							FrameParser_t             FrameParser,
							Codec_t                   Codec,
							OutputTimer_t             OutputTimer,
							Manifestor_t              Manifestor            = NULL,
							bool                      SignalEvent           = false,
							void                     *EventUserData         = NULL ) = 0;

    virtual PlayerStatus_t   RemoveStream(              PlayerStream_t            Stream,
							bool                      SignalEvent           = false,
							void                     *EventUserData         = NULL ) = 0;

    virtual PlayerStatus_t   SwitchStream(              PlayerStream_t            Stream,
							Collator_t                Collator		= NULL,
							FrameParser_t             FrameParser		= NULL,
							Codec_t                   Codec			= NULL,
							OutputTimer_t             OutputTimer		= NULL,
							bool                      NonBlocking           = false,
							bool                      SignalEvent           = false,
							void                     *EventUserData         = NULL ) = 0;

    virtual PlayerStatus_t   DrainStream(               PlayerStream_t            Stream,
							bool                      NonBlocking           = false,
							bool                      SignalEvent           = false,
							void                     *EventUserData         = NULL ) = 0;

    //
    // Mechanisms for managing time
    //

    virtual PlayerStatus_t   SetPlaybackSpeed(          PlayerPlayback_t          Playback,
							Rational_t                Speed,
							PlayDirection_t           Direction ) = 0;

    virtual PlayerStatus_t   SetPresentationInterval(   PlayerPlayback_t          Playback,
							PlayerStream_t		  Stream			= PlayerAllStreams,
							unsigned long long        IntervalStartNativeTime       = INVALID_TIME,
							unsigned long long        IntervalEndNativeTime         = INVALID_TIME ) = 0;

    virtual PlayerStatus_t   StreamStep(                PlayerStream_t            Stream ) = 0;

    virtual PlayerStatus_t   SetNativePlaybackTime(     PlayerPlayback_t          Playback,
							unsigned long long        NativeTime,
							unsigned long long        SystemTime                    = INVALID_TIME ) = 0;

    virtual PlayerStatus_t   RetrieveNativePlaybackTime(PlayerPlayback_t          Playback,
							unsigned long long       *NativeTime ) = 0;

    virtual PlayerStatus_t   TranslateNativePlaybackTime(PlayerPlayback_t         Playback,
							unsigned long long        NativeTime,
							unsigned long long       *SystemTime ) = 0;

    virtual PlayerStatus_t   RequestTimeNotification(   PlayerStream_t            Stream,
							unsigned long long        NativeTime,
							void                     *EventUserData         = NULL ) = 0;

    //
    // Clock recovery support - for playbacks with system clock as master
    //

    virtual PlayerStatus_t   ClockRecoveryInitialize(	PlayerPlayback_t	  Playback,
							PlayerTimeFormat_t	  SourceTimeFormat	= TimeFormatPts ) = 0;


    virtual PlayerStatus_t   ClockRecoveryDataPoint(	PlayerPlayback_t	  Playback,
							unsigned long long	  SourceTime,
							unsigned long long	  LocalTime ) = 0;

    virtual PlayerStatus_t   ClockRecoveryEstimate(	PlayerPlayback_t	  Playback,
							unsigned long long	 *SourceTime,
							unsigned long long	 *LocalTime	= NULL ) = 0;

    //
    // Mechanisms for data insertion
    //

    virtual PlayerStatus_t   GetInjectBuffer(           Buffer_t                 *Buffer ) = 0;

    virtual PlayerStatus_t   InjectData(                PlayerPlayback_t          Playback,
							Buffer_t                  Buffer ) = 0;

    virtual PlayerStatus_t   InputJump(                 PlayerPlayback_t          Playback,
							PlayerStream_t            Stream,
							bool                      SurplusDataInjected,
							bool                      ContinuousReverseJump = false ) = 0;

    virtual PlayerStatus_t   InputGlitch(               PlayerPlayback_t          Playback,
							PlayerStream_t            Stream ) = 0;

    //
    // Mechanisms or data extraction 
    //

    virtual PlayerStatus_t   RequestDecodeBufferReference(
							PlayerStream_t            Stream,
							unsigned long long        NativeTime            = TIME_NOT_APPLICABLE,
							void                     *EventUserData         = NULL ) = 0;

    virtual PlayerStatus_t   ReleaseDecodeBufferReference(
							PlayerEventRecord_t      *Record ) = 0;

    //
    // Mechanisms for inserting module specific parameters
    //

    virtual PlayerStatus_t   SetModuleParameters(       PlayerPlayback_t          Playback,
							PlayerStream_t            Stream,
							PlayerComponent_t         Component,
							bool                      Immediately,
							unsigned int              ParameterBlockSize,
							void                     *ParameterBlock ) = 0;

    //
    // Support functions for the child classes
    //

    virtual PlayerStatus_t   GetBufferManager(          BufferManager_t          *BufferManager ) = 0;

    virtual PlayerStatus_t   GetClassList(              PlayerStream_t            Stream,
							Collator_t               *Collator,
							FrameParser_t            *FrameParser,
							Codec_t                  *Codec,
							OutputTimer_t            *OutputTimer,
							Manifestor_t             *Manifestor ) = 0;

    virtual PlayerStatus_t   GetCodedFrameBufferPool(   PlayerStream_t            Stream,
							BufferPool_t             *Pool			= NULL,
							unsigned int             *MaximumCodedFrameSize = NULL ) = 0;

    virtual PlayerStatus_t   GetDecodeBufferPool(       PlayerStream_t            Stream,
							BufferPool_t             *Pool ) = 0;

    virtual PlayerStatus_t   GetPostProcessControlBufferPool(
							PlayerStream_t            Stream,
							BufferPool_t             *Pool ) = 0;

    virtual PlayerStatus_t   CallInSequence(            PlayerStream_t            Stream,
							PlayerSequenceType_t      SequenceType,
							PlayerSequenceValue_t     SequenceValue,
							PlayerComponentFunction_t Fn,
							... ) = 0;

    virtual PlayerStatus_t   GetPlaybackSpeed(          PlayerPlayback_t          Playback,
							Rational_t               *Speed,
							PlayDirection_t          *Direction ) = 0;

    virtual PlayerStatus_t   GetPresentationInterval(   PlayerStream_t 		  Stream,
							unsigned long long       *IntervalStartNormalizedTime,
							unsigned long long       *IntervalEndNormalizedTime ) = 0;

    virtual unsigned char    PolicyValue(               PlayerPlayback_t          Playback,
							PlayerStream_t            Stream,
							PlayerPolicy_t            Policy ) = 0;

    virtual PlayerStatus_t   SignalEvent(               PlayerEventRecord_t      *Record ) = 0;

    virtual PlayerStatus_t   AttachDemultiplexor(       PlayerStream_t            Stream,
							Demultiplexor_t           Demultiplexor,
							DemultiplexorContext_t    Context ) = 0;

    virtual PlayerStatus_t   DetachDemultiplexor(       PlayerStream_t            Stream ) = 0;

    virtual PlayerStatus_t   MarkStreamUnPlayable(      PlayerStream_t            Stream ) = 0;

    virtual PlayerStatus_t   CheckForDemuxBufferMismatch(PlayerPlayback_t         Playback,
							PlayerStream_t            Stream ) = 0;

    virtual void             RecordNonDecodedFrame(	PlayerStream_t		  Stream,
							Buffer_t		  Buffer,
							ParsedFrameParameters_t	 *ParsedFrameParameters ) = 0;

    virtual unsigned long long   GetLastNativeTime(	PlayerPlayback_t	  Playback ) = 0;
    virtual void             SetLastNativeTime(		PlayerPlayback_t	  Playback,
							unsigned long long	  Time ) = 0;
};

// ---------------------------------------------------------------------
//
// Documentation
//

/*! \class Player_c
\brief Glue all the other components together.

This class is the glue that holds all the components. Its methods from 
the primary means for the player wrapper to manipulate a player instance.

*/

/*! \var PlayerStatus_t Player_c::InitializationStatus;
\brief Status flag indicating how the intitialization of the class went.

In order to fit well into the Linux kernel environment the player is 
compiled with exceptions and RTTI disabled. In C++ constructors cannot 
return an error code since all errors should be reported using 
exceptions. With exceptions disabled we need to use this alternative 
means to communicate that the constructed object instance is not valid. 

*/

/*! \var BufferType_t Player_c::MetaDataInputDescriptorType
*/

/*! \var BufferType_t Player_c::MetaDataCodedFrameParametersType
*/

/*! \var BufferType_t Player_c::MetaDataStartCodeListType
*/

/*! \var BufferType_t Player_c::MetaDataParsedFrameParametersType
*/

/*! \var BufferType_t Player_c::MetaDataParsedFrameParametersReferenceType
*/

/*! \var BufferType_t Player_c::MetaDataParsedVideoParametersType
*/

/*! \var BufferType_t Player_c::MetaDataParsedAudioParametersType
*/

/*! \var BufferType_t Player_c::MetaDataVideoOutputTimingType
*/

/*! \var BufferType_t Player_c::MetaDataAudioOutputTimingType
*/

/*! \var BufferType_t Player_c::MetaDataVideoOutputSurfaceDescriptorType
*/

/*! \var BufferType_t Player_c::MetaDataAudioOutputSurfaceDescriptorType
*/

//
// Mechanisms for registering global items
//

/*! \fn PlayerStatus_t Player_c::RegisterBufferManager( BufferManager_t           BufferManager )
\brief Register a buffer manager.

Register a player-global buffer manager.

\param BufferManager A pointer to a BufferManager_c instance.

\return Player status code, PlayerNoError indicates success.
*/

/*! \fn PlayerStatus_t Player_c::RegisterDemultiplexor( Demultiplexor_t           Demultiplexor )
\brief Register a demultiplexor.

Registers a player-global demultiplexor.

\param Demultiplexor A pointer to a Demultiplexor_c instance.

\return Player status code, PlayerNoError indicates success.
*/

//
// Mechanisms relating to event retrieval
//

/*! \fn PlayerStatus_t Player_c::SpecifySignalledEvents(        PlayerPlayback_t          Playback,
							PlayerStream_t            Stream,
							PlayerEventMask_t         Events,
							void                     *UserData              = NULL )
\brief Enable/disable ongoing event signalling.

This function is specifically for enabling/disabling ongoing event signalling.

\param Playback Playback identifier.
\param Stream   Stream identifier.
\param Events   Mask indicating which occurances should raise an event record.
\param UserData Optional user pointer to be associated with any event raised.

\return Player status code, PlayerNoError indicates success.
*/

/*! \fn PlayerStatus_t Player_c::SetEventSignal(            PlayerPlayback_t          Playback,
							PlayerStream_t            Stream,
							PlayerEventMask_t         Events,
							OS_Event_t               *Event )
\brief Set the event to be signalled whenever an event record is generated.

\param Playback Playback identifier.
\param Stream   Stream identifier.
\param Events   Mask indicating which occurances should raise an event record.
\param Event Pointer to an OS abstracted event (not to be confused with Player 2 Event records)

\return Player status code, PlayerNoError indicates success.
*/

/*! \fn PlayerStatus_t Player_c::GetEventRecord(            PlayerPlayback_t          Playback,
							PlayerStream_t            Stream,
							PlayerEventMask_t         Events,
							PlayerEventRecord_t      *Record,
							bool                      NonBlocking           = false )
\brief Retrieve an event record from the player.

This method is responsible for retrieving Event records from the player.
The call is a blocking call by default, but NonBlocking can override this. 

\param Playback Playback identifier.
\param Stream   Stream identifier.
\param Events   Mask indicating which occurances should raise an event record.
\param Record Pointer to an event record (to be filled by the call)
\param NonBlocking If true do not block if there are no events pending.

\return Player status code, PlayerNoError indicates success.
*/

//
// Mechanisms for policy management
//

/*! \fn PlayerStatus_t Player_c::SetPolicy(                     PlayerPlayback_t          Playback,
							PlayerStream_t            Stream,
							PlayerPolicy_t            Policy,
							unsigned char             PolicyValue           = PolicyValueApply )
\brief Set playback policy for the specified playback and stream.

This method allows the control of behaviours within Player 2, whether 
for a specific playback stream, or for all streams. 

\param Playback Playback identifier (or PlayerAllPlaybacks to set policy for all playbacks)
\param Stream   Stream identifier (or PlayerAllStreams to set policy for all streams)
\param Policy   Policy identifier
\param PolicyValue Value for the policy (defaults to PolicyValueApply)

\return Player status code, PlayerNoError indicates success.
*/

//
// Mechanisms for managing playbacks
//

/*! \fn PlayerStatus_t Player_c::CreatePlayback(                OutputCoordinator_t       OutputCoordinator,
							PlayerPlayback_t         *Playback )
\brief Create a playback context.

This method is responsible for creation of a playback context.

\param OutputCoordinator Pointer to an OutputCoordinator_c instance.
\param Playback Pointer to a variable, to be filled with the playback identifier.

\return Player status code, PlayerNoError indicates success.
*/

/*! \fn PlayerStatus_t Player_c::TerminatePlayback(             PlayerPlayback_t          Playback,
							bool                      SignalEvent           = false,
							void                     *EventUserData         = NULL )
\brief Destroy a playback context.

Whether or not the termination should be immediate, or involve the 
playout out all component streams, will be subject to the current 
applicable policies in effect.

\param Playback      Playback identifier to be terminated.
\param SignalEvent   If true, an event record will be generated when termination completes.
\param EventUserData Optional user pointer to be associated with any event raised.

\return Player status code, PlayerNoError indicates success.
*/

/*! \fn PlayerStatus_t Player_c::AddStream(                     PlayerPlayback_t          Playback,
							PlayerStream_t           *Stream,
							PlayerStreamType_t        StreamType,
							Collator_t                Collator,
							FrameParser_t             FrameParser,
							Codec_t                   Codec,
							OutputTimer_t             OutputTimer,
							Manifestor_t              Manifestor            = NULL,
							bool                      SignalEvent           = false,
							void                     *EventUserData         = NULL )

\brief Add a new stream to an existing playback.

\param Playback      Playback identifier.
\param Stream        Pointer to a variable, to be filled with the playback identifier.
\param StreamType    Stream type (audio/video) identifier.
\param Collator      Pointer to a Collator_c instance.
\param FrameParser   Pointer to a FrameParser_c instance.
\param Codec         Pointer to a Codec_c instance.
\param OutputTimer   Pointer to an OutputTimer_c instance.
\param Manifestor    Pointer to a Manifestor_c instance, if NULL, it will be assumed that no decode output buffers will be generated for this stream.
\param SignalEvent   If true, an event record will be generated when the stream's first frame is manifested.
\param EventUserData Optional user pointer to be associated with any event raised.

\return Player status code, PlayerNoError indicates success.
*/

/*! \fn PlayerStatus_t Player_c::RemoveStream(          PlayerStream_t            Stream,
							bool                      SignalEvent           = false,
							void                     *EventUserData         = NULL )
\brief Remove a stream to a playback context.

Like Player_c::TerminatePlayback() the nature of removal will be 
controlled by the policies currently in effect on the stream.

\param Stream        Stream identifier.
\param SignalEvent   If true, an event record will be generated when the removal is complete.
\param EventUserData Optional user pointer to be associated with any event raised.

\return Player status code, PlayerNoError indicates success.
*/

/*! \fn PlayerStatus_t Player_c::SwitchStream(		PlayerStream_t            Stream,
							Collator_t                Collator		= NULL,
							FrameParser_t             FrameParser		= NULL,
							Codec_t                   Codec			= NULL,
							OutputTimer_t             OutputTimer		= NULL,
							bool                      NonBlocking           = false,
							bool                      SignalEvent           = false,
							void                     *EventUserData         = NULL )
\brief Switch seamlessly between streams.

The purpose of a switch is to seamlessly transition between streams
(video/audio) in a playback, as such it is expected that we will be 
playing out the data already injected, and consequently the call can be
non-blocking. The playout nature can be overidden by a current policy. 
After this call all future input data should be for the new stream.

\param Stream        Stream identifier.
\param Collator      Pointer to a new Collator_c instance.
\param FrameParser   Pointer to a new FrameParser_c instance.
\param Codec         Pointer to a new Codec_c instance.
\param OutputTimer   Pointer to a new OutputTimer_c instance.
\param NonBlocking   If true the function call will not block until the first frame queued to the manifestor
\param SignalEvent   If true, an event record will be generated when the first frame of the new stream is manifested.
\param EventUserData Optional user pointer to be associated with any event raised.

\return Player status code, PlayerNoError indicates success.
*/

/*! \fn PlayerStatus_t Player_c::DrainStream(           PlayerStream_t            Stream,
							bool                      NonBlocking           = false,
							bool                      SignalEvent           = false,
							void                     *EventUserData         = NULL )
\brief Play out all data in the specified stream.

Like Player_c::TerminatePlayback() the nature of drain will be 
controlled by the policies currently in effect on the stream.

\param Stream        Stream identifier.
\param NonBlocking   If true do not block until the drain is complete.
\param SignalEvent   If true, an event record will be generated when the drain is complete.
\param EventUserData Optional user pointer to be associated with any event raised.

\return Player status code, PlayerNoError indicates success.
*/

//
// Mechanisms for managing time
//

/*! \fn PlayerStatus_t Player_c::SetPlaybackSpeed(              PlayerPlayback_t          Playback,
							Rational_t                        Speed,
							PlayDirection_t           Direction )
\brief Set the playback speed and direction.

Setting the speed to zero has the effect of pausing the playback.

\param Playback  Playback identifier
\param Speed     Rational playback speed (less than 1 meaning slow motion, greater than one meaning fast wind)
\param Direction Playback direction, either PlayForward or PlayBackward.

\return Player status code, PlayerNoError indicates success.
*/

/*! \fn PlayerStatus_t   Player_c::SetPresentationInterval(     PlayerPlayback_t          Playback,
							PlayerStream_t		  Stream			= PlayerAllStreams,
							unsigned long long        IntervalStartNativeTime       = INVALID_TIME,
							unsigned long long        IntervalEndNativeTime         = INVALID_TIME );
\brief Set the presentation interval.

This function will set the presentation interval. It is a synchronous call in 
that data injected before the call will be subject to the previous interval, 
and data injected after the call will be subject to the limits specified in 
the call, there is no requirement to drain the system before changing the 
interval. The effect of the call will be that non-reference frames with 
presentation times  that do not overlap the interval will be discarded before 
decode, reference frames that do not overlap the interval will be discarded 
after decode. Use of INVALID_TIME makes the appropriate end of the interval 
open (not to be confused with an open interval, which means something 
different entirely). 

The intervals can be set on a playback wide basis, or on an individual stream basis.

\param Playback  Playback identifier
\param Playback  Stream identifier
\param IntervalStartNativeTime  Start of the range of acceptable times.
\param IntervalEndNativeTime  End of the range of acceptable times.

\return Player status code, PlayerNoError indicates success.
*/

/*! \fn PlayerStatus_t Player_c::StreamStep(            PlayerStream_t            Stream )
\brief Step forwards or backwards to the next frame.

This method manifests a single decoded frame, only has any impact when 
the playback speed is set to zero, direction of step is as specified in 
the playback speed.

\param Stream        Stream identifier.

\return Player status code, PlayerNoError indicates success.
*/

/*! \fn PlayerStatus_t Player_c::SetNativePlaybackTime( PlayerPlayback_t          Playback,
							unsigned long long        NativeTime,
							unsigned long long        SystemTime    = INVALID_TIME )
\brief Inform the synchronizer of the current native playback time.

This interface is for the use of external sychronization modules. See
\ref time. 
It sets the synchronization point between the Native playback time, and 
the system time clock. If the system clock is unspecified, then the current 
time is taken as equivalent to the native time.

\param Playback   Playback identifier
\param NativeTime Native stream time.
\param SystemTime The system time equivalent

\return Player status code, PlayerNoError indicates success.
*/

/*! \fn PlayerStatus_t Player_c::RetrieveNativePlaybackTime(PlayerPlayback_t      Playback,
							unsigned long long       *NativeTime )
\brief Ask the synchronizer for the current native playback time.

This interface is for the use of external sychronization modules. See
\ref time.

\param Playback   Playback identifier
\param NativeTime Pointer to a variable to hold the native time.

\return Player status code, PlayerNoError indicates success.
*/

/*! \fn PlayerStatus_t Player_c::TranslateNativePlaybackTime(PlayerPlayback_t     Playback,
							unsigned long long        NativeTime,
							unsigned long long       *SystemTime )
\brief Translate stream time value to an operating system time value.

This interface is for the use of external sychronization modules. See
\ref time.

\param Playback   Playback identifier
\param NativeTime Native stream time.
\param SystemTime Pointer to a variable to hold the system time.

\return Player status code, PlayerNoError indicates success.
*/

/*! \fn PlayerStatus_t Player_c::RequestTimeNotification(       PlayerStream_t            Stream,
							unsigned long long        NativeTime,
							void                     *EventUserData         = NULL )
\brief Requstion notification when a specificed stream time is manifested.

An event record will be signalled when the playback time is reached
(indicated by the manifestation of the first frame with a playback time 
greater than or equal to the specified time), with the requested time 
being the value parameter.

\param Stream        Stream identifier.
\param NativeTime    Native stream time.
\param EventUserData Optional user pointer to be associated with any event raised.

\return Player status code, PlayerNoError indicates success.
*/

/*! \fn PlayerStatus_t   Player_c::ClockRecoveryInitialize(	PlayerPlayback_t	  Playback,
							PlayerTimeFormat_t	  SourceTimeFormat	= TimeFormatPts )
\brief Initialize clock recovery

\param Playback Playback on which we are recovering a clock
\param SourceTimeFormat the format of source times

\return Player status code, PlayerNoError indicates success.
*/

/*! \fn PlayerStatus_t   Player_c::ClockRecoveryDataPoint(	PlayerPlayback_t	  Playback,
							unsigned long long	  SourceTime,
							unsigned long long	  LocalTime )
\brief Specify a clock recovery data point

A function used to mark a clock recovery data point, specifies a source clock value, 
and a local time (monotonic local time), at which the packet transmitted at source 
time arrived at the box. 


\param Playback Playback on which we are recovering a clock
\param SourceTime The clock value at the source
\param LocalTime Arrival time of the packet containing the source clock value

\return Player status code, PlayerNoError indicates success.
*/

/*! \fn PlayerStatus_t   Player_c::ClockRecoveryEstimate(	PlayerPlayback_t	  Playback,
							unsigned long long	 *SourceTime,
							unsigned long long	 *LocalTime	= NULL )
\brief Request a clock recovery estimate

\param Playback Playback on which we are recovering a clock
\param SourceTime The value of source time (in the appropriate format) when the function is called.
\param LocalTime A copy of the monotonic clock at the time this function was called.

\return Player status code, PlayerNoError indicates success.
*/


//
// Mechanisms for data insertion
//

/*! \fn PlayerStatus_t Player_c::GetInjectBuffer(               Buffer_t                 *Buffer )
\brief Get a buffer instance to fill with data.

The buffer instance will be of a specific type, with no data 
allocation and with an attached meta data structure describing the 
input.

\param Buffer Pointer to a vaiable to hold a the pointer to the Buffer_c instance.
\return Player status code, PlayerNoError indicates success.
*/

/*! \fn PlayerStatus_t Player_c::InjectData(            PlayerPlayback_t          Playback,
							Buffer_t                  Buffer )
\brief Inject data desribed the specified buffer instance.

\param Playback Playback identifier.
\param Buffer   Pointer to a Buffer_c instance.

\return Player status code, PlayerNoError indicates success.
*/

/*! \fn PlayerStatus_t Player_c::InputJump(             PlayerPlayback_t          Playback,
							PlayerStream_t            Stream,
							bool                      SurplusDataInjected,
							bool                      ContinuousReverseJump = false )
\brief Demark discontinuous input data.

Indicates to the player that a jump in the input stream has occurred, 
and informs it whether or not surplus injected data should be 
discarded.

\param Playback Playback identifier.
\param Stream   Stream identifier.
\param SurplusDataInjected True if surplus data was injected.
\param ContinuousReverseJump True if a continuous reverse jump has occured.

\return Player status code, PlayerNoError indicates success.
*/

/*! \fn PlayerStatus_t Player_c::InputGlitch(           PlayerPlayback_t          Playback,
							PlayerStream_t            Stream )
\brief Demark a glitch in input data.

Indicates to the player that a glitch in the input stream has occurred. Usually on the order of a few transport packets.

\param Playback Playback identifier.
\param Stream   Stream identifier.

\return Player status code, PlayerNoError indicates success.
*/

//
// Mechanisms or data extraction 
//

/*! \fn PlayerStatus_t Player_c::RequestDecodeBufferReference(
							PlayerStream_t            Stream,
							unsigned long long        NativeTime            = TIME_NOT_APPLICABLE,
							void                     *EventUserData         = NULL )
\brief Requstion notification when a decode buffer is available.

An event record will be signalled when the decode buffer is available, 
with a playback time greater than or equal to the specified time is 
reached (less than or equal to in reverse play, immediate if time is 
not specified). When event record is sent its value parameter will 
contain a pointer to a Buffer_c instance.

Note: Failure to release a buffer with
Player_c::ReleaseDecodeBufferReference, will leave the captured decode 
buffer unavailable for further decodes.

<b>TODO: Describe possible uses for this interface (snapshot?)</b>

\param Stream        Stream identifier.
\param NativeTime    Native stream time.
\param EventUserData Optional user pointer to be associated with any event raised.

\return Player status code, PlayerNoError indicates success.
*/

/*! \fn PlayerStatus_t Player_c::ReleaseDecodeBufferReference(PlayerEventRecord_t *Record)
\brief Release a buffer previously allocated with Player_c::RequestDecodeBufferReference.

\param Record Pointer to the event record that was generated by a request.

\return Player status code, PlayerNoError indicates success.
*/

//
// Mechanisms for inserting module specific parameters
//

/*! \fn PlayerStatus_t Player_c::SetModuleParameters(   PlayerPlayback_t          Playback,
							PlayerStream_t            Stream,
							PlayerComponent_t         Component,
							bool                      Immediately,
							unsigned int              ParameterBlockSize,
							void                     *ParameterBlock )
\brief Specify a modules parameters.

Note: The parameter block will be used directly for immediate settings, 
but will be copied for non-immediate use, so its lifetime is the same as 
that of the function call.

\param Playback           Playback identifier.
\param Stream             Stream identifier.
\param Component          (Static) identifier of the class modules the parameters are destined for.
\param Immediately        True if the parameters are to be effective immediately (rather
			  than injected into the playback queue and effected inline).
\param ParameterBlockSize Size of the parameter block (in bytes)
\param ParameterBlock     Pointer to the parameter block

\return Player status code, PlayerNoError indicates success.
*/

//
// Support functions for the child classes
//

/*! \fn PlayerStatus_t Player_c::GetBufferManager(              BufferManager_t          *BufferManager )
\brief Obtain a pointer to the buffer manager.

<b>WARNING</b>: This is an internal function and should only be called by player components.

\param BufferManager Pointer to a variable to hold a pointer to the BufferManager_c instance.

\return Player status code, PlayerNoError indicates success.
*/

/*! \fn PlayerStatus_t Player_c::GetClassList(          PlayerStream_t            Stream,
							Collator_t               *Collator,
							FrameParser_t            *FrameParser,
							Codec_t                  *Codec,
							OutputTimer_t            *OutputTimer,
							Manifestor_t             *Manifestor )
\brief Obtain pointers to the player components used by the specified stream.

Note: Only those pointers that are non-NULL will be filled in.

<b>WARNING</b>: This is an internal function and should only be called by player components.

\param Stream      Stream identifier
\param Collator    Pointer to a variable to hold a pointer to the Collator_c instance, or NULL.
\param FrameParser Pointer to a variable to hold a pointer to the FrameParser_c instance, or NULL.
\param Codec       Pointer to a variable to hold a pointer to the Codec_c instance, or NULL.
\param OutputTimer Pointer to a variable to hold a pointer to the OutputTimer_c instance, or NULL.  
\param Manifestor  Pointer to a variable to hold a pointer to the Manifestor_c instance, or NULL.

\return Player status code, PlayerNoError indicates success.
*/

/*! \fn PlayerStatus_t Player_c::GetCodedFrameBufferPool(       PlayerStream_t            Stream,
							BufferPool_t             *Pool = NULL,
							unsigned int             *MaximumCodedFrameSize = NULL )
\brief Obtain a pointer to the coded frame buffer pool used by the specified stream.

<b>WARNING</b>: This is an internal function and should only be called by player components.

\param Stream                Stream identifier.
\param Pool                  Pointer to a variable to store a pointer to the BufferPool_c instance, or NULL.
\param MaximumCodedFrameSize Pointer to a variable to store the maximum coded frame size, or NULL.
\return Player status code, PlayerNoError indicates success.
*/

/*! \fn PlayerStatus_t Player_c::GetDecodeBufferPool(   PlayerStream_t            Stream,
							BufferPool_t             *Pool )
\brief Obtain a pointer to the decode buffer pool used by the specified stream.

\param Stream                Stream identifier.
\param Pool                  Pointer to a variable to store a pointer to the BufferPool_c instance.

\return Player status code, PlayerNoError indicates success.
*/

/*! \fn PlayerStatus_t Player_c::CallInSequence(                PlayerStream_t            Stream,
							PlayerSequenceType_t      SequenceType,
							PlayerSequenceValue_t     SequenceValue,
							PlayerComponentFunction_t Fn,
							... )
\brief Do something magic that makes sense only to that nice Mr. Haydock.

<b>TODO: This method is not adequately documented.</b>

<b>WARNING</b>: This is an internal function and should only be called by player components.

\param Stream           Stream identifier.
\param SequenceType     TODO
\param SequenceValue    TODO
\param Fn               TODO

\return Player status code, PlayerNoError indicates success.
*/

/*! \fn PlayerStatus_t Player_c::GetPlaybackSpeed(              PlayerPlayback_t          Playback,
							Rational_t                       *Speed,
							PlayDirection_t          *Direction )
\brief Get the current playback speed and direction.

<b>WARNING</b>: This is an internal function and should only be called by player components.

\param Playback Playback identifier.
\param Speed     Pointer to a variable to hold the (rational) playback speed.
\param Direction Pointer to a variable to hold the playbak direction.

\return Player status code, PlayerNoError indicates success.
*/

/*! \fn PlayerStatus_t   Player_c::GetPresentationInterval(     PlayerStream_t          Stream,
							unsigned long long       *IntervalStartNormalizedTime,
							unsigned long long       *IntervalEndNormalizedTime );
\brief Retrieve the presentation interval.

<b>WARNING</b>: This is an internal function and should only be called by player components.

<b>NOTE</b>: The player will have translated the interval into normalized time format, also the interval 
may not appear as specified in the call to set, as it may be varied by the player internals when 
switching direction of playback, to avoid manifestation of extraneous frames.

\param Stream  Stream identifier
\param IntervalStartNormalizedTime  Pointer to a variable to hold the start of the range of acceptable times.
\param IntervalEndNormalizedTime  Pointer to a variable to hold the end of the range of acceptable times.

\return Player status code, PlayerNoError indicates success.
*/

/*! \fn unsigned char Player_c::PolicyValue(            PlayerPlayback_t          Playback,
							PlayerStream_t            Stream,
							PlayerPolicy_t            Policy )
\brief Get playback policy for the specified playback and stream.
Takes as parameters a playback identifier, a stream identifier and a policy identifier.

<b>WARNING</b>: This is an internal function and should only be called by player components.

\param Playback Playback identifier
\param Stream   Stream identifier
\param Policy   Policy identifier

\return The policy value. Any policy that has not been set will return the value 0.
*/

/*! \fn PlayerStatus_t Player_c::SignalEvent(           PlayerEventRecord_t      *Record )
\brief Generate a player event.

<b>WARNING</b>: This is an internal function and should only be called by player components.

\param Record Pointer to a player 2 event record to be signalled.

\return Player status code, PlayerNoError indicates success.
*/

/*! \fn PlayerStatus_t Player_c::AttachDemultiplexor(   PlayerStream_t            Stream,
							Demultiplexor_t           Demultiplexor,
							DemultiplexorContext_t    Context )
\brief Inform the player that a particular stream is utilising a demultiplexor.

<b>WARNING</b>: This is an internal function and should only be called by player components.

\param Stream The stream using using the demultiplexor.
\param Demultiplexor The demultiplexor being used.
\param Context The demultiplexor context being used.

\return Player status code, PlayerNoError indicates success.
*/

/*! \fn PlayerStatus_t Player_c::DetachDemultiplexor(   PlayerStream_t            Stream )
\brief Inform the player that a particular stream is no longer utilising a demultiplexor.

<b>WARNING</b>: This is an internal function and should only be called by player components.

\param Stream The stream that was using using the demultiplexor.

\return Player status code, PlayerNoError indicates success.
*/

/*! \fn PlayerStatus_t Player_c::MarkStreamUnPlayable(  PlayerStream_t            Stream )
\brief Inform the player that a particular stream is not playable.

<b>WARNING</b>: This is an internal function and should only be called by player components.

This function should be called by a Frame Parser or Codec if it decides that the current 
stream is unplayable for some reason. The playerr implementation is then responsible for 
curtailing parsing and decode activities, and for informing the user of the state of 
affairs via the appropriate event.

\param Stream The stream that is unplayable.

\return Player status code, PlayerNoError indicates success.
*/

/*! \fn     PlayerStatus_t   CheckForDemuxBufferMismatch(PlayerPlayback_t         Playback,
                                                        PlayerStream_t            Stream )
\brief Supplement an error message with clue to the cause of the error.

<b>WARNING</b>: This is an internal function and should only be called by player components.

This function is called by an output timer, on discovery that data is not being delivered in
time. The function checks if the data is multiplexed, and if it is, checks to see if a mismatch
in input buffering is causing the late delivery on the specified stream.
This is a heuristic calculation, and used as an indicator to help debugging rather than a hard 
and fast piece of code.

\param Playback Playback identifier
\param Stream   Stream identifier

\return Player status code, PlayerNoError indicates success.
*/

/*! \fn     void             RecordNonDecodedFrame(	PlayerStream_t		  Stream,
							Buffer_t		  Buffer,
							ParsedFrameParameters_t	 *ParsedFrameParameters )
\brief Provide a hint to the player, that a frame submitted for decode has been discarded, and will not
come on the outoput ring.

<b>WARNING</b>: This is an internal function and should only be called by player components.

This function is called by a codec, that contains a multi-process processing chain
where a frame may be happily accepted at the input but is discarded at a subsequent portion of the chain.

\param Stream   Stream identifier
\param Buffer   The buffer discarded
\param Stream   a pointer to the parsed frame parameters

\return void
*/

#endif
