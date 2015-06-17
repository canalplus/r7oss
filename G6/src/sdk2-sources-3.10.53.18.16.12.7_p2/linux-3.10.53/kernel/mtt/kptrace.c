/*
 *  Multi-Target Trace solution
 *
 *  MTT - KPROBES/DYNAMIC TRACING SUPPORT.
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
#include <linux/module.h>
#include <linux/kprobes.h>	/* CONFIG_MODULES required */
#include <linux/kallsyms.h>	/* CONFIG_KALLSYMS required */
#include <linux/prctl.h>
#include <linux/debugfs.h>
#include <linux/sysfs.h>
#include <linux/futex.h>
#include <linux/version.h>
#include <net/sock.h>
#include <asm/sections.h>
#include <linux/relay.h>

#include <linux/mtt/kptrace.h>
#include <asm/mtt-kptrace.h>

/* Predefined components a bit crapy way to allocate, but ok for now
 **/
struct mtt_component_obj **mtt_comp_ctxt;
struct mtt_component_obj **mtt_comp_irqs;

enum kptrace_tracepoint_states {
	TP_UNUSED,
	TP_USED,
	TP_INUSE
};

static struct bus_type *mtt_subsys;

static LIST_HEAD(tracepoint_sets);
static LIST_HEAD(tracepoints);

static struct kp_tracepoint_set *user_set;
static int user_stopon;
static int user_starton;

/* forward declarations */
#define MAX_SYMBOL_SIZE 512	/*fixme */
#define MAX_STRING_SIZE 512

static char user_new_symbol[MAX_SYMBOL_SIZE];
static int user_pre_handler(struct kprobe *p, struct pt_regs *regs);
static int user_rp_handler(struct kretprobe_instance *ri, struct pt_regs *regs);

/* forward decl kprobe template handlers
 **/
static int _gen_pre4_handler(struct kprobe *p, struct pt_regs *regs,
			     uint32_t key);
static int _gen_pre2_handler(struct kprobe *p, struct pt_regs *regs,
			     uint32_t key);
static int _gen_pre1_handler(struct kprobe *p, struct pt_regs *regs,
			     uint32_t key);
static int _gen_ret_handler(struct kretprobe_instance *ri, struct pt_regs *regs,
			    uint32_t key);

/*generic with key only
 **/
#define _gen_0_handler(KEY_VAL) \
{\
	mtt_packet_t *pkt;\
	uint32_t *data;\
	if (unlikely(!mtt_comp_ctxt[0]->active_filter))\
		return 0;\
	data =\
		mtt_kptrace_pkt_get(MTT_TRACEITEM_PTR32, &pkt,\
		      (uint32_t)regs->REG_EIP);\
	*data = KEY_VAL;\
	return mtt_kptrace_pkt_put(pkt);\
}

static int _gen_sem_count_handler(struct kprobe *p, struct pt_regs *regs,
				  uint32_t key);
static int _gen_sem_activity_handler(struct kprobe *p, struct pt_regs *regs,
				     uint32_t key);

/* kptrace-optimized mtt trace builder
 **/

static struct attribute *tracepoint_attribs[] = {
	&(struct attribute){
			    .name = "enabled",
			    .mode = S_IRUGO | S_IWUSR,
			    },
	&(struct attribute){
			    .name = "callstack",
			    .mode = S_IRUGO | S_IWUSR,
			    },
	NULL
};

static struct attribute *tracepoint_set_attribs[] = {
	&(struct attribute){
			    .name = "enabled",
			    .mode = S_IRUGO | S_IWUSR,
			    },
	NULL
};

static struct attribute *user_tp_attribs[] = {
	&(struct attribute){
			    .name = "new_symbol",
			    .mode = S_IRUGO | S_IWUSR,
			    },
	&(struct attribute){
			    .name = "add",
			    .mode = S_IRUGO | S_IWUSR,
			    },
	&(struct attribute){
			    .name = "enabled",
			    .mode = S_IRUGO | S_IWUSR,
			    },
	&(struct attribute){
			    .name = "stopon",
			    .mode = S_IRUGO | S_IWUSR,
			    },
	&(struct attribute){
			    .name = "starton",
			    .mode = S_IRUGO | S_IWUSR,
			    },
	NULL
};

static ssize_t
tracepoint_set_show_attrs(struct kobject *kobj,
			  struct attribute *attr, char *buffer)
{
	struct kp_tracepoint_set *set = container_of(kobj,
						     struct kp_tracepoint_set,
						     kobj);
	if (strcmp(attr->name, "enabled") == 0) {
		if (set->enabled) {
			snprintf(buffer, PAGE_SIZE,
				 "Tracepoint set \"%s\" is enabled\n",
				 kobj->name);
		} else {
			snprintf(buffer, PAGE_SIZE,
				 "Tracepoint set \"%s\" is disabled\n",
				 kobj->name);
		}
	}
	return strlen(buffer);
}

static ssize_t
tracepoint_set_store_attrs(struct kobject *kobj,
			   struct attribute *attr, const char *buffer,
			   size_t size)
{
	struct kp_tracepoint_set *set = container_of(kobj,
						     struct kp_tracepoint_set,
						     kobj);
	if (strcmp(attr->name, "enabled") == 0) {
		if (strncmp(buffer, "1", 1) == 0)
			set->enabled = 1;
		else
			set->enabled = 0;
	}
	return size;
}

static ssize_t
tracepoint_show_attrs(struct kobject *kobj,
		      struct attribute *attr, char *buffer)
{
	struct kp_tracepoint *tp = container_of(kobj,
						struct kp_tracepoint, kobj);
	if (strcmp(attr->name, "enabled") == 0) {
		if (tp->enabled == 1) {
			snprintf(buffer, PAGE_SIZE,
				 "Tracepoint on %s is enabled\n", kobj->name);
		} else {
			snprintf(buffer, PAGE_SIZE,
				 "Tracepoint on %s is disabled\n", kobj->name);
		}
	}

	if (strcmp(attr->name, "callstack") == 0) {
		if (tp->callstack == 1) {
			snprintf(buffer, PAGE_SIZE,
				 "Callstack gathering on %s is enabled\n",
				 kobj->name);
		} else {
			snprintf(buffer, PAGE_SIZE,
				 "Callstack gathering on %s is disabled\n",
				 kobj->name);
		}
	}

	return strlen(buffer);
}

static ssize_t
tracepoint_store_attrs(struct kobject *kobj,
		       struct attribute *attr, const char *buffer, size_t size)
{
	struct kp_tracepoint *tp = container_of(kobj,
						struct kp_tracepoint, kobj);

	if (strcmp(attr->name, "enabled") == 0) {
		if (strncmp(buffer, "1", 1) == 0)
			tp->enabled = 1;
		else
			tp->enabled = 0;
	}

	if (strcmp(attr->name, "callstack") == 0) {
		if (strncmp(buffer, "1", 1) == 0)
			tp->callstack = 1;
		else
			tp->callstack = 0;
	}

	return size;
}

static ssize_t
user_show_attrs(struct kobject *kobj, struct attribute *attr, char *buffer)
{
	if (strcmp(attr->name, "new_symbol") == 0) {
		return snprintf(buffer, PAGE_SIZE, "new_symbol = %s\n",
				user_new_symbol);
	}

	if (strcmp(attr->name, "add") == 0) {
		return snprintf(buffer, PAGE_SIZE, "Adding new tracepoint %s\n",
				user_new_symbol);
	}

	if (strcmp(attr->name, "enabled") == 0) {
		if (user_set->enabled) {
			return snprintf(buffer, PAGE_SIZE,
					"User-defined tracepoints are enabled");
		} else {
			return snprintf(buffer, PAGE_SIZE,
				"User-defined tracepoints are disabled");
		}
	}

	if (strcmp(attr->name, "stopon") == 0) {
		if (user_stopon) {
			return snprintf(buffer, PAGE_SIZE,
					"Stop logging on this tracepoint: on");
		} else {
			return snprintf(buffer, PAGE_SIZE,
					"Stop logging on this tracepoint: off");
		}
	}

	if (strcmp(attr->name, "starton") == 0) {
		if (user_stopon) {
			return snprintf(buffer, PAGE_SIZE,
					"Start logging on this tracepoint: on");
		} else {
			return snprintf(buffer, PAGE_SIZE,
				"Start logging on this tracepoint: off");
		}
	}

	return snprintf(buffer, PAGE_SIZE, "Unknown attribute\n");
}

static ssize_t
user_store_attrs(struct kobject *kobj, struct attribute *attr,
		 const char *buffer, size_t size)
{
	struct list_head *p;
	struct kp_tracepoint *tp, *new_tp = NULL;

	if (strcmp(attr->name, "new_symbol") == 0)
		strncpy(user_new_symbol, buffer, MAX_SYMBOL_SIZE);

	if (strcmp(attr->name, "add") == 0) {
		/* Check it doesn't already exist, to avoid duplicates */
		list_for_each(p, &tracepoints) {
			tp = list_entry(p, struct kp_tracepoint, list);
			if (tp != NULL && tp->user_tracepoint == 1) {
				if (strncmp
				    (kobject_name(&tp->kobj), user_new_symbol,
				     MAX_SYMBOL_SIZE) == 0)
					return size;
			}
		}

		new_tp = kptrace_create_tracepoint(user_set, user_new_symbol,
						   &user_pre_handler,
						   &user_rp_handler);

		if (!new_tp) {
			printk(KERN_ERR "kptrace: Cannot create tracepoint\n");
			return -ENOSYS;
		} else {
			new_tp->stopon = user_stopon;
			new_tp->starton = user_starton;
			new_tp->user_tracepoint = 1;
			return size;
		}
	}

	if (strcmp(attr->name, "enabled") == 0) {
		if (strncmp(buffer, "1", 1) == 0)
			user_set->enabled = 1;
		else
			user_set->enabled = 0;

	}

	if (strcmp(attr->name, "stopon") == 0) {
		if (strncmp(buffer, "1", 1) == 0)
			user_stopon = 1;
		else
			user_stopon = 0;

	}

	if (strcmp(attr->name, "starton") == 0) {
		if (strncmp(buffer, "1", 1) == 0)
			user_starton = 1;
		else
			user_starton = 0;

	}

	return size;
}

