/*
 * arch/sh/kernel/cpu/clock.c - SuperH clock framework
 *
 *  Copyright (C) 2005, 2006, 2007  Paul Mundt
 *
 * This clock framework is derived from the OMAP version by:
 *
 *	Copyright (C) 2004 - 2005 Nokia Corporation
 *	Written by Tuukka Tikkanen <tuukka.tikkanen@elektrobit.com>
 *
 *  Modified for omap shared clock framework by Tony Lindgren <tony@atomide.com>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
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
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/proc_fs.h>
#include <linux/delay.h>
#include <asm/clock.h>
#include <asm/timer.h>

static LIST_HEAD(clock_list);
static DEFINE_SPINLOCK(clock_lock);
static DEFINE_MUTEX(clock_list_sem);

static void propagate_rate(struct clk *clk)
{
	struct clk *clkp;

	list_for_each_entry(clkp, &clock_list, node) {
		if (likely(clkp->parent != clk))
			continue;
		if (likely(clkp->ops && clkp->ops->recalc))
			clkp->ops->recalc(clkp);
		if (unlikely(clkp->flags & CLK_RATE_PROPAGATES))
			propagate_rate(clkp);
	}
}

int __clk_enable(struct clk *clk)
{
	kref_get(&clk->kref);

	if (clk->flags & CLK_ALWAYS_ENABLED)
		return 0;

	if (likely(clk->ops && clk->ops->enable))
		clk->ops->enable(clk);

	return 0;
}
EXPORT_SYMBOL_GPL(__clk_enable);

int clk_enable(struct clk *clk)
{
	unsigned long flags;
	int ret;

	spin_lock_irqsave(&clock_lock, flags);
	ret = __clk_enable(clk);
	spin_unlock_irqrestore(&clock_lock, flags);

	return ret;
}
EXPORT_SYMBOL_GPL(clk_enable);

static void clk_kref_release(struct kref *kref)
{
	/* Nothing to do */
}

void __clk_disable(struct clk *clk)
{
	int count = kref_put(&clk->kref, clk_kref_release);

	if (clk->flags & CLK_ALWAYS_ENABLED)
		return;

	if (!count) {	/* count reaches zero, disable the clock */
		if (likely(clk->ops && clk->ops->disable))
			clk->ops->disable(clk);
	}
}
EXPORT_SYMBOL_GPL(__clk_disable);

void clk_disable(struct clk *clk)
{
	unsigned long flags;

	spin_lock_irqsave(&clock_lock, flags);
	__clk_disable(clk);
	spin_unlock_irqrestore(&clock_lock, flags);
}
EXPORT_SYMBOL_GPL(clk_disable);

int clk_register(struct clk *clk)
{
	mutex_lock(&clock_list_sem);

	list_add(&clk->node, &clock_list);
	INIT_LIST_HEAD(&clk->childs);
	if (clk->parent)
		list_add(&clk->childs_node, &clk->parent->childs);
	kref_init(&clk->kref);

	mutex_unlock(&clock_list_sem);

	if (clk->ops && clk->ops->init)
		clk->ops->init(clk);

	if (clk->flags & CLK_ALWAYS_ENABLED) {
		pr_debug( "Clock '%s' is ALWAYS_ENABLED\n", clk->name);
		if (clk->ops && clk->ops->enable)
			clk->ops->enable(clk);
		pr_debug( "Enabled.");
	}

	return 0;
}
EXPORT_SYMBOL_GPL(clk_register);

void clk_unregister(struct clk *clk)
{
	mutex_lock(&clock_list_sem);
	list_del(&clk->node);
	if (clk->parent)
		list_del(&clk->childs_node);
	mutex_unlock(&clock_list_sem);
}
EXPORT_SYMBOL_GPL(clk_unregister);

unsigned long clk_get_rate(struct clk *clk)
{
	return clk->rate;
}
EXPORT_SYMBOL_GPL(clk_get_rate);

