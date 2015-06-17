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

Source file name : mxl584_drv.c
Author :

Low level driver for STV0910

Date        Modification                                    Name
----        ------------                                    --------
23-Apr-12   Created						HS

***************************************************************************/

/* includes ---------------------------------------------------------- */
#include <linux/version.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 2, 2)
#include <linux/export.h>
#else
#include <linux/module.h>
#endif
/*#include "globaldefs.h"*/
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/list.h>
#include <stm_fe.h>
#include <stm_fe_os.h>
#include <i2c_wrapper.h>
#include <stm_fe_demod.h>
#include <fesat_commlla_str.h>
#include <linux/firmware.h>/*linux firmware framework*/
#include "d0mxl584.h"
#include "mxl584_init.h"
#include "mxl584_drv.h"
#include "maxlineardatatypes.h"
#include "mxlware_hydra_oem_drv.h"
#include "mxlware_hydra_diseqcfskapi.h"
#include "mxlware_hydra_commonapi.h"

static int lla_wrapper_debug2;
module_param_named(lla_wrapper_debug2, lla_wrapper_debug2, int, 0644);
MODULE_PARM_DESC(lla_wrapper_debug2,
		 "Turn on/off mxl584 lla wrapper_2 debugging (default:off).");
#define dpr_info(x...) do { if (lla_wrapper_debug2) pr_info(x); } while (0)
#define dpr_err(x...) do { if (lla_wrapper_debug2) pr_err(x); } while (0)

/*****************************************************
*  Device ID used in this sample code.
* If more Hydra devices are used at the same time,
 *they should have   consecutive device ID's
******************************************************/
#define HYDRA_DEV_ID 0
#define MXL_HYDRA_DEFAULT_FW "MxL_Hydra_FW_1.1.1.19.mbin"

/*testing onlyplz remove afterwards*/
char *standard[MXL_HYDRA_DVBS2+1] = {("DSS"), ("DVB-S"), ("DVB-S2")};
char *fec[MXL_HYDRA_FEC_9_10+1] = {
	("AUTO"), ("1/2"), ("3/5"), ("2/3"), ("3/4"), ("4/5"), ("5/6"),
	("6/7"), ("7/8"), ("8/9"), ("9/10") };
char *modulation[MXL_HYDRA_MOD_8PSK+1] = {("AUTO"), ("QPSK"), ("8PSK")};
char *pilots[MXL_HYDRA_PILOTS_AUTO+1] = {("OFF"), ("ON"), ("AUTO")};
char *rolloff[MXL_HYDRA_ROLLOFF_0_35+1] = {("AUTO"), (".20"), (".25"), (".35")};
char *spectrum[MXL_HYDRA_SPECTRUM_NON_INVERTED+1] = {
	("AUTO"), ("INVERTED"), ("NORMAL")};

/************************
Current LLA revision
*************************/
static const ST_Revision_t RevisionMXL584  = "MXL584-LLA_REL_1.1.1.19";

static fe_mutex *lock_mxl;

/*****************************************************
--FUNCTION	::	FE_MXL584_GetRevision
--ACTION	::	Return current LLA version
--PARAMS IN	::	NONE
--PARAMS OUT::	NONE
--RETURN	::	Revision ==> Text string containing LLA version
--***************************************************/
ST_Revision_t FE_MXL584_GetRevision(void)
{
	return RevisionMXL584;
}

