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

#ifndef H_PLAYER
#define H_PLAYER

#include "osinline.h"
#include "stm_se.h"
#include "player_types.h"

#include "base_component_class.h"

#include "buffer.h"
#include "port.h"

#include "predefined_metadata_types.h"
#include "owner_identifiers.h"

#include "collator.h"
#include "frame_parser.h"
#include "codec.h"
#include "manifestor.h"
#include "output_coordinator.h"
#include "output_timer.h"
#include "decode_buffer_manager.h"
#include "manifestation_coordinator.h"

#define MAXIMUM_SPEED_SUPPORTED 32
#define FRACTIONAL_MINIMUM_SPEED_SUPPORTED 100000

// A macro to disallow the copy constructor and operator= functions
// This should be used in the private: declarations for a class
#ifndef DISALLOW_COPY_AND_ASSIGN
#define DISALLOW_COPY_AND_ASSIGN(TypeName) \
  TypeName(const TypeName&);               \
  void operator=(const TypeName&)
#endif

class Player_c
{
public:
    //
    // Globally visible data items
    //

    PlayerStatus_t  InitializationStatus;

    BufferType_t    MetaDataInputDescriptorType;
    BufferType_t    MetaDataCodedFrameParametersType;
    BufferType_t    MetaDataStartCodeListType;
    BufferType_t    MetaDataParsedFrameParametersType;
    BufferType_t    MetaDataParsedFrameParametersReferenceType;
    BufferType_t    MetaDataParsedVideoParametersType;
    BufferType_t    MetaDataParsedAudioParametersType;
    BufferType_t    MetaDataOutputTimingType;

    BufferType_t    BufferPlayerControlStructureType;
    BufferType_t    MetaDataSequenceNumberType;     //made available so WMA can make it's private CodedFrameBufferPool properly
    BufferType_t    MetaDataUserDataType;

    Player_c(void)
        : InitializationStatus(PlayerNoError)
        , MetaDataInputDescriptorType(0)
        , MetaDataCodedFrameParametersType(0)
        , MetaDataStartCodeListType(0)
        , MetaDataParsedFrameParametersType(0)
        , MetaDataParsedFrameParametersReferenceType(0)
        , MetaDataParsedVideoParametersType(0)
        , MetaDataParsedAudioParametersType(0)
        , MetaDataOutputTimingType(0)
        , BufferPlayerControlStructureType(0)
        , MetaDataSequenceNumberType(0)
        , MetaDataUserDataType(0)
    {}

    virtual ~Player_c(void) {}

    //
    // Mechanisms for registering global items
    //

    virtual PlayerStatus_t   RegisterBufferManager(BufferManager_t     BufferManager) = 0;

    //
    // Mechanisms relating to event retrieval
    //

    virtual PlayerStatus_t   SpecifySignalledEvents(PlayerPlayback_t   Playback,
                                                    PlayerStream_t     Stream,
                                                    PlayerEventMask_t  Events,
                                                    void              *UserData              = NULL) = 0;

    //
    // Mechanisms for policy management
    //

    virtual PlayerStatus_t   SetPolicy(PlayerPlayback_t          Playback,
                                       PlayerStream_t            Stream,
                                       PlayerPolicy_t            Policy,
                                       int                       PolicyValue           = PolicyValueApply) = 0;

    //
    // Mechanisms for managing playbacks
    //

    virtual PlayerStatus_t   CreatePlayback(OutputCoordinator_t       OutputCoordinator,
                                            PlayerPlayback_t         *Playback,
                                            bool                      SignalEvent           = false,
                                            void                     *EventUserData         = NULL) = 0;

    virtual PlayerStatus_t   TerminatePlayback(PlayerPlayback_t       Playback,
                                               bool                   SignalEvent           = false,
                                               void                  *EventUserData         = NULL) = 0;

