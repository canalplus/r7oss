/**********************************************************************
Copyright (C) 2006-2009 NXP B.V.



This program is free software: you can redistribute it and/or modify

it under the terms of the GNU General Public License as published by

the Free Software Foundation, either version 3 of the License, or

(at your option) any later version.



This program is distributed in the hope that it will be useful,

but WITHOUT ANY WARRANTY; without even the implied warranty of

MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the

GNU General Public License for more details.



You should have received a copy of the GNU General Public License

along with this program.  If not, see <http://www.gnu.org/licenses/>.
*************************************************************************/
#ifndef _TMDD_TDA182I2_ADVANCED_H	/* ----------------- */
#define _TMDD_TDA182I2_ADVANCED_H

/* ----------------------------------------------------------------------- */
/* Standard include files: */
/* ----------------------------------------------------------------------- */
/*  */
/* ----------------------------------------------------------------------- */
/* Project include files: */
/* ----------------------------------------------------------------------- */
/*  */
#ifdef __cplusplus
extern "C"
{
#endif
/* ----------------------------------------------------------------------- */
/* Types and defines: */
/* ----------------------------------------------------------------------- */
/*  */
/* SW Error codes */
#define ddTDA182I2_ERR_BASE (CID_COMP_TUNER | CID_LAYER_BSL)
#define ddTDA182I2_ERR_COMP (CID_COMP_TUNER | CID_LAYER_BSL | \
						TM_ERR_COMP_UNIQUE_START)
#define ddTDA182I2_ERR_BAD_UNIT_NUMBER (ddTDA182I2_ERR_BASE + \
							TM_ERR_BAD_UNIT_NUMBER)
#define ddTDA182I2_ERR_NOT_INITIALIZED (ddTDA182I2_ERR_BASE + \
							TM_ERR_NOT_INITIALIZED)
#define ddTDA182I2_ERR_INIT_FAILED (ddTDA182I2_ERR_BASE + \
							TM_ERR_INIT_FAILED)
#define ddTDA182I2_ERR_BAD_PARAMETER (ddTDA182I2_ERR_BASE + \
							TM_ERR_BAD_PARAMETER)
#define ddTDA182I2_ERR_NOT_SUPPORTED (ddTDA182I2_ERR_BASE + \
							TM_ERR_NOT_SUPPORTED)
#define ddTDA182I2_ERR_HW_FAILED (ddTDA182I2_ERR_COMP + 0x0001)
#define ddTDA182I2_ERR_NOT_READY (ddTDA182I2_ERR_COMP + 0x0002)
#define ddTDA182I2_ERR_BAD_VERSION (ddTDA182I2_ERR_COMP + 0x0003)
typedef enum _tmddTDA182I2PowerState_t {
	tmddTDA182I2_PowerNormalMode,	/* Device normal mode */
	/* Device standby mode with LNA and Xtal Output and Synthe on */
	tmddTDA182I2_PowerStandbyWithLNAOnAndWithXtalOnAndWithSyntheOn,
	/* Device standby mode with LNA and Xtal Output */
	tmddTDA182I2_PowerStandbyWithLNAOnAndWithXtalOn,
	/* Device standby mode with Xtal Output */
	tmddTDA182I2_PowerStandbyWithXtalOn,
	tmddTDA182I2_PowerStandby,	/* Device standby mode */
	tmddTDA182I2_PowerMax
} tmddTDA182I2PowerState_t, *ptmddTDA182I2PowerState_t;

typedef enum _tmddTDA182I2_LPF_Gain_t {
	tmddTDA182I2_LPF_Gain_Unknown = 0,	/* LPF_Gain Unknown */
	tmddTDA182I2_LPF_Gain_Frozen,	/* LPF_Gain Frozen */
	tmddTDA182I2_LPF_Gain_Free	/* LPF_Gain Free */
} tmddTDA182I2_LPF_Gain_t, *ptmddTDA182I2_LPF_Gain_t;


tmErrorCode_t tmddTDA182I2Init(tmUnitSelect_t tUnit,/*I:Unit#*/
	tmbslFrontEndDependency_t *psSrvFunc	/*  I: setup parameters */);

tmErrorCode_t tmddTDA182I2DeInit(tmUnitSelect_t tUnit/*I:Unit#*/);

/* I: Receives SW Version */
tmErrorCode_t tmddTDA182I2GetSWVersion(ptmSWVersion_t pSWVersion);

tmErrorCode_t tmddTDA182I2Reset(tmUnitSelect_t tUnit/*I:Unit#*/);
tmErrorCode_t tmddTDA182I2SetLPF_Gain_Mode(tmUnitSelect_t tUnit,/* I: Unit no.*/
			   unsigned char uMode	/* I: Unknown/Free/Frozen */);

tmErrorCode_t tmddTDA182I2GetLPF_Gain_Mode(tmUnitSelect_t tUnit,/* I: Unit no.*/
		   unsigned char *puMode	/* O: Unknown/Free/Frozen */);

tmErrorCode_t tmddTDA182I2Write(tmUnitSelect_t tUnit,/*I:Unit#*/
		UInt32 uIndex,	/* I: Start index to write */
		UInt32 uNbBytes,/* I: Number of bytes to write */
		unsigned char *puBytes	/* I: Pointer on an array of bytes */);

tmErrorCode_t tmddTDA182I2Read(tmUnitSelect_t tUnit,/*I:Unit#*/
		UInt32 uIndex,	/* I: Start index to read */
		UInt32 uNbBytes,	/* I: Number of bytes to read */
		unsigned char *puBytes	/* I: Pointer on an array of bytes */);

tmErrorCode_t tmddTDA182I2GetPOR(tmUnitSelect_t tUnit,/*I:Unit#*/
	 unsigned char *puValue/* I: Address of the var to output item value*/);

tmErrorCode_t tmddTDA182I2GetLO_Lock(tmUnitSelect_t tUnit,/*I:Unit#*/
	unsigned char *puValue/* I: Address of the var to output item value*/);

tmErrorCode_t tmddTDA182I2GetMS(tmUnitSelect_t tUnit,/*I:Unit#*/
	unsigned char *puValue/* I: Address of the var to output item value*/);

tmErrorCode_t tmddTDA182I2GetIdentity(tmUnitSelect_t tUnit,/*I:Unit#*/
	UInt16 * puValue/* I: Address of the var to output item value*/);

tmErrorCode_t tmddTDA182I2GetMinorRevision(tmUnitSelect_t tUnit,/*I:Unit#*/
	unsigned char *puValue/* I: Address of the var to output item value*/);

tmErrorCode_t tmddTDA182I2GetMajorRevision(tmUnitSelect_t tUnit,/*I:Unit#*/
	unsigned char *puValue/* I: Address of the var to output item value*/);

tmErrorCode_t tmddTDA182I2GetTM_D(tmUnitSelect_t tUnit,/*I:Unit#*/
	unsigned char *puValue/* I: Address of the var to output item value*/);

tmErrorCode_t tmddTDA182I2SetTM_ON(tmUnitSelect_t tUnit,/*I:Unit#*/
		unsigned char uValue	/* I: Item value */);

tmErrorCode_t tmddTDA182I2GetTM_ON(tmUnitSelect_t tUnit,/*I:Unit#*/
	unsigned char *puValue/* I: Address of the var to output item value*/);

tmErrorCode_t tmddTDA182I2SetPowerState(tmUnitSelect_t tUnit,/*I:Unit#*/
	tmddTDA182I2PowerState_t powerState	/* I: Power state of device */);

tmErrorCode_t tmddTDA182I2GetPowerState(tmUnitSelect_t tUnit,/*I:Unit#*/
	ptmddTDA182I2PowerState_t pPowerState	/* O: Power state of device */);

tmErrorCode_t tmddTDA182I2GetPower_Level(tmUnitSelect_t tUnit,/*I:Unit#*/
	unsigned char *puValue/* I: Address of the var to output item value*/);

tmErrorCode_t tmddTDA182I2SetIRQ_Enable(tmUnitSelect_t tUnit,/*I:Unit#*/
		unsigned char uValue	/* I: Item value */);

tmErrorCode_t tmddTDA182I2GetIRQ_Enable(tmUnitSelect_t tUnit,/*I:Unit#*/
	unsigned char *puValue/* I: Address of the var to output item value*/);

tmErrorCode_t tmddTDA182I2SetXtalCal_Enable(tmUnitSelect_t tUnit,/*I:Unit#*/
		unsigned char uValue	/* I: Item value */);

tmErrorCode_t tmddTDA182I2GetXtalCal_Enable(tmUnitSelect_t tUnit,/*I:Unit#*/
	unsigned char *puValue/* I: Address of the var to output item value*/);

tmErrorCode_t tmddTDA182I2SetMSM_RSSI_Enable(tmUnitSelect_t tUnit,/*I:Unit#*/
		unsigned char uValue	/* I: Item value */);

tmErrorCode_t tmddTDA182I2GetMSM_RSSI_Enable(tmUnitSelect_t tUnit,/*I:Unit#*/
	unsigned char *puValue/* I: Address of the var to output item value*/);

