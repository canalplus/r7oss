/***************************************************************************
  Copyright (C) 2011-2014 STMicroelectronics Ltd

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
***************************************************************************/

#include <linux/stmfp.h>
#include <linux/phy.h>
#include <linux/interrupt.h>
#include "stmfp_tcam.h"

#define DRV_MODULE_VERSION "1.0"
#define DRV_NAME "fpif"

#define FP_NAPI_BUDGET 32
#define RXDMA_THRESH 0x80
#define DELAY_TX_INTRL 0x80
#define DELAY_TX_INTRH 0x80000
#define DELAY_TX_THR 32
/* #define DELAY_RX_INTR 0x8000 */
#define DELAY_RX_INTR 0x800
#define FP_HDR_SIZE 14
#define FP_L2CAM_SIZE 128

/* eth mtu:1500, ethhdr:14, fphdr:14 doublevlanid:4+4 */
#define MAX_PKT_SIZE    (ETH_FRAME_LEN + FP_HDR_SIZE + VLAN_HLEN * 2)
#define MAX_BUF_SIZE	MAX_PKT_SIZE
#define JUMBO_FRAME_SIZE 2000
#define MIN_ETH_FRAME_SIZE 46

#define FPIF_RX_RING_SIZE 256
#define RX_RING_MOD_MASK (FPIF_RX_RING_SIZE - 1)
#define FPIF_RX_BUFS (FPIF_RX_RING_SIZE - 1)

#define FPIF_TX_RING_SIZE 256
#define TX_RING_MOD_MASK (FPIF_TX_RING_SIZE - 1)
#define FP_TX_FREE_BUDGET 25
#define FP_TX_FREE_LIMIT 16

/* Register addresses */
#define FP_DEFAULT_INTERFACE PHY_INTERFACE_MODE_RGMII_ID
#define FASTPATH_BASE		0
#define FP_PHY_ADDR 0x1
#define FP_PHY_BUS_ID 1
#define FP_PHY_MASK 1
#define FP_CLK_RATE 250	/* 250 Mhz */

#define NUM_PORTS 4

#define FP_TOP_BASE (FASTPATH_BASE + 0x12000)

#define FP_SOFT_RST FP_TOP_BASE
#define FP_MDIO_CTL1 (FP_TOP_BASE + 0x8)
#define FP_SPARE_REG0 (FP_TOP_BASE + 0xc)
#define FP_SPARE_REG1 (FP_TOP_BASE + 0x10)
#define FP_MISC (FP_TOP_BASE + 0x14)
#define FP_PORTSETTINGS_LO               (FP_TOP_BASE + 0x40)
#define FP_PORTSETTINGS_HI               (FP_TOP_BASE + 0x44)

#define INGRESS_BASE (FASTPATH_BASE + 0x14000)
#define EGRESS_BASE (FASTPATH_BASE + 0x17000)
#define FP_IMUX_TXDMA_RATE_CONTROL (INGRESS_BASE + 0xC)
#define FP_IMUX_PKTC (INGRESS_BASE + 0x214)
#define FP_IMUX_BYTEC (INGRESS_BASE + 0x218)
#define IMUX_RPT_OFF 64
#define FP_IMUX_TXDMA_TOE_RATE_CONTROL (INGRESS_BASE + 0x10)

#define FP_DEFRAG_BASE (FASTPATH_BASE + 0x18000)
#define FP_DEFRAG_CNTRL (FP_DEFRAG_BASE)

#define RGMII0_OFFSET 0x13000
#define RGMII1_OFFSET 0x19000

#define RGMII_GLOBAL_MACINFO3 0x32c
#define RGMII_MACINFO0 0x200

