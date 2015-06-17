/************************************************************************
Copyright (C) 2011 STMicroelectronics. All Rights Reserved.

This file is part of the STLinuxTV Library.

STLinuxTV is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License version 2 as published by the
Free Software Foundation.

STLinuxTV is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with STLinuxTV; see the file COPYING.  If not, write to the Free Software
Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

The STLinuxTV Library may alternatively be licensed under a proprietary
license from ST.

Source file name : demux_feeds.c
Author :           Rahul.V

Definition of interface functions for Linux DVB, stm_fe and stm_te modules.

Date        Modification                                    Name
----        ------------                                    --------
16-Jun-11   Created                                         Rahul.V

 ************************************************************************/
#include <dvb/dvb_adaptation.h>
#include <dvb/dvb_data.h>
#include <linux/version.h>
#include "demux_filter.h"
#include "demux_feeds.h"
#include "demux_common.h"

#include "linux/dvb/stm_dmx.h"

#define stv_err pr_err

#ifdef CONFIG_STLINUXTV_DECODE_DISPLAY
#include "dvb_audio.h"
#include "dvb_video.h"
#endif

/* pretty much made up number */
#define NUMPKT 6144
#define PACKET_SIZE 188		/* bytes */
#define DEMUX_BUFFERSIZE              (NUMPKT * PACKET_SIZE)	/* in bytes */
#define SEGMENT_SIZE		      (256 * PACKET_SIZE)
#define POLL_PERIOD_MS 10

static int stm_dvb_connect_dvr(struct stm_dvb_demux_s *stm_demux)
{
	char memsrc_name[MAX_NAME_LEN_SIZE];
	int ret;

	/* Write dvr opening require memsrc creation and attach */
	snprintf(memsrc_name, STM_REGISTRY_MAX_TAG_SIZE,
		 "SRC_TO_DEMUX-%d", stm_demux->demux_id);

	ret = stm_memsrc_new(memsrc_name,
			     STM_IOMODE_BLOCKING_IO, USER,
			     &stm_demux->memsrc_input);
	if (ret) {
		DMX_ERR("failed stm_memsink_new (%d)\n", ret);
		goto error_memsink_new;
	}

#ifdef CONFIG_STLINUXTV_CRYPTO
	if (stm_demux->ca_channel) {
		ret = stm_memsrc_attach(stm_demux->memsrc_input,
					stm_demux->ca_channel->transform,
					STM_DATA_INTERFACE_PUSH);
		if (ret) {
			DMX_ERR("failed to attach the memsrc device (%d)\n",
				ret);
			goto error_memsink_attach;
		}

		ret = stm_ce_transform_attach(stm_demux->ca_channel->transform,
					      stm_demux->demux_object);
		if (ret) {
			DMX_ERR("failed to attach the memsrc device (%d)\n",
				ret);
			goto error_transform_attach;
		}
	} else {
#endif

	ret = stm_memsrc_attach(stm_demux->memsrc_input,
			stm_demux->demux_object, STM_DATA_INTERFACE_PUSH);
	if (ret){
		DMX_ERR("failed to attach the memsrc device (%d)\n", ret);
		goto error_memsink_attach;
	}

#ifdef CONFIG_STLINUXTV_CRYPTO
	}
#endif

	return 0;

#ifdef CONFIG_STLINUXTV_CRYPTO
error_transform_attach:
	if (stm_memsrc_detach(stm_demux->memsrc_input))
		DMX_ERR("failed to detach from transform (%d)\n",ret);
#endif
error_memsink_attach:
	if (stm_memsrc_delete(stm_demux->memsrc_input))
		DMX_ERR("failed to delete the memsrc (%d)\n",ret);
error_memsink_new:
	return ret;
}

static int stm_dvb_disconnect_dvr(struct stm_dvb_demux_s *stm_demux)
{
	int ret;

	if (stm_demux->memsrc_input == NULL)
		return 0;

	ret = stm_memsrc_detach(stm_demux->memsrc_input);
	if (ret){
		DMX_ERR("failed to detach the memsrc (%d)\n", ret);
		goto error;
	}

	ret = stm_memsrc_delete(stm_demux->memsrc_input);
	if (ret){
		DMX_ERR("failed to delete the memsrc (%d)\n", ret);
		goto error;
	}

	stm_demux->memsrc_input = NULL;

#ifdef CONFIG_STLINUXTV_CRYPTO
	if (stm_demux->ca_channel) {
		ret = stm_ce_transform_detach(stm_demux->ca_channel->transform,
						stm_demux->demux_object);
		if (ret)
			DMX_ERR("failed to detach transform from demux\n");
	}
#endif

	return 0;

error:
	return ret;
}

static int stm_dvb_connect_frontend(struct stm_dvb_demux_s *stm_demux, unsigned int id)
{
	struct stm_dvb_demod_s *stm_demod = NULL;
	struct stm_dvb_ip_s *stm_ip = NULL;
	unsigned int index = id;
	unsigned int ip_id;
	int ret;

	/* The IP frontend are AFTER the DEMOD frontend so we first try
	 * search for a demod frontend with that value, if it is NOT available
	 * then it is most probably an IP FE */
	GET_DEMOD_OBJECT(stm_demod, id);
	if (stm_demod) {
		ret = stm_fe_demod_attach(stm_demod->demod_object,
					  stm_demux->demux_object);
		if (ret){
			DMX_ERR("failed to attach demod (%d)\n", ret);
			goto error_demod_attach;
		}
		stm_demux->demod = stm_demod;
	} else {
		if (index) {
			/* We need to shift the value to the maximum number of demod */
			do {
				index--;
				GET_DEMOD_OBJECT(stm_demod, index);
			} while (!stm_demod && index);

			if (index || stm_demod)
				ip_id = id - index - 1;
			else
				ip_id = id - index;
		} else {
			ip_id = index;
		}

		GET_IP_OBJECT(stm_ip, ip_id);
		if (stm_ip) {
			ret = stm_fe_ip_attach(stm_ip->ip_object,
					     stm_demux->demux_object);
			if (ret){
				DMX_ERR("failed to attach ip fe (%d)\n", ret);
				goto error_ip_attach;
			}
			stm_demux->ip = stm_ip;
		}
	}

	return 0;

error_ip_attach:
error_demod_attach:
	return ret;
}

static int stm_dvb_disconnect_frontend(struct stm_dvb_demux_s *stm_demux)
{
	int ret;

	if (stm_demux->demod){
		ret = stm_fe_demod_detach(stm_demux->demod->demod_object,
						stm_demux->demux_object);
		if (ret){
			DMX_ERR("failed detach demod (%d)\n", ret);
			goto error_demod_detach;
		}
		stm_demux->demod = NULL;
	}

	if (stm_demux->ip) {
		ret = stm_fe_ip_detach(stm_demux->ip->ip_object,
					     stm_demux->demux_object);
		if (ret){
			DMX_ERR("failed detach fe ip (%d)\n", ret);
			goto error_ip_detach;
		}
		stm_demux->ip = NULL;
	}

	return 0;

error_ip_detach:
error_demod_detach:
	return ret;
}

static int stm_dvb_set_source(struct dmx_demux *demux, const dmx_source_t * src)
{
	struct dvb_demux *dvb_demux = demux->priv;
	struct stm_dvb_demux_s *stm_demux =
	    container_of(dvb_demux, struct stm_dvb_demux_s, dvb_demux);

	down_write(&stm_demux->rw_sem);

	if (stm_demux->demux_running){
		up_write(&stm_demux->rw_sem);
		return -EBUSY;
	}

	if (*src < DMX_SOURCE_DVR0){
		stm_demux->demod_id = *src;
		stm_demux->demux_source = DEMUX_SOURCE_FE;
		stm_demux->demux_default_source = DEMUX_SOURCE_FE;
	} else {
		if ((*src - DMX_SOURCE_DVR0) != stm_demux->demux_id){
			up_write(&stm_demux->rw_sem);
			return -EINVAL;
		}
		stm_demux->demux_source = DEMUX_SOURCE_DVR;
		stm_demux->demux_default_source = DEMUX_SOURCE_DVR;
	}

	up_write(&stm_demux->rw_sem);

	return 0;
}

static int stm_dvb_get_caps(struct dmx_demux *demux, struct dmx_caps * caps)
{
	int ret = 0;
	stm_te_caps_t te_caps;

	ret = stm_te_get_capabilities(&te_caps);
	if (ret) {
		pr_err("Error in getting the capabilities from TE\n");
		goto get_caps_failed;
	}

	caps->num_decoders = te_caps.max_demuxes;

get_caps_failed:
	return ret;
}

static inline unsigned int stm_dvb_poll(struct stm_dvb_demux_s *demux,
					struct stm_dvb_filter_s *valid_filter,
					struct file *file, poll_table * wait)
{
	int ret, size = 0;

	poll_wait(file, &valid_filter->wait, wait);

	ret = stm_memsink_test_for_data(valid_filter->memsink_interface, &size);

	if (ret)
		return POLLERR;

	if (!size)
		return 0;

	return (POLLIN | POLLRDNORM | POLLPRI);
}

static unsigned int stm_dvb_dmx_poll(struct file *file, poll_table * wait)
{
	struct dmxdev_filter *dmxdevfilter = file->private_data;
	struct dmxdev *dev = dmxdevfilter->dev;
	struct stm_dvb_demux_s *stm_demux =
	    container_of(dev, struct stm_dvb_demux_s, dmxdev);
	struct stm_dvb_filter_chain_s *filter_chain = NULL;
	unsigned int ret;

	down_read(&stm_demux->rw_sem);

	if (mutex_lock_interruptible(&dmxdevfilter->mutex)) {
		up_read(&stm_demux->rw_sem);
		return POLLERR;
	}

	filter_chain = match_chain_from_filter(stm_demux, dmxdevfilter);

	if (!filter_chain) {
		mutex_unlock(&dmxdevfilter->mutex);
		up_read(&stm_demux->rw_sem);
		return POLLERR;
	}

	ret = stm_dvb_poll(stm_demux, filter_chain->output_filter, file, wait);

	mutex_unlock(&dmxdevfilter->mutex);
	up_read(&stm_demux->rw_sem);

	return ret;
}

static unsigned int stm_dvb_dvr_poll(struct file *file, poll_table * wait)
{
	struct dvb_device *dvbdev = file->private_data;
	struct dmxdev *dev = dvbdev->priv;
	struct stm_dvb_demux_s *stm_demux =
	    container_of(dev, struct stm_dvb_demux_s, dmxdev);
	struct stm_dvb_filter_chain_s *filter_chain = NULL;
	unsigned int ret;

	if ((file->f_flags & O_ACCMODE) == O_RDONLY) {

		down_read(&stm_demux->rw_sem);

		filter_chain = get_primary_ts_chain(stm_demux);

		if (!filter_chain) {
			up_read(&stm_demux->rw_sem);
			return POLLERR;
		}

		ret = stm_dvb_poll(stm_demux,
				filter_chain->output_filter, file, wait);

		up_read(&stm_demux->rw_sem);

		return ret;

	} else
		return (POLLOUT | POLLWRNORM | POLLPRI);
}

/* In the dvr read case all the pid filters are multiplexed into a single TS so there is no
 * need to look up which filter we need to get data from, just read from the first filter
 * set in the list since they all point to the same output filer */
static ssize_t stm_dvb_dvr_read(struct file *file,
				char __user * buf,
				size_t buffer_size, loff_t * ppos)
{
	int count = 0;
	struct dvb_device *dvbdev = file->private_data;
	struct dmxdev *dev = dvbdev->priv;
	struct stm_dvb_demux_s *stm_demux =
	    container_of(dev, struct stm_dvb_demux_s, dmxdev);
	struct stm_dvb_filter_chain_s *valid_filter_chain;
	struct stm_dvb_filter_s *valid_filter;
	int ret;

	valid_filter_chain = get_primary_ts_chain(stm_demux);

	if (valid_filter_chain) {

		valid_filter = valid_filter_chain->output_filter;

		if (file->f_flags & O_NONBLOCK) {
			/* We do not want to block here, test_for_data first */
			ret =
			    stm_memsink_test_for_data
			    (valid_filter->memsink_interface, &count);
			if (ret)
				return -EIO;
			if (!count)
				/* No data available */
				return -EWOULDBLOCK;
			if (count < buffer_size)
				/* Clamp size of data to read to the size
				 * available to avoid blocking */
				buffer_size = count;
		}

		ret =
		    stm_memsink_pull_data(valid_filter->memsink_interface, buf,
					  buffer_size, &count);
		if (ret)
			return -EIO;
	}

	return count;
}

