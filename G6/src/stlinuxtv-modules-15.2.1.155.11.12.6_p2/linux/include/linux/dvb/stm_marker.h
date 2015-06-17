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
with STLinuxTV; see the file COPYING.  If not, write to the Free Software
Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

The STLinuxTV Library may alternatively be licensed under a proprietary
license from ST.

Source file name : stm_marker.h

ST SDK2 marker helper header files.
This file contains definition of functions that are building so called
marker that aims at being inserted within the TS or PES dataflow.

Date        Modification                                    Name
----        ------------                                    --------

 ************************************************************************/

#ifndef STM_MARKER_H_
#define STM_MARKER_H_

#include <linux/types.h>

/* Market type definition */
/* Request for a time alarm (PTS based) from the stream player */
#define STM_MARKER_CMD_TIMING_REQUEST		0x00
/* Forward injection break */
#define STM_MARKER_BREAK_FORWARD		0x01
/* Backward injection break */
#define STM_MARKER_BREAK_BACKWARD		0x02
/* Backward injection break (allowing smooth reverse) */
#define STM_MARKER_BREAK_BACKWARD_SMOOTH_REVERSE	0x03
/* Break because of end of stream. */
#define STM_MARKER_BREAK_END_OF_STREAM		0x04

/* Additionnal tag "OR'ed" to previous markers */
/* Tag indicating the last fram is complete and is to be decoded */
#define STM_MARKER_ORedTAG_LAST_FRAME_COMPLETE	0x80


/* Marker size */
/* Size of basic MPEG2-TS packet. */
#define STM_MARKER_TS_SIZE	188
/* Size of basic MPEG2-PES packet. */
#define STM_MARKER_PES_SIZE	26
/* Maximum user data size. */
#define STM_MARKER_USER_DATA_SIZE	10


/* Data structure definitions. */

/* PES Packet */
struct stm_pes_marker {
	/* Normalized standard value : (00 00 01h) */
	__u8	packet_start_code_prefix[3];

	/* ST's PES marker stream_id (FBh) */
	__u8	stream_id;

	/* ST's PES marker length (00 14h) */
	__u8	PES_packet_length[2];

	/* ST's PES marker 7th byte with: */
	/*	byte7.7 and .6 = '01' (01) */
	/*	byte7.5 and .4 PES_scrambling_control (00) */
	/*	byte7.3 = PES_priority (0) */
	/*	byte7.2 = data_alignment_indicator (0) */
	/*	byte7.1 = copyright (0) */
	/*	byte7.0 = original_or_copy (0) */
	__u8	byte7;

	/* ST's PES marker 8th byte with: */
	/*	byte8.7 and .6 = PTS_DTS_flags (00) */
	/*	byte8.5 = ESCR_flag (0) */
	/*	byte8.4 = ES_rate_flag (0) */
	/*	byte8.3 = DSM_trick_mode_flag (0) */
	/*	byte8.2 = additional_copy_info_flag (0) */
	/*	byte8.1 = PES_CRC_flag (0) */
	/*	byte8.0 = PES_extension_flag (1) */
	__u8	byte8;

	/* ST's PES marker data length (11h) */
	__u8	PES_header_data_length;

	/* ST's PES marker 10th byte with: */
	/*	byte10.7 = PES_private_data_flag (1) */
	/*	byte10.6 = pack_header_field_flag (0) */
	/*	byte10.5 = program_packet_seq_cnt_flag (0) */
	/*	byte10.4 = PSTD_buffer_flag (0) */
	/*	byte10.3 to .1 = reserved (000) */
	/*	byte10.0 = PES_extension_flag_2	(0) */
	__u8	byte10;

	/* ST's PES marker pattern and user data with: */
	/*  PES_private_data[0..3] = marker pattern ("STMM") */
	/*  PES_private_data[4] = marker type (...) */
	/*  PES_private_data[5..14] = user data (...) */
	/*  PES_private_data[15] = user data size (...) */
	__u8	PES_private_data[16];
};


/* Adaptation field */
struct stm_ts_adaptation_field {
	/* ST's marker value:	(0x9D) */
	__u8	adaptation_field_length;

