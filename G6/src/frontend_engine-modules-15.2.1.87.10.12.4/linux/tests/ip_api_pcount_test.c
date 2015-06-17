/************************************************************************
Copyright (C) 2014 STMicroelectronics. All Rights Reserved.

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

Source file name : ip_api_pcount_test.c
Author :           RS

Test file to use stm fe ip object

Date        Modification					Name
----        ------------                                        -------
12-Mar-14   Created						Rupesh

************************************************************************/

#include <linux/module.h>
#include <linux/delay.h>
#include <stm_fe.h>
#include <stm_te.h>

int port_no = 8000;
char *ipaddr = "192.168.24.8";
short protocol = 2;

module_param(port_no, int, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
MODULE_PARM_DESC(port_no, "port no");
module_param(ipaddr, charp, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
MODULE_PARM_DESC(ipaddr, "ip address");
module_param(protocol, short, 0000);
MODULE_PARM_DESC(mystring, "protocol: 1 rtp: 2 udp");

stm_fe_ip_h obj;

void pkt_reset_info(stm_fe_ip_stat_t *info_p, int32_t ret)
{
	if (ret) {
		pr_err("%s: stm_fe_ip_get_stats failed", __func__);
		return;
	}
	pr_info("%s: *******************************\n", __func__);
	pr_info("packet_count = %d\n", info_p->packet_count);
	pr_info("lost_packet_count = %d\n", info_p->lost_packet_count);
	pr_info("bad_frag_count = %d\n", info_p->bad_frag_count);
	pr_info("bad_length_count = %d\n", info_p->bad_length_count);
	pr_info("%s: *******************************\n", __func__);
}

static int32_t __init stm_fe_ip_api_test_init(void)
{
	int32_t ret;
	int32_t i = 0;
	uint32_t cnt;

	stm_fe_object_info_t *fe_objs;
	stm_fe_ip_connection_t connect_params;
	stm_fe_ip_stat_t info_p;
	stm_te_object_h demux_object;

	/*initialize IP handle*/
	obj = 0;

	pr_info("Loading API test module...\n");

	ret = stm_te_demux_new("demux0", &demux_object);

	ret = stm_fe_discover(&fe_objs, &cnt);
	if (ret)
		pr_info("%s: discover fail\n", __func__);

	for (i = 0; i < cnt; i++) {
		pr_info("Searching IP FE:count %d : type %d\n",
							 i, fe_objs[i].type);
		if (fe_objs[i].type == STM_FE_IP)  {
			pr_info("-- create IP object--\n");
			ret = stm_fe_ip_new(fe_objs[i].stm_fe_obj,
						NULL, &obj);
			if (ret)
				pr_err("%s: stm_fe_ip_new failed", __func__);
			break;
		}
	}

	ret = stm_te_demux_start(demux_object);
	ret = stm_fe_ip_attach(obj, demux_object);
	if (ret)
		pr_err("error in attaching handle 0: %d", ret);

	/*fill up the connect params*/
	pr_info("%s: configuring the IP FE started\n", __func__);
	strncpy(connect_params.ip, ipaddr, sizeof(connect_params.ip)-1);
	connect_params.ip[sizeof(connect_params.ip)-1] = '\0';
	connect_params.port_no = port_no;
	connect_params.protocol = protocol;
	ret = stm_fe_ip_set_compound_control(obj,
				STM_FE_IP_CTRL_SET_CONFIG, &connect_params);
	if (ret)
		pr_err("%s: stm_fe_ip_set_compound_control failed", __func__);

	/*start the ip fe*/
	ret = stm_fe_ip_start(obj);
	if (ret)
		pr_err("%s: stm_fe_ip_start failed", __func__);

	mdelay(6000);

	ret = stm_fe_ip_get_stats(obj, &info_p);
	pkt_reset_info(&info_p, ret);

	ret = stm_fe_ip_set_compound_control(obj,
				STM_FE_IP_CTRL_RESET_STATS, &connect_params);
	if (ret)
		pr_err("%s: stm_fe_ip_set_compound_control failed to reset",
								 __func__);

	ret = stm_fe_ip_get_stats(obj, &info_p);
	pkt_reset_info(&info_p, ret);

	mdelay(6000);

	ret = stm_fe_ip_get_stats(obj, &info_p);
	pkt_reset_info(&info_p, ret);

	ret = stm_fe_ip_set_compound_control(obj,
				STM_FE_IP_CTRL_RESET_STATS, &connect_params);
	if (ret)
		pr_err("%s: stm_fe_ip_set_compound_control failed to reset",
								 __func__);

	ret = stm_fe_ip_get_stats(obj, &info_p);
	pkt_reset_info(&info_p, ret);

	ret = stm_fe_ip_stop(obj);
	if (ret)
		pr_err("%s: stm_fe_ip_stop failed", __func__);

	ret = stm_te_demux_stop(demux_object);

	ret = stm_fe_ip_detach(obj, demux_object);
	if (ret)
		pr_err("error in detaching handle 0: %d", ret);

	ret = stm_fe_ip_delete(obj);
	if (ret)
		pr_err("%s: stm_fe_ip_delete failed", __func__);

	ret = stm_te_demux_delete(demux_object);

	return 0;
}

module_init(stm_fe_ip_api_test_init);

static void __exit stm_fe_ip_api_test_term(void)
{
	pr_info("\nRemoving stmfe API test module ...\n");
}

module_exit(stm_fe_ip_api_test_term);

MODULE_DESCRIPTION("Frontend engine test module for STKPI IP frontend");
MODULE_AUTHOR("STMicroelectronics Limited");
MODULE_LICENSE("GPL");
