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
   @file   pti_stfe_v1.c
   @brief  LLD driver to manage the STFE blocks
           that belong only to this version of STFE.

   LLD driver to manage all the STFE block
       that belong only to this version of STFE.
 */

#include "linuxcommon.h"

/* Includes from API level */
#include "../pti_debug.h"
#include "../pti_hal_api.h"
#include "../pti_driver.h"

/* Includes from the HAL / ObjMan level */
#include "pti_tsinput.h"
#include "pti_stfe.h"

#define FW_VERSION  "STFE v1: no FW is used"

#define NUM_RAM_STFE_V1     (1)
#define NUM_TP_STFE_V1 	    (1)
/* CCSC Register Values */
#define STFE_CCSC_ENABLE                            (0x00000007)
#define STFE_CCSC_BYPASS                            (0x00010001)

static irqreturn_t stptiHAL_ErrorInterruptHandler(int irq, void *pDeviceHandle)
{
	stptiHAL_STFEv1_SystemBlockDevice_t *SystemBlockDevice_p =
	    (stptiHAL_STFEv1_SystemBlockDevice_t *) pDeviceHandle;
	U32 input_status;

	do {
		U32 Tmp, i;
		input_status = stptiSupport_ReadReg32(&(SystemBlockDevice_p->STATUS));

		STPTI_PRINTF("-------------------------------------------------");
		STPTI_PRINTF("INPUT_STATUS :  0x%08X", input_status);
		STPTI_PRINTF("INPUT_MASK :    0x%08X", stptiSupport_ReadReg32(&(SystemBlockDevice_p->MASK)));
		STPTI_PRINTF("-------------------------------------------------");

		for (i = 0; i < 32; i++) {
			if (((U32) input_status) >> i & 0x1) {

				if (i < STFE_Config.NumIBs) {
					Tmp = stptiSupport_ReadReg32(&(STFE_BasePointers.InputBlockBase_p[i].STATUS));
					STPTI_PRINTF("STATUS  OF IB %d     :  0x%08X", i, Tmp);
					STPTI_PRINTF("STATUS       :  FifoOverFlow        : %u", Tmp & 0x01);
					STPTI_PRINTF("STATUS       :  BufferOverflow      : %u", (Tmp >> 1) & 0x01);
					STPTI_PRINTF("STATUS       :  OutOfOrderRP        : %u", (Tmp >> 2) & 0x01);
					STPTI_PRINTF("STATUS       :  PIDOverFlow         : %u", (Tmp >> 3) & 0x01);
					STPTI_PRINTF("STATUS       :  PktOverFlow         : %u", (Tmp >> 4) & 0x01);
					STPTI_PRINTF("STATUS       :  ErrorPkts           : %u", (Tmp >> 8) & 0x0F);
					STPTI_PRINTF("STATUS       :  ShortPkts           : %u", (Tmp >> 12) & 0x0F);
					STPTI_PRINTF("STATUS       :  Lock                : %u", (Tmp >> 30) & 0x01);
				} else if (i < (STFE_Config.NumIBs + STFE_Config.NumMIBs)) {
					Tmp = stptiSupport_ReadReg32(&(STFE_BasePointers.MergedInputBlockBase_p[i - STFE_Config.NumIBs].STATUS));
					STPTI_PRINTF("STATUS  OF MIB %d     :  0x%08X", (i - STFE_Config.NumIBs), Tmp);
					STPTI_PRINTF("STATUS       :  0x%08X", Tmp);
					STPTI_PRINTF("STATUS       :  FifoOverFlow        : %u", Tmp & 0x01);
					STPTI_PRINTF("STATUS       :  BufferOverflow      : %u", (Tmp >> 1) & 0x01);
					STPTI_PRINTF("STATUS       :  OutOfOrderRP        : %u", (Tmp >> 2) & 0x01);
					STPTI_PRINTF("STATUS       :  PIDOverFlow         : %u", (Tmp >> 3) & 0x01);
					STPTI_PRINTF("STATUS       :  PktOverFlow         : %u", (Tmp >> 4) & 0x01);
					STPTI_PRINTF("STATUS       :  BadStream           : %u", (Tmp >> 5) & 0x01);
					STPTI_PRINTF("STATUS       :  ErrorPkts           : %u", (Tmp >> 8) & 0x0F);
					STPTI_PRINTF("STATUS       :  ShortPkts           : %u", (Tmp >> 12) & 0x0F);
				} else if (i < (STFE_Config.NumIBs + STFE_Config.NumMIBs + STFE_Config.NumSWTSs)) {
					Tmp = stptiSupport_ReadReg32(&(STFE_BasePointers.SWTSInputBlockBase_p[i - (STFE_Config.NumIBs + STFE_Config.NumMIBs)].STATUS));
					STPTI_PRINTF("STATUS  OF SWTS %d     :  0x%08X",
						     (i - (STFE_Config.NumIBs + STFE_Config.NumMIBs)), Tmp);
					STPTI_PRINTF("STATUS       :  0x%08X", Tmp);
					STPTI_PRINTF("STATUS       :  FifoOverFlow        : %u", Tmp & 0x01);
					STPTI_PRINTF("STATUS       :  OutOfOrderRP        : %u", (Tmp >> 2) & 0x01);
					STPTI_PRINTF("STATUS       :  DiscardedPkts       : %u", (Tmp >> 8) & 0x0F);
					STPTI_PRINTF("STATUS       :  Locked              : %u", (Tmp >> 30) & 0x01);
					STPTI_PRINTF("STATUS       :  FIFOReq             : %u", (Tmp >> 31) & 0x01);
				} else {
					STPTI_PRINTF("Error into MEMDMA or TSDMA");
				}
				STPTI_PRINTF("-------------------------------------------------");
			}
		}
	} while (input_status);

	return IRQ_HANDLED;
}

