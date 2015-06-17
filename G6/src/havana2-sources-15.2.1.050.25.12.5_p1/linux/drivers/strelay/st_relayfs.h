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
#ifndef _ST_RELAYFS_H_
#define _ST_RELAYFS_H_

#ifdef __cplusplus
extern "C" {
#endif

// Client offsets
enum {
	// streaming engine
	ST_RELAY_OFFSET_SE_START = 0x0001,
	ST_RELAY_OFFSET_SE_END   = 0x1000,

	// display engine
	ST_RELAY_OFFSET_DE_START,  // 0x1001
	ST_RELAY_OFFSET_DE_END = 0x2000,

	// transport engine
	ST_RELAY_OFFSET_TE_START,  // 0x2001
	ST_RELAY_OFFSET_TE_END = 0x3000,
};

// st relay dumping is done through {type-id, source-id, *buffer, len}
// source-id of data dumped: useful to discriminate
// cases where some type-id can be dumped from different sources
// appears in header of data dumped
// type-id of data dumped: is exposed in st_relay/ debugfs dir

// track counts of source per type
struct st_relay_source_type_count_s {
	int source;
	int type;
	unsigned int count;
};

// index management
#define TAKE_FIRST_AVAILABLE_INDEX(index, table_indexes) { \
	unsigned int n; \
	for(n = 0; n < ARRAY_SIZE(table_indexes); n++) { \
		if(table_indexes[n] == 0) { \
			table_indexes[n] = 1; \
			index = n; \
			break; \
		} \
	} \
}
#define FREE_INDEX(index, table_indexes) { \
	if (index < ARRAY_SIZE(table_indexes)) \
		table_indexes[index] = 0; \
}

// depends on RELAY and DEBUG_FS
#if defined(CONFIG_RELAY) && defined(CONFIG_DEBUG_FS)
int st_relayfs_open(void);
void st_relayfs_write(unsigned int type, unsigned int source, unsigned char *buf, unsigned int len, void *info);
unsigned int st_relayfs_getindex(unsigned int source);
void st_relayfs_freeindex(unsigned int source, unsigned int index);
#else
static inline int st_relayfs_open(void) { return 0; }
static inline void st_relayfs_write(unsigned int id, unsigned int source,
                                    unsigned char *buf, unsigned int len,
                                    void *info) {}
static inline unsigned int st_relayfs_getindex(unsigned int source) { return 0; }
static inline void st_relayfs_freeindex(unsigned int source, unsigned int index) {}
#endif  // defined(CONFIG_RELAY) && defined(CONFIG_DEBUG_FS)

#ifdef __cplusplus
}; /* extern "C" */
#endif

#endif /* _ST_RELAYFS_H_ */

