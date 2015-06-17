/*
 * STMicroelectronics FDMA dmaengine driver
 *
 * Copyright (c) 2012 STMicroelectronics Limited
 *
 * Author: John Boddie <john.boddie@st.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __ST_FDMA_LLU_H__
#define __ST_FDMA_LLU_H__


/*
 * Free running / paced specific linked list structure
 */

struct st_fdma_llu_generic {
	u32 length;
	u32 sstride;
	u32 dstride;
};

/*
 * TELSS specific linked list structure
 */
#define ST_FDMA_LLU_TELSS_HANDSETS	10
struct st_fdma_llu_telss {
	u32 node_param;
	u32 handset_param[ST_FDMA_LLU_TELSS_HANDSETS];
};

/*
 * MCHI specific linked list structure for Rx only.
 * MCHI Tx uses the paced linked list structure.
 */
struct st_fdma_llu_mchi {
	u32 length;
	u32 rx_fifo_threshold_add;
	u32 dstride;
};

/*
 * FDMA linked list structure
 */
struct st_fdma_llu {
	u32 next;	/* Used by all channels */
	u32 control;	/* Used by all channels */
	u32 nbytes;	/* Used by all channels */
	u32 saddr;	/* Used by all channels */
	u32 daddr;	/* Not used by PES */
	union {
		struct st_fdma_llu_generic generic;
		struct st_fdma_llu_telss telss;
		struct st_fdma_llu_mchi mchi;
	};
};


/*
 * Defines for maximum sized LLU structure and alignment
 */

#define ST_FDMA_LLU_SIZE		sizeof(struct st_fdma_llu)
#define ST_FDMA_LLU_ALIGN		64


/*
 * Defines for generic node control
 */

#define NODE_CONTROL_REQ_MAP_MASK	0x0000001f
#define NODE_CONTROL_REQ_MAP_FREE_RUN	0x00000000
#define NODE_CONTROL_REQ_MAP_EXT	0x0000001f
#define NODE_CONTROL_SRC_MASK		0x00000060
#define NODE_CONTROL_SRC_STATIC		0x00000020
#define NODE_CONTROL_SRC_INCR		0x00000040
#define NODE_CONTROL_DST_MASK		0x00000180
#define NODE_CONTROL_DST_STATIC		0x00000080
#define NODE_CONTROL_DST_INCR		0x00000100
#define NODE_CONTROL_SECURE		0x00008000
#define NODE_CONTROL_COMP_PAUSE		0x40000000
#define NODE_CONTROL_COMP_IRQ		0x80000000


#endif /* __ST_FDMA_LLU_H__ */
