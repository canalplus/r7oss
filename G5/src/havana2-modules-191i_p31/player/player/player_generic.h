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

Source file name : player_generic.h
Author :           Nick

Definition of the player generic class implementationfor player 2.


Date        Modification                                    Name
----        ------------                                    --------
03-Nov-06   Created                                         Nick

************************************************************************/

#ifndef H_PLAYER_GENERIC
#define H_PLAYER_GENERIC

// /////////////////////////////////////////////////////////////////////
//
//	Include any component headers

#include "player.h"
#include "allocinline.h"

// /////////////////////////////////////////////////////////////////////////
//
// Locally defined constants
//

#define PLAYER_MAX_DEMULTIPLEXORS			4
#define PLAYER_MAX_OUTSTANDING_EVENTS			32
#define PLAYER_MAX_EVENT_SIGNALS			4
#define PLAYER_MAX_CONTROL_STRUCTURE_BUFFERS		512 /* 32 */
#define PLAYER_MAX_INPUT_BUFFERS			16

#ifdef CONFIG_32BIT
#define PLAYER_MAX_DECODE_BUFFERS			64
#else
#define PLAYER_MAX_DECODE_BUFFERS			32
#endif

#define PLAYER_MAX_INLINE_PARAMETER_BLOCK_SIZE		64
#define PLAYER_MAX_DISCARDED_FRAMES			64

#define PLAYER_MAX_RING_SIZE				1024
#define PLAYER_LIMIT_ON_OUT_OF_ORDER_DECODES		13	// Limit on the number of out of order decodes, this is applied
								// because some streams (H264) limit the ref frame count, and the standard
								// calculations can then leave us with a potential for deadlock I have set 
								// it to 16 - the number of frames that can be in the manifestor

#define PLAYER_MAX_CTOP_MESSAGES			4
#define PLAYER_MAX_PTOD_MESSAGES			4
#define PLAYER_MAX_DTOM_MESSAGES			4
#define PLAYER_MAX_POSTM_MESSAGES			4

#define PLAYER_MAX_EVENT_WAIT				50	// Ms
#define PLAYER_NEXT_FRAME_EVENT_WAIT			20	// Ms (lower because it will also scan for state changes)
#define PLAYER_DEFAULT_FRAME_TIME			100	// Ms
#define PLAYER_MAX_DISCARD_DRAIN_TIME			1000	// Ms
#define PLAYER_MAX_MARKER_TIME_THROUGH_CODEC		4000	// Ms (Quite loose this one)
#define PLAYER_MAX_TIME_ON_DISPLAY			16000	// Ms (time for a single frame)
#define PLAYER_MAX_PLAYOUT_TIME				60000	// Ms (I am allowing a minute here for slideshows )

#define PLAYER_MAX_TIME_IN_RETIMING			100000	// us
#define PLAYER_RETIMING_WAIT				5	// ms

#define PLAYER_AUDIO_DEFAULT_CODED_FRAME_COUNT		256	// These default values are derived from the maximum existing values in the specific codecs
#define PLAYER_AUDIO_DEFAULT_CODED_MEMORY_SIZE		PLAYER2_AUDIO_DEFAULT_CODED_MEMORY_SIZE /* 0x400000 */
#define PLAYER_AUDIO_DEFAULT_CODED_FRAME_MAXIMUM_SIZE	0x40000
#define PLAYER_AUDIO_DEFAULT_CODED_FRAME_PARTITION_NAME	"BPA2_Region0"

#define PLAYER_VIDEO_DEFAULT_CODED_FRAME_COUNT		1024
#define PLAYER_VIDEO_DEFAULT_CODED_MEMORY_SIZE		PLAYER2_VIDEO_DEFAULT_CODED_MEMORY_SIZE /* 0x1000000 */
#define PLAYER_VIDEO_DEFAULT_CODED_FRAME_MAXIMUM_SIZE	0x100000
#define PLAYER_VIDEO_DEFAULT_CODED_FRAME_PARTITION_NAME	"BPA2_Region0"

#define PLAYER_OTHER_DEFAULT_CODED_FRAME_COUNT		1024	// Wild stab in the dark until we see some
#define PLAYER_OTHER_DEFAULT_CODED_MEMORY_SIZE		0x10000
#define PLAYER_OTHER_DEFAULT_CODED_FRAME_MAXIMUM_SIZE	0x1000
#define PLAYER_OTHER_DEFAULT_CODED_FRAME_PARTITION_NAME	"BPA2_Region0"

