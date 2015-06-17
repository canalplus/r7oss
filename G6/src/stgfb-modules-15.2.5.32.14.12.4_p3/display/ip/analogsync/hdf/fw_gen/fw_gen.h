/***********************************************************************
 *
 * File: display/ip/analogsync/hdf/fw_gen/fw_gen.h
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

#ifndef __HDF_FW_GEN__
#define __HDF_FW_GEN__

#define AWG_MAX_SIZE 64

#if defined(__cplusplus)
extern "C" {
#endif

typedef struct HDFParams_s
{
  uint32_t                     ANARAMRegisters[AWG_MAX_SIZE];
  uint32_t                     ANA_SCALE_CTRL_DAC_Cb;
  uint32_t                     ANA_SCALE_CTRL_DAC_Y;
  uint32_t                     ANA_SCALE_CTRL_DAC_Cr;
} HDFParams_t;

int Ana_GenerateAwgCodeRGB(stm_analog_sync_setup_t *AnalogSyncSetup_p, HDFParams_t *HDFParams_p);
int Ana_GenerateAwgCodeYUV(stm_analog_sync_setup_t *AnalogSyncSetup_p, HDFParams_t *HDFParams_p);

#ifdef __cplusplus
}
#endif

#endif
