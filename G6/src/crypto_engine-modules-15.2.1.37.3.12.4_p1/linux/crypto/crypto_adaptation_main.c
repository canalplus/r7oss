/************************************************************************
Copyright (C) 2012 STMicroelectronics. All Rights Reserved.

This file is part of the Crypto_adaptation Library.

Crypto_adaptation is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License version 2 as published by the
Free Software Foundation.

Crypto_adaptation is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with Crypto_adaptation; see the file COPYING.  If not, write to the Free
Software Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

The Crypto_adaptation Library may alternatively be licensed under a proprietary
license from ST.

Source file name : crypto_adaptation.c

Declares crypto_adaptation module init and exit functions
************************************************************************/
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/moduleparam.h>

#include "crypto_adaptation.h"

/* Module info */
MODULE_AUTHOR("STMicroelectronics");
MODULE_DESCRIPTION("Linux crypto adaptation layer for stm_ce");
MODULE_SUPPORTED_DEVICE("");
MODULE_LICENSE("GPL");

static int __init crypto_init(void)
{
	return ce_crypto_register();
}
module_init(crypto_init);

static void __exit crypto_exit(void)
{
	ce_crypto_unregister();
	ce_destroy_all_sessions();
}
module_exit(crypto_exit);
