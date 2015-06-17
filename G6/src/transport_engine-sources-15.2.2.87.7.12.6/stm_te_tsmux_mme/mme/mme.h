/**************************************************************
 * Copyright (C) 2010   STMicroelectronics. All Rights Reserved.
 * This file is part of the latest release of the Multicom4 project.
 *
 * This release is fully functional and provides all of the original
 * MME functionality.This release  is now considered stable and ready
 * for integration with other software components.

 * Multicom4 is a free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation version 2.

 * Multicom4 is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with Multicom4; see the file COPYING.  If not, write to the
 * Free Software Foundation, 59 Temple Place - Suite 330, Boston,
 * MA 02111-1307, USA.

 * Written by Multicom team at STMicroelectronics in November 2010.
 * Contact multicom.support@st.com.
**************************************************************/

/*
 *
 */

#ifndef _MME_H
#define _MME_H

#ifdef __cplusplus
extern "C" {
#endif	/* __cplusplus */

/* Constants */

#define MME_MAX_TRANSFORMER_NAME	(63U)

#define MME_MAX_CACHE_LINE		(32)
#define MME_CACHE_LINE_ALIGN(X)		((void *)(((unsigned) (X) + \
		(MME_MAX_CACHE_LINE-1)) & ~(MME_MAX_CACHE_LINE-1)))
#define MME_CACHE_LINE_ALIGNED(X)	(((unsigned) (X) & \
		(MME_MAX_CACHE_LINE-1)) == 0)

/* Aliased names for some of the inconsistently named (or simply overly long)
 * identifiers */

#define MME_Deinit			MME_Term
#define MME_Error_t			MME_ERROR
#define MME_NO_MEMORY			MME_NOMEM
#define MME_SET_GLOBAL_TRANSFORM_PARAMS	MME_SET_PARAMS
#define MME_EMBX_ERROR			MME_ICS_ERROR

/* Version info */

#define __MULTICOM_VERSION__		"4.0.51 : Orly"
#define __MULTICOM_VERSION_MAJOR__	(4)
#define __MULTICOM_VERSION_MINOR__	(0)
#define __MULTICOM_VERSION_PATCH__	(51)

/* Version macros like the ones found in Linux */
#define MULTICOM_VERSION(a, b, c)	(((a) << 16) + ((b) << 8) + (c))
#define MULTICOM_VERSION_CODE	MULTICOM_VERSION( \
					__MULTICOM_VERSION_MAJOR__, \
					__MULTICOM_VERSION_MINOR__, \
					__MULTICOM_VERSION_PATCH__)

/* Enumerations */

/* MME_AllocationFlags_t:
 * Used to qualify MME_DataBuffer_t memory allocation
 */
typedef enum MME_AllocationFlags_t {
	MME_ALLOCATION_PHYSICAL = 1,
	MME_ALLOCATION_CACHED   = 2,
	MME_ALLOCATION_UNCACHED = 4
} MME_AllocationFlags_t;

/* MME_CommandCode_t:
 * Used to specify a command to a transformer
 */
typedef enum MME_CommandCode_t {
	MME_SET_PARAMS,
	MME_TRANSFORM,
	MME_SEND_BUFFERS,
	MME_PING				/* MME4 API extension */
} MME_CommandCode_t;

/* MME_CommandState_t:
 * Used to specify the current state of a submitted command
 */
typedef enum MME_CommandState_t {
	MME_COMMAND_IDLE,
	MME_COMMAND_PENDING,
	MME_COMMAND_EXECUTING,
	MME_COMMAND_COMPLETED,
	MME_COMMAND_FAILED
} MME_CommandState_t;

/* MME_CommandEndType_t:
 * Used to specify whether the Client callback or Wake
 * should be actioned on command completion
 *
 * MME_COMMAND_END_RETURN_NO_INFO - No Callback/Wake
 * MME_COMMAND_END_RETURN_NOTIFY  - Call Client callback
 * MME_COMMAND_END_RETURN_WAKE    - Wakeup MME_Wait() call
 */
typedef enum MME_CommandEndType_t {
	MME_COMMAND_END_RETURN_NO_INFO,
	MME_COMMAND_END_RETURN_NOTIFY,
	MME_COMMAND_END_RETURN_WAKE		/* MME4 API extension */
} MME_CommandEndType_t;

/* MME_Event_t:
 * Used to indicate a transform event type to the Client callback
 */
