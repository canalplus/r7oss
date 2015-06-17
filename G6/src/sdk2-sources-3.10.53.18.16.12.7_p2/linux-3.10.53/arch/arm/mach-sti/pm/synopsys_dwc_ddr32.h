/*
 * -------------------------------------------------------------------------
 * Copyright (C) 2014  STMicroelectronics
 * Author:	Sudeep Biswas		<sudeep.biswas@st.com>
 *		Francesco M. Virlinzi	<francesco.virlinzi@st.com>
 *
 * May be copied or modified under the terms of the GNU General Public
 * License V.2 ONLY.  See linux/COPYING for more information.
 * ------------------------------------------------------------------------- */

#ifndef __STI_SYNOPSYS_DWC_DDR32_H__
#define __STI_SYNOPSYS_DWC_DDR32_H__

/*
 * Synopsys DWC SDram Protocol Controller
 * For registers description see:
 * DesignWare Cores DDR3/2 SDRAM Protocol - Controller -
 * Databook - Version 2.10a - February 4, 2009'
 */
#define DDR_SCTL				0x4
#define DDR_SCTL_CFG				0x1
#define DDR_SCTL_GO				0x2
#define DDR_SCTL_SLEEP				0x3
#define DDR_SCTL_WAKEUP				0x4

#define DDR_STAT				0x8
#define DDR_STAT_MASK				0x7
#define DDR_STAT_CONFIG				0x1
#define DDR_STAT_ACCESS				0x3
#define DDR_STAT_LOW_POWER			0x5

#define DDR_DTU_CFG				0x208
#define DDR_DTU_CFG_ENABLE			0x1

#define DDR_PHY_IOCRV1				0x31C
/*
 * Synopsys DWC SDram Phy Controller
 * For registers description see:
 * 'DesignWare Cores DDR3/2 SDRAM PHY -
 *  Databook - February 5, 2009'
 *
 * - Table 5.1: PHY Control Register Mapping
 * and
 * - Table 5.30: PUB Control Register Mapping
 */
#define DDR_PHY_REG(idx)			(0x400 + (idx) * 4)

#define DDR_PHY_PIR				DDR_PHY_REG(1)	/* 0x04 */
#define DDR_PHY_PIR_PLL_RESET			(1 << 7)
#define DDR_PHY_PIR_PLL_PD			(1 << 8)

#define DDR_PHY_PGCR0				DDR_PHY_REG(2)	/* 0x08 */
#define DDR_PHY_PGCR1				DDR_PHY_REG(3)	/* 0x0c */

#define DDR_PHY_PGSR0				DDR_PHY_REG(4)	/* 0x10 */
#define DDR_PHY_PGSR0_INIT			(1 << 0)
#define DDR_PHY_PGSR0_PLLINIT			(1 << 1)
#define DDR_PHY_PGSR0_CAL			(1 << 2)
#define DDR_PHY_PGSR0_APLOCK			(1 << 5)

#define DDR_PHY_ACIOCR				DDR_PHY_REG(14)	/* 0x38 */
#define DDR_PHY_ACIOCR_ACOE		        (1 << 1)
#define DDR_PHY_ACIOCR_ACODT			(1 << 2)
#define DDR_PHY_ACIOCR_ACPDD			(1 << 3)
#define DDR_PHY_ACIOCR_ACPDR			(1 << 4)
#define DDR_PHY_ACIOCR_CKODT			(7 << 5)
#define DDR_PHY_ACIOCR_CKPDD			(7 << 8)
#define DDR_PHY_ACIOCR_CSPDD			(15 << 18)

#define DDR_PHY_DXCCR				DDR_PHY_REG(15) /* 0x3C */
#define DDR_PHY_DXCCR_DXODT			(1 << 0)
#define DDR_PHY_DXCCR_DXPDD			(1 << 3)
#define DDR_PHY_DXCCR_DXPDR			(1 << 4)

#define DDR_PHY_DSGCR				DDR_PHY_REG(16) /* 0x40 */
#define DDR_PHY_CKOE				(1 << 28)
#define DDR_PHY_ODTOE				(1 << 29)

#define DDR_PHY_PLLREADY			(DDR_PHY_PGSR0_INIT |	\
						DDR_PHY_PGSR0_PLLINIT |	\
						DDR_PHY_PGSR0_CAL |	\
						DDR_PHY_PGSR0_APLOCK)

/* DDR PLL Specific */
#define DDR_PLL_CFG_OFFSET			0x7d8
#define DDR_PLL_STATUS_OFFSET			0x8e4

/* DDR DTU Specific */
#define DTUWACTL				0x200
#define DTURACTL				0x204
#define DTUWD0					0x210
#define DTUWD1					0x214
#define DTUWD2					0x218
#define DTUWD3					0x21c
#define DTUAWDT					0x0b0
#define DTUCFG					0x208
#define DTUECTL					0x20c

#endif /* __STI_SYNOPSYS_DWC_DDR32_H__ */