	/* ST's marker adaptation_field second byte with: */
	/*	byte2.7 = discontinuity_indicator (1) */
	/*	byte2.6 = random_access_indicator (0) */
	/*	byte2.5 = elementary__stream_priority_indicator	(0) */
	/*	byte2.4 = PCR_flag (0) */
	/*	byte2.3 = OPCR_flag (0) */
	/*	byte2.2 = splicing_point_flag (0) */
	/*	byte2.1	= transport_private_data_flag (1) */
	/*	byte2.0 = adaptation_field_extension_flag (0) */
	__u8 byte2;

	/* ST's marker private data length (10h) */
	__u8 transport_private_data_length;

	/* ST's marker pattern ("STM FakeTSPacket") */
	__u8 private_data_byte[16];

	/* ST's marker stuffing bytes (0xff)) */
	__u8 Stuffing_byte[139];
};

/* MPEG2-TS Marker packet. */
struct stm_ts_marker {
	/* Normalized standard value : 47h */
	__u8 sync_byte;

	/* User defined pid (msb) of channel to be considered, but with: */
	/*	pid_h.7 = transport_error_indicator */
	/*	pid_h.6 = payload_unit_start_indicator */
	/*	pid_h.5 = transport_priority */
	__u8 pid_h;

	/* User defined pid (msb) of channel to be considered. */
	__u8 pid_l;

	/* Fourth byte of TS marker packet with: */
	/*	ts_byte_4.7 and .6 = adaptation_field_control */
	/*	ts_byte_4.5 and .4 = transport_scrambling_indicator */
	/*	ts_byte_4.3 to .0  = continuity_counter	*/
	__u8	byte4;

	/* MPEG2-System TS adaptation_field */
	struct stm_ts_adaptation_field adaptation_field;

	/* MPEG2-system PES_packet. */
	struct stm_pes_marker PES_packet;
};


/* Local static variables. */

/* Default TS marker content */
static __u8 static_ts_marker[STM_MARKER_TS_SIZE] = {
/*                |<--- adaptation        S     T     M           F     a */
0x47, 0x40, 0x00, 0x30, 0x9D, 0x82, 0x10, 0x53, 0x54, 0x4D, 0x20, 0x46, 0x61,
/* k  e     T     S     P     a     c     k     e     t     <-stuffing bytes */
0x6B, 0x65, 0x54, 0x53, 0x50, 0x61, 0x63, 0x6B, 0x65, 0x74, 0xFF, 0xFF, 0xFF,
/*---------------------------------------------------------------------------*/
0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
/*---------------------------------------------------------------------------*/
0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
/*---------------------------------------------------------------------------*/
0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
/*---------------------------------------------------------------------------*/
0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
/*---------------------------------------------------------------------------*/
0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
/*---------------------------------------------------------------------------*/
0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
/*---------------------------------------------------------------------------*/
0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
/*---------------------------------------------------------------------------*/
0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
/*---------------------------------------------------------------------------*/
0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
/*---------------------------------------------------------------------------*/
0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
/*------------------------------->|<---PES packets --------------------------*/
0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x01, 0xFB, 0x00, 0x14, 0x80,
/*---------------------------------------------------------------------------*/
0x01, 0x11, 0x80, 0x53, 0x54, 0x4D, 0x4D, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
/*-------------------------------->*/
0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};



