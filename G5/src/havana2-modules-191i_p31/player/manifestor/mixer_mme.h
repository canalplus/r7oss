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

Source file name : mixer_mme.h
Author :           Daniel

Concrete definition of an MME mixer driver.


Date        Modification                                    Name
----        ------------                                    --------
29-Jun-07   Created                                         Daniel

************************************************************************/

#ifndef H_MIXER_MME_CLASS
#define H_MIXER_MME_CLASS

#include "player_types.h"
#include "manifestor_audio_ksound.h"
#include "mixer.h"
#include "pseudo_mixer.h"
#include "alsa_backend_ops.h"
#include "dtshd.h"

// /////////////////////////////////////////////////////////////////////////
//
// Macro definitions.
//

/* top most input is reserved for interactive audio */
#define MIXER_MAX_INPUTS ACC_MIXER_MAX_NB_INPUT
#define MIXER_MAX_CLIENTS (MIXER_MAX_INPUTS - 2)
#define MIXER_CODED_DATA_INPUT 2
#define MIXER_INTERACTIVE_INPUT 3
#define MIXER_MAX_INTERACTIVE_CLIENTS 8

#define MIXER_STAGE_PRE_MIX (MIXER_MAX_CLIENTS+1)
#define MIXER_STAGE_POST_MIX (MIXER_STAGE_PRE_MIX+1)
#define MIXER_STAGE_MAX (MIXER_STAGE_POST_MIX+1)

#define MIXER_MAX_48K_GRANULE 1536
#define MIXER_NUM_PERIODS 2

#undef  MIXER_TAG
#define MIXER_TAG                          "Mixer_Mme_c::"

#define MIXER_AUDIO_MAX_INPUT_BUFFERS  (MIXER_MAX_CLIENTS + 1 + MIXER_MAX_INTERACTIVE_CLIENTS)
#define MIXER_AUDIO_MAX_OUTPUT_BUFFERS 4
#define MIXER_AUDIO_MAX_BUFFERS        (MIXER_AUDIO_MAX_INPUT_BUFFERS +\
                                             MIXER_AUDIO_MAX_OUTPUT_BUFFERS)
#define MIXER_AUDIO_PAGES_PER_BUFFER   8
#define MIXER_AUDIO_MAX_PAGES          (MIXER_AUDIO_PAGES_PER_BUFFER * MIXER_AUDIO_MAX_BUFFERS)

#define MIXER_AUDIO_MAX_OUTPUT_ATTENUATION    -96
#define MIXER_MIN_FREQ_AC3_ENCODER 48000
#define MIXER_MAX_FREQ_AC3_ENCODER 48000
#define MIXER_MIN_FREQ_DTS_ENCODER 44100
#define MIXER_MAX_FREQ_DTS_ENCODER 48000

/// Number of 128 sample chunks taken for the 'limiter' gain processing module to perform a mute.
#define MIXER_LIMITER_MUTE_RAMP_DOWN_PERIOD ( 128 / 128)

/// Number of 128 sample chunks taken for the 'limiter' gain processing module to perform an unmute.
#define MIXER_LIMITER_MUTE_RAMP_UP_PERIOD   (1024 / 128)

//Freeze this structure as it is now.
//Otherwise, changes to AudioMixer_ProcessorTypes.h that inserted structures
//could cause huge problems at aggregation (because lack of initialised size
//field on substructures means we can't even skip over them)
//Of course, remain vulnerable to changes in higher structures, but I'm
//taking a bet they'll change less
typedef struct 
{
    enum eAccMixerId            Id;                //!< Id of the PostProcessing structure.
    U16                         StructSize;        //!< Size of this structure
    U8                          NbPcmProcessings;  //!< NbPcmProcessings on main[0..3] and aux[4..7]
    U8                          AuxSplit;          //!< Point of split between Main output and Aux output
    MME_BassMgtGlobalParams_t   BassMgt;
    MME_EqualizerGlobalParams_t Equalizer;
    MME_TempoGlobalParams_t     TempoControl;
    MME_DCRemoveGlobalParams_t  DCRemove;
    MME_DelayGlobalParams_t     Delay;
    MME_EncoderPPGlobalParams_t Encoder;
    MME_SfcPPGlobalParams_t     Sfc;
    MME_Resamplex2GlobalParams_t Resamplex2;
    MME_CMCGlobalParams_t       CMC;
    MME_DMixGlobalParams_t      Dmix;
    MME_FatpipeGlobalParams_t	FatPipeOrSpdifOut;
    MME_LimiterGlobalParams_t	Limiter;
} MME_LxPcmPostProcessingGlobalParameters_Frozen_t; //!< PcmPostProcessings Params

