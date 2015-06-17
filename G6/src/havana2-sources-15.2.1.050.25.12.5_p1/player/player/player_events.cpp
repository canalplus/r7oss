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

#include "havana_stream.h"
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
//      Specify which ongoing events will be signalled
//

PlayerStatus_t   Player_Generic_c::SpecifySignalledEvents(
    PlayerPlayback_t          Playback,
    PlayerStream_t            Stream,
    PlayerEventMask_t         Events,
    void                     *UserData)
{
    SE_VERBOSE(group_player, "Playback 0x%p Stream 0x%p Events %llu", Playback, Stream, Events);

    //
    // Check the parameters are legal
    //

    if ((Playback == PlayerAllPlaybacks) || (Stream == PlayerAllStreams))
    {
        SE_ERROR("Signalled events must be specified on a single stream basis\n");
        return PlayerError;
    }

    //
    // Pass on down to the subclasses
    //
    Stream->GetCollator()->SpecifySignalledEvents(Events, UserData);
    Stream->GetFrameParser()->SpecifySignalledEvents(Events, UserData);
    Stream->GetCodec()->SpecifySignalledEvents(Events, UserData);
    Stream->GetOutputTimer()->SpecifySignalledEvents(Events, UserData);
    Stream->GetManifestationCoordinator()->SpecifySignalledEvents(Events, UserData);

    return PlayerNoError;
}

