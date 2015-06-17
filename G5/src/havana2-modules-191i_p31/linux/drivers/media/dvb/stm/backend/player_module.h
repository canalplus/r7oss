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

Source file name : player_module.h - backend streamer device definitions
Author :           Julian

Date        Modification                                    Name
----        ------------                                    --------
24-Mar-03   Created                                         Julian

************************************************************************/

#ifndef H_PLAYER_MODULE
#define H_PLAYER_MODULE

#include "report.h"

#define MODULE_NAME             "ST Havana Player1"

#ifndef false
#define false   0
#define true    1
#endif

#define TRANSPORT_PACKET_SIZE           188
#define DEMUX_BUFFER_SIZE               (200*TRANSPORT_PACKET_SIZE)

/*      Debug printing macros   */
#ifndef ENABLE_PLAYER_DEBUG
#define ENABLE_PLAYER_DEBUG             0
#endif

#define PLAYER_DEBUG(fmt, args...)      ((void) (ENABLE_PLAYER_DEBUG && \
                                            (report(severity_note, "Player:%s: " fmt, __FUNCTION__, ##args), 0)))

/* Output trace information off the critical path */
#define PLAYER_TRACE(fmt, args...)      (report(severity_note, "Player:%s: " fmt, __FUNCTION__, ##args))
/* Output errors, should never be output in 'normal' operation */
#define PLAYER_ERROR(fmt, args...)      (report(severity_error, "Player:%s: " fmt, __FUNCTION__, ##args))

#endif
