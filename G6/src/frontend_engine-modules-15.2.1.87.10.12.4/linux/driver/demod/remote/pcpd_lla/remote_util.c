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

Source file name : remote_util.c
Author :           Shobhit

PCPD LLA for remote tuning

Date        Modification                                    Name
----        ------------                                    --------
1-Nov-12   Created                                         Rahul

************************************************************************/
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/socket.h>
#include <linux/in.h>
#include <linux/net.h>
#include <linux/slab.h>
#include <net/sock.h>
#include <net/inet_common.h>
#include <linux/netdevice.h>
#include <linux/inetdevice.h>
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
#include "d0remote.h"

/* * Name: store_data_le32()
*
* Description:store the 32bit value in LE format
*/
void store_data_le32(u_int8_t *msg, uint32_t value)
{
	msg[0] = value & 0xFF;
	msg[1] = (value >> 8) & 0xFF;
	msg[2] = (value >> 16) & 0xFF;
	msg[3] = (value >> 24) & 0xFF;
}

/*
 * Name: load_data_le32()
 *
 * Description:calculate the 32bit value in LE format
 */
unsigned int load_data_le32(uint8_t *msg)
{
	unsigned int value;

	value = msg[0] | (msg[1] << 8) | (msg[2] << 16) | (msg[3] << 24);

	return value;
}

/*
 * Name: load_data_le16()
 *
 * Description:calculate the 16bit value in LE format
 */
u_int16_t load_data_le16(u_int8_t *msg)
{
	u_int16_t value;

	value = msg[0] | (msg[1] << 8);

	return value;
}

/*
 * Name: pcpd_create_msg()
 *
 * Description:pcpd message creation
 */
void pcpd_create_msg(struct stm_fe_demod_s *priv,
				struct pcpd_packet_data_t *pcpd_msg)
{
	static uint16_t txn = 1;

	pcpd_msg->version = PCPD_CM_VERSION;
	pcpd_msg->type = PCPD_COMMAND_TYPE;
	pcpd_msg->offset = 0;
	pcpd_msg->destination = ((CPUID_ECM << 8) | (ROLEID_ECM));
	pcpd_msg->source = (CPUID_ESTB << 8) | (ROLEID_ESTB);
	pcpd_msg->txn_id = txn;
	pcpd_msg->magic_number = MAGIC_NUM;
	pcpd_msg->cmd_id = PCPD_REMOTE_TUNER_REQUEST;
	priv->fe_remote.txn_id = txn;
	txn++;
	if (txn >= 255)
		txn = 1;
}

/*
 * Name: transport_setup()
 *
 * Description:socket creation for sync/async messages
 */
int transport_setup(struct stm_fe_demod_s *priv)
{
	int ret = 0;
	struct sockaddr_in *addr_in;
	struct net_device *device = NULL;
	struct in_device *in_dev = NULL;
	struct in_ifaddr *if_info = NULL;

	ret = sock_create_kern(AF_INET, SOCK_DGRAM, 0, &priv->fe_remote.sock);
	if (ret < 0) {
		pr_err("%s: socket failed: %d\n", __func__, ret);
		return ret;
	}

	device = __dev_get_by_name(&init_net, priv->config->eth_name);
	if (device)
		in_dev = (struct in_device *)device->ip_ptr;
	if (in_dev)
		if_info = in_dev->ifa_list;

	if (!if_info) {
		pr_err("%s: %s - interface not found\n", __func__,
							priv->config->eth_name);
		return -ENODEV;
	}

	addr_in = (struct sockaddr_in *)
				stm_fe_malloc(sizeof(struct sockaddr_in));
	if (!addr_in) {
		pr_err("%s: mem alloc failed for addr_in\n", __func__);
		return -ENOMEM;
	}
	memset(addr_in, 0, sizeof(struct sockaddr_in));
	addr_in->sin_family = AF_INET;
	addr_in->sin_port = htons(ESTB_PORT_SYNC_CMD);
	addr_in->sin_addr.s_addr = if_info->ifa_local;
	memset(&(addr_in->sin_zero), '\0', 8);

	ret = priv->fe_remote.sock->ops->bind(priv->fe_remote.sock,
			(struct sockaddr *) addr_in, sizeof(struct sockaddr));
	if (ret < 0) {
		pr_err("%s: binding of socket failed: %d\n", __func__, ret);
		kfree(addr_in);
		return ret;
	}

	addr_in->sin_port = htons(ECM_PORT_SYNC_CMD);
	addr_in->sin_addr.s_addr = htonl(ECM_IP_ADDR);
	ret = priv->fe_remote.sock->ops->connect(priv->fe_remote.sock,
		(struct sockaddr *) addr_in, sizeof(struct sockaddr), 0);
	if (ret < 0) {
		pr_err("%s: connection of socket failed: %d\n", __func__, ret);
		kfree(addr_in);
		return ret;
	}

	return ret;
}

uint32_t remote_modulationmap(stm_fe_demod_modulation_t mod)
{
	uint32_t lla_mod = 0;

	switch (mod) {
	case STM_FE_DEMOD_MODULATION_4QAM:
		lla_mod = STPCPD_REMOTE_MOD_QAM;
		break;
	case STM_FE_DEMOD_MODULATION_16QAM:
		lla_mod = STPCPD_REMOTE_MOD_16QAM;
		break;
	case STM_FE_DEMOD_MODULATION_32QAM:
		lla_mod = STPCPD_REMOTE_MOD_32QAM;
		break;
	case STM_FE_DEMOD_MODULATION_64QAM:
		lla_mod = STPCPD_REMOTE_MOD_64QAM;
		break;
	case STM_FE_DEMOD_MODULATION_128QAM:
		lla_mod = STPCPD_REMOTE_MOD_128QAM;
		break;
	case STM_FE_DEMOD_MODULATION_256QAM:
		lla_mod = STPCPD_REMOTE_MOD_256QAM;
		break;
	default:
		lla_mod = 0xFFFFFFFF;
		break;
	}

	return lla_mod;
}

stm_fe_demod_modulation_t remote_getmodulation(uint32_t mod)
{
	stm_fe_demod_modulation_t fe_mod = STM_FE_DEMOD_MODULATION_NONE;

	switch (mod) {
	case STPCPD_REMOTE_MOD_4QAM:
		fe_mod = STM_FE_DEMOD_MODULATION_4QAM;
		break;
	case STPCPD_REMOTE_MOD_16QAM:
		fe_mod = STM_FE_DEMOD_MODULATION_16QAM;
		break;
	case STPCPD_REMOTE_MOD_32QAM:
		fe_mod = STM_FE_DEMOD_MODULATION_32QAM;
		break;
	case STPCPD_REMOTE_MOD_64QAM:
		fe_mod = STM_FE_DEMOD_MODULATION_64QAM;
		break;
	case STPCPD_REMOTE_MOD_128QAM:
		fe_mod = STM_FE_DEMOD_MODULATION_128QAM;
		break;
	case STPCPD_REMOTE_MOD_256QAM:
		fe_mod = STM_FE_DEMOD_MODULATION_256QAM;
		break;
	default:
		fe_mod = STM_FE_DEMOD_MODULATION_NONE;
		break;
	}

	return fe_mod;
}
