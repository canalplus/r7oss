/*
 *  HPET based high-frequency timer
 *
 *  Copyright (c) 2005 Clemens Ladisch
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */

#include "adriver.h"
#include <linux/init.h>
#include <linux/time.h>
#include <linux/threads.h>
#include <linux/interrupt.h>
#include <linux/moduleparam.h>
#include <sound/core.h>
#include <sound/timer.h>
#include <sound/info.h>
#include <linux/hpet.h>

static int frequency = 1024;
static snd_timer_t *hpetimer;
static struct hpet_task hpet_task;

static int snd_hpet_open(snd_timer_t *timer)
{
	int err;

	err = hpet_register(&hpet_task, 0);
	if (err < 0)
		return err;
	hpet_control(&hpet_task, HPET_IRQFREQ, frequency);
	timer->private_data = &hpet_task;
	return 0;
}

static int snd_hpet_close(snd_timer_t *timer)
{
	hpet_unregister(&hpet_task);
	hpet_task.ht_opaque = NULL;
	return 0;
}

static int snd_hpet_start(snd_timer_t *timer)
{
	return hpet_control(&hpet_task, HPET_IE_ON, 0);
}

static int snd_hpet_stop(snd_timer_t *timer)
{
	return hpet_control(&hpet_task, HPET_IE_OFF, 0);
}

static void snd_hpet_task_func(void *private_data)
{
	snd_timer_interrupt((snd_timer_t *)private_data, 1);
}

static struct _snd_timer_hardware snd_hpet_hw = {
	.flags = SNDRV_TIMER_HW_AUTO,
	.ticks = 100000000,
	.open = snd_hpet_open,
	.close = snd_hpet_close,
	.start = snd_hpet_start,
	.stop = snd_hpet_stop,
};

static int __init hpetimer_init(void)
{
	int err;
	snd_timer_t *timer;

	if (frequency < 32 || frequency > 8192) {
		snd_printk(KERN_ERR "invalid frequency %d\n", frequency);
		return -EINVAL;
	}

	err = snd_timer_global_new("hpet", SNDRV_TIMER_GLOBAL_HPET, &timer);
	if (err < 0)
		return err;

	timer->module = THIS_MODULE;
	strcpy(timer->name, "high precision event timer");
	timer->hw = snd_hpet_hw;
	timer->hw.resolution = 1000000000 / frequency;

	hpet_task.ht_func = snd_hpet_task_func;
	hpet_task.ht_data = timer;

	err = snd_timer_global_register(timer);
	if (err < 0) {
		snd_timer_global_free(timer);
		return err;
	}
	hpetimer = timer;
	return 0;
}

static void __exit hpetimer_exit(void)
{
	if (hpetimer)
		snd_timer_global_unregister(hpetimer);
}


module_init(hpetimer_init)
module_exit(hpetimer_exit)

module_param(frequency, int, 0444);
MODULE_PARM_DESC(frequency, "timer frequency in Hz");

MODULE_AUTHOR("Clemens Ladisch <clemens@ladisch.de>");
MODULE_DESCRIPTION("High Precision Event Timer");
MODULE_LICENSE("GPL");

MODULE_ALIAS("snd-timer-" __stringify(SNDRV_TIMER_GLOBAL_HPET));

EXPORT_NO_SYMBOLS;
