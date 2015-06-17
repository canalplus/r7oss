/*
 *  Multi-Target Trace solution
 *
 *  MTT - DATA PACKET HANDLING.
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
#include <linux/version.h>
#include <linux/kobject.h>
#include <linux/bug.h>
#include <linux/slab.h>
#include <linux/hardirq.h>

#include <linux/mtt/mtt.h>

#ifdef CONFIG_KPTRACE
#include <linux/mtt/kptrace.h>
#endif

/* Simply allocate one packet per core for now. */
#define MAX_PACKETS 8

/* Command packets handling structures. */
mtt_packet_t *mtt_pkts_cmd;

#ifdef CONFIG_KPTRACE
mtt_packet_t *mtt_pkts_ctxt;
mtt_packet_t *mtt_pkts_irqs;
#endif

struct packet_index {
	unsigned int val;
	spinlock_t lock;
};

static DEFINE_PER_CPU(struct packet_index, cur_packet);
static DEFINE_PER_CPU(mtt_packet_t, pkts[MAX_PACKETS]);

/* Initialize packet handling, preallocate/preinit packets. */
int __init mtt_pkt_init(void)
{
	int core;
	int err = 0;

	/* Bail out if already init because of console. */
	if (mtt_pkts_cmd && mtt_pkts_cmd[0].u.buf &&
	    (*(uint32_t *) (mtt_pkts_cmd[0].u.buf) == (MTT_SYNC
						       | (MTT_PTCL_VER_MAJOR <<
							  12)
						       | (MTT_PTCL_VER_MINOR <<
							  8)
						       | MTT_TARGET_LIN0)))
		return 0;

	/* Allocate the command packet handling structures */
	mtt_pkts_cmd = kmalloc(num_possible_cpus() * sizeof(mtt_packet_t),
			       GFP_KERNEL);

	if (!mtt_pkts_cmd)
		return -ENOMEM;

#ifdef CONFIG_KPTRACE
	mtt_pkts_irqs = kmalloc(num_possible_cpus() * sizeof(mtt_packet_t),
				GFP_KERNEL);
	mtt_pkts_ctxt = kmalloc(num_possible_cpus() * sizeof(mtt_packet_t),
				GFP_KERNEL);

	if (unlikely(!mtt_pkts_irqs || !mtt_pkts_ctxt))
		return -ENOMEM;
#endif

	/* Preallocate a trace packet per core.
	 * this will be small pieces of cache aligned
	 * contiguous memory.
	 *
	 * for now, we do not allocate more than a page per
	 * packet.
	 */
	for (core = 0; (core < num_possible_cpus()) && !err; core++) {
		int i;
		mtt_packet_t *p = per_cpu(pkts, core);
		struct packet_index *p_index = &per_cpu(cur_packet, core);

		p_index->val = 0;
		spin_lock_init(&p_index->lock);

		for (i = 0; (i < MAX_PACKETS) && !err; i++) {
			p[i].u.buf = kmalloc(MAX_PKT_SIZE, GFP_KERNEL);
			err = p[i].u.buf ? 0 : 1;
		}

		/* Command & notification packets */
		if (!err)
			mtt_pkts_cmd[core].u.buf = kmalloc(MAX_PKT_SIZE,
							   GFP_KERNEL);
		if (!mtt_pkts_cmd[core].u.buf)
			err = ENOMEM;

		if (!err) {
			/* Initialize packet header in advance
			 * do this once params are known
			 */
			MTT_PTCL_SET_HEADER(mtt_pkts_cmd[core].u.buf,
					    MTT_TARGET_LIN0 + core,
					    MTT_CMD_VERSION, MTT_PARAM_RET);
		}
#ifdef CONFIG_KPTRACE
		/*context switch packets */
		mtt_pkts_ctxt[core].u.buf = kmalloc(MAX_PKT_SIZE, GFP_KERNEL);

		if (!mtt_pkts_ctxt[core].u.buf)
			err = ENOMEM;

		MTT_PTCL_SET_HEADER(mtt_pkts_ctxt[core].u.buf,
				    MTT_TARGET_LIN0 + core,
				    MTT_CMD_CSWITCH, mtt_sys_config.params);

		/* Irqs packets */
		mtt_pkts_irqs[core].u.buf = kmalloc(MAX_PKT_SIZE, GFP_KERNEL);

		if (!mtt_pkts_irqs[core].u.buf)
			err = ENOMEM;

		MTT_PTCL_SET_HEADER(mtt_pkts_irqs[core].u.buf,
				    MTT_TARGET_LIN0 + core,
				    MTT_CMD_TRACE, mtt_sys_config.params);
#endif /*CONFIG_KPTRACE */
	}

	if (!err)
		mtt_printk(KERN_DEBUG "mtt: pre-allocated/pre-init packets.\n");
	else {
		/* kfree is safe on null pointers, just try to release
		 * whatever we got.
		 */
		mtt_pkt_cleanup();
		return -ENOMEM;
	}

	return 0;
}
early_initcall(mtt_pkt_init);

