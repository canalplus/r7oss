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

#ifndef H_MIXER_MME_CLASS
#define H_MIXER_MME_CLASS

#include "audio_player.h"
#include "player_types.h"
#include "manifestor_audio_ksound.h"
#include "audio_generator.h"
#include "mixer.h"
#include "dtshd.h"
#include "aac_audio.h"
#include "havana_stream.h"
#include "mixer_request_manager.h"

#include "mixer_client.h"

#include "mixer_player.h"
#include "audio_generator.h"

#include "mixer_settings.h"

#undef TRACE_TAG
#define TRACE_TAG   "Mixer_Mme_c"

// /////////////////////////////////////////////////////////////////////////
//
// Macro definitions.
//
#define MIXER_MAX_48K_GRANULE 1536

#define MIXER_NUM_PERIODS 2

/// Number of 128 sample chunks taken for the 'limiter' gain processing module to perform a mute.
#define MIXER_LIMITER_MUTE_RAMP_DOWN_PERIOD ( 128 / 128)

/// Number of 128 sample chunks taken for the 'limiter' gain processing module to perform an unmute.
#define MIXER_LIMITER_MUTE_RAMP_UP_PERIOD   (1024 / 128)

#define INTERMIXER_BUFFER_COUNT 2
#define MAX_INTER_BUFFER_SIZE (1536 * 8 * 4)

//Freeze this structure as it is now.
//Otherwise, changes to AudioMixer_ProcessorTypes.h that inserted structures
//could cause huge problems at aggregation (because lack of initialized size
//field on substructures means we can't even skip over them)
//Of course, remain vulnerable to changes in higher structures, but I'm
//taking a bet they'll change less
typedef struct
{
    enum eAccMixerId            Id;                //!< Id of the PostProcessing structure.
    U16                         StructSize;        //!< Size of this structure
    U8                          NbPcmProcessings;  //!< NbPcmProcessings on main[0..3] and aux[4..7]
    U8                          AuxSplit;          //!< Point of split between Main output and Aux output
    MME_ExtBassMgtGlobalParams_t BassMgt;
    MME_EqualizerGlobalParams_t Equalizer;
    MME_TempoGlobalParams_t     TempoControl;
    MME_DCRemoveGlobalParams_t  DCRemove;
    MME_DelayGlobalParams_t     Delay;
    MME_EncoderPPGlobalParams_t Encoder;
    MME_SfcPPGlobalParams_t     Sfc;
    MME_Resamplex2GlobalParams_t Resamplex2;
    MME_CMCGlobalParams_t       CMC;
    MME_DMixGlobalParams_t      Dmix;
    MME_FatpipeGlobalParams_t   FatPipeOrSpdifOut;
    MME_LimiterGlobalParams_t   Limiter;
    MME_BTSCGlobalParams_t      Btsc;
    MME_METARENCGlobalParams_t  MetaRencode; // DDRE and DTSMetadata
    MME_VolumeManagersParams_u  VolumeManagerParams;
    MME_VirtualizerParams_u     VirtualizerParams;
    MME_UpmixerParams_u         UpmixerParams;
    MME_DialogEnhancerParams_u  DialogEnhancerParams;
    MME_TuningGlobalParams_t    VolumeManagerGlobalParams;
    MME_TuningGlobalParams_t    VirtualizerGlobalParams;
    MME_TuningGlobalParams_t    UpmixerGlobalParams;
    MME_TuningGlobalParams_t    DialogEnhancerGlobalParams;
} MME_LxPcmPostProcessingGlobalParameters_Frozen_t; //!< PcmPostProcessings Params

//Redefine this to deal with aggregation of parameter chains
typedef struct
{
    U32                                        StructSize;      //!< Size of this structure
    MME_LxMixerInConfig_t                      InConfig;        //!< Specific configuration of input
    MME_LxMixerGainSet_t                       InGainConfig;    //!< Specific configuration of input gains
    MME_LxMixerPanningSet_t                    InPanningConfig; //!< Specific configuration of input panning
    MME_LxMixerInIAudioConfig_t                InIaudioConfig;  //!< Specific configuration of iaudio input
    MME_LxMixerBDGeneral_t                     InBDGenConfig;   //!< some general config for BD mixer
    MME_LxMixerOutConfig_t                     OutConfig;       //!< output specific configuration information
    MME_LxMixerOutTopology_t                   OutTopology;     //!< output specific topology information
    MME_LxPcmPostProcessingGlobalParameters_Frozen_t PcmParams[MIXER_AUDIO_MAX_OUTPUT_BUFFERS + 1]; //!< PcmPostProcessings Params (Outputs + CommonChain)
} MME_LxMixerBDTransformerGlobalParams_Extended_t;

