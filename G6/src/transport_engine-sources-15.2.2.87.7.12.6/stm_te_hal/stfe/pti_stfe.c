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
   @file   pti_stfe.c
   @brief  The stfe driver for STFE

   This file implements the functions for configuring the STFE hardware for
   the input of data. it is a common file, for specific STFE version function
   see the related file.
 */

#if 0
#define STPTI_PRINT
#endif

/* Includes ---------------------------------------------------------------- */
#include "linuxcommon.h"

/* Includes from API level */
#include "../pti_debug.h"
#include "../pti_hal_api.h"
#include "../pti_driver.h"

/* Includes from the HAL / ObjMan level */
#include "pti_tsinput.h"
#include "pti_stfe.h"

/* MACROS ------------------------------------------------------------------ */

/* Private Constants ------------------------------------------------------- */
#define STFE_MIB_FLUSH                              (0x00000004)

#define STFE_TSDMA_PACECFG_ENABLE                   (0x80000000)
#define STFE_TSDMA_PACECFG_AUTO                     (0x40000000)
#define STFE_TSDMA_PACECFG_TTS_FORMAT               (0x00000010)
#define STFE_TSDMA_PACECFG_CNT_MASK                 (0x0000000F)

/* General values for masking data */
#define STFE_SLD_MASK                               (0x0000FFFF)
#define STFE_TAG_ENABLE_MASK                        (0x00000001)
#define STFE_TAG_TTS_CONV_MASK                      (0x00000002)
#define STFE_TAG_HEADER_MASK                        (0xFFFF0000)
#define STFE_TAG_COUNTER_MASK                       (0x00000006)
#define STFE_SYSTEM_ENABLE                          (0x00000001)
#define STFE_PID_ENABLE                             (0x80000000)
#define STFE_SYSTEM_RESET                           (0x00000002)
#define STFE_PKT_SIZE_MASK                          (0x00000FFF)
#define STFE_TAG_STREAM_MASK                        (0x3F)

/*for unknow reason the TAG and the PID filter in the simulator are mapped in an incorrect address
 the problem is fixed mapping another area*/
#if defined(TANGOSIM) || defined(TANGOSIM2)
#define WORKAROUND_SIM_SASG2
#endif

#ifdef WORKAROUND_SIM_SASG2
static U8 *Mapped_second_area = 0;
#endif

/* Public Variables -------------------------------------------------------- */

stptiTSHAL_InputResource_t TSInputHWMapping;

/* @brief: this array contains the pointer to the different HW peripherals.
 * the only exception is for the TSDMA which is a variable containing two pointers
 * because in two different STFE version their offset change heavily and this trick
 * avoid splitting code.
 * it is initialized at startup because it depends on IP cut version */
stptiHAL_STFE_Device_t STFE_BasePointers = { NULL, NULL, NULL, NULL, NULL, NULL, NULL, {NULL, NULL}, NULL };

extern stptiTSHAL_HW_function_t const *stptiTSHAL_HW_function_stfe;

/**********************************************************
 *  	HW INIT
 **********************************************************/
/**
   @brief  This function is called at init time

   it initialize all the Global structures

   @param  	none
   @return      none
 */
ST_ErrorCode_t stptiTSHAL_StfeMapHW(U32 *pN_TPs)
{
	ST_ErrorCode_t Error = ST_NO_ERROR;
	U32 i = 0;
	BOOL errorMappingRam = FALSE;
	stptiTSHAL_InputResource_t *TSInputHWMapping_p = stptiTSHAL_GetHWMapping();
	stptiHAL_STFE_TSDmaBlockDevice_t *TSDmaBlockBase_p = NULL;

	Error = stptiTSHAL_HW_function_stfe->stptiTSHAL_StfeCreateConfig(
			&STFE_Config.RamConfig,
			pN_TPs);

	if (Error)
		return Error;

#if defined( TANGOFPGA )
	/* STFE is not supported on the FPGA */
	TSInputHWMapping_p->StfeRegs.STFE_MappedAddress_p = 0;

	for (i = 0; i < STFE_Config.RamConfig.NumRam; i++)
		TSInputHWMapping_p->StfeRegs.STFE_RAM_MappedAddress_p[i] = 0;

	TSInputHWMapping_p->StfeRegs.STFE_RAM_Ptr_MappedAddress_p = 0;
#else

#if defined(TANGOSIM) || defined(TANGOSIM2) || defined(CONFIG_STM_VIRTUAL_PLATFORM)
	/* This is a hack to make sure we can map the STFE_RAM on the ST200 simulator.
	   It seems the BSP doesn't map this region by default. */
	if (mmap_create((void *)STFE_Config.PhysicalAddress,
			mmap_pagesize(STFE_Config.MapSize),
			mmap_protect_rw, mmap_uncached, mmap_pagesize(STFE_Config.MapSize)) == NULL) {
		stpti_printf("Unable to map the STFE into the address space (uncached) %d",
			     mmap_pagesize(STFE_Config.MapSize));
	}

	for (i = 0; i < STFE_Config.RamConfig.NumRam; i++) {
		if (mmap_create((void *)
				((U32) STFE_Config.RAMPhysicalAddress +
				 (U32) STFE_Config.RamConfig.RAMAddr[i]),
				mmap_pagesize(STFE_Config.RamConfig.RAMSize[i]),
				mmap_protect_rw,
				mmap_uncached, mmap_pagesize(STFE_Config.RamConfig.RAMSize[i])) == NULL) {
			stpti_printf("Unable to map the STFE_RAM into the address space (uncached)");
		}
	}

	if (mmap_create((void *)STFE_Config.RamConfig.RAMPointerPhyAddr,
			mmap_pagesize(STFE_Config.RamConfig.RAMPtrSize),
			mmap_protect_rw, mmap_uncached, mmap_pagesize(STFE_Config.RamConfig.RAMPtrSize)) == NULL) {
		stpti_printf("Unable to map the STFE_RAM into the address space (uncached)");
	}
#endif

	TSInputHWMapping_p->StfeRegs.STFE_MappedAddress_p =
	    ioremap_nocache((unsigned long)STFE_Config.PhysicalAddress, STFE_Config.MapSize);
	stpti_printf("Mapped registers... [%p] => [%08x]", STFE_Config.PhysicalAddress,
		     (U32) TSInputHWMapping_p->StfeRegs.STFE_MappedAddress_p);

#ifdef WORKAROUND_SIM_SASG2
	Mapped_second_area = ioremap_nocache(STFE_Config.PhysicalAddress + STFE_Config.TagOffset, 0x8000);
#endif

	for (i = 0; i < STFE_Config.RamConfig.NumRam; i++) {
		U32 RAMPhysicalAddress = STFE_Config.RamConfig.RAMAddr[i] + (U32) STFE_Config.RAMPhysicalAddress;

		TSInputHWMapping_p->StfeRegs.STFE_RAM_MappedAddress_p[i] =
		    ioremap_nocache(RAMPhysicalAddress, STFE_Config.RamConfig.RAMSize[i]);

		stpti_printf("Mapped registers... [%08x] => [%08x]",
			     RAMPhysicalAddress, (U32) TSInputHWMapping_p->StfeRegs.STFE_RAM_MappedAddress_p[i]);

		if (TSInputHWMapping_p->StfeRegs.STFE_RAM_MappedAddress_p[i] == NULL)
			errorMappingRam = TRUE;
	}

	TSInputHWMapping_p->StfeRegs.STFE_RAM_Ptr_MappedAddress_p =
	    ioremap_nocache(STFE_Config.RamConfig.RAMPointerPhyAddr, STFE_Config.RamConfig.RAMPtrSize);

	stpti_printf("Mapped registers... [%08x] => [%08x]",
		     STFE_Config.RamConfig.RAMPointerPhyAddr,
		     (U32) TSInputHWMapping_p->StfeRegs.STFE_RAM_Ptr_MappedAddress_p);

	if ((TSInputHWMapping_p->StfeRegs.STFE_MappedAddress_p == NULL) || (errorMappingRam == TRUE)
	    || (TSInputHWMapping_p->StfeRegs.STFE_RAM_Ptr_MappedAddress_p == NULL)) {
		Error = ST_ERROR_NO_MEMORY;
		STPTI_PRINTF_ERROR("Unable to map the STFE into the address space (uncached) %d", STFE_Config.MapSize);
		STPTI_PRINTF_ERROR("Unable to map the STFE_RAM into the address space (uncached)");
	} else {
		/* Setup the base pointers to individual blocks */
		STFE_BasePointers.InputBlockBase_p =
		    (stptiHAL_STFE_InputBlockDevice_t *) (((U32) (TSInputHWMapping_p->StfeRegs.STFE_MappedAddress_p)) +
							  STFE_Config.IBOffset);
		STFE_BasePointers.MergedInputBlockBase_p =
		    (stptiHAL_STFE_MergedInputBlockDevice_t *) &STFE_BasePointers.InputBlockBase_p[STFE_Config.NumIBs];
		STFE_BasePointers.SWTSInputBlockBase_p =
		    (stptiHAL_STFE_SWTSInputBlockDevice_t *) &STFE_BasePointers.MergedInputBlockBase_p[STFE_Config.
													NumMIBs];
		STFE_BasePointers.SystemBlockBase_p =
		    (stptiHAL_STFE_SystemBlockDevice_t *) (((U32) (TSInputHWMapping_p->StfeRegs.STFE_MappedAddress_p)) +
							   STFE_Config.SystemRegsOffset);

#ifdef WORKAROUND_SIM_SASG2
		STFE_BasePointers.TagCounterBase_p = (stptiHAL_STFE_TagCounterDevice_t *) (U32) (Mapped_second_area);
		STFE_BasePointers.PidFilterBaseAddressPhys_p =
		    (stptiHAL_STFE_PidFilterDevice_t *) (((U32) (Mapped_second_area) - STFE_Config.TagOffset) +
							 STFE_Config.PidFilterOffset);
		STFE_BasePointers.CCSCBlockBase_p =
		    (stptiHAL_STFE_CCSCBlockDevice_t *) (((U32) (Mapped_second_area) - STFE_Config.TagOffset) +
							 STFE_Config.CCSCOffset);
		STFE_BasePointers.DmaBlockBase_p =
		    (stptiHAL_STFE_DmaBlockDevice_t *) (((U32) (Mapped_second_area) - STFE_Config.TagOffset) +
							STFE_Config.MemDMAOffset);
		TSDmaBlockBase_p =
		    (stptiHAL_STFE_TSDmaBlockDevice_t *) (((U32) (Mapped_second_area) - STFE_Config.TagOffset) +
							  STFE_Config.TSDMAOffset);
#else // normal condition
		STFE_BasePointers.TagCounterBase_p =
		    (stptiHAL_STFE_TagCounterDevice_t *) (((U32) (TSInputHWMapping_p->StfeRegs.STFE_MappedAddress_p)) +
							  STFE_Config.TagOffset);
		STFE_BasePointers.PidFilterBaseAddressPhys_p =
		    (stptiHAL_STFE_PidFilterDevice_t *) (((U32) (TSInputHWMapping_p->StfeRegs.STFE_MappedAddress_p)) +
							 STFE_Config.PidFilterOffset);
		STFE_BasePointers.CCSCBlockBase_p =
		    (stptiHAL_STFE_CCSCBlockDevice_t *) (((U32) (TSInputHWMapping_p->StfeRegs.STFE_MappedAddress_p)) +
							 STFE_Config.CCSCOffset);
		STFE_BasePointers.DmaBlockBase_p =
		    (stptiHAL_STFE_DmaBlockDevice_t *) (((U32) (TSInputHWMapping_p->StfeRegs.STFE_MappedAddress_p)) +
							STFE_Config.MemDMAOffset);
		TSDmaBlockBase_p =
		    (stptiHAL_STFE_TSDmaBlockDevice_t *) (((U32) (TSInputHWMapping_p->StfeRegs.STFE_MappedAddress_p)) +
							STFE_Config.TSDMAOffset);
#endif
		stptiTSHAL_HW_function_stfe->stptiTSHAL_StfeTsdmaSetBasePointer(TSDmaBlockBase_p);
	}
#endif
	stptiHAL_debug_register_STFE(STFE_Config.NumIBs, STFE_Config.NumSWTSs, STFE_Config.NumMIBs);

	return Error;
}

