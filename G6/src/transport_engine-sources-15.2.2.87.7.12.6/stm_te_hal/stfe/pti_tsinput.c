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
   @file   pti_tsinput.c
   @brief  The tsinput interface for TSIN live input

   It includes functionality/logic to control STFE MIB, SWTS and TSDMA blocks.
   No HW management is performed here

 */

#if 0
#define STPTI_PRINT
#endif

/*
 * TODO
 * GPIO for orly
 */

/* Includes ---------------------------------------------------------------- */
#include "linuxcommon.h"

/* Includes from API level */
#include "pti_platform.h"
#include "../pti_debug.h"
#include "../pti_hal_api.h"
#include "../pti_driver.h"

/* Includes from the HAL / ObjMan level */
#include "pti_tsinput.h"
#include "pti_stfe.h"

/* MACROS ------------------------------------------------------------------ */

/* Private Constants ------------------------------------------------------- */

/* This offset is the bit number of the lsb of the pid within the first 8 bytes of the pkt,
counting from the lsb of the 8th byte to arrive. */
#define STFE_DVB_PID_BIT_OFFSET                     (40)

#define STFE_DVB_DEFAULT_LOCK                       (3)
#define STFE_DVB_DEFAULT_DROP                       (3)

#define STFE_DVB_NO_LOCK_DROP                       (0)

#define STFE_DVB_DMA_PKT_SIZE                       (200)

#define STFE_TAG_BYTE_SIZE                          (8)

#define STFE_INPUT_FORMAT_SERIAL_NOT_PARALLEL       (0x00000001)
#define STFE_INPUT_FORMAT_BYTE_ENDIANNESS           (0x00000002)
#define STFE_INPUT_FORMAT_ASYNC_NOT_SYNC            (0x00000004)
#define STFE_INPUT_FORMAT_ALIGN_BYTE_SOP            (0x00000008)
#define STFE_INPUT_FORMAT_INVERT_TS_CLK             (0x00000010)
#define STFE_INPUT_FORMAT_IGNORE_ERROR_BYTE         (0x00000020)
#define STFE_INPUT_FORMAT_IGNORE_ERROR_PKT          (0x00000040)
#define STFE_INPUT_FORMAT_IGNORE_ERROR_SOP          (0x00000080)

#define STFE_STATUS_FIFO_OVERFLOW                   (0x00000001)
#define STFE_STATUS_BUFFER_OVERFLOW                 (0x00000002)
#define STFE_STATUS_OUT_OF_ORDER_RP                 (0x00000004)
#define STFE_STATUS_PID_OVERFLOW                    (0x00000008)
#define STFE_STATUS_PKT_OVERFLOW                    (0x00000010)
#define STFE_STATUS_LOCK                            (0x40000000)

#define STFE_STATUS_ERROR_PKTS                      (0x00000F00)
#define STFE_STATUS_ERROR_PKTS_POSITION             (8)

#define STFE_STATUS_ERROR_SHORT_PKTS                (0x0000F000)
#define STFE_STATUS_ERROR_SHORT_PKTS_POSITION       (12)

#define STFE_DVB_PID_BIT_SIZE                       (13)

/* @define TSDMA DEST FORMAT defines */
#define STFE_TSDMA_DEST_FORMAT_SERIAL_N_PARALLEL    (0x00000001)
#define STFE_TSDMA_DEST_FORMAT_BYTE_ENDIANNESS      (0x00000002)
#define STFE_TSDMA_DEST_FORMAT_REMOVE_TAG           (0x00000004)
#define STFE_TSDMA_DEST_FORMAT_COMPLETE_BYTE        (0x00000008)
#define STFE_TSDMA_DEST_FORMAT_SOP_FIRST            (0x00000010)
#define STFE_TSDMA_DEST_FORMAT_WAIT_PKT             (0x00000020)

/* @define Macro For Memory allocation (DDR RAM, PID FILTER, PID FILTER TRACKER) */
#define STFE_PID_FILTER_BIT_SIZE      (1)
#define STFE_PID_FILTER_BIT_WIDTH     (13)
#define STFE_PID_RAM_SIZE             (((1 << STFE_PID_FILTER_BIT_WIDTH) * STFE_PID_FILTER_BIT_SIZE) / 8)	/* Size in bytes of required PID RAM (1bit/pid) */
#define STFE_PID_TRACKER_RAM_SIZE     ((1<<STFE_PID_FILTER_BIT_WIDTH)+1)					/* Size in bytes of required PID Tracker RAM (1byte/pid) + 1 for wildcard pid */
#define STFE_PID_ALIGNMENT_SIZE       (STFE_PID_RAM_SIZE)							/* Must be the same as the PID size  */
#define STFE_BUFFER_ALIGN_MULTIPLE    (32)

/* @define MAX amount of MIB channel stream */
#define MAX_MIB    (32)

/* @define STREAM MASKS */
#define STFE_STREAMID_NONE (0x0000FFFF)
#define STFE_STREAMID_SWTS (0x00001000)
#define STFE_STREAMID_MIB  (0x00002000)

#define STFECHANNEL_UNUSED  (0xFFFF)

#define ALL_TP_MASK(tp) ((1<<tp) - 1)

/* @define STREAM MACRO FUNCTIONS  */
#define stptiHAL_StreamIDTestSWTS(StreamID) ( (StreamID & STFE_STREAMID_SWTS) ? TRUE: FALSE )
#define stptiHAL_StreamIDTestNone(StreamID) ( (StreamID == STFE_STREAMID_NONE) ? TRUE: FALSE )
#define stptiHAL_StreamIDTestMIB(StreamID)  ( (StreamID & STFE_STREAMID_MIB) ? TRUE: FALSE )

/* @define Macro Functions */
#define stptiHAL_IBIndexConvert2AbsSWTSIndex( Index ) (Index - STFE_Config.NumIBs)
#define stptiHAL_StreamIndexByType(StreamID) ( (stptiHAL_StreamIDTestMIB(StreamID)) ? (FindStreamID_MIB(StreamID)) :((StreamID) & ~(STFE_STREAMID_SWTS)))
#define stptiTSHAL_Stfe_NeedMibChannel(TSInputScenario) ( TSInputScenario & (stptiHAL_TSINPUT_SCENARIO_3 | stptiHAL_TSINPUT_SCENARIO_5 | stptiHAL_TSINPUT_SCENARIO_8))
#define stptiTSHAL_Stfe_NeedCscc(TSInputScenario) ( TSInputScenario & (stptiHAL_TSINPUT_SCENARIO_3 | stptiHAL_TSINPUT_SCENARIO_5 | stptiHAL_TSINPUT_SCENARIO_9))
#define stptiTSHAL_Stfe_DisablePidFilter(TSInputScenario, StfeType) ((TSInputScenario & (stptiHAL_TSINPUT_SCENARIO_2 | stptiHAL_TSINPUT_SCENARIO_3 | \
											stptiHAL_TSINPUT_SCENARIO_9 | stptiHAL_TSINPUT_SCENARIO_10) ) && \
											(StfeType == stptiTSHAL_TSINPUT_STFE_IB))

#define stptiTSHAL_Stfe_DisableLock(TSInputScenario) ((TSInputScenario & (stptiHAL_TSINPUT_SCENARIO_9 | stptiHAL_TSINPUT_SCENARIO_10 )))

#define stptiTSHAL_Stfe_NeedParallelMode(TSInputScenario, StfeType) ( (TSInputScenario & (stptiHAL_TSINPUT_SCENARIO_3 | stptiHAL_TSINPUT_SCENARIO_5)) && \
									(StfeType == stptiTSHAL_TSINPUT_STFE_MIB))

#define stptiTSHAL_Stfe_UseTag(TSInputScenario) (TSInputScenario & ( stptiHAL_TSINPUT_SCENARIO_1 | stptiHAL_TSINPUT_SCENARIO_2 | \
						stptiHAL_TSINPUT_SCENARIO_3 | stptiHAL_TSINPUT_SCENARIO_4 | \
						stptiHAL_TSINPUT_SCENARIO_5 | stptiHAL_TSINPUT_SCENARIO_6 | \
						stptiHAL_TSINPUT_SCENARIO_7 | stptiHAL_TSINPUT_SCENARIO_8 ))

#define stptiTSHAL_MibSetClear(set) if (STFE_Config.NumMIBs) \
					stptiTSHAL_StfeEnable(0, stptiTSHAL_TSINPUT_STFE_MIB, set);

#define PidAddressOffset(Pid) (((Pid*PIDRespW)/32)*4)
#define PidBitPosition(Pid) (1 << ((Pid*PIDRespW + TP_requester)%32))

/*
 * This macro is called when, in the middle of a function
 * the mutex is retaken: you have to check that the channel
 * is still alive, becuase in the meanwhile
 * something else could have deallocated him
 */
#define CHECK_CHANNEL_IS_STILL_SET() do { \
						if (TSInput_p->Index_byType == STFECHANNEL_UNUSED) \
							return STPTI_ERROR_TSINPUT_NOT_CONFIGURED;	\
					} while (0)

/* Typedef definition -------------------------------------------------------- */
typedef enum stptiHAL_TSInputPacketlength_e {
	stptiHAL_TSINPUT_PACKET_LENGTH_DVB = 188,
	stptiHAL_TSINPUT_PACKET_LENGTH_TAGGED_DVB = 196,
} stptiHAL_TSInputPacketlength_t;

typedef enum stptiHAL_TSInputScenario_e {	// PID  MemDma //
	stptiHAL_TSINPUT_SCENARIO_1 = 0x0001,	// Yes  Yes    // IB -> MEMDMA -> TP
	stptiHAL_TSINPUT_SCENARIO_2 = 0x0002,	// No   No     // IB -> TSOUT -> CI -> (then in another IB configured in Scenario 1)
	stptiHAL_TSINPUT_SCENARIO_3 = 0x0004,	// Yes  Yes    // IB -> TSOUT -> MSCC -> MIB -> MEMDMA -> TP
	stptiHAL_TSINPUT_SCENARIO_4 = 0x0008,	// No   No     // SWTS -> TSOUT -> CI -> (then in another IB configured in Scenario 1)
	stptiHAL_TSINPUT_SCENARIO_5 = 0x0010,	// Yes  Yes    // SWTS -> TSOUT -> MSCC -> MIB -> MEMDMA -> TP
	stptiHAL_TSINPUT_SCENARIO_6 = 0x0020,	// Yes  No     // IB -> TSOUT -> (another device)
	stptiHAL_TSINPUT_SCENARIO_7 = 0x0040,	// No   No     // SWTS -> TSOUT -> (another device)
	stptiHAL_TSINPUT_SCENARIO_8 = 0x0080,	// Yes  Yes    // MIB -> MEMDMA -> TP
	stptiHAL_TSINPUT_SCENARIO_9 = 0x0100,	// No   No     // IB (already tagged) -> TSOUT -> MSCC -> (then in a MIB configured in Scenario 8)
	stptiHAL_TSINPUT_SCENARIO_10 = 0x0200,	// No   Yes    // IB (already tagged) -> MEMDMA -> TP
	stptiHAL_TSINPUT_SCENARIO_Deallocate = 0x8000,	// Deallocate the channel
	stptiHAL_TSINPUT_SCENARIO_UNDEFINED = 0xFFFF,
} stptiHAL_TSInputScenario_t;

typedef struct {
	U8 *PIDTableStart_p;						/**< Pointer to memory aligned for PID table            */
	U32 PIDTableSize;
	U8 *PIDTableTracker_p;						/**< Pointer to memory returned by kmalloc */
	BOOL PIDFilterEnabled;						/**< Is the PID Filter Enabled */
	U32 PIDsEnabledNoWildcard;					/**< # of PIDs enabled on PID filter */
	BOOL LeakyPidEnabled;						/**< Is the Leaky PID Enabled */
	U32 *PidsEnabledForTP_p;					/**< Matrix with a counter of the active PID for each TP */
	U8 *RAMStart_p;							/**< Pointer to memory aligned for packet DMA transfers */
	U32 RAMSize;							/**< Size of memory aligned for packet DMA transfers  */
	U32 AllocatedNumOfPkts;						/**< Size of sys ram used to copy data to in pkts       */
	stptiTSHAL_TSInputConfigParams_t Config;			/**< Container for the STFE current configuration       */
	stptiSupport_DMAMemoryStructure_t RAMCachedMemoryStructure;	/**< A structure holding the range of cached memory for memory aligned for packet DMA transfers*/
	stptiSupport_DMAMemoryStructure_t PIDTableCachedMemoryStructure;/**< A structure holding the range of flushable memory for the STFE pid table */
} stptiTSHAL_TSInput_t;

typedef struct {
	stptiTSHAL_TSInput_t Stfe;
	U32 IBBlockIndex;
	U32 Index_byType;
	U32 OutputBlockIndex;
	U32 MemDmaChannel;
	U32 PidFilterIndex;
	U32 MIBChannel;
	U32 Tag;
	stptiHAL_TSInputStfeType_t StfeType;
	stptiHAL_TSInputScenario_t TSInputScenario;
} stptiTSHAL_InputDevices_t;

/* Public Variables -------------------------------------------------------- */

stptiTSHAL_HW_function_t const *stptiTSHAL_HW_function_stfe = NULL;
stptiTSHAL_STFE_Config_t STFE_Config;

/* Private Variables ------------------------------------------------------- */
static stptiTSHAL_InputDevices_t stptiTSHAL_TSInputs[MAX_NUMBER_OF_TSINPUTS];
static U32 PIDRespW = 0;
static U32 MibChannel_StreamId[MAX_MIB];
static struct mutex *stptiTSHAL_TSInputMutex_p;
static stptiSupport_DMAMemoryStructure_t ShadowPIDTableCachedMemoryStructure;
static U8 *ShadowPIDTableStart_p;

static const stptiTSHAL_HW_function_t stptiTSHAL_HW_function_stfev1 = {
	stptiTSHAL_Stfev1_CreateConfig,
	stptiTSHAL_Stfev1_LoadMemDmaFw,
	stptiTSHAL_Stfev1_SystemInit,
	stptiTSHAL_Stfev1_SystemTerm,
	stptiTSHAL_Stfev1_SystemIBRoute,
	stptiTSHAL_Stfev1_EnableLeakyPid,
	stptiTSHAL_Stfev1_LeakCountReset,
	stptiTSHAL_Stfev1_TsdmaSetBasePointer,
	stptiTSHAL_Stfev1_TsdmaEnableInputRoute,
	stptiTSHAL_Stfev1_TsdmaConfigureDestFormat,
	stptiTSHAL_Stfev1_TsdmaDumpMixedReg,
	stptiTSHAL_Stfev1_MemdmaClearDREQLevel,
	stptiTSHAL_Stfev1_Memdma_ram2beUsed,
	stptiTSHAL_Stfev1_Write_RamInputNode_ptrs,
	stptiTSHAL_Stfev1_MemdmaInit,
	stptiTSHAL_Stfev1_MemdmaTerm,
	stptiTSHAL_Stfev1_MemdmaChangeSubstreamStatus,
	stptiTSHAL_Stfev1_MemDmaRegisterDump,
	stptiTSHAL_Stfev1_DmaPTRRegisterDump,
	stptiTSHAL_Stfev1_SystemRegisterDump,
	stptiTSHAL_Stfev1_CCSCConfig,
	stptiTSHAL_Stfev1_CCSCBypass,
	stptiTSHAL_Stfev1_MonitorDREQLevels,
	stptiTSHAL_Stfev1_InputClockEnable
};

static const stptiTSHAL_HW_function_t stptiTSHAL_HW_function_stfev2 = {
	stptiTSHAL_Stfev2_CreateConfig,
	stptiTSHAL_Stfev2_LoadMemDmaFw,
	stptiTSHAL_Stfev2_SystemInit,
	stptiTSHAL_Stfev2_SystemTerm,
	stptiTSHAL_Stfev2_SystemIBRoute,
	stptiTSHAL_Stfev2_EnableLeakyPid,
	stptiTSHAL_Stfev2_LeakCountReset,
	stptiTSHAL_Stfev2_TsdmaSetBasePointer,
	stptiTSHAL_Stfev2_TsdmaEnableInputRoute,
	stptiTSHAL_Stfev2_TsdmaConfigureDestFormat,
	stptiTSHAL_Stfev2_TsdmaDumpMixedReg,
	stptiTSHAL_Stfev2_MemdmaClearDREQLevel,
	stptiTSHAL_Stfev2_Memdma_ram2beUsed,
	stptiTSHAL_Stfev2_Write_RamInputNode_ptrs,
	stptiTSHAL_Stfev2_MemdmaInit,
	stptiTSHAL_Stfev2_MemdmaTerm,
	stptiTSHAL_Stfev2_MemdmaChangeSubstreamStatus,
	stptiTSHAL_Stfev2_MemDmaRegisterDump,
	stptiTSHAL_Stfev2_DmaPTRRegisterDump,
	stptiTSHAL_Stfev2_SystemRegisterDump,
	stptiTSHAL_Stfev2_CCSCConfig,
	stptiTSHAL_Stfev2_CCSCBypass,
	stptiTSHAL_Stfev2_MonitorDREQLevels,
	stptiTSHAL_Stfev2_InputClockEnable
};

/* Private Function Prototypes --------------------------------------------- */

/* Although these prototypes are not exported directly they are exported through the API constant below. */
/* Add the definition to pti_hal.h */

ST_ErrorCode_t stptiTSHAL_Init(void *params);
ST_ErrorCode_t stptiTSHAL_Term(void);
ST_ErrorCode_t stptiTSHAL_TSInputStfeConfigure(stptiTSHAL_TSInputConfigParams_t * TSInputConfigParams_p);
ST_ErrorCode_t stptiTSHAL_TSInputStfeEnable(U32 StreamID, BOOL Enable);
ST_ErrorCode_t stptiTSHAL_TSInputStfeGetStatus(stptiTSHAL_TSInputStatus_t * TSInputStatus_p);
ST_ErrorCode_t stptiTSHAL_TSInputStfeReset(U32 StreamID);
ST_ErrorCode_t stptiTSHAL_TSInputStfeGetTimer(U32 StreamID, stptiTSHAL_TimerValue_t * TimerValue_p, BOOL * IsTSIN_p);
ST_ErrorCode_t stptiTSHAL_TSInputStfeIBRegisterDump(U32 StreamID, BOOL Verbose, char *Buffer, int *StringSize_p,
						    int Limit, int Offset, int *EOF_p);
