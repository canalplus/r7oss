/************************************************************************
Copyright (C) 2012 STMicroelectronics. All Rights Reserved.

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

Source file name : ca_transform.c

Implementation of LinuxDVB CA using STKPI stm_ce driver

************************************************************************/
#include <dvb/dvb_adaptation.h>
#include <stm_ce.h>
#include <stm_ce_key_rules.h>
#include <stm_ce_fuse.h>
#include <demux/demux_filter.h>
#include <dvb/dvb_adapt_ca.h>
#include "linux/dvb/stm_ca.h"

#define stv_err pr_err

/* We use a 2-stage stm_ce stream transform for each CA channel/index. For
 * each transform stage 0 is always used for the DSC cipher and stage 1 for
 * the SCR cipher */
#define NB_TRANSFORM_STAGES 2
#define STAGE_DSC 0
#define STAGE_SCR 1

/* Hard-coded number of slots/descramblers to report via LinuxDVB interface
 * until stm_ce provides a mechanism for querying available resources */
#define STM_CA_MAX_SLOT 1
#define STM_CA_MAX_DESCR 256

static struct stm_dvb_ca_channel_s *get_channel_by_index(struct stm_dvb_ca_s
							 *stm_ca,
							 unsigned int index)
{
	struct list_head *ptr;
	struct stm_dvb_ca_channel_s *channel;
	list_for_each(ptr, &stm_ca->channel_list) {
		channel = list_entry(ptr, struct stm_dvb_ca_channel_s, list);
		if (channel->index == index)
			return channel;
	}
	return NULL;
}

/**
 * Converts LDVB polarity value to stm_ce polarity
 */
static inline uint32_t get_ce_key_polarity(unsigned int ldvb_polarity)
{
	switch (ldvb_polarity) {
	case DVB_CA_POLARITY_EVEN:
		return STM_CE_KEY_POLARITY_EVEN;
	case DVB_CA_POLARITY_ODD:
		return STM_CE_KEY_POLARITY_ODD;
	case DVB_CA_POLARITY_NONE:
		return STM_CE_KEY_POLARITY_NONE;
	default:
		return ldvb_polarity;
	}
}

/**
 * Converts LDVB cipher value to stm_ce cipher
 */
static inline uint32_t get_ce_cipher(unsigned int ldvb_cipher)
{
	switch (ldvb_cipher) {
	case DVB_CA_CIPHER_NONE:
		return STM_CE_CIPHER_NONE;
	case DVB_CA_CIPHER_DVB_CSA:
		return STM_CE_CIPHER_DVB_CSA2;
	case DVB_CA_CIPHER_DVB_CSA3:
		return STM_CE_CIPHER_DVB_CSA3;
	case DVB_CA_CIPHER_AES:
		return STM_CE_CIPHER_AES;
	case DVB_CA_CIPHER_TDES:
		return STM_CE_CIPHER_TDES;
	case DVB_CA_CIPHER_MULTI2:
		return STM_CE_CIPHER_MULTI2;
	case DVB_CA_CIPHER_RESERVED0:
		return STM_CE_CIPHER_RESERVED0;
	default:
		return STM_CE_CIPHER_NONE;
	}
}

/**
 * Converts LDVB chaining value to stm_ce chaining
 */
static inline uint32_t get_ce_chaining(unsigned int ldvb_chaining)
{
	switch (ldvb_chaining) {
	case DVB_CA_CHAINING_NONE:
		return STM_CE_CHAINING_NONE;
	case DVB_CA_CHAINING_CBC:
		return STM_CE_CHAINING_CBC;
	case DVB_CA_CHAINING_CTR:
		return STM_CE_CHAINING_CTR;
	case DVB_CA_CHAINING_OFB:
		return STM_CE_CHAINING_OFB;
	case DVB_CA_CHAINING_RCBC:
		return STM_CE_CHAINING_RCBC;
	case DVB_CA_CHAINING_DVB_LSA_CBC:
		return STM_CE_CHAINING_DVB_LSA_CBC;
	case DVB_CA_CHAINING_DVB_LSA_RCBC:
		return STM_CE_CHAINING_DVB_LSA_RCBC;
	default:
		return STM_CE_CHAINING_NONE;
	}
}

/**
 * Converts LDVB residue value to stm_ce residue
 */
