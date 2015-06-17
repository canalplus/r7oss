/************************************************************************
Copyright (C) 2003-2014 STMicroelectronics. All Rights Reserved.

This file is part of the Streaming Engine.

Streaming Engine is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

Streaming Engine is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with Streaming Engine; see the file COPYING.  If not, write to the Free
Software Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307,
USA.

The Streaming Engine Library may alternatively be licensed under a
proprietary license from ST.
************************************************************************/
#ifndef _ST_RELAYFS_TE_H_
#define _ST_RELAYFS_TE_H_

#include "st_relayfs.h"

#ifdef __cplusplus
extern "C" {
#endif

//////// transport sources & types
// TODO - FIXME TE has 'specific' (meaning not in lign with original design)
// way of using sources & types : all duplicated
// which is not the case for other strelayfs users (like SE or COREDISPLAY_TEST)
// =>either reduce sources or types or 'merge' the ST_RELAY_SOURCE and ST_RELAY_TYPE enums
enum {
	ST_RELAY_SOURCE_DEMUX_INJECTOR = ST_RELAY_OFFSET_TE_START,
	ST_RELAY_SOURCE_DEMUX_INJECTOR1,
	ST_RELAY_SOURCE_DEMUX_INJECTOR2,
	ST_RELAY_SOURCE_DEMUX_INJECTOR3,
	ST_RELAY_SOURCE_DEMUX_INJECTOR4,
	ST_RELAY_SOURCE_DEMUX_INJECTOR5,
	ST_RELAY_SOURCE_DEMUX_INJECTOR6,
	ST_RELAY_SOURCE_DEMUX_INJECTOR7,

	ST_RELAY_SOURCE_TSMUX_PULL,
	ST_RELAY_SOURCE_TSMUX_PULL1,
	ST_RELAY_SOURCE_TSMUX_PULL2,
	ST_RELAY_SOURCE_TSMUX_PULL3,

	ST_RELAY_SOURCE_TSMUX_TSG_INJECTOR,
	ST_RELAY_SOURCE_TSMUX_TSG_INJECTOR1,
	ST_RELAY_SOURCE_TSMUX_TSG_INJECTOR2,
	ST_RELAY_SOURCE_TSMUX_TSG_INJECTOR3,
	ST_RELAY_SOURCE_TSMUX_TSG_INJECTOR4,
	ST_RELAY_SOURCE_TSMUX_TSG_INJECTOR5,
	ST_RELAY_SOURCE_TSMUX_TSG_INJECTOR6,
	ST_RELAY_SOURCE_TSMUX_TSG_INJECTOR7,
	ST_RELAY_SOURCE_TSMUX_TSG_INJECTOR8,
	ST_RELAY_SOURCE_TSMUX_TSG_INJECTOR9,
	ST_RELAY_SOURCE_TSMUX_TSG_INJECTOR10,
	ST_RELAY_SOURCE_TSMUX_TSG_INJECTOR11,
	ST_RELAY_SOURCE_TSMUX_TSG_INJECTOR12,
	ST_RELAY_SOURCE_TSMUX_TSG_INJECTOR13,
	ST_RELAY_SOURCE_TSMUX_TSG_INJECTOR14,
	ST_RELAY_SOURCE_TSMUX_TSG_INJECTOR15,

	ST_RELAY_SOURCE_TSMUX_TSG_SEC_INJECTOR,
	ST_RELAY_SOURCE_TSMUX_TSG_SEC_INJECTOR1,
	ST_RELAY_SOURCE_TSMUX_TSG_SEC_INJECTOR2,
	ST_RELAY_SOURCE_TSMUX_TSG_SEC_INJECTOR3,

	ST_RELAY_SOURCE_TE_LAST // keep last
};

enum {
	ST_RELAY_TYPE_DEMUX_INJECTOR = ST_RELAY_OFFSET_TE_START,
	ST_RELAY_TYPE_DEMUX_INJECTOR1,
	ST_RELAY_TYPE_DEMUX_INJECTOR2,
	ST_RELAY_TYPE_DEMUX_INJECTOR3,
	ST_RELAY_TYPE_DEMUX_INJECTOR4,
	ST_RELAY_TYPE_DEMUX_INJECTOR5,
	ST_RELAY_TYPE_DEMUX_INJECTOR6,
	ST_RELAY_TYPE_DEMUX_INJECTOR7,

	ST_RELAY_TYPE_TSMUX_PULL,
	ST_RELAY_TYPE_TSMUX_PULL1,
	ST_RELAY_TYPE_TSMUX_PULL2,
	ST_RELAY_TYPE_TSMUX_PULL3,

	ST_RELAY_TYPE_TSMUX_TSG_INJECTOR,
	ST_RELAY_TYPE_TSMUX_TSG_INJECTOR1,
	ST_RELAY_TYPE_TSMUX_TSG_INJECTOR2,
	ST_RELAY_TYPE_TSMUX_TSG_INJECTOR3,
	ST_RELAY_TYPE_TSMUX_TSG_INJECTOR4,
	ST_RELAY_TYPE_TSMUX_TSG_INJECTOR5,
	ST_RELAY_TYPE_TSMUX_TSG_INJECTOR6,
	ST_RELAY_TYPE_TSMUX_TSG_INJECTOR7,
	ST_RELAY_TYPE_TSMUX_TSG_INJECTOR8,
	ST_RELAY_TYPE_TSMUX_TSG_INJECTOR9,
	ST_RELAY_TYPE_TSMUX_TSG_INJECTOR10,
	ST_RELAY_TYPE_TSMUX_TSG_INJECTOR11,
	ST_RELAY_TYPE_TSMUX_TSG_INJECTOR12,
	ST_RELAY_TYPE_TSMUX_TSG_INJECTOR13,
	ST_RELAY_TYPE_TSMUX_TSG_INJECTOR14,
	ST_RELAY_TYPE_TSMUX_TSG_INJECTOR15,

	ST_RELAY_TYPE_TSMUX_TSG_SEC_INJECTOR,
	ST_RELAY_TYPE_TSMUX_TSG_SEC_INJECTOR1,
	ST_RELAY_TYPE_TSMUX_TSG_SEC_INJECTOR2,
	ST_RELAY_TYPE_TSMUX_TSG_SEC_INJECTOR3,

	ST_RELAY_TYPE_TE_LAST // keep last
};

// nb entries source-type for transport
// current computation assumes each source'N' is used with only type'N' (no crossing)
#define NB_ENTRIES_SOURCE_TYPE_TE (max(ST_RELAY_SOURCE_TE_LAST, ST_RELAY_TYPE_TE_LAST) - ST_RELAY_OFFSET_TE_START)

// depends on RELAY and DEBUG_FS
#if defined(CONFIG_RELAY) && defined(CONFIG_DEBUG_FS)
bool is_client_te(int value);
int st_relayfs_open_te(void);
void st_relayfs_close_te(void);
//
void st_relayfs_write_te(unsigned int type, unsigned int source, unsigned char *buf, unsigned int len, void *info);
//
unsigned int st_relayfs_getindex_forsource_te(unsigned int source);
void st_relayfs_freeindex_forsource_te(unsigned int source, unsigned int index);
#else
static inline bool is_client_te(int value) { return false; }
static inline int st_relayfs_open_te(void) { return 0; }
static inline void st_relayfs_close_te(void) {}
//
static inline void st_relayfs_write_te(unsigned int id, unsigned int source,
                                       unsigned char *buf, unsigned int len,
                                       void *info) {}
//
static inline unsigned int st_relayfs_getindex_forsource_te(unsigned int source) { return 0; }
static inline void st_relayfs_freeindex_forsource_te(unsigned int source, unsigned int index) {}
#endif  // defined(CONFIG_RELAY) && defined(CONFIG_DEBUG_FS)

#ifdef __cplusplus
}; /* extern "C" */
#endif

#endif /* _ST_RELAYFS_TE_H_ */

