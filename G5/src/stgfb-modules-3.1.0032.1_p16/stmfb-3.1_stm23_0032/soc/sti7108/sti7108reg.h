/***********************************************************************
 *
 * File: soc/sti7108/sti7108reg.h
 * Copyright (c) 2009 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef _STI7108REG_H
#define _STI7108REG_H

/* STi7108 Base addresses ----------------------------------------------------*/
#define STi7108_REGISTER_BASE          0xFD000000

/* STi7108 Base address size -------------------------------------------------*/
#define STi7108_REG_ADDR_SIZE          0x01710000

#define STi7108_TVOUT_FDMA_BASE        0x00000000
#  define STi7108_HDMI_BASE              (STi7108_TVOUT_FDMA_BASE + 0x0000)
#  define STi7108_HDMI_SPDIF_BASE        (STi7108_TVOUT_FDMA_BASE + 0x0c00)
#  define STi7108_HDMI_PCMPLAYER_BASE    (STi7108_TVOUT_FDMA_BASE + 0x0d00)
#  define STi7108_AUX_TVOUT_BASE         (STi7108_TVOUT_FDMA_BASE + 0x0e00) /* Yes, this really is in the HDMI register space on 7108 */

#define STi7108_TVOUT_CPU_BASE         0x00538000
#  define STi7108_DENC_BASE              (STi7108_TVOUT_CPU_BASE + 0x0000)
#  define STi7108_VTG2_BASE              (STi7108_TVOUT_CPU_BASE + 0x0200)
#  define STi7108_VTG1_BASE              (STi7108_TVOUT_CPU_BASE + 0x0300)
#  define STi7108_MAIN_TVOUT_BASE        (STi7108_TVOUT_CPU_BASE + 0x0400)
#  define STi7108_FLEXDVO_BASE           (STi7108_TVOUT_CPU_BASE + 0x0600)
#  define STi7108_HD_FORMATTER_BASE      (STi7108_TVOUT_CPU_BASE + 0x0800)
#    define STi7108_HD_FORMATTER_DIG1      (STi7108_HD_FORMATTER_BASE + 0x0100)
#    define STi7108_HD_FORMATTER_DIG2      (STi7108_HD_FORMATTER_BASE + 0x0180)
#    define STi7108_HD_FORMATTER_AWGANC    (STi7108_HD_FORMATTER_BASE + 0x0200)
#    define STi7108_HD_FORMATTER_AWG       (STi7108_HD_FORMATTER_BASE + 0x0300)
#  define STi7108_CEC_BASE               (STi7108_TVOUT_CPU_BASE + 0x0c00)
#define STi7108_HD_FORMATTER_AWG_SIZE  64


#define STi7108_CLKGEN_BASE            0x00546000 // clockgen B
#define STi7108_SYSCFG_BANK0_BASE      0x00E30000
#define STi7108_SYSCFG_BANK3_BASE      0x00500000
#define STi7108_SYSCFG_BANK4_BASE      0x01700000

#define STi7108_BLITTER_BASE           (0x00ABE000 + 0x0a00) // base + static regs offset

#define STi7108_COMPOSITOR_BASE        0x00545000
#define STi7108_HD_BASE                0x00100000 // HQVDP
#define STi7108_SD_BASE                0x00544000 // VDP


/* The compositor (COMP) offset addresses ------------------------------*/
#define STi7108_CUR_OFFSET     0x000
#define STi7108_GDP1_OFFSET    0x100
#define STi7108_GDP2_OFFSET    0x200
#define STi7108_GDP3_OFFSET    0x300
#define STi7108_GDP4_OFFSET    0x400
#define STi7108_VID1_OFFSET    0x700
#define STi7108_VID2_OFFSET    0x800
#define STi7108_MIXER1_OFFSET  0xC00
#define STi7108_MIXER2_OFFSET  0xD00

/* STi7108 Display Clockgen B registers --------------------------------------*/
#define CKGB_LCK       0x0010 /* Write lock register */

#define CKGB_FS0_CFG   0x0014 /* HD Clock FSynth */
#define CKGB_FS0_MD1   0x0018
#define CKGB_FS0_PE1   0x001C
#define CKGB_FS0_EN1   0x0020
#define CKGB_FS0_SDIV1 0x0024

#define CKGB_FS0_CLCKOUT_CTRL 0x0058