//Redefine this to deal with aggregation of parameter chains
typedef struct
{
    U32                                        StructSize;      //!< Size of this structure
    MME_LxMixerInConfig_t                      InConfig;        //!< Specific configuration of input
    MME_LxMixerGainSet_t                       InGainConfig;    //!< Specific configuration of input gains
    MME_LxMixerPanningSet_t                    InPanningConfig; //!< Specific configuration of input panning
    MME_LxMixerInIAudioConfig_t                InIaudioConfig;  //!< Specific configuration of iaudio input
    MME_LxMixerBDGeneral_t                     InBDGenConfig;   //!< some geenral config for BD mixer
    MME_LxMixerOutConfig_t                     OutConfig;       //!< output specific configuration information
    MME_LxPcmPostProcessingGlobalParameters_Frozen_t PcmParams[MIXER_AUDIO_MAX_OUTPUT_BUFFERS];
                                                                //!< PcmPostProcessings Params
} MME_LxMixerBDTransformerGlobalParams_Extended_t;

typedef struct
{
  U32                                         BytesUsed;  // Amount of this structure already filled
  MME_MixerFrameOutExtStatus_t                FrameOutStatus;
  MME_LimiterStatus_t                         LimiterStatus[MIXER_AUDIO_MAX_OUTPUT_BUFFERS];
} MME_MixerFrameExtStatusLimit_t;

typedef struct
{
  MME_LxMixerTransformerFrameDecodeStatusParams_t     MixStatus;
	MME_MixerFrameExtStatusLimit_t                      MixExtStatus;
} MME_LxMixerTransformerFrameMix_ExtendedParams_t;

//Redefine this too, to incorporate the above extension
typedef struct
{
    U32                   StructSize; //!< Size of this structure
	
    //! System Init 
    enum eAccProcessApply CacheFlush; //!< If ACC_DISABLED then the cached 
    //!< data aren't sent back at the end of the command
	
    //! Mixer Init Params    
    U32                   NbInput; //the number of inputs that the mixer will control simultaneously (this number is limited by definition to 4)
    U32                   MaxNbOutputSamplesPerTransform; //the largest window size the host wants to control
	
    U32                   OutputNbChannels; 	//the interleaving of the output buffer that the host will send to the mixer at each transform
    enum eAccFsCode       OutputSamplingFreq;	//the target output sampling frequency at which the audio will be played back
	
    //! Mixer specific global parameters 
    MME_LxMixerBDTransformerGlobalParams_Extended_t	GlobalParams; 
	
} MME_LxMixerTransformerInitBDParams_Extended_t; 

////////////////////////////////////////////////////////////////////////////
///
/// Mixer implementated using MMSU's MIXER_TRANSFORMER.
///
class Mixer_Mme_c:public Mixer_c
{
private:

    struct
    {
	InputState_t State;
	Manifestor_AudioKsound_c *Manifestor;
        ParsedAudioParameters_t Parameters;
	bool Muted;
    } Clients[MIXER_MAX_CLIENTS];
    
    OS_Mutex_t ClientsStateManagementMutex;
    
    struct
    {
        InputState_t State;
        unsigned int PlayPointer;
        struct alsa_substream_descriptor Descriptor;
    } InteractiveClients[MIXER_MAX_INTERACTIVE_CLIENTS];

    bool PlaybackThreadRunning;
    OS_Thread_t PlaybackThreadId;
    OS_Event_t PlaybackThreadTerminated;

