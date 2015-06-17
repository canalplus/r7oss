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

Source file name : demux_filter.c

Filters/chains handling functions

Date        Modification                                    Name
----        ------------                                    --------

 ************************************************************************/

#include <dvb/dvb_adaptation.h>
#include "demux_filter.h"
#include "demux_common.h"
#ifdef CONFIG_STLINUXTV_CRYPTO
#include <dvb/dvb_adapt_ca.h>
#endif

#define stv_err pr_err

/**
 * stm_dmx_subscription_create()
 * This function creates the subscription for transport engine events
 * which can be received on the output filter. These events can be
 * used for reading data and poll
 */
static int stm_dmx_subscription_create(struct stm_dvb_filter_s *filter)
{
	stm_event_subscription_h evt_subs;
	stm_event_subscription_entry_t *evt_entry = NULL;
	int ret = 0;

	if (!filter || !filter->memsink_interface) {
		printk(KERN_ERR "No output filter/output memsink created\n");
		ret = -EINVAL;
		goto evt_subs_done;
	}

	evt_entry = kzalloc(sizeof(stm_event_subscription_entry_t), GFP_KERNEL);
	if (!evt_entry) {
		printk(KERN_ERR "Out of memory for subscription entry\n");
		ret = -ENOMEM;
		goto evt_subs_done;
	}

	/*
	 * Create the event subscription with the following events
	 */
	evt_entry->object = filter->memsink_interface;
	evt_entry->event_mask = STM_MEMSINK_EVENT_DATA_AVAILABLE |
				STM_MEMSINK_EVENT_BUFFER_OVERFLOW;
	evt_entry->cookie = NULL;

	ret = stm_event_subscription_create(evt_entry, 1, &evt_subs);
	if (ret) {
		printk(KERN_ERR "Failed to create event subscription\n");
		goto evt_subs_create_failed;
	}

	filter->evt_subs = evt_subs;
	filter->evt_subs_entry = evt_entry;

	return 0;

evt_subs_create_failed:
	kfree(evt_entry);
evt_subs_done:
	return ret;
}

/**
 * stm_dmx_subscription_remove()
 * This function removes the subscription entries and the subscription
 * created for the output filter
 */
static void stm_dmx_subscription_remove(struct stm_dvb_filter_s *filter)
{
	stm_event_subscription_h evt_subs;
	stm_event_subscription_entry_t *evt_entry;

	/*
	 * If there's no event entry, then there won't be any subcription
	 * created, so, return
	 */
	if (!filter || !filter->evt_subs_entry) {
		printk(KERN_DEBUG "There's no filter or its subscrption entry\n");
		return;
	}

	evt_subs = filter->evt_subs;
	evt_entry = filter->evt_subs_entry;

	/*
	 * Delete the event subscription entry
	 */
	if (stm_event_subscription_modify(evt_subs, evt_entry,
					STM_EVENT_SUBSCRIPTION_OP_REMOVE))
		printk(KERN_ERR "Failed to delete subscription entry\n");

	/*
	 * Delete the subscription
	 */
	if (stm_event_subscription_delete(evt_subs))
		printk(KERN_ERR "Failed to delete subscription\n");

	if (evt_entry)
		kfree(evt_entry);

	filter->evt_subs = NULL;
	filter->evt_subs_entry = NULL;
}

/**
 * stm_dmx_delete_chain() - delete chain and associated filters
 * @chain: demux chain created for the particular filter
 * This detaches input filter from output filter to stop the data flow from
 * TE and follows it with deletion of input filter. Now, detach memsink from
 * output filter if the data flow is non-tunneled or with decoder if tunneled.
 * Complete this call with deletion of output filter and removal of chain
 * from internal linked lists.
 */
