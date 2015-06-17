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
#ifndef PREPROC_AUDIO_H
#define PREPROC_AUDIO_H

#include "preproc_base.h"

#define PREPROC_AUDIO_DEFAULT_INPUT_SAMPLING_FREQ    48000
#define PREPROC_AUDIO_DEFAULT_INPUT_CHANNEL_ALLOC    8 // If you make this more than 8, take care of the corresponding code
#define PREPROC_AUDIO_DEFAULT_INPUT_CHAN_0           STM_SE_AUDIO_CHAN_L
#define PREPROC_AUDIO_DEFAULT_INPUT_CHAN_1           STM_SE_AUDIO_CHAN_R
#define PREPROC_AUDIO_DEFAULT_INPUT_CHAN_2           STM_SE_AUDIO_CHAN_LFE
#define PREPROC_AUDIO_DEFAULT_INPUT_CHAN_3           STM_SE_AUDIO_CHAN_C
#define PREPROC_AUDIO_DEFAULT_INPUT_CHAN_4           STM_SE_AUDIO_CHAN_LS
#define PREPROC_AUDIO_DEFAULT_INPUT_CHAN_5           STM_SE_AUDIO_CHAN_RS
#define PREPROC_AUDIO_DEFAULT_INPUT_CHAN_6           STM_SE_AUDIO_CHAN_LREARS
#define PREPROC_AUDIO_DEFAULT_INPUT_CHAN_7           STM_SE_AUDIO_CHAN_RREARS
#define PREPROC_AUDIO_DEFAULT_INPUT_EMPHASIS         STM_SE_NO_EMPHASIS

#define PREPROC_AUDIO_DEFAULT_OUTPUT_SAMPLING_FREQ    48000
#define PREPROC_AUDIO_DEFAULT_OUTPUT_CHANNEL_COUNT    2
#define PREPROC_AUDIO_DEFAULT_OUTPUT_CHAN_0           STM_SE_AUDIO_CHAN_L
#define PREPROC_AUDIO_DEFAULT_OUTPUT_CHAN_1           STM_SE_AUDIO_CHAN_R

//! will reject input frames that have more samples than this
#define PREPROC_AUDIO_IN_MAX_NB_SAMPLES               (STM_SE_AUDIO_ENCODE_MAX_SAMPLES_PER_FRAME)
//!< will split processings if necessary
#define PREPROC_AUDIO_OUT_MAX_NB_SAMPLES              (STM_SE_AUDIO_ENCODE_MAX_SAMPLES_PER_FRAME)

/// We have a max non 2^ upsampling ratio of 1.5=3/2 (32->48kHz)
/// We want to be able to support 8->48kHz : 8*4 = 32; 32*1.5 = 48kHz; 8kHz*6=48kHz
//! maximum up-sampling ratio
#define PREPROC_AUDIO_MAX_UPSAMPLING_RATIO             6

/**
 * @brief Some statistics for the audio preprocessor
 * They may not have a real meaning due to changing sampling rates, channels, etc */
typedef struct PreprocAudioStatistics_s
{
    struct
    {
        uint64_t Inputs;   //<! Number of calls to Input();
        uint64_t Frames;   //<! Number of calls to Input() with valid Buffer;
        uint64_t FramesOk; //<! Number of calls to Input() that completed ok;
        uint64_t Bytes;    //<! Number of bytes injected
        uint64_t Samples;  //<! Number of samples received
    } In;
    struct
    {
        uint64_t Frames;  //<! Number of buffers sent to output ring
        uint64_t Bytes;   //<! Number of bytes sent to output ring
        uint64_t Samples; //<! Number of samples sent to output ring
    } Out;
} PreprocAudioStatistics_t;

/** @brief Internal storage format to store controls */
typedef struct PreprocAudioControl_s
{
    stm_se_audio_core_format_t  CoreFormat; //!< requester core audio format

    struct
    {
        bool                StreamDriven;
        stm_se_dual_mode_t  DualMode;
    } DualMono;

} PreprocAudioControls_t;

/* @brief Stores useful information from an uncompressed_frame Buffer_c
 * Goal is to abstract buffer_manager and limit calls to buffer manager in the rest of the processing
 * We get the Size, Address and Metatada pointers, and that should be enough
 * until the buffer needs to be send to ring or returned to pool
 */
typedef struct PreprocAudioBuffer_s
{
    Buffer_t                                   Buffer;               //!< A pointer to the Buffer_c object so the object can be retrieved
    uint32_t                                   Size;                 //!< The size in bytes if the Buffer_c payload
    void                                      *CachedAddress;        //!< got from ObtainDataReference with CachedAddress argument
    void                                      *PhysicalAddress;      //!< got from ObtainDataReference with CachedAddress argument
    stm_se_uncompressed_frame_metadata_t      *Metadata;             //!< a pointer to the stm_se_uncompressed_frame_metadata_t attached the buffer
    __stm_se_encode_coordinator_metadata_t    *EncodeCoordinatorMetadata;//!< a pointer to the __stm_se_encode_coordinator_metadata_t attached the buffer
    stm_se_encode_process_metadata_t          *ProcessMetadata;      //!< a pointer to process metatada: NULL for inputs
} PreprocAudioBuffer_t;

