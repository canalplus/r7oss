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

#include "osinline.h"
#include "ksound.h"
#include "st_relayfs_se.h"
#include "player_threads.h"

#include "audio_reader.h"
#include "timestamps.h"
#undef TRACE_TAG
#define TRACE_TAG "Audio_Reader_c"

static OS_TaskEntry(ReaderThreadStub)
{
    Audio_Reader_c *Reader = (Audio_Reader_c *) Parameter;
    Reader->ReaderThread();
    OS_TerminateThread();
    return NULL;
}

unsigned char Audio_Reader_c::CalculateSamplingFreq(snd_pcm_uframes_t AudioFrames,
                                                    signed long long TimeDiffuSecs,
                                                    int32_t SampleDiff)
{
    uint32_t                   CalculatedSampleRate;
    uint32_t                   IsoSampleRate;
    unsigned char              DiscreteSampleRate;

    /* There are 2 ways of deducing sampling freqeuncy:
     * 1. from reader control STM_SE_CTRL_AUDIO_READER_SOURCE_INFO
     *    This info is obtained from the input driver (HDMi Rx)
     * 2. Calculate sampling frequency based timestamp differences
     *    and the number of samples captured during that time
     * If the frequency indicated in the SOURCE_INFO then use that
     * other wise use method 2 to calculate frequency
     */
    StmSeTranslateIntegerSamplingFrequencyToHdmi(ReaderSourceInfo.sampling_frequency, &DiscreteSampleRate);
    if (CEA861_0k != DiscreteSampleRate)
    {
        SampleRate = ReaderSourceInfo.sampling_frequency;
        return DiscreteSampleRate;
    }
    if (TimeDiffuSecs == 0 || TimeDiffuSecs > AUDIO_READER_MAX_TIME_DIFF)
    {
        SE_DEBUG(group_audio_reader, "time diff is invalid..skip : %llu\n", TimeDiffuSecs);
        return CEA861_0k;
    }

    int               LogIndex     = MonitoringIndex % AUDIO_READER_SAMPLE_RATE_LOCK;
    snd_pcm_uframes_t ActualFrames = AudioFrames - SampleDiff;
    MonitoringTimeWindow        += TimeDiffuSecs - MonitoringTimeDiffuSecs[LogIndex];
    MonitoringAudioFramesWindow += ActualFrames - MonitoringAudioFrames[LogIndex];

    MonitoringTimeDiffuSecs[LogIndex] = TimeDiffuSecs;
    MonitoringAudioFrames[LogIndex]   = ActualFrames;
    MonitoringIndex++;

    if (MonitoringIndex >= AUDIO_READER_SAMPLE_RATE_LOCK)
    {
        /* MonitoringTimeWindow is accumulatation of TimeDiffuSecs - last TimeDiffuSecs.
           Function return before execution of this code if TimeDiffuSecs is zero.
           so MonitoringTimeWindow can never be zero. */

        CalculatedSampleRate = (MonitoringAudioFramesWindow * 1000000ull) / (MonitoringTimeWindow);
    }
    else
    {
        // Function return before execution of this code if TimeDiffuSecs is zero.
        CalculatedSampleRate = (((unsigned long long)ActualFrames) * 1000000ull) / (TimeDiffuSecs);
    }

    if (CalculatedSampleRate <= 16000)
    {
        IsoSampleRate = 0;
    }
    else if (CalculatedSampleRate <= MID(32000, 44100))
    {
        IsoSampleRate = 32000;
    }
    else if (CalculatedSampleRate <= MID(44100, 48000))
    {
        IsoSampleRate = 44100;
    }
    else if (CalculatedSampleRate <= MID(48000, 64000))
    {
        IsoSampleRate = 48000;
    }
    else if (CalculatedSampleRate <= MID(88200, 96000))
    {
        IsoSampleRate = 88200;
    }
    else if (CalculatedSampleRate <= MID(96000, 128000))
    {
        IsoSampleRate = 96000;
    }
    else if (CalculatedSampleRate <= MID(176400, 192000))
    {
        IsoSampleRate = 176400;
    }
    else
    {
        IsoSampleRate = 192000;
    }
    StmSeTranslateIntegerSamplingFrequencyToHdmi(IsoSampleRate, &DiscreteSampleRate);
    if (IsoSampleRate != SampleRate)
    {
        if (LastSampleRate != IsoSampleRate)
        {
            LastSampleRate        = IsoSampleRate;
            SampleRateChangeCount = 0;
        }

        SE_INFO(group_audio_reader, "New Rate estimated[%d :: %llu / %llu ]:%d [recorded as %d]\n",
                SampleRateChangeCount, MonitoringAudioFramesWindow, MonitoringTimeWindow,
                CalculatedSampleRate, IsoSampleRate);

        if (++SampleRateChangeCount == AUDIO_READER_SAMPLE_RATE_LOCK)
        {
            SampleRate = LastSampleRate;
            SampleRateChangeCount = 0;
            SE_INFO(group_audio_reader, "New Rate locked[%d]:%d [recorded as %d]\n",
                    SampleRateChangeCount, CalculatedSampleRate, IsoSampleRate);
        }
        else
        {
            DiscreteSampleRate = ReaderDiscreteSampleRate;
        }
    }

    return DiscreteSampleRate;
}

