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

#ifndef H_PLAYER_TYPES
#define H_PLAYER_TYPES

#ifdef __cplusplus
#include "ring.h"
#include "stack.h"
#include "rational.h"
#endif // __cplusplus

#include "stm_se.h"

// /////////////////////////////////////////////////////////////////////////
//
//      defines that may be used within various classes
//

#define MAXIMUM_NUMBER_OF_SUPPORTED_MANIFESTATIONS  8
#define MAXIMUM_NUMBER_OF_TIMINGS_PER_MANIFESTATION 3
#define MAXIMUM_MANIFESTATION_TIMING_COUNT      ((MAXIMUM_NUMBER_OF_SUPPORTED_MANIFESTATIONS) * (MAXIMUM_NUMBER_OF_TIMINGS_PER_MANIFESTATION))

#define PLAYER_MINIMUM_NUMBER_OF_WORKING_CODED_BUFFERS  24      // Absolute minimum number of working coded buffers free in a playback.
// To support reverse play, you need at least enough for one sequence
// plus whatever is need for the open group nature of the sequence ie
// an I and up to 3 Bs on the next sequence

#define PLAYER_MINIMUM_NUMBER_OF_WORKING_DECODE_BUFFERS 3       // Absolute minimum number of working decode buffers free in a playback

// 1H delay allowed, this high delay allows usecases such as slideset-like mpeg file usecase
#define PLAYER_DELAY_LIMIT_IN_US 3600*1000000ULL

#define PTS_MASK 0x00000001FFFFFFFFULL // PTS mask

// /////////////////////////////////////////////////////////////////////////
//
//      The invalid values, used as initializers or markers
//

#define INVALID_INDEX                   0xffffffff
#define INVALID_SEQUENCE_VALUE          0xfeedfacedeadbeefULL

#define INVALID_TIME                    INVALID_SEQUENCE_VALUE
#define TIME_NOT_APPLICABLE             INVALID_TIME
#define UNSPECIFIED_TIME                (INVALID_TIME + 1)

#define ValidTime(T)                    ((((unsigned long long)T) != INVALID_TIME) && (((unsigned long long)T) != UNSPECIFIED_TIME))
#define NotValidTime(T)                 (!ValidTime(T))
#define ValidIndex(I)                   ((I) != INVALID_INDEX)

// /////////////////////////////////////////////////////////////////////////
//
//      The enumerated types, and associated wrapping mask types
//

//
// Status values specific to the player, component classes may
// add their own here we define the base values for each component.
// TODO(pht) unifiy SE errors

#define BASE_PLAYER                     0x00000001
#define BASE_BUFFER                     0x00001000

#define BASE_COLLATOR                   0x00003000
#define BASE_FRAME_PARSER               0x00004000
#define BASE_CODEC                      0x00005000
#define BASE_MANIFESTOR                 0x00006000
#define BASE_OUTPUT_TIMER               0x00007000
#define BASE_OUTPUT_COORDINATOR         0x00008000
#define BASE_DECODE_BUFFER_MANAGER      0x00009000
#define BASE_MANIFESTATION_COORDINATOR  0x0000a000

#define BASE_ENCODER                    0x0000b000
#define BASE_PREPROC                    0x0000c000
#define BASE_CODER                      0x0000d000
#define BASE_TRANSPORTER                0x0000e000
#define BASE_ENCODE_COORDINATOR         0x0000f000

#define BASE_EXTERNAL                   0xf0000000

enum
{
    PlayerNoError               = BASE_PLAYER,
    PlayerError,
    PlayerImplementationError,
    PlayerNotSupported,
    PlayerInsufficientMemory,
    PlayerTooMany,
    PlayerUnknownStream,
    PlayerMatchNotFound,
    PlayerNoEventRecords,
    PlayerTimedOut,
    PlayerBusy,
};

typedef unsigned int    PlayerStatus_t;

//
// enumeration of the player components - used in parameter block addressing
//

typedef enum
{
    ComponentPlayer             = BASE_PLAYER,
    ComponentCollator           = BASE_COLLATOR,
    ComponentFrameParser        = BASE_FRAME_PARSER,
    ComponentCodec              = BASE_CODEC,
    ComponentManifestor         = BASE_MANIFESTOR,
    ComponentOutputTimer        = BASE_OUTPUT_TIMER,
    ComponentOutputCoordinator  = BASE_OUTPUT_COORDINATOR,
    ComponentDecodeBufferManager = BASE_DECODE_BUFFER_MANAGER,
    ComponentManifestationCoordinator = BASE_MANIFESTATION_COORDINATOR,
    ComponentEncoder            = BASE_ENCODER,
    ComponentPreproc            = BASE_PREPROC,
    ComponentCoder              = BASE_CODER,
    ComponentTransporter        = BASE_TRANSPORTER,
    ComponentEncodeCoordinator  = BASE_ENCODE_COORDINATOR,
    ComponentExternal           = BASE_EXTERNAL
} PlayerComponent_t;

typedef unsigned int PlayerComponentFunction_t;

//
// The player policies
//

