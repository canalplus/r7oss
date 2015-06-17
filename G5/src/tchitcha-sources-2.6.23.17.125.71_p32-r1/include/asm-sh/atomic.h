#ifndef __ASM_SH_ATOMIC_H
#define __ASM_SH_ATOMIC_H

/*
 * Atomic operations that C can't guarantee us.  Useful for
 * resource counting etc..
 *
 */

typedef struct { volatile int counter; } atomic_t;

#define ATOMIC_INIT(i)	( (atomic_t) { (i) } )

#define atomic_read(v)		((v)->counter)
#define atomic_set(v,i)		((v)->counter = (i))

#include <linux/compiler.h>
#include <asm/system.h>

#ifdef CONFIG_CPU_SH4A
#include <asm/atomic-llsc.h>
#elif defined(CONFIG_SH_GRB)
#include <asm/atomic-grb.h>
#else
#include <asm/atomic-irq.h>
#endif

#define atomic_add_negative(a, v)	(atomic_add_return((a), (v)) < 0)

#define atomic_dec_return(v) atomic_sub_return(1,(v))
#define atomic_inc_return(v) atomic_add_return(1,(v))

/*
 * atomic_inc_and_test - increment and test
 * @v: pointer of type atomic_t
 *
 * Atomically increments @v by 1
 * and returns true if the result is zero, or false for all
 * other cases.
 */
#define atomic_inc_and_test(v) (atomic_inc_return(v) == 0)

#define atomic_sub_and_test(i,v) (atomic_sub_return((i), (v)) == 0)
#define atomic_dec_and_test(v) (atomic_sub_return(1, (v)) == 0)

#define atomic_inc(v) atomic_add(1,(v))
#define atomic_dec(v) atomic_sub(1,(v))

#if defined(CONFIG_SH_GRB)
static inline int atomic_cmpxchg(atomic_t *v, int old, int new)
{
	int ret;
/*
	ret = v->counter;
	if (likely(ret == old))
		v->counter = new;
*/
	asm volatile(
		"   .align 2		\n\t"
		"   mova     1f,  r0	\n\t"
		"   nop			\n\t"
		"   mov     r15,  r1	\n\t"
		"   mov    #-8,  r15	\n\t"
		"   mov.l   @%1,  %0	\n\t"
		"   cmp/eq   %2,  %0	\n\t"
		"   bf	     1f		\n\t"
		"   mov.l    %3, @%1	\n\t"
		"1: mov      r1,  r15	\n\t"
		: "=&r" (ret)
		: "r" (v), "r" (old), "r" (new)
		: "memory" , "r0", "r1" , "t" );

	return ret;
}

static inline int atomic_add_unless(atomic_t *v, int a, int u)
{
	int ret;
	unsigned long tmp;
/*
	ret = v->counter;
	if (ret != u)
		v->counter += a;
*/
	asm volatile(
		"   .align 2		\n\t"
		"   mova    1f,   r0	\n\t"
		"   nop			\n\t"
		"   mov    r15,   r1	\n\t"
		"   mov    #-12,  r15	\n\t"
		"   mov.l  @%2,   %1	\n\t"
		"   mov	    %1,   %0    \n\t"
		"   cmp/eq  %4,   %0	\n\t"
		"   bt/s    1f		\n\t"
		"    add    %3,   %1	\n\t"
		"   mov.l   %1,  @%2	\n\t"
		"1: mov     r1,   r15	\n\t"
		: "=&r" (ret), "=&r" (tmp)
		: "r" (v), "r" (a), "r" (u)
		: "memory" , "r0", "r1" , "t" );

	return ret != u;
}
#else
static inline int atomic_cmpxchg(atomic_t *v, int old, int new)
{
	int ret;
	unsigned long flags;

	local_irq_save(flags);
	ret = v->counter;
	if (likely(ret == old))
		v->counter = new;
	local_irq_restore(flags);

	return ret;
}

static inline int atomic_add_unless(atomic_t *v, int a, int u)
{
	int ret;
	unsigned long flags;

	local_irq_save(flags);
	ret = v->counter;
	if (ret != u)
		v->counter += a;
	local_irq_restore(flags);

	return ret != u;
}
#endif

#define atomic_xchg(v, new) (xchg(&((v)->counter), new))
#define atomic_inc_not_zero(v) atomic_add_unless((v), 1, 0)

/* Atomic operations are already serializing on SH */
#define smp_mb__before_atomic_dec()	barrier()
#define smp_mb__after_atomic_dec()	barrier()
#define smp_mb__before_atomic_inc()	barrier()
#define smp_mb__after_atomic_inc()	barrier()

#include <asm-generic/atomic.h>
#endif /* __ASM_SH_ATOMIC_H */
