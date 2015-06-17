/************************************************************************
Copyright (C) 2013 STMicroelectronics. All Rights Reserved.

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

Source file name : tsclient.c
Author :           Shobhit

tsserver LLA for PID filtering

Date        Modification                                    Name
----        ------------                                    --------
21-Jan-13    Created                                         Shobhit

************************************************************************/

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/socket.h>
#include <linux/in.h>
#include <linux/net.h>
#include <linux/slab.h>
#include <net/sock.h>
#include <net/inet_common.h>
#include <linux/syscalls.h>
#include <linux/uaccess.h>
#include <linux/list.h>
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
#include <stm_fe_diseqc.h>
#include <lla_utils.h>
#include "tsclient.h"

/*
 * Name: tsclient_init_ts()
 *
 * Description: Providing Initiliazing parameters to tsclient
 *
 */
int tsclient_init_ts(struct stm_fe_demod_s *priv, char *demux_name)
{

	int ret = 0;
	struct iovec *iov;
	struct msghdr *sock_msg;
	mm_segment_t old_mm;
	char *recv_msg;
	char *tmp;
	char **reply;
	int *list;
	int id = 0;
	int size = 0;
	int status;
	int num = 0;
	int j, i = 0;
	int cnt = 0;
	struct socket **sockt;

	sockt = (struct socket **)stm_fe_malloc(sizeof(struct socket *));
	if (!sockt) {
		pr_err("%s: mem alloc failed for iov\n", __func__);
		return -ENOMEM;
	}

	iov = (struct iovec *)stm_fe_malloc(sizeof(struct iovec));
	if (!iov) {
		pr_err("%s: mem alloc failed for iov\n", __func__);
		ret = -ENOMEM;
		goto sockt_deallocate;
	}
	sock_msg = stm_fe_malloc(sizeof(struct msghdr));
	if (!sock_msg) {
		pr_err("%s: mem alloc failed for sock_msg\n", __func__);
		ret = -ENOMEM;
		goto iov_deallocate;
	}

	recv_msg = (char *)stm_fe_malloc(MAX_MSG_SIZE);
	if (!recv_msg) {
		pr_err("%s: mem alloc failed for msg\n", __func__);
		ret = -ENOMEM;
		goto sock_deallocate;
	}
	reply = (char **)stm_fe_malloc(MAX_CMD_PARAM * sizeof(*reply));
	if (!reply) {
		pr_err("%s: mem alloc failed for reply\n", __func__);
		ret = -ENOMEM;
		goto recv_deallocate;
	}
	ret = transport_ts_setup(sockt, priv);
	if (ret < 0) {
		pr_err("%s: transport_ts_setup failed: %d\n", __func__, ret);
		goto reply_deallocate;
	}

	/*FRONTEND LIST */
	tmp = "PG_FRONTEND_LIST\n";
	size = strlen(tmp);
	memset(iov, 0, sizeof(struct iovec));
	memset(sock_msg, 0, sizeof(struct msghdr));
	iov[0].iov_base = (void *)tmp;
	iov[0].iov_len = size;
	sock_msg->msg_name = NULL;
	sock_msg->msg_namelen = 0;
	sock_msg->msg_iov = iov;
	sock_msg->msg_iovlen = 1;
	sock_msg->msg_control = NULL;
	sock_msg->msg_controllen = 0;
	sock_msg->msg_flags = MSG_WAITALL;
	/*Send packet over TCP socket */
	old_mm = get_fs();
	set_fs(KERNEL_DS);
resend_fl:
	ret = sock_sendmsg(*sockt, sock_msg, size);
	set_fs(old_mm);
	if (ret != size) {
		pr_err("%s: send msg failed: %d\n", __func__, ret);
		if (cnt < 10) {
			cnt++;
			WAIT_N_MS(50);
			goto resend_fl;
		} else {
			goto end;
		}
	} else {
		pr_info("%s: PG_FRONTEND_LIST over socket success: %d\n",
								__func__, ret);
	}

	iov[0].iov_base = recv_msg;
	iov[0].iov_len = MAX_MSG_SIZE;
	sock_msg->msg_iov = iov;
	old_mm = get_fs();
	set_fs(KERNEL_DS);
	cnt = 0;
	/*Receive packet from TCP socket */
recv_fl:
	ret = sock_recvmsg(*sockt, sock_msg, MAX_MSG_SIZE - 1, 0);
	set_fs(old_mm);
	if (ret < 0) {
		pr_err("%s: recv msg failed: %d\n", __func__, ret);
		if (cnt < 10) {
			cnt++;
			WAIT_N_MS(50);
			goto recv_fl;
		} else {
			goto end;
		}
	}
	pr_info("%s: PG_FRONTEND_LIST successful: %d\n", __func__, ret);
	pr_info("%s: {{%s}}\n", __func__, recv_msg);

	i = 0;
	reply[i++] = &(recv_msg[0]);
	for (j = 0; (j < ret-1) && (i < MAX_CMD_PARAM) ; j++) {
		if (recv_msg[j] == '\n') {
			recv_msg[j] = '\0';
			reply[i++] = &(recv_msg[j+1]);
		}
	}
	ret = kstrtoint(reply[0], 0, &status);
	if (ret) {
		pr_err("%s: err in strtoint conv: %d!", __func__, ret);
		goto end;
	}
	pr_info("{{status = %d}}\n", status);
	if (!status) {
		ret = kstrtoint(reply[1], 0, &num);
		if (ret) {
			pr_err("%s: err in strtoint conv: %d!", __func__, ret);
			goto end;
		}
		pr_info("%s: Number of Frontends= %d\n", __func__, num);
		if (num > 0) {
			list = (int *)stm_fe_malloc(num * sizeof(int));
			if (!list) {
				pr_err("%s:Memory allocation fail for list\n",
						__func__);
				ret = -ENOMEM;
				goto end;
			}
			parse_command(&reply[2], num, list);
			id = list[priv->demod_id];
			stm_fe_free((void **)&list);
		} else {
			ret = -ENODEV;
			goto end;
		}
	} else {
		pr_info("%s: status = %d msg = {{%s}}\n",
						    __func__, status, reply[1]);
		ret = -EFAULT;
		goto end;
	}

	old_mm = get_fs();
	set_fs(KERNEL_DS);
	ret = inet_release(*sockt);
	set_fs(old_mm);

	/*CREATE GROUP */
	tmp = (char *)stm_fe_malloc(MAX_STRING_SIZE);
	if (!tmp) {
		pr_err("%s: mem alloc failed for tmp\n", __func__);
		ret = -ENOMEM;
		goto end;
	}

	size = snprintf(tmp, MAX_STRING_SIZE, "PG_GRP_CREATE %s->%s,%d",
		 priv->demod_name, demux_name, id);
	if (size >= MAX_STRING_SIZE - 1) {
		pr_err("%s: too long command\n", __func__);
		ret = -ENOMEM;
		goto msg_deallocate;
	}
	ret = transport_ts_setup(sockt, priv);
	if (ret < 0) {
		pr_err("%s: transport_ts_setup failed: %d\n", __func__, ret);
		stm_fe_free((void **)&tmp);
		goto reply_deallocate;
	}

	memset(iov, 0, sizeof(struct iovec));
	memset(sock_msg, 0, sizeof(struct msghdr));
	pr_info("{%s}\n", tmp);
	iov[0].iov_base = (void *)tmp;
	iov[0].iov_len = size;
	sock_msg->msg_name = NULL;
	sock_msg->msg_namelen = 0;
	sock_msg->msg_iov = iov;
	sock_msg->msg_iovlen = 1;
	sock_msg->msg_control = NULL;
	sock_msg->msg_controllen = 0;
	sock_msg->msg_flags = MSG_WAITALL;
	/*Send packet over TCP socket */
	old_mm = get_fs();
	set_fs(KERNEL_DS);
	cnt = 0;
resend_pg:
	ret = sock_sendmsg(*sockt, sock_msg, size);
	set_fs(old_mm);
	if (ret != size) {
		pr_err("%s: send msg failed: %d\n", __func__, ret);
		if (cnt < 10) {
			cnt++;
			WAIT_N_MS(50);
			goto resend_pg;
		} else {
			goto msg_deallocate;
		}
	} else {
		pr_info("%s: PG_GRP_CREATE over socket: success= %d\n",
								 __func__, ret);
	}

	iov[0].iov_base = recv_msg;
	iov[0].iov_len = MAX_MSG_SIZE;
	sock_msg->msg_iov = iov;
	old_mm = get_fs();
	set_fs(KERNEL_DS);
	cnt = 0;
	/*Receive packet from TCP socket */
recv_pg:
	ret = sock_recvmsg(*sockt, sock_msg, MAX_MSG_SIZE - 1, 0);
	set_fs(old_mm);
	if (ret < 0) {
		pr_err("%s: recv msg failed: %d\n", __func__, ret);
		if (cnt < 10) {
			cnt++;
			WAIT_N_MS(50);
			goto recv_pg;
		} else {
			goto msg_deallocate;
		}
	}
	pr_info("%s: PG_GRP_CREATE successful: %d\n", __func__, ret);
	pr_info("%s: {{%s}}\n", __func__, recv_msg);
	i = 0;
	reply[i++] = &(recv_msg[0]);
	for (j = 0; j < ret-1 && (i < MAX_CMD_PARAM); j++) {
		if (recv_msg[j] == '\n') {
			recv_msg[j] = '\0';
			reply[i++] = &(recv_msg[j+1]);
		}
	}
	ret = kstrtoint(reply[0], 0, &status);
	if (ret) {
		pr_err("%s: err in strtoint conv: %d!", __func__, ret);
		goto msg_deallocate;
	}
	pr_info("{{status = %d}}\n", status);
	if (!status) {
		snprintf(priv->grp_name, STM_REGISTRY_MAX_TAG_SIZE, "%s->%s",
				priv->demod_name, demux_name);
	} else if (status == 2) {
		pr_info("%s: Frontend already exists\n", __func__);
		snprintf(priv->grp_name, STM_REGISTRY_MAX_TAG_SIZE, "%s->%s",
				priv->demod_name, demux_name);
	} else {
		pr_info("%s: Frontend doesn't exists\n", __func__);
		pr_info("%s: status = %d msg = {{%s}}\n",
						    __func__, status, reply[1]);
		ret = -ENODEV;
	}

msg_deallocate:
	stm_fe_free((void **)&tmp);
end:
	old_mm = get_fs();
	set_fs(KERNEL_DS);
	inet_release(*sockt);
	set_fs(old_mm);
reply_deallocate:
	stm_fe_free((void **)&reply);
recv_deallocate:
	stm_fe_free((void **)&recv_msg);
sock_deallocate:
	stm_fe_free((void **)&sock_msg);
iov_deallocate:
	stm_fe_free((void **)&iov);
sockt_deallocate:
	stm_fe_free((void **)&sockt);

	return ret;
}
EXPORT_SYMBOL(tsclient_init_ts);