void Audio_Reader_c::PutBits(BitPacker_t *ld, uint32_t code, uint32_t length)
{
    uint32_t bit_buf;
    uint32_t bit_left;
    bit_buf = ld->BitBuffer;
    bit_left = ld->Remaining;

    if (length < bit_left)
    {
        /* fits into current buffer */
        bit_buf = (bit_buf << length) | code;
        bit_left -= length;
    }
    else
    {
        /* doesn't fit */
        bit_buf <<= bit_left;
        bit_buf |= code >> (length - bit_left);
        ld->Ptr[0] = (char)(bit_buf >> 24);
        ld->Ptr[1] = (char)(bit_buf >> 16);
        ld->Ptr[2] = (char)(bit_buf >> 8);
        ld->Ptr[3] = (char)bit_buf;
        ld->Ptr += 4;
        length -= bit_left;
        bit_buf = code & ((1 << length) - 1);
        bit_left = 32 - length;
    }

    /* writeback */
    ld->BitBuffer = bit_buf;
    ld->Remaining = bit_left;
}

void Audio_Reader_c::FlushBits(BitPacker_t *ld)
{
    ld->BitBuffer <<= ld->Remaining;

    while (ld->Remaining < 32)
    {
        *ld->Ptr++ = ld->BitBuffer >> 24;
        ld->BitBuffer <<= 8;
        ld->Remaining += 8;
    }

    ld->Remaining = 32;
    ld->BitBuffer = 0;
}

int32_t Audio_Reader_c::InsertPrivateDataHeader(uint8_t *data,
                                                int32_t payload_size,
                                                int32_t sampling_frequency)
{
    BitPacker_t ld2 = { data, 0, 32 };
    /* payload_size in bytes */
    PutBits(&ld2, payload_size, 16);
    /* Channel Assignment */
    PutBits(&ld2, ReaderSourceInfo.channel_alloc, 5);
    /* Sampling Freq - refer eCEA861_Sfrequency */
    PutBits(&ld2, sampling_frequency, 4);
    /* Bits Per Sample */
    PutBits(&ld2, 0, 2);
    /* Emphasis Flag */
    PutBits(&ld2, 0, 1);
    /* No. Of Channels*/
    PutBits(&ld2, ReaderSourceInfo.channel_count, 4);
    /* down_mix_inhibit */
    PutBits(&ld2, ReaderSourceInfo.down_mix_inhibit, 1);
    /* level_shift_value */
    PutBits(&ld2, ReaderSourceInfo.level_shift_value, 4);
    /* lfe_playback_level */
    PutBits(&ld2, ReaderSourceInfo.lfe_playback_level, 2);
    /* stream_type */
    PutBits(&ld2, ReaderSourceInfo.stream_type, 4);
    /* Reserved */
    PutBits(&ld2, 0, 21);
    FlushBits(&ld2);
    return (ld2.Ptr - data);
}