int clk_set_rate(struct clk *clk, unsigned long rate)
{
	int ret = -EOPNOTSUPP;

	if (likely(clk->ops && clk->ops->set_rate)) {
		unsigned long flags;

		spin_lock_irqsave(&clock_lock, flags);
		ret = clk->ops->set_rate(clk, rate);
		spin_unlock_irqrestore(&clock_lock, flags);
	}

	if (unlikely(clk->flags & CLK_RATE_PROPAGATES))
		propagate_rate(clk);
	return ret;
}
EXPORT_SYMBOL_GPL(clk_set_rate);

int clk_set_rate_ex(struct clk *clk, unsigned long rate, int algo_id)
{
	int ret = -EOPNOTSUPP;

	if (likely(clk->ops && clk->ops->set_rate_ex)) {
		unsigned long flags;

		spin_lock_irqsave(&clock_lock, flags);
		ret = clk->ops->set_rate_ex(clk, rate, algo_id);
		spin_unlock_irqrestore(&clock_lock, flags);
	}

	if (unlikely(clk->flags & CLK_RATE_PROPAGATES))
		propagate_rate(clk);

	return ret;
}
EXPORT_SYMBOL_GPL(clk_set_rate_ex);

void clk_recalc_rate(struct clk *clk)
{
	if (likely(clk->ops && clk->ops->recalc)) {
		unsigned long flags;

		spin_lock_irqsave(&clock_lock, flags);
		clk->ops->recalc(clk);
		spin_unlock_irqrestore(&clock_lock, flags);
	}

	if (unlikely(clk->flags & CLK_RATE_PROPAGATES))
		propagate_rate(clk);
}
EXPORT_SYMBOL_GPL(clk_recalc_rate);

long clk_round_rate(struct clk *clk, unsigned long rate)
{
	if (likely(clk->ops && clk->ops->round_rate)) {
		unsigned long flags, rounded;

		spin_lock_irqsave(&clock_lock, flags);
		rounded = clk->ops->round_rate(clk, rate);
		spin_unlock_irqrestore(&clock_lock, flags);

		return rounded;
	}

	return clk_get_rate(clk);
}
EXPORT_SYMBOL_GPL(clk_round_rate);

int clk_set_parent(struct clk *clk, struct clk *parent)
{
	int ret = -EINVAL;
	struct clk *old;
	if (!parent || !clk)
		return ret;
	old = clk->parent;
	if (likely(clk->ops && clk->ops->set_parent)) {
		unsigned long flags;
		spin_lock_irqsave(&clock_lock, flags);
		ret = clk->ops->set_parent(clk, parent);
		spin_unlock_irqrestore(&clock_lock, flags);
		clk->parent = (ret ? old : parent);
	}
	if (unlikely(clk->flags & CLK_RATE_PROPAGATES))
		propagate_rate(clk);
	return ret;
}
EXPORT_SYMBOL_GPL(clk_set_parent);

int clk_observe(struct clk *clk, unsigned long *div)
{
	int ret = -EINVAL;
	if (!clk)
		return ret;
	if (likely(clk->ops && clk->ops->observe))
		ret = clk->ops->observe(clk, div);
	return ret;
}

/*
 * Returns a clock. Note that we first try to use device id on the bus
 * and clock name. If this fails, we try to use clock name only.
 */
struct clk *clk_get(struct device *dev, const char *id)
{
	struct clk *p, *clk = ERR_PTR(-ENOENT);
	int idno;

	if (dev == NULL || dev->bus != &platform_bus_type)
		idno = -1;
	else
		idno = to_platform_device(dev)->id;

	mutex_lock(&clock_list_sem);
	list_for_each_entry(p, &clock_list, node) {
		if (p->id == idno &&
		    strcmp(id, p->name) == 0 && try_module_get(p->owner)) {
			clk = p;
			goto found;
		}
	}

	list_for_each_entry(p, &clock_list, node) {
		if (strcmp(id, p->name) == 0 && try_module_get(p->owner)) {
			clk = p;
			break;
		}
	}

found:
	mutex_unlock(&clock_list_sem);

