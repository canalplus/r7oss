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
#ifndef __KTM_CONTROLDATA_EVENT_H
#define __KTM_CONTROLDATA_EVENT_H

#define MESSAGE_DEPTH 16
#define MAX_RELAYFS_STRING_LENGTH      560   /* maximum number of characters per line in the output file */

uint8_t SubscribeEvent(void);
void UnsubscribeEvent(void);
void HandleEventH264(unsigned int number_of_events, stm_event_info_t *eventsInfo);
void HandleEventMpeg2(unsigned int number_of_events, stm_event_info_t *eventsInfo);

#endif
