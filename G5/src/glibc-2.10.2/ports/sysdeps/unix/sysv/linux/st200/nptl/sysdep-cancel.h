/* Copyright (C) 2005 Free Software Foundation, Inc.
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

#include <sysdep.h>
#include <tls.h>
#ifndef __ASSEMBLER__
# include <nptl/pthreadP.h>
#endif

#if !defined NOT_IN_libc || defined IS_IN_libpthread || defined IS_IN_librt

# undef PSEUDO
# define PSEUDO(name, syscall_name, args)			\
  ENTRY (name)                                                 ;\
.Lpseudo_start:                                                ;\
	SINGLE_THREAD_P($b0,$r8)			       ;\
        ;;                                                     ;\
        brf     $b0,1f                                         ;\
        ;;                                                     ;\
        DO_CALL (SYS_ify (syscall_name))                       ;\
        ;;                                                     ;\
        br      $b0,.Lsyscall_error                            ;\
        ;;                                                     ;\
        goto    .Lpseudo_end                                   ;\
        ;;                                                     ;\
1:                                                             ;\
        /* Open up a new stack frame. In the new frame,         \
           R12+0..R12+15 are kept for called fns,               \
           R16+16..R12+39 hold saved arguments,                 \
           R12+40..R12+43 hold saved return address.            \
           R12+44..R12+47 are unused.                           \
           Note that R12+32..R12+47 are the 16 free bytes above \
           the SP passed in, so we only adjust R12 by 32. */    \
	add	$r12 = $r12, -32 			       ;\
        ;;                                                     ;\
.LCFI0:                                                        ;\
	stw	40[$r12] = $r63  			       ;\
        ;;                                                     ;\
.LCFI1:                                                        ;\
	SAVE_ARGS_##args				       ;\
        ;;                                                     ;\
	CENABLE  					       ;\
        ;;                                                     ;\
	/* Save the CENABLE return value in R63. That register	\
	   is preserved across syscall and the real return 	\
	   address is saved on the stack.  */			\
        mov     $r63 = $r16                                    ;\
        ;;                                                     ;\
	LOAD_ARGS_##args  				       ;\
        ;;                                                     ;\
        DO_CALL (SYS_ify (syscall_name))                       ;\
        ;;                                                     ;\
        stw     16[$r12] = $r16                                ;\
        mov     $r16 = $r63                                    ;\
        br      $b0, 3f                                        ;\
        ;;                                                     ;\
.LCFI2:                                                        ;\
	CDISABLE					       ;\
        ;;                                                     ;\
        ldw     $r63 = 40[$r12]                                ;\
        ;;                                                     ;\
.LCFI3:                                                        ;\
        ldw     $r16= 16[$r12]                                 ;\
        add     $r12 = $r12, 32                                ;\
	ret						       ;\
        ;;                                                     ;\
3:                                                             ;\
.LCFI4:                                                        ;\
	CDISABLE 					       ;\
        ;;                                                     ;\
	ldw	$r63 = 40[$r12] 		               ;\
        ;;                                                     ;\
.LCFI5:                                                        ;\
        ldw     $r16 = 16[$r12]                                ;\
        add     $r12 = $r12, 32                                ;\
        ;;                                                     ;\
.Lsyscall_error:                                               ;\
	SYSCALL_ERROR_HANDLER				       ;\
        mov     $r16 = -1                                      ;\
        ;;                                                     ;\
.Lpseudo_end:						       ;\
   /* Create unwinding information for the syscall wrapper. */		\
   .section .eh_frame,"a",@progbits;					\
.Lframe1:								\
   .word .LECIE1 - .LSCIE1;						\
.LSCIE1:								\
   .word 0x0;								\
   .byte   0x1;								\
   AUGMENTATION_STRING;							\
   .uleb128 0x1;							\
   .sleb128 -4;								\
   .byte 0x3f;								\
   AUGMENTATION_PARAM;							\
   .byte 0xc;								\
   .uleb128 0xc;							\
   .uleb128 0x10;							\
   .balign 4;								\
.LECIE1:								\
.LSFDE1:								\
   .word .LEFDE1-.LASFDE1;						\