void stptiTSHAL_Stfev1_LoadMemDmaFw(void)
{

}

int stptiTSHAL_Stfev1_CreateConfig(stptiTSHAL_STFE_RamConfig_t *RamConfig_p, U32 *pN_TPs)
{
	U32 i;

	memset(RamConfig_p, 0xF, sizeof(stptiTSHAL_STFE_RamConfig_t));

	RamConfig_p->NumPtrRecords = STFE_Config.NumIBs + STFE_Config.NumMIBs;

	RamConfig_p->NumRam = NUM_RAM_STFE_V1;
	RamConfig_p->RAMSize[0] = STFE_Config.RAMMapSize;
	RamConfig_p->RAMAddr[0] = 0x0000;

	/*** split the ram into pointer and fifo zones **/
	for (i = 0; i < RamConfig_p->NumRam; i++) {
		/* We need to calculate the memory organisation based on the size of the internal RAM
		   We need 1 32byte pointer record for each IB and MIB's, records are not required for SWTS as they are only routed through the TSDMA */
		U32 NumPtrRecords = RamConfig_p->NumPtrRecords;
		U32 StartAddressOfFifos = NumPtrRecords * 0x20; // sizeof(stptiHAL_STFEv1_InputNode_t);

		/* Make sure the start address is aligned by 64 */
		if (StartAddressOfFifos % 0x40) {
			StartAddressOfFifos += 0x40;
			StartAddressOfFifos &= ~0x3F;
		}

		RamConfig_p->RAMNumIB[i] = STFE_Config.NumIBs;
		RamConfig_p->RAMNumMIB[i] = STFE_Config.NumMIBs;
		RamConfig_p->RAMNumSWTS[i] = STFE_Config.NumSWTSs;
		RamConfig_p->RAMAddr[i] = StartAddressOfFifos;
		RamConfig_p->RAMPtrSize = StartAddressOfFifos;
		RamConfig_p->RAMPointerPhyAddr = (U32) STFE_Config.RAMPhysicalAddress;

		RamConfig_p->RAMSize[i] = RamConfig_p->RAMSize[i] - RamConfig_p->RAMPtrSize;
		RamConfig_p->FifoSize[i] =
		    ((RamConfig_p->RAMSize[i]) /
		     (RamConfig_p->RAMNumIB[i] + RamConfig_p->RAMNumMIB[i] + RamConfig_p->RAMNumSWTS[i]))
		    & ~0x3f;
	}

	*pN_TPs = NUM_TP_STFE_V1;

	STPTI_PRINTF("THIS DEVICE DOES NOT REQUIRE A MEMDMA FW");
	return 0;
}

/**********************************************************
 *
 *  	TSDMA REGISTERS
 *
 **********************************************************/
void stptiTSHAL_Stfev1_TsdmaSetBasePointer(stptiHAL_STFE_TSDmaBlockDevice_t *TSDmaBlockBase_p)
{
	STFE_BasePointers.TSDmaBlockBase.TSDMA_CHANNEL =
	    (stptiHAL_STFE_TSDmaBlockChannel_t *) ((U32) TSDmaBlockBase_p + (U32) TSDMA_CHANNEL_OFFSET_STFEv1);
	STFE_BasePointers.TSDmaBlockBase.TSDMA_OUTPUT =
	    (stptiHAL_STFE_TSDmaBlockOutput_t *) ((U32) TSDmaBlockBase_p + (U32) TSDMA_OUTPUT_OFFSET_STFEv1);
}
/**
   @brief  STFE TSDMA Hardware worker function

   Set/Clear the Input Enable for the select channel

   Remember the index supplied in this channel may be different to the value used for the MEMDMA

   @param  ChannelIndex       Selected input channel
   @param  OutputBlockIndex   Selected output channel
   @param  Enable             Bool for enable/disable
   @return                    Register value
 */
void stptiTSHAL_Stfev1_TsdmaEnableInputRoute(U32 ChannelIndex, U32 OutputBlockIndex, BOOL Enable)
{
	if (STFE_BasePointers.TSDmaBlockBase.TSDMA_CHANNEL != NULL && (ChannelIndex < STFE_MAX_TSDMA_BLOCK_SIZE)
	    && (OutputBlockIndex < STFE_Config.NumTSDMAs)) {
		stpti_printf("ChannelIndex %d OutputBlockIndex %d Enable %d", ChannelIndex, OutputBlockIndex, Enable);

		stptiSupport_ReadModifyWriteReg32((volatile U32 *)
						  &(STFE_BasePointers.TSDmaBlockBase.TSDMA_OUTPUT[OutputBlockIndex].
						    STFE_TSDmaMixedReg.STFEv1_TSDmaMixedReg.DEST_ROUTE),
						  (1 << ChannelIndex), (Enable ? (1 << ChannelIndex) : 0));
	}
}

/**
   @brief  STFE TSDMA Hardware worker function

   Configure Out Channel to TSDMA - Currently only one channel but will be more on future devices.

   @param  TSInputHWMapping_p Pointer to mapped registers
   @param  Data               U32 value to write to register
   @return                    none
 */
