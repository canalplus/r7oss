/************************************************************************
Copyright (C) 2005 STMicroelectronics. All Rights Reserved.

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

Source file name : havana_playback.h derived from havana_player.h
Author :           Nick

Definition of the implementation of havana playback module for havana.


Date        Modification                                    Name
----        ------------                                    --------
14-Feb-07   Created                                         Julian

************************************************************************/

#ifndef H_HAVANA_PLAYBACK
#define H_HAVANA_PLAYBACK

#include "player.h"
#include "havana_player.h"

#define MAX_STREAMS_PER_PLAYBACK        32

/*      Debug printing macros   */
#ifndef ENABLE_PLAYBACK_DEBUG
#define ENABLE_PLAYBACK_DEBUG           0
#endif

#define PLAYBACK_DEBUG(fmt, args...)      ((void) (ENABLE_PLAYBACK_DEBUG && \
                                            (report(severity_note, "HavanaPlayback_c::%s: " fmt, __FUNCTION__, ##args), 0)))

/* Output trace information off the critical path */
#define PLAYBACK_TRACE(fmt, args...)      (report(severity_note, "HavanaPlayback_c::%s: " fmt, __FUNCTION__, ##args))
/* Output errors, should never be output in 'normal' operation */
#define PLAYBACK_ERROR(fmt, args...)      (report(severity_error, "HavanaPlayback_c::%s: " fmt, __FUNCTION__, ##args))


class HavanaStream_c;

/// Player wrapper class responsible for managing a playback.
class HavanaPlayback_c
{

private:
    PlayDirection_t             Direction;

    class OutputCoordinator_c*  OutputCoordinator;

    unsigned int                CurrentSpeed;
    bool                        RealTime;
    bool                        SyncState;
    bool                        SurroundSoundOn;
    /*
    AspectRatio_t               AspectRatio;
    DisplayFormat_t             DisplayFormat;
    */
    OS_Mutex_t                  Lock;
    bool                        LockInitialised;

    class HavanaPlayer_c*       HavanaPlayer;
    class HavanaStream_c*       Stream[MAX_STREAMS_PER_PLAYBACK];
    class HavanaDemux_c*        Demux[MAX_DEMUX_CONTEXTS];

    class BufferManager_c*      BufferManager;
    class Player_c*             Player;
    PlayerPlayback_t            PlayerPlayback;

public:

                                HavanaPlayback_c               (void);

                               ~HavanaPlayback_c               (void);

    HavanaStatus_t              Init                           (class HavanaPlayer_c*           HavanaPlayer,
                                                                class Player_c*                 Player,
                                                                class BufferManager_c*          BufferManager);

    HavanaStatus_t              Active                         (void);

    HavanaStatus_t              AddDemux                       (unsigned int                    DemuxId,
                                                                class HavanaDemux_c**           HavanaDemux);
    HavanaStatus_t              RemoveDemux                    (class HavanaDemux_c*            HavanaDemux);

    HavanaStatus_t              AddStream                      (char*                           Media,
                                                                char*                           Format,
                                                                char*                           Encoding,
                                                                unsigned int                    SurfaceId,
                                                                class HavanaStream_c**          HavanaStream);
    HavanaStatus_t              RemoveStream                   (class HavanaStream_c*           HavanaStream);

    HavanaStatus_t              SetSpeed                       (int                             PlaySpeed);
    HavanaStatus_t              GetSpeed                       (int*                            PlaySpeed);
    HavanaStatus_t              SetNativePlaybackTime          (unsigned long long              NativeTime,
                                                                unsigned long long              SystemTime);
    HavanaStatus_t              SetOption                      (play_option_t                   Option,
                                                                unsigned int                    Value);
    HavanaStatus_t              CheckEvent                     (struct PlayerEventRecord_s*     PlayerEvent);
    HavanaStatus_t              GetPlayerEnvironment           (PlayerPlayback_t*               PlayerPlayback);

    HavanaStatus_t              SetClockDataPoint              (clock_data_point_t*             DataPoint);
};


#endif

