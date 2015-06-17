/***********************************************************************
 *
 * File: display/ip/analogsync/stmanalogsynctypes.h
 * Copyright (c) 2011 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef _STM_ANALOG_SYNC_TYPES_H
#define _STM_ANALOG_SYNC_TYPES_H

#include <stm_display.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct stm_analog_sync_setup_s
{
  stm_display_mode_id_t         TimingMode;
  stm_display_analog_factors_t  AnalogFactors;
} stm_analog_sync_setup_t;

#ifdef __cplusplus
}
#endif

#endif /* _STM_ANALOG_SYNC_TYPES_H */