    MME_TransformerHandle_t MMEHandle;
    bool MMEInitialized;
    OS_Semaphore_t MMECallbackSemaphore;
    OS_Semaphore_t MMEParamCallbackSemaphore;
    /// Undefined unless MMEInitialized is true. When defined, true if the MME callback thread has had its
    /// priority boosted.
    bool MMECallbackThreadBoosted;
    bool MMENeedsParameterUpdate;
    
    /// Copy of the sampling frequency selected when the mixer transformer is initialized. May be a zero.
    unsigned int InitializedSamplingFrequency;
    
    /// Copy of the sampling frequency selected when the mixer transformer was configured. Should never be
    /// zero at the point of 'consumption'.
    unsigned int MixerSamplingFrequency;
    
    /// Copy of the sampling frequency selected when the PCM player was configured. This is basically a
    /// copy of Mixer_Mme_c::MixerSamplingFrequency with a subtly different life time. Should never be zero
    /// at the point of 'consumption'.
    unsigned int NominalOutputSamplingFrequency;
    
    /// Copy of the mixer granule (period size) selected when the mixer transformer was configured. Should
    /// never be zero at the point of 'consumption'.
    unsigned int MixerGranuleSize;

    PcmPlayer_c::OutputEncoding PrimaryCodedDataType;

    /// Pointer to the upstream mixer configuration. Changes to this structure will be reflected at the
    /// interface level. Be warned that this structure may be updated without notice (or locking) from
    /// other threads. Where stability is valued use Mixer_Mme_c::OutputConfiguration instead.
    struct snd_pseudo_mixer_settings *UpstreamConfiguration;
    
    /// Latched copy of the upstream configuration. The copy taken whenever ALSA notifies us that the state
    /// has changed. The copy is used to configure the firmware since it is stable (i.e. its value doesn't
    /// change except when we let it).
    struct snd_pseudo_mixer_settings OutputConfiguration;

    struct DownmixFirmwareStruct {
	    struct snd_pseudo_mixer_downmix_header header;
	    struct snd_pseudo_mixer_downmix_index index[];
    } *DownmixFirmware;

    enum eAccAcMode LastInputMode[MIXER_AUDIO_MAX_OUTPUT_BUFFERS]; ///< Used to suppress duplicative messages
    enum eAccAcMode LastOutputMode[MIXER_AUDIO_MAX_OUTPUT_BUFFERS]; ///< Used to suppress duplicative messages
    
    /// Whenever the mixer starts up we keep a copy the output topology to keep us safe against
    /// any changes to this structure.
    struct snd_pseudo_mixer_downstream_topology ActualTopology;
    /// Convenience value derived from ActualTopology.
    unsigned int ActualNumDownstreamCards;

    PcmPlayer_c *PcmPlayer[MIXER_AUDIO_MAX_OUTPUT_BUFFERS];
    PcmPlayerSurfaceParameters_t PcmPlayerSurfaceParameters;
    OS_Event_t PcmPlayerSurfaceParametersUpdated;
    void *PcmPlayerMappedSamples[MIXER_AUDIO_MAX_OUTPUT_BUFFERS];
    bool PcmPlayerNeedsParameterUpdate;
    
    /// Index of the primary client. The primary client is the one that dictacts the hardware settings.
    unsigned int PrimaryClient;
    
    struct
    {
	char         TransformName[MME_MAX_TRANSFORMER_NAME];
	unsigned int MixerPriority;
    } AudioConfiguration;

    struct
    {
	MME_Command_t Command;
	MME_DataBuffer_t *DataBufferList[MIXER_AUDIO_MAX_BUFFERS];
	MME_DataBuffer_t DataBuffers[MIXER_AUDIO_MAX_BUFFERS];
	MME_ScatterPage_t ScatterPages[MIXER_AUDIO_MAX_PAGES];
	unsigned int BufferIndex[MIXER_AUDIO_MAX_PAGES];
	unsigned int ScatterPagesInUse;
	MME_LxMixerTransformerFrameDecodeParams_t InputParams;
		MME_LxMixerTransformerFrameMix_ExtendedParams_t OutputParams;
    } MixerCommand;

