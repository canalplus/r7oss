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

Source file name :tuner_wrapper.c
Author :           Shobhit

wrapper for tuner LLAs

Date        Modification                                    Name
----        ------------                                    --------
20-Jun-11   Created                                         Shobhit

************************************************************************/

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/stm/plat_dev.h>
#include <linux/stm/demod.h>
#include <linux/stm/lnb.h>
#include <linux/stm/diseqc.h>
#include <stm_registry.h>
#include <stm_fe.h>
#include <stfe_utilities.h>
#include <stm_fe_os.h>
#include <i2c_wrapper.h>
#include "tuner_wrapper.h"
#include <stm_fe_demod.h>
#include <fesat_commlla_str.h>

TUNER_Error_t FE_Sat_TunerInit(void *pTunerInit, STCHIP_Handle_t *TunerHandle)
{
	TUNER_Error_t err;
	struct tuner_ops_s *tuner_ops;
	STCHIP_Handle_t chip_h;
	chip_h = (*TunerHandle);

	tuner_ops = (struct tuner_ops_s *)(chip_h->tuner_ops);

	/*Check for the validity of this function pointer */
	if (!tuner_ops->init)
		return TUNER_INVALID_HANDLE;
	err = tuner_ops->init(pTunerInit, TunerHandle);

	return err;

}
EXPORT_SYMBOL(FE_Sat_TunerInit);

void FE_Sat_TunerSetReferenceFreq(STCHIP_Handle_t tuner_h, U32 Reference)
{
	struct tuner_ops_s *tuner_ops;

	tuner_ops = (struct tuner_ops_s *)(tuner_h->tuner_ops);
	/*Check for the validity of this function pointer */
	if (tuner_ops->set_referenceFreq)
		tuner_ops->set_referenceFreq(tuner_h, Reference);

}
EXPORT_SYMBOL(FE_Sat_TunerSetReferenceFreq);

TUNER_Error_t FE_Sat_TunerSetGain(STCHIP_Handle_t tuner_h, S32 Gain)
{
	TUNER_Error_t err;
	struct tuner_ops_s *tuner_ops;

	tuner_ops = (struct tuner_ops_s *)(tuner_h->tuner_ops);

	/*Check for the validity of this function pointer */
	if (!tuner_ops->set_gain)
		return TUNER_INVALID_HANDLE;
	err = tuner_ops->set_gain(tuner_h, Gain);

	return err;
}
EXPORT_SYMBOL(FE_Sat_TunerSetGain);

U32 FE_Sat_TunerGetFrequency(STCHIP_Handle_t tuner_h)
{
	struct tuner_ops_s *tuner_ops;
	U32 frequency = 0;
	tuner_ops = (struct tuner_ops_s *)(tuner_h->tuner_ops);

	/*Check for the validity of this function pointer */
	if (!tuner_ops->get_frequency)
		return 0;
	frequency = tuner_ops->get_frequency(tuner_h);

	return frequency;
}
EXPORT_SYMBOL(FE_Sat_TunerGetFrequency);

TUNER_Error_t FE_Sat_TunerSetFrequency(STCHIP_Handle_t tuner_h, U32 Frequency)
{
	TUNER_Error_t err;
	struct tuner_ops_s *tuner_ops;
	tuner_ops = (struct tuner_ops_s *)(tuner_h->tuner_ops);

	/*Check for the validity of this function pointer */
	if (!tuner_ops->set_frequency)
		return CHIPERR_INVALID_HANDLE;
	err = tuner_ops->set_frequency(tuner_h, Frequency);

	return err;
}
EXPORT_SYMBOL(FE_Sat_TunerSetFrequency);

TUNER_Error_t FE_Sat_TunerSetBandwidth(STCHIP_Handle_t tuner_h, U32 Bandwidth)
{
	TUNER_Error_t err;
	struct tuner_ops_s *tuner_ops;

	tuner_ops = (struct tuner_ops_s *)(tuner_h->tuner_ops);
	/*Check for the validity of this function pointer */
	if (!tuner_ops->set_bandWidth)
		return CHIPERR_INVALID_HANDLE;
	err = tuner_ops->set_bandWidth(tuner_h, Bandwidth);

	return err;
}
EXPORT_SYMBOL(FE_Sat_TunerSetBandwidth);

TUNER_Error_t FE_Sat_TunerSwitchInput(STCHIP_Handle_t tuner_h, U8 input)
{
	TUNER_Error_t err;
	struct tuner_ops_s *tuner_ops;

	if (!tuner_h) {
		pr_err("%s: invalid tuner handle\n", __func__);
		return TUNER_INVALID_HANDLE;
	}
	tuner_ops = (struct tuner_ops_s *)(tuner_h->tuner_ops);
	if (!tuner_ops) {
		pr_err("%s: invalid tuner_ops\n", __func__);
		return TUNER_INVALID_HANDLE;
	}
	if (!tuner_ops->switch_input) {
		pr_err("%s: FP tuner_switch_input is NULL\n", __func__);
		return TUNER_INVALID_HANDLE;
	}
	err = tuner_ops->switch_input(tuner_h, input);

	return err;
}
EXPORT_SYMBOL(FE_Sat_TunerSwitchInput);

