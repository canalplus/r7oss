/************************************************************************
Copyright (C) 2014 STMicroelectronics. All Rights Reserved.

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

Source file name : mxl214_drv.c
Author :

Low level driver for MXL214

Date        Modification                                    Name
----        ------------                                    --------
30-JUN-14   Created						MS

***************************************************************************/

/* includes ---------------------------------------------------------- */
#include <linux/version.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 2, 2)
#include <linux/export.h>
#else
#include <linux/module.h>
#endif
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/list.h>
#include <linux/semaphore.h>
#include <stm_fe.h>
#include <stm_fe_os.h>
#include <i2c_wrapper.h>
#include <stm_fe_demod.h>
#include <linux/firmware.h>/*linux firmware framework*/
#include "d0mxl214.h"
#include "mxl214_init.h"
#include "mxl214_drv.h"
#include <fecab_commlla_str.h>
#include <mxl_hrcls_demodapi.h>
#include "mxl_hrcls_oem_drv.h"


static int lla_wrapper_debug2;
module_param_named(lla_wrapper_debug2, lla_wrapper_debug2, int, 0644);
MODULE_PARM_DESC(lla_wrapper_debug2,
		 "Turn on/off mxl214 lla wrapper_2 debugging (default:off).");
#define dpr_info(x...) do { if (lla_wrapper_debug2) pr_info(x); } while (0)
#define dpr_err(x...) do { if (lla_wrapper_debug2) pr_err(x); } while (0)

/*****************************************************
*  Device ID used in this sample code.
* If more Hrcls devices are used at the same time,
 *they should have  consecutive device ID's
******************************************************/
#define MXL_HRCLS_DEVICE_ID 0
/******************************************************************************
  Default carystal capacitors value. This value is platform dependent.
 *****************************************************************************/
#define HRCLS_XTAL_CAP_VALUE 0

/******************************************************************************
  Firmware filename.
 ******************************************************************************/
#define HRCLS_FIRMWARE_FILENAME "MxL_HRCLS_FW.mbin"


/************************
Current LLA revision
*************************/
static const ST_Revision_t RevisionMXL214  = "MXL214-LLA_REL_4_1_5_4";

static DEFINE_MUTEX(lock_mxl);

static MXL_STATUS_E FE_MXL214_checkVersion(MXL_HRCLS_DEV_VER_T *verInfoPtr);
static MXL_STATUS_E mxl_waitForTunerLock(MXL_HRCLS_TUNER_ID_E tunerId);
static MXL_HRCLS_QAM_TYPE_E
	stfe_to_mxl214_modulation(FE_CAB_Modulation_t Modulation);
static FE_CAB_Modulation_t
	mxl214_to_stfe_modulation(MXL_HRCLS_QAM_TYPE_E Modulation);
static MXL_HRCLS_ANNEX_TYPE_E stfe_to_mxl214_annex(FE_CAB_FECType_t Annex);
/*static FE_CAB_FECType_t mxl214_to_stfe_annex(MXL_HRCLS_ANNEX_TYPE_E Annex);*/
static MXL_STATUS_E mxl_lockDemod(MXL_HRCLS_CHAN_ID_E chanId,
		MXL_HRCLS_DMD_ID_E demodId, UINT32 freqkHz, UINT8 bwMHz,
		MXL_HRCLS_ANNEX_TYPE_E annexType, MXL_HRCLS_QAM_TYPE_E qamType,
		UINT16 symbolRatekSps, MXL_HRCLS_IQ_FLIP_E iqFlip);
static MXL_STATUS_E mxl_waitForChannelLock(MXL_HRCLS_CHAN_ID_E chanId);
static MXL_STATUS_E FE_MXL214_downloadFirmware(UINT8 DEV_ID,
					STCHIP_Handle_t *hChipHandle);
static MXL_STATUS_E mxl_enableFbTuner(void);

/*****************************************************
--FUNCTION	::	FE_MXL214_GetRevision
--ACTION	::	Return current LLA version
--PARAMS IN	::	NONE
--PARAMS OUT::	NONE
--RETURN	::	Revision ==> Text string containing LLA version
--***************************************************/
ST_Revision_t FE_MXL214_GetRevision(void)
{
	return RevisionMXL214;
}

