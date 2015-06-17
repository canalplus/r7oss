/*
 * drivers/net/stmmac/stmmac_timer.c
 *
 * Use Timers for mitigating network interrupts.
 * Currently it's possible to use both the SH4 RTC device
 * and the TMU channel 2.
 *
 * Copyright (C) 2008 by STMicroelectronics
 * Author: Giuseppe Cavallaro <peppe.cavallaro@st.com>
*/

#include <linux/kernel.h>
#include <linux/etherdevice.h>
#include "stmmac_timer.h"

static void stmmac_timer_handler(void *data)
{
	struct net_device *dev = (struct net_device *)data;

	stmmac_timer_work(dev);

	return;
}

#define STMMAC_TIMER_MSG(timer,freq) \
printk(KERN_INFO "stmmac_timer: %s Timer ON (freq %dHz)\n", timer, freq);

#if defined(CONFIG_STMMAC_RTC_TIMER)
#include <linux/rtc.h>
static struct rtc_device *stmmac_rtc;
static rtc_task_t stmmac_task;

static void stmmac_rtc_start(unsigned int new_freq)
{
	rtc_irq_set_freq(stmmac_rtc, &stmmac_task, new_freq);
	rtc_irq_set_state(stmmac_rtc, &stmmac_task, 1);
	return;
}

static void stmmac_rtc_stop(void)
{
	rtc_irq_set_state(stmmac_rtc, &stmmac_task, 0);
	return;
}

int stmmac_open_hw_timer(struct net_device *dev, struct stmmac_timer *tm)
{
	stmmac_task.private_data = dev;
	stmmac_task.func = stmmac_timer_handler;

	stmmac_rtc = rtc_class_open(CONFIG_RTC_HCTOSYS_DEVICE);
	if (stmmac_rtc == NULL) {
		printk(KERN_ERR "open rtc device failed\n");
		return -ENODEV;
	}

	rtc_irq_register(stmmac_rtc, &stmmac_task);

	/* Periodic mode is not supported */
	if ((rtc_irq_set_freq(stmmac_rtc, &stmmac_task, tm->freq) < 0)) {
		printk(KERN_ERR "set periodic failed\n");
		rtc_irq_unregister(stmmac_rtc, &stmmac_task);
		rtc_class_close(stmmac_rtc);
		return -1;
	}

	STMMAC_TIMER_MSG(CONFIG_RTC_HCTOSYS_DEVICE, tm->freq);

	tm->timer_start = stmmac_rtc_start;
	tm->timer_stop = stmmac_rtc_stop;

	return 0;
}

int stmmac_close_hw_timer(void)
{
	rtc_irq_set_state(stmmac_rtc, &stmmac_task, 0);
	rtc_irq_unregister(stmmac_rtc, &stmmac_task);
	rtc_class_close(stmmac_rtc);
	return 0;
}

#elif defined(CONFIG_STMMAC_TMU_TIMER)
#include <linux/clk.h>
#define TMU_CHANNEL "tmu2_clk"
static struct clk *timer_clock;

static void stmmac_tmu_start(unsigned int new_freq)
{
	clk_set_rate(timer_clock, new_freq);
	clk_enable(timer_clock);
	return;
}

static void stmmac_tmu_stop(void)
{
	clk_disable(timer_clock);
	return;
}

int stmmac_open_hw_timer(struct net_device *dev, struct stmmac_timer *tm)
{
	timer_clock = clk_get(NULL, TMU_CHANNEL);

	if (timer_clock == NULL)
		return -1;

	if (tmu2_register_user(stmmac_timer_handler, (void *)dev) < 0) {
		timer_clock = NULL;
		return -1;
	}

	STMMAC_TIMER_MSG("TMU2", tm->freq);
	tm->timer_start = stmmac_tmu_start;
	tm->timer_stop = stmmac_tmu_stop;

	return 0;
}

int stmmac_close_hw_timer(void)
{
	clk_disable(timer_clock);
	tmu2_unregister_user();
	clk_put(timer_clock);
	return 0;
}
#endif
