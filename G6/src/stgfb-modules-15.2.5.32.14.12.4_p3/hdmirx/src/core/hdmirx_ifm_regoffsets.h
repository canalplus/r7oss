/***********************************************************************
 *
 * File: hdmirx/src/core/hdmirx_ifm_regoffsets.h
 * Copyright (c) 2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef __HDMIRX_IFM_REGOFFSETS_H__
#define __HDMIRX_IFM_REGOFFSETS_H__

#include <InternalTypes.h>

#define HDMI_RX_INST_SOFT_RESETS                0x0000
#define HDMI_RX_INST_HARD_RESETS                0x0004
#define HDMI_RX_HOST_CONTROL                    0x0008
#define HDMI_RX_AFR_CONTROL                     0x000C
#define HDMI_RX_AFB_CONTROL                     0x0010
#define HDMI_RX_INPUT_IRQ_MASK                  0x0014
#define HDMI_RX_INPUT_IRQ_STATUS                0x0018
#define HDMI_RX_TEST_IRQ                        0x001c
#define HDMI_RX_INSTRU_CLOCK_CONFIG             0x0020
#define HDMI_RX_IFM_CLK_CONTROL                 0x0024
#define HDMI_RX_INSTRUMENT_POLARITY_CONTROL     0x0028
/* 0x002C ~0x00FF -> Reserved */

/*HDMI PU Registers */
#define PU_CONFIG                               0x0140
#define HDRX_CRC_CTRL                           0x0144
#define HDRX_CRC_ERR_STATUS                     0x0148
#define HDRX_CRC_RED                            0x014C
#define HDRX_CRC_GREEN                          0x0150
#define HDRX_CRC_BLUE                           0x0154
#define PU_DEBUG_TEST_CTRL                      0x0158
#define PU_SPARE_REG                            0x015C
/* 0x0120 ~0x01FF -> Reserved */

/* IFM Register definitions */
#define HDMI_RX_IFM_CTRL                        0x0060
#define HDMI_RX_IFM_WATCHDOG                    0x0064
#define HDMI_RX_IFM_HLINE                       0x0068
#define HDMI_RX_IFM_CHANGE                      0x006C

#define HDMI_RX_IFM_HS_PERIOD                   0x0070
#define HDMI_RX_IFM_HS_PULSE                    0x0074
#define HDMI_RX_IFM_VS_PERIOD                   0x0078
#define HDMI_RX_IFM_VS_PULSE                    0x007C

#define HDMI_RX_INPUT_CONTROL                   0x00B0
#define HDMI_RX_INPUT_HS_DELAY                  0x00B4
#define HDMI_RX_INPUT_VS_DELAY                  0x00B8
#define HDMI_RX_INPUT_H_ACT_START               0x00BC
#define HDMI_RX_INPUT_H_ACT_WIDTH               0x00C0
#define HDMI_RX_INPUT_V_ACT_START_ODD           0x00C4
#define HDMI_RX_INPUT_V_ACT_START_EVEN          0x00C8
#define HDMI_RX_INPUT_V_ACT_LENGTH              0x00CC
#define HDMI_RX_INPUT_FREEZE_VIDEO              0x00D0
#define HDMI_RX_INPUT_FLAGLINE                  0x00D4
#define HDMI_RX_INPUT_VLOCK_EVENT               0x00D8
#define HDMI_RX_INPUT_FRAME_RESET_LINE          0x00DC
#define HDMI_RX_INPUT_REF_READY_LINE            0x00E0

#define HDMI_RX_INPUT_HMEASURE_START            0x00E4
#define HDMI_RX_INPUT_HMEASURE_WIDTH            0x00E8
#define HDMI_RX_INPUT_VMEASURE_START            0x00EC
#define HDMI_RX_INPUT_VMEASURE_LENGTH           0x00F0

#define HDMI_RX_INPUT_IBD_CONTROL               0x00F4
#define HDMI_RX_INPUT_IBD_HTOTAL                0x00F8
#define HDMI_RX_INPUT_IBD_VTOTAL                0x00FC
#define HDMI_RX_INPUT_IBD_HSTART                0x0100
#define HDMI_RX_INPUT_IBD_HWIDTH                0x0104
#define HDMI_RX_INPUT_IBD_VSTART                0x0108
#define HDMI_RX_INPUT_IBD_VLENGTH               0x010C

