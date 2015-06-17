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
   @file   pti_stfe.h
   @brief  STFE  Low Level Driver.

   This file declares the LLD for the STFE peripheral

   This file should ONLY contain function prototypes and constants that are
   INTERNAL to the stptiHAL. NOTHING outside of the tango
   directory should be referring to this file.

   Defines and enums relevent to the API of the HAL must be put into
   pti_hal_api.h and will be shared across all HALs.
 */

#ifndef _PTI_STFE_H_
#define _PTI_STFE_H_

//------------------------------------------------------------------------------------
#include "pti_stfe_v1.h"
#include "pti_stfe_v2.h"

#define MAX_NUMRAM						(16)

/* Be careful if you change this value - always check its impact on the memory map */
#define MAX_NUMBER_OF_TSINPUTS        (32)
#define STFE_MAX_TSDMA_BLOCK_SIZE     (64)

#define STFE_LEAK_COUNT_RESET         (0x28be0)

typedef struct {
	volatile U32 INPUT_FORMAT_CONFIG;
	volatile U32 SLAD_CONFIG;
	volatile U32 TAG_BYTES;
	volatile U32 PID_SETUP;
	volatile U32 PACKET_LENGTH;
	volatile U32 BUFFER_START;
	volatile U32 BUFFER_END;
	volatile U32 READ_POINTER;
	volatile U32 WRITE_POINTER;
	volatile U32 PRIORITY_THRESHOLD;
	volatile U32 STATUS;
	volatile U32 MASK;
	volatile U32 SYSTEM;
	volatile U8 padding[12];
} stptiHAL_STFE_InputBlockDevice_t;

typedef struct {
	volatile U32 INPUT_FORMAT_CONFIG;
	volatile U32 Reserved;
	volatile U32 STREAM_IDENT;
	volatile U32 PID_SETUP;
	volatile U32 PACKET_LENGTH;
	volatile U32 BUFFER_START;
	volatile U32 BUFFER_END;
	volatile U32 READ_POINTER;
	volatile U32 WRITE_POINTER;
	volatile U32 PRIORITY_THRESHOLD;
	volatile U32 STATUS;
	volatile U32 MASK;
	volatile U32 SYSTEM;
	volatile U8 padding[12];
} stptiHAL_STFE_MergedInputBlockDevice_t;

typedef struct {
	volatile U32 FIFO_CONFIG;
	volatile U32 SLAD_CONFIG;
	volatile U32 TAG_BYTE_ADD;
	volatile U32 TAG_BYTE_REPLACE;
	volatile U32 PACKET_LENGTH;
	volatile U32 BUFFER_START;
	volatile U32 BUFFER_END;
	volatile U32 READ_POINTER;
	volatile U32 WRITE_POINTER;
	volatile U32 Reserved;
	volatile U32 STATUS;
	volatile U32 MASK;
	volatile U32 SYSTEM;
	volatile U8 padding[12];
} stptiHAL_STFE_SWTSInputBlockDevice_t;

typedef struct {
	volatile U32 COUNTER_LSW;
	volatile U32 COUNTER_MSW;
	volatile U8 padding[56];
} stptiHAL_STFE_TagCounterDevice_t;

typedef struct {
	volatile U32 PIDF_BASE[64];

	union {
		stptiHAL_STFEv1_PidLeakFilter_t STFEv1_PidLeakFilter;
		stptiHAL_STFEv2_PidLeakFilter_t STFEv2_PidLeakFilter;
	} STFE_PidLeakFilter;
} stptiHAL_STFE_PidFilterDevice_t;

typedef struct {
	volatile U32 STREAM_BASE;
	volatile U32 STREAM_TOP;
	volatile U32 STREAM_PKT_SIZE;
	volatile U32 STREAM_FORMAT;
	volatile U32 STREAM_PACE_CFG;
	volatile U32 STREAM_PC;
	volatile U32 STREAM_STATUS;
	volatile U32 padding;
} stptiHAL_STFE_TSDmaBlockChannel_t;

