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

Source file name : stats.c
Author :           SD

fe ip statistics reporting via sysfs

Date        Modification                                    Name
----        ------------                                    --------
05-May-12   Created                                         SD

************************************************************************/
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
#include <stm_registry.h>
#include <stm_fe_ip.h>
#include "stats.h"
#include <linux/log2.h>

#define STM_REGISTRY_IPFE_INFO "dt_ipfe_info"
#define STM_REGISTRY_IPFE_STATS "dt_ipfe_stats"
#define STM_REGISTRY_IPFE_BUFF_STATS "dt_ipfe_buff_stats"
#define IPFE_CONNECTION_INFO "ipfe_connection_info"
#define IPFE_STATS "ipfe_stats"
#define IPFE_JITTER_CONF "jitter_config"
#define IPFE_BUFF_STATS "ipfe_buff_stats"

#define NO_JITTER      0
#define LOW_JITTER     1
#define MED_JITTER     2
#define HIGH_JITTER    3


char *get_str(int val)
{
	char *str;

	switch (val) {
	case 1:
		str = "TS-RTP";
	break;

	case 2:
		str = "TS-UDP";
	break;

	default:
		str = NULL;
	}
	return str;
}
/*
* ipfe_jitter_conf_print__handler: helper function to print ip jitter config
* specific info inside sysfs
*/
int ipfe_jitter_conf_print_handler(stm_object_h object, char *buf, size_t size,
					      char *user_buf, size_t user_size)
{
	int total_write = 0;
	struct stm_fe_ip_s *ipfe = object;

	total_write = scnprintf(user_buf, user_size, "%d\n", ipfe->jitter_conf);

	return total_write;
}
/*
* ipfe_jitter_conf_store__handler: helper function to store ip jitter config
* specific info inside sysfs
*/
int ipfe_jitter_conf_store_handler(stm_object_h object, char *buf, size_t size,
					const char *user_buf, size_t user_size)
{
	struct stm_fe_ip_s *ipfe;
	long unsigned int jitter_conf;
	int ret;

	ipfe = object;
	if (ipfe->ip_flags.start_active == true) {
		pr_err("%s: can't change Jitter conf. after ip_start\n",
								      __func__);
		return -EINVAL;
	}

	ret = kstrtoul(user_buf, 10, &jitter_conf);
	if (jitter_conf != NO_JITTER && jitter_conf != LOW_JITTER &&
		jitter_conf != MED_JITTER && jitter_conf != HIGH_JITTER) {
		pr_err("%s: jitter conf val is unsupported\n", __func__);
		return -EINVAL;
	}
	ipfe->jitter_conf = jitter_conf;
	return user_size;
}
/*
* ipfe_info_print_handler: helper function to print ip fe connection
* specific info inside sysfs
*/
int ipfe_info_print_handler(stm_object_h object, char *buf, size_t size,
					char *user_buf, size_t user_size)
{
	int total_write = 0;
	struct stm_fe_ip_s *ipfe = object;

	total_write = scnprintf(user_buf,
				user_size,
				"Connection params info:%s\n",
				"stm_fe_ip");

	total_write += scnprintf(user_buf + total_write,
				user_size - total_write,
				"\n");

	if (ipfe->params->is_ipv6) {
		total_write += scnprintf(user_buf + total_write,
				user_size - total_write,
				"IP Address(ipv6):%pI6c\n"
				"Port:%d\n"
				"Protocol:%s\n",
				(void *)(&ipfe->params->ipv6addr),
				ipfe->params->port_no,
				get_str(ipfe->params->protocol)
				);
	} else {
		total_write += scnprintf(user_buf + total_write,
					user_size - total_write,
					"IP Address(ipv4):%pI4\n"
					"Port:%d\n"
					"Protocol:%s\n",
					(void *)(&ipfe->params->ipaddr),
					ipfe->params->port_no,
					get_str(ipfe->params->protocol)
					);
	}

	total_write += scnprintf(user_buf + total_write,
				user_size - total_write,
				"\n");

	return total_write;
}

