/*
 * arch/sh/kernel/cpu/clk.c - STMicroelectronics clock framework
 *
 *  Copyright (C) 2009, STMicroelectronics
 *  Author: Francesco M. Virlinzi <francesco.virlinzi@st.com>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License V2 _ONLY_.  See the file "COPYING" in the main directory of this archive
 * for more details.
 */
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/sysdev.h>
#include <linux/mutex.h>
#include <linux/list.h>
#include <linux/kref.h>
#include <linux/seq_file.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/proc_fs.h>
#include <linux/delay.h>
#include <linux/stm/clk.h>

static LIST_HEAD(global_list);
/*static DEFINE_SPINLOCK(global_lock);*/
static DEFINE_MUTEX(global_list_sem);

enum clk_ops_id {
	_CLK_INIT = 0,
	_CLK_ENABLE,
	_CLK_DISABLE,
	_CLK_SET_RATE,
	_CLK_SET_PARENT,
	_CLK_RECALC,
	_CLK_ROUND,
	_CLK_OBSERVE,
	_CLK_MEASURE
};


enum child_event {
	_CHILD_CLK_ENABLED = 1,
	_CHILD_CLK_DISABLED,
};

/*
 * The CLK_ENABLED flag is used to set/reset the enabled clock flag
 * unfortunatelly the rate == or != zero isn't enough because
 * the TMU2 used by the STMac need to be set (as rate) before it's enabled
 *
 * In the future when the TMU will be registered as device this constraint
 * can be removed and enabled/disabled clock will be checked only with
 * the rate equal or not to zero
 */
#define CLK_ENABLED (1 << 15)
static inline int clk_is_enabled(struct clk *clk)
{
	return (clk->flags & CLK_ENABLED);
}

static inline void clk_set_enabled(struct clk *clk)
{
	clk->flags |= CLK_ENABLED;
}

static inline void clk_set_disabled(struct clk *clk)
{
	clk->flags &= ~CLK_ENABLED;
}


/*
 * All the __clk_xxxx operation will not raise propagation system
 */
static int
__clk_operation(struct clk *clk, unsigned long data, enum clk_ops_id id_ops)
{
	int ret = -EINVAL;
	unsigned long *ops_fns = (unsigned long *)clk->ops;
	if (likely(ops_fns && ops_fns[id_ops])) {
		int (*fns)(struct clk *clk, unsigned long rate)
			= (void *)ops_fns[id_ops];
		long flags;
		spin_lock_irqsave(&clk->lock, flags);
		ret = fns(clk, data);
		spin_unlock_irqrestore(&clk->lock, flags);
	}
	return ret;
}

static inline int __clk_init(struct clk *clk)
{
	return __clk_operation(clk, 0, _CLK_INIT);
}

static inline int __clk_enable(struct clk *clk)
{
	return __clk_operation(clk, 0, _CLK_ENABLE);
}

static inline int __clk_disable(struct clk *clk)
{
	return __clk_operation(clk, 0, _CLK_DISABLE);
}

static inline int __clk_set_rate(struct clk *clk, unsigned long rate)
{
	return __clk_operation(clk, rate, _CLK_SET_RATE);
}

static inline int __clk_set_parent(struct clk *clk, struct clk *parent)
{
	return __clk_operation(clk, (unsigned long)parent, _CLK_SET_PARENT);
}

static inline void __clk_recalc_rate(struct clk *clk)
{
	__clk_operation(clk, 0, _CLK_RECALC);
}

static inline int __clk_observe(struct clk *clk, unsigned long value)
{
	return __clk_operation(clk, value, _CLK_OBSERVE);
}

static inline int __clk_get_measure(struct clk *clk, unsigned long value)
{
	return __clk_operation(clk, value, _CLK_MEASURE);
}

static inline int clk_is_always_enabled(struct clk *clk)
{
	return (clk->flags & CLK_ALWAYS_ENABLED);
}

static inline int clk_wants_propagate(struct clk *clk)
{
	return (clk->flags & CLK_RATE_PROPAGATES);
}

static inline int clk_wants_auto_switching(struct clk *clk)
{
	return (clk->flags & CLK_AUTO_SWITCHING);
}

static int clk_notify_to_parent(enum child_event code, struct clk *clk)
{
	int ret = 0;

	if (!clk)
		return ret;

	switch (code) {
	case _CHILD_CLK_ENABLED:
		++clk->nr_active_clocks;
		break;
	case _CHILD_CLK_DISABLED:
		--clk->nr_active_clocks;
		break;
	}

	if (!clk_wants_auto_switching(clk))
		return ret;

	if (!clk->nr_active_clocks) /* no user... disable */
		clk_disable(clk);
	else if (clk->nr_active_clocks == 1) /* the first user... enable */
		clk_enable(clk);

	return 0;
}