int32_t Audio_Reader_c::InsertPesHeader(uint8_t *data, int32_t size,
                                        uint8_t stream_id,
                                        unsigned long long int pts,
                                        int32_t pic_start_code)
{
    BitPacker_t ld2 = { data, 0, 32 };

    if (size > MAX_PES_PACKET_SIZE)
    {
        SE_ERROR("Packet bigger than 63.9K size:%d\n", size);
    }

    /* Start Code: 0-4:4 */
    PutBits(&ld2, 0x0, 8);
    PutBits(&ld2, 0x0, 8);
    PutBits(&ld2, 0x1, 8);
    PutBits(&ld2, stream_id, 8);
    /* PES Packet Length: 5-6:2 */
    PutBits(&ld2, size + 3 + (pts != INVALID_PTS_VALUE ? 5 : 0) + (pic_start_code ? (5 /*+1 */) : 0), 16);
    /* 7:1 */
    PutBits(&ld2, 0x2, 2);  // 10
    /* PES_Scrambling_control */
    PutBits(&ld2, 0x0, 2);
    /* PES_Priority */
    PutBits(&ld2, 0x0, 1);
    /* data_alignment_indicator */
    PutBits(&ld2, 0x1, 1);
    /* Copyright */
    PutBits(&ld2, 0x0, 1);
    /* Original or Copy */
    PutBits(&ld2, 0x0, 1);

    /* 8:1 */
    /* PTS_DTS flag */
    if (pts != INVALID_PTS_VALUE)
    {
        PutBits(&ld2, 0x2, 2);
    }
    else
    {
        PutBits(&ld2, 0x0, 2);
    }

    /* ESCR_flag */
    PutBits(&ld2, 0x0, 1);
    /* ES_rate_flag */
    PutBits(&ld2, 0x0, 1);
    /* DSM_trick_mode_flag */
    PutBits(&ld2, 0x0, 1);
    /* additional_copy_ingo_flag */
    PutBits(&ld2, 0x0, 1);
    /* PES_CRC_flag */
    PutBits(&ld2, 0x0, 1);
    /* PES_extension_flag */
    PutBits(&ld2, 0x0, 1);

    /* PES_header_data_length 9:1 */
    if (pts != INVALID_PTS_VALUE)
    {
        PutBits(&ld2, 0x5, 8);
    }
    else
    {
        PutBits(&ld2, 0x0, 8);
    }

    /* PTS 10-14:5*/
    if (pts != INVALID_PTS_VALUE)
    {
        PutBits(&ld2, 0x2, 4);
        PutBits(&ld2, (pts >> 30) & 0x7, 3);
        PutBits(&ld2, 0x1, 1);
        PutBits(&ld2, (pts >> 15) & 0x7fff, 15);
        PutBits(&ld2, 0x1, 1);
        PutBits(&ld2, pts & 0x7fff, 15);
        PutBits(&ld2, 0x1, 1);
    }

    if (pic_start_code)
    {
        PutBits(&ld2, 0x0, 8);
        PutBits(&ld2, 0x0, 8);
        PutBits(&ld2, 0x1, 8);
        PutBits(&ld2, pic_start_code & 0xff, 8);
        PutBits(&ld2, (pic_start_code >> 8) & 0xff, 8);
        //14 + 4 = 18
    }

    FlushBits(&ld2);
    return (ld2.Ptr - data);
}

void Audio_Reader_c::CalculateReaderBufferSize(void)
{
    AudioPeriodFrames   = AdjustedGrain();

    AudioBufferFrames   = AudioPeriodFrames * AUDIO_READER_NUM_OF_BUFFERS;
    CaptureGrainUpdated = false;
}

unsigned long long Audio_Reader_c::ProcessTiming(snd_pcm_uframes_t AudioFrames)
{
    unsigned long long TimeDiffuSecs;
    int32_t            SampleDiff;
    snd_pcm_uframes_t  SamplesAvailable;
    unsigned long long TimeStampuSecs;
    int32_t            Result;

    Result = PcmGetTime(&SamplesAvailable, &TimeStampuSecs);

    if (Result < 0)
    {
        return INVALID_TIME;
    }

    if (NotValidTime(PrevTimeStampuSecs))
    {
        PrevTimeStampuSecs   = TimeStampuSecs;
        PrevSamplesAvailable = SamplesAvailable;
        return INVALID_TIME;
    }

    /* Calculate Sampling Frequency */
    TimeDiffuSecs = TimeStampuSecs   - PrevTimeStampuSecs;
    SampleDiff    = SamplesAvailable - PrevSamplesAvailable;

    /* There are chances that the data buffered in the capture buffer is more than PeriodSize
     * i.e. AudioFrames > PeriodSize.
     * Also the timestamp obtained from ksnd will be associated with AudioFrames and NOT PeriodSize.
     * But only PeriodSize worth of data is processed in SE Reader at one time
     * AudioFramesRem represents the extra data being buffered.
     * This will be used in calculating exact number of samples arrived in
     * TimeDiffuSecs time
     * Without using AudioFramesRem,wrong sampling freq will be detected
     * especially at start up
     */
    if (AudioFramesRem < AudioFrames)
    {
        AudioFrames -= AudioFramesRem;
    }
    else
        SE_DEBUG(group_audio_reader, "AudioFramesRem > AudioFrames is unexpected %u %u\n",
                 (unsigned int)AudioFrames, (unsigned int)AudioFramesRem);

    ReaderDiscreteSampleRate      = (eCEA861_Sfrequency) CalculateSamplingFreq(AudioFrames, TimeDiffuSecs, SampleDiff);
    PrevSamplesAvailable     = SamplesAvailable;
    PrevTimeStampuSecs       = TimeStampuSecs;
    return TimeStampuSecs;
}