/*
* ipfe_stats_print_handler: helper function to print ip fe statistical
* info inside sysfs
*/
int ipfe_stats_print_handler(stm_object_h object, char *buf, size_t size,
					char *user_buf, size_t user_size)
{
	int total_write = 0;
	struct stm_fe_ip_s *ipfe = object;

	total_write = scnprintf(user_buf,
				user_size,
				"Packet reception: statistical info:%s\n",
				"stm_fe_ip");

	total_write += scnprintf(user_buf + total_write,
				user_size - total_write,
				"\n");

	total_write += scnprintf(user_buf + total_write,
				user_size - total_write,
				"total packet in:%d\n"
				"total packet freed:%d\n"
				"lost packet count:%d\n",
				/*"bad frag count:%d\n"
				 "bad length count:%d\n",*/
				ipfe->ip_data.stats.packet_count,
				ipfe->ip_data.stats.total_pkts_freed,
				ipfe->ip_data.stats.lost_packet_count
				/*ipfe->ip_stat.bad_frag_count,
				 ipfe->ip_stat.bad_length_count*/
				);

	total_write += scnprintf(user_buf + total_write,
				user_size - total_write,
				"\n");

	return total_write;
}

/*
* ipfe_buff_stats_print_handler: helper function to print ip fe buffer
* statistical info inside sysfs
*/
int ipfe_buff_stats_print_handler(stm_object_h object, char *buf, size_t size,
					      char *user_buf, size_t user_size)
{
	int total_write = 0;
	struct stm_fe_ip_s *ipfe = object;

	total_write = scnprintf(user_buf,
				user_size,
				"Packet reception: statistical info:%s\n",
				"stm_fe_ip");

	total_write += scnprintf(user_buf + total_write,
				user_size - total_write,
				"\n");

	total_write += scnprintf(user_buf + total_write,
				user_size - total_write,
				/*"buffer size:%d\n"*/
				"tail:%d\n"
				"wstart:%d\n"
				"wend:%d\n"
				"push time:%d ms\n"
				"delay between start and first push:%d ms\n"
				"gap between start and last stop:%d ms\n"
				"buffer overflow count: %d\n"
				"injection rate to IP thread by nethook: %d\n",
				/*ipfe->sys_data.buff.buff_size,*/
				ipfe->ip_data.tail,
				ipfe->ip_data.wstart,
				ipfe->ip_data.wend,
				ipfe->ip_data.push_time_ms,
				ipfe->ip_data.first_inj_latency,
				ipfe->ip_data.stop_start_gap,
				ipfe->ip_data.overflow_count,
				ipfe->ip_data.inj_rate
				);

	total_write += scnprintf(user_buf + total_write,
				user_size - total_write,
				"\n");

	return total_write;
}