static inline uint32_t get_ce_residue(unsigned int ldvb_residue)
{
	switch (ldvb_residue) {
	case DVB_CA_RESIDUE_NONE:
		return STM_CE_RESIDUE_NONE;
	case DVB_CA_RESIDUE_DVS042:
		return STM_CE_RESIDUE_DVS042;
	case DVB_CA_RESIDUE_CTS:
		return STM_CE_RESIDUE_CTS;
	case DVB_CA_RESIDUE_SA_CTS:
		return STM_CE_RESIDUE_SA_CTS;
	case DVB_CA_RESIDUE_IPTV:
		return STM_CE_RESIDUE_IPTV;
	case DVB_CA_RESIDUE_PLAIN_LR:
		return STM_CE_RESIDUE_PLAIN_LR;
	default:
		return STM_CE_RESIDUE_NONE;
	}
}

/**
 * Creates an stm_ce session object for the given ca device if none exists
 *
 * This function is called before any CA command to ensure that a session
 * object exists. This prevents stm_ce session objects being created for each
 * LDVB CA device, even if the device is not in use
 */
static inline int start_session(struct stm_dvb_ca_s *stm_ca)
{
	int err = 0;
	if (!stm_ca->session) {
		err = stm_ce_session_new(stm_ca->profile, &stm_ca->session);
		if (err)
			printk(KERN_ERR "ca%d: Failed to create stm_ce session "
			       "for profile %s (%d)\n",
			       stm_ca->ca_id, stm_ca->profile, err);
	}
	return err;
}

/**
 * Gets a ca channel object from its index.
 *
 * Channel objects are used to hold data for a single LDVB CA index. Each
 * channel owns a single stm_ce transform object
 * If no ca channel exists at this index a new channel object is created
 */
static int ca_get_channel(struct stm_dvb_ca_s *stm_ca, unsigned int index,
			  struct stm_dvb_ca_channel_s **channel,
			  stm_ce_transform_type_t transform_type)
{
	int err;
	struct stm_dvb_ca_channel_s *c = NULL;

	/* Try to get an already existing channel */
	c = get_channel_by_index(stm_ca, index);
	if (c) {
		*channel = c;
		return 0;
	}

	/* Start stm_ce session if not already started */
	err = start_session(stm_ca);
	if (err)
		return err;

	c = kzalloc(sizeof(struct stm_dvb_ca_channel_s), GFP_KERNEL);
	if (!c)
		return -ENOMEM;

	INIT_LIST_HEAD(&c->pid_list);

	err = stm_ce_transform_new((stm_ce_handle_t) stm_ca->session,
			   transform_type,
			   (transform_type == STM_CE_TRANSFORM_TYPE_STREAM)?
			   NB_TRANSFORM_STAGES:1,
			   &c->transform);
	if (err) {
		printk(KERN_ERR "ca%d: transform_new return %d\n",
				stm_ca->ca_id, err);
		goto error;
	}

	c->index = index;
	list_add(&c->list, &stm_ca->channel_list);
	*channel = c;
	return 0;

error:
	kfree(c);
	return err;
}

static int attach_descr_memory_transform(struct stm_dvb_ca_channel_s *channel,
					 struct stm_dvb_demux_s *demux)
{
	stm_ce_handle_t transform;
	int err;

	/* We should only attach the transform to the demux if the source
	 if a memsrc already */
	if (!demux->memsrc_input)
		return 0;

	demux->ca_channel = channel;
	transform = channel->transform;

	err = stm_memsrc_detach(demux->memsrc_input);
	if (err) {
		printk(KERN_ERR "%s: failed to detach memsrc from demux (%d)\n",
					__func__, err);
		return err;
	}

	err = stm_memsrc_attach(demux->memsrc_input,
				transform, STM_DATA_INTERFACE_PUSH);
	if (err) {
		printk(KERN_ERR "%s: failed to attach memsrc to ca (%d)\n",
					__func__, err);
		return err;
	}

	err = stm_ce_transform_attach(transform, demux->demux_object);
	if (err) {
		printk(KERN_ERR "%s: failed to attach ca to demux (%d)\n",
					__func__, err);
		return err;
	}

	return 0;
}

static int detach_descr_memory_transform(struct stm_dvb_demux_s *demux)
{
	stm_ce_handle_t transform;
	int err;

	/* Verify there is actually a ca_channel for the demux */
	if (!demux->ca_channel)
		return 0;

	/* If there is a memsrc_input, then we are attached in the middle */
	if (!demux->memsrc_input)
		return 0;

	transform = demux->ca_channel->transform;

	err = stm_memsrc_detach(demux->memsrc_input);
	if (err) {
		printk(KERN_ERR "%s: failed to detach memsrc from ca (%d)\n",
					__func__, err);
		return err;
	}

	err = stm_ce_transform_detach(transform, demux->demux_object);
	if (err) {
		printk(KERN_ERR "%s: failed to detach ca from demux (%d)\n",
					__func__, err);
		return err;
	}

	err = stm_memsrc_attach(demux->memsrc_input,
				demux->demux_object, STM_DATA_INTERFACE_PUSH);
	if (err) {
		printk(KERN_ERR "%s: failed to attach memsrc to demux (%d)\n",
					__func__, err);
		return err;
	}

	demux->ca_channel = NULL;

	return 0;
}

