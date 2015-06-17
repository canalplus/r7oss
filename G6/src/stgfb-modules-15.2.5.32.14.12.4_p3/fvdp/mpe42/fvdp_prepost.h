/***********************************************************************
 *
 * File: fvdp/mpe42/fvdp_prepost.h
 * Copyright (c) 2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef FVDP_PREPOST_H
#define FVDP_PREPOST_H

/* Includes ----------------------------------------------------------------- */

#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus */

#include "fvdp_router.h"

/* Exported Constants ------------------------------------------------------- */
#define MAX_MEMORY_CROP_H_START                32
#define MAX_MEMORY_CROP_V_START                16

/* Exported Types ----------------------------------------------------------- */

/* Exported Variables ------------------------------------------------------- */

extern DataPathModule_t PRE_MODULE[NUM_FVDP_CH];
extern DataPathModule_t POST_MODULE[NUM_FVDP_CH];

/* Exported Macros ---------------------------------------------------------- */

/* Exported Functions ------------------------------------------------------- */

#if defined(__cplusplus)
}
#endif /* __cplusplus */

#endif /* FVDP_PREPOST_H */

