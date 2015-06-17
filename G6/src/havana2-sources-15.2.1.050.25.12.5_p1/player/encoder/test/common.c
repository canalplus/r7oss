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
#include "common.h"
#include "osdev_mem.h"
#include "player_types.h"

#include <linux/ktime.h>

#define BPA_PARTITION_NAME "BPA2_Region0"
#define BUFFER_ALIGNMENT_IN_BYTES 16
#define MAX_WAIT_EOS_MS 60000

/// Global audio test context (communication between injection and callback)
EncoderStkpiAudioTest_t gAudioTest;

/// Global video test context (communication between injection and callback)
EncoderStkpiVideoTest_t gVideoTest[TEST_MAX_ENCODE_STREAM];

/// Test encode context
EncoderContext gContext[TEST_MAX_ENCODE] = {{0}};

/// Fake buffer table
bpa_buffer fake_buffer[MAX_FAKE_BUFFER] = {{0}};

char *PassFail(int v)
{
    /*   0 : Pass,
     * < 0 : Failure with standard error return value
     * > 0 : Number of failures detected
     */
    return (0 == v ? "Pass" : "Fail");
}


void EncoderTestCleanup(EncoderContext *context)
{
    // No need to return error on this function helper
    // We simply have to try our best to clean up as much as possible.
    int result = 0;

    if (context->audio_stream[0])
    {
        result = stm_se_encode_stream_delete(context->audio_stream[0]);

        if (result < 0)
        {
            pr_err("Error: %s (audio_stream[0]) stm_se_encode_stream_delete returned %d\n", __func__, result);
        }
    }

    if (context->video_stream[0])
    {
        result = stm_se_encode_stream_delete(context->video_stream[0]);

        if (result < 0)
        {
            pr_err("Error: %s (video_stream[0]) stm_se_encode_stream_delete returned %d\n", __func__, result);
        }
    }

    if (context->encode)
    {
        result = stm_se_encode_delete(context->encode);

        if (result < 0)
        {
            pr_err("Error: %s stm_se_encode_delete returned %d\n", __func__, result);
        }
    }
}

void MultiEncoderTestCleanup(EncoderContext *context)
{
    // No need to return error on this function helper
    // We simply have to try our best to clean up as much as possible.
    int result = 0;
    int i = 0;

    for (i = 0; i < TEST_MAX_ENCODE_STREAM; i++)
    {
        if (context->audio_stream[i])
        {
            pr_info("%s: (audio_stream %d) stm_se_encode_stream_delete call\n", __FUNCTION__, i);
            result = stm_se_encode_stream_delete(context->audio_stream[i]);

            if (result < 0)
            {
                pr_err("Error: %s (audio_stream %d) stm_se_encode_stream_delete returned %d\n", __FUNCTION__, i, result);
            }
        }

        if (context->video_stream[i])
        {
            pr_info("%s: (video_stream %d) stm_se_encode_stream_delete call\n", __FUNCTION__, i);
            result = stm_se_encode_stream_delete(context->video_stream[i]);

            if (result < 0)
            {
                pr_err("Error: %s (video_stream %d) stm_se_encode_stream_delete returned %d\n", __FUNCTION__, i, result);
            }
        }
    }

    if (context->encode)
    {
        result = stm_se_encode_delete(context->encode);

        if (result < 0)
        {
            pr_err("Error: %s stm_se_encode_delete returned %d\n", __FUNCTION__, result);
        }
    }
}

int AllocateBuffer(int size, bpa_buffer *buffer)
{
    unsigned long   physical = 0;
    void           *virtual  = NULL;
    virtual = OSDEV_AllignedMallocHwBuffer(BUFFER_ALIGNMENT_IN_BYTES, size, BPA_PARTITION_NAME, &physical);

    if (virtual == NULL)
    {
        return -ENOMEM;
    }

    buffer->virtual   = virtual;
    buffer->physical  = physical;
    buffer->size      = size;
    buffer->partition = BPA_PARTITION_NAME;
    return 0;
}

int FreeBuffer(bpa_buffer *buffer)
{
    int status;
    status = OSDEV_AllignedFreeHwBuffer(buffer->virtual, buffer->partition);

    if (status < 0)
    {
        pr_err("Error: %s Failed to free buffer of size %d from %s\n", __func__, buffer->size, buffer->partition);
    }

    return status;
}