typedef struct
{
    U32                                         BytesUsed;  // Amount of this structure already filled
    MME_MixerFrameOutExtStatus_t                FrameOutStatus;
    MME_LimiterStatus_t                         LimiterStatus[MIXER_AUDIO_MAX_OUTPUT_BUFFERS];
    MME_CrcStatus_t                             Crc; // If CRC computation is enabled , then CRC for the 4 mixer output is reported in a single struct.
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
    union
    {
        enum eAccProcessApply CacheFlush; //!< If ACC_DISABLED then the cached
        MME_MemLogParams_t    MemConfig;  //!< Control Memory allocation reporting and Cache
    };
    //!< data aren't sent back at the end of the command

    //! Mixer Init Params
    U32                   NbInput        :  4; //the number of inputs that the mixer will control simultaneously (this number is limited by definition to 4)
    U32                   Reserved       : 20;
    U32                   DisableMixing  :  1; //! if set, disable the mixing stage(valid only for NbInputs == 1)
    U32                   EnableSFreqGroup: 1; //! if set allow the MixerMT to process the mixing per group of SFreq.
    U32                   BlockWise      :  2; //Each transform will be processed in sublock of (128, 256, 516 .. ) samples i.e, 64 * 2 ^ blockwise
    U32                   Index          :  4; // index of mixer transform(for postmortem) debugging
    U32                   MaxNbOutputSamplesPerTransform; //the largest window size the host wants to control

    union
    {
        U32                 Card;
        MME_OutChan_t       SubCard;     //the interleaving of the output buffer that the host will send to the mixer at each transform
    } OutputNbChannels;
    enum eAccFsCode       OutputSamplingFreq;   //the target output sampling frequency at which the audio will be played back

    //! Mixer specific global parameters
    MME_LxMixerBDTransformerGlobalParams_Extended_t     GlobalParams;

} MME_LxMixerTransformerInitBDParams_Extended_t;

////////////////////////////////////////////////////////////////////////////
///
/// Mixer implemented using MMSU's MIXER_TRANSFORMER.
///
class Mixer_Mme_c : public Mixer_c
{
public:
    enum
    {
        MIXER_CODED_DATA_INPUT_SPDIF_IDX = 0,
        MIXER_CODED_DATA_INPUT_HDMI_IDX = 1,
        MIXER_CODED_DATA_INPUT_MAX_NUM = 2
    };

    Mixer_Mme_c(const char *Name, char *TransformerName, size_t TransformerNameSize,
                stm_se_mixer_spec_t Topology);

    ~Mixer_Mme_c(void);

    char Name[32];  // TODO(pht) move out of public

    PlayerStatus_t Halt();
    PlayerStatus_t Reset();
    PlayerStatus_t IsDisconnected() const;
    PlayerStatus_t IsIdle() const;
    PlayerStatus_t SetModuleParameters(unsigned int  ParameterBlockSize,
                                       void         *ParameterBlock);
    PlayerStatus_t SetOption(stm_se_ctrl_t ctrl, int value);
    PlayerStatus_t GetOption(stm_se_ctrl_t ctrl, int &value) /*const*/;
    PlayerStatus_t SetCompoundOption(stm_se_ctrl_t ctrl, const void *value);
    PlayerStatus_t GetCompoundOption(stm_se_ctrl_t ctrl, void *value) /*const*/;

    PlayerStatus_t SendAttachPlayer(Audio_Player_c *AudioPlayer);
    PlayerStatus_t SendDetachPlayer(Audio_Player_c *AudioPlayer);

    PlayerStatus_t SendGetProcessingRingBufCountRequest(Manifestor_AudioKsound_c *Manifestor, int *bufCountPtr);

    PlayerStatus_t SendRegisterManifestorRequest(Manifestor_AudioKsound_c *Manifestor,
                                                 HavanaStream_c *Stream,
                                                 stm_se_sink_input_port_t input_port);
    PlayerStatus_t SendDeRegisterManifestorRequest(Manifestor_AudioKsound_c *Manifestor);
    PlayerStatus_t SendEnableManifestorRequest(Manifestor_AudioKsound_c *Manifestor);
    PlayerStatus_t SendDisableManifestorRequest(Manifestor_AudioKsound_c *Manifestor);
    PlayerStatus_t SendSetupAudioGeneratorRequest(Audio_Generator_c *aGenerator);
    PlayerStatus_t SendFreeAudioGeneratorRequest(Audio_Generator_c *aGenerator);