static int attach_scr_memory_transform(struct stm_dvb_ca_channel_s *channel,
				       struct stm_dvb_demux_s *demux)
{
	int err;
	struct stm_dvb_filter_chain_s *chain;
	struct stm_dvb_filter_s *filter;
	stm_ce_handle_t transform;

	/* First need to get the demux TS output filter */
	chain = get_primary_ts_chain(demux);
	if (!chain) {
		printk(KERN_ERR "Failed to find a valid demux TS output\n");
		return -EINVAL;
	}

	/* Detach the demux from the memsink */
	filter = chain->output_filter;

	/* Get the transform assigned for this output filter */
	filter->ca_channel = channel;
	transform = filter->ca_channel->transform;

	err = stm_te_filter_detach(filter->handle,
			   (void *)filter->memsink_interface);
	if (err) {
		printk(KERN_ERR "Failed to detach ts output (%d)\n", err);
		return err;
	}

	/* Attach the memsink to transformer to the memsink */
	err = stm_ce_transform_attach(transform,
				      filter->memsink_interface);
	if (err) {
		printk(KERN_ERR "Failed to attach the transform (%d)\n", err);
		return err;
	}

	/* Attach the TS filter to the transform */
	err = stm_te_filter_attach(filter->handle, transform);
	if (err) {
		printk(KERN_ERR "Failed to attach the TS filter (%d)\n", err);
		return err;
	}

	return 0;
}

static int detach_scr_memory_transform(struct stm_dvb_demux_s *demux)
{
	int err;
	struct stm_dvb_filter_chain_s *chain;
	struct stm_dvb_filter_s *filter;
	stm_ce_handle_t transform;

	/* First need to get the demux TS output filter */
	chain = get_primary_ts_chain(demux);
	if (!chain) {
		printk(KERN_ERR "Failed to find a valid demux TS output\n");
		return -EINVAL;
	}

	/* Detach the demux from the memsink */
	filter = chain->output_filter;

	/* Verify there is a ca_channel to detach */
	if (!filter->ca_channel)
		return 0;

	/* Get the transform assigned for this output filter */
	transform = filter->ca_channel->transform;

	err = stm_ce_transform_detach(transform,
				      filter->memsink_interface);
	if (err) {
		printk(KERN_ERR "Failed to detach ca output (%d)\n", err);
		return err;
	}

	/* Attach the memsink to transformer to the memsink */
	err = stm_te_filter_detach(filter->handle, transform);
	if (err) {
		printk(KERN_ERR "Failed to detach the transform (%d)\n", err);
		return err;
	}

	/* Attach the TS filter to the memsink */
	err = stm_te_filter_attach(filter->handle,
				   (void *)filter->memsink_interface);
	if (err) {
		printk(KERN_ERR "Failed to attach the TS filter (%d)\n", err);
		return err;
	}

	filter->ca_channel = NULL;

	return 0;
}

/**
 * Deletes a CA channel object
 */
static int ca_del_channel(struct stm_dvb_ca_s *stm_ca,
			  struct stm_dvb_ca_channel_s *c)
{
	int err;
	struct stm_dvb_filter_chain_s *chain;
	struct stm_dvb_filter_s *filter;

	switch(c->mode & CA_MODE_MASK){
	case CA_MODE_PACKET:
		/* If the channel is a stream channel, we must detach the
		 * ca_channel from all pid_filter it might be attached */
		list_for_each_entry(chain,
				    &stm_ca->demux->pid_filter_handle_list,
				    input_list) {
			filter = chain->input_filter;
			if (filter->ca_channel == c)
				filter_set_ca_channel(filter, NULL);
		}
		break;
	case CA_MODE_BLOCK:
		/* If the channel is a memory channel, we must check and
		 * remove it from the demux / memsrc / memsink objects */
		if ((c->mode & CA_MODE_DIR_MASK) == CA_MODE_DESCR)
			detach_descr_memory_transform(stm_ca->demux);
		else
			detach_scr_memory_transform(stm_ca->demux);
		break;
	default:
		/* Nothing to do */
		break;
	}

	err = stm_ce_transform_delete(c->transform);

	list_del(&c->list);
	kfree(c);
	return err;
}

