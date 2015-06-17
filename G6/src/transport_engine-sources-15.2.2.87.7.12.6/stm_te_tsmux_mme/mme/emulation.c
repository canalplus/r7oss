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

Source file name : emulation.c
Architects :	   Dan Thomson & Pete Steiglitz

The emulatioon of simple mme functionality for the multiplexor testing


Date	Modification				Architects
----	------------				----------
24-Apr-12   Created                             DT & PS

************************************************************************/

#include <stddef.h>
#include <linux/string.h>
#include <stm_te_dbg.h>

#include "mme.h"

#define MAX_HANDLES	4

#define inrange(val, lower, upper)    (((val) >= (lower)) && ((val) <= (upper)))

/* Variables defining the transform */

static char Name[MME_MAX_TRANSFORMER_NAME];
static MME_AbortCommand_t AbortFunction;
static MME_GetTransformerCapability_t GetCapabilityFunction;
static MME_InitTransformer_t InitFunction;
static MME_ProcessCommand_t ProcessCommandFunction;
static MME_TermTransformer_t TerminateFunction;

typedef struct TransformContext_s {
	bool Open;
	MME_GenericCallback_t Callback;
	void *CallbackUserData;
	void *Context;

	unsigned int NextCommandId;
	MME_Command_t *OutstandingCommands[256];
} TransformContext_t;

static TransformContext_t TransformContexts[MAX_HANDLES];

/* Function to list outstanding commands */

MME_ERROR MME_RegisterTransformer(const char *name,
		MME_AbortCommand_t abortFunc,
		MME_GetTransformerCapability_t getTransformerCapabilityFunc,
		MME_InitTransformer_t initTransformerFunc,
		MME_ProcessCommand_t processCommandFunc,
		MME_TermTransformer_t termTransformerFunc)
{
	memset(TransformContexts, 0x00,
			MAX_HANDLES * sizeof(TransformContext_t));

	strncpy(&Name[0], name, MME_MAX_TRANSFORMER_NAME);
	AbortFunction = abortFunc;
	GetCapabilityFunction = getTransformerCapabilityFunc;
	InitFunction = initTransformerFunc;
	ProcessCommandFunction = processCommandFunc;
	TerminateFunction = termTransformerFunc;
	return MME_SUCCESS;
}

MME_ERROR MME_DeregisterTransformer(const char *name)
{
	memset(TransformContexts, 0x00,
			MAX_HANDLES * sizeof(TransformContext_t));

	strncpy(&Name[0], "", MME_MAX_TRANSFORMER_NAME);
	AbortFunction = NULL;
	GetCapabilityFunction = NULL;
	InitFunction = NULL;
	ProcessCommandFunction = NULL;
	TerminateFunction = NULL;
	return MME_SUCCESS;
}

/* Functions being emulated */

/*	NOTE since I don't use DueTime, I store the handle in it, this
 *	     allows me to retrieve the appropriate callback address.
 */

MME_ERROR MME_NotifyHost(MME_Event_t event, MME_Command_t *commandInfo,
		MME_ERROR res)
{
	MME_CommandId_t savedCmdId = commandInfo->CmdStatus.CmdId;
	MME_Time_t savedDueTime = commandInfo->DueTime;

	pr_debug("MME_NotifyHost Duetime=%d\n", commandInfo->DueTime);
	if (commandInfo->DueTime >= MAX_HANDLES) {
		pr_err(" Bad handle %d\n", commandInfo->DueTime);
		return MME_INVALID_ARGUMENT;
	}

	if ((commandInfo->CmdEnd == MME_COMMAND_END_RETURN_NOTIFY)
			&& (TransformContexts[commandInfo->DueTime].Callback
					!= NULL)) {
		TransformContexts[commandInfo->DueTime].Callback(
				event,
				commandInfo,
				TransformContexts[commandInfo->DueTime].CallbackUserData);
	}

	/* Callback may free the command info structure, so used saved copies */
	if ((event == MME_COMMAND_COMPLETED_EVT)
			&& (TransformContexts[savedDueTime].OutstandingCommands[savedCmdId]
			== commandInfo)) {
		TransformContexts[savedDueTime].OutstandingCommands[savedCmdId]
			= NULL;
	}

	return MME_SUCCESS;
}