static int __clk_for_each_child(struct clk *clk,
		int (*fn)(struct clk *clk, void *data), void *data)
{
	struct clk *clkp;
	int result = 0;

	if (!fn || !clk)
		return -EINVAL;

	list_for_each_entry(clkp, &clk->children, children_node)
		result |= fn(clkp, data);

	return result;
}

static void clk_propagate_rate(struct clk *clk);
static int __clk_propagate_rate(struct clk *clk, void *data)
{
	__clk_recalc_rate(clk);

	if (likely(clk_wants_propagate(clk)))
		clk_propagate_rate(clk);

	return 0;
}

static void clk_propagate_rate(struct clk *clk)
{
	__clk_for_each_child(clk, __clk_propagate_rate, NULL);
}

int clk_enable(struct clk *clk)
{
	int ret = 0;

	if (!clk)
		return -EINVAL;

	if (clk_is_always_enabled(clk))
		/* No enable required! */
		return 0;

/*
	if (clk_get_rate(clk))
		return 0;
*/
	if (clk_is_enabled(clk))
		return 0;

	clk_notify_to_parent(_CHILD_CLK_ENABLED, clk->parent);

	ret = __clk_enable(clk);
	if (!ret) {
		clk_set_enabled(clk);
		clk_notify_to_parent(_CHILD_CLK_DISABLED, clk->parent);
	}
	return ret;
}
EXPORT_SYMBOL(clk_enable);

void clk_disable(struct clk *clk)
{
	if (!clk)
		return;

	if (clk_is_always_enabled(clk))
		/* this clock can not be disabled */
		return;
/*
	if (!clk_get_rate(clk))
		return;
*/
	if (!clk_is_enabled(clk))
		return;

	__clk_disable(clk);
	clk_set_disabled(clk);
	clk_notify_to_parent(_CHILD_CLK_DISABLED, clk->parent);
}
EXPORT_SYMBOL(clk_disable);

int clk_register(struct clk *clk)
{
	if (!clk || !clk->name)
		return -EINVAL;

	mutex_lock(&global_list_sem);

	list_add_tail(&clk->node, &global_list);
	INIT_LIST_HEAD(&clk->children);
	spin_lock_init(&clk->lock);

	clk->nr_active_clocks = 0;

	if (clk->parent)
		list_add_tail(&clk->children_node, &clk->parent->children);

	kref_init(&clk->kref);

	mutex_unlock(&global_list_sem);

	__clk_init(clk);

	if (clk_is_always_enabled(clk)) {
		__clk_enable(clk);
		clk_set_enabled(clk);
	}
	return 0;
}
EXPORT_SYMBOL(clk_register);

void clk_unregister(struct clk *clk)
{
	if (!clk)
		return;
	mutex_lock(&global_list_sem);
	list_del(&clk->node);
	if (clk->parent)
		list_del(&clk->children_node);
	mutex_unlock(&global_list_sem);
}
EXPORT_SYMBOL(clk_unregister);

unsigned long clk_get_rate(struct clk *clk)
{
	if (!clk)
		return -EINVAL;
	return clk->rate;
}
EXPORT_SYMBOL(clk_get_rate);

int clk_set_rate(struct clk *clk, unsigned long rate)
{
	int ret = -EINVAL;

	if (!clk)
		return ret;

	if (rate == clk_get_rate(clk))
		return 0;

	ret = __clk_set_rate(clk, rate);

	if (clk_wants_propagate(clk) && !ret)
		clk_propagate_rate(clk);
	return ret;
}
EXPORT_SYMBOL(clk_set_rate);

long clk_round_rate(struct clk *clk, unsigned long rate)
{
	unsigned long ret = clk_get_rate(clk);

	if (likely(clk->ops && clk->ops->round_rate))
		ret = clk->ops->round_rate(clk, rate);
	return ret;
}
EXPORT_SYMBOL(clk_round_rate);

struct clk *clk_get_parent(struct clk *clk)
{
	if (!clk)
		return NULL;
	return clk->parent;
}
EXPORT_SYMBOL(clk_get_parent);