/*****************************************************
--FUNCTION	::	FE_MXL584_Init
--ACTION	::	Initialisation of the MXL584 chip
--PARAMS IN	::	pInit	==>	Front End Init parameters
--PARAMS OUT::	NONE
--RETURN	::	Handle to MXL584
--***************************************************/
FE_MXL584_Error_t FE_MXL584_Init(FE_MXL584_InitParams_t	*pInit,
				 FE_MXL584_Handle_t *Handle)
{
	FE_MXL584_InternalParams_t *p = NULL;
	/* Demodulator chip initialisation parameters */
	Demod_InitParams_t d_init_p;
	FE_MXL584_Error_t err = FE_LLA_NO_ERROR;
	STCHIP_Error_t  d_err = CHIPERR_NO_ERROR;
	int ret = -1;
	UINT32 intrMask = 0;
	MXL_HYDRA_INTR_CFG_T intrCfg;
	MXL_HYDRA_VER_INFO_T mxl_ver;

	/* Internal params structure allocation */
	p = (FE_MXL584_InternalParams_t *)(*Handle);

	if (p == NULL)
		return FE_LLA_INVALID_HANDLE;

	/* Chip initialisation/Demodulator  Init */
	d_init_p.Chip = (p->hDemod);
	d_init_p.NbDefVal = MXL_HYDRA_NBREGS;
	d_init_p.Chip->RepeaterHost = NULL;
	d_init_p.Chip->RepeaterFn = NULL;
	d_init_p.Chip->Repeater = FALSE;
	d_init_p.Chip->I2cAddr = 0x60;
	strcpy((char *)d_init_p.Chip->Name, pInit->DemodName);

	lock_mxl = stm_fe_malloc(sizeof(fe_mutex));
	if (lock_mxl == NULL)
		return -EINVAL;

	ret = stm_fe_mutex_init(lock_mxl);
	if (ret != 0) {
		stm_fe_free((void **)&lock_mxl);
		pr_err("%s :Mutex init fail:lock_mxl\n", __func__);
		return ret;
	}

	p->status = MXL_SUCCESS;

	d_err = MXL584_Init(&d_init_p, &(p->hDemod));

	stm_fe_mutex_lock(lock_mxl);

	p->status = MxLWare_HYDRA_API_CfgDrvInit(HYDRA_DEV_ID, p->hDemod,
						MXL_HYDRA_DEVICE_584);
	stm_fe_mutex_unlock(lock_mxl);

	/*check if device already INITIALIZED;
	*need not call the below API's for more than 1 instances */
	if (p->status == MXL_ALREADY_INITIALIZED) {
		pr_info("%s :MxLWare_HYDRA_API_CfgDrvInit status =%d\n",
				__func__, p->status);
		return FE_LLA_NO_ERROR;
	} else if (p->status != MXL_SUCCESS) {
		pr_err("%s :MxLWare_HYDRA_API_CfgDrvInit status =%d\n",
				 __func__, p->status);
		return FE_LLA_BAD_PARAMETER;
	}

	stm_fe_mutex_lock(lock_mxl);
	p->status = MxLWare_HYDRA_API_CfgDevOverwriteDefault(HYDRA_DEV_ID);
	stm_fe_mutex_unlock(lock_mxl);

	if (p->status != MXL_SUCCESS) {
		pr_err("%s :_HYDRA_API_CfgDevOverwriteDefault status = %d\n",
			__func__, p->status);
		return FE_LLA_BAD_PARAMETER;
	}
	/*download FW*/
	stm_fe_mutex_lock(lock_mxl);

	p->status = FE_MXL584_downloadFirmware(HYDRA_DEV_ID, &(p->hDemod));
	stm_fe_mutex_unlock(lock_mxl);
	if (p->status != MXL_SUCCESS) {
		pr_err("%s :FE_MXL584_downloadFirmware status =%d\n",
				__func__, p->status);
		return FE_LLA_BAD_PARAMETER;
	} else if (p->status == MXL_SUCCESS) {

		stm_fe_mutex_lock(lock_mxl);
		p->status = MxLWare_HYDRA_API_ReqDevVersionInfo(HYDRA_DEV_ID,
						&mxl_ver);
		stm_fe_mutex_unlock(lock_mxl);
		if (p->status == MXL_SUCCESS) {
			pr_info("%s :Firmware ver. %d.%d.%d.%d-RC%d\n",
				__func__, mxl_ver.firmwareVer[0],
				mxl_ver.firmwareVer[1],
				mxl_ver.firmwareVer[2],
				mxl_ver.firmwareVer[3],
				mxl_ver.firmwareVer[4]);
		} else {
			pr_err("%s :HYDRA_API_ReqDevVersionInfo status = %d\n",
					__func__, p->status);
			return FE_LLA_BAD_PARAMETER;
		}
	}
	/* Sleep for 500 ms*/
	ChipWaitOrAbort(p->hDemod, 500);

	/* Configure device interrupts*/
	intrCfg.intrDurationInNanoSecs = 10000; /*10 ms*/
	intrCfg.intrType = HYDRA_HOST_INTR_TYPE_LEVEL_POSITIVE;
	intrMask = MXL_HYDRA_INTR_EN |
			MXL_HYDRA_INTR_FSK |
			MXL_HYDRA_INTR_DMD_FEC_LOCK_0 |
			MXL_HYDRA_INTR_DMD_FEC_LOCK_4 |
			MXL_HYDRA_INTR_DMD_FEC_LOCK_1 |
			MXL_HYDRA_INTR_DMD_FEC_LOCK_2 |
			MXL_HYDRA_INTR_DMD_FEC_LOCK_3 |
			MXL_HYDRA_INTR_DMD_FEC_LOCK_5;
	stm_fe_mutex_lock(lock_mxl);
	p->status = MxLWare_HYDRA_API_CfgDevInterrupt(HYDRA_DEV_ID, intrCfg,
								intrMask);
	stm_fe_mutex_unlock(lock_mxl);
	if (p->status != MXL_SUCCESS) {
		pr_err("%s :MxLWare_HYDRA_API_CfgDevInterrupt status =%d\n",
							__func__, p->status);
		return FE_LLA_BAD_PARAMETER;
	}

	/*Select either DiSEqC or FSK - Select FSK */
	stm_fe_mutex_lock(lock_mxl);
	p->status = MxLWare_HYDRA_API_CfgDevDiseqcFskOpMode(HYDRA_DEV_ID,
					MXL_HYDRA_AUX_CTRL_MODE_FSK);
	stm_fe_mutex_unlock(lock_mxl);
	if (p->status != MXL_SUCCESS) {
		pr_err("%s :_HYDRA_API_CfgDevDiseqcFskOpMode status =%d\n",
					__func__, p->status);
		return FE_LLA_BAD_PARAMETER;
	}

	/* Configure FSK operating mode*/
	stm_fe_mutex_lock(lock_mxl);
	p->status = MxLWare_HYDRA_API_CfgFskOpMode(HYDRA_DEV_ID,
					MXL_HYDRA_FSK_CFG_TYPE_39KPBS);
	stm_fe_mutex_unlock(lock_mxl);
	if (p->status != MXL_SUCCESS) {
		pr_err("%s :_HYDRA_API_CfgDevDiseqcFskOpMode status =%d\n",
					__func__, p->status);
		return FE_LLA_BAD_PARAMETER;
	}
#if 0
	/* Config TS MUX mode No mux for TS0 to TS7*/
	p->status = MxLWare_HYDRA_API_CfgTSMixMuxMode(HYDRA_DEV_ID,
					MXL_HYDRA_TS_GROUP_0_3,
					MXL_HYDRA_TS_MUX_DISABLE);
	if (p->status != MXL_SUCCESS) {
		pr_err("%s :MxLWare_HYDRA_API_CfgTSMixMuxMode status =%d\n",
							__func__, p->status);
		return FE_LLA_BAD_PARAMETER;
	}
	p->status = MxLWare_HYDRA_API_CfgTSMixMuxMode(HYDRA_DEV_ID,
					MXL_HYDRA_TS_GROUP_4_7,
					MXL_HYDRA_TS_MUX_DISABLE);
	if (p->status != MXL_SUCCESS) {
		pr_err("%s :MxLWare_HYDRA_API_CfgTSMixMuxMode status =%d\n",
							__func__, p->status);
		return FE_LLA_BAD_PARAMETER;
	}
#endif
#if 0
	/* Config TS interface of the device*/
	ts_cfg.enable = MXL_ENABLE;
	ts_cfg.lsbOrMsbFirst = MXL_HYDRA_MPEG_SERIAL_MSB_1ST;
	ts_cfg.maxMpegClkRate = 104;
	ts_cfg.mpegClkPhase = MXL_HYDRA_MPEG_CLK_PHASE_SHIFT_90_DEG;
	ts_cfg.mpegClkPol = MXL_HYDRA_MPEG_ACTIVE_HIGH;
	ts_cfg.mpegClkType = MXL_HYDRA_MPEG_CLK_GAPPED;
	ts_cfg.mpegErrorIndication = MXL_HYDRA_MPEG_ERR_INDICATION_DISABLED;
	ts_cfg.mpegMode = MXL_HYDRA_MPEG_MODE_SERIAL_3_WIRE;
	ts_cfg.mpegSyncPol = MXL_HYDRA_MPEG_ACTIVE_HIGH;
	ts_cfg.mpegSyncPulseWidth = MXL_HYDRA_MPEG_SYNC_WIDTH_BIT;
	ts_cfg.mpegValidPol = MXL_HYDRA_MPEG_ACTIVE_HIGH;

	for (j = MXL_HYDRA_DEMOD_ID_0 ; j < MXL_HYDRA_DEMOD_MAX; j++) {
		/*if (j == MXL_HYDRA_DEMOD_ID_0)
			ts_cfg.mpegClkPhase =
				MXL_HYDRA_MPEG_CLK_PHASE_SHIFT_90_DEG;
		else
			ts_cfg.mpegClkPhase =
				MXL_HYDRA_MPEG_CLK_PHASE_SHIFT_90_DEG;
		*/
		if (j == MXL_HYDRA_DEMOD_ID_3)
				ts_cfg.maxMpegClkRate = 66;
		else
				ts_cfg.maxMpegClkRate = 104;

		p->status = MxLWare_HYDRA_API_CfgMpegOutParams(HYDRA_DEV_ID,
					(MXL_HYDRA_DEMOD_ID_E)j, &ts_cfg);
		if (p->status != MXL_SUCCESS) {
			pr_err("%s :_HYDRA_API_CfgMpegOutParams status = %d\n",
						__func__, p->status);
			return FE_LLA_BAD_PARAMETER;
		}
		dpr_info("%s :<D=%d> En = %d lsbOrMsbFirst = %d\n"
			"maxMpegClkRate = %d  mpegClkPhase =  %d\n"
			"mpegClkPol =  %d   mpegClkType = %d\n"
			"mpegErrorIndication = %d mpegMode = %d\n"
			"mpegSyncPol = %d mpegSyncPulseWidth = %d\n"
			"mpegValidPol = %d\n ", __func__, j,
			ts_cfg.enable, ts_cfg.lsbOrMsbFirst,
			ts_cfg.maxMpegClkRate, ts_cfg.mpegClkPhase,
			ts_cfg.mpegClkPol, ts_cfg.mpegClkType,
			ts_cfg.mpegErrorIndication,
			ts_cfg.mpegMode, ts_cfg.mpegSyncPol,
			ts_cfg.mpegSyncPulseWidth, ts_cfg.mpegValidPol);

	}
#endif
	return err;
}
/************************************************************************
--FUNCTION	::	FE_MXL584_GetSignalInfo
--ACTION	::	Return informations on the locked transponder
--PARAMS IN	::	Handle	==>	Front End Handle
		::	Demod	==>	Cuurent demod 1 or 2
--PARAMS OUT::	pInfo	==> Informations (BER,C/N,power ...)
--RETURN	::	Error (if any)
--*********************************************************************/
FE_MXL584_Error_t FE_MXL584_GetSignalInfo(FE_MXL584_Handle_t Handle,
					  FE_MXL584_DEMOD_t Demod,
					  FE_MXL584_SignalInfo_t *pInfo)
{
	FE_MXL584_InternalParams_t *p = NULL;
	MXL_HYDRA_DEMOD_LOCK_T   lock;
	MXL_HYDRA_DEMOD_STATUS_T d_stats;
	MXL_HYDRA_TUNE_PARAMS_T tp;
	SINT32 rf = 0;
	SINT16 cnr = 0;

	p = (FE_MXL584_InternalParams_t *)Handle;
	if (p == NULL)
		return FE_LLA_INVALID_HANDLE;

	stm_fe_mutex_lock(lock_mxl);
	p->status = MxLWare_HYDRA_API_ReqDemodLockStatus(HYDRA_DEV_ID,
					(MXL_HYDRA_DEMOD_ID_E)Demod, &lock);
	stm_fe_mutex_unlock(lock_mxl);
	if (p->status != MXL_SUCCESS) {
		pr_err("%s :MxLWare_HYDRA_API_ReqDemodLockStatus status =%d\n",
							__func__, p->status);
	return FE_LLA_BAD_PARAMETER;
	}

	if (lock.fecLock == MXL_TRUE) {
		pInfo->Locked = TRUE;
		stm_fe_mutex_lock(lock_mxl);
		p->status = MxLWare_HYDRA_API_ReqDemodChanParams(HYDRA_DEV_ID,
					(MXL_HYDRA_DEMOD_ID_E)Demod, &tp);
		p->status |=
		  MxLWare_HYDRA_API_ReqDemodErrorCounters(HYDRA_DEV_ID,
					(MXL_HYDRA_DEMOD_ID_E)Demod, &d_stats);
		stm_fe_mutex_unlock(lock_mxl);
		if (p->status != MXL_SUCCESS) {
			pr_err("%s :_HYDRA_API_ReqDemodChanParams status =%d\n",
					__func__, p->status);
			return FE_LLA_BAD_PARAMETER;
		}
		pInfo->Standard = mxl_to_stfe_standard(tp.standardMask);
		pInfo->Frequency = tp.frequencyInHz/1000;
		pInfo->SymbolRate = tp.symbolRateKSps*1000;

		if (pInfo->Standard == FE_SAT_DVBS2_STANDARD) {
			pInfo->PunctureRate =
				mxl_to_stfe_fec_map(tp.params.paramsS2.fec);
			pInfo->ModCode = p->DemodResults.ModCode;
			pInfo->Pilots = tp.params.paramsS2.pilots;
			pInfo->FrameLength = p->DemodResults.FrameLength;
			pInfo->RollOff =
				mxl_to_stfe_r_off(tp.params.paramsS.rollOff);
			pInfo->Modulation =
			mxl_to_stfe_modulation(tp.params.paramsS2.modulation);
		} else if (pInfo->Standard == FE_SAT_DVBS1_STANDARD) {
			pInfo->PunctureRate =
				mxl_to_stfe_fec_map(tp.params.paramsS.fec);
			pInfo->Modulation =
			mxl_to_stfe_modulation(tp.params.paramsS.modulation);
			pInfo->RollOff =
				mxl_to_stfe_r_off(tp.params.paramsS.rollOff);
		}
		pInfo->Spectrum = mxl_to_stfe_spectrum(tp.spectrumInfo);
	}

	/* Wait for 1 seconds */
	ChipWaitOrAbort(p->hDemod, 1000);/*??*/

	stm_fe_mutex_lock(lock_mxl);
	p->status = MxLWare_HYDRA_API_ReqDemodSNR(HYDRA_DEV_ID,
					(MXL_HYDRA_DEMOD_ID_E)Demod,
					(SINT16 *)&cnr);
	stm_fe_mutex_unlock(lock_mxl);
	if (p->status != MXL_SUCCESS) {
		pr_err("%s :MxLWare_HYDRA_API_ReqDemodSNR status =%d\n",
							__func__, p->status);
		return FE_LLA_BAD_PARAMETER;
	}
	if (p->status == MXL_SUCCESS)
		pInfo->C_N = cnr;

	stm_fe_mutex_lock(lock_mxl);
	p->status = MxLWare_HYDRA_API_ReqDemodRxPowerLevel(HYDRA_DEV_ID,
					(MXL_HYDRA_DEMOD_ID_E)Demod,
					(SINT32 *)&rf);
	stm_fe_mutex_unlock(lock_mxl);
	if (p->status != MXL_SUCCESS) {
		pr_err("%s :MxLWare_HYDRA_API_ReqDemodSNR status = %d\n",
						__func__, p->status);
		return FE_LLA_BAD_PARAMETER;
	}
	if (p->status == MXL_SUCCESS)
		pInfo->Power = rf/100;

	return FE_LLA_NO_ERROR;
}
/*****************************************************
--FUNCTION	::	FE_MXL584_Tracking
--ACTION	::	Return Tracking informations : lock status, RF level,
			C/N and BER.
--PARAMS IN	::	Handle	==>	Front End Handle
		::	Demod	==>	Current demod 1/2/3/4/5/6/7/8
--PARAMS OUT::	pTrackingInfo	==> pointer to FE_Sat_TrackingInfo_t struct.
		::
--RETURN	::	Error (if any)
--***************************************************/
FE_MXL584_Error_t FE_MXL584_Tracking(FE_MXL584_Handle_t Handle,
					FE_MXL584_DEMOD_t Demod,
					FE_Sat_TrackingInfo_t *pTrackingInfo)
{

	return FE_LLA_NO_ERROR;
}

