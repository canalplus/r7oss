/***********************************************************************
 *
 * File: STMCommon/stmbdispregs.h
 * Copyright (c) 2007 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef _STM_BDISP_REGS_H
#define _STM_BDISP_REGS_H

#define BDISP_CTL      0x0
#define BDISP_ITS      0x4
#define BDISP_STA      0x8
#define BDISP_SSBA1    0x10
#define BDISP_SSBA2    0x14
#define BDISP_SSBA3    0x18
#define BDISP_SSBA4    0x1C
#define BDISP_SSBA5    0x20
#define BDISP_SSBA6    0x24
#define BDISP_SSBA7    0x28
#define BDISP_SSBA8    0x2C
#define BDISP_STBA1    0x30
#define BDISP_STBA2    0x34
#define BDISP_STBA3    0x38
#define BDISP_STBA4    0x3C
#define BDISP_CQ1_BASE 0x40
#define BDISP_CQ2_BASE 0x50
#define BDISP_AQ1_BASE 0x60
#define BDISP_AQ2_BASE 0x70
#define BDISP_AQ3_BASE 0x80
#define BDISP_AQ4_BASE 0x90
#define BDISP_SSBA9    0xA0
#define BDISP_SSBA10   0xA4
#define BDISP_SSBA11   0xA8
#define BDISP_SSBA12   0xAC
#define BDISP_SSBA13   0xB0
#define BDISP_SSBA14   0xB4
#define BDISP_SSBA15   0xB8
#define BDISP_SSBA16   0xBC
#define BDISP_SGA1     0xC0
#define BDISP_SGA2     0xC4
#define BDISP_PRI      0xF8

/*
 * Memory PLUG setup registers
 */
#define BDISP_S1_MAGIC      0x100
#define BDISP_S1_MAX_OPCODE 0x104
#define BDISP_S1_CHUNK      0x108
#define BDISP_S1_MESSAGE    0x10C
#define BDISP_S1_PAGE       0x110

#define BDISP_S2_MAGIC      0x120
#define BDISP_S2_MAX_OPCODE 0x124
#define BDISP_S2_CHUNK      0x128
#define BDISP_S2_MESSAGE    0x12C
#define BDISP_S2_PAGE       0x130

#define BDISP_S3_MAGIC      0x140
#define BDISP_S3_MAX_OPCODE 0x144
#define BDISP_S3_CHUNK      0x148
#define BDISP_S3_MESSAGE    0x14C
#define BDISP_S3_PAGE       0x150

#define BDISP_XYL_MAGIC      0x160
#define BDISP_XYL_MAX_OPCODE 0x164
#define BDISP_XYL_CHUNK      0x168
#define BDISP_XYL_MESSAGE    0x16C
#define BDISP_XYL_PAGE       0x170

#define BDISP_TARGET_MAGIC      0x180
#define BDISP_TARGET_MAX_OPCODE 0x184
#define BDISP_TARGET_CHUNK      0x188
#define BDISP_TARGET_MESSAGE    0x18C
#define BDISP_TARGET_PAGE       0x190


#define BDISP_CTL_BIG_NOT_LITTLE    (1L<<28)
#define BDISP_CTL_STEP_BY_STEP      (1L<<29)
#define BDISP_CTL_GLOBAL_SOFT_RESET (1L<<31)

#define BDISP_ITS_CQ_COMPLETED      (1L)
#define BDISP_ITS_CQ_RETRIGGERED    (2L)
#define BDISP_ITS_CQ_NODE_NOTIFY    (4L)
#define BDISP_ITS_CQ_REPACED        (8L)
#define BDISP_ITS_CQ_MASK           (0xF)
#define BDISP_ITS_CQ1_SHIFT         (0)
#define BDISP_ITS_CQ2_SHIFT         (4)

#define BDISP_ITS_AQ_LNA_REACHED    (1L)
#define BDISP_ITS_AQ_STOPPED        (2L)
#define BDISP_ITS_AQ_NODE_REPEAT    (4L)
#define BDISP_ITS_AQ_NODE_NOTIFY    (8L)
#define BDISP_ITS_AQ_MASK           (0xf)
#define BDISP_ITS_AQ1_SHIFT         (12)
#define BDISP_ITS_AQ2_SHIFT         (16)
#define BDISP_ITS_AQ3_SHIFT         (20)
#define BDISP_ITS_AQ4_SHIFT         (24)

#define BDISP_STA_IDLE              (1L<<0)

#define BDISP_PRI_MASK              0xF
#define BDISP_PRI_CQ1_SHIFT         (0)
#define BDISP_PRI_CQ2_SHIFT         (4)
#define BDISP_PRI_AQ1_SHIFT         (12)
#define BDISP_PRI_AQ2_SHIFT         (16)
#define BDISP_PRI_AQ3_SHIFT         (20)
#define BDISP_PRI_AQ4_SHIFT         (24)

/*
 * Composition queue offsets and defines
 */
