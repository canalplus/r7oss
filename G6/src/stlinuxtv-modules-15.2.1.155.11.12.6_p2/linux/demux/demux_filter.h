/************************************************************************
Copyright (C) 2011 STMicroelectronics. All Rights Reserved.

This file is part of the STLinuxTV Library.

STLinuxTV is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License version 2 as published by the
Free Software Foundation.

STLinuxTV is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with STLinuxTV; see the file COPYING.  If not, write to the Free Software
Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

The STLinuxTV Library may alternatively be licensed under a proprietary
license from ST.

Source file name : demux_filter.h

Filters/Chains handling functions header

Date        Modification                                    Name
----        ------------                                    --------

 ************************************************************************/

#ifndef _DEMUX_FILTER_H
#define _DEMUX_FILTER_H

#include <dvb/dvb_adaptation.h>
#include <linux/dvb/stm_dmx.h>

enum map_e {
	SECTION_MAP = 0,
	TS_MAP,
	PES_MAP,
	PCR_MAP,
	INDEX_MAP,
	MAX_MAP
};

enum chain_type {
	PES_CHAIN = 0,
	PCR_CHAIN,
	SECTION_CHAIN,
	INDEX_CHAIN
};

enum input_filter_type {
	INPUT_PID_FILTER = 0,
	INSERT_PID_FILTER,
	REPLACE_PID_FILTER
};

struct stm_dvb_filter_s {
	stm_te_object_h handle;
#ifdef CONFIG_STLINUXTV_CRYPTO
	struct stm_dvb_ca_channel_s *ca_channel;
#endif
	int users;
	stm_memsink_h memsink_interface;
	stm_event_subscription_h evt_subs;
	stm_event_subscription_entry_t *evt_subs_entry;
	stm_object_h decoder;
	wait_queue_head_t wait;

	/* The last 3 fields are used to buffer data from the TE in some cases.
	 * This is necessary in case of index, since otherwise we can't know properly how much data to received
	 */
	char *buf_data;
	unsigned int buf_offset;
	unsigned int buf_len;
};

struct stm_dvb_filter_chain_s {
	struct list_head input_list;
	struct list_head output_list;
	struct stm_dvb_filter_s *input_filter;
	struct stm_dvb_filter_s *output_filter;
	int pid;
#define CHAIN_STOPPED	0
#define CHAIN_STARTED	1
	int state;
	int type;
	void *filter;
};

#ifdef CONFIG_STLINUXTV_CRYPTO
struct stm_dvb_ca_pid_s {
	struct list_head ca_pid_list;
	struct stm_dvb_ca_s *stm_ca;
	ca_pid_t ca_pid;
};

int stm_dvb_demux_add_ca_pid(struct stm_dvb_ca_s *stm_ca, ca_pid_t *ca_pid);
int stm_dvb_demux_ca_set_pid(struct stm_dvb_demux_s *stm_demux, unsigned int pid);
#endif

void init_filter(struct stm_dvb_filter_s *filter);

struct stm_dvb_filter_chain_s *add_chain_to_demux(struct stm_dvb_demux_s *demux,
						  struct stm_dvb_filter_s
						  *input,
						  struct stm_dvb_filter_s
						  *output, unsigned int pid,
						  enum map_e, void *filter,
						  enum input_filter_type filter_type);
int attach_sink_to_chain(struct stm_dvb_filter_chain_s *chain,
			 stm_object_h sink, char *memsink_name,
			 stm_data_mem_type_t address);

int stm_dmx_delete_chain(struct stm_dvb_filter_chain_s *chain);
int del_all_chains_from_demux(struct stm_dvb_demux_s *demux);
int del_filters_from_chain(struct stm_dvb_filter_chain_s *chain);

struct stm_dvb_filter_chain_s *get_primary_ts_chain(struct stm_dvb_demux_s
						    *demux);
struct stm_dvb_filter_s *get_pid_filter(struct stm_dvb_demux_s *demux, int pid);
struct stm_dvb_filter_chain_s *get_primary_ts_chain(struct stm_dvb_demux_s
						    *demux);
struct stm_dvb_filter_chain_s *match_filter_input_pid(struct stm_dvb_demux_s
						      *demux, int pid);
struct stm_dvb_filter_chain_s *match_filter_output_pid(struct stm_dvb_demux_s
						       *demux, int pid,
						       enum map_e);
struct stm_dvb_filter_chain_s *match_chain_from_filter(struct stm_dvb_demux_s
						       *demux, struct dmxdev_filter
						       *filter);
struct stm_dvb_filter_chain_s *match_chain_from_decoder(struct
							     stm_dvb_demux_s
							     *demux,
							     stm_object_h
							     decoder);
struct stm_dvb_filter_s *get_filter_pacing_output(struct stm_dvb_demux_s *demux);

struct stm_dvb_filter_chain_s *demux_get_pcr_by_type(struct stm_dvb_demux_s *stm_demux,
								dmx_pes_type_t pcr_type);
#if defined CONFIG_STLINUXTV_FRONTEND_DEMUX
extern int stm_dvb_detach_stream_from_demux(void *stm_demux,
			stm_object_h demux_filter, stm_object_h decoder);
extern int stm_dvb_attach_stream_to_demux(void *stm_demux,
			stm_object_h demux_filter, stm_object_h decoder);
#else
/*
 * These function are intended for hw demux, so, do nothing
 * for sw demux, but allow the calls to complete successfully
 */
static inline int stm_dvb_detach_stream_from_demux(void *stm_demux,
				stm_object_h demux_filter, stm_object_h decoder)
{
	return 0;
}
static inline int stm_dvb_attach_stream_to_demux(void *stm_demux,
				stm_object_h demux_filter, stm_object_h decoder)
{
	return 0;
}
#endif

int stm_dvb_chain_start( struct stm_dvb_filter_chain_s *chain );
int stm_dvb_chain_stop( struct stm_dvb_filter_chain_s *chain );
int stm_dvb_demux_get_control(struct stm_dvb_demux_s *stm_demux,
                struct dmxdev_filter *dmxdevfilter, struct dmx_ctrl *ctrl);

#define ALLOC_FILTER(__p, _error_) 						\
	do {									\
		__p = kzalloc(sizeof(struct stm_dvb_filter_s), GFP_KERNEL); 	\
                if (!(__p)) { 							\
			result = -ENOMEM;					\
			goto _error_;						\
		}								\
		init_waitqueue_head(&__p->wait);				\
	} while (0)

#endif /* _DEMUX_FILTER_H */
