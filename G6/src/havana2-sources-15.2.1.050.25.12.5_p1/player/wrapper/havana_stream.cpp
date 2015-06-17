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

#include <stm_event.h>

#include "osinline.h"
#include "mpeg2.h"
#include "player.h"
#include "collator_common.h"
#include "output_timer_base.h"
#include "codec_mme_base.h"
#include "decode_buffer_manager_base.h"
#include "manifestor_audio.h"
#include "manifestor_video.h"
#include "havana_player.h"
#include "havana_playback.h"
#include "havana_capture.h"
#include "havana_user_data.h"
#include "pes.h"

#include "havana_display.h"
#include "player_policy.h"
#include "havana_stream.h"

//{{{  HavanaStream_c
HavanaStream_c::HavanaStream_c(void)
    : UserDataSender()
    , TransformerId(0)
    , InputLock()
    , ManifestorLock()
    , PlayerStreamType(StreamTypeNone)
    , PlayerStream(NULL)
    , HavanaPlayer(NULL)
    , HavanaPlayback(NULL)
    , HavanaCapture(NULL)
    , Player(NULL)
    , Collator(NULL)
    , FrameParser(NULL)
    , Codec(NULL)
    , OutputTimer(NULL)
    , DecodeBufferManager(NULL)
    , ManifestationCoordinator(NULL)
    , DecodeBufferPool(NULL)
    , Manifestor()
    , AudioPassThroughLock()
    , NbAudioPlayerSinks(0)
    , AudioPlayers()
    , AudioPassThrough(NULL)
    , Encoding(STM_SE_STREAM_ENCODING_AUDIO_NONE)
{
    OS_InitializeMutex(&InputLock);
    OS_InitializeMutex(&ManifestorLock);
    OS_InitializeMutex(&AudioPassThroughLock);
}
//}}}
//{{{  ~HavanaStream_c
HavanaStream_c::~HavanaStream_c(void)
{
    if (PlayerStream != NULL)
    {
        // tell player we want to bin rest of stream
        Player->SetPolicy(HavanaPlayback->GetPlayerPlayback(), PlayerStream, PolicyPlayoutOnTerminate, PolicyValueDiscard);

        // finally remove stream from player
        Player->RemoveStream(PlayerStream, true);
    }

    delete Collator;
    delete FrameParser;
    delete Codec;
    delete OutputTimer;
    delete DecodeBufferManager;
    delete ManifestationCoordinator;
    delete HavanaCapture;

    OS_TerminateMutex(&ManifestorLock);
    OS_TerminateMutex(&InputLock);
    OS_TerminateMutex(&AudioPassThroughLock);
}
//}}}
//{{{  Init
//{{{  doxynote
/// \brief Create and initialise all of the necessary player components for a new player stream
/// \param HavanaPlayer         Grandparent class
/// \param Player               The player
/// \param HavanaPlayback       The havana playback to which the stream will be added
/// \param Media                Textual description of media (audio or video)
/// \param Encoding             The encoding of the stream content (MPEG2/H264 etc)
/// \param Multiplex            Name of multiplex if present (TS)
/// \return                     Havana status code, HavanaNoError indicates success.
//}}}
HavanaStatus_t HavanaStream_c::Init(class HavanaPlayer_c           *InitHavanaPlayer,
                                    class Player_c                 *InitPlayer,
                                    class HavanaPlayback_c         *InitHavanaPlayback,
                                    stm_se_media_t                  Media,
                                    stm_se_stream_encoding_t        InitEncoding,
                                    unsigned int                    Id)
{
    HavanaStatus_t Status          = HavanaNoError;
    PlayerStatus_t PlayerStatus    = PlayerNoError;

    SE_VERBOSE(group_havana, "Stream %x %x\n", Media, InitEncoding);

    Player                = InitPlayer;
    HavanaPlayer          = InitHavanaPlayer;
    HavanaPlayback        = InitHavanaPlayback;

    if (Media == STM_SE_MEDIA_AUDIO)
    {
        PlayerStreamType        = StreamTypeAudio;
    }
    else if (Media == STM_SE_MEDIA_VIDEO)
    {
        PlayerStreamType        = StreamTypeVideo;
    }
    else
    {
        PlayerStreamType        = StreamTypeOther;
    }

    if (Collator == NULL)
    {
        Status  = HavanaPlayer->CallFactory(ComponentCollator, PlayerStreamType, InitEncoding, (void **)&Collator);
        if (Status != HavanaNoError)
        {
            return Status;
        }
    }


    if (FrameParser == NULL)
    {
        Status  = HavanaPlayer->CallFactory(ComponentFrameParser, PlayerStreamType, InitEncoding, (void **)&FrameParser);
        if (Status != HavanaNoError)
        {
            return Status;
        }
    }

    if (Codec == NULL)
    {
        Status  = HavanaPlayer->CallFactory(ComponentCodec, PlayerStreamType, InitEncoding, (void **)&Codec);
        if (Status != HavanaNoError)
        {
            return Status;
        }
    }

    {
        CodecParameterBlock_t           CodecParameters;
        CodecStatus_t                   CodecStatus;
        TransformerId                   = Id & 0x01;
        CodecParameters.ParameterType   = CodecSelectTransformer;
        CodecParameters.Transformer     = TransformerId;
        CodecStatus                     = Codec->SetModuleParameters(sizeof(CodecParameterBlock_t), &CodecParameters);

        if (CodecStatus != CodecNoError)
        {
            SE_ERROR("Failed to set codec parameters (%08x)\n", CodecStatus);
            return HavanaNoMemory;
        }

        SE_DEBUG(group_havana, "Selecting transformer %d for %s stream instance %d\n", TransformerId,
                 (PlayerStreamType == StreamTypeAudio ? "audio" : (PlayerStreamType == StreamTypeVideo ? "video" : "other")), Id);
    }

    if (OutputTimer == NULL)
    {
        Status  = HavanaPlayer->CallFactory(ComponentOutputTimer, PlayerStreamType,
                                            PlayerStreamType == StreamTypeAudio ? STM_SE_STREAM_ENCODING_AUDIO_NONE : STM_SE_STREAM_ENCODING_VIDEO_NONE,
                                            (void **)&OutputTimer);
        if (Status != HavanaNoError)
        {
            return Status;
        }
    }

    if (DecodeBufferManager == NULL)
    {
        Status  = HavanaPlayer->CallFactory(ComponentDecodeBufferManager, PlayerStreamType,
                                            PlayerStreamType == StreamTypeAudio ? STM_SE_STREAM_ENCODING_AUDIO_NONE : STM_SE_STREAM_ENCODING_VIDEO_NONE,
                                            (void **)&DecodeBufferManager);
        if (Status != HavanaNoError)
        {
            return Status;
        }
    }

    if (ManifestationCoordinator == NULL)
    {
        Status  = HavanaPlayer->CallFactory(ComponentManifestationCoordinator, PlayerStreamType,
                                            PlayerStreamType == StreamTypeAudio ? STM_SE_STREAM_ENCODING_AUDIO_NONE : STM_SE_STREAM_ENCODING_VIDEO_NONE,
                                            (void **)&ManifestationCoordinator);
        if (Status != HavanaNoError)
        {
            return Status;
        }
    }

    Status = ConfigureDecodeBufferManager();
    if (Status != HavanaNoError)
    {
        return Status;
    }

    if ((InitEncoding == STM_SE_STREAM_ENCODING_VIDEO_UNCOMPRESSED) || (InitEncoding == STM_SE_STREAM_ENCODING_VIDEO_CAP))
    {
        PlayerParameterBlock_t  PlayerParameters;
        PlayerParameters.ParameterType               = PlayerSetCodedFrameBufferParameters;
        PlayerParameters.CodedFrame.StreamType       = StreamTypeVideo;
        PlayerParameters.CodedFrame.CodedFrameCount  = DVP_CODED_FRAME_COUNT;

        if (InitEncoding == STM_SE_STREAM_ENCODING_VIDEO_UNCOMPRESSED)
        {
            PlayerParameters.CodedFrame.CodedMemorySize = DVP_FRAME_MEMORY_SIZE;
        }
        else
        {
            PlayerParameters.CodedFrame.CodedMemorySize = DVP_FRAME_MEMORY_SIZE / 2;
        }

        PlayerParameters.CodedFrame.CodedFrameMaximumSize = DVP_MAXIMUM_FRAME_SIZE;
        strncpy(PlayerParameters.CodedFrame.CodedMemoryPartitionName, DVP_MEMORY_PARTITION,
                sizeof(PlayerParameters.CodedFrame.CodedMemoryPartitionName));
        PlayerParameters.CodedFrame.CodedMemoryPartitionName[
            sizeof(PlayerParameters.CodedFrame.CodedMemoryPartitionName) - 1] = '\0';

        Player->SetModuleParameters(HavanaPlayback->GetPlayerPlayback(), PlayerAllStreams,
                                    sizeof(PlayerParameterBlock_t), &PlayerParameters);
    }

    PlayerStatus = Player->AddStream(HavanaPlayback->GetPlayerPlayback(),
                                     &PlayerStream,
                                     PlayerStreamType,
                                     Id,
                                     Collator,
                                     FrameParser,
                                     Codec,
                                     OutputTimer,
                                     DecodeBufferManager,
                                     ManifestationCoordinator,
                                     this,
                                     (UserDataSource_t)&UserDataSender,
                                     true);
    if (PlayerStatus != PlayerNoError)
    {
        SE_ERROR("Unable to create player stream\n");
        return HavanaNoMemory;
    }

    // User Data sender init
    Status      = UserDataSender.Init(PlayerStream);
    if (Status != HavanaNoError)
    {
        return Status;
    }

    PlayerStatus      = Player->GetDecodeBufferPool(PlayerStream, &DecodeBufferPool);
    if (PlayerStatus != PlayerNoError)
    {
        SE_ERROR("Unable to access decode buffer pool\n");
        return HavanaNoMemory;
    }

#ifdef LOWMEMORYBANDWIDTH
    PlayerStream->SetEncoding(InitEncoding);
#endif

    Player->SpecifySignalledEvents(HavanaPlayback->GetPlayerPlayback(), PlayerStream, EventAllEvents);

    Encoding = InitEncoding;

    return HavanaNoError;
}
//}}}
//{{{  AddManifestor
HavanaStatus_t HavanaStream_c::AddManifestor(class Manifestor_c     *NewManifestor)
{
    PlayerStatus_t      PlayerStatus    = PlayerNoError;
    HavanaStatus_t      HavanaStatus    = HavanaNoError;
    unsigned int        Index;

    if (ManifestationCoordinator == NULL)
    {
        SE_ERROR("Unable to add Manifestor - ManifestationCoordinator is NULL\n");
        return HavanaNoMemory;
    }

    OS_LockMutex(&ManifestorLock);
    HavanaStatus        = FindManifestor(NULL, &Index);

    if (HavanaStatus != HavanaNoError)
    {
        SE_ERROR("Unable to add Manifestor - too many manifestors\n");
        OS_UnLockMutex(&ManifestorLock);
        return HavanaError;
    }

    SE_VERBOSE(group_havana, "Adding %p\n", NewManifestor);
    PlayerStatus      = ManifestationCoordinator->AddManifestation(NewManifestor, NULL);
    if (PlayerStatus != PlayerNoError)
    {
        SE_ERROR("Unable to add Manifestor - %d\n", PlayerStatus);
        OS_UnLockMutex(&ManifestorLock);
        return HavanaError;
    }

    Manifestor[Index]   = NewManifestor;

    if (PlayerStreamType == StreamTypeVideo)
    {
        class Manifestor_Video_c       *VideoManifestor         = (class Manifestor_Video_c *)NewManifestor;
        unsigned int                    ManifestorCapabilities;
        VideoManifestor->GetCapabilities(&ManifestorCapabilities);

        if ((ManifestorCapabilities & MANIFESTOR_CAPABILITY_GRAB) != 0)
        {
            if (HavanaCapture == NULL)
            {
                HavanaCapture           = new HavanaCapture_c();
            }

            if (HavanaCapture != NULL)
            {
                HavanaCapture->Init((class Manifestor_Base_c *)NewManifestor);
            }
            else
            {
                SE_ERROR("Unable to create capture classes - frame grab not enabled\n");
            }
        }
    }

    OS_UnLockMutex(&ManifestorLock);
    return HavanaNoError;
}
//}}}
//{{{  RemoveManifestor
HavanaStatus_t HavanaStream_c::RemoveManifestor(class Manifestor_c  *OldManifestor)
{
    PlayerStatus_t      PlayerStatus    = PlayerNoError;
    HavanaStatus_t      HavanaStatus    = HavanaNoError;
    unsigned int        Index;

    if (ManifestationCoordinator == NULL)
    {
        SE_ERROR("Unable to remove Manifestor - ManifestationCoordinator is NULL\n");
        return HavanaNoMemory;
    }

    OS_LockMutex(&ManifestorLock);
    HavanaStatus        = FindManifestor(OldManifestor, &Index);

    if (HavanaStatus != HavanaNoError)
    {
        SE_ERROR("Unable to remove Manifestor - Manifestor not found\n");
        OS_UnLockMutex(&ManifestorLock);
        return HavanaStatus;
    }

    Manifestor[Index]   = NULL;
    PlayerStatus        = ManifestationCoordinator->RemoveManifestation(OldManifestor, NULL);

    if (PlayerStatus != PlayerNoError)
    {
        SE_ERROR("Unable to remove Manifestor %d\n", PlayerStatus);
        OS_UnLockMutex(&ManifestorLock);
        return HavanaError;
    }

    if (PlayerStreamType == StreamTypeVideo)
    {
        class Manifestor_Video_c       *VideoManifestor         = (class Manifestor_Video_c *)OldManifestor;
        unsigned int                    ManifestorCapabilities;
        VideoManifestor->GetCapabilities(&ManifestorCapabilities);

        if ((ManifestorCapabilities & MANIFESTOR_CAPABILITY_GRAB) != 0)
        {
            if (HavanaCapture != NULL)
            {
                HavanaCapture->DeInit((class Manifestor_Base_c *)OldManifestor);
            }
            else
            {
                SE_ERROR("Unable to DeInit capture classes - frame grab not disabled\n");
            }
        }
    }

    OS_UnLockMutex(&ManifestorLock);
    return HavanaNoError;
}
//}}}
//{{{  FindManifestor
//     This functions must be locked (ManifestorLock) in caller function
HavanaStatus_t HavanaStream_c::FindManifestor(class Manifestor_c *ManifestorToFind, unsigned int *Index)
{
    int         i;
    SE_VERBOSE(group_havana, "\n");
    OS_AssertMutexHeld(&ManifestorLock);

    for (i = 0; i < MAX_MANIFESTORS; i++)
    {
        if (Manifestor[i] == ManifestorToFind)
        {
            break;
        }
    }

    if (i < MAX_MANIFESTORS)
    {
        *Index          = i;
    }
    else
    {
        return HavanaError;
    }

    return HavanaNoError;
}
//}}}
//{{{  FindManifestorByCapability
HavanaStatus_t HavanaStream_c::FindManifestorByCapability(unsigned int Capability, class Manifestor_c **MatchingManifestor)
{
    int         i;
    SE_VERBOSE(group_havana, "\n");
    OS_LockMutex(&ManifestorLock);

    for (i = 0; i < MAX_MANIFESTORS; i++)
    {
        if (Manifestor[i] != NULL)
        {
            unsigned int                    ManifestorCapabilities;
            Manifestor[i]->GetCapabilities(&ManifestorCapabilities);

            if ((ManifestorCapabilities & Capability) != 0)
            {
                break;
            }
        }
    }

    if (i < MAX_MANIFESTORS)
    {
        *MatchingManifestor     = Manifestor[i];
    }

    OS_UnLockMutex(&ManifestorLock);
    return (i < MAX_MANIFESTORS) ? HavanaNoError : HavanaError;
}
//}}}
//{{{  InjectData
HavanaStatus_t HavanaStream_c::InjectData(const void    *Data,
                                          unsigned int   DataLength,
                                          PlayerInputDescriptor_t InjectedDataDescriptor)
{
    PlayerStatus_t              PlayerStatus;
    Buffer_t                    Buffer;
    PlayerInputDescriptor_t    *InputDescriptor;
    OS_LockMutex(&InputLock);
    PlayerStatus = Player->GetInjectBuffer(&Buffer);

    if (PlayerStatus != PlayerNoError)
    {
        SE_ERROR("Failed get inject buffer\n");
        OS_UnLockMutex(&InputLock);
        return HavanaError;
    }

    Buffer->ObtainMetaDataReference(Player->MetaDataInputDescriptorType, (void **)&InputDescriptor);
    SE_ASSERT(InputDescriptor != NULL);
    memcpy(InputDescriptor, &InjectedDataDescriptor, sizeof(PlayerInputDescriptor_t));

    InputDescriptor->UnMuxedStream              = PlayerStream;

    Buffer->RegisterDataReference(DataLength, (void *)Data);
    Buffer->SetUsedDataSize(DataLength);
    Player->InjectData(HavanaPlayback->GetPlayerPlayback(), Buffer);

    OS_UnLockMutex(&InputLock);

    return HavanaNoError;
}
//}}}