uint64_t ConvertTimeStamp(uint64_t FromValue, stm_se_time_format_t FromFormat, stm_se_time_format_t ToFormat)
{
    uint64_t ToValue = INVALID_TIME; //In case of invalid format(s)

    //No arithmetic for invalid time stamps, 0,  identical formats.
    if ((FromFormat == ToFormat)
        || (NotValidTime(FromValue))
        || (0 == FromValue))
    {
        return FromValue;
    }

    // integer divisions are rounded to nearest integer: round_div(a,b) {return (a+b/2)/b;}
    switch (ToFormat)
    {
        uint64_t den;//denominator

    case TIME_FORMAT_US :
        switch (FromFormat)
        {
        case TIME_FORMAT_PTS :
            // T*1000/90 => T*200/18 => (T*200+9)/18
            ToValue = (FromValue * 200ULL) + 9ULL ;
            den = 18ULL;
            do_div(ToValue, den);
            return (ToValue);

        case TIME_FORMAT_27MHz :
            // T/27
            ToValue = FromValue + (27ULL / 2);
            den =  27ULL;
            do_div(ToValue, den);
            return (ToValue);

        case TIME_FORMAT_US :
            return (FromValue);
        }

        break;

    case TIME_FORMAT_PTS :
        switch (FromFormat)
        {
        case TIME_FORMAT_US   :
            // T*90/1000 => (T*90 + 500)/1000 => (T*9 + 50)/100
            ToValue = (FromValue * 9ULL) + 50ULL;
            den =  100ULL;
            do_div(ToValue, den);
            return (ToValue);

        case TIME_FORMAT_27MHz:
            // T/300 => (T+150)/300
            ToValue = FromValue + 150ULL;
            den = 300ULL;
            do_div(ToValue, den);
            return (ToValue);

        case TIME_FORMAT_PTS:
            return (FromValue);
        }

        break;

    case TIME_FORMAT_27MHz :
        switch (FromFormat)
        {
        case TIME_FORMAT_PTS  :
            return (FromValue * 300ULL);

        case TIME_FORMAT_US   :
            return (FromValue * 27ULL);

        case TIME_FORMAT_27MHz:
            return (FromValue);
        }

        break;
    }

    return ToValue;
}