BOOL FE_Sat_TunerGetStatus(STCHIP_Handle_t tuner_h)
{
	struct tuner_ops_s *tuner_ops;
	bool locked = false;
	tuner_ops = (struct tuner_ops_s *)(tuner_h->tuner_ops);

	/*Check for the validity of this function pointer */
	if (!tuner_ops->get_status)
		return FALSE;
	locked = tuner_ops->get_status(tuner_h);

	return locked;
}
EXPORT_SYMBOL(FE_Sat_TunerGetStatus);

TUNER_Error_t FE_Sat_TunerSetStandby(STCHIP_Handle_t tuner_h, U8 StandbyOn)
{
	TUNER_Error_t err;
	struct tuner_ops_s *tuner_ops;
	tuner_ops = (struct tuner_ops_s *)(tuner_h->tuner_ops);

	/*Check for the validity of this function pointer */
	if (!tuner_ops->set_standby) {
		pr_err("%s: FP tuner_set_standby is NULL\n", __func__);
		return CHIPERR_INVALID_HANDLE;
	}
	err = tuner_ops->set_standby(tuner_h, StandbyOn);


	return err;
}
EXPORT_SYMBOL(FE_Sat_TunerSetStandby);

S32
FE_Sat_TunerGetRFGain(STCHIP_Handle_t tuner_h, U32 AGCIntegrator, S32 BBGain)
{
	S32 Gain100dB = 0;
	struct tuner_ops_s *tuner_ops = tuner_h->tuner_ops;

	if (tuner_ops->get_rfgain)
		Gain100dB = tuner_ops->get_rfgain(tuner_h, AGCIntegrator,
									BBGain);

	return Gain100dB;
}
EXPORT_SYMBOL(FE_Sat_TunerGetRFGain);

TUNER_Error_t FE_Sat_TunerSetHMR_Filter(STCHIP_Handle_t tuner_h,
								S32 filterValue)
{
	TUNER_Error_t err;
	struct tuner_ops_s *tuner_ops = tuner_h->tuner_ops;

	if (!tuner_ops->set_hmr_filter)
		return CHIPERR_INVALID_HANDLE;

	err = tuner_ops->set_hmr_filter(tuner_h, filterValue);

	return err;
}
EXPORT_SYMBOL(FE_Sat_TunerSetHMR_Filter);

S32 FE_Sat_TunerGetGain(STCHIP_Handle_t tuner_h)
{
	struct tuner_ops_s *tuner_ops;
	tuner_ops = (struct tuner_ops_s *)(tuner_h->tuner_ops);

	if (!tuner_ops->get_gain)
		return CHIPERR_INVALID_HANDLE;

	return tuner_ops->get_gain(tuner_h);
}
EXPORT_SYMBOL(FE_Sat_TunerGetGain);

TUNER_Error_t FE_Sat_TunerSetLNAInput(STCHIP_Handle_t tuner_h,
						    TUNER_RFSource_t tunerinput)
{
	struct tuner_ops_s *tuner_ops;
	tuner_ops = (struct tuner_ops_s *)(tuner_h->tuner_ops);

	if (!tuner_ops->set_lnainput)
		return CHIPERR_INVALID_HANDLE;

	return tuner_ops->set_lnainput(tuner_h, tunerinput);
}
EXPORT_SYMBOL(FE_Sat_TunerSetLNAInput);




void FE_Sat_TunerTerm(STCHIP_Handle_t tuner_h)
{
	struct tuner_ops_s *tuner_ops;

	tuner_ops = (struct tuner_ops_s *)(tuner_h->tuner_ops);
	/*Check for the validity of this function pointer */
	if (tuner_ops->term)
		tuner_ops->term(tuner_h);
	else
		pr_err("%s: FP tuner_ops->tuner_term is NULL\n", __func__);

}
EXPORT_SYMBOL(FE_Sat_TunerTerm);

static int32_t __init tuner_sat_wrapper_init(void)
{
	pr_info("Loading sat tuner wrapper module ...\n");

	return 0;
}

module_init(tuner_sat_wrapper_init);

static void __exit tuner_sat_wrapper_term(void)
{
	pr_info("Removing sat tuner wrapper module ...\n");

}

module_exit(tuner_sat_wrapper_term);

MODULE_DESCRIPTION("Abstraction layer for satellite tuner device drivers");
MODULE_AUTHOR("STMicroelectronics Limited");
MODULE_LICENSE("GPL");
