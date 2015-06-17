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

Source file name : tsmux_mme.c
Architects :	   Dan Thomson & Pete Steiglitz

The tsmux mme interface layer implementation


Date	Modification				Architects
----	------------				----------
24-Apr-12   Created				DT & PS

************************************************************************/
#include <linux/hrtimer.h>
#include <linux/slab.h>
#include <stm_te_dbg.h>
#include "mme.h"
#include "multiplexor.h"
#include "TsmuxTransformerTypes.h"

/*
 ---------------------------------------------------------------------
 a few important compatibility checks
 */
#if TSMUX_MAX_PROGRAMS != MAXIMUM_NUMBER_OF_PROGRAMS
#error Mismatch between supported number of programs between components
#endif

#if TSMUX_MAX_STREAMS_PER_PROGRAM != MAXIMUM_NUMBER_OF_STREAMS_PER_PROGRAM
#error Mismatch between supported number of streams per program between components
#endif

#if TSMUX_MAX_BUFFERS != MAXIMUM_NUMBER_OF_BUFFERS
#error Mismatch between supported number of buffers between components
#endif

#if TSMUX_MAXIMUM_PROGRAM_NAME_SIZE != MAXIMUM_NAME_SIZE
#error Mismatch between supported maximum name size between components
#endif

#if TSMUX_MAXIMUM_DESCRIPTOR_SIZE != MAXIMUM_DESCRIPTOR_SIZE
#error Mismatch between supported maximum descriptor size between components
#endif

#if TSMUX_MAXIMUM_INDEX_IDENTIFIER_SIZE != MAXIMUM_INDEX_IDENTIFIER_SIZE
#error Mismatch between supported maximum identifier size between components
#endif


#define TSMUX_PAGE_SIZE (4 * PAGE_SIZE)

/*
 ---------------------------------------------------------------------
 Default translation
 */
#define ApplyDefault(v, d)	(((v) == TSMUX_MME_UNSPECIFIED) ? (d) : (v))

/*
 ---------------------------------------------------------------------
 Internal structure related to any open transformer
 */
typedef struct TransformContext_s {
	MME_Command_t *OutstandingCommands[TSMUX_MAX_BUFFERS];
	MultiplexorContext_t MultiplexorContext;
	MultiplexorOutputStatus_t OutputStatus;
} TransformContext_t;

/*
 ---------------------------------------------------------------------
 PRIVATE - callback function from multiplexor
 */
#if 0
static void TsmuxBufferReleaseCallback(MultiplexorContext_t Context,
	bool Cancelled, MultiplexorBufferParameters_t *Parameters)
{
	MME_ERROR Status;
	TransformContext_t *TransformContext =
		(TransformContext_t *) Parameters->UserParameters;

	/* Now we signal completion of the send buffers command. */
	if (TransformContext->OutstandingCommands[Parameters->BufferId] ==
			NULL) {
		pr_err("TsmuxBufferReleaseCallback - Command not outstanding.\n");
		return;
	}

	Status = (Cancelled ? MME_COMMAND_ABORTED : MME_SUCCESS);
	TransformContext->OutstandingCommands[Parameters->BufferId]->CmdStatus.Error
		= Status;

	Status = MME_NotifyHost(MME_COMMAND_COMPLETED_EVT,
		TransformContext->OutstandingCommands[Parameters->BufferId],
		Status);

	if (Status != MME_SUCCESS)
		pr_err("TsmuxBufferReleaseCallback - Failed to notify host of command completion (%d).\n",
			Status);

	TransformContext->OutstandingCommands[Parameters->BufferId] = NULL;
}
#endif

static void TsmuxInternalBufferReleaseCallback(MultiplexorContext_t Context,
	bool Cancelled, MultiplexorBufferParameters_t *Parameters)
{
	MultiplexorScatterPage_t *Page;
	int i;

	for (Page = Parameters->Pages, i = 0;
			i < Parameters->PageCount; Page = Page->Next, i++) {
		kfree(Page->Base);
		Page->Base = NULL;
		Page->Size = 0;
	}

	MultiplexorFreeBufferStructure(Context, Parameters);

	return;
}

/*
 ---------------------------------------------------------------------
 PRIVATE - Function to translate a multiplexor status
 */
