/*
 * Copyright (C) 2014 STMicroelectronics Limited
 * Author: David McKay <david.mckay@st.com>
 * Author: Ajit Pal Singh <ajitpal.singh@st.com>
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 */
#ifndef _ST_AMBA_BRIDGE_H
#define _ST_AMBA_BRIDGE_H
#include <linux/device.h>

/* Non-STBus to STBUS Convertor/Bridge config datastructure */
struct st_amba_bridge_config {
	 /* Bridge type: */
	enum {
		st_amba_type1, /* Older version ADCS: 7526996 */
		st_amba_type2,
	} type;

	/* Type of transaction to generate on stbus side */
	enum {
		st_amba_opc_LD4_ST4,
		st_amba_opc_LD8_ST8,
		st_amba_opc_LD16_ST16,
		st_amba_opc_LD32_ST32,
		st_amba_opc_LD64_ST64
	} max_opcode;

	/* These fields must be power of two */
	unsigned int chunks_in_msg;
	unsigned int packets_in_chunk;
	/* Enable STBus posted writes */
	enum {
		st_amba_write_posting_disabled,
		st_amba_write_posting_enabled
	} write_posting;

	/*
	 * We could use a union here, but it makes
	 * the initialistion code look clunky unfortunately
	 */

	/* Fields unique to v1 */
	struct {
		unsigned int req_timeout;
	} type1;
	/* Fields unique to v2 */
	struct {
		/*
		 * Some versions of the type 2 convertor do not
		 * have the sd_config at all, so some of the
		 * fields below cannot be changed or even read!
		 */
		unsigned int sd_config_missing:1;
		unsigned int threshold;/* power of 2 */
		enum {
			st_amba_ahb_burst_based,
			st_amba_ahb_cell_based
		} req_notify;
		enum {
			st_amba_complete_transaction,
			st_amba_abort_transaction
		} cont_on_error;
		enum {
			st_amba_msg_merge_disabled,
			st_amba_msg_merge_enabled
		} msg_merge;
		enum {
			st_amba_stbus_cell_based,
			st_amba_stbus_threshold_based
		} trigger_mode;
		enum {
			st_amba_read_ahead_disabled,
			st_amba_read_ahead_enabled
		} read_ahead;
	} type2;
};

/*
 * Create an amba plug, the calling code needs to map the registers into base.
 * The device associated with this plug must be passed in. This will not access
 * the registers in any way, so can be called before the device has a clock for
 * example. There is no release method, as all memory allocation is done via
 * the devres functions so will automatically be released when the device goes
 */
struct st_amba_bridge *
st_amba_bridge_create(struct device *dev,
		      void __iomem *base,
		      struct st_amba_bridge_config *bus_config);

/*
 * Set up the amba convertor registers, should be called
 * for first time init and after resume
 */
void st_amba_bridge_init(struct st_amba_bridge *plug);

/*
 * Macros to assist in building up the various data structures,
 * there is a lot of commonality between the devices. If there
 * should be as much is an interesting question
 */
#define ST_DEFAULT_TYPE2_AMBA_PLUG_CONFIG				\
	.type			=	st_amba_type2,			\
	.max_opcode		=	st_amba_opc_LD32_ST32,		\
	.write_posting		=	st_amba_write_posting_disabled,\
	.chunks_in_msg		=	0, /* messaging disabled */	\
	.packets_in_chunk	=	0,				\
	.type2.req_notify	=	st_amba_ahb_burst_based,	\
	.type2.cont_on_error	=	st_amba_complete_transaction,	\
	.type2.msg_merge	=	st_amba_msg_merge_disabled,	\
	.type2.trigger_mode	=	st_amba_stbus_threshold_based,	\
	.type2.read_ahead	=	st_amba_read_ahead_enabled,	\
	.type2.threshold	=	32


/*
 * Same for USB, though different SOCs seem to use different threshold
 * values. Again they can be overridden.
 */
#define ST_DEFAULT_USB_AMBA_PLUG_CONFIG(thresh)			\
	ST_DEFAULT_TYPE2_AMBA_PLUG_CONFIG,				\
	.packets_in_chunk			= 8,			\
	.type2.threshold			= (thresh)

/*
 * Older SOCs need different settings, which depend on the LMI width.
 * This is the default for 32 bit wide LMIs, threshold should be reduced
 * to 16 for 16 bit wide LMI.
 */
#define ST_DEFAULT_USB_AMBA_PLUG_CONFIG_OLD				\
	ST_DEFAULT_TYPE2_AMBA_PLUG_CONFIG,				\
	.max_opcode		=	st_amba_opc_LD64_ST64,		\
	.packets_in_chunk	=	2,				\
	.type2.threshold	=	128

/* SATA uses type 1 convertors */
#define ST_DEFAULT_SATA_AMBA_PLUG_CONFIG				\
	.type			=	st_amba_type1,			\
	.max_opcode		=	st_amba_opc_LD32_ST32,		\
	.write_posting		=	st_amba_write_posting_disabled,\
	.chunks_in_msg		=	8,				\
	.packets_in_chunk	=	4,				\
	.type1.req_timeout	=	0

struct st_amba_bridge_config *st_of_get_amba_config(struct device *dev);

#endif /* _ST_AMBA_BRIDGE_H */
