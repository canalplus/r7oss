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

#ifndef H_HAVANA_PLAYBACK
#define H_HAVANA_PLAYBACK

#include "player.h"
#include "havana_player.h"

#undef TRACE_TAG
#define TRACE_TAG   "HavanaPlayback_c"

class HavanaStream_c;

/// Player wrapper class responsible for managing a playback.
class HavanaPlayback_c
{
public:
    HavanaPlayback_c(void);
    ~HavanaPlayback_c(void);

    HavanaStatus_t              Init(class HavanaPlayer_c           *HavanaPlayer,
                                     class Player_c                 *Player,
                                     class BufferManager_c          *BufferManager);

    HavanaStatus_t              AddStream(stm_se_media_t                  Media,
                                          stm_se_stream_encoding_t        Encoding,
                                          class HavanaStream_c          **HavanaStream);
    HavanaStatus_t              RemoveStream(class HavanaStream_c        *HavanaStream);

    HavanaStatus_t              SetSpeed(int                             PlaySpeed);
    HavanaStatus_t              GetSpeed(int                            *PlaySpeed);
    HavanaStatus_t              SetNativePlaybackTime(unsigned long long NativeTime,
                                                      unsigned long long SystemTime);
    HavanaStatus_t              SetOption(PlayerPolicy_t                 Option,
                                          int                            Value);
    HavanaStatus_t              GetOption(PlayerPolicy_t                 Option,
                                          int                           *Value);

    HavanaStatus_t              SetClockDataPoint(stm_se_time_format_t   TimeFormat,
                                                  bool                   NewSequence,
                                                  unsigned long long     SourceTime,
                                                  unsigned long long     SystemTime);
    HavanaStatus_t              GetClockDataPoint(unsigned long long    *SourceTime,
                                                  unsigned long long    *SystemTime);

    HavanaStatus_t              GetStatistics(struct __stm_se_playback_statistics_s *Statistics);
    HavanaStatus_t              ResetStatistics(void);

    HavanaStatus_t              LowPowerEnter(void);
    HavanaStatus_t              LowPowerExit(void);

    PlayerPlayback_t GetPlayerPlayback()
    {
        return PlayerPlayback;
    };

private:
    PlayDirection_t             Direction;
    unsigned int                CurrentSpeed;

    OS_Mutex_t                  Lock;

    class HavanaPlayer_c       *HavanaPlayer;
    class HavanaStream_c       *Stream[MAX_STREAMS_PER_PLAYBACK];

    class BufferManager_c      *BufferManager;
    class Player_c             *Player;
    PlayerPlayback_t            PlayerPlayback;

    class OutputCoordinator_c  *OutputCoordinator;

    DISALLOW_COPY_AND_ASSIGN(HavanaPlayback_c);
};

//{{{  doxynote
/// \class HavanaPlayback_c
/// \Description The \c HavanaPlayback_c class manages playbacks.  It is a fairly thin layer

//}}}

#endif
