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

Source file name : stvvglna.c
Author :

Low level driver for STVVGLNA

Date        Modification                                    Name
----        ------------                                --------
23-Apr-12   Created

************************************************************************/
/*#include "globaldefs.h"*/
#include <linux/types.h>
#include <linux/module.h>
#include <stfe_utilities.h>
#include "stvvglna.h"

STCHIP_Error_t STVVGLNA_Init(void *pVGLNAInit_v, STCHIP_Handle_t *ChipHandle)
{
	STCHIP_Info_t ChipInfo;
	STCHIP_Handle_t hChip = NULL;
	STCHIP_Error_t error = CHIPERR_NO_ERROR;
	SAT_VGLNA_Params_t pVGLNAInit = (SAT_VGLNA_Params_t) pVGLNAInit_v;

	STCHIP_Register_t DefSTVVGLNAVal[STVVGLNA_NBREGS] = {
		{RSTVVGLNA_REG0, 0x20},
		{RSTVVGLNA_REG1, 0x0F},
		{RSTVVGLNA_REG2, 0x50},
		{RSTVVGLNA_REG3, 0x20}
	};

#ifdef CHIP_STAPI
	/*Fix coverity Warning */
	if (*ChipHandle)
		/*Copy settings already contained in hTuner to ChipInfo: Change
		 * ///AG */
		memcpy(&ChipInfo, *ChipHandle, sizeof(STCHIP_Info_t));
	else
		return CHIPERR_INVALID_HANDLE;
#endif

	if (pVGLNAInit != NULL) {
		if (strlen((char *)pVGLNAInit->Name) < MAXNAMESIZE)
			/* Tuner name */
			strcpy((char *)ChipInfo.Name, (char *)pVGLNAInit->Name);
		else
			error = TUNER_TYPE_ERR;

		/* Repeater host */
		ChipInfo.RepeaterHost = pVGLNAInit->RepeaterHost;
		/* Repeater enable/disable function */
		ChipInfo.RepeaterFn = pVGLNAInit->RepeaterFn;
		/* Tuner need to enable repeater */
		ChipInfo.Repeater = pVGLNAInit->Repeater;
		/* Init tuner I2C address */
		ChipInfo.I2cAddr = pVGLNAInit->I2cAddress;

		ChipInfo.NbRegs = STVVGLNA_NBREGS;
		ChipInfo.NbFields = STVVGLNA_NBFIELDS;
		ChipInfo.ChipMode = STCHIP_MODE_SUBADR_8;
		ChipInfo.WrStart = RSTVVGLNA_REG0;
		ChipInfo.WrSize = STVVGLNA_NBREGS;
		ChipInfo.RdStart = RSTVVGLNA_REG0;
		ChipInfo.RdSize = STVVGLNA_NBREGS;
		ChipInfo.pData = NULL;

#ifdef HOST_PC
		hChip = ChipOpen(&ChipInfo);
		(*ChipHandle) = hChip;
#endif

#ifdef CHIP_STAPI
		hChip = *ChipHandle;
		memcpy(hChip, &ChipInfo, sizeof(STCHIP_Info_t));

#endif

		if (!(*ChipHandle))
			error = CHIPERR_INVALID_HANDLE;

		if (hChip != NULL) {
			/*******************************
			**   CHIP REGISTER MAP IMAGE INITIALIZATION
			**     ----------------------
			********************************/
			ChipUpdateDefaultValues(hChip, DefSTVVGLNAVal);
			hChip->ChipID =
			    ChipGetOneRegister(hChip, RSTVVGLNA_REG0);
			error = hChip->Error;
		}

	} else
		error = CHIPERR_INVALID_HANDLE;

	return error;

}
EXPORT_SYMBOL(STVVGLNA_Init);

STCHIP_Error_t STVVGLNA_SetStandby(STCHIP_Handle_t hChip, U8 StandbyOn)
{
	STCHIP_Error_t error = CHIPERR_NO_ERROR;

	if (hChip != NULL) {
		if (StandbyOn)
			error = ChipSetField(hChip, FSTVVGLNA_LNAGCPWD, 1);
		else
			error = ChipSetField(hChip, FSTVVGLNA_LNAGCPWD, 0);
	} else
		error = CHIPERR_INVALID_HANDLE;

	return error;
}
EXPORT_SYMBOL(STVVGLNA_SetStandby);

STCHIP_Error_t STVVGLNA_GetStatus(STCHIP_Handle_t hChip, U8 *Status)
{
	STCHIP_Error_t error = CHIPERR_NO_ERROR;
	U8 flagAgcLow, flagAgcHigh;

	if (hChip != NULL) {
		flagAgcHigh = ChipGetField(hChip, FSTVVGLNA_RFAGCHIGH);
		flagAgcLow = ChipGetField(hChip, FSTVVGLNA_RFAGCLOW);

		if (flagAgcHigh)
			(*Status) = VGLNA_RFAGC_HIGH;
		else if (flagAgcLow)
			(*Status) = (U8) VGLNA_RFAGC_LOW;
		else
			(*Status) = (U8) VGLNA_RFAGC_NORMAL;

		/*error=ChipGetError(hChip);    */
	} else
		error = CHIPERR_INVALID_HANDLE;

	return error;
}
EXPORT_SYMBOL(STVVGLNA_GetStatus);

STCHIP_Error_t STVVGLNA_GetGain(STCHIP_Handle_t hChip, S32 *Gain)
{

	S32 VGO, swlnaGain;

	STCHIP_Error_t error = CHIPERR_NO_ERROR;

	if (hChip != NULL) {
		VGO = ChipGetField(hChip, FSTVVGLNA_VGO);
		/*error=ChipGetError(hChip); */

		/*Trig to read the VGO and SWLNAGAIN val */
		ChipSetFieldImage(hChip, FSTVVGLNA_GETAGC, 1);
		ChipSetRegisters(hChip, RSTVVGLNA_REG1, 1);
		WAIT_N_MS(5);

		/*(31-VGO[4:0]) * 13/31 + (3-SWLNAGAIN[1:0])*6 */
		VGO = ChipGetField(hChip, FSTVVGLNA_VGO);
		swlnaGain = ChipGetField(hChip, FSTVVGLNA_SWLNAGAIN);
		(*Gain) = (31 - VGO) * 13;
		(*Gain) /= 31;
		(*Gain) += (6 * (3 - swlnaGain));

	} else
		error = CHIPERR_INVALID_HANDLE;

	return error;
}
EXPORT_SYMBOL(STVVGLNA_GetGain);

STCHIP_Error_t STVVGLNA_Term(STCHIP_Handle_t hChip)
{
	STCHIP_Error_t error = CHIPERR_NO_ERROR;

	if (hChip) {
#ifndef CHIP_STAPI
		if (hChip->pData)
			free(hChip->pData);
		ChipClose(hChip); /*Changes are required for STAPI as well */
#endif
	}

	return error;
}
EXPORT_SYMBOL(STVVGLNA_Term);

static int32_t __init stvvglna_init(void)
{
	pr_info("Loading VGLNA module ...\n");

	return 0;
}

module_init(stvvglna_init);

static void __exit stvvglna_term(void)
{
	pr_info("Removing VGLNA module ...\n");

}

module_exit(stvvglna_term);

MODULE_DESCRIPTION("STVVGLNA device drivers");
MODULE_AUTHOR("STMicroelectronics Limited");
MODULE_LICENSE("GPL");
