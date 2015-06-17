/***********************************************************************
 *
 * File: fvdp/mpe42/fvdp_madi.h
 * Copyright (c) 2013 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef FVDP_MADI_H
#define FVDP_MADI_H

/* Includes ----------------------------------------------------------------- */

#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus */
#include "fvdp_private.h"
#include "../fvdp.h"
#include "../fvdp_vqdata.h"

/* Exported Functions ------------------------------------------------------- */
void          Madi_Init         (void);
void          Madi_Enable       (void);
void          Madi_Disable      (void);

FVDP_Result_t Madi_SetVqData    (const VQ_McMadi_Parameters_t * vq_data_p);
void          Madi_LoadDefaultVqData(void);
FVDP_Result_t Madi_SetPixelNumberForMDThr(uint16_t H_Size, uint16_t V_Size);
FVDP_Result_t Madi_SetLASDThr(uint16_t  H_Size, uint16_t V_Size);

#if defined(__cplusplus)
}
#endif /* __cplusplus */

#endif /* FVDP_TNRLITE_H */

