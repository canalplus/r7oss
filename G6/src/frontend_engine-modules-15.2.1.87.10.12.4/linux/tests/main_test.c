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
20-Jun-11   Created                                         Shobhit
01-Aug-11   Added DVB-C/T                                   Ankur
18-Sep-12   Fix includes                                    André
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
#include <stm_registry.h>
#include <linux/kthread.h>
#include <stm_fe.h>
#include <stm_fe_os.h>

#define FIRST 0
#define LAST 1

#define LIVE_FEED
/*#define MODULATOR_DVBS1
#define MODULATOR_DVBS2*/
#define LNB_ENABLE
/*#define DEMOD367_QAM*/
/*#define DEMOD367_OFDM*/
/*This is to be enabled with DEMOD367_QAM/DEMOD367_OFDM*/
/*#define DYNAMIC_TC_SWITCHING*/

#define TIME(tstart, tend) ((tend.tv_sec - tstart.tv_sec) * 1000 \
				+ (tend.tv_usec - tstart.tv_usec) / 1000)

struct stmfe_instance_s {
	stm_fe_demod_h obj;
	stm_event_subscription_h fe_subscription;
	fe_semaphore *event_sem;
	int32_t event_id;
};

char *stmfe_status_conversion(stm_fe_demod_status_t status);
char *stmfe_fec_conversion(stm_fe_demod_fec_rate_t fec);
char *stmfe_stndard_conversion(stm_fe_demod_tx_std_t status);
char *stmfe_mod_conversion(stm_fe_demod_modulation_t mod);
char *stmfe_hierarchy_conversion(stm_fe_demod_hierarchy_t hierarchy);
char *stmfe_alpha_conversion(stm_fe_demod_hierarchy_alpha_t alpha);
char *stmfe_guard_conversion(stm_fe_demod_guard_t gd);
char *stmfe_fft_mode_conversion(stm_fe_demod_fft_mode_t fft_mode);
char *stmfe_roll_off_conversion(stm_fe_demod_roll_off_t rf);
char *stmfe_bw_conversion(stm_fe_demod_bw_t bw);