typedef enum MME_Event_t {
	MME_COMMAND_COMPLETED_EVT,
	MME_DATA_UNDERFLOW_EVT,
	MME_NOT_ENOUGH_MEMORY_EVT,
	MME_NEW_COMMAND_EVT,
	MME_TRANSFORMER_TIMEOUT
} MME_Event_t;


/* MME_ERROR:
 * The errors that may be returned by MME API functions or by
 * transformer entry point functions
 */
typedef enum MME_ERROR {
	MME_SUCCESS                     = 0,
	MME_DRIVER_NOT_INITIALIZED      = 1,
	MME_DRIVER_ALREADY_INITIALIZED  = 2,
	MME_NOMEM                       = 3,
	MME_INVALID_TRANSPORT           = 4,	/* DEPRECATED */
	MME_INVALID_HANDLE              = 5,
	MME_INVALID_ARGUMENT            = 6,
	MME_UNKNOWN_TRANSFORMER         = 7,
	MME_TRANSFORMER_NOT_RESPONDING  = 8,
	MME_HANDLES_STILL_OPEN          = 9,
	MME_COMMAND_STILL_EXECUTING     = 10,
	MME_COMMAND_ABORTED             = 11,
	MME_DATA_UNDERFLOW              = 12,
	MME_DATA_OVERFLOW               = 13,
	MME_TRANSFORM_DEFERRED          = 14,
	MME_SYSTEM_INTERRUPT            = 15,
	MME_ICS_ERROR                   = 16,
	MME_INTERNAL_ERROR              = 17,
	MME_NOT_IMPLEMENTED             = 18,
	MME_COMMAND_TIMEOUT             = 19	/* MME4 API extension */
} MME_ERROR;

/* MME_CacheFlags_t
 * Cache management flags used allow optimizations to be applied
 * for each MME_ScatterPage_t (FlagsIn/FlagsOut)
 */
typedef enum MME_CacheFlags_t {
	MME_DATA_CACHE_COHERENT   = (1 << 31),
	/* In/Out: Memory/Page is already coherent with cache */
	MME_DATA_TRANSIENT        = (1 << 30),
	/* In: Cache flush is not necessary in Companion */
	MME_REMOTE_CACHE_COHERENT = (1 << 29),
	/* Out: Remote Page translation should be COHERENT */
	MME_DATA_PHYSICAL         = (1 << 28)
	/* In/Out: Memory address is Physical don't translate */
} MME_CacheFlags_t;

/* MME_Tuneable_t:
 * Keys used to modify tuneable parameters
 */
typedef enum {
	/* no need to MME_THREAD_STACK_SIZE, this is managed by the ICS call */
	MME_TUNEABLE_MANAGER_THREAD_PRIORITY,
	MME_TUNEABLE_TRANSFORMER_THREAD_PRIORITY,
	MME_TUNEABLE_EXECUTION_LOOP_HIGHEST_PRIORITY,
	MME_TUNEABLE_EXECUTION_LOOP_ABOVE_NORMAL_PRIORITY,
	MME_TUNEABLE_EXECUTION_LOOP_NORMAL_PRIORITY,
	MME_TUNEABLE_EXECUTION_LOOP_BELOW_NORMAL_PRIORITY,
	MME_TUNEABLE_EXECUTION_LOOP_LOWEST_PRIORITY,
	/* TIMEOUT SUPPORT - allow default timeout to be modified */
	MME_TUNEABLE_TRANSFORMER_TIMEOUT,
	MME_TUNEABLE_BUFFER_POOL_SIZE,	/* MME4 API Extension */
	MME_TUNEABLE_COMMAND_TIMEOUT,		/* MME4 API Extension */

	MME_TUNEABLE_MAX
} MME_Tuneable_t;

/* MME_Priority_t:
 * The MME scheduler priority at which a transform is executed
 */
typedef enum MME_Priority_t {
	MME_PRIORITY_HIGHEST      = 5000,
	MME_PRIORITY_ABOVE_NORMAL = 4000,
	MME_PRIORITY_NORMAL       = 3000,
	MME_PRIORITY_BELOW_NORMAL = 2000,
	MME_PRIORITY_LOWEST       = 1000
} MME_Priority_t;

/* Types: simple */

/* CPU architecture-specific */

#if defined(__sh__) || defined(__st200__) || defined(__ILP32__) || defined(__arm__)
/* SH4/ST40 */
typedef unsigned int  MME_UINT;
typedef unsigned long MME_ULONG;
typedef unsigned long MME_SIZE;
typedef double        MME_GENERIC64;
#define               MME_MaxTime_c 0xffffffff
#else
#error Unsupported CPU type
#endif

