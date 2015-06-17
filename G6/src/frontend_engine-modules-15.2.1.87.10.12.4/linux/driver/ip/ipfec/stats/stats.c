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

Source file name : stats.c
Author :           SP

ip fec statistics reporting via sysfs

Date        Modification                                    Name
----        ------------                                    --------
28-Mar-13   Created                                         SP

************************************************************************/
#include <linux/platform_device.h>
#include <stm_registry.h>
#include <stm_fe.h>
#include <stm_fe_os.h>
#include <stm_fe_ip.h>
#include <stm_fe_ipfec.h>

#define STM_REGISTRY_IPFEC_STATS "dt_ipfec_stats"
#define STM_REGISTRY_IPFEC_BUFF_STATS "dt_ipfec_buff_stats"
#define IPFEC_STATS "ipfec_packet_stats"
#define IPFEC_BUFF_STATS "ipfec_buff_stats"


/*
* ipfec_stats_print_handler: helper function to print ip fec statistical
* info inside sysfs
*/
int ipfec_stats_print_handler(stm_object_h object, char *buf, size_t size,
					char *user_buf, size_t user_size)
{
	int total_write = 0;
	struct stm_fe_ipfec_s *ipfec = object;

	total_write = scnprintf(user_buf,
				user_size,
				"Packet reception: statistical info:%s\n",
				"stm_fe_ipfec");

	total_write += scnprintf(user_buf + total_write,
				user_size - total_write,
				"\n");

	total_write += scnprintf(user_buf + total_write,
				user_size - total_write,
				"packet count:%d\n"
				"lost packet count:%d\n"
				"corrected packet count:%d\n"
				"rejected packet count:%d\n",
				ipfec->ipfec_data.stats.packet_count,
				ipfec->ipfec_data.stats.lost_packet_count,
				ipfec->ipfec_data.stats.corrected_packet_count,
				ipfec->ipfec_data.stats.rejected_packet_count
				);

	total_write += scnprintf(user_buf + total_write,
				user_size - total_write,
				"\n");

	return total_write;
}

/*
* ipfec_buff_stats_print_handler: helper function to print ip fec buffer
* statistical info inside sysfs
*/
int ipfec_buff_stats_print_handler(stm_object_h object, char *buf,
			size_t size, char *user_buf, size_t user_size)
{
	int total_write = 0;
	struct stm_fe_ipfec_s *ipfec = object;

	total_write = scnprintf(user_buf,
				user_size,
				"Buffer info:%s\n",
				"stm_fe_ipfec");

	total_write += scnprintf(user_buf + total_write,
				user_size - total_write,
				"\n");

	total_write += scnprintf(user_buf + total_write,
				user_size - total_write,
				"row_d:%d\n"
				"col_l:%d\n"
				"tail:%d\n"
				"head:%d\n"
				"search head:%d\n"
				"recovery time:%d ms\n"
				"last snbase:%d\n"
				"buffer overflow count: %d\n",
				ipfec->ipfec_data.row_d,
				ipfec->ipfec_data.col_l,
				ipfec->ipfec_data.tail,
				ipfec->ipfec_data.head,
				ipfec->ipfec_data.search_head,
				ipfec->ipfec_data.recovery_time_ms,
				ipfec->ipfec_data.last_snbase,
				ipfec->ipfec_data.overflow_count
				);

	total_write += scnprintf(user_buf + total_write,
				user_size - total_write,
				"\n");

	return total_write;
}

