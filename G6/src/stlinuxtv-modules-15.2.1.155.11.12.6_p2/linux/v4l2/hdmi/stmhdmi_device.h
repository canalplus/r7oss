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
#ifndef __STMHDMI_DEVICE_H_
#define __STMHDMI_DEVICE_H_

#include <linux/stm/stmcorehdmi.h>
#include <media/v4l2-dev.h>

#define EDID_MAX_SIZE           128
/*
 * Standard EDID defines
 */
#define STM_EDID_MANUFACTURER        0x08
#define STM_EDID_PROD_ID             0x0A
#define STM_EDID_SERIAL_NR           0x0C
#define STM_EDID_PRODUCTION_WK       0x10
#define STM_EDID_PRODUCTION_YR       0x11
#define STM_EDID_VERSION             0x12
#define STM_EDID_REVISION            0x13
#define STM_EDID_MAX_HSIZE           0x15
#define STM_EDID_MAX_VSIZE           0x16
#define STM_EDID_FEATURES            0x18
#define STM_EDID_ESTABLISHED_TIMING1 0x23
#define STM_EDID_ESTABLISHED_TIMING2 0x24
#define STM_EDID_MANUFACTURER_TIMING 0x25
#define STM_EDID_STD_TIMING_START    0x26
#define STM_EDID_DEFAULT_TIMING      0x36
#define STM_EDID_INFO_1              0x48
#define STM_EDID_INFO_2              0x5A
#define STM_EDID_INFO_3              0x6C

#define STM_EDID_MD_RANGES           0xFD
#define STM_EDID_MD_TYPE_NAME        0xFC

#define STM_EDID_EXTENSION           0x7E

#define STM_EDID_VGA_TIMING_CODE     0x20

#define STM_EDID_FEATURES_GTF        (1<<0)
#define STM_EDID_FEATURES_PT         (1<<1)
#define STM_EDID_FEATURES_SRGB       (1<<1)

#define STM_EDID_DTD_SIZE            18

/*
 * CEA Extension defines
 */
#define STM_EXT_TYPE              0
#define STM_EXT_TYPE_CEA          0x2
#define STM_EXT_TYPE_EDID_20      0x20
#define STM_EXT_TYPE_COLOR_INFO_0 0x30
#define STM_EXT_TYPE_DI           0x40
#define STM_EXT_TYPE_LS           0x50
#define STM_EXT_TYPE_MI           0x60
#define STM_EXT_TYPE_BLOCK_MAP    0xF0
#define STM_EXT_TYPE_MANUFACTURER 0xFF
#define STM_EXT_VERSION           1
#define STM_EXT_TIMING_OFFSET     2
#define STM_EXT_FORMATS           3
#define STM_EXT_DATA_BLOCK        4

#define STM_EXT_FORMATS_YCBCR_422 (1<<4)
#define STM_EXT_FORMATS_YCBCR_444 (1<<5)
#define STM_EXT_FORMATS_AUDIO     (1<<6)
#define STM_EXT_FORMATS_UNDERSCAN (1<<7)

#define STM_CEA_BLOCK_TAG_SHIFT   5
#define STM_CEA_BLOCK_TAG_MASK    (0x7<<STM_CEA_BLOCK_TAG_SHIFT)
#define STM_CEA_BLOCK_LEN_MASK    0x1F

#define STM_CEA_BLOCK_TAG_AUDIO   1
#define STM_CEA_BLOCK_TAG_VIDEO   2
#define STM_CEA_BLOCK_TAG_VENDOR  3
#define STM_CEA_BLOCK_TAG_SPEAKER 4
#define STM_CEA_BLOCK_TAG_EXT     7

#define STM_CEA_BLOCK_AUDIO_FORMAT_MASK   0x78
#define STM_CEA_BLOCK_AUDIO_FORMAT_SHIFT  3
#define STM_CEA_BLOCK_AUDIO_CHANNELS_MASK 0x7

#define STM_CEA_BLOCK_EXT_TAG_VCDB        0x0
#define STM_CEA_BLOCK_EXT_TAG_COLORIMETRY 0x5

/* VSDB byte 8 block content flags */
#define STM_HDMI_VSDB_VIDEO_PRESENT      (1L<<5)
#define STM_HDMI_VSDB_I_LATENCY_PRESENT  (1L<<6)
#define STM_HDMI_VSDB_LATENCY_PRESENT    (1L<<7)

/* VSDB video features flags */
#define STM_HDMI_VSDB_MULTI_STRUCTURE_ALL      (1L<<5)
#define STM_HDMI_VSDB_MULTI_STRUCTURE_AND_MASK (2L<<5)
#define STM_HDMI_VSDB_MULTI_MASK               (3L<<5)
#define STM_HDMI_VSDB_3D_PRESENT               (1L<<7)
#define STM_HDMI_VSDB_3D_FLAGS_MASK            (7L<<5)

#define STM_3D_DETAIL_ALL           (0x0)
#define STM_3D_DETAIL_H_SUBSAMPLING (0x1)
#define STM_3D_DETAIL_All_QUINCUNX  (0x6)

int stmhdmi_read_edid(struct stm_hdmi *hdmi);
int stmhdmi_safe_edid(struct stm_hdmi *hdmi);
void stmhdmi_invalidate_edid_info(struct stm_hdmi *hdmi);
void stmhdmi_dump_edid(struct stm_hdmi *hdmi);
int stmhdmi_manager(void *data);
int stmhdmi_register_sysfs_attributes(struct device *device);
void stmhdmi_unregister_sysfs_attributes(struct device *device);
void stm_v4l2_event_queue(int event_type, int event_queue);

#undef DEBUG_HDMI
#ifdef DEBUG_HDMI
#define HDMI_DEBUG(msg, args...)	printk(KERN_DEFAULT "%s(): " msg, __FUNCTION__, ## args)
#else
#define HDMI_DEBUG(msg, args...)
#endif
#define HDMI_INFO(msg, args...)		printk(KERN_INFO "%s(): " msg, __FUNCTION__, ## args)
#define HDMI_ERROR(msg, args...)	printk(KERN_ERR "%s(): " msg, __FUNCTION__, ## args)

#endif /* __STMHDMI_DEVICE_H_ */
