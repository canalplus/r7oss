/*
 * arch/sh/oprofile/op_model_timer.c
 *
 * Copyright (C) 2007 Dave Peverley
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Generic SH profiling - just uses the timer interrupt. This is separate to
 * the standard support so we can still hook into the
 *
 */
#include <linux/kernel.h>
#include <linux/notifier.h>
#include <linux/smp.h>
#include <linux/oprofile.h>
#include <linux/profile.h>
#include <linux/init.h>
#include <asm/ptrace.h>
#include <linux/errno.h>

#include "op_sh_model.h"


static int timer_notify(struct pt_regs *regs)
{
        oprofile_add_sample(regs, 0);
        return 0;
}

static int sh_timer_setup_ctrs(void)
{
        /* TODO : This is using the timer irq... */
        return 0;
}

static void sh_timer_stop(void)
{
        unregister_timer_hook(timer_notify);
}

static int sh_timer_start(void)
{
        int ret;

	ret = register_timer_hook(timer_notify);

        if (ret < 0) {
                printk(KERN_ERR "oprofile: unable to register timer hook\n");
                return ret;
        }

        return 0;
}

static int sh_timer_init(void)
{
        return 0;
}

struct op_sh_model_spec op_shtimer_spec = {
        .init           = sh_timer_init,
	.num_counters   = 0,  // TODO : Copy timer config to report?
	.setup_ctrs     = sh_timer_setup_ctrs,
        .start          = sh_timer_start,
        .stop           = sh_timer_stop,
        .name           = "timer",
};
