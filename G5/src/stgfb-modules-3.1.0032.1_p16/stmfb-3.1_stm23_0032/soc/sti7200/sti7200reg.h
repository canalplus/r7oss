/***********************************************************************
 *
 * File: soc/sti7200/sti7200reg.h
 * Copyright (c) 2007,2008 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef _STI7200REG_H
#define _STI7200REG_H

/* STi7200 Base addresses ----------------------------------------------------*/
#define STi7200_REGISTER_BASE          0xFD000000

/* STi7200 Base address size -------------------------------------------------*/
#define STi7200_REG_ADDR_SIZE          0x00A80000

#define STi7200_PIO3                   0x00023000
#define STi7200_PIO4                   0x00024000
#define STi7200_PIO5                   0x00025000

#define STi7200C1_HDMI_BASE            0x00106000
#define STi7200C2_HDMI_BASE            0x00112000

#define STi7200C1_LOCAL_DISPLAY_BASE   0x0010C000
#define STi7200C2_LOCAL_DISPLAY_BASE   0x00A48000

#define STi7200C1_LOCAL_AUX_TVOUT_BASE 0x0010C500
#define STi7200C2_LOCAL_AUX_TVOUT_BASE 0x00112E00 /* In HDMI Register space like 7111 */

#define STi7200C1_LOCAL_FORMATTER_BASE 0x0010D800
#define STi7200C2_LOCAL_FORMATTER_BASE 0x00A48800 /* Moved to 0x800 from the local display base as in 7111 */
#define STi7200C1_LOCAL_FORMATTER_AWG_SIZE     50
#define STi7200C2_LOCAL_FORMATTER_AWG_SIZE     64

#define STi7200C1_REMOTE_DENC_BASE     0x0010F000
#define STi7200C1_VTG3_BASE            0x0010F200
#define STi7200C1_REMOTE_TVOUT_BASE    0x0010F400
#define STi7200C2_REMOTE_DENC_BASE     0x00A50000
#define STi7200C2_VTG3_BASE            0x00A50200
#define STi7200C2_REMOTE_TVOUT_BASE    0x00115000

#define STi7200_AUDCFG_BASE            0x00601000
#define STi7200_CLKGEN_BASE            0x00701000
#define STi7200_SYSCFG_BASE            0x00704000

#define STi7200_BLITTER_BASE           0x00940A00

#define STi7200_FLEXVP_BASE            0x00A00000
#define STi7200_LOCAL_COMPOSITOR_BASE  0x00A41000
#define STi7200_REMOTE_COMPOSITOR_BASE 0x00A42000
#define STi7200_LOCAL_DEI_BASE         0x00A43000
#define STi7200_REMOTE_DEI_BASE        0x00A44000

/* HDMI offset addresses -----------------------------------------------------*/
#define STi7200_HDMI_SPDIF_OFFSET      0xC00
#define STi7200_HDMI_PCMPLAYER_OFFSET  0xD00

/* Local display offset addresses --------------------------------------------*/
#define STi7200_LOCAL_DENC_OFFSET      0x000
#define STi7200_VTG2_OFFSET            0x200
#define STi7200_VTG1_OFFSET            0x300
#define STi7200_MAIN_TVOUT_OFFSET      0x400
#define STi7200_FDVO_OFFSET            0x600

/* The compositor (COMP) offset addresses ------------------------------------*/
#define STi7200_CUR_OFFSET     0x000
#define STi7200_GDP1_OFFSET    0x100
#define STi7200_GDP2_OFFSET    0x200
#define STi7200_GDP3_OFFSET    0x300
#define STi7200_GDP4_OFFSET    0x400
#define STi7200_ALP_OFFSET     0x600
#define STi7200_VID1_OFFSET    0x700
#define STi7200_VID2_OFFSET    0x800
#define STi7200_MIXER1_OFFSET  0xC00
#define STi7200_MIXER2_OFFSET  0xD00
#define STi7200_CAPTURE_OFFSET 0xE00

/* STi7200 Display Clockgen B registers --------------------------------------*/
#define CLKB_FS0_SETUP    0x000
#define CLKB_FS1_SETUP    0x004
#define CLKB_FS2_SETUP    0x008
#define CLKB_FS0_CLK1_CFG 0x00C
#define CLKB_FS0_CLK2_CFG 0x010
#define CLKB_FS0_CLK3_CFG 0x014
#define CLKB_FS0_CLK4_CFG 0x018
#define CLKB_FS1_CLK1_CFG 0x01C
#define CLKB_FS1_CLK2_CFG 0x020
#define CLKB_FS1_CLK3_CFG 0x024
#define CLKB_FS1_CLK4_CFG 0x028
#define CLKB_FS2_CLK1_CFG 0x02C
#define CLKB_FS2_CLK2_CFG 0x030
#define CLKB_FS2_CLK3_CFG 0x034
#define CLKB_FS2_CLK4_CFG 0x038

