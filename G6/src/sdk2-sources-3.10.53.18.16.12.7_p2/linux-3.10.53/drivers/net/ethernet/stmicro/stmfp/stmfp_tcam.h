/***************************************************************************
  This is the TCAM org hdr for the ST fastpath on-chip Ethernet controllers.
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

/*filter tcam register masks & shifts */
#define FC_SOURCE_SRCP_SHIFT 4
#define FC_SOURCE_SRCP_MASK 0xc0
#define FC_SOURCE_RECIRC 1
#define FC_SOURCE_RECIRC_MASK 0x2
#define FC_IFINDEX_MASK (0x7 << 16)
#define FC_CTRL_DONE_SHIFT 30
#define FC_CTRL_MNGL_SHIFT 21
#define FC_CTRL_DST_SHIFT 13
#define FC_CTRL_REDIR_SHIFT 12
#define FC_CTRL_BRIDGE_SHIFT 11
#define FC_CTRL_CONTINUE_SHIFT 1
#define FC_CTRL_VALID_SHIFT 0
#define FC_CTRL_QOS_SHIFT 3
#define FC_CTRL_REQOS_SHIFT 2
#define FC_CTRL_DROP_SHIFT 6

#define FP_FILT_BASE 0x15000
#define FP_FCAM_BASE (FP_FILT_BASE + 0x800)
#define FP_FC_RST (FP_FCAM_BASE + 0)
#define FP_FC_CTRL (FP_FCAM_BASE + 0x4)
#define FP_FC_SOURCE (FP_FCAM_BASE + 0x8)
#define FP_FC_IF_INDEX (FP_FCAM_BASE + 0xc)
#define FP_FC_MAC_D_H (FP_FCAM_BASE + 0x10)
#define FP_FC_MAC_D_L (FP_FCAM_BASE + 0x14)
#define FP_FC_MAC_D_MASK_H (FP_FCAM_BASE + 0x18)
#define FP_FC_MAC_D_MASK_L (FP_FCAM_BASE + 0x1c)
#define FP_FC_MAC_S_H (FP_FCAM_BASE + 0x20)
#define FP_FC_MAC_S_L (FP_FCAM_BASE + 0x24)
#define FP_FC_MAC_S_MASK_H (FP_FCAM_BASE + 0x28)
#define FP_FC_MAC_S_MASK_L (FP_FCAM_BASE + 0x2c)
#define FP_FC_ETH_TYPE (FP_FCAM_BASE + 0x2c)
#define FP_FC_PROTO (FP_FCAM_BASE + 0x48)

#define FP_FC_CMD (FP_FCAM_BASE + 0xa0)

#define TCAM_IDX_INV 0xff
#define TCAM_FWRULES_IDX 0
#define TCAM_CM_SRCCPU 0
#define TCAM_CM_SRCGIGE1 1
#define TCAM_CM_SRCDOCSIS 2
#define TCAM_CM_SRCDOCSIS_DROP 15
#define TCAM_SYSTEM_IDX 16
#define TCAM_MNGL_SQ_IDX 19
#define TCAM_MNGL_VLAN_IDX 20
#define TCAM_PROMS_FP_IDX 23
#define TCAM_BRIDGE_IDX 29
#define TCAM_PROMS_SP_IDX 61
#define TCAM_ALLMULTI_IDX 61
#define NUM_TCAM_ENTRIES 64
#define TCAM_RD 0x80000000
#define TCAM_WR 0

struct fp_tcam_info {
	u8 sp;
	u8 dest;
	u8 redir;
	u8 bridge;
	u8 cont;
	u8 all_multi;
	u8 drop;
	u8 done;
	u8 ifindex;
	u8 rewrite_qos;
	u8 qos_label;
	u8 if_recirc;
	u8 if_mangled;
	u16 mngl_ptr;
	u16 eth_type;
	u16 prot;
	unsigned char *dev_addr_d;
	unsigned char *dev_addr_s;
};

extern void init_tcam(void *base);
extern void init_tcam_standalonecm(void *base);
extern void remove_tcam_standalonecm(void *base);
extern void remove_tcam(void *base, int idx);
extern void add_tcam(void *base, struct fp_tcam_info *tcam_info, int idx);
extern void mod_tcam(void *base, struct fp_tcam_info *tcam_info, int idx);
extern void remove_tcam_standalonecm(void *base);
