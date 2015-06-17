/* 64-bit multiplication and division
   Copyright (C) 2003
   Free Software Foundation, Inc.
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

/* clarkes:
   ST200 compiler does not use standard gcc names for these
   functions, for example it uses __divl instead of __divdi3.
   Compiler-generated calls to __divl pull in the appropriate module
   of libarith.a, but that also contains a definition of
   __divdi3 (which is the same code as __divl).  That definition
   then clashes with the glibc definition of __divdi3, which is normally
   in this file.
   So why does glibc need to define __divdi3 et al.?  Not entirely
   sure at the moment: there are clues on the glibc mailing list -
   something to do with the version in libgcc.so being hidden.
   This will need to be revisited when we have a libgcc.so.
*/

/* Prototypes of exported functions.  */
extern long long __divdi3_internal (long long u, long long v);
extern long long __moddi3_internal (long long u, long long v);
extern unsigned long long __udivdi3_internal (unsigned long long u,
					      unsigned long long v);
extern unsigned long long __umoddi3_internal (unsigned long long u,
					      unsigned long long v);

long long
__divdi3_internal (long long u, long long v)
{
  return __divl (u, v);
}

long long
__moddi3_internal (long long u, long long v)
{
  return __modl (u, v);
}

unsigned long long
__udivdi3_internal (unsigned long long u, unsigned long long v)
{
  return __divul (u, v);
}

unsigned long long
__umoddi3_internal (unsigned long long u, unsigned long long v)
{
  return __modul (u, v);
}

/* We declare these with compat_symbol so that they are not visible at
   link time.  Programs must use the functions from libgcc.  */
#if defined HAVE_ELF && defined SHARED && defined DO_VERSIONING
# include <shlib-compat.h>
compat_symbol (libc, __divdi3_internal, __divdi3, GLIBC_2_0);
compat_symbol (libc, __moddi3_internal, __moddi3, GLIBC_2_0);
compat_symbol (libc, __udivdi3_internal, __udivdi3, GLIBC_2_0);
compat_symbol (libc, __umoddi3_internal, __umoddi3, GLIBC_2_0);
#endif