    virtual PlayerStatus_t   AddStream(PlayerPlayback_t               Playback,
                                       PlayerStream_t                *Stream,
                                       PlayerStreamType_t             StreamType,
                                       unsigned int                   InstanceId,
                                       Collator_t                     Collator,
                                       FrameParser_t                  FrameParser,
                                       Codec_t                        Codec,
                                       OutputTimer_t                  OutputTimer,
                                       DecodeBufferManager_t          DecodeBufferManager,
                                       ManifestationCoordinator_t     ManifestationCoordinator,
                                       HavanaStream_t                 HavanaStream,
                                       UserDataSource_t               UserDataSender,
                                       bool                           SignalEvent           = false,
                                       void                          *EventUserData         = NULL) = 0;

    virtual PlayerStatus_t   RemoveStream(PlayerStream_t              Stream,
                                          bool                        SignalEvent           = false,
                                          void                       *EventUserData         = NULL) = 0;

    //
    // Mechanisms for managing time
    //

    virtual PlayerStatus_t   SetPresentationInterval(PlayerPlayback_t        Playback,
                                                     PlayerStream_t          Stream            = PlayerAllStreams,
                                                     unsigned long long      IntervalStartNativeTime       = INVALID_TIME,
                                                     unsigned long long      IntervalEndNativeTime         = INVALID_TIME) = 0;

    virtual PlayerStatus_t   SetNativePlaybackTime(PlayerPlayback_t          Playback,
                                                   unsigned long long        NativeTime,
                                                   unsigned long long        SystemTime                    = INVALID_TIME) = 0;

    virtual PlayerStatus_t   RetrieveNativePlaybackTime(PlayerPlayback_t     Playback,
                                                        unsigned long long  *NativeTime,
                                                        PlayerTimeFormat_t    NativeTimeFormat  = TimeFormatPts) = 0;

    virtual PlayerStatus_t   TranslateNativePlaybackTime(PlayerPlayback_t    Playback,
                                                         unsigned long long  NativeTime,
                                                         unsigned long long *SystemTime) = 0;

    //
    // Clock recovery support - for playbacks with system clock as master
    //

    virtual PlayerStatus_t   ClockRecoveryInitialize(PlayerPlayback_t     Playback,
                                                     PlayerTimeFormat_t    SourceTimeFormat  = TimeFormatPts) = 0;


    virtual PlayerStatus_t   ClockRecoveryDataPoint(PlayerPlayback_t      Playback,
                                                    unsigned long long    SourceTime,
                                                    unsigned long long    LocalTime) = 0;

    virtual PlayerStatus_t   ClockRecoveryEstimate(PlayerPlayback_t       Playback,
                                                   unsigned long long    *SourceTime,
                                                   unsigned long long    *LocalTime = NULL) = 0;

    //
    // Mechanisms for data insertion
    //

    virtual PlayerStatus_t   GetInjectBuffer(Buffer_t           *Buffer) = 0;

    virtual PlayerStatus_t   InjectData(PlayerPlayback_t         Playback,
                                        Buffer_t                 Buffer) = 0;

    //
    // Mechanisms for inserting module specific parameters
    //

    virtual PlayerStatus_t   SetModuleParameters(PlayerPlayback_t  Playback,
                                                 PlayerStream_t    Stream,
                                                 unsigned int      ParameterBlockSize,
                                                 void             *ParameterBlock) = 0;
    //
    // Support functions for the child classes
    //

    virtual PlayerStatus_t   GetBufferManager(BufferManager_t        *BufferManager) = 0;

    virtual PlayerStatus_t   GetClassList(PlayerStream_t              Stream,
                                          Collator_t                 *Collator,
                                          FrameParser_t              *FrameParser,
                                          Codec_t                    *Codec,
                                          OutputTimer_t              *OutputTimer,
                                          ManifestationCoordinator_t *ManifestationCoordinator) = 0;

    virtual PlayerStatus_t   GetDecodeBufferPool(PlayerStream_t        Stream,
                                                 BufferPool_t         *Pool) = 0;

    virtual BufferPool_t     GetControlStructurePool() = 0;

    virtual PlayerStatus_t   GetPlaybackSpeed(PlayerPlayback_t         Playback,
                                              Rational_t              *Speed,
                                              PlayDirection_t         *Direction) = 0;

