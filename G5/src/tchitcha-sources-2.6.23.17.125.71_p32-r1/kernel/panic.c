/*
 *  linux/kernel/panic.c
 *
 *  Copyright (C) 1991, 1992  Linus Torvalds
 */

/*
 * This function is used through-out the kernel (including mm and fs)
 * to indicate a major problem.
 */
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/reboot.h>
#include <linux/notifier.h>
#include <linux/init.h>
#include <linux/sysrq.h>
#include <linux/interrupt.h>
#include <linux/nmi.h>
#include <linux/kexec.h>
#include <linux/debug_locks.h>
#include <asm/io.h>

int panic_on_oops;
int tainted;
static int pause_on_oops;
static int pause_on_oops_flag;
static DEFINE_SPINLOCK(pause_on_oops_lock);

int panic_timeout;

ATOMIC_NOTIFIER_HEAD(panic_notifier_list);

EXPORT_SYMBOL(panic_notifier_list);

static int __init panic_setup(char *str)
{
	panic_timeout = simple_strtoul(str, NULL, 0);
	return 1;
}
__setup("panic=", panic_setup);

static long no_blink(long time)
{
	return 0;
}

#if (defined CONFIG_SERIAL_ST_ASC_CONSOLE) && (defined CONFIG_SERIAL_ST_ASC_BUFFERED)
void PrintkfFlushBuffer (void);
#endif // CONFIG_SERIAL_ST_ASC_CONSOLE & CONFIG_SERIAL_ST_ASC_BUFFERED

/* Returns how long it waited in ms */
long (*panic_blink)(long time);
EXPORT_SYMBOL(panic_blink);

#define ST40_CPG_REGS_BASE 0xffc00000
#define SH4_TMU_REGS_BASE 0xffd80000
typedef volatile unsigned char *const          sh4_byte_reg_t;
typedef volatile unsigned short *const         sh4_word_reg_t;
typedef volatile unsigned int *const           sh4_dword_reg_t;
typedef volatile unsigned long long *const     sh4_gword_reg_t;

#define SH4_BYTE_REG(address)  ((sh4_byte_reg_t) (address))
#define SH4_WORD_REG(address)  ((sh4_word_reg_t) (address))
#define SH4_DWORD_REG(address) ((sh4_dword_reg_t) (address))
#define SH4_GWORD_REG(address) ((sh4_gword_reg_t) (address))

/* IMPORTANT NOTE */
/* 2 metods are provided for CPU reset */
/* 1st method: we use SH4 (ST40) watchdog */
static void sh_reset_WD(void) 
{
#define ST40_CPG_WTCNT  SH4_WORD_REG(ST40_CPG_REGS_BASE + 0x08)
#define ST40_CPG_WTCSR2 SH4_WORD_REG(ST40_CPG_REGS_BASE + 0x1c)
#define ST40_CPG_WTCSR  SH4_WORD_REG(ST40_CPG_REGS_BASE + 0x0c)
   /*
    * We will use the on-chip watchdog timer to force a
    * power-on-reset of the device.
    * A power-on-reset is required to guarantee all SH4-200 cores
    * will reset back into 29-bit mode, if they were in SE mode.
    * However, on SH4-300 series parts, issuing a TRAP instruction
    * with SR.BL=1 is sufficient. However, we will use a "one size fits
    * all" solution here, and use the watchdog for all SH parts.
    */

   /* WTCNT         = FF  counter to overflow next tick */
   *ST40_CPG_WTCNT = 0x5AFF;

   /* WTCSR2.CKS[3]  = 0   use legacy clock dividers */
   *ST40_CPG_WTCSR2 = 0xAA00;

   /* WTCSR2.CKS[3]  = 0   use legacy clock dividers */
   *ST40_CPG_WTCSR2 = 0xAA00;

   /* WTCSR.TME      = 1   enable up-count counter */
   /* WTCSR.WT       = 1   watchdog timer mode */
   /* WTCSR.RSTS     = 0   enable power-on reset */
   /* WTCSR.CKS[2:0] = 2   clock division ratio 1:128 */
   /* NOTE: we need CKS to be big enough to allow
    * U-boot to disable the watchdog, AFTER the reset,
    * otherwise, we enter an infinite-loop of resetting! */
   *ST40_CPG_WTCSR = 0xA5C2;
}

