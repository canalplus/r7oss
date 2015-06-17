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
 * Tests for tuneable interface of osinline.h.
 *
 * Note that these tests will actually validate the
 * userland implementation of osinline in this test framework,
 * not the actual kernel implementation in the player.
 * Hence, consider that:
 * - it validates/documents the interface usage,
 * - it validates the osinline part of the test framework,
 * - it does not validate the actual implementation in kernel mode.
 */

/*
 * Note that there is mostly nothing tested here except the interface
 * as the userland implementation actually does not provide support
 * for this.
 *
 * TODO:
 * - complete tests for the kernel implementation
 * - optionnaly implement the support in userland and test it.
 */

static unsigned int a_tuneable_value;

TEST(OSInlineTuneable, SimpleTuneable) {

    OS_RegisterTuneable("a_tuneable_value", &a_tuneable_value);

    OS_UnregisterTuneable("a_tuneable_value");
}
