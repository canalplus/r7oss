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

Source file name : main_test.c
Author :           Shobhit

Test file to use stm fe object

Date        Modification                                    Name
----        ------------                                    --------
01-Sep-11   Created                                         Shobhit
18-Sep-12   Modified                                        André
************************************************************************/

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/bug.h>
#include <linux/list.h>
#include <linux/platform_device.h>
#include <linux/stm/plat_dev.h>
#include <linux/stm/demod.h>
#include <linux/stm/lnb.h>
#include <linux/stm/diseqc.h>
#include <linux/delay.h>
#include <linux/unistd.h>
#include <linux/kthread.h>
#include <stm_registry.h>
#include <stm_fe.h>
#include <stm_fe_os.h>

#define FIRST 0
#define LAST 1

//#define LNB_ENABLE
#define SCAN_BETWEEN_BOUNDARIES
/* #define DEMOD367_QAM */
/* #define DEMOD367_OFDM */
/* #define SCAN_TABLE_LIST */
/* #define TER_SCAN */
//#define SAT_SCAN
#define CAB_SCAN
char *stmfe_status_conversion(stm_fe_demod_status_t status);
char *stmfe_fec_conversion(stm_fe_demod_fec_rate_t fec);
char *stmfe_stndard_conversion(stm_fe_demod_tx_std_t status);
char *stmfe_mod_conversion(stm_fe_demod_modulation_t mod);
char *stmfe_hierarchy_conversion(stm_fe_demod_hierarchy_t hierarchy);
char *stmfe_alpha_conversion(stm_fe_demod_hierarchy_alpha_t alpha);
char *stmfe_guard_conversion(stm_fe_demod_guard_t gd);
char *stmfe_fft_mode_conversion(stm_fe_demod_fft_mode_t fft_mode);

struct stmfe_instance_s {
	stm_fe_demod_h obj;
	stm_event_subscription_h fe_subscription;
	fe_semaphore *event_sem;
	int32_t event_id;
};

/*!
* @brief	Converts enum value into corresponding index value
*		of the static array implemented in conversion functions
*/
int stmfe_etoi(int enum_val, int arr_size, int pos)
{
	int i, k;

	i = ffs(enum_val);
	/*check if more than one bit is set */
	k = ffs(enum_val >> i);

	/* if the position of "UNKNOWN" is LAST */
	if (pos) {
		if ((i >= arr_size) || (k))
			i = arr_size - 1;
	}
	/* if the position of "UNKNOWN" is FIRST */
	else {
		if ((i >= arr_size) || (k))
			i = 0;
	}
	return i;
}

char *stmfe_get_event(int event_id)
{
	int i;
	static char event[][35] = {
		"UNKNOWN",
		"STM_FE_DEMOD_EVENT_TUNE_LOCKED",
		"STM_FE_DEMOD_EVENT_TUNE_UNLOCKED",
		"STM_FE_DEMOD_EVENT_TUNE_FAILED",
		"STM_FE_DEMOD_EVENT_SCAN_LOCKED",
		"STM_FE_DEMOD_EVENT_SCAN_COMPLETE",
		"STM_FE_DEMOD_EVENT_LNB_FAILURE",
		"STM_FE_DEMOD_EVENT_HW_FAILURE"
	};

	i = stmfe_etoi(event_id, ARRAY_SIZE(event), FIRST);
	return event[i];
}

int stmfe_print_event(int event_id)
{
	pr_info("\nstm_fe_scan_test : %s reported !\n",
	       stmfe_get_event(event_id));
	return 0;
}

int stmfe_capture_event(void *data)
{
	struct stmfe_instance_s *fe_instance;
	int evt, actual_count;
	stm_event_info_t events[FRONTEND_EVENTS];
	fe_instance = (struct stmfe_instance_s *)data;

	while (!kthread_should_stop()) {
		evt = stm_event_wait(fe_instance->fe_subscription, -1,
				     FRONTEND_EVENTS, &actual_count, events);
		if (!evt) {
			stmfe_print_event(events[0].event.event_id);
			stm_fe_sema_signal(fe_instance->event_sem);
			fe_instance->event_id = events[0].event.event_id;
		} else if (evt == -ECANCELED) {
			pr_info("%s: Forced exit\n", __func__);
			break;
		}

	}
	pr_info("%s: thread exiting....!\n", __func__);
	return 0;
}

