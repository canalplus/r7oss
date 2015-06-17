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

Source file name : demod_scan.c
Author :           shobhit

state mechine implementation of demod

Date        Modification                                    Name
----        ------------                                    --------
25-Jun-11   Created                                         shobhit

************************************************************************/

#include <linux/kernel.h>
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
#include <stm_fe_demod.h>
#include <stm_fe_lnb.h>
#include "demod_scan.h"

#define SCALED_ROLL_OFF		120	/*Lowest value */
#define STEP_SIZE		(2*1000)
#define KZ_MULTIPLIER		1000
#define SAT_STEP(x)		((((x/KZ_MULTIPLIER)*SCALED_ROLL_OFF)/100)/2)
#define TER_STEP(x, y)	(x < (y * KZ_MULTIPLIER) ? y * KZ_MULTIPLIER : x)

#define MAX(a, b)  (((a) > (b)) ? (a) : (b))

uint32_t conversion_bandwidth(stm_fe_demod_bw_t band_width)
{
	uint32_t bw = 0;
	switch (band_width) {
	case STM_FE_DEMOD_BW_1_7:
		bw = 17; /*For DVB-T2 bw=1.7 MHz - fix is required*/
		break;
	case STM_FE_DEMOD_BW_5_0:
		bw = 5;
		break;
	case STM_FE_DEMOD_BW_6_0:
		bw = 6;
		break;
	case STM_FE_DEMOD_BW_7_0:
		bw = 7;
		break;
	case STM_FE_DEMOD_BW_8_0:
		bw = 8;
		break;
	case STM_FE_DEMOD_BW_10_0:
		bw = 10;
		break;
	default:
		break;
	}

	return bw;
}

/*
 * Name: calc_next_freq()
 *
 * Description: next frequency calculation
 *
 */