tmErrorCode_t tmddTDA182I2SetMSM_LOCalc_Enable(tmUnitSelect_t tUnit,/*I:Unit#*/
		unsigned char uValue	/* I: Item value */);

tmErrorCode_t tmddTDA182I2GetMSM_LOCalc_Enable(tmUnitSelect_t tUnit,/*I:Unit#*/
	unsigned char *puValue/* I: Address of the var to output item value*/);

tmErrorCode_t tmddTDA182I2SetMSM_RFCAL_Enable(tmUnitSelect_t tUnit,/*I:Unit#*/
		unsigned char uValue	/* I: Item value */);

tmErrorCode_t tmddTDA182I2GetMSM_RFCAL_Enable(tmUnitSelect_t tUnit,/*I:Unit#*/
	unsigned char *puValue/* I: Address of the var to output item value*/);

tmErrorCode_t tmddTDA182I2SetMSM_IRCAL_Enable(tmUnitSelect_t tUnit,/*I:Unit#*/
		unsigned char uValue	/* I: Item value */);

tmErrorCode_t tmddTDA182I2GetMSM_IRCAL_Enable(tmUnitSelect_t tUnit,/*I:Unit#*/
	unsigned char *puValue/* I: Address of the var to output item value*/);

tmErrorCode_t tmddTDA182I2SetMSM_RCCal_Enable(tmUnitSelect_t tUnit,/*I:Unit#*/
		unsigned char uValue	/* I: Item value */);

tmErrorCode_t tmddTDA182I2GetMSM_RCCal_Enable(tmUnitSelect_t tUnit,/*I:Unit#*/
	unsigned char *puValue/* I: Address of the var to output item value*/);

tmErrorCode_t tmddTDA182I2SetIRQ_Clear(tmUnitSelect_t tUnit/*I:Unit#*/);

tmErrorCode_t tmddTDA182I2SetXtalCal_Clear(tmUnitSelect_t tUnit,/*I:Unit#*/
		unsigned char uValue	/* I: Item value */);

tmErrorCode_t tmddTDA182I2GetXtalCal_Clear(tmUnitSelect_t tUnit,/*I:Unit#*/
	unsigned char *puValue/* I: Address of the var to output item value*/);

tmErrorCode_t tmddTDA182I2SetMSM_RSSI_Clear(tmUnitSelect_t tUnit,/*I:Unit#*/
		unsigned char uValue	/* I: Item value */);

tmErrorCode_t tmddTDA182I2GetMSM_RSSI_Clear(tmUnitSelect_t tUnit,/*I:Unit#*/
	unsigned char *puValue/* I: Address of the var to output item value*/);

tmErrorCode_t tmddTDA182I2SetMSM_LOCalc_Clear(tmUnitSelect_t tUnit,/*I:Unit#*/
		unsigned char uValue	/* I: Item value */);

tmErrorCode_t tmddTDA182I2GetMSM_LOCalc_Clear(tmUnitSelect_t tUnit,/*I:Unit#*/
	unsigned char *puValue/* I: Address of the var to output item value*/);

tmErrorCode_t tmddTDA182I2SetMSM_RFCal_Clear(tmUnitSelect_t tUnit,/*I:Unit#*/
		unsigned char uValue	/* I: Item value */);

tmErrorCode_t tmddTDA182I2GetMSM_RFCal_Clear(tmUnitSelect_t tUnit,/*I:Unit#*/
	unsigned char *puValue/* I: Address of the var to output item value*/);

tmErrorCode_t tmddTDA182I2SetMSM_IRCAL_Clear(tmUnitSelect_t tUnit,/*I:Unit#*/
		unsigned char uValue	/* I: Item value */);

tmErrorCode_t tmddTDA182I2GetMSM_IRCAL_Clear(tmUnitSelect_t tUnit,/*I:Unit#*/
	unsigned char *puValue/* I: Address of the var to output item value*/);

tmErrorCode_t tmddTDA182I2SetMSM_RCCal_Clear(tmUnitSelect_t tUnit,/*I:Unit#*/
		unsigned char uValue	/* I: Item value */);

tmErrorCode_t tmddTDA182I2GetMSM_RCCal_Clear(tmUnitSelect_t tUnit,/*I:Unit#*/
	unsigned char *puValue/* I: Address of the var to output item value*/);

tmErrorCode_t tmddTDA182I2SetIRQ_Set(tmUnitSelect_t tUnit,/*I:Unit#*/
		unsigned char uValue	/* I: Item value */);

tmErrorCode_t tmddTDA182I2GetIRQ_Set(tmUnitSelect_t tUnit,/*I:Unit#*/
	unsigned char *puValue/* I: Address of the var to output item value*/);

tmErrorCode_t tmddTDA182I2SetXtalCal_Set(tmUnitSelect_t tUnit,/*I:Unit#*/
		unsigned char uValue	/* I: Item value */);

tmErrorCode_t tmddTDA182I2GetXtalCal_Set(tmUnitSelect_t tUnit,/*I:Unit#*/
	unsigned char *puValue/* I: Address of the var to output item value*/);

tmErrorCode_t tmddTDA182I2SetMSM_RSSI_Set(tmUnitSelect_t tUnit,/*I:Unit#*/
		unsigned char uValue	/* I: Item value */);

tmErrorCode_t tmddTDA182I2GetMSM_RSSI_Set(tmUnitSelect_t tUnit,/*I:Unit#*/
	unsigned char *puValue/* I: Address of the var to output item value*/);

tmErrorCode_t tmddTDA182I2SetMSM_LOCalc_Set(tmUnitSelect_t tUnit,/*I:Unit#*/
		unsigned char uValue	/* I: Item value */);

tmErrorCode_t tmddTDA182I2GetMSM_LOCalc_Set(tmUnitSelect_t tUnit,/*I:Unit#*/
	unsigned char *puValue/* I: Address of the var to output item value*/);

tmErrorCode_t tmddTDA182I2SetMSM_RFCal_Set(tmUnitSelect_t tUnit,/*I:Unit#*/
		unsigned char uValue	/* I: Item value */);

tmErrorCode_t tmddTDA182I2GetMSM_RFCal_Set(tmUnitSelect_t tUnit,/*I:Unit#*/
	unsigned char *puValue/* I: Address of the var to output item value*/);

tmErrorCode_t tmddTDA182I2SetMSM_IRCAL_Set(tmUnitSelect_t tUnit,/*I:Unit#*/
		unsigned char uValue	/* I: Item value */);

tmErrorCode_t tmddTDA182I2GetMSM_IRCAL_Set(tmUnitSelect_t tUnit,/*I:Unit#*/
	unsigned char *puValue/* I: Address of the var to output item value*/);

tmErrorCode_t tmddTDA182I2SetMSM_RCCal_Set(tmUnitSelect_t tUnit,/*I:Unit#*/
		unsigned char uValue	/* I: Item value */);

tmErrorCode_t tmddTDA182I2GetMSM_RCCal_Set(tmUnitSelect_t tUnit,/*I:Unit#*/
	unsigned char *puValue/* I: Address of the var to output item value*/);

tmErrorCode_t tmddTDA182I2GetIRQ_status(tmUnitSelect_t tUnit,/*I:Unit#*/
	unsigned char *puValue/* I: Address of the var to output item value*/);

tmErrorCode_t tmddTDA182I2GetMSM_XtalCal_End(tmUnitSelect_t tUnit,/*I:Unit#*/
	unsigned char *puValue/* I: Address of the var to output item value*/);

tmErrorCode_t tmddTDA182I2GetMSM_RSSI_End(tmUnitSelect_t tUnit,/*I:Unit#*/
	unsigned char *puValue/* I: Address of the var to output item value*/);

tmErrorCode_t tmddTDA182I2GetMSM_LOCalc_End(tmUnitSelect_t tUnit,/*I:Unit#*/
	unsigned char *puValue/* I: Address of the var to output item value*/);

tmErrorCode_t tmddTDA182I2GetMSM_RFCal_End(tmUnitSelect_t tUnit,/*I:Unit#*/
	unsigned char *puValue/* I: Address of the var to output item value*/);

tmErrorCode_t tmddTDA182I2GetMSM_IRCAL_End(tmUnitSelect_t tUnit,/*I:Unit#*/
	unsigned char *puValue/* I: Address of the var to output item value*/);

tmErrorCode_t tmddTDA182I2GetMSM_RCCal_End(tmUnitSelect_t tUnit,/*I:Unit#*/
	unsigned char *puValue/* I: Address of the var to output item value*/);

tmErrorCode_t tmddTDA182I2SetLT_Enable(tmUnitSelect_t tUnit,/*I:Unit#*/
		unsigned char uValue	/* I: Item value */);

tmErrorCode_t tmddTDA182I2GetLT_Enable(tmUnitSelect_t tUnit,/*I:Unit#*/
	unsigned char *puValue/* I: Address of the var to output item value*/);