int stm_dmx_delete_chain(struct stm_dvb_filter_chain_s *chain)
{
	int ret;

	/*
	 * No chain, so, nothing to delete
	 */
	if (!chain)
		return 0;

	/*
	 * Detach input filter from output filter
	 */
	stm_dvb_chain_stop(chain);

	if (0 == (--(chain->input_filter->users))) {
#ifdef CONFIG_STLINUXTV_CRYPTO
		/* A channel associated with an input filter is always a
		 * a packet mode channel - it must be detacted via the
		 * filter_set_ca_channel function */
		if (chain->input_filter->ca_channel)
			filter_set_ca_channel(chain->input_filter, NULL);
#endif

		if (0 != stm_te_filter_delete(chain->input_filter->handle)) {
			printk(KERN_ERR "Unable to delete input filter\n");
			chain->input_filter->users++;
			return -EINVAL;
		}
		kfree(chain->input_filter);
	}

	if (0 == (--(chain->output_filter->users))) {
#ifdef CONFIG_STLINUXTV_CRYPTO
		/* A channel associated with an output filter is always a
		 * a block mode scrambling channel. In that case the channel
		 * must be both detached from the output filter AND the
		 * memsink object */
		if (chain->output_filter->ca_channel) {
			ret = stm_te_filter_detach(chain->output_filter->handle,
				   chain->output_filter->ca_channel->transform);
			if (ret)
				printk(KERN_ERR "Failed to detach output "\
						"filter from transform (%d)\n",
						ret);

			ret = stm_ce_transform_detach(
				chain->output_filter->ca_channel->transform,
				chain->output_filter->memsink_interface);
			if (ret)
				printk(KERN_ERR "Failed to detach output "\
						"filter from transform (%d)\n",
						ret);
		} else
#endif
		if (chain->output_filter->memsink_interface) {
			ret = stm_te_filter_detach(chain->output_filter->handle,
						(void *)chain->output_filter->
						 memsink_interface);
			if (ret)
				printk(KERN_ERR "Unable to detach filter "\
					"object from its attached sink (%d)\n",
					ret);
		}

		if (chain->output_filter->memsink_interface) {
			/*
			 * Delete the subscription associated
			 * with the output filter
			 */
			stm_dmx_subscription_remove(chain->output_filter);

			if (stm_memsink_delete(chain->output_filter->
					   memsink_interface))
				printk(KERN_ERR
					"%s: failed to delete memsink\n", __func__);
		}
		if (chain->output_filter->decoder) {
			if (0 != stm_te_filter_detach(
						chain->output_filter->handle,
						(void *)chain->
						output_filter->decoder))
				printk(KERN_ERR
					"Unable to detach filter object from SE object\n");
		}
		if (chain->output_filter->buf_data)
			kfree(chain->output_filter->buf_data);
		if (0 != stm_te_filter_delete(chain->output_filter->handle))
			printk(KERN_ERR "Unable to delete filter object\n");
		kfree(chain->output_filter);
	}

	/*
	 * Remove this particular chain from the global list
	 */
	list_del(&chain->input_list);
	list_del(&chain->output_list);
	kfree(chain);

	return 0;
}

void dump_demux_chain_data(struct stm_dvb_demux_s *demux)
{
	struct list_head *temp;
	struct stm_dvb_filter_chain_s *match;

	printk(KERN_DEBUG "Listing the pid filter list\n");
	list_for_each_prev(temp, &demux->pid_filter_handle_list) {
		match =
		    list_entry(temp, struct stm_dvb_filter_chain_s, input_list);
		if (match) {
			printk(KERN_DEBUG
			       "\tPid is %d input is 0x%p output is 0x%p\n",
			       match->pid, match->input_filter,
			       match->output_filter);
		}
	}

	printk(KERN_DEBUG "Listing the PES_output filter list\n");
	list_for_each_prev(temp, &demux->pes_filter_handle_list) {
		match =
		    list_entry(temp, struct stm_dvb_filter_chain_s,
			       output_list);
		if (match) {
			printk(KERN_DEBUG
			       "\tPid is %d input is 0x%p output is 0x%p\n",
			       match->pid, match->input_filter,
			       match->output_filter);
		}
	}

	printk(KERN_DEBUG "Listing the PCR_output filter list\n");
	list_for_each_prev(temp, &demux->pcr_filter_handle_list) {
		match =
		    list_entry(temp, struct stm_dvb_filter_chain_s,
			       output_list);
		if (match) {
			printk(KERN_DEBUG
			       "\tPid is %d input is 0x%p output is 0x%p\n",
			       match->pid, match->input_filter,
			       match->output_filter);
		}
	}

	printk(KERN_DEBUG "Listing the TS_output filter list\n");
	list_for_each_prev(temp, &demux->ts_filter_handle_list) {
		match =
		    list_entry(temp, struct stm_dvb_filter_chain_s,
			       output_list);
		if (match) {
			printk(KERN_DEBUG
			       "\tPid is %d input is 0x%p output is 0x%p\n",
			       match->pid, match->input_filter,
			       match->output_filter);
		}
	}

	printk(KERN_DEBUG "Listing the INDEX_output filter list\n");
	list_for_each_prev(temp, &demux->index_filter_handle_list) {
		match =
		    list_entry(temp, struct stm_dvb_filter_chain_s,
			       output_list);
		if (match) {
			printk(KERN_DEBUG
			       "\tPid is %d input is 0x%p output is 0x%p\n",
			       match->pid, match->input_filter,
			       match->output_filter);
		}
	}
}

