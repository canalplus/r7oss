/***********************************************************************
 *
 * File: soc/sti7111/sti7111reg.h
 * Copyright (c) 2008 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef _STI7111REG_H
#define _STI7111REG_H

/* STi7111 Base addresses ----------------------------------------------------*/
#define STi7111_REGISTER_BASE          0xFD000000

/* STi7111 Base address size -------------------------------------------------*/
#define STi7111_REG_ADDR_SIZE          0x01211000

#define STi7111_PIO4                   0x00024000

#define STi7111_TVOUT_FDMA_BASE        0x00104000
#  define STi7111_HDMI_BASE              (STi7111_TVOUT_FDMA_BASE + 0x0000)
#  define STi7111_HDMI_SPDIF_BASE        (STi7111_TVOUT_FDMA_BASE + 0x0c00)
#  define STi7111_HDMI_PCMPLAYER_BASE    (STi7111_TVOUT_FDMA_BASE + 0x0d00)
#  define STi7111_AUX_TVOUT_BASE         (STi7111_TVOUT_FDMA_BASE + 0x0e00) /* Yes, this really is in the HDMI register space on 7111 */

#define STi7111_TVOUT_CPU_BASE         0x01030000
#  define STi7111_DENC_BASE              (STi7111_TVOUT_CPU_BASE + 0x0000)
#  define STi7111_VTG2_BASE              (STi7111_TVOUT_CPU_BASE + 0x0200)
#  define STi7111_VTG1_BASE              (STi7111_TVOUT_CPU_BASE + 0x0300)
#  define STi7111_MAIN_TVOUT_BASE        (STi7111_TVOUT_CPU_BASE + 0x0400)
#  define STi7111_FLEXDVO_BASE           (STi7111_TVOUT_CPU_BASE + 0x0600)
#  define STi7111_HD_FORMATTER_BASE      (STi7111_TVOUT_CPU_BASE + 0x0800)
#    define STi7111_HD_FORMATTER_DIG1      (STi7111_HD_FORMATTER_BASE + 0x0100)
#    define STi7111_HD_FORMATTER_DIG2      (STi7111_HD_FORMATTER_BASE + 0x0180)
#    define STi7111_HD_FORMATTER_AWGANC    (STi7111_HD_FORMATTER_BASE + 0x0200)
#    define STi7111_HD_FORMATTER_AWG       (STi7111_HD_FORMATTER_BASE + 0x0300)
#  define STi7111_CEC_BASE               (STi7111_TVOUT_CPU_BASE + 0x0c00)
#define STi7111_HD_FORMATTER_AWG_SIZE  64


#define STi7111_AUDCFG_BASE            0x01210000
#define STi7111_CLKGEN_BASE            0x01000000 // clockgen B
#define STi7111_SYSCFG_BASE            0x01001000

#define STi7111_BLITTER_BASE           (0x0120b000 + 0x0a00) // base + static regs offset

#define STi7111_COMPOSITOR_BASE        0x0120A000
#define STi7111_HD_BASE                0x01002000
#define STi7111_SD_BASE                0x01003000


/* The compositor (COMP) offset addresses ------------------------------*/
#define STi7111_CUR_OFFSET     0x000
#define STi7111_GDP1_OFFSET    0x100
#define STi7111_GDP2_OFFSET    0x200
#define STi7111_GDP3_OFFSET    0x300
#define STi7111_ALP_OFFSET     0x600
#define STi7111_VID1_OFFSET    0x700
#define STi7111_VID2_OFFSET    0x800
#define STi7111_MIXER1_OFFSET  0xC00
#define STi7111_MIXER2_OFFSET  0xD00
#define STi7111_CAPTURE_OFFSET 0xE00

/* STi7111 Display Clockgen B registers --------------------------------------*/
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

#define CKGB_CFG_TMDS_HDMI(x)       ((x)<<0)
#define CKGB_CFG_DISP_HD(x)         ((x)<<4)
#define CKGB_CFG_656(x)             ((x)<<6)
#define CKGB_CFG_DISP_ID(x)         ((x)<<8)
#define CKGB_CFG_PIX_SD_FS0(x)      ((x)<<10)
#define CKGB_CFG_PIX_SD_FS1(x)      ((x)<<12)

#define CKGB_CFG_PIX_HD_FS1_N_FS0   (1L<<14)


#define CKGB_SRC_GDP3_FS1_NOT_FS0   (1L<<0)
#define CKGB_SRC_PIXSD_FS1_NOT_FS0  (1L<<1)
#define CKGB_SRC_DVP_FS1_NOT_FS0    (1L<<2)
#define CKGB_SRC_DVP_FSYNTH_NOT_EXT (1L<<3)

#define CKGB_DIV1024_TMDS           (1L<<1)
#define CKGB_DIV1024_PIX_HD         (1L<<3)
#define CKGB_DIV1024_DISP_HD        (1L<<4)
#define CKGB_DIV1024_CCIR656        (1L<<5)
#define CKGB_DIV1024_DISP_ID        (1L<<6)
#define CKGB_DIV1024_PIX_SD         (1L<<7)

#define CKGB_EN_PIX_HD              (1L<<3)
#define CKGB_EN_DISP_HD             (1L<<4)
#define CKGB_EN_TMDS_HDMI           (1L<<5)
#define CKGB_EN_656                 (1L<<6)
#define CKGB_EN_GDP3                (1L<<7)
#define CKGB_EN_DISP_ID             (1L<<8)
#define CKGB_EN_PIX_SD              (1L<<9)

