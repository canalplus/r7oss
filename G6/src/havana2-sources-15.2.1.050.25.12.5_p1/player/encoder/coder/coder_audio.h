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
#ifndef CODER_AUDIO_H
#define CODER_AUDIO_H

#include "coder_base.h"
#include "audio_conversions.h"

/* Compilation flags to allow or not to auto using Greater or Less than bit rate
 * in case bit-rate set in control is not supported */
#define CODER_AUDIO_AUTO_BITRATE_OFF 0 //If bit-rate is not found, do not try to use neighbor
#define CODER_AUDIO_AUTO_BITRATE_LTE 1 //If bit-rate is not found, try to use nearest neighbor by default
#define CODER_AUDIO_AUTO_BITRATE_GTE 2 //If bit-rate is not found, try to use nearest neighbor by excess
#define CODER_AUDIO_AUTO_BITRATE_FLAG CODER_AUDIO_AUTO_BITRATE_OFF  //Which mode to use

#define CODER_AUDIO_BPS_PER_KBPS      1000

#define CODER_AUDIO_DEFAULT_ENCODING_BITRATE        192000
#define CODER_AUDIO_DEFAULT_ENCODING_CHANNEL_ALLOC  8 // If you make this more than 8, take care of the corresponding code

#define CODER_AUDIO_UPWARD_DIV(x,y)    (((x)+((y)-1))/(y))

//!< for default {freq, mode, bitrate} table
typedef struct CoderAudioDefaultBitRate_s
{
    unsigned int    SampleRate;
    enum eAccAcMode AudioCodingMode;
    unsigned int    DefaultBitRate;
} CoderAudioDefaultBitRate_t;

//!< static codec specific configuration , independent of (mme) implementation
typedef struct CoderAudioStaticConfiguration_s
{
    stm_se_encode_stream_encoding_t Encoding;
    unsigned int MaxNbChannels;              //!< Max width of buffer than can be processed
    unsigned int MaxNbSamplesPerInput;       //!< Max nb of pcm samples per chan that can be processed
    unsigned int MaxNbSamplesPerCodedFrame;  //!< Max nb of samples in an es frame
    unsigned int MinNbSamplesPerCodedFrame;  //!< Min nb of samples in an es frame
    unsigned int MaxCodedFrameSize;          //!< Max ES frame size (at any bitrate, Fs, N Chan, ...)
} CoderAudioStaticConfiguration_t;

//!< base audio metadata
typedef struct CoderBaseAudioMetadata_s
{
    uint32_t sample_rate;
    uint8_t  channel_count;
    uint8_t  chan[CODER_AUDIO_DEFAULT_ENCODING_CHANNEL_ALLOC];
} CoderBaseAudioMetadata_t;

//!< coder_audio controls
typedef struct CoderAudioControls_s
{
    stm_se_audio_bitrate_control_t BitRateCtrl; //!< bit rate control structure
    bool BitRateUpdated;       //!< Set if user update the bitrate or if default bitrate updated depending upon sample rate and channel placement
    int  TargetLoudness;       //!< @todo Usage to be precised
    bool CrcOn;                //!< Whether CRC is computed and put in stream (mpeg)
    bool StreamIsCopyrighted;  //!< SCMS: whether stream has copyright info
    bool OneCopyIsAuthorized;  //!< SCMS: if stream is copyrighted whether a serial copy can be made.
} CoderAudioControls_t;

struct CoderAudioEncodingParameters_s
{
    stm_se_uncompressed_frame_metadata_t    InputMetadata;
    __stm_se_encode_coordinator_metadata_t  EncodeCoordinatorMetadata;
    CoderAudioControls_t                    Controls;

    // ctor for non trivial init
    CoderAudioEncodingParameters_s(const CoderBaseAudioMetadata_t &baseaudiometadata,
                                   const CoderAudioControls_t &basecontrols)
        : InputMetadata()
        , EncodeCoordinatorMetadata()
        , Controls(basecontrols)
    {
        InputMetadata.audio.core_format.sample_rate = baseaudiometadata.sample_rate;
        InputMetadata.audio.core_format.channel_placement.channel_count = baseaudiometadata.channel_count;

        /* even if buffer is larger that what is handled by defaults, extra chans will be muted */
        for (int i = 0; i < lengthof(InputMetadata.audio.core_format.channel_placement.chan); i++)
        {
            if (i < CODER_AUDIO_DEFAULT_ENCODING_CHANNEL_ALLOC)
            {
                InputMetadata.audio.core_format.channel_placement.chan[i] = (uint8_t)baseaudiometadata.chan[i];
            }
            else
            {
                InputMetadata.audio.core_format.channel_placement.chan[i] = (uint8_t)STM_SE_AUDIO_CHAN_STUFFING;
            }
        }

        InputMetadata.audio.sample_format = STM_SE_AUDIO_PCM_FMT_S32LE;
    }
};
typedef struct CoderAudioEncodingParameters_s CoderAudioEncodingParameters_t;

