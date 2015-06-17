/***********************************************************************
 *
 * File: display/ip/displaytiming/stmflexclkgen_reg.h
 * Copyright (c) 2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef _STM_FLEX_CLK_GEN_REG_H
#define _STM_FLEX_CLK_GEN_REG_H

#define STM_FLEX_CLKGEN_DIVMUX_MUX            0x000 // DivMux mux select,3*4byte registers, 8 muxes per register (4bits of config each in ascending order)

#define STM_FLEX_CLKGEN_DIVMUX_DIV            0x00C // DivMux div,3*4byte registers, 8 muxes per register (4bits of config each in ascending order)

#define STM_FLEX_CLKGEN_CROSSBAR              0x018 // Crossbar, 16x4byte registers, 4 outputs per register (8bits of config each) in ascending order

#define STM_FLEX_CLKGEN_PRE_DIV_0             0x058 // Start of pre divider config block, 64x4byte registers, one per divider output

#define STM_FLEX_CLKGEN_FINAL_DIV_0           0x164 // Start of final divider config block, 64x4byte registers, one per divider output

#define STM_FLEX_CLKGEN_BEAT_COUNT_1_4        0x26C // Semi synchronous beat counters 1-4, 4x8bit 0 - asynch mode beat count 1, or val+1 gives a count between 2-256
#define STM_FLEX_CLKGEN_BEAT_COUNT_5_8        0x270 // Semi synchronous beat counters 5-8, 4x8bit 0 - asynch mode beat count 1, or val+1 gives a count between 2-256

#define STM_FLEX_CLKGEN_GFG                   0x2A0 // Start of GFG0
#define STM_FLEX_CLKGEN_GFG_SIZE              0x28  // Bytes between GFG config blocks

#define STM_FLEX_CLKGEN_EXT_CONFIG_0          0x3E0
#define STM_FLEX_CLKGEN_EXT_CONFIG_1          0x3E4
#define STM_FLEX_CLKGEN_EXT_CONFIG_2          0x3E8
#define STM_FLEX_CLKGEN_EXT_CONFIG_3          0x3EC
#define STM_FLEX_CLKGEN_EXT_CONFIG_4          0x3F0
#define STM_FLEX_CLKGEN_EXT_CONFIG_5          0x3F4
#define STM_FLEX_CLKGEN_EXT_CONFIG_6          0x3F8
#define STM_FLEX_CLKGEN_EXT_CONFIG_7          0x3FC


#define STM_FLEX_CLKGEN_CROSSBAR_SEL_MASK     0x3F    // Six bits, 0-63
#define STM_FLEX_CLKGEN_CROSSBAR_ENABLE       (1L<<6)
#define STM_FLEX_CLKGEN_CROSSBAR_STATUS_MASK  (1L<<7) // Read only change status bit

#define STM_FLEX_CLKGEN_PREDIV_DIVISOR_MASK   0x3FF   // Divide 1-1024 (value+1)

#define STM_FLEX_CLKGEN_FINALDIV_DIVISOR_MASK 0x3F    // Divide 1-64 (value+1)
#define STM_FLEX_CLKGEN_FINALDIV_OUTPUT_EN    (1L<<6)
#define STM_FLEX_CLKGEN_FINALDIV_SEMI_SYNC_EN (1L<<7)

/******************************************************************************
 * GFG FS660 Mapping
 */
#define STM_FCG_FS660_RESETS      0x0
#define STM_FCG_FS660_NDIV        0x4
#define STM_FCG_FS660_PRG_ENABLES 0xC
#define STM_FCG_FS660_CH0_CFG     0x14
#define STM_FCG_FS660_CH1_CFG     0x18
#define STM_FCG_FS660_CH2_CFG     0x1C
#define STM_FCG_FS660_CH3_CFG     0x20

/*
 * FS660_RESETS register Resets/Standby etc
 */
#define STM_FCG_FS660_NRST0       (1L<<0)
#define STM_FCG_FS660_NRST1       (1L<<1)
#define STM_FCG_FS660_NRST2       (1L<<2)
#define STM_FCG_FS660_NRST3       (1L<<3)

#define STM_FCG_FS660_NSB0        (1L<<8)
#define STM_FCG_FS660_NSB1        (1L<<9)
#define STM_FCG_FS660_NSB2        (1L<<10)
#define STM_FCG_FS660_NSB3        (1L<<11)

#define STM_FCG_FS660_NPDPLL      (1L<<12)
#define STM_FCG_FS660_INPUTREQSEL (1L<<16) /* 0 - 24-30MHz 1 - 48-60MHz */

#define STM_FCG_FS660_PLL_LOCKED  (1L<<24) /* Read only status */

/*
 * FS660_NDIV PLL NDIV value
 */
#define STM_FCG_FS660_NDIV_SHIFT  (16)
#define STM_FCG_FS660_NDIV_MASK   (0x7 << STM_FCG_FS660_NDIV_SHIFT)

/*
 * FS660_PRG_ENABLES channel MD/PE programming enables
 */
#define STM_FCG_FS660_EN_PROG0    (1L<<0)
#define STM_FCG_FS660_EN_PROG1    (1L<<1)
#define STM_FCG_FS660_EN_PROG2    (1L<<2)
#define STM_FCG_FS660_EN_PROG3    (1L<<3)

/*
 * FS660_CHX channel programming
 */
#define STM_FCG_FS660_PE_SHIFT    (0)
#define STM_FCG_FS660_PE_MASK     (0x7FFF)
#define STM_FCG_FS660_MD_SHIFT    (15)
#define STM_FCG_FS660_MD_MASK     (0x1F)
#define STM_FCG_FS660_SDIV_SHIFT  (20)
#define STM_FCG_FS660_SDIV_MASK   (0xF)

#define STM_FCG_FS660_NSDIV       (1L<<24)

/*****************************************************************************/
#endif // _STM_FLEX_CLK_GEN_REG_H
