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

#ifndef _LINUX_ST200_SYSDEP_H
#define _LINUX_ST200_SYSDEP_H 1

#include <sysdeps/st200/sysdep.h>
#include <sysdeps/unix/sysdep.h>

/* Defines RTLD_PRIVATE_ERRNO and USE_DL_SYSINFO.  */
#include <dl-sysdep.h>

#include <tls.h>

#undef SYS_ify
#define SYS_ify(syscall_name)	(__NR_##syscall_name)

#ifdef __ASSEMBLER__

/* ST200 syscalls return:
     b0 = 0 for success, 1 for failure
     r16 = result (or failure code on failure)
     r17 = second result for those syscalls that have two results.
*/

#undef PSEUDO
#define	PSEUDO(name, syscall_name, args)	\
  ENTRY(name)					\
    DO_CALL (SYS_ify(syscall_name))            ;\
        ;;                                     ;\
        br      $b0, 1f                        ;\
        ;;                                     ;\
        ret                                    ;\
1:                                              \
        SYSCALL_ERROR_HANDLER                  ;\
        mov     $r16 = -1                      ;\
        ;;

#undef PSEUDO_END
#define PSEUDO_END(name)	.endp

#undef	PSEUDO_NOERRNO
#define	PSEUDO_NOERRNO(name, syscall_name, args) \
    ENTRY(name)					 \
    DO_CALL (SYS_ify(syscall_name))             ;\
        ;;

#undef	PSEUDO_END_NOERRNO
#define	PSEUDO_END_NOERRNO(name) .endp

#define ret_NOERRNO ret

#define	PSEUDO_ERRVAL(name, syscall_name, args) \
    ENTRY(name)					\
    DO_CALL (SYS_ify(syscall_name))            ;\
        ;;

#undef	PSEUDO_END_ERRVAL
#define	PSEUDO_END_ERRVAL(name)  .endp

#define ret_ERRVAL ret

#define DO_CALL(num)				\
	mov $r15=num			       ;\
        ;;                                     ;\
	syscall 0

#define ret return $r63                          ;\
        ;;

#ifndef PIC
#define SYSCALL_ERROR_HANDLER \
        goto    __syscall_error ;\
        ;;
#else
# if defined _LIBC_REENTRANT && !RTLD_PRIVATE_ERRNO
#  if USE___THREAD
#   ifndef NOT_IN_libc
#    define SYSCALL_ERROR_ERRNO __libc_errno
#   else
#    define SYSCALL_ERROR_ERRNO errno
#   endif
#   define SYSCALL_ERROR_HANDLER                   \
        mov     $r9        = $r63                 ;\
        call    $r63       = 1f                   ;\
        ;;                                        ;\
1:      add     $r8        = $r63, @neggprel(1b)  ;\
        mov     $r63       = $r9                  ;\
        ;;                                        ;\
        ldw     $r8        = @gotoff(@tprel(SYSCALL_ERROR_ERRNO))[$r8] ;\
        ;;                                        ;\
        add     $r8        = $r8, $r13            ;\
        ;;                                        ;\
        stw     0[$r8]     = $r16
#  else
/* Store r16 into location returned by __errno_location */
/* Note GP (r14) needs to be set up before call to __errno_location
   in case that call goes through the PLT. */
#   define SYSCALL_ERROR_HANDLER                   \
        add     $r12       = $r12, -32            ;\
        stw     -16[$r12]  = $r63                 ;\
        ;;                                        ;\
        stw     20[$r12]   = $r1                  ;\
	mov     $r1        = $r16                 ;\
        call    $r63       = 1f                   ;\
        ;;                                        ;\
1:      stw     24[$r12]   = $r14                 ;\
        add     $r14       = $r63, @neggprel(1b)  ;\
        call    $r63       = __errno_location     ;\
        ;;                                        ;\
        ldw     $r63       = 16[$r12]             ;\
        ;;                                        ;\
        stw     0[$r16]    = $r1                  ;\
        ;;                                        ;\
        ldw     $r1        = 20[$r12]             ;\
        ;;                                        ;\
        ldw     $r14       = 24[$r12]             ;\
	add     $r12       = $r12, 32