/* Operations for the three kobj types */
static const struct sysfs_ops tracepoint_sysfs_ops = {
	&tracepoint_show_attrs, &tracepoint_store_attrs
};

static const struct sysfs_ops tracepoint_set_sysfs_ops = {
	&tracepoint_set_show_attrs, &tracepoint_set_store_attrs
};
static const struct sysfs_ops user_sysfs_ops = {
	&user_show_attrs,
	&user_store_attrs
};

/* Three kobj types: tracepoints, tracepoint sets,
 * the special "user" tracepoint set
 **/
static struct kobj_type kp_tracepointype = {
	NULL,
	&tracepoint_sysfs_ops,
	tracepoint_attribs
};

static struct kobj_type kp_tracepoint_setype = {
	NULL,
	&tracepoint_set_sysfs_ops,
	tracepoint_set_attribs
};

static struct kobj_type user_type = {
	NULL,
	&user_sysfs_ops,
	user_tp_attribs
};

static struct kp_tracepoint *__create_tracepoint(
				struct kp_tracepoint_set *set,
				const char *name,
				int (*entry_handler) (struct
						       kprobe *,
						       struct
						       pt_regs
						       *),
				int (*return_handler) (struct
							kretprobe_instance
							*,
							struct
							pt_regs
							*),
				int late_tracepoint,
				const char *alias)
{
	struct kp_tracepoint *tp;
	tp = kzalloc(sizeof(*tp), GFP_KERNEL);
	if (!tp) {
		printk(KERN_WARNING
		       "kptrace: Failed to allocate memory for tracepoint %s\n",
		       name);
		return NULL;
	}

	tp->enabled = 0;
	tp->callstack = 0;
	tp->stopon = 0;
	tp->starton = 0;
	tp->user_tracepoint = 0;
	tp->late_tracepoint = late_tracepoint;
	tp->inserted = TP_UNUSED;

	/* The 'alias' is the tracepoint name exposed via sysfs. By default, it
	 * is the symbol name */
	if (!alias)
		alias = name;

	if (entry_handler != NULL) {
		tp->kp.addr = (kprobe_opcode_t *) kallsyms_lookup_name(name);

		if (tp->late_tracepoint == 1)
			tp->kp.flags |= KPROBE_FLAG_DISABLED;

		if (!tp->kp.addr) {
			printk(KERN_WARNING "kptrace: Symbol %s not found\n",
			       name);
			kfree(tp);
			return NULL;
		}
		tp->kp.pre_handler = entry_handler;
	}

	if (return_handler != NULL) {
		if (entry_handler != NULL)
			tp->rp.kp.addr = tp->kp.addr;
		else
			tp->rp.kp.addr =
			    (kprobe_opcode_t *) kallsyms_lookup_name(name);

		tp->rp.handler = return_handler;
		tp->rp.maxactive = 128;
	}

	list_add(&tp->list, &tracepoints);

	if (kobject_init_and_add(&tp->kobj, &kp_tracepointype, &set->kobj,
				 alias) < 0) {
		printk(KERN_WARNING "kptrace: Failed add to add kobject %s\n",
		       name);
		return NULL;
	}

	return tp;
}

/*
 * Creates a tracepoint in the given set. Pointers to entry and/or return
 * handlers can be NULL if it is not necessary to track those events.
 *
 * This function only initializes the data structures and adds the sysfs node.
 * To actually add the kprobes and start tracing, use insert_tracepoint().
 */
struct kp_tracepoint *kptrace_create_tracepoint(
				struct kp_tracepoint_set *set,
				const char *name,
				int (*entry_handler) (struct
						      kprobe *,
						      struct
						      pt_regs
						      *),
				int (*return_handler) (struct
						       kretprobe_instance
						       *,
						       struct
						       pt_regs
						       *))
{
	return __create_tracepoint(set, name, entry_handler,
				   return_handler, 0, NULL);
}

/*
 * As kptrace_create_tracepoint(), except that is exposed in sysfs with the
 * name "alias", rather than its symbol name. This is useful to allow a common
 * sysfs entry between kernel versions where the actual symbols have changed,
 * for example kthread_create() changed from a function to a macro.
 */
struct kp_tracepoint *kptrace_create_aliased_tracepoint(struct kp_tracepoint_set
					*set, const char *name,
					int (*entry_handler)
					 (struct kprobe *,
					  struct pt_regs *),
					int (*return_handler)
					 (struct
					  kretprobe_instance *,
					  struct pt_regs *),
					const char *alias)
{
	return __create_tracepoint(set, name, entry_handler,
				   return_handler, 0, alias);
}

/*
 * As create_tracepoint(), except that the tracepoint is not armed until all
 * tracepoints have been added. This is useful when tracing a function used
 * in the kprobe code, such as mutex_lock().
 */
static struct kp_tracepoint *kptrace_create_late_tracepoint(struct
					    kp_tracepoint_set
					    *set,
					    const char *name,
					    int (*entry_handler)
					     (struct kprobe *,
					      struct pt_regs *),
					    int
					     (*return_handler)
					     (struct
					      kretprobe_instance
					      *,
					      struct pt_regs *))
{
	return __create_tracepoint(set, name, entry_handler,
				   return_handler, 1, NULL);
}

/*
 * Combines the functionality of kptrace_create_aliased_tracepoint() and
 * kptrace_create_late_tracepoint().
 */
struct kp_tracepoint *kptrace_create_late_aliased_tracepoint(struct
					     kp_tracepoint_set
					     *set,
					     const char *name,
					     int
					      (*entry_handler)
					      (struct kprobe *,
					       struct pt_regs
					       *), int
					      (*return_handler)
					      (struct
					       kretprobe_instance
					       *,
					       struct pt_regs
					       *),
					     const char *alias)
{
	return __create_tracepoint(set, name, entry_handler,
				   return_handler, 1, alias);
}

/*
 * Registers the kprobes for the tracepoint, so that it will start to
 * be logged.
 *
 * kretprobes are only registered the first time. After that, we only
 * register and unregister the initial kprobe. This prevents race
 * conditions where a function is halfway through execution when the
 * probe is removed.
 */
static int insert_tracepoint(struct kp_tracepoint *tp)
{
	int r = 0;
	if (tp->inserted != TP_INUSE) {
		if (tp->kp.addr != NULL)
			r += register_kprobe(&tp->kp);

		if (tp->rp.kp.addr != NULL) {
			if (tp->inserted == TP_UNUSED)
				r += register_kretprobe(&tp->rp);
			else if (tp->inserted == TP_USED)
				r += register_kprobe(&tp->rp.kp);
		}
		tp->inserted = TP_INUSE;
	}
	return r;
}

/* Insert all enabled tracepoints in this set */
static int insert_tracepoints_in_set(struct kp_tracepoint_set *set)
{
	struct list_head *p;
	struct kp_tracepoint *tp;
	int r = 0;

	list_for_each(p, &tracepoints) {
		tp = list_entry(p, struct kp_tracepoint, list);
		if (tp->kobj.parent) {
			if ((strcmp(tp->kobj.parent->name, set->kobj.name) == 0)
			    && (tp->enabled == 1))
				r += insert_tracepoint(tp);
		}
	}
	return r;
}

/*
 * Unregister the kprobes for the tracepoint. From kretprobes,
 * only unregister the initial kprobe to prevent race condition
 * when function is halfway through execution when the probe is
 * removed.
 */
int unregister_tracepoint(struct kp_tracepoint *tp)
{
	if (tp->kp.addr != NULL) {
		if (tp->late_tracepoint)
			arch_disarm_kprobe(&tp->kp);
		unregister_kprobe(&tp->kp);
/* Remove the disabled flag, or registration
* fails the second time.
*/
		tp->kp.flags &= ~KPROBE_FLAG_DISABLED;
	}

	if (tp->rp.kp.addr != NULL) {
		unregister_kprobe(&tp->rp.kp);
		tp->rp.kp.flags &= ~KPROBE_FLAG_DISABLED;
	}

	tp->inserted = TP_USED;
	return 0;
}

/*
 * Allocates the data structures for a new tracepoint set and
 * creates a sysfs node for it.
 */
static struct kp_tracepoint_set *create_tracepoint_set(const char *name)
{
	struct kp_tracepoint_set *set;
	set = kzalloc(sizeof(*set), GFP_KERNEL);
	if (!set)
		return NULL;

	list_add(&set->list, &tracepoint_sets);

	if (kobject_init_and_add(&set->kobj, &kp_tracepoint_setype,
				 &(mtt_subsys->dev_root->kobj), name) < 0)
		printk(KERN_WARNING "kptrace: Failed to add kobject %s\n",
		       name);

	set->enabled = 0;

	return set;
}

/* Inserts all the tracepoints in each enabled set */
int mtt_kptrace_start(void)
{
	struct list_head *p, *tmp;
	struct kp_tracepoint_set *set;
	struct kp_tracepoint *tp;
	int r = 0;

	mtt_printk(KERN_DEBUG "mtt_kptrace_start: enabling tracepoints.\n");

	list_for_each(p, &tracepoint_sets) {
		set = list_entry(p, struct kp_tracepoint_set, list);
		if (set->enabled)
			r = insert_tracepoints_in_set(set);

	}

	if (r) {
		mtt_printk(KERN_DEBUG "mtt_kptrace_start: failed.\n");
		return -EINVAL;
	}

	mtt_printk(KERN_DEBUG "mtt_kptrace_start: arming late probes.\n");

	/* Arm any "late" tracepoints */
	list_for_each_safe(p, tmp, &tracepoints) {
		tp = list_entry(p, struct kp_tracepoint, list);
		if (tp->late_tracepoint && tp->enabled) {
			if (kprobe_disabled(&tp->kp))
				arch_arm_kprobe(&tp->kp);
		}
	}
	return 0;
}

