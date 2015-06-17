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
#ifndef __KTM_CONTROLDATA_H
#define __KTM_CONTROLDATA_H

#include <stm_event.h>
#include <stm_common.h>

#include "player_version.h"

#include "report.h"
#include "stm_se.h"

struct ControlDataTestCtx
{
    stm_se_playback_h PlaybackHandle;
    stm_se_play_stream_h StreamH264Handle;
    stm_se_play_stream_h StreamMpeg2Handle;
    void *Stream;
    int StreamSize;
    uint32_t InjectionSize;
    int PlaybackSpeed;
    stm_se_play_stream_subscription_h MessageSubscriptionH264;
    stm_se_play_stream_subscription_h MessageSubscriptionMpeg2;
    stm_event_subscription_h    EventSubscriptionH264;
    stm_event_subscription_h    EventSubscriptionMpeg2;
    int FSIndex;
    stm_se_stream_encoding_t StreamEncoding;
};

#endif