/* 2d method: we use SH4 trap handlfer #0 */

/* fct to stop all 3 available timers inside SH4 */
static void tmu_timer_stop (unsigned int timer)
{
  #define TSTR SH4_BYTE_REG(SH4_TMU_REGS_BASE + 0x04)
  if (timer > 2)
         return;
  writeb(readb(TSTR) & ~(1 << timer), TSTR);
}
void sh_reset_TRAP0(void) 
{
  ulong sr;
  /* Timer Unit control registers (common to all SH4 variants) */
  #define TOCR SH4_BYTE_REG(SH4_TMU_REGS_BASE + 0x00)
  #define TCOR0        SH4_DWORD_REG(SH4_TMU_REGS_BASE + 0x08)
  #define TCNT0        SH4_DWORD_REG(SH4_TMU_REGS_BASE + 0x0c)
  #define TCR0 SH4_WORD_REG(SH4_TMU_REGS_BASE + 0x10)
  #define TCOR1        SH4_DWORD_REG(SH4_TMU_REGS_BASE + 0x14)
  #define TCNT1        SH4_DWORD_REG(SH4_TMU_REGS_BASE + 0x18)
  #define TCR1 SH4_WORD_REG(SH4_TMU_REGS_BASE + 0x1c)
  #define TCOR2        SH4_DWORD_REG(SH4_TMU_REGS_BASE + 0x20)
  #define TCNT2        SH4_DWORD_REG(SH4_TMU_REGS_BASE + 0x24)
  #define TCR2 SH4_WORD_REG(SH4_TMU_REGS_BASE + 0x28)
  #define TCPR2        SH4_DWORD_REG(SH4_TMU_REGS_BASE + 0x2c)

  //printk("%s TCNT0=0x%x\n", __FUNCTION__, readl(TCNT0));
  //printk("%s TCOR0=0x%x\n", __FUNCTION__, readl(TCOR0));
  //printk("%s TCR0=0x%x\n", __FUNCTION__, readl(TCR0));
  /* stop all timers */
  tmu_timer_stop(0);
  tmu_timer_stop(1);
  tmu_timer_stop(2);
  /* reinitialise all counters & status registers */
  writel(0xFFFFFFFF, TCNT0);
  writel(0xFFFFFFFF, TCOR0);
  writew(0, TCR0);
  writel(0xFFFFFFFF, TCNT1);
  writel(0xFFFFFFFF, TCOR1);
  writew(0, TCR1);
  writel(0xFFFFFFFF, TCNT2);
  writel(0xFFFFFFFF, TCOR2);
  writew(0, TCR2);
  //printk("%s TCNT0=0x%x\n", __FUNCTION__, readl(TCNT0));
  //printk("%s TCOR0=0x%x\n", __FUNCTION__, readl(TCOR0));
  //printk("%s TCR0=0x%x\n", __FUNCTION__, readl(TCR0));
    
  /* raise trap #0 */
  asm ("stc sr, %0":"=r" (sr));
  sr |= (1 << 28);       /* set block bit */
  asm ("ldc %0, sr": :"r" (sr));
  asm volatile ("trapa #0");
}
EXPORT_SYMBOL(sh_reset_TRAP0);

#if defined (CONFIG_REBOOT_ON_OOM) || defined (CONFIG_REBOOT_ON_PANIC) || defined (CONFIG_REBOOT_ON_SIGNAL) || defined (CONFIG_REBOOT_ON_OOPS)


void sh_reset (void) __attribute__ ((noreturn));
void sh_reset (void)
{
  /* 2 metods are provided for CPU reset */
  /* 1st method: we use SH4 (ST40) watchdog */
  //sh_reset_WD();
  /* 2d method: we use SH4 trap handlfer #0 */
  sh_reset_TRAP0();
   /* wait for H/W reset to kick in ... */
   for (;;);
}
EXPORT_SYMBOL(sh_reset);
#else
void sh_reset (void) __attribute__ ((noreturn));
void sh_reset (void)
{
   for (;;);
}
#endif

/**
 *	panic - halt the system
 *	@fmt: The text string to print
 *
 *	Display a message, then perform cleanups.
 *
 *	This function never returns.
 */

