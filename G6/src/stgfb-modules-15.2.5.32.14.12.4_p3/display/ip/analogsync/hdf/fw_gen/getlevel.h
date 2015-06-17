/***********************************************************************
 *
 * File: display/ip/analogsync/hdf/fw_gen/getlevel.h
 * Copyright (c) 2011 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#include "fw_common.h"
#include <stm_display.h>

#ifndef __GETLEVEL_AS_H__
#define __GETLEVEL_AS_H__

#ifdef __cplusplus
extern "C" {
#endif

typedef struct video_s{
   uint32_t sync_low;
   uint32_t sync_high;
   uint32_t blank_vbi_lines;
   uint32_t blank_act_vid;
   uint32_t black;
   uint32_t peak_luma;
} video_t;


/* function declarations */
bool Get_AS_Levels(stm_display_mode_id_t TimingMode, video_t *Video_p);

#ifdef __cplusplus
}
#endif

#endif /* __GETLEVEL_AS_H__ */

