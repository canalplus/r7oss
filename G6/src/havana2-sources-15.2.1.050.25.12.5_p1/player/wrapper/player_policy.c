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

/*
 * WARNING
 *
 * This file is implement in C rather than C++ in order to take
 * advantage of C99 designated array initializers (as a means to get
 * compile time alignment checking for the lookup tables).
 */

#include <stm_registry.h>

#include "player_types.h"
#include "stm_se.h"

#ifndef lengthof
#define lengthof(x) ((int) (sizeof(x) / sizeof(x[0])))
#endif

//{{{ SE control conversion to player policy
const PlayerPolicy_t StmMapOption [] =
{
    [STM_SE_CTRL_LIVE_PLAY] = PolicyLivePlayback,
    [STM_SE_CTRL_ENABLE_SYNC] = PolicyAVDSynchronization,
    // player, playback streams
    [STM_SE_CTRL_CLAMP_PLAY_INTERVAL_ON_DIRECTION_CHANGE] = PolicyClampPlaybackIntervalOnPlaybackDirectionChange,
    [STM_SE_CTRL_PLAYOUT_ON_TERMINATE] = PolicyPlayoutOnTerminate,
    [STM_SE_CTRL_PLAYOUT_ON_SWITCH] = PolicyPlayoutOnSwitch,
    [STM_SE_CTRL_PLAYOUT_ON_DRAIN] = PolicyPlayoutOnDrain,
    [STM_SE_CTRL_IGNORE_STREAM_UNPLAYABLE_CALLS] = PolicyIgnoreStreamUnPlayableCalls,
    [STM_SE_CTRL_DECODE_BUFFERS_USER_ALLOCATED] = PolicyDecodeBuffersUserAllocated,
    // collators
    [STM_SE_CTRL_LIMIT_INJECT_AHEAD] = PolicyLimitInputInjectAhead,
    [STM_SE_CTRL_ENABLE_CONTROL_DATA_DETECTION] = PolicyControlDataDetection,
    [STM_SE_CTRL_REDUCE_COLLATED_DATA] = PolicyReduceCollatedData,
    [STM_SE_CTRL_OPERATE_COLLATOR2_IN_REVERSIBLE_MODE] = PolicyOperateCollator2InReversibleMode,
    // frameparsers
    [STM_SE_CTRL_MPEG2_IGNORE_PROGRESSIVE_FRAME_FLAG] = PolicyMPEG2DoNotHonourProgressiveFrameFlag,
    [STM_SE_CTRL_MPEG2_APPLICATION_TYPE] = PolicyMPEG2ApplicationType,
    [STM_SE_CTRL_H264_TREAT_DUPLICATE_DPB_VALUES_AS_NON_REF_FRAME_FIRST] = PolicyH264TreatDuplicateDpbValuesAsNonReferenceFrameFirst,
    [STM_SE_CTRL_H264_FORCE_PIC_ORDER_CNT_IGNORE_DPB_DISPLAY_FRAME_ORDERING] = PolicyH264ForcePicOrderCntIgnoreDpbDisplayFrameOrdering,
    [STM_SE_CTRL_H264_VALIDATE_DPB_VALUES_AGAINST_PTS_VALUES] = PolicyH264ValidateDpbValuesAgainstPTSValues,
    [STM_SE_CTRL_H264_TREAT_TOP_BOTTOM_PICTURE_AS_INTERLACED] = PolicyH264TreatTopBottomPictureStructAsInterlaced,
    [STM_SE_CTRL_H264_ALLOW_NON_IDR_RESYNCHRONIZATION] = PolicyH264AllowNonIDRResynchronization,
    [STM_SE_CTRL_FRAME_RATE_CALCULUATION_PRECEDENCE] = PolicyFrameRateCalculationPrecedence,
    [STM_SE_CTRL_CONTAINER_FRAMERATE] = PolicyContainerFrameRate,
    // codecs
    [STM_SE_CTRL_HEVC_ALLOW_BAD_PREPROCESSED_FRAMES] = PolicyAllowBadHevcPreProcessedFrames,
    [STM_SE_CTRL_H264_ALLOW_BAD_PREPROCESSED_FRAMES] = PolicyAllowBadPreProcessedFrames,
    [STM_SE_CTRL_VIDEO_DECODE_MEMORY_PROFILE] = PolicyVideoPlayStreamMemoryProfile,
    [STM_SE_CTRL_ERROR_DECODING_LEVEL] = PolicyErrorDecodingLevel,
    [STM_SE_CTRL_AUDIO_APPLICATION_TYPE] = PolicyAudioApplicationType,
    [STM_SE_CTRL_AUDIO_SERVICE_TYPE]     = PolicyAudioServiceType,
    [STM_SE_CTRL_REGION] = PolicyRegionType,
    [STM_SE_CTRL_AUDIO_PROGRAM_REFERENCE_LEVEL] = PolicyAudioProgramReferenceLevel,
    [STM_SE_CTRL_AUDIO_SUBSTREAM_ID]     = PolicyAudioSubstreamId,
    [STM_SE_CTRL_ALLOW_REFERENCE_FRAME_SUBSTITUTION] = PolicyAllowReferenceFrameSubstitution,
    [STM_SE_CTRL_DISCARD_FOR_REFERENCE_QUALITY_THRESHOLD] = PolicyDiscardForReferenceQualityThreshold,
    [STM_SE_CTRL_DISCARD_FOR_MANIFESTATION_QUALITY_THRESHOLD] = PolicyDiscardForManifestationQualityThreshold,
    // codecs and manifestors
    [STM_SE_CTRL_DECIMATE_DECODER_OUTPUT] = PolicyDecimateDecoderOutput,
    // manifestors
    [STM_SE_CTRL_DISPLAY_FIRST_FRAME_EARLY] = PolicyManifestFirstFrameEarly,
    [STM_SE_CTRL_VIDEO_LAST_FRAME_BEHAVIOUR] = PolicyVideoLastFrameBehaviour,
    // output timer and coordinator
    [STM_SE_CTRL_MASTER_CLOCK] = PolicyMasterClock,
    [STM_SE_CTRL_EXTERNAL_TIME_MAPPING] = PolicyExternalTimeMapping,
    [STM_SE_CTRL_EXTERNAL_TIME_MAPPING_VSYNC_LOCKED] = PolicyExternalTimeMappingVsyncLocked,
    [STM_SE_CTRL_VIDEO_START_IMMEDIATE] = PolicyVideoStartImmediate,
    [STM_SE_CTRL_PACKET_INJECTOR] = PolicyPacketInjectorPlayback,
    // output timer
    [STM_SE_CTRL_VIDEO_KEY_FRAMES_ONLY] = PolicyStreamOnlyKeyFrames,
    [STM_SE_CTRL_VIDEO_DISCARD_FRAMES] = PolicyStreamDiscardFrames,
    [STM_SE_CTRL_VIDEO_SINGLE_GOP_UNTIL_NEXT_DISCONTINUITY] = PolicyStreamSingleGroupBetweenDiscontinuities,
    [STM_SE_CTRL_DISCARD_LATE_FRAMES] = PolicyDiscardLateFrames,
    [STM_SE_CTRL_REBASE_ON_DATA_DELIVERY_LATE] = PolicyRebaseOnFailureToDeliverDataInTime,
    [STM_SE_CTRL_ALLOW_REBASE_AFTER_LATE_DECODE] = PolicyRebaseOnFailureToDecodeInTime,
    [STM_SE_CTRL_TRICK_MODE_DOMAIN] = PolicyTrickModeDomain,
    [STM_SE_CTRL_ALLOW_FRAME_DISCARD_AT_NORMAL_SPEED] = PolicyAllowFrameDiscardAtNormalSpeed,
    // output coordinator
    [STM_SE_CTRL_CLOCK_RATE_ADJUSTMENT_LIMIT] = PolicyClockPullingLimit2ToTheNPartsPerMillion,
    [STM_SE_CTRL_SYMMETRIC_JUMP_DETECTION] = PolicySymmetricJumpDetection,
    [STM_SE_CTRL_SYMMETRIC_PTS_FORWARD_JUMP_DETECTION_THRESHOLD] = PolicyPtsForwardJumpDetectionThreshold,
    [STM_SE_CTRL_PLAYBACK_LATENCY] = PolicyLivePlaybackLatencyVariabilityMs,
    [STM_SE_CTRL_HDMI_RX_MODE]     = PolicyHdmiRxMode,
    // output coordinator, collator
    [STM_SE_CTRL_RUNNING_DEVLOG] = PolicyRunningDevlog,
    // not implemented (no policy reader)
    [STM_SE_CTRL_TRICK_MODE_AUDIO] = PolicyTrickModeAudio,
    [STM_SE_CTRL_PLAY_24FPS_VIDEO_AT_25FPS] = PolicyPlay24FPSVideoAt25FPS,
    [STM_SE_CTRL_USE_PTS_DEDUCED_DEFAULT_FRAME_RATES] = PolicyUsePTSDeducedDefaultFrameRates,
    // audio
    [STM_SE_CTRL_STREAM_DRIVEN_DUALMONO] = PolicyAudioStreamDrivenDualMono,
    [STM_SE_CTRL_DUALMONO] = PolicyAudioDualMonoChannelSelection,
    [STM_SE_CTRL_DEEMPHASIS] = PolicyAudioDeEmphasis,
    // top
    [STM_SE_CTRL_MAX_PLAYER_OPTION] = NonCascadedControls
};
//}}}

unsigned int StmMapOptionMax = lengthof(StmMapOption);