    PlayerStatus_t UpdateManifestorParameters(Manifestor_AudioKsound_c *Manifestor,
                                              ParsedAudioParameters_t *ParsedAudioParameters,
                                              bool TakeMixerClientLock);

    PlayerStatus_t SetManifestorEmergencyMuteState(Manifestor_AudioKsound_c *Manifestor, bool Muted);

    PlayerStatus_t SetOutputRateAdjustment(int adjust);

    PlayerStatus_t LowPowerEnter(__stm_se_low_power_mode_t low_power_mode);
    PlayerStatus_t LowPowerExit(void);

    void PlaybackThread();
    void CallbackFromMME(MME_Event_t Event, MME_Command_t *Command, bool isFirstMixer);

    void GetDRCParameters(DRCParams_t *DRC);

    PlayerStatus_t SetupAudioGenerator(Audio_Generator_c *aGenerator);
    PlayerStatus_t SetupAudioGenerator(MixerRequest_c &aMixerRequest);
    PlayerStatus_t SetAudioGeneratortoStoppedState(Audio_Generator_c *aGenerator);
    PlayerStatus_t FreeAudioGenerator(Audio_Generator_c *aGenerator);
    PlayerStatus_t FreeAudioGenerator(MixerRequest_c &aMixerRequest);
    PlayerStatus_t SetupInteractiveAudio(Audio_Generator_c *aGenerator);
    PlayerStatus_t FreeInteractiveAudio(Audio_Generator_c *iAudio);

    PlayerStatus_t StartAudioGenerator(Audio_Generator_c *aGenerator);
    PlayerStatus_t StopAudioGenerator(Audio_Generator_c *aGenerator);

    PlayerStatus_t GetChannelConfiguration(enum eAccAcMode *AcMode);

    PlayerStatus_t UpdateTransformerId(const char *transformerName);

    inline uint32_t GetMixerGranuleSize() const
    {
        return MixerGranuleSize;
    }

    inline uint32_t GetMixerSamplingFrequency() const
    {
        return MixerSamplingFrequency;
    }

    inline uint32_t GetWorstCaseStartupDelayInMicroSeconds() const
    {
        return (MIXER_NUM_PERIODS * SND_PSEUDO_MIXER_MAX_GRAIN * 1000000ull) / 32000;
    }

    static const char *LookupMixerCommand(U16 cmd)
    {
        switch ((enum eMixerCommand)cmd)
        {
#define E(x) case x: return #x
            E(MIXER_STOP);
            E(MIXER_PLAY);
            E(MIXER_FADE_OUT);
            E(MIXER_MUTE);
            E(MIXER_PAUSE);
            E(MIXER_FADE_IN);
            E(MIXER_FOFI);
#undef E
        default:
            return "INVALID";
        }
    }

private:
    stm_se_mixer_spec_t mTopology;
    // For control of primary and secondary clients of mixer input.
    Mixer_Client_c Client[MIXER_MAX_CLIENTS];
    Mixer_Request_Manager_c MixerRequestManager;

    bool PlaybackThreadRunning;
    OS_Event_t mPlaybackThreadOnOff;

    // Low power data
    __stm_se_low_power_mode_t   LowPowerMode;  // Save the current low power mode in case of standby
    bool        IsLowPowerState;          // HPS/CPS: indicates when SE is in low power state
    bool        IsLowPowerMMEInitialized; // CPS: indicates whether MME transformer was initialized when entering low power state
    OS_Event_t  LowPowerEnterEvent;       // HPS/CPS: event used to synchronize with the mixer thread on low power entry
    OS_Event_t  LowPowerExitEvent;        // HPS/CPS: event used to synchronize with the mixer thread on low power exit
    OS_Event_t  LowPowerExitCompletedEvent;//HPS/CPS: event used to synchronize with the mixer thread on low power exit completi
    int32_t     LowPowerEnterThreadLoop;  // HPS/CPS: counter used to ensure that the mixer output becomes null on low power entry
    int32_t     LowPowerPostMixGainSave;  // HPS/CPS: ouput mixer gain to be saved/restored on low power entry/exit
    bool        InLowPowerState;          // HPS/CPS: mixer state In or Out of LowPower

    enum
    {
        Idle,
        Starting,
        Playback
    } ThreadState;

    bool     PcmPlayersStarted;
    uint32_t mSPDIFCardId;
    uint32_t mHDMICardId;

    int32_t  WaitNCommandForAudioToAudioSync;

