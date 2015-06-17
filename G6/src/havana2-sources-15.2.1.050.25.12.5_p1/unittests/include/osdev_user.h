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

#ifndef H_OSDEV_USER
#define H_OSDEV_USER

/* --- Include the os specific headers we will need --- */

#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <sys/mman.h>
#include <sys/stat.h>


#define OSDEV_INVALID_DEVICE            -1

typedef enum {
    OSDEV_NoError       = 0,
    OSDEV_Error
} OSDEV_Status_t;

typedef int                     OSDEV_DeviceIdentifier_t;

static inline OSDEV_Status_t   OSDEV_Open(char                            *Name,
                                          OSDEV_DeviceIdentifier_t        *Handle) {
    int      Device;

    *Handle = OSDEV_INVALID_DEVICE;

    Device  = open(Name, O_RDWR);
    if (Device < 0) {
        return OSDEV_Error;
    }

    *Handle = (OSDEV_DeviceIdentifier_t)Device;

    return OSDEV_NoError;
}

static inline OSDEV_Status_t   OSDEV_Close(OSDEV_DeviceIdentifier_t     Handle) {
    close(Handle);
    return OSDEV_NoError;
}

static inline OSDEV_Status_t   OSDEV_Ioctl(OSDEV_DeviceIdentifier_t         Handle,
                                           unsigned int                     Command,
                                           void                            *Argument,
                                           unsigned int                     ArgumentSize) {
    int Status;

    Status = ioctl(Handle, (int)Command, Argument);
    if (Status < 0) {
        return OSDEV_Error;
    }

    return OSDEV_NoError;
}

#endif