typedef enum
{
    NonCascadedControls = 0,   // This is the default value return by the StmMapOption for non policy control

#define PolicyValueDisapply                 STM_SE_CTRL_VALUE_DISAPPLY
#define PolicyValueApply                    STM_SE_CTRL_VALUE_APPLY
#define PolicyValueAuto                     STM_SE_CTRL_VALUE_AUTO

    // Policy to use for live playback
    PolicyLivePlayback,                         // Apply/Disapply

    // Enable/disable stream synchronization
    PolicyAVDSynchronization,               // Apply/Disapply

    //
    PolicyClampPlaybackIntervalOnPlaybackDirectionChange,  // Apply/Disapply

    // Playout policies for terminate/switch/drain stream
#define PolicyValuePlayout                  STM_SE_CTRL_VALUE_PLAYOUT
#define PolicyValueDiscard                  STM_SE_CTRL_VALUE_DISCARD
    PolicyPlayoutOnTerminate,
    PolicyPlayoutOnSwitch,
    PolicyPlayoutOnDrain,
    PolicyPlayoutOnInternalDrain,

    // Policy to ignore requests to mark a stream as unplayable
    PolicyIgnoreStreamUnPlayableCalls,              // Apply/Disapply

    // Policy to indicate that the next playStream creation must not
    // Allocate decode buffer because they will be provided by user
    PolicyDecodeBuffersUserAllocated,       // Apply/Disapply

    // Policy enables a limiting mechanism for injecting data ahead of time
    // this forces the collator to only inject a limited amount of data ahead of time.
    PolicyLimitInputInjectAhead,            // Apply/Disapply

    // Policy to check a code for detecting control data
    PolicyControlDataDetection,             // Apply/Disapply

    // Policy to force playstream collator to accumulate the minimum required partitions/frames
    PolicyReduceCollatedData,

    // Operate collator2 in reversible mode
    PolicyOperateCollator2InReversibleMode,         // Apply/Disapply

    // collator policy to force the player to monitor PCR V PTS situation (no control associated!)
    PolicyMonitorPCRTiming,                 // Apply/Disapply

    // Policy controlling whether or not we respect the progressive_frame
    // flag in an mpeg2 picture coding extension header. Some streams
    // from a french broadcaster lie in this field, and this causes
    // interlaced frames to be incorrectly treated as progressive.
    // This policy is quite dangerous since many streams (DVD, VCD and
    // any using 3:2 pulldown) depend on the progressive_frame flag
    // being honoured.
    PolicyMPEG2DoNotHonourProgressiveFrameFlag,     // Apply/Disapply

    // Specify the application for mpeg2 decoding, this affects default values for
    // colour matrices.
#define PolicyValueMPEG2ApplicationMpeg2    STM_SE_CTRL_VALUE_MPEG2_APPLICATION_MPEG2
#define PolicyValueMPEG2ApplicationAtsc     STM_SE_CTRL_VALUE_MPEG2_APPLICATION_ATSC
#define PolicyValueMPEG2ApplicationDvb      STM_SE_CTRL_VALUE_MPEG2_APPLICATION_DVB
    PolicyMPEG2ApplicationType,

    // Policies enabling workarounds for badly coded dpb values
    PolicyH264TreatDuplicateDpbValuesAsNonReferenceFrameFirst,  // Apply/Disapply
    PolicyH264ForcePicOrderCntIgnoreDpbDisplayFrameOrdering,    // Apply/Disapply
    PolicyH264ValidateDpbValuesAgainstPTSValues,                // Apply/Disapply
    // For transport streams this is preferable to the previous one

    // Policy to treat top bottom, and bottom top picture structures as
    // being interlaced
    PolicyH264TreatTopBottomPictureStructAsInterlaced,          // Apply/Disapply

    // Policy controlling the allowed H264 streams. Standard
    // streams contain IDR frames as re-synchronization points
    // jumps need to be to an IDR to guarantee reference frame
    // integrity. BBC broadcasts do not incorporate IDRs and use
    // I frames to indicate re-synchronization points.
    PolicyH264AllowNonIDRResynchronization,     // Apply/Disapply

#define PolicyValuePrecedenceStreamPtsContainerDefault  STM_SE_CTRL_VALUE_PRECEDENCE_STREAM_PTS_CONTAINER_DEFAULT
#define PolicyValuePrecedenceStreamContainerPtsDefault  STM_SE_CTRL_VALUE_PRECEDENCE_STREAM_CONTAINER_PTS_DEFAULT
#define PolicyValuePrecedencePtsStreamContainerDefault  STM_SE_CTRL_VALUE_PRECEDENCE_PTS_STREAM_CONTAINER_DEFAULT
#define PolicyValuePrecedencePtsContainerStreamDefault  STM_SE_CTRL_VALUE_PRECEDENCE_PTS_CONTAINER_STREAM_DEFAULT
#define PolicyValuePrecedenceContainerPtsStreamDefault  STM_SE_CTRL_VALUE_PRECEDENCE_CONTAINER_PTS_STREAM_DEFAULT
#define PolicyValuePrecedenceContainerStreamPtsDefault  STM_SE_CTRL_VALUE_PRECEDENCE_CONTAINER_STREAM_PTS_DEFAULT
    PolicyFrameRateCalculationPrecedence,

    // Policy to set container framerate
    PolicyContainerFrameRate,       // range 7..120

    // Policy controlling whether or not we allow H264 frames that fail
    // the pre-processor to be decoded.
    PolicyAllowBadPreProcessedFrames,        // Apply/Disapply

    // Policy controlling whether or not we allow HEVC frames that fail
    // the pre-processor to be decoded.
    PolicyAllowBadHevcPreProcessedFrames,        // Apply/Disapply

    // The set of policies that control decoding and error concealment options
    //
    //    Decoding mode -
    //      maximal, or give up when we find an error
    //
    //    Reference frame substitution -
    //      when a reference frame is discarded for
    //      decoding/quality issues, do we a recent
    //      frame to substitute for reference purposes.
    //
    //    Discard for reference quality threshold -
    //      Quality of a frame is 0..255, if a
    //      frame is less than the specified value
    //      it will not be used for reference
    //
    //    Discard for manifestation quality threshold -
    //      Quality of a frame is 0..255, if a
    //      frame is less than the specified value
    //      it will not be manifested
#define PolicyValuePolicyErrorDecodingLevelNormal   STM_SE_CTRL_VALUE_ERROR_DECODING_LEVEL_NORMAL
#define PolicyValuePolicyErrorDecodingLevelMaximum  STM_SE_CTRL_VALUE_ERROR_DECODING_LEVEL_MAXIMUM
    PolicyErrorDecodingLevel,

    // Specify the application for audio decoding purposes.
#define PolicyValueAudioApplicationIso               STM_SE_CTRL_VALUE_AUDIO_APPLICATION_ISO
#define PolicyValueAudioApplicationDvd               STM_SE_CTRL_VALUE_AUDIO_APPLICATION_DVD
#define PolicyValueAudioApplicationDvb               STM_SE_CTRL_VALUE_AUDIO_APPLICATION_DVB
#define PolicyValueAudioApplicationMS10              STM_SE_CTRL_VALUE_AUDIO_APPLICATION_MS10
#define PolicyValueAudioApplicationMS11              STM_SE_CTRL_VALUE_AUDIO_APPLICATION_MS11
#define PolicyValueAudioApplicationMS12              STM_SE_CTRL_VALUE_AUDIO_APPLICATION_MS12
#define PolicyValueAudioApplicationHDMIRxGameMode    STM_SE_CTRL_VALUE_AUDIO_APPLICATION_HDMIRX_GAME_MODE
    PolicyAudioApplicationType,

    // Clarify to which type of audio service corresponds an audio play-stream
#define PolicyValueAudioServicePrimary                  STM_SE_CTRL_VALUE_AUDIO_SERVICE_PRIMARY
#define PolicyValueAudioServiceSecondary                STM_SE_CTRL_VALUE_AUDIO_SERVICE_SECONDARY
#define PolicyValueAudioServiceMain                     STM_SE_CTRL_VALUE_AUDIO_SERVICE_MAIN
#define PolicyValueAudioServiceAudioDescription         STM_SE_CTRL_VALUE_AUDIO_SERVICE_AUDIO_DESCRIPTION
#define PolicyValueAudioServiceMainAndAudioDescription  STM_SE_CTRL_VALUE_AUDIO_SERVICE_MAIN_AND_AUDIO_DESCRIPTION
#define PolicyValueAudioServiceCleanAudio               STM_SE_CTRL_VALUE_AUDIO_SERVICE_CLEAN_AUDIO
#define PolicyValueAudioServiceAudioGenerator           STM_SE_CTRL_VALUE_AUDIO_SERVICE_AUDIO_GENERATOR
#define PolicyValueAudioServiceInteractiveAudio         STM_SE_CTRL_VALUE_AUDIO_SERVICE_INTERACTIVE_AUDIO
    PolicyAudioServiceType,

    // Policy to enable the use of certain region specific controls
#define PolicyValueRegionUndefined          STM_SE_CTRL_VALUE_REGION_UNDEFINED
#define PolicyValueRegionATSC               STM_SE_CTRL_VALUE_REGION_ATSC
#define PolicyValueRegionDVB                STM_SE_CTRL_VALUE_REGION_DVB
#define PolicyValueRegionNORDIG             STM_SE_CTRL_VALUE_REGION_NORDIG
#define PolicyValueRegionDTG                STM_SE_CTRL_VALUE_REGION_DTG
#define PolicyValueRegionARIB               STM_SE_CTRL_VALUE_REGION_ARIB
#define PolicyValueRegionDTMB               STM_SE_CTRL_VALUE_REGION_DTMB
    PolicyRegionType,

    // Policy to get the audio program reference level from the user
    PolicyAudioProgramReferenceLevel,       // range -3100..0

    // In a stream with multiple substream, specify which substream shall be decoded
    PolicyAudioSubstreamId,     // range 0..31

    // codec policies
    PolicyAllowReferenceFrameSubstitution,
    PolicyDiscardForReferenceQualityThreshold,
    PolicyDiscardForManifestationQualityThreshold,

    // Allow the manifestor to display decimated decoded output
    // Value equals decimation amount
#define PolicyValueDecimateDecoderOutputDisabled        STM_SE_CTRL_VALUE_DECIMATE_DECODER_OUTPUT_DISABLED
#define PolicyValueDecimateDecoderOutputHalf            STM_SE_CTRL_VALUE_DECIMATE_DECODER_OUTPUT_HALF
#define PolicyValueDecimateDecoderOutputQuarter         STM_SE_CTRL_VALUE_DECIMATE_DECODER_OUTPUT_QUARTER
    PolicyDecimateDecoderOutput,

    // * PolicyValueVideoPlayStreamMemoryProfileHDLegacy keeps the current (high)
    // memory footprint (9 HD buffers for both ref and raster buffers).
    // * PolicyValueVideoPlayStreamMemoryProfileHDOptimized introduces a
    // slightly lower footprint (6 HD buffers for ref and 8 HD raster buffers),
    // it can be used for most usecases, but will NOT work for smooth reverse.
    // * PolicyValueVideoPlayStreamMemoryProfile4K2K for 4K2K uc
    // * PolicyValueVideoPlayStreamMemoryProfileSD for SD profile (mosaic uc)
    // * PolicyValueVideoPlayStreamMemoryProfileHD720p for 720p
#define PolicyValueVideoPlayStreamMemoryProfileHDLegacy            STM_SE_CTRL_VALUE_VIDEO_DECODE_HD_LEGACY_PROFILE
#define PolicyValueVideoPlayStreamMemoryProfileHDOptimized         STM_SE_CTRL_VALUE_VIDEO_DECODE_HD_PROFILE
#define PolicyValueVideoPlayStreamMemoryProfile4K2K                STM_SE_CTRL_VALUE_VIDEO_DECODE_4K2K_PROFILE
#define PolicyValueVideoPlayStreamMemoryProfileSD                  STM_SE_CTRL_VALUE_VIDEO_DECODE_SD_PROFILE
#define PolicyValueVideoPlayStreamMemoryProfileHD720p              STM_SE_CTRL_VALUE_VIDEO_DECODE_720P_PROFILE
#define PolicyValueVideoPlayStreamMemoryProfileUHD                 STM_SE_CTRL_VALUE_VIDEO_DECODE_UHD_PROFILE
#define PolicyValueVideoPlayStreamMemoryProfileHDMIRepeaterHD      STM_SE_CTRL_VALUE_VIDEO_HDMIRX_REP_HD_PROFILE
#define PolicyValueVideoPlayStreamMemoryProfileHDMIRepeaterUHD     STM_SE_CTRL_VALUE_VIDEO_HDMIRX_REP_UHD_PROFILE
#define PolicyValueVideoPlayStreamMemoryProfileHDMINonRepeaterHD   STM_SE_CTRL_VALUE_VIDEO_HDMIRX_NON_REP_HD_PROFILE
#define PolicyValueVideoPlayStreamMemoryProfileHDMINonRepeaterUHD  STM_SE_CTRL_VALUE_VIDEO_HDMIRX_NON_REP_UHD_PROFILE
    PolicyVideoPlayStreamMemoryProfile,

    // Policy to allow manifestation of the first frame decoded early.
    PolicyManifestFirstFrameEarly,          // Apply/Disapply

    // Policy to force a null manifestation on drain and shutdown for video
    // Implicitly applied for StreamSwitch. Audio must always mute.
#define PolicyValueLeaveLastFrameOnScreen   STM_SE_CTRL_VALUE_LEAVE_LAST_FRAME_ON_SCREEN
#define PolicyValueBlankScreen              STM_SE_CTRL_VALUE_BLANK_SCREEN
    PolicyVideoLastFrameBehaviour,

    // Master clock mechanisms, define which clock is going to be used
    // to master mappings between system time and playback time
#define PolicyValueVideoClockMaster         STM_SE_CTRL_VALUE_VIDEO_CLOCK_MASTER
#define PolicyValueAudioClockMaster         STM_SE_CTRL_VALUE_AUDIO_CLOCK_MASTER
#define PolicyValueSystemClockMaster        STM_SE_CTRL_VALUE_SYSTEM_CLOCK_MASTER
    PolicyMasterClock,

    // Use externally specified mapping between playback time and system time (use for AVR fixed offset).
    PolicyExternalTimeMapping,              // Apply/Disapply
    PolicyExternalTimeMappingVsyncLocked,   // Apply/Disapply - only relevant if external mapping applied

    // Policy forcing the startup synchronization to work on
    // the basis of starting at the first video frame and
    // letting audio join in appropriately. this has particular
    // effect in some transport streams where there may be a
    // lead in time of up to 1 second of audio. Should only be set
    // if using the aggressive "PolicyDiscardLateFrames" policy.
    PolicyVideoStartImmediate,              // Apply/Disapply

    // Policy to Deploy some workarounds for odd behaviours, especially with
    // regard to time, that are typical of packet injectors
    PolicyPacketInjectorPlayback,           // Apply/Disapply

    // Trick mode decode controls, to allow Key frames
    // only, and only one group between discontinuities.
    PolicyStreamOnlyKeyFrames,              // Apply/Disapply

#define PolicyValueNoDiscard                STM_SE_CTRL_VALUE_NO_DISCARD
#define PolicyValueReferenceFramesOnly      STM_SE_CTRL_VALUE_REFERENCE_FRAMES_ONLY
#define PolicyValueKeyFramesOnly            STM_SE_CTRL_VALUE_KEY_FRAMES_ONLY
    PolicyStreamDiscardFrames,

    PolicyStreamSingleGroupBetweenDiscontinuities,         // Apply/Disapply

    // Policy controlling the discard of late frames,
    // if not set then late frames will be manifested,
    // and avd synchronization will be responsible for pulling the
    // stream back into line. If this is set, then any frame that
    // is too late for its manifestation time will be unceremoniously
    // junked. Changed to have a range of possibilities.
    // Never, always, and only after a synchronize until the first
    // frame arrives in time.
#define PolicyValueDiscardLateFramesNever                               STM_SE_CTRL_VALUE_DISCARD_LATE_FRAMES_NEVER
#define PolicyValueDiscardLateFramesAlways                              STM_SE_CTRL_VALUE_DISCARD_LATE_FRAMES_ALWAYS
#define PolicyValueDiscardLateFramesAfterSynchronize                    STM_SE_CTRL_VALUE_DISCARD_LATE_FRAMES_AFTER_SYNCHRONIZE
    PolicyDiscardLateFrames,

    // Two policies allowing the rebasing of the system clock
    // when we are struggling to decode or feed data in time.
    PolicyRebaseOnFailureToDeliverDataInTime,   // Apply/Disapply
    PolicyRebaseOnFailureToDecodeInTime,        // Apply/Disapply

    // Trick mode policy defining fast (forward or reverse) domains
    // Note these are ordered values do NOT change the order
    // code may well use greater than comparisons.
#define PolicyValueTrickModeAuto                                        STM_SE_CTRL_VALUE_TRICK_MODE_AUTO
#define PolicyValueTrickModeDecodeAll                                   STM_SE_CTRL_VALUE_TRICK_MODE_DECODE_ALL
#define PolicyValueTrickModeDecodeAllDegradeNonReferenceFrames          STM_SE_CTRL_VALUE_TRICK_MODE_DECODE_ALL_DEGRADE_NON_REFERENCE_FRAMES
#define PolicyValueTrickModeStartDiscardingNonReferenceFrames           STM_SE_CTRL_VALUE_TRICK_MODE_DISCARD_NON_REFERENCE_FRAMES
#define PolicyValueTrickModeDecodeReferenceFramesDegradeNonKeyFrames    STM_SE_CTRL_VALUE_TRICK_MODE_DECODE_REFERENCE_FRAMES_DEGRADE_NON_KEY_FRAMES
#define PolicyValueTrickModeDecodeKeyFrames                             STM_SE_CTRL_VALUE_TRICK_MODE_DECODE_KEY_FRAMES
#define PolicyValueTrickModeDiscontinuousKeyFrames                      STM_SE_CTRL_VALUE_TRICK_MODE_DISCONTINUOUS_KEY_FRAMES
    PolicyTrickModeDomain,

    // Allow the discard of B frames when running at normal speed, if the decode cannot keep up.
    PolicyAllowFrameDiscardAtNormalSpeed,           // Apply/Disapply

    // Policy controlling the limit at which we can pull the clocks rates
    // this policy is used to restrict the clock pulling software from
    // going crazy. The value N will force clock rate pulling to be limitted to
    // 2^N parts per million.
    PolicyClockPullingLimit2ToTheNPartsPerMillion,  // Apply/Disapply

    // set the forward jump threshold for the PTS jump detector as 2^N seconds
    PolicySymmetricJumpDetection,                       // Apply/Disapply
    PolicyPtsForwardJumpDetectionThreshold,             // N value

    // Policy to specify the variability in live playback latency in ms
    // this forces a playback to start latein order to buffer up the appropriate
    // latency variability period. The value is between 0 and 2550 ms (2.5s)
    PolicyLivePlaybackLatencyVariabilityMs,    // range 0..2550

    // Specify the HDMIRx mode.
#define PolicyValueHdmiRxModeDisabled                STM_SE_CTRL_VALUE_HDMIRX_DISABLED
#define PolicyValueHdmiRxModeRepeater                STM_SE_CTRL_VALUE_HDMIRX_REPEATER
#define PolicyValueHdmiRxModeNonRepeater             STM_SE_CTRL_VALUE_HDMIRX_NON_REPEATER
    PolicyHdmiRxMode,

    // Policy to signal the player that it is being run via a devlog
    PolicyRunningDevlog,                    // Apply/Disapply

    // policies not implemented (no policy reader)
    //
#define PolicyValueTrickAudioMute               STM_SE_CTRL_VALUE_TRICK_AUDIO_MUTE
#define PolicyValueTrickAudioPitchCorrected     STM_SE_CTRL_VALUE_TRICK_AUDIO_PITCH_CORRECTED
    PolicyTrickModeAudio,

    PolicyPlay24FPSVideoAt25FPS,                    // Apply/Disapply
    PolicyUsePTSDeducedDefaultFrameRates,           // Apply/Disapply

    // Policy that controls the application of DUAL-MONO channel-selection.
#define PolicyValueUserEnforcedDualMono     STM_SE_CTRL_VALUE_USER_ENFORCED_DUALMONO
#define PolicyValueStreamDrivenDualMono     STM_SE_CTRL_VALUE_STREAM_DRIVEN_DUALMONO
    PolicyAudioStreamDrivenDualMono,

    // Policy that controls dual mono mode
#define PolicyValueDualMonoStereoOut        STM_SE_STEREO_OUT
#define PolicyValueDualMonoLeftOut          STM_SE_DUAL_LEFT_OUT
#define PolicyValueDualMonoRightOut         STM_SE_DUAL_RIGHT_OUT
#define PolicyValueDualMonoMixLeftRightOut  STM_SE_MONO_OUT
    PolicyAudioDualMonoChannelSelection,

    // Policy that controls de-emphasis application
    PolicyAudioDeEmphasis,

    //
    PolicyNotImplemented,

    // keep last
    PolicyMaxPolicy
} PlayerPolicy_t;

