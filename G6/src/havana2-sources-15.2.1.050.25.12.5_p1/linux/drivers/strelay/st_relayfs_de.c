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
#include <linux/module.h>
#include <linux/debugfs.h>
#include <linux/slab.h>
#include <linux/err.h>

#include "st_relayfs_de.h"
#include "st_relay_core.h"

// hash table used to init relay entries; entry names are exposed in st_relay/ debugfs dir
struct st_relay_typename_s  relay_entries_init_de[] = {
	{ ST_RELAY_TYPE_COREDISPLAY_CRC_MISR,        "CoreDisplayCrcMisr" },
	{ ST_RELAY_TYPE_COREDISPLAY_TEST_CAPABILITY, "CoreDisplayTestCapability" },
};
struct st_relay_entry_s *relay_entries_de;
struct st_relay_source_type_count_s *relay_source_type_counts_de;

unsigned int coredisplay_test_indexes[4] = { 0, 0, 0, 0 };   // ST_RELAY_SOURCE_COREDISPLAY_TEST+

bool is_client_de(int value)
{
	if ((value >= ST_RELAY_OFFSET_DE_START) && (value <= ST_RELAY_OFFSET_DE_END)) {
		return true;
	} else {
		return false;
	}
}

void st_relayfs_write_de(unsigned int type, unsigned int source, unsigned char *buf, unsigned int len,
                         void *info)
{
	struct st_relay_entry_header_s header;
	int res;
	int n;
	int relay_entries_nb = ARRAY_SIZE(relay_entries_init_de);
	struct st_relay_entry_s *relay_entries = relay_entries_de;
	int relay_source_type_counts_nb = NB_ENTRIES_SOURCE_TYPE_DE;
	struct st_relay_source_type_count_s *relay_source_type_counts = relay_source_type_counts_de;
	struct st_relay_entry_s *relay_entry = NULL;
	struct st_relay_source_type_count_s *sourcetype_entry = NULL;

	if (buf == NULL) {
		if (len != 0) {
			pr_warn("warning: %s null buf with len:%d\n", __func__, len);
		}
		return;
	}

	if (relay_entries == NULL) {
		// relay channel was not open; return silently
		return;
	}

	// get entry associated to type
	for (n = 0; n < relay_entries_nb; n++) {
		if (relay_entries[n].type == type) {
			relay_entry = &relay_entries[n];
			break;
		}
	}
	if (relay_entry == NULL) {
		pr_err("Error: %s invalid type:%d - no entry\n", __func__, type);
		return;
	}

	// return silently if not active
	if (relay_entry->active == 0) {
		return;
	}

	// get source-type count associated
	for (n = 0; n < relay_source_type_counts_nb; n++) {
		if ((relay_source_type_counts[n].source == 0) && (relay_source_type_counts[n].type == 0)) {
			// first empty slot : take it
			relay_source_type_counts[n].source = source;  // no control on source validity
			relay_source_type_counts[n].type = type;
			relay_source_type_counts[n].count = 0;
			sourcetype_entry = &relay_source_type_counts[n];
			break;
		} else if ((relay_source_type_counts[n].source == source) && (relay_source_type_counts[n].type == type)) {
			// reuse previously set slot
			sourcetype_entry = &relay_source_type_counts[n];
			break;
		}
	}

	if (sourcetype_entry == NULL) {
		pr_err("Error: %s no source type entry: table full\n", __func__);
		return;
	}

	// prepare header
	strncpy(header.name, relay_entry->name, sizeof(header.name));
	header.name[sizeof(header.name) - 1] = '\0';
	header.x      = 0;	//mini meta data
	header.y      = 0;
	header.z      = 0;
	header.ident  = ST_RELAY_MAGIC_IDENT;
	header.source = source;
	header.count  = sourcetype_entry->count;
	header.len    = len;

	// check for specific header contents: none in that case

	res = st_relay_core_write_headerandbuffer(&header, sizeof(header), buf, len);
	if (res != 0) {
		if (sourcetype_entry) { sourcetype_entry->count++; }
	}
}

unsigned int st_relayfs_getindex_forsource_de(unsigned int source)
{
	unsigned int index = 0;
	if (source == ST_RELAY_SOURCE_COREDISPLAY_TEST) {
		TAKE_FIRST_AVAILABLE_INDEX(index, coredisplay_test_indexes);
	} else {
		pr_err("Error: %s invalid source %d\n", __func__, source);
	}
	return index;
}

void st_relayfs_freeindex_forsource_de(unsigned int source, unsigned int index)
{
	if (source == ST_RELAY_SOURCE_COREDISPLAY_TEST) {
		FREE_INDEX(index, coredisplay_test_indexes);
	} else {
		pr_err("Error: %s invalid source %d\n", __func__, source);
	}
}

int st_relayfs_open_de(void)
{
	// source-type counts - inits all to 0 => meaning all empty
	relay_source_type_counts_de = kzalloc(sizeof(struct st_relay_source_type_count_s) * NB_ENTRIES_SOURCE_TYPE_DE,
	                                      GFP_KERNEL);
	if (relay_source_type_counts_de == NULL) {
		pr_err("Error: %s failed alloc\n", __func__);
		return -ENOMEM;
	}

	// set up st_relay debugfs top dir and channel
	relay_entries_de = st_relay_core_register(ARRAY_SIZE(relay_entries_init_de), relay_entries_init_de);
	if (IS_ERR(relay_entries_de)) {
		pr_warn("warning: %s st_relay_core not setup\n", __func__); // can happen if debugfs not setup
		relay_entries_de = NULL;
		kfree(relay_source_type_counts_de);
		relay_source_type_counts_de = NULL;
		return 0;  // don't fail
	}
	if (relay_entries_de == NULL) {
		pr_err("Error: %s register failed\n", __func__);
		kfree(relay_source_type_counts_de);
		relay_source_type_counts_de = NULL;
		return -ENOMEM;
	}

	return 0;
}

void st_relayfs_close_de(void)
{
	kfree(relay_source_type_counts_de);
	relay_source_type_counts_de = NULL;

	if (relay_entries_de != NULL) {
		st_relay_core_unregister(ARRAY_SIZE(relay_entries_init_de), relay_entries_de);
		relay_entries_de = NULL;
	}
}

