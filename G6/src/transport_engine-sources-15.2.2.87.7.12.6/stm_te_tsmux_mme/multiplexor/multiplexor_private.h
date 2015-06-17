/************************************************************************
Copyright (C) 2012 STMicroelectronics. All Rights Reserved.

This is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License version 2 as published by the
Free Software Foundation.

This is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this; see the file COPYING.  If not, write to the Free Software
Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

This may alternatively be licensed under a proprietary
license from ST.

Source file name : multilexor_private.h
Architects :	   Dan Thomson & Pete Steiglitz

Definition of the multiplexorinternal header file.


Date	Modification				Architects
----	------------				----------
22-May-12   Created				 DT & PS

************************************************************************/

#ifndef H_MULTIPLEXOR_PRIVATE
#define H_MULTIPLEXOR_PRIVATE

#include <stm_te_dbg.h>
#include "multiplexor.h"

/* Useful DVB constants */

#define DVB_HEADER_SIZE				4
#define DVB_PACKET_SIZE				188
#define DVB_MAX_PAYLOAD_SIZE			184
#define DVB_PCR_AF_SIZE				6

#define DVB_SYNC_BYTE				0x47000000

#define DVB_PACKET_ERROR_MASK			0x00800000
#define DVB_PAYLOAD_UNIT_START_MASK		0x00400000
#define DVB_PRIORITY_MASK			0x00200000
#define DVB_PID_MASK				0x001fff00
#define DVB_SCRAMBLED_MASK			0x00000080
#define DVB_SCRAMBLING_POLARITY_MASK		0x00000040
#define DVB_ADAPTATION_FIELD_MASK		0x00000020
#define DVB_PAYLOAD_PRESENT_MASK		0x00000010
#define DVB_CONTINUITY_COUNT_MASK		0x0000000f

#define DVB_PID_SHIFT				8

#define DVB_DISCONTINUITY_INDICATOR_MASK	0x80
#define DVB_RAP_INDICATOR_MASK			0x40
#define DVB_PCR_INDICATOR_MASK			0x10

#define DvbPacketHeader(Pusi, Pid, Scrambled, Polarity, Adaptation, Payload, ContinuityCount)	\
		DVB_SYNC_BYTE						|\
		(Pusi ? DVB_PAYLOAD_UNIT_START_MASK : 0)		|\
		(Pid << DVB_PID_SHIFT)					|\
		(Scrambled ? DVB_SCRAMBLED_MASK : 0)			|\
		(Polarity ? DVB_SCRAMBLING_POLARITY_MASK : 0)		|\
		(Adaptation ? DVB_ADAPTATION_FIELD_MASK : 0)		|\
		(Payload ? DVB_PAYLOAD_PRESENT_MASK : 0)		|\
		(ContinuityCount);


/*

 Definition of the bitfield access macros in an output packet descriptor

	Bits	0..1
			00 =>	Data packet
				Bits 2..3	Program
				Bits 4..7	Stream

			01 =>	PCR

			10 =>	Table
				Bits 4..7
					0000 => PAT
					0001 => PMT with bits 2..3 Program
					0010 => SDT

			11 =>	Padding


*/

#define OUTPUT_DATA_PACKET		0x00
#define OUTPUT_PCR_PACKET		0x01
#define OUTPUT_TABLE_PACKET		0x02
#define OUTPUT_NULL_PACKET		0x03

#define TABLE_ID_PAT			0x00
#define TABLE_ID_PMT			0x01
#define TABLE_ID_SDT			0x02
#define TABLE_ID_DIT			0x03

#define ExtractPacketType(b)		((b) & 0x03)
#define ExtractProgram(b)		(((b) >> 2) & 0x03)
#define ExtractStream(b)		(((b) >> 4) & 0x0f)
#define ExtractTableId(b)		(((b) >> 4) & 0x0f)
#define ExtractTransitionFlag(b)	(((b) >> 2) & 0x01)

