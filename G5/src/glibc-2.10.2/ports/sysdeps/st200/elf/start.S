/* Startup code for ST200 and ELF.
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

#include <sysdep.h>

#define sp $r0.12
#define gp $r0.14

/* This is the canonical entry point, usually the first thing in the text
   segment.

	Note that the code in the .init section has already been run.
	This includes _init and _libc_init

	At this entry point, most registers' values are unspecified, except:

   r16		Contains a function pointer to be registered with `atexit'.
		This is how the dynamic linker arranges to have DT_FINI
		functions called for shared libraries that have been loaded
		before this code runs.

   sp		The stack contains the arguments and environment:
		0(sp)			argc
		4(sp)			argv[0]
		...
		(4*argc)(sp)		NULL
		(4*(argc+1))(sp)	envp[0]
		...
					NULL
*/
	.global _start

	.proc
	.type _start,@function
_start:
	/* Allocate scratch area for callees, preserve stack alignment */
	add	sp  = sp, -32
	call	$r0.63 = 0f
	;;
0:	add     gp = $r0.63, @neggprel(0b) /* Note that gprel(0b) is text-data calc. */
	mov	$r0.21 = $r0.16    /* rtld_fini */
	;;
	/* QQQ:	 Do we need to do anything for backtracing here? */
	/* Call __libc_start_main(main, argc, argv, init, fini, rtld_fini,
	                          stack_end) */

	ldw	$r0.16 = @gotoff(main)[gp]
	;;
	ldw	$r0.17 = 32[sp]
	add	$r0.18 = sp, 36
	;;
	ldw	$r0.19 = @gotoff(__libc_csu_init)[gp]
	;;
	ldw	$r0.20 = @gotoff(__libc_csu_fini)[gp]
	/* r21 already contains rtld_fini */
	add	$r0.22 = sp, 32
	;;
	call	$r0.63 = __libc_start_main
	;;
	/* Die	horribly if we ever come back from here */
	call	$r0.63 = abort
	;;
	.endp

/* Define a symbol for the first piece of initialized data.  */
	.data
	.globl __data_start
__data_start:
	.long 0
	.weak data_start
	data_start = __data_start
