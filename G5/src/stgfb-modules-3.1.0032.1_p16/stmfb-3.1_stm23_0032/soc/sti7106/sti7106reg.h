/***********************************************************************
 *
 * File: soc/sti7106/sti7106reg.h
 * Copyright (c) 2009 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef _STI7106REG_H
#define _STI7106REG_H

#include <soc/sti7105/sti7105reg.h>

/* STi7106 Sysconfig reg2, HDMI configuration --------------------------------*/

/*
 * HDMI PHY configuration is now part of the HDMI cell for HDMI1.3 deepcolor,
 * so we undefine the 7111/7105 PHY definitions.
 */
#undef SYS_CFG2_HDMIPHY_USER_COMP_MASK
#undef SYS_CFG2_HDMIPHY_USER_COMP_EN
#undef SYS_CFG2_HDMIPHY_BUFFER_SPEED_MASK
#undef SYS_CFG2_HDMIPHY_BUFFER_SPEED_16
#undef SYS_CFG2_HDMIPHY_BUFFER_SPEED_8
#undef SYS_CFG2_HDMIPHY_BUFFER_SPEED_4
#undef SYS_CFG2_HDMIPHY_PREEMP_ON
#undef SYS_CFG2_HDMIPHY_PREEMP_STR_SHIFT
#undef SYS_CFG2_HDMIPHY_PREEMP_STR_MASK
#undef SYS_CFG2_HDMIPHY_PREEMP_WIDTH_MASK
#undef SYS_CFG2_HDMIPHY_PREEMP_WIDTH_EXT
#undef SYS_CFG2_HDMI_POFF
#undef SYS_CFG2_HDMI_AUDIO_8CH_N_2CH

#define SYS_CFG2_HDMI_REJECTION_PLL_N_BYPASS (1L<<26)
#define SYS_CFG2_HDMI_TMDS_CLKB_N_PHY        (1L<<31)

/* STi7106 Sysconfig reg3, Analogue DAC & HDMI PLL control -------------------*/
#undef SYS_CFG3_DVO_TO_DVP_EN
#define SYS_CFG3_DVP_CLOCK_CLKB_N_PADS     (1L<<10)

/* STi7106 Sysconfig reg6, Digital Video out config -------------  -----------*/
#define SYS_CFG6_DVO0_CLOCK_INVERT         (1L<<16)
#define SYS_CFG6_DVO0_DATA_RETIME          (1L<<17)
#define SYS_CFG6_DVO1_CLOCK_INVERT         (1L<<18)
#define SYS_CFG6_DVO1_DATA_RETIME          (1L<<19)
#define SYS_CFG6_DVO_TO_DVP_DATA_EN        (1L<<24)

#endif // _STI7106REG_H