#ifdef CONFIG_FULL_PANIC
NORET_TYPE void panic(const char * fmt, ...)
{
	static char buf[1024];
	va_list args;
#else
NORET_TYPE void tiny_panic(int a, ...)
{
#endif
	long i;
#if defined (CONFIG_REBOOT_ON_PANIC)
	printk("Kernel PANIC Detected\n");
#if (defined CONFIG_SERIAL_ST_ASC_CONSOLE) && (defined CONFIG_SERIAL_ST_ASC_BUFFERED)
	PrintkfFlushBuffer();
#endif
	sh_reset();
#endif // CONFIG_REBOOT_ON_PANIC

#if defined(CONFIG_S390)
        unsigned long caller = (unsigned long) __builtin_return_address(0);
#endif

	/*
	 * It's possible to come here directly from a panic-assertion and not
	 * have preempt disabled. Some functions called from here want
	 * preempt to be disabled. No point enabling it later though...
	 */
	preempt_disable();

	bust_spinlocks(1);

#ifdef CONFIG_FULL_PANIC
	va_start(args, fmt);
	vsnprintf(buf, sizeof(buf), fmt, args);
	va_end(args);
	printk(KERN_EMERG "Kernel panic - not syncing: %s\n",buf);
#else
	printk(KERN_EMERG "Kernel panic - not syncing\n");
#endif

	bust_spinlocks(0);

	/*
	 * If we have crashed and we have a crash kernel loaded let it handle
	 * everything else.
	 * Do we want to call this before we try to display a message?
	 */
	crash_kexec(NULL);

#ifdef CONFIG_SMP
	/*
	 * Note smp_send_stop is the usual smp shutdown function, which
	 * unfortunately means it may not be hardened to work in a panic
	 * situation.
	 */
	smp_send_stop();
#endif

#ifdef CONFIG_FULL_PANIC
	atomic_notifier_call_chain(&panic_notifier_list, 0, buf);
#else
	atomic_notifier_call_chain(&panic_notifier_list, 0, "");
#endif

	if (!panic_blink)
		panic_blink = no_blink;

	if (panic_timeout > 0) {
		/*
	 	 * Delay timeout seconds before rebooting the machine. 
		 * We can't use the "normal" timers since we just panicked..
	 	 */
		printk(KERN_EMERG "Rebooting in %d seconds..",panic_timeout);
		for (i = 0; i < panic_timeout*1000; ) {
			touch_nmi_watchdog();
			i += panic_blink(i);
			mdelay(1);
			i++;
		}
		/*	This will not be a clean reboot, with everything
		 *	shutting down.  But if there is a chance of
		 *	rebooting the system it will be rebooted.
		 */
		emergency_restart();
	}
#ifdef __sparc__
	{
		extern int stop_a_enabled;
		/* Make sure the user can actually press Stop-A (L1-A) */
		stop_a_enabled = 1;
		printk(KERN_EMERG "Press Stop-A (L1-A) to return to the boot prom\n");
	}
#endif
#if defined(CONFIG_S390)
        disabled_wait(caller);
#endif
	local_irq_enable();
	for (i = 0;;) {
		touch_softlockup_watchdog();
		i += panic_blink(i);
		mdelay(1);
		i++;
	}
}

#ifdef CONFIG_FULL_PANIC
EXPORT_SYMBOL(panic);
#else
EXPORT_SYMBOL(tiny_panic);
#endif

/**
 *	print_tainted - return a string to represent the kernel taint state.
 *
 *  'P' - Proprietary module has been loaded.
 *  'F' - Module has been forcibly loaded.
 *  'S' - SMP with CPUs not designed for SMP.
 *  'R' - User forced a module unload.
 *  'M' - Machine had a machine check experience.
 *  'B' - System has hit bad_page.
 *  'U' - Userspace-defined naughtiness.
 *
 *	The string is overwritten by the next call to print_taint().
 */
 
const char *print_tainted(void)
{
	static char buf[20];
	if (tainted) {
		snprintf(buf, sizeof(buf), "Tainted: %c%c%c%c%c%c%c%c",
			tainted & TAINT_PROPRIETARY_MODULE ? 'P' : 'G',
			tainted & TAINT_FORCED_MODULE ? 'F' : ' ',
			tainted & TAINT_UNSAFE_SMP ? 'S' : ' ',
			tainted & TAINT_FORCED_RMMOD ? 'R' : ' ',
 			tainted & TAINT_MACHINE_CHECK ? 'M' : ' ',
			tainted & TAINT_BAD_PAGE ? 'B' : ' ',
			tainted & TAINT_USER ? 'U' : ' ',
			tainted & TAINT_DIE ? 'D' : ' ');
	}
	else
		snprintf(buf, sizeof(buf), "Not tainted");
	return(buf);
}

void add_taint(unsigned flag)
{
	debug_locks = 0; /* can't trust the integrity of the kernel anymore */
	tainted |= flag;
}
EXPORT_SYMBOL(add_taint);

static int __init pause_on_oops_setup(char *str)
{
	pause_on_oops = simple_strtoul(str, NULL, 0);
	return 1;
}
__setup("pause_on_oops=", pause_on_oops_setup);

static void spin_msec(int msecs)
{
	int i;

	for (i = 0; i < msecs; i++) {
		touch_nmi_watchdog();
		mdelay(1);
	}
}

/*
 * It just happens that oops_enter() and oops_exit() are identically
 * implemented...
 */
static void do_oops_enter_exit(void)
{
	unsigned long flags;
	static int spin_counter;
#ifdef CONFIG_REBOOT_ON_OOPS
        printk("Kernel OOPS Detected\n");
#if (defined CONFIG_SERIAL_ST_ASC_CONSOLE) && (defined CONFIG_SERIAL_ST_ASC_BUFFERED)
        PrintkfFlushBuffer ();
        dump_stack ();
#endif // CONFIG_SERIAL_ST_ASC_CONSOLE & CONFIG_SERIAL_ST_ASC_BUFFERED
	sh_reset();
#endif // CONFIG_REBOOT_ON_OOPS

	if (!pause_on_oops)
		return;

	spin_lock_irqsave(&pause_on_oops_lock, flags);
	if (pause_on_oops_flag == 0) {
		/* This CPU may now print the oops message */
		pause_on_oops_flag = 1;
	} else {
		/* We need to stall this CPU */
		if (!spin_counter) {
			/* This CPU gets to do the counting */
			spin_counter = pause_on_oops;
			do {
				spin_unlock(&pause_on_oops_lock);
				spin_msec(MSEC_PER_SEC);
				spin_lock(&pause_on_oops_lock);
			} while (--spin_counter);
			pause_on_oops_flag = 0;
		} else {
			/* This CPU waits for a different one */
			while (spin_counter) {
				spin_unlock(&pause_on_oops_lock);
				spin_msec(1);
				spin_lock(&pause_on_oops_lock);
			}
		}
	}
	spin_unlock_irqrestore(&pause_on_oops_lock, flags);
}

/*
 * Return true if the calling CPU is allowed to print oops-related info.  This
 * is a bit racy..
 */
int oops_may_print(void)
{
	return pause_on_oops_flag == 0;
}

/*
 * Called when the architecture enters its oops handler, before it prints
 * anything.  If this is the first CPU to oops, and it's oopsing the first time
 * then let it proceed.
 *
 * This is all enabled by the pause_on_oops kernel boot option.  We do all this
 * to ensure that oopses don't scroll off the screen.  It has the side-effect
 * of preventing later-oopsing CPUs from mucking up the display, too.
 *
 * It turns out that the CPU which is allowed to print ends up pausing for the
 * right duration, whereas all the other CPUs pause for twice as long: once in
 * oops_enter(), once in oops_exit().
 */
void oops_enter(void)
{
	debug_locks_off(); /* can't trust the integrity of the kernel anymore */
	do_oops_enter_exit();
}

/*
 * Called when the architecture exits its oops handler, after printing
 * everything.
 */
void oops_exit(void)
{
	do_oops_enter_exit();
}

#ifdef CONFIG_CC_STACKPROTECTOR
/*
 * Called when gcc's -fstack-protector feature is used, and
 * gcc detects corruption of the on-stack canary value
 */
void __stack_chk_fail(void)
{
	panic("stack-protector: Kernel stack is corrupted");
}
EXPORT_SYMBOL(__stack_chk_fail);
#endif