tmErrorCode_t tmddTDA182I2SetAGC1_6_15dB(tmUnitSelect_t tUnit,/*I:Unit#*/
		unsigned char uValue	/* I: Item value */);

tmErrorCode_t tmddTDA182I2GetAGC1_6_15dB(tmUnitSelect_t tUnit,/*I:Unit#*/
	unsigned char *puValue/* I: Address of the var to output item value*/);

tmErrorCode_t tmddTDA182I2SetAGC1_TOP(tmUnitSelect_t tUnit,/*I:Unit#*/
		unsigned char uValue	/* I: Item value */);

tmErrorCode_t tmddTDA182I2GetAGC1_TOP(tmUnitSelect_t tUnit,/*I:Unit#*/
	unsigned char *puValue/* I: Address of the var to output item value*/);

tmErrorCode_t tmddTDA182I2SetAGC2_TOP(tmUnitSelect_t tUnit,/*I:Unit#*/
		unsigned char uValue	/* I: Item value */);

tmErrorCode_t tmddTDA182I2GetAGC2_TOP(tmUnitSelect_t tUnit,/*I:Unit#*/
	unsigned char *puValue/* I: Address of the var to output item value*/);

tmErrorCode_t tmddTDA182I2SetAGCs_Up_Step_assym(tmUnitSelect_t tUnit,/*I:Unit#*/
		unsigned char uValue	/* I: Item value */);

tmErrorCode_t tmddTDA182I2GetAGCs_Up_Step_assym(tmUnitSelect_t tUnit,/*I:Unit#*/
	unsigned char *puValue/* I: Address of the var to output item value*/);

tmErrorCode_t tmddTDA182I2SetAGCs_Up_Step(tmUnitSelect_t tUnit,/*I:Unit#*/
		unsigned char uValue	/* I: Item value */);

tmErrorCode_t tmddTDA182I2GetAGCs_Up_Step(tmUnitSelect_t tUnit,/*I:Unit#*/
	unsigned char *puValue/* I: Address of the var to output item value*/);

tmErrorCode_t
tmddTDA182I2SetPulse_Shaper_Disable(tmUnitSelect_t tUnit,/*I:Unit#*/
		unsigned char uValue	/* I: Item value */);

tmErrorCode_t
tmddTDA182I2GetPulse_Shaper_Disable(tmUnitSelect_t tUnit,/*I:Unit#*/
	unsigned char *puValue/* I: Address of the var to output item value*/);

tmErrorCode_t tmddTDA182I2SetAGCK_Step(tmUnitSelect_t tUnit,/*I:Unit#*/
		unsigned char uValue	/* I: Item value */);

tmErrorCode_t tmddTDA182I2GetAGCK_Step(tmUnitSelect_t tUnit,/*I:Unit#*/
	unsigned char *puValue/* I: Address of the var to output item value*/);

tmErrorCode_t tmddTDA182I2SetAGCK_Mode(tmUnitSelect_t tUnit,/*I:Unit#*/
		unsigned char uValue	/* I: Item value */);

tmErrorCode_t tmddTDA182I2GetAGCK_Mode(tmUnitSelect_t tUnit,/*I:Unit#*/
	unsigned char *puValue/* I: Address of the var to output item value*/);

tmErrorCode_t tmddTDA182I2SetPD_RFAGC_Adapt(tmUnitSelect_t tUnit,/*I:Unit#*/
		unsigned char uValue	/* I: Item value */);

tmErrorCode_t tmddTDA182I2GetPD_RFAGC_Adapt(tmUnitSelect_t tUnit,/*I:Unit#*/
	unsigned char *puValue/* I: Address of the var to output item value*/);

tmErrorCode_t tmddTDA182I2SetRFAGC_Adapt_TOP(tmUnitSelect_t tUnit,/*I:Unit#*/
		unsigned char uValue	/* I: Item value */);

tmErrorCode_t tmddTDA182I2GetRFAGC_Adapt_TOP(tmUnitSelect_t tUnit,/*I:Unit#*/
	unsigned char *puValue/* I: Address of the var to output item value*/);

tmErrorCode_t tmddTDA182I2SetRFAGC_Low_BW(tmUnitSelect_t tUnit,/*I:Unit#*/
		unsigned char uValue	/* I: Item value */);

tmErrorCode_t tmddTDA182I2GetRFAGC_Low_BW(tmUnitSelect_t tUnit,/*I:Unit#*/
	unsigned char *puValue/* I: Address of the var to output item value*/);

tmErrorCode_t tmddTDA182I2SetRF_Atten_3dB(tmUnitSelect_t tUnit,/*I:Unit#*/
		unsigned char uValue	/* I: Item value */);

tmErrorCode_t tmddTDA182I2GetRF_Atten_3dB(tmUnitSelect_t tUnit,/*I:Unit#*/
	unsigned char *puValue/* I: Address of the var to output item value*/);

tmErrorCode_t tmddTDA182I2SetRFAGC_Top(tmUnitSelect_t tUnit,/*I:Unit#*/
		unsigned char uValue	/* I: Item value */);

tmErrorCode_t tmddTDA182I2GetRFAGC_Top(tmUnitSelect_t tUnit,/*I:Unit#*/
	unsigned char *puValue/* I: Address of the var to output item value*/);

tmErrorCode_t tmddTDA182I2SetIR_Mixer_Top(tmUnitSelect_t tUnit,/*I:Unit#*/
		unsigned char uValue	/* I: Item value */);

tmErrorCode_t tmddTDA182I2GetIR_Mixer_Top(tmUnitSelect_t tUnit,/*I:Unit#*/
	unsigned char *puValue/* I: Address of the var to output item value*/);

tmErrorCode_t tmddTDA182I2SetAGCs_Do_Step_assym(tmUnitSelect_t tUnit,/*I:Unit#*/
		unsigned char uValue	/* I: Item value */);

tmErrorCode_t tmddTDA182I2GetAGCs_Do_Step_assym(tmUnitSelect_t tUnit,/*I:Unit#*/
	unsigned char *puValue/* I: Address of the var to output item value*/);

tmErrorCode_t tmddTDA182I2SetAGC5_Ana(tmUnitSelect_t tUnit,/*I:Unit#*/
		unsigned char uValue	/* I: Item value */);

tmErrorCode_t tmddTDA182I2GetAGC5_Ana(tmUnitSelect_t tUnit,/*I:Unit#*/
	unsigned char *puValue/* I: Address of the var to output item value*/);

tmErrorCode_t tmddTDA182I2SetAGC5_TOP(tmUnitSelect_t tUnit,/*I:Unit#*/
		unsigned char uValue	/* I: Item value */);

tmErrorCode_t tmddTDA182I2GetAGC5_TOP(tmUnitSelect_t tUnit,/*I:Unit#*/
	unsigned char *puValue/* I: Address of the var to output item value*/);

tmErrorCode_t tmddTDA182I2SetIF_Level(tmUnitSelect_t tUnit,/*I:Unit#*/
		unsigned char uValue	/* I: Item value */);

tmErrorCode_t tmddTDA182I2GetIF_Level(tmUnitSelect_t tUnit,/*I:Unit#*/
	unsigned char *puValue/* I: Address of the var to output item value*/);

tmErrorCode_t tmddTDA182I2SetIF_HP_Fc(tmUnitSelect_t tUnit,/*I:Unit#*/
		unsigned char uValue	/* I: Item value */);

tmErrorCode_t tmddTDA182I2GetIF_HP_Fc(tmUnitSelect_t tUnit,/*I:Unit#*/
	unsigned char *puValue/* I: Address of the var to output item value*/);

tmErrorCode_t tmddTDA182I2SetIF_ATSC_Notch(tmUnitSelect_t tUnit,/*I:Unit#*/
		unsigned char uValue	/* I: Item value */);

tmErrorCode_t tmddTDA182I2GetIF_ATSC_Notch(tmUnitSelect_t tUnit,/*I:Unit#*/
	unsigned char *puValue/* I: Address of the var to output item value*/);

tmErrorCode_t tmddTDA182I2SetLP_FC_Offset(tmUnitSelect_t tUnit,/*I:Unit#*/
		unsigned char uValue	/* I: Item value */);

tmErrorCode_t tmddTDA182I2GetLP_FC_Offset(tmUnitSelect_t tUnit,/*I:Unit#*/
	unsigned char *puValue/* I: Address of the var to output item value*/);

tmErrorCode_t tmddTDA182I2SetLP_FC(tmUnitSelect_t tUnit,/*I:Unit#*/
		unsigned char uValue	/* I: Item value */);

tmErrorCode_t tmddTDA182I2GetLP_FC(tmUnitSelect_t tUnit,/*I:Unit#*/
	unsigned char *puValue/* I: Address of the var to output item value*/);

tmErrorCode_t tmddTDA182I2SetI2C_Clock_Mode(tmUnitSelect_t tUnit,/*I:Unit#*/
		unsigned char uValue	/* I: Item value */);