static MME_ERROR TsmuxTranslateStatus(MultiplexorStatus_t Status)
{
	switch (Status) {
	case MultiplexorNoError:
		return TSMUX_NO_ERROR;
	case MultiplexorError:
		return TSMUX_GENERIC_ERROR;
	case MultiplexorInvalidParameter:
		return TSMUX_INVALID_PARAMETER;
	case MultiplexorIdInUse:
		return TSMUX_ID_IN_USE;
	case MultiplexorInvalidId:
		return TSMUX_ID_OUT_OF_RANGE;
	case MultiplexorAllBuffersInUse:
		return TSMUX_ALL_BUFFERS_IN_USE;
	case MultiplexorAllPagesInUse:
		return TSMUX_ALL_SCATTER_PAGES_IN_USE;
	case MultiplexorUnsupportedStreamtype:
		return TSMUX_UNSUPPORTED_TSMUX_STREAM_TYPE;
	case MultiplexorUnrecognisedId:
		return TSMUX_UNRECOGNISED_ID;
	case MultiplexorInputOverflow:
		return TSMUX_INPUT_OVERFLOW;
	case MultiplexorRepeatIntervalExceedsDuration:
		return TSMUX_REPEAT_INTERVAL_EXCEEDS_DURATION;
	case MultiplexorOutputOverflow:
		return TSMUX_OUTPUT_OVERFLOW;
	case MultiplexorInputUnderflow:
		return TSMUX_INPUT_UNDERFLOW;
	case MultiplexorBitBufferViolation:
		return TSMUX_BIT_BUFFER_VIOLATION;
	case MultiplexorDeliveryFailure:
		return TSMUX_MULTIPLEX_DTS_VIOLATION;
	case MultiplexorDTSIntegrityFailure:
		return TSMUX_DTS_INTEGRITY_FAILURE;
	case MultiplexorNoPcrChannelForProgram:
		return TSMUX_NO_PCR_PID_SPECIFIED;
	default:
		return TSMUX_GENERIC_ERROR | (Status << 8);
	}
}

/*
 ---------------------------------------------------------------------
 PRIVATE - Add program function
 */
static MME_ERROR TsmuxAddProgram(void *context, MME_Command_t *commandInfo,
	TSMUX_AddProgram_t *AddProgram)
{
	TransformContext_t *Context = (TransformContext_t *) context;
	MultiplexorStatus_t Status;
	MultiplexorProgramParameters_t Parameters;

	memset(&Parameters, 0x00, sizeof(MultiplexorProgramParameters_t));

	Parameters.ProgramId = AddProgram->ProgramId;
	Parameters.TablesProgram = AddProgram->TablesProgram != 0;
	Parameters.ProgramNumber
		= ApplyDefault(AddProgram->ProgramNumber,
				AddProgram->ProgramId);
	Parameters.PMTPid
		= ApplyDefault(AddProgram->PMTPid,
				(0x30 + AddProgram->ProgramId));
	Parameters.StreamsMayBeScrambled = AddProgram->StreamsMayBeScrambled
		!= 0;
	Parameters.OptionalDescriptorSize = AddProgram->OptionalDescriptorSize;

	strncpy(Parameters.ProviderName, AddProgram->ProviderName,
		MAXIMUM_NAME_SIZE - 1);
	Parameters.ProviderName[MAXIMUM_NAME_SIZE - 1] = 0;

	strncpy(Parameters.ServiceName, AddProgram->ServiceName,
		MAXIMUM_NAME_SIZE - 1);
	Parameters.ServiceName[MAXIMUM_NAME_SIZE - 1] = 0;

	if (Parameters.OptionalDescriptorSize != 0)
		memcpy(Parameters.Descriptor, AddProgram->Descriptor,
				Parameters.OptionalDescriptorSize);

	Status = MultiplexorAddProgram(Context->MultiplexorContext,
		&Parameters);

	commandInfo->CmdStatus.Error = TsmuxTranslateStatus(Status);
	return (Status == MultiplexorNoError) ? MME_SUCCESS
		: MME_INVALID_ARGUMENT;
}

/*
 ---------------------------------------------------------------------
 PRIVATE - remove program function
 */
static MME_ERROR TsmuxRemoveProgram(void *context, MME_Command_t *commandInfo,
	TSMUX_RemoveProgram_t *RemoveProgram)
{
	TransformContext_t *Context = (TransformContext_t *) context;
	MultiplexorStatus_t Status;
	MultiplexorProgramParameters_t Parameters;

	memset(&Parameters, 0x00, sizeof(MultiplexorProgramParameters_t));

	Parameters.ProgramId = RemoveProgram->ProgramId;

	Status = MultiplexorRemoveProgram(Context->MultiplexorContext,
		&Parameters);

	commandInfo->CmdStatus.Error = TsmuxTranslateStatus(Status);
	return (Status == MultiplexorNoError) ? MME_SUCCESS
		: MME_INVALID_ARGUMENT;
}

/*
 ---------------------------------------------------------------------
 PRIVATE - Add stream function
 */
