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

#include "encoder_test.h"
#include "common.h"

#include <stm_memsrc.h>
#include <stm_memsink.h>

int MemsinkConnectionTest(EncoderTestArg *arg)
{
    EncoderContext context = {0};
    int result = 0;
    // Our Memsink object
    stm_object_h memsink_object;
    // Create an Encode Object
    result = stm_se_encode_new("Encode0", &context.encode);

    if (result < 0)
    {
        pr_err("Error: %s Failed to create a new Encode\n", __func__);
        return result;
    }

    // Create Stream
    result = stm_se_encode_stream_new("EncodeStream0", context.encode, STM_SE_ENCODE_STREAM_ENCODING_VIDEO_NULL, &context.video_stream[0]);

    if (result < 0)
    {
        pr_err("Error: %s Failed to create a new STM_SE_ENCODE_STREAM_ENCODING_VIDEO_NULL EncodeStream\n", __func__);
        EncoderTestCleanup(&context);
        return result;
    }

    // Create a MemSink
    result = stm_memsink_new("EncoderVideoSink", STM_IOMODE_BLOCKING_IO, KERNEL, (stm_memsink_h *)&memsink_object);

    if (result < 0)
    {
        pr_err("Error: %s Failed to create a new memsink\n", __func__);
        EncoderTestCleanup(&context);
        return result;
    }

    // Attach the memsink
    result = stm_se_encode_stream_attach(context.video_stream[0], memsink_object);

    if (result < 0)
    {
        pr_err("Error: %s Failed to attach encode stream to memsink for video\n", __func__);

        if (stm_memsink_delete((stm_memsink_h)memsink_object) < 0)   // Don't want to leave this hanging around
        {
            pr_err("Error: %s Failed to delete memsink object\n", __func__);
        }

        EncoderTestCleanup(&context);
        return result;
    }

    //
    // Real World would perform data flow here...
    //
    // Detach the memsink
    result = stm_se_encode_stream_detach(context.video_stream[0], memsink_object);

    if (result < 0)
    {
        pr_err("Error: %s Failed to detach encode stream from memsink\n", __func__);

        if (stm_memsink_delete((stm_memsink_h)memsink_object) < 0)    // Don't want to leave this hanging around
        {
            pr_err("Error: %s Failed to delete memsink object\n", __func__);
        }

        EncoderTestCleanup(&context);
        return result;
    }

    // Remove the memsink
    result = stm_memsink_delete((stm_memsink_h)memsink_object);

    if (result < 0)
    {
        pr_err("Error: %s Failed to detach encode stream from memsink\n", __func__);
        EncoderTestCleanup(&context);
        return result;
    }

    // Remove Stream and Encode
    result = stm_se_encode_stream_delete(context.video_stream[0]);

    if (result < 0)
    {
        pr_err("Error: %s Failed to delete STM_SE_ENCODE_STREAM_ENCODING_VIDEO_NULL EncodeStream %d\n", __func__, result);
        EncoderTestCleanup(&context);
        return result;
    }

    result = stm_se_encode_delete(context.encode);

    if (result < 0)
    {
        pr_err("Error: %s stm_se_encode_delete returned %d\n", __func__, result);
        return result;
    }

    // Success
    return result;
}


ENCODER_TEST(MemsinkConnectionTest, "Test the connection of a Memsink", BASIC_TEST);
