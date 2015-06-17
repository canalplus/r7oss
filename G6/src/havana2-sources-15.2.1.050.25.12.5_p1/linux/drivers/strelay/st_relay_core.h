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
#ifndef _ST_RELAY_CORE_H_
#define _ST_RELAY_CORE_H_

#ifdef __cplusplus
extern "C" {
#endif

//st relay data magic identity used for synchro check by user apps
#define ST_RELAY_MAGIC_IDENT 0x12345678

//name length so that sizeof(struct st_relay_entry_header_s) is 64
//(header length expected by user tool)
#define ST_RELAY_TYPE_NAME_LEN 36
// st relay buffer header
struct st_relay_entry_header_s {
	unsigned char name[ST_RELAY_TYPE_NAME_LEN];

	// slots for any metadata
	unsigned int x;
	unsigned int y;
	unsigned int z;

	// synchro
	unsigned int ident;  // set to ST_RELAY_MAGIC_IDENT
	unsigned int source;
	unsigned int count;
	unsigned int len;
};

// type-name match
struct st_relay_typename_s {
	int type;
	const char *name;
};

// st relay entry internals (per type)
struct st_relay_entry_s {
	const char    *name;    // debugfs entry name
	unsigned int   active;  // tunable activation controlled via debugfs
	struct dentry *dentry;  // debugfs related entry

	int   type;             // for custom user needs
	void *user_private;     // for custom user needs
};

// processes full header + buffer write
size_t st_relay_core_write_headerandbuffer(const struct st_relay_entry_header_s *header, size_t headerlength,
                                           const void *buf, size_t buflength);

// registers with core: on first register st_relay debugfs dir and relayfs channel are setup
// allocates and inits entries as per provided init table; setup debugfs dir associated
struct st_relay_entry_s *st_relay_core_register(int nb_entries, struct st_relay_typename_s *entries_tn_init);
// unregisters from core; on last unregister unwind st_relay debugfs and relayfs
// frees provided entries with debugfs unsetup
void st_relay_core_unregister(int nb_entries, struct st_relay_entry_s *entries);

#ifdef __cplusplus
}; /* extern "C" */
#endif

#endif /* _ST_RELAY_CORE_H_ */

