/************************************************************************
Copyright (C) 2011 STMicroelectronics. All Rights Reserved.

This file is part of the STLinuxTV Library.

STLinuxTV is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License version 2 as published by the
Free Software Foundation.

STLinuxTV is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with STLinuxTV; see the file COPYING.  If not, write to the Free Software
Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

The STLinuxTV Library may alternatively be licensed under a proprietary
license from ST.

Source file name : stm-dmx.h
Author :           Nick <nick.haydock@st.com>

Header for dmx ST proprietary ioctls

Date        Modification                                    Name
----        ------------                                    --------

 ************************************************************************/

#ifndef _STMDVBDMX_H_
#define _STMDVBDMX_H_

struct dmx_pcr {
	unsigned long long pcr;	/* Returned in pts format */
	unsigned long long system_time;
};

typedef enum {
        DMX_INDEX_NONE                  = 0x0000, /* No event */
        DMX_INDEX_PUSI                  = 0x0001, /* Payload Unit Start Indicator Flag */
        DMX_INDEX_SCRAM_TO_CLEAR        = 0x0002, /* Scrambling Change To Clear Flag */
        DMX_INDEX_TO_EVEN_SCRAM         = 0x0004, /* Scrambling Change To Even Flag */
        DMX_INDEX_TO_ODD_SCRAM          = 0x0008, /* Scrambling Change To Odd Flag */
        DMX_INDEX_DISCONTINUITY         = 0x0010, /* Discontinuity Flag */
        DMX_INDEX_RANDOM_ACCESS         = 0x0020, /* Random Access Flag */
        DMX_INDEX_ES_PRIORITY		= 0x0040, /* ES Priority Flag */
        DMX_INDEX_PCR                   = 0x0080, /* PCR Flag*/
        DMX_INDEX_OPCR                  = 0x0100, /* OPCR Flag */
        DMX_INDEX_SPLICING_POINT        = 0x0200, /* Splicing Point Flag */
        DMX_INDEX_TS_PRIVATE_DATA       = 0x0400, /* Transport Private Data Flag */
        DMX_INDEX_ADAPTATION_EXT        = 0x0800, /* Adaptation Field Extension Flag */
        DMX_INDEX_FIRST_REC_PACKET      = 0x1000, /* First Recorded Packet */
        DMX_INDEX_START_CODE            = 0x2000, /* Index Start Code Flag */
        DMX_INDEX_PTS                   = 0x4000  /* PTS Flag */
} dmx_index_event;

struct dmx_index {
        __u16 pid;
        dmx_index_event event;
        unsigned int packet_count;
        unsigned long long pcr;
        unsigned long long system_time;
        unsigned char mpeg_start_code;
        unsigned char mpeg_start_code_offset;
        __u16 extra_bytes;
        unsigned char extra[5];
};

struct dmx_index_pid {
        __u16 pid;
       __u8 number_of_start_codes;
       __u8 *start_codes;
       dmx_index_event flags;
};

struct dmx_index_filter_params {
        unsigned int n_pids;
        struct dmx_index_pid *pids;
};

struct dmx_insert_filter_params {
	unsigned int pid;
	unsigned int freq_ms;
	unsigned char *data;
	unsigned int data_size;
};

struct dmx_replace_filter_params {
	unsigned int pid;
	unsigned char *data;
	unsigned int data_size;
};

typedef enum {
        DMX_TS_AUTO			= 0x0000, /* Auto-detected by demux */
        DMX_TS_TYPE_DVB			= 0x0001, /* DVB type, 188 bytes MPEG TS packet */
        DMX_TS_TYPE_TTS			= 0x0002  /* 192 bytes TTS (time stamped transport stream) */
} dmx_ts_format_t;

typedef enum {
	DMX_SCRAMBLED,	/* Non descrambled data generated */
	DMX_DESCRAMBLED	/* Descrambled data generated */
} dmx_scrambling_t;

/*
 * dmx ctrl structure is to mangage TE controls which cannot be
 * intrinsically decided upon filter creation or any other activity
 * @id        : ctrl id
 * @size      : normally 0 or don't care unless ctrl requires @data
 * @value     : integer value for control
 * @start_code: start codes to be given for GOP detection
 * @data      : compound control data(may be array of int, .. or a struct)
 * @output    : dmx_output_t, dmx_output_sti
 * @pes_type  : dmx_pes_type_t (audio/video/pcr)
 */
#define MAX_START_CODE		6

struct dmx_ctrl {
	__u32 id;
	__u32 size;
	union {
		__u32 value;
		__u8 start_code[MAX_START_CODE];
		void *data;
	};
	__u32 output;
	dmx_pes_type_t pes_type;
};

/*
 * TE controls exposed using the 'struct dmx_ctrl.id'
 */

#define DMX_CTRL_OUTPUT_BUFFER_STATUS              0x1

/*
 * The above control will expose the buffer level of output buffer
 * to use this control, user will fill below mentioned value
 * control id = DMX_CTRL_OUTPUT_BUFFER_STATUS
 * pes_type = DMX_PES_AUDIO/DMX_PES_VIDEO/DMX_PES_PCR
 * the value field will return bytes in buffer
 */


#define DMX_FLUSH_CHANNEL		_IO('o',   160)
#define DMX_SET_INDEX_FILTER		_IOW('o',  162, struct dmx_index_filter_params)
#define DMX_SET_INS_FILTER		_IOW('o',  163, struct dmx_insert_filter_params)
#define DMX_SET_REP_FILTER		_IOW('o',  164, struct dmx_replace_filter_params)
#define DMX_SET_TS_FORMAT		_IOW('o',   165, dmx_ts_format_t)
#define DMX_SET_SCRAMBLING		_IOW('o',   166, dmx_scrambling_t)
#define DMX_GET_CTRL			_IOW('o',   167, struct dmx_ctrl)

/* When we disconnect one of the PES filters associated with the TS read, then the
 * default flushing behavior can be overridden by the flag DMX_TS_NO_FLUSH_ON_DETACH.
 * This will not flush demux filters when one of the PID filters is detached. This
 * flag is only applicable for TS buffer.
 */
#define DMX_TS_NO_FLUSH_ON_DETACH 0x8

#endif