/* TIMEOUT SUPPORT: Maximum timeout value which basically means don't timeout */
#define MME_TIMEOUT_INFINITE	MME_MaxTime_c

/* Architecture neutral */
/* MME_Time_t:
 * MME time
 */
typedef MME_UINT MME_Time_t;

/* MME_GenericParams_t:
 * A transformer specific parameter
 */
typedef void *MME_GenericParams_t;

/* MME_CommandId_t:
 * The identifier assigned by MME to a submitted command
 */
typedef MME_UINT MME_CommandId_t;

/* MME_TransformerHandle_t:
 * The handle of a transformer instance assigned by MME
 */
typedef MME_UINT MME_TransformerHandle_t;

/* MME_MemoryHandle_t:
 * The handle of a registered memory region assigned by MME
 */
typedef MME_UINT MME_MemoryHandle_t;

/* Types: structures */

/* MME_CommandStatus_t:
 * Structure containing state associated with a command
 */
typedef struct MME_CommandStatus_t {
	MME_CommandId_t		CmdId;
	MME_CommandState_t	State;
	MME_Time_t		ProcessedTime;
	MME_ERROR		Error;
	MME_UINT		AdditionalInfoSize;
	MME_GenericParams_t	AdditionalInfo_p;
} MME_CommandStatus_t;

/* MME_ScatterPage_t:
 * Structure describing a single scatter page - see MME_DataBuffer_t
 */
typedef struct MME_ScatterPage_t {
	void		*Page_p;
	MME_UINT	Size;
	MME_UINT	BytesUsed;
	MME_UINT	FlagsIn;
	MME_UINT	FlagsOut;
} MME_ScatterPage_t;

/* MME_DataBuffer_t:
 * Structure describing a data buffer
 */
typedef struct MME_DataBuffer_t {
	MME_UINT		StructSize;
	void			*UserData_p;
	MME_UINT		Flags;
	MME_UINT		StreamNumber;
	MME_UINT		NumberOfScatterPages;
	MME_ScatterPage_t	*ScatterPages_p;
	MME_UINT		TotalSize;
	MME_UINT		StartOffset;
} MME_DataBuffer_t;

/* MME_Command_t:
 * Structure to pass a command to a transformer
 */
typedef struct MME_Command_t {
	MME_UINT		StructSize;
	MME_CommandCode_t	CmdCode;
	MME_CommandEndType_t	CmdEnd;
	MME_Time_t		DueTime;
	MME_UINT		NumberInputBuffers;
	MME_UINT		NumberOutputBuffers;
	MME_DataBuffer_t	**DataBuffers_p;
	MME_CommandStatus_t	CmdStatus;
	MME_UINT		ParamSize;
	MME_GenericParams_t	Param_p;
} MME_Command_t;

/* MME_DataFormat_t:
 * Structure describing the data format that a transformer supports
 */
typedef struct MME_DataFormat_t {
	unsigned char	FourCC[4];
} MME_DataFormat_t;


/* MME_TransformerCapability_t:
 * Structure containing information pertaining to a transformer's capability
 */
typedef struct MME_TransformerCapability_t {
	MME_UINT		StructSize;
	MME_UINT		Version;
	MME_DataFormat_t	InputType;
	MME_DataFormat_t	OutputType;
	MME_UINT		TransformerInfoSize;
	MME_GenericParams_t	TransformerInfo_p;
} MME_TransformerCapability_t;


/* Transformer Client callback function */
typedef void (*MME_GenericCallback_t) (MME_Event_t event,
		MME_Command_t *callbackData, void *userData);

/* MME_TransformerInitParams_t:
 * The parameters with which to initialize a transformer
 */
typedef struct MME_TransformerInitParams_t {
	MME_UINT		StructSize;
	MME_Priority_t		Priority;
	MME_GenericCallback_t	Callback;
	void			*CallbackUserData;
	MME_UINT		TransformerInitParamsSize;
	MME_GenericParams_t	TransformerInitParams_p;
} MME_TransformerInitParams_t;

/* Types: functions */

/* Transformer: AbortCommand entry point */
typedef MME_ERROR (*MME_AbortCommand_t)
		(void *context, MME_CommandId_t commandId);

/* Transformer: GetTransformerCapability entry point */
typedef MME_ERROR (*MME_GetTransformerCapability_t)
		(MME_TransformerCapability_t *capability);

/* Transformer: InitTransformer entry point */
typedef MME_ERROR (*MME_InitTransformer_t)
		(MME_UINT initParamsLength, MME_GenericParams_t initParams,
				void **context);