ST_ErrorCode_t stptiTSHAL_TSInputStfeSetClearPid(U32 TP_requester, U32 StreamID, U16 Pid, BOOL Set);
ST_ErrorCode_t stptiTSHAL_TSInputOverridePIDFilter(U32 StreamBitmask, BOOL EnablePIDFilter);
ST_ErrorCode_t stptiTSHAL_TSInputStfeMonitorDREQLevels(U32 * tp_read_pointers, U8 num_rps);
ST_ErrorCode_t stptiTSHAL_TSInputSetPowerState(stptiTSHAL_TSInputPowerState_t NewState);
ST_ErrorCode_t stptiTSHAL_TSInputNotifyPDevicePowerState(U32 pDeviceIndex, BOOL PowerOn);
ST_ErrorCode_t stptiTSHAL_TSInputGetPlatformDevice(struct platform_device **pdev);

/*
 * STATIC FUNCTION DECLARATION
 */
/* Helper Function */
static ST_ErrorCode_t stptiTSHAL_StfePidTableSync(stptiTSHAL_InputDevices_t *TSInput_p, U8 TPmask);
static ST_ErrorCode_t stptiTSHAL_StfePidTableClear(stptiTSHAL_InputDevices_t *TSInput_p, U8 TPmask);
static ST_ErrorCode_t stptiTSHAL_StfeDisableTP(stptiTSHAL_InputDevices_t *TSInput_p, U8 TPmask);
static ST_ErrorCode_t stptiTSHAL_StfeConfigureTP(stptiTSHAL_InputDevices_t *TSInput_p, U8 TPmask);
static ST_ErrorCode_t stptiTSHAL_TSInputObtainMIBChannel(stptiTSHAL_InputDevices_t * TSInput_p);
static ST_ErrorCode_t stptiTSHAL_StfeAllocate(stptiTSHAL_InputDevices_t * TSInput_p,
					      stptiSupport_DMAMemoryStructure_t * DMAMemoryStructure_p, size_t Size,
					      U32 BaseAlignment, U8 ** mem_ptr);
static ST_ErrorCode_t stptiTSHAL_StfeDDRRAMAllocate(stptiTSHAL_InputDevices_t * TSInput_p);
static ST_ErrorCode_t stptiTSHAL_StfeMemAllocate(stptiTSHAL_InputDevices_t * TSInput_p);
static void stptiTSHAL_TSInputResetChannel(stptiTSHAL_InputDevices_t * TSInput_p);
static ST_ErrorCode_t stptiTSHAL_StfeShutDown(void);
static ST_ErrorCode_t stptiTSHAL_StfeDeallocate(stptiTSHAL_InputDevices_t * TSInput_p);
static ST_ErrorCode_t stptiTSHAL_StfeConfigureInput(stptiTSHAL_InputDevices_t * TSInput_p);
static ST_ErrorCode_t stptiTSHAL_StfeDisableInput(stptiTSHAL_InputDevices_t * TSInput_p);
static ST_ErrorCode_t stptiTSHAL_StfeBlockConfigure(stptiTSHAL_InputDevices_t * TSInput_p, U32 Index_byType,
						    stptiHAL_TSInputStfeType_t StfeType);
static ST_ErrorCode_t stptiTSHAL_StfeSetLeakyPid(stptiTSHAL_InputDevices_t * TSInput_p);
static ST_ErrorCode_t stptiTSHAL_StfeChangeActivityState(stptiTSHAL_InputDevices_t * TSInput_p);
static ST_ErrorCode_t stptiTSHAL_StfeSetPid(stptiTSHAL_InputDevices_t * TSInput_p, U16 Pid, U32 TP_requester);
static ST_ErrorCode_t stptiTSHAL_StfeClearPid(stptiTSHAL_InputDevices_t * TSInput_p, U16 Pid, U32 TP_requester);
static ST_ErrorCode_t stptiTSHAL_StfeTsdmaConfigure(stptiTSHAL_InputDevices_t * TSInput_p);
static void stptiTSHAL_StfeWriteToInternalRAMDDRPtrs(stptiTSHAL_InputDevices_t * TSInput_p, U32 BufferSize,
						     U32 PktSize);
static stptiHAL_TSInputScenario_t stptiTSHAL_Stfe_ValidateInput(stptiTSHAL_TSInputConfigParams_t *
								TSInputConfigParams_p);
static void stptiTSHAL_StfeDisableMIB(void);
static void stptiTSHAL_EnableClocks(void);
static void stptiTSHAL_DisableClocks(void);
static ST_ErrorCode_t stptiTSHAL_TSInputSleep(void);
static ST_ErrorCode_t stptiTSHAL_TSInputResume(void);
static void __enable_clk(struct stpti_clk *clk, bool set_parent);

/* Function managing input parameter */
static U32 CalculateMEMDMAChannel(stptiTSHAL_InputDevices_t * TSInput_p);
static U32 CalculatePidFilterIndexChannel(stptiTSHAL_InputDevices_t * TSInput_p);
static U32 FindStreamID_MIB(U32 StreamID);
static U32 GetStreamID_MIB(U32 StreamID);
static void ResetStreamID_MIB(U32 StreamID);
static U32 ConvertAPItoTSHALIndex(U32 StreamID, BOOL AllocateMibChannel);
static U32 ConvertTagtoTSHALIndex(U32 StreamID);
static stptiHAL_TSInputStfeType_t ExtractIBTypeFromStreamID(U32 StreamID);
static U32 ConvertDestToTSDMAOutput(stptiTSHAL_TSInputDestination_t Routing);

/* debug Functions */
static void stptiTSHAL_StfeDDRRAMDump(stptiTSHAL_InputDevices_t * TSInput_p, stptiSUPPORT_sprintf_t * ctx_p);
static void stptiTSHAL_StfePidEnabledTableDump(stptiTSHAL_InputDevices_t * TSInput_p, stptiSUPPORT_sprintf_t * ctx_p);
/* Public Constants ------------------------------------------------------- */

/* Export the API */
const stptiTSHAL_TSInputAPI_t stptiTSHAL_TSInputAPI = {
	stptiTSHAL_Init,
	stptiTSHAL_Term,
	stptiTSHAL_TSInputStfeConfigure,
	stptiTSHAL_TSInputStfeEnable,
	stptiTSHAL_TSInputStfeGetStatus,
	stptiTSHAL_TSInputStfeReset,
	stptiTSHAL_TSInputStfeIBRegisterDump,
	stptiTSHAL_TSInputStfeSetClearPid,
	stptiTSHAL_TSInputStfeGetTimer,
	stptiTSHAL_TSInputOverridePIDFilter,
	stptiTSHAL_TSInputStfeMonitorDREQLevels,
	stptiTSHAL_TSInputSetPowerState,
	stptiTSHAL_TSInputNotifyPDevicePowerState,
	stptiTSHAL_TSInputGetPlatformDevice,
};

/* Private Functions ------------------------------------------------------- */



/**
   @brief  re-sync the STFE block to vDevices PIDs

   @param  TSInput_p         A pointer to the STFE block's current parameters/settings
   @return                         A standard st error type...
                                   - ST_ERROR_NO_MEMORY if no channels free, or no memory.
                                   - ST_NO_ERROR if no errors
 */
static ST_ErrorCode_t stptiTSHAL_StfePidTableSync(stptiTSHAL_InputDevices_t *TSInput_p, U8 TPmask)
{
	U32 StreamID = TSInput_p->Stfe.Config.StreamID;
	U32 pDeviceIndex, Count, Count2;
	ST_ErrorCode_t Error = ST_NO_ERROR;

	U16 Pids[256];
	U16 PIDsFound;

	mutex_unlock(stptiTSHAL_TSInputMutex_p);
	for (pDeviceIndex = 0; pDeviceIndex < STFE_Config.NumTPUsed; pDeviceIndex++) {
		if ((TPmask >> pDeviceIndex) & 0x1) {
			FullHandle_t vDeviceHandles[MAX_NUMBER_OF_VDEVICES];
			U32 vDeviceCount = 0;
			vDeviceCount =
			    stptiOBJMAN_ReturnChildObjects(stptiOBJMAN_pDeviceObjectHandle(pDeviceIndex), vDeviceHandles,
							   MAX_NUMBER_OF_VDEVICES, OBJECT_TYPE_VDEVICE);

			for (Count = 0; Count < vDeviceCount; Count++) {
				if (!stptiOBJMAN_IsNullHandle(vDeviceHandles[Count])) {
					U32 vDeviceStreamID;
					ST_ErrorCode_t Error;
					BOOL UseTimerTag;
					Error =
					    stptiHAL_call(vDevice.HAL_vDeviceGetStreamID, vDeviceHandles[Count],
							  &vDeviceStreamID, &UseTimerTag);
					if (ST_NO_ERROR == Error && StreamID == vDeviceStreamID) {
						Error =
						    stptiHAL_call(vDevice.HAL_vDeviceLookupPIDs, vDeviceHandles[Count], Pids,
								  256, &PIDsFound);
						if (ST_NO_ERROR == Error) {
							for (Count2 = 0; Count2 < PIDsFound; Count2++) {
								stpti_printf("Setting PID 0x%x", Pids[Count2]);
								stptiTSHAL_TSInputStfeSetClearPid(pDeviceIndex, StreamID,
												  Pids[Count2], TRUE);
							}
						}
					}
				}
			}
		}
	}
	if (mutex_lock_interruptible(stptiTSHAL_TSInputMutex_p))
		return ST_ERROR_TIMEOUT;

	CHECK_CHANNEL_IS_STILL_SET();

	return Error;
}

/**
   @brief  Clear Pid RAM, Pid Tracker RAM, and Pid Tracker for TP

   Helper function to allow a clean disable

   @param  TSInput_p         A pointer to the STFE block's current parameters/settings
   @return                         A standard st error type...
                                   - ST_ERROR_NO_MEMORY if no channels free, or no memory.
                                   - ST_NO_ERROR if no errors
 */
static ST_ErrorCode_t stptiTSHAL_StfePidTableClear(stptiTSHAL_InputDevices_t *TSInput_p, U8 TPmask)
{
	U32 i = 0;

	if (TSInput_p->Stfe.PIDTableStart_p != NULL) {

		U8 mask = 0xFF;
		U8 BitInAByte = 8;
		U8 repetition = BitInAByte/PIDRespW;

		for (i = 0; i < repetition; i++)
			mask &= ~(TPmask << i*PIDRespW);

		memset((void *)TSInput_p->Stfe.PIDTableStart_p, mask, TSInput_p->Stfe.PIDTableSize);
		stptiSupport_FlushRegion(&TSInput_p->Stfe.PIDTableCachedMemoryStructure,
					 (void *)TSInput_p->Stfe.PIDTableStart_p, TSInput_p->Stfe.PIDTableSize);
	}

	if (TSInput_p->Stfe.PidsEnabledForTP_p != NULL) {
		for (i = 0; i < STFE_Config.NumTPUsed; i++) {

			if ((TPmask >> i) & 0x1) {

				if (TSInput_p->Stfe.PIDTableTracker_p != NULL) {
					TSInput_p->Stfe.PIDsEnabledNoWildcard -= ((TSInput_p->Stfe.PidsEnabledForTP_p[i] -
							TSInput_p->Stfe.PIDTableTracker_p[stptiTSHAL_DVB_WILDCARD_PID +
							STFE_PID_TRACKER_RAM_SIZE*i]));

					memset((void *)&TSInput_p->Stfe.PIDTableTracker_p[STFE_PID_TRACKER_RAM_SIZE*i],
							0x00, STFE_PID_TRACKER_RAM_SIZE);

					stptiTSHAL_StfeChangeActivityState(TSInput_p);
				} else {
					TSInput_p->Stfe.PIDsEnabledNoWildcard -= TSInput_p->Stfe.PidsEnabledForTP_p[i];
				}
				TSInput_p->Stfe.PidsEnabledForTP_p[i] = 0;
			}
		}
	}

	return ST_NO_ERROR;
}

/**
   @brief  Helper function for configuring the TP to accept data from the selected IB

   Configure the Live input on the TP. Obviously this function is called if a MEMDMA channel is setup.

   @param  TSInput_p         A pointer to the STFE block's current parameters/settings
   @return                   A standard st error type...
                                   - STPTI_ERROR_TSINPUT_ROUTE_UNAVAILABLE if MIB channel all in use
                                   - ST_NO_ERROR if no errors

 */
static ST_ErrorCode_t stptiTSHAL_StfeDisableTP(stptiTSHAL_InputDevices_t *TSInput_p, U8 TPmask)
{
	ST_ErrorCode_t Error = ST_NO_ERROR;
	U8 i;

	Error = stptiTSHAL_HW_function_stfe->stptiTSHAL_StfeMemdmaChangeSubstreamStatus(TSInput_p->MemDmaChannel, TPmask, FALSE);

	if (Error != STPTI_ERROR_TSINPUT_HW_ACCESS_ERROR) {
		mutex_unlock(stptiTSHAL_TSInputMutex_p);
		for (i = 0; i < STFE_Config.NumTPUsed; i++) {

			stptiHAL_pDeviceConfigLiveParams_t LiveParams;
			if ((TPmask >> i) & 0x1) {
				/* Finally disable the TP */
				LiveParams.Base = 0;
				LiveParams.SizeInPkts = 0;
				LiveParams.PktLength = 0;
				LiveParams.Channel = TSInput_p->MemDmaChannel;
				Error =
				    stptiHAL_call(pDevice.HAL_pDeviceConfigureLive,
						  stptiOBJMAN_pDeviceObjectHandle(i), &LiveParams);

				if (ST_ERROR_SUSPENDED == Error)
					Error = ST_NO_ERROR;
				if (ST_ERROR_TIMEOUT == Error)
					Error = STPTI_ERROR_TSINPUT_HW_ACCESS_ERROR;
			}
		}
		if (mutex_lock_interruptible(stptiTSHAL_TSInputMutex_p))
			return ST_ERROR_TIMEOUT;

		CHECK_CHANNEL_IS_STILL_SET();
	}
	return Error;
}


static ST_ErrorCode_t stptiTSHAL_StfeConfigureTP(stptiTSHAL_InputDevices_t *TSInput_p, U8 TPmask)
{
	ST_ErrorCode_t Error = ST_NO_ERROR;
	stptiHAL_pDeviceConfigLiveParams_t LiveParams;
	U32 PktSize = STFE_DVB_DMA_PKT_SIZE;	/* Default size for DVB */
	U32 i;
	U32 TP_RAMSize = TSInput_p->Stfe.RAMSize / STFE_Config.NumTPUsed;

	memset(&LiveParams, 0, sizeof(stptiHAL_pDeviceConfigLiveParams_t));
	/* Clear the DREQ counter - this is important as turning on/off the input can loose sync from this */
	stptiTSHAL_HW_function_stfe->stptiTSHAL_StfeMemdmaClearDREQLevel(TSInput_p->MemDmaChannel,
									 TSInput_p->Stfe.AllocatedNumOfPkts);

	/* Calculate IB packet size based on transport type etc */
	switch (TSInput_p->Stfe.Config.PacketLength) {
	case (stptiHAL_TSINPUT_PACKET_LENGTH_DVB):
	case (stptiHAL_TSINPUT_PACKET_LENGTH_TAGGED_DVB):
	default:
		PktSize = STFE_DVB_DMA_PKT_SIZE;
		break;
	}

	mutex_unlock(stptiTSHAL_TSInputMutex_p);
	for (i = 0; i < STFE_Config.NumTPUsed; i++) {

		if ((TPmask >> i) & 0x1) {
			LiveParams.Base =
			    (U32) stptiSupport_VirtToPhys(&TSInput_p->Stfe.RAMCachedMemoryStructure,
							  &TSInput_p->Stfe.RAMStart_p[i * TP_RAMSize]);
			LiveParams.SizeInPkts = TSInput_p->Stfe.AllocatedNumOfPkts;
			LiveParams.PktLength = PktSize;
			LiveParams.Channel = TSInput_p->MemDmaChannel;
			/* reset the STFE pointers as well */
			stptiTSHAL_StfeWriteToInternalRAMDDRPtrs(TSInput_p,
					TSInput_p->Stfe.AllocatedNumOfPkts * PktSize,
					PktSize);
			Error = stptiHAL_call(pDevice.HAL_pDeviceConfigureLive, stptiOBJMAN_pDeviceObjectHandle(i), &LiveParams);
			if (Error == ST_ERROR_SUSPENDED)
				Error = ST_NO_ERROR;

			if (Error != ST_NO_ERROR)
				break;
		}
	}
	if (mutex_lock_interruptible(stptiTSHAL_TSInputMutex_p))
		return ST_ERROR_TIMEOUT;

	/* set the TP entry in the DMA TP enable register */
	stptiTSHAL_HW_function_stfe->stptiTSHAL_StfeMemdmaChangeSubstreamStatus(TSInput_p->MemDmaChannel, TPmask,
									 TRUE);

	CHECK_CHANNEL_IS_STILL_SET();

	return Error;
}

/**
   @brief  Helper function for selecting a MIB channel

   Check MIB channels currently in use and then select a new one if available

   @param  TSInput_p         A pointer to the STFE block's current parameters/settings
   @return                   A standard st error type...
                                   - STPTI_ERROR_TSINPUT_ROUTE_UNAVAILABLE if MIB channel all in use
                                   - ST_NO_ERROR if no errors
 */
static ST_ErrorCode_t stptiTSHAL_TSInputObtainMIBChannel(stptiTSHAL_InputDevices_t *TSInput_p)
{
	ST_ErrorCode_t Error = ST_NO_ERROR;

	/* If this channel is already set to TSDMA then we can skip this stage
	 * therefore we do it, it the routing is on the DEMUX */
	if (TSInput_p->MIBChannel == STFECHANNEL_UNUSED) {
		U8 i;
		BOOL MIBUsed[64] = { 0 };

		/* Walk through all the TSInputs and decide if there is a free MIB channel */

		/* Tally the used MIB channels in the system */
		for (i = 0; i < (STFE_Config.NumIBs + STFE_Config.NumSWTSs + STFE_Config.NumMIBs); i++) {
			if (stptiTSHAL_TSInputs[i].MIBChannel != STFECHANNEL_UNUSED) {
				MIBUsed[stptiTSHAL_TSInputs[i].MIBChannel] = TRUE;
			}
		}
		/* Find a free MIB */
		for (i = 0; i < STFE_Config.NumMIBs; i++) {
			if (MIBUsed[i] == FALSE) {
				break;
			}
		}
		/* No free MIBs so generate an error */
		if (i >= STFE_Config.NumMIBs) {
			Error = STPTI_ERROR_TSINPUT_ROUTE_UNAVAILABLE;
			TSInput_p->MIBChannel = STFECHANNEL_UNUSED;
			STPTI_PRINTF_ERROR("TSInput %d no MIB channel available", TSInput_p->IBBlockIndex);
			stpti_printf("TSInput %d no MIB channel available", TSInput_p->IBBlockIndex);
		} else {
			TSInput_p->MIBChannel = i;
			stpti_printf("TSInput %d set to use MIB channel %d", TSInput_p->IBBlockIndex, i);
		}
	}
	return Error;
}

