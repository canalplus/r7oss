/* Atomic operations used inside libc.  Linux/SH version.
   Copyright (C) 2003 Free Software Foundation, Inc.
   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
   02111-1307 USA.  */

#include <stdint.h>

typedef int8_t atomic8_t;
typedef uint8_t uatomic8_t;
typedef int_fast8_t atomic_fast8_t;
typedef uint_fast8_t uatomic_fast8_t;

typedef int16_t atomic16_t;
typedef uint16_t uatomic16_t;
typedef int_fast16_t atomic_fast16_t;
typedef uint_fast16_t uatomic_fast16_t;

typedef int32_t atomic32_t;
typedef uint32_t uatomic32_t;
typedef int_fast32_t atomic_fast32_t;
typedef uint_fast32_t uatomic_fast32_t;

typedef int64_t atomic64_t;
typedef uint64_t uatomic64_t;
typedef int_fast64_t atomic_fast64_t;
typedef uint_fast64_t uatomic_fast64_t;

typedef intptr_t atomicptr_t;
typedef uintptr_t uatomicptr_t;
typedef intmax_t atomic_max_t;
typedef uintmax_t uatomic_max_t;

/* SH kernel has implemented a gUSA ("g" User Space Atomicity) support
   for the user space atomicity. The atomicity macros use this scheme.

  Reference:
    Niibe Yutaka, "gUSA: Simple and Efficient User Space Atomicity
    Emulation with Little Kernel Modification", Linux Conference 2002,
    Japan. http://lc.linux.or.jp/lc2002/papers/niibe0919h.pdf (in
    Japanese).

    Niibe Yutaka, "gUSA: User Space Atomicity with Little Kernel
    Modification", LinuxTag 2003, Rome.
    http://www.semmel.ch/Linuxtag-DVD/talks/170/paper.html (in English).

    B.N. Bershad, D. Redell, and J. Ellis, "Fast Mutual Exclusion for
    Uniprocessors",  Proceedings of the Fifth Architectural Support for
    Programming Languages and Operating Systems (ASPLOS), pp. 223-233,
    October 1992. http://www.cs.washington.edu/homes/bershad/Papers/Rcs.ps

  SuperH ABI:
      r15:    -(size of atomic instruction sequence) < 0
      r0:     end point
      r1:     saved stack pointer
*/

/* Avoid having lots of different versions of compare and exchange,
   by having this one complicated version. Parameters:
      bwl:     b, w or l for 8, 16 and 32 bit versions.
      version: val or bool, depending on whether the result is the
               previous value or a bool indicating whether the transfer
               did happen (note this needs inverting before being
               returned in atomic_compare_and_exchange_bool).
*/

