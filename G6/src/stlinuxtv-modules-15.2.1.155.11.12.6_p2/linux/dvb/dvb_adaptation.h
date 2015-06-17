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

Source file name : dvb_adaptation.h
Author :           Rahul.V

Header for STLinuxTV package

Date        Modification                                    Name
----        ------------                                    --------
16-Jun-11   Created                                         Rahul.V

 ************************************************************************/

#ifndef _DVB_ADAPTATION_H
#define _DVB_ADAPTATION_H

#include <linux/rwsem.h>

#include <stm_registry.h>
#include <stm_fe.h>
#include <stm_te.h>
#ifdef CONFIG_STLINUXTV_CRYPTO
#include <stm_ce.h>
#include <linux/dvb/ca.h>
#include <linux/dvb/stm_ca.h>
#endif
#include <dvb_demux.h>
#include <dmxdev.h>
#include <linux/list.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/platform_device.h>
#include <linux/stm/plat_dev.h>
#include <linux/i2c.h>
#include <linux/kthread.h>
#include <linux/stm/demod.h>
#include <linux/stm/lnb.h>
#include <linux/stm/ip.h>
#include <linux/stm/ipfec.h>
#include <linux/stm/diseqc.h>
#include <linux/stm/rf_matrix.h>

#include <linux/platform_device.h>
#include <dvb_frontend.h>

#include <stm_memsink.h>
#include <stm_memsrc.h>

#ifdef CONFIG_STLINUXTV_DECODE_DISPLAY
#include "dvb_module.h"
#endif

#define MAX_NAME_LEN  (31)
#define MAX_NAME_LEN_SIZE (MAX_NAME_LEN + 1)

struct stm_dvb_demod_s {
	struct list_head list;
	char dvb_demod_name[MAX_NAME_LEN_SIZE];
	uint32_t demod_id;
	struct demod_config_s *demod_config;
	struct i2c_adapter *demod_i2c;
	struct i2c_adapter *tuner_i2c;
	struct dvb_frontend *demod;
	void *dvb_demod_config;
	void *dvb_tuner_config;
	stm_fe_demod_h demod_object;
	bool ctrl_via_stm_fe;
};

struct stm_dvb_lnb_s {
	struct list_head list;
	char dvb_lnb_name[MAX_NAME_LEN_SIZE];
	uint32_t lnb_id;
	struct lnb_config_s *lnb_config;
	struct i2c_adapter *i2c;
	struct stm_dvb_demod_s *dvb_demod;
	stm_fe_lnb_h lnb_object;
	bool ctrl_via_stm_fe;
};

struct stm_dvb_diseqc_s {
	struct list_head list;
	char dvb_diseqc_name[MAX_NAME_LEN_SIZE];
	uint32_t diseqc_id;
	struct diseqc_config_s *diseqc_config;
	struct i2c_adapter *i2c;
	struct stm_dvb_demod_s *dvb_demod;
	stm_fe_diseqc_h diseqc_object;
	bool ctrl_via_stm_fe;
};

struct stm_dvb_rf_matrix_s {
	struct list_head list;
	char dvb_rf_matrix_name[MAX_NAME_LEN_SIZE];
	uint32_t id;
	struct rf_matrix_config_s *config;
	struct i2c_adapter *i2c;
	struct stm_dvb_demod_s *dvb_demod;
	stm_fe_rf_matrix_h rf_matrix_object;
	bool ctrl_via_stm_fe;
};

#ifdef CONFIG_STLINUXTV_CRYPTO
struct stm_dvb_ca_channel_s {
	struct list_head list;
	unsigned int index;
#define CA_MODE_PACKET	(1 << 1)
#define CA_MODE_BLOCK	(1 << 2)
#define CA_MODE_MASK	(CA_MODE_BLOCK | CA_MODE_PACKET)
#define CA_MODE_DIR_MASK	(1 << 0)
#define CA_MODE_DESCR	(0 << 0)
#define CA_MODE_SCR	(1 << 0)
	unsigned int mode;
	stm_ce_handle_t transform;
	struct list_head pid_list;
};

struct stm_dvb_ca_s {
	struct list_head list;
	uint32_t ca_id;
	struct dvb_device *cadev;
	stm_ce_handle_t session;
	struct list_head channel_list;
	struct stm_dvb_demux_s *demux;
	char *profile;
};
#endif

struct stm_dvb_ip_s {
	struct list_head list;
	char dvb_ip_name[MAX_NAME_LEN_SIZE];
	uint32_t ip_id;
	struct ip_config_s *ip_config;
	struct dvb_ipfe *ip;
	struct stm_dvb_ip_s *dvb_ip;
	stm_fe_ip_h ip_object;
};

struct stm_dvb_ipfec_s {
	struct list_head list;
	char dvb_ipfec_name[MAX_NAME_LEN_SIZE];
	uint32_t ipfec_id;
	struct ipfec_config_s *ipfec_config;
	struct dvb_ipfe *ip;
	struct stm_dvb_ip_s *dvb_ip;
	stm_fe_ip_fec_h ipfec_object;
};

struct stm_dvb_demux_s {
	struct list_head list;
	char dvb_demux_name[MAX_NAME_LEN];
	uint32_t demux_id;
	dmx_pes_type_t pcr_type;
#define DEMUX_SOURCE_FE	0
#define DEMUX_SOURCE_DVR 1
	uint32_t demux_source;
	uint32_t demux_default_source;
	uint32_t demod_id;
	stm_te_object_h demux_object;
	stm_te_object_h pacing_object;
	struct dvb_demux dvb_demux;
	struct dmxdev dmxdev;
	struct dmx_frontend hw_frontend;
	struct list_head pes_filter_handle_list;
	struct list_head sec_filter_handle_list;
	struct list_head pcr_filter_handle_list;
	struct list_head ts_filter_handle_list;
	struct list_head index_filter_handle_list;
	struct list_head pid_filter_handle_list;
	uint32_t filter_count;
	stm_memsrc_h memsrc_input;
	bool demux_running;
	struct rw_semaphore rw_sem;
	struct task_struct *read_task;
	struct dmx_frontend *frontend;
	struct stm_dvb_demod_s *demod;
	struct stm_dvb_ip_s *ip;
	struct file_operations dvr_ops;
	struct file_operations dmx_ops;
#ifdef CONFIG_STLINUXTV_CRYPTO
	struct stm_dvb_ca_s *ca;
	/* ca_channel used for handling injection into the demux */
	struct stm_dvb_ca_channel_s *ca_channel;
	struct list_head ca_pid_list;
#endif
	int32_t users;
#define DVR_BUF_SIZE	196608	/* TODO: To be tuned */
	char *dvr_buf;
	struct DvbContext_s *DvbContext;
	struct PlaybackDeviceContext_s *PlaybackContext;
};

#endif /* _DVB_ADAPTATION_H */