static int stm_dvb_set_ts_format(struct file *file,
					struct stm_dvb_demux_s *stm_demux,
					const dmx_ts_format_t *format)
{
	int ret = 0;

	/* Only 2 open type are handled here, dvr_open already block the rest */
	if ((file->f_flags & O_ACCMODE) == O_WRONLY){
		stm_te_input_type_t type;

		switch(*format){
		case DMX_TS_AUTO:
			type = STM_TE_INPUT_TYPE_UNKNOWN;
			break;
		case DMX_TS_TYPE_DVB:
			type = STM_TE_INPUT_TYPE_DVB;
			break;
		case DMX_TS_TYPE_TTS:
			type = STM_TE_INPUT_TYPE_TTS;
			break;
		default:
			DMX_ERR("Unsupported TS data type (%d)\n", *format);
			return -EINVAL;
		}

		/* Setting the input data type */
		ret = stm_te_demux_set_control(stm_demux->demux_object,
						STM_TE_DEMUX_CNTRL_INPUT_TYPE,
						(unsigned int)type);
		if (ret){
			DMX_ERR("Failed to set demux input type: %d\n", ret);
			goto failed;
		}
	} else {
		/* Setting the output data type */
		struct stm_dvb_filter_chain_s *valid_filter_chain;
		bool dlna_output = false;

		valid_filter_chain = get_primary_ts_chain(stm_demux);

		if (!valid_filter_chain){
			/* No TS output defined yet */
			DMX_ERR("%s: Must create a TS_TAP filter first\n",
				__FUNCTION__);
			ret = -EINVAL;
			goto failed;
		}

		switch(*format){
		case DMX_TS_TYPE_DVB:
			dlna_output = false;
			break;
		case DMX_TS_TYPE_TTS:
			dlna_output = true;
			break;
		default:
			DMX_ERR("Unsupported TS data type (%d)\n", *format);
			return -EINVAL;
		}

		/* Setting the output data type */
		ret = stm_te_demux_set_control(stm_demux->demux_object,
					STM_TE_TS_FILTER_CONTROL_DLNA_OUTPUT,
					(unsigned int)dlna_output);
		if (ret){
			DMX_ERR("Failed to set demux output type: %d\n", ret);
			goto failed;
		}
	}

failed:
	return ret;
}

static int stm_dvb_set_scrambling(struct file *file,
					struct stm_dvb_demux_s *stm_demux,
					const dmx_scrambling_t *scrambling)
{
	int ret = 0;
	bool secure = true;
	struct stm_dvb_filter_chain_s *valid_filter_chain;
	struct dmxdev_filter *dmxdev_filter = (struct dmxdev_filter *)file->private_data;
	if ((file->f_flags & O_ACCMODE) != O_RDONLY){
		DMX_ERR("%s: Scrambling mode of injected data is not allowed\n",
			__FUNCTION__);
		return -EINVAL;
	}

	valid_filter_chain = match_chain_from_filter(stm_demux, dmxdev_filter);


	if (!valid_filter_chain){
		/* No TS output defined yet */
		DMX_ERR("%s: Must create a TS_TAP filter first\n",
			__FUNCTION__);
		ret = -EINVAL;
		goto failed;
	}

	switch(*scrambling){
	case DMX_SCRAMBLED:
		secure = true;
		break;
	case DMX_DESCRAMBLED:
		secure = false;
		break;
	default:
		DMX_ERR("Unsupported scrambling type (%d)\n", *scrambling);
		return -EINVAL;
	}

	/* Setting the output data type */
	ret = stm_te_filter_set_control(valid_filter_chain->output_filter->handle,
				STM_TE_TS_FILTER_CONTROL_SECURE_OUTPUT,
				(unsigned int)secure);
	if (ret){
		DMX_ERR("Failed to set demux output scrambling: %d\n", ret);
		goto failed;
	}

failed:
	return ret;
}

static long stm_dvb_dvr_ioctl(struct file *file,
			      unsigned int cmd, unsigned long arg)
{
	struct dvb_device *dvbdev = file->private_data;
	struct dmxdev *dev = dvbdev->priv;
	struct stm_dvb_demux_s *stm_demux =
	    container_of(dev, struct stm_dvb_demux_s, dmxdev);
	int ret = 0;

	switch (cmd) {
	case DMX_SET_TS_FORMAT:

		down_write(&stm_demux->rw_sem);

		ret = stm_dvb_set_ts_format(file,
						stm_demux,
						(const dmx_ts_format_t *)arg);

		up_write(&stm_demux->rw_sem);

		break;

	default:
		goto generic_dvr_ioctl;
	}

	return ret;

generic_dvr_ioctl:
	return stm_demux->dvr_ops.unlocked_ioctl(file, cmd, arg);
}

static ssize_t stm_dvb_dvr_write(struct file *file,
				 const char __user * buf,
				 size_t count, loff_t * ppos)
{
	struct dvb_device *dvbdev = file->private_data;
	struct dmxdev *dev = dvbdev->priv;
	struct stm_dvb_demux_s *stm_demux =
	    container_of(dev, struct stm_dvb_demux_s, dmxdev);
	char *offset = (char *)buf;
	int injected_data;
	int ret;

	/* Get the memsrc that is attached to this demux */
	if (!(stm_demux->pacing_object))
		return -EINVAL;

	ret = stm_memsrc_push_data(stm_demux->memsrc_input, offset, count,
				   &injected_data);
	if (ret) {
		DMX_ERR("failed to push data into the memsrc (%d)\n", ret);
		return -EIO;
	}

	return injected_data;
}

static int stm_dvb_create_demux(struct stm_dvb_demux_s *stm_demux)
{
	int ret = 0;

	if (!stm_demux->users && !stm_demux->demux_object) {
		if (0 !=
		    stm_te_demux_new(stm_demux->dvb_demux_name,
				     &stm_demux->demux_object)) {
			DMX_ERR("Unable to get new demux\n");
			ret = -ENOMEM;
			goto error;
		}
		DMX_DBG("No demux so adding one\n");
	}
	stm_demux->users++;

error:
	return ret;
}

static int stm_dvb_internal_demux_delete(struct stm_dvb_demux_s *stm_demux)
{
	int ret = 0;

#ifdef CONFIG_STLINUXTV_CRYPTO
	struct list_head *temp;
	struct list_head *pos;
	struct stm_dvb_ca_pid_s *chain_entry;

	list_for_each_safe(pos, temp, &stm_demux->ca_pid_list) {
		chain_entry =
		    list_entry(pos, struct stm_dvb_ca_pid_s, ca_pid_list);
		if (chain_entry) {
			list_del(&chain_entry->ca_pid_list);
			kfree(chain_entry);
		}
	}
#endif

	if (stm_demux->demod) {
		if (0 !=
		    stm_fe_demod_detach(stm_demux->demod->demod_object,
					stm_demux->demux_object))
			DMX_ERR("Unable to detach demod output from demux\n");

	}

	if (stm_demux->ip) {
		if (0 !=
		    stm_fe_ip_detach(stm_demux->ip->ip_object,
				     stm_demux->demux_object))
			DMX_ERR("Unable to detach IP front-end"
					" output from demux\n");
	}

	del_all_chains_from_demux(stm_demux);

	if (stm_demux->demux_object) {
		ret = stm_te_demux_delete(stm_demux->demux_object);
		if (0 != ret)
			DMX_ERR("Unable to delete demux object\n");
		stm_demux->demux_object = NULL;
	}

	return ret;
}

static int stm_dvb_delete_demux(struct stm_dvb_demux_s *stm_demux)
{
	int ret = 0;

	stm_demux->users--;

	if (!stm_demux->users && stm_demux->demux_object) {
		DMX_DBG("No users anymore so closing\n");
		stm_dvb_internal_demux_delete(stm_demux);

		stm_demux->demux_default_source = DEMUX_SOURCE_FE;
		stm_demux->demux_source = DEMUX_SOURCE_FE;
		stm_demux->demod_id = stm_demux->demux_id;
	}

	return ret;
}

static int stm_dvb_demux_start(struct stm_dvb_demux_s *stm_demux)
{
	int ret;

	if (stm_demux->demux_running == true)
		return 0;

	if (stm_demux->demux_source == DEMUX_SOURCE_FE){
		ret = stm_dvb_connect_frontend(stm_demux, stm_demux->demod_id);
		if (ret)
			return ret;
	}

	ret = stm_te_demux_start(stm_demux->demux_object);
	if (ret){
		DMX_ERR("failed to start demux (%d)\n", ret);
		if (stm_demux->demux_source == DEMUX_SOURCE_FE)
			stm_dvb_disconnect_frontend(stm_demux);

		return ret;
	}
	stm_demux->demux_running = true;

	return 0;
}

static int stm_dvb_demux_stop(struct stm_dvb_demux_s *stm_demux)
{
	int ret;

	if (stm_demux->demux_running == false)
		return 0;

	ret = stm_te_demux_stop(stm_demux->demux_object);
	if (ret){
		DMX_ERR("failed to stop demux (%d)\n", ret);
		return ret;
	}

	if (stm_demux->demux_source == DEMUX_SOURCE_FE)
		stm_dvb_disconnect_frontend(stm_demux);

	stm_demux->demux_running = false;

	return 0;
}

static int stm_dvb_dmx_open(struct inode *node, struct file *file)
{
	struct dvb_device *dvbdev = file->private_data;
	struct dmxdev *dev = dvbdev->priv;
	struct stm_dvb_demux_s *stm_demux =
	    container_of(dev, struct stm_dvb_demux_s, dmxdev);
	int ret;

	/*
	 * Call the default open routine
	 */
	ret = stm_demux->dmx_ops.open(node, file);
	if (ret) {
		pr_err("%s(): Failed to open demux device\n", __func__);
		goto open_failed;
	}
	/*
	 * Check if the exclusive open of demux is allowed
	 */

	down_write(&stm_demux->rw_sem);

	if ((file->f_flags & O_EXCL) == O_EXCL) {
		/* The user would like an unused demux */
		if (stm_demux->users) {
			ret = -EBUSY;
			goto excl_open_failed;
		}
	}

	/*
	 * Open the TE demux object
	 */
	ret = stm_dvb_create_demux(stm_demux);
	if (ret) {
		pr_err(":%s(): Failed to open te demux object\n", __func__);
		goto excl_open_failed;
	}

	up_write(&stm_demux->rw_sem);

	return 0;

excl_open_failed:
	up_write(&stm_demux->rw_sem);
	stm_demux->dmx_ops.release(node, file);
open_failed:
	return ret;
}

/**
 * stm_dvb_dmx_chain_release() - remove filter chain from demux
 * @stm_demux   : ST demux
 * @dmxdevfilter: linux demux filter
 * Release the filter chain and adjust the pacing filter on TE
 */
void stm_dvb_dmx_chain_release(struct stm_dvb_demux_s *stm_demux,
				struct dmxdev_filter *dmxdevfilter)
{
	struct stm_dvb_filter_chain_s *filter_chain;

	down_write(&stm_demux->rw_sem);
	mutex_lock(&dmxdevfilter->mutex);

	/*
	 * Look if there's any existing chain for this demux. If we find
	 * one, then release one
	 */
	if (!(filter_chain = match_chain_from_filter(stm_demux, dmxdevfilter)))
		goto chain_release_done;

	/*
	 * Reset the pacing object
	 */
	if (filter_chain->output_filter->handle == stm_demux->pacing_object)
		stm_demux->pacing_object = NULL;

	stm_dmx_delete_chain(filter_chain);
	stm_demux->filter_count--;
	if (get_pcr_pes_type(dmxdevfilter->params.pes.pes_type) >= 0)
		stm_demux->pcr_type = DMX_PES_LAST;

	/*
	 * Re-assign the pacing object to a separate existing filter
	 */
	if (stm_demux->filter_count && stm_demux->pacing_object == NULL) {
		struct stm_dvb_filter_s *output_filter;

		output_filter = get_filter_pacing_output(stm_demux);
		if (!output_filter)
			goto chain_release_done;

		if (0 != stm_te_demux_set_control(stm_demux->demux_object,
					STM_TE_DEMUX_CNTRL_PACING_OUTPUT,
					(unsigned int) (output_filter->handle)))
			printk(KERN_ERR "Fail Pacing filter not set\n");
		else
			stm_demux->pacing_object = output_filter->handle;
	}

chain_release_done:
	mutex_unlock(&dmxdevfilter->mutex);
	up_write(&stm_demux->rw_sem);
}

/**
 * stm_dvb_dmx_release() - standard release callback from kernel
 */
static int stm_dvb_dmx_release(struct inode *node, struct file *file)
{
	struct dmxdev_filter *dmxdevfilter = file->private_data;
	struct dmxdev *dev = dmxdevfilter->dev;
	struct stm_dvb_demux_s *stm_demux =
	    container_of(dev, struct stm_dvb_demux_s, dmxdev);
	int ret;

	/*
	 * Stop the filter and release the chain
	 */
	stm_dvb_dmx_chain_release(stm_demux, dmxdevfilter);

	down_write(&stm_demux->rw_sem);

	/*
	 * If there's no filter left for this demux, then stop
	 * the data flow from/to demux
	 */
	if (!(stm_demux->filter_count)) {
		ret = stm_dvb_demux_stop(stm_demux);
		if (ret)
			DMX_ERR("failed to stop demux (%d)\n", ret);

		stm_demux->PlaybackContext = NULL;
	}

	ret = stm_dvb_delete_demux(stm_demux);
	if (ret)
		DMX_ERR("failed to delete the demux (%d)\n", ret);

	up_write(&stm_demux->rw_sem);

	ret = stm_demux->dmx_ops.release(node, file);
	if (ret)
		DMX_ERR("dmx standard release failed (%d)\n", ret);

	return ret;
}

