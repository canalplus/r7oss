/************************************************************************
Copyright (C) 2014 STMicroelectronics. All Rights Reserved.

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
 * Implementation of v4l2 audio decoder controls
************************************************************************/

#include <media/v4l2-ctrls.h>

#include "stm_se.h"

#include "stm_v4l2_decoder.h"
#include <linux/stm_v4l2_decoder_export.h>

#define MUTEX_INTERRUPTIBLE(mutex)		\
	if (mutex_lock_interruptible(mutex))	\
		return -ERESTARTSYS;

/**
 * auddec_g_volatile_ctrl() - get the control value
 */
static int auddec_g_volatile_ctrl(struct v4l2_ctrl *ctrl)
{
	int ret = 0;
	struct stm_v4l2_decoder_context *auddec_ctx;

	auddec_ctx = (struct stm_v4l2_decoder_context *)ctrl->priv;

	MUTEX_INTERRUPTIBLE(&auddec_ctx->mutex);

	if (!auddec_ctx->playback_ctx) {
		pr_err("%s(): audio decoder not setup for getting this control\n", __func__);
		ret = -EINVAL;
		goto g_ctrl_done;
	}

	switch (ctrl->id) {
	case V4L2_CID_MPEG_STI_HDMI_REPEATER_MODE:

		ret = stm_se_playback_get_control(auddec_ctx->playback_ctx->playback,
					STM_SE_CTRL_HDMI_RX_MODE,
					&ctrl->val);

		pr_debug("%s(): read the repeater mode = %d\n", __func__, ctrl->val);

		break;

	default:
		ret = -EINVAL;
	}

g_ctrl_done:
	mutex_unlock(&auddec_ctx->mutex);
	return ret;
}

/**
 * auddec_s_ctrl() - set control on auddec
 */
static int auddec_s_ctrl(struct v4l2_ctrl *ctrl)
{
	int ret = 0;

	struct stm_v4l2_decoder_context *auddec_ctx;

	auddec_ctx = (struct stm_v4l2_decoder_context *)ctrl->priv;

	MUTEX_INTERRUPTIBLE(&auddec_ctx->mutex);

	if (!auddec_ctx->playback_ctx) {
		pr_err("%s(): audio decoder not setup for setting this control\n", __func__);
		ret = -EINVAL;
		goto s_ctrl_done;
	}

	switch (ctrl->id) {
	case V4L2_CID_MPEG_STI_HDMI_REPEATER_MODE:

		ret = stm_se_playback_set_control(auddec_ctx->playback_ctx->playback,
					STM_SE_CTRL_HDMI_RX_MODE, ctrl->val);

		pr_debug("%s(): setting the repeater mode = %d\n", __func__, ctrl->val);
		break;

	default:
		ret = -EINVAL;
	}

s_ctrl_done:
	mutex_unlock(&auddec_ctx->mutex);
	return ret;
}

struct v4l2_ctrl_ops auddec_ctrl_ops = {
	.g_volatile_ctrl = auddec_g_volatile_ctrl,
	.s_ctrl = auddec_s_ctrl,
};

/**
 * stm_v4l2_auddec_ctrl_init() - Initialize audio decoder control
 */
int stm_v4l2_auddec_ctrl_init(struct v4l2_ctrl_handler *ctrl_handler, void *priv)
{
	int ret = 0, i;

	/*
	 * Intialize the v4l2 controls
	 */
	static const struct v4l2_ctrl_config auddec_ctrl_config[] = {
		{
		.ops = &auddec_ctrl_ops,
		.id = V4L2_CID_MPEG_STI_HDMI_REPEATER_MODE,
		.name = "Audio Decoder Repeater Mode",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.min = V4L2_CID_MPEG_STI_HDMIRX_DISABLED,
		.max = V4L2_CID_MPEG_STI_HDMIRX_NON_REPEATER,
		.step = 1,
		.def = V4L2_CID_MPEG_STI_HDMIRX_DISABLED,
		.flags = V4L2_CTRL_FLAG_VOLATILE,
		},
	};

	/*
	 * Register the subdev control handler with the core
	 */
	ret = v4l2_ctrl_handler_init(ctrl_handler,
				ARRAY_SIZE(auddec_ctrl_config));
	if (ret) {
		pr_err("%s(): failed to init audio control handler\n", __func__);
		goto ctrl_init_failed;
	}

	for (i = 0; i < ARRAY_SIZE(auddec_ctrl_config); i++) {
		v4l2_ctrl_new_custom(ctrl_handler, &auddec_ctrl_config[i], priv);
		if (ctrl_handler->error) {
			pr_err("%s(): adding auddec controls failed\n", __func__);
			ret = ctrl_handler->error;
			break;
		}
	}

	if (ret)
		v4l2_ctrl_handler_free(ctrl_handler);

ctrl_init_failed:
	return ret;
}

/**
 * stm_v4l2_auddec_ctrl_exit() - Frees the audio decoder controls
 */
void stm_v4l2_auddec_ctrl_exit(struct v4l2_ctrl_handler *ctrl_handler)
{
	v4l2_ctrl_handler_free(ctrl_handler);
}
