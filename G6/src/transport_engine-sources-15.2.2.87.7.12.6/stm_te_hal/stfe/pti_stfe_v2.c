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
   @file   pti_stfe_v2.c
   @brief  LLD driver to manage the STFE blocks
           that belong only to this version of STFE.

   LLD driver to manage all the STFE block
       that belong only to this version of STFE.
 */

#include "linuxcommon.h"

/* Includes from API level */
#include "../config/pti_platform.h"
#include "../pti_debug.h"
#include "../pti_hal_api.h"
#include "../pti_driver.h"

/* Includes from the HAL / ObjMan level */
#include "pti_tsinput.h"
#include "pti_stfe.h"

/* CCSC Register Values */
#define STFE_CCSC_ENABLE                            (0x00000001)
#define STFE_CCSC_BYPASS                            (0x00010000)

/* logic of this mask calculation is based on reg SYS_INPUT_CLKEN
 * bit definition to enable each STFE input
 * NumSWTS:NumMIB;NumMIB:NumIB;NumIB:0
 * */
#define STFE_GET_IB_EN_MASK(max_num_input, bit_shift, clk_enable_mask)	{\
	uint32_t __temp = 0;\
	int i = 0;\
	for (i = 0; i < max_num_input; i++) \
		__temp |= (1 << i); \
	clk_enable_mask = (__temp << bit_shift); \
}
static struct semaphore MemDmaSemaphore;

#if !defined( TANGOSIM ) && !defined( TANGOSIM2 ) && !defined(CONFIG_STM_VIRTUAL_PLATFORM)
static void stptiTSHAL_StfeMemdmaFWLoad(stptiHAL_STFEv2_DmaBlockDevice_t * DmaBlockDevice_phy_p);
#endif

/*
 * STATIC FUNCTION DEFINITION
 */
static irqreturn_t stptiHAL_MemDmaInterruptHandler(int irq, void *pDeviceHandle)
{

	stptiHAL_STFE_DmaBlockDevice_t *DmaBlockBase_p = (stptiHAL_STFE_DmaBlockDevice_t *) pDeviceHandle;
	stptiSupport_WriteReg32((volatile U32 *)
				&(DmaBlockBase_p->STFEv2_DmaBlockDevice.MEMDMA_DMEM.MEMDMA_SW.MEMDMA_IDLE_IRQ), 0);
	up(&MemDmaSemaphore);

	return IRQ_HANDLED;
}

static irqreturn_t stptiHAL_ErrorInterruptHandler(int irq, void *pDeviceHandle)
{
	stptiHAL_STFEv2_SystemBlockDevice_t *SystemBlockDevice_p = (stptiHAL_STFEv2_SystemBlockDevice_t *)pDeviceHandle;
	U32 input_status;
	U32 other_status;
	U32 Tmp, i;

	input_status = stptiSupport_ReadReg32(&(SystemBlockDevice_p->INPUT_STATUS[0]));
	other_status = stptiSupport_ReadReg32(&(SystemBlockDevice_p->OTHER_STATUS[0]));

	STPTI_PRINTF("-------------------------------------------------");
	STPTI_PRINTF("INPUT_STATUS :  0x%08X", input_status);
	STPTI_PRINTF("OTHER_STATUS :  0x%08X", other_status);
	STPTI_PRINTF("INPUT_MASK :    0x%08X", stptiSupport_ReadReg32(&(SystemBlockDevice_p->INPUT_MASK[0])));
	STPTI_PRINTF("OTHER_MASK :    0x%08X", stptiSupport_ReadReg32(&(SystemBlockDevice_p->OTHER_MASK[0])));
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
			} else {
				Tmp = stptiSupport_ReadReg32(&(STFE_BasePointers.SWTSInputBlockBase_p[i -(STFE_Config.NumIBs + STFE_Config.NumMIBs)].STATUS));
				STPTI_PRINTF("STATUS  OF SWTS %d     :  0x%08X",
					     (i - (STFE_Config.NumIBs + STFE_Config.NumMIBs)), Tmp);
				STPTI_PRINTF("STATUS       :  0x%08X", Tmp);
				STPTI_PRINTF("STATUS       :  FifoOverFlow        : %u", Tmp & 0x01);
				STPTI_PRINTF("STATUS       :  OutOfOrderRP        : %u", (Tmp >> 2) & 0x01);
				STPTI_PRINTF("STATUS       :  DiscardedPkts       : %u", (Tmp >> 8) & 0x0F);
				STPTI_PRINTF("STATUS       :  Locked              : %u", (Tmp >> 30) & 0x01);
				STPTI_PRINTF("STATUS       :  FIFOReq             : %u", (Tmp >> 31) & 0x01);
			}
			STPTI_PRINTF("-------------------------------------------------");
		}
	}

	if (other_status) {
		STPTI_PRINTF("Error into MEMDMA or TSDMA");
		STPTI_PRINTF("-------------------------------------------------");
	}

	return IRQ_HANDLED;
}

#if !defined( TANGOSIM ) && !defined( TANGOSIM2 ) && !defined(CONFIG_STM_VIRTUAL_PLATFORM)
static void stptiTSHAL_StfeMemdmaFWLoad(stptiHAL_STFEv2_DmaBlockDevice_t * DmaBlockDevice_phy_p)
{
	struct platform_device *pdev = STFE_Config.pdev;
	struct stpti_stfe_config *platform_data = pdev->dev.platform_data;
	Elf32_Ehdr *ehdr;
	Elf32_Phdr *phdr;
	int err = 0;
	int i;
	U8 *pImem = NULL;
	U8 *pDmem = NULL;

	BUG_ON(STPTI_Driver.cachedSTFEFW == NULL);

	dev_info(&pdev->dev, "Loading firmware: %s\n", platform_data->firmware);

	/* Check ELF magic */
	ehdr = (Elf32_Ehdr *)STPTI_Driver.cachedSTFEFW->data;
	if (ehdr->e_ident[EI_MAG0] != ELFMAG0 ||
	    ehdr->e_ident[EI_MAG1] != ELFMAG1 ||
	    ehdr->e_ident[EI_MAG2] != ELFMAG2 ||
	    ehdr->e_ident[EI_MAG3] != ELFMAG3) {
		dev_err(&pdev->dev, "Invalid ELF magic\n");
		err = -ENODEV;
	}

	/* Check program headers are within firmware size */
	if (ehdr->e_phoff + (ehdr->e_phnum * sizeof(Elf32_Phdr)) > STPTI_Driver.cachedSTFEFW->size) {
		dev_err(&pdev->dev, "Program headers outside of firmware file\n");
		err = -ENODEV;
		goto done;
	}

	/* Map IMEM and DMEM regions */
	pImem = ioremap_nocache((U32)&DmaBlockDevice_phy_p->MEMDMA_IMEM, sizeof(DmaBlockDevice_phy_p->MEMDMA_IMEM));
	pDmem = ioremap_nocache((U32)&DmaBlockDevice_phy_p->MEMDMA_DMEM, sizeof(DmaBlockDevice_phy_p->MEMDMA_DMEM));

	if (!pImem || !pDmem) {
		dev_err(&pdev->dev, "Failed to map MEMDMA region\n");
		err = -ENODEV;
		goto done;
	}

	memset(pDmem, 0x0, sizeof(DmaBlockDevice_phy_p->MEMDMA_DMEM));
	/* reset the Imem as well before reload */
	memset(pImem, 0x0, sizeof(DmaBlockDevice_phy_p->MEMDMA_IMEM));

	phdr = (Elf32_Phdr *)(STPTI_Driver.cachedSTFEFW->data + ehdr->e_phoff);
	for (i = 0; i < ehdr->e_phnum && !err; i++) {
		/* Only consider LOAD segments */
		if (PT_LOAD == phdr->p_type) {
			U8 *dest;
			BOOL imem = FALSE;

			/* Check segment is contained within the fw->data
			 * buffer */
			if (phdr->p_offset + phdr->p_filesz > STPTI_Driver.cachedSTFEFW->size) {
				dev_err(&pdev->dev, "Segment %d is outside of firmware file\n", i);
				err = -EINVAL;
				break;
			}

			/* MEMDMA IMEM has executable flag set, otherwise load
			 * this segment to DMEM*/
			if (phdr->p_flags & PF_X) {
				dest = pImem;
				imem = TRUE;
			} else {
				dest = pDmem;
			}

			/* The Slim ELF file uses 32-bit word addressing for
			 * load offsets */
			dest += (phdr->p_paddr & 0xFFFFF) * sizeof(U32);

			/* For DMEM segments copy the segment data from the ELF
			 * file and pad segment with zeroes
			 *
			 * For IMEM segments, the segment contains 24-bit
			 * instructions which must be padded to 32-bit
			 * instructions before being written. The written
			 * segment is padded with NOP instructions */
			if (!imem) {
				dev_info(&pdev->dev,
					 "Loading DMEM segment %d 0x%08x (0x%x bytes) -> 0x%08x (0x%x bytes)\n", i,
					 phdr->p_paddr, phdr->p_filesz, (U32)dest, phdr->p_memsz);
				memcpy(dest, STPTI_Driver.cachedSTFEFW->data + phdr->p_offset, phdr->p_filesz);
				memset(dest + phdr->p_filesz, 0, phdr->p_memsz - phdr->p_filesz);
			} else {
				const U8 *imem_src = STPTI_Driver.cachedSTFEFW->data + phdr->p_offset;
				U8 *imem_dest = dest;
				int j;

				dev_info(&pdev->dev,
					 "Loading IMEM segment %d 0x%08x (0x%x bytes) -> 0x%08x (0x%x bytes)\n", i,
					 phdr->p_paddr, phdr->p_filesz, (U32)dest, phdr->p_memsz + phdr->p_memsz / 3);
				for (j = 0; j < phdr->p_filesz; j++) {
					*imem_dest = *imem_src;

					/* Every 3 bytes, add an additional
					 * padding zero in destination */
					if (j % 3 == 2) {
						imem_dest++;
						*imem_dest = 0x00;
					}

					imem_dest++;
					imem_src++;
				}
			}
		}
		phdr++;
	}
done:
	if (pImem)
		iounmap(pImem);
	if (pDmem)
		iounmap(pDmem);
}
#endif
/*
 * GLOBAL DEFINITION
 */
