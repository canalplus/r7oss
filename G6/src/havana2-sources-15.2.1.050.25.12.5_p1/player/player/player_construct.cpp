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

#include "player_version.h"
#include "player_generic.h"
#include "player_playback.h"
#include "player_stream.h"

#undef TRACE_TAG
#define TRACE_TAG "Player_Generic_c"

#define TUNEABLE_NAME_AUDIO_ENABLE_COPROCESSOR_PARSING   "audio_enable_coprocessor_parser"

// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Constructor function - Initialize our data
//

// The coprocessor is capable of doing the frame-parsing for some codec (e.g AAC) .
// This switch disable this coprocessing by default.
unsigned int volatile BaseComponentClass_c::EnableCoprocessorParsing = 0;

Player_Generic_c::Player_Generic_c(void)
    : Lock()
    , BufferCodedFrameBufferType()
    , ShutdownPlayer(false)
    , BufferManager(NULL)
    , BufferInputBufferType()
    , PlayerControlStructurePool(NULL)
    , InputBufferPool(NULL)
    , ListOfPlaybacks(NULL)
    , PolicyRecord()
    , ControlsRecord()
    , AudioCodedFrameCount(PLAYER_AUDIO_DEFAULT_CODED_FRAME_COUNT)
    , AudioCodedMemorySize(PLAYER_AUDIO_DEFAULT_CODED_MEMORY_SIZE)
    , AudioCodedFrameMaximumSize(PLAYER_AUDIO_DEFAULT_CODED_FRAME_MAXIMUM_SIZE)
    , AudioCodedMemoryPartitionName()
    , VideoCodedFrameCount(PLAYER_VIDEO_DEFAULT_CODED_FRAME_COUNT)
    , VideoCodedMemorySize(PLAYER_VIDEO_DEFAULT_HD_CODED_MEMORY_SIZE)
    , VideoCodedFrameMaximumSize(PLAYER_VIDEO_DEFAULT_HD_CODED_FRAME_MAXIMUM_SIZE)
    , VideoCodedMemoryPartitionName()
    , OtherCodedFrameCount(PLAYER_OTHER_DEFAULT_CODED_FRAME_COUNT)
    , OtherCodedMemorySize(PLAYER_OTHER_DEFAULT_CODED_MEMORY_SIZE)
    , OtherCodedFrameMaximumSize(PLAYER_OTHER_DEFAULT_CODED_FRAME_MAXIMUM_SIZE)
    , OtherCodedMemoryPartitionName()
    , ArchivedReservedMemoryBlocks()
    , LowPowerMode(STM_SE_LOW_POWER_MODE_HPS)
#ifdef LOWMEMORYBANDWIDTH
    , mSemMemBWLimiter()
    , mNbStreams(0)
