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

Source file name : player_types.h
Author :           Nick

Definition of the player types for player 2
module.


Date        Modification                                    Name
----        ------------                                    --------
12-Oct-06   Created                                         Nick

************************************************************************/

#ifndef H_PLAYER_TYPES
#define H_PLAYER_TYPES

// Bits that I haven't sourced yet

// /////////////////////////////////////////////////////////////////////////
//
//      External types
//

#include "ring.h"
#include "stack.h"
#include "rational.h"

// /////////////////////////////////////////////////////////////////////////
//
//      defines that may be used within various classes
//

#define PLAYER_MINIMUM_NUMBER_OF_WORKING_CODED_BUFFERS  24      // Absolute minimum number of working coded buffers free in a playback.
								// To support reverse play, you need at least enough for one sequence
								// plus whatever is need for the open group nature of the sequence ie
								// an I and upto 3 Bs on the next sequence

#define PLAYER_MINIMUM_NUMBER_OF_WORKING_DECODE_BUFFERS 3       // Absolute minimum number of working decode buffers free in a playback

// /////////////////////////////////////////////////////////////////////////
//
//      The invalid values, used as initializers or markers
//

#define INVALID_INDEX                   0xffffffff
#define INVALID_SEQUENCE_VALUE          0xfeedfacedeadbeefULL

#define INVALID_TIME                    INVALID_SEQUENCE_VALUE
#define TIME_NOT_APPLICABLE             INVALID_TIME
#define UNSPECIFIED_TIME                (INVALID_TIME + 1)

#define ValidTime(T)                    (((T) != INVALID_TIME) && ((T) != UNSPECIFIED_TIME))
#define ValidIndex(I)                   ((I) != INVALID_INDEX)

// /////////////////////////////////////////////////////////////////////////
//
//      The enumerated types, and associated wrapping mask types
//

    //
    // Status values specific to the player, component classes may
    // add their own here we define the base values for each component.
    //

#define BASE_PLAYER             0x0001
#define BASE_BUFFER             0x1000
#define BASE_DEMULTIPLEXOR      0x2000
#define BASE_COLLATOR           0x3000
#define BASE_FRAME_PARSER       0x4000
#define BASE_CODEC              0x5000
#define BASE_MANIFESTOR         0x6000
#define BASE_OUTPUT_TIMER       0x7000
#define BASE_OUTPUT_COORDINATOR 0x8000
#define BASE_EXTERNAL           0xf000

enum
{
    PlayerNoError               = BASE_PLAYER,
    PlayerError,
    PlayerImplementationError,
    PlayerNotSupported,
    PlayerInsufficientMemory,
    PlayerToMany,
    PlayerUnknownStream,
    PlayerMatchNotFound,
    PlayerNoEventRecords,
    PlayerUnknowMuxType,
    PlayerTimedOut
};

typedef unsigned int    PlayerStatus_t;

    //
    // enumeration of the player components - used in parameter block addressing
    //

typedef enum
{
    ComponentPlayer             = BASE_PLAYER,
    ComponentDemultiplexor      = BASE_DEMULTIPLEXOR,
    ComponentCollator           = BASE_COLLATOR,
    ComponentFrameParser        = BASE_FRAME_PARSER,
    ComponentCodec              = BASE_CODEC,
    ComponentManifestor         = BASE_MANIFESTOR,
    ComponentOutputTimer        = BASE_OUTPUT_TIMER,
    ComponentOutputCoordinator  = BASE_OUTPUT_COORDINATOR,
    ComponentExternal           = BASE_EXTERNAL
} PlayerComponent_t;

    //
    // player component functions - used in in-sequence calling
    //