// 

#define BASE_EXTENSIONS		0x10000

enum
{
    OSFnSetEventOnManifestation		= BASE_EXTENSIONS,
    OSFnSetEventOnPostManifestation,

    PlayerFnSwitchCollator,
    PlayerFnSwitchFrameParser,
    PlayerFnSwitchCodec,
    PlayerFnSwitchOutputTimer,
    PlayerFnSwitchComplete
};     

// /////////////////////////////////////////////////////////////////////////
//
// The definition of the player parameter block (set module parameters calls)
//

typedef enum
{
    PlayerSetCodedFrameBufferParameters		= 0
} PlayerParameterBlockType_t;

//

typedef struct PlayerSetCodedFrameBufferParameters_s
{
    PlayerStreamType_t		  StreamType;
    unsigned int		  CodedFrameCount;
    unsigned int		  CodedMemorySize;
    unsigned int		  CodedFrameMaximumSize;
    char			  CodedMemoryPartitionName[ALLOCATOR_MAX_PARTITION_NAME_SIZE];
} PlayerSetCodedFrameBufferParameters_t;

//

typedef struct PlayerParameterBlock_s
{
    PlayerParameterBlockType_t			ParameterType;

    union
    {
        PlayerSetCodedFrameBufferParameters_t	CodedFrame;
    };
} PlayerParameterBlock_t;


// /////////////////////////////////////////////////////////////////////////
//
// The player control structure buffer type and associated structures
//

typedef enum 
{
    ActionInSequenceCall	= 0
} PlayerControlAction_t;

//

typedef struct PlayerInSequenceParams_s
{
    PlayerComponentFunction_t	  Fn;
    unsigned int		  UnsignedInt;
    void			 *Pointer;
    unsigned char		  Block[PLAYER_MAX_INLINE_PARAMETER_BLOCK_SIZE];
    PlayerEventRecord_t		  Event;

} PlayerInSequenceParams_t;

//

typedef struct PlayerControlStructure_s
{
    PlayerControlAction_t	  Action;
    PlayerSequenceType_t	  SequenceType;
    PlayerSequenceValue_t	  SequenceValue;

    PlayerInSequenceParams_t	  InSequence;

} PlayerControlStructure_t;

//

#define BUFFER_PLAYER_CONTROL_STRUCTURE		"PlayerControlStructure"
#define BUFFER_PLAYER_CONTROL_STRUCTURE_TYPE   {BUFFER_PLAYER_CONTROL_STRUCTURE, BufferDataTypeBase, AllocateFromOSMemory, 4, 0, true, true, sizeof(PlayerControlStructure_t)}


// /////////////////////////////////////////////////////////////////////////
//
// The input buffer type definition (no-allocation)
//

#define BUFFER_INPUT				"InputBuffer"
#define BUFFER_INPUT_TYPE   			{BUFFER_INPUT, BufferDataTypeBase, NoAllocation, 1, 0, false, false, 0}

#define BUFFER_CODED_FRAME_BUFFER		"CodedFrameBuffer"
#define BUFFER_CODED_FRAME_BUFFER_TYPE		{BUFFER_CODED_FRAME_BUFFER, BufferDataTypeBase, AllocateFromSuppliedBlock, 4, 64, false, false, 0}

// /////////////////////////////////////////////////////////////////////////
//
// The Buffer sequence number meta data type, allowing the 
// attachment of a sequence number to all coded buffers.
//

typedef struct PlayerSequenceNumber_s
{
    bool			MarkerFrame;
    unsigned long long		Value;

    // Statistical values for each buffer

    unsigned long long		TimeEntryInProcess0;
    unsigned long long		DeltaEntryInProcess0;
    unsigned long long		TimeEntryInProcess1;
    unsigned long long		DeltaEntryInProcess1;
    unsigned long long		TimeEntryInProcess2;
    unsigned long long		DeltaEntryInProcess2;
    unsigned long long		TimeEntryInProcess3;
    unsigned long long		DeltaEntryInProcess3;

    unsigned long long		TimePassToCodec;
    unsigned long long		TimePassToManifestor;
} PlayerSequenceNumber_t;