void stptiTSHAL_Stfev1_TsdmaConfigureDestFormat(U32 OutputIndex, U32 Data)
{
	if (STFE_BasePointers.TSDmaBlockBase.TSDMA_CHANNEL != NULL && (OutputIndex < STFE_Config.NumTSDMAs)) {
		stpti_printf("Data 0x%x", Data);

		stptiSupport_WriteReg32((volatile U32 *)
					&(STFE_BasePointers.TSDmaBlockBase.TSDMA_OUTPUT[OutputIndex].STFE_TSDmaMixedReg.STFEv1_TSDmaMixedReg.DEST_FORMAT_STFEv1), Data);
	}
}

/**
   @brief  STFE TSDMA Hardware worker function

 */
void stptiTSHAL_Stfev1_TsdmaDumpMixedReg(U32 TSOutputIndex, stptiSUPPORT_sprintf_t *ctx_p)
{
	stptiSUPPORT_sprintf(ctx_p, "DEST_ROUTE     :  0x%08X\n",
			     stptiSupport_ReadReg32(&(STFE_BasePointers.TSDmaBlockBase.TSDMA_OUTPUT[TSOutputIndex].STFE_TSDmaMixedReg.STFEv1_TSDmaMixedReg.DEST_ROUTE)));
	stptiSUPPORT_sprintf(ctx_p, "DEST_FORMAT    :  0x%08X\n",
			     stptiSupport_ReadReg32(&(STFE_BasePointers.TSDmaBlockBase.TSDMA_OUTPUT[TSOutputIndex].STFE_TSDmaMixedReg.STFEv1_TSDmaMixedReg.DEST_FORMAT_STFEv1)));
}

/**********************************************************
 *
 *  	SYSTEM REGISTERS
 *
 **********************************************************/
void stptiTSHAL_Stfev1_SystemInit(void)
{
	U32 Error_InterruptNumber = STFE_Config.Error_InterruptNumber;

	if (Error_InterruptNumber) {

		if (request_irq(Error_InterruptNumber, stptiHAL_ErrorInterruptHandler,
				IRQF_DISABLED, "Error_MBX", STFE_BasePointers.SystemBlockBase_p)) {
			STPTI_PRINTF_ERROR("MEMDMA ISR installation failed on number 0x%08x.", Error_InterruptNumber);
		} else {
			STPTI_PRINTF("TP ISR installed on number 0x%08x.", Error_InterruptNumber);
		}
		stptiSupport_WriteReg32((volatile U32 *)
					&(STFE_BasePointers.SystemBlockBase_p->STFEv1_SystemBlockDevice.MASK),
					0xFFFFFFFF);
	}
}

void stptiTSHAL_Stfev1_SystemTerm(void)
{
#if !defined( TANGOSIM ) && !defined( TANGOSIM2 )

	U32 Error_InterruptNumber = STFE_Config.Error_InterruptNumber;

	if (Error_InterruptNumber) {
		stptiSupport_WriteReg32((volatile U32 *)
					&(STFE_BasePointers.SystemBlockBase_p->STFEv1_SystemBlockDevice.MASK), 0x0);

		//1 - install the IRQ
		disable_irq(Error_InterruptNumber);
		free_irq(Error_InterruptNumber, STFE_BasePointers.SystemBlockBase_p);
		STPTI_PRINTF("TP ISR uninstalled on number 0x%08x.", Error_InterruptNumber);
	}
#endif
}

/**
   @brief  Stfe Hardware worker function

   Select route for IB output

   Remember the index supplied in this channel may be different to the value used for the MEMDMA

   @param  TSInputHWMapping_p Pointer to mapped registers
   @param  Index              Selected channel
   @param  Destination        Routing Destination
   @return                    none
 */
void stptiTSHAL_Stfev1_SystemIBRoute(U32 Index, stptiTSHAL_TSInputDestination_t Destination)
{
	if (STFE_BasePointers.SystemBlockBase_p != NULL && (Index < MAX_NUMBER_OF_TSINPUTS)) {
		stpti_printf("");

		stptiSupport_ReadModifyWriteReg32((volatile U32 *)
						  &(STFE_BasePointers.SystemBlockBase_p->STFEv1_SystemBlockDevice.
						    DMA_ROUTE), (1 << Index),
						  ((Destination != stptiTSHAL_TSINPUT_DEST_DEMUX) ? (1 << Index) : 0));
	}
}

/**********************************************************
 *
 *  	PID FILTER REGISTERS
 *
 **********************************************************/
void stptiTSHAL_Stfev1_EnableLeakyPid(U32 STFE_InputChannel, BOOL EnableLeakyPID)
{

	if (STFE_BasePointers.PidFilterBaseAddressPhys_p != NULL) {
		if (EnableLeakyPID)
			stptiSupport_ReadModifyWriteReg32(&(STFE_BasePointers.PidFilterBaseAddressPhys_p->STFE_PidLeakFilter.STFEv1_PidLeakFilter.PIDF_LEAK_ENABLE),
							  1 << STFE_InputChannel, 1 << STFE_InputChannel);
		else
			stptiSupport_ReadModifyWriteReg32(&(STFE_BasePointers.PidFilterBaseAddressPhys_p->STFE_PidLeakFilter.STFEv1_PidLeakFilter.PIDF_LEAK_ENABLE),
							  1 << STFE_InputChannel, 0);
	}

}

void stptiTSHAL_Stfev1_LeakCountReset(void)
{
	/* This sets the rate, but does enable it.  It is enabled in stptiTSHAL_StfeChangeActivityState() when a pid is set */
	stptiSupport_WriteReg32(&(STFE_BasePointers.PidFilterBaseAddressPhys_p->STFE_PidLeakFilter.STFEv1_PidLeakFilter.PIDF_LEAK_COUNT_RESET), STFE_LEAK_COUNT_RESET);	/* (gray coded value) Let through 1 in every 1000 packets */
}

