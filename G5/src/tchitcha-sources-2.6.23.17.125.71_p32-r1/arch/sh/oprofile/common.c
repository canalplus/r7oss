/*
 * arch/sh/oprofile/op_model_null.c
 *
 * Copyright (C) 2003  Paul Mundt
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 */
#include <linux/kernel.h>
#include <linux/oprofile.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/sysdev.h>
#include <linux/mutex.h>


#include "op_sh_model.h"
#include "op_counter.h"

static struct op_sh_model_spec *op_sh_model;
static int op_sh_enabled;
static DEFINE_MUTEX(op_sh_mutex);

struct op_counter_config *counter_config;

static int op_sh_setup(void)
{
        int ret;

        spin_lock(&oprofilefs_lock);
        ret = op_sh_model->setup_ctrs();
        spin_unlock(&oprofilefs_lock);
        return ret;
}

static int op_sh_start(void)
{
        int ret = -EBUSY;

        mutex_lock(&op_sh_mutex);
        if (!op_sh_enabled) {
                ret = op_sh_model->start();
                op_sh_enabled = !ret;
        }
        mutex_unlock(&op_sh_mutex);
        return ret;
}

static void op_sh_stop(void)
{
        mutex_lock(&op_sh_mutex);
        if (op_sh_enabled)
                op_sh_model->stop();
        op_sh_enabled = 0;
        mutex_unlock(&op_sh_mutex);
}

#ifdef CONFIG_PM
#error This needs to be implemented!
#else
#define init_driverfs() do { } while (0)
#define exit_driverfs() do { } while (0)
#endif /* CONFIG_PM */


int __init oprofile_arch_init(struct oprofile_operations *ops)
{
        struct op_sh_model_spec *spec = NULL;
        int ret = -ENODEV;

#if defined(CONFIG_OPROFILE_TMU)
	spec = &op_sh7109_spec;
#else
        spec = &op_shtimer_spec;
#endif

	if (spec) {
                ret = spec->init();

                if (ret < 0)
                        return ret;

                op_sh_model = spec;
                init_driverfs();
                ops->create_files       = NULL;
                ops->setup              = op_sh_setup;
                ops->shutdown           = op_sh_stop;
                ops->start              = op_sh_start;
                ops->stop               = op_sh_stop;
                ops->cpu_type           = op_sh_model->name;
		ops->backtrace          = sh_backtrace;
                printk(KERN_INFO "oprofile: using %s\n", spec->name);
	}

	return ret;
}

void oprofile_arch_exit(void)
{
        if (op_sh_model) {
                exit_driverfs();
                op_sh_model = NULL;
        }
}
