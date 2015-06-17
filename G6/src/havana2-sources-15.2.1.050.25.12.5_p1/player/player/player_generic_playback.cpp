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

#include "player_threads.h"

#include "ring_generic.h"
#include "player_generic.h"
#include "player_playback.h"
#include "player_stream.h"
#include "collate_to_parse_edge.h"
#include "parse_to_decode_edge.h"
#include "decode_to_manifest_edge.h"
#include "post_manifest_edge.h"
#include "es_processor_base.h"

#undef TRACE_TAG
#define TRACE_TAG "Player_Generic_c"

// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Create a new playback
//

PlayerStatus_t   Player_Generic_c::CreatePlayback(
    OutputCoordinator_t       OutputCoordinator,
    PlayerPlayback_t         *Playback,
    bool                      SignalEvent,
    void                     *EventUserData)
{
    PlayerPlayback_t          NewPlayback;
    OS_Thread_t               Thread;
    PlayerEventRecord_t       Event;

    *Playback = NULL;

    NewPlayback = new struct PlayerPlayback_s;
    if (NewPlayback == NULL)
    {
        SE_ERROR("Unable to allocate new playback structure\n");
        return PlayerInsufficientMemory;
    }

    NewPlayback->OutputCoordinator  = OutputCoordinator;
    NewPlayback->AudioCodedFrameCount               = AudioCodedFrameCount;
    NewPlayback->AudioCodedMemorySize               = AudioCodedMemorySize;
    NewPlayback->AudioCodedFrameMaximumSize         = AudioCodedFrameMaximumSize;
    memcpy(NewPlayback->AudioCodedMemoryPartitionName, AudioCodedMemoryPartitionName, ALLOCATOR_MAX_PARTITION_NAME_SIZE);
    NewPlayback->VideoCodedFrameCount               = VideoCodedFrameCount;
    NewPlayback->VideoCodedMemorySize               = VideoCodedMemorySize;
    NewPlayback->VideoCodedFrameMaximumSize         = VideoCodedFrameMaximumSize;
    memcpy(NewPlayback->VideoCodedMemoryPartitionName, VideoCodedMemoryPartitionName, ALLOCATOR_MAX_PARTITION_NAME_SIZE);
    NewPlayback->OtherCodedFrameCount               = OtherCodedFrameCount;
    NewPlayback->OtherCodedMemorySize               = OtherCodedMemorySize;
    NewPlayback->OtherCodedFrameMaximumSize         = OtherCodedFrameMaximumSize;
    memcpy(NewPlayback->OtherCodedMemoryPartitionName, OtherCodedMemoryPartitionName, ALLOCATOR_MAX_PARTITION_NAME_SIZE);
    NewPlayback->Player = this;
    ResetStatistics(NewPlayback);
    SetLastNativeTime(NewPlayback, INVALID_TIME);

    OS_LockMutex(&Lock);
    NewPlayback->Next   = ListOfPlaybacks;
    ListOfPlaybacks     = NewPlayback;
    OS_UnLockMutex(&Lock);

    NewPlayback->OutputCoordinator->RegisterPlayer(this, NewPlayback, PlayerAllStreams);

    NewPlayback->DrainSignalThreadRunning = true;

    if (OS_CreateThread(&Thread, PlayerProcessDrain, NewPlayback, &player_tasks_desc[SE_TASK_PLAYBACK_DRAIN]) != OS_NO_ERROR)
    {
        SE_ERROR("Unable to create drain playback thread\n");
        NewPlayback->DrainSignalThreadRunning = false;
        return PlayerError;
    }

    // Wait for drain process to run
    OS_Status_t WaitStatus;
    do
    {
        WaitStatus = OS_WaitForEventAuto(&NewPlayback->DrainStartStopEvent, PLAYER_MAX_EVENT_WAIT);
        if (WaitStatus == OS_TIMED_OUT)
        {
            SE_WARNING("Playback 0x%p Still waiting for drain thread to start\n", NewPlayback);
        }
    }
    while (WaitStatus == OS_TIMED_OUT);

    OS_ResetEvent(&NewPlayback->DrainStartStopEvent);

    SE_INFO(group_player, "Playback 0x%p drain thread created\n", NewPlayback);

    *Playback = NewPlayback;

    return PlayerNoError;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Delete a playback
//

PlayerStatus_t   Player_Generic_c::TerminatePlayback(
    PlayerPlayback_t          Playback,
    bool                      SignalEvent,
    void                     *EventUserData)
{
    PlayerEventRecord_t       Event;
    PlayerPlayback_t         *PointerToPlayback;

    for (PointerToPlayback       = &ListOfPlaybacks;
         (*PointerToPlayback)   != NULL;
         PointerToPlayback       = &((*PointerToPlayback)->Next))
    {
        if ((*PointerToPlayback) == Playback)
        {
            (*PointerToPlayback)  = Playback->Next;
            break;
        }
    }

    delete Playback;

    return PlayerNoError;
}

// //////////////////////////////////////////////////////////////////////////////////////////////////
//
// It deletes the oldest archive block that is to say the first one.
// It returns false is there is no block to delete and true otherwise.
//
bool Player_Generic_c::DeleteTheOldestArchiveBlock(bool EnableLock)
{
    SE_VERBOSE(group_player, "> this 0x%p\n", this);
    bool returnValue = true;

    if (EnableLock)
    {
        OS_LockMutex(&Lock);
    }

    if (ArchivedReservedMemoryBlocks[0].BlockSize == 0)
    {
        returnValue = false;
    }
    else
    {
        SE_VERBOSE(group_player, "@ this 0x%p Deleting now: BufferPool 0x%p, AllocatorMemoryDevice 0x%p\n", this, (void *)(ArchivedReservedMemoryBlocks[0].BufferPool),
                   (void *)(ArchivedReservedMemoryBlocks[0].AllocatorMemoryDevice));
        BufferManager->DestroyPool(ArchivedReservedMemoryBlocks[0].BufferPool);

        AllocatorClose(&ArchivedReservedMemoryBlocks[0].AllocatorMemoryDevice);
    }

#if (MAXIMUM_NUMBER_OF_RESERVED_MANIFESTATION_MEMORY_BLOCKS > 1)

    for (int i = 1; i < MAXIMUM_NUMBER_OF_RESERVED_MANIFESTATION_MEMORY_BLOCKS; i++)
    {
        ArchivedReservedMemoryBlocks[i - 1].BlockBase = ArchivedReservedMemoryBlocks[i].BlockBase;
        ArchivedReservedMemoryBlocks[i - 1].BlockSize = ArchivedReservedMemoryBlocks[i].BlockSize;
        ArchivedReservedMemoryBlocks[i - 1].BufferPool = ArchivedReservedMemoryBlocks[i].BufferPool;
        ArchivedReservedMemoryBlocks[i - 1].AllocatorMemoryDevice = ArchivedReservedMemoryBlocks[i].AllocatorMemoryDevice;
    }

#endif
    ArchivedReservedMemoryBlocks[MAXIMUM_NUMBER_OF_RESERVED_MANIFESTATION_MEMORY_BLOCKS - 1].BlockBase = NULL;
    ArchivedReservedMemoryBlocks[MAXIMUM_NUMBER_OF_RESERVED_MANIFESTATION_MEMORY_BLOCKS - 1].BlockSize = 0;
    ArchivedReservedMemoryBlocks[MAXIMUM_NUMBER_OF_RESERVED_MANIFESTATION_MEMORY_BLOCKS - 1].BufferPool = NULL;
    ArchivedReservedMemoryBlocks[MAXIMUM_NUMBER_OF_RESERVED_MANIFESTATION_MEMORY_BLOCKS - 1].AllocatorMemoryDevice = NULL;

    if (EnableLock)
    {
        OS_UnLockMutex(&Lock);
    }

    SE_VERBOSE(group_player, "< this 0x%p\n", this);
    return returnValue;
}

// //////////////////////////////////////////////////////////////////////////////////////////////////
//
// It empties the ArchivedReservedMemoryBlocks array destroying all the buffer pool and closing all
// all the allocator device.
//
void Player_Generic_c::ResetArchive(bool EnableLock)
{
    SE_VERBOSE(group_player, "> this 0x%p\n", this);

    if (EnableLock)
    {
        OS_LockMutex(&Lock);
    }

    for (int i = 0; i < MAXIMUM_NUMBER_OF_RESERVED_MANIFESTATION_MEMORY_BLOCKS; i++)
    {
        if (ArchivedReservedMemoryBlocks[i].BlockSize != 0)
        {
            SE_VERBOSE(group_player, "@ this 0x%p ArchivedReservedMemoryBlocks[%d]: BlockBase 0x%p, BlockSize %x, BufferPool 0x%p, AllocatorMemoryDevice 0x%p\n",
                       this, i, (void *)(ArchivedReservedMemoryBlocks[i].BlockBase), ArchivedReservedMemoryBlocks[i].BlockSize, (void *)(ArchivedReservedMemoryBlocks[i].BufferPool),
                       (void *)(ArchivedReservedMemoryBlocks[i].AllocatorMemoryDevice));
            BufferManager->DestroyPool(ArchivedReservedMemoryBlocks[i].BufferPool);

            AllocatorClose(&ArchivedReservedMemoryBlocks[i].AllocatorMemoryDevice);

            ArchivedReservedMemoryBlocks[i].BlockBase = NULL;
            ArchivedReservedMemoryBlocks[i].BlockSize = 0;
            ArchivedReservedMemoryBlocks[i].BufferPool = NULL;
        }
    }

    if (EnableLock)
    {
        OS_UnLockMutex(&Lock);
    }

    SE_VERBOSE(group_player, "< this 0x%p\n", this);
}

// //////////////////////////////////////////////////////////////////////////////////////////////////
//
// It displays the content of the ArchivedReservedMemoryBlocks array.
//
void Player_Generic_c::DisplayArchiveBlocks(bool EnableLock)
{
    SE_VERBOSE(group_player, "> @ this 0x%p\n", this);

    if (EnableLock)
    {
        OS_LockMutex(&Lock);
    }

    for (int i = 0; i < MAXIMUM_NUMBER_OF_RESERVED_MANIFESTATION_MEMORY_BLOCKS; i++)
    {
        SE_VERBOSE(group_player, "@ this 0x%p ArchivedReservedMemoryBlocks[%d]: BlockBase 0x%p, BlockSize %x, BufferPool 0x%p, AllocatorMemoryDevice 0x%p\n",
                   this, i, (void *)(ArchivedReservedMemoryBlocks[i].BlockBase), ArchivedReservedMemoryBlocks[i].BlockSize, (void *)(ArchivedReservedMemoryBlocks[i].BufferPool),
                   (void *)(ArchivedReservedMemoryBlocks[i].AllocatorMemoryDevice));
    }

    if (EnableLock)
    {
        OS_UnLockMutex(&Lock);
    }

    SE_VERBOSE(group_player, "< @ this 0x%p\n", this);
}

//
// Returns true of ArchivedReservedMemoryBlocks is full and false
// otherwise.
//
bool Player_Generic_c::TheArchiveIsfull(bool EnableLock)
{
    bool archiveIsFull = true;

    if (EnableLock)
    {
        OS_LockMutex(&Lock);
    }

    for (int i = 0; i < MAXIMUM_NUMBER_OF_RESERVED_MANIFESTATION_MEMORY_BLOCKS; i++)
    {
        if (ArchivedReservedMemoryBlocks[i].BlockSize == 0)
        {
            archiveIsFull = false;
        }
    }

    if (EnableLock)
    {
        OS_UnLockMutex(&Lock);
    }

    return archiveIsFull;
}

// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Add a stream to an existing playback
//

PlayerStatus_t   Player_Generic_c::AddStream(PlayerPlayback_t           Playback,
                                             PlayerStream_t            *Stream,
                                             PlayerStreamType_t         StreamType,
                                             unsigned int               InstanceId,
                                             Collator_t                 Collator,
                                             FrameParser_t              FrameParser,
                                             Codec_t                    Codec,
                                             OutputTimer_t              OutputTimer,
                                             DecodeBufferManager_t      DecodeBufferManager,
                                             ManifestationCoordinator_t ManifestationCoordinator,
                                             HavanaStream_t             HavanaStream,
                                             UserDataSource_t           UserDataSender,
                                             bool                       SignalEvent,
                                             void                      *EventUserData)
{
    *Stream = NULL;

    if (StreamType == StreamTypeNone)
    {
        SE_ERROR("Stream type must be specified (not StreamTypeNone)\n");
        return PlayerError;
    }
    // TODO(pht) check if StreamTypeOther still a valid item

    PlayerStream_c *NewStream = new PlayerStream_c(this, Playback, HavanaStream, StreamType, InstanceId, DecodeBufferManager, Collator, UserDataSender);
    if (NewStream == NULL)
    {
        SE_ERROR("Unable to allocate new stream structure\n");
        return PlayerInsufficientMemory;
    }

    PlayerStatus_t Status = NewStream->FinalizeInit(Collator, FrameParser, Codec, OutputTimer,
                                                    ManifestationCoordinator, BufferManager,
                                                    SignalEvent, EventUserData);
    if (Status != PlayerNoError)
    {
        SE_ERROR("Failed to initialize stream\n");
        delete NewStream;
        return Status;
    }

    Playback->RegisterStream(NewStream);

    *Stream = NewStream;
#ifdef LOWMEMORYBANDWIDTH
    mNbStreams ++;
#endif

    return PlayerNoError;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Remove a stream from an existing playback
//

PlayerStatus_t   Player_Generic_c::RemoveStream(PlayerStream_t            Stream,
                                                bool                      SignalEvent,
                                                void                     *EventUserData)
{
    PlayerStatus_t          Status;
    PlayerEventRecord_t     Event;
    SE_DEBUG(group_player, "Playback 0x%p Stream 0x%p\n", Stream->GetPlayback(), Stream);

    //
    // First drain the stream
    //
    Status = Stream->Drain(false, false, NULL, PolicyPlayoutOnTerminate, false, NULL);
    if (Status != PlayerNoError)
    {
        SE_ERROR("Failed to drain stream\n");
        delete Stream;
        return Status;
    }

    //
    // Since we blocked in the drain, we should now be able to shutdown the stream cleanly.
    //
    SE_DEBUG(group_player, "Playback 0x%p Stream 0x%p completed\n", Stream->GetPlayback(), Stream);
    delete Stream;
#ifdef LOWMEMORYBANDWIDTH
    mNbStreams --;

    if (mNbStreams == 0)
    {
        OS_SemaphoreInitialize(&mSemMemBWLimiter, 1);
    }
#endif

    return Status;
}

//
//  Low power enter / exit functions at playback level
//  These methods are used to stop current streams processing (e.g. collator) to speed-up low power enter procedure
//

PlayerStatus_t   Player_Generic_c::PlaybackLowPowerEnter(PlayerPlayback_t Playback)
{
    SE_INFO(group_player, "Playback 0x%p\n", Playback);
    // Save low power state at playback level
    Playback->LowPowerEnter();
    return PlayerNoError;
}

PlayerStatus_t   Player_Generic_c::PlaybackLowPowerExit(PlayerPlayback_t Playback)
{
    SE_INFO(group_player, "Playback 0x%p\n", Playback);
    // Reset low power state at playback level
    Playback->LowPowerExit();
    return PlayerNoError;
}


