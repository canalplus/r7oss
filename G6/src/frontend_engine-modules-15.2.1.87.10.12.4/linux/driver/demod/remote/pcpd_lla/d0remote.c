/************************************************************************
Copyright (C) 2012 STMicroelectronics. All Rights Reserved.

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

Source file name : d0remote.c
Author :           Shobhit

PCPD LLA for remote tuning

Date        Modification                                    Name
----        ------------                                    --------
1-Nov-12    Created                                         Rahul

************************************************************************/

#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/socket.h>
#include <linux/in.h>
#include <linux/net.h>
#include <linux/slab.h>
#include <net/sock.h>
#include <net/inet_common.h>
#include <linux/mutex.h>
#include <linux/syscalls.h>
#include <linux/uaccess.h>
#include <linux/list.h>
#include <linux/wait.h>
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
#include <linux/sched.h>
#include "stm_fe_demod.h"
#include "stm_fe_ip.h"
#include "stm_fe_lnb.h"
#include "stm_fe_engine.h"
#include "stm_fe_diseqc.h"
#include "lla_utils.h"
#include "d0remote.h"
#include "tsclient.h"
static bool config_sock;
static struct socket *sock_p;
static DEFINE_MUTEX(config_mutex);

static int thread_inp_fe_remote[2] = {SCHED_NORMAL, 0};
module_param_array_named(thread_INP_FE_Remote, thread_inp_fe_remote, int, NULL,
									0644);
MODULE_PARM_DESC(thread_INP_FE_Remote, "INP-FE-Remote thread: s(Mode),p(Priority)");

struct stm_fe_demod_s *get_instance(uint32_t chnl_id)
{
	struct list_head *pos;
	struct stm_fe_demod_s *demod;
	list_for_each(pos, get_demod_list()) {
		demod = list_entry(pos, struct stm_fe_demod_s, list);
		if (demod->fe_remote.chnl_id == chnl_id)
			return demod;
	}
	return NULL;
}
struct stm_fe_demod_s *check_txn_id(uint16_t txn_id)
{
	struct list_head *pos;
	struct stm_fe_demod_s *demod;
	list_for_each(pos, get_demod_list()) {
		demod = list_entry(pos, struct stm_fe_demod_s, list);
		if (demod->fe_remote.txn_id == txn_id)
			return demod;
	}
	return NULL;
}
/*
 * Name: remote_thread()
 *
 * Description:Thread to receive all the pcpd packets
 */
static int remote_thread(void *data)
{
	int ret;
	struct stm_fe_demod_s *priv;
	struct iovec *iov;
	struct msghdr *sock_msg;
	char *msg;
	mm_segment_t old_mm;
	uint32_t chnl_id = 0;
	uint32_t cmd;
	uint32_t cmd_id;
	uint32_t status;
	struct stm_fe_demod_s *demod;
	uint16_t txn_id;

	priv = data;
	iov = (struct iovec *)stm_fe_malloc(sizeof(struct iovec));
	if (!iov) {
		pr_err("%s: mem alloc failed for iov\n", __func__);
		return -ENOMEM;
	}
	sock_msg = (struct msghdr *)stm_fe_malloc(sizeof(struct msghdr));
	if (!sock_msg) {
		pr_err("%s: mem alloc failed for sock_msg\n", __func__);
		stm_fe_free((void **)&iov);
		return -ENOMEM;
	}
	msg = (char *)stm_fe_malloc(MAX_MSG_SIZE);
	if (!msg) {
		pr_err("%s: mem alloc failed for msg\n", __func__);
		stm_fe_free((void **)&sock_msg);
		stm_fe_free((void **)&iov);
		return -ENOMEM;
	}

	while (!stm_fe_thread_should_stop()) {
		memset(iov, 0, sizeof(struct iovec));
		memset(sock_msg, 0, sizeof(struct msghdr));
		memset(msg, 0, MAX_MSG_SIZE);
		iov[0].iov_base = msg;
		iov[0].iov_len = MAX_MSG_SIZE;
		sock_msg->msg_name = NULL;
		sock_msg->msg_namelen = 0;
		sock_msg->msg_iov = iov;
		sock_msg->msg_iovlen = 1;
		sock_msg->msg_control = NULL;
		sock_msg->msg_controllen = 0;
		sock_msg->msg_flags = 0;
		sock_msg->msg_iov = iov;
		old_mm = get_fs();
		set_fs(KERNEL_DS);
		/*Receive packet from UDP socket*/
		ret = sock_recvmsg(priv->fe_remote.sock, sock_msg,
							MAX_MSG_SIZE-1, 0);
		set_fs(old_mm);
		if (ret < 0) {
			pr_err("%s: recv msg failed: %d\n", __func__, ret);
			stm_fe_free((void **)&msg);
			stm_fe_free((void **)&sock_msg);
			stm_fe_free((void **)&iov);
			return ret;
		}

		cmd_id = load_data_le32((u_int8_t *) &msg[12]);
		if (cmd_id != PCPD_REMOTE_TUNER_RESPONSE)
			continue;
		cmd = load_data_le32((u_int8_t *) &msg[16]);
		chnl_id = load_data_le32((u_int8_t *) &msg[20]);
		if (cmd == STPCPD_REMOTE_OPEN) {
			txn_id = load_data_le16((u_int8_t *) &msg[8]);
			status = load_data_le32((u_int8_t *) &msg[24]);
			demod = check_txn_id(txn_id);
			if (status == STPCPD_REMOTE_STATUS_SUCCESS)
				demod->fe_remote.chnl_id = chnl_id;
		} else
			demod = get_instance(chnl_id);
		stm_fe_fifo_in(&demod->fe_remote.remote_fifo, msg, ret);
		demod->fe_remote.flag = 1;
		fe_wake_up_interruptible(&demod->fe_remote.remote_waitq);
	}
	stm_fe_free((void **)&msg);
	stm_fe_free((void **)&sock_msg);
	stm_fe_free((void **)&iov);

	return 0;
}
/*
 * Name: stm_fe_remote_attach()
 *
 * Description: Installed the wrapper function pointers for LLA
 *
 */