int clk_set_parent(struct clk *clk, struct clk *parent)
{
	int ret = -EINVAL;
	struct clk *old_parent;
	unsigned long old_rate;

	if (!parent || !clk)
		return ret;

	if (parent == clk_get_parent(clk))
		return 0;

	old_parent = clk_get_parent(clk);
	old_rate = clk_get_rate(clk);

	if (old_rate)
		/* enable the new parent if required */
		clk_notify_to_parent(_CHILD_CLK_ENABLED, parent);

	ret = __clk_set_parent(clk, parent);

	/* update the parent field */
	clk->parent = (ret ? old_parent : parent);

	if (old_rate)
		/* notify to the parent the 'disable' clock */
		clk_notify_to_parent(_CHILD_CLK_DISABLED,
			(ret ? parent : old_parent));
	/* propagate if required */
	if (!ret && likely(clk_wants_propagate(clk)))
		clk_propagate_rate(clk);

	return ret;
}
EXPORT_SYMBOL(clk_set_parent);

int clk_observe(struct clk *clk, unsigned long *div)
{
	int ret = -EINVAL;
	if (!clk)
		return ret;
	return __clk_observe(clk, (unsigned long) div);
}
EXPORT_SYMBOL(clk_observe);

/*
 * Returns a clock. Note that we first try to use device id on the bus
 * and clock name. If this fails, we try to use clock name only.
 */
struct clk *clk_get(struct device *dev, const char *name)
{
	struct clk *clkp, *clk = ERR_PTR(-ENOENT);

	mutex_lock(&global_list_sem);

	list_for_each_entry(clkp, &global_list, node) {
		if (strcmp(name, clkp->name) == 0 &&
		    try_module_get(clkp->owner)) {
			clk = clkp;
			break;
		}
	}

	mutex_unlock(&global_list_sem);

	return clk;
}
EXPORT_SYMBOL(clk_get);

void clk_put(struct clk *clk)
{
	if (clk && !IS_ERR(clk))
		module_put(clk->owner);
}
EXPORT_SYMBOL(clk_put);

int clk_for_each(int (*fn)(struct clk *clk, void *data), void *data)
{
	struct clk *clkp;
	int result = 0;

	if (!fn)
		return -EINVAL;

	mutex_lock(&global_list_sem);
	list_for_each_entry(clkp, &global_list, node)
		result |= fn(clkp, data);
	mutex_unlock(&global_list_sem);
	return result;
}
EXPORT_SYMBOL(clk_for_each);

int clk_for_each_child(struct clk *clk, int (*fn)(struct clk *clk, void *data),
		void *data)
{
	int ret = 0;
	mutex_lock(&global_list_sem);
	ret = __clk_for_each_child(clk, fn, data);
	mutex_unlock(&global_list_sem);
	return ret;
}
EXPORT_SYMBOL(clk_for_each_child);

#ifdef CONFIG_PROC_FS
static void *clk_seq_next(struct seq_file *s, void *v, loff_t *pos)
{
	struct list_head *tmp;
	union {
		loff_t value;
		long parts[2];
	} ltmp;

	ltmp.value = *pos;
	tmp = (struct list_head *)ltmp.parts[0];
	tmp = tmp->next;
	ltmp.parts[0] = (long)tmp;

	*pos = ltmp.value;

	if (tmp == &global_list)
		return NULL; /* No more to read */
	return pos;
}

static void *clk_seq_start(struct seq_file *s, loff_t *pos)
{
	if (!*pos) { /* first call! */
		 union {
			  loff_t value;
			  long parts[2];
		 } ltmp;
		 ltmp.parts[0] = (long) global_list.next;
		 *pos = ltmp. value;
		 return pos;
	}
	--(*pos); /* to realign *pos value! */

	return clk_seq_next(s, NULL, pos);
}

static int clk_seq_show(struct seq_file *s, void *v)
{
	unsigned long *l = (unsigned long *)v;
	struct list_head *node = (struct list_head *)(*l);
	struct clk *clk = container_of(node, struct clk, node);
	unsigned long rate = clk_get_rate(clk);
	if (unlikely(!rate && !clk->parent))
		return 0;
	seq_printf(s, "%-12s\t: %ld.%02ldMHz - ", clk->name,
		rate / 1000000, (rate % 1000000) / 10000);
	seq_printf(s, "[%ld.%02ldMHz] - ", clk->nominal_rate / 1000000,
		(clk->nominal_rate % 1000000) / 10000);
	seq_printf(s, "[0x%p]", clk);
	if (clk_is_enabled(clk))
		seq_printf(s, " - enabled");

	if (clk->parent)
		seq_printf(s, " - [%s]", clk->parent->name);
	seq_printf(s, "\n");
	return 0;
}

static void clk_seq_stop(struct seq_file *s, void *v)
{
}

static struct seq_operations clk_seq_ops = {
	.start = clk_seq_start,
	.next = clk_seq_next,
	.stop = clk_seq_stop,
	.show = clk_seq_show,
};