//{{{  Discontinuity
HavanaStatus_t HavanaStream_c::Discontinuity(bool                    ContinuousReverse,
                                             bool                    SurplusData,
                                             bool                    EndOfStream)
{
    SE_VERBOSE(group_havana, "Surplus %d, reverse %d\n", SurplusData, ContinuousReverse);
    OS_LockMutex(&InputLock);

    if (EndOfStream)
    {
        PlayerStream->EndOfStream();
    }
    else
    {
        PlayerStream->InputJump(SurplusData, ContinuousReverse, false);
    }

    OS_UnLockMutex(&InputLock);
    return HavanaNoError;
}
//}}}
//{{{  Drain
HavanaStatus_t HavanaStream_c::Drain(bool            Discard)
{
    int PolicyValue = Discard ? PolicyValueDiscard : PolicyValuePlayout;
    SE_VERBOSE(group_havana, "Discard %d\n", Discard);
    Player->SetPolicy(HavanaPlayback->GetPlayerPlayback(), PlayerStream, PolicyPlayoutOnDrain, PolicyValue);
    PlayerStatus_t Status = PlayerStream->Drain(false, false, NULL, PolicyPlayoutOnDrain, false, NULL);
    if (PlayerNoError != Status)
    {
        SE_ERROR("DrainStream failed\n");
        return HavanaError;
    }
    // i.e. we wait rather than use the event
    return HavanaNoError;
}
//}}}
//{{{  Enable
HavanaStatus_t HavanaStream_c::Enable(bool    Manifest)
{
    ManifestorStatus_t  ManifestorStatus;
    unsigned int        ManifestorCapabilities;
    OS_LockMutex(&ManifestorLock);
    SE_VERBOSE(group_havana, "Manifest = %x\n", Manifest);

    if (PlayerStreamType == StreamTypeVideo)
    {
        for (int i = 0; i < MAX_MANIFESTORS; i++)
        {
            if (Manifestor[i] != NULL)
            {
                //
                // Video blank/unblanck and Audio mute/un-mute feature
                // is currently only supported by Manifestor with MANIFESTOR_CAPABILITY_DISPLAY capability
                //
                Manifestor[i]->GetCapabilities(&ManifestorCapabilities);

                if ((ManifestorCapabilities & (MANIFESTOR_CAPABILITY_DISPLAY)) == MANIFESTOR_CAPABILITY_DISPLAY)
                {
                    class Manifestor_Video_c   *VideoManifestor = (class Manifestor_Video_c *)Manifestor[i];

                    if (Manifest)
                    {
                        ManifestorStatus            = VideoManifestor->Enable();
                    }
                    else
                    {
                        ManifestorStatus            = VideoManifestor->Disable();
                    }

                    if (ManifestorStatus != ManifestorNoError)
                    {
                        SE_ERROR("Failed to enable video output surface\n");
                        OS_UnLockMutex(&ManifestorLock);
                        return HavanaError;
                    }
                }
            }
        }
    }
    else if (PlayerStreamType == StreamTypeAudio)
    {
        SE_WARNING("stm_se_play_stream_set_enable is DEPRECATED for audio streams\n"
                   "Use stm_se_play_stream_set_control(stream, STM_SE_CTRL_PLAY_STREAM_MUTE, mute) instead\n");
        this->SetControl(STM_SE_CTRL_PLAY_STREAM_MUTE,
                         Manifest ? STM_SE_CTRL_VALUE_DISAPPLY : STM_SE_CTRL_VALUE_APPLY);
    }
    else
    {
        SE_ERROR("Unknown stream type\n");
        OS_UnLockMutex(&ManifestorLock);
        return HavanaError;
    }

    OS_UnLockMutex(&ManifestorLock);
    return HavanaNoError;
}
//}}}
//{{{  GetEnable
HavanaStatus_t HavanaStream_c::GetEnable(bool    *Manifest)
{
    unsigned int    ManifestorCapabilities;
    OS_LockMutex(&ManifestorLock);

    if (PlayerStreamType == StreamTypeVideo)
    {
        for (int i = 0; i < MAX_MANIFESTORS; i++)
        {
            if (Manifestor[i] != NULL)
            {
                //
                // Video blank/unblanck and Audio mute/un-mute feature
                // is currently only supported by Manifestor with MANIFESTOR_CAPABILITY_DISPLAY capability
                //
                Manifestor[i]->GetCapabilities(&ManifestorCapabilities);

                if ((ManifestorCapabilities & (MANIFESTOR_CAPABILITY_DISPLAY)) == MANIFESTOR_CAPABILITY_DISPLAY)
                {
                    class Manifestor_Video_c       *VideoManifestor = (class Manifestor_Video_c *)Manifestor[i];
                    *Manifest = VideoManifestor->GetEnable();
                    break;
                }
            }
        }
    }
    else if (PlayerStreamType == StreamTypeAudio)
    {
        int muted = STM_SE_CTRL_VALUE_DISAPPLY;
        GetControl(STM_SE_CTRL_PLAY_STREAM_MUTE, &muted);
        *Manifest = (muted == STM_SE_CTRL_VALUE_APPLY) ? false : true;
    }
    else
    {
        SE_ERROR("Unknown stream type\n");
        OS_UnLockMutex(&ManifestorLock);
        return HavanaError;
    }

    OS_UnLockMutex(&ManifestorLock);
    return HavanaNoError;
}
//}}}
//{{{  SetOption
HavanaStatus_t HavanaStream_c::SetOption(PlayerPolicy_t          Option,
                                         int                     Value)
{
    PlayerStatus_t      Status;
    PlayerStream_t      Stream  = PlayerStream;
    bool                policyHasChanged;
    SE_VERBOSE(group_havana, "%d, 0x%x\n", Option, Value);
    // Value is "known" to be in range because it comes from a lookup table in the wrapper

    if (Option == PolicyDecimateDecoderOutput)
    {
        bool unauthorizedDecimatedPolicy = false;
        int DecimationPolicy = Player->PolicyValue(HavanaPlayback->GetPlayerPlayback(),
                                                   Stream, PolicyDecimateDecoderOutput);
        if ((DecimationPolicy == PolicyValueDecimateDecoderOutputDisabled) &&
            (Value != PolicyValueDecimateDecoderOutputDisabled))
        {
            // No decimated buffer have been allocated
            unauthorizedDecimatedPolicy = true;
        }
        else if ((DecimationPolicy == PolicyValueDecimateDecoderOutputQuarter) &&
                 (Value == PolicyValueDecimateDecoderOutputHalf))
        {
            // Decimated buffer size is too small
            unauthorizedDecimatedPolicy = true;
        }
        if (unauthorizedDecimatedPolicy)
        {
            SE_ERROR("Unable to enable decimation at stream level (policy: %d value: %d)\n",
                     Option, Value);
            return HavanaError;
        }
    }

    if (Option == StmMapOption [STM_SE_CTRL_EXTERNAL_TIME_MAPPING])
    {
        // todo: is this behaviour correct ?
        // if a policy is not applicable on a stream, should we really apply it to all streams ?
        // should we return an error instead ?
        Stream          = PlayerAllStreams;
    }

    policyHasChanged = (Value != Player->PolicyValue(HavanaPlayback->GetPlayerPlayback(), Stream, Option));
    Status  = Player->SetPolicy(HavanaPlayback->GetPlayerPlayback(), Stream, Option, Value);

    if (Status != PlayerNoError)
    {
        SE_ERROR("Unable to set stream option %d, 0x%x\n", Option, Value);
        return HavanaError;
    }

    if (policyHasChanged)
    {
        SE_DEBUG(group_havana, "Stream policy %d has changed\n", Option);

        if ((PolicyAudioDualMonoChannelSelection == Option) ||
            (PolicyAudioStreamDrivenDualMono     == Option) ||
            (PolicyAudioDeEmphasis               == Option))
        {
            UpdateStreamParams(NEW_DUALMONO_CONFIG | NEW_EMPHASIS_CONFIG);
        }
    }

    return HavanaNoError;
}
//}}}
//{{{  GetOption
HavanaStatus_t HavanaStream_c::GetOption(PlayerPolicy_t          Option,
                                         int                    *Value)
{
    *Value              = Player->PolicyValue(HavanaPlayback->GetPlayerPlayback(), PlayerStream, Option);
    SE_VERBOSE(group_havana, "%d, %d\n", Option, *Value);
    return HavanaNoError;
}
//}}}
//{{{  Step
HavanaStatus_t HavanaStream_c::Step(void)
{
    PlayerStatus_t      Status;
    SE_VERBOSE(group_havana, "\n");
    Status      = PlayerStream->Step();

    if (Status != PlayerNoError)
    {
        SE_ERROR("Failed to step stream\n");
        return HavanaError;
    }

    return HavanaNoError;
}
//}}}
//{{{  SetPlayInterval
HavanaStatus_t HavanaStream_c::SetPlayInterval(unsigned long long Start, unsigned long long End)
{
    PlayerStatus_t      Status;
    unsigned long long  PlayStart;
    unsigned long long  PlayEnd;
    SE_VERBOSE(group_havana, "Setting play interval from %llx to %llx\n", Start, End);
    PlayStart           = (Start == STM_SE_PLAY_TIME_NOT_BOUNDED) ? INVALID_TIME : Start;
    PlayEnd             = (End   == STM_SE_PLAY_TIME_NOT_BOUNDED) ? INVALID_TIME : End;
    Status      = Player->SetPresentationInterval(HavanaPlayback->GetPlayerPlayback(), PlayerStream, PlayStart, PlayEnd);

    if (Status != PlayerNoError)
    {
        SE_ERROR("Failed to set play interval - Status = %x\n", Status);
        return HavanaError;
    }

    return HavanaNoError;
}
//}}}
//{{{  ResetStatistics
HavanaStatus_t HavanaStream_c::ResetStatistics(void)
{
    PlayerStream->ResetStatistics();
    return (HavanaNoError);
}
//}}}
//{{{  GetStatistics
HavanaStatus_t HavanaStream_c::GetStatistics(struct statistics_s *Statistics)
{
    SE_ASSERT(sizeof(statistics_s) == sizeof(PlayerStreamStatistics_t));
    *(PlayerStreamStatistics_t *)Statistics = PlayerStream->GetStatistics();
    return (HavanaNoError);
}
//}}}
//{{{  ResetAttributes
HavanaStatus_t HavanaStream_c::ResetAttributes(void)
{
    PlayerStream->ResetAttributes();
    return (HavanaNoError);
}
//}}}
//{{{  GetAttributes
HavanaStatus_t HavanaStream_c::GetAttributes(struct attributes_s *Attributes)
{
    SE_ASSERT(sizeof(struct attributes_s) == sizeof(PlayerStreamAttributes_t)); // n.c...
    *(PlayerStreamAttributes_t *) Attributes = PlayerStream->GetAttributes();
    return (HavanaNoError);
}
//}}}
//{{{  StreamType
PlayerStreamType_t HavanaStream_c::StreamType(void)
{
    SE_VERBOSE(group_havana, "\n");
    return PlayerStreamType;
}
//}}}
//{{{  GetPlayInfo
HavanaStatus_t HavanaStream_c::GetPlayInfo(stm_se_play_stream_info_t *PlayInfo)
{
    SE_VERBOSE(group_havana, "%p\n", Manifestor);

    PlayInfo->pts               = INVALID_TIME;
    PlayInfo->presentation_time = INVALID_TIME;
    PlayInfo->system_time       = INVALID_TIME;
    PlayInfo->frame_count       = 0ull;

    OS_LockMutex(&ManifestorLock);

    // API limitation: can get info from *only* one manifestor
    // choose to have higher prio for DISPLAY vs GRAB vs ENCODE
    // if multiple manifestors of same type (ENCODE for instance),
    // then will take first one..
    int idx_info   = MAX_MANIFESTORS;
    int idx_grab   = MAX_MANIFESTORS;
    int idx_encode = MAX_MANIFESTORS;
    for (int i = 0; i < MAX_MANIFESTORS; i++)
    {
        if (Manifestor[i] != NULL)
        {
            unsigned int                    ManifestorCapabilities;
            Manifestor[i]->GetCapabilities(&ManifestorCapabilities);

            if ((ManifestorCapabilities & MANIFESTOR_CAPABILITY_DISPLAY) != 0)
            {
                idx_info = i;
                SE_DEBUG(group_havana, "using info from display %d\n", idx_info);
                break; // stop on first manifestor display found
            }

            if ((ManifestorCapabilities & MANIFESTOR_CAPABILITY_GRAB) != 0)
            {
                if (MAX_MANIFESTORS == idx_grab) { idx_grab = i; }
                // keep going
            }

            if ((ManifestorCapabilities & MANIFESTOR_CAPABILITY_ENCODE) != 0)
            {
                if (MAX_MANIFESTORS == idx_encode) { idx_encode = i; }
                // keep going
            }
        }
    }

    if (idx_info == MAX_MANIFESTORS)
    {
        if (idx_grab < MAX_MANIFESTORS)
        {
            idx_info = idx_grab;
            SE_DEBUG(group_havana, "using info from grab %d\n", idx_grab);
        }
        else if (idx_encode < MAX_MANIFESTORS)
        {
            idx_info = idx_encode;
            SE_DEBUG(group_havana, "using info from encode %d\n", idx_encode);
        }
    }


    ManifestorStatus_t       Status = ManifestorError;
    if (idx_info < MAX_MANIFESTORS)
    {
        Status = Manifestor[idx_info]->GetNativeTimeOfCurrentlyManifestedFrame(&PlayInfo->pts);
        if (Status == ManifestorNoError)
        {
            Status = Manifestor[idx_info]->GetFrameCount(&PlayInfo->frame_count);
        }
    }

    PlayerStatus_t       StatusRnpt = PlayerNoError;
    // In non synchronized mode ,no mapping between real time and native PTS time
    bool AVDSyncOff = (Player->PolicyValue(HavanaPlayback->GetPlayerPlayback(), PlayerStream, PolicyAVDSynchronization) == PolicyValueDisapply);
    if (AVDSyncOff)
    {
        // without A/V synchro there is no relation between the sysclock and the stream pts time
        PlayInfo->presentation_time = INVALID_TIME;
    }
    else
    {
        // no trace in case of error : done under RetrieveNativePlaybackTime
        StatusRnpt = Player->RetrieveNativePlaybackTime(HavanaPlayback->GetPlayerPlayback(),
                                                        &PlayInfo->presentation_time, TimeFormatUs);
    }

    PlayInfo->system_time = OS_GetTimeInMicroSeconds();


    OS_UnLockMutex(&ManifestorLock);

    SE_DEBUG2(group_havana, group_avsync, "Stream 0x%p PTS=%llu (0x%llx) PT=%llu (0x%llx)\n"
              , GetStream()
              , PlayInfo->pts
              , PlayInfo->pts
              , PlayInfo->presentation_time
              , PlayInfo->presentation_time
             );
    return ((Status == ManifestorNoError) && (StatusRnpt == PlayerNoError)) ? HavanaNoError : HavanaError;
}
//}}}
//{{{  Switch
//{{{  doxynote
/// \brief Create and initialise all of the necessary player components for a new player stream
/// \param HavanaPlayer         Grandparent class
/// \param Player               The player
/// \param PlayerPlayback       The player playback to which the stream will be added
/// \param Media                Textual description of media (audio or video)
/// \param Format               Stream packet format (PES)
/// \param Encoding             The encoding of the stream content (MPEG2/H264 etc)
/// \param Multiplex            Name of multiplex if present (TS)
/// \return                     Havana status code, HavanaNoError indicates success.
//}}}
HavanaStatus_t HavanaStream_c::Switch(stm_se_stream_encoding_t SwitchEncoding)
{
    HavanaStatus_t              Status                  = HavanaNoError;
    PlayerStatus_t              PlayerStatus            = PlayerNoError;
    CodecParameterBlock_t       CodecParameters;
    CodecStatus_t               CodecStatus;
    class Collator_c           *PendingCollator         = NULL;
    class FrameParser_c        *PendingFrameParser      = NULL;
    class Codec_c              *PendingCodec            = NULL;

    OS_LockMutex(&InputLock);

    SE_DEBUG(group_havana, "current Encoding %x, new encoding %x\n", Encoding, SwitchEncoding);

    // force new collator
    Status = HavanaPlayer->CallFactory(ComponentCollator, PlayerStreamType, SwitchEncoding, (void **)&PendingCollator);
    if (Status != HavanaNoError)
    {
        goto factoryFail;
    }

    // force new frame parser
    Status = HavanaPlayer->CallFactory(ComponentFrameParser, PlayerStreamType, SwitchEncoding, (void **)&PendingFrameParser);
    if (Status != HavanaNoError)
    {
        goto factoryFail;
    }

    // force new codec
    Status = HavanaPlayer->CallFactory(ComponentCodec, PlayerStreamType, SwitchEncoding, (void **)&PendingCodec);
    if (Status != HavanaNoError)
    {
        goto factoryFail;
    }

    CodecParameters.ParameterType       = CodecSelectTransformer;
    CodecParameters.Transformer         = TransformerId;
    CodecStatus = PendingCodec->SetModuleParameters(sizeof(CodecParameterBlock_t), &CodecParameters);
    if (CodecStatus != CodecNoError)
    {
        SE_ERROR("Failed to set codec parameters (%08x)\n", CodecStatus);
        Status = HavanaNoMemory;
        goto factoryFail;
    }
    SE_DEBUG(group_havana, "(Stream type %d) Selecting transformer %d\n", PlayerStreamType, TransformerId);

    PlayerStatus = PlayerStream->Switch(PendingCollator,
                                        PendingFrameParser,
                                        PendingCodec,
                                        true);
    if (PlayerStatus != PlayerNoError)
    {
        SE_ERROR("Unable to switch player stream\n");

        if (PlayerStatus == PlayerBusy)
        {
            Status = HavanaBusy;
        }
        else
        {
            Status = HavanaNoMemory;
        }
        goto factoryFail;
    }

    // Wait for all edges to complete switch processing before deleting old
    // stage objects.
    OS_SemaphoreWaitAuto(&PlayerStream->mSemaphoreStreamSwitchComplete);

    // switch collator pointers
    SE_DEBUG(group_havana, "Deleting previous collator:%p new collator:%p\n",
             Collator, PendingCollator);
    delete Collator;
    Collator                = PendingCollator;

    // switch frame parser pointers
    SE_DEBUG(group_havana, "Deleting previous frame_parser:%p  new frame_parser:%p\n",
             FrameParser, PendingFrameParser);
    delete FrameParser;
    FrameParser             = PendingFrameParser;

    // switch codec pointers
    SE_DEBUG(group_havana, "Deleting previous codec:%p new codec:%p\n",
             Codec, PendingCodec);
    delete Codec;
    Codec                   = PendingCodec;

    // switch encoding
#ifdef LOWMEMORYBANDWIDTH
    PlayerStream->SetEncoding(SwitchEncoding);
#endif
    Encoding = SwitchEncoding;

    OS_UnLockMutex(&InputLock);

    return HavanaNoError;

factoryFail:
    delete PendingCollator;
    delete PendingFrameParser;
    delete PendingCodec;

    OS_UnLockMutex(&InputLock);

    return Status;
}
//}}}

