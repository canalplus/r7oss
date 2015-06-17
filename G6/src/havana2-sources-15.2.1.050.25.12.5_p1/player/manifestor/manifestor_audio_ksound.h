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

#ifndef MANIFESTOR_AUDIO_KSOUND_H
#define MANIFESTOR_AUDIO_KSOUND_H

#include <mme.h>
#include <ACC_Transformers/Audio_DecoderTypes.h>
#include "se_mixer_transformer_types.h"
#include "osinline.h"
#include "allocinline.h"
#include "manifestor_audio.h"
#include "pcmplayer_ksound.h"
#include "mixer.h"
#include "manifestor_audio_codedDataInput.h"

class Mixer_Mme_c;

#undef TRACE_TAG
#define TRACE_TAG   "Manifestor_AudioKsound_c"

////////////////////////////////////////////////////////////////////////////
///
/// Audio manifestor based on the ksound ALSA subset.
///
class Manifestor_AudioKsound_c : public Manifestor_Audio_c
{
public:
    enum BypassPhysicalChannel_t
    {
        /// The bypass is done over SPDIF
        SPDIF = 0,

        /// The bypass is done over HDMI
        HDMI = 1,

        /// number of possible values
        BypassPhysicalChannelCount
    };

    enum OutputState_t
    {
        /// There is no manifestor connected to this input.
        DISCONNECTED,

        /// Neither the input nor the output side are running.
        STOPPED,

        /// The output side is primed and ready but might not be sending (muted)
        /// samples to the speakers yet.
        STARTING,

        /// The output side is running but no connected to a input.
        /// This state is effectively a soft mute state during which it is safe to (hard)
        /// unmute anything connected downstream.
        MUTED,

        /// The output side is running ahead of the input.
        STARVED,

        /// The output side is running and consuming input.
        PLAYING
    };

    /* Constructor / Destructor */
    Manifestor_AudioKsound_c(void);
    ~Manifestor_AudioKsound_c(void);

    /* Overrides for component base class functions */
    ManifestorStatus_t   Halt(void);
    ManifestorStatus_t   Reset(void);
    ManifestorStatus_t   SetModuleParameters(unsigned int  ParameterBlockSize,
                                             void         *ParameterBlock);
    ManifestorStatus_t   Connect(Port_c *Port);

    /* Manifestor audio class functions */
    ManifestorStatus_t  OpenOutputSurface(class HavanaStream_c *Stream, stm_se_sink_input_port_t input_port = STM_SE_SINK_INPUT_PORT_PRIMARY);
    ManifestorStatus_t  CloseOutputSurface(void);
    ManifestorStatus_t  WaitForProcessingBufRingRelease();
    ManifestorStatus_t  GetNextQueuedManifestationTime(unsigned long long    *Time, unsigned int *NumTimes);

    ManifestorStatus_t  QueueBuffer(AudioStreamBuffer_t   *StreamBufPtr);
    ManifestorStatus_t  ReleaseBuffer(AudioStreamBuffer_t *StreamBufPtr);
    ManifestorStatus_t  QueueNullManifestation(void);
    ManifestorStatus_t  FlushDisplayQueue(void);
    ManifestorStatus_t  ReleaseQueuedDecodeBuffers(void);
    void                GetProcessingRingBufCount(int *bufCountPtr);

    void                UpdateDisplayTimeOfNextCommit(unsigned long long Time);


    void   CallbackFromMME(MME_Event_t        Event,
                           MME_Command_t        *Command);

    ManifestorStatus_t FillOutInputBuffer(unsigned int SamplesToDescatter,
                                          Rational_c RescaleFactor,
                                          bool FinalBuffer,
                                          MME_DataBuffer_t *DataBuffer,
                                          tMixerFrameParams *MixerFrameParams,
                                          bool Muted,
                                          MME_DataBuffer_t CodedMixerDataBufferArray[BypassPhysicalChannelCount] = 0,
                                          tMixerFrameParams CodedMixerFrameParamsArray[BypassPhysicalChannelCount] = 0,
                                          bool BypassChannelSPDIF = false,
                                          bool BypassChannelHDMI = false,
                                          bool BypassSDChannelSPDIF = false,
                                          bool BypassSDChannelHDMI = false,
                                          PcmPlayer_c::OutputEncoding *OutputEncodingSPDIF = NULL,
                                          PcmPlayer_c::OutputEncoding *OutputEncodingHDMI = NULL);

