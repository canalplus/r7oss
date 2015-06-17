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

#ifndef H_SE_MIXER_TRANSFORMER_TYPES
#define H_SE_MIXER_TRANSFORMER_TYPES

#include <stm_se/audio_mixer.h>

// /////////////////////////////////////////////////////////////////////////
//
// Macro definitions.
//
#define MIXER_MAX_PCM_INPUTS          STM_SE_MIXER_NB_MAX_DECODED_AUDIO_INPUTS     /* PlayStream clients */
#define MIXER_MAX_CODED_INPUTS        2 /* Coded Data for SPDIF Bypass and Coded Data for HDMI Bypass */
#define MIXER_MAX_AUDIOGEN_INPUTS     STM_SE_MIXER_NB_MAX_APPLICATION_AUDIO_INPUTS /* AudioGenerator Inputs */
#define MIXER_MAX_INTERACTIVE_INPUTS  1

#define ACC_MIXER_MAX_NB_INPUT        MIXER_MAX_PCM_INPUTS + MIXER_MAX_CODED_INPUTS + MIXER_MAX_AUDIOGEN_INPUTS + MIXER_MAX_INTERACTIVE_INPUTS
#define MIXER_MAX_CLIENTS             MIXER_MAX_PCM_INPUTS


#include <ACC_Transformers/AudioMixer_ProcessorTypes.h>

#define MIXER_CODED_DATA_INPUT        MIXER_MAX_CLIENTS                                 /* Index to the first CODED input       */
#define MIXER_AUDIOGEN_DATA_INPUT     (MIXER_CODED_DATA_INPUT + MIXER_MAX_CODED_INPUTS) /* Index to the first Audio Generator Input */
#define MIXER_INTERACTIVE_INPUT       (MIXER_AUDIOGEN_DATA_INPUT + MIXER_MAX_AUDIOGEN_INPUTS) /* Index to the Interactive Audio Input */
#define MIXER_MAX_INTERACTIVE_CLIENTS 8

#define MIXER_STAGE_PRE_MIX           (MIXER_MAX_CLIENTS+1)
#define MIXER_STAGE_POST_MIX          (MIXER_STAGE_PRE_MIX+1)
#define MIXER_STAGE_MAX               (MIXER_STAGE_POST_MIX+1)

#define MIXER_AUDIO_MAX_INPUT_BUFFERS  (MIXER_MAX_CLIENTS + MIXER_MAX_CODED_INPUTS + MIXER_MAX_AUDIOGEN_INPUTS + MIXER_MAX_INTERACTIVE_CLIENTS)
#define MIXER_AUDIO_MAX_OUTPUT_BUFFERS STM_SE_MIXER_NB_MAX_OUTPUTS
#define MIXER_AUDIO_MAX_BUFFERS        (MIXER_AUDIO_MAX_INPUT_BUFFERS +\
                                       MIXER_AUDIO_MAX_OUTPUT_BUFFERS)
#define MIXER_AUDIO_PAGES_PER_BUFFER   8
#define MIXER_AUDIO_MAX_PAGES          (MIXER_AUDIO_PAGES_PER_BUFFER * MIXER_AUDIO_MAX_BUFFERS)

/* First Mixer Buffers detail
 * INPUT[2]:
 *      PCM BUFFER[2]
 * OUTPUT[1]:
 *      PCM BUFFER[1]
 */
#define FIRST_MIXER_MAX_OUTPUT_BUFFERS 1
#define FIRST_MIXER_MAX_BUFFERS        (MIXER_MAX_PCM_INPUTS+FIRST_MIXER_MAX_OUTPUT_BUFFERS)
#define FIRST_MIXER_MAX_PAGES          (FIRST_MIXER_MAX_BUFFERS*MIXER_AUDIO_PAGES_PER_BUFFER)

#endif // H_SE_MIXER_TRANSFORMER_TYPES