/*****************************************************
--FUNCTION	::	FE_MXL214_Init
--ACTION	::	Initialisation of the MXL214 chip
--PARAMS IN	::	pInit	==>	Front End Init parameters
--PARAMS OUT::	NONE
--RETURN	::	Handle to MXL214
--***************************************************/
FE_MXL214_Error_t FE_MXL214_Init(FE_CAB_InitParams_t	*pInit,
				 FE_MXL214_Handle_t *Handle)
{
	FE_MXL214_InternalParams_t *p = NULL;
	/* Demodulator chip initialisation parameters */
	Demod_InitParams_t d_init_p;
	STCHIP_Error_t  d_err = CHIPERR_NO_ERROR;
	MXL_HRCLS_DEV_VER_T devVerInfo;
	FE_MXL214_Error_t err = FE_LLA_NO_ERROR;

	/* Internal params structure allocation */
	p = (FE_MXL214_InternalParams_t *)(*Handle);

	if (p == NULL)
		return FE_LLA_INVALID_HANDLE;

	/* Chip initialisation/Demodulator  Init */
	d_init_p.Chip = (p->hDemod);
	d_init_p.NbDefVal = MXL214_HRCLS_NBREGS;
	d_init_p.Chip->RepeaterHost = NULL;
	d_init_p.Chip->RepeaterFn = NULL;
	d_init_p.Chip->Repeater = FALSE;
	d_init_p.Chip->I2cAddr = pInit->DemodI2cAddr;
	strcpy((char *)d_init_p.Chip->Name, pInit->DemodName);

	p->status = MXL_SUCCESS;

	d_err = MXL214_Init(&d_init_p, &(p->hDemod));

	stm_fe_mutex_lock(&lock_mxl);
	p->status = MxLWare_HRCLS_API_CfgDrvInit(MXL_HRCLS_DEVICE_ID, p->hDemod,
						MXL_HRCLS_DEVICE_214);
	stm_fe_mutex_unlock(&lock_mxl);

	/*check if device already INITIALIZED;
	*has to confirm:need not call below API's for more than 1 instances*/
	if (p->status == MXL_ALREADY_INITIALIZED) {
		pr_info("%s %d:MxLWare_HRCLS_API_CfgDrvInit status =%d\n",
				__func__, __LINE__, p->status);
		return FE_LLA_NO_ERROR;
	} else if (p->status != MXL_SUCCESS) {
		pr_err("%s %d:MxLWare_HRCLS_API_CfgDrvInit status =%d\n",
				 __func__, __LINE__, p->status);
		return FE_LLA_BAD_PARAMETER;
	}

	stm_fe_mutex_lock(&lock_mxl);
	p->status = MxLWare_HRCLS_API_CfgDevReset(MXL_HRCLS_DEVICE_ID);
	stm_fe_mutex_unlock(&lock_mxl);

	if (p->status != MXL_SUCCESS) {
		pr_err("%s %d:MxLWare_HRCLS_API_CfgDevReset status = %d\n",
			__func__, __LINE__, p->status);
		return FE_LLA_BAD_PARAMETER;
	}

	stm_fe_mutex_lock(&lock_mxl);
	p->status = MxLWare_HRCLS_API_CfgDevXtalSetting(MXL_HRCLS_DEVICE_ID,
			HRCLS_XTAL_CAP_VALUE);
	stm_fe_mutex_unlock(&lock_mxl);
	if (p->status != MXL_SUCCESS) {
		pr_err("%s  %d:MxLWare_HRCLS_API_CfgDevXtalSetting status = %d\n",
			__func__, __LINE__, p->status);
		return FE_LLA_BAD_PARAMETER;
	}

	stm_fe_mutex_lock(&lock_mxl);
	p->status =
		MxLWare_HRCLS_API_CfgTunerDsCalDataLoad(MXL_HRCLS_DEVICE_ID);
	stm_fe_mutex_unlock(&lock_mxl);
	if (p->status != MXL_SUCCESS) {
		pr_err("%s  %d:MxLWare_HRCLS_API_CfgTunerDsCalDataLoad status = %d ::Calibration data not available. Power reporting will not be accurate\n",
		__func__, __LINE__, p->status);
		/*dont retrun as only reporting will be not accurate*/
		/*return FE_LLA_BAD_PARAMETER;*/
	}

	stm_fe_mutex_lock(&lock_mxl);
	p->status = FE_MXL214_checkVersion(&devVerInfo);
	stm_fe_mutex_unlock(&lock_mxl);
	if (p->status != MXL_SUCCESS) {
		pr_err("%s  %d Device_ID =%d Version checking FAILED!: mxl_checkVersion status = %d\n",
		__func__, __LINE__, MXL_HRCLS_DEVICE_ID, p->status);
		return FE_LLA_BAD_PARAMETER;
	 }
	/**********************************************************************
	  Make sure firmware is not already downloaded
	 *********************************************************************/
	if (devVerInfo.firmwareDownloaded == MXL_TRUE) {
		pr_err("%s  %d Device_ID =%d Firmware already running.Forgot about h/w reset? :mxl_checkVersion status = %d\n",
			__func__, __LINE__, MXL_HRCLS_DEVICE_ID, p->status);
		return FE_LLA_BAD_PARAMETER;
	}

	stm_fe_mutex_lock(&lock_mxl);
	p->status =
		FE_MXL214_downloadFirmware(MXL_HRCLS_DEVICE_ID, &(p->hDemod));
	stm_fe_mutex_unlock(&lock_mxl);

	if (p->status != MXL_SUCCESS) {
		pr_err("%s  %d FE_MXL214_downloadFirmware status = %d\n",
						__func__, __LINE__, p->status);
		return FE_LLA_BAD_PARAMETER;
	 }

	stm_fe_mutex_lock(&lock_mxl);
	p->status = FE_MXL214_checkVersion(&devVerInfo);
	stm_fe_mutex_unlock(&lock_mxl);
	if (p->status != MXL_SUCCESS) {
		pr_err("%s  %d Device_ID =%d Version checking FAILED!: mxl_checkVersion status = %d\n",
		__func__, __LINE__, MXL_HRCLS_DEVICE_ID, p->status);
		return FE_LLA_BAD_PARAMETER;
	 }
#if 0
	stm_fe_mutex_lock(&lock_mxl);
	p->status = MxLWare_HRCLS_API_CfgTunerEnable(MXL_HRCLS_DEVICE_ID,
		MXL_HRCLS_FULLBAND_TUNER);
	stm_fe_mutex_unlock(&lock_mxl);
	if (p->status != MXL_SUCCESS) {
		pr_err("%s Enable FB tuner FAILED:MxLWare_HRCLS_API_CfgTunerEnable status = %d\n",
		__func__, __LINE__, p->status);
		return FE_LLA_BAD_PARAMETER;
	}

	stm_fe_mutex_lock(&lock_mxl);
	p->status = mxl_waitForTunerLock(MXL_HRCLS_FULLBAND_TUNER);
	stm_fe_mutex_unlock(&lock_mxl);
	if (p->status != MXL_SUCCESS) {
		pr_err("%s Fullband tuner synth lock TIMEOUT:mxl_waitForTunerLock status = %d\n",
		__func__, __LINE__, p->status);
		return FE_LLA_BAD_PARAMETER;
	 }
#endif
	stm_fe_mutex_lock(&lock_mxl);
	p->status = mxl_enableFbTuner();
	stm_fe_mutex_unlock(&lock_mxl);
	if (p->status != MXL_SUCCESS) {
		pr_err("%s  %d Device_ID =%d Enable FB tuner FAILED:mxl_enableFbTuner= %d\n",
			__func__, __LINE__, MXL_HRCLS_DEVICE_ID, p->status);
		return FE_LLA_BAD_PARAMETER;
	}
	/*This step can be skipped as NO_MUX_4 is a default mode*/
	stm_fe_mutex_lock(&lock_mxl);
	p->status = MxLWare_HRCLS_API_CfgXpt(MXL_HRCLS_DEVICE_ID,
					MXL_HRCLS_XPT_MODE_NO_MUX_4);
	stm_fe_mutex_unlock(&lock_mxl);

	if (p->status != MXL_SUCCESS) {
		pr_err("%s  %d Cfg XPT mux mode FAILED :MxLWare_HRCLS_API_CfgXpt status = %d\n",
			__func__, __LINE__, p->status);
		return FE_LLA_BAD_PARAMETER;
	}


	/*Only needed for 4-wire TS mode, skip for 3-wire mode*/
	stm_fe_mutex_lock(&lock_mxl);
	p->status  =
	MxLWare_HRCLS_API_CfgDemodMpegOutGlobalParams(MXL_HRCLS_DEVICE_ID,
		MXL_HRCLS_MPEG_CLK_POSITIVE, MXL_HRCLS_MPEG_DRV_MODE_2X,
					MXL_HRCLS_MPEG_CLK_56_21MHz);
	stm_fe_mutex_unlock(&lock_mxl);

	if (p->status != MXL_SUCCESS) {
		pr_err("%s  %d Global MPEG params setting FAILED :MxLWare_HRCLS_API_CfgDemodMpegOutGlobalParams status = %d\n",
			__func__, __LINE__, p->status);
		return FE_LLA_BAD_PARAMETER;
	}

	return err;
}
/*****************************************************
--FUNCTION	::	FE_MXL214_Search
--ACTION	::	Search for a valid transponder
--PARAMS IN	::	Handle ==> Front End Handle
			sp ==> Search parameters
			rp ==> Result of the search
--PARAMS OUT	::	NONE
--RETURN	::	Error (if any)
--***************************************************/
FE_MXL214_Error_t	FE_MXL214_Search(FE_MXL214_Handle_t Handle,
					FE_MXL214_DEMOD_t Demod,
					FE_MXL214_SearchParams_t *sp,
					FE_MXL214_SearchResult_t *rp)
{
	FE_MXL214_Error_t err = FE_LLA_NO_ERROR;
	FE_MXL214_InternalParams_t *p;
	UINT8 bwMHz;
	MXL_HRCLS_DMD_ID_E Demod_Id;
	MXL_BOOL_E qamLock, fecLock, mpegLock, relock;
	MXL_HRCLS_ANNEX_TYPE_E annexType;
	MXL_HRCLS_QAM_TYPE_E  qamType;
	UINT16 symbolRatekSps;
	MXL_HRCLS_CHAN_ID_E chanId = 0;

	if (Handle == NULL)
		return FE_LLA_INVALID_HANDLE;

	p = (FE_MXL214_InternalParams_t *) Handle;

	rp->Locked = FALSE;
	p->Frequency_kHz = sp->Frequency_kHz;
	p->SymbolRate_Bds = sp->SymbolRate_Bds;
	p->SearchRange_Hz = sp->SearchRange_Hz;
	p->Modulation = stfe_to_mxl214_modulation(sp->Modulation);
	p->Annex = stfe_to_mxl214_annex(sp->FECType);
	if (p->Annex == MXL_HRCLS_ANNEX_B)
		bwMHz = 6;
	else
		bwMHz = 8;

	symbolRatekSps = sp->SymbolRate_Bds/1000;

	Demod_Id = (MXL_HRCLS_DMD_ID_E)Demod;
	if (Demod_Id == MXL_HRCLS_DEMOD0) {
		chanId = 0;
		pr_info("\n%s  %d demod_id=%d Chanid=%d :\n",
			__func__, __LINE__, Demod_Id, chanId);
	}
	if (Demod_Id == MXL_HRCLS_DEMOD1) {
		chanId = 1;
		pr_info("\n%s  %d demod_id=%d Chanid=%d :\n",
			__func__, __LINE__, Demod_Id, chanId);

	}
	if (Demod_Id == MXL_HRCLS_DEMOD2) {
		chanId = 2;
		pr_info("\n%s  %d demod_id=%d Chanid=%d :\n",
			__func__, __LINE__, Demod_Id, chanId);
	}
	if (Demod_Id == MXL_HRCLS_DEMOD3) {
		chanId = 3;
		pr_info("\n%s  %d demod_id=%d Chanid=%d :\n",
			__func__, __LINE__, Demod_Id, chanId);
	}

	pr_info("%s  %d:tune params for cable :\n", __func__, __LINE__);
	pr_info("%s  %dFreq = %d kHz\n bwMHz = %d Hz\n Annex=%d\n Modulation=%d\n Symbol Rate = %d bds symbolRatekSps=%d MXL_HRCLS_IQ_AUTO\n",
			__func__, __LINE__, p->Frequency_kHz, bwMHz, p->Annex,
			p->Modulation, p->SymbolRate_Bds, symbolRatekSps);

	stm_fe_mutex_lock(&lock_mxl);
	p->status = mxl_lockDemod(chanId, Demod_Id,
		p->Frequency_kHz, bwMHz, p->Annex, p->Modulation,
		symbolRatekSps, MXL_HRCLS_IQ_AUTO);
	ChipWaitOrAbort(p->hDemod, 100);/*to ask*/
	stm_fe_mutex_unlock(&lock_mxl);
	if (p->status != MXL_SUCCESS) {
		pr_err("%s  %d Demod lock FAILED: mxl_lockDemod status = %d\n",
			__func__, __LINE__, p->status);
		return FE_LLA_SEARCH_FAILED;
	}

	stm_fe_mutex_lock(&lock_mxl);
	p->status = MxLWare_HRCLS_API_ReqDemodAllLockStatus(MXL_HRCLS_DEVICE_ID,
		Demod_Id, &qamLock, &fecLock, &mpegLock, &relock);
	stm_fe_mutex_unlock(&lock_mxl);
	if (p->status != MXL_SUCCESS) {
		pr_err("%s  %d Demod lock FAILED: mxl_lockDemod status = %d\n",
		__func__, __LINE__, p->status);
		return FE_LLA_SEARCH_FAILED;
	}

	if ((MXL_TRUE == qamLock) &&
		(MXL_TRUE == fecLock) && (MXL_TRUE == mpegLock)
		&& (MXL_FALSE == relock)) {

		stm_fe_mutex_lock(&lock_mxl);
		p->status = MxLWare_HRCLS_API_CfgUpdateDemodSettings
					(MXL_HRCLS_DEVICE_ID, Demod_Id);
		stm_fe_mutex_unlock(&lock_mxl);
		if (p->status != MXL_SUCCESS) {
			pr_err("%s  %d MxLWare_HRCLS_API_CfgUpdateDemodSettings status = %d\n",
			__func__, __LINE__, p->status);
			return FE_LLA_SEARCH_FAILED;
		}

		rp->Locked = TRUE;
		rp->Frequency_kHz = sp->Frequency_kHz;
		rp->SymbolRate_Bds = sp->SymbolRate_Bds;

		stm_fe_mutex_lock(&lock_mxl);
		p->status = MxLWare_HRCLS_API_ReqDemodAnnexQamType
			(MXL_HRCLS_DEVICE_ID, Demod_Id, &annexType, &qamType);
		stm_fe_mutex_unlock(&lock_mxl);
		if (p->status != MXL_SUCCESS) {
			pr_err("%s  %d MxLWare_HRCLS_API_ReqDemodAnnexQamType status = %d\n",
				__func__, __LINE__, p->status);
			return FE_LLA_SEARCH_FAILED;
		}
		rp->Modulation = mxl214_to_stfe_modulation(qamType);
		/*rp->annex = mxl214_to_stfe_annex(annexType);*/
		p->Modulation = qamType;
		p->Annex = annexType;

		pr_info("\n%s  %d Freq =%d kHz\nSymbolRate_Bds = %d Hz\nModulation = %d bds\nAnnex=%d\n",
			__func__, __LINE__, rp->Frequency_kHz,
			rp->SymbolRate_Bds, rp->Modulation, p->Annex);

	} else {
		rp->Locked = FALSE;
	}

	return err;
}