    char *MMETransformerName;
    size_t MMETransformerNameSize;
    MME_TransformerHandle_t MMEHandle;
    MME_TransformerHandle_t MMEFirstMixerHdl;
    bool LowPowerWakeUpFirstMixer;
    MME_TransformerInitParams_t MMEInitParams;
    MME_TransformerInitParams_t FirstMixerMMEInitParams;
    bool MMEInitialized;
    OS_Semaphore_t MMECallbackSemaphore;
    OS_Semaphore_t MMEFirstMixerCallbackSemaphore;
    OS_Semaphore_t MMEParamCallbackSemaphore;

    RingGeneric_c *InterMixerFreeBufRing;
    RingGeneric_c *InterMixerFillBufRing;

    /// Undefined unless MMEInitialized is true. When defined, true if the MME callback thread has had its
    /// priority boosted.
    bool MMECallbackThreadBoosted;

    // Will trigger parameter update for MME mixer, in main loop of PlaybackThread()
    bool MMENeedsParameterUpdate;

    /// Can be set by STKPI control, i.e. STM_SE_FIXED_OUTPUT_FREQUENCY or kept as MIXER_SAMPLING_FREQUENCY_STKPI_UNSET.
    uint32_t InitializedSamplingFrequency;

    /// Copy of the sampling frequency selected when the mixer transformer was configured. Should never be
    /// zero at the point of 'consumption'.
    uint32_t MixerSamplingFrequency;

    /// Copy of the sampling frequency selected when the PCM player was configured. This is basically a
    /// copy of Mixer_Mme_c::MixerSamplingFrequency with a subtly different life time. Should never be zero
    /// at the point of 'consumption'.
    uint32_t NominalOutputSamplingFrequency;

    /// default grain with which the mixer is scheduled (can be changed by user on a per mixer basis)
    uint32_t MasterOutputGrain;

    /// Copy of the mixer granule (period size) selected when the mixer transformer was configured. Should
    /// never be zero at the point of 'consumption'.
    uint32_t MixerGranuleSize;

    /// Helps in configuring output for the bypass.
    uint32_t MasterClientIndex;
    /// For bypass or encode feature, depending on original stream kind and audio players client choice.
    PcmPlayer_c::OutputEncoding PrimaryCodedDataType[MIXER_MAX_CODED_INPUTS];

    /// Pointer to the upstream mixer configuration. Changes to this structure will be reflected at the
    /// interface level. Be warned that this structure may be updated without notice (or locking) from
    /// other threads. Where stability is valued use Mixer_Mme_c::OutputConfiguration instead.
    struct snd_pseudo_mixer_settings *UpstreamConfiguration;

    Mixer_Settings_c MixerSettings;

    struct DownmixFirmwareStruct
    {
        struct snd_pseudo_mixer_downmix_header header;
        struct snd_pseudo_mixer_downmix_index index[];
    } *DownmixFirmware;

    /// Controls of Audio_Player_c objects actually attached to the mixer.
    uint32_t NumberOfAudioPlayerAttached; ///< Number of active players attached to this mixer
    uint32_t NumberOfAudioPlayerInitialized; ///< Number of active players initialized with this mixer

    /// The number of Audio_Generator_c objects actually attached to the mixer.
    uint32_t NumberOfAudioGeneratorAttached;
    uint32_t NumberOfInteractiveAudioAttached;

    int32_t FirstActivePlayer; ///< Identify the first active player attached to this mixer

    OS_Event_t PcmPlayerSurfaceParametersUpdated;

    bool PcmPlayerNeedsParameterUpdate;

    /// Index of the primary client. The primary client is the one that dictacts the hardware settings.
    uint32_t PrimaryClient;
    uint32_t SecondaryClient;

    /// MixerParams as used by InitializeMMETransformer
    /// This would not normally be a class member but at 2920 bytes we shouldn't allocate
    /// it on the stack, and neither do we want to be adding in malloc/free calls.
    MME_LxMixerTransformerInitBDParams_Extended_t MixerParams;

    struct
    {
        bool         CRC;
    } AudioConfiguration;

    struct
    {
        MME_Command_t Command;
        MME_DataBuffer_t *DataBufferList[FIRST_MIXER_MAX_BUFFERS];
        MME_DataBuffer_t DataBuffers[FIRST_MIXER_MAX_BUFFERS];
        MME_ScatterPage_t ScatterPages[FIRST_MIXER_MAX_PAGES];
        AudioStreamBuffer_t *BufferIndex[FIRST_MIXER_MAX_PAGES];
        unsigned int ScatterPagesInUse;
        MME_LxMixerTransformerFrameDecodeParams_t InputParams;
        MME_LxMixerTransformerFrameMix_ExtendedParams_t OutputParams;
    } FirstMixerCommand;

