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

Source file name : output_timer_base.h
Author :           Nick

Basic instance of the output timer class module for player 2.


Date        Modification                                    Name
----        ------------                                    --------
09-Mar-07   Created                                         Nick

************************************************************************/

#ifndef H_OUTPUT_TIMER_BASE
#define H_OUTPUT_TIMER_BASE

#include "player.h"

// /////////////////////////////////////////////////////////////////////////
//
// Frequently used macros
//

#define GROUP_STRUCTURE_INTEGRATION_PERIOD	8

#ifndef ENABLE_OUTPUT_TIMER_DEBUG
#define ENABLE_OUTPUT_TIMER_DEBUG 0
#endif

#define OUTPUT_TIMER_TAG      "Output timer"
#define OUTPUT_TIMER_FUNCTION (ENABLE_OUTPUT_TIMER_DEBUG ? __PRETTY_FUNCTION__ : OUTPUT_TIMER_TAG)

/* Output debug information (which may be on the critical path) but is usually turned off */
#define OUTPUT_TIMER_DEBUG(fmt, args...) ((void)(ENABLE_OUTPUT_TIMER_DEBUG && \
                                          (report(severity_note, "%s: " fmt, OUTPUT_TIMER_FUNCTION, ##args), 0)))

/* Output trace information off the critical path */
#define OUTPUT_TIMER_TRACE(fmt, args...) (report(severity_note, "%s: " fmt, OUTPUT_TIMER_FUNCTION, ##args))
/* Output errors, should never be output in 'normal' operation */
#define OUTPUT_TIMER_ERROR(fmt, args...) (report(severity_error, "%s: " fmt, OUTPUT_TIMER_FUNCTION, ##args))


// ---------------------------------------------------------------------
//
// Local type definitions
//

    //
    // Enumeration of the possible states of the synchronizer
    //

typedef enum
{
    SyncStateInSynchronization			= 1,
    SyncStateSuspectedSyncError,
    SyncStateConfirmedSyncError,
    SyncStateStartAwaitingCorrectionWorkthrough,
    SyncStateAwaitingCorrectionWorkthrough
} SynchronizationState_t;

    //
    // Enumeration of the decoding in time states
    //

typedef enum
{
    DecodingInTime				= 1,
    AccumulatingDecodeInTimeFailures,
    AccumulatedDecodeInTimeFailures,
    PauseBetweenDecodeInTimeIntegrations
} DecodeInTimeState_t;

    //
    // Enumeration of trick mode domains
    //

typedef enum
{
    TrickModeInvalid					= 0,
    TrickModeDecodeAll					= PolicyValueTrickModeDecodeAll,
    TrickModeDecodeAllDegradeNonReferenceFrames		= PolicyValueTrickModeDecodeAllDegradeNonReferenceFrames,
    TrickModeStartDiscardingNonReferenceFrames		= PolicyValueTrickModeStartDiscardingNonReferenceFrames,
    TrickModeDecodeReferenceFramesDegradeNonKeyFrames	= PolicyValueTrickModeDecodeReferenceFramesDegradeNonKeyFrames,
    TrickModeDecodeKeyFrames				= PolicyValueTrickModeDecodeKeyFrames,
    TrickModeDiscontinuousKeyFrames			= PolicyValueTrickModeDiscontinuousKeyFrames
} TrickModeDomain_t;

    //
    // Configuration structure for the base class
    //

typedef struct OutputTimerConfiguration_s
{
    const char			 *OutputTimerName;

    PlayerStreamType_t		  StreamType;

    BufferType_t		  AudioVideoDataParsedParametersType;
    unsigned int		  SizeOfAudioVideoDataParsedParameters;

    BufferType_t		  AudioVideoDataOutputTimingType;
    unsigned int		  SizeOfAudioVideoDataOutputTiming;

    void			**AudioVideoDataOutputSurfaceDescriptorPointer;

    //
    // Values controlling the decode window for a frame, 
    // the first of which can be refined over a period as 
    // frames are timed. 
    //

    unsigned long long		  FrameDecodeTime;
    unsigned long long		  EarlyDecodePorch;
    unsigned long long		  MinimumManifestorLatency;

    unsigned int		  MaximumDecodeTimesToWait;	// This will be adjusted by the speed factor when used.

    //
    // Values controlling the avsync mechanisms
    //

    unsigned long long		  SynchronizationErrorThreshold;
    unsigned long long		  SynchronizationErrorThresholdForExternalSync;
    unsigned int		  SynchronizationIntegrationCount;
    unsigned int		  SynchronizationWorkthroughCount;

    //
    // trick mode controls
    //

    bool			  ReversePlaySupported;
    Rational_t			  MinimumSpeedSupported;
    Rational_t			  MaximumSpeedSupported;

} OutputTimerConfiguration_t;

    //
    // Parameter block settings
    //

typedef enum
{
    OutputTimerSetNormalizedTimeOffset		= BASE_OUTPUT_TIMER
} OutputTimerParameterBlockType_t;

//

typedef struct OutputTimerSetNormalizedTimeOffset_s
{
    long long		  Value;
} OutputTimerSetNormalizedTimeOffset_t;

//

typedef struct OutputTimerParameterBlock_s
{
    OutputTimerParameterBlockType_t		ParameterType;

    union
    {
	OutputTimerSetNormalizedTimeOffset_t	Offset; ///< Offset to apply to the stream (in microseconds)
    };
} OutputTimerParameterBlock_t;


// ---------------------------------------------------------------------
//
// Class definition
//

class OutputTimer_Base_c : public OutputTimer_c
{
protected:

    // Data

    OS_Mutex_t			  Lock;
    OutputTimerConfiguration_t	  Configuration;

    OutputCoordinator_t		  OutputCoordinator;
    OutputCoordinatorContext_t	  OutputCoordinatorContext;
    BufferManager_t		  BufferManager;
    BufferPool_t		  DecodeBufferPool;
    BufferPool_t		  CodedFrameBufferPool;
    unsigned int		  CodedFrameBufferMaximumSize;

    CodecTrickModeParameters_t	  TrickModeParameters;
    TrickModeDomain_t		  TrickModeDomain;
    unsigned char		  TrickModePolicy;
    unsigned int		  TrickModeCheckReferenceFrames;
    Rational_t			  TrickModeDomainTransitToDegradeNonReference;
    Rational_t			  TrickModeDomainTransitToStartDiscard;
    Rational_t			  TrickModeDomainTransitToDecodeReference;
    Rational_t			  TrickModeDomainTransitToDecodeKey;

    Rational_t			  CodedFrameRate;
    Rational_t			  LastCodedFrameRate;
    Rational_t			  EmpiricalCodedFrameRateLimit;
    Rational_t			  AdjustedSpeedAfterFrameDrop;
    Rational_t			  PortionOfPreDecodeFramesToLose;
    Rational_t			  AccumulatedPreDecodeFramesToLose;

    unsigned int		  NextGroupStructureEntry;
    unsigned int		  GroupStructureSizes[GROUP_STRUCTURE_INTEGRATION_PERIOD];
    unsigned int		  GroupStructureReferences[GROUP_STRUCTURE_INTEGRATION_PERIOD];
    unsigned int		  GroupStructureSizeCount;
    unsigned int		  GroupStructureReferenceCount;
    Rational_t			  TrickModeGroupStructureIndependentFrames;
    Rational_t			  TrickModeGroupStructureReferenceFrames;
    Rational_t			  TrickModeGroupStructureNonReferenceFrames;

    unsigned char		  LastTrickModePolicy;				// Used to detect changes in trick mode parameters
    TrickModeDomain_t		  LastTrickModeDomain;
    Rational_t			  LastPortionOfPreDecodeFramesToLose;
    Rational_t			  LastTrickModeSpeed;
    Rational_t			  LastTrickModeGroupStructureIndependentFrames;
    Rational_t			  LastTrickModeGroupStructureReferenceFrames;

    long long	 		  DecodeTimeJumpThreshold;
    long long			  NormalizedTimeOffset;

    ParsedFrameParameters_t	 *PreDecodeParsedFrameParameters;		// Pre decode buffer pointers
    void			 *PreDecodeParsedAudioVideoDataParameters;

    ParsedFrameParameters_t	 *PostDecodeParsedFrameParameters;		// Post decode buffer pointers
    void			 *PostDecodeParsedAudioVideoDataParameters;
    void			 *PostDecodeAudioVideoDataOutputTiming;

    void			 *PostManifestAudioVideoDataOutputTiming;	// Post manifest buffer pointers

    bool			  TimeMappingValidForDecodeTiming;	// Values used in decode time window checking
    unsigned int		  TimeMappingInvalidatedAtDecodeIndex;
    unsigned long long		  LastSeenDecodeTime;

    unsigned long long		  NextExpectedPlaybackTime;		// Value used in check for PTS jumps
    unsigned long long		  PlaybackTimeIncrement;

    OutputTimerStatus_t		  LastFrameDropPreDecodeDecision;
    unsigned int		  KeyFramesSinceDiscontinuity;
    unsigned long long		  LastKeyFramePlaybackTime;

    unsigned long long		  LastActualFrameTime;			// Values used/generated by post manifestation processing
    unsigned long long		  LastExpectedFrameTime;
    Rational_t			  OutputRateAdjustment;
    Rational_t			  SystemClockAdjustment;

    SynchronizationState_t	  SynchronizationState;			// Values used in synchronization
    unsigned int		  SynchronizationFrameCount;
    bool			  SynchronizationAtStartup;
    unsigned int		  SynchronizationErrorIntegrationCount;
    long long 			  SynchronizationAccumulatedError;
    long long 			  SynchronizationError;
    unsigned int		  FrameWorkthroughCount;

    DecodeInTimeState_t		  DecodeInTimeState;
    unsigned int		  DecodeInTimeFailures;
    unsigned int		  DecodeInTimeCodedDataLevel;
    unsigned long long 		  DecodeInTimeBehind;
    bool			  SeenAnInTimeFrame;

    unsigned char		  LastSynchronizationPolicy;

    // Functions

    bool   PlaybackTimeCheckGreaterThanOrEqual(			unsigned long long 	T0,
								unsigned long long	T1 );

    bool   PlaybackTimeCheckIntervalsIntersect(			unsigned long long 	I0Start,
								unsigned long long	I0End,
								unsigned long long 	I1Start,
								unsigned long long	I1End );

    void			  SetTrickModeDomainBoundaries(	void );
    OutputTimerStatus_t		  TrickModeControl(		void );
    void			  InitializeGroupStructure(	void );
    void			  MonitorGroupStructure(	ParsedFrameParameters_t	 *ParsedFrameParameters );
    void			  DecodeInTimeFailure(		unsigned long long	  FailedBy );

    OutputTimerStatus_t		  ExtractPointersPreDecode(	Buffer_t		Buffer );
    OutputTimerStatus_t		  ExtractPointersPostDecode(	Buffer_t		Buffer );
    OutputTimerStatus_t		  ExtractPointersPostManifest(	Buffer_t		Buffer );

    OutputTimerStatus_t		  DropTestBeforeDecodeWindow(	Buffer_t		Buffer );
    OutputTimerStatus_t		  DropTestBeforeDecode(		Buffer_t		Buffer );
    OutputTimerStatus_t		  DropTestBeforeOutputTiming(	Buffer_t		Buffer );
    OutputTimerStatus_t		  DropTestBeforeManifestation(	Buffer_t		Buffer );

public:

    //
    // Constructor/Destructor methods
    //

    OutputTimer_Base_c(	void );
    ~OutputTimer_Base_c(	void );

    //
    // Overrides for component base class functions
    //

    OutputTimerStatus_t   Halt(				void );
    OutputTimerStatus_t   Reset(			void );

    OutputTimerStatus_t   SetModuleParameters(		unsigned int	  ParameterBlockSize,
							void		 *ParameterBlock );

    //
    // OutputTimer class functions
    //

    OutputTimerStatus_t   RegisterOutputCoordinator(	OutputCoordinator_t	  OutputCoordinator );

    OutputTimerStatus_t   ResetTimeMapping(		void );

    OutputTimerStatus_t   AwaitEntryIntoDecodeWindow(   Buffer_t		  Buffer );

    OutputTimerStatus_t   TestForFrameDrop(		Buffer_t		  Buffer,
							OutputTimerTestPoint_t	  TestPoint );

    OutputTimerStatus_t   GenerateFrameTiming(		Buffer_t		  Buffer );

    OutputTimerStatus_t   RecordActualFrameTiming(	Buffer_t		  Buffer );
    
    OutputTimerStatus_t   GetStreamStartDelay(		unsigned long long	 *Delay );

    static const char *   LookupStreamType(             PlayerStreamType_t StreamType );

protected:

    //
    // Function to perform AVD synchronization, could be overridden, but not recomended
    //

    virtual OutputTimerStatus_t   PerformAVDSync(		unsigned long long	  ExpectedFrameOutputTime,
								unsigned long long	  ActualFrameOutputTime );


    //
    // Functions to be provided by the stream specific implementations
    //

    virtual OutputTimerStatus_t   InitializeConfiguration(	void ) = 0;

    virtual OutputTimerStatus_t   FrameDuration(		void			  *ParsedAudioVideoDataParameters,
								unsigned long long	  *Duration ) = 0;

    virtual OutputTimerStatus_t   FillOutFrameTimingRecord(	unsigned long long	  SystemTime,
								void			 *ParsedAudioVideoDataParameters,
								void			 *AudioVideoDataOutputTiming ) = 0;

    virtual OutputTimerStatus_t   CorrectSynchronizationError(	void ) = 0;
};
#endif

