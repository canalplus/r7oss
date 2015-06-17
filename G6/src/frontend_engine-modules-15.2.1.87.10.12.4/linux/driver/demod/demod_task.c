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

Source file name : demod_task.c
Author :           shobhit

state mechine implementation of demod

Date        Modification                                    Name
----        ------------                                    --------
25-Jun-11   Created                                         shobhit

************************************************************************/

#include <linux/kernel.h>
#include <linux/kthread.h>
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
#include <linux/module.h>
#include <linux/sched.h>

#define NUMBER_LOST_BEFORE_UNLOCK	3

static int thread_inp_fe_demod[2] = {SCHED_NORMAL, 0};
module_param_array_named(thread_INP_FE_Demod, thread_inp_fe_demod, int, NULL,
									0644);
MODULE_PARM_DESC(thread_INP_FE_Demod, "INP-FE-Demod thread: s(Mode),p(Priority)");

static int demod_thread(void *data);

static void demod_state_idle(struct stm_fe_demod_s *priv,
		      stm_fe_demod_event_t *event);
static void demod_state_tune(struct stm_fe_demod_s *priv,
		      stm_fe_demod_event_t *event);
static void demod_state_scan(struct stm_fe_demod_s *priv,
		      stm_fe_demod_event_t *event);
static void demod_state_tracking(struct stm_fe_demod_s *priv,
			  stm_fe_demod_event_t *event);
static void check_lnb_config(struct stm_fe_demod_s *priv,
		      stm_fe_demod_event_t *event);

static void (*stm_fe_states_table[])(struct stm_fe_demod_s *priv,
				      stm_fe_demod_event_t *event) = {
			demod_state_idle,
			demod_state_tune,
			demod_state_scan,
			demod_state_tracking
};

int evt_notify(struct stm_fe_demod_s *priv, stm_fe_demod_event_t *fe_event)
{
	int ret = 0;
	stm_event_t event;
	if (priv->event_disable == false) {
		event.event_id = *fe_event;
		event.object = (stm_fe_demod_h)priv;
		ret = stm_event_signal(&event);
		priv->event_disable = true;
	}
	return ret;
}

/*
 * Name: demod_task_open()
 *
 * Description:creating a task to handle demod state machine
 */
int demod_task_open(struct stm_fe_demod_s *priv)
{
	int ret = -1;
	char task_name[20];
	struct stm_fe_task_s *task;

	if (!priv) {
		pr_err("%s: demod obj not initialised\n", __func__);
		return -EINVAL;
	}

	task = &priv->fe_task;
	task->guard_sem = (fe_semaphore *)stm_fe_malloc(sizeof(fe_semaphore));
	if (!task->guard_sem) {
		pr_err("%s: mem alloc failed for semaphore\n", __func__);
		return -ENOMEM;
	}

	/*semaphore to synchronize access to tuning status : initial count 1 */
	ret = stm_fe_sema_init(task->guard_sem, 1);
	if (ret) {
		stm_fe_free((void **)&task->guard_sem);
		pr_err("%s: semaphore init failed: guard_sem\n", __func__);
		return ret;
	}
	task->thread = NULL;
	/* fetask awakes to perform background processing */
	task->delay_in_ms = DEMOD_IDLE_PERIOD_MS;
	snprintf(task_name, sizeof(task_name), "INP-FE-Demod%d",
								priv->demod_id);
	ret = stm_fe_thread_create(&task->thread, demod_thread, (void *)priv,
						task_name, thread_inp_fe_demod);

	if (ret) {
		stm_fe_free((void **)&task->guard_sem);
		pr_err("%s: fe task creation failed\n", __func__);
	}

	return ret;
}

/*
 * Name: demod_task_close()
 *
 * Description:Termination of task and semaphores
 */

