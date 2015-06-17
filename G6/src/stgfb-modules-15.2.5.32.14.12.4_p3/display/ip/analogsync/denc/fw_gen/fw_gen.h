/***********************************************************************
 *
 * File: display/ip/analogsync/denc/fw_gen/fw_gen.h
 * Copyright (c) 2011 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#include "fw_common.h"
#include <stm_display.h>

#include <display/ip/analogsync/stmanalogsynctypes.h>

#ifndef __DENC_FW_GEN__
#define __DENC_FW_GEN__


#if defined(__cplusplus)
extern "C" {
#endif

typedef struct DACMult_Config_s
{
  uint8_t                      DACMult_Config_Cb;
  uint8_t                      DACMult_Config_Y;
  uint8_t                      DACMult_Config_Cr;
} DACMult_Config_t;

int Ana_GenerateDencCodeRGB(stm_analog_sync_setup_t *AnalogSyncSetup_p, DACMult_Config_t *DACMult_Config_p);
int Ana_GenerateDencCodeYUV(stm_analog_sync_setup_t *AnalogSyncSetup_p, DACMult_Config_t *DACMult_Config_p);

#ifdef __cplusplus
}
#endif

#endif
