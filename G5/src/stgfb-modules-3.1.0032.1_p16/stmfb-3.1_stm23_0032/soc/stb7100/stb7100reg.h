/***********************************************************************
 *
 * File: soc/stb7100/stb7100reg.h
 * Copyright (c) 2005 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef STb7100REG_H
#define STb7100REG_H

/* STb7100 Base addresses ----------------------------------------------------*/
#define STb7100_REGISTER_BASE       0x18000000

/* STb7100 Base address size -------------------------------------------------*/
#define STb7100_REG_ADDR_SIZE       0x02000000

#define STb7100_PIO2_BASE           0x00022000
#define STb7100_PCMP0_BASE          0x00101000
#define STb7100_SPDIF_BASE          0x00103000
#define STb7100_PRCONV_BASE         0x00103800

#define STb7100_CLKGEN_BASE         0x01000000
#define STb7100_SYSCFG_BASE         0x01001000
#define STb7100_HD_DISPLAY_BASE     0x01002000 
#define STb7100_SD_DISPLAY_BASE     0x01003000 

#define STb7100_AWG_BASE            0x01004800
#define STb7100_VOS_BASE            0x01005000 
#define STb7100_HDMI_BASE           0x01005800

#define STb7100_COMPOSITOR_BASE     0x0120A000 
#define STb7100_BLITTER_BASE        0x0120B000
#define STb7109_BDISP_BASE          0x0120BA00
#define STb7100_DENC_BASE           0x0120C000

#define STb7100_AUDIO_BASE          0x01210000

/* The compositor (COMP) offset addresses ------------------------------------*/ 
#define STb7100_CUR_OFFSET     0x000
#define STb7100_GDP1_OFFSET    0x100
#define STb7100_GDP2_OFFSET    0x200
#define STb7109_GDP3_OFFSET    0x300 /* cut 3 */
#define STb7100_ALP_OFFSET     0x600
#define STb7100_VID1_OFFSET    0x700
#define STb7100_VID2_OFFSET    0x800
#define STb7100_MIXER1_OFFSET  0xC00
#define STb7100_MIXER2_OFFSET  0xD00

/* STb7100 VOS Display Configuration Registers -------------------------------*/
#define DSP_CFG_CLK    0x0070
#define DSP_CFG_DIG    0x0074
#define DSP_CFG_ANA    0x0078
#define DSP_CFG_DAC    0x00DC

#define DSP_CLK_DENC_MAIN_NOT_AUX          (1L<<0)
#define DSP_CLK_HDMI_YCBCR_NOT_RGB_IN      (1L<<1)
#define DSP_CLK_UPSAMPLER_ENABLE           (1L<<2)
#define DSP_CLK_UPSAMPLE_4X_NOT_2X         (1L<<3)
#define DSP_CLK_UPSAMPLE_COEFF_512         (0L<<4)
#define DSP_CLK_UPSAMPLE_COEFF_256         (1L<<4)
#define DSP_CLK_UPSAMPLE_COEFF_1024        (2L<<4)
#define DSP_CLK_UPSAMPLE_COEFF_MASK        (3L<<4)
#define DSP_CLK_DVP_REF_AUX_NOT_MAIN       (1L<<6)
#define DSP_CLK_HDMI_OFF_NOT_ON            (1L<<7)
#define DSP_CLK_UPSAMPLE_4X_SD_PROGRESSIVE (1L<<10)
#define DSP_CLK_7109C3_VID2_HD_CLK_SEL     (1L<<11)

#define DSP_DIG_AUX_NOT_MAIN          (1L<<0)
#define DSP_DIG_OUTPUT_ENABLE         (1L<<1)

#define DSP_ANA_RGB_NOT_YCBCR_OUT     (1L<<0)
#define DSP_ANA_HD_SYNC_ENABLE        (1L<<1)
#define DSP_ANA_RESCALE_FOR_SYNC      (1L<<2)
#define DSP_ANA_USE_AWG_NOT_770x_SYNC (1L<<3)

#define DSP_DAC_SD_POWER_ON           (1L<<7)
#define DSP_DAC_HD_POWER_ON           (1L<<8)


/* STb7100 VOS Main video output registers -----------------------------------*/
#define DHDO_ACFG     0x007C
#define DHDO_ABROAD   0x0080
#define DHDO_BBROAD   0x0084
#define DHDO_CBROAD   0x0088
#define DHDO_BACTIVE  0x008C
#define DHDO_CACTIVE  0x0090
#define DHDO_BROADL   0x0094
#define DHDO_BLANKL   0x0098
#define DHDO_ACTIVEL  0x009C
#define DHDO_ZEROL    0x00A0
#define DHDO_MIDL     0x00A4
#define DHDO_HI       0x00A8
#define DHDO_MIDLO    0x00AC
#define DHDO_LO       0x00B0
#define DHDO_COLOR    0x00B4
#define DHDO_YMLT     0x00B8
#define DHDO_CMLT     0x00BC
#define DHDO_COFF     0x00C0
#define DHDO_YOFF     0x00C4

