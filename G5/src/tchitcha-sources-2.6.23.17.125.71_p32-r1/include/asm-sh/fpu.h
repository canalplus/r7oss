#ifndef __ASM_SH_FPU_H
#define __ASM_SH_FPU_H

/*
 * FPU lazy state save handling.
 */

static __inline__ void disable_fpu(void)
{
	unsigned long __dummy;

	/* Set FD flag in SR */
	__asm__ __volatile__("stc	sr, %0\n\t"
			     "or	%1, %0\n\t"
			     "ldc	%0, sr"
			     : "=&r" (__dummy)
			     : "r" (SR_FD));
}

static __inline__ void enable_fpu(void)
{
	unsigned long __dummy;

	/* Clear out FD flag in SR */
	__asm__ __volatile__("stc	sr, %0\n\t"
			     "and	%1, %0\n\t"
			     "ldc	%0, sr"
			     : "=&r" (__dummy)
			     : "r" (~SR_FD));
}

static __inline__ void release_fpu(struct pt_regs *regs)
{
	regs->sr |= SR_FD;
}

static __inline__ void grab_fpu(struct pt_regs *regs)
{
	regs->sr &= ~SR_FD;
}

void save_fpu(struct task_struct *tsk);
void fpu_state_restore(struct pt_regs *regs);

static inline void __unlazy_fpu(struct task_struct *tsk, struct pt_regs *regs)
{
	if (task_thread_info(tsk)->status & TS_USEDFPU) {
		task_thread_info(tsk)->status &= ~TS_USEDFPU;
		save_fpu(tsk);
		release_fpu(regs);
	} else
		tsk->fpu_counter = 0;
}

static inline void clear_fpu(struct task_struct *tsk, struct pt_regs *regs)
{
	if (task_thread_info(tsk)->status & TS_USEDFPU) {
		task_thread_info(tsk)->status &= ~TS_USEDFPU;
		release_fpu(regs);
	}
}

static inline void unlazy_fpu(struct task_struct *tsk, struct pt_regs *regs)
{
	preempt_disable();
	__unlazy_fpu(tsk, regs);
	preempt_enable();
}

#endif /* __ASM_SH_FPU_H */