/**
 * @brief This function starts the STFE hardware
 *
 * It it called during initialisation and after resume from passive standby
 *
 * @return ST_NO_ERROR
 */
ST_ErrorCode_t stptiTSHAL_StfeStartHW(void) {

	stptiTSHAL_HW_function_stfe->stptiTSHAL_StfeMemdmaInit();

	stptiTSHAL_HW_function_stfe->stptiTSHAL_StfeCCSCBypass();

	stptiTSHAL_HW_function_stfe->stptiTSHAL_StfeSystemInit();

	if (STFE_Config.NumMIBs)
		stptiTSHAL_StfeEnable(0, stptiTSHAL_TSINPUT_STFE_MIB, FALSE);

	return ST_NO_ERROR;
}
/**
	@brief  This function is called to disable the IRQ.
		This function gets caleld when going into sleep and shutting down the system


	@param       none
	@return      none
 */

void stptiTSHAL_StfeIrqDisabled(void)
{
	if (STFE_Config.PowerState == stptiTSHAL_TSINPUT_SLEEPING)
		return;
	stptiTSHAL_HW_function_stfe->stptiTSHAL_StfeSystemTerm();
	stptiTSHAL_HW_function_stfe->stptiTSHAL_StfeMemdmaTerm();
}

/**
   @brief  This function is called at shutting down time

   it undoes what is done at init time

   @param  	none
   @return      none
 */
void stptiTSHAL_StfeUnmapHW(void) {
	U32 i = 0;
	stptiTSHAL_InputResource_t *TSInputHWMapping_p = stptiTSHAL_GetHWMapping();

	stptiHAL_debug_unregister_STFE();

	/* Unmap any mapped registers */

	if (TSInputHWMapping_p->StfeRegs.STFE_RAM_Ptr_MappedAddress_p != NULL) {
		iounmap(TSInputHWMapping_p->StfeRegs.STFE_RAM_Ptr_MappedAddress_p);
		TSInputHWMapping_p->StfeRegs.STFE_RAM_Ptr_MappedAddress_p = NULL;
	}

	for (i = 0; i < STFE_Config.RamConfig.NumRam; i++) {
		if (TSInputHWMapping_p->StfeRegs.STFE_RAM_MappedAddress_p[i] != NULL) {
			iounmap(TSInputHWMapping_p->StfeRegs.STFE_RAM_MappedAddress_p[i]);
			TSInputHWMapping_p->StfeRegs.STFE_RAM_MappedAddress_p[i] = NULL;
		}
	}

	if (TSInputHWMapping_p->StfeRegs.STFE_MappedAddress_p != NULL) {
		iounmap(TSInputHWMapping_p->StfeRegs.STFE_MappedAddress_p);
		TSInputHWMapping_p->StfeRegs.STFE_MappedAddress_p = NULL;
	}
#ifdef WORKAROUND_SIM_SASG2
	if (Mapped_second_area != NULL) {
		iounmap(Mapped_second_area);
		Mapped_second_area = NULL;
	}
#endif

	/* Clear the base pointers to individual blocks */
	STFE_BasePointers.InputBlockBase_p = NULL;
	STFE_BasePointers.MergedInputBlockBase_p = NULL;
	STFE_BasePointers.SWTSInputBlockBase_p = NULL;
	STFE_BasePointers.TagCounterBase_p = NULL;
	STFE_BasePointers.PidFilterBaseAddressPhys_p = NULL;
	STFE_BasePointers.SystemBlockBase_p = NULL;
	STFE_BasePointers.DmaBlockBase_p = NULL;
	STFE_BasePointers.TSDmaBlockBase.TSDMA_CHANNEL = NULL;
	STFE_BasePointers.TSDmaBlockBase.TSDMA_OUTPUT = NULL;
	STFE_BasePointers.CCSCBlockBase_p = NULL;
}

/**********************************************************
 * 	GENERIC SWTS IB MIB
 **********************************************************/
/**
   @brief  Hardware worker function

   This function reads the system register for the STFE block of interest
   and return if the block is active

   @param  Index              Index to select input block
   @param  Type               Type of STFE input block to access as the mapping is different
   @return                    TRUE is block is active, FALSE otherwise
 */
BOOL stptiTSHAL_StfeGetIBEnableStatus(U16 Index, stptiHAL_TSInputStfeType_t IBType) {
	U32 RetValue = 0;

	if (STFE_BasePointers.InputBlockBase_p != NULL && STFE_BasePointers.MergedInputBlockBase_p != NULL
	    && STFE_BasePointers.SWTSInputBlockBase_p != NULL) {
		stpti_printf("Index %d IBType %d", Index, IBType);

		switch (IBType) {
		case (stptiTSHAL_TSINPUT_STFE_IB):
			RetValue =
			    stptiSupport_ReadReg32((volatile U32 *)&(STFE_BasePointers.InputBlockBase_p[Index].SYSTEM));
			break;
		case (stptiTSHAL_TSINPUT_STFE_MIB):
			RetValue =
			    stptiSupport_ReadReg32((volatile U32 *)
						   &(STFE_BasePointers.MergedInputBlockBase_p[Index].SYSTEM));
			break;
		case (stptiTSHAL_TSINPUT_STFE_SWTS):
			RetValue =
			    stptiSupport_ReadReg32((volatile U32 *)
						   &(STFE_BasePointers.SWTSInputBlockBase_p[Index].SYSTEM));
			break;
		default:
			RetValue = 0;
			break;
		}
	}
	return (RetValue & STFE_SYSTEM_ENABLE);
}

/**
   @brief  Hardware worker function

   This function reads the status register for the STFE block of interest

   @param  Index              Index to select input block
   @param  Type               Type of STFE input block to access as the mapping is different
   @return                    Register value
 */
U32 stptiTSHAL_StfeReadStatusReg(U32 Index, stptiHAL_TSInputStfeType_t IBType) {
	U32 RetValue = 0;

	if (STFE_BasePointers.InputBlockBase_p != NULL && STFE_BasePointers.MergedInputBlockBase_p != NULL
	    && STFE_BasePointers.SWTSInputBlockBase_p != NULL && (Index < MAX_NUMBER_OF_TSINPUTS)) {
		stpti_printf("");

		switch (IBType) {
		case (stptiTSHAL_TSINPUT_STFE_IB):
			RetValue =
			    stptiSupport_ReadReg32((volatile U32 *)&(STFE_BasePointers.InputBlockBase_p[Index].STATUS));
			break;
		case (stptiTSHAL_TSINPUT_STFE_MIB):
			RetValue =
			    stptiSupport_ReadReg32((volatile U32 *)
						   &(STFE_BasePointers.MergedInputBlockBase_p[Index].STATUS));
			break;
		case (stptiTSHAL_TSINPUT_STFE_SWTS):
			RetValue =
			    stptiSupport_ReadReg32((volatile U32 *)
						   &(STFE_BasePointers.SWTSInputBlockBase_p[Index].STATUS));
			break;
		default:
			/* nothing to do */
			break;
		}
	}
	return (RetValue);
}

/**
   @brief  STFE Hardware worker function

   Enable the selected input block - will always reset the IB too

   @param  Index              Index to select input block
   @param  Type               Type of STFE input block to access as the mapping is different
   @param  Enable             if True enable the block
   @return                    none
 */
void stptiTSHAL_StfeEnable(U32 Index, stptiHAL_TSInputStfeType_t IBType, BOOL Enable) {
	if (STFE_BasePointers.InputBlockBase_p != NULL && STFE_BasePointers.MergedInputBlockBase_p != NULL
	    && STFE_BasePointers.SWTSInputBlockBase_p != NULL && (Index < MAX_NUMBER_OF_TSINPUTS)) {
		stpti_printf("Index 0x%04x Type 0x%02x Enable 0x%02x", Index, IBType, (U8) Enable);

		switch (IBType) {
		case (stptiTSHAL_TSINPUT_STFE_IB):
			stptiSupport_ReadReg32((volatile U32 *)&(STFE_BasePointers.InputBlockBase_p[Index].STATUS));
			stptiSupport_WriteReg32((volatile U32 *)&(STFE_BasePointers.InputBlockBase_p[Index].MASK),
						(Enable ? 0xFFFFFFFF : 0));
			stptiSupport_WriteReg32((volatile U32 *)&(STFE_BasePointers.InputBlockBase_p[Index].SYSTEM),
						((U32) Enable & STFE_SYSTEM_ENABLE) | (Enable ? STFE_SYSTEM_RESET : 0));
			break;
		case (stptiTSHAL_TSINPUT_STFE_MIB):
			stptiSupport_WriteReg32((volatile U32 *)
						&(STFE_BasePointers.MergedInputBlockBase_p[Index].SYSTEM),
						((U32) Enable & STFE_SYSTEM_ENABLE) | (Enable ? STFE_SYSTEM_RESET : 0));
			break;
		case (stptiTSHAL_TSINPUT_STFE_SWTS):
			stptiSupport_ReadReg32((volatile U32 *)&(STFE_BasePointers.SWTSInputBlockBase_p[Index].STATUS));
			stptiSupport_WriteReg32((volatile U32 *)&(STFE_BasePointers.SWTSInputBlockBase_p[Index].MASK),
						(Enable ? 0xFFFFFFFF : 0));
			stptiSupport_WriteReg32((volatile U32 *)&(STFE_BasePointers.SWTSInputBlockBase_p[Index].SYSTEM),
						((U32) Enable & STFE_SYSTEM_ENABLE) | (Enable ? STFE_SYSTEM_RESET : 0));
			break;
		default:
			/* nothing to do */
			break;
		}
	}
}

/**
   @brief  STFE Hardware worker function

   Configure the TAG bytes register for the selected input block

   @param  Index              Index to select input block
   @param  IBType             IB, SWTS or MIB?
   @param  Tagheader          Tag value 16 bits
   @param  InputTagged        Only for SWTS: it changes if it was previously tagged
   @return                    none
 */
