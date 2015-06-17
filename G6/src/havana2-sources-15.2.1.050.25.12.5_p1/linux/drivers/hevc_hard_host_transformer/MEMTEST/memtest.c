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

Source file name : memtest.c

Description: memory testing module

************************************************************************/

#include <linux/platform_device.h>
#include <linux/poll.h>
#include <linux/ptrace.h>
#include <linux/sort.h>

#include "st_relayfs_se.h"
#include "memtest.h"
#include "sbag.h"

static struct semaphore MT_mutex;
static const char* MT_hardware;
static uint32_t MT_decode_index;

static int compare_range(const void *left, const void *right)
{
	MT_AddressRange_t* left_range = (MT_AddressRange_t*)left;
	MT_AddressRange_t* right_range = (MT_AddressRange_t*)right;
	if (left_range->start < right_range->start)
		return -1;
	else if (left_range->start == right_range->start)
		return 0;
	else
		return 1;
}

void MT_MonitorBuffers(uint32_t source_id, uint32_t source_mask, int buffers_number, MT_AddressRange_t allowed_ranges[])
{
	MT_AddressRange_t forbidden_ranges[MT_MAX_RANGES];
	static char descriptions[MT_MAX_RANGES][100]; // too big for the stack
	int allowed, forbidden = 0, i;

	if (buffers_number >= MT_MAX_RANGES)
	{
		pr_err("Error: Hades: Memtest: too many buffers to monitor\n");
		return;
	}

	if (buffers_number == 1)
	{
		// Only 1 range for the source => watch exterior of buffer
		SBAG_SetWatchpoint(allowed_ranges[0].name, allowed_ranges[0].start, allowed_ranges[0].length, 0, source_id, source_mask);
		return;
	}

	// Sort allowed ranges in order of increasing addresses
	sort(allowed_ranges, buffers_number, sizeof(MT_AddressRange_t), compare_range, NULL);

	// Compute forbidden ranges
	if (allowed_ranges[0].start != 0)
	{
		snprintf(descriptions[0], sizeof(descriptions[0]), "before %s", allowed_ranges[0].name);
		descriptions[0][sizeof(descriptions[0]) - 1] = '\0';
		forbidden_ranges[0].start = 0;
		forbidden_ranges[0].length = allowed_ranges[0].start;
		++forbidden;
	}
	for (allowed = 0; allowed < buffers_number - 1; allowed++)
	{
		if (allowed_ranges[allowed].start + allowed_ranges[allowed].length > allowed_ranges[allowed + 1].start)
		{
			pr_err("Error: Hades: Memtest: overlapping buffers\n");
			return;
		}
		if (allowed_ranges[allowed].start + allowed_ranges[allowed].length != allowed_ranges[allowed + 1].start)
		{
			snprintf(descriptions[forbidden], sizeof(descriptions[forbidden]), "%s - %s",
			         allowed_ranges[allowed].name, allowed_ranges[allowed + 1].name);
			descriptions[forbidden][sizeof(descriptions[forbidden]) - 1] = '\0';
			forbidden_ranges[forbidden].start = allowed_ranges[allowed].start + allowed_ranges[allowed].length;
			forbidden_ranges[forbidden].length = allowed_ranges[allowed + 1].start - forbidden_ranges[forbidden].start;
			++forbidden;
		}
	}
	if (allowed_ranges[buffers_number - 1].start + (allowed_ranges[buffers_number - 1].length - 1) != 0xFFFFFFFF)
	{
		snprintf(descriptions[forbidden], sizeof(descriptions[forbidden]), "after %s", allowed_ranges[buffers_number - 1].name);
		descriptions[forbidden][sizeof(descriptions[forbidden]) - 1] = '\0';
		forbidden_ranges[forbidden].start = allowed_ranges[buffers_number - 1].start + allowed_ranges[buffers_number - 1].length;
		forbidden_ranges[forbidden].length = (0xFFFFFFFF - forbidden_ranges[forbidden].start) + 1;
		++forbidden;
	}

	if (forbidden > MT_MAX_RANGES)
	{
		pr_err("Error: Hades: Memtest: keeping only %d ranges to monitor\n", MT_MAX_RANGES);
		forbidden = MT_MAX_RANGES;
	}

	for (i = 0; i < forbidden; i++)
		SBAG_SetWatchpoint(descriptions[i], forbidden_ranges[i].start, forbidden_ranges[i].length, 1, source_id, source_mask);
}

void MT_OpenMonitoring(const char* hardware, uint32_t decode_index)
{
	static bool initialized = false;

	if (! initialized)
	{
		// This works only because the PP starts well before the Decoder
		sema_init(&MT_mutex, 1);
		initialized = true;
	}

	down(&MT_mutex);
	MT_hardware = hardware;
	MT_decode_index = decode_index;
}

void MT_StartMonitoring(int skip)
{
	SBAG_Start(skip);
}

void MT_StopMonitoring(void)
{
	SBAG_Result(MT_hardware);
	up(&MT_mutex);
}

void MT_LogPass(int requested)
{
	st_relayfs_print_se(ST_RELAY_TYPE_HEVC_MEMORY_CHECK, ST_RELAY_SOURCE_SE,
			"%s: decode index %d: PASS, requested %d\n", MT_hardware, MT_decode_index, requested);
}

void MT_LogFail(const char* description, uint32_t source, uint32_t address)
{
	st_relayfs_print_se(ST_RELAY_TYPE_HEVC_MEMORY_CHECK, ST_RELAY_SOURCE_SE,
			"%s: decode index %d: FAIL: source %d: %s, address %#08x\n",
			MT_hardware, MT_decode_index, source, description, address);
}