#define CKGB_FS1_CFG   0x005C /* SD Clock FSynth */
#define CKGB_FS1_MD1   0x0060
#define CKGB_FS1_PE1   0x0064
#define CKGB_FS1_EN1   0x0068
#define CKGB_FS1_SDIV1 0x006C

#define CKGB_FS1_CLCKOUT_CTRL 0x00A0

#define CKGB_DISPLAY_CFG      0x00A4
#define CKGB_CLK_SRC          0x00A8
#define CKGB_DIV1024          0x00AC
#define CKGB_ENABLE           0x00B0
#define CKGB_CLK_OUT_SEL      0x00B4
#define CKGB_CLK_REF_SEL      0x00B8

#define CKGB_LCK_UNLOCK      0xC0DE
#define CKGB_LCK_LOCK        0xC1A0

#define CKGB_FSCFG_NDIV      (1L<<0)
#define CKGB_FSCFG_REF_MASK  (3<<1)
#  define CKGB_FSCFG_GOODREF   (0L<<1)
#  define CKGB_FSCFG_VBADREF   (1L<<1)
#  define CKGB_FSCFG_BADREF    (2L<<1)
#  define CKGB_FSCFG_VGOODREF  (3L<<1)
#define CKGB_FSCFG_NOT_RESET (1L<<3)
#define CKGB_FSCFG_POWERUP   (1L<<4)

#define CKGB_CFG_DIV2               (0)
#define CKGB_CFG_DIV4               (1)
#define CKGB_CFG_DIV8               (2)
#define CKGB_CFG_BYPASS             (3)
#define CKGB_CFG_MASK               (3)

#define CKGB_CFG_PIX_SD_FS0(x)      ((x)<<10)
#define CKGB_CFG_PIX_SD_FS1(x)      ((x)<<12)


#define CKGB_SRC_PIXSD_FS1_NOT_FS0  (1L<<1)

#define CKGB_DIV1024_PIX_HD         (1L<<3)
#define CKGB_DIV1024_PIX_SD         (1L<<7)

#define CKGB_EN_PIX_HD              (1L<<3)
#define CKGB_EN_PIX_SD              (1L<<9)

#define CKGB_OUT_SEL_PIX_HD         (3L)
#define CKGB_OUT_SEL_PIX_SD         (8L)

#define CKGB_REF_SEL_SYS_CLKIN      (0)
#define CKGB_REF_SEL_SYS_ALTCLKIN   (1L)
#define CKGB_REF_SEL_MASK           (3L)

/* STi7108 Sysconfig bank0 reg12, Reset controller for HDMI PHY               */
#define SYS_CFG12_B0 0x28

#define SYS_CFG12_B0_HDMI_PHY_N_RST (1L<<23)

/* STi7108 Sysstatus bank3 reg5, HDMI rejection PLL lock ---------------------*/
#define SYS_STA5 0x14

#define SYS_STA5_HDMI_PLL_LOCK   (1L<<0)

/* STi7108 Sysconfig bank3 reg0, TVOut clocks   ------------- ----------------*/
#define SYS_CFG0 0x18

#define SYS_CFG0_CLK_STOP_SHIFT(channel)  (channel)
#define SYS_CFG0_CLK_SEL_SHIFT(channel) ((channel) + 12)

#define SYS_CFG0_CLK_STOP(channel)     (1L << SYS_CFG0_CLK_STOP_SHIFT(channel))
#define SYS_CFG0_CLK_SEL_SD(channel) (1L << SYS_CFG0_CLK_SEL_SHIFT(channel))

/* STi7108 Sysconfig bank3 reg1, TVOut clocks   ------------- ----------------*/
#define SYS_CFG1 0x1C

#define SYS_CFG1_CLK_DIV_BYPASS 0
#define SYS_CFG1_CLK_DIV_2      0x1
#define SYS_CFG1_CLK_DIV_4      0x2
#define SYS_CFG1_CLK_DIV_8      0x3
#define SYS_CFG1_CLK_DIV_MASK   0x3

#define SYS_CFG1_CLK_DIV_SHIFT(channel) ((channel)*2)

/* STi7108 Sysconfig bank3 reg2, Video2 PIP mode -----------------------------*/
#define SYS_CFG2 0x20

#define SYS_CFG2_PIP_MODE (1L<<0)

/* STi7108 Sysconfig bank3 reg3, Analogue DAC --------------------------------*/
#define SYS_CFG3 0x24