	return clk;
}
EXPORT_SYMBOL_GPL(clk_get);

void clk_put(struct clk *clk)
{
	if (clk && !IS_ERR(clk))
		module_put(clk->owner);
}
EXPORT_SYMBOL_GPL(clk_put);

static int show_clocks(char *buf, char **start, off_t off,
		       int len, int *eof, void *data)
{
	struct clk *clk;
	char *p = buf;

	list_for_each_entry_reverse(clk, &clock_list, node) {
		unsigned long rate = clk_get_rate(clk);

		/*
		 * Don't bother listing dummy clocks with no ancestry
		 * that only support enable and disable ops.
		 */
		if (unlikely(!rate && !clk->parent))
			continue;

		p += sprintf(p, "%-12s\t: %ld.%02ldMHz\n", clk->name,
			     rate / 1000000, (rate % 1000000) / 10000);
	}

	return p - buf;
}

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

static int clks_sysdev_suspend(struct sys_device *dev, pm_message_t state)
{
	static pm_message_t prev_state;
	unsigned long rate;
	struct clk *clkp;

	switch (state.event) {
	case PM_EVENT_ON: /* Resume from: */
		switch (prev_state.event) {
		case PM_EVENT_FREEZE: /* Hibernation */
			list_for_each_entry(clkp, &clock_list, node)
				if (likely(clkp->ops)) {
					rate = clkp->rate;
					if (likely(clkp->ops->set_parent))
						clkp->ops->set_parent(clkp,
							clkp->parent);
					if (likely(clkp->ops->set_rate))
						clkp->ops->set_rate(clkp,
							clkp->rate);
					if (likely(clkp->ops->recalc))
						clkp->ops->recalc(clkp);

				};
		break;
		case PM_EVENT_SUSPEND: /* Suspend/Standby */
			list_for_each_entry(clkp, &clock_list, node) {
				if (!likely(clkp->ops))
					continue;
				/* check if the pm modified the clock */
				if (!pm_clk_is_modified(clkp))
					continue;
				pm_clk_set(clkp, 0);
				/* turn-on */
				if (pm_clk_is_off(clkp) && clkp->ops->enable)
					clkp->ops->enable(clkp);
				else
				if (likely(clkp->ops->set_rate))
					clkp->ops->set_rate(clkp, clkp->rate *
						pm_clk_ratio(clkp));
			};
		break;
		}
	break;
	case PM_EVENT_FREEZE:
	break;
	case PM_EVENT_SUSPEND:
		/* reduces/turns-off the frequency based
		 * on the flags directive
		 */
		list_for_each_entry_reverse(clkp, &clock_list, node) {
			if (!clkp->ops)
				continue;
			if (!clkp->rate) /* already disabled */
				continue;
			pm_clk_set(clkp, 1);
			/* turn-off */
			if (pm_clk_is_off(clkp) && clkp->ops->disable)
					clkp->ops->disable(clkp);
			else /* reduce */
			if (likely(clkp->ops->set_rate))
				clkp->ops->set_rate(clkp, clkp->rate /
					pm_clk_ratio(clkp));

		}
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

static int __init clk_proc_init(void)
{
	struct proc_dir_entry *p;

	sysdev_class_register(&clk_sysdev_class);
	sysdev_driver_register(&clk_sysdev_class, &clks_sysdev_driver);
	sysdev_register(&clks_sysdev_dev);

	p = create_proc_read_entry("clocks", S_IRUSR, NULL,
				   show_clocks, NULL);
	if (unlikely(!p))
		return -EINVAL;

	return 0;
}
subsys_initcall(clk_proc_init);

int clk_for_each(int (*fn)(struct clk *clk, void *data), void *data)
{
	struct clk *clkp;
	int result = 0;

	if (!fn)
		return -1;

	mutex_lock(&clock_list_sem);
	list_for_each_entry(clkp, &clock_list, node) {
		result |= fn(clkp, data);
		}
	mutex_unlock(&clock_list_sem);
	return result;
}