static MME_ERROR TsmuxAddStream(void *context, MME_Command_t *commandInfo,
	TSMUX_AddStream_t *AddStream)
{
	TransformContext_t *Context = (TransformContext_t *) context;
	MultiplexorStatus_t Status;
	MultiplexorStreamParameters_t Parameters;

	memset(&Parameters, 0x00, sizeof(MultiplexorStreamParameters_t));

	Parameters.ProgramId = AddStream->ProgramId;
	Parameters.StreamId = AddStream->StreamId;

	Parameters.IncorporatePcrPacket = AddStream->IncorporatePcr != 0;
	Parameters.StreamPes = AddStream->StreamContent
		== TSMUX_STREAM_CONTENT_PES;

	Parameters.StreamPid = ApplyDefault(AddStream->StreamPid,
		(0x100 * Parameters.ProgramId + Parameters.StreamId));
	Parameters.StreamType = AddStream->StreamType;

	Parameters.OptionalDescriptorSize = AddStream->OptionalDescriptorSize;
	if (Parameters.OptionalDescriptorSize != 0)
		memcpy(Parameters.Descriptor, AddStream->Descriptor,
			Parameters.OptionalDescriptorSize);

	Parameters.DTSIntegrityThreshold
		= ApplyDefault(AddStream->DTSIntegrityThreshold90KhzTicks,
				90000);
	Parameters.DecoderBitBufferSize
		= ApplyDefault(AddStream->DecoderBitBufferSize,
				(Parameters.StreamPes ? 1000000 : 0));
	Parameters.MultiplexAheadLimit
		= ApplyDefault(AddStream->MultiplexAheadLimit90KhzTicks,
				(Parameters.StreamPes ? 90000 : 0));

	Status = MultiplexorAddStream(Context->MultiplexorContext, &Parameters);

	commandInfo->CmdStatus.Error = TsmuxTranslateStatus(Status);
	return (Status == MultiplexorNoError) ? MME_SUCCESS
		: MME_INVALID_ARGUMENT;
}

/*
 ---------------------------------------------------------------------
 PRIVATE - Remove stream function
 */
static MME_ERROR TsmuxRemoveStream(void *context, MME_Command_t *commandInfo,
	TSMUX_RemoveStream_t *RemoveStream)
{
	TransformContext_t *Context = (TransformContext_t *) context;
	MultiplexorStatus_t Status;
	MultiplexorStreamParameters_t Parameters;

	memset(&Parameters, 0x00, sizeof(MultiplexorStreamParameters_t));

	Parameters.ProgramId = RemoveStream->ProgramId;
	Parameters.StreamId = RemoveStream->StreamId;

	Status = MultiplexorRemoveStream(Context->MultiplexorContext,
		&Parameters);

	commandInfo->CmdStatus.Error = TsmuxTranslateStatus(Status);
	return (Status == MultiplexorNoError) ? MME_SUCCESS
		: MME_INVALID_ARGUMENT;
}

/*
 ---------------------------------------------------------------------
 PRIVATE - Perform a transform function
 */