void dynamic_tc_switching(stm_fe_demod_h obj,
			  stm_event_subscription_h fe_subscription)
{
	int32_t ch = 0;
#ifdef DYNAMIC_TC_SWITCHING
	stm_fe_demod_tune_params_t tune_param;
	stm_event_info_t events[FRONTEND_EVENTS];
	stm_fe_demod_info_t fe_info;
	struct timeval tstart, tend;
	int32_t evt;
	int32_t k = 0;
	int32_t ret;
	int32_t actual_count;
#endif
	for (ch = 0; ch < 2; ch++) {
#ifdef DEMOD367_OFDM
		pr_info("\n tuning for DVB-T\n");
		/**DVB-T***/
		tune_param.std = STM_FE_DEMOD_TX_STD_DVBT;
		tune_param.u_tune.dvbt.fec = STM_FE_DEMOD_FEC_RATE_1_2;
		tune_param.u_tune.dvbt.guard = STM_FE_DEMOD_GUARD_1_8;
		tune_param.u_tune.dvbt.fft_mode = STM_FE_DEMOD_FFT_MODE_8K;
		tune_param.u_tune.dvbt.hierarchy = STM_FE_DEMOD_HIERARCHY_NONE;
		tune_param.u_tune.dvbt.freq = 514000;	/* 1328000;  //khz */
		tune_param.u_tune.dvbt.mod = STM_FE_DEMOD_MODULATION_QPSK;
		tune_param.u_tune.dvbt.inv =
		    STM_FE_DEMOD_SPECTRAL_INVERSION_AUTO;
		tune_param.u_tune.dvbt.bw = STM_FE_DEMOD_BW_8_0;

		do_gettimeofday(&tstart);
		ret = stm_fe_demod_tune(obj, &tune_param);
		if (ret)
			pr_info("%s: Unable to tune\n", __func__);
		else
			pr_info("%s: Tune API OK\n", __func__);

		evt =
		    stm_event_wait(fe_subscription, -1, FRONTEND_EVENTS,
				   &actual_count, events);

		if (!evt) {
			if (events[0].event.event_id ==
			    STM_FE_DEMOD_EVENT_TUNE_LOCKED) {
				do_gettimeofday(&tend);
				pr_info(
					"\nEvent STM_FE_DEMOD_EVENT_TUNE_LOCKED in %ld ms\n",
				     TIME(tstart, tend));
				for (k = 0; k < 50; k++) {
					ret =
					    stm_fe_demod_get_info(obj,
								  &fe_info);
					pr_info(
					       "%s**************GET INFO***********\n",
					       __func__);

					if (fe_info.u_info.tune.
					    demod_info.std ==
					    STM_FE_DEMOD_TX_STD_DVBT) {
						pr_info(
						       "%s : F= %d, FEC= %s, MOD= %s,"
						       "status= %s, BER=%d10^6,"
						       "SNR=%d , RF Level=%d std= %s\n"
						       "BW= %sMHz HIER= %s, ALPHA= %s, GUARD= %s, FFT_MODE= %s",
						       __func__,
						       fe_info.u_info.tune.
						       demod_info.u_channel.
						       dvbt.freq,
						       stmfe_fec_conversion
						       (fe_info.u_info.
							tune.demod_info.
							u_channel.dvbt.fec),
						       stmfe_mod_conversion
						       (fe_info.u_info.
							tune.demod_info.
							u_channel.dvbt.mod),
						       stmfe_status_conversion
						       (fe_info.u_info.
							tune.demod_info.
							u_channel.dvbt.status),
						       fe_info.u_info.tune.
						       demod_info.u_channel.
						       dvbt.ber,
						       fe_info.u_info.tune.
						       demod_info.u_channel.
						       dvbt.snr,
						       fe_info.u_info.tune.
						       demod_info.u_channel.
						       dvbt.signal_strength,
						       stmfe_stndard_conversion
						       (fe_info.u_info.
							tune.demod_info.std),
						       stmfe_bw_conversion
						       (fe_info.u_info.
							tune.demod_info.
							u_channel.dvbt.bw),
						      stmfe_hierarchy_conversion
						       (fe_info.u_info.
							tune.demod_info.
							u_channel.dvbt.
							hierarchy),
						       stmfe_alpha_conversion
						       (fe_info.u_info.
							tune.demod_info.
							u_channel.dvbt.alpha),
						       stmfe_guard_conversion
						       (fe_info.u_info.
							tune.demod_info.
							u_channel.dvbt.guard),
						       stmfe_fft_mode_conversion
						       (fe_info.u_info.
							tune.demod_info.
							u_channel.dvbt.
							fft_mode));
					}

					mdelay(800);

				}
			} else if (events[0].event.event_id ==
				   STM_FE_DEMOD_EVENT_TUNE_UNLOCKED) {
				pr_info("\nEvent STM_FE_DEMOD_EVENT_TUNE_UNLOCKED\n");
			} else if (events[0].event.event_id ==
				   STM_FE_DEMOD_EVENT_TUNE_FAILED) {
				do_gettimeofday(&tend);
				pr_info("\nEvent STM_FE_DEMOD_EVENT_TUNE_FAILED in %ld ms",
				       TIME(tstart, tend));
			}
		}
#endif
#ifdef DEMOD367_QAM
		pr_info("\n tuning for DVB-C\n");

		/**DVB-C***/
		tune_param.std = STM_FE_DEMOD_TX_STD_DVBC;
		tune_param.u_tune.dvbc.freq = 663000;	/*1328000;*/    /*khz*/
		tune_param.u_tune.dvbc.mod = STM_FE_DEMOD_MODULATION_64QAM;
		tune_param.u_tune.dvbc.inv =
		    STM_FE_DEMOD_SPECTRAL_INVERSION_AUTO;
		tune_param.u_tune.dvbc.sr = 6875000;

		do_gettimeofday(&tstart);
		ret = stm_fe_demod_tune(obj, &tune_param);
		if (ret)
			pr_info("%s: Unable to tune\n", __func__);
		else
			pr_info("%s: Tune API OK\n", __func__);

		evt =
		    stm_event_wait(fe_subscription, -1, FRONTEND_EVENTS,
				   &actual_count, events);

		if (!evt) {
			if (events[0].event.event_id ==
			    STM_FE_DEMOD_EVENT_TUNE_LOCKED) {
				do_gettimeofday(&tend);
				pr_info(
					"\n Event = STM_FE_DEMOD_EVENT_TUNE_LOCKED in %ld ms\n",
				     TIME(tstart, tend));
				for (k = 0; k < 5; k++) {
					ret =
					    stm_fe_demod_get_info(obj,
								  &fe_info);
					pr_info(
					       "%s**************GET INFO***********\n",
					       __func__);

					if (fe_info.u_info.tune.
					    demod_info.std ==
					    STM_FE_DEMOD_TX_STD_DVBC) {
						pr_info(
						       "%s : F= %d,SR= %d,MOD= %s,"
						       "status= %s,BER=%d10^6,"
						       "SNR=%d , RF Level=%d std= %s\n",
						       __func__,
						       fe_info.u_info.tune.
						       demod_info.u_channel.
						       dvbc.freq,
						       fe_info.u_info.tune.
						       demod_info.u_channel.
						       dvbc.sr,
						       stmfe_mod_conversion
						       (fe_info.u_info.
							tune.demod_info.
							u_channel.dvbc.mod),
						       stmfe_status_conversion
						       (fe_info.u_info.
							tune.demod_info.
							u_channel.dvbc.status),
						       fe_info.u_info.tune.
						       demod_info.u_channel.
						       dvbc.ber,
						       fe_info.u_info.tune.
						       demod_info.u_channel.
						       dvbc.snr,
						       fe_info.u_info.tune.
						       demod_info.u_channel.
						       dvbc.signal_strength,
						       stmfe_stndard_conversion
						       (fe_info.u_info.
							tune.demod_info.std));
					}

					mdelay(800);

				}
			} else if (events[0].event.event_id ==
				   STM_FE_DEMOD_EVENT_TUNE_UNLOCKED) {
				pr_info(
				       "\n Event = STM_FE_DEMOD_EVENT_TUNE_UNLOCKED\n");
			} else if (events[0].event.event_id ==
				   STM_FE_DEMOD_EVENT_TUNE_FAILED) {
				do_gettimeofday(&tend);
				pr_info(
				       "\n Event = STM_FE_DEMOD_EVENT_TUNE_FAILED in %ld ms",
				       TIME(tstart, tend));
			}
		}
#endif
	}

}

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
	pr_info("\nstm_fe_test : %s reported !\n",
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

static int32_t __init stm_fe_test_init(void)
{
	int32_t sem_ret;
	int32_t ret;
	int32_t i = 0;
	int32_t j = 0;
	int32_t wait_ctr = 10;
	int evt;
	stm_fe_object_info_t *fe;
	stm_fe_demod_h obj = NULL;
	uint32_t cnt;
	stm_fe_demod_tune_params_t tune_param;
	stm_event_subscription_entry_t events_entry;

	struct stmfe_instance_s fe_instance;
	fe_thread capture_event;

#ifndef DYNAMIC_TC_SWITCHING
	struct timeval tstart, tend;
	stm_fe_demod_info_t fe_info;
	int32_t k = 0;
#endif
	int num_std;
#ifdef LNB_ENABLE
	stm_fe_lnb_config_t lnb_config;
	stm_fe_lnb_h lnb_handle = NULL;
	stm_fe_diseqc_h diseqc_handle = NULL;
#endif
	pr_info("\nLoading main test module...\n");

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
	lnb_config.polarization = STM_FE_LNB_PLR_VERTICAL;
	ret = stm_fe_lnb_set_config(lnb_handle, &lnb_config);
	if (ret)
		pr_info("%s: Unable to set LNB\n", __func__);
	else
		pr_info("%s: Set LNB parameters\n", __func__);

#endif

	/*************TUNING**************/
/**DVB-S***/
#ifdef MODULATOR_DVBS1
	tune_param.std = STM_FE_DEMOD_TX_STD_DVBS;
	tune_param.u_tune.dvbs.freq = 1010000;	/* khz */
	tune_param.u_tune.dvbs.fec = STM_FE_DEMOD_FEC_RATE_3_4;
	tune_param.u_tune.dvbs.inv = STM_FE_DEMOD_SPECTRAL_INVERSION_AUTO;
	tune_param.u_tune.dvbs.sr = 24444444;
	pr_info("\nF=%d , SR=%d\n", tune_param.u_tune.dvbs.freq,
	       tune_param.u_tune.dvbs.sr);
#endif
/**DVB-S2***/
#ifdef MODULATOR_DVBS2
	tune_param.std = STM_FE_DEMOD_TX_STD_DVBS2;
	tune_param.u_tune.dvbs2.freq = 1010000;	/* khz */
	tune_param.u_tune.dvbs2.fec = STM_FE_DEMOD_FEC_RATE_3_4;
	tune_param.u_tune.dvbs2.inv = STM_FE_DEMOD_SPECTRAL_INVERSION_AUTO;
	tune_param.u_tune.dvbs2.sr = 24444444;
	tune_param.u_tune.dvbs2.roll_off = STM_FE_DEMOD_ROLL_OFF_0_35;
	pr_info("\nF=%d , SR=%d\n", tune_param.u_tune.dvbs2.freq,
	       tune_param.u_tune.dvbs2.sr);
#endif
/**Live feed**/
#ifdef LIVE_FEED
 /**DVB-S***/
	tune_param.std = STM_FE_DEMOD_TX_STD_DVBS;
	tune_param.u_tune.dvbs.freq = 1010000;	/* 1328000;       //khz */
	tune_param.u_tune.dvbs.fec = STM_FE_DEMOD_FEC_RATE_3_4;
	tune_param.u_tune.dvbs.inv = STM_FE_DEMOD_SPECTRAL_INVERSION_AUTO;
	tune_param.u_tune.dvbs.sr = 27500000;
	pr_info("\nF=%d , SR=%d\n", tune_param.u_tune.dvbs.freq,
	       tune_param.u_tune.dvbs.sr);
#endif

#ifdef DEMOD367_OFDM
 /**DVB-T***/
	tune_param.std = STM_FE_DEMOD_TX_STD_DVBT;
	tune_param.u_tune.dvbt.fec = STM_FE_DEMOD_FEC_RATE_1_2;
	tune_param.u_tune.dvbt.guard = STM_FE_DEMOD_GUARD_1_8;
	tune_param.u_tune.dvbt.fft_mode = STM_FE_DEMOD_FFT_MODE_8K;
	tune_param.u_tune.dvbt.hierarchy = STM_FE_DEMOD_HIERARCHY_NONE;
	tune_param.u_tune.dvbt.freq = 514000;	/* 1328000;       //khz */
	tune_param.u_tune.dvbt.mod = STM_FE_DEMOD_MODULATION_QPSK;
	tune_param.u_tune.dvbt.inv = STM_FE_DEMOD_SPECTRAL_INVERSION_AUTO;
	tune_param.u_tune.dvbt.bw = STM_FE_DEMOD_BW_8_0;
	pr_info("\nF=%d , BW=%d\n", tune_param.u_tune.dvbt.freq,
	       tune_param.u_tune.dvbt.bw);
#endif

#ifdef DEMOD367_QAM
	/**DVB-C***/
	tune_param.std = STM_FE_DEMOD_TX_STD_DVBC;
	tune_param.u_tune.dvbc.freq = 663000;	/* 1328000;       //khz */
	tune_param.u_tune.dvbc.mod = STM_FE_DEMOD_MODULATION_64QAM;
	tune_param.u_tune.dvbc.inv = STM_FE_DEMOD_SPECTRAL_INVERSION_AUTO;
	tune_param.u_tune.dvbc.sr = 6875000;
	pr_info("\nF=%d , SR=%d\n", tune_param.u_tune.dvbc.freq,
	       tune_param.u_tune.dvbc.sr);
#endif

#ifdef DYNAMIC_TC_SWITCHING
	dynamic_tc_switching(obj, fe_instance.fe_subscription);
#else
	pr_info("\n going for tune...\n");
	do_gettimeofday(&tstart);
	ret = stm_fe_demod_tune(obj, &tune_param);
	if (ret)
		pr_info("%s: Unable to tune\n", __func__);
	else
		pr_info("%s: Tune API OK\n", __func__);

	while (wait_ctr) {
		evt =
		    stm_fe_sema_wait_timeout(fe_instance.event_sem,
					     stm_fe_delay_ms(1000));
		if (fe_instance.event_id == STM_FE_DEMOD_EVENT_TUNE_LOCKED)
			break;
		pr_info("%s: Waiting for Lock...%d", __func__,
		       wait_ctr);
		wait_ctr--;
	}

	if (fe_instance.event_id == STM_FE_DEMOD_EVENT_TUNE_LOCKED) {
		do_gettimeofday(&tend);
		pr_info("\n Event = STM_FE_DEMOD_EVENT_TUNE_LOCKED in %ld ms\n",
		       TIME(tstart, tend));
		for (k = 0; k < 5; k++) {
			ret = stm_fe_demod_get_info(obj, &fe_info);
			pr_info(
			       "%s**************GET INFO***********\n",
			       __func__);

			if (fe_info.u_info.tune.demod_info.std ==
			    STM_FE_DEMOD_TX_STD_DVBS) {
				pr_info(
				       "%s : F= %d,SR= %d,FEC= %s,"
				       "status= %s,SNR=%d ,std= %s\n",
				       __func__,
				       fe_info.u_info.tune.demod_info.u_channel.
				       dvbs.freq,
				       fe_info.u_info.tune.demod_info.u_channel.
				       dvbs.sr,
				       stmfe_fec_conversion(fe_info.u_info.
					    tune.demod_info.u_channel.dvbs.fec),
				       stmfe_status_conversion
				       (fe_info.u_info.tune.demod_info.
					u_channel.dvbs.status),
				       fe_info.u_info.tune.demod_info.u_channel.
				       dvbs.snr,
				       stmfe_stndard_conversion(fe_info.u_info.
						tune.demod_info.std));
			} else if (fe_info.u_info.tune.demod_info.std ==
				   STM_FE_DEMOD_TX_STD_DVBS2) {
				pr_info(
						"%s : F= %d,SR= %d,FEC= %s,status= %s,Roll off=%s,SNR=%d,std= %s\n",
						__func__,
				       fe_info.u_info.tune.demod_info.u_channel.
				       dvbs2.freq,
				       fe_info.u_info.tune.demod_info.u_channel.
				       dvbs2.sr,
				       stmfe_fec_conversion(fe_info.u_info.tune.
							    demod_info.
							    u_channel.
							    dvbs2.fec),
				       stmfe_status_conversion(fe_info.u_info.
					       tune.demod_info.u_channel.
							       dvbs2.status),
				       stmfe_roll_off_conversion(fe_info.u_info.
						 tune.demod_info.u_channel.
								dvbs2.roll_off),
				       fe_info.u_info.tune.demod_info.u_channel.
				       dvbs2.snr,
				       stmfe_stndard_conversion(fe_info.u_info.
								tune.demod_info.
								std));
			} else if (fe_info.u_info.tune.demod_info.std ==
				   STM_FE_DEMOD_TX_STD_DVBC) {
				pr_info(
						"%s : F= %d,SR= %d,MOD= %s,status= %s,BER=%d10^6,SNR=%d , RF Level=%d std= %s\n",
				       __func__,
				       fe_info.u_info.tune.demod_info.u_channel.
				       dvbc.freq,
				       fe_info.u_info.tune.demod_info.u_channel.
				       dvbc.sr,
				       stmfe_mod_conversion(fe_info.u_info.
					    tune.demod_info.u_channel.dvbc.mod),
				       stmfe_status_conversion
				       (fe_info.u_info.tune.demod_info.
					u_channel.dvbc.status),
				       fe_info.u_info.tune.demod_info.u_channel.
				       dvbc.ber,
				       fe_info.u_info.tune.demod_info.u_channel.
				       dvbc.snr,
				       fe_info.u_info.tune.demod_info.u_channel.
				       dvbc.signal_strength,
				       stmfe_stndard_conversion(fe_info.u_info.
							tune.demod_info.std));
			} else if (fe_info.u_info.tune.demod_info.std ==
				   STM_FE_DEMOD_TX_STD_DVBT) {
				pr_info(
						"%s : F= %d, FEC= %s, MOD= %s,status= %s, BER=%d10^6,SNR=%d , RF Level=%d std= %s\nBW= %sMHz HIER= %s, ALPHA= %s, GUARD= %s, FFT_MODE= %s",
				       __func__,
				       fe_info.u_info.tune.demod_info.u_channel.
				       dvbt.freq,
				       stmfe_fec_conversion(fe_info.u_info.
					    tune.demod_info.u_channel.dvbt.fec),
				       stmfe_mod_conversion(fe_info.u_info.
					    tune.demod_info.u_channel.dvbt.mod),
				       stmfe_status_conversion
				       (fe_info.u_info.tune.demod_info.
					u_channel.dvbt.status),
				       fe_info.u_info.tune.demod_info.u_channel.
				       dvbt.ber,
				       fe_info.u_info.tune.demod_info.u_channel.
				       dvbt.snr,
				       fe_info.u_info.tune.demod_info.u_channel.
				       dvbt.signal_strength,
				       stmfe_stndard_conversion(fe_info.u_info.
								tune.
								demod_info.std),
				       stmfe_bw_conversion(fe_info.u_info.tune.
							   demod_info.u_channel.
							   dvbt.bw),
				       stmfe_hierarchy_conversion(fe_info.
					  u_info.tune.demod_info.u_channel.
							  dvbt.hierarchy),
				       stmfe_alpha_conversion(fe_info.u_info.
					  tune.demod_info.u_channel.dvbt.alpha),
				       stmfe_guard_conversion(fe_info.u_info.
					  tune.demod_info.u_channel.dvbt.guard),
				       stmfe_fft_mode_conversion(fe_info.u_info.
					       tune.demod_info.u_channel.dvbt.
					       fft_mode));
			}

			mdelay(800);

		}
	} else if (fe_instance.event_id == STM_FE_DEMOD_EVENT_TUNE_UNLOCKED) {
		pr_info(
		       "\n Event = STM_FE_DEMOD_EVENT_TUNE_UNLOCKED\n");
	} else if (fe_instance.event_id == STM_FE_DEMOD_EVENT_TUNE_FAILED) {
		do_gettimeofday(&tend);
		pr_info(
		       "\n Event = STM_FE_DEMOD_EVENT_TUNE_FAILED in %ld ms",
		       TIME(tstart, tend));
	}
#endif
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
		"DVBT2", "ATSC", "ISDBT"
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

char *stmfe_roll_off_conversion(stm_fe_demod_roll_off_t rf)
{
	int i, k;
	static char roll_off[][10] = {
		"UNKNOWN", "0.20", "0.25", "0.35"
	};

	i = ffs(rf);
	/*check if more than one bit is set */
	k = ffs(rf >> i);

	if ((i >= ARRAY_SIZE(roll_off)) || (k))
		i = 0;

	return roll_off[i];
}

char *stmfe_bw_conversion(stm_fe_demod_bw_t bw)
{
	int i = 0;
	static char bw_mhz[][25] = {
		"1.7", "5", "6", "7", "8", "10"
	};

	do {
		i++;
		bw = bw >> 1;
	} while (bw);
	return bw_mhz[i];
}

static void __exit stm_fe_test_term(void)
{
	pr_info("\nRemoving stmfe test module ...\n");

	/*demod , lnb , diseq delete */
	/*detach */
}

module_init(stm_fe_test_init);
module_exit(stm_fe_test_term);

MODULE_DESCRIPTION("Frontend engine test module for STKPI");
MODULE_AUTHOR("STMicroelectronics Limited");
MODULE_LICENSE("GPL");
