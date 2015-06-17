/************************************************************************
COPYRIGHT (C) STMicroelectronics 2005

Source file name : havana_playback.cpp - derived from havana_player.cpp
Author :           Julian

Implementation of the playback module for havana.


Date        Modification                                    Name
----        ------------                                    --------
14-Feb-07   Created                                         Julian

************************************************************************/

#include "backend_ops.h"
#include "fixed.h"
#include "player.h"
#include "output_coordinator_base.h"
#include "havana_player.h"
#include "havana_playback.h"
#include "havana_demux.h"
#include "havana_stream.h"

//{{{  HavanaPlayback_c
HavanaPlayback_c::HavanaPlayback_c (void)
{
    int i;

    PLAYBACK_DEBUG("\n");

    for (i = 0; i < MAX_STREAMS_PER_PLAYBACK; i++)
        Stream[i]       = NULL;

    for (i = 0; i < MAX_DEMUX_CONTEXTS; i++)
        Demux[i]        = NULL;

    PlayerPlayback      = NULL;
    OutputCoordinator   = NULL;

    LockInitialised     = false;

}
//}}}
//{{{  ~HavanaPlayback_c
HavanaPlayback_c::~HavanaPlayback_c (void)
{
    int i;

    PLAYBACK_DEBUG("\n");

    if (OutputCoordinator != NULL)
        delete OutputCoordinator;

    for (i = 0; i < MAX_STREAMS_PER_PLAYBACK; i++)
    {
        if (Stream[i] != NULL)
        {
            delete Stream[i];
            Stream[i]   = NULL;
        }
    }
    for (i = 0; i < MAX_DEMUX_CONTEXTS; i++)
    {
        if (Demux[i] != NULL)
        {
            delete Demux[i];
            Demux[i]    = NULL;
        }
    }

    if (PlayerPlayback != NULL)
    {
        Player->TerminatePlayback (PlayerPlayback, true);
        PlayerPlayback  = NULL;
    }

    if (LockInitialised)
    {
        OS_TerminateMutex (&Lock);
        LockInitialised         = false;
    }

}
//}}}
//{{{  Init
HavanaStatus_t HavanaPlayback_c::Init  (class HavanaPlayer_c*   HavanaPlayer,
                                        class Player_c*         Player,
                                        class BufferManager_c*  BufferManager)
{
    PlayerStatus_t      PlayerStatus    = PlayerNoError;

    PLAYBACK_DEBUG("\n");

    this->HavanaPlayer          = HavanaPlayer;
    this->Player                = Player;
    this->BufferManager         = BufferManager;

    if (!LockInitialised)
    {
        if (OS_InitializeMutex (&Lock) != OS_NO_ERROR)
        {
            PLAYBACK_ERROR ("Failed to initialize InputLock mutex\n");
            return HavanaNoMemory;
        }
        LockInitialised         = true;
    }

    if (OutputCoordinator == NULL)
        OutputCoordinator   = new OutputCoordinator_Base_c();
    if (OutputCoordinator == NULL)
    {
        PLAYBACK_ERROR("Unable to create output coordinator\n");
        return HavanaNoMemory;
    }

    if (PlayerPlayback == NULL)
    {
        PlayerStatus    = Player->CreatePlayback (OutputCoordinator, &PlayerPlayback, true);    // Indicate we want the creation event
        if (PlayerStatus != PlayerNoError)
        {
            PLAYBACK_ERROR("Unable to create playback context %x\n", PlayerStatus);
            return HavanaNoMemory;
        }
    }

    //    PLAYBACK_DEBUG("%p\n", PlayerPlayback);

    return HavanaNoError;
}
//}}}

//{{{  AddDemux
//{{{  doxynote
/// \brief  Create a new demux context
/// \return Havana status code, HavanaNoError indicates success.
//}}}
HavanaStatus_t HavanaPlayback_c::AddDemux      (unsigned int                    DemuxId,
                                                class HavanaDemux_c**           HavanaDemux)
{
    HavanaStatus_t              Status;
    class Demultiplexor_c*      Demultiplexor;
    DemultiplexorContext_t      DemuxContext;

    if (DemuxId >= MAX_DEMUX_CONTEXTS)
        return HavanaError;

    if (Demux[DemuxId] != NULL)
    {
        *HavanaDemux        = Demux[DemuxId];
        return HavanaNoError;
    }

    PLAYBACK_DEBUG("Getting demux context for Demux %d\n", DemuxId);
    if (HavanaPlayer->GetDemuxContext (DemuxId, &Demultiplexor, &DemuxContext) != HavanaNoError)
    {
        PLAYBACK_ERROR("Unable to create demux context\n");
        return HavanaNoMemory;
    }

