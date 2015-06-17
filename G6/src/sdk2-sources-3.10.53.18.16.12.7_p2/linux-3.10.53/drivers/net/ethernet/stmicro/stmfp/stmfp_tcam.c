/***************************************************************************
  This is the TCAM org file for the ST fastpath on-chip Ethernet controllers.
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

#include <linux/module.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/vmalloc.h>
#include <linux/pagemap.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/tcp.h>
#include <net/tcp.h>
#include <linux/ipv6.h>
#include <net/checksum.h>
#include <net/ip6_checksum.h>
#include <linux/if_vlan.h>
#include <linux/cpu.h>
#include <linux/smp.h>
#include <linux/skbuff.h>
#include <linux/phy.h>
#include <linux/netdevice.h>
#include "stmfp_tcam.h"

static inline void fpif_write_reg(void __iomem *fp_reg, u32 val)
{
	writel(val, fp_reg);
}


static int is_valid_tcam(void *base, int idx)
{
	int valid;

	fpif_write_reg(base + FP_FC_RST, 0);
	fpif_write_reg(base + FP_FC_CMD, TCAM_RD | idx);
	valid = readl(base + FP_FC_CTRL) & (1 << FC_CTRL_VALID_SHIFT);

	return valid;
}


void remove_tcam(void *base, int idx)
{
	int valid;

	valid = is_valid_tcam(base, idx);
	if (!valid) {
		pr_err("fp:ERROR:TCAM idx %d already removed\n", idx);
		return;
	}
	fpif_write_reg(base + FP_FC_RST, 0);
	fpif_write_reg(base + FP_FC_CMD, TCAM_WR | idx);

	return;
}
EXPORT_SYMBOL(remove_tcam);


void mod_tcam(void *base, struct fp_tcam_info *tcam_info,
		int idx)
{
	unsigned char *dev_addr = tcam_info->dev_addr_d;
	unsigned char *dev_addr_s = tcam_info->dev_addr_s;
	u32 fc_ctrl, val = 0, sp = tcam_info->sp;

	fpif_write_reg(base + FP_FC_RST, 0);
	if (tcam_info->if_recirc) {
		val = FC_SOURCE_RECIRC | FC_SOURCE_RECIRC_MASK;
		fpif_write_reg(base + FP_FC_SOURCE, val);
	}
	if (sp != SP_ANY) {
		val |= FC_SOURCE_SRCP_MASK | sp << FC_SOURCE_SRCP_SHIFT;
		fpif_write_reg(base + FP_FC_SOURCE, val);
	}
	fc_ctrl = readl(base + FP_FC_CTRL) | 1 << FC_CTRL_VALID_SHIFT;
	fc_ctrl = fc_ctrl | (tcam_info->dest << FC_CTRL_DST_SHIFT);
	fc_ctrl = fc_ctrl | (tcam_info->bridge << FC_CTRL_BRIDGE_SHIFT);
	fc_ctrl = fc_ctrl | (tcam_info->redir << FC_CTRL_REDIR_SHIFT);
	fc_ctrl = fc_ctrl | (tcam_info->cont << FC_CTRL_CONTINUE_SHIFT);
	fc_ctrl = fc_ctrl | (tcam_info->drop << FC_CTRL_DROP_SHIFT);
	fc_ctrl = fc_ctrl | (tcam_info->rewrite_qos << FC_CTRL_REQOS_SHIFT);
	fc_ctrl = fc_ctrl | (tcam_info->qos_label << FC_CTRL_QOS_SHIFT);
	fc_ctrl = fc_ctrl | (tcam_info->done << FC_CTRL_DONE_SHIFT);
	if (tcam_info->if_mangled)
		fc_ctrl = fc_ctrl | ((tcam_info->mngl_ptr & 0x1ff)
			<< FC_CTRL_MNGL_SHIFT);
	fpif_write_reg(base + FP_FC_CTRL, fc_ctrl);

	if (sp == SP_DOCSIS) {
		val = FC_IFINDEX_MASK | tcam_info->ifindex;
		fpif_write_reg(base + FP_FC_IF_INDEX, val);
	}

	if (dev_addr) {
		val = dev_addr[0] << 8 | dev_addr[1];
		fpif_write_reg(base + FP_FC_MAC_D_H, val);
		fpif_write_reg(base + FP_FC_MAC_D_MASK_H, 0xffff);
		val = (dev_addr[2] << 24) | (dev_addr[3] << 16) |
		       (dev_addr[4] << 8) | dev_addr[5];
		fpif_write_reg(base + FP_FC_MAC_D_L, val);
		fpif_write_reg(base + FP_FC_MAC_D_MASK_L, 0xffffffff);
	}

	if (tcam_info->all_multi) {
		fpif_write_reg(base + FP_FC_MAC_D_H, 0x100);
		fpif_write_reg(base + FP_FC_MAC_D_MASK_H, 0x100);
	}

	if (dev_addr_s) {
		val = dev_addr_s[0] << 8 | dev_addr_s[1];
		fpif_write_reg(base + FP_FC_MAC_S_H, val);
		fpif_write_reg(base + FP_FC_MAC_S_MASK_H, 0xffff);
		val = (dev_addr_s[2] << 24) | (dev_addr_s[3] << 16) |
		       (dev_addr_s[4] << 8) | dev_addr_s[5];
		fpif_write_reg(base + FP_FC_MAC_S_L, val);
		fpif_write_reg(base + FP_FC_MAC_S_MASK_L, 0xffffffff);
	}

	if (tcam_info->eth_type)
		fpif_write_reg(base + FP_FC_ETH_TYPE, tcam_info->eth_type);
	if (tcam_info->prot)
		fpif_write_reg(base + FP_FC_PROTO, tcam_info->prot);

	fpif_write_reg(base + FP_FC_CMD, TCAM_WR | idx);

	return;
}
EXPORT_SYMBOL(mod_tcam);


void add_tcam(void *base, struct fp_tcam_info *tcam_info,
		     int idx)
{
	int valid;

	if (idx >= NUM_TCAM_ENTRIES)
		pr_err("fp:ERROR:Invalid TCAM idx passed\n");
	valid = is_valid_tcam(base, idx);
	if (!valid)
		mod_tcam(base, tcam_info, idx);
	else
		pr_err("fp:ERROR:idx %d is already used\n", idx);

	return;
}
EXPORT_SYMBOL(add_tcam);


void init_tcam(void *base)
{
	int idx, valid;

	for (idx = 0; idx < NUM_TCAM_ENTRIES; idx++) {
		fpif_write_reg(base + FP_FC_RST, 0);
		fpif_write_reg(base + FP_FC_CMD, TCAM_RD | idx);
		valid = readl(base + FP_FC_CTRL);
		valid = valid & (1 << FC_CTRL_VALID_SHIFT);
		if (valid)
			pr_err("fp:ERROR:TCAM entries already used\n");
		fpif_write_reg(base + FP_FC_CMD, TCAM_WR | idx);
	}

	return;
}
EXPORT_SYMBOL(init_tcam);


void init_tcam_standalonecm(void *base)
{
	struct fp_tcam_info tcam_info;
	int idx, i;

	/* Adding TCAM entry if src is CPU*/
	memset(&tcam_info, 0, sizeof(tcam_info));
	tcam_info.sp = SP_SWDEF;
	tcam_info.drop = 1;
	tcam_info.done = 1;
	idx = TCAM_CM_SRCCPU;
	add_tcam(base, &tcam_info, idx);

	/* Adding TCAM entry for GIGE1->DOCSIS */
	memset(&tcam_info, 0, sizeof(tcam_info));
	tcam_info.sp = SP_ISIS;
	tcam_info.bridge = 1;
	tcam_info.redir = 1;
	tcam_info.dest = DEST_DOCSIS;
	tcam_info.done = 1;
	idx = TCAM_CM_SRCGIGE1;
	add_tcam(base, &tcam_info, idx);

	/* Adding 8 TCAM entries for DOCSIS->GIGE1 */
	for (i = 0; i < 8; i++) {
		memset(&tcam_info, 0, sizeof(tcam_info));
		tcam_info.sp = SP_DOCSIS;
		idx = TCAM_CM_SRCDOCSIS;
		tcam_info.bridge = 1;
		tcam_info.redir = 1;
		tcam_info.done = 1;
		tcam_info.dest = DEST_ISIS;
		tcam_info.rewrite_qos = 1;
		tcam_info.qos_label = i;
		tcam_info.ifindex = i;
		add_tcam(base, &tcam_info, idx + i);
	}

	return;
}
EXPORT_SYMBOL(init_tcam_standalonecm);


void remove_tcam_standalonecm(void *base)
{
	int i;

	remove_tcam(base, TCAM_CM_SRCCPU);
	remove_tcam(base, TCAM_CM_SRCGIGE1);
	for (i = 0; i < 8; i++)
		remove_tcam(base, TCAM_CM_SRCDOCSIS + i);

	return;
}
EXPORT_SYMBOL(remove_tcam_standalonecm);