tmErrorCode_t tmddTDA182I2GetI2C_Clock_Mode(tmUnitSelect_t tUnit,/*I:Unit#*/
	unsigned char *puValue/* I: Address of the var to output item value*/);

tmErrorCode_t tmddTDA182I2SetDigital_Clock_Mode(tmUnitSelect_t tUnit,/*I:Unit#*/
		unsigned char uValue	/* I: Item value */);

tmErrorCode_t tmddTDA182I2GetDigital_Clock_Mode(tmUnitSelect_t tUnit,/*I:Unit#*/
	unsigned char *puValue/* I: Address of the var to output item value*/);

tmErrorCode_t tmddTDA182I2SetXtalOsc_AnaReg_En(tmUnitSelect_t tUnit,/*I:Unit#*/
		unsigned char uValue	/* I: Item value */);

tmErrorCode_t tmddTDA182I2GetXtalOsc_AnaReg_En(tmUnitSelect_t tUnit,/*I:Unit#*/
	unsigned char *puValue/* I: Address of the var to output item value*/);

tmErrorCode_t tmddTDA182I2SetXTout(tmUnitSelect_t tUnit,/*I:Unit#*/
		unsigned char uValue	/* I: Item value */);

tmErrorCode_t tmddTDA182I2GetXTout(tmUnitSelect_t tUnit,/*I:Unit#*/
	unsigned char *puValue/* I: Address of the var to output item value*/);

tmErrorCode_t tmddTDA182I2SetIF_Freq(tmUnitSelect_t tUnit,/*I:Unit#*/
		UInt32 uValue	/* I: Item value */);

tmErrorCode_t tmddTDA182I2GetIF_Freq(tmUnitSelect_t tUnit,/*I:Unit#*/
	UInt32 * puValue/* I: Address of the var to output item value*/);

tmErrorCode_t tmddTDA182I2SetRF_Freq(tmUnitSelect_t tUnit,/*I:Unit#*/
		UInt32 uValue	/* I: Item value */);

tmErrorCode_t tmddTDA182I2GetRF_Freq(tmUnitSelect_t tUnit,/*I:Unit#*/
	UInt32 * puValue/* I: Address of the var to output item value*/);

tmErrorCode_t tmddTDA182I2SetRSSI_Meas(tmUnitSelect_t tUnit,/*I:Unit#*/
		unsigned char uValue	/* I: Item value */);

tmErrorCode_t tmddTDA182I2GetRSSI_Meas(tmUnitSelect_t tUnit,/*I:Unit#*/
	unsigned char *puValue/* I: Address of the var to output item value*/);

tmErrorCode_t tmddTDA182I2SetRF_CAL_AV(tmUnitSelect_t tUnit,/*I:Unit#*/
		unsigned char uValue	/* I: Item value */);

tmErrorCode_t tmddTDA182I2GetRF_CAL_AV(tmUnitSelect_t tUnit,/*I:Unit#*/
	unsigned char *puValue/* I: Address of the var to output item value*/);

tmErrorCode_t tmddTDA182I2SetRF_CAL(tmUnitSelect_t tUnit,/*I:Unit#*/
		unsigned char uValue	/* I: Item value */);

tmErrorCode_t tmddTDA182I2GetRF_CAL(tmUnitSelect_t tUnit,/*I:Unit#*/
	unsigned char *puValue/* I: Address of the var to output item value*/);

tmErrorCode_t tmddTDA182I2SetIR_CAL_Loop(tmUnitSelect_t tUnit,/*I:Unit#*/
		unsigned char uValue	/* I: Item value */);

tmErrorCode_t tmddTDA182I2GetIR_CAL_Loop(tmUnitSelect_t tUnit,/*I:Unit#*/
	unsigned char *puValue/* I: Address of the var to output item value*/);

tmErrorCode_t tmddTDA182I2SetIR_Cal_Image(tmUnitSelect_t tUnit,/*I:Unit#*/
		unsigned char uValue	/* I: Item value */);

tmErrorCode_t tmddTDA182I2GetIR_Cal_Image(tmUnitSelect_t tUnit,/*I:Unit#*/
	unsigned char *puValue/* I: Address of the var to output item value*/);

tmErrorCode_t tmddTDA182I2SetIR_CAL_Wanted(tmUnitSelect_t tUnit,/*I:Unit#*/
		unsigned char uValue	/* I: Item value */);

tmErrorCode_t tmddTDA182I2GetIR_CAL_Wanted(tmUnitSelect_t tUnit,/*I:Unit#*/
	unsigned char *puValue/* I: Address of the var to output item value*/);

tmErrorCode_t tmddTDA182I2SetRC_Cal(tmUnitSelect_t tUnit,/*I:Unit#*/
		unsigned char uValue	/* I: Item value */);

tmErrorCode_t tmddTDA182I2GetRC_Cal(tmUnitSelect_t tUnit,/*I:Unit#*/
	unsigned char *puValue/* I: Address of the var to output item value*/);

tmErrorCode_t tmddTDA182I2SetCalc_PLL(tmUnitSelect_t tUnit,/*I:Unit#*/
		unsigned char uValue	/* I: Item value */);

tmErrorCode_t tmddTDA182I2GetCalc_PLL(tmUnitSelect_t tUnit,/*I:Unit#*/
	unsigned char *puValue/* I: Address of the var to output item value*/);

tmErrorCode_t tmddTDA182I2SetXtalCal_Launch(tmUnitSelect_t tUnit/*I:Unit#*/);

tmErrorCode_t tmddTDA182I2GetXtalCal_Launch(tmUnitSelect_t tUnit,/*I:Unit#*/
	unsigned char *puValue/* I: Address of the var to output item value*/);

tmErrorCode_t tmddTDA182I2SetMSM_Launch(tmUnitSelect_t tUnit/*I:Unit#*/);

tmErrorCode_t tmddTDA182I2GetMSM_Launch(tmUnitSelect_t tUnit,/*I:Unit#*/
	unsigned char *puValue/* I: Address of the var to output item value*/);

tmErrorCode_t tmddTDA182I2SetPSM_AGC1(tmUnitSelect_t tUnit,/*I:Unit#*/
		unsigned char uValue	/* I: Item value */);

tmErrorCode_t tmddTDA182I2GetPSM_AGC1(tmUnitSelect_t tUnit,/*I:Unit#*/
	unsigned char *puValue/* I: Address of the var to output item value*/);

tmErrorCode_t tmddTDA182I2SetPSM_StoB(tmUnitSelect_t tUnit,/*I:Unit#*/
		unsigned char uValue	/* I: Item value */);

tmErrorCode_t tmddTDA182I2GetPSM_StoB(tmUnitSelect_t tUnit,/*I:Unit#*/
	unsigned char *puValue/* I: Address of the var to output item value*/);

tmErrorCode_t tmddTDA182I2SetPSMRFpoly(tmUnitSelect_t tUnit,/*I:Unit#*/
		unsigned char uValue	/* I: Item value */);

tmErrorCode_t tmddTDA182I2GetPSMRFpoly(tmUnitSelect_t tUnit,/*I:Unit#*/
	unsigned char *puValue/* I: Address of the var to output item value*/);

tmErrorCode_t tmddTDA182I2SetPSM_Mixer(tmUnitSelect_t tUnit,/*I:Unit#*/
		unsigned char uValue	/* I: Item value */);

tmErrorCode_t tmddTDA182I2GetPSM_Mixer(tmUnitSelect_t tUnit,/*I:Unit#*/
	unsigned char *puValue/* I: Address of the var to output item value*/);

tmErrorCode_t tmddTDA182I2SetPSM_Ifpoly(tmUnitSelect_t tUnit,/*I:Unit#*/
		unsigned char uValue	/* I: Item value */);

tmErrorCode_t tmddTDA182I2GetPSM_Ifpoly(tmUnitSelect_t tUnit,/*I:Unit#*/
	unsigned char *puValue/* I: Address of the var to output item value*/);

tmErrorCode_t tmddTDA182I2SetPSM_Lodriver(tmUnitSelect_t tUnit,/*I:Unit#*/
		unsigned char uValue	/* I: Item value */);

tmErrorCode_t tmddTDA182I2GetPSM_Lodriver(tmUnitSelect_t tUnit,/*I:Unit#*/
	unsigned char *puValue/* I: Address of the var to output item value*/);

tmErrorCode_t tmddTDA182I2SetDCC_Bypass(tmUnitSelect_t tUnit,/*I:Unit#*/
		unsigned char uValue	/* I: Item value */);

tmErrorCode_t tmddTDA182I2GetDCC_Bypass(tmUnitSelect_t tUnit,/*I:Unit#*/
	unsigned char *puValue/* I: Address of the var to output item value*/);

tmErrorCode_t tmddTDA182I2SetDCC_Slow(tmUnitSelect_t tUnit,/*I:Unit#*/
		unsigned char uValue	/* I: Item value */);

tmErrorCode_t tmddTDA182I2GetDCC_Slow(tmUnitSelect_t tUnit,/*I:Unit#*/
	unsigned char *puValue/* I: Address of the var to output item value*/);