    struct
    {
        MME_Command_t Command;
        MME_DataBuffer_t *DataBufferList[MIXER_AUDIO_MAX_BUFFERS];
        MME_DataBuffer_t DataBuffers[MIXER_AUDIO_MAX_BUFFERS];
        MME_ScatterPage_t ScatterPages[MIXER_AUDIO_MAX_PAGES];
        AudioStreamBuffer_t *BufferIndex[MIXER_AUDIO_MAX_PAGES];
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

    struct
    {
        MME_Command_t Command;
        MME_LxMixerBDTransformerGlobalParams_Extended_t InputParams;
        MME_LxMixerTransformerSetGlobalStatusParams_t OutputParams;
    } FirstMixerParamsCommand;

    /// Elementary PCM processing chains for an output.
    MME_LxPcmPostProcessingGlobalParameters_Frozen_t CurrentOutputPcmParams;

    /// Audio player objects.
    Mixer_Player_c MixerPlayer[STM_SE_MIXER_NB_MAX_OUTPUTS];

    // Audio generator table
    Audio_Generator_c       *Generator[MAX_AUDIO_GENERATORS];
    Audio_Generator_c       *InteractiveAudio[MAX_INTERACTIVE_AUDIO];

    // History variables
    uint32_t FirstMixerSamplingFrequencyHistory;
    uint32_t MixerSamplingFrequencyHistory;
    bool     FirstMixerSamplingFrequencyUpdated;
    bool     MixerSamplingFrequencyUpdated;
    bool     OutputTopologyUpdated;
    bool     FirstMixerClientAudioParamsUpdated;
    bool     ClientAudioParamsUpdated;
    bool     AudioGeneratorUpdated;
    bool     IAudioUpdated;

    //Timing measurement
    long long int MixCommandStartTime;

    int LookupClientFromStream(HavanaStream_c *Stream) const;
    uint32_t LookupPcmPlayersMaxSamplingFrequency() const;
    uint32_t LookupMixerSamplingFrequency() const;
    enum eAccAcMode LookupMixerAudioMode() const;
    uint32_t LookupMixerGranuleSize(uint32_t Frequency) const;

    inline uint32_t LockClientAndLookupMixerGranuleSize(uint32_t Frequency)
    {
        uint32_t Granule;
        Client[PrimaryClient].LockTake();
        Granule  = LookupMixerGranuleSize(Frequency);
        Client[PrimaryClient].LockRelease();
        return Granule;
    }

    PlayerStatus_t UpdatePlayerComponentsModuleParameters();
    void UpdatePcmPlayersIec60958StatusBits();

    PlayerStatus_t StartPlaybackThread();
    void TerminatePlaybackThread();

    bool PlaybackThreadLowPowerEnter(void);
    void PlaybackThreadLowPowerExit(void);

    PlayerStatus_t InitializePcmPlayers();
    PlayerStatus_t InitializePcmPlayer(uint32_t PlayerIdx);
    void           TerminatePcmPlayers();
    void           DeployPcmPlayersUnderrunRecovery();
    PlayerStatus_t MapPcmPlayersSamples(uint32_t SampleCount, bool NonBlock);
    PlayerStatus_t CommitPcmPlayersMappedSamples();
    PlayerStatus_t GetAndUpdateDisplayTimeOfNextCommit();

    PlayerStatus_t StartPcmPlayers();
    PlayerStatus_t UpdatePcmPlayersParameters();

    PlayerStatus_t AllocateInterMixerRings(unsigned int numberOfBuffer, unsigned int bufferSize);
    void FreeInterMixerRing(RingGeneric_c *Ring);
    void FreeInterMixerRings();

    PlayerStatus_t MMETransformerSanityCheck(const char *transformerName);
    PlayerStatus_t InitializeMMETransformer(MME_TransformerInitParams_t *initParams, bool isFirstMixer);
    PlayerStatus_t TerminateMMETransformer(MME_TransformerHandle_t MMEHdl);
    PlayerStatus_t TerminateMMETransformers(void);
    PlayerStatus_t SendMMEMixCommand();
    PlayerStatus_t WaitForMMECallback();
    PlayerStatus_t UpdateMixerParameters();

    void ResetOutputConfiguration();
    void ResetMixingMetadata();

    void ResetMixerSettings();

