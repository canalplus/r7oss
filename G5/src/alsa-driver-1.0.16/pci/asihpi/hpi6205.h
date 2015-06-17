/*****************************************************************************

    AudioScience HPI driver
    Copyright (C) 1997-2003  AudioScience Inc. <support@audioscience.com>

    This program is free software; you can redistribute it and/or modify
    it under the terms of version 2 of the GNU General Public License as
    published by the Free Software Foundation;

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

Host Interface module for an ASI6205 based
bus mastering PCI adapter.

Copyright AudioScience, Inc., 2003
******************************************************************************/

#ifndef _HPI6205_H_
#define _HPI6205_H_

#include "hpi.h"

/***********************************************************
	Defines used for basic messaging
************************************************************/
#define H620_HIF_RESET		0
#define H620_HIF_IDLE		1
#define H620_HIF_GET_RESP	2
#define H620_HIF_DATA_DONE	3
#define H620_HIF_DATA_MASK	0x10
#define H620_HIF_SEND_DATA	0x14
#define H620_HIF_GET_DATA	0x15
#define H620_HIF_UNKNOWN		0xffff

/***********************************************************
	Types used for mixer control caching
************************************************************/

#define H620_MAX_ISTREAMS 32
#define H620_MAX_OSTREAMS 32
#define HPI_NMIXER_CONTROLS 2048

/*********************************************************************
	This is used for background buffer bus mastering stream buffers.
**********************************************************************/
struct hostbuffer_status_6205 {
	u32 dwSamplesProcessed;
	u32 dwAuxiliaryDataAvailable;
	u32 dwStreamState;
	/* DSP index in to the host bus master buffer. */
	u32 dwDSPIndex;
	/* Host index in to the host bus master buffer. */
	u32 dwHostIndex;
	u32 dwSizeInBytes;
};

/*********************************************************************
	This is used for dynamic control cache allocation
**********************************************************************/
struct controlcache_6205 {
	u32 dwNumberOfControls;
	u32 dwPhysicalPCI32address;
	u32 dwSpare;
};

/*********************************************************************
	This is used for dynamic allocation of async event array
**********************************************************************/
struct async_event_buffer_6205 {
	u32 dwPhysicalPCI32address;
	u32 dwSpare;
	struct hpi_fifo_buffer b;
};

/***********************************************************
	The Host located memory buffer that the 6205 will bus master
	in and out of.
************************************************************/
#define HPI6205_SIZEOF_DATA (16*1024)
struct bus_master_interface {
	u32 dwHostCmd;
	u32 dwDspAck;
	u32 dwTransferSizeInBytes;
	union {
		struct hpi_message MessageBuffer;
		struct hpi_response ResponseBuffer;
		u8 bData[HPI6205_SIZEOF_DATA];
	} u;
	struct controlcache_6205 aControlCache;
	struct async_event_buffer_6205 aAsyncBuffer;
	struct hostbuffer_status_6205
	 aInStreamHostBufferStatus[H620_MAX_ISTREAMS];
	struct hostbuffer_status_6205
	 aOutStreamHostBufferStatus[H620_MAX_OSTREAMS];
};

#endif