static int32_t __init stm_fe_scan_test_init(void)
{
	int32_t sem_ret;
	int32_t ret;
	int32_t i = 0;
	int32_t j = 0;
	int32_t k;
	int evt;
	stm_fe_object_info_t *fe;
	stm_fe_demod_h obj = NULL;
	uint32_t cnt;
	stm_event_subscription_entry_t events_entry;
	struct stmfe_instance_s fe_instance;
	fe_thread capture_event;

	stm_fe_demod_scan_configure_t scan_config;
	int num_std;
	stm_fe_demod_info_t fe_info;
#ifdef LNB_ENABLE
	stm_fe_lnb_config_t lnb_config;
	stm_fe_lnb_h lnb_handle = NULL;
	stm_fe_diseqc_h diseqc_handle = NULL;
#endif
	pr_info("\nLoading scan test module...\n");
	ret = stm_fe_discover(&fe, &cnt);
	if (ret)
		pr_info("%s: discover fail\n", __func__);
	pr_info("\n *****CAPABILITIES OF DEMOD****\n");
	num_std = fe[0].u_caps.demod.num_stds;
	pr_info("\n num std = %d\n", num_std);
	for (j = 0; j < num_std; j++) {
		pr_info("\n cap std[%d] = %s\n", j,
		       stmfe_stndard_conversion(fe[0].u_caps.demod.
						tune_caps[j].std));
		if (fe[0].u_caps.demod.tune_caps[j].std ==
		    STM_FE_DEMOD_TX_STD_DVBS) {
			pr_info(" cap Fmin = %d Fmax = %d",
			       fe[0].u_caps.demod.tune_caps[j].u_tune.
			       dvbs.freq_min,
			       fe[0].u_caps.demod.tune_caps[j].u_tune.
			       dvbs.freq_max);
		} else if (fe[0].u_caps.demod.tune_caps[j].std ==
			   STM_FE_DEMOD_TX_STD_DVBC) {
			pr_info(" cap Fmin = %d Fmax = %d",
			       fe[0].u_caps.demod.tune_caps[j].u_tune.
			       dvbc.freq_min,
			       fe[0].u_caps.demod.tune_caps[j].u_tune.
			       dvbc.freq_max);
		} else if (fe[0].u_caps.demod.tune_caps[j].std ==
			   STM_FE_DEMOD_TX_STD_DVBT) {
			pr_info(" cap Fmin = %d Fmax = %d",
			       fe[0].u_caps.demod.tune_caps[j].u_tune.
			       dvbt.freq_min,
			       fe[0].u_caps.demod.tune_caps[j].u_tune.
			       dvbt.freq_max);
		}
	}
	for (i = 0; i < cnt; i++)
		pr_info("\n  -- %s\n", fe[i].stm_fe_obj);

	if (fe[0].type == STM_FE_DEMOD) {
		ret = stm_fe_demod_new(fe[0].stm_fe_obj, &obj);
		if (ret) {

			pr_info(
			       "%s : Unable to create new instance of demod\n",
			       __func__);
			return -1;
		} else {
			pr_info(
			       "%s : Created new instance of demod\n",
			       __func__);
		}
	}
#ifdef LNB_ENABLE
	if (fe[1].type == STM_FE_LNB) {
		ret =
		    stm_fe_lnb_new(fe[1].stm_fe_obj, obj, &lnb_handle);

		if (ret) {

			pr_info(
			       "%s : Unable to create new instance of lnb\n",
			       __func__);
			return -1;
		} else {
			pr_info(
			       "%s : Created new instance of lnb\n", __func__);
		}
	}
	if (fe[2].type == STM_FE_DISEQC) {
		ret =
		    stm_fe_diseqc_new(fe[2].stm_fe_obj, obj,
				      &diseqc_handle);
		if (ret) {
			pr_info(
			       "%s : Unable to create new instance of diseqc\n",
			       __func__);
			return -1;
		} else {
			pr_info(
			       "%s : Created new instance of diseqc\n",
			       __func__);
		}
	}
#endif
	events_entry.object = obj;
	events_entry.event_mask =
	    STM_FE_DEMOD_EVENT_TUNE_LOCKED | STM_FE_DEMOD_EVENT_TUNE_UNLOCKED |
	    STM_FE_DEMOD_EVENT_TUNE_FAILED | STM_FE_DEMOD_EVENT_SCAN_LOCKED |
	    STM_FE_DEMOD_EVENT_SCAN_COMPLETE | STM_FE_DEMOD_EVENT_LNB_FAILURE |
	    STM_FE_DEMOD_EVENT_HW_FAILURE;
	events_entry.cookie = NULL;

	evt = stm_event_subscription_create(&events_entry, FRONTEND_EVENTS,
					    &fe_instance.fe_subscription);
	if (evt)
		pr_info("%s: Subscription of event fail\n",
		       __func__);
	else
		pr_info("%s: Subscription of event OK\n", __func__);
	fe_instance.obj = obj;
	fe_instance.event_sem =
	    (fe_semaphore *) stm_fe_malloc(sizeof(fe_semaphore));
	sem_ret = stm_fe_sema_init(fe_instance.event_sem, 0);
	if (sem_ret)
		pr_info("%s: stm_fe_sema_init fail\n", __func__);
	else
		pr_info("%s: stm_fe_sema_init OK\n", __func__);
	ret = stm_fe_thread_create(&capture_event, stmfe_capture_event,
				   (void *)&fe_instance, "INP-RF-Cap-Evt", 0);

	if (ret)
		pr_info("%s: stmfe_capture_event creation fail\n",
		       __func__);
	else
		pr_info("%s: stmfe_capture_event creation OK\n",
		       __func__);

#ifdef LNB_ENABLE

	/******************Configure the lnb *********************/
	lnb_config.lnb_tone_state = STM_FE_LNB_TONE_OFF;
	lnb_config.polarization = STM_FE_LNB_PLR_HORIZONTAL;
	if (lnb_config.polarization == STM_FE_LNB_PLR_HORIZONTAL)
		pr_info("\n LNB Configuration : H\n");
	else if (lnb_config.polarization == STM_FE_LNB_PLR_VERTICAL)
		pr_info("\n LNB Configuration : V\n");
	else
		pr_info("\n LNB Configuration : OFF\n");
	ret = stm_fe_lnb_set_config(lnb_handle, &lnb_config);
	if (ret)
		pr_info("%s: Unable to set LNB\n", __func__);
	else
		pr_info("%s: Set LNB parameters\n", __func__);

#endif
	k = 0;
/**************************SCAN API**********/
#ifdef SAT_SCAN
	scan_config.scan_stds = STM_FE_DEMOD_SCAN_TX_STD_DVB_SAT;
	scan_config.u_config.dvb_sat.fecs = STM_FE_DEMOD_FEC_RATE_AUTO;
	scan_config.u_config.dvb_sat.mods =
	    STM_FE_DEMOD_FEATURE_AUTO_MODULATION;
	scan_config.u_config.dvb_sat.sr_table[0] = 2000000;	/* 27500000; */
	scan_config.u_config.dvb_sat.sr_table[1] = 28100000;
	scan_config.u_config.dvb_sat.sr_table_num = 2;

#ifdef SCAN_BETWEEN_BOUNDARIES
	scan_config.u_config.dvb_sat.table_form =
	    STM_FE_DEMOD_SCAN_TABLE_FORM_BOUNDARIES;
	scan_config.u_config.dvb_sat.freq_table[0] = 950000;	/* Start */
	scan_config.u_config.dvb_sat.freq_table[1] = 1110000;/*2150000;*//*End*/
	scan_config.u_config.dvb_sat.freq_table[2] = 36000;	/*step size */
	pr_info("\n Fstart= %d ,Fend= %d,Step Size = %d\n",
	       scan_config.u_config.dvb_sat.freq_table[0],
	       scan_config.u_config.dvb_sat.freq_table[1],
	       scan_config.u_config.dvb_sat.freq_table[2]);
#endif

#ifdef SCAN_TABLE_LIST
	scan_config.u_config.dvb_sat.table_form =
	    STM_FE_DEMOD_SCAN_TABLE_FORM_LIST;
	scan_config.u_config.dvb_sat.freq_table[0] = 958000;	/* Channel 1 */
	scan_config.u_config.dvb_sat.freq_table[1] = 1000000;	/* Channel 2 */
	scan_config.u_config.dvb_sat.freq_table[2] = 1280000;	/* Channel 3 */
	scan_config.u_config.dvb_sat.freq_table[3] = 1486000;	/* Channel 4 */
	scan_config.u_config.dvb_sat.freq_table[4] = 1686000;	/* Channel 5 */
	/* Channel 6 and so on ..*/
	/* scan_config.u_config.dvb_sat.freq_table[5] = 2086000;*/
	scan_config.u_config.dvb_sat.freq_table_num = 5;
	for (k = 0; k < scan_config.u_config.dvb_sat.freq_table_num; k++)
		pr_info("\n Freq Table [%d] = %d\n", k,
		       scan_config.u_config.dvb_sat.freq_table[k]);
#endif
#endif

#ifdef TER_SCAN

	scan_config.scan_stds = STM_FE_DEMOD_SCAN_TX_STD_DVB_TER;
	scan_config.u_config.dvb_ter.fecs = STM_FE_DEMOD_FEC_RATE_AUTO;
	scan_config.u_config.dvb_ter.mods =
	    STM_FE_DEMOD_FEATURE_AUTO_MODULATION;
	scan_config.u_config.dvb_ter.bw = STM_FE_DEMOD_BW_8_0;

#ifdef SCAN_BETWEEN_BOUNDARIES
	scan_config.u_config.dvb_ter.table_form =
	    STM_FE_DEMOD_SCAN_TABLE_FORM_BOUNDARIES;
	scan_config.u_config.dvb_ter.freq_table[0] = 474000;	/* Start */
	scan_config.u_config.dvb_ter.freq_table[1] = 585000;/*2150000;*//* End*/
	scan_config.u_config.dvb_ter.freq_table[2] = 4000;	/*step size */
	pr_info("\n Fstart= %d ,Fend= %d,Step Size = %d\n",
	       scan_config.u_config.dvb_ter.freq_table[0],
	       scan_config.u_config.dvb_ter.freq_table[1],
	       scan_config.u_config.dvb_ter.freq_table[2]);
#endif
#ifdef SCAN_TABLE_LIST
	scan_config.u_config.dvb_ter.table_form =
	    STM_FE_DEMOD_SCAN_TABLE_FORM_LIST;
	scan_config.u_config.dvb_ter.freq_table[0] = 474000;	/* Channel 1 */
	scan_config.u_config.dvb_ter.freq_table[1] = 499000;	/* Channel 2 */
	scan_config.u_config.dvb_ter.freq_table[2] = 514000;	/* Channel 3 */
	scan_config.u_config.dvb_ter.freq_table[3] = 522000;	/* Channel 3 */
	scan_config.u_config.dvb_ter.freq_table[4] = 538000;	/* Channel 4 */
	scan_config.u_config.dvb_ter.freq_table[5] = 580000;	/* Channel 4 */
	/* scan_config.u_config.dvb_sat.freq_table[4] = 1686000;*//* Channel 5*/
	/* scan_config.u_config.dvb_sat.freq_table[5] = 2086000; */
	scan_config.u_config.dvb_ter.freq_table_num = 6;
	for (k = 0; k < scan_config.u_config.dvb_sat.freq_table_num; k++)
		pr_info("\n Freq Table [%d] = %d\n", k,
		       scan_config.u_config.dvb_sat.freq_table[k]);

#endif
#endif

#ifdef CAB_SCAN
	scan_config.scan_stds = STM_FE_DEMOD_SCAN_TX_STD_DVB_CAB;
	scan_config.u_config.dvb_cab.mods = STM_FE_DEMOD_MODULATION_AUTO;
#ifdef SCAN_BETWEEN_BOUNDARIES
	scan_config.u_config.dvb_cab.table_form =
	    STM_FE_DEMOD_SCAN_TABLE_FORM_BOUNDARIES;
	scan_config.u_config.dvb_cab.freq_table[0] = 471000;	/* Start */
	scan_config.u_config.dvb_cab.freq_table[1] = 679000; /*2150000;*//*End*/
	scan_config.u_config.dvb_cab.freq_table[2] = 6000;	/*step size */
	scan_config.u_config.dvb_cab.sr_table[0] = 6875000;	/* 27500000; */
	/* scan_config.u_config.dvb_cab.sr_table[1] = 5875000; */
	/* scan_config.u_config.dvb_cab.sr_table[2] = 5005000; */
	scan_config.u_config.dvb_cab.sr_table_num = 1;
	pr_info("\n Fstart= %d ,Fend= %d,Step Size = %d\n",
	       scan_config.u_config.dvb_cab.freq_table[0],
	       scan_config.u_config.dvb_cab.freq_table[1],
	       scan_config.u_config.dvb_cab.freq_table[2]);
#endif
#ifdef SCAN_TABLE_LIST
	scan_config.u_config.dvb_cab.table_form =
	    STM_FE_DEMOD_SCAN_TABLE_FORM_LIST;
	scan_config.u_config.dvb_cab.freq_table[0] = 474000;	/* Channel 1 */
	scan_config.u_config.dvb_cab.freq_table[1] = 499000;	/* Channel 2 */
	scan_config.u_config.dvb_cab.freq_table[2] = 514000;	/* Channel 3 */
	scan_config.u_config.dvb_cab.freq_table[3] = 663000;	/* Channel 3 */
	/* scan_config.u_config.dvb_cab.freq_table[4] = 545000;*//* Channel 4 */
	/* scan_config.u_config.dvb_cab.freq_table[5] = 580000;*//* Channel 4 */
	scan_config.u_config.dvb_cab.freq_table_num = 4;
	scan_config.u_config.dvb_cab.sr_table[0] = 6875000;	/* 27500000; */
	scan_config.u_config.dvb_cab.sr_table_num = 1;
	for (k = 0; k < scan_config.u_config.dvb_sat.freq_table_num; k++)
		pr_info("\n Freq Table [%d] = %d\n", k,
		       scan_config.u_config.dvb_sat.freq_table[k]);
#endif
#endif
	pr_info("\n going for scanning .....\n");
	/* Start the scan */
	ret =
	    stm_fe_demod_scan(obj, STM_FE_DEMOD_SCAN_NEW,
			      &scan_config);
	if (ret)
		pr_info("%s: Unable to scan\n", __func__);
	else
		pr_info("%s: Scan API OK\n", __func__);

	pr_info("%s: Please wait while we scan...\n", __func__);
	evt = stm_fe_sema_wait(fe_instance.event_sem);

	if (fe_instance.event_id == STM_FE_DEMOD_EVENT_SCAN_LOCKED) {
		pr_info("\n Event = STM_FE_DEMOD_EVENT_SCAN_LOCKED\n");

		ret = stm_fe_demod_get_info(obj, &fe_info);
		pr_info(
		       "%s**************GET INFO***********\n", __func__);

		if (fe_info.u_info.scan.demod_info.std ==
		    STM_FE_DEMOD_TX_STD_DVBS) {
			pr_info(
			       "%s : F= %d,SR= %d,FEC= %s,"
			       "status= %s,SNR=%d ,std= %s\n",
			       __func__,
			       fe_info.u_info.scan.demod_info.u_channel.dvbs.
			       freq,
			       fe_info.u_info.scan.demod_info.u_channel.dvbs.sr,
			       stmfe_fec_conversion(fe_info.u_info.
						    scan.demod_info.u_channel.
						    dvbs.fec),
			       stmfe_status_conversion(fe_info.
						       u_info.scan.demod_info.
						       u_channel.dvbs.status),
			       fe_info.u_info.scan.demod_info.u_channel.dvbs.
			       snr,
			       stmfe_stndard_conversion(fe_info.u_info.scan.
							demod_info.std));
		} else if (fe_info.u_info.scan.demod_info.std ==
			   STM_FE_DEMOD_TX_STD_DVBS2) {
			pr_info(
				"%s : F= %d,SR= %d,FEC= %s,status= %s,Roll off=%d,SNR=%d ,std= %s\n",
			       __func__,
			       fe_info.u_info.scan.demod_info.u_channel.dvbs2.
			       freq,
			       fe_info.u_info.scan.demod_info.u_channel.dvbs2.
			       sr,
			       stmfe_fec_conversion(fe_info.u_info.
				    scan.demod_info.u_channel.dvbs2.fec),
			       stmfe_status_conversion(fe_info.
						       u_info.scan.demod_info.
						       u_channel.dvbs2.status),
			       fe_info.u_info.scan.demod_info.u_channel.dvbs2.
			       roll_off,
			       fe_info.u_info.scan.demod_info.u_channel.dvbs2.
			       snr,
			       stmfe_stndard_conversion(fe_info.u_info.scan.
							demod_info.std));
		} else if (fe_info.u_info.scan.demod_info.std ==
			   STM_FE_DEMOD_TX_STD_DVBT) {
			pr_info(
					"%s : F= %d,FEC= %s,status= %s,SNR=%d ,std= %s\n",
					__func__,
			       fe_info.u_info.scan.demod_info.u_channel.dvbt.
			       freq,
			       stmfe_fec_conversion(fe_info.u_info.
						    scan.demod_info.u_channel.
						    dvbt.fec),
			       stmfe_status_conversion(fe_info.
						       u_info.scan.demod_info.
						       u_channel.dvbt.status),
			       fe_info.u_info.scan.demod_info.u_channel.dvbt.
			       snr,
			       stmfe_stndard_conversion(fe_info.u_info.scan.
							demod_info.std));
		} else if (fe_info.u_info.scan.demod_info.std ==
			   STM_FE_DEMOD_TX_STD_DVBC) {
			pr_info("%s: F= %d,SR= %d,mod=%s,status=%s\n",
			       __func__,
			       fe_info.u_info.scan.demod_info.u_channel.dvbc.
			       freq,
			       fe_info.u_info.scan.demod_info.u_channel.dvbc.sr,
			       stmfe_mod_conversion(fe_info.u_info.
						    scan.demod_info.
						    u_channel.dvbc.mod),
			       stmfe_status_conversion(fe_info.u_info.scan.
						       demod_info.u_channel.
						       dvbc.status));
		}
		for (;;) {
			pr_info(
			       "%s : going for scan Resume\n", __func__);
			ret =
			    stm_fe_demod_scan(obj,
					      STM_FE_DEMOD_SCAN_RESUME,
					      &scan_config);
			if (ret) {
				pr_info(
				       "%s : Unable to scan Resume\n",
				       __func__);
			} else {
				pr_info(
				       "%s : Scan Resume API OK\n", __func__);
			}

			evt = stm_fe_sema_wait(fe_instance.event_sem);

			if (fe_instance.event_id ==
			    STM_FE_DEMOD_EVENT_SCAN_COMPLETE) {
				pr_info(
				    "\n Event = STM_FE_DEMOD_EVENT_SCAN_COMPLETE\n");
				break;
			} else if (fe_instance.event_id ==
				   STM_FE_DEMOD_EVENT_SCAN_LOCKED) {
				pr_info(
				 "\n Event = STM_FE_DEMOD_EVENT_SCAN_LOCKED\n");

				ret =
				    stm_fe_demod_get_info
				    (obj, &fe_info);
				pr_info(
				       "%s**************GET INFO***********\n",
				       __func__);

				if (fe_info.u_info.scan.demod_info.std ==
				    STM_FE_DEMOD_TX_STD_DVBS) {
					pr_info(
					       "%s : F= %d,SR= %d,FEC= %s,status= %s,SNR=%d ,std= %s\n",
					       __func__,
					       fe_info.u_info.scan.
					       demod_info.u_channel.dvbs.freq,
					       fe_info.u_info.scan.
					       demod_info.u_channel.dvbs.sr,
					       stmfe_fec_conversion
					       (fe_info.u_info.
						scan.demod_info.u_channel.dvbs.
						fec),
					       stmfe_status_conversion(fe_info.
						       u_info.scan.demod_info.
							       u_channel.dvbs.
								       status),
					       fe_info.u_info.scan.
					       demod_info.u_channel.dvbs.snr,
					       stmfe_stndard_conversion(fe_info.
							u_info.scan.
							demod_info.std));
				} else if (fe_info.u_info.scan.demod_info.std ==
					   STM_FE_DEMOD_TX_STD_DVBS2) {
					pr_info(
					       "%s : F= %d,SR= %d,FEC= %s,"
					       "status= %s,Roll off=%d,"
					       "SNR=%d ,std= %s\n", __func__,
					       fe_info.u_info.scan.
					       demod_info.u_channel.dvbs2.freq,
					       fe_info.u_info.scan.
					       demod_info.u_channel.dvbs2.sr,
					       stmfe_fec_conversion
					       (fe_info.u_info.
						scan.demod_info.u_channel.dvbs2.
						fec),
					       stmfe_status_conversion(fe_info.
						       u_info.scan.demod_info.
						       u_channel.dvbs2.
						       status),
					       fe_info.u_info.scan.
					       demod_info.u_channel.dvbs2.
					       roll_off,
					       fe_info.u_info.scan.
					       demod_info.u_channel.dvbs2.snr,
					       stmfe_stndard_conversion(fe_info.
							u_info.scan.
							demod_info.std));
				} else if (fe_info.u_info.scan.demod_info.std ==
					   STM_FE_DEMOD_TX_STD_DVBT) {
					pr_info(
						"%s : F= %d,FEC= %s,status= %s,SNR=%d ,std= %s\n",
						__func__,
					       fe_info.u_info.scan.
					       demod_info.u_channel.dvbt.freq,
					       stmfe_fec_conversion
					       (fe_info.u_info.
						scan.demod_info.u_channel.dvbs2.
						fec),
					       stmfe_status_conversion(fe_info.
						       u_info.scan.demod_info.
						       u_channel.dvbt.
						       status),
					       fe_info.u_info.scan.
					       demod_info.u_channel.dvbt.snr,
					       stmfe_stndard_conversion(fe_info.
							u_info.scan.
							demod_info.std));
				}

			}

		}
	} else if (fe_instance.event_id == STM_FE_DEMOD_EVENT_SCAN_COMPLETE) {
		pr_info("\n Event = STM_FE_DEMOD_EVENT_SCAN_COMPLETE\n");
	}

/********************************************/
#ifdef LNB_ENABLE
	ret = stm_fe_diseqc_delete(diseqc_handle);
	if (ret) {

		pr_info(
		       "%s : Unable to delete diseqc object\n", __func__);
	} else {

		pr_info("%s: Deleted diseqc object\n", __func__);
	}
	ret = stm_fe_lnb_delete(lnb_handle);
	if (ret) {

		pr_info(
		       "%s : Unable to delete lnb object\n", __func__);
	} else {

		pr_info("%s: Deleted lnb object\n", __func__);
	}
#endif

	ret = stm_fe_demod_delete(obj);
	if (ret) {

		pr_info(
		       "%s : Unable to delete demod object\n", __func__);
	} else {

		pr_info("%s: Deleted demod object\n", __func__);
	}

	evt = stm_event_subscription_delete(fe_instance.fe_subscription);
	if (evt)
		pr_info("%s: Unsubscription of event fail\n",
		       __func__);
	else
		pr_info("%s: Unsubscription of event OK\n",
		       __func__);
	kthread_stop(capture_event);
	return 0;

}