void stptiTSHAL_StfeTAGBytesConfigure(U32 Index, stptiHAL_TSInputStfeType_t IBType, U16 TagHeader,
				      stptiTSHAL_TSInputTagging_t InputTagged) {
	InputTagged = InputTagged;	/* guard against compiler warning */

	if (STFE_BasePointers.InputBlockBase_p != NULL && STFE_BasePointers.MergedInputBlockBase_p != NULL
	    && STFE_BasePointers.SWTSInputBlockBase_p != NULL && (Index < MAX_NUMBER_OF_TSINPUTS)) {

		stpti_printf("Index 0x%04x IBType 0x%02x TagHeader 0x%04x", Index, IBType, TagHeader);

		switch (IBType) {
		case (stptiTSHAL_TSINPUT_STFE_IB):
			stptiSupport_ReadModifyWriteReg32((volatile U32 *)
							  &(STFE_BasePointers.InputBlockBase_p[Index].TAG_BYTES),
							  (STFE_TAG_ENABLE_MASK | STFE_TAG_HEADER_MASK),
							  (((InputTagged) ? 0 : STFE_TAG_ENABLE_MASK) |
							   ((U32) TagHeader << 16))
			    );
			break;

		case (stptiTSHAL_TSINPUT_STFE_MIB):
			/* When set the tag header do not configure the MASK otherwise the stream will be automatically enabled */
			stptiSupport_WriteReg32((volatile U32 *)
						&(STFE_BasePointers.MergedInputBlockBase_p[Index].STREAM_IDENT),
						((U32) TagHeader));

			break;
		case (stptiTSHAL_TSINPUT_STFE_SWTS):

			/* Tag bytes always need to be output by the SWTS block,
			   If the input is tagged with an STFE tag we just need to replace the tag ID so the system can identify the source of the data.
			   If the input is tagged with a TTS tag we need to add the tag when the TTS conversion is performed.
			   If the input is not tagged we need to just add a tag */

			if (stptiTSHAL_TSINPUT_TAGS_TTS == InputTagged) {
				stptiSupport_ReadModifyWriteReg32((volatile U32 *)
								  &(STFE_BasePointers.SWTSInputBlockBase_p[Index].
								    TAG_BYTE_ADD),
								  (STFE_TAG_TTS_CONV_MASK | STFE_TAG_HEADER_MASK),
								  (STFE_TAG_TTS_CONV_MASK | ((U32) TagHeader << 16))
				    );
			} else if (stptiTSHAL_TSINPUT_TAGS_STFE == InputTagged) {
				/* Input tagged so replace so that the data gets to the correct TP vDevice. i.e. the data may have been recorded with tags off a different IB */
				stptiSupport_WriteReg32((volatile U32 *)
							&(STFE_BasePointers.SWTSInputBlockBase_p[Index].
							  TAG_BYTE_REPLACE), STFE_TAG_HEADER_MASK);

				stptiSupport_ReadModifyWriteReg32((volatile U32 *)
								  &(STFE_BasePointers.SWTSInputBlockBase_p[Index].
								    TAG_BYTE_ADD), STFE_TAG_HEADER_MASK,
								  ((U32) TagHeader << 16)
				    );
			} else {
				/* Add Tag bytes when accepting untagged data streams */
				stptiSupport_ReadModifyWriteReg32((volatile U32 *)
								  &(STFE_BasePointers.SWTSInputBlockBase_p[Index].
								    TAG_BYTE_ADD),
								  (STFE_TAG_ENABLE_MASK | STFE_TAG_HEADER_MASK),
								  (((InputTagged) ? 0 : STFE_TAG_ENABLE_MASK) |
								   ((U32) TagHeader << 16))
				    );
			}
			break;
		default:
			InputTagged = InputTagged;	/* To avoid complier warning */
			break;
		}
	}
}

/**
   @brief  STFE Hardware worker function

   Configure the PID Filter for the selected input block

   @param  Index              Index to select input block
   @param  IBType             IB, SWTS or MIB?
   @param  PIDBitSize         Number of bits per PID
   @param  Offset             Number of bits into the packet for the lsb of the PID
   @param  Enable             True to enable filter
   @return                    none
 */
void stptiTSHAL_StfePIDFilterConfigure(U32 Index, stptiHAL_TSInputStfeType_t IBType, U8 PIDBitSize, U8 Offset,
				       BOOL Enable) {
	U32 EnableBit;

	if (STFE_BasePointers.InputBlockBase_p != NULL && STFE_BasePointers.MergedInputBlockBase_p != NULL
	    && (Index < MAX_NUMBER_OF_TSINPUTS)) {
		stpti_printf("Index 0x%04x PIDBitSize 0x%08x Offset 0x%02x Enable 0x%02x", (U32) Index, PIDBitSize,
			     Offset, (U8) Enable);

		if (PIDBitSize > 13) {
			PIDBitSize = 13;
		}

		switch (IBType) {
		case (stptiTSHAL_TSINPUT_STFE_IB):
			EnableBit = (Enable ? 0x80000000 : 0x00000000);
			stptiSupport_WriteReg32((volatile U32 *)&(STFE_BasePointers.InputBlockBase_p[Index].PID_SETUP), EnableBit	/* bit 31 to set enable */
						| ((U32) PIDBitSize << 6)								/* bits 6:11 to set the number of bits used for PID filtering */
						|(Offset & 0x3F)									/* bits 0:5 bit offset into stream to test for the PID */
			    );
			break;
		case (stptiTSHAL_TSINPUT_STFE_MIB):
			EnableBit = (Enable ? (0x00010000 << Index) : 0x00000000);
			stptiSupport_WriteReg32((volatile U32 *)&(STFE_BasePointers.MergedInputBlockBase_p[Index].PID_SETUP), EnableBit | ((U32) PIDBitSize << 6)	/* bits 6:11 to set the number of bits used for PID filtering */
						|(Offset & 0x3F)													/* bits 0:5 bit offset into stream to test for the PID */
			    );
			break;
		default:
			/* Nothing to do as this is not relevant for the SWTS STFE block */
			break;
		}
	}
}

/**
   @brief  STFE Hardware worker function

   Enable/Disable the PID Filter for the selected input block

   @param  Index              Index to select input block
   @param  IBType             IB, SWTS or MIB?
   @param  Enable             True to enable filter
   @return                    none
 */
void stptiTSHAL_StfePIDFilterEnable(U32 Index, stptiHAL_TSInputStfeType_t IBType, BOOL Enable) {
	U32 EnableMask, EnableBit = 0;

	if (STFE_BasePointers.InputBlockBase_p != NULL && STFE_BasePointers.MergedInputBlockBase_p != NULL
	    && (Index < MAX_NUMBER_OF_TSINPUTS)) {
		switch (IBType) {
		case (stptiTSHAL_TSINPUT_STFE_IB):
			EnableMask = 0x80000000;
			if (Enable)
				EnableBit = EnableMask;
			stptiSupport_ReadModifyWriteReg32((volatile U32 *)
							  &(STFE_BasePointers.InputBlockBase_p[Index].PID_SETUP),
							  EnableMask, EnableBit);
			break;
		case (stptiTSHAL_TSINPUT_STFE_MIB):
			EnableMask = 0x00010000 << Index;
			if (Enable)
				EnableBit = EnableMask;
			stptiSupport_ReadModifyWriteReg32((volatile U32 *)
							  &(STFE_BasePointers.MergedInputBlockBase_p[Index].PID_SETUP),
							  EnableMask, EnableBit);
			break;
		default:
			/* Nothing to do as this is not relevant for the SWTS STFE block */
			break;
		}
	}
}

/**
   @brief  STFE Hardware worker function

   Configure the Tag Counter selected input block

   @param  Index              Index to select input block
   @param  TagCounter         Counter number
   @return                    none
 */
void stptiTSHAL_StfeTAGCounterSelect(U32 Index, U8 TagCounter) {
	if (STFE_BasePointers.InputBlockBase_p != NULL && (Index < MAX_NUMBER_OF_TSINPUTS)) {
		stpti_printf("Index 0x%04x TagCounter 0x%02x", Index, TagCounter);

		stptiSupport_ReadModifyWriteReg32((volatile U32 *)
						  &(STFE_BasePointers.InputBlockBase_p[Index].TAG_BYTES),
						  STFE_TAG_COUNTER_MASK, ((U32) TagCounter << 1)
		    );
	}
}

/**
   @brief  STFE Hardware worker function

   Set the Sync Lock Drop params for the selected input block

   @param  Index              Index to select input block
   @param  IBType             IB, SWTS or MIB?
   @param  Lock               Number of TS packet to lock to the streamSTFE_RAM_Mapped_InputNode_p
   @param  Drop               Number of TS packet to lose to unlock
   @param  Token              Sync token
   @return                    none
 */
void stptiTSHAL_StfeWriteSLD(U32 Index, stptiHAL_TSInputStfeType_t IBType, U8 Lock, U8 Drop, U8 Token) {
	if (STFE_BasePointers.InputBlockBase_p != NULL && STFE_BasePointers.SWTSInputBlockBase_p != NULL
	    && (Index < MAX_NUMBER_OF_TSINPUTS)) {

		stpti_printf("Index 0x%04x Type 0x%02x Lock 0x%02x Drop 0x%02x Token 0x%02x", (U32) Index, IBType, Lock,
			     Drop, Token);

		switch (IBType) {
		case (stptiTSHAL_TSINPUT_STFE_IB):
			stptiSupport_ReadModifyWriteReg32((volatile U32 *)
							  &(STFE_BasePointers.InputBlockBase_p[Index].SLAD_CONFIG),
							  STFE_SLD_MASK,
							  (U32) (Lock & 0x0F) | ((U32) (Drop & 0x0F) << 4) |
							  ((U32) (Token & 0xFF) << 8)
			    );
			break;
		case (stptiTSHAL_TSINPUT_STFE_SWTS):
			stptiSupport_ReadModifyWriteReg32((volatile U32 *)
							  &(STFE_BasePointers.SWTSInputBlockBase_p[Index].SLAD_CONFIG),
							  STFE_SLD_MASK,
							  (U32) (Lock & 0x0F) | ((U32) (Drop & 0x0F) << 4) |
							  ((U32) (Token & 0xFF) << 8)
			    );
			break;
		default:
			break;
		}
	}
}

/**
   @brief  STFE Hardware worker function

   Set input format register for the selected input block

   @param  Index              Index to select input block
   @param  IBType             IB or MIB?
   @param  Format             Format Value
   @return                    none
 */
void stptiTSHAL_StfeWriteInputFormat(U32 Index, stptiHAL_TSInputStfeType_t IBType, U32 Format) {
	if (STFE_BasePointers.InputBlockBase_p != NULL && STFE_BasePointers.MergedInputBlockBase_p != NULL
	    && (Index < MAX_NUMBER_OF_TSINPUTS)) {
		stpti_printf("Index 0x%04x Type 0x%02x Format 0x%08x", (U32) Index, IBType, Format);

		switch (IBType) {
		case (stptiTSHAL_TSINPUT_STFE_IB):
			stptiSupport_WriteReg32((volatile U32 *)
						&(STFE_BasePointers.InputBlockBase_p[Index].INPUT_FORMAT_CONFIG),
						Format);
			break;
		case (stptiTSHAL_TSINPUT_STFE_MIB):
			stptiSupport_WriteReg32((volatile U32 *)
						&(STFE_BasePointers.MergedInputBlockBase_p[Index].INPUT_FORMAT_CONFIG),
						Format);
			break;
		default:
			/* nothing to do */
			break;
		}
	}
}

/**
   @brief  STFE Hardware worker function

   Set input packet length for the selected input block

   @param  Index              Index to select input block
   @param  IBType             IB, SWTS or MIB?
   @param  Length             Packet length in bytes
   @return                    none
 */