enum
{
    PlayerFnRegisterBufferManager       = BASE_PLAYER,
    PlayerFnRegisterDemultiplexor,
    PlayerFnSpecifySignalledEvents,
    PlayerFnSetEventSignal,
    PlayerFnGetEventRecord,
    PlayerFnSetPolicy,
    PlayerFnCreatePlayback,
    PlayerFnTerminatePlayback,
    PlayerFnAddStream,
    PlayerFnRemoveStream,
    PlayerFnSwitchStream,
    PlayerFnDrainStream,
    PlayerFnSetPlaybackSpeed,
    PlayerFnStreamStep,
    PlayerFnSetNativePlaybackTime,
    PlayerFnRetrieveNativePlaybackTime,
    PlayerFnTranslateNativePlaybackTime,
    PlayerFnRequestTimeNotification,
    PlayerFnGetInjectBuffer,
    PlayerFnInjectData,
    PlayerFnInputJump,
    PlayerFnRequestDecodeBufferReference,
    PlayerFnReleaseDecodeBufferReference,
    PlayerFnSetModuleParameters,
    PlayerFnGetBufferManager,
    PlayerFnGetClassList,
    PlayerFnGetCodedFrameBufferPool,
    PlayerFnGetDecodeBufferPool,
    PlayerFnGetPostProcessControlBufferPool,
    PlayerFnCallInSequence,
    PlayerFnGetPlaybackSpeed,
    PlayerFnGetPresentationInterval,
    PlayerFnPolicyValue,
    PlayerFnSignalEvent,
    PlayerFnAttachDemultiplexor,
    PlayerFnDetachDemultiplexor,
    PlayerFnMarkStreamUnPlayable
};

typedef unsigned int PlayerComponentFunction_t;

    //
    // The player policies
    //
    //      NOTE when setting up, 0 will always be the
    //           default initialization value for any policy.
    //

#define PolicyValueDisapply                     0
#define PolicyValueApply                        1

