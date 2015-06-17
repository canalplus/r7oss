/************************************************************************
Copyright (C) 2012 STMicroelectronics. All Rights Reserved.

This file is part of the stm_fe Library.

stm_fe is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License version 2 as published by the
Free Software Foundation.

stm_fe is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with stm_fe; see the file COPYING.  If not, write to the Free Software
Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

The stm_fe Library may alternatively be licensed under a proprietary
license from ST.

Source file name : stvvglna.h
Author :

Header for STVVGLNA low level driver

Date        Modification                                    Name
----        ------------                                --------
23-Apr-12   Created

************************************************************************/
#ifndef _STVVGLNA_H
#define _STVVGLNA_H

#include <i2c_wrapper.h>

#if defined(CHIP_STAPI)		/*For STAPI use only */
/*#include "dbtypes.h"  for Install function*/
#endif

/*REG0*/
#define RSTVVGLNA_REG0  0x0000
#define FSTVVGLNA_LNA_IDENT  0x00000080
#define FSTVVGLNA_CUT_IDENT  0x00000060
#define FSTVVGLNA_RELEASE_ID  0x00000010
#define FSTVVGLNA_AGCTUPD  0x00000008
#define FSTVVGLNA_AGCTLOCK  0x00000004
#define FSTVVGLNA_RFAGCHIGH  0x00000002
#define FSTVVGLNA_RFAGCLOW  0x00000001

/*REG1*/
#define RSTVVGLNA_REG1  0x0001
#define FSTVVGLNA_LNAGCPWD  0x00010080
#define FSTVVGLNA_GETOFF  0x00010040
#define FSTVVGLNA_GETAGC  0x00010020
#define FSTVVGLNA_VGO  0x0001001f

/*REG2*/
#define RSTVVGLNA_REG2  0x0002
#define FSTVVGLNA_PATH2OFF  0x00020080
#define FSTVVGLNA_RFAGCPREF  0x00020070
#define FSTVVGLNA_PATH1OFF  0x00020008
#define FSTVVGLNA_RFAGCMODE  0x00020007

/*REG3*/
#define RSTVVGLNA_REG3  0x0003
#define FSTVVGLNA_SELGAIN  0x00030080
#define FSTVVGLNA_LCAL  0x00030070
#define FSTVVGLNA_RFAGCUPDATE  0x00030008
#define FSTVVGLNA_RFAGCCALSTART  0x00030004
#define FSTVVGLNA_SWLNAGAIN  0x00030003

#define STVVGLNA_NBREGS	4
#define STVVGLNA_NBFIELDS	20

typedef enum {
	VGLNA_RFAGC_HIGH,
	VGLNA_RFAGC_LOW,
	VGLNA_RFAGC_NORMAL
} SAT_VGLNA_STATUS;

typedef struct {
	ST_String_t Name;	/* Tuner name */
	U8 I2cAddress;		/* Tuner I2C address */
	U32 Reference;		/* reference frequency (Hz) */

	BOOL Repeater;
	STCHIP_Handle_t RepeaterHost;
	STCHIP_RepeaterFn_t RepeaterFn;

	FE_DEMOD_Model_t DemodModel;	/* Demod used with the tuner */

#ifdef CHIP_STAPI
	U32 TopLevelHandle;	/*Used in STTUNER */
#endif
	void *additionalParams;
} SAT_VGLNA_InitParams_t;

typedef SAT_VGLNA_InitParams_t *SAT_VGLNA_Params_t;

/* functions --------------------------------------------------------------- */

/* create instance of chip register mappings */

STCHIP_Error_t STVVGLNA_Init(void *pVGLNAInit, STCHIP_Handle_t *ChipHandle);

STCHIP_Error_t STVVGLNA_SetStandby(STCHIP_Handle_t hChip, U8 StandbyOn);
STCHIP_Error_t STVVGLNA_GetStatus(STCHIP_Handle_t hChip, U8 *Status);
STCHIP_Error_t STVVGLNA_GetGain(STCHIP_Handle_t hChip, S32 *Gain);

STCHIP_Error_t STVVGLNA_Term(STCHIP_Handle_t hChip);

#if defined(CHIP_STAPI)		/*For STAPI use only */
/*ST_ErrorCode_t STFRONTEND_STVVGLNA_Install(STFRONTEND_tunerDbase_t *Tuner,
 * STFRONTEND_TunerType_t TunerType);*/
/*ST_ErrorCode_t STFRONTEND_STVVGLNA_Uninstall(STFRONTEND_tunerDbase_t
 * *Tuner);*/
#endif

#endif /* _STVVGLNA_H */
