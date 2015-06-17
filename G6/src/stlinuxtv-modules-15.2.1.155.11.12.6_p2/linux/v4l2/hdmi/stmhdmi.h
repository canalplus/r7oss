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
 *************************************************************************/
#ifndef _STMHDMI_H
#define _STMHDMI_H

/*
 * Or'd flags to STMHDMIIO_FLUSH_DATA_PACKET_QUEUE
 */
#define STMHDMIIO_FLUSH_ACP_QUEUE    0x00000001
#define STMHDMIIO_FLUSH_ISRC_QUEUE   0x00000002
#define STMHDMIIO_FLUSH_GAMUT_QUEUE  0x00000004
#define STMHDMIIO_FLUSH_VENDOR_QUEUE 0x00000008
#define STMHDMIIO_FLUSH_NTSC_QUEUE   0x00000010
#define STMHDMIIO_FLUSH_ALL          0x0000001f

enum stmhdmiio_overscan_mode {
	STMHDMIIO_SCAN_UNKNOWN,
	STMHDMIIO_SCAN_OVERSCANNED,
	STMHDMIIO_SCAN_UNDERSCANNED
};

enum stmhdmiio_content_type {
	STMHDMIIO_IT_GRAPHICS,
	STMHDMIIO_IT_PHOTO,
	STMHDMIIO_IT_CINEMA,
	STMHDMIIO_IT_GAME,
	STMHDMIIO_CE_CONTENT,
};

/*
 * The STMHDMIIO_SET_QUANTIZATION_MODE ioctl changes the RGB or YCC quantization
 * bits set in the AVI infoframe when the sink device reports support for them.
 *
 * The auto mode will pick the most appropriate value based on the configuration
 * of the HDMI output.
 */
enum stmhdmiio_quantization_mode {
	STMHDMIIO_QUANTIZATION_AUTO,
	STMHDMIIO_QUANTIZATION_DEFAULT,
	STMHDMIIO_QUANTIZATION_LIMITED,
	STMHDMIIO_QUANTIZATION_FULL
};

#define STMHDMIIO_AUDIO_SOURCE_2CH_I2S		0
#define STMHDMIIO_AUDIO_SOURCE_PCM		STMHDMIIO_AUDIO_SOURCE_2CH_I2S
#define STMHDMIIO_AUDIO_SOURCE_SPDIF		1
#define STMHDMIIO_AUDIO_SOURCE_8CH_I2S		2
#define STMHDMIIO_AUDIO_SOURCE_NONE		0xFFFFFFFF

#define STMHDMIIO_SAFE_MODE_DVI		0
#define STMHDMIIO_SAFE_MODE_HDMI	1

#endif /* _STMHDMI_H */