#define METADATA_SEQUENCE_NUMBER		"SequenceNumber"
#define METADATA_SEQUENCE_NUMBER_TYPE		{METADATA_SEQUENCE_NUMBER, MetaDataTypeBase, AllocateFromOSMemory, 4, 0, true, false, sizeof(PlayerSequenceNumber_t)}

// /////////////////////////////////////////////////////////////////////////
//
// Locally defined structures
//

    // ---------------------------------------------------------
    //	The policy state record
    //

typedef enum
{
    PolicyPlayoutAlwaysPlayout	= PolicyMaxPolicy,
    PolicyPlayoutAlwaysDiscard,

    PolicyStatisticsOnAudio,
    PolicyStatisticsOnVideo,

    PolicyMaxExtraPolicy

} PlayerExtraPolicies_t;


#define POLICY_WORDS				(((unsigned int)PolicyMaxExtraPolicy + 31)/32)

typedef struct PlayerPolicyState_s
{
    unsigned int	  Specified[POLICY_WORDS];
    unsigned char	  Value[PolicyMaxExtraPolicy];
} PlayerPolicyState_t;


    // ---------------------------------------------------------
    //	The accumulated buffer table type (held during re-ordering after decode)
    //

typedef struct PlayerBufferRecord_s
{
    Buffer_t		  		 Buffer;
    unsigned long long			 SequenceNumber;

    bool				 ReleasedBuffer;

    union
    {
	ParsedFrameParameters_t		*ParsedFrameParameters;
	PlayerControlStructure_t	*ControlStructure;
	unsigned int			 DisplayFrameIndex;
    };
} PlayerBufferRecord_t;


    // ---------------------------------------------------------
    //	The statistical data 
    //

typedef struct StatisticFields_s
{
    unsigned int			 Count;
    unsigned long long 			 Total;
    unsigned long long 			 Longest;
    unsigned long long 			 Shortest;
} StatisticFields_t;

//

typedef struct PlayerStreamStatistics_s
{
    unsigned int			 Count;
    StatisticFields_t			 DeltaEntryIntoProcess0;
    StatisticFields_t			 DeltaEntryIntoProcess1;
    StatisticFields_t			 DeltaEntryIntoProcess2;
    StatisticFields_t			 DeltaEntryIntoProcess3;

    StatisticFields_t			 Traverse0To1;
    StatisticFields_t			 Traverse1To2;
    StatisticFields_t			 Traverse2To3;

    StatisticFields_t			 FrameTimeInProcess1;
    StatisticFields_t			 FrameTimeInProcess2;
    StatisticFields_t			 TotalTraversalTime;

} PlayerStreamStatistics_t;


    // ---------------------------------------------------------
    //	The stream structure
    //

typedef class Player_Generic_c	*Player_Generic_t;

struct PlayerStream_s
{
    PlayerStream_t		  Next;
    Player_Generic_t		  Player;
    PlayerPlayback_t		  Playback;

    PlayerStreamType_t		  StreamType;
    Collator_t			  Collator;
    FrameParser_t		  FrameParser;
    Codec_t			  Codec;
    OutputTimer_t		  OutputTimer;
    Manifestor_t		  Manifestor;

    Demultiplexor_t		  Demultiplexor;
    DemultiplexorContext_t	  DemultiplexorContext;

    bool			  UnPlayable;

    bool			  Terminating;
    unsigned int		  ProcessRunningCount;
    unsigned int		  ExpectedProcessCount;
    OS_Event_t			  StartStopEvent;

    OS_Event_t			  Drained;
    bool			  BuffersComingOutOfManifestation;

    bool			  CodecReset;
    bool			  SwitchStreamInProgress;
    OS_Event_t			  SwitchStreamLastOneOutOfTheCodec;
    Collator_t			  SwitchingToCollator;
    FrameParser_t		  SwitchingToFrameParser;
    Codec_t			  SwitchingToCodec;
    OutputTimer_t		  SwitchingToOutputTimer;

    bool			  Step;
    OS_Event_t			  SingleStepMayHaveHappened;

    Ring_t			  CollatedFrameRing;
    Ring_t			  ParsedFrameRing;
    Ring_t			  DecodedFrameRing;
    Ring_t			  ManifestedBufferRing;