/* Remove all tracepoints */
void mtt_kptrace_stop(void)
{
	struct list_head *p, *tmp;
	struct kp_tracepoint *tp;

	mtt_printk(KERN_DEBUG "mtt_kptrace_stop.\n");

	/* Paranoia: Mute the trace points */
	mtt_comp_ctxt[0]->active_filter = 0;

	list_for_each_safe(p, tmp, &tracepoints) {
		tp = list_entry(p, struct kp_tracepoint, list);

		if (tp->inserted == TP_INUSE)
			unregister_tracepoint(tp);

		if (tp->user_tracepoint == 1) {
			kobject_put(&tp->kobj);
			tp->kp.addr = NULL;
			list_del(p);
		} else {
			tp->enabled = 0;
		}
	}
}

int __init mtt_kptrace_comp_alloc(void)
{
	int core;
	int err = 0;

	/* We make MTT_COMPID_LIN_CTXT the "master" component for
	 * kptrace: MTT_COMPID_LIN_CTXT.level is checked to know
	 * whether kptrace is enabled of not.
	 *
	 * component for context messages,
	 * in peculiar 'C' that are very frequent
	 */
	char name_irqs[] = "mtt_irqs0";
	char name_ctxt[] = "mtt_ctxt0";

	/* Allocation the component descriptors */
	mtt_comp_irqs =
		kmalloc(sizeof(struct mtt_component_obj) * num_possible_cpus(),
			GFP_KERNEL);

	mtt_comp_ctxt =
		kmalloc(sizeof(struct mtt_component_obj) * num_possible_cpus(),
			GFP_KERNEL);

	if (mtt_comp_ctxt && mtt_comp_irqs)
		for (core = 0; (core < num_possible_cpus()) && !err; core++) {
			name_irqs[sizeof(name_irqs) - 2] = 0x30 + core;
			name_ctxt[sizeof(name_ctxt) - 2] = 0x30 + core;
			mtt_comp_irqs[core] =
				mtt_component_alloc(MTT_COMPID_LIN_IRQ
						+ MTT_COMPID_MAOFFSET * core,
							name_irqs, 0);
			mtt_comp_ctxt[core] =
				mtt_component_alloc(MTT_COMPID_LIN_CTXT
						+ MTT_COMPID_MAOFFSET * core,
							name_ctxt, 0);
			if (!mtt_comp_irqs[core] || !mtt_comp_ctxt[core])
				err = 1;
			else {
				/* Cptimization */
				mtt_pkts_ctxt[core].comp = mtt_comp_ctxt[core];
				mtt_pkts_irqs[core].comp = mtt_comp_irqs[core];
			}
		}

	/* Cleanup is done by remove_component_tree if this failed. */
	if (!err) {
		/* Unlike other components, kptrace is not following
		 * the global filter, we `individualize` it by setting
		 * the level to a different value than ALL.
		 * The "master" component to enable/disable is ctxt[0].*/
		mtt_comp_ctxt[0]->filter = MTT_LEVEL_DEBUG;

		/* Like other components the active level is checked in
		 * kprobes handlers: muting the linux core will also mute
		 * kptrace. */
		mtt_comp_ctxt[0]->active_filter = MTT_LEVEL_NONE;
		return 0;
	}

	kfree(mtt_comp_irqs);
	kfree(mtt_comp_ctxt);

	/* If a mtt_component_alloc failed the calling code will cleanup
	 * whatever we succeeded to chain in the comp list, and needs
	 * to be cleaned. */
	return err;
}

/* LEGACY KPTRACE API */

/*
 * Legacy printf-like routine.
 */
void kptrace_write_record(const char *buf)
{
	mtt_packet_t *pkt;
	uint32_t type_info = 0;
	uint32_t size;
	uint32_t *data;

	if (unlikely(!mtt_comp_ctxt[0]->active_filter))
		return;

	data = mtt_kptrace_pkt_get(type_info, &pkt,
				   (uint32_t) __builtin_return_address(0));

	data[0] = KEY_K | KEY_ARGS(0);

	size = strnlen(buf, (MAX_STRING_SIZE - 8));
	memcpy((char *)(&data[1]), buf, size);

	/* Add arguments size to string size */
	size += 4 * 1;

	mtt_pkt_update_align32(pkt->length, size);

	/* Add type encoding. */
	type_info = MTT_TRACEITEM_BLOB(size);

	pkt->u.preamble->type_info = type_info;

	mtt_kptrace_pkt_put(pkt);
}
EXPORT_SYMBOL(kptrace_write_record);

void kpprintf(char *fmt, ...)
{
	va_list ap;
	mtt_packet_t *pkt;
	uint32_t type_info = 0;
	uint32_t size;
	uint32_t *data;

	if (unlikely(!mtt_comp_ctxt[0]->active_filter))
		return;

	data = mtt_kptrace_pkt_get(type_info, &pkt,
				   (uint32_t) __builtin_return_address(0));

	data[0] = KEY_K | KEY_ARGS(0);

	/* build the string in the packet. */
	va_start(ap, fmt);
	size = 1 + vsprintf((char *)(&data[1]), fmt, ap);
	va_end(ap);

	/* Add arguments size to string size */
	size += 4 * 1;

	mtt_pkt_update_align32(pkt->length, size);

	/* Add type encoding. */
	type_info = MTT_TRACEITEM_BLOB(size);

	pkt->u.preamble->type_info = type_info;

	mtt_kptrace_pkt_put(pkt);
}
EXPORT_SYMBOL(kpprintf);

/*
 * Indicates that this is an interesting point in the code.
 * Intended to be highlighted prominantly in a GUI.
 */
void kptrace_mark(void)
{
	mtt_packet_t *pkt;
	uint32_t *data;

	if (unlikely(!mtt_comp_ctxt[0]->active_filter))
		return;

	data = mtt_kptrace_pkt_get(MTT_TRACEITEM_PTR32, &pkt,
				(uint32_t) __builtin_return_address(0));

	*data = KEY_KM;
	mtt_kptrace_pkt_put(pkt);
}
EXPORT_SYMBOL(kptrace_mark);

/*
 * Stops the logging of trace records until kptrace_restart()
 * is called.
 */
void kptrace_pause(void)
{
	mtt_packet_t *pkt;
	uint32_t *data = mtt_kptrace_pkt_get(MTT_TRACEITEM_PTR32, &pkt, 0);
	*data = KEY_KP;

	mtt_comp_ctxt[0]->active_filter = MTT_LEVEL_NONE;

	mtt_kptrace_pkt_put(pkt);
}
EXPORT_SYMBOL(kptrace_pause);

/*
 * Restarts logging of trace after a kptrace_pause()
 */
void kptrace_restart(void)
{
	mtt_packet_t *pkt;
	uint32_t *data = mtt_kptrace_pkt_get(MTT_TRACEITEM_PTR32, &pkt, 0);
	*data = KEY_KR;

	mtt_comp_ctxt[0]->active_filter = MTT_LEVEL_DEBUG;

	mtt_kptrace_pkt_put(pkt);
}
EXPORT_SYMBOL(kptrace_restart);

static int
vmalloc_rp_handler(struct kretprobe_instance *ri, struct pt_regs *regs)
{
	return _gen_ret_handler(ri, regs, KEY_Mv | KEY_ARGI(0));
}

static int
get_free_pages_rp_handler(struct kretprobe_instance *ri, struct pt_regs *regs)
{
	return _gen_ret_handler(ri, regs, KEY_Mg | KEY_ARGI(0));
}

int alloc_pages_rp_handler(struct kretprobe_instance *ri, struct pt_regs *regs)
{
	return _gen_ret_handler(ri, regs, KEY_Ma | KEY_ARGI(0));
}

static int
kmem_cache_alloc_rp_handler(struct kretprobe_instance *ri, struct pt_regs *regs)
{
	return _gen_ret_handler(ri, regs, KEY_Ms | KEY_ARGI(0));
}

static int user_rp_handler(struct kretprobe_instance *ri, struct pt_regs *regs)
{
	return _gen_ret_handler(ri, regs, KEY_u | KEY_ARGI(0));
}

int syscall_rp_handler(struct kretprobe_instance *ri, struct pt_regs *regs)
{
	return _gen_ret_handler(ri, regs, KEY_X | KEY_ARGI(0));
}

static int
kmalloc_rp_handler(struct kretprobe_instance *ri, struct pt_regs *regs)
{
	return _gen_ret_handler(ri, regs, KEY_Mm | KEY_ARGI(0));
}

static int
down_trylock_rp_handler(struct kretprobe_instance *ri, struct pt_regs *regs)
{
	return _gen_ret_handler(ri, regs, KEY_Zt | KEY_ARGI(0));
}

static int
down_read_trylock_rp_handler(struct kretprobe_instance *ri,
			     struct pt_regs *regs)
{
	return _gen_ret_handler(ri, regs, KEY_Za | KEY_ARGI(0));
}

static int
down_write_trylock_rp_handler(struct kretprobe_instance *ri,
			      struct pt_regs *regs)
{
	return _gen_ret_handler(ri, regs, KEY_Zb | KEY_ARGI(0));
}

static int
bigphysarea_alloc_pages_rp_handler(struct kretprobe_instance *ri,
				   struct pt_regs *regs)
{
	return _gen_ret_handler(ri, regs, KEY_Mb | KEY_ARGH(0));
}

static int
bpa2_alloc_pages_rp_handler(struct kretprobe_instance *ri, struct pt_regs *regs)
{
	return _gen_ret_handler(ri, regs, KEY_Mh | KEY_ARGH(0));
}

static int
netif_receive_skb_rp_handler(struct kretprobe_instance *ri,
			     struct pt_regs *regs)
{
	return _gen_ret_handler(ri, regs, KEY_Nr | KEY_ARGI(0));
}

static int
dev_queue_xmit_rp_handler(struct kretprobe_instance *ri, struct pt_regs *regs)
{
	return _gen_ret_handler(ri, regs, KEY_Nx | KEY_ARGI(0));
}