#define RGMII_RX_STAT_RESET 0x800
#define RGMII_RX_BYTE_COUNT_HI 0x808
#define RGMII_RX_BYTE_COUNT_LO 0x80C
#define RGMII_RX_ERROR_COUNT 0x814
#define RGMII_RX_UNICAST_COUNT_HI 0x820
#define RGMII_RX_UNICAST_COUNT_LO 0x824
#define RGMII_RX_MCAST_COUNT_HI 0x828
#define RGMII_RX_MCAST_COUNT_LO 0x82C
#define RGMII_RX_BCAST_COUNT_HI 0x830
#define RGMII_RX_BCAST_COUNT_LO 0x834
#define RGMII_RX_FCS_ERR_CNT 0x83C
#define RGMII_RX_OVERSIZED_ERR_CNT 0x844
#define RGMII_RX_SYMBOL_ERR_CNT 0x84c
#define RGMII_RX_ALIGN_ERR_CNT 0x854

#define RGMII_TX_STAT_RESET 0x880
#define RGMII_TX_BYTE_COUNT_HI 0x888
#define RGMII_TX_BYTE_COUNT_LO 0x88C
#define RGMII_TX_ABORT_COUNT 0x894
#define RGMII_TX_CMPL_COUNT_HI 0x8b8
#define RGMII_TX_CMPL_COUNT_LO 0x8bc
#define RGMII_TX_1COLL_COUNT 0x8cc
#define RGMII_TX_MULT_COLL_COUNT 0x8d4
#define RGMII_TX_DEFER_COUNT 0x8dc
#define RGMII_TX_LATE_COLL 0x8e4
#define RGMII_TX_EXCESS_COLL 0x8ec
#define RGMII_TX_ABORT_INTERR_COLL 0x8f4

#define L2CAM_BASE (FASTPATH_BASE + 0x16000)
#define L2_CAM_MAC_DA_LOW L2CAM_BASE
#define L2_CAM_MAC_DA_HIGH (L2CAM_BASE + 4)
#define L2_CAM_CFG_MODE (L2CAM_BASE + 0x10)
#define L2_CAM_CFG_COMMAND (L2CAM_BASE + 0x14)
#define L2_CAM_CFG_STATUS (L2CAM_BASE + 0x18)

#define QMAN_BASE (FASTPATH_BASE + 0x10000)
#define QOS_Q_BASE (QMAN_BASE + 0x800)
#define QMAN_CTRL_BASE                   (QMAN_BASE + 0x000)

#define SU_Q_GLOBAL_PACKET_RESERVE       (QMAN_CTRL_BASE + 0x044)
#define SU_Q_GLOBAL_BUFFER_RESERVE       (QMAN_CTRL_BASE + 0x048)
#define SU_Q_PACKET_RESERVE              (QMAN_CTRL_BASE + 0x04C)
#define SU_Q_BUFFER_RESERVE              (QMAN_CTRL_BASE + 0x050)
#define SU_Q_MAX_PKT_G 0x40
#define SU_Q_MAX_BUF_G 0x80
#define SU_Q_MAX_PKT 0x10
#define SU_Q_MAX_BUF 0x20


#define STARTUP_Q_BASE                   (QMAN_CTRL_BASE + 0x200)
#define STARTUP_Q_RPT_OFF                0x40

#define SU_Q_RESET_STATS                 (STARTUP_Q_BASE + 0x0)
#define SU_Q_FASTPATH_ID                 (STARTUP_Q_BASE + 0x4)
#define SU_Q_BUSY                        (STARTUP_Q_BASE + 0x8)
#define SU_Q_RELEASE                     (STARTUP_Q_BASE + 0xC)

#define QOS_Q_RPT_OFFSET                 0x80

#define QOS_Q_START_PTR                  (QOS_Q_BASE + 0x00)
#define QOS_Q_END_PTR                    (QOS_Q_BASE + 0x04)
#define QOS_Q_CONTROL                    (QOS_Q_BASE + 0x10)
#define QOS_Q_THRES_0                    (QOS_Q_BASE + 0x1C)
#define QOS_Q_THRES_1                    (QOS_Q_BASE + 0x20)
#define QOS_Q_THRES_2                    (QOS_Q_BASE + 0x24)
#define QOS_Q_DROP_ENTRY_LIMIT           (QOS_Q_BASE + 0x28)
#define QOS_Q_BUFF_RSRV                  (QOS_Q_BASE + 0x2C)