static MXL_STATUS_E mxl_lockDemod(MXL_HRCLS_CHAN_ID_E chanId,
			MXL_HRCLS_DMD_ID_E demodId, UINT32 freqkHz, UINT8 bwMHz,
			MXL_HRCLS_ANNEX_TYPE_E annexType,
			MXL_HRCLS_QAM_TYPE_E qamType, UINT16 symbolRatekSps,
			MXL_HRCLS_IQ_FLIP_E iqFlip)
{
	MXL_STATUS_E status = MXL_SUCCESS;

	/*XPT/MPEG configuration for MPEG (not OOB) output only*/
	if (annexType !=  MXL_HRCLS_ANNEX_OOB) {
		MXL_HRCLS_XPT_MPEGOUT_PARAM_T mpegXptParams;

		/* sample MPEG configuration*/
		mpegXptParams.enable = MXL_ENABLE;
		mpegXptParams.lsbOrMsbFirst = MXL_HRCLS_MPEG_SERIAL_MSB_1ST;
		mpegXptParams.mpegSyncPulseWidth =
			MXL_HRCLS_MPEG_SYNC_WIDTH_BYTE;
		mpegXptParams.mpegValidPol = MXL_HRCLS_MPEG_ACTIVE_HIGH;
		mpegXptParams.mpegSyncPol = MXL_HRCLS_MPEG_ACTIVE_HIGH;
		mpegXptParams.mpegClkPol = MXL_HRCLS_MPEG_CLK_POSITIVE;
		/* in 4-wire mode, this value has to be the same as the value in
		 * MxLWare_HRCLS_API_CfgDemodMpegOutGlobalParams*/
		mpegXptParams.clkFreq = MXL_HRCLS_MPEG_CLK_56_21MHz;
		/*2x default setting;*/
		mpegXptParams.mpegPadDrv.padDrvMpegSyn =
						MXL_HRCLS_MPEG_DRV_MODE_2X;
		/*2x default setting;*/
		mpegXptParams.mpegPadDrv.padDrvMpegDat =
						MXL_HRCLS_MPEG_DRV_MODE_2X;
		/*2x default setting;*/
		mpegXptParams.mpegPadDrv.padDrvMpegVal =
						MXL_HRCLS_MPEG_DRV_MODE_2X;

		/*In MxL254 and NO_MUX_4 mode, outputId = demodId*/
		/*Check MxLWare API User Guide for other modes' mappings*/
		status = MxLWare_HRCLS_API_CfgXptOutput(MXL_HRCLS_DEVICE_ID,
			(MXL_HRCLS_XPT_OUTPUT_ID_E) demodId, &mpegXptParams);
	}

	if (status != MXL_SUCCESS) {
		pr_err("%s  %d MPEG output cfg FAILED: MxLWare_HRCLS_API_CfgXptOutput status = %d\n",
		__func__, __LINE__, status);
		return MXL_FAILURE;
	}

	status = MxLWare_HRCLS_API_CfgTunerChanTune(MXL_HRCLS_DEVICE_ID,
					chanId, bwMHz, freqkHz * 1000);
	if (status != MXL_SUCCESS) {
		pr_err("%s  %d Tune FAILED CHAN_ID[%d]: MxLWare_HRCLS_API_CfgTunerChanTune status = %d\n",
		__func__, __LINE__, chanId, status);
		return MXL_FAILURE;
	}

	status = mxl_waitForChannelLock(chanId);
	if (status != MXL_SUCCESS) {
		pr_err("%s  %d Tune FAILED CHAN_ID[%d]: mxl_waitForChannelLock status = %d\n",
		__func__, __LINE__, chanId, status);
		return MXL_FAILURE;
	}

	status = MxLWare_HRCLS_API_CfgDemodEnable(MXL_HRCLS_DEVICE_ID,
						demodId, MXL_TRUE);
	if (status != MXL_SUCCESS) {
		pr_err("%s  %d DemodID[%d] enable FAILED Demod: MxLWare_HRCLS_API_CfgDemodEnable status = %d\n",
		__func__, __LINE__, demodId, status);
		return MXL_FAILURE;
	}
	/*AdcIqFlip configuration for MPEG (not OOB) output only*/
	if (annexType !=  MXL_HRCLS_ANNEX_OOB)
		status = MxLWare_HRCLS_API_CfgDemodAdcIqFlip
				(MXL_HRCLS_DEVICE_ID, demodId, iqFlip);

	if (status != MXL_SUCCESS) {
		pr_err("%s  %d Demod[%d] Cfg IQFlip FAILED : MxLWare_HRCLS_API_CfgDemodAdcIqFlip status = %d\n",
		__func__, __LINE__, demodId, status);
		return MXL_FAILURE;
	}

	status = MxLWare_HRCLS_API_CfgDemodAnnexQamType(MXL_HRCLS_DEVICE_ID,
			demodId, annexType, qamType);
	if (status != MXL_SUCCESS) {
		pr_err("%s  %d Demod[%d] Cfg AnnexQamType FAILED : MxLWare_HRCLS_API_CfgDemodAnnexQamType status = %d\n",
		__func__, __LINE__, demodId, status);
		return MXL_FAILURE;
	}

	if (annexType != MXL_HRCLS_ANNEX_OOB)
		status = MxLWare_HRCLS_API_CfgDemodSymbolRate
			(MXL_HRCLS_DEVICE_ID, demodId, symbolRatekSps * 1000,
			symbolRatekSps * 1000);
	 else
		status = MxLWare_HRCLS_API_CfgDemodSymbolRateOOB
			(MXL_HRCLS_DEVICE_ID, demodId,
			(MXL_HRCLS_OOB_SYM_RATE_E) symbolRatekSps);

	if (status != MXL_SUCCESS) {
		pr_err("%s  %d Demod[%d] Cfg SymbolRate FAILED : MxLWare_HRCLS_API_CfgDemodSymbolRate status = %d\n",
		__func__, __LINE__, demodId, status);
		return MXL_FAILURE;
	}

	status = MxLWare_HRCLS_API_CfgDemodRestart(MXL_HRCLS_DEVICE_ID,
			demodId);
	if (status != MXL_SUCCESS) {
		pr_err("%s  %d Demod[%d] restart FAILED : MxLWare_HRCLS_API_CfgDemodRestart status = %d\n",
		__func__, __LINE__, demodId, status);
		return MXL_FAILURE;
	}

	if ((status == MXL_SUCCESS) && (annexType ==  MXL_HRCLS_ANNEX_OOB)) {
		MXL_HRCLS_OOB_CFG_T oobParams;

		/* sample OOB configuration*/
		oobParams.pn23SyncMode       = SYNC_MODE_ERROR;
		oobParams.pn23Feedback       = SYNC_MODE_ERROR;
		oobParams.syncPulseWidth     = MXL_HRCLS_OOB_SYNC_WIDTH_BYTE;
		oobParams.oob3WireModeEnable = MXL_FALSE;
		oobParams.enablePn23Const    = MXL_FALSE;
		oobParams.validPol           = MXL_HRCLS_OOB_ACTIVE_HIGH;
		oobParams.syncPol            = MXL_HRCLS_OOB_ACTIVE_HIGH;
		oobParams.clkPol             = MXL_HRCLS_OOB_CLK_POSITIVE;
		oobParams.oobOutMode         = OOB_CRX_DRX_MODE;
		oobParams.enable             = MXL_TRUE;

		status = MxLWare_HRCLS_API_CfgDemodOutParamsOOB
				(MXL_HRCLS_DEVICE_ID, demodId, &oobParams);
		if (status != MXL_SUCCESS) {
			pr_err("%s  %d Demod[%d] OOB Cfg FAILED  : MxLWare_HRCLS_API_CfgDemodOutParamsOOB status = %d\n",
			__func__, __LINE__, demodId, status);
			return MXL_FAILURE;
		}
	}

	return status;
}

