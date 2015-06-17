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
 * - Exporting audio decoder to mixer connection functionality
 *************************************************************************/
#ifndef H_AUDIO_OUT
#define H_AUDIO_OUT

int stm_dvb_audio_connect_sink(const struct media_pad *src_pad,
				const struct media_pad *dst_pad,
				stm_se_play_stream_h play_stream);

int stm_dvb_audio_disconnect_sink(const struct media_pad *src_pad,
				const struct media_pad *dst_pad,
				stm_se_play_stream_h play_stream);
#endif
