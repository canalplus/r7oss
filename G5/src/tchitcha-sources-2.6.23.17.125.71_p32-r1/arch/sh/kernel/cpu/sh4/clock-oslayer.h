/******************************************************************************
 *
 * File name   : clock-oslayer.h
 * Description : Low Level API - OS Specifics
 *
 * COPYRIGHT (C) 2009 STMicroelectronics - All Rights Reserved
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 *****************************************************************************/

#ifndef __CLKLLA_OSLAYER_H
#define __CLKLLA_OSLAYER_H

#include <linux/io.h>
#include <linux/stm/sysconf.h>
#include <linux/stm/pio.h>
#include <asm-generic/errno-base.h>

#define clk_t	struct clk
#define U32 					unsigned long

/* Register access macros */
#define CLK_READ(addr)	  			ioread32(addr)
#define CLK_WRITE(addr,val)			iowrite32(val, addr)
#define STSYS_ReadRegDev32LE(addr)		ioread32(addr)
#define STSYS_WriteRegDev32LE(addr,val)		iowrite32(val, addr)

#define SYSCONF(type, num, lsb, msb)			\
	static struct sysconf_field *sys_##type##_##num##_##lsb##_##msb

#define SYSCONF_CLAIM(type, num, lsb, msb)		\
	 sys_##type##_##num##_##lsb##_##msb =		\
		sysconf_claim(type, num, lsb, msb, "Clk lla")

#define SYSCONF_READ(type, num, lsb, msb)		\
	sysconf_read(sys_##type##_##num##_##lsb##_##msb)

#define SYSCONF_WRITE(type, num, lsb, msb, value)	\
	sysconf_write(sys_##type##_##num##_##lsb##_##msb, value)

static inline
void PIO_SET_MODE(unsigned long bank, unsigned long line, long mode)
{
	static struct stpio_pin *pio;
	if (!pio)
		pio =
		    stpio_request_pin(bank, line, "Clk Observer", mode);
	else
		stpio_configure_pin(pio, mode);
}

/*
 * Linux needs 3 extra clocks:
 * Both the clocks are virtually created as child clock of physical clock
 * with a relationship 1:1 with the parent
 *
 * The GENERIC_LINUX_CLK has to be used to add 'virtual' clock
 * used for backward compatibility in Linux.
 * Mandatory Linux needs 3 clocks
 * - sh4_clk: it's the cpu clock
 * - module_clk: it's the parent of TMUs clock
 * - comms_clk: it's the clock used by COMMS block
 */
#define GENERIC_LINUX_OPERATIONS()			\
static int generic_clk_recalc(struct clk *clk_p)	\
{							\
	clk_p->rate = clk_p->parent->rate;		\
	return 0;					\
}							\
static struct clk_ops generic_clk_ops =			\
{							\
	.init = generic_clk_recalc,			\
	.recalc = generic_clk_recalc,			\
};

#define GENERIC_LINUX_CLK(_name, _parent)		\
{							\
	.name		= #_name,			\
	.parent		= &(_parent),			\
	.flags		= CLK_RATE_PROPAGATES,		\
	.ops		= &generic_clk_ops,		\
}

#define GENERIC_LINUX_CLKS(_sh4_clk_p, _module_clk_p, _comms_clk_p)	\
GENERIC_LINUX_OPERATIONS();						\
static struct clk generic_linux_clks[] =				\
{									\
	GENERIC_LINUX_CLK(sh4_clk, _sh4_clk_p),				\
	GENERIC_LINUX_CLK(module_clk, _module_clk_p),			\
	GENERIC_LINUX_CLK(comms_clk, _comms_clk_p)			\
}

#define REGISTER_GENERIC_LINUX_CLKS()					\
{									\
	int i;								\
	for (i = 0; i < ARRAY_SIZE(generic_linux_clks); ++i) {		\
		generic_linux_clks[i].parent->flags |= CLK_RATE_PROPAGATES;\
		if (!clk_register(&generic_linux_clks[i]))		\
			clk_enable(&generic_linux_clks[i]);		\
	}								\
}

#define _CLK_OPS(_name, _desc, _init, _setparent, _setfreq, _recalc,	\
		     _enable, _disable, _observe, _measure, _obspoint)	\
static struct clk_ops  _name= {						\
	.init = _init,							\
	.set_parent = _setparent,					\
	.set_rate = _setfreq,						\
	.recalc = _recalc,						\
	.enable = _enable,						\
	.disable = _disable,						\
	.observe = _observe,						\
	.get_measure = _measure,					\
}

/* Clock registration macro (used by clock-xxxx.c) */
#define _CLK(_id, _ops, _nominal, _flags)				\
[_id] = (clk_t){ .name = #_id,						\
		 .id = (_id),						\
		 .ops = (_ops),						\
		 .flags = (_flags),					\
		 .nominal_rate = (_nominal),				\
		 .children = LIST_HEAD_INIT(clk_clocks[_id].children),	\
}

#define _CLK_P(_id, _ops, _nominal, _flags, _parent)			\
[_id] = (clk_t){ .name = #_id,						\
		 .id = (_id),						\
		 .ops = (_ops),						\
		 .flags = (_flags),					\
		 .nominal_rate = (_nominal),				\
		 .parent = (_parent),					\
		 .children = LIST_HEAD_INIT(clk_clocks[_id].children),	\
}

/* deprecated macros */
#define REGISTER_OPS(_name, _desc, _init, _setparent, _setfreq, _recalc,\
		     _enable, _disable, _observe, _measure, _obspoint)	\
	_CLK_OPS(_name, _desc, _init, _setparent, _setfreq, _recalc,	\
		     _enable, _disable, _observe, _measure, _obspoint)

#define REGISTER_CLK(_id, _ops, _nominal, _flags)			\
	_CLK(_id, _ops, _nominal, _flags)

#define REGISTER_CLK_P(_id, _ops, _nominal, _flags, _parent)		\
	_CLK_P(_id, _ops, _nominal, _flags, _parent)

#define time_ticks_per_sec()		CONFIG_HZ
#define task_delay(x)			mdelay((x)/CONFIG_HZ)

/* Low level API errors */
typedef enum clk_err {
	CLK_ERR_NONE = 0,
	CLK_ERR_FEATURE_NOT_SUPPORTED = -EPERM,
	CLK_ERR_BAD_PARAMETER = -EINVAL,
	CLK_ERR_INTERNAL = -EFAULT	/* Internal & fatal error */
} clk_err_t;


#endif /* #ifndef __CLKLLA_OSLAYER_H */
