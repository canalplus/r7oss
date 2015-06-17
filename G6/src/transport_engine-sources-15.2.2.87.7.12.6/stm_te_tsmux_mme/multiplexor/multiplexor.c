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
17-Apr-12   Created                              DT & PS

************************************************************************/
#include <stm_te_dbg.h>

#include "multiplexor.h"
#include "multiplexor_private.h"

static unsigned char	InitialPcrPacket[SIZE_OF_TRANSPORT_PACKET] = {
	0x47, 0x1f, 0xfe, 0x20,
	0xB7, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
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

/*
 ---------------------------------------------------------------------

 PUBLIC - Function to fill out a capability structure

 */

MultiplexorStatus_t MultiplexorGetCapabilities(
	MultiplexorCapability_t *Capabilities)
{
	memset(Capabilities, 0x00, sizeof(MultiplexorCapability_t));

	Capabilities->MaxNameSize = MAXIMUM_NAME_SIZE;
	Capabilities->MaxDescriptorSize = MAXIMUM_DESCRIPTOR_SIZE;
	Capabilities->MaximumNumberOfPrograms = MAXIMUM_NUMBER_OF_PROGRAMS;
	Capabilities->MaximumNumberOfStreamsPerProgram
		= MAXIMUM_NUMBER_OF_STREAMS_PER_PROGRAM;
	Capabilities->MaximumNumberOfBuffers = MAXIMUM_NUMBER_OF_BUFFERS;
	Capabilities->MaximumNumberOfScatterPages
		= MAXIMUM_NUMBER_OF_SCATTER_PAGES;

	return MultiplexorNoError;
}

/*
 ---------------------------------------------------------------------

 PUBLIC - Function to get me a context and initialize it

 */

MultiplexorStatus_t MultiplexorOpen(MultiplexorContext_t *Context,
	MultiplexorInitializationParameters_t *Parameters)
{
	unsigned int i;
	CRingStatus_t RingStatus;
	MultiplexorContext_t NewContext;
	unsigned int BitsPerPcrPeriod;
	unsigned int BitsPerPacket;
	unsigned int OutputPacketAllocation;
	unsigned char *OutputPacketDescriptors;
	MultiplexorScatterPage_t *Pages;
	MultiplexorBufferParameters_t *Buffers;
	CRing_t *PageRing;
	CRing_t *BufferRing;

	/* Validate parameters */

	ParameterRangeCheck(Parameters->PcrPeriod, 900, 9000,
		"MultiplexorOpen - Invalid Pcr Period.");
	ParameterRangeCheck(Parameters->Bitrate, (100*1024), (60*1024*1024),
		"MultiplexorOpen - Invalid bitrate.");
	ParameterRangeCheck(Parameters->TransportStreamId, 0, 0xffff,
		"MultiplexorOpen - Invalid transport stream id.");

	ParameterAssert((!Parameters->GenerateSdt || Parameters->GeneratePatPmt),
		"MultiplexorOpen - Invalid flag combination generate Sdt, but not Pat/Pmt");
	ParameterAssert((Parameters->BufferReleaseCallback != NULL),
		"MultiplexorOpen - No buffer release callback specified.");

	if (Parameters->GeneratePcrStream)
		ParameterRangeCheck(Parameters->PcrPid, 0, 0x1ffe,
				"MultiplexorOpen - Invalid Pcr Pid.");

	/* Get a context */
	*Context = NULL;
	NewContext = kzalloc(sizeof(struct MultiplexorContext_s), GFP_KERNEL);

	if (NewContext == NULL) {
		pr_err("MultiplexorOpen - Failed to obtain memory for context.\n");
		return MultiplexorError;
	}

	/* Garner the memory for the copy descriptors */

	BitsPerPcrPeriod = (Parameters->Bitrate * Parameters->PcrPeriod)
		/ 90000;
	BitsPerPacket = 8 * (Parameters->TimeStampedPackets ? 192 : 188);
	/* Round to a word with at least 1 spare */
	OutputPacketAllocation = ((BitsPerPcrPeriod / BitsPerPacket) + 4)
		& 0xfffffffc;
	OutputPacketDescriptors = kzalloc(OutputPacketAllocation, GFP_KERNEL);

	if (OutputPacketDescriptors == NULL) {
		pr_err("MultiplexorOpen - Failed to obtain memory for output copy descriptors.\n");
		kfree(NewContext);
		return MultiplexorError;
	}

	/* Get the scatter pages */
	Pages = kzalloc(
		MAXIMUM_NUMBER_OF_SCATTER_PAGES
		* sizeof(MultiplexorScatterPage_t), GFP_KERNEL);
	if (Pages == NULL) {
		pr_err("MultiplexorOpen - Failed to obtain memory for scatter page structures.\n");
		kfree(OutputPacketDescriptors);
		kfree(NewContext);
		return MultiplexorError;
	}

	RingStatus = CRingNew(&PageRing, MAXIMUM_NUMBER_OF_SCATTER_PAGES);
	if (RingStatus != CRingNoError) {
		pr_err("MultiplexorOpen - Failed to obtain storage ring for scatter page structures.\n");
		kfree(Pages);
		kfree(OutputPacketDescriptors);
		kfree(NewContext);
		return MultiplexorError;
	}

	for (i = 0; i < MAXIMUM_NUMBER_OF_SCATTER_PAGES; i++) {
		RingStatus = CRingInsert(PageRing, &Pages[i]);
		if (RingStatus != CRingNoError)
			pr_crit("MultiplexorOpen - Fatal error inserting page on storage ring (%d)\n",
				i);
	}

	/* Get the buffer structures */

	Buffers = kzalloc(MAXIMUM_NUMBER_OF_BUFFERS *
			sizeof(MultiplexorBufferParameters_t), GFP_KERNEL);
	if (Buffers == NULL) {
		pr_err("MultiplexorOpen - Failed to obtain memory for buffer structures.\n");
		CRingDestroy(PageRing);
		kfree(Pages);
		kfree(OutputPacketDescriptors);
		kfree(NewContext);
		return MultiplexorError;
	}

	RingStatus = CRingNew(&BufferRing, MAXIMUM_NUMBER_OF_BUFFERS);
	if (RingStatus != CRingNoError) {
		pr_err("MultiplexorOpen - Failed to obtain storage ring for buffer structures.\n");
		kfree(Buffers);
		CRingDestroy(PageRing);
		kfree(Pages);
		kfree(OutputPacketDescriptors);
		kfree(NewContext);
		return MultiplexorError;
	}

	for (i = 0; i < MAXIMUM_NUMBER_OF_BUFFERS; i++) {
		RingStatus = CRingInsert(BufferRing, &Buffers[i]);
		if (RingStatus != CRingNoError)
			pr_crit("MultiplexorOpen - Fatal error inserting buffer on storage ring (%d)\n",
				i);
	}

	/* Fill out the context */
	memset(NewContext, 0x00, sizeof(struct MultiplexorContext_s));

	memcpy(&NewContext->Settings, Parameters,
		sizeof(MultiplexorInitializationParameters_t));
	memcpy(&NewContext->PcrPacket, InitialPcrPacket,
		SIZE_OF_TRANSPORT_PACKET);

	mutex_init(&NewContext->Lock);
	mutex_init(&NewContext->FreeLock);

	NewContext->RegenerateTables = true;
	NewContext->PcrInitialized = false;
	NewContext->ScatterPages = Pages;
	NewContext->FreeScatterPages = PageRing;
	NewContext->Buffers = Buffers;
	NewContext->FreeBuffers = BufferRing;

	NewContext->BitsPerPacket = BitsPerPacket;

	NewContext->OutputPacketAllocation = OutputPacketAllocation;
	NewContext->OutputPacketCount = 0;
	NewContext->OutputPacketDescriptors = OutputPacketDescriptors;

	*Context = NewContext;

	return MultiplexorNoError;
}

/*
 ---------------------------------------------------------------------

 PUBLIC - Function to close a multiplex, destroying the context

 */

MultiplexorStatus_t MultiplexorClose(MultiplexorContext_t Context)
{
	unsigned int i;
	void *Dummy;

	/* Any programs ? */
	for (i = 0; i < MAXIMUM_NUMBER_OF_PROGRAMS; i++)
		if (Context->Programs[i].EntryUsed)
			MultiplexorRemoveProgram(Context, &Context->Programs[i]);

	/* Release the buffer structures */
	while (CringNonEmpty(Context->FreeBuffers))
		CRingExtract(Context->FreeBuffers, &Dummy);

	CRingDestroy(Context->FreeBuffers);

	kfree(Context->Buffers);

	/* Release the page structures */
	while (CringNonEmpty(Context->FreeScatterPages))
		CRingExtract(Context->FreeScatterPages, &Dummy);

	CRingDestroy(Context->FreeScatterPages);

	kfree(Context->ScatterPages);

	/* Release the packet descriptors */
	kfree(Context->OutputPacketDescriptors);

	/* Check mutex locks before freeing */
	WARN_ON(mutex_is_locked(&Context->FreeLock));
	WARN_ON(mutex_is_locked(&Context->Lock));

	/* Finally let the context go */
	kfree(Context);

	return MultiplexorNoError;
}

/*
 ---------------------------------------------------------------------

 PUBLIC - Function to add a program into the mix

 */
MultiplexorStatus_t MultiplexorAddProgram(MultiplexorContext_t Context,
	MultiplexorProgramParameters_t *Parameters)
{
	/* Validate parameters */
	ParameterRangeCheck(Parameters->ProgramId, 0,
			(MAXIMUM_NUMBER_OF_PROGRAMS-1),
			"MultiplexorAddProgram - Invalid Program Id.");
	ParameterRangeCheck(Parameters->ProgramNumber, 0, 0xffff,
		"MultiplexorAddProgram - Invalid Program Number.");
	ParameterRangeCheck(Parameters->PMTPid, 1, 0x1ffe,
		"MultiplexorAddProgram - Invalid PMT Pid.");
	ParameterRangeCheck(Parameters->OptionalDescriptorSize, 0,
		MAXIMUM_DESCRIPTOR_SIZE,
		"MultiplexorAddProgram - OptionalDescriptorSize too large.");

	/* Simple checks */
	if (mutex_lock_interruptible(&Context->Lock) != 0)
		return MultiplexorError;

	if (Context->Programs[Parameters->ProgramId].EntryUsed) {
		pr_err("MultiplexorAddProgram - ProgramId already in use.\n");
		mutex_unlock(&Context->Lock);
		return MultiplexorIdInUse;
	}

	/* Copy in and inform the transform it needs to regenerate tables */
	memcpy(&Context->Programs[Parameters->ProgramId], Parameters,
		sizeof(MultiplexorProgramParameters_t));

	Context->Programs[Parameters->ProgramId].EntryUsed = true;
	Context->RegenerateTables = true;

	mutex_unlock(&Context->Lock);

	return MultiplexorNoError;
}

/*
 ---------------------------------------------------------------------

 PUBLIC - Function to remove a program from the mix

 */
MultiplexorStatus_t MultiplexorRemoveProgram(MultiplexorContext_t Context,
	MultiplexorProgramParameters_t *Parameters)
{
	unsigned int i;

	/* Validate parameters */
	ParameterRangeCheck(Parameters->ProgramId, 0,
		(MAXIMUM_NUMBER_OF_PROGRAMS-1),
		"MultiplexorRemoveProgram - Invalid Program Id.");

	/* Simple checks */
	if (mutex_lock_interruptible(&Context->Lock) != 0)
		return MultiplexorError;

	if (!Context->Programs[Parameters->ProgramId].EntryUsed) {
		pr_err("MultiplexorRemoveProgram - ProgramId not in use (%d).\n",
			Parameters->ProgramId);
		mutex_unlock(&Context->Lock);
		return MultiplexorUnrecognisedId;
	}

	/* Any streams ? */
	for (i = 0; i < MAXIMUM_NUMBER_OF_STREAMS_PER_PROGRAM; i++)
		if (Context->Streams[Parameters->ProgramId][i].EntryUsed) {
			mutex_unlock(&Context->Lock);
			MultiplexorRemoveStream(
				Context,
				&Context->Streams[Parameters->ProgramId][i]);
			if (mutex_lock_interruptible(&Context->Lock) != 0)
				return MultiplexorError;
		}

	/* Unuse the program and force re-calculation of the tables */
	Context->Programs[Parameters->ProgramId].EntryUsed = false;
	Context->RegenerateTables = true;

	mutex_unlock(&Context->Lock);

	return MultiplexorNoError;
}

/*
 ---------------------------------------------------------------------

 PUBLIC - Function to add a stream onto a program

 */
MultiplexorStatus_t MultiplexorAddStream(MultiplexorContext_t Context,
	MultiplexorStreamParameters_t *Parameters)
{
	/* Validate parameters */
	ParameterRangeCheck(Parameters->ProgramId, 0,
			(MAXIMUM_NUMBER_OF_PROGRAMS-1),
			"MultiplexorAddStream - Invalid Program Id.");
	ParameterRangeCheck(Parameters->StreamId, 0,
		(MAXIMUM_NUMBER_OF_STREAMS_PER_PROGRAM-1),
		"MultiplexorAddStream - Invalid Stream Id.");
	ParameterRangeCheck(Parameters->StreamPid, 0, 0x1ffe,
		"MultiplexorAddStream - Invalid Pid.");
	ParameterRangeCheck(Parameters->OptionalDescriptorSize,
		0, MAXIMUM_DESCRIPTOR_SIZE,
		"MultiplexorAddStream - OptionalDescriptorSize too large.");
	ParameterRangeCheck(Parameters->DecoderBitBufferSize, 0,
		(64*1024*1024),
		"MultiplexorAddStream - Decoder bit buffer ridiculously large.");

	/* Simple checks */
	if (mutex_lock_interruptible(&Context->Lock) != 0)
		return MultiplexorError;

	if (!Context->Programs[Parameters->ProgramId].EntryUsed) {
		pr_err("MultiplexorAddStream - ProgramId not in use (%d).\n",
			Parameters->ProgramId);
		mutex_unlock(&Context->Lock);
		return MultiplexorUnrecognisedId;
	}

	if (Context->Streams[Parameters->ProgramId][Parameters->StreamId].EntryUsed) {
		pr_err("MultiplexorAddStream - StreamId already in use.\n");
		mutex_unlock(&Context->Lock);
		return MultiplexorIdInUse;
	}

	/* Copy in and inform the transform it needs to regenerate tables */
	memcpy(&Context->Streams[Parameters->ProgramId][Parameters->StreamId],
		Parameters, sizeof(MultiplexorStreamParameters_t));

	Context->Streams[Parameters->ProgramId][Parameters->StreamId].LastQueuedDTS
		= INVALID_TIME;

	Context->Streams[Parameters->ProgramId][Parameters->StreamId].EntryUsed
		= true;
	Context->RegenerateTables = true;

	/* obtain memory for the bit buffer expiry records */
	Context->Streams[Parameters->ProgramId][Parameters->StreamId].BitBufferExpiryPcrs
		= kzalloc(NUMBER_OF_BIT_BUFFER_EXPIRY_RECORDS *
				sizeof(unsigned long long), GFP_KERNEL);
	Context->Streams[Parameters->ProgramId][Parameters->StreamId].BitBufferExpiryBits
		= kzalloc(NUMBER_OF_BIT_BUFFER_EXPIRY_RECORDS *
				sizeof(unsigned int), GFP_KERNEL);

	if ((Context->Streams[Parameters->ProgramId][Parameters->StreamId].BitBufferExpiryPcrs
			== NULL)
		|| (Context->Streams[Parameters->ProgramId][Parameters->StreamId].BitBufferExpiryBits
				== NULL)) {
		pr_err("MultiplexorAddStream - No memory for Bit Buffer Expiry records.\n");
		if (Context->Streams[Parameters->ProgramId][Parameters->StreamId].BitBufferExpiryPcrs
				!= NULL)
			kfree(Context->Streams[Parameters->ProgramId][Parameters->StreamId].BitBufferExpiryPcrs);
		mutex_unlock(&Context->Lock);
		return MultiplexorError;
	}

	mutex_unlock(&Context->Lock);

	return MultiplexorNoError;
}

/*
 ---------------------------------------------------------------------

 PUBLIC - Function to remove a stream from a program

 */
MultiplexorStatus_t MultiplexorRemoveStream(MultiplexorContext_t Context,
	MultiplexorStreamParameters_t *Parameters)
{
	unsigned int BufferId;
	MultiplexorStreamParameters_t *Stream;

	/* Validate parameters */
	ParameterRangeCheck(Parameters->ProgramId, 0,
		(MAXIMUM_NUMBER_OF_PROGRAMS-1),
		"MultiplexorRemoveStream - Invalid Program Id.");
	ParameterRangeCheck(Parameters->StreamId, 0,
		(MAXIMUM_NUMBER_OF_STREAMS_PER_PROGRAM-1),
		"MultiplexorRemoveStream - Invalid Stream Id.");

	/* Simple checks */
	if (mutex_lock_interruptible(&Context->Lock) != 0)
		return MultiplexorError;

	Stream = &Context->Streams[Parameters->ProgramId][Parameters->StreamId];

	if (!Stream->EntryUsed) {
		pr_err("MultiplexorRemoveStream - Stream not in use (%d, %d).\n",
			Parameters->ProgramId, Parameters->StreamId);
		mutex_unlock(&Context->Lock);
		return MultiplexorUnrecognisedId;
	}

	/* If there are any data buffers attached, then cancel them. */
	while (Stream->DataBuffers != NULL) {
		/* Take before losing lock just in case */
		BufferId = Stream->DataBuffers->BufferId;
		mutex_unlock(&Context->Lock);
		MultiplexorCancelBuffer(Context, BufferId);
		if (mutex_lock_interruptible(&Context->Lock) != 0)
			return MultiplexorError;
	}

	/* Free the bit buffer expiry records */
	kfree(Stream->BitBufferExpiryPcrs);
	kfree(Stream->BitBufferExpiryBits);

	/* Let the stream go, and force re-calculation of tables. */
	Stream->EntryUsed = false;
	Context->RegenerateTables = true;

	mutex_unlock(&Context->Lock);

	return MultiplexorNoError;
}

/*
 ---------------------------------------------------------------------

 PUBLIC - Function to get a buffer descriptor

 */
MultiplexorStatus_t MultiplexorGetBufferStructure(MultiplexorContext_t Context,
	unsigned int PageCount, MultiplexorBufferParameters_t **Parameters)
{
	CRingStatus_t Status;
	MultiplexorBufferParameters_t *Buffer;
	MultiplexorScatterPage_t *Page;

	/* Validate parameters */
	ParameterRangeCheck(PageCount, 0, MAXIMUM_NUMBER_OF_SCATTER_PAGES/4,
		"MultiplexorGetBufferStructure - Requesting too many page structures.");

	/* Get me a buffer structure */
	*Parameters = NULL;

	if (mutex_lock_interruptible(&Context->Lock) != 0)
		return MultiplexorError;

	Status = CRingExtract(Context->FreeBuffers, (void **) &Buffer);
	if (Status != CRingNoError) {
		pr_err("MultiplexorGetBufferStructure - No free buffer structures.\n");
		mutex_unlock(&Context->Lock);
		return MultiplexorAllBuffersInUse;
	}

	memset(Buffer, 0x00, sizeof(MultiplexorBufferParameters_t));

	Buffer->BufferId = Buffer - Context->Buffers;

	/* Get and add the pages we wish to use. */
	for (Buffer->PageCount = 0; Buffer->PageCount < PageCount; Buffer->PageCount++) {
		Status = CRingExtract(Context->FreeScatterPages,
			(void **) &Page);
		if (Status != CRingNoError) {
			pr_err("MultiplexorGetBufferStructure - Failed to get scatter page structures.\n");

			while (Buffer->Pages != NULL) {
				Page = Buffer->Pages;
				Buffer->Pages = Page->Next;
				CRingInsert(Context->FreeScatterPages, Page);
			}

			CRingInsert(Context->FreeBuffers, Buffer);

			mutex_unlock(&Context->Lock);
			return MultiplexorAllPagesInUse;
		}

		Page->Size = 0;
		Page->Base = NULL;

		Page->Next = Buffer->Pages;
		Buffer->Pages = Page;
	}

	*Parameters = Buffer;

	mutex_unlock(&Context->Lock);

	return MultiplexorNoError;
}

/*
 ---------------------------------------------------------------------

 PUBLIC - Function to free up a buffer structure

 NOTE here we take the much less restrictive free lock. This is because free
 can be called by the transform (indirectly), while already holding the main lock.

 */
MultiplexorStatus_t MultiplexorFreeBufferStructure(
	MultiplexorContext_t Context, MultiplexorBufferParameters_t *Parameters)
{
	CRingStatus_t Status;
	MultiplexorScatterPage_t *Page;

	if (mutex_lock_interruptible(&Context->FreeLock) != 0)
		return MultiplexorError;

	/* Free up the pages */
	while (Parameters->Pages != NULL) {
		Page = Parameters->Pages;
		Parameters->Pages = Page->Next;
		Status = CRingInsert(Context->FreeScatterPages, Page);
		if (Status != CRingNoError)
			pr_crit("MultiplexorFreeBufferStructure - Overflow on page storage ring.\n");
	}

	/* Free up the buffer */
	Status = CRingInsert(Context->FreeBuffers, Parameters);
	if (Status != CRingNoError)
		pr_crit("MultiplexorFreeBufferStructure - Overflow on buffer storage ring.\n");

	mutex_unlock(&Context->FreeLock);

	return MultiplexorNoError;
}

static bool stream_is_paused(MultiplexorStreamParameters_t *Stream)
{

	if (Stream->stream_paused ||
		(Stream->DataBuffers != NULL && Stream->DataBuffers->Size == 0))
		return true;
	else
		return false;
}

/*
 ---------------------------------------------------------------------

 PUBLIC - Function to supply a buffer

 */
MultiplexorStatus_t MultiplexorSupplyBuffer(MultiplexorContext_t Context,
	MultiplexorBufferParameters_t *Parameters)
{
	unsigned int i, j;
	MultiplexorStreamParameters_t *Stream;
	MultiplexorStreamParameters_t *OtherStream;
	MultiplexorBufferParameters_t **Pointer;
	MultiplexorBufferParameters_t *Buffer;
	unsigned int UnPaddedSize;
	unsigned long long Threshold;

	/* Validate parameters */
	ParameterRangeCheck(Parameters->ProgramId, 0,
		(MAXIMUM_NUMBER_OF_PROGRAMS-1),
		"MultiplexorSupplyBuffer - Invalid Program Id.");
	ParameterRangeCheck(Parameters->StreamId, 0,
		(MAXIMUM_NUMBER_OF_STREAMS_PER_PROGRAM-1),
		"MultiplexorSupplyBuffer - Invalid Stream Id.");
	ParameterRangeCheck(Parameters->BufferId, 0,
		(MAXIMUM_NUMBER_OF_BUFFERS-1),
		"MultiplexorSupplyBuffer - Invalid Buffer Id - should be as supplied in MultiplexorGetBufferStructure.");
	ParameterRangeCheck(Parameters->Size, 0, (8*1024*1024),
		"MultiplexorSupplyBuffer - Ridiculously large buffer.");
	ParameterAssert((Parameters->TotalUsedSize <= Parameters->Size),
		"MultiplexorSupplyBuffer - TotalUsedSize cannot greater than Size.");
	ParameterRangeCheck(Parameters->PageCount, 0,
		MAXIMUM_NUMBER_OF_SCATTER_PAGES/4,
		"MultiplexorSupplyBuffer - Too many page structures.");
	ParameterRangeCheck(Parameters->DTS, 0, 0x1ffffffffull,
		"MultiplexorSupplyBuffer - Invalid DTS.");
	ParameterRangeCheck(Parameters->DTSDuration, 0, (100*90000),
		"MultiplexorSupplyBuffer - Ridiculously long duration.");
	ParameterRangeCheck(Parameters->RepeatInterval, 0, (10*90000),
		"MultiplexorSupplyBuffer - Ridiculously long repeat interval.");
	ParameterRangeCheck(Parameters->DITTransitionFlag, 0, 1,
		"MultiplexorSupplyBuffer - Dit transition flag is not 0 or 1.");
	ParameterRangeCheck(Parameters->IndexSize, 0,
		MAXIMUM_INDEX_IDENTIFIER_SIZE,
		"MultiplexorSupplyBuffer - IndexSize too large.");

	/* Checks leading to extracting the stream pointer */
	if (mutex_lock_interruptible(&Context->Lock) != 0)
		return MultiplexorError;

	Stream = &Context->Streams[Parameters->ProgramId][Parameters->StreamId];
	if (!Stream->EntryUsed) {
		pr_err("MultiplexorSupplyBuffer - Stream not in use (%d, %d).\n",
			Parameters->ProgramId, Parameters->StreamId);
		mutex_unlock(&Context->Lock);
		return MultiplexorUnrecognisedId;
	}

	if (!Stream->StreamPes && (Parameters->TotalUsedSize > (4 * 1024))) {
		pr_err("MultiplexorSupplyBuffer - Section buffer larger than 4k in size (%d).\n",
			Parameters->TotalUsedSize);
		mutex_unlock(&Context->Lock);
		return MultiplexorInvalidParameter;
	}

	/* Perform the DTS integrity check */
	if ((Stream->DTSIntegrityThreshold != 0) && (Parameters->Size != 0)
			&& !Parameters->Repeating) {
		/* If this is not the very first buffer, and the stream is not
		 * currently paused i.e. a zero size last buffer */
		if (Stream->LastQueuedDTS != INVALID_TIME) {
			if (!stream_is_paused(Stream) &&
				(!InTimePeriod(Parameters->DTS,
				Stream->LastQueuedDTS,
				PcrLimit(Stream->LastQueuedDTS
					+ Stream->DTSIntegrityThreshold)))) {

				pr_err("MultiplexorSupplyBuffer - Fail DTS Integrity check stream=%d (Last %010llx, This %010llx).\n",
					Parameters->StreamId,
					Stream->LastQueuedDTS,
					Parameters->DTS);
				mutex_unlock(&Context->Lock);
				return MultiplexorDTSIntegrityFailure;
			}
		} else {
			for (i = 0; i < MAXIMUM_NUMBER_OF_PROGRAMS; i++)
				if (Context->Programs[i].EntryUsed)
					for (j = 0; j < MAXIMUM_NUMBER_OF_STREAMS_PER_PROGRAM; j++)
						if (Context->Streams[i][j].EntryUsed) {
							OtherStream = &Context->Streams[i][j];
							Threshold = max(
									Stream->DTSIntegrityThreshold,
									OtherStream->DTSIntegrityThreshold);

							if ((OtherStream->DataBuffers != NULL) &&
								(OtherStream->DataBuffers->Size	!= 0) &&
								!OtherStream->DataBuffers->Repeating &&
								(OtherStream->StreamPes ||
									(!OtherStream->StreamPes &&
										OtherStream->DataBuffers->DTS != 0)) &&
								!InTimePeriod(Parameters->DTS,
									PcrLimit(OtherStream->DataBuffers->DTS - Threshold),
									PcrLimit(OtherStream->DataBuffers->DTS + Threshold))) {
								pr_warn("MultiplexorSupplyBuffer - Fail DTS Integrity check stream=%d (Other %010llx, This %010llx).\n",
									Parameters->StreamId,
									OtherStream->DataBuffers->DTS,
									Parameters->DTS);
							}
						}
		}
	}

	/* Calculate the number of transport stream packets that this buffer will occupy */
	/* If RAP is set then increase unpadded size by 2 */
	if (Parameters->Size != 0) {
		UnPaddedSize = Parameters->Size
			+ ((Parameters->Discontinuity ||
				Parameters->RequestRAPBit)
				? 2 : 0)
			+ (Stream->StreamPes ? 0 : 1);
		if (Parameters->RequestRAPBit
				&& Stream->IncorporatePcrPacket)
			UnPaddedSize += DVB_PCR_AF_SIZE;
		Parameters->NumberOfTransportPackets = (UnPaddedSize
			+ DVB_MAX_PAYLOAD_SIZE - 1)
			/ DVB_MAX_PAYLOAD_SIZE;
	} else {
		pr_debug("Received pause buffer on stream=%d\n",
				Parameters->StreamId);
		Parameters->NumberOfTransportPackets = 0;
	}

	Parameters->CurrentTransportPacket = 0;

	if (Stream->StreamPes &&
		((Parameters->NumberOfTransportPackets * 8 * 184) >
			Stream->DecoderBitBufferSize)) {
		pr_err("MultiplexorSupplyBuffer - Buffer larger than decoder bit buffer, impossible to multiplex (%d).\n",
			Parameters->Size);

		mutex_unlock(&Context->Lock);
		return MultiplexorBitBufferViolation;
	}

	/* For PES or non-repeating sections just append the buffer to the end of the buffer list */
	if(!Stream->StreamPes && !Parameters->Repeating)
	{
		Pointer = &Stream->DataBuffers;
		*Pointer = Parameters;
	}
	else if (Stream->StreamPes) {
		for (Pointer = &Stream->DataBuffers;
			(*Pointer != NULL) && ((*Pointer)->NumberOfTransportPackets != 0);
				Pointer = &((*Pointer)->NextDataBuffer));

		/* This cleans out any empty buffer as the last entry
		 * NOTE only the last can be empty due to this code :) */
		if (*Pointer != NULL) {
			pr_debug("Clearing pause buffer on stream=%d\n",
					Parameters->StreamId);
			Context->Settings.BufferReleaseCallback(Context, false,
				*Pointer);
		}

		*Pointer = Parameters;
	}
	/* For repeating section, or an established repeating section
	 * we release any previous buffers, and possibly initialise the DTS value
	 */
	else {
		while (Stream->DataBuffers != NULL) {
			Buffer = Stream->DataBuffers;
			Stream->DataBuffers = Buffer->NextDataBuffer;

			Context->Settings.BufferReleaseCallback(Context, false,
				Buffer);
		}

		Stream->DataBuffers = Parameters;

		if (Parameters->Repeating)
			Parameters->DTS
				= Context->PcrInitialized ? PcrLimit(Context->InitialPcr + Context->PcrOffset)
								: 0;
	}

	/* record the DIT insertion flag */
	Parameters->OutstandingDITRequest = Parameters->RequestDITInsertion;

	/* record the RAP insertion flag */
	Parameters->OutstandingRAPRequest = Parameters->RequestRAPBit;
	Parameters->OutstandingRAPIndexRequest = Parameters->RequestRAPBit;

	/* Update the Last_queuedDTS */
	/* In case of pause do not update this PTS */
	/* Because on unpause PTS should be inline with PCR */
	if (Parameters->Size != 0) {
		Stream->LastQueuedDTS = (Parameters->Repeating) ?
			INVALID_TIME : Parameters->DTS;
		Stream->stream_paused = 0;
	}
	else
		Stream->stream_paused = 1;

	mutex_unlock(&Context->Lock);

	return MultiplexorNoError;
}

/*
 ---------------------------------------------------------------------

 PUBLIC - Function to un-supply a peviously supplied buffer
 via the buffer Id

 */
MultiplexorStatus_t MultiplexorCancelBuffer(MultiplexorContext_t Context,
	unsigned int BufferId)
{
	MultiplexorBufferParameters_t *Buffer;
	MultiplexorStreamParameters_t *Stream;
	MultiplexorBufferParameters_t **Pointer;

	/* Cheat to find the stream */
	if (mutex_lock_interruptible(&Context->Lock) != 0)
		return MultiplexorError;

	if (!inrange(BufferId, 0, MAXIMUM_NUMBER_OF_BUFFERS)) {
		pr_err("MultiplexorCancelBuffer - BufferId invalid (%d).\n",
			BufferId);
		mutex_unlock(&Context->Lock);
		return MultiplexorInvalidId;
	}

	Buffer = &Context->Buffers[BufferId];
	Stream = &Context->Streams[Buffer->ProgramId][Buffer->StreamId];

	if (!inrange(Buffer->ProgramId, 0, MAXIMUM_NUMBER_OF_PROGRAMS-1) ||
		!inrange(Buffer->StreamId, 0, MAXIMUM_NUMBER_OF_STREAMS_PER_PROGRAM-1)
		|| !Stream->EntryUsed) {
		pr_err("MultiplexorCancelBuffer - Failed to identify stream for BufferId.\n");
		mutex_unlock(&Context->Lock);
		return MultiplexorUnrecognisedId;
	}

	/* Find the pointer to this buffer, and pluck it out of the list */
	for (Pointer = &Stream->DataBuffers; *Pointer != NULL;
		Pointer = &((*Pointer)->NextDataBuffer))

		if (*Pointer == Buffer) {
			*Pointer = Buffer->NextDataBuffer;

			Context->Settings.BufferReleaseCallback(Context, true,
				Buffer);

			mutex_unlock(&Context->Lock);
			return MultiplexorNoError;
		}

	/* didn't find it */
	pr_err("MultiplexorCancelBuffer - Failed to find buffer attached to the stream.\n");
	mutex_unlock(&Context->Lock);
	return MultiplexorUnrecognisedId;
}

/*
 ---------------------------------------------------------------------

 PUBLIC - Function to prepare for output (performing all
 checking prior to the actual performance

 */
MultiplexorStatus_t MultiplexorPrepareOutput(MultiplexorContext_t Ctx,
	MultiplexorOutputParameters_t *Parameters,
	MultiplexorOutputStatus_t *StatusInfo)
{
	unsigned int i, j;
	MultiplexorStatus_t Status;
	unsigned int BytesNeeded;

	/* Validate parameters */
	ParameterAssert((Parameters->OutputBuffer != NULL),
		"MultiplexorPrepareOutput - Output buffer is NULL.");

	/* Record the interesting bits of the input */
	if (mutex_lock_interruptible(&Ctx->Lock) != 0)
		return MultiplexorError;

	memset(StatusInfo, 0x00, sizeof(MultiplexorOutputStatus_t));

	Ctx->Flushing = Parameters->Flush;
	Ctx->OutputBuffer = Parameters->OutputBuffer;
	Ctx->OutputStatus = StatusInfo;
	Ctx->IndexCount = 0;

	/* Do we need to do table regeneration
	 * Split up into regenerate packed stream list, and regenerate
	 * actual tables. */
	if (Ctx->RegenerateTables) {

		Ctx->PMTCount = 0;
		Ctx->StreamCount = 0;

		for (i = 0; i < MAXIMUM_NUMBER_OF_PROGRAMS; i++)
			if (Ctx->Programs[i].EntryUsed) {

				for (j = 0;
					j < MAXIMUM_NUMBER_OF_STREAMS_PER_PROGRAM;
					j++)
					if (Ctx->Streams[i][j].EntryUsed)
						Ctx->PackedStreams
						[Ctx->StreamCount++]
							= &Ctx->Streams[i][j];
			}

		MultiplexorGenerateDit(Ctx);

		if (Ctx->Settings.GeneratePatPmt) {
			MultiplexorGeneratePat(Ctx);

			if (Ctx->Settings.GenerateSdt)
				MultiplexorGenerateSdt(Ctx);

			for (i = 0; i < MAXIMUM_NUMBER_OF_PROGRAMS; i++)
				if (Ctx->Programs[i].EntryUsed &&
					!Ctx->Programs[i].TablesProgram) {
					Ctx->PMTCount++;
					Status = MultiplexorGeneratePmt(
						Ctx, i);
					if (Status != MultiplexorNoError) {
						pr_err("MultiplexorPrepareOutput - Failed to generate tables.\n");
						mutex_unlock(&Ctx->Lock);
						return Status;
					}
				}
		}

		Ctx->RegenerateTables = false;
	}

	/* Check that we have initialized the PCR value */
	if (!Ctx->PcrInitialized) {
		Status = MultiplexorGenerateInitialPcr(Ctx);
		if (Status != MultiplexorNoError) {
			pr_err("MultiplexorPrepareOutput - Failed to generate initial Pcr.\n");
			mutex_unlock(&Ctx->Lock);
			return Status;
		}
	}

	/* Get the bitrate */
	Status = MultiplexorCaclculateBitrate(Ctx);
	if (Status != MultiplexorNoError) {
		mutex_unlock(&Ctx->Lock);
		return Status;
	}

	BytesNeeded = (Ctx->OutputPackets
		* (Ctx->Settings.TimeStampedPackets ? 192 : 188));
	if (BytesNeeded > Ctx->OutputBuffer->Size) {
		pr_err("MultiplexorPrepareOutput - Output buffer not large enough (%d > %d).\n",
			BytesNeeded, Ctx->OutputBuffer->Size);
		mutex_unlock(&Ctx->Lock);

		Ctx->OutputStatus->OverflowOutputSize = BytesNeeded;
		return MultiplexorOutputOverflow;
	}

	/* Output PCR packets */
	Ctx->OutputPacketCount = 0;

	if (Ctx->Settings.GeneratePcrStream)
		OutputCode(CodeForPcr());

	/* output the pcr within the data */
	for (i = 0; i < Ctx->StreamCount; i++)
		if (Ctx->PackedStreams[i]->IncorporatePcrPacket) {
			OutputCode(CodeForPcrOnStream(
					Ctx->PackedStreams[i]->ProgramId,
					Ctx->PackedStreams[i]->StreamId));
		}
	/* Now perform the multiplexing of the streams */
	Status = MultiplexorPerformMultiplex(Ctx);
	if ((Status != MultiplexorNoError) && (Status
		!= MultiplexorDeliveryFailure)) {

		mutex_unlock(&Ctx->Lock);
		return Status;
	}

	/* Update Output status with information from this output */
	Ctx->OutputStatus->NonOutputDataRemains = false;
	Ctx->OutputStatus->PCR
		= PcrLimit(Ctx->InitialPcr + Ctx->PcrOffset);
	Ctx->OutputStatus->OffsetFromFirstOutput = Ctx->PcrOffset;
	Ctx->OutputStatus->OutputDuration = Ctx->Settings.PcrPeriod;
	Ctx->OutputStatus->Bitrate = Ctx->Bitrate;
	Ctx->OutputStatus->IndexCount = Ctx->IndexCount;
	Ctx->OutputStatus->OutputTSPackets = Ctx->OutputPacketCount;

	Ctx->PcrOffset += Ctx->Settings.PcrPeriod;
	Ctx->TotalPacketCount += Ctx->OutputPacketCount;

	mutex_unlock(&Ctx->Lock);
	return Status;
}

/*
 ---------------------------------------------------------------------

 PUBLIC - Function to perform an output

 */
MultiplexorStatus_t MultiplexorPerformOutput(MultiplexorContext_t Context)
{
	unsigned int i;
	MultiplexorStreamParameters_t *Stream;
	unsigned int RealDataPackets;

	/* Prepare the output fields */
	if (mutex_lock_interruptible(&Context->Lock) != 0)
		return MultiplexorError;

	Context->OutputBuffer->TotalUsedSize = 0;
	Context->OutputBuffer->CurrentPage = Context->OutputBuffer->Pages;
	Context->OutputBuffer->RemainingPageSize
		= Context->OutputBuffer->CurrentPage->Size;
	Context->OutputBuffer->PagePointer
		= Context->OutputBuffer->CurrentPage->Base;

	for (i = 0; i < Context->OutputPacketCount; i++)
		MultiplexorOutputOnePacket(Context, i,
			Context->OutputPacketDescriptors[i]);

	/* Update Output status with information from this output */
	for (i = 0; i < Context->StreamCount; i++) {
		Stream = Context->PackedStreams[i];

		if (Stream->DataBuffers != NULL) {
			RealDataPackets
				= Stream->DataBuffers->NumberOfTransportPackets;
			if (Stream->IncorporatePcrPacket
				&& (Stream->DataBuffers->NumberOfTransportPackets
				!= 0))

				RealDataPackets = RealDataPackets - 1;

			if ((Stream->DataBuffers != NULL) &&
				(RealDataPackets != 0) &&
				!Stream->DataBuffers->Repeating)

				Context->OutputStatus->NonOutputDataRemains
					= true;
		}

		if (!Stream->StreamPes)
			Stream->CurrentBitBufferLevel = 0;

		Context->OutputStatus->DecoderBitBufferLevels[Stream->ProgramId][Stream->StreamId]
			= Stream->CurrentBitBufferLevel;
	}

	mutex_unlock(&Context->Lock);
	return MultiplexorNoError;
}


