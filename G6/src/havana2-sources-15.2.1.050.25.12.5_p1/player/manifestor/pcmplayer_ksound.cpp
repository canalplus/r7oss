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

#include "st_relayfs_se.h"
#include "pcmplayer_ksound.h"

#undef TRACE_TAG
#define TRACE_TAG   "PcmPlayer_Ksound_c"

extern "C" int snprintf(char *buf, long size, const char *fmt, ...) __attribute__((format(printf, 3, 4))) ;

#define MAX_TIME_TO_GET_AUDIO_DELAY (500ull)

#define FSYNTH_CONTROL_NAME "PCM Playback Oversampling Freq. Adjustment"
#define FSYNTH_CONTROL_UNITY 0

static void UpdateIec958SamplingFrequency(unsigned int SampleRateHz, struct snd_aes_iec958 *Status)
{
    //
    // Strip out of the original sampling frequency
    //
    Status->status[3] &= ~0x0f;

    //
    // Drop in a new one
    //

    switch (SampleRateHz)
    {
        // macro to let us express table entries in the same form as the tables in the standard...
#define C(b0, b1, b2, b3, sfreq) case sfreq: Status->status[3] |= b0<<0 | b1<<1 | b2<<2 | b3<<3; break
        C(0, 0, 1, 0,   22050);
        C(0, 0, 0, 0,   44100);
        C(0, 0, 0, 1,   88200);
        C(0, 0, 1, 1,  176400);
        C(0, 1, 1, 0,   24000);
        C(0, 1, 0, 0,   48000);
        C(0, 1, 0, 1,   96000);
        C(0, 1, 1, 1,  192000);
        C(1, 1, 0, 0,   32000);
        C(1, 0, 0, 1,  768000);

    default:
        Status->status[3] |= 1;
#undef C
    }
}


////////////////////////////////////////////////////////////////////////////
///
///
///
PcmPlayer_Ksound_c::PcmPlayer_Ksound_c(unsigned int Soundcard, unsigned int Substream, unsigned int dev_num, unsigned int SPDIFmode)
    : PcmPlayer_c()
    , SoundcardHandle(NULL)
    , SoundcardMappedBuffer(NULL)
    , SoundcardMappedOffset(0)
    , SoundcardMappedSamples(0)
    , TimeOfNextCommit(0)
    , BufferType(ST_RELAY_TYPE_DATA_TO_PCM0 + dev_num)
    , id()
    , control()
    , elem(NULL)
    , fsynth_adjust(10000000)  // illegal value (represents 10x speedup)
    , iec958_id()
    , iec958_control()
    , iec958_elem(NULL)
    , iec958_val_id()
    , iec958_val_control()
    , iec958_val_elem(NULL)
    , SPDIFmode(SPDIFmode)
    , ResamplingFactor_x32(Q11_5_UNITY)
    , Identity()
{
    snprintf(Identity, sizeof(Identity), "hw:%d,%d", Soundcard, Substream);
    Identity[sizeof(Identity) - 1] = '\0';

    // TODO(pht) move to a FinalizeInit method
    int Result = ksnd_pcm_open(&SoundcardHandle, Soundcard, Substream, SND_PCM_STREAM_PLAYBACK);
    if (0 != Result)
    {
        SE_ERROR("Cannot open ALSA device %s\n", Identity);
        InitializationStatus = PlayerError;
        return;
    }

    /*
     * set up access to fsynth control
     */
    ksnd_ctl_elem_id_set_interface(&id, SNDRV_CTL_ELEM_IFACE_PCM);
    ksnd_ctl_elem_id_set_device(&id, Substream);
    ksnd_ctl_elem_id_set_name(&id, FSYNTH_CONTROL_NAME);

    // search for a suitable index ('randomly' assigned by sound driver)
    int Index;
    for (Index = 0; Index < 16; Index++)
    {
        ksnd_ctl_elem_id_set_index(&id, Index);
        elem = ksnd_substream_find_elem(SoundcardHandle->substream, &id);

        if (elem)
        {
            ksnd_ctl_elem_value_set_id(&control, &id);
            break;
        }
    }

    if (!elem)
    {
        SE_ERROR("ALSA device %s has no frequency tuning control\n", Identity);
    }

    /*
     * set up access to the iec bits (if available)
     */
    // get all the properties from the previous id
    memcpy(&iec958_id, &id, sizeof(iec958_id));
    ksnd_ctl_elem_id_set_interface(&iec958_id, SNDRV_CTL_ELEM_IFACE_PCM); //SND_CTL_ELEM_IFACE_MIXER);
    ksnd_ctl_elem_id_set_name(&iec958_id, "IEC958 Playback Default");
    ksnd_ctl_elem_id_set_device(&iec958_id, Substream);

    // search for a suitable index ('randomly' assigned by sound driver)
    for (Index = 0; Index < 16; Index++)
    {
        ksnd_ctl_elem_id_set_index(&iec958_id, Index);
        iec958_elem = ksnd_substream_find_elem(SoundcardHandle->substream, &iec958_id);

        if (iec958_elem)
        {
            ksnd_ctl_elem_value_set_id(&iec958_control, &iec958_id);
            break;
        }
    }

    SE_DEBUG(group_audio_player, "ALSA device (hw:%d,%d) %s support IEC60958 channel status control\n",
             Soundcard, Substream, (iec958_elem ? "does" : "doesn't"));
    /*
     * set up access to the validity bits (if available)
     */
    // get all the properties from the previous id
    memcpy(&iec958_val_id, &id, sizeof(iec958_val_id));
    ksnd_ctl_elem_id_set_interface(&iec958_val_id, SNDRV_CTL_ELEM_IFACE_PCM); //SND_CTL_ELEM_IFACE_MIXER);
    ksnd_ctl_elem_id_set_name(&iec958_val_id, "IEC958 Encoded Data Playback Default");
    ksnd_ctl_elem_id_set_device(&iec958_val_id, Substream);

    // search for a suitable index ('randomly' assigned by sound driver)
    for (Index = 0; Index < 16; Index++)
    {
        ksnd_ctl_elem_id_set_index(&iec958_val_id, Index);
        iec958_val_elem = ksnd_substream_find_elem(SoundcardHandle->substream, &iec958_val_id);

        if (iec958_val_elem)
        {
            ksnd_ctl_elem_value_set_id(&iec958_val_control, &iec958_val_id);
            break;
        }
    }

    SE_DEBUG(group_audio_player, "ALSA device (hw:%d,%d) %s support IEC60958 validity bits control\n",
             Soundcard, Substream, (iec958_val_elem ? "does" : "doesn't"));
}