int stm_fe_remote_attach(struct stm_fe_demod_s *priv)
{
	if (!priv->ops)
		return -EFAULT;

	priv->ops->init = remote_init;
	priv->ops->tune = remote_tune;
	priv->ops->tracking = remote_tracking;
	priv->ops->status = remote_status;
	priv->ops->abort = remote_abort;
	priv->ops->standby = remote_standby;
	priv->ops->detach = stm_fe_remote_detach;
	priv->ops->restore = remote_restore;
	priv->ops->unlock = remote_unlock;
	priv->ops->term = remote_term;
	priv->ops->tsclient_init = tsclient_init_ts;
	priv->ops->tsclient_term = tsclient_term_ts;
	priv->ops->tsclient_pid_configure = tsclient_pid_start;
	priv->ops->tsclient_pid_deconfigure = tsclient_pid_stop;

	return 0;
}
EXPORT_SYMBOL(stm_fe_remote_attach);

/*
 * Name: remote_init()
 *
 * Description: Providing Initiliazing parameters to LLA
 *
 */
int remote_init(struct stm_fe_demod_s *priv)
{

	int ret = 0;
	struct iovec *iov;
	struct msghdr *sock_msg;
	mm_segment_t old_mm;
	int size = 0 ;
	char *msg;
	struct pcpd_packet_data_t *pcpd_msg;
	uint32_t ts_config = 0;
	uint32_t status;
	uint32_t chnl_id = 0;

	ret = stm_fe_fifo_alloc(&priv->fe_remote.remote_fifo, FIFO_SIZE);
	if (ret < 0)
		return ret;

	fe_init_waitqueue_head(&priv->fe_remote.remote_waitq);
	priv->fe_remote.flag = 0;
	stm_fe_mutex_lock(&config_mutex);
	if (config_sock == false) {
		ret = transport_setup(priv);
		sock_p = priv->fe_remote.sock;
		config_sock = true;
		if (ret < 0) {
			pr_err("%s: transport_setup failed: %d\n",
								 __func__, ret);
			stm_fe_mutex_unlock(&config_mutex);
			stm_fe_fifo_free(&priv->fe_remote.remote_fifo);
			return ret;
		}
		priv->fe_remote.sock = sock_p;
		ret = stm_fe_thread_create(&priv->fe_remote.remote_thread,
				remote_thread, (void *)priv, "INP-FE-Remote",
							  thread_inp_fe_remote);

		if (ret) {
			pr_err("%s: fe task creation failed\n", __func__);
			stm_fe_mutex_unlock(&config_mutex);
			stm_fe_fifo_free(&priv->fe_remote.remote_fifo);
			return -ENOMEM;
		}
	}
	priv->fe_remote.sock = sock_p;

	stm_fe_mutex_unlock(&config_mutex);
	pcpd_msg = (struct pcpd_packet_data_t *)
			stm_fe_malloc(sizeof(struct pcpd_packet_data_t));
	if (!pcpd_msg) {
		pr_err("%s: mem alloc failed for pcpd_msg\n", __func__);
		stm_fe_fifo_free(&priv->fe_remote.remote_fifo);
		return -ENOMEM;
	}

	pcpd_create_msg(priv, pcpd_msg);

	store_data_le32(pcpd_msg->data, STPCPD_REMOTE_OPEN);
	size += 4;
	store_data_le32(&pcpd_msg->data[4], chnl_id);
	size += 4;
	if (priv->config->ts_out & DEMOD_TS_SERIAL_PUNCT_CLOCK)
		ts_config |= STPCPD_REMOTE_TS_SERIAL_PUNCT_CLOCK;
	if (priv->config->ts_out & DEMOD_TS_SERIAL_CONT_CLOCK)
		ts_config |= STPCPD_REMOTE_TS_SERIAL_CONT_CLOCK;
	if (priv->config->ts_out & DEMOD_TS_PARALLEL_PUNCT_CLOCK)
		ts_config |= STPCPD_REMOTE_TS_PARALLEL_PUNCT_CLOCK;
	if (priv->config->ts_out & DEMOD_TS_DVBCI_CLOCK)
		ts_config |= STPCPD_REMOTE_TS_DVBCI_CLOCK;
	if (priv->config->ts_out & FE_TS_FALLINGEDGE_CLOCK)
		ts_config |= STPCPD_REMOTE_TS_FALLINGEDGE_CLOCK;
	if (priv->config->ts_out & DEMOD_TS_SYNCBYTE_OFF)
		ts_config |= STPCPD_REMOTE_TS_SYNCBYTE_OFF;
	if (priv->config->ts_out & DEMOD_TS_PARITYBYTES_ON)
		ts_config |= STPCPD_REMOTE_TS_PARITYBYTES_ON;
	if (priv->config->ts_out & DEMOD_TS_SWAP_ON)
		ts_config |= STPCPD_REMOTE_TS_SWAP_ON;
	if (priv->config->ts_out & DEMOD_TS_SMOOTHER_ON)
		ts_config |= STPCPD_REMOTE_TS_SMOOTHER_ON;

	store_data_le32(&pcpd_msg->data[8], ts_config);
	size += 4;
	pcpd_msg->length = size;

	iov = (struct iovec *)stm_fe_malloc(sizeof(struct iovec));
	if (!iov) {
		pr_err("%s: mem alloc failed for iov\n", __func__);
		stm_fe_free((void **)&pcpd_msg);
		stm_fe_fifo_free(&priv->fe_remote.remote_fifo);
		return -ENOMEM;
	}
	sock_msg = (struct msghdr *)stm_fe_malloc(sizeof(struct msghdr));
	if (!sock_msg) {
		pr_err("%s: mem alloc failed for sock_msg\n", __func__);
		stm_fe_free((void **)&iov);
		stm_fe_free((void **)&pcpd_msg);
		stm_fe_fifo_free(&priv->fe_remote.remote_fifo);
		return -ENOMEM;
	}
	msg = (char *)stm_fe_malloc(MAX_MSG_SIZE);
	if (!msg) {
		pr_err("%s: mem alloc failed for msg\n", __func__);
		stm_fe_free((void **)&sock_msg);
		stm_fe_free((void **)&iov);
		stm_fe_free((void **)&pcpd_msg);
		stm_fe_fifo_free(&priv->fe_remote.remote_fifo);
		return -ENOMEM;
	}
	old_mm = get_fs();
	set_fs(KERNEL_DS);

	memset(iov, 0, sizeof(struct iovec));
	memset(sock_msg, 0, sizeof(struct msghdr));
	iov[0].iov_base = (void *)pcpd_msg;
	iov[0].iov_len = PCPD_HEADER_SIZE + size;
	sock_msg->msg_name = NULL;
	sock_msg->msg_namelen = 0;
	sock_msg->msg_iov = iov;
	sock_msg->msg_iovlen = 1;
	sock_msg->msg_control = NULL;
	sock_msg->msg_controllen = 0;
	sock_msg->msg_flags = 0;
	/*Send packet over UDP socket*/
	ret = sock_sendmsg(priv->fe_remote.sock, sock_msg,
						PCPD_HEADER_SIZE + size);
	set_fs(old_mm);
	if (ret < 0) {
		pr_err("%s: send msg failed: %d\n", __func__, ret);
		stm_fe_fifo_free(&priv->fe_remote.remote_fifo);
		goto err;
	} else {
		pr_info("%s: Tune open success = %d\n", __func__, ret);
	}

	fe_wait_event_interruptible_timeout(priv->fe_remote.remote_waitq,
					priv->fe_remote.flag != 0,
					msecs_to_jiffies(REMOTE_DELAY_MS));
	priv->fe_remote.flag = 0;
	ret = stm_fe_fifo_out(&priv->fe_remote.remote_fifo, msg, FIFO_SIZE);
	if (ret < 0) {
		stm_fe_fifo_free(&priv->fe_remote.remote_fifo);
		goto err;
	}
	msg[ret] = '\0';

	status = load_data_le32((u_int8_t *) &msg[24]);
	if (status == STPCPD_REMOTE_STATUS_SUCCESS) {
		ret = 0;
	} else {
		ret = -EINVAL;
		pr_err("%s: Error with status= %d\n", __func__, status);
	}

err:
	stm_fe_free((void **)&msg);
	stm_fe_free((void **)&sock_msg);
	stm_fe_free((void **)&iov);
	stm_fe_free((void **)&pcpd_msg);

	return ret;
}
EXPORT_SYMBOL(remote_init);
/*
 * Name: remote_tune()
 *
 * Description: Provided tuning parameters to remote tuner
 * and perfomed tuning
 *
 */