typedef union {
	stptiHAL_STFEv1_TSDmaMixedReg STFEv1_TSDmaMixedReg;
	stptiHAL_STFEv2_TSDmaMixedReg STFEv2_TSDmaMixedReg;
} stptiHAL_STFE_TSDmaMixedReg;

typedef struct {
	stptiHAL_STFE_TSDmaMixedReg STFE_TSDmaMixedReg;
	volatile U32 DEST_FIFO_TRIG;
	volatile U32 DEST_BYPASS;
	volatile U32 DEST_CLKIDIV;
	volatile U32 DEST_FORMAT_STFEv2;
	volatile U32 DEST_STATUS;
	volatile U32 padding2;
} stptiHAL_STFE_TSDmaBlockOutput_t;

typedef struct {
	stptiHAL_STFE_TSDmaBlockChannel_t *TSDMA_CHANNEL;
	stptiHAL_STFE_TSDmaBlockOutput_t *TSDMA_OUTPUT;
} stptiHAL_STFE_TSDmaBlockDevice_t;

typedef union {
	stptiHAL_STFEv1_SystemBlockDevice_t STFEv1_SystemBlockDevice;
	stptiHAL_STFEv2_SystemBlockDevice_t STFEv2_SystemBlockDevice;
} stptiHAL_STFE_SystemBlockDevice_t;

typedef union {
	stptiHAL_STFEv1_DmaBlockDevice_t STFEv1_DmaBlockDevice;
	stptiHAL_STFEv2_DmaBlockDevice_t STFEv2_DmaBlockDevice;
} stptiHAL_STFE_DmaBlockDevice_t;

typedef struct {
	volatile U32 CCSC_CTRL;
	volatile U32 CCSC_CMP_RES;
	volatile U32 CCSC_CLK_CTRL;
} stptiHAL_STFE_CCSCBlockDevice_t;

/* Structure to hold all the base pointers */
typedef struct {

	stptiHAL_STFE_InputBlockDevice_t *InputBlockBase_p;
	stptiHAL_STFE_MergedInputBlockDevice_t *MergedInputBlockBase_p;
	stptiHAL_STFE_SWTSInputBlockDevice_t *SWTSInputBlockBase_p;
	stptiHAL_STFE_TagCounterDevice_t *TagCounterBase_p;
	stptiHAL_STFE_PidFilterDevice_t *PidFilterBaseAddressPhys_p;
	stptiHAL_STFE_SystemBlockDevice_t *SystemBlockBase_p;
	stptiHAL_STFE_DmaBlockDevice_t *DmaBlockBase_p;
	stptiHAL_STFE_TSDmaBlockDevice_t TSDmaBlockBase;
	stptiHAL_STFE_CCSCBlockDevice_t *CCSCBlockBase_p;
} stptiHAL_STFE_Device_t;

/* STFE Internal RAM Layout */

typedef struct {
	U8 *STFE_MappedAddress_p;			/**< Mapped Frontend devices address(es) */
	U8 *STFE_RAM_Ptr_MappedAddress_p;		/**< Mapped Frontend RAM address(es) */
	U8 *STFE_RAM_MappedAddress_p[MAX_NUMRAM];	/**< Mapped Frontend RAM address(es) */
} stptiTSHAL_StfeInput_t;

//------------------------------------------------------------------------------------
typedef struct {
	U32 NumRam;
	U32 RAMPointerPhyAddr;
	U32 RAMPtrSize;		/* whole ram size */
	U32 RAMPtrBlock;	/* size of a block */
	U32 NumPtrRecords;	/* number of IB + MIB */
	U32 RAMSize[MAX_NUMRAM];
	U32 RAMAddr[MAX_NUMRAM];
	U32 FifoSize[MAX_NUMRAM];
	U32 RAMNumIB[MAX_NUMRAM];
	U32 RAMNumMIB[MAX_NUMRAM];
	U32 RAMNumSWTS[MAX_NUMRAM];
} stptiTSHAL_STFE_RamConfig_t;

