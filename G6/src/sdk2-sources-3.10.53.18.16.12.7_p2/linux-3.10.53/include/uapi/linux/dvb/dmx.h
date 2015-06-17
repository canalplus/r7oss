/*
 * dmx.h
 *
 * Copyright (C) 2000 Marcus Metzler <marcus@convergence.de>
 *                  & Ralph  Metzler <ralph@convergence.de>
 *                    for convergence integrated media GmbH
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 */

#ifndef _UAPI_DVBDMX_H_
#define _UAPI_DVBDMX_H_

#include <linux/types.h>
#ifndef __KERNEL__
#include <time.h>
#endif


#define DMX_FILTER_SIZE 16

typedef enum
{
	DMX_OUT_DECODER, /* Streaming directly to decoder. */
	DMX_OUT_TAP,     /* Output going to a memory buffer */
			 /* (to be retrieved via the read command).*/
	DMX_OUT_TS_TAP,  /* Output multiplexed into a new TS  */
			 /* (to be retrieved by reading from the */
			 /* logical DVR device).                 */
	DMX_OUT_TSDEMUX_TAP /* Like TS_TAP but retrieved from the DMX device */
} dmx_output_t;


typedef enum
{
	DMX_IN_FRONTEND, /* Input from a front-end device.  */
	DMX_IN_DVR       /* Input from the logical DVR device.  */
} dmx_input_t;