/*****************************************************
--FUNCTION	::	FE_MXL584_Status
--ACTION	::	Return locked status
--PARAMS IN	::	Handle	==>	Front End Handle
		::Demod	==>	Current demod 1/2/3/4/5/6/7/8
--PARAMS OUT	::	NONE
--RETURN	::Bool (locked or not)
--***************************************************/
BOOL	FE_MXL584_Status(FE_MXL584_Handle_t Handle, FE_MXL584_DEMOD_t Demod)
{
	FE_MXL584_InternalParams_t *p = NULL;
	MXL_HYDRA_DEMOD_LOCK_T  lock;
	U8 num_trials = 2, index = 0;

	p = (FE_MXL584_InternalParams_t *) Handle;
	lock.fecLock = FALSE;/*Initial lock is FALSE*/

	if (p == NULL)
		return FALSE;

	while (((index) < num_trials) && (lock.fecLock != (MXL_BOOL_E)TRUE)) {
		ChipWaitOrAbort(p->hDemod, 1000);
		stm_fe_mutex_lock(lock_mxl);
		p->status =
		  MxLWare_HYDRA_API_ReqDemodLockStatus(HYDRA_DEV_ID, Demod,
							&lock);
		stm_fe_mutex_unlock(lock_mxl);
		if (p->status != MXL_SUCCESS) {
			pr_err("%s :_HYDRA_API_ReqDemodLockStatus status =%d\n",
					__func__, p->status);
			return FE_LLA_I2C_ERROR;
		}
		index++;
	}

	if ((p->status == MXL_SUCCESS) &&
		(lock.fecLock == (MXL_BOOL_E)TRUE))
		return lock.fecLock;
	else
		return FALSE;

}

/*****************************************************
--FUNCTION	::	FE_MXL584_Unlock
--ACTION	::	Unlock the demodulator , set the demod to idle state
--PARAMS IN	::	Handle	==>Front End Handle
		::	Demod	==>Current demod 1 or 2
-PARAMS OUT	::	NONE.
--RETURN	::	Error (if any)
--***************************************************/
FE_MXL584_Error_t FE_MXL584_Unlock(FE_MXL584_Handle_t Handle,
					FE_MXL584_DEMOD_t Demod)
{
	FE_MXL584_InternalParams_t *p;
	p = (FE_MXL584_InternalParams_t *)Handle;

	if (p == NULL)
		return FE_LLA_INVALID_HANDLE;

	stm_fe_mutex_lock(lock_mxl);
	p->status = MxLWare_HYDRA_API_CfgDemodDisable(HYDRA_DEV_ID, Demod);
	stm_fe_mutex_unlock(lock_mxl);
	if (p->status != MXL_SUCCESS) {
		pr_err("%s :MxLWare_HYDRA_API_CfgDemodDisable status =%d\n",
							__func__, p->status);
		return FE_LLA_I2C_ERROR;
	}
	if (p->hDemod->Error)
		return FE_LLA_I2C_ERROR;

	return FE_LLA_NO_ERROR;
}
/*****************************************************
--FUNCTION	::	FE_MXL584_SetStandby
--ACTION	::	Set demod STANDBAY mode On/Off
--PARAMS IN	::	Handle	==>	Front End Handle

-PARAMS OUT::	NONE.
--RETURN	::	Error (if any)
- Ajout demod et separe les 2
--***************************************************/
FE_MXL584_Error_t FE_MXL584_SetStandby(FE_MXL584_Handle_t Handle,
					U8 StandbyOn, FE_MXL584_DEMOD_t Demod)
{
	FE_MXL584_InternalParams_t *p ;
	p = (FE_MXL584_InternalParams_t	*)Handle;

	if (p == NULL)
		return FE_LLA_INVALID_HANDLE;

	if (StandbyOn) {
		stm_fe_mutex_lock(lock_mxl);
		p->status = MxLWare_HYDRA_API_CfgDevPowerMode(HYDRA_DEV_ID,
						MXL_HYDRA_PWR_MODE_STANDBY);
		stm_fe_mutex_unlock(lock_mxl);
		if (p->status != MXL_SUCCESS) {
			pr_err("%s :_HYDRA_API_CfgDevPowerMode Status =%d\n",
					__func__, p->status);
			return FE_LLA_I2C_ERROR;
		}
	} else {
		stm_fe_mutex_lock(lock_mxl);
		p->status = MxLWare_HYDRA_API_CfgDevPowerMode(HYDRA_DEV_ID,
						MXL_HYDRA_PWR_MODE_ACTIVE);
		stm_fe_mutex_unlock(lock_mxl);
		if (p->status != MXL_SUCCESS) {
			pr_err("%s :_HYDRA_API_CfgDevPowerMode status =%d\n",
					__func__, p->status);
			return FE_LLA_I2C_ERROR;
		}
	}
	if (p->hDemod->Error)
		return FE_LLA_I2C_ERROR;
	return FE_LLA_NO_ERROR;
}