/**
 * Sets the profile for a CA device.
 *
 * This operation can only be performed before the stm_ce for that CA device
 * has been created
 */
static int ca_set_profile(struct stm_dvb_ca_s *stm_ca, const char *profile)
{
	/* If the session object has already been created for this ca device,
	 * we cannot change the profile. */
	if (stm_ca->session) {
		printk(KERN_ERR "ca%d: Unable to change profile, device "
		       "busy\n", stm_ca->ca_id);
		return -EBUSY;
	}

	if (!stm_ca->profile)
		stm_ca->profile = kzalloc(DVB_CA_PROFILE_SIZE, GFP_KERNEL);
	if (!stm_ca->profile)
		return -ENOMEM;
	strncpy(stm_ca->profile, profile, DVB_CA_PROFILE_SIZE);

	return start_session(stm_ca);
}

/**
 * Resets a CA device.
 *
 * This deletes all channels under this CA device and any stm_ce session
 * object associated with the device
 */
static int ca_reset(struct stm_dvb_ca_s *stm_ca)
{
	struct list_head *ptr;
	struct list_head *ptr2;
	int err;

	/* Delete all ca channels for this CA device */
	list_for_each_safe(ptr, ptr2, &stm_ca->channel_list) {
		err =
		    ca_del_channel(stm_ca, list_entry
				   (ptr, struct stm_dvb_ca_channel_s, list));
		if (err)
			return err;
	}

	/* TODO: reset all rule/ladder data before session deletion using
	 * stm_ce_session_set_rule_data */
	if (stm_ca->session) {
		err = stm_ce_session_delete((stm_ce_handle_t) stm_ca->session);
		if (err)
			printk(KERN_ERR "Unable to delete stm_ce session "
			       "(%d)\n", err);
	}
	if (!err)
		stm_ca->session = NULL;
	return err;
}

/**
 * Sets key ladder data for a given ladder on a CA device
 *
 * Ladder data normally consists of an array of encrypted keys. The format and
 * number of keys is specific to the ladder, identified by rule_id
 */
static int ca_set_ladder(struct stm_dvb_ca_s *stm_ca, unsigned int rule_id,
			 dvb_ca_key_t * keys, unsigned char n_keys)
{
	int err;
	int i;
	stm_ce_buffer_t buf[DVB_CA_MAX_LADDER_KEYS];

	if (n_keys > DVB_CA_MAX_LADDER_KEYS)
		return -EINVAL;

	/* Start session if not already started */
	err = start_session(stm_ca);
	if (err)
		return err;

	/* Create array of stm_ce_buffer structs to point at key data */
	for (i = 0; i < n_keys; i++) {
		buf[i].data = keys[i].data;
		buf[i].size = keys[i].size;
	}

	return stm_ce_session_set_rule_data(stm_ca->session, rule_id, buf);
}

/**
 * Sets a broadcast or local scrambling key for a given CA channel
 *
 * The channel must have previously been configured with a valid cipher
 *
 * The stage parameter is either STAGE_DSC for descrambling keys or STAGE_SCR
 * for scrambling keys
 */
static int ca_set_key(struct stm_dvb_ca_s *stm_ca, unsigned int index,
		      unsigned int stage, dvb_ca_key_t * key)
{
	struct stm_dvb_ca_channel_s *channel;
	stm_ce_buffer_t ce_key;

	channel = get_channel_by_index(stm_ca, index);

	if (NULL == channel)
		return -EINVAL;

	ce_key.data = key->data;
	ce_key.size = (size_t) key->size;

	/* stage only makes sense in case of stream channel - otherwise
	   it's always zero since we create transform with only 1 stage
	   for memory channel */
	if ((channel->mode & CA_MODE_MASK) == CA_MODE_BLOCK)
		stage = 0;

	return stm_ce_transform_set_key(channel->transform, stage, &ce_key,
					get_ce_key_polarity(key->polarity));
}

/**
 * Sets a broadcast or local scrambling IV for a given CA channel
 *
 * The channel must have previously been configured with a valid chaining-mode
 * cipher.
 *
 * The stage parameter is either STAGE_DSC for descrambling IVs or STAGE_SCR
 * for scrambling IVs
 */
static int ca_set_iv(struct stm_dvb_ca_s *stm_ca, unsigned int index,
		     unsigned int stage, dvb_ca_key_t * iv)
{
	struct stm_dvb_ca_channel_s *channel;
	stm_ce_buffer_t ce_iv;

	channel = get_channel_by_index(stm_ca, index);
	if (NULL == channel)
		return -EINVAL;

	ce_iv.data = iv->data;
	ce_iv.size = (size_t) iv->size;

	return stm_ce_transform_set_iv(channel->transform, stage, &ce_iv,
				       get_ce_key_polarity(iv->polarity));
}

