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

#include "ring_generic.h"

#undef TRACE_TAG
#define TRACE_TAG "RingGeneric_c"

RingGeneric_c::RingGeneric_c(unsigned int MaxEntries, const char *Name)
    : Ring_c()
    , Lock()
    , Signal()
    , Limit(MaxEntries + 1)
    , NextExtract(0)
    , NextInsert(0)
    , Storage(NULL)
    , mName((Name != NULL) ? Name : "Anonymous Ring")
{
    OS_InitializeSpinLock(&Lock);
    OS_InitializeEvent(&Signal);

    // TODO(pht) move to a FinalizeInit method
    Storage = new uintptr_t[Limit];
    if (Storage == NULL)
    {
        InitializationStatus = RingNoMemory;
    }
}

RingGeneric_c::~RingGeneric_c(void)
{
    if (NonEmpty()) { SE_DEBUG(group_buffer, "caution:%s ring (%p) is non empty on dtor\n", mName, this); }
    OS_TerminateSpinLock(&Lock);
    OS_TerminateEvent(&Signal);
    delete[] Storage;
}

// Must be called with spinlock held
RingStatus_t   RingGeneric_c::Insert_l(uintptr_t      Value)
{
    unsigned int OldNextInsert;

    OldNextInsert       = NextInsert;
    Storage[NextInsert] = Value;
    NextInsert++;

    if (NextInsert == Limit)
    {
        NextInsert = 0;
    }

    if (NextInsert == NextExtract)
    {
        NextInsert      = OldNextInsert;
        return RingTooManyEntries;
    }

    return RingNoError;
}

RingStatus_t   RingGeneric_c::Insert(uintptr_t      Value)
{
    OS_LockSpinLockIRQSave(&Lock);

    RingStatus_t Status = Insert_l(Value);

    OS_UnLockSpinLockIRQRestore(&Lock);

    OS_SetEvent(&Signal);

    return Status;
}

RingStatus_t   RingGeneric_c::InsertFromIRQ(uintptr_t      Value)
{
    OS_LockSpinLock(&Lock);

    RingStatus_t Status = Insert_l(Value);

    OS_UnLockSpinLock(&Lock);

    OS_SetEvent(&Signal);

    return Status;
}

RingStatus_t RingGeneric_c::Peek(uintptr_t    *Value)
{
    OS_LockSpinLockIRQSave(&Lock);
    if (NextExtract != NextInsert)
    {
        *Value = Storage[NextExtract];
        OS_UnLockSpinLockIRQRestore(&Lock);
        return RingNoError;
    }

    OS_UnLockSpinLockIRQRestore(&Lock);

    return RingNothingToGet;
}

RingStatus_t   RingGeneric_c::Extract(uintptr_t       *Value,
                                      unsigned int     BlockingPeriod)
{
    //
    // If there is nothing in the ring we wait for up to the specified period.
    //
    OS_ResetEvent(&Signal);
    OS_Smp_Mb();

    if ((NextExtract == NextInsert) && (BlockingPeriod != RING_NONE_BLOCKING))
    {
        OS_WaitForEventAuto(&Signal, BlockingPeriod);
    }

    OS_LockSpinLockIRQSave(&Lock);

    if (NextExtract != NextInsert)
    {
        *Value = Storage[NextExtract];
        NextExtract++;

        if (NextExtract == Limit)
        {
            NextExtract = 0;
        }

        OS_UnLockSpinLockIRQRestore(&Lock);
        return RingNoError;
    }

    OS_UnLockSpinLockIRQRestore(&Lock);
    return RingNothingToGet;
}

RingStatus_t   RingGeneric_c::ExtractLastInserted(uintptr_t       *Value,
                                                  unsigned int     BlockingPeriod)
{
    //
    // If there is nothing in the ring we wait for up to the specified period.
    //
    OS_ResetEvent(&Signal);
    OS_Smp_Mb();

    if ((NextExtract == NextInsert) && (BlockingPeriod != RING_NONE_BLOCKING))
    {
        OS_WaitForEventAuto(&Signal, BlockingPeriod);
    }

    OS_LockSpinLockIRQSave(&Lock);

    if (NextExtract != NextInsert)
    {
        if (NextInsert > 0)
        {
            NextInsert--;
        }
        else
        {
            NextInsert = Limit - 1;
        }

        *Value = Storage[NextInsert];

        OS_UnLockSpinLockIRQRestore(&Lock);
        return RingNoError;
    }

    OS_UnLockSpinLockIRQRestore(&Lock);
    return RingNothingToGet;
}

RingStatus_t   RingGeneric_c::Flush(void)
{
    OS_LockSpinLockIRQSave(&Lock);
    NextExtract = 0;
    NextInsert  = 0;
    OS_UnLockSpinLockIRQRestore(&Lock);
    return RingNoError;
}

bool   RingGeneric_c::NonEmpty(void)
{
    bool result;
    OS_LockSpinLockIRQSave(&Lock);
    result = (NextExtract != NextInsert);
    OS_UnLockSpinLockIRQRestore(&Lock);
    return result;
}

unsigned int RingGeneric_c::NbOfEntries(void)
{
    unsigned int    NbEntries;
    OS_LockSpinLockIRQSave(&Lock);
    NbEntries = (NextInsert + Limit - NextExtract) % Limit;
    OS_UnLockSpinLockIRQRestore(&Lock);
    return NbEntries;
}