//!< Stores current Input() parameters
struct CoderAudioCurrentParameters_s
{
    CoderAudioControls_t                  Controls;
    stm_se_uncompressed_frame_metadata_t *InputMetadata;
    __stm_se_encode_coordinator_metadata_t *EncodeCoordinatorMetadata;

    // ctor for non trivial init
    explicit CoderAudioCurrentParameters_s(const CoderAudioControls_t &basecontrols)
        : Controls(basecontrols), InputMetadata(NULL), EncodeCoordinatorMetadata(NULL) {}
};
typedef struct CoderAudioCurrentParameters_s CoderAudioCurrentParameters_t;

//!< Coder Audio statistics
typedef struct CoderAudioStatistics_s
{
    unsigned int InputFrames;      //!< Number of time Input() has been called
    unsigned int InputBytes;       //!< Number of bytes injected
    unsigned int InputFramesOk;    //!< Number of time Input() successfully when through
    unsigned int OutputFrames;     //!< Number of frames properly sent to output
    unsigned int OutputBytes;      //!< Number of bytes properly sent to outout
} CoderAudioStatistics_t;

/////

class Coder_Audio_c : public Coder_Base_c
{
public:
    Coder_Audio_c(const CoderAudioStaticConfiguration_t &staticconfigs,
                  const CoderBaseAudioMetadata_t &baseaudiometadata,
                  const CoderAudioControls_t &basecontrols);
    virtual ~Coder_Audio_c(void);

    CoderStatus_t   Halt(void);

    virtual CoderStatus_t   Input(Buffer_t  Buffer);
    virtual CoderStatus_t   SetControl(stm_se_ctrl_t    Control, const void *Data);
    virtual CoderStatus_t   GetControl(stm_se_ctrl_t    Control,       void *Data);
    virtual CoderStatus_t   SetCompoundControl(stm_se_ctrl_t    Control, const void *Data);
    virtual CoderStatus_t   GetCompoundControl(stm_se_ctrl_t    Control,       void *Data);

protected:
    const CoderAudioStaticConfiguration_t mStaticConfiguration;
    CoderAudioStatistics_t                mCoderAudioStatistics;

    const CoderAudioEncodingParameters_t &GetConstRefPrevEncodingParameters(void) const { return mPreviousEncodingParameters; }
    const CoderAudioCurrentParameters_t  &GetConstRefCurEncodingParameters(void)  const { return mCurrentEncodingParameters; }

    // populate fake current from previous
    void UpdateFakeBufCurrentFromPreviousParameters(void)
    {
        // Because Fake buffer does not have proper audio metadata we need to populate
        mCurrentEncodingParameters.InputMetadata->audio = mPreviousEncodingParameters.InputMetadata.audio;
        // Fake buffer timestamp needs to be be invalidated.
        TimeStamp_c InvalidTimeStamp; //Default value: invalid
        mCurrentEncodingParameters.InputMetadata->native_time = InvalidTimeStamp.Value(mPreviousEncodingParameters.InputMetadata.native_time_format);
        mCurrentEncodingParameters.InputMetadata->native_time_format = mPreviousEncodingParameters.InputMetadata.native_time_format;
        mCurrentEncodingParameters.EncodeCoordinatorMetadata->encoded_time = InvalidTimeStamp.Value(mPreviousEncodingParameters.EncodeCoordinatorMetadata.encoded_time_format);
        mCurrentEncodingParameters.EncodeCoordinatorMetadata->encoded_time_format = mPreviousEncodingParameters.EncodeCoordinatorMetadata.encoded_time_format;
    }

    // replacement bitrate
    void UpdateCurrentBitRate(unsigned int ReplacementBitrate)
    {
        mCurrentEncodingParameters.Controls.BitRateCtrl.bitrate = ReplacementBitrate;
        // TODO check if need to update mNextControls.BitRateCtrl.bitrate to avoid recurrent replacement (and warning) ?
    }

    void            ReleaseMainCodedBuffer(void);           //!< To be called to release buffers references taken in Input()
    CoderStatus_t   SendCodedBufferOut(Buffer_t Buffer, unsigned int Size);          //!< To be called to send buffer out;

    //Implementation, Codec Dependent, Polymorphed

    virtual void    InputPost();    //!< To be called at end for children Input()
    virtual bool    AreCurrentControlsAndMetadataSupported(const CoderAudioCurrentParameters_t *) = 0;

private:
    unsigned int                   mRelayfsIndex;
    CoderAudioEncodingParameters_t mPreviousEncodingParameters;
    CoderAudioCurrentParameters_t  mCurrentEncodingParameters;
    CoderAudioControls_t           mNextControls;

    void FillDefaultBitRate();

    DISALLOW_COPY_AND_ASSIGN(Coder_Audio_c);
};

#endif /* CODER_AUDIO_H */