/**
   @brief  Helper function for memory allocation

   This function helps to allocate RAM blocks; depending on their purpose it use a different allocator

   @param  TSInput_p               A pointer to the STFE block's current parameters/settings
   @param  DMAMemoryStructure_p    A pointer to where to put the Memory Structure (For uncached mem).
   @param  Size                    Amount of Memory to allocate (in bytes)
   @param  BaseAlignment           Address Boundary Alignment (For uncached mem)
   @param  mem_ptr		   A pointer where to store the pointer for the allocated memory.
   @return                         A standard st error type...
                                   - ST_ERROR_NO_MEMORY if no channels free, or no memory.
                                   - ST_NO_ERROR if no errors
 */
static ST_ErrorCode_t stptiTSHAL_StfeAllocate(stptiTSHAL_InputDevices_t *TSInput_p,
					      stptiSupport_DMAMemoryStructure_t *DMAMemoryStructure_p, size_t Size,
					      U32 BaseAlignment, U8 **mem_ptr)
{
	ST_ErrorCode_t Error = ST_NO_ERROR;

	if (*mem_ptr == NULL) {
		if (BaseAlignment != 0) {
			if (DMAMemoryStructure_p && STFE_Config.pdev)
				DMAMemoryStructure_p->Dev = &STFE_Config.pdev->dev;

			if (DMAMemoryStructure_p)
				*mem_ptr = stptiSupport_MemoryAllocateForDMA(Size,
									     BaseAlignment,
									     DMAMemoryStructure_p,
									     stptiSupport_ZONE_NONE);
		} else {
			*mem_ptr = kmalloc(Size, GFP_KERNEL);
		}

		if (*mem_ptr != NULL) {
			memset((void *)*mem_ptr, 0x00, Size);
		} else {
			STPTI_PRINTF_ERROR("Failed to allocate %d bytes on channel %u", Size,
					   (TSInput_p->IBBlockIndex));

			stptiTSHAL_StfeDeallocate(TSInput_p);
			Error = ST_ERROR_NO_MEMORY;
		}
	}
	return Error;
}

/**
   @brief  Helper function for memory allocation

   This function allocates the DDRRAM for the STFE input block for MEMDMA data

   @param  TSInput_p               A pointer to the STFE block's current parameters/settings
   @return                         A standard st error type...
                                   - ST_ERROR_NO_MEMORY if no channels free, or no memory.
                                   - ST_NO_ERROR if no errors
 */
static ST_ErrorCode_t stptiTSHAL_StfeDDRRAMAllocate(stptiTSHAL_InputDevices_t * TSInput_p)
{
	ST_ErrorCode_t Error = ST_NO_ERROR;

	/* Allocate system memory for the STFE to put data input if required */
	if (TSInput_p->Stfe.AllocatedNumOfPkts != TSInput_p->Stfe.Config.MemoryPktNum) {
		U8 *RAMBuffer_p = NULL;
		U32 RamSize = 0;
		U32 ActualNumOfPkts = 0;
		U32 AdjustedPktLen = 0;
		U32 NormalizePckLen = TSInput_p->Stfe.Config.PacketLength;

		/* If Currently using memory then free it up */
		if (TSInput_p->Stfe.AllocatedNumOfPkts) {
			stpti_printf("stptiHAL_TSInputMemAllocate freeing memory for existing %u packets",
				     TSInput_p->Stfe.AllocatedNumOfPkts);

			/* Make sure you use the previous partition incase a new one has been passed in */
			stptiSupport_MemoryDeallocateForDMA(&TSInput_p->Stfe.RAMCachedMemoryStructure);
			TSInput_p->Stfe.RAMStart_p = NULL;
			TSInput_p->Stfe.RAMSize = 0;
			TSInput_p->Stfe.AllocatedNumOfPkts = 0;
		}

		/* DMA transfers in 32 byte blocks. Thus packet size must be a multiple of 8 bytes and buffer size a multiple of 4 packets. */
		/* For DVB                       = 188 bytes packet + 8 bytes Tag + 4 bytes = 200 */
		if (TSInput_p->Stfe.Config.MemoryPktNum % 8) {
			/* Round up to a multiple of 8 packets - Datasheet suggests a multiple of 4 packets which is fine if you allocate twice for Base and NextBase i.e. MOD32
			   but as we divide the memory allocated by 2 for the buffer pointers the total memory needs to MOD 64 = 0 */
			ActualNumOfPkts =
			    (TSInput_p->Stfe.Config.MemoryPktNum + (8 - (TSInput_p->Stfe.Config.MemoryPktNum % 8)));
		} else {
			ActualNumOfPkts = TSInput_p->Stfe.Config.MemoryPktNum;
		}

		// because we have the packet lenght already enlarge of the tag
		if ((TSInput_p->TSInputScenario == stptiHAL_TSINPUT_SCENARIO_9)
		    || (TSInput_p->TSInputScenario == stptiHAL_TSINPUT_SCENARIO_10))
			NormalizePckLen -= STFE_TAG_BYTE_SIZE;

		if (NormalizePckLen % 8) {
			/* Adjust the pkt len for a multiple of 8 bytes */
			AdjustedPktLen = (NormalizePckLen + 8 + (8 - (NormalizePckLen % 8)));
		} else {
			AdjustedPktLen = (NormalizePckLen + 8);
		}

		stpti_printf("Actual Num of TS packets to allocate: %04u", ActualNumOfPkts);
		stpti_printf("Adjusted TS packet Len for STFE     : %04u", AdjustedPktLen);

		/* Memory should be aligned to 32 bytes for bus addresses so overallocate & then re-align */
		RamSize = (ActualNumOfPkts * AdjustedPktLen * STFE_Config.NumTPUsed);
		Error =
		    stptiTSHAL_StfeAllocate(TSInput_p, &TSInput_p->Stfe.RAMCachedMemoryStructure, RamSize,
					    STFE_BUFFER_ALIGN_MULTIPLE, &RAMBuffer_p);

		if (Error == ST_NO_ERROR) {
			/* Now store the pointers  */
			TSInput_p->Stfe.RAMStart_p = RAMBuffer_p;
			TSInput_p->Stfe.RAMSize = RamSize;
			TSInput_p->Stfe.AllocatedNumOfPkts = ActualNumOfPkts;
			TSInput_p->Stfe.Config.MemoryPktNum = ActualNumOfPkts;

#if defined (STTBX_PRINT)
			if (((U32)(TSInput_p->Stfe.RAMStart_p)) % STFE_BUFFER_ALIGN_MULTIPLE) {
				STPTI_PRINTF_ERROR("Ram Buffer address not %u byte aligned 0x%08x",
						   STFE_BUFFER_ALIGN_MULTIPLE, (U32)(TSInput_p->Stfe.RAMStart_p));
			} else {
				stpti_printf("Allocated 0x%x bytes of RAM at 0x%08x",
					     (ActualNumOfPkts * AdjustedPktLen) + STFE_BUFFER_ALIGN_MULTIPLE - 1,
					     (U32) TSInput_p->Stfe.RAMRealStart_p);
			}
#endif
		}
	}
	return Error;
}

/**
   @brief  Helper function for memory allocation

   This function allocates the RAM for the STFE input block to MEMDMA data,
   a RAM for Pid filtering, and 2 other tracker RAM

   @param  TSInput_p               A pointer to the STFE block's current parameters/settings
   @return                         A standard st error type...
                                   - ST_ERROR_NO_MEMORY if no channels free, or no memory.
                                   - ST_NO_ERROR if no errors
 */
static ST_ErrorCode_t stptiTSHAL_StfeMemAllocate(stptiTSHAL_InputDevices_t *TSInput_p)
{
	ST_ErrorCode_t Error = ST_NO_ERROR;

	U32 stfe_tracker_ram_size = STFE_PID_TRACKER_RAM_SIZE * STFE_Config.NumTPUsed;
	U32 PidsEnabledForTP_Size = STFE_Config.NumTPUsed * sizeof(U32);

	TSInput_p->Stfe.PIDTableSize = (((STFE_PID_RAM_SIZE) * 8 * PIDRespW) / 8);
	if (TSInput_p->PidFilterIndex != STFECHANNEL_UNUSED) {
		if (ST_NO_ERROR !=
		    (Error =
		     stptiTSHAL_StfeAllocate(TSInput_p, &TSInput_p->Stfe.PIDTableCachedMemoryStructure,
					     TSInput_p->Stfe.PIDTableSize, TSInput_p->Stfe.PIDTableSize,
					     &TSInput_p->Stfe.PIDTableStart_p))) {
			stpti_printf("Error Allocating 0x%x bytes of PID RAM\n", TSInput_p->Stfe.PIDTableSize);
		} else if (ST_NO_ERROR !=
			   (Error =
			    stptiTSHAL_StfeAllocate(TSInput_p, 0, stfe_tracker_ram_size, 0,
						    &TSInput_p->Stfe.PIDTableTracker_p))) {
			stpti_printf("Error Allocating 0x%x bytes of PID TRACKER RAM\n", stfe_tracker_ram_size);
		}
	}

	/* we allocate always this, because we use it in scenarion 10 too */
	if (ST_NO_ERROR == Error) {
		if (ST_NO_ERROR !=
				   (Error =
				    stptiTSHAL_StfeAllocate(TSInput_p, 0, PidsEnabledForTP_Size, 0,
							    (U8 **) &TSInput_p->Stfe.PidsEnabledForTP_p))) {
				stpti_printf("Error Allocating 0x%x bytes of PID FOR TP RAM\n", PidsEnabledForTP_Size);
			}
	}

	if (ST_NO_ERROR == Error) {
		if (TSInput_p->MemDmaChannel != STFECHANNEL_UNUSED) {
			if (ST_NO_ERROR != (Error = stptiTSHAL_StfeDDRRAMAllocate(TSInput_p))) {
				stpti_printf("Error Allocating DDRRAM\n");
			} else {
				stpti_printf("Allocated 0x%x bytes of PID RAM at 0x%08x", TSInput_p->Stfe.PIDTableSize,
					     (U32) TSInput_p->Stfe.PIDTableStart_p);
				stpti_printf("PID RAM size 0x%x bytes", TSInput_p->Stfe.PIDTableSize);
				stpti_printf("Allocate %u bytes of RAM @ 0x%x to track Pids", stfe_tracker_ram_size,
					     (U32) TSInput_p->Stfe.PIDTableTracker_p);
				stpti_printf("Allocate %u bytes of RAM @ 0x%x to track TPs", PidsEnabledForTP_Size,
					     (U32) TSInput_p->Stfe.PidsEnabledForTP_p);
			}
		}
	}

	return (Error);
}

/**
   @brief  Helper function for Clear a TSInput channel information

   This function can be called to reset all the meaningful parameter of an input channel
   it was choose to use a unique function to avoid bugs in future code modification

   @param  TSInput_p         A pointer to the TSInput current parameters/settings
   @return - none
 */
static void stptiTSHAL_TSInputResetChannel(stptiTSHAL_InputDevices_t *TSInput_p)
{
	/* First clear any existing MIB channel with this StreamID */
	ResetStreamID_MIB(TSInput_p->Stfe.Config.StreamID);
	/* Reset the TSInput structure to prevent any errors on re-configuration */
	memset(&(TSInput_p->Stfe), 0x00, sizeof(stptiTSHAL_TSInput_t));
	TSInput_p->MIBChannel = STFECHANNEL_UNUSED;
	TSInput_p->Index_byType = STFECHANNEL_UNUSED;
	TSInput_p->IBBlockIndex = STFECHANNEL_UNUSED;
	TSInput_p->MemDmaChannel = STFECHANNEL_UNUSED;
	TSInput_p->OutputBlockIndex = STFECHANNEL_UNUSED;
	TSInput_p->PidFilterIndex = STFECHANNEL_UNUSED;
	TSInput_p->Tag = STFECHANNEL_UNUSED;

}

/**
   @brief  Helper function for memory deallocation and disabling the

   This function can be called to disable all stfe inputs and free all associated memory
   This is intended to be used in the event of the driver being shutdown

   @param  - None
   @return - A standard st error type...
           - ST_NO_ERROR if no errors
 */
static ST_ErrorCode_t stptiTSHAL_StfeShutDown(void)
{
	U32 Index;

	stpti_printf("");

	for (Index = 0; Index < (STFE_Config.NumIBs + STFE_Config.NumSWTSs + STFE_Config.NumMIBs); Index++) {
		if (stptiTSHAL_TSInputs[Index].IBBlockIndex == STFECHANNEL_UNUSED)
			continue;

		/* Deallocation condition */
		stptiTSHAL_TSInputs[Index].Stfe.Config.MemoryPktNum = 0;

		stptiTSHAL_TSInputStfeConfigure(&stptiTSHAL_TSInputs[Index].Stfe.Config);
	}
	return ST_NO_ERROR;
}

/**
   @brief  Helper function for memory deallocation

   This function can be called to deallocate all associated memory for a channel

   @param  TSInput_p               A pointer to the STFE block's current parameters/settings

 */
static ST_ErrorCode_t stptiTSHAL_StfeDeallocate(stptiTSHAL_InputDevices_t *TSInput_p)
{
	ST_ErrorCode_t Error = ST_NO_ERROR;
	stpti_printf("");

	// be sure the channel is disable, otherwise system will remain in undefined state
	Error = stptiTSHAL_StfeDisableInput(TSInput_p);

	/* If there is memory allocated for the Pid ram free it */
	if (TSInput_p->Stfe.PIDTableStart_p != NULL) {
		stpti_printf("Freeing PID RAM @ 0x%x", (U32)TSInput_p->Stfe.PIDTableStart_p);
		stptiSupport_MemoryDeallocateForDMA(&TSInput_p->Stfe.PIDTableCachedMemoryStructure);
		TSInput_p->Stfe.PIDTableStart_p = NULL;
		TSInput_p->Stfe.PIDTableSize = 0;
		TSInput_p->Stfe.PIDsEnabledNoWildcard = 0;
	}

	if (TSInput_p->Stfe.PIDTableTracker_p != NULL) {
		stpti_printf("Freeing PID TRACKER RAM @ 0x%x", (U32)TSInput_p->Stfe.PIDTableTracker_p);
		kfree(TSInput_p->Stfe.PIDTableTracker_p);
		TSInput_p->Stfe.PIDTableTracker_p = NULL;
	}

	if (TSInput_p->Stfe.PidsEnabledForTP_p != NULL) {
		stpti_printf("Freeing TP TRACKER RAM @ 0x%x", (U32)TSInput_p->Stfe.PidsEnabledForTP_p);
		kfree(TSInput_p->Stfe.PidsEnabledForTP_p);
		TSInput_p->Stfe.PidsEnabledForTP_p = NULL;
	}

	/* If there is memory allocated for the DMA FIFO RAM then free it */
	if (TSInput_p->Stfe.RAMStart_p != NULL) {
		stpti_printf("Freeing FIFO RAM @ 0x%x", (U32) TSInput_p->Stfe.RAMStart_p);
		stptiSupport_MemoryDeallocateForDMA(&TSInput_p->Stfe.RAMCachedMemoryStructure);
		TSInput_p->Stfe.RAMStart_p = NULL;
		TSInput_p->Stfe.RAMSize = 0;
		TSInput_p->Stfe.AllocatedNumOfPkts = 0;
	}
	return Error;
}

/**
   @brief  Helper function for disabling an input channel

   This function is called to configure an input channel and associated connection.

   @param  TSInput_p         A pointer to the STFE block's current parameters/settings
   @return - A standard st error type...
	- ST_NO_ERROR if no errors
 */
static ST_ErrorCode_t stptiTSHAL_StfeConfigureInput(stptiTSHAL_InputDevices_t *TSInput_p)
{
	ST_ErrorCode_t Error = ST_NO_ERROR;

	/* configure for IB and SWTS */
	if (TSInput_p->StfeType != stptiTSHAL_TSINPUT_STFE_MIB) {
		stptiTSHAL_StfeEnable(TSInput_p->Index_byType, TSInput_p->StfeType, FALSE);
		Error = stptiTSHAL_StfeBlockConfigure(TSInput_p, TSInput_p->Index_byType, TSInput_p->StfeType);
	}

	/* configure for MIB */
	if (TSInput_p->MIBChannel != STFECHANNEL_UNUSED) {
		stptiTSHAL_StfeMIBEnableSubstream(TSInput_p->MIBChannel, FALSE);
		Error = stptiTSHAL_StfeBlockConfigure(TSInput_p, TSInput_p->MIBChannel, stptiTSHAL_TSINPUT_STFE_MIB);
	}
	/* If the routing is TSDMA then setup */
	if (TSInput_p->OutputBlockIndex != STFECHANNEL_UNUSED)
		Error = stptiTSHAL_StfeTsdmaConfigure(TSInput_p);

	if (TSInput_p->MIBChannel != STFECHANNEL_UNUSED)
		stptiTSHAL_StfeMIBEnableSubstream(TSInput_p->MIBChannel, TRUE);

	/* Clearing and syncing PID table */
	/* Clear the PID RAM and trackerTable */
	stptiTSHAL_StfePidTableClear(TSInput_p, ALL_TP_MASK(STFE_Config.NumTPUsed));
	/* For each vdevice in existance on this pDevice that matches the get the pid list */
	Error = stptiTSHAL_StfePidTableSync(TSInput_p, ALL_TP_MASK(STFE_Config.NumTPUsed));

	/* if not mib */
	if (TSInput_p->StfeType != stptiTSHAL_TSINPUT_STFE_MIB)
		stptiTSHAL_StfeEnable(TSInput_p->Index_byType, TSInput_p->StfeType, TRUE);

	TSInput_p->Stfe.Config.InputBlockEnable = TRUE;

	return Error;
}

/**
   @brief  Helper function for disabling an input channel

   This function should be called to disable an input channel and associated connection.

   1/ Disable MIB, SWTS, IB to halt input of data into the system
   2/ if TSDMA Wait for stream to become inactive, clear config
   3/ if DEMUX wait for the MEMDMA to become idle, clear config and clear the TP config for that channel

   @param  TSInput_p         A pointer to the STFE block's current parameters/settings
   @return - A standard st error type...
           - ST_NO_ERROR if no errors
 */