#endif
{
    SE_INFO(group_player, "setting up Player2 - version %s\n", PLAYER_VERSION);

    OS_InitializeMutex(&Lock);
#ifdef LOWMEMORYBANDWIDTH
    OS_SemaphoreInitialize(&mSemMemBWLimiter, 1);
#endif

    strncpy(AudioCodedMemoryPartitionName, PLAYER_AUDIO_DEFAULT_CODED_FRAME_PARTITION_NAME,
            sizeof(AudioCodedMemoryPartitionName));
    AudioCodedMemoryPartitionName[sizeof(AudioCodedMemoryPartitionName) - 1] = '\0';

    strncpy(VideoCodedMemoryPartitionName, PLAYER_VIDEO_DEFAULT_CODED_FRAME_PARTITION_NAME,
            sizeof(VideoCodedMemoryPartitionName));
    VideoCodedMemoryPartitionName[sizeof(VideoCodedMemoryPartitionName) - 1] = '\0';

    strncpy(OtherCodedMemoryPartitionName, PLAYER_OTHER_DEFAULT_CODED_FRAME_PARTITION_NAME,
            sizeof(OtherCodedMemoryPartitionName));
    OtherCodedMemoryPartitionName[sizeof(OtherCodedMemoryPartitionName) - 1] = '\0';

    //
    SetPolicy(PlayerAllPlaybacks, PlayerAllStreams, (PlayerPolicy_t)PolicyPlayoutAlwaysPlayout,  PolicyValuePlayout);
    SetPolicy(PlayerAllPlaybacks, PlayerAllStreams, (PlayerPolicy_t)PolicyPlayoutAlwaysDiscard,  PolicyValueDiscard);
    SetPolicy(PlayerAllPlaybacks, PlayerAllStreams, (PlayerPolicy_t)PolicyStatisticsOnAudio,  PolicyValueDisapply);
    SetPolicy(PlayerAllPlaybacks, PlayerAllStreams, (PlayerPolicy_t)PolicyStatisticsOnVideo,  PolicyValueDisapply);
    //
    SetPolicy(PlayerAllPlaybacks, PlayerAllStreams, PolicyLivePlayback,        PolicyValueDisapply);
    SetPolicy(PlayerAllPlaybacks, PlayerAllStreams, PolicyAVDSynchronization,  PolicyValueApply);
    // player, playback streams
    SetPolicy(PlayerAllPlaybacks, PlayerAllStreams, PolicyClampPlaybackIntervalOnPlaybackDirectionChange,  PolicyValueApply);
    SetPolicy(PlayerAllPlaybacks, PlayerAllStreams, PolicyPlayoutOnTerminate,          PolicyValuePlayout);
    /* By default, stream switch policy if set to PolicyValueDiscard to avoid memory overlapping in parsed buffer
       (i.e. parser starting to early on still-used data by codec) -> PolicyPlayoutOnSwitch MUST be PolicyValueDiscard */
    SetPolicy(PlayerAllPlaybacks, PlayerAllStreams, PolicyPlayoutOnSwitch,             PolicyValueDiscard);
    SetPolicy(PlayerAllPlaybacks, PlayerAllStreams, PolicyPlayoutOnDrain,              PolicyValuePlayout);
    SetPolicy(PlayerAllPlaybacks, PlayerAllStreams, PolicyPlayoutOnInternalDrain,      PolicyValueDiscard);
    SetPolicy(PlayerAllPlaybacks, PlayerAllStreams, PolicyIgnoreStreamUnPlayableCalls, PolicyValueApply);
    SetPolicy(PlayerAllPlaybacks, PlayerAllStreams, PolicyDecodeBuffersUserAllocated,  PolicyValueDisapply);
    // collators
    SetPolicy(PlayerAllPlaybacks, PlayerAllStreams, PolicyLimitInputInjectAhead,               PolicyValueApply);
    SetPolicy(PlayerAllPlaybacks, PlayerAllStreams, PolicyControlDataDetection,                PolicyValueApply);
    SetPolicy(PlayerAllPlaybacks, PlayerAllStreams, PolicyReduceCollatedData,                  PolicyValueDisapply);
    SetPolicy(PlayerAllPlaybacks, PlayerAllStreams, PolicyOperateCollator2InReversibleMode,    PolicyValueDisapply);
    SetPolicy(PlayerAllPlaybacks, PlayerAllStreams, PolicyMonitorPCRTiming,                    PolicyValueDisapply);
    // frameparsers
    SetPolicy(PlayerAllPlaybacks, PlayerAllStreams, PolicyMPEG2DoNotHonourProgressiveFrameFlag,    PolicyValueApply);
    SetPolicy(PlayerAllPlaybacks, PlayerAllStreams, PolicyMPEG2ApplicationType,                    PolicyValueMPEG2ApplicationDvb);
    SetPolicy(PlayerAllPlaybacks, PlayerAllStreams, PolicyH264TreatDuplicateDpbValuesAsNonReferenceFrameFirst, PolicyValueApply);
    SetPolicy(PlayerAllPlaybacks, PlayerAllStreams, PolicyH264ForcePicOrderCntIgnoreDpbDisplayFrameOrdering,   PolicyValueApply);
    SetPolicy(PlayerAllPlaybacks, PlayerAllStreams, PolicyH264ValidateDpbValuesAgainstPTSValues,               PolicyValueApply);
    SetPolicy(PlayerAllPlaybacks, PlayerAllStreams, PolicyH264TreatTopBottomPictureStructAsInterlaced,         PolicyValueDisapply);
    SetPolicy(PlayerAllPlaybacks, PlayerAllStreams, PolicyH264AllowNonIDRResynchronization,                    PolicyValueApply);
    SetPolicy(PlayerAllPlaybacks, PlayerAllStreams, PolicyFrameRateCalculationPrecedence, PolicyValuePrecedencePtsStreamContainerDefault);
    SetPolicy(PlayerAllPlaybacks, PlayerAllStreams, PolicyContainerFrameRate, 0);
    // codecs
    SetPolicy(PlayerAllPlaybacks, PlayerAllStreams, PolicyAllowBadPreProcessedFrames,   PolicyValueApply);
    SetPolicy(PlayerAllPlaybacks, PlayerAllStreams, PolicyAllowBadHevcPreProcessedFrames,   PolicyValueDisapply);
    SetPolicy(PlayerAllPlaybacks, PlayerAllStreams, PolicyVideoPlayStreamMemoryProfile, PolicyValueVideoPlayStreamMemoryProfileHDOptimized);
    SetPolicy(PlayerAllPlaybacks, PlayerAllStreams, PolicyErrorDecodingLevel,          PolicyValuePolicyErrorDecodingLevelMaximum);
    SetPolicy(PlayerAllPlaybacks, PlayerAllStreams, PolicyAudioApplicationType,        PolicyValueAudioApplicationIso);
    SetPolicy(PlayerAllPlaybacks, PlayerAllStreams, PolicyAudioServiceType,            PolicyValueAudioServiceMain);
    SetPolicy(PlayerAllPlaybacks, PlayerAllStreams, PolicyRegionType,                  PolicyValueRegionDVB);
    SetPolicy(PlayerAllPlaybacks, PlayerAllStreams, PolicyAudioProgramReferenceLevel,  0);   //Default level is set to 0 to let
    //the fw decide based upon the stream
    SetPolicy(PlayerAllPlaybacks, PlayerAllStreams, PolicyAudioSubstreamId,            0);
    SetPolicy(PlayerAllPlaybacks, PlayerAllStreams, PolicyAllowReferenceFrameSubstitution,         PolicyValueApply);
    SetPolicy(PlayerAllPlaybacks, PlayerAllStreams, PolicyDiscardForReferenceQualityThreshold,     0);
    SetPolicy(PlayerAllPlaybacks, PlayerAllStreams, PolicyDiscardForManifestationQualityThreshold, 0);
    // codecs and manifestors
    SetPolicy(PlayerAllPlaybacks, PlayerAllStreams, PolicyDecimateDecoderOutput, PolicyValueDecimateDecoderOutputHalf);
    // manifestors
    SetPolicy(PlayerAllPlaybacks, PlayerAllStreams, PolicyManifestFirstFrameEarly, PolicyValueApply);
    SetPolicy(PlayerAllPlaybacks, PlayerAllStreams, PolicyVideoLastFrameBehaviour, PolicyValueLeaveLastFrameOnScreen);
    // output timer and coordinator
    SetPolicy(PlayerAllPlaybacks, PlayerAllStreams, PolicyMasterClock,                     PolicyValueVideoClockMaster);
    SetPolicy(PlayerAllPlaybacks, PlayerAllStreams, PolicyExternalTimeMapping,             PolicyValueDisapply);
    SetPolicy(PlayerAllPlaybacks, PlayerAllStreams, PolicyExternalTimeMappingVsyncLocked,  PolicyValueDisapply);
    SetPolicy(PlayerAllPlaybacks, PlayerAllStreams, PolicyVideoStartImmediate,     PolicyValueApply);
    SetPolicy(PlayerAllPlaybacks, PlayerAllStreams, PolicyPacketInjectorPlayback,  PolicyValueApply);
    // output timer
    SetPolicy(PlayerAllPlaybacks, PlayerAllStreams, PolicyStreamOnlyKeyFrames,     PolicyValueDisapply);
    SetPolicy(PlayerAllPlaybacks, PlayerAllStreams, PolicyStreamDiscardFrames,     PolicyValueNoDiscard);
    SetPolicy(PlayerAllPlaybacks, PlayerAllStreams, PolicyStreamSingleGroupBetweenDiscontinuities,  PolicyValueDisapply);
    SetPolicy(PlayerAllPlaybacks, PlayerAllStreams, PolicyDiscardLateFrames,       PolicyValueDiscardLateFramesAfterSynchronize);
    SetPolicy(PlayerAllPlaybacks, PlayerAllStreams, PolicyRebaseOnFailureToDeliverDataInTime,  PolicyValueApply);
    SetPolicy(PlayerAllPlaybacks, PlayerAllStreams, PolicyRebaseOnFailureToDecodeInTime,       PolicyValueApply);
    SetPolicy(PlayerAllPlaybacks, PlayerAllStreams, PolicyTrickModeDomain,         PolicyValueTrickModeAuto);
    SetPolicy(PlayerAllPlaybacks, PlayerAllStreams, PolicyAllowFrameDiscardAtNormalSpeed,      PolicyValueDisapply);
    // output coordinator
    SetPolicy(PlayerAllPlaybacks, PlayerAllStreams, PolicyClockPullingLimit2ToTheNPartsPerMillion, 8);
    SetPolicy(PlayerAllPlaybacks, PlayerAllStreams, PolicySymmetricJumpDetection,                  PolicyValueApply);
    SetPolicy(PlayerAllPlaybacks, PlayerAllStreams, PolicyPtsForwardJumpDetectionThreshold,        4);
    SetPolicy(PlayerAllPlaybacks, PlayerAllStreams, PolicyLivePlaybackLatencyVariabilityMs,        0);
    SetPolicy(PlayerAllPlaybacks, PlayerAllStreams, PolicyHdmiRxMode,                              PolicyValueHdmiRxModeDisabled);
    // output coordinator, collator
    SetPolicy(PlayerAllPlaybacks, PlayerAllStreams, PolicyRunningDevlog,   PolicyValueDisapply);
    // not implemented (no policy reader)
    SetPolicy(PlayerAllPlaybacks, PlayerAllStreams, PolicyTrickModeAudio,  PolicyValueTrickAudioMute);
    SetPolicy(PlayerAllPlaybacks, PlayerAllStreams, PolicyPlay24FPSVideoAt25FPS,           PolicyValueDisapply);
    SetPolicy(PlayerAllPlaybacks, PlayerAllStreams, PolicyUsePTSDeducedDefaultFrameRates,  PolicyValueApply);
    // audio
    SetPolicy(PlayerAllPlaybacks, PlayerAllStreams, PolicyAudioStreamDrivenDualMono,       PolicyValueStreamDrivenDualMono);
    SetPolicy(PlayerAllPlaybacks, PlayerAllStreams, PolicyAudioDualMonoChannelSelection,   PolicyValueDualMonoStereoOut);
    SetPolicy(PlayerAllPlaybacks, PlayerAllStreams, PolicyAudioDeEmphasis,                 PolicyValueDisapply);
    //
    // Decoder instrumentation / certification support
    //
    Codec_c::RegisterTuneable();
    //
    // Mixer instrumentation support
    //
    Manifestor_c::RegisterTuneable();
    //
    // do not add normal initialization after this point
    //
    OS_RegisterTuneable(TUNEABLE_NAME_AUDIO_ENABLE_COPROCESSOR_PARSING, (unsigned int *) & (BaseComponentClass_c::EnableCoprocessorParsing));
}

// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Destructor function - close down
//

Player_Generic_c::~Player_Generic_c(void)
{
    ShutdownPlayer = true;
    SE_INFO(group_player, "Releasing Player2\n");

    OS_UnregisterTuneable(TUNEABLE_NAME_AUDIO_ENABLE_COPROCESSOR_PARSING);
    Manifestor_c::UnregisterTuneable();
    Codec_c::UnregisterTuneable();

    ResetArchive(true);

#ifdef LOWMEMORYBANDWIDTH
    OS_SemaphoreTerminate(&mSemMemBWLimiter);
#endif
    OS_TerminateMutex(&Lock);
}

#ifdef LOWMEMORYBANDWIDTH
OS_Status_t Player_Generic_c::getSemMemBWLimiter(unsigned int timeout)
{
    return OS_SemaphoreWaitTimeout(&mSemMemBWLimiter, timeout);
}

void Player_Generic_c::releaseSemMemBWLimiter()
{
    OS_SemaphoreSignal(&mSemMemBWLimiter);
}
#endif
