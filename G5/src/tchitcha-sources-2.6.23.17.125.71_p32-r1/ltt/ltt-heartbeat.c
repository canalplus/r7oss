/*
 * (C) Copyright	2006 -
 * 		Mathieu Desnoyers (mathieu.desnoyers@polymtl.ca)
 *
 * notes : heartbeat timer cannot be used for early tracing in the boot process,
 * as it depends on timer interrupts.
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
#include <linux/ltt-core.h>
#include <linux/workqueue.h>
#include <linux/cpu.h>
#include <linux/timex.h>
#include <linux/bitops.h>
#include <linux/marker.h>
#include <linux/ltt-tracer.h>

#define BITS_OF_COMPACT_DATA		11

/* Expected minimum probe duration, in cycles */
#define MIN_PROBE_DURATION		400
/* Expected maximum interrupt latency in ms : 15ms, *2 for security */
#define EXPECTED_INTERRUPT_LATENCY	30

static struct timer_list heartbeat_timer;
static unsigned int precalc_heartbeat_expire;

int ltt_compact_data_shift;
EXPORT_SYMBOL_GPL(ltt_compact_data_shift);

int ltt_tsc_lsb_truncate;	/* Number of LSB removed from compact tsc */
EXPORT_SYMBOL_GPL(ltt_tsc_lsb_truncate);
int ltt_tscbits;		/* 32 - tsc_lsb_truncate - tsc_msb_cutoff */
EXPORT_SYMBOL_GPL(ltt_tscbits);

static void heartbeat_ipi(void *info)
{
	struct ltt_probe_private_data call_data;

	call_data.trace = NULL;
	call_data.force = 0;
	call_data.id = MARKER_ID_HEARTBEAT_32;
	/* Log a heartbeat event for each trace, each tracefile */
	for (call_data.channel = ltt_channel_index_begin();
			call_data.channel < ltt_channel_index_end();
			call_data.channel += ltt_channel_index_size())
		__trace_mark(0, core_heartbeat_32, &call_data,
				MARK_NOARGS);
}

static void heartbeat_ipi_full(void *info)
{
	struct ltt_probe_private_data call_data;

	call_data.trace = info;
	call_data.force = 1;
	call_data.id = MARKER_ID_HEARTBEAT_64;
	call_data.cpu = -1;
	/* Log a heartbeat event for each trace, each tracefile */
	for (call_data.channel = ltt_channel_index_begin();
			call_data.channel < ltt_channel_index_end();
			call_data.channel += ltt_channel_index_size())
		__trace_mark(0, core_heartbeat_64, &call_data,
				"timestamp #8u%llu", ltt_get_timestamp64());
}

/*
 * Write the full TSC in the traces. To be called when we cannot assume that the
 * heartbeat events will keep a trace buffer synchronized. (missing timer
 * events, tracing starts, tracing restarts).
 */
void ltt_write_full_tsc(struct ltt_trace_struct *trace)
{
	on_each_cpu(heartbeat_ipi_full, trace, 1, 1);
}
EXPORT_SYMBOL_GPL(ltt_write_full_tsc);

/* We need to be in process context to do an IPI */
static void heartbeat_work(struct work_struct *work)
{
	on_each_cpu(heartbeat_ipi, NULL, 1, 0);
}

static DECLARE_WORK(hb_work, heartbeat_work);

/*
 * heartbeat_timer : - Timer function generating heartbeat.
 * @data: unused
 *
 * Guarantees at least 1 execution of heartbeat before low word of TSC wraps.
 */
static void heartbeat_timer_fct(unsigned long data)
{
	PREPARE_WORK(&hb_work, heartbeat_work);
	schedule_work(&hb_work);

	mod_timer(&heartbeat_timer, jiffies + precalc_heartbeat_expire);
}

/*
 * init_heartbeat_timer: - Start timer generating heartbeat events.
 */
static void init_heartbeat_timer(void)
{
	int tsc_msb_cutoff;
	int data_bits = BITS_OF_COMPACT_DATA;
	int max_tsc_msb_cutoff;
	unsigned long mask;

	if (loops_per_jiffy > 0) {
		printk(KERN_DEBUG "LTT : ltt-heartbeat init\n");
		printk(KERN_DEBUG "Requested number of bits %d\n", data_bits);

		ltt_tsc_lsb_truncate = max(0,
			(int)get_count_order(MIN_PROBE_DURATION)-1);
		max_tsc_msb_cutoff =
			32 - 1 - get_count_order(((1UL << 1)
			+ (EXPECTED_INTERRUPT_LATENCY*HZ/1000)
			+ LTT_PERCPU_TIMER_INTERVAL + 1)
			* (loops_per_jiffy << 1));
		printk(KERN_DEBUG "Available number of bits %d\n",
			ltt_tsc_lsb_truncate + max_tsc_msb_cutoff);
		if (ltt_tsc_lsb_truncate + max_tsc_msb_cutoff <
			data_bits) {
			printk(KERN_DEBUG "Number of bits truncated to %d\n",
				ltt_tsc_lsb_truncate + max_tsc_msb_cutoff);
			data_bits = ltt_tsc_lsb_truncate + max_tsc_msb_cutoff;
		}

		tsc_msb_cutoff = data_bits - ltt_tsc_lsb_truncate;

		if (tsc_msb_cutoff > 0)
			mask = (1UL<<(32-tsc_msb_cutoff))-1;
		else
			mask = 0xFFFFFFFFUL;
		precalc_heartbeat_expire =
			(mask/(loops_per_jiffy << 1)
				- 1 - LTT_PERCPU_TIMER_INTERVAL
				- (EXPECTED_INTERRUPT_LATENCY*HZ/1000)) >> 1;
		WARN_ON(precalc_heartbeat_expire == 0);
		printk(KERN_DEBUG
			"Heartbeat timer will fire each %u jiffies.\n",
			precalc_heartbeat_expire);

		ltt_tscbits = 32 - ltt_tsc_lsb_truncate - tsc_msb_cutoff;
		printk(KERN_DEBUG
			"Compact TSC init : truncate %d lsb, cutoff %d msb.\n",
			ltt_tsc_lsb_truncate, tsc_msb_cutoff);
	} else
		printk(KERN_WARNING
			"LTT: no tsc for heartbeat timer "
			"- continuing without one \n");
}

