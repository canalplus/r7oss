/*
 * (C) Copyright	2006,2007 -
 * 		Mathieu Desnoyers (mathieu.desnoyers@polymtl.ca)
 *
 * notes : ltt-timestamp timer-based clock cannot be used for early tracing in
 * the boot process, as it depends on timer interrupts.
 *
 * The timer needs to be only on one CPU to support hotplug.
 * We have the choice between schedule_delayed_work_on and an IPI to get each
 * CPU to write the heartbeat. IPI has been chosen because it is considered
 * faster than passing through the timer to get the work scheduled on all the
 * CPUs.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/timer.h>
#include <linux/workqueue.h>
#include <linux/cpu.h>
#include <linux/timex.h>
#include <linux/bitops.h>
#include <linux/ltt.h>
#include <linux/smp.h>
#include <linux/sched.h> /* FIX for m68k local_irq_enable in on_each_cpu */

atomic_t lttng_generic_clock;
EXPORT_SYMBOL(lttng_generic_clock);

/* Expected maximum interrupt latency in ms : 15ms, *2 for security */
#define EXPECTED_INTERRUPT_LATENCY	30

static struct timer_list stsc_timer;
static unsigned int precalc_expire;

/* For architectures with 32 bits TSC */
static struct synthetic_tsc_struct {
	u32 tsc[2][2];	/* a pair of 2 32 bits. [0] is the MSB, [1] is LSB */
	unsigned int index;	/* Index of the current synth. tsc. */
} ____cacheline_aligned synthetic_tsc[NR_CPUS];

/* Called from IPI : either in interrupt or process context */
static void ltt_update_synthetic_tsc(void)
{
	struct synthetic_tsc_struct *cpu_synth;
	u32 tsc;

	preempt_disable();
	cpu_synth = &synthetic_tsc[smp_processor_id()];
	tsc = ltt_get_timestamp32();		/* We deal with a 32 LSB TSC */

	if (tsc < cpu_synth->tsc[cpu_synth->index][1]) {
		unsigned int new_index = cpu_synth->index ? 0 : 1; /* 0 <-> 1 */
		/*
		 * Overflow
		 * Non atomic update of the non current synthetic TSC, followed
		 * by an atomic index change. There is no write concurrency,
		 * so the index read/write does not need to be atomic.
		 */
		cpu_synth->tsc[new_index][1] = tsc; /* LSB update */
		cpu_synth->tsc[new_index][0] =
			cpu_synth->tsc[cpu_synth->index][0]+1; /* MSB update */
		cpu_synth->index = new_index;	/* atomic change of index */
	} else {
		/*
		 * No overflow : we can simply update the 32 LSB of the current
		 * synthetic TSC as it's an atomic write.
		 */
		cpu_synth->tsc[cpu_synth->index][1] = tsc;
	}
	preempt_enable();
}

/* Called from buffer switch : in _any_ context (even NMI) */
u64 ltt_read_synthetic_tsc(void)
{
	struct synthetic_tsc_struct *cpu_synth;
	u64 ret;
	unsigned int index;
	u32 tsc;

	preempt_disable();
	cpu_synth = &synthetic_tsc[smp_processor_id()];
	index = cpu_synth->index; /* atomic read */
	tsc = ltt_get_timestamp32();		/* We deal with a 32 LSB TSC */

	if (tsc < cpu_synth->tsc[index][1]) {
		/* Overflow */
		ret = ((u64)(cpu_synth->tsc[index][0]+1) << 32) | ((u64)tsc);
	} else {
		/* no overflow */
		ret = ((u64)cpu_synth->tsc[index][0] << 32) | ((u64)tsc);
	}
	preempt_enable();
	return ret;
}
EXPORT_SYMBOL_GPL(ltt_read_synthetic_tsc);

static void synthetic_tsc_ipi(void *info)
{
	ltt_update_synthetic_tsc();
}

/* We need to be in process context to do an IPI */
static void synthetic_tsc_work(struct work_struct *work)
{
	on_each_cpu(synthetic_tsc_ipi, NULL, 1, 1);
}
static DECLARE_WORK(stsc_work, synthetic_tsc_work);

/*
 * stsc_timer : - Timer function synchronizing synthetic TSC.
 * @data: unused
 *
 * Guarantees at least 1 execution before low word of TSC wraps.
 */
static void stsc_timer_fct(unsigned long data)
{
	PREPARE_WORK(&stsc_work, synthetic_tsc_work);
	schedule_work(&stsc_work);

	mod_timer(&stsc_timer, jiffies + precalc_expire);
}

/*
 * precalc_stsc_interval: - Precalculates the interval between the 32 bits TSC
 * wraparounds.
 */
static int __init precalc_stsc_interval(void)
{
	unsigned long mask;

	mask = 0xFFFFFFFFUL;
	precalc_expire =
		(mask/((ltt_frequency() / HZ * ltt_freq_scale()) << 1)
			- 1 - (EXPECTED_INTERRUPT_LATENCY*HZ/1000)) >> 1;
	WARN_ON(precalc_expire == 0);
	printk(KERN_DEBUG "Synthetic TSC timer will fire each %u jiffies.\n",
		precalc_expire);
	return 0;
}

/*
 * 	hotcpu_callback - CPU hotplug callback
 * 	@nb: notifier block
 * 	@action: hotplug action to take
 * 	@hcpu: CPU number
 *
 *	Sets the new CPU's current synthetic TSC to the same value as the
 *	currently running CPU.
 *
 * 	Returns the success/failure of the operation. (NOTIFY_OK, NOTIFY_BAD)
 */
static int __cpuinit hotcpu_callback(struct notifier_block *nb,
				unsigned long action,
				void *hcpu)
{
	unsigned int hotcpu = (unsigned long)hcpu;
	struct synthetic_tsc_struct *cpu_synth;
	u64 local_count;

	switch (action) {
	case CPU_UP_PREPARE:
		cpu_synth = &synthetic_tsc[hotcpu];
		local_count = ltt_read_synthetic_tsc();
		cpu_synth->tsc[0][1] = (u32)local_count; /* LSB */
		cpu_synth->tsc[0][0] = (u32)(local_count >> 32); /* MSB */
		cpu_synth->index = 0;
		smp_wmb();	/* Writing in data of CPU about to come up */
		break;
	case CPU_ONLINE:
		/*
		 * FIXME : heartbeat events are currently broken with CPU
		 * hotplug : events can be recorded before heartbeat, heartbeat
		 * too far from trace start and are broken with trace stop/start
		 * as well.
		 */
		/* As we are preemptible, make sure it runs on the right cpu */
		smp_call_function_single(hotcpu, synthetic_tsc_ipi, NULL, 1, 0);
		break;
	}
	return NOTIFY_OK;
}

/* Called from one CPU, before any tracing starts, to init each structure */
static int __init ltt_init_synthetic_tsc(void)
{
	int cpu;
	hotcpu_notifier(hotcpu_callback, 3);
	precalc_stsc_interval();
	init_timer(&stsc_timer);
	stsc_timer.function = stsc_timer_fct;
	stsc_timer.expires = jiffies + precalc_expire;
	add_timer(&stsc_timer);
	return 0;
}

__initcall(ltt_init_synthetic_tsc);
