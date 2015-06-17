/***********************************************************************
 *
 * File: fvdp/mpe42/fvdp_tnr.h
 * Copyright (c) 2013 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef FVDP_TNR_H
#define FVDP_TNR_H

/* Includes ----------------------------------------------------------------- */

#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus */
#include "fvdp_private.h"
#include "../fvdp.h"
#include "../fvdp_vqdata.h"

/* Exported Functions ------------------------------------------------------- */
void Tnr_Init (FVDP_HandleContext_t* HandleContext_p);
void Tnr_Enable     (bool InterlacedInput, uint32_t H_Active, uint32_t V_Active);
void Tnr_Bypas      (uint32_t H_Active, uint32_t V_Active);
FVDP_Result_t Tnr_UpdateTnrControlRegister(void);
void Tnr_Demo       (uint32_t H_Start, uint32_t H_End, uint32_t V_Start, uint32_t V_End, bool enable);
FVDP_Result_t Tnr_SetVqData(const VQ_TNR_Advanced_Parameters_t * vq_data_p);
void Tnr_LoadDefaultVqData(void);
#if defined(__cplusplus)
}
#endif /* __cplusplus */

#endif /* FVDP_TNRLITE_H */

