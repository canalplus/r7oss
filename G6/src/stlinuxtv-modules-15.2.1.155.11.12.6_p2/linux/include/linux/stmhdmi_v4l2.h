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
 * Implementation of HDMI controls using V4L2
 *************************************************************************/
#ifndef __STMHDMI_V4L2_H_
#define __STMHDMI_V4L2_H_

/*
 * FIXME:
 * The following Digital Video (DV) controls are not available
 * in the current kernel release. These needs to removed once
 * we have them as standard kernel release
 * The patch has been used from:
 * 	https://patchwork.kernel.org/patch/1305631/
 */
/* TODO: Removal start here */
struct v4l2_subdev_edid {
	__u32 pad;
	__u32 start_block;
	__u32 blocks;
	__u32 reserved[5];
	__u8 *edid;
};

#define VIDIOC_SUBDEV_G_EDID	_IOWR('V', 63, struct v4l2_subdev_edid)
#define VIDIOC_SUBDEV_S_EDID	_IOWR('V', 64, struct v4l2_subdev_edid)

#define V4L2_CTRL_CLASS_DV 0x00a00000
/*  DV-class control IDs defined by V4L2 */
#define V4L2_CID_DV_CLASS_BASE			(V4L2_CTRL_CLASS_DV | 0x900)
#define V4L2_CID_DV_CLASS			(V4L2_CTRL_CLASS_DV | 1)

#define	V4L2_CID_DV_TX_HOTPLUG			(V4L2_CID_DV_CLASS_BASE + 1)
#define	V4L2_CID_DV_TX_RXSENSE			(V4L2_CID_DV_CLASS_BASE + 2)
#define	V4L2_CID_DV_TX_EDID_PRESENT		(V4L2_CID_DV_CLASS_BASE + 3)
#define	V4L2_CID_DV_TX_MODE			(V4L2_CID_DV_CLASS_BASE + 4)
#define V4L2_CID_DV_TX_RGB_RANGE		(V4L2_CID_DV_CLASS_BASE + 5)

#define	V4L2_CID_DV_RX_POWER_PRESENT		(V4L2_CID_DV_CLASS_BASE + 100)
#define V4L2_CID_DV_RX_RGB_RANGE		(V4L2_CID_DV_CLASS_BASE + 101)
/* TODO: Removal ends here */

/*
 * Defining STM DV control class
 */
#define V4L2_CID_DV_STM_CLASS_BASE			(V4L2_CID_DV_CLASS | 0x2000)

#define V4L2_CID_DV_STM_SPD_VENDOR_DATA			(V4L2_CID_DV_STM_CLASS_BASE + 1)
#define V4L2_CID_DV_STM_SPD_PRODUCT_DATA		(V4L2_CID_DV_STM_CLASS_BASE + 2)
#define V4L2_CID_DV_STM_SPD_IDENTIFIER_DATA		(V4L2_CID_DV_STM_CLASS_BASE + 3)
#define V4L2_CID_DV_STM_TX_AVMUTE			(V4L2_CID_DV_STM_CLASS_BASE + 4)
#define V4L2_CID_DV_STM_TX_AUDIO_SOURCE			(V4L2_CID_DV_STM_CLASS_BASE + 5)
/*
 * enum defining the Audio source controls. These are currently mapped
 * to the audio source controls in "stmfb/include/stmdisplayoutput.h" with
 * stm_display_output_audio_source_e.
 */
enum v4l2_dv_stm_audio_source_type {
	V4L2_DV_STM_AUDIO_SOURCE_2CH_I2S ,
	V4L2_DV_STM_AUDIO_SOURCE_PCM,
	V4L2_DV_STM_AUDIO_SOURCE_SPDIF,
	V4L2_DV_STM_AUDIO_SOURCE_8CH_I2S ,
	V4L2_DV_STM_AUDIO_SOURCE_NONE
};

#define V4L2_DV_STM_TX_OVERSCAN_MODE			(V4L2_CID_DV_STM_CLASS_BASE + 6)
/*
 * enum defining the scan mode for HDMI-Tx interface. There is a one-to-one
 * mapping with VIBE defined scan controls
 */
enum v4l2_dv_stm_overscan_mode {
	V4L2_DV_STM_SCAN_UNKNOWN ,
	V4L2_DV_STM_SCAN_OVERSCANNED ,
	V4L2_DV_STM_SCAN_UNDERSCANNED ,
};

#define V4L2_CID_DV_STM_TX_CONTENT_TYPE			(V4L2_CID_DV_STM_CLASS_BASE + 7)
/*
 * enum defining the content control of HDMI-Tx interface. There is one-to-one
 * mapping with the VIBE defined content controls
 */
enum v4l2_dv_stm_content_type {
	V4L2_DV_STM_IT_GRAPHICS ,
	V4L2_DV_STM_IT_PHOTO ,
	V4L2_DV_STM_IT_CINEMA ,
	V4L2_DV_STM_IT_GAME ,
	V4L2_DV_STM_CE_CONTENT,
};

#define V4L2_CID_DV_STM_FLUSH_DATA_PACKET_QUEUE		(V4L2_CID_DV_STM_CLASS_BASE + 8)
/*
 * enum defining the queues for HDMI-Tx inteface
 */
enum v4l2_dv_stm_packet_queue {
	V4L2_DV_STM_FLUSH_ACP_QUEUE = 0x1,
	V4L2_DV_STM_FLUSH_ISRC_QUEUE = 0x2,
	V4L2_DV_STM_FLUSH_VENDOR_QUEUE = 0x4,
	V4L2_DV_STM_FLUSH_GAMUT_QUEUE = 0x8,
	V4L2_DV_STM_FLUSH_NTSC_QUEUE = 0x10
};
	
#define V4L2_CID_DV_STM_TX_CONTROL			(V4L2_CID_DV_STM_CLASS_BASE + 9)
#define V4L2_CID_DV_STM_ENCRYPTED_FRAME_CONTROL		(V4L2_CID_DV_STM_CLASS_BASE + 10)
#define V4L2_CID_DV_STM_SRM_VALUE			(V4L2_CID_DV_STM_CLASS_BASE + 11)

/*
 * New ST specific events are added here
 */
#define V4L2_EVENT_CLASS_DV			(V4L2_EVENT_PRIVATE_START + 1 * 1000)
#define V4L2_EVENT_STM_TX_HOTPLUG		(V4L2_EVENT_CLASS_DV + 1)

/*
 * Private HDMI-Hotplug states
 */
enum v4l2_stm_dv_hpd_state {
	V4L2_DV_STM_HPD_STATE_LOW,
	V4L2_DV_STM_HPD_STATE_HIGH,
};
#endif
