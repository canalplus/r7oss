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

#ifndef H_THREADS
#define H_THREADS

#include "osinline.h"

#ifdef __cplusplus
extern "C" {
#endif

// task ids
// keep order in sync with player_tasks_desc and thpolprio_inits
enum tasks_desc_e {
	SE_TASK_AUDIO_CTOP,
	SE_TASK_AUDIO_PTOD,
	SE_TASK_AUDIO_DTOM,
	SE_TASK_AUDIO_MPost,

	SE_TASK_VIDEO_CTOP,
	SE_TASK_VIDEO_PTOD,
	SE_TASK_VIDEO_DTOM,
	SE_TASK_VIDEO_MPost,

	SE_TASK_VIDEO_H264INT,
	SE_TASK_VIDEO_HEVCINT,

	SE_TASK_AUDIO_READER,
	SE_TASK_AUDIO_STREAMT,
	SE_TASK_AUDIO_MIXER,
	SE_TASK_AUDIO_BCAST_MIXER,
	SE_TASK_AUDIO_BYPASS_MIXER,

	SE_TASK_MANIF_COORD,
	SE_TASK_MANIF_BRSRCGRAB,
	SE_TASK_MANIF_BRCAPTURE,
	SE_TASK_MANIF_DSVIDEO,

	SE_TASK_PLAYBACK_DRAIN,

	SE_TASK_ENCOD_AUDITOP,
	SE_TASK_ENCOD_AUDPTOC,
	SE_TASK_ENCOD_AUDCTOO,

	SE_TASK_ENCOD_VIDITOP,
	SE_TASK_ENCOD_VIDPTOC,
	SE_TASK_ENCOD_VIDCTOO,

	SE_TASK_ENCOD_COORD,

	SE_TASK_OTHER, // keep last!
};

extern OS_TaskDesc_t player_tasks_desc[];

#ifdef __cplusplus
}
#endif

#endif  // H_THREADS