char *stmfe_status_conversion(stm_fe_demod_status_t status)
{
	static char status_d[][20] = {
		"UNKNOWN", "SIGNAL FOUND", "NO SIGNAL", "EMPTY", "LOCKED"
	};

	return status_d[status];

}

char *stmfe_stndard_conversion(stm_fe_demod_tx_std_t status)
{
	int i = 0;
	static char standard_d[][25] = {
		"DVBS", "DVBS2", "DVBC", "DVBC2", "J83_AC", "J83_B", "DVBT",
		"DVBT2",
		"ATSC", "ISDBT"
	};

	do {
		i++;
		status = status >> 1;
	} while (status);
	return standard_d[i - 1];

}

char *stmfe_fec_conversion(stm_fe_demod_fec_rate_t fec)
{
	int i = 0;
	static char fecrate[][25] = {
		"UNKNOWN", "1_2", "2_3", "3_4", "4_5", "5_6", "6_7", "7_8",
		"8_9", "1_4",
		"1_3", "2_5", "3_5", "9_10"
	};

	do {
		i++;
		fec = fec >> 1;
	} while (fec);
	return fecrate[i];
}

char *stmfe_mod_conversion(stm_fe_demod_modulation_t mod)
{
	int i = 0;
	static char modulation[][25] = {
		"UNKNOWN", "BPSK", "QPSK", "8PSK", "16APSK", "32APSK", "4QAM",
		"16QAM",
		"32QAM", "64QAM",
		"128QAM", "256QAM", "512QAM", "1024QAM", "2VSB", "4VSB", "8VSB",
		"16VSB"
	};

	do {
		i++;
		mod = mod >> 1;
	} while (mod);
	pr_info("\n i = %d\n", i);
	return modulation[i];
}