struct stm_dvb_filter_chain_s *add_chain_to_demux(struct stm_dvb_demux_s *demux,
						  struct stm_dvb_filter_s
						  *input,
						  struct stm_dvb_filter_s
						  *output, unsigned int pid,
						  enum map_e map, void *filter,
						  enum input_filter_type filter_type)
{
	struct list_head *list_to_add;
	struct stm_dvb_filter_chain_s *chain;
	stm_te_filter_t output_type;
	struct stm_dvb_filter_s *new_input = NULL, *new_output = NULL;
	unsigned int type;
	int result;
	int ret;

	chain = kzalloc(sizeof(struct stm_dvb_filter_chain_s), GFP_KERNEL);
	if (!chain) {
		printk(KERN_ERR "%s: failed to allocate memory for chain\n",
		       __func__);
		return NULL;
	}

	chain->state = CHAIN_STOPPED;

	if (!input) {
		ALLOC_FILTER(new_input, error_alloc_input_filter);

		switch (filter_type) {
			case INPUT_PID_FILTER:
				ret =
					stm_te_pid_filter_new(demux->demux_object, pid,
							&new_input->handle);
				break;
			case INSERT_PID_FILTER:
				ret =
					stm_te_pid_ins_filter_new(demux->demux_object, pid,
							&new_input->handle);
				break;
			case REPLACE_PID_FILTER:
				ret =
					stm_te_pid_rep_filter_new(demux->demux_object, pid,
							&new_input->handle);
				break;
			default:
				goto error_bad_filter_type;
		}
		if (ret) {
			printk(KERN_ERR
			       "%s: stm_te_pid_filter_new failed (%d)\n",
			       __func__, ret);
			goto error_pid_filter_new;
		}
		input = new_input;
	}
	chain->input_filter = input;
	input->users++;

	switch (map) {
	case PES_MAP:
		list_to_add = &demux->pes_filter_handle_list;
		output_type = STM_TE_PES_FILTER;
		type = PES_CHAIN;
		break;
	case TS_MAP:
		list_to_add = &demux->ts_filter_handle_list;
		output_type = STM_TE_TS_FILTER;
		type = PES_CHAIN;
		break;
	case SECTION_MAP:
		list_to_add = &demux->sec_filter_handle_list;
		output_type = STM_TE_SECTION_FILTER;
		type = SECTION_CHAIN;
		break;
	case PCR_MAP:
		list_to_add = &demux->pcr_filter_handle_list;
		output_type = STM_TE_PCR_FILTER;
		type = PCR_CHAIN;
		break;
	case INDEX_MAP:
		list_to_add = &demux->index_filter_handle_list;
		output_type = STM_TE_TS_INDEX_FILTER;
		type = INDEX_CHAIN;
		break;
	case MAX_MAP:
	default:
		goto error_bad_map_type;
	}

	if (!output) {
		ALLOC_FILTER(new_output, error_alloc_output_filter);
		ret =
		    stm_te_output_filter_new(demux->demux_object, output_type,
					     &new_output->handle);
		if (ret) {
			printk(KERN_ERR
			       "%s: stm_te_output_filter_new failed (%d)\n",
			       __func__, ret);
			goto error_output_filter_new;
		}
		output = new_output;
	}
	chain->output_filter = output;
	output->users++;

