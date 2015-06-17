/***********************************************************************
 *
 * File: soc/sti7105/sti7105reg.h
 * Copyright (c) 2008 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef _STI7105REG_H
#define _STI7105REG_H

#include <soc/sti7111/sti7111reg.h>

#define STi7105_FLEXDVO1_BASE            (STi7111_TVOUT_FDMA_BASE + 0x0600)

#define CKGB_CFG_656_1(x)           ((x)<<2)


/* STi7105 Sysconfig reg2, HDMI power off & PIO9/7 hotplug control -----------*/
#define SYS_CFG2_BCH_HDMI_BCH_DIVSEL       (1L<<28)
#define SYS_CFG2_CEC_RX_EN                 (1L<<29)
#define SYS_CFG2_HDMI_AUDIO_8CH_N_2CH      (1L<<30)

/* STi7105 Sysconfig reg3, Analogue DAC & HDMI PLL control -------------------*/
#define SYS_CFG3_DVO_TO_DVP_EN             (1L<<10)

/* STi7105 Sysconfig reg6, Digital Video out config -------------  -----------*/
#define SYS_CFG6_BOT_N_TOP_INVERSION       (1L<<1)
#define SYS_CFG6_DVO0_H_NOT_V              (1L<<2)
#define SYS_CFG6_DVO0_REF_NOT_SYNC         (1L<<3)
#define SYS_CFG6_DVO0_AUX_NOT_MAIN         (1L<<4)
#define SYS_CFG6_DVO0_OLD                  (1L<<5)
#define SYS_CFG6_DVO1_H_NOT_V              (1L<<12)
#define SYS_CFG6_DVO1_REF_NOT_SYNC         (1L<<13)
#define SYS_CFG6_DVO1_AUX_NOT_MAIN         (1L<<14)
#define SYS_CFG6_DVO1_OLD                  (1L<<15)

#endif // _STI7105REG_H