static MXL_STATUS_E mxl_waitForTunerLock(MXL_HRCLS_TUNER_ID_E tunerId)
{
	MXL_STATUS_E status;
	MXL_HRCLS_TUNER_STATUS_E lockStatus = MXL_HRCLS_TUNER_DISABLED;
	do {
		MxLWare_HRCLS_OEM_DelayUsec(1000);

		status = MxLWare_HRCLS_API_ReqTunerLockStatus
			(MXL_HRCLS_DEVICE_ID, tunerId, &lockStatus);
	} while ((status == MXL_SUCCESS) &&
		(lockStatus != MXL_HRCLS_TUNER_LOCKED));
		/*tODO: add timeout checking*/

	return ((status == MXL_SUCCESS) &&
		(lockStatus == MXL_HRCLS_TUNER_LOCKED)) ?
		MXL_SUCCESS : MXL_FAILURE;
}
static MXL_STATUS_E mxl_waitForChannelLock(MXL_HRCLS_CHAN_ID_E chanId)
{
	MXL_STATUS_E status;
	MXL_HRCLS_CHAN_STATUS_E lockStatus = MXL_HRCLS_CHAN_DISABLED;
	do {
		MxLWare_HRCLS_OEM_DelayUsec(1000);
		status = MxLWare_HRCLS_API_ReqTunerChanStatus
			(MXL_HRCLS_DEVICE_ID, chanId, &lockStatus);
	} while ((status == MXL_SUCCESS) &&
		(lockStatus != MXL_HRCLS_CHAN_LOCKED));

	return ((status == MXL_SUCCESS) &&
		(lockStatus == MXL_HRCLS_CHAN_LOCKED)) ?
		MXL_SUCCESS : MXL_FAILURE;
}