int remote_tune(struct stm_fe_demod_s *priv, bool *lock)
{
	int ret = 0;
	struct iovec *iov;
	struct msghdr *sock_msg;
	mm_segment_t old_mm;
	char *msg;
	int size = 0 ;
	struct pcpd_packet_data_t *pcpd_msg;
	uint32_t chnl_id = 0;
	uint32_t cmd;
	uint32_t status;
	uint32_t fec_type;
	stm_fe_demod_channel_info_t *remote_info;

	remote_info = &priv->c_tinfo.u_info.tune.demod_info;

	pcpd_msg = (struct pcpd_packet_data_t *)
			stm_fe_malloc(sizeof(struct pcpd_packet_data_t));
	if (!pcpd_msg) {
		pr_err("%s: mem alloc failed for pcpd_msg\n", __func__);
		return -ENOMEM;
	}
	pcpd_create_msg(priv, pcpd_msg);
	store_data_le32(pcpd_msg->data, STPCPD_REMOTE_SEARCH);
	size += 4;
	store_data_le32(&pcpd_msg->data[4], priv->fe_remote.chnl_id);
	size += 4;
	store_data_le32(&pcpd_msg->data[8],
			priv->tp.u_tune.j83b.freq * 1000);
	size += 4;
	store_data_le32(&pcpd_msg->data[12], 0);
	size += 4;
	store_data_le32(&pcpd_msg->data[16],
			priv->tp.u_tune.j83b.sr);
	size += 4;
	store_data_le32(&pcpd_msg->data[20],
		remote_modulationmap(priv->tp.u_tune.j83b.mod));
	size += 4;
	if (priv->tp.std == STM_FE_DEMOD_TX_STD_J83_B)
		fec_type = STPCPD_REMOTE_ANNEX_B;
	else
		fec_type = STPCPD_REMOTE_ANNEX_A_C;
	store_data_le32(&pcpd_msg->data[24], fec_type);
	size += 4;

	pcpd_msg->length = size;

	iov = (struct iovec *)stm_fe_malloc(sizeof(struct iovec));
	if (!iov) {
		pr_err("%s: mem alloc failed for iov\n", __func__);
		stm_fe_free((void **)&pcpd_msg);
		return -ENOMEM;
	}
	sock_msg = (struct msghdr *)stm_fe_malloc(sizeof(struct msghdr));
	if (!sock_msg) {
		pr_err("%s: mem alloc failed for sock_msg\n", __func__);
		stm_fe_free((void **)&iov);
		stm_fe_free((void **)&pcpd_msg);
		return -ENOMEM;
	}

	old_mm = get_fs();
	set_fs(KERNEL_DS);

	memset(iov, 0, sizeof(struct iovec));
	memset(sock_msg, 0, sizeof(struct msghdr));
	iov[0].iov_base = (void *)pcpd_msg;
	iov[0].iov_len = PCPD_HEADER_SIZE + size;
	sock_msg->msg_name = NULL;
	sock_msg->msg_namelen = 0;
	sock_msg->msg_iov = iov;
	sock_msg->msg_iovlen = 1;
	sock_msg->msg_control = NULL;
	sock_msg->msg_controllen = 0;
	sock_msg->msg_flags = 0;
	/*Send packet over UDP socket*/
	ret = sock_sendmsg(priv->fe_remote.sock, sock_msg,
						PCPD_HEADER_SIZE + size);
	set_fs(old_mm);
	if (ret < 0) {
		pr_err("%s: send msg failed: %d\n", __func__, ret);
		stm_fe_free((void **)&sock_msg);
		stm_fe_free((void **)&iov);
		stm_fe_free((void **)&pcpd_msg);
		return ret;
	} else {
		pr_info("%s: Tune req: success= %d\n", __func__, ret);
	}

	msg = (char *)stm_fe_malloc(MAX_MSG_SIZE);
	if (!msg) {
		pr_err("%s: mem alloc failed for msg\n", __func__);
		stm_fe_free((void **)&sock_msg);
		stm_fe_free((void **)&iov);
		stm_fe_free((void **)&pcpd_msg);
		return -ENOMEM;
	}

	fe_wait_event_interruptible_timeout(priv->fe_remote.remote_waitq,
					priv->fe_remote.flag != 0,
					msecs_to_jiffies(REMOTE_DELAY_MS));
	priv->fe_remote.flag = 0;
	ret = stm_fe_fifo_out(&priv->fe_remote.remote_fifo, msg, FIFO_SIZE);
	if (ret < 0)
		goto err;

	msg[ret] = '\0';
	cmd = load_data_le32((u_int8_t *) &msg[16]);
	chnl_id = load_data_le32((u_int8_t *) &msg[20]);
	status = load_data_le32((u_int8_t *) &msg[24]);
	if (cmd == STPCPD_REMOTE_SEARCH) {
		if (status == STPCPD_REMOTE_STATUS_SUCCESS) {
			*lock = 1;
			remote_info->std = STM_FE_DEMOD_TX_STD_J83_B;
			remote_info->u_channel.j83b.freq =
				load_data_le32((u_int8_t *) &msg[28])/1000;
			remote_info->u_channel.j83b.sr =
					load_data_le32((u_int8_t *) &msg[40]);
			remote_info->u_channel.j83b.inv =
					load_data_le32((u_int8_t *) &msg[44]);
			remote_info->u_channel.j83b.mod =
			     remote_getmodulation(load_data_le32((u_int8_t *)
								    &msg[32]));
			remote_info->u_channel.j83b.snr =
					load_data_le32((u_int8_t *) &msg[48]);
			remote_info->u_channel.j83b.ber =
					load_data_le32((u_int8_t *) &msg[52]);
			remote_info->u_channel.j83b.status =
							STM_FE_DEMOD_SYNC_OK;
		} else {
			remote_info->u_channel.j83b.status =
						STM_FE_DEMOD_NO_SIGNAL;
			pr_err("%s: remote search failed\n", __func__);
		}
	} else {
		pr_info("%s: cmd not matched cmd = %d chl = %d\n",
				   __func__, cmd, priv->fe_remote.chnl_id);
	}
	ret = 0;
err:
	stm_fe_free((void **)&msg);
	stm_fe_free((void **)&sock_msg);
	stm_fe_free((void **)&iov);
	stm_fe_free((void **)&pcpd_msg);

	return ret;
}
EXPORT_SYMBOL(remote_tune);