/**
 * Sets the cipher configuration for a CA channel
 */
static int ca_set_stream_config(struct stm_dvb_ca_s *stm_ca,
				unsigned int index,
				unsigned int stage,
				const dvb_ca_config_t * config)
{
	int err;
	struct stm_dvb_ca_channel_s *channel;
	stm_ce_transform_config_t ce_config = {0};

	err =
	    ca_get_channel(stm_ca, index, &channel,
			   STM_CE_TRANSFORM_TYPE_STREAM);
	if (err)
		return err;

	/* Translate LDVB config into stm_ce api values */
	ce_config.key_rule_id = config->key_ladder_id;
	ce_config.iv_rule_id = config->iv_ladder_id;
	ce_config.cipher = get_ce_cipher(config->cipher);
	ce_config.chaining = get_ce_chaining(config->chaining);
	ce_config.residue = get_ce_residue(config->residue);
	ce_config.msc = config->msc;

	/* Cipher direction is determined by the stage */
	channel->mode = CA_MODE_PACKET;
	if (stage == STAGE_SCR)
		ce_config.direction = STM_CE_DIRECTION_ENCRYPT;
	else
		ce_config.direction = STM_CE_DIRECTION_DECRYPT;

	return stm_ce_transform_set_config(channel->transform, stage,
					   &ce_config);
}

static int ca_set_memory_config(struct stm_dvb_ca_s *stm_ca,
				unsigned int index,
				unsigned int stage,
				const dvb_ca_config_t * config)
{
	int err;
	struct stm_dvb_ca_channel_s *channel;
	stm_ce_transform_config_t ce_config = {0};
	stm_ce_handle_t transform;

	/* creates memory transform if not exists */
	err = ca_get_channel(stm_ca, index, &channel,
			   STM_CE_TRANSFORM_TYPE_MEMORY);
	if (err)
		return err;

	transform = channel->transform;
	channel->mode = CA_MODE_BLOCK;

	/* Translate LDVB config into stm_ce api values */
	ce_config.key_rule_id = config->key_ladder_id;
	ce_config.iv_rule_id = config->iv_ladder_id;
	ce_config.cipher = get_ce_cipher(config->cipher);
	ce_config.chaining = get_ce_chaining(config->chaining);
	ce_config.residue = get_ce_residue(config->residue);
	ce_config.msc = config->msc;
	ce_config.packet_size = config->dma_transfer_packet_size;

	/* Cipher direction is determined by the stage */
	if (stage == STAGE_SCR)
		ce_config.direction = STM_CE_DIRECTION_ENCRYPT;
	else
		ce_config.direction = STM_CE_DIRECTION_DECRYPT;

	err = stm_ce_transform_set_config(transform, 0, &ce_config);
	if (err)
		return err;

	if (stage == STAGE_DSC)
		err = attach_descr_memory_transform(channel, stm_ca->demux);
	else
		err = attach_scr_memory_transform(channel, stm_ca->demux);

	return err;
}

static int ca_get_info(struct stm_dvb_ca_s *stm_ca, unsigned int index,
		dvb_ca_info_t *info)
{
	struct stm_dvb_ca_channel_s *channel;

	channel = get_channel_by_index(stm_ca, index);
	if (!channel) {
		printk(KERN_ERR "Bad CA index: %d\n", index);
		return -EINVAL;
	}

	/* TODO: Currently there is no way to differentiate whether a RAW or
	 * DSC transform is applied */
	info->ce_dsc_transform_hdl = channel->transform;
	info->ce_raw_dsc_transform_hdl = channel->transform;
	info->ce_raw_scr_transform_hdl = channel->transform;

	info->ce_session_hdl = stm_ca->session;

	return 0;
}

/**
 * filter_set_ca_channel- Sets the CA channel used by a demux PID filter
 *
 * This attaches the underlying TE pid filter to the underlying CE transform
 * object.
 * Detaches the filter if it already is attached to a transform, before
 * attaching to the new transform.
 *
 * If channel is NULL only perform the detachment
 */
int filter_set_ca_channel(struct stm_dvb_filter_s *filter,
			  struct stm_dvb_ca_channel_s *channel)
{
	int err = 0;

	if (filter->ca_channel) {
		err = stm_te_filter_detach(filter->handle,
					   filter->ca_channel->transform);
		if (err) {
			printk(KERN_ERR "Failed to detach filter from "
			       "transform (%d)\n", err);
			return err;
		}
		filter->ca_channel = NULL;
	}