static int clk_proc_open(struct inode *inode, struct file *file)
{
	return seq_open(file, &clk_seq_ops);
}

static struct file_operations clk_proc_ops = {
	.owner = THIS_MODULE,
	.open = clk_proc_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release,
};

static int __init clk_proc_init(void)
{
	struct proc_dir_entry *p;

	p = create_proc_entry("clocks", S_IRUGO, NULL);

	if (unlikely(!p))
		return -EINVAL;

	p->proc_fops = &clk_proc_ops;

	return 0;
}
subsys_initcall(clk_proc_init);
#endif

#ifdef CONFIG_PM
/*
 * The standard pm_clk_ratio rule allowes a default ratio of 1
 *
 * pm_ratio = (flags.RATIO + 1 ) << (flags.EXP)
 */
static inline int pm_clk_ratio(struct clk *clk)
{
	register unsigned int val, exp;

	val = ((clk->flags >> CLK_PM_RATIO_SHIFT) &
		((1 << CLK_PM_RATIO_NRBITS) -1)) + 1;
	exp = ((clk->flags >> CLK_PM_EXP_SHIFT) &
		((1 << CLK_PM_EXP_NRBITS) -1));

	return (val << exp);
}

static inline int pm_clk_is_off(struct clk *clk)
{
	return ((clk->flags & CLK_PM_TURNOFF) == CLK_PM_TURNOFF);
}

static inline void pm_clk_set(struct clk *clk, int edited)
{
#define CLK_PM_EDITED (1<<CLK_PM_EDIT_SHIFT)
	clk->flags &= ~CLK_PM_EDITED;
	clk->flags |= (edited ? CLK_PM_EDITED : 0);
}

static inline int pm_clk_is_modified(struct clk *clk)
{
	return ((clk->flags & CLK_PM_EDITED) != 0);
}

static int clk_resume_from_standby(struct clk *clk, void *data)
{
	if (!likely(clk->ops))
		return 0;
	/* check if the pm modified the clock */
	if (!pm_clk_is_modified(clk))
		return 0;;

	pm_clk_set(clk, 0);

	if (pm_clk_is_off(clk))
		__clk_enable(clk);
	else
		__clk_set_rate(clk, clk->rate * pm_clk_ratio(clk));
	return 0;
}

static int clk_resume_from_hibernation(struct clk *clk, void *data)
{
	unsigned long rate = clk->rate;

	__clk_set_parent(clk, clk->parent);
	__clk_set_rate(clk, rate);
	__clk_recalc_rate(clk);
	return 0;
}

static int clk_on_standby(struct clk *clk, void *data)
{
	if (!clk->ops)
		return 0;
/*
	if (!clk->rate) already disabled
		return 0;
*/
	if (!clk_is_enabled(clk)) /* already disabled */
		return 0;

	pm_clk_set(clk, 1);	/* set as modified */
	if (pm_clk_is_off(clk))		/* turn-off */
		__clk_disable(clk);
	else    /* reduce */
		__clk_set_rate(clk, clk->rate / pm_clk_ratio(clk));
	return 0;
}

static int clks_sysdev_suspend(struct sys_device *dev, pm_message_t state)
{
	static pm_message_t prev_state;

	switch (state.event) {
	case PM_EVENT_ON:
		switch (prev_state.event) {
		case PM_EVENT_FREEZE: /* Resumeing from hibernation */
			clk_for_each(clk_resume_from_hibernation, NULL);
			break;
		case PM_EVENT_SUSPEND:
			clk_for_each(clk_resume_from_standby, NULL);
			break;
		}
	case PM_EVENT_SUSPEND:
		clk_for_each(clk_on_standby, NULL);
		break;
	case PM_EVENT_FREEZE:
		break;
	}
	prev_state = state;
	return 0;
}

static int clks_sysdev_resume(struct sys_device *dev)
{
	return clks_sysdev_suspend(dev, PMSG_ON);
}

static struct sysdev_class clk_sysdev_class = {
	set_kset_name("clks"),
};

static struct sysdev_driver clks_sysdev_driver = {
	.suspend = clks_sysdev_suspend,
	.resume = clks_sysdev_resume,
};

static struct sys_device clks_sysdev_dev = {
	.cls = &clk_sysdev_class,
};

static int __init clk_sysdev_init(void)
{
	sysdev_class_register(&clk_sysdev_class);
	sysdev_driver_register(&clk_sysdev_class, &clks_sysdev_driver);
	sysdev_register(&clks_sysdev_dev);
	return 0;
}

subsys_initcall(clk_sysdev_init);
#endif

