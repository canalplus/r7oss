/* Copyright (C) 2003 Free Software Foundation, Inc.
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


#define atomic_full_barrier()	__asm ("sync" ::: "memory")
#define atomic_write_barrier()	__asm ("sync" ::: "memory")

#define __arch_compare_and_exchange_val_xx_acq(mem, newval, oldval, load_op, store_op) \
({			                                                \
  __typeof (*(mem)) __st200_tmp;                                        \
  int __st200_tmp2;                                                     \
  __typeof (mem) __st200_memp = (mem);                                  \
  						                        \
  __asm__ __volatile__ (						\
        "       call    $r63  = 1f                                   \n"\
        "       ;;                                                   \n"\
        "1:     " #load_op " %0    = 0[%2]                           \n"\
        "       mov     $r62  = $r63                                 \n"\
        "       or      $r12  = $r12, 1                              \n"\
        "       ;;                                                   \n"\
        "       cmpeq   $b0   = %0, %3                               \n"\
        "       ;;                                                   \n"\
	"       slct    %1    = $b0, %4, %0                          \n"\
        "       ;;                                                   \n"\
        "       " #store_op " 0[%2] = %1                             \n"\
        "       and     $r12  = $r12, ~1                             \n"\
        : "=&r" (__st200_tmp) /* %0 */, "=&r" (__st200_tmp2) /* %1 */   \
        : "r" (__st200_memp) /* %2 */, "r" (oldval) /* %3 */,           \
          "r" (newval) /* %4 */						\
	: "b0","r62","r63","memory" );                                  \
  __st200_tmp;                                                          \
})

#define __arch_compare_and_exchange_val_8_acq(mem, newval, oldval)      \
  __arch_compare_and_exchange_val_xx_acq(mem,newval,oldval, ldb, stb)

#define __arch_compare_and_exchange_val_16_acq(mem, newval, oldval)     \
  __arch_compare_and_exchange_val_xx_acq(mem,newval,oldval, ldh, sth)

#define __arch_compare_and_exchange_val_32_acq(mem, newval, oldval)     \
  __arch_compare_and_exchange_val_xx_acq(mem,newval,oldval, ldw, stw)

#define __arch_compare_and_exchange_val_64_acq(mem, newval, oldval)	\
  ({									\
    extern long long __st200_unsupported_arch_compare_and_exchange_val_64_acq(void); \
    __st200_unsupported_arch_compare_and_exchange_val_64_acq(); })

#define atomic_compare_and_exchange_val_acq(mem, newval, oldval)       \
({                                                                     \
  __typeof (*(mem)) __st200_result;                                    \
  if (sizeof (*mem) == 1)                                              \
    __st200_result = __arch_compare_and_exchange_val_8_acq (mem, newval, oldval); \
  else if (sizeof (*mem) == 2)						\
    __st200_result = __arch_compare_and_exchange_val_16_acq (mem, newval, oldval); \
  else if (sizeof (*mem) == 4)						\
    __st200_result = __arch_compare_and_exchange_val_32_acq (mem, newval, oldval); \
  else									\
    __st200_result = __arch_compare_and_exchange_val_64_acq (mem, newval, oldval); \
  __st200_result;							\
})

#define __arch_atomic_exchange_xx_acq(mem, value, load_op, store_op)	\
({			                                                \
  __typeof (*(mem)) __st200_val;                                        \
  __typeof (mem) __st200_memp = (mem);                                  \
  						                        \
  __asm__ __volatile__ (						\
        "       call    $r63  = 1f                                   \n"\
        "       ;;                                                   \n"\
        "1:     " #load_op " %0    = 0[%1]                           \n"\
        "       mov     $r62  = $r63                                 \n"\
        "       or      $r12  = $r12, 1                              \n"\
        "       ;;                                                   \n"\
        "       " #store_op " 0[%1] = %2                             \n"\
        "       and     $r12  = $r12, ~1                             \n"\
        : "=&r" (__st200_val) /* %0 */					\
        : "r" (__st200_memp) /* %1 */, "r" (value) /* %2 */		\
	: "r62","r63","memory" );                                       \
  __st200_val;                                                          \
})

#define __arch_atomic_exchange_8_acq(mem, value)	\
  __arch_atomic_exchange_xx_acq(mem, value, ldb, stb)

#define __arch_atomic_exchange_16_acq(mem, value)	\
  __arch_atomic_exchange_xx_acq(mem, value, ldh, sth)

