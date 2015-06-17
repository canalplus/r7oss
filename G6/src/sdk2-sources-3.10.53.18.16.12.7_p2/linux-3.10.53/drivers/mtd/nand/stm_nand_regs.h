/*
 *   drivers/mtd/nand/stm_nandc_regs.h
 *
 *   STMicroelectronics NAND Controller register definitions
 *   Applicable to:
 *	Standalone Hamming Contoller (FLEX mode & AFM)
 *	NANDi Hamming Controller (FLEX & AFM)
 *	NANDi BCH Controller (AFM)
 *
 *   Copyright (c) 2008-2011 STMicroelectronics Limited
 *   Author: Angus Clark <angus.clark@st.com>
 *
 *   May be copied or modified under the terms of the GNU General Public
 *   License.  See linux/COPYING for more information.
 *
 */

#ifndef STM_NANDC_REGS_H
#define STM_NANDC_REGS_H

/* Hamming Controller Registers (Offsets from EMINAND_BASE) */
#define NANDHAM_BOOTBANK_CFG				0x000
#define NANDHAM_RBN_STA				0x004
#define NANDHAM_INT_EN					0x010
#define NANDHAM_INT_STA				0x014
#define NANDHAM_INT_CLR				0x018
#define NANDHAM_INT_EDGE_CFG				0x01C
#define NANDHAM_CTL_TIMING				0x040
#define NANDHAM_WEN_TIMING				0x044
#define NANDHAM_REN_TIMING				0x048
#define NANDHAM_BLOCK_ZERO_REMAP_REG			0x04C
#define NANDHAM_FLEXMODE_CFG				0x100
#define NANDHAM_FLEX_MUXCTRL				0x104
#define NANDHAM_FLEX_DATAWRITE_CONFIG			0x10C
#define NANDHAM_FLEX_DATAREAD_CONFIG			0x110
#define NANDHAM_FLEX_CMD				0x114
#define NANDHAM_FLEX_ADD				0x118
#define NANDHAM_FLEX_DATA				0x120
#define NANDHAM_VERSION_REG				0x144
#define NANDHAM_MULTI_CS_CONFIG_REG			0x1EC
#define NANDHAM_AFM_SEQ_REG_1				0x200
#define NANDHAM_AFM_SEQ_REG_2				0x204
#define NANDHAM_AFM_SEQ_REG_3				0x208
#define NANDHAM_AFM_SEQ_REG_4				0x20C
#define NANDHAM_AFM_ADD				0x210
#define NANDHAM_AFM_EXTRA				0x214
#define NANDHAM_AFM_CMD				0x218
#define NANDHAM_AFM_SEQ_CFG				0x21C
#define NANDHAM_AFM_GEN_CFG				0x220
#define NANDHAM_AFM_SEQ_STA				0x240
#define NANDHAM_AFM_ECC_REG_0				0x280
#define NANDHAM_AFM_ECC_REG_1				0x284
#define NANDHAM_AFM_ECC_REG_2				0x288
#define NANDHAM_AFM_ECC_REG_3				0x28C
#define NANDHAM_AFM_DATA_FIFO				0x300