/* Preset sysconfig in preallocated packets, if it changed */
void mtt_pkt_config(void)
{
	int core;
	for (core = 0; core < num_possible_cpus(); core++) {
		_PKT_SET_CMD(&mtt_pkts_cmd[core],
			     (MTT_CMD_TRACE | mtt_sys_config.params));
#ifdef CONFIG_KPTRACE
		_PKT_SET_CMD(&mtt_pkts_ctxt[core],
			     (MTT_CMD_CSWITCH
			      | MTT_PARAM_CTXT | mtt_sys_config.params));
		_PKT_SET_CMD(&mtt_pkts_irqs[core],
			     (MTT_CMD_TRACE | mtt_sys_config.params));
#endif
	}
}

void mtt_pkt_cleanup(void)
{
	int core, j;

	for (core = 0; core < num_possible_cpus(); core++) {
		mtt_packet_t *p = per_cpu(pkts, core);

		for (j = 0; j < MAX_PACKETS; j++)
			kfree(p[j].u.buf);

		kfree(mtt_pkts_cmd[core].u.buf);
#ifdef CONFIG_KPTRACE
		kfree(mtt_pkts_ctxt[core].u.buf);
		kfree(mtt_pkts_irqs[core].u.buf);
#endif
	}

	kfree(mtt_pkts_cmd);
	mtt_pkts_cmd = 0;

#ifdef CONFIG_KPTRACE
	kfree(mtt_pkts_ctxt);
	kfree(mtt_pkts_irqs);
#endif

}

/* Return a preallocated trace packet, or allocate */
mtt_packet_t *mtt_pkt_alloc(mtt_target_type_t target)
{
	struct packet_index *p_index = &per_cpu(cur_packet, target);
	mtt_packet_t *p_pkts = per_cpu(pkts, target);
	mtt_packet_t *p;
	unsigned long flags;

	spin_lock_irqsave(&p_index->lock, flags);
	p_index->val = (p_index->val + 1) & (MAX_PACKETS - 1);
	p = &p_pkts[p_index->val];

	spin_unlock_irqrestore(&p_index->lock, flags);

	MTT_PTCL_SET_HEADER(p->u.buf, target, MTT_CMD_TRACE,
			    mtt_sys_config.params);

	return p;
}

/* Return a preallocated raw packet, or allocate
 * hopefully none will use this from irq...
 * */
mtt_packet_t *mtt_pkt_alloc_raw(mtt_target_type_t target, size_t len)
{
	mtt_packet_t *pktsp;

	/* pktsp is guarantied to be non null. */
	pktsp = mtt_pkt_alloc(target);

	if (pktsp->length >= len)
		return pktsp;

	/* Was too small, release. */
	kfree(pktsp->u.buf);

	/* Allocate, or re-allocate with sufficient size
	 * for foolishly big dump packets, allocate from emergency pool
	 * as the system status is already compromised probably.
	 * Let the user know who did this huge allocation, in case it leads
	 * to issues. */
	pktsp->u.buf = kmalloc_track_caller(len, GFP_DMA | GFP_ATOMIC);
	if (!pktsp->u.buf)
		return NULL;

	pktsp->length = len;

	return pktsp;
}

/* Put 64 TS into frame */
#define mtt_pkt_put_tstv(buf, tv) { \
	*(uint32_t *)(buf+0) = tv.tv_sec; \
	*(uint32_t *)(buf+4) = tv.tv_usec; \
	buf += 8; \
}