/* Return: -1: fail; 0: pass*/
static int AudioPullAndCheckCodedFrame(unsigned int frame_nr,
                                       audio_expected_test_result_t *ExpectedResults,
                                       int *pBytesReceived,
                                       bool *EosReceived)
{
    int retval = 0;
    uint64_t pts_64;
    int64_t pts_ms;
    stm_se_capture_buffer_t *p_memsink_pull_buffer;
    u32 crc = 0;
    unsigned int m_availableLen = 0;
    *pBytesReceived = 0;
    retval = stm_memsink_test_for_data(gAudioTest.Sink.obj, &m_availableLen);

    if (retval && (-EAGAIN != retval))
    {
        pr_err("Error: %s PullAndCheckCodedFrame : stm_memsink_test_for_data failed (%d)\n", __func__, retval);
        return -1;
    }

    if (-EAGAIN == retval)
    {
        // No buffer available
        return 0;
    }

    //
    // setup memsink_pull_buffer
    //
    p_memsink_pull_buffer = (stm_se_capture_buffer_t *)gAudioTest.Sink.m_Buffer;
    p_memsink_pull_buffer->physical_address = NULL;
    p_memsink_pull_buffer->virtual_address  = &gAudioTest.Sink.m_Buffer[sizeof(stm_se_capture_buffer_t)];
    p_memsink_pull_buffer->buffer_length    = gAudioTest.Sink.m_BufferLen - sizeof(stm_se_capture_buffer_t);
    retval = stm_memsink_pull_data(gAudioTest.Sink.obj,
                                   p_memsink_pull_buffer,
                                   p_memsink_pull_buffer->buffer_length,
                                   &m_availableLen);

    if (retval != 0)
    {
        pr_err("Error: %s PullAndCheckCodedFrame : stm_memsink_pull_data fails (%d)\n", __func__, retval);
        return -1;
    }

    *pBytesReceived  = p_memsink_pull_buffer->payload_length;
    //Convert timestamp to reference PTS
    pts_64 = ConvertTimeStamp(p_memsink_pull_buffer->u.compressed.native_time,
                              p_memsink_pull_buffer->u.compressed.native_time_format,
                              TIME_FORMAT_PTS);
    pts_64 = pts_64 & (((1ULL << 33) - 1));
    //compute a friendly signed pts in ms
    //because I know the PTSs I know that if they are close to pts33maxm we wrapped
    {
        uint64_t pts_wrapped = ((1ULL << 33) - 1) - pts_64;

        if (pts_wrapped < pts_64)
        {
            //Wrapped
            pts_ms = pts_wrapped + 45;
            do_div(pts_ms, 90);
            pts_ms = -pts_ms;
            pr_err("Error: %s PullAndCheckCodedFrame : negative time detected (%lld)\n", __func__, pts_ms);
        }
        else
        {
            pts_ms = pts_64 + 45;
            do_div(pts_ms, 90);
        }
    }

    // Check for EOS
    if (p_memsink_pull_buffer->u.compressed.discontinuity & STM_SE_DISCONTINUITY_EOS)
    {
        if (gAudioTest.MorePrints)
        {
            pr_info("\n%s Frame[%03i], EOS Detected! discontinuity 0x%x\n", __func__, frame_nr, p_memsink_pull_buffer->u.compressed.discontinuity);
        }

        *EosReceived = true;
    }

    //Compute crc
    crc = crc32_be(0, p_memsink_pull_buffer->virtual_address, *pBytesReceived);

    if ((0 != *pBytesReceived) && (NULL != ExpectedResults))
    {
        //Check size
        if ((0 != ExpectedResults->expected_results[frame_nr].size)
            && (*pBytesReceived != ExpectedResults->expected_results[frame_nr].size))
        {
            pr_err("Error: %s Frame[%03i], with pts 0x%llX (%d ms),  Expected %03i bytes, got %d\n"
                   , __func__
                   , frame_nr
                   , pts_64, (int32_t)pts_ms
                   , ExpectedResults->expected_results[frame_nr].size, *pBytesReceived
                  );
            return -1;
        }

        // check crc
        if ((0 != ExpectedResults->expected_results[frame_nr].crc)
            && (crc != ExpectedResults->expected_results[frame_nr].crc))
        {
            pr_err("Error: %s Frame[%03i] of %04i bytes, with pts 0x%llX (%d ms), : expected CRC Value  0x%08x, got 0x%08x\n"
                   , __func__
                   , frame_nr, *pBytesReceived
                   , pts_64, (int32_t)pts_ms
                   ,  ExpectedResults->expected_results[frame_nr].crc, crc
                  );
            return -1;
        }

        {
            //Check PTS with a +/- epsilon this is required as we don't have 64 bit precision here.
            uint64_t epsilon = 45; // 45 pts = 0.5 ms

            if ((0 != ExpectedResults->expected_results[frame_nr].pts)
                && ((pts_64 > epsilon + ExpectedResults->expected_results[frame_nr].pts)
                    || (pts_64 < ExpectedResults->expected_results[frame_nr].pts - epsilon)))
            {
                pr_err("Error: %s Frame[%03i] of %04i bytes, with CRC of %08X : expected pts Value 0x%llX (%u ms), got %llX (%d ms)\n"
                       , __func__
                       , frame_nr, *pBytesReceived
                       , crc
                       , ExpectedResults->expected_results[frame_nr].pts, ((uint32_t)ExpectedResults->expected_results[frame_nr].pts) / 90
                       , pts_64, (int32_t)pts_ms
                      );
                return -1;
            }
        }
    }

    if (0 != *pBytesReceived)
    {
        //reach here means "checks ok" or "no checks"
        if (gAudioTest.MorePrints)
        {
            pr_info("%s Frame[%03i] of %04i bytes, with pts 0x%llX (%d ms), CRC = 0x%08X %s\n",
                    __func__,
                    frame_nr, *pBytesReceived,
                    pts_64, (int32_t)pts_ms,
                    crc,
                    ((NULL != ExpectedResults) && (0 != ExpectedResults->expected_results[frame_nr].crc)) ? "Checked OK" : "*not checked*"
                   );
        }
    }

    return 0;
}


