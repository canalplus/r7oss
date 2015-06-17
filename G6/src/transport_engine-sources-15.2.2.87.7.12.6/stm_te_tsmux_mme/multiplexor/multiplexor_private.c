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

Source file name : multilexor.c
Architects :	   Dan Thomson & Pete Steiglitz

High level functionality of the multiplexor


Date	Modification				Architects
----	------------				----------
17-Apr-12   Created			      DT & PS

************************************************************************/
#include "stm_te.h"
#include <stm_te_dbg.h>

#include "multiplexor_private.h"

/* Static data for a null packet */
static unsigned char	NullPacket[SIZE_OF_TRANSPORT_PACKET] = {
	0x47, 0x1f, 0xff, 0x00,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

/* PRIVATE - Function to append a CRC to an existing data block */
MultiplexorStatus_t MultiplexorAppendCrc(unsigned char *Data,
	unsigned int *DataLength)
{
	unsigned int i;
	unsigned int Crc;

	/* Calculate the CRC of the existing block */
	Crc = 0xFFFFFFFF;

	for (i = 0; i < (*DataLength); i++) {
		Crc ^= ((unsigned int) Data[i]) << 24;

		Crc = (Crc & 0x80000000) ? ((Crc << 1) ^ 0x04C11DB7) :
				(Crc << 1);
		Crc = (Crc & 0x80000000) ? ((Crc << 1) ^ 0x04C11DB7) :
				(Crc << 1);
		Crc = (Crc & 0x80000000) ? ((Crc << 1) ^ 0x04C11DB7) :
				(Crc << 1);
		Crc = (Crc & 0x80000000) ? ((Crc << 1) ^ 0x04C11DB7) :
				(Crc << 1);
		Crc = (Crc & 0x80000000) ? ((Crc << 1) ^ 0x04C11DB7) :
				(Crc << 1);
		Crc = (Crc & 0x80000000) ? ((Crc << 1) ^ 0x04C11DB7) :
				(Crc << 1);
		Crc = (Crc & 0x80000000) ? ((Crc << 1) ^ 0x04C11DB7) :
				(Crc << 1);
		Crc = (Crc & 0x80000000) ? ((Crc << 1) ^ 0x04C11DB7) :
				(Crc << 1);
	}

	/* Now append to the data block */
	Data[i++] = (Crc >> 24) & 0xff;
	Data[i++] = (Crc >> 16) & 0xff;
	Data[i++] = (Crc >> 8) & 0xff;
	Data[i++] = Crc & 0xff;

	*DataLength = i;

	return MultiplexorNoError;
}

/* PRIVATE - Function to generate the pat table */
MultiplexorStatus_t MultiplexorGeneratePat(MultiplexorContext_t Context)
{
	unsigned int i;
	unsigned char TableVersion;
	unsigned char *Pat;
	unsigned char *SectionStart;
	unsigned char *LengthField;
	unsigned int SectionLength;

	/* Generate the header */
	Pat = Context->PatPacket;

	*(Pat++) = 0x47; /* Sync byte */
	*(Pat++) = 0x40; /* Pusi */
	*(Pat++) = 0x00; /* Pid == 0 */
	*(Pat) |= 0x10; /* Not scrambled, */
	/* Payload only, */
	/* CC not changed from current/initial value */

	if (Context->Settings.GlobalFlags
			& TABLE_DISCONTINUITY_FLAG) {
		/* Adaptation field exist */
		*(Pat++) |= DVB_ADAPTATION_FIELD_MASK;
		/* Adaptation field size */
		*(Pat++) = 1;
		*(Pat++) |=
			DVB_DISCONTINUITY_INDICATOR_MASK;
	} else
		*(Pat++) &= (~DVB_ADAPTATION_FIELD_MASK);

	*(Pat++) = 0x00; /* Section pointer value */

	/* Output the section */
	SectionStart = Pat;

	*(Pat++) = 0x00; /* Table ID */
	*(Pat++) = 0xB0; /* section syntax on */
	/* reserved fields */
	/* first 4 bits of section length */
	LengthField = Pat++; /* Leave length for calculation later */
	/* Transport stream id */
	*(Pat++) = (Context->Settings.TransportStreamId >> 8) & 0xff;
	*(Pat++) = Context->Settings.TransportStreamId & 0xff;

	TableVersion = (*Pat + 2) & 0x3e; /* Increment table version */
	*(Pat++) = 0xc1 | TableVersion; /* Reserved fields */
	/* Table version */
	/* Current indicator */

	*(Pat++) = 0x00; /* Section Number */

	*(Pat++) = 0x00; /* Last section number */

	for (i = 0; i < MAXIMUM_NUMBER_OF_PROGRAMS; i++)
		if (Context->Programs[i].EntryUsed
				&& !Context->Programs[i].TablesProgram) {
			/* Program Number */
			*(Pat++) = (Context->Programs[i].ProgramNumber >> 8)
				& 0xff;
			*(Pat++) = Context->Programs[i].ProgramNumber & 0xff;
			/* Reserved + 5 bits of PMT pid */
			*(Pat++) = 0xe0 | ((Context->Programs[i].PMTPid >> 8)
				& 0x1f);
			/* bottom 8 bits of PMT pid */
			*(Pat++) = Context->Programs[i].PMTPid & 0xff;
		}

	/* Append the CRC - calculated with length already set */
	/* accumulated length + 4 gives us the CRC field */
	*LengthField = Pat - (LengthField + 1) + 4;

	SectionLength = Pat - SectionStart;
	MultiplexorAppendCrc(SectionStart, &SectionLength);

	Pat += 4;

	/* Stuff out to packet length */
	memset(Pat, 0xff, &Context->PatPacket[SIZE_OF_TRANSPORT_PACKET] - Pat);

	return MultiplexorNoError;
}


/* PRIVATE - Function to generate a pmt table */
/* */
/*	NOTE2 -	We have upto N streams per program, and the descriptors */
/*		are upto M chars in length which means the PMT is */
/*		which means the PMT is 16 + M + N * (5 + M) max */
/*		size. This must fit into 1 */
/*		transport packet. Do not make it any larger */
/*		without adding checking, and removing the fatal */
/*		error check */
MultiplexorStatus_t MultiplexorGeneratePmt(MultiplexorContext_t Context,
	unsigned int ProgramId)
{
	unsigned int i;
	MultiplexorProgramParameters_t *Program;
	MultiplexorStreamParameters_t *Stream;
	unsigned char TableVersion;
	unsigned char *Pmt;
	unsigned char *SectionStart;
	unsigned char *LengthField;
	unsigned int SectionLength;
	unsigned int PcrPid;

	/* Check that the pmt will fit in a single packet */
	if ((16 + MAXIMUM_DESCRIPTOR_SIZE
			+ (MAXIMUM_NUMBER_OF_STREAMS_PER_PROGRAM *
			(5 + MAXIMUM_DESCRIPTOR_SIZE))) > 183)
		pr_crit("MultiplexorGeneratePmt - Pmt may be larger than one packet, code renovations are needed.\n");

	/* Which PcrPid to use */
	if (Context->Settings.GeneratePcrStream) {
		PcrPid = Context->Settings.PcrPid;
	} else {
		PcrPid = 0;

		for (i = 0; i < MAXIMUM_NUMBER_OF_STREAMS_PER_PROGRAM; i++)
			if (Context->Streams[ProgramId][i].EntryUsed
				&& Context->Streams[ProgramId][i].IncorporatePcrPacket) {
				PcrPid =
					Context->Streams[ProgramId][i].StreamPid;
				break;
			}

		if (i == MAXIMUM_NUMBER_OF_STREAMS_PER_PROGRAM) {
			pr_err("MultiplexorGeneratePmt - No PCR available for ProgramId %d.\n",
				ProgramId);
			return MultiplexorNoPcrChannelForProgram;
		}
	}

	/* Generate the header */
	Program = &Context->Programs[ProgramId];
	Pmt = Program->PmtPacket;
	/* Sync byte */
	*(Pmt++) = 0x47;
	/* Pusi + 5 bits of pid */
	*(Pmt++) = 0x40 | ((Context->Programs[ProgramId].PMTPid >> 8) & 0x1f);
	/* Bottom 8 bits of Pid */
	*(Pmt++) = Context->Programs[ProgramId].PMTPid & 0xff;
	*(Pmt) |= 0x10; /* Not scrambled, */
	/* Payload only, */
	/* CC not changed from current/initial value */

	if (Context->Settings.GlobalFlags
				& TABLE_DISCONTINUITY_FLAG) {
		/* Adaptation field exist */
		*(Pmt++) |= DVB_ADAPTATION_FIELD_MASK;
		/* Adaptation field size */
		*(Pmt++) = 1;
		*(Pmt++) |=
			DVB_DISCONTINUITY_INDICATOR_MASK;
	} else
		*(Pmt++) &= (~DVB_ADAPTATION_FIELD_MASK);

	*(Pmt++) = 0x00; /* Section pointer value */

	/* Output the section */
	SectionStart = Pmt;
	/* Table ID */
	*(Pmt++) = 0x02;
	/* section syntax on */
	*(Pmt++) = 0xB0;
	/* reserved fields */
	/* first 4 bits of section length */
	/* Leave length for calculation later */
	LengthField = Pmt++;
	/* Program Number */
	*(Pmt++) = (Context->Programs[ProgramId].ProgramNumber >> 8) & 0xff;
	*(Pmt++) = Context->Programs[ProgramId].ProgramNumber & 0xff;

	/* Increment table version */
	TableVersion = (*Pmt + 2) & 0x3e;
	/* Reserved fields */
	*(Pmt++) = 0xc1 | TableVersion;
	/* Table version */
	/* Current indicator */

	*(Pmt++) = 0x00; /* Section Number */

	*(Pmt++) = 0x00; /* Last section number */

	*(Pmt++) = (PcrPid >> 8) & 0xff; /* Pcr pid */
	*(Pmt++) = PcrPid & 0xff; /* */

	*(Pmt++) = 0x00; /* Program info descriptor size */
	*(Pmt++) = Program->OptionalDescriptorSize;

	if (Program->OptionalDescriptorSize != 0)
		memcpy(Pmt, Program->Descriptor,
			Program->OptionalDescriptorSize);
	Pmt += Program->OptionalDescriptorSize;

	for (i = 0; i < MAXIMUM_NUMBER_OF_STREAMS_PER_PROGRAM; i++)
		if (Context->Streams[ProgramId][i].EntryUsed) {
			Stream = &Context->Streams[ProgramId][i];
			*(Pmt++) = Stream->StreamType; /* stream type */
			/* Reserved + 5 bits of stream pid */
			*(Pmt++) = 0xe0 | ((Stream->StreamPid >> 8) & 0x1f);
			/* bottom 8 bits of stream pid */
			*(Pmt++) = Stream->StreamPid & 0xff;
			/* Program info descriptor size */
			*(Pmt++) = 0x00;
			*(Pmt++) = Stream->OptionalDescriptorSize;

			if (Stream->OptionalDescriptorSize != 0)
				memcpy(Pmt, Stream->Descriptor,
					Stream->OptionalDescriptorSize);
			Pmt += Stream->OptionalDescriptorSize;
		}

	/* Append the CRC - calculated with length already set */
	/* accumulated length + 4 gives us the CRC field */
	*LengthField = Pmt - (LengthField + 1) + 4;

	SectionLength = Pmt - SectionStart;
	MultiplexorAppendCrc(SectionStart, &SectionLength);

	Pmt += 4;

	/* Stuff out to packet length */
	memset(Pmt, 0xff, &Program->PmtPacket[SIZE_OF_TRANSPORT_PACKET] - Pmt);

	return MultiplexorNoError;
}

/* PRIVATE - Function to generate a simple sdt table */
/* */
/*	NOTE -	The Sdt is a simple version of this table, */
/*		we provide only one descriptor (per program), */
/*		the service descriptor. */
/* */
/*	NOTE2 -	We have upto 4 programs, and the strings are */
/*		upto 15 chars in length (not counting the null). */
/*		which means the SDT is 15 + 4 * (10 + 2*15) max */
/*		size. Or 175 bytes long. This fits in 1 */
/*		transport packet. Do not make it any larger */
/*		without adding checking, and removing the fatal */
/*		error check */
MultiplexorStatus_t MultiplexorGenerateSdt(MultiplexorContext_t Context)
{
	unsigned int i;
	unsigned char TableVersion;
	unsigned char *Sdt;
	unsigned char *SectionStart;
	unsigned char *LengthField;
	unsigned int SectionLength;

	if ((MAXIMUM_NUMBER_OF_PROGRAMS > 4) || (MAXIMUM_NAME_SIZE > 16))
		pr_crit("MultiplexorGenerateSdt - Sdt may be larger than one packet, code renovations are needed.\n");

	/* Generate the header */
	Sdt = Context->SdtPacket;
	/* Sync byte */
	*(Sdt++) = 0x47;
	/* Pusi */
	*(Sdt++) = 0x40;
	/* Pid == 0011 */
	*(Sdt++) = 0x11;
	/* Not scrambled, */
	*(Sdt) |= 0x10;
	/* Payload only, */
	/* CC not changed from current/initial value */

	if (Context->Settings.GlobalFlags
				& TABLE_DISCONTINUITY_FLAG) {
		/* Adaptation field exist */
		*(Sdt++) |= DVB_ADAPTATION_FIELD_MASK;
		/* Adaptation field size */
		*(Sdt++) = 1;
		*(Sdt++) |=
			DVB_DISCONTINUITY_INDICATOR_MASK;
	} else
		*(Sdt++) &= (~DVB_ADAPTATION_FIELD_MASK);

	/* Section pointer value */
	*(Sdt++) = 0x00;

	/* Output the section */
	SectionStart = Sdt;
	/* Table ID */
	*(Sdt++) = 0x42;
	/* section syntax on */
	*(Sdt++) = 0xB0;
	/* reserved fields */
	/* first 4 bits of section length */
	/* Leave length for calculation later */
	LengthField = Sdt++;
	/* Transport stream id */
	*(Sdt++) = (Context->Settings.TransportStreamId >> 8) & 0xff;
	*(Sdt++) = Context->Settings.TransportStreamId & 0xff;
	/* Increment table version */
	TableVersion = (*Sdt + 2) & 0x3e;
	/* Reserved fields */
	*(Sdt++) = 0xc1 | TableVersion;
	/* Table version */
	/* Current indicator */
	/* Section Number */
	*(Sdt++) = 0x00;
	/* Last section number */
	*(Sdt++) = 0x00;
	/* Original network ID */
	*(Sdt++) = 0x00;
	*(Sdt++) = 0x00;
	/* Reserved for future use - not sure about value */
	*(Sdt++) = 0x00;

	for (i = 0; i < MAXIMUM_NUMBER_OF_PROGRAMS; i++)
		if (Context->Programs[i].EntryUsed
			&& !Context->Programs[i].TablesProgram) {
			/* Program Number */
			*(Sdt++) = 0x00;
			*(Sdt++) = i;
			/* Reserved (not sure of value) + EIT flags */
			*(Sdt++) = 0x00;
			/* Running + free_CA_Mode + 4 bits of descriptors length */
			*(Sdt++) = 0x80 |
				(Context->Programs[i].StreamsMayBeScrambled ?
						0x10 : 0);
			/* 8 bits of descriptor loop length */
			*(Sdt++) = 5 +
				strlen(Context->Programs[i].ProviderName) +
				strlen(Context->Programs[i].ServiceName);
			 /* The descriptor tag */
			*(Sdt++) = 0x48;
			/* Descriptor length */
			*(Sdt++) = 3 +
				strlen(Context->Programs[i].ProviderName) +
				strlen(Context->Programs[i].ServiceName);
			/* Service type = digital television service */
			*(Sdt++) = 0x01;
			/* Provider */
			*(Sdt++) = strlen(Context->Programs[i].ProviderName);
			if (strlen(Context->Programs[i].ProviderName) != 0)
				memcpy(Sdt, Context->Programs[i].ProviderName,
					strlen(Context->Programs[i].ProviderName));
			Sdt += strlen(Context->Programs[i].ProviderName);
			/* service */
			*(Sdt++) = strlen(Context->Programs[i].ServiceName);
			if (strlen(Context->Programs[i].ServiceName))
				memcpy(Sdt, Context->Programs[i].ServiceName,
					strlen(Context->Programs[i].ServiceName));
			Sdt += strlen(Context->Programs[i].ServiceName);
		}

	/* Append the CRC - calculated with length already set */
	/* accumulated length + 4 gives us the CRC field */
	*LengthField = Sdt - (LengthField + 1) + 4;

	SectionLength = Sdt - SectionStart;
	MultiplexorAppendCrc(SectionStart, &SectionLength);

	Sdt += 4;

	/* Stuff out to packet length */
	memset(Sdt, 0xff, &Context->SdtPacket[SIZE_OF_TRANSPORT_PACKET] - Sdt);

	return MultiplexorNoError;
}

/* PRIVATE - Function to generate a dit table */
MultiplexorStatus_t MultiplexorGenerateDit(MultiplexorContext_t Context)
{
	unsigned char *Dit;

	/* Generate the header */
	Dit = Context->DitPacket;
	/* Sync byte */
	*(Dit++) = 0x47;
	/* Pusi */
	*(Dit++) = 0x40;
	/* Pid == 001e */
	*(Dit++) = 0x1e;
	/* Not scrambled, */
	*(Dit++) |= 0x10;
	/* Payload only, */
	/* CC not changed from current/initial value */
	/* Section pointer value */
	*(Dit++) = 0x00;

	/* Output the section */
	/* Table ID */
	*(Dit++) = 0x7e;
	/* section syntax off */
	*(Dit++) = 0x00;
	/* reserved fields */
	/* first 4 bits of section length */
	/* Length is 1 */
	*(Dit++) = 0x01;
	/* Transition type flag - to be filled in on insertion */
	*(Dit++) = 0x00;

	/* Stuff out to packet length */
	memset(Dit, 0xff, &Context->DitPacket[SIZE_OF_TRANSPORT_PACKET] - Dit);

	return MultiplexorNoError;
}

/* PRIVATE - Function to generate the initial PCR. */
/* */
/* This function generates an initial PCR by calculating the value
 * such that the time at which the first packet will leave its associated
 * T-STD buffer will be when all of the T-STD buffers reach their optimum
 * percentage level of fullness at the maximum bitrate available. */
MultiplexorStatus_t MultiplexorGenerateInitialPcr(MultiplexorContext_t Context)
{
	unsigned int i;
	unsigned long long MinDTS;
	unsigned long long TotalTStdBits;
	unsigned long long TimeToFillBuffers;
	bool prewrapped = false;
	MultiplexorStreamParameters_t *stream;

	/* Special case no streams */
	if (Context->StreamCount == 0) {
		Context->InitialPcr = 0;
		Context->PcrOffset = 0;
		Context->PcrInitialized = true;
		return MultiplexorNoError;
	}

	/* Scan all used streams, to check that they all have */
	/* some at least one buffer (with its DTS). */
	/* To find the earliest DTS, and to sum the sizes of */
	/* all T-STD buffers. */
	MinDTS = 0xffffffffffffffffull;
	TotalTStdBits = 0;

	for (i = 0; i < Context->StreamCount; i++) {
		stream = Context->PackedStreams[i];

		if (stream->DataBuffers == NULL) {
			if (Context->Flushing)
				continue;

			pr_err("MultiplexorGenerateInitialPcr - No data at all for stream.\n");
			Context->OutputStatus->UnderflowProgram	=
					stream->ProgramId;
			Context->OutputStatus->UnderflowStream =
					stream->StreamId;
			return MultiplexorInputUnderflow;
		}

		if (stream->StreamPes) {
			unsigned long long firstDTS;

			TotalTStdBits
				+= stream->DecoderBitBufferSize;

			if (stream->DataBuffers->NumberOfTransportPackets
					== 0) {
				pr_debug("Stream %d:%d is paused, ignoring for initial PCR\n",
					stream->ProgramId, stream->StreamId);
				continue;
			} else {
				pr_debug("Stream %d:%d is NOT paused, using for initial PCR\n",
					stream->ProgramId, stream->StreamId);
			}

			firstDTS = stream->DataBuffers->DTS;

			if (MinDTS == 0xffffffffffffffffull)
				MinDTS = firstDTS;
			/* Normal case where both minDTS and firstDTS are on
			 * the same side of the DTS wrap boundary.
			 * We take the new DTS if it is less than the current
			 * minDTS.
			 * This is confirmed with the second condition which
			 * compares the distance between the timestamps
			 * compared to the distance to the MAX plus the
			 * distance from zero. In this case the distance
			 * between the timestamps should be shorter */
			if ((firstDTS < MinDTS) &&
					((MinDTS - firstDTS) <
					(MAX_DTS - MinDTS + firstDTS)))
				MinDTS = firstDTS;
			/* Case where one of the input DTS values is
			 * pre-wrapped due to, for example, audio timestamp
			 * correction.
			 * In this case the 2 timestamps span across the max
			 * timestamp boundary. So here the new DTS is taken if
			 * it is MORE than the current minDTS!.
			 * We confirm that we are spanning the max DTS boundary
			 * by confirming that the distance between the 2
			 * timestamps is greater than the sum of the distance
			 * to the max value plus the distance from zero. In
			 * this case distance between the timestamps will be
			 * greater.
			 * We also flag here that we have found a pre-wrapped
			 * timestamp so that we can adjust the PCR shift
			 * calculation accordingly
			 */
			if ((firstDTS > MinDTS) && ((firstDTS - MinDTS) >
					(MAX_DTS - firstDTS + MinDTS))) {
				MinDTS = firstDTS;
				prewrapped = true;
			}
		}
	}

	if (MinDTS == 0xffffffffffffffffull)
		MinDTS = 0;

	/* Now perform the calculation. */
	TimeToFillBuffers = (TotalTStdBits * 90000ull)
		/ Context->Settings.Bitrate;
	if (TimeToFillBuffers < Context->Settings.PcrPeriod)
		TimeToFillBuffers = Context->Settings.PcrPeriod;

	Context->InitialPcr = PcrLimit(MinDTS - TimeToFillBuffers);
	Context->PcrOffset = 0;
	Context->ShiftPCR = 0;

	pr_debug("minDTS=0x%010llx timetofill=0x%010llx calc PCR=0x%010llx\n",
		MinDTS, TimeToFillBuffers, Context->InitialPcr);

	/* Shift the PCR and all PTS/DTS values if the first PCR is wrapped
	 * or if one of the input DTS values was pre-wrapped relative to the
	 * others (which guarantees that the PCR is wrapped).
	 * The shift is calculated so that the first PCR value is zero.
	 */
	if (prewrapped || (Context->InitialPcr > MinDTS)) {
		/* Add 1 to take initial PCR to 0 instead of 0x1ffffffff */
		Context->ShiftPCR = MAX_DTS - Context->InitialPcr + 1;
		pr_debug("PCR shift=0x%010llx\n", Context->ShiftPCR);
	}

	Context->PcrInitialized = true;

	/* Finally update the DTS value for all repeating buffers or
	 * initial section buffers with DTS equal to 0 */
	for (i = 0; i < Context->StreamCount; i++) {
		stream = Context->PackedStreams[i];
		if ((stream->DataBuffers != NULL) &&
			(stream->DataBuffers->Repeating
			|| (!stream->StreamPes &&
				stream->DataBuffers->DTS == 0)))

			stream->DataBuffers->DTS = Context->InitialPcr;
	}

	return MultiplexorNoError;
}

/* PRIVATE - Function to format PCR as per TS AF */
void MultiplexorBuildAFPCR(unsigned char *buf, unsigned long long clock27MHz)
{
	unsigned long long PCR;
	unsigned long long PCRExtension;

	PCR = clock27MHz / 300;

	/* Avoid using modulus - may be in kernel */
	PCRExtension = clock27MHz - (300 * PCR);
	buf[0] = ((PCR >> 25) & 0xff);
	buf[1] = ((PCR >> 17) & 0xff);
	buf[2] = ((PCR >> 9) & 0xff);
	buf[3] = ((PCR >> 1) & 0xff);
	buf[4] = ((PCR << 7) & 0x80) | ((PCRExtension >> 8) & 0x01);
	buf[5] = (PCRExtension & 0xff);
}

#define PROGRAM_STREAM_MAP	0xBC
#define PADDING_STREAM		0xBE
#define PRIVATE_STREAM_2	0xBF
#define ECM_STREAM		0xF0
#define EMM_STREAM		0xF1
#define PROGRAM_STREAM_DIR	0xFF
#define DSMCC_STREAM		0xF2
#define H222_1_TYPE_E_STREAM	0xF8

static bool MultiplexorPesHasHeader(unsigned char stream_id)
{
	switch (stream_id) {
	case PROGRAM_STREAM_MAP:
	case PADDING_STREAM:
	case PRIVATE_STREAM_2:
	case ECM_STREAM:
	case EMM_STREAM:
	case PROGRAM_STREAM_DIR:
	case DSMCC_STREAM:
	case H222_1_TYPE_E_STREAM:
		return false;
	default:
		return true;
	}
}

static void MultiplexorCorrectPesHeader(MultiplexorBufferParameters_t *Buffer,
	unsigned long long shift)
{
	unsigned char *data;
	unsigned int pes_length;
	unsigned long long pts;
	unsigned char mark;

	/* Check there is enough data for a valid PES header */
	if (Buffer->TotalUsedSize < 14 || Buffer->Pages->Size < 14) {
		pr_warn("Buffer too short for PES header, not correcting\n");
		return;
	}
	data = Buffer->Pages[0].Base;

	/* Check we have a valid PES header with timestamps */
	if (!((data[0] == 0x0) && (data[1] == 0x0) && (data[2] == 0x1))) {
		pr_warn("Buffer has invalid start code, not correcting\n");
		return;
	}
	if (!MultiplexorPesHasHeader(data[3])) {
		pr_warn("Buffer has no pes header, not correcting\n");
		return;
	}
	pes_length = (data[4] << 8) | data[5];
	if ((pes_length < 8) && (pes_length != 0)) {
		pr_warn("Buffer has invalid pes length, not correcting\n");
		return;
	}
	if (!(data[7] & 0x80)) {
		pr_warn("Buffer has no PTS flag, not correcting\n");
		return;
	}
	if (((data[7] & 0xc0) == 0x80) && data[8] < 5) {
		pr_warn("Buffer has invalid pes header length, not correcting\n");
		return;
	}
	if (((data[7] & 0xc0) == 0xc0) && data[8] < 10) {
		pr_warn("Buffer has invalid pes header length, not correcting\n");
		return;
	}
	/* Get the top PTS bits marker bit mask */
	if ((data[7] & 0xc0) == 0x80)
		mark = 0x21;
	else
		mark = 0x31;
	/* Now extract the PTS value */
	pts = ((unsigned long long)data[9]  & 0x0e) << 29;
	pts |= ((unsigned long long)data[10] & 0xff) << 22;
	pts |= ((unsigned long long)data[11] & 0xfe) << 14;
	pts |= ((unsigned long long)data[12] & 0xff) << 7;
	pts |= ((unsigned long long)data[13] & 0xfe) >> 1;
	/* Shift the PTS value */
	pts = PcrLimit(pts + shift);
	/* Update the pes header PTS value */
	data[9] = (unsigned char)(pts >> 29 & 0x0e) | mark;
	data[10] = (unsigned char)(pts >> 22);
	data[11] = (unsigned char)(pts >> 14) | 0x01;
	data[12] = (unsigned char)(pts >> 7);
	data[13] = (unsigned char)(pts << 1) | 0x01;
	/* If we have DTS also, then shift this too */
	if ((data[7] & 0xc0) == 0xc0) {
		/* Extract the DTS value */
		pts = ((unsigned long long)data[14]  & 0x0e) << 29;
		pts |= ((unsigned long long)data[15] & 0xff) << 22;
		pts |= ((unsigned long long)data[16] & 0xfe) << 14;
		pts |= ((unsigned long long)data[17] & 0xff) << 7;
		pts |= ((unsigned long long)data[18] & 0xfe) >> 1;
		/* Shift the DTS value */
		pts = PcrLimit(pts + shift);
		/* Update the pes header DTS value */
		/* Set the actual DTS value */
		data[14] = (unsigned char)(pts >> 29 & 0x0e) | 0x11;
		data[15] = (unsigned char)(pts >> 22);
		data[16] = (unsigned char)(pts >> 14) | 0x01;
		data[17] = (unsigned char)(pts >> 7);
		data[18] = (unsigned char)(pts << 1) | 0x01;
	}
	pr_debug("Corrected PES header for dts 0x%010llx is 0x%010llx\n",
		Buffer->DTS, pts);
	return;
}

static void complete_buffer(MultiplexorContext_t Context,
	MultiplexorBufferParameters_t *Buffer)
{
	MultiplexorOutputStatus_t *status;

	status = Context->OutputStatus;
	status->CompletedBuffers[status->CompletedBufferCount++] =
			(unsigned int)Buffer->UserData;
	pr_debug("Adding buffer %p to completed list\n",
		Buffer->UserData);
	Context->Settings.BufferReleaseCallback(
		Context, false, Buffer);
	return;
}

/* PRIVATE - Function to output one packet based on the descriptor code */
MultiplexorStatus_t MultiplexorOutputOnePacket(MultiplexorContext_t Context,
	unsigned int PacketNumber, unsigned char Code)
{
	unsigned long long Current27MhzClock;
	unsigned char *Packet;
	MultiplexorStreamParameters_t *Stream;
	MultiplexorBufferParameters_t *Buffer;
	bool FirstPacket;
	bool LastPacket;
	bool Adaptation;
	unsigned int Pid;
	unsigned int CC;
	unsigned int PayloadSizeAvailable;
	unsigned int AdaptationPad;
	unsigned int Header;
	unsigned int AdaptationSize;
	unsigned int ThisOutput;
	unsigned char AdaptationBuffer[DVB_MAX_PAYLOAD_SIZE];

	/* Output the 27mhz clock header for time stamped packets */
	Current27MhzClock
		= (PcrLimit(Context->OutputStatus->PCR + Context->ShiftPCR)
				* 300)
			+ ((Context->BitsPerPacket
				* PacketNumber
				* 27000000ULL)
				/ Context->OutputStatus->Bitrate);

	if (Context->Settings.TimeStampedPackets)
		MultiplexorOutputWord(Context,
			(unsigned int)(Current27MhzClock & 0xffffffffull));

	/* Process the individual packet types */
	switch (ExtractPacketType(Code)) {
	case OUTPUT_DATA_PACKET:
		Stream = &Context->Streams[ExtractProgram(Code)][ExtractStream(Code)];
		Buffer = Stream->DataBuffers;
		FirstPacket = Buffer->RemainingBufferSize == 0;

		PayloadSizeAvailable = DVB_MAX_PAYLOAD_SIZE;
		Adaptation = false;
		AdaptationSize = 0;

		AdaptationBuffer[0] = 0;
		AdaptationBuffer[1] = 0;

		if (FirstPacket) {
			Buffer->RemainingBufferSize = Buffer->TotalUsedSize;
			Buffer->CurrentPage = Buffer->Pages;
			Buffer->RemainingPageSize = Buffer->CurrentPage->Size;
			Buffer->PagePointer = Buffer->CurrentPage->Base;

			if (Buffer->Discontinuity) {
				Adaptation = true;
				AdaptationSize = 2;
				PayloadSizeAvailable -= 2;
				AdaptationBuffer[1] |=
					DVB_DISCONTINUITY_INDICATOR_MASK;
			}
			/* Section pointer field */
			if (Stream->StreamPes == false)
				PayloadSizeAvailable -= 1;
			else {
				/* Correct PES PTS/DTS if necessary */
				if (Context->ShiftPCR)
					MultiplexorCorrectPesHeader(Buffer,
							Context->ShiftPCR);
			}
		}

		if (Buffer->RequestRAPBit && Buffer->OutstandingRAPRequest) {
			if (!Adaptation) {
				Adaptation = true;
				AdaptationSize = 2;
				PayloadSizeAvailable -= 2;
			}
			if (Stream->IncorporatePcrPacket) {
				AdaptationSize += DVB_PCR_AF_SIZE;
				PayloadSizeAvailable -= DVB_PCR_AF_SIZE;
				AdaptationBuffer[1] |=
					DVB_PCR_INDICATOR_MASK;
			}
			AdaptationBuffer[1] |= DVB_RAP_INDICATOR_MASK;
			Buffer->OutstandingRAPRequest = false;
			pr_debug("Generating RAP bit on ts data packet");
		}
		LastPacket = Buffer->RemainingBufferSize
			<= PayloadSizeAvailable;
		AdaptationPad = LastPacket ? (PayloadSizeAvailable -
			Buffer->RemainingBufferSize) : 0;

		if (AdaptationPad)
			Adaptation = true;

		Stream->ContinuityCount = (Stream->ContinuityCount + 1) & 0xf;

		Header = DvbPacketHeader(FirstPacket, Stream->StreamPid,
				Buffer->Scrambled, Buffer->Parity, Adaptation,
				true, Stream->ContinuityCount);
		MultiplexorOutputWord(Context, Header);

		/* Output the adaptation field */
		/* Currently our implementation has length byte, 1 flags byte,
		 * and optional stuffing bytes. */
		if (Adaptation) {
			AdaptationSize += AdaptationPad;
			if (AdaptationSize > DVB_MAX_PAYLOAD_SIZE) {
				pr_crit("Mux Implementation Error");
				AdaptationSize = DVB_MAX_PAYLOAD_SIZE;
			}

			AdaptationBuffer[0] = AdaptationSize - 1;
			/* Note if AdaptationSize == 1 we don't output
			 * this byte */

			if (FirstPacket &&
				AdaptationBuffer[1] & DVB_PCR_INDICATOR_MASK) {
				MultiplexorBuildAFPCR(&AdaptationBuffer[2],
							Current27MhzClock);

				memset(&AdaptationBuffer[2 + DVB_PCR_AF_SIZE],
						0xff,
						(unsigned int)(AdaptationSize -
						(2 + DVB_PCR_AF_SIZE)));
			} else if (AdaptationSize > 2)
				memset(&AdaptationBuffer[2], 0xff,
					(AdaptationSize - 2));

			MultiplexorOutputData(Context, AdaptationBuffer,
				AdaptationSize);

			PayloadSizeAvailable -= AdaptationPad;
		}

		/* Output section pointer field - always zero */
		if (FirstPacket && (Stream->StreamPes == false))
			MultiplexorOutputByte(Context, 0x00);

		/* Output packet body */
		while (PayloadSizeAvailable != 0) {
			ThisOutput = min(PayloadSizeAvailable,
				Buffer->RemainingPageSize);
			MultiplexorOutputData(Context, Buffer->PagePointer,
				ThisOutput);
			PayloadSizeAvailable -= ThisOutput;

			Buffer->RemainingBufferSize -= ThisOutput;
			Buffer->RemainingPageSize -= ThisOutput;
			Buffer->PagePointer += ThisOutput;
			if ((Buffer->RemainingPageSize == 0)
				&& (Buffer->RemainingBufferSize != 0)) {
				Buffer->CurrentPage = Buffer->CurrentPage->Next;

				if (Buffer->CurrentPage == NULL) {
					pr_crit("MultiplexorOutputOnePacket - Run out of pages in output (%d %d).\n",
							PayloadSizeAvailable,
							LastPacket);
					return MultiplexorError;
				}
				Buffer->RemainingPageSize
					= Buffer->CurrentPage->Size;
				Buffer->PagePointer = Buffer->CurrentPage->Base;
			}
		}

		/* If we have exhausted the buffer, move on to the next */
		if (LastPacket) {
			if (Buffer->Repeating) {
				/* Force reset of pointers */
				Buffer->RemainingBufferSize = 0;
			} else {
				Stream->DataBuffers = Buffer->NextDataBuffer;
				complete_buffer(Context, Buffer);
			}
		}

		break;

	case OUTPUT_PCR_PACKET:
		if ((ExtractProgram(Code) == 3) && (ExtractStream(Code) == 15)) {
			Pid = Context->Settings.PcrPid;
			CC = 0;
			if (Context->Settings.GlobalFlags
					& PCR_DISCONTINUITY_FLAG)
				Context->PcrPacket[5] |=
					DVB_DISCONTINUITY_INDICATOR_MASK;
		} else {
			Stream = &Context->Streams[ExtractProgram(Code)][ExtractStream(Code)];
			Pid = Stream->StreamPid;
			CC = Stream->ContinuityCount;
		}

		Context->PcrPacket[1] = ((Pid >> 8) & 0xff);
		Context->PcrPacket[2] = (Pid & 0xff);
		Context->PcrPacket[3] = (Context->PcrPacket[3] & 0xf0) |
						(CC & 0x0f);
		MultiplexorBuildAFPCR(&Context->PcrPacket[6],
					Current27MhzClock);

		MultiplexorOutputData(Context, Context->PcrPacket,
			SIZE_OF_TRANSPORT_PACKET);
		if (Context->Settings.GlobalFlags & PCR_DISCONTINUITY_FLAG) {
			Context->PcrPacket[5] &=
				(~DVB_DISCONTINUITY_INDICATOR_MASK);
			Context->Settings.GlobalFlags &=
				(~PCR_DISCONTINUITY_FLAG);
		}
		break;

	case OUTPUT_TABLE_PACKET:
	{
		uint32_t table_code = ExtractTableId(Code);
		switch (table_code) {
		case TABLE_ID_PAT:
			Packet = Context->PatPacket;
			break;

		case TABLE_ID_PMT:
			Packet = Context->Programs[ExtractProgram(Code)].PmtPacket;
			break;

		case TABLE_ID_SDT:
			Packet = Context->SdtPacket;
			break;

		case TABLE_ID_DIT:
			Packet = Context->DitPacket;
			/* Insert the transition flag as the only data in
			 * the section */
			Packet[8] = (ExtractTransitionFlag(Code) != 0) ?
					0x80 : 0x00;
			break;

		default:
			pr_crit("MultiplexorOutputOnePacket - Unknown table ID (%d).\n",
				ExtractTableId(Code));
			return MultiplexorError;
		}

		/* Increment the continuity count field */
		Packet[3] = (Packet[3] & 0xf0) | ((Packet[3] + 1) & 0x0f);
		MultiplexorOutputData(Context, Packet, SIZE_OF_TRANSPORT_PACKET);
		if ((Context->Settings.GlobalFlags
				& TABLE_DISCONTINUITY_FLAG)
				&& (table_code == TABLE_ID_DIT)) {
			Context->Settings.GlobalFlags &=
				(~TABLE_DISCONTINUITY_FLAG);
			Context->RegenerateTables = true;
		}
		break;
	}
	case OUTPUT_NULL_PACKET:
		MultiplexorOutputData(Context, NullPacket,
			SIZE_OF_TRANSPORT_PACKET);
		break;
	}

	return MultiplexorNoError;
}

/* PRIVATE - Function to output one byte */
MultiplexorStatus_t MultiplexorOutputByte(MultiplexorContext_t Context,
	unsigned char Value)
{
	MultiplexorBufferParameters_t *Buffer;

	Buffer = Context->OutputBuffer;

	*(Buffer->PagePointer++) = Value;
	Buffer->TotalUsedSize++;
	Buffer->RemainingPageSize--;

	if (Buffer->RemainingPageSize == 0) {
		Buffer->CurrentPage = Buffer->CurrentPage->Next;
		Buffer->RemainingPageSize = Buffer->CurrentPage->Size;
		Buffer->PagePointer = Buffer->CurrentPage->Base;
	}

	return MultiplexorNoError;
}


/* PRIVATE - Function to output one word */
MultiplexorStatus_t MultiplexorOutputWord(MultiplexorContext_t Context,
	unsigned int Value)
{
	MultiplexorBufferParameters_t *Buffer;

	Buffer = Context->OutputBuffer;

	if (Buffer->RemainingPageSize >= 4) {
		Buffer->PagePointer[0] = (Value >> 24) & 0xff;
		Buffer->PagePointer[1] = (Value >> 16) & 0xff;
		Buffer->PagePointer[2] = (Value >> 8) & 0xff;
		Buffer->PagePointer[3] = Value & 0xff;
		Buffer->PagePointer += 4;
		Buffer->TotalUsedSize += 4;
		Buffer->RemainingPageSize -= 4;

		if (Buffer->RemainingPageSize == 0) {
			Buffer->CurrentPage = Buffer->CurrentPage->Next;
			Buffer->RemainingPageSize = Buffer->CurrentPage->Size;
			Buffer->PagePointer = Buffer->CurrentPage->Base;
		}
	} else {
		MultiplexorOutputByte(Context, ((Value >> 24) & 0xff));
		MultiplexorOutputByte(Context, ((Value >> 16) & 0xff));
		MultiplexorOutputByte(Context, ((Value >> 8) & 0xff));
		MultiplexorOutputByte(Context, (Value & 0xff));
	}

	return MultiplexorNoError;
}

/* PRIVATE - Function to output a block of data */
MultiplexorStatus_t MultiplexorOutputData(MultiplexorContext_t Context,
	unsigned char *Data, unsigned int Size)
{
	MultiplexorBufferParameters_t *Buffer;
	unsigned int ThisOutput;

	if (Size > SIZE_OF_TRANSPORT_PACKET) {
		pr_warn("Attempting to output more than a packet, %d\n", Size);
	}

	Buffer = Context->OutputBuffer;

	while (Size > 0) {
		ThisOutput = min(Size, Buffer->RemainingPageSize);
		if (ThisOutput > SIZE_OF_TRANSPORT_PACKET)
			pr_warn("Bad output memcpy Size=%d, Remaining=%d\n",
					Size, Buffer->RemainingPageSize);
		memcpy(Buffer->PagePointer, Data, ThisOutput);
		Size -= ThisOutput;
		Data += ThisOutput;
		Buffer->PagePointer += ThisOutput;
		Buffer->TotalUsedSize += ThisOutput;
		Buffer->RemainingPageSize -= ThisOutput;

		if (Buffer->RemainingPageSize == 0) {
			Buffer->CurrentPage = Buffer->CurrentPage->Next;
			Buffer->RemainingPageSize = Buffer->CurrentPage->Size;
			Buffer->PagePointer = Buffer->CurrentPage->Base;
		}
	}

	return MultiplexorNoError;
}

/* PRIVATE - Critical function that calculates how many packets */
/*		can be transmitted for a particular stream during */
/*		one pcr period. */
MultiplexorStatus_t MultiplexorCalculatePcrPeriodPackets(
	MultiplexorContext_t Context, MultiplexorStreamParameters_t *Stream,
	unsigned int *Packets)
{
	unsigned int i;
	int BitBufferLevel;
	int BitsNeeded;
	int PacketsToTransmit;
	unsigned long long Dts;
	unsigned long long Periodstart;
	unsigned long long PeriodEnd;
	unsigned long long PeriodDontScanBeyond;
	MultiplexorBufferParameters_t *CheckBuffer;

	bool IsTableStream = Context->Programs[Stream->ProgramId].TablesProgram;

	/* Is there a PCR packet */
	PacketsToTransmit = Stream->IncorporatePcrPacket ? 1 : 0;

	/* Split off the simple section streams */
	Periodstart = PcrLimit(Context->InitialPcr + Context->PcrOffset);
	PeriodEnd = PcrLimit(Context->InitialPcr + Context->PcrOffset +
			Context->Settings.PcrPeriod);
	PeriodDontScanBeyond = PcrLimit(Context->InitialPcr +
			Context->PcrOffset + Context->Settings.PcrPeriod +
			Stream->MultiplexAheadLimit);
	pr_debug("Stream_id=%d, Periodstart=0x%llx, PeriodEnd=0x%llx PeriodDontScanBeyond=0x%llx\n",
		Stream->StreamId, Periodstart, PeriodEnd, PeriodDontScanBeyond);


	if (!Stream->StreamPes && (Stream->DataBuffers != NULL)
		&& Stream->DataBuffers->Repeating) {

		for (Dts = Stream->DataBuffers->DTS;
				InTimePeriod(Dts, Periodstart, PeriodEnd);
				Dts = PcrLimit(Dts +
					Stream->DataBuffers->RepeatInterval))
			PacketsToTransmit +=
				Stream->DataBuffers->NumberOfTransportPackets;

		*Packets = PacketsToTransmit;

		return MultiplexorNoError;
	}

	/* Handle the bit buffer levelled streams */
	BitBufferLevel = Stream->CurrentBitBufferLevel;

	/* First what data in the bit buffer will be extracted during this period */
	for (i = Stream->NextUsedExpiryRecord; i < Stream->NextFreeExpiryRecord; i++)
		if (InTimePeriod(Stream->BitBufferExpiryPcrs[i%NUMBER_OF_BIT_BUFFER_EXPIRY_RECORDS],
				Periodstart, PeriodEnd))
			BitBufferLevel -=
				Stream->BitBufferExpiryBits[i%NUMBER_OF_BIT_BUFFER_EXPIRY_RECORDS];

	/* What queued data will expire in the same period */
	/* NOTE This verifies that we actually have data queued */
	/*      for the period. */
	for (CheckBuffer = Stream->DataBuffers;
		(CheckBuffer != NULL) &&
			(CheckBuffer->NumberOfTransportPackets != 0);
		CheckBuffer = CheckBuffer->NextDataBuffer) {
		if (InTimePeriod(CheckBuffer->DTS, Periodstart, PeriodEnd))
			BitBufferLevel -= (CheckBuffer->TotalUsedSize * 8);
		else if (TimeIsBefore(CheckBuffer->DTS, Periodstart)) {
			pr_warn("Bit buffer level is too short stream id %d\n",
				Stream->StreamId);
			BitBufferLevel -= (CheckBuffer->TotalUsedSize * 8);
		}
		else
			break;
	}

	if ((CheckBuffer == NULL) && !Context->Flushing && !IsTableStream) {
		pr_err("MultiplexorCalculatePcrPeriodPackets - Ran out of data while computing bits to be removed from the bit buffer during the PCR period.\n");
		pr_debug("Stream_id=%d, Periodstart=0x%llx, PeriodEnd=0x%llx\n",
			Stream->StreamId, Periodstart, PeriodEnd);
		for (CheckBuffer = Stream->DataBuffers;
			(CheckBuffer != NULL) &&
				(CheckBuffer->NumberOfTransportPackets != 0);
			CheckBuffer = CheckBuffer->NextDataBuffer) {
			pr_debug("  DTS 0x%010llx\n", CheckBuffer->DTS);
		}
		pr_debug("  Period = 0x%010llx to 0x%010llx\n", Periodstart,
			PeriodEnd);
		Context->OutputStatus->UnderflowProgram = Stream->ProgramId;
		Context->OutputStatus->UnderflowStream = Stream->StreamId;
		return MultiplexorInputUnderflow;
	}


	/* Now we need to check that there are sufficient data to be able to
	 * fill the bit buffer we do this check before calculating the number
	 * of packets that we would actually like to transmit to achieve
	 * desired fullness, because it will make the latter walking simpler */
	/* NOTE add in those already transmitted to make life easy. */
	/* NOTE2 it takes 16 bits to signal a discontinuity in the
	 * adaptation field */

	BitsNeeded = Stream->DecoderBitBufferSize - BitBufferLevel;
	if ((Stream->DataBuffers != NULL)
			&& (Stream->DataBuffers->CurrentTransportPacket != 0))
		BitsNeeded += (Stream->DataBuffers->CurrentTransportPacket
			* 184 * 8) -
			(Stream->DataBuffers->Discontinuity ? 16 : 0);

	for (CheckBuffer = Stream->DataBuffers;
		(CheckBuffer != NULL) &&
			(CheckBuffer->NumberOfTransportPackets != 0);
			CheckBuffer = CheckBuffer->NextDataBuffer) {

		BitsNeeded -= CheckBuffer->Size * 8;
		if (BitsNeeded <= 0)
			break;
		if (!InTimePeriod(CheckBuffer->DTS, Periodstart,
				PeriodDontScanBeyond))
			break;
	}

	if ((CheckBuffer == NULL) && !Context->Flushing && !IsTableStream) {
		pr_err("MultiplexorCalculatePcrPeriodPackets - Ran out of data while checking bit availability for transmission.\n");
		pr_debug("Stream_id=%d, BitsNeeded=%d, Periodstart=0x%llx, PeriodDontScanBeyond=0x%llx\n",
			Stream->StreamId, BitsNeeded,
			Periodstart, PeriodDontScanBeyond);
		for (CheckBuffer = Stream->DataBuffers;
				(CheckBuffer != NULL);
				CheckBuffer = CheckBuffer->NextDataBuffer) {
			pr_debug("  Size=%d, DTS 0x%010llx\n",
					CheckBuffer->Size, CheckBuffer->DTS);
		}
		Context->OutputStatus->UnderflowProgram = Stream->ProgramId;
		Context->OutputStatus->UnderflowStream = Stream->StreamId;
		return MultiplexorInputUnderflow;
	}

	/* Finally how many packets do we actually fancy transmitting rounded to the nearest packet */
	BitsNeeded = ManifestorDesiredBitBufferLevel(Stream->DecoderBitBufferSize)
				- BitBufferLevel;

	if (Stream->DataBuffers != NULL) {
		if (Stream->DataBuffers->CurrentTransportPacket != 0)
			BitsNeeded += (Stream->DataBuffers->CurrentTransportPacket
					* 184 * 8) -
					(Stream->DataBuffers->Discontinuity ?
							16 : 0);

		PacketsToTransmit = -Stream->DataBuffers->CurrentTransportPacket;
	}

	for (CheckBuffer = Stream->DataBuffers;
		(CheckBuffer != NULL) &&
			(CheckBuffer->NumberOfTransportPackets != 0);
			CheckBuffer = CheckBuffer->NextDataBuffer) {

		if (CheckBuffer->RequestDITInsertion)
			PacketsToTransmit++;

		if (BitsNeeded >= (CheckBuffer->Size * 8)) {
			PacketsToTransmit +=
				CheckBuffer->NumberOfTransportPackets;
			BitsNeeded -= (CheckBuffer->Size * 8);
		} else {
			PacketsToTransmit += (BitsNeeded + (4 * 184)) /
					(8 * 184);
			break;
		}

		if (!InTimePeriod(CheckBuffer->DTS, Periodstart,
				PeriodDontScanBeyond))
			break;
	}
	/* Is there a PCR packet */
	PacketsToTransmit += Stream->IncorporatePcrPacket ? 1 : 0;

	*Packets = PacketsToTransmit;

	return MultiplexorNoError;
}

/* PRIVATE - Function to calculate the output bitrate */
/* */
/*	We calculate an empirical bitrate before applying */
/*	fixed rate or other actions, this also performs */
/*	significant checking on the input data buffers. */
MultiplexorStatus_t MultiplexorCaclculateBitrate(MultiplexorContext_t Context)
{
	unsigned int i;
	MultiplexorStatus_t Status;
	unsigned int PcrPeriodsPerTableOuput;
	unsigned int ElapsedPcrPeriods;
	unsigned int Count;
	unsigned int TotalPacketCount;
	unsigned int AllowedPacketCount;

	/* Calculate how many times we output tables this period */
	Context->PatPmtCountThisPeriod = 0;
	if (Context->Settings.GeneratePatPmt && Context->Settings.TablePeriod) {
		if (Context->Settings.TablePeriod
			<= Context->Settings.PcrPeriod) {

			Context->PatPmtCountThisPeriod
				= (Context->Settings.PcrPeriod
					+ (Context->Settings.TablePeriod - 1))
					/ Context->Settings.TablePeriod;
		} else {
			PcrPeriodsPerTableOuput =
					Context->Settings.TablePeriod
				/ Context->Settings.PcrPeriod;
			ElapsedPcrPeriods = Context->PcrOffset
				/ Context->Settings.PcrPeriod;

			if ((ElapsedPcrPeriods % PcrPeriodsPerTableOuput) == 0)
				Context->PatPmtCountThisPeriod = 1;
		}
	}

	/* Initialize the packet count for the PCR packet and any
	 * auto-generated tables */
	/* PCR */
	TotalPacketCount = Context->Settings.GeneratePcrStream ? 1 : 0;

	TotalPacketCount += Context->PatPmtCountThisPeriod * (1 + /* Pat */
			(Context->Settings.GenerateSdt ? 1 : 0) + /* Sdt */
			Context->PMTCount); /* Pmt */

	/* Now sum the packet counts for each stream */
	for (i = 0; i < Context->StreamCount; i++) {
		Status = MultiplexorCalculatePcrPeriodPackets(Context,
			Context->PackedStreams[i], &Count);
		if (Status != MultiplexorNoError)
			return Status;

		TotalPacketCount += Count;
	}

	/* The ideal bit rate is then this packet rate, or less if this is a lower fixed rate stream */
	/* Note separate divisions in calc of AllowedPacketCount guarantees we will do two 64/32 NOT */
	/* one 64/64 divisions the latter of which are forbidden in the kernel. */
	AllowedPacketCount
		= (unsigned int) ((((unsigned long long) (Context->Settings.PcrPeriod
			* Context->Settings.Bitrate))
			/ 90000ull) / Context->BitsPerPacket);

	Context->OutputPackets
		= Context->Settings.FixedBitrate ? AllowedPacketCount
				: min(AllowedPacketCount,
						TotalPacketCount);

	Context->Bitrate
		 = (unsigned int) (((unsigned long long) (Context->OutputPackets
				 * Context->BitsPerPacket * 90000ull))
				 / Context->Settings.PcrPeriod);

	return MultiplexorNoError;
}

static void generate_index(MultiplexorContext_t c,
	MultiplexorBufferParameters_t *b,
	unsigned int program,
	unsigned int stream,
	uint32_t mask)
{
	MultiplexorIndexRecord_t *rec;
	unsigned char *idx;
	unsigned int *cnt = &c->IndexCount;
	if (b == NULL) {
		if (c->Settings.GenerateIndex) {
			uint32_t indexes = STM_TE_TSG_INDEX_PAT |
				STM_TE_TSG_INDEX_PMT |
				STM_TE_TSG_INDEX_SDT;

			indexes &= mask;
			if (indexes) {
				pr_debug("Generating table index %X\n",
						indexes);
				rec = &c->OutputStatus->Index[*cnt];
				rec->PacketOffset = c->TotalPacketCount +
					c->OutputPacketCount;
				rec->pts = 0;
				rec->Program = program;
				rec->Stream = stream;
				rec->IdentifierSize = sizeof(indexes);
				memcpy(rec->Identifier, &indexes,
						sizeof(indexes));
				(*cnt)++;
			}
		}
		return;
	}

	idx = b->IndexData;
	if (c->Settings.GenerateIndex && (b->IndexSize != 0)) {
		uint32_t indexes = idx[0] | (idx[1] << 8) | (idx[2] << 16)
			| (idx[3] << 24);

		indexes &= mask;
		if (indexes) {
			pr_debug("Generating indexes-2 %X\n", indexes);
			rec = &c->OutputStatus->Index[*cnt];
			rec->PacketOffset = c->TotalPacketCount +
				c->OutputPacketCount;
			rec->Program = program;
			rec->Stream = stream;
			rec->pts = PcrLimit(b->DTS + c->ShiftPCR);
			rec->IdentifierSize = sizeof(indexes);
			memcpy(rec->Identifier, &indexes, sizeof(indexes));
			(*cnt)++;
			/* Prevent output more than once per buffer */
			idx[0] &= ~indexes;
			idx[1] &= ~(indexes >> 8);
			idx[3] &= ~(indexes >> 16);
			idx[4] &= ~(indexes >> 24);
		}
	}
}

/* PRIVATE - Function to calculate the output bitrate */
/*	The function responsible for actually generating the multiplex. */
MultiplexorStatus_t MultiplexorPerformMultiplex(MultiplexorContext_t Ctx)
{
	unsigned int i, j;
	unsigned int Index;
	bool ScheduleFailure;
	unsigned long long BasePcr;
	unsigned long long PcrPacketDuration;
	unsigned long long ExpiryBase;
	unsigned long long Vpcr;
	unsigned long long Vdts = 0ULL;
	unsigned long long MinVdts;
	unsigned long long VdtsRotate;
	MultiplexorStreamParameters_t *Stream;
	MultiplexorStreamParameters_t *MinStream;
	MultiplexorBufferParameters_t *Buffer;
	unsigned int NewBitBufferLevel;

	/* First pass mark current multiplex buffer for each stream */
	for (i = 0; i < Ctx->StreamCount; i++) {
		Ctx->PackedStreams[i]->CurrentMultiplexBuffer
			= Ctx->PackedStreams[i]->DataBuffers;
	}

	/* Main loop */
	ScheduleFailure = false;
	BasePcr = Ctx->InitialPcr + Ctx->PcrOffset;
	PcrPacketDuration
		= (Ctx->Settings.PcrPeriod + Ctx->OutputPackets
				- 1) /
				((Ctx->OutputPackets != 0) ?
					Ctx->OutputPackets : 1);
	ExpiryBase = PcrLimit(BasePcr - 0x100000000ull);

	MinStream = NULL;

	for (i = 1; i < Ctx->OutputPackets; i++) {
		/* First, is it time to output the tables ? */

		if ((Ctx->PatPmtCountThisPeriod != 0)
			&& (((i - 1) %
				(Ctx->OutputPackets /
				Ctx->PatPmtCountThisPeriod))
				== 0)) {

			OutputCode(CodeForPat());
			generate_index(Ctx, NULL, MUX_DEFAULT_UNSPECIFIED,
					MUX_DEFAULT_UNSPECIFIED,
					STM_TE_TSG_INDEX_PAT);
			if (Ctx->Settings.GenerateSdt) {
				OutputCode(CodeForSdt());
				i++;
				generate_index(Ctx, NULL,
					MUX_DEFAULT_UNSPECIFIED,
					MUX_DEFAULT_UNSPECIFIED,
					STM_TE_TSG_INDEX_SDT);
			}

			for (j = 0; j < MAXIMUM_NUMBER_OF_PROGRAMS; j++)
				if (Ctx->Programs[j].EntryUsed &&
					!Ctx->Programs[j].TablesProgram) {
					OutputCode(CodeForPmt(j));
					i++;
					generate_index(Ctx, NULL,
						MUX_DEFAULT_UNSPECIFIED,
						MUX_DEFAULT_UNSPECIFIED,
						STM_TE_TSG_INDEX_PMT);
				}

			continue;
		}

		Vpcr = PcrLimit(BasePcr + ((i * Ctx->Settings.PcrPeriod) /
				Ctx->OutputPackets));

		/* We are going to cycle through the streams looking for the
		 * lowest VDTS value, but we modify this by adding 0x400000000
		 * for those > ManifestorDesiredBitBufferLevel so that these
		 * will only be selected if there are no buffers at a lower
		 * level. Those with a full bit buffer will never be selected.
		 *
		 * Also to simplify the arithmetic we rotate the wrapping Vdts
		 * values before comparison this avoids complicated wrapping
		 * arithmetic. The rotation is by 1fxxx - (Vpcr - 0x100000000)
		 * or fxxx - Vpcr. */

		MinVdts = 0x800000000ull;
		VdtsRotate = 0xffffffffull - Vpcr;

		for (j = 0; j < Ctx->StreamCount; j++) {
			Stream = Ctx->PackedStreams[j];
			Buffer = Stream->CurrentMultiplexBuffer;

			if (Stream->StreamPes) {
				/* First has any chunk expired from the bit
				 * buffer */
				/* NOTE we process this list even if there are
				 * no more buffers, or we have a placeholder
				 * empty buffer */
				if ((Stream->NextUsedExpiryRecord
					!= Stream->NextFreeExpiryRecord)
					&& InTimePeriod(Stream->BitBufferExpiryPcrs[Stream->NextUsedExpiryRecord
						% NUMBER_OF_BIT_BUFFER_EXPIRY_RECORDS],
							ExpiryBase, Vpcr)) {

					Stream->CurrentBitBufferLevel
						-= Stream->BitBufferExpiryBits[(Stream->NextUsedExpiryRecord++)
						% NUMBER_OF_BIT_BUFFER_EXPIRY_RECORDS];
				}

				/* Loosely figure out if we can fit a packet in, bit wet finger */
				/* in the air use 184 as data size (varies for end packets). */
				/* Wet finger in the air, this matters little */
				NewBitBufferLevel
					= (Stream->CurrentBitBufferLevel
						+ (184 * 8));
				if ((Buffer == NULL)
					|| (Buffer->NumberOfTransportPackets
						== 0)
					|| (NewBitBufferLevel
						> Stream->DecoderBitBufferSize))
					continue;

				if ((Stream->NextFreeExpiryRecord
					- Stream->NextUsedExpiryRecord)
					>= (NUMBER_OF_BIT_BUFFER_EXPIRY_RECORDS
							- 1)) {
					pr_err("MultiplexorPerformMultiplex - All expiry records used on stream (%d:%d).\n",
						Stream->ProgramId,
						Stream->StreamId);
					continue;
				}

				/* Construct out Vdts (don't forget the rotation) */
				Vdts = PcrLimit(VdtsRotate + Buffer->DTS +
						((Buffer->CurrentTransportPacket * Buffer->DTSDuration) /
							Buffer->NumberOfTransportPackets));
				Vdts |= (NewBitBufferLevel <= ManifestorDesiredBitBufferLevel(Stream->DecoderBitBufferSize)) ? 0
					: 0x400000000ull;
			} else if ((Buffer == NULL)
				|| (Buffer->NumberOfTransportPackets
				== 0)) {
				/* Any placeholder zero length buffers are
				 * just ignored. */
				continue;
			} else {
				/* For section content, we do not send before
				 * the DTS is hit, for a rotated dts this is
				 * equivalent to a dts > 0xffffffff. */
				Vdts = PcrLimit(VdtsRotate + Buffer->DTS +
					((Buffer->CurrentTransportPacket *
						Buffer->DTSDuration) /
						Buffer->NumberOfTransportPackets));
				if ((Buffer->CurrentTransportPacket == 0)
					&& (Vdts > 0xffffffffull))
					continue;
			}

			/* Perform the comparison */
			if (Vdts < MinVdts) {
				MinVdts = Vdts;
				MinStream = Stream;
			}
		}

		/* We should now have found the stream for which to output
		 * a packet so we output it. */
		if (MinVdts >= 0x800000000ull || !MinStream) {
			OutputCode(CodeForNullPacket());
		} else {
			/* Do we wish to abdicate this output to a DIT
			 * insertion */

			Buffer = MinStream->CurrentMultiplexBuffer;

			if (Buffer->OutstandingDITRequest) {
				generate_index(Ctx,
					Buffer,
					MinStream->ProgramId,
					MinStream->StreamId,
					STM_TE_TSG_INDEX_DIT);
				OutputCode(CodeForDit(Buffer->DITTransitionFlag));
				Buffer->OutstandingDITRequest = false;
				continue;
			}

			/* Is this the first output from this buffer, do we
			 * need to generate an index record */
			/* First packet */
			if (Buffer->CurrentTransportPacket == 0)
				generate_index(Ctx, Buffer,
					MinStream->ProgramId,
					MinStream->StreamId,
					~(STM_TE_TSG_INDEX_DIT |
					STM_TE_TSG_INDEX_PAT   |
					STM_TE_TSG_INDEX_PMT |
					STM_TE_TSG_INDEX_SDT));
				if (Buffer->OutstandingRAPIndexRequest)
					Buffer->OutstandingRAPIndexRequest =
					false;
			/* Output the packet, increment the appropriate
			 * counters */
			OutputCode(CodeForDataPacket(MinStream->ProgramId,
					MinStream->StreamId));

			Buffer->CurrentTransportPacket++;
			MinStream->CurrentBitBufferLevel += 184 * 8;

			/* Move onto the next buffer ? */
			if (Buffer->CurrentTransportPacket ==
					Buffer->NumberOfTransportPackets) {
				if (MinStream->StreamPes) {
					/* Just check for late delivery. */
					if (!ScheduleFailure
						&& (PcrLimit(VdtsRotate + Buffer->DTS - PcrPacketDuration)
						< 0x100000000ull)) {
						pr_err("MultiplexorPerformMultiplex(%d:%d) - Failed to incorporate buffer in multiplex in time to meet DTS 0x%llx (%llu ticks late).\n",
							Buffer->ProgramId,
							Buffer->StreamId,
							Buffer->DTS,
							(0x100000000ull
							- PcrLimit(VdtsRotate + Buffer->DTS - PcrPacketDuration)));
						pr_err("i=%d Vpcr=0x%llx Vdts=0x%llx VdtsRotate=0x%llx PcrPacketDuration=0x%llx\n",
							i, Vpcr, Vdts,
							VdtsRotate,
							PcrPacketDuration);
						ScheduleFailure = true;
					}

					Index = (MinStream->NextFreeExpiryRecord++)
						% NUMBER_OF_BIT_BUFFER_EXPIRY_RECORDS;
					MinStream->BitBufferExpiryPcrs[Index]
						= Buffer->DTS;
					MinStream->BitBufferExpiryBits[Index]
						= 8 * 184 * Buffer->NumberOfTransportPackets;

					if ((MinStream->NextFreeExpiryRecord
						% NUMBER_OF_BIT_BUFFER_EXPIRY_RECORDS)
						== (MinStream->NextUsedExpiryRecord
						% NUMBER_OF_BIT_BUFFER_EXPIRY_RECORDS))
						pr_crit("MultiplexorPerformMultiplex - All expiry records used - implementation error (should be checked earlier.\n");

					MinStream->CurrentMultiplexBuffer
						= Buffer->NextDataBuffer;
				} else {
					if (Buffer->Repeating) {
						Buffer->CurrentTransportPacket = 0;
						Buffer->DTS = PcrLimit(Buffer->DTS + Buffer->RepeatInterval);
						Buffer->OutstandingDITRequest
							= Buffer->RequestDITInsertion;
					} else {
						MinStream->CurrentMultiplexBuffer
							= Buffer->NextDataBuffer;
					}
				}
			}
		}
	}

	return ScheduleFailure ? MultiplexorDeliveryFailure
		: MultiplexorNoError;
}