void Audio_Reader_c::ProcessPacket(const snd_pcm_channel_area_t *CaptureAreas,
                                   snd_pcm_uframes_t CaptureOffset,
                                   snd_pcm_uframes_t CaptureFrames,
                                   unsigned long long TimeStampuSecs)
{
    uint32_t            HeaderLength, AudioDataSize;
    uint8_t             PesHeader[PES_AUDIO_HEADER_SIZE];
    uint8_t            *AudioData   = NULL;

    AudioData = (uint8_t *)CaptureAreas[0].addr +
                (CaptureAreas[0].first / 8) +
                (CaptureOffset * CaptureAreas[0].step / 8);
    AudioDataSize = CaptureFrames * (AUDIO_SAMPLE_DEPTH * NumChannels / 8);

    if (ReaderDiscreteSampleRate == CEA861_0k)
    {
        SE_DEBUG(group_audio_reader, "Invalid sampleratefreq\n");
        return;
    }
    /* Generate PES Header */
    memset(PesHeader, 0, PES_AUDIO_HEADER_SIZE);
    HeaderLength = InsertPesHeader(PesHeader, AudioDataSize + SPDIFIN_PRIVATE_HEADER_LENGTH, PES_PRIVATE_STREAM1, INVALID_PTS_VALUE, 0);
    HeaderLength += InsertPrivateDataHeader(&PesHeader[HeaderLength], AudioDataSize, ReaderDiscreteSampleRate);
    st_relayfs_write_se(ST_RELAY_TYPE_DATA_FROM_PCM0, ST_RELAY_SOURCE_SE, AudioData, AudioDataSize, 0);

    /* Inject PES Header,Data Packet to PlayStream */
    PlayerInputDescriptor_t   InputDescriptor;
    InputDescriptor.PlaybackTimeValid          = true;
    InputDescriptor.PlaybackTime               = TimeStampuSecs;
    InputDescriptor.DecodeTimeValid            = false;
    InputDescriptor.SourceTimeFormat           = TimeFormatUs;
    InputDescriptor.DataSpecificFlags          = 0;
    PlayStream->InjectData(PesHeader, HeaderLength, InputDescriptor);
    InputDescriptor.PlaybackTimeValid          = false;
    PlayStream->InjectData(AudioData, AudioDataSize, InputDescriptor);

}

int32_t Audio_Reader_c::PcmWait(snd_pcm_uframes_t  *CaptureFrames)
{
    int32_t Result = 0;

    *CaptureFrames = ksnd_pcm_avail_update(SoundcardHandle);

    /* If Reader is stopped and if ksnd_pcm_wait() returns error, the reader
     * thread will never exit,hence the check for ReaderThreadRunning */

    // Calling ksnd_pcm_wait should return as soon as avail_min samples are available
    // allow the worst case wait period duration of the AudioBufferFrames + 10 ms (1 jiffy because
    // If "schedule_timeout" function is called just before jiffies is incremented it will wait atleast worst case wait period).

    uint32_t timeout_in_ms;
    uint32_t PeriodSampleRate = (SampleRate) ? SampleRate : AUDIO_DEFAULT_SAMPLE_RATE;
    timeout_in_ms = AudioBufferFrames * 1000 / PeriodSampleRate + 10;
    while ((*CaptureFrames < AudioPeriodFrames) && (ReaderThreadRunning))
    {
        Result = ksnd_pcm_wait(SoundcardHandle, timeout_in_ms);
        if (Result <= 0)
        {
            SE_ERROR("Failed to wait for capture period expiry: %d\n", Result);
            break;
        }
        *CaptureFrames = ksnd_pcm_avail_update(SoundcardHandle);
    }

    return Result;
}

int32_t Audio_Reader_c::PcmSetParams()
{
    int32_t Result;

    CalculateReaderBufferSize();

    SE_DEBUG(group_audio_reader, "SetParams: Channels: %d sample Depth : %d SampleRate : %d PeriodFrame: %d BufferFrame: %d\n",
             NumChannels, AUDIO_SAMPLE_DEPTH, SampleRate, (unsigned int)AudioPeriodFrames,
             (unsigned int)AudioBufferFrames);

    Result = ksnd_pcm_set_params(SoundcardHandle, NumChannels, AUDIO_SAMPLE_DEPTH,
                                 SampleRate, AudioPeriodFrames, AudioBufferFrames);

    return Result;
}

int32_t Audio_Reader_c::PcmGetTime(snd_pcm_uframes_t *SamplesAvailable, unsigned long long *TimeStampUs)
{
    int32_t            Result;
    struct timespec    TimeStampAsTimeSpec;
    snd_pcm_uframes_t  Samples;

    Result = ksnd_pcm_mtimestamp(SoundcardHandle, &Samples, &TimeStampAsTimeSpec);

    if (Result == 0)
    {
        *SamplesAvailable = Samples;
        *TimeStampUs      = (TimeStampAsTimeSpec.tv_sec * 1000000ll + TimeStampAsTimeSpec.tv_nsec / 1000);

        SE_DEBUG(group_audio_reader, "SamplesAvailable %d / TimeStampUs %llu\n", (unsigned int) *SamplesAvailable, *TimeStampUs);
    }
    else
    {
        SE_ERROR("%x\n", Result);
    }

    return Result;
}