static int
sock_sendmsg_rp_handler(struct kretprobe_instance *ri, struct pt_regs *regs)
{
	return _gen_ret_handler(ri, regs, KEY_Ss | KEY_ARGI(0));
}

static int
do_setitimer_rp_handler(struct kretprobe_instance *ri, struct pt_regs *regs)
{
	return _gen_ret_handler(ri, regs, KEY_Is | KEY_ARGI(0));
}

static int
it_real_fn_rp_handler(struct kretprobe_instance *ri, struct pt_regs *regs)
{
	return _gen_ret_handler(ri, regs, KEY_Ie | KEY_ARGI(0));
}

static int
sock_recvmsg_rp_handler(struct kretprobe_instance *ri, struct pt_regs *regs)
{
	return _gen_ret_handler(ri, regs, KEY_Sr | KEY_ARGI(0));
}

/*generic rp handler*/
static int _gen_ret_handler(struct kretprobe_instance *ri, struct pt_regs *regs,
			    uint32_t key)
{
	mtt_packet_t *pkt;
	uint32_t type_info =
	    MTT_TYPE(MTT_TRACEITEM_INT32, MTT_TYPEP_VECTOR, sizeof(uint32_t),
		     2);
	uint32_t *data;

	if (!mtt_comp_ctxt[0]->active_filter)
		return 0;

	data = mtt_kptrace_pkt_get(type_info, &pkt,
				   (uint32_t) (ri->rp->kp.addr));

	data[0] = key;
	data[1] = (int)regs->REG_RET;

	return mtt_kptrace_pkt_put(pkt);
}

static int vfree_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
	mtt_packet_t *pkt;
	uint32_t *data;
	uint32_t type_info =
	    MTT_TYPE(MTT_TRACEITEM_UINT32, MTT_TYPEP_VECTOR, sizeof(uint32_t),
		     2);

	if (unlikely(!mtt_comp_ctxt[0]->active_filter))
		return 0;

	data = mtt_kptrace_pkt_get(type_info, &pkt, (uint32_t) regs->REG_EIP);

	data[0] = KEY_MQ | KEY_ARGH(0);
	data[1] = (int)regs->REG_RET;

	return mtt_kptrace_pkt_put(pkt);
}

static int wake_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
	mtt_packet_t *pkt;
	uint32_t *data;
	uint32_t type_info =
	    MTT_TYPE(MTT_TRACEITEM_INT32, MTT_TYPEP_VECTOR, sizeof(uint32_t),
		     2);
	uint32_t old_params = mtt_sys_config.params;

	if (unlikely(!mtt_comp_ctxt[0]->active_filter))
		return 0;

	data = mtt_kptrace_pkt_get(type_info, &pkt, (uint32_t) regs->REG_EIP);

	/* If we try and put a timestamp on this, we'll cause a deadlock */
	mtt_sys_config.params &= ~(MTT_PARAM_TSTV | MTT_PARAM_TS64);

	data[0] = KEY_W | KEY_ARGI(0);
	data[1] = ((struct task_struct *)regs->REG_ARG0)->pid;

	mtt_sys_config.params = old_params;
	return mtt_kptrace_pkt_put(pkt);
}

static int
softirq_rp_handler(struct kretprobe_instance *ri, struct pt_regs *regs)
{
	_gen_0_handler(KEY_s);
}

static int ipi_rp_handler(struct kretprobe_instance *ri, struct pt_regs *regs)
{
	long core;
	mtt_packet_t *pkt;
	uint32_t *data;

	if (likely(mtt_comp_ctxt[0]->active_filter)) {
		core = raw_smp_processor_id();

		/* allocate or retrieve a preallocated TRACE frame */
		pkt = &mtt_pkts_irqs[core];

		data = (uint32_t *)mtt_pkt_get(mtt_comp_irqs[core], pkt,
			MTT_TRACEITEM_UINT32, (uint32_t)regs->REG_EIP);

		*data = KEY_ip;

		/*tell the output driver to lock if she needs to.*/
		mtt_cur_out_drv->write_func(pkt, DRVLOCK);
	}
	return 0;
}

/* short pass IRQ message output handler,
 * on dedicated channel
 **/
#define def_irq_any_handler(EVENT_KEY)\
	long core;\
	mtt_packet_t *pkt;\
	uint32_t *data;\
\
	if (likely(mtt_comp_ctxt[0]->active_filter)) {\
		core = raw_smp_processor_id();\
\
		/* allocate or retrieve a preallocated TRACE frame */\
		pkt = &mtt_pkts_irqs[core];\
\
		/* the irq number is in traceid in that peculiar case.*/\
		data = (uint32_t *)mtt_pkt_get(mtt_comp_irqs[core], pkt,\
			MTT_TRACEITEM_UINT32, (int)regs->REG_ARG0);\
\
		data[0] = EVENT_KEY;\
\
		/*tell the output driver to lock if she needs to.*/\
		mtt_cur_out_drv->write_func(pkt, DRVLOCK);\
	} \
	return 0;\

static int irq_KEY_Ix_handler(struct kretprobe_instance *ri,
				struct pt_regs *regs)
{
	def_irq_any_handler(KEY_Ix);
}

static int irq_KEY_i_handler(struct kretprobe_instance *ri,
				struct pt_regs *regs)
{
	def_irq_any_handler(KEY_i);
}

/* ================ ENTRY HANDLERS =============*/

static int irq_KEY_I_handler(struct kprobe *kp, struct pt_regs *regs)
{
	def_irq_any_handler(KEY_I);
}

static int softirq_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
	_gen_0_handler(KEY_S);
}

static int ipi_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
	long core;
	mtt_packet_t *pkt;
	uint32_t *data;

	if (likely(mtt_comp_ctxt[0]->active_filter)) {
		core = raw_smp_processor_id();

		/* ARM is not using optimized kprobes yet, so this is executed
		 * with irqs disabled. We assume this cannot be nested with
		 * another irq-type kprobe handler. Hence we just pick a packet
		 * in an array.
		 */
		pkt = &mtt_pkts_irqs[core];

		data = (uint32_t *)mtt_pkt_get(mtt_comp_irqs[core], pkt,
			MTT_TRACEITEM_UINT32, (uint32_t)regs->REG_EIP);

		*data = KEY_IP;

		/*tell the output driver to lock if she needs to.*/
		mtt_cur_out_drv->write_func(pkt, DRVLOCK);
	}
	return 0;
}

static int exit_thread_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
	_gen_0_handler(KEY_KX);
}

static int netif_receive_skb_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
	_gen_0_handler(KEY_NR);
}

static int dev_queue_xmit_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
	_gen_0_handler(KEY_NX);
}

static int kthread_create_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
	return _gen_pre1_handler(p, regs, KEY_KC | KEY_ARGH(0));
}

static int kernel_thread_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
	return _gen_pre1_handler(p, regs, KEY_KT | KEY_ARGH(0));
}

static int kfree_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
	return _gen_pre1_handler(p, regs, KEY_MF | KEY_ARGH(0));
}

static int vmalloc_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
	return _gen_pre1_handler(p, regs, KEY_MV | KEY_ARGH(0));
}

static int it_real_fn_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
	return _gen_pre1_handler(p, regs, KEY_IE | KEY_ARGH(0));
}

static int mutex_lock_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
	return _gen_pre1_handler(p, regs, KEY_ZM | KEY_ARGH(0));
}

static int mutex_unlock_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
	return _gen_pre1_handler(p, regs, KEY_Zm | KEY_ARGH(0));
}

static int underscore_up_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
	return _gen_pre1_handler(p, regs, KEY_Zu | KEY_ARGH(0));
}

static int
bigphysarea_free_pages_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
	return _gen_pre1_handler(p, regs, KEY_MC | KEY_ARGH(0));
}

/*generic syscall entry handler*/
static int _gen_pre1_handler(struct kprobe *p, struct pt_regs *regs,
			     uint32_t key)
{
	mtt_packet_t *pkt;
	uint32_t *data;
	uint32_t type_info =
	    MTT_TYPE(MTT_TRACEITEM_UINT32, MTT_TYPEP_VECTOR, sizeof(uint32_t),
		     2);

	if (unlikely(!mtt_comp_ctxt[0]->active_filter))
		return 0;

	data = mtt_kptrace_pkt_get(type_info, &pkt, (uint32_t) regs->REG_EIP);

	data[0] = key;
	data[1] = (int)regs->REG_ARG0;

	return mtt_kptrace_pkt_put(pkt);
}

static int
kthread_create_rp_handler(struct kretprobe_instance *ri, struct pt_regs *regs)
{
	mtt_packet_t *pkt;
	uint32_t type_info = 0;
	uint32_t size;
	uint32_t *data;
	struct task_struct *new_task = (struct task_struct *)regs->REG_RET;

	if (unlikely(!mtt_comp_ctxt[0]->active_filter))
		return 0;

	data = mtt_kptrace_pkt_get(type_info, &pkt, (uint32_t) regs->REG_EIP);

	data[0] = KEY_Kc | KEY_ARGI(0) | KEY_ARGS(1);
	data[1] = new_task->pid;

	size = strnlen((char *)new_task->comm, (MAX_STRING_SIZE - 8));
	memcpy((char *)&data[2], (char *)new_task->comm, size);

	/* Add arguments size to string size */
	size += 4 * 2;

	mtt_pkt_update_align32(pkt->length, size);

	/* Add type encoding. */
	type_info = MTT_TRACEITEM_BLOB(size);

	pkt->u.preamble->type_info = type_info;

	return mtt_kptrace_pkt_put(pkt);
}

int syscall_ihhh_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
	return _gen_pre4_handler(p, regs, KEY_E | KEY_ARGS_IHHH);
}

int syscall_iihh_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
	return _gen_pre4_handler(p, regs, KEY_E | KEY_ARGS_IIHH);
}

int syscall_hihi_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
	return _gen_pre4_handler(p, regs, KEY_E | KEY_ARGS_HIIH);
}

int syscall_ihih_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
	return _gen_pre4_handler(p, regs, KEY_E | KEY_ARGS_IHIH);
}