static int AudioSendBufferToStream(bpa_buffer *buffer, stm_se_encode_stream_h stream, uint64_t pts, bool EOS, int frames_injected)
{
    int result = 0;
    stm_se_uncompressed_frame_metadata_t descriptor;
    memset(&descriptor, 0, sizeof(stm_se_uncompressed_frame_metadata_t));
    descriptor =  gAudioTest.UMetadata;

    if (EOS)
    {
        descriptor.discontinuity = STM_SE_DISCONTINUITY_EOS;

        if (gAudioTest.MorePrints)
        {
            pr_info("%s: EOS Injected with last buffer\n", __func__);
        }
    }
    if ((gAudioTest.FadeOutFrameNo > 0) && (frames_injected == gAudioTest.FadeOutFrameNo))
    {
        descriptor.discontinuity = STM_SE_DISCONTINUITY_FADEOUT;
    }
    if ((gAudioTest.MuteFrameNo > 0) && (frames_injected == gAudioTest.MuteFrameNo))
    {
        descriptor.discontinuity = STM_SE_DISCONTINUITY_MUTE;
    }
    if ((gAudioTest.FadeInFrameNo > 0) && (frames_injected == gAudioTest.FadeInFrameNo))
    {
        descriptor.discontinuity = STM_SE_DISCONTINUITY_FADEIN;
    }

    if (gAudioTest.TestWithMicroSecInjection)
    {
        descriptor.native_time_format = TIME_FORMAT_US;
    }
    else
    {
        descriptor.native_time_format = TIME_FORMAT_PTS;
    }

    descriptor.native_time = ConvertTimeStamp(pts, TIME_FORMAT_PTS, descriptor.native_time_format);
    descriptor.system_time   = jiffies;
    result = stm_se_encode_stream_inject_frame(stream, buffer->virtual, buffer->physical, buffer->size, descriptor);

    if (result < 0)
    {
        pr_err("Error: %s Failed to send buffer\n", __func__);
    }

    return result;
}


static void AudioFrameEncodedCallback(unsigned int nbevent, stm_event_info_t *events)
{
    unsigned int i;
    EncoderContext *Context;
    stm_event_info_t *EventInfo;

    if (nbevent < 1)
    {
        return;
    }

    EventInfo = events;
    Context = (EncoderContext *)EventInfo->cookie;

    if (events->events_missed)
    {
        pr_err("Error: %s Event(s) missed\n", __func__);
        gAudioTest.NrEventsReceived++;
    }

    if (NULL == Context->mutex)
    {
        return;
    }

    mutex_lock(Context->mutex);

    for (i = 0; i < nbevent; i++)
    {
        EventInfo = (events + i);

        if (!(EventInfo->event.event_id & STM_SE_ENCODE_STREAM_EVENT_FRAME_ENCODED))
        {
            pr_err("Error: %s Event ID %d not expected\n", __func__, EventInfo->event.event_id);
            continue;
        }

        gAudioTest.NrEventsReceived++;
        //@todo here use a sempaphore post
    }

    if (NULL == Context->mutex)
    {
        return;
    }

    mutex_unlock(Context->mutex);
}

