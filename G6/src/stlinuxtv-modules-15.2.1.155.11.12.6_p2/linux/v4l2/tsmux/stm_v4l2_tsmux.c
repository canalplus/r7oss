/************************************************************************
Copyright (C) 2007, 2009, 2010 STMicroelectronics. All Rights Reserved.

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
 * Implementation of linux v4l2 tsmux device
************************************************************************/
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/printk.h>
#include <linux/moduleparam.h>
#include <linux/list.h>
#include <linux/time.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/videodev2.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/scatterlist.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-subdev.h>
#include <media/v4l2-device.h>
#include <media/videobuf2-core.h>
#include <media/videobuf2-memops.h>
#include <media/videobuf2-vmalloc.h>
#include <linux/media.h>
#include "stm_memsink.h"
#include "stm_te_if_tsmux.h"
#include "stm_te.h"
#include "stm_se.h"
#include "stmedia.h"
#include "linux/stm/stmedia_export.h"
#include "stm_v4l2_tsmux.h"
#include "linux/dvb/dvb_v4l2_export.h"
#include "stm_v4l2_encode.h"

/******************************
 * DEFINES
 ******************************/

/* Wake up at about 30 fps */
#define WAKE_NUMERATOR 30
#define WAKE_DENOMINATOR 1001

#define TSMUX_MODULE_NAME "tsmux"

/* The last pad is used as the CAPTURE PAD. This ID will be used to identify the CAPTURE PAD*/
#define TSMUX_CAPTURE_PAD_ID TSMUX_NUM_OUTPUTS

/* Number of video buffers in each output queue */
/* NOTE MUST BE LESS THAN VIDEO_MAX_FRAME=32 */
#define TSMUX_OUTPUT_NUM_BUFFERS VIDEO_MAX_FRAME

#define TSMUX_MAX_PROGRAM_NAME_SIZE 16
#define TSMUX_MAX_DESCRIPTOR_SIZE 16

static unsigned tsmux_dev_nums = 0;

