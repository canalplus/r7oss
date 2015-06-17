/************************************************************************
Copyright (C) 2013 STMicroelectronics. All Rights Reserved.

This file is part of the STLinuxTV Library.

STLinuxTV is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License version 2 as published by the
Free Software Foundation.

STLinuxTV is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with player2; see the file COPYING.  If not, write to the Free Software
Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

The STLinuxTV Library may alternatively be licensed under a proprietary
license from ST.
*  Support common functions for st v4l2 driver
************************************************************************/

#include <linux/kernel.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/export.h>

int stm_v4l2_convert_v4l2_SE_define(int def, const int table[][2], unsigned int table_length)
{
	int i;
	int max_num;

	max_num = table_length/sizeof(int)/2;

	for(i=0; i<max_num; i++)
	{
		if(table[i][0] == def)
		{
			return table[i][1];
		}
	}
	return -1;
}
EXPORT_SYMBOL(stm_v4l2_convert_v4l2_SE_define);

int stm_v4l2_convert_SE_v4l2_define(int def, const int table[][2], unsigned int table_length)
{
	int i;
	int max_num;

	max_num = table_length/sizeof(int)/2;

	for(i=0; i<max_num; i++)
	{
		if(table[i][1] == def)
		{
			return table[i][0];
		}
	}
	return -1;
}
EXPORT_SYMBOL(stm_v4l2_convert_SE_v4l2_define);

unsigned long long stm_v4l2_get_systemtime_us(void)
{
	ktime_t Ktime = ktime_get();
	return ktime_to_us(Ktime);
}
EXPORT_SYMBOL(stm_v4l2_get_systemtime_us);

