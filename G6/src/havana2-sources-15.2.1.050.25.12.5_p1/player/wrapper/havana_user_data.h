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

#ifndef HAVANA_USER_DATA_H
#define HAVANA_USER_DATA_H

#include "osinline.h"
#include <stm_memsrc.h>
#include <stm_memsink.h>
#include <stm_registry.h>

#include "allocinline.h"
#include "player.h"
#include "player_generic.h"
#include "player_playback.h"
#include "player_stream.h"
#include "havana_player.h"

#undef TRACE_TAG
#define TRACE_TAG   "UserDataSource_c"

#define MAX_UD_BUFFERS  ((10*400)+1)

class UserDataSource_c
{
public:
    /* Constructor / Destructor */
    UserDataSource_c(void);
    ~UserDataSource_c(void);

// Public User Data  methods
    HavanaStatus_t  Init(PlayerStream_t     PlayerStream);
    HavanaStatus_t  Connect(stm_object_h       SinkHandle);
    HavanaStatus_t  Disconnect(stm_object_h       SinkHandle);
    HavanaStatus_t  GetUserDataFromDecodeBuffer(Buffer_t Buffer);

// Method for MemorySink Pull access
    uint32_t        UDBlockRead(uint8_t *userBufferAddr, uint32_t userBufferLen);
    int32_t         UDBlockAvailable();
    OS_Status_t     UDBlockWaitFor();

private:
    PlayerStream_t     UserDataPlayStream;

// Connection info and circular buffer access protection
    bool               Initialized;
    bool               Connected;
    OS_Mutex_t         UDBlockBufferLock;
    OS_Semaphore_t     SignalNewUD;

// Data for circular buffer management
    uint8_t            UDBlockBuffers[MAX_UD_BUFFERS];
    uint32_t           UDBlockBufferSize;
    uint32_t           UDBlockWritePos;
    uint32_t           UDBlockReadPos;
    uint32_t           UDBlockFreeLen;
    bool               NewUDComplete;

// Current connection data
    struct stm_data_interface_pull_sink PullSinkInterface;
    stm_object_h                        SinkHandle;

// internal circular buffer procedures
    HavanaStatus_t    InsertNewUserDataIntoUserDataQueue(Buffer_t Buffer);
    uint32_t          GetCurrentBlockLen();

    DISALLOW_COPY_AND_ASSIGN(UserDataSource_c);
};

#endif