//
// The event codes, and their enveloping mask
//

#define EventIllegalIdentifier                  0x0000000000000000ull
#define EventAllEvents                          0xffffffffffffffffull

// One-shot events
#define EventFirstFrameManifested               0x0000000000000001ull
// Free slot                                    0x0000000000000002ull
// Free slot                                    0x0000000000000004ull
#define EventStreamSwitched                     0x0000000000000008ull
#define EventStreamDrained                      0x0000000000000010ull
// Free slot                                    0x0000000000000020ull
// Free slot                                    0x0000000000000040ull
#define EventStreamUnPlayable                   0x0000000000000080ull
#define EventFailedToQueueBufferToDisplay       0x0000000000000100ull

// Free slot                                    0x0000000000000200ull
// Free slot                                    0x0000000000000400ull
// Free slot                                    0x0000000000000800ull
// Free slot                                    0x0000000000001000ull
// Free slot                                    0x0000000000002000ull
// Free slot                                    0x0000000000004000ull
// Free slot                                    0x0000000000008000ull
// Free slot                                    0x0000000000010000ull

#define EventVsyncOffsetMeasured                0x0000000000020000ull
#define EventFatalHardwareFailure               0x0000000000040000ull

#define EventFrameStarvation                    0x0000000000080000ull
#define EventFrameSupplied                      0x0000000000100000ull
// Ongoing events
// Free slot                                    0x0000000100000000ull
#define EventSourceVideoParametersChangeManifest    0x0000000200000000ull

