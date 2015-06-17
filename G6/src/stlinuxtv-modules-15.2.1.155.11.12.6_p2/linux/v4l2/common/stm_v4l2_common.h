/************************************************************************
Copyright (C) 2013 STMicroelectronics. All Rights Reserved.

This file is part of the STLInuxTV Library.

STLInuxTV is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License version 2 as published by the
Free Software Foundation.

STLInuxTV is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with player2; see the file COPYING.  If not, write to the Free Software
Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

The STLInuxTV Library may alternatively be licensed under a proprietary
license from ST.
 *  Support common functions for st v4l2 driver
************************************************************************/

#ifndef STM_V4L2_COMMON_H
#define STM_V4L2_COMMON_H

int stm_v4l2_convert_v4l2_SE_define(int def, const int table[][2], unsigned int table_length);
int stm_v4l2_convert_SE_v4l2_define(int def, const int table[][2], unsigned int table_length);

long long stm_v4l2_get_systemtime_us(void);

#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 7, 0))
#define STM_V4L2_FUNC(func, file, fh, ptr)	func(file, fh, ptr)
#define STM_V4L2_FUNC1(func, fh, ptr)		func(fh, ptr)
#else
#define STM_V4L2_FUNC(func, file, fh, ptr)	func(file, fh, const ptr)
#define STM_V4L2_FUNC1(func, fh, ptr)		func(fh, const ptr)
#endif

#endif