static ST_ErrorCode_t stptiTSHAL_StfeDisableInput(stptiTSHAL_InputDevices_t *TSInput_p)
{
	ST_ErrorCode_t Error = ST_NO_ERROR;
	BOOL isEnabled = stptiTSHAL_StfeGetIBEnableStatus((U16)TSInput_p->Index_byType, TSInput_p->StfeType);

	if (TSInput_p->StfeType == stptiTSHAL_TSINPUT_STFE_MIB)
		isEnabled |= stptiTSHAL_StfeMIBReadSubstreamStatus(TSInput_p->MIBChannel);

	if (isEnabled == TRUE) {

		Error = stptiTSHAL_StfePidTableClear(TSInput_p, ALL_TP_MASK(STFE_Config.NumTPUsed));
		if (Error != ST_NO_ERROR)
			return Error;

		/* Wait a short while to allow any packets to propagate through the IB */
		msleep(10);

		if (TSInput_p->MIBChannel != STFECHANNEL_UNUSED)
			stptiTSHAL_StfeMIBEnableSubstream(TSInput_p->MIBChannel, FALSE);

		/* Stop the input block from rx'ing data */
		if (TSInput_p->StfeType != stptiTSHAL_TSINPUT_STFE_MIB)
			stptiTSHAL_StfeEnable(TSInput_p->Index_byType, TSInput_p->StfeType, FALSE);

		/* If the block is routed to the TSDMA we must disable the TSDMA then the MIB before the TP */
		if (TSInput_p->OutputBlockIndex != STFECHANNEL_UNUSED)
			Error =
			    stptiTSHAL_StfeTsdmaFlushSubstream(TSInput_p->IBBlockIndex, TSInput_p->OutputBlockIndex);

		/* Disable the MEMDMA now */
		if (ST_NO_ERROR == Error) {
			if (TSInput_p->MemDmaChannel != STFECHANNEL_UNUSED)
				Error = stptiTSHAL_StfeDisableTP(TSInput_p, ALL_TP_MASK(STFE_Config.NumTPUsed));
		}
	}

	TSInput_p->Stfe.Config.InputBlockEnable = FALSE;

	return Error;
}

/**
   @brief  Helper function for configuring an input block ( IB, SWTS, MIB)

   This function configures the some of the registers of the standard STFE input( SWTS IB MIB)
   This function is called at configuration only for MIB because its registers are shared
   by all MIB streams

   @param  TSInput_p         A pointer to the STFE block's current parameters/settings
   @param  Index_byType      The index of the channel to configure
   @param  StfeType          The StfeType of the channel to configure : IB, SWTS, MIB
   @return - A standard st error type...
           - ST_NO_ERROR if no errors
 */
static ST_ErrorCode_t stptiTSHAL_StfeConfigureHWRegister(stptiTSHAL_InputDevices_t * TSInput_p, U32 Index_byType,
							 stptiHAL_TSInputStfeType_t StfeType)
{
	ST_ErrorCode_t Error = ST_NO_ERROR;
	U32 IFormat = 0;
	U8 PIDBitSize = stptiHAL_DVB_PID_BIT_SIZE;
	U32 PIDBitOffset;
	U32 packet_lenght =
	    ((StfeType ==
	      stptiTSHAL_TSINPUT_STFE_MIB) ? (TSInput_p->Stfe.Config.PacketLength +
					      STFE_TAG_BYTE_SIZE) : (TSInput_p->Stfe.Config.PacketLength));

	switch (TSInput_p->Stfe.Config.PacketLength) {
	case (stptiHAL_TSINPUT_PACKET_LENGTH_DVB):
	case (stptiHAL_TSINPUT_PACKET_LENGTH_TAGGED_DVB):
	default:
		PIDBitOffset = STFE_DVB_PID_BIT_OFFSET;
		break;
	}

	stptiTSHAL_StfeWritePktLength(Index_byType, StfeType, packet_lenght);

	if (StfeType != stptiTSHAL_TSINPUT_STFE_SWTS) {
		/* If routing == tsout then disable the pid filter */
		if (stptiTSHAL_Stfe_DisablePidFilter(TSInput_p->TSInputScenario, StfeType))
			TSInput_p->Stfe.PIDFilterEnabled = FALSE;
		else
			TSInput_p->Stfe.PIDFilterEnabled = TRUE;

		stptiTSHAL_StfePIDFilterConfigure(Index_byType, StfeType, PIDBitSize, PIDBitOffset,
						  TSInput_p->Stfe.PIDFilterEnabled);

		/* if using CableCard, when configure Mib, force to be in parallel mode */
		if (TRUE == TSInput_p->Stfe.Config.SerialNotParallel
		    && (!stptiTSHAL_Stfe_NeedParallelMode(TSInput_p->TSInputScenario, StfeType))) {
			IFormat = STFE_INPUT_FORMAT_SERIAL_NOT_PARALLEL;
			/* We may need to provide option to the API at a later date */
			IFormat |= STFE_INPUT_FORMAT_BYTE_ENDIANNESS;
		}
		if (TRUE == TSInput_p->Stfe.Config.AsyncNotSync) {
			IFormat |= STFE_INPUT_FORMAT_ASYNC_NOT_SYNC;
		}
		/* when merged inout on IB block, SOP has to be used in place of Lock, sync, Drop */
		if ((TRUE == TSInput_p->Stfe.Config.AlignByteSOP)
		    || (stptiTSHAL_Stfe_DisableLock(TSInput_p->TSInputScenario))) {
			IFormat |= STFE_INPUT_FORMAT_ALIGN_BYTE_SOP;
		}
		if ((TRUE == TSInput_p->Stfe.Config.InvertTSClk)
		    && (!stptiTSHAL_Stfe_NeedParallelMode(TSInput_p->TSInputScenario, StfeType))) {
			IFormat |= STFE_INPUT_FORMAT_INVERT_TS_CLK;
		}
		if (TRUE == TSInput_p->Stfe.Config.IgnoreErrorInByte) {
			IFormat |= STFE_INPUT_FORMAT_IGNORE_ERROR_BYTE;
		}
		if (TRUE == TSInput_p->Stfe.Config.IgnoreErrorInPkt) {
			IFormat |= STFE_INPUT_FORMAT_IGNORE_ERROR_PKT;
		}
		if (TRUE == TSInput_p->Stfe.Config.IgnoreErrorAtSOP) {
			IFormat |= STFE_INPUT_FORMAT_IGNORE_ERROR_SOP;
		}

		stptiTSHAL_StfeWriteInputFormat(Index_byType, StfeType, IFormat);
	}

	return Error;
}

/**
   @brief  Helper function for configuring an input block ( IB, SWTS, MIB)

   This function should be called to configure a standard STFE input: SWTS IB MIB
   This function can be called any time because its registers belongs to a stream only

   @param  TSInput_p         A pointer to the STFE block's current parameters/settings
   @param  Index_byType      The index of the channel to configure
   @param  StfeType          The StfeType of the channel to configure : IB, SWTS, MIB
   @return - A standard st error type...
	- ST_NO_ERROR if no errors
 */
static ST_ErrorCode_t stptiTSHAL_StfeBlockConfigure(stptiTSHAL_InputDevices_t *TSInput_p, U32 Index_byType,
						    stptiHAL_TSInputStfeType_t StfeType)
{
	ST_ErrorCode_t Error = ST_NO_ERROR;
	U8 Lock = 0;
	U8 Drop = 0;
	U8 Token = 0;

	U32 PktSize = STFE_DVB_DMA_PKT_SIZE; /* Default size for DVB */

	stpti_printf("Index %d", TSInput_p->IBBlockIndex);

	switch (TSInput_p->Stfe.Config.PacketLength) {
	case (stptiHAL_TSINPUT_PACKET_LENGTH_DVB):
	case (stptiHAL_TSINPUT_PACKET_LENGTH_TAGGED_DVB):
	default:
		if (TRUE == TSInput_p->Stfe.Config.SyncLDEnable) {
			Lock = STFE_DVB_DEFAULT_LOCK;
			Drop = STFE_DVB_DEFAULT_DROP;
			Token = stptiHAL_SYNC_BYTE;
		}
		PktSize = STFE_DVB_DMA_PKT_SIZE;
		break;
	}

	if (stptiTSHAL_Stfe_DisableLock(TSInput_p->TSInputScenario)) {
		Lock = STFE_DVB_NO_LOCK_DROP;
		Drop = STFE_DVB_NO_LOCK_DROP;
	}

	/* Configure the input parameters based on transport type */
	stptiTSHAL_StfeWriteSLD(Index_byType, StfeType, Lock, Drop, Token);

	/* Configure stream tagging - modify the index to the correct SWTS number - This function also configures for replace or add tags. */
	stptiTSHAL_StfeTAGBytesConfigure(Index_byType, StfeType, TSInput_p->Tag, TSInput_p->Stfe.Config.InputTagging);
	/* Use default for replace tag register as we are going to assume the input is not tagged and the SWTS is going to add the tag */

	/* Setup internal RAM pointers */
	stptiTSHAL_StfeWriteToIBInternalRAMPtrs(Index_byType, StfeType);

	if (StfeType != stptiTSHAL_TSINPUT_STFE_SWTS) {

		if (TSInput_p->MemDmaChannel != STFECHANNEL_UNUSED)
			stptiTSHAL_StfeWriteToInternalRAMDDRPtrs(TSInput_p,
								 TSInput_p->Stfe.AllocatedNumOfPkts * PktSize, PktSize);

		/* Enable PID filtering on the input block - PID offset is dependant on transport */
		if (TSInput_p->PidFilterIndex != STFECHANNEL_UNUSED)
			stptiTSHAL_StfeWritePIDBase(TSInput_p->PidFilterIndex,
						    (U32)stptiSupport_VirtToPhys(&TSInput_p->Stfe.PIDTableCachedMemoryStructure,
										 TSInput_p->Stfe.PIDTableStart_p));
	}

	if (StfeType != stptiTSHAL_TSINPUT_STFE_MIB) {
		/* for MIB it is done on configuration stage */
		stptiTSHAL_StfeConfigureHWRegister(TSInput_p, Index_byType, StfeType);
	}

	if (StfeType == stptiTSHAL_TSINPUT_STFE_IB) {
		/* Setup Routing to TSDMA or MEMDMA */
		stptiTSHAL_StfeTAGCounterSelect(Index_byType, (U8)(TSInput_p->Stfe.Config.ClkRvSrc));

		stptiTSHAL_HW_function_stfe->stptiTSHAL_StfeSystemIBRoute(Index_byType, TSInput_p->Stfe.Config.Routing);
	}

	if (stptiTSHAL_Stfe_NeedCscc(TSInput_p->TSInputScenario))
		stptiTSHAL_HW_function_stfe->stptiTSHAL_StfeCCSCConfig();

	return Error;
}

/**
   @brief  Helper function to configure the leaky Pid

   This function should be called to configure the leaky Pid

   @param  TSInput_p          A pointer to the STFE block's current parameters/settings
   @param  TP_requester       Index Of the TP core that requests the action
   @return                    none
 */
static ST_ErrorCode_t stptiTSHAL_StfeSetLeakyPid(stptiTSHAL_InputDevices_t *TSInput_p)
{
	U32 i = 0;
	/* LeakyPID is only used if no wildcard pids (PIDFilterEnable == TRUE), but other pids are set (i.e. Input is Active) */
	BOOL EnableLeakyPID = (TSInput_p->Stfe.PIDFilterEnabled == TRUE) && (TSInput_p->Stfe.PIDsEnabledNoWildcard != 0);

	if (TSInput_p->Stfe.LeakyPidEnabled != EnableLeakyPID) {
		if (!STFE_Config.SoftwareLeakyPID) {
			stptiTSHAL_HW_function_stfe->stptiTSHAL_StfeEnableLeakyPid(TSInput_p->PidFilterIndex,
										   EnableLeakyPID);
		} else {
			mutex_unlock(stptiTSHAL_TSInputMutex_p);

			for (i = 0; i < STFE_Config.NumTPUsed; i++)
				stptiHAL_call(pDevice.HAL_pDeviceEnableSWLeakyPID, stptiOBJMAN_pDeviceObjectHandle(i),
					      TSInput_p->MemDmaChannel, EnableLeakyPID);

			if (mutex_lock_interruptible(stptiTSHAL_TSInputMutex_p))
				return ST_ERROR_TIMEOUT;

			CHECK_CHANNEL_IS_STILL_SET();

		}
		TSInput_p->Stfe.LeakyPidEnabled = EnableLeakyPID;
	}
	return ST_NO_ERROR;
}

/**
   @brief  Advertise change in Activity State for an Input

   This function is called when an input becomes active or inactive, usually determined by whether
   any pids are set.  It allows the input filtering to be able to be adjusted to prevent any
   packets getting to the demux (if going inactive), or filtering to be relaxed (ungated) if going
   active.

   This is important for "leaky pid".  The STFE has a pid filter, but packets still leak through.
   This is necessary to speed up PAT/PMT section acquistion, as 4 packets must be gathered before
   they are passed onto the demux.  When no pids are set "leaky pid" must be disabled as the demux
   might be put into a state where it can't read packets (such as power down).  In such cases the
   STFE pid filter stops all packets getting through but the STFE is still active to make sure
   that the datapath can empty.

   For wildcard pid, "leaky pid" is unnecessary since every packet will be presented to the demux.
   For this in place of disabling the PID filtering we are swapping the pid filter base with the ShadowPidTable
   in which all the pids are enable.This help us from preventing the packer drops.
   On older devices such as 7108 "leaky pid" results in an interrupt load, so it is desirable to only enable this when needed.

   @param  TSInput_p          A pointer to the STFE block's current parameters/settings
   @return                    none
 */
static ST_ErrorCode_t stptiTSHAL_StfeChangeActivityState(stptiTSHAL_InputDevices_t *TSInput_p)
{
	ST_ErrorCode_t Error = ST_NO_ERROR;
	U32 i = 0;
	bool WildcardPidEnabled = false;

	for (i = 0; i < STFE_Config.NumTPUsed; i++) {
		if (TSInput_p->Stfe.PIDTableTracker_p[stptiTSHAL_DVB_WILDCARD_PID + STFE_PID_TRACKER_RAM_SIZE*i] == 0) {
			stptiTSHAL_StfeWritePIDBase(TSInput_p->PidFilterIndex,
				(U32)stptiSupport_VirtToPhys(&TSInput_p->Stfe.PIDTableCachedMemoryStructure,
				TSInput_p->Stfe.PIDTableStart_p));
		} else {
			stptiTSHAL_StfeWritePIDBase(TSInput_p->PidFilterIndex,
				(U32)stptiSupport_VirtToPhys(&ShadowPIDTableCachedMemoryStructure,
				ShadowPIDTableStart_p));
			WildcardPidEnabled = true;
		}
	}

	if (!WildcardPidEnabled) {
		if (TSInput_p->MIBChannel != STFECHANNEL_UNUSED)
			stptiTSHAL_StfePIDFilterEnable(TSInput_p->MIBChannel,
			       stptiTSHAL_TSINPUT_STFE_MIB, TSInput_p->Stfe.PIDFilterEnabled);
		else
			stptiTSHAL_StfePIDFilterEnable(TSInput_p->Index_byType,
			       TSInput_p->StfeType, TSInput_p->Stfe.PIDFilterEnabled);
		Error = stptiTSHAL_StfeSetLeakyPid(TSInput_p);
	}
	return Error;
}

static ST_ErrorCode_t stptiTSHAL_StfeSetClearTPTransmission(stptiTSHAL_InputDevices_t *TSInput_p, U32 TP_requester, BOOL Set)
{
	ST_ErrorCode_t Error = ST_NO_ERROR;

	if (Set == TRUE) {
		if (TSInput_p->Stfe.PidsEnabledForTP_p[TP_requester] == 0 && TSInput_p->MemDmaChannel != STFECHANNEL_UNUSED)
			stptiTSHAL_StfeConfigureTP(TSInput_p, 1 << TP_requester);

		TSInput_p->Stfe.PidsEnabledForTP_p[TP_requester]++;

	} else {
		if (TSInput_p->Stfe.PidsEnabledForTP_p[TP_requester] == 1 && TSInput_p->MemDmaChannel != STFECHANNEL_UNUSED)
			stptiTSHAL_StfeDisableTP(TSInput_p, 1 << TP_requester);

		if (TSInput_p->Stfe.PidsEnabledForTP_p[TP_requester])
			TSInput_p->Stfe.PidsEnabledForTP_p[TP_requester]--;
	}
	return Error;
}
/**
   @brief  STFE Hardware worker function

   Set a PID number in the PID filter by modifying the PID table in RAM.
   This function assumes PID width of 13 bits hence 8192 values and
   1 bit per PID hence 32 PIDs can be set in one 32 bit word.

   @param  TSInput_p          A pointer to the STFE block's current parameters/settings
   @param  Pid                Pid value to select
   @param  TP_requester       Index Of the TP core that requests the action
   @return                    none
 */
static ST_ErrorCode_t stptiTSHAL_StfeSetPid(stptiTSHAL_InputDevices_t *TSInput_p, U16 Pid, U32 TP_requester)
{
	U32 PidTableVirtStartAddr = (U32)TSInput_p->Stfe.PIDTableStart_p;
	ST_ErrorCode_t Error = ST_NO_ERROR;

	U32 Value;
	U32 BitPosition;
	U32 AddressOffset;

	stpti_printf("Pid 0x%03x", Pid);

	stptiTSHAL_StfeSetClearTPTransmission(TSInput_p, TP_requester, TRUE);

	if (Pid < stptiTSHAL_DVB_WILDCARD_PID) {
		AddressOffset = PidAddressOffset(Pid);
		BitPosition = PidBitPosition(Pid);

		Value = stptiSupport_ReadReg32((volatile U32 *)(PidTableVirtStartAddr + AddressOffset)) | BitPosition;
		stptiSupport_WriteReg32((volatile U32 *)(PidTableVirtStartAddr + AddressOffset), Value);

		/* flush the cache for the pid */
		stptiSupport_FlushRegion(&TSInput_p->Stfe.PIDTableCachedMemoryStructure,
					 (void *)(PidTableVirtStartAddr + AddressOffset), sizeof(U32));

		TSInput_p->Stfe.PIDsEnabledNoWildcard++;
	}

	Error = stptiTSHAL_StfeChangeActivityState(TSInput_p);

	return Error;
}

/**
   @brief  STFE Hardware worker function

   Clear a PID number in the PID filter by modifying the PID table in RAM.
   This function assumes PID width of 13 bits hence 8192 values and
   1 bit per PID hence 32 PIDs can be set in one 32 bit word.

   @param  TSInput_p          A pointer to the STFE block's current parameters/settings
   @param  Pid                Pid value to select
   @param  TP_requester       Index Of the TP core that requests the action
   @return                    none
 */
