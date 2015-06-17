/***********************************************************************
 *
 * File: STMCommon/stmhdmiregs.h
 * Copyright (c) 2008 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef _STMHDMI_REGS_H
#define _STMHDMI_REGS_H

#define STM_HDMI_CFG               0x000
#define STM_HDMI_INT_EN            0x004
#define STM_HDMI_INT_STA           0x008
#define STM_HDMI_INT_CLR           0x00C
#define STM_HDMI_STA               0x010
#define STM_HDMI_EXTS_MAX_DLY      0x014
#define STM_HDMI_EXTS_MIN          0x018
#define STM_HDMI_DVI_CONTROL_BITS  0x01C
#define STM_HDMI_GPOUT             0x020 /* Generic register name */
#define STM_HDMI_SYNC_CFG          0x020 /* 7109Cut3 datasheet name for GPOUT*/
#define STM_HDMI_ACTIVE_VID_XMIN   0x100
#define STM_HDMI_ACTIVE_VID_XMAX   0x104
#define STM_HDMI_ACTIVE_VID_YMIN   0x108
#define STM_HDMI_ACTIVE_VID_YMAX   0x10C
#define STM_HDMI_DEFAULT_CHL0_DATA 0x110
#define STM_HDMI_DEFAULT_CHL1_DATA 0x114
#define STM_HDMI_DEFAULT_CHL2_DATA 0x118
#define STM_HDMI_CHL0_CAP_DATA     0x11C
#define STM_HDMI_AUDIO_CFG         0x200
#define STM_HDMI_SPDIF_FIFO_STATUS 0x204
#define STM_HDMI_IFRAME_HEAD_WD    0x210
#define STM_HDMI_IFRAME_PKT_WD0    0x214
#define STM_HDMI_IFRAME_PKT_WD1    0x218
#define STM_HDMI_IFRAME_PKT_WD2    0x21C
#define STM_HDMI_IFRAME_PKT_WD3    0x220
#define STM_HDMI_IFRAME_PKT_WD4    0x224
#define STM_HDMI_IFRAME_PKT_WD5    0x228
#define STM_HDMI_IFRAME_PKT_WD6    0x22C
#define STM_HDMI_IFRAME_CFG        0x230
#define STM_HDMI_DI_DMA_CFG        0x234 /* HDMI v2.9 cell only */
#define STM_HDMI_IFRAME_FIFO_STA   0x240 /* Not in HDMI v2.9 cell, >= 7106 */
#define STM_HDMI_SAMPLE_FLAT_MASK  0x244
#define STM_HDMI_GEN_CTRL_PKT_CFG  0x248
#define STM_HDMI_AUDN              0x400
#define STM_HDMI_AUD_CTS           0x404 /* Not in all devices */
/* The following are for the HDMI v2.9 cell */
#define STM_HDMI_SRZ_PLL_CFG       0x504
#define STM_HDMI_SRZ_TAP_1         0x508
#define STM_HDMI_SRZ_TAP_2         0x50C
#define STM_HDMI_SRZ_TAP_3         0x510
#define STM_HDMI_SRZ_CTRL          0x514
#define STM_HDMI_DI2_HEAD_WD       0x600
#define STM_HDMI_DI3_HEAD_WD       0x620
#define STM_HDMI_DI4_HEAD_WD       0x640
#define STM_HDMI_DI5_HEAD_WD       0x660
#define STM_HDMI_DI6_HEAD_WD       0x680