/* BCH Controller Registers (Offsets from EMI_NAND) */
#define NANDBCH_BOOTBANK_CFG				0x000
#define NANDBCH_RBN_STA				0x004
#define NANDBCH_INT_EN					0x010
#define NANDBCH_INT_STA				0x014
#define NANDBCH_INT_CLR				0x018
#define NANDBCH_INT_EDGE_CFG				0x01C
#define NANDBCH_CTL_TIMING				0x040
#define NANDBCH_WEN_TIMING				0x044
#define NANDBCH_REN_TIMING				0x048
#define NANDBCH_BLOCK_ZERO_REMAP_REG			0x04C
#define NANDBCH_BOOT_STATUS				0x050
#define NANDBCH_FALSE_BOOT_REG				0x054
#define NANDBCH_FALSE_BOOT_STATUS			0x058
#define NANDBCH_CONTROLLER_CFG				0x100
#define NANDBCH_FLEX_MUXCTRL				0x104
#define NANDBCH_FLEX_DATAWRITE_CONFIG			0x10C
#define NANDBCH_FLEX_DATAREAD_CONFIG			0x110
#define NANDBCH_VERSION_REG				0x144
#define NANDBCH_ADDRESS_REG_1				0x1F0
#define NANDBCH_ADDRESS_REG_2				0x1F4
#define NANDBCH_ADDRESS_REG_3				0x1F8
#define NANDBCH_MULTI_CS_CONFIG_REG			0x1FC
#define NANDBCH_SEQ_REG_1				0x200
#define NANDBCH_SEQ_REG_2				0x204
#define NANDBCH_SEQ_REG_3				0x208
#define NANDBCH_SEQ_REG_4				0x20C
#define NANDBCH_ADD					0x210
#define NANDBCH_EXTRA_REG				0x214
#define NANDBCH_CMD					0x218
#define NANDBCH_GEN_CFG				0x220
#define NANDBCH_DELAY_REG				0x224
#define NANDBCH_SEQ_CFG				0x22C
#define NANDBCH_SEQ_STA				0x270
#define NANDBCH_DATA_BUFFER_ENTRY_0			0x280
#define NANDBCH_DATA_BUFFER_ENTRY_1			0x284
#define NANDBCH_DATA_BUFFER_ENTRY_2			0x288
#define NANDBCH_DATA_BUFFER_ENTRY_3			0x28C
#define NANDBCH_DATA_BUFFER_ENTRY_4			0x290
#define NANDBCH_DATA_BUFFER_ENTRY_5			0x294
#define NANDBCH_DATA_BUFFER_ENTRY_6			0x298
#define NANDBCH_DATA_BUFFER_ENTRY_7			0x29C
#define NANDBCH_ECC_SCORE_REG_A			0x2A0
#define NANDBCH_ECC_SCORE_REG_B			0x2A4
#define NANDBCH_CHECK_STATUS_REG_A			0x2A8
#define NANDBCH_CHECK_STATUS_REG_B			0x2AC
#define NANDBCH_BUFFER_LIST_PTR			0x300
#define NANDBCH_SEQ_PTR_REG				0x304
#define NANDBCH_ERROR_THRESHOLD_REG			0x308

/* EMISS NAND BCH STPLUG Registers (Offsets from EMISS_NAND_DMA) */
#define EMISS_NAND_RD_DMA_PAGE_SIZE			0x000
#define EMISS_NAND_RD_DMA_MAX_OPCODE_SIZE		0x004
#define EMISS_NAND_RD_DMA_MIN_OPCODE_SIZE		0x008
#define EMISS_NAND_RD_DMA_MAX_CHUNK_SIZE		0x00C
#define EMISS_NAND_RD_DMA_MAX_MESSAGE_SIZE		0x010

#define EMISS_NAND_WR_DMA_PAGE_SIZE			0x100
#define EMISS_NAND_WR_DMA_MAX_OPCODE_SIZE		0x104
#define EMISS_NAND_WR_DMA_MIN_OPCODE_SIZE		0x108
#define EMISS_NAND_WR_DMA_MAX_CHUNK_SIZE		0x10C
#define EMISS_NAND_WR_DMA_MAX_MESSAGE_SIZE		0x110


/*
 * Hamming/BCH controller interrupts
 */

/* NANDxxx_INT_EN/NANDxxx_INT_STA */
/*      Common */
#define NAND_INT_ENABLE			(0x1 << 0)
#define NAND_INT_RBN				(0x1 << 2)
#define NAND_INT_SEQCHECK			(0x1 << 5)
/*      Hamming only */
#define NANDHAM_INT_DATA_DREQ			(0x1 << 3)
#define NANDHAM_INT_SEQ_DREQ			(0x1 << 4)
#define NANDHAM_INT_ECC_FIX_REQ		(0x1 << 6)
/*      BCH only */
#define NANDBCH_INT_SEQNODESOVER		(0x1 << 7)
#define NANDBCH_INT_ECCTHRESHOLD		(0x1 << 8)

/* NANDxxx_INT_CLR */
/*      Common */
#define NAND_INT_CLR_RBN			(0x1 << 2)
#define NAND_INT_CLR_SEQCHECK			(0x1 << 3)
/*      Hamming only */
#define NANDHAM_INT_CLR_ECC_FIX_REQ		(0x1 << 4)
#define NANDHAM_INT_CLR_DATA_DREQ		(0x1 << 5)
#define NANDHAM_INT_CLR_SEQ_DREQ		(0x1 << 6)
/*      BCH only */
#define NANDBCH_INT_CLR_SEQNODESOVER		(0x1 << 5)
#define NANDBCH_INT_CLR_ECCTHRESHOLD		(0x1 << 6)