// Free slot                                    0x0000000400000000ull
#define EventSourceFrameRateChangeManifest      0x0000000800000000ull

// Free slot                                    0x0000001000000000ull
// Free slot                                    0x0000002000000000ull
// Free slot                                    0x0000004000000000ull

#define EventFailedToDecodeInTime               0x0000008000000000ull
#define EventFailedToDeliverDataInTime          0x0000010000000000ull

#define EventTrickModeDomainChange              0x0000020000000000ull
// Free slot                                    0x0000040000000000ull

// Free slot                                    0x0000080000000000ull
#define EventNewFrameDisplayed                  0x0000100000000000ull
#define EventNewFrameDecoded                    0x0000200000000000ull
#define EventStreamInSync                       0x0000400000000000ull

#define EventAlarmParsedPts                     0x0000800000000000ull
#define EventSourceAudioParametersChange        0x0001000000000000ull

#define EventDiscarding                         0x0002000000000000ull
#define EventAlarmPts                           0x0004000000000000ull

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
    TimeFormatUs            = 0,
    TimeFormatPts
} PlayerTimeFormat_t;


typedef enum
{
    ChannelSelectStereo    = 0,
    ChannelSelectLeft,
    ChannelSelectRight,
    ChannelSelectMono
} PlayerChannelSelect_t;