typedef struct {
	U32 stfe_version;
	void *PhysicalAddress;
	U32 MapSize;
	void *RAMPhysicalAddress;
	U32 RAMMapSize;
	U32 NumIBs;
	U32 NumMIBs;
	U32 NumSWTSs;
	U32 NumTSDMAs;
	U32 NumCCSCs;
	U32 NumTagCs;
	U32 IBOffset;
	U32 TagOffset;
	U32 PidFilterOffset;
	U32 SystemRegsOffset;
	U32 MemDMAOffset;
	U32 TSDMAOffset;
	U32 CCSCOffset;
	stptiTSHAL_STFE_RamConfig_t RamConfig;
	U32 Idle_InterruptNumber;
	U32 Error_InterruptNumber;
	BOOL SoftwareLeakyPID;
	U32 NumTP;
	U32 NumTPUsed;
	struct platform_device *pdev;
	stptiTSHAL_TSInputPowerState_t PowerState;
	BOOL ClocksEnabled;
	stptiTSHAL_TSInputDestination_t DefaultDest;
	U32 tsin_enabled;
} stptiTSHAL_STFE_Config_t;

typedef struct {
	stptiTSHAL_StfeInput_t StfeRegs; /**< Currently STFE is only input device type supported */
} stptiTSHAL_InputResource_t;

typedef struct stptiTSHAL_HW_function_s {
	int (*stptiTSHAL_StfeCreateConfig)
	 (stptiTSHAL_STFE_RamConfig_t * RamConfig_p, U32 *pN_TPs);
	void (*stptiTSHAL_StfeLoadMemDmaFw) (void);
	void (*stptiTSHAL_StfeSystemInit) (void);
	void (*stptiTSHAL_StfeSystemTerm) (void);
	void (*stptiTSHAL_StfeSystemIBRoute) (U32 Index, stptiTSHAL_TSInputDestination_t Destination);
	void (*stptiTSHAL_StfeEnableLeakyPid) (U32 STFE_InputChannel, BOOL EnableLeakyPID);
	void (*stptiTSHAL_StfeLeakCountReset) (void);
	void (*stptiTSHAL_StfeTsdmaSetBasePointer)(stptiHAL_STFE_TSDmaBlockDevice_t *TSDmaBlockBase_p);
	void (*stptiTSHAL_StfeTsdmaEnableInputRoute) (U32 ChannelIndex, U32 OutputBlockIndex, BOOL Enable);
	void (*stptiTSHAL_StfeTsdmaConfigureDestFormat) (U32 OutputIndex, U32 Data);
	void (*stptiTSHAL_StfeTsdmaDumpMixedReg) (U32 TSOutputIndex, stptiSUPPORT_sprintf_t * ctx_p);
	void (*stptiTSHAL_StfeMemdmaClearDREQLevel) (U32 Index, U32 BusSize);
	 U32(*stptiTSHAL_StfeMemdma_ram2beUsed) (U32 Index, stptiHAL_TSInputStfeType_t IBType, U32 * pIndexInRam);
	void (*stptiTSHAL_StfeWrite_RamInputNode_ptrs) (U32 MemDmaChannel, U8 * RAMStart_p,
							stptiSupport_DMAMemoryStructure_t RAMCachedMemoryStructure,
							U32 BufferSize, U32 PktSize, U32 FifoSize, U32 InternalBase);
	void (*stptiTSHAL_StfeMemdmaInit) (void);
	void (*stptiTSHAL_StfeMemdmaTerm) (void);
	ST_ErrorCode_t (*stptiTSHAL_StfeMemdmaChangeSubstreamStatus)(U32 MemDmaChannel, U32 TPMask, BOOL status);
	void (*stptiTSHAL_StfeMemDmaRegisterDump) (U32 MemDmaIndex, BOOL Verbose, stptiSUPPORT_sprintf_t * ctx_p);
	void (*stptiTSHAL_StfeDmaPTRRegisterDump) (U32 MemDmaIndex, stptiSUPPORT_sprintf_t * ctx_p);
	void (*stptiTSHAL_StfeSystemRegisterDump) (stptiSUPPORT_sprintf_t * ctx_p);
	void (*stptiTSHAL_StfeCCSCConfig) (void);
	void (*stptiTSHAL_StfeCCSCBypass) (void);
	void (*stptiTSHAL_StfeMonitorDREQLevels) (U32 * tp_read_pointers, U8 num_rps, U32 dma_pck_size);
	void (*stptiTSHAL_StfeEnableInputClock) (U32 Index, stptiHAL_TSInputStfeType_t IBType, BOOL Enable);

} stptiTSHAL_HW_function_t;

