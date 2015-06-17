#ifndef __ASM_SH_SYSTEM_H
#define __ASM_SH_SYSTEM_H

/*
 * Copyright (C) 1999, 2000  Niibe Yutaka  &  Kaz Kojima
 * Copyright (C) 2002 Paul Mundt
 */

#include <linux/irqflags.h>
#include <linux/compiler.h>
#include <linux/linkage.h>
#include <asm/types.h>
#include <asm/ptrace.h>

struct task_struct *__switch_to(struct task_struct *prev,
				struct task_struct *next);

/*
 *	switch_to() should switch tasks to task nr n, first
 */

#define switch_to(prev, next, last) do {				\
 struct task_struct *__last;						\
 register unsigned long *__ts1 __asm__ ("r1") = &prev->thread.sp;	\
 register unsigned long *__ts2 __asm__ ("r2") = &prev->thread.pc;	\
 register unsigned long *__ts4 __asm__ ("r4") = (unsigned long *)prev;	\
 register unsigned long *__ts5 __asm__ ("r5") = (unsigned long *)next;	\
 register unsigned long *__ts6 __asm__ ("r6") = &next->thread.sp;	\
 register unsigned long __ts7 __asm__ ("r7") = next->thread.pc;		\
 __asm__ __volatile__ (".balign 4\n\t" 					\
		       "__switch_to_begin:\n\t"				\
		       "stc.l	gbr, @-r15\n\t" 			\
		       "sts.l	pr, @-r15\n\t" 				\
		       "mov.l	r8, @-r15\n\t" 				\
		       "mov.l	r9, @-r15\n\t" 				\
		       "mov.l	r10, @-r15\n\t" 			\
		       "mov.l	r11, @-r15\n\t" 			\
		       "mov.l	r12, @-r15\n\t" 			\
		       "mov.l	r13, @-r15\n\t" 			\
		       "mov.l	r14, @-r15\n\t" 			\
		       "mov.l	r15, @r1	! save SP\n\t"		\
		       "mov.l	@r6, r15	! change to new stack\n\t" \
		       "mova	1f, %0\n\t" 				\
		       "mov.l	%0, @r2		! save PC\n\t" 		\
		       "mov.l	2f, %0\n\t" 				\
		       "jmp	@%0		! call __switch_to\n\t" \
		       " lds	r7, pr		!  with return to new PC\n\t" \
		       ".balign	4\n"					\
		       "2:\n\t"						\
		       ".long	__switch_to\n"				\
		       "1:\n\t"						\
		       "mov.l	@r15+, r14\n\t"				\
		       "mov.l	@r15+, r13\n\t"				\
		       "mov.l	@r15+, r12\n\t"				\
		       "mov.l	@r15+, r11\n\t"				\
		       "mov.l	@r15+, r10\n\t"				\
		       "mov.l	@r15+, r9\n\t"				\
		       "mov.l	@r15+, r8\n\t"				\
		       "lds.l	@r15+, pr\n\t"				\
		       "ldc.l	@r15+, gbr\n\t"				\
		       "__switch_to_end:\n\t"				\
		       : "=z" (__last)					\
		       : "r" (__ts1), "r" (__ts2), "r" (__ts4), 	\
			 "r" (__ts5), "r" (__ts6), "r" (__ts7) 		\
		       : "r3", "t");					\
	last = __last;							\
} while (0)

#ifdef CONFIG_CPU_SH4A
#define __icbi()			\
{					\
	unsigned long __addr;		\
	__addr = 0xa8000000;		\
	__asm__ __volatile__(		\
		"icbi   %0\n\t"		\
		: /* no output */	\
		: "m" (__m(__addr)));	\
}
#endif

/*
 * A brief note on ctrl_barrier(), the control register write barrier.
 *
 * Legacy SH cores typically require a sequence of 8 nops after
 * modification of a control register in order for the changes to take
 * effect. On newer cores (like the sh4a and sh5) this is accomplished
 * with icbi.
 *
 * Also note that on sh4a in the icbi case we can forego a synco for the
 * write barrier, as it's not necessary for control registers.
 *
 * Historically we have only done this type of barrier for the MMUCR, but
 * it's also necessary for the CCR, so we make it generic here instead.
 */
