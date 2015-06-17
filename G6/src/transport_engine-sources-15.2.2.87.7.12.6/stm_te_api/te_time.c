/******************************************************************************
Copyright (C) 2011, 2012 STMicroelectronics. All Rights Reserved.

This file is part of the Transport Engine Library.

Transport Engine is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License version 2 as published by
the Free Software Foundation.

Transport Engine is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with Transport Engine; see the file COPYING.  If not, write to the Free
Software Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

The Transport Engine Library may alternatively be licensed under a proprietary
license from ST.

Source file name : te_time.c

Defines Transport Engine timestamp processing functions

Functions in this file are common to PCR and TS Index filters
******************************************************************************/

#include <stm_te_dbg.h>
#include <pti_hal_api.h>
#include <pti_tshal_api.h>

#include "te_object.h"
#include "te_time.h"
#include "te_demux.h"
#include "te_pcr_filter.h"
#include "te_ts_index_filter.h"

/* Constants for handling STC wrap on 33-bit STC clock */
#define STC_MAX 0x1ffffffffLL
#define STC_WRAP_ADJUST (STC_MAX + 1)
#define STC_MSB (STC_WRAP_ADJUST >> 1)

static int te_get_time_point(struct te_obj *demux, te_pcr_time_point_t *tp)
{
	int err;

	err = te_demux_get_device_time(demux, &tp->stc_time, &tp->sys_time);
	if (err)
		return err;

	tp->stc_time &= STC_MAX;

	return err;
}

void te_convert_raw_data_to_point(te_pcr_data_t *raw_pcr_data,
		te_pcr_time_point_t *tp)
{
	__u16 *pcr_segment = (__u16 *) (raw_pcr_data);

	/* TP outputs PCR and CLK in big endian order. Convert the format of
	 * the pcr data in place */
	be16_to_cpus(pcr_segment);
	be16_to_cpus(pcr_segment + 1);
	be16_to_cpus(pcr_segment + 2);
	be16_to_cpus(pcr_segment + 3);
	be16_to_cpus(pcr_segment + 4);
	be16_to_cpus(pcr_segment + 5);

	/* 32 MSBs first. Shift and include last lsbit */
	tp->pcr_time = ((unsigned long long)(pcr_segment[0]) << 17) |
	    ((unsigned long long)(pcr_segment[1]) << 1) |
	    ((unsigned long long)(pcr_segment[2]) >> 15);

	/* 32 MSBs first. Shift and include last lsbit */
	tp->stc_time = ((unsigned long long)(pcr_segment[3]) << 17) |
	    ((unsigned long long)(pcr_segment[4]) << 1) |
	    ((unsigned long long)(pcr_segment[5]) >> 15);

	tp->stc_time &= STC_MAX;
}

unsigned long long te_interpolate_points(te_pcr_time_point_t *left,
		te_pcr_time_point_t *right, unsigned long long stc_mid)
{
	unsigned long long sys_time = 0;

	/*
	 *                             (right->sys_time - left->sys_time) * (stc_mid - left->stc_time)
	 * sys_time = left->sys_time + ---------------------------------------------------------------
	 *                                         (right->stc_time - left->stc_time)
	 */

	unsigned long long rangeSys = (right->sys_time - left->sys_time);
	unsigned int rangeSTC = (unsigned int)(right->stc_time -
			left->stc_time);
	unsigned int deltaSTC = (unsigned int)(stc_mid - left->stc_time);

	if (rangeSTC != 0) {
		sys_time = rangeSys * deltaSTC;
		do_div(sys_time, rangeSTC);
	}
	sys_time += left->sys_time;

	return sys_time;
}

#define WINDOW_SIZE (1000000)