    struct
    {
	MME_Command_t Command;
	MME_LxMixerBDTransformerGlobalParams_Extended_t InputParams;
	MME_LxMixerTransformerSetGlobalStatusParams_t OutputParams;
    } ParamsCommand;

    /// Nominal (meaning pre-optimisation) PCM processing chains for each of the outputs.
    MME_LxPcmPostProcessingGlobalParameters_Frozen_t NominalPcmParams[MIXER_AUDIO_MAX_OUTPUT_BUFFERS];

    int LookupClient( Manifestor_AudioKsound_c * Manifestor );
    unsigned int LookupMaxMixerSamplingFrequency();
    unsigned int LookupMixerSamplingFrequency();
    enum eAccAcMode LookupMixerAudioMode();
    unsigned int LookupMixerGranuleSize(unsigned int Frequency);
    
    int SelectGainBasedOnMuteSettings(int ClientIdOrMixingStage, int NormalValue, int MutedValue);

    PlayerStatus_t UpdateGlobalState();
    PlayerStatus_t UpdatePlayerComponentsModuleParameters();
    void UpdateIec60958StatusBits();

    PlayerStatus_t StartPlaybackThread();
    void TerminatePlaybackThread();

    PlayerStatus_t InitializePcmPlayer();
    void TerminatePcmPlayer();
    PlayerStatus_t MapSamples(unsigned int SampleCount, bool NonBlock = false);
    PlayerStatus_t CommitMappedSamples();
    PlayerStatus_t StartPcmPlayer();
    PlayerStatus_t UpdatePcmPlayerParameters();

    PlayerStatus_t InitializeMMETransformer( void );
    PlayerStatus_t TerminateMMETransformer( void );
    PlayerStatus_t SendMMEMixCommand();
    PlayerStatus_t WaitForMMECallback();
    PlayerStatus_t UpdateMixerParameters();
    
    void ResetOutputConfiguration();
    void ResetMixingMetadata();

    PlayerStatus_t FillOutTransformerGlobalParameters( MME_LxMixerBDTransformerGlobalParams_Extended_t *
						       GlobalParams );
    
    PcmPlayer_c::OutputEncoding LookupOutputEncoding( int dev_num, unsigned int freq = 0 );
    unsigned int LookupOutputSamplingFrequency( int dev_num );
#if 0
    unsigned int LookupOutputNumberOfSamples( int dev_num, unsigned int NominalMixerGranuleSize, 
                                              unsigned int ActualSampleRateHz, unsigned int NominalOutputSamplingFrequency );
#endif
    unsigned int LookupOutputNumberOfChannels( int dev_num );

    unsigned int LookupIec60958FrameRate( PcmPlayer_c::OutputEncoding Encoding );
    unsigned int LookupRepetitionPeriod( PcmPlayer_c::OutputEncoding Encoding );

    void FillOutDeviceDownmixParameters( MME_LxPcmPostProcessingGlobalParameters_Frozen_t & PcmParams, 
	    			         int dev_num, bool EnableDMix );
    void FillOutDeviceSpdifParameters( MME_LxPcmPostProcessingGlobalParameters_Frozen_t & PcmParams,
	    			       int dev_num, PcmPlayer_c::OutputEncoding OutputEncoding );
    PlayerStatus_t FillOutDevicePcmParameters( MME_LxPcmPostProcessingGlobalParameters_Frozen_t &
						       PcmParams , int dev_num );

    
    PlayerStatus_t AggregatePcmParameters( MME_LxPcmPostProcessingGlobalParameters_Frozen_t &
						       PcmParams );
    
    PlayerStatus_t FillOutMixCommand();
    PlayerStatus_t FillOutOutputBuffer( MME_DataBuffer_t *DataBuffer );
    void FillOutInputBuffer( unsigned int ManifestorId );
    void FillOutInteractiveBuffer( unsigned int InteractiveId );
    void FillOutSilentBuffer( MME_DataBuffer_t *DataBuffer,
                              tMixerFrameParams *MixerFrameParams = NULL );
    
