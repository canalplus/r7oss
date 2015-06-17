/*
 *  Multi-Target Trace solution
 *
 *  MTT - API DEFINITION FOR ST INTERNAL SUPPORT CODE.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * Copyright (C) STMicroelectronics, 2011
 */
#ifndef __mtt_api_internal_h
#define __mtt_api_internal_h

#include "../mtt.h"
#include <linux/mtt/systrace.h>


/**
 * The MTT specification says that a channel can be allocated to a
 * software component upon mtt_open, so there is conceptually a mapping
 * between the componentID (that identifies the software emitting the trace)
 * and the Channel on the STM Port.
 * Predefined IDs will store the STM channel & master in the LSBs.
 * this provides a quick way to associate a ComponentID to a STM channel.
 *
 * There is a 4 bits field in component-ID
 *  used to encode the coreID on which the trace component is running
 *  Componend ID field is as follows:
 *
 *  33222222222211111111110000000000
 *  10987654321098765432109876543210
 *  ................MMMM............
 *
 * MTT_COMPID_MAOFFSET specify the location of this field
 * It is used as follows: "MTT_COMPID_MAOFFSET * core"
 * when calling "mtt_component_alloc" in component.c and kptrace.c
 *
 * MTT_COMPID_MAMSK allows to isolate the core field in component-ID
 */
#define MTT_COMPID_MASHIFT  12                      /* Start at bit 12 */
#define MTT_COMPID_MAOFFSET (1<<MTT_COMPID_MASHIFT) /* Start at bit 12 */
#define MTT_COMPID_MAMSK    0x0000F000              /* Bits 12 to 15 */



/**
 * The MTT specification says that a channel can be allocated to a
 * software component upon mtt_open, so there is conceptually a mapping
 * between the componentID (that identifies the software emitting the trace)
 * and the Channel on the STM Port.
 * Predefined IDs will store the STM channel & master in the LSBs.
 * this provides a quick way to associate a ComponentID to a STM channel.
 *
 * There is a 8 bits field in component-ID
 *   used to encode the STM channel directly
 *   in the component ID in the case of predefined COMP ids
 *   and channel semantics.
 *
 *  33222222222211111111110000000000
 *  10987654321098765432109876543210
 *  ........................CCCCCCCC
 *
 * It is used as follows: "MTT_COMPID_LIN_IRQ + MTT_CH_MASTER_OFFSET * core"
 * when calling "mtt_component_alloc" in component.c and kptrace.c
 *
 * MTT_COMPID_CHMSK allows to isolate the core field in component-ID
 */
#define MTT_COMPID_CHSHIFT  0          /* Start at bit 0 */
#define MTT_COMPID_CHOFFSET 0          /* Start at bit 0 */
#define MTT_COMPID_CHMSK    0x000000FF /* Bits 0 to 7 */


/* COMMON CHANNEL DEFINITIONS */
/* Channel definitions are moved to systrace.h */

/*  == ST internal component definitions ==
 **/
#define MTT_COMPID_ST	 0x80000000 /* Indicates an ST-predefined ID */

/* OS21 PREDEFINED COMPONENTS */
#define MTT_COMPID_OS21		(MTT_COMPID_ST|0x00010000)

/* XP70 PREDEFINED COMPONENTS */
#define MTT_COMPID_XP70		(MTT_COMPID_ST|0x00020000)

/* DECODER PREDEFINED COMPONENTS
 *
 * (used by the decoder to send _legacy_ data to the gui).
 * The 2 subcomponents bytes may contain the master/channel ids
 * if needed, else 0.
 */
#define MTT_COMPID_DEC    (MTT_COMPID_ST|0x00030000)

/* STLINUX PREDEFINED COMPONENTS
 */
#define MTT_COMPID_LIN		(MTT_COMPID_ST|0x00040000)
#define MTT_COMPID_LIN_DATA	(MTT_COMPID_LIN|MTT_CH_LIN_DATA)
#define MTT_COMPID_LIN_CTXT	(MTT_COMPID_LIN|MTT_CH_LIN_CTXT)
#define MTT_COMPID_LIN_KMUX	(MTT_COMPID_LIN|MTT_CH_LIN_KMUX)
#define MTT_COMPID_LIN_KMETA	(MTT_COMPID_LIN|MTT_CH_LIN_KMETA)
#define MTT_COMPID_LIN_IRQ	(MTT_COMPID_LIN|MTT_CH_IRQ)
#define MTT_COMPID_LIN_META	(MTT_COMPID_LIN|MTT_CH_META)
#define MTT_COMPID_LIN_RAW	(MTT_COMPID_LIN|MTT_CH_RAW)
#define MTT_COMPID_LIN_AMUX	(MTT_COMPID_LIN|MTT_CH_MUX)

#define MTT_COMPID_FREERTOS	(MTT_COMPID_ST|0x00050000)

#define MTT_COMPID_MCOM		(MTT_COMPID_ST|0x00060000)

/* Predefined component for printk's and syslogd. */
#define MTT_COMPID_SLOG		(MTT_COMPID_ST|0x00070000|MTT_CH_SLOG)

/* STAPI PREDEFINED COMPONENTS */
#define MTT_COMPID_STAPI	(MTT_COMPID_ST|0x00080000)

/* STKPI PREDEFINED COMPONENTS */
#define MTT_COMPID_STKPI	(MTT_COMPID_ST|0x00100000)

#endif /* __mtt_api_internal_h */