static MME_ERROR TsmuxTransform(void *context, MME_Command_t *commandInfo,
	TSMUX_TransformParam_t *Transform)
{
	unsigned int i;
	TransformContext_t *Context = (TransformContext_t *) context;
	MultiplexorStatus_t Status;
	MultiplexorStatus_t PerformStatus;
	MultiplexorOutputParameters_t Parameters;
	MultiplexorScatterPage_t *Page;
	TSMUX_CommandStatus_t *CommandStatus;
	unsigned int TotalSize;
	ktime_t StartTime;
	struct timespec ts;
	getrawmonotonic(&ts);
	StartTime = timespec_to_ktime(ts);

	/* Simple parameter tests */
	if ((commandInfo->NumberInputBuffers != 0) ||
			(commandInfo->NumberOutputBuffers != 1) ||
			(commandInfo->DataBuffers_p == NULL) ||
			(commandInfo->DataBuffers_p[0] == NULL) ||
			(commandInfo->DataBuffers_p[0]->NumberOfScatterPages == 0) ||
			(commandInfo->DataBuffers_p[0]->ScatterPages_p == NULL)) {
		pr_err("TsmuxTransform - Incorrect buffer list\n");
		commandInfo->CmdStatus.Error = TSMUX_INVALID_PARAMETER;
		return MME_INVALID_ARGUMENT;
	}

	/* Fill out the transform parameters and Get a buffer structure and
	 * fill it in. */
	memset(&Parameters, 0x00, sizeof(MultiplexorOutputParameters_t));
	memset(&Context->OutputStatus, 0x00,
		sizeof(MultiplexorOutputParameters_t));

	Parameters.Flush = (Transform->TransformFlags
		& TSMUX_TRANSFORM_FLAG_FLUSH) != 0;
	pr_debug("TsmuxTransform flush is %d\n", Parameters.Flush);

	/* Get a buffer structure and fill it in. */
	Status = MultiplexorGetBufferStructure(Context->MultiplexorContext,
		commandInfo->DataBuffers_p[0]->NumberOfScatterPages,
		&Parameters.OutputBuffer);
	if (Status != MultiplexorNoError) {
		pr_err("TsmuxTransform - Failed to allocate a buffer structure.\n");
		commandInfo->CmdStatus.Error = TsmuxTranslateStatus(Status);
		return MME_NOMEM;
	}

	Parameters.OutputBuffer->Size
		= commandInfo->DataBuffers_p[0]->TotalSize;

	for (Page = Parameters.OutputBuffer->Pages, i = 0;
		i < Parameters.OutputBuffer->PageCount; Page = Page->Next, i++) {

		Page->Size
			= commandInfo->DataBuffers_p[0]->ScatterPages_p[i].Size;
		Page->Base
			= commandInfo->DataBuffers_p[0]->ScatterPages_p[i].Page_p;
	}

	/* Attempt to do the transform
	 NOTE	we preserve the status if it is MultiplexorDeliveryFailure,
	 this is a success status but with a proviso on the nature
	 of the generated multiplex. */
	Status = MultiplexorPrepareOutput(Context->MultiplexorContext,
		&Parameters, &Context->OutputStatus);
	if ((Status == MultiplexorNoError) || (Status
		== MultiplexorDeliveryFailure)) {

		PerformStatus = MultiplexorPerformOutput(
			Context->MultiplexorContext);
		if (PerformStatus != MultiplexorNoError)
			Status = PerformStatus;
	}

	/* Copy over the output status information */
	CommandStatus
		= (TSMUX_CommandStatus_t *)commandInfo->CmdStatus.AdditionalInfo_p;

	CommandStatus->StructSize = sizeof(TSMUX_CommandStatus_t);

	CommandStatus->OutputBufferSizeNeeded
		= Context->OutputStatus.OverflowOutputSize;
	CommandStatus->DataUnderflowFirstProgram
		= Context->OutputStatus.UnderflowProgram;
	CommandStatus->DataUnderflowFirstStream
		= Context->OutputStatus.UnderflowStream;

	CommandStatus->OutputSize = Parameters.OutputBuffer->TotalUsedSize;
	CommandStatus->OffsetFromStart
		= (TSMUX_MME_TIMESTAMP) Context->OutputStatus.OffsetFromFirstOutput;
	CommandStatus->OutputDuration = Context->OutputStatus.OutputDuration;
	CommandStatus->Bitrate = Context->OutputStatus.Bitrate;
	CommandStatus->PCR = (TSMUX_MME_TIMESTAMP) Context->OutputStatus.PCR;
	CommandStatus->TransformOutputFlags
		 = Context->OutputStatus.NonOutputDataRemains ?
			TSMUX_TRANSFORM_OUTPUT_FLAG_UNCONSUMED_DATA : 0;

	CommandStatus->NumberOfIndexRecords = Context->OutputStatus.IndexCount;
	for (i = 0; i < Context->OutputStatus.IndexCount; i++) {
		CommandStatus->IndexRecords[i].PacketOffset
			= Context->OutputStatus.Index[i].PacketOffset;
		CommandStatus->IndexRecords[i].Program
			 = Context->OutputStatus.Index[i].Program;
		CommandStatus->IndexRecords[i].Stream
			= Context->OutputStatus.Index[i].Stream;
		CommandStatus->IndexRecords[i].IndexIdentifierSize
			= Context->OutputStatus.Index[i].IdentifierSize;
		if (Context->OutputStatus.Index[i].IdentifierSize != 0)
			memcpy(
				CommandStatus->IndexRecords[i].IndexIdentifier,
				Context->OutputStatus.Index[i].Identifier,
				Context->OutputStatus.Index[i].IdentifierSize);
		CommandStatus->IndexRecords[i].pts =
			Context->OutputStatus.Index[i].pts;
	}

	memcpy(
		CommandStatus->DecoderBitBufferLevels,
		Context->OutputStatus.DecoderBitBufferLevels,
		TSMUX_MAX_PROGRAMS * TSMUX_MAX_STREAMS_PER_PROGRAM
			* sizeof(MME_UINT));

	/* Copy the completed buffers into the transform status */
	CommandStatus->CompletedBufferCount =
			Context->OutputStatus.CompletedBufferCount;
	for (i = 0; i < Context->OutputStatus.CompletedBufferCount; i++) {
		CommandStatus->CompletedBuffers[i] =
				Context->OutputStatus.CompletedBuffers[i];
	}

	/* Finally transfer the information regarding the output to
	 * mme output buffer structure, and release the multiplexor one. */
	if ((Status == MultiplexorNoError) ||
			(Status == MultiplexorDeliveryFailure)) {

		TotalSize = CommandStatus->OutputSize;
		for (i = 0; i < Parameters.OutputBuffer->PageCount; i++) {
			commandInfo->DataBuffers_p[0]->ScatterPages_p[i].BytesUsed
				= min(TotalSize,
					commandInfo->DataBuffers_p[0]->ScatterPages_p[i].Size);
			TotalSize -=
				commandInfo->DataBuffers_p[0]->ScatterPages_p[i].BytesUsed;
		}
	}

	MultiplexorFreeBufferStructure(Context->MultiplexorContext,
		Parameters.OutputBuffer);

	commandInfo->CmdStatus.Error = TsmuxTranslateStatus(Status);
	CommandStatus->TransformDurationMs =
		(ktime_to_ns(ktime_sub(ktime_get(), StartTime)) / 1000000);
	CommandStatus->TSPacketsOut =
				Context->OutputStatus.OutputTSPackets;

	return ((Status == MultiplexorNoError) ||
			(Status == MultiplexorDeliveryFailure)) ?
				MME_SUCCESS : MME_INVALID_ARGUMENT;
}

