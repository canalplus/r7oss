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
#ifndef DVB_DVP_H_
#define DVB_DVP_H_

#include "dvb_avr_export.h"

typedef struct avr_v4l_shared_handle_s
{
	struct DeviceContext_s 	avr_device_context;

	unsigned long long		target_latency;
	long long 				audio_video_latency_offset;
	bool 					update_player2_time_mapping;
	unsigned int			dvp_irq;
	int*					mapped_dvp_registers;

	void                   *audio_context; //<! Cached for use by avr_set_vsync_offset()

	struct rw_semaphore mixer_settings_semaphore;
	struct snd_pseudo_mixer_settings mixer_settings; //<! Only valid if the magic number is correct
	const struct snd_pseudo_mixer_downmix_rom *downmix_firmware;
} avr_v4l2_shared_handle_t;

int avr_set_external_time_mapping (avr_v4l2_shared_handle_t *shared_context, struct StreamContext_s* stream,
                                     unsigned long long nativetime, unsigned long long systemtime);
void avr_invalidate_external_time_mapping (avr_v4l2_shared_handle_t *shared_context);

void avr_set_vsync_offset(avr_v4l2_shared_handle_t *shared_context, long long vsync_offset );
#endif /*DVB_DVP_H_*/

