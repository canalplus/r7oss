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

#include <stdarg.h>
#include "player_generic.h"
#include "player_playback.h"
#include "player_stream.h"
#include "collate_to_parse_edge.h"
#include "parse_to_decode_edge.h"
#include "decode_to_manifest_edge.h"
#include "post_manifest_edge.h"

#undef TRACE_TAG
#define TRACE_TAG "Player_Generic_c"

// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Useful defines/macros that need not be user visible
//

// Used for human readable policy reporting
static const char *LookupPlayerPolicy(PlayerPolicy_t Policy)
{
    switch ((unsigned int)Policy)
    {
#define C(x) case x: return #x
        C(PolicyLivePlayback);
        C(PolicyAVDSynchronization);
        C(PolicyClampPlaybackIntervalOnPlaybackDirectionChange);
        C(PolicyPlayoutOnTerminate);
        C(PolicyPlayoutOnSwitch);
        C(PolicyPlayoutOnDrain);
        C(PolicyPlayoutOnInternalDrain);
        C(PolicyIgnoreStreamUnPlayableCalls);
        C(PolicyDecodeBuffersUserAllocated);
        C(PolicyLimitInputInjectAhead);
        C(PolicyControlDataDetection);
        C(PolicyReduceCollatedData);
        C(PolicyOperateCollator2InReversibleMode);
        C(PolicyMonitorPCRTiming);
        C(PolicyMPEG2DoNotHonourProgressiveFrameFlag);
        C(PolicyMPEG2ApplicationType);
        C(PolicyH264TreatDuplicateDpbValuesAsNonReferenceFrameFirst);
        C(PolicyH264ForcePicOrderCntIgnoreDpbDisplayFrameOrdering);
        C(PolicyH264ValidateDpbValuesAgainstPTSValues);
        C(PolicyH264TreatTopBottomPictureStructAsInterlaced);
        C(PolicyH264AllowNonIDRResynchronization);
        C(PolicyFrameRateCalculationPrecedence);
        C(PolicyContainerFrameRate);
        C(PolicyAllowBadPreProcessedFrames);
        C(PolicyAllowBadHevcPreProcessedFrames);
        C(PolicyVideoPlayStreamMemoryProfile);
        C(PolicyErrorDecodingLevel);
        C(PolicyAudioApplicationType);
        C(PolicyAudioServiceType);
        C(PolicyRegionType);
        C(PolicyAudioProgramReferenceLevel);
        C(PolicyAudioSubstreamId);
        C(PolicyAllowReferenceFrameSubstitution);
        C(PolicyDiscardForReferenceQualityThreshold);
        C(PolicyDiscardForManifestationQualityThreshold);
        C(PolicyDecimateDecoderOutput);
        C(PolicyManifestFirstFrameEarly);
        C(PolicyVideoLastFrameBehaviour);
        C(PolicyMasterClock);
        C(PolicyExternalTimeMapping);
        C(PolicyExternalTimeMappingVsyncLocked);
        C(PolicyVideoStartImmediate);
        C(PolicyPacketInjectorPlayback);
        C(PolicyStreamOnlyKeyFrames);
        C(PolicyStreamDiscardFrames);
        C(PolicyStreamSingleGroupBetweenDiscontinuities);
        C(PolicyDiscardLateFrames);
        C(PolicyRebaseOnFailureToDeliverDataInTime);
        C(PolicyRebaseOnFailureToDecodeInTime);
        C(PolicyTrickModeDomain);
        C(PolicyAllowFrameDiscardAtNormalSpeed);
        C(PolicyClockPullingLimit2ToTheNPartsPerMillion);
        C(PolicySymmetricJumpDetection);
        C(PolicyPtsForwardJumpDetectionThreshold);
        C(PolicyLivePlaybackLatencyVariabilityMs);
        C(PolicyHdmiRxMode);
        C(PolicyRunningDevlog);
        C(PolicyTrickModeAudio);
        C(PolicyPlay24FPSVideoAt25FPS);
        C(PolicyUsePTSDeducedDefaultFrameRates);
        C(PolicyAudioStreamDrivenDualMono);
        C(PolicyAudioDualMonoChannelSelection);
        C(PolicyAudioDeEmphasis);
        // Private policies
        C(PolicyPlayoutAlwaysPlayout);
        C(PolicyPlayoutAlwaysDiscard);
        C(PolicyStatisticsOnAudio);
        C(PolicyStatisticsOnVideo);
#undef C

    default:
        if (Policy < (PlayerPolicy_t) PolicyMaxExtraPolicy)
        {
            SE_ERROR("LookupPlayerPolicy - Lookup failed for policy %d\n", Policy);
            return "UNKNOWN";
        }

        return "UNSPECIFIED";
    }
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Static constant data
//

static BufferDataDescriptor_t  MetaDataInputDescriptor          = METADATA_PLAYER_INPUT_DESCRIPTOR_TYPE;
static BufferDataDescriptor_t  MetaDataCodedFrameParameters     = METADATA_CODED_FRAME_PARAMETERS_TYPE;
static BufferDataDescriptor_t  MetaDataStartCodeList            = METADATA_START_CODE_LIST_TYPE;
static BufferDataDescriptor_t  MetaDataParsedFrameParameters    = METADATA_PARSED_FRAME_PARAMETERS_TYPE;
static BufferDataDescriptor_t  MetaDataParsedFrameParametersReference = METADATA_PARSED_FRAME_PARAMETERS_REFERENCE_TYPE;
static BufferDataDescriptor_t  MetaDataParsedVideoParameters    = METADATA_PARSED_VIDEO_PARAMETERS_TYPE;
static BufferDataDescriptor_t  MetaDataParsedAudioParameters    = METADATA_PARSED_AUDIO_PARAMETERS_TYPE;
static BufferDataDescriptor_t  MetaDataOutputTiming             = METADATA_OUTPUT_TIMING_TYPE;

static BufferDataDescriptor_t  BufferPlayerControlStructure     = BUFFER_PLAYER_CONTROL_STRUCTURE_TYPE;
static BufferDataDescriptor_t  BufferInputBuffer                = BUFFER_INPUT_TYPE;
static BufferDataDescriptor_t  MetaDataSequenceNumberDescriptor = METADATA_SEQUENCE_NUMBER_TYPE;
static BufferDataDescriptor_t  MetaDataUserDataDescriptor       = METADATA_USER_DATA_TYPE;

static BufferDataDescriptor_t  BufferCodedFrameBuffer           = BUFFER_CODED_FRAME_BUFFER_TYPE;

// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Register the buffer manager to the player
//

PlayerStatus_t   Player_Generic_c::RegisterBufferManager(
    BufferManager_t       BufferManager)
{
    // TODO(pht) rollbacks on error!

    if (this->BufferManager != NULL)
    {
        SE_ERROR("Attempt to change buffer manager, not permitted\n");
        return PlayerError;
    }

    this->BufferManager     = BufferManager;
    //
    // Register the predefined types, and the two buffer types used by the player
    //
    BufferStatus_t Status  = BufferManager->CreateBufferDataType(&MetaDataInputDescriptor,         &MetaDataInputDescriptorType);
    if (Status != BufferNoError)
    {
        return PlayerError;
    }

    Status  = BufferManager->CreateBufferDataType(&MetaDataCodedFrameParameters,        &MetaDataCodedFrameParametersType);
    if (Status != BufferNoError)
    {
        return PlayerError;
    }

    Status  = BufferManager->CreateBufferDataType(&MetaDataStartCodeList,           &MetaDataStartCodeListType);
    if (Status != BufferNoError)
    {
        return PlayerError;
    }

    Status  = BufferManager->CreateBufferDataType(&MetaDataParsedFrameParameters,       &MetaDataParsedFrameParametersType);
    if (Status != BufferNoError)
    {
        return PlayerError;
    }

    Status  = BufferManager->CreateBufferDataType(&MetaDataParsedFrameParametersReference,  &MetaDataParsedFrameParametersReferenceType);
    if (Status != BufferNoError)
    {
        return PlayerError;
    }

    Status  = BufferManager->CreateBufferDataType(&MetaDataParsedVideoParameters,       &MetaDataParsedVideoParametersType);
    if (Status != BufferNoError)
    {
        return PlayerError;
    }

    Status  = BufferManager->CreateBufferDataType(&MetaDataParsedAudioParameters,       &MetaDataParsedAudioParametersType);
    if (Status != BufferNoError)
    {
        return PlayerError;
    }

    Status  = BufferManager->CreateBufferDataType(&MetaDataOutputTiming,            &MetaDataOutputTimingType);
    if (Status != BufferNoError)
    {
        return PlayerError;
    }

    Status  = BufferManager->CreateBufferDataType(&MetaDataSequenceNumberDescriptor,    &MetaDataSequenceNumberType);
    if (Status != BufferNoError)
    {
        return PlayerError;
    }

    Status  = BufferManager->CreateBufferDataType(&BufferPlayerControlStructure,        &BufferPlayerControlStructureType);
    if (Status != BufferNoError)
    {
        return PlayerError;
    }

    Status  = BufferManager->CreateBufferDataType(&BufferInputBuffer,           &BufferInputBufferType);
    if (Status != BufferNoError)
    {
        return PlayerError;
    }

    Status  = BufferManager->CreateBufferDataType(&BufferCodedFrameBuffer,          &BufferCodedFrameBufferType);
    if (Status != BufferNoError)
    {
        return PlayerError;
    }

    Status  = BufferManager->CreateBufferDataType(&MetaDataUserDataDescriptor,     &MetaDataUserDataType);
    if (Status != BufferNoError)
    {
        return PlayerError;
    }

    //
    // Create pools of buffers for input and player control structures
    //
    Status  = BufferManager->CreatePool(&PlayerControlStructurePool, BufferPlayerControlStructureType, PLAYER_MAX_CONTROL_STRUCTURE_BUFFERS);
    if (Status != BufferNoError)
    {
        SE_ERROR("Failed to create pool of player control structures\n");
        return PlayerError;
    }

    Status  = BufferManager->CreatePool(&InputBufferPool, BufferInputBufferType, PLAYER_MAX_INPUT_BUFFERS);
    if (Status != BufferNoError)
    {
        SE_ERROR("Failed to create pool of input buffers\n");
        return PlayerError;
    }

    Status = InputBufferPool->AttachMetaData(MetaDataInputDescriptorType);
    if (Status != BufferNoError)
    {
        SE_ERROR("Failed to attach an input descriptor to all input buffers\n");
        return PlayerError;
    }

    return PlayerNoError;
}

// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Set a policy
//
PlayerStatus_t   Player_Generic_c::SetPolicy(PlayerPlayback_t     Playback,
                                             PlayerStream_t        Stream,
                                             PlayerPolicy_t        Policy,
                                             int                   PolicyValue)
{
    PlayerPolicyState_t *SpecificPolicyRecord;
    SE_INFO(group_player, "Playback 0x%p Stream 0x%p %-45s %d\n", Playback,
            Stream, LookupPlayerPolicy(Policy), PolicyValue);

    // Check Policy parameter validity
    if (Policy >= (PlayerPolicy_t)PolicyMaxExtraPolicy)
    {
        SE_ERROR("Invalid policy %d\n", Policy);
        return PlayerError;
    }

    // Check parameters consistency
    if ((Playback        != PlayerAllPlaybacks) &&
        (Stream         != PlayerAllStreams) &&
        (Stream->GetPlayback()   != Playback))
    {
        SE_ERROR("Attempt to set policy on specific stream, and differing specific playback\n");
        return PlayerError;
    }

    // Determine at which level the policy should be applied
    if (Stream != PlayerAllStreams)
    {
        SpecificPolicyRecord  = &Stream->PolicyRecord;
    }
    else if (Playback != PlayerAllPlaybacks)
    {
        SpecificPolicyRecord  = &Playback->PolicyRecord;
    }
    else
    {
        SpecificPolicyRecord  = &PolicyRecord;
    }

    // Save new policy settings
    SpecificPolicyRecord->Specified[Policy / 32]  |= 1 << (Policy % 32);
    SpecificPolicyRecord->Value[Policy]      = PolicyValue;
    return PlayerNoError;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Set module parameters
//

PlayerStatus_t   Player_Generic_c::SetModuleParameters(
    PlayerPlayback_t   Playback,
    PlayerStream_t     Stream,
    unsigned int       ParameterBlockSize,
    void              *ParameterBlock)
{
    PlayerParameterBlock_t  *PlayerParameterBlock = (PlayerParameterBlock_t *)ParameterBlock;

    if (ParameterBlockSize != sizeof(PlayerParameterBlock_t))
    {
        SE_ERROR("Invalid parameter block\n");
        return PlayerError;
    }

    switch (PlayerParameterBlock->ParameterType)
    {
    case PlayerSetCodedFrameBufferParameters:

        //
        // Check that this is not stream specific, and update the relevant values.
        //
        if (Stream != PlayerAllStreams)
        {
            SE_ERROR("Coded frame parameters cannot be set after a stream has been created\n");
            return PlayerError;
        }

        switch (PlayerParameterBlock->CodedFrame.StreamType)
        {
        case StreamTypeAudio:
            if (Playback == PlayerAllPlaybacks)
            {
                AudioCodedFrameCount        = PlayerParameterBlock->CodedFrame.CodedFrameCount;
                AudioCodedMemorySize        = PlayerParameterBlock->CodedFrame.CodedMemorySize;
                AudioCodedFrameMaximumSize      = PlayerParameterBlock->CodedFrame.CodedFrameMaximumSize;
                memcpy(AudioCodedMemoryPartitionName, PlayerParameterBlock->CodedFrame.CodedMemoryPartitionName, ALLOCATOR_MAX_PARTITION_NAME_SIZE);
            }
            else
            {
                Playback->AudioCodedFrameCount  = PlayerParameterBlock->CodedFrame.CodedFrameCount;
                Playback->AudioCodedMemorySize  = PlayerParameterBlock->CodedFrame.CodedMemorySize;
                Playback->AudioCodedFrameMaximumSize = PlayerParameterBlock->CodedFrame.CodedFrameMaximumSize;
                memcpy(Playback->AudioCodedMemoryPartitionName, PlayerParameterBlock->CodedFrame.CodedMemoryPartitionName, ALLOCATOR_MAX_PARTITION_NAME_SIZE);
            }

            break;

        case StreamTypeVideo:
            if (Playback == PlayerAllPlaybacks)
            {
                VideoCodedFrameCount        = PlayerParameterBlock->CodedFrame.CodedFrameCount;
                VideoCodedMemorySize        = PlayerParameterBlock->CodedFrame.CodedMemorySize;
                VideoCodedFrameMaximumSize      = PlayerParameterBlock->CodedFrame.CodedFrameMaximumSize;
                memcpy(VideoCodedMemoryPartitionName, PlayerParameterBlock->CodedFrame.CodedMemoryPartitionName, ALLOCATOR_MAX_PARTITION_NAME_SIZE);
            }
            else
            {
                Playback->VideoCodedFrameCount  = PlayerParameterBlock->CodedFrame.CodedFrameCount;
                Playback->VideoCodedMemorySize  = PlayerParameterBlock->CodedFrame.CodedMemorySize;
                Playback->VideoCodedFrameMaximumSize = PlayerParameterBlock->CodedFrame.CodedFrameMaximumSize;
                memcpy(Playback->VideoCodedMemoryPartitionName, PlayerParameterBlock->CodedFrame.CodedMemoryPartitionName, ALLOCATOR_MAX_PARTITION_NAME_SIZE);
            }

            break;

        case StreamTypeOther:
            if (Playback == PlayerAllPlaybacks)
            {
                OtherCodedFrameCount        = PlayerParameterBlock->CodedFrame.CodedFrameCount;
                OtherCodedMemorySize        = PlayerParameterBlock->CodedFrame.CodedMemorySize;
                OtherCodedFrameMaximumSize      = PlayerParameterBlock->CodedFrame.CodedFrameMaximumSize;
                memcpy(OtherCodedMemoryPartitionName, PlayerParameterBlock->CodedFrame.CodedMemoryPartitionName, ALLOCATOR_MAX_PARTITION_NAME_SIZE);
            }
            else
            {
                Playback->OtherCodedFrameCount  = PlayerParameterBlock->CodedFrame.CodedFrameCount;
                Playback->OtherCodedMemorySize  = PlayerParameterBlock->CodedFrame.CodedMemorySize;
                Playback->OtherCodedFrameMaximumSize = PlayerParameterBlock->CodedFrame.CodedFrameMaximumSize;
                memcpy(Playback->OtherCodedMemoryPartitionName, PlayerParameterBlock->CodedFrame.CodedMemoryPartitionName, ALLOCATOR_MAX_PARTITION_NAME_SIZE);
            }

            break;

        default:
            SE_ERROR("Attempt to set Coded frame parameters for StreamTypeNone\n");
            return PlayerError;
            break;
        }

        break;

    default:
        SE_ERROR("Unrecognised parameter block (%d)\n", PlayerParameterBlock->ParameterType);
        return PlayerError;
    }

    return  PlayerNoError;
}

// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Set the presentation interval
//

PlayerStatus_t   Player_Generic_c::SetPresentationInterval(
    PlayerPlayback_t      Playback,
    PlayerStream_t        Stream,
    unsigned long long    IntervalStartNativeTime,
    unsigned long long    IntervalEndNativeTime)
{
    //
    // First just check the consistency of the input
    //

    if ((Stream == PlayerAllStreams) ?
        (Playback == PlayerAllPlaybacks) :
        (((Playback != PlayerAllPlaybacks) && (Playback != Stream->GetPlayback())) ||
         ((Playback == PlayerAllPlaybacks) && (Stream->GetPlayback() == PlayerAllPlaybacks))
        ))
    {
        SE_ERROR("Invalid combination of Playback/Stream specifiers (%d %d %d)\n",
                 (Playback != PlayerAllPlaybacks), (Stream != PlayerAllStreams), ((Stream == PlayerAllStreams) ? 1 : (Playback == Stream->GetPlayback())));
        return PlayerError;
    }

    if (Playback == PlayerAllPlaybacks)
    {
        Playback  = Stream->GetPlayback();
    }

    return Playback->SetPresentationInterval(Stream, IntervalStartNativeTime, IntervalEndNativeTime);
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Get the buffer manager
//

PlayerStatus_t   Player_Generic_c::GetBufferManager(
    BufferManager_t      *BufferManager)
{
    *BufferManager  = this->BufferManager;

    if (this->BufferManager == NULL)
    {
        SE_ERROR("BufferManager has not been registerred\n");
        return PlayerError;
    }

    return PlayerNoError;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Find the class instances associated with a stream playback
//

PlayerStatus_t   Player_Generic_c::GetClassList(PlayerStream_t            Stream,
                                                Collator_t           *Collator,
                                                FrameParser_t            *FrameParser,
                                                Codec_t              *Codec,
                                                OutputTimer_t            *OutputTimer,
                                                ManifestationCoordinator_t   *ManifestationCoordinator)
{
    if (Collator != NULL)
    {
        *Collator         = Stream->GetCollator();
    }

    if (FrameParser != NULL)
    {
        *FrameParser          = Stream->GetFrameParser();
    }

    if (Codec != NULL)
    {
        *Codec                = Stream->GetCodec();
    }

    if (OutputTimer != NULL)
    {
        *OutputTimer          = Stream->GetOutputTimer();
    }

    if (ManifestationCoordinator != NULL)
    {
        *ManifestationCoordinator = Stream->GetManifestationCoordinator();
    }

    return PlayerNoError;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Find the decode buffer pool associated with a stream playback
//

PlayerStatus_t   Player_Generic_c::GetDecodeBufferPool(
    PlayerStream_t        Stream,
    BufferPool_t         *Pool)
{
    *Pool   = Stream->DecodeBufferPool;
    return PlayerNoError;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Discover a playback speed
//

PlayerStatus_t   Player_Generic_c::GetPlaybackSpeed(
    PlayerPlayback_t      Playback,
    Rational_t       *Speed,
    PlayDirection_t      *Direction)
{
    *Speed  = Playback->mSpeed;
    *Direction  = Playback->mDirection;
    return PlayerNoError;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Retrieve the presentation interval
//

PlayerStatus_t   Player_Generic_c::GetPresentationInterval(
    PlayerStream_t        Stream,
    unsigned long long   *IntervalStartNormalizedTime,
    unsigned long long   *IntervalEndNormalizedTime)
{
    unsigned long long  Limit;

    if (IntervalStartNormalizedTime != NULL)
    {
        Limit   = Stream->GetPlayback()->PresentationIntervalReversalLimitStartNormalizedTime;
        *IntervalStartNormalizedTime    = NotValidTime(Limit) ?
                                          Stream->RequestedPresentationIntervalStartNormalizedTime :
                                          max(Limit, Stream->RequestedPresentationIntervalStartNormalizedTime);
    }

    if (IntervalEndNormalizedTime != NULL)
    {
        Limit   = Stream->GetPlayback()->PresentationIntervalReversalLimitEndNormalizedTime;
        *IntervalEndNormalizedTime  = NotValidTime(Limit) ?
                                      Stream->RequestedPresentationIntervalEndNormalizedTime :
                                      min(Limit, Stream->RequestedPresentationIntervalEndNormalizedTime);
    }

    return PlayerNoError;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Discover the status of a policy
//

int     Player_Generic_c::PolicyValue(PlayerPlayback_t    Playback,
                                      PlayerStream_t        Stream,
                                      PlayerPolicy_t        Policy)
{
    int             Value;
    unsigned int        Offset;
    unsigned int        Mask;

    // Check Policy parameter validity
    if (Policy >= (PlayerPolicy_t)PolicyMaxExtraPolicy)
    {
        SE_ERROR("Invalid policy %d\n", Policy);
        return PlayerError;
    }

    // Check parameters consistency
    if ((Playback        != PlayerAllPlaybacks) &&
        (Stream         != PlayerAllStreams) &&
        (Stream->GetPlayback()   != Playback))
    {
        SE_ERROR("Attempt to get policy on specific stream, and differing specific playback\n");
        return PlayerError;
    }

    if (Stream != PlayerAllStreams)
    {
        Playback  = Stream->GetPlayback();
    }

    Offset      = Policy / 32;
    Mask        = (1 << (Policy % 32));
    Value       = 0;

    if ((PolicyRecord.Specified[Offset] & Mask) != 0)
    {
        Value = PolicyRecord.Value[Policy];
    }

    if (Playback != PlayerAllPlaybacks)
    {
        if ((Playback->PolicyRecord.Specified[Offset] & Mask) != 0)
        {
            Value = Playback->PolicyRecord.Value[Policy];
        }

        if (Stream != PlayerAllStreams)
        {
            if ((Stream->PolicyRecord.Specified[Offset] & Mask) != 0)
            {
                Value = Stream->PolicyRecord.Value[Policy];
            }
        }
    }

    return Value;
}

// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//   Store the control's data into the specified object
//
PlayerStatus_t      Player_Generic_c::SetControl(PlayerPlayback_t     Playback,
                                                 PlayerStream_t        Stream,
                                                 stm_se_ctrl_t         Ctrl,
                                                 const void            *Value)
{
    SE_DEBUG(group_player, "Playback 0x%p Stream 0x%p Ctrl %d\n", Playback, Stream, Ctrl);

    // controls the consistency of the request
    if ((Playback    != PlayerAllPlaybacks) &&
        (Stream         != PlayerAllStreams) &&
        (Stream->GetPlayback()   != Playback))
    {
        SE_ERROR("Attempt to set control on specific stream, and differing specific playback\n");
        return PlayerError;
    }

    // gets the object on which the control is applied (Stream, Playback or Player)
    PlayerControslStorageState_t    *SpecificControlRecord;

    if (Stream != PlayerAllStreams)
    {
        SpecificControlRecord   = &Stream->ControlsRecord;
    }
    else if (Playback != PlayerAllPlaybacks)
    {
        SpecificControlRecord   = &Playback->ControlsRecord;
    }
    else
    {
        SpecificControlRecord   = &ControlsRecord;
    }

    PlayerControlStorage_t  ControlRecordName = ControlsRecord_not_implemented;

    switch (Ctrl)
    {
    case STM_SE_CTRL_AUDIO_DYNAMIC_RANGE_COMPRESSION:
    {
        stm_se_drc_t *drc = (stm_se_drc_t *) Value;

        ControlRecordName = ControlsRecord_drc_main;
        SpecificControlRecord->Control_drc = *drc;
        SE_VERBOSE(group_player, "Set drc record: mode=%d cut=%d boost=%d\n", drc->mode, drc->cut, drc->boost);
        break;
    }

    case STM_SE_CTRL_STREAM_DRIVEN_STEREO:
    {
        ControlRecordName = ControlsRecord_StreamDrivenDownmix;
        SpecificControlRecord->Control_StreamDrivenDownmix = (bool *) Value;
        SE_VERBOSE(group_player, "Set StreamDrivenDownmix record: %d\n", SpecificControlRecord->Control_StreamDrivenDownmix);
        break;
    }

    case STM_SE_CTRL_SPEAKER_CONFIG:
    {
        stm_se_audio_channel_assignment_t *channelassignment = (stm_se_audio_channel_assignment_t *) Value;
        ControlRecordName = ControlsRecord_SpeakerConfig;
        SpecificControlRecord->Control_SpeakerConfig = *channelassignment;
        break;
    }

    case STM_SE_CTRL_AAC_DECODER_CONFIG:
    {
        stm_se_mpeg4aac_t *aac = (stm_se_mpeg4aac_t *) Value;
        ControlRecordName = ControlsRecord_AacConfig;
        SpecificControlRecord->Control_AacConfig = *aac;
        break;
    }

    case STM_SE_CTRL_PLAY_STREAM_MUTE:
    {
        ControlRecordName = ControlsRecord_mute;
        SpecificControlRecord->Control_mute = ((int)Value == STM_SE_CTRL_VALUE_APPLY) ? true : false;
        SE_VERBOSE(group_player, "Set Mute record: %d\n", SpecificControlRecord->Control_mute);
        break;
    }

    default:
        SE_ERROR("Control %d not supported\n", Ctrl);
        return PlayerError;
    }

    // at this point: ControlsRecord_not_implemented != ControlRecordName
    // indicates that the control is now specified for this object
    SpecificControlRecord->Specified[ControlRecordName / 32]  |= 1 << (ControlRecordName % 32);
    return PlayerNoError;
}

// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//   Retrieve the control's datas from the appropriate object
//   (from stream object if available, else from playback object, else from player object)
//
PlayerStatus_t      Player_Generic_c::GetControl(PlayerPlayback_t     Playback,
                                                 PlayerStream_t        Stream,
                                                 stm_se_ctrl_t         Ctrl,
                                                 void                  *Value)
{
    SE_VERBOSE(group_player, "Playback 0x%p Stream 0x%p Ctrl %d\n", Playback, Stream, Ctrl);
    PlayerControslStorageState_t    *SpecificControlRecord;
    SpecificControlRecord = NULL;
    PlayerStatus_t  PlayerStatus;

    // controls the consistency of the request
    if ((Playback        != PlayerAllPlaybacks) &&
        (Stream         != PlayerAllStreams) &&
        (Stream->GetPlayback()   != Playback))
    {
        SE_ERROR("Attempt to get control on specific stream, and differing specific playback\n");
        return PlayerError;
    }

    if (Stream != PlayerAllStreams)
    {
        Playback    = Stream->GetPlayback();
    }

    switch (Ctrl)
    {
    case STM_SE_CTRL_AUDIO_DYNAMIC_RANGE_COMPRESSION:
    {
        stm_se_drc_t *drc = (stm_se_drc_t *) Value;

        PlayerStatus = MapControl(Playback, Stream, ControlsRecord_drc_main, SpecificControlRecord);

        if (PlayerNoError != PlayerStatus)
        {
            return PlayerStatus;
        }

        *drc = SpecificControlRecord->Control_drc;

        SE_VERBOSE(group_player, "Get drc record: mode=%d cut=%d boost=%d\n", drc->mode, drc->cut, drc->boost);
        break;
    }

    case STM_SE_CTRL_STREAM_DRIVEN_STEREO:
    {
        bool *streamdrivendownmix = (bool *) Value;
        PlayerStatus = MapControl(Playback, Stream, ControlsRecord_StreamDrivenDownmix, SpecificControlRecord);

        if (PlayerNoError != PlayerStatus)
        {
            return PlayerStatus;
        }

        *streamdrivendownmix = SpecificControlRecord->Control_StreamDrivenDownmix;
        SE_VERBOSE(group_player, "Get StreamDrivenDownmix record: %d\n", *streamdrivendownmix);
        break;
    }

    case STM_SE_CTRL_SPEAKER_CONFIG:
    {
        stm_se_audio_channel_assignment_t *channelassignment = (stm_se_audio_channel_assignment_t *) Value;
        PlayerStatus = MapControl(Playback, Stream, ControlsRecord_SpeakerConfig, SpecificControlRecord);

        if (PlayerNoError != PlayerStatus)
        {
            return PlayerStatus;
        }

        *channelassignment = SpecificControlRecord->Control_SpeakerConfig;
        break;
    }

    case STM_SE_CTRL_AAC_DECODER_CONFIG:
    {
        stm_se_mpeg4aac_t *aac_config = (stm_se_mpeg4aac_t *) Value;

        PlayerStatus = MapControl(Playback, Stream, ControlsRecord_AacConfig, SpecificControlRecord);
        if (PlayerNoError != PlayerStatus)
        {
            return PlayerStatus;
        }
        *aac_config = SpecificControlRecord->Control_AacConfig;
        break;
    }

    case STM_SE_CTRL_PLAY_STREAM_MUTE:
    {
        bool *mute = (bool *) Value;
        PlayerStatus = MapControl(Playback, Stream, ControlsRecord_mute, SpecificControlRecord);

        if (PlayerNoError != PlayerStatus)
        {
            return PlayerStatus;
        }

        *mute = SpecificControlRecord->Control_mute;
        SE_VERBOSE(group_player, "Get Mute record: %d\n", *mute);
        break;
    }

    default:
        SE_ERROR("Not supported ctrl %d\n", Ctrl);
        return PlayerError;
    }

    return PlayerNoError;
}

// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//   Find the appropriate object which holds the control's data
//
//
PlayerStatus_t      Player_Generic_c::MapControl(
    PlayerPlayback_t        Playback,
    PlayerStream_t          Stream,
    PlayerControlStorage_t  ControlRecordName,
    PlayerControslStorageState_t *&SpecificControlRecord
)
{
    SE_VERBOSE(group_player, "Playback 0x%p Stream 0x%p CtrlRecName %d\n", Playback, Stream, ControlRecordName);
    unsigned int         Offset;
    unsigned int         Mask;
    // get the applicable control if exists (from Stream, Playback or Player)
    Offset      = ControlRecordName / 32;
    Mask        = (1 << (ControlRecordName % 32));

    // get the control at Player level if exists
    if ((ControlsRecord.Specified[Offset] & Mask) != 0)
    {
        SpecificControlRecord   = &ControlsRecord;
        SE_VERBOSE(group_player, "data found at Player level for ControlRecordName=%d\n", ControlRecordName);
    }

    // if a specific playback is indicated, get the control at Playback level if exists
    if (Playback != PlayerAllPlaybacks)
    {
        if ((Playback->ControlsRecord.Specified[Offset] & Mask) != 0)
        {
            SpecificControlRecord   = &Playback->ControlsRecord;
            SE_VERBOSE(group_player, "data found at Playback level for ControlRecordName=%d\n", ControlRecordName);
        }

        // if a specific stream is indicated, get the control at Stream level if exists
        if (Stream != PlayerAllStreams)
        {
            if ((Stream->ControlsRecord.Specified[Offset] & Mask) != 0)
            {
                SpecificControlRecord   = &Stream->ControlsRecord;
                SE_VERBOSE(group_player, "data found at Stream level for ControlRecordName=%d\n", ControlRecordName);
            }
        }
    }

    if (NULL == SpecificControlRecord)
    {
        SE_VERBOSE(group_player, "No datas available for ControlRecordName=%d\n", ControlRecordName);
        return PlayerMatchNotFound;
    }

    return PlayerNoError;
}

// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Low power enter
//

PlayerStatus_t Player_Generic_c::LowPowerEnter(__stm_se_low_power_mode_t low_power_mode)
{
    // Save the requested low power mode
    LowPowerMode = low_power_mode;
    return PlayerNoError;
}

// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Low power exit
//

PlayerStatus_t Player_Generic_c::LowPowerExit(void)
{
    // Nothing to do here
    return PlayerNoError;
}

// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Get current low power mode: HPS or CPS
//

__stm_se_low_power_mode_t Player_Generic_c::GetLowPowerMode(void)
{
    return LowPowerMode;
}