int AudioFramesEncode(EncoderContext *context)
{
    unsigned int expected_number_of_frames = 0;
    int result = 0, OverallResult = 0;
    int i;
    int loops_waiting_for_eos = 0;
    unsigned int EventProcessed;
    // Event entry
    stm_event_subscription_entry_t event_entry = { 0 };
    int injected = 0;
    int frames_injected = 0;
    bpa_buffer raw_audio_frame;
    gAudioTest.EosReceived = false;
    // Allocate Memory Buffer
    result = AllocateBuffer(gAudioTest.BytesPerInjection, &raw_audio_frame);

    if (result < 0)
    {
        pr_err("Error: %s Failed to allocate a physical buffer of size %d bytes to store the audio stream (%d)\n", __func__, gAudioTest.BytesPerInjection, result);
        return result;
    }

    // Create Event Subscription
    if (gAudioTest.CollectFramesOnCallback)
    {
        event_entry.object             = context->audio_stream[0];
        event_entry.event_mask         = STM_SE_ENCODE_STREAM_EVENT_FRAME_ENCODED;
        event_entry.cookie             = context;
        result = stm_event_subscription_create(&event_entry, 1, &context->event_subscription[0]);

        if (result < 0)
        {
            pr_err("Error: %s Failed to Create Event Subscription: Stream %x\n", __func__, (uint32_t) & context->audio_stream[0]);
            context->event_subscription[0] = NULL;
            FreeBuffer(&raw_audio_frame);
            return result;
        }

        // Set up Callback Function
        result =  stm_event_set_handler(context->event_subscription[0], &AudioFrameEncodedCallback);

        if (result < 0)
        {
            pr_err("Error: %s Failed to Set Event Handler\n", __func__);

            if (0 >  stm_event_subscription_delete(context->event_subscription[0]))
            {
                pr_err("Error: %s stm_event_subscription_delete failed\n", __func__);
            }

            context->event_subscription[0] = NULL;
            FreeBuffer(&raw_audio_frame);
            return result;
        }

        // Initialize Mutex
        context->mutex = (struct mutex *)kzalloc(sizeof(struct mutex), GFP_KERNEL);

        if (context->mutex == NULL)
        {
            if (0 >  stm_event_subscription_delete(context->event_subscription[0]))
            {
                pr_err("Error: %s stm_event_subscription_delete failed\n", __func__);
            }

            context->event_subscription[0] = NULL;
            FreeBuffer(&raw_audio_frame);
            return -1;
        }

        mutex_init(context->mutex);
        //Initiliase Semaphore
        context->semaphore = (struct semaphore *)kzalloc(sizeof(struct semaphore), GFP_KERNEL);

        if (context->semaphore == NULL)
        {
            kfree(context->mutex);
            context->mutex = NULL;

            if (0 >  stm_event_subscription_delete(context->event_subscription[0]))
            {
                pr_err("Error: %s stm_event_subscription_delete failed\n", __func__);
            }

            context->event_subscription[0] = NULL;
            FreeBuffer(&raw_audio_frame);
            return -1;
        }

        sema_init(context->semaphore, 0);
    }

    /* Check if CRC checks are disabled */
    if (NULL == gAudioTest.ExpectedTestResult)
    {
        pr_warning("%s: WARNING! WARNING! CRC checks are disabled WARNING! WARNING!..\n", __func__);
    }
    else
    {
        expected_number_of_frames = gAudioTest.ExpectedTestResult->expected_nr_frames;
    }

    for (i = 0, injected = 0, gAudioTest.FramesReceived = 0, gAudioTest.NrEventsReceived = 0, EventProcessed = 0 ;
         0 < (gAudioTest.InputVectorSize - injected);
         i++, frames_injected++)
    {
        uint64_t pts;
        int size_to_inject = min(gAudioTest.BytesPerInjection, (gAudioTest.InputVectorSize - injected));

        if (!gAudioTest.CollectFramesOnCallback)
        {
            uint32_t BytesReceived = 0;

            /* Single Thread Read/Write: first read what is available prior to injection  */
            do
            {
                bool EosReceived = false;
                result = AudioPullAndCheckCodedFrame(gAudioTest.FramesReceived, gAudioTest.ExpectedTestResult, &BytesReceived, &EosReceived);

                if (EosReceived)
                {
                    //pr_info("%s EOS Received\n",  __func__);
                    gAudioTest.EosReceived = EosReceived;
                }

                if (result < 0)
                {
                    pr_err("Error: %s PullAndCheckCodedFrame Failed\n", __func__);
                    OverallResult--;
                }
                else if (0 < BytesReceived)
                {
                    gAudioTest.FramesReceived++;
                }

                msleep(gAudioTest.MsPerInjection / 10);
            }
            while (((0 == result) && (0 < BytesReceived)) && (!gAudioTest.EosReceived));
        }
        else
        {
            /* Single Thread Read/Write: first read what is available prior to injection  */
            // Check memsink only if we have received events
            // @todo replace counter check by a semaphore wait timeout immediate
            // Note: NrEventsReceived could wrap, hence != instead of >
            for (; gAudioTest.NrEventsReceived != EventProcessed; EventProcessed++)
            {
                uint32_t BytesReceived = 0;
                bool EosReceived = false;
                result = AudioPullAndCheckCodedFrame(gAudioTest.FramesReceived, gAudioTest.ExpectedTestResult, &BytesReceived, &EosReceived);

                if (EosReceived)
                {
                    //pr_info("%s EOS Received\n",  __func__);
                    gAudioTest.EosReceived = EosReceived;
                }

                if (result < 0)
                {
                    pr_err("Error: %s PullAndCheckCodedFrame Failed\n", __func__);
                    OverallResult--;
                }
                else if (0 < BytesReceived)
                {
                    gAudioTest.FramesReceived++;
                }
            }
        }

        // Do Injection
        // Copy 1 Frame to the physically contiguous buffer
        memcpy(raw_audio_frame.virtual, gAudioTest.InputVector + injected, size_to_inject);
        raw_audio_frame.size = size_to_inject;
        injected += size_to_inject;

        if (!gAudioTest.TestWithPtsWrap)
        {
            /* Start PTS from a 6s offset to be sure no-one force-starts count from 0 */
            pts = (90 * 6000); //6000 ms * 90
        }
        else
        {
            pts = 1;
        }

        /* add the current time
         *  T = (frame_number * frame_duration_ms)*90
         *
         *  frame_number  = (injection_number * (number_of_parts_per_injection / number_of_parts_per_frame);
         *
         *  frame_duration_ms = (1000* number_of_samples_per_frame / sampling_frequency )
         *
         *  frame_duration_ms = (10* number_of_samples_per_frame / (sampling_frequency/100) )
         *
         */
        {
            uint64_t Denom = gAudioTest.UMetadata.audio.core_format.sample_rate / 100;
            uint64_t Num   = (90 * i * gAudioTest.SamplesPerInjection * 10) + (Denom / 2);
            do_div(Num, Denom);
            pts += Num;
        }

        if (gAudioTest.TestWithExtrapolation) //test extrapolation: makes subsequent PTS 0
        {
            if (0 < frames_injected)
            {
                pts = 0x0;
            }
        }

        if (gAudioTest.MorePrints)
        {
            pr_info("%s Injection [%03i], %d bytes %d samples pts = 0x%llX (%u ms), %d samples to go\n", __func__,
                    i, size_to_inject,
                    size_to_inject / (gAudioTest.BytesPerSample * gAudioTest.UMetadata.audio.core_format.channel_placement.channel_count),
                    pts,
                    (uint32_t)pts / 90,
                    (gAudioTest.InputVectorSize - injected) / (gAudioTest.BytesPerSample * gAudioTest.UMetadata.audio.core_format.channel_placement.channel_count));
        }
        if (gAudioTest.InjectEosAfterLastFrame)
        {
            result = AudioSendBufferToStream(&raw_audio_frame, context->audio_stream[0], pts, false, frames_injected);
        }
        else
        {
            result = AudioSendBufferToStream(&raw_audio_frame, context->audio_stream[0], pts, (0 >= (gAudioTest.InputVectorSize - injected)), frames_injected);
        }

        if (result < 0)
        {
            pr_err("Error: %s SendAudioBufferToStream Failed\n", __func__);

            if (gAudioTest.CollectFramesOnCallback)
            {
                kfree(context->mutex);
                context->mutex = NULL;

                if (0 >  stm_event_subscription_delete(context->event_subscription[0]))
                {
                    pr_err("Error: %s stm_event_subscription_delete failed\n", __func__);
                }

                context->event_subscription[0] = NULL;
            }

            FreeBuffer(&raw_audio_frame);
            return result;
        }
    }

    /* Post injection */
    /* Insert discontinuity EOS */
    if (gAudioTest.InjectEosAfterLastFrame)
    {
        result = stm_se_encode_stream_inject_discontinuity(context->audio_stream[0], STM_SE_DISCONTINUITY_EOS);

        if (result < 0)
        {
            pr_err("Error: %s Failed to inject discontinuity EOS\n", __func__);
            FreeBuffer(&raw_audio_frame);
            return result;
        }
    }

    /* Wait for EOS */
    if (gAudioTest.MorePrints)
    {
        pr_info("%s Injection Complete: wait till EOS received\n", __func__);
    }

    while (!gAudioTest.EosReceived)
    {
        /* wait X ms between each try */
        msleep(gAudioTest.MsPerInjection);
        pr_info(".");

        if (!gAudioTest.CollectFramesOnCallback)
        {
            bool EosReceived = false;
            uint32_t BytesReceived = 0;
            result = AudioPullAndCheckCodedFrame(gAudioTest.FramesReceived, gAudioTest.ExpectedTestResult, &BytesReceived, &EosReceived);

            if (EosReceived)
            {
                if (gAudioTest.MorePrints)
                {
                    pr_info("\n%s EOS Received\n",  __func__);
                }

                gAudioTest.EosReceived = true;
            }

            if (result < 0)
            {
                pr_err("\n%s PullAndCheckCodedFrame Failed\n", __func__);
                OverallResult--;
            }
            else if (0 < BytesReceived)
            {
                gAudioTest.FramesReceived++;
            }
        }
        else
        {
            // Check memsink only if we have received events
            // Note EventProcessed could wrap, hence != instead of >
            // @todo replace counter check by a semaphore wait
            for (; gAudioTest.NrEventsReceived != EventProcessed; EventProcessed++)
            {
                uint32_t BytesReceived = 0;
                bool EosReceived = false;
                result = AudioPullAndCheckCodedFrame(gAudioTest.FramesReceived, gAudioTest.ExpectedTestResult, &BytesReceived, &EosReceived);

                if (EosReceived)
                {
                    //pr_info("%s EOS Received\n",  __func__);
                    gAudioTest.EosReceived = EosReceived;
                }

                if (result < 0)
                {
                    pr_err("Error: %s PullAndCheckCodedFrame Failed\n", __func__);
                    OverallResult--;
                }
                else if (0 < BytesReceived)
                {
                    gAudioTest.FramesReceived++;
                }
            }
        }

        loops_waiting_for_eos++;

        if (loops_waiting_for_eos * gAudioTest.MsPerInjection > MAX_WAIT_EOS_MS)
        {
            pr_err("\n%s Waited for EOS for too long: Failed\n", __func__);
            OverallResult--;
            break;
        }
    }

    if (gAudioTest.CollectFramesOnCallback)
    {
        pr_info("\n");
    }

    /* Remove the event */
    if (gAudioTest.CollectFramesOnCallback)
    {
        // Terminate Mutex
        kfree(context->mutex);
        context->mutex = NULL;
        kfree(context->semaphore);
        context->semaphore = NULL;

        if (0 > stm_event_subscription_delete(context->event_subscription[0]))
        {
            pr_err("Error: %s Failed to Delete Event Subscription: Stream %x\n", __func__, (uint32_t) & context->audio_stream[0]);
        }

        context->event_subscription[0] = NULL;
    }

    // Free BPA Memory

    if (0 > FreeBuffer(&raw_audio_frame))
    {
        pr_err("Error: %s Failed to free BPA Buffer (%d)\n", __func__, result);
    }

    /* Check we passed criteria */
    if (0 == OverallResult)
    {
        if ((NULL != gAudioTest.ExpectedTestResult) && (gAudioTest.FramesReceived != expected_number_of_frames))
        {
            pr_err("Error: %s Got different number of frames (%d) than expected (%d)\n\n", __func__, gAudioTest.FramesReceived, expected_number_of_frames);
            OverallResult--;
        }
        else
        {
            if (gAudioTest.MorePrints)
                pr_info("%s: Successfully encoded %d frames, %s\n\n"
                        , __func__,
                        gAudioTest.FramesReceived, (NULL != gAudioTest.ExpectedTestResult) ? "with number of frames, sizes, crc checked" : "with number of frames, sizes, crc *UNCHECKED (!!)*");
        }
    }
    else
    {
        if ((NULL != gAudioTest.ExpectedTestResult) && (gAudioTest.FramesReceived != expected_number_of_frames))
        {
            pr_err("Error: %s Got different number of sucessful frames (%d) than expected (%d)\n\n", __func__, gAudioTest.FramesReceived, expected_number_of_frames);
            OverallResult--;
        }
    }

    return OverallResult;
}

