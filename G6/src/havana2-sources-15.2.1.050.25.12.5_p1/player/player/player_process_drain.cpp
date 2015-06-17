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

#include "player_generic.h"
#include "player_playback.h"
#include "player_stream.h"

#undef TRACE_TAG
#define TRACE_TAG "Player_Generic_c"

// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Useful defines/macros that need not be user visible
//

// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      C stub
//

//{{{  Thread entry stub
OS_TaskEntry(PlayerProcessDrain)
{
    PlayerPlayback_t        Playback = (PlayerPlayback_t)Parameter;
    Playback->Player->ProcessDrain(Playback);
    OS_TerminateThread();
    return NULL;
}
//}}}

// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Main process code
//

//{{{  ProcessDrain
void  Player_Generic_c::ProcessDrain(PlayerPlayback_t Playback)
{
    PlayerStatus_t  Status;

    //
    // Signal we have started
    //
    OS_SetEvent(&Playback->DrainStartStopEvent);
    SE_INFO(group_player, "Starting Playback 0x%p\n", Playback);

    while (Playback->DrainSignalThreadRunning)
    {
        OS_Status_t WaitStatus;
        do
        {
            WaitStatus = OS_WaitForEventAuto(&Playback->GetDrainSignalEvent(), 10000);
        }
        while (WaitStatus == OS_TIMED_OUT);

        OS_ResetEvent(&Playback->GetDrainSignalEvent());

        // check we weren't awoken due to shutdown
        if (Playback->DrainSignalThreadRunning)
        {
            //
            // Commence drain on all playing streams
            //
            SE_INFO(group_player, "Playback 0x%p Received event - issuing a drain\n", Playback);
            Status = Playback->InternalDrain(PolicyPlayoutOnInternalDrain, false);
            if (Status != PlayerNoError)
            {
                SE_ERROR("Playback 0x%p Failed to drain streams\n", Playback);
            }
        }
    }

    SE_INFO(group_player, "Terminating Playback 0x%p\n", Playback);

    OS_Smp_Mb(); // Read memory barrier: rmb_for_Player_Generic_Terminating coupled with: wmb_for_Player_Generic_Terminating
    OS_SetEvent(&Playback->DrainStartStopEvent);
}
//}}}