/* Put PID into frame */
#define mtt_pkt_put_ctxt(buf) { \
	*(uint32_t *)buf = current_thread_info()->task->pid ; \
	buf += 4; \
}

/* Put level into frame */
#define mtt_pkt_put_level(buf, level) { \
	*(uint32_t *)buf = level; \
	buf += 4; \
}

/* Put component ID */
#define mtt_pkt_put_comp_id(buf, co) { \
	*(uint32_t *)buf = co->id; \
	buf += 4; \
}

/* Store the trace ID */
#define mtt_pkt_put_trace_id(buf, loc) { \
	*(uint32_t *)buf = loc; \
	buf += 4; \
}

/* Put type_info/size */
#define mtt_pkt_put_type_info(buf, t) { \
	*(uint32_t *)buf = t; \
	buf += 4; \
}

#define DEFAULT_PKT_LINUX	(6*4)

/* Initialize payload for a given type
 * and set packet size param.
 **/
uint32_t *mtt_pkt_get(struct mtt_component_obj *co, mtt_packet_t *p,
		      mtt_trace_item_t type_info, uint32_t loc)
{
	char *buf;

	uint32_t params = _PKT_GET_CMD(p);

	/* Prepare size for a regular Linux trace packet. */
	p->length = DEFAULT_PKT_LINUX;
	p->comp = (void *)co;

	/*-----------------------preamble part--------------------------*/

	/* The first fields are pre-initialized.
	 * sync-version-target
	 * command-param
	 **/

	/* Move to componentid/
	 */
	buf = p->u.buf + 2 * 4;
	mtt_pkt_put_comp_id(buf, co);

	/* Move to trace ID/
	 */
	mtt_pkt_put_trace_id(buf, loc);

	/* Write the type info */
	mtt_pkt_put_type_info(buf, type_info);

	/*-----------------------optional part--------------------------*/

	/* Write the PID if needed */
	if (unlikely(!(params & MTT_PARAM_CTXT)))
		p->length -= 4;
	else
		mtt_pkt_put_ctxt(buf);

	/* Write the time-stamp if needed */
	if (params & MTT_PARAM_TSTV) {
		struct timeval tv;
		do_gettimeofday(&tv);
		mtt_pkt_put_tstv(buf, tv);
		p->length += 8;
	}

	/* Add payload size
	 * the rounding is to insure that a frame
	 * start is always aligned to a 32bits boundary.
	 **/
	mtt_pkt_update_align32(p->length, type_info);

	/* Handle level, but no. */
	if (unlikely(params & MTT_PARAM_LEVEL)) {
		mtt_pkt_put_level(buf, 0);
		p->length += 4;
	}

	/* Return offset of payload */
	return (uint32_t *) buf;
}

#ifdef CONFIG_KPTRACE
/* Optimized packet initializer, for context switches. */
int mtt_cswitch(uint32_t old_c, uint32_t new_c)
{
	char *buf;
	mtt_packet_t *pkt;
	struct mtt_component_obj *co;
	uint32_t params;
	int core;

	/* Test the kptrace master switch
	 * so that we can mute this probe
	 * quickly when stopping trace. */
	if (!mtt_comp_ctxt[0]->active_filter)
		return 0;

	core = raw_smp_processor_id();
	co = mtt_comp_ctxt[core];
	pkt = &mtt_pkts_ctxt[core];

	params = _PKT_GET_CMD(pkt);

	/* prepare size for a regular linux trace packet. */
	pkt->length = DEFAULT_PKT_LINUX;

	/*-----------------------preamble part--------------------------*/

	/* the first fields are pre-initialized.
	 * sync-version-target
	 * command-param
	 **/

	/* move to componentid/
	 */
	buf = pkt->u.buf + 2 * 4;
	mtt_pkt_put_comp_id(buf, co);

	/* move to trace ID/
	 */
	mtt_pkt_put_trace_id(buf, 0);

	/* write the type info
	 **/
	mtt_pkt_put_type_info(buf, MTT_TRACEITEM_INT32);

	/*-----------------------optional part--------------------------*/

	/*write the PID : CTXT is implied for this one! */
	*(uint32_t *) buf = old_c;
	buf += 4;

	/*write the time-stamp if needed */
	if (params & MTT_PARAM_TSTV) {
		struct timeval tv;
		do_gettimeofday(&tv);
		mtt_pkt_put_tstv(buf, tv);
		pkt->length += 8;
	}

	/* add payload size
	 * the rounding is to insure that a frame
	 * start is always aligned to a 32bits boundary.
	 **/
	mtt_pkt_update_align32(pkt->length, MTT_TRACEITEM_INT32);

	*(uint32_t *) buf = new_c;

	/*tell the output driver to lock if she needs to. */
	mtt_cur_out_drv->write_func(pkt, DRVLOCK);
	return 0;
}
#endif