int demod_task_close(struct stm_fe_demod_s *priv)
{

	int ret = 0, err = 0;
	struct stm_fe_task_s *task;

	if (!priv) {
		pr_err("%s: demod obj not initialised\n", __func__);
		return -EINVAL;
	}

	task = &priv->fe_task;

/*correction to avoid state machine deadlock
			if semaphore is already acquired by demod_suspend*/

	if (priv->rpm_suspended == true) {
		task->delay_in_ms = DEMOD_WAKE_UP;
		fe_wake_up_interruptible(&task->waitq);
		task->nextstate = FE_TASKSTATE_IDLE;
		stm_fe_sema_signal(task->guard_sem);
		goto skip_sem;
	}

	/* wait until sattask is out of its critical section */
	ret = stm_fe_sema_wait(task->guard_sem);
	if (ret)
		pr_err("%s: stm_fe_sema_wait failed\n", __func__);
	/* Wakeup the task (fallthrough early) */
	task->delay_in_ms = DEMOD_WAKE_UP;
	fe_wake_up_interruptible(&task->waitq);
	task->nextstate = FE_TASKSTATE_IDLE;
	stm_fe_sema_signal(task->guard_sem);

skip_sem:
	/* stop the locking algo */
	if (priv->ops->unlock) {
		ret = priv->ops->unlock(priv);
		if (ret) {
			pr_err("%s: demod unlock failed: %d\n", __func__, ret);
			err = ret;
		}
	} else {
		pr_err("%s: FP demod_handle->ops->unlock is NULL\n", __func__);
		err = -EFAULT;
	}

	/*stop a thread created by kthread_create() */
	ret = stm_fe_thread_stop(&task->thread);

	if (ret) {
		pr_info("%s: stm_fe_thread_stop failed\n", __func__);
		err = ret;
	}

	stm_fe_free((void **)&priv->fe_task.guard_sem);

	return err;
}

/*
 * Name: demod_thread()
 *
 * Description:starting of the demod state machine
 */
static int demod_thread(void *data)
{
	int ret;
	struct stm_fe_demod_s *priv;
	stm_fe_demod_event_t event;
	struct stm_fe_task_s *task;
	if (!data) {
		pr_err("%s: data ptr is NULL\n", __func__);
		return -EFAULT;
	}
	priv = data;
	task = &priv->fe_task;
	task->nextstate = FE_TASKSTATE_IDLE;
	fe_init_waitqueue_head(&task->waitq);

	while (!stm_fe_thread_should_stop()) {
		ret = stm_fe_sema_wait(task->guard_sem);
		if (ret)
			pr_err("%s: sema_wait: guard_sem= %d\n", __func__, ret);

		if (!priv) {
			pr_err("%s: ERROR: demod priv is NULL\n", __func__);
			stm_fe_sema_signal(task->guard_sem);
			continue;
		}
		stm_fe_states_table[task->nextstate](priv, &event);
		stm_fe_sema_signal(task->guard_sem);
		fe_wait_event_interruptible_timeout(task->waitq,
		       !task->delay_in_ms, msecs_to_jiffies(task->delay_in_ms));

	}

	return 0;
}

/*
 * Name: demod_state_idle()
 * Description: Describes the task behaviour in IDLE state of demod
 * state machine
 * Next State:FE_TASKSTATE_SCANNING,FE_TASKSTATE_TUNE
 * Previous State:FE_TASKSTATE_IDLE,FE_TASKSTATE_TUNE,FE_TASKSTATE_SCANNING
 */
static
void demod_state_idle(struct stm_fe_demod_s *priv, stm_fe_demod_event_t *event)
{
	priv->fe_task.prevstate = FE_TASKSTATE_IDLE;
}

/*
 * Name: demod_state_tune()
 * Description: Describes the task behaviour in TUNE state of
 * demod state machine
 * Next State:FE_TASKSTATE_TRACKING,FE_TASKSTATE_TUNE
 * Previous State:FE_TASKSTATE_IDLE,FE_TASKSTATE_TRACKING
 */