int syscall_hihh_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
	return _gen_pre4_handler(p, regs, KEY_E | KEY_ARGS_HIHH);
}

int syscall_hiih_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
	return _gen_pre4_handler(p, regs, KEY_E | KEY_ARGS_HIIH);
}

int syscall_hhhh_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
	return _gen_pre4_handler(p, regs, KEY_E | KEY_ARGS_HHHH);
}

static int user_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
	return _gen_pre4_handler(p, regs, KEY_U | KEY_ARGS_HHHH);
}

static int bpa2_alloc_pages_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
	return _gen_pre4_handler(p, regs, KEY_MH | KEY_ARGS_HIII);
}

/*generic syscall entry handler*/
static int _gen_pre4_handler(struct kprobe *p, struct pt_regs *regs,
			     uint32_t key)
{
	mtt_packet_t *pkt;
	uint32_t *data;
	uint32_t type_info =
	    MTT_TYPE(MTT_TRACEITEM_INT32, MTT_TYPEP_VECTOR, sizeof(uint32_t),
		     5);

	if (unlikely(!mtt_comp_ctxt[0]->active_filter))
		return 0;

	data = mtt_kptrace_pkt_get(type_info, &pkt, (uint32_t) regs->REG_EIP);

	data[0] = key;
	data[1] = (int)regs->REG_ARG0;
	data[2] = (int)regs->REG_ARG1;
	data[3] = (int)regs->REG_ARG2;
	data[4] = (int)regs->REG_ARG3;

	return mtt_kptrace_pkt_put(pkt);
}

/* Special syscall handler for prctl, in order to get the process name
 out of prctl(PR_SET_NAME) calls. */
static int syscall_prctl_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
	mtt_packet_t *pkt;
	uint32_t *data;
	uint32_t type_info = 0;

	if (unlikely(!mtt_comp_ctxt[0]->active_filter))
		return 0;

	data = mtt_kptrace_pkt_get(type_info, &pkt, (uint32_t) regs->REG_EIP);

	if ((unsigned)regs->REG_ARG0 == PR_SET_NAME) {
		char *string_p;
		long size;

		data[0] = KEY_E | KEY_ARGI(0) | KEY_ARGS(1);
		data[1] = (int)regs->REG_ARG0;
		string_p = (char *)&data[2];

		size =
		    strncpy_from_user(string_p, (char *)regs->REG_ARG1,
				      MAX_STRING_SIZE - 5 * 4);
		if (size < 0) {
			size = sizeof("<copy_from_user failed>");
			snprintf(string_p, MAX_STRING_SIZE - 6 * 4,
				"<copy_from_user failed>");
		}

		type_info =
		    MTT_TRACEITEM_BLOB(2 * 4 + size + 1 /*trailing \0 */);

	} else {
		data[0] = KEY_E | KEY_ARGS_IHHH;
		data[1] = (int)regs->REG_ARG0;
		data[2] = (int)regs->REG_ARG1;
		data[3] = (int)regs->REG_ARG2;
		data[4] = (int)regs->REG_ARG3;

		type_info =
		    MTT_TYPE(MTT_TRACEITEM_INT32, MTT_TYPEP_VECTOR,
			     sizeof(uint32_t), 5);
	}

	mtt_pkt_update_align32(pkt->length, type_info);
	pkt->u.preamble->type_info = type_info;

	return mtt_kptrace_pkt_put(pkt);
}

/* Output syscall arguments in string, hex, hex, hex format */
int syscall_hhhs_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
	mtt_packet_t *pkt;
	uint32_t *data;
	uint32_t type_info = 0;
	uint32_t size;
	uint32_t sys_call_pc = (uint32_t) regs->REG_EIP;

	if (unlikely(!mtt_comp_ctxt[0]->active_filter))
		return 0;

	data = mtt_kptrace_pkt_get(type_info, &pkt, sys_call_pc);

	data[0] = KEY_E | KEY_ARGS_HHHS;
	data[1] = (int)regs->REG_ARG1;
	data[2] = (int)regs->REG_ARG2;
	data[3] = (int)regs->REG_ARG3;

	if (sys_call_pc == (uint32_t) do_execve) {
		size =
		    snprintf((char *)&data[4], MAX_STRING_SIZE - 5 * 4,
			     (char *)regs->REG_ARG0);
	} else {
		size =
		    strncpy_from_user((char *)&data[4], (char *)regs->REG_ARG0,
				      MAX_STRING_SIZE - 5 * 4);
		if (size < 0)
			size =
			    snprintf((char *)&data[4], MAX_STRING_SIZE - 5 * 4,
				     "<copy_from_user failed>");
	}

	size += 4 * 4;

	mtt_pkt_update_align32(pkt->length, size);

	/* Add type encoding. */
	type_info = MTT_TRACEITEM_BLOB(size);

	pkt->u.preamble->type_info = type_info;

	return mtt_kptrace_pkt_put(pkt);
}

/* Output syscall arguments in string, int, hex, hex format. */
int syscall_ihhs_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
	mtt_packet_t *pkt;
	uint32_t *data;
	uint32_t type_info = 0;
	uint32_t size;

	if (unlikely(!mtt_comp_ctxt[0]->active_filter))
		return 0;

	data = mtt_kptrace_pkt_get(type_info, &pkt, (uint32_t) regs->REG_EIP);

	data[0] = KEY_E | KEY_ARGS_IHHS;
	data[1] = (int)regs->REG_ARG1;
	data[2] = (int)regs->REG_ARG2;
	data[3] = (int)regs->REG_ARG3;

	size = strncpy_from_user((char *)&data[4], (char *)regs->REG_ARG0,
				 MAX_STRING_SIZE - 6 * 4);
	if (size < 0)
		size =
		    snprintf((char *)&data[4], MAX_STRING_SIZE - 6 * 4,
			     "<copy_from_user failed>");

	/* Add arguments size to string size */
	size += 4 * 4;

	mtt_pkt_update_align32(pkt->length, size);

	/* Add type encoding. */
	type_info = MTT_TRACEITEM_BLOB(size);

	pkt->u.preamble->type_info = type_info;

	return mtt_kptrace_pkt_put(pkt);
}

static int hash_futex_handler(struct kprobe *p, struct pt_regs *regs)
{
	mtt_packet_t *pkt;
	uint32_t *data;
	union futex_key *fukey;
	uint32_t type_info =
	    MTT_TYPE(MTT_TRACEITEM_PTR32, MTT_TYPEP_VECTOR, sizeof(uint32_t),
		     4);

	if (unlikely(!mtt_comp_ctxt[0]->active_filter))
		return 0;

	data = mtt_kptrace_pkt_get(type_info, &pkt, (uint32_t) regs->REG_EIP);

	fukey = (union futex_key *)regs->REG_ARG0;

	data[0] = KEY_HF;
	data[1] = fukey->both.word;
	data[2] = (uint32_t) fukey->both.ptr;
	data[3] = fukey->both.offset;

	return mtt_kptrace_pkt_put(pkt);
}

static int kmalloc_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
	return _gen_pre2_handler(p, regs, KEY_MM);
}

static int get_free_pages_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
	return _gen_pre2_handler(p, regs, KEY_MG);
}

int alloc_pages_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
	return _gen_pre2_handler(p, regs, KEY_MA);
}

static int free_pages_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
	return _gen_pre2_handler(p, regs, KEY_MZ);
}

static int kmem_cache_alloc_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
	return _gen_pre2_handler(p, regs, KEY_MS);
}

static int kmem_cache_free_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
	return _gen_pre2_handler(p, regs, KEY_MX);
}

static int bpa2_free_pages_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
	return _gen_pre2_handler(p, regs, KEY_MI | KEY_ARGH(0) | KEY_ARGH(1));
}

/*generic syscall entry handler*/
static int _gen_pre2_handler(struct kprobe *p, struct pt_regs *regs,
			     uint32_t key)
{
	mtt_packet_t *pkt;
	uint32_t *data;
	uint32_t type_info =
	    MTT_TYPE(MTT_TRACEITEM_INT32, MTT_TYPEP_VECTOR, sizeof(uint32_t),
		     3);

	if (unlikely(!mtt_comp_ctxt[0]->active_filter))
		return 0;

	data = mtt_kptrace_pkt_get(type_info, &pkt, (uint32_t) regs->REG_EIP);

	data[0] = key | KEY_ARGH(0) | KEY_ARGI(1);
	data[1] = (int)regs->REG_ARG0;
	data[2] = (int)regs->REG_ARG1;

	return mtt_kptrace_pkt_put(pkt);
}

static int do_page_fault_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
	mtt_packet_t *pkt;
	uint32_t *data;
	uint32_t type_info =
	    MTT_TYPE(MTT_TRACEITEM_INT32, MTT_TYPEP_VECTOR, sizeof(uint32_t),
		     4);

	if (unlikely(!mtt_comp_ctxt[0]->active_filter))
		return 0;

	data = mtt_kptrace_pkt_get(type_info, &pkt, (uint32_t) regs->REG_EIP);

	data[0] = KEY_MD | KEY_ARGH(0) | KEY_ARGI(1) | KEY_ARGH(2);
	data[1] = (unsigned int)((struct pt_regs *)regs->REG_ARG0)->REG_EIP;
	data[2] = (int)regs->REG_ARG1;
	data[3] = (int)regs->REG_ARG2;

	return mtt_kptrace_pkt_put(pkt);
}

static int
bigphysarea_alloc_pages_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
	mtt_packet_t *pkt;
	uint32_t *data;
	uint32_t type_info =
	    MTT_TYPE(MTT_TRACEITEM_INT32, MTT_TYPEP_VECTOR, sizeof(uint32_t),
		     4);

	if (unlikely(!mtt_comp_ctxt[0]->active_filter))
		return 0;

	data = mtt_kptrace_pkt_get(type_info, &pkt, (uint32_t) regs->REG_EIP);

	data[0] = KEY_MB | KEY_ARGH(0) | KEY_ARGI(1) | KEY_ARGI(2);
	data[1] = (int)regs->REG_ARG0;
	data[2] = (int)regs->REG_ARG1;
	data[3] = (int)regs->REG_ARG2;

	return mtt_kptrace_pkt_put(pkt);
}