#define CLKB_CLKRCV_CFG   0x040
#define CLKB_OUT_MUX_CFG  0x048
#define CLKB_POWER_CFG    0x058

#define CLKB_FS_SETUP_NDIV       (1L<<0)
#define CLKB_FS_SETUP_NRST       (1L<<2)
#define CLKB_FS_SETUP_BW_SHIFT   3
#define CLKB_FS_SETUP_BW_MASK    (3L<<CLKB_FS_SETUP_BW_SHIFT)

#define CLKB_CLKCFG_PE_SHIFT     0
#define CLKB_CLKCFG_PE_MASK      (0xffff << CLKB_CLKCFG_PE_SHIFT)
#define CLKB_CLKCFG_MD_SHIFT     16
#define CLKB_CLKCFG_MD_MASK      (0x1f << CLKB_CLKCFG_MD_SHIFT)
#define CLKB_CLKCFG_SDIV_SHIFT   22
#define CLKB_CLKCFG_SDIV_MASK    (0x7 << CLKB_CLKCFG_SDIV_SHIFT)
#define CLKB_CLKCFG_EN_PRG       (1L << 21)
#define CLKB_CLKCFG_SELCLKOUT    (1L << 25)
#define CLKB_CLKCFG_SDIV3        (1L << 27)

#define CLKB_CLKRCV_FS0_CH1_EN   (1L<<0)
#define CLKB_CLKRCV_FS0_CH2_EN   (1L<<1)
#define CLKB_CLKRCV_FS1_CH1_EN   (1L<<4)
#define CLKB_CLKRCV_FS1_CH2_EN   (1L<<5)

#define CLKB_OUT_MUX_216_FS1_CH2 (1L<<13)
/*
 * Cut1
 */
#define CLKBC1_OUT_MUX_SD0_EXT   (1L<<14)
#define CLKBC1_OUT_MUX_SD1_EXT   (1L<<15)
/*
 * Cut2
 */
#define CLKBC2_OUT_MUX_SD0_DIV2  (1L<<14)
#define CLKBC2_OUT_MUX_SD1_DIV2  (1L<<15)
#define CLKBC2_OUT_MUX_SD0_EXT   (1L<<16)
#define CLKBC2_OUT_MUX_SD1_EXT   (1L<<17)

#define CLKB_POWER_FS0_ANA_POFF  (1L<<0)
#define CLKB_POWER_FS1_ANA_POFF  (1L<<1)
#define CLKB_POWER_FS2_ANA_POFF  (1L<<2)
#define CLKB_POWER_FS0_CH1_POFF  (1L<<3)
#define CLKB_POWER_FS0_CH2_POFF  (1L<<4)
#define CLKB_POWER_FS1_CH1_POFF  (1L<<7)
#define CLKB_POWER_FS1_CH2_POFF  (1L<<8)

/* STi7200 Sysstatus reg2, HDMI rejection PLL lock ---------------------------*/
#define SYS_STA2 0x10
#define SYS_STA2_HDMI_PLL_LOCK           (1L<<2)


/* STi7200 Sysconfig reg1, Analogue DAC control ------------------------------*/
#define SYS_CFG1 0x104

#define SYS_CFG1_DAC_SD0_HZW_DISABLE     (1L<<0)
#define SYS_CFG1_DAC_SD0_HZV_DISABLE     (1L<<1)
#define SYS_CFG1_DAC_SD0_HZU_DISABLE     (1L<<2)
#define SYS_CFG1_DAC_SD1_HZW_DISABLE     (1L<<3)
#define SYS_CFG1_DAC_SD1_HZV_DISABLE     (1L<<4)
#define SYS_CFG1_DAC_SD1_HZU_DISABLE     (1L<<5)
#define SYS_CFG1_DAC_HD_HZW_DISABLE      (1L<<6)
#define SYS_CFG1_DAC_HD_HZV_DISABLE      (1L<<7)
#define SYS_CFG1_DAC_HD_HZU_DISABLE      (1L<<8)

/* STi7200 Sysconfig reg3, HDMI PLL control ----------------------------------*/
#define SYS_CFG3 0x10C

#define SYS_CFG3_S_HDMI_RST_N            (1L<<0)
#define SYS_CFG3_PLL_S_HDMI_EN           (1L<<1)
#define SYS_CFG3_PLL_S_HDMI_PDIV_SHIFT   (2)
#define SYS_CFG3_PLL_S_HDMI_PDIV_MASK    (7L<<SYS_CFG3_PLL_S_HDMI_PDIV_SHIFT)
#define SYS_CFG3_PLL_S_HDMI_NDIV_SHIFT   (5)
#define SYS_CFG3_PLL_S_HDMI_NDIV_MASK    (0xFF<<SYS_CFG3_PLL_S_HDMI_NDIV_SHIFT)
#define SYS_CFG3_PLL_S_HDMI_MDIV_SHIFT   (13)
#define SYS_CFG3_PLL_S_HDMI_MDIV_MASK    (0xFF<<SYS_CFG3_PLL_S_HDMI_MDIV_SHIFT)

