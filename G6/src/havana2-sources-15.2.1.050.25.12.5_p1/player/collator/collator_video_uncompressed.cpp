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

#include "uncompressed.h"
#include "collator_video_uncompressed.h"

//
Collator_VideoUncompressed_c::Collator_VideoUncompressed_c(void)
    : Collator_Base_c()
{
    Configuration.GenerateStartCodeList      = false;  // Uncompressed data does not have start codes
    Configuration.MaxStartCodes              = 0;
    Configuration.StreamIdentifierMask       = 0x00;
    Configuration.StreamIdentifierCode       = 0x00;
    Configuration.BlockTerminateMask         = 0x00;
    Configuration.BlockTerminateCode         = 0x00;
    Configuration.InsertFrameTerminateCode   = false;
    Configuration.TerminalCode               = 0x00;
    Configuration.ExtendedHeaderLength       = 0;
    Configuration.DeferredTerminateFlag      = false;
}

////////////////////////////////////////////////////////////////////////////
//
// Input, simply take the supplied uncompressed data and pass it on
//

CollatorStatus_t Collator_VideoUncompressed_c::DoInput(PlayerInputDescriptor_t  *Input,
                                                       unsigned int              DataLength,
                                                       void                     *Data,
                                                       bool                      NonBlocking,
                                                       unsigned int             *DataLengthRemaining)
{
    UncompressedBufferDesc_t *UncompressedDataDesc = (UncompressedBufferDesc_t *)Data;

    SE_ASSERT(!NonBlocking);
    AssertComponentState(ComponentRunning);

    //
    // If we are running InputJump(), wait until it finish, then resume data injection
    //
    if (OS_MutexIsLocked(&InputJumpLock))
    {
        SE_INFO(group_collator_video, "InputJumpLock locked: Drain process is running! Awaiting unlock..\n");
    }
    OS_LockMutex(&InputJumpLock);
    InputEntry(Input, DataLength, Data, NonBlocking);

    //SE_ERROR("> Collator_VideoUncompressed_c::Input: DataLength %d Data %p NonBlocking %d\n", DataLength, Data, NonBlocking);

    if (DataLength != sizeof(UncompressedBufferDesc_t))
    {
        SE_ERROR("Data is wrong size (%d != %d)\n", DataLength, sizeof(UncompressedBufferDesc_t));
        return CollatorError;
    }

    //
    // Attach the decode buffer mentioned in the input
    // to the current coded frame buffer, to ensure release
    // at the appropriate time.
    //
    Buffer_t    CapturedBuffer    = (Buffer_t)(UncompressedDataDesc->BufferClass);
    CodedFrameBuffer->AttachBuffer(CapturedBuffer);

    SE_DEBUG(group_collator_video, "%p %p %d %d CodedFrameBuffer %p\n", UncompressedDataDesc->Buffer, UncompressedDataDesc->BufferClass,
             UncompressedDataDesc->Content.Width, UncompressedDataDesc->Content.Height, CodedFrameBuffer);

    //
    // Extract the descriptor timing information
    //
    ActOnInputDescriptor(Input);

    //
    // Copy the buffer descriptor to the CodedFrame and pass it on
    //
    CollatorStatus_t Status  = AccumulateData(DataLength, (unsigned char *)Data);
    if (Status == CollatorNoError)
    {
        Status    = InternalFrameFlush();
    }

    //
    // Unlock to allow InputJump() function to run if need
    //
    OS_UnLockMutex(&InputJumpLock);

    InputExit();
    return Status;
}