    void FillOutGlobalPcmInputMixerConfig(MME_LxMixerInConfig_t *InConfig);
    void FillOutGlobalCodedInputMixerConfig(MME_LxMixerInConfig_t *InConfig, bool *IsOutputBypassed);
    void FillOutGlobalInteractiveInputMixerConfig(MME_LxMixerInConfig_t *InConfig);
    void FillOutGlobalGeneratorInputMixerConfig(MME_LxMixerInConfig_t *InConfig);
    void FillOutGlobalInputGainConfig(MME_LxMixerGainSet_t *InGainConfig);
    void FillOutGlobalInputPanningConfig(MME_LxMixerPanningSet_t *InPanningConfig);
    void FillOutGlobalInteractiveAudioConfig(MME_LxMixerInIAudioConfig_t *InIaudioConfig);
    void FillOutGlobalOutputTopology(MME_LxMixerOutTopology_t *OutTopology);

    void UpdateOutputTopology(MME_LxMixerOutTopology_t *OutTopology, int BypassPlayerId, int BypassChannelCount);

    void FillOutTransformerGlobalParameters(MME_LxMixerBDTransformerGlobalParams_Extended_t *GlobalParams);
    void FillOutFirstMixerGlobalParameters(MME_LxMixerBDTransformerGlobalParams_Extended_t *GlobalParams);

    uint32_t LookupPcmPlayerOutputSamplingFrequency(PcmPlayer_c::OutputEncoding OutputEncoding, uint32_t MixerPlayerIndex) const;

    uint32_t LookupIec60958FrameRate(PcmPlayer_c::OutputEncoding Encoding) const;

    void FillOutDeviceDownmixParameters(MME_LxPcmPostProcessingGlobalParameters_Frozen_t &PcmParams,
                                        uint32_t MixerPlayerIndex,
                                        bool EnableDMix,
                                        int RegionType);
    void FillOutDeviceSpdifParameters(MME_LxPcmPostProcessingGlobalParameters_Frozen_t &PcmParams,
                                      uint32_t MixerPlayerIndex,
                                      PcmPlayer_c::OutputEncoding OutputEncoding);
    void MMESetFatpipeConfig(MME_LxPcmPostProcessingGlobalParameters_Frozen_t &PcmParams,
                             uint32_t MixerPlayerIndex,
                             PcmPlayer_c::OutputEncoding OutputEncoding,
                             AudioOriginalEncoding_t OriginalEncoding);

    void MMESetSpdifConfig(MME_LxPcmPostProcessingGlobalParameters_Frozen_t &PcmParams,
                           uint32_t MixerPlayerIndex,
                           PcmPlayer_c::OutputEncoding OutputEncoding,
                           AudioOriginalEncoding_t OriginalEncoding);

    PlayerStatus_t FillOutDevicePcmParameters(MME_LxPcmPostProcessingGlobalParameters_Frozen_t &PcmParams,
                                              uint32_t MixerPlayerIndex,
                                              int ApplicationType,
                                              int RegionType);
    PlayerStatus_t FillCommonChainPcmParameters(uint8_t *ptr, uint32_t &size, int ApplicationType);

    void ResetBufferTypeFlags();

    PlayerStatus_t FillOutMixCommand();
    PlayerStatus_t FillOutOutputBuffers();
    PlayerStatus_t FillOutFirstMixerOutputBuffers();
    void SetMixerCodedInputStopCommand(bool updateRequired);
    void DumpMixerCodedInput();
    void FillOutFirstMixerInputBuffer(uint32_t ManifestorId, bool FillSilentBuffer);
    void FillOutSecondMixerInputBuffer();
    void FillOutInputBuffer(uint32_t ManifestorId, bool FillSilentBuffer);
    void FillOutAudioGenBuffer(uint32_t Id);
    void FillOutInteractiveAudioBuffer(MME_DataBuffer_t *DataBuffer, int InteractiveAudioIdx);
    void FillOutSilentBuffer(MME_DataBuffer_t *DataBuffer,
                             tMixerFrameParams *MixerFrameParams = NULL);

    PlayerStatus_t UpdateFirstMixerOutputBuffers();
    PlayerStatus_t UpdateOutputBuffers();

    void UpdateInputBuffer(uint32_t ManifestorId, bool ReleaseProcessingBuffer = false);
    void UpdateSecondMixerInputBuffer();
    void UpdateAudioGenBuffer(uint32_t AudioGenId);
    void UpdateInteractiveAudioBuffer(uint32_t AudioGenId);
    void UpdateMixingMetadata();

