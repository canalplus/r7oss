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

Source file name : end.c
Author :           Pete

Date        Modification                                    Name
----        ------------                                    --------
20-Feb-10   Created                                         Pete

************************************************************************/

#include <linux/io.h>
#include <linux/init.h>
#include <linux/autoconf.h>

static __init int stm_init(void)
{
	return 0;
}

static __exit void stm_exit(void)
{

}
initcall_t  __initcall_stm_end6 __attribute_used__ __attribute__((__section__(".initcall6.init"))) = stm_init;
exitcall_t  __exitcall_stm_end __attribute_used__ __attribute__((__section__(".exitcall.exit"))) = stm_exit;