static void VideoFrameEncodedCallback(unsigned int nbevent, stm_event_info_t *events)
{
    unsigned int i;
    EncoderContext *Context;
    stm_event_info_t *EventInfo;

    if (nbevent < 1)
    {
        return;
    }

    EventInfo = events;
    Context = (EncoderContext *)EventInfo->cookie;

    if (events->events_missed)
    {
        pr_info("%s Event(s) missed\n", __func__);
        gVideoTest[Context->stream_id].NrEventsReceived++;
    }

    if (NULL == Context->mutex)
    {
        return;
    }

    mutex_lock(Context->mutex);

    for (i = 0; i < nbevent; i++)
    {
        EventInfo = (events + i);

        switch (EventInfo->event.event_id)
        {
        case STM_SE_ENCODE_STREAM_EVENT_FRAME_ENCODED:
            gVideoTest[Context->stream_id].EncodedFramesReceived++;
            break;
        case STM_SE_ENCODE_STREAM_EVENT_VIDEO_FRAME_SKIPPED:
            gVideoTest[Context->stream_id].SkippedFramesReceived++;
            break;
        case STM_SE_ENCODE_STREAM_EVENT_FATAL_ERROR:
            gVideoTest[Context->stream_id].FatalErrorReceived++;
            break;
        case STM_SE_ENCODE_STREAM_EVENT_FATAL_HARDWARE_FAILURE:
            gVideoTest[Context->stream_id].HardwareFailureReceived++;
            break;
        default:
            pr_err("Error: %s - failed, Event ID %d not expected\n", __func__, EventInfo->event.event_id);
            break;
        }

        gVideoTest[Context->stream_id].NrEventsReceived++;
    }

    mutex_unlock(Context->mutex);
    return;
}