#define GENERATE__arch_compare_and_exchange(size, mem, newval, oldval, bwl, version)	\
static __always_inline	\
int32_t __arch_compare_and_exchange_func_##version##_##size##_acq(	\
	int *mem, int newval, int oldval)	\
{ \
    signed long __result; \
     __asm __volatile ("\
	.align 2\n\
	mova 1f,r0\n\
	nop\n\
	mov r15,r1\n\
	mov #-8,r15\n\
     0: mov." #bwl " @%1,%0\n\
	cmp/eq %0,%3\n\
	bf 1f\n\
	mov." #bwl " %2,@%1\n\
     1: mov r1,r15\n\
     .ifeqs \"bool\",\"" #version "\"\n\
        movt %0\n\
     .endif\n"					\
	: "=&r" (__result), "+r" (mem)	\
	: "r" (newval), "r" (oldval)	\
	: "r0", "r1", "t", "memory");		\
	return __result; \
}

GENERATE__arch_compare_and_exchange(8, mem, newval, oldval, b, val)
GENERATE__arch_compare_and_exchange(16, mem, newval, oldval, w, val)
GENERATE__arch_compare_and_exchange(32, mem, newval, oldval, l, val)

#define __arch_compare_and_exchange_val_8_acq(mem, newval, oldval) \
  __arch_compare_and_exchange_func_val_8_acq((int *) mem, (int) newval, (int) oldval)

#define __arch_compare_and_exchange_val_16_acq(mem, newval, oldval) \
  __arch_compare_and_exchange_func_val_16_acq((int *) mem, (int) newval, (int) oldval)

#define __arch_compare_and_exchange_val_32_acq(mem, newval, oldval) \
  __arch_compare_and_exchange_func_val_32_acq((int *) mem, (int) newval, (int) oldval)

/* XXX We do not really need 64-bit compare-and-exchange.  At least
   not in the moment.  Using it would mean causing portability
   problems since not many other 32-bit architectures have support for
   such an operation.  So don't define any code for now.  */

# define __arch_compare_and_exchange_val_64_acq(mem, newval, oldval) \
  (abort (), 0)

/* For "bool" routines, return if the exchange did NOT occur */

GENERATE__arch_compare_and_exchange(8, mem, newval, oldval, b, bool)
GENERATE__arch_compare_and_exchange(16, mem, newval, oldval, w, bool)
GENERATE__arch_compare_and_exchange(32, mem, newval, oldval, l, bool)

#define __arch_compare_and_exchange_bool_8_acq(mem, newval, oldval) \
  (! __arch_compare_and_exchange_func_bool_8_acq((int *) mem, (int) newval, (int) oldval))

#define __arch_compare_and_exchange_bool_16_acq(mem, newval, oldval) \
  (! __arch_compare_and_exchange_func_bool_16_acq((int *) mem, (int) newval, (int) oldval))

#define __arch_compare_and_exchange_bool_32_acq(mem, newval, oldval) \
  (! __arch_compare_and_exchange_func_bool_32_acq((int *) mem, (int) newval, (int) oldval))

# define __arch_compare_and_exchange_bool_64_acq(mem, newval, oldval) \
  (abort (), 0)

/* Similar to the above, have one template which can be used in a
   number of places. This version returns both the old and the new
   values of the location. Parameters:
      bwl:     b, w or l for 8, 16 and 32 bit versions.
      oper:    The instruction to perform on the old value.
   Note old is not sign extended, so should be an unsigned long.
*/

#define GENERATE__arch_operate_old_new(size, mem, value, bwl, oper, ret) \
static __always_inline	\
int32_t __arch_operate_old_new_func_##oper##_##size(	\
	int *mem, int##size##_t value)	\
{ \
	  int##size##_t new, old;	\
	__asm __volatile ("\
	.align 2\n\
	mova 1f,r0\n\
	mov r15,r1\n\
	nop\n\
	mov #-8,r15\n\
     0: mov." #bwl " @%2,%0\n\
	mov %0,%1\n\
	" #oper " %3,%1\n\
	mov." #bwl " %1,@%2\n\
     1: mov r1,r15"			\
	: "=&r" (old), "=&r" (new), "+r" (mem)	\
	: "r" (value)	\
	: "r0", "r1", "memory");	\
	return  (int32_t) ret;	\
}

GENERATE__arch_operate_old_new(8,  mem, value, b, add, old)
GENERATE__arch_operate_old_new(16, mem, value, w, add, old)
GENERATE__arch_operate_old_new(32, mem, value, l, add, old)


#define __arch_exchange_and_add_8_int(mem, value)			\
    __arch_operate_old_new_func_add_8((int *) (mem), (int) value)

#define __arch_exchange_and_add_16_int(mem, value)			\
    __arch_operate_old_new_func_add_16((int *) (mem), (int) value)

#define __arch_exchange_and_add_32_int(mem, value)			\
    __arch_operate_old_new_func_add_32((int *) (mem), (int) value)


#define __arch_exchange_and_add_64_int(mem, value)			\
  (abort (), 0)

#define atomic_exchange_and_add(mem, value) \
  __atomic_val_bysize (__arch_exchange_and_add, int, mem, value)

/* Again, another template. We get a slight optimisation when the old value
   does not need to be returned. Parameters:
      bwl:     b, w or l for 8, 16 and 32 bit versions.
      oper:    The instruction to perform on the old value.
*/

#define GENERATE__arch_operate_new(size, mem, value, bwl, oper)	\
static __always_inline	\
int32_t	__arch_operate_new_func_##oper##_##size(	\
	int *mem, int32_t value)	\
{ \
  int32_t __value = (value);	\
  int##size##_t __new;	\
     __asm __volatile ("\
	.align 2\n\
	mova 1f,r0\n\
	mov r15,r1\n\
	mov #-6,r15\n\
     0: mov." #bwl " @%1,%0\n\
	" #oper " %2,%0\n\
	mov." #bwl " %0,@%1\n\
     1: mov r1,r15"			\
	: "=&r" (__new), "+r" (mem)	\
	: "r" (__value)	\
	: "r0", "r1", "memory");	\
	return __new;				\
}

GENERATE__arch_operate_new(8,  mem, value, b, add)
GENERATE__arch_operate_new(16, mem, value, w, add)
GENERATE__arch_operate_new(32, mem, value, l, add)

#define __arch_add_8_int(mem, value)		\
  __arch_operate_new_func_add_8((int *) mem, (int) value)

#define __arch_add_16_int(mem, value)		\
  __arch_operate_new_func_add_16((int *) mem, (int) value)

#define __arch_add_32_int(mem, value)		\
  __arch_operate_new_func_add_32((int *) mem, (int) value)

#define __arch_add_64_int(mem, value)		\
  (abort (), 0)

#define atomic_add(mem, value) \
  ((void) __atomic_val_bysize (__arch_add, int, mem, value))


#define __arch_add_negative_8_int(mem, value)		\
  (__arch_operate_new_func_add_8((int *) mem,  (int) value) < 0)

#define __arch_add_negative_16_int(mem, value)		\
  (__arch_operate_new_func_add_16((int *) mem, (int) value) < 0)

#define __arch_add_negative_32_int(mem, value)		\
  (__arch_operate_new_func_add_32((int *) mem, (int) value) < 0)

#define __arch_add_negative_64_int(mem, value)		\
  (abort (), 0)

#define atomic_add_negative(mem, value) \
  __atomic_bool_bysize (__arch_add_negative, int, mem, value)


#define __arch_add_zero_8_int(mem, value)		\
  (__arch_operate_new_func_add_8((int *) mem, (int) value) == 0)

#define __arch_add_zero_16_int(mem, value)		\
  (__arch_operate_new_func_add_16((int *) mem, (int) value) == 0)

#define __arch_add_zero_32_int(mem, value)		\
  (__arch_operate_new_func_add_32((int *) mem, (int) value) == 0)

#define __arch_add_zero_64_int(mem, value)		\
  (abort (), 0)

#define atomic_add_zero(mem, value) \
  __atomic_bool_bysize (__arch_add_zero, int, mem, value)


#define atomic_increment_and_test(mem) atomic_add_zero((mem), 1)
#define atomic_decrement_and_test(mem) atomic_add_zero((mem), -1)

GENERATE__arch_operate_new(8,  mem, value, b, or)
GENERATE__arch_operate_new(16, mem, value, w, or)
GENERATE__arch_operate_new(32, mem, value, l, or)

#define __arch_bit_set_8_int(mem, value)		\
  __arch_operate_new_func_or_8((int *) mem, 1<<(value))

#define __arch_bit_set_16_int(mem, value)		\
  __arch_operate_new_func_or_16((int *) mem, 1<<(value))

#define __arch_bit_set_32_int(mem, value)		\
  __arch_operate_new_func_or_32((int *) mem, 1<<(value))

#define __arch_bit_set_64_int(mem, value)			\
  (abort (), 0)

GENERATE__arch_operate_old_new(8,  mem, value, b, or, old & value)
GENERATE__arch_operate_old_new(16, mem, value, w, or, old & value)
GENERATE__arch_operate_old_new(32, mem, value, l, or, old & value)

#define atomic_bit_set(mem, value) \
  ((void) __atomic_val_bysize (__arch_bit_set, int, mem, value))



#define __arch_bit_test_set_8_int(mem, value)				\
    __arch_operate_old_new_func_or_8((int *) mem, 1<<(value))

#define __arch_bit_test_set_16_int(mem, value)				\
    __arch_operate_old_new_func_or_16((int *) mem, 1<<(value))

#define __arch_bit_test_set_32_int(mem, value)				\
    __arch_operate_old_new_func_or_32((int *) mem, 1<<(value))

#define __arch_bit_test_set_64_int(mem, value)	\
  (abort (), 0)

#define atomic_bit_test_set(mem, value) \
  __atomic_val_bysize (__arch_bit_test_set, int, mem, value)