static int sock_sendmsg_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
	mtt_packet_t *pkt;
	uint32_t *data;
	uint32_t type_info =
	    MTT_TYPE(MTT_TRACEITEM_INT32, MTT_TYPEP_VECTOR, sizeof(uint32_t),
		     3);

	if (unlikely(!mtt_comp_ctxt[0]->active_filter))
		return 0;

	data = mtt_kptrace_pkt_get(type_info, &pkt, (uint32_t) regs->REG_EIP);

	data[0] = KEY_SS | KEY_ARGH(0) | KEY_ARGI(1);
	data[1] = (int)regs->REG_ARG0;
	data[2] = (int)regs->REG_ARG2;

	return mtt_kptrace_pkt_put(pkt);
}

static int sock_recvmsg_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
	mtt_packet_t *pkt;
	uint32_t *data;
	uint32_t type_info =
	    MTT_TYPE(MTT_TRACEITEM_INT32, MTT_TYPEP_VECTOR, sizeof(uint32_t),
		     4);

	if (unlikely(!mtt_comp_ctxt[0]->active_filter))
		return 0;

	data = mtt_kptrace_pkt_get(type_info, &pkt, (uint32_t) regs->REG_EIP);

	data[0] = KEY_SR | KEY_ARGH(0) | KEY_ARGI(1) | KEY_ARGI(2);
	data[1] = (int)regs->REG_ARG0;
	data[2] = (int)regs->REG_ARG2;
	data[3] = (int)regs->REG_ARG3;

	return mtt_kptrace_pkt_put(pkt);
}

static int do_setitimer_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
	mtt_packet_t *pkt;
	uint32_t *data;
	uint32_t type_info =
	    MTT_TYPE(MTT_TRACEITEM_INT32, MTT_TYPEP_VECTOR, sizeof(uint32_t),
		     6);
	struct itimerval *value = (struct itimerval *)regs->REG_ARG1;

	if (unlikely(!mtt_comp_ctxt[0]->active_filter))
		return 0;

	data = mtt_kptrace_pkt_get(type_info, &pkt, (uint32_t) regs->REG_EIP);

	data[0] = KEY_IS;
	data[1] = (int)regs->REG_ARG0;
	data[2] = value->it_interval.tv_sec;
	data[3] = value->it_interval.tv_usec;
	data[4] = value->it_value.tv_sec;
	data[5] = value->it_value.tv_usec;

	return mtt_kptrace_pkt_put(pkt);
}

static int down_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
	return _gen_sem_count_handler(p, regs, KEY_ZD);
}

static int
down_interruptible_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
	return _gen_sem_count_handler(p, regs, KEY_ZI);
}

static int down_trylock_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
	return _gen_sem_count_handler(p, regs, KEY_ZT);
}

static int up_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
	return _gen_sem_count_handler(p, regs, KEY_ZU);
}

/*generic syscall entry handler*/
static int _gen_sem_count_handler(struct kprobe *p, struct pt_regs *regs,
				  uint32_t key)
{
	mtt_packet_t *pkt;
	uint32_t *data;
	uint32_t type_info =
	    MTT_TYPE(MTT_TRACEITEM_INT32, MTT_TYPEP_VECTOR, sizeof(uint32_t),
		     3);
	struct semaphore *sem = (struct semaphore *)regs->REG_ARG0;

	if (unlikely(!mtt_comp_ctxt[0]->active_filter))
		return 0;

	data = mtt_kptrace_pkt_get(type_info, &pkt, (uint32_t) regs->REG_EIP);

	data[0] = key | KEY_ARGH(0) | KEY_ARGI(1);
	data[1] = (int)regs->REG_ARG0;
	data[2] = (unsigned int)sem->count;

	return mtt_kptrace_pkt_put(pkt);
}

static int down_read_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
	return _gen_sem_activity_handler(p, regs, KEY_ZR);
}

static int down_read_trylock_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
	return _gen_sem_activity_handler(p, regs, KEY_ZA);
}

static int up_read_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
	return _gen_sem_activity_handler(p, regs, KEY_Zr);
}

static int down_write_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
	return _gen_sem_activity_handler(p, regs, KEY_ZW);
}

static int
down_write_trylock_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
	return _gen_sem_activity_handler(p, regs, KEY_ZB);
}

static int up_write_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
	return _gen_sem_activity_handler(p, regs, KEY_Zw);
}

/*generic syscall entry handler*/
static int _gen_sem_activity_handler(struct kprobe *p, struct pt_regs *regs,
				     uint32_t key)
{
	mtt_packet_t *pkt;
	uint32_t *data;
	uint32_t type_info =
	    MTT_TYPE(MTT_TRACEITEM_INT32, MTT_TYPEP_VECTOR, sizeof(uint32_t),
		     3);
	struct rw_semaphore *sem = (struct rw_semaphore *)regs->REG_ARG0;

	if (unlikely(!mtt_comp_ctxt[0]->active_filter))
		return 0;

	data = mtt_kptrace_pkt_get(type_info, &pkt, (uint32_t) regs->REG_EIP);

	data[0] = key | KEY_ARGH(0) | KEY_ARGI(1);
	data[1] = (int)regs->REG_ARG0;
	data[2] = (unsigned int)sem->activity;

	return mtt_kptrace_pkt_put(pkt);
}

/**
 * Hooking of "log_sample" oprofile samplig function
 * in order to create an MTT packet. This allows for instance
 * to see the PMU events in the Kptrace chart.
 */
static int oprofile_add_sample_pre_handler(struct kprobe *p,
					    struct pt_regs *regs)
{
	mtt_packet_t *pkt;
	uint32_t *data;
	struct vm_area_struct *vma;
	struct mm_struct *mm;
	uint32_t addr = (uint32_t)(regs->REG_ARG1);
	uint32_t key = KEY_P;

	if (unlikely(!mtt_comp_ctxt[0]->active_filter))
		return 0;

	/* for usermode samples, store the offset within the file */
	if (addr < PAGE_OFFSET) {
		key = KEY_Pu;
		mm = current_thread_info()->task->mm;

		if (mm) {
			/* search the sorted tree of vma */
			vma = find_vma(mm, addr);
			/* vma should be non null, and offset <  vm_end */
			if (vma &&
			    likely((addr >= vma->vm_start) && (vma->vm_file)))
				addr = (vma->vm_pgoff << PAGE_SHIFT)
						 + addr
						 - vma->vm_start;
		}
	}

	data = mtt_kptrace_pkt_get(MTT_TRACEVECTOR_UINT32(1), &pkt, addr);

	*data = key;

	return mtt_kptrace_pkt_put(pkt);
}

/* Add the main sysdev and the "user" tracepoint set */
static int create_user_sysfs_tree(struct bus_type *m_subsys)
{
	/* in /sys/devices/system/mtt/mtt0 */
	user_set = kzalloc(sizeof(*user_set), GFP_KERNEL);
	if (!user_set) {
		printk(KERN_WARNING
		       "kptrace: Failed to allocate memory for user set\n");
		return -ENOMEM;
	}
	list_add(&user_set->list, &tracepoint_sets);

	if (kobject_init_and_add(&user_set->kobj, &user_type,
				 &(m_subsys->dev_root->kobj), "user") < 0)
		printk(KERN_WARNING "kptrace: Failed to add kobject user\n");
	user_set->enabled = 0;

	return 0;
}

static void init_core_event_logging(void)
{
	struct kp_tracepoint_set *set =
	    create_tracepoint_set("core_kernel_events");

	if (set == NULL) {
		printk(KERN_WARNING
		       "kptrace: unable to create core tracepoint set.\n");
		return;
	}

	/*install ARCH specific probes */
	arch_init_core_event_logging(set);

	kptrace_create_tracepoint(set, "handle_simple_irq", irq_KEY_I_handler,
				  irq_KEY_i_handler);
	kptrace_create_tracepoint(set, "handle_level_irq", irq_KEY_I_handler,
				  irq_KEY_i_handler);
	kptrace_create_tracepoint(set, "handle_fasteoi_irq", irq_KEY_I_handler,
				  irq_KEY_i_handler);
	kptrace_create_tracepoint(set, "handle_edge_irq", irq_KEY_I_handler,
				  irq_KEY_i_handler);
	kptrace_create_tracepoint(set, "irq_exit", NULL, irq_KEY_Ix_handler);

	kptrace_create_tracepoint(set, "tasklet_hi_action", softirq_pre_handler,
				  softirq_rp_handler);
	kptrace_create_tracepoint(set, "net_tx_action", softirq_pre_handler,
				  softirq_rp_handler);
	kptrace_create_tracepoint(set, "net_rx_action", softirq_pre_handler,
				  softirq_rp_handler);
	kptrace_create_tracepoint(set, "blk_done_softirq", softirq_pre_handler,
				  softirq_rp_handler);
	kptrace_create_tracepoint(set, "tasklet_action", softirq_pre_handler,
				  softirq_rp_handler);

	/* Since 2.6.38, kthread_create() is a macro. If the symbol exists,
	 * we set the tracepoint on that, if it doesn't then we use
	 * kthread_create_on_node with an alias so that it appears in sysfs
	 * as "kthread_create". This provides a common sysfs interface
	 * on each side of the change. */
	if (kallsyms_lookup_name("kthread_create"))
		kptrace_create_tracepoint(set, "kthread_create",
					  kthread_create_pre_handler,
					  kthread_create_rp_handler);
	else
		kptrace_create_aliased_tracepoint(set, "kthread_create_on_node",
						  kthread_create_pre_handler,
						  kthread_create_rp_handler,
						  "kthread_create");

	kptrace_create_tracepoint(set, "kernel_thread",
				  kernel_thread_pre_handler, NULL);
	kptrace_create_tracepoint(set, "exit_thread", exit_thread_pre_handler,
				  NULL);
}