	chain->pid = pid;
	chain->filter = filter;
	chain->type = type;

	printk(KERN_DEBUG
	       "Address to add to 0x%p are input 0x%p : output 0x%p\n", chain,
	       chain->input_filter, chain->output_filter);

	list_add(&chain->input_list, &demux->pid_filter_handle_list);

	list_add(&chain->output_list, list_to_add);

	dump_demux_chain_data(demux);

	return chain;

error_output_filter_new:
	if (new_output)
		kfree(new_output);
	input->users--;
error_alloc_output_filter:
error_bad_map_type:
	if (new_input) {
		ret = stm_te_filter_delete(new_input->handle);
		if (ret)
			printk(KERN_ERR
			       "%s: failed to delete the filter (%d)\n",
			       __func__, ret);
	}
error_bad_filter_type:
error_pid_filter_new:
	if (new_input)
		kfree(new_input);
error_alloc_input_filter:
	kfree(chain);
	return NULL;
}

/**
 * attach_sink_to_chain
 * Attach output filter to sink. The sink may be
 * a. memsink to fetch the data from demux
 * b. decoder handle to push data directly to decoder
 */
int attach_sink_to_chain(struct stm_dvb_filter_chain_s *chain,
			 stm_object_h sink, char *memsink_name,
			 stm_data_mem_type_t address)
{
	int ret;
	struct stm_dvb_filter_s *output_filter;

	output_filter = chain->output_filter;

	if (sink == NULL) {
		/* Need to create a proper memsink object and connect it to the output filter */
		ret = stm_memsink_new(memsink_name,
				STM_IOMODE_BLOCKING_IO,
				address,
				&output_filter->memsink_interface);
		if (ret) {
			printk(KERN_ERR "%s: failed to create memsink (%d)\n",
					__func__, ret);
			goto error_memsink_new;
		}

		ret = stm_te_filter_attach(output_filter->handle,
				output_filter->memsink_interface);
		if (ret) {
			printk(KERN_ERR
					"%s: failed to attach the memsink to the output filter (%d)\n",
					__func__, ret);
			goto error_memsink_attach;
		}

		/*
		 * Create the event subscription for TE events
		 */
		ret = stm_dmx_subscription_create(output_filter);
		if (ret) {
			printk(KERN_ERR "Failed to create subscription for TE events\n");
			goto error_filter_subs_create;
		}

		/*
		 * Set a wait queue for poll
		 */
		ret = stm_event_set_wait_queue(output_filter->evt_subs,
				&output_filter->wait, true);
		if (ret) {
			printk(KERN_ERR "Failed to setup wait queue for TE events\n");
			goto error_filter_set_wait_q_failed;
		}

	} else {
		/* Simply attach the provided sink */
		output_filter->decoder = sink;
		ret = stm_te_filter_attach(output_filter->handle,
				   (void *)output_filter->decoder);

		if (ret) {
			printk(KERN_ERR
			       "%s: failed to attach the sink to the output filter (%d)\n",
			       __func__, ret);
			goto error_sink_attach;
		}
	}

	return 0;

error_sink_attach:
	return ret;

error_filter_set_wait_q_failed:
	stm_dmx_subscription_remove(output_filter);
error_filter_subs_create:
	if (stm_te_filter_detach(output_filter->handle,
					output_filter->memsink_interface))
		printk(KERN_ERR "Failed to detach output filter from memsink\n");
error_memsink_attach:
	if (stm_memsink_delete(output_filter->memsink_interface))
		printk(KERN_ERR "%s: failed to delete the memsink\n", __func__);
error_memsink_new:
	return ret;
}

struct stm_dvb_filter_chain_s *get_primary_ts_chain(struct stm_dvb_demux_s
						    *demux)
{
	struct list_head *pos;

	list_for_each(pos, &demux->ts_filter_handle_list) {
		return list_entry(pos, struct stm_dvb_filter_chain_s,
				  output_list);
	}

	return NULL;
}

