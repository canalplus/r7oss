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

Source file name : multilexor.h
Architects :	   Dan Thomson & Pete Steiglitz

Definition of the multiplexor header file.


Date	Modification				Architects
----	------------				----------
16-Apr-12   Created				 DT & PS

************************************************************************/

#ifndef H_MULTIPLEXOR
#define H_MULTIPLEXOR

#include <linux/mutex.h>
#include <linux/slab.h>
#include "cring.h"

/*
 ---------------------------------------------------------------------
 Locally defined constants
 */
enum {
	MultiplexorNoError = 0,
	MultiplexorError,
	MultiplexorInvalidParameter,
	MultiplexorIdInUse,
	MultiplexorInvalidId,
	MultiplexorAllBuffersInUse,
	MultiplexorAllPagesInUse,
	MultiplexorUnsupportedStreamtype,
	MultiplexorUnrecognisedId,
	MultiplexorInputOverflow,
	MultiplexorRepeatIntervalExceedsDuration,
	MultiplexorOutputOverflow,
	MultiplexorInputUnderflow,
	MultiplexorBitBufferViolation,
	MultiplexorDeliveryFailure,
	MultiplexorDTSIntegrityFailure,
	MultiplexorNoPcrChannelForProgram
};

typedef unsigned int MultiplexorStatus_t;
/* NOTE 16 is a limit that guarantees the SDT will fit in one
 * transport packet */
#define MAXIMUM_NAME_SIZE			16
#define MAXIMUM_DESCRIPTOR_SIZE			16
#define MAXIMUM_INDEX_IDENTIFIER_SIZE		8
#define MAXIMUM_NUMBER_OF_PROGRAMS		4
#define MAXIMUM_NUMBER_OF_STREAMS_PER_PROGRAM	7
#define MAXIMUM_NUMBER_OF_BUFFERS		512
#define MAXIMUM_NUMBER_OF_SCATTER_PAGES		4096
#define SIZE_OF_TRANSPORT_PACKET		188

/* Global flags */
/* Set TS discontinuity indicator for PCR packet
 * like internally generated PCR and Table packets */
#define PCR_DISCONTINUITY_FLAG		0x00000001
/* Set TS discontinuity indicator for Table packets */
#define TABLE_DISCONTINUITY_FLAG	0x00000002

/*
 ---------------------------------------------------------------------
 The structure typedef's early to free up declaration ordering constraints
 */
typedef struct MultiplexorContext_s *MultiplexorContext_t;
typedef struct MultiplexorCapability_s MultiplexorCapability_t;
typedef struct MultiplexorInitializationParameters_s
	MultiplexorInitializationParameters_t;
typedef struct MultiplexorProgramParameters_s MultiplexorProgramParameters_t;
typedef struct MultiplexorStreamParameters_s MultiplexorStreamParameters_t;
typedef struct MultiplexorScatterPage_s MultiplexorScatterPage_t;
typedef struct MultiplexorBufferParameters_s MultiplexorBufferParameters_t;
typedef struct MultiplexorOutputParameters_s MultiplexorOutputParameters_t;
typedef struct MultiplexorIndexRecord_s MultiplexorIndexRecord_t;
typedef struct MultiplexorOutputStatus_s MultiplexorOutputStatus_t;

/*
 ---------------------------------------------------------------------
 Type for the free a buffer callback, plus a couple of predefines to make
 things hang together
 */
typedef void (*MultiplexorFreeBufferCallback_t)(MultiplexorContext_t Context,
	bool Cancelled, MultiplexorBufferParameters_t *Parameters);

/*
 ---------------------------------------------------------------------
 Structure defining the capability information for this multpliexor code
 */
struct MultiplexorCapability_s {
	unsigned int MaxNameSize;
	unsigned int MaxDescriptorSize;

	unsigned int MaximumNumberOfPrograms;
	unsigned int MaximumNumberOfStreamsPerProgram;

	unsigned int MaximumNumberOfBuffers;
	unsigned int MaximumNumberOfScatterPages;
};

/*
 ---------------------------------------------------------------------
 Structure defining the initialization parameters for this multiplexor
 */
struct MultiplexorInitializationParameters_s {
	unsigned long long PcrPeriod;

	bool GeneratePcrStream;
	bool TimeStampedPackets;

	bool GeneratePatPmt;
	bool GenerateSdt;

	bool GenerateIndex;

	bool FixedBitrate;

	unsigned int GlobalFlags;
	unsigned int PcrPid;
	unsigned long long TablePeriod;
	unsigned int TransportStreamId;
	unsigned int Bitrate;

	MultiplexorFreeBufferCallback_t BufferReleaseCallback;

};

/*
 ---------------------------------------------------------------------
 Structure holding a programs parameters
 */