//{{{  GetDecodeBuffer
HavanaStatus_t HavanaStream_c::GetDecodeBuffer(stm_pixel_capture_format_t    Format,
                                               unsigned int                  Width,
                                               unsigned int                  Height,
                                               Buffer_t                      *Buffer,
                                               uint32_t                      *LumaAddress,
                                               uint32_t                      *ChromaOffset,
                                               unsigned int                  *Stride,
                                               bool                          NonBlockingInCaseOfFailure)
{
    PlayerStatus_t PlayerStatus = PlayerStream->GetDecodeBuffer(Format, Width, Height,
                                                                Buffer, LumaAddress, ChromaOffset,
                                                                Stride, NonBlockingInCaseOfFailure);

    if (PlayerStatus != PlayerNoError)
    {
        SE_ERROR("Unable to get a decode buffer\n");
        return HavanaError;
    }

    return HavanaNoError;
}
//}}}

//{{{  ReturnDecodeBuffer
HavanaStatus_t HavanaStream_c::ReturnDecodeBuffer(Buffer_t Buffer)
{
    PlayerStatus_t PlayerStatus = PlayerStream->ReturnDecodeBuffer(Buffer);

    if (PlayerStatus != PlayerNoError)
    {
        SE_ERROR("Failed to return buffer\n");
        return HavanaError;
    }

    return HavanaNoError;
}
//}}}