////////////////////////////////////////////////////////////////////////////
///
///
///
PcmPlayer_Ksound_c::~PcmPlayer_Ksound_c()
{
    if (SoundcardHandle)
    {
        ksnd_pcm_close(SoundcardHandle);
    }
}


////////////////////////////////////////////////////////////////////////////
///
///
///
PlayerStatus_t PcmPlayer_Ksound_c::MapSamples(unsigned int SampleCount, bool NonBlock,
                                              void **MappedSamplesPP)
{
    snd_pcm_uframes_t Avail;
    int Result;
    int timeout_in_ms = -1; // -1 fallback value => 10 seconds in ksound
    unsigned long long time0 = 0;

    // Adjust the sample count to switch between the nominal and actual sampling frequency
    SampleCount = (SampleCount * ResamplingFactor_x32) / Q11_5_UNITY;

    while ((Avail = ksnd_pcm_avail_update(SoundcardHandle)) < SampleCount)
    {
        if (NonBlock)
        {
            SE_ERROR("Insufficient samples available for mapping (%s). Wanted %u Found %lu\n",
                     Identity, SampleCount, Avail);
            return PlayerError;
        }

        // Calling ksnd_pcm_wait should return as soon as avail_min samples are available
        // As ksound set avail_min to one period, ideally ksnd_pcm_wait duration should not exceed one period.
        // But in the low-latency case when the PeriodSize is 512 samples ksnd_pcm_wait is exceeding 1 period
        // time. So allow the worst case wait (NumPeriods * 1 period duration).
        // Please note that timeout value is specified in ms, but it is converted internally in jiffies
        // and today jiffies granularity is 10msec/jiffies!
        // Consequently we set the ksnd_pcm_wait timeout value to NumPeriods * period duration
        if (SurfaceParameters.ActualSampleRateHz != 0 && SurfaceParameters.PeriodSize != 0)
        {
            timeout_in_ms = (SurfaceParameters.PeriodSize * 1000 * SurfaceParameters.NumPeriods) / SurfaceParameters.ActualSampleRateHz;
        }
        // Due to jiffies granularity (10msec/jiffies),
        // clamp timeout at 10msec to avoid requesting a 0 jiffie timeout
        if (timeout_in_ms >= 0 && timeout_in_ms < 10)
        {
            timeout_in_ms = 10;
        }

        if (SE_IS_VERBOSE_ON(group_audio_player))
        {
            time0 = OS_GetTimeInMicroSeconds();
        }

        Result = ksnd_pcm_wait(SoundcardHandle, timeout_in_ms);

        if (SE_IS_VERBOSE_ON(group_audio_player))
        {
            SE_VERBOSE(group_audio_player, "[%s] ksnd_pcm_wait duration: % 6lldus timeout:%dms (now:%lld)",
                       Identity, OS_GetTimeInMicroSeconds() - time0, timeout_in_ms, OS_GetTimeInMicroSeconds());
        }
        if (Result <= 0)
        {
            SE_ERROR("Underrun before waiting for period expiry (%d) for %s\n",
                     -Result, Identity);
            return PlayerError;
        }
    }

    SoundcardMappedSamples = SampleCount;
    Result = ksnd_pcm_mmap_begin(SoundcardHandle, &SoundcardMappedBuffer,
                                 &SoundcardMappedOffset, &SoundcardMappedSamples);

    if (Result < 0)
    {
        SE_ERROR("Underrun before mapping buffer (%d) for %s\n", -Result, Identity);
        return PlayerError;
    }

    // TODO: fail it SoundcardMappedSamples != SampleCount...
    unsigned char *FirstMappedSample =
        ((unsigned char *)(SoundcardMappedBuffer->addr) +
         (SoundcardMappedBuffer->first / 8) +
         (SoundcardMappedOffset * (SoundcardMappedBuffer->step / 8)));
    *MappedSamplesPP = (void *) FirstMappedSample;
    return PlayerNoError;
}


