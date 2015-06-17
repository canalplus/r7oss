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
#ifndef STM_V4L2_TSMUX_H
#define STM_V4L2_TSMUX_H

#include <linux/videodev2.h>
#include "stm_te.h"
#include "stm_te_if_tsmux.h"
#include <media/v4l2-ctrls.h>

/* Max length of a name */
#define TSMUX_MAX_NAME_SIZE 32

/* NOTE: Number of tsmux inputs per tsmux must be power of 2 */
#define TSMUX_NUM_OUTPUTS 8

#define STM_V4L2_TSMUX_CONNECT_NONE   0
#define STM_V4L2_TSMUX_CONNECT_INJECT 1
#define STM_V4L2_TSMUX_CONNECT_ENCODE 2

struct tsmux_buffer {
	struct vb2_buffer vb;
	struct list_head list;	/* List of buffers for tsmux */
};

static LIST_HEAD(tsmux_devlist);

struct tsmux_dmaqueue {
	struct list_head active;

	/* thread for pushing buffers to transport engine */
	struct task_struct *kthread;
	wait_queue_head_t wq;
};

enum tsmux_type {
	TSMUX_INPUT,
	TSMUX_OUTPUT,
};

struct tsmux_param_fmt {
	u32 type;		/* buffer type */
	u32 dvb_data_type;	/* data type */
	u32 sizebuf;		/* buffer size */
};

struct tsmux_context {
	struct tsmux_device *dev;
	char name[TSMUX_MAX_NAME_SIZE];
	enum tsmux_type type;
	/* TODO Use a union for the fields for the 2 types of ctx */
	stm_te_object_h tsg_object;
	bool tsg_connected;
	stm_te_object_h tsmux_object;
	bool tsmux_started;
	stm_memsink_h memsink_object;
	stm_te_object_h tsg_index_object;
	stm_memsink_h memsink_index_object;
	int connected_to_index_filter;
	unsigned int section_table_period;

	struct tsmux_fmt *fmt;
	struct vb2_queue vb_vidq;
	unsigned int bufcount;
	int users;

	spinlock_t slock;
	struct mutex mutex;
	struct v4l2_ctrl_handler ctx_ctrl_handler;

	struct tsmux_param_fmt src_fmt;
	struct tsmux_param_fmt dst_fmt;

	int   src_connect_type;
	void* connection_src;

	struct tsmux_dmaqueue vidq;
};

struct tsmux_device {
	struct list_head tsmux_devlist;
	int users;
	int output_pad_users;

	spinlock_t slock;
	struct mutex mutex;	/* tsmux device lock */

	struct video_device *vfd;

	struct media_pad dev_pad[TSMUX_NUM_OUTPUTS + 1];
	/* TODO seperate the device and subdev struct */
	struct v4l2_subdev tsmux_subdev;
	struct media_pad subdev_pad[TSMUX_NUM_OUTPUTS + 1];

	int tsmux_id;
	struct tsmux_context tsmux_in_ctx;
	struct tsmux_context tsmux_out_ctx[TSMUX_NUM_OUTPUTS];

	stm_te_async_data_interface_sink_t tsg_if;
	stm_data_interface_pull_src_t tsmux_if;
};

struct stm_tsmux_vfh {
	struct v4l2_fh fh;
	struct tsmux_context *ctx;
};

#define file_to_tsmux_ctx(_e) \
	((struct stm_tsmux_vfh*)container_of(_e->private_data, struct stm_tsmux_vfh, fh))->ctx;

#endif /* STM_V4L2_TSMUX_H */