	if (!channel)
		return 0;

	err = stm_te_filter_attach(filter->handle, channel->transform);
	if (err) {
		printk(KERN_ERR "Failed to attach filter to "
		       "transform (%d)\n", err);
		return err;
	}

	filter->ca_channel = channel;

	return 0;
}

/**
 * Prepare Fuse operation
 *
 */
int ca_fuse_op(struct stm_dvb_ca_s *stm_ca, unsigned int index,
		dvb_ca_fuse_t *pFuse_dvb_op)
{
	int err = 0, err2 = 0;
	struct stm_ce_fuse_op *pFuse_ce_op = NULL;

	pFuse_ce_op = kzalloc(sizeof(*pFuse_ce_op), GFP_KERNEL);

	if (!pFuse_ce_op)
		return -ENOMEM;

	/*start session if not already started */
	err = start_session(stm_ca);
	if (err) {
		kfree(pFuse_ce_op);
		return err;
	}

	/* copy data from dvb_ca_fuse_t to struct stm_ce_fuse_op */
	/* copy fuse name string */
	strlcpy(pFuse_ce_op->fuse_name, pFuse_dvb_op->name,
			min(CE_FUSE_NAME_MAX_SIZE, DVB_CA_MAX_FUSE_NAME));
	/* copy operation (translate DVB_CA operation id to CE operation id
	 * and if needed copy data */
	switch (pFuse_dvb_op->op) {
	case DVB_CA_FUSE_READ:
		pFuse_ce_op->command = FC_READ;
		break;
	case DVB_CA_FUSE_WRITE:
		pFuse_ce_op->command = FC_WRITE;
		memcpy(pFuse_ce_op->fuse_value, pFuse_dvb_op->value,
				min_t(size_t, sizeof(pFuse_ce_op->fuse_value),
					sizeof(pFuse_dvb_op->value)));
		break;
	case DVB_CA_FUSE_WRITE_AND_LOCK:
		pFuse_ce_op->command = FC_WRITE_AND_LOCK;
		memcpy(pFuse_ce_op->fuse_value, pFuse_dvb_op->value,
				min_t(size_t, sizeof(pFuse_ce_op->fuse_value),
					sizeof(pFuse_dvb_op->value)));
		break;
	case DVB_CA_FUSE_LOCK:
		pFuse_ce_op->command = FC_LOCK;
		/* memset(pFuse_ce_op->fuse_value,
		 *            sizeof(pFuse_ce_op->fuse_value), 0);
		 * not needed kzallocated */
		break;
	default:
		printk(KERN_ERR "dvb.fuse_op=%d: unknown op command\n",
		       pFuse_dvb_op->op);
		err = -EOPNOTSUPP;
		goto error;
	}

	/* call STKPI */
	err = stm_ce_session_set_compound_control(stm_ca->session,
			STM_CE_SESSION_CMPD_CTRL_FUSE_OP, pFuse_ce_op);
	if (err)
		printk(KERN_ERR "Error stm_ce_session_set_compound_control %d\n",
				err);

	/* in case of READ, copy back data */
	if (pFuse_dvb_op->op == DVB_CA_FUSE_READ)
		memcpy(pFuse_dvb_op->value,
			pFuse_ce_op->fuse_value,
			min_t(size_t, sizeof(pFuse_ce_op->fuse_value),
				sizeof(pFuse_dvb_op->value)));

error:
	/* delete session */
	err2 = stm_ce_session_delete((stm_ce_handle_t) stm_ca->session);
	if (err2)
		printk(KERN_ERR "Unable to delete stm_ce session "
				"(%d)\n", err2);
	else
		stm_ca->session = NULL;

	kfree(pFuse_ce_op);
	return err;
}

int ca_fuse_write(struct stm_dvb_ca_s *stm_ca, unsigned int index,
		dvb_ca_fuse_t *pFuse_dvb_op)
{
	if ((pFuse_dvb_op->op != DVB_CA_FUSE_WRITE_AND_LOCK) &&
	    (pFuse_dvb_op->op != DVB_CA_FUSE_WRITE) &&
	    (pFuse_dvb_op->op != DVB_CA_FUSE_LOCK))
		return -EINVAL;

	return ca_fuse_op(stm_ca, index, pFuse_dvb_op);
}

int ca_fuse_read(struct stm_dvb_ca_s *stm_ca, unsigned int index,
		dvb_ca_fuse_t *pFuse_dvb_op)
{
	if (pFuse_dvb_op->op != DVB_CA_FUSE_READ)
		return -EINVAL;

	return ca_fuse_op(stm_ca, index, pFuse_dvb_op);
}