////////////////////////////////////////////////////////////////////////////
///
///
///
PlayerStatus_t PcmPlayer_Ksound_c::CommitMappedSamples()
{
    int Result;
    unsigned char *FirstMappedSample =
        ((unsigned char *)(SoundcardMappedBuffer->addr) +
         (SoundcardMappedBuffer->first / 8) +
         (SoundcardMappedOffset * (SoundcardMappedBuffer->step / 8)));
    st_relayfs_write_se(BufferType, ST_RELAY_SOURCE_SE,
                        FirstMappedSample,
                        (unsigned int) SamplesToBytes((SoundcardMappedSamples * Q11_5_UNITY) / ResamplingFactor_x32),
                        0);
    Result = ksnd_pcm_mmap_commit(SoundcardHandle, SoundcardMappedOffset, SoundcardMappedSamples);

    if (Result < 0 || (snd_pcm_uframes_t) Result != SoundcardMappedSamples)
    {
        SE_ERROR("Underrun before commit (%d) for %s\n", -Result, Identity);
        return PlayerError;
    }

    // no samples are now mapped
    SoundcardMappedSamples = 0;
    TimeOfNextCommit = 0;
    return PlayerNoError;
}


////////////////////////////////////////////////////////////////////////////
///
/// Obtain the time at which the first sample of the next commit will be displayed.
///
/// \param TimeP Pointer to be filled with the a time relative to ::OS_GetTimeInMicroseconds().
///
PlayerStatus_t PcmPlayer_Ksound_c::GetTimeOfNextCommit(unsigned long long *TimeP)
{
    int Result;

    // lazily recalculate the time at which the first
    if (!TimeOfNextCommit)
    {
        unsigned int SampleRate = SurfaceParameters.ActualSampleRateHz;
        unsigned int BufferSize = (SurfaceParameters.PeriodSize * SurfaceParameters.NumPeriods);

        if (0 == SampleRate)
        {
            SE_INFO(group_audio_player, "SampleRate 0; forcing default\n");
            SampleRate = 48000; // default
        }

        struct timespec TimeStampAsTimeSpec;

        unsigned long long TimeStamp;

        snd_pcm_uframes_t SamplesAvailable;

        snd_pcm_sframes_t  DelayInSamples;

        unsigned long long DelayInMicroSeconds;

        Result = ksnd_pcm_mtimestamp(SoundcardHandle, &SamplesAvailable, &TimeStampAsTimeSpec);

        if (0 == Result)
        {
            TimeStamp = (TimeStampAsTimeSpec.tv_sec * 1000000ll) + (TimeStampAsTimeSpec.tv_nsec / 1000);
        }
        else
        {
#if 0
            SE_ERROR("Cannot read the sound card timestamp (%d) for %s\n",
                     -Result, Identity);
#endif
            // error recovery...
            TimeStamp = OS_GetTimeInMicroSeconds();
            SamplesAvailable = 0;
        }

        BufferSize = (SurfaceParameters.PeriodSize * SurfaceParameters.NumPeriods);
        DelayInSamples = BufferSize - SamplesAvailable;
        DelayInMicroSeconds = (DelayInSamples * 1000000ull) / SampleRate;
        TimeOfNextCommit = TimeStamp + DelayInMicroSeconds;
    }

    *TimeP = TimeOfNextCommit;
    return PlayerNoError;
}