void stptiTSHAL_Stfev2_LoadMemDmaFw(void)
{
	// MEMDMA FW LOADING
#if !defined( TANGOSIM ) && !defined( TANGOSIM2 ) && !defined(CONFIG_STM_VIRTUAL_PLATFORM)
	stptiTSHAL_StfeMemdmaFWLoad((stptiHAL_STFEv2_DmaBlockDevice_t *)((U32) STFE_Config.PhysicalAddress +
									 (U32) STFE_Config.MemDMAOffset));
#endif
}

int stptiTSHAL_Stfev2_CreateConfig(stptiTSHAL_STFE_RamConfig_t *RamConfig_p, U32 *pN_TPs)
{
	U32 i = 0;
	U32 NumMIbCell = 0;
	U32 reg_value = 0;
	U32 NMIBs = 0;
	U32 NIBs = 0;
	U32 NTPs = 0;

	U32 RamToBeUsed = 0;
	stptiHAL_STFEv2_SystemBlockDevice_t *STFE_SYSMappedAddress_p = NULL;
	stptiHAL_STFEv2_DmaBlockDevice_t *STFE_DMAMappedAddress_p = NULL;
	stptiHAL_STFE_DmaSWDevice_t *STFE_DMASW_MappedAddress_p = NULL;
	stptiHAL_STFE_DmaCPUDevice_t *STFE_DMACPU_MappedAddress_p = NULL;
	U32 dma_cpu_offset = (U32)&STFE_DMAMappedAddress_p->MEMDMA_CPU;
	U32 dma_dmem_offset = (U32)&STFE_DMAMappedAddress_p->MEMDMA_DMEM;

	//***** read the system registry  *****/
#if defined(TANGOSIM) || defined(TANGOSIM2) || defined(CONFIG_STM_VIRTUAL_PLATFORM)
	/* This is a hack to make sure we can map the STFE_RAM on the ST200 simulator.
	   It seems the BSP doesn't map this region by default. */
	mmap_create((void *)(((U32)STFE_Config.PhysicalAddress + (U32)STFE_Config.SystemRegsOffset)),
		    mmap_pagesize(sizeof(stptiHAL_STFEv2_SystemBlockDevice_t)),
		    mmap_protect_rw, mmap_uncached, mmap_pagesize(sizeof(stptiHAL_STFEv2_SystemBlockDevice_t)));

	mmap_create((void *)((U32)STFE_Config.PhysicalAddress + (U32)STFE_Config.MemDMAOffset + dma_dmem_offset),
		    mmap_pagesize(sizeof(stptiHAL_STFE_DmaSWDevice_t)),
		    mmap_protect_rw, mmap_uncached, mmap_pagesize(sizeof(stptiHAL_STFE_DmaSWDevice_t)));

#endif

	STFE_SYSMappedAddress_p =
	    ioremap_nocache((unsigned long)STFE_Config.PhysicalAddress + (unsigned long)STFE_Config.SystemRegsOffset,
			    sizeof(stptiHAL_STFEv2_SystemBlockDevice_t));

	STFE_DMASW_MappedAddress_p =
	    ioremap_nocache((unsigned long)STFE_Config.PhysicalAddress + (unsigned long)STFE_Config.MemDMAOffset +
			    dma_dmem_offset, sizeof(stptiHAL_STFE_DmaSWDevice_t));

	STFE_DMACPU_MappedAddress_p =
	    ioremap_nocache((unsigned long)STFE_Config.PhysicalAddress + (unsigned long)STFE_Config.MemDMAOffset +
			    dma_cpu_offset, sizeof(stptiHAL_STFE_DmaCPUDevice_t));
	if (STFE_SYSMappedAddress_p == NULL ||
			STFE_DMASW_MappedAddress_p == NULL||STFE_DMACPU_MappedAddress_p ==NULL) {
		STPTI_PRINTF("ERROR: failed to Memory Map STFE resources\n");
		return -ENODEV;
	}
	// reset STFE slim core
	stptiSupport_WriteReg32((volatile U32*)&(STFE_DMACPU_MappedAddress_p->MEMDMA_CPU_RUN),0);
	stptiSupport_WriteReg32((volatile U32*)(&(STFE_DMACPU_MappedAddress_p->MEMDMA_CPU_RUN)+0x1),1);
	stptiSupport_WriteReg32((volatile U32*)(&(STFE_DMACPU_MappedAddress_p->MEMDMA_CPU_RUN)+0x1),4);
	//********** workout parameter  *******/
	NIBs = stptiSupport_ReadReg32((volatile U32 *)&(STFE_SYSMappedAddress_p->NUM_IB));
	memcpy((void *)&STFE_Config.NumIBs, &NIBs, sizeof(U32));

	NumMIbCell = stptiSupport_ReadReg32((volatile U32 *)&(STFE_SYSMappedAddress_p->NUM_MIB));
	NMIBs = 0;
	for (i = 0; i < NumMIbCell; i++) {
		NMIBs +=
		    (((stptiSupport_ReadReg32((volatile U32 *)&(STFE_SYSMappedAddress_p->MIB_CFG[i].INFO))) >>
		      STFEv2_NUM_SUBSTREAMS_SHIFT) & STFEv2_CFG_BLOCK_MASK);
	}
	memcpy((void *)&STFE_Config.NumMIBs, &NMIBs, sizeof(U32));

	reg_value = stptiSupport_ReadReg32((volatile U32 *)&(STFE_SYSMappedAddress_p->NUM_SWTS));
	memcpy((void *)&STFE_Config.NumSWTSs, &reg_value, sizeof(U32));

	reg_value = stptiSupport_ReadReg32((volatile U32 *)&(STFE_SYSMappedAddress_p->NUM_TSDMA));
	memcpy((void *)&STFE_Config.NumTSDMAs, &reg_value, sizeof(U32));

	reg_value = stptiSupport_ReadReg32((volatile U32 *)&(STFE_SYSMappedAddress_p->NUM_CCSC));
	memcpy((void *)&STFE_Config.NumCCSCs, &reg_value, sizeof(U32));

	NTPs = stptiSupport_ReadReg32((volatile U32 *)&(STFE_SYSMappedAddress_p->NUM_TP));

	//**************************************
	// ANALYZE RAM
	//**************************************
	{
		memset(RamConfig_p, 0xF, sizeof(stptiTSHAL_STFE_RamConfig_t));

		RamConfig_p->NumRam = stptiSupport_ReadReg32((volatile U32 *)&(STFE_SYSMappedAddress_p->NUM_RAM));

		for (i = 0; i < RamConfig_p->NumRam; i++) {
			reg_value =  stptiSupport_ReadReg32((volatile U32 *)&(STFE_SYSMappedAddress_p->RAM_CFG[i].RAM_ADDR));
			RamConfig_p->RAMAddr[i] = reg_value;

			reg_value = stptiSupport_ReadReg32((volatile U32 *)&(STFE_SYSMappedAddress_p->RAM_CFG[i].RAM_SIZE));
			RamConfig_p->RAMSize[i] = reg_value;
			RamConfig_p->FifoSize[i] = 0;
			RamConfig_p->RAMNumIB[i] = 0;
			RamConfig_p->RAMNumMIB[i] = 0;
			RamConfig_p->RAMNumSWTS[i] = 0;
		}

		//selecting for each block, on which RAM they are
		for (i = 0; i < STFE_Config.NumIBs; i++) {
			reg_value = (stptiSupport_ReadReg32((volatile U32 *)&(STFE_SYSMappedAddress_p->IB_CFG[i].INFO)));
			RamToBeUsed = ((reg_value >> STFEv2_RAM_NUM_SHIFT) & STFEv2_CFG_BLOCK_MASK);
			RamConfig_p->RAMNumIB[RamToBeUsed]++;
		}

		for (i = 0; i < STFE_Config.NumSWTSs; i++) {
			reg_value = (stptiSupport_ReadReg32((volatile U32 *)&(STFE_SYSMappedAddress_p->SWTS_CFG[i].INFO)));
			RamToBeUsed = ((reg_value >> STFEv2_RAM_NUM_SHIFT) & STFEv2_CFG_BLOCK_MASK);
			RamConfig_p->RAMNumSWTS[RamToBeUsed]++;
		}

		for (i = 0; i < NumMIbCell; i++) {
			U32 Mib_streams = 0;
			reg_value = (stptiSupport_ReadReg32((volatile U32 *)&(STFE_SYSMappedAddress_p->MIB_CFG[i].INFO)));

			RamToBeUsed = ((reg_value >> STFEv2_RAM_NUM_SHIFT) & STFEv2_CFG_BLOCK_MASK);
			Mib_streams = (reg_value >> STFEv2_NUM_SUBSTREAMS_SHIFT) & STFEv2_CFG_BLOCK_MASK;
			RamConfig_p->RAMNumMIB[RamToBeUsed] += Mib_streams;
		}

		for (i = 0; i < RamConfig_p->NumRam; i++) {
			U32 temp_ram_addr = RamConfig_p->RAMAddr[i];

			if (temp_ram_addr % 0x40) {
				temp_ram_addr += 0x40;
				temp_ram_addr &= ~0x3F;
			}

			RamConfig_p->RAMSize[i] -= (temp_ram_addr - RamConfig_p->RAMAddr[i]);
			RamConfig_p->RAMAddr[i] = temp_ram_addr;
			RamConfig_p->FifoSize[i] = ((RamConfig_p->RAMSize[i]) / (RamConfig_p->RAMNumIB[i] + RamConfig_p->RAMNumMIB[i] + RamConfig_p->RAMNumSWTS[i])) & ~0x3f;
		}

		// MEMDMA FW LOADING
#if !defined( TANGOSIM ) && !defined( TANGOSIM2 ) && !defined(CONFIG_STM_VIRTUAL_PLATFORM)
		stptiTSHAL_StfeMemdmaFWLoad((stptiHAL_STFEv2_DmaBlockDevice_t *)((U32) STFE_Config.PhysicalAddress + (U32) STFE_Config.MemDMAOffset));
#endif

		reg_value = stptiSupport_ReadReg32((volatile U32 *)&(STFE_DMASW_MappedAddress_p->MEMDMA_PTRREC_BASE));
		RamConfig_p->RAMPointerPhyAddr = ((U32) STFE_Config.PhysicalAddress + (U32) STFE_Config.MemDMAOffset + dma_dmem_offset) + (U32) reg_value;

		reg_value = stptiSupport_ReadReg32((volatile U32 *)&(STFE_DMASW_MappedAddress_p->MEMDMA_PTRREC_OFFSET));
		RamConfig_p->RAMPtrBlock = reg_value;

		RamConfig_p->RAMPtrSize = (STFE_Config.NumIBs + STFE_Config.NumMIBs) * reg_value;

		RamConfig_p->NumPtrRecords = STFE_Config.NumIBs + STFE_Config.NumMIBs;
	}

	{
		U32 MemDMA_Channel = STFE_DMASW_MappedAddress_p->MEMDMA_FW_CFG & 0xFF;
		U32 TPs = (STFE_DMASW_MappedAddress_p->MEMDMA_FW_CFG >> 8) & 0xFF;

		if ((MemDMA_Channel != (NMIBs + NIBs)) || (TPs != NTPs)) {
			STPTI_PRINTF("ERROR: THIS IS THE INCORRECT MEMDMA FW");
			STPTI_PRINTF("IT WORKS FOR %d MEMDMA CH and %d TP", MemDMA_Channel, TPs);
			STPTI_PRINTF("BUT THERE ARE %d MEMDMA CH and %d TP", (NMIBs + NIBs), NTPs);
		} else {
			STPTI_PRINTF("THIS IS THE RIGHT MEMDMA FW");
			STPTI_PRINTF("THERE ARE %d MEMDMA CH and %d TP", MemDMA_Channel, TPs);
		}
		*pN_TPs = TPs;
	}
	//***** unmap the system registry  *****/
	iounmap(STFE_SYSMappedAddress_p);
	iounmap(STFE_DMASW_MappedAddress_p);
	iounmap(STFE_DMACPU_MappedAddress_p);

	return 0;
}