/*!
* @brief	initialisation of the statistics (adding attributes)
* @param[in]	ipfec pointer to stm_fe_ipfec_s structure
*/
int stm_ipfec_stats_init(struct stm_fe_ipfec_s *ipfec)
{
	int err = -1;
	int ret = 0;
	char dt_stats_info[20], dt_buff_stats_info[25];
	stm_registry_type_def_t def1, def2;


	/*register stats handler*/
	snprintf(dt_stats_info, sizeof(STM_REGISTRY_IPFEC_STATS) + 3,
			STM_REGISTRY_IPFEC_STATS "_%d", ipfec->ipfec_id);
	def1.print_handler = ipfec_stats_print_handler;
	err = stm_registry_add_data_type(dt_stats_info, &def1);
	if (err) {
		pr_err("%s: reg add_data_type failed: %d\n", __func__, err);
		return err;
	}

	err = stm_registry_add_attribute((stm_object_h)ipfec,
					IPFEC_STATS,
					dt_stats_info,
					&(ipfec->ipfec_data.stats),
					sizeof(stm_fe_ip_fec_stat_t));
	if (err) {
		pr_err("%s: %s, %s, %s\n", __func__,
				ipfec->ipfec_name, IPFEC_STATS, dt_stats_info);
		ret = stm_registry_remove_data_type(dt_stats_info);
		pr_err("%s: reg add_attribute failed: %d\n", __func__, err);
		return err;
	}

	/*register buffer stats handler*/
	snprintf(dt_buff_stats_info, sizeof(STM_REGISTRY_IPFEC_BUFF_STATS) +
					3, STM_REGISTRY_IPFEC_BUFF_STATS "_%d",
					ipfec->ipfec_id);
	def2.print_handler = ipfec_buff_stats_print_handler;
	err = stm_registry_add_data_type(dt_buff_stats_info, &def2);
	if (err) {
		ret = stm_registry_remove_attribute((stm_object_h)ipfec,
							IPFEC_STATS);
		ret = stm_registry_remove_data_type(dt_stats_info);
		pr_err("%s: reg add_data_type failed: %d\n", __func__, err);
		return err;
	}

	err = stm_registry_add_attribute((stm_object_h)ipfec,
					IPFEC_BUFF_STATS,
					dt_buff_stats_info,
					&(ipfec->ipfec_data),
					sizeof(struct ipfec_params));
	if (err) {
		ret = stm_registry_remove_data_type(dt_buff_stats_info);
		ret = stm_registry_remove_attribute((stm_object_h)ipfec,
						IPFEC_STATS);
		ret = stm_registry_remove_data_type(dt_stats_info);
		pr_err("%s: reg add_attribute failed: %d\n", __func__, err);
		return err;
	}

	return err;
}

/*!
* @brief	termination of the statistics (removal of attributes)
* @param[in]	ipfec pointer to stm_fe_ip_s structure
*/
int stm_ipfec_stats_term(struct stm_fe_ipfec_s *ipfec)
{
	int ret = -1, err = 0;
	char dt_stats_info[20], dt_buff_stats_info[25];

	snprintf(dt_stats_info, sizeof(STM_REGISTRY_IPFEC_STATS) +
			3, STM_REGISTRY_IPFEC_STATS "_%d", ipfec->ipfec_id);

	snprintf(dt_buff_stats_info, sizeof(STM_REGISTRY_IPFEC_BUFF_STATS) + 3,
				STM_REGISTRY_IPFEC_BUFF_STATS "_%d",
				ipfec->ipfec_id);


	ret = stm_registry_remove_attribute((stm_object_h)ipfec,
					    IPFEC_STATS);
	if (ret) {
		pr_err("%s: reg remove_attribute failed: %d\n", __func__, ret);
		err = ret;
	}

	ret = stm_registry_remove_attribute((stm_object_h)ipfec,
						IPFEC_BUFF_STATS);
	if (ret) {
		pr_err("%s: reg remove_attribute failed: %d\n", __func__, ret);
		err = ret;
	}

	ret = stm_registry_remove_data_type(dt_stats_info);
	if (ret) {
		pr_err("%s: reg remove_data_type failed: %d\n", __func__, ret);
		err = ret;
	}

	ret = stm_registry_remove_data_type(dt_buff_stats_info);
	if (ret) {
		pr_err("%s: reg remove_data_type failed: %d\n", __func__, ret);
		err = ret;
	}

	return err;
}