static int stm_dvb_dvr_open(struct inode *node, struct file *file)
{
	struct dvb_device *dvbdev = file->private_data;
	struct dmxdev *dev = dvbdev->priv;
	struct stm_dvb_demux_s *stm_demux =
	    container_of(dev, struct stm_dvb_demux_s, dmxdev);
	int ret;

	if ((file->f_flags & O_ACCMODE) == O_RDWR) {
		DMX_ERR("Not supported rdwr, need to open twice\n");
		return -EINVAL;
	}

	ret = stm_demux->dvr_ops.open(node, file);
	if (ret){
		DMX_ERR("failed to open the dvr open (%d)\n", ret);
		return ret;
	}

	down_write(&stm_demux->rw_sem);

	ret = stm_dvb_create_demux(stm_demux);
	if (ret)
		goto error_create_demux;

	if ((file->f_flags & O_ACCMODE) == O_RDONLY)
		goto finish;

	/* In case of DVR write, we need to connect the dvr */
	if(stm_demux->demux_running == true){
		ret = stm_te_demux_stop(stm_demux->demux_object);
		if (ret){
			DMX_ERR("failed to stop demux (%d)\n", ret);
			goto error_demux_stop;
		}

		if (stm_demux->demux_source == DEMUX_SOURCE_FE){
			ret = stm_dvb_disconnect_frontend(stm_demux);
			if (ret){
				DMX_ERR("failed to disconnect the frontend (%d)\n", ret);
				goto error_disconnect_demod;
			}
		}
	}

	stm_demux->demux_source = DEMUX_SOURCE_DVR;
	ret = stm_dvb_connect_dvr(stm_demux);
	if (ret){
		DMX_ERR("failed to connect the dvr (%d)\n", ret);
		goto error_connect_dvr;
	}

	if(stm_demux->demux_running == true){
		ret = stm_te_demux_start(stm_demux->demux_object);
		if (ret){
			DMX_ERR("failed to restart demux (%d)\n", ret);
			goto error_demux_restart;
		}
	}

finish:
	up_write(&stm_demux->rw_sem);
	return ret;

error_demux_restart:
	stm_dvb_disconnect_dvr(stm_demux);
error_connect_dvr:
error_disconnect_demod:
	/* Probably doesn't make sense to restart again, we are broken */
	stm_demux->demux_running = false;
error_demux_stop:
	stm_dvb_delete_demux(stm_demux);
error_create_demux:
	up_write(&stm_demux->rw_sem);
	stm_demux->dvr_ops.release(node, file);
	return ret;
}

static int stm_dvb_dvr_release(struct inode *node, struct file *file)
{
	struct dvb_device *dvbdev = file->private_data;
	struct dmxdev *dev = dvbdev->priv;
	struct stm_dvb_demux_s *stm_demux =
	    container_of(dev, struct stm_dvb_demux_s, dmxdev);
	int ret;

	down_write(&stm_demux->rw_sem);

	if ((file->f_flags & O_ACCMODE) == O_WRONLY) {
		/* In case of DVR write, we need to disconnect the dvr */
		if(stm_demux->demux_running == true){
			ret = stm_te_demux_stop(stm_demux->demux_object);
			if (ret){
				DMX_ERR("failed to stop demux (%d)\n", ret);
				goto error_demux_stop;
			}
		}

		ret = stm_dvb_disconnect_dvr(stm_demux);
		if (ret){
			DMX_ERR("failed to disconnect the dvr (%d)\n", ret);
			goto error_disconnect_dvr;
		}
		stm_demux->demux_source = stm_demux->demux_default_source;

		if(stm_demux->demux_running == true){
			if (stm_demux->demux_source == DEMUX_SOURCE_FE){
				ret = stm_dvb_connect_frontend(stm_demux, stm_demux->demod_id);
				if (ret)
					DMX_DBG("connection to frontend failed(%d)\n", ret);
			}

			ret = stm_te_demux_start(stm_demux->demux_object);
			if (ret){
				DMX_ERR("failed to restart demux (%d)\n", ret);
				goto error_demux_restart;
			}
		}
	}

	ret = stm_dvb_delete_demux(stm_demux);
	if (ret){
		DMX_ERR("failed to delete demux (%d)\n", ret);
		goto error_delete_demux;
	}

	up_write(&stm_demux->rw_sem);

	return stm_demux->dvr_ops.release(node, file);

error_delete_demux:
	/* Can't do much, that is bad */
error_demux_restart:
error_disconnect_dvr:
	/* To have a proper state */
	stm_demux->demux_running = false;
error_demux_stop:
	up_write(&stm_demux->rw_sem);
	return -EIO;
}

static ssize_t stm_dvb_dmx_pcr_read(struct file* file,
				    struct stm_dvb_filter_s *filter,
				    char __user * buf, size_t buffer_size)
{
	int count = 0;
	int result = 0;
	struct dmx_pcr pcr;
	stm_te_pcr_t from_te;

	if (buffer_size < sizeof(from_te))
		return -EIO;

	if (file->f_flags & O_NONBLOCK) {
		/* We do not want to block here, test_for_data first */
		result = stm_memsink_test_for_data(filter->memsink_interface,
						&count);
		if (result)
			return -EIO;
		if (!count)
			/* No data available */
			return -EWOULDBLOCK;
		if (count < buffer_size)
			/* Clamp size of data to read to the size available to
			 * avoid blocking */
			buffer_size = count;
	}

	result = stm_memsink_pull_data(filter->memsink_interface,
				    &from_te, sizeof(from_te), &count);
	if (result)
		return -EIO;

	if (count != sizeof(from_te))
		return -EIO;

	pcr.system_time = from_te.system_time;
	pcr.pcr = from_te.pcr;

	result = copy_to_user((void __user *)buf, &pcr, sizeof(pcr));
	if (result) {
		DMX_ERR("failed to copy data to user (%d)\n", result);
		return result;
	}

	return count;
}

static unsigned int te_index_to_dvb_index(char *te_data, struct dmx_index *index)
{
	stm_te_ts_index_data_t *from_te = (stm_te_ts_index_data_t *)te_data;
	unsigned int i;

	index->pid = from_te->pid;
	index->event = DMX_INDEX_NONE;
	if (from_te->flags & STM_TE_INDEX_PUSI)
		index->event = DMX_INDEX_PUSI;
	else if (from_te->flags & STM_TE_INDEX_TO_EVEN_SCRAM)
		index->event = DMX_INDEX_TO_EVEN_SCRAM;
	else if (from_te->flags & STM_TE_INDEX_TO_ODD_SCRAM)
		index->event = DMX_INDEX_TO_ODD_SCRAM;
	else if (from_te->flags & STM_TE_INDEX_SCRAM_TO_CLEAR)
		index->event = DMX_INDEX_SCRAM_TO_CLEAR;
	else if (from_te->flags & STM_TE_INDEX_FIRST_REC_PACKET)
		index->event = DMX_INDEX_FIRST_REC_PACKET;
	else if (from_te->flags & STM_TE_INDEX_START_CODE)
		index->event = DMX_INDEX_START_CODE;
	else if (from_te->flags & STM_TE_INDEX_ADAPTATION_EXT)
		index->event = DMX_INDEX_ADAPTATION_EXT;
	else if (from_te->flags & STM_TE_INDEX_TS_PRIVATE_DATA)
		index->event = DMX_INDEX_TS_PRIVATE_DATA;
	else if (from_te->flags & STM_TE_INDEX_SPLICING_POINT)
		index->event = DMX_INDEX_SPLICING_POINT;
	else if (from_te->flags & STM_TE_INDEX_OPCR)
		index->event = DMX_INDEX_OPCR;
	else if (from_te->flags & STM_TE_INDEX_PCR)
		index->event = DMX_INDEX_PCR;
	else if (from_te->flags & STM_TE_INDEX_ES_PRIORITY)
		index->event = DMX_INDEX_ES_PRIORITY;
	else if (from_te->flags & STM_TE_INDEX_RANDOM_ACCESS)
		index->event = DMX_INDEX_RANDOM_ACCESS;
	else if (from_te->flags & STM_TE_INDEX_DISCONTINUITY)
		index->event = DMX_INDEX_DISCONTINUITY;
	else if (from_te->flags & STM_TE_INDEX_PTS)
		index->event = DMX_INDEX_PTS;

	index->packet_count = from_te->packet_count;
	index->pcr = from_te->pcr;
	index->system_time = from_te->system_time;
	index->mpeg_start_code = from_te->mpeg_start_code;
	index->mpeg_start_code_offset = from_te->mpeg_start_code_offset;
	index->extra_bytes = from_te->number_of_extra_bytes;

	/* Copy the meta-data into 'extra[i]'.
	   Here we expect the 'extra_bytes' to be of max of 5 bytes(PTS
	   returned from TE is 5 bytes).
	 */
	if (((index->extra_bytes > 0) && (index->extra_bytes <= 5))
	    && ((index->event == DMX_INDEX_START_CODE) ||
	    (index->event == DMX_INDEX_PTS))) {
		for (i = 0; i < index->extra_bytes; i++)
			index->extra[i] =
			    *(te_data + sizeof(stm_te_ts_index_data_t) + i);
	}

	return sizeof(stm_te_ts_index_data_t) + from_te->number_of_extra_bytes;
}

static ssize_t stm_dvb_dmx_index_read(struct file *file, struct stm_dvb_filter_s *filter,
				      char __user * buf, size_t buffer_size)
{
	unsigned int count = 0;
	int result = 0;
	struct dmx_index index;
	uint32_t te_data_count = 0;
	uint32_t te_read_count = 0;

	if (buffer_size < sizeof(struct dmx_index))
		return -EOVERFLOW;

	while( (buffer_size - count) >= sizeof(struct dmx_index) ){
		if (filter->buf_data) {
			/* Get from local STLinuxTV buffer */
			/* The TE never give us less than a full index */
			filter->buf_offset += te_index_to_dvb_index((filter->buf_data + filter->buf_offset), &index);

			result = copy_to_user((void __user *)buf, &index, sizeof(index));
			if (result) {
				DMX_ERR("failed to copy data to user (%d)\n", result);
				goto failed;
			}

			buf += sizeof(index);
			count += sizeof(index);

			memset(&index, 0, sizeof(index));

			/* Check if there are still data inside the local buffer */
			if (filter->buf_offset >= filter->buf_len){
				kfree(filter->buf_data);
				filter->buf_data = NULL;
				filter->buf_offset = filter->buf_len = 0;
			}
		}else{
			/* Get from lower layer */
			result = stm_memsink_test_for_data(filter->memsink_interface, &te_data_count);
			if (result){
				DMX_ERR("failed to retrieve information from memsink (%d)\n", result);
				goto failed;
			}

			if (te_data_count == 0)
				/* No data */
				break;

			/* That shouln't happen */
			if (te_data_count < sizeof(stm_te_ts_index_data_t)){
				DMX_ERR("TE return impossible number of data (%d)\n", result);
				result = -EIO;
				goto failed;
			}

			/*Allocate memory to store the pulled data. */
			filter->buf_data = (char *)kzalloc(te_data_count, GFP_KERNEL);
			if (!filter->buf_data) {
				DMX_ERR("failed to allocate memory\n");
				result = -ENOMEM;
				goto failed;
			}

			result = stm_memsink_pull_data(filter->memsink_interface, filter->buf_data,
							te_data_count, &te_read_count);
			if (result){
				DMX_ERR("failed to pull data from TE\n");
				kfree(filter->buf_data);
				filter->buf_data = NULL;
				result = -EIO;
				goto failed;
			}

			if (te_read_count != te_data_count){
				DMX_ERR("failed to pull data from TE\n");
				kfree(filter->buf_data);
				filter->buf_data = NULL;
				result = -EIO;
				goto failed;
			}

			filter->buf_len = te_read_count;
		}
	}

	if ((file->f_flags & O_NONBLOCK) && !count)
		return -EWOULDBLOCK;

	return count;

failed:
	return result;
}

/* Need to check here how much of the buffer has been consumed and only grab as needed based
 * on the space that was given to us */
