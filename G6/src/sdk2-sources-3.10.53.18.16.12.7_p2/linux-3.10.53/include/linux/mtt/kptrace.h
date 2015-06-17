/*
 *  Multi-Target Trace solution
 *
 *  DRIVER/INTERNAL IMPLEMENTATION HEADER FOR KPTRACE PART

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
#ifndef _MTT_KPTRACE_H_
#define _MTT_KPTRACE_H_

#include <linux/mtt/mtt.h>

/* Predefined ones */
extern struct mtt_component_obj **mtt_comp_ctxt;
extern struct mtt_component_obj **mtt_comp_irqs;
extern mtt_packet_t *mtt_pkts_irqs;
extern mtt_packet_t *mtt_pkts_ctxt;

/* Internal APIs */
int mtt_cswitch(uint32_t old_c, uint32_t new_c);
int mtt_kptrace_ctxt(uint32_t prev);
uint32_t *mtt_kptrace_pkt_get(uint32_t type_info, mtt_packet_t **p,
			uint32_t loc);
int mtt_kptrace_pkt_put(mtt_packet_t *p);

/* KPTrace: builtin dynamic tracing support
 **/
int mtt_kptrace_init(struct bus_type *mtt_subsys);
void mtt_kptrace_cleanup(void);
int mtt_kptrace_start(void);
void mtt_kptrace_stop(void);
int mtt_kptrace_comp_alloc(void);

/* KPTrace defs */
struct kp_tracepoint {
	struct kprobe kp;
	struct kretprobe rp;
	struct kobject kobj;
	int enabled;
	int callstack;
	int inserted;
	int stopon;
	int starton;
	int user_tracepoint;
	int late_tracepoint;
	const struct file_operations *ops;
	struct list_head list;
};

struct kp_tracepoint_set {
	int enabled;
	struct kobject kobj;
	const struct file_operations *ops;
	struct list_head list;
};

struct kp_tracepoint *kptrace_create_tracepoint(
	struct kp_tracepoint_set *set,
	const char *name,
	int (*entry_handler)(struct kprobe *, struct pt_regs *),
	int (*return_handler)(struct kretprobe_instance *, struct pt_regs *)
);

/* Per-architecture hooks. */
extern void arch_init_syscall_logging(struct kp_tracepoint_set *set);
extern void arch_init_core_event_logging(struct kp_tracepoint_set *set);
extern void arch_init_memory_logging(struct kp_tracepoint_set *set);

int syscall_ihhh_pre_handler(struct kprobe *p, struct pt_regs *regs);
int syscall_iihh_pre_handler(struct kprobe *p, struct pt_regs *regs);
int syscall_hhhh_pre_handler(struct kprobe *p, struct pt_regs *regs);
int syscall_rp_handler(struct kretprobe_instance *ri, struct pt_regs *regs);
int syscall_pre_handler(struct kprobe *p, struct pt_regs *regs);

int alloc_pages_pre_handler(struct kprobe *p, struct pt_regs *regs);
int alloc_pages_rp_handler(struct kretprobe_instance *ri, struct pt_regs *regs);

#define CALL(x) kptrace_create_tracepoint(set, #x, syscall_hhhh_pre_handler, \
	syscall_rp_handler);

#define CALL_ABI(native, compat) kptrace_create_tracepoint(set, #native, \
	syscall_hhhh_pre_handler, \
	syscall_rp_handler);

#define CALL_CUSTOM_PRE(x, eh) kptrace_create_tracepoint(set, #x, eh, \
	syscall_rp_handler);

#define CALL_CUSTOM_PRE_ABI(native, compat, eh) \
	kptrace_create_tracepoint(set, #native, eh, syscall_rp_handler);

#endif /*_MTT_KPTRACE_H_*/