int VideoEncodeEventSubscribe(EncoderContext *context)
{
    int result = 0;
    // Event entry
    stm_event_subscription_entry_t event_entry = { 0 };
    gVideoTest[context->stream_id].EosReceived = false;

    // Create Event Subscription
    event_entry.object             = context->video_stream[context->stream_id];
    event_entry.event_mask         = STM_SE_ENCODE_STREAM_EVENT_FRAME_ENCODED |
                                     STM_SE_ENCODE_STREAM_EVENT_VIDEO_FRAME_SKIPPED |
                                     STM_SE_ENCODE_STREAM_EVENT_FATAL_ERROR |
                                     STM_SE_ENCODE_STREAM_EVENT_FATAL_HARDWARE_FAILURE;
    event_entry.cookie             = context;
    result = stm_event_subscription_create(&event_entry, 1, &context->event_subscription[context->stream_id]);

    if (result < 0)
    {
        pr_err("Error: %s - stm_event_subscription_create failed, Stream %x \n", __func__, (uint32_t) & context->video_stream[context->stream_id]);
        context->event_subscription[context->stream_id] = NULL;
        return result;
    }

    // Set up Callback Function
    result =  stm_event_set_handler(context->event_subscription[context->stream_id], &VideoFrameEncodedCallback);

    if (result < 0)
    {
        pr_err("Error: %s - stm_event_set_handler failed\n", __func__);

        if (0 >  stm_event_subscription_delete(context->event_subscription[context->stream_id]))
        {
            pr_err("Error: %s - stm_event_subscription_delete failed\n", __func__);
        }

        context->event_subscription[context->stream_id] = NULL;
        return result;
    }

    // Initialize Mutex
    context->mutex = (struct mutex *)kzalloc(sizeof(struct mutex), GFP_KERNEL);

    if (context->mutex == NULL)
    {
        if (0 >  stm_event_subscription_delete(context->event_subscription[context->stream_id]))
        {
            pr_err("Error: %s - stm_event_subscription_delete failed\n", __func__);
        }

        context->event_subscription[context->stream_id] = NULL;
        return -1;
    }

    mutex_init(context->mutex);

    //Initiliase Semaphore
    context->semaphore = (struct semaphore *)kzalloc(sizeof(struct semaphore), GFP_KERNEL);

    if (context->semaphore == NULL)
    {
        kfree(context->mutex);
        context->mutex = NULL;

        if (0 >  stm_event_subscription_delete(context->event_subscription[context->stream_id]))
        {
            pr_err("Error: %s - stm_event_subscription_delete failed\n", __func__);
        }

        context->event_subscription[context->stream_id] = NULL;
        return -1;
    }

    sema_init(context->semaphore, 0);

    return 0;
}

int VideoEncodeEventUnsubscribe(EncoderContext *context)
{
    kfree(context->mutex);
    context->mutex = NULL;
    kfree(context->semaphore);
    context->semaphore = NULL;

    if (0 > stm_event_subscription_delete(context->event_subscription[context->stream_id]))
    {
        pr_err("Error: %s - stm_event_subscription_delete failed: Stream %x \n", __func__, (uint32_t) & context->video_stream[context->stream_id]);
        return -1;
    }

    context->event_subscription[context->stream_id] = NULL;

    return 0;
}

unsigned int GetTimeInSeconds(void)
{
    ktime_t kt;
    struct timeval tv;
    struct timespec ts;

    getrawmonotonic(&ts);

    kt = timespec_to_ktime(ts);
    tv = ktime_to_timeval(kt);

    return tv.tv_sec;
}

// In order to avoid scene change, sequence is read
// from start to end and from end to start in a loop
int get_next_frame_id(int frame_id, int max_frame_nb)
{
    static int increment = 1;

    if (frame_id >= (max_frame_nb - 1))
    {
        increment = -1;
    }
    else if (frame_id <= 0)
    {
        increment = 1;
    }
    return (frame_id + increment);
}