#define CodeForDataPacket(p, s)	(OUTPUT_DATA_PACKET | ((p) << 2) | ((s) << 4))
#define CodeForPcrOnStream(p, s) (OUTPUT_PCR_PACKET | ((p) << 2) | ((s) << 4))
#define CodeForPcr()		CodeForPcrOnStream(3, 15)
#define CodeForPat()		(OUTPUT_TABLE_PACKET | (TABLE_ID_PAT << 4))
#define CodeForPmt(p)		(OUTPUT_TABLE_PACKET | (TABLE_ID_PMT << 4) | \
								((p) << 2))
#define CodeForSdt()		(OUTPUT_TABLE_PACKET | (TABLE_ID_SDT << 4))
#define CodeForDit(f)		(OUTPUT_TABLE_PACKET | (TABLE_ID_DIT << 4) | \
								((f) << 2))
#define CodeForNullPacket()	(OUTPUT_NULL_PACKET)

#define OutputCode(c) \
	do { \
		Ctx->OutputPacketDescriptors[Ctx->OutputPacketCount++] = c; \
	} while (0)


/*
 macro to wrap up the anding with 1ffffffff for a PCR/PTS/DTS value
*/
#define MUX_DEFAULT_UNSPECIFIED	0xffffffffU
#define INVALID_TIME		0xffffffffffffffffull
#define MAX_DTS			0x1ffffffffull
#define InTimePeriod(V, S, E)	((E < S) ? ((V >= S) || (V < E)) \
					: ((V >= S) && (V < E)))
#define	PcrLimit(V)		((V) & MAX_DTS)

#define TimeIsBefore(V, R) (((V < R) && ((R - V) < (MAX_DTS - R + V))) || \
				((V > R) && ((V - R) > (MAX_DTS - V + R))))

/*
 macro to give a desired bit buffer level from the actual bit buffer size
 see "A Versattile Multiplexing Algorithm Exclusively Based on the
 MPEG-2 Systems Layer"
*/

#define	ManifestorDesiredBitBufferLevel(BBS)	((BBS * 75) / 100)

/*

 macro to wrap a parameter check

*/
#undef inrange
#define inrange(val, lower, upper)    (((val) >= (lower)) && ((val) <= (upper)))

#define ParameterAssert(B, S) \
	do { \
		if (!(B)) { \
			pr_err("%s\n", S); \
			return MultiplexorInvalidParameter; \
		} \
	} while (0)
#define ParameterRangeCheck(V, L, U, S)	ParameterAssert(inrange(V, L, U), S)

/*

 Function declarations

*/

MultiplexorStatus_t MultiplexorAppendCrc(unsigned char *Data,
	unsigned int *DataLength);

MultiplexorStatus_t MultiplexorGeneratePat(MultiplexorContext_t Context);

MultiplexorStatus_t MultiplexorGeneratePmt(MultiplexorContext_t Context,
	unsigned int ProgramId);

MultiplexorStatus_t MultiplexorGenerateSdt(MultiplexorContext_t Context);

MultiplexorStatus_t MultiplexorGenerateDit(MultiplexorContext_t Context);

MultiplexorStatus_t MultiplexorGenerateInitialPcr(MultiplexorContext_t Context);

MultiplexorStatus_t MultiplexorOutputOnePacket(MultiplexorContext_t Context,
	unsigned int PacketNumber, unsigned char Code);

MultiplexorStatus_t MultiplexorOutputByte(MultiplexorContext_t Context,
	unsigned char Value);

MultiplexorStatus_t MultiplexorOutputWord(MultiplexorContext_t Context,
	unsigned int Value);

MultiplexorStatus_t MultiplexorOutputData(MultiplexorContext_t Context,
	unsigned char *Data, unsigned int Size);

MultiplexorStatus_t MultiplexorCalculatePcrPeriodPackets(
	MultiplexorContext_t Context, MultiplexorStreamParameters_t *Stream,
	unsigned int *Packets);

MultiplexorStatus_t MultiplexorCaclculateBitrate(MultiplexorContext_t Context);

MultiplexorStatus_t MultiplexorPerformMultiplex(MultiplexorContext_t Context);

#endif