int remote_status(struct stm_fe_demod_s *priv, bool *locked)
{
	stm_fe_demod_channel_info_t *pdemod_Info;
	stm_fe_demod_status_t *pstatus;

	pdemod_Info = &priv->c_tinfo.u_info.tune.demod_info;

	*locked = 1;
	pstatus = &pdemod_Info->u_channel.j83b.status;
	if (*pstatus == STM_FE_DEMOD_SYNC_OK)
		*locked = 1;
	else
		*locked = 0;

	return 0;
}
EXPORT_SYMBOL(remote_status);

/*
 * Name: remote_tracking()
 *
 * Description: Checking the lock status of demodulator
 *
 */
int remote_tracking(struct stm_fe_demod_s *priv)
{
	int ret = 0;
	struct iovec *iov;
	struct msghdr *sock_msg;
	mm_segment_t old_mm;
	char *msg;
	int size = 0 ;
	struct pcpd_packet_data_t *pcpd_msg;
	uint32_t chnl_id = 0;
	uint32_t cmd;
	uint32_t status;
	stm_fe_demod_channel_info_t *remote_info;

	remote_info = &priv->c_tinfo.u_info.tune.demod_info;

	pcpd_msg = (struct pcpd_packet_data_t *)
			stm_fe_malloc(sizeof(struct pcpd_packet_data_t));
	if (!pcpd_msg) {
		pr_err("%s: mem alloc failed for pcpd_msg\n", __func__);
		return -ENOMEM;
	}
	pcpd_create_msg(priv, pcpd_msg);
	store_data_le32(pcpd_msg->data, STPCPD_REMOTE_TRACKING);
	size += 4;
	store_data_le32(&pcpd_msg->data[4], priv->fe_remote.chnl_id);
	size += 4;

	pcpd_msg->length = size;

	iov = (struct iovec *)stm_fe_malloc(sizeof(struct iovec));
	if (!iov) {
		pr_err("%s: mem alloc failed for iov\n", __func__);
		stm_fe_free((void **)&pcpd_msg);
		return -ENOMEM;
	}
	sock_msg = (struct msghdr *)stm_fe_malloc(sizeof(struct msghdr));
	if (!sock_msg) {
		pr_err("%s: mem alloc failed for sock_msg\n", __func__);
		stm_fe_free((void **)&iov);
		stm_fe_free((void **)&pcpd_msg);
		return -ENOMEM;
	}

	old_mm = get_fs();
	set_fs(KERNEL_DS);

	memset(iov, 0, sizeof(struct iovec));
	memset(sock_msg, 0, sizeof(struct msghdr));
	iov[0].iov_base = (void *)pcpd_msg;
	iov[0].iov_len = PCPD_HEADER_SIZE + size;
	sock_msg->msg_name = NULL;
	sock_msg->msg_namelen = 0;
	sock_msg->msg_iov = iov;
	sock_msg->msg_iovlen = 1;
	sock_msg->msg_control = NULL;
	sock_msg->msg_controllen = 0;
	sock_msg->msg_flags = 0;
	/*Send packet over UDP socket*/
	ret = sock_sendmsg(priv->fe_remote.sock, sock_msg,
						PCPD_HEADER_SIZE + size);
	set_fs(old_mm);
	if (ret < 0) {
		pr_err("%s: send msg failed: %d\n", __func__, ret);
		stm_fe_free((void **)&sock_msg);
		stm_fe_free((void **)&iov);
		stm_fe_free((void **)&pcpd_msg);
		return ret;
	}

	msg = (char *)stm_fe_malloc(MAX_MSG_SIZE);
	if (!msg) {
		pr_err("%s: mem alloc failed for msg\n", __func__);
		stm_fe_free((void **)&sock_msg);
		stm_fe_free((void **)&iov);
		stm_fe_free((void **)&pcpd_msg);
		return -ENOMEM;
	}

	fe_wait_event_interruptible_timeout(priv->fe_remote.remote_waitq,
			priv->fe_remote.flag != 0, msecs_to_jiffies(10000));
	priv->fe_remote.flag = 0;
	ret = stm_fe_fifo_out(&priv->fe_remote.remote_fifo, msg, FIFO_SIZE);
	if (ret < 0)
		goto err;
	msg[ret] = '\0';
	cmd = load_data_le32((u_int8_t *) &msg[16]);
	chnl_id = load_data_le32((u_int8_t *) &msg[20]);
	status = load_data_le32((u_int8_t *) &msg[24]);
	if (cmd == STPCPD_REMOTE_TRACKING) {
		if (status == STPCPD_REMOTE_STATUS_SUCCESS) {
			remote_info->std = STM_FE_DEMOD_TX_STD_J83_B;
			remote_info->u_channel.j83b.freq =
				load_data_le32((u_int8_t *) &msg[28])/1000;
			remote_info->u_channel.j83b.sr =
					load_data_le32((u_int8_t *) &msg[40]);
			remote_info->u_channel.j83b.inv =
					load_data_le32((u_int8_t *) &msg[44]);
			remote_info->u_channel.j83b.mod =
			   remote_getmodulation(load_data_le32((u_int8_t *)
								   &msg[32]));
			remote_info->u_channel.j83b.snr =
					load_data_le32((u_int8_t *) &msg[48]);
			remote_info->u_channel.j83b.ber =
					load_data_le32((u_int8_t *) &msg[52]);
			remote_info->u_channel.j83b.status =
							STM_FE_DEMOD_SYNC_OK;
		} else {
			ret = -EINVAL;
			remote_info->std = 0;
			remote_info->u_channel.j83b.freq = 0;
			remote_info->u_channel.j83b.sr = 0;
			remote_info->u_channel.j83b.inv = 0;
			remote_info->u_channel.j83b.mod = 0;
			remote_info->u_channel.j83b.snr = 0;
			remote_info->u_channel.j83b.ber = 0;
			remote_info->u_channel.j83b.status =
							STM_FE_DEMOD_NO_SIGNAL;
			pr_err("%s: remote tracking failed\n", __func__);
		}
	} else {
		pr_info("%s: cmd not matched cmd = %d chl = %d\n",
				   __func__, cmd, priv->fe_remote.chnl_id);
	}
	ret = 0;
err:
	stm_fe_free((void **)&msg);
	stm_fe_free((void **)&sock_msg);
	stm_fe_free((void **)&iov);
	stm_fe_free((void **)&pcpd_msg);

	return ret;
}
EXPORT_SYMBOL(remote_tracking);