char *stmfe_hierarchy_conversion(stm_fe_demod_hierarchy_t hierarchy)
{
	int i = 0;
	static char hier[][25] = {
		"NONE", "LOW", "HIGH", "A", "B", "C"
	};

	do {
		i++;
		hierarchy = hierarchy >> 1;
	} while (hierarchy);
	return hier[i];
}

char *stmfe_alpha_conversion(stm_fe_demod_hierarchy_alpha_t alpha)
{
	int i = 0;
	static char hier_alpha[][25] = {
		"NONE", "ALPHA_1", "ALPHA_2", "ALPHA_4"
	};

	do {
		i++;
		alpha = alpha >> 1;
	} while (alpha);
	return hier_alpha[i];
}

char *stmfe_guard_conversion(stm_fe_demod_guard_t gd)
{
	int i = 0;
	static char guard[][25] = {
		"1_4", "1_8", "1_16", "1_32", "1_128", "19_128", "19_256",
		"AUTO"
	};

	do {
		i++;
		gd = gd >> 1;
	} while (gd);
	return guard[i - 1];
}

char *stmfe_fft_mode_conversion(stm_fe_demod_fft_mode_t fft_mode)
{
	int i = 0;
	static char mode[][25] = {
		"1K", "2K", "4K", "8K", "16K", "32K", "AUTO"
	};

	do {
		i++;
		fft_mode = fft_mode >> 1;
	} while (fft_mode);
	return mode[i - 1];
}

static void __exit stm_fe_scan_test_term(void)
{
	pr_info("\nRemoving stmfe scan test module ...\n");
	/*demod , lnb , diseq delete */
	/*detach */
}

module_init(stm_fe_scan_test_init);
module_exit(stm_fe_scan_test_term);

MODULE_DESCRIPTION("Frontend engine test module for STKPI scan feature");
MODULE_AUTHOR("STMicroelectronics Limited");
MODULE_LICENSE("GPL");