MME_ERROR MME_GetTransformerCapability(const char *name,
		MME_TransformerCapability_t *capability)
{
	if ((!name) || (strncmp(name, Name, 128) != 0))
		return MME_UNKNOWN_TRANSFORMER;

	return GetCapabilityFunction(capability);
}

MME_ERROR MME_InitTransformer(const char *name,
		MME_TransformerInitParams_t *params,
		MME_TransformerHandle_t *handlep)
{
	unsigned int i;
	MME_ERROR Status;

	if ((!name) || (strncmp(name, Name, 128) != 0))
		return MME_UNKNOWN_TRANSFORMER;

	for (i = 0; i < MAX_HANDLES; i++)
		if (!TransformContexts[i].Open)
			break;

	if (i >= MAX_HANDLES)
		return MME_NOMEM;

	Status = InitFunction(params->TransformerInitParamsSize,
			params->TransformerInitParams_p,
			&TransformContexts[i].Context);

	if (Status == MME_SUCCESS) {
		if (TransformContexts[i].Context == NULL)
			return MME_INTERNAL_ERROR;

		TransformContexts[i].Open = true;
		TransformContexts[i].Callback = params->Callback;
		TransformContexts[i].CallbackUserData
		= params->CallbackUserData;

		*handlep = i;
	}

	return Status;
}

MME_ERROR MME_SendCommand(MME_TransformerHandle_t handle,
		MME_Command_t *commandInfo)
{
	MME_ERROR Status;
	bool DeferredCommand;

	if (!TransformContexts[handle].Open)
		return MME_INVALID_HANDLE;

	commandInfo->DueTime = handle;
	commandInfo->CmdStatus.CmdId
	= (TransformContexts[handle].NextCommandId++) & 0xff;

	Status = ProcessCommandFunction(TransformContexts[handle].Context,
			commandInfo);

	/* Now do we issue a callback */

	DeferredCommand = (Status == MME_TRANSFORM_DEFERRED);

	if ((TransformContexts[handle].Callback != NULL)
			&& (commandInfo->CmdEnd
					== MME_COMMAND_END_RETURN_NOTIFY)
					&& !DeferredCommand) {
		TransformContexts[handle].Callback(MME_COMMAND_COMPLETED_EVT,
				commandInfo,
				TransformContexts[handle].CallbackUserData);
	}

	/* Should we record this command */

	if (DeferredCommand) {
		pr_debug("MME_SendCommand handle=%p %d\n",
				(void *) commandInfo->DueTime,
				commandInfo->DueTime);
		TransformContexts[handle].OutstandingCommands[commandInfo->CmdStatus.CmdId]
			= commandInfo;
	}

	return Status;
}

MME_ERROR MME_AbortCommand(MME_TransformerHandle_t handle,
		MME_CommandId_t cmdId)
{
	MME_ERROR Status;

	if (!TransformContexts[handle].Open)
		return MME_INVALID_HANDLE;

	if (!inrange(cmdId, 0, 255)
		|| (TransformContexts[handle].OutstandingCommands[cmdId]
			== NULL))
		return MME_INVALID_ARGUMENT;

	Status = AbortFunction(TransformContexts[handle].Context, cmdId);

	if (Status == MME_SUCCESS)
		TransformContexts[handle].OutstandingCommands[cmdId] = NULL;

	return Status;
}

MME_ERROR MME_TermTransformer(MME_TransformerHandle_t handle)
{
	unsigned int i;
	MME_ERROR Status;

	if (!TransformContexts[handle].Open)
		return MME_INVALID_HANDLE;

	for (i = 0; i < 256; i++)
		if (TransformContexts[handle].OutstandingCommands[i] != NULL)
			return MME_COMMAND_STILL_EXECUTING;

	Status = TerminateFunction(TransformContexts[handle].Context);
	if (Status == MME_SUCCESS)
		memset(&TransformContexts[handle], 0x00,
				sizeof(TransformContext_t));

	return Status;
}

MME_ERROR MME_RegisterMemory(MME_TransformerHandle_t handle, void *base,
		MME_SIZE size, MME_MemoryHandle_t *handlep)
{
	return MME_SUCCESS;
}
