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

#include "player.h"
#include "output_coordinator_base.h"
#include "havana_player.h"
#include "havana_stream.h"
#include "havana_playback.h"

#undef TRACE_TAG
#define TRACE_TAG   "HavanaPlayback_c"

//{{{  HavanaPlayback_c
HavanaPlayback_c::HavanaPlayback_c(void)
    : Direction(PlayForward)
    , CurrentSpeed()
    , Lock()
    , HavanaPlayer(NULL)
    , Stream()
    , BufferManager()
    , Player(NULL)
    , PlayerPlayback(NULL)
    , OutputCoordinator(NULL)
{
    SE_VERBOSE(group_havana, "\n");

    OS_InitializeMutex(&Lock);
}
//}}}
//{{{  ~HavanaPlayback_c
HavanaPlayback_c::~HavanaPlayback_c(void)
{
    SE_VERBOSE(group_havana, "\n");

    for (int i = 0; i < MAX_STREAMS_PER_PLAYBACK; i++)
    {
        delete Stream[i];
    }

    if (PlayerPlayback != NULL)
    {
        Player->TerminatePlayback(PlayerPlayback, true);
    }

    delete OutputCoordinator;

    OS_TerminateMutex(&Lock);
}
//}}}
//{{{  Init
HavanaStatus_t HavanaPlayback_c::Init(class HavanaPlayer_c   *HavanaPlayer,
                                      class Player_c         *Player,
                                      class BufferManager_c  *BufferManager)
{
    PlayerStatus_t      PlayerStatus    = PlayerNoError;
    SE_VERBOSE(group_havana, "\n");
    this->HavanaPlayer          = HavanaPlayer;
    this->Player                = Player;
    this->BufferManager         = BufferManager;

    if (OutputCoordinator == NULL)
    {
        OutputCoordinator   = new OutputCoordinator_Base_c();
    }

    if (OutputCoordinator == NULL)
    {
        SE_ERROR("Unable to create output coordinator\n");
        return HavanaNoMemory;
    }

    if (PlayerPlayback == NULL)
    {
        PlayerStatus    = Player->CreatePlayback(OutputCoordinator, &PlayerPlayback, true);     // Indicate we want the creation event
        if (PlayerStatus != PlayerNoError)
        {
            SE_ERROR("Unable to create playback context %x\n", PlayerStatus);
            delete OutputCoordinator;
            OutputCoordinator = NULL;
            return HavanaNoMemory;
        }
    }

    SE_VERBOSE(group_havana, "%p\n", PlayerPlayback);
    return HavanaNoError;
}
//}}}

//{{{  AddStream
//{{{  doxynote
/// \brief Create a new stream, initialise it and add it to this playback
/// \param Media        Textual description of media (audio or video)
/// \param Encoding     The encoding of the stream content (MPEG2/H264 etc)
/// \param HavanaStream Reference to created stream
/// \return Havana status code, HavanaNoError indicates success.
//}}}
HavanaStatus_t HavanaPlayback_c::AddStream(stm_se_media_t                  Media,
                                           stm_se_stream_encoding_t        Encoding,
                                           class HavanaStream_c          **HavanaStream)
{
    HavanaStatus_t      Status;
    int                 i;
    int                 InstanceID;
    class HavanaStream_c *newStream = NULL;
    SE_VERBOSE(group_havana, "Media %d Encoding %d\n", Media, Encoding);
    // We need to lock the HavanaPlayer_c mutex first, and then the HavanaPlayback_c mutex
    // This is to prevent lock inversions when calling the HavanaPlayer->AttachStream() method (see bug 23317)
    OS_LockMutex(&this->HavanaPlayer->Lock);
    OS_LockMutex(&Lock);

    for (i = 0; i < MAX_STREAMS_PER_PLAYBACK; i++)
    {
        if (Stream[i] == NULL)
        {
            break;
        }
    }

    if (i == MAX_STREAMS_PER_PLAYBACK)
    {
        SE_ERROR("Unable to create stream context - Too many streams\n");
        OS_UnLockMutex(&Lock);
        OS_UnLockMutex(&this->HavanaPlayer->Lock);
        return HavanaTooManyStreams;
    }