#define QOS_Q_SRR_INFO                   (QOS_Q_BASE + 0x30)
#define QOS_Q_DROP_COUNT                 (QOS_Q_BASE + 0x34)
#define QOS_Q_PASS_COUNT                 (QOS_Q_BASE + 0x38)
#define QOS_Q_GLOBAL_POOL_EMPTY_COUNT    (QOS_Q_BASE + 0x3C)
#define QOS_Q_THRESHOLD_0_PASS_COUNT     (QOS_Q_BASE + 0x40)
#define QOS_Q_THRESHOLD_1_PASS_COUNT     (QOS_Q_BASE + 0x44)
#define QOS_Q_THRESHOLD_2_PASS_COUNT     (QOS_Q_BASE + 0x48)
#define QOS_Q_THRESHOLD_0_DROP_COUNT     (QOS_Q_BASE + 0x4C)
#define QOS_Q_THRESHOLD_1_DROP_COUNT     (QOS_Q_BASE + 0x50)
#define QOS_Q_THRESHOLD_2_DROP_COUNT     (QOS_Q_BASE + 0x54)
#define QOS_Q_MAX_PACKET_COUNT           (QOS_Q_BASE + 0x58)
#define QOS_Q_MAX_BUFFER_COUNT           (QOS_Q_BASE + 0x5C)
#define QOS_Q_CLEAR_STATS               (QOS_Q_BASE + 0x60)

#define QOS_CTRL_BASE                    (QMAN_BASE + 0xF80)
#define QOS_TRANSMIT_DESCRIPTOR          (QOS_CTRL_BASE + 0x00)
#define QOS_Q_COMMON_CNT_THRESH          (QOS_CTRL_BASE + 0x20)
#define QOS_Q_COMMON_CNT_EMPTY_COUNT     (QOS_CTRL_BASE + 0x24)
#define QOS_Q_SRR_BIT_RATE_CTRL          (QOS_CTRL_BASE + 0x38)

#define DEVID_RXDMA 3
#define DEVID_RECIRC 4
#define NUM_QOS_QUEUES 15
#define DOCSIS_QOS_START 0
#define DOCSIS_QOS_END 3
#define GIGE_QOS_START 4
#define GIGE_QOS_END 7
#define ISIS_QOS_START 8
#define ISIS_QOS_END 8
#define NUM_STARTUP_QUEUES 8
#define NUM_QOS_DESCRIPTORS              8
#define QOS_DESCRIPTOR_RPT_OFF           0x04
#define NUM_QOS_Q_SRR_BIT_RATE_CTRLS     2
#define QOS_Q_SRR_BIT_RATE_CTRL_OFF      0x04

#define PORT_SETTINGS                    4
#define PORT_SETTINGS_RPT_OFF            0x08

#define EMUX_THRESHOLDS                  4
#define EMUX_THRESHOLD_RPT_OFF           0x40
#define FP_EMUX_CNTRL                    (EGRESS_BASE + 0x100)
#define FP_EMUX_THRESHOLD                (EGRESS_BASE + 0x104)
#define FP_EMUX_PACKET_COUNT		(EGRESS_BASE + 0x110)
#define FP_EMUX_BYTE_COUNT		(EGRESS_BASE + 0x114)

#define EMUX_MIN_LENGTHS                 3
#define EMUX_MIN_LENGTH_RPT_OFF          0x40
#define FP_EMUX_MIN_LENGTH               (EGRESS_BASE + 0x108)

