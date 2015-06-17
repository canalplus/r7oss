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

Source file name : utility_tests.c
Author :           Shobhit

Test file to use diseqc and lnbh24

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

char *stmfe_status_conversion(stm_fe_demod_status_t status);

char *stmfe_fec_conversion(stm_fe_demod_fec_rate_t fec);

static int32_t __init utility_tests_init(void)
{
	int32_t ret;
	int32_t i = 0;
	stm_fe_object_info_t *fe;
	stm_fe_demod_h obj = NULL;
	stm_fe_lnb_h lnb_handle = NULL;
	stm_fe_diseqc_h diseqc_handle = NULL;
	uint32_t cnt;

	stm_fe_lnb_config_t lnb_config;
	stm_fe_diseqc_msg_t diseqc_msg[2];

	pr_info("\nLoading module utility tests ...\n");

/*stm_event_subscription_create*/

	ret = stm_fe_discover(&fe, &cnt);
	if (ret)
		pr_info("%s: discover fail\n", __func__);

	for (i = 0; i < cnt; i++)
		pr_info("\n  -- %s\n", fe[i].stm_fe_obj);

	if (fe[0].type == STM_FE_DEMOD) {
		ret = stm_fe_demod_new(fe[0].stm_fe_obj, &obj);
		if (ret) {

			pr_info(
			       "%s : Unable to create new instance of demod\n",
			       __func__);
		} else {

			pr_info(
			       "%s : able to create new instance of demod\n",
			       __func__);
		}
	}

	for (i = 0; i < cnt; i++) {
		if (fe[i].type == STM_FE_LNB)  {
			ret = stm_fe_lnb_new(fe[i].stm_fe_obj, obj,
					&lnb_handle);

			if (ret) {

				pr_info(
						"%s : Unable to create new instance of lnb\n",
						__func__);
				break;
			} else {

				pr_info(
						"%s : able to create new instance of lnb\n",
						__func__);
				break;
			}

		}
	}

	for (i = 0; i < cnt; i++) {
		if (fe[i].type == STM_FE_DISEQC) {
			ret =
			     stm_fe_diseqc_new(fe[i].stm_fe_obj, obj,
						&diseqc_handle);
			if (ret) {

				pr_info(
						"%s : Unable to create new instance of diseqc\n",
						__func__);
				break;
			} else {

				pr_info(
						"%s : able to create new instance of diseqc\n",
						__func__);
				break;
			}

		}
	}

	/**************Configure the lnb ******************/

	lnb_config.lnb_tone_state = STM_FE_LNB_TONE_OFF;
	lnb_config.polarization = STM_FE_LNB_PLR_HORIZONTAL;
	ret = stm_fe_lnb_set_config(lnb_handle, &lnb_config);
	if (ret)
		pr_info("%s: Unable to set LNB\n", __func__);
	else
		pr_info("%s: able to set LNB\n", __func__);
	mdelay(500);

	/**********************DiSEqC Transfer ************************/

	/* prepare send packet */
	diseqc_msg[0].msg[0] = 0xe2;	/* 1st byte */
	diseqc_msg[0].msg[1] = 0x10;
	diseqc_msg[0].msg[2] = 0x22;
	diseqc_msg[0].msg_len = 3;
	diseqc_msg[0].op = STM_FE_DISEQC_TRANSFER_SEND;
	diseqc_msg[0].timegap = 0;	/* No time gap */
	diseqc_msg[0].timeout = 0;	/* No timeout for sending */

	diseqc_msg[1].op = STM_FE_DISEQC_TRANSFER_RECEIVE;
	diseqc_msg[1].timegap = 0;	/* No time gap */
	diseqc_msg[1].timeout = 300;	/* timeout for receiving */

	ret =
	    stm_fe_diseqc_transfer(diseqc_handle, STM_FE_DISEQC_COMMAND,
				   diseqc_msg, 2);
	pr_info("\n***** DISEQC REPLY = 0x%x *****\n", diseqc_msg[1].msg[0]);
	mdelay(10);
	/* prepare send packet */
	diseqc_msg[0].msg[0] = 0xe2;	/* 1st byte */
	diseqc_msg[0].msg[1] = 0x10;
	diseqc_msg[0].msg[2] = 0x20;
	diseqc_msg[0].msg_len = 3;
	diseqc_msg[0].op = STM_FE_DISEQC_TRANSFER_SEND;
	diseqc_msg[0].timegap = 0;	/* No time gap */
	diseqc_msg[0].timeout = 0;	/* No timeout for sending */

	diseqc_msg[1].op = STM_FE_DISEQC_TRANSFER_RECEIVE;
	diseqc_msg[1].timegap = 0;	/* No time gap */
	diseqc_msg[1].timeout = 300;	/* timeout for receiving */

	ret = stm_fe_diseqc_transfer(diseqc_handle, STM_FE_DISEQC_COMMAND,
				     diseqc_msg, 2);
	pr_info("\n***** DISEQC REPLY = 0x%x *****\n", diseqc_msg[1].msg[0]);
	mdelay(10);

	/* prepare send packet */
	diseqc_msg[0].msg[0] = 0xe2;	/* 1st byte */
	diseqc_msg[0].msg[1] = 0x10;
	diseqc_msg[0].msg[2] = 0x21;
	diseqc_msg[0].msg_len = 3;
	diseqc_msg[0].op = STM_FE_DISEQC_TRANSFER_SEND;
	diseqc_msg[0].timegap = 0;	/* No time gap */
	diseqc_msg[0].timeout = 0;	/* No timeout for sending */

	diseqc_msg[1].op = STM_FE_DISEQC_TRANSFER_RECEIVE;
	diseqc_msg[1].timegap = 0;	/* No time gap */
	diseqc_msg[1].timeout = 300;	/* timeout for receiving */

	ret = stm_fe_diseqc_transfer(diseqc_handle, STM_FE_DISEQC_COMMAND,
				     diseqc_msg, 2);
	pr_info("\n***** DISEQC REPLY = 0x%x *****\n", diseqc_msg[1].msg[0]);
	mdelay(10);

	return 0;

}

module_init(utility_tests_init);

static void __exit utility_tests_term(void)
{
	pr_info("\nRemoving utiltiy tests module ...\n");

}

module_exit(utility_tests_term);

MODULE_DESCRIPTION("Frontend engine test module for STKPI");
MODULE_AUTHOR("STMicroelectronics Limited");
MODULE_LICENSE("GPL");
