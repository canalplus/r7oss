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
#include <linux/err.h>

#include "st_relayfs_se.h"
#include "st_relayfs_de.h"
#include "st_relayfs_te.h"

MODULE_DESCRIPTION("strelay client functions");
MODULE_AUTHOR("STMicroelectronics");
MODULE_LICENSE("GPL");

// used by transport and display engines
EXPORT_SYMBOL(st_relayfs_write);
EXPORT_SYMBOL(st_relayfs_getindex);
EXPORT_SYMBOL(st_relayfs_freeindex);

// used by streaming engine : direct calls (no indirection)
// TODO(pht) move these functions directly in player2 module
EXPORT_SYMBOL(st_relayfs_write_se);
EXPORT_SYMBOL(st_relayfs_print_se);
EXPORT_SYMBOL(st_relayfs_getindex_forsource_se);
EXPORT_SYMBOL(st_relayfs_freeindex_forsource_se);
EXPORT_SYMBOL(st_relayfs_getindex_fortype_se);
EXPORT_SYMBOL(st_relayfs_freeindex_fortype_se);

void st_relayfs_write(unsigned int type, unsigned int source, unsigned char *buf, unsigned int len, void *info)
{
	BUG_ON(is_client_se(type));

	if (is_client_de(type)) {
		st_relayfs_write_de(type, source, buf, len, info);
	} else if (is_client_te(type)) {
		st_relayfs_write_te(type, source, buf, len, info);
	}
}

unsigned int st_relayfs_getindex(unsigned int source)
{
	BUG_ON(is_client_se(source));

	if (is_client_de(source)) {
		return st_relayfs_getindex_forsource_de(source);
	} else if (is_client_te(source)) {
		return st_relayfs_getindex_forsource_te(source);
	} else {
		return 0;
	}
}

void st_relayfs_freeindex(unsigned int source, unsigned int index)
{
	BUG_ON(is_client_se(source));

	if (is_client_de(source)) {
		st_relayfs_freeindex_forsource_de(source, index);
	} else if (is_client_te(source)) {
		st_relayfs_freeindex_forsource_te(source, index);
	}
}

int st_relayfs_open(void)
{
	int status;

	status = st_relayfs_open_se();
	if (status) {
		return -ENOMEM;
	}

	status = st_relayfs_open_de();
	if (status) {
		st_relayfs_close_se();
		return -ENOMEM;
	}

	status = st_relayfs_open_te();
	if (status) {
		st_relayfs_close_de();
		st_relayfs_close_se();
		return -ENOMEM;
	}

	pr_info("%s done ok\n", __func__);
	return 0;
}

void st_relayfs_close(void)
{
	st_relayfs_close_se();
	st_relayfs_close_de();
	st_relayfs_close_te();

	pr_info("%s done\n", __func__);
}

module_init(st_relayfs_open);
module_exit(st_relayfs_close);

