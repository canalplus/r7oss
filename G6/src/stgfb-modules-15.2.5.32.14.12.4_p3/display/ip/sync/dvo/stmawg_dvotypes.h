/***********************************************************************
 *
 * File: display/ip/sync/dvo/stmawg_dvotypes.h
 * Copyright (c) 2013 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef _STM_AWG_DVO_TYPES_H
#define _STM_AWG_DVO_TYPES_H

#include <stm_display.h>

#ifdef __cplusplus
extern "C" {
#endif


typedef struct stm_awg_dvo_parameters_s
{
  bool    bAllowEmbeddedSync;
  bool    bEnableData;
  uint8_t OutputClkDealy;
} stm_awg_dvo_parameters_t;

#ifdef __cplusplus
}
#endif

#endif /* _STM_AWG_DVO_TYPES_H */