/*
 * Name: tsclient_pid_configure()
 *
 * Description: Function handler for PID configuration
 *
 */
int tsclient_pid_configure(stm_object_h demod_obj, stm_object_h demux_obj,
		       uint32_t pid,  char *filter_state)
{
	int ret = -1;
	struct stm_fe_demod_s *priv;
	struct iovec *iov;
	struct msghdr *sock_msg;
	mm_segment_t old_mm;
	char *tmp;
	char *recv_msg;
	char **reply;
	int size = 0;
	int status;
	int j, i;
	int cnt = 0;
	struct socket **sockt;

	if (demod_obj)
		priv = ((struct stm_fe_demod_s *)demod_obj);
	else
		return -EINVAL;

	sockt = (struct socket **)stm_fe_malloc(sizeof(struct socket *));
	if (!sockt) {
		pr_err("%s: mem alloc failed for iov\n", __func__);
		return -ENOMEM;
	}

	iov = (struct iovec *)stm_fe_malloc(sizeof(struct iovec));
	if (!iov) {
		pr_err("%s: mem alloc failed for iov\n", __func__);
		ret = -ENOMEM;
		goto sockt_deallocate;
	}
	sock_msg = (struct msghdr *)stm_fe_malloc(sizeof(struct msghdr));
	if (!sock_msg) {
		pr_err("%s: mem alloc failed for sock_msg\n", __func__);
		ret = -ENOMEM;
		goto iov_deallocate;
	}
	tmp = (char *)stm_fe_malloc(MAX_STRING_SIZE);
	if (!tmp) {
		pr_err("%s: mem alloc failed for tmp\n", __func__);
		ret = -ENOMEM;
		goto sock_deallocate;
	}
	recv_msg = (char *)stm_fe_malloc(MAX_MSG_SIZE);
	if (!recv_msg) {
		pr_err("%s: mem alloc failed for msg\n", __func__);
		ret = -ENOMEM;
		goto msg_deallocate;
	}
	reply = (char **)stm_fe_malloc(MAX_CMD_PARAM * sizeof(*reply));
	if (!reply) {
		pr_err("%s: mem alloc failed for reply\n", __func__);
		ret = -ENOMEM;
		goto recv_deallocate;
	}
	size = snprintf(tmp, MAX_STRING_SIZE, "PG_PID_ADD %s,%d,%s",
			priv->grp_name, pid, filter_state);
	if (size >= MAX_STRING_SIZE-1) {
		pr_err("%s: too long command\n", __func__);
		ret = -ENAMETOOLONG;
		goto reply_deallocate;
	}
	ret = transport_ts_setup(sockt, priv);
	if (ret < 0) {
		pr_err("%s: transport_ts_setup failed: %d\n", __func__, ret);
		goto reply_deallocate;
	}
	memset(iov, 0, sizeof(struct iovec));
	memset(sock_msg, 0, sizeof(struct msghdr));
	pr_info("{%s}\n", tmp);
	iov[0].iov_base = (void *)tmp;
	iov[0].iov_len = size;
	sock_msg->msg_name = NULL;
	sock_msg->msg_namelen = 0;
	sock_msg->msg_iov = iov;
	sock_msg->msg_iovlen = 1;
	sock_msg->msg_control = NULL;
	sock_msg->msg_controllen = 0;
	sock_msg->msg_flags = MSG_WAITALL;
	/*Send packet over TCP socket */
	old_mm = get_fs();
	set_fs(KERNEL_DS);
resend_pid:
	ret = sock_sendmsg(*sockt, sock_msg, size);
	set_fs(old_mm);
	if (ret != size) {
		pr_err("%s: send msg failed: %d\n", __func__, ret);
		if (cnt < 10) {
			cnt++;
			WAIT_N_MS(50);
			goto resend_pid;
		} else {
			goto end;
		}
	} else {
		pr_info("%s: PG_PID_ADD over socket:success = %d\n",
								__func__, ret);
	}

	iov[0].iov_base = recv_msg;
	iov[0].iov_len = MAX_MSG_SIZE;
	sock_msg->msg_iov = iov;
	old_mm = get_fs();
	set_fs(KERNEL_DS);
	cnt = 0;
	/*Receive packet from TCP socket */
recv_pid:
	ret = sock_recvmsg(*sockt, sock_msg, MAX_MSG_SIZE - 1, 0);
	set_fs(old_mm);
	if (ret < 0) {
		pr_err("%s: recv msg failed: %d\n", __func__, ret);
		if (cnt < 10) {
			cnt++;
			WAIT_N_MS(50);
			goto recv_pid;
		} else {
			goto end;
		}
	}
	pr_info("PG_PID_ADD successful: %d\n", ret);
	pr_info("{{%s}}\n", recv_msg);
	i = 0;
	reply[i++] = &(recv_msg[0]);
	for (j = 0; (j < ret-1) && (i < MAX_CMD_PARAM) ; j++) {
		if (recv_msg[j] == '\n') {
			recv_msg[j] = '\0';
			reply[i++] = &(recv_msg[j+1]);
		}
	}
	ret = kstrtoint(reply[0], 0, &status);
	if (ret) {
		pr_err("%s: err in strtoint conv: %d!", __func__, ret);
		goto end;
	}
	if (status) {
		pr_info("%s: Frontend grp doesn't exist\n", __func__);
		pr_info("%s: status = %d msg = {{%s}}\n",
						    __func__, status, reply[1]);
		ret = -ENODEV;
	} else {
		pr_info("%s: PID configure : SUCCESS\n", __func__);
	}

end:
	old_mm = get_fs();
	set_fs(KERNEL_DS);
	inet_release(*sockt);
	set_fs(old_mm);
reply_deallocate:
	stm_fe_free((void **)&reply);
recv_deallocate:
	stm_fe_free((void **)&recv_msg);
msg_deallocate:
	stm_fe_free((void **)&tmp);
sock_deallocate:
	stm_fe_free((void **)&sock_msg);
iov_deallocate:
	stm_fe_free((void **)&iov);
sockt_deallocate:
	stm_fe_free((void **)&sockt);

	return ret;
}

