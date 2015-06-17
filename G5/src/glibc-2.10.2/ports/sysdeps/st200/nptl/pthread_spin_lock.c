/* POSIX spinlock implementation.
   Copyright (C) 2003 Free Software Foundation, Inc.
   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public License as
   published by the Free Software Foundation; either version 2.1 of the
   License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.  */

#include <pthreadP.h>

int
pthread_spin_lock (pthread_spinlock_t *lock)
{
  unsigned int tmp;

  asm volatile ("	call $r63 = 1f \n"
		"	;;\n"
		"1:\n"
		"	mov $r62 = $r63\n"
		"	or $r12, $r12, 1\n"
		"	ldw %0 = 0[ %1 ]\n"
		"	;;\n"
		"	cmpne $b0 = %0 , 0 \n"
		"	stw 0[ %1 ] = $r62 \n"
		"	and $r12 = $r12 , ~1\n"
		"	;;\n"
		"	br $b0, 1b"
                : "=&r" (tmp)
                : "r" (lock)
                : "memory", "r62", "r63", "b0");

  return 0;
}