static ssize_t stm_dvb_dmx_read(struct file *file,
				char __user * buf,
				size_t buffer_size, loff_t * ppos)
{
	int count = 0;
	struct dmxdev_filter *dmxdevfilter = file->private_data;
	struct dmxdev *dev = dmxdevfilter->dev;
	struct stm_dvb_demux_s *stm_demux =
		container_of(dev, struct stm_dvb_demux_s, dmxdev);
	struct stm_dvb_filter_chain_s *filter_chain = NULL;
	struct stm_dvb_filter_s *valid_filter;
	int ret = 0;

	down_read(&stm_demux->rw_sem);

	if (mutex_lock_interruptible(&dmxdevfilter->mutex)) {
		up_read(&stm_demux->rw_sem);
		return -ERESTARTSYS;
	}

	filter_chain = match_chain_from_filter(stm_demux, dmxdevfilter);

	if (!filter_chain)
		goto dmx_read_done;

	valid_filter = filter_chain->output_filter;

	switch (filter_chain->type) {
	case SECTION_CHAIN:
	{
		struct dmx_sct_filter_params *sec = &dmxdevfilter->params.sec;
		stm_event_info_t evt_info;
		int num_events;

		/*
		 * If demux open is non-blocking and timeout is specified,
		 * return an error as this combination is absurd
		 */
		if ((file->f_flags & O_NONBLOCK) && sec->timeout) {
			DMX_ERR("Timeout with non-block open not allowed\n");
			ret = -EINVAL;
			goto dmx_read_done;
		}

		/*
		 * In blocking open if there's a timeout, wait for specifed
		 * time till there's no data else ETIMEDOUT
		 */
		if (!(file->f_flags & O_NONBLOCK) && sec->timeout) {

			/*
			 * Check if the data is available now. It may happen
			 * that application may not issue an immediate read()
			 * after setting the filter. This will result in TE
			 * piling section data and return once per successful
			 * stm_event_wait() and probably obsolete data.
			 */
			ret = stm_memsink_test_for_data
				(valid_filter->memsink_interface, &count);

			if (ret) {
				ret = -EIO;
				goto dmx_read_done;
			}

			/*
			 * If we don't have any data at the moment, then wait.
			 */
			if (!count) {

				ret = stm_event_wait(valid_filter->evt_subs,
						sec->timeout * 1000,
						1, &num_events, &evt_info);

				/*
				 * If section data is not received, then
				 * stm_event_wait will return -ETIMEDOUT
				 */
				if (ret) {
					if (ret != -ETIMEDOUT)
						DMX_ERR("Event interface "
								" failed\n");
					DMX_DBG("Section Timeout received\n");
					goto dmx_read_done;
				}

				if (evt_info.event.event_id &
					STM_MEMSINK_EVENT_BUFFER_OVERFLOW) {
					ret = -EOVERFLOW;
					goto dmx_read_done;
				}
			}
		}

		if (file->f_flags & O_NONBLOCK) {

			/* We do not want to block here, test_for_data first */
			ret = stm_memsink_test_for_data
				(valid_filter->memsink_interface, &count);

			if (ret) {
				ret = -EIO;
				goto dmx_read_done;
			}

			/* No data available */
			if (!count) {
				ret = -EWOULDBLOCK;
				goto dmx_read_done;
			}
		}

		/*
		 * In case of SECTION memsink is a USER type,
		 * so no handling needed
		 */
		ret = stm_memsink_pull_data(valid_filter->memsink_interface,
					 buf, buffer_size, &count);
		if (ret) {
			ret = -EIO;
			goto dmx_read_done;
		}

		ret = count;

		break;
	}

	case PES_CHAIN:
		if (file->f_flags & O_NONBLOCK) {
			/* We do not want to block here,
			 * test_for_data first */
			ret =
				stm_memsink_test_for_data
				(valid_filter->memsink_interface, &count);
			if (ret) {
				ret = -EIO;
				goto dmx_read_done;
			}
			if (!count) {
				/* No data available */
				ret = -EWOULDBLOCK;
				goto dmx_read_done;
			}
			if (count < buffer_size)
				/* Clamp size of data to read to the size
				 * available to avoid blocking */
				buffer_size = count;
		}

		/* In case of PES memsink is a USER type, so no handling needed */
		ret =
			stm_memsink_pull_data(valid_filter->
					memsink_interface, buf,
					buffer_size, &count);
		if (ret) {
			ret = -EIO;
			goto dmx_read_done;
		}

		ret = count;
		break;
	case PCR_CHAIN:
		/* In case of PCR memsink that is KERNEL type, we do reformating before copy_to_user */
		ret =
			stm_dvb_dmx_pcr_read(file,
					valid_filter,
					buf, buffer_size);
		break;
	case INDEX_CHAIN:
		/* For INDEX memsink we need to reformat the data to be sent to user-space */
		ret =
			stm_dvb_dmx_index_read(file, valid_filter, buf, buffer_size);
		break;
	default:
		/* That shouldn't happen */
		DMX_ERR("filter type wrong - that is bad\n");
		BUG();
		break;
	}

dmx_read_done:
	mutex_unlock(&dmxdevfilter->mutex);
	up_read(&stm_demux->rw_sem);
	return ret;
}

/* handles Insert PID filters,
*/
static int stm_dvb_dmx_set_insert_filter(struct stm_dvb_demux_s *stm_demux,
				    struct dmxdev_filter *dmxdevfilter,
				    struct dmx_insert_filter_params *filter)
{
	int result;
	char memsink_name[STM_REGISTRY_MAX_TAG_SIZE];
	struct stm_dvb_filter_chain_s *existing_output_filter_chain;
	struct stm_dvb_filter_chain_s *new_chain;
	struct stm_dvb_filter_s *output_filter=NULL;
	struct stm_dvb_filter_s *input_filter=NULL;
	int pid = filter->pid;
	char *data = NULL;

	existing_output_filter_chain = get_primary_ts_chain(stm_demux);
	if (existing_output_filter_chain) {
		/* Already have one with that pid so link the new ones output to
		* this one */
		output_filter = existing_output_filter_chain->output_filter;
	}

	new_chain =
		add_chain_to_demux(stm_demux, input_filter, output_filter, pid,
				TS_MAP, dmxdevfilter,INSERT_PID_FILTER);
	if (!new_chain)
		return -EIO;

	/* FIXME! we have to do this first for the PID Insert filter */
	result = stm_dvb_chain_start(new_chain);

	if (result) {
		goto error_chain_start;
	}

	if (!new_chain->output_filter->memsink_interface)
	{
		snprintf(memsink_name, STM_REGISTRY_MAX_TAG_SIZE,
				"TS_TO_SINK-%d:%d:%p",
				stm_demux->demux_id, pid, new_chain->output_filter);

		result =
			attach_sink_to_chain(new_chain, NULL, memsink_name, USER);

		if (result)
			goto error_chain_start;

	}

	/*
	 * Maintaining a kernel buffer for user-data. Not sure if
	 * get_user_pages() for a max of 4KB data is worth the overhead.
	 */
	data = kmalloc(filter->data_size, GFP_KERNEL);
	if (!data) {
		DMX_ERR("Out of memory for insertion data\n");
		result = -ENOMEM;
		goto error_chain_start;
	}

	result = copy_from_user(data,
			(const void __user *)filter->data, filter->data_size);
	if (result) {
		DMX_ERR("Failed to copy insertion data (%d)\n", result);
		goto error_chain_start;
	}

	/*configures a pid insertion filter object */
	result = stm_te_pid_ins_filter_set(new_chain->input_filter->handle,
				   data, filter->data_size, filter->freq_ms);
	if (result) {
		DMX_ERR("Setting insertion parameter failed (%d)\n", result);
		goto error_chain_start;
	}

	/*
	 * Free the memory as consumed by TE now
	 */
	kfree(data);

	data = NULL;

#ifdef CONFIG_STLINUXTV_CRYPTO
	stm_dvb_demux_ca_set_pid(stm_demux, filter->pid);
#endif

	result = stm_dvb_demux_start(stm_demux);
	if (result) {
		DMX_ERR("Failed to start the demux (%d)\n", result);
		goto error_chain_start;
	}

	stm_demux->filter_count++;

	return 0;

error_chain_start:
	if (data)
		kfree(data);
	stm_dmx_delete_chain(new_chain);
	return result;
}

/* handles Replace PID filters,
*/
static int stm_dvb_dmx_set_replace_filter(struct stm_dvb_demux_s *stm_demux,
					  struct dmxdev_filter *dmxdevfilter,
					  struct dmx_replace_filter_params *filter)
{
	int result;
	char memsink_name[STM_REGISTRY_MAX_TAG_SIZE];
	struct stm_dvb_filter_chain_s *existing_output_filter_chain;
	struct stm_dvb_filter_chain_s *new_chain;
	struct stm_dvb_filter_s *output_filter=NULL;
	struct stm_dvb_filter_s *input_filter=NULL;
	int pid = filter->pid;
	char *data = NULL;

	existing_output_filter_chain = get_primary_ts_chain(stm_demux);
	if (existing_output_filter_chain) {
		/* Already have one with that pid so link the new ones output to
		* this one */
		output_filter = existing_output_filter_chain->output_filter;
	}

	new_chain =
		add_chain_to_demux(stm_demux, input_filter, output_filter, pid,
				TS_MAP, dmxdevfilter,REPLACE_PID_FILTER);
	if (!new_chain)
		return -EIO;

	/*
	 * FIXME! we have to do this first for the PID Replace filter
	 */
	result = stm_dvb_chain_start(new_chain);
	if (result) {
		goto error_chain_start;
	}

	if (!new_chain->output_filter->memsink_interface)
	{
		snprintf(memsink_name, STM_REGISTRY_MAX_TAG_SIZE,
				"TS_TO_SINK-%d:%d:%p",
				stm_demux->demux_id, pid, new_chain->output_filter);

		result =
			attach_sink_to_chain(new_chain, NULL, memsink_name, USER);

		if (result)
			goto error_chain_start;

	}

	/*
	 * Maintaining a kernel buffer for user-data. get_user_pages()
	 * for a max of 183 bytes data doesn't seem to be worth the overhead.
	 */
	data = kmalloc(filter->data_size, GFP_KERNEL);
	if (!data) {
		DMX_ERR("Out of memory for replacement pid data\n");
		result = -ENOMEM;
		goto error_chain_start;
	}

	result = copy_from_user(data,
			(const void __user *)filter->data, filter->data_size);
	if (result) {
		DMX_ERR("Failed to copy replacement pid data (%d)\n", result);
		goto error_chain_start;
	}

	/*
	 * Configure TE for replace filter
	 */
	result = stm_te_pid_rep_filter_set(new_chain->input_filter->handle,
					data, filter->data_size);
	if (result) {
		DMX_ERR("Replace filter set control failed (%d)\n", result);
		goto error_chain_start;
	}

	/*
	 * Free the memory as consumed by TE now
	 */
	kfree(data);

	data = NULL;

#ifdef CONFIG_STLINUXTV_CRYPTO
	stm_dvb_demux_ca_set_pid(stm_demux, filter->pid);
#endif

	result = stm_dvb_demux_start(stm_demux);
	if (result) {
		DMX_ERR("Failed to start demux (%d)\n", result);
		goto error_chain_start;
	}

	stm_demux->filter_count++;

	return 0;

error_chain_start:
	if (data)
		kfree(data);
	stm_dmx_delete_chain(new_chain);
	return result;
}

/* Policy regarding filter allocation is on a per demux basis and limit
 * are set by the demux. And hence why the logic for this is here and
 * not in the filter code */
static int stm_dvb_create_pcr_map(struct stm_dvb_demux_s *stm_demux,
				  struct dmxdev_filter *dmxdev_filter,
				  struct dmx_pes_filter_params *filter,
				  struct stm_dvb_filter_chain_s **chain)
{
	int result;
	struct stm_dvb_filter_chain_s *matched_chain;
	struct stm_dvb_filter_chain_s *new_chain;
	struct stm_dvb_filter_s *input_filter = NULL;

	/* Search for an existing same pid filter */
	matched_chain = match_filter_input_pid(stm_demux, filter->pid);
	if (matched_chain)
		input_filter = matched_chain->input_filter;

	new_chain =
	    add_chain_to_demux(stm_demux, input_filter, NULL, filter->pid,
			       PCR_MAP, dmxdev_filter,INPUT_PID_FILTER);
	if (!new_chain)
		return -EIO;

	/* immediate start */
	if (filter->flags & DMX_IMMEDIATE_START) {
		result = stm_dvb_chain_start(new_chain);
		if (result) {
			goto error_chain_start;
		}
	}

	*chain = new_chain;
	return 0;

error_chain_start:
	stm_dmx_delete_chain(new_chain);

	return result;
}

/* handles mapping between PID and TS filters,
 * This is a many to one mapping.
 * if there is already a TS fillter
 * then required PID filter is created then this is plummed into
 * the output filter */
static int stm_dvb_create_ts_map(struct stm_dvb_demux_s *stm_demux,
				 struct dmxdev_filter *dmxdevfilter,
				 struct dmx_pes_filter_params *filter)
{
	int result;
	int pid = filter->pid;
	char memsink_name[STM_REGISTRY_MAX_TAG_SIZE];
	struct stm_dvb_filter_chain_s *existing_output_filter_chain;
	struct stm_dvb_filter_chain_s *existing_input_filter_chain;
	struct stm_dvb_filter_chain_s *new_chain;
	struct stm_dvb_filter_s *output_filter = NULL;
	struct stm_dvb_filter_s *input_filter = NULL;

	/* Search for an existing same TS output filter */
	existing_output_filter_chain = get_primary_ts_chain(stm_demux);
	if (existing_output_filter_chain)
		output_filter = existing_output_filter_chain->output_filter;

	/* Search for an existing same pid filter */
	existing_input_filter_chain = match_filter_input_pid(stm_demux, pid);
	if (existing_input_filter_chain)
		input_filter = existing_input_filter_chain->input_filter;

	new_chain =
	    add_chain_to_demux(stm_demux, input_filter, output_filter, pid,
			       TS_MAP, dmxdevfilter,INPUT_PID_FILTER);
	if (!new_chain)
		return -EIO;

	/* check the immediate start */
	if (filter->flags & DMX_IMMEDIATE_START) {
		result = stm_dvb_chain_start(new_chain);
		if (result) {
			goto error_start_chain;
		}
	}

	if (filter->flags & DMX_TS_NO_FLUSH_ON_DETACH) {
		result = stm_te_filter_set_control(output_filter->handle,
				       STM_TE_OUTPUT_FILTER_CONTROL_FLUSHING_BEHAVIOUR,
				       STM_TE_FILTER_CONTROL_FLUSH_NONE);
		if (result) {
			pr_err("stm_te_filter_set_control failed (%d)\n", result);
			goto error_start_chain;
		}
	}

