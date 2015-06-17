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

#include "stack_generic.h"

#undef TRACE_TAG
#define TRACE_TAG "StackGeneric_c"

// ------------------------------------------------------------------------
// Constructor function

StackGeneric_c::StackGeneric_c(unsigned int MaxEntries)
    : Stack_c()
    , Lock()
    , Limit(MaxEntries)
    , Level(0)
    , Storage(NULL)
{
    OS_InitializeMutex(&Lock);

    // TODO(pht) move to a FinalizeInit method
    Storage = new unsigned int[Limit];
    if (Storage == NULL)
    {
        InitializationStatus = StackNoMemory;
    }
}

// ------------------------------------------------------------------------
// Destructor function

StackGeneric_c::~StackGeneric_c(void)
{
    if (NonEmpty()) { SE_DEBUG(group_buffer, "caution: %p non empty on dtor\n", this);  }
    OS_TerminateMutex(&Lock);
    delete[] Storage;
}

// ------------------------------------------------------------------------
// Insert function

StackStatus_t   StackGeneric_c::Push(unsigned int       Value)
{
    OS_LockMutex(&Lock);

    if (Level == Limit)
    {
        OS_UnLockMutex(&Lock);
        return StackTooManyEntries;
    }

    Storage[Level++]    = Value;
    OS_UnLockMutex(&Lock);
    return StackNoError;
}

// ------------------------------------------------------------------------
// Extract function

StackStatus_t   StackGeneric_c::Pop(unsigned int    *Value)
{
    //
    // If there is nothing in the Stack we return
    //
    if (Level != 0)
    {
        OS_LockMutex(&Lock);
        *Value = Storage[--Level];
        OS_UnLockMutex(&Lock);
        return StackNoError;
    }

    return StackNothingToGet;
}


// ------------------------------------------------------------------------
// Flush function

StackStatus_t   StackGeneric_c::Flush(void)
{
    Level   = 0;
    OS_Smp_Mb();
    return StackNoError;
}


// ------------------------------------------------------------------------
// Non-empty function

bool   StackGeneric_c::NonEmpty(void)
{
    OS_Smp_Mb();
    return (Level != 0);
}