static MXL_STATUS_E mxl_enableFbTuner(void)
{
	MXL_STATUS_E status;

	/**********************************************************************
	Enable Fullband tuner
	*********************************************************************/
	status = MxLWare_HRCLS_API_CfgTunerEnable(MXL_HRCLS_DEVICE_ID,
			MXL_HRCLS_FULLBAND_TUNER);
	if (status != MXL_FAILURE) {
		/**************************************************************
		Wait for FB tuner to lock
		**************************************************************/
		status = mxl_waitForTunerLock(MXL_HRCLS_FULLBAND_TUNER);
		if (status != MXL_SUCCESS) {
			pr_info("%s  %d DEvide_id=%d Fullband tuner synth lock TIMEOUT! status = %d\n",
			__func__, __LINE__, MXL_HRCLS_DEVICE_ID, status);
		}
	} else {
		pr_info("%s  %d DEvide_id=%d Cannot enable fullband tuner status = %d\n",
		__func__, __LINE__, MXL_HRCLS_DEVICE_ID, status);
	}

	return MXL_SUCCESS;
}

/************************************************************************
--FUNCTION	::	FE_MXL214_GetSignalInfo
--ACTION	::	Return informations on the locked transponder
--PARAMS IN	::	Handle	==>	Front End Handle
		::	Demod	==>	Cuurent demod 1 or 2
--PARAMS OUT::	pInfo	==> Informations (BER,C/N,power ...)
--RETURN	::	Error (if any)
--*********************************************************************/
FE_MXL214_Error_t FE_MXL214_GetSignalInfo(FE_MXL214_Handle_t Handle,
					  FE_MXL214_DEMOD_t Demod,
					  FE_CAB_SignalInfo_t *pInfo)
{
	FE_MXL214_InternalParams_t *p = NULL;
	MXL_BOOL_E qamLock, fecLock, mpegLock, relock;
	MXL_HRCLS_DMD_ID_E demodId;
	UINT16 rxPwrIndBuV;
	MXL_HRCLS_RX_PWR_ACCURACY_E  accuracy;
	UINT16 snr;
	MXL_HRCLS_DMD_STAT_CNT_T  stats;

	p = (FE_MXL214_InternalParams_t *)Handle;
	if (p == NULL)
		return FE_LLA_INVALID_HANDLE;

	demodId = Demod;
	stm_fe_mutex_lock(&lock_mxl);
	p->status = MxLWare_HRCLS_API_ReqDemodAllLockStatus(MXL_HRCLS_DEVICE_ID,
			demodId, &qamLock, &fecLock, &mpegLock, &relock);
	stm_fe_mutex_unlock(&lock_mxl);
	if (p->status != MXL_SUCCESS) {
		pr_err("%s  %d MxLWare_HRCLS_API_ReqDemodAllLockStatus status = %d\n",
			__func__, __LINE__, p->status);
		return FE_LLA_TRACKING_FAILED;
	}
#if 0
	if ((MXL_TRUE == qamLock) && (MXL_TRUE == fecLock) &&
		(MXL_TRUE == mpegLock) && (MXL_FALSE == relock)) {
		stm_fe_mutex_lock(&lock_mxl);
		p->status  = MxLWare_HRCLS_API_CfgUpdateDemodSettings
				(MXL_HRCLS_DEVICE_ID, MXL_HRCLS_DEMOD0);
		stm_fe_mutex_unlock(&lock_mxl);
		if (p->status != MXL_SUCCESS) {
			pr_err("%s MxLWare_HRCLS_API_CfgUpdateDemodSettings status = %d\n",
			__func__, __LINE__, p->status);
			return FE_LLA_TRACKING_FAILED;
		}
	} else {
		return FE_LLA_TRACKING_FAILED;
	}
#endif
	if ((MXL_TRUE == qamLock) && (MXL_TRUE == fecLock) &&
		(MXL_TRUE == mpegLock) && (MXL_FALSE == relock)) {

		pInfo->Locked = TRUE;
		pInfo->SymbolRate = p->SymbolRate_Bds;
		pInfo->Frequency = p->Frequency_kHz;
		pInfo->Modulation = mxl214_to_stfe_modulation(p->Modulation);
		/*pInfo->SpectInversion = */

		stm_fe_mutex_lock(&lock_mxl);
		p->status = MxLWare_HRCLS_API_ReqDemodSnr(MXL_HRCLS_DEVICE_ID,
				demodId, &snr);
		stm_fe_mutex_unlock(&lock_mxl);
		if (p->status != MXL_SUCCESS) {
			pr_err("%s  %d MxLWare_HRCLS_API_ReqDemodSnr status = %d\n",
				__func__, __LINE__, p->status);
			return FE_LLA_TRACKING_FAILED;
		}
		/*snr is in db*10*/
		pInfo->SNRx10dB_u32 = snr;

		stm_fe_mutex_lock(&lock_mxl);
		p->status = MxLWare_HRCLS_API_ReqDemodErrorStat
				(MXL_HRCLS_DEVICE_ID, demodId, &stats);
		stm_fe_mutex_unlock(&lock_mxl);
		if (p->status != MXL_SUCCESS) {
			pr_err("%s  %d MxLWare_HRCLS_API_ReqDemodErrorStat status = %d\n",
				__func__, __LINE__, p->status);
			return FE_LLA_TRACKING_FAILED;
		}
		/*snr is in db*10*/
		pInfo->SNRx10dB_u32 = snr;

		stm_fe_mutex_lock(&lock_mxl);
		p->status = MxLWare_HRCLS_API_ReqTunerRxPwr(MXL_HRCLS_DEVICE_ID,
				demodId, &rxPwrIndBuV, &accuracy);
		stm_fe_mutex_unlock(&lock_mxl);
		if (p->status != MXL_SUCCESS) {
			pr_err("%s  %d MxLWare_HRCLS_API_ReqTunerRxPwr status = %d\n",
				__func__, __LINE__, p->status);
			return FE_LLA_TRACKING_FAILED;
		}
		/*rxPwrIndBuV is in dBuv*10 */
		pInfo->RFLevelx10dBm_s32 = rxPwrIndBuV - 1090;
		 pr_err("%s  %d pInfo->RFLevelx10dBm_s32=%d snr=%d status = %d\n",
			 __func__, __LINE__, pInfo->RFLevelx10dBm_s32,
			 pInfo->SNRx10dB_u32, p->status);

	}
	 else {
		 return FE_LLA_TRACKING_FAILED;
	 }


	return FE_LLA_NO_ERROR;
}

