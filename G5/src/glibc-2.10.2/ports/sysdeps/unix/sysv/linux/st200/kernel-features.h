/* Set flags signalling availability of kernel features based on given
   kernel version number.
   Copyright (C) 2009 Free Software Foundation, Inc.
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

/* The first kernel for st200 was 2.6.16, and it had all of these features ... */
#if __LINUX_KERNEL_VERSION >= 132624
# define __ASSUME_TRUNCATE64_SYSCALL 1
# define __ASSUME_STAT64_SYSCALL     1
# define __ASSUME_FCNTL64            1
# define __ASSUME_UTIMES             1
#endif

#include_next <kernel-features.h>