#define DHDO_ACFG_SYNC_CHROMA_ENABLE     (1L<<0)
#define DHDO_ACFG_PROGRESSIVE_SMPTE_295M (1L<<1)
#define DHDO_ACFG_PROGRESSIVE_SMPTE_293M (1L<<2)

#define DHDO_COLOR_ITUR_601_NOT_709   (1L<<0)
#define DHDO_COLOR_SMPTE240M_NOT_ITUR (1L<<1)

/* STb7100 VOS SD DVO/C656 registers -----------------------------------------*/
#define C656_ACTL   0x00C8
#define C656_BACT   0x00CC
#define C656_BLANKL 0x00D0
#define C656_EACT   0x00D4
#define C656_PAL    0x00D8

#define C656_PAL_NOT_NTSC (1L<<0)

/* STb7100 VOS Upsampler coefficient registers -----------------------------------*/
#define COEFF_SET1_1 0x00E4
#define COEFF_SET1_2 0x00E8
#define COEFF_SET1_3 0x00EC
#define COEFF_SET1_4 0x00F0
#define COEFF_SET2_1 0x00F4
#define COEFF_SET2_2 0x00F8
#define COEFF_SET2_3 0x00FC
#define COEFF_SET2_4 0x0100
#define COEFF_SET3_1 0x0104
#define COEFF_SET3_2 0x0108
#define COEFF_SET3_3 0x010C
#define COEFF_SET3_4 0x0110
#define COEFF_SET4_1 0x0114
#define COEFF_SET4_2 0x0118
#define COEFF_SET4_3 0x011C
#define COEFF_SET4_4 0x0120

/* STb7100 VOS HDMI Status ---------------------------------------------------*/
#define HDMI_PHY_LCK_STA 0x0124

#define HDMI_PHY_DLL_LCK  (1L<<0)
#define HDMI_PHY_RPLL_LCK (1L<<1)

/* STb7100 AWG Waveform Generator --------------------------------------------*/
#define AWG_CTRL_0  0x0000
#define AWG_RAM_REG 0x0100

#define AWG_CTRL_ENABLE                      (1L<<0)
#define AWG_CTRL_HSYNC_NO_FORCE_IP_INCREMENT (1L<<1)
#define AWG_CTRL_VSYNC_FIELD_NOT_FRAME       (1L<<2)

/* STb7100 Display Clockgen B registers --------------------------------------*/
#define CKGB_LCK       0x0010 /* Write lock register */

#define CKGB_FS0_CFG   0x0014 /* HD Clock FSynth */
#define CKGB_FS0_MD1   0x0018
#define CKGB_FS0_PE1   0x001C
#define CKGB_FS0_EN1   0x0020
#define CKGB_FS0_SDIV1 0x0024

#define CKGB_FS1_CFG   0x005C /* SD Clock FSynth */
#define CKGB_FS1_MD1   0x0060
#define CKGB_FS1_PE1   0x0064
#define CKGB_FS1_EN1   0x0068
#define CKGB_FS1_SDIV1 0x006C

#define CKGB_DISP_CFG      0x00A4
#define CKGB_CLK_SRC       0x00A8
#define CKGB_CLK_DIV       0x00AC
#define CKGB_CLK_EN        0x00B0
#define CKGB_CLK_OUT_SEL   0x00B4
#define CKGB_CLK_REF_SEL   0x00B8

#define CKGB_LCK_UNLOCK      0xC0DE
#define CKGB_LCK_LOCK        0xC1A0

#define CKGB_FSCFG_NDIV      (1L<<0)
#define CKGB_FSCFG_GOODREF   (0)
#define CKGB_FSCFG_VBADREF   (1L<<1)
#define CKGB_FSCFG_BADREF    (2L<<1)
#define CKGB_FSCFG_VGOODREF  (3L<<1)
#define CKGB_FSCFG_NOT_RESET (1L<<3)
#define CKGB_FSCFG_POWERUP   (1L<<4)