static ST_ErrorCode_t stptiTSHAL_StfeClearPid(stptiTSHAL_InputDevices_t *TSInput_p, U16 Pid, U32 TP_requester)
{
	U32 PidTableVirtStartAddr = (U32) TSInput_p->Stfe.PIDTableStart_p;
	ST_ErrorCode_t Error = ST_NO_ERROR;

	U32 Value;
	U32 BitPosition;
	U32 AddressOffset;

	stpti_printf("Pid 0x%03x", Pid);

	if (Pid < stptiTSHAL_DVB_WILDCARD_PID) {
		AddressOffset = PidAddressOffset(Pid);
		BitPosition = PidBitPosition(Pid);

		Value =
		    stptiSupport_ReadReg32((volatile U32 *)(PidTableVirtStartAddr + AddressOffset)) & (~BitPosition);
		stptiSupport_WriteReg32((volatile U32 *)(PidTableVirtStartAddr + AddressOffset), Value);

		/* flush the cache for the pid */
		stptiSupport_FlushRegion(&TSInput_p->Stfe.PIDTableCachedMemoryStructure,
					 (void *)(PidTableVirtStartAddr + AddressOffset), sizeof(U32));

		if (TSInput_p->Stfe.PIDsEnabledNoWildcard != 0)
			TSInput_p->Stfe.PIDsEnabledNoWildcard--;

	}

	Error = stptiTSHAL_StfeChangeActivityState(TSInput_p);
	if (Error != ST_NO_ERROR)
		return Error;

	stptiTSHAL_StfeSetClearTPTransmission(TSInput_p, TP_requester, FALSE);

	return Error;
}

/**
   @brief  Helper function for configuring an input to a TSDMA block

   This function should be called to configure a STFE TSDMA substream.

   @param  TSInput_p          A pointer to the STFE block's current parameters/settings
   @return - A standard st error type...
           - ST_NO_ERROR if no errors
 */
static ST_ErrorCode_t stptiTSHAL_StfeTsdmaConfigure(stptiTSHAL_InputDevices_t *TSInput_p)
{
	ST_ErrorCode_t Error = ST_NO_ERROR;
	U32 DestFormat = 0;
	BOOL setPace = 0;

	stptiTSHAL_StfeTsdmaConfigureInternalRAM(TSInput_p->Index_byType, TSInput_p->StfeType);

	/* b) Configure the packet size of the TSDMA to expect tranport packets including tags */
	stptiTSHAL_StfeTsdmaConfigurePktSize(TSInput_p->IBBlockIndex,
					     (U32) (stptiHAL_TSINPUT_PACKET_LENGTH_TAGGED_DVB));

	/* c) Setup stream format - Tell the TSDMA to expect tagged streams; tagged data will always be output from SWTS and IB */
	stptiTSHAL_StfeTsdmaConfigureStreamFormat(TSInput_p->IBBlockIndex, TRUE);

	/* d) Setup pacing if required */
	setPace = ((TSInput_p->StfeType == stptiTSHAL_TSINPUT_STFE_SWTS) &&
		   (TSInput_p->Stfe.Config.InputTagging != stptiTSHAL_TSINPUT_TAGS_NONE));
	stptiTSHAL_StfeTsdmaConfigurePacing(TSInput_p->IBBlockIndex,
					    TSInput_p->Stfe.Config.OutputPacingRate,
					    TSInput_p->Stfe.Config.OutputPacingClkSrc,
					    (TSInput_p->Stfe.Config.InputTagging ==
					     stptiTSHAL_TSINPUT_TAGS_TTS), setPace);

	/* e) Configure the output - always parallel - remove TAG for CIMode */
	DestFormat = STFE_TSDMA_DEST_FORMAT_WAIT_PKT;
	DestFormat |= (TSInput_p->Stfe.Config.OutputSerialNotParallel) ? STFE_TSDMA_DEST_FORMAT_SERIAL_N_PARALLEL : 0;
	DestFormat |= (TSInput_p->Stfe.Config.CIMode) ? STFE_TSDMA_DEST_FORMAT_REMOVE_TAG : 0;

	stptiTSHAL_HW_function_stfe->stptiTSHAL_StfeTsdmaConfigureDestFormat(TSInput_p->OutputBlockIndex, DestFormat);

	/* f) Set FIFO trigger - currently use default */

	/* g) Set route to enable input/output - Currently bypass 'a la Dave mode' not used */
	stptiTSHAL_HW_function_stfe->stptiTSHAL_StfeTsdmaEnableInputRoute(TSInput_p->IBBlockIndex,
									  TSInput_p->OutputBlockIndex, TRUE);

	return (Error);
}

/**
   @brief  STFE Hardware worker function

   Write in the MEMDMA ram the Address pointer to DDR for each block

   @param  TSInput_p          A pointer to the STFE block's current parameters/settings
   @param  BufferSize         The size of the buffer in the DDR RAM
   @param  PktSize            StreamID to select input block
   @return                    none
 */
static void stptiTSHAL_StfeWriteToInternalRAMDDRPtrs(stptiTSHAL_InputDevices_t *TSInput_p, U32 BufferSize, U32 PktSize)
{
	U32 IndexInRam = 0;
	U32 RamToBeUsed = 0;
	U32 StartAddressOfFifos = 0;
	U32 FifoSize = 0;
	U32 InternalBase = 0;

	if (TSInput_p->MIBChannel == STFECHANNEL_UNUSED) {
		RamToBeUsed =
		    stptiTSHAL_HW_function_stfe->stptiTSHAL_StfeMemdma_ram2beUsed(TSInput_p->Index_byType,
										  TSInput_p->StfeType, &IndexInRam);
	} else {
		RamToBeUsed =
		    stptiTSHAL_HW_function_stfe->stptiTSHAL_StfeMemdma_ram2beUsed(TSInput_p->MIBChannel,
										  stptiTSHAL_TSINPUT_STFE_MIB,
										  &IndexInRam);
	}

	StartAddressOfFifos = STFE_Config.RamConfig.RAMAddr[RamToBeUsed];
	FifoSize = STFE_Config.RamConfig.FifoSize[RamToBeUsed];

	InternalBase = StartAddressOfFifos + (IndexInRam * FifoSize);

	stptiTSHAL_HW_function_stfe->stptiTSHAL_StfeWrite_RamInputNode_ptrs(TSInput_p->MemDmaChannel,
									    TSInput_p->Stfe.RAMStart_p,
									    TSInput_p->Stfe.RAMCachedMemoryStructure,
									    BufferSize, PktSize, FifoSize,
									    InternalBase);
}

/**
   @brief  STFE Hardware worker function

   Disable Mib if no streams are used

   @param			none
   @return			none
 */
static void stptiTSHAL_StfeDisableMIB(void)
{
	BOOL MIB_used = FALSE;
	U32 Index = 0;
	for (Index = 0; Index < (STFE_Config.NumIBs + STFE_Config.NumSWTSs + STFE_Config.NumMIBs); Index++) {
		if (stptiTSHAL_TSInputs[Index].MIBChannel != STFECHANNEL_UNUSED) {
			MIB_used = TRUE;
			break;
		}
	}
	if (MIB_used == FALSE)
		stptiTSHAL_MibSetClear(FALSE);

}

/**
   @brief  this function take the input configuration and return the scenario it correspond

   Checking on the input parameters, it deduces which scenario is correspond.

   @param  TSInputConfigParams_p	input configuration
   @return stptiHAL_TSInputScenario_t   The correspondent scenario
 */
static stptiHAL_TSInputScenario_t stptiTSHAL_Stfe_ValidateInput(stptiTSHAL_TSInputConfigParams_t *
								TSInputConfigParams_p)
{
	stptiHAL_TSInputStfeType_t StfeType = ExtractIBTypeFromStreamID(TSInputConfigParams_p->StreamID);

	if (TSInputConfigParams_p->MemoryPktNum == 0)
		return stptiHAL_TSINPUT_SCENARIO_Deallocate;

	if ((StfeType == stptiTSHAL_TSINPUT_STFE_IB)
	    && (TSInputConfigParams_p->Routing == stptiTSHAL_TSINPUT_DEST_DEMUX)
	    && (TSInputConfigParams_p->CIMode == FALSE)
	    && (TSInputConfigParams_p->InputTagging == stptiTSHAL_TSINPUT_TAGS_NONE))
		return stptiHAL_TSINPUT_SCENARIO_1;

	if ((StfeType == stptiTSHAL_TSINPUT_STFE_IB)
	    && (TSInputConfigParams_p->Routing < stptiTSHAL_TSINPUT_DEST_EXT_TSOUT0)
	    && (TSInputConfigParams_p->CIMode == TRUE))
		return stptiHAL_TSINPUT_SCENARIO_2;

	if ((StfeType == stptiTSHAL_TSINPUT_STFE_IB)
	    && ((U32) ConvertDestToTSDMAOutput(TSInputConfigParams_p->Routing) < STFE_Config.NumCCSCs)
	    && (TSInputConfigParams_p->Routing < stptiTSHAL_TSINPUT_DEST_EXT_TSOUT0)
	    && (TSInputConfigParams_p->CIMode == FALSE)
	    && (TSInputConfigParams_p->InputTagging == stptiTSHAL_TSINPUT_TAGS_NONE))
		return stptiHAL_TSINPUT_SCENARIO_3;

	if ((StfeType == stptiTSHAL_TSINPUT_STFE_SWTS)
	    && (TSInputConfigParams_p->Routing < stptiTSHAL_TSINPUT_DEST_EXT_TSOUT0)
	    && (TSInputConfigParams_p->CIMode == TRUE))
		return stptiHAL_TSINPUT_SCENARIO_4;

	if ((StfeType == stptiTSHAL_TSINPUT_STFE_SWTS)
	    && ((U32) ConvertDestToTSDMAOutput(TSInputConfigParams_p->Routing) < STFE_Config.NumCCSCs)
	    && (TSInputConfigParams_p->Routing < stptiTSHAL_TSINPUT_DEST_EXT_TSOUT0)
	    && (TSInputConfigParams_p->CIMode == FALSE))
		return stptiHAL_TSINPUT_SCENARIO_5;

	if ((StfeType == stptiTSHAL_TSINPUT_STFE_IB)
	    && (TSInputConfigParams_p->Routing >= stptiTSHAL_TSINPUT_DEST_EXT_TSOUT0)
	    && (TSInputConfigParams_p->CIMode == FALSE))
		return stptiHAL_TSINPUT_SCENARIO_6;

	if ((StfeType == stptiTSHAL_TSINPUT_STFE_SWTS)
	    && (TSInputConfigParams_p->Routing >= stptiTSHAL_TSINPUT_DEST_EXT_TSOUT0)
	    && (TSInputConfigParams_p->CIMode == FALSE))
		return stptiHAL_TSINPUT_SCENARIO_7;

	if ((StfeType == stptiTSHAL_TSINPUT_STFE_MIB)
	    && (TSInputConfigParams_p->Routing == stptiTSHAL_TSINPUT_DEST_DEMUX)
	    && (TSInputConfigParams_p->CIMode == FALSE))
		return stptiHAL_TSINPUT_SCENARIO_8;

	if ((StfeType == stptiTSHAL_TSINPUT_STFE_IB)
	    && ((U32) ConvertDestToTSDMAOutput(TSInputConfigParams_p->Routing) < STFE_Config.NumCCSCs)
	    && (TSInputConfigParams_p->Routing < stptiTSHAL_TSINPUT_DEST_EXT_TSOUT0)
	    && (TSInputConfigParams_p->CIMode == FALSE)
	    && (TSInputConfigParams_p->InputTagging == stptiTSHAL_TSINPUT_TAGS_STFE))
		return stptiHAL_TSINPUT_SCENARIO_9;

	if ((StfeType == stptiTSHAL_TSINPUT_STFE_IB)
	    && (TSInputConfigParams_p->Routing == stptiTSHAL_TSINPUT_DEST_DEMUX)
	    && (TSInputConfigParams_p->CIMode == FALSE)
	    && (TSInputConfigParams_p->InputTagging == stptiTSHAL_TSINPUT_TAGS_STFE))
		return stptiHAL_TSINPUT_SCENARIO_10;

	return stptiHAL_TSINPUT_SCENARIO_UNDEFINED;
}

/**
   @brief  Stfe worker function

   Calculate the MEMDMA channel index

   @param  TSInput_p          A pointer to the STFE block's current parameters/settings
   @return                    MEMDMA channel index
 */
static U32 CalculateMEMDMAChannel(stptiTSHAL_InputDevices_t *TSInput_p)
{
	U32 MemDmaChannel = STFECHANNEL_UNUSED;

	switch (TSInput_p->TSInputScenario) {
	case stptiHAL_TSINPUT_SCENARIO_3:
	case stptiHAL_TSINPUT_SCENARIO_5:
	case stptiHAL_TSINPUT_SCENARIO_8:
		MemDmaChannel = STFE_Config.NumIBs + TSInput_p->MIBChannel;
		break;
	case stptiHAL_TSINPUT_SCENARIO_1:
	case stptiHAL_TSINPUT_SCENARIO_10:
		MemDmaChannel = TSInput_p->IBBlockIndex;
		break;
	default:
		MemDmaChannel = STFECHANNEL_UNUSED;
		break;
	}
	return MemDmaChannel;
}

/**
   @brief  Stfe worker function

   Calculate the Pid Filter Index channel index

   @param  TSInput_p          A pointer to the STFE block's current parameters/settings
   @return                    Pid Filter index
 */
static U32 CalculatePidFilterIndexChannel(stptiTSHAL_InputDevices_t *TSInput_p)
{
	U32 PidFilterIndex = STFECHANNEL_UNUSED;

	switch (TSInput_p->TSInputScenario) {
	case stptiHAL_TSINPUT_SCENARIO_3:
	case stptiHAL_TSINPUT_SCENARIO_5:
	case stptiHAL_TSINPUT_SCENARIO_8:
		PidFilterIndex = STFE_Config.NumIBs + TSInput_p->MIBChannel;
		break;
	case stptiHAL_TSINPUT_SCENARIO_1:
	case stptiHAL_TSINPUT_SCENARIO_6:
		PidFilterIndex = TSInput_p->Index_byType;
		break;
	default:
		PidFilterIndex = STFECHANNEL_UNUSED;
		break;
	}
	return PidFilterIndex;
}

/*
 *  this three following function match a Mib StreadID with the MIB stream channel
 *  An additional table is used to track this information
 */
/**
   @brief  Stfe worker function

   find the Mib index for the input StreamId

   @param  StreamID	      API streamID value
   @return                    Index of Mib Stream
 */
static U32 FindStreamID_MIB(U32 StreamID)
{
	U32 i;

	for (i = 0; i < STFE_Config.NumMIBs; i++) {
		if (StreamID == MibChannel_StreamId[i])
			return i;
	}
	return STFE_STREAMID_NONE;
}

/**
   @brief  Stfe worker function

   find the free Mib index entry for the input StreamId

   @param  StreamID	      API streamID value
   @return                    Index of Mib Stream
 */
static U32 GetStreamID_MIB(U32 StreamID)
{
	U32 i;

	for (i = 0; i < STFE_Config.NumMIBs; i++) {
		if (STFE_STREAMID_NONE == MibChannel_StreamId[i]) {
			MibChannel_StreamId[i] = StreamID;
			return i;
		}
	}
	return STFE_STREAMID_NONE;
}

/**
   @brief  Stfe worker function

   delete the input StreamId in its Mib index entry

   @param  StreamID	      API streamID value
   @return                    Index of Mib Stream
 */
static void ResetStreamID_MIB(U32 StreamID)
{
	U32 i;

	for (i = 0; i < STFE_Config.NumMIBs; i++) {
		if (StreamID == MibChannel_StreamId[i]) {
			MibChannel_StreamId[i] = STFE_STREAMID_NONE;
		}
	}
}

/**
   @brief  Stfe worker function

   Calculate an index to an input source (the order is: IB, SWTS, MIB)
   Firstly, the routine Check if it is an MIB stream, if so, look for an empty channel

   @param  StreamID           API streamID value
   @param  AllocateMibChannel In case of Mib channel for that stream does not exist, allocate it
   @return IBBlockIndex       Index of the input block
 */
static U32 ConvertAPItoTSHALIndex(U32 StreamID, BOOL AllocateMibChannel)
{
	U32 IBBlockIndex = STFE_STREAMID_NONE;

	if (!stptiHAL_StreamIDTestNone(StreamID)) {
		if (stptiHAL_StreamIDTestMIB(StreamID)) {
			/* Check if this streamID is being used in the MIB */
			IBBlockIndex = FindStreamID_MIB(StreamID);

			if ((IBBlockIndex == STFE_STREAMID_NONE) && (AllocateMibChannel == TRUE))
				IBBlockIndex = GetStreamID_MIB(StreamID);

			if (IBBlockIndex != STFE_STREAMID_NONE)
				IBBlockIndex += STFE_Config.NumIBs + STFE_Config.NumSWTSs;
		} else if (stptiHAL_StreamIDTestSWTS(StreamID)) {
			if ((StreamID & ~STFE_STREAMID_SWTS) < STFE_Config.NumSWTSs) {
				IBBlockIndex = (StreamID & ~STFE_STREAMID_SWTS) + STFE_Config.NumIBs;
			}
		} else if (StreamID < STFE_Config.NumIBs) {
			IBBlockIndex = StreamID;
		}
	}
	return (IBBlockIndex);
}

/**
   @brief  Stfe worker function

   Calculate an index to an input source (the order is: IB, SWTS, MIB)
   Mind: in some cases StreamID is passed, but when CC is used, the parameter is the TAG

   @param  Tag	              API Tag value
   @return IBBlockIndex       Index of the input block
 */
static U32 ConvertTagtoTSHALIndex(U32 Tag)
{
	U32 IBBlockIndex = STFE_STREAMID_NONE;
	U32 Index;

	if (!stptiHAL_StreamIDTestNone(Tag)) {
		if (stptiHAL_StreamIDTestMIB(Tag)) {
			IBBlockIndex = FindStreamID_MIB(Tag);

			if (IBBlockIndex != STFE_STREAMID_NONE)
				IBBlockIndex += STFE_Config.NumIBs + STFE_Config.NumSWTSs;
		} else {
			for (Index = 0; Index < (STFE_Config.NumIBs + STFE_Config.NumSWTSs); Index++) {
				if (stptiTSHAL_TSInputs[Index].Tag == Tag) {
					IBBlockIndex = Index;
					break;
				}
			}
		}
	}
	return (IBBlockIndex);
}

/**
   @brief  Stfe worker function

   Extract the IB type from the streamID

   @param  StreamID           API streamID value
   @return IBType             the type of the Stream
 */
static stptiHAL_TSInputStfeType_t ExtractIBTypeFromStreamID(U32 StreamID)
{
	stptiHAL_TSInputStfeType_t IBType = stptiTSHAL_TSINPUT_STFE_NONE;

	if (!stptiHAL_StreamIDTestNone(StreamID)) {
		if (stptiHAL_StreamIDTestMIB(StreamID)) {
			IBType = stptiTSHAL_TSINPUT_STFE_MIB;
		} else if (stptiHAL_StreamIDTestSWTS(StreamID)) {
			IBType = stptiTSHAL_TSINPUT_STFE_SWTS;
		} else {
			IBType = stptiTSHAL_TSINPUT_STFE_IB;
		}
	}
	return (IBType);
}