/**********************************************************
 *
 *  	TSDMA REGISTERS
 *
 **********************************************************/
void stptiTSHAL_Stfev2_TsdmaSetBasePointer(stptiHAL_STFE_TSDmaBlockDevice_t *TSDmaBlockBase_p)
{
	STFE_BasePointers.TSDmaBlockBase.TSDMA_CHANNEL =
	    (stptiHAL_STFE_TSDmaBlockChannel_t *) ((U32) TSDmaBlockBase_p + (U32) TSDMA_CHANNEL_OFFSET_STFEv2);
	STFE_BasePointers.TSDmaBlockBase.TSDMA_OUTPUT =
	    (stptiHAL_STFE_TSDmaBlockOutput_t *) ((U32) TSDmaBlockBase_p + (U32) TSDMA_OUTPUT_OFFSET_STFEv2);
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
void stptiTSHAL_Stfev2_TsdmaEnableInputRoute(U32 ChannelIndex, U32 OutputBlockIndex, BOOL Enable)
{
	if (STFE_BasePointers.TSDmaBlockBase.TSDMA_CHANNEL != NULL && (ChannelIndex < STFE_MAX_TSDMA_BLOCK_SIZE)
	    && (OutputBlockIndex < STFE_Config.NumTSDMAs)) {
		U32 route_bank = (ChannelIndex >> 5) & 0x1;
		U32 channel_index = (ChannelIndex & 0x1F);
		stpti_printf("ChannelIndex %d OutputBlockIndex %d Enable %d", ChannelIndex, OutputBlockIndex, Enable);

		stptiSupport_ReadModifyWriteReg32((volatile U32 *)&(STFE_BasePointers.TSDmaBlockBase.TSDMA_OUTPUT[OutputBlockIndex].STFE_TSDmaMixedReg.STFEv2_TSDmaMixedReg.DEST_ROUTE[route_bank]),
						  (1 << channel_index), (Enable ? (1 << channel_index) : 0));
	}
}

/**
   @brief  STFE TSDMA Hardware worker function

   Configure Out Channel to TSDMA - Currently only one channel but will be more on future devices.

   @param  TSInputHWMapping_p Pointer to mapped registers
   @param  Data               U32 value to write to register
   @return                    none
 */
void stptiTSHAL_Stfev2_TsdmaConfigureDestFormat(U32 OutputIndex, U32 Data)
{
	if (STFE_BasePointers.TSDmaBlockBase.TSDMA_CHANNEL != NULL && (OutputIndex < STFE_Config.NumTSDMAs)) {
		stpti_printf("Data 0x%x", Data);

		stptiSupport_WriteReg32((volatile U32 *)&(STFE_BasePointers.TSDmaBlockBase.TSDMA_OUTPUT[OutputIndex].DEST_FORMAT_STFEv2), Data);
	}
}

/**
   @brief  STFE TSDMA Hardware worker function

 */
void stptiTSHAL_Stfev2_TsdmaDumpMixedReg(U32 TSOutputIndex, stptiSUPPORT_sprintf_t * ctx_p)
{
	stptiSUPPORT_sprintf(ctx_p, "DEST_ROUTE[1]     :  0x%08X\n",
			     stptiSupport_ReadReg32(&(STFE_BasePointers.TSDmaBlockBase.TSDMA_OUTPUT[TSOutputIndex].STFE_TSDmaMixedReg.STFEv2_TSDmaMixedReg.DEST_ROUTE[0])));
	stptiSUPPORT_sprintf(ctx_p, "DEST_ROUTE[2]     :  0x%08X\n",
			     stptiSupport_ReadReg32(&(STFE_BasePointers.TSDmaBlockBase.TSDMA_OUTPUT[TSOutputIndex].STFE_TSDmaMixedReg.STFEv2_TSDmaMixedReg.DEST_ROUTE[1])));
	stptiSUPPORT_sprintf(ctx_p, "DEST_FORMAT    :  0x%08X\n",
			     stptiSupport_ReadReg32(&(STFE_BasePointers.TSDmaBlockBase.TSDMA_OUTPUT[TSOutputIndex].DEST_FORMAT_STFEv2)));
}

/**********************************************************
 *
 *  	SYSTEM REGISTERS
 *
 **********************************************************/

void stptiTSHAL_Stfev2_SystemInit(void)
{
	U32 Error_InterruptNumber = STFE_Config.Error_InterruptNumber;

	if (Error_InterruptNumber) {

		if (request_irq(Error_InterruptNumber, stptiHAL_ErrorInterruptHandler,
				IRQF_DISABLED, "Error_MBX", STFE_BasePointers.SystemBlockBase_p)) {
			STPTI_PRINTF_ERROR("ERROR TSIN IRS installation failed on number 0x%08x.",
					   Error_InterruptNumber);
		} else {
			STPTI_PRINTF("ERROR TSIN ISR error installed on number 0x%08x.", Error_InterruptNumber);
		}

		stptiSupport_WriteReg32((volatile U32 *)&(STFE_BasePointers.SystemBlockBase_p->STFEv2_SystemBlockDevice.INPUT_MASK[0]),
					0xFFFFFFFF);
		stptiSupport_WriteReg32((volatile U32 *)&(STFE_BasePointers.SystemBlockBase_p->STFEv2_SystemBlockDevice.OTHER_MASK[0]),
					0xFFFFFFFF);
	}
	stptiSupport_WriteReg32((
			&(STFE_BasePointers.SystemBlockBase_p->STFEv2_SystemBlockDevice.INPUT_CLK_ENABLE[0])),
			STFE_Config.tsin_enabled);
}

void stptiTSHAL_Stfev2_SystemTerm(void)
{
#if !defined( TANGOSIM ) && !defined( TANGOSIM2 )

	U32 Error_InterruptNumber = STFE_Config.Error_InterruptNumber;
	if (Error_InterruptNumber) {
		stptiSupport_WriteReg32((volatile U32 *)&(STFE_BasePointers.SystemBlockBase_p->STFEv2_SystemBlockDevice.INPUT_MASK[0]),
					0x0);
		stptiSupport_WriteReg32((volatile U32 *)&(STFE_BasePointers.SystemBlockBase_p->STFEv2_SystemBlockDevice.OTHER_MASK[0]),
					0x0);

		//1 - install the IRQ
		disable_irq(Error_InterruptNumber);
		free_irq(Error_InterruptNumber, STFE_BasePointers.SystemBlockBase_p);
		STPTI_PRINTF("ERROR TSIN ISR error uninstalled on number 0x%08x.", Error_InterruptNumber);
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
void stptiTSHAL_Stfev2_SystemIBRoute(U32 Index, stptiTSHAL_TSInputDestination_t Destination)
{
	if (STFE_BasePointers.SystemBlockBase_p != NULL && (Index < MAX_NUMBER_OF_TSINPUTS)) {
		stpti_printf("");

		stptiSupport_ReadModifyWriteReg32((volatile U32 *)&(STFE_BasePointers.SystemBlockBase_p->STFEv2_SystemBlockDevice.DMA_ROUTE),
						  (1 << Index),
						  ((Destination != stptiTSHAL_TSINPUT_DEST_DEMUX) ? (1 << Index) : 0));
	}
}

/**********************************************************
 *
 *  	PID FILTER REGISTERS
 *
 **********************************************************/
void stptiTSHAL_Stfev2_EnableLeakyPid(U32 STFE_InputChannel, BOOL EnableLeakyPID)
{
	if (STFE_BasePointers.PidFilterBaseAddressPhys_p != NULL) {
		U32 PidF_bank = (STFE_InputChannel >> 5) & 0x1;
		U32 PidF_index = (STFE_InputChannel & 0x1F);

		if (EnableLeakyPID)
			stptiSupport_ReadModifyWriteReg32(&(STFE_BasePointers.PidFilterBaseAddressPhys_p->STFE_PidLeakFilter.STFEv2_PidLeakFilter.PIDF_LEAK_ENABLE[PidF_bank]),
							  1 << PidF_index, 1 << PidF_index);
		else
			stptiSupport_ReadModifyWriteReg32(&(STFE_BasePointers.PidFilterBaseAddressPhys_p->STFE_PidLeakFilter.STFEv2_PidLeakFilter.PIDF_LEAK_ENABLE[PidF_bank]),
							  1 << PidF_index, 0);
	}
}

void stptiTSHAL_Stfev2_LeakCountReset(void)
{
	/* This sets the rate, but does enable it.  It is enabled in stptiTSHAL_StfeChangeActivityState() when a pid is set */
	stptiSupport_WriteReg32(&(STFE_BasePointers.PidFilterBaseAddressPhys_p->STFE_PidLeakFilter.STFEv2_PidLeakFilter.PIDF_LEAK_COUNT_RESET), STFE_LEAK_COUNT_RESET);	/* (gray coded value) Let through 1 in every 1000 packets */
}

/*************************************
 *
 * function not implemented in this version of the MEMDMA
 *
 *************************************/
void stptiTSHAL_Stfev2_MemdmaClearDREQLevel(U32 Index, U32 BusSize)
{
	// avoid warnings
	Index = Index;
	BusSize = BusSize;
}

U32 stptiTSHAL_Stfev2_Memdma_ram2beUsed(U32 Index, stptiHAL_TSInputStfeType_t IBType, U32 * pIndexInRam)
{
	U32 RamtoBeUsed = 0xFFFF;
	U32 i = 0;
	*pIndexInRam = Index;

	switch (IBType) {
	case (stptiTSHAL_TSINPUT_STFE_IB):
		{
			RamtoBeUsed = (((stptiSupport_ReadReg32(
				(volatile U32 *)&(STFE_BasePointers.SystemBlockBase_p->STFEv2_SystemBlockDevice.IB_CFG[Index].INFO))) >> STFEv2_RAM_NUM_SHIFT) & STFEv2_CFG_BLOCK_MASK);

			for (i = 0; i < RamtoBeUsed; i++) {
				(*pIndexInRam) -= STFE_Config.RamConfig.RAMNumIB[i];
			}
		}
		break;
	case (stptiTSHAL_TSINPUT_STFE_MIB):
		{
			U32 MibCell = 0, MibCellMax = 0;
			MibCellMax = stptiSupport_ReadReg32((volatile U32 *)&(STFE_BasePointers.SystemBlockBase_p->STFEv2_SystemBlockDevice.NUM_MIB));

			while (MibCell < MibCellMax) {
				if (Index < (((stptiSupport_ReadReg32((volatile U32 *)&(STFE_BasePointers.SystemBlockBase_p->STFEv2_SystemBlockDevice.MIB_CFG[MibCell].INFO))) >> STFEv2_NUM_SUBSTREAMS_SHIFT) & STFEv2_CFG_BLOCK_MASK)) {
					RamtoBeUsed = (((stptiSupport_ReadReg32((volatile U32 *)
						&(STFE_BasePointers.SystemBlockBase_p->STFEv2_SystemBlockDevice.SWTS_CFG[MibCell].INFO))) >> STFEv2_RAM_NUM_SHIFT) & STFEv2_CFG_BLOCK_MASK);
					break;
				} else {
					MibCell++;
					Index -= (((stptiSupport_ReadReg32((volatile U32 *)&(STFE_BasePointers.SystemBlockBase_p->STFEv2_SystemBlockDevice.MIB_CFG[MibCell].INFO))) >> STFEv2_NUM_SUBSTREAMS_SHIFT) & STFEv2_CFG_BLOCK_MASK);
				}
			}

			for (i = 0; i < RamtoBeUsed; i++) {
				(*pIndexInRam) -= STFE_Config.RamConfig.RAMNumMIB[i];
			}
			(*pIndexInRam) += STFE_Config.RamConfig.RAMNumIB[RamtoBeUsed];
		}
		break;
	case (stptiTSHAL_TSINPUT_STFE_SWTS):
		{
			RamtoBeUsed = (((stptiSupport_ReadReg32((volatile U32 *)
				&(STFE_BasePointers.SystemBlockBase_p->STFEv2_SystemBlockDevice.SWTS_CFG[Index].INFO))) >> STFEv2_RAM_NUM_SHIFT) & STFEv2_CFG_BLOCK_MASK);

			for (i = 0; i < RamtoBeUsed; i++) {
				(*pIndexInRam) -= STFE_Config.RamConfig.RAMNumSWTS[i];
			}
			(*pIndexInRam) += STFE_Config.RamConfig.RAMNumIB[RamtoBeUsed] +
			    STFE_Config.RamConfig.RAMNumMIB[RamtoBeUsed];
		}
		break;
	default:
		RamtoBeUsed = 0xFFFF;
		break;
	}
	return RamtoBeUsed;
}

void stptiTSHAL_Stfev2_InputClockEnable(U32 Index,
				stptiHAL_TSInputStfeType_t IBType, BOOL Enable)
{
	U32 input_clock_value = 0, input_ib_mask = 0;
	uint32_t input_clken_value = STFE_Config.tsin_enabled;

	if (STFE_BasePointers.InputBlockBase_p != NULL && \
			STFE_BasePointers.MergedInputBlockBase_p != NULL\
			&& STFE_BasePointers.SWTSInputBlockBase_p != NULL &&\
			(Index < MAX_NUMBER_OF_TSINPUTS)) {
		stpti_printf("Index 0x%04x Type 0x%02x Enable 0x%02x"
			" Tsin_enabled 0x%x",
			Index, IBType, (U8) Enable,
			STFE_Config.tsin_enabled);

		switch (IBType) {
		case (stptiTSHAL_TSINPUT_STFE_IB):
			STFE_GET_IB_EN_MASK(STFE_Config.NumIBs,
					0, input_ib_mask);

			input_clock_value = ((1 << Index) & input_ib_mask);
			if (Enable == true)
				input_clken_value |= input_clock_value;
			else
				input_clken_value &= ~input_clock_value;
			stptiSupport_WriteReg32(
				(&(STFE_BasePointers.SystemBlockBase_p->STFEv2_SystemBlockDevice.INPUT_CLK_ENABLE[0])),
				input_clken_value);
			break;
		case (stptiTSHAL_TSINPUT_STFE_MIB):
			STFE_GET_IB_EN_MASK(STFE_Config.NumMIBs,
					STFE_Config.NumIBs, input_ib_mask);

			if ((Index == 0) && (STFE_Config.NumMIBs)) {
			input_clock_value = (1 << STFE_Config.NumIBs) \
						& input_ib_mask;
				if (Enable == true)
					input_clken_value |= input_clock_value;
				else
					input_clken_value &= ~input_clock_value;
				stptiSupport_WriteReg32(
					(&(STFE_BasePointers.SystemBlockBase_p->STFEv2_SystemBlockDevice.INPUT_CLK_ENABLE[0])),
					input_clken_value);
			}
			break;
		case (stptiTSHAL_TSINPUT_STFE_SWTS):
			STFE_GET_IB_EN_MASK(STFE_Config.NumSWTSs,
				(STFE_Config.NumIBs + STFE_Config.NumMIBs),
				input_ib_mask);

			input_clock_value = ((1 << (STFE_Config.NumIBs	+ \
						STFE_Config.NumMIBs)) << Index);
			if (Enable == true)
				input_clken_value |= input_clock_value;
			else
				input_clken_value &= ~input_clock_value;
			stptiSupport_WriteReg32((
				&(STFE_BasePointers.SystemBlockBase_p->STFEv2_SystemBlockDevice.INPUT_CLK_ENABLE[0])),
				input_clken_value);
			break;
		default:
			/* nothing to do */
			break;
		}
		STFE_Config.tsin_enabled = stptiSupport_ReadReg32(
				&(STFE_BasePointers.SystemBlockBase_p->STFEv2_SystemBlockDevice.INPUT_CLK_ENABLE[0]));
	}
}

void stptiTSHAL_Stfev2_Write_RamInputNode_ptrs(U32 MemDmaChannel, U8 *RAMStart_p,
					       stptiSupport_DMAMemoryStructure_t RAMCachedMemoryStructure,
					       U32 BufferSize, U32 PktSize, U32 FifoSize, U32 InternalBase)
{
	U32 i = 0;
	stptiTSHAL_InputResource_t *TSInputHWMapping_p = stptiTSHAL_GetHWMapping();
	U32 Input_offset = STFE_Config.RamConfig.RAMPtrBlock;
	stptiHAL_STFEv2_InputNode_t *STFE_RAM_Mapped_InputNode_p =
	    (stptiHAL_STFEv2_InputNode_t *) (TSInputHWMapping_p->StfeRegs.STFE_RAM_Ptr_MappedAddress_p +
					     Input_offset * MemDmaChannel);

	stptiSupport_WriteReg32((volatile U32 *)&(STFE_RAM_Mapped_InputNode_p->MEM_BASE), InternalBase);
	stptiSupport_WriteReg32((volatile U32 *)&(STFE_RAM_Mapped_InputNode_p->MEM_TOP), InternalBase + FifoSize - 1);
	stptiSupport_WriteReg32((volatile U32 *)&(STFE_RAM_Mapped_InputNode_p->TS_PKT_SIZE), PktSize);
	stptiSupport_WriteReg32((volatile U32 *)&(STFE_RAM_Mapped_InputNode_p->ENABLED_TP), 0);

	for (i = 0; i < STFE_Config.NumTPUsed; i++) {
		U32 Base = (U32) stptiSupport_VirtToPhys(&RAMCachedMemoryStructure, &RAMStart_p[i * BufferSize]);
		U32 Top = (U32) stptiSupport_VirtToPhys(&RAMCachedMemoryStructure, &RAMStart_p[i * BufferSize] + (BufferSize) - 1);

		stptiSupport_WriteReg32((volatile U32 *)&(STFE_RAM_Mapped_InputNode_p->TP_BUFFER_p[i].TP_BASE), Base);
		stptiSupport_WriteReg32((volatile U32 *)&(STFE_RAM_Mapped_InputNode_p->TP_BUFFER_p[i].TP_TOP), Top);
		stptiSupport_WriteReg32((volatile U32 *)&(STFE_RAM_Mapped_InputNode_p->TP_BUFFER_p[i].TP_WP), Base);
		stptiSupport_WriteReg32((volatile U32 *)&(STFE_RAM_Mapped_InputNode_p->TP_BUFFER_p[i].TP_RP), Base);
	}
}

/**
   @brief  STFE Hardware worker function
stptiTSHAL_StfeMemdmaInit
   Write the DMA base address location in the internal STFE RAM

   @param  Address            Internal RAM address
   @return                    none
 */
void stptiTSHAL_Stfev2_MemdmaInit(void)
{
	U32 MemDma_InterruptNumber = STFE_Config.Idle_InterruptNumber;

	//1 - install the IRQ for the MEMDMA
	sema_init(&MemDmaSemaphore, 0);

	if (request_irq(MemDma_InterruptNumber, stptiHAL_MemDmaInterruptHandler,
			IRQF_DISABLED, "MemDma_MBX", STFE_BasePointers.DmaBlockBase_p)) {
		STPTI_PRINTF_ERROR("MEMDMA ISR installation failed on number 0x%08x.", MemDma_InterruptNumber);
	} else {
		STPTI_PRINTF("MEMDMA ISR installed on number 0x%08x.", MemDma_InterruptNumber);
	}
	//2- Start the DMA
	stptiSupport_WriteReg32((volatile U32 *)
				&(STFE_BasePointers.DmaBlockBase_p->STFEv2_DmaBlockDevice.MEMDMA_PER.MEMDMA_PER_STBUS_SYNC), 1);
	stptiSupport_WriteReg32((volatile U32 *)
				&(STFE_BasePointers.DmaBlockBase_p->STFEv2_DmaBlockDevice.MEMDMA_CPU.MEMDMA_CPU.MEMDMA_CPU_RUN), 1);
}

void stptiTSHAL_Stfev2_MemdmaTerm(void)
{
#if !defined( TANGOSIM ) && !defined( TANGOSIM2 )

	U32 MemDma_InterruptNumber = STFE_Config.Idle_InterruptNumber;
	// 1 reset the DMA CPU
	/* 1.1 stop Mem DMA CPU*/
	stptiSupport_WriteReg32((volatile U32 *)&(STFE_BasePointers.DmaBlockBase_p->STFEv2_DmaBlockDevice.MEMDMA_CPU.MEMDMA_CPU.MEMDMA_CPU_RUN), 0);

	/* 1.2 stop the CPU pipe-line */
	stptiSupport_WriteReg32((volatile U32 *)(&(STFE_BasePointers.DmaBlockBase_p->STFEv2_DmaBlockDevice.MEMDMA_CPU.MEMDMA_CPU.MEMDMA_CPU_RUN)+0x01), 1);

	/* 1.3 Reset the CPU pipe-line*/
	stptiSupport_WriteReg32((volatile U32 *)(&(STFE_BasePointers.DmaBlockBase_p->STFEv2_DmaBlockDevice.MEMDMA_CPU.MEMDMA_CPU.MEMDMA_CPU_RUN)+0x01), 4);
	//2 - dis-install the IRQ for the MEMDMA
	disable_irq(MemDma_InterruptNumber);
	free_irq(MemDma_InterruptNumber, STFE_BasePointers.DmaBlockBase_p);
	STPTI_PRINTF("MEMDMA ISR uninstalled on number 0x%08x.", MemDma_InterruptNumber);
#endif
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
ST_ErrorCode_t stptiTSHAL_Stfev2_MemdmaChangeSubstreamStatus(U32 MemDmaChannel, U32 TPMask, BOOL status)
{
	ST_ErrorCode_t Error = ST_NO_ERROR;
	stptiTSHAL_InputResource_t *TSInputHWMapping_p = stptiTSHAL_GetHWMapping();
	U32 Input_offset = STFE_Config.RamConfig.RAMPtrBlock;
	stptiHAL_STFEv2_InputNode_t *STFE_RAM_Mapped_InputNode_p =
	    (stptiHAL_STFEv2_InputNode_t *) (TSInputHWMapping_p->StfeRegs.STFE_RAM_Ptr_MappedAddress_p +
					     Input_offset * MemDmaChannel);

	if (status == FALSE) {
		stptiSupport_ReadModifyWriteReg32((volatile U32 *)&(STFE_RAM_Mapped_InputNode_p->ENABLED_TP), TPMask, 0x0);

		if (STFE_BasePointers.DmaBlockBase_p != NULL) {
			stptiSupport_WriteReg32((volatile U32 *)
						&(STFE_BasePointers.DmaBlockBase_p->STFEv2_DmaBlockDevice.MEMDMA_DMEM.MEMDMA_SW.MEMDMA_IDLE_IRQ),
						(MemDmaChannel | (1 << 31)));

			if (down_timeout(&MemDmaSemaphore, msecs_to_jiffies(10000))) {
				Error = STPTI_ERROR_TSINPUT_HW_ACCESS_ERROR;
			}

		} else {
			Error = STPTI_ERROR_TSINPUT_HW_ACCESS_ERROR;
		}
	} else {
		stptiSupport_ReadModifyWriteReg32((volatile U32 *)&(STFE_RAM_Mapped_InputNode_p->ENABLED_TP), 0x0, TPMask);
	}

	return Error;
}

void stptiTSHAL_Stfev2_MemDmaRegisterDump(U32 MemDmaIndex, BOOL Verbose, stptiSUPPORT_sprintf_t * ctx_p)
{
	int i;
	U32 addr = (U32) &STFE_BasePointers.DmaBlockBase_p->STFEv2_DmaBlockDevice.MEMDMA_DMEM.MEMDMA_DMEM[0];
	U32 InternalBaseEnd =
	    (U32) &STFE_BasePointers.DmaBlockBase_p->STFEv2_DmaBlockDevice.MEMDMA_DMEM.MEMDMA_DMEM[0x1000];
	U32 InternalBase = addr;

	MemDmaIndex = MemDmaIndex;

	stptiSUPPORT_sprintf(ctx_p, "\t FIRMWARE REVISION \t 0x%08X:\n",
			     STFE_BasePointers.DmaBlockBase_p->STFEv2_DmaBlockDevice.MEMDMA_DMEM.MEMDMA_SW.MEMDMA_FW_V);
	stptiSUPPORT_sprintf(ctx_p, "\t FIRMWARE CONFIG   \t 0x%08X:\n",
			     STFE_BasePointers.DmaBlockBase_p->STFEv2_DmaBlockDevice.MEMDMA_DMEM.MEMDMA_SW.
			     MEMDMA_FW_CFG);
	stptiSUPPORT_sprintf(ctx_p, "-------------------------------------------------\n");

	if (Verbose) {
		if (addr) {
			stptiSUPPORT_sprintf(ctx_p, "DUMP OF THE MEMDMA DMEM REG\n");
			while (addr < (U32) InternalBaseEnd) {
				stptiSUPPORT_sprintf(ctx_p, "0x%08X ", (addr - InternalBase));
				for (i = 0; i < 8; i++) {
					U32 c = 0;
					for (c = 0; c < 4; c++) {
						stptiSUPPORT_sprintf(ctx_p, "%02x", ((U8 *)addr)[c]);
					}
					stptiSUPPORT_sprintf(ctx_p, " ");
					addr += 4;
				}
				stptiSUPPORT_sprintf(ctx_p, "\n");
			}
			stptiSUPPORT_sprintf(ctx_p, "-------------------------------------------------\n");
		}
		stptiSUPPORT_sprintf(ctx_p, "--------------------\n");
		stptiSUPPORT_sprintf(ctx_p, "\t MEMDMA CPU REG:\n");
		for (i = 0; i < 20; i++) {
			stptiSUPPORT_sprintf(ctx_p, "\t CPU 0x%02X: \t 0x%08X\n", i * 4,
					     STFE_BasePointers.DmaBlockBase_p->STFEv2_DmaBlockDevice.MEMDMA_CPU.
					     padding[i]);
		}
		stptiSUPPORT_sprintf(ctx_p, "--------------------\n");
		stptiSUPPORT_sprintf(ctx_p, "\t MEMDMA RF REG:\n");
		for (i = 0; i < 16; i++) {
			stptiSUPPORT_sprintf(ctx_p, "\t RF 0x%02X: \t 0x%08X\n", i * 4,
					     STFE_BasePointers.DmaBlockBase_p->STFEv2_DmaBlockDevice.MEMDMA_RF[i]);
		}
		stptiSUPPORT_sprintf(ctx_p, "--------------------\n");
		stptiSUPPORT_sprintf(ctx_p, "\t MEMDMA PER REG:\n");
		for (i = 0; i < STFE_Config.NumTP; i++) {
			stptiSUPPORT_sprintf(ctx_p, "\t TPx Dreq 0x%02X:\t 0x%08X\t 0x%08X\n", i,
					     STFE_BasePointers.DmaBlockBase_p->STFEv2_DmaBlockDevice.MEMDMA_PER.
					     MEMDMA_PER_TP_DREQ[2 * i],
					     STFE_BasePointers.DmaBlockBase_p->STFEv2_DmaBlockDevice.MEMDMA_PER.
					     MEMDMA_PER_TP_DREQ[2 * i + 1]);
		}
		for (i = 0; i < STFE_Config.NumTP; i++) {
			stptiSUPPORT_sprintf(ctx_p, "\t TPx Dack 0x%02X:\t 0x%08X\t 0x%08X\n", i,
					     STFE_BasePointers.DmaBlockBase_p->STFEv2_DmaBlockDevice.MEMDMA_PER.
					     MEMDMA_PER_TP_DACK[2 * i],
					     STFE_BasePointers.DmaBlockBase_p->STFEv2_DmaBlockDevice.MEMDMA_PER.
					     MEMDMA_PER_TP_DACK[2 * i + 1]);
		}

		stptiSUPPORT_sprintf(ctx_p, "\t STBUS_SYNC: \t 0x%08X\n",
				     STFE_BasePointers.DmaBlockBase_p->STFEv2_DmaBlockDevice.MEMDMA_PER.
				     MEMDMA_PER_STBUS_SYNC);
		stptiSUPPORT_sprintf(ctx_p, "\t STBUS_ACCESS:\t 0x%08X\n",
				     STFE_BasePointers.DmaBlockBase_p->STFEv2_DmaBlockDevice.MEMDMA_PER.
				     MEMDMA_PER_STBUS_ACCESS);
		stptiSUPPORT_sprintf(ctx_p, "\t STBUS_ADDRESS:\t 0x%08X\n",
				     STFE_BasePointers.DmaBlockBase_p->STFEv2_DmaBlockDevice.MEMDMA_PER.
				     MEMDMA_PER_STBUS_ADDRESS);
		stptiSUPPORT_sprintf(ctx_p, "\t IDLE_INT: \t 0x%08X\n",
				     STFE_BasePointers.DmaBlockBase_p->STFEv2_DmaBlockDevice.MEMDMA_PER.
				     MEMDMA_PER_STBUS_IDLE_INT);
		stptiSUPPORT_sprintf(ctx_p, "\t PRIORITY: \t 0x%08X\n",
				     STFE_BasePointers.DmaBlockBase_p->STFEv2_DmaBlockDevice.MEMDMA_PER.
				     MEMDMA_PER_PRIORITY);

		stptiSUPPORT_sprintf(ctx_p, "\t MAX_OPCODE: \t 0x%08X\n",
				     STFE_BasePointers.DmaBlockBase_p->STFEv2_DmaBlockDevice.MEMDMA_PER.
				     MEMDMA_PER_MAX_OPCODE);
		stptiSUPPORT_sprintf(ctx_p, "\t MAX_CHUNCK: \t 0x%08X\n",
				     STFE_BasePointers.DmaBlockBase_p->STFEv2_DmaBlockDevice.MEMDMA_PER.
				     MEMDMA_PER_MAX_CHUNCK);
		stptiSUPPORT_sprintf(ctx_p, "\t PAGE_SIZE: \t 0x%08X\n",
				     STFE_BasePointers.DmaBlockBase_p->STFEv2_DmaBlockDevice.MEMDMA_PER.
				     MEMDMA_PER_PAGE_SIZE);

		stptiSUPPORT_sprintf(ctx_p, "\t MBOX_STATUS:\t 0x%08X\t 0x%08X\n",
				     STFE_BasePointers.DmaBlockBase_p->STFEv2_DmaBlockDevice.MEMDMA_PER.
				     MEMDMA_PER_MBOX_STATUS[0],
				     STFE_BasePointers.DmaBlockBase_p->STFEv2_DmaBlockDevice.MEMDMA_PER.
				     MEMDMA_PER_MBOX_STATUS[1]);
		stptiSUPPORT_sprintf(ctx_p, "\t MBOX_SET:\t 0x%08X\t 0x%08X\n",
				     STFE_BasePointers.DmaBlockBase_p->STFEv2_DmaBlockDevice.MEMDMA_PER.
				     MEMDMA_PER_MBOX_SET[0],
				     STFE_BasePointers.DmaBlockBase_p->STFEv2_DmaBlockDevice.MEMDMA_PER.
				     MEMDMA_PER_MBOX_SET[1]);
		stptiSUPPORT_sprintf(ctx_p, "\t MBOX_CLEAR: \t 0x%08X\t 0x%08X\n",
				     STFE_BasePointers.DmaBlockBase_p->STFEv2_DmaBlockDevice.MEMDMA_PER.
				     MEMDMA_PER_MBOX_CLEAR[0],
				     STFE_BasePointers.DmaBlockBase_p->STFEv2_DmaBlockDevice.MEMDMA_PER.
				     MEMDMA_PER_MBOX_CLEAR[1]);
		stptiSUPPORT_sprintf(ctx_p, "\t MBOX_MASK: \t 0x%08X\t 0x%08X\n",
				     STFE_BasePointers.DmaBlockBase_p->STFEv2_DmaBlockDevice.MEMDMA_PER.
				     MEMDMA_PER_MBOX_MASK[0],
				     STFE_BasePointers.DmaBlockBase_p->STFEv2_DmaBlockDevice.MEMDMA_PER.
				     MEMDMA_PER_MBOX_MASK[1]);

		stptiSUPPORT_sprintf(ctx_p, "\t INJ_PKT_SRC:\t 0x%08X\n",
				     STFE_BasePointers.DmaBlockBase_p->STFEv2_DmaBlockDevice.MEMDMA_PER.
				     MEMDMA_PER_INJECT_PKT_SRC);
		stptiSUPPORT_sprintf(ctx_p, "\t INK_PKT_DEST:\t 0x%08X\n",
				     STFE_BasePointers.DmaBlockBase_p->STFEv2_DmaBlockDevice.MEMDMA_PER.
				     MEMDMA_PER_INKECT_PKT_DEST);
		stptiSUPPORT_sprintf(ctx_p, "\t INJ_PKT_ADDR:\t 0x%08X\n",
				     STFE_BasePointers.DmaBlockBase_p->STFEv2_DmaBlockDevice.MEMDMA_PER.
				     MEMDMA_PER_INJECT_PKT_ADDR);
		stptiSUPPORT_sprintf(ctx_p, "\t INJ_PKT:\t 0x%08X\n",
				     STFE_BasePointers.DmaBlockBase_p->STFEv2_DmaBlockDevice.MEMDMA_PER.
				     MEMDMA_PER_INJECT_PKT);
		stptiSUPPORT_sprintf(ctx_p, "\t PAT_PTR_INIT:\t 0x%08X\n",
				     STFE_BasePointers.DmaBlockBase_p->STFEv2_DmaBlockDevice.MEMDMA_PER.
				     MEMDMA_PER_PAT_PTR_INIT);
		stptiSUPPORT_sprintf(ctx_p, "\t PAT_PTR: \t 0x%08X\n",
				     STFE_BasePointers.DmaBlockBase_p->STFEv2_DmaBlockDevice.MEMDMA_PER.
				     MEMDMA_PER_PAT_PTR);
		stptiSUPPORT_sprintf(ctx_p, "\t SLP_MASK: \t 0x%08X\n",
				     STFE_BasePointers.DmaBlockBase_p->STFEv2_DmaBlockDevice.MEMDMA_PER.
				     MEMDMA_PER_SLEEP_MASK);
		stptiSUPPORT_sprintf(ctx_p, "\t SLP_COUNTER:\t 0x%08X\n",
				     STFE_BasePointers.DmaBlockBase_p->STFEv2_DmaBlockDevice.MEMDMA_PER.
				     MEMDMA_PER_SLEEP_COUNTER);
		stptiSUPPORT_sprintf(ctx_p, "-------------------------------------------------\n");
	}
}

void stptiTSHAL_Stfev2_DmaPTRRegisterDump(U32 MemDmaIndex, stptiSUPPORT_sprintf_t * ctx_p)
{
	U32 j = 0, addr = 0;

	stptiTSHAL_InputResource_t *TSInputHWMapping_p = stptiTSHAL_GetHWMapping();

	addr = (U32) (TSInputHWMapping_p->StfeRegs.STFE_RAM_Ptr_MappedAddress_p);

	if (addr) {
		stptiHAL_STFEv2_InputNode_t *STFE_InputNode_p =
		    (stptiHAL_STFEv2_InputNode_t *) ((U32) addr + (STFE_Config.RamConfig.RAMPtrBlock * MemDmaIndex));

		if (STFE_InputNode_p->MEM_TOP != 0) {
			volatile stptiHAL_STFE_bufferTp_t *TP_BUFFER_p = STFE_InputNode_p->TP_BUFFER_p;

			stptiSUPPORT_sprintf(ctx_p, "MEMDMA RAM POINTERS......\n");
			stptiSUPPORT_sprintf(ctx_p, "Input Block %d:\n", MemDmaIndex);

			stptiSUPPORT_sprintf(ctx_p, "\t Input Block MEM_BASE \t 0x%08X:\n", STFE_InputNode_p->MEM_BASE);
			stptiSUPPORT_sprintf(ctx_p, "\t Input Block MEM_TOP \t 0x%08X:\n", STFE_InputNode_p->MEM_TOP);
			stptiSUPPORT_sprintf(ctx_p, "\t packet Size \t\t %d:\n", STFE_InputNode_p->TS_PKT_SIZE);
			stptiSUPPORT_sprintf(ctx_p, "\t TP ENABLED  \t\t 0x%08X:\n\n", STFE_InputNode_p->ENABLED_TP);

			for (j = 0; j < STFE_Config.NumTP; j++) {
				stptiSUPPORT_sprintf(ctx_p, "\t TP_%d BASE \t 0x%08X:\n", j, TP_BUFFER_p[j].TP_BASE);
				stptiSUPPORT_sprintf(ctx_p, "\t TP_%d TOP \t 0x%08X:\n", j, TP_BUFFER_p[j].TP_TOP);
				stptiSUPPORT_sprintf(ctx_p, "\t TP_%d WP \t 0x%08X:\n", j, TP_BUFFER_p[j].TP_WP);
				stptiSUPPORT_sprintf(ctx_p, "\t TP_%d RP \t 0x%08X:\n\n", j, TP_BUFFER_p[j].TP_RP);
			}
		}
	}
	stptiSUPPORT_sprintf(ctx_p, "-------------------------------------------------\n");
}

void stptiTSHAL_Stfev2_SystemRegisterDump(stptiSUPPORT_sprintf_t * ctx_p)
{
	stptiSUPPORT_sprintf(ctx_p, "----------- STFE System debug -------------------\n");
	stptiSUPPORT_sprintf(ctx_p, "INPUT_STATUS :  0x%08X\n",
			     stptiSupport_ReadReg32(&(STFE_BasePointers.SystemBlockBase_p->STFEv2_SystemBlockDevice.INPUT_STATUS[0])));
	stptiSUPPORT_sprintf(ctx_p, "OTHER_STATUS :  0x%08X\n",
			     stptiSupport_ReadReg32(&(STFE_BasePointers.SystemBlockBase_p->STFEv2_SystemBlockDevice.OTHER_STATUS[0])));
	stptiSUPPORT_sprintf(ctx_p, "INPUT_MASK :    0x%08X\n",
			     stptiSupport_ReadReg32(&(STFE_BasePointers.SystemBlockBase_p->STFEv2_SystemBlockDevice.INPUT_MASK[0])));
	stptiSUPPORT_sprintf(ctx_p, "OTHER_MASK :    0x%08X\n",
			     stptiSupport_ReadReg32(&(STFE_BasePointers.SystemBlockBase_p->STFEv2_SystemBlockDevice.OTHER_MASK[0])));


	stptiSUPPORT_sprintf(ctx_p, "INPUT_CLKEN :    0x%08X\n",
			     stptiSupport_ReadReg32(&(STFE_BasePointers.SystemBlockBase_p->STFEv2_SystemBlockDevice.INPUT_CLK_ENABLE[0])));
	stptiSUPPORT_sprintf(ctx_p, "OTHER_CLKEN :    0x%08X\n",
			     stptiSupport_ReadReg32(&(STFE_BasePointers.SystemBlockBase_p->STFEv2_SystemBlockDevice.OTHER_CLK_ENABLE[0])));


	/* TODO Add all the other system registers here */
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
void stptiTSHAL_Stfev2_CCSCConfig(void)
{
	if (STFE_Config.NumCCSCs)
		stptiSupport_WriteReg32((volatile U32 *)&(STFE_BasePointers.CCSCBlockBase_p->CCSC_CTRL), STFE_CCSC_ENABLE);
}

/**
   @brief  Stfe Hardware worker function

   Configure the CCSC for bypass mode

   @param                     none
   @return                    none
 */
void stptiTSHAL_Stfev2_CCSCBypass(void)
{
	if (STFE_Config.NumCCSCs)
		stptiSupport_WriteReg32((volatile U32 *)&(STFE_BasePointers.CCSCBlockBase_p->CCSC_CTRL), STFE_CCSC_BYPASS);
}

/**
   @brief  Function to monitor the DREQ levels into memdma

   This function  monitor the DREQ levels into memdma. in this STFE version this function is empty

   @param  tp_read_pointers     Array containing the level to monitor (one for each channel).
   @param  num_rps    		The number of channel to monitor

   @return                    none
 */
void stptiTSHAL_Stfev2_MonitorDREQLevels(U32 * tp_read_pointers, U8 num_rps, U32 dma_pck_size)
{
	tp_read_pointers = tp_read_pointers;
	num_rps = num_rps;
	dma_pck_size = dma_pck_size;
}