int32_t  Audio_Reader_c::PcmStart()
{
    int32_t Result;

    Result  = PcmSetParams();
    Result |= ksnd_pcm_prepare(SoundcardHandle);

    SE_DEBUG(group_audio_reader, "prepare : %x\n", Result);
    if (Result == 0)
    {
        Result = ksnd_pcm_start(SoundcardHandle);
        SE_DEBUG(group_audio_reader, "start : %x\n", Result);

        // run a first "blind" capture to ensure we get stable timings
        // once we are in the capture and process loop.
        if (Result == 0)
        {
            const snd_pcm_channel_area_t *CaptureAreas = NULL;
            snd_pcm_uframes_t             CaptureFrames, CaptureOffset;
            snd_pcm_sframes_t             CommitedFrames;

            // Calculate the SampleRate
            SampleRate            = 0;
            LastSampleRate        = 0;
            SampleRateChangeCount = 0;
            PrevTimeStampuSecs    = INVALID_TIME;
            do
            {
                Result = PcmWait(& CaptureFrames);
                if (Result < 0)
                {
                    return Result;
                }
                else if (CaptureFrames == 0)
                {
                    return -1;
                }

                if (NotValidTime(PrevTimeStampuSecs))
                {
                    // Get the timestamp right now so that one can compute the sfreq
                    // from the very next acquired block.
                    Result = PcmGetTime(&PrevSamplesAvailable, &PrevTimeStampuSecs);
                    if (Result < 0)
                    {
                        return Result;
                    }
                }
                else
                {
                    unsigned long long TimeStampuSecs = ProcessTiming(CaptureFrames);
                    if (NotValidTime(TimeStampuSecs))
                    {
                        /* dont process the current packet */
                        SE_ERROR("Cannot read the sound card timestamp\n");
                        return -1;
                    }
                }

                // acquire the first block
                Result = ksnd_pcm_mmap_begin(SoundcardHandle, &CaptureAreas,
                                             &CaptureOffset, &CaptureFrames);
                if (Result < 0)
                {
                    SE_ERROR("Failed to mmap capture buffer\n");
                    return Result;
                }

                // release the first block (without processing it).
                CommitedFrames = ksnd_pcm_mmap_commit(SoundcardHandle, CaptureOffset,
                                                      CaptureFrames);
                if (CommitedFrames < 0 ||
                    (snd_pcm_uframes_t) CommitedFrames != CaptureFrames)
                {
                    SE_ERROR("Capture XRUN (commit %ld captured %ld)\n", CommitedFrames, CaptureFrames);
                    PcmErrorRecovery();
                }
            }
            while (SampleRate == 0);
        }
    }
    else
    {
        SE_ERROR("unrecoverable error in reader %x\n", Result);
    }

    return Result;
}

int32_t  Audio_Reader_c::PcmRestart()
{
    int32_t Result     = ksnd_pcm_stop(SoundcardHandle);

    if (Result == 0)
    {
        Result = PcmStart();
    }

    return Result;
}

int32_t Audio_Reader_c::PcmErrorRecovery()
{
    int32_t Result = 0;

    do
    {
        // Reinitialise the SamplingFrequency so that the PcmParams can be
        // reset to a valid value.
        ReaderDiscreteSampleRate      = CEA861_48k;
        SampleRate               = AUDIO_DEFAULT_SAMPLE_RATE;

        Result = PcmRestart();

        if (Result == 0)
        {
            SE_DEBUG(group_audio_reader, "Reader Error Recovered\n");
        }
        else
        {
            SE_ERROR("Reader Error Recovery Failed\n");
        }

    }
    while ((ReaderThreadRunning) && (Result < 0));

    return Result;
}

