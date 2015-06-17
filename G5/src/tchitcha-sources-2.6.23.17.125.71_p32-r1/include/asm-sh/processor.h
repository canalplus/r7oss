/*
 * include/asm-sh/processor.h
 *
 * Copyright (C) 1999, 2000  Niibe Yutaka
 * Copyright (C) 2002, 2003  Paul Mundt
 */

#ifndef __ASM_SH_PROCESSOR_H
#define __ASM_SH_PROCESSOR_H
#ifdef __KERNEL__

#include <linux/compiler.h>
#include <asm/page.h>
#include <asm/types.h>
#include <asm/cache.h>
#include <asm/ptrace.h>
#include <asm/cpu-features.h>

/*
 * Default implementation of macro that returns current
 * instruction pointer ("program counter").
 */
#define current_text_addr() ({ void *pc; __asm__("mova	1f, %0\n.align 2\n1:":"=z" (pc)); pc; })

/* Core Processor Version Register */
#define CCN_PVR		0xff000030
#define CCN_CVR		0xff000040
#define CCN_PRR		0xff000044
#define CCN_RAMCR	0xff000074	/* ST40-300 */

/*
 *  CPU type and hardware bug flags. Kept separately for each CPU.
 *
 *  Each one of these also needs a CONFIG_CPU_SUBTYPE_xxx entry
 *  in arch/sh/mm/Kconfig, as well as an entry in arch/sh/kernel/setup.c
 *  for parsing the subtype in get_cpu_subtype().
 */
enum cpu_type {
	/* SH-2 types */
	CPU_SH7619,

	/* SH-2A types */
	CPU_SH7206,

	/* SH-3 types */
	CPU_SH7705, CPU_SH7706, CPU_SH7707,
	CPU_SH7708, CPU_SH7708S, CPU_SH7708R,
	CPU_SH7709, CPU_SH7709A, CPU_SH7710, CPU_SH7712,
	CPU_SH7720, CPU_SH7729,

	/* SH-4 types */
	CPU_SH7750, CPU_SH7750S, CPU_SH7750R, CPU_SH7751, CPU_SH7751R,
	CPU_SH7760,
	CPU_FLI7510, CPU_FLI7520, CPU_FLI7530, CPU_FLI7540,
	CPU_ST40RA, CPU_ST40GX1, CPU_STI5528, CPU_STM8000,
	CPU_STX5197, CPU_STX5206,
	CPU_STB7100, CPU_STX7105, CPU_STX7106, CPU_STX7108, CPU_STB7109,
	CPU_STX7111, CPU_STX7141,
	CPU_STX7200,
	CPU_SH4_202, CPU_SH4_501,

	/* SH-4A types */
	CPU_SH7770, CPU_SH7780, CPU_SH7781, CPU_SH7785, CPU_SHX3,

	/* SH4AL-DSP types */
	CPU_SH7343, CPU_SH7722,

	/* Unknown subtype */
	CPU_SH_NONE
};

struct sh_cpuinfo {
	unsigned int type;
	int cut_major, cut_minor;
	unsigned long loops_per_jiffy;
	unsigned long asid_cache;

	struct cache_info icache;	/* Primary I-cache */
	struct cache_info dcache;	/* Primary D-cache */
	struct cache_info scache;	/* Secondary cache */

	unsigned long flags;
} __attribute__ ((aligned(SMP_CACHE_BYTES)));

extern struct sh_cpuinfo cpu_data[];
#define boot_cpu_data cpu_data[0]
#define current_cpu_data cpu_data[smp_processor_id()]
#define raw_current_cpu_data cpu_data[raw_smp_processor_id()]

/*
 * User space process size: 2GB.
 *
 * Since SH7709 and SH7750 have "area 7", we can't use 0x7c000000--0x7fffffff
 */
#define TASK_SIZE	0x7c000000UL

/* This decides where the kernel will search for a free chunk of vm
 * space during mmap's.
 */
#define TASK_UNMAPPED_BASE	(TASK_SIZE / 3)

/*
 * Bit of SR register
 *
 * FD-bit:
 *     When it's set, it means the processor doesn't have right to use FPU,
 *     and it results exception when the floating operation is executed.
 *
 * IMASK-bit:
 *     Interrupt level mask
 */