/*!
* @brief	initialisation of the statistics (adding attributes)
* @param[in]	ipfe pointer to stm_fe_ip_s structure
*/
int stm_ipfe_stats_init(struct stm_fe_ip_s *ipfe)
{
	int err = -1;
	int ret = 0;
	char dt_ipfe_info[20], dt_stats_info[20], dt_buff_stats_info[25],
							   jitter_conf_info[20];
	stm_registry_type_def_t def1, def2, def3, def4;

	/*register connection info handler*/
	snprintf(dt_ipfe_info, sizeof(STM_REGISTRY_IPFE_INFO) + 3,
				STM_REGISTRY_IPFE_INFO "_%d", ipfe->ip_id);
	def1.print_handler = ipfe_info_print_handler;
	err = stm_registry_add_data_type(dt_ipfe_info, &def1);
	if (err) {
		pr_err("%s: Failed registry_add_data_type = %d\n", __func__,
									   err);
		return err;
	}

	err = stm_registry_add_attribute((stm_object_h)ipfe,
						IPFE_CONNECTION_INFO,
						dt_ipfe_info,
						&(ipfe->params),
						sizeof(stm_fe_ip_connection_t));

	if (err) {
		ret = stm_registry_remove_data_type(dt_ipfe_info);
		pr_err("%s: Failed registry_add_attribute = %d\n", __func__,
									   err);
		return err;
	}

	/*register stats handler*/
	snprintf(dt_stats_info, sizeof(STM_REGISTRY_IPFE_STATS) + 3,
				 STM_REGISTRY_IPFE_STATS "_%d", ipfe->ip_id);
	def2.print_handler = ipfe_stats_print_handler;
	err = stm_registry_add_data_type(dt_stats_info, &def2);
	if (err) {
		ret = stm_registry_remove_attribute((stm_object_h)ipfe,
							IPFE_CONNECTION_INFO);
		ret = stm_registry_remove_data_type(dt_ipfe_info);
		pr_err("%s: Failed registry_add_data_type = %d\n", __func__,
									   err);
		return err;
	}

	err = stm_registry_add_attribute((stm_object_h)ipfe,
					IPFE_STATS,
					dt_stats_info,
					&(ipfe->ip_data.stats),
					sizeof(stm_fe_ip_stat_t));
	if (err) {
		ret = stm_registry_remove_data_type(dt_stats_info);
		ret = stm_registry_remove_attribute((stm_object_h)ipfe,
						IPFE_CONNECTION_INFO);
		ret = stm_registry_remove_data_type(dt_ipfe_info);
		pr_err("%s: Failed registry_add_attribute = %d\n", __func__,
									   err);
		return err;
	}

	/*register buffer stats handler*/
	snprintf(dt_buff_stats_info, sizeof(STM_REGISTRY_IPFE_BUFF_STATS) + 3,
					 STM_REGISTRY_IPFE_BUFF_STATS "_%d",
								ipfe->ip_id);
	def3.print_handler = ipfe_buff_stats_print_handler;
	err = stm_registry_add_data_type(dt_buff_stats_info, &def3);
	if (err) {
		ret = stm_registry_remove_attribute((stm_object_h)ipfe,
							IPFE_CONNECTION_INFO);
		ret = stm_registry_remove_attribute((stm_object_h)ipfe,
							IPFE_STATS);
		ret = stm_registry_remove_data_type(dt_ipfe_info);
		ret = stm_registry_remove_data_type(dt_stats_info);
		pr_err("%s: Failed registry_add_data_type = %d\n", __func__,
									   err);
		return err;
	}

	err = stm_registry_add_attribute((stm_object_h)ipfe,
					IPFE_BUFF_STATS,
					dt_buff_stats_info,
					&(ipfe->ip_data.stats),
					sizeof(stm_fe_ip_stat_t));
	if (err) {
		ret = stm_registry_remove_data_type(dt_buff_stats_info);
		ret = stm_registry_remove_attribute((stm_object_h)ipfe,
						IPFE_CONNECTION_INFO);
		ret = stm_registry_remove_attribute((stm_object_h)ipfe,
						IPFE_STATS);
		ret = stm_registry_remove_data_type(dt_ipfe_info);
		ret = stm_registry_remove_data_type(dt_stats_info);
		pr_err("%s: Failed registry_add_attribute = %d\n", __func__,
									   err);
		return err;
	}

	snprintf(jitter_conf_info, sizeof(IPFE_JITTER_CONF) + 3,
					 IPFE_JITTER_CONF "_%d", ipfe->ip_id);

	def4.store_handler = ipfe_jitter_conf_store_handler;
	def4.print_handler = ipfe_jitter_conf_print_handler;
	err = stm_registry_add_data_type(jitter_conf_info, &def4);
	if (err) {
		ret = stm_registry_remove_data_type(dt_buff_stats_info);
		ret = stm_registry_remove_attribute((stm_object_h)ipfe,
						IPFE_CONNECTION_INFO);
		ret = stm_registry_remove_attribute((stm_object_h)ipfe,
						IPFE_STATS);
		ret = stm_registry_remove_attribute((stm_object_h)ipfe,
						IPFE_BUFF_STATS);
		ret = stm_registry_remove_data_type(dt_ipfe_info);
		ret = stm_registry_remove_data_type(dt_stats_info);
		pr_err("%s: Failed registry_add_attribute = %d\n", __func__,
									   err);
		return err;
	}
	err = stm_registry_add_attribute((stm_object_h)ipfe,
					IPFE_JITTER_CONF,
					jitter_conf_info,
					&(ipfe->jitter_conf),
					sizeof(ipfe->jitter_conf));
	if (err) {
		ret = stm_registry_remove_data_type(dt_buff_stats_info);
		ret = stm_registry_remove_attribute((stm_object_h)ipfe,
						IPFE_CONNECTION_INFO);
		ret = stm_registry_remove_attribute((stm_object_h)ipfe,
						IPFE_STATS);
		ret = stm_registry_remove_attribute((stm_object_h)ipfe,
						IPFE_BUFF_STATS);
		ret = stm_registry_remove_data_type(dt_ipfe_info);
		ret = stm_registry_remove_data_type(dt_stats_info);
		ret = stm_registry_remove_data_type(jitter_conf_info);
		pr_err("%s: Failed registry_add_attribute = %d\n", __func__,
									   err);
		return err;
	}
	return err;
}

