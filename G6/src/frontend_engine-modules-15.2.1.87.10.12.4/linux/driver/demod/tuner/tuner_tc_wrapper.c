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

Source file name :tuner_tc_wrapper.c
Author :          Ankur

wrapper for tuner LLAs

Date        Modification                                    Name
----        ------------                                    --------
01-Aug-11   Created                                         Ankur

************************************************************************/

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/stm/plat_dev.h>
#include <linux/stm/demod.h>
#include <stm_registry.h>
#include <stm_fe.h>
#include <stfe_utilities.h>
#include <stm_fe_os.h>
#include <i2c_wrapper.h>
#include "tuner_wrapper.h"
#include <stm_fe_demod.h>
#include "fe_tc_tuner.h"

TUNER_Error_t FE_TunerInit(void *pTunerInit, STCHIP_Handle_t *TunerHandle)
{
	TUNER_Error_t err;
	struct tuner_ops_s *tuner_ops;
	STCHIP_Handle_t chip_h;

	chip_h = (*TunerHandle);

	tuner_ops = (struct tuner_ops_s *)(chip_h->tuner_ops);
	/*Check for the validity of this function pointer */
	if (!tuner_ops->init) {
		pr_info("%s: NULL T/C tuner_init\n", __func__);
		return TUNER_INVALID_HANDLE;
	}
	err = tuner_ops->init(pTunerInit, TunerHandle);

	return err;

}
EXPORT_SYMBOL(FE_TunerInit);

void FE_TunerSetReferenceFreq(STCHIP_Handle_t tuner_h, U32 Reference)
{
	struct tuner_ops_s *tuner_ops;

	tuner_ops = (struct tuner_ops_s *)(tuner_h->tuner_ops);
	/*Check for the validity of this function pointer */
	if (tuner_ops->set_referenceFreq)
		tuner_ops->set_referenceFreq(tuner_h, Reference);
	else
		pr_info("%s: NULL T/C tuner_set_referenceFreq\n", __func__);

}
EXPORT_SYMBOL(FE_TunerSetReferenceFreq);

TUNER_Error_t FE_TunerSetGain(STCHIP_Handle_t tuner_h, S32 Gain)
{
	TUNER_Error_t err;
	struct tuner_ops_s *tuner_ops;

	tuner_ops = (struct tuner_ops_s *)(tuner_h->tuner_ops);

	/*Check for the validity of this function pointer */
	if (!tuner_ops->set_gain) {
		pr_info("%s: NULL T/C tuner_set_gain\n", __func__);
		return TUNER_INVALID_HANDLE;
	}
	err = tuner_ops->set_gain(tuner_h, Gain);

	return err;
}
EXPORT_SYMBOL(FE_TunerSetGain);

U32 FE_TunerGetFrequency(STCHIP_Handle_t tuner_h)
{
	struct tuner_ops_s *tuner_ops;
	U32 frequency = 0;
	tuner_ops = (struct tuner_ops_s *)(tuner_h->tuner_ops);

	/*Check for the validity of this function pointer */
	if (!tuner_ops->get_frequency) {
		pr_info("%s: NULL T/C tuner_get_frequency\n", __func__);
		return 0;
	}
	frequency = tuner_ops->get_frequency(tuner_h);

	return frequency;
}
EXPORT_SYMBOL(FE_TunerGetFrequency);

TUNER_Error_t FE_TunerSetFrequency(STCHIP_Handle_t tuner_h, U32 Frequency)
{
	TUNER_Error_t err;
	struct tuner_ops_s *tuner_ops;
	tuner_ops = (struct tuner_ops_s *)(tuner_h->tuner_ops);

	/*Check for the validity of this function pointer */
	if (!tuner_ops->set_frequency) {
		pr_info("%s: NULL T/C tuner_set_frequency\n", __func__);
		return TUNER_INVALID_HANDLE;
	}
	err = tuner_ops->set_frequency(tuner_h, Frequency);

	return err;
}
EXPORT_SYMBOL(FE_TunerSetFrequency);

