/************************************************************************
 * Copyright (C) 2014 STMicroelectronics. All Rights Reserved.
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
 * Definitions for STM V4L2 Decoder controls
 *************************************************************************/
#ifndef __STM_V4L2_DECODER_EXPORT_H__
#define __STM_V4L2_DECODER_EXPORT_H__

/*
 * ST specific decoder controls
 * @V4L2_CID_MPEG_STI_HDMI_REPEATER_MODE: This control is to configure the decoders
 *                                        in low latency mode, when the input is coming from hdmirx
 */
#define V4L2_CID_MPEG_STI_BASE			(V4L2_CTRL_CLASS_MPEG | 0x3000)
#define V4L2_CID_MPEG_STI_HDMI_REPEATER_MODE	(V4L2_CID_MPEG_STI_BASE + 1)

/*
 * Repeater controls over decoder
 */
enum repeater_mode {
	V4L2_CID_MPEG_STI_HDMIRX_DISABLED,
	V4L2_CID_MPEG_STI_HDMIRX_REPEATER,
	V4L2_CID_MPEG_STI_HDMIRX_NON_REPEATER,
};

#endif