#ifdef TSMUX_DEBUG
#define dprintk(ctx, level, fmt, arg...) \
	v4l2_dbg(level, 3, ctx, fmt, ## arg)
#else
#define dprintk(ctx, level, fmt, arg...) \
	v4l2_dbg(level, 0, ctx, fmt, ## arg)
#endif

/******************************
 * GLOBAL VARIABLES
 ******************************/

static unsigned int thread_stl_tsmux[2] = { SCHED_NORMAL, 0 };
module_param_array_named(thread_STL_Tsmux,thread_stl_tsmux, int, NULL, 0644);
MODULE_PARM_DESC(thread_STL_Tsmux, "STL-Tsmux thread:s(Mode),p(Priority)");

struct tsmux_fmt {
	char *name;
	u32 fourcc;
	u32 types;
	u32 flags;
};

static struct tsmux_fmt tsmux_formats[] = {
	{
	 .name = "TS",
	 .fourcc = V4L2_DVB_FMT_TS,
	 .types = V4L2_BUF_TYPE_DVB_CAPTURE,
	 .flags = V4L2_FMT_FLAG_COMPRESSED,
	 },
	{
	 .name = "DLNA",
	 .fourcc = V4L2_DVB_FMT_DLNA_TS,
	 .types = V4L2_BUF_TYPE_DVB_CAPTURE,
	 .flags = V4L2_FMT_FLAG_COMPRESSED,
	 },
	{
	 .name = "ES",
	 .fourcc = V4L2_DVB_FMT_ES,
	 .types = V4L2_BUF_TYPE_DVB_OUTPUT,
	 .flags = V4L2_FMT_FLAG_COMPRESSED,
	 },
	{
	 .name = "PES",
	 .fourcc = V4L2_DVB_FMT_PES,
	 .types = V4L2_BUF_TYPE_DVB_OUTPUT,
	 .flags = V4L2_FMT_FLAG_COMPRESSED,
	 },
	{
	 .name = "DATA",
	 .fourcc = V4L2_DVB_FMT_DATA,
	 .types = V4L2_BUF_TYPE_DVB_OUTPUT,
	 .flags = V4L2_FMT_FLAG_COMPRESSED,
	 },
};

static struct v4l2_queryctrl stm_v4l2_tsmux_ctrls[] = {
	{
	 .id = V4L2_CID_STM_TSMUX_PCR_PERIOD,
	 .type = V4L2_CTRL_TYPE_INTEGER,
	 .name = "PCR Period",
	 .minimum = 900,
	 .maximum = 9000,
	 .step = 1,
	 .default_value = 9000,
	 .flags = 0,
	 },
	{
	 .id = V4L2_CID_STM_TSMUX_GEN_PCR_STREAM,
	 .type = V4L2_CTRL_TYPE_BOOLEAN,
	 .name = "Generate PCR Stream",
	 .minimum = 0,
	 .maximum = 1,
	 .step = 1,
	 .default_value = 0,
	 .flags = 0,
	 },
	{
	 .id = V4L2_CID_STM_TSMUX_PCR_PID,
	 .type = V4L2_CTRL_TYPE_INTEGER,
	 .name = "PCR Pid",
	 .minimum = 0,
	 .maximum = 0x1ffe,
	 .step = 1,
	 .default_value = 0x1ffe,
	 .flags = 0,
	 },
	{
	 .id = V4L2_CID_STM_TSMUX_TABLE_GEN,
	 .type = V4L2_CTRL_TYPE_BITMASK,
	 .name = "Table Generation Flags",
	 .minimum = V4L2_STM_TSMUX_TABLE_GEN_NONE,
	 .maximum = (V4L2_STM_TSMUX_TABLE_GEN_SDT |
		     V4L2_STM_TSMUX_TABLE_GEN_PAT_PMT),
	 .step = 0,
	 .default_value = (V4L2_STM_TSMUX_TABLE_GEN_SDT |
			   V4L2_STM_TSMUX_TABLE_GEN_PAT_PMT),
	 .flags = 0,
	 },
	{
	 .id = V4L2_CID_STM_TSMUX_TABLE_PERIOD,
	 .type = V4L2_CTRL_TYPE_INTEGER,
	 .name = "Table Period",
	 .minimum = 2250,
	 .maximum = 9000,
	 .step = 1,
	 .default_value = 9000,
	 .flags = 0,
	 },
	{
	 .id = V4L2_CID_STM_TSMUX_TS_ID,
	 .type = V4L2_CTRL_TYPE_INTEGER,
	 .name = "TS ID",
	 .minimum = 0,
	 .maximum = 0xffff,
	 .step = 1,
	 .default_value = 0,
	 .flags = 0,
	 },
	{
	 .id = V4L2_CID_STM_TSMUX_BITRATE_IS_CONSTANT,
	 .type = V4L2_CTRL_TYPE_BOOLEAN,
	 .name = "TS Fixed Bitrate",
	 .minimum = 0,
	 .maximum = 1,
	 .step = 1,
	 .default_value = 0,
	 .flags = 0,
	 },
	{
	 .id = V4L2_CID_STM_TSMUX_BITRATE,
	 .type = V4L2_CTRL_TYPE_INTEGER,
	 .name = "TS Bitrate",
	 .minimum = 100 * 1024,
	 .maximum = 60 * 1024 * 1024,
	 .step = 1,
	 .default_value = 48 * 1024 * 1024,
	 .flags = 0,
	 },
	{
	 .id = V4L2_CID_STM_TSMUX_PROGRAM_NUMBER,
	 .type = V4L2_CTRL_TYPE_INTEGER,
	 .name = "Program Number",
	 .minimum = 0,
	 .maximum = 0xffff,
	 .step = 1,
	 .default_value = 1,
	 .flags = 0,
	 },
	{
	 .id = V4L2_CID_STM_TSMUX_PMT_PID,
	 .type = V4L2_CTRL_TYPE_INTEGER,
	 .name = "PMT PID",
	 .minimum = 1,
	 .maximum = 0x1ffd,
	 .step = 1,
	 .default_value = 0x31,
	 .flags = 0,
	 },
	{
	 .id = V4L2_CID_STM_TSMUX_SDT_PROV_NAME,
	 .type = V4L2_CTRL_TYPE_STRING,
	 .name = "SDT Provider Name",
	 .minimum = 0,
	 .maximum = TSMUX_MAX_PROGRAM_NAME_SIZE,
	 .step = 1,
	 .default_value = 0,
	 .flags = 0,
	 },
	{
	 .id = V4L2_CID_STM_TSMUX_SDT_SERV_NAME,
	 .type = V4L2_CTRL_TYPE_STRING,
	 .name = "SDT Service Name",
	 .minimum = 0,
	 .maximum = TSMUX_MAX_PROGRAM_NAME_SIZE,
	 .step = 1,
	 .default_value = 0,
	 .flags = 0,
	 },
	{
	 .id = V4L2_CID_STM_TSMUX_PMT_DESCRIPTOR,
	 .type = V4L2_CTRL_TYPE_STRING,
	 .name = "PMT Descriptor",
	 .minimum = 0,
	 .maximum = TSMUX_MAX_DESCRIPTOR_SIZE,
	 .step = 1,
	 .default_value = 0,
	 .flags = 0,
	 },
	{
	 .id = V4L2_CID_STM_TSMUX_INCLUDE_PCR,
	 .type = V4L2_CTRL_TYPE_BOOLEAN,
	 .name = "Include PCR In Stream",
	 .minimum = 0,
	 .maximum = 0,
	 .step = 1,
	 .default_value = 0,
	 .flags = 0,
	 },
	{
	 .id = V4L2_CID_STM_TSMUX_STREAM_PID,
	 .type = V4L2_CTRL_TYPE_INTEGER,
	 .name = "Stream PID",
	 .minimum = 0,
	 .maximum = 0x1ffe,
	 .step = 1,
	 .default_value = 0x100,
	 .flags = 0,
	 },
	{
	 .id = V4L2_CID_STM_TSMUX_PES_STREAM_ID,
	 .type = V4L2_CTRL_TYPE_INTEGER,
	 .name = "PES Stream_id",
	 .minimum = 0,
	 .maximum = 0xff,
	 .step = 1,
	 .default_value = 0x0,
	 .flags = 0,
	 },
	{
	 .id = V4L2_CID_STM_TSMUX_STREAM_DESCRIPTOR,
	 .type = V4L2_CTRL_TYPE_STRING,
	 .name = "Stream Descriptor",
	 .minimum = 0,
	 .maximum = TSMUX_MAX_DESCRIPTOR_SIZE,
	 .step = 1,
	 .default_value = 0,
	 .flags = 0,
	 },
	{
	 .id = V4L2_CID_STM_TSMUX_BIT_BUFFER_SIZE,
	 .type = V4L2_CTRL_TYPE_INTEGER,
	 .name = "Decoder Bit Buffer Size",
	 .minimum = 0,
	 .maximum = 64 * 1024 * 1024,
	 .step = 1,
	 .default_value = 4 * 1024 * 1024,
	 .flags = 0,
	 },
	{
	 .id = V4L2_CID_STM_TSMUX_INDEXING_MASK,
	 .type = V4L2_CTRL_TYPE_BITMASK,
	 .name = "Indexing Mask",
	 .minimum = V4L2_STM_TSMUX_INDEX_NONE,
	 .maximum = V4L2_STM_TSMUX_INDEX_PUSI | V4L2_STM_TSMUX_INDEX_PTS |
	 V4L2_STM_TSMUX_INDEX_I_FRAME | V4L2_STM_TSMUX_INDEX_B_FRAME |
	 V4L2_STM_TSMUX_INDEX_P_FRAME | V4L2_STM_TSMUX_INDEX_DIT |
	 V4L2_STM_TSMUX_INDEX_RAP,
	 .step = 0,
	 .default_value = V4L2_STM_TSMUX_INDEX_NONE,
	 .flags = 0,
	 },
	{
	 .id = V4L2_CID_STM_TSMUX_SECTION_PERIOD,
	 .type = V4L2_CTRL_TYPE_INTEGER,
	 .name = "Section Period",
	 .minimum = 0,
	 .maximum = 2000,
	 .step = 1,
	 .default_value = 0,
	 .flags = 0,
	 },
	{
	 .id = V4L2_CID_STM_TSMUX_INDEXING,
	 .type = V4L2_CTRL_TYPE_BOOLEAN,
	 .name = "Indexing Enable or Disable",
	 .minimum = 0,
	 .maximum = 1,
	 .step = 1,
	 .default_value = 0,
	 .flags = 0,
	 },
};

static int v4l2_to_tsmux_map[][3] = {
	{V4L2_CID_STM_TSMUX_PCR_PERIOD, TSMUX_INPUT,
	 STM_TE_TSMUX_CNTRL_PCR_PERIOD},
	{V4L2_CID_STM_TSMUX_GEN_PCR_STREAM, TSMUX_INPUT,
	 STM_TE_TSMUX_CNTRL_GEN_PCR_STREAM},
	{V4L2_CID_STM_TSMUX_PCR_PID, TSMUX_INPUT, STM_TE_TSMUX_CNTRL_PCR_PID},
	{V4L2_CID_STM_TSMUX_TABLE_GEN, TSMUX_INPUT,
	 STM_TE_TSMUX_CNTRL_TABLE_GEN},
	{V4L2_CID_STM_TSMUX_TABLE_PERIOD, TSMUX_INPUT,
	 STM_TE_TSMUX_CNTRL_TABLE_PERIOD},
	{V4L2_CID_STM_TSMUX_TS_ID, TSMUX_INPUT, STM_TE_TSMUX_CNTRL_TS_ID},
	{V4L2_CID_STM_TSMUX_BITRATE_IS_CONSTANT, TSMUX_INPUT,
	 STM_TE_TSMUX_CNTRL_BIT_RATE_IS_CONSTANT},
	{V4L2_CID_STM_TSMUX_BITRATE, TSMUX_INPUT, STM_TE_TSMUX_CNTRL_BIT_RATE},
	{V4L2_CID_STM_TSMUX_PROGRAM_NUMBER, TSMUX_INPUT,
	 STM_TE_TSMUX_CNTRL_PROGRAM_NUMBER},
	{V4L2_CID_STM_TSMUX_PMT_PID, TSMUX_INPUT, STM_TE_TSMUX_CNTRL_PMT_PID},
	{V4L2_CID_STM_TSMUX_SDT_PROV_NAME, TSMUX_INPUT,
	 STM_TE_TSMUX_CNTRL_SDT_PROV_NAME},
	{V4L2_CID_STM_TSMUX_SDT_SERV_NAME, TSMUX_INPUT,
	 STM_TE_TSMUX_CNTRL_SDT_SERV_NAME},
	{V4L2_CID_STM_TSMUX_PMT_DESCRIPTOR, TSMUX_INPUT,
	 STM_TE_TSMUX_CNTRL_PMT_DESCRIPTOR},
	{V4L2_CID_STM_TSMUX_INDEXING_MASK, TSMUX_INPUT,
	 STM_TE_TSG_INDEX_FILTER_CONTROL_INDEX},
	{V4L2_CID_STM_TSMUX_INCLUDE_PCR, TSMUX_OUTPUT,
	 STM_TE_TSG_FILTER_CONTROL_INCLUDE_PCR},
	{V4L2_CID_STM_TSMUX_STREAM_PID, TSMUX_OUTPUT,
	 STM_TE_TSG_FILTER_CONTROL_STREAM_PID},
	{V4L2_CID_STM_TSMUX_PES_STREAM_ID, TSMUX_OUTPUT,
	 STM_TE_TSG_FILTER_CONTROL_PES_STREAM_ID},
	{V4L2_CID_STM_TSMUX_STREAM_DESCRIPTOR, TSMUX_OUTPUT,
	 STM_TE_TSG_FILTER_CONTROL_STREAM_DESCRIPTOR},
	{V4L2_CID_STM_TSMUX_BIT_BUFFER_SIZE, TSMUX_OUTPUT,
	 STM_TE_TSG_FILTER_CONTROL_BIT_BUFFER_SIZE},
	{V4L2_CID_STM_TSMUX_IGNORE_AUTO_PAUSE, TSMUX_OUTPUT,
	 STM_TE_TSG_FILTER_CONTROL_IGNORE_AUTO_PAUSE},
};

/**************/
/* Prototypes */
/**************/
static int tsmux_thread(void *data);
static int attach_to_tsg_index_filter(struct tsmux_context *out_ctx);
static int detach_from_tsg_index_filter(struct tsmux_context *out_ctx);
static int create_tsg_index_filter(struct tsmux_context *octx);
/*********************************/
/* Control Framework for v4l2-tsmux */
/*********************************/

static int v4l2_to_tsmux_ctrl(unsigned int ctrl_id, enum tsmux_type type,
			      unsigned int *te_ctrl_id)
{
	int ret = 0;
	int size = 0;
	int i;

	if ((ctrl_id == V4L2_CID_STM_TSMUX_SECTION_PERIOD) ||
	    (ctrl_id == V4L2_CID_STM_TSMUX_INDEXING))
		return 0;

	size = ARRAY_SIZE(v4l2_to_tsmux_map);

	for (i = 0; i < size; i++) {
		if ((ctrl_id == v4l2_to_tsmux_map[i][0]) &&
		    (type == v4l2_to_tsmux_map[i][1])) {
			*te_ctrl_id = v4l2_to_tsmux_map[i][2];
			break;
		}
	}

	if (i == size)
		ret = -EINVAL;

	return ret;
}

static int tsmux_s_ctrl(struct v4l2_ctrl *ctrl_p)
{
	int ret = 0;
	struct tsmux_context *ctx =
	    container_of(ctrl_p->handler, struct tsmux_context,
			 ctx_ctrl_handler);
	unsigned int ctrl_id = ctrl_p->id;
	int ctrl_value = ctrl_p->val;
	int value;
	unsigned int te_ctrl_id = 0;

	ret = v4l2_to_tsmux_ctrl(ctrl_id, ctx->type, &te_ctrl_id);
	if (ret) {
		pr_err("Unknown ctrl_id = 0x%x\n", ctrl_id);
		return ret;
	}

	/* TODO Before actually calling the TE functions we should
	 * go through the whole set of controls and check for validity
	 */
	/* TODO Should also check the status of the tsmux/tsg TE object */
	switch (ctrl_id) {
	case V4L2_CID_STM_TSMUX_PCR_PERIOD:
	case V4L2_CID_STM_TSMUX_GEN_PCR_STREAM:
	case V4L2_CID_STM_TSMUX_PCR_PID:
	case V4L2_CID_STM_TSMUX_TABLE_PERIOD:
	case V4L2_CID_STM_TSMUX_TS_ID:
	case V4L2_CID_STM_TSMUX_BITRATE_IS_CONSTANT:
	case V4L2_CID_STM_TSMUX_BITRATE:
	case V4L2_CID_STM_TSMUX_PROGRAM_NUMBER:
	case V4L2_CID_STM_TSMUX_PMT_PID:
		ret = stm_te_tsmux_set_control(ctx->tsmux_object,
					       te_ctrl_id, ctrl_value);
		break;
	case V4L2_CID_STM_TSMUX_SDT_PROV_NAME:
	case V4L2_CID_STM_TSMUX_SDT_SERV_NAME:
		ret =
		    stm_te_tsmux_set_compound_control(ctx->tsmux_object,
						      te_ctrl_id,
						      ctrl_p->string);
		break;
	case V4L2_CID_STM_TSMUX_PMT_DESCRIPTOR:
	{
		stm_te_tsmux_descriptor_t te_desc;
		int size = 0;

		size = strlen(ctrl_p->string);
		if (!size ||
			size > TSMUX_MAX_DESCRIPTOR_SIZE) {
			ret = -EINVAL;
			break;
		}
		te_desc.size = size;
		memcpy(te_desc.descriptor, ctrl_p->string, size);
		ret =
		    stm_te_tsmux_set_compound_control(ctx->tsmux_object,
						      te_ctrl_id,
						      &te_desc);
		break;
	}
	case V4L2_CID_STM_TSMUX_TABLE_GEN:

		value = STM_TE_TSMUX_CNTRL_TABLE_GEN_NONE;
		if (ctrl_value & V4L2_STM_TSMUX_TABLE_GEN_PAT_PMT)
			value |= STM_TE_TSMUX_CNTRL_TABLE_GEN_PAT_PMT;
		if (ctrl_value & V4L2_STM_TSMUX_TABLE_GEN_SDT)
			value |= STM_TE_TSMUX_CNTRL_TABLE_GEN_SDT;
		ret = stm_te_tsmux_set_control(ctx->tsmux_object,
					       te_ctrl_id, value);
		break;
		/* TSG Stream Controls */
	case V4L2_CID_STM_TSMUX_INCLUDE_PCR:
	case V4L2_CID_STM_TSMUX_STREAM_PID:
	case V4L2_CID_STM_TSMUX_PES_STREAM_ID:
	case V4L2_CID_STM_TSMUX_BIT_BUFFER_SIZE:
	case V4L2_CID_STM_TSMUX_IGNORE_AUTO_PAUSE:
		ret = stm_te_filter_set_control(ctx->tsg_object,
						te_ctrl_id, ctrl_value);
		break;
	case V4L2_CID_STM_TSMUX_STREAM_DESCRIPTOR:
	{
		stm_te_tsmux_descriptor_t te_desc;
		int size = 0;

		size = strlen(ctrl_p->string);
		if (!size ||
			size > TSMUX_MAX_DESCRIPTOR_SIZE) {
			ret = -EINVAL;
			break;
		}
		te_desc.size = size;
		memcpy(te_desc.descriptor, ctrl_p->string, size);
		ret =
		    stm_te_filter_set_compound_control(ctx->tsg_object,
						       te_ctrl_id,
						       &te_desc);
		break;
	}
	case V4L2_CID_STM_TSMUX_SECTION_PERIOD:
		if (ctx->type != TSMUX_OUTPUT) {
			ret = -EINVAL;
			break;
		}
		ctx->section_table_period = ctrl_value;
		break;
	case V4L2_CID_STM_TSMUX_INDEXING_MASK:

		value = 0;
		if (ctrl_value & V4L2_STM_TSMUX_INDEX_PUSI)
			value |= STM_TE_TSG_INDEX_PUSI;
		if (ctrl_value & V4L2_STM_TSMUX_INDEX_PTS)
			value |= STM_TE_TSG_INDEX_PTS;
		if (ctrl_value & V4L2_STM_TSMUX_INDEX_I_FRAME)
			value |= STM_TE_TSG_INDEX_I_FRAME;
		if (ctrl_value & V4L2_STM_TSMUX_INDEX_B_FRAME)
			value |= STM_TE_TSG_INDEX_B_FRAME;
		if (ctrl_value & V4L2_STM_TSMUX_INDEX_P_FRAME)
			value |= STM_TE_TSG_INDEX_P_FRAME;
		if (ctrl_value & V4L2_STM_TSMUX_INDEX_DIT)
			value |= STM_TE_TSG_INDEX_DIT;
		if (ctrl_value & V4L2_STM_TSMUX_INDEX_RAP)
			value |= STM_TE_TSG_INDEX_RAP;

		ret = stm_te_filter_set_control(ctx->tsg_index_object,
						te_ctrl_id, value);

		break;
	case V4L2_CID_STM_TSMUX_INDEXING:
		if (ctx->type != TSMUX_OUTPUT) {
			ret = -EINVAL;
			break;
		}

		if (ctrl_value) {
			if (!ctx->dev->tsmux_in_ctx.tsg_index_object) {
				ret = create_tsg_index_filter(ctx);
				if (ret) {
					pr_err
					    ("Failure in create_tsg_index_filter() \n");
					return ret;
				}
			}

			ret = attach_to_tsg_index_filter(ctx);
			if (ret) {
				pr_err("Failed to attach to index flter \n");
				return ret;
			}

			ctx->connected_to_index_filter = 1;
		} else {
			ret = detach_from_tsg_index_filter(ctx);
			if (ret) {
				pr_err("Failed to detach from index flter \n");
				return ret;
			}

			ctx->connected_to_index_filter = 0;
		}
		break;

	default:
		pr_warn("Unrecognised tsmux control value 0x%x\n", ctrl_id);
		ret = -EINVAL;
	}

	return ret;
}

static int tsmux_try_ctrl(struct v4l2_ctrl *ctrl)
{
	int ret = 0;
	union {
		s32 val;
		s64 val64;
		char *string;
	} temp_val;

	/* since the values in v4l2 layer are not in sync with those in the kpi, we update
	   the current values by querying them here. This is required for the ctrl framework to
	   decide to invoke s_ctrl */
	if (ctrl->flags & V4L2_CTRL_FLAG_VOLATILE) {

		switch (ctrl->type) {
		case V4L2_CTRL_TYPE_STRING:
			temp_val.string = kzalloc(ctrl->maximum, GFP_KERNEL);
			/* strings are always 0-terminated */
			strcpy(temp_val.string, ctrl->string);
			break;
		case V4L2_CTRL_TYPE_INTEGER64:
			temp_val.val64 = ctrl->val64;
			break;
		default:
			temp_val.val = ctrl->val;
			break;
		}

		ret = ctrl->ops->g_volatile_ctrl(ctrl);
		switch (ctrl->type) {
		case V4L2_CTRL_TYPE_STRING:
			/* strings are always 0-terminated */
			strcpy(ctrl->cur.string, ctrl->string);
			strcpy(ctrl->string, temp_val.string);
			kfree(temp_val.string);
			break;
		case V4L2_CTRL_TYPE_INTEGER64:
			ctrl->cur.val64 = ctrl->val64;
			ctrl->val64 = temp_val.val64;
			break;
		default:
			ctrl->cur.val = ctrl->val;
			ctrl->val = temp_val.val;
			break;
		}
	}
	return ret;
}

static int tsmux_g_ctrl(struct v4l2_ctrl *ctrl_p)
{
	int ret = 0;
	struct tsmux_context *ctx =
	    container_of(ctrl_p->handler, struct tsmux_context,
			 ctx_ctrl_handler);
	unsigned int ctrl_id = ctrl_p->id;
	int te_ctrl_value;
	unsigned int te_ctrl_id = 0;

	ret = v4l2_to_tsmux_ctrl(ctrl_id, ctx->type, &te_ctrl_id);
	if (ret) {
		pr_err("Unknown ctrl_id = 0x%x\n", ctrl_id);
		return ret;
	}

	/* TODO Before actually calling the TE functions we should
	 * go through the whole set of controls and check for validity
	 */
	/* TODO Should also check the status of the tsmux/tsg TE object */
	switch (ctrl_id) {
	case V4L2_CID_STM_TSMUX_PCR_PERIOD:
	case V4L2_CID_STM_TSMUX_GEN_PCR_STREAM:
	case V4L2_CID_STM_TSMUX_PCR_PID:
	case V4L2_CID_STM_TSMUX_TABLE_PERIOD:
	case V4L2_CID_STM_TSMUX_TS_ID:
	case V4L2_CID_STM_TSMUX_BITRATE_IS_CONSTANT:
	case V4L2_CID_STM_TSMUX_BITRATE:
	case V4L2_CID_STM_TSMUX_PROGRAM_NUMBER:
	case V4L2_CID_STM_TSMUX_PMT_PID:
		ret = stm_te_tsmux_get_control(ctx->tsmux_object,
					       te_ctrl_id, &ctrl_p->val);
		break;

	case V4L2_CID_STM_TSMUX_TABLE_GEN:

		ctrl_p->val = V4L2_STM_TSMUX_TABLE_GEN_NONE;
		ret = stm_te_tsmux_get_control(ctx->tsmux_object,
					       te_ctrl_id, &te_ctrl_value);

		if (te_ctrl_value & STM_TE_TSMUX_CNTRL_TABLE_GEN_PAT_PMT)
			ctrl_p->val |= V4L2_STM_TSMUX_TABLE_GEN_PAT_PMT;
		if (te_ctrl_value & STM_TE_TSMUX_CNTRL_TABLE_GEN_SDT)
			ctrl_p->val |= V4L2_STM_TSMUX_TABLE_GEN_SDT;
		break;

	case V4L2_CID_STM_TSMUX_SDT_PROV_NAME:
	case V4L2_CID_STM_TSMUX_SDT_SERV_NAME:
	case V4L2_CID_STM_TSMUX_PMT_DESCRIPTOR:
		ret = stm_te_tsmux_get_compound_control(ctx->tsmux_object,
							te_ctrl_id,
							ctrl_p->string);
		break;

		/* TSG Stream Controls */
	case V4L2_CID_STM_TSMUX_INCLUDE_PCR:
	case V4L2_CID_STM_TSMUX_STREAM_PID:
	case V4L2_CID_STM_TSMUX_PES_STREAM_ID:
	case V4L2_CID_STM_TSMUX_BIT_BUFFER_SIZE:
	case V4L2_CID_STM_TSMUX_IGNORE_AUTO_PAUSE:
		ret = stm_te_filter_get_control(ctx->tsg_object,
						te_ctrl_id, &ctrl_p->val);
		break;

	case V4L2_CID_STM_TSMUX_STREAM_DESCRIPTOR:
		ret =
		    stm_te_filter_get_compound_control(ctx->tsg_object,
						       te_ctrl_id,
						       ctrl_p->string);
		break;

	case V4L2_CID_STM_TSMUX_SECTION_PERIOD:
		if (ctx->type != TSMUX_OUTPUT) {
			ret = -EINVAL;
			break;
		}
		ctrl_p->val = ctx->section_table_period;
		break;

	case V4L2_CID_STM_TSMUX_INDEXING_MASK:

		te_ctrl_value = 0;
		ret = stm_te_filter_get_control(ctx->tsg_index_object,
						te_ctrl_id, &te_ctrl_value);
		if (te_ctrl_value & STM_TE_TSG_INDEX_PUSI)
			ctrl_p->val |= V4L2_STM_TSMUX_INDEX_PUSI;
		if (te_ctrl_value & STM_TE_TSG_INDEX_PTS)
			ctrl_p->val |= V4L2_STM_TSMUX_INDEX_PTS;
		if (te_ctrl_value & STM_TE_TSG_INDEX_I_FRAME)
			ctrl_p->val |= V4L2_STM_TSMUX_INDEX_I_FRAME;
		if (te_ctrl_value & STM_TE_TSG_INDEX_B_FRAME)
			ctrl_p->val |= V4L2_STM_TSMUX_INDEX_B_FRAME;
		if (te_ctrl_value & STM_TE_TSG_INDEX_P_FRAME)
			ctrl_p->val |= V4L2_STM_TSMUX_INDEX_P_FRAME;
		if (te_ctrl_value & STM_TE_TSG_INDEX_DIT)
			ctrl_p->val |= V4L2_STM_TSMUX_INDEX_DIT;
		if (te_ctrl_value & STM_TE_TSG_INDEX_RAP)
			ctrl_p->val |= V4L2_STM_TSMUX_INDEX_RAP;
		break;

	case V4L2_CID_STM_TSMUX_INDEXING:
		ctrl_p->val = ctx->connected_to_index_filter;
		break;

	default:
		pr_warn("Unrecognised tsmux control value 0x%x\n", ctrl_id);
		ret = -EINVAL;
	}

	return ret;
}

static const struct v4l2_ctrl_ops tsmux_ctrl_ops = {
	.s_ctrl = tsmux_s_ctrl,
	.try_ctrl = tsmux_try_ctrl,
	.g_volatile_ctrl = tsmux_g_ctrl,
};

int tsmux_init_ctrl(struct tsmux_context *dev_p)
{
	static struct v4l2_ctrl_config stm_v4l2_tsmux_custom_ctrls[] = {
		{
		 .ops = &tsmux_ctrl_ops,
		 .id = V4L2_CID_STM_TSMUX_PCR_PERIOD,
		 .type = V4L2_CTRL_TYPE_INTEGER,
		 .name = "PCR Period",
		 .min = 900,
		 .max = 9000,
		 .step = 1,
		 .def = 9000,
		 .flags = V4L2_CTRL_FLAG_VOLATILE,
		 },
		{
		 .ops = &tsmux_ctrl_ops,
		 .id = V4L2_CID_STM_TSMUX_GEN_PCR_STREAM,
		 .type = V4L2_CTRL_TYPE_BOOLEAN,
		 .name = "Generate PCR Stream",
		 .min = 0,
		 .max = 1,
		 .step = 1,
		 .def = 0,
		 .flags = V4L2_CTRL_FLAG_VOLATILE,
		 },
		{
		 .ops = &tsmux_ctrl_ops,
		 .id = V4L2_CID_STM_TSMUX_PCR_PID,
		 .type = V4L2_CTRL_TYPE_INTEGER,
		 .name = "PCR Pid",
		 .min = 0,
		 .max = 0x1ffe,
		 .step = 1,
		 .def = 0x1ffe,
		 .flags = V4L2_CTRL_FLAG_VOLATILE,
		 },
		{
		 .ops = &tsmux_ctrl_ops,
		 .id = V4L2_CID_STM_TSMUX_TABLE_GEN,
		 .type = V4L2_CTRL_TYPE_BITMASK,
		 .name = "Table Generation Flags",
		 .min = V4L2_STM_TSMUX_TABLE_GEN_NONE,
		 .max = (V4L2_STM_TSMUX_TABLE_GEN_SDT |
			 V4L2_STM_TSMUX_TABLE_GEN_PAT_PMT),
		 .step = 0,
		 .def = (V4L2_STM_TSMUX_TABLE_GEN_SDT |
			 V4L2_STM_TSMUX_TABLE_GEN_PAT_PMT),
		 .flags = V4L2_CTRL_FLAG_VOLATILE,
		 },
		{
		 .ops = &tsmux_ctrl_ops,
		 .id = V4L2_CID_STM_TSMUX_TABLE_PERIOD,
		 .type = V4L2_CTRL_TYPE_INTEGER,
		 .name = "Table Period",
		 .min = 2250,
		 .max = 9000,
		 .step = 1,
		 .def = 9000,
		 .flags = V4L2_CTRL_FLAG_VOLATILE,
		 },
		{
		 .ops = &tsmux_ctrl_ops,
		 .id = V4L2_CID_STM_TSMUX_TS_ID,
		 .type = V4L2_CTRL_TYPE_INTEGER,
		 .name = "TS ID",
		 .min = 0,
		 .max = 0xffff,
		 .step = 1,
		 .def = 0,
		 .flags = V4L2_CTRL_FLAG_VOLATILE,
		 },
		{
		 .ops = &tsmux_ctrl_ops,
		 .id = V4L2_CID_STM_TSMUX_BITRATE_IS_CONSTANT,
		 .type = V4L2_CTRL_TYPE_BOOLEAN,
		 .name = "TS Fixed Bitrate",
		 .min = 0,
		 .max = 1,
		 .step = 1,
		 .def = 0,
		 .flags = V4L2_CTRL_FLAG_VOLATILE,
		 },
		{
		 .ops = &tsmux_ctrl_ops,
		 .id = V4L2_CID_STM_TSMUX_BITRATE,
		 .type = V4L2_CTRL_TYPE_INTEGER,
		 .name = "TS Bitrate",
		 .min = 100 * 1024,
		 .max = 60 * 1024 * 1024,
		 .step = 1,
		 .def = 48 * 1024 * 1024,
		 .flags = V4L2_CTRL_FLAG_VOLATILE,
		 },
		{
		 .ops = &tsmux_ctrl_ops,
		 .id = V4L2_CID_STM_TSMUX_PROGRAM_NUMBER,
		 .type = V4L2_CTRL_TYPE_INTEGER,
		 .name = "Program Number",
		 .min = 0,
		 .max = 0xffff,
		 .step = 1,
		 .def = 1,
		 .flags = V4L2_CTRL_FLAG_VOLATILE,
		 },
		{
		 .ops = &tsmux_ctrl_ops,
		 .id = V4L2_CID_STM_TSMUX_PMT_PID,
		 .type = V4L2_CTRL_TYPE_INTEGER,
		 .name = "PMT PID",
		 .min = 1,
		 .max = 0x1ffd,
		 .step = 1,
		 .def = 0x31,
		 .flags = V4L2_CTRL_FLAG_VOLATILE,
		 },
		{
		 .ops = &tsmux_ctrl_ops,
		 .id = V4L2_CID_STM_TSMUX_SDT_PROV_NAME,
		 .type = V4L2_CTRL_TYPE_STRING,
		 .name = "SDT Provider Name",
		 .min = 0,
		 .max = TSMUX_MAX_PROGRAM_NAME_SIZE,
		 .step = 1,
		 .def = 0,
		 .flags = V4L2_CTRL_FLAG_VOLATILE,
		 },
		{
		 .ops = &tsmux_ctrl_ops,
		 .id = V4L2_CID_STM_TSMUX_SDT_SERV_NAME,
		 .type = V4L2_CTRL_TYPE_STRING,
		 .name = "SDT Service Name",
		 .min = 0,
		 .max = TSMUX_MAX_PROGRAM_NAME_SIZE,
		 .step = 1,
		 .def = 0,
		 .flags = V4L2_CTRL_FLAG_VOLATILE,
		 },
		{
		 .ops = &tsmux_ctrl_ops,
		 .id = V4L2_CID_STM_TSMUX_PMT_DESCRIPTOR,
		 .type = V4L2_CTRL_TYPE_STRING,
		 .name = "PMT Descriptor",
		 .min = 0,
		 .max = TSMUX_MAX_DESCRIPTOR_SIZE,
		 .step = 1,
		 .def = 0,
		 .flags = V4L2_CTRL_FLAG_VOLATILE,
		 },
		{
		 .ops = &tsmux_ctrl_ops,
		 .id = V4L2_CID_STM_TSMUX_INCLUDE_PCR,
		 .type = V4L2_CTRL_TYPE_BOOLEAN,
		 .name = "Include PCR In Stream",
		 .min = 0,
		 .max = 0,
		 .step = 1,
		 .def = 0,
		 .flags = V4L2_CTRL_FLAG_VOLATILE,
		 },
		{
		 .ops = &tsmux_ctrl_ops,
		 .id = V4L2_CID_STM_TSMUX_STREAM_PID,
		 .type = V4L2_CTRL_TYPE_INTEGER,
		 .name = "Stream PID",
		 .min = 0,
		 .max = 0x1ffe,
		 .step = 1,
		 .def = 0x100,
		 .flags = V4L2_CTRL_FLAG_VOLATILE,
		 },
		{
		 .ops = &tsmux_ctrl_ops,
		 .id = V4L2_CID_STM_TSMUX_PES_STREAM_ID,
		 .type = V4L2_CTRL_TYPE_INTEGER,
		 .name = "PES Stream_id",
		 .min = 0,
		 .max = 0xff,
		 .step = 1,
		 .def = 0x0,
		 .flags = V4L2_CTRL_FLAG_VOLATILE,
		 },
		{
		 .ops = &tsmux_ctrl_ops,
		 .id = V4L2_CID_STM_TSMUX_STREAM_DESCRIPTOR,
		 .type = V4L2_CTRL_TYPE_STRING,
		 .name = "Stream Descriptor",
		 .min = 0,
		 .max = TSMUX_MAX_DESCRIPTOR_SIZE,
		 .step = 1,
		 .def = 0,
		 .flags = V4L2_CTRL_FLAG_VOLATILE,
		 },
		{
		 .ops = &tsmux_ctrl_ops,
		 .id = V4L2_CID_STM_TSMUX_BIT_BUFFER_SIZE,
		 .type = V4L2_CTRL_TYPE_INTEGER,
		 .name = "Decoder Bit Buffer Size",
		 .min = 0,
		 .max = 64 * 1024 * 1024,
		 .step = 1,
		 .def = 4 * 1024 * 1024,
		 .flags = V4L2_CTRL_FLAG_VOLATILE,
		 },
		{
		 .ops = &tsmux_ctrl_ops,
		 .id = V4L2_CID_STM_TSMUX_INDEXING_MASK,
		 .type = V4L2_CTRL_TYPE_BITMASK,
		 .name = "Indexing Mask",
		 .min = V4L2_STM_TSMUX_INDEX_NONE,
		 .max = V4L2_STM_TSMUX_INDEX_PUSI | V4L2_STM_TSMUX_INDEX_PTS |
		 V4L2_STM_TSMUX_INDEX_I_FRAME | V4L2_STM_TSMUX_INDEX_B_FRAME |
		 V4L2_STM_TSMUX_INDEX_P_FRAME | V4L2_STM_TSMUX_INDEX_DIT |
		 V4L2_STM_TSMUX_INDEX_RAP,
		 .step = 0,
		 .def = V4L2_STM_TSMUX_INDEX_NONE,
		 .flags = V4L2_CTRL_FLAG_VOLATILE,
		 },
		{
		 .ops = &tsmux_ctrl_ops,
		 .id = V4L2_CID_STM_TSMUX_SECTION_PERIOD,
		 .type = V4L2_CTRL_TYPE_INTEGER,
		 .name = "Section Period",
		 .min = 0,
		 .max = 2000,
		 .step = 1,
		 .def = 0,
		 .flags = V4L2_CTRL_FLAG_VOLATILE,
		 },
		{
		 .ops = &tsmux_ctrl_ops,
		 .id = V4L2_CID_STM_TSMUX_INDEXING,
		 .type = V4L2_CTRL_TYPE_BOOLEAN,
		 .name = "Indexing Enable or Disable",
		 .min = 0,
		 .max = 1,
		 .step = 1,
		 .def = 0,
		 .flags = V4L2_CTRL_FLAG_VOLATILE,
		 },
		{
		 .ops = &tsmux_ctrl_ops,
		 .id = V4L2_CID_STM_TSMUX_IGNORE_AUTO_PAUSE,
		 .type = V4L2_CTRL_TYPE_BOOLEAN,
		 .name = "Auto Pause Enable or Disable",
		 .min = 0,
		 .max = 1,
		 .step = 1,
		 .def = 0,
		 .flags = V4L2_CTRL_FLAG_VOLATILE,
		},
	};

	int ret = 0;
	int i = 0;

	ret = v4l2_ctrl_handler_init(&dev_p->ctx_ctrl_handler,
				     ARRAY_SIZE(stm_v4l2_tsmux_custom_ctrls));
	if (ret) {
		printk(KERN_ERR
		       "v4l2_ctrl_handler_init() failed for tsmux ret = %d",
		       ret);
		return ret;
	}

	for (i = 0; i < ARRAY_SIZE(stm_v4l2_tsmux_custom_ctrls); i++) {
		v4l2_ctrl_new_custom(&dev_p->ctx_ctrl_handler,
				     &stm_v4l2_tsmux_custom_ctrls[i], NULL);
		if (dev_p->ctx_ctrl_handler.error) {
			ret = dev_p->ctx_ctrl_handler.error;
			v4l2_ctrl_handler_free(&dev_p->ctx_ctrl_handler);
			printk(KERN_ERR
			       "v4l2_ctrl_new_custom() failed for tsmux. ret = %x",
			       ret);
			return ret;
		}
	}

	return 0;
}

/*********************************/
/* Code related to TSMUX Objects */
/*********************************/
static int release_buffer(stm_object_h src, struct stm_data_block *block_list,
			  uint32_t block_count)
{
	int ret = 0;
	struct tsmux_context *ctx = (struct tsmux_context *)src;
	char name[] = "STL-Tsmuxout";
	struct tsmux_buffer *buf;
	void *vaddr;
	enum vb2_buffer_state state = VB2_BUF_STATE_DONE;

	if (ctx == NULL) {
		pr_err("NULL ctx on release_buffer\n");
		ret = -1;
		goto error;
	}
	if (strncmp(name, ctx->name, sizeof(name) - 1) != 0) {
		pr_err("Invalid ctx name on release_buffer %s, %s, %d\n",
		       ctx->name, &name[0], sizeof(name));
		ret = -1;
		goto error;
	}
	dprintk(ctx, 1, "%s\n", __func__);
	spin_lock(&ctx->slock);
	if (list_empty(&ctx->vidq.active)) {
		pr_err("Release buffer when list is empty\n");
		ret = -EINVAL;
		spin_unlock(&ctx->slock);
		goto error;
	}
	buf = list_entry(ctx->vidq.active.next, struct tsmux_buffer, list);
	list_del(&buf->list);
	spin_unlock(&ctx->slock);
	ctx->bufcount--;
	pr_debug("%s: Active buffers decremented now = %d\n",
		 ctx->name, ctx->bufcount);

	vaddr = vb2_plane_vaddr(&buf->vb, 0);
	if (!vaddr) {
		pr_err("Can't get buffer vaddr\n");
		ret = -EINVAL;
		goto error;
	}
	if (vaddr == block_list[0].data_addr) {
		state = VB2_BUF_STATE_DONE;
	} else {
		pr_err("Unmatched buffer released vaddr=%p bl=%p\n",
		       vaddr, block_list[0].data_addr);
		state = VB2_BUF_STATE_ERROR;
	}
	vb2_buffer_done(&buf->vb, state);

error:
	return ret;
}

struct stm_te_async_data_interface_src tsg_src_if = {
	release_buffer
};

static int tsmux_g_index(struct file *file, void *fh,
			 struct v4l2_enc_idx *tsmux_idx)
{
	struct tsmux_context *ctx;
	struct tsmux_device *dev = NULL;
	unsigned int bytes_available = 0;
	unsigned int bytes_read = 0;
	stm_te_tsg_index_data_t *index_data = NULL;
	stm_te_tsg_index_data_t *from_te;
	int ret = 0;
	int i = 0;

	dev = video_drvdata(file);

	if (dev == NULL) {
		ret = -EINVAL;
		goto error;
	}

	ctx = &dev->tsmux_in_ctx;

	ret =
	    stm_memsink_test_for_data(ctx->memsink_index_object,
				      &bytes_available);
	if (ret) {
		ret = -EIO;
		goto error;
	}

	if (bytes_available == 0) {
		tsmux_idx->entries = 0;
		return 0;
	}

	/*We can send back only V4L2_ENC_IDX_ENTRIES to application. */
	if ((bytes_available / sizeof(stm_te_tsg_index_data_t)) >
	    V4L2_ENC_IDX_ENTRIES)
		bytes_available =
		    sizeof(stm_te_tsg_index_data_t) * V4L2_ENC_IDX_ENTRIES;

	/*Allocate memory to store the pulled data. */
	index_data =
	    (stm_te_tsg_index_data_t *) kzalloc(bytes_available, GFP_KERNEL);
	if (!index_data) {
		ret = -ENOMEM;
		goto error;
	}

	memset(index_data, 0, bytes_available);

	ret = stm_memsink_pull_data(ctx->memsink_index_object,
				    index_data, bytes_available, &bytes_read);
	if (ret) {
		kfree(index_data);
		index_data = NULL;
		ret = -EIO;
		goto error;
	}

	if (bytes_read != bytes_available) {
		kfree(index_data);
		index_data = NULL;
		ret = -EIO;
		goto error;
	}

	tsmux_idx->entries = bytes_read / sizeof(stm_te_tsg_index_data_t);
	tsmux_idx->entries_cap = V4L2_ENC_IDX_ENTRIES;

	for (i = 0; i < tsmux_idx->entries; i++) {
		from_te = (stm_te_tsg_index_data_t *) (index_data + i);

		if (ctx->dst_fmt.dvb_data_type == V4L2_DVB_FMT_TS)
			tsmux_idx->entry[i].offset =
			    from_te->packet_count * 188;
		else if (ctx->dst_fmt.dvb_data_type == V4L2_DVB_FMT_DLNA_TS)
			tsmux_idx->entry[i].offset =
			    from_te->packet_count * 192;
		tsmux_idx->entry[i].reserved[STM_TSMUX_INDEX_RESERVED_PID] =
		    from_te->pid;
		tsmux_idx->entry[i].pts = from_te->pts;
		/* The following value may be useful in future if length is required by application.
		   This value is not used right now. So commenting. */
		/*tsmux_idx->entry[i].length = ; */

		tsmux_idx->entry[i].flags = V4L2_STM_TSMUX_INDEX_NONE;
		if (from_te->index & STM_TE_TSG_INDEX_PUSI)
			tsmux_idx->entry[i].flags = V4L2_ENC_IDX_FRAME_PUSI;
		else if (from_te->index & STM_TE_TSG_INDEX_PTS)
			tsmux_idx->entry[i].flags = V4L2_ENC_IDX_FRAME_PTS;
		else if (from_te->index & STM_TE_TSG_INDEX_I_FRAME)
			tsmux_idx->entry[i].flags = V4L2_ENC_IDX_FRAME_I;
		else if (from_te->index & STM_TE_TSG_INDEX_B_FRAME)
			tsmux_idx->entry[i].flags = V4L2_ENC_IDX_FRAME_B;
		else if (from_te->index & STM_TE_TSG_INDEX_P_FRAME)
			tsmux_idx->entry[i].flags = V4L2_ENC_IDX_FRAME_P;
		else if (from_te->index & STM_TE_TSG_INDEX_DIT)
			tsmux_idx->entry[i].flags = V4L2_ENC_IDX_FRAME_DIT;
		else if (from_te->index & STM_TE_TSG_INDEX_RAP)
			tsmux_idx->entry[i].flags = V4L2_ENC_IDX_FRAME_RAP;
	}

	kfree(index_data);

error:
	return ret;
}

static int send_data(struct tsmux_context *ctx, struct tsmux_buffer *buf)
{
	int ret = 0;
	unsigned int data_size;
	unsigned int freq_m;
	void *vaddr = NULL;

	vaddr = vb2_plane_vaddr(&buf->vb, 0);
	if (!vaddr) {
		pr_err("Invalid buffer vaddr\n");
		ret = -EINVAL;
		return ret;
	}

	data_size = vb2_get_plane_payload(&buf->vb, 0);
	freq_m = ctx->section_table_period;

	ret =
	    stm_te_tsg_sec_filter_set(ctx->tsg_object, vaddr, data_size,
				      freq_m);
	if (ret) {
		pr_err("Couldn't set the section data \n");
		ret = -EINVAL;
		goto error;
	}

error:
	return ret;
}

static int send_buffer(struct tsmux_context *ctx, struct tsmux_buffer *buf)
{
	void *vaddr;
	int ret = 0;
	struct stm_data_block *block_list;
	int data_blocks;
	unsigned long bytesused;
	void *meta_p = NULL;
	struct v4l2_buffer *v4l2_buf = &buf->vb.v4l2_buf;
	struct v4l2_tsmux_src_es_metadata es_meta;
	struct stm_se_compressed_frame_metadata_s metadata;
	uint64_t temp_time;
	uint64_t native_time;

	dprintk(ctx, 3, "%s\n", __func__);
	memset(&metadata, 0, sizeof(metadata));
	if (!ctx->dev->tsg_if.queue_buffer) {
		pr_err("No queue_buffer function to call\n");
		ret = -EINVAL;
		goto error;
	}
	vaddr = vb2_plane_vaddr(&buf->vb, 0);
	if (!vaddr) {
		pr_err("Invalid buffer vaddr\n");
		ret = -EINVAL;
		goto error;
	}
	block_list = kzalloc(sizeof(*block_list), GFP_KERNEL);
	if (!block_list) {
		pr_err("Block list not allocated\n");
		ret = -ENOMEM;
		goto error;
	}
	bytesused = vb2_get_plane_payload(&buf->vb, 0);
	block_list->data_addr = vaddr;
	block_list->len = bytesused;
	block_list->next = NULL;

	if ((ctx->src_fmt.dvb_data_type == V4L2_DVB_FMT_ES) ||
	    (ctx->src_fmt.dvb_data_type == V4L2_DVB_FMT_PES)) {
		meta_p = (void *)v4l2_buf->reserved;

		if (meta_p) {
			ret = copy_from_user(&es_meta, meta_p,
					     sizeof(struct
						    v4l2_tsmux_src_es_metadata));
			if (ret) {
				pr_err("can't get input ES meta data\n");
				ret = -EINVAL;
				goto error_kfree;
			}
		}

		/*DTS and dit_transition is set based on if metadata_exists or not. */
		if (meta_p) {
			/*If metadata exists, then copy these fields from metadata. */
			metadata.discontinuity = (es_meta.dit_transition ?
					STM_SE_DISCONTINUITY_DISCONTINUOUS :
					STM_SE_DISCONTINUITY_CONTINUOUS);
		}
	}

	/*end-of-stream and PTS is decided based on 'byteused'
	   as it is not coming in metadata from user-space. */
	if (bytesused && (v4l2_buf->flags & V4L2_BUF_FLAG_STM_TSMUX_NULL_PACKET)) {
		pr_err("Invalid data: filled buffer sent as null packet\n");
		ret = -EINVAL;
		goto error_kfree;
	} else if ((!bytesused && (v4l2_buf->flags & V4L2_BUF_FLAG_STM_TSMUX_NULL_PACKET))
		   || (bytesused)) {
		temp_time = (uint64_t)buf->vb.v4l2_buf.timestamp.tv_usec;
		native_time = (uint64_t)buf->vb.v4l2_buf.timestamp.tv_sec;

		temp_time = temp_time * 90;
		do_div(temp_time, 1000);
		metadata.native_time = (native_time * 90000) + temp_time;
		metadata.encoded_time = metadata.native_time;
		/* Pass metadata information to the TSMux
		 * Those information are only used for indexing purpose
		 * and thus we only set them in case of the filter is
		 * being indexed */
		if (bytesused && ctx->connected_to_index_filter) {
			metadata.encoding = STM_SE_ENCODE_STREAM_ENCODING_VIDEO_H264;
			if (buf->vb.v4l2_buf.flags & V4L2_BUF_FLAG_STM_ENCODE_GOP_START)
				metadata.video.new_gop = true;
			if (buf->vb.v4l2_buf.flags & V4L2_BUF_FLAG_STM_ENCODE_CLOSED_GOP)
				metadata.video.closed_gop = true;

			if (buf->vb.v4l2_buf.flags & V4L2_BUF_FLAG_KEYFRAME)
				metadata.video.picture_type = STM_SE_PICTURE_TYPE_I;
			else if (buf->vb.v4l2_buf.flags & V4L2_BUF_FLAG_PFRAME)
				metadata.video.picture_type = STM_SE_PICTURE_TYPE_P;
			else if (buf->vb.v4l2_buf.flags & V4L2_BUF_FLAG_BFRAME)
				metadata.video.picture_type = STM_SE_PICTURE_TYPE_B;
		}
	} else if (!bytesused)
		metadata.discontinuity = STM_SE_DISCONTINUITY_EOS;

	ret = ctx->dev->tsg_if.queue_buffer(ctx->tsg_object,
					    &metadata, block_list, 1,
					    &data_blocks);
	if (ret) {
		pr_err("Can't send buffer to tsg_filter\n");
		ret = -EINVAL;
		goto error_kfree;
	}
	ctx->bufcount++;
	pr_debug("%s: Active buffers incremented now = %d\n",
		 ctx->name, ctx->bufcount);

error_kfree:
	kfree(block_list);
error:
	return ret;
}

static int create_tsmux(struct tsmux_context *ctx)
{
	int ret = 0;
	char name[STM_REGISTRY_MAX_TAG_SIZE];

	dprintk(ctx, 2, "%s\n", __func__);
	snprintf(name, sizeof(name), "%s-tsmux", ctx->name);
	if (ctx->tsmux_object) {
		pr_err("Tsmux object already exists, deleting it\n");
		if (0 != stm_te_tsmux_delete(ctx->tsmux_object)) {
			pr_err("Can't delete existing tsmux object\n");
			ret = -EINVAL;
			goto error;
		}
	}
	if (0 != stm_te_tsmux_new(name, &ctx->tsmux_object)) {
		pr_err("Unable to get new tsmux\n");
		ret = -ENOMEM;
		goto error;
	}
	snprintf(name, sizeof(name), "%s-memsrc", ctx->name);
	if (ctx->memsink_object) {
		pr_err("Memsink object already exists, deleting it\n");
		if (0 != stm_memsink_delete(ctx->memsink_object)) {
			pr_err("Can't delete exising memsink object\n");
			ret = -EINVAL;
			goto error;
		}
	}

	/* We need to have BLOCKING mode beause we want to wait for the
	 * buffer to be filled
	 */
	if (0 != stm_memsink_new(name, STM_IOMODE_BLOCKING_IO,
				 KERNEL, &ctx->memsink_object)) {
		pr_err("Unable to get new memsink\n");
		ret = -ENOMEM;
		goto error;
	}
	if (0 != stm_te_tsmux_attach(ctx->tsmux_object, ctx->memsink_object)) {
		pr_err("Can't attach tsmux to memsink\n");
		ret = -EINVAL;
		goto error;
	}

error:
	return ret;
}

static int destroy_tsmux(struct tsmux_context *ctx)
{
	int ret = 0;

	dprintk(ctx, 2, "%s\n", __func__);
	if (ctx->tsmux_object) {
		if (ctx->memsink_object) {
			if (0 != stm_te_tsmux_detach(ctx->tsmux_object,
						     ctx->memsink_object)) {
				pr_err("Can't detach tsmux from memsink\n");
				ret = -EINVAL;
			}
			if (0 != stm_memsink_delete(ctx->memsink_object)) {
				pr_err("Can't close memsink %s\n", ctx->name);
				ret = -EINVAL;
			}
			ctx->memsink_object = NULL;
		}
		if (0 != stm_te_tsmux_delete(ctx->tsmux_object)) {
			pr_err("Can't delete tsmux %s\n", ctx->name);
			ret = -EINVAL;
		}
		ctx->tsmux_object = NULL;
	}
	return ret;
}

static int tsmux_start(struct tsmux_context *ctx)
{
	int ret = 0;
	struct sched_param param;

	dprintk(ctx, 1, "%s\n", __func__);

	if (ctx->tsmux_object && ctx->tsmux_started == false) {
		if (0 != stm_te_tsmux_start(ctx->tsmux_object)) {
			pr_err("Can't start tsmux object\n");
			ret = -EINVAL;
			goto error;
		}
		ctx->tsmux_started = true;
		ctx->vidq.kthread = kthread_run(tsmux_thread, ctx, ctx->name);
	} else {
		pr_warn("Attempt to start an already started tsmux\n");
	}

	if (thread_stl_tsmux[1]) {
		param.sched_priority = thread_stl_tsmux[1];
		if (sched_setscheduler(ctx->vidq.kthread,
				thread_stl_tsmux[0], &param)) {
			printk(KERN_ERR "FAILED to set scheduling \
			parameters to priority %d Mode :(%d)\n", \
			thread_stl_tsmux[1], thread_stl_tsmux[0]);
		}
	}

	dprintk(ctx, 1, "returning from %s\n", __func__);
error:
	return ret;
}

static int tsmux_stop(struct tsmux_context *ctx)
{
	int ret = 0;

	dprintk(ctx, 1, "%s\n", __func__);

	/* shutdown control thread */
	if (ctx->vidq.kthread) {
		kthread_stop(ctx->vidq.kthread);
		ctx->vidq.kthread = NULL;
	}
	if (ctx->tsmux_object && ctx->tsmux_started) {
		/* Stop streaming should only be called when we know that the
		 * last buffer has been consumed, so we should now explicity
		 * stop the tsmux, in order to correctly immediately free the
		 * end of stream buffers.
		 */
		if (0 != stm_te_tsmux_set_control(ctx->tsmux_object,
					 STM_TE_TSMUX_CNTRL_STOP_MODE,
					 STM_TE_TSMUX_CNTRL_STOP_MODE_FORCED)) {
			pr_err("Can't set the STOP MODE to FORCED\n");
			ret = -EINVAL;
			return ret;
		}

		if (0 != stm_te_tsmux_stop(ctx->tsmux_object))
			pr_warn("Can't properly stop tsmux object\n");

		ctx->tsmux_started = false;
	}

	/* Release all active buffers */
	while (!list_empty(&ctx->vidq.active)) {
		struct tsmux_buffer *buf;
		buf = list_entry(ctx->vidq.active.next,
				 struct tsmux_buffer, list);
		list_del(&buf->list);
		vb2_buffer_done(&buf->vb, VB2_BUF_STATE_ERROR);
		dprintk(ctx, 2, "[%p/%d] done\n", buf, buf->vb.v4l2_buf.index);
	}

	return 0;
}

static int attach_to_tsg_index_filter(struct tsmux_context *out_ctx)
{
	int ret = 0;
	struct tsmux_context *ctx = &out_ctx->dev->tsmux_in_ctx;

	if (out_ctx->tsg_object && ctx->tsg_index_object) {
		ret =
		    stm_te_tsmux_attach(out_ctx->tsg_object,
					ctx->tsg_index_object);
		if (ret) {
			pr_err
			    ("ERROR in attach tsg filter to index filter, ret=%d \n",
			     ret);
			ret = -ENOMEM;
		}
	}

	return 0;
}

static int detach_from_tsg_index_filter(struct tsmux_context *out_ctx)
{
	int ret = 0;
	struct tsmux_context *ctx = &out_ctx->dev->tsmux_in_ctx;

	/* tsg_object can be detached from tsg_index_object
	   only if tsmux is in STOP state. */
	if (ctx->tsmux_object && ctx->tsmux_started == false) {
		if (out_ctx->connected_to_index_filter == 1) {
			if (out_ctx->tsg_object
			    && out_ctx->dev->tsmux_in_ctx.tsg_index_object) {
				ret =
				    stm_te_tsmux_detach(out_ctx->tsg_object,
							out_ctx->dev->
							tsmux_in_ctx.
							tsg_index_object);
				if (ret) {
					pr_err
					    ("Unable to detach tsg-filter from index-filter.\n");
					goto error;
				}
			}
			ctx->connected_to_index_filter = 0;
		}
	} else {
		pr_err("TSMUX is not in STOP state, cannot detach.\n");
		ret = -EINVAL;
	}

error:
	return ret;
}

static int create_tsg_index_filter(struct tsmux_context *octx)
{
	int ret = 0;
	char name[STM_REGISTRY_MAX_TAG_SIZE];
	struct tsmux_context *ctx = &octx->dev->tsmux_in_ctx;
	stm_te_object_h tsmux = octx->dev->tsmux_in_ctx.tsmux_object;

	ret = stm_te_tsg_index_filter_new(tsmux, &ctx->tsg_index_object);
	if (ret) {
		pr_err("Unable to get new index filter\n");
		ret = -ENOMEM;
	}

	/* We need to have BLOCKING mode beause we want to wait for the
	 * buffer to be filled
	 */
	snprintf(name, sizeof(name), "%s-memindex", ctx->name);
	if (0 != stm_memsink_new(name, STM_IOMODE_BLOCKING_IO,
				 KERNEL, &ctx->memsink_index_object)) {
		pr_err("Unable to get new index memsink\n");
		ret = -ENOMEM;
		goto error;
	}

	if (0 !=
	    stm_te_tsmux_attach(ctx->tsg_index_object,
				ctx->memsink_index_object)) {
		pr_err("Can't attach tsg_index_filter to memsink\n");
		ret = -EINVAL;
		goto error;
	}

error:
	return ret;
}

static int create_tsg_sec_filter(struct tsmux_context *ctx)
{
	int ret = 0;
	stm_te_object_h tsmux = ctx->dev->tsmux_in_ctx.tsmux_object;

	ret = stm_te_tsg_sec_filter_new(tsmux, &ctx->tsg_object);
	if (ret) {
		pr_err("Unable to get new section filter\n");
		ret = -ENOMEM;
	}
	return ret;
}

static int create_tsg_filter(struct tsmux_context *ctx)
{
	int ret = 0;
	stm_te_object_h tsmux = ctx->dev->tsmux_in_ctx.tsmux_object;
	stm_object_h tsg_obj_type;
	stm_object_h ctx_h = (stm_object_h) ctx;
	size_t actual_size;
	char data_type[STM_REGISTRY_MAX_TAG_SIZE];

	dprintk(ctx, 2, "%s\n", __func__);
	if (0 != stm_te_tsg_filter_new(tsmux, &ctx->tsg_object)) {
		pr_err("Unable to get new tsg filter\n");
		ret = -ENOMEM;
		goto error;
	}
	if (!ctx->dev->tsg_if.connect) {
		if (0 != stm_registry_get_object_type(ctx->tsg_object,
						      &tsg_obj_type)) {
			pr_err("Can't find tsg filter obj type\n");
			ret = -EINVAL;
			goto error;
		}
		if (0 != stm_registry_get_attribute(tsg_obj_type,
						    STM_TE_ASYNC_DATA_INTERFACE,
						    data_type,
						    sizeof
						    (stm_te_async_data_interface_sink_t),
						    &ctx->dev->tsg_if,
						    &actual_size)) {
			pr_err("Can't find tsg filter async interface\n");
			ret = -EINVAL;
			goto error;
		}
	}
	if (0 != ctx->dev->tsg_if.connect(ctx_h, ctx->tsg_object, &tsg_src_if)) {
		pr_err("Can't connect to tsg filter\n");
		ret = -EINVAL;
		goto error;
	}
	ctx->tsg_connected = true;

error:
	return ret;
}

static int destroy_tsg_filter(struct tsmux_context *ctx)
{
	int ret = 0;
	stm_object_h dummy = (stm_object_h) NULL;

	dprintk(ctx, 2, "%s\n", __func__);
	if (ctx->tsg_object) {
		if (ctx->src_fmt.dvb_data_type != V4L2_DVB_FMT_DATA) {
			if (ctx->dev->tsg_if.disconnect && ctx->tsg_connected) {
				if (0 != ctx->dev->tsg_if.disconnect(dummy,
								     ctx->tsg_object)) {
					pr_err
					    ("Can't disconnect from tsg_filter\n");
					ret = -EINVAL;
				}
				ctx->tsg_connected = false;
			}
		}
		if (0 != stm_te_filter_delete(ctx->tsg_object)) {
			pr_err("Can't close output %s\n", ctx->name);
			ret = -EINVAL;
		}
		ctx->tsg_object = NULL;
	}
	return ret;
}

static int tsg_start(struct tsmux_context *ctx)
{
	int ret = 0;
	struct tsmux_context *muxctx = &ctx->dev->tsmux_in_ctx;
	struct tsmux_buffer *buf;

	/* Send any active buffers queued before STREAMON called */
	list_for_each_entry(buf, &ctx->vidq.active, list) {
		dprintk(ctx, 2, "[%p/%d] sending\n", buf,
			buf->vb.v4l2_buf.index);
		ret = send_buffer(ctx, buf);
		if (ret) {
			pr_err("Can't send buffer to tsg_filter\n");
			spin_lock(&ctx->slock);
			list_del(&buf->list);
			spin_unlock(&ctx->slock);
			vb2_buffer_done(&buf->vb, VB2_BUF_STATE_ERROR);
			goto error;
		}
		/* Wake the mux thread in case it is ready now */
		dprintk(ctx, 1, "Waking mux thread, check if ready to mux\n");
		wake_up_interruptible(&muxctx->vidq.wq);
	}

error:
	return ret;
}

static int tsg_stop(struct tsmux_context *ctx)
{
	int ret = 0;

	/* Release any active buffers */
	while (!list_empty(&ctx->vidq.active)) {
		struct tsmux_buffer *buf;
		buf = list_entry(ctx->vidq.active.next,
				 struct tsmux_buffer, list);
		list_del(&buf->list);
		vb2_buffer_done(&buf->vb, VB2_BUF_STATE_ERROR);
		dprintk(ctx, 2, "[%p/%d] done\n", buf, buf->vb.v4l2_buf.index);
	}
	return ret;
}

static void destroy_all(struct tsmux_device *dev)
{
	struct tsmux_context *ctx;
	int i;
	int ret = 0;

	ctx = &dev->tsmux_in_ctx;
	/* First stop the tsmux if it is running. This will close
	 * all streams and release all buffers calling the release_buffer
	 * callback to free them back to the v4l2 queues.
	 */
	if (ctx->tsmux_object && ctx->tsmux_started == true)
		tsmux_stop(ctx);

	if (dev->tsmux_in_ctx.tsg_index_object
	    && dev->tsmux_in_ctx.memsink_index_object) {
		ret =
		    stm_te_tsmux_detach(dev->tsmux_in_ctx.tsg_index_object,
					dev->tsmux_in_ctx.memsink_index_object);
		if (ret) {
			pr_err
			    ("Unable to detach index-filter from index-memsink\n");
		}
	}

	/* Now close down the tsg objects */
	for (i = 0; i < TSMUX_NUM_OUTPUTS; i++) {
		/*
		   - check if we have a index filter.
		   - if yes detach the tsg filter from index filter
		 */
		if (dev->tsmux_out_ctx[i].connected_to_index_filter == 1) {
			if (dev->tsmux_out_ctx[i].tsg_object
			    && dev->tsmux_in_ctx.tsg_index_object) {
				ret =
				    stm_te_tsmux_detach(dev->tsmux_out_ctx
							[i].tsg_object,
							dev->tsmux_in_ctx.
							tsg_index_object);
				if (ret) {
					pr_err
					    ("Unable to detach tsg-filter from index-filter\n");
				}
			}

			ctx->connected_to_index_filter = 0;
		}

		destroy_tsg_filter(&dev->tsmux_out_ctx[i]);
	}
	/* Now close down the tsmux object */

	if (dev->tsmux_in_ctx.memsink_index_object) {
		ret =
		    stm_memsink_delete(dev->tsmux_in_ctx.memsink_index_object);
		if (ret) {
			pr_err("Unable to delete index-memsink \n");
		}
		dev->tsmux_in_ctx.memsink_index_object = NULL;
	}

	if (dev->tsmux_in_ctx.tsg_index_object) {
		ret = stm_te_filter_delete(dev->tsmux_in_ctx.tsg_index_object);
		if (ret) {
			pr_err("Unable to delete index-filter \n");
		}
	}

	destroy_tsmux(&dev->tsmux_in_ctx);

	if (dev->tsmux_in_ctx.tsg_index_object) {
		dev->tsmux_in_ctx.tsg_index_object = NULL;
		dev->tsmux_in_ctx.tsmux_object = NULL;
	}

	return;
}

#define frames_to_ms(frames)					\
	((frames * WAKE_NUMERATOR * 1000) / WAKE_DENOMINATOR)

static bool ready_to_mux(struct tsmux_context *ctx)
{
	int ret = false;
	struct tsmux_dmaqueue *dmaq = &ctx->vidq;

	if (list_empty(&dmaq->active)) {
		dprintk(ctx, 1, "Nothing in mux buffer queue\n");
		ret = false;
		goto out;
	} else
		ret = true;

out:
	return ret;
}

static int tsmux_get_data(struct tsmux_context *ctx)
{
	int ret = 0;
	struct tsmux_buffer *buf;
	struct tsmux_dmaqueue *dmaq = &ctx->vidq;
	uint32_t bytesused;
	void *vaddr;

	dprintk(ctx, 1, "%s\n", __func__);

	spin_lock(&ctx->slock);
	if (list_empty(&dmaq->active)) {
		dprintk(ctx, 3, "No active queue to serve\n");
		spin_unlock(&ctx->slock);
		return 0;
	}
	buf = list_entry(dmaq->active.next, struct tsmux_buffer, list);
	spin_unlock(&ctx->slock);

	if (!buf->vb.v4l2_planes[0].m.userptr) {
		pr_err("Invalid tsmux buffer\n");
		goto error;
	}
	vaddr = vb2_plane_vaddr(&buf->vb, 0);
	if (!vaddr) {
		pr_err("Can't get buf vaddr\n");
		goto error;
	}
	ret = stm_memsink_pull_data(ctx->memsink_object,
				    vaddr,
				    buf->vb.v4l2_planes[0].length, &bytesused);
	if (ret == -EAGAIN) {
		/* Don't remove buffer from list, just pass it
		 * again next time */
		goto error;
	} else {
		/* Remove buffer from list */
		spin_lock(&ctx->slock);
		list_del(&buf->list);
		spin_unlock(&ctx->slock);
		if (ret) {
			pr_err("Error pulling from memsink\n");
			vb2_set_plane_payload(&buf->vb, 0, 0);
			do_gettimeofday(&buf->vb.v4l2_buf.timestamp);
			vb2_buffer_done(&buf->vb, VB2_BUF_STATE_ERROR);
		} else {
			dprintk(ctx, 1, "Memsink returned %d bytes\n",
				bytesused);
			vb2_set_plane_payload(&buf->vb, 0, bytesused);
			do_gettimeofday(&buf->vb.v4l2_buf.timestamp);
			vb2_buffer_done(&buf->vb, VB2_BUF_STATE_DONE);
		}
	}
error:
	return ret;
}

static int tsmux_thread(void *data)
{
	int ret = 0;
	struct tsmux_context *ctx = data;
	struct tsmux_dmaqueue *dmaq = &ctx->vidq;
	int timeout;

	dprintk(ctx, 1, "thread started\n");
	timeout = msecs_to_jiffies(frames_to_ms(100));
	while (!kthread_should_stop()) {
		dprintk(ctx, 1, "Thread doing wait\n");
		ret = wait_event_interruptible_timeout(dmaq->wq,
						       ready_to_mux(ctx),
						       timeout);
		if (ret < 0) {
			pr_err("Tsmux thread interrupted\n");
			break;
		} else if (!ret) {
			dprintk(ctx, 1, "thread timeout\n");
			continue;
		}
		dprintk(ctx, 1, "Getting tsmux data\n");
		ret = tsmux_get_data(ctx);
		if ((ret < 0) && (ret != -EAGAIN)) {
			pr_err("Tsmux get data returned error\n");
			ctx->vidq.kthread = NULL;
			destroy_all(ctx->dev);
			break;
		}
	}
	dprintk(ctx, 1, "thread: exit\n");
	ctx->vidq.kthread = NULL;
	return 0;
}

/* **************** */
/* Queue operations */
/* **************** */

static int queue_setup(struct vb2_queue *vq,
		       const struct v4l2_format *fmt,
		       unsigned int *nbuffers, unsigned int *nplanes,
		       unsigned int sizes[], void *alloc_ctxs[])
{
	struct tsmux_context *ctx = vb2_get_drv_priv(vq);
	int ret = 0, count = *nbuffers;

	switch (vq->type) {
	case V4L2_BUF_TYPE_DVB_OUTPUT:
		*nplanes = 1;
		*nbuffers = TSMUX_OUTPUT_NUM_BUFFERS;

		/*
		 * Inform tsmux about the number of user buffers. This control
		 * specifies the number of minimum buffers which must remain
		 * empty, so, that the flow control is not applied. The flow
		 * control here, is the release callback which is not issued
		 * by tsmux until the particular buffer index starting from the
		 * first queued buffer in the list has been consumed.
		 */
		ret = stm_te_filter_set_control(ctx->tsg_object,
					STM_TE_TSG_FILTER_CONTROL_NUM_INPUT_BUFFERS,
					*nbuffers);
		if (ret)
			pr_err("%s(): failed to configure tsmux for flow control\n", __func__);

		break;

	case V4L2_BUF_TYPE_DVB_CAPTURE:
		*nplanes = 1;
		*nbuffers = count;
		break;

	default:
		return -EINVAL;
	}

	return ret;
}

static void buf_queue(struct vb2_buffer *vb)
{
	struct tsmux_context *ctx = vb2_get_drv_priv(vb->vb2_queue);
	struct tsmux_buffer *buf = container_of(vb, struct tsmux_buffer, vb);
	struct tsmux_device *dev = ctx->dev;
	struct tsmux_context *in_ctx = &dev->tsmux_in_ctx;
	int ret = 0;

	dprintk(ctx, 3, "%s\n", __func__);

	if (ctx->src_fmt.dvb_data_type != V4L2_DVB_FMT_DATA) {
		spin_lock(&ctx->slock);
		list_add_tail(&buf->list, &ctx->vidq.active);
		spin_unlock(&ctx->slock);
	}

	/* Check if the tsmux ctx is streaming before trying to send
	 * a buffer to an output, or kicking the tsmux thread for an input
	 */
	if (!vb2_is_streaming(&in_ctx->vb_vidq))
		return;

	if (ctx->type == TSMUX_OUTPUT) {
		if (ctx->src_fmt.dvb_data_type == V4L2_DVB_FMT_DATA) {
			ret = send_data(ctx, buf);
			if (ret) {
				pr_err("Can't send buffer to tsg_sec_filter\n");
				vb2_buffer_done(vb, VB2_BUF_STATE_ERROR);
			} else {
				vb2_buffer_done(vb, VB2_BUF_STATE_DONE);
			}
		} else {
			ret = send_buffer(ctx, buf);
			if (ret) {
				pr_err("Can't send buffer to tsg_filter\n");
				spin_lock(&ctx->slock);
				list_del(&buf->list);
				spin_unlock(&ctx->slock);
				vb2_buffer_done(vb, VB2_BUF_STATE_ERROR);
			}
		}
	} else {
		if (ctx->vidq.kthread) {
			/* Wake the mux thread to check if ready */
			dprintk(ctx, 1, "Waking mux thread\n");
			wake_up_interruptible(&ctx->vidq.wq);
		} else {
			pr_warn("Mux thread exited, release buffer\n");
			spin_lock(&ctx->slock);
			list_del(&buf->list);
			spin_unlock(&ctx->slock);
			vb2_buffer_done(vb, VB2_BUF_STATE_ERROR);
		}
	}
}

static int start_streaming(struct vb2_queue *vq, unsigned int count)
{
	struct tsmux_context *ctx = vb2_get_drv_priv(vq);
	struct tsmux_device *dev = ctx->dev;
	int ret = 0;
	int i;
	struct tsmux_context *out_ctx;

	dprintk(ctx, 2, "%s\n", __func__);
	if (ctx->type != TSMUX_INPUT)
		return 0;

	/* We can now startup the tsmux and all its currently
	 * attached tsg steams
	 */
	ret = tsmux_start(ctx);
	if (ret) {
		pr_err("Can't start tsmux %s\n", ctx->name);
		goto error;
	}
	for (i = 0; i < TSMUX_NUM_OUTPUTS; i++) {
		out_ctx = &dev->tsmux_out_ctx[i];
		if (out_ctx->tsg_object) {
			ret = tsg_start(out_ctx);
			if (ret) {
				pr_err("Can't start stream %d\n", i);
				goto error;
			}
		}
	}
error:
	return ret;
}

/* abort streaming and wait for last buffer */
static int stop_streaming(struct vb2_queue *vq)
{
	struct tsmux_context *ctx = vb2_get_drv_priv(vq);
	int ret = 0;
	dprintk(ctx, 2, "%s\n", __func__);
	if (ctx->type == TSMUX_OUTPUT)
		ret = tsg_stop(ctx);
	else
		ret = tsmux_stop(ctx);

	return ret;
}

static void wait_prepare(struct vb2_queue *q)
{
	struct tsmux_context *ctx = vb2_get_drv_priv(q);
	/* Release the driver lock while ioctl gets user mmap semaphore */
	dprintk(ctx, 3, "%s\n", __func__);
	mutex_unlock(&ctx->dev->mutex);

}

static void wait_finish(struct vb2_queue *q)
{
	struct tsmux_context *ctx = vb2_get_drv_priv(q);
	/* Re-acquire the driver lock now the ioctl has the mmap semaphore */
	dprintk(ctx, 3, "%s\n", __func__);
	mutex_lock(&ctx->dev->mutex);
}

static struct vb2_ops tsmux_qops = {
	.queue_setup = queue_setup,
	.wait_prepare = wait_prepare,
	.wait_finish = wait_finish,
	.start_streaming = start_streaming,
	.stop_streaming = stop_streaming,
	.buf_queue = buf_queue,
};

/* *************** */
/* ioctl functions */
/* *************** */

static int tsmux_query_cap(struct file *file, void *priv,
			   struct v4l2_capability *cap)
{
	struct tsmux_device *dev;
	dev = video_drvdata(file);

	snprintf(cap->driver, sizeof(cap->driver), "TS Muxer #%d",
		 dev->tsmux_id);
	strlcpy(cap->card, "STMicroelectronics", sizeof(cap->card));
	cap->bus_info[0] = 0;
	cap->version = LINUX_VERSION_CODE;
	cap->capabilities = V4L2_CAP_DVB_CAPTURE | V4L2_CAP_DVB_OUTPUT
	    | V4L2_CAP_STREAMING;

	return 0;
}

static int tsmux_enum_fmt(struct file *file, void *priv, struct v4l2_fmtdesc *f)
{
	struct tsmux_fmt *fmt;
	int i, num, size;
	u32 type = f->type;

	size = ARRAY_SIZE(tsmux_formats);
	num = 0;

	for (i = 0; i < size; ++i) {
		if (tsmux_formats[i].types == type) {
			/* index-th format of type type found ? */
			if (num == f->index)
				break;
			/* Correct type but haven't reached our index yet,
			 * just increment per-type index */
			++num;
		}
	}

	if (i < size) {
		/* Format found */
		fmt = &tsmux_formats[i];
		f->flags = fmt->flags;
		strncpy(f->description, fmt->name, sizeof(f->description) - 1);
		f->pixelformat = fmt->fourcc;
		memset(f->reserved, 0, sizeof(f->reserved));
		return 0;
	}

	/* Format not found */
	return -EINVAL;
}

static struct tsmux_fmt *find_tsmux_enum_format(struct v4l2_format *f)
{
	struct tsmux_fmt *fmt;
	int size;
	int k;

	size = ARRAY_SIZE(tsmux_formats);

	for (k = 0; k < size; k++) {
		fmt = &tsmux_formats[k];
		if ((f->type == fmt->types)
		    && (f->fmt.dvb.data_type == fmt->fourcc))
			break;
	}

	if (k == size)
		return NULL;

	return &tsmux_formats[k];
}

static int tsmux_try_fmt(struct file *file, void *priv, struct v4l2_format *f)
{
	struct tsmux_context *ctx = file->private_data;
	struct tsmux_fmt *fmt;

	if (ctx == NULL)
		return -EINVAL;

	fmt = find_tsmux_enum_format(f);
	if (fmt == NULL)
		return -EINVAL;

	return 0;

}

static int tsmux_s_fmt_output(struct file *file, void *priv,
			      struct v4l2_format *f)
{
	struct tsmux_context *ctx;
	struct tsmux_device *dev;
	int ret = 0, index;
	int ctrl_value;
	struct stm_tsmux_vfh *vfh =
	    container_of(file->private_data, struct stm_tsmux_vfh, fh);

	dev = video_drvdata(file);

	if (dev == NULL) {
		ret = -EINVAL;
		goto error;
	}

	index = dev->tsmux_id;

	if (index >= tsmux_dev_nums) {
		ret = -EINVAL;
		goto error;
	}

	if (dev->output_pad_users >= TSMUX_NUM_OUTPUTS) {
		dprintk(ctx, 1, "Too many users for the current device %d\n",
			index);
		ret = -EINVAL;
		goto error;
	}

	ctx = &dev->tsmux_out_ctx[dev->output_pad_users % TSMUX_NUM_OUTPUTS];
	dprintk(ctx, 1, "%s ctx=%p\n", __func__, ctx);

	dev->output_pad_users++;

	if (vfh->ctx == NULL) {
		dev->users++;
		vfh->ctx = ctx;
		vfh->fh.ctrl_handler = &ctx->ctx_ctrl_handler;
	}

	if (!dev->tsmux_in_ctx.tsmux_object) {
		pr_err("TSMUX input must be set first\n");
		ret = -EINVAL;
		goto error;
	}

	if (!ctx->users && !ctx->tsg_object) {
		if (f->fmt.dvb.data_type == V4L2_DVB_FMT_DATA)
			ret = create_tsg_sec_filter(ctx);
		else
			ret = create_tsg_filter(ctx);
		if (ret)
			goto error;
		dprintk(ctx, 1, "No tsg filter so added one\n");
	}

	if (f->fmt.dvb.data_type == V4L2_DVB_FMT_ES)
		ctrl_value = 0;
	else if (f->fmt.dvb.data_type == V4L2_DVB_FMT_PES)
		ctrl_value = 1;
	else if (f->fmt.dvb.data_type == V4L2_DVB_FMT_DATA)
		ctrl_value = 0;
	else {
		ret = -EINVAL;
		goto error;
	}

	if (f->fmt.dvb.data_type != V4L2_DVB_FMT_DATA) {
		ret = stm_te_filter_set_control(ctx->tsg_object,
						STM_TE_TSG_FILTER_CONTROL_STREAM_IS_PES,
						ctrl_value);
		if (ret) {
			pr_err("Failed to set control \n");
			goto error;
		}

		ctrl_value = f->fmt.dvb.codec;

		if (ctrl_value <=
		    V4L2_STM_TSMUX_STREAM_TYPE_RESERVED ||
		    ctrl_value >= V4L2_STM_TSMUX_STREAM_TYPE_VIDEO_DIRAC) {

			ret = -EINVAL;
			goto error;
		}

		ret = stm_te_filter_set_control(ctx->tsg_object,
						STM_TE_TSG_FILTER_CONTROL_STREAM_TYPE,
						ctrl_value);
		if (ret) {
			pr_err("Failed to set control \n");
			goto error;
		}
	}

	ctx->src_fmt.type = f->type;
	ctx->src_fmt.sizebuf = f->fmt.dvb.buffer_size;
	ctx->src_fmt.dvb_data_type = f->fmt.dvb.data_type;
	ctx->users++;

error:
	return ret;
}

static int tsmux_s_fmt_capture(struct file *file, void *priv,
			       struct v4l2_format *f)
{
	struct tsmux_context *ctx;
	struct tsmux_device *dev = NULL;
	int ret = 0, index;
	int ctrl_value;
	struct stm_tsmux_vfh *vfh =
	    container_of(file->private_data, struct stm_tsmux_vfh, fh);

	dev = video_drvdata(file);

	if (dev == NULL) {
		ret = -EINVAL;
		goto error;
	}

	index = dev->tsmux_id;

	if (index >= tsmux_dev_nums) {
		ret = -EINVAL;
		goto error;
	}

	ctx = &dev->tsmux_in_ctx;
	dprintk(ctx, 1, "%s ctx=%p \n", __func__, ctx);

	if (vfh->ctx == NULL) {
		dev->users++;
		vfh->ctx = ctx;
		vfh->fh.ctrl_handler = &ctx->ctx_ctrl_handler;
	}

	if (!ctx->users && !ctx->tsmux_object) {
		ret = create_tsmux(ctx);
		if (ret)
			goto error;
		dprintk(ctx, 1, "No tsmux so added one\n");
	}

	if (f->fmt.dvb.data_type == V4L2_DVB_FMT_TS)
		ctrl_value = STM_TE_OUTPUT_TYPE_DVB;
	else if (f->fmt.dvb.data_type == V4L2_DVB_FMT_DLNA_TS)
		ctrl_value = STM_TE_OUTPUT_TYPE_TTS;
	else {
		ret = -EINVAL;
		goto error;
	}

	ret = stm_te_tsmux_set_control(ctx->tsmux_object,
				       STM_TE_TSMUX_CNTRL_OUTPUT_TYPE,
				       ctrl_value);
	if (ret) {
		pr_err("Failed to set control \n");
		goto error;
	}

	ctx->dst_fmt.type = f->type;
	ctx->dst_fmt.sizebuf = f->fmt.dvb.buffer_size;
	ctx->dst_fmt.dvb_data_type = f->fmt.dvb.data_type;
	ctx->users++;

error:
	return ret;
}

static int tsmux_g_fmt(struct file *file, void *priv, struct v4l2_format *f)
{
	struct tsmux_context *ctx = file->private_data;

	if (ctx == NULL)
		return -EINVAL;

	switch (f->type) {
	case V4L2_BUF_TYPE_DVB_OUTPUT:
		f->fmt.dvb.buffer_size = ctx->src_fmt.sizebuf;
		f->fmt.dvb.data_type = ctx->src_fmt.dvb_data_type;
		break;
	case V4L2_BUF_TYPE_DVB_CAPTURE:
		f->fmt.dvb.buffer_size = ctx->dst_fmt.sizebuf;
		f->fmt.dvb.data_type = ctx->dst_fmt.dvb_data_type;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int tsmux_queryctrl(struct file *file, void *priv,
			   struct v4l2_queryctrl *qc)
{
	int i, size;
	int id;

	size = ARRAY_SIZE(stm_v4l2_tsmux_ctrls);
	id = qc->id;

	for (i = 0; i < size; ++i) {
		if (id == stm_v4l2_tsmux_ctrls[i].id) {
			*qc = stm_v4l2_tsmux_ctrls[i];
			break;
		}
	}

	if (i >= size)
		return -EINVAL;

	return 0;
}

static int tsmux_reqbufs(struct file *file, void *priv,
			 struct v4l2_requestbuffers *reqbufs)
{
	struct tsmux_context *ctx = file_to_tsmux_ctx(file);

	if (ctx == NULL)
		return -EINVAL;
	dprintk(ctx, 1, "%s ctx=%p,reqtype=%d,qtype=%d\n",
		__func__, ctx, reqbufs->type, ctx->vb_vidq.type);

	return vb2_reqbufs(&ctx->vb_vidq, reqbufs);
}

static int tsmux_querybuf(struct file *file, void *priv,
			  struct v4l2_buffer *buf)
{
	struct tsmux_context *ctx = file_to_tsmux_ctx(file);

	if (ctx == NULL)
		return -EINVAL;
	dprintk(ctx, 1, "%s\n", __func__);

	return vb2_querybuf(&ctx->vb_vidq, buf);
}

static int tsmux_qbuf(struct file *file, void *priv, struct v4l2_buffer *buf)
{
	struct tsmux_context *ctx = file_to_tsmux_ctx(file);

	if (ctx == NULL)
		return -EINVAL;
	dprintk(ctx, 2, "%s useraddr=0x%lx len=%d bytesused=%d\n", __func__,
		buf->m.userptr, buf->length, buf->bytesused);

	/* Check for bad buffer */
	if (buf->bytesused > buf->length)
		return -EINVAL;

	if (ctx->type == TSMUX_OUTPUT) {
		if (!ctx->tsg_object) {
			pr_err("Invalid tsg object\n");
			return -EINVAL;
		}
	} else {
		if (!ctx->tsmux_object) {
			pr_err("Invalid tsmux object\n");
			return -EINVAL;
		}
	}

	return vb2_qbuf(&ctx->vb_vidq, buf);
}

static int tsmux_dqbuf(struct file *file, void *priv, struct v4l2_buffer *buf)
{
	struct tsmux_context *ctx = file_to_tsmux_ctx(file);

	if (ctx == NULL)
		return -EINVAL;
	dprintk(ctx, 2, "%s\n", __func__);

	return vb2_dqbuf(&ctx->vb_vidq, buf, (file->f_flags & O_NONBLOCK));
}

static int tsmux_streamon(struct file *file, void *priv,
			  enum v4l2_buf_type type)
{
	int ret;
	struct tsmux_context *ctx = file_to_tsmux_ctx(file);

	if (ctx == NULL)
		return -EINVAL;
	dprintk(ctx, 1, "%s\n", __func__);

	switch (type) {
	case V4L2_BUF_TYPE_DVB_OUTPUT:
		ret = vb2_streamon(&ctx->vb_vidq, type);

#ifdef TUNNELLING_SUPPORTED
		if (ret == 0)
			ctx->src_connect_type = STM_V4L2_TSMUX_CONNECT_INJECT;
#endif
		break;
	case V4L2_BUF_TYPE_DVB_CAPTURE:
		ret = vb2_streamon(&ctx->vb_vidq, type);

#ifdef TUNNELLING_SUPPORTED
		/*Check for tunnelling between encoder-tsmux */
		if (ctx->connection_src) {
			struct stm_v4l2_encoder_device *EncoderDevice;
			EncoderDevice =
			    (struct stm_v4l2_encoder_device *)
			    ctx->connection_src;

			if (EncoderDevice->encode_stream) {
				ret =
				    stm_se_encode_stream_attach
				    (EncoderDevice->encode_stream,
				     ctx->tsmux_object);
				if (ret) {
					printk(KERN_ERR
					       "can't attach the encoder to tsmux\n");
					return -EIO;
				}
				ctx->src_connect_type =
				    STM_V4L2_TSMUX_CONNECT_ENCODE;
			}
		}
#endif
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int tsmux_streamoff(struct file *file, void *priv,
			   enum v4l2_buf_type type)
{
#ifdef TUNNELLING_SUPPORTED
	int err = 0;
#endif
	int ret;
	struct tsmux_context *ctx = file_to_tsmux_ctx(file);

	if (ctx == NULL)
		return -EINVAL;
	dprintk(ctx, 1, "%s\n", __func__);

	switch (type) {
	case V4L2_BUF_TYPE_DVB_OUTPUT:

#ifdef TUNNELLING_SUPPORTED
		if (ctx->src_connect_type == STM_V4L2_TSMUX_CONNECT_INJECT)
			ctx->src_connect_type = STM_V4L2_TSMUX_CONNECT_NONE;
#endif

		ret = vb2_streamoff(&ctx->vb_vidq, type);
		break;
	case V4L2_BUF_TYPE_DVB_CAPTURE:

#ifdef TUNNELLING_SUPPORTED
		/* check tunneling */
		/*The TSMUX should check the tunnelling only with encoder, as encoder
		   will be checking the tunneling with decoder.. */
		if (ctx->connection_src &&
		    ctx->src_connect_type == STM_V4L2_TSMUX_CONNECT_ENCODE) {

			struct stm_v4l2_encoder_device *EncoderDevice;
			EncoderDevice =
			    (struct stm_v4l2_encoder_device *)
			    ctx->connection_src;

			if (EncoderDevice->encode_stream) {
				ret =
				    stm_se_encode_stream_detach
				    (EncoderDevice->encode_stream,
				     ctx->tsmux_object);
				if (ret) {
					printk(KERN_ERR
					       "====>>> can't detach the tsmux\n");
					err++;
				}
			}
			ctx->src_connect_type = STM_V4L2_TSMUX_CONNECT_NONE;
		}

		if (ctx->src_connect_type == STM_V4L2_TSMUX_CONNECT_INJECT)
			ctx->src_connect_type = STM_V4L2_TSMUX_CONNECT_NONE;
#endif

		ret = vb2_streamoff(&ctx->vb_vidq, type);
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

/* *************** */
/* File operations */
/* *************** */

static int tsmux_open(struct file *file)
{
	struct stm_tsmux_vfh *vfh;
	struct tsmux_device *dev;
	dev = video_drvdata(file);

	/* Allocate memory */
	vfh = kzalloc(sizeof(struct stm_tsmux_vfh), GFP_KERNEL);
	if (vfh == NULL) {
		printk(KERN_ERR
		       "ERROR : memory allocation failed on tsmux_open() \n");
		return -ENOMEM;
	}
	v4l2_fh_init(&vfh->fh, dev->vfd);
	file->private_data = &vfh->fh;
	vfh->ctx = NULL;
	v4l2_fh_add(&vfh->fh);

	return 0;
}

static int tsmux_close(struct file *file)
{
	struct stm_tsmux_vfh *vfh =
	    container_of(file->private_data, struct stm_tsmux_vfh, fh);
	struct tsmux_context *ctx = vfh->ctx;
	struct tsmux_device *dev;
	int ret = 0;

	if (ctx == NULL)
		goto error;

	dprintk(ctx, 1, "close called (dev=%s), file %p\n", ctx->name, file);

	dev = ctx->dev;
	ctx->users--;

	if (ctx->type == TSMUX_OUTPUT)
		dev->output_pad_users--;

	if (ctx->users > 0)
		goto error;

	/* Closing any ctx closes the whole tsmux object structure */
	destroy_all(dev);

	/* Free the queue */
	vb2_queue_release(&ctx->vb_vidq);

	v4l2_fh_del(&vfh->fh);
	v4l2_fh_exit(&vfh->fh);

	file->private_data = NULL;
	kfree(vfh);

	dev->users--;
	if (dev->users > 0)
		return 0;

	return ret;

error:
	v4l2_fh_del(&vfh->fh);
	v4l2_fh_exit(&vfh->fh);
	file->private_data = NULL;
	kfree(vfh);

	return ret;
}

static unsigned int tsmux_poll(struct file *file,
			       struct poll_table_struct *wait)
{
	struct tsmux_context *ctx = file_to_tsmux_ctx(file);

	dprintk(ctx, 3, "%s\n", __func__);

	return vb2_poll(&ctx->vb_vidq, file, wait);
}

static struct v4l2_file_operations tsmux_fops = {
	.owner = THIS_MODULE,
	.open = tsmux_open,
	.release = tsmux_close,
	.unlocked_ioctl = video_ioctl2,
	.poll = tsmux_poll,
};

static const struct v4l2_ioctl_ops tsmux_ioctl_ops = {

	.vidioc_querycap = tsmux_query_cap,

	.vidioc_enum_fmt_dvb_out = tsmux_enum_fmt,
	.vidioc_enum_fmt_dvb_cap = tsmux_enum_fmt,
	.vidioc_enum_fmt_vid_out_mplane = tsmux_enum_fmt,
	.vidioc_enum_fmt_vid_cap_mplane = tsmux_enum_fmt,

	.vidioc_try_fmt_dvb_out = tsmux_try_fmt,
	.vidioc_try_fmt_dvb_cap = tsmux_try_fmt,
	.vidioc_try_fmt_vid_out_mplane = tsmux_try_fmt,
	.vidioc_try_fmt_vid_cap_mplane = tsmux_try_fmt,

	.vidioc_s_fmt_dvb_out = tsmux_s_fmt_output,
	.vidioc_s_fmt_dvb_cap = tsmux_s_fmt_capture,
	.vidioc_s_fmt_vid_out_mplane = tsmux_s_fmt_output,
	.vidioc_s_fmt_vid_cap_mplane = tsmux_s_fmt_capture,

	.vidioc_g_fmt_dvb_out = tsmux_g_fmt,
	.vidioc_g_fmt_vid_out_mplane = tsmux_g_fmt,
	.vidioc_g_fmt_dvb_cap = tsmux_g_fmt,
	.vidioc_g_fmt_vid_cap_mplane = tsmux_g_fmt,

	.vidioc_queryctrl = tsmux_queryctrl,
	.vidioc_g_enc_index = tsmux_g_index,

	.vidioc_reqbufs = tsmux_reqbufs,
	.vidioc_querybuf = tsmux_querybuf,

	.vidioc_qbuf = tsmux_qbuf,
	.vidioc_dqbuf = tsmux_dqbuf,

	.vidioc_streamon = tsmux_streamon,
	.vidioc_streamoff = tsmux_streamoff,

};

void tsmux_device_release(struct video_device *vdev)
{
	video_device_release(vdev);
}

static struct video_device tsmux_template = {
	.name = "tsmux",
	.fops = &tsmux_fops,
	.ioctl_ops = &tsmux_ioctl_ops,
	.release = tsmux_device_release,
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 7, 0))
        .vfl_dir = VFL_DIR_M2M,
#endif
};

/* **************** */
/* tsmux ctxice  */
/* **************** */

static int tsmux_context_queue_init(struct tsmux_context *ctx,
				    enum v4l2_buf_type type)
{
	struct vb2_queue *q;
	int err = 0;

	q = &ctx->vb_vidq;
	memset(q, 0, sizeof(ctx->vb_vidq));
	q->type = type;
	q->io_modes = VB2_USERPTR;
	q->drv_priv = ctx;
	q->buf_struct_size = sizeof(struct tsmux_buffer);
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 9, 0))
	q->timestamp_type = V4L2_BUF_FLAG_TIMESTAMP_MONOTONIC;
#endif
	q->ops = &tsmux_qops;
	q->mem_ops = &vb2_vmalloc_memops;
	err = vb2_queue_init(q);

	return err;
}

static int tsmux_context_init_ctrl(struct tsmux_device *dev)
{
	struct tsmux_context *ctx;
	int err = 0;
	int i;

	/* Initialize the V4L2 ctxs */
	for (i = 0; i < TSMUX_NUM_OUTPUTS; i++) {
		ctx = &dev->tsmux_out_ctx[i];
		err = tsmux_init_ctrl(ctx);
		if (err) {
			printk(KERN_ERR "%s: tsmux ctrl init failed (%d)\n",
			       __func__, err);
		}
	}
	ctx = &dev->tsmux_in_ctx;
	err = tsmux_init_ctrl(ctx);
	if (err) {
		printk(KERN_ERR "%s: tsmux ctrl init failed (%d)\n",
		       __func__, err);
	}

	return err;
}

/**
 * stm_v4l2_tsmux_ctrl_exit() - free the tsmux control handler
 */
static void stm_v4l2_tsmux_ctrl_exit(struct tsmux_device *tsmux)
{
	struct v4l2_ctrl_handler *hdl;
	int i;

	for (i = 0; i < TSMUX_NUM_OUTPUTS; i++) {
		hdl = &tsmux->tsmux_out_ctx[i].ctx_ctrl_handler;
		v4l2_ctrl_handler_free(hdl);
	}

	hdl = &tsmux->tsmux_in_ctx.ctx_ctrl_handler;
	v4l2_ctrl_handler_free(hdl);
}

static int tsmux_context_init(struct tsmux_device *dev)
{
	struct tsmux_context *ctx;
	int err = 0;
	int i;

	/* Initialize the V4L2 ctxs */
	for (i = 0; i < TSMUX_NUM_OUTPUTS; i++) {
		ctx = &dev->tsmux_out_ctx[i];
		snprintf(ctx->name, sizeof(ctx->name),
			 "STL-Tsmuxout%02d.%02d", dev->tsmux_id, i);
		ctx->dev = dev;
		ctx->fmt = &tsmux_formats[0];
		ctx->type = TSMUX_OUTPUT;
		ctx->tsmux_object = NULL;
		ctx->tsmux_started = false;
		ctx->tsg_object = NULL;
		ctx->tsg_connected = false;
		ctx->memsink_object = NULL;
		ctx->users = 0;
		ctx->connection_src = NULL;
		ctx->tsg_index_object = NULL;
		ctx->memsink_index_object = NULL;
		ctx->connected_to_index_filter = 0;
		ctx->section_table_period = 0;
		spin_lock_init(&ctx->slock);
		mutex_init(&ctx->mutex);
		err = tsmux_context_queue_init(ctx, V4L2_BUF_TYPE_DVB_OUTPUT);
		if (err) {
			pr_err("Can't init queue for %s\n", ctx->name);
			goto error;
		}
		/* init tsmux dma queues */
		INIT_LIST_HEAD(&ctx->vidq.active);
		ctx->bufcount = 0;
		init_waitqueue_head(&ctx->vidq.wq);
	}
	ctx = &dev->tsmux_in_ctx;
	snprintf(ctx->name, sizeof(ctx->name), "tsmuxin%02d", dev->tsmux_id);
	ctx->dev = dev;
	ctx->fmt = &tsmux_formats[0];
	ctx->type = TSMUX_INPUT;
	ctx->tsmux_object = NULL;
	ctx->tsmux_started = false;
	ctx->tsg_object = NULL;
	ctx->tsg_connected = false;
	ctx->memsink_object = NULL;
	ctx->users = 0;
	ctx->connection_src = NULL;
	ctx->tsg_index_object = NULL;
	ctx->memsink_index_object = NULL;
	ctx->connected_to_index_filter = 0;
	ctx->section_table_period = 0;
	spin_lock_init(&ctx->slock);
	mutex_init(&ctx->mutex);
	err = tsmux_context_queue_init(ctx, V4L2_BUF_TYPE_DVB_CAPTURE);
	if (err) {
		pr_err("Can't init queue for %s\n", ctx->name);
		goto error;
	}
	/* init tsmux dma queues */
	INIT_LIST_HEAD(&ctx->vidq.active);
	ctx->bufcount = 0;
	init_waitqueue_head(&ctx->vidq.wq);

error:
	return err;
}

static int tsmux_context_release(struct tsmux_device *dev)
{
	struct tsmux_context *ctx;
	int i;

	/* Release everything associated with the ctx queue */
	for (i = 0; i < TSMUX_NUM_OUTPUTS; i++) {
		ctx = &dev->tsmux_out_ctx[i];
		vb2_queue_release(&ctx->vb_vidq);
	}
	ctx = &dev->tsmux_in_ctx;
	vb2_queue_release(&ctx->vb_vidq);

	return 0;
}

static int tsmux_create_vid_connection(void)
{
#ifdef TUNNELLING_SUPPORTED
	struct media_entity *src;
	struct media_entity *sink;
	int ret;

	src =
	    stm_media_find_entity_with_type_first
	    (MEDIA_ENT_T_V4L2_SUBDEV_VIDEO_ENCODER);
	while (src) {

		sink =
		    stm_media_find_entity_with_type_first
		    (MEDIA_ENT_T_V4L2_SUBDEV_TSMUXER);
		while (sink) {
			ret = media_entity_create_link(src, 1, sink, 0, 0);
			if (ret < 0) {
				printk(KERN_ERR "failed video connection \n");
				goto link_create_err;
			}
			sink =
			    stm_media_find_entity_with_type_next(sink,
								 MEDIA_ENT_T_V4L2_SUBDEV_TSMUXER);
		}

		src =
		    stm_media_find_entity_with_type_next(src,
							 MEDIA_ENT_T_V4L2_SUBDEV_VIDEO_ENCODER);
	}
#endif

	return 0;

#ifdef TUNNELLING_SUPPORTED
link_create_err:
	return ret;
#endif
}

static int tsmux_create_aud_connection(void)
{
#ifdef TUNNELLING_SUPPORTED
	struct media_entity *src;
	struct media_entity *sink;
	int ret;

	src =
	    stm_media_find_entity_with_type_first
	    (MEDIA_ENT_T_V4L2_SUBDEV_AUDIO_ENCODER);
	while (src) {

		sink =
		    stm_media_find_entity_with_type_first
		    (MEDIA_ENT_T_V4L2_SUBDEV_TSMUXER);
		while (sink) {
			ret = media_entity_create_link(src, 1, sink, 1, 0);
			if (ret < 0) {
				printk(KERN_ERR "failed audio connection \n");
				goto link_create_err;
			}
			sink =
			    stm_media_find_entity_with_type_next(sink,
								 MEDIA_ENT_T_V4L2_SUBDEV_TSMUXER);
		}

		src =
		    stm_media_find_entity_with_type_next(src,
							 MEDIA_ENT_T_V4L2_SUBDEV_AUDIO_ENCODER);
	}
#endif

	return 0;

#ifdef TUNNELLING_SUPPORTED
link_create_err:
	return ret;
#endif
}

static int tsmux_create_dev_subdev_conns(struct tsmux_device *dev_p)
{
	struct media_entity *src, *sink;
	int i, ret;
	unsigned int media_flags =
	    MEDIA_LNK_FL_IMMUTABLE | MEDIA_LNK_FL_ENABLED;

	/* Create links from media-entity to tsmux device */
	/* src --> media-entity i.e tsmux_subdev */
	/* sink --> v4l2-tsmux-device i.e vfd */
	src = &(dev_p->tsmux_subdev.entity);
	sink = &(dev_p->vfd->entity);
	ret =
	    media_entity_create_link(src, TSMUX_CAPTURE_PAD_ID, sink,
				     TSMUX_CAPTURE_PAD_ID, media_flags);
	if (ret < 0) {
		printk(KERN_ERR "%s: failed to create link (%d)\n",
		       __func__, ret);
		return ret;
	}

	/* Create links from tsmux device to media-entity */
	/* src --> v4l2-tsmux-device i.e vfd */
	/* sink --> media-entity i.e tsmux_subdev */
	for (i = 0; i < TSMUX_NUM_OUTPUTS; i++) {
		src = &(dev_p->vfd->entity);
		sink = &(dev_p->tsmux_subdev.entity);
		ret = media_entity_create_link(src, i, sink, i, media_flags);
		if (ret < 0) {
			printk(KERN_ERR "%s: failed to create link (%d)\n",
			       __func__, ret);
			return ret;
		}
	}
	return 0;
}

static int tsmux_subdev_link_setup(struct media_entity *entity,
				   const struct media_pad *local,
				   const struct media_pad *remote, u32 flags)
{
#ifdef TUNNELLING_SUPPORTED
	struct tsmux_device *dev_p;
	struct stm_v4l2_encoder_device *dev_p_encoder;
	struct v4l2_subdev *subdev;
	int i = 0;

	if (local->flags == MEDIA_PAD_FL_SINK) {
		if (flags & MEDIA_LNK_FL_ENABLED) {

			subdev = media_entity_to_v4l2_subdev(local->entity);
			dev_p =
			    container_of(subdev, struct tsmux_device,
					 tsmux_subdev);

			if (remote->entity->type ==
			    MEDIA_ENT_T_V4L2_SUBDEV_VIDEO_ENCODER) {
				dev_p_encoder =
				    encoder_entity_to_EncoderDevice
				    (remote->entity);
				dev_p->tsmux_in_ctx.connection_src =
				    (void *)dev_p_encoder;
				/*Since a single tsmux_in may be connected to several tsmux_out[]
				   so we should make tsmux_out[] also aware of encoder-connection.. */
				for (i = 0; i < 8; i++) {
					dev_p->tsmux_out_ctx[i].connection_src =
					    (void *)dev_p_encoder;
				}
			}
		} else {
			subdev = media_entity_to_v4l2_subdev(local->entity);
			dev_p =
			    container_of(subdev, struct tsmux_device,
					 tsmux_subdev);

			if (remote->entity->type ==
			    MEDIA_ENT_T_V4L2_SUBDEV_VIDEO_ENCODER) {
				dev_p->tsmux_in_ctx.connection_src = NULL;
				/*Also set all the tsmux_out[] to NULL.. */
				for (i = 0; i < 8; i++) {
					dev_p->tsmux_out_ctx[i].connection_src =
					    NULL;
				}
			}
		}
	}
#endif
	return 0;
}

static int tsmux_device_link_setup(struct media_entity *entity,
				   const struct media_pad *local,
				   const struct media_pad *remote, u32 flags)
{
	return 0;
}

static const struct media_entity_operations tsmux_subdev_media_ops = {
	.link_setup = tsmux_subdev_link_setup,
};

static const struct media_entity_operations tsmux_dev_media_ops = {
	.link_setup = tsmux_device_link_setup,
};

static const struct v4l2_subdev_ops tsmux_subdev_ops = {
};

static int tsmux_init_subdev(struct tsmux_device *dev_p)
{
	struct v4l2_subdev *subdev = &dev_p->tsmux_subdev;
	struct media_entity *me = &subdev->entity;
	int ret, i;

	/* Initialize the V4L2 subdev / MC entity */
	v4l2_subdev_init(subdev, &tsmux_subdev_ops);
	snprintf(subdev->name, sizeof(subdev->name), "ts-mux-%02d",
		 dev_p->tsmux_id);

	v4l2_set_subdevdata(subdev, dev_p);
	for (i = 0; i < TSMUX_NUM_OUTPUTS; i++) {
		dev_p->subdev_pad[i].flags = MEDIA_PAD_FL_SINK;
	}
	dev_p->subdev_pad[TSMUX_CAPTURE_PAD_ID].flags = MEDIA_PAD_FL_SOURCE;

	ret =
	    media_entity_init(me, TSMUX_NUM_OUTPUTS + 1, dev_p->subdev_pad, 0);
	if (ret < 0) {
		printk(KERN_ERR "%s: entity init failed(%d)\n", __func__, ret);
		return ret;
	}

	me->ops = &tsmux_subdev_media_ops;
	me->type = MEDIA_ENT_T_V4L2_SUBDEV_TSMUXER;

	ret = tsmux_context_init_ctrl(dev_p);
	if (ret) {
		printk(KERN_ERR "%s: tsmux ctrl init failed (%d)\n",
		       __func__, ret);
	}
	ret = stm_media_register_v4l2_subdev(subdev);
	if (ret < 0) {
		media_entity_cleanup(me);
		printk(KERN_ERR "%s: stm_media register failed (%d)\n",
		       __func__, ret);
		return ret;
	}

	return 0;
}

/**
 * tsmux_exit_subdev() - unregister the tsmux sub-devices
 */
static int tsmux_exit_subdev(struct tsmux_device *tsmux)
{
	struct v4l2_subdev *subdev = &tsmux->tsmux_subdev;
	struct media_entity *me = &subdev->entity;

	stm_media_unregister_v4l2_subdev(subdev);

	stm_v4l2_tsmux_ctrl_exit(tsmux);

	media_entity_cleanup(me);

	return 0;
}

/* ********************** */
/* Main Module Operations */
/* ********************** */

static int tsmux_create_instance(int inst)
{
	struct tsmux_device *dev;
	struct video_device *vfd;
	/*struct v4l2_ctrl_handler *hdl; */
	int ret, i;

	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (!dev)
		return -ENOMEM;

	dev->tsmux_id = inst;
	dev->users = 0;
	dev->output_pad_users = 0;
	memset(&dev->tsg_if, 0, sizeof(dev->tsg_if));
	memset(&dev->tsmux_if, 0, sizeof(dev->tsmux_if));

	tsmux_context_init(dev);

	/* initialize locks */
	spin_lock_init(&dev->slock);
	mutex_init(&dev->mutex);

	ret = -ENOMEM;
	vfd = video_device_alloc();
	if (!vfd)
		goto free_dev;

	*vfd = tsmux_template;

	/*
	 * Provide a mutex to v4l2 core. It will be used to protect
	 * all fops and v4l2 ioctls.
	 */
	vfd->lock = &dev->mutex;
	for (i = 0; i < TSMUX_NUM_OUTPUTS; i++) {
		dev->dev_pad[i].flags = MEDIA_PAD_FL_SOURCE;
	}

	dev->dev_pad[TSMUX_CAPTURE_PAD_ID].flags = MEDIA_PAD_FL_SINK;
	vfd->entity.ops = &tsmux_dev_media_ops;
	ret =
	    media_entity_init(&vfd->entity, TSMUX_NUM_OUTPUTS + 1, dev->dev_pad,
			      0);
	if (ret < 0)
		goto rel_vdev;

	snprintf(vfd->name, sizeof(vfd->name), "%s-%03d", TSMUX_MODULE_NAME,
		 inst);

	ret = stm_media_register_v4l2_video_device(vfd, VFL_TYPE_TSMUX, -1);
	if (ret < 0)
		goto entity_cleanup;

	video_set_drvdata(vfd, dev);

	/* Now that everything is fine, let's add it to device list */
	list_add(&dev->tsmux_devlist, &tsmux_devlist);

	dev->vfd = vfd;

	ret = tsmux_init_subdev(dev);
	if (ret)
		goto entity_cleanup;

	tsmux_create_dev_subdev_conns(dev);

	return 0;
entity_cleanup:
	media_entity_cleanup(&vfd->entity);
rel_vdev:
	video_device_release(vfd);
free_dev:
	kfree(dev);
	return ret;
}

static int __init stm_v4l2_tsmux_init(void)
{
	int ret = 0, i;
	stm_te_caps_t te_caps;

	ret = stm_te_get_capabilities(&te_caps);
	if (ret) {
		printk("Error in getting the capabilities from TE \n");
		return ret;
	}

	if (te_caps.max_tsmuxes == 0) {
		printk(KERN_ERR
		       "Wrong Capability returned from TransportEngine \n");
		return 0;
	}

	for (i = 0; i < te_caps.max_tsmuxes; i++) {
		ret = tsmux_create_instance(i);
		if (ret) {
			/* If some instantiations succeeded, keep driver */
			if (i)
				ret = 0;
			break;
		}
	}

	if (ret < 0) {
		pr_err("tsmux: error %d while loading driver\n", ret);
		return ret;
	}

	tsmux_create_vid_connection();
	tsmux_create_aud_connection();

	/* n_devs will reflect the actual number of allocated devices */
	tsmux_dev_nums = i;

	/*
	 * TSMUX is the last module to be loaded which is creating subdev.
	 * TSMUX will be having subdev nodes for tunneled encode->tsmux, so,
	 * creating subdev nodes here.
	 */
	ret = stm_media_register_v4l2_subdev_nodes();
	if (ret)
		pr_err("%s(): Failed to register subdev nodes\n", __func__);

	return ret;
}

/**
 * stm_v4l2_tsmux_exit() - module exit of tsmux
 */
static void __exit stm_v4l2_tsmux_exit(void)
{
	struct tsmux_device *tsmux;
	struct list_head *list1, *list2;

	list_for_each_safe(list1, list2, &tsmux_devlist) {

		list_del(list1);

		tsmux = list_entry(list1, struct tsmux_device, tsmux_devlist);

		tsmux_context_release(tsmux);

		tsmux_exit_subdev(tsmux);

		video_unregister_device(tsmux->vfd);

		media_entity_cleanup(&tsmux->vfd->entity);

		kfree(tsmux);
	}
}

module_init(stm_v4l2_tsmux_init);
module_exit(stm_v4l2_tsmux_exit);
MODULE_DESCRIPTION("STMicroelectronics V4L2 Tsmux device");
MODULE_AUTHOR("STMicroelectronics Limited");
MODULE_LICENSE("GPL");
