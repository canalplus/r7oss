/************************************************************************
 * Copyright (C) 2013 STMicroelectronics. All Rights Reserved.
 *
 * This file is part of the STLinuxTV Library.
 *
 * STLinuxTV is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License version 2 as published by the
 * Free Software Foundation.
 *
 * STLinuxTV is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with player2; see the file COPYING.  If not, write to the Free Software
 * Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * The STLinuxTV Library may alternatively be licensed under a proprietary
 * license from ST.
 * Implementation of demux functions for use by demux only
 *************************************************************************/
#ifndef __DEMUX_COMMON_H__
#define __DEMUX_COMMON_H__

#include <linux/dvb/dmx.h>

static const unsigned int audio_ids[] = {
	DMX_PES_AUDIO0 , DMX_PES_AUDIO1 , DMX_PES_AUDIO2 , DMX_PES_AUDIO3,
	DMX_PES_AUDIO4 , DMX_PES_AUDIO5 , DMX_PES_AUDIO6 , DMX_PES_AUDIO7,
	DMX_PES_AUDIO8 , DMX_PES_AUDIO9 , DMX_PES_AUDIO10, DMX_PES_AUDIO11,
	DMX_PES_AUDIO12, DMX_PES_AUDIO13, DMX_PES_AUDIO14, DMX_PES_AUDIO15,
	DMX_PES_AUDIO16, DMX_PES_AUDIO17, DMX_PES_AUDIO18, DMX_PES_AUDIO19,
	DMX_PES_AUDIO20, DMX_PES_AUDIO21, DMX_PES_AUDIO22, DMX_PES_AUDIO23,
	DMX_PES_AUDIO24, DMX_PES_AUDIO25, DMX_PES_AUDIO26, DMX_PES_AUDIO27,
	DMX_PES_AUDIO28, DMX_PES_AUDIO29, DMX_PES_AUDIO30, DMX_PES_AUDIO31
};

static const unsigned int video_ids[] = {
	DMX_PES_VIDEO0 , DMX_PES_VIDEO1 , DMX_PES_VIDEO2 , DMX_PES_VIDEO3,
	DMX_PES_VIDEO4 , DMX_PES_VIDEO5 , DMX_PES_VIDEO6 , DMX_PES_VIDEO7,
	DMX_PES_VIDEO8 , DMX_PES_VIDEO9 , DMX_PES_VIDEO10, DMX_PES_VIDEO11,
	DMX_PES_VIDEO12, DMX_PES_VIDEO13, DMX_PES_VIDEO14, DMX_PES_VIDEO15,
	DMX_PES_VIDEO16, DMX_PES_VIDEO17, DMX_PES_VIDEO18, DMX_PES_VIDEO19,
	DMX_PES_VIDEO20, DMX_PES_VIDEO21, DMX_PES_VIDEO22, DMX_PES_VIDEO23,
	DMX_PES_VIDEO24, DMX_PES_VIDEO25, DMX_PES_VIDEO26, DMX_PES_VIDEO27,
	DMX_PES_VIDEO28, DMX_PES_VIDEO29, DMX_PES_VIDEO30, DMX_PES_VIDEO31
};

static const unsigned int ttx_ids[] = {
	DMX_PES_TELETEXT0 , DMX_PES_TELETEXT1 , DMX_PES_TELETEXT2 , DMX_PES_TELETEXT3,
	DMX_PES_TELETEXT4 , DMX_PES_TELETEXT5 , DMX_PES_TELETEXT6 , DMX_PES_TELETEXT7,
	DMX_PES_TELETEXT8 , DMX_PES_TELETEXT9 , DMX_PES_TELETEXT10, DMX_PES_TELETEXT11,
	DMX_PES_TELETEXT12, DMX_PES_TELETEXT13, DMX_PES_TELETEXT14, DMX_PES_TELETEXT15,
	DMX_PES_TELETEXT16, DMX_PES_TELETEXT17, DMX_PES_TELETEXT18, DMX_PES_TELETEXT19,
	DMX_PES_TELETEXT20, DMX_PES_TELETEXT21, DMX_PES_TELETEXT22, DMX_PES_TELETEXT23,
	DMX_PES_TELETEXT24, DMX_PES_TELETEXT25, DMX_PES_TELETEXT26, DMX_PES_TELETEXT27,
	DMX_PES_TELETEXT28, DMX_PES_TELETEXT29, DMX_PES_TELETEXT30, DMX_PES_TELETEXT31
};

