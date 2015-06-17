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

#ifndef H_STACK_GENERIC
#define H_STACK_GENERIC

#include "stack.h"

class StackGeneric_c : public Stack_c
{
public:
    explicit StackGeneric_c(unsigned int MaxEntries = 16);
    ~StackGeneric_c(void);

    StackStatus_t Push(unsigned int  Value);
    StackStatus_t Pop(unsigned int  *Value);
    StackStatus_t Flush(void);
    bool          NonEmpty(void);

private:
    OS_Mutex_t     Lock;
    unsigned int   Limit;
    unsigned int   Level;
    unsigned int  *Storage;

    DISALLOW_COPY_AND_ASSIGN(StackGeneric_c);
};
#endif
