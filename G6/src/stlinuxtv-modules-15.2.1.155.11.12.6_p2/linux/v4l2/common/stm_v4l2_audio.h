/************************************************************************
Copyright (C) 2013 STMicroelectronics. All Rights Reserved.

This file is part of the STLInuxTV Library.

STLInuxTV is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License version 2 as published by the
Free Software Foundation.

STLInuxTV is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with player2; see the file COPYING.  If not, write to the Free Software
Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

The STLInuxTV Library may alternatively be licensed under a proprietary
license from ST.
 *  contains audio specific for st v4l2 driver
************************************************************************/

#ifndef STM_V4L2_AUDIO_H
#define STM_V4L2_AUDIO_H

static const int stm_v4l2_audio_pcm_format_table[][2] = {
	{ V4L2_MPEG_AUDIO_STM_PCM_FMT_32_S32LE,  STM_SE_AUDIO_PCM_FMT_S32LE  },
	{ V4L2_MPEG_AUDIO_STM_PCM_FMT_32_S32BE,  STM_SE_AUDIO_PCM_FMT_S32BE  },
	{ V4L2_MPEG_AUDIO_STM_PCM_FMT_24_S24LE,  STM_SE_AUDIO_PCM_FMT_S24LE  },
	{ V4L2_MPEG_AUDIO_STM_PCM_FMT_24_S24BE,  STM_SE_AUDIO_PCM_FMT_S24BE  },
	{ V4L2_MPEG_AUDIO_STM_PCM_FMT_16_S16LE,  STM_SE_AUDIO_PCM_FMT_S16LE  },
	{ V4L2_MPEG_AUDIO_STM_PCM_FMT_16_S16BE,  STM_SE_AUDIO_PCM_FMT_S16BE  },
	{ V4L2_MPEG_AUDIO_STM_PCM_FMT_16_U16BE,  STM_SE_AUDIO_PCM_FMT_U16BE  },
	{ V4L2_MPEG_AUDIO_STM_PCM_FMT_16_U16LE,  STM_SE_AUDIO_PCM_FMT_U16LE  },
	{ V4L2_MPEG_AUDIO_STM_PCM_FMT_8_U8,      STM_SE_AUDIO_PCM_FMT_U8     },
	{ V4L2_MPEG_AUDIO_STM_PCM_FMT_8_S8,      STM_SE_AUDIO_PCM_FMT_S8     },
	{ V4L2_MPEG_AUDIO_STM_PCM_FMT_8_ALAW_8,  STM_SE_AUDIO_PCM_FMT_ALAW_8 },
	{ V4L2_MPEG_AUDIO_STM_PCM_FMT_8_ULAW_8,  STM_SE_AUDIO_PCM_FMT_ULAW_8 }
};

#endif
