/* Nested function trampoline for ST200.
   Copyright (C) 2004 Free Software Foundation, Inc.
   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the GNU C Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.  */

#include <string.h>

static const int even_trampoline_template[] =
  {
    0x00009fc0, /* mov $r9 = $r63 */
    0x08000fc0, /* mov $r63 = function & 0x1ff */
    0x95000000, /* imml (function >> 9) & 0x7fffff ;; */
    0x08000200, /* mov $r8 = context & 0x1ff */
    0x95000000, /* imml (context >> 9) & 0x7fffff ;; */
    0x31800000, /* goto $r63 */
    0x8003f240  /* mov  $r63 = $r9 ;; */
  };

static void flush_data_cache(void *addr, int sz)
{
  __st220prgadd((int)addr, 0);
  __st220prgadd((int)addr, 32);
  __st220sync();
}

static void purge_instruction_cache(void *addr, int sz)
{
  __st220prgins();
  /* No need for syncins, because the following bundle is
     known to be good. */
}

void __init_trampoline (int *trampoline,
			void *function,
			void *context)
{
  trampoline[0] = even_trampoline_template[0];
  trampoline[1] = even_trampoline_template[1]
    | (((int)function) & 0x1ff) << 12;
  trampoline[2] = even_trampoline_template[2]
    | (((int)function) >> 9) & 0x7fffff;
  trampoline[3] = even_trampoline_template[3]
    | (((int)context) & 0x1ff) << 12;
  trampoline[4] = even_trampoline_template[4]
    | (((int)context) >> 9) & 0x7fffff;
  memcpy(&trampoline[5], &even_trampoline_template[5],
	 sizeof (even_trampoline_template)
	 - (5 * sizeof(even_trampoline_template[0])));
  flush_data_cache (trampoline, sizeof(even_trampoline_template));
  purge_instruction_cache (trampoline, sizeof(even_trampoline_template));
}