#define BDISP_CQ_TRIG_IP  0x0
#define BDISP_CQ_TRIG_CTL 0x4
#define BDISP_CQ_PACE_CTL 0x8
#define BDISP_CQ_IP       0xC

#define BDISP_CQ_TRIG_CTL_LINE_NUM_MASK      0x0FFF
#define BDISP_CQ_TRIG_CTL_NO_TRIGGER         (0)
#define BDISP_CQ_TRIG_CTL_TOP_FIELD_LINE     (1L<<12)
#define BDISP_CQ_TRIG_CTL_TOP_FIELD_VSYNC    (2L<<12)
#define BDISP_CQ_TRIG_CTL_TOP_FIELD_PACE     (3L<<12)
#define BDISP_CQ_TRIG_CTL_BOT_FIELD_LINT     (5L<<12)
#define BDISP_CQ_TRIG_CTL_BOT_FIELD_VSYNC    (6L<<12)
#define BDISP_CQ_TRIG_CTL_BOT_FIELD_PACE     (7L<<12)
#define BDISP_CQ_TRIG_CTL_IMMEDIATE          (8L<<12)
#define BDISP_CQ_TRIG_CTL_COMPLETED_IRQ_EN   (1L<<20)
#define BDISP_CQ_TRIG_CTL_RETRIGGERED_IRQ_EN (1L<<21)
#define BDISP_CQ_TRIG_CTL_NODE_NOTIFY_IRQ_EN (1L<<22)
#define BDISP_CQ_TRIG_CTL_REPACED_IRQ_EN     (1L<<23)
#define BDISP_CQ_TRIG_CTL_RETRIGGER_FINISH   (1L<<24)
#define BDISP_CQ_TRIG_CTL_RETRIGGER_SUSPEND  (2L<<24)
#define BDISP_CQ_TRIG_CTL_RETRIGGER_ABORT    (4L<<24)
#define BDISP_CQ_TRIG_CTL_RETRIGGER_STOP     (8L<<24)
#define BDISP_CQ_TRIG_CTL_QUEUE_EN           (1L<<31)

/*
 * This is the definition for 7109. Earlier versions of the
 * IP had a different layout for this register, so beware if you are familiar
 * with that.
 */
#define BDISP_CQ_PACE_CTL_START_MASK         0x0FFF
#define BDISP_CQ_PACE_CTL_COUNT_SHIFT        16
#define BDISP_CQ_PACE_CTL_COUNT_MASK         (0x0FFF << BDISP_CQ_PACE_CTL_COUNT_SHIFT)
#define BDISP_CQ_PACE_CTL_DYNAMIC            (1L<<31)

/*
 * Application queue offsets and defines
 */
#define BDISP_AQ_CTL      0x0
#define BDISP_AQ_IP       0x4
#define BDISP_AQ_LNA      0x8
#define BDISP_AQ_STA      0xc

#define BDISP_AQ_CTL_PRIORITY_MASK      0x3
#define BDISP_AQ_CTL_IRQ_MASK           (0xF << 20)
#define BDISP_AQ_CTL_NODE_REPEAT_INT    (1L  << 20)
#define BDISP_AQ_CTL_QUEUE_STOPPED_INT  (2L  << 20)
#define BDISP_AQ_CTL_LNA_INT            (4L  << 20)
#define BDISP_AQ_CTL_NODE_COMPLETED_INT (8L  << 20)
#define BDISP_AQ_CTL_EVENT_MASK         (0x7 << 24)
#define BDISP_AQ_CTL_EVENT_SUSPEND      (0)
#define BDISP_AQ_CTL_EVENT_ABORT_REPEAT (2L  << 24)
#define BDISP_AQ_CTL_EVENT_STOP         (4L  << 24)
#define BDISP_AQ_CTL_QUEUE_EN           (1L  << 31)

/*
 * BDisp CIC
 */
#define BLIT_CIC_GROUP2                 (1L<<2)
#define BLIT_CIC_GROUP3                 (1L<<3)
#define BLIT_CIC_GROUP4                 (1L<<4)
#define BLIT_CIC_GROUP5                 (1L<<5)
#define BLIT_CIC_GROUP6                 (1L<<6)
#define BLIT_CIC_GROUP7                 (1L<<7)
#define BLIT_CIC_GROUP8                 (1L<<8)
#define BLIT_CIC_GROUP9                 (1L<<9)
#define BLIT_CIC_GROUP10                (1L<<10)
#define BLIT_CIC_GROUP11                (1L<<11)
#define BLIT_CIC_GROUP12                (1L<<12)
#define BLIT_CIC_GROUP13                (1L<<13)
#define BLIT_CIC_GROUP14                (1L<<14)
#define BLIT_CIC_GROUP15                (1L<<15)
#define BLIT_CIC_GROUP16                (1L<<16)
#define BLIT_CIC_GROUP17                (1L<<17)
#define BLIT_CIC_GROUP18                (1L<<18)

/*
 * BDisp specific node register defines
 */