	if (!output_filter) {
		snprintf(memsink_name, STM_REGISTRY_MAX_TAG_SIZE,
			 "TS_TO_SINK-%d", stm_demux->demux_id);

		result =
		    attach_sink_to_chain(new_chain, NULL, memsink_name, USER);
		if (result)
			goto error_attach_sink;

		if (!stm_demux->pacing_object) {
			if (0 !=
			    stm_te_demux_set_control(stm_demux->demux_object,
						     STM_TE_DEMUX_CNTRL_PACING_OUTPUT,
						     (unsigned int)
						     (new_chain->output_filter->
						      handle))) {
				DMX_ERR("Pacing filter not set\n");
			} else {
				stm_demux->pacing_object =
				    new_chain->output_filter->handle;
			}
		}
	}

	return 0;

error_attach_sink:
error_start_chain:
	stm_dmx_delete_chain(new_chain);

	return result;
}

/* handles mapping between PID and PES filters,
 * This is a one to many mapping.
 * if there is already a PID fillter that matches
 * the required pid then this is plummed in
 * else a new one is created with corresponding output
 * filter then linked togther */
static int stm_dvb_create_pes_map(struct stm_dvb_demux_s *stm_demux,
				  struct dmxdev_filter *dmxdevfilter,
				  struct dmx_pes_filter_params *filter,
				  struct stm_dvb_filter_chain_s **chain)
{
	int result;
	int pid = filter->pid;
	struct stm_dvb_filter_chain_s *matched_chain;
	struct stm_dvb_filter_chain_s *new_chain;
	struct stm_dvb_filter_s *input_filter = NULL;

	/* Search for an existing same pid filter */
	matched_chain = match_filter_input_pid(stm_demux, pid);
	if (matched_chain)
		input_filter = matched_chain->input_filter;

	new_chain =
	    add_chain_to_demux(stm_demux, input_filter, NULL, pid, PES_MAP,
			       dmxdevfilter,INPUT_PID_FILTER);
	if (!new_chain)
		return -EIO;

	/* Checking the IMMEDIATE_START flag here and connecting the input filter to the output filter is a
	 * workaround to a bug in TE which forbid to connect the output filter to the SE without having first
	 * connected the input filter to the output filter
	 * check the immediate start */
	if (filter->flags & DMX_IMMEDIATE_START) {
		result = stm_dvb_chain_start(new_chain);
		if (result) {
			goto error_chain_start;
		}
	}

	*chain = new_chain;

	/* Set the pacing_object to anything if it is not set and update it
	 * if we create a PES_VIDEO */
	if (!(stm_demux->pacing_object)
	    || (get_video_dec_pes_type(filter->pes_type) >= 0)) {
		if (0 !=
		    stm_te_demux_set_control(stm_demux->demux_object,
					     STM_TE_DEMUX_CNTRL_PACING_OUTPUT,
					     (unsigned
					      int)(new_chain->output_filter->
						   handle))) {
		} else {
			stm_demux->pacing_object =
			    new_chain->output_filter->handle;
		}
	}

	return 0;

error_chain_start:
	stm_dmx_delete_chain(new_chain);

	return result;
}

/* handles mapping between PID and SEC filters,
 * This is a one to many mapping.
 * if there is already a PID fillter that matches
 * the required pid then this is plummed in
 * else a new one is created with corresponding output
 * filter then linked togther */
static int stm_dvb_create_section_map(struct stm_dvb_demux_s *stm_demux,
				      struct dmxdev_filter *dmxdevfilter,
				      struct dmx_sct_filter_params *filter)
{
	int result;
	int neg = 0;
	int mask = 0;
	int byte;
#define BLOCK_STEP  3
	char memsink_name[STM_REGISTRY_MAX_TAG_SIZE];
	char debug_string[(DMX_MAX_FILTER_SIZE * BLOCK_STEP) + 1];
	int pid = filter->pid;
	struct stm_dvb_filter_chain_s *matched_chain;
	struct stm_dvb_filter_chain_s *new_chain;
	struct stm_dvb_filter_s *input_filter = NULL;
	struct stm_dvb_filter_s *output_filter;
	struct dmx_section_filter sec_filter;

	/* Search for an existing same pid filter */
	matched_chain = match_filter_input_pid(stm_demux, pid);
	if (matched_chain)
		input_filter = matched_chain->input_filter;

	new_chain =
	    add_chain_to_demux(stm_demux, input_filter, NULL, pid, SECTION_MAP,
			       dmxdevfilter,INPUT_PID_FILTER);
	if (!new_chain)
		return -EIO;
	output_filter = new_chain->output_filter;

	/*
	 * Transport Engine default behaviour is to check CRC for all sections
	 * and discard packets whose CRC is not valid (where SSI=1)
	 */
	if (!(filter->flags & DMX_CHECK_CRC)) {
		result = stm_te_filter_set_control(output_filter->handle,
			STM_TE_SECTION_FILTER_CONTROL_DISCARD_ON_CRC_ERROR, 0);
		if (result) {
			DMX_ERR("TE config. not to drop CRC errored packets"
					" failed with error: %d\n", result);
			result = -EINVAL;
			goto error_filter_set_control;
		}
	}

	/*
	 * Transport Engine default behaviour is to push section data as
	 * a stream of bytes which are available at the moment. So,
	 * suppression of default behaviour is required to satisfy LinuxDVB
	 */
	result = stm_te_filter_set_control(output_filter->handle,
		STM_TE_OUTPUT_FILTER_CONTROL_READ_IN_QUANTISATION_UNITS, 1);
	if (result) {
		DMX_ERR("TE config. failed for packetised section read\n");
		goto error_filter_set_control;
	}

	/*
	 * If a single snapshot of section data is required, then configure
	 * Transport Engine to stop filtering when one packet has been acquired
	 */
	if (filter->flags & DMX_ONESHOT) {
		DMX_DBG("Configuring TE for a 1 packet section read\n");
		result = stm_te_filter_set_control(output_filter->handle,
				STM_TE_SECTION_FILTER_CONTROL_REPEATED, 0);
		if (result) {
			DMX_ERR("TE config. failed for 1 packet section read\n");
			goto error_filter_set_control;
		}
	}

	/* filter setting */

	/* invert mode */
	for (byte = 0; byte < DMX_FILTER_SIZE; byte++) {
		filter->filter.mode[byte] ^= 0xff;
	}

	memcpy(&sec_filter.filter_value[3],
	       &filter->filter.filter[1], DMX_FILTER_SIZE - 1);
	memcpy(&sec_filter.filter_mask[3],
	       &filter->filter.mask[1], DMX_FILTER_SIZE - 1);
	memcpy(&sec_filter.filter_mode[3],
	       &filter->filter.mode[1], DMX_FILTER_SIZE - 1);

	sec_filter.filter_value[0] = filter->filter.filter[0];
	sec_filter.filter_mask[0] = filter->filter.mask[0];
	sec_filter.filter_mode[0] = filter->filter.mode[0];
	sec_filter.filter_mask[1] = 0;
	sec_filter.filter_mask[2] = 0;

	for (byte = 0; byte < DMX_MAX_FILTER_SIZE; byte++) {
		sprintf(debug_string + (BLOCK_STEP * byte),
			"%02X:", sec_filter.filter_value[byte]);
	}
	DMX_DBG("filter_value = %s\n", debug_string);
	for (byte = 0; byte < DMX_MAX_FILTER_SIZE; byte++) {
		if ((byte < 1) || (byte > 2))
			mask |= sec_filter.filter_mask[byte];
		sprintf(debug_string + (BLOCK_STEP * byte),
			"%02X:", sec_filter.filter_mask[byte]);
	}
	DMX_DBG("filter_mask  = %s\n", debug_string);
	for (byte = 0; byte < DMX_MAX_FILTER_SIZE; byte++) {
		sprintf(debug_string + (BLOCK_STEP * byte),
			"%02X:", sec_filter.filter_mode[byte]);
	}
	DMX_DBG("filter_mode  = %s\n", debug_string);

	/* OK we need to know if we are setting up a positive or positive/negative match mode filter */
	for (byte = 3; byte < DMX_MAX_FILTER_SIZE; byte++) {
		if (sec_filter.filter_mode[byte] != 0xFF) {
			neg = 1;
			break;
		}
	}
	if (sec_filter.filter_mode[0] != 0xFF)
		neg = 1;

	if (!mask) {
		/* nothing to do so leave as an open tiny filter */
		DMX_DBG("Open section filter required\n");
	} else if (!neg) {
		DMX_DBG("Positive section filter required\n");

		if (0 !=
		    (result =
		     stm_te_section_filter_set(output_filter->handle,
					       DMX_MAX_FILTER_SIZE,
					       sec_filter.filter_value,
					       sec_filter.filter_mask))) {
			DMX_ERR("stm_te_section_filter_set() Error = %d\n", result);
			result = -EINVAL;
			goto error_filter_set_control;
		}
	} else {
		/* Note positive/negative match mode is only supported for 8 bytes length on TANGO hardware */
		DMX_DBG("Positive/negative filter required\n");
		if (0 !=
		    (result =
		     stm_te_section_filter_positive_negative_set
		     (output_filter->handle, 10, sec_filter.filter_value,
		      sec_filter.filter_mask, sec_filter.filter_mode))) {
			DMX_ERR("stm_te_section_filter_positive_negative_set() Error = %d\n", result);
			result = -EINVAL;
			goto error_filter_set_control;
		}
	}

	snprintf(memsink_name, STM_REGISTRY_MAX_TAG_SIZE,
		 "SEC_TO_SINK-%d:%d:%p", stm_demux->demux_id, pid,
		 output_filter->handle);

	result = attach_sink_to_chain(new_chain, NULL, memsink_name, USER);
	if (result)
		goto error_filter_set_control;

	if (!stm_demux->pacing_object) {
		if (0 != stm_te_demux_set_control(stm_demux->demux_object,
						  STM_TE_DEMUX_CNTRL_PACING_OUTPUT,
						  (unsigned
						   int)(output_filter->handle)))
		{
			DMX_ERR("Pacing filter not set\n");
		} else {
			stm_demux->pacing_object = output_filter->handle;
		}
	}

	/* check the immediate start */
	if (filter->flags & DMX_IMMEDIATE_START) {
		result = stm_dvb_chain_start(new_chain);
		if (result) {
			goto error_filter_set_control;
		}
	}

	return 0;

error_filter_set_control:
	stm_dmx_delete_chain(new_chain);
	return result;
}

/* handles mapping between PID and INDEX filters,
 */
static int stm_dvb_create_index_map(struct stm_dvb_demux_s *stm_demux,
					struct dmxdev_filter *dmxdevfilter,
					struct dmx_index_pid *idx_pid)
{
	int result;
	char memsink_name[STM_REGISTRY_MAX_TAG_SIZE];
	struct stm_dvb_filter_chain_s *matched_chain, *new_chain;
	struct stm_dvb_filter_s *input_filter = NULL;
	stm_te_ts_index_set_params_t index_params = {0};
	int pid = idx_pid->pid;

	/*
	 * Search for an existing same pid filter
	 */
	matched_chain = match_filter_input_pid(stm_demux, pid);
	if (matched_chain)
		input_filter = matched_chain->input_filter;

	new_chain =
	    add_chain_to_demux(stm_demux, input_filter, NULL, pid, INDEX_MAP,
			       dmxdevfilter,INPUT_PID_FILTER);
	if (!new_chain) {
		result = -EIO;
		goto error_new_chain;
	}

	/*
	 * Map the application input params to TE params
	 */
	memset(&index_params, 0, sizeof(index_params));
	index_params.index_definition = 0;

	if (idx_pid->flags & DMX_INDEX_PUSI)
		index_params.index_definition |= STM_TE_INDEX_PUSI;

	if (idx_pid->flags & DMX_INDEX_SCRAM_TO_CLEAR)
		index_params.index_definition |= STM_TE_INDEX_SCRAM_TO_CLEAR;

	if (idx_pid->flags & DMX_INDEX_TO_EVEN_SCRAM)
		index_params.index_definition |= STM_TE_INDEX_TO_EVEN_SCRAM;

	if (idx_pid->flags & DMX_INDEX_TO_ODD_SCRAM)
		index_params.index_definition |= STM_TE_INDEX_TO_ODD_SCRAM;

	if (idx_pid->flags & DMX_INDEX_DISCONTINUITY)
		index_params.index_definition |= STM_TE_INDEX_DISCONTINUITY;

	if (idx_pid->flags & DMX_INDEX_RANDOM_ACCESS)
		index_params.index_definition |= STM_TE_INDEX_RANDOM_ACCESS;

	if (idx_pid->flags & DMX_INDEX_ES_PRIORITY)
		index_params.index_definition |= STM_TE_INDEX_ES_PRIORITY;

	if (idx_pid->flags & DMX_INDEX_PCR)
		index_params.index_definition |= STM_TE_INDEX_PCR;

	if (idx_pid->flags & DMX_INDEX_OPCR)
		index_params.index_definition |= STM_TE_INDEX_OPCR;

	if (idx_pid->flags & DMX_INDEX_SPLICING_POINT)
		index_params.index_definition |= STM_TE_INDEX_SPLICING_POINT;

	if (idx_pid->flags & DMX_INDEX_TS_PRIVATE_DATA)
		index_params.index_definition |= STM_TE_INDEX_TS_PRIVATE_DATA;