tmErrorCode_t tmddTDA182I2SetDCC_psm(tmUnitSelect_t tUnit,/*I:Unit#*/
		unsigned char uValue	/* I: Item value */);

tmErrorCode_t tmddTDA182I2GetDCC_psm(tmUnitSelect_t tUnit,/*I:Unit#*/
	unsigned char *puValue/* I: Address of the var to output item value*/);

tmErrorCode_t tmddTDA182I2SetIR_Loop(tmUnitSelect_t tUnit,/*I:Unit#*/
		unsigned char uValue	/* I: Item value */);

tmErrorCode_t tmddTDA182I2GetIR_Loop(tmUnitSelect_t tUnit,/*I:Unit#*/
	unsigned char *puValue/* I: Address of the var to output item value*/);

tmErrorCode_t tmddTDA182I2SetIR_Target(tmUnitSelect_t tUnit,/*I:Unit#*/
		unsigned char uValue	/* I: Item value */);

tmErrorCode_t tmddTDA182I2GetIR_Target(tmUnitSelect_t tUnit,/*I:Unit#*/
	unsigned char *puValue/* I: Address of the var to output item value*/);

tmErrorCode_t tmddTDA182I2SetIR_GStep(tmUnitSelect_t tUnit,/*I:Unit#*/
		unsigned char uValue	/* I: Item value */);

tmErrorCode_t tmddTDA182I2GetIR_GStep(tmUnitSelect_t tUnit,/*I:Unit#*/
	unsigned char *puValue/* I: Address of the var to output item value*/);

tmErrorCode_t tmddTDA182I2SetIR_Corr_Boost(tmUnitSelect_t tUnit,/*I:Unit#*/
		unsigned char uValue	/* I: Item value */);

tmErrorCode_t tmddTDA182I2GetIR_Corr_Boost(tmUnitSelect_t tUnit,/*I:Unit#*/
	unsigned char *puValue/* I: Address of the var to output item value*/);

tmErrorCode_t tmddTDA182I2SetIR_FreqLow_Sel(tmUnitSelect_t tUnit,/*I:Unit#*/
		unsigned char uValue	/* I: Item value */);

tmErrorCode_t tmddTDA182I2GetIR_FreqLow_Sel(tmUnitSelect_t tUnit,/*I:Unit#*/
	unsigned char *puValue/* I: Address of the var to output item value*/);

tmErrorCode_t tmddTDA182I2SetIR_mode_ram_store(tmUnitSelect_t tUnit,/*I:Unit#*/
		unsigned char uValue	/* I: Item value */);

tmErrorCode_t tmddTDA182I2GetIR_mode_ram_store(tmUnitSelect_t tUnit,/*I:Unit#*/
	unsigned char *puValue/* I: Address of the var to output item value*/);

tmErrorCode_t tmddTDA182I2SetIR_FreqLow(tmUnitSelect_t tUnit,/*I:Unit#*/
		unsigned char uValue	/* I: Item value */);

tmErrorCode_t tmddTDA182I2GetIR_FreqLow(tmUnitSelect_t tUnit,/*I:Unit#*/
	unsigned char *puValue/* I: Address of the var to output item value*/);

tmErrorCode_t tmddTDA182I2SetIR_FreqMid(tmUnitSelect_t tUnit,/*I:Unit#*/
		unsigned char uValue	/* I: Item value */);

tmErrorCode_t tmddTDA182I2GetIR_FreqMid(tmUnitSelect_t tUnit,/*I:Unit#*/
	unsigned char *puValue/* I: Address of the var to output item value*/);

tmErrorCode_t tmddTDA182I2SetCoarse_IR_FreqHigh(tmUnitSelect_t tUnit,/*I:Unit#*/
		unsigned char uValue	/* I: Item value */);

tmErrorCode_t tmddTDA182I2GetCoarse_IR_FreqHigh(tmUnitSelect_t tUnit,/*I:Unit#*/
	unsigned char *puValue/* I: Address of the var to output item value*/);

tmErrorCode_t tmddTDA182I2SetIR_FreqHigh(tmUnitSelect_t tUnit,/*I:Unit#*/
		unsigned char uValue	/* I: Item value */);

tmErrorCode_t tmddTDA182I2GetIR_FreqHigh(tmUnitSelect_t tUnit,/*I:Unit#*/
	unsigned char *puValue/* I: Address of the var to output item value*/);


tmErrorCode_t tmddTDA182I2SetPD_Vsync_Mgt(tmUnitSelect_t tUnit,/*I:Unit#*/
		unsigned char uValue	/* I: Item value */);

tmErrorCode_t tmddTDA182I2GetPD_Vsync_Mgt(tmUnitSelect_t tUnit,/*I:Unit#*/
	unsigned char *puValue/* I: Address of the var to output item value*/);

tmErrorCode_t tmddTDA182I2SetPD_Ovld(tmUnitSelect_t tUnit,/*I:Unit#*/
		unsigned char uValue	/* I: Item value */);

tmErrorCode_t tmddTDA182I2GetPD_Ovld(tmUnitSelect_t tUnit,/*I:Unit#*/
	unsigned char *puValue/* I: Address of the var to output item value*/);

tmErrorCode_t tmddTDA182I2SetPD_Udld(tmUnitSelect_t tUnit,/*I:Unit#*/
		unsigned char uValue	/* I: Item value */);

tmErrorCode_t tmddTDA182I2GetPD_Udld(tmUnitSelect_t tUnit,/*I:Unit#*/
	unsigned char *puValue/* I: Address of the var to output item value*/);

tmErrorCode_t tmddTDA182I2SetAGC_Ovld_TOP(tmUnitSelect_t tUnit,/*I:Unit#*/
		unsigned char uValue	/* I: Item value */);

tmErrorCode_t tmddTDA182I2GetAGC_Ovld_TOP(tmUnitSelect_t tUnit,/*I:Unit#*/
	unsigned char *puValue/* I: Address of the var to output item value*/);

tmErrorCode_t tmddTDA182I2SetAGC_Ovld_Timer(tmUnitSelect_t tUnit,/*I:Unit#*/
		unsigned char uValue	/* I: Item value */);

tmErrorCode_t tmddTDA182I2GetAGC_Ovld_Timer(tmUnitSelect_t tUnit,/*I:Unit#*/
	unsigned char *puValue/* I: Address of the var to output item value*/);

tmErrorCode_t tmddTDA182I2SetIR_Mixer_loop_off(tmUnitSelect_t tUnit,/*I:Unit#*/
		unsigned char uValue	/* I: Item value */);

tmErrorCode_t tmddTDA182I2GetIR_Mixer_loop_off(tmUnitSelect_t tUnit,/*I:Unit#*/
	unsigned char *puValue/* I: Address of the var to output item value*/);

tmErrorCode_t tmddTDA182I2SetIR_Mixer_Do_step(tmUnitSelect_t tUnit,/*I:Unit#*/
		unsigned char uValue	/* I: Item value */);

tmErrorCode_t tmddTDA182I2GetIR_Mixer_Do_step(tmUnitSelect_t tUnit,/*I:Unit#*/
	unsigned char *puValue/* I: Address of the var to output item value*/);

tmErrorCode_t tmddTDA182I2SetHi_Pass(tmUnitSelect_t tUnit,/*I:Unit#*/
		unsigned char uValue	/* I: Item value */);

tmErrorCode_t tmddTDA182I2GetHi_Pass(tmUnitSelect_t tUnit,/*I:Unit#*/
	unsigned char *puValue/* I: Address of the var to output item value*/);

tmErrorCode_t tmddTDA182I2SetIF_Notch(tmUnitSelect_t tUnit,/*I:Unit#*/
		unsigned char uValue	/* I: Item value */);

tmErrorCode_t tmddTDA182I2GetIF_Notch(tmUnitSelect_t tUnit,/*I:Unit#*/
	unsigned char *puValue/* I: Address of the var to output item value*/);

tmErrorCode_t tmddTDA182I2SetAGC1_loop_off(tmUnitSelect_t tUnit,/*I:Unit#*/
		unsigned char uValue	/* I: Item value */);

tmErrorCode_t tmddTDA182I2GetAGC1_loop_off(tmUnitSelect_t tUnit,/*I:Unit#*/
	unsigned char *puValue/* I: Address of the var to output item value*/);

tmErrorCode_t tmddTDA182I2SetAGC1_Do_step(tmUnitSelect_t tUnit,/*I:Unit#*/
		unsigned char uValue	/* I: Item value */);

tmErrorCode_t tmddTDA182I2GetAGC1_Do_step(tmUnitSelect_t tUnit,/*I:Unit#*/
	unsigned char *puValue/* I: Address of the var to output item value*/);

tmErrorCode_t tmddTDA182I2SetForce_AGC1_gain(tmUnitSelect_t tUnit,/*I:Unit#*/
		unsigned char uValue	/* I: Item value */);