    unsigned int		  CodedFrameCount;
    unsigned int		  CodedMemorySize;
    unsigned int		  CodedFrameMaximumSize;
    char			  CodedMemoryPartitionName[ALLOCATOR_MAX_PARTITION_NAME_SIZE];

    allocator_device_t		  CodedFrameMemoryDevice;
    void			 *CodedFrameMemory[3];
    BufferType_t		  CodedFrameBufferType;
    BufferType_t		  TranscodedFrameBufferType;
    BufferPool_t		  CodedFrameBufferPool;

    BufferType_t		  DecodeBufferType;
    BufferPool_t		  DecodeBufferPool;
    BufferPool_t		  PostProcessControlBufferPool;

    unsigned int		  NumberOfDecodeBuffers;

    unsigned int		  MarkerInCodedFrameIndex;
    unsigned long long		  NextBufferSequenceNumber;

    bool			  ReTimeQueuedFrames;
    unsigned long long		  ReTimeStart;

    PlayerPolicyState_t		  PolicyRecord;

    //
    // Presentation interval values
    //

    unsigned long long		  RequestedPresentationIntervalStartNormalizedTime;
    unsigned long long		  RequestedPresentationIntervalEndNormalizedTime;

    //
    // Process specific drain flags
    //

    unsigned long long		  DrainSequenceNumber;

    bool			  DiscardingUntilMarkerFrameCtoP;
    bool			  DiscardingUntilMarkerFramePtoD;
    bool			  DiscardingUntilMarkerFrameDtoM;
    bool			  DiscardingUntilMarkerFramePostM;

    //
    // Accumulated list of coded data buffers that were not decoded,
    // passed from parse->decode to decode->manifest, used to patch
    // holes in the display frame indices (avoids the latency of waiting 
    // for max decodes out of order before correcting for missing indices).
    //

    unsigned int		  InsertionsIntoNonDecodedBuffers;
    unsigned int		  RemovalsFromNonDecodedBuffers;
    unsigned int		  DisplayIndicesCollapse;
    PlayerBufferRecord_t	  NonDecodedBuffers[PLAYER_MAX_DISCARDED_FRAMES];

    //
    // Accumulated decode buffers in decoder to manifestor process
    //

    PlayerBufferRecord_t	  AccumulatedDecodeBufferTable[PLAYER_MAX_DECODE_BUFFERS];

    //
    // Accumulated messages in each process
    //

    PlayerBufferRecord_t	  AccumulatedBeforeCtoPControlMessages[PLAYER_MAX_CTOP_MESSAGES];
    PlayerBufferRecord_t	  AccumulatedAfterCtoPControlMessages[PLAYER_MAX_CTOP_MESSAGES];

    PlayerBufferRecord_t	  AccumulatedBeforePtoDControlMessages[PLAYER_MAX_PTOD_MESSAGES];
    PlayerBufferRecord_t	  AccumulatedAfterPtoDControlMessages[PLAYER_MAX_PTOD_MESSAGES];

    PlayerBufferRecord_t	  AccumulatedBeforeDtoMControlMessages[PLAYER_MAX_DTOM_MESSAGES];
    PlayerBufferRecord_t	  AccumulatedAfterDtoMControlMessages[PLAYER_MAX_DTOM_MESSAGES];

    PlayerBufferRecord_t	  AccumulatedBeforePostMControlMessages[PLAYER_MAX_POSTM_MESSAGES];
    PlayerBufferRecord_t	  AccumulatedAfterPostMControlMessages[PLAYER_MAX_POSTM_MESSAGES];

    //
    // The statistics data
    //

    PlayerStreamStatistics_t	  Statistics;

    //
    // Useful counts/debugging data (added/removed at will
    //

    unsigned int		  FramesToManifestorCount;
    unsigned int		  FramesFromManifestorCount;

};

    // ---------------------------------------------------------
    //	The playback structure
    //

struct PlayerPlayback_s
{
    PlayerPlayback_t		  Next;

    OutputCoordinator_t		  OutputCoordinator;
    PlayerStream_t		  ListOfStreams;

    Rational_t			  Speed;
    PlayDirection_t		  Direction;