TUNER_Error_t FE_TunerSetBandWidth(STCHIP_Handle_t tuner_h, U32 Bandwidth)
{
	TUNER_Error_t err;
	struct tuner_ops_s *tuner_ops;

	tuner_ops = (struct tuner_ops_s *)(tuner_h->tuner_ops);
	/*Check for the validity of this function pointer */
	if (!tuner_ops->set_bandWidth) {
		pr_info("%s: NULL T/C tuner_set_bandWidth\n", __func__);
		return TUNER_INVALID_HANDLE;
	}
	err = tuner_ops->set_bandWidth(tuner_h, Bandwidth);

	return err;
}
EXPORT_SYMBOL(FE_TunerSetBandWidth);

BOOL FE_TunerGetStatus(STCHIP_Handle_t tuner_h)
{
	struct tuner_ops_s *tuner_ops;
	bool locked = false;
	tuner_ops = (struct tuner_ops_s *)(tuner_h->tuner_ops);

	/*Check for the validity of this function pointer */
	if (!tuner_ops->get_status) {
		pr_info("%s: NULL T/C tuner_get_status\n", __func__);
		return FALSE;
	}
	locked = tuner_ops->get_status(tuner_h);

	return locked;
}
EXPORT_SYMBOL(FE_TunerGetStatus);

TUNER_Error_t FE_TunerSetStandbyMode(STCHIP_Handle_t tuner_h, U8 StandbyOn)
{
	TUNER_Error_t err;
	struct tuner_ops_s *tuner_ops;
	tuner_ops = (struct tuner_ops_s *)(tuner_h->tuner_ops);

	/*Check for the validity of this function pointer */
	if (!tuner_ops->set_standby) {
		pr_err("%s: NULL T/C tuner_set_standby\n", __func__);
		return TUNER_INVALID_HANDLE;
	}
	err = tuner_ops->set_standby(tuner_h, StandbyOn);

	return err;
}
EXPORT_SYMBOL(FE_TunerSetStandbyMode);

TUNER_Error_t FE_TunerStartAndCalibrate(STCHIP_Handle_t tuner_h)
{
	TUNER_Error_t err;
	struct tuner_ops_s *tuner_ops;
	tuner_ops = (struct tuner_ops_s *)(tuner_h->tuner_ops);

	/*Check for the validity of this function pointer */
	if (!tuner_ops->start_and_calibrate) {
		pr_info("%s: NULL T/C tuner_start_and_calibrate\n", __func__);
		return TUNER_INVALID_HANDLE;
	}
	err = tuner_ops->start_and_calibrate(tuner_h);

	return err;
}
EXPORT_SYMBOL(FE_TunerStartAndCalibrate);

TUNER_Error_t FE_TunerSetLna(STCHIP_Handle_t tuner_h)
{
	TUNER_Error_t err;
	struct tuner_ops_s *tuner_ops;
	tuner_ops = (struct tuner_ops_s *)(tuner_h->tuner_ops);

	/*Check for the validity of this function pointer */
	if (!tuner_ops->set_lna) {
		pr_info("%s: T/C tuner_set_lna\n", __func__);
		return TUNER_INVALID_HANDLE;
	}
	err = tuner_ops->set_lna(tuner_h);

	return err;
}
EXPORT_SYMBOL(FE_TunerSetLna);

TUNER_Error_t FE_TunerAdjustRfPower(STCHIP_Handle_t tuner_h, int Delta)
{
	TUNER_Error_t err;
	struct tuner_ops_s *tuner_ops;
	tuner_ops = (struct tuner_ops_s *)(tuner_h->tuner_ops);

	/*Check for the validity of this function pointer */
	if (!tuner_ops->adjust_rfpower) {
		pr_info("%s: NULL T/C tuner_adjust_rfpower\n", __func__);
		return TUNER_INVALID_HANDLE;
	}
	err = tuner_ops->adjust_rfpower(tuner_h, Delta);

	return err;
}
EXPORT_SYMBOL(FE_TunerAdjustRfPower);

