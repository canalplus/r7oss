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

Source file name : api_test.c
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
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/fcntl.h>
#include <linux/vmalloc.h>
#include <stm_registry.h>
#include <stm_fe.h>
#include <stm_te.h>
#include <stm_fe_os.h>
#include <linux/uaccess.h>
#include <stm_memsrc.h>
#include <stm_memsink.h>
#include "stm_event.h"
#include "stm_data_interface.h"

#define IP_LIST_MAX_ENTRY 2

int port_no = 6000;
char *ipaddr = "192.168.24.8";
short protocol = 2;
int v;

stm_fe_ip_h obj[2];

uint8_t *buf2[1024];
char intfile[] = "/root/intermediate.ts";

struct sink2file_s {
	stm_memsink_h memsink;
	char *filename;
	uint32_t chunk_size;
	bool use_events;
	struct file *_fp;
	struct task_struct *_thread;
};

module_param(port_no, int, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
MODULE_PARM_DESC(port_no, "port no");
module_param(ipaddr, charp, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
MODULE_PARM_DESC(ipaddr, "ip address");
module_param(protocol, short, 0000);
MODULE_PARM_DESC(protocol, "protocol: 1 rtp: 2 udp");
module_param(v, int, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
MODULE_PARM_DESC(v, "vmalloc: 0 kmalloc 1 vmalloc");

static int sink_thread_run(struct sink2file_s *sink_parm);
static int
__attribute__ ((unused))sink_thread_stop(struct sink2file_s *sink_parm);

/* File utility functions */
static struct file *file_open(const char *path, int flags, int rights);
static void file_close(struct file *file);
static int file_read(struct file *file, unsigned long long offset,
	      unsigned char *data, unsigned int size);
static int file_write(struct file *file, unsigned long long offset,
	       unsigned char *data, unsigned int size);
static int __attribute__ ((unused)) file_size(const char *path);

struct file *file_open(const char *path, int flags, int rights)
{
	struct file *filp = NULL;
	mm_segment_t oldfs;
	int err = 0;

	oldfs = get_fs();
	set_fs(get_ds());
	filp = filp_open(path, flags, rights);
	set_fs(oldfs);
	if (IS_ERR(filp)) {
		err = PTR_ERR(filp);
		return NULL;
	}
	return filp;
}

void file_close(struct file *file)
{
	filp_close(file, NULL);
}

int file_read(struct file *file, unsigned long long offset,
	      unsigned char *data, unsigned int size)
{
	mm_segment_t oldfs;
	int ret;

	oldfs = get_fs();
	set_fs(get_ds());

	ret = vfs_read(file, data, size, &offset);

	set_fs(oldfs);
	return ret;
}

int file_write(struct file *file, unsigned long long offset,
	       unsigned char *data, unsigned int size)
{
	mm_segment_t oldfs;
	int ret;

	oldfs = get_fs();
	set_fs(get_ds());

	ret = vfs_write(file, data, size, &offset);

	set_fs(oldfs);
	return ret;
}

int file_size(const char *path)
{
	int offset = 0, pre_offset = 0;
	unsigned int size = 50;
	unsigned char data[size];

	struct file *file = file_open(path, O_RDONLY, 0);

	do {
		pre_offset = offset;
		offset += file_read(file, offset, data, size);
	} while (pre_offset != offset);

	file_close(file);

	return offset;
}

/* Kthread function for reading from a memsink into a file */
int sink2file_func(void *ptr)
{
	struct sink2file_s *params = (struct sink2file_s *) ptr;
	int err = 0;
	uint8_t *buf;
	uint32_t wr_offset = 0;

	stm_event_subscription_h evt_sub = NULL;
	stm_event_info_t evt_info;
	stm_event_subscription_entry_t memsink_data_evt = {
		.object = params->memsink,
		.event_mask = STM_MEMSINK_EVENT_DATA_AVAILABLE,
		.cookie = NULL,
	};

	if (v == 1)
		buf = vmalloc(1024 * 1024);
	else
		buf = kmalloc(128 * 1024, GFP_KERNEL);
	if (!buf) {
		pr_info("Could not allocate read buffer\n");
		file_close(params->_fp);
		return -ENOMEM;
	}
	params->use_events = 0;
	if (params->use_events) {
		err = stm_event_subscription_create(&memsink_data_evt, 1,
						    &evt_sub);
		pr_info("Subscribed to data event on memsink %p (err=%d)\n",
		       params->memsink, err);
	}
	params->filename = intfile;

	pr_info("\n Starting read from memsink %p -> file %s\n",
			params->memsink, params->filename);
	while (!kthread_should_stop()) {
		uint32_t available_data = 0;
		uint32_t read_size = 0;
		uint32_t write_size;

		if (!params->use_events) {
			/* Polled mode: Test for data and read when available.
			 *                          * Otherwise sleep */
			err = stm_memsink_test_for_data(params->memsink,
							&available_data), 0;
			if (!available_data)
				continue;
		} else {
			/* Event mode: Wait for event */
			unsigned int nb_evt = 0;

			err = stm_event_wait(evt_sub, 500, 1, &nb_evt,
					     &evt_info);
			if (!nb_evt || err) {
				err = 0;
				continue;
			}
			pr_info("Received data event from memsink %p\n",
			       params->memsink);
			err = stm_memsink_test_for_data(params->memsink,
							&available_data), 0;
		}

		while (available_data) {

			if (v == 1)
				err = stm_memsink_pull_data(params->memsink,
					buf,
					200 * 1024,
					&read_size);
			else
				err = stm_memsink_pull_data(params->memsink,
					buf,
					1316,
					&read_size);

			if (err == -EBUSY) {
				err = 0;
				continue;
			}
			if (err) {
				pr_info("stm_memsink_pull_data, memsink %p ret %d\n",
						params->memsink, err);
				break;
			}
			/* Write data to file */
			write_size = 0;

			while (write_size < read_size) {
				write_size += file_write(params->_fp,
							 wr_offset + write_size,
							 buf, read_size);
			}
			wr_offset += write_size;
			available_data -= read_size;
		}
	}

	if (evt_sub)
		err = stm_event_subscription_delete(evt_sub);

	/* Wait until thread should stop in case the above read loop finished
	   due to an error */
	while (!kthread_should_stop())
		msleep(50);
	file_close(params->_fp);

	if (v == 1)
		vfree(buf);
	else
		kfree(buf);

	return err;
}

/* Starts a thread to read data from a memsink and dump it to a file */
int sink_thread_run(struct sink2file_s *sink_parm)
{
	sink_parm->_fp = file_open(sink_parm->filename,
				   O_WRONLY | O_SYNC | O_CREAT | O_TRUNC,
				   S_IRUGO | S_IWUGO);
	if (!sink_parm->_fp) {
		pr_info("Could not open %s for writing\n", sink_parm->filename);
		return -ENOENT;
	}

	sink_parm->_thread = kthread_run(sink2file_func, sink_parm,
					 "TeTstSink");

	if (IS_ERR(sink_parm->_thread)) {
		pr_info("Could not start sink thread\n");
		return -ENOMEM;
	}
	return 0;
}

/* Stops a sink thread, started with sink_thread_run */
int sink_thread_stop(struct sink2file_s *sink_parm)
{
	int err;

	if (IS_ERR(sink_parm->_thread) || !sink_parm->_thread)
		return -EINVAL;

	msleep(50);
	err = kthread_stop(sink_parm->_thread);
	sink_parm->_thread = NULL;

	return err;
}

static int32_t __init stm_fe_ip_api_test_init(void)
{
	int32_t ret;
	int32_t i = 0;
	uint32_t cnt, idx = 0;

	stm_fe_object_info_t *fe_objs;
	stm_fe_ip_connection_t connect_params;
	stm_memsink_h memsink = (stm_memsink_h) NULL;
	struct sink2file_s *sink_parm;

	sink_parm = kmalloc(sizeof(struct sink2file_s), GFP_KERNEL);

	/*initialize IP handle */
	obj[0] = 0;
	obj[1] = 0;

	/*discover stm_fe */
	ret = stm_fe_discover(&fe_objs, &cnt);
	if (ret)
		pr_info("%s: discover fail\n", __func__);

	/*look for ip fe */
	for (i = 0; i < cnt; i++) {
		pr_info("Searching IP FE:count %d: type %d\n", i,
				fe_objs[i].type);
		if (fe_objs[i].type == STM_FE_IP) {
			pr_info("create IP obj with different name\n");
			ret = stm_fe_ip_new(fe_objs[i].stm_fe_obj,
					    NULL, &obj[idx++]);
			if (ret)
				pr_err("%s: stm_fe_ip_new failed\n", __func__);
		}
	}

	if (v == 1)
		ret = stm_memsink_new("memsink", STM_IOMODE_BLOCKING_IO,
						KERNEL_VMALLOC, &memsink);
	else
		ret = stm_memsink_new("memsink", STM_IOMODE_BLOCKING_IO,
						KERNEL, &memsink);
	if (ret)
		pr_err("%s: failed to create stm_memsink_new",
		       __func__);

	ret = stm_fe_ip_attach(obj[0], memsink);
	if (ret)
		pr_err("%s: failed to attach stm_memsink_new",
		       __func__);

	/*fill up the connect params */
	pr_info("%s: configuring the IP FE started\n", __func__);
	strncpy(connect_params.ip, ipaddr, sizeof(connect_params.ip) - 1);
	connect_params.ip[sizeof(connect_params.ip) - 1] = '\0';
	connect_params.port_no = port_no;
	connect_params.protocol = protocol;
	ret = stm_fe_ip_set_compound_control(obj[0],
					     STM_FE_IP_CTRL_SET_CONFIG,
					     &connect_params);
	if (ret)
		pr_err("%s: stm_fe_ip_set_compound_control failed",
				__func__);

	sink_parm->filename = intfile;
	sink_parm->memsink = memsink;
	sink_parm->chunk_size = 1316 * 20;

	/*attach te interface to ip fe object */
	sink_thread_run(sink_parm);

	ret = stm_fe_ip_start(obj[0]);
	if (ret)
		pr_err("%s: stm_fe_ip_start failed", __func__);

	return 0;
}

module_init(stm_fe_ip_api_test_init);

static void __exit stm_fe_ip_api_test_term(void)
{
	uint32_t idx = 0, ret = 0;

	pr_info("\nRemoving stmfe API test module ...\n");

	/*delete ip fe instances */
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
