/*
 * Copyright (C) 2008 STMicroelectronics Limited
 * Author: Angus Clark <angus.clark@st.com
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 */

#ifndef __LINUX_STM_NAND_H
#define __LINUX_STM_NAND_H

/* Timing Paramters for NAND Controller.  See ADCS #7864584: "NAND Flash support
 * upgrades for FMI Functional Secification".
 */
struct nand_timing_data {
	/* Times specified in ns.  (Will be rounded up to nearest multiple of
	   EMI clock period.) */
	int sig_setup;
	int sig_hold;
	int CE_deassert;
	int WE_to_RBn;

	int wr_on;
	int wr_off;

	int rd_on;
	int rd_off;

	int chip_delay;		/* delay in us */
};


#endif /* __LINUX_STM_NAND_H */