void stptiTSHAL_StfeWritePktLength(U32 Index, stptiHAL_TSInputStfeType_t IBType, U32 Length) {
	if (STFE_BasePointers.InputBlockBase_p != NULL && STFE_BasePointers.MergedInputBlockBase_p != NULL
	    && STFE_BasePointers.SWTSInputBlockBase_p != NULL && (Index < MAX_NUMBER_OF_TSINPUTS)) {
		stpti_printf("Index 0x%04x IBType 0x%02x Length 0x%08x", Index, IBType, Length);

		switch (IBType) {
		case (stptiTSHAL_TSINPUT_STFE_IB):
			stptiSupport_WriteReg32((volatile U32 *)
						&(STFE_BasePointers.InputBlockBase_p[Index].PACKET_LENGTH),
						Length & STFE_PKT_SIZE_MASK);
			break;
		case (stptiTSHAL_TSINPUT_STFE_MIB):
			stptiSupport_WriteReg32((volatile U32 *)
						&(STFE_BasePointers.MergedInputBlockBase_p[Index].PACKET_LENGTH),
						Length & STFE_PKT_SIZE_MASK);
			break;
		case (stptiTSHAL_TSINPUT_STFE_SWTS):
			stptiSupport_WriteReg32((volatile U32 *)
						&(STFE_BasePointers.SWTSInputBlockBase_p[Index].PACKET_LENGTH),
						Length & STFE_PKT_SIZE_MASK);
			break;
		default:
			break;
		}
	}
}

/**********************************************************
 *
 *  	RAM MANAGEMENT
 *
 **********************************************************/
/**
   @brief  STFE Hardware worker function

   Configure pointers to internal RAM for the selected input block -
   This function is aware of the internal RAM organization.
   There are a defined amount of space for each block in the internal ram dedicated for this purpose
   Mind: there are different Internal RAM, each can host at max 16 blocks
   Index is relative to the object i.e. SWTS1 --- index = 1 and IBtype = stptiTSHAL_TSINPUT_STFE_SWTS
   For Mib Write and Read pointers have to be reset too. Write pointers is reset by setting
   BUFFER_START to 0xffffffff

   @param  Index              Index to select input block
   @param  IBType             IB, SWTS or MIB?
   @return                    none
 */
void stptiTSHAL_StfeWriteToIBInternalRAMPtrs(U32 Index, stptiHAL_TSInputStfeType_t IBType) {
	U32 IndexInRam = 0;
	U32 RamToBeUsed = stptiTSHAL_HW_function_stfe->stptiTSHAL_StfeMemdma_ram2beUsed(Index, IBType, &IndexInRam);
	U32 StartAddressOfFifos = STFE_Config.RamConfig.RAMAddr[RamToBeUsed];
	U32 FifoSize = STFE_Config.RamConfig.FifoSize[RamToBeUsed];
	U32 InternalBase = StartAddressOfFifos + (IndexInRam * FifoSize);

	if ((InternalBase + FifoSize - 1) < (STFE_Config.RamConfig.RAMSize[RamToBeUsed] + StartAddressOfFifos)) {
		/* Configure the IB pointers to internal RAM */
		switch (IBType) {
		case (stptiTSHAL_TSINPUT_STFE_IB):
			stptiSupport_WriteReg32((volatile U32 *)
						&(STFE_BasePointers.InputBlockBase_p[Index].BUFFER_START),
						InternalBase);
			stptiSupport_WriteReg32((volatile U32 *)&(STFE_BasePointers.InputBlockBase_p[Index].BUFFER_END),
						InternalBase + FifoSize - 1);
			break;
		case (stptiTSHAL_TSINPUT_STFE_MIB):
			stptiSupport_WriteReg32((volatile U32 *)
						&(STFE_BasePointers.MergedInputBlockBase_p[Index].BUFFER_START),
						0xffffffff);
			stptiSupport_WriteReg32((volatile U32 *)
						&(STFE_BasePointers.MergedInputBlockBase_p[Index].BUFFER_START),
						InternalBase);
			stptiSupport_WriteReg32((volatile U32 *)
						&(STFE_BasePointers.MergedInputBlockBase_p[Index].READ_POINTER),
						InternalBase);
			stptiSupport_WriteReg32((volatile U32 *)
						&(STFE_BasePointers.MergedInputBlockBase_p[Index].BUFFER_END),
						InternalBase + FifoSize - 1);
			break;
		case (stptiTSHAL_TSINPUT_STFE_SWTS):
			stptiSupport_WriteReg32((volatile U32 *)
						&(STFE_BasePointers.SWTSInputBlockBase_p[Index].BUFFER_START),
						InternalBase);
			stptiSupport_WriteReg32((volatile U32 *)
						&(STFE_BasePointers.SWTSInputBlockBase_p[Index].BUFFER_END),
						InternalBase + FifoSize - 1);
			break;
		default:
			break;
		}
	}
	stpti_printf("Index 0x%04x Block Type %d Buffer Start 0x%08x Buffer End 0x%08x", Index, IBType, InternalBase,
		     InternalBase + FifoSize - 1);
}

/**********************************************************
 *
 *  	TAG REGISTERS
 *
 **********************************************************/
/**
   @brief  Returns the Timer value for this Input

   This function looks up which timer is associated to this input.

   @param  Index              Index to select input block
   @param  IBType             IB, SWTS or MIB?
   @param  TimeValue_p         Pointer to time information
   @param  IsTSIN_p            If IB block = TRUE (it means that the inout is timestamped)
   @return                     none
 */
ST_ErrorCode_t stptiTSHAL_StfeGetHWTimer(U32 IBBlockIndex, stptiHAL_TSInputStfeType_t IBType,
					 stptiTSHAL_TimerValue_t * TimeValue_p, BOOL * IsTSIN_p) {
	ST_ErrorCode_t Error = ST_NO_ERROR;
	struct timespec ts;
	ktime_t	ktime;

#if defined( TANGOFPGA )

	if (TimeValue_p != NULL) {
		/* Output the value for tag insertion */
		TimeValue_p->TagLSWord = 0;
		TimeValue_p->TagMSWord = 0;
		getrawmonotonic(&ts);
		ktime = timespec_to_ktime(ts);
		TimeValue_p->SystemTime = ktime_to_us(ktime);

		/* Output the timer in a more usual format */
		TimeValue_p->Clk27MHzDiv300Bit32 = 0;
		TimeValue_p->Clk27MHzDiv300Bit31to0 = 0;
		TimeValue_p->Clk27MHzModulus300 = 0;
	}

	/* Return back if this is a TSIN, which can be useful to know if this input is going to (or has been) timestamped */
	if (IsTSIN_p != NULL) {
		*IsTSIN_p = FALSE;
	}
#else
	U32 TagWord0, TagWord1;
	unsigned long Flags;
	DEFINE_SPINLOCK(LocalLock);

	if (STFE_BasePointers.InputBlockBase_p != NULL && STFE_BasePointers.TagCounterBase_p != NULL &&
			TimeValue_p != NULL) {
		/* Disable interrupts to avoid deschedule between reading
		 * device time and system time */
		spin_lock_irqsave(&LocalLock, Flags);

		if (IBType == stptiTSHAL_TSINPUT_STFE_IB) {
			U32 Tmp = stptiSupport_ReadReg32(&(STFE_BasePointers.InputBlockBase_p[IBBlockIndex].TAG_BYTES));
			U32 Counter = ((Tmp >> 1) & 0x03);
			TagWord0 = stptiSupport_ReadReg32(&(STFE_BasePointers.TagCounterBase_p[Counter].COUNTER_LSW));
			TagWord1 = stptiSupport_ReadReg32(&(STFE_BasePointers.TagCounterBase_p[Counter].COUNTER_MSW));
		} else {
			/* Return the default value */
			TagWord0 = stptiSupport_ReadReg32(&(STFE_BasePointers.TagCounterBase_p[1].COUNTER_LSW));
			TagWord1 = stptiSupport_ReadReg32(&(STFE_BasePointers.TagCounterBase_p[1].COUNTER_MSW));
		}

		if (TimeValue_p != NULL) {
			getrawmonotonic(&ts);
			ktime = timespec_to_ktime(ts);
			TimeValue_p->SystemTime = ktime_to_us(ktime);
		}
		spin_unlock_irqrestore(&LocalLock, Flags);

		/* Output the value for tag insertion */
		TimeValue_p->TagLSWord = TagWord0;
		TimeValue_p->TagMSWord = TagWord1;

		/* Output the timer in a more usual format */
		TimeValue_p->Clk27MHzDiv300Bit32 = (TagWord1 & 0x200) >> 9;
		TimeValue_p->Clk27MHzDiv300Bit31to0 = ((TagWord1 & 0x1FF) << 23) | (TagWord0 >> 9);
		TimeValue_p->Clk27MHzModulus300 = (U16) (TagWord0 & 0x1FF);
	}

	/* Return back if this is a TSIN, which can be useful to know if this
	 * input is going to (or has been) timestamped */
	if (IBType == stptiTSHAL_TSINPUT_STFE_IB) {
		if (IsTSIN_p != NULL) {
			*IsTSIN_p = TRUE;
		}
	} else {
		if (IsTSIN_p != NULL) {
			*IsTSIN_p = FALSE;
		}
	}
#endif
	return (Error);
}

/**********************************************************
 *
 *  	MEMDMA REGISTERS
 *
 **********************************************************/
// TAKE from different files, being the cell different
/**********************************************************
 *
 *  	PID FILTER REGISTERS
 *
 **********************************************************/
/**
   @brief  STFE Hardware worker function

   Configure the PID RAM base address for the STFE block

   @param  Index              Index to select input block
   @param  Address            Physical memory Address
   @return                    None
 */
void stptiTSHAL_StfeWritePIDBase(U32 Index, U32 Address) {
	if (STFE_BasePointers.PidFilterBaseAddressPhys_p != NULL && (Index < MAX_NUMBER_OF_TSINPUTS)) {
		stptiSupport_WriteReg32(&(STFE_BasePointers.PidFilterBaseAddressPhys_p->PIDF_BASE[Index]), Address);

		if (!STFE_Config.SoftwareLeakyPID) {
			stptiTSHAL_HW_function_stfe->stptiTSHAL_StfeLeakCountReset();
		} else {
			STPTI_PRINTF("Using Software Leaky PID\n");
		}

		stpti_printf("Index 0x%04x Address 0x%08x", Index, Address);
	}
}

/**********************************************************
 *
 *  	TSDMA REGISTERS
 *
 **********************************************************/
/**
   @brief  STFE Hardware worker function

   Flush the selected MEMDMA substream - retries a few times while waiting to complete.

   @param  Index              Index to select input block
   @return                    Error
 */
ST_ErrorCode_t stptiTSHAL_StfeTsdmaFlushSubstream(U32 IBIndex, U32 TSOutputIndex) {
	ST_ErrorCode_t Error = ST_NO_ERROR;
	U8 Count = 10;

	/* Wait for the Input Idle on TSDMA */
	while (Count > 0) {
		if (!(stptiTSHAL_StfeTsdmaReadStreamStatus(IBIndex) & 0x01)) {
			Count--;
			udelay(1000);
		} else {
			/* Success - TSDMA idle on this channel */
			break;
		}
	}

	if (Count > 0) {
		/* Disable the Input on the TSDMA */
		stptiTSHAL_HW_function_stfe->stptiTSHAL_StfeTsdmaEnableInputRoute(IBIndex, TSOutputIndex, FALSE);
	} else {
		Error = STPTI_ERROR_TSINPUT_HW_ACCESS_ERROR;
	}
	return Error;
}