#define CKGB_OUT_SEL_TMDS           (1L)
#define CKGB_OUT_SEL_PIX_HD         (3L)
#define CKGB_OUT_SEL_DISP_HD        (4L)
#define CKGB_OUT_SEL_GDP3           (6L)
#define CKGB_OUT_SEL_DISP_SD        (7L)
#define CKGB_OUT_SEL_PIX_SD         (8L)

#define CKGB_REF_SEL_SYSA_CLKIN      (0)
#define CKGB_REF_SEL_SYSB_CLKIN      (1L)
#define CKGB_REF_SEL_SYSA_ALTCLKIN   (2L)
#define CKGB_REF_SEL_MASK            (3L)

/*
 * Offsets into the audio hardware so we can determine the HDMI audio
 * setup without making a dependency between the display and audio drivers.
 */
#define AUD_FSYN_CFG   0x00
#define AUD_FSYN0      0x10
#define AUD_FSYN1      0x20
#define AUD_FSYN2      0x30
#define AUD_FSYN_MD    0x0
#define AUD_FSYN_PE    0x4
#define AUD_FSYN_SDIV  0x8
#define AUD_FSYN_EN    0xC

#define AUD_FSYN_CFG_RST     (1L<<0)
#define AUD_FSYN_CFG_FS0_SEL (1L<<2)
#define AUD_FSYN_CFG_FS1_SEL (1L<<3)
#define AUD_FSYN_CFG_FS2_SEL (1L<<4)
#define AUD_FSYN_CFG_FS0_EN  (1L<<6)
#define AUD_FSYN_CFG_FS1_EN  (1L<<7)
#define AUD_FSYN_CFG_FS2_EN  (1L<<8)
#define AUD_FSYN_CFG_FS0_NSB (1L<<10)
#define AUD_FSYN_CFG_FS1_NSB (1L<<11)
#define AUD_FSYN_CFG_FS2_NSB (1L<<12)
#define AUD_FSYN_CFG_NPDA    (1L<<14)
#define AUD_FSYN_CFG_SYSA_IN (0)
#define AUD_FSYN_CFG_SYSB_IN (1L<<23)
#define AUD_FSYN_CFG_IN_MASK (3L<<23)


/* STi7111 Sysstatus reg9, HDMI rejection PLL lock ---------------------------*/
#define SYS_STA9 0x2C
#define SYS_STA9_HDMI_PLL_LOCK   (1L<<0)

/* STi7111 Sysconfig reg1, HDMI Phy compensation -----------------------------*/
#define SYS_CFG1 0x104

/* STi7111 Sysconfig reg2, HDMI power off & PIO4/7 hotplug control -----------*/
#define SYS_CFG2 0x108

#define SYS_CFG2_HDMIPHY_USER_COMP_MASK    0xffff
#define SYS_CFG2_HDMIPHY_USER_COMP_EN      (1L<<16)
#define SYS_CFG2_HDMIPHY_BUFFER_SPEED_MASK (3L<<17)
#  define SYS_CFG2_HDMIPHY_BUFFER_SPEED_16   (0L<<17)
#  define SYS_CFG2_HDMIPHY_BUFFER_SPEED_8    (1L<<17)
#  define SYS_CFG2_HDMIPHY_BUFFER_SPEED_4    (3L<<17)
#define SYS_CFG2_HDMIPHY_PREEMP_ON         (1L<<19)
#define SYS_CFG2_HDMIPHY_PREEMP_STR_SHIFT  (20)
#define SYS_CFG2_HDMIPHY_PREEMP_STR_MASK   (3L<<SYS_CFG2_HDMIPHY_PREEMP_STR_SHIFT)
#define SYS_CFG2_HDMIPHY_PREEMP_WIDTH_MASK (7L<<22)
#define SYS_CFG2_HDMIPHY_PREEMP_WIDTH_EXT  (1L<<25)
#define SYS_CFG2_HDMI_POFF                 (1L<<26)
#define SYS_CFG2_HDMI_HOTPLUG_EN           (1L<<27)

/* STi7111 Sysconfig reg3, Analogue DAC & HDMI PLL control -------------------*/
#define SYS_CFG3 0x10C

#define SYS_CFG3_DAC_SD_HZW_DISABLE     (1L<<0)
#define SYS_CFG3_DAC_SD_HZV_DISABLE     (1L<<1)
#define SYS_CFG3_DAC_SD_HZU_DISABLE     (1L<<2)
#define SYS_CFG3_DAC_HD_HZW_DISABLE     (1L<<3)
#define SYS_CFG3_DAC_HD_HZV_DISABLE     (1L<<4)
#define SYS_CFG3_DAC_HD_HZU_DISABLE     (1L<<5)

#define SYS_CFG3_S_HDMI_RST_N            (1L<<11)
#define SYS_CFG3_PLL_S_HDMI_EN           (1L<<12)
#define SYS_CFG3_PLL_S_HDMI_PDIV_SHIFT   (13)
#define SYS_CFG3_PLL_S_HDMI_PDIV_MASK    (7L<<SYS_CFG3_PLL_S_HDMI_PDIV_SHIFT)
#define SYS_CFG3_PLL_S_HDMI_NDIV_SHIFT   (16)
#define SYS_CFG3_PLL_S_HDMI_NDIV_MASK    (0xFF<<SYS_CFG3_PLL_S_HDMI_NDIV_SHIFT)
#define SYS_CFG3_PLL_S_HDMI_MDIV_SHIFT   (24)
#define SYS_CFG3_PLL_S_HDMI_MDIV_MASK    (0xFF<<SYS_CFG3_PLL_S_HDMI_MDIV_SHIFT)

/* STi7111 Sysconfig reg6, Video2 PIP mode -----------------------------------*/
#define SYS_CFG6 0x118

#define SYS_CFG6_VIDEO2_MAIN_N_AUX (1L<<0)

#endif // _STI7111REG_H