static int TsmuxAllocatePages(unsigned int numPages,
	MultiplexorScatterPage_t *Pages,
	unsigned int totalSize)
{
	MultiplexorScatterPage_t *Page;
	int i, j, sizeRemaining;

	if (!numPages) {
		pr_err("Attempt to allocate 0 pages\n");
		return 1;
	}
	sizeRemaining = totalSize;

	for (Page = Pages, i = 0; i < (numPages - 1); Page = Page->Next, i++) {
		Page->Base = kzalloc(TSMUX_PAGE_SIZE, GFP_KERNEL);
		if (Page->Base == NULL) {
			pr_err("Unable to allocate an internal page\n");
			for (j = i, Page = Pages; j > 0;
					Page = Page->Next, j--)
				kfree(Page->Base);
			return 1;
		}

		Page->Size = TSMUX_PAGE_SIZE;
		sizeRemaining -= TSMUX_PAGE_SIZE;
	}
	Page->Base = kzalloc(sizeRemaining, GFP_KERNEL);
	if (Page->Base == NULL) {
		pr_err("Unable to allocate last internal page\n");
		return 1;
	}
	Page->Size = sizeRemaining;
	pr_debug("Allocated %d pages of size %d and remaining page size %d\n",
		(numPages - 1), (unsigned int)TSMUX_PAGE_SIZE, sizeRemaining);

	return 0;
}

static int TsmuxCopyBuffer(MME_DataBuffer_t *cmdBuf,
	MultiplexorBufferParameters_t *muxBuf)
{
	MME_ScatterPage_t *cmdPage;
	MultiplexorScatterPage_t *muxPage;
	unsigned int muxPageLeft, cmdPageLeft;
	unsigned char *muxPagePtr, *cmdPagePtr;
	int i;
	static bool first = true;

	/* Check sizes of buffers */
	if (cmdBuf->TotalSize > muxBuf->Size) {
		pr_err("Internal buffer too small\n");
		return 1;
	}

	if (first) {
		pr_debug("cmdNumPages=%d\n", cmdBuf->NumberOfScatterPages);
		for (i = 0; i < cmdBuf->NumberOfScatterPages; i++) {
			pr_debug("cmdPage->Size=%d cmdPage->Page_p=%p\n",
					cmdBuf->ScatterPages_p[i].Size,
					cmdBuf->ScatterPages_p[i].Page_p);
		}
	}

	muxPage = muxBuf->Pages;
	if (!muxPage) {
		pr_err("Invalid multiplexer first page pointer\n");
		return 1;
	}

	muxPagePtr = muxPage->Base;
	muxPageLeft = muxPage->Size;

	for (i = 0, cmdPage = cmdBuf->ScatterPages_p;
			i < cmdBuf->NumberOfScatterPages; i++, cmdPage++) {
		if (!muxPage) {
			pr_err("Invalid multiplexer page pointer\n");
			return 1;
		}
		if (!cmdPage) {
			pr_err("Invalid MME command page pointer\n");
			return 1;
		}
		cmdPageLeft = cmdPage->Size;
		cmdPagePtr = cmdPage->Page_p;

		do {
			if (cmdPageLeft <= muxPageLeft) {
				if (first)
					pr_debug("Copying %d from %p to %p\n",
						cmdPageLeft, cmdPagePtr,
						muxPagePtr);
				memcpy(muxPagePtr, cmdPagePtr, cmdPageLeft);
				muxPagePtr += cmdPageLeft;
				muxPageLeft -= cmdPageLeft;
				cmdPageLeft = 0;
			} else {
				if (first)
					pr_debug("Copying %d from %p to %p\n",
						muxPageLeft, cmdPagePtr,
						muxPagePtr);
				memcpy(muxPagePtr, cmdPagePtr, muxPageLeft);
				cmdPagePtr += muxPageLeft;
				cmdPageLeft -= muxPageLeft;
				muxPageLeft = 0;
			}
			if (muxPageLeft == 0 && muxPage->Next) {
				muxPage = muxPage->Next;
				if (!muxPage) {
					pr_err("Invalid multiplexer first page pointer\n");
					return 1;
				}
				muxPagePtr = muxPage->Base;
				muxPageLeft = muxPage->Size;
			}
		} while (cmdPageLeft != 0);
	}
	first = false;

	return 0;
}


/*
 ---------------------------------------------------------------------
 PRIVATE - supply an input buffer to the multiplexor
 */
