/************************************************************************
Copyright (C) 2003-2013 STMicroelectronics. All Rights Reserved.

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
#ifndef H_PLAYER_PLAYBACK_INTERFACE
#define H_PLAYER_PLAYBACK_INTERFACE

#include "player_types.h"

class PlayerPlaybackInterface_c
{
public:

    virtual ~PlayerPlaybackInterface_c();

    virtual PlayerPlaybackStatistics_t &GetStatistics() = 0;

    virtual OS_Event_t &GetDrainSignalEvent() = 0;

    virtual bool IsLowPowerState() = 0;
    virtual void LowPowerEnter() = 0;
    virtual void LowPowerExit() = 0;
};

#endif // H_PLAYER_PLAYBACK_INTERFACE
