#ifndef __ASM_SH_CLOCK_H
#define __ASM_SH_CLOCK_H

#include <linux/kref.h>
#include <linux/list.h>
#include <linux/seq_file.h>
#include <linux/clk.h>

struct clk;

struct clk_ops {
	int (*init)(struct clk *clk);
	int (*enable)(struct clk *clk);
	int (*disable)(struct clk *clk);
	void (*recalc)(struct clk *clk);
	int (*set_rate)(struct clk *clk, unsigned long rate);
	int (*set_parent)(struct clk *clk, struct clk *parent);
	long (*round_rate)(struct clk *clk, unsigned long rate);
	int (*observe)(struct clk *clk, unsigned long *div); /* Route clock on external pin */
	unsigned long (*get_measure)(struct clk *clk);
	void *private_data;
	int (*set_rate_ex)(struct clk *clk, unsigned long rate, int aldo_id);
};

struct clk {
	struct list_head	node;
	const char		*name;
	int			id;
	struct module		*owner;

	struct clk		*parent;
	struct clk_ops		*ops;

	void			*private_data;

	struct kref		kref;

	unsigned long		rate;
	unsigned long		nominal_rate;
	unsigned long		flags;

	struct list_head	childs;
	struct list_head	childs_node;
};

#define CLK_ALWAYS_ENABLED	(1 << 0)
#define CLK_RATE_PROPAGATES	(1 << 1)

#define CLK_PM_EXP_SHIFT	(24)
#define CLK_PM_EXP_NRBITS	(7)
#define CLK_PM_RATIO_SHIFT	(16)
#define CLK_PM_RATIO_NRBITS	(8)
#define CLK_PM_EDIT_SHIFT	(31)
#define CLK_PM_EDIT_NRBITS	(1)
#define CLK_PM_TURNOFF		(((1<<CLK_PM_EXP_NRBITS)-1) << CLK_PM_EXP_SHIFT)

/* Should be defined by processor-specific code */
void arch_init_clk_ops(struct clk_ops **, int type);

/* arch/sh/kernel/cpu/clock.c */
int clk_init(void);

int __clk_enable(struct clk *);
void __clk_disable(struct clk *);

void clk_recalc_rate(struct clk *);

int clk_register(struct clk *);
void clk_unregister(struct clk *);

int clk_for_each(int (*fn)(struct clk *clk, void *data), void *data);

/**
 * Routes the clock on an external pin (if possible)
 */
int clk_observe(struct clk *clk, unsigned long *div);

/**
 * Evaluate the clock rate in hardware (if possible)
 */
unsigned long clk_get_measure(struct clk *clk);

/* the exported API, in addition to clk_set_rate */
/**
 * clk_set_rate_ex - set the clock rate for a clock source, with additional parameter
 * @clk: clock source
 * @rate: desired clock rate in Hz
 * @algo_id: algorithm id to be passed down to ops->set_rate
 *
 * Returns success (0) or negative errno.
 */
int clk_set_rate_ex(struct clk *clk, unsigned long rate, int algo_id);

enum clk_sh_algo_id {
	NO_CHANGE = 0,

	IUS_N1_N1,
	IUS_322,
	IUS_522,
	IUS_N11,

	SB_N1,

	SB3_N1,
	SB3_32,
	SB3_43,
	SB3_54,

	BP_N1,

	IP_N1,
};

/* arch/sh/kernel/cpu/clock.c */
int clk_init(void);
#endif /* __ASM_SH_CLOCK_H */
