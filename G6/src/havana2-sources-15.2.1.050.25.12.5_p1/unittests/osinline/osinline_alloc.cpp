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

#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include <sys/time.h>

#include "osinline.h"

/*
 * Tests for malloc/free interfaces of osinline.h.
 *
 * Note that these tests will actually validate the
 * userland implementation of osinline in this test framework,
 * not the actual kernel implementation in the player.
 * Hence, consider that:
 * - it validates/documents the interface usage,
 * - it validates the osinline part of the test framework,
 * - it does not validate the actual implementation in kernel mode.
 */

TEST(OSInlineAlloc, SimpleAlloc) {
    void *ptr;
    int i;
    unsigned int alloc_sizes[] = { 1, 2, 3, 4, 5, 6, 7, 8,
                                   10 * 1000, 100 * 1000, 1000 * 1000
                                 };

    for (i = 0; i < sizeof(alloc_sizes) / sizeof(*alloc_sizes); i++) {
        ptr = OS_Malloc(alloc_sizes[i]);
        ASSERT_NE((void *)0, ptr);
        memset(ptr, 0, alloc_sizes[i]);
        OS_Free(ptr);
    }
}

TEST(OSInlineAlloc, HugeAlloc) {
    void *ptr;
    int i;
    unsigned int huge_sizes[] = { 1000 * 1000 * 1000U,
                                  2000 * 1000 * 1000U,
                                  3000 * 1000 * 1000U,
                                  4000 * 1000 * 1000U,
                                  ~0U /* unsigned int max. */
                                };

    for (i = 0; i < sizeof(huge_sizes) / sizeof(*huge_sizes); i++) {
        /* Should not create an issue, either fail (return NULL) or
           return the allocated space. */
        ptr = OS_Malloc(huge_sizes[i]);
        OS_Free(ptr);
    }
}

TEST(OSInlineAlloc, InvalidAlloc) {
    void *ptr;

    /* Defined to return NULL. */
    ptr = OS_Malloc(0);
    ASSERT_EQ(NULL, ptr);

    /* Defined to be a no-op. */
    OS_Free(NULL);
}