int te_convert_arrival_to_systime(struct te_out_filter *output,
		te_pcr_data_t *raw_pcr_data, unsigned long long *pcr,
		unsigned long long *sys_time, bool move_window)
{
	struct te_time_points *time;
	te_pcr_time_point_t mid_point;	/* The point we want to convert */
	te_pcr_time_point_t *mid = &mid_point;

	/*
	 * First get our private data for our points...
	 */
	switch (output->obj.type) {
	case TE_OBJ_TS_INDEX_FILTER:
		time = te_ts_index_get_times(&output->obj);
		break;
	case TE_OBJ_PCR_FILTER:
		time = te_pcr_get_times(&output->obj);
		break;
	default:
		pr_warning("object does not contain time info\n");
		return -EINVAL;
	}

	/*
	 * Process our 'mid' point of which we want to convert
	 */
	if (output->obj.type == TE_OBJ_PCR_FILTER) {
		te_convert_raw_data_to_point(raw_pcr_data, mid);
	} else {
		/* TS_INDEX_FILTER_OBJECT seems to be passing its values in
		 * through the pointers */
		mid->pcr_time = *pcr;
		mid->stc_time = *sys_time;
	}

	/* If STC has wrapped, adjust the mid time point to
	 * compensate */
	if ((time->left.stc_time & STC_MSB) && !(mid->stc_time & STC_MSB)) {
		if (!(mid->stc_time & STC_WRAP_ADJUST))
			mid->stc_time |= STC_WRAP_ADJUST;
	}

	if (move_window == true) {
		/* Get Right point from *current* clock times.
		 * te_get_device_time can return error, so we pass this along
		 * if it does.  */
		if (0 !=
		    te_get_time_point(output->obj.parent, &time->right))
			return -EINVAL;

		/* If STC has wrapped, adjust the right time point to
		 * compensate */
		if ((time->left.stc_time & STC_MSB) &&
				!(time->right.stc_time & STC_MSB)) {
			if (!(time->right.stc_time & STC_WRAP_ADJUST))
				time->right.stc_time |= STC_WRAP_ADJUST;
		}

		/*
		 *  if (candidate < mid)
		 *   {
		 *     left = candidate
		 *     candidate = 0;
		 *   }
		 */
		if (time->left_candidate.stc_time != 0
		    && time->left_candidate.stc_time < mid->stc_time) {
			time->left = time->left_candidate;
			memset ( (void *) &time->left_candidate, 0, sizeof(time->left_candidate));
			/* If the new left time point has been adjusted for
			 * STC wrap, we know that all time points have been
			 * adjusted, so we can safely undo the adjustment */
			if (time->left.stc_time & STC_WRAP_ADJUST) {
				time->left.stc_time &= ~(STC_WRAP_ADJUST);
				time->right.stc_time &= ~(STC_WRAP_ADJUST);
				mid->stc_time &= ~(STC_WRAP_ADJUST);
				pr_info("%s: All time points have wrapped\n",
						output->obj.name);
			}
		}

		/*
		 * If (candidate == 0)
		 *   if (right > left+window)
		 *     candidate = right
		 */
		if ((time->left_candidate.sys_time == 0)
		    && (time->right.sys_time > (time->left.sys_time + WINDOW_SIZE))) {
			time->left_candidate = time->right;
		}
	}

	/*
	 * Perform some pre-calculation sanity checks
	 * This check can fail if our clocks are stopped
	 */
	if ((time->right.stc_time - time->left.stc_time == 0) ||
			(time->right.sys_time - time->left.sys_time == 0))
		return -EINVAL;

	/* Perform linear interpolation on the ktime at the STC stamped point */
	mid->sys_time = te_interpolate_points(&time->left, &time->right, mid->stc_time);

	/* If the computed ktime is outside the window ktime then this is not
	 * interpolation so ignore */
	if ((time->left.sys_time > mid->sys_time)
	    || (mid->sys_time > time->right.sys_time))
		return -EINVAL;

	/* Set our return values */
	*pcr = mid->pcr_time;
	*sys_time = mid->sys_time;

	return 0;
}

int te_obj_init_time(struct te_obj *filter)
{
	int err;
	struct te_time_points *time;

	switch (filter->type) {
	case TE_OBJ_TS_INDEX_FILTER:
		time = te_ts_index_get_times(filter);
		break;
	case TE_OBJ_PCR_FILTER:
		time = te_pcr_get_times(filter);
		break;
	default:
		pr_warning("object does not contain time info\n");
		return -EINVAL;
	}

	err = te_get_time_point(filter->parent, &time->left);
	if (err) {
		pr_err("Error generating initial time points for %s (%p)\n",
				filter->name, filter);
	}

	return err;
}