//

// /////////////////////////////////////////////////////////////////////////
//
// The wrapper pointer types, here before the structures to
// allow for those self referential structures (such as lists),
// and incestuous references.
//

#ifdef __cplusplus

typedef class Collator_c                    *Collator_t;
typedef class FrameParser_c                 *FrameParser_t;
typedef class Codec_c                       *Codec_t;
typedef class OutputTimer_c                 *OutputTimer_t;
typedef class OutputCoordinator_c           *OutputCoordinator_t;
typedef class DecodeBufferManager_c         *DecodeBufferManager_t;
typedef class ManifestationCoordinator_c    *ManifestationCoordinator_t;
typedef class Manifestor_c                  *Manifestor_t;
typedef class Player_c                      *Player_t;
typedef class UserDataSource_c              *UserDataSource_t;

#ifdef UNITTESTS
typedef class PlayerStreamInterface_c       *PlayerStream_t;
typedef struct PlayerPlaybackInterface_c      *PlayerPlayback_t;
#else
typedef class PlayerStream_c                *PlayerStream_t;
typedef struct PlayerPlayback_s             *PlayerPlayback_t;
#endif
typedef class HavanaStream_c                *HavanaStream_t;
#endif // __cplusplus

#define PlayerAllPlaybacks  NULL
#define PlayerAllStreams    NULL

