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

Source file name : te_constants.h

Defines various hard constants used throughout the stm_te library
******************************************************************************/

#ifndef __TE_CONSTANTS_H
#define __TE_CONSTANTS_H

/* TE constants for determining capabilities based on reported HAL
 * capabilities */

/* The available pDevice slots are divided equally between vDevices. However we
 * can normally overcommit the slots since slots can be dynamically shared
 * between vDevices. This works so long as all vDevices don't require their
 * maximum number of slots (which in practice is almost always).
 *
 * This macro converts the number of slots available on a pDevice to the number
 * that should be allocated to each vDevice.
 *
 * In this case we have overcommited by 50% (3/2). Care must be taken to avoid
 * introducing runtime floating-point arithmetic via this macro (only integer
 * multiplication/division is permitted in the kernel)
 */
#define TE_SLOTS_PER_VDEVICE(pdevice_slots, pdevice_vdevice) \
	(((pdevice_slots / pdevice_vdevice) * 3) / 2)

/* TSMUX limits */
#define TE_MAX_TSMUXES (4)
#define TE_MAX_SECTION_FILTER_LENGTH (16+2)	/* +2 for length field */
#define TE_MAX_SECTION_POS_NEG_LENGTH (8+2)	/* +2 for length field */
#define TE_MAX_SECTION_FILTERS (256)
#define TE_MAX_TSMUXES (4)
#define TE_MAX_TSG_PER_TSMUX (7)
#define TE_MAX_TSG_FILTERS (TE_MAX_TSMUXES * TE_MAX_TSG_PER_TSMUX)
#define TE_MAX_TSG_INDEX_PER_TSG_FILTER (1)

/* System Maximums */
#define TE_MAX_SECTION_SIZE (4096)
#define TE_MAX_INJECTOR_NODES (96)

/* TE Constants */
#define TE_DLNA_PACKET_SIZE (192)
#define TE_TAGGED_PACKET_SIZE (196)
#define TE_NULL_STREAM_ID (0xFFFF)
#define TE_PES_STREAM_ID_ALL (0)
#define TE_ANALYSIS_PKTS (3)
#define TE_DEFAULT_DEVICE_INDEX (0)

/* DVB Constants */
#define DVB_SYNC_BYTE (0x47)
#define DVB_PID_MAX_VALUE (0x1fff)
#define DVB_HEADER_SIZE (4)
#define DVB_PAYLOAD_SIZE (184)
#define DVB_PACKET_SIZE (DVB_HEADER_SIZE + DVB_PAYLOAD_SIZE)

/* Defaults that can be overridden by controls */
#define TE_DEFAULT_PID_FILTER_FLUSHING STM_TE_FILTER_CONTROL_FLUSH_ON_PID_CHANGE
#define TE_DEFAULT_OUTPUT_FILTER_FLUSHING STM_TE_FILTER_CONTROL_FLUSH_ON_DETACH
#define TE_DEFAULT_OUTPUT_FILTER_OVERFLOW STM_TE_FILTER_CONTROL_OVERFLOW_DISCARDS_NEW_DATA
#define TE_DEFAULT_SECTION_FILTER_LEN (2)
#define TE_DEFAULT_SECTION_FILTER_REPEAT (true)
#define TE_DEFAULT_DISCARD_ON_CRC_ERROR (true)
#define TE_DEFAULT_SECTION_FILTER_FORCE_CRC_CHECK (false)
#define TE_DEFAULT_SECTION_FILTER_VERSION_NOT_MATCH (false)
#define TE_DEFAULT_SECTION_FILTER_POS_NEG_MODE (false)

#endif