void calc_nxt_freq(struct stm_fe_demod_s *priv, bool lock,
					stm_fe_demod_event_t *event)
{
	uint32_t curr_sr = 0;
	uint32_t bw = 0;
	stm_fe_demod_channel_info_t *info;
	stm_fe_demod_dvb_ter_scan_config_t *ter;
	stm_fe_demod_dvb_sat_scan_config_t *sat;
	stm_fe_demod_dvb_cab_scan_config_t *cab;
	struct fe_scan_config *s;
	stm_fe_demod_scan_table_form_t table_form = 0;
	uint32_t freq = 0;

	ter = &priv->scanparams.u_config.dvb_ter;
	sat = &priv->scanparams.u_config.dvb_sat;
	cab = &priv->scanparams.u_config.dvb_cab;
	info = &priv->c_tinfo.u_info.scan.demod_info;
	s = &priv->fe_scan;
	bw = conversion_bandwidth(ter->bw);

	if (lock)
		goto locked;
	else
		goto not_locked;

locked:
	switch (info->std) {
	case STM_FE_DEMOD_TX_STD_DVBS:
	case STM_FE_DEMOD_TX_STD_DVBS2:
		table_form = sat->table_form;
		freq = info->u_channel.dvbs2.freq;
		break;

	case STM_FE_DEMOD_TX_STD_DVBT:
		table_form = ter->table_form;
		freq = info->u_channel.dvbt.freq;
		break;

	case STM_FE_DEMOD_TX_STD_DVBC:
		table_form = cab->table_form;
		freq = info->u_channel.dvbc.freq;
		break;

	default:
		pr_err("%s: invalid demod standard!\n", __func__);
		break;
	}

	if (table_form == STM_FE_DEMOD_SCAN_TABLE_FORM_BOUNDARIES) {
		switch (info->std) {
		case STM_FE_DEMOD_TX_STD_DVBS:
		case STM_FE_DEMOD_TX_STD_DVBS2:
			curr_sr = info->u_channel.dvbs2.sr;
			s->step_size = SAT_STEP(curr_sr);
			s->search_freq = freq + s->step_size + STEP_SIZE;
			break;

		case STM_FE_DEMOD_TX_STD_DVBT:
			s->step_size = TER_STEP(s->ter_step, bw);
			s->search_freq = freq + s->step_size;
			break;

		case STM_FE_DEMOD_TX_STD_DVBC:
			s->search_freq = freq + s->step_size;

			if ((s->search_freq > s->end_freq)
			     && (s->sr_table_size > 1)
			     && (s->sr_cnt != s->sr_table_size)) {
				s->search_freq = s->scan_freq[0];
				s->sr_act = cab->sr_table[++s->sr_cnt];
			}
			break;

		default:
			break;
		}
	} else { /* STM_FE_DEMOD_SCAN_TABLE_FORM_LIST */
		if (s->scan_table_cnt == s->table_size - 1) {
			s->search_freq = s->end_freq + step_freq(priv);
		} else {
			s->scan_table_cnt++;
			s->search_freq = s->scan_freq[s->scan_table_cnt];
		}
	}
	return;

not_locked:
	switch (priv->scanparams.scan_stds) {
	case STM_FE_DEMOD_SCAN_TX_STD_DVB_SAT:
		table_form = sat->table_form;
		break;

	case STM_FE_DEMOD_SCAN_TX_STD_DVB_TER:
		table_form = ter->table_form;
		break;

	case STM_FE_DEMOD_SCAN_TX_STD_DVB_CAB:
		table_form = cab->table_form;
		break;

	default:
		pr_err("%s: invalid demod standard!\n", __func__);
		break;
	}
	if (table_form == STM_FE_DEMOD_SCAN_TABLE_FORM_BOUNDARIES) {
		switch (priv->scanparams.scan_stds) {
		case STM_FE_DEMOD_SCAN_TX_STD_DVB_SAT:
			s->step_size = SAT_STEP(s->sr_min);
			s->search_freq = s->search_freq + s->step_size +
				STEP_SIZE;
			break;

		case STM_FE_DEMOD_SCAN_TX_STD_DVB_TER:
			s->search_freq = s->search_freq + s->step_size;
			break;

		case STM_FE_DEMOD_SCAN_TX_STD_DVB_CAB:
			s->search_freq = s->search_freq + s->step_size;
			if ((s->search_freq > s->end_freq)
			    && (s->sr_table_size > 1)
			    && (s->sr_cnt != s->sr_table_size - 1)) {
				s->search_freq = s->scan_freq[0];
				s->sr_act = cab->sr_table[++s->sr_cnt];
			}
			break;

		default:
			break;
		}
	} else { /* STM_FE_DEMOD_SCAN_TABLE_FORM_LIST */
		if (s->scan_table_cnt == s->table_size - 1) {
			s->search_freq = (s->end_freq + step_freq(priv));
		} else {
			(s->scan_table_cnt)++;
			s->search_freq = s->scan_freq[s->scan_table_cnt];
		}
	}

	if (s->search_freq >= s->end_freq + step_freq(priv)) {
		*event = STM_FE_DEMOD_EVENT_SCAN_COMPLETE;
		priv->fe_task.nextstate = FE_TASKSTATE_IDLE;
		priv->info.u_info.scan.demod_info = *info;
		if (evt_notify(priv, event))
			pr_err("%s: evt_notify failed\n", __func__);
	}
}

/*
 * Name: new_scan_setup()
 *
 * Description: scan start frequency calculation
 *
 */