	if (idx_pid->flags & DMX_INDEX_ADAPTATION_EXT)
		index_params.index_definition |= STM_TE_INDEX_ADAPTATION_EXT;

	if (idx_pid->flags & DMX_INDEX_FIRST_REC_PACKET)
		index_params.index_definition |= STM_TE_INDEX_FIRST_REC_PACKET;

	if (idx_pid->flags & DMX_INDEX_START_CODE)
		index_params.index_definition |= STM_TE_INDEX_START_CODE;

	if (idx_pid->flags & DMX_INDEX_PTS)
		index_params.index_definition |= STM_TE_INDEX_PTS;

	/*
	 * Application indicated that the start codes are available
	 */
	if ((index_params.index_definition &
		 STM_TE_INDEX_START_CODE) == STM_TE_INDEX_START_CODE) {

		int start_code_size;

		/*
		 * Check for sanity of start codes info passed
		 */
		if (!idx_pid->start_codes || (idx_pid->start_codes &&
					!idx_pid->number_of_start_codes)) {
			DMX_ERR("Invalid parameters for start codes\n");
			result = -EINVAL;
			goto error_new_chain;
		}

		/*
		 * Fill in the start code info for TE
		 */
		start_code_size = sizeof(uint8_t) *
					idx_pid->number_of_start_codes;

		index_params.number_of_start_codes =
					idx_pid->number_of_start_codes;
		index_params.start_codes = kzalloc(start_code_size, GFP_KERNEL);

		if (!index_params.start_codes) {
			DMX_ERR("Out of memory to store start codes\n");
			result = -ENOMEM;
			goto error_new_chain;
		}

		result = copy_from_user(index_params.start_codes,
				(const void __user *)idx_pid->start_codes,
				start_code_size);
		if (result) {
			DMX_ERR("Failed to copy start codes (%d)\n", result);
			goto error_new_chain;
		}

	}

	/*
	 * FIXME! we have to do this first for the ts index filter
	 */
	result = stm_dvb_chain_start(new_chain);
	if (result) {
		DMX_ERR("Starting index filter chain failed (%d)\n", result);
		goto error_new_chain;
	}

	/*
	 * Set what to index on
	 */
	result = stm_te_filter_set_compound_control
				(new_chain->output_filter->handle,
				STM_TE_TS_INDEX_FILTER_CONTROL_INDEX_SET,
				(const void *)&index_params);
	if (result) {
		DMX_ERR("Failed to set index filter control (%d)\n", result);
		goto error_new_chain;
	}

	/*
	 * Create and attach memsink for reading data
	 */
	snprintf(memsink_name, STM_REGISTRY_MAX_TAG_SIZE,
		 "INDEX_TO_SINK-%d:%d:%p",
		 stm_demux->demux_id, pid, new_chain->output_filter->handle);

	result = attach_sink_to_chain(new_chain, NULL, memsink_name, KERNEL);
	if (result)
		goto error_new_chain;

	stm_demux->filter_count++;

#ifdef CONFIG_STLINUXTV_CRYPTO
	stm_dvb_demux_ca_set_pid(stm_demux, idx_pid->pid);
#endif

	result = stm_dvb_demux_start(stm_demux);
	if (result) {
		DMX_ERR("Starting demux failed (%d)\n", result);
		goto error_new_chain;
	}

	/*
	 * TE is done copying the start-codes, release the memory here
	 */
	if (index_params.start_codes)
		kfree(index_params.start_codes);

	return 0;

error_new_chain:
	if (index_params.start_codes)
		kfree(index_params.start_codes);
	stm_dmx_delete_chain(new_chain);
	return result;
}

#ifdef CONFIG_STLINUXTV_DECODE_DISPLAY
/**
 * demux_connect_video_decoder() - setup video decoder for tunneled playback
 * @vid_ctx          : video decoder context
 * @output_filter    : demux handle of output filter
 * @playback_ctx     : demuxer playback context
 * @playback_created : updated to 1 to keep track who created playback
 */
static int demux_connect_video_decoder(struct VideoDeviceContext_s *vid_ctx,
				stm_object_h output_filter,
				struct PlaybackDeviceContext_s **playback_ctx,
				bool *playback_created,
				bool *play_status)
{
	int ret = 0;

	mutex_lock(&vid_ctx->viddev_mutex);
	mutex_lock(&vid_ctx->vidops_mutex);

	/*
	 * Video decoder needs to opened in RW/W mode to create a filter
	 */
	if (!vid_ctx->VideoOpenWrite) {
		pr_err("%s(): video decoder is not opened in RW/W mode\n", __func__);
		ret = -EBADF;
		goto connect_done;
	}

	/*
	 * Check if this decoder is configured to have a demux source?
	 */
	if (vid_ctx->VideoState.stream_source != VIDEO_SOURCE_DEMUX) {
		pr_err("%s(): video decoder not setup to be sink of demux\n", __func__);
		ret = -EINVAL;
		goto connect_done;
	}

	/*
	 * Allocate a playback if necessary, else use the earlier one
	 */
	if (*playback_ctx && (*playback_ctx)->Playback) {

		if (!vid_ctx->playback_context->Playback) {

			vid_ctx->playback_context = *playback_ctx;

		} else if (vid_ctx->playback_context !=  *playback_ctx) {

			pr_err("%s(): invalid pes_type, as video decoder is already used\n", __func__);
			ret = -EINVAL;
			goto connect_done;
		}
	}
	*playback_ctx = vid_ctx->playback_context;

	/*
	 * Create a new playback if required
	 */
	mutex_lock(&(*playback_ctx)->Playback_alloc_mutex);
	if (!(*playback_ctx)->Playback){

		ret = DvbPlaybackCreate((*playback_ctx)->Id,
					&(*playback_ctx)->Playback);
		if (ret) {
			pr_err("%s(): failed to create a new playback\n", __func__);
			mutex_unlock(&(*playback_ctx)->Playback_alloc_mutex);
			goto connect_done;
		}
		*playback_created = 1;
	}
	mutex_unlock(&(*playback_ctx)->Playback_alloc_mutex);


	/*
	 * If the decoder is stopped, we do nothing
	 */
	vid_ctx->demux_filter = output_filter;
	if (vid_ctx->VideoPlayState == STM_DVB_VIDEO_STOPPED) {
		pr_debug("%s(): video decoder is stopped and tunnel path is deferred\n", __func__);
		goto connect_done;
	}

	/*
	 * Start the decoder and connect to output filter
	 */
	ret = VideoIoctlPlay(vid_ctx);
	if (ret) {
		pr_err("%s(): failed to complete tunneled path\n",  __func__);
		goto play_failed;
	}
	*play_status = true;

	mutex_unlock(&vid_ctx->vidops_mutex);
	mutex_unlock(&vid_ctx->viddev_mutex);

	return 0;

play_failed:
	vid_ctx->demux_filter = NULL;
	if (*playback_created) {
		mutex_lock(&(*playback_ctx)->Playback_alloc_mutex);
		if (DvbPlaybackDelete((*playback_ctx)->Playback))
			pr_err("%s(): failed to delete playback\n", __func__);
		(*playback_ctx)->Playback = NULL;
		mutex_unlock(&(*playback_ctx)->Playback_alloc_mutex);
	}
connect_done:
	mutex_unlock(&vid_ctx->vidops_mutex);
	mutex_unlock(&vid_ctx->viddev_mutex);
	return ret;
}

/**
 * demux_disconnect_video_decoder() - stop video decoder if started by demuxer
 * @vid_ctx          : video decoder context
 * @playback_ctx     : demuxer playback context
 * @playback_created : if playback created, we delete it
 */
static void demux_disconnect_video_decoder(struct VideoDeviceContext_s *vid_ctx,
				struct PlaybackDeviceContext_s **playback_ctx,
				bool playback_created)
{
	mutex_lock(&vid_ctx->viddev_mutex);
	mutex_lock(&vid_ctx->vidops_mutex);

	/*
	 * If video decoder closed down under, then do nothing
	 */
	if (!vid_ctx->VideoOpenWrite) {
		pr_debug("%s(): video decoder already closed or not in RW/W mode\n", __func__);
		goto disconnect_done;
	}

	/*
	 * Check if this decoder is configured to have a demux source?
	 */
	if (vid_ctx->VideoState.stream_source != VIDEO_SOURCE_DEMUX) {
		pr_err("%s(): video decoder not setup to be sink of demux\n", __func__);
		goto disconnect_done;
	}

	/*
	 * If the decoder is started by demuxer, then we stop it
	 */
	if (vid_ctx->VideoPlayState != STM_DVB_VIDEO_STOPPED)
		VideoIoctlStop(vid_ctx, DVB_OPTION_VALUE_DISABLE);

	vid_ctx->demux_filter = NULL;

	/*
	 * Delete the playback context
	 */
	if (playback_created) {
		mutex_lock(&(*playback_ctx)->Playback_alloc_mutex);
		if (DvbPlaybackDelete((*playback_ctx)->Playback))
			pr_err("%s(): failed to delete playback\n", __func__);
		(*playback_ctx)->Playback = NULL;
		mutex_unlock(&(*playback_ctx)->Playback_alloc_mutex);
	}

disconnect_done:
	mutex_unlock(&vid_ctx->vidops_mutex);
	mutex_unlock(&vid_ctx->viddev_mutex);
}

/**
 * demux_connect_audio_decoder() - setup audio decoder for tunneled playback
 * @aud_ctx          : audio decoder context
 * @output_filter    : demux handle of output filter
 * @playback_ctx     : demuxer playback context
 * @playback_created : updated to 1 to keep track who created playback
 */
static int demux_connect_audio_decoder(struct AudioDeviceContext_s *aud_ctx,
				stm_object_h output_filter,
				struct PlaybackDeviceContext_s **playback_ctx,
				bool *playback_created,
				bool *play_status)
{
	int ret = 0;

	mutex_lock(&aud_ctx->auddev_mutex);
	mutex_lock(&aud_ctx->audops_mutex);

	/*
	 * Audio decoder needs to be opened in RW/W mode to allow filter creation
	 */
	if (!aud_ctx->AudioOpenWrite) {
		pr_err("%s(): audio device not opened in RW/W mode\n", __func__);
		ret = -EBADF;
		goto connect_done;
	}

	/*
	 * Check this decoder is configured to have a demux source?
	 */
	if (aud_ctx->AudioState.stream_source != AUDIO_SOURCE_DEMUX){
		pr_err("%s(): audio decoder not configure for demux source\n", __func__);
		ret = -EINVAL;
		goto connect_done;
	}

	/*
	 * Allocate a playback if necessary, else reuse earlier one
	 */
	if (*playback_ctx && (*playback_ctx)->Playback) {

		if (!aud_ctx->playback_context->Playback) {

			aud_ctx->playback_context = *playback_ctx;

		} else if (aud_ctx->playback_context !=  *playback_ctx){
			pr_err("%s(): invalid pes_type, as video decoder is already used\n", __func__);
			ret = -EINVAL;
			goto connect_done;
		}
	}
	*playback_ctx = aud_ctx->playback_context;

	/*
	 * Allocate a new playback if required
	 */
	mutex_lock(&(*playback_ctx)->Playback_alloc_mutex);
	if (!(*playback_ctx)->Playback){

		ret = DvbPlaybackCreate((*playback_ctx)->Id,
						&(*playback_ctx)->Playback);
		if (ret) {
			pr_err("%s(): failed to create a new playback\n", __func__);
			mutex_unlock(&(*playback_ctx)->Playback_alloc_mutex);
			goto connect_done;
		}
		*playback_created = 1;
	}
	mutex_unlock(&(*playback_ctx)->Playback_alloc_mutex);


	/*
	 * If the decoder is stopped, we do nothing
	 */
	aud_ctx->demux_filter = output_filter;
	if (aud_ctx->AudioPlayState == STM_DVB_AUDIO_STOPPED){
		pr_debug("%s(): audio decoder is stopped and tunnel path is deferred\n", __func__);
		goto connect_done;
	}

	/*
	 * Complete the tunneled path, by completing the play
	 */
	ret = AudioIoctlPlay(aud_ctx);
	if (ret) {
		pr_debug("%s(): failed to complete the tunneled path\n", __func__);
		goto play_failed;
	}
	*play_status = true;

	mutex_unlock(&aud_ctx->audops_mutex);
	mutex_unlock(&aud_ctx->auddev_mutex);

	return 0;

play_failed:
	aud_ctx->demux_filter = NULL;
	if (playback_created) {
		mutex_lock(&(*playback_ctx)->Playback_alloc_mutex);
		if (DvbPlaybackDelete((*playback_ctx)->Playback))
			pr_err("%s(): failed to delete playback\n", __func__);
		(*playback_ctx)->Playback = NULL;
		mutex_unlock(&(*playback_ctx)->Playback_alloc_mutex);
	}
connect_done:
	mutex_unlock(&aud_ctx->audops_mutex);
	mutex_unlock(&aud_ctx->auddev_mutex);
	return ret;
}

/**
 * demux_disconnect_audio_decoder() - stop audio decoder if started by demux
 * @aud_ctx          : audio decoder context
 * @playback_ctx     : demuxer playback context
 * @playback_created : updated to 1 to keep track who created playback
 */