static MME_ERROR TsmuxSendBuffers(void *context, MME_Command_t *commandInfo,
	TSMUX_SendBuffersParam_t *SendBuffer)
{
	TransformContext_t *Context = (TransformContext_t *) context;
	MultiplexorStatus_t Status;
	bool ScatterPagesNeeded;
	MultiplexorBufferParameters_t *Buffer;
	unsigned int numScatterPages = 0;

	/* Simple parameter tests */
	ScatterPagesNeeded = (SendBuffer->BufferFlags
		& TSMUX_BUFFER_FLAG_NO_DATA) == 0;

	if ((commandInfo->NumberInputBuffers != 1) ||
			(commandInfo->NumberOutputBuffers != 0) ||
			(commandInfo->DataBuffers_p == NULL) ||
			(commandInfo->DataBuffers_p[0] == NULL) ||
			(ScatterPagesNeeded &&
				((commandInfo->DataBuffers_p[0]->NumberOfScatterPages == 0) ||
					(commandInfo->DataBuffers_p[0]->ScatterPages_p == NULL)))) {
		pr_err("TsmuxSendBuffers - Incorrect buffer list\n");
		return MME_INVALID_ARGUMENT;
	}

	if (((SendBuffer->BufferFlags & TSMUX_BUFFER_FLAG_NO_DATA) != 0)
		&& ((SendBuffer->BufferFlags & ~TSMUX_BUFFER_FLAG_NO_DATA) != 0)) {
		pr_err("TsmuxSendBuffers - Buffer has NO_DATA set, yet has other flags also set (%04x).\n",
			SendBuffer->BufferFlags);
		return MME_INVALID_ARGUMENT;
	}
	if (ScatterPagesNeeded)
		numScatterPages = (commandInfo->DataBuffers_p[0]->TotalSize +
				(TSMUX_PAGE_SIZE - 1)) / TSMUX_PAGE_SIZE;
	pr_debug("NumScatterPages is %d\n", numScatterPages);

	/* Get a buffer structure and fill it in. */
	Status = MultiplexorGetBufferStructure(Context->MultiplexorContext,
			numScatterPages, &Buffer);
	if (Status != MultiplexorNoError) {
		pr_err("TsmuxSendBuffers - Failed to allocate a buffer structure.\n");
		commandInfo->CmdStatus.Error = TsmuxTranslateStatus(Status);
		return MME_NOMEM;
	}
	Buffer->Size = commandInfo->DataBuffers_p[0]->TotalSize;
	pr_debug("Buffer->Size is %d\n", Buffer->Size);

	if (ScatterPagesNeeded) {
		if (TsmuxAllocatePages(numScatterPages, Buffer->Pages,
				commandInfo->DataBuffers_p[0]->TotalSize)) {
			commandInfo->CmdStatus.Error = TSMUX_NO_MEMORY;
			return MME_NOMEM;
		}
		if (TsmuxCopyBuffer(commandInfo->DataBuffers_p[0], Buffer)) {
			commandInfo->CmdStatus.Error = TSMUX_NO_MEMORY;
			return MME_NOMEM;
		}
	}

	Buffer->ProgramId = SendBuffer->ProgramId;
	Buffer->StreamId = SendBuffer->StreamId;
	Buffer->TotalUsedSize = commandInfo->DataBuffers_p[0]->TotalSize;
	Buffer->DTS = (unsigned long long) SendBuffer->DTS;
	Buffer->DTSDuration = SendBuffer->DTSDuration;
	Buffer->Discontinuity = (SendBuffer->BufferFlags
		& TSMUX_BUFFER_FLAG_DISCONTINUITY) != 0;
	Buffer->Scrambled = (SendBuffer->BufferFlags
		& TSMUX_BUFFER_FLAG_TRANSPORT_SCRAMBLED) != 0;
	Buffer->Parity = ((SendBuffer->BufferFlags
		& TSMUX_BUFFER_FLAG_KEY_PARITY) != 0) ? 1 : 0;
	Buffer->Repeating = (SendBuffer->BufferFlags
		& TSMUX_BUFFER_FLAG_REPEATING) != 0;
	Buffer->RequestDITInsertion = (SendBuffer->BufferFlags
		& TSMUX_BUFFER_FLAG_REQUEST_DIT_INSERTION) != 0;

	if ((SendBuffer->BufferFlags
		& TSMUX_BUFFER_FLAG_REQUEST_RAP_BIT) != 0) {
		Buffer->RequestRAPBit = true;
	}
	Buffer->RepeatInterval
		= ApplyDefault(SendBuffer->RepeatInterval90KhzTicks, 9000);
	Buffer->DITTransitionFlag = SendBuffer->DITTransitionFlagValue;
	Buffer->IndexSize = SendBuffer->IndexIdentifierSize;
	Buffer->UserParameters = Context;
	Buffer->UserData = commandInfo->DataBuffers_p[0]->UserData_p;
	pr_debug("Buffer with UserData %p created\n", Buffer->UserData);

	if (SendBuffer->IndexIdentifierSize != 0) {
		pr_debug("SendBuffer contains %d index bytes\n",
			SendBuffer->IndexIdentifierSize);
		memcpy(Buffer->IndexData, SendBuffer->IndexIdentifier,
			SendBuffer->IndexIdentifierSize);
	}

	Status = MultiplexorSupplyBuffer(Context->MultiplexorContext, Buffer);

	if (Status != MultiplexorNoError) {
		MultiplexorFreeBufferStructure(Context->MultiplexorContext,
			Buffer);
	}

	commandInfo->CmdStatus.Error = TsmuxTranslateStatus(Status);
	return (Status == MultiplexorNoError) ?
			MME_SUCCESS : MME_INVALID_ARGUMENT;
}

/*
 ---------------------------------------------------------------------
 PUBLIC - MME Abort Function
 */
static MME_ERROR TsmuxAbort(void *context, MME_CommandId_t commandId)
{
	unsigned int i;
	TransformContext_t *Context = (TransformContext_t *) context;

	for (i = 0; i < TSMUX_MAX_BUFFERS; i++)
		if ((Context->OutstandingCommands[i] != NULL)
			&& (Context->OutstandingCommands[i]->CmdStatus.CmdId
			== commandId)) {

			MultiplexorCancelBuffer(Context->MultiplexorContext, i);
			break;
		}

	return MME_SUCCESS;
}