//{{{  RegisterBufferCaptureCallback
stream_buffer_capture_callback HavanaStream_c::RegisterBufferCaptureCallback(stm_se_event_context_h          Context,
                                                                             stream_buffer_capture_callback  Callback)
{
    SE_VERBOSE(group_havana, "\n");

    if (HavanaCapture != NULL)
    {
        return HavanaCapture->RegisterBufferCaptureCallback(Context, Callback);
    }

    SE_ERROR("Capture not created\n");
    return NULL;
}
//}}}
//{{{  GetElementaryBufferLevel
HavanaStatus_t HavanaStream_c::GetElementaryBufferLevel(
    stm_se_ctrl_play_stream_elementary_buffer_level_t    *ElementaryBufferLevel)
{
    SE_VERBOSE(group_havana, "\n");

    if (PlayerStream == NULL)
    {
        SE_ERROR("PlayerStream is null. (%p)\n", PlayerStream);
        return HavanaError;
    }

    return (HavanaStatus_t) PlayerStream->GetElementaryBufferLevel(ElementaryBufferLevel);
}

//}}}
//{{{  GetCompoundControl
HavanaStatus_t HavanaStream_c::GetCompoundControl(
    stm_se_ctrl_t         Ctrl,
    void                 *Value)
{
    switch (Ctrl)
    {
    case  STM_SE_CTRL_PLAY_STREAM_ELEMENTARY_BUFFER_LEVEL:
        GetElementaryBufferLevel((stm_se_ctrl_play_stream_elementary_buffer_level_t *) Value);
        break;

    case STM_SE_CTRL_AUDIO_DYNAMIC_RANGE_COMPRESSION:
        if ((PlayerStreamType == StreamTypeAudio) && (Codec != NULL))
        {
            PlayerStatus_t  PlayerStatus;
            PlayerStatus = Player->GetControl(HavanaPlayback->GetPlayerPlayback(),
                                              PlayerStream,
                                              STM_SE_CTRL_AUDIO_DYNAMIC_RANGE_COMPRESSION,
                                              Value);

            if (PlayerStatus != PlayerNoError)
            {
                SE_ERROR("Failed to get control %d (%08x)\n", Ctrl, PlayerStatus);
                return HavanaError;
            }

            SE_VERBOSE(group_havana, "Get DRC parameters for stream %d\n", PlayerStreamType);
        }

        break;

    case STM_SE_CTRL_SPEAKER_CONFIG:
        if ((PlayerStreamType == StreamTypeAudio) && (Codec != NULL))
        {
            PlayerStatus_t  PlayerStatus;
            PlayerStatus = Player->GetControl(HavanaPlayback->GetPlayerPlayback(),
                                              PlayerStream,
                                              STM_SE_CTRL_SPEAKER_CONFIG,
                                              Value);

            if (PlayerStatus != PlayerNoError)
            {
                SE_ERROR("Failed to get control %d (%08x)\n", Ctrl, PlayerStatus);
                return HavanaError;
            }

            SE_VERBOSE(group_havana, "Get Channel Assignment parameters for stream %d\n", PlayerStreamType);
        }

        break;

    case STM_SE_CTRL_NEGOTIATE_VIDEO_DECODE_BUFFERS:
        //
        // This control is a read only compound control to get the minimal number of the
        // decode buffers required to correctly decode stream of codec type specified in the codec field.
        //
        // In the current implementation, the codec field is not used and a fix number of HD buffers
        // is required instead to support H264 SD and HD normal decode
        //
        stm_se_play_stream_decoded_frame_buffer_info_t   *ShareBufferControl;
        ShareBufferControl = (stm_se_play_stream_decoded_frame_buffer_info_t *)Value;
        // 1920x1088 decoded buffer size
        ShareBufferControl->buffer_size = DECODE_BUFFER_HD_SIZE;
        // HW constraints  (0=no attributes)
        ShareBufferControl->mem_attribute  = 0;
        // HW constraints  (0 = any alignment)
        ShareBufferControl->buffer_alignement = 0;
        // Required number of decode buffer (16) to normal decode
        // for SD and HD h264 stream
        ShareBufferControl->number_of_buffers = 16;
        break;

    case STM_SE_CTRL_MPEG2_TIME_CODE:
        if ((PlayerStreamType == StreamTypeVideo) && (Encoding == STM_SE_STREAM_ENCODING_VIDEO_MPEG2))
        {
            FrameParserStatus_t FrameParserStatus;
            FrameParserStatus = PlayerStream->GetFrameParser()->GetMpeg2TimeCode((stm_se_ctrl_mpeg2_time_code_t *) Value);

            if (FrameParserStatus != FrameParserNoError)
            {
                SE_ERROR("Failed to get control %d (%08x)\n", Ctrl, FrameParserStatus);
                return HavanaError;
            }
        }
        else
        {
            SE_ERROR("Not supported ctrl %x\n", Ctrl);
            return HavanaError;
        }

        break;

    default:
        SE_ERROR("Not supported ctrl %x\n", Ctrl);
        return HavanaError;
    }

    return HavanaNoError;
}
//}}}
//{{{  SetCompoundControl
HavanaStatus_t HavanaStream_c::SetCompoundControl(
    stm_se_ctrl_t         Ctrl,
    void                 *Value)
{
    switch (Ctrl)
    {
    case STM_SE_CTRL_AAC_DECODER_CONFIG:
        if (PlayerStreamType == StreamTypeAudio)
        {
            // store parameters
            PlayerStatus_t  PlayerStatus;
            PlayerStatus = Player->SetControl(HavanaPlayback->GetPlayerPlayback(),
                                              PlayerStream,
                                              STM_SE_CTRL_AAC_DECODER_CONFIG,
                                              Value);
            if (PlayerStatus != PlayerNoError)
            {
                SE_ERROR("Failed to set control %d (%08x)\n", Ctrl, PlayerStatus);
                return HavanaError;
            }

            SE_VERBOSE(group_havana, "Specifying AAC config for stream %d\n", PlayerStreamType);
            UpdateStreamParams(NEW_AAC_CONFIG);
        }
        break;

    case STM_SE_CTRL_AUDIO_DYNAMIC_RANGE_COMPRESSION:
        if (PlayerStreamType == StreamTypeAudio)
        {
            // store parameters
            PlayerStatus_t  PlayerStatus;
            PlayerStatus = Player->SetControl(HavanaPlayback->GetPlayerPlayback(),
                                              PlayerStream,
                                              STM_SE_CTRL_AUDIO_DYNAMIC_RANGE_COMPRESSION,
                                              Value);

            if (PlayerStatus != PlayerNoError)
            {
                SE_ERROR("Failed to set control %d (%08x)\n", Ctrl, PlayerStatus);
                return HavanaError;
            }

            UpdateStreamParams(NEW_DRC_CONFIG);
            SE_VERBOSE(group_havana, "Selecting DRC parameters for stream %d\n", PlayerStreamType);
        }

        break;

    case STM_SE_CTRL_SPEAKER_CONFIG:
        if (PlayerStreamType == StreamTypeAudio)
        {
            // 1- store control parameters (can get them with the corresponding Get function
            PlayerStatus_t  PlayerStatus;
            PlayerStatus = Player->SetControl(HavanaPlayback->GetPlayerPlayback(),
                                              PlayerStream,
                                              STM_SE_CTRL_SPEAKER_CONFIG,
                                              Value);

            if (PlayerStatus != PlayerNoError)
            {
                SE_ERROR("Failed to set control %d (%08x)\n", Ctrl, PlayerStatus);
                return HavanaError;
            }

            UpdateStreamParams(NEW_SPEAKER_CONFIG);
            SE_VERBOSE(group_havana, "Specifying SpeakerConfig parameters for stream %d\n", PlayerStreamType);
        }
        break;

    case STM_SE_CTRL_SHARE_VIDEO_DECODE_BUFFERS:

        //
        // This control is a write only compound control to give a list of decode buffers to the play stream.
        // In the current implementation this control can be called only once for a given play_stream,
        // mean the whole bunch of buffers which form the decode pool has to be available at the time of this call
        //
        if (!DecodeBufferManager)
        {
            SE_ERROR("Failed to set control for individual decode buffer - DecodeBufferManager is missing\n");
            return HavanaError;
        }

        DecodeBufferManagerParameterBlock_t              Parameters;
        stm_se_play_stream_decoded_frame_buffer_info_t   *ShareBufferControl;
        ShareBufferControl = (stm_se_play_stream_decoded_frame_buffer_info_t *)Value;
        memset(&Parameters, 0, sizeof(DecodeBufferManagerParameterBlock_t));
        Parameters.ParameterType   = DecodeBufferManagerPrimaryIndividualBufferList;
        Parameters.PrimaryIndividualData.BufSize       = ShareBufferControl->buffer_size;
        Parameters.PrimaryIndividualData.NumberOfBufs  = ShareBufferControl->number_of_buffers;

        //
        // Make sure number of buffer is < MAX_DECODE_BUFFER
        //
        if (Parameters.PrimaryIndividualData.NumberOfBufs > MAX_DECODE_BUFFERS)
        {
            SE_ERROR("Too many individual buffer (%d)\n", Parameters.PrimaryIndividualData.NumberOfBufs);
            return HavanaError;
        }

// TODO Check supplied buffers are >= requested according codec/profile

        if (ShareBufferControl->buf_list == NULL)
        {
            SE_ERROR("List of buffers is missing : buf_list = NULL\n");
            return HavanaError;
        }

        for (unsigned int i = 0 ; i < Parameters.PrimaryIndividualData.NumberOfBufs; i++)
        {
            Parameters.PrimaryIndividualData.AllocatorMemory[i][PhysicalAddress] = ShareBufferControl->buf_list[i].physical_address;
            Parameters.PrimaryIndividualData.AllocatorMemory[i][CachedAddress]   = ShareBufferControl->buf_list[i].virtual_address;
            Parameters.PrimaryIndividualData.AllocatorMemory[i][2] = NULL;
        }

        //
        // Transmit individual buffer to DecodeBufferManager
        //
        if (DecodeBufferManager->SetModuleParameters(sizeof(Parameters), &Parameters) != DecodeBufferManagerNoError)
        {
            SE_ERROR("Failed to set control for individual decode buffer\n");
            return HavanaError;
        }

        //
        // Now switch to same codec to take into account individual buffers
        //
        if (Switch(Encoding) != HavanaNoError)
        {
            SE_ERROR("Failed to switch to individual decode buffer\n");
            return HavanaError;
        }

        break;

    case  STM_SE_CTRL_PLAY_STREAM_ELEMENTARY_BUFFER_LEVEL:
    default:
        SE_ERROR("Not supported ctrl %x\n", Ctrl);
        return HavanaError;
    }

    return HavanaNoError;
}
//}}}
//{{{  SetAlarm
HavanaStatus_t HavanaStream_c::SetAlarm(stm_se_play_stream_alarm_t   alarm,
                                        bool  enable,
                                        void *value)
{
    PlayerStatus_t Status;
    Status = PlayerStream->SetAlarm(alarm, enable, value);
    if (Status != PlayerNoError)
    {
        SE_ERROR("Status %d\n", Status);
        return HavanaError;
    }

    return HavanaNoError;
}
//}}}
//{{{  SetDiscardTrigger
HavanaStatus_t HavanaStream_c::SetDiscardTrigger(stm_se_play_stream_discard_trigger_t const &trigger)
{
    PlayerStatus_t Status;
    Status = PlayerStream->SetDiscardTrigger(trigger);
    if (Status != PlayerNoError)
    {
        SE_ERROR("Status %d\n", Status);
        return HavanaError;
    }

    return HavanaNoError;
}
//}}}
//{{{  ResetDiscardTrigger
HavanaStatus_t HavanaStream_c::ResetDiscardTrigger(void)
{
    PlayerStatus_t Status;
    Status = PlayerStream->ResetDiscardTrigger();
    if (Status != PlayerNoError)
    {
        SE_ERROR("Status %d\n", Status);
        return HavanaError;
    }

    return HavanaNoError;
}
//}}}
//{{{  GetControl
HavanaStatus_t HavanaStream_c::GetControl(
    stm_se_ctrl_t         Ctrl,
    int                 *Value)
{
    switch (Ctrl)
    {
    case STM_SE_CTRL_STREAM_DRIVEN_STEREO:
    case STM_SE_CTRL_PLAY_STREAM_NBCHANNELS:
    case STM_SE_CTRL_PLAY_STREAM_SAMPLING_FREQUENCY:
        if (PlayerStreamType == StreamTypeAudio)
        {
            PlayerStatus_t  PlayerStatus;
            PlayerStatus = Player->GetControl(HavanaPlayback->GetPlayerPlayback(),
                                              PlayerStream,
                                              Ctrl,
                                              (void *) Value);

            if (PlayerStatus != PlayerNoError)
            {
                SE_ERROR("Failed to get control %d (%08x)\n", Ctrl, PlayerStatus);
                return HavanaError;
            }
        }

        break;

    case STM_SE_CTRL_PLAY_STREAM_MUTE:
        if (PlayerStreamType == StreamTypeAudio)
        {
            PlayerStatus_t PlayerStatus = Player->GetControl(HavanaPlayback->GetPlayerPlayback(),
                                                             PlayerStream,
                                                             STM_SE_CTRL_PLAY_STREAM_MUTE,
                                                             (void *) Value);

            if (PlayerStatus != PlayerNoError)
            {
                SE_ERROR("Failed to get STM_SE_CTRL_PLAY_STREAM_MUTE control (%08x)\n", PlayerStatus);
                return HavanaError;
            }
        }

        break;

    default:
        SE_ERROR("Not supported ctrl %x\n", Ctrl);
        return HavanaError;
    }

    return HavanaNoError;
}
//}}}
//{{{  SetControl
HavanaStatus_t HavanaStream_c::SetControl(
    stm_se_ctrl_t         Ctrl,
    int                 Value)
{
    int audio_update = 0;

    switch (Ctrl)
    {
    case  STM_SE_CTRL_STREAM_DRIVEN_STEREO:
        audio_update = NEW_STEREO_CONFIG;
        SE_VERBOSE(group_havana, "Set StreamDrivenStereo=%s on audio stream\n", Value ? "true" : "false");
        break;
    case  STM_SE_CTRL_PLAY_STREAM_SAMPLING_FREQUENCY:
        audio_update = NEW_SAMPLING_FREQUENCY_CONFIG;
        SE_VERBOSE(group_havana, "Set SamplingFreq=%d on audio stream\n", Value);
        break;

    case  STM_SE_CTRL_PLAY_STREAM_NBCHANNELS:
        audio_update = NEW_NBCHAN_CONFIG;
        SE_VERBOSE(group_havana, "Set NbChannels=%d on audio stream\n", Value);
        break;

    case STM_SE_CTRL_PLAY_STREAM_MUTE:
        audio_update = NEW_MUTE_CONFIG;
        SE_VERBOSE(group_havana, "Set Mute=%s on audio stream\n", Value ? "true" : "false");
        break;

    default:
        SE_ERROR("Not supported ctrl %x\n", Ctrl);
        return HavanaError;
    }

    if (audio_update)
    {
        if (PlayerStreamType == StreamTypeAudio)
        {
            PlayerStatus_t  PlayerStatus;
            PlayerStatus = Player->SetControl(HavanaPlayback->GetPlayerPlayback(),
                                              PlayerStream,
                                              Ctrl,
                                              (void *) Value);

            if (PlayerStatus != PlayerNoError)
            {
                SE_ERROR("Failed to set control %d (%08x)\n", Ctrl, PlayerStatus);
                return HavanaError;
            }

            UpdateStreamParams(audio_update);
        }
        else
        {
            SE_ERROR("This control belongs to Audio Stream Type but PlayStreamType is not audio\n");
            return HavanaError;
        }
    }

    return HavanaNoError;
}
//}}}
//{{{  UpdateStreamParam
HavanaStatus_t HavanaStream_c::UpdateStreamParams(unsigned int Config)
{
    PlayerStream->ApplyDecoderConfig(Config);
    return HavanaNoError;
}
//}}}
//{{{  GetTransformerId
unsigned int    HavanaStream_c::GetTransformerId(void)
{
    return TransformerId;
}
//}}}