/*****************************************************
--FUNCTION	::	FE_MXL584_SetAbortFlag
--ACTION	::	Set Abort flag On/Off
--PARAMS IN	::	Handle	==>	Front End Handle

-PARAMS OUT::	NONE.
--RETURN	::	Error (if any)
ajoute demod et separe les 2
--***************************************************/
FE_MXL584_Error_t FE_MXL584_SetAbortFlag(FE_MXL584_Handle_t Handle,
					BOOL Abort, FE_MXL584_DEMOD_t Demod)
{
	FE_MXL584_InternalParams_t *p;
	p = (FE_MXL584_InternalParams_t *)Handle;

	if (p == NULL)
		return FE_LLA_INVALID_HANDLE;

	ChipAbort(p->hDemod, Abort);
	if (p->hDemod->Error)
		return FE_LLA_I2C_ERROR;


	return FE_LLA_NO_ERROR;
}
/*****************************************************
--FUNCTION	::	FE_MXL584_Search
--ACTION	::	Search for a valid transponder
--PARAMS IN	::	Handle ==> Front End Handle
			sp ==> Search parameters
			rp ==> Result of the search
--PARAMS OUT	::	NONE
--RETURN	::	Error (if any)
--***************************************************/
FE_MXL584_Error_t	FE_MXL584_Search(FE_MXL584_Handle_t Handle,
					FE_MXL584_DEMOD_t Demod,
					FE_MXL584_SearchParams_t *sp,
					FE_MXL584_SearchResult_t *rp)
{
	FE_MXL584_Error_t err = FE_LLA_NO_ERROR;
	FE_MXL584_InternalParams_t *p;
	U8 num_trials = 2, index = 0;
	MXL_HYDRA_DEMOD_STATUS_T d_stats;
	MXL_HYDRA_TUNE_PARAMS_T tp;
	MXL_HYDRA_DEMOD_LOCK_T lock;
	MXL_HYDRA_TUNER_ID_E tuner_id = MXL_HYDRA_TUNER_ID_0;
	MXL_HYDRA_TS_PID_FLT_CTRL_E pid_fltr;
	MXL_HYDRA_TS_PID_T pids;

	if (Handle == NULL)
		return FE_LLA_INVALID_HANDLE;

	rp->Locked = FALSE;

	p = (FE_MXL584_InternalParams_t *) Handle;
	p->DemodSearchStandard = sp->Standard;
	p->DemodSymbolRate = sp->SymbolRate;
	p->DemodSearchRange = sp->SearchRange;
	p->TunerFrequency = sp->Frequency;
	p->DemodSearchAlgo = sp->SearchAlgo;
	p->DemodSearch_IQ_Inv = sp->IQ_Inversion;
	p->DemodPunctureRate = sp->PunctureRate;
	p->DemodModulation = sp->Modulation;
	p->DemodModcode = sp->Modcode;

	dpr_info("%s :D=%d Standard =%d F_Hz=%u SearchRange_Hz=%u\n"
		"SR_Sps=%u Spectrum=%d FEC =%d Mod=%d\n",
		__func__, Demod, p->DemodSearchStandard, p->TunerFrequency,
		p->DemodSearchRange, p->DemodSymbolRate, p->DemodSearch_IQ_Inv,
		p->DemodPunctureRate, p->DemodModulation);

	/* Cfg channel tuner parameters */
	tp.standardMask = stfe_to_mxl_standard(sp->Standard);
	tp.frequencyInHz = sp->Frequency*1000U;
	tp.freqSearchRangeKHz = sp->SearchRange/1000;
	tp.symbolRateKSps = sp->SymbolRate/1000;
	tp.spectrumInfo = MXL_HYDRA_SPECTRUM_NON_INVERTED;
	if (tp.standardMask == MXL_HYDRA_DVBS) {
		tp.params.paramsS.fec = stfe_to_mxl_fec_map(sp->PunctureRate);
		tp.params.paramsS.modulation = MXL_HYDRA_MOD_QPSK;
	} else if (tp.standardMask == MXL_HYDRA_DVBS2) {
		tp.params.paramsS2.fec = stfe_to_mxl_fec_map(sp->PunctureRate);
		tp.params.paramsS2.modulation =
				stfe_to_mxl_modulation(sp->Modulation);
		tp.params.paramsS2.pilots = MXL_HYDRA_PILOTS_AUTO;
	}
	tp.params.paramsS.rollOff = MXL_HYDRA_ROLLOFF_AUTO;
	dpr_info("%s :D =%d ::Tune>Standard =%d Freq_Hz = %u\n"
		"SearchRange_KHz =%u SR_KSps=%u Spectrum =%d\n"
		"FEC =%d Modulation =%d Roll_Off =%d\n",
		__func__, Demod,	tp.standardMask, tp.frequencyInHz,
		tp.freqSearchRangeKHz, tp.symbolRateKSps, tp.spectrumInfo,
		tp.params.paramsS.fec, tp.params.paramsS.modulation,
		tp.params.paramsS.rollOff);

	switch (Demod) {
	default:
	case 0:
	case 1:	/*tuner 0 for Demod 0 and 1*/
		tuner_id = MXL_HYDRA_TUNER_ID_0;
		break;
	case 2:
	case 3: /*tuner 1 for Demod 2 and 3*/
		tuner_id = MXL_HYDRA_TUNER_ID_1;
		break;
	case 4:
	case 5: /*tuner 2 for Demod 4 and 5*/
		tuner_id = MXL_HYDRA_TUNER_ID_2;
		break;
	case 6:
	case 7: /*tuner 3 for Demod 6 and 7*/
		tuner_id = MXL_HYDRA_TUNER_ID_3;
		break;
	}
	MxLWare_HYDRA_API_CfgTunerEnable(HYDRA_DEV_ID, tuner_id, TRUE);

	/*DROP all pids*/
	pid_fltr = MXL_HYDRA_TS_PIDS_DROP_ALL;
	stm_fe_mutex_lock(lock_mxl);
	p->status =
	MxLWare_HYDRA_API_CfgTSPidFilterCtrl(HYDRA_DEV_ID,
	(MXL_HYDRA_DEMOD_ID_E)Demod, pid_fltr);
	if ((p->status != MXL_SUCCESS) &&
	(p->hDemod->Error == CHIPERR_NO_ERROR)) {
		printk(KERN_ERR
			"%s:MxLWare_HYDRA_API_CfgTSPidFilterCtrl =%d\n",
			__func__, p->status);
		return FE_LLA_I2C_ERROR;
	}
	stm_fe_mutex_unlock(lock_mxl);


	while (((index) < num_trials) &&
		(rp->Locked != TRUE)) {
		stm_fe_mutex_lock(lock_mxl);
		p->status = MxLWare_HYDRA_API_CfgDemodChanTune(HYDRA_DEV_ID,
			(MXL_HYDRA_TUNER_ID_E)tuner_id,
			(MXL_HYDRA_DEMOD_ID_E)Demod, &tp);
		stm_fe_mutex_unlock(lock_mxl);
		if ((p->status != MXL_SUCCESS) &&
		   (p->hDemod->Error == CHIPERR_NO_ERROR)) {
			pr_err("%s :MxLWare_HYDRA_API_CfgDemodChanTune =%d\n",
						__func__, p->status);
			return FE_LLA_I2C_ERROR;
		}
		ChipWaitOrAbort(p->hDemod, 1000);/*??*/

		stm_fe_mutex_lock(lock_mxl);
		MxLWare_HYDRA_API_ReqDemodLockStatus(HYDRA_DEV_ID,
			(MXL_HYDRA_DEMOD_ID_E)Demod, &lock);
		stm_fe_mutex_unlock(lock_mxl);
		if (lock.fecLock == MXL_TRUE) {
			rp->Locked = TRUE;
			stm_fe_mutex_lock(lock_mxl);
			MxLWare_HYDRA_API_ReqDemodChanParams(HYDRA_DEV_ID,
				(MXL_HYDRA_DEMOD_ID_E)Demod, &tp);
			if ((p->status != MXL_SUCCESS) ||
			(p->hDemod->Error != CHIPERR_NO_ERROR)) {
				printk(KERN_ERR
				"%s:MxLWare_HYDRA_API_ReqDemodChanParams =%d\n",
				__func__, p->status);
				return FE_LLA_I2C_ERROR;
				}
			MxLWare_HYDRA_API_ReqDemodErrorCounters(HYDRA_DEV_ID,
				(MXL_HYDRA_DEMOD_ID_E)Demod, &d_stats);
			if ((p->status != MXL_SUCCESS) ||
				(p->hDemod->Error != CHIPERR_NO_ERROR)) {
				printk(KERN_ERR
					"%s:MxLWare_HYDRA_API_ReqDemodChanParams =%d\n",
					__func__, p->status);
				return FE_LLA_I2C_ERROR;
				}

				/*If lock Allow PID 0 only
				>>for the case when All pids are Dropped.*/
				pids.originalPid = 0x00;
				pids.remappedPid = 0;
				pids.enable =  TRUE;
				pids.allowOrDrop = TRUE;
				pids.enablePidRemap = FALSE;

				p->status = MxLWare_HYDRA_API_CfgPidFilterTbl
				(HYDRA_DEV_ID, (MXL_HYDRA_DEMOD_ID_E)Demod,
					&pids, 1);
				if ((p->status != MXL_SUCCESS) &&
				  (p->hDemod->Error == CHIPERR_NO_ERROR)) {
					printk(KERN_ERR
					"%s:MxLWare_HYDRA_API_CfgPidFilterTbl =%d\n",
					__func__, p->status);
					return FE_LLA_I2C_ERROR;
				}

			stm_fe_mutex_unlock(lock_mxl);
			dpr_info("%s :D = %d - L {%s, Freq = %d\n"
				"SR= %d,spectrum= %s, Modulation = %s,\n"
				"CR = %s, rolloff = %s}\n", __func__,
				(MXL_HYDRA_DEMOD_ID_E)Demod,
				standard[tp.standardMask],
				tp.frequencyInHz, tp.symbolRateKSps,
				spectrum[tp.spectrumInfo],
				modulation[tp.params.paramsS.modulation],
				fec[tp.params.paramsS.fec],
				rolloff[tp.params.paramsS.rollOff]);

			dpr_info("%s MXL D=%d LOCK {%d, Freq = %d,\n"
				"SRate =%d, spectrum = %d,Mod = %d,\n"
				"CR = %d, rolloff = %d}\n", __func__,
				(MXL_HYDRA_DEMOD_ID_E)Demod,
				tp.standardMask, tp.frequencyInHz,
				tp.symbolRateKSps, tp.spectrumInfo,
				tp.params.paramsS.modulation,
				tp.params.paramsS.fec,
				tp.params.paramsS.rollOff);

			rp->Standard = mxl_to_stfe_standard(tp.standardMask);
			rp->Frequency = tp.frequencyInHz/1000;
			rp->SymbolRate = tp.symbolRateKSps*1000;

			if (rp->Standard == FE_SAT_DVBS2_STANDARD) {
				rp->PunctureRate = mxl_to_stfe_fec_map(
					tp.params.paramsS2.fec);
				rp->ModCode = p->DemodResults.ModCode;
				rp->Pilots = tp.params.paramsS2.pilots;
				rp->FrameLength = p->DemodResults.FrameLength;
				rp->RollOff = mxl_to_stfe_r_off(
					tp.params.paramsS2.rollOff);
				rp->Modulation = mxl_to_stfe_modulation(
					tp.params.paramsS2.modulation);
			} else if (rp->Standard == FE_SAT_DVBS1_STANDARD) {
				rp->PunctureRate =
				mxl_to_stfe_fec_map(tp.params.paramsS.fec);
				rp->Modulation =
					mxl_to_stfe_modulation(
					tp.params.paramsS.modulation);
				rp->RollOff =
				  mxl_to_stfe_r_off(
					tp.params.paramsS.rollOff);
			}
			rp->Spectrum = mxl_to_stfe_spectrum(
							tp.spectrumInfo);

			dpr_info("%s :D =%d - LOCK {%d, Freq = %d SR = %d\n"
				"spect = %d Mod= %d CR = %d, rolloff = %d}\n",
				__func__, (MXL_HYDRA_DEMOD_ID_E)Demod,
				rp->Standard, rp->Frequency, rp->SymbolRate,
				rp->Spectrum, rp->Modulation, rp->PunctureRate,
				rp->RollOff);

			/* Some bug in the TS3 settings.Set TS3 after Demod3
			_Search_ or _tune_ is called.*/
			if (Demod == 3) {
				err = config_mpeg_interface_ts3();
				if ((MXL_STATUS_E)err != MXL_SUCCESS)
					return FE_LLA_BAD_PARAMETER;
			}
		} else {
			rp->Locked = FALSE;
			index++;
		}
	}

return err;
}
FE_MXL584_Error_t FE_MXL584_cfg_pid_fltr(FE_MXL584_Handle_t Handle,
					MXL_HYDRA_DEMOD_ID_E Demod_Number,
					uint32_t pid, BOOL filter_state)
{
	FE_MXL584_Error_t err = FE_LLA_NO_ERROR;
	FE_MXL584_InternalParams_t *pParams = NULL;
	MXL_HYDRA_TS_PID_T pids;
	U8 numPids;
	pParams = (FE_MXL584_InternalParams_t	*) Handle;

	pids.originalPid = pid;
	pids.remappedPid = 0;
	pids.enable =  filter_state;
	pids.allowOrDrop = filter_state;
	pids.enablePidRemap = FALSE;
	dpr_info("\n\n\n<%s> *******originalpid=%d, remappedpid=%d enable=%d allowordrop=%d eanablepidremap=%d..filter_state =%d\n",
		__func__, pids.originalPid, pids.remappedPid, pids.enable,
		pids.allowOrDrop, pids.enablePidRemap, filter_state);

	if (pParams == NULL)
		return FE_LLA_INVALID_HANDLE;

	numPids	= 1;

	stm_fe_mutex_lock(lock_mxl);
	pParams->status = MxLWare_HYDRA_API_CfgPidFilterTbl(
				HYDRA_DEV_ID, Demod_Number, &pids, numPids);
	stm_fe_mutex_unlock(lock_mxl);

	if (pParams->status != MXL_SUCCESS)
		return FE_LLA_I2C_ERROR ;
	else
		return FE_LLA_NO_ERROR ;

return err;
}