/**
   @brief  Stfe worker function

   Calculate the TSDMA output index

   @param  Routing            DMA route value
   @return OutputBlockIndex   Index of the TSOUT
 */
static U32 ConvertDestToTSDMAOutput(stptiTSHAL_TSInputDestination_t Routing)
{
	U32 OutputBlockIndex = STFECHANNEL_UNUSED;

	switch (Routing) {
	case stptiTSHAL_TSINPUT_DEST_TSOUT0:
	case stptiTSHAL_TSINPUT_DEST_EXT_TSOUT0:
		OutputBlockIndex = 0;
		break;
	case stptiTSHAL_TSINPUT_DEST_TSOUT1:
	case stptiTSHAL_TSINPUT_DEST_EXT_TSOUT1:
		OutputBlockIndex = 1;
		break;
	default:
		OutputBlockIndex = STFECHANNEL_UNUSED;
		break;
	}
	return (OutputBlockIndex);
}

/**
   @brief  Stfe Debug function

   Dump the DDR RAM associate with the input block.
   There are 40 bytes for each line so, pckt header
   is always in the same place every 5 lines

   @param  TSInput_p    A pointer to the STFE block's current parameters/settings
   @param  ctx_p	Info for DATA printing out
   @return none
 */
static void stptiTSHAL_StfeDDRRAMDump(stptiTSHAL_InputDevices_t *TSInput_p, stptiSUPPORT_sprintf_t *ctx_p)
{
	U32 j;

	U32 addr = (U32) TSInput_p->Stfe.RAMStart_p;
	U32 size = TSInput_p->Stfe.RAMSize;
	U32 end = addr + size;

	if (addr != 0) {
		stptiSUPPORT_sprintf(ctx_p, "DDR RAM......\n");
		stptiSupport_InvalidateRegion(&TSInput_p->Stfe.RAMCachedMemoryStructure, (void *)addr, size);
		while (addr < end) {
			stptiSUPPORT_sprintf(ctx_p, "0x%08X ", (U32) stptiSupport_VirtToPhys(&TSInput_p->Stfe.RAMCachedMemoryStructure, (void *)addr));	/* Print address */
			for (j = 0; j < 10; j++) {
				U32 c = 0;
				for (c = 0; c < 4; c++) {
					stptiSUPPORT_sprintf(ctx_p, "%02x", ((U8 *) addr)[c]);
				}
				stptiSUPPORT_sprintf(ctx_p, " ");
				addr += 4;
			}
			stptiSUPPORT_sprintf(ctx_p, "\n");
		}
	}
	stptiSUPPORT_sprintf(ctx_p, "-------------------------------------------------\n");
}

/**
   @brief  Stfe Debug function

   Print out the PID FILTERING info associate with the input block

   @param  TSInput_p    A pointer to the STFE block's current parameters/settings
   @param  ctx_p	Info for DATA printing out
   @return none
 */
static void stptiTSHAL_StfePidEnabledTableDump(stptiTSHAL_InputDevices_t *TSInput_p, stptiSUPPORT_sprintf_t *ctx_p)
{
	U32 i = 0, j = 0;
	U32 Pid_number = 0;
	U32 PidTableStart_size = TSInput_p->Stfe.PIDTableSize;
	U32 PID_phy_addr =
	    stptiSupport_ReadReg32((volatile U32 *)
				   &(STFE_BasePointers.PidFilterBaseAddressPhys_p->
				     PIDF_BASE[TSInput_p->PidFilterIndex]));
	U32 *Tmp = (U32 *) TSInput_p->Stfe.PIDTableStart_p;
	U32 ShadowPidTable = (U32) stptiSupport_VirtToPhys(&ShadowPIDTableCachedMemoryStructure,
				ShadowPIDTableStart_p);

	if (Tmp) {
		U32 value = 0;
		U32 EndAddr = ((U32) Tmp + PidTableStart_size);

		stptiSUPPORT_sprintf(ctx_p, "PID BLOCK...... %d\n", TSInput_p->PidFilterIndex);

		stptiSUPPORT_sprintf(ctx_p, "PID PHY ADDR......  0x%08X\n", PID_phy_addr);

		stptiSUPPORT_sprintf(ctx_p, "PID TABLE...... 0x%08x\n", (U32) Tmp);
		/* If thie check is passed this means the wildcard is enable and PID_phy_addr points to SHADOW Table*/
		if (ShadowPidTable == PID_phy_addr)
			stptiSUPPORT_sprintf(ctx_p, "SHADOW TABLE ON ......... 0x%08X\n", ShadowPidTable);

		while (Tmp < (U32 *) EndAddr) {
			U32 PidMap = *((volatile U32 *)Tmp);
			j = 0;

			while (j < 32) {
				value = ((PidMap >> j) & ALL_TP_MASK(STFE_Config.NumTP));
				if (value) {
					stptiSUPPORT_sprintf(ctx_p, " [0x%04x --> ", Pid_number);

					for (i = 0; i < STFE_Config.NumTP; i++) {
						stptiSUPPORT_sprintf(ctx_p, " TP_%d=%d", i, (value >> i) & 1);
					}
					stptiSUPPORT_sprintf(ctx_p, "]\n");
				}
				j += PIDRespW;
				Pid_number++;
			}
			Tmp++;
		}
		stptiSUPPORT_sprintf(ctx_p, "\n");
	}
	stptiSUPPORT_sprintf(ctx_p, "-------------------------------------------------\n");
}

static void __enable_clk(struct stpti_clk *pti_clk, bool set_parent)
{
	struct clk *clk = NULL;
	int err = 0;

	if (pti_clk->clk) {
		clk = pti_clk->clk;
		if (IS_ERR(clk)) {
			pr_warning("Clock %s not found", pti_clk->name);
			return;
		}
			else
				pti_clk->enable_count++;


		dev_info(&STFE_Config.pdev->dev, "Enabling clock %s\n",
				pti_clk->name);
		err = clk_prepare_enable(pti_clk->clk);
		if (err)
			pr_err("Unable to enable clock %s (%d)\n",
					pti_clk->name, err);
		if (pti_clk->parent_clk && set_parent)
			clk_set_parent(pti_clk->clk,
					pti_clk->parent_clk);

		if (pti_clk->freq) {
			dev_info(&STFE_Config.pdev->dev,
					"Setting clock %s to %uHz\n",
					pti_clk->name,
					pti_clk->freq);
			err = clk_set_rate(pti_clk->clk, pti_clk->freq);
			if (err)
				pr_err("Unable to set rate (%d)\n", err);
		}

		dev_info(&STFE_Config.pdev->dev, "Clock %s @ %luHz\n",
				pti_clk->name,
				clk_get_rate(pti_clk->clk));
	} else {
		dev_warn(&STFE_Config.pdev->dev,
				"Clock %s unavailable\n", pti_clk->name);
	}

}

/**
 * @brief Enables the tag counter 1/STFE clock used for tagging
 */
static void stptiTSHAL_EnableStfeCoreClocks(void)
{
	int i = 0;
	struct stpti_stfe_config *dev_data;
	if (!STFE_Config.pdev)
		return;

	dev_data = STFE_Config.pdev->dev.platform_data;
	for (i = 0;i < dev_data->nb_clk;i++) {
		if(strcmp(dev_data->clk[i].name, "stfe_ccsc") == 0) {
#ifndef CONFIG_ARCH_STI
			if (dev_data->stfe_ccsc_clk_enabled == false) {
				__enable_clk(&(dev_data->clk[i]), false);
				dev_data->stfe_ccsc_clk_enabled = true;
			}
#else
			__enable_clk(&(dev_data->clk[i]), false);
#endif
		}

		if(strcmp(dev_data->clk[i].name, "stfe_frc-0") == 0)
			__enable_clk(&(dev_data->clk[i]), false);
	}
}

/**
 * @brief Enables all TSHAL clocks
 */
static void stptiTSHAL_EnableClocks(void)
{
	int i;
	struct stpti_stfe_config *dev_data;

	if (!STFE_Config.pdev)
		return;
	dev_data = STFE_Config.pdev->dev.platform_data;

	for (i = 0; i < dev_data->nb_clk; i++)
		__enable_clk(&(dev_data->clk[i]), true);

}

/**
 * @brief Disables all TSHAL clocks
 */
static void stptiTSHAL_DisableClocks(void)
{
	int i;
	struct stpti_stfe_config *dev_data;

	if (!STFE_Config.pdev)
		return;

	dev_data = STFE_Config.pdev->dev.platform_data;
	for (i = 0; i < dev_data->nb_clk; i++) {
		if (dev_data->clk[i].clk) {
			dev_info(&STFE_Config.pdev->dev,
				"Disabling clock %s\n", dev_data->clk[i].name);
			if (dev_data->clk[i].enable_count > 0) {
				clk_disable_unprepare(dev_data->clk[i].clk);
				dev_data->clk[i].enable_count--;
			}
		}
	}
}
/**
 * * @brief Disables TAGcounter 1/STFE clock used for indexing
 * */
static void stptiTSHAL_DisableStfeCoreClock(void)
{
	struct clk *clk = NULL;
	int i = 0;
	struct stpti_stfe_config *dev_data;
	if (!STFE_Config.pdev)
		return;

	dev_data = STFE_Config.pdev->dev.platform_data;
	for(i = 0;i < dev_data->nb_clk;i++){
#ifndef CONFIG_ARCH_STI
		if(strcmp(dev_data->clk[i].name, "stfe_frc-0") == 0) {
#else
		if((strcmp(dev_data->clk[i].name, "stfe_ccsc") == 0) ||
			(strcmp(dev_data->clk[i].name, "stfe_frc-0") == 0)) {
#endif
			if (dev_data->clk[i].clk)
				clk = dev_data->clk[i].clk;
			if (IS_ERR(clk)) {
				pr_warning("Clock %s not found",
					dev_data->clk[i].name);
			} else {
				if (dev_data->clk[i].enable_count > 0) {
					clk_disable_unprepare(
						dev_data->clk[i].clk);
					dev_data->clk[i].enable_count--;
				}
			}
		}
	}
}

/**
 * @brief Puts STFE device into sleep state
 *
 * Disables all inputs and disables STFE clocks
 *
 * @return ST_NO_ERROR on success or a standard ST error number
 **/
static ST_ErrorCode_t stptiTSHAL_TSInputSleep(void)
{
	U32 Index;
	ST_ErrorCode_t Error = ST_NO_ERROR;

	for (Index = 0;
	     Index < (STFE_Config.NumIBs + STFE_Config.NumSWTSs + STFE_Config.NumMIBs) && ST_NO_ERROR == Error;
	     Index++) {
		if (stptiTSHAL_TSInputs[Index].IBBlockIndex == STFECHANNEL_UNUSED) {
			continue;
		}
		Error = stptiTSHAL_StfeDisableInput(&stptiTSHAL_TSInputs[Index]);
	}

	if (ST_NO_ERROR == Error) {
		stptiTSHAL_StfeIrqDisabled();
		STFE_Config.PowerState = stptiTSHAL_TSINPUT_SLEEPING;
	}

	/* disable clock for configured input blocks*/
	for (Index = 0;
		Index < (STFE_Config.NumIBs + STFE_Config.NumSWTSs + \
			STFE_Config.NumMIBs) && ST_NO_ERROR == Error;
		Index++) {
		if (stptiTSHAL_TSInputs[Index].IBBlockIndex == \
				STFECHANNEL_UNUSED) {
			continue;
		}
		stptiTSHAL_HW_function_stfe->stptiTSHAL_StfeEnableInputClock(
				Index, stptiTSHAL_TSInputs[Index].StfeType,
				FALSE);
	}

	stptiTSHAL_DisableStfeCoreClock();
	stptiTSHAL_DisableClocks();
	return Error;
}

/**
 * @brief Resumes STFE device from suspend state
 *
 * Enables STFE clocks then re-enables all inputs, which were previous disabled
 * by stptiTSHAL_StfeSuspend
 *
 * @return ST_NO_ERROR on success or a standard ST error number
 **/
static ST_ErrorCode_t stptiTSHAL_TSInputResume(void)
{
	U32 Index;
	ST_ErrorCode_t Error = ST_NO_ERROR;

	stptiTSHAL_EnableClocks();

	stptiTSHAL_EnableStfeCoreClocks();

	/* Enable clock only for needed input blocks*/
	for (Index = 0;
		Index < (STFE_Config.NumIBs + STFE_Config.NumSWTSs + \
			STFE_Config.NumMIBs) && ST_NO_ERROR == Error;
		Index++) {
		if (stptiTSHAL_TSInputs[Index].IBBlockIndex == \
				STFECHANNEL_UNUSED) {
			continue;
		}
		stptiTSHAL_HW_function_stfe->stptiTSHAL_StfeEnableInputClock(
				Index, stptiTSHAL_TSInputs[Index].StfeType,
				TRUE);
	}

	stptiTSHAL_HW_function_stfe->stptiTSHAL_StfeLoadMemDmaFw();

	stptiTSHAL_StfeStartHW();

	for (Index = 0;
	     Index < (STFE_Config.NumIBs + STFE_Config.NumSWTSs + STFE_Config.NumMIBs) && ST_NO_ERROR == Error;
	     Index++) {
		if (stptiTSHAL_TSInputs[Index].IBBlockIndex == STFECHANNEL_UNUSED) {
			continue;
		}
		Error = stptiTSHAL_StfeConfigureInput(&stptiTSHAL_TSInputs[Index]);
	}

	if (ST_NO_ERROR == Error) {
		STFE_Config.PowerState = stptiTSHAL_TSINPUT_RUNNING;
	}

	return Error;
}

/*****************************************
 *   EXPORTED FUNCTIONS
 *****************************************/
/**
   @brief Maps and initialises the STFE device

   @param params                   - Device-specific configuration data

   @return                         A standard st error type...
                                   - ST_ERROR_NO_MEMORY if no channels free, or no memory.
                                   - ST_NO_ERROR if no errors
 */
ST_ErrorCode_t stptiTSHAL_Init(void *params)
{

	U32 i = 0;
	struct platform_device *pdev = (struct platform_device *)params;
	struct stpti_stfe_config *dev_data = pdev->dev.platform_data;
	struct resource *res = NULL;

	STPTI_PRINTF("Initialising STFE device %s\n", pdev->name);

	if (stptiTSHAL_TSInputMutex_p == NULL) {
		stptiTSHAL_TSInputMutex_p = kmalloc(sizeof(struct mutex), GFP_KERNEL);
		mutex_init(stptiTSHAL_TSInputMutex_p);

		memset(&STFE_Config, 0, sizeof(STFE_Config));

		STFE_Config.pdev = pdev;
		STFE_Config.DefaultDest = dev_data->default_dest;

		/* Set device DMA coherent mask (32-bit address space) */
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 34))
		pdev->dev.coherent_dma_mask = DMA_BIT_MASK(32);
#else
		if (dma_set_coherent_mask(&pdev->dev, DMA_BIT_MASK(32)))
			dev_err(&pdev->dev, "dma_set_coherent_mask failed\n");
#endif

		/* Read the number of registered TPs */
		stptiAPI_DriverGetNumberOfpDevices(&STFE_Config.NumTPUsed);

		/* Copy STFE configuration parameters from platform device data
		 * into global STFE_Config struct
		 *
		 * Note: This is a intermediate step to decouple the STFE code
		 * from the old stptiCONFIG code as part of the platform device
		 * migration. During future refactoring work we should consider
		 * porting the hardware initialisation functions to read the
		 * configuration directly from the platform device
		 */

		res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "stfe");
		if (!res)
			return ST_ERROR_BAD_PARAMETER;

		STFE_Config.PhysicalAddress = (void *)res->start;
		STFE_Config.MapSize = resource_size(res);
		dev_info(&pdev->dev, "Resource %s [0x%x:0x%x]\n", res->name, res->start, res->end);

		res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "stfe-ram");
		if (res) {
			STFE_Config.RAMPhysicalAddress = (void *)res->start;
			STFE_Config.RAMMapSize = resource_size(res);
			dev_info(&pdev->dev, "Resource %s [0x%x:0x%x]\n", res->name, res->start, res->end);
		}

		/* Only stfe v2 devices supply use interrupt number */
		res = platform_get_resource_byname(pdev, IORESOURCE_IRQ, "stfe-idle-irq");
		if (res && (int)res->start >= 0) {
			STFE_Config.Idle_InterruptNumber = (int)res->start;
			dev_info(&pdev->dev, "Idle IRQ %d\n", res->start);
		}

		res = platform_get_resource_byname(pdev, IORESOURCE_IRQ, "stfe-error-irq");
		if (res && (int)res->start >= 0) {
			STFE_Config.Error_InterruptNumber = (int)res->start;
			dev_info(&pdev->dev, "error IRQ %d\n", res->start);
		}

		STFE_Config.stfe_version = dev_data->stfe_version;
		STFE_Config.NumIBs = dev_data->nb_ib;
		STFE_Config.NumMIBs = dev_data->nb_mib;
		STFE_Config.NumSWTSs = dev_data->nb_swts;
		STFE_Config.NumTSDMAs = dev_data->nb_tsdma;
		STFE_Config.NumCCSCs = dev_data->nb_ccsc;
		STFE_Config.NumTagCs = dev_data->nb_tag;

		STFE_Config.IBOffset = dev_data->ib_offset;
		STFE_Config.TagOffset = dev_data->tag_offset;
		STFE_Config.PidFilterOffset = dev_data->pid_filter_offset;
		STFE_Config.SystemRegsOffset = dev_data->system_regs_offset;
		STFE_Config.MemDMAOffset = dev_data->memdma_offset;
		STFE_Config.TSDMAOffset = dev_data->tsdma_offset;
		STFE_Config.CCSCOffset = dev_data->ccsc_offset;
		STFE_Config.tsin_enabled = 0;

		/* If software leaky pid is available on any TP device, set the
		 * global STFE_Config flag */
		for (i = 0; i < STFE_Config.NumTPUsed; i++) {
			stptiHAL_call_unlocked(pDevice.HAL_pDeviceSWLeakyPIDAvailable,
				      stptiOBJMAN_pDeviceObjectHandle(i), &STFE_Config.SoftwareLeakyPID);
			if (STFE_Config.SoftwareLeakyPID) {
				STPTI_PRINTF("STFE:TP has SW Leaky PID available\n");
				break;
			}
		}
		stptiTSHAL_EnableStfeCoreClocks();
		if (STFE_Config.stfe_version == STFE_V1)
			stptiTSHAL_HW_function_stfe = &stptiTSHAL_HW_function_stfev1;
		else
			stptiTSHAL_HW_function_stfe = &stptiTSHAL_HW_function_stfev2;

		{
			U32 v = 0;
			stptiTSHAL_StfeMapHW(&STFE_Config.NumTP);

			/* round it up: it works on 32 bits */
			/* http://graphics.stanford.edu/~seander/bithacks.html#RoundUpPowerOf2 */

			v = STFE_Config.NumTP;
			v--;
			v |= v >> 1;
			v |= v >> 2;
			v |= v >> 4;
			v |= v >> 8;
			v |= v >> 16;
			v++;

			PIDRespW  = v;
		}

		if (!ShadowPIDTableStart_p) {
			U32 PIDTableSize = ((STFE_PID_RAM_SIZE) * 8 * PIDRespW) / 8;
			ShadowPIDTableStart_p = stptiSupport_MemoryAllocateForDMA(
					PIDTableSize,
					PIDTableSize,
					&ShadowPIDTableCachedMemoryStructure,
					stptiSupport_ZONE_LARGE_NON_SECURE);
			if (NULL == ShadowPIDTableStart_p) {
				stpti_printf("Error Allocating 0x%x bytes for Shadow pid table\n",
						PIDTableSize);
				return ST_ERROR_NO_MEMORY;
			} else {
			memset((void *)ShadowPIDTableStart_p, 0xFF, PIDTableSize);
			stptiSupport_FlushRegion(&ShadowPIDTableCachedMemoryStructure,
					(void *)ShadowPIDTableStart_p,
					PIDTableSize);
			}
		}

		/* Initialise the hardware */
		stptiTSHAL_StfeStartHW();

		for (i = 0; i < MAX_MIB; i++)
			MibChannel_StreamId[i] = STFE_STREAMID_NONE;

		/* be sure to non use meaningless value */
		for (i = 0; i < MAX_NUMBER_OF_TSINPUTS; i++)
			stptiTSHAL_TSInputResetChannel(&stptiTSHAL_TSInputs[i]);

	} else {
		return ST_ERROR_ALREADY_INITIALIZED;
	}
	return ST_NO_ERROR;

}