tmErrorCode_t tmddTDA182I2GetForce_AGC1_gain(tmUnitSelect_t tUnit,/*I:Unit#*/
	unsigned char *puValue/* I: Address of the var to output item value*/);

tmErrorCode_t tmddTDA182I2SetAGC1_Gain(tmUnitSelect_t tUnit,/*I:Unit#*/
		unsigned char uValue	/* I: Item value */);

tmErrorCode_t tmddTDA182I2GetAGC1_Gain(tmUnitSelect_t tUnit,/*I:Unit#*/
	unsigned char *puValue/* I: Address of the var to output item value*/);

tmErrorCode_t tmddTDA182I2SetAGC5_loop_off(tmUnitSelect_t tUnit,/*I:Unit#*/
		unsigned char uValue	/* I: Item value */);

tmErrorCode_t tmddTDA182I2GetAGC5_loop_off(tmUnitSelect_t tUnit,/*I:Unit#*/
	unsigned char *puValue/* I: Address of the var to output item value*/);

tmErrorCode_t tmddTDA182I2SetAGC5_Do_step(tmUnitSelect_t tUnit,/*I:Unit#*/
		unsigned char uValue	/* I: Item value */);

tmErrorCode_t tmddTDA182I2GetAGC5_Do_step(tmUnitSelect_t tUnit,/*I:Unit#*/
	unsigned char *puValue/* I: Address of the var to output item value*/);

tmErrorCode_t tmddTDA182I2SetForce_AGC5_gain(tmUnitSelect_t tUnit,/*I:Unit#*/
		unsigned char uValue	/* I: Item value */);

tmErrorCode_t tmddTDA182I2GetForce_AGC5_gain(tmUnitSelect_t tUnit,/*I:Unit#*/
	unsigned char *puValue/* I: Address of the var to output item value*/);

tmErrorCode_t tmddTDA182I2SetAGC5_Gain(tmUnitSelect_t tUnit,/*I:Unit#*/
		unsigned char uValue	/* I: Item value */);

tmErrorCode_t tmddTDA182I2GetAGC5_Gain(tmUnitSelect_t tUnit,/*I:Unit#*/
	unsigned char *puValue/* I: Address of the var to output item value*/);

tmErrorCode_t tmddTDA182I2SetRFCAL_Freq0(tmUnitSelect_t tUnit,/*I:Unit#*/
		unsigned char uValue	/* I: Item value */);

tmErrorCode_t tmddTDA182I2GetRFCAL_Freq0(tmUnitSelect_t tUnit,/*I:Unit#*/
	unsigned char *puValue/* I: Address of the var to output item value*/);

tmErrorCode_t tmddTDA182I2SetRFCAL_Freq1(tmUnitSelect_t tUnit,/*I:Unit#*/
		unsigned char uValue	/* I: Item value */);

tmErrorCode_t tmddTDA182I2GetRFCAL_Freq1(tmUnitSelect_t tUnit,/*I:Unit#*/
	unsigned char *puValue/* I: Address of the var to output item value*/);

tmErrorCode_t tmddTDA182I2SetRFCAL_Freq2(tmUnitSelect_t tUnit,/*I:Unit#*/
		unsigned char uValue	/* I: Item value */);

tmErrorCode_t tmddTDA182I2GetRFCAL_Freq2(tmUnitSelect_t tUnit,/*I:Unit#*/
	unsigned char *puValue/* I: Address of the var to output item value*/);

tmErrorCode_t tmddTDA182I2SetRFCAL_Freq3(tmUnitSelect_t tUnit,/*I:Unit#*/
		unsigned char uValue	/* I: Item value */);

tmErrorCode_t tmddTDA182I2GetRFCAL_Freq3(tmUnitSelect_t tUnit,/*I:Unit#*/
	unsigned char *puValue/* I: Address of the var to output item value*/);

tmErrorCode_t tmddTDA182I2SetRFCAL_Freq4(tmUnitSelect_t tUnit,/*I:Unit#*/
		unsigned char uValue	/* I: Item value */);

tmErrorCode_t tmddTDA182I2GetRFCAL_Freq4(tmUnitSelect_t tUnit,/*I:Unit#*/
	unsigned char *puValue/* I: Address of the var to output item value*/);

tmErrorCode_t tmddTDA182I2SetRFCAL_Freq5(tmUnitSelect_t tUnit,/*I:Unit#*/
		unsigned char uValue	/* I: Item value */);

tmErrorCode_t tmddTDA182I2GetRFCAL_Freq5(tmUnitSelect_t tUnit,/*I:Unit#*/
	unsigned char *puValue/* I: Address of the var to output item value*/);

tmErrorCode_t tmddTDA182I2SetRFCAL_Freq6(tmUnitSelect_t tUnit,/*I:Unit#*/
		unsigned char uValue	/* I: Item value */);

tmErrorCode_t tmddTDA182I2GetRFCAL_Freq6(tmUnitSelect_t tUnit,/*I:Unit#*/
	unsigned char *puValue/* I: Address of the var to output item value*/);

tmErrorCode_t tmddTDA182I2SetRFCAL_Freq7(tmUnitSelect_t tUnit,/*I:Unit#*/
		unsigned char uValue	/* I: Item value */);

tmErrorCode_t tmddTDA182I2GetRFCAL_Freq7(tmUnitSelect_t tUnit,/*I:Unit#*/
	unsigned char *puValue/* I: Address of the var to output item value*/);

tmErrorCode_t tmddTDA182I2SetRFCAL_Freq8(tmUnitSelect_t tUnit,/*I:Unit#*/
		unsigned char uValue	/* I: Item value */);

tmErrorCode_t tmddTDA182I2GetRFCAL_Freq8(tmUnitSelect_t tUnit,/*I:Unit#*/
	unsigned char *puValue/* I: Address of the var to output item value*/);

tmErrorCode_t tmddTDA182I2SetRFCAL_Freq9(tmUnitSelect_t tUnit,/*I:Unit#*/
		unsigned char uValue	/* I: Item value */);

tmErrorCode_t tmddTDA182I2GetRFCAL_Freq9(tmUnitSelect_t tUnit,/*I:Unit#*/
	unsigned char *puValue/* I: Address of the var to output item value*/);

tmErrorCode_t tmddTDA182I2SetRFCAL_Freq10(tmUnitSelect_t tUnit,/*I:Unit#*/
		unsigned char uValue	/* I: Item value */);

tmErrorCode_t tmddTDA182I2GetRFCAL_Freq10(tmUnitSelect_t tUnit,/*I:Unit#*/
	unsigned char *puValue/* I: Address of the var to output item value*/);

tmErrorCode_t tmddTDA182I2SetRFCAL_Freq11(tmUnitSelect_t tUnit,/*I:Unit#*/
		unsigned char uValue	/* I: Item value */);

tmErrorCode_t tmddTDA182I2GetRFCAL_Freq11(tmUnitSelect_t tUnit,/*I:Unit#*/
	unsigned char *puValue/* I: Address of the var to output item value*/);

tmErrorCode_t tmddTDA182I2SetRFCAL_Offset0(tmUnitSelect_t tUnit,/*I:Unit#*/
		unsigned char uValue	/* I: Item value */);

tmErrorCode_t tmddTDA182I2GetRFCAL_Offset0(tmUnitSelect_t tUnit,/*I:Unit#*/
	unsigned char *puValue/* I: Address of the var to output item value*/);

tmErrorCode_t tmddTDA182I2SetRFCAL_Offset1(tmUnitSelect_t tUnit,/* I: Unit no.*/
		unsigned char uValue	/* I: Item value */);

tmErrorCode_t tmddTDA182I2GetRFCAL_Offset1(tmUnitSelect_t tUnit,/*I:Unit#*/
	unsigned char *puValue/* I: Address of the var to output item value*/);

tmErrorCode_t tmddTDA182I2SetRFCAL_Offset2(tmUnitSelect_t tUnit,/*I:Unit#*/
		unsigned char uValue	/* I: Item value */);

tmErrorCode_t tmddTDA182I2GetRFCAL_Offset2(tmUnitSelect_t tUnit,/*I:Unit#*/
	unsigned char *puValue/* I: Address of the var to output item value*/);

tmErrorCode_t tmddTDA182I2SetRFCAL_Offset3(tmUnitSelect_t tUnit,/*I:Unit#*/
		unsigned char uValue	/* I: Item value */);

tmErrorCode_t tmddTDA182I2GetRFCAL_Offset3(tmUnitSelect_t tUnit,/*I:Unit#*/
	unsigned char *puValue/* I: Address of the var to output item value*/);

tmErrorCode_t tmddTDA182I2SetRFCAL_Offset4(tmUnitSelect_t tUnit,/*I:Unit#*/
		unsigned char uValue	/* I: Item value */);

tmErrorCode_t tmddTDA182I2GetRFCAL_Offset4(tmUnitSelect_t tUnit,/*I:Unit#*/
	unsigned char *puValue/* I: Address of the var to output item value*/);

