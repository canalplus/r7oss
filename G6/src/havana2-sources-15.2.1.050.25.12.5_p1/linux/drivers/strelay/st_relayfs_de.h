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
#ifndef _ST_RELAYFS_DE_H_
#define _ST_RELAYFS_DE_H_

#include "st_relayfs.h"

#ifdef __cplusplus
extern "C" {
#endif

//////// display sources & types
enum {
	ST_RELAY_SOURCE_COREDISPLAY_TEST = ST_RELAY_OFFSET_DE_START,
	ST_RELAY_SOURCE_COREDISPLAY_TEST2,
	ST_RELAY_SOURCE_COREDISPLAY_TEST3,
	ST_RELAY_SOURCE_COREDISPLAY_TEST4,
};
enum {
	ST_RELAY_TYPE_COREDISPLAY_CRC_MISR = ST_RELAY_OFFSET_DE_START,
	ST_RELAY_TYPE_COREDISPLAY_TEST_CAPABILITY,
};

// nb entries source-type for display
#define NB_ENTRIES_SOURCE_TYPE_DE  (4*2)

// depends on RELAY and DEBUG_FS
#if defined(CONFIG_RELAY) && defined(CONFIG_DEBUG_FS)
bool is_client_de(int value);
int st_relayfs_open_de(void);
void st_relayfs_close_de(void);
//
void st_relayfs_write_de(unsigned int type, unsigned int source, unsigned char *buf, unsigned int len, void *info);
//
unsigned int st_relayfs_getindex_forsource_de(unsigned int source);
void st_relayfs_freeindex_forsource_de(unsigned int source, unsigned int index);
#else
static inline bool is_client_de(int value) { return false; }
static inline int st_relayfs_open_de(void) { return 0; }
static inline void st_relayfs_close_de(void) {}
//
static inline void st_relayfs_write_de(unsigned int id, unsigned int source,
                                       unsigned char *buf, unsigned int len,
                                       void *info) {}
//
static inline unsigned int st_relayfs_getindex_forsource_de(unsigned int source) { return 0; }
static inline void st_relayfs_freeindex_forsource_de(unsigned int source, unsigned int index) {}
#endif  // defined(CONFIG_RELAY) && defined(CONFIG_DEBUG_FS)

#ifdef __cplusplus
}; /* extern "C" */
#endif

#endif /* _ST_RELAYFS_DE_H_ */