struct MultiplexorProgramParameters_s {
	unsigned int ProgramId;
	/* Sections only - no pat/pmt entry */
	bool TablesProgram;
	/* Pat fields */
	unsigned int ProgramNumber;
	unsigned int PMTPid;
	/* Sdt fields */
	bool StreamsMayBeScrambled;
	char ProviderName[MAXIMUM_NAME_SIZE];
	char ServiceName[MAXIMUM_NAME_SIZE];

	unsigned int OptionalDescriptorSize;
	unsigned char Descriptor[MAXIMUM_DESCRIPTOR_SIZE];

	/* Multiplexor private data */

	bool EntryUsed;

	unsigned char PmtPacket[SIZE_OF_TRANSPORT_PACKET];
};

/*
 ---------------------------------------------------------------------
 Structure holding a streams parameters
 */
#define NUMBER_OF_BIT_BUFFER_EXPIRY_RECORDS	(MAXIMUM_NUMBER_OF_BUFFERS+1)

struct MultiplexorStreamParameters_s {
	unsigned int ProgramId;
	unsigned int StreamId;

	bool IncorporatePcrPacket;
	bool StreamPes;

	unsigned int StreamPid;
	unsigned int StreamType;

	unsigned int OptionalDescriptorSize;
	unsigned char Descriptor[MAXIMUM_DESCRIPTOR_SIZE];

	unsigned long long DTSIntegrityThreshold;
	unsigned int DecoderBitBufferSize;
	unsigned long long MultiplexAheadLimit;

	/* Multiplexor private data */

	bool EntryUsed;

	unsigned long long LastQueuedDTS;
	unsigned char stream_paused;

	MultiplexorBufferParameters_t *DataBuffers;
	MultiplexorBufferParameters_t *CurrentMultiplexBuffer;
	unsigned int ContinuityCount;
	unsigned int CurrentBitBufferLevel;

	unsigned int NextUsedExpiryRecord;
	unsigned int NextFreeExpiryRecord;
	unsigned long long *BitBufferExpiryPcrs;
	unsigned int *BitBufferExpiryBits;
};

/*
 ---------------------------------------------------------------------
 Structure holding a page descripto
 */
struct MultiplexorScatterPage_s {
	MultiplexorScatterPage_t *Next;
	unsigned int Size;
	unsigned char *Base;
};

/*
 ---------------------------------------------------------------------
 Structure holding a buffer descriptor
 */
struct MultiplexorBufferParameters_s {
	unsigned int ProgramId;
	unsigned int StreamId;
	unsigned int BufferId;

	unsigned int Size;
	unsigned int TotalUsedSize;
	unsigned int PageCount;
	MultiplexorScatterPage_t *Pages;

	unsigned long long DTS;
	unsigned long long DTSDuration;

	bool Discontinuity;
	bool Scrambled;
	bool Parity;
	bool Repeating;
	bool RequestDITInsertion;
	bool RequestRAPBit;

	unsigned long long RepeatInterval;

	unsigned int DITTransitionFlag;

	unsigned int IndexSize;
	unsigned char IndexData[MAXIMUM_INDEX_IDENTIFIER_SIZE];

	void *UserParameters;
	void *UserData;

	/* Multiplexor private data */

	bool OutstandingDITRequest;
	bool OutstandingRAPRequest;
	bool OutstandingRAPIndexRequest;

	unsigned int NumberOfTransportPackets;
	unsigned int CurrentTransportPacket;

	MultiplexorBufferParameters_t *NextDataBuffer;
	MultiplexorScatterPage_t *CurrentPage;

	unsigned int RemainingBufferSize;
	unsigned int RemainingPageSize;
	unsigned char *PagePointer;
};

/*
 ---------------------------------------------------------------------
 Structure holding the output parameters for single output pass
 */
struct MultiplexorOutputParameters_s {
	bool Flush;

	MultiplexorBufferParameters_t *OutputBuffer;
};

/*
 ---------------------------------------------------------------------
 Structure holding an index record
 */
struct MultiplexorIndexRecord_s {
	unsigned int PacketOffset;
	unsigned int Program;
	unsigned int Stream;
	unsigned int IdentifierSize;
	unsigned char Identifier[MAXIMUM_INDEX_IDENTIFIER_SIZE];
	unsigned long long pts;
};

/*
 ---------------------------------------------------------------------
 Structure holding the output parameters for single output pass
 */
struct MultiplexorOutputStatus_s {
	unsigned int OverflowOutputSize;
	unsigned int UnderflowProgram;
	unsigned int UnderflowStream;

	bool NonOutputDataRemains;

	unsigned long long PCR;
	unsigned long long OffsetFromFirstOutput;
	unsigned long long OutputDuration;
	unsigned int Bitrate;