tmErrorCode_t tmddTDA182I2SetRFCAL_Offset5(tmUnitSelect_t tUnit,/*I:Unit#*/
		unsigned char uValue	/* I: Item value */);

tmErrorCode_t tmddTDA182I2GetRFCAL_Offset5(tmUnitSelect_t tUnit,/*I:Unit#*/
	unsigned char *puValue/* I: Address of the var to output item value*/);

tmErrorCode_t tmddTDA182I2SetRFCAL_Offset6(tmUnitSelect_t tUnit,/*I:Unit#*/
		unsigned char uValue	/* I: Item value */);

tmErrorCode_t tmddTDA182I2GetRFCAL_Offset6(tmUnitSelect_t tUnit,/*I:Unit#*/
	unsigned char *puValue/* I: Address of the var to output item value*/);

tmErrorCode_t tmddTDA182I2SetRFCAL_Offset7(tmUnitSelect_t tUnit,/*I:Unit#*/
		unsigned char uValue	/* I: Item value */);

tmErrorCode_t tmddTDA182I2GetRFCAL_Offset7(tmUnitSelect_t tUnit,/*I:Unit#*/
	unsigned char *puValue/* I: Address of the var to output item value*/);

tmErrorCode_t tmddTDA182I2SetRFCAL_Offset8(tmUnitSelect_t tUnit,/*I:Unit#*/
		unsigned char uValue	/* I: Item value */);

tmErrorCode_t tmddTDA182I2GetRFCAL_Offset8(tmUnitSelect_t tUnit,/*I:Unit#*/
	unsigned char *puValue/* I: Address of the var to output item value*/);

tmErrorCode_t tmddTDA182I2SetRFCAL_Offset9(tmUnitSelect_t tUnit,/*I:Unit#*/
		unsigned char uValue	/* I: Item value */);

tmErrorCode_t tmddTDA182I2GetRFCAL_Offset9(tmUnitSelect_t tUnit,/*I:Unit#*/
	unsigned char *puValue/* I: Address of the var to output item value*/);

tmErrorCode_t tmddTDA182I2SetRFCAL_Offset10(tmUnitSelect_t tUnit,/*I:Unit#*/
		unsigned char uValue	/* I: Item value */);

tmErrorCode_t tmddTDA182I2GetRFCAL_Offset10(tmUnitSelect_t tUnit,/*I:Unit#*/
	unsigned char *puValue/* I: Address of the var to output item value*/);

tmErrorCode_t tmddTDA182I2SetRFCAL_Offset11(tmUnitSelect_t tUnit,/*I:Unit#*/
		unsigned char uValue	/* I: Item value */);

tmErrorCode_t tmddTDA182I2GetRFCAL_Offset11(tmUnitSelect_t tUnit,/*I:Unit#*/
	unsigned char *puValue/* I: Address of the var to output item value*/);

tmErrorCode_t
tmddTDA182I2SetRFCAL_SW_Algo_Enable(tmUnitSelect_t tUnit,/*I:Unit#*/
		unsigned char uValue	/* I: Item value */);

tmErrorCode_t
tmddTDA182I2GetRFCAL_SW_Algo_Enable(tmUnitSelect_t tUnit,/*I:Unit#*/
	unsigned char *puValue/* I: Address of the var to output item value*/);

tmErrorCode_t tmddTDA182I2SetRF_Filter_Bypass(tmUnitSelect_t tUnit,/*I:Unit#*/
		unsigned char uValue	/* I: Item value */);

tmErrorCode_t tmddTDA182I2GetRF_Filter_Bypass(tmUnitSelect_t tUnit,/*I:Unit#*/
	unsigned char *puValue/* I: Address of the var to output item value*/);

tmErrorCode_t tmddTDA182I2SetAGC2_loop_off(tmUnitSelect_t tUnit,/*I:Unit#*/
		unsigned char uValue	/* I: Item value */);

tmErrorCode_t tmddTDA182I2GetAGC2_loop_off(tmUnitSelect_t tUnit,/*I:Unit#*/
	unsigned char *puValue/* I: Address of the var to output item value*/);

tmErrorCode_t tmddTDA182I2SetForce_AGC2_gain(tmUnitSelect_t tUnit,/*I:Unit#*/
		unsigned char uValue	/* I: Item value */);

tmErrorCode_t tmddTDA182I2GetForce_AGC2_gain(tmUnitSelect_t tUnit,/*I:Unit#*/
	unsigned char *puValue/* I: Address of the var to output item value*/);

tmErrorCode_t tmddTDA182I2SetRF_Filter_Gv(tmUnitSelect_t tUnit,/*I:Unit#*/
		unsigned char uValue	/* I: Item value */);

tmErrorCode_t tmddTDA182I2GetRF_Filter_Gv(tmUnitSelect_t tUnit,/*I:Unit#*/
	unsigned char *puValue/* I: Address of the var to output item value*/);

tmErrorCode_t tmddTDA182I2SetRF_Filter_Band(tmUnitSelect_t tUnit,/*I:Unit#*/
		unsigned char uValue	/* I: Item value */);

tmErrorCode_t tmddTDA182I2GetRF_Filter_Band(tmUnitSelect_t tUnit,/*I:Unit#*/
	unsigned char *puValue/* I: Address of the var to output item value*/);

tmErrorCode_t tmddTDA182I2SetRF_Filter_Cap(tmUnitSelect_t tUnit,/*I:Unit#*/
		unsigned char uValue	/* I: Item value */);

tmErrorCode_t tmddTDA182I2GetRF_Filter_Cap(tmUnitSelect_t tUnit,/*I:Unit#*/
	unsigned char *puValue/* I: Address of the var to output item value*/);

tmErrorCode_t tmddTDA182I2SetAGC2_Do_step(tmUnitSelect_t tUnit,/*I:Unit#*/
		unsigned char uValue	/* I: Item value */);

tmErrorCode_t tmddTDA182I2GetAGC2_Do_step(tmUnitSelect_t tUnit,/*I:Unit#*/
	unsigned char *puValue/* I: Address of the var to output item value*/);

tmErrorCode_t tmddTDA182I2SetGain_Taper(tmUnitSelect_t tUnit,/*I:Unit#*/
		unsigned char uValue	/* I: Item value */);

tmErrorCode_t tmddTDA182I2GetGain_Taper(tmUnitSelect_t tUnit,/*I:Unit#*/
	unsigned char *puValue/* I: Address of the var to output item value*/);

tmErrorCode_t tmddTDA182I2SetRF_BPF(tmUnitSelect_t tUnit,/*I:Unit#*/
		unsigned char uValue	/* I: Item value */);

tmErrorCode_t tmddTDA182I2GetRF_BPF(tmUnitSelect_t tUnit,/*I:Unit#*/
	unsigned char *puValue/* I: Address of the var to output item value*/);

tmErrorCode_t tmddTDA182I2SetRF_BPF_Bypass(tmUnitSelect_t tUnit,/*I:Unit#*/
		unsigned char uValue	/* I: Item value */);

tmErrorCode_t tmddTDA182I2GetRF_BPF_Bypass(tmUnitSelect_t tUnit,/*I:Unit#*/
	unsigned char *puValue/* I: Address of the var to output item value*/);

tmErrorCode_t tmddTDA182I2SetN_CP_Current(tmUnitSelect_t tUnit,/*I:Unit#*/
		unsigned char uValue	/* I: Item value */);

tmErrorCode_t tmddTDA182I2GetN_CP_Current(tmUnitSelect_t tUnit,/*I:Unit#*/
	unsigned char *puValue/* I: Address of the var to output item value*/);

tmErrorCode_t tmddTDA182I2GetUp_AGC5(tmUnitSelect_t tUnit,/*I:Unit#*/
	unsigned char *puValue/* I: Address of the var to output item value*/);

tmErrorCode_t tmddTDA182I2GetDo_AGC5(tmUnitSelect_t tUnit,/*I:Unit#*/
	unsigned char *puValue/* I: Address of the var to output item value*/);

tmErrorCode_t tmddTDA182I2GetUp_AGC4(tmUnitSelect_t tUnit,/*I:Unit#*/
	unsigned char *puValue/* I: Address of the var to output item value*/);

tmErrorCode_t tmddTDA182I2GetDo_AGC4(tmUnitSelect_t tUnit,/*I:Unit#*/
	unsigned char *puValue/* I: Address of the var to output item value*/);

tmErrorCode_t tmddTDA182I2GetUp_AGC2(tmUnitSelect_t tUnit,/*I:Unit#*/
	unsigned char *puValue/* I: Address of the var to output item value*/);

tmErrorCode_t tmddTDA182I2GetDo_AGC2(tmUnitSelect_t tUnit,/*I:Unit#*/
	unsigned char *puValue/* I: Address of the var to output item value*/);

tmErrorCode_t tmddTDA182I2GetUp_AGC1(tmUnitSelect_t tUnit,/*I:Unit#*/
	unsigned char *puValue/* I: Address of the var to output item value*/);

