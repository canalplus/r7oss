/************************************************************************
Copyright (C) 2011 STMicroelectronics. All Rights Reserved.

This file is part of the Transport Engine Library.

The Transport Engine is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

Transport Engine is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with Transport Engine; see the file COPYING.  If not, write to the Free
Software Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307,
USA.

The Transport Engine Library may alternatively be licensed under a
proprietary license from ST.

************************************************************************/
/**
   @file   pti_tshal_api.h
   @brief  The PTI TSHAL API for TSInput functionality

   This defines the API for the PTI HAL for controlling TSIN inputs.
   This is the equivalent of stpti.h for HAL.

   The idea is to create a structure of function pointers for every function in the TSHAL.  These are
   populated by constant structures contained in TSHAL at initialation in pti_driver.c.

   When calling the TSHAL you must used the MACRO defined here called... stptiTSHAL_call()
   This will lookup the function required (a function pointer) and once dereferenced it can be
   called.

   This will allow the future expansion when different STFE like devices are created.

 */

#ifndef _PTI_TSHAL_API_H_
#define _PTI_TSHAL_API_H_

/* Includes ---------------------------------------------------------------- */

/* ANSI C includes */

/* STAPI includes */
#include "stddefs.h"

/* Includes from API level */

/* DO NOT INCLUDE stpti.h !!!!
   We must have no dependence on the STAPI API, to remain API neutral, and avoid getting locked into
   backwards compatibility issues.  The STAPI PTI API will need to convert STAPI PTI API types to
   the HAL API types.  pti_stpti.h imports specifically allowed defines from stpti.h (such as
   STPTI_ErrorType_t) */
#include "pti_stpti.h" /* include specifically allowed defines from stpti.h */

/* Includes from the HAL / ObjMan level */
#include "objman/pti_object.h" /* although the object manager is not really needed for the tshal, it holds the Function Pool lookup function */

/* DO NOT INCLUDE object headers !!!!
 * We must aim to be hardware neutral here.  Object headers can include this file, but not vice
 * versa. */

/* Exported Constants ------------------------------------------------------ */

#define stptiTSHAL_SYNC_BYTE                (0x47)
#define stptiTSHAL_DVB_PID_BIT_SIZE         (13)
#define stptiTSHAL_DVB_WILDCARD_PID         ((1<<stptiTSHAL_DVB_PID_BIT_SIZE))	/* should be 0x2000 */
#define stptiTSHAL_TAG_MASK		    (0x1FFF)

typedef enum stptiTSHAL_TSInputDestination_e {
	stptiTSHAL_TSINPUT_DEST_DEMUX,
	stptiTSHAL_TSINPUT_DEST_TSOUT0,
	stptiTSHAL_TSINPUT_DEST_TSOUT1,
	stptiTSHAL_TSINPUT_DEST_EXT_TSOUT0,
	stptiTSHAL_TSINPUT_DEST_EXT_TSOUT1,
	stptiTSHAL_TSINPUT_DEST_DEFAULT,
	stptiTSHAL_TSINPUT_DEST_END	/* Always Leave Last in the List !! */
} stptiTSHAL_TSInputDestination_t;

typedef enum stptiTSHAL_TSInputTagging_e {
	stptiTSHAL_TSINPUT_TAGS_NONE,
	stptiTSHAL_TSINPUT_TAGS_STFE,
	stptiTSHAL_TSINPUT_TAGS_TTS,
	stptiTSHAL_TSINPUT_TAGS_END	/* Always Leave Last in the List !! */
} stptiTSHAL_TSInputTagging_t;

typedef enum stptiTSHAL_TSInputPowerState_e {
	stptiTSHAL_TSINPUT_RUNNING,
	stptiTSHAL_TSINPUT_SLEEPING,
	stptiTSHAL_TSINPUT_STOPPED,
	stptiTSHAL_TSINPUT_POWERDOWN,
} stptiTSHAL_TSInputPowerState_t;

/* Exported Types ---------------------------------------------------------- */

/* The API for the TSInput ----------------------------------------- */

typedef struct {
	U32 StreamID;
	U32 PacketLength;
	BOOL SyncLDEnable;
	BOOL SerialNotParallel;
	BOOL AsyncNotSync;
	BOOL AlignByteSOP;
	BOOL InvertTSClk;
	BOOL IgnoreErrorInByte;
	BOOL IgnoreErrorInPkt;
	BOOL IgnoreErrorAtSOP;
	BOOL InputBlockEnable;
	U32 MemoryPktNum;
	U32 ClkRvSrc;
	stptiTSHAL_TSInputDestination_t Routing;
	stptiTSHAL_TSInputTagging_t InputTagging;
	U32 OutputPacingRate;
	U32 OutputPacingClkSrc;
	BOOL CIMode;
	BOOL OutputSerialNotParallel;
	U32 TSTag;
} stptiTSHAL_TSInputConfigParams_t;

typedef struct {
	BOOL Lock;
	BOOL FifoOverflow;
	BOOL BufferOverflow;
	BOOL OutOfOrderRP;
	BOOL PktOverflow;
	BOOL DMAPointerError;
	BOOL DMAOverflow;
	U8 ErrorPackets;
	U8 ShortPackets;
	U32 StreamID;
} stptiTSHAL_TSInputStatus_t;