/*
 ---------------------------------------------------------------------
 PUBLIC - MME Get capability Function
 */
static MME_ERROR TsmuxGetCapability(MME_TransformerCapability_t *capability)
{
	TSMUX_TransformerCapability_t *TsmuxCapabilities =
		(TSMUX_TransformerCapability_t *) capability->TransformerInfo_p;
	MultiplexorCapability_t Capabilities;

	if (capability->TransformerInfoSize
		!= sizeof(TSMUX_TransformerCapability_t)) {

		pr_err("Invalid structure size for Capability structure (%d %d)\n",
				capability->TransformerInfoSize,
				sizeof(TSMUX_TransformerCapability_t));
		return MME_INVALID_ARGUMENT;
	}

	MultiplexorGetCapabilities(&Capabilities);

	if ((Capabilities.MaxNameSize < TSMUX_MAXIMUM_PROGRAM_NAME_SIZE) ||
			(Capabilities.MaxDescriptorSize <
					TSMUX_MAXIMUM_DESCRIPTOR_SIZE)) {
		pr_err("Max byte field sizes too small for tsmux (Name %d %d, Descriptor %d %d)\n",
				Capabilities.MaxNameSize,
				TSMUX_MAXIMUM_PROGRAM_NAME_SIZE,
				Capabilities.MaxDescriptorSize,
				TSMUX_MAXIMUM_DESCRIPTOR_SIZE);
		return MME_INVALID_ARGUMENT;
	}

	capability->Version = TSMUX_MME_VERSION;
	memcpy(capability->InputType.FourCC, "N/A", 4);
	memcpy(capability->OutputType.FourCC, "N/A", 4);

	TsmuxCapabilities->ApiVersion = TSMUX_MME_VERSION;
	TsmuxCapabilities->MaximumNumberOfPrograms
		= Capabilities.MaximumNumberOfPrograms;
	TsmuxCapabilities->MaximumNumberOfStreams
		= Capabilities.MaximumNumberOfStreamsPerProgram;
	TsmuxCapabilities->MaximumNumberOfBuffersPerStream
		= Capabilities.MaximumNumberOfBuffers;

	return MME_SUCCESS;
}

/*
 ---------------------------------------------------------------------
 PUBLIC - MME Init Function
 */
static MME_ERROR TsmuxInit(MME_UINT initParamsLength,
	MME_GenericParams_t initParams, void **context)
{
	MultiplexorStatus_t Status;
	TransformContext_t *Context;
	TSMUX_InitTransformerParam_t *TsmuxParameters =
		(TSMUX_InitTransformerParam_t *) initParams;
	MultiplexorInitializationParameters_t MultiplexorParameters;

	if ((initParamsLength != sizeof(TSMUX_InitTransformerParam_t))
		|| (TsmuxParameters->StructSize
			!= sizeof(TSMUX_InitTransformerParam_t))) {
		pr_err("Invalid structure size for transformer init params structure (%d %d %d)\n",
				initParamsLength, TsmuxParameters->StructSize,
				sizeof(TSMUX_InitTransformerParam_t));
		return MME_INVALID_ARGUMENT;
	}

	Context = kzalloc(sizeof(TransformContext_t), GFP_KERNEL);
	if (Context == NULL)
		return MME_NOMEM;

	memset(Context, 0x00, sizeof(TransformContext_t));

	memset(&MultiplexorParameters, 0x00,
		sizeof(MultiplexorInitializationParameters_t));

	MultiplexorParameters.PcrPeriod
		= ApplyDefault(TsmuxParameters->PcrPeriod90KhzTicks, 9000);
	MultiplexorParameters.TimeStampedPackets
		= (TsmuxParameters->TimeStampedPackets != 0);
	MultiplexorParameters.GeneratePcrStream
		= TsmuxParameters->GeneratePcrStream != 0;
	MultiplexorParameters.PcrPid
		= ApplyDefault(TsmuxParameters->PcrPid, 0x1ffe);
	MultiplexorParameters.GeneratePatPmt
		= (TsmuxParameters->TableGenerationFlags
				& TSMUX_TABLE_GENERATION_PAT_PMT) != 0;
	MultiplexorParameters.GenerateSdt
		= (TsmuxParameters->TableGenerationFlags
				& TSMUX_TABLE_GENERATION_SDT) != 0;
	MultiplexorParameters.TablePeriod
		= ApplyDefault(TsmuxParameters->TablePeriod90KhzTicks,
				MultiplexorParameters.PcrPeriod);
	MultiplexorParameters.TransportStreamId
		= ApplyDefault(TsmuxParameters->TransportStreamId, 0);
	MultiplexorParameters.GenerateIndex
		= TsmuxParameters->RecordIndexIdentifiers != 0;
	MultiplexorParameters.FixedBitrate = TsmuxParameters->FixedBitrate != 0;
	MultiplexorParameters.Bitrate = TsmuxParameters->Bitrate;
	MultiplexorParameters.BufferReleaseCallback
		= TsmuxInternalBufferReleaseCallback;
	MultiplexorParameters.GlobalFlags = TsmuxParameters->GlobalFlags;

	Status = MultiplexorOpen(&Context->MultiplexorContext,
		&MultiplexorParameters);
	if (Status != MultiplexorNoError) {
		kfree(Context);
		return MME_NOMEM;
	}

	*context = (void *) Context;
	return MME_SUCCESS;
}

