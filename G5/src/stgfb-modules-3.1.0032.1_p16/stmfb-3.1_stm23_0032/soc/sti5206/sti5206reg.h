/***********************************************************************
 *
 * File: soc/sti5206/sti5206reg.h
 * Copyright (c) 2008 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef _STI5206REG_H
#define _STI5206REG_H

/* STi5206 Base addresses ----------------------------------------------------*/
#define STi5206_REGISTER_BASE          0xFD000000

/* STi5206 Base address size -------------------------------------------------*/
#define STi5206_REG_ADDR_SIZE          0x01211000

#define STi5206_TVOUT_FDMA_BASE        0x00104000
#  define STi5206_HDMI_BASE              (STi5206_TVOUT_FDMA_BASE + 0x0000)
#  define STi5206_HDMI_SPDIF_BASE        (STi5206_TVOUT_FDMA_BASE + 0x0c00)
#  define STi5206_HDMI_PCMPLAYER_BASE    (STi5206_TVOUT_FDMA_BASE + 0x0d00)
#  define STi5206_AUX_TVOUT_BASE         (STi5206_TVOUT_FDMA_BASE + 0x0e00) /* Yes, this really is in the HDMI register space on 5206 */

#define STi5206_TVOUT_CPU_BASE         0x01030000
#  define STi5206_DENC_BASE              (STi5206_TVOUT_CPU_BASE + 0x0000)
#  define STi5206_VTG2_BASE              (STi5206_TVOUT_CPU_BASE + 0x0200)
#  define STi5206_VTG1_BASE              (STi5206_TVOUT_CPU_BASE + 0x0300)
#  define STi5206_MAIN_TVOUT_BASE        (STi5206_TVOUT_CPU_BASE + 0x0400)
#  define STi5206_FLEXDVO_BASE           (STi5206_TVOUT_CPU_BASE + 0x0600)
#  define STi5206_HD_FORMATTER_BASE      (STi5206_TVOUT_CPU_BASE + 0x0800)
#    define STi5206_HD_FORMATTER_DIG1      (STi5206_HD_FORMATTER_BASE + 0x0100)
#    define STi5206_HD_FORMATTER_DIG2      (STi5206_HD_FORMATTER_BASE + 0x0180)
#    define STi5206_HD_FORMATTER_AWGANC    (STi5206_HD_FORMATTER_BASE + 0x0200)
#    define STi5206_HD_FORMATTER_AWG       (STi5206_HD_FORMATTER_BASE + 0x0300)
#  define STi5206_CEC_BASE               (STi5206_TVOUT_CPU_BASE + 0x0c00)
#define STi5206_HD_FORMATTER_AWG_SIZE  64


#define STi5206_AUDCFG_BASE            0x01210000
#define STi5206_CLKGEN_BASE            0x01000000 // clockgen B
#define STi5206_SYSCFG_BASE            0x01001000

#define STi5206_BLITTER_BASE           (0x0120b000 + 0x0a00) // base + static regs offset

#define STi5206_COMPOSITOR_BASE        0x0120A000
#define STi5206_HD_BASE                0x01002000
#define STi5206_SD_BASE                0x01003000


/* The compositor (COMP) offset addresses ------------------------------*/
#define STi5206_GDP1_OFFSET    0x100
#define STi5206_GDP2_OFFSET    0x200
#define STi5206_GDP3_OFFSET    0x300
#define STi5206_VID1_OFFSET    0x700
#define STi5206_VID2_OFFSET    0x800
#define STi5206_MIXER1_OFFSET  0xC00
#define STi5206_MIXER2_OFFSET  0xD00

/* STi5206 Display Clockgen B registers --------------------------------------*/
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

#define CKGB_REF_SEL_SYSA_CLKIN      (0)
#define CKGB_REF_SEL_SYSB_CLKIN      (1L)
#define CKGB_REF_SEL_SYSA_ALTCLKIN   (2L)
#define CKGB_REF_SEL_SYSA_MASK       (3L)


/* STi5206 Sysconfig reg3, Analogue DAC & HDMI PLL control -------------------*/
#define SYS_CFG3 0x10C

#define SYS_CFG3_DAC_SD_HZW_DISABLE     (1L<<0)
#define SYS_CFG3_DAC_SD_HZV_DISABLE     (1L<<1)
#define SYS_CFG3_DAC_SD_HZU_DISABLE     (1L<<2)
#define SYS_CFG3_DAC_HD_HZW_DISABLE     (1L<<3)
#define SYS_CFG3_DAC_HD_HZV_DISABLE     (1L<<4)
#define SYS_CFG3_DAC_HD_HZU_DISABLE     (1L<<5)

/* STi5206 Sysconfig reg6, Video2 PIP mode -----------------------------------*/
#define SYS_CFG6 0x118

#define SYS_CFG6_VIDEO2_MAIN_N_AUX (1L<<0)

/* STi5206 Sysconfig reg10, DVO pad enable -----------------------------------*/
#define SYS_CFG10 0x128

#define SYS_CFG10_DVO_ENABLE (1L<<9)

/* STi5206 Sysconfig reg15, TVOut clocks   -----------------------------------*/
#define SYS_CFG15 0x13C

#define CLK_PIX_ED_CHANNEL   0
#define CLK_DISP_ED_CHANNEL  1
#define CLK_PIX_SD_CHANNEL   2
#define CLK_DISP_SD_CHANNEL  3
#define CLK_656_CHANNEL      4
#define CLK_GDP1_CHANNEL     5
#define CLK_PIP_CHANNEL      6

#define SYS_CFG15_CLK_DIV_BYPASS 0
#define SYS_CFG15_CLK_DIV_2      0x1
#define SYS_CFG15_CLK_DIV_4      0x2
#define SYS_CFG15_CLK_DIV_8      0x3
#define SYS_CFG15_CLK_DIV_MASK   0x3

#define SYS_CFG15_CLK_DIV_SHIFT(channel) ((channel)*2)
#define SYS_CFG15_CLK_EN_SHIFT(channel)  ((channel) + 16)
#define SYS_CFG15_CLK_SEL_SHIFT(channel) ((channel) + 24)

#define SYS_CFG15_CLK_EN(channel)     (1L << SYS_CFG15_CLK_EN_SHIFT(channel))
#define SYS_CFG15_CLK_SEL_SD(channel) (1L << SYS_CFG15_CLK_SEL_SHIFT(channel))

#endif // _STI5206REG_H