    newStream = new HavanaStream_c();
    if (newStream == NULL)
    {
        SE_ERROR("Unable to create stream context - insufficient memory\n");
        OS_UnLockMutex(&Lock);
        OS_UnLockMutex(&this->HavanaPlayer->Lock);
        return HavanaNoMemory;
    }

    // Add stream to the current player stream list and retrieve its ID.
    HavanaPlayer->AttachStream(newStream, Media, &InstanceID);
    // Initialize new stream instance
    Status      = newStream->Init(HavanaPlayer,
                                  Player,
                                  this,
                                  Media,
                                  Encoding,
                                  InstanceID);
    if (Status != HavanaNoError)
    {
        // Remove the stream from the current player stream list.
        HavanaPlayer->DetachStream(newStream, Media);
        delete newStream;
        OS_UnLockMutex(&Lock);
        OS_UnLockMutex(&this->HavanaPlayer->Lock);
        return Status;
    }

    //
    // Playback can be informed now
    //
    Stream[i] = newStream;
    *HavanaStream = Stream[i];
    SE_DEBUG(group_havana, "Adding stream %p, %d\n", Stream[i], i);
    OS_UnLockMutex(&Lock);
    OS_UnLockMutex(&this->HavanaPlayer->Lock);
    return HavanaNoError;
}
//}}}
//{{{  RemoveStream
HavanaStatus_t HavanaPlayback_c::RemoveStream(class HavanaStream_c   *HavanaStream)
{
    int               i;
    stm_se_media_t    Media;
    SE_VERBOSE(group_havana, "\n");

    if (HavanaStream == NULL)
    {
        return HavanaStreamInvalid;
    }

    // We need to lock the HavanaPlayer_c mutex first, and then the HavanaPlayback_c mutex
    // This is to prevent lock inversions when calling the HavanaPlayer->DetachStream() method (see bug 23317)
    OS_LockMutex(&this->HavanaPlayer->Lock);
    OS_LockMutex(&Lock);

    for (i = 0; i < MAX_STREAMS_PER_PLAYBACK; i++)
    {
        if (Stream[i] == HavanaStream)
        {
            break;
        }
    }

    if (i == MAX_STREAMS_PER_PLAYBACK)
    {
        SE_ERROR("Unable to locate stream for delete\n");
        OS_UnLockMutex(&Lock);
        OS_UnLockMutex(&this->HavanaPlayer->Lock);
        return HavanaStreamInvalid;
    }

    if (Stream[i]->StreamType() == StreamTypeAudio)
    {
        Media = STM_SE_MEDIA_AUDIO;
    }
    else if (Stream[i]->StreamType() == StreamTypeVideo)
    {
        Media = STM_SE_MEDIA_VIDEO;
    }
    else
    {
        Media = STM_SE_MEDIA_ANY;
    }

    // Stream removed: remove it also from the current player stream list.
    HavanaPlayer->DetachStream(Stream[i], Media);
    SE_DEBUG(group_havana, "Removing stream %p, %d\n", Stream[i], i);
    delete Stream[i];
    Stream[i]   = NULL;
    OS_UnLockMutex(&Lock);
    OS_UnLockMutex(&this->HavanaPlayer->Lock);
    return HavanaNoError;
}
//}}}
//{{{  SetSpeed
HavanaStatus_t HavanaPlayback_c::SetSpeed(int        PlaySpeed)
{
    PlayerStatus_t      Status;
    PlayDirection_t     Direction;
    Rational_t          Speed, MinSpeed, MaxSpeed;

    if (PlaySpeed == STM_SE_PLAYBACK_SPEED_REVERSE_STOPPED)
    {
        Direction       = PlayBackward;
        PlaySpeed       = STM_SE_PLAYBACK_SPEED_STOPPED;
    }
    else if (PlaySpeed >= 0)
    {
        Direction       = PlayForward;
    }
    else
    {
        Direction       = PlayBackward;
        PlaySpeed       = -PlaySpeed;
    }

    Speed               = Rational_t(PlaySpeed, STM_SE_PLAYBACK_SPEED_NORMAL_PLAY);
    MaxSpeed = MAXIMUM_SPEED_SUPPORTED; // rational
    MinSpeed = Rational_t(1, FRACTIONAL_MINIMUM_SPEED_SUPPORTED);

    if (Speed > MaxSpeed)
    {
        Speed = MaxSpeed;
    }

    if (PlaySpeed != STM_SE_PLAYBACK_SPEED_STOPPED &&
        Speed < MinSpeed)
    {
        Speed = MinSpeed;
    }

    SE_DEBUG(group_havana, "Setting speed to %d.%06d\n", Speed.IntegerPart(), Speed.RemainderDecimal());
    Status      = PlayerPlayback->SetPlaybackSpeed(Speed, Direction);

    if (Status != PlayerNoError)
    {
        SE_ERROR("Failed to set speed - Status = %x\n", Status);
        return HavanaError;
    }

    return HavanaNoError;
}
//}}}
//{{{  GetSpeed
HavanaStatus_t HavanaPlayback_c::GetSpeed(int       *PlaySpeed)
{
    PlayerStatus_t      Status;
    PlayDirection_t     Direction;
    Rational_t          Speed;
    Status      = Player->GetPlaybackSpeed(PlayerPlayback, &Speed, &Direction);

    if (Status != PlayerNoError)
    {
        SE_ERROR("Failed to get speed - Status = %x\n", Status);
        return HavanaError;
    }

    SE_DEBUG(group_havana, "Getting speed of %d.%06d\n", Speed.IntegerPart(), Speed.RemainderDecimal());
    *PlaySpeed          = (int)IntegerPart(Speed * STM_SE_PLAYBACK_SPEED_NORMAL_PLAY);

    if (((*PlaySpeed) == STM_SE_PLAYBACK_SPEED_STOPPED) && (Direction != PlayForward))
    {
        *PlaySpeed      = STM_SE_PLAYBACK_SPEED_REVERSE_STOPPED;
    }
    else if (Direction != PlayForward)
    {
        *PlaySpeed      = -*PlaySpeed;
    }

    return HavanaNoError;
}
//}}}
//{{{  SetNativePlaybackTime
HavanaStatus_t HavanaPlayback_c::SetNativePlaybackTime(unsigned long long      NativeTime,
                                                       unsigned long long      SystemTime)
{
    PlayerStatus_t      Status;
    Status      = Player->SetNativePlaybackTime(PlayerPlayback,  NativeTime, SystemTime);

    if (Status != PlayerNoError)
    {
        SE_ERROR("Unable to SetNativePlaybackTime\n");
        return HavanaError;
    }

    return HavanaNoError;
}
//}}}
//{{{  SetOption
HavanaStatus_t HavanaPlayback_c::SetOption(PlayerPolicy_t          Option,
                                           int                     Value)
{
    PlayerStatus_t      Status;
    bool                policyHasChanged;
    SE_VERBOSE(group_havana, "%d, %d\n", Option, Value);
    // Value is "known" to be in range because it comes from a lookup table in the wrapper
    policyHasChanged = (Value != Player->PolicyValue(PlayerPlayback, PlayerAllStreams, Option));
    Status  = Player->SetPolicy(PlayerPlayback, PlayerAllStreams, Option, Value);

    if (Status != PlayerNoError)
    {
        SE_ERROR("Unable to set playback option %x, %x\n", Option, Value);
        return HavanaError;
    }

    if (policyHasChanged)
    {
        SE_DEBUG(group_havana, "Playback policy %d has changed\n", Option);

        if ((PolicyAudioDualMonoChannelSelection == Option) ||
            (PolicyAudioStreamDrivenDualMono     == Option) ||
            (PolicyAudioDeEmphasis               == Option))
        {
            for (int i = 0; i < MAX_STREAMS_PER_PLAYBACK; i++)
            {
                if (Stream[i] != NULL)
                {
                    Status = Stream[i]->UpdateStreamParams(NEW_DUALMONO_CONFIG | NEW_EMPHASIS_CONFIG);

                    if (Status != HavanaNoError)
                    {
                        SE_ERROR("Unable to set playback policy %d on stream[%d]\n", Option, i);
                        return HavanaError;
                    }
                }
            }
        }
    }

    return HavanaNoError;
}
//}}}
//{{{  GetOption
HavanaStatus_t HavanaPlayback_c::GetOption(PlayerPolicy_t           Option,
                                           int                    *Value)
{
    *Value = Player->PolicyValue(PlayerPlayback, PlayerAllStreams, Option);
    SE_DEBUG(group_havana, "%x, %x\n", Option, *Value);
    return HavanaNoError;
}
//}}}
//{{{  SetClockDataPoint
HavanaStatus_t HavanaPlayback_c::SetClockDataPoint(stm_se_time_format_t    TimeFormat,
                                                   bool                    NewSequence,
                                                   unsigned long long      SourceTime,
                                                   unsigned long long      SystemTime)
{
    PlayerStatus_t              Status;
    PlayerTimeFormat_t          SourceTimeFormat        = (TimeFormat == TIME_FORMAT_US) ? TimeFormatUs : TimeFormatPts;
    bool                        OutputCoordInitialized  = false;
    // ClockRecovery is not initialized when there's no A/V stream attached and it's not a new PCR sequence, so initialize it now.
    // This can happen when zapping between streams with the same PCR PID
    PlayerPlayback->OutputCoordinator->ClockRecoveryIsInitialized(&OutputCoordInitialized);

    if (NewSequence || OutputCoordInitialized == false)
    {
        Status  = Player->ClockRecoveryInitialize(PlayerPlayback, SourceTimeFormat);
    }

    SE_VERBOSE2(group_havana, group_avsync, "Received PCR - (SourceTime=%llu, SystemTime=%llu)\n", SourceTime, SystemTime);

    Status      = Player->ClockRecoveryDataPoint(PlayerPlayback, SourceTime, SystemTime);

    if (Status != PlayerNoError)
    {
        SE_ERROR("Unable to set clock data point\n");
        return HavanaError;
    }

    return HavanaNoError;
}
//}}}
//{{{  GetClockDataPoint
HavanaStatus_t HavanaPlayback_c::GetClockDataPoint(unsigned long long     *SourceTime,
                                                   unsigned long long     *SystemTime)
{
    PlayerStatus_t              Status;
    Status      = Player->ClockRecoveryEstimate(PlayerPlayback, SourceTime, SystemTime);

    if (Status != PlayerNoError)
    {
        SE_ERROR("Unable to retrieve clock data point\n");
        return HavanaError;
    }

    SE_DEBUG(group_havana, "Clock estimate - (%llu, %llu, %llu)\n", *SourceTime, *SystemTime, *SourceTime - *SystemTime);
    return HavanaNoError;
}
//}}}
//{{{  ResetStatistics
HavanaStatus_t HavanaPlayback_c::ResetStatistics(void)
{
    Player->ResetStatistics(PlayerPlayback);
    return (HavanaNoError);
}
//}}}
//{{{  GetStatistics
HavanaStatus_t HavanaPlayback_c::GetStatistics(struct __stm_se_playback_statistics_s *Statistics)
{
    struct PlayerPlaybackStatistics_s PlayerStatistics = Player->GetStatistics(PlayerPlayback);
    memcpy(Statistics, &PlayerStatistics, sizeof(struct PlayerPlaybackStatistics_s));
    return (HavanaNoError);
}
//}}}

