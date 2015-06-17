/************************************************************************
Copyright (C) 2000 STMicroelectronics. All Rights Reserved.

This file is part of the Player2 Library.

Player2 is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License version 2 as published by the
Free Software Foundation.

Player2 is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with player2; see the file COPYING.  If not, write to the Free Software
Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

The Player2 Library may alternatively be licensed under a proprietary
license from ST.

Source file name : aeabi_ldivmod.c (kernel version)
Author :           Kieran

Contains wrapper for 64 bit divides on ARM cores for TransportEngine kernel
modules.

Date        Modification                                    Name
----        ------------                                    --------
28-Jul-11   Created                                         Kieran

************************************************************************/
#include <stm_te_dbg.h>
#include <linux/math64.h>
#include <linux/module.h>

uint64_t __aeabi_uldivmod(uint64_t n, uint64_t d)
{
	if (unlikely(d & 0xffffffff00000000ULL)) {
		pr_warn("TE: 64-bit/64-bit div loses precision (0x%llx/0x%llx)\n",
				n, d);
		BUG();
	}

	return div64_u64(n, d);
}

int64_t __aeabi_ldivmod(int64_t n, int64_t d)
{
	int c = 0;
	s64 res;

	if (n < 0LL) {
		c = ~c;
		n = -n;
	}

	if (d < 0LL) {
		c = ~c;
		d = -d;
	}

	if (unlikely(d & 0xffffffff00000000ULL)) {
		pr_warn("TE: 64-bit/64-bit div loses precision (0x%llx/0x%llx)\n",
				n, d);
		BUG();
	}

	res = div64_u64(n, d);

	if (c)
		res = -res;

	return res;
}