void new_scan_setup(struct stm_fe_demod_s *priv)
{
	uint32_t i = 0;
	uint32_t cnt = 0;
	uint32_t bw = 0;
	bool sat_auto_sr = false;
	stm_fe_demod_dvb_ter_scan_config_t *ter;
	stm_fe_demod_dvb_sat_scan_config_t *sat;
	stm_fe_demod_dvb_cab_scan_config_t *cab;
	struct fe_scan_config *s;

	ter = &priv->scanparams.u_config.dvb_ter;
	sat = &priv->scanparams.u_config.dvb_sat;
	cab = &priv->scanparams.u_config.dvb_cab;
	s = &priv->fe_scan;
	bw = conversion_bandwidth(ter->bw);

	/*
	 * sat_auto_sr flag is put in place to calculate the sr_min value in
	 * case of AUTO SR for the step_freq() fn to handle the demod_scan
	 * check for SCAN_COMPLETE
	 */
	if (!sat->sr_table_num)
		sat_auto_sr = true;
	sat->sr_table_num = MAX(1, sat->sr_table_num);
	cab->sr_table_num = MAX(1, cab->sr_table_num);

	if (priv->scanparams.scan_stds == STM_FE_DEMOD_SCAN_TX_STD_DVB_SAT) {
		if (sat_auto_sr == true)
			s->sr_min = 1000000;
		else
			s->sr_min = sat->sr_table[i];

		for (i = 0; i < sat->sr_table_num - 1; ++i) {
			if (s->sr_min > sat->sr_table[i + 1])
				s->sr_min = sat->sr_table[i + 1];
		}
		s->scan_freq = sat->freq_table;
		if (sat->table_form ==
				STM_FE_DEMOD_SCAN_TABLE_FORM_BOUNDARIES) {
			s->search_freq = s->scan_freq[cnt++];
			s->end_freq = s->scan_freq[cnt];
		} else {
			s->table_size = sat->freq_table_num;
			s->search_freq = s->scan_freq[s->scan_table_cnt];
			s->end_freq = s->scan_freq[s->table_size - 1];

		}
	} else if (priv->scanparams.scan_stds ==
			STM_FE_DEMOD_SCAN_TX_STD_DVB_TER) {
		s->scan_freq = ter->freq_table;
		if (ter->table_form ==
				STM_FE_DEMOD_SCAN_TABLE_FORM_BOUNDARIES) {
			s->search_freq = s->scan_freq[cnt++];
			s->end_freq = s->scan_freq[cnt++];
			s->ter_step = s->scan_freq[cnt];
			s->step_size = (!s->ter_step) ?  bw * KZ_MULTIPLIER
								: s->ter_step;
		} else {
			s->table_size = ter->freq_table_num;
			s->search_freq = s->scan_freq[s->scan_table_cnt];
			s->end_freq = s->scan_freq[s->table_size - 1];
			s->step_size = bw * KZ_MULTIPLIER;

		}
	} else if (priv->scanparams.scan_stds ==
			STM_FE_DEMOD_SCAN_TX_STD_DVB_CAB) {
		s->scan_freq = cab->freq_table;
		if (cab->table_form ==
		    STM_FE_DEMOD_SCAN_TABLE_FORM_BOUNDARIES) {
			s->search_freq = s->scan_freq[cnt++];
			s->end_freq = s->scan_freq[cnt++];
			s->sr_act = cab->sr_table[s->sr_cnt];
			s->sr_table_size = cab->sr_table_num;
			s->step_size = s->scan_freq[cnt];
		} else {
			s->table_size = cab->freq_table_num;
			s->search_freq = s->scan_freq[s->scan_table_cnt];
			s->end_freq = s->scan_freq[s->table_size - 1];
			s->sr_act = cab->sr_table[s->sr_cnt];
		}
	}
}

/*
 * Name: step_freq()
 *
 * Description:to calculate step size
 *
 */
uint32_t step_freq(struct stm_fe_demod_s *priv)
{
	uint32_t freq = 0;
	switch (priv->scanparams.scan_stds) {
	case STM_FE_DEMOD_SCAN_TX_STD_DVB_SAT:
		freq = SAT_STEP(priv->fe_scan.sr_min);
		break;
	case STM_FE_DEMOD_SCAN_TX_STD_DVB_TER:
		freq = 200;	/*200KHz offset */
		break;
	case STM_FE_DEMOD_SCAN_TX_STD_DVB_CAB:
		freq = 300;	/*300KHz offset */
		break;
	default:
		break;
	}

	return freq;
}
