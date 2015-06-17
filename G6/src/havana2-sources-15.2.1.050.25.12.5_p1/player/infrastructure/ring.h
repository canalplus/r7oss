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

#ifndef H_RING
#define H_RING

#include "port.h"

#define RING_NONE_BLOCKING  0

class Ring_c : public Port_c
{
public:
    RingStatus_t InitializationStatus;

    Ring_c(void) : Port_c(), InitializationStatus(RingNoError) {};
    virtual ~Ring_c(void) {};

    virtual RingStatus_t Extract(uintptr_t    *Value,
                                 unsigned int  BlockingPeriod = RING_NONE_BLOCKING) = 0;
    virtual RingStatus_t Flush(void) = 0;
};

typedef class Ring_c *Ring_t;

#endif