/*************************************
 *
 * function not implemented in this version of the MEMDMA
 *
 *************************************/
U32 stptiTSHAL_Stfev1_Memdma_ram2beUsed(U32 Index, stptiHAL_TSInputStfeType_t IBType, U32 *pIndexInRam)
{
	U32 RamtoUse = 0;	// there is only one RAM

	switch (IBType) {
	case (stptiTSHAL_TSINPUT_STFE_IB):
		*pIndexInRam = Index;
		break;
	case (stptiTSHAL_TSINPUT_STFE_MIB):
		*pIndexInRam = Index + STFE_Config.RamConfig.RAMNumIB[RamtoUse];
		break;
	case (stptiTSHAL_TSINPUT_STFE_SWTS):
		*pIndexInRam = Index + STFE_Config.RamConfig.RAMNumIB[RamtoUse] +
		    STFE_Config.RamConfig.RAMNumMIB[RamtoUse];
		break;
	default:
		break;
	}
	return (RamtoUse);
}

void stptiTSHAL_Stfev1_MemdmaTerm(void)
{
	// no additional steps are required to stop the MemDma
}

typedef struct {
	U32 BUS_BASE;
	U32 BUS_TOP;
} stptiTSHAL_MonitorPointers;

static stptiTSHAL_MonitorPointers STFE_MonitorPointers[MAX_NUMBER_OF_TSINPUTS] =
    { {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0},
    {0, 0}, {0, 0} };

void stptiTSHAL_Stfev1_Write_RamInputNode_ptrs(U32 MemDmaChannel, U8 *RAMStart_p,
					       stptiSupport_DMAMemoryStructure_t RAMCachedMemoryStructure,
					       U32 BufferSize, U32 PktSize, U32 FifoSize, U32 InternalBase)
{
	stptiTSHAL_InputResource_t *TSInputHWMapping_p = stptiTSHAL_GetHWMapping();
	stptiHAL_STFEv1_InputNode_t *STFE_RAM_Mapped_InputNode_p =
	    (stptiHAL_STFEv1_InputNode_t *) TSInputHWMapping_p->StfeRegs.STFE_RAM_Ptr_MappedAddress_p;

	U32 Base = (U32) stptiSupport_VirtToPhys(&RAMCachedMemoryStructure, RAMStart_p);
	U32 Top = (U32) stptiSupport_VirtToPhys(&RAMCachedMemoryStructure, RAMStart_p + BufferSize - 1);
	U32 NextBase = Base;
	U32 NextTop = Top;

	stpti_printf("Index 0x%04x Base 0x%08x Top 0x%08x NextBase 0x%08x NextTop 0x%08x PktSize 0x%08x", MemDmaChannel,
		     Base, Top, NextBase, NextTop, PktSize);

	stptiSupport_WriteReg32((volatile U32 *)&(STFE_RAM_Mapped_InputNode_p[MemDmaChannel].BUS_BASE), Base);
	stptiSupport_WriteReg32((volatile U32 *)&(STFE_RAM_Mapped_InputNode_p[MemDmaChannel].BUS_TOP), Top);
	stptiSupport_WriteReg32((volatile U32 *)&(STFE_RAM_Mapped_InputNode_p[MemDmaChannel].BUS_WP), Base);
	stptiSupport_WriteReg32((volatile U32 *)&(STFE_RAM_Mapped_InputNode_p[MemDmaChannel].BUS_NEXT_BASE), NextBase);
	stptiSupport_WriteReg32((volatile U32 *)&(STFE_RAM_Mapped_InputNode_p[MemDmaChannel].BUS_NEXT_TOP), NextTop);
	stptiSupport_WriteReg32((volatile U32 *)&(STFE_RAM_Mapped_InputNode_p[MemDmaChannel].TS_PKT_SIZE), PktSize);

	stptiSupport_WriteReg32((volatile U32 *)&(STFE_RAM_Mapped_InputNode_p[MemDmaChannel].MEM_BASE), InternalBase);
	stptiSupport_WriteReg32((volatile U32 *)&(STFE_RAM_Mapped_InputNode_p[MemDmaChannel].MEM_TOP),
				InternalBase + FifoSize - 1);

	/* Save copies for the DMA monitoring task as these are switched by the MEMDMA engine during runtime */
	STFE_MonitorPointers[MemDmaChannel].BUS_BASE = Base;
	STFE_MonitorPointers[MemDmaChannel].BUS_TOP = NextTop;
}

/**
   @brief  STFE Hardware worker function

   Write the DMA base address location in the internal STFE RAM

   @param  Address            Internal RAM address
   @return                    none
 */
void stptiTSHAL_Stfev1_MemdmaInit(void)
{

	// FW_VERSION

	U32 Address_PointerBase = 0x000000;
	if (STFE_BasePointers.DmaBlockBase_p != NULL) {
		stpti_printf("Address 0x%08x", Address_PointerBase);

		stptiSupport_WriteReg32((volatile U32 *)
					&(STFE_BasePointers.DmaBlockBase_p->STFEv1_DmaBlockDevice.POINTER_BASE),
					Address_PointerBase);
	}
}

void stptiTSHAL_Stfev1_InputClockEnable(U32 Index, stptiHAL_TSInputStfeType_t
		IBType, BOOL Enable)
{
	/* to be implemented later */
}

/*************************************
 *
 *
 * GLOBAL FUNCTIONS
 *
 *
 * **********************************/