//  EXTERN GLOBAL VARIABLES
extern stptiHAL_STFE_Device_t STFE_BasePointers;
extern stptiTSHAL_STFE_Config_t STFE_Config;
extern stptiTSHAL_InputResource_t TSInputHWMapping;

#define stptiTSHAL_GetHWMapping() (&TSInputHWMapping)

ST_ErrorCode_t stptiTSHAL_StfeMapHW(U32 *pN_TPs);
ST_ErrorCode_t stptiTSHAL_StfeStartHW(void);
void stptiTSHAL_StfeIrqDisabled(void);
void stptiTSHAL_StfeUnmapHW(void);

ST_ErrorCode_t stptiTSHAL_StfeGetHWTimer(U32 IBBlockIndex, stptiHAL_TSInputStfeType_t IBType,
					 stptiTSHAL_TimerValue_t * TimeValue_p, BOOL * IsTSIN_p);
BOOL stptiTSHAL_StfeGetIBEnableStatus(U16 Index, stptiHAL_TSInputStfeType_t IBType);
U32 stptiTSHAL_StfeReadStatusReg(U32 Index, stptiHAL_TSInputStfeType_t IBType);
void stptiTSHAL_StfeEnable(U32 Index, stptiHAL_TSInputStfeType_t IBType, BOOL Enable);
void stptiTSHAL_StfeTAGBytesConfigure(U32 Index, stptiHAL_TSInputStfeType_t IBType, U16 TagHeader,
				      stptiTSHAL_TSInputTagging_t InputTagged);
void stptiTSHAL_StfeWritePIDBase(U32 Index, U32 Address);
void stptiTSHAL_StfePIDFilterConfigure(U32 Index, stptiHAL_TSInputStfeType_t IBType, U8 PIDBitSize, U8 Offset,
				       BOOL Enable);
void stptiTSHAL_StfePIDFilterEnable(U32 Index, stptiHAL_TSInputStfeType_t IBType, BOOL Enable);
void stptiTSHAL_StfeTAGCounterSelect(U32 Index, U8 TagCounter);
void stptiTSHAL_StfeWriteSLD(U32 Index, stptiHAL_TSInputStfeType_t IBType, U8 Lock, U8 Drop, U8 Token);
void stptiTSHAL_StfeWriteInputFormat(U32 Index, stptiHAL_TSInputStfeType_t IBType, U32 Format);
void stptiTSHAL_StfeWritePktLength(U32 Index, stptiHAL_TSInputStfeType_t IBType, U32 Length);
void stptiTSHAL_StfeWriteToIBInternalRAMPtrs(U32 Index, stptiHAL_TSInputStfeType_t IBType);

void stptiTSHAL_StfeEnableLeakyPid(U32 STFE_InputChannel, BOOL EnableLeakyPID);
ST_ErrorCode_t stptiTSHAL_StfeTsdmaFlushSubstream(U32 IBIndex, U32 TSOutputIndex);
U32 stptiTSHAL_StfeTsdmaReadStreamStatus(U32 Index);
void stptiTSHAL_StfeTsdmaConfigureInternalRAM(U32 Index, stptiHAL_TSInputStfeType_t IBType);
void stptiTSHAL_StfeTsdmaConfigurePktSize(U32 Index, U32 PktSize);
void stptiTSHAL_StfeTsdmaConfigureStreamFormat(U32 Index, BOOL Tagged);
void stptiTSHAL_StfeTsdmaConfigurePacing(U32 IBBlockIndex, U32 OutputPacingRate, U32 OutputPacingClkSrc,
					 BOOL InputTagging, BOOL SetPace);