    virtual PlayerStatus_t   GetPresentationInterval(PlayerStream_t           Stream,
                                                     unsigned long long       *IntervalStartNormalizedTime,
                                                     unsigned long long       *IntervalEndNormalizedTime) = 0;

    virtual int              PolicyValue(PlayerPlayback_t          Playback,
                                         PlayerStream_t            Stream,
                                         PlayerPolicy_t            Policy) = 0;

    virtual unsigned long long   GetLastNativeTime(PlayerPlayback_t   Playback) = 0;
    virtual void             SetLastNativeTime(PlayerPlayback_t       Playback,
                                               unsigned long long     Time) = 0;

    //
    // Low power functions
    //

    virtual PlayerStatus_t   LowPowerEnter(__stm_se_low_power_mode_t   low_power_mode) = 0;
    virtual PlayerStatus_t   LowPowerExit(void) = 0;
    virtual PlayerStatus_t   PlaybackLowPowerEnter(PlayerPlayback_t    Playback) = 0;
    virtual PlayerStatus_t   PlaybackLowPowerExit(PlayerPlayback_t     Playback) = 0;
    virtual __stm_se_low_power_mode_t   GetLowPowerMode(void) = 0;

    virtual PlayerPlaybackStatistics_t  GetStatistics(PlayerPlayback_t    Playback) = 0;
    virtual void                        ResetStatistics(PlayerPlayback_t  Playback) = 0;

    virtual PlayerStatus_t      SetControl(PlayerPlayback_t   Playback,
                                           PlayerStream_t     Stream,
                                           stm_se_ctrl_t      Ctrl,
                                           const void        *Value) = 0;