    unsigned long long		  PresentationIntervalReversalLimitStartNormalizedTime;
    unsigned long long		  PresentationIntervalReversalLimitEndNormalizedTime;
    unsigned long long		  RequestedPresentationIntervalStartNormalizedTime;
    unsigned long long		  RequestedPresentationIntervalEndNormalizedTime;

    PlayerPolicyState_t		  PolicyRecord;

    unsigned int		  AudioCodedFrameCount;			// One set of these will be used whenever a stream is added
    unsigned int		  AudioCodedMemorySize;
    unsigned int		  AudioCodedFrameMaximumSize;
    char			  AudioCodedMemoryPartitionName[ALLOCATOR_MAX_PARTITION_NAME_SIZE];
    unsigned int		  VideoCodedFrameCount;
    unsigned int		  VideoCodedMemorySize;
    unsigned int		  VideoCodedFrameMaximumSize;
    char			  VideoCodedMemoryPartitionName[ALLOCATOR_MAX_PARTITION_NAME_SIZE];
    unsigned int		  OtherCodedFrameCount;
    unsigned int		  OtherCodedMemorySize;
    unsigned int		  OtherCodedFrameMaximumSize;
    char			  OtherCodedMemoryPartitionName[ALLOCATOR_MAX_PARTITION_NAME_SIZE];

    unsigned long long 		  LastNativeTime;
};


    // ---------------------------------------------------------
    //	Structures to allow me to have a list of events, and a list of signals
    //

typedef struct EventListEntry_s
{
    unsigned int	NextIndex;
    PlayerEventRecord_t	Record;
} EventListEntry_t;

//

typedef struct EventSignalEntry_s
{
    PlayerPlayback_t	  Playback;
    PlayerStream_t	  Stream;
    PlayerEventMask_t	  Events;
    OS_Event_t		 *Signal;
} EventSignalEntry_t;


// /////////////////////////////////////////////////////////////////////////
//
// The C task entry stubs
//

extern "C" {
OS_TaskEntry(PlayerProcessCollateToParse);
OS_TaskEntry(PlayerProcessParseToDecode);
OS_TaskEntry(PlayerProcessDecodeToManifest);
OS_TaskEntry(PlayerProcessPostManifest);
}

// /////////////////////////////////////////////////////////////////////////
//
// The class definition
//

class Player_Generic_c : public Player_c
{
private:

    // Data

    OS_Mutex_t		  Lock;
    bool		  ShutdownPlayer;

    BufferManager_t	  BufferManager;
    BufferType_t	  BufferPlayerControlStructureType;
    BufferType_t	  BufferInputBufferType;
    BufferType_t	  BufferCodedFrameBufferType;
//    MetaDataType_t	  MetaDataSequenceNumberType;	//made public so WMA can make it's private CodedFrameBufferPool properly

    BufferPool_t	  PlayerControlStructurePool;
    BufferPool_t	  InputBufferPool;

    unsigned int	  DemultiplexorCount;
    Demultiplexor_t	  Demultiplexors[PLAYER_MAX_DEMULTIPLEXORS];

    PlayerPlayback_t	  ListOfPlaybacks;

    PlayerPolicyState_t	  PolicyRecord;

    OS_Event_t		  InternalEventSignal;
    unsigned int	  EventListHead;
    unsigned int	  EventListTail;
    EventListEntry_t	  EventList[PLAYER_MAX_OUTSTANDING_EVENTS];
    EventSignalEntry_t	  ExternalEventSignals[PLAYER_MAX_EVENT_SIGNALS];

    unsigned int	  AudioCodedFrameCount;			// One set of these will be used whenever a stream is added
    unsigned int	  AudioCodedMemorySize;
    unsigned int	  AudioCodedFrameMaximumSize;
    char		  AudioCodedMemoryPartitionName[ALLOCATOR_MAX_PARTITION_NAME_SIZE];
    unsigned int	  VideoCodedFrameCount;
    unsigned int	  VideoCodedMemorySize;
    unsigned int	  VideoCodedFrameMaximumSize;
    char		  VideoCodedMemoryPartitionName[ALLOCATOR_MAX_PARTITION_NAME_SIZE];
    unsigned int	  OtherCodedFrameCount;
    unsigned int	  OtherCodedMemorySize;
    unsigned int	  OtherCodedFrameMaximumSize;
    char		  OtherCodedMemoryPartitionName[ALLOCATOR_MAX_PARTITION_NAME_SIZE];