/* NANDxxx_INT_EDGE_CFG */
#define NAND_EDGE_CFG_RBN_RISING		0x1
#define NAND_EDGE_CFG_RBN_FALLING		0x2
#define NAND_EDGE_CFG_RBN_ANY			0x3

/* NANDBCH_CONTROLLER_CFG/NANDHAM_FLEXMODE_CFG */
#define CFG_ENABLE_FLEX			0x1
#define CFG_ENABLE_AFM				0x2
#define CFG_RESET				(0x1 << 3)
#define CFG_RESET_ECC(x)			(0x1 << (7 + (x)))
#define CFG_RESET_ECC_ALL			(0xff << 7)


/*
 * BCH Controller
 */

/* ECC Modes */
#define BCH_18BIT_ECC				0
#define BCH_30BIT_ECC				1
#define BCH_NO_ECC				2
#define BCH_ECC_RSRV				3

/* NANDBCH_BOOTBANK_CFG */
#define BOOT_CFG_RESET				(0x1 << 3)

/* NANDBCH_CTL_TIMING */
#define NANDBCH_CTL_SETUP(x)			((x) & 0xff)
#define NANDBCH_CTL_HOLD(x)			(((x) & 0xff) << 8)
#define NANDBCH_CTL_WERBN(x)			(((x) & 0xff) << 24)

/* NANDBCH_WEN_TIMING */
#define NANDBCH_WEN_ONTIME(x)			((x) & 0xff)
#define NANDBCH_WEN_OFFTIME(x)			(((x) & 0xff) << 8)
#define NANDBCH_WEN_ONHALFCYCLE		(0x1 << 16)
#define NANDBCH_WEN_OFFHALFCYCLE		(0x1 << 17)

/* NANDBCH_REN_TIMING */
#define NANDBCH_REN_ONTIME(x)			((x) & 0xff)
#define NANDBCH_REN_OFFTIME(x)			(((x) & 0xff) << 8)
#define NANDBCH_REN_ONHALFCYCLE		(0x1 << 16)
#define NANDBCH_REN_OFFHALFCYCLE		(0x1 << 17)
#define NANDBCH_REN_TELQV(x)			(((x) & 0xff) << 24)

/* NANDBCH_BLOCK_ZERO_REMAP_REG */
#define NANDBCH_BACKUP_COPY_FOUND		(0x1 << 0)
#define NANDBCH_ORIG_CODE_CORRUPTED		(0x1 << 1)
#define NANDBCH_BLK_ZERO_REMAP(x)		((x) >> 14)

/* NANDBCH_BOOT_STATUS */
#define NANDBCH_BOOT_MAX_ERRORS(x)		((x) & 0x1f)

/* NANDBCH_GEN_CFG */
#define GEN_CFG_DATA_8_NOT_16			(0x1 << 16)
#define GEN_CFG_EXTRA_ADD_CYCLE		(0x1 << 18)
#define GEN_CFG_2X8_MODE			(0x1 << 19)
#define GEN_CFG_ECC_SHIFT			20
#define GEN_CFG_18BIT_ECC		(BCH_18BIT_ECC << GEN_CFG_ECC_SHIFT)
#define GEN_CFG_30BIT_ECC		(BCH_30BIT_ECC << GEN_CFG_ECC_SHIFT)
#define GEN_CFG_NO_ECC			(BCH_NO_ECC << GEN_CFG_ECC_SHIFT)
#define GEN_CFG_LAST_SEQ_NODE			(0x1 << 22)

/* NANDBCH_SEQ_CFG */
#define SEQ_CFG_REPEAT_COUNTER(x)		((x) & 0xffff)
#define SEQ_CFG_SEQ_IDENT(x)			(((x) & 0xff) << 16)
#define SEQ_CFG_DATA_WRITE			(0x1 << 24)
#define SEQ_CFG_ERASE				(0x1 << 25)
#define SEQ_CFG_GO_STOP			(0x1 << 26)