#define __arch_atomic_exchange_32_acq(mem, value)	\
  __arch_atomic_exchange_xx_acq(mem, value, ldw, stw)

#define __arch_atomic_exchange_64_acq(mem, value)	\
  ({									\
    extern long long __st200_unsupported_arch_atomic_exchange_64_acq(void); \
    __st200_unsupported_arch_atomic_exchange_64_acq(); })

#define atomic_exchange_acq(mem, value) \
({                                                                     \
  __typeof (*(mem)) __st200_result;                                    \
  if (sizeof (*mem) == 1)                                              \
    __st200_result = __arch_atomic_exchange_8_acq (mem, value);        \
  else if (sizeof (*mem) == 2)					       \
    __st200_result = __arch_atomic_exchange_16_acq (mem, value);       \
  else if (sizeof (*mem) == 4)					       \
    __st200_result = __arch_atomic_exchange_32_acq (mem, value);       \
  else								       \
    __st200_result = __arch_atomic_exchange_64_acq (mem, value);       \
  __st200_result;                                                      \
})

#define __arch_atomic_exchange_and_add_xx(mem, value, load_op, store_op) \
({			                                                \
  __typeof (*(mem)) __st200_val, __st200_tmp;                           \
  __typeof (mem) __st200_memp = (mem);                                  \
  						                        \
  __asm__ __volatile__ (						\
        "       call    $r63  = 1f                                   \n"\
        "       ;;                                                   \n"\
        "1:     " #load_op " %0    = 0[%2]                           \n"\
        "       mov     $r62  = $r63                                 \n"\
        "       or      $r12  = $r12, 1                              \n"\
        "       ;;                                                   \n"\
        "       add     %1    = %0, %3                               \n"\
        "       ;;                                                   \n"\
        "       " #store_op " 0[%2] = %1                             \n"\
        "       and     $r12  = $r12, ~1                             \n"\
        : "=&r" (__st200_val) /* %0 */, "=&r" (__st200_tmp) /* %1 */	\
        : "r" (__st200_memp) /* %2 */, "r" (value) /* %3 */		\
	: "r62","r63","memory" );                                       \
  __st200_val;                                                          \
})

#define __arch_atomic_exchange_and_add_8(mem, value)                   \
  __arch_atomic_exchange_and_add_xx(mem, value, ldb, stb)

#define __arch_atomic_exchange_and_add_16(mem, value)                   \
  __arch_atomic_exchange_and_add_xx(mem, value, ldh, sth)

#define __arch_atomic_exchange_and_add_32(mem, value)                   \
  __arch_atomic_exchange_and_add_xx(mem, value, ldw, stw)

#define __arch_atomic_exchange_and_add_64(mem, value)			\
  ({									\
    extern long long __st200_unsupported_arch_atomic_exchange_and_add_64(void); \
    __st200_unsupported_arch_atomic_exchange_and_add_64(); })

#define atomic_exchange_and_add(mem, value) \
  ({									\
    __typeof (*(mem)) __st200_result;					\
    if (sizeof (*mem) == 1)						\
      __st200_result = __arch_atomic_exchange_and_add_8 (mem, value);	\
    else if (sizeof (*mem) == 2)					\
      __st200_result = __arch_atomic_exchange_and_add_16 (mem, value);	\
    else if (sizeof (*mem) == 4)					\
      __st200_result = __arch_atomic_exchange_and_add_32 (mem, value);	\
    else								\
      __st200_result = __arch_atomic_exchange_and_add_64 (mem, value);	\
    __st200_result;							\
})

#define __arch_atomic_decrement_if_positive_xx(mem, load_op, store_op)	\
({			                                                \
  __typeof (*(mem)) __st200_tmp, __st200_val;                           \
  __typeof (mem) __st200_memp = (mem);                                  \
  						                        \
  __asm__ __volatile__ (						\
        "       call    $r63  = 1f                                   \n"\
        "       ;;                                                   \n"\
        "1:     " #load_op " %0    = 0[%2]                           \n"\
        "       mov     $r62  = $r63                                 \n"\
        "       or      $r12  = $r12, 1                              \n"\
        "       ;;                                                   \n"\
        "       cmpgt   $b0   = %0, $r0                              \n"\
        "       add     %1    = %0, -1                               \n"\
        "       ;;                                                   \n"\
	"       slct    %1    = $b0, %1, %0                          \n"\
        "       ;;                                                   \n"\
        "       " #store_op " 0[%2] = %1                             \n"\
        "       and     $r12  = $r12, ~1                             \n"\
        : "=&r" (__st200_val) /* %0 */, "=&r" (__st200_tmp) /* %1 */	\
        : "r" (__st200_memp) /* %2 */					\
	: "b0","r62","r63","memory" );                                  \
  __st200_val;                                                          \
})

