/************************************************************************
Copyright (C) 2007 STMicroelectronics. All Rights Reserved.

This file is part of the Player2 Library.

Player2 is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License version 2 as published by the
Free Software Foundation.

Player2 is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with player2; see the file COPYING.  If not, write to the Free Software
Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

The Player2 Library may alternatively be licensed under a proprietary
license from ST.

Source file name : manifestor_audio_ksound.h
Author :           Daniel

Definition of the surface class for havana.


Date        Modification                                    Name
----        ------------                                    --------
14-May-07   Created (from manifestor_audio_stmfb.h)         Daniel

************************************************************************/

#ifndef MANIFESTOR_AUDIO_KSOUND_H
#define MANIFESTOR_AUDIO_KSOUND_H

#include <mme.h>
#include <ACC_Transformers/Audio_DecoderTypes.h>
#include <ACC_Transformers/Mixer_ProcessorTypes.h>

#include "osinline.h"
#include "allocinline.h"
#include "manifestor_audio.h"
#include "pcmplayer_ksound.h"
#include "mixer.h"

// /////////////////////////////////////////////////////////////////////////
//
// Constants, typedefs
//

class Mixer_Mme_c;


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
        SPDIF,
        
        /// The bypass is done over HDMI
        HDMI,
        
        /// number of possible values
        BypassPhysicalChannelCount
    };
    
        
private:

    bool digitalFlag;
    bool errorFrameSeen;
    unsigned int numGoodFrames;

    Mixer_Mme_c *Mixer;
    bool RegisteredWithMixer;
    bool EnabledWithMixer;

    /// Held when any of the DisplayTimeOfNextCommit family of variables are accessed.
    OS_Mutex_t DisplayTimeOfNextCommitMutex;
    
    unsigned int SamplesQueuedForDisplayAfterDisplayTimeOfNextCommit;
    unsigned int SamplesPlayedSinceDisplayTimeOfNextCommitWasCalculated;
    
    unsigned int SamplesToRemoveWhenPlaybackCommences; ///< Carry over samples from previous attempt to shorten.
    bool ReleaseAllInputBuffersDuringUpdate; ///< Flush all pended buffers during update.
    
    unsigned long long DisplayTimeOfNextCommit;
    unsigned long long LastDisplayTimeOfNextCommit; ///< Used only for debugging (to display deltas)
    
    ParsedAudioParameters_t              InputAudioParameters; ///< SampleCount is unused
    unsigned int			 OutputSampleRateHz;
    unsigned int                         OutputChannelCount;
    unsigned int                         OutputSampleDepthInBytes;
    unsigned long long                   LastActualSystemPlaybackTime;			 
    unsigned int                         CodedFrameSampleCount;
    
    unsigned int SamplesUntilNextCodedDataRepetitionPeriod;
    bool FirstCodedDataBufferPartiallyConsumed;
    bool IsCodedDataPlaying;
    PcmPlayer_c::OutputEncoding CurrentCodedDataEncoding;
    unsigned int CurrentCodedDataRepetitionPeriod;
    int LastCodedDataSyncOffset; ///< Used only for debugging (to display deltas)

    static const unsigned int SamplesNeededForFadeOutAfterResampling = 128;
    unsigned int SamplesNeededForFadeOutBeforeResampling;
    
    OutputState_t OutputState;
    bool                                 IsTranscoded; ///< keeps track of whether the Coded Buffer has an attached Transcoded Buffer

    int HandleRequestedOutputTiming(unsigned int BufferIndex);
    void HandleAnticipatedOutputTiming(unsigned int BufferIndex, unsigned int SampleOffset);
    
    ManifestorStatus_t ShortenDataBuffer( MME_DataBuffer_t *DataBuffer,
                                          tMixerFrameParams *MixerFrameParams,
                                          unsigned int SamplesToRemoveBeforeResampling,
                                          unsigned int IdealisedStartOffset,
                                          unsigned int EndOffsetAfterResampling,
                                          Rational_c RescaleFactor );
    ManifestorStatus_t ExtendDataBuffer( MME_DataBuffer_t *DataBuffer,
                                         tMixerFrameParams *MixerFrameParams,
                                         unsigned int SamplesToInjectBeforeResampling,
                                         unsigned int EndOffsetAfterResampling,
                                         Rational_c RescaleFactor );
    
    ManifestorStatus_t UpdateAudioParameters(unsigned int BufferIndex);

    ManifestorStatus_t FlushInputBuffer( MME_DataBuffer_t *DataBuffer,
                                         MME_MixerInputStatus_t *InputStatus,
                                         MME_DataBuffer_t *CodedDataBuffer,
                                         MME_MixerInputStatus_t *CodedInputStatus );

    PcmPlayer_c::OutputEncoding LookupCodedDataBufferOutputEncoding(
                                                Buffer_c *CodedFrameBuffer, unsigned int CodedDataBufferSize,
                                                unsigned int *RepetitionPeriod,
                                                BypassPhysicalChannel_t BypassChannel );

    static PcmPlayer_c::OutputEncoding LookupCodedDtsDataBufferOutputEncoding(
        ParsedAudioParameters_t *ParsedAudioParameters,
        unsigned int CodedDataBufferSize,
        unsigned int *RepetitionPeriod );

    static bool DoesTranscodedBufferExist(BypassPhysicalChannel_t BypassChannel, ParsedAudioParameters_t * AudioParameters);
    
    void DequeueFirstCodedDataBuffer( MME_DataBuffer_t *CodedDataBuffer );
                                                
    unsigned int LookupCodedDataBufferLength( MME_DataBuffer_t *CodedDataBuffer );
                                                                                                
    ManifestorStatus_t FillOutCodedDataBuffer( MME_DataBuffer_t *PcmBuffer,
                                               tMixerFrameParams *PcmFrameParams,
                                               MME_DataBuffer_t *CodedDataBuffer,
                                               tMixerFrameParams *MixerFrameParams,
                                               PcmPlayer_c::OutputEncoding *OutputEncoding,
                                               BypassPhysicalChannel_t BypassChannel );
                                                   
    ManifestorStatus_t UpdateCodedDataBuffer( MME_DataBuffer_t *CodedDataBuffer,
                                              MME_MixerInputStatus_t *CodedInputStatus );
                                                   
    ManifestorStatus_t FlushCodedDataBuffer( MME_DataBuffer_t *CodedDataBuffer );

    ManifestorStatus_t ReleaseTranscodedDataBuffer( Buffer_c *CodedDataBuffer );

    /// Calculate from the input audio parameters the number of bytes per sample.
    inline unsigned int BytesPerSample()
    {
        return InputAudioParameters.Source.ChannelCount * (InputAudioParameters.Source.BitsPerSample / 8);
    }

    /// Use the input audio parameters to convert a length in samples (c.f. ALSA frames) to bytes
    inline unsigned int SamplesToBytes( unsigned int SampleLength )
    {
        return SampleLength * BytesPerSample();
    }
    
    /// Use the input audio parameters to convert a length in bytes to samples (c.f. ALSA frames)
    inline unsigned int BytesToSamples( unsigned int ByteLength )
    {
        return ByteLength / BytesPerSample();
    }

