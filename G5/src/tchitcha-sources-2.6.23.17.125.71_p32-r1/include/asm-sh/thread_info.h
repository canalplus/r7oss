#ifndef __ASM_SH_THREAD_INFO_H
#define __ASM_SH_THREAD_INFO_H

/* SuperH version
 * Copyright (C) 2002  Niibe Yutaka
 *
 * The copyright of original i386 version is:
 *
 *  Copyright (C) 2002  David Howells (dhowells@redhat.com)
 *  - Incorporating suggestions made by Linus Torvalds and Dave Miller
 */
#ifdef __KERNEL__
#include <asm/page.h>

#ifndef __ASSEMBLY__
#include <asm/processor.h>

struct thread_info {
	struct task_struct	*task;		/* main task structure */
	struct exec_domain	*exec_domain;	/* execution domain */
	unsigned long		flags;		/* low level flags */
	__u32			status;		/* thread synchronous flags */
	__u32			cpu;
	int			preempt_count; /* 0 => preemptable, <0 => BUG */
	mm_segment_t		addr_limit;	/* thread address space */
	struct restart_block	restart_block;
	unsigned long		previous_sp;	/* sp of previous stack in case
						   of nested IRQ stacks */
	__u8			supervisor_stack[0];
};

#endif

#define PREEMPT_ACTIVE		0x10000000

#if defined(CONFIG_4KSTACKS)
#define THREAD_SIZE_ORDER	(0)
#elif defined(CONFIG_PAGE_SIZE_4KB)
#define THREAD_SIZE_ORDER	(1)
#elif defined(CONFIG_PAGE_SIZE_8KB)
#define THREAD_SIZE_ORDER	(1)
#elif defined(CONFIG_PAGE_SIZE_64KB)
#define THREAD_SIZE_ORDER	(0)
#else
#error "Unknown thread size"
#endif

#define THREAD_SIZE	(PAGE_SIZE << THREAD_SIZE_ORDER)
#define STACK_WARN	(THREAD_SIZE >> 3)

/*
 * macros/functions for gaining access to the thread information structure
 */
#ifndef __ASSEMBLY__
#define INIT_THREAD_INFO(tsk)			\
{						\
	.task		= &tsk,			\
	.exec_domain	= &default_exec_domain,	\
	.flags		= 0,			\
	.status		= 0,			\
	.cpu		= 0,			\
	.preempt_count	= 1,			\
	.addr_limit	= KERNEL_DS,		\
	.restart_block	= {			\
		.fn = do_no_restart_syscall,	\
	},					\
}

#define init_thread_info	(init_thread_union.thread_info)
#define init_stack		(init_thread_union.stack)

/* how to get the current stack pointer from C */
register unsigned long current_stack_pointer asm("r15") __attribute_used__;

/* how to get the thread information struct from C */
static inline struct thread_info *current_thread_info(void)
{
	struct thread_info *ti;
#ifdef CONFIG_CPU_HAS_SR_RB
	__asm__("stc	r7_bank, %0" : "=r" (ti));
#else
	unsigned long __dummy;

	__asm__ __volatile__ (
		"mov	r15, %0\n\t"
		"and	%1, %0\n\t"
		: "=&r" (ti), "=r" (__dummy)
		: "1" (~(THREAD_SIZE - 1))
		: "memory");
#endif

	return ti;
}

/* thread information allocation */
#ifdef CONFIG_DEBUG_STACK_USAGE
#define alloc_thread_info(ti)	kzalloc(THREAD_SIZE, GFP_KERNEL)
#else
#define alloc_thread_info(ti)	kmalloc(THREAD_SIZE, GFP_KERNEL)
#endif
#define free_thread_info(ti)	kfree(ti)

#endif /* __ASSEMBLY__ */

/*
 * thread information flags
 * - these are process state flags that various assembly files may need to access
 * - pending work-to-be-done flags are in LSW
 * - other flags in MSW
 */
#define TIF_SYSCALL_TRACE	0	/* syscall trace active */
#define TIF_SIGPENDING		1	/* signal pending */
#define TIF_NEED_RESCHED	2	/* rescheduling necessary */
#define TIF_RESTORE_SIGMASK	3	/* restore signal mask in do_signal() */
#define TIF_SINGLESTEP		4	/* singlestepping active */
#define TIF_KERNEL_TRACE	5	/* kernel trace active */
#define TIF_POLLING_NRFLAG	17	/* true if poll_idle() is polling TIF_NEED_RESCHED */
#define TIF_MEMDIE		18
#define TIF_FREEZE		19
#define TIF_UAC_NOPRINT		20	/* PR_UNALIGN_NOPRINT (1) */
#define TIF_UAC_SIGBUS		21	/* PR_UNALIGN_SIGBUS (2) */


#define _TIF_SYSCALL_TRACE	(1<<TIF_SYSCALL_TRACE)
#define _TIF_SIGPENDING		(1<<TIF_SIGPENDING)
#define _TIF_NEED_RESCHED	(1<<TIF_NEED_RESCHED)
#define _TIF_RESTORE_SIGMASK	(1<<TIF_RESTORE_SIGMASK)
#define _TIF_SINGLESTEP		(1<<TIF_SINGLESTEP)
#define _TIF_KERNEL_TRACE	(1<<TIF_KERNEL_TRACE)
#define _TIF_POLLING_NRFLAG	(1<<TIF_POLLING_NRFLAG)
#define _TIF_FREEZE		(1<<TIF_FREEZE)
#define _TIF_UAC_NOPRINT	(1<<TIF_UAC_NOPRINT)
#define _TIF_UAC_SIGBUS		(1<<TIF_UAC_SIGBUS)

#define _TIF_WORK_MASK		0x000000DE	/* work to do on interrupt/exception return */
#define _TIF_ALLWORK_MASK	0x000000FF	/* work to do on any return to u-space */

/* PR_[GS]ET_UNALIGN prctls */
#define SH_UAC_SHIFT		20
#define SH_UAC_MASK		(_TIF_UAC_SIGBUS | _TIF_UAC_NOPRINT)

#define SET_UNALIGN_CTL(task,value)	\
({	\
	task_thread_info(task)->flags =	\
	((task_thread_info(task)->flags & ~SH_UAC_MASK)	\
				| (((value) << SH_UAC_SHIFT) & SH_UAC_MASK));\
	0;	\
})

#define GET_UNALIGN_CTL(task,addr)	\
({	\
	put_user((task_thread_info(task)->flags & SH_UAC_MASK) \
	>> SH_UAC_SHIFT,	\
	(int __user *) (addr));	\
})

/*
 * Thread-synchronous status.
 *
 * This is different from the flags in that nobody else
 * ever touches our thread-synchronous status, so we don't
 * have to worry about atomic accesses.
 */
#define TS_USEDFPU		0x0001	/* FPU used by this task this quantum */

#endif /* __KERNEL__ */

#endif /* __ASM_SH_THREAD_INFO_H */