////////////////////////////////////////////////////////////////////////////
///
/// Configure the soundcard's surface parameters.
///
/// \todo Contains an explicit hack to constrain the sample rate to >32000.
///
PlayerStatus_t PcmPlayer_Ksound_c::SetParameters(PcmPlayerSurfaceParameters_t *ParamsP)
{
    int Result;
    snd_pcm_uframes_t BufferSize;
    snd_pcm_uframes_t PeriodSize;
    SurfaceParameters = *ParamsP;

    if (SoundcardMappedSamples)
    {
        // this is not a 'hard' error hence this message is neither _TRACEd nor _ERRORed.
        // our caller knows that if we return an error it has to wait until the samples are unmapped...
        SE_DEBUG(group_audio_player, "Not safe to update soundcard parameters for %s at this point\n", Identity);
        return PlayerError;
    }

    // calculate the resampling factor
    ResamplingFactor_x32 = (SurfaceParameters.ActualSampleRateHz * Q11_5_UNITY) /
                           SurfaceParameters.PeriodParameters.SampleRateHz;

    // being connected to a FatPipe output has the same affect as a 4x downsample on the mappings and
    // the period
    if (SPDIFmode == OUTPUT_FATPIPE)
    {
        SurfaceParameters.ActualSampleRateHz *= 4;
        ResamplingFactor_x32 *= 4;
        //SurfaceParameters.PeriodParameters.ChannelCount /= 4;
    }

    SurfaceParameters.PeriodSize = (SurfaceParameters.PeriodSize * ResamplingFactor_x32) / Q11_5_UNITY;
    Result = ksnd_pcm_set_params(SoundcardHandle,
                                 SurfaceParameters.PeriodParameters.ChannelCount,
                                 SurfaceParameters.PeriodParameters.BitsPerSample,
                                 SurfaceParameters.ActualSampleRateHz,
                                 SurfaceParameters.PeriodSize,
                                 (SurfaceParameters.PeriodSize * SurfaceParameters.NumPeriods));

    if (0 != Result)
    {
        SE_ERROR("Cannot set ALSA parameters for %s\n", Identity);
        SE_ERROR("Channel Count - %d\n", SurfaceParameters.PeriodParameters.ChannelCount);
        SE_ERROR("BitsPerSample - %d\n", SurfaceParameters.PeriodParameters.BitsPerSample);
        SE_ERROR("SampleRateHz  - %d\n", SurfaceParameters.ActualSampleRateHz);
        return PlayerError;
    }

    Result = ksnd_pcm_get_params(SoundcardHandle, &BufferSize, &PeriodSize);

    if (0 != Result)
    {
        SE_ERROR("Cannot read back ALSA parameters for %s\n", Identity);
        return PlayerError;
    }

    if (0 != (BufferSize % PeriodSize))
    {
        SE_ERROR("BufferSize is not a multiple of the PeriodSize for %s\n", Identity);
        return PlayerError;
    }

    // TODO: ensure we specify the period rather than adapt to it...
    SurfaceParameters.PeriodParameters.BitsPerSample = 32;
    SurfaceParameters.PeriodSize = PeriodSize;
    SurfaceParameters.NumPeriods = BufferSize / PeriodSize;
    SE_DEBUG(group_audio_player, "%s: SampleRate %d  PeriodSize %ld (%ldms)  BufferSize %ld (%ldms)  NumPeriods %d\n",
             Identity, SurfaceParameters.ActualSampleRateHz,
             PeriodSize, PeriodSize * 1000 / SurfaceParameters.ActualSampleRateHz,
             BufferSize, BufferSize * 1000 / SurfaceParameters.ActualSampleRateHz,
             SurfaceParameters.NumPeriods);
    *ParamsP = SurfaceParameters;

    // ensure the surface parameters reflect the nominal behavior rather than the resampled/fatpipe
    // behavior (but we must ensure our own copy of the surface parameters accurately reflects the
    // surface to ensure that calculations such as GetTimeOfNextCommit() operate on the correct values)
    if (SPDIFmode == OUTPUT_FATPIPE)
    {
        ParamsP->PeriodParameters.ChannelCount *= 4;
        ParamsP->ActualSampleRateHz /= 4;
    }

    ParamsP->PeriodSize = (SurfaceParameters.PeriodSize * Q11_5_UNITY) / ResamplingFactor_x32;
    return PlayerNoError;
}


