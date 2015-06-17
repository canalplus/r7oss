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
#include "decode_to_manifest_edge.h"
#include "post_manifest_edge.h"

#undef TRACE_TAG
#define TRACE_TAG "Player_Generic_c"
// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Useful defines/macros that need not be user visible
//

// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Set the native playback time for a playback.
//

PlayerStatus_t   Player_Generic_c::SetNativePlaybackTime(
    PlayerPlayback_t      Playback,
    unsigned long long    NativeTime,
    unsigned long long    SystemTime)
{
    PlayerStatus_t        Status;
    unsigned long long    NormalizedTime;
    unsigned long long    Now = OS_GetTimeInMicroSeconds();
    SE_DEBUG(group_player, "%016llx %016llx (%016llx)\n", NativeTime, SystemTime, Now);

    if (NotValidTime(NativeTime))
    {
        //
        // Invalid, reset all time mappings
        //
        Playback->OutputCoordinator->ResetTimeMapping(PlaybackContext);
    }
    else
    {
        Status  = Playback->TranslateTimeNativeToNormalized(NativeTime, &NormalizedTime, TimeFormatPts);
        if (Status != PlayerNoError)
        {
            SE_ERROR("Playback 0x%p  Failed to translate native time to normalized\n", Playback);
            return Status;
        }

        Status  = Playback->OutputCoordinator->EstablishTimeMapping(PlaybackContext, NormalizedTime, SystemTime);
        if (Status != PlayerNoError)
        {
            SE_ERROR("Playback 0x%p Failed to establish time mapping\n", Playback);
            return Status;
        }
    }

    return PlayerNoError;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Retrieve the native playback time for a playback.
//

PlayerStatus_t   Player_Generic_c::RetrieveNativePlaybackTime(
    PlayerPlayback_t      Playback,
    unsigned long long   *NativeTime,
    PlayerTimeFormat_t    NativeTimeFormat)
{
    PlayerStatus_t        Status;
    unsigned long long    Now;
    unsigned long long    NormalizedTime;
    //
    // What is the system time
    //
    Now     = OS_GetTimeInMicroSeconds();
    //
    // Translate that to playback time
    //
    Status  = Playback->OutputCoordinator->TranslateSystemTimeToPlayback(PlaybackContext, Now, &NormalizedTime);
    if (Status != PlayerNoError)
    {
        // fails when mapping not established
        SE_DEBUG(group_player, "Playback 0x%p could not translate system time to playback time: mapping not established\n", Playback);
        return Status;
    }

    //
    // Translate to PTS time format if requested
    // in case of error: trace done within TranslateTimeNormalizedToNative
    //
    Status = Playback->TranslateTimeNormalizedToNative(NormalizedTime, NativeTime, NativeTimeFormat);

    return Status;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Translate the native playback time for a playback into system time.
//

PlayerStatus_t   Player_Generic_c::TranslateNativePlaybackTime(
    PlayerPlayback_t      Playback,
    unsigned long long    NativeTime,
    unsigned long long   *SystemTime)
{
    PlayerStatus_t      Status;
    unsigned long long  NormalizedTime;

    if (NotValidTime(NativeTime))
    {
        *SystemTime = INVALID_TIME;
    }
    else
    {
        // in case of error: trace done within TranslateTimeNativeToNormalized
        Status  = Playback->TranslateTimeNativeToNormalized(NativeTime, &NormalizedTime, TimeFormatPts);
        if (Status != PlayerNoError)
        {
            return Status;
        }

        Status  = Playback->OutputCoordinator->TranslatePlaybackTimeToSystem(PlaybackContext, NormalizedTime, SystemTime);
        if (Status != PlayerNoError)
        {

            // fails when mapping not established
            SE_DEBUG(group_player, "Playback 0x%p could not translate playback time to system time: mapping not established\n", Playback);
            return Status;
        }
    }

    return PlayerNoError;
}



// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Clock recovery functions are entirely handled by the output coordinator
//

PlayerStatus_t   Player_Generic_c::ClockRecoveryInitialize(
    PlayerPlayback_t      Playback,
    PlayerTimeFormat_t    SourceTimeFormat)
{
    return Playback->OutputCoordinator->ClockRecoveryInitialize(SourceTimeFormat);
}

PlayerStatus_t   Player_Generic_c::ClockRecoveryDataPoint(
    PlayerPlayback_t      Playback,
    unsigned long long    SourceTime,
    unsigned long long    LocalTime)
{
    return Playback->OutputCoordinator->ClockRecoveryDataPoint(SourceTime, LocalTime);
}

PlayerStatus_t   Player_Generic_c::ClockRecoveryEstimate(
    PlayerPlayback_t      Playback,
    unsigned long long   *SourceTime,
    unsigned long long   *LocalTime)
{
    return Playback->OutputCoordinator->ClockRecoveryEstimate(SourceTime, LocalTime);
}

// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Functions to hold the playback times (cross stream)
//

unsigned long long   Player_Generic_c::GetLastNativeTime(
    PlayerPlayback_t     Playback)
{
    return Playback->LastNativeTime;
}

void   Player_Generic_c::SetLastNativeTime(PlayerPlayback_t   Playback,
                                           unsigned long long    Time)
{
    Playback->LastNativeTime    = Time;
}