//{{{  CreatePassThroughMixer
HavanaStatus_t  HavanaStream_c::CreatePassThrough(void)
{
    stm_se_mixer_spec_t     topology;

    // reset the topology to the definition of bypass :
    topology.type = STM_SE_MIXER_BYPASS;

    topology.nb_max_decoded_audio     = 1;
    topology.nb_max_application_audio = 0;
    topology.nb_max_interactive_audio = 0;
    topology.nb_max_players           = STM_SE_MIXER_NB_MAX_OUTPUTS;

    AudioPassThrough = new Mixer_Mme_c("AudioPassThrough",
                                       mixerTransformerArray[0].transformerName, sizeof(mixerTransformerArray[0].transformerName),
                                       topology);

    if (AudioPassThrough == NULL)
    {
        SE_ERROR("Unable to create AudioPassThrough context - insufficient memory\n");
        return HavanaNoMemory;
    }

    SE_DEBUG(group_havana, "Created mixer 0x%p\n", AudioPassThrough);
    return HavanaNoError;
}
//}}}
//{{{  AttachToPassThrough
HavanaStatus_t  HavanaStream_c::AttachToPassThrough(stm_object_h audio_player)
{
    Audio_Player_c *hw_player = (Audio_Player_c *) audio_player;

    if (PlayerNoError != AudioPassThrough->SendAttachPlayer(hw_player))
    {
        SE_ERROR("Error attaching mixer 0x%p to player 0x%p\n", AudioPassThrough, hw_player);
        return HavanaError;
    }

    SE_DEBUG(group_havana, "Attaching 0x%p to 0x%p\n", hw_player, AudioPassThrough);
    return HavanaNoError;
}
//}}}
//{{{  DetachFromPassThrough
HavanaStatus_t  HavanaStream_c::DetachFromPassThrough(stm_object_h audio_player)
{
    Audio_Player_c *hw_player = (Audio_Player_c *) audio_player;

    if (PlayerNoError != AudioPassThrough->SendDetachPlayer(hw_player))
    {
        SE_ERROR("Error detaching mixer 0x%p to player 0x%p\n", AudioPassThrough, hw_player);
        return HavanaError;
    }

    SE_DEBUG(group_havana, "Detaching 0x%p to 0x%p\n", hw_player, AudioPassThrough);
    return HavanaNoError;
}
//}}}
//{{{  DeletePassThroughMixer
HavanaStatus_t  HavanaStream_c::DeletePassThrough(void)
{
    if (AudioPassThrough->IsDisconnected() != PlayerNoError)
    {
        SE_ERROR("PassThroughMixer 0x%p still connected - cannot be deleted\n", AudioPassThrough);
        return HavanaError;
    }

    SE_DEBUG(group_havana, "Delete PassThroughMixer 0x%p\n", AudioPassThrough);
    delete AudioPassThrough;
    AudioPassThrough = NULL;
    return HavanaNoError;
}
//}}}
//{{{  AddAudioPlayer
HavanaStatus_t  HavanaStream_c::AddAudioPlayer(stm_object_h sink)
{
    HavanaStatus_t status = HavanaNoError;
    int            free_idx = -1;

    SE_VERBOSE(group_havana, "\n");
    if (sink == NULL)
    {
        SE_ERROR("Invalid sink\n");
        return HavanaError;
    }

    OS_LockMutex(&AudioPassThroughLock);
    for (int i = 0; i < MAX_AUDIO_PLAYERS; i++)
    {
        if ((AudioPlayers[i] == NULL) && (free_idx == -1))
        {
            free_idx = i;
        }

        if (AudioPlayers[i] == sink)
        {
            OS_UnLockMutex(&AudioPassThroughLock);
            SE_ERROR("This sink 0x%p is already attached!\n", sink);
            return HavanaError;
        }
    }

    if (free_idx == -1)
    {
        OS_UnLockMutex(&AudioPassThroughLock);
        SE_ERROR("No player placeholder has been found for this sink 0x%p!\n", sink);
        return HavanaError;
    }

    bool new_mixer = false;
    if (NbAudioPlayerSinks == 0)
    {
        status    = CreatePassThrough();
        new_mixer = true;
    }

    if (status == HavanaNoError)
    {
        status = AttachToPassThrough(sink);

        if (status == HavanaNoError)
        {
            AudioPlayers[free_idx] = sink;
            NbAudioPlayerSinks ++;
        }
    }

    OS_UnLockMutex(&AudioPassThroughLock);

    if ((status == HavanaNoError) && (new_mixer == false))
    {
        return HavanaMixerAlreadyExists;
    }
    return status;
}
//}}}
//{{{  RemoveAudioPlayer
HavanaStatus_t  HavanaStream_c::RemoveAudioPlayer(stm_object_h sink)
{
    SE_VERBOSE(group_havana, "\n");
    if (sink == NULL)
    {
        SE_ERROR("Invalid sink\n");
        return HavanaError;
    }

    int i;

    OS_LockMutex(&AudioPassThroughLock);

    for (i = 0; i < MAX_AUDIO_PLAYERS; i++)
    {
        if (AudioPlayers[i] == sink)
        {
            break;
        }
    }

    if (i == MAX_AUDIO_PLAYERS)
    {
        OS_UnLockMutex(&AudioPassThroughLock);
        SE_ERROR("This sink 0x%p is not attached!\n", sink);
        return HavanaError;
    }

    // !! the mixer will deadlock the play-stream if no player is attached to it.
    // Hence, before detatching the last player from the passthrough mixer we
    // should detach the play-stream from the passthrough mixer.

    if (NbAudioPlayerSinks == 1)
    {
        HavanaPlayer->DeleteDisplay(this, AudioPassThrough);
    }

    HavanaStatus_t status = DetachFromPassThrough(sink);

    if (status == HavanaNoError)
    {
        AudioPlayers[i] = NULL;
        NbAudioPlayerSinks --;

        if (NbAudioPlayerSinks == 0)
        {
            DeletePassThrough();
        }
    }

    OS_UnLockMutex(&AudioPassThroughLock);
    return status;
}
//}}}