/* NANDBCH_SEQ_STA */
#define SEQ_STA_RUN				(0x1 << 4)

/*
 * BCH Commands
 */
#define BCH_OPC_STOP			0x0
#define BCH_OPC_CMD			0x1
#define BCH_OPC_INC			0x2
#define BCH_OPC_DEC_JUMP		0x3
#define BCH_OPC_DATA			0x4
#define BCH_OPC_DELAY			0x5
#define BCH_OPC_CHECK			0x6
#define BCH_OPC_ADDR			0x7
#define BCH_OPC_NEXT_CHIP_ON		0x8
#define BCH_OPC_DEC_JMP_MCS		0x9
#define BCH_OPC_ECC_SCORE		0xA

#define BCH_INSTR(opc, opr)		((opc) | ((opr) << 4))

#define BCH_CMD_ADDR			BCH_INSTR(BCH_OPC_CMD, 0)
#define BCH_CL_CMD_1			BCH_INSTR(BCH_OPC_CMD, 1)
#define BCH_CL_CMD_2			BCH_INSTR(BCH_OPC_CMD, 2)
#define BCH_CL_CMD_3			BCH_INSTR(BCH_OPC_CMD, 3)
#define BCH_CL_EX_0			BCH_INSTR(BCH_OPC_CMD, 4)
#define BCH_CL_EX_1			BCH_INSTR(BCH_OPC_CMD, 5)
#define BCH_CL_EX_2			BCH_INSTR(BCH_OPC_CMD, 6)
#define BCH_CL_EX_3			BCH_INSTR(BCH_OPC_CMD, 7)
#define BCH_INC(x)			BCH_INSTR(BCH_OPC_INC, (x))
#define BCH_DEC_JUMP(x)		BCH_INSTR(BCH_OPC_DEC_JUMP, (x))
#define BCH_STOP			BCH_INSTR(BCH_OPC_STOP, 0)
#define BCH_DATA_1_SECTOR		BCH_INSTR(BCH_OPC_DATA, 0)
#define BCH_DATA_2_SECTOR		BCH_INSTR(BCH_OPC_DATA, 1)
#define BCH_DATA_4_SECTOR		BCH_INSTR(BCH_OPC_DATA, 2)
#define BCH_DATA_8_SECTOR		BCH_INSTR(BCH_OPC_DATA, 3)
#define BCH_DATA_16_SECTOR		BCH_INSTR(BCH_OPC_DATA, 4)
#define BCH_DATA_32_SECTOR		BCH_INSTR(BCH_OPC_DATA, 5)
#define BCH_DELAY_0			BCH_INSTR(BCH_OPC_DELAY, 0)
#define BCH_DELAY_1			BCH_INSTR(BCH_OPC_DELAY, 1)
#define BCH_OP_ERR			BCH_INSTR(BCH_OPC_CHECK, 0)
#define BCH_CACHE_ERR			BCH_INSTR(BCH_OPC_CHECK, 1)
#define BCH_ERROR			BCH_INSTR(BCH_OPC_CHECK, 2)
#define BCH_AL_EX_0			BCH_INSTR(BCH_OPC_ADDR, 0)
#define BCH_AL_EX_1			BCH_INSTR(BCH_OPC_ADDR, 1)
#define BCH_AL_EX_2			BCH_INSTR(BCH_OPC_ADDR, 2)
#define BCH_AL_EX_3			BCH_INSTR(BCH_OPC_ADDR, 3)
#define BCH_AL_AD_0			BCH_INSTR(BCH_OPC_ADDR, 4)
#define BCH_AL_AD_1			BCH_INSTR(BCH_OPC_ADDR, 5)
#define BCH_AL_AD_2			BCH_INSTR(BCH_OPC_ADDR, 6)
#define BCH_AL_AD_3			BCH_INSTR(BCH_OPC_ADDR, 7)
#define BCH_NEXT_CHIP_ON		BCH_INSTR(BCH_OPC_NEXT_CHIP_ON, 0)
#define BCH_DEC_JMP_MCS(x)		BCH_INSTR(BCH_OPC_DEC_JMP_MCS, (x))
#define BCH_ECC_SCORE(x)		BCH_INSTR(BCH_OPC_ECC_SCORE, (x))


/*
 * Hamming-FLEX register fields
 */

