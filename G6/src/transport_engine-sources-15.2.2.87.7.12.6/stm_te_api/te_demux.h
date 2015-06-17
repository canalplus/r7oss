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

Source file name : te_demux.h

Declares TE demux type and operators
******************************************************************************/
#ifndef __TE_DEMUX_H
#define __TE_DEMUX_H

#include <linux/list.h>
#include <linux/workqueue.h>
#include "te_object.h"
#include "te_hal_obj.h"
#include "te_buffer_work.h"
#include "te_input_filter.h"

typedef struct {
	unsigned long  inject_input_latency ;
	unsigned long  inject_time_tp;
	 /* not used now as TP injection time and input push latency
	  * are sufficient . Can be used or updated in future */
	unsigned long  inject_pacing_time;
	unsigned long  inject_push_sink_time;
} te_demux_inject_time_stats_t;

struct te_demux {
	/* Demux input parameters */
	uint32_t stream_id;
	uint32_t ts_tag;
	uint32_t input_type;
	uint32_t discard_dupe_ts;

	/* Child filters */
	struct list_head in_filters;
	struct list_head out_filters;

	rwlock_t out_flt_rw_lock;
	struct mutex of_sec_lock;

	/* Linked objects */
	struct list_head autotargets;
	struct list_head linked_pids;

	/* Output filter used to "pace" injected data */
	struct te_obj *pacing_filter;

	/* HAL resources */
	struct te_hal_obj *hal_vdevice;
	struct te_hal_obj *hal_session;
	struct te_hal_obj *hal_signal;
	struct te_hal_obj *hal_injector;

	/* Injection analysis buffer */
	uint8_t *analysis;
	size_t   analysis_length;

	/* Upstream connected object */
	stm_object_h upstream_obj;
	stm_data_interface_push_notify_t push_notify;
	stm_data_interface_push_sink_t sink_interface;

	/* Upstream disconnection pending */
	bool disconnect_pending;


	/* Statistics */
	stm_te_demux_stats_t stat_upon_reset;
	struct device sysfs_dev;

	/*inject time*/
	ktime_t last_push ;
	te_demux_inject_time_stats_t push_time_stats;

	struct workqueue_struct *work_que;

	struct te_buffer_work_queue *bwq;

	/* remote pid filtering functions */
	int (*stm_fe_bc_pid_set)(stm_object_h demod_object,
			stm_object_h demux_object, uint32_t pid);
	int (*stm_fe_bc_pid_clear)(stm_object_h demod_object,
			stm_object_h demux_object, uint32_t pid);

	/* strelay index */
	unsigned int injector_relayfs_index;

	/* Upstream disconnection pending */
	bool use_timer_tag;

	struct te_obj obj;
};

struct te_autotarget {
	struct list_head entry;
	stm_te_pid_t pid;
	stm_te_object_h target;
};

struct te_linkedpid {
	struct list_head entry;
	stm_te_pid_t p_pid;
	stm_te_pid_t s_pid;
	stm_te_secondary_pid_mode_t mode;
	struct te_obj *p_filter;
	struct te_obj *s_filter;
};

struct te_demux *te_demux_from_obj(struct te_obj *obj);

int te_demux_new(char *name, struct te_obj **new_demux);
int te_demux_start(struct te_obj *obj);
int te_demux_stop(struct te_obj *obj);

int te_demux_get_device_time(struct te_obj *demux, uint64_t *dev_time,
		uint64_t *sys_time);

int te_demux_register_pid_autotarget(stm_te_object_h demux, stm_te_pid_t pid,
					stm_te_object_h target);

int te_demux_unregister_pid_autotarget(stm_te_object_h demux, stm_te_pid_t pid);

int te_demux_pid_announce(struct te_in_filter *input);
int te_demux_pid_announce_change(struct te_in_filter *input,
		stm_te_pid_t old_pid, stm_te_pid_t new_pid);

int te_demux_reset_stat(struct te_obj *obj);

int te_demux_link_secondary_pid(stm_te_object_h dmx_h, stm_te_pid_t pri,
				stm_te_pid_t sec,
				stm_te_secondary_pid_mode_t mode);

int te_demux_unlink_secondary_pid(stm_te_object_h dmx_h, stm_te_pid_t pid);
#endif