#if 0
FE_MXL584_Error_t FE_MXL584_cfg_ts_mux(FE_MXL584_Handle_t Handle,
					MXL_HYDRA_DEMOD_ID_E Demod_Number)
{

	FE_MXL584_Error_t err = FE_LLA_NO_ERROR;
	FE_MXL584_InternalParams_t *pParams = NULL;
	pParams = (FE_MXL584_InternalParams_t	*) Handle;

	if (pParams == NULL)
		return FE_LLA_INVALID_HANDLE;

	stm_fe_mutex_lock(Lock_MxL_RW);
	/*hard codings must be chnaged.*/
	pParams->status =
		MxLWare_HYDRA_API_CfgTSMixMuxMode(HYDRA_DEV_ID,
			MXL_HYDRA_TS_GROUP_0_3,
			MXL_HYDRA_TS_MUX_2_TO_1););
	stm_fe_mutex_unlock(Lock_MxL_RW);
	if (pParams->status != MXL_SUCCESS)
		return FE_LLA_I2C_ERROR ;
	else
		return FE_LLA_NO_ERROR ;

return err;
}
#endif

/*****************************************************
--FUNCTION	::	FE_MXL584_Term
--ACTION	::	Terminate STV0910 chip connection
--PARAMS IN	::	Handle	==>	Front End Handle
--PARAMS OUT::	NONE
--RETURN	::	Error (if any)
--***************************************************/
FE_MXL584_Error_t FE_MXL584_Term(FE_MXL584_Handle_t Handle)
{
	FE_MXL584_InternalParams_t *p = NULL;
	p = (FE_MXL584_InternalParams_t	*) Handle;

	if (p == NULL)
		return FE_LLA_INVALID_HANDLE;

	if (NULL != lock_mxl) {
		stm_fe_free((void **)&lock_mxl);
		pr_err("%s :Mutex free fail:lock_mxl\n", __func__);
		lock_mxl = NULL;
		/*return FE_LLA_BAD_PARAMETER;*/
	}

return FE_LLA_NO_ERROR;
}