static void demux_disconnect_audio_decoder(struct AudioDeviceContext_s *aud_ctx,
				struct PlaybackDeviceContext_s **playback_ctx,
				bool playback_created)
{
	mutex_lock(&aud_ctx->auddev_mutex);
	mutex_lock(&aud_ctx->audops_mutex);

	/*
	 * Audio decoder closed down under
	 */
	if (!aud_ctx->AudioOpenWrite) {
		pr_debug("%s(): audio device not opened in RW/W mode\n", __func__);
		goto disconnect_done;
	}

	/*
	 * Check this decoder is configured to have a demux source?
	 */
	if (aud_ctx->AudioState.stream_source != AUDIO_SOURCE_DEMUX){
		pr_err("%s(): audio decoder not configure for demux source\n", __func__);
		goto disconnect_done;
	}

	/*
	 * If the decoder is started by demux, we stop it
	 */
	if (aud_ctx->AudioPlayState != STM_DVB_AUDIO_STOPPED)
		AudioIoctlStop(aud_ctx);

	aud_ctx->demux_filter = NULL;

	/*
	 * Delete the playback if it was created by audio decoder
	 */
	if (playback_created) {
		mutex_lock(&(*playback_ctx)->Playback_alloc_mutex);
		if (DvbPlaybackDelete((*playback_ctx)->Playback))
			pr_err("%s(): failed to delete playback\n", __func__);
		(*playback_ctx)->Playback = NULL;
		mutex_unlock(&(*playback_ctx)->Playback_alloc_mutex);
	}

disconnect_done:
	mutex_unlock(&aud_ctx->audops_mutex);
	mutex_unlock(&aud_ctx->auddev_mutex);
}

static int stm_dvb_create_pes_tunneling(struct stm_dvb_demux_s *stm_demux,
					dmx_pes_type_t pes_type,
					struct stm_dvb_filter_chain_s *chain)
{
	int ret = 0, dec_id, is_pcr = 0;
	bool playback_created, play_status = false;
	stm_object_h playback, decoder_obj;
	char playback_name[32], playstream_name[32];
	struct VideoDeviceContext_s *vid_ctx = NULL;
	struct AudioDeviceContext_s *aud_ctx = NULL;
	struct DvbContext_s *dvb_ctx = stm_demux->DvbContext;
	struct stm_dvb_filter_chain_s *pcr_chain;

	/*
	 * Check the sanity of pes_type to make sure, we don't go ahead
	 * with invalid pcr type
	 */
	if ((pes_type < 0) || (pes_type >= DMX_PES_LAST) ||
				    (pes_type == DMX_PES_OTHER)) {
		pr_err("%s((): invalid pes_type for tunneling\n", __func__);
		ret = -EINVAL;
		goto setup_failed;
	}

	/*
	 * Find out the decoder id for finding out the decoder context
	 */
	if ((dec_id = get_video_dec_pes_type(pes_type)) >= 0) {

		if (dec_id >= dvb_ctx->video_dev_nb) {
			pr_err("%s(): video pes_type greater than max decoders\n", __func__);
			ret = -EINVAL;
			goto setup_failed;
		}
		vid_ctx = &dvb_ctx->VideoDeviceContext[dec_id];

	} else if ((dec_id = get_audio_dec_pes_type(pes_type)) >= 0) {

		if (dec_id >= dvb_ctx->audio_dev_nb){
			pr_err("%s(): audio pes_type greater than max decoders\n", __func__);
			ret = -EINVAL;
			goto setup_failed;
		}
		aud_ctx = &dvb_ctx->AudioDeviceContext[dec_id];
	} else {
		dec_id = get_pcr_pes_type(pes_type);
		if (dec_id > dvb_ctx->video_dev_nb) {
			pr_err("%s(): invalid pcr type for max decoders\n", __func__);
			goto setup_failed;
		}
		is_pcr = 1;
	}

	/*
	 * With demuxer relying on the playback context of audio/video
	 * decoder, we cannot associate pcr to any playback context if
	 * audio/video filters are not created before pcr, so, we save
	 * pcr_id and exit.
	 */
	if (is_pcr && !stm_demux->PlaybackContext) {
		pr_debug("%s(): No playback context to associate PCR to\n", __func__);
		stm_demux->pcr_type = pes_type;
		goto setup_failed;
	}

	/*
	 * Create tunneled audio/video decoder paths
	 */
	if (vid_ctx) {

		ret = demux_connect_video_decoder(vid_ctx,
					chain->output_filter->handle,
					&stm_demux->PlaybackContext,
					&playback_created,
					&play_status);
		if (ret) {
			pr_err("%s(): failed to create video tunneled path\n", __func__);
			goto setup_failed;
		}

		snprintf(playstream_name, 32, "dvb%d.video%d",
					dvb_ctx->DvbAdapter->num, dec_id);

	} else if (aud_ctx) {

		ret = demux_connect_audio_decoder(aud_ctx,
					chain->output_filter->handle,
					&stm_demux->PlaybackContext,
					&playback_created,
					&play_status);
		if (ret) {
			pr_err("%s(): failed to create audio tunneled path\n", __func__);
			goto setup_failed;
		}
		snprintf(playstream_name, 32, "dvb%d.audio%d",
					 dvb_ctx->DvbAdapter->num, dec_id);
	}
	stm_demux->PlaybackContext->stm_demux = stm_demux;

	/*
	 * We find out the playback and play stream object and connect to TE
	 */
	snprintf(playback_name, 32, "playback%d", stm_demux->PlaybackContext->Id);

	ret = stm_registry_get_object(STM_REGISTRY_INSTANCES, playback_name, &playback);
	if (ret) {
		pr_err("%s(): failed to get playback object\n", __func__);
		goto get_playback_failed;
	}
	decoder_obj = playback;

	/*
	 * If there's no pcr, then we need to connect play stream to TE
	 */
	if (!is_pcr) {

		/*
		 * If the play was successful, then only get the play stream
		 */
		if (play_status) {

			ret = stm_registry_get_object(playback,
						playstream_name, &decoder_obj);
			if (ret) {
				pr_err("%s(): failed to get play stream object\n", __func__);
				goto get_playback_failed;
			}
		}

		/*
		 * If we have the stored pcr id, then use that to associate
		 * pcr with the playback.
		 */
		if ((get_pcr_pes_type(stm_demux->pcr_type) >= 0) &&
				((pcr_chain = demux_get_pcr_by_type(stm_demux, stm_demux->pcr_type)))) {

			ret = attach_sink_to_chain(pcr_chain, playback, NULL, 0);
			if (ret) {
				pr_err("%s(): failed to connect pcr to playback\n", __func__);
				goto get_playback_failed;
			}
			stm_demux->pcr_type = DMX_PES_LAST;
		}
	}

	/*
	 * Now the real connect between TE to SE
	 */
	if (is_pcr || play_status) {
		ret = attach_sink_to_chain(chain, decoder_obj, NULL, 0 );
		if (ret) {
			pr_err("%s(): failed to attach decoder object to output filter\n", __func__);
			goto get_playback_failed;
		}
	}

	return 0;

get_playback_failed:
	if (vid_ctx)
		demux_disconnect_video_decoder(vid_ctx,
				&stm_demux->PlaybackContext, playback_created);
	else if (aud_ctx)
		demux_disconnect_audio_decoder(aud_ctx,
				&stm_demux->PlaybackContext, playback_created);
setup_failed:
	return ret;
}
#endif

static int stm_dvb_dmx_set_pes_filter(struct dmxdev_filter *dmxdevfilter,
				      struct dmx_pes_filter_params *filter)
{
	struct dmxdev *dev = dmxdevfilter->dev;
	struct stm_dvb_demux_s *stm_demux =
	    container_of(dev, struct stm_dvb_demux_s, dmxdev);
	dmx_output_t out_type = filter->output;
	struct stm_dvb_filter_chain_s *chain;
	char memsink_name[STM_REGISTRY_MAX_TAG_SIZE];
	stm_data_mem_type_t address;

	int result = 0, pcr_id = 0;

	pcr_id = get_pcr_pes_type(filter->pes_type);
	if (pcr_id >= 0) {
		if ((out_type == DMX_OUT_TSDEMUX_TAP)
		    || (out_type == DMX_OUT_TS_TAP)) {
			DMX_ERR("For Pes type of DMX_PES_PCR output type is invalid \n");
			return -EINVAL;
		}
	}

	/* create object, chain */
	switch (out_type) {
	case DMX_OUT_TAP:
		if (pcr_id >= 0) {
			DMX_DBG("Setting PCR\n");
			result = stm_dvb_create_pcr_map(stm_demux,
							dmxdevfilter,
							filter, &chain);
			if (result)
				break;

			snprintf(memsink_name, STM_REGISTRY_MAX_TAG_SIZE,
				 "PCR_TO_SINK-%d:%d:%p", stm_demux->demux_id,
				 filter->pid, chain->output_filter->handle);
			address = KERNEL;
		} else {
			DMX_DBG("Setting PES\n");
			result = stm_dvb_create_pes_map(stm_demux,
							dmxdevfilter, filter,
							&chain);
			if (result)
				break;

			snprintf(memsink_name, STM_REGISTRY_MAX_TAG_SIZE,
				 "PCR_TO_SINK-%d:%d:%p", stm_demux->demux_id,
				 filter->pid, chain->output_filter->handle);
			address = USER;
		}

		result =
		    attach_sink_to_chain(chain, NULL, memsink_name, address);
		if (result)
			stm_dmx_delete_chain(chain);

		break;
	case DMX_OUT_TSDEMUX_TAP:
		DMX_DBG("Setting TS - Read from Demux\n");
		result = stm_dvb_create_ts_map(stm_demux, dmxdevfilter, filter);
		break;
	case DMX_OUT_TS_TAP:
		DMX_DBG("Setting TS - Read from Dvr\n");
		result = stm_dvb_create_ts_map(stm_demux, dmxdevfilter, filter);
		break;
#ifdef CONFIG_STLINUXTV_DECODE_DISPLAY
	case DMX_OUT_DECODER:
		DMX_DBG("Setting Tunneled path\n");
		if (pcr_id >= 0)
			result = stm_dvb_create_pcr_map(stm_demux,
							dmxdevfilter,
							filter, &chain);
		else
			result =
			    stm_dvb_create_pes_map(stm_demux, dmxdevfilter,
						   filter, &chain);

		if (result)
			break;

		result =
		    stm_dvb_create_pes_tunneling(stm_demux, filter->pes_type,
						 chain);
		if (result) {
			stm_dmx_delete_chain(chain);
			break;
		}

		break;
#endif
	default:
		result = -EINVAL;
	}

	if (result) {
		DMX_ERR("Error creating pes, ts map fitler chain\n");
		return result;
	}

#ifdef CONFIG_STLINUXTV_CRYPTO
	stm_dvb_demux_ca_set_pid(stm_demux, filter->pid);
#endif

	stm_demux->filter_count++;

	stm_dvb_demux_start(stm_demux);

	return 0;
}

static int stm_dvb_dmx_set_sec_filter(struct dmxdev_filter *dmxdevfilter,
				      struct dmx_sct_filter_params *filter)
{
	struct dmxdev *dev = dmxdevfilter->dev;
	struct stm_dvb_demux_s *stm_demux =
	    container_of(dev, struct stm_dvb_demux_s, dmxdev);
	int result = 0;

	DMX_DBG("Using HARDWARE Filtering\n");
	result = stm_dvb_create_section_map(stm_demux, dmxdevfilter, filter);
	if (result) {
		DMX_ERR("Error creating pes, ts map fitler chain\n");
		return result;
	}

	stm_demux->filter_count++;

#ifdef CONFIG_STLINUXTV_CRYPTO
	stm_dvb_demux_ca_set_pid(stm_demux, filter->pid);
#endif

	stm_dvb_demux_start(stm_demux);

	return 0;
}

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 35))
static int stm_dvb_dmx_ioctl(struct inode *inode, struct file *file,
			     unsigned int cmd, unsigned long arg)
#else
static long stm_dvb_dmx_ioctl(struct file *file,
			      unsigned int cmd, unsigned long arg)