ST_ErrorCode_t stptiTSHAL_StfeMIBEnableSubstream(U16 Index, BOOL SetClear);
BOOL stptiTSHAL_StfeMIBReadSubstreamStatus(U16 Index);

//*************  DUMP *****************/
void stptiTSHAL_StfeIBRegisterDump(U32 IBBlockIndex, stptiSUPPORT_sprintf_t *ctx_p);
void stptiTSHAL_StfeSWTSRegisterDump(U32 SWTSBlockIndex, stptiSUPPORT_sprintf_t *ctx_p);
void stptiTSHAL_StfeMIBRegisterDump(U32 MIBChannel, stptiSUPPORT_sprintf_t *ctx_p);
void stptiTSHAL_StfeTSDMARegisterDump(U32 IBBlockIndex, U32 TSOutputIndex, stptiSUPPORT_sprintf_t *ctx_p);
void stptiTSHAL_StfePckRAMDump(U32 IBBlockIndex, stptiHAL_TSInputStfeType_t IBType, stptiSUPPORT_sprintf_t *ctx_p);
void stptiTSHAL_StfeCCSCRegisterDump(stptiSUPPORT_sprintf_t *ctx_p);

/*
 * INIT /TERM
 */
void stptiTSHAL_Stfev1_LoadMemDmaFw(void);
void stptiTSHAL_Stfev2_LoadMemDmaFw(void);

int stptiTSHAL_Stfev1_CreateConfig(stptiTSHAL_STFE_RamConfig_t *RamConfig_p, U32 *pN_TPs);
int stptiTSHAL_Stfev2_CreateConfig(stptiTSHAL_STFE_RamConfig_t *RamConfig_p, U32 *pN_TPs);

void stptiTSHAL_Stfev1_MemdmaInit(void);
void stptiTSHAL_Stfev2_MemdmaInit(void);

void stptiTSHAL_Stfev1_MemdmaTerm(void);
void stptiTSHAL_Stfev2_MemdmaTerm(void);

/*
 * TSDMA
 */
void stptiTSHAL_Stfev1_TsdmaSetBasePointer(stptiHAL_STFE_TSDmaBlockDevice_t *TSDmaBlockBase_p);
void stptiTSHAL_Stfev2_TsdmaSetBasePointer(stptiHAL_STFE_TSDmaBlockDevice_t *TSDmaBlockBase_p);

void stptiTSHAL_Stfev1_TsdmaEnableInputRoute(U32 ChannelIndex, U32 OutputBlockIndex, BOOL Enable);
void stptiTSHAL_Stfev2_TsdmaEnableInputRoute(U32 ChannelIndex, U32 OutputBlockIndex, BOOL Enable);

void stptiTSHAL_Stfev1_TsdmaConfigureDestFormat(U32 OutputIndex, U32 Data);
void stptiTSHAL_Stfev2_TsdmaConfigureDestFormat(U32 OutputIndex, U32 Data);

void stptiTSHAL_Stfev1_TsdmaDumpMixedReg(U32 TSOutputIndex, stptiSUPPORT_sprintf_t *ctx_p);
void stptiTSHAL_Stfev2_TsdmaDumpMixedReg(U32 TSOutputIndex, stptiSUPPORT_sprintf_t *ctx_p);
/*
 * SYSTEM
 */
void stptiTSHAL_Stfev1_SystemInit(void);
void stptiTSHAL_Stfev2_SystemInit(void);

void stptiTSHAL_Stfev2_SystemTerm(void);
void stptiTSHAL_Stfev1_SystemTerm(void);

void stptiTSHAL_Stfev1_SystemIBRoute(U32 Index, stptiTSHAL_TSInputDestination_t Destination);
void stptiTSHAL_Stfev2_SystemIBRoute(U32 Index, stptiTSHAL_TSInputDestination_t Destination);
/*
 * PIDFilter
 */
