/*
 *  sti_backend.h  -- Platform Backend layer
 *
 *  Copyright (c) 2014 STMicroelectronics
 *  Author: Arnaud Pouliquen <arnaud.pouliquen@st.com>,
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 */

#ifndef STI_BACKEND_H
#define STI_BACKEND_H

struct dma_chan *sti_backend_pcm_request_chan(struct snd_soc_pcm_runtime *rtd);
const struct  snd_pcm_hardware *sti_backend_get_pcm_hardware(
					struct snd_soc_dai *dai);
int sti_backend_pcm_create_dai_ctrl(struct snd_soc_pcm_runtime *rtd);
#endif