static void init_smp_event_logging(void)
{
	struct kp_tracepoint_set *set =
		create_tracepoint_set("smp_events");

	if (set == NULL) {
		printk(KERN_WARNING
			"kptrace: unable to create smp tracepoint set.\n");
		return;
	}

	if (IS_ENABLED(CONFIG_HAVE_ARM_TWD)) {
		kptrace_create_tracepoint(set, "twd_handler",
			ipi_pre_handler, ipi_rp_handler);
	}

	kptrace_create_tracepoint(set, "scheduler_ipi",
		ipi_pre_handler, ipi_rp_handler);
	kptrace_create_tracepoint(set,
			"generic_smp_call_function_single_interrupt",
			ipi_pre_handler, ipi_rp_handler);
}

static void init_profiler_logging(void)
{
	struct kp_tracepoint_set *set;

	set = create_tracepoint_set("profiler_events");

	if (set == NULL) {
		pr_warn("kptrace: unable to create profiler tracepoint set.\n");
		return;
	}

	kptrace_create_tracepoint(set, "log_sample",
				  oprofile_add_sample_pre_handler,
				  NULL);
}

static void init_syscall_logging(void)
{
	struct kp_tracepoint_set *set = create_tracepoint_set("syscalls");

	mtt_printk(KERN_DEBUG "init_syscall_logging\n");

	if (set == NULL) {
		printk(KERN_WARNING
		       "kptrace: unable to create syscall tracepoint set.\n");
		return;
	}

	CALL(sys_restart_syscall)
	    CALL(sys_exit)
	    CALL(sys_fork)
	    CALL_CUSTOM_PRE(sys_read, syscall_ihih_pre_handler);
	CALL_CUSTOM_PRE(sys_write, syscall_ihih_pre_handler);
	CALL_CUSTOM_PRE(sys_open, syscall_hhhs_pre_handler);
	CALL_CUSTOM_PRE(sys_close, syscall_ihhh_pre_handler);
	CALL(sys_creat)
	    CALL(sys_link)
	    CALL(sys_unlink)
	    CALL(sys_chdir)
	    CALL(sys_mknod)
	    CALL(sys_chmod)
	    CALL(sys_lchown16)
	    CALL(sys_lseek)
	    CALL(sys_getpid)
	    CALL(sys_mount)
	    CALL(sys_setuid16)
	    CALL(sys_getuid16)
	    CALL(sys_ptrace)
	    CALL(sys_pause)
	    CALL_CUSTOM_PRE(sys_access, syscall_ihhs_pre_handler);
	CALL(sys_nice)
	    CALL(sys_sync)
	    CALL(sys_kill)
	    CALL(sys_rename)
	    CALL(sys_mkdir)
	    CALL(sys_rmdir)
	    CALL_CUSTOM_PRE(sys_dup, syscall_ihhh_pre_handler);
	CALL(sys_pipe)
	    CALL(sys_times)
	    CALL(sys_brk)
	    CALL(sys_setgid16)
	    CALL(sys_getgid16)
	    CALL(sys_geteuid16)
	    CALL(sys_getegid16)
	    CALL(sys_acct)
	    CALL(sys_umount)
	    CALL_CUSTOM_PRE(sys_ioctl, syscall_iihh_pre_handler);
	CALL(sys_fcntl)
	    CALL(sys_setpgid)
	    CALL(sys_umask)
	    CALL(sys_chroot)
	    CALL(sys_ustat)
	    CALL_CUSTOM_PRE(sys_dup2, syscall_iihh_pre_handler);
	CALL(sys_getppid)
	    CALL(sys_getpgrp)
	    CALL(sys_setsid)
	    CALL(sys_sigaction)
	    CALL(sys_setreuid16)
	    CALL(sys_setregid16)
	    CALL(sys_sigsuspend)
	    CALL(sys_sigpending)
	    CALL(sys_sethostname)
	    CALL(sys_setrlimit)
	    CALL(sys_getrusage)
	    CALL(sys_gettimeofday)
	    CALL(sys_settimeofday)
	    CALL(sys_getgroups16)
	    CALL(sys_setgroups16)
	    CALL(sys_symlink)
	    CALL(sys_readlink)
	    CALL(sys_uselib)
	    CALL(sys_swapon)
	    CALL(sys_reboot)
	    CALL_CUSTOM_PRE(sys_munmap, syscall_hihh_pre_handler);
	CALL(sys_truncate)
	    CALL(sys_ftruncate)
	    CALL(sys_fchmod)
	    CALL(sys_fchown16)
	    CALL(sys_getpriority)
	    CALL(sys_setpriority)
	    CALL(sys_statfs)
	    CALL(sys_fstatfs)
	    CALL(sys_syslog)
	    CALL(sys_setitimer)
	    CALL(sys_getitimer)
	    CALL(sys_newstat)
	    CALL(sys_newlstat)
	    CALL(sys_newfstat)
	    CALL(sys_vhangup)
	    CALL(sys_wait4)
	    CALL(sys_swapoff)
	    CALL(sys_sysinfo)
	    CALL(sys_fsync)
	    CALL(sys_sigreturn)
	    CALL(sys_clone)
	    CALL(sys_setdomainname)
	    CALL(sys_newuname)
	    CALL(sys_adjtimex)
	    CALL(sys_mprotect)
	    CALL(sys_sigprocmask)
	    CALL(sys_init_module)
	    CALL(sys_delete_module)
	    CALL(sys_quotactl)
	    CALL(sys_getpgid)
	    CALL(sys_fchdir)
	    CALL(sys_bdflush)
	    CALL(sys_sysfs)
	    CALL(sys_personality)
	    CALL(sys_setfsuid16)
	    CALL(sys_setfsgid16)
	    CALL(sys_llseek)
	    CALL(sys_getdents)
	    CALL(sys_select)
	    CALL(sys_flock)
	    CALL(sys_msync)
	    CALL(sys_readv)
	    CALL(sys_writev)
	    CALL(sys_getsid)
	    CALL(sys_fdatasync)
	    CALL(sys_sysctl)
	    CALL(sys_mlock)
	    CALL(sys_munlock)
	    CALL(sys_mlockall)
	    CALL(sys_munlockall)
	    CALL(sys_sched_setparam)
	    CALL(sys_sched_getparam)
	    CALL(sys_sched_setscheduler)
	    CALL(sys_sched_getscheduler)
	    CALL(sys_sched_yield)
	    CALL(sys_sched_get_priority_max)
	    CALL(sys_sched_get_priority_min)
	    CALL(sys_sched_rr_get_interval)
	    CALL(sys_nanosleep)
	    CALL(sys_setresuid16)
	    CALL(sys_getresuid16)
	    CALL(sys_poll)
	    CALL(sys_setresgid16)
	    CALL(sys_getresgid16)
	    CALL_CUSTOM_PRE(sys_prctl, syscall_prctl_pre_handler);
	CALL(sys_rt_sigreturn)
	    CALL(sys_rt_sigaction)
	    CALL(sys_rt_sigprocmask)
	    CALL(sys_rt_sigpending)
	    CALL(sys_rt_sigtimedwait)
	    CALL(sys_rt_sigqueueinfo)
	    CALL(sys_rt_sigsuspend)
	    CALL(sys_chown16)
	    CALL(sys_getcwd)
	    CALL(sys_capget)
	    CALL(sys_capset)
	    CALL(sys_sendfile)
	    CALL(sys_vfork)
	    CALL(sys_getrlimit)
	    CALL_CUSTOM_PRE(sys_mmap2, syscall_hiih_pre_handler);
	CALL(sys_lchown)
	    CALL(sys_getuid)
	    CALL(sys_getgid)
	    CALL(sys_geteuid)
	    CALL(sys_getegid)
	    CALL(sys_setreuid)
	    CALL(sys_setregid)
	    CALL(sys_getgroups)
	    CALL(sys_setgroups)
	    CALL(sys_fchown)
	    CALL(sys_setresuid)
	    CALL(sys_getresuid)
	    CALL(sys_setresgid)
	    CALL(sys_getresgid)
	    CALL(sys_chown)
	    CALL(sys_setuid)
	    CALL(sys_setgid)
	    CALL(sys_setfsuid)
	    CALL(sys_setfsgid)
	    CALL(sys_getdents64)
	    CALL(sys_pivot_root)
	    CALL(sys_mincore)
	    CALL(sys_madvise)
	    CALL(sys_gettid)
	    CALL(sys_setxattr)
	    CALL(sys_lsetxattr)
	    CALL(sys_fsetxattr)
	    CALL(sys_getxattr)
	    CALL(sys_lgetxattr)
	    CALL(sys_fgetxattr)
	    CALL(sys_listxattr)
	    CALL(sys_llistxattr)
	    CALL(sys_flistxattr)
	    CALL(sys_removexattr)
	    CALL(sys_lremovexattr)
	    CALL(sys_fremovexattr)
	    CALL(sys_tkill)
	    CALL(sys_sendfile64)
	    CALL(sys_futex)
	    CALL(sys_sched_setaffinity)
	    CALL(sys_sched_getaffinity)
	    CALL(sys_io_setup)
	    CALL(sys_io_destroy)
	    CALL(sys_io_getevents)
	    CALL(sys_io_submit)
	    CALL(sys_io_cancel)
	    CALL(sys_exit_group)
	    CALL(sys_lookup_dcookie)
	    CALL(sys_epoll_create)
	    CALL(sys_remap_file_pages)
	    CALL(sys_set_tid_address)
	    CALL(sys_timer_create)
	    CALL(sys_timer_settime)
	    CALL(sys_timer_gettime)
	    CALL(sys_timer_getoverrun)
	    CALL(sys_timer_delete)
	    CALL(sys_clock_settime)
	    CALL(sys_clock_gettime)
	    CALL(sys_clock_getres)
	    CALL(sys_clock_nanosleep)
	    CALL(sys_statfs64)
	    CALL(sys_fstatfs64)
	    CALL(sys_tgkill)
	    CALL(sys_utimes)
	    CALL(sys_fadvise64_64)
	    CALL(sys_mq_open)
	    CALL(sys_mq_unlink)
	    CALL(sys_mq_timedsend)
	    CALL(sys_mq_timedreceive)
	    CALL(sys_mq_notify)
	    CALL(sys_mq_getsetattr)
	    CALL(sys_waitid)
	    CALL(sys_socket)
	    CALL(sys_listen)
	    CALL(sys_accept)
	    CALL(sys_getsockname)
	    CALL(sys_getpeername)
	    CALL(sys_socketpair)
	    CALL(sys_send)
	    CALL(sys_recv)
	    CALL(sys_recvfrom)
	    CALL(sys_shutdown)
	    CALL(sys_setsockopt)
	    CALL(sys_getsockopt)
	    CALL(sys_recvmsg)
	    CALL(sys_semget)
	    CALL(sys_semctl)
	    CALL(sys_msgsnd)
	    CALL(sys_msgrcv)
	    CALL(sys_msgget)
	    CALL(sys_msgctl)
	    CALL(sys_shmat)
	    CALL(sys_shmdt)
	    CALL(sys_shmget)
	    CALL(sys_shmctl)
	    CALL(sys_add_key)
	    CALL(sys_request_key)
	    CALL(sys_keyctl)
	    CALL(sys_ioprio_set)
	    CALL(sys_ioprio_get)
	    CALL(sys_inotify_init)
	    CALL(sys_inotify_add_watch)
	    CALL(sys_inotify_rm_watch)
	    CALL(sys_mbind)
	    CALL(sys_get_mempolicy)
	    CALL(sys_set_mempolicy)
	    CALL(sys_openat)
	    CALL(sys_mkdirat)
	    CALL(sys_mknodat)
	    CALL(sys_fchownat)
	    CALL(sys_futimesat)
	    CALL(sys_unlinkat)
	    CALL(sys_renameat)
	    CALL(sys_linkat)
	    CALL(sys_symlinkat)
	    CALL(sys_readlinkat)
	    CALL(sys_fchmodat)
	    CALL(sys_faccessat)
	    CALL(sys_unshare)
	    CALL(sys_set_robust_list)
	    CALL(sys_get_robust_list)
	    CALL(sys_splice)
	    CALL(sys_tee)
	    CALL(sys_vmsplice)
	    CALL(sys_move_pages)
	    CALL(sys_kexec_load)
	    kptrace_create_tracepoint(set, "do_execve",
				      syscall_hhhs_pre_handler,
				      syscall_rp_handler);

	kptrace_create_tracepoint(set, "hash_futex", hash_futex_handler, NULL);

	/*install/override ARCH specific probes */
	arch_init_syscall_logging(set);
}

