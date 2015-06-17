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

#ifndef H_ES_PROCESSOR
#define H_ES_PROCESSOR

#include "player_types.h"
#include "port.h"
#include "buffer.h"

class ES_Processor_c : public Port_c
{
public:
    virtual PlayerStatus_t FinalizeInit(PlayerStream_t Stream) = 0;

    // Connect to downtream component
    virtual PlayerStatus_t Connect(Port_c *Port) = 0;

    // Trigger configuration handling (set start and end, cancel a programmed triger)
    virtual PlayerStatus_t SetDiscardTrigger(stm_se_play_stream_discard_trigger_t const &trigger) = 0;

    // reset any pending Triggers
    virtual PlayerStatus_t Reset() = 0;

    // Configure PTS based alarm
    virtual PlayerStatus_t SetAlarm(bool enable, stm_se_play_stream_pts_and_tolerance_t const &config) = 0;
};

#endif // H_ES_PROCESSOR