tmErrorCode_t tmddTDA182I2GetDo_AGC1(tmUnitSelect_t tUnit,/*I:Unit#*/
	unsigned char *puValue/* I: Address of the var to output item value*/);

tmErrorCode_t tmddTDA182I2GetAGC2_Gain_Read(tmUnitSelect_t tUnit,/*I:Unit#*/
	unsigned char *puValue/* I: Address of the var to output item value*/);

tmErrorCode_t tmddTDA182I2GetAGC1_Gain_Read(tmUnitSelect_t tUnit,/*I:Unit#*/
	unsigned char *puValue/* I: Address of the var to output item value*/);

tmErrorCode_t tmddTDA182I2GetTOP_AGC3_Read(tmUnitSelect_t tUnit,/*I:Unit#*/
	unsigned char *puValue/* I: Address of the var to output item value*/);

tmErrorCode_t tmddTDA182I2GetAGC5_Gain_Read(tmUnitSelect_t tUnit,/*I:Unit#*/
	unsigned char *puValue/* I: Address of the var to output item value*/);

tmErrorCode_t tmddTDA182I2GetAGC4_Gain_Read(tmUnitSelect_t tUnit,/*I:Unit#*/
	unsigned char *puValue/* I: Address of the var to output item value*/);

tmErrorCode_t tmddTDA182I2SetRSSI(tmUnitSelect_t tUnit,/*I:Unit#*/
		unsigned char uValue	/* I: Item value */);

tmErrorCode_t tmddTDA182I2GetRSSI(tmUnitSelect_t tUnit,/*I:Unit#*/
	unsigned char *puValue/* I: Address of the var to output item value*/);

tmErrorCode_t tmddTDA182I2SetRSSI_AV(tmUnitSelect_t tUnit,/*I:Unit#*/
		unsigned char uValue	/* I: Item value */);

tmErrorCode_t tmddTDA182I2GetRSSI_AV(tmUnitSelect_t tUnit,/*I:Unit#*/
	unsigned char *puValue/* I: Address of the var to output item value*/);

tmErrorCode_t tmddTDA182I2SetRSSI_Cap_Reset_En(tmUnitSelect_t tUnit,/*I:Unit#*/
		unsigned char uValue	/* I: Item value */);

tmErrorCode_t tmddTDA182I2GetRSSI_Cap_Reset_En(tmUnitSelect_t tUnit,/*I:Unit#*/
	unsigned char *puValue/* I: Address of the var to output item value*/);

tmErrorCode_t tmddTDA182I2SetRSSI_Cap_Val(tmUnitSelect_t tUnit,/*I:Unit#*/
		unsigned char uValue	/* I: Item value */);

tmErrorCode_t tmddTDA182I2GetRSSI_Cap_Val(tmUnitSelect_t tUnit,/*I:Unit#*/
	unsigned char *puValue/* I: Address of the var to output item value*/);

tmErrorCode_t tmddTDA182I2SetRSSI_Ck_Speed(tmUnitSelect_t tUnit,/*I:Unit#*/
		unsigned char uValue	/* I: Item value */);

tmErrorCode_t tmddTDA182I2GetRSSI_Ck_Speed(tmUnitSelect_t tUnit,/*I:Unit#*/
	unsigned char *puValue/* I: Address of the var to output item value*/);

tmErrorCode_t tmddTDA182I2SetRSSI_Dicho_not(tmUnitSelect_t tUnit,/*I:Unit#*/
		unsigned char uValue	/* I: Item value */);

tmErrorCode_t tmddTDA182I2GetRSSI_Dicho_not(tmUnitSelect_t tUnit,/*I:Unit#*/
	unsigned char *puValue/* I: Address of the var to output item value*/);

tmErrorCode_t tmddTDA182I2SetRFCAL_Phi2(tmUnitSelect_t tUnit,/*I:Unit#*/
		unsigned char uValue	/* I: Item value */);

tmErrorCode_t tmddTDA182I2GetRFCAL_Phi2(tmUnitSelect_t tUnit,/*I:Unit#*/
	unsigned char *puValue/* I: Address of the var to output item value*/);

tmErrorCode_t tmddTDA182I2SetDDS_Polarity(tmUnitSelect_t tUnit,/*I:Unit#*/
		unsigned char uValue	/* I: Item value */);

tmErrorCode_t tmddTDA182I2GetDDS_Polarity(tmUnitSelect_t tUnit,/*I:Unit#*/
	unsigned char *puValue/* I: Address of the var to output item value*/);

tmErrorCode_t tmddTDA182I2SetRFCAL_DeltaGain(tmUnitSelect_t tUnit,/*I:Unit#*/
		unsigned char uValue	/* I: Item value */);

tmErrorCode_t tmddTDA182I2GetRFCAL_DeltaGain(tmUnitSelect_t tUnit,/*I:Unit#*/
	unsigned char *puValue/* I: Address of the var to output item value*/);

tmErrorCode_t tmddTDA182I2SetIRQ_Polarity(tmUnitSelect_t tUnit,/*I:Unit#*/
		unsigned char uValue	/* I: Item value */);

tmErrorCode_t tmddTDA182I2GetIRQ_Polarity(tmUnitSelect_t tUnit,/*I:Unit#*/
	unsigned char *puValue/* I: Address of the var to output item value*/);

tmErrorCode_t tmddTDA182I2_ExpertGetrfcal_log_0(tmUnitSelect_t tUnit,/*I:Unit#*/
	unsigned char *puValue/* I: Address of the var to output item value*/);

tmErrorCode_t tmddTDA182I2_ExpertGetrfcal_log_1(tmUnitSelect_t tUnit,/*I:Unit#*/
	unsigned char *puValue/* I: Address of the var to output item value*/);

tmErrorCode_t tmddTDA182I2_ExpertGetrfcal_log_2(tmUnitSelect_t tUnit,/*I:Unit#*/
	unsigned char *puValue/* I: Address of the var to output item value*/);

tmErrorCode_t tmddTDA182I2_ExpertGetrfcal_log_3(tmUnitSelect_t tUnit,/*I:Unit#*/
	unsigned char *puValue/* I: Address of the var to output item value*/);

tmErrorCode_t tmddTDA182I2_ExpertGetrfcal_log_4(tmUnitSelect_t tUnit,/*I:Unit#*/
	unsigned char *puValue/* I: Address of the var to output item value*/);

tmErrorCode_t tmddTDA182I2_ExpertGetrfcal_log_5(tmUnitSelect_t tUnit,/*I:Unit#*/
	unsigned char *puValue/* I: Address of the var to output item value*/);

tmErrorCode_t tmddTDA182I2_ExpertGetrfcal_log_6(tmUnitSelect_t tUnit,/*I:Unit#*/
	unsigned char *puValue/* I: Address of the var to output item value*/);

tmErrorCode_t tmddTDA182I2_ExpertGetrfcal_log_7(tmUnitSelect_t tUnit,/*I:Unit#*/
	unsigned char *puValue/* I: Address of the var to output item value*/);

tmErrorCode_t tmddTDA182I2_ExpertGetrfcal_log_8(tmUnitSelect_t tUnit,/*I:Unit#*/
	unsigned char *puValue/* I: Address of the var to output item value*/);

tmErrorCode_t tmddTDA182I2_ExpertGetrfcal_log_9(tmUnitSelect_t tUnit,/*I:Unit#*/
	unsigned char *puValue/* I: Address of the var to output item value*/);

tmErrorCode_t
tmddTDA182I2_ExpertGetrfcal_log_10(tmUnitSelect_t tUnit,/*I:Unit#*/
	unsigned char *puValue/* I: Address of the var to output item value*/);

tmErrorCode_t
tmddTDA182I2_ExpertGetrfcal_log_11(tmUnitSelect_t tUnit,/*I:Unit#*/
	unsigned char *puValue/* I: Address of the var to output item value*/);

tmErrorCode_t tmddTDA182I2LaunchRF_CAL(tmUnitSelect_t tUnit/*I:Unit#*/);

tmErrorCode_t tmddTDA182I2WaitIRQ(tmUnitSelect_t tUnit,/*I:Unit#*/
			  UInt32 timeOut,	/* I: timeout */
			  UInt32 waitStep,	/* I: wait step */
			  unsigned char irqStatus	/* I: IRQs to wait */);

tmErrorCode_t tmddTDA182I2WaitXtalCal_End(tmUnitSelect_t tUnit,/*I:Unit#*/
				  UInt32 timeOut,	/* I: timeout */
				  UInt32 waitStep	/* I: wait step */);

tmErrorCode_t tmddTDA182I2SetResetWait(tmUnitSelect_t tUnit,/*I:Unit#*/
		/* I: Determine if we need to wait in Reset function */
		Bool bWait);

tmErrorCode_t tmddTDA182I2GetResetWait(tmUnitSelect_t tUnit,/*I:Unit#*/
		/* O: Determine if we need to wait in Reset function */
		bool *pbWait);

#ifdef __cplusplus
}
#endif

#endif /* _TMDD_TDA182I2_ADVANCED_H //--------------- */