/*
 * Name: tsclient_pid_start()
 *
 * Description: Function handler for PID configuration
 *
 */
int tsclient_pid_start(stm_object_h demod_obj, stm_object_h demux_obj,
		       uint32_t pid)
{
	return tsclient_pid_configure(demod_obj, demux_obj, pid, "STARTED");
}
EXPORT_SYMBOL(tsclient_pid_start);
/*
 * Name: tsclient_pid_stop()
 *
 * Description: Function handler for PID configuration
 *
 */
int tsclient_pid_stop(stm_object_h demod_obj, stm_object_h demux_obj,
		      uint32_t pid)
{
	return tsclient_pid_configure(demod_obj, demux_obj, pid, "STOPPED");
}
EXPORT_SYMBOL(tsclient_pid_stop);

/*
 * Name: tsclient_term_ts()
 *
 * Description: Deletion of PID and gp name
 *
 */
int tsclient_term_ts(struct stm_fe_demod_s *priv, char *demux_name)
{

	int ret = -1;
	struct iovec *iov;
	struct msghdr *sock_msg;
	mm_segment_t old_mm;
	char *tmp;
	char *recv_msg;
	char **reply;
	int size = 0;
	int status;
	int j, i;
	int cnt = 0;
	struct socket **sockt;

	sockt = (struct socket **)stm_fe_malloc(sizeof(struct socket *));
	if (!sockt) {
		pr_err("%s: mem alloc failed for iov\n", __func__);
		return -ENOMEM;
	}

	iov = (struct iovec *)stm_fe_malloc(sizeof(struct iovec));
	if (!iov) {
		pr_err("%s: mem alloc failed for iov\n", __func__);
		stm_fe_free((void **)&sockt);
		return -ENOMEM;
	}
	sock_msg = (struct msghdr *)stm_fe_malloc(sizeof(struct msghdr));
	if (!sock_msg) {
		pr_err("%s: mem alloc failed for sock_msg\n", __func__);
		stm_fe_free((void **)&sockt);
		stm_fe_free((void **)&iov);
		return -ENOMEM;
	}
	ret = transport_ts_setup(sockt, priv);
	if (ret < 0) {
		pr_err("%s: transport_ts_setup failed: %d\n", __func__, ret);
		stm_fe_free((void **)&sock_msg);
		stm_fe_free((void **)&sockt);
		stm_fe_free((void **)&iov);
		return ret;
	}
	/*CREATE GROUP */
	tmp = (char *)stm_fe_malloc(MAX_STRING_SIZE);
	if (!tmp) {
		pr_err("%s: mem alloc failed for tmp\n", __func__);
		stm_fe_free((void **)&sock_msg);
		stm_fe_free((void **)&sockt);
		stm_fe_free((void **)&iov);
		return -ENOMEM;
	}
	recv_msg = (char *)stm_fe_malloc(MAX_MSG_SIZE);
	if (!recv_msg) {
		pr_err("%s: mem alloc failed for msg\n", __func__);
		stm_fe_free((void **)&tmp);
		stm_fe_free((void **)&sock_msg);
		stm_fe_free((void **)&sockt);
		stm_fe_free((void **)&iov);
		return -ENOMEM;
	}
	reply = (char **)stm_fe_malloc(MAX_CMD_PARAM * sizeof(*reply));
	if (!reply) {
		pr_err("%s: mem alloc failed for reply\n", __func__);
		stm_fe_free((void **)&recv_msg);
		stm_fe_free((void **)&tmp);
		stm_fe_free((void **)&sock_msg);
		stm_fe_free((void **)&sockt);
		stm_fe_free((void **)&iov);
		return -ENOMEM;
	}

	size = snprintf(tmp, MAX_STRING_SIZE, "PG_GRP_DEL %s", priv->grp_name);
	if (size >= MAX_STRING_SIZE-1) {
		pr_err("%s: too long command\n", __func__);
		stm_fe_free((void **)&reply);
		stm_fe_free((void **)&recv_msg);
		stm_fe_free((void **)&tmp);
		stm_fe_free((void **)&sock_msg);
		stm_fe_free((void **)&sockt);
		stm_fe_free((void **)&iov);
		return -ENAMETOOLONG;
	}
	memset(iov, 0, sizeof(struct iovec));
	memset(sock_msg, 0, sizeof(struct msghdr));
	pr_info("%s: {%s}\n", __func__, tmp);
	iov[0].iov_base = (void *)tmp;
	iov[0].iov_len = size;
	sock_msg->msg_name = NULL;
	sock_msg->msg_namelen = 0;
	sock_msg->msg_iov = iov;
	sock_msg->msg_iovlen = 1;
	sock_msg->msg_control = NULL;
	sock_msg->msg_controllen = 0;
	sock_msg->msg_flags = MSG_WAITALL;
	/*Send packet over TCP socket */
	old_mm = get_fs();
	set_fs(KERNEL_DS);
resend_term:
	ret = sock_sendmsg(*sockt, sock_msg, size);
	set_fs(old_mm);
	if (ret != size) {
		pr_err("%s: send msg failed: %d\n", __func__, ret);
		if (cnt < 10) {
			cnt++;
			WAIT_N_MS(50);
			goto resend_term;
		} else {
			stm_fe_free((void **)&reply);
			stm_fe_free((void **)&recv_msg);
			stm_fe_free((void **)&tmp);
			stm_fe_free((void **)&sock_msg);
			stm_fe_free((void **)&sockt);
			stm_fe_free((void **)&iov);
			return ret;
		}
	} else {
		pr_info("%s: PG_PID_ADD over socket: success= %d\n",
								 __func__, ret);
	}

	iov[0].iov_base = recv_msg;
	iov[0].iov_len = MAX_MSG_SIZE;
	sock_msg->msg_iov = iov;
	old_mm = get_fs();
	set_fs(KERNEL_DS);
	cnt = 0;
	/*Receive packet from TCP socket */
recv_term:
	ret = sock_recvmsg(*sockt, sock_msg, MAX_MSG_SIZE - 1, 0);
	set_fs(old_mm);
	if (ret < 0) {
		pr_err("%s: recv msg failed: %d\n", __func__, ret);
		if (cnt < 10) {
			cnt++;
			WAIT_N_MS(50);
			goto recv_term;
		} else {
			goto end;
		}
	}
	pr_info("%s: PG_PID_DEL successful: %d\n", __func__, ret);
	pr_info("{{%s}}\n", recv_msg);
	i = 0;
	reply[i++] = &(recv_msg[0]);
	for (j = 0; (j < ret-1) && (i < MAX_CMD_PARAM) ; j++) {
		if (recv_msg[j] == '\n') {
			recv_msg[j] = '\0';
			reply[i++] = &(recv_msg[j+1]);
		}
	}
	ret = kstrtoint(reply[0], 0, &status);
	if (ret) {
		pr_err("%s: err in strtoint conv: %d!", __func__, ret);
		goto end;
	}
	if (status) {
		pr_info("%s: Frontend grp doesn't exist\n", __func__);
		pr_info("%s: status = %d msg = {{%s}}\n",
						    __func__, status, reply[1]);
		ret = -ENODEV;
	} else {
		pr_info("%s: PID Deletion : SUCCESS\n", __func__);
	}
end:
	old_mm = get_fs();
	set_fs(KERNEL_DS);
	inet_release(*sockt);
	set_fs(old_mm);
	stm_fe_free((void **)&reply);
	stm_fe_free((void **)&recv_msg);
	stm_fe_free((void **)&tmp);
	stm_fe_free((void **)&sock_msg);
	stm_fe_free((void **)&sockt);
	stm_fe_free((void **)&iov);

	return ret;
}
EXPORT_SYMBOL(tsclient_term_ts);

static int32_t __init tsserver_lla_init(void)
{
	pr_info("Loading tsserver client module...\n");

	return 0;
}

module_init(tsserver_lla_init);

static void __exit tsserver_lla_term(void)
{
	pr_info("Removing tsserver client module ...\n");

}

module_exit(tsserver_lla_term);

MODULE_DESCRIPTION("Low level driver for tsserver client module");
MODULE_AUTHOR("STMicroelectronics Limited");
MODULE_LICENSE("GPL");
