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

#ifndef H_AUDIO_MIXER_PARAMS
#define H_AUDIO_MIXER_PARAMS

/* create/remove_sysfs_mixer_selection:
 *
 * This interface can be used by player2 module
 * in order to create sysfs files that allows user to choose
 * the name of the MME Transformer that will be used for a given mixer.
 * sysfs files :
 * /sys/devices/platform/stm-se.0/mixer0TransformerId
 * /sys/devices/platform/stm-se.0/mixer1TransformerId
 * /sys/devices/platform/stm-se.0/mixer2TransformerId
 * /sys/devices/platform/stm-se.0/mixer3TransformerId
 */
void create_sysfs_mixer_selection(struct device *dev);

void remove_sysfs_mixer_selection(struct device *dev);

#endif /* H_AUDIO_MIXER_PARAMS */