#define FP_EMUX_STATS_SETS               4
#define FP_EMUX_STATS_RPT_OFF            0x40
#define FP_EMUX_MAX_DEPTH                (EGRESS_BASE + 0x10C)
#define FP_EMUX_PACKET_COUNT             (EGRESS_BASE + 0x110)
#define FP_EMUX_BYTE_COUNT               (EGRESS_BASE + 0x114)
#define FP_EMUX_DROP_PACKET_COUNT        (EGRESS_BASE + 0x118)
#define FP_EMUX_ERROR_COUNT              (EGRESS_BASE + 0x120)
#define FP_EMUX_RESTART_COUNT            (EGRESS_BASE + 0x124)
#define FP_EMUX_FLUSH_PACKET_COUNT       (EGRESS_BASE + 0x128)
#define FP_EMUX_FLUSH_BYTE_COUNT         (EGRESS_BASE + 0x12C)

#define FP_EMUX_MIN_LENGTH_RECIRC        (EGRESS_BASE + 0x1C8)

#define FASTPATH_TXDMA_BASE (FASTPATH_BASE + 0xc000)
#define FASTPATH_TOE_BASE (FASTPATH_BASE + 0xe000)

#define FPTXDMA_IRQ_FLAGS (FASTPATH_TXDMA_BASE + 0x0)
#define FPTXDMA_IRQ_ENABLES_0 (FASTPATH_TXDMA_BASE + 0x4)
#define FPTXDMA_BPAI_CLEAR (FASTPATH_TXDMA_BASE + 0x50)
#define FPTXDMA_BPAI_PRIORITY (FASTPATH_TXDMA_BASE + 0x54)
#define FPTXDMA_T3W_CONFIG (FASTPATH_TXDMA_BASE + 0x40)
#define FPTXDMA_ENDIANNESS (FASTPATH_TXDMA_BASE + 0x4c)
#define FPTXDMA_BPAI_CPU_0 (FASTPATH_TXDMA_BASE + 0x80)
#define FPTXDMA_BPAI_IP_0 (FASTPATH_TXDMA_BASE + 0x84)
#define FPTXDMA_BPAI_DONE_0 (FASTPATH_TXDMA_BASE + 0x88)
#define FPTXDMA_PKTCNTR_BUFFER_0 (FASTPATH_TXDMA_BASE + 0x100)
#define FPTXDMA_BUFPTR (FASTPATH_TXDMA_BASE + 0x800)

#define FPTOE_BPAI_CLEAR (FASTPATH_TOE_BASE + 0x50)
#define FPTOE_BPAI_PRIORITY (FASTPATH_TOE_BASE + 0x54)
#define FPTOE_T3W_CONFIG (FASTPATH_TOE_BASE + 0x40)
#define FPTOE_ENDIANNESS (FASTPATH_TOE_BASE + 0x4c)
#define FPTOE_MAX_NONSEG (FASTPATH_TOE_BASE + 0x5c)


#define FASTPATH_RXDMA_BASE (FASTPATH_BASE + 0x11000)
#define FPRXDMA_IRQ_FLAGS (FASTPATH_RXDMA_BASE + 0x0)
#define FPRXDMA_IRQ_ENABLES_0 (FASTPATH_RXDMA_BASE + 0x4)
#define FPRXDMA_ENDIANNESS (FASTPATH_RXDMA_BASE + 0x4c)
#define FPRXDMA_T3R_CONFIG (FASTPATH_RXDMA_BASE + 0x40)
#define FPRXDMA_BPAI_CLEAR (FASTPATH_RXDMA_BASE + 0x50)
#define FPRXDMA_BPAI_CPU_0 (FASTPATH_RXDMA_BASE + 0x80)
#define FPRXDMA_BPAI_IP_0 (FASTPATH_RXDMA_BASE + 0x84)
#define FPRXDMA_BPAI_DONE_0 (FASTPATH_RXDMA_BASE + 0x88)
#define FPRXDMA_PKTCNTR_BUFFER_0 (FASTPATH_RXDMA_BASE + 0x118)
#define FPRXDMA_BUFPTR (FASTPATH_RXDMA_BASE + 0x400)