    Demux[DemuxId]   = new HavanaDemux_c();
    if (Demux[DemuxId] == NULL)
    {
        PLAYBACK_ERROR("Unable to create demux context - insufficient memory\n");
        return HavanaNoMemory;
    }

    Status      = Demux[DemuxId]->Init (Player,
                                        PlayerPlayback,
                                        DemuxContext);
    if (Status != HavanaNoError)
    {
        delete Demux[DemuxId];
        Demux[DemuxId]          = NULL;
        return Status;
    }

    *HavanaDemux        = Demux[DemuxId];
    PLAYBACK_DEBUG("%p\n", Demux[DemuxId]);

    return HavanaNoError;
}
//}}}
//{{{  RemoveDemux
HavanaStatus_t HavanaPlayback_c::RemoveDemux   (class HavanaDemux_c*   HavanaDemux)
{
    int         i;

    PLAYBACK_DEBUG("\n");
    if (HavanaDemux == NULL)
        return HavanaDemuxInvalid;

    for (i = 0; i < MAX_DEMUX_CONTEXTS; i++)
    {
        if (Demux[i] == HavanaDemux)
            break;
    }
    if (i == MAX_DEMUX_CONTEXTS)
    {
        PLAYBACK_ERROR("Unable to locate demux context for delete\n");
        return HavanaDemuxInvalid;
    }

    delete Demux[i];
    PLAYBACK_DEBUG("%p\n", Demux[i]);
    Demux[i]            = NULL;

    return HavanaNoError;
}
//}}}

//{{{  Active
//{{{  doxynote
/// \brief Ask playback if it has any streams are currently registered
///        Used to determin if playback is available for use.
/// \return Havana status code, HavanaNoError indicates no registered streams.
//}}}
HavanaStatus_t HavanaPlayback_c::Active  (void)
{
    int                 i;

    PLAYBACK_DEBUG("\n");

    for (i = 0; i < MAX_STREAMS_PER_PLAYBACK; i++)
    {
        if (Stream[i] != NULL)
            return HavanaPlaybackActive;
    }

    return HavanaNoError;
}
//}}}
//{{{  AddStream
//{{{  doxynote
/// \brief Create a new stream, initialise it and add it to this playback
/// \param Media        Textual description of media (audio or video)
/// \param Format       Stream packet format (PES)
/// \param Encoding     The encoding of the stream content (MPEG2/H264 etc)
/// \param SurfaceId    Topological identifier for PiP and speaker-in-speaker.
/// \param HavanaStream Reference to created stream
/// \return Havana status code, HavanaNoError indicates success.
//}}}
HavanaStatus_t HavanaPlayback_c::AddStream     (char*                   Media,
                                                char*                   Format,
                                                char*                   Encoding,
                                                unsigned int            SurfaceId,
                                                class HavanaStream_c**  HavanaStream)
{
    HavanaStatus_t      Status;
    int                 i;

    //if (*HavanaStream != NULL)                  // Device already has this stream
    //    return HavanaStreamAlreadyExists;

    OS_LockMutex (&Lock);
    for (i = 0; i < MAX_STREAMS_PER_PLAYBACK; i++)
    {
        if (Stream[i] == NULL)
            break;
    }

    if (i == MAX_STREAMS_PER_PLAYBACK)
    {
        PLAYBACK_ERROR("Unable to create stream context - Too many streams\n");
        OS_UnLockMutex (&Lock);
        return HavanaTooManyStreams;
    }

    Stream[i]   = new HavanaStream_c();
    if (Stream[i] == NULL)
    {
        PLAYBACK_ERROR("Unable to create stream context - insufficient memory\n");
        OS_UnLockMutex (&Lock);
        return HavanaNoMemory;
    }

    Status      = Stream[i]->Init      (HavanaPlayer,
                                        Player,
                                        PlayerPlayback,
                                        Media,
                                        Format,
                                        Encoding,
                                        SurfaceId);

    if (Status != HavanaNoError)
    {
        delete Stream[i];
        Stream[i]       = NULL;
        OS_UnLockMutex (&Lock);
        return Status;
    }

    //PLAYBACK_DEBUG ("Adding stream %p, %d\n", Stream[i], i);
    *HavanaStream       = Stream[i];

    OS_UnLockMutex (&Lock);