/** @brief this structure indicates the current parameters for the current Input()
 * @var PreprocAudioCurrentParameters_t::InputMetadata a pointer to the metadata
 * @var PreprocAudioCurrentParameters_t::Controls latched controls
 * */
typedef struct PreprocAudioCurrentParameters_s
{
    stm_se_uncompressed_frame_metadata_t      *InputMetadata;
    PreprocAudioControls_t                     Controls;
} PreprocAudioCurrentParameters_t;

/** @brief this structure can store default/previous parameters for default/past Input()
 * @var PreprocAudioStoredParameters_t::InputMetadata a copy of the metadata
 * @var PreprocAudioStoredParameters_t::Controls a copy of the controls
 * */
typedef struct PreprocAudioStoredParameters_s
{
    stm_se_uncompressed_frame_metadata_t       InputMetadata;
    PreprocAudioControls_t                     Controls;
} PreprocAudioStoredParameters_t;

class Preproc_Audio_c: public Preproc_Base_c
{
public:
    Preproc_Audio_c(void);
    ~Preproc_Audio_c(void);

    PreprocStatus_t           SetControl(stm_se_ctrl_t  Control, const void *Data);
    PreprocStatus_t           GetControl(stm_se_ctrl_t  Control, void       *Data);
    PreprocStatus_t           SetCompoundControl(stm_se_ctrl_t Control, const void *Data);
    PreprocStatus_t           GetCompoundControl(stm_se_ctrl_t Control, void *Data);

    virtual PreprocStatus_t   Halt(void);

    virtual PreprocStatus_t   Input(Buffer_t Buffer);      // To be called by children::Input() first

    virtual void              GetChannelConfiguration(int64_t *AcMode) = 0;

protected:
    PreprocAudioBuffer_t              CurrentInputBuffer; //!< Stores info about the Buffer from Input()
    PreprocAudioCurrentParameters_t   CurrentParameters;  //!< Stores the information about the current parameters
    PreprocAudioBuffer_t              MainOutputBuffer;   //!< Stores info about the buffer auto got from Preproc_Base_c

    PreprocAudioStoredParameters_t    DefaultParameters;  //!< Stores the information about the default parameters
    PreprocAudioStoredParameters_t    PreviousParameters; //!< Stores the information about the previous parameters

    void                      ReleaseMainPreprocFrameBuffer(void);                       // In case of error releases PreprocFrameBuffer
    PreprocStatus_t           SendBufferToOutput(PreprocAudioBuffer_t *);                // Must be used to send to output ring
    void                      ReleasePreprocOutFrameBuffer(PreprocAudioBuffer_t *);

    virtual void              InputPost(PreprocStatus_t Status);              // To be called at the end of input to update statistics, etc

    virtual bool              AreCurrentControlsAndMetadataSupported(); // Check CurrentInputBuffer and CurrentParameters

    // Basic uncompressed audio buffer parsing functions
    PreprocStatus_t           FillPreprocAudioBufferFromInputBuffer(PreprocAudioBuffer_t *, Buffer_t); // To get useful info in one location
    PreprocStatus_t           FillPreprocAudioBufferFromPreProcBuffer(PreprocAudioBuffer_t *, Buffer_t); // To get useful info in one location
    PreprocStatus_t           GetNumberAllocatedChannels(PreprocAudioBuffer_t *, uint32_t *Channels);  // Get total number of channels from an uncompressed audio buffer
    PreprocStatus_t           GetNumberBytesPerSample(PreprocAudioBuffer_t *, uint32_t *Bytes);        // Get sample width from an uncompressed audio buffer
    PreprocStatus_t           GetNumberSamples(PreprocAudioBuffer_t *, uint32_t *Samples);             // Get number of samples in an uncompressed audio buffer

private:
    unsigned int              mRelayfsIndex;
    PreprocAudioStatistics_t  AudioStatistics;          //!< Some counters for debug
    PreprocAudioControls_t    Controls;                 //!< Direct storage

    void                      CheckCurrentControlsForBypassValues(void);        // For latched 'bypass' values, use Metadata instead
    PreprocStatus_t           InputUpdateStatistics(PreprocAudioBuffer_t *);    // To update statistics at Input()
    PreprocStatus_t           OutputUpdateStatistics(PreprocAudioBuffer_t *);   // To update statistics at Output()
    void                      SaveCurrentParametersToPrevious(void);            // Stores current parameters so they can be compared at next iteration
};

#endif /* PREPROC_AUDIO_H */