/**
   @brief  TSDMA Hardware worker function

   Read the idle bit/status register of the selected TSDMA input

   Remember the index supplied in this channel may be different to the value used for the MEMDMA

   @param  Index              Selected channel
   @return                    Register value
 */
U32 stptiTSHAL_StfeTsdmaReadStreamStatus(U32 Index) {
	if (STFE_BasePointers.TSDmaBlockBase.TSDMA_CHANNEL != NULL && (Index < STFE_MAX_TSDMA_BLOCK_SIZE)) {
		stpti_printf("");

		return (stptiSupport_ReadReg32
			((volatile U32 *)&(STFE_BasePointers.TSDmaBlockBase.TSDMA_CHANNEL[Index].STREAM_STATUS)));
	} else {
		return (0);
	}
}

/**
   @brief  STFE TSDMA Hardware worker function

   Configure pointers to internal RAM for the selected input block - this function is aware of Internal RAM organisation

   @param  Index              Index to select input block -
   @return                    none
 */

void stptiTSHAL_StfeTsdmaConfigureInternalRAM(U32 Index, stptiHAL_TSInputStfeType_t IBType) {
	U32 IndexInRam = 0;
	U32 RamToBeUsed = stptiTSHAL_HW_function_stfe->stptiTSHAL_StfeMemdma_ram2beUsed(Index, IBType, &IndexInRam);
	U32 StartAddressOfFifos = STFE_Config.RamConfig.RAMAddr[RamToBeUsed];
	U32 FifoSize = STFE_Config.RamConfig.FifoSize[RamToBeUsed];
	U32 InternalBase = StartAddressOfFifos + (IndexInRam * FifoSize);

	if (STFE_BasePointers.TSDmaBlockBase.TSDMA_CHANNEL != NULL && (Index < STFE_MAX_TSDMA_BLOCK_SIZE) &&
	    ((InternalBase + FifoSize - 1) < (STFE_Config.RamConfig.RAMSize[RamToBeUsed] + StartAddressOfFifos))) {
		/* Configure the IB pointers to internal RAM */
		switch (IBType) {
		case (stptiTSHAL_TSINPUT_STFE_IB):
			stptiSupport_WriteReg32((volatile U32 *)
						&(STFE_BasePointers.TSDmaBlockBase.TSDMA_CHANNEL[Index].STREAM_BASE),
						InternalBase);
			stptiSupport_WriteReg32((volatile U32 *)
						&(STFE_BasePointers.TSDmaBlockBase.TSDMA_CHANNEL[Index].STREAM_TOP),
						InternalBase + FifoSize - 1);
			break;
		case (stptiTSHAL_TSINPUT_STFE_SWTS):
			stptiSupport_WriteReg32((volatile U32 *)
						&(STFE_BasePointers.TSDmaBlockBase.
						  TSDMA_CHANNEL[Index + STFE_Config.NumIBs].STREAM_BASE), InternalBase);
			stptiSupport_WriteReg32((volatile U32 *)
						&(STFE_BasePointers.TSDmaBlockBase.
						  TSDMA_CHANNEL[Index + STFE_Config.NumIBs].STREAM_TOP),
						InternalBase + FifoSize - 1);
			break;
		default:
			break;
		}
	}
	stpti_printf("Index 0x%04x Buffer Start 0x%08x Buffer End 0x%08x", Index, InternalBase,
		     InternalBase + FifoSize - 1);
}

/**
   @brief  STFE TSDMA Hardware worker function

   Configure input packet size to TSDMA - expects a StreamId index

   @param  Index              Index to select .
   @param  PktSize            PktSize including tag bytes but no stuffing bytes.
   @return                    none
 */

void stptiTSHAL_StfeTsdmaConfigurePktSize(U32 Index, U32 PktSize) {
	if (STFE_BasePointers.TSDmaBlockBase.TSDMA_CHANNEL != NULL && (Index < STFE_MAX_TSDMA_BLOCK_SIZE)) {
		stptiSupport_WriteReg32((volatile U32 *)
					&(STFE_BasePointers.TSDmaBlockBase.TSDMA_CHANNEL[Index].STREAM_PKT_SIZE),
					PktSize);
	}
}

/**
   @brief  STFE TSDMA Hardware worker function

   Configure tagging to TSDMA - expects a StreamId index

   @param  Index              Index select input block or SWTS
   @param  Tagged             if TRUE the input is tagged
   @return                    none
 */
void stptiTSHAL_StfeTsdmaConfigureStreamFormat(U32 Index, BOOL Tagged) {
	if (STFE_BasePointers.TSDmaBlockBase.TSDMA_CHANNEL != NULL && (Index < STFE_MAX_TSDMA_BLOCK_SIZE)) {
		stpti_printf("Address 0x%x Index %d Tagged %d",
			     (U32) & (STFE_BasePointers.TSDmaBlockBase.TSDMA_CHANNEL[Index].STREAM_FORMAT), Index,
			     Tagged);

		stptiSupport_WriteReg32((volatile U32 *)
					&(STFE_BasePointers.TSDmaBlockBase.TSDMA_CHANNEL[Index].STREAM_FORMAT),
					((Tagged == TRUE) ? 0x00000001 : 0x00000000));
	}
}

/**
   @brief  STFE TSDMA Hardware worker function

   Configure Out Channel to TSDMA - Currently only one channel but will be more on future devices.

   @param  TSInputHWMapping_p Pointer to mapped registers
   @param  Data               U32 value to write to register
   @return                    none
 */
void stptiTSHAL_StfeTsdmaConfigurePacing(U32 IBBlockIndex, U32 OutputPacingRate, U32 OutputPacingClkSrc,
					 BOOL InputTagging, BOOL SetPace) {
	if (STFE_BasePointers.TSDmaBlockBase.TSDMA_CHANNEL != NULL && (IBBlockIndex < STFE_MAX_TSDMA_BLOCK_SIZE)
	    && (OutputPacingClkSrc < 2)) {
		U32 TTS = 0;
		U32 ENABLE = 0;

		stpti_printf("IBBlockIndex %u OutputPacingRate %u OutputPacingClkSrc %u", IBBlockIndex,
			     OutputPacingRate, OutputPacingClkSrc);

		if (InputTagging) {
			TTS = STFE_TSDMA_PACECFG_TTS_FORMAT;
		}
		if ((OutputPacingRate < 0x100) && SetPace) {
			ENABLE = STFE_TSDMA_PACECFG_ENABLE | STFE_TSDMA_PACECFG_AUTO;
		}

		stptiSupport_WriteReg32((volatile U32 *)
					&(STFE_BasePointers.TSDmaBlockBase.TSDMA_CHANNEL[IBBlockIndex].STREAM_PACE_CFG),
					((OutputPacingRate << 8) & 0xFF00) | ENABLE | TTS | OutputPacingClkSrc);
	}
}

/**********************************************************
 *
 *  	MIB REGISTERS
 *
 **********************************************************/
/**
   @brief  STFE Hardware worker function

   Enable/Disable (and Flush) the selected MIB substream - For disabling retries a few times while waiting to complete.

   @param  Index              Index to select input block
   @return                    Error
 */
ST_ErrorCode_t stptiTSHAL_StfeMIBEnableSubstream(U16 Index, BOOL SetClear) {
	ST_ErrorCode_t Error = ST_NO_ERROR;
	U8 Count = 10;

	stpti_printf("Index 0x%04x", Index);

	if (STFE_BasePointers.MergedInputBlockBase_p != NULL) {
		stptiSupport_ReadReg32((volatile U32 *)&(STFE_BasePointers.MergedInputBlockBase_p[Index].STATUS));
		stptiSupport_WriteReg32((volatile U32 *)&(STFE_BasePointers.MergedInputBlockBase_p[Index].MASK),
					(SetClear ? 0xFFFFFFFF : 0));

		if (SetClear == TRUE) {
			stptiSupport_ReadModifyWriteReg32((volatile U32 *)
							  &(STFE_BasePointers.MergedInputBlockBase_p[Index].
							    STREAM_IDENT), 0xFFFF0000, 0xFFFF0000);
		} else {
			stptiSupport_ReadModifyWriteReg32((volatile U32 *)
							  &(STFE_BasePointers.MergedInputBlockBase_p[Index].
							    STREAM_IDENT), 0xFFFF0000, 0x00000000);

			stptiSupport_ReadModifyWriteReg32((volatile U32 *)
							  &(STFE_BasePointers.MergedInputBlockBase_p[Index].SYSTEM),
							  STFE_MIB_FLUSH, STFE_MIB_FLUSH);

			/* Wait for the bit to be reset */

			while (Count > 0) {
				if (stptiSupport_ReadReg32((volatile U32 *)&(STFE_BasePointers.MergedInputBlockBase_p[Index].SYSTEM)) & STFE_MIB_FLUSH) {
					Count--;
					udelay(1000);
				} else {
					/* Success */
					break;
				}
			}
			if (Count == 0) {
				Error = STPTI_ERROR_TSINPUT_HW_ACCESS_ERROR;
			}
		}
	}
	return Error;
}

/**
   @brief  STFE Hardware worker function

   Read the TAG mask register for the selected MIB substream

   @param  TSInputHWMapping_p Pointer to mapped registers
   @return                    mask
 */
BOOL stptiTSHAL_StfeMIBReadSubstreamStatus(U16 Index) {
	U32 Mask = 0;
	if (STFE_BasePointers.MergedInputBlockBase_p != NULL) {
		stpti_printf("Index 0x%04x", Index);

		Mask =
		    (stptiSupport_ReadReg32
		     ((volatile U32 *)&(STFE_BasePointers.MergedInputBlockBase_p[Index].STREAM_IDENT)) >> 16);
	}
	return ((Mask != 0) ? (TRUE) : (FALSE));
}

/**********************************************************
 *
 *
 *   FUNCTION TO DUMP REGISTRY CONTENTE
 *
 *
 **********************************************************/