// /////////////////////////////////////////////////////////////////////////
//
// The structures
//

// -----------------------------------------------------------
// The event record
//

#ifdef __cplusplus
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
        bool                       Bool;
    } Value[13];

    // ExtraValue is used to notify codec specific parameters
    union
    {
        unsigned int               UnsignedInt;
        unsigned long long         LongLong;
        void                      *Pointer;
        bool                       Bool;
    } ExtraValue[1];

    //
    // Since rationals are a class, we need to separate them from the union and make them specific
    //

    Rational_t                     Rational;

    void                          *UserData;

    unsigned int                   Reason;

    PlayerEventRecord_s(void)
        : Code()
        , Playback(0)
        , Stream(NULL)
        , PlaybackTime(0)
        , Value()
        , ExtraValue()
        , Rational()
        , UserData(NULL)
        , Reason(0)
    {}
} PlayerEventRecord_t;
#endif //cplusplus

//

// ---------------------------------------------------------
//  The statistical data
//

typedef struct StatisticFields_s
{
    unsigned int        Count;
    unsigned long long  Total;
    unsigned long long  Longest;
    unsigned long long  Shortest;
} StatisticFields_t;

typedef struct BufferPoolLevel_s
{
    unsigned int        BuffersInPool;
    unsigned int        BuffersWithNonZeroReferenceCount;
    unsigned int        MemoryInPool;
    unsigned int        MemoryAllocated;
    unsigned int        MemoryInUse;
    unsigned int        LargestFreeMemoryBlock;
} BufferPoolLevel;