struct stm_dvb_filter_chain_s *match_filter_output_pid(struct stm_dvb_demux_s
						       *demux, int pid,
						       enum map_e map)
{
	struct stm_dvb_filter_chain_s *match = NULL;
	struct list_head *list_to_search;
	struct list_head *pos;

	switch (map) {
	case PES_MAP:
		list_to_search = &demux->pes_filter_handle_list;
		break;
	case TS_MAP:
		list_to_search = &demux->ts_filter_handle_list;
		break;
	case SECTION_MAP:
		list_to_search = &demux->sec_filter_handle_list;
		break;
	case PCR_MAP:
		list_to_search = &demux->pcr_filter_handle_list;
		break;
	case INDEX_MAP:
		list_to_search = &demux->index_filter_handle_list;
		break;
	case MAX_MAP:
	default:
		return NULL;

	}

	list_for_each(pos, list_to_search) {
		match =
		    list_entry(pos, struct stm_dvb_filter_chain_s, output_list);
		if (match && (match->pid == pid)) {
			return match;
		}
	}

	return NULL;
}

struct stm_dvb_filter_chain_s *match_chain_from_filter(struct stm_dvb_demux_s
						       *demux, struct dmxdev_filter
						       *filter)
{
	struct dmxdev_filter *dmxdevfilter = filter;
	struct stm_dvb_filter_chain_s *match = NULL;
	struct list_head *list_to_search;
	struct list_head *pos;

	/* check pcr filter */
	list_to_search = &demux->pcr_filter_handle_list;
	list_for_each(pos, list_to_search) {
		match =
		    list_entry(pos, struct stm_dvb_filter_chain_s, output_list);
		if (match && (match->filter == (void *)filter)) {
			return match;
		}
	}

	/* check the index filter */
	list_to_search = &demux->index_filter_handle_list;
	list_for_each(pos, list_to_search) {
		match =
		    list_entry(pos, struct stm_dvb_filter_chain_s, output_list);
		if (match && (match->filter == (void *)filter)) {
			return match;
		}
	}

	if (dmxdevfilter->type == DMXDEV_TYPE_PES) {
		if ((dmxdevfilter->params.pes.output == DMX_OUT_TAP) ||
		    (dmxdevfilter->params.pes.output == DMX_OUT_DECODER)) {
			list_to_search = &demux->pes_filter_handle_list;
		} else {
			list_to_search = &demux->ts_filter_handle_list;
		}
	} else if (dmxdevfilter->type == DMXDEV_TYPE_SEC) {
		list_to_search = &demux->sec_filter_handle_list;
	} else {
		/* insert/replace filter will fall here due to DMXDEV_TYPE_NONE
		 * type.
		 * TODO: use additional check to identify insert/replace filter
		 * to avoid unnecessary search for other invalid filters
		 */
		list_to_search = &demux->ts_filter_handle_list;
	}

	list_for_each(pos, list_to_search) {
		match =
		    list_entry(pos, struct stm_dvb_filter_chain_s, output_list);
		if (match && (match->filter == (void *)filter)) {
			return match;
		}
	}

	return NULL;
}

/* Finds the filter chain based on the input filter */
struct stm_dvb_filter_chain_s *match_filter_input_pid(struct stm_dvb_demux_s
						      *demux, int pid)
{
	struct stm_dvb_filter_chain_s *match = NULL;
	struct list_head *pos;

	list_for_each(pos, &demux->pid_filter_handle_list) {
		match =
		    list_entry(pos, struct stm_dvb_filter_chain_s, input_list);
		if (match && (match->pid == pid)) {
			printk(KERN_DEBUG "Match was 0x%p - %d\n", match, pid);
			return match;
		}
	}

	return NULL;
}

/* Finds the filter chain based on the output filter streaming engine interface */
struct stm_dvb_filter_chain_s *match_filter_from_decoder(struct
							      stm_dvb_demux_s
							      *demux,
							      stm_object_h
							      interface)
{
	struct stm_dvb_filter_chain_s *match = NULL;
	struct list_head *pos;

