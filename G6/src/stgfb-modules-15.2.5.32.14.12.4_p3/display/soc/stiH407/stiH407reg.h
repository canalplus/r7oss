/***********************************************************************
 *
 * File: display/soc/stiH407/stiH407reg.h
 * Copyright (c) 2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef _STIH407REG_H
#define _STIH407REG_H

/* STiH407 Base addresses for display IP ------------------------------------ */
#define STiH407_REGISTER_BASE       0x08D00000

/* STiH407 Base address size -------------------------------------------------*/
#define STiH407_REG_ADDR_SIZE       0x01012000 // To end of compo block

/*
 * The two TVOut register blocks:
 * TVOUT0 is TVOUT base address, TVOUT1 is TVOUT glue base address
 */
#define STiH407_TVOUT0_BASE         0x00000000
#  define STiH407_DENC_BASE              (STiH407_TVOUT0_BASE + 0x0000)
#  define STiH407_VTG_AUX_BASE           (STiH407_TVOUT0_BASE + 0x0200)
#  define STiH407_FLEXDVO_BASE           (STiH407_TVOUT0_BASE + 0x0400)

#  define STiH407_HD_FORMATTER_BASE      (STiH407_TVOUT0_BASE + 0x2000)
#    define STiH407_HD_FORMATTER_AWGANC    (STiH407_HD_FORMATTER_BASE + 0x0200)
#    define STiH407_HD_FORMATTER_AWG       (STiH407_HD_FORMATTER_BASE + 0x0300)

#  define STiH407_VTG_MAIN_BASE          (STiH407_TVOUT0_BASE + 0x2800)
#  define STiH407_HDMI_BASE              (STiH407_TVOUT0_BASE + 0x4000)


#define STiH407_TVOUT1_BASE         0x00008000
#  define STiH407_TVO_MAIN_PF_BASE       (STiH407_TVOUT1_BASE + 0x000)
#  define STiH407_TVO_AUX_PF_BASE        (STiH407_TVOUT1_BASE + 0x100)
#  define STiH407_TVO_VIP_DENC_BASE      (STiH407_TVOUT1_BASE + 0x200)
#  define STiH407_TVO_TTXT_BASE          (STiH407_TVOUT1_BASE + 0x300)
#  define STiH407_TVO_VIP_HDF_BASE       (STiH407_TVOUT1_BASE + 0x400)
#  define STiH407_TVO_VIP_HDMI_BASE      (STiH407_TVOUT1_BASE + 0x500)
#  define STiH407_TVO_VIP_DVO_BASE       (STiH407_TVOUT1_BASE + 0x600)

#define STiH407_HD_FORMATTER_AWG_SIZE  64


#define STiH407_UNIPLAYER_HDMI_BASE 0x00080000

#define STiH407_CLKGEN_A_12         0x00401000 // HDMI Rejection
#define STiH407_CLKGEN_D_10         0x00404000 // HDMI Audio Clock
#define STiH407_CLKGEN_D_12         0x00406000 // Video clocks/VCC

/* SYSCFG --------------------------------------------------------------------*/
#define STiH407_SYSCFG_CORE         0x005B0000 // Also known as SYSCFG_GP_10

/* Video planes --------------------------------------------------------------*/
#define STiH407_HQVDP_BASE          0x00F00000
#define STiH407_VDP_BASE            0x01010000

/* The compositor (COMP) offset addresses ------------------------------------*/
#define STiH407_COMPOSITOR_BASE     0x01011000
#define STiH407_CUR_OFFSET     0x000
#define STiH407_GDP1_OFFSET    0x100
#define STiH407_GDP2_OFFSET    0x200
#define STiH407_GDP3_OFFSET    0x300
#define STiH407_GDP4_OFFSET    0x400
#define STiH407_VID1_OFFSET    0x700
#define STiH407_VID2_OFFSET    0x800
#define STiH407_MIXER1_OFFSET  0xC00
#define STiH407_MIXER2_OFFSET  0xD00


/* STiH407 TVO regs, Analogue DAC --------------------------------------------*/
#define TVO_SD_DAC_CONFIG               0x220
#define TVO_HD_DAC_CONFIG               0x420
#define TVO_DAC_DEL_SHIFT               8
#define TVO_DAC_DEL_WIDTH               3
#define TVO_DAC_DENC_OUT_SD_SHIFT       4
#define TVO_DAC_DENC_OUT_SD_WIDTH       1
#define TVO_DAC_DENC_OUT_SD_DAC123      0L
#define TVO_DAC_DENC_OUT_SD_DAC456      1L
#define TVO_DAC_POFF_DACS_SHIFT         0
#define TVO_DAC_POFF_DACS_WIDTH         1
#define TVO_DAC_POFF_DACS               1L

#define TVO_ADAPTIVE_DCMTN_FLTR_CFG       0x20
#define TVO_DCMTN_FLTR_CFG_23TAP_ADAPTIVE 0x0
#define TVO_DCMTN_FLTR_CFG_23TAP_LINEAR   0x1
#define TVO_DCMTN_FLTR_CFG_SAMPLE_DROP    0x3

/* STiH407 TVO MISR ---------------------------------------------------*/
#define STiH407_TVO_MAIN_IN_VID_FORMAT  0x0030
#define STiH407_TVO_AUX_IN_VID_FORMAT   0x0130
#define STiH407_TVO_TVO_IN_FMT_SIGNED   (0x1<<0)
#define STiH407_TVO_SYNC_EXT            (0x0<<4) /* stiH407 not dual die solution, only internal VTG available*/

#define STiH407_TVO_HD_OUT_CTRL             (STiH407_TVOUT1_BASE + 0x04A0)
#define STiH407_TVO_MAIN_PF_CTRL            (STiH407_TVOUT1_BASE + 0x0480)
#define STiH407_TVO_SD_OUT_CTRL             (STiH407_TVOUT1_BASE + 0x02C0)
#define STiH407_TVO_AUX_PF_CTRL             (STiH407_TVOUT1_BASE + 0x0280)


/* STiH407 Syscfg ------------------------------------------------------------*/

/* Sysconfig Core 5072, Video DACs control -----------------------------------*/
#define SYS_CFG5072 0x120

#define SYS_CFG5072_DAC_HZX   (1L<<0) /* CVBS SD DAC, note NO S-VIDEO on this design */
#define SYS_CFG5072_DAC_HZUVW (1L<<1) /* YUV/RGB HD DACs all together */

/* Sysconfig Core 5073, Video input and Dual SCART control -------------------*/
#define SYS_CFG5073                 0x124
#define SYS_CFG5073_DUAL_SCART_EN   (1L<<1) /* Switches Aux VDP to use main VTG */

/* Sysconfig Core 5131, Reset Generator control 0 ----------------------------*/
#define SYS_CFG5131 0x20c

/*
 * TODO: Add display related reset bits when information available from design
 */

/* Sysconfig Core 5132, Reset Generator control 1 ----------------------------*/
#define SYS_CFG5132 0x210

#define SYS_CFG5131_RST_N_HDTVOUT     (1L<<11)

#endif // _STIH407REG_H