/**
   @brief Shuts-down and unmaps the STFE device

   @param                          none
   @return                         A standard st error type...
                                   - ST_ERROR_NO_MEMORY if no channels free, or no memory.
                                   - ST_NO_ERROR if no errors
 */
ST_ErrorCode_t stptiTSHAL_Term(void)
{

	ST_ErrorCode_t Error = ST_NO_ERROR;

	if (stptiTSHAL_TSInputMutex_p != NULL) {

		stpti_printf("Unmapping the hardware");

		/* This is a workaround to ensure clocks are disabled when
		*stopping STFE.
		* Full fix is to track clock refcounting. See Bug 39707
		*/
		stptiTSHAL_EnableClocks();

		/* Make sure all Stfe blocks are disabled before destroying MMU mappings. */
		Error = stptiTSHAL_StfeShutDown();

		stptiTSHAL_MibSetClear(FALSE);
		/* Ensure clocks are disabled and update power state */
		stptiTSHAL_TSInputSetPowerState(stptiTSHAL_TSINPUT_STOPPED);

		stptiTSHAL_StfeUnmapHW();
		if (ShadowPIDTableStart_p) {
			stptiSupport_MemoryDeallocateForDMA(&ShadowPIDTableCachedMemoryStructure);
			ShadowPIDTableStart_p = NULL;
		}

		/* This is a workaround to ensure clocks are disabled when
		 * stopping STFE.
		 * Full fix is to track clock refcounting. See Bug 39707
		 */
		stptiTSHAL_DisableClocks();

		kfree(stptiTSHAL_TSInputMutex_p);
		stptiTSHAL_TSInputMutex_p = NULL;

	} else {
		Error = ST_ERROR_ALREADY_INITIALIZED;
	}

	return Error;
}

/**
   @brief  Top level config function for a TSInput

   This function calls all the necessary helper functions to configure the required hardware blocks for an input data route.

   Hence when we received a request to configure an input, first a scenario is identified and depending on it,
   the needed HW resource are allocated/identified.

   @param  TSInputConfigParams_p   Pointer to memory containing the settings
   @param  Force                   If you are deallocating set this to ignore HW errors
   @return                         A standard st error type...
                                   - ST_ERROR_NO_MEMORY if no channels free, or no memory.
                                   - ST_ERROR_BAD_PARAMETER if no scenario identified or no Input channel correspond to input StreamID.
                                   - ST_NO_ERROR if no errors
 */

ST_ErrorCode_t stptiTSHAL_TSInputStfeConfigure(stptiTSHAL_TSInputConfigParams_t *TSInputConfigParams_p)
{
	ST_ErrorCode_t Error = ST_NO_ERROR;
	stptiTSHAL_InputDevices_t *TSInput_p = NULL;
	U32 IBBlockIndex = 0;
	stptiHAL_TSInputScenario_t TSInputScenario = stptiHAL_TSINPUT_SCENARIO_UNDEFINED;

	STPTI_PRINTF("Configuring TSInput StreamID 0x%04x (starting %s)", TSInputConfigParams_p->StreamID,
		     (TSInputConfigParams_p->InputBlockEnable ? "Enabled" : "Disabled"));

	/* Convert "default" routing to the actual platform-specific routing */
	if (TSInputConfigParams_p->Routing == stptiTSHAL_TSINPUT_DEST_DEFAULT)
		TSInputConfigParams_p->Routing = STFE_Config.DefaultDest;

	TSInputScenario = stptiTSHAL_Stfe_ValidateInput(TSInputConfigParams_p);
	if (TSInputScenario == stptiHAL_TSINPUT_SCENARIO_UNDEFINED)
		return ST_ERROR_BAD_PARAMETER;

	if (mutex_lock_interruptible(stptiTSHAL_TSInputMutex_p))
		return ST_ERROR_TIMEOUT;

	/* We need to validate the of index of the streamid depending on the numbers of the different blocks available as we now have a dynamically configurable system */
	IBBlockIndex = ConvertAPItoTSHALIndex(TSInputConfigParams_p->StreamID, TRUE);
	TSInput_p = &(stptiTSHAL_TSInputs[IBBlockIndex]);

	if (IBBlockIndex == STFE_STREAMID_NONE) {
		mutex_unlock(stptiTSHAL_TSInputMutex_p);
		return ST_ERROR_BAD_PARAMETER;
	}

	if ((TSInputScenario == stptiHAL_TSINPUT_SCENARIO_Deallocate) &&
	    (TSInput_p->Index_byType == STFE_STREAMID_NONE)) {
		mutex_unlock(stptiTSHAL_TSInputMutex_p);
		return ST_ERROR_BAD_PARAMETER;
	}

	TSInput_p->TSInputScenario = TSInputScenario;
	if (TSInput_p->TSInputScenario != stptiHAL_TSINPUT_SCENARIO_Deallocate) {
		TSInput_p->Index_byType = stptiHAL_StreamIndexByType(TSInputConfigParams_p->StreamID);
		TSInput_p->StfeType = ExtractIBTypeFromStreamID(TSInputConfigParams_p->StreamID);
		TSInput_p->IBBlockIndex = IBBlockIndex;
		TSInput_p->OutputBlockIndex = (U32) ConvertDestToTSDMAOutput(TSInputConfigParams_p->Routing);

		if (stptiTSHAL_Stfe_NeedMibChannel(TSInput_p->TSInputScenario))
			Error = stptiTSHAL_TSInputObtainMIBChannel(TSInput_p);
		else
			TSInput_p->MIBChannel = STFECHANNEL_UNUSED;

		/* Calculate MEMDMAChannel - must be called after MIB channel obtained */
		TSInput_p->MemDmaChannel = CalculateMEMDMAChannel(TSInput_p);
		TSInput_p->PidFilterIndex = CalculatePidFilterIndexChannel(TSInput_p);

		if (stptiTSHAL_Stfe_UseTag(TSInput_p->TSInputScenario))
			TSInput_p->Tag = (TSInputConfigParams_p->TSTag & stptiTSHAL_TAG_MASK);

		if (TSInput_p->StfeType == stptiTSHAL_TSINPUT_STFE_MIB)
			TSInput_p->Index_byType = TSInput_p->MIBChannel;
	}

	if (ST_NO_ERROR == Error) {

		/* This function only allocates when necessary so call it every time */
		/* RESET CONDITION */
		if (TSInput_p->TSInputScenario == stptiHAL_TSINPUT_SCENARIO_Deallocate) {
			stpti_printf("Freeing resources as the Packet Memory Size is zero.");

			/* Memory error has occured so clear all saved parameters to prevent further error on re-configuration attempts */
			stpti_printf("Clearing all parameters for TSInput");
			Error = stptiTSHAL_StfeDeallocate(TSInput_p);
			if (TSInput_p->StfeType !=
					stptiTSHAL_TSINPUT_STFE_MIB)
				stptiTSHAL_HW_function_stfe->stptiTSHAL_StfeEnableInputClock(TSInput_p->IBBlockIndex,
						TSInput_p->StfeType, FALSE);

			stptiTSHAL_TSInputResetChannel(TSInput_p);
			if (TSInput_p->StfeType == stptiTSHAL_TSINPUT_STFE_MIB)
				stptiTSHAL_HW_function_stfe->stptiTSHAL_StfeEnableInputClock(TSInput_p->IBBlockIndex,
						TSInput_p->StfeType, FALSE);

			stptiTSHAL_StfeDisableMIB();
			stptiTSHAL_DisableClocks();
		} else {
			/* Save the new configuration params in global memory */
			memcpy(&TSInput_p->Stfe.Config, TSInputConfigParams_p,
			       sizeof(stptiTSHAL_TSInputConfigParams_t));
			Error = stptiTSHAL_StfeMemAllocate(TSInput_p);
			stptiTSHAL_EnableClocks();

			stptiTSHAL_HW_function_stfe->stptiTSHAL_StfeEnableInputClock(TSInput_p->IBBlockIndex,
					TSInput_p->StfeType, TRUE);

			if (ST_NO_ERROR == Error) {

				if ((TSInput_p->MIBChannel != STFECHANNEL_UNUSED)
				    && (!stptiTSHAL_StfeGetIBEnableStatus(0, stptiTSHAL_TSINPUT_STFE_MIB))) {
					Error =
					    stptiTSHAL_StfeConfigureHWRegister(TSInput_p, TSInput_p->MIBChannel,
									       stptiTSHAL_TSINPUT_STFE_MIB);
					if (ST_NO_ERROR == Error)
						stptiTSHAL_MibSetClear(TRUE);
				}

				mutex_unlock(stptiTSHAL_TSInputMutex_p);
				Error =
				    stptiTSHAL_TSInputStfeEnable(TSInputConfigParams_p->StreamID,
								 TSInputConfigParams_p->InputBlockEnable);
				if (ST_NO_ERROR != Error)
					return Error;

				if (mutex_lock_interruptible(stptiTSHAL_TSInputMutex_p))
					return ST_ERROR_TIMEOUT;

				if (TSInput_p->Index_byType == STFECHANNEL_UNUSED) {
					mutex_unlock(stptiTSHAL_TSInputMutex_p);
					return STPTI_ERROR_TSINPUT_NOT_CONFIGURED;
				}
			}
		}
	}
	mutex_unlock(stptiTSHAL_TSInputMutex_p);

	return Error;
}

/**
   @brief  Enable or Disable for TSInput

   This function disabled or enabled input on a STFE channel.
   The HW is reconfigured everytime we do an enable to restore the functionality after a disable

   @param  StreamID                StreamId for the channel of interest
   @param  Enable	           Enable or Disable

   @return                         A standard st error type...
                                   - ST_ERROR_NO_MEMORY if no channels free, or no memory.
                                   - ST_NO_ERROR if no errors
 */
ST_ErrorCode_t stptiTSHAL_TSInputStfeEnable(U32 StreamID, BOOL Enable)
{
	ST_ErrorCode_t Error = ST_NO_ERROR;
	stptiTSHAL_InputDevices_t *TSInput_p = NULL;
	U32 IBBlockIndex = 0;

	STPTI_PRINTF("%s TSInput StreamID 0x%04x", (Enable ? "Enabling" : "Disabling"), StreamID);

	if (mutex_lock_interruptible(stptiTSHAL_TSInputMutex_p))
		return ST_ERROR_TIMEOUT;
	/* We need to validate the of index of the streamid depending on the numbers of the different blocks available as we now have a dynamically configurable system */
	IBBlockIndex = ConvertAPItoTSHALIndex(StreamID, FALSE);

	if (IBBlockIndex == STFE_STREAMID_NONE)
		Error = ST_ERROR_BAD_PARAMETER;
	else {
		TSInput_p = &(stptiTSHAL_TSInputs[IBBlockIndex]);

		if (TSInput_p->IBBlockIndex != STFECHANNEL_UNUSED) {
			if (Enable == FALSE)
				Error = stptiTSHAL_StfeDisableInput(TSInput_p);
			else
				Error = stptiTSHAL_StfeConfigureInput(TSInput_p);
		} else {
			Error = STPTI_ERROR_TSINPUT_NOT_CONFIGURED;
		}
	}

	mutex_unlock(stptiTSHAL_TSInputMutex_p);

	return Error;
}

/**
   @brief  Obtain status for TSInput

   This function retrieves the status information for a STFE channel curently just works for SWTS and IB.

   @param  TSInputHWMapping        Pointer memory used to get the mapped addresses
   @param  StreamID                StreamId for the channel of interest

   @return                         A standard st error type...
                                   - ST_ERROR_NO_MEMORY if no channels free, or no memory.
                                   - ST_NO_ERROR if no errors
 */
ST_ErrorCode_t stptiTSHAL_TSInputStfeGetStatus(stptiTSHAL_TSInputStatus_t *TSInputStatus_p)
{
	ST_ErrorCode_t Error = ST_NO_ERROR;
	U32 Status;
	stptiTSHAL_InputDevices_t *TSInput_p = NULL;
	U32 IBBlockIndex = 0;

	/* We need to validate the of index of the streamid depending on the numbers of the different blocks available as we now have a dynamically configurable system */
	IBBlockIndex = ConvertAPItoTSHALIndex(TSInputStatus_p->StreamID, FALSE);

	if (IBBlockIndex == STFE_STREAMID_NONE) {
		Error = ST_ERROR_BAD_PARAMETER;
	} else {
		TSInput_p = &(stptiTSHAL_TSInputs[IBBlockIndex]);

		/* Is the block configured? */
		if (TSInput_p->Stfe.AllocatedNumOfPkts > 0) {
			U32 Temp = TSInputStatus_p->StreamID;
			/* Clear the structure */
			memset((void *)TSInputStatus_p, 0x00, sizeof(stptiTSHAL_TSInputStatus_t));
			TSInputStatus_p->StreamID = Temp;

			Status = stptiTSHAL_StfeReadStatusReg(TSInput_p->Index_byType, TSInput_p->StfeType);

			/* Block        Bits   31       30     15:12        11:8          4       3(not checK)       2              1             0
			   IB                  -     LOCKED SHORT_PKTS  ERROR_PKTS  PKT_OVERFLOW PID_OVERFLOW OUT_OF_ORDER_RP BUF_OVERFLOW FIFO_OVERFLOW
			   MIB                 -       -    SHORT_PKTS  ERROR_PKTS  PKT_OVERFLOW PID_OVERFLOW OUT_OF_ORDER_RP BUF_OVERFLOW FIFO_OVERFLOW
			   SWTS             FIFOREQ  LOCKED             ERROR_PKTS       -           -        OUT_OF_ORDER_RP     -        FIFO_OVERFLOW */

			if (STFE_STATUS_FIFO_OVERFLOW & Status)
				TSInputStatus_p->FifoOverflow = TRUE;

			if (STFE_STATUS_OUT_OF_ORDER_RP & Status)
				TSInputStatus_p->OutOfOrderRP = TRUE;

			TSInputStatus_p->ErrorPackets =
			    ((Status & STFE_STATUS_ERROR_PKTS) >> STFE_STATUS_ERROR_PKTS_POSITION);

			if (TSInput_p->StfeType != stptiTSHAL_TSINPUT_STFE_SWTS) {
				if (STFE_STATUS_BUFFER_OVERFLOW & Status)
					TSInputStatus_p->BufferOverflow = TRUE;

				if (STFE_STATUS_PKT_OVERFLOW & Status)
					TSInputStatus_p->PktOverflow = TRUE;

				TSInputStatus_p->ShortPackets =
				    ((Status & STFE_STATUS_ERROR_SHORT_PKTS) >> STFE_STATUS_ERROR_SHORT_PKTS_POSITION);
			}

			if (TSInput_p->StfeType != stptiTSHAL_TSINPUT_STFE_MIB) {
				if (STFE_STATUS_LOCK & Status)
					TSInputStatus_p->Lock = TRUE;
				else
					TSInputStatus_p->Lock = FALSE;
			}
		} else {
			Error = STPTI_ERROR_TSINPUT_NOT_CONFIGURED;
		}
	}
	return Error;
}

/**
   @brief  Reset the STFE TSInput

   This function resets the register addresses for the STFE device.

   If this function is called you must accept that there will be a discontinuity in input data when the input is currently enabled.

   @param  StreamID                StreamId for the channel of interest
   @return                         A standard st error type...
                                   - ST_ERROR_NO_MEMORY if no channels free, or no memory.
                                   - ST_NO_ERROR if no errors
 */
ST_ErrorCode_t stptiTSHAL_TSInputStfeReset(U32 StreamID)
{
	ST_ErrorCode_t Error = ST_NO_ERROR;

	Error = stptiTSHAL_TSInputStfeEnable(StreamID, FALSE);
	if (ST_NO_ERROR == Error) {
		Error = stptiTSHAL_TSInputStfeEnable(StreamID, TRUE);
	}
	return Error;
}

/**
   @brief  Clear or Set Pid Filter for a Pid value

   This function either sets or clears one pid value for the STFE Pid Filter.

   @param  TP_requester            Index Of the TP core that requests the action
   @param  StreamID                StreamID for the channel of interest
   @param  Pid                     Unique Pid value selected
   @param  Set                     if True set the Pid else clear value

   @return                         A standard st error type...
                                   - ST_ERROR_NO_MEMORY if no channels free, or no memory.
                                   - ST_NO_ERROR if no errors
 */