	list_for_each(pos, &demux->pes_filter_handle_list) {
		match =
		    list_entry(pos, struct stm_dvb_filter_chain_s, output_list);
		if (match && match->output_filter
		    && match->output_filter->decoder == interface) {
			printk(KERN_DEBUG "Match was 0x%p\n", match);
			return match;
		}
	}

	return NULL;
}

/* Finds the filter chain based on the output filter handle */
struct stm_dvb_filter_chain_s *match_filter_from_output_handler(struct
							      stm_dvb_demux_s
							      *demux,
							      stm_object_h
							      demux_filter)
{
	struct stm_dvb_filter_chain_s *match = NULL;
	struct list_head *pos;

	list_for_each(pos, &demux->pes_filter_handle_list) {
		match =
		    list_entry(pos, struct stm_dvb_filter_chain_s, output_list);
		if (match && match->output_filter
		    && match->output_filter->handle == demux_filter) {
			printk(KERN_DEBUG "Match was 0x%p\n", match);
			return match;
		}
	}

	return NULL;
}

struct stm_dvb_filter_s *get_pid_filter(struct stm_dvb_demux_s *demux, int pid)
{
	struct stm_dvb_filter_chain_s *chain;
	struct stm_dvb_filter_s *filter = NULL;
	int map = 0;

#ifdef SINGLE_LIST
	chain = match_filter_pid(demux, pid, map);
	if (chain && chain->input_filter) {
		filter = chain->input_filter;
		goto out;
	}
#else
	for (map = 0; map < MAX_MAP; map++) {
		chain = match_filter_input_pid(demux, pid);
		if (chain && chain->input_filter) {
			filter = chain->input_filter;
			goto out;
		}
	}
#endif

out:
	return filter;
}

struct stm_dvb_filter_s *get_filter_pacing_output(struct stm_dvb_demux_s *demux)
{
	struct list_head *temp;
	dmx_pes_type_t pes_type;
	struct dmxdev_filter *dmxdevfilter;
	struct stm_dvb_filter_s *filter = NULL;
	struct stm_dvb_filter_chain_s *match, *aud, *ttx, *subt, *pcr, *sec;

	/*
	 * We need to find a new output filter on which to set pacing on.
	 * A. Scan the list and find out PES filter if any. The priority is:
	 *    - DMX_PES_VIDEOxx
	 *    - DMX_PES_AUDIOxx
	 *    - DMX_PES_TELETEXTxx
	 *    - DMX_PES_SUBTITLExx
	 *    - DMX_PES_PCRxx
	 * B. If no PES filter, look out for section filter
	 */
	aud = ttx = subt = pcr = sec = NULL;
	list_for_each_prev(temp, &demux->pid_filter_handle_list) {

		match = list_entry(temp,
				struct stm_dvb_filter_chain_s, input_list);

		dmxdevfilter = match->filter;

		if (dmxdevfilter->type == DMXDEV_TYPE_PES) {

			pes_type = dmxdevfilter->params.pes.pes_type;

			/*
			 * Is there's video chain, return output filtera now
			 */
			if (get_video_dec_pes_type(pes_type) > 0)
				return match->output_filter;

			/*
			 * Iterate over the complete list for this demux and
			 * return the output_filter in the same priority order
			 * as stated above.
			 */
#ifdef CONFIG_STLINUXTV_DECODE_DISPLAY
			if (!aud) {
				if (get_audio_dec_pes_type(pes_type) > 0) {
					aud = match;
					continue;
				}
			}
#endif

			if (!ttx) {
				if (stm_dmx_ttx_get_pes_type(pes_type) > 0) {
					ttx = match;
					continue;
				}
			}

			if (!subt) {
				if (stm_dmx_subt_get_pes_type(pes_type) > 0) {
					subt = match;
					continue;
				}
			}

			if (!pcr) {
				if (get_pcr_pes_type(pes_type) > 0) {
					pcr = match;
					continue;
				}
			}

		} else if ((dmxdevfilter->type == DMXDEV_TYPE_SEC) && !sec) {

			sec = match;
		}
	}

	/*
	 * Find out which output filter to send back
	 */
	if (aud)
		filter = aud->output_filter;
	else if (ttx)
		filter = ttx->output_filter;
	else if (subt)
		filter = subt->output_filter;
	else if (pcr)
		filter = pcr->output_filter;
	else if (sec)
		filter = sec->output_filter;

