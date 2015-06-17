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

Source file name : stm_fe_demod.h
Author :           Rahul.V

Header for stm_fe_demod.c

Date        Modification                                    Name
----        ------------                                    --------
16-Jun-11   Created                                         Rahul.V

************************************************************************/
#ifndef _STM_FE_DEMOD_H
#define _STM_FE_DEMOD_H

#include <stm_te_if_tsinhw.h>
#define STM_FE_NO_OPTIONS_DEFAULT -1
#define STM_FE_USE_LLA_DRIVERS     1
#define STM_FE_BYPASS_LLA_DRIVERS  0
#define IS_NAME_MATCH(_x, _name) (!strncmp((_x), (_name), strlen(_name)))

#define DEMOD_ACQUISITION_RETRIAL_PERIOD_MS 100
#define DEMOD_RELOCK_MONITOR_PERIOD_MS      100
#define DEMOD_SIGNAL_STATUS_TRACK_PERIOD_MS 200
#define DEMOD_IDLE_PERIOD_MS                20000
#define DEMOD_PARAM_UPDATE_PERIOD_MS        800
#define DEMOD_WAKE_UP                       0
/* stat attributes structure */
struct stat_attrs_s {
	unsigned long last_acq_time_ticks;
};


/* Private data common to all demodulation objects */
struct stm_fe_demod_obj_s {
	void *priv;
};

enum stm_fe_taskstate_e {
	FE_TASKSTATE_IDLE,
	FE_TASKSTATE_TUNE,
	FE_TASKSTATE_SCAN,
	FE_TASKSTATE_TRACKING
};

struct stm_fe_task_s {
	enum stm_fe_taskstate_e nextstate;	/*holds the next state
						 *of state machine*/
	enum stm_fe_taskstate_e prevstate;	/*holds the previous state
						 *of state machine*/
	fe_thread thread;
	/*For Synchronising task & Application */
	fe_semaphore *guard_sem;
	uint32_t delay_in_ms;
	uint32_t track_cnt;
	uint32_t lockcount;
	stm_fe_demod_event_t event;
	wait_queue_head_t waitq;
};

struct stm_fe_remote_s {
	uint32_t chnl_id;
	struct socket *sock;
	fe_thread remote_thread;
	struct kfifo remote_fifo;
	wait_queue_head_t remote_waitq;
	uint16_t txn_id;
	uint32_t flag;
};

struct fe_scan_config {
	uint32_t	search_freq;
	uint32_t	end_freq;
	uint32_t	sr_min;
	uint32_t	sr_act;
	uint32_t	sr_table_size;
	uint32_t	sr_cnt;
	uint32_t	step_size;
	uint32_t	scan_table_cnt;
	uint32_t	table_size;
	uint32_t	*scan_freq;
	uint32_t	ter_step;
};

struct stm_fe_demod_s;

struct demod_ops_s {

	int (*init) (struct stm_fe_demod_s *priv);
	int (*tune) (struct stm_fe_demod_s *priv, bool *lock);
	int (*status) (struct stm_fe_demod_s *priv, bool *lock);
	int (*tracking) (struct stm_fe_demod_s *priv);
	int (*unlock) (struct stm_fe_demod_s *priv);
	int (*term) (struct stm_fe_demod_s *priv);
	int (*abort) (struct stm_fe_demod_s *priv, bool abort);
	int (*detach) (struct stm_fe_demod_s *priv);
	int (*scan) (struct stm_fe_demod_s *priv, bool *lock);
	int (*standby) (struct stm_fe_demod_s *priv, bool standby);
	int (*restore) (struct stm_fe_demod_s *priv);
	int (*tsclient_init) (struct stm_fe_demod_s *priv, char *demux_name);
	int (*tsclient_term) (struct stm_fe_demod_s *priv, char *demux_name);
	int (*tsclient_pid_configure) (stm_object_h priv,
				stm_object_h demux_object, uint32_t pid);
	int (*tsclient_pid_deconfigure) (stm_object_h priv,
				stm_object_h demux_object, uint32_t pid);

};
struct stm_fe_demod_s {
	struct list_head list;
	struct list_head *list_h;
	char demod_name[32];
	uint32_t demod_id;
	struct dentry *demod_dump;
	stm_fe_lnb_h lnb_h;
	stm_fe_diseqc_h diseqc_h;
	stm_fe_rf_matrix_h rf_matrix_h;
	uint32_t rf_src_selected;
	struct demod_config_s *config;
	struct stm_fe_task_s fe_task;
	stm_fe_demod_info_t c_tinfo;
	stm_fe_demod_status_t signal_status;
	stm_fe_demod_tune_params_t tp;
	stm_fe_demod_scan_configure_t scanparams;
	stm_fe_demod_context_t context;
	stm_fe_demod_info_t info;
	void *demod_params;
	void *add_demod_params;
	fe_i2c_adapter demod_i2c;
	STCHIP_Handle_t demod_h;
	STCHIP_Handle_t tuner_h;
	struct demod_ops_s *ops;
	bool event_disable;
	bool bypass_control;
	stm_fe_demod_scan_context_t scan_context;
	bool tune_enable;
	bool scan_enable;
	bool rpm_suspended;
	struct fe_scan_config fe_scan;
	struct stat_attrs_s stat_attrs;
	struct platform_device *demod_data;
	struct stm_fe_remote_s fe_remote;
	char grp_name[60];
	bool dt_enable;
	bool first_tunerinfo;
};
int stm_fe_demod_probe(struct platform_device *pdev);
int stm_fe_demod_remove(struct platform_device *pdev);
void stm_fe_demod_shutdown(struct platform_device *pdev);

#ifdef CONFIG_PM
int32_t stm_fe_demod_suspend(struct device *dev);
int32_t stm_fe_demod_resume(struct device *dev);
int32_t stm_fe_demod_freeze(struct device *dev);
int32_t stm_fe_demod_restore(struct device *dev);
#ifdef CONFIG_PM_RUNTIME
int32_t stm_fe_demod_rt_suspend(struct device *dev);
int32_t stm_fe_demod_rt_resume(struct device *dev);
#endif
#endif

struct stm_fe_demod_s *demod_from_name(const char *name);
struct stm_fe_demod_s *demod_from_id(uint32_t id);
int demod_task_open(struct stm_fe_demod_s *priv);
int demod_task_close(struct stm_fe_demod_s *priv);
#endif /* _STM_FE_DEMOD_H */