.LASFDE1:								\
   .word .LASFDE1-.Lframe1;						\
   START_SYMBOL_REF;							\
   .word .Lpseudo_end - .Lpseudo_start;					\
   AUGMENTATION_PARAM_FDE;						\
   .byte 0x4;  /* DW_CFA_advance_loc4 */				\
   .word .LCFI0-.Lpseudo_start;						\
   .byte 0xe;  /* DW_CFA_def_cfa_offset */				\
   .uleb128 48;								\
   .byte 0x4; /* DW_CFA_advance_loc4 */					\
   .word .LCFI1-.LCFI0;							\
   .byte 0xbf; /* DW_CFA_offset r63 */					\
   .uleb128 2; /* CFA - 8 */						\
   .byte 0x4; /* DW_CFA_advance_loc4 */					\
   .word .LCFI2-.LCFI1;                                 		\
   .byte 0xa; /* DW_CFA_remember_state */               		\
   .byte 0x4; /* DW_CFA_advance_loc4 */                  		\
   .word .LCFI3-.LCFI2;							\
   .byte 0xff; /* DW_CFA_restore r63 */                  		\
   .byte 0x4; /* DW_CFA_advance_loc4 */                   		\
   .word .LCFI4-.LCFI3;                                 		\
   .byte 0xb;  /* DW_CFA_restore_state */                  		\
   .byte 0x4; /* DW_CFA_advance_loc4 */					\
   .word .LCFI5-.LCFI4;                                 		\
   .byte 0xff; /* DW_CFA_restore r63 */                   		\
   .byte 0x4; /* DW_CFA_advance_loc4 */                   		\
   .word .Lsyscall_error-.LCFI5;                        		\
   .byte 0xe; /* DW_CFA_def_cfa_offset */                 		\
   .uleb128 16;								\
   .balign 4;								\
.LEFDE1:                                                                \
.previous

# ifdef SHARED
#  define AUGMENTATION_STRING .string "zR"
#  define AUGMENTATION_PARAM .uleb128 1; .byte 0x1b
#  define AUGMENTATION_PARAM_FDE .uleb128 0
#  define START_SYMBOL_REF .long .Lpseudo_start-.
# else
#  define AUGMENTATION_STRING .ascii "\0"
#  define AUGMENTATION_PARAM
#  define AUGMENTATION_PARAM_FDE
#  define START_SYMBOL_REF .long .Lpseudo_start
# endif

# undef PSEUDO_END
# define PSEUDO_END(sym)					\
        .endp                                                  ;\
        .size sym, .-sym

# define SAVE_ARGS_0	/* Nothing.  */
# define SAVE_ARGS_1	                  stw 16[$r12] = $r16;
# define SAVE_ARGS_2	SAVE_ARGS_1; ;; ; stw 20[$r12] = $r17;
# define SAVE_ARGS_3	SAVE_ARGS_2; ;; ; stw 24[$r12] = $r18;
# define SAVE_ARGS_4	SAVE_ARGS_3; ;; ; stw 28[$r12] = $r19;
# define SAVE_ARGS_5	SAVE_ARGS_4; ;; ; stw 32[$r12] = $r20;
# define SAVE_ARGS_6	SAVE_ARGS_5; ;; ; stw 36[$r12] = $r21;

# define LOAD_ARGS_0	/* Nothing.  */
# define LOAD_ARGS_1	                  ldw $r16 = 16[$r12];
# define LOAD_ARGS_2	LOAD_ARGS_1; ;; ; ldw $r17 = 20[$r12];
# define LOAD_ARGS_3	LOAD_ARGS_2; ;; ; ldw $r18 = 24[$r12];
# define LOAD_ARGS_4	LOAD_ARGS_3; ;; ; ldw $r19 = 28[$r12];
# define LOAD_ARGS_5	LOAD_ARGS_4; ;; ; ldw $r20 = 32[$r12];
# define LOAD_ARGS_6	LOAD_ARGS_5; ;; ; ldw $r21 = 36[$r12];

# ifdef IS_IN_libpthread
#  define __local_enable_asynccancel	__pthread_enable_asynccancel
#  define __local_disable_asynccancel	__pthread_disable_asynccancel
#  define __local_multiple_threads	__pthread_multiple_threads
# elif !defined NOT_IN_libc
#  define __local_enable_asynccancel	__libc_enable_asynccancel
#  define __local_disable_asynccancel	__libc_disable_asynccancel
#  define __local_multiple_threads	__libc_multiple_threads
# elif defined IS_IN_librt
#  define __local_enable_asynccancel	__librt_enable_asynccancel
#  define __local_disable_asynccancel	__librt_disable_asynccancel
# else
#  error Unsupported library
# endif

# define CENABLE	call $r63 = __local_enable_asynccancel
# define CDISABLE	call $r63 = __local_disable_asynccancel

# ifndef __ASSEMBLER__
#  define SINGLE_THREAD_P \
	__builtin_expect (THREAD_GETMEM (THREAD_SELF, \
	 		                header.multiple_threads) == 0, 1)
# else
#  define SINGLE_THREAD_P(breg,greg) \
         ldw greg = MULTIPLE_THREADS_OFFSET[$r13]            ;\
         ;;                                                   ;\
         cmpeq breg = greg, $r0
# endif

#elif !defined __ASSEMBLER__

# define SINGLE_THREAD_P (1)
# define NO_CANCELLATION 1

#endif

#ifndef __ASSEMBLER__
# define RTLD_SINGLE_THREAD_P \
  __builtin_expect (THREAD_GETMEM (THREAD_SELF, \
				   header.multiple_threads) == 0, 1)
#endif