    // Functions

    PlayerStatus_t   CleanUpAfterStream(  	PlayerStream_t			  Stream );

    PlayerStatus_t   PerformInSequenceCall(	PlayerStream_t			  Stream,
						PlayerControlStructure_t	 *ControlStructure );

    PlayerStatus_t   AccumulateControlMessage(	Buffer_t			  Buffer,
						PlayerControlStructure_t	 *Message,
						unsigned int			 *MessageCount,
						unsigned int			  MessageTableSize,
						PlayerBufferRecord_t		 *MessageTable );

    PlayerStatus_t   ProcessControlMessage(	PlayerStream_t			  Stream,
						Buffer_t			  Buffer,
						PlayerControlStructure_t	 *Message );

    PlayerStatus_t   ProcessAccumulatedControlMessages(
						PlayerStream_t			  Stream,
						unsigned int			 *MessageCount,
						unsigned int			  MessageTableSize,
						PlayerBufferRecord_t		 *MessageTable,
						unsigned long long		  SequenceNumber,
						unsigned long long		  Time );

    PlayerStatus_t   InternalDrainPlayback(	PlayerPlayback_t	  Playback,
						PlayerPolicy_t		  PlayoutPolicy,
						bool			  ParseAllFrames );

    PlayerStatus_t   InternalDrainStream(	PlayerStream_t	 	  Stream,
						bool			  NonBlocking,
						bool			  SignalEvent,
						void			 *EventUserData,
						PlayerPolicy_t		  PlayoutPolicy,
						bool			  ParseAllFrames );

    PlayerStatus_t   WaitForDrainCompletion(	PlayerStream_t		  Stream,
						bool			  Discard );

    PlayerStatus_t   ScanEventListForMatch( 	PlayerPlayback_t	  Playback,
						PlayerStream_t		  Stream,
						PlayerEventMask_t	  Events,
						unsigned int		**PtrToIndex );

    bool   EventMatchesCriteria( 		PlayerEventRecord_t	 *Record,
						PlayerPlayback_t	  Playback,
						PlayerStream_t		  Stream,
						PlayerEventMask_t	  Events );

    void   ProcessStatistics(			PlayerStream_t		  Stream,
						PlayerSequenceNumber_t	 *Record );

    bool   CheckForNonDecodedFrame( 		PlayerStream_t		  Stream,
						unsigned int		  DisplayFrameIndex );

    void   FlushNonDecodedFrameList( 		PlayerStream_t		  Stream );

    PlayerStatus_t   SetModuleParameters(	PlayerPlayback_t	  Playback,
						PlayerStream_t	 	  Stream,
						unsigned int		  ParameterBlockSize,
						void			 *ParameterBlock );

    void    SwitchCollator(			PlayerStream_t		  Stream );
    void    SwitchFrameParser(			PlayerStream_t		  Stream );
    void    SwitchCodec(			PlayerStream_t		  Stream );
    void    SwitchOutputTimer(			PlayerStream_t		  Stream );
    void    SwitchComplete(			PlayerStream_t		  Stream );

    // Internal process functions called via C

public:

    void ProcessCollateToParse(			PlayerStream_t		  Stream );
    void ProcessParseToDecode(			PlayerStream_t		  Stream );
    void ProcessDecodeToManifest(		PlayerStream_t		  Stream );
    void ProcessPostManifest(			PlayerStream_t		  Stream );

public:

    //
    // Constructor/Destructor methods
    //

    Player_Generic_c( void );
    ~Player_Generic_c( void );

    //
    // Mechanisms for registering global items
    //

    PlayerStatus_t   RegisterBufferManager(	BufferManager_t		  BufferManager );

    PlayerStatus_t   RegisterDemultiplexor(	Demultiplexor_t		  Demultiplexor );

    //
    // Mechanisms relating to event retrieval
    //

    PlayerStatus_t   SpecifySignalledEvents(	PlayerPlayback_t	  Playback,
						PlayerStream_t	 	  Stream,
						PlayerEventMask_t	  Events,
						void			 *UserData	= NULL );

    PlayerStatus_t   SetEventSignal(		PlayerPlayback_t	  Playback,
						PlayerStream_t	 	  Stream,
						PlayerEventMask_t	  Events,
						OS_Event_t		 *Event );

