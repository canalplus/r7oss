/***********************************************************************
 *
 * File: fvdp/mpe42/fvdp_tnrlite.h
 * Copyright (c) 2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef FVDP_TNRLITE_H
#define FVDP_TNRLITE_H

/* Includes ----------------------------------------------------------------- */

#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus */
#include "fvdp_private.h"
#include "../fvdp.h"
#include "../fvdp_vqdata.h"

/* Exported Constants ------------------------------------------------------- */

/* Exported Types ----------------------------------------------------------- */
void TnrLite_Init     (void);
void TnrLite_Enable   (bool LumaOnly);
void TnrLite_Disable  (void);
FVDP_Result_t TnrLite_SetVqData(VQ_TNR_Adaptive_Parameters_t *, bool LumaOnly, uint32_t H_Active, uint32_t V_Active);

/* Exported Variables ------------------------------------------------------- */



/* Exported Macros ---------------------------------------------------------- */

/* Exported Functions ------------------------------------------------------- */

#if defined(__cplusplus)
}
#endif /* __cplusplus */

#endif /* FVDP_TNRLITE_H */