/**
   @brief  STFE Hardware worker function

   Flush the selected MEMDMA substream - retries a few times while waiting to complete.

   @param  Index              Index to select input block
   @return                    Error
 */
ST_ErrorCode_t stptiTSHAL_Stfev1_MemdmaChangeSubstreamStatus(U32 MemDmaChannel, U32 TPMask, BOOL status)
{
	ST_ErrorCode_t Error = ST_NO_ERROR;
	U8 Count = 10;

	TPMask = TPMask;

	/* nothing to do while setting */
	if (status == TRUE)
		return ST_NO_ERROR;

	while (Count > 0) {
		if (STFE_BasePointers.DmaBlockBase_p != NULL) {
			if ((stptiSupport_ReadReg32(&(STFE_BasePointers.DmaBlockBase_p->STFEv1_DmaBlockDevice.IDLE_INT_STATUS))) & (1 << MemDmaChannel)) {
				/* Success */
				break;
			} else {
				Count--;
				udelay(1000);
			}
		} else {
			Count = 0;
			break;
		}
	}

	if (Count == 0) {
		Error = STPTI_ERROR_TSINPUT_HW_ACCESS_ERROR;
	}
	return Error;
}
/**
   @brief  STFE Hardware worker function

   Clear the number of DREQ requests in the queue for the selected input block

   @param  Index              Index to select input block
   @return                    none
 */
void stptiTSHAL_Stfev1_MemdmaClearDREQLevel(U32 Index, U32 BusSize)
{
	if (STFE_BasePointers.DmaBlockBase_p != NULL && (Index < STFE_MAX_MEMDMA_BLOCK_SIZE)) {
		stpti_printf("Index 0x%04x BusSize 0x%08x,  Previous level 0x%08x", (U32) Index, BusSize,
			     stptiSupport_ReadReg32((volatile U32 *)
						    &(STFE_BasePointers.DmaBlockBase_p->STFEv1_DmaBlockDevice.BUS_LEVEL[Index])));

		stptiSupport_ReadModifyWriteReg32((volatile U32 *)
						  &(STFE_BasePointers.DmaBlockBase_p->STFEv1_DmaBlockDevice.DMA_MODE),
						  (1 << Index), (1 << Index)
		    );

		stptiSupport_WriteReg32((volatile U32 *)
					&(STFE_BasePointers.DmaBlockBase_p->STFEv1_DmaBlockDevice.BUS_SIZE[Index]),
					BusSize);

		stptiSupport_WriteReg32((volatile U32 *)
					&(STFE_BasePointers.DmaBlockBase_p->STFEv1_DmaBlockDevice.BUS_LEVEL[Index]),
					0x00000000);

		/* Clear any overflow on this channel */
		stptiSupport_WriteReg32((volatile U32 *)
					&(STFE_BasePointers.DmaBlockBase_p->STFEv1_DmaBlockDevice.ERROR_INT_STATUS),
					0x00010000 << Index);

	}
}