typedef struct {
	U32 TagMSWord;
	U32 TagLSWord;
	U32 Clk27MHzDiv300Bit32;
	U32 Clk27MHzDiv300Bit31to0;
	U32 Clk27MHzModulus300;
	U64 SystemTime;
} stptiTSHAL_TimerValue_t;

typedef struct {
	ST_ErrorCode_t(*TSHAL_TSInputMapHW) (void *params);												/* stptiTSHAL_TSInputStfeMapHW */
	ST_ErrorCode_t(*TSHAL_TSInputUnMapHW) (void);													/* stptiTSHAL_TSInputStfeUnMapHW */
	ST_ErrorCode_t(*TSHAL_TSInputConfigure) (stptiTSHAL_TSInputConfigParams_t *TSInputConfigParams_p);						/* stptiTSHAL_TSInputStfeConfigure */
	ST_ErrorCode_t(*TSHAL_TSInputEnable) (U32 StreamID, BOOL Enable);										/* stptiTSHAL_TSInputStfeEnable */
	ST_ErrorCode_t(*TSHAL_TSInputGetStatus) (stptiTSHAL_TSInputStatus_t *TSInputStatus_p);								/* stptiTSHAL_TSInputStfeGetStatus */
	ST_ErrorCode_t(*TSHAL_TSInputReset) (U32 StreamID);												/* stptiTSHAL_TSInputStfeReset */
	ST_ErrorCode_t(*TSHAL_TSInputIBRegisterDump) (U32 StreamID, BOOL Verbose, char *Buffer, int *StringSize_p, int Limit, int Offset, int *EOF_p);	/* stptiTSHAL_TSInputStfeIBRegisterDump */
	ST_ErrorCode_t(*TSHAL_TSInputSetClearPid) (U32 TP_requester, U32 StreamID, U16 Pid, BOOL Set);							/* stptiTSHAL_TSInputStfeSetClearPid */
	ST_ErrorCode_t(*TSHAL_TSInputStfeGetTimer) (U32 StreamID, stptiTSHAL_TimerValue_t *TimerValue_p, BOOL * IsTSIN_p);				/* stptiTSHAL_TSInputStfeGetTimer */
	ST_ErrorCode_t(*TSHAL_TSInputOverridePIDFilter) (U32 StreamBitmask, BOOL EnablePIDFilter);							/* stptiTSHAL_TSInputOverridePIDFilter */
	ST_ErrorCode_t(*TSHAL_TSInputStfeMonitorDREQLevels) (U32 *tp_read_pointers, U8 num_rps);							/* stptiTSHAL_TSInputStfeMonitorDREQLevels */
	ST_ErrorCode_t(*TSHAL_TSInputSetPowerState) (stptiTSHAL_TSInputPowerState_t NewState);								/* stptiTSHAL_TSInputSetPowerState */
	ST_ErrorCode_t(*TSHAL_TSInputNotifyPDevicePowerState) (U32 pDeviceIndex, BOOL PowerOn);								/* stptiTSHAL_TSInputNotifyPDevicePowerState */
	ST_ErrorCode_t(*TSHAL_TSInputGetPlatformDevice) (struct platform_device **pdev);								/* stptiTSHAL_TSInputGetPlatformDevice */
} stptiTSHAL_TSInputAPI_t;

/* The Complete API for the TSHAL ------------------------------------------ */
typedef struct {
	stptiTSHAL_TSInputAPI_t TSInput;
} TSHAL_API_t;

/* (effectively) Exported Function Prototypes ------------------------------ */

/**
   @brief  The macro for calling TSHAL functions.

   This macro calls the specified TSHAL function.  It helps to keep the code clean as it performs a
   function pointer lookup.  It is coded in this way to allow multiple TSHALs to coexist in the same
   driver.

   Example of usage...
     Error = stptiTSHAL_call( TSInput.TSHAL_TSInputConfigure, params_p );

   @param  Function            The TSHAL function to call.  This will be prefixed by the object type
                               as listed in TSHAL_API_t above.

   @param  TSDevice            The index of the hardware input device you are performing the operation on.

   @param  ...                 Extra parameters to be passed to the TSHAL function.

   @return                     A standard st error type as given by the TSHAL function.

 */
#define stptiTSHAL_call(Function, TSDevice, ...)         ({                 \
	ST_ErrorCode_t stptiTSHAL_ErrorReturn = ST_ERROR_FEATURE_NOT_SUPPORTED;	\
	TSHAL_API_t *stptiTSHAL_FunctionPool_p = (TSHAL_API_t *)stptiOBJMAN_ReturnTSHALFunctionPool(TSDevice);\
	if (TSHAL_PowerState()) {					 \
		if (stptiTSHAL_FunctionPool_p == NULL) {		 \
			stptiTSHAL_ErrorReturn = ST_ERROR_BAD_PARAMETER; \
		}							 \
		if (stptiTSHAL_FunctionPool_p->Function != NULL) {	 \
			stptiTSHAL_ErrorReturn = (*(stptiTSHAL_FunctionPool_p->Function))(__VA_ARGS__); \
		}							 \
	}								 \
	(stptiTSHAL_ErrorReturn);					 \
	})

#endif /* _PTI_TSHAL_API_H_ */