/*
/// \fn int             int stm_ts_marker_create (
						__u8	marker_type,
						void	*address,
						__u16	size,
						__u16	*marker_size,
						const __u16	pid,
						const __u8	*user_data,
						const __u8	user_data_size);
///
/// \Description This function is creating a ST propriatary MPEG2-TS marker.
///
/// \param marker_type:	Kind of marker to be created.
/// \param address:	Address in memory the marker pattern
//			should be copied into.
/// \param size:	Size in memory available for the pattern copy
//			(min is STM_MARKER_TS_SIZE)
/// \param marker_size:	Pointer to return the size effectively
//			written at address.
/// \param pid:		PID of the TS channel the TS marker should
//			be inserted into.
/// \param user_data:	Pointer to user Application data to be associated
//			to the marker.
/// \param user_data_size:	Size of user Application data
//				(max is STM_MARKER_USER_DATA_SIZE)
///
/// \retval -1      Error when creating the marker
/// \retval 0		Success.
*/
static inline int stm_ts_marker_create(
	__u8	marker_type,
	void	*address,
	__u16	size,
	__u16	*marker_size,
	const __u16	pid,
	const __u8	*user_data,
	const __u8	user_data_size)
{
	/* Locally used variables. */
	struct stm_ts_marker *ts_marker_p =
				(struct stm_ts_marker *)static_ts_marker;

	/* Check input parameters */
	if (address == NULL || marker_size == NULL)
		return -1;
	if (user_data_size && user_data == NULL)
		return -1;
	if ((size < STM_MARKER_TS_SIZE) ||
	    (user_data_size > STM_MARKER_USER_DATA_SIZE))
		return -1;

	/* Set the PID of the TS channel */
	ts_marker_p->pid_h = (ts_marker_p->pid_h & 0xC0) |
				((pid & 0x1f00) >> 8);
	ts_marker_p->pid_l = pid & 0x00ff;

	/* Set-up the marker type. */
	ts_marker_p->PES_packet.PES_private_data[4] = marker_type;

	/* Set-up the size of user data. */
	ts_marker_p->PES_packet.PES_private_data[15] = user_data_size;
	if (user_data_size != 0) {
		/* Copy user data if relevant */
		/* (position 5 in field private_data) */
		memcpy(&ts_marker_p->PES_packet.PES_private_data[5],
		       user_data,
		       user_data_size);
	}

	/* Finally, copy back the TS marker pattern into caller's buffer. */
	memcpy(address, ts_marker_p, STM_MARKER_TS_SIZE);

	/* Update marker effective size. */
	*marker_size = STM_MARKER_TS_SIZE;

	return 0;
}


/*
/// \fn int             int stm_pes_marker_create (
						__u8	marker_type,
						void	*address,
						__u16	size,
						__u16	*marker_size,
						const __u8	*user_data,
						const __u8	user_data_size);
///
/// \Description This function is creating a ST propriatary MPEG2-PES marker.
///
/// \param marker_type:	Kind of marker to be created.
/// \param address:	Address in memory the marker pattern
//			should be copied into.
/// \param size:	Size in memory available for the pattern copy
//			(min is STM_MARKER_PES_SIZE)
/// \param marker_size:	Pointer to return the size effectively written
//			at address.
/// \param user_data:	Pointer to user Application data to be associated
//			to the marker.
/// \param user_data_size:	Size of user Application data
//				(max is STM_MARKER_USER_DATA_SIZE)
///
/// \retval -1      Error when creating the marker
/// \retval 0		Success.
*/
static inline int stm_pes_marker_create(
	__u8	marker_type,
	void	*address,
	__u16	size,
	__u16	*marker_size,
	const __u8	*user_data,
	const __u8	user_data_size)
{
	/* Catch-up directly the position of the PES packet
	 * within the static ts marker. */
	struct stm_ts_marker * ts_marker = (struct stm_ts_marker *)static_ts_marker;
	struct stm_pes_marker *pes_marker_p = &ts_marker->PES_packet;

	/* Check input parameters */
	if (address == NULL || marker_size == NULL)
		return -1;
	if (user_data_size && user_data == NULL)
		return -1;
	if ((size < STM_MARKER_PES_SIZE) ||
	    (user_data_size > STM_MARKER_USER_DATA_SIZE))
		return -1;

	/* Set the marker type. */
	pes_marker_p->PES_private_data[4] = marker_type;
	/* Set-up the size of user data. */
	pes_marker_p->PES_private_data[15] = user_data_size;
	if (user_data_size != 0)
		/* Copy user data if relevant
		 * (position 5 in field private_data) */
		memcpy(&pes_marker_p->PES_private_data[5],
			user_data,
			user_data_size);

	/* Finally, copy back the TS marker pattern into caller's buffer. */
	memcpy(address, pes_marker_p, STM_MARKER_PES_SIZE);

	/* Update marker effective size. */
	*marker_size = STM_MARKER_PES_SIZE;

	return 0;

}

#endif
