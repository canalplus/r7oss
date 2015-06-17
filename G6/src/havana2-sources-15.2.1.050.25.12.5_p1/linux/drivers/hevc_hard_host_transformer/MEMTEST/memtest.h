/************************************************************************
Copyright (C) 2012 STMicroelectronics. All Rights Reserved.

This file is part of the Streaming Engine Library.

Streaming Engine is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License version 2 as published by the
Free Software Foundation.

Streaming Engine is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with Streaming Engine; see the file COPYING.  If not, write to the Free Software
Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

The Streaming Engine Library may alternatively be licensed under a proprietary
license from ST.

Source file name : memtest.h

Description: memory testing module API

************************************************************************/
#ifndef H_MEMTEST_API
#define H_MEMTEST_API

#include <linux/types.h>

// CIC plugs: Firmware code
// Hades Addresses: Base runtime boot zone (9DDD'D000), PEDF reloc zone (8000'0000) and L3 (8200'0000)
//  Host Addresses: Base runtime boot zone (7800'0000), PEDF reloc zone (7A00'0000) and L3 (4100'0000)
#define MT_SOURCE_CODE_MIN 2
#define MT_SOURCE_CODE_MAX 8
#define MT_SOURCE_CODE_ID    0x00
#define MT_SOURCE_CODE_MASK  0x0F

// DIC plugs : HWPEs buffers
// Addresses: L3 (4100'0000 => 4F60'0000)
// Buffers: read BB, IB; write O4, PPB, R2B
#define MT_SOURCE_DATA_HWPE 18

// DIC plugs : Firmware data
// Addresses: L3 (4100'0000 => 4F60'0000)
// Buffers: PEDF run command, run command payload, MME command, MME status
#define MT_SOURCE_DATA_FIRMWARE 17

// backdoor plug: XPRED reference access thru MCC
// Addresses: reference omega buffers (luma + chroma)
// Buffers: O4/read
#define MT_SOURCE_XPRED 50

// PP write plugs
// Addresses: L3 (4100'0000 => 4F60'0000)
// Buffers: IB
#define MT_SOURCE_PP_ST 32 // Slice table
#define MT_SOURCE_PP_SH 33 // Slice headers
#define MT_SOURCE_PP_CT 34 // CTB table
#define MT_SOURCE_PP_CC 35 // CTB commands
#define MT_SOURCE_PP_CR 36 // CTB residuals

// PP read plugs
// Addresses: L3 (4100'0000 => 4F60'0000)
// Buffers: BB
#define MT_SOURCE_PP_BB 40 // Bit buffer

// Mask for mono-plug sources
#define MT_SOURCE_MONO_MASK 0


typedef struct
{
	const char* name;
	uint32_t  start;  // physical
	uint32_t  length; // bytes
} MT_AddressRange_t;

#define MT_MAX_RANGES 100

void MT_OpenMonitoring(const char* hardware, uint32_t decode_index);
void MT_MonitorBuffers(uint32_t source_id, uint32_t source_mask, int buffers_number, MT_AddressRange_t ranges[]);
void MT_StartMonitoring(int skip);
void MT_StopMonitoring(void);
void MT_LogPass(int requested);
void MT_LogFail(const char* description, uint32_t source, uint32_t address);

#endif // H_MEMTEST_API
