/*
 * STMicroelectronics BDispII driver - nodes
 *
 * Copyright (c) 2012 STMicroelectronics Limited
 *
 * Author: André Draszik <andre.draszik@st.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifdef __KERNEL__
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/io.h>
#else /* __KERNEL__ */
#include <stdio.h>
#include <direct/types.h>
#define readl(__addr) (*(volatile uint32_t *) (__addr))
#define __iomem
#endif /* __KERNEL__ */

#include <linux/stm/bdisp2_nodegroups.h>

#include "blitter_os.h"

#include "bdisp2/bdispII_nodes.h"

#define STR_SIZE 1500

char *
bdisp2_sprint_node(const struct _BltNodeGroup00 __iomem * const config_general,
		   bool register_dump)
{
	const struct _BltNodeGroup01 * const config_target =
		(struct _BltNodeGroup01 *) (config_general + 1);
	const void *pos = config_target + 1;
	char *str;
	int sz = 0;

	str = stm_blitter_allocate_mem(STR_SIZE);
	if (!str)
		return NULL;

	sz += snprintf(&str[sz], STR_SIZE - sz,
		       "BLT_NIP  BLT_CIC  BLT_INS  BLT_ACK : %.8x %.8x %.8x %.8x\n",
		       readl(&config_general->BLT_NIP),
		       readl(&config_general->BLT_CIC),
		       readl(&config_general->BLT_INS),
		       readl(&config_general->BLT_ACK));
	sz += snprintf(&str[sz], STR_SIZE - sz,
		       "BLT_TBA  BLT_TTY  BLT_TXY  BLT_TSZ : %.8x %.8x %.8x %.8x\n",
		       readl(&config_target->BLT_TBA),
		       readl(&config_target->BLT_TTY),
		       readl(&config_target->BLT_TXY),
		       readl(&config_target->BLT_TSZ_S1SZ));

	if (register_dump
	    || config_general->BLT_CIC & BLIT_CIC_NODE_COLOR) {
		const struct _BltNodeGroup02 * const config_color = pos;
		pos = config_color + 1;

		sz += snprintf(&str[sz], STR_SIZE - sz,
			       "BLT_S1CF BLT_S2CF                  : %.8x %.8x\n",
			       readl(&config_color->BLT_S1CF),
			       readl(&config_color->BLT_S2CF));
	}
	if (register_dump
	    || config_general->BLT_CIC & BLIT_CIC_NODE_SOURCE1) {
		const struct _BltNodeGroup03 * const config_source1 = pos;
		pos = config_source1 + 1;

		sz += snprintf(&str[sz], STR_SIZE - sz,
			       "BLT_S1BA BLT_S1TY BLT_S1XY         : %.8x %.8x %.8x\n",
			       readl(&config_source1->BLT_S1BA),
			       readl(&config_source1->BLT_S1TY),
			       readl(&config_source1->BLT_S1XY));
	}
	if (register_dump
	    || config_general->BLT_CIC & BLIT_CIC_NODE_SOURCE2) {
		const struct _BltNodeGroup04 * const config_source2 = pos;
		pos = config_source2 + 1;

		sz += snprintf(&str[sz], STR_SIZE - sz,
			       "BLT_S2BA BLT_S2TY BLT_S2XY BLT_S2SZ: %.8x %.8x %.8x %.8x\n",
			       readl(&config_source2->BLT_S2BA),
			       readl(&config_source2->BLT_S2TY),
			       readl(&config_source2->BLT_S2XY),
			       readl(&config_source2->BLT_S2SZ));
	}
	if (register_dump
	    || config_general->BLT_CIC & BLIT_CIC_NODE_SOURCE3) {
		const struct _BltNodeGroup05 * const config_source3 = pos;
		pos = config_source3 + 1;

		sz += snprintf(&str[sz], STR_SIZE - sz,
			       "BLT_S3BA BLT_S3TY BLT_S3XY BLT_S3SZ: %.8x %.8x %.8x %.8x\n",
			       readl(&config_source3->BLT_S3BA),
			       readl(&config_source3->BLT_S3TY),
			       readl(&config_source3->BLT_S3XY),
			       readl(&config_source3->BLT_S3SZ));
	}
	if (register_dump
	    || config_general->BLT_CIC & BLIT_CIC_NODE_CLIP) {
		const struct _BltNodeGroup06 * const config_clip = pos;
		pos = config_clip + 1;

		sz += snprintf(&str[sz], STR_SIZE - sz,
			       "BLT_CWO  BLT_CWS                   : %.8x %.8x\n",
			       readl(&config_clip->BLT_CWO),
			       readl(&config_clip->BLT_CWS));
	}
	if (register_dump
	    || config_general->BLT_CIC & BLIT_CIC_NODE_CLUT) {
		const struct _BltNodeGroup07 * const config_clut = pos;
		pos = config_clut + 1;

		sz += snprintf(&str[sz], STR_SIZE - sz,
			       "BLT_CCO  BLT_CML                   : %.8x %.8x\n",
			       readl(&config_clut->BLT_CCO),
			       readl(&config_clut->BLT_CML));
	}
	if (register_dump
	    || config_general->BLT_CIC & BLIT_CIC_NODE_FILTERS) {
		const struct _BltNodeGroup08 * const config_filters = pos;
		pos = config_filters + 1;

		sz += snprintf(&str[sz], STR_SIZE - sz,
			       "BLT_FCTL BLT_PMK                   : %.8x %.8x\n",
			       readl(&config_filters->BLT_FCTL_RZC),
			       readl(&config_filters->BLT_PMK));
	}
	if (register_dump
	    || config_general->BLT_CIC & BLIT_CIC_NODE_2DFILTERSCHR) {
		const struct _BltNodeGroup09 * const config_2dfilterschr = pos;
		pos = config_2dfilterschr + 1;

		sz += snprintf(&str[sz], STR_SIZE - sz,
			       "BLT_RSF  BLT_RZI  BLT_HFP  BLT_VFP : %.8x %.8x %.8x %.8x\n",
			       readl(&config_2dfilterschr->BLT_RSF),
			       readl(&config_2dfilterschr->BLT_RZI),
			       readl(&config_2dfilterschr->BLT_HFP),
			       readl(&config_2dfilterschr->BLT_VFP));
	}
	if (register_dump
	    || config_general->BLT_CIC & BLIT_CIC_NODE_2DFILTERSLUMA) {
		const struct _BltNodeGroup10 * const config_2dfiltersluma = pos;
		pos = config_2dfiltersluma + 1;

		sz += snprintf(&str[sz], STR_SIZE - sz,
			       "BLT_YRSF BLT_YRZI BLT_YHFP BLT_YVFP: %.8x %.8x %.8x %.8x\n",
			       readl(&config_2dfiltersluma->BLT_Y_RSF),
			       readl(&config_2dfiltersluma->BLT_Y_RZI),
			       readl(&config_2dfiltersluma->BLT_Y_HFP),
			       readl(&config_2dfiltersluma->BLT_Y_VFP));
	}
	if (register_dump
	    || config_general->BLT_CIC & BLIT_CIC_NODE_FLICKER) {
		const struct _BltNodeGroup11 * const config_flicker = pos;
		pos = config_flicker + 1;

		sz += snprintf(&str[sz], STR_SIZE - sz,
			       "BLT_FF0  BLT_FF1  BLT_FF2  BLT_FF3 : %.8x %.8x %.8x %.8x\n",
			       readl(&config_flicker->BLT_FF0),
			       readl(&config_flicker->BLT_FF1),
			       readl(&config_flicker->BLT_FF2),
			       readl(&config_flicker->BLT_FF3));
	}
	if (register_dump
	    || config_general->BLT_CIC & BLIT_CIC_NODE_COLORKEY) {
		const struct _BltNodeGroup12 * const config_ckey = pos;
		pos = config_ckey + 1;

		sz += snprintf(&str[sz], STR_SIZE - sz,
			       "BLT_KEY1 BLT_KEY2                  : %.8x %.8x\n",
			       readl(&config_ckey->BLT_KEY1),
			       readl(&config_ckey->BLT_KEY2));
	}
	if (register_dump) {
		const struct _BltNodeGroup13 * const config_xyl = pos;
		pos = config_xyl + 1;
	}
	if (register_dump
	    || config_general->BLT_CIC & BLIT_CIC_NODE_STATIC) {
		const struct _BltNodeGroup14 * const config_static = pos;
		pos = config_static + 1;

		sz += snprintf(&str[sz], STR_SIZE - sz,
			       "BLT_SAR  BLT_USR                   : %.8x %.8x\n",
			       readl(&config_static->BLT_SAR),
			       readl(&config_static->BLT_USR));
	}
	if (register_dump)
		pos += 8;
	if (register_dump
	    || config_general->BLT_CIC & BLIT_CIC_NODE_IVMX) {
		const struct _BltNodeGroup15 * const config_ivmx = pos;
		pos = config_ivmx + 1;

		sz += snprintf(&str[sz], STR_SIZE - sz,
			       "BLT_IVMX BLT_IVMX BLT_IVMX BLT_IVMX: %.8x %.8x %.8x %.8x\n",
			       readl(&config_ivmx->BLT_IVMX0),
			       readl(&config_ivmx->BLT_IVMX1),
			       readl(&config_ivmx->BLT_IVMX2),
			       readl(&config_ivmx->BLT_IVMX3));
	}
	if (register_dump
	    || config_general->BLT_CIC & BLIT_CIC_NODE_OVMX) {
		const struct _BltNodeGroup16 * const config_ovmx = pos;
		pos = config_ovmx + 1;

		sz += snprintf(&str[sz], STR_SIZE - sz,
			       "BLT_OVMX BLT_OVMX BLT_OVMX BLT_OVMX: %.8x %.8x %.8x %.8x\n",
			       readl(&config_ovmx->BLT_OVMX0),
			       readl(&config_ovmx->BLT_OVMX1),
			       readl(&config_ovmx->BLT_OVMX2),
			       readl(&config_ovmx->BLT_OVMX3));
	}
	if (register_dump) {
		const struct _BltNodeGroup17 * const config_pace = pos;
		pos = config_pace + 1;
	}
	if (register_dump
	    || config_general->BLT_CIC & BLIT_CIC_NODE_VC1R) {
		const struct _BltNodeGroup18 * const config_vc1 = pos;
		pos = config_vc1 + 1;

		sz += snprintf(&str[sz], STR_SIZE - sz,
			       "BLT_VC1R                           : %.8x\n",
			       readl(&config_vc1->BLT_VC1R));
	}
	if (register_dump
	    || config_general->BLT_CIC & BLIT_CIC_NODE_GRADIENT_FILL) {
		const struct _BltNodeGroup19 * const config_grad = pos;
		pos = config_grad + 1;

		sz += snprintf(&str[sz], STR_SIZE - sz,
			       "BLT_HGF  BLT_VGF                   : %.8x %.8x\n",
			       readl(&config_grad->BLT_HGF),
			       readl(&config_grad->BLT_VGF));
	}

	return str;
}