#define __arch_atomic_decrement_if_positive_8(mem)                     \
  __arch_atomic_decrement_if_positive_xx(mem, ldw, stw)

#define __arch_atomic_decrement_if_positive_16(mem)                     \
  __arch_atomic_decrement_if_positive_xx(mem, ldw, stw)

#define __arch_atomic_decrement_if_positive_32(mem)                     \
  __arch_atomic_decrement_if_positive_xx(mem, ldw, stw)

#define __arch_atomic_decrement_if_positive_64(mem)                     \
  ({									\
    extern long long __st200_unsupported_arch_atomic_decrement_if_positive_64(void); \
    __st200_unsupported_arch_atomic_decrement_if_positive_64(); })

/* Decrement *MEM if it is > 0, and return the old value.  */
#define atomic_decrement_if_positive(mem) \
  ({ __typeof (*(mem)) __st200_result;					\
    if (sizeof (*mem) == 1)						\
      __st200_result = __arch_atomic_decrement_if_positive_8 (mem);	\
    else if (sizeof (*mem) == 2)					\
      __st200_result = __arch_atomic_decrement_if_positive_16 (mem);	\
    else if (sizeof (*mem) == 4)					\
      __st200_result = __arch_atomic_decrement_if_positive_32 (mem);	\
    else								\
      __st200_result = __arch_atomic_decrement_if_positive_64 (mem);	\
    __st200_result;							\
  })

#define __arch_atomic_bit_test_set_xx(mem, bit, load_op, store_op)	\
  ({			                                                \
    __typeof (*(mem)) __st200_tmp, __st200_val;				\
    __typeof (mem) __st200_memp = (mem);				\
    __typeof (*(mem)) __st200_mask = ((__typeof (*mem))1 << (bit));	\
    									\
    __asm__ __volatile__ (						\
			  "       call    $r63  = 1f                \n" \
			  "       ;;                                \n" \
			  "1:     " #load_op " %0    = 0[%2]        \n" \
			  "       mov     $r62  = $r63              \n" \
			  "       or      $r12  = $r12, 1           \n" \
			  "       ;;                                \n" \
			  "       or      %1    = %0, %3            \n" \
			  "       and     %0    = %0, %3            \n" \
			  "       ;;                                \n" \
			  "       " #store_op " 0[%2] = %1          \n" \
			  "       and     $r12  = $r12, ~1          \n" \
			  : "=&r" (__st200_val) /* %0 */, "=&r" (__st200_tmp) /* %1 */ \
			  : "r" (__st200_memp) /* %2 */, "r" (__st200_mask) /* %3 */ \
			  : "r62","r63","memory" );			\
  __st200_val;                                                          \
})

#define __arch_atomic_bit_test_set_8(mem, bit)                         \
  __arch_atomic_bit_test_set_xx(mem, bit, ldb, stb)

#define __arch_atomic_bit_test_set_16(mem, bit)                         \
  __arch_atomic_bit_test_set_xx(mem, bit, ldh, sth)

#define __arch_atomic_bit_test_set_32(mem, bit)                         \
  __arch_atomic_bit_test_set_xx(mem, bit, ldw, stw)

#define __arch_atomic_bit_test_set_64(mem, bit)                         \
  ({									\
    extern long long __st200_unsupported_arch_atomic_bit_test_set_64(void); \
    __st200_unsupported_arch_atomic_bit_test_set_64(); })

#define atomic_bit_test_set(mem, bit)					\
  ({ __typeof (*(mem)) __st200_result;					\
    if (sizeof (*mem) == 1)						\
      __st200_result = __arch_atomic_bit_test_set_32 (mem, bit);	\
    else if (sizeof (*mem) == 2)					\
      __st200_result = __arch_atomic_bit_test_set_16 (mem, bit);	\
    else if (sizeof (*mem) == 4)					\
      __st200_result = __arch_atomic_bit_test_set_32 (mem, bit);	\
    else								\
      __st200_result = __arch_atomic_bit_test_set_64 (mem, bit);	\
    __st200_result;							\
  })