/* IFM_CTRL */
#define HDMI_IFM_EN                             BIT0
#define HDMI_IFM_MEASEN                         BIT1
#define HDMI_IFM_HOFFSETEN                      BIT2
#define HDMI_IFM_FIELD_SEL                      BIT3
#define HDMI_IFM_FIELD_EN                       BIT4
#define HDMI_IFM_INT_ODD_EN                     BIT5
#define HDMI_IFM_SPACE_SEL                      BIT6
#define HDMI_IFM_ODD_INV                        BIT7
#define HDMI_IFM_FIELD_DETECT_MODE              BIT8
#define HDMI_CLK_PER_FRAME_EN                   BIT9
#define HDMI_CLK_INTLC_EN                       BIT10
#define HDMI_IFM_FREERUN_ODD                    BIT11
#define HDMI_IFM_SOURCE_SEL                     0XF000
#define HDMI_WD_H_POLARITY                      BIT14
#define HDMI_WD_V_POLARITY                      BIT15

/* For HS and VS Period and Pulses */
#define HDMI_RX_IFM_HS_PERIODBitFieldMask               0x0000ffff
#define HDMI_RX_IFM_HS_PULSEBitFieldMask                0x0000ffff
#define HDMI_RX_IFM_VS_PERIODBitFieldMask               0x0000ffff
#define HDMI_RX_IFM_VS_PULSEBitFieldMask                0x0000ffff

/* For HDMI_RX_INPUT_H_ACT_START */
#define HDMI_RX_INPUT_H_ACT_STARTBitFieldMask           0x0000ffff

/* For HDMI_RX_INPUT_H_ACT_WIDTH */
#define HDMI_RX_INPUT_H_ACT_WIDTHBitFieldMask           0x0000ffff

/* For HDMI_RX_INPUT_V_ACT_START_ODD */
#define HDMI_RX_INPUT_V_ACT_START_ODDBitFieldMask       0x0000ffff

/* For HDMI_RX_INPUT_V_ACT_START_EVEN */
#define HDMI_RX_INPUT_V_ACT_START_EVENBitFieldMask      0x0000ffff

/* For HDMI_RX_INPUT_V_ACT_LENGTH */
#define HDMI_RX_INPUT_V_ACT_LENGTHBitFieldMask          0x0000ffff

/* For HDMI_IFM_Control */
#define HDMI_RX_INPUT_CONTROLIP_RUN_EN                  0x00000001
#define HDMI_RX_INPUT_CONTROLIP_INTLC_EN                0x00000004

/* HDMI_RX_INPUT_HMEASURE_START */
#define HDMI_RX_INPUT_HMEASURE_STARTBitFieldMask        0x0000ffff

/* HDMI_RX_INPUT_VMEASURE_START */
#define HDMI_RX_INPUT_VMEASURE_STARTBitFieldMask        0x0000ffff

/* HDMI_RX_INPUT_HMEASURE_WIDTH */
#define HDMI_RX_INPUT_HMEASURE_WIDTHBitFieldMask        0x0000ffff

/* HDMI_RX_INPUT_VMEASURE_LENGTH */
#define HDMI_RX_INPUT_VMEASURE_LENGTHBitFieldMask       0x0000ffff

/* For IBD RO Registers */
#define HDMI_INPUT_IBD_HTOTALBitFieldMask               0x0000ffff
#define HDMI_INPUT_IBD_VTOTALBitFieldMask               0x0000ffff
#define HDMI_INPUT_IBD_HSTARTBitFieldMask               0x0000ffff
#define HDMI_INPUT_IBD_HWIDTHBitFieldMask               0x0000ffff
#define HDMI_INPUT_IBD_VSTARTBitFieldMask               0x0000ffff
#define HDMI_INPUT_IBD_VLENGTHBitFieldMask              0x0000ffff

/* For HDMI_RX_INPUT_IBD_CONTROL */
#define HDMI_RX_INPUT_MEASURE_DE_nDATA          BIT0
#define HDMI_RX_INPUT_IBD_MEASWINDOW_EN         BIT2
#define HDMI_RX_INPUT_IBD_RGB_SEL_MASK          0x000C
#define HDMI_RX_INPUT_DET_THOLD_SEL_MASK        0x00F0
#define HDMI_RX_INPUT_IBD_EN                    BIT8

/* For HDMI Instrumentation Block */
/* HDMI_RX_INST_SOFT_RESETS */
#define HDMI_RX_IFM_RESET                       BIT0
#define HDMI_RX_IMD_RESET                       BIT1