MXL_STATUS_E FE_MXL584_downloadFirmware(UINT8 DEV_ID,
					STCHIP_Handle_t *hChipHandle)
{
	MXL_STATUS_E result = MXL_FAILURE;
	int ret = -1;
	const struct firmware *fw;
	UINT8 *fw_file = MXL_HYDRA_DEFAULT_FW;
	STCHIP_Handle_t hChip;
	hChip = (*hChipHandle);

	/* request the firmware, this will block and timeout */
	ret = request_firmware(&fw, fw_file, hChip->dev_i2c->dev.parent);
	if (ret) {
		pr_err("%s :did not find the firmware file. (%s)\n"
			" on firmware-problems. (%d)\n",
			__func__, fw_file, ret);
		return MXL_FAILURE;
	}
	pr_info("%s :downloading firmware from file '%s'\n"
		"fwSize =%u fwdata =0x%x device = 0x%x\n",
		__func__, fw_file , fw->size, (unsigned int) fw->data,
		(unsigned int)hChip->dev_i2c->dev.parent);

	result = MxLWare_HYDRA_API_CfgDevFWDownload(HYDRA_DEV_ID, (U32)fw->size,
						(U8 *)fw->data, NULL);
	if (result != MXL_SUCCESS)
		pr_err("%s : FW download Failure Err = %d ", __func__, result);

	/*release the fw image*/
	release_firmware(fw);

	return result;
}

int config_mpeg_interface_ts3()
{
	MXL_HYDRA_MPEGOUT_PARAM_T ts_cfg;

	ts_cfg.enable = MXL_ENABLE;
	ts_cfg.lsbOrMsbFirst = MXL_HYDRA_MPEG_SERIAL_MSB_1ST;
	ts_cfg.maxMpegClkRate = 66;
	ts_cfg.mpegClkPhase = MXL_HYDRA_MPEG_CLK_PHASE_SHIFT_90_DEG;
	ts_cfg.mpegClkPol = MXL_HYDRA_MPEG_ACTIVE_HIGH;
	ts_cfg.mpegClkType = MXL_HYDRA_MPEG_CLK_GAPPED;
	ts_cfg.mpegErrorIndication =
		MXL_HYDRA_MPEG_ERR_INDICATION_DISABLED;
	ts_cfg.mpegMode = MXL_HYDRA_MPEG_MODE_SERIAL_3_WIRE;
	ts_cfg.mpegSyncPol = MXL_HYDRA_MPEG_ACTIVE_HIGH;
	ts_cfg.mpegSyncPulseWidth = MXL_HYDRA_MPEG_SYNC_WIDTH_BIT;
	ts_cfg.mpegValidPol = MXL_HYDRA_MPEG_ACTIVE_HIGH;

	dpr_info("%s: mxl TS3 configured\n", __func__);

	return MxLWare_HYDRA_API_CfgMpegOutParams(HYDRA_DEV_ID,
				MXL_HYDRA_DEMOD_ID_3, &ts_cfg);

}