    virtual PlayerStatus_t      GetControl(PlayerPlayback_t   Playback,
                                           PlayerStream_t     Stream,
                                           stm_se_ctrl_t      Ctrl,
                                           void              *Value) = 0;
#ifdef LOWMEMORYBANDWIDTH
    virtual OS_Status_t getSemMemBWLimiter(unsigned int timeout) = 0;
    virtual void releaseSemMemBWLimiter() = 0;
#endif
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
\brief Status flag indicating how the initialization of the class went.

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

/*! \var BufferType_t Player_c::MetaDataOutputTimingType
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
\param Events   Mask indicating which occurrences should raise an event record.
\param UserData Optional user pointer to be associated with any event raised.

\return Player status code, PlayerNoError indicates success.
*/

//
// Mechanisms for policy management
//

/*! \fn PlayerStatus_t Player_c::SetPolicy(                     PlayerPlayback_t          Playback,
                            PlayerStream_t            Stream,
                            PlayerPolicy_t            Policy,
                            int                       PolicyValue           = PolicyValueApply )
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
                            unsigned int              InstanceId,
                            Collator_t                Collator,
                            FrameParser_t             FrameParser,
                            Codec_t                   Codec,
                            OutputTimer_t             OutputTimer,
                            DecodeBufferManager_t     DecodeBufferManager,
                            ManifestationCoordinator_t   ManifestationCoordinator,
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
\param DecodeBufferManager   Pointer to a DecodeBufferManager_c instance.
\param ManifestationCoordinator    Pointer to a ManifestorCoordinator_c instance.
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



//
// Mechanisms for managing time
//

/*! \fn PlayerStatus_t   Player_c::SetPresentationInterval(     PlayerPlayback_t          Playback,
                            PlayerStream_t        Stream            = PlayerAllStreams,
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

/*! \fn PlayerStatus_t Player_c::SetNativePlaybackTime( PlayerPlayback_t          Playback,
                            unsigned long long        NativeTime,
                            unsigned long long        SystemTime    = INVALID_TIME )
\brief Inform the synchronizer of the current native playback time.

This interface is for the use of external synchronization modules. See
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
                            unsigned long long       *NativeTime ,
                PlayerTimeFormat_t    NativeTimeFormat  = TimeFormatPts)
\brief Ask the synchronizer for the current native playback time.

This interface is for the use of external synchronization modules. See
\ref time.

\param Playback   Playback identifier
\param NativeTime Pointer to a variable to hold the native time.
\param NativeTimeFormat The format of NativeTime

\return Player status code, PlayerNoError indicates success.
*/

/*! \fn PlayerStatus_t Player_c::TranslateNativePlaybackTime(PlayerPlayback_t     Playback,
                            unsigned long long        NativeTime,
                            unsigned long long       *SystemTime )
\brief Translate stream time value to an operating system time value.

This interface is for the use of external synchronization modules. See
\ref time.

\param Playback   Playback identifier
\param NativeTime Native stream time.
\param SystemTime Pointer to a variable to hold the system time.

\return Player status code, PlayerNoError indicates success.
*/

/*! \fn PlayerStatus_t   Player_c::ClockRecoveryInitialize( PlayerPlayback_t      Playback,
                            PlayerTimeFormat_t    SourceTimeFormat  = TimeFormatPts )
\brief Initialize clock recovery

\param Playback Playback on which we are recovering a clock
\param SourceTimeFormat the format of source times

\return Player status code, PlayerNoError indicates success.
*/

/*! \fn PlayerStatus_t   Player_c::ClockRecoveryDataPoint(  PlayerPlayback_t      Playback,
                            unsigned long long    SourceTime,
                            unsigned long long    LocalTime )
\brief Specify a clock recovery data point

A function used to mark a clock recovery data point, specifies a source clock value,
and a local time (monotonic local time), at which the packet transmitted at source
time arrived at the box.


\param Playback Playback on which we are recovering a clock
\param SourceTime The clock value at the source
\param LocalTime Arrival time of the packet containing the source clock value

\return Player status code, PlayerNoError indicates success.
*/

/*! \fn PlayerStatus_t   Player_c::ClockRecoveryEstimate(   PlayerPlayback_t      Playback,
                            unsigned long long   *SourceTime,
                            unsigned long long   *LocalTime = NULL )
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

\param Buffer Pointer to a variable to hold a the pointer to the Buffer_c instance.
\return Player status code, PlayerNoError indicates success.
*/

/*! \fn PlayerStatus_t Player_c::InjectData(            PlayerPlayback_t          Playback,
                            Buffer_t                  Buffer )
\brief Inject data described the specified buffer instance.

\param Playback Playback identifier.
\param Buffer   Pointer to a Buffer_c instance.

\return Player status code, PlayerNoError indicates success.
*/

//
// Mechanisms for inserting module specific parameters
//

/*! \fn PlayerStatus_t Player_c::SetModuleParameters( PlayerPlayback_t          Playback,
                            PlayerStream_t            Stream,
                            unsigned int              ParameterBlockSize,
                            void                     *ParameterBlock )
\brief Specify a modules parameters.

Note: The parameter block will be used directly for immediate settings,
but will be copied for non-immediate use, so its lifetime is the same as
that of the function call.

\param Playback           Playback identifier.
\param Stream             Stream identifier.
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
                            ManifestationCoordinator_t  *ManifestationCoordinator )
\brief Obtain pointers to the player components used by the specified stream.

Note: Only those pointers that are non-NULL will be filled in.

<b>WARNING</b>: This is an internal function and should only be called by player components.

\param Stream      Stream identifier
\param Collator    Pointer to a variable to hold a pointer to the Collator_c instance, or NULL.
\param FrameParser Pointer to a variable to hold a pointer to the FrameParser_c instance, or NULL.
\param Codec       Pointer to a variable to hold a pointer to the Codec_c instance, or NULL.
\param OutputTimer Pointer to a variable to hold a pointer to the OutputTimer_c instance, or NULL.
\param ManifestationCoordinator  Pointer to a variable to hold a pointer to the ManifestationCoordinator_c instance, or NULL.

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
\param Direction Pointer to a variable to hold the playback direction.

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

/*! \fn int Player_c::PolicyValue(            PlayerPlayback_t          Playback,
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

#endif