	return filter;
}

int del_all_chains_from_demux(struct stm_dvb_demux_s *demux)
{
	struct list_head *pos;
	struct list_head *temph;
	struct stm_dvb_filter_chain_s *chain_entry;

	list_for_each_safe(pos, temph, &demux->pid_filter_handle_list) {
		chain_entry =
		    list_entry(pos, struct stm_dvb_filter_chain_s, input_list);
		if (chain_entry) {
			printk(KERN_DEBUG "Trying to delete %d\n",
			       chain_entry->pid);
			dump_demux_chain_data(demux);
			stm_dmx_delete_chain(chain_entry);
			printk(KERN_DEBUG "Deleted\n");
			chain_entry = NULL;
			dump_demux_chain_data(demux);
		}
	}

	return 0;
}

struct stm_dvb_filter_chain_s *demux_get_pcr_by_type(struct stm_dvb_demux_s *stm_demux,
								dmx_pes_type_t pcr_type)
{
	struct list_head *pos;
	struct stm_dvb_filter_chain_s *pcr_chain;
	struct dmxdev_filter *dmxdev_filter;

	list_for_each(pos, &stm_demux->pcr_filter_handle_list) {
		pcr_chain = list_entry(pos,  struct stm_dvb_filter_chain_s, output_list);
		dmxdev_filter = pcr_chain->filter;
		if (dmxdev_filter->params.pes.pes_type == pcr_type)
			return pcr_chain;
	}

	return NULL;
}

/* Finds the PCR filter chain based on the output filtere */
struct stm_dvb_filter_chain_s *find_pcr_filter(struct stm_dvb_demux_s *demux)
{
	struct stm_dvb_filter_chain_s *match = NULL;
	struct list_head *pos;
	list_for_each(pos, &demux->pcr_filter_handle_list) {
		match =
			list_entry(pos, struct stm_dvb_filter_chain_s, output_list);
		if (match && match->output_filter) {
			printk(KERN_DEBUG "Match was 0x%p\n", match);
			return match;
		}
	}

	return NULL;
}

int stm_dvb_detach_stream_from_demux(void * stm_demux, stm_object_h demux_filter, stm_object_h decoder)
{
	struct stm_dvb_filter_chain_s *chain;
	int ret;

	/*
	 * We are supporting sw/hw demux. This call is made by
	 * audio/video devices to detach demux, but for sw demux
	 * this cannot proceed furthur.
	 */
	if (!stm_demux)
		return 0;

	chain =
	    match_filter_from_decoder((struct stm_dvb_demux_s *)stm_demux,
					   decoder);

	/* Check if the decoder is still known by the demux (ie still attached).
	 * It might not be there anymore if the demux has already been stopped.
	 * This is NOT an error
	 */
	if (!chain)
		return 0;

	if (chain->output_filter->handle != demux_filter){
		printk(KERN_ERR "%s: Wrong demux filter handle\n", __func__);
		return 0;
	}

	ret = stm_te_filter_detach(demux_filter, (void *)decoder);
	if (ret) {
		printk(KERN_ERR
		       "%s: failed to detach the play_stream from the output filter (%d)\n",
		       __func__, ret);
		return -EIO;
	}
	chain->output_filter->decoder = NULL;

	/*Search for PCR output filter and detach it from SE interface if it is connected*/
	chain = find_pcr_filter((struct stm_dvb_demux_s *)stm_demux);

	if ((chain !=0)&&(chain->output_filter->decoder!=NULL)) {

		ret = stm_te_filter_detach(chain->output_filter->handle,
				(void *)chain->output_filter->decoder);
		if (ret) {
			printk(KERN_ERR
					"%s: failed to detach the playback object from the PCR  output filter (%d)\n",
					__func__, ret);
			return -EIO;
		}
		chain->output_filter->decoder = NULL;
	}


	return 0;
}