/*!
* @brief	termination of the statistics (removal of attributes)
* @param[in]	ipfe pointer to stm_fe_ip_s structure
*/
int stm_ipfe_stats_term(struct stm_fe_ip_s *ipfe)
{
	int ret = -1, err = 0;
	char dt_ipfe_info[20], dt_stats_info[20], dt_buff_stats_info[25];

	snprintf(dt_ipfe_info, sizeof(STM_REGISTRY_IPFE_INFO) + 3,
				STM_REGISTRY_IPFE_INFO "_%d", ipfe->ip_id);
	snprintf(dt_stats_info, sizeof(STM_REGISTRY_IPFE_STATS) + 3,
				STM_REGISTRY_IPFE_STATS "_%d", ipfe->ip_id);
	snprintf(dt_buff_stats_info, sizeof(STM_REGISTRY_IPFE_BUFF_STATS) + 3,
				STM_REGISTRY_IPFE_BUFF_STATS "_%d",
				ipfe->ip_id);

	ret = stm_registry_remove_attribute((stm_object_h)ipfe,
					    IPFE_CONNECTION_INFO);
	if (ret) {
		pr_err("%s: Failed stm_registry_remove_attribute = %d\n",
								 __func__, ret);
		err = ret;
	}

	ret = stm_registry_remove_attribute((stm_object_h)ipfe,
					    IPFE_STATS);
	if (ret) {
		pr_err("%s: Failed stm_registry_remove_attribute = %d\n",
								 __func__, ret);
		err = ret;
	}

	ret = stm_registry_remove_attribute((stm_object_h)ipfe,
					    IPFE_BUFF_STATS);
	if (ret) {
		pr_err("%s: Failed stm_registry_remove_attribute = %d\n",
								 __func__, ret);
		err = ret;
	}

	ret = stm_registry_remove_data_type(dt_ipfe_info);
	if (ret) {
		pr_err("%s: Failed stm_registry_remove_data_type = %d\n",
								 __func__, ret);
		err = ret;
	}

	ret = stm_registry_remove_data_type(dt_stats_info);
	if (ret) {
		pr_err("%s: Failed stm_registry_remove_data_type = %d\n",
								 __func__, ret);
		err = ret;
	}

	ret = stm_registry_remove_data_type(dt_buff_stats_info);
	if (ret) {
		pr_err("%s: Failed stm_registry_remove_data_type = %d\n",
								 __func__, ret);
		err = ret;
	}

	return err;
}