////////////////////////////////////////////////////////////////////////////
///
///
///
PlayerStatus_t PcmPlayer_Ksound_c::SetOutputRateAdjustment(int Rate, int *ActualRateP)
{
    int adjust;
    int res;

    if (elem)
    {
        adjust  = Rate - (1000000 - FSYNTH_CONTROL_UNITY);

        if (adjust != fsynth_adjust)
        {
            SE_DEBUG(group_audio_player, "Setting output rate adjustment to %d parts per million for %s\n",
                     adjust - FSYNTH_CONTROL_UNITY, Identity);
            ksnd_ctl_elem_value_set_integer(&control, 0, adjust);
            res = ksnd_hctl_elem_write(elem, &control);

            if (res < 0)
                SE_ERROR("Cannot set output rate adjustment (%d) for %s\n",
                         res, Identity);

            fsynth_adjust = adjust;
        }
    }

    *ActualRateP = Rate;
    return PlayerNoError;
}


////////////////////////////////////////////////////////////////////////////
///
/// Update the IEC60958 status bits of the inferior sound card.
///
PlayerStatus_t PcmPlayer_Ksound_c::SetIec60958StatusBits(struct snd_pseudo_mixer_aes_iec958 *Iec958Status)
{
    struct snd_aes_iec958 Status = *((struct snd_aes_iec958 *) Iec958Status);

    if (iec958_elem)
    {
        int res;
        UpdateIec958SamplingFrequency(SurfaceParameters.ActualSampleRateHz, &Status);
        //
        // Issue the update
        //
        SE_DEBUG(group_audio_player, "Status %02x %02x %02x %02x %02x for %s\n",
                 Status.status[0], Status.status[1], Status.status[2],
                 Status.status[3], Status.status[4], Identity);
        ksnd_ctl_elem_value_set_iec958(&iec958_control, &Status);
        res = ksnd_hctl_elem_write(iec958_elem, &iec958_control);

        if (res < 0)
        {
            SE_ERROR("Cannot set output rate adjustment (%d) for %s\n", res, Identity);
            return PlayerError;
        }

        return PlayerNoError;
    }

    SE_DEBUG(group_audio_player, "Skipped IEC60958 status update (not support for %s)\n", Identity);
    return PlayerNoError;
}


////////////////////////////////////////////////////////////////////////////
///
/// Update the IEC61937 status bits of the inferior sound card.
///
PlayerStatus_t PcmPlayer_Ksound_c::SetIec61937StatusBits(struct snd_pseudo_mixer_aes_iec958 *Iec958Status)
{
    struct snd_aes_iec958 Status = *((struct snd_aes_iec958 *) Iec958Status);

    if (iec958_elem)
    {
        int res;
        UpdateIec958SamplingFrequency(SurfaceParameters.ActualSampleRateHz, &Status);
        //
        // override the channel status necessary bits for IEC61937 mode
        //
        Status.status[0] &= ~0x30;  // IEC958_AES0_CON_MODE in <asoundef.h>, mode 0
        Status.status[0] |= (1 << 1); // IEC958_AES0_NONAUDIO in <asoundef.h>, channel status bit 1
        Status.status[4] = (1 << 1); // IEC958_AES4_CON_WORDLEN_20_16, 16-bits
        //
        // Issue the update
        //
        SE_DEBUG(group_audio_player, "Status %02x %02x %02x %02x %02x for %s\n",
                 Status.status[0], Status.status[1], Status.status[2],
                 Status.status[3], Status.status[4], Identity);
        ksnd_ctl_elem_value_set_iec958(&iec958_control, &Status);
        res = ksnd_hctl_elem_write(iec958_elem, &iec958_control);

        if (res < 0)
        {
            SE_ERROR("Cannot set channel status bits  (%d) for %s\n", res, Identity);
            return PlayerError;
        }
    }
    else
    {
        SE_DEBUG(group_audio_player, "Skipped IEC60958 status update (not support for %s)\n", Identity);
    }

    return PlayerNoError;
}