typedef struct PlayerStreamStatistics_s
{
    unsigned int        Count;
    StatisticFields_t   DeltaEntryIntoProcess0;
    StatisticFields_t   DeltaEntryIntoProcess1;
    StatisticFields_t   DeltaEntryIntoProcess2;
    StatisticFields_t   DeltaEntryIntoProcess3;

    StatisticFields_t   Traverse0To1;
    StatisticFields_t   Traverse1To2;
    StatisticFields_t   Traverse2To3;

    StatisticFields_t   FrameTimeInProcess1;
    StatisticFields_t   FrameTimeInProcess2;
    StatisticFields_t   TotalTraversalTime;

    unsigned int        FrameCountLaunchedDecode;
    unsigned int        VidFrameCountLaunchedDecodeI;
    unsigned int        VidFrameCountLaunchedDecodeP;
    unsigned int        VidFrameCountLaunchedDecodeB;

    unsigned int        FrameCountDecoded;

    unsigned int        FrameCountManifested;
    unsigned int        FrameCountToManifestor;
    unsigned int        FrameCountFromManifestor;

    unsigned int        FrameParserError;
    unsigned int        FrameParserNoStreamParametersError;
    unsigned int        FrameParserPartialFrameParametersError;
    unsigned int        FrameParserUnhandledHeaderError;
    unsigned int        FrameParserHeaderSyntaxError;
    unsigned int        FrameParserHeaderUnplayableError;
    unsigned int        FrameParserStreamSyntaxError;
    unsigned int        FrameParserFailedToCreateReversePlayStacksError;
    unsigned int        FrameParserFailedToAllocateBufferError;
    unsigned int        FrameParserReferenceListConstructionDeferredError;
    unsigned int        FrameParserInsufficientReferenceFramesError;
    unsigned int        FrameParserStreamUnplayableError;

    unsigned int        FrameDecodeError;
    unsigned int        FrameDecodeMBOverflowError;
    unsigned int        FrameDecodeRecoveredError;
    unsigned int        FrameDecodeNotRecoveredError;
    unsigned int        FrameDecodeErrorTaskTimeOutError;

    unsigned int        FrameCodecError;
    unsigned int        FrameCodecUnknownFrameError;

    unsigned int        DroppedBeforeDecodeWindowSingleGroupPlayback;
    unsigned int        DroppedBeforeDecodeWindowKeyFramesOnly;
    unsigned int        DroppedBeforeDecodeWindowOutsidePresentationInterval;
    unsigned int        DroppedBeforeDecodeWindowTrickModeNotSupported;
    unsigned int        DroppedBeforeDecodeWindowTrickMode;
    unsigned int        DroppedBeforeDecodeWindowOthers;
    unsigned int        DroppedBeforeDecodeWindowTotal;

    unsigned int        DroppedBeforeDecodeSingleGroupPlayback;
    unsigned int        DroppedBeforeDecodeKeyFramesOnly;
    unsigned int        DroppedBeforeDecodeOutsidePresentationInterval;
    unsigned int        DroppedBeforeDecodeTrickModeNotSupported;
    unsigned int        DroppedBeforeDecodeTrickMode;
    unsigned int        DroppedBeforeDecodeOthers;
    unsigned int        DroppedBeforeDecodeTotal;

    unsigned int        DroppedBeforeOutputTimingOutsidePresentationInterval;
    unsigned int        DroppedBeforeOutputTimingOthers;
    unsigned int        DroppedBeforeOutputTimingTotal;

    unsigned int        DroppedBeforeManifestationUntimed;
    unsigned int        DroppedBeforeManifestationTooLateForManifestation;
    unsigned int        DroppedBeforeManifestationTrickModeNotSupported;
    unsigned int        DroppedBeforeManifestationOthers;
    unsigned int        DroppedBeforeManifestationTotal;

    unsigned int        VideoFrameRateIntegerPart;
    unsigned int        VideoFrameRateRemainderDecimal;

    unsigned int        BufferCountFromCollator;
    unsigned int        BufferCountToFrameParser;

    unsigned int        CollatorAudioElementrySyncLostCount;
    unsigned int        CollatorAudioPesSyncLostCount;

    unsigned int        FrameCountFromFrameParser;
    unsigned int        FrameParserAudioSampleRate;
    unsigned int        FrameParserAudioFrameSize;

    unsigned int        FrameCountToCodec;
    unsigned int        FrameCountFromCodec;
    unsigned int        CodecAudioCodingMode;
    unsigned int        CodecAudioSamplingFrequency;
    unsigned int        CodecAudioNumOfOutputSamples;
    unsigned int        ManifestorAudioMixerStarved;

    unsigned long       MaxVideoHwDecodeTime;
    unsigned long       MinVideoHwDecodeTime;
    unsigned long       AvgVideoHwDecodeTime;

    BufferPoolLevel     CodedFrameBufferPool;
    BufferPoolLevel     DecodeBufferPool;

    unsigned int        H264PreprocErrorSCDetected;
    unsigned int        H264PreprocErrorBitInserted;
    unsigned int        H264PreprocIntBufferOverflow;
    unsigned int        H264PreprocBitBufferUnderflow;
    unsigned int        H264PreprocBitBufferOverflow;
    unsigned int        H264PreprocReadErrors;
    unsigned int        H264PreprocWriteErrors;

    unsigned int        HevcPreprocErrorSCDetected;
    unsigned int        HevcPreprocErrorEOS;
    unsigned int        HevcPreprocErrorEndOfDma;
    unsigned int        HevcPreprocErrorRange;
    unsigned int        HevcPreprocErrorEntropyDecode;

    int                 OutputRateGradient[MAXIMUM_MANIFESTATION_TIMING_COUNT];
    unsigned int        OutputRateFramesToIntegrateOver[MAXIMUM_MANIFESTATION_TIMING_COUNT];
    unsigned int        OutputRateIntegrationCount[MAXIMUM_MANIFESTATION_TIMING_COUNT];
    int                 OutputRateClockAdjustment[MAXIMUM_MANIFESTATION_TIMING_COUNT];

    unsigned long long  SystemTime;
    unsigned long long  PresentationTime;
    unsigned long long  Pts;
    unsigned long long  SyncError;
    unsigned long long  OutputSyncError0;
    unsigned long long  OutputSyncError1;

    unsigned int        CodecFrameLength;
    unsigned int        CodecNumberOfOutputChannels;
    unsigned int        DolbyPulseIDCount;
    unsigned int        DolbyPulseSBRPresent;
    unsigned int        DolbyPulsePSPresent;
} PlayerStreamStatistics_t;