/* Transformer: ProcessCommand entry point */
typedef MME_ERROR (*MME_ProcessCommand_t)
		(void *context, MME_Command_t *commandInfo);

/* Transformer: TermTransformer entry point */
typedef MME_ERROR (*MME_TermTransformer_t) (void *context);

/* Macros */

/* These macros are shown in their most simplistic form. On mixed endian
 * systems these macros can use (sizeof(field) == n) if networks to cope.
 * Since such a network can be solved at compile time this would not impact
 * runtime performance.
 */

/* MME_PARAM has a different implementation of machines with a different
 * endian. This permits the MME implementation on a big endian machine
 * to perform a 64-bit byte swap
 */

#if defined(__sh__) || defined(__st200__) || defined(__LITTLE_ENDIAN__) || defined(__arm__)
/* this is the definition of MME_PARAM for little endian machines */
#define _MME_PARAM_ADDRESS(p, f) ((MME_TYPE_##f*)(((MME_GENERIC64*)p)+\
		MME_OFFSET_##f))

#else
/* this is the definition of MME_PARAM for big endian machines */
#define _MME_PARAM_ADDRESS(p, f) \
		((MME_TYPE_##f*)(((char *)((MME_GENERIC64*)p+MME_OFFSET_##f))\
				+(sizeof(MME_GENERIC64)-sizeof(MME_TYPE_##f))))

/* At the moment we do not support BE machines */
#error Unsupported CPU type
#endif

/* A parameter */
#define MME_PARAM(p, f)            (*_MME_PARAM_ADDRESS(p, f))

/* A parameter sublist */
#define MME_PARAM_SUBLIST(p, f)    _MME_PARAM_ADDRESS(p, f)

/* Access a parameter at a given index */
#define MME_INDEXED_PARAM(p, f, i) (*((MME_TYPE_##f*)(((MME_GENERIC64*)p)+\
					MME_OFFSET_##f+(i))))

/* Get number of paramters */
#define MME_LENGTH(id)             (MME_LENGTH_##id)

/* Get number of paramters */
#define MME_LENGTH_BYTES(id)       ((MME_LENGTH_##id)*sizeof(MME_GENERIC64))

/* Function declarations */

/* MME_AbortCommand()
 * Attempt to abort a submitted command
 *
 * Asynchronous operation whose success will be observed in the CmdStatus of
 * the aborted command
 */
MME_ERROR MME_AbortCommand(MME_TransformerHandle_t handle,
	MME_CommandId_t cmdId);

/* MME_KillCommand()
 * Abort a command without communicating with the remote processor.
 * Should only be used when we know the transformer processor has definitely
 * crashed
 */
MME_ERROR MME_KillCommand(MME_TransformerHandle_t handle,
	MME_CommandId_t cmdId);

/* MME_KillCommandAll()
 * Abort all commands without communicating with the remote processor.
 * Should only be used when we know the transformer processor has definitely
 * crashed
 */
MME_ERROR MME_KillCommandAll(MME_TransformerHandle_t handle);

/* MME_AllocDataBuffer()
 * Allocate a data buffer that is optimal for the transformer instantiation
 * to pass between a Client and companion
 */
MME_ERROR MME_AllocDataBuffer(MME_TransformerHandle_t handle,
			       MME_UINT size,
			       MME_AllocationFlags_t flags,
			       MME_DataBuffer_t **dataBuffer_p);

/* MME_DeregisterTransformer()
 * Deregister a transformer that has been registered
 */
MME_ERROR MME_DeregisterTransformer(const char *name);

/* MME_DeregisterTransport()
 * Deregister an EMBX transport being used by MME
 *
 * DEPRECATED
 */
MME_ERROR MME_DeregisterTransport(const char *name);

/* MME_FreeDataBuffer()
 * Free a buffer previously allocated with MME_AllocDataBuffer
 */
MME_ERROR MME_FreeDataBuffer(MME_DataBuffer_t *DataBuffer);

/* MME_GetTransformerCapability()
 * Obtain the capabilities of a transformer
 */
MME_ERROR MME_GetTransformerCapability(const char *name,
				MME_TransformerCapability_t *capability);

/* MME_Init()
 * Initialize MME
 */
MME_ERROR MME_Init(void);

/* MME_InitTransformer()
 * Create a transformer instance on a companion
 */
MME_ERROR MME_InitTransformer(const char *name,
			       MME_TransformerInitParams_t *params,
			       MME_TransformerHandle_t *handlep);

/* MME_ModifyTuneable()
 * Modify system wide configuration parameters such as thread priority.
 */
MME_ERROR MME_ModifyTuneable(MME_Tuneable_t key, MME_UINT value);

/* MME_GetTuneable()
 * Returns current value of system wide configuration parameters such as
 * thread priority.
 */
MME_UINT MME_GetTuneable(MME_Tuneable_t key);

/* MME_NotifyHost()
 * Notify the Command Client that a transformer event has occurred
 */
MME_ERROR MME_NotifyHost(MME_Event_t event, MME_Command_t *commandInfo,
	MME_ERROR res);

/* MME_RegisterTransformer()
 * Register a transformer after which instantiations may be made
 */
MME_ERROR MME_RegisterTransformer(const char *name,
		MME_AbortCommand_t abortFunc,
		MME_GetTransformerCapability_t getTransformerCapabilityFunc,
		MME_InitTransformer_t initTransformerFunc,
		MME_ProcessCommand_t processCommandFunc,
		MME_TermTransformer_t termTransformerFunc);

/* MME_RegisterTransport()
 * Register an existing EMBX transport for use by MME
 *
 * DEPRECATED
 */
MME_ERROR MME_RegisterTransport(const char *name);

/* MME_Run()
 * Run the MME message loop on a companion CPU
 *
 * DEPRECATED
 */
MME_ERROR MME_Run(void);

/* MME_SendCommand()
 * Send a transformer command to a transformer instance
 */
MME_ERROR MME_SendCommand(MME_TransformerHandle_t handle,
	MME_Command_t *commandInfo);

/* MME_Term()
 * Terminate MME on the local CPU.
 */
MME_ERROR MME_Term(void);

/* MME_TermTransformer()
 * Terminate a transformer instance
 */
MME_ERROR MME_TermTransformer(MME_TransformerHandle_t handle);

/* MME_KillTransformer()
 * Terminate a transformer instance, without communicating
 * with the remote processor, which we assume has crashed
 */
MME_ERROR MME_KillTransformer(MME_TransformerHandle_t handle);

/* MME_IsStillAlive()
 * Ping a remote transformer to see if it's still running
 * Sets alive to be non zero if it replies, zero otherwise
 */
MME_ERROR MME_IsStillAlive(MME_TransformerHandle_t handle,
	MME_UINT *alive);

/*
 * Multihost extensions - DEPRECATED
 */
MME_ERROR MME_HostRegisterTransport(const char *name);
MME_ERROR MME_HostDeregisterTransport(const char *name);
MME_ERROR MME_HostInit(void);
MME_ERROR MME_HostTerm(void);

/*
 * MME4 New APIs
 */

/*
 * Return a pointer to the MME version string
 *
 * This string takes the form:
 *
 *   {major number}.{minor number}.{patch number} [text]
 *
 * That is, a major, minor and release number, separated by
 * decimal points, and optionally followed by a space and a text string.
 */
const char *MME_Version(void);

/* MME_WaitCommand()
 * Block waiting for an issued Command to complete. Command must have been
 * issued using the MME_COMMAND_END_RETURN_WAKE CmdEnd notification type
 *
 * Returns MME_SUCCESS when transformation has completed and CmdStatus has
 * been updated
 * Corresponding MME_Event status is updated via eventp
 *
 * Can return MME_COMMAND_TIMEOUT, MME_SYSTEM_INTERRUPT or MME_ICS_ERROR
 */
MME_ERROR MME_WaitCommand(MME_TransformerHandle_t handle,
			MME_CommandId_t cmdId,
			MME_Event_t *eventp,
			MME_Time_t timeout);

/* MME_PingTransformer()
 * Ping a remote transformer to see if it's still running
 * Waits for timeout period for response before returning an error
 */
MME_ERROR MME_PingTransformer(MME_TransformerHandle_t handle,
		MME_Time_t timeout);

/*
 * Register a memory region for use with the specified transformer instance
 *
 * Only data buffers which lie within the specified region can be used
 * during transform operations
 *
 */
MME_ERROR MME_RegisterMemory(MME_TransformerHandle_t handle,
			void *base,
			MME_SIZE size,
			MME_MemoryHandle_t *handlep);

MME_ERROR MME_DeregisterMemory(MME_MemoryHandle_t handle);


#include <mme/mme_debug.h>		/* Debug logging */


#ifdef __cplusplus
}
#endif	/* __cplusplus */
#endif	/* _MME_H */

/*
 * Local Variables:
 *  tab-width: 8
 *  c-indent-level: 2
 *  c-basic-offset: 2
 * End:
 */