////////////////////////////////////////////////////////////////////////////
///
/// Update the IEC60958 validity bits of the inferior sound card.
///
PlayerStatus_t PcmPlayer_Ksound_c::SetIec60958ValidityBits(enum eIecValidity Validity)
{
    if (iec958_val_elem)
    {
        int res;
        // set validity bit to 1 (compressed mode)
        ksnd_ctl_elem_value_set_integer(&iec958_val_control, 0, (int)Validity);
        res = ksnd_hctl_elem_write(iec958_val_elem, &iec958_val_control);

        if (res < 0)
        {
            SE_ERROR("Cannot set validity bits to %d (%d) for %s\n", (int) Validity, res, Identity);
            return PlayerError;
        }
    }
    else
    {
        SE_DEBUG(group_audio_player, "Skipped IEC60958 validity update (not supported for %s)\n", Identity);
    }

    return PlayerNoError;
}


////////////////////////////////////////////////////////////////////////////
///
/// Calculate the length in bytes of a number of samples.
///
unsigned int PcmPlayer_Ksound_c::SamplesToBytes(unsigned int SampleCount)
{
    // Adjust the sample count to switch between the nominal and actual sampling frequency
    SampleCount = (SampleCount * ResamplingFactor_x32) / Q11_5_UNITY;
    return SampleCount * SurfaceParameters.PeriodParameters.ChannelCount *
           (SurfaceParameters.PeriodParameters.BitsPerSample / 8);
}

////////////////////////////////////////////////////////////////////////////
///
/// Calculate the length in samples of a number of bytes.
///
unsigned int PcmPlayer_Ksound_c::BytesToSamples(unsigned int ByteCount)
{
    // Adjust the sample count to switch between the nominal and actual sampling frequency
    ByteCount = (ByteCount * Q11_5_UNITY) / ResamplingFactor_x32;
    return (ByteCount * 8) / (SurfaceParameters.PeriodParameters.ChannelCount *
                              SurfaceParameters.PeriodParameters.BitsPerSample);
}

////////////////////////////////////////////////////////////////////////////
///
/// Take a PCM player that has reported an error and try to get it running again.
///
PlayerStatus_t PcmPlayer_Ksound_c::DeployUnderrunRecovery()
{
    int Result;
    // Reset the card
    SoundcardMappedSamples = 0;
    TimeOfNextCommit = 0;
    Result = ksnd_pcm_prepare(SoundcardHandle);

    if (Result < 0)
    {
        // failed upon first attempt : try a full restart
        SE_ERROR("Can't prepare (%d) for %s so try to restart\n", -Result, Identity);
        Result = ksnd_pcm_stop(SoundcardHandle);

        if (0 != Result)
        {
            SE_ERROR("Can't stop (%d) for %s while trying to restart\n", -Result, Identity);
            return PlayerError;
        }

        Result = ksnd_pcm_set_params(SoundcardHandle,
                                     SurfaceParameters.PeriodParameters.ChannelCount,
                                     SurfaceParameters.PeriodParameters.BitsPerSample,
                                     SurfaceParameters.ActualSampleRateHz,
                                     SurfaceParameters.PeriodSize,
                                     (SurfaceParameters.PeriodSize * SurfaceParameters.NumPeriods));

        if (0 != Result)
        {
            SE_ERROR("Cannot set ALSA parameters for %s\n", Identity);
            SE_ERROR("Channel Count - %d\n", SurfaceParameters.PeriodParameters.ChannelCount);
            SE_ERROR("BitsPerSample - %d\n", SurfaceParameters.PeriodParameters.BitsPerSample);
            SE_ERROR("SampleRateHz  - %d\n", SurfaceParameters.ActualSampleRateHz);
            return PlayerError;
        }

        Result = ksnd_pcm_prepare(SoundcardHandle);

        if (Result < 0)
        {
            // failed upon first attempt : try a full restart
            SE_ERROR("Can't recover from underrun (%d) for %s\n", -Result, Identity);
            return PlayerError;
        }
    }

    return PlayerNoError;
}