#define STM_HDMI_CFG_EN                        (1L<<0)
#define STM_HDMI_CFG_HDMI_NOT_DVI              (1L<<1)
#define STM_HDMI_CFG_CP_EN                     (1L<<2)
#define STM_HDMI_CFG_ESS_NOT_OESS              (1L<<3)
#define STM_HDMI_CFG_SYNC_POL_NEG              (1L<<4)
#define STM_HDMI_V29_CFG_H_SYNC_POL_NEG        (1L<<4)
#define STM_HDMI_V29_CFG_SINK_TERM_DETECT_EN   (1L<<5)
#define STM_HDMI_V29_CFG_V_SYNC_POL_NEG        (1L<<6)
#define STM_HDMI_CFG_422_EN                    (1L<<8)
#define STM_HDMI_CFG_PIXEL_RPT_EN              (1L<<9) /* TMDS = 2*PIXCLOCK */
#define STM_HDMI_V29_422_DECIMATION_BYPASS     (1L<<9)
#define STM_HDMI_CFG_DLL_CFG_MASK              (3L<<10)
#define STM_HDMI_CFG_DLL_CFG_SHIFT             (10)
#define STM_HDMI_CFG_DLL_1X_TMDS               (1L<<STM_HDMI_CFG_DLL_CFG_SHIFT)
#define STM_HDMI_CFG_DLL_2X_TMDS               (2L<<STM_HDMI_CFG_DLL_CFG_SHIFT)
#define STM_HDMI_CFG_DLL_4X_TMDS               (3L<<STM_HDMI_CFG_DLL_CFG_SHIFT)
#define STM_HDMI_V29_422_DROP_CB_N_CR          (1L<<10)
/*#define STM_HDMI_CFG_BCH_CLK_4XTMDS_NOT_2XTMDS (1L<<12) Obsolete, functionality removed from HW */
#define STM_HDMI_V29_VID_FIFO_OVERRUN_CLR      (1L<<12)
#define STM_HDMI_V29_VID_FIFO_UNDERRUN_CLR     (1L<<13)
#define STM_HDMI_V29_VID_PIX_REP_SHIFT         (16)
#define STM_HDMI_V29_VID_PIX_REP_1             (0L<<STM_HDMI_V29_VID_PIX_REP_SHIFT)
#define STM_HDMI_V29_VID_PIX_REP_2             (1L<<STM_HDMI_V29_VID_PIX_REP_SHIFT)
#define STM_HDMI_V29_VID_PIX_REP_3             (2L<<STM_HDMI_V29_VID_PIX_REP_SHIFT)
#define STM_HDMI_V29_VID_PIX_REP_4             (3L<<STM_HDMI_V29_VID_PIX_REP_SHIFT)
#define STM_HDMI_V29_VID_PIX_REP_MASK          (3L<<STM_HDMI_V29_VID_PIX_REP_SHIFT)
#define STM_HDMI_V29_SINK_SUPPORTS_DEEP_COLOR  (1L<<30)
#define STM_HDMI_CFG_SW_RST_EN                 (1L<<31)

#define STM_HDMI_INT_GLOBAL                    (1L<<0)
#define STM_HDMI_INT_SW_RST                    (1L<<1)
#define STM_HDMI_INT_IFRAME                    (1L<<2)
#define STM_HDMI_INT_PIX_CAP                   (1L<<3)
#define STM_HDMI_INT_HOT_PLUG                  (1L<<4)
#define STM_HDMI_INT_DLL_LCK                   (1L<<5)
#define STM_HDMI_INT_NEW_FRAME                 (1L<<6)
#define STM_HDMI_INT_GENCTRL_PKT               (1L<<7)
#define STM_HDMI_INT_SPDIF_FIFO_OVERRUN        (1L<<8)
/*
 * The following interrupts are for v2.9
 */
#define STM_HDMI_INT_VID_FIFO_UNDERRUN         (1L<<9)
#define STM_HDMI_INT_VID_FIFO_OVERRUN          (1L<<10)
#define STM_HDMI_INT_SINK_TERM_PRESENT         (1L<<11)
#define STM_HDMI_INT_DI_2                      (1L<<16)
#define STM_HDMI_INT_DI_3                      (1L<<17)
#define STM_HDMI_INT_DI_4                      (1L<<18)
#define STM_HDMI_INT_DI_5                      (1L<<19)
#define STM_HDMI_INT_DI_6                      (1L<<20)
#define STM_HDMI_INT_DI_DMA_VSYNC_DONE         (1L<<21)
#define STM_HDMI_INT_DI_VSYNC_DONE             (1L<<22)

#define STM_HDMI_STA_SW_RST                    (1L<<1)
#define STM_HDMI_STA_INFO_BUF                  (1L<<2) /* Not in HDMI v2.9 cell */
#define STM_HDMI_STA_PIX_CAP                   (1L<<3)
#define STM_HDMI_STA_HOT_PLUG                  (1L<<4)
#define STM_HDMI_STA_DLL_LCK                   (1L<<5)