#define SYS_CFG3_DAC_SD_HZW_DISABLE  (1L<<0)
#define SYS_CFG3_DAC_SD_HZV_DISABLE  (1L<<1)
#define SYS_CFG3_DAC_SD_HZU_DISABLE  (1L<<2)
#define SYS_CFG3_DAC_HD_HZW_DISABLE  (1L<<5)
#define SYS_CFG3_DAC_HD_HZV_DISABLE  (1L<<6)
#define SYS_CFG3_DAC_HD_HZU_DISABLE  (1L<<7)

/* STi7108 Sysconfig bank3 reg4, HDMI PLL ------------------------------------*/
#define SYS_CFG4 0x28

#define SYS_CFG4_PLL_S_HDMI_EN           (1L<<0)
#define SYS_CFG4_PLL_S_HDMI_PDIV_SHIFT   (24)
#define SYS_CFG4_PLL_S_HDMI_PDIV_MASK    (7L<<SYS_CFG4_PLL_S_HDMI_PDIV_SHIFT)
#define SYS_CFG4_PLL_S_HDMI_NDIV_SHIFT   (16)
#define SYS_CFG4_PLL_S_HDMI_NDIV_MASK    (0xFF<<SYS_CFG4_PLL_S_HDMI_NDIV_SHIFT)
#define SYS_CFG4_PLL_S_HDMI_MDIV_SHIFT   (8)
#define SYS_CFG4_PLL_S_HDMI_MDIV_MASK    (0xFF<<SYS_CFG4_PLL_S_HDMI_MDIV_SHIFT)

/* STi7108 Sysconfig bank4 reg0, PIO15 HDMI HPD      -------------------------*/
#define SYS_CFG0_B4 0x0

#define SYS_CFG0_B4_PIO15_1_ALT1 (1L<<4)

/* STi7108 Sysconfig bank4 reg24, Audio FSynth Config-------------------------*/
#define SYS_CFG24_B4 0x60

#define SYS_CFG24_B4_N_RST         (1L<<0)
#define SYS_CFG24_B4_FS0_SEL       (1L<<2)
#define SYS_CFG24_B4_FS1_SEL       (1L<<3)
#define SYS_CFG24_B4_FS2_SEL       (1L<<4)
#define SYS_CFG24_B4_FS3_SEL       (1L<<5)
#define SYS_CFG24_B4_FS0_EN        (1L<<6)
#define SYS_CFG24_B4_FS1_EN        (1L<<7)
#define SYS_CFG24_B4_FS2_EN        (1L<<8)
#define SYS_CFG24_B4_FS3_EN        (1L<<9)
#define SYS_CFG24_B4_FS0_NSB       (1L<<10)
#define SYS_CFG24_B4_FS1_NSB       (1L<<11)
#define SYS_CFG24_B4_FS2_NSB       (1L<<12)
#define SYS_CFG24_B4_FS3_NSB       (1L<<13)
#define SYS_CFG24_B4_NPDA          (1L<<14)
#define SYS_CFG24_B4_AUD_NDIV      (1L<<15)
#define SYS_CFG24_B4_BW_SEL_SHIFT  (16)
#define SYS_CFG24_B4_BW_SEL_MASK   (0x3<<SYS_CFG24_B4_BW_SEL_SHIFT)
#define SYS_CFG24_B4_CLK_IN_SHIFT  (23)
#define SYS_CFG24_B4_CLK_IN_MASK   (0x3<<SYS_CFG24_B4_CLK_IN_SHIFT)

/* STi7108 sysconfig bank4, Audio FSynth registers ---------------------------*/
#define SYS_CFG_B4_FS0_MD   0x64
#define SYS_CFG_B4_FS0_PE   0x68
#define SYS_CFG_B4_FS0_SDIV 0x6C
#define SYS_CFG_B4_FS0_EN   0x70
#define SYS_CFG_B4_FS1_MD   0x74
#define SYS_CFG_B4_FS1_PE   0x78
#define SYS_CFG_B4_FS1_SDIV 0x7C
#define SYS_CFG_B4_FS1_EN   0x80
#define SYS_CFG_B4_FS2_MD   0x84
#define SYS_CFG_B4_FS2_PE   0x88
#define SYS_CFG_B4_FS2_SDIV 0x8C
#define SYS_CFG_B4_FS2_EN   0x90
#define SYS_CFG_B4_FS3_MD   0x94
#define SYS_CFG_B4_FS3_PE   0x98
#define SYS_CFG_B4_FS3_SDIV 0x9C
#define SYS_CFG_B4_FS3_EN   0xA0

#endif // _STI7108REG_H