void stptiTSHAL_Stfev1_MemDmaRegisterDump(U32 MemDmaIndex, BOOL Verbose, stptiSUPPORT_sprintf_t *ctx_p)
{
	U32 Tmp = stptiSupport_ReadReg32(&(STFE_BasePointers.DmaBlockBase_p->STFEv1_DmaBlockDevice.DATA_INT_STATUS));

	Verbose = Verbose;

	stptiSUPPORT_sprintf(ctx_p, FW_VERSION);
	stptiSUPPORT_sprintf(ctx_p, "-------------------------------------------------\n");

	stptiSUPPORT_sprintf(ctx_p, "DINT_STATUS  :  0x%08X\n", Tmp);
	stptiSUPPORT_sprintf(ctx_p, "DINT_STATUS  :  Data Int            : %u\n", (Tmp >> MemDmaIndex) & 0x01);
	stptiSUPPORT_sprintf(ctx_p, "-------------------------------------------------\n");

	Tmp = stptiSupport_ReadReg32(&(STFE_BasePointers.DmaBlockBase_p->STFEv1_DmaBlockDevice.DATA_INT_MASK));
	stptiSUPPORT_sprintf(ctx_p, "DINT_MASK    :  0x%08X\n", Tmp);
	stptiSUPPORT_sprintf(ctx_p, "DINT_MASK    :  Swap Mask           : %u\n", (Tmp >> MemDmaIndex) & 0x01);
	stptiSUPPORT_sprintf(ctx_p, "-------------------------------------------------\n");

	Tmp = stptiSupport_ReadReg32(&(STFE_BasePointers.DmaBlockBase_p->STFEv1_DmaBlockDevice.ERROR_INT_STATUS));
	stptiSUPPORT_sprintf(ctx_p, "EINT_STATUS  :  0x%08X\n", Tmp);
	stptiSUPPORT_sprintf(ctx_p, "EINT_STATUS  :  PtrError            : %u\n", (Tmp >> MemDmaIndex) & 0x01);
	stptiSUPPORT_sprintf(ctx_p, "EINT_STATUS  :  OverflowError       : %u\n", (Tmp >> (MemDmaIndex + 16)) & 0x01);
	stptiSUPPORT_sprintf(ctx_p, "-------------------------------------------------\n");

	Tmp = stptiSupport_ReadReg32(&(STFE_BasePointers.DmaBlockBase_p->STFEv1_DmaBlockDevice.ERROR_INT_MASK));
	stptiSUPPORT_sprintf(ctx_p, "EINT_MASK    :  0x%08X\n", Tmp);
	stptiSUPPORT_sprintf(ctx_p, "EINT_MASK    :  PtrMask             : %u\n", (Tmp >> MemDmaIndex) & 0x01);
	stptiSUPPORT_sprintf(ctx_p, "EINT_MASK    :  OverflowError       : %u\n", (Tmp >> (MemDmaIndex + 16)) & 0x01);
	stptiSUPPORT_sprintf(ctx_p, "-------------------------------------------------\n");

	Tmp = stptiSupport_ReadReg32(&(STFE_BasePointers.DmaBlockBase_p->STFEv1_DmaBlockDevice.ERROR_OVERRIDE));
	stptiSUPPORT_sprintf(ctx_p, "ERR_OVERRIDE :  0x%08X\n", Tmp);
	stptiSUPPORT_sprintf(ctx_p, "ERR_OVERRIDE :  OverRide            : %u\n", (Tmp >> (MemDmaIndex + 16)) & 0x01);
	stptiSUPPORT_sprintf(ctx_p, "-------------------------------------------------\n");

	Tmp = stptiSupport_ReadReg32(&(STFE_BasePointers.DmaBlockBase_p->STFEv1_DmaBlockDevice.IDLE_INT_STATUS));
	stptiSUPPORT_sprintf(ctx_p, "IDLE_INT_STAT:  0x%08X\n", Tmp);
	stptiSUPPORT_sprintf(ctx_p, "IDLE_INT_STAT:  IdleInt             : %u\n", (Tmp >> MemDmaIndex) & 0x01);
	stptiSUPPORT_sprintf(ctx_p, "-------------------------------------------------\n");

	Tmp = stptiSupport_ReadReg32(&(STFE_BasePointers.DmaBlockBase_p->STFEv1_DmaBlockDevice.IDLE_INT_MASK));
	stptiSUPPORT_sprintf(ctx_p, "IDLE_INT_MASK:  0x%08X\n", Tmp);
	stptiSUPPORT_sprintf(ctx_p, "IDLE_INT_MASK:  IdleMask            : %u\n", (Tmp >> MemDmaIndex) & 0x01);
	stptiSUPPORT_sprintf(ctx_p, "-------------------------------------------------\n");

	Tmp = stptiSupport_ReadReg32(&(STFE_BasePointers.DmaBlockBase_p->STFEv1_DmaBlockDevice.DMA_MODE));
	stptiSUPPORT_sprintf(ctx_p, "DMA_MODE     :  0x%08X\n", Tmp);
	stptiSUPPORT_sprintf(ctx_p, "DMA_MODE     :  Mode                : %u\n", (Tmp >> MemDmaIndex) & 0x01);
	stptiSUPPORT_sprintf(ctx_p, "-------------------------------------------------\n");

	stptiSUPPORT_sprintf(ctx_p, "PTR_BASE     :  0x%08X\n",
			     stptiSupport_ReadReg32(&(STFE_BasePointers.DmaBlockBase_p->STFEv1_DmaBlockDevice.
						     POINTER_BASE)));

	stptiSUPPORT_sprintf(ctx_p, "BUS_BASE     :  0x%08X\n",
			     stptiSupport_ReadReg32(&(STFE_BasePointers.DmaBlockBase_p->STFEv1_DmaBlockDevice.
						     BUS_BASE)));
	stptiSUPPORT_sprintf(ctx_p, "BUS_TOP      :  0x%08X\n",
			     stptiSupport_ReadReg32(&(STFE_BasePointers.DmaBlockBase_p->STFEv1_DmaBlockDevice.BUS_TOP)));
	stptiSUPPORT_sprintf(ctx_p, "BUS_WP       :  0x%08X\n",
			     stptiSupport_ReadReg32(&(STFE_BasePointers.DmaBlockBase_p->STFEv1_DmaBlockDevice.BUS_WP)));
	stptiSUPPORT_sprintf(ctx_p, "PKT_SIZE     :  %u\n",
			     stptiSupport_ReadReg32(&(STFE_BasePointers.DmaBlockBase_p->STFEv1_DmaBlockDevice.
						     PKT_SIZE)));
	stptiSUPPORT_sprintf(ctx_p, "BUS_NBASE    :  0x%08X\n",
			     stptiSupport_ReadReg32(&(STFE_BasePointers.DmaBlockBase_p->STFEv1_DmaBlockDevice.
						     BUS_NEXT_BASE)));
	stptiSUPPORT_sprintf(ctx_p, "BUS_NTOP     :  0x%08X\n",
			     stptiSupport_ReadReg32(&(STFE_BasePointers.DmaBlockBase_p->STFEv1_DmaBlockDevice.
						     BUS_NEXT_TOP)));
	stptiSUPPORT_sprintf(ctx_p, "MEM_BASE     :  0x%08X\n",
			     stptiSupport_ReadReg32(&(STFE_BasePointers.DmaBlockBase_p->STFEv1_DmaBlockDevice.
						     MEM_BASE)));
	stptiSUPPORT_sprintf(ctx_p, "MEM_TOP      :  0x%08X\n",
			     stptiSupport_ReadReg32(&(STFE_BasePointers.DmaBlockBase_p->STFEv1_DmaBlockDevice.MEM_TOP)));
	stptiSUPPORT_sprintf(ctx_p, "MEM_RP       :  0x%08X\n",
			     stptiSupport_ReadReg32(&(STFE_BasePointers.DmaBlockBase_p->STFEv1_DmaBlockDevice.MEM_RP)));

	stptiSUPPORT_sprintf(ctx_p, "DMA_BUS_SIZE :  0x%08X\n",
			     stptiSupport_ReadReg32(&(STFE_BasePointers.DmaBlockBase_p->STFEv1_DmaBlockDevice.
						     BUS_SIZE[MemDmaIndex])));
	stptiSUPPORT_sprintf(ctx_p, "DMA_BUS_LEV  :  0x%08X\n",
			     stptiSupport_ReadReg32(&(STFE_BasePointers.DmaBlockBase_p->STFEv1_DmaBlockDevice.
						     BUS_LEVEL[MemDmaIndex])));
	stptiSUPPORT_sprintf(ctx_p, "DMA_BUS_THRES:  0x%08X\n",
			     stptiSupport_ReadReg32(&(STFE_BasePointers.DmaBlockBase_p->STFEv1_DmaBlockDevice.
						     BUS_THRES[MemDmaIndex])));

}

