/*
 * Test TSC synchronization
 */
#include <linux/module.h>
#include <linux/timer.h>
#include <linux/timex.h>
#include <linux/jiffies.h>
#include <linux/cpu.h>
#include <linux/kthread.h>
#include <linux/mutex.h>

/* Workaround for 32 bits archs which does not define this */
#ifdef CONFIG_X86_32
#define get_cycles_sync get_cycles
#endif

#define MAX_CYCLES_DELTA 1000ULL

static DEFINE_PER_CPU(cycles_t, tsc_count);
static DEFINE_MUTEX(tscsync_mutex);

static DEFINE_PER_CPU(int, wait_sync);
static DEFINE_PER_CPU(int, wait_end_sync);

int ltt_tsc_is_sync = 1;
EXPORT_SYMBOL(ltt_tsc_is_sync);

cycles_t ltt_last_tsc;
EXPORT_SYMBOL(ltt_last_tsc);

/*
 * Mark it noinline so we make sure it is not unrolled.
 * Wait until value is reached.
 */
static noinline void tsc_barrier(long wait_cpu, int value)
{
	sync_core();
	per_cpu(wait_sync, smp_processor_id())--;
	do {
		barrier();
	} while (unlikely(per_cpu(wait_sync, wait_cpu) > value));
	__get_cpu_var(tsc_count) = get_cycles_sync();
}

/*
 * Worker thread called on each CPU.
 * First wait with interrupts enabled, then wait with interrupt disabled,
 * for precision. We are already bound to one CPU.
 */
static void test_sync(void *arg)
{
	long wait_cpu = (long)arg;
	unsigned long flags;

	local_irq_save(flags);
	/* Make sure the instructions are in I-CACHE */
	tsc_barrier(wait_cpu, 1);
	tsc_barrier(wait_cpu, 0);
	per_cpu(wait_end_sync, smp_processor_id())--;
	do {
		barrier();
	} while (unlikely(per_cpu(wait_end_sync, wait_cpu) > 0));
	local_irq_restore(flags);
}

/*
 * Do loops (making sure no unexpected event changes the timing), keep the
 * best one. The result of each loop is the highest tsc delta between the
 * master CPU and the slaves.
 */
static int test_tsc_synchronization(void)
{
	long cpu, master;
	cycles_t max_diff = 0, diff, best_loop, worse_loop = 0;
	int i;

	mutex_lock(&tscsync_mutex);
	preempt_disable();
	master = smp_processor_id();
	for_each_online_cpu(cpu) {
		if (master == cpu)
			continue;
		best_loop = ULLONG_MAX;
		for (i = 0; i < 10; i++) {
			/*
			 * Each CPU (master and slave) must decrement the
			 * wait_sync value twice (one for priming in cache).
			 */
			per_cpu(wait_sync, master) = 2;
			per_cpu(wait_sync, cpu) = 2;
			per_cpu(wait_end_sync, master) = 1;
			per_cpu(wait_end_sync, cpu) = 1;
			smp_call_function_single(cpu, test_sync,
						(void *)master, 1, 0);
			test_sync((void *)cpu);
			diff = abs(per_cpu(tsc_count, cpu)
				- per_cpu(tsc_count, master));
			best_loop = min(best_loop, diff);
			worse_loop = max(worse_loop, diff);
		}
		max_diff = max(best_loop, max_diff);
	}
	preempt_enable();
	if (max_diff >= MAX_CYCLES_DELTA) {
		printk(KERN_WARNING
			"LTTng : Your timestamp counter is not reliable.\n"
			"See LTTng documentation to find the "
			"appropriate solution for your architecture.\n");
		printk("TSC unsynchronized : %llu cycles delta is over "
			"threshold %llu\n", max_diff, MAX_CYCLES_DELTA);
	}
	mutex_unlock(&tscsync_mutex);
	return max_diff < MAX_CYCLES_DELTA;
}
EXPORT_SYMBOL_GPL(test_tsc_synchronization);

static int __init tsc_test_init(void)
{
	ltt_tsc_is_sync = test_tsc_synchronization();
	return 0;
}

__initcall(tsc_test_init);