typedef struct PlayerPlaybackStatistics_s
{
    unsigned int        ClockRecoveryAccumulatedPoints;
    int                 ClockRecoveryClockAdjustment;
    unsigned int        ClockRecoveryCummulativeDiscardedPoints;
    unsigned int        ClockRecoveryCummulativeDiscardResets;
    int                 ClockRecoveryActualGradient;
    int                 ClockRecoveryEstablishedGradient;
    unsigned int        ClockRecoveryIntegrationTimeWindow;
    unsigned int        ClockRecoveryIntegrationTimeElapsed;
} PlayerPlaybackStatistics_t;

// -----------------------------------------------------------
// The attribute descriptor
//

#define SYSFS_ATTRIBUTE_ID_NA                   0
#define SYSFS_ATTRIBUTE_ID_BOOL                 SYSFS_ATTRIBUTE_ID_NA + 1
#define SYSFS_ATTRIBUTE_ID_INTEGER              SYSFS_ATTRIBUTE_ID_BOOL + 1
#define SYSFS_ATTRIBUTE_ID_CONSTCHARPOINTER     SYSFS_ATTRIBUTE_ID_INTEGER + 1
#define SYSFS_ATTRIBUTE_ID_UNSIGNEDLONGLONGINT  SYSFS_ATTRIBUTE_ID_CONSTCHARPOINTER +1

typedef struct PlayerAttributeDescriptor_s
{
    int                          Id;

    union
    {
        const char              *ConstCharPointer;
        int                      Int;
        unsigned long long int   UnsignedLongLongInt;
        bool                     Bool;
    } u;

} PlayerAttributeDescriptor_t;

// ---------------------------------------------------------
//  The attributes data
//
typedef struct PlayerStreamAttributes_s
{
    PlayerAttributeDescriptor_t input_format;
    PlayerAttributeDescriptor_t decode_errors;
    PlayerAttributeDescriptor_t number_channels;
    PlayerAttributeDescriptor_t sample_frequency;
    PlayerAttributeDescriptor_t number_of_samples_processed;
    PlayerAttributeDescriptor_t supported_input_format;
} PlayerStreamAttributes_t;

#endif