#define SR_MD		0x40000000
#define SR_FD		0x00008000
#define SR_DSP		0x00001000
#define SR_IMASK	0x000000f0

/*
 * FPU structure and data
 */

struct sh_fpu_hard_struct {
	unsigned long fp_regs[16];
	unsigned long xfp_regs[16];
	unsigned long fpscr;
	unsigned long fpul;

	long status; /* software status information */
};

/* Dummy fpu emulator  */
struct sh_fpu_soft_struct {
	unsigned long fp_regs[16];
	unsigned long xfp_regs[16];
	unsigned long fpscr;
	unsigned long fpul;

	unsigned char lookahead;
	unsigned long entry_pc;
};

union sh_fpu_union {
	struct sh_fpu_hard_struct hard;
	struct sh_fpu_soft_struct soft;
};

struct thread_struct {
	/* Saved registers when thread is descheduled */
	unsigned long sp;
	unsigned long pc;

	/* Hardware debugging registers */
	unsigned long ubc_pc;

	/* floating point info */
	union sh_fpu_union fpu;
};

typedef struct {
	unsigned long seg;
} mm_segment_t;

/* Count of active tasks with UBC settings */
extern int ubc_usercnt;

#define INIT_THREAD  {						\
	.sp = sizeof(init_stack) + (long) &init_stack,		\
}

/*
 * Do necessary setup to start up a newly executed thread.
 */
#define start_thread(regs, new_pc, new_sp)	 \
	set_fs(USER_DS);			 \
	regs->pr = 0;				 \
	regs->sr = SR_FD;	/* User mode. */ \
	regs->pc = new_pc;			 \
	regs->regs[15] = new_sp

/* Forward declaration, a strange C thing */
struct task_struct;
struct mm_struct;

/* Free all resources held by a thread. */
extern void release_thread(struct task_struct *);

/* Prepare to copy thread state - unlazy all lazy status */
void prepare_to_copy(struct task_struct *tsk);

/*
 * create a kernel thread without removing it from tasklists
 */
extern int kernel_thread(int (*fn)(void *), void * arg, unsigned long flags);

/* Copy and release all segment info associated with a VM */
#define copy_segments(p, mm)	do { } while(0)
#define release_segments(mm)	do { } while(0)


/* Double presision, NANS as NANS, rounding to nearest, no exceptions */
#define FPSCR_INIT  0x00080000

#define	FPSCR_CAUSE_MASK	0x0001f000	/* Cause bits */
#define	FPSCR_FLAG_MASK		0x0000007c	/* Flag bits */

/*
 * Return saved PC of a blocked thread.
 */
#define thread_saved_pc(tsk)	(tsk->thread.pc)

void show_trace(struct task_struct *tsk, unsigned long *sp,
		struct pt_regs *regs);
extern unsigned long get_wchan(struct task_struct *p);

#define KSTK_EIP(tsk)  (task_pt_regs(tsk)->pc)
#define KSTK_ESP(tsk)  (task_pt_regs(tsk)->regs[15])

#define cpu_sleep()	__asm__ __volatile__ ("sleep" : : : "memory")
#define cpu_relax()	barrier()

#if defined(CONFIG_CPU_SH2A) || defined(CONFIG_CPU_SH3) || \
    defined(CONFIG_CPU_SH4)
#define PREFETCH_STRIDE		L1_CACHE_BYTES
#define ARCH_HAS_PREFETCH
#define ARCH_HAS_PREFETCHW
static inline void prefetch(void *x)
{
	__asm__ __volatile__ ("pref @%0\n\t" : : "r" (x) : "memory");
}

#define prefetchw(x)	prefetch(x)
#endif

#ifdef CONFIG_VSYSCALL
extern int vsyscall_init(void);
#else
#define vsyscall_init() do { } while (0)
#endif

/* arch/sh/kernel/setup.c */
const char *get_cpu_subtype(struct sh_cpuinfo *c);

#endif /* __KERNEL__ */
#endif /* __ASM_SH_PROCESSOR_H */