    static uint32_t LookupRepetitionPeriod(PcmPlayer_c::OutputEncoding Encoding);
    static enum eFsRange TranslateIntegerSamplingFrequencyToRange(uint32_t IntegerFrequency);
    static const char *LookupPolicyValueAudioApplicationType(int ApplicationType);
    static const char *LookupAudioOriginalEncoding(AudioOriginalEncoding_t AudioOriginalEncoding);
    static const char *LookupDiscreteSamplingFrequency(enum eAccFsCode DiscreteFrequency);
    static const char *LookupDiscreteSamplingFrequencyRange(enum eFsRange DiscreteRange);
    static uint32_t LookupSpdifPreamblePc(PcmPlayer_c::OutputEncoding Encoding);
    static const char *LookupAudioMode(enum eAccAcMode DiscreteMode);
    static enum eAccAcMode TranslateChannelAssignmentToAudioMode(struct stm_se_audio_channel_assignment ChannelAssignment);
    static struct stm_se_audio_channel_assignment TranslateAudioModeToChannelAssignment(enum eAccAcMode AudioMode);
    static enum eAccAcMode TranslateDownstreamCardToMainAudioMode(const struct snd_pseudo_mixer_downstream_card &DownstreamCard);
    static enum eAccAcMode TranslateDownstreamCardToAuxAudioMode(const struct snd_pseudo_mixer_downstream_card &DownstreamCard);
    static enum eAccAcMode LookupFatPipeOutputMode(enum eAccAcMode InputMode);
    static bool CheckDownmixBypass(const struct snd_pseudo_mixer_downstream_card &DownstreamCard);

    void CleanUpOnError();

    void GetOutputParameters(OutputTimerParameterBlock_t &ParameterBlockOT)
    {
        // Prepare the A/V offset command
        ParameterBlockOT.ParameterType = OutputTimerSetNormalizedTimeOffset;
        ParameterBlockOT.Offset.Value  = MixerSettings.OutputConfiguration.master_latency * 1000; // Value is in microseconds
    }

    void GetDownmixParameters(CodecParameterBlock_t &ParameterBlockDownmix)
    {
        // Prepare the downmix command
        ParameterBlockDownmix.ParameterType = CodecSpecifyDownmix;
        ParameterBlockDownmix.Downmix.StreamDrivenDownmix = MixerSettings.MixerOptions.Stream_driven_downmix_enable;
    }

    Mixer_Client_c::MixerClientParameters GetClientParameters(void)
    {
        Mixer_Client_c::MixerClientParameters params;
        // Prepare the A/V offset command
        GetOutputParameters(params.ParameterBlockOT);

        SE_DEBUG(group_mixer,  "Master A/V sync offset is %lld us\n", params.ParameterBlockOT.Offset.Value);

        // Prepare the downmix command
        GetDownmixParameters(params.ParameterBlockDownmix);
        SE_DEBUG(group_mixer, "Downmix parameters: StreamDrivenDownmix=%d\n",
                 params.ParameterBlockDownmix.Downmix.StreamDrivenDownmix);
        return params;
    }

    void CheckAndStartClients(void);

    PlayerStatus_t AttachPlayer(MixerRequest_c &aMixerRequest_c);
    PlayerStatus_t AttachPlayer(Audio_Player_c *Audio_Player);


    PlayerStatus_t DetachPlayer(MixerRequest_c &aMixerRequest_c);
    PlayerStatus_t DetachPlayer(Audio_Player_c *Audio_Player);

    PlayerStatus_t GetProcessingRingBufCount(MixerRequest_c &aMixerRequest_c);
    PlayerStatus_t GetProcessingRingBufCount(Manifestor_AudioKsound_c *Manifestor, int *bufCountPtr);

    PlayerStatus_t RegisterManifestor(MixerRequest_c &aMixerRequest_c);
    PlayerStatus_t RegisterManifestor(Manifestor_AudioKsound_c *Manifestor,
                                      class HavanaStream_c *Stream,
                                      stm_se_sink_input_port_t input_port);

    PlayerStatus_t DeRegisterManifestor(MixerRequest_c &aMixerRequest_c);
    PlayerStatus_t DeRegisterManifestor(Manifestor_AudioKsound_c *Manifestor);


    PlayerStatus_t EnableManifestor(MixerRequest_c &aMixerRequest_c);
    PlayerStatus_t EnableManifestor(Manifestor_AudioKsound_c *Manifestor);


    PlayerStatus_t DisableManifestor(MixerRequest_c &aMixerRequest_c);
    PlayerStatus_t DisableManifestor(Manifestor_AudioKsound_c *Manifestor);


    PlayerStatus_t CheckClientsState(MixerRequest_c &aMixerRequest);
    PlayerStatus_t CheckClientsState();

    void SetStoppingClientsToStoppedState();
    void ReleaseNonQueuedBuffersOfStoppingClients();