void stptiTSHAL_StfeIBRegisterDump(U32 IBBlockIndex, stptiSUPPORT_sprintf_t *ctx_p) {
	U32 Tmp;
	/* If this is a STFE Input Block */
	Tmp = stptiSupport_ReadReg32(&(STFE_BasePointers.InputBlockBase_p[IBBlockIndex].INPUT_FORMAT_CONFIG));

	stptiSUPPORT_sprintf(ctx_p, "----------- STFE Input Block %u debug -----------\n", (U8) IBBlockIndex);
	stptiSUPPORT_sprintf(ctx_p, "BASE_ADDRESS :  0x%08X\n\n",
			     (unsigned)&STFE_BasePointers.InputBlockBase_p[IBBlockIndex]);
	stptiSUPPORT_sprintf(ctx_p, "INPUT_FORMAT :  0x%08X\n", Tmp);
	stptiSUPPORT_sprintf(ctx_p, "INPUT_FORMAT :  SerialNotParallel   : %u\n", Tmp & 0x01);
	stptiSUPPORT_sprintf(ctx_p, "INPUT_FORMAT :  ByteEndianness      : %u\n", (Tmp >> 1) & 0x01);
	stptiSUPPORT_sprintf(ctx_p, "INPUT_FORMAT :  AsyncNotSync        : %u\n", (Tmp >> 2) & 0x01);
	stptiSUPPORT_sprintf(ctx_p, "INPUT_FORMAT :  AlignByteSOP        : %u\n", (Tmp >> 3) & 0x01);
	stptiSUPPORT_sprintf(ctx_p, "INPUT_FORMAT :  InvertTSClk         : %u\n", (Tmp >> 4) & 0x01);
	stptiSUPPORT_sprintf(ctx_p, "INPUT_FORMAT :  IgnoreErrorInByte   : %u\n", (Tmp >> 5) & 0x01);
	stptiSUPPORT_sprintf(ctx_p, "INPUT_FORMAT :  IgnoreErrorInPkt    : %u\n", (Tmp >> 6) & 0x01);
	stptiSUPPORT_sprintf(ctx_p, "INPUT_FORMAT :  IgnoreErrorAtSOP    : %u\n", (Tmp >> 7) & 0x01);
	stptiSUPPORT_sprintf(ctx_p, "-------------------------------------------------\n");
	Tmp = stptiSupport_ReadReg32(&(STFE_BasePointers.InputBlockBase_p[IBBlockIndex].SLAD_CONFIG));
	stptiSUPPORT_sprintf(ctx_p, "SLAD_CONFIG  :  0x%08X\n", Tmp);
	stptiSUPPORT_sprintf(ctx_p, "SLAD_CONFIG  :  Sync                : %u\n", Tmp & 0x0F);
	stptiSUPPORT_sprintf(ctx_p, "SLAD_CONFIG  :  Drop                : %u\n", (Tmp >> 4) & 0x0F);
	stptiSUPPORT_sprintf(ctx_p, "SLAD_CONFIG  :  Token               : 0x%02X\n", (Tmp >> 8) & 0xFF);
	stptiSUPPORT_sprintf(ctx_p, "SLAD_CONFIG  :  SLDEndianness       : %u\n", (Tmp >> 16) & 0x01);
	stptiSUPPORT_sprintf(ctx_p, "-------------------------------------------------\n");

	Tmp = stptiSupport_ReadReg32(&(STFE_BasePointers.InputBlockBase_p[IBBlockIndex].TAG_BYTES));
	stptiSUPPORT_sprintf(ctx_p, "TAG_BYTES    :  0x%08X\n", Tmp);
	stptiSUPPORT_sprintf(ctx_p, "TAG_BYTES    :  Enable              : %u\n", Tmp & 0x01);
	stptiSUPPORT_sprintf(ctx_p, "TAG_BYTES    :  Counter             : %u\n", (Tmp >> 1) & 0x03);
	stptiSUPPORT_sprintf(ctx_p, "TAG_BYTES    :  Header              : 0x%04X\n", (Tmp >> 16) & 0xFFFF);
	stptiSUPPORT_sprintf(ctx_p, "-------------------------------------------------\n");
	stptiSUPPORT_sprintf(ctx_p, "TAG_COUNTER  0x%08x%08x\n",
			     stptiSupport_ReadReg32(&
						    (STFE_BasePointers.TagCounterBase_p[((Tmp >> 1) & 0x03)].
						     COUNTER_LSW)),
			     stptiSupport_ReadReg32(&
						    (STFE_BasePointers.TagCounterBase_p[((Tmp >> 1) & 0x03)].
						     COUNTER_MSW)));
	stptiSUPPORT_sprintf(ctx_p, "-------------------------------------------------\n");

	Tmp = stptiSupport_ReadReg32(&(STFE_BasePointers.InputBlockBase_p[IBBlockIndex].PID_SETUP));
	stptiSUPPORT_sprintf(ctx_p, "PID_SETUP    :  0x%08X\n", Tmp);
	stptiSUPPORT_sprintf(ctx_p, "PID_SETUP    :  Offset              : %u\n", Tmp & 0x3F);
	stptiSUPPORT_sprintf(ctx_p, "PID_SETUP    :  NumBits             : %u\n", (Tmp >> 6) & 0x3F);
	stptiSUPPORT_sprintf(ctx_p, "PID_SETUP    :  Enable              : %u\n", (Tmp >> 31) & 0x01);
	stptiSUPPORT_sprintf(ctx_p, "-------------------------------------------------\n");

	stptiSUPPORT_sprintf(ctx_p, "PKT_LENGTH   :  %u\n",
			     stptiSupport_ReadReg32(&(STFE_BasePointers.InputBlockBase_p[IBBlockIndex].PACKET_LENGTH)) &
			     0xFFF);
	stptiSUPPORT_sprintf(ctx_p, "BUFFER_START :  0x%08X\n",
			     stptiSupport_ReadReg32(&(STFE_BasePointers.InputBlockBase_p[IBBlockIndex].BUFFER_START)));
	stptiSUPPORT_sprintf(ctx_p, "BUFFER_END   :  0x%08X\n",
			     stptiSupport_ReadReg32(&(STFE_BasePointers.InputBlockBase_p[IBBlockIndex].BUFFER_END)));
	stptiSUPPORT_sprintf(ctx_p, "READ_POINTER :  0x%08X\n",
			     stptiSupport_ReadReg32(&(STFE_BasePointers.InputBlockBase_p[IBBlockIndex].READ_POINTER)));
	stptiSUPPORT_sprintf(ctx_p, "WRITE_POINTER:  0x%08X\n",
			     stptiSupport_ReadReg32(&(STFE_BasePointers.InputBlockBase_p[IBBlockIndex].WRITE_POINTER)));
	stptiSUPPORT_sprintf(ctx_p, "P_THRESHOLD  :  %u\n",
			     stptiSupport_ReadReg32(&(STFE_BasePointers.InputBlockBase_p[IBBlockIndex].PRIORITY_THRESHOLD)) & 0xF);
	stptiSUPPORT_sprintf(ctx_p, "-------------------------------------------------\n");

	Tmp = stptiSupport_ReadReg32(&(STFE_BasePointers.InputBlockBase_p[IBBlockIndex].STATUS));
	stptiSUPPORT_sprintf(ctx_p, "STATUS       :  0x%08X\n", Tmp);
	stptiSUPPORT_sprintf(ctx_p, "STATUS       :  FifoOverFlow        : %u\n", Tmp & 0x01);
	stptiSUPPORT_sprintf(ctx_p, "STATUS       :  BufferOverflow      : %u\n", (Tmp >> 1) & 0x01);
	stptiSUPPORT_sprintf(ctx_p, "STATUS       :  OutOfOrderRP        : %u\n", (Tmp >> 2) & 0x01);
	stptiSUPPORT_sprintf(ctx_p, "STATUS       :  PIDOverFlow         : %u\n", (Tmp >> 3) & 0x01);
	stptiSUPPORT_sprintf(ctx_p, "STATUS       :  PktOverFlow         : %u\n", (Tmp >> 4) & 0x01);
	stptiSUPPORT_sprintf(ctx_p, "STATUS       :  ErrorPkts           : %u\n", (Tmp >> 8) & 0x0F);
	stptiSUPPORT_sprintf(ctx_p, "STATUS       :  ShortPkts           : %u\n", (Tmp >> 12) & 0x0F);
	stptiSUPPORT_sprintf(ctx_p, "STATUS       :  Lock                : %u\n", (Tmp >> 30) & 0x01);
	stptiSUPPORT_sprintf(ctx_p, "-------------------------------------------------\n");

	Tmp = stptiSupport_ReadReg32(&(STFE_BasePointers.InputBlockBase_p[IBBlockIndex].MASK));
	stptiSUPPORT_sprintf(ctx_p, "MASK         :  0x%08X\n", Tmp);
	stptiSUPPORT_sprintf(ctx_p, "MASK         :  FifoOverFlow        : %u\n", Tmp & 0x01);
	stptiSUPPORT_sprintf(ctx_p, "MASK         :  BufferOverflow      : %u\n", (Tmp >> 1) & 0x01);
	stptiSUPPORT_sprintf(ctx_p, "MASK         :  OutOfOrderRP        : %u\n", (Tmp >> 2) & 0x01);
	stptiSUPPORT_sprintf(ctx_p, "MASK         :  PIDOverFlow         : %u\n", (Tmp >> 3) & 0x01);
	stptiSUPPORT_sprintf(ctx_p, "MASK         :  PktOverFlow         : %u\n", (Tmp >> 4) & 0x01);
	stptiSUPPORT_sprintf(ctx_p, "MASK         :  ErrorPkts           : %u\n", (Tmp >> 8) & 0x0F);
	stptiSUPPORT_sprintf(ctx_p, "MASK         :  ShortPkts           : %u\n", (Tmp >> 12) & 0x0F);
	stptiSUPPORT_sprintf(ctx_p, "-------------------------------------------------\n");

	Tmp = stptiSupport_ReadReg32(&(STFE_BasePointers.InputBlockBase_p[IBBlockIndex].SYSTEM));
	stptiSUPPORT_sprintf(ctx_p, "SYSTEM       :  0x%08X\n", Tmp);
	stptiSUPPORT_sprintf(ctx_p, "SYSTEM       :  Enable              : %u\n", Tmp & 0x01);
	stptiSUPPORT_sprintf(ctx_p, "SYSTEM       :  Reset               : %u\n", (Tmp >> 1) & 0x01);
	stptiSUPPORT_sprintf(ctx_p, "SYSTEM       :  Bypass              : %u\n", (Tmp >> 2) & 0x01);
	stptiSUPPORT_sprintf(ctx_p, "-------------------------------------------------\n");
}

