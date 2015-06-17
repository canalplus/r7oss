/* Definition of stack frame structure.  ST200 version.
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

/* ST200 compiler does not yet support the gcc builtins required for
   backtrace.  So this is all stubbed out - backtracing is not
   supported.
*/

struct layout
{
  void *__unbounded next;
  void *__unbounded return_address;
};

#define FIRST_FRAME_POINTER (0)
#define ADVANCE_STACK_FRAME(next) (0)