//{{{  LowPowerEnter
HavanaStatus_t  HavanaPlayback_c::LowPowerEnter(void)
{
    int i;
    SE_VERBOSE(group_havana, "\n");
    // Call player related method
    // It must be called first to allow stopping collator action before calling Drain (bug 24248)
    Player->PlaybackLowPowerEnter(PlayerPlayback);
    // Lock access to Playback
    OS_LockMutex(&Lock);

    // Call Streams method
    for (i = 0; i < MAX_STREAMS_PER_PLAYBACK; i++)
    {
        if (Stream[i] != NULL)
        {
            Stream[i]->LowPowerEnter();
        }
    }

    // Unlock access to Playback
    OS_UnLockMutex(&Lock);
    return HavanaNoError;
}
//}}}

//{{{  LowPowerExit
HavanaStatus_t  HavanaPlayback_c::LowPowerExit(void)
{
    int i;
    SE_VERBOSE(group_havana, "\n");
    // Lock access to Playback
    OS_LockMutex(&Lock);

    // Call Streams method
    for (i = 0; i < MAX_STREAMS_PER_PLAYBACK; i++)
    {
        if (Stream[i] != NULL)
        {
            Stream[i]->LowPowerExit();
        }
    }

    // Unlock access to Playback
    OS_UnLockMutex(&Lock);
    // Call player related method
    Player->PlaybackLowPowerExit(PlayerPlayback);
    return HavanaNoError;
}
//}}}

