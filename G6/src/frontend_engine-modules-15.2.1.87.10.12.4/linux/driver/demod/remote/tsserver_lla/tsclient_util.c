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

Source file name : tsclient_util.c
Author :           Shobhit

Tsserver LLA utility file for PID filtering

Date        Modification                                    Name
----        ------------                                    --------
20-Jan-13   Created                                         shobhit

************************************************************************/
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/socket.h>
#include <linux/in.h>
#include <linux/net.h>
#include <linux/slab.h>
#include <net/sock.h>
#include <net/inet_common.h>
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
#include "tsclient.h"

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
 * Name: transport_setup()
 *
 * Description:socket creation for sync/async messages
 */
int transport_ts_setup(struct socket **sockt, struct stm_fe_demod_s *priv)
{
	int ret = 0;
	struct sockaddr_in *addr_in;
	struct socket *sock_temp;
	mm_segment_t old_mm;

	ret = sock_create_kern(AF_INET, SOCK_STREAM, 0, sockt);
	if (ret < 0) {
		pr_err("%s: socket failed: %d\n", __func__, ret);
		return ret;
	}
	sock_temp = *sockt;
	addr_in =
	    (struct sockaddr_in *)stm_fe_malloc(sizeof(struct sockaddr_in));
	if (!addr_in) {
		pr_err("%s: mem alloc failed for addr_in\n", __func__);
		return -ENOMEM;
	}
	memset(addr_in, 0, sizeof(struct sockaddr_in));
	addr_in->sin_family = AF_INET;
	addr_in->sin_port = htons(priv->config->remote_port);
	addr_in->sin_addr.s_addr = htonl(priv->config->remote_ip);
	ret =
	    sock_temp->ops->connect(*sockt, (struct sockaddr *)addr_in,
				    sizeof(struct sockaddr), 0);
	if (ret < 0) {
		pr_err("%s: connection of socket failed: %d\n", __func__, ret);
		old_mm = get_fs();
		set_fs(KERNEL_DS);
		inet_release(*sockt);
		set_fs(old_mm);
		stm_fe_free((void **)&addr_in);
		return ret;
	} else
		pr_info("%s: socket connect ok: %d\n", __func__, ret);
	stm_fe_free((void **)&addr_in);
	return ret;
}

void parse_command(char **str, int num, int *list)
{
	int i;
	int j;

	for (i = 0; i < num; i++) {
		j = 0;
		list[i] = 0;
		while ('0' <= str[i][j] && str[i][j] <= '9') {
			list[i] = (list[i] * 10) + (str[i][j] - '0');
			j++;
		}
		pr_info("%s: Frontend list: %d\n", __func__, list[i]);
	}
}
