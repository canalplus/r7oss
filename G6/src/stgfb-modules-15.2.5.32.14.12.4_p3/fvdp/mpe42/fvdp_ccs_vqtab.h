/***********************************************************************
 *
 * File: ip/fvdp/mpe42/fvdp_ccs_vqtab.h
 * Copyright (c) 2013 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

/* Define to prevent recursive inclusion */
#ifndef FVDP_CCS_VQTAB_H
#define FVDP_CCS_VQTAB_H

/* Includes ----------------------------------------------------------------- */

    /* C++ support */
#ifdef __cplusplus
    extern "C" {
#endif

/* Private Constants -------------------------------------------------------- */

// default VQData tables
VQ_CCS_Parameters_t CCSVQTable =
{
	{
		sizeof(VQ_CCS_Parameters_t),
		FVDP_CCS,
		0
	},
	1,	// EnableGlobalMotionAdaptive
	0,	// MotionInterpolation
	6,	// LowMotionTh0
	32,	// LowMotionTh1
	6,	// StdMotionTh0
	32,	// StdMotionTh1
	0,	// HighMotionTh0
	0,	// HighMotionTh1
	3,	// GlobalMotionStdTh
	4800	// GlobalMotionHighTh
};

/* Exported Types ----------------------------------------------------------- */

/* Exported Macros ---------------------------------------------------------- */

/* Exported Variables ------------------------------------------------------- */

/* Exported Functions ------------------------------------------------------- */

#ifdef __cplusplus
}
#endif

#endif /* FVDP_CCS_VQTAB_H */

