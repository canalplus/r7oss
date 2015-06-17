/************************************************************************
Copyright (C) 2011 STMicroelectronics. All Rights Reserved.

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

Source file name : stv0367qam_init.c
Author :           LLA

367 LLA init file

Date        Modification                                    Name
----        ------------                                    --------
20-Jun-11   Created
04-Jul-12   Updated to v1.9
************************************************************************/
#ifndef ST_OSLINUX
/*#include <string.h>*/
#endif
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/delay.h>
#include <linux/stm/demod.h>
#include <stfe_utilities.h>
#include <stm_fe_os.h>
#include <i2c_wrapper.h>

/*#include <globaldefs.h>*/
#include "stv0367qam_init.h"
/*#include "stv0367qam_drv.h"*/

STCHIP_Register_t Def367qamVal[STV0367qam_NBREGS] = {
	{R367qam_ID, 0x60}
	,			/* ID */
	{R367qam_I2CRPT, 0x22}
	,			/* I2CRPT */
	{R367qam_TOPCTRL, 0x10}
	,			/* TOPCTRL */
	{R367qam_IOCFG0, 0x80}
	,			/* IOCFG0 */
	{R367qam_DAC0R, 0x00}
	,			/* DAC0R */
	{R367qam_IOCFG1, 0x00}
	,			/* IOCFG1 */
	{R367qam_DAC1R, 0x00}
	,			/* DAC1R */
	{R367qam_IOCFG2, 0x00}
	,			/* IOCFG2 */
	{R367qam_SDFR, 0x00}
	,			/* SDFR */
	{R367qam_AUX_CLK, 0x00}
	,			/* AUX_CLK */
	{R367qam_FREESYS1, 0x00}
	,			/* FREESYS1 */
	{R367qam_FREESYS2, 0x00}
	,			/* FREESYS2 */
	{R367qam_FREESYS3, 0x00}
	,			/* FREESYS3 */
	{R367qam_GPIO_CFG, 0x55}
	,			/* GPIO_CFG */
	{R367qam_GPIO_CMD, 0x01}
	,			/* GPIO_CMD */
	{R367qam_TSTRES, 0x00}
	,			/* TSTRES */
	{R367qam_ANACTRL, 0x00}
	,			/* ANACTRL */
	{R367qam_TSTBUS, 0x00}
	,			/* TSTBUS */
	{R367qam_RF_AGC1, 0xea}
	,			/* RF_AGC1 */
	{R367qam_RF_AGC2, 0x82}
	,			/* RF_AGC2 */
	{R367qam_ANADIGCTRL, 0x0b}
	,			/* ANADIGCTRL */
	{R367qam_PLLMDIV, 0x01}
	,			/* PLLMDIV */
	{R367qam_PLLNDIV, 0x08}
	,			/* PLLNDIV */
	{R367qam_PLLSETUP, 0x18}
	,			/* PLLSETUP */
	{R367qam_DUAL_AD12, 0x04}
	,			/* DUAL_AD12 */
	{R367qam_TSTBIST, 0x00}
	,			/* TSTBIST */
	{R367qam_CTRL_1, 0x00}
	,			/* CTRL_1 */
	{R367qam_CTRL_2, 0x03}
	,			/* CTRL_2 */
	{R367qam_IT_STATUS1, 0x2b}
	,			/* IT_STATUS1 */
	{R367qam_IT_STATUS2, 0x08}
	,			/* IT_STATUS2 */
	{R367qam_IT_EN1, 0x00}
	,			/* IT_EN1 */
	{R367qam_IT_EN2, 0x00}
	,			/* IT_EN2 */
	{R367qam_CTRL_STATUS, 0x04}
	,			/* CTRL_STATUS */
	{R367qam_TEST_CTL, 0x00}
	,			/* TEST_CTL */
	{R367qam_AGC_CTL, 0x73}
	,			/* AGC_CTL */
	{R367qam_AGC_IF_CFG, 0x50}
	,			/* AGC_IF_CFG */
	{R367qam_AGC_RF_CFG, 0x00}
	,			/* AGC_RF_CFG */
	{R367qam_AGC_PWM_CFG, 0x03}
	,			/* AGC_PWM_CFG */
	{R367qam_AGC_PWR_REF_L, 0x5a}
	,			/* AGC_PWR_REF_L */
	{R367qam_AGC_PWR_REF_H, 0x00}
	,			/* AGC_PWR_REF_H */
	{R367qam_AGC_RF_TH_L, 0xff}
	,			/* AGC_RF_TH_L */
	{R367qam_AGC_RF_TH_H, 0x07}
	,			/* AGC_RF_TH_H */
	{R367qam_AGC_IF_LTH_L, 0x00}
	,			/* AGC_IF_LTH_L */
	{R367qam_AGC_IF_LTH_H, 0x08}
	,			/* AGC_IF_LTH_H */
	{R367qam_AGC_IF_HTH_L, 0xff}
	,			/* AGC_IF_HTH_L */
	{R367qam_AGC_IF_HTH_H, 0x07}
	,			/* AGC_IF_HTH_H */
	{R367qam_AGC_PWR_RD_L, 0xa0}
	,			/* AGC_PWR_RD_L */
	{R367qam_AGC_PWR_RD_M, 0xe9}
	,			/* AGC_PWR_RD_M */
	{R367qam_AGC_PWR_RD_H, 0x03}
	,			/* AGC_PWR_RD_H */
	{R367qam_AGC_PWM_IFCMD_L, 0xe4}
	,			/* AGC_PWM_IFCMD_L */
	{R367qam_AGC_PWM_IFCMD_H, 0x00}
	,			/* AGC_PWM_IFCMD_H */
	{R367qam_AGC_PWM_RFCMD_L, 0xff}
	,			/* AGC_PWM_RFCMD_L */
	{R367qam_AGC_PWM_RFCMD_H, 0x07}
	,			/* AGC_PWM_RFCMD_H */
	{R367qam_IQDEM_CFG, 0x01}
	,			/* IQDEM_CFG */
	{R367qam_MIX_NCO_LL, 0x22}
	,			/* MIX_NCO_LL */
	{R367qam_MIX_NCO_HL, 0x96}
	,			/* MIX_NCO_HL */
	{R367qam_MIX_NCO_HH, 0x55}
	,			/* MIX_NCO_HH */
	{R367qam_SRC_NCO_LL, 0xff}
	,			/* SRC_NCO_LL */
	{R367qam_SRC_NCO_LH, 0x0c}
	,			/* SRC_NCO_LH */
	{R367qam_SRC_NCO_HL, 0xf5}
	,			/* SRC_NCO_HL */
	{R367qam_SRC_NCO_HH, 0x20}
	,			/* SRC_NCO_HH */
	{R367qam_IQDEM_GAIN_SRC_L, 0x06}
	,			/* IQDEM_GAIN_SRC_L */
	{R367qam_IQDEM_GAIN_SRC_H, 0x01}
	,			/* IQDEM_GAIN_SRC_H */
	{R367qam_IQDEM_DCRM_CFG_LL, 0xfe}
	,			/* IQDEM_DCRM_CFG_LL */
	{R367qam_IQDEM_DCRM_CFG_LH, 0xff}
	,			/* IQDEM_DCRM_CFG_LH */
	{R367qam_IQDEM_DCRM_CFG_HL, 0x0f}
	,			/* IQDEM_DCRM_CFG_HL */
	{R367qam_IQDEM_DCRM_CFG_HH, 0x00}
	,			/* IQDEM_DCRM_CFG_HH */
	{R367qam_IQDEM_ADJ_COEFF0, 0x34}
	,			/* IQDEM_ADJ_COEFF0 */
	{R367qam_IQDEM_ADJ_COEFF1, 0xae}
	,			/* IQDEM_ADJ_COEFF1 */
	{R367qam_IQDEM_ADJ_COEFF2, 0x46}
	,			/* IQDEM_ADJ_COEFF2 */
	{R367qam_IQDEM_ADJ_COEFF3, 0x77}
	,			/* IQDEM_ADJ_COEFF3 */
	{R367qam_IQDEM_ADJ_COEFF4, 0x96}
	,			/* IQDEM_ADJ_COEFF4 */
	{R367qam_IQDEM_ADJ_COEFF5, 0x69}
	,			/* IQDEM_ADJ_COEFF5 */
	{R367qam_IQDEM_ADJ_COEFF6, 0xc7}
	,			/* IQDEM_ADJ_COEFF6 */
	{R367qam_IQDEM_ADJ_COEFF7, 0x01}
	,			/* IQDEM_ADJ_COEFF7 */
	{R367qam_IQDEM_ADJ_EN, 0x04}
	,			/* IQDEM_ADJ_EN */
	{R367qam_IQDEM_ADJ_AGC_REF, 0x94}
	,			/* IQDEM_ADJ_AGC_REF */
	{R367qam_ALLPASSFILT1, 0xc9}
	,			/* ALLPASSFILT1 */
	{R367qam_ALLPASSFILT2, 0x2d}
	,			/* ALLPASSFILT2 */
	{R367qam_ALLPASSFILT3, 0xa3}
	,			/* ALLPASSFILT3 */
	{R367qam_ALLPASSFILT4, 0xfb}
	,			/* ALLPASSFILT4 */
	{R367qam_ALLPASSFILT5, 0xf6}
	,			/* ALLPASSFILT5 */
	{R367qam_ALLPASSFILT6, 0x45}
	,			/* ALLPASSFILT6 */
	{R367qam_ALLPASSFILT7, 0x6f}
	,			/* ALLPASSFILT7 */
	{R367qam_ALLPASSFILT8, 0x7e}
	,			/* ALLPASSFILT8 */
	{R367qam_ALLPASSFILT9, 0x05}
	,			/* ALLPASSFILT9 */
	{R367qam_ALLPASSFILT10, 0x0a}
	,			/* ALLPASSFILT10 */
	{R367qam_ALLPASSFILT11, 0x51}
	,			/* ALLPASSFILT11 */
	{R367qam_TRL_AGC_CFG, 0x20}
	,			/* TRL_AGC_CFG */
	{R367qam_TRL_LPF_CFG, 0x28}
	,			/* TRL_LPF_CFG */
	{R367qam_TRL_LPF_ACQ_GAIN, 0x44}
	,			/* TRL_LPF_ACQ_GAIN */
	{R367qam_TRL_LPF_TRK_GAIN, 0x22}
	,			/* TRL_LPF_TRK_GAIN */
	{R367qam_TRL_LPF_OUT_GAIN, 0x03}
	,			/* TRL_LPF_OUT_GAIN */
	{R367qam_TRL_LOCKDET_LTH, 0x04}
	,			/* TRL_LOCKDET_LTH */
	{R367qam_TRL_LOCKDET_HTH, 0x11}
	,			/* TRL_LOCKDET_HTH */
	{R367qam_TRL_LOCKDET_TRGVAL, 0x20}
	,			/* TRL_LOCKDET_TRGVAL */
	{R367qam_IQ_QAM, 0x01}
	,			/* IQ_QAM */
	{R367qam_FSM_STATE, 0xa0}
	,			/* FSM_STATE */
	{R367qam_FSM_CTL, 0x08}
	,			/* FSM_CTL */
	{R367qam_FSM_STS, 0x0c}
	,			/* FSM_STS */
	{R367qam_FSM_SNR0_HTH, 0x00}
	,			/* FSM_SNR0_HTH */
	{R367qam_FSM_SNR1_HTH, 0x00}
	,			/* FSM_SNR1_HTH */
	{R367qam_FSM_SNR2_HTH, 0x00}
	,			/* FSM_SNR2_HTH */
	{R367qam_FSM_SNR0_LTH, 0x00}
	,			/* FSM_SNR0_LTH */
	{R367qam_FSM_SNR1_LTH, 0x00}
	,			/* FSM_SNR1_LTH */
	{R367qam_FSM_EQA1_HTH, 0x00}
	,			/* FSM_EQA1_HTH */
	{R367qam_FSM_TEMPO, 0x32}
	,			/* FSM_TEMPO */
	{R367qam_FSM_CONFIG, 0x03}
	,			/* FSM_CONFIG */
	{R367qam_EQU_I_TESTTAP_L, 0x11}
	,			/* EQU_I_TESTTAP_L */
	{R367qam_EQU_I_TESTTAP_M, 0x00}
	,			/* EQU_I_TESTTAP_M */
	{R367qam_EQU_I_TESTTAP_H, 0x00}
	,			/* EQU_I_TESTTAP_H */
	{R367qam_EQU_TESTAP_CFG, 0x00}
	,			/* EQU_TESTAP_CFG */
	{R367qam_EQU_Q_TESTTAP_L, 0xff}
	,			/* EQU_Q_TESTTAP_L */
	{R367qam_EQU_Q_TESTTAP_M, 0x00}
	,			/* EQU_Q_TESTTAP_M */
	{R367qam_EQU_Q_TESTTAP_H, 0x00}
	,			/* EQU_Q_TESTTAP_H */
	{R367qam_EQU_TAP_CTRL, 0x00}
	,			/* EQU_TAP_CTRL */
	{R367qam_EQU_CTR_CRL_CONTROL_L, 0x11}
	,			/* EQU_CTR_CRL_CONTROL_L */
	{R367qam_EQU_CTR_CRL_CONTROL_H, 0x05}
	,			/* EQU_CTR_CRL_CONTROL_H */
	{R367qam_EQU_CTR_HIPOW_L, 0x00}
	,			/* EQU_CTR_HIPOW_L */
	{R367qam_EQU_CTR_HIPOW_H, 0x00}
	,			/* EQU_CTR_HIPOW_H */
	{R367qam_EQU_I_EQU_LO, 0xef}
	,			/* EQU_I_EQU_LO */
	{R367qam_EQU_I_EQU_HI, 0x00}
	,			/* EQU_I_EQU_HI */
	{R367qam_EQU_Q_EQU_LO, 0xee}
	,			/* EQU_Q_EQU_LO */
	{R367qam_EQU_Q_EQU_HI, 0x00}
	,			/* EQU_Q_EQU_HI */
	{R367qam_EQU_MAPPER, 0xc5}
	,			/* EQU_MAPPER */
	{R367qam_EQU_SWEEP_RATE, 0x80}
	,			/* EQU_SWEEP_RATE */
	{R367qam_EQU_SNR_LO, 0x64}
	,			/* EQU_SNR_LO */
	{R367qam_EQU_SNR_HI, 0x03}
	,			/* EQU_SNR_HI */
	{R367qam_EQU_GAMMA_LO, 0x00}
	,			/* EQU_GAMMA_LO */
	{R367qam_EQU_GAMMA_HI, 0x00}
	,			/* EQU_GAMMA_HI */
	{R367qam_EQU_ERR_GAIN, 0x36}
	,			/* EQU_ERR_GAIN */
	{R367qam_EQU_RADIUS, 0xaa}
	,			/* EQU_RADIUS */
	{R367qam_EQU_FFE_MAINTAP, 0x00}
	,			/* EQU_FFE_MAINTAP */
	{R367qam_EQU_FFE_LEAKAGE, 0x63}
	,			/* EQU_FFE_LEAKAGE */
	{R367qam_EQU_FFE_MAINTAP_POS, 0xdf}
	,			/* EQU_FFE_MAINTAP_POS */
	{R367qam_EQU_GAIN_WIDE, 0x88}
	,			/* EQU_GAIN_WIDE */
	{R367qam_EQU_GAIN_NARROW, 0x41}
	,			/* EQU_GAIN_NARROW */
	{R367qam_EQU_CTR_LPF_GAIN, 0xd1}
	,			/* EQU_CTR_LPF_GAIN */
	{R367qam_EQU_CRL_LPF_GAIN, 0xa7}
	,			/* EQU_CRL_LPF_GAIN */
	{R367qam_EQU_GLOBAL_GAIN, 0x06}
	,			/* EQU_GLOBAL_GAIN */
	{R367qam_EQU_CRL_LD_SEN, 0x85}
	,			/* EQU_CRL_LD_SEN */
	{R367qam_EQU_CRL_LD_VAL, 0xe2}
	,			/* EQU_CRL_LD_VAL */
	{R367qam_EQU_CRL_TFR, 0x20}
	,			/* EQU_CRL_TFR */
	{R367qam_EQU_CRL_BISTH_LO, 0x00}
	,			/* EQU_CRL_BISTH_LO */
	{R367qam_EQU_CRL_BISTH_HI, 0x00}
	,			/* EQU_CRL_BISTH_HI */
	{R367qam_EQU_SWEEP_RANGE_LO, 0x00}
	,			/* EQU_SWEEP_RANGE_LO */
	{R367qam_EQU_SWEEP_RANGE_HI, 0x00}
	,			/* EQU_SWEEP_RANGE_HI */
	{R367qam_EQU_CRL_LIMITER, 0x40}
	,			/* EQU_CRL_LIMITER */
	{R367qam_EQU_MODULUS_MAP, 0x90}
	,			/* EQU_MODULUS_MAP */
	{R367qam_EQU_PNT_GAIN, 0xa7}
	,			/* EQU_PNT_GAIN */
	{R367qam_FEC_AC_CTR_0, 0x16}
	,			/* FEC_AC_CTR_0 */
	{R367qam_FEC_AC_CTR_1, 0x0b}
	,			/* FEC_AC_CTR_1 */
	{R367qam_FEC_AC_CTR_2, 0x88}
	,			/* FEC_AC_CTR_2 */
	{R367qam_FEC_AC_CTR_3, 0x02}
	,			/* FEC_AC_CTR_3 */
	{R367qam_FEC_STATUS, 0x12}
	,			/* FEC_STATUS */
	{R367qam_RS_COUNTER_0, 0x7d}
	,			/* RS_COUNTER_0 */
	{R367qam_RS_COUNTER_1, 0xd0}
	,			/* RS_COUNTER_1 */
	{R367qam_RS_COUNTER_2, 0x19}
	,			/* RS_COUNTER_2 */
	{R367qam_RS_COUNTER_3, 0x0b}
	,			/* RS_COUNTER_3 */
	{R367qam_RS_COUNTER_4, 0xa3}
	,			/* RS_COUNTER_4 */
	{R367qam_RS_COUNTER_5, 0x00}
	,			/* RS_COUNTER_5 */
	{R367qam_BERT_0, 0x01}
	,			/* BERT_0 */
	{R367qam_BERT_1, 0x25}
	,			/* BERT_1 */
	{R367qam_BERT_2, 0x41}
	,			/* BERT_2 */
	{R367qam_BERT_3, 0x39}
	,			/* BERT_3 */
	{R367qam_OUTFORMAT_0, 0xc2}
	,			/* OUTFORMAT_0 */
	{R367qam_OUTFORMAT_1, 0x22}
	,			/* OUTFORMAT_1 */
	{R367qam_SMOOTHER_2, 0x28}
	,			/* SMOOTHER_2 */
	{R367qam_TSMF_CTRL_0, 0x01}
	,			/* TSMF_CTRL_0 */
	{R367qam_TSMF_CTRL_1, 0xc6}
	,			/* TSMF_CTRL_1 */
	{R367qam_TSMF_CTRL_3, 0x43}
	,			/* TSMF_CTRL_3 */
	{R367qam_TS_ON_ID_0, 0x00}
	,			/* TS_ON_ID_0 */
	{R367qam_TS_ON_ID_1, 0x00}
	,			/* TS_ON_ID_1 */
	{R367qam_TS_ON_ID_2, 0x00}
	,			/* TS_ON_ID_2 */
	{R367qam_TS_ON_ID_3, 0x00}
	,			/* TS_ON_ID_3 */
	{R367qam_RE_STATUS_0, 0x00}
	,			/* RE_STATUS_0 */
	{R367qam_RE_STATUS_1, 0x00}
	,			/* RE_STATUS_1 */
	{R367qam_RE_STATUS_2, 0x00}
	,			/* RE_STATUS_2 */
	{R367qam_RE_STATUS_3, 0x00}
	,			/* RE_STATUS_3 */
	{R367qam_TS_STATUS_0, 0x00}
	,			/* TS_STATUS_0 */
	{R367qam_TS_STATUS_1, 0x00}
	,			/* TS_STATUS_1 */
	{R367qam_TS_STATUS_2, 0xa0}
	,			/* TS_STATUS_2 */
	{R367qam_TS_STATUS_3, 0x00}
	,			/* TS_STATUS_3 */
	{R367qam_T_O_ID_0, 0x00}
	,			/* T_O_ID_0 */
	{R367qam_T_O_ID_1, 0x00}
	,			/* T_O_ID_1 */
	{R367qam_T_O_ID_2, 0x00}
	,			/* T_O_ID_2 */
	{R367qam_T_O_ID_3, 0x00}
	,			/* T_O_ID_3 */
};