FE_MXL584_Error_t FE_MXL584_Taging_Muxing(struct stm_fe_demod_s *priv)
{
	FE_MXL584_InternalParams_t *p = NULL;
	FE_MXL584_Error_t err = FE_LLA_NO_ERROR;
	struct list_head *pos;
	struct stm_fe_demod_s *demod_prev[10];
	U8 i = 7;
	U8 j = 0;
	MXL_HYDRA_TS_MUX_PREFIX_HEADER_T pfix_hdr;

	/* Internal params structure allocation */
	p = (FE_MXL584_InternalParams_t *)(&priv->demod_params);

	if (p == NULL)
		return FE_LLA_INVALID_HANDLE;

	if (priv->demod_id == 3) {
		i = 3;
		list_for_each(pos, priv->list_h) {
			demod_prev[i] =
				list_entry(pos, struct stm_fe_demod_s, list);
			i--;
		}
		if (demod_prev[3]->config->demux_tsin_id
				== demod_prev[2]->config->demux_tsin_id) {

			if (demod_prev[2]->config->demux_tsin_id ==
					demod_prev[1]->config->demux_tsin_id) {
				/*callAPI for4:1muxing+tagging for this group*/
				/* Config TS MUX mode No mux for TS0 to TS7*/
				/* Config TS MUX mode*/
				/*=>> 2:1 mux for TS0 to TS3*/
				stm_fe_mutex_lock(lock_mxl);
				p->status = MxLWare_HYDRA_API_CfgTSMixMuxMode
					(HYDRA_DEV_ID,	MXL_HYDRA_TS_GROUP_0_3,
						MXL_HYDRA_TS_MUX_4_TO_1);
				stm_fe_mutex_unlock(lock_mxl);
				if (p->status != MXL_SUCCESS) {
					pr_err("%s :MxLWare_HYDRA_API_CfgTSMixMuxMode status =%d\n",
					__func__, p->status);
					return FE_LLA_BAD_PARAMETER;
				}
			} else {
				/*call API for 2:1muxing+tagging for thisgroup*/
				/* Config TS MUX mode No mux for TS0 to TS7*/
				/* Config TS MUX mode*/
				/*=>> 2:1 mux for TS0 to TS3*/
				stm_fe_mutex_lock(lock_mxl);
				p->status = MxLWare_HYDRA_API_CfgTSMixMuxMode
					(HYDRA_DEV_ID, MXL_HYDRA_TS_GROUP_0_3,
						MXL_HYDRA_TS_MUX_2_TO_1);
				stm_fe_mutex_unlock(lock_mxl);
				if (p->status != MXL_SUCCESS) {
					pr_err("%s :MxLWare_HYDRA_API_CfgTSMixMuxMode status =%d\n",
					__func__, p->status);
					return FE_LLA_BAD_PARAMETER;
				}
			}
			/*tagging for group 0 to 3*/
			for (j = MXL_HYDRA_DEMOD_ID_0 ;
					j <= MXL_HYDRA_DEMOD_ID_3; j++) {
				pfix_hdr.enable = TRUE;
				pfix_hdr.numByte = 8;/* 8 byte header added*/

				/*the added tag has value of Demod_no.*/
				memset(pfix_hdr.header, 0, 12);
				pfix_hdr.header[6] = j; /* endianess */
				/*uint8_t arr =0;
				printk("\n");
				for(arr =0 ; arr<8; arr++) {
				printk("%s	pfix_hdr.header[%d]\n",
					__func__,pfix_hdr.header[arr]);
				}*/
				stm_fe_mutex_lock(lock_mxl);
				p->status =
				MxLWare_HYDRA_API_CfgTsMuxPrefixExtraTsHeader
					(HYDRA_DEV_ID, (MXL_HYDRA_DEMOD_ID_E)j,
						&pfix_hdr);
				stm_fe_mutex_unlock(lock_mxl);
				if (p->status != MXL_SUCCESS) {
					printk(KERN_ERR
					"%s:HYDRA_API_CfgTsMuxPrefixExtraTsHeader=%d\n",
							__func__, p->status);
					return FE_LLA_BAD_PARAMETER;
				}
			}

		} else {
		/*NO muxing*/
			stm_fe_mutex_lock(lock_mxl);
			p->status = MxLWare_HYDRA_API_CfgTSMixMuxMode
				(HYDRA_DEV_ID, MXL_HYDRA_TS_GROUP_0_3,
						MXL_HYDRA_TS_MUX_DISABLE);
			stm_fe_mutex_unlock(lock_mxl);
			if (p->status != MXL_SUCCESS) {
				pr_err("%s :MxLWare_HYDRA_API_CfgTSMixMuxMode status =%d\n",
				__func__, p->status);
				return FE_LLA_BAD_PARAMETER;
			}
		}

		FE_MXL584_SetTSout(priv);
	}


	if (priv->demod_id == 7) {
		i = 7;
		list_for_each(pos, priv->list_h) {
			demod_prev[i] =
				list_entry(pos, struct stm_fe_demod_s, list);
			i--;
		}
		if (demod_prev[7]->config->demux_tsin_id ==
					demod_prev[6]->config->demux_tsin_id) {
			if (demod_prev[6]->config->demux_tsin_id  ==
					demod_prev[5]->config->demux_tsin_id) {
				/*call API for 4:1 muxing+taggingforthis group*/
				/* Config TS MUX mode No mux for TS0 to TS7*/
				/* Config TS MUX mode*/
				/*=>> 2:1 mux for TS0 to TS3*/
				stm_fe_mutex_lock(lock_mxl);
				p->status = MxLWare_HYDRA_API_CfgTSMixMuxMode
				(HYDRA_DEV_ID, MXL_HYDRA_TS_GROUP_4_7,
						MXL_HYDRA_TS_MUX_4_TO_1);
				stm_fe_mutex_unlock(lock_mxl);
				if (p->status != MXL_SUCCESS) {
					pr_err("%s :MxLWare_HYDRA_API_CfgTSMixMuxMode status =%d\n",
					__func__, p->status);
					return FE_LLA_BAD_PARAMETER;
				}
			} else {
				/*call API for2:1muxing+tagging for this group*/
				/* Config TS MUX mode No mux for TS0 to TS7*/
				/* Config TS MUX mode*/
				/*=>> 2:1 mux for TS0 to TS3*/
				stm_fe_mutex_lock(lock_mxl);
				p->status = MxLWare_HYDRA_API_CfgTSMixMuxMode
				(HYDRA_DEV_ID, MXL_HYDRA_TS_GROUP_4_7,
						MXL_HYDRA_TS_MUX_2_TO_1);
				stm_fe_mutex_unlock(lock_mxl);
				if (p->status != MXL_SUCCESS) {
					pr_err("%s :MxLWare_HYDRA_API_CfgTSMixMuxMode status =%d\n",
					__func__, p->status);
					return FE_LLA_BAD_PARAMETER;
				}
			}
			/*tagging for group 4 to 7*/
			for (j = MXL_HYDRA_DEMOD_ID_4 ;
					j <= MXL_HYDRA_DEMOD_ID_7; j++) {

				pfix_hdr.enable = TRUE;
				pfix_hdr.numByte = 8;/* 8 byte header added*/

				/*the added tag has value of Demod_no.*/
				memset(pfix_hdr.header, 0, 12);
				pfix_hdr.header[6] = j; /* endianess */
				/*uint8_t arr =0;
				printk("\n");
				for(arr =0 ; arr<8; arr++) {
				printk("%s	pfix_hdr.header[%d]\n",
					__func__,pfix_hdr.header[arr]);
				}*/
				stm_fe_mutex_lock(lock_mxl);
				p->status =
				MxLWare_HYDRA_API_CfgTsMuxPrefixExtraTsHeader
				(HYDRA_DEV_ID, (MXL_HYDRA_DEMOD_ID_E)j,
							&pfix_hdr);
				stm_fe_mutex_unlock(lock_mxl);
				if (p->status != MXL_SUCCESS) {
					printk(KERN_ERR
					"%s:HYDRA_API_CfgTsMuxPrefixExtraTsHeader=%d\n",
							__func__, p->status);
					return FE_LLA_BAD_PARAMETER;
				}
			}

		} else {
			/*NO muxing*/
			stm_fe_mutex_lock(lock_mxl);
			p->status = MxLWare_HYDRA_API_CfgTSMixMuxMode
			(HYDRA_DEV_ID, MXL_HYDRA_TS_GROUP_4_7,
					MXL_HYDRA_TS_MUX_DISABLE);
			stm_fe_mutex_unlock(lock_mxl);
			if (p->status != MXL_SUCCESS) {
				pr_err("%s :MxLWare_HYDRA_API_CfgTSMixMuxMode status =%d\n",
						__func__, p->status);
				return FE_LLA_BAD_PARAMETER;
			}
		}
				FE_MXL584_SetTSout(priv);
		}

return err;
}

FE_MXL584_Error_t FE_MXL584_SetTSout(struct stm_fe_demod_s *priv)
{
	FE_MXL584_InternalParams_t *p = NULL;
	FE_MXL584_Error_t err = FE_LLA_NO_ERROR;

	MXL_HYDRA_MPEGOUT_PARAM_T ts_cfg;
	U8 j = 0;

	/* Internal params structure allocation */
	p = (FE_MXL584_InternalParams_t *)(&priv->demod_params);

	if (p == NULL)
		return FE_LLA_INVALID_HANDLE;

	/* Config TS interface of the device*/
	ts_cfg.enable = MXL_ENABLE;
	ts_cfg.lsbOrMsbFirst = MXL_HYDRA_MPEG_SERIAL_MSB_1ST;
	ts_cfg.maxMpegClkRate = 104;
	ts_cfg.mpegClkPhase = MXL_HYDRA_MPEG_CLK_PHASE_SHIFT_90_DEG;
	ts_cfg.mpegClkPol = MXL_HYDRA_MPEG_ACTIVE_HIGH;
	ts_cfg.mpegClkType = MXL_HYDRA_MPEG_CLK_GAPPED;
	ts_cfg.mpegErrorIndication = MXL_HYDRA_MPEG_ERR_INDICATION_DISABLED;
	ts_cfg.mpegMode = MXL_HYDRA_MPEG_MODE_SERIAL_3_WIRE;
	ts_cfg.mpegSyncPol = MXL_HYDRA_MPEG_ACTIVE_HIGH;
	ts_cfg.mpegSyncPulseWidth = MXL_HYDRA_MPEG_SYNC_WIDTH_BIT;
	ts_cfg.mpegValidPol = MXL_HYDRA_MPEG_ACTIVE_HIGH;

	for (j = MXL_HYDRA_DEMOD_ID_0; j < MXL_HYDRA_DEMOD_MAX; j++) {
		/*if (j == MXL_HYDRA_DEMOD_ID_0)
			ts_cfg.mpegClkPhase =
				MXL_HYDRA_MPEG_CLK_PHASE_SHIFT_90_DEG;
		else
			ts_cfg.mpegClkPhase =
				MXL_HYDRA_MPEG_CLK_PHASE_SHIFT_90_DEG;
		*/
		if (j == MXL_HYDRA_DEMOD_ID_3)
				ts_cfg.maxMpegClkRate = 66;
		else
				ts_cfg.maxMpegClkRate = 104;
		stm_fe_mutex_lock(lock_mxl);
		p->status = MxLWare_HYDRA_API_CfgMpegOutParams
		(HYDRA_DEV_ID, (MXL_HYDRA_DEMOD_ID_E)j, &ts_cfg);
		stm_fe_mutex_unlock(lock_mxl);
		if (p->status != MXL_SUCCESS) {
			pr_err("%s :_HYDRA_API_CfgMpegOutParams status = %d\n",
						__func__, p->status);
			return FE_LLA_BAD_PARAMETER;
		}
		dpr_info("%s :<D=%d> En = %d lsbOrMsbFirst = %d\n"
			"maxMpegClkRate = %d  mpegClkPhase =  %d\n"
			"mpegClkPol =  %d	mpegClkType = %d\n"
			"mpegErrorIndication = %d mpegMode = %d\n"
			"mpegSyncPol = %d mpegSyncPulseWidth = %d\n"
			"mpegValidPol = %d\n ", __func__, j,
			ts_cfg.enable, ts_cfg.lsbOrMsbFirst,
			ts_cfg.maxMpegClkRate, ts_cfg.mpegClkPhase,
			ts_cfg.mpegClkPol, ts_cfg.mpegClkType,
			ts_cfg.mpegErrorIndication,
			ts_cfg.mpegMode, ts_cfg.mpegSyncPol,
			ts_cfg.mpegSyncPulseWidth, ts_cfg.mpegValidPol);

	}
return err;
}