ST_ErrorCode_t stptiTSHAL_TSInputStfeSetClearPid(U32 TP_requester, U32 StreamID, U16 Pid, BOOL Set)
{
	ST_ErrorCode_t Error = ST_NO_ERROR;
	stptiTSHAL_InputDevices_t *TSInput_p = NULL;
	U8 PidCount;
	U32 ByteOffset;
	U32 IBBlockIndex = STFE_STREAMID_NONE;

	if (mutex_lock_interruptible(stptiTSHAL_TSInputMutex_p))
		return ST_ERROR_TIMEOUT;
	/* We need to validate the of index of the streamid depending on the numbers of the different blocks available as we now have a dynamically configurable system */
	if ((STFE_Config.PowerState == stptiTSHAL_TSINPUT_SLEEPING) ||
		(STFE_Config.PowerState == stptiTSHAL_TSINPUT_POWERDOWN))
		stptiTSHAL_EnableStfeCoreClocks();

	IBBlockIndex = ConvertTagtoTSHALIndex(StreamID);

	if (IBBlockIndex == STFE_STREAMID_NONE) {
		/* if not match any StreamId, look for scenario 10 */
		U32 Index;
		for (Index = 0; Index < STFE_Config.NumIBs; Index++) {

			TSInput_p = &(stptiTSHAL_TSInputs[Index]);
			if (TSInput_p->TSInputScenario == stptiHAL_TSINPUT_SCENARIO_10)
				stptiTSHAL_StfeSetClearTPTransmission(TSInput_p, TP_requester, Set);
		}
	} else {
		TSInput_p = &(stptiTSHAL_TSInputs[IBBlockIndex]);

		 if (TSInput_p->PidFilterIndex != STFECHANNEL_UNUSED) {
			/* We need to check the tracker RAM here */
			ByteOffset = Pid + STFE_PID_TRACKER_RAM_SIZE * TP_requester;

			if (Pid <= stptiTSHAL_DVB_WILDCARD_PID) {
				/* Read the current count */
				PidCount = *(TSInput_p->Stfe.PIDTableTracker_p + ByteOffset);

				if (Set) {
					if (PidCount < 255) {
						PidCount++;
						*(TSInput_p->Stfe.PIDTableTracker_p + ByteOffset) = PidCount;
					}
					/* Only need to set it once */
					if (PidCount == 1) {
						Error = stptiTSHAL_StfeSetPid(TSInput_p, Pid, TP_requester);
					}
				} else {
					if (PidCount) {
						PidCount--;
						*(TSInput_p->Stfe.PIDTableTracker_p + ByteOffset) = PidCount;
					}
					/* We are clearing so only do it once when required */
					if (PidCount == 0) {
						Error = stptiTSHAL_StfeClearPid(TSInput_p, Pid, TP_requester);
					}
				}
			} else {
				Error = ST_ERROR_BAD_PARAMETER;
			}
		} else {
			Error = ST_ERROR_BAD_PARAMETER;
		}
	}
	if ((STFE_Config.PowerState == stptiTSHAL_TSINPUT_SLEEPING) ||\
		(STFE_Config.PowerState == stptiTSHAL_TSINPUT_POWERDOWN))
		stptiTSHAL_DisableStfeCoreClock();

	mutex_unlock(stptiTSHAL_TSInputMutex_p);
	return (Error);
}

/**
   @brief  Returns the Timer value for this Input

   This function looks up which timer is associated to this input.

   @param  StreamID            StreamID to select input block
   @param  TimeValue_p         Pointer to time information
   @param  IsTSIN_p            If IB block = TRUE (it means that the inout is timestamped)
   @return                     none
 */
ST_ErrorCode_t stptiTSHAL_TSInputStfeGetTimer(U32 StreamID, stptiTSHAL_TimerValue_t *TimeValue_p, BOOL *IsTSIN_p)
{
	ST_ErrorCode_t Error = ST_NO_ERROR;
	U32 IBBlockIndex = STFE_STREAMID_NONE;

	/* We need to validate the of index of the streamid depending on the numbers of the different blocks available as we now have a dynamically configurable system */
	if (mutex_lock_interruptible(stptiTSHAL_TSInputMutex_p))
		return ST_ERROR_TIMEOUT;

	IBBlockIndex = ConvertAPItoTSHALIndex(StreamID, FALSE);

	if (IBBlockIndex == STFE_STREAMID_NONE) {
		Error = stptiTSHAL_StfeGetHWTimer(IBBlockIndex, stptiTSHAL_TSINPUT_STFE_NONE, TimeValue_p, IsTSIN_p);
	} else {
		Error =
		    stptiTSHAL_StfeGetHWTimer(IBBlockIndex, stptiTSHAL_TSInputs[IBBlockIndex].StfeType, TimeValue_p,
					      IsTSIN_p);
	}
	mutex_unlock(stptiTSHAL_TSInputMutex_p);

	return (Error);
}

/**
   @brief  Function to Disable/Enable the STFE Pid Filter

   This function overrides the STFE Pid filter.  It is called in interrupt context.

   @param  StreamBitmask      Bitmask to indicate which TSInput is being used.
   @param  EnablePIDFilter    FALSE - Disable Pid Filter, TRUE - Enable PID filter

   @return                    none
 */
ST_ErrorCode_t stptiTSHAL_TSInputOverridePIDFilter(U32 StreamBitmask, BOOL EnablePIDFilter)
{
	U32 i = 0;
	stptiTSHAL_InputDevices_t *TSInput_p;
	U32 Index;

	if (mutex_lock_interruptible(stptiTSHAL_TSInputMutex_p))
		return ST_ERROR_TIMEOUT;

	while (StreamBitmask) {
		if (StreamBitmask & 1) {
			for (Index = 0; Index < MAX_NUMBER_OF_TSINPUTS; Index++) {
				TSInput_p = &(stptiTSHAL_TSInputs[Index]);
				if (TSInput_p->MemDmaChannel == i) {
					BOOL PIDFilterEnable = TRUE;

					if (EnablePIDFilter == FALSE)
						PIDFilterEnable = FALSE;
					else {
						U32 j = 0;
						for (j = 0; j < STFE_Config.NumTPUsed; j++)
							if (TSInput_p->Stfe.
							    PIDTableTracker_p[stptiTSHAL_DVB_WILDCARD_PID
									+ STFE_PID_TRACKER_RAM_SIZE*i] != 0)
								PIDFilterEnable = FALSE;
					}

					if (TSInput_p->MIBChannel != STFECHANNEL_UNUSED)
						stptiTSHAL_StfePIDFilterEnable(TSInput_p->MIBChannel,
									       stptiTSHAL_TSINPUT_STFE_MIB,
									       PIDFilterEnable);
					else
						stptiTSHAL_StfePIDFilterEnable(TSInput_p->Index_byType,
									       TSInput_p->StfeType, PIDFilterEnable);

					TSInput_p->Stfe.PIDFilterEnabled = PIDFilterEnable;
					break;
				}
			}
		}
		StreamBitmask >>= 1;
		i++;

		if (i >= (STFE_Config.NumIBs + STFE_Config.NumMIBs))
			break;
	}
	mutex_unlock(stptiTSHAL_TSInputMutex_p);

	return ST_NO_ERROR;
}

/**
   @brief  Function to monitor the DREQ levels into memdma

   This function  monitor the DREQ levels into memdma.

   @param  tp_read_pointers     Array containing the level to monitor (one for each channel).
   @param  num_rps    		The number of channel to monitor

   @return                    none
 */
ST_ErrorCode_t stptiTSHAL_TSInputStfeMonitorDREQLevels(U32 *tp_read_pointers, U8 num_rps)
{
	if (mutex_lock_interruptible(stptiTSHAL_TSInputMutex_p))
		return ST_ERROR_TIMEOUT;

	stptiTSHAL_HW_function_stfe->stptiTSHAL_StfeMonitorDREQLevels(tp_read_pointers, num_rps, STFE_DVB_DMA_PKT_SIZE);
	mutex_unlock(stptiTSHAL_TSInputMutex_p);

	return ST_NO_ERROR;
}

/**
   @brief Changes the power state of the TSInput device

   @param NewState - The requested power state

   @return - A standard st error type...
           - ST_NO_ERROR if no errors
 */
ST_ErrorCode_t stptiTSHAL_TSInputSetPowerState(stptiTSHAL_TSInputPowerState_t NewState)
{
	ST_ErrorCode_t Error = ST_NO_ERROR;

	if (mutex_lock_interruptible(stptiTSHAL_TSInputMutex_p))
		return ST_ERROR_TIMEOUT;

	switch (NewState) {
	case stptiTSHAL_TSINPUT_RUNNING:
		switch (STFE_Config.PowerState) {
		case stptiTSHAL_TSINPUT_RUNNING:
			/* Already running - do nothing */
			break;
		case stptiTSHAL_TSINPUT_SLEEPING:
			Error = stptiTSHAL_TSInputResume();
			break;
		default:
			STPTI_PRINTF_ERROR("TSHAL cannot enter running state from state %d", STFE_Config.PowerState);
			Error = ST_ERROR_FEATURE_NOT_SUPPORTED;
			break;
		}
		break;

	case stptiTSHAL_TSINPUT_SLEEPING:
		switch (STFE_Config.PowerState) {
		case stptiTSHAL_TSINPUT_SLEEPING:
			/* Already sleeping - do nothing */
			break;
		case stptiTSHAL_TSINPUT_POWERDOWN:
			/* Need to power-up first before sleeping */
			STFE_Config.PowerState = NewState;
			break;
		case stptiTSHAL_TSINPUT_RUNNING:
			Error = stptiTSHAL_TSInputSleep();
			break;
		default:
			STPTI_PRINTF_ERROR("TSHAL cannot enter sleep state from state %d", STFE_Config.PowerState);
			Error = ST_ERROR_FEATURE_NOT_SUPPORTED;
			break;
		}
		break;

	case stptiTSHAL_TSINPUT_POWERDOWN:
	case stptiTSHAL_TSINPUT_STOPPED:
		switch (STFE_Config.PowerState) {
		case stptiTSHAL_TSINPUT_POWERDOWN:
			break;
		case stptiTSHAL_TSINPUT_STOPPED:
			/* Already powered-down - do nothing */
			break;
		case stptiTSHAL_TSINPUT_SLEEPING:
			/* We do not store any hardware state in powerdown. So
			 * powerdown is the same as stopped.
			 * Just ensure that clocks are stopped */
			STFE_Config.PowerState = NewState;
			break;
		case stptiTSHAL_TSINPUT_RUNNING:
			/*disable the IRQ*/
			stptiTSHAL_StfeIrqDisabled();
			stptiTSHAL_DisableStfeCoreClock();
			break;
		default:
			STPTI_PRINTF_ERROR("TSHAL cannot power-down from state %d", STFE_Config.PowerState);
			Error = ST_ERROR_FEATURE_NOT_SUPPORTED;
			break;
		}
		break;

	default:
		STPTI_PRINTF_ERROR("Bad power state requested: %d", NewState);
		Error = ST_ERROR_BAD_PARAMETER;
	}

	mutex_unlock(stptiTSHAL_TSInputMutex_p);
	return Error;
}

/**
 * @brief Called to notify the TSHAL that a given pDevice has powered-on/off
 *
 * @param pDeviceIndex  - Index of pDevice whose power state has changed
 * @param PowerOn       - Indicates whether the given pDevice has powered-on
 *                        (TRUE) or powered-off (FALSE)
 *
 * @return ST_NO_ERROR on success or a standard ST error code
 **/
ST_ErrorCode_t stptiTSHAL_TSInputNotifyPDevicePowerState(U32 pDeviceIndex, BOOL PowerOn)
{
	U32 i;
	ST_ErrorCode_t Error = ST_NO_ERROR;
	stptiHAL_pDeviceConfigLiveParams_t LiveParams;
	U32 PktSize = STFE_DVB_DMA_PKT_SIZE;	/* Default size for DVB */

	if (mutex_lock_interruptible(stptiTSHAL_TSInputMutex_p))
		return ST_ERROR_TIMEOUT;
	for (i = 0; i < (STFE_Config.NumIBs + STFE_Config.NumSWTSs + STFE_Config.NumMIBs) && ST_NO_ERROR == Error; i++) {
		stptiTSHAL_InputDevices_t *TSInput_p = &stptiTSHAL_TSInputs[i];
		U32 TP_RAMSize = TSInput_p->Stfe.RAMSize / STFE_Config.NumTPUsed;

		if (TSInput_p->MemDmaChannel == STFECHANNEL_UNUSED)
			continue;

		/* Calculate IB packet size based on transport type etc */
		switch (TSInput_p->Stfe.Config.PacketLength) {
		case (stptiHAL_TSINPUT_PACKET_LENGTH_DVB):
		case (stptiHAL_TSINPUT_PACKET_LENGTH_TAGGED_DVB):
		default:
			PktSize = STFE_DVB_DMA_PKT_SIZE;
			break;
		}

		LiveParams.Base =
		    (U32) stptiSupport_VirtToPhys(&TSInput_p->Stfe.RAMCachedMemoryStructure,
						  &TSInput_p->Stfe.RAMStart_p[i * TP_RAMSize]);
		LiveParams.SizeInPkts = TSInput_p->Stfe.AllocatedNumOfPkts;
		LiveParams.PktLength = PktSize;
		LiveParams.Channel = TSInput_p->MemDmaChannel;
		/* make sure stfe ddr pointers are reset */
		stptiTSHAL_StfeWriteToInternalRAMDDRPtrs(TSInput_p,
				TSInput_p->Stfe.AllocatedNumOfPkts * PktSize,
				PktSize);
		Error =
		    (stptiHAL_call
		     (pDevice.HAL_pDeviceConfigureLive, stptiOBJMAN_pDeviceObjectHandle(pDeviceIndex), &LiveParams));
		if (Error == ST_ERROR_SUSPENDED)
			Error = ST_NO_ERROR;
		if (Error != ST_NO_ERROR)
			break;
	}
	mutex_unlock(stptiTSHAL_TSInputMutex_p);
	return ST_NO_ERROR;
}

ST_ErrorCode_t stptiTSHAL_TSInputGetPlatformDevice(struct platform_device **pdev)
{
	ST_ErrorCode_t Error = ST_NO_ERROR;

	if (mutex_lock_interruptible(stptiTSHAL_TSInputMutex_p))
		return ST_ERROR_TIMEOUT;
	*pdev = STFE_Config.pdev;

	mutex_unlock(stptiTSHAL_TSInputMutex_p);
	return Error;
}


/**
   @brief  STFE Hardware worker function

   Debug function to print registers

   @param  StreamID       StreamID to select input block
   @param  Verbose        Increase the amount of outputted data
   @param  Buffer         Buffer to write to
   @param  Limit          Maximum number of characters to output (including terminator)
   @param  Offset         Number of characters to skip before starting to put data into the buffer

   @return                none
 */
ST_ErrorCode_t stptiTSHAL_TSInputStfeIBRegisterDump(U32 StreamID, BOOL Verbose, char *Buffer, int *StringSize_p,
						    int Limit, int Offset, int *EOF_p)
{
	stptiSUPPORT_sprintf_t ctx;
	U32 IBBlockIndex = 0;
	ST_ErrorCode_t Error = ST_NO_ERROR;
	stptiTSHAL_InputDevices_t *TSInput_p = NULL;

	/* We need to validate the of index of the streamid depending on the numbers of the different blocks available as we now have a dynamically configurable system */
	if (stptiHAL_StreamIDTestMIB(StreamID)) {
		/* For dump of MIBs the streamID represents the MIB channel number so don't use the normal
		 * IBBlockIndex lookup method */
		if ((StreamID & ~STFE_STREAMID_MIB) < STFE_Config.NumMIBs) {
			IBBlockIndex = STFE_Config.NumIBs + STFE_Config.NumSWTSs;
			IBBlockIndex += (StreamID & ~STFE_STREAMID_MIB);
		} else
			IBBlockIndex = STFE_STREAMID_NONE;
	} else {
		IBBlockIndex = ConvertAPItoTSHALIndex(StreamID, FALSE);
	}

	if (IBBlockIndex == STFE_STREAMID_NONE) {
		Error = ST_ERROR_BAD_PARAMETER;
	} else {
		/* Initialise stptiSUPPORT_sprintf context structure */
		ctx.p = Buffer;
		ctx.CharsOutput = 0;
		ctx.CharsLeft = Limit - 1;	/* Minus 1 to allow for terminator */
		ctx.CurOffset = 0;
		ctx.StartOffset = Offset;
		ctx.StoppedPrintingEarly = FALSE;

		TSInput_p = &(stptiTSHAL_TSInputs[IBBlockIndex]);

		if (TSInput_p->Index_byType == STFECHANNEL_UNUSED) {
			stptiSUPPORT_sprintf(&ctx, "-------------- Uninitialised Input --------------\n");
			return ST_NO_ERROR;
		}

		stptiTSHAL_HW_function_stfe->stptiTSHAL_StfeSystemRegisterDump(&ctx);

		if (TSInput_p->StfeType == stptiTSHAL_TSINPUT_STFE_IB) {
			stptiTSHAL_StfeIBRegisterDump(TSInput_p->Index_byType, &ctx);
			stptiTSHAL_StfePckRAMDump(TSInput_p->Index_byType, TSInput_p->StfeType, &ctx);
		} else if (TSInput_p->StfeType == stptiTSHAL_TSINPUT_STFE_SWTS) {
			stptiTSHAL_StfeSWTSRegisterDump(TSInput_p->Index_byType, &ctx);
			stptiTSHAL_StfePckRAMDump(TSInput_p->Index_byType, TSInput_p->StfeType, &ctx);
		}

		if (TSInput_p->MIBChannel != STFECHANNEL_UNUSED) {
			stptiTSHAL_StfeMIBRegisterDump(TSInput_p->MIBChannel, &ctx);
			stptiTSHAL_StfePckRAMDump(TSInput_p->MIBChannel, stptiTSHAL_TSINPUT_STFE_MIB, &ctx);
		}

		if (TSInput_p->OutputBlockIndex != STFECHANNEL_UNUSED)
			stptiTSHAL_StfeTSDMARegisterDump(IBBlockIndex, TSInput_p->OutputBlockIndex, &ctx);

		stptiTSHAL_StfeCCSCRegisterDump(&ctx);

		if (TSInput_p->PidFilterIndex != STFECHANNEL_UNUSED)
			stptiTSHAL_StfePidEnabledTableDump(TSInput_p, &ctx);

		if (TSInput_p->MemDmaChannel != STFECHANNEL_UNUSED) {
			stptiTSHAL_HW_function_stfe->stptiTSHAL_StfeDmaPTRRegisterDump(TSInput_p->MemDmaChannel, &ctx);
			stptiTSHAL_HW_function_stfe->stptiTSHAL_StfeMemDmaRegisterDump(TSInput_p->MemDmaChannel,
										       Verbose, &ctx);

			if (Verbose)
				stptiTSHAL_StfeDDRRAMDump(TSInput_p, &ctx);
		}

		*StringSize_p = ctx.CharsOutput;

		if (ctx.StoppedPrintingEarly) {
			*EOF_p = 0;
		} else {
			*EOF_p = 1;
		}
	}

	return (Error);
}