static MXL_STATUS_E FE_MXL214_checkVersion(MXL_HRCLS_DEV_VER_T *verInfoPtr)
{
	/*********************************************************************
	Read MxLWare, Firmware and Bootloader version.
	*********************************************************************/
	MXL_STATUS_E status = MxLWare_HRCLS_API_ReqDevVersionInfo
				(MXL_HRCLS_DEVICE_ID, verInfoPtr);

	if (status == MXL_SUCCESS) {
		pr_info("%s  %d [HRCLS][%d] chipVersion=%d MxLWare: %d.%d.%d.%d-RC%d Firmware:%d.%d.%d.%d-RC%d Bootloader:%d.%d.%d.%d fwDownloaded=%d\n",
		__func__, __LINE__,
		MXL_HRCLS_DEVICE_ID,
		verInfoPtr->chipVersion,
		verInfoPtr->mxlWareVer[0],
		verInfoPtr->mxlWareVer[1],
		verInfoPtr->mxlWareVer[2],
		verInfoPtr->mxlWareVer[3],
		verInfoPtr->mxlWareVer[4],
		verInfoPtr->firmwareVer[0],
		verInfoPtr->firmwareVer[1],
		verInfoPtr->firmwareVer[2],
		verInfoPtr->firmwareVer[3],
		verInfoPtr->firmwareVer[4],
		verInfoPtr->bootLoaderVer[0],
		verInfoPtr->bootLoaderVer[1],
		verInfoPtr->bootLoaderVer[2],
		verInfoPtr->bootLoaderVer[3],
		verInfoPtr->firmwareDownloaded
		);
	}
	return status;
}