    ////////////////////////////////////////////////////////////////////////////
    ///
    /// Specification: it returns true if all the clients are in the aState
    /// state and false otherwise.
    ///
    inline bool AllClientsAreInState(InputState_t aState)
    {
        bool allClientInTheState = true;

        for (uint32_t ClientIdx(0); (ClientIdx < MIXER_MAX_CLIENTS) && allClientInTheState; ClientIdx++)
        {
            if (Client[ClientIdx].GetState() != aState)
            {
                allClientInTheState = false;
            }
        }

        return allClientInTheState;
    }

    ////////////////////////////////////////////////////////////////////////////
    ///
    /// Specification: it returns true if all connected clients are in the aState
    /// state and false otherwise.
    ///
    inline bool AllConnectedClientsAreInState(InputState_t aState)
    {
        bool allClientInTheState = true;

        for (uint32_t ClientIdx(0); (ClientIdx < MIXER_MAX_CLIENTS) && allClientInTheState; ClientIdx++)
        {
            if ((Client[ClientIdx].GetState() != aState) && (Client[ClientIdx].GetState() != DISCONNECTED))
            {
                allClientInTheState = false;
            }
        }

        return allClientInTheState;
    }

    bool AllAudioGenIAudioInputsAreDisconnected(void);

    ////////////////////////////////////////////////////////////////////////////
    ///
    /// Specification: it returns true if there is at least one client in the
    /// aState state and false otherwise.
    ///
    inline bool ThereIsOneClientInState(InputState_t aState)
    {
        bool found = false;

        for (uint32_t ClientIdx(0); (ClientIdx < MIXER_MAX_CLIENTS) && !found; ClientIdx++)
        {
            if (Client[ClientIdx].GetState() == aState)
            {
                found = true;
            }
        }

        return found;
    }


    bool ThereIsOneAudioGenIAudioInputInState(stm_se_audio_generator_state_t aState);

    inline bool CheckAndUpdateAllAudioPlayers()
    {
        bool Status(false);

        //SE_DEBUG(group_mixer, ">\n");

        for (uint32_t CountOfAudioPlayerAttached(0), PlayerIdx(0); CountOfAudioPlayerAttached < NumberOfAudioPlayerAttached; PlayerIdx++)
        {
            if (true == MixerPlayer[PlayerIdx].IsPlayerObjectAttached())
            {
                CountOfAudioPlayerAttached++;

                if (true == MixerPlayer[PlayerIdx].CheckAndUpdatePlayerObject())
                {
                    Status = true;
                    SE_DEBUG(group_mixer, "<%s returning true\n", &Name[0]);
                }
            }
        }

        //SE_DEBUG(group_mixer, "< returning %s\n", (true == Status) ? "true" : "false");
        return Status;
    }

    /// Is allowed to be called only in PlaybackThread() context.
    inline bool IsThisClientRequestedToBeBypassed(uint32_t ClientId, bool &BypassSpdif, bool &BypassHdmi,
                                                  bool &BypassSDSpdif, bool &BypassSDHdmi)
    {
        bool Result(false);
        BypassSpdif = false;
        BypassHdmi = false;
        BypassSDSpdif = false;
        BypassSDHdmi = false;

        if (PrimaryClient == ClientId)
        {
            for (uint32_t CountOfAudioPlayerAttached(0), PlayerIdx(0); CountOfAudioPlayerAttached < NumberOfAudioPlayerAttached; PlayerIdx++)
            {
                if (true == MixerPlayer[PlayerIdx].IsPlayerObjectAttached())
                {
                    CountOfAudioPlayerAttached++;

                    if (true == MixerPlayer[PlayerIdx].IsBypass())
                    {
                        Result = true;

                        if (true == MixerPlayer[PlayerIdx].IsConnectedToSpdif())
                        {
                            BypassSpdif = true;
                            BypassSDSpdif = MixerPlayer[PlayerIdx].IsBypassSD() ? true : false;
                        }
                        else if (true == MixerPlayer[PlayerIdx].IsConnectedToHdmi())
                        {
                            BypassHdmi = true;
                            BypassSDHdmi = MixerPlayer[PlayerIdx].IsBypassSD() ? true : false;
                        }
                    }
                }
            }
        }

        SE_VERBOSE(group_mixer, "< %d is %s (BypassSpdif:%s; BypassHdmi:%s BypassSDSpdif:%s BypassSDHdmi:%s)\n",
                   ClientId,
                   Result        ? "true" : "false",
                   BypassSpdif   ? "true" : "false",
                   BypassHdmi    ? "true" : "false",
                   BypassSDSpdif ? "true" : "false",
                   BypassSDHdmi  ? "true" : "false");
        return Result;
    }

    DISALLOW_COPY_AND_ASSIGN(Mixer_Mme_c);
};

#endif // H_MIXER_MME_CLASS