#ifdef CONFIG_CPU_SH4A
#define mb()		__asm__ __volatile__ ("synco": : :"memory")
#define rmb()		mb()
#define wmb()		__asm__ __volatile__ ("synco": : :"memory")
#define ctrl_barrier()	__icbi()
#define read_barrier_depends()	do { } while(0)
#else
#define mb()		__asm__ __volatile__ ("": : :"memory")
#define rmb()		mb()
#define wmb()		__asm__ __volatile__ ("": : :"memory")
#define ctrl_barrier()	__asm__ __volatile__ ("nop;nop;nop;nop;nop;nop;nop;nop")
#define read_barrier_depends()	do { } while(0)
#endif

#ifdef CONFIG_SMP
#define smp_mb()	mb()
#define smp_rmb()	rmb()
#define smp_wmb()	wmb()
#define smp_read_barrier_depends()	read_barrier_depends()
#else
#define smp_mb()	barrier()
#define smp_rmb()	barrier()
#define smp_wmb()	barrier()
#define smp_read_barrier_depends()	do { } while(0)
#endif

#define set_mb(var, value) do { (void)xchg(&var, value); } while (0)

extern unsigned long cached_to_uncached;

/*
 * Jump to uncached area.
 * When handling TLB or caches, we need to do it from an uncached area.
 */
#define jump_to_uncached()			\
do {						\
	unsigned long __dummy;			\
	__asm__ __volatile__(			\
		"mova	1f, %0\n\t"		\
		"add	%1, %0\n\t"		\
		"jmp	@%0\n\t"		\
		" nop\n\t" 			\
		".balign 4\n"			\
		"1:"				\
		: "=&z" (__dummy)		\
		: "r" (cached_to_uncached));	\
} while (0)

/*
 * Back to cached area.
 */
#define back_to_cached()				\
do {							\
	unsigned long __dummy;				\
	ctrl_barrier();					\
	__asm__ __volatile__(				\
		"mov.l	1f, %0\n\t"			\
		"jmp	@%0\n\t"			\
		" nop\n\t"				\
		".balign 4\n"				\
		"1:	.long 2f\n"			\
		"2:"					\
		: "=&r" (__dummy));			\
} while (0)

#define __uses_jump_to_uncached \
	noinline __attribute__ ((__section__ (".uncached")))

#if defined(CONFIG_SH_GRB)
static inline unsigned long xchg_u32(volatile u32 * m, unsigned long val)
{
       unsigned long retval;
       asm volatile(
                "   .align 2              \n\t"
                "   mova    1f,   r0      \n\t" // r0 = end point
                "   nop                   \n\t"
                "   mov    r15,   r1      \n\t" // r1 = saved sp
                "   mov    #-4,   r15     \n\t" // LOGIN
                "   mov.l  @%1,   %0      \n\t" // load  old value
                "   mov.l   %2,   @%1     \n\t" // store new value
                "1: mov     r1,   r15     \n\t" // LOGOUT
                : "=&r" (retval),  // 0
                  "+r"  (m)        // 1
                : "r"   (val)      // 2
                : "memory", "r0", "r1" );

        return retval;
}

static inline unsigned long xchg_u8(volatile u8 * m, unsigned long
 val)
{
       unsigned long retval;

        asm volatile(
	       "   .align  2             \n\t"
               "   mova    1f,   r0      \n\t" // r0 = end point
	       "   mov    r15,   r1      \n\t" // r1 = saved sp
               "   mov    #-6,   r15     \n\t" // LOGIN
               "   mov.b  @%1,   %0      \n\t" // load  old value
               "   extu.b  %0,   %0      \n\t" // extend as unsigned
               "   mov.b   %2,   @%1     \n\t" // store new value
               "1: mov     r1,   r15     \n\t" // LOGOUT
                : "=&r" (retval), // 0
                  "+r"  (m)       // 1
                : "r"   (val)     // 2
                : "memory" , "r0", "r1");
        return retval ;
}
#else
static inline unsigned long xchg_u32(volatile u32 *m, unsigned long val)
{
	unsigned long flags, retval;

	local_irq_save(flags);
	retval = *m;
	*m = val;
	local_irq_restore(flags);
	return retval;
}

static inline unsigned long xchg_u8(volatile u8 *m, unsigned long val)
{
	unsigned long flags, retval;

	local_irq_save(flags);
	retval = *m;
	*m = val & 0xff;
	local_irq_restore(flags);
	return retval;
}
#endif

extern void __xchg_called_with_bad_pointer(void);

#define __xchg(ptr, x, size)				\
({							\
	unsigned long __xchg__res;			\
	volatile void *__xchg_ptr = (ptr);		\
	switch (size) {					\
	case 4:						\
		__xchg__res = xchg_u32(__xchg_ptr, x);	\
		break;					\
	case 1:						\
		__xchg__res = xchg_u8(__xchg_ptr, x);	\
		break;					\
	default:					\
		__xchg_called_with_bad_pointer();	\
		__xchg__res = x;			\
		break;					\
	}						\
							\
	__xchg__res;					\
})