/* Hash Def of register masks */
#define FPHDR_SP_MASK 0xc00
#define FPHDR_SP_SHIFT 10
#define FPHDR_BRIDGE_MASK 0x100
#define FPHDR_CSUM_MASK 0x80
#define FPHDR_DEST_MASK 0x7f
#define FPHDR_DEST_SHIFT 0
#define FPHDR_MANGLELIST_MASK 0x1ff
#define FPHDR_NEXTHOP_IDX_MASK 0xff
#define FPHDR_SMAC_IDX_MASK 0x700
#define FPHDR_SMAC_IDX_SHIFT 8
#define FPHDR_PROTO_MASK 0xc0000000
#define FPHDR_LEN_MASK 0xffff0000
#define FPHDR_LEN_SHIFT 16
#define FPHDR_IFIDX_SHIFT 16
#define FPHDR_DONE_MASK 0x40000
#define FPHDR_TSO_LEN_MASK 0xc0000000
#define FPHDR_NOMANGLE 7
#define FPHDR_PROTO_SHIFT 30
#define FPHDR_L3_SHIFT 25
#define FPHDR_L4_SHIFT 16
#define FPHDR_MSS_SHIFT 9
#define FPHDR_PROT_TCP 3
#define FPHDR_TCP_MSS_WORD 362


#define FPRXDMA0_IRQ_MASK 7
#define FPTXDMA0_IRQ_MASK 7

#define L2CAM_BRIDGE_SHIFT 31
#define L2CAM_DP_SHIFT 24
#define L2CAM_SP_SHIFT 16
#define L2CAM_IDX_SHIFT 0
#define L2CAM_IDX_MASK 0x7f
#define L2CAM_STS_MASK 0x7f
#define L2CAM_STS_SHIFT 8
#define L2CAM_COLL_MASK 8
#define L2CAM_CLEAR 2
#define L2CAM_READ 3
#define L2CAM_DEL 1
#define L2CAM_ADD 0
#define SW_MANAGED 1
#define HW_MANAGED 0

#define MACINFO_DUPLEX 0x4000000
#define MACINFO_HALF_DUPLEX (0 << 26)
#define MACINFO_FULL_DUPLEX (1 << 26)
#define MACINFO_SPEED 0x3000000
#define MACINFO_SPEED_1000 (2 << 24)
#define MACINFO_SPEED_100 (1 << 24)
#define MACINFO_SPEED_10 0
#define MACINFO_RXEN (1 << 23)
#define MACINFO_TXEN (1 << 22)
#define MACINFO_RGMII_MODE (1 << 11)
#define MACINFO_DONTDECAPIQ (1 << 13)
#define MACINFO_MTU1 (1 << 14)
#define MACINFO_FLOWCTRL_REACTION_EN (1 << 18)

#define MISC_DEFRAG_EN (1 << 5)
#define MISC_PASS_BAD (1 << 4)

#define DEFRAG_REPLACE (1 << 4)
#define DEFRAG_PAD_REMOVAL (1 << 1)

#define PKTLEN_ERR (1 << 12)
#define MALFORM_PKT (1 << 11)
#define EARLY_EOF (1 << 10)
#define L4_CSUM_ERR (1 << 9)
#define IPV4_L3_CSUM_ERR (1 << 8)
#define IP_SRC_DHCP (1 << 6)
#define IP_DST_CLASS_E (1 << 5)
#define SAMEIP_SRC_DEST (1 << 4)
#define IPSRC_LOOP (1 << 3)
#define TTL0_ERR (1 << 2)
#define IPV4_BAD_HLEN (1 << 1)
#define MCAST_MAC_SRC (1 << 0)