void stptiTSHAL_Stfev1_EnableLeakyPid(U32 STFE_InputChannel, BOOL EnableLeakyPID);
void stptiTSHAL_Stfev2_EnableLeakyPid(U32 STFE_InputChannel, BOOL EnableLeakyPID);

void stptiTSHAL_Stfev1_LeakCountReset(void);
void stptiTSHAL_Stfev2_LeakCountReset(void);
/*
 * MEMDMA
 */
void stptiTSHAL_Stfev1_MemdmaClearDREQLevel(U32 Index, U32 BusSize);
void stptiTSHAL_Stfev2_MemdmaClearDREQLevel(U32 Index, U32 BusSize);

U32 stptiTSHAL_Stfev1_Memdma_ram2beUsed(U32 Index, stptiHAL_TSInputStfeType_t IBType, U32 *pIndexInRam);
U32 stptiTSHAL_Stfev2_Memdma_ram2beUsed(U32 Index, stptiHAL_TSInputStfeType_t IBType, U32 *pIndexInRam);

ST_ErrorCode_t stptiTSHAL_Stfev1_MemdmaChangeSubstreamStatus(U32 MemDmaChannel, U32 TPMask, BOOL status);
ST_ErrorCode_t stptiTSHAL_Stfev2_MemdmaChangeSubstreamStatus(U32 MemDmaChannel, U32 TPMask, BOOL status);

void stptiTSHAL_Stfev1_MemDmaRegisterDump(U32 MemDmaIndex, BOOL Verbose, stptiSUPPORT_sprintf_t *ctx_p);
void stptiTSHAL_Stfev2_MemDmaRegisterDump(U32 MemDmaIndex, BOOL Verbose, stptiSUPPORT_sprintf_t *ctx_p);
/*
 * INTERNAL RAM
 */
void stptiTSHAL_Stfev1_Write_RamInputNode_ptrs(U32 MemDmaChannel, U8 *RAMStart_p,
					       stptiSupport_DMAMemoryStructure_t RAMCachedMemoryStructure,
					       U32 BufferSize, U32 PktSize, U32 FifoSize, U32 InternalBase);
void stptiTSHAL_Stfev2_Write_RamInputNode_ptrs(U32 MemDmaChannel, U8 *RAMStart_p,
					       stptiSupport_DMAMemoryStructure_t RAMCachedMemoryStructure,
					       U32 BufferSize, U32 PktSize, U32 FifoSize, U32 InternalBase);

void stptiTSHAL_Stfev1_DmaPTRRegisterDump(U32 MemDmaIndex, stptiSUPPORT_sprintf_t *ctx_p);
void stptiTSHAL_Stfev2_DmaPTRRegisterDump(U32 MemDmaIndex, stptiSUPPORT_sprintf_t *ctx_p);

void stptiTSHAL_Stfev1_SystemRegisterDump(stptiSUPPORT_sprintf_t *ctx_p);
void stptiTSHAL_Stfev2_SystemRegisterDump(stptiSUPPORT_sprintf_t *ctx_p);
/*
 * CCSC Config
 */
void stptiTSHAL_Stfev1_CCSCConfig(void);
void stptiTSHAL_Stfev2_CCSCConfig(void);

void stptiTSHAL_Stfev1_CCSCBypass(void);
void stptiTSHAL_Stfev2_CCSCBypass(void);
/*
 * Dreq level monitor
 */
void stptiTSHAL_Stfev1_MonitorDREQLevels(U32 *tp_read_pointers, U8 num_rps, U32 dma_pck_size);
void stptiTSHAL_Stfev2_MonitorDREQLevels(U32 *tp_read_pointers, U8 num_rps, U32 dma_pck_size);

/* Input clock enable/disable */
void stptiTSHAL_Stfev1_InputClockEnable(U32 Index, stptiHAL_TSInputStfeType_t IBType, BOOL Enable);
void stptiTSHAL_Stfev2_InputClockEnable(U32 Index, stptiHAL_TSInputStfeType_t IBType, BOOL Enable);
#endif /* _PTI_STFE_H_ */
