/************************************************************************
Copyright (C) 2007 STMicroelectronics. All Rights Reserved.

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
 * Header file for the V4L2 dvp capture device driver for ST SoC
************************************************************************/
#ifndef DVB_CAP_H_
#define DVB_CAP_H_

#include "dvb_cap_export.h"

typedef struct cap_v4l_shared_handle_s
{
	struct DeviceContext_s 	cap_device_context;

	unsigned long long		target_latency;
	long long 				audio_video_latency_offset;
	bool 					update_player2_time_mapping;
	unsigned int			cap_irq;
	unsigned int			cap_irq2;
	int*					mapped_cap_registers;
	int*					mapped_vtg_registers;
} cap_v4l2_shared_handle_t;

int cap_set_external_time_mapping (cap_v4l2_shared_handle_t *shared_context, struct StreamContext_s* stream,
                                     unsigned long long nativetime, unsigned long long systemtime);
void cap_invalidate_external_time_mapping (cap_v4l2_shared_handle_t *shared_context);

void cap_set_vsync_offset(cap_v4l2_shared_handle_t *shared_context, long long vsync_offset );
#endif /*DVB_CAP_H_*/