static void init_memory_logging(void)
{
	struct kp_tracepoint_set *set = create_tracepoint_set("memory_events");
	if (set == NULL) {
		printk(KERN_WARNING
		       "kptrace: unable to create memory tracepoint set.\n");
		return;
	}

	kptrace_create_tracepoint(set, "__kmalloc", kmalloc_pre_handler,
				  kmalloc_rp_handler);
	kptrace_create_tracepoint(set, "kfree", kfree_pre_handler, NULL);
	kptrace_create_tracepoint(set, "do_page_fault",
				  do_page_fault_pre_handler, NULL);
	kptrace_create_tracepoint(set, "vmalloc", vmalloc_pre_handler,
				  vmalloc_rp_handler);
	kptrace_create_tracepoint(set, "vfree", vfree_pre_handler, NULL);
	kptrace_create_tracepoint(set, "__get_free_pages",
				  get_free_pages_pre_handler,
				  get_free_pages_rp_handler);
	kptrace_create_tracepoint(set, "free_pages", free_pages_pre_handler,
				  NULL);
	kptrace_create_tracepoint(set, "kmem_cache_alloc",
				  kmem_cache_alloc_pre_handler,
				  kmem_cache_alloc_rp_handler);
	kptrace_create_tracepoint(set, "kmem_cache_free",
				  kmem_cache_free_pre_handler, NULL);

	if (IS_ENABLED(CONFIG_BPA2) || IS_ENABLED(CONFIG_BIGPHYS_AREA)) {
		kptrace_create_tracepoint(set, "__bigphysarea_alloc_pages",
				bigphysarea_alloc_pages_pre_handler,
				bigphysarea_alloc_pages_rp_handler);
		kptrace_create_tracepoint(set, "bigphysarea_free_pages",
				bigphysarea_free_pages_pre_handler, NULL);
		kptrace_create_tracepoint(set, "__bpa2_alloc_pages",
				bpa2_alloc_pages_pre_handler,
				bpa2_alloc_pages_rp_handler);
		kptrace_create_tracepoint(set, "bpa2_free_pages",
				bpa2_free_pages_pre_handler, NULL);
	}

	/*install/override ARCH specific probes */
	arch_init_memory_logging(set);
}

static void init_network_logging(void)
{
	struct kp_tracepoint_set *set = create_tracepoint_set("network_events");
	if (set == NULL) {
		printk(KERN_WARNING
		       "kptrace: unable to create network tracepoint set.\n");
		return;
	}

	kptrace_create_tracepoint(set, "netif_receive_skb",
				  netif_receive_skb_pre_handler,
				  netif_receive_skb_rp_handler);
	kptrace_create_tracepoint(set, "dev_queue_xmit",
				  dev_queue_xmit_pre_handler,
				  dev_queue_xmit_rp_handler);
	kptrace_create_tracepoint(set, "sock_sendmsg", sock_sendmsg_pre_handler,
				  sock_sendmsg_rp_handler);
	kptrace_create_tracepoint(set, "sock_recvmsg", sock_recvmsg_pre_handler,
				  sock_recvmsg_rp_handler);
}

static void init_timer_logging(void)
{
	struct kp_tracepoint_set *set = create_tracepoint_set("timer_events");
	if (!set) {
		printk(KERN_WARNING
		       "kptrace: unable to create timer tracepoint set.\n");
		return;
	}

	kptrace_create_tracepoint(set, "do_setitimer", do_setitimer_pre_handler,
				  do_setitimer_rp_handler);
	kptrace_create_tracepoint(set, "it_real_fn", it_real_fn_pre_handler,
				  it_real_fn_rp_handler);
	kptrace_create_tracepoint(set, "run_timer_softirq", softirq_pre_handler,
				  softirq_rp_handler);
	kptrace_create_tracepoint(set, "try_to_wake_up", wake_pre_handler,
				  NULL);
}

static void init_synchronization_logging(void)
{
	struct kp_tracepoint_set *set =
	    create_tracepoint_set("synchronization_events");
	if (!set) {
		printk(KERN_WARNING
		       "kptrace: unable to create synchronization tracepoint "
		       "set.\n");
		return;
	}

	if (!IS_ENABLED(CONFIG_DEBUG_LOCK_ALLOC))
		kptrace_create_late_tracepoint(set, "mutex_lock",
					       mutex_lock_pre_handler, NULL);
	else
		kptrace_create_late_aliased_tracepoint(set, "mutex_lock_nested",
					       mutex_lock_pre_handler, NULL,
					       "mutex_lock");

	kptrace_create_late_tracepoint(set, "mutex_unlock",
				       mutex_unlock_pre_handler, NULL);

	kptrace_create_tracepoint(set, "down", down_pre_handler, NULL);
	kptrace_create_tracepoint(set, "down_interruptible",
				  down_interruptible_pre_handler, NULL);
	kptrace_create_tracepoint(set, "down_trylock", down_trylock_pre_handler,
				  down_trylock_rp_handler);
	kptrace_create_tracepoint(set, "up", up_pre_handler, NULL);
	kptrace_create_tracepoint(set, "__up", underscore_up_pre_handler, NULL);

	kptrace_create_tracepoint(set, "down_read", down_read_pre_handler,
				  NULL);
	kptrace_create_tracepoint(set, "down_read_trylock",
				  down_read_trylock_pre_handler,
				  down_read_trylock_rp_handler);
	kptrace_create_tracepoint(set, "down_write", down_write_pre_handler,
				  NULL);
	kptrace_create_tracepoint(set, "down_write_trylock",
				  down_write_trylock_pre_handler,
				  down_write_trylock_rp_handler);
	kptrace_create_tracepoint(set, "up_read", up_read_pre_handler, NULL);
	kptrace_create_tracepoint(set, "up_write", up_write_pre_handler, NULL);
}

/*
 * ktprace_init - initialize the relay channel and the sysfs tree
 */
int __init mtt_kptrace_init(struct bus_type *m_subsys)
{
	int err = create_user_sysfs_tree(m_subsys);
	if (err) {
		mtt_printk(KERN_ERR "kptrace: initialization failed.\n");
		return err;
	}

	mtt_subsys = m_subsys;

	init_core_event_logging();

	if (IS_ENABLED(CONFIG_SMP))
		init_smp_event_logging();

	init_syscall_logging();
	init_memory_logging();
	init_network_logging();
	init_timer_logging();

	if (IS_ENABLED(CONFIG_KPTRACE_SYNC))
		init_synchronization_logging();

	if (IS_ENABLED(CONFIG_HAVE_OPROFILE))
		init_profiler_logging();

	mtt_printk(KERN_INFO "kptrace: initialized\n");
	return 0;
}

/*
 * kptrace_cleanup - free all the tracepoints and sets, remove the sysdev and
 * destroy the relay channel.
 */
void mtt_kptrace_cleanup(void)
{
	struct list_head *p;
	struct kp_tracepoint_set *set;
	struct kp_tracepoint *tp;

	mtt_printk(KERN_DEBUG "kptrace_cleanup\n");

	list_for_each(p, &tracepoint_sets) {
		set = list_entry(p, struct kp_tracepoint_set, list);
		if (set != NULL) {
			kobject_put(&set->kobj);
			kfree(set);
		}
	}

	list_for_each(p, &tracepoints) {
		tp = list_entry(p, struct kp_tracepoint, list);
		if (tp != NULL) {
			kobject_put(&tp->kobj);
			kfree(tp);
		}
	}
}