typedef enum
{
    //
    // These are here as placeholders, neither of them are implemented at this time
    //

    PolicyTrickModeAudio               = 0,             // Discard/ManifestSampleRateConvert/ManifestPitchCorrected
    PolicyPlay24FPSVideoAt25FPS,                        // Enable/Disable

    //
    // Master clock mechanisms, define which clock is going to be used
    // to master mappings between system time and playback time
    //

#define PolicyValueVideoClockMaster             0
#define PolicyValueAudioClockMaster             1
#define PolicyValueSystemClockMaster            2

    PolicyMasterClock,

    //
    // Use externally specified mapping between playback time and system time (use for AVR fixed offset).
    //

    PolicyExternalTimeMapping,                          // Apply/Disapply
    PolicyExternalTimeMappingVsyncLocked,               // Apply/Disapply - only relevent if external mapping applied

    //
    // Enable/disable stream synchronization
    //

    PolicyAVDSynchronization,                           // Apply/Disapply

    //
    // Policy to set the value of the number of iterations to wait for synchronization.
    //

    PolicySyncStartImmediate,

    //
    // Policy to allow manifestation of the first frame decoded early.
    //

    PolicyManifestFirstFrameEarly,                      // Apply/Disapply

    //
    // Policy to force a null manifestation on shutdown for video
    // audio must always mute.
    //

#define PolicyValueLeaveLastFrameOnScreen       0
#define PolicyValueBlankScreen                  1

    PolicyVideoBlankOnShutdown,                         // Apply/Disapply

    //
    // Trick mode decode controls, to allow Key frames
    // only, and only one group between discontinuities.
    //

    PolicyStreamOnlyKeyFrames,                          // Apply/Disapply
    PolicyStreamSingleGroupBetweenDiscontinuities,
    PolicyClampPlaybackIntervalOnPlaybackDirectionChange,

    //
    // Playout policies for terminate/switch/drain stream
    //

#define PolicyValuePlayout              0
#define PolicyValueDiscard              1

    PolicyPlayoutOnTerminate,                           // PolicyValuePlayout/PolicyValueDiscard
    PolicyPlayoutOnSwitch,
    PolicyPlayoutOnDrain,

    //
    // Display policies
    //

#define PolicyValue4x3                  0
#define PolicyValue16x9                 1

    PolicyDisplayAspectRatio,                           // PolicyValue4x3/PolicyValue16x9

//

#define PolicyValueCentreCutOut         0
#define PolicyValueLetterBox            1
#define PolicyValuePanScan              2
#define PolicyValueFullScreen           3
#define PolicyValueZoom_4_3             4

    PolicyDisplayFormat,

    //
    // Trick mode policy defining fast (forward or reverse) domains
    // Note these are ordered values do NOT change the order
    // code may well use greater than comparisons.
    //

#define PolicyValueTrickModeAuto                                        0
#define PolicyValueTrickModeDecodeAll                                   1
#define PolicyValueTrickModeDecodeAllDegradeNonReferenceFrames          2
#define PolicyValueTrickModeStartDiscardingNonReferenceFrames           3
#define PolicyValueTrickModeDecodeReferenceFramesDegradeNonKeyFrames    4
#define PolicyValueTrickModeDecodeKeyFrames                             5
#define PolicyValueTrickModeDiscontinuousKeyFrames                      6

    PolicyTrickModeDomain,

    //
    // Policy controlling the discard of late frames,
    // if not set then late frames will be manifested,
    // and avd synchronization will be responsible for pulling the
    // stream back into line. If this is set, then any frame that
    // is too late for its manifestation time will be unceremoniously
    // junked. Changed to have a range of possibilities.
    // Never, always, and only after a synchronize until the first
    // frame arrives in time.
    //

#define PolicyValueDiscardLateFramesNever                               0
#define PolicyValueDiscardLateFramesAlways                              1
#define PolicyValueDiscardLateFramesAfterSynchronize                    2

    PolicyDiscardLateFrames,

    //
    // Policy forcing the startup synchronization to work on
    // the basis of starting at the first video frame and
    // letting audio join in appropriately. this has particular
    // effect in some transport streams where there may be a
    // lead in time of upto 1 second of audio. Should only be set
    // if using the agressive "PolicyDiscardLateFrames" policy.
    //

    PolicyVideoStartImmediate,

    //
    // Two policies allowing the rebasing of the system clock
    // when we are struggling to decode or feed data in time.
    //

    PolicyRebaseOnFailureToDeliverDataInTime,
    PolicyRebaseOnFailureToDecodeInTime,

    //
    // Policy controlling the allowed H264 streams. Standard
    // streams contauin IDR frames as re-synchronization points
    // jumps need to be to an IDR to guarantee reference frame
    // integrity. BBC broadcasts do not incorporate IDRs and use
    // I frames to indicate re-synchronization points.
    //

    PolicyH264AllowNonIDRResynchronization,

    //
    // Policy controlling whether or not we allow frames that fail the
    // pre-processor to be decoded.
    //

    PolicyH264AllowBadPreProcessedFrames,

    //
    // Policy controlling whether or not we respect the progressive_frame
    // flag in an mpeg2 picture coding extension header. Some streams
    // from a french broadcaster lie in this field, and this causes
    // interlaced frames to be incorrectly treated as progressive.
    // This policy is quite dangerous since many streams (DVD, VCD and
    // any using 3:2 pulldown) depend on the progressive_frame flag
    // being honoured.
    //

    PolicyMPEG2DoNotHonourProgressiveFrameFlag,

    //
    // Policy controlling the limit at which we can pull the clocks rates
    // this policy is used to restrict the clock pulling software from
    // going crazy. The value N will force clock rate pulling to be limitted to
    // 2^N parts per million.
    //

    PolicyClockPullingLimit2ToTheNPartsPerMillion,

    //
    // Policy enables a limiting mechanism for injecting data ahead of time
    // this forces the collator to only inject a limited amount of data ahead of time.
    //

    PolicyLimitInputInjectAhead,

    //
    // Specify the application for mpeg2 decoding, this affects default values for 
    // colour matrices. 
    //

#define PolicyValueMPEG2ApplicationMpeg2		                0
#define PolicyValueMPEG2ApplicationAtsc			                1
#define PolicyValueMPEG2ApplicationDvb			                2

    PolicyMPEG2ApplicationType,

    //
    // Allow the manifestor to display decimated decoded output
    // Value equals decimation amount
    //

#define PolicyValueDecimateDecoderOutputDisabled                        0
#define PolicyValueDecimateDecoderOutputHalf                            1
#define PolicyValueDecimateDecoderOutputQuarter                         2
     
    PolicyDecimateDecoderOutput,

    //
    // set the forward jump threshold for the PTS jump detector as 2^N seconds
    //

    PolicySymmetricJumpDetection,
    PolicyPtsForwardJumpDetectionThreshold,
     
    //
    // Policies enabling workarounds for badly coded dpb values
    //

    PolicyH264TreatDuplicateDpbValuesAsNonReferenceFrameFirst,
    PolicyH264ForcePicOrderCntIgnoreDpbDisplayFrameOrdering,

    //
    // Policy to treat top bottom, and bottom top picture structures as
    // being interlaced
    //

    PolicyH264TreatTopBottomPictureStructAsInterlaced,

    //
    // Allow the manifestor to know which is the pixel aspect ratio correction requested
    //
    
#define PolicyValuePixelAspectRatioCorrectionDisabled   100
    
    PolicyPixelAspectRatioCorrection,

    //
    // Allow the discard of B frames when running at normal speed, if the decode cannot keep up.
    //
    
    PolicyAllowFrameDiscardAtNormalSpeed,

    //
    // Operate collator 2 in reversible mode
    //
    
    PolicyOperateCollator2InReversibleMode,

    //
    // Smooth display window resize.
    // The value N specifies the number of steps.
    //

    PolicyVideoOutputWindowResizeSteps,

    //
    // Policy to ignore requests to mark a stream as unplayable
    //

    PolicyIgnoreStreamUnPlayableCalls,

    //
    // Policy to allow the use (as a default) of video frame rates deduced from incoming PTS values
    // NOTE any specified frame rate will override the deduced values.
    //      Implementation of this policy is done only by H264 at the 
    //      time of writing.
    //

    PolicyUsePTSDeducedDefaultFrameRates,

//

    PolicyMaxPolicy
} PlayerPolicy_t;

    //
    // The event codes, and their enveloping mask
    //