int stm_dvb_attach_stream_to_demux(void *stm_demux, stm_object_h demux_filter, stm_object_h decoder)
{
	struct stm_dvb_filter_chain_s *chain;
	int ret;

	/*
	 * We are supporting sw/hw demux. This call is made by
	 * audio/video devices to detach demux, but for sw demux
	 * this cannot proceed furthur.
	 */
	if (!stm_demux)
		return 0;

	chain =
	    match_filter_from_output_handler((struct stm_dvb_demux_s *)stm_demux,
					   demux_filter);
	if (!chain) {
		printk(KERN_ERR "%s: couldn't find the output filter object\n", __func__);
		/* Filter already no longer exist in Demux world, so no need to do anything */
		return -EINVAL;
	}

	if (chain->output_filter->decoder &&
		(chain->output_filter->decoder != decoder)){
		printk(KERN_ERR "%s: decoder and demux information mismatch\n", __func__);
		return -EINVAL;
	}

	chain->output_filter->decoder = decoder;

	ret = stm_te_filter_attach(demux_filter, (void *)decoder);
	if (ret) {
		printk(KERN_ERR
			"%s: failed to attach the play_stream from the output filter (%d)\n",
			__func__, ret);
		return -EIO;
	}

	return 0;
}

int stm_dvb_chain_start( struct stm_dvb_filter_chain_s *chain )
{
	int ret;

	if (chain->state == CHAIN_STARTED)
		return 0;

	ret = stm_te_filter_attach(chain->input_filter->handle,
				   chain->output_filter->handle);
	if (ret) {
		printk(KERN_ERR
		       "%s: failed to attach TE input & output filters (%d)\n",
		       __func__, ret);
		goto error_filter_attach;
	}

	chain->state = CHAIN_STARTED;
	return 0;

error_filter_attach:
	return -EIO;
}

int stm_dvb_chain_stop( struct stm_dvb_filter_chain_s *chain )
{
	int ret;

	if (chain->state == CHAIN_STOPPED)
		return 0;

	ret = stm_te_filter_detach(chain->input_filter->handle,
				   chain->output_filter->handle);
	if (ret) {
		printk(KERN_ERR
		       "%s: failed to detach TE input & output filters (%d)\n",
		       __func__, ret);
		goto error_filter_detach;
	}

	chain->state = CHAIN_STOPPED;
	return 0;

error_filter_detach:
	return -EIO;
}


#ifdef CONFIG_STLINUXTV_CRYPTO
int stm_dvb_demux_ca_set_pid(struct stm_dvb_demux_s *stm_demux, unsigned int pid)
{
	struct stm_dvb_ca_pid_s *ca_pid_info;
	struct list_head *temp;
	int result = 0;
	struct stm_dvb_filter_s *filter = NULL;

	list_for_each(temp, &stm_demux->ca_pid_list) {
		ca_pid_info = list_entry(temp, struct stm_dvb_ca_pid_s, ca_pid_list);
		if(ca_pid_info->ca_pid.pid == pid) {
			filter = get_pid_filter(stm_demux, pid);
			result = stm_dvb_ca_set_filter_channel(ca_pid_info->stm_ca,
						&ca_pid_info->ca_pid, filter);
			if (result)
				stv_err("unable to set pid %d on ca %p\n",
						ca_pid_info->ca_pid.pid, &ca_pid_info->stm_ca);
			break;
		}
	}

	return result;
}

int stm_dvb_demux_add_ca_pid(struct stm_dvb_ca_s *stm_ca, ca_pid_t *ca_pid)
{
	int ret = 0;
	struct stm_dvb_ca_pid_s *dvb_ca_pidinfo;

	dvb_ca_pidinfo = kzalloc(sizeof(*dvb_ca_pidinfo), GFP_KERNEL);
	if (!dvb_ca_pidinfo) {
		stv_err("out of memory for pidinfo\n");
		ret = -ENOMEM;
		goto error;
	}

	dvb_ca_pidinfo->stm_ca = stm_ca;

	memcpy(&dvb_ca_pidinfo->ca_pid, ca_pid, sizeof(*ca_pid));

	list_add(&dvb_ca_pidinfo->ca_pid_list, &stm_ca->demux->ca_pid_list);

error:
	return ret;
}
#endif
