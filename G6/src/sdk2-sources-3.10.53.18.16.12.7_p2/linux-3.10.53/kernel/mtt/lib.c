/*
 *  Multi-Target Trace solution
 *
 *  MTT - KERNEL API IMPLEMENTATION FILE.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * Copyright (C) STMicroelectronics, 2011
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/spinlock.h>
#include <linux/hardirq.h>

#define __MTT_IMPL__

#include <linux/mtt/mtt.h>

#ifdef CONFIG_KPTRACE
#include <linux/mtt/kptrace.h>
#endif

/*======================== PUBLIC APIs ===============================*/
/*
 Some API entry points are not implemented kernel side.
 mtt_get_capabilities.
 mtt_get_component_handle
 mtt_get_component_filter
 Triggers will be implemented in the future.
*/
DEFINE_SPINLOCK(mttlib_lock);

mtt_return_t mtt_initialize(const mtt_init_param_t *init_param)
{
	return MTT_ERR_NONE;
}
EXPORT_SYMBOL(mtt_initialize);

mtt_return_t mtt_uninitialize(void)
{
	return MTT_ERR_NONE;
}
EXPORT_SYMBOL(mtt_uninitialize);

mtt_return_t
mtt_open(const uint32_t comp_id, const char *comp_name, mtt_comp_handle_t *co)
{
	mtt_printk(KERN_DEBUG
		   "mtt_open: openning trace connection for ID %d (name=%s)\n",
		   comp_id, comp_name);

	if (unlikely(!co))
		return MTT_ERR_PARAM;

	/*allocated a component if needed */
	*co = (mtt_comp_handle_t) mtt_component_alloc(comp_id, comp_name, 0);
	if (!co)
		return MTT_ERR_CONNECTION;

	return MTT_ERR_NONE;
}
EXPORT_SYMBOL(mtt_open);

mtt_return_t mtt_close(const mtt_comp_handle_t co)
{
	if (unlikely(!co))
		return MTT_ERR_PARAM;

	mtt_component_delete(co);

	return MTT_ERR_NONE;
}
EXPORT_SYMBOL(mtt_close);

/*
 * best is to use mtt_trace, plus optionally a short hint to identify
 * the monitored signal. printf-like stuff is a poor approach and has its
 * cost in terms of intrusivity...
 * */
mtt_return_t
mtt_print(const mtt_comp_handle_t co,
	  const mtt_trace_level_t level, const char *format_string, ...)
{
	unsigned long flags;
	va_list ap;
	mtt_packet_t *p;
	mtt_return_t err;
	mtt_trace_item_t type_info;
	uint32_t *payload_ptr;
	uint32_t target;
	int len;

	err = MTT_ERR_NONE;
	flags = 0;

	if (unlikely(!co))
		return MTT_ERR_PARAM;

	/*traces likely to be disabled. */
	if (likely(((struct mtt_component_obj *)co)->active_filter < level))
		return err;

	target = MTT_TARGET_LIN0 + raw_smp_processor_id();

	/* allocate or retrieve a preallocated TRACE frame */
	p = mtt_pkt_alloc(target);

	/* get a pointer to the string payload in the packet buffer
	 **/
	type_info = MTT_TRACEITEM_STRING(0);

	payload_ptr = mtt_pkt_get((struct mtt_component_obj *)co,
				  p, type_info,
				  (uint32_t) __builtin_return_address(0));

	/* build the string in the packet, no copy.
	 **/
	va_start(ap, format_string);
	len = 1 + vsnprintf((char *)payload_ptr, MAX_PKT_PAYLOAD,
				format_string, ap);
	va_end(ap);

	/* update type_info with the string size, now that we know it. */
	_PKT_GET_TYPE_INFO(p) += len;

	mtt_pkt_update_align32(p->length, len);

	spin_lock_irqsave(&mttlib_lock, flags);

	/* send the frame packet
	 * the output driver must take care of locking if needed */
	mtt_cur_out_drv->write_func(p, APILOCK);

	spin_unlock_irqrestore(&mttlib_lock, flags);

	return err;
}
EXPORT_SYMBOL(mtt_print);

/*
 * mtt_trace is very compact for most traces, which is good.
 * */