    return HavanaNoError;
}
//}}}
//{{{  RemoveStream
HavanaStatus_t HavanaPlayback_c::RemoveStream  (class HavanaStream_c*   HavanaStream)
{
    int         i;

    PLAYBACK_DEBUG("\n");
    if (HavanaStream == NULL)
        return HavanaStreamInvalid;

    OS_LockMutex (&Lock);

    for (i = 0; i < MAX_STREAMS_PER_PLAYBACK; i++)
    {
        if (Stream[i] == HavanaStream)
            break;
    }
    if (i == MAX_STREAMS_PER_PLAYBACK)
    {
        PLAYBACK_ERROR("Unable to locate stream for delete\n");
        OS_UnLockMutex (&Lock);
        return HavanaStreamInvalid;
    }

    //PLAYBACK_DEBUG ("Removing stream %p, %d\n", Stream[i], i);
    delete Stream[i];
    Stream[i]   = NULL;

    OS_UnLockMutex (&Lock);

    return HavanaNoError;
}
//}}}
//{{{  SetSpeed
HavanaStatus_t HavanaPlayback_c::SetSpeed   (int        PlaySpeed)
{
    PlayerStatus_t      Status;
    PlayDirection_t     Direction;
    Rational_t          Speed;

    if ((unsigned int)PlaySpeed == PLAY_SPEED_REVERSE_STOPPED)
    {
        Direction       = PlayBackward;
        PlaySpeed       = PLAY_SPEED_STOPPED;
    }
    else if (PlaySpeed >= 0)
        Direction       = PlayForward;
    else
    {
        Direction       = PlayBackward;
        PlaySpeed       = -PlaySpeed;
    }

    Speed               = Rational_t(PlaySpeed, PLAY_SPEED_NORMAL_PLAY);

    PLAYBACK_DEBUG("Setting speed to %d.%06d\n", Speed.IntegerPart(), Speed.RemainderDecimal());
    Status      = Player->SetPlaybackSpeed     (PlayerPlayback, Speed, Direction);
    if (Status != PlayerNoError)
    {
        PLAYBACK_ERROR("Failed to set speed - Status = %x\n", Status);
        return HavanaError;
    }

    return HavanaNoError;
}
//}}}
//{{{  GetSpeed
HavanaStatus_t HavanaPlayback_c::GetSpeed   (int*       PlaySpeed)
{
    PlayerStatus_t      Status;
    PlayDirection_t     Direction;
    Rational_t          Speed;

    Status      = Player->GetPlaybackSpeed     (PlayerPlayback, &Speed, &Direction);
    if (Status != PlayerNoError)
    {
        PLAYBACK_ERROR("Failed to get speed - Status = %x\n", Status);
        return HavanaError;
    }
    PLAYBACK_DEBUG("Getting speed of %d.%06d\n", Speed.IntegerPart(), Speed.RemainderDecimal());

    *PlaySpeed          = (int)IntegerPart (Speed * PLAY_SPEED_NORMAL_PLAY);
    if (((*PlaySpeed) == PLAY_SPEED_STOPPED) && (Direction != PlayForward))
        *PlaySpeed      = PLAY_SPEED_REVERSE_STOPPED;
    else if (Direction != PlayForward)
        *PlaySpeed      = -*PlaySpeed;

    return HavanaNoError;
}
//}}}
//{{{  SetNativePlaybackTime
HavanaStatus_t HavanaPlayback_c::SetNativePlaybackTime (unsigned long long      NativeTime,
                                                        unsigned long long      SystemTime)
{
    PlayerStatus_t      Status;

    Status      = Player->SetNativePlaybackTime(PlayerPlayback,  NativeTime, SystemTime);
    if (Status != PlayerNoError)
    {
        PLAYBACK_ERROR("Unable to SetNativePlaybackTime \n");
        return HavanaError;
    }

    return HavanaNoError;
}
//}}}
//{{{  SetOption
HavanaStatus_t HavanaPlayback_c::SetOption     (play_option_t           Option,
                                                unsigned int            Value)
{
    unsigned char       PolicyValue     = 0;
    PlayerPolicy_t      PlayerPolicy;
    PlayerStatus_t      Status;

    switch (Option)
    {
        case PLAY_OPTION_MASTER_CLOCK:
            PlayerPolicy        = PolicyMasterClock;
            switch (Value)
            {
                case PLAY_OPTION_VALUE_VIDEO_CLOCK_MASTER:
                    PolicyValue = PolicyValueVideoClockMaster;
                    break;
                case PLAY_OPTION_VALUE_AUDIO_CLOCK_MASTER:
                    PolicyValue = PolicyValueAudioClockMaster;
                    break;
                default:
                    PolicyValue = PolicyValueSystemClockMaster;
                    break;
            }
            break;
        case PLAY_OPTION_DISCARD_LATE_FRAMES:
            PlayerPolicy        = PolicyDiscardLateFrames;
            switch (Value)
            {
                case PLAY_OPTION_VALUE_DISCARD_LATE_FRAMES_NEVER:
                    PolicyValue = PolicyValueDiscardLateFramesNever;
                    break;
                case PLAY_OPTION_VALUE_DISCARD_LATE_FRAMES_ALWAYS:
                    PolicyValue = PolicyValueDiscardLateFramesAlways;
                    break;
                default:
                    PolicyValue = PolicyValueDiscardLateFramesAfterSynchronize;
                    break;
            }
            break;
        case PLAY_OPTION_VIDEO_START_IMMEDIATE:
            PlayerPolicy        = PolicyVideoStartImmediate;
            PolicyValue         = (Value == PLAY_OPTION_VALUE_ENABLE) ? PolicyValueApply : PolicyValueDisapply;
            break;
        case PLAY_OPTION_PTS_SYMMETRIC_JUMP_DETECTION:
            PlayerPolicy        = PolicySymmetricJumpDetection;
            PolicyValue         = Value;
            break;
        case PLAY_OPTION_PTS_FORWARD_JUMP_DETECTION_THRESHOLD:
            PlayerPolicy        = PolicyPtsForwardJumpDetectionThreshold;
            PolicyValue         = Value;
            break;
        case PLAY_OPTION_ALLOW_FRAME_DISCARD_AT_NORMAL_SPEED:
            PlayerPolicy        = PolicyAllowFrameDiscardAtNormalSpeed;
            PolicyValue         = Value;
            break;
        case PLAY_OPTION_OPERATE_COLLATOR2_IN_REVERSIBLE_MODE:
            PlayerPolicy        = PolicyOperateCollator2InReversibleMode;
            PolicyValue         = Value;
            break;
        case PLAY_OPTION_SYNC_START_IMMEDIATE:
            PlayerPolicy        = PolicySyncStartImmediate;
            PolicyValue         = Value;
            break;
        default:
            PLAYBACK_ERROR("Unknown option %d\n", Option);
            return HavanaError;

    }

    Status  = Player->SetPolicy (PlayerPlayback, PlayerAllStreams, PlayerPolicy, PolicyValue);
    if (Status != PlayerNoError)
    {
        PLAYBACK_ERROR("Unable to set playback option %x, %x\n", PlayerPolicy, PolicyValue);
        return HavanaError;
    }
    return HavanaNoError;
}
//}}}
//{{{  SetClockDataPoint
HavanaStatus_t HavanaPlayback_c::SetClockDataPoint     (clock_data_point_t*        DataPoint)
{
    PlayerStatus_t              Status;
    PlayerTimeFormat_t          SourceTimeFormat        = (DataPoint->time_format == DVB_TIME_FORMAT_US) ? TimeFormatUs : TimeFormatPts;

    Status      = Player->ClockRecoveryInitialize      (PlayerPlayback, SourceTimeFormat);

    Status      = Player->ClockRecoveryDataPoint       (PlayerPlayback, DataPoint->source_time, DataPoint->system_time);
    if (Status != PlayerNoError)
    {
        PLAYBACK_ERROR("Unable to set clock data point\n");
        return HavanaError;
    }

    return HavanaNoError;
}
//}}}

//{{{  CheckEvent
HavanaStatus_t HavanaPlayback_c::CheckEvent      (struct PlayerEventRecord_s*     PlayerEvent)
{
    int                 i;
    HavanaStatus_t      HavanaStatus    = HavanaError;

    OS_LockMutex (&Lock);                       // Make certain we cannot delete stream while checking the event
    for (i = 0; i < MAX_STREAMS_PER_PLAYBACK; i++)
    {
        if (Stream[i] != NULL)
        {
            HavanaStatus    = Stream[i]->CheckEvent (PlayerEvent);
            if (HavanaStatus == HavanaNoError)
                break;
        }
    }
    OS_UnLockMutex (&Lock);

    return HavanaStatus;
}
//}}}
//{{{  GetPlayerEnvironment
HavanaStatus_t HavanaPlayback_c::GetPlayerEnvironment    (PlayerPlayback_t*               PlayerPlayback)
{
    //STREAM_DEBUG("\n");

    if ((this->PlayerPlayback == NULL))
    {
        STREAM_ERROR ("PlayerPlayback parameter is null. (%p) \n", this->PlayerPlayback);
        return HavanaError;
    }

    *PlayerPlayback     = this->PlayerPlayback;

    return HavanaNoError;
}
//}}}