    ManifestorStatus_t UpdateInputBuffer(MME_DataBuffer_t *DataBuffer,
                                         MME_MixerInputStatus_t *InputStatus,
                                         MME_DataBuffer_t CodedDataBufferArray[BypassPhysicalChannelCount] = NULL,
                                         MME_MixerInputStatus_t CodedInputStatusArray[BypassPhysicalChannelCount] = NULL,
                                         bool ForceReleaseProcessingDecodedBuffer = false);

    virtual ManifestorStatus_t  GetChannelConfiguration(enum eAccAcMode *AcMode);
    virtual bool                IsTranscodeNeeded(void);
    virtual bool                IsCompressedFrameNeeded(void);

    inline bool IsRunning()
    {
        return IsPlaying;
    };

    static inline const char *LookupOutputState(OutputState_t OutputState)
    {
        switch (OutputState)
        {
#define E(x) case x: return #x
            E(DISCONNECTED);
            E(STOPPED);
            E(STARTING);
            E(MUTED);
            E(STARVED);
            E(PLAYING);
#undef E

        default:
            return "INVALID";
        }
    }

    inline const char *LookupState()
    {
        return LookupOutputState(OutputState);
    }

private:
    Mixer_Mme_c *Mixer;
    bool RegisteredWithMixer;
    bool EnabledWithMixer;

    /// Held when any of the DisplayTimeOfNextCommit family of variables are accessed.
    OS_Mutex_t DisplayTimeOfNextCommitMutex;

    unsigned int SamplesQueuedForDisplayAfterDisplayTimeOfNextCommit;
    unsigned int SamplesPlayedSinceDisplayTimeOfNextCommitWasCalculated;

    unsigned int SamplesToRemoveWhenPlaybackCommences; ///< Carry over samples from previous attempt to shorten.
    bool ReleaseAllInputBuffersDuringUpdate; ///< Flush all pended buffers during update.
    bool ReleaseProcessingDecodedBuffer;

    unsigned long long DisplayTimeOfNextCommit;
    unsigned long long LastDisplayTimeOfNextCommit; ///< Used only for debugging (to display deltas)

    ParsedAudioParameters_t              InputAudioParameters; ///< SampleCount is unused
    unsigned int                         OutputChannelCount;
    unsigned int                         OutputSampleDepthInBytes;
    unsigned long long                   LastActualSystemPlaybackTime;
    unsigned int                         InputAudioModeHistory;
    unsigned int                         InputAudioModeChangeCount;

    class CodedDataInput_c CodedInput[MIXER_MAX_CODED_INPUTS];///< For coded data input management in case of bypass.

    static unsigned int volatile SamplesNeededForFadeOutAfterResampling;
    unsigned int SamplesNeededForFadeOutBeforeResampling;

    OutputState_t OutputState;
    bool          IsPlaying;
    unsigned int  PageCount;
    unsigned int  Dropped;

    bool          TranscodeRequested;
    bool          CompressedFrameRequested;

    int HandleRequestedOutputTiming(AudioStreamBuffer_t *StreamBufPtr);
    void HandleAnticipatedOutputTiming(AudioStreamBuffer_t *StreamBufPtr, unsigned int SampleOffset);

