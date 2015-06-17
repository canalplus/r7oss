/************************************************************************
Copyright (C) 2003 STMicroelectronics. All Rights Reserved.

This file is part of the Player2 Library.

Player2 is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License version 2 as published by the
Free Software Foundation.

Player2 is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with player2; see the file COPYING.  If not, write to the Free Software
Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

The Player2 Library may alternatively be licensed under a proprietary
license from ST.

Source file name : display.h - access to platform specific display info
Author :           Julian

Date        Modification                                    Name
----        ------------                                    --------
05-Apr-07   Created                                         Julian

************************************************************************/

#ifndef H_DISPLAY
#define H_DISPLAY

#include "osdev_user.h"

#define DISPLAY_ID_MAIN                 0
#define DISPLAY_ID_PIP                  1
#define DISPLAY_ID_REMOTE               2

typedef enum
{
    BufferLocationSystemMemory  = 0x01,
    BufferLocationVideoMemory   = 0x02,
    BufferLocationEither        = 0x03,
} BufferLocation_t;

#ifdef __cplusplus
extern "C" {
#endif

int             DisplayInit             (void);
int             GetDisplayInfo          (unsigned int           Id,
                                         DeviceHandle_t*        DisplayDevice,
                                         unsigned int*          PlaneId,
                                         unsigned int*          OutputId,
                                         BufferLocation_t*      BufferLocation);

#ifdef __cplusplus
}
#endif

#endif
