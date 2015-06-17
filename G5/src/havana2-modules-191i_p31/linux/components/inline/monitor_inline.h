/************************************************************************
Copyright (C) 2008 STMicroelectronics. All Rights Reserved.

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

Source file name : monitorinline.h (kernel version)
Author :           Julian

Provides access to the monitor functions and types

Date        Modification                                    Name
----        ------------                                    --------
30-Jul-08   Created                                         Julian

************************************************************************/

#ifndef H_MONITOR_INLINE
#define H_MONITOR_INLINE


#include "monitor_types.h"

#if defined (CONFIG_MONITOR)

#ifdef __cplusplus
extern "C" {
#endif
void                    MonitorSignalEvent     (monitor_event_code_t            EventCode,
                                                unsigned int                    Parameters[MONITOR_PARAMETER_COUNT],
                                                const char*                     Description);
#ifdef __cplusplus
}
#endif

#else

static inline void      MonitorSignalEvent     (monitor_event_code_t            EventCode,
                                                unsigned int                    Parameters[MONITOR_PARAMETER_COUNT],
                                                const char*                     Description) {}

#endif

#endif
