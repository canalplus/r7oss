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

Source file name : havana_demux.h derived from havana_player.h
Author :           Julian

Definition of the implementation of demux module for havana.


Date        Modification                                    Name
----        ------------                                    --------
17-Apr-07   Created                                         Julian

************************************************************************/

#ifndef H_HAVANA_DEMUX
#define H_HAVANA_DEMUX

#include "osdev_user.h"
#include "player.h"
#include "player_generic.h"
#include "havana_playback.h"

/*      Debug printing macros   */
#ifndef ENABLE_DEMUX_DEBUG
#define ENABLE_DEMUX_DEBUG             1
#endif

#define DEMUX_DEBUG(fmt, args...)      ((void) (ENABLE_DEMUX_DEBUG && \
                                            (report(severity_note, "HavanaDemux_c::%s: " fmt, __FUNCTION__, ##args), 0)))

/* Output trace information off the critical path */
#define DEMUX_TRACE(fmt, args...)      (report(severity_note, "HavanaDemux_c::%s: " fmt, __FUNCTION__, ##args))
/* Output errors, should never be output in 'normal' operation */
#define DEMUX_ERROR(fmt, args...)      (report(severity_error, "HavanaDemux_c::%s: " fmt, __FUNCTION__, ##args))


/// Player wrapper component to manage demultiplexing.
class HavanaDemux_c
{
private:
    OS_Mutex_t                  InputLock;

    class Player_c*             Player;
    PlayerPlayback_t            PlayerPlayback;

    DemultiplexorContext_t      DemuxContext;

public:

                                HavanaDemux_c                  (void);
                               ~HavanaDemux_c                  (void);

    HavanaStatus_t              Init                           (class Player_c*                 Player,
                                                                PlayerPlayback_t                PlayerPlayback,
                                                                DemultiplexorContext_t          DemultiplexorContext);
    HavanaStatus_t              InjectData                     (const unsigned char*            Data,
                                                                unsigned int                    DataLength);
};

#endif

