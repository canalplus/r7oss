/************************************************************************
Copyright (C) 2010 STMicroelectronics. All Rights Reserved.

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

Source file name : unified.c
Author :           Pete

Date        Modification                                    Name
----        ------------                                    --------
20-Feb-10   Created                                         Pete

************************************************************************/

#include <linux/io.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/autoconf.h>

extern initcall_t  __initcall_stm_start6[], __initcall_stm_end6[];
extern exitcall_t  __exitcall_stm_start[], __exitcall_stm_end[];

static __init int stm_load_player2_unified(void)
{
	initcall_t *call;

        for (call = __initcall_stm_start6; call < __initcall_stm_end6; call++)
	{
		int ret;
		ret = (*call)();
		if (ret)
			printk("The modules at %p failed to load\n",call);
	}
	
	return 0;
}

static __init void stm_unload_player2_unified(void)
{
	exitcall_t *call;

        for (call = __exitcall_stm_end; call >= __exitcall_stm_start; call--)
	{
		(*call)();
	}
}

module_init(stm_load_player2_unified);
module_exit(stm_unload_player2_unified);
MODULE_DESCRIPTION("Player2 Unified Driver");
MODULE_AUTHOR("STMicroelectronics Limited");
MODULE_VERSION("1.0");
MODULE_LICENSE("GPL");