#define STM_HDMI_EXTS_MAX_DLY_DEFAULT          (0x2)
#define STM_HDMI_EXTS_MIN_DEFAULT              (0x32)

#define STM_HDMI_AUD_CFG_8CH_NOT_2CH           (1L<<0)
#define STM_HDMI_AUD_CFG_SPDIF_CLK_SHIFT        1
#define STM_HDMI_AUD_CFG_SPDIF_CLK_MASK        (3L<<STM_HDMI_AUD_CFG_SPDIF_CLK_SHIFT)
#define STM_HDMI_AUD_CFG_FIFO0_OVERRUN_CLR     (1L<<4)
#define STM_HDMI_AUD_CFG_FIFO1_OVERRUN_CLR     (1L<<5)
#define STM_HDMI_AUD_CFG_FIFO2_OVERRUN_CLR     (1L<<6)
#define STM_HDMI_AUD_CFG_FIFO3_OVERRUN_CLR     (1L<<7)
#define STM_HDMI_AUD_CFG_FIFO_OVERRUN_CLR      ( STM_HDMI_AUD_CFG_FIFO0_OVERRUN_CLR \
                                               | STM_HDMI_AUD_CFG_FIFO1_OVERRUN_CLR \
                                               | STM_HDMI_AUD_CFG_FIFO2_OVERRUN_CLR \
                                               | STM_HDMI_AUD_CFG_FIFO3_OVERRUN_CLR)
#define STM_HDMI_AUD_CFG_CTS_CAL_BYPASS        (1L<<8)
#define STM_HDMI_AUD_CFG_DATATYPE_SHIFT        (9)
#define STM_HDMI_AUD_CFG_DATATYPE_NORMAL       (0)
#define STM_HDMI_AUD_CFG_DATATYPE_ONEBIT       (1L<<STM_HDMI_AUD_CFG_DATATYPE_SHIFT)
#define STM_HDMI_AUD_CFG_DATATYPE_DST          (2L<<STM_HDMI_AUD_CFG_DATATYPE_SHIFT)
#define STM_HDMI_AUD_CFG_DATATYPE_HBR          (3L<<STM_HDMI_AUD_CFG_DATATYPE_SHIFT)
#define STM_HDMI_AUD_CFG_DATATYPE_MASK         (7L<<STM_HDMI_AUD_CFG_DATATYPE_SHIFT)
#define STM_HDMI_AUD_CFG_CTS_CLK_DIV_SHIFT     (12)
#define STM_HDMI_AUD_CFG_CTS_CLK_DIV_MASK      (3L<<STM_HDMI_AUD_CFG_CTS_CLK_DIV_SHIFT)
#define STM_HDMI_AUD_CFG_DST_DOUBLE_RATE       (1L<<15)
#define STM_HDMI_AUD_CFG_DST_SAMPLE_INVALID    (1L<<16)
#define STM_HDMI_AUD_CFG_ONEBIT_SP0_INVALID    (1L<<18)
#define STM_HDMI_AUD_CFG_ONEBIT_SP1_INVALID    (1L<<19)
#define STM_HDMI_AUD_CFG_ONEBIT_SP2_INVALID    (1L<<20)
#define STM_HDMI_AUD_CFG_ONEBIT_SP3_INVALID    (1L<<21)
#define STM_HDMI_AUD_CFG_ONEBIT_ALL_INVALID    ( STM_HDMI_AUD_CFG_ONEBIT_SP0_INVALID \
                                               | STM_HDMI_AUD_CFG_ONEBIT_SP1_INVALID \
                                               | STM_HDMI_AUD_CFG_ONEBIT_SP2_INVALID \
                                               | STM_HDMI_AUD_CFG_ONEBIT_SP3_INVALID )
/*
 * HDMI 1.3 cell audio config bits
 */
#define STM_HDMI_FOUR_SP_PER_TRANSFER          (1L<<22)
#define STM_HDMI_FORCED_HBR_BBITS              (1L<<23)
#define STM_HDMI_SELECT_SPDIF_IN_0             (1L<<27)

#define STM_HDMI_IFRAME_CFG_EN                 (1L<<0)
#define STM_HDMI_IFRAME_STA_BUSY               (1L<<0)