void stptiTSHAL_Stfev1_DmaPTRRegisterDump(U32 MemDmaIndex, stptiSUPPORT_sprintf_t *ctx_p)
{
	stptiTSHAL_InputResource_t *TSInputHWMapping_p = stptiTSHAL_GetHWMapping();
	stptiHAL_STFEv1_InputNode_t *STFE_RAM_Mapped_InputNode_p =
	    (stptiHAL_STFEv1_InputNode_t *) TSInputHWMapping_p->StfeRegs.STFE_RAM_Ptr_MappedAddress_p;

	stptiSUPPORT_sprintf(ctx_p, "MEMDMA RAM POINTERS......\n");
	stptiSUPPORT_sprintf(ctx_p, "RAM_BUS_BASE :  0x%08X\n",
			     stptiSupport_ReadReg32(&(STFE_RAM_Mapped_InputNode_p[MemDmaIndex].BUS_BASE)));
	stptiSUPPORT_sprintf(ctx_p, "RAM_BUS_TOP  :  0x%08X\n",
			     stptiSupport_ReadReg32(&(STFE_RAM_Mapped_InputNode_p[MemDmaIndex].BUS_TOP)));
	stptiSUPPORT_sprintf(ctx_p, "RAM_BUS_WP   :  0x%08X\n",
			     stptiSupport_ReadReg32(&(STFE_RAM_Mapped_InputNode_p[MemDmaIndex].BUS_WP)));
	stptiSUPPORT_sprintf(ctx_p, "RAM_PKT_SIZE :  %u\n",
			     stptiSupport_ReadReg32(&(STFE_RAM_Mapped_InputNode_p[MemDmaIndex].TS_PKT_SIZE)));
	stptiSUPPORT_sprintf(ctx_p, "RAM_BUS_NBASE:  0x%08X\n",
			     stptiSupport_ReadReg32(&(STFE_RAM_Mapped_InputNode_p[MemDmaIndex].BUS_NEXT_BASE)));
	stptiSUPPORT_sprintf(ctx_p, "RAM_BUS_NTOP :  0x%08X\n",
			     stptiSupport_ReadReg32(&(STFE_RAM_Mapped_InputNode_p[MemDmaIndex].BUS_NEXT_TOP)));
	stptiSUPPORT_sprintf(ctx_p, "RAM_MEM_BASE :  0x%08X\n",
			     stptiSupport_ReadReg32(&(STFE_RAM_Mapped_InputNode_p[MemDmaIndex].MEM_BASE)));
	stptiSUPPORT_sprintf(ctx_p, "RAM_MEM_TOP  :  0x%08X\n",
			     stptiSupport_ReadReg32(&(STFE_RAM_Mapped_InputNode_p[MemDmaIndex].MEM_TOP)));
	stptiSUPPORT_sprintf(ctx_p, "-------------------------------------------------\n");
}

void stptiTSHAL_Stfev1_SystemRegisterDump(stptiSUPPORT_sprintf_t *ctx_p)
{
	stptiSUPPORT_sprintf(ctx_p, "----------- STFE System debug -------------------\n");
	stptiSUPPORT_sprintf(ctx_p, "STATUS       :  0x%08X\n",
			     stptiSupport_ReadReg32(&(STFE_BasePointers.SystemBlockBase_p->STFEv1_SystemBlockDevice.STATUS)));
	stptiSUPPORT_sprintf(ctx_p, "MASK         :  0x%08X\n",
			     stptiSupport_ReadReg32(&(STFE_BasePointers.SystemBlockBase_p->STFEv1_SystemBlockDevice.MASK)));
	stptiSUPPORT_sprintf(ctx_p, "DMA_ROUTE    :  0x%08X\n",
			     stptiSupport_ReadReg32(&(STFE_BasePointers.SystemBlockBase_p->STFEv1_SystemBlockDevice.DMA_ROUTE)));
	stptiSUPPORT_sprintf(ctx_p, "CLK_ENABLE   :  0x%08X\n",
			     stptiSupport_ReadReg32(&(STFE_BasePointers.SystemBlockBase_p->STFEv1_SystemBlockDevice.CLK_ENABLE)));
	stptiSUPPORT_sprintf(ctx_p, "-------------------------------------------------\n");

}

/**********************************************************
 *
 *  	CCSC REGISTERS
 *
 **********************************************************/
/**
   @brief  Stfe Hardware worker function

   Configure the CCSC for TSOUT 196 bytes

   @param                     none
   @return                    none
 */
void stptiTSHAL_Stfev1_CCSCConfig(void)
{
	if (STFE_Config.NumCCSCs) {
		stptiSupport_WriteReg32((volatile U32 *)
					&(STFE_BasePointers.CCSCBlockBase_p->CCSC_CLK_CTRL), 0x00000002);
		stptiSupport_WriteReg32((volatile U32 *)
					&(STFE_BasePointers.CCSCBlockBase_p->CCSC_CTRL), STFE_CCSC_ENABLE);
	}
}

