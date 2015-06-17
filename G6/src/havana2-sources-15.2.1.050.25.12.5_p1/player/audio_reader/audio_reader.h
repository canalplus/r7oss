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

#ifndef H_AUDIO_READER_CLASS
#define H_AUDIO_READER_CLASS

#include "pcmplayer.h"
#include "ksound.h"
#include "player.h"
#include "havana_stream.h"
#include "least_squares.h"
#include "audio_conversions.h"
#include "spdifin_audio.h"

#define AUDIO_READER_NAME_LENGTH        24

#define AUDIO_READER_SAMPLE_RATE_LOCK   4
#define AUDIO_DEFAULT_SAMPLE_RATE       48000
#define AUDIO_DEFAULT_CHANNELS          2
#define AUDIO_MAX_CHANNELS              8
#define AUDIO_SAMPLE_DEPTH              32

#define AUDIO_READER_DEFAULT_PERIOD_MS   10 // in ms
#define AUDIO_READER_DEFAULT_PERIOD_SPL 512 // in samples , based on 48k

#define AUDIO_READER_ROUND_GRAIN(x) (((x)+127) & 0xFF80) // Round to upper 128 samples grain


#define AUDIO_READER_NUM_OF_BUFFERS     2
#define AUDIO_READER_MAX_TIME_DIFF      150000
/* AUDIO_READER_MAX_TIME_DIFF represents the maximum possible valid timediff (
 * in usecs) between audio packets arriving from alsa driver.
 * The miximum valid time diff occurs when transitioning from 192k to 32k
 * ie. MAX_TIME_DIFF = (Maximum Period Size / Minimum Freq)
 *                   = 4096/32k = 128msec( rounded off to 150msec)
*/


#define PES_AUDIO_HEADER_SIZE           (32 + SPDIFIN_PRIVATE_HEADER_LENGTH)
#define PES_PRIVATE_STREAM1             0xbd
#define MAX_PES_PACKET_SIZE             65400
#define INVALID_PTS_VALUE               0x200000000ull
#define MID(x,y) ((x+y)/2)

typedef struct BitPacker_s
{
    uint8_t  *Ptr;/* write pointer */
    uint32_t BitBuffer;/* bitreader shifter */
    uint32_t Remaining;/* number of remaining in the shifter */
} BitPacker_t;


class Audio_Reader_c : public BaseComponentClass_c
{
public:
    char                       Alsaname[AUDIO_READER_NAME_LENGTH];

    Audio_Reader_c(const char *name, const char *hw_name);
    ~Audio_Reader_c(void);

    void                       ReaderThread();
    PlayerStatus_t             Attach(HavanaStream_c *play_stream);
    PlayerStatus_t             Detach(HavanaStream_c *play_stream);
    PlayerStatus_t             SetCompoundOption(stm_se_ctrl_t ctrl, const void *value);
    PlayerStatus_t             GetCompoundOption(stm_se_ctrl_t ctrl, void *value);
    PlayerStatus_t             SetOption(stm_se_ctrl_t ctrl, const int32_t value);
    PlayerStatus_t             GetOption(stm_se_ctrl_t ctrl, int32_t *value);

private:
    ksnd_pcm_t                *SoundcardHandle;
    uint32_t                   SampleRate;
    uint32_t                   LastSampleRate;
    int                        SampleRateChangeCount;
    uint32_t                   NumChannels;
    eCEA861_Sfrequency         ReaderDiscreteSampleRate;
    snd_pcm_uframes_t          AudioPeriodFrames;
    snd_pcm_uframes_t          AudioBufferFrames;
    snd_pcm_uframes_t          AudioFramesRem;
    class HavanaStream_c      *PlayStream;
    OS_Mutex_t                 PlayStreamAttachMutex;

    bool                       ReaderThreadRunning;
    OS_Event_t                 ReaderThreadTerminated;
    unsigned long long         PrevTimeStampuSecs;
    snd_pcm_uframes_t          PrevSamplesAvailable;


    /* SampleRate monitoring */
    int                        MonitoringIndex;
    unsigned long long         MonitoringTimeWindow;
    unsigned long long         MonitoringAudioFramesWindow;
    unsigned long long         MonitoringTimeDiffuSecs[AUDIO_READER_SAMPLE_RATE_LOCK];
    unsigned long long         MonitoringAudioFrames[AUDIO_READER_SAMPLE_RATE_LOCK];

    /*STM_SE_CTRL_AUDIO_READER_SOURCE_INFO*/
    stm_se_ctrl_audio_reader_source_info_t ReaderSourceInfo;
    bool                       ReaderInfoPresent;
    OS_Event_t                 ReaderInfoUpdated;

    uint32_t                   CaptureGrain;
    bool                       CaptureGrainUpdated;
    int                        DelayMs;
    int                        LatencyMs;

    inline uint32_t AdjustedGrain(void)
    {
        uint32_t Sfreq = (ReaderDiscreteSampleRate == CEA861_0k) ?
                         AUDIO_DEFAULT_SAMPLE_RATE :
                         SampleRate;

        return AUDIO_READER_ROUND_GRAIN(CaptureGrain * Sfreq / 48000);
    }

    void                       ProcessPacket(const snd_pcm_channel_area_t *CaptureAreas,
                                             snd_pcm_uframes_t CaptureOffset,
                                             snd_pcm_uframes_t CaptureFrames,
                                             unsigned long long TimeStampuSecs);

    unsigned long long         ProcessTiming(snd_pcm_uframes_t AudioPeriodFrames);

    int32_t                    InsertPesHeader(uint8_t *data, int32_t size,
                                               uint8_t stream_id,
                                               unsigned long long int pts,
                                               int32_t pic_start_code);

    int32_t                    InsertPrivateDataHeader(uint8_t *data,
                                                       int32_t payload_size,
                                                       int32_t sampling_frequency);

    unsigned char             CalculateSamplingFreq(snd_pcm_uframes_t AudioFrames,
                                                    signed long long TimeDiff,
                                                    int32_t SampleDiff);

    void                       CalculateReaderBufferSize();

    int32_t                    PcmGetTime(snd_pcm_uframes_t *SamplesAvailable, unsigned long long *TimeStampUs);
    int32_t                    PcmErrorRecovery();
    int32_t                    PcmSetParams();
    int32_t                    PcmStart();
    int32_t                    PcmRestart();
    int32_t                    PcmWait(snd_pcm_uframes_t  *CaptureFrames);
    void                       PutBits(BitPacker_t *ld,
                                       uint32_t code,
                                       uint32_t length);

    void                       FlushBits(BitPacker_t *ld);

    PlayerStatus_t             StartReaderThread();
    void                       TerminateReaderThread();

    DISALLOW_COPY_AND_ASSIGN(Audio_Reader_c);
};

#endif // H_AUDIO_READER_CLASS