#define xchg(ptr,x)	\
	((__typeof__(*(ptr)))__xchg((ptr),(unsigned long)(x), sizeof(*(ptr))))

#ifndef CONFIG_LTT
#if defined(CONFIG_SH_GRB)
static inline unsigned long __cmpxchg_u32(volatile u32 * m, unsigned long old,
        unsigned long new)
{
       unsigned long retval;
        asm volatile(
               "   .align  2             \n\t"
               "   mova    1f,   r0      \n\t" // r0 = end point
               "   nop                   \n\t"
               "   mov    r15,   r1      \n\t" // r1 = saved sp
               "   mov    #-8,   r15     \n\t" // LOGIN
               "   mov.l  @%1,   %0      \n\t" // load  old value
               "   cmp/eq  %0,   %2      \n\t" //
               "   bf            1f      \n\t" // if not equal
               "   mov.l   %3,   @%1     \n\t" // store new value
               "1: mov     r1,   r15     \n\t" // LOGOUT
                : "=&r" (retval)
                : "r" (m), "r" (old), "r" (new)
                : "memory" , "r0", "r1", "t");
        return retval;
}
#else
static inline unsigned long __cmpxchg_u32(volatile int * m, unsigned long old,
	unsigned long new)
{
	__u32 retval;
	unsigned long flags;

	local_irq_save(flags);
	retval = *m;
	if (retval == old)
		*m = new;
	local_irq_restore(flags);       /* implies memory barrier  */
	return retval;
}
#endif

/* This function doesn't exist, so you'll get a linker error
 * if something tries to do an invalid cmpxchg(). */
extern void __cmpxchg_called_with_bad_pointer(void);

#define __HAVE_ARCH_CMPXCHG 1

static inline unsigned long __cmpxchg(volatile void * ptr, unsigned long old,
		unsigned long new, int size)
{
	switch (size) {
	case 4:
		return __cmpxchg_u32(ptr, old, new);
	}
	__cmpxchg_called_with_bad_pointer();
	return old;
}

#define cmpxchg(ptr,o,n)						 \
  ({									 \
     __typeof__(*(ptr)) _o_ = (o);					 \
     __typeof__(*(ptr)) _n_ = (n);					 \
     (__typeof__(*(ptr))) __cmpxchg((ptr), (unsigned long)_o_,		 \
				    (unsigned long)_n_, sizeof(*(ptr))); \
  })
#else
#include <asm-generic/cmpxchg-local.h>
/*
* cmpxchg_local and cmpxchg64_local are atomic wrt current CPU. Always make
* them available.
*/
#define cmpxchg_local(ptr,o,n) \
	(__typeof__(*(ptr)))__cmpxchg_local_generic((ptr), (unsigned long)(o), \
	(unsigned long)(n), sizeof(*(ptr)))
#define cmpxchg64_local(ptr,o,n) __cmpxchg64_local_generic((ptr), (o), (n))
#ifndef CONFIG_SMP
#include <asm-generic/cmpxchg.h>
#endif
#endif /* CONFIG_LTT */

extern void die(const char *str, struct pt_regs *regs, long err) __attribute__ ((noreturn));

extern void *set_exception_table_vec(unsigned int vec, void *handler);

static inline void *set_exception_table_evt(unsigned int evt, void *handler)
{
	return set_exception_table_vec(evt >> 5, handler);
}

/*
 * SH-2A has both 16 and 32-bit opcodes, do lame encoding checks.
 */
#ifdef CONFIG_CPU_SH2A
extern unsigned int instruction_size(unsigned int insn);
#else
#define instruction_size(insn)	(2)
#endif

/* XXX
 * disable hlt during certain critical i/o operations
 */
#define HAVE_DISABLE_HLT
void disable_hlt(void);
void enable_hlt(void);

void default_idle(void);
void per_cpu_trap_init(void);

asmlinkage void break_point_trap(unsigned long r4, unsigned long r5,
				   unsigned long r6, unsigned long r7,
				   struct pt_regs __regs);
asmlinkage void debug_trap_handler(unsigned long r4, unsigned long r5,
				   unsigned long r6, unsigned long r7,
				   struct pt_regs __regs);
asmlinkage void bug_trap_handler(unsigned long r4, unsigned long r5,
				 unsigned long r6, unsigned long r7,
				 struct pt_regs __regs);

#define arch_align_stack(x) (x)

#endif