	unsigned int
		DecoderBitBufferLevels[MAXIMUM_NUMBER_OF_PROGRAMS][MAXIMUM_NUMBER_OF_STREAMS_PER_PROGRAM];

	unsigned int IndexCount;
	MultiplexorIndexRecord_t Index[MAXIMUM_NUMBER_OF_BUFFERS];
	unsigned int CompletedBufferCount;
	unsigned int CompletedBuffers[MAXIMUM_NUMBER_OF_BUFFERS];
	unsigned int OutputTSPackets;
};

/*
 ---------------------------------------------------------------------
 Context structure for an open multiplexor
 */
struct MultiplexorContext_s {

	/* Record of the various entities given to us */

	MultiplexorInitializationParameters_t Settings;

	MultiplexorProgramParameters_t Programs[MAXIMUM_NUMBER_OF_PROGRAMS];
	MultiplexorStreamParameters_t
		Streams[MAXIMUM_NUMBER_OF_PROGRAMS][MAXIMUM_NUMBER_OF_STREAMS_PER_PROGRAM];

	unsigned int PMTCount;
	unsigned int StreamCount;
	MultiplexorStreamParameters_t *PackedStreams[MAXIMUM_NUMBER_OF_PROGRAMS
		* MAXIMUM_NUMBER_OF_STREAMS_PER_PROGRAM];

	/* Various state variables */

	struct mutex Lock;
	struct mutex FreeLock;

	bool Flushing;
	bool RegenerateTables;

	bool PcrInitialized;
	unsigned int TotalPacketCount;
	unsigned long long InitialPcr;
	unsigned long long ShiftPCR;
	unsigned long long PcrOffset;
	unsigned int PatPmtCountThisPeriod;

	MultiplexorScatterPage_t *ScatterPages;
	CRing_t *FreeScatterPages;

	MultiplexorBufferParameters_t *Buffers;
	CRing_t *FreeBuffers;

	unsigned int BitsPerPacket;

	unsigned char PcrPacket[SIZE_OF_TRANSPORT_PACKET];
	unsigned char PatPacket[SIZE_OF_TRANSPORT_PACKET];
	unsigned char SdtPacket[SIZE_OF_TRANSPORT_PACKET];
	unsigned char DitPacket[SIZE_OF_TRANSPORT_PACKET];
	/* Count of index records in the output status */
	unsigned int IndexCount;
	MultiplexorBufferParameters_t *OutputBuffer;
	MultiplexorOutputStatus_t *OutputStatus;
	/* Calculated value of how many packets to output */
	unsigned int OutputPackets;
	unsigned int Bitrate;

	unsigned int OutputPacketAllocation;
	unsigned int OutputPacketCount;
	/* See private header file for bit pattern */
	unsigned char *OutputPacketDescriptors;
};

/*
 ---------------------------------------------------------------------
 Function declarations
 */
MultiplexorStatus_t MultiplexorGetCapabilities(
	MultiplexorCapability_t *Capabilities);

MultiplexorStatus_t MultiplexorOpen(MultiplexorContext_t *Context,
	MultiplexorInitializationParameters_t *Parameters);

MultiplexorStatus_t MultiplexorClose(MultiplexorContext_t Context);

MultiplexorStatus_t MultiplexorAddProgram(MultiplexorContext_t Context,
	MultiplexorProgramParameters_t *Parameters);

MultiplexorStatus_t MultiplexorRemoveProgram(MultiplexorContext_t Context,
	MultiplexorProgramParameters_t *Parameters);

MultiplexorStatus_t MultiplexorAddStream(MultiplexorContext_t Context,
	MultiplexorStreamParameters_t *Parameters);

MultiplexorStatus_t MultiplexorRemoveStream(MultiplexorContext_t Context,
	MultiplexorStreamParameters_t *Parameters);

MultiplexorStatus_t MultiplexorGetBufferStructure(MultiplexorContext_t Context,
	unsigned int PageCount, MultiplexorBufferParameters_t **Parameters);

MultiplexorStatus_t
	MultiplexorFreeBufferStructure(MultiplexorContext_t Context,
		MultiplexorBufferParameters_t *Parameters);

MultiplexorStatus_t MultiplexorSupplyBuffer(MultiplexorContext_t Context,
	MultiplexorBufferParameters_t *Parameters);

MultiplexorStatus_t MultiplexorCancelBuffer(MultiplexorContext_t Context,
	unsigned int BufferId);

MultiplexorStatus_t MultiplexorPrepareOutput(MultiplexorContext_t Context,
	MultiplexorOutputParameters_t *Parameters,
	MultiplexorOutputStatus_t *StatusInfo);

MultiplexorStatus_t MultiplexorPerformOutput(MultiplexorContext_t Context);

#endif