typedef enum dmx_ts_pes
{
	DMX_PES_AUDIO0,
	DMX_PES_VIDEO0,
	DMX_PES_TELETEXT0,
	DMX_PES_SUBTITLE0,
	DMX_PES_PCR0,

	DMX_PES_AUDIO1,
	DMX_PES_VIDEO1,
	DMX_PES_TELETEXT1,
	DMX_PES_SUBTITLE1,
	DMX_PES_PCR1,

	DMX_PES_AUDIO2,
	DMX_PES_VIDEO2,
	DMX_PES_TELETEXT2,
	DMX_PES_SUBTITLE2,
	DMX_PES_PCR2,

	DMX_PES_AUDIO3,
	DMX_PES_VIDEO3,
	DMX_PES_TELETEXT3,
	DMX_PES_SUBTITLE3,
	DMX_PES_PCR3,

	DMX_PES_OTHER,

	DMX_PES_AUDIO4,
	DMX_PES_VIDEO4,
	DMX_PES_TELETEXT4,
	DMX_PES_SUBTITLE4,
	DMX_PES_PCR4,

	DMX_PES_AUDIO5,
	DMX_PES_VIDEO5,
	DMX_PES_TELETEXT5,
	DMX_PES_SUBTITLE5,
	DMX_PES_PCR5,

	DMX_PES_AUDIO6,
	DMX_PES_VIDEO6,
	DMX_PES_TELETEXT6,
	DMX_PES_SUBTITLE6,
	DMX_PES_PCR6,

	DMX_PES_AUDIO7,
	DMX_PES_VIDEO7,
	DMX_PES_TELETEXT7,
	DMX_PES_SUBTITLE7,
	DMX_PES_PCR7,

	DMX_PES_AUDIO8,
	DMX_PES_VIDEO8,
	DMX_PES_TELETEXT8,
	DMX_PES_SUBTITLE8,
	DMX_PES_PCR8,

	DMX_PES_AUDIO9,
	DMX_PES_VIDEO9,
	DMX_PES_TELETEXT9,
	DMX_PES_SUBTITLE9,
	DMX_PES_PCR9,

	DMX_PES_AUDIO10,
	DMX_PES_VIDEO10,
	DMX_PES_TELETEXT10,
	DMX_PES_SUBTITLE10,
	DMX_PES_PCR10,

	DMX_PES_AUDIO11,
	DMX_PES_VIDEO11,
	DMX_PES_TELETEXT11,
	DMX_PES_SUBTITLE11,
	DMX_PES_PCR11,

	DMX_PES_AUDIO12,
	DMX_PES_VIDEO12,
	DMX_PES_TELETEXT12,
	DMX_PES_SUBTITLE12,
	DMX_PES_PCR12,

	DMX_PES_AUDIO13,
	DMX_PES_VIDEO13,
	DMX_PES_TELETEXT13,
	DMX_PES_SUBTITLE13,
	DMX_PES_PCR13,

	DMX_PES_AUDIO14,
	DMX_PES_VIDEO14,
	DMX_PES_TELETEXT14,
	DMX_PES_SUBTITLE14,
	DMX_PES_PCR14,

	DMX_PES_AUDIO15,
	DMX_PES_VIDEO15,
	DMX_PES_TELETEXT15,
	DMX_PES_SUBTITLE15,
	DMX_PES_PCR15,

	DMX_PES_AUDIO16,
	DMX_PES_VIDEO16,
	DMX_PES_TELETEXT16,
	DMX_PES_SUBTITLE16,
	DMX_PES_PCR16,

	DMX_PES_AUDIO17,
	DMX_PES_VIDEO17,
	DMX_PES_TELETEXT17,
	DMX_PES_SUBTITLE17,
	DMX_PES_PCR17,

	DMX_PES_AUDIO18,
	DMX_PES_VIDEO18,
	DMX_PES_TELETEXT18,
	DMX_PES_SUBTITLE18,
	DMX_PES_PCR18,

	DMX_PES_AUDIO19,
	DMX_PES_VIDEO19,
	DMX_PES_TELETEXT19,
	DMX_PES_SUBTITLE19,
	DMX_PES_PCR19,

	DMX_PES_AUDIO20,
	DMX_PES_VIDEO20,
	DMX_PES_TELETEXT20,
	DMX_PES_SUBTITLE20,
	DMX_PES_PCR20,

	DMX_PES_AUDIO21,
	DMX_PES_VIDEO21,
	DMX_PES_TELETEXT21,
	DMX_PES_SUBTITLE21,
	DMX_PES_PCR21,

	DMX_PES_AUDIO22,
	DMX_PES_VIDEO22,
	DMX_PES_TELETEXT22,
	DMX_PES_SUBTITLE22,
	DMX_PES_PCR22,

	DMX_PES_AUDIO23,
	DMX_PES_VIDEO23,
	DMX_PES_TELETEXT23,
	DMX_PES_SUBTITLE23,
	DMX_PES_PCR23,

	DMX_PES_AUDIO24,
	DMX_PES_VIDEO24,
	DMX_PES_TELETEXT24,
	DMX_PES_SUBTITLE24,
	DMX_PES_PCR24,

	DMX_PES_AUDIO25,
	DMX_PES_VIDEO25,
	DMX_PES_TELETEXT25,
	DMX_PES_SUBTITLE25,
	DMX_PES_PCR25,

	DMX_PES_AUDIO26,
	DMX_PES_VIDEO26,
	DMX_PES_TELETEXT26,
	DMX_PES_SUBTITLE26,
	DMX_PES_PCR26,

	DMX_PES_AUDIO27,
	DMX_PES_VIDEO27,
	DMX_PES_TELETEXT27,
	DMX_PES_SUBTITLE27,
	DMX_PES_PCR27,

	DMX_PES_AUDIO28,
	DMX_PES_VIDEO28,
	DMX_PES_TELETEXT28,
	DMX_PES_SUBTITLE28,
	DMX_PES_PCR28,

	DMX_PES_AUDIO29,
	DMX_PES_VIDEO29,
	DMX_PES_TELETEXT29,
	DMX_PES_SUBTITLE29,
	DMX_PES_PCR29,

	DMX_PES_AUDIO30,
	DMX_PES_VIDEO30,
	DMX_PES_TELETEXT30,
	DMX_PES_SUBTITLE30,
	DMX_PES_PCR30,

	DMX_PES_AUDIO31,
	DMX_PES_VIDEO31,
	DMX_PES_TELETEXT31,
	DMX_PES_SUBTITLE31,
	DMX_PES_PCR31,

	DMX_PES_LAST
} dmx_pes_type_t;