#endif
{
	int result = 0;
	struct dmxdev_filter *dmxdev_filter =
	    (struct dmxdev_filter *)file->private_data;
	struct dmxdev *dev = (struct dmxdev *)dmxdev_filter->dev;
	struct stm_dvb_demux_s *stm_demux =
	    container_of(dev, struct stm_dvb_demux_s, dmxdev);

	/* In case of SET.*FILTER, we must remove the existing filter if applicable */
	switch (cmd) {
	case DMX_SET_INDEX_FILTER:
	case DMX_SET_PES_FILTER:
	case DMX_SET_FILTER:
	case DMX_SET_INS_FILTER:
	case DMX_SET_REP_FILTER:

		/*
		 * If a new filter is to be configured on the same fd, then
		 * we release the chain specific data and the chain itself.
		 */
		DMX_DBG("Remove the existing Filter\n");
		stm_dvb_dmx_chain_release(stm_demux, dmxdev_filter);
	}

	switch (cmd) {
	case DMX_SET_INDEX_FILTER:
	{
		struct dmx_index_filter_params index_filter;
		struct dmx_index_pid idx_pid;

		DMX_DBG("IN>> setting index filter\n");

		result = copy_from_user(&index_filter,
			(const void __user *)arg, sizeof(index_filter));
		if (result) {
			DMX_ERR("failed to copy data from user (%d)\n", result);
			return result;
		}

		result = copy_from_user(&idx_pid,
				(const void __user *)index_filter.pids,
				sizeof(idx_pid));
		if (result) {
			DMX_ERR("failed to copy index data (%d)\n", result);
			return result;
		}

		down_write(&stm_demux->rw_sem);

		if (mutex_lock_interruptible(&dmxdev_filter->mutex)) {
			up_write(&stm_demux->rw_sem);
			return -ERESTARTSYS;
		}

		result = stm_dvb_create_index_map(stm_demux,
					 dmxdev_filter, &idx_pid);

		mutex_unlock(&dmxdev_filter->mutex);
		up_write(&stm_demux->rw_sem);

		DMX_DBG("OUT<< setting index filter\n");

		break;
	}

	case DMX_SET_INS_FILTER:
	{
		struct dmx_insert_filter_params insert_filter;

		result = copy_from_user(&insert_filter,
			(const void __user *)arg, sizeof(insert_filter));
		if (result) {
			DMX_ERR("failed to copy data from user (%d)\n", result);
			return result;
		}

		DMX_DBG("Setting INSERT Filter\n");

		down_write(&stm_demux->rw_sem);

		if (mutex_lock_interruptible(&dmxdev_filter->mutex)) {
			up_write(&stm_demux->rw_sem);
			return -ERESTARTSYS;
		}

		result = stm_dvb_dmx_set_insert_filter(stm_demux,
					dmxdev_filter, &insert_filter);

		mutex_unlock(&dmxdev_filter->mutex);
		up_write(&stm_demux->rw_sem);
		break;
	}
	case DMX_SET_SCRAMBLING:
	{
		down_write(&stm_demux->rw_sem);

		result = stm_dvb_set_scrambling(file,
				stm_demux,
				(const dmx_scrambling_t *)arg);

		up_write(&stm_demux->rw_sem);

		break;
	}
	case DMX_SET_REP_FILTER:
	{
		struct dmx_replace_filter_params replace_filter;

		result = copy_from_user(&replace_filter,
				(const void __user *)arg, sizeof(replace_filter));
		if (result) {
			DMX_ERR("failed to copy data from user (%d)\n", result);
			return result;
		}

		DMX_DBG("Setting REPLACE Filter\n");
		down_write(&stm_demux->rw_sem);

		if (mutex_lock_interruptible(&dmxdev_filter->mutex)) {
			up_write(&stm_demux->rw_sem);
			return -ERESTARTSYS;
		}

		result = stm_dvb_dmx_set_replace_filter(stm_demux,
					dmxdev_filter, &replace_filter);

		mutex_unlock(&dmxdev_filter->mutex);
		up_write(&stm_demux->rw_sem);
		break;
	}

	case DMX_SET_PES_FILTER:
	{
		struct dmx_pes_filter_params pes_filter;

		result = copy_from_user(&pes_filter,
			   (const void __user *)arg, sizeof(pes_filter));
		if (result) {
			DMX_ERR("failed to copy pes para from user (%d)\n", result);
			return result;
		}

		down_write(&stm_demux->rw_sem);

		if (mutex_lock_interruptible(&dmxdev_filter->mutex)) {
			up_write(&stm_demux->rw_sem);
			return -ERESTARTSYS;
		}

		result = stm_dvb_dmx_set_pes_filter(dmxdev_filter, &pes_filter);

		mutex_unlock(&dmxdev_filter->mutex);
		up_write(&stm_demux->rw_sem);

		if (result) {
			DMX_ERR("failed. set_pes_filter (%d)\n", result);
			return result;
		}

		goto generic_dmx_ioctl;
	}

	case DMX_SET_FILTER:
	{
		struct dmx_sct_filter_params sec_filter;

		result = copy_from_user(&sec_filter,
			   (const void __user *)arg, sizeof(sec_filter));
		if (result) {
			DMX_ERR("failed to copy section para from user (%d)\n", result);
			return result;
		}

		down_write(&stm_demux->rw_sem);

		if (mutex_lock_interruptible(&dmxdev_filter->mutex)) {
			up_write(&stm_demux->rw_sem);
			return -ERESTARTSYS;
		}

		result = stm_dvb_dmx_set_sec_filter(dmxdev_filter, &sec_filter);

		mutex_unlock(&dmxdev_filter->mutex);
		up_write(&stm_demux->rw_sem);

		if (result) {
			DMX_ERR("failed. set_filter (%d)\n", result);
			return result;
		}

		goto generic_dmx_ioctl;
	}

	case DMX_FLUSH_CHANNEL:
	{
		struct stm_dvb_filter_chain_s *filter_chain;

		down_write(&stm_demux->rw_sem);

		if (mutex_lock_interruptible(&dmxdev_filter->mutex)) {
			up_write(&stm_demux->rw_sem);
			return -ERESTARTSYS;
		}

		filter_chain = match_chain_from_filter(stm_demux, dmxdev_filter);

		if (filter_chain) {
			result = stm_te_filter_set_control(filter_chain->output_filter->handle,
					STM_TE_OUTPUT_FILTER_CONTROL_FLUSH, 1);
			if (result) {
				DMX_ERR("failed. demux_flush (%d)\n", result);
				mutex_unlock(&dmxdev_filter->mutex);
				up_write(&stm_demux->rw_sem);
				return result;
			}
		}

		mutex_unlock(&dmxdev_filter->mutex);
		up_write(&stm_demux->rw_sem);

		break;
	}

	case DMX_START:
	{
		struct stm_dvb_filter_chain_s *filter_chain;

		down_write(&stm_demux->rw_sem);

		if (mutex_lock_interruptible(&dmxdev_filter->mutex)) {
			up_write(&stm_demux->rw_sem);
			return -ERESTARTSYS;
		}

		filter_chain = match_chain_from_filter(stm_demux, dmxdev_filter);

		if (filter_chain) {
			result = stm_dvb_chain_start(filter_chain);
			if (result) {
				mutex_unlock(&dmxdev_filter->mutex);
				up_write(&stm_demux->rw_sem);
				return result;
			}
		}

		mutex_unlock(&dmxdev_filter->mutex);
		up_write(&stm_demux->rw_sem);

		goto generic_dmx_ioctl;
	}

	case DMX_STOP:
	{
		struct stm_dvb_filter_chain_s *filter_chain;

		down_write(&stm_demux->rw_sem);

		if (mutex_lock_interruptible(&dmxdev_filter->mutex)) {
			up_write(&stm_demux->rw_sem);
			return -ERESTARTSYS;
		}

		filter_chain = match_chain_from_filter(stm_demux, dmxdev_filter);

		if (filter_chain) {
			result = stm_dvb_chain_stop(filter_chain);
			if (0 != result) {
				mutex_unlock(&dmxdev_filter->mutex);
				up_write(&stm_demux->rw_sem);
				return result;
			}
		}

		mutex_unlock(&dmxdev_filter->mutex);
		up_write(&stm_demux->rw_sem);

		goto generic_dmx_ioctl;
	}

	case DMX_SET_BUFFER_SIZE:
	{
		struct stm_dvb_filter_chain_s *chain;
		unsigned long size = arg;

		down_write(&stm_demux->rw_sem);

		if (mutex_lock_interruptible(&dmxdev_filter->mutex)) {
			up_write(&stm_demux->rw_sem);
			return -ERESTARTSYS;
		}

		chain = match_chain_from_filter(stm_demux, dmxdev_filter);
		if (!chain) {
			pr_err("%s(): create a filter before this control\n", __func__);
			mutex_unlock(&dmxdev_filter->mutex);
			up_write(&stm_demux->rw_sem);
			return -EINVAL;
		}

		result = stm_te_filter_set_control(chain->output_filter->handle,
						STM_TE_OUTPUT_FILTER_CONTROL_BUFFER_SIZE,
						size);
		if (result) {
			pr_err("%s(): failed to set buffer size with hw\n", __func__);
			mutex_unlock(&dmxdev_filter->mutex);
			up_write(&stm_demux->rw_sem);
			return result;
		}

		mutex_unlock(&dmxdev_filter->mutex);
		up_write(&stm_demux->rw_sem);

		goto generic_dmx_ioctl;
	}
	case DMX_GET_CTRL:
	{
		struct dmx_ctrl ctrl;

		/*
		 * Get the control data sent by user
		 */
		memset(&ctrl, 0, sizeof(ctrl));
		result = copy_from_user(&ctrl, (const void * __user)arg, sizeof(ctrl));
		if (result) {
			stv_err("failed to copy ctrl data\n");
			break;
		}

		down_write(&stm_demux->rw_sem);

		if (mutex_lock_interruptible(&dmxdev_filter->mutex)) {
			up_write(&stm_demux->rw_sem);
			return -ERESTARTSYS;
		}

		result = stm_dvb_demux_get_control(stm_demux, dmxdev_filter, &ctrl);

		if (!result) {
			result = copy_to_user((void __user *)arg, &ctrl, sizeof(ctrl));
			if (result)
				stv_err("failed to copy ctrl data to user\n");
		} else
			stv_err("failed to get control\n");

		mutex_unlock(&dmxdev_filter->mutex);
		up_write(&stm_demux->rw_sem);

		break;
	}

	default:
		goto generic_dmx_ioctl;
	}

	return result;

generic_dmx_ioctl:

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 35))
	return stm_demux->dmx_ops.ioctl(inode, file, cmd, arg);
#else
	return stm_demux->dmx_ops.unlocked_ioctl(file, cmd, arg);
#endif
}

static int stm_dvb_start_feed(struct dvb_demux_feed *dvbdmxfeed)
{
	return 0;
}

static int stm_dvb_stop_feed(struct dvb_demux_feed *dvbdmxfeed)
{
	return 0;
}

static struct file_operations dvr_file_ops = {

};

int stm_dvb_demux_setup_demux(struct stm_dvb_demux_s *stm_demux,
			      struct dvb_device *dev)
{
	struct file_operations stm_dvb_override_ops;

	stm_demux->dvb_demux.start_feed = stm_dvb_start_feed;
	stm_demux->dvb_demux.stop_feed = stm_dvb_stop_feed;

	INIT_LIST_HEAD(&stm_demux->pes_filter_handle_list);
	INIT_LIST_HEAD(&stm_demux->ts_filter_handle_list);
	INIT_LIST_HEAD(&stm_demux->sec_filter_handle_list);
	INIT_LIST_HEAD(&stm_demux->pid_filter_handle_list);
	INIT_LIST_HEAD(&stm_demux->pcr_filter_handle_list);
	INIT_LIST_HEAD(&stm_demux->index_filter_handle_list);

	init_rwsem(&stm_demux->rw_sem);
	stm_demux->filter_count = 0;
	stm_demux->demux_running = false;
	stm_demux->demux_object = NULL;
	stm_demux->users = 0;

	/* Set a set source method */
	stm_demux->dmxdev.demux->set_source = stm_dvb_set_source;
	stm_demux->demod_id = stm_demux->demux_id;

	stm_demux->dmxdev.demux->get_caps = stm_dvb_get_caps;
	/* Replace the demux read call here */
	memcpy(&stm_demux->dmx_ops,
	       stm_demux->dmxdev.dvbdev->fops, sizeof(struct file_operations));

	memcpy(&stm_dvb_override_ops,
	       stm_demux->dmxdev.dvbdev->fops, sizeof(struct file_operations));

	stm_dvb_override_ops.read = stm_dvb_dmx_read;
	stm_dvb_override_ops.poll = stm_dvb_dmx_poll;
	stm_dvb_override_ops.open = stm_dvb_dmx_open;
	stm_dvb_override_ops.release = stm_dvb_dmx_release;
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 35))
	stm_dvb_override_ops.ioctl = stm_dvb_dmx_ioctl;
#else
	stm_dvb_override_ops.unlocked_ioctl = stm_dvb_dmx_ioctl;
#endif

	memcpy((void *)stm_demux->dmxdev.dvbdev->fops,
	       &stm_dvb_override_ops, sizeof(struct file_operations));

	/* Replace the dvr calls, bit more complex as we need to add a
	 * writer to ther dvb_device else there is no permission to write */

	memcpy(&stm_demux->dvr_ops,
	       stm_demux->dmxdev.dvr_dvbdev->fops,
	       sizeof(struct file_operations));

	memcpy(&dvr_file_ops,
	       stm_demux->dmxdev.dvr_dvbdev->fops,
	       sizeof(struct file_operations));

	dvr_file_ops.owner = THIS_MODULE;
	dvr_file_ops.read = stm_dvb_dvr_read;
	dvr_file_ops.write = stm_dvb_dvr_write;
	dvr_file_ops.open = stm_dvb_dvr_open;
	dvr_file_ops.release = stm_dvb_dvr_release;
	dvr_file_ops.poll = stm_dvb_dvr_poll;
	dvr_file_ops.unlocked_ioctl = stm_dvb_dvr_ioctl;

	dev->fops = &dvr_file_ops;

#ifdef CONFIG_STLINUXTV_CRYPTO
	INIT_LIST_HEAD(&stm_demux->ca_pid_list);
#endif

	return 0;
}

/* Call holding lock */
int stm_dvb_demux_remove_demux(struct stm_dvb_demux_s *stm_demux)
{
	stm_dvb_internal_demux_delete(stm_demux);

	return 0;
}