/**
 * Handles to LDVB CA_SEND_MSG ioctl.
 *
 * This ioctl is used to perform ST-specific CA device configuration
 */
int stm_dvb_ca_send_msg(struct stm_dvb_ca_s *stm_ca, dvb_ca_msg_t * msg)
{
	int err;

	/* CA message must have CA_STM type */
	if (msg->type != CA_STM) {
		printk(KERN_ERR "ca%d: Received message with wrong type: "
		       "0x%x\n", stm_ca->ca_id, msg->type);
		return -EINVAL;
	}

	switch (msg->command) {
	case DVB_CA_SET_PROFILE:
		err = ca_set_profile(stm_ca, msg->u.profile);
		break;
	case DVB_CA_RESET:
		err = ca_reset(stm_ca);
		break;
	case DVB_CA_SET_LADDER_DATA:
		err = ca_set_ladder(stm_ca, msg->u.ladder.id,
				    msg->u.ladder.keys, msg->u.ladder.n_keys);
		break;
	case DVB_CA_SET_DSC_CONFIG:
		err = ca_set_stream_config(stm_ca, msg->index,
					   STAGE_DSC, &msg->u.config);
		break;
	case DVB_CA_SET_SCR_CONFIG:
		err = ca_set_stream_config(stm_ca, msg->index,
					   STAGE_SCR, &msg->u.config);
		break;
	case DVB_CA_SET_RAW_DSC_CONFIG:
		err = ca_set_memory_config(stm_ca, msg->index,
					   STAGE_DSC, &msg->u.config);
		break;
	case DVB_CA_SET_RAW_SCR_CONFIG:
		err = ca_set_memory_config(stm_ca, msg->index,
					   STAGE_SCR, &msg->u.config);
		break;
	case DVB_CA_SET_DSC_KEY:
		err = ca_set_key(stm_ca, msg->index, STAGE_DSC, &msg->u.key);
		break;
	case DVB_CA_SET_SCR_KEY:
		err = ca_set_key(stm_ca, msg->index, STAGE_SCR, &msg->u.key);
		break;
	case DVB_CA_SET_DSC_IV:
		err = ca_set_iv(stm_ca, msg->index, STAGE_DSC, &msg->u.key);
		break;
	case DVB_CA_SET_SCR_IV:
		err = ca_set_iv(stm_ca, msg->index, STAGE_SCR, &msg->u.key);
		break;
	case DVB_CA_FUSE_OP:
		err = ca_fuse_write(stm_ca, msg->index, &msg->u.fuse);
		break;
	default:
		printk(KERN_ERR "ca%d: unknown stm command %u\n",
		       stm_ca->ca_id, msg->command);
		err = -EOPNOTSUPP;
	}

	return err;
}

/**
 * Handles to LDVB CA_GET_MSG ioctl.
 *
 * This ioctl is used to read ST-specific CA device configuration
 */
int stm_dvb_ca_get_msg(struct stm_dvb_ca_s *stm_ca, dvb_ca_msg_t *msg)
{
	int err;

	/* CA message must have CA_STM type */
	if (msg->type != CA_STM) {
		printk(KERN_ERR "ca%d: Received message with wrong type: 0x%x\n",
				stm_ca->ca_id, msg->type);
		return -EINVAL;
	}

	switch (msg->command) {
	case DVB_CA_GET_INFO:
		err = ca_get_info(stm_ca, msg->index, &msg->u.info);
		break;
	case DVB_CA_FUSE_OP:
		err = ca_fuse_read(stm_ca, msg->index, &msg->u.fuse);
		break;

	default:
		printk(KERN_ERR "ca%d: unknown stm command %u\n",
		       stm_ca->ca_id, msg->command);
		err = -EOPNOTSUPP;
	}

	return err;
}

/**
 * stm_dvb_ca_set_descr
 *
 * Configures the ca channel associated with the index "ca_descr->index".
 * If no channel already exists for this index, creates a new channel
 *
 * This function is to support the legacy LDVB-CA SET_DESCR ioctl, which only
 * supports DVBCSA algorithm with 8-byte clear keys
 */