static
void demod_state_tune(struct stm_fe_demod_s *priv, stm_fe_demod_event_t *event)
{
	int ret;
	bool lock = false;
	unsigned long t1, t2;
	struct stm_fe_task_s *task = &priv->fe_task;
	priv->info.context = STM_FE_DEMOD_TUNE;
	task->prevstate = FE_TASKSTATE_TUNE;

	t1 = stm_fe_time_now();

	if (!priv->ops->tune) {
		pr_err("%s: FP priv->ops->tune is NULL\n", __func__);
	} else {
		ret = priv->ops->tune(priv, &lock);
/*		if (ret)
			pr_info("%s: tune failed\n", __func__);*/
	}
	t2 = stm_fe_time_now();
	priv->stat_attrs.last_acq_time_ticks = t2 - t1;

	if (lock) {
		*event = STM_FE_DEMOD_EVENT_TUNE_LOCKED;
		task->nextstate = FE_TASKSTATE_TRACKING;
		priv->signal_status = STM_FE_DEMOD_SYNC_OK;
		task->lockcount = NUMBER_LOST_BEFORE_UNLOCK;
		task->delay_in_ms = DEMOD_WAKE_UP;
		priv->event_disable = false;
		task->track_cnt = 0;
	} else {
		*event = STM_FE_DEMOD_EVENT_TUNE_FAILED;
		task->nextstate = FE_TASKSTATE_TUNE;
		task->delay_in_ms = DEMOD_ACQUISITION_RETRIAL_PERIOD_MS;
	}

	priv->info.u_info.tune.demod_info =
		priv->c_tinfo.u_info.tune.demod_info;
	if (evt_notify(priv, event))
		pr_err("%s: evt_notify failed\n", __func__);
}

/*
 * Name: demod_state_scan()
 * Description: Describes the task behaviour in SCANNING state of
 * demod state machine
 * Next State:FE_TASKSTATE_IDLE,FE_TASKSTATE_SCAN
 * Previous State:FE_TASKSTATE_IDLE
 */
static
void demod_state_scan(struct stm_fe_demod_s *priv, stm_fe_demod_event_t *event)
{
	int ret = 0;
	bool lock = false;
	struct stm_fe_task_s *task;
	struct fe_scan_config *conf;

	task = &priv->fe_task;
	conf = &priv->fe_scan;
	priv->info.context = STM_FE_DEMOD_SCAN;
	task->prevstate = FE_TASKSTATE_SCAN;

	if (priv->scan_context == STM_FE_DEMOD_SCAN_NEW)
		new_scan_setup(priv);

	if (conf->search_freq >= (conf->end_freq + step_freq(priv))) {
		*event = STM_FE_DEMOD_EVENT_SCAN_COMPLETE;
		task->nextstate = FE_TASKSTATE_IDLE;
		priv->info.u_info.scan.demod_info =
			priv->c_tinfo.u_info.scan.demod_info;
		priv->event_disable = false;
		if (evt_notify(priv, event))
			pr_err("%s: evt_notify failed\n", __func__);
		return;
	}

	do {
		if (!priv->ops->scan) {
			pr_err("%s: FP ops->scan is NULL\n", __func__);
		} else {
			ret = priv->ops->scan(priv, &lock);
			if (ret)
				pr_info("%s: scan failed\n", __func__);
		}
		if (lock) {
			*event = STM_FE_DEMOD_EVENT_SCAN_LOCKED;
			task->nextstate = FE_TASKSTATE_IDLE;
			priv->info.u_info.scan.demod_info =
			    priv->c_tinfo.u_info.scan.demod_info;
			calc_nxt_freq(priv, lock, event);
			task->delay_in_ms = DEMOD_WAKE_UP;
			task->track_cnt = 0;
			priv->event_disable = false;
			if (evt_notify(priv, event))
				pr_err("%s: evt_notify failed\n", __func__);
			break;

		}

		if (conf->search_freq >= conf->end_freq + step_freq(priv)) {
			*event = STM_FE_DEMOD_EVENT_SCAN_COMPLETE;
			task->nextstate = FE_TASKSTATE_IDLE;
			priv->info.u_info.scan.demod_info =
				priv->c_tinfo.u_info.scan.demod_info;
			priv->event_disable = false;
			evt_notify(priv, event);
		}

		task->delay_in_ms = DEMOD_ACQUISITION_RETRIAL_PERIOD_MS;
		calc_nxt_freq(priv, lock, event);
	} while (conf->search_freq < (conf->end_freq + step_freq(priv)));
}

