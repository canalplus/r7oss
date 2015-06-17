/*******************************************************************************
  Header file for stmfp platform data
  Copyright (C) 2009  STMicroelectronics Ltd

  This program is free software; you can redistribute it and/or modify it
  under the terms and conditions of the GNU General Public License,
  version 2, as published by the Free Software Foundation.

  This program is distributed in the hope it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
  more details.

  You should have received a copy of the GNU General Public License along
  with this program; if not, write to the Free Software Foundation, Inc.,
  51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.

  The full GNU General Public License is included in this distribution in
  the file called "COPYING".

  Author: manish rathi <manish.rathi@st.com>
*******************************************************************************/

#ifndef __STMFP_PLATFORM_DATA
#define __STMFP_PLATFORM_DATA

#include <linux/platform_device.h>
#include <linux/if.h>
#include <linux/bitops.h>

extern int fp_claim_resources(struct platform_device *pdev);
extern void fp_add_tcam_promisc(const void *netdev_log,
				const void *netdev_phy, int fp_idx);

#define NUM_PHY_INTFS 3
#define NUM_VIRT_INTFS 2
#define NUM_INTFS (NUM_PHY_INTFS + NUM_VIRT_INTFS)

struct stmfp_mdio_bus_data {
	int bus_id;
	int (*phy_reset)(void *priv);
	unsigned int phy_mask;
	int *irqs;
	int probed_phy_irq;
};


enum IF_DEVID { DEVID_DOCSIS , DEVID_GIGE0 , DEVID_GIGE1, DEVID_FP0, DEVID_FP1};
enum FP_VERSION { FP , FPLITE, FP2 };

struct plat_fpif_data {
	char *phy_bus_name;
	int bus_id;
	int phy_addr;
	int interface;
	int id;
	int iftype;
	int tso_enabled;
	int tx_dma_ch;
	int rx_dma_ch;
	char ifname[IFNAMSIZ];
	struct stmfp_mdio_bus_data *mdio_bus_data;
	int buf_thr;
	int q_idx;
};

struct plat_stmfp_data {
	int available_l2cam;
	int l2cam_size;
	int version;
	u32 fp_clk_rate;
	int common_cnt;
	int empty_cnt;
	struct plat_fpif_data *if_data[NUM_INTFS];
	/* Below callbacks are used mainly for FPGA-FP */
	void (*platinit)(void *fpgrp);
	void (*preirq)(void *fpgrp);
	void (*postirq)(void *fpgrp);
};

enum IFBITMAP {
	DEST_DOCSIS = BIT(DEVID_DOCSIS),
	DEST_GIGE = BIT(DEVID_GIGE0),
	DEST_ISIS = BIT(DEVID_GIGE1),
	DEST_APDMA = BIT(3),
	DEST_NPDMA = BIT(4),
	DEST_WDMA = BIT(5),
	DEST_RECIRC = BIT(6)
};

enum IF_SP {
	SP_DOCSIS = DEVID_DOCSIS,
	SP_GIGE = DEVID_GIGE0,
	SP_ISIS = DEVID_GIGE1,
	SP_SWDEF = 3,
	SP_ANY = 4
};

#define MNGL_IDX_WLAN0 3
#define MNGL_IDX_WLAN1 4

#endif