#  endif /* USE___THREAD */
# else
/* Store r16 into errno.  */
/* Need to set up GP so we can load the address of errno.
   We can put the GP into a scratch register (r8) because
   we are in a leaf routine, this avoids having to callee-save
   r14.  */
#  ifdef RTLD_PRIVATE_ERRNO
#    define SYSCALL_ERROR_HANDLER_ERRNO rtld_errno
#  else
#    define SYSCALL_ERROR_HANDLER_ERRNO errno
#  endif
#  define SYSCALL_ERROR_HANDLER	                    \
        mov     $r9        = $r63                  ;\
        call    $r63       = 1f                    ;\
        ;;                                         ;\
1:      add     $r8        = $r63, @neggprel(1b)   ;\
        mov     $r63       = $r9                   ;\
        ;;                                         ;\
        ldw     $r8        = @gotoff(SYSCALL_ERROR_HANDLER_ERRNO)[$r8]   ;\
        ;;                                         ;\
        stw     0[$r8]     = $r16
# endif	/* _LIBC_REENTRANT */
#endif /* PIC */

#else /* not __ASSEMBLER__ */

#define ASMFMT_0
#define ASMFMT_1 \
	ASMFMT_0, "r" (r16)
#define ASMFMT_2 \
	ASMFMT_1, "r" (r17)
#define ASMFMT_3 \
	ASMFMT_2, "r" (r18)
#define ASMFMT_4 \
	ASMFMT_3, "r" (r19)
#define ASMFMT_5 \
	ASMFMT_4, "r" (r20)
#define ASMFMT_6 \
	ASMFMT_5, "r" (r21)
#define ASMFMT_7 \
	ASMFMT_6, "r" (r22)

#define inline_syscall_clobbers                                         \
                       "memory",                                        \
                       "r8",  "r9",  "r10", "r11",                      \
                                                                 "r23", \
                       "r24", "r25", "r26", "r27", "r28", "r29", "r30", \
                       "r31", "r32", "r33", "r34", "r35", "r36", "r37", \
                       "r38", "r39", "r40", "r41", "r42", "r43", "r44", \
                       "r45", "r46", "r47", "r48", "r49", "r50", "r51", \
                       "r52", "r53", "r54", "r55", "r56", "r57", "r58", \
                       "r59", "r60", "r61", "r62",                      \
                       "b0", "b1", "b2", "b3", "b4", "b5", "b6", "b7"

#define INLINE_SYSCALL_CLOBBERS_7 inline_syscall_clobbers
#define INLINE_SYSCALL_CLOBBERS_6 \
        INLINE_SYSCALL_CLOBBERS_7 , "r22"
#define INLINE_SYSCALL_CLOBBERS_5 \
        INLINE_SYSCALL_CLOBBERS_6 , "r21"
#define INLINE_SYSCALL_CLOBBERS_4 \
        INLINE_SYSCALL_CLOBBERS_5 , "r20"
#define INLINE_SYSCALL_CLOBBERS_3 \
        INLINE_SYSCALL_CLOBBERS_4 , "r19"
#define INLINE_SYSCALL_CLOBBERS_2 \
        INLINE_SYSCALL_CLOBBERS_3 , "r18"
#define INLINE_SYSCALL_CLOBBERS_1 \
        INLINE_SYSCALL_CLOBBERS_2 , "r17"
#define INLINE_SYSCALL_CLOBBERS_0 \
        INLINE_SYSCALL_CLOBBERS_1

#define SUBSTITUTE_ARGS_0()
#define SUBSTITUTE_ARGS_1(arg1)                                     \
        register long r16 __asm__("r16") = (long)(arg1)

#define SUBSTITUTE_ARGS_2(arg1, arg2)                               \
        SUBSTITUTE_ARGS_1(arg1);                                    \
        register long r17 __asm__("r17") = (long)(arg2)

#define SUBSTITUTE_ARGS_3(arg1, arg2, arg3)                         \
        SUBSTITUTE_ARGS_2(arg1, arg2);                              \
        register long r18 __asm__("r18") = (long)(arg3)