/* NANDHAM_FLEX_DATAREAD/WRITE_CONFIG */
#define FLEX_DATA_CFG_WAIT_RBN			(0x1 << 27)
#define FLEX_DATA_CFG_BEATS_1			(0x1 << 28)
#define FLEX_DATA_CFG_BEATS_2			(0x2 << 28)
#define FLEX_DATA_CFG_BEATS_3			(0x3 << 28)
#define FLEX_DATA_CFG_BEATS_4			(0x0 << 28)
#define FLEX_DATA_CFG_BYTES_1			(0x0 << 30)
#define FLEX_DATA_CFG_BYTES_2			(0x1 << 30)
#define FLEX_DATA_CFG_CSN			(0x1 << 31)

/* NANDHAM_FLEX_CMD */
#define FLEX_CMD_RBN				(0x1 << 27)
#define FLEX_CMD_BEATS_1			(0x1 << 28)
#define FLEX_CMD_BEATS_2			(0x2 << 28)
#define FLEX_CMD_BEATS_3			(0x3 << 28)
#define FLEX_CMD_BEATS_4			(0x0 << 28)
#define FLEX_CMD_CSN				(0x1 << 31)
#define FLEX_CMD(x)				(((x) & 0xff) |	\
						 FLEX_CMD_RBN |	\
						 FLEX_CMD_BEATS_1 |	\
						 FLEX_CMD_CSN)
/* NANDHAM_FLEX_ADD */
#define FLEX_ADDR_RBN				(0x1 << 27)
#define FLEX_ADDR_BEATS_1			(0x1 << 28)
#define FLEX_ADDR_BEATS_2			(0x2 << 28)
#define FLEX_ADDR_BEATS_3			(0x3 << 28)
#define FLEX_ADDR_BEATS_4			(0x0 << 28)
#define FLEX_ADDR_ADD8_VALID			(0x1 << 30)
#define FLEX_ADDR_CSN				(0x1 << 31)

/*
 * Hamming-AFM register fields
 */
/* NANDHAM_AFM_SEQ_CFG */
#define AFM_SEQ_CFG_GO				(0x1 << 26)
#define AFM_SEQ_CFG_DIR_WRITE			(0x1 << 24)

/* NANDHAM_AFM_GEN_CFG */
#define AFM_GEN_CFG_DATA_8_NOT_16		(0x1 << 16)
#define AFM_GEN_CFG_LARGE_PAGE			(0x1 << 17)
#define AFM_GEN_CFG_EXTRA_ADD_CYCLE		(0x1 << 18)

/*
 * AFM Commands
 */
#define AFM_STOP			0x0
#define AFM_CMD				0x1
#define AFM_INC				0x2
#define AFM_DEC_JUMP			0x3
#define AFM_DATA			0x4
#define AFM_SPARE			0x5
#define AFM_CHECK			0x6
#define AFM_ADDR			0x7
#define AFM_WRBN			0xA

/* The ARM memcpy_toio routines are totally unoptimised and simply
 * do a byte loop. This causes a problem in that the NAND controller
 * can only support 32 bit reads and writes. On the SH4 memcpy_toio
 * is fully optimised and will always write as efficiently as possible,
 * using some cache optimisations so it is worth using
 */

static inline void stm_nand_memcpy_toio(volatile void __iomem *dst,
					const void *src, size_t count)
{
	BUG_ON(count & 3);
	BUG_ON((unsigned long)dst & 3);
	BUG_ON((unsigned long)src & 3);
#ifdef __arm__
	while (count) {
		writel_relaxed(*(u32 *)src, dst);
		src += 4;
		dst += 4;
		count -= 4;
	}
	mb();
#else
	memcpy_toio(dst, src, count);
#endif
}

static inline void stm_nand_memcpy_fromio(void *dst,
					  const volatile void __iomem *src,
					  int count)
{
	BUG_ON(count & 3);
	BUG_ON((unsigned long)dst & 3);
	BUG_ON((unsigned long)src & 3);
#ifdef __arm__
	while (count) {
		*(u32 *)dst = readl_relaxed(src);
		src += 4;
		dst += 4;
		count -= 4;
	}
	mb();
#else
	memcpy_fromio(dst, src, count);
#endif
}

#endif /* STM_NANDC_REGS_H */
