/**
 * \file    st_bios.h
 * \author  Carl Shaw <carl.shaw@st.com>
 * \version 1.0
 * \brief   Adds support for calling ST BIOS functions
 *
 * This file contains support for calling BIOS functions from C code
 * via an inline macro
 *
 * Copyright (c) 2009 STMicroelectronics Limited.  All rights reserved.
 */

#ifndef ST_BIOS_H
#define ST_BIOS_H

#ifndef __ASSEMBLY__

#define SET_ARGS_0()
#define SET_ARGS_1(arg0)						\
       long int _arg0 = (long int) (arg0);				\
       register long int r4 __asm__ ("%r4") = (long int) (_arg0)
#define SET_ARGS_2(arg0, arg1)						\
       long int _arg0 = (long int) (arg0);				\
       long int _arg1 = (long int) (arg1);				\
       register long int r4 __asm__ ("%r4") = (long int) (_arg0);	\
       register long int r5 __asm__ ("%r5") = (long int) (_arg1)
#define SET_ARGS_3(arg0, arg1, arg2)					\
       long int _arg0 = (long int) (arg0);				\
       long int _arg1 = (long int) (arg1);				\
       long int _arg2 = (long int) (arg2);				\
       register long int r4 __asm__ ("%r4") = (long int) (_arg0);	\
       register long int r5 __asm__ ("%r5") = (long int) (_arg1);	\
       register long int r6 __asm__ ("%r6") = (long int) (_arg2)
#define SET_ARGS_4(arg0, arg1, arg2, arg3)				\
       long int _arg0 = (long int) (arg0);				\
       long int _arg1 = (long int) (arg1);				\
       long int _arg2 = (long int) (arg2);				\
       long int _arg3 = (long int) (arg3);				\
       register long int r4 __asm__ ("%r4") = (long int) (_arg0);	\
       register long int r5 __asm__ ("%r5") = (long int) (_arg1);	\
       register long int r6 __asm__ ("%r6") = (long int) (_arg2);	\
       register long int r7 __asm__ ("%r7") = (long int) (_arg3)
#define SET_ARGS_5(arg0, arg1, arg2, arg3, arg4)			\
       long int _arg0 = (long int) (arg0);				\
       long int _arg1 = (long int) (arg1);				\
       long int _arg2 = (long int) (arg2);				\
       long int _arg3 = (long int) (arg3);				\
       long int _arg4 = (long int) (arg4);				\
       register long int r4 __asm__ ("%r4") = (long int) (_arg0);	\
       register long int r5 __asm__ ("%r5") = (long int) (_arg1);	\
       register long int r6 __asm__ ("%r6") = (long int) (_arg2);	\
       register long int r7 __asm__ ("%r7") = (long int) (_arg3);	\
       register long int r0 __asm__ ("%r0") = (long int) (_arg4)
#define SET_ARGS_6(arg0, arg1, arg2, arg3, arg4, arg5)			\
       long int _arg0 = (long int) (arg0);				\
       long int _arg1 = (long int) (arg1);				\
       long int _arg2 = (long int) (arg2);				\
       long int _arg3 = (long int) (arg3);				\
       long int _arg4 = (long int) (arg4);				\
       long int _arg5 = (long int) (arg5);				\
       register long int r4 __asm__ ("%r4") = (long int)(_arg0);	\
       register long int r5 __asm__ ("%r5") = (long int) (_arg1);	\
       register long int r6 __asm__ ("%r6") = (long int) (_arg2);	\
       register long int r7 __asm__ ("%r7") = (long int) (_arg3);	\
       register long int r0 __asm__ ("%r0") = (long int) (_arg4);	\
       register long int r1 __asm__ ("%r1") = (long int) (_arg5)
#define SET_ARGS_7(arg0, arg1, arg2, arg3, arg4, arg5, arg6)		\
       long int _arg0 = (long int) (arg0);				\
       long int _arg1 = (long int) (arg1);				\
       long int _arg2 = (long int) (arg2);				\
       long int _arg3 = (long int) (arg3);				\
       long int _arg4 = (long int) (arg4);				\
       long int _arg5 = (long int) (arg5);				\
       long int _arg6 = (long int) (arg6);				\
       register long int r4 __asm__ ("%r4") = (long int) (_arg0);	\
       register long int r5 __asm__ ("%r5") = (long int) (_arg1);	\
       register long int r6 __asm__ ("%r6") = (long int) (_arg2);	\
       register long int r7 __asm__ ("%r7") = (long int) (_arg3);	\
       register long int r0 __asm__ ("%r0") = (long int) (_arg4);	\
       register long int r1 __asm__ ("%r1") = (long int) (_arg5);	\
       register long int r2 __asm__ ("%r2") = (long int) (_arg6)

#define BIOS_TRAP0       "trapa #0x40\n\t"
#define BIOS_TRAP1       "trapa #0x41\n\t"
#define BIOS_TRAP2       "trapa #0x42\n\t"
#define BIOS_TRAP3       "trapa #0x43\n\t"
#define BIOS_TRAP4       "trapa #0x44\n\t"
#define BIOS_TRAP5       "trapa #0x45\n\t"
#define BIOS_TRAP6       "trapa #0x46\n\t"
#define BIOS_TRAP7       "trapa #0x47\n\t"

#define REGS_0
#define REGS_1\
       , "r" (r4)
#define REGS_2\
       , "r" (r4), "r" (r5)
#define REGS_3\
       , "r" (r4), "r" (r5), "r" (r6)
#define REGS_4\
       , "r" (r4), "r" (r5), "r" (r6), "r" (r7)
#define REGS_5\
       , "r" (r4), "r" (r5), "r" (r6), "r" (r7), "0" (r0)
#define REGS_6\
       , "r" (r4), "r" (r5), "r" (r6), "r" (r7), "0" (r0), "r" (r1)
#define REGS_7\
       , "r" (r4), "r" (r5), "r" (r6), "r" (r7), "0" (r0), "r" (r1), "r" (r2)

#define INLINE_BIOS(call, numargs, args...)			\
 ({								\
    unsigned long int result;					\
    register long int r3 __asm__ ("%r3") = call;		\
    SET_ARGS_##numargs(args);					\
								\
    __asm__ __volatile__ (BIOS_TRAP##numargs			\
		 : "=z" (result)				\
		 : "r" (r3) REGS_##numargs			\
		 : "memory");					\
								\
    (int) result;						\
 })

#endif /* __ASSEMBLY__ */
#endif /* ST_BIOS_H */