/* HDMI_RX_INST_HARD_RESETS */
#define HDMI_RX_IFM_HARD_RESET                  BIT0
#define HDMI_RX_IMD_HARD_RESET                  BIT1

/* HDMI_RX_HOST_CONTROL */
#define LOCK_RD_REG                             BIT0
#define LOCK_VALID_IMD                          BIT1
#define LOCK_VALID_IFM                          BIT2

/* HDMI_RX_AFR_CONTROL */
#define HDMI_RX_AFR_DISP_IFM_ERR_EN             BIT0
#define HDMI_RX_AFR_DISP_IBD_ERR_EN             BIT1
#define HDMI_RX_AFR_DISP_CLK_ERR_ENN            BIT2
#define HDMI_RX_AFR_DISP_VID_MUTE_EN            BIT3
#define HDMI_RX_AFR_DDDS_IFM_ERR_EN             BIT4
#define HDMI_RX_AFR_DDDS_IBD_ERR_EN             BIT5
#define HDMI_RX_AFR_DDDS_CLK_ERR_EN             BIT6

/* HDMI_RX_AFB_CONTROL */
#define HDMI_RX_AFB_IFM_ERR_EN                  BIT0
#define HDMI_RX_AFB_IBD_ERR_EN                  BIT1
#define HDMI_RX_AFB_CLK_ERR_EN                  BIT2
#define HDMI_RX_AFB_VID_MUTE_EN                 BIT3

/* HDMI_RX_INPUT_IRQ_MASK */
#define HDMI_RX_INPUT_NO_HS_MASK                BIT0
#define HDMI_RX_INPUT_NO_VS_MASK                BIT1
#define HDMI_RX_INPUT_HS_PERIOD_ERR_MASK        BIT2
#define HDMI_RX_INPUT_VS_PERIOD_ERR_MASK        BIT3
#define HDMI_RX_INPUT_COMP_VBI_RDY_MASK         BIT4
#define HDMI_RX_INPUT_INTLC_ERR_MASK            BIT5
#define HDMI_RX_INPUT_ACTIVE_MASK               BIT6
#define HDMI_RX_INPUT_BLANK_MASK                BIT7
#define HDMI_RX_INPUT_LINEFLAG_MASK             BIT8
#define HDMI_RX_INPUT_VS_MASK                   BIT9
#define HDMI_RX_INPUT_AFR_DETECT_MASK           BIT10
#define HDMI_RX_VCR_MODE_MASK                   BIT11
#define HDMI_RX_TEST_IRQ_MASK                   BIT12

/* HDMI_RX_INPUT_IRQ_STATUS */
#define HDMI_RX_INPUT_NO_HS                     BIT0
#define HDMI_RX_INPUT_NO_VS                     BIT1
#define HDMI_RX_INPUT_HS_PERIOD_ERR             BIT2
#define HDMI_RX_INPUT_VS_PERIOD_ERR             BIT3
#define HDMI_RX_INPUT_COMP_VBI_RDY              BIT4
#define HDMI_RX_INPUT_INTLC_ERR                 BIT5
#define HDMI_RX_INPUT_VACTIVE                   BIT6
#define HDMI_RX_INPUT_VBLANK                    BIT7
#define HDMI_RX_INPUT_LINEFLAG                  BIT8
#define HDMI_RX_INPUT_VS                        BIT9
#define HDMI_RX_INPUT_AFR_DETECT                BIT10
#define HDMI_RX_VCR_MODE                        BIT11
/* #define HDMI_RX_TEST_IRQ                        BIT12 */

/* HDMI_GENERATE_TEST_IRQ */
#define HDMI_RX_GENERATE_TEST_IRQ               BIT0

/* HDMI_RX_IFM_CLK_CONTROL */
#define HDMI_RX_IFM_CLK_DISABLE                 BIT0

/* HDMI_RX_INSTRUMENT_POLARITY_CONTROL */
#define HDMI_RX_VS_POL                          BIT0
#define HDMI_RX_HS_POL                          BIT1
#define HDMI_RX_DV_POL                          BIT2

#define HDMI_RX_INSTRUMENT_POLARITY_CONTROLHDMI_VS_POLShift   0x00000000
#define HDMI_RX_INSTRUMENT_POLARITY_CONTROLHDMI_HS_POLShift   0x00000001
#define HDMI_RX_INSTRUMENT_POLARITY_CONTROLHDMI_DV_POLShift   0x00000002

#endif /* End of __HDMIRX_IFM_REGOFFSETS_H__ */