/*
 * ltt_init_compact_markers reserves the number of bits to identify the event
 * numbers in the compact headers. It must be called every time the compact
 * markers are changed.
 */
void ltt_init_compact_markers(unsigned int num_compact_events)
{
	if (loops_per_jiffy > 0) {
		ltt_compact_data_shift =
			get_count_order(num_compact_events) + ltt_tscbits;
		printk(KERN_DEBUG "Data shifted from %d bits\n",
			ltt_compact_data_shift);

		printk(KERN_DEBUG "%d bits used for event IDs, "
			"%d available for data.\n",
			get_count_order(num_compact_events),
			32 - ltt_compact_data_shift);
	} else
		printk(KERN_WARNING
			"LTT: no tsc for heartbeat timer "
			"- continuing without one \n");
}
EXPORT_SYMBOL_GPL(ltt_init_compact_markers);


static void start_heartbeat_timer(void)
{
	if (precalc_heartbeat_expire > 0) {
		printk(KERN_DEBUG "LTT : ltt-heartbeat start\n");

		init_timer(&heartbeat_timer);
		heartbeat_timer.function = heartbeat_timer_fct;
		heartbeat_timer.expires = jiffies + precalc_heartbeat_expire;
		add_timer(&heartbeat_timer);
	} else
		printk(KERN_WARNING
			"LTT: no tsc for heartbeat timer "
			"- continuing without one \n");
}

/*
 * stop_heartbeat_timer: - Stop timer generating heartbeat events.
 */
static void stop_heartbeat_timer(void)
{
	if (loops_per_jiffy > 0) {
		printk(KERN_DEBUG "LTT : ltt-heartbeat stop\n");
		del_timer(&heartbeat_timer);
	}
}

/*
 * 	heartbeat_hotcpu_full - CPU hotplug heartbeat
 * 	@cpu: CPU number
 *
 * 	Writes an heartbeat event in each traces, in forced mode, on behalf of
 * 	the CPU soon to be brought online. Events are written by another CPU in
 * 	buffers not actually used by the new comer yet.
 */

static void __cpuinit heartbeat_hotcpu_full(int cpu)
{
	struct ltt_probe_private_data call_data;

	call_data.trace = NULL;
	call_data.force = 1;
	call_data.id = MARKER_ID_HEARTBEAT_64;
	call_data.cpu = cpu;
	/* Log a heartbeat event for each trace, each tracefile */
	for (call_data.channel = ltt_channel_index_begin();
			call_data.channel < ltt_channel_index_end();
			call_data.channel += ltt_channel_index_size())
		__trace_mark(0, core_heartbeat_64, &call_data,
				"timestamp #8u%llu", ltt_get_timestamp64());
	/*
	 * Make sure the data is consistent for the CPU being brought online
	 * soon.
	 */
	smp_wmb();
}

/*
 * 	heartbeat_hotcpu_callback - CPU hotplug callback
 * 	@nb: notifier block
 * 	@action: hotplug action to take
 * 	@hcpu: CPU number
 *
 * 	Returns the success/failure of the operation. (NOTIFY_OK, NOTIFY_BAD)
 */
static int __cpuinit heartbeat_hotcpu_callback(struct notifier_block *nb,
				unsigned long action,
				void *hcpu)
{
	unsigned int hotcpu = (unsigned long)hcpu;

	switch (action) {
	case CPU_UP_PREPARE:
	case CPU_UP_PREPARE_FROZEN:
		heartbeat_hotcpu_full(hotcpu);
		break;
	case CPU_ONLINE:
		break;
	}
	return NOTIFY_OK;
}

int ltt_heartbeat_trigger(enum ltt_heartbeat_functor_msg msg)
{
	printk(KERN_DEBUG "LTT : ltt-heartbeat trigger\n");
	switch (msg) {
	case LTT_HEARTBEAT_START:
		start_heartbeat_timer();
		break;
	case LTT_HEARTBEAT_STOP:
		stop_heartbeat_timer();
		break;
	}
	return 0;
}
EXPORT_SYMBOL_GPL(ltt_heartbeat_trigger);

static int __init ltt_heartbeat_init(void)
{
	printk(KERN_INFO "LTT : ltt-heartbeat init\n");
	/* lower priority than relay, lower than synthetic tsc */
	hotcpu_notifier(heartbeat_hotcpu_callback, 2);
	init_heartbeat_timer();
	return 0;
}

__initcall(ltt_heartbeat_init);