void stptiTSHAL_StfeSWTSRegisterDump(U32 SWTSBlockIndex, stptiSUPPORT_sprintf_t *ctx_p) {
	U32 Tmp;

	Tmp = stptiSupport_ReadReg32(&(STFE_BasePointers.SWTSInputBlockBase_p[SWTSBlockIndex].FIFO_CONFIG));

	stptiSUPPORT_sprintf(ctx_p, "----------- STFE SWTS Input Block %u debug ------\n", (U8) SWTSBlockIndex);
	stptiSUPPORT_sprintf(ctx_p, "BASE_ADDRESS :  0x%08X\n\n",
			     (unsigned)&STFE_BasePointers.SWTSInputBlockBase_p[SWTSBlockIndex]);
	stptiSUPPORT_sprintf(ctx_p, "FIFO_CONFIG  :  0x%08X\n", Tmp);
	stptiSUPPORT_sprintf(ctx_p, "-------------------------------------------------\n");
	Tmp = stptiSupport_ReadReg32(&(STFE_BasePointers.SWTSInputBlockBase_p[SWTSBlockIndex].SLAD_CONFIG));
	stptiSUPPORT_sprintf(ctx_p, "SLAD_CONFIG  :  0x%08X\n", Tmp);
	stptiSUPPORT_sprintf(ctx_p, "SLAD_CONFIG  :  Sync                : %u\n", Tmp & 0x0F);
	stptiSUPPORT_sprintf(ctx_p, "SLAD_CONFIG  :  Drop                : %u\n", (Tmp >> 4) & 0x0F);
	stptiSUPPORT_sprintf(ctx_p, "SLAD_CONFIG  :  Token               : 0x%02X\n", (Tmp >> 8) & 0xFF);
	stptiSUPPORT_sprintf(ctx_p, "SLAD_CONFIG  :  SLDEndianness       : %u\n", (Tmp >> 16) & 0x01);
	stptiSUPPORT_sprintf(ctx_p, "SLAD_CONFIG  :  Tagged              : %u\n", (Tmp >> 17) & 0x01);
	stptiSUPPORT_sprintf(ctx_p, "-------------------------------------------------\n");

	Tmp = stptiSupport_ReadReg32(&(STFE_BasePointers.SWTSInputBlockBase_p[SWTSBlockIndex].TAG_BYTE_ADD));
	stptiSUPPORT_sprintf(ctx_p, "TAG_BYTES    :  0x%08X\n", Tmp);
	stptiSUPPORT_sprintf(ctx_p, "TAG_BYTES    :  Enable              : %u\n", Tmp & 0x01);
	stptiSUPPORT_sprintf(ctx_p, "TAG_BYTES    :  Header              : 0x%04X\n", (Tmp >> 16) & 0xFFFF);
	stptiSUPPORT_sprintf(ctx_p, "-------------------------------------------------\n");

	Tmp = stptiSupport_ReadReg32(&(STFE_BasePointers.SWTSInputBlockBase_p[SWTSBlockIndex].TAG_BYTE_REPLACE));
	stptiSUPPORT_sprintf(ctx_p, "TAG_BYTE_REP :  0x%08X\n", Tmp);
	stptiSUPPORT_sprintf(ctx_p, "TAG_BYTE_REP :  Mask                : 0x%04X\n", (Tmp >> 16) & 0xFFFF);
	stptiSUPPORT_sprintf(ctx_p, "-------------------------------------------------\n");

	stptiSUPPORT_sprintf(ctx_p, "PKT_LENGTH   :  %u\n",
			     stptiSupport_ReadReg32(&
						    (STFE_BasePointers.SWTSInputBlockBase_p[SWTSBlockIndex].
						     PACKET_LENGTH)) & 0xFFF);
	stptiSUPPORT_sprintf(ctx_p, "BUFFER_START :  0x%08X\n",
			     stptiSupport_ReadReg32(&
						    (STFE_BasePointers.SWTSInputBlockBase_p[SWTSBlockIndex].
						     BUFFER_START)));
	stptiSUPPORT_sprintf(ctx_p, "BUFFER_END   :  0x%08X\n",
			     stptiSupport_ReadReg32(&
						    (STFE_BasePointers.SWTSInputBlockBase_p[SWTSBlockIndex].
						     BUFFER_END)));
	stptiSUPPORT_sprintf(ctx_p, "READ_POINTER :  0x%08X\n",
			     stptiSupport_ReadReg32(&
						    (STFE_BasePointers.SWTSInputBlockBase_p[SWTSBlockIndex].
						     READ_POINTER)));
	stptiSUPPORT_sprintf(ctx_p, "WRITE_POINTER:  0x%08X\n",
			     stptiSupport_ReadReg32(&
						    (STFE_BasePointers.SWTSInputBlockBase_p[SWTSBlockIndex].
						     WRITE_POINTER)));
	stptiSUPPORT_sprintf(ctx_p, "-------------------------------------------------\n");

	Tmp = stptiSupport_ReadReg32(&(STFE_BasePointers.SWTSInputBlockBase_p[SWTSBlockIndex].STATUS));
	stptiSUPPORT_sprintf(ctx_p, "STATUS       :  0x%08X\n", Tmp);
	stptiSUPPORT_sprintf(ctx_p, "STATUS       :  FifoOverFlow        : %u\n", Tmp & 0x01);
	stptiSUPPORT_sprintf(ctx_p, "STATUS       :  OutOfOrderRP        : %u\n", (Tmp >> 2) & 0x01);
	stptiSUPPORT_sprintf(ctx_p, "STATUS       :  DiscardedPkts       : %u\n", (Tmp >> 8) & 0x0F);
	stptiSUPPORT_sprintf(ctx_p, "STATUS       :  Locked              : %u\n", (Tmp >> 30) & 0x01);
	stptiSUPPORT_sprintf(ctx_p, "STATUS       :  FIFOReq             : %u\n", (Tmp >> 31) & 0x01);
	stptiSUPPORT_sprintf(ctx_p, "-------------------------------------------------\n");

	Tmp = stptiSupport_ReadReg32(&(STFE_BasePointers.SWTSInputBlockBase_p[SWTSBlockIndex].MASK));
	stptiSUPPORT_sprintf(ctx_p, "MASK         :  0x%08X\n", Tmp);
	stptiSUPPORT_sprintf(ctx_p, "MASK         :  FifoOverFlow        : %u\n", Tmp & 0x01);
	stptiSUPPORT_sprintf(ctx_p, "MASK         :  OutOfOrderRP        : %u\n", (Tmp >> 2) & 0x01);
	stptiSUPPORT_sprintf(ctx_p, "MASK         :  DiscardedPkts       : %u\n", (Tmp >> 8) & 0x0F);
	stptiSUPPORT_sprintf(ctx_p, "-------------------------------------------------\n");

	Tmp = stptiSupport_ReadReg32(&(STFE_BasePointers.SWTSInputBlockBase_p[SWTSBlockIndex].SYSTEM));
	stptiSUPPORT_sprintf(ctx_p, "SYSTEM       :  0x%08X\n", Tmp);
	stptiSUPPORT_sprintf(ctx_p, "SYSTEM       :  Enable              : %u\n", Tmp & 0x01);
	stptiSUPPORT_sprintf(ctx_p, "SYSTEM       :  Reset               : %u\n", (Tmp >> 1) & 0x01);
	stptiSUPPORT_sprintf(ctx_p, "-------------------------------------------------\n");
}

void stptiTSHAL_StfeMIBRegisterDump(U32 MIBChannel, stptiSUPPORT_sprintf_t *ctx_p) {

	U32 Tmp = stptiSupport_ReadReg32(&(STFE_BasePointers.MergedInputBlockBase_p[0].INPUT_FORMAT_CONFIG));

	stptiSUPPORT_sprintf(ctx_p, "----------- STFE Merged Input Block %u debug ----\n", (U8) MIBChannel);
	stptiSUPPORT_sprintf(ctx_p, "INPUT_FORMAT :  0x%08X\n", Tmp);
	stptiSUPPORT_sprintf(ctx_p, "INPUT_FORMAT :  SerialNotParallel   : %u\n", Tmp & 0x01);
	stptiSUPPORT_sprintf(ctx_p, "INPUT_FORMAT :  ByteEndianness      : %u\n", (Tmp >> 1) & 0x01);
	stptiSUPPORT_sprintf(ctx_p, "INPUT_FORMAT :  AsyncNotSync        : %u\n", (Tmp >> 2) & 0x01);
	stptiSUPPORT_sprintf(ctx_p, "INPUT_FORMAT :  InvertTSClk         : %u\n", (Tmp >> 4) & 0x01);
	stptiSUPPORT_sprintf(ctx_p, "INPUT_FORMAT :  IgnoreErrorInByte   : %u\n", (Tmp >> 5) & 0x01);
	stptiSUPPORT_sprintf(ctx_p, "INPUT_FORMAT :  IgnoreErrorInPkt    : %u\n", (Tmp >> 6) & 0x01);
	stptiSUPPORT_sprintf(ctx_p, "INPUT_FORMAT :  IgnoreErrorAtSOP    : %u\n", (Tmp >> 7) & 0x01);
	stptiSUPPORT_sprintf(ctx_p, "-------------------------------------------------\n");

	Tmp = stptiSupport_ReadReg32(&(STFE_BasePointers.MergedInputBlockBase_p[MIBChannel].STREAM_IDENT));
	stptiSUPPORT_sprintf(ctx_p, "STREAM_IDENT :  0x%08X\n", Tmp);
	stptiSUPPORT_sprintf(ctx_p, "STREAM_IDENT :  Mask                : 0x%02x\n", (Tmp >> 16) & 0xFF);
	stptiSUPPORT_sprintf(ctx_p, "STREAM_IDENT :  Value               : 0x%02x\n", Tmp & 0xFF);
	stptiSUPPORT_sprintf(ctx_p, "-------------------------------------------------\n");

	Tmp = stptiSupport_ReadReg32(&(STFE_BasePointers.MergedInputBlockBase_p[0].PID_SETUP));
	stptiSUPPORT_sprintf(ctx_p, "PID_SETUP    :  0x%08X\n", Tmp);
	stptiSUPPORT_sprintf(ctx_p, "PID_SETUP    :  Offset              : %u\n", Tmp & 0x3F);
	stptiSUPPORT_sprintf(ctx_p, "PID_SETUP    :  NumBits             : %u\n", (Tmp >> 6) & 0x3F);
	stptiSUPPORT_sprintf(ctx_p, "PID_SETUP    :  Enable              : %u\n", (Tmp >> 16 >> MIBChannel) & 0x01);
	stptiSUPPORT_sprintf(ctx_p, "-------------------------------------------------\n");

	stptiSUPPORT_sprintf(ctx_p, "PKT_LENGTH   :  %u\n",
			     stptiSupport_ReadReg32(&(STFE_BasePointers.MergedInputBlockBase_p[0].PACKET_LENGTH)) &
			     0xFFF);
	stptiSUPPORT_sprintf(ctx_p, "BUFFER_START :  0x%08X\n",
			     stptiSupport_ReadReg32(&(STFE_BasePointers.MergedInputBlockBase_p[MIBChannel].BUFFER_START)));
	stptiSUPPORT_sprintf(ctx_p, "BUFFER_END   :  0x%08X\n",
			     stptiSupport_ReadReg32(&(STFE_BasePointers.MergedInputBlockBase_p[MIBChannel].BUFFER_END)));
	stptiSUPPORT_sprintf(ctx_p, "READ_POINTER :  0x%08X\n",
			     stptiSupport_ReadReg32(&(STFE_BasePointers.MergedInputBlockBase_p[MIBChannel].READ_POINTER)));
	stptiSUPPORT_sprintf(ctx_p, "WRITE_POINTER:  0x%08X\n",
			     stptiSupport_ReadReg32(&(STFE_BasePointers.MergedInputBlockBase_p[MIBChannel].WRITE_POINTER)));
	stptiSUPPORT_sprintf(ctx_p, "P_THRESHOLD  :  0x%08X\n",
			     stptiSupport_ReadReg32(&(STFE_BasePointers.MergedInputBlockBase_p[0].PRIORITY_THRESHOLD)));
	stptiSUPPORT_sprintf(ctx_p, "-------------------------------------------------\n");

	Tmp = stptiSupport_ReadReg32(&(STFE_BasePointers.MergedInputBlockBase_p[MIBChannel].STATUS));
	stptiSUPPORT_sprintf(ctx_p, "STATUS       :  0x%08X\n", Tmp);
	stptiSUPPORT_sprintf(ctx_p, "STATUS       :  FifoOverFlow        : %u\n", Tmp & 0x01);
	stptiSUPPORT_sprintf(ctx_p, "STATUS       :  BufferOverflow      : %u\n", (Tmp >> 1) & 0x01);
	stptiSUPPORT_sprintf(ctx_p, "STATUS       :  OutOfOrderRP        : %u\n", (Tmp >> 2) & 0x01);
	stptiSUPPORT_sprintf(ctx_p, "STATUS       :  PIDOverFlow         : %u\n", (Tmp >> 3) & 0x01);
	stptiSUPPORT_sprintf(ctx_p, "STATUS       :  PktOverFlow         : %u\n", (Tmp >> 4) & 0x01);
	stptiSUPPORT_sprintf(ctx_p, "STATUS       :  BadStream           : %u\n", (Tmp >> 5) & 0x01);
	stptiSUPPORT_sprintf(ctx_p, "STATUS       :  ErrorPkts           : %u\n", (Tmp >> 8) & 0x0F);
	stptiSUPPORT_sprintf(ctx_p, "STATUS       :  ShortPkts           : %u\n", (Tmp >> 12) & 0x0F);
	stptiSUPPORT_sprintf(ctx_p, "-------------------------------------------------\n");

	Tmp = stptiSupport_ReadReg32(&(STFE_BasePointers.MergedInputBlockBase_p[MIBChannel].MASK));
	stptiSUPPORT_sprintf(ctx_p, "MASK         :  0x%08X\n", Tmp);
	stptiSUPPORT_sprintf(ctx_p, "MASK         :  FifoOverFlow        : %u\n", Tmp & 0x01);
	stptiSUPPORT_sprintf(ctx_p, "MASK         :  BufferOverflow      : %u\n", (Tmp >> 1) & 0x01);
	stptiSUPPORT_sprintf(ctx_p, "MASK         :  OutOfOrderRP        : %u\n", (Tmp >> 2) & 0x01);
	stptiSUPPORT_sprintf(ctx_p, "MASK         :  PIDOverFlow         : %u\n", (Tmp >> 3) & 0x01);
	stptiSUPPORT_sprintf(ctx_p, "MASK         :  PktOverFlow         : %u\n", (Tmp >> 4) & 0x01);
	stptiSUPPORT_sprintf(ctx_p, "MASK         :  BadStream           : %u\n", (Tmp >> 5) & 0x01);
	stptiSUPPORT_sprintf(ctx_p, "MASK         :  ErrorPkts           : %u\n", (Tmp >> 8) & 0x0F);
	stptiSUPPORT_sprintf(ctx_p, "MASK         :  ShortPkts           : %u\n", (Tmp >> 12) & 0x0F);
	stptiSUPPORT_sprintf(ctx_p, "-------------------------------------------------\n");

	Tmp = stptiSupport_ReadReg32(&(STFE_BasePointers.MergedInputBlockBase_p[MIBChannel].SYSTEM));
	stptiSUPPORT_sprintf(ctx_p, "SYSTEM       :  0x%08X\n", Tmp);
	stptiSUPPORT_sprintf(ctx_p, "SYSTEM       :  Enable              : %u\n", Tmp & 0x01);
	stptiSUPPORT_sprintf(ctx_p, "SYSTEM       :  Reset               : %u\n", (Tmp >> 1) & 0x01);
	stptiSUPPORT_sprintf(ctx_p, "SYSTEM       :  Flush               : %u\n", (Tmp >> 2) & 0x01);
	stptiSUPPORT_sprintf(ctx_p, "-------------------------------------------------\n");
}