void Audio_Reader_c::ReaderThread()
{
    int32_t                       Result       = 0;
    const snd_pcm_channel_area_t *CaptureAreas = NULL;
    snd_pcm_uframes_t             CaptureFrames, CaptureOffset;
    snd_pcm_sframes_t             CommitedFrames;
    unsigned long long            TimeStampuSecs;

    // Wait for mixer thread to be in safe state (no more MME commands issued)
    do
    {
        OS_WaitForEventAuto(&ReaderInfoUpdated, OS_INFINITE);
    }
    while (ReaderThreadRunning && !ReaderInfoPresent);

    if (ReaderThreadRunning)
    {
        Result = PcmStart();

        if ((Result < 0) && ReaderThreadRunning)
        {
            PcmErrorRecovery();
        }
    }

    while (ReaderThreadRunning)
    {
        Result = PcmWait(&CaptureFrames);

        while ((ReaderThreadRunning) && (Result < 0))
        {
            Result = PcmErrorRecovery();
        }

        if (ReaderThreadRunning == false)
        {
            break;
        }

        TimeStampuSecs = ProcessTiming(CaptureFrames);

        if (NotValidTime(TimeStampuSecs))
        {
            /* dont process the current packet */
            SE_ERROR("Failed to read sound card timestamp\n");
            continue;
        }


        if (CaptureFrames > AudioPeriodFrames)
        {
            SE_DEBUG(group_audio_reader, "CaptureFrames %lld\n", (unsigned long long)CaptureFrames);
            AudioFramesRem = CaptureFrames - AudioPeriodFrames;
        }
        else
        {
            AudioFramesRem = 0;
        }

        CaptureFrames = AudioPeriodFrames;

        /* Logic for Dynamic period size calculation based on frequency
         * once the frequency is deduced,unireader is re-programmed with the
         * period size ( and buffer size) corresponding to the frequency.
         */
        uint32_t ExpectedGrain = AdjustedGrain();
        if ((ReaderDiscreteSampleRate != CEA861_0k) &&
            (AudioPeriodFrames != ExpectedGrain))
        {
            SE_DEBUG(group_audio_reader, "Reader Period Mismatch %d:%d\n", (unsigned int)AudioPeriodFrames, ExpectedGrain);
            PcmRestart();
            AudioFramesRem = 0;
            continue;
        }

        Result = ksnd_pcm_mmap_begin(SoundcardHandle, &CaptureAreas,
                                     &CaptureOffset, &CaptureFrames);

        if (Result < 0)
        {
            SE_ERROR("Failed to mmap capture buffer\n");
            break;
        }
        if (CaptureFrames == 0)
        {
            SE_DEBUG(group_audio_reader, "No Captured Samples\n");
            continue;
        }

        /* If Play Stream is connected, then process packet and inject */
        OS_LockMutex(&PlayStreamAttachMutex);

        if (PlayStream)
        {
            ProcessPacket(CaptureAreas, CaptureOffset, CaptureFrames, TimeStampuSecs);
        }

        OS_UnLockMutex(&PlayStreamAttachMutex);
        CommitedFrames = ksnd_pcm_mmap_commit(SoundcardHandle, CaptureOffset,
                                              CaptureFrames);

        if (CommitedFrames < 0 ||
            (snd_pcm_uframes_t) CommitedFrames != CaptureFrames)
        {
            SE_ERROR("Capture XRUN (commit %ld captured %ld)\n",
                     CommitedFrames, CaptureFrames);
            PcmErrorRecovery();
        }

        /*Check if the source info ( from HDMI rx) is different from unireader
         * parameters. If yes,re-configure unireader and start.
         * Currently only sampling frequency and channel count is considered.
         * remaining parameters channel_alloc,level_shift_value,lfe_playback_level
         * down_mix_inhibit are not yet used*/
        if (ReaderInfoPresent)
        {
            if (NumChannels != ReaderSourceInfo.channel_count)
            {
                /*if num of channels set using Reader control is not same as the
                 * default one, reconfigure uni reader */
                SE_DEBUG(group_audio_reader, "Num of Channels changed %d->%d,Reader to be reconfigured",
                         NumChannels, ReaderSourceInfo.channel_count);
                NumChannels = ReaderSourceInfo.channel_count;
                PcmRestart();
            }
        }
        if (CaptureGrainUpdated)
        {
            PcmRestart();
        }
    }

    OS_Smp_Mb(); // Read memory barrier: rmb_for_AudioReader_Terminating coupled with: wmb_for_AudioReader_Terminating

    ksnd_pcm_close(SoundcardHandle);
    SE_DEBUG(group_audio_reader,  "Terminating\n");
    OS_SetEvent(&ReaderThreadTerminated);
}

PlayerStatus_t Audio_Reader_c::StartReaderThread()
{
    if (ReaderThreadRunning)
    {
        SE_ERROR("Audio Reader thread is already running\n");
        return PlayerError;
    }

    ReaderThreadRunning = true;

    OS_Thread_t Thread;
    if (OS_CreateThread(&Thread, ReaderThreadStub, this, &player_tasks_desc[SE_TASK_AUDIO_READER]) != OS_NO_ERROR)
    {
        SE_ERROR("Unable to create audio reader thread\n");
        ReaderThreadRunning = false;
        return PlayerError;
    }

    SE_DEBUG(group_audio_reader, "Reader Thread Created\n");
    return PlayerNoError;
}

void Audio_Reader_c::TerminateReaderThread()
{
    if (ReaderThreadRunning)
    {
        // Ask thread to terminate
        OS_ResetEvent(&ReaderThreadTerminated);
        OS_Smp_Mb(); // Write memory barrier: wmb_for_AudioReader_Terminating coupled with: rmb_for_AudioReader_Terminating
        ReaderThreadRunning = false;
        ReaderInfoPresent   = false;
        OS_SetEvent(&ReaderInfoUpdated);

        // wait for the thread to terminate
        OS_WaitForEventAuto(&ReaderThreadTerminated, OS_INFINITE);
    }

    SE_DEBUG(group_audio_reader, "Reader Thread Deleted\n");
}