/*
 * Name: demod_state_tracking()
 *
 * Description: Describes the task behaviour in TRACKING state
 * of demod state machine
 * Next State:FE_TASKSTATE_TRACKING,FE_TASKSTATE_TUNE
 * Previous State:FE_TASKSTATE_TUNE
 */
static void demod_state_tracking(struct stm_fe_demod_s *priv,
						stm_fe_demod_event_t *event)
{
	bool lock = FALSE;
	int ret;
	struct stm_fe_task_s *task;

	task = &priv->fe_task;
	task->nextstate = FE_TASKSTATE_TRACKING;
	task->prevstate = FE_TASKSTATE_TRACKING;

	/*call tracking function */
	if (!priv->ops->status)
		pr_err("%s: FP priv->ops->status is NULL\n", __func__);
	else
		ret = priv->ops->status(priv, &lock);

	if (lock) {
		task->lockcount = NUMBER_LOST_BEFORE_UNLOCK;
		task->delay_in_ms = DEMOD_SIGNAL_STATUS_TRACK_PERIOD_MS;

		if (!(task->track_cnt % (DEMOD_PARAM_UPDATE_PERIOD_MS /
					DEMOD_SIGNAL_STATUS_TRACK_PERIOD_MS))){
			/*call tracking function */
			if (!priv->ops->tracking)
				pr_err("%s: FP tracking is NULL\n", __func__);
			else
				ret = priv->ops->tracking(priv);
		}
		task->track_cnt++;
	} else {
		if (task->lockcount == NUMBER_LOST_BEFORE_UNLOCK)
			priv->event_disable = false;

		check_lnb_config(priv, event);
		task->lockcount = task->lockcount - 1;
	}
	priv->info.u_info.tune.demod_info =
		priv->c_tinfo.u_info.tune.demod_info;

	if (!task->lockcount) {
		pr_info("%s: lock lost for %s\n", __func__, priv->demod_name);
		*event = STM_FE_DEMOD_EVENT_TUNE_UNLOCKED;
		task->nextstate = FE_TASKSTATE_TUNE;
		priv->event_disable = false;
		task->delay_in_ms = DEMOD_RELOCK_MONITOR_PERIOD_MS;
		if (evt_notify(priv, event))
			pr_err("%s: evt_notify failed\n", __func__);
	}
}

/*
 * Name: check_lnb_config()
 *
 * Description: checking of LNB configuration
 * during tracking
 */
static
void check_lnb_config(struct stm_fe_demod_s *priv, stm_fe_demod_event_t *event)
{
	struct stm_fe_lnb_s *lnb_priv;
	struct lnb_config_t lnbconfig;
	int ret = 0;

	if (!priv->lnb_h)
		return;

	lnb_priv = (struct stm_fe_lnb_s *)priv->lnb_h;

	if (lnb_priv->ops->getconfig) {
		ret = lnb_priv->ops->getconfig(lnb_priv, &lnbconfig);
		if (ret) {
			pr_err("%s: lnb_getconfig failed\n", __func__);
			return;
		}
		switch (lnbconfig.status) {
		case LNB_STATUS_OVER_TEMPERATURE:
		case LNB_STATUS_SHORTCIRCUIT_OVERTEMPERATURE:
		case LNB_STATUS_SHORT_CIRCUIT:
			*event = STM_FE_DEMOD_EVENT_LNB_FAILURE;
			if (evt_notify(priv, event))
				pr_err("%s: evt_notify failed\n", __func__);
			break;
		default:
			break;
		}
	} else {
		pr_err("%s: FP lnb_ops->lnb_getconfig is NULL\n", __func__);
	}
}
