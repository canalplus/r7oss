/* Copyright (C) 2002, 2009 Free Software Foundation, Inc.
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

/* Define the machine-dependent type `jmp_buf'.  ST200 version. */
#ifndef _BITS_SETJMP_H
#define _BITS_SETJMP_H  1

#if !defined _SETJMP_H && !defined _PTHREAD_H
# error "Never include <bits/setjmp.h> directly; use <setjmp.h> instead."
#endif

 /* 0:    stack pointer  (r12)
  * 1:    link register  (r63)
  * 2-10: preserved regs (r1-r7, r13, r14)
  */
#define _JBLEN (2 + 9)

#define _JB_STACK_PTR  0
#define _JB_LINK       4
#define _JB_PRE(n)     (8+(n)<<2)

#ifndef __ASSEMBLER__
typedef long int __jmp_buf[_JBLEN];
#endif

#endif  /* bits/setjmp.h */
