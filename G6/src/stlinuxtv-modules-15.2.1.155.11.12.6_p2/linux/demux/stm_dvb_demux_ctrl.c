/************************************************************************
Copyright (C) 2015 STMicroelectronics. All Rights Reserved.

This file is part of the STLinuxTV Library.

STLinuxTV is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License version 2 as published by the
Free Software Foundation.

STLinuxTV is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with player2; see the file COPYING.  If not, write to the Free Software
Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

The STLinuxTV Library may alternatively be licensed under a proprietary
license from ST.
 * demuxer controls implementation
************************************************************************/
#include <linux/types.h>
#include <linux/errno.h>

#include "demux_filter.h"
#include "demux_common.h"
#include "linux/dvb/stm_dmx.h"
#define stv_err pr_err

/**
 * demux_validate_ctrl() - check for supported controls
 */
static int demux_validate_ctrl(__u32 id)
{
	int ret = 0;

	switch (id) {
	case DMX_CTRL_OUTPUT_BUFFER_STATUS:
		break;

	default:
		ret = -EINVAL;
	}

	return ret;
}

/**
 * demux_set_ctrl() - set control on pes filter
 */
static int demux_get_ctrl(struct stm_dvb_demux_s *stm_demux,
		struct dmxdev_filter *dmxdevfilter, struct dmx_ctrl *ctrl)
{
	int ret = 0;
	struct stm_dvb_filter_chain_s *chain = NULL;
	stm_te_output_filter_stats_t output_filter_status;

	/*
	 * Initial processing for control
	 */
	switch (ctrl->id) {
	case DMX_CTRL_OUTPUT_BUFFER_STATUS:

		/*
		 * Check if we have filter created to apply a control
		 */
		chain = match_chain_from_filter(stm_demux, dmxdevfilter);
		if (!chain) {
			stv_err("no filter found for this instance\n");
			ret = -EINVAL;
		}

		break;
	}

	if (ret) {
		stv_err("failed to process the control\n");
		goto get_ctrl_failed;
	}

	/*
	 * Get control now
	 */
	switch (ctrl->id) {
	case DMX_CTRL_OUTPUT_BUFFER_STATUS:
		ret = stm_te_filter_get_compound_control(chain->output_filter->handle,
				STM_TE_OUTPUT_FILTER_CONTROL_STATUS,
				&output_filter_status);
		ctrl->value = output_filter_status.bytes_in_buffer;

		if (ret)
			stv_err("get control value failed %d\n", ret);
		break;

	}

get_ctrl_failed:
	return ret;
}

/**
 * stm_dvb_demux_get_control() - get control
 */
int stm_dvb_demux_get_control(struct stm_dvb_demux_s *stm_demux,
			struct dmxdev_filter *dmxdevfilter, struct dmx_ctrl *ctrl)
{
	int ret = 0;

	/*
	 * Check if the control is exposed to user-space
	 */
	ret = demux_validate_ctrl(ctrl->id);
	if (ret) {
		stv_err("invalid control received\n");
		goto set_control_done;
	}

	/*
	 * Is the control for valid pes_type?
	 */
	if (ctrl->pes_type != dmxdevfilter->params.pes.pes_type) {
		pr_err("%s(): pes type does not match with filter pes type\n", __func__);
		ret = -EINVAL;
		goto set_control_done;
	}

	/*
	 * Get the control on demux
	 */
	ret = demux_get_ctrl(stm_demux, dmxdevfilter, ctrl);
	if (ret)
		stv_err("failed to get control on demux\n");

set_control_done:
	return ret;
}