#define DEF_QOSNONIP (3 << 30)
#define DEF_QOSIP (3 << 28)
#define DEF_QOS_LBL (7 << 16)
#define DEF_VLAN_ID (1 << 4)
#define NOVLAN_HW (1 << 0)

#define EMUX_THR 0x180
#define IMUX_TXDMA_RATE 0x00010008

#define DMA_REV_ENDIAN 1
#define DMA_MAX_NONSEG_SIZE 0x1ff
#define CONFIG_OP16 (1 << 0)
#define CONFIG_OP32 (1 << 1)
#define CONFIG_MOPEN (1 << 2)
#define BPAI_PRIO 0x07000000

#define BW_SHAPING (1 << 28)
#define MAX_MBPS (1000 << 16)

#define MAX_RXDMA	2
#define MAX_TXDMA	1

/* This is number of driver accelerated interfaces. e.g. wlan0, wlan1 */
#define MAX_ACCEL_IF	2

#define MAX_FNET_CPU	2

#define FILT_BADF (FP_FILT_BASE + 0x4c)
#define FILT_BADF_DROP (FP_FILT_BASE + 0x50)

#define MAX_FP_BRIDGE NUM_INTFS
#define IDX_INV 0xff

struct rx_ch_reg {
	u32 rx_cpu;
	u32 rx_ip;
	u32 rx_done;
	u32 rx_thresh;
	u32 rx_delay;
	u32 holes[3];
};

struct fp_rxdma_regs {
	u32 rx_irq_flags;
	u32 rx_irq_enables[3];
	u32 rx_test_bus_sel;
	u32 dummies0[11];
	u32 rx_t3w_config;
	u32 rx_t3w_maxmsg;
	u32 rx_t3w_throttle;
	u32 rx_endianness;
	u32 rx_bpai_clear;
	u32 dummies1[11];
	struct rx_ch_reg per_ch[3];
	u32 dummies3[(0x104 - 0xe0) / 4];
	u32 rx_errcntr_no_buff[3];
	u32 dummies2[(0x400 - 0x110) / 4];
	u32 buf[3][256];
};

struct tx_ch_reg {
	u32 tx_cpu;
	u32 tx_ip;
	u32 tx_done;
	u32 tx_delay;
	u32 holes[4];
};

struct tx_buf {
	u32 lo;
	u32 hi;
};

struct fp_txdma_regs {
	u32 tx_irq_flags;
	u32 tx_irq_enables[3];
	u32 tx_test_bus_sel;
	u32 dummies0[11];
	u32 tx_t3w_config;
	u32 tx_t3w_maxmsg;
	u32 tx_t3w_throttle;
	u32 tx_endianness;
	u32 tx_bpai_clear;
	u32 tx_bpai_priority;
	u32 dummies1[10];
	struct tx_ch_reg per_ch[3];
	u32 dummies2[(0x800 - 0xe0) / 4];
	struct tx_buf buf[3][256];
};

struct fp_hdr {
	u32 word0;
	u32 word1;
	u32 word2;
	u32 word3;
};

struct fp_data {
	struct fp_hdr hdr;
};

struct fp_rx_ring {
	struct sk_buff *skb;
	dma_addr_t dma_ptr;
};

struct fp_tx_ring {
	struct sk_buff *skb;
	void *skb_data;
	dma_addr_t dma_ptr;
	int len_eop;
	void *priv;
};

struct fp_txdesc {
	int cpu;
	int idx;
};

struct fpif_rxdma {
	struct fp_rxdma_regs *rxbase;
	struct rx_ch_reg *rx_ch_reg;
	u32 *bufptr;
	u8 head_rx;
	u8 last_rx;
	struct fp_rx_ring fp_rx_skbuff[FPIF_RX_RING_SIZE];
	unsigned long users;
	/* This lock  protect critical region in functions accessing rx ring */
	spinlock_t fpif_rxlock;

};