    PlayerStatus_t UpdateOutputBuffer( MME_DataBuffer_t *DataBuffer );
    void UpdateInputBuffer( unsigned int ManifestorId );
    void UpdateInteractiveBuffer( unsigned int InteractiveId );
    void UpdateMixingMetadata();
    PlayerStatus_t ConfigureHDMICell(PcmPlayer_c::OutputEncoding OutputEncoding, unsigned int CardFlags);

    static enum eAccFsCode TranslateIntegerSamplingFrequencyToDiscrete( unsigned int SamplingFrequency );
    static unsigned int TranslateDiscreteSamplingFrequencyToInteger( enum eAccFsCode DiscreteFrequency );
    static enum eFsRange TranslateIntegerSamplingFrequencyToRange( unsigned int IntegerFrequency );
    static const char * LookupDiscreteSamplingFrequency( enum eAccFsCode DiscreteFrequency );
    static const char * LookupDiscreteSamplingFrequencyRange( enum eFsRange DiscreteRange );
    static unsigned int LookupSpdifPreamblePc( PcmPlayer_c::OutputEncoding Encoding );
    static const char * LookupAudioMode( enum eAccAcMode DiscreteMode );
    static enum eAccAcMode TranslateChannelAssignmentToAudioMode(
	        struct snd_pseudo_mixer_channel_assignment ChannelAssignment );
    static struct snd_pseudo_mixer_channel_assignment TranslateAudioModeToChannelAssignment(
	    	enum eAccAcMode AudioMode );
    static enum eAccAcMode TranslateDownstreamCardToMainAudioMode(
	    	struct snd_pseudo_mixer_downstream_card *DownstreamCard );
    static enum eAccAcMode TranslateDownstreamCardToAuxAudioMode(
	    	struct snd_pseudo_mixer_downstream_card *DownstreamCard );
    static enum eAccAcMode LookupFatPipeOutputMode(enum eAccAcMode InputMode);


public:

    Mixer_Mme_c();
    ~Mixer_Mme_c();

    PlayerStatus_t Halt();
    PlayerStatus_t Reset();
    PlayerStatus_t SetModuleParameters( unsigned int  ParameterBlockSize,
                                        void         *ParameterBlock );

    PlayerStatus_t RegisterManifestor( Manifestor_AudioKsound_c *Manifestor );
    PlayerStatus_t DeRegisterManifestor( Manifestor_AudioKsound_c *Manifestor );
    PlayerStatus_t EnableManifestor( Manifestor_AudioKsound_c *Manifestor );
    PlayerStatus_t DisableManifestor( Manifestor_AudioKsound_c *Manifestor );
    PlayerStatus_t UpdateManifestorParameters( Manifestor_AudioKsound_c *Manifestor,
                                               ParsedAudioParameters_t *ParsedAudioParameters );
    PlayerStatus_t SetManifestorEmergencyMuteState( Manifestor_AudioKsound_c *Manifestor, bool Muted );

    PlayerStatus_t LookupDataBuffer( Manifestor_AudioKsound_c * Manifestor,
                                     MME_DataBuffer_t **DataBufferPP );

    PlayerStatus_t SetOutputRateAdjustment( int adjust );

    void PlaybackThread();
    void CallbackFromMME( MME_Event_t Event, MME_Command_t *Command );
    
    PlayerStatus_t AllocInteractiveInput( int *InteractiveId );
    PlayerStatus_t FreeInteractiveInput( int InteractiveId );
    PlayerStatus_t SetupInteractiveInput( int InteractiveId, struct alsa_substream_descriptor *Descriptor );
    PlayerStatus_t PrepareInteractiveInput( int InteractiveId);
    PlayerStatus_t EnableInteractiveInput( int InteractiveId);
    PlayerStatus_t DisableInteractiveInput( int InteractiveId);

    inline bool IsPlaybackThreadRunning() { return PlaybackThreadRunning; }
    inline unsigned int GetMixerGranuleSize() { return MixerGranuleSize; }
    inline unsigned int GetWorstCaseStartupDelayInMicroSeconds() {
	return (MIXER_NUM_PERIODS * MIXER_MAX_48K_GRANULE * 1000000ull) / 32000;
    }
    
};

#endif // H_MIXER_MME_CLASS