/*
 * Name: remote_abort()
 *
 * Description: Abort demod and tuner operations
 *
 */
int remote_abort(struct stm_fe_demod_s *priv, bool abort)
{
	int ret = 0;

	return ret;
}
EXPORT_SYMBOL(remote_abort);

/*
 * Name: remote_unlock()
 *
 * Description: Unlock demod and tuner operations
 *
 */
int remote_unlock(struct stm_fe_demod_s *priv)
{
	int ret = 0;

	return ret;
}
EXPORT_SYMBOL(remote_unlock);

/*
 * Name: remote_standby()
 *
 * Description: low power mode of demodulator
 *
 */
int remote_standby(struct stm_fe_demod_s *priv, bool standby)
{
	int ret = 0;

	return ret;
}
EXPORT_SYMBOL(remote_standby);

/*
 * Name: stm_fe_remote_detach()
 *
 * Description: Uninstalled the wrapper function pointers for LLA
 *
 */
int stm_fe_remote_detach(struct stm_fe_demod_s *priv)
{
	if (!priv->ops)
		return -EFAULT;

	priv->ops->tsclient_pid_deconfigure = NULL;
	priv->ops->tsclient_pid_configure = NULL;
	priv->ops->tsclient_term = NULL;
	priv->ops->tsclient_init = NULL;
	priv->ops->term = NULL;
	priv->ops->unlock = NULL;
	priv->ops->restore = NULL;
	priv->ops->detach = NULL;
	priv->ops->standby = NULL;
	priv->ops->abort = NULL;
	priv->ops->status = NULL;
	priv->ops->tracking = NULL;
	priv->ops->tune = NULL;
	priv->ops->init = NULL;

	return 0;
}
EXPORT_SYMBOL(stm_fe_remote_detach);