Audio_Reader_c::Audio_Reader_c(const char *name,
                               const char *hw_name)
    : Alsaname()
    , SoundcardHandle(NULL)
    , SampleRate(AUDIO_DEFAULT_SAMPLE_RATE)
    , LastSampleRate(0)
    , SampleRateChangeCount(0)
    , NumChannels(AUDIO_DEFAULT_CHANNELS)
    , ReaderDiscreteSampleRate(CEA861_48k)
    , AudioPeriodFrames(0)
    , AudioBufferFrames(0)
    , AudioFramesRem(0)
    , PlayStream(NULL)
    , PlayStreamAttachMutex()
    , ReaderThreadRunning(false)
    , ReaderThreadTerminated()
    , PrevTimeStampuSecs(INVALID_TIME)
    , PrevSamplesAvailable(0)
    , MonitoringIndex(0)
    , MonitoringTimeWindow(0)
    , MonitoringAudioFramesWindow(0)
    , MonitoringTimeDiffuSecs()
    , MonitoringAudioFrames()
    , ReaderSourceInfo()
    , ReaderInfoPresent(false)
    , ReaderInfoUpdated()
    , CaptureGrain(AUDIO_READER_DEFAULT_PERIOD_SPL)
    , CaptureGrainUpdated(true)
    , DelayMs(0)
    , LatencyMs(0)
{
    OS_InitializeMutex(&PlayStreamAttachMutex);
    OS_InitializeEvent(&ReaderThreadTerminated);
    OS_InitializeEvent(&ReaderInfoUpdated);

    strncpy(Alsaname, hw_name, sizeof(Alsaname));
    Alsaname[sizeof(Alsaname) - 1] = '\0';

    uint32_t minor = 0;
    uint32_t major = hw_name[3] - '0';

    if (6 == strlen(hw_name))
    {
        minor = hw_name[5] - '0';
    }

    // TODO(pht) move to a FinalizeInit method
    int32_t Result = ksnd_pcm_open(&SoundcardHandle, major, minor,
                                   SND_PCM_STREAM_CAPTURE);
    if (Result < 0)
    {
        SE_ERROR("Cannot Open ALSA Capture Device %d %d\n", hw_name[3], hw_name[5]);
        InitializationStatus = PlayerError;
        return;
    }

    Result = PcmSetParams();
    if (Result < 0)
    {
        SE_ERROR("Cannot initialize ALSA parameters\n");
        ksnd_pcm_close(SoundcardHandle);
        InitializationStatus = PlayerError;
        return;
    }

    StartReaderThread();
}

Audio_Reader_c::~Audio_Reader_c()
{
    TerminateReaderThread();  // shall ensure ksnd_pcm_close is called (if need be)

    OS_TerminateEvent(&ReaderThreadTerminated);
    OS_TerminateEvent(&ReaderInfoUpdated);
    OS_TerminateMutex(&PlayStreamAttachMutex);
}

PlayerStatus_t Audio_Reader_c::Attach(HavanaStream_c *play_stream)
{
    OS_LockMutex(&PlayStreamAttachMutex);
    PlayStream = play_stream;
    OS_UnLockMutex(&PlayStreamAttachMutex);
    return PlayerNoError;
}

PlayerStatus_t Audio_Reader_c::Detach(HavanaStream_c *play_stream)
{
    OS_LockMutex(&PlayStreamAttachMutex);

    if (PlayStream == play_stream)
    {
        PlayStream->Drain(true); // Drain before detach to flush old data
        PlayStream = NULL;
    }
    else
    {
        OS_UnLockMutex(&PlayStreamAttachMutex);
        SE_ERROR("play stream %x is NOT attahced to audio reader\n ", (int32_t)play_stream);
        return PlayerError;
    }

    OS_UnLockMutex(&PlayStreamAttachMutex);
    return PlayerNoError;
}

