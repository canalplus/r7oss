/***********************************************************************
 *
 * File: display/ip/sync/dvo/fw_gen/fw_gen.h
 * Copyright (c) 2013 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef __DVOFMW_H
#define __DVOFMW_H

#include "fw_common.h"

#include <display/ip/sync/dvo/stmawg_dvotypes.h>

#if defined(__cplusplus)
extern "C" {
#endif

int stm_dvo_awg_generate_code ( const stm_display_mode_t * const pMode
                              , const uint32_t                   uFormat
                              , stm_awg_dvo_parameters_t         sAwgCodeParams
                              , uint8_t *                        pRamSize
                              , uint32_t **                      pRamCode);

#ifdef __cplusplus
}
#endif

#endif /* __DVOFMW_H */