/*
 ---------------------------------------------------------------------
 PUBLIC - MME Command Function
 */
static MME_ERROR TsmuxCommand(void *context, MME_Command_t *commandInfo)
{
	MME_ERROR Status;
	unsigned int ExpectedSize;
	TSMUX_SetGlobalParams_t *GlobalParams;

	/* Check the status structure exists */
	if ((commandInfo->CmdCode == MME_TRANSFORM) &&
		((commandInfo->CmdStatus.AdditionalInfoSize !=
				sizeof(TSMUX_CommandStatus_t)) ||
			(commandInfo->CmdStatus.AdditionalInfo_p == NULL))) {
		pr_err("TsmuxCommand - Incorrect command status structure (%d %d %d)\n",
			commandInfo->CmdStatus.AdditionalInfoSize,
			sizeof(TSMUX_CommandStatus_t),
			(commandInfo->CmdStatus.AdditionalInfo_p == NULL));

		return MME_INVALID_ARGUMENT;
	}

	commandInfo->CmdStatus.Error = MME_SUCCESS;

	/* Check the command size */
	Status = MME_SUCCESS;
	ExpectedSize = 0;

	switch (commandInfo->CmdCode) {
	case MME_SET_PARAMS:
		ExpectedSize = sizeof(TSMUX_SetGlobalParams_t);
		break;
	case MME_TRANSFORM:
		ExpectedSize = sizeof(TSMUX_TransformParam_t);
		break;
	case MME_SEND_BUFFERS:
		ExpectedSize = sizeof(TSMUX_SendBuffersParam_t);
		break;
	default:
		Status = MME_INVALID_ARGUMENT;
		break;
	}

	if (Status != MME_SUCCESS) {
		pr_err("TsmuxCommand - Unsupported command (%d)\n",
			commandInfo->CmdCode);
		return Status;
	}

	if (commandInfo->ParamSize != ExpectedSize) {
		pr_err("TsmuxCommand - Invalid structure size for params structure (%d %d %d)\n",
				commandInfo->CmdCode, commandInfo->ParamSize,
				ExpectedSize);
		return MME_INVALID_ARGUMENT;
	}

	/* Process the command */
	switch (commandInfo->CmdCode) {
	case MME_SET_PARAMS:
		GlobalParams = (TSMUX_SetGlobalParams_t *) commandInfo->Param_p;
		switch (GlobalParams->CommandCode) {
		case TSMUX_COMMAND_ADD_PROGRAM:
			Status = TsmuxAddProgram(context, commandInfo,
				&GlobalParams->CommandSubStructure.AddProgram);
			break;

		case TSMUX_COMMAND_REMOVE_PROGRAM:
			Status = TsmuxRemoveProgram(context, commandInfo,
				&GlobalParams->CommandSubStructure.RemoveProgram);
			break;

		case TSMUX_COMMAND_ADD_STREAM:
			Status = TsmuxAddStream(context, commandInfo,
				&GlobalParams->CommandSubStructure.AddStream);
			break;

		case TSMUX_COMMAND_REMOVE_STREAM:
			Status = TsmuxRemoveStream(context, commandInfo,
				&GlobalParams->CommandSubStructure.RemoveStream);
			break;

		default:
			pr_err("TsmuxCommand - Unsupported global sub-command (%d)\n",
					GlobalParams->CommandCode);
			Status = MME_INVALID_ARGUMENT;
		}
		break;

	case MME_TRANSFORM:
		Status = TsmuxTransform(context, commandInfo,
			(TSMUX_TransformParam_t *) commandInfo->Param_p);
		break;

	case MME_SEND_BUFFERS:
		Status = TsmuxSendBuffers(context, commandInfo,
			(TSMUX_SendBuffersParam_t *) commandInfo->Param_p);
		break;

	default:
		break;
	}

	return Status;
}

/*
 ---------------------------------------------------------------------
 PUBLIC - MME Terminate Function
 */
static MME_ERROR TsmuxTerminate(void *context)
{
	TransformContext_t *Context = (TransformContext_t *) context;

	MultiplexorClose(Context->MultiplexorContext);
	kfree(Context);

	return MME_SUCCESS;
}

/*
 ---------------------------------------------------------------------
 PUBLIC - Function to register this transform
 */
MME_ERROR TsmuxRegister(void)
{
	MME_ERROR Status;

	Status = MME_RegisterTransformer(TSMUX_MME_TRANSFORMER_NAME,
		TsmuxAbort, TsmuxGetCapability, TsmuxInit,
		TsmuxCommand, TsmuxTerminate);
	return Status;
}