/**
   @brief  Stfe Hardware worker function

   Configure the CCSC for bypass mode

   @param                     none
   @return                    none
 */
void stptiTSHAL_Stfev1_CCSCBypass(void)
{
	if (STFE_Config.NumCCSCs) {
		stptiSupport_WriteReg32((volatile U32 *)
					&(STFE_BasePointers.CCSCBlockBase_p->CCSC_CLK_CTRL), 0x00000002);
		stptiSupport_WriteReg32((volatile U32 *)
					&(STFE_BasePointers.CCSCBlockBase_p->CCSC_CTRL), STFE_CCSC_BYPASS);
	}
}

/**
   @brief  Function to monitor the DREQ levels into memdma

   This function  monitor the DREQ levels into memdma.

   @param  tp_read_pointers     Array containing the level to monitor (one for each channel).
   @param  num_rps    		The number of channel to monitor

   @return                    none
 */
void stptiTSHAL_Stfev1_MonitorDREQLevels(U32 *tp_read_pointers, U8 num_rps, U32 dma_pck_size)
{
	stptiTSHAL_InputResource_t *TSInputHWMapping_p = stptiTSHAL_GetHWMapping();
	stptiHAL_STFEv1_InputNode_t *STFE_RAM_Mapped_InputNode_p =
	    (stptiHAL_STFEv1_InputNode_t *) TSInputHWMapping_p->StfeRegs.STFE_RAM_Ptr_MappedAddress_p;
	U32 base_pointer, top_pointer, write_pointer, size, level;
	U32 bytes_in_ddr, num_pkts_in_ddr = 0;
	int i;
	static U32 sec_count = 0;
	static U32 last_level = 0;

	sec_count++;
	for (i = 0; i < (STFE_Config.NumIBs + STFE_Config.NumMIBs) && (i < num_rps); i++) {
		size =
		    stptiSupport_ReadReg32((volatile U32 *)
					   &(STFE_BasePointers.DmaBlockBase_p->STFEv1_DmaBlockDevice.BUS_SIZE[i]));
		base_pointer = STFE_MonitorPointers[i].BUS_BASE;
		top_pointer = STFE_MonitorPointers[i].BUS_TOP;
		level =
		    stptiSupport_ReadReg32((volatile U32 *)
					   &(STFE_BasePointers.DmaBlockBase_p->STFEv1_DmaBlockDevice.BUS_LEVEL[i]));
		write_pointer = stptiSupport_ReadReg32(&(STFE_RAM_Mapped_InputNode_p[i].BUS_WP));

		if ((tp_read_pointers[i] != 0) && write_pointer != tp_read_pointers[i]) {
			if (write_pointer > tp_read_pointers[i]) {
				bytes_in_ddr = write_pointer - tp_read_pointers[i];
				num_pkts_in_ddr = bytes_in_ddr / dma_pck_size;
				stpti_printf("###1 RP: 0x%x WP: 0x%x bytes_in_ddr %u num_pkts_in_ddr %u level %u",
					     tp_read_pointers[i], write_pointer, bytes_in_ddr, num_pkts_in_ddr, level);

			} else {
				bytes_in_ddr = ((write_pointer - base_pointer) + (top_pointer - tp_read_pointers[i]));
				num_pkts_in_ddr = bytes_in_ddr / dma_pck_size;
				stpti_printf
				    ("###2 RP: 0x%x WP: 0x%x BP: 0x%x TP: 0x%x bytes_in_ddr %u num_pkts_in_ddr %u level %u",
				     tp_read_pointers[i], write_pointer, base_pointer, top_pointer, bytes_in_ddr,
				     num_pkts_in_ddr, level);
			}

			if ((level < num_pkts_in_ddr) && (num_pkts_in_ddr > (size / 2))) {
				stptiSupport_WriteReg32((volatile U32 *)
							&(STFE_BasePointers.DmaBlockBase_p->STFEv1_DmaBlockDevice.BUS_LEVEL[i]), (num_pkts_in_ddr / 2));
				stpti_printf("DMAChannel %u DMA level corrected to %u pkts", i, (num_pkts_in_ddr / 2));

				stpti_printf("DMA level corrected IB %u! RP: 0x%x WP: 0x%x bytes_in_ddr %u num_pkts_in_ddr %u level %u new level %u at %u seconds",
				     i, tp_read_pointers[i], write_pointer, bytes_in_ddr, num_pkts_in_ddr,
				     (num_pkts_in_ddr / 2), level, sec_count);
				stpti_printf("Level re-read check %u",
					     stptiSupport_ReadReg32((volatile U32 *)
								    &(STFE_BasePointers.DmaBlockBase_p->STFEv1_DmaBlockDevice.BUS_LEVEL[i])));
				last_level = 0;
			} else if (last_level < num_pkts_in_ddr) {
				last_level = num_pkts_in_ddr;
				if (i < STFE_Config.NumIBs)
					stpti_printf("Level increasing to %u pkts PID filter setup 0x%x  IB %u  %u seconds",
					     last_level,
					     stptiSupport_ReadReg32(&(STFE_BasePointers.InputBlockBase_p[i].PID_SETUP)),
					     i, sec_count);
				else
					stpti_printf("Level increasing to %u pkts PID filter setup 0x%x  MIB %u  %u seconds",
					     last_level,
					     stptiSupport_ReadReg32(&(STFE_BasePointers.MergedInputBlockBase_p[i - STFE_Config.NumIBs].PID_SETUP)),
					     i, sec_count);
			}
		}
	}
}