void stptiTSHAL_StfeTSDMARegisterDump(U32 IBBlockIndex, U32 TSOutputIndex, stptiSUPPORT_sprintf_t *ctx_p) {
	/* TSDMA registers */
	stptiSUPPORT_sprintf(ctx_p, "----------- TSDMA channel %u debug ---------------\n", (U8) IBBlockIndex);
	stptiSUPPORT_sprintf(ctx_p, "STREAM_BASE    :  0x%08X\n",
			     stptiSupport_ReadReg32(&(STFE_BasePointers.TSDmaBlockBase.TSDMA_CHANNEL[IBBlockIndex].STREAM_BASE)));
	stptiSUPPORT_sprintf(ctx_p, "STREAM_TOP     :  0x%08X\n",
			     stptiSupport_ReadReg32(&(STFE_BasePointers.TSDmaBlockBase.TSDMA_CHANNEL[IBBlockIndex].STREAM_TOP)));
	stptiSUPPORT_sprintf(ctx_p, "PKT_SIZE       :  %u\n",
			     stptiSupport_ReadReg32(&(STFE_BasePointers.TSDmaBlockBase.TSDMA_CHANNEL[IBBlockIndex].STREAM_PKT_SIZE)));
	stptiSUPPORT_sprintf(ctx_p, "STREAM_FORMAT  :  0x%08X\n",
			     stptiSupport_ReadReg32(&(STFE_BasePointers.TSDmaBlockBase.TSDMA_CHANNEL[IBBlockIndex].STREAM_FORMAT)));
	stptiSUPPORT_sprintf(ctx_p, "STREAM_PACECFG :  0x%08X\n",
			     stptiSupport_ReadReg32(&(STFE_BasePointers.TSDmaBlockBase.TSDMA_CHANNEL[IBBlockIndex].STREAM_PACE_CFG)));
	stptiSUPPORT_sprintf(ctx_p, "STREAM_PC      :  0x%08X\n",
			     stptiSupport_ReadReg32(&(STFE_BasePointers.TSDmaBlockBase.TSDMA_CHANNEL[IBBlockIndex].STREAM_PC)));
	stptiSUPPORT_sprintf(ctx_p, "STREAM_STATUS  :  0x%08X\n",
			     stptiSupport_ReadReg32(&(STFE_BasePointers.TSDmaBlockBase.TSDMA_CHANNEL[IBBlockIndex].STREAM_STATUS)));
	stptiSUPPORT_sprintf(ctx_p, "-------------------------------------------------\n");

	stptiSUPPORT_sprintf(ctx_p, "----------- TSOUT %u debug ---------------\n", (U8) TSOutputIndex);
	stptiTSHAL_HW_function_stfe->stptiTSHAL_StfeTsdmaDumpMixedReg(TSOutputIndex, ctx_p);
	stptiSUPPORT_sprintf(ctx_p, "DEST_FIFO_TRIG :  0x%08X\n",
			     stptiSupport_ReadReg32(&(STFE_BasePointers.TSDmaBlockBase.TSDMA_OUTPUT[TSOutputIndex].DEST_FIFO_TRIG)));
	stptiSUPPORT_sprintf(ctx_p, "DEST_BYPASS    :  0x%08X\n",
			     stptiSupport_ReadReg32(&(STFE_BasePointers.TSDmaBlockBase.TSDMA_OUTPUT[TSOutputIndex].DEST_BYPASS)));
	stptiSUPPORT_sprintf(ctx_p, "DEST_CLKIDIV   :  0x%08X\n",
			     stptiSupport_ReadReg32(&(STFE_BasePointers.TSDmaBlockBase.TSDMA_OUTPUT[TSOutputIndex].DEST_CLKIDIV)));
	stptiSUPPORT_sprintf(ctx_p, "DEST_STAT      :  0x%08X\n",
			     stptiSupport_ReadReg32(&(STFE_BasePointers.TSDmaBlockBase.TSDMA_OUTPUT[TSOutputIndex].DEST_STATUS)));
	stptiSUPPORT_sprintf(ctx_p, "-------------------------------------------------\n");
}

void stptiTSHAL_StfeCCSCRegisterDump(stptiSUPPORT_sprintf_t *ctx_p) {
	stptiSUPPORT_sprintf(ctx_p, "----------- CCSC debug --------------------------\n");
	if (STFE_Config.NumCCSCs) {
		/* CCSC Registers */
		stptiSUPPORT_sprintf(ctx_p, "CCSC_CTRL      :  0x%08X\n",
				     stptiSupport_ReadReg32(&(STFE_BasePointers.CCSCBlockBase_p->CCSC_CTRL)));
		stptiSUPPORT_sprintf(ctx_p, "CCSC_CMP_RES   :  0x%08X\n",
				     stptiSupport_ReadReg32(&(STFE_BasePointers.CCSCBlockBase_p->CCSC_CMP_RES)));
		stptiSUPPORT_sprintf(ctx_p, "CCSC_CLK_CTRL  :  0x%08X\n",
				     stptiSupport_ReadReg32(&(STFE_BasePointers.CCSCBlockBase_p->CCSC_CLK_CTRL)));
	} else {
		stptiSUPPORT_sprintf(ctx_p, "----------- no CCSC on this platform ------------\n");
	}
	stptiSUPPORT_sprintf(ctx_p, "-------------------------------------------------\n");
}

void stptiTSHAL_StfePckRAMDump(U32 IBBlockIndex, stptiHAL_TSInputStfeType_t IBType, stptiSUPPORT_sprintf_t *ctx_p) {
	char block_name[16];
	U32 addr = 0, j = 0;
	U32 IndexInRam = 0;
	U32 Start_addr = 0;
	stptiTSHAL_InputResource_t *TSInputHWMapping_p = stptiTSHAL_GetHWMapping();
	U32 RamToBeUsed =
	    stptiTSHAL_HW_function_stfe->stptiTSHAL_StfeMemdma_ram2beUsed(IBBlockIndex, IBType, &IndexInRam);
	U32 StartAddressOfFifos = (U32) TSInputHWMapping_p->StfeRegs.STFE_RAM_MappedAddress_p[RamToBeUsed];
	U32 FifoSize = STFE_Config.RamConfig.FifoSize[RamToBeUsed];
	U32 InternalBase = StartAddressOfFifos + (IndexInRam * FifoSize);
	U32 InternalBaseEnd = StartAddressOfFifos + ((IndexInRam + 1) * FifoSize);

	switch (IBType) {
	case (stptiTSHAL_TSINPUT_STFE_IB):
		Start_addr = stptiSupport_ReadReg32(&(STFE_BasePointers.InputBlockBase_p[IBBlockIndex].BUFFER_START));
		snprintf(block_name, sizeof(block_name), "IB_BLOCK_%d", IBBlockIndex);
		break;
	case (stptiTSHAL_TSINPUT_STFE_MIB):
		Start_addr =
		    stptiSupport_ReadReg32(&(STFE_BasePointers.MergedInputBlockBase_p[IBBlockIndex].BUFFER_START));
		snprintf(block_name, sizeof(block_name), "MIB_BLOCK_%d", IBBlockIndex);
		break;
	case (stptiTSHAL_TSINPUT_STFE_SWTS):
		Start_addr =
		    stptiSupport_ReadReg32(&(STFE_BasePointers.SWTSInputBlockBase_p[IBBlockIndex].BUFFER_START));
		snprintf(block_name, sizeof(block_name), "SWTS_BLOCK_%d", IBBlockIndex);
		break;
	default:
		break;
	}

	addr = InternalBase;
	if (addr) {
		stptiSUPPORT_sprintf(ctx_p, "DUMP OF THE internal RAM of %s......\n", block_name);
		while (addr < (U32) InternalBaseEnd) {
			stptiSUPPORT_sprintf(ctx_p, "0x%08X ", Start_addr + (addr - InternalBase));
			for (j = 0; j < 8; j++) {
				U32 c = 0;
				for (c = 0; c < 4; c++) {
					stptiSUPPORT_sprintf(ctx_p, "%02x", ((U8 *) addr)[c]);
				}
				stptiSUPPORT_sprintf(ctx_p, " ");
				addr += 4;
			}
			stptiSUPPORT_sprintf(ctx_p, "\n");
		}
		stptiSUPPORT_sprintf(ctx_p, "-------------------------------------------------\n");
	}
}