#define EventIllegalIdentifier                  0x0000000000000000ull
#define EventAllEvents                          0xffffffffffffffffull

    // One-shot events
#define EventFirstFrameManifested               0x0000000000000001ull
#define EventPlaybackTerminated                 0x0000000000000002ull
#define EventStreamTerminated                   0x0000000000000004ull
#define EventStreamSwitched                     0x0000000000000008ull
#define EventStreamDrained                      0x0000000000000010ull
#define EventTimeNotification                   0x0000000000000020ull
#define EventDecodeBufferAvailable              0x0000000000000040ull
#define EventStreamUnPlayable                   0x0000000000000080ull
#define EventFailedToQueueBufferToDisplay       0x0000000000000100ull

#define EventPlaybackCreated                    0x0000000000000200ull
#define EventStreamCreated                      0x0000000000000400ull
#define EventInputFormatCreated                 0x0000000000000800ull
#define EventSupportedInputFormatCreated        0x0000000000001000ull
#define EventDecodeErrorsCreated                0x0000000000002000ull
#define EventSampleFrequencyCreated             0x0000000000004000ull
#define EventNumberChannelsCreated              0x0000000000008000ull
#define EventNumberOfSamplesProcessedCreated    0x0000000000010000ull

#define EventVsyncOffsetMeasured		0x0000000000020000ull
#define EventFatalHardwareFailure		0x0000000000040000ull

    // Ongoing events
#define EventSizeChangeParse                    0x0000000100000000ull
#define EventSourceSizeChangeManifest           0x0000000200000000ull
#define EventOutputSizeChangeManifest           0x0000000400000000ull

#define EventFrameRateChangeParse               0x0000000800000000ull
#define EventSourceFrameRateChangeManifest      0x0000001000000000ull
#define EventOutputFrameRateChangeManifest      0x0000002000000000ull

#define EventBufferRelease                      0x0000004000000000ull

#define EventTimeMappingEstablished             0x0000008000000000ull
#define EventTimeMappingReset                   0x0000010000000000ull