/*
 * HDMI 1.3 cell IFrame configuration bits
 */
#define STM_HDMI_IFRAME_DISABLED               (0)
#define STM_HDMI_IFRAME_SINGLE_SHOT            (1)
#define STM_HDMI_IFRAME_FIELD                  (2)
#define STM_HDMI_IFRAME_FRAME                  (3)
#define STM_HDMI_IFRAME_MASK                   (3)
#define STM_HDMI_IFRAME_CFG_DI_n(x,n)          ((x)<<((n-1)*4)) /* n is defined for 1-6 */

#define STM_HDMI_DI_DMA_SEL_DISABLED           (0)
#define STM_HDMI_DI_DMA_SEL_MASK               (0xf)
#define STM_HDMI_DI_DMA_REQ_VSYNC_SHIFT        (4)
#define STM_HDMI_DI_DMA_REQ_VSYNC_MASK         (0xff<<STM_HDMI_DI_DMA_REQ_VSYNC_SHIFT)
#define STM_HDMI_DI_DMA_REQ_TOTAL_SHIFT        (16)
#define STM_HDMI_DI_DMA_REQ_TOTAL_MASK         (0x3ff<<STM_HDMI_DI_DMA_REQ_TOTAL_SHIFT)

#define STM_HDMI_SAMPLE_FLAT_SP0               (1L<<0)
#define STM_HDMI_SAMPLE_FLAT_SP1               (1L<<1)
#define STM_HDMI_SAMPLE_FLAT_SP2               (1L<<2)
#define STM_HDMI_SAMPLE_FLAT_SP3               (1L<<3)
#define STM_HDMI_SAMPLE_FLAT_ALL               ( STM_HDMI_SAMPLE_FLAT_SP0 \
                                               | STM_HDMI_SAMPLE_FLAT_SP1 \
                                               | STM_HDMI_SAMPLE_FLAT_SP2 \
                                               | STM_HDMI_SAMPLE_FLAT_SP3)

#define STM_HDMI_GENCTRL_PKT_EN                 (1L<<0)
/* #define STM_HDMI_GENCTRL_PKT_BUF_NOT_REG       (1L<<1) Obsolete in HDMI1.3 cell and we could never make it work anyway */
#define STM_HDMI_GENCTRL_PKT_AVMUTE             (1L<<2)
/*
 * HDMI v2.9 cell defines for deepcolour support
 */
#define STM_HDMI_GENCTRL_PKT_COLORDEPTH_SHIFT   (4)
#define STM_HDMI_GENCTRL_PKT_COLORDEPTH_UNKNOWN (0x0)
#define STM_HDMI_GENCTRL_PKT_COLORDEPTH_24BIT   (0x4<<STM_HDMI_GENCTRL_PKT_COLORDEPTH_SHIFT)
#define STM_HDMI_GENCTRL_PKT_COLORDEPTH_30BIT   (0x5<<STM_HDMI_GENCTRL_PKT_COLORDEPTH_SHIFT)
#define STM_HDMI_GENCTRL_PKT_COLORDEPTH_36BIT   (0x6<<STM_HDMI_GENCTRL_PKT_COLORDEPTH_SHIFT)
#define STM_HDMI_GENCTRL_PKT_COLORDEPTH_48BIT   (0x7<<STM_HDMI_GENCTRL_PKT_COLORDEPTH_SHIFT)
#define STM_HDMI_GENCTRL_PKT_COLORDEPTH_MASK    (0xf<<STM_HDMI_GENCTRL_PKT_COLORDEPTH_SHIFT)
#define STM_HDMI_GENCTRL_PKT_DEFAULT_PHASE      (1L<<8)
#define STM_HDMI_GENCTRL_PKT_PACK_PHASE_SHIFT   (12)
#define STM_HDMI_GENCTRL_PKT_PACK_PHASE_4       (0x0)
#define STM_HDMI_GENCTRL_PKT_PACK_PHASE_1       (0x1)
#define STM_HDMI_GENCTRL_PKT_PACK_PHASE_2       (0x2)
#define STM_HDMI_GENCTRL_PKT_PACK_PHASE_3       (0x3)
#define STM_HDMI_GENCTRL_PKT_PACK_PHASE_MASK    (0xf)
#define STM_HDMI_GENCTRL_PKT_PACK_PHASE(x)      ((x)<<STM_HDMI_GENCTRL_PKT_PACK_PHASE_SHIFT)
#define STM_HDMI_GENCTRL_PKT_FORCE_PACK_PHASE   (1L<<16)