/*
 * Name: remote_restore()
 *
 * Description: Re-Initiliazing LLA after restoring from CPS
 *
 */
int remote_restore(struct stm_fe_demod_s *priv)
{
	int ret = 0;

	return ret;
}
EXPORT_SYMBOL(remote_restore);
/*
 * Name: remote_term()
 *
 * Description: Terminate demod and tuner operations
 *
 */
int remote_term(struct stm_fe_demod_s *priv)
{
	int ret = 0;
	struct sockaddr_in *addr_in;
	struct iovec *iov;
	struct msghdr *sock_msg;
	mm_segment_t old_mm;
	char *msg;
	int size = 0 ;
	struct pcpd_packet_data_t *pcpd_msg;
	uint32_t chnl_id = 0;
	uint32_t cmd;
	uint32_t status;

	pcpd_msg = (struct pcpd_packet_data_t *)
			stm_fe_malloc(sizeof(struct pcpd_packet_data_t));
	if (!pcpd_msg) {
		pr_err("%s: mem alloc failed for pcpd_msg\n", __func__);
		return -ENOMEM;
	}
	iov = (struct iovec *)stm_fe_malloc(sizeof(struct iovec));
	if (!iov) {
		pr_err("%s: mem alloc failed for iov\n", __func__);
		stm_fe_free((void **)&pcpd_msg);
		return -ENOMEM;
	}
	sock_msg = (struct msghdr *)stm_fe_malloc(sizeof(struct msghdr));
	if (!sock_msg) {
		pr_err("%s: mem alloc failed for sock_msg\n", __func__);
		stm_fe_free((void **)&iov);
		stm_fe_free((void **)&pcpd_msg);
		return -ENOMEM;
	}

	pcpd_create_msg(priv, pcpd_msg);
	store_data_le32(pcpd_msg->data, STPCPD_REMOTE_CLOSE);
	size += 4;
	store_data_le32(&pcpd_msg->data[4], priv->fe_remote.chnl_id);
	size += 4;

	pcpd_msg->length = size;
	msg = (char *)stm_fe_malloc(MAX_MSG_SIZE);
	if (!msg) {
		pr_err("%s: mem alloc failed for msg\n", __func__);
		stm_fe_free((void **)&sock_msg);
		stm_fe_free((void **)&iov);
		stm_fe_free((void **)&pcpd_msg);
		return -ENOMEM;
	}
	old_mm = get_fs();
	set_fs(KERNEL_DS);

	memset(iov, 0, sizeof(struct iovec));
	memset(sock_msg, 0, sizeof(struct msghdr));
	iov[0].iov_base = (void *)pcpd_msg;
	iov[0].iov_len = PCPD_HEADER_SIZE + size;
	sock_msg->msg_name = NULL;
	sock_msg->msg_namelen = 0;
	sock_msg->msg_iov = iov;
	sock_msg->msg_iovlen = 1;
	sock_msg->msg_control = NULL;
	sock_msg->msg_controllen = 0;
	sock_msg->msg_flags = 0;
	/*Send packet over UDP socket*/
	ret = sock_sendmsg(priv->fe_remote.sock, sock_msg,
						PCPD_HEADER_SIZE + size);
	set_fs(old_mm);
	if (ret < 0) {
		pr_err("%s: send msg failed: %d\n", __func__, ret);
		stm_fe_free((void **)&msg);
		stm_fe_free((void **)&sock_msg);
		stm_fe_free((void **)&iov);
		stm_fe_free((void **)&pcpd_msg);
		return ret;
	}

	fe_wait_event_interruptible_timeout(priv->fe_remote.remote_waitq,
					priv->fe_remote.flag != 0,
					msecs_to_jiffies(REMOTE_DELAY_MS));
	priv->fe_remote.flag = 0;
	ret = kfifo_out(&priv->fe_remote.remote_fifo, msg, FIFO_SIZE);
	if (ret < 0) {
		pr_err("%s: kfifo_out : Failed= %d\n", __func__, ret);
		goto err;
	}
	msg[ret] = '\0';
	cmd = load_data_le32((u_int8_t *) &msg[16]);
	chnl_id = load_data_le32((u_int8_t *) &msg[20]);
	status = load_data_le32((u_int8_t *) &msg[24]);

	if ((cmd == STPCPD_REMOTE_CLOSE)
				&& (chnl_id == priv->fe_remote.chnl_id)) {
		if (status != STPCPD_REMOTE_STATUS_SUCCESS) {
			ret = -EINVAL;
			pr_err("%s: remote close failed.status= %d\n", __func__,
									status);
		}
	} else {
		pr_info("%s: Remote close command not matched\n", __func__);
	}
	/*stop a thread created by kthread_create() */
	ret = stm_fe_thread_stop(&priv->fe_remote.remote_thread);
	if (ret)
		pr_err("%s: stm_fe_thread_stop failed\n", __func__);

	/*Now connect to own socket*/
	addr_in = (struct sockaddr_in *)
				stm_fe_malloc(sizeof(struct sockaddr_in));
	if (!addr_in) {
		pr_err("%s: mem alloc failed for addr_in\n", __func__);
		stm_fe_free((void **)&msg);
		stm_fe_free((void **)&sock_msg);
		stm_fe_free((void **)&iov);
		stm_fe_free((void **)&pcpd_msg);
		return -ENOMEM;
	}
	addr_in->sin_port = htons(ESTB_PORT_SYNC_CMD);
	addr_in->sin_addr.s_addr = htonl(INADDR_ANY);
	ret = priv->fe_remote.sock->ops->connect(priv->fe_remote.sock,
		(struct sockaddr *) addr_in, sizeof(struct sockaddr), 0);
	if (ret < 0) {
		pr_err("%s: connection of socket failed: %d\n", __func__, ret);
		goto err;
	}
	pcpd_create_msg(priv, pcpd_msg);
	pcpd_msg->length = 0;
	pcpd_msg->cmd_id = PCPD_REMOTE_TUNER_RESPONSE;
	old_mm = get_fs();
	set_fs(KERNEL_DS);

	memset(iov, 0, sizeof(struct iovec));
	memset(sock_msg, 0, sizeof(struct msghdr));
	iov[0].iov_base = (void *)pcpd_msg;
	iov[0].iov_len = PCPD_HEADER_SIZE + size;
	sock_msg->msg_name = NULL;
	sock_msg->msg_namelen = 0;
	sock_msg->msg_iov = iov;
	sock_msg->msg_iovlen = 1;
	sock_msg->msg_control = NULL;
	sock_msg->msg_controllen = 0;
	sock_msg->msg_flags = 0;
	/*Send packet over UDP socket*/
	ret = sock_sendmsg(priv->fe_remote.sock, sock_msg,
						PCPD_HEADER_SIZE + size);
	set_fs(old_mm);
	if (ret < 0) {
		pr_err("%s: send msg failed: %d\n", __func__, ret);
		goto err;
	}
	fe_wait_event_interruptible_timeout(priv->fe_remote.remote_waitq,
					priv->fe_remote.flag != 0,
					msecs_to_jiffies(REMOTE_DELAY_MS));
	priv->fe_remote.flag = 0;
	stm_fe_fifo_out(&priv->fe_remote.remote_fifo, msg, FIFO_SIZE);

err:
	stm_fe_free((void **)&addr_in);
	stm_fe_free((void **)&msg);
	stm_fe_free((void **)&sock_msg);
	stm_fe_free((void **)&iov);
	stm_fe_free((void **)&pcpd_msg);
	sock_release(priv->fe_remote.sock);
	stm_fe_free((void **)&sock_p);
	stm_fe_free((void **)&priv->fe_remote.sock);

	return ret;
}
EXPORT_SYMBOL(remote_term);

static int32_t __init remote_lla_init(void)
{
	pr_info("Loading remote driver module...\n");

	return 0;
}

module_init(remote_lla_init);

static void __exit remote_lla_term(void)
{
	pr_info("Removing remote driver module ...\n");

}

module_exit(remote_lla_term);

MODULE_DESCRIPTION("Low level driver for remote tuner");
MODULE_AUTHOR("STMicroelectronics Limited");
MODULE_LICENSE("GPL");
