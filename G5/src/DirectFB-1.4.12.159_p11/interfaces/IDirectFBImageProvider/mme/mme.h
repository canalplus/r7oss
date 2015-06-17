/*******************************************************************/
/* Copyright 2004 STMicroelectronics R&D Ltd. All rights reserved. */
/*                                                                 */
/* File: mme.h                                                     */
/*                                                                 */
/* Description:                                                    */
/*         Public export header for the MME API.                   */
/*                                                                 */
/*******************************************************************/

#ifndef _MME_H
#define _MME_H

#ifdef __cplusplus
extern "C" {
#endif				/* __cplusplus */

/* Constants */

#define MME_MAX_TRANSFORMER_NAME ((unsigned int) 128)

#define MME_MAX_CACHE_LINE		32
#define MME_CACHE_LINE_ALIGN(X)		((void*)(((unsigned) (X) + (MME_MAX_CACHE_LINE-1)) & ~(MME_MAX_CACHE_LINE-1)))
#define MME_CACHE_LINE_ALIGNED(X)	(((unsigned) (X) & (MME_MAX_CACHE_LINE-1)) == 0)

/* Aliased names for some of the inconstantly named (or simply overly long) identifiers */

#define MME_Deinit     MME_Term
#define MME_Error_t    MME_ERROR
#define MME_NO_MEMORY  MME_NOMEM
#define MME_SET_PARAMS MME_SET_GLOBAL_TRANSFORM_PARAMS

/* Version info */

#define __MULTICOM_VERSION__          "multicom : 3.2.4 (RC3)"
#define __MULTICOM_VERSION_MAJOR__    (3)
#define __MULTICOM_VERSION_MINOR__    (2)
#define __MULTICOM_VERSION_PATCH__    (4)

/* Version macros like the ones found in Linux */
#define MULTICOM_VERSION(a,b,c)       (((a) << 16) + ((b) << 8) + (c))
#define MULTICOM_VERSION_CODE	      MULTICOM_VERSION(__MULTICOM_VERSION_MAJOR__, __MULTICOM_VERSION_MINOR__, __MULTICOM_VERSION_PATCH__)

/* Enumerations */

/* MME_AllocationFlags_t: 
 * Used to qualify memory allocation
 */
typedef enum MME_AllocationFlags_t {
	MME_ALLOCATION_PHYSICAL = 1,
	MME_ALLOCATION_CACHED = 2,
	MME_ALLOCATION_UNCACHED = 4
} MME_AllocationFlags_t;

/* MME_CommandCode_t: 
 * Used to specify a command to a transfomer
 */
typedef enum MME_CommandCode_t {
	MME_SET_GLOBAL_TRANSFORM_PARAMS,
	MME_TRANSFORM,
	MME_SEND_BUFFERS
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
 * Used to specify whether the host callback should be
 * called on command completion
 */
typedef enum MME_CommandEndType_t {
	MME_COMMAND_END_RETURN_NO_INFO,
	MME_COMMAND_END_RETURN_NOTIFY
} MME_CommandEndType_t;

/* MME_Event_t:
 * Used to indicate a transform event type to the host callback
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
	MME_SUCCESS = 0,
	MME_DRIVER_NOT_INITIALIZED,
	MME_DRIVER_ALREADY_INITIALIZED,
	MME_NOMEM,
	MME_INVALID_TRANSPORT,
	MME_INVALID_HANDLE,
	MME_INVALID_ARGUMENT,
	MME_UNKNOWN_TRANSFORMER,
	MME_TRANSFORMER_NOT_RESPONDING,
	MME_HANDLES_STILL_OPEN,
	MME_COMMAND_STILL_EXECUTING,
	MME_COMMAND_ABORTED,
	MME_DATA_UNDERFLOW,
	MME_DATA_OVERFLOW,
	MME_TRANSFORM_DEFERRED,
	MME_SYSTEM_INTERRUPT,
        MME_EMBX_ERROR,
	MME_INTERNAL_ERROR,
        MME_NOT_IMPLEMENTED
} MME_ERROR;

/* MME_CacheFlags_t
 * Cache management flags used allow domain specific communication optimizations to be applied
 */
typedef enum MME_CacheFlags_t {
	MME_DATA_CACHE_COHERENT = (int) ((unsigned) 1 << 31),
	MME_DATA_TRANSIENT = (1 << 30),
	MME_REMOTE_CACHE_COHERENT = (1 << 29),
	MME_DATA_PHYSICAL = (1 << 28)
} MME_CacheFlags_t;

/* MME_Tuneable_t:
 * Keys used to modify tuneable parameters
 */
typedef enum {
	/* no need to MME_THREAD_STACK_SIZE, this is managed by the EMBX call */
	MME_TUNEABLE_MANAGER_THREAD_PRIORITY,
	MME_TUNEABLE_TRANSFORMER_THREAD_PRIORITY,
	MME_TUNEABLE_EXECUTION_LOOP_HIGHEST_PRIORITY,
	MME_TUNEABLE_EXECUTION_LOOP_ABOVE_NORMAL_PRIORITY,
	MME_TUNEABLE_EXECUTION_LOOP_NORMAL_PRIORITY,
	MME_TUNEABLE_EXECUTION_LOOP_BELOW_NORMAL_PRIORITY,
	MME_TUNEABLE_EXECUTION_LOOP_LOWEST_PRIORITY,
	/* TIMEOUT SUPPORT - allow default timeout to be modified */
	MME_TUNEABLE_TRANSFORMER_TIMEOUT,

	MME_TUNEABLE_MAX
} MME_Tuneable_t;
		
/* MME_Priority_t:
 * The MME scheduler priority at which a transform is executed
 */
typedef enum MME_Priority_t {
	MME_PRIORITY_HIGHEST = 5000,
	MME_PRIORITY_ABOVE_NORMAL = 4000,
	MME_PRIORITY_NORMAL = 3000,
	MME_PRIORITY_BELOW_NORMAL = 2000,
	MME_PRIORITY_LOWEST = 1000
} MME_Priority_t;

/* Types: simple */

/* CPU architecture-specfic */

#if defined __SH4__ || defined __sh__
/* SH4/ST40 */
typedef unsigned int MME_UINT;
typedef double       MME_GENERIC64;
#define              MME_MaxTime_c 0xffffffff
#elif defined __CORE__
/* ST20 */
typedef unsigned int MME_UINT;
typedef double       MME_GENERIC64;
#define              MME_MaxTime_c 0xffffffff
#elif defined __ST200__ || defined __st200__
/* ST200 */
typedef unsigned int MME_UINT;
typedef double       MME_GENERIC64;
#define              MME_MaxTime_c 0xffffffff
#elif defined __IA32__ || defined __i386__
typedef unsigned int MME_UINT;
typedef double       MME_GENERIC64;
#define              MME_MaxTime_c 0xffffffff
#else
#error Unsupported CPU type
#endif

/* TIMEOUT SUPPORT: Maximum timeout value which basically means don't timeout */
#define MME_TIMEOUT_INFINITE	MME_MaxTime_c

/* Arhitecture neutral */
/* MME_Time_t: 
 * MME time
 */
typedef MME_UINT MME_Time_t;

/* MME_GenericParams_t: 
 * A transformer specific parameter
 */
typedef void* MME_GenericParams_t;

/* MME_CommandId_t: 
 * The identifier assigned by MME to a submitted command
 */
typedef MME_UINT MME_CommandId_t;

/* MME_TransformerHandle_t: 
 * The handle of a transformer instance assigned by MME
 */
typedef MME_UINT MME_TransformerHandle_t;

/* Types: structures */

/* MME_CommandStatus_t:
 * Structure containing state associated with a command
 */
typedef struct MME_CommandStatus_t {
	MME_CommandId_t      CmdId;
	MME_CommandState_t   State;
	MME_Time_t           ProcessedTime;
	MME_ERROR            Error;
	MME_UINT             AdditionalInfoSize;
	MME_GenericParams_t  AdditionalInfo_p;
} MME_CommandStatus_t;

/* MME_ScatterPage_t:
 * Structure describing a single scatter page - see MME_DataBuffer_t
 */
typedef struct MME_ScatterPage_t {
	void*    Page_p;
	MME_UINT Size;
	MME_UINT BytesUsed;
	MME_UINT FlagsIn;
	MME_UINT FlagsOut;
} MME_ScatterPage_t;

/* MME_DataBuffer_t:
 * Structure describing a data buffer
 */
typedef struct MME_DataBuffer_t {
	MME_UINT              StructSize;
	void*                 UserData_p;
	MME_UINT              Flags;
	MME_UINT              StreamNumber;
	MME_UINT              NumberOfScatterPages;
	MME_ScatterPage_t*    ScatterPages_p;
	MME_UINT              TotalSize;
	MME_UINT              StartOffset;
} MME_DataBuffer_t;

/* MME_Command_t:
 * Structure to pass a command to a transformer
 */
typedef struct MME_Command_t {
	MME_UINT                  StructSize;
	MME_CommandCode_t         CmdCode;
	MME_CommandEndType_t      CmdEnd;
	MME_Time_t                DueTime;
	MME_UINT                  NumberInputBuffers;
	MME_UINT                  NumberOutputBuffers;
	MME_DataBuffer_t**        DataBuffers_p;
	MME_CommandStatus_t	  CmdStatus;
	MME_UINT                  ParamSize;
	MME_GenericParams_t       Param_p;
} MME_Command_t;

/* MME_DataFormat_t:
 * Structure describing the data format that a transformer supports
 */
typedef struct MME_DataFormat_t {
	unsigned char FourCC[4];
} MME_DataFormat_t;


/* MME_TransformerCapability_t:
 * Structure containg information pertaining to a transformers capability
 */
typedef struct MME_TransformerCapability_t {
	MME_UINT             StructSize;
	MME_UINT             Version;
	MME_DataFormat_t     InputType;
	MME_DataFormat_t     OutputType;
	MME_UINT             TransformerInfoSize;
	MME_GenericParams_t  TransformerInfo_p;
} MME_TransformerCapability_t;


/* Host callback function */
typedef void (*MME_GenericCallback_t) (MME_Event_t Event, MME_Command_t * CallbackData, void *UserData);

/* MME_TransformerInitParams_t:
 * The paramters with which to initialize a transformer
 */
typedef struct MME_TransformerInitParams_t {
	MME_UINT		StructSize;
	MME_Priority_t		Priority;
	MME_GenericCallback_t	Callback;
	void*			CallbackUserData;
	MME_UINT		TransformerInitParamsSize;
	MME_GenericParams_t	TransformerInitParams_p;
} MME_TransformerInitParams_t;

/* Types: functions */

/* Transformer: AbortCommand entry point */
typedef MME_ERROR (*MME_AbortCommand_t) (void *context, MME_CommandId_t commandId);

/* Transformer: GetTransformerCapability entry point */
typedef MME_ERROR (*MME_GetTransformerCapability_t) (MME_TransformerCapability_t * capability);

/* Transformer: InitTransformer entry point */
typedef MME_ERROR (*MME_InitTransformer_t) (MME_UINT initParamsLength, MME_GenericParams_t initParams, void **context);

/* Transformer: ProcessCommand entry point */
typedef MME_ERROR (*MME_ProcessCommand_t) (void *context, MME_Command_t * commandInfo);

/* Transformer: TermTransformer entry point */
typedef MME_ERROR (*MME_TermTransformer_t) (void *context);

/* Macros */

/* These macros are shown in their most simplistic form. On mixed endian   
 * systems these macros can use (sizeof(field) == n) if networks to cope.
 * Since such a network can be solved at compile time this would not impact
 * runtime performance.
 */

/* MME_PARAM has a different implementation of machines with a different
 * endiannes. This permits the MME implementation on a big endian machine
 * to perform a 64-bit byte swap
 */

#if defined (__CORE__) || defined(__SH4__) || defined(__ST200__) || defined(__IA32__) || defined(__i386__)
/* this is the definition of MME_PARAM for little endian machines */
#define _MME_PARAM_ADDRESS(p, f)   ((MME_TYPE_##f*)(((MME_GENERIC64*)p)+MME_OFFSET_##f))

#else
/* this is the definition of MME_PARAM for big endian machines */
#define _MME_PARAM_ADDRESS(p, f)   \
        ((MME_TYPE_##f *)(((char *)((MME_GENERIC64*)p+MME_OFFSET_##f))+(sizeof(MME_GENERIC64)-sizeof(MME_TYPE_##f))))

/* At the moment we do not support BE machines */
#error Unsupported CPU type
#endif

/* A parameter */
#define MME_PARAM(p, f)            (*_MME_PARAM_ADDRESS(p, f))

/* A parameter sublist */
#define MME_PARAM_SUBLIST(p, f)    _MME_PARAM_ADDRESS(p, f)

/* Access a parameter at a given index */
#define MME_INDEXED_PARAM(p, f, i) (*((MME_TYPE_##f*)(((MME_GENERIC64*)p)+MME_OFFSET_##f+(i))))

/* Get number of paramters */
#define MME_LENGTH(id)             (MME_LENGTH_##id)

/* Get number of paramters */
#define MME_LENGTH_BYTES(id)       ((MME_LENGTH_##id)*sizeof(MME_GENERIC64))

/* Function declarations */

/* MME_AbortCommand()
 * Attempt to abort a submitted command 
 */
MME_ERROR MME_AbortCommand(MME_TransformerHandle_t Handle, MME_CommandId_t CmdId);

/* MME_KillCommand()
 * Abort a command without communicating with the remote processor. Should only be used 
 * when we know the transformer processor has definitely crashed
 */
MME_ERROR MME_KillCommand(MME_TransformerHandle_t Handle, MME_CommandId_t CmdId);

/* MME_KillCommandAll()
 * Abort all commands without communicating with the remote processor. Should only be used 
 * when we know the transformer processor has definitely crashed
 */
MME_ERROR MME_KillCommandAll(MME_TransformerHandle_t Handle);

/* MME_AllocDataBuffer()
 * Allocate a data buffer that is optimal for the transformer instantiation
 * to pass between a host and companion
 */
MME_ERROR MME_AllocDataBuffer(  MME_TransformerHandle_t handle,
				MME_UINT size,
				MME_AllocationFlags_t flags,
				MME_DataBuffer_t ** dataBuffer_p);

/* MME_DeregisterTransformer()
 * Deregister a transformer that has been registered
 */
MME_ERROR MME_DeregisterTransformer(const char *name);

/* MME_DeregisterTransport()
 * Deregister an EMBX transport being used by MME
 */
MME_ERROR MME_DeregisterTransport(const char *name);

/* MME_FreeDataBuffer()
 * Free a buffer previously allocated with MME_AllocDataBuffer 
 */
MME_ERROR MME_FreeDataBuffer(MME_DataBuffer_t * DataBuffer_p);

/* MME_GetTransformerCapability()
 * Obtain the capabilities of a transformer 
 */
MME_ERROR MME_GetTransformerCapability(const char *TransformerName,
					       MME_TransformerCapability_t * TransformerCapability_p);

/* MME_Init()
 * Initialize MME 
 */
MME_ERROR MME_Init(void);

/* MME_InitTransformer()
 * Create a transformer instance on a companion 
 */
MME_ERROR MME_InitTransformer(const char *Name,
        		      MME_TransformerInitParams_t * Params_p, 
                              MME_TransformerHandle_t * Handle_p);
  
/* MME_InitTransformer()
 * Create a transformer instance on a companion (using Callbacks on the host)
 */
MME_ERROR MME_InitTransformer_Callback(const char *Name,
				       MME_TransformerInitParams_t * Params_p, 
				       MME_TransformerHandle_t * Handle_p);

/* MME_ModifyTuneable()
 * Modify system wide configuration parameters such as thread priority.
 */
MME_ERROR MME_ModifyTuneable(MME_Tuneable_t key, MME_UINT value);

/* MME_NotifyHost()
 * Notify the host that a transformer event has occurred
 */
MME_ERROR MME_NotifyHost(MME_Event_t event, MME_Command_t * commandInfo, MME_ERROR errorCode);

/* MME_RegisterTransformer()
 * Register a transformer after which instantiations may be made
 */
MME_ERROR MME_RegisterTransformer(const char *name,
				  MME_AbortCommand_t abortFunc,
				  MME_GetTransformerCapability_t getTransformerCapabilityFunc,
				  MME_InitTransformer_t initTransformerFunc,
				  MME_ProcessCommand_t processCommandFunc,
				  MME_TermTransformer_t termTransformerFunc);

/* MME_RegisterTransformer_Callback() - using callbacks rather than helper threads
 * Register a transformer after which instantiations may be made
 */
MME_ERROR MME_RegisterTransformer_Callback(const char *name,
					   MME_AbortCommand_t abortFunc,
					   MME_GetTransformerCapability_t getTransformerCapabilityFunc,
					   MME_InitTransformer_t initTransformerFunc,
					   MME_ProcessCommand_t processCommandFunc,
					   MME_TermTransformer_t termTransformerFunc);

/* MME_RegisterTransport()
 * Register an existing EMBX transport for use by MME
 */
MME_ERROR MME_RegisterTransport(const char *name);

/* MME_Run()
 * Run the MME message loop on a companion CPU
 */
MME_ERROR MME_Run(void);

/* MME_SendCommand()
 * Send a transformer command to a transformer instance
 */
MME_ERROR MME_SendCommand(MME_TransformerHandle_t Handle, MME_Command_t * CmdInfo_p);

/* MME_Term()
 * Terminate MME
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
MME_ERROR MME_IsStillAlive(MME_TransformerHandle_t handle, MME_UINT* alive);

/*
 * Multihost extensions
 */
MME_ERROR MME_HostRegisterTransport(const char *name);
MME_ERROR MME_HostDeregisterTransport(const char* name);
MME_ERROR MME_HostInit(void);
MME_ERROR MME_HostTerm(void);

#ifdef __cplusplus
}
#endif				/* __cplusplus */
#endif				/* _MME_H */