PlayerStatus_t Audio_Reader_c::SetCompoundOption(stm_se_ctrl_t ctrl, const void *value)
{
    stm_se_ctrl_audio_reader_source_info_t *ReaderInfo;

    switch (ctrl)
    {
    case STM_SE_CTRL_AUDIO_READER_SOURCE_INFO:
    {
        ReaderInfo = (stm_se_ctrl_audio_reader_source_info_t *) value;
        SE_DEBUG(group_audio_reader, "SOURCE_INFO: NbCh %d Sf %d ChanAlloc %d StreamType %d DownMixInhibit %d LevelShift %d LfeLevel %d\n",
                 ReaderInfo->channel_count,
                 ReaderInfo->sampling_frequency,
                 ReaderInfo->channel_alloc,
                 ReaderInfo->stream_type,
                 ReaderInfo->down_mix_inhibit,
                 ReaderInfo->level_shift_value,
                 ReaderInfo->lfe_playback_level);

        if (AUDIO_MAX_CHANNELS < ReaderInfo->channel_count)
        {
            SE_ERROR("%d channels not supported, max is %d\n",
                     ReaderInfo->channel_count,
                     AUDIO_MAX_CHANNELS);
            ReaderSourceInfo.channel_count = AUDIO_MAX_CHANNELS;
        }
        else
        {
            ReaderSourceInfo.channel_count = ReaderInfo->channel_count;
        }

        ReaderSourceInfo.sampling_frequency = ReaderInfo->sampling_frequency;
        ReaderSourceInfo.channel_alloc      = ReaderInfo->channel_alloc;
        ReaderSourceInfo.stream_type        = ReaderInfo->stream_type;
        ReaderSourceInfo.down_mix_inhibit   = ReaderInfo->down_mix_inhibit;
        ReaderSourceInfo.level_shift_value  = ReaderInfo->level_shift_value;
        ReaderSourceInfo.lfe_playback_level = ReaderInfo->lfe_playback_level;
        ReaderInfoPresent = true;
        OS_SetEvent(&ReaderInfoUpdated);
        break;
    }

    default:
        return PlayerError;
    }

    return PlayerNoError;
}

PlayerStatus_t Audio_Reader_c::GetCompoundOption(stm_se_ctrl_t ctrl, void *value)
{
    if (value == NULL)
    {
        SE_ERROR("NULL pointer passed\n");
        return PlayerError;
    }

    stm_se_ctrl_audio_reader_source_info_t *ReaderInfo;

    switch (ctrl)
    {
    case STM_SE_CTRL_AUDIO_READER_SOURCE_INFO:
    {
        ReaderInfo = (stm_se_ctrl_audio_reader_source_info_t *)value;
        ReaderInfo->channel_count      = ReaderSourceInfo.channel_count;
        ReaderInfo->sampling_frequency = ReaderSourceInfo.sampling_frequency;
        ReaderInfo->channel_alloc      = ReaderSourceInfo.channel_alloc;
        ReaderInfo->stream_type        = ReaderSourceInfo.stream_type;
        ReaderInfo->down_mix_inhibit   = ReaderSourceInfo.down_mix_inhibit;
        ReaderInfo->level_shift_value  = ReaderSourceInfo.level_shift_value;
        ReaderInfo->lfe_playback_level = ReaderSourceInfo.lfe_playback_level;

        break;
    }

    default:
        return PlayerError;
    }

    return PlayerNoError;
}

PlayerStatus_t Audio_Reader_c::SetOption(stm_se_ctrl_t ctrl, const int32_t value)
{
    switch (ctrl)
    {
    case STM_SE_CTRL_AUDIO_READER_GRAIN:
    {
        SE_DEBUG(group_audio_reader, "Set STM_SE_CTRL_AUDIO_READER_GRAIN to %d\n", value);
        if (value < 0)
        {
            return PlayerError;
        }
        else if ((uint32_t) value != CaptureGrain)
        {
            CaptureGrain        = (uint32_t) value;
            CaptureGrainUpdated = true;
        }
        break;
    }
    case STM_SE_CTRL_AUDIO_DELAY:
    {
        SE_DEBUG(group_audio_reader, "Set STM_SE_CTRL_AUDIO_DELAY to %d\n", value);
        DelayMs = value;
        break;
    }
    default:
    {
        SE_ERROR("Set unknown control %d to %d\n", (int) ctrl, value);
        return PlayerError;
    }
    }
    return PlayerNoError;
}

PlayerStatus_t Audio_Reader_c::GetOption(stm_se_ctrl_t ctrl, int32_t *value)
{
    if (value == NULL)
    {
        SE_ERROR("NULL pointer passed\n");
        return PlayerError;
    }

    switch (ctrl)
    {
    case STM_SE_CTRL_AUDIO_READER_GRAIN:
    {
        * value = CaptureGrain;
        SE_DEBUG(group_audio_reader, "Read %d for STM_SE_CTRL_AUDIO_READER_GRAIN\n", *value);
        break;
    }
    case STM_SE_CTRL_AUDIO_DELAY:
    {
        * value = DelayMs;
        SE_DEBUG(group_audio_reader, "Read %d for STM_SE_CTRL_AUDIO_DELAY\n", *value);
        break;
    }
    default:
    {
        SE_ERROR("Read unknown control %d\n", (int) ctrl);
        return PlayerError;
    }
    }
    return PlayerNoError;
}
