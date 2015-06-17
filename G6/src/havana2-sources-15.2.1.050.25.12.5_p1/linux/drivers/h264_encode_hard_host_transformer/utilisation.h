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

#ifndef UTILISATION_H_
#define UTILISATION_H_

#include <linux/sysfs.h>

extern struct HvaDriverData *pDriverData;

struct utilisation {
	unsigned long long previous_end;
	unsigned long long active;
	unsigned long long idle;
	unsigned long long total;
};

static inline void utilisation_reset(struct utilisation *u)
{
	u->previous_end = 0;
	u->active       = 0;
	u->idle         = 0;
	u->total        = 0;
}

static inline void utilisation_update(struct utilisation *u, unsigned long long start, unsigned long long end)
{
	// Todo: Make this safe for Wrapping!
	if (end < start) {
		dev_dbg(pDriverData->dev, "Resetting utilisation based on wrap: start = %llu, end = %llu\n", start, end);
		utilisation_reset(u); // Reset utilisations due to timer wrap.
	}

	if (u->previous_end == 0) {
		u->previous_end = end;
		return;
		// Cannot continue without existing state.
	}

	u->active      += end   - start;
	u->idle        += start - u->previous_end;
	u->total       += end   - u->previous_end;   // (Adds both idle and active time [ previous ] idle [ active ]
	// Store State;
	u->previous_end = end;
}


unsigned int utilisation_calculate(struct utilisation *u)
{
	unsigned long long utilisation = 0;

	if (u->total == 0) {
		return 0;
	}

	// Utilisation = (Active / Total) * 100
	// However to maintain precision and avoid overflow because we have large numbers...
	// Utilisation = Active / (Total / 100)

	// utilisation = (u->active / u->total) * 100;
	utilisation = u->active * 100;
	utilisation *= 100; // Add two decimal points of precision to the integer calculation...
	do_div(utilisation, u->total);

	dev_dbg(pDriverData->dev, "Utilisation: active = %llu, idle = %llu, total = %llu, utilisation = %llu\n", u->active,
	        u->idle, u->total, utilisation);

	// Alternate
	// utilisation = u->active / (u->total / 100);

	// Reset for the next iteration
	utilisation_reset(u);

	return (unsigned int)utilisation;
}

#define DEFINE_UTILISATION(x) struct utilisation x = { 0 }

#define SYSFS_DECLARE_UTILISATION( VarName )                                                       \
static DEFINE_UTILISATION( VarName );                                                              \
static ssize_t SysfsShow_##VarName( struct device *dev, struct device_attribute *attr, char *buf ) \
{                                                                                                  \
    unsigned int util = utilisation_calculate( &VarName );                                         \
    return sprintf(buf, "%d\n", util);                                                             \
}                                                                                                  \
                                                                                                   \
static DEVICE_ATTR(VarName, S_IRUGO, SysfsShow_##VarName, NULL)


#endif /* UTILISATION_H_ */