#define BLIT_INS_ENABLE_DEI             (1L<<13)
#define BLIT_INS_ENABLE_DOT_SELECTOR    (1L<<16)
#define BLIT_INS_ENABLE_VC1R            (1L<<17)
#define BLIT_INS_ENABLE_PACING          (1L<<30)
#define BLIT_INS_ENABLE_COMPLETION_IRQ  (1L<<31)

#define BLIT_TY_A1_SUBST                (1L<<22)

#define BLIT_CCO_CLUT_SRC1_N_SRC2       (1L<<15)

#define BLIT_RZI_H_INIT_SHIFT           (0)
#define BLIT_RZI_H_INIT_MASK            (0x3FFL << BLIT_RZI_H_INIT_SHIFT)
#define BLIT_RZI_H_REPEAT_SHIFT         (12)
#define BLIT_RZI_H_REPEAT_MASK          (0x7L << BLIT_RZI_H_REPEAT_SHIFT)
#define BLIT_RZI_V_INIT_SHIFT           (16)
#define BLIT_RZI_V_INIT_MASK            (0x3FFL << BLIT_RZI_V_INIT_SHIFT)
#define BLIT_RZI_V_REPEAT_SHIFT         (28)
#define BLIT_RZI_V_REPEAT_MASK          (0x7L << BLIT_RZI_V_REPEAT_SHIFT)

#define BLIT_SAR_S1BA_SHIFT             (0)
#define BLIT_SAR_S1BA_1                 (1L  << BLIT_SAR_S1BA_SHIFT)
#define BLIT_SAR_S1BA_2                 (2L  << BLIT_SAR_S1BA_SHIFT)
#define BLIT_SAR_S1BA_3                 (3L  << BLIT_SAR_S1BA_SHIFT)
#define BLIT_SAR_S1BA_4                 (4L  << BLIT_SAR_S1BA_SHIFT)
#define BLIT_SAR_S1BA_MASK              (0x7L<< BLIT_SAR_S1BA_SHIFT)

#define BLIT_SAR_TBA_SHIFT              (24)
#define BLIT_SAR_TBA_1                  (1L  << BLIT_SAR_TBA_SHIFT)
#define BLIT_SAR_TBA_2                  (2L  << BLIT_SAR_TBA_SHIFT)
#define BLIT_SAR_TBA_3                  (3L  << BLIT_SAR_TBA_SHIFT)
#define BLIT_SAR_TBA_4                  (4L  << BLIT_SAR_TBA_SHIFT)
#define BLIT_SAR_TBA_MASK               (0x7L<< BLIT_SAR_TBA_SHIFT)

#define BDISP_GROUP_0 \
  ULONG BLT_NIP;      \
  ULONG BLT_CIC;      \
  ULONG BLT_INS;      \
  ULONG BLT_ACK

#define BDISP_GROUP_1 \
  ULONG BLT_TBA;      \
  ULONG BLT_TTY;      \
  ULONG BLT_TXY;      \
  ULONG BLT_TSZ

#define BDISP_GROUP_2 \
  ULONG BLT_S1CF;     \
  ULONG BLT_S2CF

#define BDISP_GROUP_3 \
  ULONG BLT_S1BA;     \
  ULONG BLT_S1TY;     \
  ULONG BLT_S1XY;     \
  ULONG BLT_UDF1

#define BDISP_GROUP_4 \
  ULONG BLT_S2BA;     \
  ULONG BLT_S2TY;     \
  ULONG BLT_S2XY;     \
  ULONG BLT_S2SZ

#define BDISP_GROUP_5 \
  ULONG BLT_S3BA;     \
  ULONG BLT_S3TY;     \
  ULONG BLT_S3XY;     \
  ULONG BLT_S3SZ

#define BDISP_GROUP_6 \
  ULONG BLT_CWO;      \
  ULONG BLT_CWS

#define BDISP_GROUP_7 \
  ULONG BLT_CCO;      \
  ULONG BLT_CML

#define BDISP_GROUP_8 \
  ULONG BLT_F_RZC;    \
  ULONG BLT_PMK

#define BDISP_GROUP_9 \
  ULONG BLT_RSF;      \
  ULONG BLT_RZI;      \
  ULONG BLT_HFP;      \
  ULONG BLT_VFP

#define BDISP_GROUP_10 \
  ULONG BLT_Y_RSF;     \
  ULONG BLT_Y_RZI;     \
  ULONG BLT_Y_HFP;     \
  ULONG BLT_Y_VFP

#define BDISP_GROUP_11 \
  ULONG BLT_FF0;       \
  ULONG BLT_FF1;       \
  ULONG BLT_FF2;       \
  ULONG BLT_FF3

#define BDISP_GROUP_12 \
  ULONG BLT_KEYS[2]

#define BDISP_GROUP_14 \
  ULONG BLT_SAR;       \
  ULONG BLT_USR

#define BDISP_GROUP_13 \
  ULONG BLT_XYL;       \
  ULONG BLT_XYP

#define BDISP_GROUP_15 \
  ULONG BLT_IVMX[4]

#define BDISP_GROUP_16 \
  ULONG BLT_OVMX[4]

#endif /* _STM_BDISP_REGS_H */