    PlayerStatus_t   GetEventRecord(		PlayerPlayback_t	  Playback,
						PlayerStream_t	 	  Stream,
						PlayerEventMask_t	  Events,
						PlayerEventRecord_t	 *Record,
						bool			  NonBlocking	= false );

    //
    // Mechanisms for policy management
    //

    PlayerStatus_t   SetPolicy(			PlayerPlayback_t	  Playback,
						PlayerStream_t	 	  Stream,
						PlayerPolicy_t		  Policy,
						unsigned char		  PolicyValue 		= PolicyValueApply );

    //
    // Mechanisms for managing playbacks
    //

    PlayerStatus_t   CreatePlayback(		OutputCoordinator_t	  OutputCoordinator,
						PlayerPlayback_t	 *Playback,
                        bool                  SignalEvent           = false,
                        void                 *EventUserData         = NULL );

    PlayerStatus_t   TerminatePlayback(		PlayerPlayback_t	  Playback,
						bool			  SignalEvent		= false,
						void			 *EventUserData		= NULL );

    PlayerStatus_t   AddStream(			PlayerPlayback_t	  Playback,
						PlayerStream_t	 	 *Stream,
						PlayerStreamType_t	  StreamType,
						Collator_t		  Collator,
						FrameParser_t		  FrameParser,
						Codec_t			  Codec,
						OutputTimer_t		  OutputTimer,
						Manifestor_t		  Manifestor		= NULL,
						bool			  SignalEvent		= false,
						void			 *EventUserData		= NULL );

    PlayerStatus_t   RemoveStream(		PlayerStream_t	 	  Stream,
						bool			  SignalEvent		= false,
						void			 *EventUserData		= NULL );

    PlayerStatus_t   SwitchStream(              PlayerStream_t            Stream,
						Collator_t                Collator		= NULL,
						FrameParser_t             FrameParser		= NULL,
						Codec_t                   Codec			= NULL,
						OutputTimer_t             OutputTimer		= NULL,
						bool                      NonBlocking           = false,
						bool                      SignalEvent           = false,
						void                     *EventUserData         = NULL );

    PlayerStatus_t   DrainStream(		PlayerStream_t	 	  Stream,
						bool			  NonBlocking		= false,
						bool			  SignalEvent		= false,
						void			 *EventUserData		= NULL );
    //
    // Mechanisms for managing time
    //

    PlayerStatus_t   SetPlaybackSpeed(		PlayerPlayback_t	  Playback,
						Rational_t		  Speed,
						PlayDirection_t		  Direction );

    PlayerStatus_t   SetPresentationInterval(	PlayerPlayback_t	  Playback,
						PlayerStream_t		  Stream			= PlayerAllStreams,
						unsigned long long	  IntervalStartNativeTime	= INVALID_TIME,
						unsigned long long	  IntervalEndNativeTime		= INVALID_TIME );

    PlayerStatus_t   StreamStep(		PlayerStream_t	 	  Stream );

    PlayerStatus_t   SetNativePlaybackTime(	PlayerPlayback_t	  Playback,
						unsigned long long	  NativeTime,
						unsigned long long	  SystemTime			= INVALID_TIME );

    PlayerStatus_t   RetrieveNativePlaybackTime(PlayerPlayback_t	  Playback,
						unsigned long long	 *NativeTime );

    PlayerStatus_t   TranslateNativePlaybackTime(PlayerPlayback_t	  Playback,
						unsigned long long	  NativeTime,
						unsigned long long	 *SystemTime );

    PlayerStatus_t   RequestTimeNotification(	PlayerStream_t	 	  Stream,
						unsigned long long	  NativeTime,
						void			 *EventUserData		= NULL );

    //
    // Clock recovery support - for playbacks with system clock as master
    //

    PlayerStatus_t   ClockRecoveryInitialize(	PlayerPlayback_t	  Playback,
						PlayerTimeFormat_t	  SourceTimeFormat	= TimeFormatPts );


    PlayerStatus_t   ClockRecoveryDataPoint(	PlayerPlayback_t	  Playback,
						unsigned long long	  SourceTime,
						unsigned long long	  LocalTime );

    PlayerStatus_t   ClockRecoveryEstimate(	PlayerPlayback_t	  Playback,
						unsigned long long	 *SourceTime,
						unsigned long long	 *LocalTime	= NULL );

