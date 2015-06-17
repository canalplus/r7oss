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

#if __KERNEL__

#include <linux/kernel.h>

#define errmsg(fmt, ...) pr_crit(fmt, ##__VA_ARGS__)

#else

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>

/* This differs from the __KERNEL__ version of the same macro because
 * there no need to concatenate KERN_CRIT to the format string (and
 * therefore we can avoid using GNU extensions for this version).
 */
#define errmsg(...) do { printf(__VA_ARGS__); fflush(stdout); } while (0)

#endif

#include "report.h"
#include "fatal_error.h"

/* --- */

static void default_error_handler(void *ctx, stm_object_h badobject)
{
    if (badobject)
    {
        errmsg("Streaming engine failure (or precondition violation) when acting upon object at 0x%p\n",
               badobject);
    }
    else
    {
        errmsg("Internal streaming engine failure (or precondition violation)\n");
    }

#if __KERNEL__
    BUG();
#else
    assert(0);
#endif
}

/* --- */

static void *error_ctx = NULL;
static stm_error_handler error_handler = default_error_handler;

void set_fatal_error_handler(void *ctx, stm_error_handler handler)
{
    error_ctx = ctx;
    error_handler = handler;
}

void fatal_error(stm_object_h *obj)
{
    error_handler(error_ctx, obj);
}