    ManifestorStatus_t FillOutCodedMmeBufferArray(MME_DataBuffer_t CodedDataBufferArray[BypassPhysicalChannelCount],
                                                  Buffer_c *CodedFrameBuffer,
                                                  AudioStreamBuffer_t *curStreamBuf,
                                                  Buffer_c *TranscodedFrameBuffer,
                                                  Buffer_c *CompressedFrameBuffer,
                                                  bool BypassChannelSPDIF,
                                                  bool BypassChannelHDMI,
                                                  bool BypassSDChannelSPDIF,
                                                  bool BypassSDChannelHDMI);
    void EvaluateCodedDataBufferAdjustement(MME_DataBuffer_t *CurrentCodedMmeDataBuffer,
                                            int SamplesInPcmBuffer,
                                            uint32_t CodedIndex,
                                            int  *pDelta,
                                            bool *pMuteRequestAsserted,
                                            bool *pPauseRequestAsserted);
    void AdjustCodedDataBufferAndReturnRemaingDelta(MME_DataBuffer_t *CodedMmeDataBuffer,
                                                    int  *pDelta,
                                                    uint32_t CodedIndex,
                                                    uint32_t RatioCompressedVsPcm);
    void PrepareCodedDataBufferMixCommand(MME_DataBuffer_t  *CurrentCodedMmeDataBuffer,
                                          tMixerFrameParams *CurrentCodedFrameParams,
                                          tMixerFrameParams *PcmFrameParams,
                                          uint32_t CodedIndex,
                                          int  Delta,
                                          bool PlayerMuted,
                                          bool MuteRequestAsserted,
                                          bool PauseRequestAsserted);
    ManifestorStatus_t ShortenDataBuffer(MME_DataBuffer_t *DataBuffer,
                                         tMixerFrameParams *MixerFrameParams,
                                         uint32_t SamplesToRemoveBeforeResampling,
                                         uint32_t IdealisedStartOffset,
                                         uint32_t EndOffsetAfterResampling,
                                         Rational_c RescaleFactor, bool muted);
    ManifestorStatus_t ExtendDataBuffer(MME_DataBuffer_t *DataBuffer,
                                        tMixerFrameParams *MixerFrameParams,
                                        uint32_t SamplesToInjectBeforeResampling,
                                        uint32_t EndOffsetAfterResampling,
                                        Rational_c RescaleFactor, bool muted);

    ManifestorStatus_t UpdateAudioParameters(AudioStreamBuffer_t *StreamBufPtr, bool TakeMixerClientLock);

    ManifestorStatus_t FlushInputBuffer(MME_DataBuffer_t *DataBuffer,
                                        MME_MixerInputStatus_t *InputStatus,
                                        MME_DataBuffer_t CodedDataBufferArray[BypassPhysicalChannelCount],
                                        MME_MixerInputStatus_t CodedInputStatusArray[BypassPhysicalChannelCount]);

    virtual ManifestorStatus_t  GetDRCParams(DRCParams_t *DRC);
    PcmPlayer_c::OutputEncoding LookupCodedDataBufferOutputEncoding(Buffer_c *CodedFrameBuffer,
                                                                    const ParsedAudioParameters_t *ParsedAudioParameters,
                                                                    const ParsedFrameParameters_t *ParsedFrameParameters,
                                                                    uint32_t CodedDataBufferSize,
                                                                    uint32_t *RepetitionPeriod,
                                                                    uint32_t *Oversampling,
                                                                    uint32_t *CodedFrameSampleCount,
                                                                    BypassPhysicalChannel_t BypassChannel,
                                                                    bool bypassSD);

    PcmPlayer_c::OutputEncoding LookupCodedDtsDataBufferOutputEncoding(const ParsedAudioParameters_t *ParsedAudioParameters,
                                                                       uint32_t CodedDataBufferSize,
                                                                       uint32_t *RepetitionPeriod) const;
    PcmPlayer_c::OutputEncoding LookupCodedAacDataBufferOutputEncoding(Buffer_c *CodedFrameBuffer,
                                                                       const ParsedAudioParameters_t *ParsedAudioParameters,
                                                                       const ParsedFrameParameters_t *ParsedFrameParameters,
                                                                       uint32_t *CodedFrameSampleCount,
                                                                       uint32_t *RepetitionPeriod) const;