#define CKGB_CFG_PIX_SD_FS0_DIV2    (0)
#define CKGB_CFG_PIX_SD_FS0_DIV4    (1L<<10)
#define CKGB_CFG_DISP_ID_FS1_DIV2   (0)
#define CKGB_CFG_656_FS0_DIV2       (0)
#define CKGB_CFG_656_FS0_DIV4       (1L<<6)
#define CKGB_CFG_DISP_HD_FS0_DIV2   (0)
#define CKGB_CFG_DISP_HD_FS0_DIV4   (1L<<4)
#define CKGB_CFG_DISP_HD_FS0_DIV8   (2L<<4)
#define CKGB_CFG_PIX_HDMI_FS0_DIV2  (0)
#define CKGB_CFG_PIX_HDMI_FS0_DIV4  (1L<<2)
#define CKGB_CFG_PIX_HDMI_FS0_DIV8  (2L<<2)
#define CKGB_CFG_TMDS_HDMI_FS0_DIV2 (0)
#define CKGB_CFG_TMDS_HDMI_FS0_DIV4 (1L<<0)

#define CKGB_SRC_GDPx_FS1_NOT_FS0   (1L<<0)
#define CKGB_SRC_PIXSD_FS1_NOT_FS0  (1L<<1)
#define CKGB_SRC_DVP_FS1_NOT_FS0    (1L<<2)
#define CKGB_SRC_DVP_FSYNTH_NOT_EXT (1L<<3)

#define CKGB_EN_DAA                 (1L<<0)
#define CKGB_EN_DSS                 (1L<<1)
#define CKGB_EN_BCH_HDMI            (1L<<2)
#define CKGB_EN_PIX_HD              (1L<<3)
#define CKGB_EN_DISP_HD             (1L<<4)
#define CKGB_EN_TMDS_HDMI           (1L<<5)
#define CKGB_EN_656                 (1L<<6)
#define CKGB_EN_GDP2                (1L<<7)
#define CKGB_EN_DISP_ID             (1L<<8)
#define CKGB_EN_PIX_SD              (1L<<9)
#define CKGB_EN_PIX_HDMI            (1L<<10)
#define CKGB_EN_PIX_PIPE            (1L<<11)
#define CKGB_EN_PIX_TTXT            (1L<<12)
#define CKGB_EN_PIX_LPC             (1L<<13)

#define CKGB_OUT_SEL_PIX_HD          (3L)
#define CKGB_OUT_SEL_DISP_HD         (4L)
#define CKGB_OUT_SEL_656             (5L)
#define CKGB_OUT_SEL_DISP_SD         (7L)
#define CKGB_OUT_SEL_PIX_SD          (8L)

#define CKGB_REF_SEL_INTERNAL       (0)
#define CKGB_REF_SEL_EXTERNAL       (1L<<0)

/* STb7100 Sysconfig reg3, HDMI PLL and analogue DAC control -----------------*/
#define SYS_CFG3 0x10C

#define SYS_CFG3_DAC_SD_HZW_DISABLE      (1L<<0)
#define SYS_CFG3_DAC_SD_HZV_DISABLE      (1L<<1)
#define SYS_CFG3_DAC_SD_HZU_DISABLE      (1L<<2)
#define SYS_CFG3_DAC_HD_HZW_DISABLE      (1L<<3)
#define SYS_CFG3_DAC_HD_HZV_DISABLE      (1L<<4)
#define SYS_CFG3_DAC_HD_HZU_DISABLE      (1L<<5)
#define SYS_CFG3_TST_DAC_SD_CMDS         (1L<<6)
#define SYS_CFG3_TST_DAC_SD_CMDR         (1L<<7)
#define SYS_CFG3_TST_DAC_HD_CMDS         (1L<<8)
#define SYS_CFG3_TST_DAC_HD_CMDR         (1L<<9)
#define SYS_CFG3_S_HDMI_RST_N            (1L<<11)
#define SYS_CFG3_PLL_S_HDMI_EN           (1L<<12)
#define SYS_CFG3_PLL_S_HDMI_PDIV_SHIFT   (13)
#define SYS_CFG3_PLL_S_HDMI_PDIV_MASK    (7L<<SYS_CFG3_PLL_S_HDMI_PDIV_SHIFT)
#define SYS_CFG3_PLL_S_HDMI_NDIV_SHIFT   (16)
#define SYS_CFG3_PLL_S_HDMI_NDIV_MASK    (0xFF<<SYS_CFG3_PLL_S_HDMI_NDIV_SHIFT)
#define SYS_CFG3_PLL_S_HDMI_MDIV_SHIFT   (24)
#define SYS_CFG3_PLL_S_HDMI_MDIV_MASK    (0xFF<<SYS_CFG3_PLL_S_HDMI_MDIV_SHIFT)

/* STb7109Cut3 Sysconfig reg34, HDMI BCH clock divider */
#define SYS_CFG34 0x188

#define SYS_CFG34_CLK_BCH_HDMI_DIVSEL    (1L<<0)

#endif //STb7100REG_H