static MXL_STATUS_E FE_MXL214_downloadFirmware(UINT8 DEV_ID,
					STCHIP_Handle_t *hChipHandle)
{
	MXL_STATUS_E result = MXL_FAILURE;
	int ret = -1;
	const struct firmware *fw;
	UINT8 *fw_file = HRCLS_FIRMWARE_FILENAME;
	STCHIP_Handle_t hChip;
	hChip = (*hChipHandle);

	/* request the firmware, this will block and timeout */
	ret = request_firmware(&fw, fw_file, hChip->dev_i2c->dev.parent);
	if (ret) {
		pr_err("%s  %d :did not find the firmware file. (%s)\n"
			" on firmware-problems. (%d)\n",
			__func__, __LINE__, fw_file, ret);
		return MXL_FAILURE;
	}
	pr_info("%s  %d :downloading firmware from file '%s'\n"
		"fwSize =%u fwdata =0x%x device = 0x%x\n",
		__func__, __LINE__, fw_file , fw->size, (unsigned int) fw->data,
		(unsigned int)hChip->dev_i2c->dev.parent);

	result = MxLWare_HRCLS_API_CfgDevFirmwareDownload(MXL_HRCLS_DEVICE_ID,
			(U32)fw->size, (U8 *)fw->data, NULL);
	if (result != MXL_SUCCESS)
		pr_err("%s  %d : FW download Failure Err = %d ",
			__func__, __LINE__, result);

	/*release the fw image*/
	release_firmware(fw);

	return result;
}

/*****************************************************
--FUNCTION	::	FE_MXL214_Term
--ACTION	::	Terminate mxl214 chip connection
--PARAMS IN	::	Handle	==>	Front End Handle
--PARAMS OUT::	NONE
--RETURN	::	Error (if any)
--***************************************************/
FE_MXL214_Error_t FE_MXL214_Term(FE_MXL214_Handle_t Handle)
{
	FE_MXL214_InternalParams_t *p = NULL;
	p = (FE_MXL214_InternalParams_t	*) Handle;

	if (p == NULL)
		return FE_LLA_INVALID_HANDLE;

	/*has to implement*/

	return FE_LLA_NO_ERROR;
}

/*****************************************************
--FUNCTION	::	FE_MXL214_Status
--ACTION	::	Return locked status
--PARAMS IN	::	Handle	==>	Front End Handle
		::Demod	==>	Current demod 1/2/3/4
--PARAMS OUT	::	NONE
--RETURN	::Bool (locked or not)
--***************************************************/
BOOL FE_MXL214_Status(FE_MXL214_Handle_t Handle, FE_MXL214_DEMOD_t Demod)
{
	FE_MXL214_InternalParams_t *p = NULL;
	MXL_BOOL_E qamLock, fecLock, mpegLock, relock;
	U8 num_trials = 2, index = 0;


	p = (FE_MXL214_InternalParams_t *) Handle;
	fecLock = (MXL_BOOL_E)FALSE;/*Initial lock is FALSE*/

	if (p == NULL)
		return FALSE;

	while (((index) < num_trials) && (fecLock != (MXL_BOOL_E)TRUE)) {
		ChipWaitOrAbort(p->hDemod, 1000);
		stm_fe_mutex_lock(&lock_mxl);
		p->status =
		MxLWare_HRCLS_API_ReqDemodAllLockStatus(MXL_HRCLS_DEVICE_ID,
		Demod, &qamLock, &fecLock, &mpegLock, &relock);
		stm_fe_mutex_unlock(&lock_mxl);
		if (p->status != MXL_SUCCESS) {
			pr_err("%s  %d :_HYDRA_API_ReqDemodLockStatus status =%d\n",
					__func__, __LINE__, p->status);
			return FE_LLA_I2C_ERROR;
		}
		index++;
	}

	if ((p->status == MXL_SUCCESS) &&
		(fecLock == (MXL_BOOL_E)TRUE))
		return fecLock;
	else
		return FALSE;

}