    //
    // Mechanisms for data insertion
    //

    PlayerStatus_t   GetInjectBuffer(		Buffer_t		 *Buffer );

    PlayerStatus_t   InjectData(		PlayerPlayback_t	  Playback,
						Buffer_t		  Buffer );

    PlayerStatus_t   InputJump(			PlayerPlayback_t	  Playback,
						PlayerStream_t		  Stream,
						bool			  SurplusDataInjected,
						bool			  ContinuousReverseJump	= false );

    PlayerStatus_t   InputGlitch(		PlayerPlayback_t	  Playback,
						PlayerStream_t		  Stream );

    //
    // Mechanisms or data extraction 
    //

    PlayerStatus_t   RequestDecodeBufferReference(
						PlayerStream_t		  Stream,
						unsigned long long	  NativeTime		= TIME_NOT_APPLICABLE,
						void			 *EventUserData		= NULL );

    PlayerStatus_t   ReleaseDecodeBufferReference(
						PlayerEventRecord_t	 *Record );

    //
    // Mechanisms for inserting module specific parameters
    //

    PlayerStatus_t   SetModuleParameters(	PlayerPlayback_t	  Playback,
						PlayerStream_t	 	  Stream,
						PlayerComponent_t	  Component,
						bool			  Immediately,
						unsigned int		  ParameterBlockSize,
						void			 *ParameterBlock );

    //
    // Support functions for the child classes
    //

    PlayerStatus_t   GetBufferManager(		BufferManager_t		 *BufferManager );

    PlayerStatus_t   GetClassList(		PlayerStream_t	 	  Stream,
						Collator_t		 *Collator,
						FrameParser_t		 *FrameParser,
						Codec_t			 *Codec,
						OutputTimer_t		 *OutputTimer,
						Manifestor_t		 *Manifestor );

    PlayerStatus_t   GetCodedFrameBufferPool(	PlayerStream_t	 	  Stream,
						BufferPool_t		 *Pool			= NULL,
						unsigned int		 *MaximumCodedFrameSize = NULL );

    PlayerStatus_t   GetDecodeBufferPool(	PlayerStream_t	 	  Stream,
						BufferPool_t		 *Pool );

    PlayerStatus_t   GetPostProcessControlBufferPool(
						PlayerStream_t	 	  Stream,
						BufferPool_t		 *Pool );

    PlayerStatus_t   CallInSequence(		PlayerStream_t		  Stream,
						PlayerSequenceType_t	  SequenceType,
						PlayerSequenceValue_t	  SequenceValue,
						PlayerComponentFunction_t Fn,
						... );

    PlayerStatus_t   GetPlaybackSpeed(		PlayerPlayback_t	  Playback,
						Rational_t		 *Speed,
						PlayDirection_t		 *Direction );

    PlayerStatus_t   GetPresentationInterval(	PlayerStream_t		  Stream,
						unsigned long long	 *IntervalStartNormalizedTime,
						unsigned long long	 *IntervalEndNormalizedTime );

    unsigned char    PolicyValue(		PlayerPlayback_t	  Playback,
						PlayerStream_t		  Stream,
						PlayerPolicy_t		  Policy );

    PlayerStatus_t   SignalEvent(		PlayerEventRecord_t	 *Record );

    PlayerStatus_t   AttachDemultiplexor(	PlayerStream_t		  Stream,
						Demultiplexor_t		  Demultiplexor,
						DemultiplexorContext_t	  Context );

    PlayerStatus_t   DetachDemultiplexor(	PlayerStream_t		  Stream );

    PlayerStatus_t   MarkStreamUnPlayable(	PlayerStream_t		  Stream );

    PlayerStatus_t   CheckForDemuxBufferMismatch(
						PlayerPlayback_t          Playback,
						PlayerStream_t            Stream );

    void             RecordNonDecodedFrame(	PlayerStream_t		  Stream,
						Buffer_t		  Buffer,
						ParsedFrameParameters_t	 *ParsedFrameParameters );

    unsigned long long   GetLastNativeTime(	PlayerPlayback_t	  Playback );
    void             SetLastNativeTime(		PlayerPlayback_t	  Playback,
						unsigned long long	  Time );
};

#endif