/* Initialize payload for a given type
 * and set packet size param. */
uint32_t *mtt_pkt_get_with_hint(struct mtt_component_obj *co,
				mtt_packet_t *p,
				mtt_trace_item_t type_info, uint32_t loc)
{
	char *buf;
	uint32_t params;

	/* Prepare size for a regular linux trace packet. */
	p->length = DEFAULT_PKT_LINUX + 12;
	p->comp = (void *)co;

	params = _PKT_GET_CMD(p);
	(p)->u.preamble->command_param |= MTT_PARAM_MLOC;

	/*-----------------------preamble part--------------------------*/

	/* Move to componentid/
	 */
	buf = p->u.buf + 2 * 4;
	mtt_pkt_put_comp_id(buf, co);

	/* Move to trace ID/
	 */
	mtt_pkt_put_trace_id(buf, loc);

	/* Write the type info */
	mtt_pkt_put_type_info(buf, type_info);

	/*-----------------------optional part--------------------------*/

	/* Write the PID if needed */
	if (unlikely(!(params & MTT_PARAM_CTXT)))
		p->length -= 4;
	else
		mtt_pkt_put_ctxt(buf);

	/* Write the time-stamp if needed */
	if (params & MTT_PARAM_TSTV) {
		struct timeval tv;
		do_gettimeofday(&tv);
		mtt_pkt_put_tstv(buf, tv);
		p->length += 8;
	}

	/* Add payload size
	 * the rounding is to insure that a frame
	 * start is always aligned to a 32bits boundary.
	 **/
	mtt_pkt_update_align32(p->length, type_info);

	/* Handle level, but no. */
	if (unlikely(params & MTT_PARAM_LEVEL)) {
		mtt_pkt_put_level(buf, 0);
		p->length += 4;
	}

	/* Return offset of payload */
	return (uint32_t *) buf;
}

/* Initialize payload for a given type
 * and set packet size param.
 **/
uint32_t *mtt_pkt_get_raw(struct mtt_component_obj *co, mtt_packet_t *p,
			  size_t len, uint32_t loc)
{
	char *buf;

	/* Prepare size for a regular linux trace packet. */
	p->length = DEFAULT_PKT_LINUX;
	p->comp = (void *)co;

	/*-----------------------preamble part--------------------------*/

	/* The first fields are pre-initialized.
	 * sync-version-target
	 * command-param
	 **/

	/* Move to componentid */
	buf = p->u.buf + 2 * 4;
	mtt_pkt_put_comp_id(buf, co);

	/* Move to trace ID */
	mtt_pkt_put_trace_id(buf, loc);

	/* Write the type info */
	mtt_pkt_put_type_info(buf, len);

	/*-----------------------optional part--------------------------*/

	/* Write the PID if needed */
	if (unlikely(!(mtt_sys_config.params & MTT_PARAM_CTXT)))
		p->length -= 4;
	else
		mtt_pkt_put_ctxt(buf);

	/* Write the time-stamp if needed */
	if (mtt_sys_config.params & MTT_PARAM_TSTV) {
		struct timeval tv;
		do_gettimeofday(&tv);
		mtt_pkt_put_tstv(buf, tv);
		p->length += 8;
	}

	/* Add payload size
	 * the rounding is to insure that a frame
	 * start is always aligned to a 32bits boundary.
	 **/
	mtt_pkt_update_align32(p->length, len);

	/* Handle level, but no. */
	if (unlikely(mtt_sys_config.params & MTT_PARAM_LEVEL)) {
		mtt_pkt_put_level(buf, 0);
		p->length += 4;
	}

	/* Return offset of payload */
	return (uint32_t *) buf;
}
