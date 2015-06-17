/******************************************************************************
Copyright (C) 2011, 2012 STMicroelectronics. All Rights Reserved.

This file is part of the Transport Engine Library.

Transport Engine is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License version 2 as published by
the Free Software Foundation.

Transport Engine is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with Transport Engine; see the file COPYING.  If not, write to the Free
Software Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

The Transport Engine Library may alternatively be licensed under a proprietary
license from ST.

Source file name : te_time.c

Defines Transport Engine timestamp processing functions

Functions in this file are common to PCR and TS Index filters
******************************************************************************/

#ifndef __TE_TIME_H
#define __TE_TIME_H

#include "te_object.h"
#include "te_output_filter.h"

#define PCR_DATA_LEN (12)
typedef struct {
	uint8_t raw_data[PCR_DATA_LEN];
} te_pcr_data_t;

typedef struct {
	uint64_t sys_time;
	uint64_t stc_time;
	uint64_t pcr_time;
} te_pcr_time_point_t;

struct te_time_points {
	te_pcr_time_point_t left;
	te_pcr_time_point_t left_candidate;
	te_pcr_time_point_t right;
};

int te_convert_arrival_to_systime(struct te_out_filter *output,
				  te_pcr_data_t *raw_pcr_data,
				  unsigned long long *pcr,
				  unsigned long long *sys_time,
				  bool move_window);

int te_obj_init_time(struct te_obj *filter);

#endif