struct fpif_txdma {
	u8 head_tx;
	u8 last_tx;
	u8 head_tx_sw[MAX_FNET_CPU];
	u8 start_tx_sw[MAX_FNET_CPU];
	u8 last_tx_sw[MAX_FNET_CPU];
	u8 done_tx_sw[MAX_FNET_CPU];
	u8 avail[MAX_FNET_CPU];
	struct fp_txdma_regs *txbase;
	struct tx_ch_reg *tx_ch_reg;
	struct tx_buf *bufptr;
	struct fp_tx_ring fp_tx_skbuff[MAX_FNET_CPU][FPIF_TX_RING_SIZE];
	struct fp_txdesc fp_txb[FPIF_TX_RING_SIZE];
	/* This lock  protect critical region in functions accessing tx ring */
	spinlock_t fpif_txlock;
	/* This lock protect critical region for cpu tx ring */
	spinlock_t txlock[MAX_FNET_CPU];
	unsigned long users;
};

struct fpif_grp {
	void __iomem *base;
	struct fp_rxdma_regs *rxbase;
	struct fpif_rxdma rxdma_info[MAX_RXDMA];
	struct net_device *ndev_cpu[MAX_ACCEL_IF];
	struct net_device *netdev[NUM_INTFS];
	struct device *dev;
	struct fp_txdma_regs *txbase;
	struct fpif_txdma txdma_info[MAX_TXDMA];
	struct fp_tx_ring fp_tx_ring_cpu[MAX_FNET_CPU][FPIF_TX_RING_SIZE];
	u8 available_l2cam;
	u8 l2cam_size;
	u8 l2_idx[FP_L2CAM_SIZE];
	char wlan0[IFNAMSIZ];
	char wlan1[IFNAMSIZ];
	int wlan0_cnt;
	int wlan1_cnt;
	struct plat_stmfp_data *plat;
	short rx_irq0;
	short rx_irq1;
	short tx_irq;
	unsigned long stop_q;
	struct clk *clk_fp;
	struct clk *clk_ife;
	struct reset_control *rstc;
};

struct fpif_priv {
	struct net_device *netdev;
	struct fpif_grp *fpgrp;
	u8 rx_dma_ch;
	struct fpif_rxdma *rxdma_ptr;
	int rx_buffer_size;
	struct sk_buff_head rx_recycle;
	struct napi_struct napi;
	struct device *dev;
	int id;
	u32 dma_port;
	u32 sp; /* Fastpath source port */
	u32 dmap; /* Fastpath Destination Map */
	u8 tx_dma_ch;
	struct fpif_txdma *txdma_ptr;
	struct phy_device *phydev;
	int oldlink;
	int speed;
	int oldduplex;
	unsigned int flow_ctrl;
	unsigned int pause;
	struct mii_bus *mii;
	int mii_irq[PHY_MAX_ADDR];
	/* This lock  protect critical region in fpif_adjust_link */
	spinlock_t fpif_lock;
	struct plat_fpif_data *plat;
	void *rgmii_base;
	u32 ifidx;
	u8 logical_if;
	unsigned long users;
	u32 msg_enable;
	u8 stand_cm_en;
	u8 ifaddr_idx;
	u8 allmulti_idx;
	u8 promisc_idx;
	u8 br_l2cam_idx;
	u8 br_tcam_idx;
};

struct fp_qos_queue {
	int q_size;
	int threshold_0;
	int threshold_1;
	int threshold_2;
	int buf_rsvd;
};

struct fp_promisc_info {
	short ifidx_log;
	u8 mac[ETH_ALEN];
	struct net_device *netdev;
};


extern int fpif_mdio_register(struct net_device *ndev);
extern int fpif_mdio_unregister(struct net_device *ndev);
extern void fpif_set_ethtool_ops(struct net_device *netdev);
extern int fpif_wait_till_done(struct fpif_priv *priv);
extern void stmfp_init_sysfs(struct net_device *netdev);
extern void stmfp_hwinit_badf(struct fpif_grp *fpgrp);
