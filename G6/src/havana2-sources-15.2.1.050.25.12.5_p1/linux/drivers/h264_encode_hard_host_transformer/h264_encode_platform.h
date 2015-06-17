/************************************************************************
Copyright (C) 2003-2014 STMicroelectronics. All Rights Reserved.

This file is part of the Streaming Engine.

Streaming Engine is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

Streaming Engine is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with Streaming Engine; see the file COPYING.  If not, write to the Free
Software Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307,
USA.

The Streaming Engine Library may alternatively be licensed under a
proprietary license from ST.
************************************************************************/

#ifndef H_H264_ENCODE_PLATFORM
#define H_H264_ENCODE_PLATFORM

//HVA driver should manage IRQ_ITS & IRQ_ERR
#define HVA_INTERRUPT_NB    2
//HVA driver should manage HVA registers & HVA ESRAM
#define HVA_BASE_ADDRESS_NB 2
//HVA driver should manage 1 only clock
#define HVA_CLKS_NR 1

//used to store platform information retrieved from platform device "hva_device_<SOC>"
//ie IRQ & base addresses from platform_device resources
//   clock HW name        from platform_data
typedef struct HVAPlatformData_s {
	uint32_t   BaseAddress[HVA_BASE_ADDRESS_NB];
	uint32_t   Size[HVA_BASE_ADDRESS_NB];
	uint32_t   Interrupt[HVA_INTERRUPT_NB];
	spinlock_t hw_lock;
} HVAPlatformData_t;

#endif // H_H264_ENCODE_PLATFORM
