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

#ifndef H_RING_GENERIC
#define H_RING_GENERIC

#include "ring.h"
#include "osinline.h"

class RingGeneric_c : public Ring_c
{
public:
    RingGeneric_c(unsigned int MaxEntries = 16, const char *Name = NULL);
    ~RingGeneric_c(void);

    RingStatus_t Insert(uintptr_t     Value);
    RingStatus_t InsertFromIRQ(uintptr_t Value);
    RingStatus_t Extract(uintptr_t    *Value, unsigned int   BlockingPeriod = OS_INFINITE);
    RingStatus_t ExtractLastInserted(uintptr_t    *Value, unsigned int   BlockingPeriod = OS_INFINITE);
    RingStatus_t Peek(uintptr_t    *Value);
    RingStatus_t Flush(void);
    bool         NonEmpty(void);
    unsigned int NbOfEntries(void);

private:
    OS_SpinLock_t Lock;
    OS_Event_t    Signal;
    unsigned int  Limit;
    unsigned int  NextExtract;
    unsigned int  NextInsert;
    uintptr_t    *Storage;
    const char   *mName;

    RingStatus_t Insert_l(uintptr_t Value);

    DISALLOW_COPY_AND_ASSIGN(RingGeneric_c);
};

#endif