#define DMX_PES_AUDIO    DMX_PES_AUDIO0
#define DMX_PES_VIDEO    DMX_PES_VIDEO0
#define DMX_PES_TELETEXT DMX_PES_TELETEXT0
#define DMX_PES_SUBTITLE DMX_PES_SUBTITLE0
#define DMX_PES_PCR      DMX_PES_PCR0


typedef struct dmx_filter
{
	__u8  filter[DMX_FILTER_SIZE];
	__u8  mask[DMX_FILTER_SIZE];
	__u8  mode[DMX_FILTER_SIZE];
} dmx_filter_t;


struct dmx_sct_filter_params
{
	__u16          pid;
	dmx_filter_t   filter;
	__u32          timeout;
	__u32          flags;
#define DMX_CHECK_CRC       1
#define DMX_ONESHOT         2
#define DMX_IMMEDIATE_START 4
#define DMX_KERNEL_CLIENT   0x8000
};


struct dmx_pes_filter_params
{
	__u16          pid;
	dmx_input_t    input;
	dmx_output_t   output;
	dmx_pes_type_t pes_type;
	__u32          flags;
};

typedef struct dmx_caps {
	__u32 caps;
	int num_decoders;
} dmx_caps_t;

typedef enum {
	DMX_SOURCE_FRONT0 = 0,
	DMX_SOURCE_FRONT1,
	DMX_SOURCE_FRONT2,
	DMX_SOURCE_FRONT3,
	DMX_SOURCE_FRONT4,
	DMX_SOURCE_FRONT5,
	DMX_SOURCE_FRONT6,
	DMX_SOURCE_FRONT7,
	DMX_SOURCE_FRONT8,
	DMX_SOURCE_FRONT9,
	DMX_SOURCE_FRONT10,
	DMX_SOURCE_FRONT11,
	DMX_SOURCE_FRONT12,
	DMX_SOURCE_FRONT13,
	DMX_SOURCE_FRONT14,
	DMX_SOURCE_FRONT15,
	DMX_SOURCE_DVR0   = 16,
	DMX_SOURCE_DVR1,
	DMX_SOURCE_DVR2,
	DMX_SOURCE_DVR3,
	DMX_SOURCE_DVR4,
	DMX_SOURCE_DVR5,
	DMX_SOURCE_DVR6,
	DMX_SOURCE_DVR7,
	DMX_SOURCE_DVR8,
	DMX_SOURCE_DVR9,
	DMX_SOURCE_DVR10,
	DMX_SOURCE_DVR11,
	DMX_SOURCE_DVR12,
	DMX_SOURCE_DVR13,
	DMX_SOURCE_DVR14,
	DMX_SOURCE_DVR15
} dmx_source_t;

struct dmx_stc {
	unsigned int num;	/* input : which STC? 0..N */
	unsigned int base;	/* output: divisor for stc to get 90 kHz clock */
	__u64 stc;		/* output: stc in 'base'*90 kHz units */
};


#define DMX_START                _IO('o', 41)
#define DMX_STOP                 _IO('o', 42)
#define DMX_SET_FILTER           _IOW('o', 43, struct dmx_sct_filter_params)
#define DMX_SET_PES_FILTER       _IOW('o', 44, struct dmx_pes_filter_params)
#define DMX_SET_BUFFER_SIZE      _IO('o', 45)
#define DMX_GET_PES_PIDS         _IOR('o', 47, __u16[5])
#define DMX_GET_CAPS             _IOR('o', 48, dmx_caps_t)
#define DMX_SET_SOURCE           _IOW('o', 49, dmx_source_t)
#define DMX_GET_STC              _IOWR('o', 50, struct dmx_stc)
#define DMX_ADD_PID              _IOW('o', 51, __u16)
#define DMX_REMOVE_PID           _IOW('o', 52, __u16)

#endif /* _UAPI_DVBDMX_H_ */
