/*
 * ALSA library for kernel clients (ksound)
 * 
 * Copyright (c) 2007 STMicroelectronics R&D Limited <daniel.thompson@st.com>
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef _SOUND_KSOUND_STREAMING_H_
#define _SOUND_KSOUND_STREAMING_H_

#include <ksound.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * capture -> playback stream redirection
 * both handles shall be previously open and set up (possibly with the same parameters)
 */
int ksnd_pcm_streaming_start(ksnd_pcm_streaming_t *handle, ksnd_pcm_t *capture, ksnd_pcm_t *playback);
void ksnd_pcm_streaming_stop(ksnd_pcm_streaming_t handle);

#ifdef __cplusplus
}; /* extern "C" */
#endif

#endif /* _SOUND_KSOUND_H_ */