int stm_dvb_ca_set_descr(struct stm_dvb_ca_s *stm_ca, ca_descr_t * ca_descr)
{
	int err;

	/* Set CA channel to DVBCSA2 descrambler configuration */
	dvb_ca_key_t csa2_key;
	static const dvb_ca_config_t csa2_config = {
		.cipher = DVB_CA_CIPHER_DVB_CSA,
		.chaining = DVB_CA_CHAINING_NONE,
		.residue = DVB_CA_RESIDUE_NONE,
		.key_ladder_id = RULE_CLEAR_CW8,
		.iv_ladder_id = STM_CE_KEY_RULES_NONE,
		.msc = DVB_CA_MSC_AUTO,
	};
	struct stm_dvb_ca_channel_s *c;

	/* If the channel exists assume config set, so setting new key only. */
	c = get_channel_by_index(stm_ca, ca_descr->index);
	if (!c) {
		err = ca_set_stream_config(stm_ca, ca_descr->index,
					   STAGE_DSC, &csa2_config);
		if (err)
			return err;
	}

	/* Set CA channel key with the provided 8-byte key */
	memcpy(csa2_key.data, ca_descr->cw, 8);
	csa2_key.size = 8;
	csa2_key.polarity = ca_descr->parity;
	err = ca_set_key(stm_ca, ca_descr->index, STAGE_DSC, &csa2_key);

	return err;
}

/**
 * Associates a given CA channel, specified by ca_pid->index with a given PID
 * on the corresponding demux.
 *
 * This results in an attachment between the stm_te pid filter and the CA
 * channel's stm_ce transform object
 */
int stm_dvb_ca_set_filter_channel(struct stm_dvb_ca_s *stm_ca, ca_pid_t * ca_pid,
						struct stm_dvb_filter_s *filter)
{
	int err = 0;
	struct stm_dvb_ca_channel_s *channel = NULL;
	/* Get the new channel (if required; -1 indicates disable scrambling
	 * for this PID) */
	if (-1 != ca_pid->index) {
		channel = get_channel_by_index(stm_ca, ca_pid->index);
		if (!channel) {
			printk(KERN_ERR "ca%d: Cannot use uninitialized "
			       "index %d\n", stm_ca->ca_id, ca_pid->index);
			err = -EINVAL;
			goto error;
		}
	}

	/* If this PID is already associated with the same index do nothing. */
	if (filter->ca_channel && filter->ca_channel->index == ca_pid->index)
		goto error;

	/* Associate the new channel with this pid (will disassociate any
	 * previous channel. If channel is NULL this will disable scrambling
	 * for this PID */
	err = filter_set_ca_channel(filter, channel);

error:
	return err;
}
/**
 * Gets the PID filter and sets the given CA channel.
 */
int stm_dvb_ca_set_pid(struct stm_dvb_ca_s *stm_ca, ca_pid_t * ca_pid)
{
	int err = 0;
	struct stm_dvb_filter_s *filter = NULL;

	down_write(&stm_ca->demux->rw_sem);

	/*
	 * Find the pid filter for this pid
	 */
	filter = get_pid_filter(stm_ca->demux, ca_pid->pid);
	if (!filter) {
		err = stm_dvb_demux_add_ca_pid(stm_ca, ca_pid);
		if (err)
			stv_err("unable to add ca pid to demux\n");
		goto done;
	}

	err = stm_dvb_ca_set_filter_channel(stm_ca, ca_pid, filter);

done:
	up_write(&stm_ca->demux->rw_sem);
	return err;
}

/**
 * Implements the LinuxDVB CA_RESET command for an stm ca device
 */
int stm_dvb_ca_reset(struct stm_dvb_ca_s *stm_ca)
{
	return ca_reset(stm_ca);
}

/**
 * Implements the LinuxDVB CA_GET_CAP command for an stm ca device
 */
int stm_dvb_ca_get_cap(struct stm_dvb_ca_s *stm_ca, ca_caps_t *caps)
{
	memset(caps, 0, sizeof(*caps));

	/* TODO: When stm_ce supports querying of maximum number of transforms
	 * we can supply useful information here */
	caps->slot_num = STM_CA_MAX_SLOT;
	caps->slot_type = CA_DESCR;
	caps->descr_num = STM_CA_MAX_DESCR;
	caps->descr_type = CA_STM;

	return 0;
}

/**
 * Implements the LinuxDVB CA_GET_SLOT_INFO command for an stm ca device
 */
int stm_dvb_ca_get_slot_info(struct stm_dvb_ca_s *stm_ca, ca_slot_info_t *info)
{
	if (info->num < 0 || info->num > STM_CA_MAX_SLOT)
		return -EINVAL;

	info->type = CA_DESCR;
	info->flags = 0;
	return 0;
}

/**
 * Implements the LinuxDVB CA_GET_DESCR_INFO command for an stm ca device
 */
int stm_dvb_ca_get_descr_info(struct stm_dvb_ca_s *stm_ca,
		ca_descr_info_t *info)
{
	if (info->num > STM_CA_MAX_DESCR)
		return -EINVAL;

	info->type = CA_STM;
	return 0;
}
