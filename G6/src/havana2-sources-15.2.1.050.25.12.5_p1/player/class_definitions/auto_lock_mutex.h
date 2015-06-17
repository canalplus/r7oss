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

#ifndef _AUTO_LOCK_MUTEX_H
#define _AUTO_LOCK_MUTEX_H

#include "player.h"

///////////////////////////////////////////////////////////////////////////
///
/// Mutex wrapper to ensure the mutex is automatically released.
///
/// This uses an odd (but suprisingly common) C++ trick to ensure
/// the mutex is not left locked by accident. Basically the constructor
/// takes the mutex and the destructor, which is called automatically when
/// the variable goes out of scope (e.g. by a method return), releases
/// it.
///
/// Usage: AutoLockMutex AutoLock( &NameOfMutex );
class AutoLockMutex
{
public:
    inline AutoLockMutex(OS_Mutex_t *M)
        : Mutex(M)
    {
        OS_LockMutex(M);
    }
    inline ~AutoLockMutex()
    {
        OS_UnLockMutex(Mutex);
    }
private:
    OS_Mutex_t *Mutex;

    DISALLOW_COPY_AND_ASSIGN(AutoLockMutex);
};

#endif
