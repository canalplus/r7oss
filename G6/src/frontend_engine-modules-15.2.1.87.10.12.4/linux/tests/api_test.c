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

Source file name : api_test.c
Author :           Shobhit

Test file to use stm fe object

Date        Modification                                    Name
----        ------------                                    --------
20-Jun-11   Created                                         Shobhit

************************************************************************/

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/list.h>
#include <linux/platform_device.h>
#include <linux/stm/plat_dev.h>
#include <linux/stm/demod.h>
#include <linux/stm/lnb.h>
#include <linux/stm/diseqc.h>
#include <linux/delay.h>
#include <linux/unistd.h>
#include <stm_registry.h>
#include <stm_fe.h>

static int32_t __init stm_fe_api_test_init(void)
{
	int32_t ret;
	int32_t i = 0;
	stm_fe_object_info_t *fe;
	stm_fe_demod_h obj = NULL;
	stm_fe_lnb_h lnb_handle = NULL;
	stm_fe_diseqc_h diseqc_handle = NULL;
	uint32_t cnt;
	stm_fe_demod_tune_params_t tune_param;
	stm_fe_lnb_config_t lnb_config;
	stm_fe_demod_scan_configure_t scan_config;
	stm_fe_demod_info_t demod_info;

	/* FOR IPFE API */
	stm_fe_ip_h ip_obj;
	stm_fe_ip_stat_t info_p;

	pr_info("\nLoading API test module...\n");
	pr_info("\n-----------------------------------------------\n");
	pr_info("UT_001: STM FE DISCOVER\n");
	pr_info("-----------------------------------------------\n");

	ret = stm_fe_discover(&fe, &cnt);
	for (i = 0; i < cnt; i++)
		pr_info("\n  -- %s\n", fe[i].stm_fe_obj);
	if (ret)
		pr_info("RESULT : FAILED\n");
	else
		pr_info("RESULT : PASSED\n");
	pr_info("-----------------------------------------------\n");

	if (fe[0].type == STM_FE_DEMOD) {
		pr_info(
		       "\n-----------------------------------------------\n");
		pr_info(
		       "UT_002: CREATE DEMOD OBJECT WITH FALSE NAME\n");
		pr_info(
		       "-----------------------------------------------\n");
		ret = stm_fe_demod_new("unknown", &obj);
		if (ret) {
			pr_info("RESULT : PASSED\n");
			pr_info(
			       "-----------------------------------------------\n");

			pr_info(
			       "\n-----------------------------------------------\n");
			pr_info(
			       "UT_003: TUNE WITHOUT DEMOD CREATION\n");
			pr_info(
			       "-----------------------------------------------\n");
			tune_param.std = STM_FE_DEMOD_TX_STD_DVBS;
			tune_param.u_tune.dvbs.freq = 1330000;	/*khz*/
			tune_param.u_tune.dvbs.fec = STM_FE_DEMOD_FEC_RATE_3_4;
			tune_param.u_tune.dvbs.inv =
			    STM_FE_DEMOD_SPECTRAL_INVERSION_AUTO;
			tune_param.u_tune.dvbs.sr = 27500000;
			ret = stm_fe_demod_tune(obj, &tune_param);
			if (ret)
				pr_info("RESULT : PASSED\n");
			else
				pr_info("RESULT : FAILED\n");
			pr_info(
			       "-----------------------------------------------\n");

			pr_info(
			       "\n-----------------------------------------------\n");
			pr_info(
			       "UT_004: CREATE DEMOD OBJECT WITH VALID NAME\n");
			pr_info(
			       "-----------------------------------------------\n");
			for (i = 0; i < cnt; i++) {
				if (fe[i].type == STM_FE_DEMOD) {
					ret =
					    stm_fe_demod_new(fe[i].stm_fe_obj,
							     &obj);
					break;
				}
			}
			if (ret)
				pr_info("RESULT : FAILED\n");
			else
				pr_info("RESULT : PASSED\n");
			pr_info(
			       "-----------------------------------------------\n");
		} else {
			pr_info("RESULT : FAILED\n");
			pr_info(
			       "-----------------------------------------------\n");
		}
	}
	pr_info("\n-----------------------------------------------\n");
	pr_info("UT_005: CREATE LNB OBJECT WITH FALSE NAME\n");
	pr_info("-----------------------------------------------\n");
	ret = stm_fe_lnb_new("unknown", obj, &lnb_handle);
	if (ret) {
		pr_info("RESULT : PASSED\n");
		pr_info(
		       "-----------------------------------------------\n");

		pr_info(
		       "\n-----------------------------------------------\n");
		pr_info("UT_006: USING LNB OBJECT WITHOUT CREATION\n");
		pr_info(
		       "-----------------------------------------------\n");
		lnb_config.lnb_tone_state = STM_FE_LNB_TONE_OFF;
		lnb_config.polarization = STM_FE_LNB_PLR_VERTICAL;
		ret = stm_fe_lnb_set_config(lnb_handle, &lnb_config);
		if (ret) {
			pr_info("RESULT : PASSED\n");
			pr_info(
			       "-----------------------------------------------\n");
		} else {
			pr_info("RESULT : FAILED\n");
			pr_info(
			       "-----------------------------------------------\n");
		}

		pr_info(
		       "\n-----------------------------------------------\n");
		pr_info("UT_007: CREATE LNB OBJECT WITH VALID NAME\n");
		pr_info(
		       "-----------------------------------------------\n");
		for (i = 0; i < cnt; i++) {
			if (fe[i].type == STM_FE_LNB) {
				ret =
				    stm_fe_lnb_new(fe[i].stm_fe_obj,
						   obj, &lnb_handle);
				break;
			}
		}
		if (ret)
			pr_info("RESULT : FAILED\n");
		else
			pr_info("RESULT : PASSED\n");
		pr_info(
		       "-----------------------------------------------\n");
	} else {
		pr_info("RESULT : FAILED\n");
		pr_info(
		       "-----------------------------------------------\n");
	}

	pr_info("\n-----------------------------------------------\n");
	pr_info("UT_008: CREATE DISEQC OBJECT WITH FALSE NAME\n");
	pr_info("-----------------------------------------------\n");
	ret = stm_fe_diseqc_new("unknown", obj, &diseqc_handle);
	if (ret) {
		pr_info("RESULT : PASSED\n");
		pr_info(
		       "-----------------------------------------------\n");

		pr_info(
		       "\n-----------------------------------------------\n");
		pr_info(
		       "UT_009 CREATE DISEQC OBJECT WITH VALID NAME\n");
		pr_info(
		       "-----------------------------------------------\n");

		for (i = 0; i < cnt; i++) {
			if (fe[i].type == STM_FE_DISEQC) {
				ret =
				    stm_fe_diseqc_new(fe[i].stm_fe_obj,
						      obj,
						      &diseqc_handle);
				break;
			}
		}
		if (ret)
			pr_info("RESULT : FAILED\n");
		else
			pr_info("RESULT : PASSED\n");
		pr_info(
		       "-----------------------------------------------\n");
	} else {
		pr_info("RESULT : FAILED\n");
		pr_info(
		       "-----------------------------------------------\n");
	}

	pr_info("\n-----------------------------------------------\n");
	pr_info("UT_010 DELETE DISEQC OBJECT\n");
	pr_info("-----------------------------------------------\n");
	ret = stm_fe_diseqc_delete(diseqc_handle);
	if (ret)
		pr_info("RESULT : FAILED\n");
	else
		pr_info("RESULT : PASSED\n");
	pr_info("-----------------------------------------------\n");

	pr_info("\n-----------------------------------------------\n");
	pr_info("UT_011 DELETE LNB OBJECT\n");
	pr_info("-----------------------------------------------\n");
	ret = stm_fe_lnb_delete(lnb_handle);
	if (ret)
		pr_info("RESULT : FAILED\n");
	else
		pr_info("RESULT : PASSED\n");
	pr_info("-----------------------------------------------\n");

	pr_info("\n-----------------------------------------------\n");
	pr_info("UT_012: LNB SET CONFIG AFTER LNB DELETE\n");
	pr_info("-----------------------------------------------\n");
	lnb_config.lnb_tone_state = STM_FE_LNB_TONE_OFF;
	lnb_config.polarization = STM_FE_LNB_PLR_VERTICAL;
	ret = stm_fe_lnb_set_config(lnb_handle, &lnb_config);
	if (ret)
		pr_info("RESULT : PASSED\n");
	else
		pr_info("RESULT : FAILED\n");
	pr_info("-----------------------------------------------\n");

	pr_info("\n-----------------------------------------------\n");
	pr_info("UT_013 DELETE DEMOD OBJECT\n");
	pr_info("-----------------------------------------------\n");
	ret = stm_fe_demod_delete(obj);
	if (ret)
		pr_info("RESULT : FAILED\n");
	else
		pr_info("RESULT : PASSED\n");
	pr_info("-----------------------------------------------\n");

	pr_info("\n-----------------------------------------------\n");
	pr_info("UT_014 DEMOD TUNE AFTER DEMOD DELETE\n");
	pr_info("-----------------------------------------------\n");
	/*pr_info("\n Tuner after demod delete\n");*/
	tune_param.std = STM_FE_DEMOD_TX_STD_DVBS;
	tune_param.u_tune.dvbs.freq = 1330000;	/*khz*/
	tune_param.u_tune.dvbs.fec = STM_FE_DEMOD_FEC_RATE_3_4;
	tune_param.u_tune.dvbs.inv = STM_FE_DEMOD_SPECTRAL_INVERSION_AUTO;
	tune_param.u_tune.dvbs.sr = 27500000;
	ret = stm_fe_demod_tune(obj, &tune_param);
	if (ret)
		pr_info("RESULT : PASSED\n");
	else
		pr_info("RESULT : FAILED\n");
	pr_info("-----------------------------------------------\n");

	pr_info("\n-----------------------------------------------\n");
	pr_info("UT_015 DEMOD STOP AFTER DEMOD DELETE\n");
	pr_info("-----------------------------------------------\n");
	ret = stm_fe_demod_stop(obj);
	if (ret)
		pr_info("RESULT : PASSED\n");
	else
		pr_info("RESULT : FAILED\n");
	pr_info("-----------------------------------------------\n");

	pr_info("\n-----------------------------------------------\n");
	pr_info("UT_016 DEMOD SCAN AFTER DEMOD DELETE\n");
	pr_info("-----------------------------------------------\n");
	memset(&scan_config, 0, sizeof(stm_fe_demod_scan_configure_t));
	scan_config.scan_stds = STM_FE_DEMOD_SCAN_TX_STD_DVB_SAT;
	scan_config.u_config.dvb_sat.fecs = STM_FE_DEMOD_FEATURE_AUTO_FEC;
	scan_config.u_config.dvb_sat.mods =
	    STM_FE_DEMOD_FEATURE_AUTO_MODULATION;
	scan_config.u_config.dvb_sat.table_form =
	    STM_FE_DEMOD_SCAN_TABLE_FORM_BOUNDARIES;
	scan_config.u_config.dvb_sat.freq_table[0] = 950000;	/* Start */
	scan_config.u_config.dvb_sat.freq_table[1] = 2150000;	/* End */
	scan_config.u_config.dvb_sat.freq_table[2] = 36000;
	ret =
	    stm_fe_demod_scan(obj, STM_FE_DEMOD_SCAN_NEW,
			      &scan_config);
	if (ret)
		pr_info("RESULT : PASSED\n");
	else
		pr_info("RESULT : FAILED\n");
	pr_info("-----------------------------------------------\n");

	pr_info("\n-----------------------------------------------\n");
	pr_info("UT_017 DEMOD GET_INFO AFTER DEMOD DELETE\n");
	pr_info("-----------------------------------------------\n");
	ret = stm_fe_demod_get_info(obj, &demod_info);
	if (ret)
		pr_info("RESULT : PASSED\n");
	else
		pr_info("RESULT : FAILED\n");
	pr_info("-----------------------------------------------\n");

	pr_info("\n-----------------------------------------------\n");
	pr_info("UT_018 DELETE DEMOD OBJECT AFTER DEMOD DELETE\n");
	pr_info("-----------------------------------------------\n");
	ret = stm_fe_demod_delete(obj);
	if (ret)
		pr_info("RESULT : PASSED\n");
	else
		pr_info("RESULT : FAILED\n");
	pr_info("-----------------------------------------------\n");

	pr_info("\n-----------------------------------------------\n");
	pr_info("UT_019 CREATE IPFE OBJECT WITH FALSE NAME\n");
	pr_info("-----------------------------------------------\n");
	ret = stm_fe_ip_new("unknown", NULL, &ip_obj);
	if (ret) {
		pr_info("RESULT : PASSED\n");
		pr_info(
		       "-----------------------------------------------\n");

		pr_info(
		       "\n-----------------------------------------------\n");
		pr_info("UT_020 CREATE IPFE OBJECT WITH VALID NAME\n");
		pr_info(
		       "-----------------------------------------------\n");
		for (i = 0; i < cnt; i++) {
			if (fe[i].type == STM_FE_IP) {
				ret =
				    stm_fe_ip_new(fe[i].stm_fe_obj, NULL,
						  &ip_obj);
				break;
			}
		}
		if (ret)
			pr_info("RESULT : FAILED\n");
		else
			pr_info("RESULT : PASSED\n");
		pr_info(
		       "-----------------------------------------------\n");

		pr_info(
		       "\n-----------------------------------------------\n");
		pr_info("UT_021 DELETE IPFE OBJECT\n");
		pr_info(
		       "-----------------------------------------------\n");
		ret = stm_fe_ip_delete(ip_obj);
		if (ret)
			pr_info("RESULT : FAILED\n");
		else
			pr_info("RESULT : PASSED\n");
		pr_info(
		       "-----------------------------------------------\n");

		pr_info(
		       "\n-----------------------------------------------\n");
		pr_info("UT_022 IPFE START AFTER IPFE DELETE\n");
		pr_info(
		       "-----------------------------------------------\n");
		ret = stm_fe_ip_start(ip_obj);
		if (ret)
			pr_info("RESULT : PASSED\n");
		else
			pr_info("RESULT : FAILED\n");
		pr_info(
		       "-----------------------------------------------\n");

		pr_info(
		       "\n-----------------------------------------------\n");
		pr_info("UT_021 IPFE STOP AFTER IPFE DELETE\n");
		pr_info(
		       "-----------------------------------------------\n");
		ret = stm_fe_ip_stop(ip_obj);
		if (ret)
			pr_info("RESULT : PASSED\n");
		else
			pr_info("RESULT : FAILED\n");
		pr_info(
		       "-----------------------------------------------\n");

		pr_info(
		       "\n-----------------------------------------------\n");
		pr_info("UT_022 IPFE GET_STATS AFTER IPFE DELETE\n");
		pr_info(
		       "-----------------------------------------------\n");
		ret = stm_fe_ip_get_stats(ip_obj, &info_p);
		if (ret)
			pr_info("RESULT : PASSED\n");
		else
			pr_info("RESULT : FAILED\n");
		pr_info(
		       "-----------------------------------------------\n");

		pr_info(
		       "\n-----------------------------------------------\n");
		pr_info(
		       "UT_023 DELETE IPFE OBJECT AFTER IPFE DELETE\n");
		pr_info(
		       "-----------------------------------------------\n");
		ret = stm_fe_ip_delete(ip_obj);
		if (ret)
			pr_info("RESULT : PASSED\n");
		else
			pr_info("RESULT : FAILED\n");
		pr_info(
		       "-----------------------------------------------\n");
	} else {
		pr_info("RESULT : FAILED\n");
		pr_info(
		       "-----------------------------------------------\n");
	}

	return 0;
}

module_init(stm_fe_api_test_init);

static void __exit stm_fe_api_test_term(void)
{
	pr_info("\nRemoving stmfe API test module ...\n");

	/*demod , lnb , diseq delete */
	/*detach */
}

module_exit(stm_fe_api_test_term);

MODULE_DESCRIPTION("Frontend engine test module for STKPI");
MODULE_AUTHOR("STMicroelectronics Limited");
MODULE_LICENSE("GPL");