mtt_return_t
mtt_trace(const mtt_comp_handle_t co,
	  const mtt_trace_level_t level,
	  const uint32_t type_info, const void *data, const char *hint)
{
	unsigned long flags;
	mtt_packet_t *p;
	mtt_return_t err;
	uint32_t *payload_ptr;
	size_t payload_siz;
	uint32_t target;

	err = MTT_ERR_NONE;
	flags = 0;

	/* trap a call to mtt_trace while open was not done
	 * or component handle is NULL.
	 **/
	if (unlikely(!co))
		return MTT_ERR_PARAM;

	/*traces likely to be disabled. */
	if (likely(((struct mtt_component_obj *)co)->active_filter < level))
		return err;

	target = MTT_TARGET_LIN0 + raw_smp_processor_id();

	/* allocate or retrieve a preallocated TRACE frame */
	p = mtt_pkt_alloc(target);

	/*NOTICE: support hint as a short up-to-12 bytes string
	 * we can always read 3 uint32_t from hint and this is
	 * less expensive than dealing with a hash table etc...
	 **/
	payload_siz = type_info & 0x0000FFFF;

	if (hint) {
		payload_ptr = mtt_pkt_get_with_hint((struct mtt_component_obj *)
						    co, p, type_info,
						    (uint32_t)
						    __builtin_return_address
						    (0));

		payload_ptr[payload_siz / 4 + 0] = *((uint32_t *) (hint + 0));
		payload_ptr[payload_siz / 4 + 1] = *((uint32_t *) (hint + 4));
		payload_ptr[payload_siz / 4 + 2] = *((uint32_t *) (hint + 8));

	} else {
		payload_ptr = mtt_pkt_get((struct mtt_component_obj *)co,
					  p, type_info,
					  (uint32_t)
					  __builtin_return_address(0));
	}

	memcpy(payload_ptr, data, payload_siz);

	spin_lock_irqsave(&mttlib_lock, flags);

	/* send the frame packet
	 * the output driver must take care of locking if needed */
	mtt_cur_out_drv->write_func(p, APILOCK);

	spin_unlock_irqrestore(&mttlib_lock, flags);

	return err;
}
EXPORT_SYMBOL(mtt_trace);

/*
 * best is to use mtt_trace, plus optionally a short hint to identify
 * the monitored signal. raw stuff will not be handled by the analysis tools
 * and isn't really a gain of intrusivity...
 * */
mtt_return_t
mtt_data_raw(const uint32_t co_id,
	     const uint32_t level, const uint32_t len, const void *data)
{
	unsigned long flags;
	mtt_packet_t *p;
	mtt_return_t err;
	uint32_t *payload_ptr;
	uint32_t target;
	struct mtt_component_obj *co;

	err = MTT_ERR_NONE;
	flags = 0;

	/* This trace routine will be used for bug data dumps
	 * only, we don't allocate a component, but just output
	 * the packet on the kmux channel
	 */
	(void)co_id; /* ignored for now */

	co = mtt_comp_kmux[raw_smp_processor_id()];

	/*traces likely to be disabled. */
	if (likely(co->active_filter < level))
		return err;

	target = MTT_TARGET_LIN0 + raw_smp_processor_id();

	/* allocate or retrieve a preallocated RAW packet */
	p = mtt_pkt_alloc_raw(target, len);

	spin_lock_irqsave(&mttlib_lock, flags);

	payload_ptr = mtt_pkt_get_raw(co, p,
				      len,
				      (uint32_t) __builtin_return_address(0));

	/*type_info is a full 32 size field in this case! */
	memcpy(payload_ptr, data, len);

	/* send the frame packet
	 * the output driver must take care of locking if needed */
	mtt_cur_out_drv->write_func(p, APILOCK);

	spin_unlock_irqrestore(&mttlib_lock, flags);

	return err;
}
EXPORT_SYMBOL(mtt_data_raw);

mtt_return_t
mtt_set_core_filter(const uint32_t new_level, uint32_t *prev_level)
{
	if (prev_level)
		*prev_level = mtt_sys_config.filter;

	if (new_level != mtt_sys_config.filter) {
		mtt_sys_config.filter = new_level;
		mtt_component_snap_filter(new_level);
	}

	return MTT_ERR_NONE;
}
EXPORT_SYMBOL(mtt_set_core_filter);

mtt_return_t mtt_get_core_filter(uint32_t *level)
{
	if (unlikely(!level))
		return MTT_ERR_PARAM;

	*level = mtt_sys_config.filter;

	return MTT_ERR_NONE;
}
EXPORT_SYMBOL(mtt_get_core_filter);

mtt_return_t mtt_set_component_filter(const mtt_comp_handle_t co,
				      const uint32_t new_level,
				      uint32_t *prev_level)
{
	if (unlikely(!co))
		return MTT_ERR_PARAM;

	if (prev_level)
		*prev_level = ((struct mtt_component_obj *)co)->filter;

	((struct mtt_component_obj *)co)->filter = new_level;
	((struct mtt_component_obj *)co)->active_filter = new_level;

	return MTT_ERR_NONE;
}
EXPORT_SYMBOL(mtt_set_component_filter);