//{{{  LowPowerEnter
HavanaStatus_t  HavanaStream_c::LowPowerEnter(void)
{
    SE_VERBOSE(group_havana, "\n");

    // Call drain to flush all incoming buffers
    // No need to take the InputLock around the Drain.
    Drain(true);

    // Call player related method
    // It must be called after Drain as it will stop the decode thread
    PlayerStream->LowPowerEnter();
    return HavanaNoError;
}
//}}}
//{{{  LowPowerExit
HavanaStatus_t  HavanaStream_c::LowPowerExit(void)
{
    SE_VERBOSE(group_havana, "\n");
    // Call player related method
    PlayerStream->LowPowerExit();
    return HavanaNoError;
}
//}}}
//{{{  ConfigureDecodeBufferManager
HavanaStatus_t  HavanaStream_c::ConfigureDecodeBufferManager(void)
{
    int MemProfilePolicy, DecimationPolicy, HdmiRxModePolicy;
    DecodeBufferManagerParameterBlock_t  Parameters;

    memset(&Parameters, 0, sizeof(DecodeBufferManagerParameterBlock_t));
    Parameters.ParameterType       = DecodeBufferManagerPartitionData;

    if (PlayerStreamType == StreamTypeVideo)
    {
        unsigned int Size, CopySize, MBStructSize;
        unsigned int AdditionalBlockSize  = 0x1000;   // This is taken from the rmv code (128*8)
        unsigned int EffectiveTransformerId = TransformerId;

        MemProfilePolicy = Player->PolicyValue(HavanaPlayback->GetPlayerPlayback(),
                                               PlayerAllStreams, PolicyVideoPlayStreamMemoryProfile);

        HdmiRxModePolicy = Player->PolicyValue(HavanaPlayback->GetPlayerPlayback(),
                                               PlayerAllStreams, PolicyHdmiRxMode);

        SE_INFO(group_havana, "MemProfile = %d HdmiRxMode = %d\n", MemProfilePolicy, HdmiRxModePolicy);

        switch (HdmiRxModePolicy)
        {

        // HDMI-Rx use case
        // As a summary, the buffer pool size guesstimated are the following:
        // |input                 |A/V synchro       | buffer pool size |
        // |----------------------|------------------|------------------|
        // |HD ready, full HD     | repeater mode    |6                 |
        // |UHD                   | repeater mode    |6                 |
        // |HD ready, full HD     | non repeater mode|12                |
        // |UHD                   | non repeater mode|7                 |

        case STM_SE_CTRL_VALUE_HDMIRX_REPEATER:
            switch (MemProfilePolicy)
            {
            case PolicyValueVideoPlayStreamMemoryProfileHDMIRepeaterUHD:
                Size                            = 6 * UHD_444_BUFFER_SIZE;
                CopySize                        = 0; //No reference buffers needed
                break;

            case PolicyValueVideoPlayStreamMemoryProfileHDMIRepeaterHD:
                Size                            = 6 * HD_444_BUFFER_SIZE;
                CopySize                        = 0; //No reference buffers needed
                break;

            default:
                //Set default to HDMIRx Repeater HD memory profile
                SE_WARNING("For HDMIRxMode: %d incorrect MemoryProfile: %d is set\n", HdmiRxModePolicy, MemProfilePolicy);
                SE_WARNING("For HDMIRxMode: %d reset MemoryProfile to %d\n", HdmiRxModePolicy, PolicyValueVideoPlayStreamMemoryProfileHDMIRepeaterHD);
                Player->SetPolicy(HavanaPlayback->GetPlayerPlayback(), PlayerStream, PolicyVideoPlayStreamMemoryProfile, PolicyValueVideoPlayStreamMemoryProfileHDMIRepeaterHD);
                Size                            = 6 * HD_444_BUFFER_SIZE;
                CopySize                        = 0; //No reference buffers needed
                break;
            }
            break;

        case STM_SE_CTRL_VALUE_HDMIRX_NON_REPEATER:
            switch (MemProfilePolicy)
            {
            case PolicyValueVideoPlayStreamMemoryProfileHDMINonRepeaterUHD:
                Size                            = 7 * UHD_444_BUFFER_SIZE;
                CopySize                        = 0; //No reference buffers needed
                break;

            case PolicyValueVideoPlayStreamMemoryProfileHDMINonRepeaterHD:
                Size                            = 12 * HD_444_BUFFER_SIZE;
                CopySize                        = 0; //No reference buffers needed
                break;

            default:
                //Set default to HDMIRx Non Repeater HD memory profile
                SE_WARNING("For HDMIRxMode: %d incorrect MemoryProfile: %d is set\n", HdmiRxModePolicy, MemProfilePolicy);
                SE_WARNING("For HDMIRxMode: %d reset MemoryProfile to %d\n", HdmiRxModePolicy, PolicyValueVideoPlayStreamMemoryProfileHDMINonRepeaterHD);
                Player->SetPolicy(HavanaPlayback->GetPlayerPlayback(), PlayerStream, PolicyVideoPlayStreamMemoryProfile, PolicyValueVideoPlayStreamMemoryProfileHDMINonRepeaterHD);
                Size                            = 12 * HD_444_BUFFER_SIZE;
                CopySize                        = 0; //No reference buffers needed
                break;
            }
            break;

        case STM_SE_CTRL_VALUE_HDMIRX_DISABLED:
        default:
            // Keep pool size based in video decoding memory profiles.
            // Size = number of raster buffers X YUV420 frame size
            // CopySize = number of reference buffers X YUV420 frame size
            switch (MemProfilePolicy)
            {
            case PolicyValueVideoPlayStreamMemoryProfileHDOptimized:
                SE_INFO(group_havana, "HD optimized memory usage\n");
                Size = 9 * HD_420_BUFFER_SIZE;
                CopySize = 7 * HD_420_BUFFER_SIZE;
                break;

            case PolicyValueVideoPlayStreamMemoryProfile4K2K:
                SE_INFO(group_havana, "4K2K memory usage\n");
                Size = 9 * UHD_4K2K_420_BUFFER_SIZE;
                CopySize = 6 * UHD_4K2K_420_BUFFER_SIZE;
                break;

            case PolicyValueVideoPlayStreamMemoryProfileUHD:
                SE_INFO(group_havana, "UHD memory usage\n");
                Size = 9 * UHD_420_BUFFER_SIZE;
                CopySize = 7 * UHD_420_BUFFER_SIZE;
                break;

            case PolicyValueVideoPlayStreamMemoryProfileSD:
                SE_INFO(group_havana, "SD memory usage\n");
                Size = 9 * SD_420_BUFFER_SIZE;
                CopySize = 12 * SD_420_BUFFER_SIZE;
                break;

            case PolicyValueVideoPlayStreamMemoryProfileHD720p:
                SE_INFO(group_havana, "HD 720p memory usage\n");
                Size = 9 * HD_720P_420_BUFFER_SIZE;
                CopySize = 9 * HD_720P_420_BUFFER_SIZE;
                break;

            case PolicyValueVideoPlayStreamMemoryProfileHDLegacy:
            default:
                SE_INFO(group_havana, "HD max legacy memory usage\n");
                Size = 9 * HD_420_BUFFER_SIZE;
                CopySize = Size;
                break;
            }
            break;
        }

        /* MBStructSize stands for amount of memory reserved to the PPB buffers,
           one PPB per FB, each PPB size = HD MBstructure size x nb of MBs (8160 MBs per HD picture)
           HD MBstructure size = 48bytes (instead of 64 previously), 384 is the 420 MB size */
        MBStructSize = 48 * CopySize / 384;

        SE_VERBOSE(group_havana, "PrimaryManifestationPartitionSize: %d VideoDecodeCopyPartitionSize: %d\n",
                   Size, CopySize);

        // partitions will be appended with -0 or -1 depending TransformerId
#define DECL_P(name, partition) \
        char name[] = partition; name[sizeof(name)-2] = '0' + EffectiveTransformerId;
        DECL_P(Partition,            "vid-output-X");
        DECL_P(DecimatedPartition,   "vid-decimated-X");
        DECL_P(CopyPartition,        "vid-copied-X");
        DECL_P(MacroblockPartition,  "vid-macroblock-X");
        DECL_P(PostProcessPartition, "vid-postproc-X");
        DECL_P(AdditionalPartition,  "vid-extra-data-X");
#undef DECL_P
        Parameters.PartitionData.NumberOfDecodeBuffers                    = MAX_VIDEO_DECODE_BUFFERS;

        //
        // In case decode buffers are provided by user, do not allocate buffer yet
        //
        const char *part = NULL;
        if (Player->PolicyValue(HavanaPlayback->GetPlayerPlayback(), PlayerAllStreams, PolicyDecodeBuffersUserAllocated) == PolicyValueApply)
        {
            part = AllocatedLaterPartition;
            // Reset default value for next playstream new
            Player->SetPolicy(PlayerAllPlaybacks, PlayerAllStreams,   PolicyDecodeBuffersUserAllocated, PolicyValueDisapply);
        }
        else
        {
            part = Partition;
        }

        Parameters.PartitionData.PrimaryManifestationPartitionSize = Size;

        strncpy(Parameters.PartitionData.PrimaryManifestationPartitionName, part,
                sizeof(Parameters.PartitionData.PrimaryManifestationPartitionName));
        Parameters.PartitionData.PrimaryManifestationPartitionName[
            sizeof(Parameters.PartitionData.PrimaryManifestationPartitionName) - 1] = '\0';

        DecimationPolicy = Player->PolicyValue(HavanaPlayback->GetPlayerPlayback(),
                                               PlayerAllStreams, PolicyDecimateDecoderOutput);
        if (DecimationPolicy == PolicyValueDecimateDecoderOutputHalf)
        {
            Parameters.PartitionData.DecimatedManifestationPartitionSize = Size / 4;
            SE_INFO(group_player, "Decimation policy: OutputHalf (size: %d bytes)\n",
                    Parameters.PartitionData.DecimatedManifestationPartitionSize);
        }
        else if (DecimationPolicy == PolicyValueDecimateDecoderOutputQuarter)
        {
            //we only support decimation factor of 4 horizontally and 2 vertically
            Parameters.PartitionData.DecimatedManifestationPartitionSize =  Size / 8;
            SE_INFO(group_player, "Decimation policy: OutputQuarter (size: %d bytes)\n",
                    Parameters.PartitionData.DecimatedManifestationPartitionSize);
        }
        else
        {
            Parameters.PartitionData.DecimatedManifestationPartitionSize = 0;
            SE_INFO(group_player, "Decimated buffers NOT allocated\n");
        }
        strncpy(Parameters.PartitionData.DecimatedManifestationPartitionName, DecimatedPartition,
                sizeof(Parameters.PartitionData.DecimatedManifestationPartitionName));
        Parameters.PartitionData.DecimatedManifestationPartitionName[
            sizeof(Parameters.PartitionData.DecimatedManifestationPartitionName) - 1] = '\0';

        Parameters.PartitionData.VideoDecodeCopyPartitionSize             = CopySize;
        strncpy(Parameters.PartitionData.VideoDecodeCopyPartitionName, CopyPartition,
                sizeof(Parameters.PartitionData.VideoDecodeCopyPartitionName));
        Parameters.PartitionData.VideoDecodeCopyPartitionName[
            sizeof(Parameters.PartitionData.VideoDecodeCopyPartitionName) - 1] = '\0';

        Parameters.PartitionData.VideoMacroblockStructurePartitionSize    = MBStructSize;
        strncpy(Parameters.PartitionData.VideoMacroblockStructurePartitionName, MacroblockPartition,
                sizeof(Parameters.PartitionData.VideoMacroblockStructurePartitionName));
        Parameters.PartitionData.VideoMacroblockStructurePartitionName[
            sizeof(Parameters.PartitionData.VideoMacroblockStructurePartitionName) - 1] = '\0';

        Parameters.PartitionData.VideoPostProcessControlPartitionSize     = 0;
        strncpy(Parameters.PartitionData.VideoPostProcessControlPartitionName, "",
                sizeof(Parameters.PartitionData.VideoPostProcessControlPartitionName));
        Parameters.PartitionData.VideoPostProcessControlPartitionName[
            sizeof(Parameters.PartitionData.VideoPostProcessControlPartitionName) - 1] = '\0';

        Parameters.PartitionData.AdditionalMemoryBlockPartitionSize       = AdditionalBlockSize;
        strncpy(Parameters.PartitionData.AdditionalMemoryBlockPartitionName, AdditionalPartition,
                sizeof(Parameters.PartitionData.AdditionalMemoryBlockPartitionName));
        Parameters.PartitionData.AdditionalMemoryBlockPartitionName[
            sizeof(Parameters.PartitionData.AdditionalMemoryBlockPartitionName) - 1] = '\0';
    }
    else
    {
#define DECL_P(name, partition) \
        char name[] = partition; name[sizeof(name)-2] = '0' + TransformerId;
        DECL_P(Partition,            "aud-output-X");
#undef DECL_P
        Parameters.PartitionData.NumberOfDecodeBuffers                    = 32;
        Parameters.PartitionData.PrimaryManifestationPartitionSize        = AUDIO_BUFFER_MEMORY;
        strncpy(Parameters.PartitionData.PrimaryManifestationPartitionName, Partition,
                sizeof(Parameters.PartitionData.PrimaryManifestationPartitionName));
        Parameters.PartitionData.PrimaryManifestationPartitionName[
            sizeof(Parameters.PartitionData.PrimaryManifestationPartitionName) - 1] = '\0';
    }

    DecodeBufferManager->SetModuleParameters(sizeof(Parameters), &Parameters);

    return HavanaNoError;
}
//}}}