MXL_HYDRA_FEC_E stfe_to_mxl_fec_map(FE_Sat_Rate_t FECRate)
{
	MXL_HYDRA_FEC_E mxl_fec = MXL_HYDRA_FEC_AUTO;
	switch (FECRate) {
	case FE_SAT_PR_UNKNOWN:
	default:
		mxl_fec = MXL_HYDRA_FEC_AUTO;
		break;
	case FE_SAT_PR_1_2:
		mxl_fec = MXL_HYDRA_FEC_1_2;
		break;
	/*case FE_SAT_PR_3_5: mxl_fec = MXL_HYDRA_FEC_3_5;
	break;*/
	case FE_SAT_PR_2_3:
		mxl_fec = MXL_HYDRA_FEC_2_3;
		break;
	case FE_SAT_PR_3_4:
		mxl_fec = MXL_HYDRA_FEC_3_4;
		break;
	case FE_SAT_PR_4_5:
		mxl_fec = MXL_HYDRA_FEC_4_5;
		break;
	case FE_SAT_PR_5_6:
		mxl_fec = MXL_HYDRA_FEC_5_6;
		break;
	case FE_SAT_PR_6_7:
		mxl_fec = MXL_HYDRA_FEC_6_7;
		break;
	case FE_SAT_PR_7_8:
		mxl_fec = MXL_HYDRA_FEC_7_8;
		break;
	case FE_SAT_PR_8_9:
		mxl_fec = MXL_HYDRA_FEC_8_9;
		break;
	/*    case FE_SAT_PR_9_10: mxl_fec = MXL_HYDRA_FEC_9_10;
	break;*/
	}
	return mxl_fec;
}

FE_Sat_Rate_t mxl_to_stfe_fec_map(MXL_HYDRA_FEC_E FECRate)
{
	FE_Sat_Rate_t stfe_fec = FE_SAT_PR_UNKNOWN;
	switch (FECRate) {
	case MXL_HYDRA_FEC_AUTO:
	default:
		stfe_fec = FE_SAT_PR_UNKNOWN;
		break;
	case MXL_HYDRA_FEC_1_2:
		stfe_fec = FE_SAT_PR_1_2;
		break;
	/*case FE_SAT_PR_3_5: stfe_fec = MXL_HYDRA_FEC_3_5;
	break;
	case MXL_HYDRA_FEC_1_2: stfe_fec = FE_SAT_PR_1_2;
	break;*/
	case MXL_HYDRA_FEC_3_4:
		stfe_fec = FE_SAT_PR_3_4;
		break;
	case MXL_HYDRA_FEC_4_5:
		stfe_fec = FE_SAT_PR_4_5;
		break;
	case MXL_HYDRA_FEC_5_6:
		stfe_fec = FE_SAT_PR_5_6;
		break;
	case MXL_HYDRA_FEC_6_7:
		stfe_fec = FE_SAT_PR_6_7;
		break;
	case MXL_HYDRA_FEC_7_8:
		stfe_fec = FE_SAT_PR_7_8;
		break;
	case MXL_HYDRA_FEC_8_9:
		stfe_fec = FE_SAT_PR_8_9;
		break;
	/*    case FE_SAT_PR_9_10: stfe_fec = MXL_HYDRA_FEC_9_10;
	break;*/
	}
	return stfe_fec;
}

MXL_HYDRA_BCAST_STD_E
	stfe_to_mxl_standard(FE_Sat_SearchStandard_t Standard)
{
	MXL_HYDRA_BCAST_STD_E SearchStandard;
	switch (Standard) {
	case FE_SAT_AUTO_SEARCH:
		SearchStandard = MXL_HYDRA_DVBS ;
		break;
	default:
	case FE_SAT_SEARCH_DVBS1:
		SearchStandard = MXL_HYDRA_DVBS ;
		break;
	case FE_SAT_SEARCH_DVBS2:
		SearchStandard = MXL_HYDRA_DVBS2 ;
		break;
	case FE_SAT_SEARCH_DSS:
		SearchStandard = MXL_HYDRA_DSS ;
		break;
	}
	return SearchStandard;
}

FE_Sat_TrackingStandard_t
	mxl_to_stfe_standard(MXL_HYDRA_BCAST_STD_E Standard)
{
	FE_Sat_TrackingStandard_t TrackingStandard;
	switch (Standard) {
	case MXL_HYDRA_DVBS:
		TrackingStandard = FE_SAT_DVBS1_STANDARD ;
		break;
	default:
	case MXL_HYDRA_DVBS2:
		TrackingStandard = FE_SAT_DVBS2_STANDARD ;
		break;
	case MXL_HYDRA_DSS:
		TrackingStandard = FE_SAT_DSS_STANDARD ;
		break;
	}
	return TrackingStandard;
}

FE_Sat_RollOff_t  mxl_to_stfe_r_off(MXL_HYDRA_ROLLOFF_E r_off_mxl)
{
	FE_Sat_RollOff_t RollOff_SAT ;
	switch (r_off_mxl) {
	case MXL_HYDRA_ROLLOFF_0_20:
		RollOff_SAT = FE_SAT_20 ;
		break;
	case MXL_HYDRA_ROLLOFF_0_25:
		RollOff_SAT = FE_SAT_25 ;
		break;
	default:
	case MXL_HYDRA_ROLLOFF_AUTO:
	case MXL_HYDRA_ROLLOFF_0_35:
		RollOff_SAT = FE_SAT_35 ;
		break;
	}
	return RollOff_SAT;
}

FE_Sat_IQ_Inversion_t
	mxl_to_stfe_spectrum(MXL_HYDRA_SPECTRUM_E spectrum_mxl)
{
	FE_Sat_IQ_Inversion_t Spectrum_SAT;
	switch (spectrum_mxl)	{
	case MXL_HYDRA_SPECTRUM_INVERTED:
		Spectrum_SAT = FE_SAT_IQ_SWAPPED;
		break;
	default:
	case MXL_HYDRA_SPECTRUM_AUTO:
	case MXL_HYDRA_SPECTRUM_NON_INVERTED:
		Spectrum_SAT = FE_SAT_IQ_NORMAL ;
		break;
	}
	return Spectrum_SAT;
}


MXL_HYDRA_MODULATION_E
	stfe_to_mxl_modulation(FE_Sat_Modulation_t Modulation)
{
	MXL_HYDRA_MODULATION_E mxl_mod;
	switch (Modulation) {
	default:
	case FE_SAT_MOD_UNKNOWN:
		mxl_mod = MXL_HYDRA_MOD_AUTO;
		break;
	case FE_SAT_MOD_QPSK:
		mxl_mod = MXL_HYDRA_MOD_QPSK;
		break;
	case FE_SAT_MOD_8PSK:
		mxl_mod = MXL_HYDRA_MOD_8PSK;
		break;
	}
	return mxl_mod;
}

FE_Sat_Modulation_t
	mxl_to_stfe_modulation(MXL_HYDRA_MODULATION_E Modulation)
{
	FE_Sat_Modulation_t stfe_mod;
	switch (Modulation) {
	default:
	case MXL_HYDRA_MOD_AUTO:
		stfe_mod = FE_SAT_MOD_UNKNOWN;
		break;
	case MXL_HYDRA_MOD_QPSK:
		stfe_mod = FE_SAT_MOD_QPSK;
		break;
	case MXL_HYDRA_MOD_8PSK:
		stfe_mod = FE_SAT_MOD_8PSK;
		break;
	}
	return stfe_mod;
}