    static bool DoesTranscodedBufferExist(BypassPhysicalChannel_t BypassChannel, const ParsedAudioParameters_t *AudioParameters, bool bypassSD);
    static bool DoesCompressedFrameBufferExist(BypassPhysicalChannel_t BypassChannel, const ParsedAudioParameters_t *AudioParameters);
    void DequeueCodedDataBuffer(MME_DataBuffer_t *CodedMmeDataBuffer , uint32_t PageIdx, uint32_t NPages, CodedDataInput_c ThisCodedInput);
    void RemoveSamplesFromCodedDataBuffer(MME_DataBuffer_t *CodedMmeDataBuffer, int32_t FirstPageIdxWithPaOffset, uint16_t PaOffsetOfFirstPageToUpdate, int32_t LastPageIdxWithPaOffset,
                                          uint16_t PaOffsetOfLastPageToUpdate, uint32_t CodedIndex);

    uint32_t LookupCodedDataBufferLength(MME_DataBuffer_t *CodedDataBuffer,
                                         CodedDataInput_c &ThisCodedInput);

    ManifestorStatus_t FillOutCodedDataBuffer(MME_DataBuffer_t  *PcmBuffer,
                                              tMixerFrameParams *PcmFrameParams,
                                              MME_DataBuffer_t   CodedDataBufferArray[BypassPhysicalChannelCount],
                                              tMixerFrameParams  CodedFrameParamsArray[BypassPhysicalChannelCount],
                                              bool BypassChannelSPDIF,
                                              bool BypassChannelHDMI,
                                              bool BypassSDChannelSPDIF,
                                              bool BypassSDChannelHDMI,
                                              PcmPlayer_c::OutputEncoding *OutputEncodingSPDIF,
                                              PcmPlayer_c::OutputEncoding *OutputEncodingHDMI);

    void UpdateCodedDataBuffer(MME_DataBuffer_t *CodedDataBuffer,
                               MME_MixerInputStatus_t *CodedInputStatus,
                               CodedDataInput_c &ThisCodedInput);

    ManifestorStatus_t FlushCodedDataBuffer(MME_DataBuffer_t *CodedMmeDataBuffer, CodedDataInput_c ThisCodedInput);

    /// Calculate from the input audio parameters the number of bytes per sample.
    inline uint32_t BytesPerSample() const
    {
        SE_DEBUG(group_manifestor_audio_ksound, "ChannelCount %u BitsPerSample %u",
                 InputAudioParameters.Source.ChannelCount, InputAudioParameters.Source.BitsPerSample);
        return InputAudioParameters.Source.ChannelCount * (InputAudioParameters.Source.BitsPerSample / 8);
    }

    /// Use the input audio parameters to convert a length in samples (c.f. ALSA frames) to bytes
    inline uint32_t SamplesToBytes(uint32_t SampleLength) const
    {
        return SampleLength * BytesPerSample();
    }

    /// Use the input audio parameters to convert a length in bytes to samples (c.f. ALSA frames)
    inline uint32_t BytesToSamples(uint32_t ByteLength) const
    {
        return ByteLength / BytesPerSample();
    }

    /// Use the Layout info of the SPDIFin parameters to convert a length in bytes to samples
    inline uint32_t BytesToSpdifSamples(uint32_t ByteLength) const
    {
        int layout = 0; //Currently only supporting Layout = 0 (2 channel). This function need to be updated when layout 1 and HBRA is supported
        int NoChannelInThisLayout = 0;
        if (layout == 0) // 2 channel in layout 0
        {
            NoChannelInThisLayout  = 2;
        }
        else // 8 channel in layout 1 and HBRA
        {
            NoChannelInThisLayout  = 8;
        }
        return ByteLength / (NoChannelInThisLayout * 4);
    }

    /// Use the Layout info of the SPDIFin parameters to convert a length in samples to bytes
    inline uint32_t SpdifSamplesToBytes(uint32_t SampleLength) const
    {
        int layout = 0; //Currently only supporting Layout = 0 (2 channel). This function need to be updated when layout 1 and HBRA is supported
        int NoChannelInThisLayout = 0;
        if (layout == 0) // 2 channel in layout 0
        {
            NoChannelInThisLayout  = 2;
        }
        else // 8 channel in layout 1 and HBRA
        {
            NoChannelInThisLayout  = 8;
        }
        return SampleLength * (NoChannelInThisLayout * 4);
    }

    DISALLOW_COPY_AND_ASSIGN(Manifestor_AudioKsound_c);
};

#endif // MANIFESTOR_AUDIO_KSOUND_H
