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
#ifndef __AUDIO_BUFFER_LOLITA_H__
#define __AUDIO_BUFFER_LOLITA_H__

// This file describes the following content:
//  lolita 18432 samples (lcm for ddce51, aace, mp3) 48kHz 2ch stereo s32le

#define LOLITA_BUFFER_SIZE        147456
#define LOLITA_SAMPLING_FREQUENCY 48000
#define LOLITA_PCM_FORMAT         STM_SE_AUDIO_PCM_FMT_S32LE
#define LOLITA_BYTES_PER_SAMPLE   4
#define LOLITA_EMPHASIS           STM_SE_NO_EMPHASIS
#define LOLITA_PROG_LEVEL         0
#define LOLITA_NB_CHAN            2
#define LOLITA_CHAN_0             STM_SE_AUDIO_CHAN_L
#define LOLITA_CHAN_1             STM_SE_AUDIO_CHAN_R
#define LOLITA_CHAN_2             STM_SE_AUDIO_CHAN_LFE
#define LOLITA_CHAN_3             STM_SE_AUDIO_CHAN_C
#define LOLITA_CHAN_4             STM_SE_AUDIO_CHAN_LS
#define LOLITA_CHAN_5             STM_SE_AUDIO_CHAN_RS
#define LOLITA_CHAN_6             STM_SE_AUDIO_CHAN_LREARS
#define LOLITA_CHAN_7             STM_SE_AUDIO_CHAN_RREARS

extern unsigned char lolita_audio_buffer[LOLITA_BUFFER_SIZE];
#endif // __AUDIO_BUFFER_LOLITA_H__