int32_t FE_TunerGetRFLevel(STCHIP_Handle_t tuner_h, u_int8_t RFAGC, int IFAGC)
{
	int32_t power;
	struct tuner_ops_s *tuner_ops;
	tuner_ops = (struct tuner_ops_s *)(tuner_h->tuner_ops);

	/*Check for the validity of this function pointer */
	if (!tuner_ops->get_rflevel) {
		pr_info("%s: NULL T/C tuner_get_rflevel\n", __func__);
		return TUNER_INVALID_HANDLE;
	}
	power = tuner_ops->get_rflevel(tuner_h, RFAGC, IFAGC);

	return power;
}
EXPORT_SYMBOL(FE_TunerGetRFLevel);

u_int32_t FE_TunerGetIF_Freq(STCHIP_Handle_t tuner_h)
{
	U32 ifreq = 0;

	/*Check for the validity of this pointer */
	if (tuner_h && tuner_h->pData)
		ifreq = ((TUNER_Params_t) (tuner_h->pData))->IF;
	else
		pr_info("%s: NULL T/C FE_TunerGetIF_Freq\n", __func__);
	return ifreq;
}
EXPORT_SYMBOL(FE_TunerGetIF_Freq);

TUNER_Model_t FE_TunerGetModelName(STCHIP_Handle_t tuner_h)
{
	TUNER_Model_t model = TUNER_NULL;

	if (tuner_h && tuner_h->pData)
		model = ((TUNER_Params_t) (tuner_h->pData))->Model;
	else
		pr_info("%s: NULL T/C FE_TunerGetModelName\n", __func__);

	return model;
}
EXPORT_SYMBOL(FE_TunerGetModelName);

TUNER_Error_t FE_SwitchTunerToDVBC(STCHIP_Handle_t tuner_h)
{
	TUNER_Error_t err;
	struct tuner_ops_s *tuner_ops;
	tuner_ops = (struct tuner_ops_s *)(tuner_h->tuner_ops);

	/*Check for the validity of this function pointer */
	if (!tuner_ops->switch_to_dvbc) {
		pr_info("%s: NULL T/C tuner_switch_to_dvbc\n", __func__);
		return TUNER_INVALID_HANDLE;
	}
	err = tuner_ops->switch_to_dvbc(tuner_h);

	return err;
}
EXPORT_SYMBOL(FE_SwitchTunerToDVBC);

TUNER_Error_t FE_SwitchTunerToDVBT(STCHIP_Handle_t tuner_h)
{
	TUNER_Error_t err;
	struct tuner_ops_s *tuner_ops;
	tuner_ops = (struct tuner_ops_s *)(tuner_h->tuner_ops);

	/*Check for the validity of this function pointer */
	if (!tuner_ops->switch_to_dvbt) {
		pr_info("%s: NULL T/C tuner_switch_to_dvbt\n", __func__);
		return TUNER_INVALID_HANDLE;
	}
	err = tuner_ops->switch_to_dvbt(tuner_h);

	return err;
}
EXPORT_SYMBOL(FE_SwitchTunerToDVBT);

TUNER_Error_t FE_TunerTerm(STCHIP_Handle_t tuner_h)
{
	struct tuner_ops_s *tuner_ops;
	TUNER_Error_t err;
	tuner_ops = (struct tuner_ops_s *)(tuner_h->tuner_ops);
	/*Check for the validity of this function pointer */
	if (tuner_ops->term) {
		err = tuner_ops->term(tuner_h);
	} else {
		pr_info("%s: NULL T/C tuner_term\n", __func__);
		err = TUNER_INVALID_HANDLE;
	}

	return err;
}
EXPORT_SYMBOL(FE_TunerTerm);

static int32_t __init tuner_tc_wrapper_init(void)
{
	pr_info("Loading t/c tuner wrapper module ...\n");

	return 0;
}

module_init(tuner_tc_wrapper_init);

static void __exit tuner_tc_wrapper_term(void)
{
	pr_info("Removing t/c tuner wrapper module ...\n");

}

module_exit(tuner_tc_wrapper_term);

MODULE_DESCRIPTION
	("Abstraction layer for terrestrial and cable tuner device drivers");
MODULE_AUTHOR("STMicroelectronics Limited");
MODULE_LICENSE("GPL");