STCHIP_Error_t STV0367qam_Init(Demod_InitParams_t *InitParams,
			       STCHIP_Handle_t *hChipHandle)
{
	STCHIP_Handle_t hChip;
	STCHIP_Error_t error;
	U16 i, j;

	/* Fill elements of external chip data structure */
	InitParams->Chip->NbRegs = STV0367qam_NBREGS;
	InitParams->Chip->NbFields = STV0367qam_NBFIELDS;
	InitParams->Chip->ChipMode = STCHIP_MODE_SUBADR_16;
	InitParams->Chip->RepeaterFn = NULL;
	InitParams->Chip->pData = NULL;

#ifdef HOST_PC
	(*hChipHandle) = ChipOpen(InitParams->Chip);
#endif
	hChip = (*hChipHandle);

	j = 1;
	if (hChip != NULL) {
		/*******************************
	    **   REGISTERS CONFIGURATION
	    **     ----------------------
	    ********************************/
		ChipUpdateDefaultValues(hChip, Def367qamVal);
		/*Read id register to check i2c error */
		ChipGetRegisters(hChip, R367qam_ID, 1);
		if (hChip->Error == CHIPERR_NO_ERROR) {
			for (i = 1; i <= STV0367qam_NBREGS; i++) {
				if (i != STV0367qam_NBREGS) {
					if (Def367qamVal[i].Addr ==
					    Def367qamVal[i - 1].Addr + 1) {
						j++;
					} else if (j == 1) {
						ChipSetOneRegister(hChip,
								  Def367qamVal
								  [i - 1].Addr,
								  Def367qamVal
								  [i -
								   1].Value);
					} else {
						ChipSetRegisters(hChip,
								Def367qamVal[i
									     -
									     j].
								Addr, j);
						j = 1;
					}
				} else {
					if (j == 1) {
						ChipSetOneRegister(hChip,
								   Def367qamVal
								   [i - 1].Addr,
								   Def367qamVal
								   [i -
								    1].Value);
					} else {
						ChipSetRegisters(hChip,
								Def367qamVal[i
									     -
									    j].
								 Addr, j);
						j = 1;
					}
				}
			}
		}
		error = hChip->Error;
	} else {
		error = CHIPERR_INVALID_HANDLE;
	}

	return error;
}