mtt_return_t mtt_get_component_filter(const mtt_comp_handle_t co,
				      uint32_t *level)
{
	if (unlikely(!co || !level))
		return MTT_ERR_PARAM;

	if (level)
		*level = ((struct mtt_component_obj *)co)->filter;

	return MTT_ERR_NONE;
}
EXPORT_SYMBOL(mtt_get_component_filter);

#ifdef CONFIG_KPTRACE

/*======================== KPTRACE INTERNAL HANDLERS ==================*/

/*
 * Allocate and prepare a packet.
 *
 * returns the pointer to the right offset in the packet to
 * start writing the payload.
 */
uint32_t *mtt_kptrace_pkt_get(uint32_t type_info, mtt_packet_t **p,
		uint32_t loc)
{
	long core = 0;
	uint32_t target = MTT_TARGET_LIN0;

	BUG_ON(!p);

	/* kptrace may be disabled, but this is checked prior
	 * to enabling the trace points. */

	core = raw_smp_processor_id();

	target = MTT_TARGET_LIN0 + core;

	/* allocate or retrieve a preallocated TRACE frame */
	*p = mtt_pkt_alloc(target);

	return mtt_pkt_get(mtt_comp_cpu[core], *p, type_info, loc);
}

/* Send the packet for output and release it. */
int mtt_kptrace_pkt_put(mtt_packet_t *p)
{
	unsigned long flags = 0;

	spin_lock_irqsave(&mttlib_lock, flags);

	/* send the frame packet
	* the output driver must take care of locking if needed */
	mtt_cur_out_drv->write_func(p, APILOCK);

	spin_unlock_irqrestore(&mttlib_lock, flags);
	return 0;
}

#endif

/*===================== COMMANDS & NOTIFICATIONS ==================*/

/* send a notification or reply packet for MTT_CMD_GET_CNAME
 **/
void mtt_get_cname_tx(const uint32_t comp_id)
{
	unsigned long flags = 0;
	long core = 0;
	mtt_packet_t *p;
	mtt_cmd_get_cname_t *payload_ptr;
	uint32_t type_info = MTT_TRACEITEM_BLOB(sizeof(mtt_cmd_get_cname_t));

	if (!(mtt_sys_config.state & MTT_STATE_RUNNING))
		return;

	core = raw_smp_processor_id();

	/* allocate or retrieve a preallocated TRACE frame */
	p = &(mtt_pkts_cmd[core]);

	_PKT_SET_CMD(p, MTT_CMD_GET_CNAME | MTT_PARAM_RET);

	payload_ptr = (mtt_cmd_get_cname_t *) mtt_pkt_get(mtt_comp_cpu[core],
							  p, type_info, 0);
	payload_ptr->comp_id = comp_id;
	strncpy(payload_ptr->comp_name, mtt_components_get_name(comp_id), 64);

	spin_lock_irqsave(&mttlib_lock, flags);

	/* send the frame packet
	 * the output driver must take care of locking if needed */
	mtt_cur_out_drv->write_func(p, APILOCK);

	spin_unlock_irqrestore(&mttlib_lock, flags);
};

/* send a notification or reply packet for MTT_CMD_VERSION
 **/
void mtt_version_tx(void)
{
	unsigned long flags = 0;
	long core = 0;
	mtt_packet_t *p;
	mtt_cmd_version_t *payload_ptr;
	uint32_t type_info = MTT_TRACEITEM_BLOB(sizeof(mtt_cmd_version_t));

	core = raw_smp_processor_id();

	/* allocate or retrieve a preallocated TRACE frame */
	p = &(mtt_pkts_cmd[core]);

	_PKT_SET_CMD(p, MTT_CMD_VERSION | MTT_PARAM_RET);

	payload_ptr = (mtt_cmd_version_t *) mtt_pkt_get(mtt_comp_cpu[core],
							p, type_info, 0);
	payload_ptr->version = MTT_API_VERSION;
	strncpy(payload_ptr->string, MTT_API_VER_STRING,
		MTT_API_VER_STRING_LEN);

	spin_lock_irqsave(&mttlib_lock, flags);

	/* send the frame packet
	 * the output driver must take care of locking if needed */
	mtt_cur_out_drv->write_func(p, APILOCK);

	spin_unlock_irqrestore(&mttlib_lock, flags);
};

/* Send a STOP notification downsteam
 */
void mtt_stop_tx(void)
{
	unsigned long flags = 0;
	mtt_packet_t *p;

	spin_lock_irqsave(&mttlib_lock, flags);

	p = &(mtt_pkts_cmd[0]);

	_PKT_SET_CMD(p, MTT_CMD_STOP | MTT_PARAM_RET);

	mtt_pkt_get(mtt_comp_cpu[0], p, 0, 0);

	mtt_cur_out_drv->write_func(p, APILOCK);

	spin_unlock_irqrestore(&mttlib_lock, flags);
};