/* STi7200 Sysconfig reg7, PIO Config      -----------------------------------*/
#define SYS_CFG7 0x11C

#define SYS_CFG7_PIO_SYNC_VTG_C (1L<<17)
#define SYS_CFG7_PIO_PAD_0      (1L<<24)

/* STi7200Cut2 Sysconfig reg19, HDMI Phy pre-emption Config ------------------*/
#define SYS_CFG19 0x14C

#define SYS_CFG19_HDMIPHY_PREEMP_ON          (1L<<0)
#define SYS_CFG19_HDMIPHY_PREEMP_STR_SHIFT   (1)
#define SYS_CFG19_HDMIPHY_PREEMP_STR_MASK    (3L<<SYS_CFG19_HDMIPHY_PREEMP_STR_SHIFT)
#define SYS_CFG19_HDMIPHY_PREEMP_WIDTH_SHIFT (3)
#define SYS_CFG19_HDMIPHY_PREEMP_WIDTH_MASK  (7L<<SYS_CFG19_HDMIPHY_PREEMP_WIDTH_SHIFT)
#define SYS_CFG19_HDMIPHY_PREEMP_WIDTH_EXT   (1L<<6)

/* STi7200 Sysconfig reg21, HDMI power off -----------------------------------*/
#define SYS_CFG21 0x154

#define SYS_CFG21_HDMI_POFF (1L<<3)

/* STi7200 Sysconfig reg40, DVO/DVP ------------------------------------------*/
#define SYS_CFG40 0x1A0

#define SYS_CFG40_DVO_OUT_PIO_EN    (1L<<0)
#define SYS_CFG40_DVO_AUX_NOT_MAIN  (1L<<1)
#define SYS_CFG40_DVO_TO_DVP_EN     (1L<<2)
#define SYS_CFG40_DVP_IN_PIO_EN     (1L<<16)

/* STi7200 Sysconfig reg46, HDMI PHY compensation bits 31:0 ------------------*/
#define SYS_CFG46 0x1B8

/* STi7200 Sysconfig reg47, HDMI PHY configuration ---------------------------*/
#define SYS_CFG47 0x1BC

#define SYS_CFG47_HDMIPHY_USER_COMP_MASK    0xffff
#define SYS_CFG47_HDMIPHY_USER_COMP_EN      (1L<<16)
#define SYS_CFG47_HDMIPHY_BUFFER_SPEED_16   (0)
#define SYS_CFG47_HDMIPHY_BUFFER_SPEED_8    (1L<<17)
#define SYS_CFG47_HDMIPHY_BUFFER_SPEED_4    (3L<<17)
#define SYS_CFG47_HDMIPHY_BUFFER_SPEED_MASK (3L<<17)

/* Sti7200Cut2 Sysconfig reg49, Video clock configuration --------------------*/
#define SYS_CFG49 0x1C4

#define SYS_CFG49_PIXEL_HD0_DIV_SHIFT        (0)
#define SYS_CFG49_PIXEL_HD0_DIV(x)           ((x)<<SYS_CFG49_PIXEL_HD0_DIV_SHIFT)
#define SYS_CFG49_PIXEL_MAIN_COMPO_DIV_SHIFT (2)
#define SYS_CFG49_PIXEL_MAIN_COMPO_DIV(x)    ((x)<<SYS_CFG49_PIXEL_MAIN_COMPO_DIV_SHIFT)
#define SYS_CFG49_PIXEL_AUX_COMPO_DIV_SHIFT  (4)
#define SYS_CFG49_PIXEL_AUX_COMPO_DIV(x)     ((x)<<SYS_CFG49_PIXEL_AUX_COMPO_DIV_SHIFT)
#define SYS_CFG49_TMDS_DIV_SHIFT             (6)
#define SYS_CFG49_TMDS_DIV(x)                ((x)<<SYS_CFG49_TMDS_DIV_SHIFT)
#define SYS_CFG49_CH34_DIV_SHIFT             (8)
#define SYS_CFG49_CH34_DIV(x)                ((x)<<SYS_CFG49_CH34_DIV_SHIFT)
#define SYS_CFG49_CLK_DIVIDES_EN             (1L<<12)

/*
 * Note names used are consistent with names of bits used for Cut1 in
 * the HDMI GPOUT register, not the 7200Cut2 datasheet.
 */
#define SYS_CFG49_REMOTE_PIX_HD              (1L<<16)
#define SYS_CFG49_LOCAL_AUX_PIX_HD           (1L<<17)
#define SYS_CFG49_LOCAL_GDP3_SD              (1L<<18)
#define SYS_CFG49_LOCAL_VDP_SD               (1L<<19)
#define SYS_CFG49_LOCAL_CAP_SD               (1L<<20)
#define SYS_CFG49_LOCAL_SDTVO_HD             (1L<<21)
#define SYS_CFG49_REMOTE_SDTVO_HD            (1L<<22)


#endif // _STI7200REG_H