#define EventFailedToDecodeInTime               0x0000020000000000ull
#define EventFailedToDeliverDataInTime          0x0000040000000000ull

#define EventTrickModeDomainChange              0x0000080000000000ull
#define EventFailureToPlaySmoothReverse         0x0000100000000000ull

#define EventInputFormatChanged                 0x0000200000000000ull

typedef unsigned long long                      PlayerEventIdentifier_t;
typedef unsigned long long                      PlayerEventMask_t;

    //
    // The stream type
    //

typedef enum
{
    StreamTypeNone                      = 0,
    StreamTypeAudio,
    StreamTypeVideo,
    StreamTypeOther
} PlayerStreamType_t;

const char *ToString(PlayerStreamType_t StreamType);

    //
    // Play direction
    //

typedef enum
{
    PlayForward = 0,
    PlayBackward
} PlayDirection_t;

    //
    // Specifiers for in sequence calling
    //

typedef enum
{
    SequenceTypeImmediate               = 0,
    SequenceTypeBeforeSequenceNumber,
    SequenceTypeAfterSequenceNumber,
    SequenceTypeBeforePlaybackTime,
    SequenceTypeAfterPlaybackTime
} PlayerSequenceType_t;

typedef unsigned long long PlayerSequenceValue_t;

    //
    // Enumeration of time formats
    //

typedef enum
{
    TimeFormatUs			= 0,
    TimeFormatPts
} PlayerTimeFormat_t;

//

typedef enum
{
    ChannelSelectStereo                 = 0,
    ChannelSelectLeft,
    ChannelSelectRight
} PlayerChannelSelect_t;

//

// /////////////////////////////////////////////////////////////////////////
//
// The wrapper pointer types, here before the structures to
// allow for those self referential structures (such as lists),
// and incestous references.
//

typedef struct DemultiplexorContext_s   *DemultiplexorContext_t;

typedef class Demultiplexor_c           *Demultiplexor_t;
typedef class Collator_c                *Collator_t;
typedef class FrameParser_c             *FrameParser_t;
typedef class Codec_c                   *Codec_t;
typedef class OutputTimer_c             *OutputTimer_t;
typedef class OutputCoordinator_c       *OutputCoordinator_t;
typedef class Manifestor_c              *Manifestor_t;
typedef class Player_c                  *Player_t;

typedef struct PlayerPlayback_s         *PlayerPlayback_t;
typedef struct PlayerStream_s           *PlayerStream_t;

#define PlayerAllPlaybacks              NULL
#define PlayerAllStreams                NULL

// /////////////////////////////////////////////////////////////////////////
//
// The structures
//

// -----------------------------------------------------------
// The event record
//

typedef struct PlayerEventRecord_s
{
    PlayerEventIdentifier_t        Code;
    PlayerPlayback_t               Playback;
    PlayerStream_t                 Stream;
    unsigned long long             PlaybackTime;

    union
    {
	unsigned int               UnsignedInt;
	unsigned long long         LongLong;
	void                      *Pointer;
    } Value[4];

    //
    // Since rationals are a class, we need to separate them from the union and make them specific
    //

    Rational_t                     Rational;

    void                          *UserData;
} PlayerEventRecord_t;

//

// -----------------------------------------------------------
// The attribute descriptor
//

#define SYSFS_ATTRIBUTE_ID_BOOL                 0
#define SYSFS_ATTRIBUTE_ID_INTEGER              SYSFS_ATTRIBUTE_ID_BOOL + 1
#define SYSFS_ATTRIBUTE_ID_CONSTCHARPOINTER     SYSFS_ATTRIBUTE_ID_INTEGER + 1
#define SYSFS_ATTRIBUTE_ID_UNSIGNEDLONGLONGINT  SYSFS_ATTRIBUTE_ID_CONSTCHARPOINTER +1

typedef struct PlayerAttributeDescriptor_s
{
    int                                 Id;

    union
    {
	const char                     *ConstCharPointer;
	int                             Int;
	unsigned long long int          UnsignedLongLongInt;
	bool                            Bool;
    } u;

} PlayerAttributeDescriptor_t;

//

#endif