/*****************************************************
--FUNCTION	::	FE_MXL214_Unlock
--ACTION	::	Unlock the demodulator , set the demod to idle state
--PARAMS IN	::	Handle	==>Front End Handle
		::	Demod	==>Current demod 1 or 2 or 3 or 4
-PARAMS OUT	::	NONE.
--RETURN	::	Error (if any)
--***************************************************/
FE_MXL214_Error_t FE_MXL214_Unlock(FE_MXL214_Handle_t Handle,
					FE_MXL214_DEMOD_t Demod)
{
	FE_MXL214_InternalParams_t *p;
	p = (FE_MXL214_InternalParams_t *)Handle;

	if (p == NULL)
		return FE_LLA_INVALID_HANDLE;

	stm_fe_mutex_lock(&lock_mxl);
	p->status = MxLWare_HRCLS_API_CfgDemodEnable(MXL_HRCLS_DEVICE_ID,
			Demod, MXL_DISABLE);
	stm_fe_mutex_unlock(&lock_mxl);
	if (p->status != MXL_SUCCESS) {
		pr_err("%s %d:MxLWare_HRCLS_API_CfgDevPowerMode Status =%d\n",
				__func__, __LINE__, p->status);
		return FE_LLA_I2C_ERROR;
	}

	return FE_LLA_NO_ERROR;
}
/*****************************************************
--FUNCTION	::	FE_MXL214_SetStandby
--ACTION	::	Set demod STANDBAY mode On/Off
--PARAMS IN	::	Handle	==>	Front End Handle

-PARAMS OUT::	NONE.
--RETURN	::	Error (if any)
--***************************************************/
FE_MXL214_Error_t FE_MXL214_SetStandby(FE_MXL214_Handle_t Handle,
					U8 StandbyOn, FE_MXL214_DEMOD_t Demod)
{
	FE_MXL214_InternalParams_t *p;
	p = (FE_MXL214_InternalParams_t	*)Handle;

	if (p == NULL)
		return FE_LLA_INVALID_HANDLE;
#if 0
	if (StandbyOn) {
		stm_fe_mutex_lock(&lock_mxl);
		p->status =
			MxLWare_HRCLS_API_CfgDevPowerMode(MXL_HRCLS_DEVICE_ID,
						MXL_HRCLS_POWER_MODE_SLEEP);
		stm_fe_mutex_unlock(&lock_mxl);
		if (p->status != MXL_SUCCESS) {
			pr_err("%s %d :MxLWare_HRCLS_API_CfgDevPowerMode Status =%d\n",
					__func__, __LINE__, p->status);
			return FE_LLA_I2C_ERROR;
		}
	} else {
		stm_fe_mutex_lock(&lock_mxl);
		p->status =
			MxLWare_HRCLS_API_CfgDevPowerMode(MXL_HRCLS_DEVICE_ID,
						MXL_HRCLS_POWER_MODE_NORMAL);
		stm_fe_mutex_unlock(&lock_mxl);
		if (p->status != MXL_SUCCESS) {
			pr_err("%s %d:MxLWare_HRCLS_API_CfgDevPowerMode status =%d\n",
					__func__, __LINE__, p->status);
			return FE_LLA_I2C_ERROR;
		}
	}

	if (p->hDemod->Error)
		return FE_LLA_I2C_ERROR;
#endif
	return FE_LLA_NO_ERROR;
}

/*****************************************************
--FUNCTION	::	FE_MXL214_SetAbortFlag
--ACTION	::	Set Abort flag On/Off
--PARAMS IN	::	Handle	==>	Front End Handle

-PARAMS OUT::	NONE.
--RETURN	::	Error (if any)
--***************************************************/
FE_MXL214_Error_t FE_MXL214_SetAbortFlag(FE_MXL214_Handle_t Handle,
					BOOL Abort, FE_MXL214_DEMOD_t Demod)
{
	FE_MXL214_InternalParams_t *p;
	p = (FE_MXL214_InternalParams_t *)Handle;

	if (p == NULL)
		return FE_LLA_INVALID_HANDLE;

	ChipAbort(p->hDemod, Abort);
	if (p->hDemod->Error)
		return FE_LLA_I2C_ERROR;

	return FE_LLA_NO_ERROR;
}

static MXL_HRCLS_QAM_TYPE_E
	stfe_to_mxl214_modulation(FE_CAB_Modulation_t Modulation)
{
	MXL_HRCLS_QAM_TYPE_E mxl_mod;
	switch (Modulation) {
	case FE_CAB_MOD_QAM16:
		mxl_mod = MXL_HRCLS_QAM16;
		break;
	case FE_CAB_MOD_QAM32:
		mxl_mod = MXL_HRCLS_QAM32;
		break;
	case FE_CAB_MOD_QAM64:
		mxl_mod = MXL_HRCLS_QAM64;
		break;
	case FE_CAB_MOD_QAM128:
		mxl_mod = MXL_HRCLS_QAM128;
		break;
	case FE_CAB_MOD_QAM256:
		mxl_mod = MXL_HRCLS_QAM256;
		break;
	case FE_CAB_MOD_QAM1024:
		mxl_mod = MXL_HRCLS_QAM1024;
		break;
/*	case FE_CAB_MOD_QPSK:
		mxl_mod = MXL_HRCLS_QPSK;
		break;
	case FE_CAB_MOD_AUTO:*/
	default:
		mxl_mod = MXL_HRCLS_QAM_AUTO;
		break;

	}
	return mxl_mod;
}

static FE_CAB_Modulation_t
	mxl214_to_stfe_modulation(MXL_HRCLS_QAM_TYPE_E Modulation)
{
	FE_CAB_Modulation_t stfe_mod;
	switch (Modulation) {
	case MXL_HRCLS_QAM16:
		stfe_mod = FE_CAB_MOD_QAM16;
		break;
	case MXL_HRCLS_QAM32:
		stfe_mod = FE_CAB_MOD_QAM32;
		break;
	case MXL_HRCLS_QAM64:
		stfe_mod = FE_CAB_MOD_QAM64;
		break;
	case MXL_HRCLS_QAM128:
		stfe_mod = FE_CAB_MOD_QAM128;
		break;
	case MXL_HRCLS_QAM256:
		stfe_mod = FE_CAB_MOD_QAM256;
		break;
	case MXL_HRCLS_QAM1024:
		stfe_mod = FE_CAB_MOD_QAM1024;
		break;
/*	case MXL_HRCLS_QPSK:
		stfe_mod = FE_CAB_MOD_QPSK;
		break;
	case MXL_HRCLS_QAM_AUTO:
	default:
		stfe_mod = FE_CAB_MOD_AUTO;
		break;*/
	default:
		stfe_mod = -1;
		break;
	}
	return stfe_mod;
}

static MXL_HRCLS_ANNEX_TYPE_E stfe_to_mxl214_annex(FE_CAB_FECType_t Annex)
{
	MXL_HRCLS_ANNEX_TYPE_E mxl_annex;
	switch (Annex) {
	default:
	case FE_CAB_FEC_A:
	case FE_CAB_FEC_C:
		mxl_annex = MXL_HRCLS_ANNEX_A;
		break;
	case FE_CAB_FEC_B:
		mxl_annex = MXL_HRCLS_ANNEX_B;
		break;
	}
	return mxl_annex;
}
/*static FE_CAB_FECType_t mxl214_to_stfe_annex(MXL_HRCLS_ANNEX_TYPE_E Annex)
{
	FE_CAB_FECType_t stfe_annex;
	switch (Annex) {
	default:
	case MXL_HRCLS_ANNEX_A:
		stfe_annex = FE_CAB_FEC_A;
		break;
	case MXL_HRCLS_ANNEX_B:
		stfe_annex = FE_CAB_FEC_B;
		break;
	}
	return stfe_annex;
}*/