public:

    /*
     * Accessor methods for Mixer SW to access the 
     * TranscodedFrameBuffer outside the Manifestor.
     * And to update the input after mix completion.
     */ 
    ManifestorStatus_t getTranscodedMmeDataBuffer( MME_DataBuffer_t *CodedMmeDataBuffer);
    
    OutputState_t getOutputState();

    /* Constructor / Destructor */
    Manifestor_AudioKsound_c                            (void);
    ~Manifestor_AudioKsound_c                           (void);

    /* Overrides for component base class functions */
    ManifestorStatus_t   Halt                          (void);
    ManifestorStatus_t   Reset                         (void);
    ManifestorStatus_t   SetModuleParameters           (unsigned int           ParameterBlockSize,
                                                        void*                  ParameterBlock);                        
    ManifestorStatus_t   GetDecodeBufferPool           (class BufferPool_c**   Pool);

    /* Manifestor video class functions */
    ManifestorStatus_t  OpenOutputSurface              ();
    ManifestorStatus_t  CloseOutputSurface             (void);
    ManifestorStatus_t  CreateDecodeBuffers            (unsigned int            Count,
                                                        unsigned int            Width,
                                                        unsigned int            Height);
    ManifestorStatus_t  DestroyDecodeBuffers           (void);
    ManifestorStatus_t  GetNextQueuedManifestationTime (unsigned long long*    Time);

    ManifestorStatus_t  QueueBuffer                    (unsigned int                    BufferIndex);
    ManifestorStatus_t  ReleaseBuffer                  (unsigned int                    BufferIndex);
    ManifestorStatus_t  QueueNullManifestation         (void);
    ManifestorStatus_t  FlushDisplayQueue              (void);

    void                UpdateDisplayTimeOfNextCommit  (unsigned long long Time);


    void   CallbackFromMME(			MME_Event_t		  Event,
						MME_Command_t		 *Command );
    
    ManifestorStatus_t FillOutInputBuffer( unsigned int SamplesToDescatter,
                                           Rational_c RescaleFactor,
                                           bool FinalBuffer,
                                           MME_DataBuffer_t *DataBuffer,
                                           tMixerFrameParams *MixerFrameParams,
                                           MME_DataBuffer_t *CodedDataBuffer = 0,
                                           tMixerFrameParams *CodedMixerFrameParams = 0,
                                           PcmPlayer_c::OutputEncoding *OutputEncoding = 0,
                                           BypassPhysicalChannel_t BypassChannel = SPDIF );

    ManifestorStatus_t UpdateInputBuffer( MME_DataBuffer_t *DataBuffer,
                                          MME_MixerInputStatus_t *InputStatus,
                                          MME_DataBuffer_t *CodedDataBuffer = 0,
                                          MME_MixerInputStatus_t *CodedInputStatus = 0 );
    
    static inline const char *LookupOutputState( OutputState_t OutputState )
    {
        switch ( OutputState )
        {
#define E(x) case x: return #x
            E( DISCONNECTED );
            E( STOPPED );
            E( STARTING );
            E( MUTED );
            E( STARVED );
            E( PLAYING );
#undef E
            default:return "INVALID";
        }
    }
    
    inline const char *LookupState()
    {
    	return LookupOutputState( OutputState );
    }
};

#endif // MANIFESTOR_AUDIO_KSOUND_H