#define SUBSTITUTE_ARGS_4(arg1, arg2, arg3, arg4)                   \
        SUBSTITUTE_ARGS_3(arg1, arg2, arg3);                        \
        register long r19 __asm__("r19") = (long)(arg4)

#define SUBSTITUTE_ARGS_5(arg1, arg2, arg3, arg4, arg5)             \
        SUBSTITUTE_ARGS_4(arg1, arg2, arg3, arg4);                  \
        register long r20 __asm__("r20") = (long)(arg5)

#define SUBSTITUTE_ARGS_6(arg1, arg2, arg3, arg4, arg5, arg6)       \
        SUBSTITUTE_ARGS_5(arg1, arg2, arg3, arg4, arg5);            \
        register long r21 __asm__("r21") = (long)(arg6)

#define SUBSTITUTE_ARGS_7(arg1, arg2, arg3, arg4, arg5, arg6, arg7) \
        SUBSTITUTE_ARGS_6(arg1, arg2, arg3, arg4, arg5, arg6);      \
        register long r22 __asm__("r22") = (long)(arg7)

#undef INLINE_SYSCALL
#define INLINE_SYSCALL(name, nr, args...)                   \
  ({                                                        \
     INTERNAL_SYSCALL_DECL(err);                            \
     int _retval;                                           \
     _retval = INTERNAL_SYSCALL(name, err, nr, args);       \
     if (INTERNAL_SYSCALL_ERROR_P(_ retval, err))           \
       {                                                    \
	 __set_errno (INTERNAL_SYSCALL_ERRNO(_retval, err)); \
         _retval = -1;                                      \
       }                                                    \
    (int) _retval; })

#undef INTERNAL_SYSCALL_DECL
#define INTERNAL_SYSCALL_DECL(err) int err

#undef INTERNAL_SYSCALL
#define INTERNAL_SYSCALL_NCS(name, err, nr, args...)        \
  ({                                                        \
    int resultvar;                                          \
    register long r15 __asm__ ("r15") = name;		    \
    SUBSTITUTE_ARGS_##nr(args);                             \
    __asm__ volatile (                                      \
      "syscall 0                                        \n" \
      ";;                                               \n" \
      "mov     %0 = $r16                                \n" \
      "mfb     %1 = $b0                                 \n" \
      ";;                                               \n" \
               : "=&r" (resultvar), "=&r" (err)             \
               : "r"  (r15) ASMFMT_##nr                     \
               : INLINE_SYSCALL_CLOBBERS_##nr);             \
    (int) resultvar; })
#define INTERNAL_SYSCALL(name, err, nr, args...)	\
  INTERNAL_SYSCALL_NCS (__NR_##name, err, nr, ##args)

#undef INTERNAL_SYSCALL_ERROR_P
#define INTERNAL_SYSCALL_ERROR_P(val, err)      (err)

#undef INTERNAL_SYSCALL_ERRNO
#define INTERNAL_SYSCALL_ERRNO(val, err)	(val)

#endif	/* __ASSEMBLER__ */

/* Pointer mangling support.  */
#if defined NOT_IN_libc && defined IS_IN_rtld
/* We cannot use the thread descriptor because in ld.so we use setjmp
   earlier than the descriptor is initialized.  */
#else
# ifdef __ASSEMBLER__
#  define PTR_MANGLE(reg, tmpreg)                   \
          ldw     tmpreg=-8[$r13]                  ;\
	  ;;			                   ;\
	PTR_MANGLE2(reg, tmpreg)
#  define PTR_MANGLE2(reg, tmpreg) \
        xor	reg=reg, tmpreg
#  define PTR_DEMANGLE(reg, tmpreg)  PTR_MANGLE (reg, tmpreg)
#  define PTR_DEMANGLE2(reg, tmpreg) PTR_MANGLE2 (reg, tmpreg)
# else
#  define PTR_MANGLE(var) \
  (var) = (__typeof (var)) ((uintptr_t) (var) ^ THREAD_GET_POINTER_GUARD ())
#  define PTR_DEMANGLE(var)	PTR_MANGLE (var)
# endif
#endif

#endif /* linux/st200/sysdep.h */