static const unsigned int subt_ids[] = {
	DMX_PES_SUBTITLE0 , DMX_PES_SUBTITLE1 , DMX_PES_SUBTITLE2 , DMX_PES_SUBTITLE3,
	DMX_PES_SUBTITLE4 , DMX_PES_SUBTITLE5 , DMX_PES_SUBTITLE6 , DMX_PES_SUBTITLE7,
	DMX_PES_SUBTITLE8 , DMX_PES_SUBTITLE9 , DMX_PES_SUBTITLE10, DMX_PES_SUBTITLE11,
	DMX_PES_SUBTITLE12, DMX_PES_SUBTITLE13, DMX_PES_SUBTITLE14, DMX_PES_SUBTITLE15,
	DMX_PES_SUBTITLE16, DMX_PES_SUBTITLE17, DMX_PES_SUBTITLE18, DMX_PES_SUBTITLE19,
	DMX_PES_SUBTITLE20, DMX_PES_SUBTITLE21, DMX_PES_SUBTITLE22, DMX_PES_SUBTITLE23,
	DMX_PES_SUBTITLE24, DMX_PES_SUBTITLE25, DMX_PES_SUBTITLE26, DMX_PES_SUBTITLE27,
	DMX_PES_SUBTITLE28, DMX_PES_SUBTITLE29, DMX_PES_SUBTITLE30, DMX_PES_SUBTITLE31
};

static const unsigned int pcr_ids[] = {
	DMX_PES_PCR0 , DMX_PES_PCR1 , DMX_PES_PCR2 , DMX_PES_PCR3 , DMX_PES_PCR4,
	DMX_PES_PCR5 , DMX_PES_PCR6 , DMX_PES_PCR7 , DMX_PES_PCR8 , DMX_PES_PCR8,
	DMX_PES_PCR9 , DMX_PES_PCR10, DMX_PES_PCR11, DMX_PES_PCR12, DMX_PES_PCR13,
	DMX_PES_PCR14, DMX_PES_PCR15, DMX_PES_PCR16, DMX_PES_PCR17, DMX_PES_PCR18,
	DMX_PES_PCR19, DMX_PES_PCR20, DMX_PES_PCR21, DMX_PES_PCR22, DMX_PES_PCR23,
	DMX_PES_PCR24, DMX_PES_PCR25, DMX_PES_PCR26, DMX_PES_PCR27, DMX_PES_PCR28,
	DMX_PES_PCR29, DMX_PES_PCR30, DMX_PES_PCR31
};

#ifdef CONFIG_STLINUXTV_DECODE_DISPLAY
/*
 * Return the number of the audio decoder based on the provided pes_type
 * If the pes_type doesn't correspond to an audio type, then the returned value is -1
 */
static inline int get_audio_dec_pes_type(unsigned int pes_type)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(audio_ids); i++) {
		if (pes_type == audio_ids[i])
			return i;
	}

	return -1;
}
#endif

/*
 * Return the number of the video decoder based on the provided pes_type
 * If the pes_type doesn't correspond to an video type, then the returned value is -1
 */
static inline int get_video_dec_pes_type(unsigned int pes_type)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(video_ids); i++) {
		if (pes_type == video_ids[i])
			return i;
	}

	return -1;
}

/**
 * stm_dmx_ttx_get_pes_type() - return teletext ID
 * @pes_type: DMX_PES_TELETEXTx
 * Return teletext ID if present in ttx_id[], else return -1
 */
static inline int stm_dmx_ttx_get_pes_type(unsigned int pes_type)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(ttx_ids); i++) {
		if (pes_type == ttx_ids[i])
			return i;
	}

	return -1;
}

/**
 * stm_dmx_subt_get_pes_type() - return subtitle ID
 * @pes_type: DMX_PES_SUBTITLEx
 * Return subtitle ID if present in subt_id[], else return -1
 */
static inline int stm_dmx_subt_get_pes_type(unsigned int pes_type)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(subt_ids); i++) {
		if (pes_type == subt_ids[i])
			return i;
	}

	return -1;
}

/*
 * Return the PCR number based on the provided pes_type
 * If the pes_type doesn't correspond to an PCR type, then the returned value is -1
 */
static inline int get_pcr_pes_type(unsigned int pes_type)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(pcr_ids); i++) {
		if (pes_type == pcr_ids[i])
			return i;
	}

	return -1;
}
#endif
