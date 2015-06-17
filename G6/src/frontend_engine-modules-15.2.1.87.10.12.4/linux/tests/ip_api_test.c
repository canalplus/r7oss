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

Source file name : ip_api_test.c
Author :           SD

Test file to use stm fe ip object

Date        Modification					Name
----        ------------                                    --------
20-Jun-11   Created						SD
06-Jun-12   Modified						SD
18-Sep-12   Modified						AD

************************************************************************/

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/bug.h>
#include <linux/string.h>
#include <linux/list.h>
#include <linux/platform_device.h>
#include <linux/stm/plat_dev.h>
#include <linux/stm/demod.h>
#include <linux/stm/lnb.h>
#include <linux/stm/diseqc.h>
#include <linux/stm/ip.h>
#include <linux/delay.h>
#include <linux/unistd.h>
#include <linux/kthread.h>
#include <stm_registry.h>
#include <stm_fe.h>
#include <stm_te.h>
#include <stm_fe_os.h>

#define FIRST 0
#define LAST 1
#define IP_LIST_MAX_ENTRY 2

#define TEST_MULTIPLE_INTF

int port_no = 8000;
char *ipaddr = "192.168.24.8";
short protocol = 2;

module_param(port_no, int, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
MODULE_PARM_DESC(port_no, "port no");
module_param(ipaddr, charp, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
MODULE_PARM_DESC(ipaddr, "ip address");
module_param(protocol, short, 0000);
MODULE_PARM_DESC(mystring, "protocol: 1 rtp: 2 udp");

stm_fe_ip_h obj[2];

/*
* stmfe_instance_s: stm fe instance for event capturing
*/
struct stmfe_instance_s {
	stm_fe_ip_h obj;
	stm_event_subscription_h fe_subscription;
	fe_semaphore *event_sem;
	int32_t event_id;
};

/*
* enum to index convertere
*/
int ipfe_etoi(int enum_val, int arr_size, int pos)
{
	int i, k;
	i = ffs(enum_val);
	/*check if more than one bit is set*/
	k = ffs(enum_val >> i);

	/* if the position of "UNKNOWN" is LAST */
	if (pos) {
		if ((i >= arr_size) || (k))
			i = arr_size - 1;
	}
	/* if the position of "UNKNOWN" is FIRST */
	else {
		if ((i >= arr_size) || (k))
			i = 0;
	}
	return i;
}

/*
* event id to name mapper
*/
char *ipfe_get_event(int event_id)
{
	static char event[][60] = {
		"STM_FE_IP_EV_FIRST_PACKET_RECEIVED",
		"STM_FE_IP_EV_RTCP_RETRANS",
		"STM_FE_IP_EV_NO_DATA_AFTER_START_TIMEOUT",
		"STM_FE_IP_EV_DATA_TIMEOUT",
		"STM_FE_IP_EV_FIFO_SUPPLIED",
		"STM_FE_IP_EV_LOSS_EVENT_BEFORE_EC",
		"STM_FE_IP_EV_LOSS_EVENT_AFTER_EC",
		"STM_FE_IP_EV_SEVERE_LOSS_EVENT_BEFORE_EC",
		"STM_FE_IP_EV_SEVERE_LOSS_EVENT_AFTER_EC"
	};

	return event[event_id];
}

/*
* helper for printing event
*/
int ipfe_print_event(int event_id)
{
	pr_info("\nstm_fe_test : %s reported !\n",
					 ipfe_get_event(event_id));
	return 0;
}

/*
* function for capturing event
*/
int ipfe_capture_event(void *data)
{
	struct stmfe_instance_s *fe_instance;
	int evt, actual_count;
	stm_event_info_t events[FRONTEND_EVENTS];

	fe_instance = (struct stmfe_instance_s *)data;

	while (!kthread_should_stop()) {
		evt = stm_event_wait(fe_instance->fe_subscription, -1,
					FRONTEND_EVENTS, &actual_count, events);
		if (!evt) {
			pr_info("IPFE Event Received\n");
			fe_instance->event_id = events[0].event.event_id;
			stm_fe_sema_signal(fe_instance->event_sem);

		} else if (evt == -ECANCELED) {
			pr_info("%s: Forced exit\n", __func__);
			break;
		}
	}
	pr_info("%s: thread exiting....!\n", __func__);
	return 0;
}

static int32_t __init stm_fe_ip_api_test_init(void)
{
	int32_t ret;
	int32_t sem_ret;
	int32_t wait_ctr = 25;
	int32_t i = 0;
	int32_t evt;
	uint32_t cnt, idx = 0;

	stm_fe_object_info_t *fe_objs;
	stm_fe_ip_connection_t connect_params;
	fe_thread capture_event;
	struct stmfe_instance_s fe_instance;
	stm_event_subscription_entry_t events_entry;
	stm_te_object_h demux_object0, demux_object1;

	pr_info("Loading API test module...\n");

	/*create a te instance*/
	ret = stm_te_demux_new("demux0", &demux_object0);
	ret = stm_te_demux_start(demux_object0);
	ret = stm_te_demux_new("demux1", &demux_object1);
	ret = stm_te_demux_start(demux_object1);


	/*initialize IP handle*/
	obj[0] = 0;
	obj[1] = 0;

	/*discover stm_fe*/
	ret = stm_fe_discover(&fe_objs, &cnt);
	if (ret)
		pr_info("%s: discover fail\n", __func__);

	/*look for ip fe*/
	for (i = 0; i < cnt; i++) {
		pr_info("Searching IP FE:count %d : type %d\n",
							 i, fe_objs[i].type);
		if (fe_objs[i].type == STM_FE_IP)  {
			pr_info("-- create IP object--\n");
			ret = stm_fe_ip_new(fe_objs[i].stm_fe_obj,
						NULL, &obj[idx++]);
			if (ret)
				pr_err("%s: stm_fe_ip_new failed",
								 __func__);
			break;
		}
	}

	/*attach te interface to ip fe object*/
	ret = stm_fe_ip_attach(obj[0], demux_object0);
	if (ret)
		pr_err("error in attaching handle 0: %d", ret);
	ret = stm_fe_ip_attach(obj[0], demux_object1);
	if (ret)
		pr_err("error in attaching handle 1: %d", ret);

	/*subscribe for event*/
	events_entry.object = obj[0];
	events_entry.event_mask =
		STM_FE_IP_EV_FIRST_PACKET_RECEIVED |
		STM_FE_IP_EV_RTCP_RETRANS |
		STM_FE_IP_EV_NO_DATA_AFTER_START_TIMEOUT |
		STM_FE_IP_EV_DATA_TIMEOUT |
		STM_FE_IP_EV_FIFO_SUPPLIED |
		STM_FE_IP_EV_LOSS_EVENT_BEFORE_EC |
		STM_FE_IP_EV_LOSS_EVENT_AFTER_EC |
		STM_FE_IP_EV_SEVERE_LOSS_EVENT_BEFORE_EC |
		STM_FE_IP_EV_SEVERE_LOSS_EVENT_AFTER_EC;

	events_entry.cookie = NULL;
	evt = stm_event_subscription_create(&events_entry, FRONTEND_EVENTS,
						  &fe_instance.fe_subscription);
	if (evt)
		pr_info("%s: Subscription of event fail\n", __func__);
	else
		pr_info("%s: Subscription of event OK\n", __func__);
	fe_instance.event_sem = (fe_semaphore *)
					stm_fe_malloc(sizeof(fe_semaphore));

	/*initialize event semaphore*/
	sem_ret = stm_fe_sema_init(fe_instance.event_sem, 0);
	if (sem_ret)
		pr_info("%s: stm_fe_sema_init fail\n", __func__);
	else
		pr_info("%s: stm_fe_sema_init OK\n", __func__);

	#ifdef TEST_MULTIPLE_INTF
	ret = stm_fe_thread_create(&capture_event, ipfe_capture_event,
				(void *)&fe_instance, "INP-IP-Cap-Evt", 0);
	#endif

	/*fill up the connect params*/
	pr_info("%s: configuring the IP FE started\n", __func__);
	strncpy(connect_params.ip, ipaddr, sizeof(connect_params.ip)-1);
	connect_params.ip[sizeof(connect_params.ip)-1] = '\0';
	connect_params.port_no = port_no;
	connect_params.protocol = protocol;
	ret = stm_fe_ip_set_compound_control(obj[0],
				STM_FE_IP_CTRL_SET_CONFIG, &connect_params);
	if (ret)
		pr_err("%s: stm_fe_ip_set_compound_control failed",
								 __func__);

	/*start the ip fe*/
	ret = stm_fe_ip_start(obj[0]);
	if (ret)
		pr_err("%s: stm_fe_ip_start failed", __func__);

	/*wait for event*/
	#ifdef TEST_MULTIPLE_INTF

	while (wait_ctr) {
		evt = stm_fe_sema_wait_timeout(fe_instance.event_sem,
							stm_fe_delay_ms(1000));

		if (evt)
			continue;

		if (fe_instance.event_id == STM_FE_IP_EV_FIRST_PACKET_RECEIVED)
			pr_info("STM_FE_IP_EV_FIRST_PACKET_RECEIVED");
		else if (fe_instance.event_id == STM_FE_IP_EV_RTCP_RETRANS)
			pr_info("STM_FE_IP_EV_RTCP_RETRANS");
		else if (fe_instance.event_id ==
				STM_FE_IP_EV_NO_DATA_AFTER_START_TIMEOUT)
			pr_info("EV_NO_DATA_AFTER_START_TIMEOUT");
		else if (fe_instance.event_id == STM_FE_IP_EV_DATA_TIMEOUT)
			pr_info("STM_FE_IP_EV_DATA_TIMEOUT");
		else if (fe_instance.event_id == STM_FE_IP_EV_FIFO_SUPPLIED)
			pr_info("STM_FE_IP_EV_FIFO_SUPPLIED");
		else if (fe_instance.event_id ==
					STM_FE_IP_EV_LOSS_EVENT_BEFORE_EC)
			pr_info("STM_FE_IP_EV_LOSS_EVENT_BEFORE_EC");
		else if (fe_instance.event_id ==
					STM_FE_IP_EV_LOSS_EVENT_AFTER_EC)
			pr_info("STM_FE_IP_EV_LOSS_EVENT_AFTER_EC");
		else if (fe_instance.event_id ==
				STM_FE_IP_EV_SEVERE_LOSS_EVENT_BEFORE_EC)
			pr_info("IP_EV_SEVERE_LOSS_EVENT_BEFORE_EC");
		else if (fe_instance.event_id ==
				STM_FE_IP_EV_SEVERE_LOSS_EVENT_AFTER_EC)
			pr_info("IP_EV_SEVERE_LOSS_EVENT_AFTER_EC");

		fe_instance.event_id = 0xff;
		wait_ctr--;
	}

	#endif

	kthread_stop(capture_event);

	evt = stm_event_subscription_delete(fe_instance.fe_subscription);
	if (evt)
		pr_info("%s: Unsubscription of event fail\n",
								__func__);
	else
		pr_info("%s: Unsubscription of event OK\n", __func__);

	return 0;

	return 0;
}

module_init(stm_fe_ip_api_test_init);

static void __exit stm_fe_ip_api_test_term(void)
{
	uint32_t idx = 0, ret = 0;

	pr_info("\nRemoving stmfe API test module ...\n");

	/*delete ip fe instances*/
	for (idx = 0; idx < IP_LIST_MAX_ENTRY; idx++) {
		if (obj[idx]) {
			ret = stm_fe_ip_delete(obj[idx]);
			if (ret)
				pr_err("%s: stm_fe_ip_delete failed",
								 __func__);
		}
	}

}

module_exit(stm_fe_ip_api_test_term);

MODULE_DESCRIPTION("Frontend engine test module for STKPI IP frontend");
MODULE_AUTHOR("STMicroelectronics Limited");
MODULE_LICENSE("GPL");