#define STM_HDMI_SRZ_PLL_CFG_POWER_DOWN         (1L<<0)
#define STM_HDMI_SRZ_PLL_CFG_VCOR_SHIFT         (1)
#define STM_HDMI_SRZ_PLL_CFG_VCOR_425MHZ        (0)
#define STM_HDMI_SRZ_PLL_CFG_VCOR_850MHZ        (1)
#define STM_HDMI_SRZ_PLL_CFG_VCOR_1700MHZ       (2)
#define STM_HDMI_SRZ_PLL_CFG_VCOR_2250MHZ       (3)
#define STM_HDMI_SRZ_PLL_CFG_VCOR_MASK          (3)
#define STM_HDMI_SRZ_PLL_CFG_VCOR(x)            ((x)<<STM_HDMI_SRZ_PLL_CFG_VCOR_SHIFT)
#define STM_HDMI_SRZ_PLL_CFG_NDIV_SHIFT         (8)
#define STM_HDMI_SRZ_PLL_CFG_NDIV_MASK          (0x1f<<STM_HDMI_SRZ_PLL_CFG_NDIV_SHIFT)
#define STM_HDMI_SRZ_PLL_CFG_MODE_SHIFT         (16)
#define STM_HDMI_SRZ_PLL_CFG_MODE_25_2_MHZ      (0x4)
#define STM_HDMI_SRZ_PLL_CFG_MODE_27_MHZ        (0x5)
#define STM_HDMI_SRZ_PLL_CFG_MODE_33_75_MHZ     (0x6)
#define STM_HDMI_SRZ_PLL_CFG_MODE_40_5_MHZ      (0x7)
#define STM_HDMI_SRZ_PLL_CFG_MODE_54_MHZ        (0x8)
#define STM_HDMI_SRZ_PLL_CFG_MODE_67_5_MHZ      (0x9)
#define STM_HDMI_SRZ_PLL_CFG_MODE_74_25_MHZ     (0xa)
#define STM_HDMI_SRZ_PLL_CFG_MODE_81_MHZ        (0xb)
#define STM_HDMI_SRZ_PLL_CFG_MODE_82_5_MHZ      (0xc)
#define STM_HDMI_SRZ_PLL_CFG_MODE_108_MHZ       (0xd)
#define STM_HDMI_SRZ_PLL_CFG_MODE_148_5_MHZ     (0xe)
#define STM_HDMI_SRZ_PLL_CFG_MODE_165_MHZ       (0xf)
#define STM_HDMI_SRZ_PLL_CFG_MODE_MASK          (0xf)
#define STM_HDMI_SRZ_PLL_CFG_MODE(x)            ((x)<<STM_HDMI_SRZ_PLL_CFG_MODE_SHIFT)
#define STM_HDMI_SRZ_PLL_CFG_SEL_12P5           (1L<<31)

#define STM_HDMI_SRZ_CTRL_POWER_DOWN            (1L<<0)
#define STM_HDMI_SRZ_CTRL_EXTERNAL_DATA_EN      (1L<<1)
#define STM_HDMI_SRZ_CTRL_SRC_TERM_SHIFT        (2)
#define STM_HDMI_SRZ_CTRL_SRC_TERM_OFF          (0)
#define STM_HDMI_SRZ_CTRL_SRC_TERM_65MV         (0x1)
#define STM_HDMI_SRZ_CTRL_SRC_TERM_84MV         (0x2)
#define STM_HDMI_SRZ_CTRL_SRC_TERM_105MV        (0x3)
#define STM_HDMI_SRZ_CTRL_SRC_TERM_MASK         (0x3)
#define STM_HDMI_SRZ_CTRL_SRC_TERM(x)           ((x)<<STM_HDMI_SRZ_CTRL_SRC_TERM_SHIFT)

#endif //_STMHDMI_REGS_H
