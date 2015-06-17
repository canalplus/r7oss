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

#include "acc_mme.h"

#include "st_relayfs_se.h"
#include "player_threads.h"

#include "mixer_mme.h"
#include "mixer_transformer.h"
#include "player_fadepan_mapping.h"

#include "codec_mme_base.h"

#undef TRACE_TAG
#define TRACE_TAG   "Mixer_Mme_c"

#define MIXER_MAX_WAIT_FOR_MME_COMMAND_COMPLETION  100     /* Ms */

#define MIXER_NB_OF_ITERATION_WITH_ERROR_BEFORE_STOPPING 3

// Number of mixer thread loops to wait before entering low power:
// 1. soft mute
// 2. zero all the periods
#define MIXER_NUMBER_OF_THREAD_LOOPS_BEFORE_LOW_POWER_ENTER     (MIXER_NUM_PERIODS + 1)

//#define SPLIT_AUX  ACC_DONT_SPLIT
#define SPLIT_AUX  ACC_SPLIT_AUTO

static const uint32_t MIXER_SAMPLING_FREQUENCY_STKPI_UNSET(0);

static const int32_t MIXER_MME_NO_ACTIVE_PLAYER(-1);

// Must be less than DEFAULT_MIXER_REQUEST_MANAGER_TIMEOUT
static const uint32_t MIXER_THREAD_SURFACE_PARAMETER_UPDATE_EVENT_TIMEOUT(3 * 1000);

/* re-map channels between ALSA and the audio firmware (swap 2,3 with 4,5) */
static const uint32_t remap_channels[SND_PSEUDO_MIXER_CHANNELS] =
{
    ACC_MAIN_LEFT,
    ACC_MAIN_RGHT,
    ACC_MAIN_LSUR,
    ACC_MAIN_RSUR,
    ACC_MAIN_CNTR,
    ACC_MAIN_LFE,
    ACC_MAIN_CSURL,
    ACC_MAIN_CSURR
};

static const  struct stm_se_audio_channel_assignment default_speaker_config =
{
    STM_SE_AUDIO_CHANNEL_PAIR_NOT_CONNECTED,
    STM_SE_AUDIO_CHANNEL_PAIR_NOT_CONNECTED,
    STM_SE_AUDIO_CHANNEL_PAIR_NOT_CONNECTED,
    STM_SE_AUDIO_CHANNEL_PAIR_NOT_CONNECTED,
    STM_SE_AUDIO_CHANNEL_PAIR_NOT_CONNECTED,
    0,
    false
};

static OS_TaskEntry(PlaybackThreadStub)
{
    Mixer_Mme_c *Mixer = static_cast<Mixer_Mme_c *>(Parameter);
    Mixer->PlaybackThread();
    OS_TerminateThread();
    return NULL;
}

// ToDO : tune TVOuput_GainAdjust, AVROutput_GainAdjust and HEADPHONEOutput_GainAdjust values in table below
//
typedef struct
{
    enum eAccBoolean  AdjustPRL;
    I32               PRL_TV;         // value in mB
    I32               PRL_AVR;        // value in mB
    I32               PRL_HeadPhone;  // value in mB
} ProgramReferenceLevelPreset_t;


// CAUTION: MUST correspond to: PolicyValueAudioApplicationXXX definitions.
#define NB_AUDIO_APPLICATION_TYPES (STM_SE_CTRL_VALUE_LAST_AUDIO_APPLICATION_TYPE + 1)

static const ProgramReferenceLevelPreset_t LimiterGainAdjustLookupTable[NB_AUDIO_APPLICATION_TYPES] =
{
    /*AdjustProgramRefLevel  PRL_TV    PRL_AVR    PRL_HeadPhone */
    { ACC_MME_FALSE,         0,          0,             0 }, // STM_SE_CTRL_VALUE_AUDIO_APPLICATION_ISO
    { ACC_MME_TRUE ,     -3100,      -3100,         -3100 }, // STM_SE_CTRL_VALUE_AUDIO_APPLICATION_DVD
    { ACC_MME_TRUE ,     -2300,      -3100,         -2300 }, // STM_SE_CTRL_VALUE_AUDIO_APPLICATION_DVB
    { ACC_MME_TRUE ,     -2300,      -3100,         -2300 }, // STM_SE_CTRL_VALUE_AUDIO_APPLICATION_MS10
    { ACC_MME_TRUE ,     -2300,      -3100,         -2300 }, // STM_SE_CTRL_VALUE_AUDIO_APPLICATION_MS11
    { ACC_MME_TRUE ,         0,          0,             0 }, // STM_SE_CTRL_VALUE_AUDIO_APPLICATION_MS12
    { ACC_MME_FALSE,         0,          0,             0 }  // STM_SE_CTRL_VALUE_AUDIO_APPLICATION_HDMIRX_GAME_MODE
};
////////////////////////////////////////////////////////////////////////////
///
/// Stub function with C linkage to connect the MME callback to the manifestor.
///
static void MMEFirstMixerCallbackStub(MME_Event_t      Event,
                                      MME_Command_t   *CallbackData,
                                      void            *UserData)
{
    Mixer_Mme_c *Self = static_cast<Mixer_Mme_c *>(UserData);
    bool isFirstMixer = true;
    Self->CallbackFromMME(Event, CallbackData, isFirstMixer);
}
////////////////////////////////////////////////////////////////////////////
///
/// Stub function with C linkage to connect the MME callback to the manifestor.
///
static void MMECallbackStub(MME_Event_t      Event,
                            MME_Command_t   *CallbackData,
                            void            *UserData)
{
    Mixer_Mme_c *Self = static_cast<Mixer_Mme_c *>(UserData);
    bool isFirstMixer = false;
    Self->CallbackFromMME(Event, CallbackData, isFirstMixer);
}

////////////////////////////////////////////////////////////////////////////
///
///
///
Mixer_Mme_c::Mixer_Mme_c(const char *Name, char *TransformerName, size_t TransformerNameSize, stm_se_mixer_spec_t Topology)
    : Mixer_c()
    , Name()
    , mTopology(Topology)
    , Client()
    , MixerRequestManager()
    , PlaybackThreadRunning(false)
    , mPlaybackThreadOnOff()
    , LowPowerMode(STM_SE_LOW_POWER_MODE_HPS)
    , IsLowPowerState(false)
    , IsLowPowerMMEInitialized(false)
    , LowPowerEnterEvent()
    , LowPowerExitEvent()
    , LowPowerExitCompletedEvent()
    , LowPowerEnterThreadLoop(0)
    , LowPowerPostMixGainSave(0)
    , InLowPowerState(false)
    , ThreadState(Idle)
    , PcmPlayersStarted(false)
    , mSPDIFCardId(STM_SE_MIXER_NB_MAX_OUTPUTS)
    , mHDMICardId(STM_SE_MIXER_NB_MAX_OUTPUTS)
    , WaitNCommandForAudioToAudioSync()
    , MMETransformerName(TransformerName)
    , MMETransformerNameSize(TransformerNameSize)
    , MMEHandle(MME_INVALID_ARGUMENT)
    , MMEFirstMixerHdl(MME_INVALID_ARGUMENT)
    , LowPowerWakeUpFirstMixer(false)
    , MMEInitParams()
    , FirstMixerMMEInitParams()
    , MMEInitialized(false)
    , MMECallbackSemaphore()
    , MMEFirstMixerCallbackSemaphore()
    , MMEParamCallbackSemaphore()
    , InterMixerFreeBufRing(NULL)
    , InterMixerFillBufRing(NULL)
    , MMECallbackThreadBoosted(false)
    , MMENeedsParameterUpdate(false)
    , InitializedSamplingFrequency(MIXER_SAMPLING_FREQUENCY_STKPI_UNSET)
    , MixerSamplingFrequency(0)
    , NominalOutputSamplingFrequency(0)
    , MasterOutputGrain(SND_PSEUDO_MIXER_DEFAULT_GRAIN)
    , MixerGranuleSize(0)
    , MasterClientIndex(0)
    , PrimaryCodedDataType()
    , UpstreamConfiguration(NULL)
    , MixerSettings()
    , DownmixFirmware(NULL)
    , NumberOfAudioPlayerAttached(0)
    , NumberOfAudioPlayerInitialized(0)
    , NumberOfAudioGeneratorAttached(0)
    , NumberOfInteractiveAudioAttached(0)
    , FirstActivePlayer(MIXER_MME_NO_ACTIVE_PLAYER)
    , PcmPlayerSurfaceParametersUpdated()
    , PcmPlayerNeedsParameterUpdate(false)
    , PrimaryClient(0)
    , SecondaryClient(1)
    , MixerParams()
    , AudioConfiguration()
    , FirstMixerCommand()
    , MixerCommand()
    , ParamsCommand()
    , FirstMixerParamsCommand()
    , CurrentOutputPcmParams()
    , MixerPlayer()
    , Generator()
    , InteractiveAudio()
    , FirstMixerSamplingFrequencyHistory(0)
    , MixerSamplingFrequencyHistory(0)
    , FirstMixerSamplingFrequencyUpdated(true)
    , MixerSamplingFrequencyUpdated(true)
    , OutputTopologyUpdated(true)
    , FirstMixerClientAudioParamsUpdated(true)
    , ClientAudioParamsUpdated(true)
    , AudioGeneratorUpdated(true)
    , IAudioUpdated(true)
    , MixCommandStartTime(0)
{
    strncpy(&this->Name[0], Name, sizeof(this->Name));
    this->Name[sizeof(this->Name) - 1] = '\0';

    if (BaseComponentClass_c::InitializationStatus != PlayerNoError)
    {
        SE_ERROR("Some troubles with InitializationStatus\n");
        return;
    }

    OS_InitializeEvent(&mPlaybackThreadOnOff);
    OS_InitializeEvent(&PcmPlayerSurfaceParametersUpdated);
    OS_InitializeEvent(&LowPowerEnterEvent);
    OS_InitializeEvent(&LowPowerExitEvent);
    OS_InitializeEvent(&LowPowerExitCompletedEvent);
    OS_SemaphoreInitialize(&MMECallbackSemaphore, 0);
    OS_SemaphoreInitialize(&MMEFirstMixerCallbackSemaphore, 0);
    OS_SemaphoreInitialize(&MMEParamCallbackSemaphore, 1);

    UpdateTransformerId(TransformerName);

    AudioConfiguration.CRC = false;
    ResetMixerSettings();
    ResetOutputConfiguration();

    for (uint32_t CodedIndex(0); CodedIndex < MIXER_MAX_CODED_INPUTS; CodedIndex++)
    {
        PrimaryCodedDataType[CodedIndex] = PcmPlayer_c::OUTPUT_IEC60958;
    }

    Reset();  // TODO(pht) ctor shall work without Reset => factorize code for both ctor and Reset usage

    OS_ResetEvent(&mPlaybackThreadOnOff);

    StartPlaybackThread();

    // wait for the thread to come to rest
    OS_WaitForEventAuto(&mPlaybackThreadOnOff, OS_INFINITE);

    // Once this mixer object has been created, MW can start attaching clients / players to it
    // this attachment relies on the read of Mixer_Request_Manager_c::ThreadState
    OS_Smp_Mb(); // Read memory barrier: rmb_for_Mixer_Starting coupled with: wmb_for_Mixer_Starting

    SE_DEBUG(group_mixer,  "Successful construction of %s\n", Name);
}


////////////////////////////////////////////////////////////////////////////
///
///
///
Mixer_Mme_c::~Mixer_Mme_c()
{
    TerminatePlaybackThread();

    OS_TerminateEvent(&PcmPlayerSurfaceParametersUpdated);
    OS_TerminateEvent(&mPlaybackThreadOnOff);
    OS_TerminateEvent(&LowPowerEnterEvent);
    OS_TerminateEvent(&LowPowerExitEvent);
    OS_TerminateEvent(&LowPowerExitCompletedEvent);
    OS_SemaphoreTerminate(&MMECallbackSemaphore);
    OS_SemaphoreTerminate(&MMEFirstMixerCallbackSemaphore);
    OS_SemaphoreTerminate(&MMEParamCallbackSemaphore);
}


////////////////////////////////////////////////////////////////////////////
///
///
///
PlayerStatus_t Mixer_Mme_c::Halt()
{
    SE_DEBUG(group_mixer, ">%s\n", Name);

    if (MMEInitialized)
    {
        if (PlayerNoError != TerminateMMETransformers())
        {
            SE_ERROR("Failed to terminated mixer transformer\n");
            // no error recovery possible
        }
    }

    // Free Hw players if some.
    TerminatePcmPlayers();
    return Mixer_c::Halt();
}


////////////////////////////////////////////////////////////////////////////
///
///
///
PlayerStatus_t Mixer_Mme_c::Reset()
{
    SE_DEBUG(group_mixer, ">%s\n", Name);

    // Do not reset MixerPlayer[] because can still be attached at current moment.

    for (uint32_t PlayerIdx(0); PlayerIdx < STM_SE_MIXER_NB_MAX_OUTPUTS; PlayerIdx++)
    {
        MixerPlayer[PlayerIdx].ResetPcmPlayerConfig();
    }

    PcmPlayerNeedsParameterUpdate = false;
    MMEHandle = MME_INVALID_ARGUMENT;
    MMEFirstMixerHdl = MME_INVALID_ARGUMENT;
    LowPowerWakeUpFirstMixer = false;
    MMEInitialized = false;
    InterMixerFreeBufRing = NULL;
    InterMixerFillBufRing = NULL;
    // MMECallbackThreadBoosted is undefined
    MMENeedsParameterUpdate = false;
    MasterClientIndex = 0;
    InitializedSamplingFrequency = MIXER_SAMPLING_FREQUENCY_STKPI_UNSET;
    MixerSamplingFrequency = 0;
    NominalOutputSamplingFrequency = 0;
    MixerGranuleSize = 0;
    //History Variables
    FirstMixerSamplingFrequencyHistory = 0;
    MixerSamplingFrequencyHistory = 0;
    MixerSamplingFrequencyUpdated = true;
    FirstMixerSamplingFrequencyUpdated = true;
    OutputTopologyUpdated = true;
    FirstMixerClientAudioParamsUpdated = true;
    ClientAudioParamsUpdated = true;
    AudioGeneratorUpdated = true;
    IAudioUpdated = true;
    WaitNCommandForAudioToAudioSync = 0; // Will be set when the clients are connected

    for (uint32_t CodedIndex(0); CodedIndex < MIXER_MAX_CODED_INPUTS; CodedIndex++)
    {
        PrimaryCodedDataType[CodedIndex] = PcmPlayer_c::OUTPUT_IEC60958;
    }

    ResetMixingMetadata();
    // Do not touch other parts of the OutputConfiguration. These are immune to reset.
    // Do not touch the DownmixFirmware. It is immune to reset.
    PrimaryClient   = 0;
    SecondaryClient = 1;
    //
    // Pre-configure the mixer command
    //
    memset(&MixerCommand, 0, sizeof(MixerCommand));
    MixerCommand.Command.StructSize = sizeof(MixerCommand.Command);
    MixerCommand.Command.CmdCode = MME_TRANSFORM;
    MixerCommand.Command.CmdEnd = MME_COMMAND_END_RETURN_NOTIFY;
    MixerCommand.Command.DueTime = 0;   // no sorting by time
    MixerCommand.Command.NumberInputBuffers = MIXER_AUDIO_MAX_INPUT_BUFFERS;
    // MixerCommand.Command.NumberOutputBuffers is not constant and must be set later
    MixerCommand.Command.DataBuffers_p = MixerCommand.DataBufferList;
    MixerCommand.Command.CmdStatus.AdditionalInfoSize = sizeof(MME_LxMixerTransformerFrameMix_ExtendedParams_t);
    MixerCommand.Command.CmdStatus.AdditionalInfo_p = &MixerCommand.OutputParams;
    MixerCommand.Command.ParamSize = sizeof(MixerCommand.InputParams);
    MixerCommand.Command.Param_p = &MixerCommand.InputParams;

    for (uint32_t i = 0; i < MIXER_AUDIO_MAX_BUFFERS; i++)
    {
        MixerCommand.DataBufferList[i] = &MixerCommand.DataBuffers[i];
        MixerCommand.DataBuffers[i].StructSize = sizeof(MixerCommand.DataBuffers[i]);
        MixerCommand.DataBuffers[i].UserData_p = &MixerCommand.BufferIndex[i * MIXER_AUDIO_PAGES_PER_BUFFER];
        MixerCommand.DataBuffers[i].ScatterPages_p = &MixerCommand.ScatterPages[i * MIXER_AUDIO_PAGES_PER_BUFFER];
        //MixerCommand.StreamNumber = i;
    }

    for (uint32_t i = 0; i < MIXER_AUDIO_MAX_PAGES; i++)
    {
        MixerCommand.BufferIndex[i] = NULL;
    }

    //
    // Pre-configure the mixer command
    //
    memset(&FirstMixerCommand, 0, sizeof(FirstMixerCommand));
    FirstMixerCommand.Command.StructSize = sizeof(FirstMixerCommand.Command);
    FirstMixerCommand.Command.CmdCode = MME_TRANSFORM;
    FirstMixerCommand.Command.CmdEnd = MME_COMMAND_END_RETURN_NOTIFY;
    FirstMixerCommand.Command.DueTime = 0;   // no sorting by time
    FirstMixerCommand.Command.NumberInputBuffers = MIXER_MAX_PCM_INPUTS;
    FirstMixerCommand.Command.NumberOutputBuffers = FIRST_MIXER_MAX_OUTPUT_BUFFERS;
    FirstMixerCommand.Command.DataBuffers_p = FirstMixerCommand.DataBufferList;
    FirstMixerCommand.Command.CmdStatus.AdditionalInfoSize = sizeof(MME_LxMixerTransformerFrameMix_ExtendedParams_t);
    FirstMixerCommand.Command.CmdStatus.AdditionalInfo_p = &FirstMixerCommand.OutputParams;
    FirstMixerCommand.Command.ParamSize = sizeof(FirstMixerCommand.InputParams);
    FirstMixerCommand.Command.Param_p = &FirstMixerCommand.InputParams;

    for (uint32_t i = 0; i < FIRST_MIXER_MAX_BUFFERS; i++)
    {
        FirstMixerCommand.DataBufferList[i] = &FirstMixerCommand.DataBuffers[i];
        FirstMixerCommand.DataBuffers[i].StructSize = sizeof(FirstMixerCommand.DataBuffers[i]);
        FirstMixerCommand.DataBuffers[i].UserData_p = &FirstMixerCommand.BufferIndex[i * MIXER_AUDIO_PAGES_PER_BUFFER];
        FirstMixerCommand.DataBuffers[i].ScatterPages_p = &FirstMixerCommand.ScatterPages[i * MIXER_AUDIO_PAGES_PER_BUFFER];
        //FirstMixerCommand.StreamNumber = i;
    }

    for (uint32_t i = 0; i < FIRST_MIXER_MAX_PAGES; i++)
    {
        FirstMixerCommand.BufferIndex[i] = NULL;
    }

    //
    // Pre-configure the parameter update command
    //
    memset(&ParamsCommand, 0, sizeof(ParamsCommand));
    ParamsCommand.Command.StructSize = sizeof(ParamsCommand.Command);
    ParamsCommand.Command.CmdCode = MME_SET_GLOBAL_TRANSFORM_PARAMS;
    ParamsCommand.Command.CmdEnd = MME_COMMAND_END_RETURN_NOTIFY;
    ParamsCommand.Command.DueTime = 0;  // no sorting by time
    ParamsCommand.Command.NumberInputBuffers = 0;
    ParamsCommand.Command.NumberOutputBuffers = 0;
    ParamsCommand.Command.DataBuffers_p = NULL;
    ParamsCommand.Command.CmdStatus.State = MME_COMMAND_COMPLETED;
    ParamsCommand.Command.CmdStatus.AdditionalInfoSize = sizeof(ParamsCommand.OutputParams);
    ParamsCommand.Command.CmdStatus.AdditionalInfo_p = &ParamsCommand.OutputParams;
    ParamsCommand.Command.ParamSize = sizeof(ParamsCommand.InputParams);
    ParamsCommand.Command.Param_p = &ParamsCommand.InputParams;

    memset(&FirstMixerParamsCommand, 0, sizeof(FirstMixerParamsCommand));
    FirstMixerParamsCommand.Command.StructSize = sizeof(FirstMixerParamsCommand.Command);
    FirstMixerParamsCommand.Command.CmdCode = MME_SET_GLOBAL_TRANSFORM_PARAMS;
    FirstMixerParamsCommand.Command.CmdEnd = MME_COMMAND_END_RETURN_NOTIFY;
    FirstMixerParamsCommand.Command.DueTime = 0;  // no sorting by time
    FirstMixerParamsCommand.Command.NumberInputBuffers = 0;
    FirstMixerParamsCommand.Command.NumberOutputBuffers = 0;
    FirstMixerParamsCommand.Command.DataBuffers_p = NULL;
    FirstMixerParamsCommand.Command.CmdStatus.State = MME_COMMAND_COMPLETED;
    FirstMixerParamsCommand.Command.CmdStatus.AdditionalInfoSize = sizeof(FirstMixerParamsCommand.OutputParams);
    FirstMixerParamsCommand.Command.CmdStatus.AdditionalInfo_p = &FirstMixerParamsCommand.OutputParams;
    FirstMixerParamsCommand.Command.ParamSize = sizeof(FirstMixerParamsCommand.InputParams);
    FirstMixerParamsCommand.Command.Param_p = &FirstMixerParamsCommand.InputParams;

    return Mixer_c::Reset();
}

////////////////////////////////////////////////////////////////////////////
///
///
///
PlayerStatus_t Mixer_Mme_c::SetModuleParameters(unsigned int ParameterBlockSize, void *ParameterBlock)
{
    SE_DEBUG(group_mixer, ">%s #########################################\n", Name);

    if (ParameterBlockSize == sizeof(snd_pseudo_mixer_settings))
    {
        struct snd_pseudo_mixer_settings *SettingsForMixer = static_cast<struct snd_pseudo_mixer_settings *>(ParameterBlock);

        if (SettingsForMixer->magic == SND_PSEUDO_MIXER_MAGIC)
        {
            UpstreamConfiguration = SettingsForMixer;
            MixerSettings.SetTobeUpdated(*SettingsForMixer);

            // If play-back is not already running, allow output configuration immediately.
            if (PlayerNoError == Mixer_Mme_c::IsIdle())
            {
                PlayerStatus_t Status;
                // Review output configuration.
                MixerSettings.CheckAndUpdate();
                // Review players.
                CheckAndUpdateAllAudioPlayers();
                // Trigger updates to come accordingly (Mixer and PCM Players).
                // Since Idle state of mixer, no thread issue for these triggers.
                PcmPlayerNeedsParameterUpdate = true;
                MMENeedsParameterUpdate = true;
                Status = UpdatePlayerComponentsModuleParameters();

                if (Status != PlayerNoError)
                {
                    SE_ERROR("Failed to update normalized time offset or CodecControls (default value will be used instead)\n");
                }
            }

            return PlayerNoError;
        }
    }

    if (ParameterBlockSize > sizeof(struct snd_pseudo_mixer_downmix_header) && ((char *) ParameterBlock)[0] == 'D')
    {
        struct snd_pseudo_mixer_downmix_header *Header = static_cast<struct snd_pseudo_mixer_downmix_header *>(ParameterBlock);

        if ((SND_PSEUDO_MIXER_DOWNMIX_HEADER_MAGIC == Header->magic) &&
            (SND_PSEUDO_MIXER_DOWNMIX_HEADER_VERSION == Header->version) &&
            (SND_PSEUDO_MIXER_DOWNMIX_HEADER_SIZE(*Header) == ParameterBlockSize))
        {
            // the firmware loader is responsible for validating the index section, by the time we reach
            // this point it is known to be valid.
            DownmixFirmware = reinterpret_cast<DownmixFirmwareStruct *>(Header);
            return PlayerNoError;
        }
    }

    if (ParameterBlockSize == sizeof(struct snd_pseudo_transformer_name))
    {
        struct snd_pseudo_transformer_name *TransformerName = static_cast<struct snd_pseudo_transformer_name *>(ParameterBlock);

        if (TransformerName->magic == SND_PSEUDO_TRANSFORMER_NAME_MAGIC)
        {
            return UpdateTransformerId(TransformerName->name);
        }
    }

    return Mixer_c::SetModuleParameters(ParameterBlockSize, ParameterBlock);
}

///////////////////////////////////////////////////////////////////////////
///
///
///
PlayerStatus_t Mixer_Mme_c::SetOption(stm_se_ctrl_t ctrl, int value)
{
    SE_DEBUG(group_mixer, ">%s (ctrl= %d, value= %d)\n", Name, ctrl, value);
    MixerOptionStruct Options = MixerSettings.GetLastOptions();

    switch (ctrl)
    {
    case STM_SE_CTRL_AUDIO_MIXER_GRAIN:
        Options.MasterGrain = value;
        SE_DEBUG(group_mixer, ">: STM_SE_CTRL_AUDIO_MIXER_GRAIN -> %d\n", value);
        break;

    case STM_SE_CTRL_STREAM_DRIVEN_STEREO:
        Options.Stream_driven_downmix_enable = value;
        SE_DEBUG(group_mixer, "STM_SE_CTRL_STREAM_DRIVEN_STEREO -> %d\n", value);
        break;

    case STM_SE_CTRL_AUDIO_GAIN:
        Options.MasterPlaybackVolume = value;
        SE_DEBUG(group_mixer, "STM_SE_CTRL_AUDIO_GAIN -> %d\n", value);
        break;

    case STM_SE_CTRL_VOLUME_MANAGER_AMOUNT:
        Options.VolumeManagerAmount = value;
        SE_DEBUG(group_mixer, "STM_SE_CTRL_VOLUME_MANAGER_AMOUNT -> %d\n", value);
        break;

    case STM_SE_CTRL_VIRTUALIZER_AMOUNT:
        Options.VirtualizerAmount = value;
        SE_DEBUG(group_mixer, "STM_SE_CTRL_VIRTUALIZER_AMOUNT -> %d\n", value);
        break;

    case STM_SE_CTRL_UPMIXER_AMOUNT:
        Options.UpmixerAmount = value;
        SE_DEBUG(group_mixer, "STM_SE_CTRL_UPMIXER_AMOUNT -> %d\n", value);
        break;

    case STM_SE_CTRL_DIALOG_ENHANCER_AMOUNT:
        Options.DialogEnhancerAmount = value;
        SE_DEBUG(group_mixer, "STM_SE_CTRL_DIALOG_ENHANCER_AMOUNT -> %d\n", value);
        break;

    default:
        SE_ERROR("Invalid control\n");
        return PlayerError;
    }

    Options.DebugDump();
    // Trigger update in mixer thread context.
    MixerSettings.SetTobeUpdated(Options);
    return PlayerNoError;
}

////////////////////////////////////////////////////////////////////////////
///
///
///
PlayerStatus_t Mixer_Mme_c::GetOption(stm_se_ctrl_t ctrl, int &value)  /*const*/
{
    SE_VERBOSE(group_mixer, ">%s (ctrl= %d)\n", Name, ctrl);
    const MixerOptionStruct Options = MixerSettings.GetLastOptions();

    switch (ctrl)
    {
    case STM_SE_CTRL_AUDIO_MIXER_GRAIN:
        value = Options.MasterGrain;
        SE_DEBUG(group_mixer, "<: STM_SE_CTRL_AUDIO_MIXER_GRAIN %d\n", value);
        break;

    case STM_SE_CTRL_STREAM_DRIVEN_STEREO:
        value = Options.Stream_driven_downmix_enable;
        SE_DEBUG(group_mixer, "<: STM_SE_CTRL_STREAM_DRIVEN_STEREO %d\n", value);
        break;

    case STM_SE_CTRL_AUDIO_GAIN:
        value = Options.MasterPlaybackVolume;
        SE_DEBUG(group_mixer, "<: STM_SE_CTRL_AUDIO_GAIN %d\n", value);
        break;

    case STM_SE_CTRL_VOLUME_MANAGER_AMOUNT:
        value = Options.VolumeManagerAmount;
        SE_DEBUG(group_mixer, "<: STM_SE_CTRL_VOLUME_MANAGER_AMOUNT %d\n", value);
        break;

    case STM_SE_CTRL_VIRTUALIZER_AMOUNT:
        value = Options.VirtualizerAmount;
        SE_DEBUG(group_mixer, "<: STM_SE_CTRL_VIRTUALIZER_AMOUNT %d\n", value);
        break;

    case STM_SE_CTRL_UPMIXER_AMOUNT:
        value = Options.UpmixerAmount;
        SE_DEBUG(group_mixer, "<: STM_SE_CTRL_UPMIXER_AMOUNT %d\n", value);
        break;

    case STM_SE_CTRL_DIALOG_ENHANCER_AMOUNT:
        value = Options.DialogEnhancerAmount;
        SE_DEBUG(group_mixer, "<: STM_SE_CTRL_DIALOG_ENHANCER_AMOUNT %d\n", value);
        break;

    default:
        SE_ERROR("Invalid control\n");
        return PlayerError;
    }

    return PlayerNoError;
}

////////////////////////////////////////////////////////////////////////////
///
///
///
PlayerStatus_t Mixer_Mme_c::SetCompoundOption(stm_se_ctrl_t ctrl, const void *value)
{
    const stm_se_audio_mixer_value_t *Value = (const stm_se_audio_mixer_value_t *) value;
    MixerOptionStruct Options = MixerSettings.GetLastOptions();
    uint32_t AudioGeneratorIdx;
    SE_DEBUG(group_mixer, ">%s (ctrl= %d)\n", Name, ctrl);

    switch (ctrl)
    {
    case STM_SE_CTRL_OUTPUT_SAMPLING_FREQUENCY:
    {
        Options.OutputSfreq.control   = Value->output_sfreq.control;
        Options.OutputSfreq.frequency = Value->output_sfreq.frequency;
    }
    break;

    case STM_SE_CTRL_SPEAKER_CONFIG:
    {
        Options.SpeakerConfig = Value->speaker_config;
        SE_DEBUG(group_mixer, "%p mixer channel assignment [pair0:%d]-[pair1:%d]-[pair2:%d]-[pair3:%d]-[pair4:%d] malleable=%d\n",
                 this,
                 Options.SpeakerConfig.pair0,
                 Options.SpeakerConfig.pair1,
                 Options.SpeakerConfig.pair2,
                 Options.SpeakerConfig.pair3,
                 Options.SpeakerConfig.pair4,
                 Options.SpeakerConfig.malleable);
    }
    break;

    case STM_SE_CTRL_AUDIO_MIXER_INPUT_COEFFICIENT:
    {
        int port;
        port = LookupClientFromStream((class HavanaStream_c *) Value->input_gain.input);

        /* Note :
           until STLinuxTV is modified to expose the handle of the input object via the input port , we'll
           consider that the port is a basic index */

        switch (port)
        {
        case STM_SE_CTRL_VALUE_AUDIO_MIXER_ROLE_PRIMARY:
        case STM_SE_CTRL_VALUE_AUDIO_MIXER_ROLE_IN3:
        case STM_SE_CTRL_VALUE_AUDIO_MIXER_ROLE_IN4:
            for (uint32_t ChannelIdx(0); ChannelIdx < SND_PSEUDO_MIXER_CHANNELS; ChannelIdx++)
            {
                Options.Gain[PrimaryClient + port][ChannelIdx] = (int)Value->input_gain.gain[remap_channels[ChannelIdx]];
                Options.Pan[PrimaryClient + port][ChannelIdx] = (int)Value->input_gain.panning[remap_channels[ChannelIdx]];
                // Update the flag
                Options.GainUpdated = true;
                Options.PanUpdated = true;
            }

            break;

        case STM_SE_CTRL_VALUE_AUDIO_MIXER_ROLE_SECONDARY:
            for (uint32_t ChannelIdx(0); ChannelIdx < SND_PSEUDO_MIXER_CHANNELS; ChannelIdx++)
            {
                Options.Gain[SecondaryClient][ChannelIdx] = (int)Value->input_gain.gain[remap_channels[ChannelIdx]];
                Options.Pan[SecondaryClient][ChannelIdx] = (int)Value->input_gain.panning[remap_channels[ChannelIdx]];
                // Update the flag
                Options.GainUpdated = true;
                Options.PanUpdated = true;
            }

            break;

        case STM_SE_CTRL_VALUE_MIXER_ROLE_AUDIO_GENERATOR_0:
        case STM_SE_CTRL_VALUE_MIXER_ROLE_AUDIO_GENERATOR_1:
        case STM_SE_CTRL_VALUE_MIXER_ROLE_AUDIO_GENERATOR_2:
        case STM_SE_CTRL_VALUE_MIXER_ROLE_AUDIO_GENERATOR_3:
        case STM_SE_CTRL_VALUE_MIXER_ROLE_AUDIO_GENERATOR_4:
        case STM_SE_CTRL_VALUE_MIXER_ROLE_AUDIO_GENERATOR_5:
        case STM_SE_CTRL_VALUE_MIXER_ROLE_AUDIO_GENERATOR_6:
        case STM_SE_CTRL_VALUE_MIXER_ROLE_AUDIO_GENERATOR_7:

            // Look for the audio generator id for which gain needs to be
            // updated.
            if (!Value->input_gain.objecthandle)
            {
                return PlayerNoError;
            }

            for (AudioGeneratorIdx = 0; AudioGeneratorIdx < MAX_AUDIO_GENERATORS; AudioGeneratorIdx++)
            {
                if (Generator[AudioGeneratorIdx] == Value->input_gain.objecthandle)
                {
                    break;
                }
            }

            if (AudioGeneratorIdx >= MAX_AUDIO_GENERATORS)
            {
                return PlayerError;
            }

            for (uint32_t ChannelIdx(0); ChannelIdx < SND_PSEUDO_MIXER_CHANNELS; ChannelIdx++)
            {
                Options.AudioGeneratorGain[AudioGeneratorIdx][ChannelIdx] = (int)Value->input_gain.gain[remap_channels[ChannelIdx]];
                // Update the flag
                Options.AudioGeneratorGainUpdated = true;
            }

            break;

        case STM_SE_CTRL_VALUE_AUDIO_MIXER_ROLE_SFX0:
        case STM_SE_CTRL_VALUE_AUDIO_MIXER_ROLE_SFX1:
        case STM_SE_CTRL_VALUE_AUDIO_MIXER_ROLE_SFX2:
        case STM_SE_CTRL_VALUE_AUDIO_MIXER_ROLE_SFX3:
        case STM_SE_CTRL_VALUE_AUDIO_MIXER_ROLE_SFX4:
        case STM_SE_CTRL_VALUE_AUDIO_MIXER_ROLE_SFX5:
        case STM_SE_CTRL_VALUE_AUDIO_MIXER_ROLE_SFX6:
        case STM_SE_CTRL_VALUE_AUDIO_MIXER_ROLE_SFX7:
            port -= STM_SE_CTRL_VALUE_AUDIO_MIXER_ROLE_SFX0; // Substract interactive port offset so can index 0-7
            Options.InteractiveGain[port] = (int)Value->input_gain.gain[0];
            // Update the flag
            Options.InteractiveGainUpdated = true;

            for (uint32_t ChannelIdx(0); ChannelIdx < SND_PSEUDO_MIXER_CHANNELS; ChannelIdx++)
            {
                Options.InteractivePan[port][ChannelIdx] = (int)Value->input_gain.panning[remap_channels[ChannelIdx]];
            }

            // Update the flag
            Options.InteractivePanUpdated = true;
            break;

        default:
            SE_ERROR("Invalid port,  %d\n", port);
            return PlayerError;
            break;
        }
    }
    break;

    case STM_SE_CTRL_AUDIO_MIXER_OUTPUT_GAIN:
        Options.PostMixGain = (int) Value->output_gain.gain[0];
        // Update the flag
        Options.PostMixGainUpdated = true;
        break;

    case STM_SE_CTRL_AUDIO_DYNAMIC_RANGE_COMPRESSION:
    {
        const stm_se_drc_t drc  = *(static_cast<const stm_se_drc_t *>(value));
        Options.Drc = drc;
        break;
    }

    case STM_SE_CTRL_VOLUME_MANAGER:
    {
        SetMixerTuningProfile((struct MME_PcmProcParamsHeader_s *) value, &Options.VolumeManagerParams);
        break;
    }

    case STM_SE_CTRL_VIRTUALIZER:
    {
        SetMixerTuningProfile((struct MME_PcmProcParamsHeader_s *) value, &Options.VirtualizerParams);
        break;
    }

    case STM_SE_CTRL_UPMIXER:
    {
        SetMixerTuningProfile((struct MME_PcmProcParamsHeader_s *) value, &Options.UpmixerParams);
        break;
    }

    case STM_SE_CTRL_DIALOG_ENHANCER:
    {
        SetMixerTuningProfile((struct MME_PcmProcParamsHeader_s *) value, &Options.DialogEnhancerParams);
        break;
    }

    default:
        SE_ERROR("Invalid control\n");
        return PlayerError;
    }

    Options.DebugDump();
    // Trigger update in mixer thread context.
    MixerSettings.SetTobeUpdated(Options);
    return PlayerNoError;
}

////////////////////////////////////////////////////////////////////////////
///
///
///
PlayerStatus_t Mixer_Mme_c::GetCompoundOption(stm_se_ctrl_t ctrl, void *value) /*const*/
{
    stm_se_audio_mixer_value_t *Value = (stm_se_audio_mixer_value_t *) value;
    //int port = Value->input_gain.port;
    int port;
    uint32_t AudioGeneratorIdx;
    int i;
    const MixerOptionStruct MixerOptions = MixerSettings.GetLastOptions();
    SE_VERBOSE(group_mixer, ">%s (ctrl= %d)\n", Name, ctrl);

    switch (ctrl)
    {
    case STM_SE_CTRL_OUTPUT_SAMPLING_FREQUENCY:
        Value->output_sfreq.control   = MixerOptions.OutputSfreq.control;
        Value->output_sfreq.frequency = MixerOptions.OutputSfreq.frequency;
        SE_DEBUG(group_mixer, "<: STM_SE_CTRL_OUTPUT_SAMPLING_FREQUENCY freq:%d\n",
                 Value->output_sfreq.frequency);
        break;

    case STM_SE_CTRL_SPEAKER_CONFIG:
        Value->speaker_config = MixerOptions.SpeakerConfig;
        SE_DEBUG(group_mixer, "<: STM_SE_CTRL_SPEAKER_CONFIG\n");
        break;

    case STM_SE_CTRL_AUDIO_MIXER_INPUT_COEFFICIENT:
        port = LookupClientFromStream((class HavanaStream_c *) Value->input_gain.input);
        SE_DEBUG(group_mixer, "<: STM_SE_CTRL_AUDIO_MIXER_INPUT_COEFFICIENT\n");

        switch (port)
        {
        case STM_SE_CTRL_VALUE_AUDIO_MIXER_ROLE_PRIMARY:
        case STM_SE_CTRL_VALUE_AUDIO_MIXER_ROLE_IN3:
        case STM_SE_CTRL_VALUE_AUDIO_MIXER_ROLE_IN4:
            for (i = 0; i < SND_PSEUDO_MIXER_CHANNELS; i++)
            {
                Value->input_gain.gain   [remap_channels[i]] = (stm_se_q3dot13_t) MixerOptions.Gain[PrimaryClient + port][i];
                Value->input_gain.panning[remap_channels[i]] = (stm_se_q3dot13_t) MixerOptions.Pan [PrimaryClient + port][i];
            }

            break;

        case STM_SE_CTRL_VALUE_AUDIO_MIXER_ROLE_SECONDARY:
            for (i = 0; i < SND_PSEUDO_MIXER_CHANNELS; i++)
            {
                Value->input_gain.gain   [remap_channels[i]] = (stm_se_q3dot13_t) MixerOptions.Gain[SecondaryClient][i];
                Value->input_gain.panning[remap_channels[i]] = (stm_se_q3dot13_t) MixerOptions.Pan [SecondaryClient][i];
            }

            break;

        case STM_SE_CTRL_VALUE_MIXER_ROLE_AUDIO_GENERATOR_0:
        case STM_SE_CTRL_VALUE_MIXER_ROLE_AUDIO_GENERATOR_1:
        case STM_SE_CTRL_VALUE_MIXER_ROLE_AUDIO_GENERATOR_2:
        case STM_SE_CTRL_VALUE_MIXER_ROLE_AUDIO_GENERATOR_3:
        case STM_SE_CTRL_VALUE_MIXER_ROLE_AUDIO_GENERATOR_4:
        case STM_SE_CTRL_VALUE_MIXER_ROLE_AUDIO_GENERATOR_5:
        case STM_SE_CTRL_VALUE_MIXER_ROLE_AUDIO_GENERATOR_6:
        case STM_SE_CTRL_VALUE_MIXER_ROLE_AUDIO_GENERATOR_7:
            if (!Value->input_gain.objecthandle)
            {
                return PlayerNoError;
            }

            for (AudioGeneratorIdx = 0; AudioGeneratorIdx < MAX_AUDIO_GENERATORS; AudioGeneratorIdx++)
            {
                if (Generator[AudioGeneratorIdx] == Value->input_gain.objecthandle)
                {
                    break;
                }
            }

            if (AudioGeneratorIdx >= MAX_AUDIO_GENERATORS)
            {
                // restore the Q3_13_UNITY values for audio
                // generator in case it is not active.
                for (uint32_t ChannelIdx(0); ChannelIdx < SND_PSEUDO_MIXER_CHANNELS; ChannelIdx++)
                {
                    Value->input_gain.gain   [remap_channels[ChannelIdx]] = Q3_13_UNITY;
                }

                return PlayerError;
            }

            for (uint32_t ChannelIdx(0); ChannelIdx < SND_PSEUDO_MIXER_CHANNELS; ChannelIdx++)
            {
                Value->input_gain.gain[remap_channels[ChannelIdx]] = (stm_se_q3dot13_t) MixerOptions.AudioGeneratorGain[AudioGeneratorIdx][ChannelIdx];
            }

            break;

        default: // STM_SE_CTRL_VALUE_AUDIO_MIXER_ROLE_SFX#
            port -= STM_SE_CTRL_VALUE_AUDIO_MIXER_ROLE_SFX0;

            for (i = 0; i < SND_PSEUDO_MIXER_CHANNELS; i++)
            {
                Value->input_gain.gain   [remap_channels[i]] = (stm_se_q3dot13_t) MixerOptions.InteractiveGain[port];
                Value->input_gain.panning[remap_channels[i]] = (stm_se_q3dot13_t) MixerOptions.InteractivePan[port][i];
            }
        }

        break;

    case STM_SE_CTRL_AUDIO_MIXER_OUTPUT_GAIN:
        Value->output_gain.gain[0] = MixerOptions.PostMixGain;
        SE_DEBUG(group_mixer, "<: STM_SE_CTRL_AUDIO_MIXER_OUTPUT_GAIN PostMixGain:%d\n",
                 MixerOptions.PostMixGain);
        break;

    case STM_SE_CTRL_AUDIO_DYNAMIC_RANGE_COMPRESSION:
    {
        stm_se_drc_t *drc = static_cast<stm_se_drc_t *>(value);
        SE_DEBUG(group_mixer, "<: STM_SE_CTRL_AUDIO_DYNAMIC_RANGE_COMPRESSION\n");

        *drc = MixerOptions.Drc;
    }
    break;

    default:
        SE_ERROR("Invalid control\n");
        return PlayerError;
    }

    return PlayerNoError;
}

////////////////////////////////////////////////////////////////////////////
///
///
/// This is a synchronous function.
/// This function locks, sets event and waits event so be sure that you
/// call it in a thread state that allows that: in particular DO NOT CALL IT
/// in a spin locked code.
///
PlayerStatus_t Mixer_Mme_c::SendRegisterManifestorRequest(Manifestor_AudioKsound_c *Manifestor,
                                                          class HavanaStream_c *Stream,
                                                          stm_se_sink_input_port_t input_port)

{
    PlayerStatus_t status;
    MixerRequest_c request;
    request.SetHavanaStream(Stream);
    request.SetManifestor(Manifestor);
    request.SetInputPort(input_port);
    request.SetFunctionToManageTheRequest(&Mixer_Mme_c::RegisterManifestor);
    status = MixerRequestManager.SendRequest(request);

    if (status != PlayerNoError)
    {
        SE_ERROR(" Error returned by MixerRequestManager.SendRequest()\n");
    }

    return status;
}
////////////////////////////////////////////////////////////////////////////
///
/// A wrapper to the Mixer_Mme_c::RegisterManifestor( Manifestor_AudioKsound_c * Manifestor , class HavanaStream_c * Stream)
/// method.
///
PlayerStatus_t Mixer_Mme_c::RegisterManifestor(MixerRequest_c &aMixerRequest_c)
{
    PlayerStatus_t Status;
    Manifestor_AudioKsound_c *Manifestor = aMixerRequest_c.GetManifestor();
    class HavanaStream_c *Stream = aMixerRequest_c.GetHavanaStream();
    stm_se_sink_input_port_t input_port = aMixerRequest_c.GetInputPort();
    Status = RegisterManifestor(Manifestor, Stream, input_port);
    return Status;
}


////////////////////////////////////////////////////////////////////////////
///
///
///
PlayerStatus_t Mixer_Mme_c::RegisterManifestor(Manifestor_AudioKsound_c *Manifestor , class HavanaStream_c *Stream, stm_se_sink_input_port_t input_port)
{
    SE_DEBUG(group_mixer, ">%s Manifestor:%p Stream:%p input_port:%d \n", Name, Manifestor, Stream, input_port);

    if (input_port >= mTopology.nb_max_decoded_audio)
    {
        SE_ERROR("Invalid port index:%d", input_port);
        return PlayerNotSupported;
    }

    Client[input_port].LockTake();

    if (Client[input_port].IsMyManifestor(Manifestor))
    {
        // Manifestor is already registered for this client.
        Client[input_port].LockRelease();
        SE_ERROR("<: Manifestor %p-%d is already registered\n", Manifestor, input_port);
        return PlayerError;
    }
    if (Client[input_port].GetState() != DISCONNECTED)
    {
        // This client does not own this manifestor, but is already taken.
        Client[input_port].LockRelease();
        SE_ERROR("<: Cannot register Manifestor%p input_port%d is already connected\n", Manifestor, input_port);
        return PlayerTooMany;
    }

    // This client is free, so take it.
    Client[input_port].RegisterManifestor(Manifestor, Stream);
    Client[input_port].LockRelease();
    PlayerStatus_t status;
    status = CheckClientsState();

    if (status != PlayerNoError)
    {
        // Undo previous register operation.
        Client[input_port].DeRegisterManifestor();
        SE_ERROR("Error returned by CheckClientsState\n");
        return PlayerError;
    }

    return PlayerNoError;
}


////////////////////////////////////////////////////////////////////////////
///
///
/// This is a synchronous function.
/// This function locks, sets event and waits event so be sure that you
/// call it in a thread state that allows that: in particular DO NOT CALL IT
/// in a spin locked code.
///
PlayerStatus_t Mixer_Mme_c::SendGetProcessingRingBufCountRequest(Manifestor_AudioKsound_c *Manifestor, int *bufCountPtr)
{
    PlayerStatus_t status;
    MixerRequest_c request;
    request.SetManifestor(Manifestor);
    request.SetFunctionToManageTheRequest(&Mixer_Mme_c::GetProcessingRingBufCount);
    status = MixerRequestManager.SendRequest(request);

    if (status != PlayerNoError)
    {
        SE_ERROR(" Error returned by MixerRequestManager.SendRequest()\n");
    }

    *bufCountPtr = request.mIntResult;
    SE_DEBUG(group_mixer, "NbOfProcessingBuf:%d\n", *bufCountPtr);
    return status;
}

PlayerStatus_t Mixer_Mme_c::GetProcessingRingBufCount(MixerRequest_c &aMixerRequest_c)
{
    return GetProcessingRingBufCount(aMixerRequest_c.GetManifestor(), &aMixerRequest_c.mIntResult);
}

PlayerStatus_t Mixer_Mme_c::GetProcessingRingBufCount(Manifestor_AudioKsound_c *Manifestor, int *bufCountPtr)
{
    uint32_t ClientIdx(0);
    for (ClientIdx = 0; ClientIdx < MIXER_MAX_CLIENTS; ClientIdx++)
    {
        Client[ClientIdx].LockTake();

        if (Client[ClientIdx].IsMyManifestor(Manifestor))
        {
            Manifestor->GetProcessingRingBufCount(bufCountPtr);
            SE_DEBUG(group_mixer, "NbOfProcessingBuf:%d\n", *bufCountPtr);
            Client[ClientIdx].LockRelease();
            return PlayerNoError;
        }

        Client[ClientIdx].LockRelease();
    }

    SE_ERROR("Unknown Manifestor:%p\n", Manifestor);
    return PlayerError;
}

////////////////////////////////////////////////////////////////////////////
///
///
/// This is a synchronous function.
/// This function locks, sets event and waits event so be sure that you
/// call it in a thread state that allows that: in particular DO NOT CALL IT
/// in a spin locked code.
///
PlayerStatus_t Mixer_Mme_c::SendDeRegisterManifestorRequest(Manifestor_AudioKsound_c *Manifestor)

{
    PlayerStatus_t status;
    MixerRequest_c request;
    request.SetManifestor(Manifestor);
    request.SetFunctionToManageTheRequest(&Mixer_Mme_c::DeRegisterManifestor);
    status = MixerRequestManager.SendRequest(request);

    if (status != PlayerNoError)
    {
        SE_ERROR(" Error returned by MixerRequestManager.SendRequest()\n");
    }

    return status;
}
////////////////////////////////////////////////////////////////////////////
///
/// A wrapper to the Mixer_Mme_c::DeRegisterManifestor( Manifestor_AudioKsound_c * Manifestor )
/// method.
///
PlayerStatus_t Mixer_Mme_c::DeRegisterManifestor(MixerRequest_c &aMixerRequest_c)
{
    PlayerStatus_t Status;
    Manifestor_AudioKsound_c *Manifestor = aMixerRequest_c.GetManifestor();
    Status = DeRegisterManifestor(Manifestor);
    return Status;
}
////////////////////////////////////////////////////////////////////////////
///
/// Remove the specified manifestor from the client table.
///
/// \todo This method must re-order the table if a client is disconnected from
///       the middle (due to very simple mappings from clients to buffers elsewhere
///       in this class). Until that happens expect to see odd things for DISCONNECTED
///       clients.
///
PlayerStatus_t Mixer_Mme_c::DeRegisterManifestor(Manifestor_AudioKsound_c *Manifestor)
{
    uint32_t ClientIdx(0);
    SE_DEBUG(group_mixer, ">%s %p\n", Name, Manifestor);

    for (ClientIdx = 0; ClientIdx < MIXER_MAX_CLIENTS; ClientIdx++)
    {
        Client[ClientIdx].LockTake();

        if (Client[ClientIdx].IsMyManifestor(Manifestor))
        {
            if (Client[ClientIdx].GetState() != STOPPED)
            {
                SE_ERROR("<: Cannot deregister an input from state %s\n", LookupInputState(Client[ClientIdx].GetState()));
                Client[ClientIdx].LockRelease();
                // Return now
                return PlayerError;
            }
            else
            {
                Client[ClientIdx].DeRegisterManifestor();
                Client[ClientIdx].LockRelease();
                PlayerStatus_t status;
                status = CheckClientsState();

                if (status != PlayerNoError)
                {
                    SE_ERROR("Error returned by CheckClientsState\n");
                    return PlayerError;
                }

                // Client correctly done.
                break;
            }
        }
        else
        {
            Client[ClientIdx].LockRelease();
        }
    }

    if (ClientIdx >= MIXER_MAX_CLIENTS)
    {
        //No client previously registered for this manifestor.
        SE_ERROR("<: Manifestor %p is not registered\n", Manifestor);
        return PlayerError;
    }

    return PlayerNoError;
}
////////////////////////////////////////////////////////////////////////////
///
///
/// This is a synchronous function.
/// This function locks, sets event and waits event so be sure that you
/// call it in a thread state that allows that: in particular DO NOT CALL IT
/// in a spin locked code.
///
PlayerStatus_t Mixer_Mme_c::SendEnableManifestorRequest(Manifestor_AudioKsound_c *Manifestor)

{
    PlayerStatus_t status;
    MixerRequest_c request;
    SE_DEBUG(group_mixer, ">%s %p\n", Name, Manifestor);
    request.SetManifestor(Manifestor);
    request.SetFunctionToManageTheRequest(&Mixer_Mme_c::EnableManifestor);
    status = MixerRequestManager.SendRequest(request);

    if (status != PlayerNoError)
    {
        SE_ERROR(" Error returned by MixerRequestManager.SendRequest()\n");
    }

    SE_DEBUG(group_mixer, "<:%p\n", Manifestor);
    return status;
}

////////////////////////////////////////////////////////////////////////////
///
/// A wrapper to the Mixer_Mme_c::EnableManifestor( Manifestor_AudioKsound_c * Manifestor )
/// method.
///
PlayerStatus_t Mixer_Mme_c::EnableManifestor(MixerRequest_c &aMixerRequest_c)
{
    PlayerStatus_t Status;
    Manifestor_AudioKsound_c *Manifestor = aMixerRequest_c.GetManifestor();
    Status = EnableManifestor(Manifestor);
    return Status;
}


////////////////////////////////////////////////////////////////////////////
///
///
///
PlayerStatus_t Mixer_Mme_c::EnableManifestor(Manifestor_AudioKsound_c *Manifestor)
{
    uint32_t ClientIdx(0);

    for (ClientIdx = 0; ClientIdx < MIXER_MAX_CLIENTS; ClientIdx++)
    {
        Client[ClientIdx].LockTake();

        if (Client[ClientIdx].IsMyManifestor(Manifestor))
        {
            // Manifestor is found.
            if (Client[ClientIdx].GetState() != STOPPED)
            {
                SE_ERROR("<: Cannot enable an input from state %s\n", LookupInputState(Client[ClientIdx].GetState()));
                Client[ClientIdx].LockRelease();
                // Return now
                return PlayerError;
            }
            else
            {
                // we must move out of the STOPPED state before calling UpdatePlayerComponentsModuleParameters()
                // otherwise the update will not take place.
                Client[ClientIdx].SetState(UNCONFIGURED);
                PlayerStatus_t Status = UpdatePlayerComponentsModuleParameters();

                if (Status != PlayerNoError)
                {
                    SE_ERROR("Failed to update normalized time offset (a zero value will be used instead)\n");
                    // no error recovery needed
                }

                Client[ClientIdx].LockRelease();
                Status = CheckClientsState();

                if (Status != PlayerNoError)
                {
                    SE_ERROR("Error returned by CheckClientsState\n");
                    return PlayerError;
                }
            }

            // The job is done, so exit now
            break;
        }
        else
        {
            // This client does not own this manifestor.
            Client[ClientIdx].LockRelease();
        }
    }

    if (ClientIdx >= MIXER_MAX_CLIENTS)
    {
        //Not enough client for this manifestor registration.
        SE_ERROR("<: Manifestor %p is not registered\n\n", Manifestor);
        return PlayerError;
    }

    return PlayerNoError;
}
////////////////////////////////////////////////////////////////////////////
///
///
/// This is a synchronous function.
/// This function locks, sets event and waits event so be sure that you
/// call it in a thread state that allows that: in particular DO NOT CALL IT
/// in a spin locked code.
///
PlayerStatus_t Mixer_Mme_c::SendDisableManifestorRequest(Manifestor_AudioKsound_c *Manifestor)

{
    PlayerStatus_t status;
    MixerRequest_c request;
    SE_DEBUG(group_mixer, ">%s %p\n", Name, Manifestor);

    if (ThreadState == Starting || ThreadState == Idle) //C.FENARD: It is not thread safe.
    {
        // The event is used to nudge the playback thread if it has never dispatched any samples
        // to the MME transformer (it has not entered its main loop yet).
        SE_DEBUG(group_mixer, "%s Set PcmPlayerSurfaceParametersUpdated Event\n", Name);
        OS_SetEvent(&PcmPlayerSurfaceParametersUpdated);
    }

    request.SetManifestor(Manifestor);
    request.SetFunctionToManageTheRequest(&Mixer_Mme_c::DisableManifestor);
    status = MixerRequestManager.SendRequest(request);

    if (status != PlayerNoError)
    {
        SE_ERROR(" Error returned by MixerRequestManager.SendRequest()\n");
    }

    if (ThreadState == Idle)
    {
        SetStoppingClientsToStoppedState();
    }

    // The previous request has just set the state to STOPPING. Now we wait for the
    // playback thread to set it to STOPPED.
    for (uint32_t ClientIdx(0); ClientIdx < MIXER_MAX_CLIENTS; ClientIdx++)
    {
        for (uint32_t LoopCount(0); Client[ClientIdx].GetState() == STOPPING; LoopCount++)
        {
            if (LoopCount > 100)
            {
                SE_ERROR("Time out waiting for manifestor %p-%d to enter STOPPED state\n",
                         Manifestor, ClientIdx);
                return PlayerError;
            }

            OS_SleepMilliSeconds(10);
        }
    }

    return status;
}

////////////////////////////////////////////////////////////////////////////
///
/// A wrapper to the Mixer_Mme_c::DisableManifestor( Manifestor_AudioKsound_c * Manifestor )
/// method.
///
PlayerStatus_t Mixer_Mme_c::DisableManifestor(MixerRequest_c &aMixerRequest_c)
{
    PlayerStatus_t Status;
    Manifestor_AudioKsound_c *Manifestor = aMixerRequest_c.GetManifestor();
    Status = DisableManifestor(Manifestor);
    return Status;
}


////////////////////////////////////////////////////////////////////////////
///
///
///
PlayerStatus_t Mixer_Mme_c::DisableManifestor(Manifestor_AudioKsound_c *Manifestor)
{
    uint32_t ClientIdx(0);

    for (ClientIdx = 0; ClientIdx < MIXER_MAX_CLIENTS; ClientIdx++)
    {
        Client[ClientIdx].LockTake();

        if (Client[ClientIdx].IsMyManifestor(Manifestor))
        {
            // Manifestor is found.
            if (Client[ClientIdx].GetState() == STOPPED || Client[ClientIdx].GetState() == DISCONNECTED)
            {
                SE_ERROR("Cannot disable an input from state %s\n", LookupInputState(Client[ClientIdx].GetState()));
                Client[ClientIdx].LockRelease();
                // Return now
                return PlayerError;
            }
            // Note that whilst the writes we are about to do to the state variable are atomic we are not
            // permitted to enter the STOPPED or STOPPING state until it is safe to do so; all code that
            // makes it unsafe to enter this state owns the mutex.
            else if (Client[ClientIdx].GetState() == UNCONFIGURED)
            {
                // If we are in the UNCONFIGURED state then the mixer main thread has not yet started
                // interacting with the manifestor. this means we can (and, in fact, must) go straight
                // to the STOPPED state without waiting for a handshake from the main thread.
                Client[ClientIdx].SetState(STOPPED);
                Client[ClientIdx].LockRelease();
            }
            else
            {
                // When the playback thread observes a client in the STOPPING state it will finalize the input
                // (request a swan song from the manifestor and then release all resources) before placing the
                // client into the STOPPED state.
                Client[ClientIdx].SetState(STOPPING);
                Client[ClientIdx].LockRelease();
                // We are here in the playback thread and we cannot set the client state to STOPPED right now
                // because we need to loop one more time in the playbac thread loop so as to do a fading and
                // other stuff. So we will wait on the STOPPED state in the client thread before returning from the
                // "disable manifestor request".
            }

            PlayerStatus_t status;
            status = CheckClientsState();

            if (status != PlayerNoError)
            {
                SE_ERROR("Error returned by CheckClientsState\n");
                return PlayerError;
            }

            // The job is done, so exit now
            break;
        }
        else
        {
            // This client does not own this manifestor.
            Client[ClientIdx].LockRelease();
        }
    }

    if (ClientIdx >= MIXER_MAX_CLIENTS)
    {
        //Did not found client for this
        SE_ERROR("<: Manifestor %p is not registered\n\n", Manifestor);
        return PlayerError;
    }

    return PlayerNoError;
}
////////////////////////////////////////////////////////////////////////////
///
///
///
void Mixer_Mme_c::GetDRCParameters(DRCParams_t *DRC)
{
    int ApplicationType(PolicyValueAudioApplicationIso);

    // Get audio application type
    {
        HavanaStream_c *Stream = Client[PrimaryClient].GetStream();
        if (Stream)
        {
            Stream->GetOption(PolicyAudioApplicationType, &ApplicationType);
            SE_VERBOSE(group_mixer, "Stream : %p, ApplicationType=%s", Stream,
                       LookupPolicyValueAudioApplicationType(ApplicationType));
        }
    }

    if (STM_SE_CTRL_VALUE_AUDIO_APPLICATION_MS12 == ApplicationType)
    {
        // If DDRE Enabled DRC/Comp handled in Mixer, decoder must always be in Line Mode with no DRC applied
        DRC->DRC_Enable = 0;                        // ON/OFF switch of DRC
        DRC->DRC_Type   = STM_SE_COMP_LINE_OUT;     // Line/RF/Custom mode
        DRC->DRC_HDR    = 0;                        // cut factor (attenuating high dynamic range signal)
        DRC->DRC_LDR    = 0;                        // boost factor (boosting low dynamic range signal)
    }
    else
    {
        DRC->DRC_Enable = MixerSettings.MixerOptions.Drc.mode != STM_SE_NO_COMPRESSION ; // ON/OFF switch of DRC
        DRC->DRC_Type   = MixerSettings.MixerOptions.Drc.mode;     // Line/RF/Custom mode
        DRC->DRC_HDR    = MixerSettings.MixerOptions.Drc.cut;      // cut factor (attenuating high dynamic range signal)
        DRC->DRC_LDR    = MixerSettings.MixerOptions.Drc.boost;    // boost factor (boosting low dynamic range signal)
    }
}


////////////////////////////////////////////////////////////////////////////
///
///
///
PlayerStatus_t Mixer_Mme_c::UpdateManifestorParameters(Manifestor_AudioKsound_c *Manifestor,
                                                       ParsedAudioParameters_t *ParsedAudioParameters,
                                                       bool TakeMixerClientLock)
{
    uint32_t ClientIdx(0);
    SE_DEBUG(group_mixer, ">%s %p %s\n", Name, Manifestor, TakeMixerClientLock ? "true" : "false");

    //
    // Store the new audio parameters
    //

    for (ClientIdx = 0; ClientIdx < MIXER_MAX_CLIENTS; ClientIdx++)
    {
        if (true == TakeMixerClientLock)
        {
            Client[ClientIdx].LockTake();
        }

        if (Client[ClientIdx].IsMyManifestor(Manifestor))
        {
            // Manifestor is found.
            Client[ClientIdx].UpdateParameters(ParsedAudioParameters);
            // Update the flag to signal change in client params
            ClientAudioParamsUpdated = true;
            SE_ASSERT(Client[ClientIdx].GetState() != DISCONNECTED);
            SE_ASSERT(Client[ClientIdx].GetState() != STOPPED);

            if (Client[ClientIdx].GetState() == UNCONFIGURED)
            {
                Client[ClientIdx].SetState(STARTING);
                Client[ClientIdx].UpdateManifestorModuleParameters(GetClientParameters());
            }

            if (true == TakeMixerClientLock)
            {
                Client[ClientIdx].LockRelease();
            }

            // Re-configure the mixer
            MMENeedsParameterUpdate = true;
            // Reconfigure the PCM player
            PcmPlayerNeedsParameterUpdate = true;
            SE_DEBUG(group_mixer, "%s Set PcmPlayerSurfaceParametersUpdated Event\n", Name);
            OS_SetEvent(&PcmPlayerSurfaceParametersUpdated);
            // The job is done, so exit now
            break;
        }
        else
        {
            // This client does not own this manifestor.
            if (true == TakeMixerClientLock)
            {
                Client[ClientIdx].LockRelease();
            }
        }
    }

    if (ClientIdx >= MIXER_MAX_CLIENTS)
    {
        //Not enough client for this manifestor registration.
        SE_ERROR("<: Manifestor %p is not registered\n\n", Manifestor);
        return PlayerError;
    }

    SE_DEBUG(group_mixer, "<\n");
    return PlayerNoError;
}

////////////////////////////////////////////////////////////////////////////
///
/// Place the specified manifestor into the (emergency) muted state.
///
PlayerStatus_t Mixer_Mme_c::SetManifestorEmergencyMuteState(Manifestor_AudioKsound_c *Manifestor, bool Muted)
{
    uint32_t ClientIdx(0);

    for (ClientIdx = 0; ClientIdx < MIXER_MAX_CLIENTS; ClientIdx++)
    {
        Client[ClientIdx].LockTake();

        if (Client[ClientIdx].IsMyManifestor(Manifestor))
        {
            // Manifestor is found.
            Client[ClientIdx].SetMute(Muted);
            Client[ClientIdx].LockRelease();
            MMENeedsParameterUpdate = true;
            // The job is done, so exit now
            break;
        }
        else
        {
            // This client does not own this manifestor.
            Client[ClientIdx].LockRelease();
        }
    }

    if (ClientIdx >= MIXER_MAX_CLIENTS)
    {
        //Not enough client for this manifestor registration.
        SE_ERROR("Manifestor %p is not registered\n", Manifestor);
        return PlayerError;
    }

    return PlayerNoError;
}

////////////////////////////////////////////////////////////////////////////
///
///
///
PlayerStatus_t Mixer_Mme_c::SetOutputRateAdjustment(int adjust)
{
    SE_VERBOSE(group_mixer, ">><<\n");

    for (uint32_t CountOfAudioPlayerAttached(0), PlayerIdx(0); CountOfAudioPlayerAttached < NumberOfAudioPlayerAttached; PlayerIdx++)
    {
        if (true == MixerPlayer[PlayerIdx].IsPlayerObjectAttached())
        {
            CountOfAudioPlayerAttached++;

            if (true == MixerPlayer[PlayerIdx].HasPcmPlayer())
            {
                // Corresponding PcmPlayer found.
                int AdjustDone;
                MixerPlayer[PlayerIdx].GetPcmPlayer()->SetOutputRateAdjustment(adjust, &AdjustDone);
                SE_VERBOSE(group_mixer, "%s %d %d\n", Name, adjust, AdjustDone);
            }
        }
    }

    return PlayerNoError;
}

////////////////////////////////////////////////////////////////////////////
///
/// Power management functions
///
/// These methods are used to synchronize stop/restart of Mixer thread in low power
/// and to terminate/init MME transformer for CPS mode
///
PlayerStatus_t Mixer_Mme_c::LowPowerEnter(__stm_se_low_power_mode_t low_power_mode)
{
    PlayerStatus_t PlayerStatus = PlayerNoError;
    SE_INFO(group_mixer, ">%s\n", Name);
    // Save the requested low power mode
    LowPowerMode = low_power_mode;
    // Reset events used for putting mixer thread in "low power"
    OS_ResetEvent(&LowPowerEnterEvent);
    OS_ResetEvent(&LowPowerExitEvent);
    OS_ResetEvent(&LowPowerExitCompletedEvent);

    // Init variables used for putting mixer thread in "low power"
    LowPowerEnterThreadLoop = 0;
    // Save low power state
    IsLowPowerState = true;
    // Trigger mixer thread to speed-up low power enter
    MixerRequest_c request;
    request.SetFunctionToManageTheRequest(&Mixer_Mme_c::CheckClientsState);
    PlayerStatus = MixerRequestManager.SendRequest(request);

    if (PlayerStatus != PlayerNoError)
    {
        SE_ERROR("Warning: Error returned by MixerRequestManager.SendRequest()\n");
    }
    else
    {
        // Wait for mixer thread to be in safe state (no more MME commands issued)
        OS_Status_t WaitStatus = OS_WaitForEventInterruptible(&LowPowerEnterEvent, OS_INFINITE);
        if (WaitStatus == OS_INTERRUPTED)
        {
            SE_INFO(group_mixer, "wait for LP enter interrupted; LowPowerEnterEvent:%d\n", LowPowerEnterEvent.Valid);
        }

        OS_Smp_Mb(); // Read memory barrier: rmb_for_InLowPowerState coupled with: wmb_for_InLowPowerState
        if (InLowPowerState == false)
        {
            PlayerStatus = PlayerError;
        }
    }

    return PlayerStatus;
}

PlayerStatus_t Mixer_Mme_c::LowPowerExit(void)
{
    PlayerStatus_t PlayerStatus = PlayerNoError;

    SE_INFO(group_mixer, ">%s\n", Name);

    // Reset low power state
    IsLowPowerState = false;
    // Wake-up mixer thread
    OS_SetEventInterruptible(&LowPowerExitEvent);

    // Wait for mixer thread to be in safe state (no more MME commands issued)
    OS_Status_t WaitStatus = OS_WaitForEventInterruptible(&LowPowerExitCompletedEvent, OS_INFINITE);
    if (WaitStatus == OS_INTERRUPTED)
    {
        SE_INFO(group_mixer, "wait for LP exit completed interrupted; LowPowerExitCompletedEvent:%d\n", LowPowerExitCompletedEvent.Valid);
    }

    OS_Smp_Mb(); // Read memory barrier: rmb_for_InLowPowerState coupled with: wmb_for_InLowPowerState
    if (InLowPowerState == true)
    {
        PlayerStatus = PlayerError;
    }

    return PlayerStatus;
}
////////////////////////////////////////////////////////////////////////////
///
/// A wrapper to the Mixer_Mme_c::CheckClientsState( )
/// method.
/// The aMixerRequest argument is not used but we need this function
/// prototype for the MixerRequest_c::SetFunctionToManageTheRequest()
///
PlayerStatus_t Mixer_Mme_c::CheckClientsState(MixerRequest_c &aMixerRequest)
{
    PlayerStatus_t Status;
    Status = CheckClientsState();
    return Status;
}
////////////////////////////////////////////////////////////////////////////
///
/// Initialize and terminate the appropriate global resources based the state of the inputs.
///
/// Basically we examine the state of all the inputs (including audio generator
/// and interactive inputs) and determine which global services should be
/// running. Then we make it so.
///
/// - If all the clients and all the interactive clients are disconnected then we halt the mixer
///   we reset it and set the thread to the Idle state.
/// - Otherwise if there is at least one client or one interactive client that is not disconnected
///   then
///       - we initialize the PCM players and the MME transformer if not already done
///       - if the thread is in Idle state and one client in UNCONFIGURED state or one interactive
///         client in STOPPED state, we set the thread state to Starting
///
PlayerStatus_t Mixer_Mme_c::CheckClientsState()
{
    PlayerStatus_t Status = PlayerNoError;
    SE_DEBUG(group_mixer, ">%s\n", Name);

    if (AllClientsAreInState(DISCONNECTED) && AllAudioGenIAudioInputsAreDisconnected())
    {
        Halt(); // No error recovery possible
        Status = Reset(); // No error recovery possible
        ThreadState = Idle;

        // All has been reset, so reset possible pending event.
        if (true == OS_TestEventSet(&PcmPlayerSurfaceParametersUpdated))
        {
            SE_DEBUG(group_mixer, "%s ###### Resetting: PcmPlayerSurfaceParametersUpdated Event\n", Name);
            OS_ResetEvent(&PcmPlayerSurfaceParametersUpdated);
        }
    }
    else
    {
        bool MixerSettingsToUpdate(false);
        bool AudioPlayersToUpdate(false);
        // Review output configuration.
        MixerSettingsToUpdate = MixerSettings.CheckAndUpdate();
        // Review players.
        AudioPlayersToUpdate = CheckAndUpdateAllAudioPlayers();

        if ((true == MixerSettingsToUpdate) || (true == AudioPlayersToUpdate))
        {
            PlayerStatus_t Status = UpdatePlayerComponentsModuleParameters();

            if (PlayerNoError != Status)
            {
                SE_ERROR("Failed to update normalized time offset or CodecControls (default value will be used instead)\n");
            }

            // Trigger updates to come accordingly (Mixer and PCM Players).
            PcmPlayerNeedsParameterUpdate = true;
            MMENeedsParameterUpdate = true;
        }

        if (Status == PlayerNoError && !MMEInitialized)
        {
            Status = InitializeMMETransformer(&MMEInitParams, false);
        }

        if (FirstActivePlayer != MIXER_MME_NO_ACTIVE_PLAYER)
        {
            Status = InitializePcmPlayers();
        }

        if ((NumberOfAudioPlayerInitialized > 0) && (Status == PlayerNoError) && (ThreadState == Idle))
        {
            if (ThereIsOneClientInState(UNCONFIGURED) || ThereIsOneAudioGenIAudioInputInState(STM_SE_AUDIO_GENERATOR_STOPPED))
            {
                ThreadState = Starting;
                // The players may have been already attached and there may be no update
                // So we ensure that the players are updated and started in the "Starting" state
                // as we may have cleared all the player "SurfaceParameters" at ::Reset()
                PcmPlayerNeedsParameterUpdate = true;
            }
        }
    }

    SE_DEBUG(group_mixer, "<%s %d\n", Name, ThreadState);
    return Status;
}

////////////////////////////////////////////////////////////////////////////
///
///

void Mixer_Mme_c::SetStoppingClientsToStoppedState()
{
    for (uint32_t ClientIdx(0); ClientIdx < MIXER_MAX_CLIENTS; ClientIdx++)
    {
        if (Client[ClientIdx].GetState() == STOPPING)
        {
            SE_DEBUG(group_mixer,  "@: Starting: Moving manifestor %p-%d from STOPPING to STOPPED\n", Client[ClientIdx].GetManifestor(), ClientIdx);
            Client[ClientIdx].SetState(STOPPED);
            MMENeedsParameterUpdate = true;
            PcmPlayerNeedsParameterUpdate = true;
        }
    }

    for (uint32_t AudioGeneratorIdx = 0; AudioGeneratorIdx < MAX_AUDIO_GENERATORS; AudioGeneratorIdx++)
    {
        if (Generator[AudioGeneratorIdx] && (Generator[AudioGeneratorIdx]->GetState() == STM_SE_AUDIO_GENERATOR_STOPPING))
        {
            SE_DEBUG(group_mixer, "Moving Audio Generator %p from STOPPING to STOPPED\n", Generator[AudioGeneratorIdx]);
            Generator[AudioGeneratorIdx]->SetState(STM_SE_AUDIO_GENERATOR_STOPPED);
            MMENeedsParameterUpdate = true;
            PcmPlayerNeedsParameterUpdate = true;
        }
    }

    for (uint32_t InteractiveAudioIdx = 0; InteractiveAudioIdx < MAX_INTERACTIVE_AUDIO; InteractiveAudioIdx++)
    {
        SE_VERBOSE(group_mixer, "Moving Interactive Audio %p from STOPPING to STOPPED\n", InteractiveAudio[InteractiveAudioIdx]);

        if (InteractiveAudio[InteractiveAudioIdx] &&
            InteractiveAudio[InteractiveAudioIdx]->GetState() == STM_SE_AUDIO_GENERATOR_STOPPING)
        {
            InteractiveAudio[InteractiveAudioIdx]->SetState(STM_SE_AUDIO_GENERATOR_STOPPED);
            MMENeedsParameterUpdate = true;
            PcmPlayerNeedsParameterUpdate = true;
        }
    }
}

////////////////////////////////////////////////////////////////////////////
///
/// Main mixer processing thread.
///
/// \b WARNING: This method is public only to allow it to be called from a
///             C linkage callback. Do not call it directly.
///
void Mixer_Mme_c::PlaybackThread()
{
    PlayerStatus_t PlaybackStatus = PlayerNoError;
    ThreadState = Idle;
    MixerRequestManager.Start();
    long long int PlaybackThreadDuration = 0;

    // Once this mixer object has been created, MW can start attaching clients / players to it
    // this attachment relies on the read of Mixer_Request_Manager_c::ThreadState
    OS_Smp_Mb(); // Write memory barrier: wmb_for_Mixer_Starting coupled with: rmb_for_Mixer_Starting
    OS_SetEvent(&mPlaybackThreadOnOff);

    // Check state of clients
    while (PlaybackThreadRunning)
    {
        uint32_t consecutiveErrorCounter = 0;
        // Whatever the playback thread state is, check and update what can have been changed client side.
        bool MixerSettingsToUpdate(false);
        bool AudioPlayersToUpdate(false);

        if (SE_IS_VERBOSE_ON(group_mixer))
        {
            if (PlaybackThreadDuration != 0)
            {
                SE_VERBOSE(group_mixer, "%s PlaybackThreadDuration:%6lldus",
                           Name, OS_GetTimeInMicroSeconds() - PlaybackThreadDuration);
            }
            PlaybackThreadDuration = OS_GetTimeInMicroSeconds();
        }

        // Review output configuration.
        MixerSettingsToUpdate = MixerSettings.CheckAndUpdate();
        // Review players.
        AudioPlayersToUpdate = CheckAndUpdateAllAudioPlayers();

        if ((true == MixerSettingsToUpdate) || (true == AudioPlayersToUpdate))
        {
            PlayerStatus_t Status = UpdatePlayerComponentsModuleParameters();

            if (PlayerNoError != Status)
            {
                SE_ERROR("Failed to update normalized time offset or CodecControls (default value will be used instead)\n");
            }

            // Trigger updates to come accordingly (Mixer and PCM Players).
            PcmPlayerNeedsParameterUpdate = true;
            MMENeedsParameterUpdate = true;
        }

        MixerRequestManager.ManageTheRequest(*this);

        // If low power state, thread must stay asleep until the low power exit signal
        if (IsLowPowerState)
        {
            // Entering low power state...
            if (PlaybackThreadLowPowerEnter())
            {
                // Signals that mixer thread is now in low power stat
                OS_Smp_Mb(); // Read memory barrier: wmb_for_InLowPowerState coupled with: rmb_for_InLowPowerState
                OS_SetEventInterruptible(&LowPowerEnterEvent);

                // Forever wait for wake-up event
                OS_Status_t WaitStatus = OS_WaitForEventInterruptible(&LowPowerExitEvent, OS_INFINITE);
                if (WaitStatus == OS_INTERRUPTED)
                {
                    SE_INFO(group_mixer, "wait for LP exit interrupted; LowPowerExitEvent:%d\n", LowPowerExitEvent.Valid);
                }

                // Exiting low power state...
                PlaybackThreadLowPowerExit();
                // Reset the playback status because we have reset the players
                PlaybackStatus = true;

                // Signals that mixer thread is now back in running state
                OS_Smp_Mb(); // Read memory barrier: wmb_for_InLowPowerState coupled with: rmb_for_InLowPowerState
                OS_SetEventInterruptible(&LowPowerExitCompletedEvent);
            }
        }

        switch (ThreadState)
        {
        case Idle:
        {
            SE_VERBOSE(group_mixer, "%s: Idle\n", Name);
            MixerRequestManager.WaitForPendingRequest(5000);
        }
        break;

        case Starting:
        {
            //
            // Wait until the PCM player is configured and we can inject the initial lump of silence
            //
            if (0 == MixerPlayer[FirstActivePlayer].GetPcmPlayerConfigSurfaceParameters().PeriodParameters.SampleRateHz)
            {
                SE_DEBUG(group_mixer, "%s Starting: Waiting for PCM player to be initialized\n", Name);

                OS_Status_t WaitStatus = OS_WaitForEventAuto(&PcmPlayerSurfaceParametersUpdated, MIXER_THREAD_SURFACE_PARAMETER_UPDATE_EVENT_TIMEOUT);

                OS_ResetEvent(&PcmPlayerSurfaceParametersUpdated);

                if (OS_TIMED_OUT == WaitStatus)
                {
                    // Just arise a trace, cause this can happen if application is starting play-back and is meeting
                    // error immediately after. Application then stops play-back, with no surface parameters being updated.
                    // Mixer thread has to exit on timeout cause is still awaiting on this surface parameters update that will never come.
                    SE_INFO(group_mixer, "%s Starting: TIMEOUT for PcmPlayerSurfaceParametersUpdated Event\n", Name);
                }

                SE_DEBUG(group_mixer, "%s Starting: Got PcmPlayerSurfaceParametersUpdated Event\n", Name);
                // acknowledge any mixer clients that have stopped
                SetStoppingClientsToStoppedState();

                if (PcmPlayerNeedsParameterUpdate)
                {
                    SE_DEBUG(group_mixer, "%s Starting: About to update PCM player parameters\n", Name);
                    OS_Status_t Status = UpdatePcmPlayersParameters();
                    if (Status != PlayerNoError)
                    {
                        SE_ERROR("Starting: Failed to update soundcard parameters\n");
                        continue;
                    }
                }
            }
            else
            {
                Client[MasterClientIndex].LockTake();

                if (Client[MasterClientIndex].ManagedByThread())
                {
                    int32_t LivePolicyValue;
                    Client[MasterClientIndex].GetStream()->GetOption(PolicyLivePlayback, &LivePolicyValue); // Live Policy will be same for all the streams so check for only master client

                    if (LivePolicyValue != PolicyValueApply)
                    {
                        WaitNCommandForAudioToAudioSync = MIXER_NUM_PERIODS; // Give a chance for MIXER_NUM_PERIODS buffers for Main and Assosiated Audio Sync for non live policy
                    }
                }

                Client[MasterClientIndex].LockRelease();
                OS_Status_t Status;
                if (PcmPlayerNeedsParameterUpdate)
                {
                    SE_DEBUG(group_mixer, "%s Starting: About to update PCM player parameters\n", Name);
                    Status = UpdatePcmPlayersParameters();
                    if (Status != PlayerNoError)
                    {
                        SE_ERROR("Starting: Failed to update soundcard parameters\n");
                        continue;
                    }
                }

                Status = StartPcmPlayers();
                if (Status == PlayerNoError)
                {
                    if ((mTopology.type == STM_SE_MIXER_DUAL_STAGE_MIXING) && (MMEFirstMixerHdl == MME_INVALID_ARGUMENT))
                    {
                        InitializeMMETransformer(&FirstMixerMMEInitParams, true);
                    }

                    ThreadState = Playback;
                }
                else
                {
                    SE_ERROR("Starting: Failed to startup PCM player\n");
                    // no recovery possible (just wait for someone else to change the PCM parameters)
                }
            }
        }
        break;

        case Playback:
        {
            //
            // Now that the PCM player is running we can keep running the main loop (which injects silence
            // when there is nothing to play) until death.
            //

            //SE_DEBUG(group_mixer, "Mixer cycle %d\n", Cycles);
            if (PlaybackStatus != PlayerNoError)
            {
                consecutiveErrorCounter++;
                SE_ERROR("Playback:  %s is trying to recover the error\n", Name);

                if (consecutiveErrorCounter >= MIXER_NB_OF_ITERATION_WITH_ERROR_BEFORE_STOPPING)
                {
                    SE_INFO(group_mixer, "%s Playback: error is considered as not recoverable for a stopping client, so let's stop any STOPPING client\n", Name);
                    CleanUpOnError();
                }

                // ensure we don't live-lock if the sound system goes mad...
                OS_SleepMilliSeconds(100);
            }
            else
            {
                if (consecutiveErrorCounter > 0)
                {
                    SE_DEBUG(group_mixer, "Playback: error has been recovered\n");
                    consecutiveErrorCounter = 0;
                }
            }

            if (PcmPlayerNeedsParameterUpdate)
            {
                SE_DEBUG(group_mixer,  "Playback: About to update PCM player parameters\n");
                PlaybackStatus = UpdatePcmPlayersParameters();

                if (PlaybackStatus != PlayerNoError)
                {
                    SE_ERROR("Playback: Failed to update soundcard parameters\n");
                    continue;
                }
            }

            if (MMENeedsParameterUpdate)
            {
                SE_DEBUG(group_mixer,  "Playback: About to update parameters\n");
                PlaybackStatus = UpdateMixerParameters();

                if (PlaybackStatus != PlayerNoError)
                {
                    SE_ERROR("Playback: Failed to update mixer parameters\n");
                    continue;
                }

                UpdatePcmPlayersIec60958StatusBits();
            }

            PlaybackStatus = FillOutMixCommand();

            if (PlaybackStatus != PlayerNoError)
            {
                SE_ERROR("Playback: Failed to populate mix command\n");
                continue;
            }

            PlaybackStatus = SendMMEMixCommand();

            if (PlaybackStatus != PlayerNoError)
            {
                SE_ERROR("Playback: Unable to issue mix command\n");
                continue;
            }

            PlaybackStatus = WaitForMMECallback();

            if (PlaybackStatus != PlayerNoError)
            {
                // Error already prompted by callee
                // SE_ERROR( "Playback: Waiting for MME callback returned error\n" );
                continue;
            }

            // acknowledge any mixer clients that have stopped
            SetStoppingClientsToStoppedState();
        }
        break;

        default:
            SE_ERROR("Invalid PlaybackThread state %d\n", ThreadState);
        }
    }

    SE_DEBUG(group_mixer,  "Terminating\n");
    OS_Smp_Mb(); // Read memory barrier: rmb_for_Mixer_Terminating coupled with: wmb_for_Mixer_Terminating
    OS_SetEvent(&mPlaybackThreadOnOff);
}

////////////////////////////////////////////////////////////////////////////
///
/// Mixer processing thread actions on low power enter.
///
bool Mixer_Mme_c::PlaybackThreadLowPowerEnter(void)
{
    bool LowPowerEnterCompleted = false;

    // Check playback thread state
    if (ThreadState == Idle)
    {
        SE_INFO(group_mixer, "%s: entering low power..\n", Name);
        // The mixer is not started, we can enter low power immedialtely
        LowPowerEnterCompleted = true;
    }
    else
    {
        // The mixer is started
        // We set the output gain to 0 and wait for 3 thread loops, to be sure that the mixer output becomes null, and thus avoid audio glitch
        if (LowPowerEnterThreadLoop == 0)
        {
            SE_INFO(group_mixer, "%s: entering low power..\n", Name);
            // Force the ouput mixer gain to 0 to avoid audio glitch
            LowPowerPostMixGainSave = MixerSettings.MixerOptions.PostMixGain;
            MixerSettings.MixerOptions.PostMixGain = 0;
            // Update the flag
            MixerSettings.MixerOptions.PostMixGainUpdated = true;
        }
        else if (LowPowerEnterThreadLoop >= MIXER_NUMBER_OF_THREAD_LOOPS_BEFORE_LOW_POWER_ENTER)
        {
            // We are now ready to enter low power state
            LowPowerEnterCompleted = true;

            // Terminate MME transformer if needed
            IsLowPowerMMEInitialized = MMEInitialized;
            InLowPowerState          = true;

            if (IsLowPowerMMEInitialized)
            {
                MME_ERROR MMEStatus;
                MMEStatus = MME_TermTransformer(MMEHandle);

                if (MMEStatus != MME_SUCCESS)
                {
                    SE_ERROR("Error returned by MME_TermTransformer()\n");

                    InLowPowerState = false;
                    return true;
                }

                MMEHandle = 0;

                if (MMEFirstMixerHdl != MME_INVALID_ARGUMENT)
                {
                    MMEStatus = MME_TermTransformer(MMEFirstMixerHdl);

                    if (MMEStatus != MME_SUCCESS)
                    {
                        SE_ERROR("Error returned by MME_TermTransformer() on MMEFirstMixerHdl\n");
                        InLowPowerState = false;
                        return true;
                    }
                    LowPowerWakeUpFirstMixer = true;
                    MMEFirstMixerHdl = MME_INVALID_ARGUMENT;
                }

            }

            // Close the PcmPlayers
            TerminatePcmPlayers();
        }

        LowPowerEnterThreadLoop ++;
    }

    return LowPowerEnterCompleted;
}

////////////////////////////////////////////////////////////////////////////
///
/// Mixer processing thread actions on low power exit.
///
void Mixer_Mme_c::PlaybackThreadLowPowerExit(void)
{
    SE_INFO(group_mixer, "%s: exiting low power..\n", Name);

    // Re-initialize MME transformer if needed
    if (IsLowPowerMMEInitialized)
    {
        // Recreate the PcmPlayers
        InitializePcmPlayers();

        if (ThreadState == Playback)
        {
            PlayerStatus_t PlayerStatus = PlayerNoError;

            PlayerStatus = UpdatePcmPlayersParameters();

            if (PlayerStatus != PlayerNoError)
            {
                SE_ERROR("Starting: Failed to update soundcard parameters\n");
            }
            else
            {
                // Restart the PcmPlayers
                StartPcmPlayers();
            }
        }

        if (ThreadState != Idle)
        {
            // Restore output gain as it was before low power
            MixerSettings.MixerOptions.PostMixGain = LowPowerPostMixGainSave;
            // Update the flag
            MixerSettings.MixerOptions.PostMixGainUpdated = true;
        }

        MME_ERROR MMEStatus;
        MMEStatus = MME_InitTransformer(MMETransformerName, &MMEInitParams, &MMEHandle);
        // [bug31642] Force a SetGlobal after Init of Mixer TF.
        MMENeedsParameterUpdate = true;

        if (MMEStatus != MME_SUCCESS)
        {
            SE_ERROR("Error returned by MME_InitTransformer()\n");
            InLowPowerState = true;
        }
        else
        {
            InLowPowerState = false;
        }

        if (LowPowerWakeUpFirstMixer)
        {
            LowPowerWakeUpFirstMixer = false;
            MMEStatus = MME_InitTransformer(MMETransformerName, &FirstMixerMMEInitParams, &MMEFirstMixerHdl);
            // [bug31642] Force a SetGlobal after Init of Mixer TF.
            MMENeedsParameterUpdate = true;

            if (MMEStatus != MME_SUCCESS)
            {
                SE_ERROR("Error returned by MME_InitTransformer() FirstMixer\n");
                InLowPowerState = true;
            }
            else
            {
                InLowPowerState = false;
            }
        }
    }
    else
    {
        InLowPowerState = false;
    }
}

////////////////////////////////////////////////////////////////////////////
///
/// Handle a callback from MME.
///
/// In addition to handling the callback this function also alters the
/// priority of the callback thread (created by MME) such that it has
/// the same priority as the mixer collation thread. This ensures that
/// the delivery of the callback will not be obscured by lower priority
/// work.
///
void Mixer_Mme_c::CallbackFromMME(MME_Event_t Event, MME_Command_t *Command, bool isFirstMixer)
{
    SE_EXTRAVERB(group_mixer, ">%s\n", Name);

    switch (Event)
    {
    case MME_COMMAND_COMPLETED_EVT:
    {
        switch (Command->CmdCode)
        {
        case MME_TRANSFORM:
        {
            SE_VERBOSE(group_mixer, ">%s: %s MME_TRANSFORM\n", Name, isFirstMixer ? "FirstMixer" : "");
            if (isFirstMixer)
            {
                OS_SemaphoreSignal(&MMEFirstMixerCallbackSemaphore);
            }
            else
            {
                OS_SemaphoreSignal(&MMECallbackSemaphore);
            }
        }
        break;

        case MME_SET_GLOBAL_TRANSFORM_PARAMS:
        {
            SE_VERBOSE(group_mixer, ">%s: %s MME_SET_GLOBAL_TRANSFORM_PARAMS\n", Name, isFirstMixer ? "FirstMixer" : "");

            if (!isFirstMixer)
            {
                OS_SemaphoreSignal(&MMEParamCallbackSemaphore);
            }

            // boost the callback priority to be the same as the mixer process
            if (!MMECallbackThreadBoosted)
            {
                OS_SetPriority(&player_tasks_desc[SE_TASK_AUDIO_MIXER]);
                MMECallbackThreadBoosted = true;
            }
        }
        break;

        default:
        {
            SE_ERROR("%s Unexpected MME CmdCode (%d)\n", isFirstMixer ? "FirstMixer" : "", Command->CmdCode);
        }
        break;
        }
    }
    break;

    default:
        SE_ERROR("%s Unexpected MME event (%d)\n", isFirstMixer ? "FirstMixer" : "", Event);
        break;
    }
}

////////////////////////////////////////////////////////////////////////////
///
/// Send a request to setup the interactive input the id of which being
/// InteractiveId.
/// This is a synchronous function.
/// This function locks, sets event and waits event so be sure that you
/// call it in a thread state that allows that: in particular DO NOT CALL IT
/// in a spin locked code.
///
PlayerStatus_t Mixer_Mme_c::SendSetupAudioGeneratorRequest(Audio_Generator_c *aGenerator)
{
    PlayerStatus_t status;
    MixerRequest_c request;
    request.SetAudioGenerator(aGenerator);
    request.SetFunctionToManageTheRequest(&Mixer_Mme_c::SetupAudioGenerator);
    status = MixerRequestManager.SendRequest(request);

    if (status != PlayerNoError)
    {
        SE_ERROR(" Error returned by MixerRequestManager.SendRequest()\n");
    }

    return status;
}

////////////////////////////////////////////////////////////////////////////
///
/// A wrapper to the Mixer_Mme_c::SetupAudioGenerator( MixerRequest_c
///                                                       &aMixerRequest_c )
/// method.
///
PlayerStatus_t Mixer_Mme_c::SetupAudioGenerator(MixerRequest_c &aMixerRequest_c)
{
    PlayerStatus_t Status;
    Audio_Generator_c *aGenerator = aMixerRequest_c.GetAudioGenerator();

    if (aGenerator->IsInteractiveAudio())
    {
        Status = SetupInteractiveAudio(aGenerator);
    }
    else
    {
        Status = SetupAudioGenerator(aGenerator);
    }

    return Status;
}

////////////////////////////////////////////////////////////////////////////
///
/// Update the configuration of the application input.
///
/// Can only be called while the input is not already configured or if it
/// is stopped (or stopping).
///
PlayerStatus_t Mixer_Mme_c::SetupAudioGenerator(Audio_Generator_c *aGenerator)
{
    uint32_t AudioGeneratorIdx;

    if (mTopology.nb_max_application_audio <= NumberOfAudioGeneratorAttached)
    {
        SE_ERROR("Can't Attach: would exceed Topology.nb_max_application_audio=%d \n", mTopology.nb_max_application_audio);
        return PlayerNotSupported;
    }

    for (AudioGeneratorIdx = 0; AudioGeneratorIdx < MAX_AUDIO_GENERATORS; AudioGeneratorIdx++)
    {
        if (!Generator[AudioGeneratorIdx])
        {
            Generator[AudioGeneratorIdx] = aGenerator;
            NumberOfAudioGeneratorAttached++;
            break;
        }
    }

    if (AudioGeneratorIdx == MAX_AUDIO_GENERATORS)
    {
        SE_ERROR("Can't Attach:Mixer table is full\n");
        return PlayerError;
    }

    return SetAudioGeneratortoStoppedState(Generator[AudioGeneratorIdx]);
}

////////////////////////////////////////////////////////////////////////////
///
/// Update the configuration of the interactive input.
///
/// Can only be called while the input is not already configured or if it
/// is stopped (or stopping).
///
PlayerStatus_t Mixer_Mme_c::SetupInteractiveAudio(Audio_Generator_c *iAudio)
{
    uint32_t InteractiveAudioIdx;

    if (mTopology.nb_max_interactive_audio <= NumberOfInteractiveAudioAttached)
    {
        SE_ERROR("Can't Attach: would exceed Topology.nb_max_interactive_audio=%d \n", mTopology.nb_max_interactive_audio);
        return PlayerNotSupported;
    }

    for (InteractiveAudioIdx = 0; InteractiveAudioIdx < MAX_INTERACTIVE_AUDIO; InteractiveAudioIdx++)
    {
        if (!InteractiveAudio[InteractiveAudioIdx])
        {
            InteractiveAudio[InteractiveAudioIdx] = iAudio;
            NumberOfInteractiveAudioAttached++;
            break;
        }
    }

    if (InteractiveAudioIdx == MAX_INTERACTIVE_AUDIO)
    {
        SE_ERROR("Can't Attach:Mixer table is full\n");
        return PlayerError;
    }

    return SetAudioGeneratortoStoppedState(InteractiveAudio[InteractiveAudioIdx]);
}

PlayerStatus_t Mixer_Mme_c::SetAudioGeneratortoStoppedState(Audio_Generator_c *aGenerator)
{
    if (aGenerator->GetState() != STM_SE_AUDIO_GENERATOR_DISCONNECTED)
    {
        SE_ERROR("Not in DISCONNECTED state\n");
        return PlayerError;
    }

    aGenerator->SetState(STM_SE_AUDIO_GENERATOR_STOPPED);
    CheckClientsState();
    PcmPlayerNeedsParameterUpdate = true;
    MMENeedsParameterUpdate = true;
    AudioGeneratorUpdated = true;
    SE_DEBUG(group_mixer, "Set PcmPlayerSurfaceParametersUpdated Event\n");
    OS_SetEvent(&PcmPlayerSurfaceParametersUpdated);
    return PlayerNoError;
}

////////////////////////////////////////////////////////////////////////////
///
/// Send a request to free the interactive input the id of which being
/// InteractiveId.
/// This is a synchronous function.
/// This function locks, sets event and waits event so be sure that you
/// call it in a thread state that allows that: in particular DO NOT CALL IT
/// in a spin locked code.
///
PlayerStatus_t Mixer_Mme_c::SendFreeAudioGeneratorRequest(Audio_Generator_c *aGenerator)
{
    PlayerStatus_t status;
    MixerRequest_c request;
    request.SetAudioGenerator(aGenerator);
    request.SetFunctionToManageTheRequest(&Mixer_Mme_c::FreeAudioGenerator);
    status = MixerRequestManager.SendRequest(request);

    if (status != PlayerNoError)
    {
        SE_ERROR(" Error returned by MixerRequestManager.SendRequest()\n");
    }

    return status;
}
////////////////////////////////////////////////////////////////////////////
///
/// A wrapper to the Mixer_Mme_c::FreeAudioGenerator( int InteractiveId )
/// method.
///
PlayerStatus_t Mixer_Mme_c::FreeAudioGenerator(MixerRequest_c &aMixerRequest)
{
    PlayerStatus_t Status;
    Audio_Generator_c *aGenerator = aMixerRequest.GetAudioGenerator();

    if (aGenerator->IsInteractiveAudio())
    {
        Status = FreeInteractiveAudio(aGenerator);
    }
    else
    {
        Status = FreeAudioGenerator(aGenerator);
    }

    return Status;
}
////////////////////////////////////////////////////////////////////////////
///
/// Free an audio generator input identifier and associated resources.
///
/// Set the state to DISCONNECTED.
///
PlayerStatus_t Mixer_Mme_c::FreeAudioGenerator(Audio_Generator_c *aGenerator)
{
    uint32_t AudioGeneratorIdx;

    for (AudioGeneratorIdx = 0; AudioGeneratorIdx < MAX_AUDIO_GENERATORS; AudioGeneratorIdx++)
    {
        if (Generator[AudioGeneratorIdx] == aGenerator)
        {
            break;
        }
    }

    if (AudioGeneratorIdx == MAX_AUDIO_GENERATORS)
    {
        return -EINVAL;
    }
    else
    {
        Generator[AudioGeneratorIdx] = NULL;
        NumberOfAudioGeneratorAttached--;
    }

    Generator[AudioGeneratorIdx] = NULL;

    for (uint32_t ChannelIdx(0); ChannelIdx < SND_PSEUDO_MIXER_CHANNELS; ChannelIdx++)
    {
        MixerSettings.MixerOptions.AudioGeneratorGain[AudioGeneratorIdx][ChannelIdx] = Q3_13_UNITY;
    }

    aGenerator->SetState(STM_SE_AUDIO_GENERATOR_DISCONNECTED);
    return CheckClientsState();
}

////////////////////////////////////////////////////////////////////////////
///
/// Free an interative input identifier and associated resources.
///
///
/// Set the state to DISCONNECTED.
///
PlayerStatus_t Mixer_Mme_c::FreeInteractiveAudio(Audio_Generator_c *iAudio)
{
    uint32_t InteractiveAudioIdx;

    for (InteractiveAudioIdx = 0; InteractiveAudioIdx < MAX_INTERACTIVE_AUDIO; InteractiveAudioIdx++)
    {
        if (InteractiveAudio[InteractiveAudioIdx] == iAudio)
        {
            break;
        }
    }

    if (InteractiveAudioIdx == MAX_INTERACTIVE_AUDIO)
    {
        return -EINVAL;
    }
    else
    {
        InteractiveAudio[InteractiveAudioIdx] = NULL;
        NumberOfInteractiveAudioAttached--;
    }

    iAudio->SetState(STM_SE_AUDIO_GENERATOR_DISCONNECTED);
    return CheckClientsState();
}

////////////////////////////////////////////////////////////////////////////
///
/// Not called in Mixer context
///
PlayerStatus_t Mixer_Mme_c::StartAudioGenerator(Audio_Generator_c *aGenerator)
{
    stm_se_audio_generator_state_t curstate = aGenerator->GetState();
    SE_DEBUG(group_mixer, ">\n");

    if ((curstate == STM_SE_AUDIO_GENERATOR_STARTING) || (curstate == STM_SE_AUDIO_GENERATOR_STARTED))
    {
        SE_ERROR("Audio Generator already STARTED\n");
        return 0;
    }

    if ((curstate != STM_SE_AUDIO_GENERATOR_STOPPED) && (curstate != STM_SE_AUDIO_GENERATOR_STOPPING))
    {
        SE_ERROR("Can not start. Audio Generator not in STOPPED state %d\n", curstate);
        return -EINVAL;
    }

    aGenerator->SetState(STM_SE_AUDIO_GENERATOR_STARTING);
    MMENeedsParameterUpdate = true;
    SE_DEBUG(group_mixer, "<\n");
    return 0;
}

////////////////////////////////////////////////////////////////////////////
///
/// Not called in Mixer context
///
PlayerStatus_t Mixer_Mme_c::StopAudioGenerator(Audio_Generator_c *aGenerator)
{
    stm_se_audio_generator_state_t curstate = aGenerator->GetState();
    SE_DEBUG(group_mixer, ">\n");

    if ((curstate == STM_SE_AUDIO_GENERATOR_STOPPING) || (curstate == STM_SE_AUDIO_GENERATOR_STOPPED))
    {
        SE_ERROR("Audio Generator already STOPPED\n");
        return 0;
    }

    if ((curstate != STM_SE_AUDIO_GENERATOR_STARTED) && (curstate != STM_SE_AUDIO_GENERATOR_STARTING))
    {
        SE_ERROR("Can not stop. Audio Generator not in STARTED state\n");
        return -EINVAL;
    }

    aGenerator->SetState(STM_SE_AUDIO_GENERATOR_STOPPING);
    SE_DEBUG(group_mixer, "<\n");
    return 0;
}

////////////////////////////////////////////////////////////////////////////
///
/// Query Channel Configuration respresented as an eAccAcMode
//
/// If a channelAssinement has been set on mixer, the associated eAccAcMode is returned
//  else if all players shared the same ChannelAssignment, the associated eAccAcMode is returned
//  Undefined eAccAcMode (i.e. ACC_MODE_ID) is returned otherwise
//
PlayerStatus_t Mixer_Mme_c::GetChannelConfiguration(enum eAccAcMode *AcMode)
{
    *AcMode = ACC_MODE_ID;

    if (true == MixerPlayer[FirstActivePlayer].IsPlayerObjectAttached())
    {
        eAccAcMode MixerAcMode = TranslateChannelAssignmentToAudioMode(MixerSettings.MixerOptions.SpeakerConfig);

        if (MixerAcMode != ACC_MODE_ID)
        {
            *AcMode = MixerAcMode;
            SE_VERBOSE(group_mixer, "Mixer:%s Use Mixer's SpeakerConfig:%s\n", Name, LookupAudioMode(MixerAcMode));
        }
        else
        {
            eAccAcMode FirstPlayerAcMode = TranslateDownstreamCardToMainAudioMode(MixerPlayer[FirstActivePlayer].GetCard());
            eAccAcMode PlayerAcMode      = ACC_MODE_ID;

            // Check global promotion (all cards issue the same downmix)
            for (uint32_t CountOfAudioPlayerAttached(0), PlayerIdx(0);
                 CountOfAudioPlayerAttached < NumberOfAudioPlayerAttached;
                 PlayerIdx++)
            {
                if (true == MixerPlayer[PlayerIdx].IsPlayerObjectAttached())
                {
                    CountOfAudioPlayerAttached++;
                    PlayerAcMode = TranslateDownstreamCardToMainAudioMode(MixerPlayer[PlayerIdx].GetCard());
                    SE_VERBOSE(group_mixer,  "Mixer:%s Player:%s AccMode:%s",
                               Name, MixerPlayer[PlayerIdx].GetCardName(), LookupAudioMode(PlayerAcMode));

                    if (PlayerAcMode != FirstPlayerAcMode)
                    {
                        SE_VERBOSE(group_mixer,  "Mixer:%s players use different SpeakerConfig (%s versus %s)\n",
                                   Name, LookupAudioMode(FirstPlayerAcMode), LookupAudioMode(PlayerAcMode));
                        break;
                    }
                }
            }

            if (PlayerAcMode == FirstPlayerAcMode)
            {
                SE_VERBOSE(group_mixer,  "Mixer:%s All players share the same SpeakerConfig:%s\n",
                           Name, LookupAudioMode(FirstPlayerAcMode));
                *AcMode = FirstPlayerAcMode;
            }
        }
    }

    return PlayerNoError;
}


PlayerStatus_t Mixer_Mme_c::UpdateTransformerId(const char *transformerName)
{
    MME_ERROR MMEStatus;
    MME_TransformerCapability_t Capability = { 0 };
    MME_LxMixerTransformerInfo_t MixerInfo = { 0 };
    SE_DEBUG(group_mixer, ">%s transformerName:%s\n", Name, transformerName);

    Capability.StructSize = sizeof(Capability);
    Capability.TransformerInfo_p = &MixerInfo;
    Capability.TransformerInfoSize = sizeof(MixerInfo);

    MMEStatus = MME_GetTransformerCapability(transformerName, &Capability);
    if (MMEStatus != MME_SUCCESS)
    {
        SE_INFO(group_mixer, "Unable to read capabilities from:%s (err:%d)\n", transformerName, MMEStatus);
        SE_INFO(group_mixer, "%s Fallback to %s\n", Name, AUDIOMIXER_MT_NAME);

        //Fallback to default mixer Transformer name
        strncpy(MMETransformerName, AUDIOMIXER_MT_NAME, MMETransformerNameSize); // keep '\0'

        return PlayerMatchNotFound;
    }
    else
    {
        strncpy(MMETransformerName, transformerName, MMETransformerNameSize); // keep '\0'
    }

    return PlayerNoError;
}

////////////////////////////////////////////////////////////////////////////
///
/// Update MME mixer parameters.
///
/// If during the course of an update we determine that there new clients then
/// we must reconfigure ourselves (and potentially initialize MME and the
/// playback thread).
///
PlayerStatus_t Mixer_Mme_c::UpdateMixerParameters()
{
    MME_ERROR ErrorCode;
    SE_DEBUG(group_mixer, ">%s Waiting for parameter semaphore\n", Name);
    OS_SemaphoreWaitAuto(&MMEParamCallbackSemaphore);
    SE_DEBUG(group_mixer, "@%s Got the parameter semaphore\n", Name);
    MMENeedsParameterUpdate = false;
    // TODO: if the SamplingFrequency has changed we ought to terminate and re-init the transformer
    {
        for (uint32_t ClientIdx(0); ClientIdx < MIXER_MAX_CLIENTS; ClientIdx++)
        {
            Client[ClientIdx].LockTake();
        }

        if (MMEFirstMixerHdl != MME_INVALID_ARGUMENT)
        {
            FillOutFirstMixerGlobalParameters(&FirstMixerParamsCommand.InputParams);
        }
        FillOutTransformerGlobalParameters(&ParamsCommand.InputParams);

        for (uint32_t ClientIdx(0); ClientIdx < MIXER_MAX_CLIENTS; ClientIdx++)
        {
            Client[ClientIdx].LockRelease();
        }
    }

    if (MMEFirstMixerHdl != MME_INVALID_ARGUMENT)
    {
        ErrorCode = MME_SendCommand(MMEFirstMixerHdl, &FirstMixerParamsCommand.Command);
        if (ErrorCode != MME_SUCCESS)
        {
            SE_ERROR("Could not issue First Mixer parameter update command (%d)\n", ErrorCode);
            return PlayerError;
        }
    }

    ErrorCode = MME_SendCommand(MMEHandle, &ParamsCommand.Command);
    if (ErrorCode != MME_SUCCESS)
    {
        SE_ERROR("Could not issue parameter update command (%d)\n", ErrorCode);
        return PlayerError;
    }

    CheckAndStartClients();
    SE_DEBUG(group_mixer, "<%s\n", Name);
    return PlayerNoError;
}
////////////////////////////////////////////////////////////////////////////
///
/// Check and START clients that are in STARTING state
///
void Mixer_Mme_c::CheckAndStartClients(void)
{
    for (uint32_t ClientIdx(0); ClientIdx < MIXER_MAX_CLIENTS; ClientIdx++)
    {
        //Client[ClientIdx].LockTake(); Not Mandatory to be taken
        if (Client[ClientIdx].GetState() == STARTING)
        {
            Client[ClientIdx].SetState(STARTED);
            //Client[ClientIdx].LockRelease();
            // place the buffer descriptor (especially NumberOfScatterPages) to the 'silent' state.
            if (MMEFirstMixerHdl != MME_INVALID_ARGUMENT)
            {
                FillOutSilentBuffer(FirstMixerCommand.Command.DataBuffers_p[ClientIdx],
                                    &FirstMixerCommand.InputParams.InputParam[ClientIdx]);
                FirstMixerCommand.Command.DataBuffers_p[ClientIdx]->Flags = BUFFER_TYPE_AUDIO_IO | ClientIdx;
            }
            else
            {
                FillOutSilentBuffer(MixerCommand.Command.DataBuffers_p[ClientIdx],
                                    &MixerCommand.InputParams.InputParam[ClientIdx]);
                MixerCommand.Command.DataBuffers_p[ClientIdx]->Flags = BUFFER_TYPE_AUDIO_IO | ClientIdx;
            }
        }
    }

    for (uint32_t AudioGeneratorIdx = 0; AudioGeneratorIdx < MAX_AUDIO_GENERATORS; AudioGeneratorIdx++)
    {
        if (Generator[AudioGeneratorIdx] && Generator[AudioGeneratorIdx]->GetState() == STM_SE_AUDIO_GENERATOR_STARTING)
        {
            Generator[AudioGeneratorIdx]->SetState(STM_SE_AUDIO_GENERATOR_STARTED);
        }
    }

    for (uint32_t InteractiveAudioIdx = 0; InteractiveAudioIdx < MAX_INTERACTIVE_AUDIO; InteractiveAudioIdx++)
    {
        if (InteractiveAudio[InteractiveAudioIdx] &&
            InteractiveAudio[InteractiveAudioIdx]->GetState() == STM_SE_AUDIO_GENERATOR_STARTING)
        {
            InteractiveAudio[InteractiveAudioIdx]->SetState(STM_SE_AUDIO_GENERATOR_STARTED);
        }
    }
}

////////////////////////////////////////////////////////////////////////////
///
/// Lookup the index into Mixer_Mme_c::Clients of the supplied play-stream object
///
int Mixer_Mme_c::LookupClientFromStream(class HavanaStream_c *Stream) const
{
    int32_t ClientIdx;

    if (Stream == NULL)
    {
        return 0;    // This shortcut will only be valid until stlinuxtv provides proper Stream pointers.
    }

    for (ClientIdx = 0; (ClientIdx < MIXER_MAX_CLIENTS) && (false == Client[ClientIdx].IsMyStream(Stream)); ClientIdx++)
    {
        ;           // do nothing
    }

    if (ClientIdx >= MIXER_MAX_CLIENTS)
    {
        /* In case the abstraction layer still doesn't provide client objects as
           control params but direct indexes, then return the client as the index */
        int idx = (int) Stream;
        /* check for primary and secondary , then check for interactive audio indexes */
        ClientIdx = ((idx == STM_SE_CTRL_VALUE_AUDIO_MIXER_ROLE_PRIMARY)   ||
                     (idx == STM_SE_CTRL_VALUE_AUDIO_MIXER_ROLE_SECONDARY) ||
                     (idx == STM_SE_CTRL_VALUE_AUDIO_MIXER_ROLE_IN3)       ||
                     (idx == STM_SE_CTRL_VALUE_AUDIO_MIXER_ROLE_IN4))      ? idx : -1;

        ClientIdx = ((idx >= STM_SE_CTRL_VALUE_MIXER_ROLE_AUDIO_GENERATOR_0) && (idx <= STM_SE_CTRL_VALUE_MIXER_ROLE_AUDIO_GENERATOR_7)) ? idx : ClientIdx;
        ClientIdx = ((idx >= STM_SE_CTRL_VALUE_AUDIO_MIXER_ROLE_SFX0) && (idx <= STM_SE_CTRL_VALUE_AUDIO_MIXER_ROLE_SFX7)) ? idx : ClientIdx;
    }

    return ClientIdx;
}

////////////////////////////////////////////////////////////////////////////
///
/// Examine the downstream topology and determine the maximum frequency we might be asked to
/// support.
///
uint32_t Mixer_Mme_c::LookupPcmPlayersMaxSamplingFrequency() const
{
    uint32_t MaxFreq(Mixer_Player_c::MIN_FREQUENCY);

    for (uint32_t CountOfAudioPlayerAttached(0), PlayerIdx(0); CountOfAudioPlayerAttached < NumberOfAudioPlayerAttached; PlayerIdx++)
    {
        if (true == MixerPlayer[PlayerIdx].IsPlayerObjectAttached())
        {
            CountOfAudioPlayerAttached++;

            if (MixerPlayer[PlayerIdx].GetCardMaxFreq() > MaxFreq)
            {
                MaxFreq = MixerPlayer[PlayerIdx].GetCardMaxFreq();
            }
        }
    }

    return MaxFreq;
}

////////////////////////////////////////////////////////////////////////////
///
/// Examine the clients attached to the mixer and determine what sampling frequency to use.
///
uint32_t Mixer_Mme_c::LookupMixerSamplingFrequency() const
{
    uint32_t SFreq = 0;
    uint32_t ClientIdx;

    // Check if mixer sampling frequency was previously forced by STKPI control, i.e. STM_SE_FIXED_OUTPUT_FREQUENCY.
    if (MIXER_SAMPLING_FREQUENCY_STKPI_UNSET != InitializedSamplingFrequency)
    {
        SE_DEBUG(group_mixer, "<%s Mixer frequency %d (due to forcibly initialized frequency)\n",
                 Name,
                 InitializedSamplingFrequency);
        return InitializedSamplingFrequency;
    }


    // Check clients, that are registered and well known, so as to get their source sample frequency to use.
    for (ClientIdx = 0; ClientIdx < MIXER_MAX_CLIENTS; ClientIdx++)
    {
        // Client Lock already taken by caller.
        if (Client[ClientIdx].ManagedByThread())
        {
            // This client is correctly stated, and its parameters are well known.
            SFreq = Client[ClientIdx].GetParameters().Source.SampleRateHz;

            if (PrimaryClient == ClientIdx)
            {
                SE_DEBUG(group_mixer, "@%s Mixer frequency %d (due to primary manifestor)\n", Name, SFreq);
                break;
            }
            else
            {
                SE_DEBUG(group_mixer, "@%s Mixer frequency %d (due to secondary manifestor %d)\n", Name, SFreq, ClientIdx);
                break;
            }
        }
    }

    if (!SFreq)
    {
        if (!MixerSamplingFrequency)
        {
            for (uint32_t AudioGeneratorIdx(0); AudioGeneratorIdx < MAX_AUDIO_GENERATORS; AudioGeneratorIdx++)
            {
                if (Generator[AudioGeneratorIdx] && Generator[AudioGeneratorIdx]->GetState() == STM_SE_AUDIO_GENERATOR_STOPPED)
                {
                    SFreq = Generator[AudioGeneratorIdx]->GetSamplingFrequency();
                    SE_DEBUG(group_mixer, "@%s Mixer frequency %d (due to audio generator %d)\n", Name, SFreq, AudioGeneratorIdx);
                    break;
                }
            }
        }
        else
        {
            SE_DEBUG(group_mixer, "No input dictates mixer frequency (falling back to previous value of %d)\n",
                     MixerSamplingFrequency);
            return MixerSamplingFrequency;
        }
        // If audio generator has not set sampling frequency.
        if (!SFreq)
        {
            SE_DEBUG(group_mixer,  "No input dictates mixer frequency (falling back to 48KHz)\n");
            return 48000;
        }
    }

    uint32_t Funrefined = SFreq;
    // Determine the highest frequency the downstream output supports
    uint32_t PcmPlayersMaxSamplingFrequency = LookupPcmPlayersMaxSamplingFrequency();

    // cap the sampling frequency to the largest of the downstream max. output frequencies.
    // this reduces the complexity of the mix and (if we are lucky) of the subsequent PCM
    // post-processing. this translates to (milli)watts...
    while (SFreq > PcmPlayersMaxSamplingFrequency)
    {
        uint32_t Fprime = SFreq / 2;

        if (Fprime * 2 != SFreq)
        {
            SE_ASSERT(0);
            break;
        }

        SFreq = Fprime;
    }

    // boost low frequencies until they exceed the system minimum (if we are mixing we
    // don't want to drag the sample rate too low and harm the other sources).
    while (SFreq < Mixer_Player_c::MIN_FREQUENCY)
    {
        SFreq *= 2;
    }

    if (Funrefined != SFreq)
    {
        SE_DEBUG(group_mixer, "Refined mixer frequency to %d due to topological constraints\n", SFreq);
    }

    SE_DEBUG(group_mixer, "<%s Returning %d\n", Name, SFreq);
    return SFreq;
}


////////////////////////////////////////////////////////////////////////////
///
/// Examine the clients attached to the mixer and determine what audio mode the mixer will adopt
///
enum eAccAcMode Mixer_Mme_c::LookupMixerAudioMode() const
{
    // Client Lock already taken by caller.
    if (Client[PrimaryClient].ManagedByThread())
    {
        // This client is correctly stated, and its parameters are well known.
        return (enum eAccAcMode)Client[PrimaryClient].GetParameters().Organisation;
    }

    for (uint32_t ClientIdx(0); ClientIdx < MIXER_MAX_CLIENTS; ClientIdx++)
    {
        // Client Lock already taken by caller.
        if (Client[ClientIdx].ManagedByThread())
        {
            // This client is correctly stated, and its parameters are well known.
            return (enum eAccAcMode) Client[ClientIdx].GetParameters().Organisation;
        }
    }

    return ACC_MODE20;
}


////////////////////////////////////////////////////////////////////////////
///
/// Calculate, from the specified sampling frequency, the size of the mixer granule.
///
uint32_t Mixer_Mme_c::LookupMixerGranuleSize(uint32_t Frequency) const
{
    uint32_t Granule = MasterOutputGrain;
    SE_VERBOSE(group_mixer, ">: MasterOutputGrain: %d\n", MasterOutputGrain);

    // Check HDMI mode from PrimaryClient if any
    int HdmiRxMode   = PolicyValueHdmiRxModeDisabled;

    if (Client[PrimaryClient].GetState() != DISCONNECTED)
    {
        HavanaStream_c *Stream = Client[PrimaryClient].GetStream();
        if (Stream)
        {
            Stream->GetOption(PolicyHdmiRxMode, &HdmiRxMode);
        }
    }

    // If HdmiRx is in repeater mode , then force the grain to the minimal one
    // in order to meet the lowest latency.
    if (HdmiRxMode == PolicyValueHdmiRxModeRepeater)
    {
        // Adjust the grain to the SamplingFrequency and round to upper 128 samples
        Granule = SND_PSEUDO_MIXER_ADJUST_GRAIN(SND_PSEUDO_MIXER_MIN_GRAIN * Frequency / 48000);
    }
    else
    {
        if (Frequency > 48000)
        {
            Granule *= 2;
        }

        if (Frequency > 96000)
        {
            Granule *= 2;
        }
    }

    SE_DEBUG(group_mixer, "<%s Returning %d for %d Hz\n", Name, Granule, Frequency);
    return Granule;
}


PlayerStatus_t Mixer_Mme_c::IsDisconnected() const
{
    uint32_t ClientIdx;
    SE_DEBUG(group_mixer, ">><<\n");

    //
    // If everything is disconnected and no resource allocated then return true
    //

    for (ClientIdx = 0; ClientIdx < MIXER_MAX_CLIENTS && Client[ClientIdx].GetState() == DISCONNECTED; ClientIdx++)
        ; // do nothing

    if (ClientIdx < MIXER_MAX_CLIENTS)
    {
        //Something is connected
        return PlayerError;
    }

    // check for connected players || Transformer initialized || Pseudo mixer
    if (((FirstActivePlayer != MIXER_MME_NO_ACTIVE_PLAYER) && (true == MixerPlayer[FirstActivePlayer].IsPlayerObjectAttached()) && (true == MixerPlayer[FirstActivePlayer].HasPcmPlayer()))
        || (true == MMEInitialized))
    {
        SE_DEBUG(group_mixer, "<%s PlayerError\n", Name);
        return PlayerError;
    }

    return PlayerNoError;
}


////////////////////////////////////////////////////////////////////////////
///
/// Check mixer is idle or not
///
PlayerStatus_t Mixer_Mme_c::IsIdle() const
{
    if (PlaybackThreadRunning && (ThreadState == Idle))
    {
        SE_DEBUG(group_mixer, "<%s PlayerNoError\n", Name);
        return PlayerNoError;
    }

    SE_DEBUG(group_mixer, "<%s PlayerError\n", Name);
    return PlayerError;
}


////////////////////////////////////////////////////////////////////////////
///
/// Ensure other player components are notified of any applicable mixer settings.
///
/// At present the output timer must be informed of the master A/V sync offset (used
/// to equalize the audio and video latency) and the codec must be informed of the
/// currently desired dynamic range control.
///
/// Each of these commands is proxied via the manifestor. Strictly speaking
/// the mixer, as a sub-class of ::BaseComponentClass_c, is able to reach in directly
/// and poke at these settings but we prefer to give the manifestor visibility of this
/// activity (in part because it might choose to block the requests if the system state
/// in inappropriate.
///
/// Warning: This method is called by userspace threads that have updated the
///          mixer controls. There is therefore absolutely no predictability about
///          when in the startup/shutdown sequences is will be called.
///
PlayerStatus_t Mixer_Mme_c::UpdatePlayerComponentsModuleParameters()
{
    PlayerStatus_t         Result = PlayerNoError;
    SE_VERBOSE(group_mixer, ">:\n");

    // Send the prepared commands to each applicable client
    for (uint32_t ClientIdx(0); ClientIdx < MIXER_MAX_CLIENTS; ClientIdx++)
    {
        // There is no point in updating a STOPPED client since these parameters will be resent as soon as
        // the manifestor is enabled. In fact, updating STOPPED clients invites races during startup and
        // shutdown since this method is called, at arbitrary times, from userspace threads. It would be
        // safe to update STOPPING clients but only it we knew the remained in the STOPPING state after
        // making this check. The mixer thread doesn't take the mutex to transition between STOPPING and
        // STOPPED (deliberately - we don't want the real time mixer thread to be blocked waiting for a
        // userspace thread to be scheduled)) so this mean we must avoid updating STOPPING clients as well.
        // Client Lock already taken by caller.
        // bug 25004 : we can apply an SetModuleParameters() on clients if they are in UNCONFIGURED state
        //             as the client is already allocated
        if (Client[ClientIdx].ManagedByThread() || (Client[ClientIdx].GetState() == UNCONFIGURED))
        {
            Mixer_Client_c::MixerClientParameters ClientParameters = GetClientParameters();
            Result = Client[ClientIdx].UpdateManifestorModuleParameters(ClientParameters);

            if (Result == PlayerError)
            {
                break;
            }
        }
    }

    SE_VERBOSE(group_mixer, "<:\n");
    return Result;
}


////////////////////////////////////////////////////////////////////////////
///
/// Update our PCM player's IEC 60958 status bits.
///
/// This is called when the PCM players are initialized and whenever the
/// mixer values change.
///
void Mixer_Mme_c::UpdatePcmPlayersIec60958StatusBits()
{
    for (uint32_t CountOfAudioPlayerAttached(0), PlayerIdx(0); CountOfAudioPlayerAttached < NumberOfAudioPlayerAttached; PlayerIdx++)
    {
        if (true == MixerPlayer[PlayerIdx].IsPlayerObjectAttached())
        {
            CountOfAudioPlayerAttached++;

            if (true == MixerPlayer[PlayerIdx].HasPcmPlayer())
            {
                enum eIecValidity IecValidity = IEC_VALID_SAMPLES;
                (void)MixerPlayer[PlayerIdx].GetPcmPlayer()->SetIec60958StatusBits(&MixerSettings.OutputConfiguration.iec958_metadata);
                PcmPlayer_c::OutputEncoding OutPutEncoding = MixerPlayer[PlayerIdx].LookupOutputEncoding(PrimaryCodedDataType[MIXER_CODED_DATA_INPUT_SPDIF_IDX],
                                                                                                         PrimaryCodedDataType[MIXER_CODED_DATA_INPUT_HDMI_IDX]);
                // it is required to specify "NonLinearPCM" channel status bits  when output is bypassed
                // Bypass also possible for SPDIFIN_PCM. Exclude that for channel status update
                if ((PcmPlayer_c::IsOutputBypassed(OutPutEncoding) || PcmPlayer_c::IsOutputEncoded(OutPutEncoding)) && (OutPutEncoding != PcmPlayer_c::BYPASS_SPDIFIN_PCM))
                {
                    SE_DEBUG(group_mixer, "@: Card %d: %s is bypassed\n",
                             PlayerIdx,
                             MixerPlayer[PlayerIdx].GetCardName());
                    IecValidity = IEC_INVALID_SAMPLES;
                    (void)MixerPlayer[PlayerIdx].GetPcmPlayer()->SetIec61937StatusBits(&MixerSettings.OutputConfiguration.iec958_metadata);
                }

                (void)MixerPlayer[PlayerIdx].GetPcmPlayer()->SetIec60958ValidityBits(IecValidity);
            }
        }
    }
}


////////////////////////////////////////////////////////////////////////////
///
///
///
PlayerStatus_t Mixer_Mme_c::StartPlaybackThread()
{
    OS_Thread_t Thread;
    int         ThreadType;

    switch (mTopology.type)
    {
    case STM_SE_MIXER_DUAL_STAGE_MIXING:
        ThreadType = SE_TASK_AUDIO_BCAST_MIXER;
        break;

    case STM_SE_MIXER_BYPASS:
        ThreadType = SE_TASK_AUDIO_BYPASS_MIXER;
        break;

    case STM_SE_MIXER_SINGLE_STAGE_MIXING:
    default:
        ThreadType = SE_TASK_AUDIO_MIXER;
        break;
    }

    if (PlaybackThreadRunning)
    {
        SE_ERROR("Playback thread is already running\n");
        return PlayerError;
    }

    PlaybackThreadRunning = true;

    if (OS_CreateThread(&Thread, PlaybackThreadStub, this, &player_tasks_desc[ThreadType]) != OS_NO_ERROR)
    {
        SE_ERROR("Unable to create mixer playback thread\n");
        PlaybackThreadRunning = false;
        return PlayerError;
    }

    return PlayerNoError;
}


////////////////////////////////////////////////////////////////////////////
///
///
///
void Mixer_Mme_c::TerminatePlaybackThread()
{
    if (PlaybackThreadRunning)
    {
        SE_DEBUG(group_mixer, ">%s\n", Name);

        // Go out of idle mode
        MixerRequestManager.Stop();

        // Ask thread to terminate
        OS_ResetEvent(&mPlaybackThreadOnOff);
        OS_Smp_Mb(); // Write memory barrier: wmb_for_Mixer_Terminating coupled with: rmb_for_Mixer_Terminating
        PlaybackThreadRunning = false;

        // set any events the thread may be blocked waiting for
        SE_DEBUG(group_mixer, "%s Set PcmPlayerSurfaceParametersUpdated Event\n", Name);
        OS_SetEvent(&PcmPlayerSurfaceParametersUpdated);

        // wait for the thread to come to rest
        OS_WaitForEventAuto(&mPlaybackThreadOnOff, OS_INFINITE);
    }

    SE_ASSERT(!PlaybackThreadRunning);
}


////////////////////////////////////////////////////////////////////////////
///
/// Open the PCM player (or players) ready for them to be configured.
///
PlayerStatus_t Mixer_Mme_c::InitializePcmPlayers()
{
    SE_DEBUG(group_mixer,  ">%s\n", Name);
    PlayerStatus_t Status = PlayerNoError;

    // Instantiate based on the latched topology
    for (uint32_t PlayerIdx(0); PlayerIdx < STM_SE_MIXER_NB_MAX_OUTPUTS; PlayerIdx++)
    {
        Status |= InitializePcmPlayer(PlayerIdx);
    }

    if (PlayerNoError != Status)
    {
        Status = PlayerError;
    }

    SE_DEBUG(group_mixer,  "<%s NumberOfAudioPlayerInitialized:%d\n", Name, NumberOfAudioPlayerInitialized);
    return Status;
}

////////////////////////////////////////////////////////////////////////////
///
/// Open the PCM player ready for them to be configured.
///
PlayerStatus_t Mixer_Mme_c::InitializePcmPlayer(uint32_t PlayerIdx)
{
    SE_VERBOSE(group_mixer,  ">%s\n", Name);
    PlayerStatus_t Status = PlayerNoError;

    if (0 == NumberOfAudioPlayerAttached)
    {
        SE_ERROR("No player available\n");
        return PlayerError;
    }

    if ((true == MixerPlayer[PlayerIdx].IsPlayerObjectAttached()) && (false == MixerPlayer[PlayerIdx].HasPcmPlayer()))
    {
        Status = MixerPlayer[PlayerIdx].CreatePcmPlayer(
                     PrimaryCodedDataType[MIXER_CODED_DATA_INPUT_SPDIF_IDX],
                     PrimaryCodedDataType[MIXER_CODED_DATA_INPUT_HDMI_IDX]);

        if (PlayerNoError != Status)
        {
            SE_ERROR("Failed to manufacture PCM player[%d]\n", PlayerIdx);
        }
        else
        {
            NumberOfAudioPlayerInitialized++;
        }
    }

    SE_VERBOSE(group_mixer,  "<%s\n", Name);
    return Status;
}


////////////////////////////////////////////////////////////////////////////
///
/// Close the PCM player (or players).
///
void Mixer_Mme_c::TerminatePcmPlayers()
{
    SE_DEBUG(group_mixer, ">%s\n", Name);

    for (uint32_t CountOfAudioPlayerAttached(0), PlayerIdx(0); CountOfAudioPlayerAttached < NumberOfAudioPlayerAttached; PlayerIdx++)
    {
        if (true == MixerPlayer[PlayerIdx].IsPlayerObjectAttached())
        {
            CountOfAudioPlayerAttached++;

            if (true == MixerPlayer[PlayerIdx].HasPcmPlayer())
            {
                MixerPlayer[PlayerIdx].DeletePcmPlayer();
                NumberOfAudioPlayerInitialized--;
            }
        }
    }

    PcmPlayersStarted = false;
    SE_DEBUG(group_mixer,  "<\n");
}

////////////////////////////////////////////////////////////////////////////
///
/// Restart all the PCM player (or players) as at least of them failed
/// to map or commit samples
///
void Mixer_Mme_c::DeployPcmPlayersUnderrunRecovery()
{
    SE_DEBUG(group_mixer,  ">%s\n", Name);

    for (uint32_t CountOfAudioPlayerAttached(0), PlayerIdx(0);
         CountOfAudioPlayerAttached < NumberOfAudioPlayerAttached;
         PlayerIdx++)
    {
        if (true == MixerPlayer[PlayerIdx].IsPlayerObjectAttached())
        {
            CountOfAudioPlayerAttached++;

            if (true == MixerPlayer[PlayerIdx].HasPcmPlayer())
            {
                // No need to record the status of this method as the method will
                // itself print an error message if it fails.
                (void)MixerPlayer[PlayerIdx].GetPcmPlayer()->DeployUnderrunRecovery();
            }
        }
    }
}


////////////////////////////////////////////////////////////////////////////
///
///
///
PlayerStatus_t Mixer_Mme_c::MapPcmPlayersSamples(uint32_t SampleCount, bool NonBlock)
{
    for (uint32_t CountOfAudioPlayerAttached(0), PlayerIdx(0); CountOfAudioPlayerAttached < NumberOfAudioPlayerAttached; PlayerIdx++)
    {
        if (true == MixerPlayer[PlayerIdx].IsPlayerObjectAttached())
        {
            CountOfAudioPlayerAttached++;

            if (true == MixerPlayer[PlayerIdx].HasPcmPlayer())
            {
                PlayerStatus_t Status;
                Status = MixerPlayer[PlayerIdx].MapPcmPlayerSamples(SampleCount, NonBlock);

                if (PlayerNoError != Status)
                {
                    SE_ERROR("PcmPlayer %d refused to map %d samples\n", PlayerIdx, SampleCount);
                    return PlayerError;
                }
            }
        }
    }

    return PlayerNoError;
}


////////////////////////////////////////////////////////////////////////////
///
/// Commit any outstanding samples to the PCM player buffers.
///
PlayerStatus_t Mixer_Mme_c::CommitPcmPlayersMappedSamples()
{
    PlayerStatus_t Status;

    for (uint32_t CountOfAudioPlayerAttached(0), PlayerIdx(0); CountOfAudioPlayerAttached < NumberOfAudioPlayerAttached; PlayerIdx++)
    {
        MME_DataBuffer_t *OutputDataBuffer = MixerCommand.Command.DataBuffers_p[MixerCommand.Command.NumberInputBuffers + CountOfAudioPlayerAttached];

        if (true == MixerPlayer[PlayerIdx].IsPlayerObjectAttached())
        {
            CountOfAudioPlayerAttached++;

            if (true == MixerPlayer[PlayerIdx].HasPcmPlayer())
            {
                SE_VERBOSE(group_mixer, "@%s %s\n", Name, MixerPlayer[PlayerIdx].GetCardName());

                uPcmBufferFlags flags;
                flags.u32 = OutputDataBuffer->ScatterPages_p->FlagsOut;

                // extract the characteristics of the output buffer.
                enum eAccFsCode discrete_freq = (enum eAccFsCode) flags.bits.SamplingFreq;
                int32_t         iso_freq      = StmSeTranslateDiscreteSamplingFrequencyToInteger(discrete_freq);
                uint32_t        sample_freq   = (iso_freq > 0) ? (uint32_t) iso_freq : 0;
                enum eAccAcMode audio_mode    = (enum eAccAcMode) flags.bits.AudioMode;
                uDowmixInfoBF downmix_info;
                downmix_info.u8 = 0;


                // Despite the fact that we are in the context of the MixerThread
                // We can't use the cloneForMixer of the AudioPlayer because we aim at
                // updating the object and the cloneForMixer can only be updated through
                // CheckAndUpdatePlayerObject() method (it is a clone).
                // It is however safe to modify directly the player-object because the AudioInfoFrame
                // are only written from the mixer-thread (not read or written from the
                // middleware that controls the audio-player object).

                PcmPlayer_c::OutputEncoding OutputEncoding = MixerPlayer[PlayerIdx].LookupOutputEncoding(PrimaryCodedDataType[MIXER_CODED_DATA_INPUT_SPDIF_IDX],
                                                                                                         PrimaryCodedDataType[MIXER_CODED_DATA_INPUT_HDMI_IDX]);
                if (Client[PrimaryClient].ManagedByThread() && (OutputEncoding == PcmPlayer_c::BYPASS_SPDIFIN_PCM))
                {
                    ParsedAudioParameters_t PrimaryClientAudioParameters;
                    // This client is correctly stated, and its parameters are well known.
                    PrimaryClientAudioParameters     = Client[PrimaryClient].GetParameters();
                    downmix_info.bits.LFELevel       = PrimaryClientAudioParameters.SpdifInProperties.LfePlaybackLevel;
                    downmix_info.bits.LevelShift     = PrimaryClientAudioParameters.SpdifInProperties.LevelShiftValue;
                    downmix_info.bits.DownmixInhibit = PrimaryClientAudioParameters.SpdifInProperties.DownMixInhibit;
                }

                Status = MixerPlayer[PlayerIdx].UpdateAudioInfoFrame(sample_freq, audio_mode, downmix_info.u8);
                if (Status != PlayerNoError)
                {
                    SE_WARNING("Unable to update AudioInfoFrame for HDMI\n");
                }

                Status = MixerPlayer[PlayerIdx].CommitPcmPlayerMappedSamples(OutputDataBuffer->ScatterPages_p);


                if (Status != PlayerNoError)
                {
                    SE_ERROR("PcmPlayer failed to commit mapped samples\n");
                    DeployPcmPlayersUnderrunRecovery();
                    return PlayerError;
                }
            }
        }
    }

    return PlayerNoError;
}

////////////////////////////////////////////////////////////////////////////
///
/// Get and update the estimated display times.
///
PlayerStatus_t Mixer_Mme_c::GetAndUpdateDisplayTimeOfNextCommit()
{
    unsigned long long DisplayTimeOfNextCommit;
    PlayerStatus_t Status;
    SE_VERBOSE(group_mixer, ">\n");

    //
    // Calculate when the next commit will be displayed and appriase the manifestors of the
    // situation. Just use the first PCM player for this.
    //
    if (true == MixerPlayer[FirstActivePlayer].HasPcmPlayer())
    {
        Status = MixerPlayer[FirstActivePlayer].GetPcmPlayer()->GetTimeOfNextCommit(&DisplayTimeOfNextCommit);

        if (Status != PlayerNoError)
        {
            SE_ERROR("PCM player won't tell us its commit latency\n");
            return Status;
        }
    }
    else
    {
        SE_ERROR("PCM player cannot be found\n");
        return PlayerError;
    }

    for (uint32_t ClientIdx(0); ClientIdx < MIXER_MAX_CLIENTS; ClientIdx++)
    {
        Client[ClientIdx].LockTake();

        if (Client[ClientIdx].ManagedByThread() || (Client[ClientIdx].GetState() == STOPPING))
        {
            // This client is correctly stated, and its manifestor is well known.
            Client[ClientIdx].GetManifestor()->UpdateDisplayTimeOfNextCommit(DisplayTimeOfNextCommit);
        }

        Client[ClientIdx].LockRelease();
    }

    SE_VERBOSE(group_mixer, "<\n");
    return PlayerNoError;
}


////////////////////////////////////////////////////////////////////////////
///
/// Populate the PCM player buffers to ensure clean startup.
///
/// Clean startup means startup without immediate starvation. If the PCM
/// player buffers are not seeded with a starting value they rapidly consume
/// the data in the decode buffers. When the decode buffers are consumed the
/// mixer enters the starved state and mutes itself.
///
/// If the PCM player buffers have a start threshold of their own buffer
/// length then this method can simply mix in advance consuming samples from
/// the buffer queue. Alternatively, if the PCM player buffer is 'short' then
/// the buffer can simply be filled with silence.
///
PlayerStatus_t Mixer_Mme_c::StartPcmPlayers()
{
    SE_DEBUG(group_mixer, ">%s\n", Name);
    ManifestorStatus_t Status;
    uint32_t BufferSize = MixerPlayer[FirstActivePlayer].GetPcmPlayerConfigSamplesAreaSize();

    if (PcmPlayersStarted)
    {
        SE_ERROR("%s trying to start pcm players not terminated!\n", Name);
    }

    Status = MapPcmPlayersSamples(BufferSize, true); // non-blocking attempt to map

    if (Status != ManifestorNoError)
    {
        SE_ERROR("Failed to map all the PCM buffer samples (%u)\n", BufferSize);
        return ManifestorError;
    }

    SE_DEBUG(group_mixer, "Injecting %d muted samples\n", BufferSize);

    for (uint32_t CountOfAudioPlayerAttached(0), PlayerIdx(0); CountOfAudioPlayerAttached < NumberOfAudioPlayerAttached; PlayerIdx++)
    {
        if (true == MixerPlayer[PlayerIdx].IsPlayerObjectAttached())
        {
            CountOfAudioPlayerAttached++;

            if (true == MixerPlayer[PlayerIdx].HasPcmPlayer())
            {
                MixerPlayer[PlayerIdx].ResetPcmPlayerMappedSamplesArea();
            }
        }
    }

    Status = CommitPcmPlayersMappedSamples();

    if (Status != ManifestorNoError)
    {
        SE_ERROR("Failed to commit samples to the player\n");
        return ManifestorError;
    }

    Status = GetAndUpdateDisplayTimeOfNextCommit();

    if (PlayerNoError != Status)
    {
        SE_ERROR("Failed to update time of next commit\n");
        return ManifestorError;
    }

    PcmPlayersStarted = true;
    return ManifestorNoError;
}


////////////////////////////////////////////////////////////////////////////
///
/// Update sound card parameters.
///
PlayerStatus_t Mixer_Mme_c::UpdatePcmPlayersParameters()
{
    SE_DEBUG(group_mixer, ">\n");
    PlayerStatus_t Status;
    PcmPlayerSurfaceParameters_t NewSurfaceParameters[MIXER_AUDIO_MAX_OUTPUT_BUFFERS] = {{{0}}};
    bool RedundantUpdate = true;
    PcmPlayerNeedsParameterUpdate = false;
    // during an update the MixerSamplingFrequency becomes stale (it is updated after we have finished
    // updating the manifestor state). this method is called between these points and therefore must
    // perform a fresh lookup.
    NominalOutputSamplingFrequency = LookupMixerSamplingFrequency();
    uint32_t NominalMixerGranuleSize = LockClientAndLookupMixerGranuleSize(NominalOutputSamplingFrequency);
    SE_DEBUG(group_mixer, "@%s %u %u %u\n", Name, MixerSamplingFrequency, NominalOutputSamplingFrequency, InitializedSamplingFrequency);

    // record the parameters we need to set and check for redundancy
    for (uint32_t CountOfAudioPlayerAttached(0), PlayerIdx(0); CountOfAudioPlayerAttached < NumberOfAudioPlayerAttached; PlayerIdx++)
    {
        if (true == MixerPlayer[PlayerIdx].IsPlayerObjectAttached())
        {
            CountOfAudioPlayerAttached++;
            PcmPlayer_c::OutputEncoding OutputEncoding = MixerPlayer[PlayerIdx].LookupOutputEncoding(PrimaryCodedDataType[MIXER_CODED_DATA_INPUT_SPDIF_IDX],
                                                                                                     PrimaryCodedDataType[MIXER_CODED_DATA_INPUT_HDMI_IDX]);
            NewSurfaceParameters[PlayerIdx].PeriodParameters.BitsPerSample = 32;
            NewSurfaceParameters[PlayerIdx].PeriodParameters.ChannelCount = MixerPlayer[PlayerIdx].LookupOutputNumberOfChannels(PrimaryCodedDataType[MIXER_CODED_DATA_INPUT_SPDIF_IDX],
                                                                                                                                PrimaryCodedDataType[MIXER_CODED_DATA_INPUT_HDMI_IDX]);
            if (Client[PrimaryClient].ManagedByThread() && ((OutputEncoding == PcmPlayer_c::BYPASS_SPDIFIN_PCM) || (OutputEncoding == PcmPlayer_c::BYPASS_SPDIFIN_COMPRESSED)))
            {
                ParsedAudioParameters_t PrimaryClientAudioParameters;
                // This client is correctly stated, and its parameters are well known.
                PrimaryClientAudioParameters = Client[PrimaryClient].GetParameters();
                NewSurfaceParameters[PlayerIdx].PeriodParameters.ChannelCount = PrimaryClientAudioParameters.SpdifInProperties.ChannelCount; // For SPDIFIN bypass set ChannelCount based as in input
            }

            NewSurfaceParameters[PlayerIdx].PeriodParameters.SampleRateHz = NominalOutputSamplingFrequency;
            NewSurfaceParameters[PlayerIdx].ActualSampleRateHz = LookupPcmPlayerOutputSamplingFrequency(OutputEncoding, PlayerIdx);
            NewSurfaceParameters[PlayerIdx].PeriodSize = NominalMixerGranuleSize;
            NewSurfaceParameters[PlayerIdx].NumPeriods = MIXER_NUM_PERIODS;
            SE_VERBOSE(group_mixer, "Card %d: %s PeriodSize: %d ActualSampleRateHz:%d ChannelCount:%d\n",
                       PlayerIdx,
                       MixerPlayer[PlayerIdx].GetCardName(),
                       NewSurfaceParameters[PlayerIdx].PeriodSize,
                       NewSurfaceParameters[PlayerIdx].ActualSampleRateHz,
                       NewSurfaceParameters[PlayerIdx].PeriodParameters.ChannelCount);

            // Check whether this is any different to the existing configuration.
            if (0 != memcmp(&NewSurfaceParameters[PlayerIdx], &MixerPlayer[PlayerIdx].GetPcmPlayerConfigSurfaceParameters(), sizeof(NewSurfaceParameters[PlayerIdx])))
            {
                SE_DEBUG(group_mixer, "%s Mismatch for #%u: %u-vs-%u  %u-vs-%u  %u-vs-%u  %u-vs-%u  %u-vs-%u  %u-vs-%u\n",
                         Name,
                         PlayerIdx,
                         NewSurfaceParameters[PlayerIdx].PeriodParameters.BitsPerSample, MixerPlayer[PlayerIdx].GetPcmPlayerConfigSurfaceParameters().PeriodParameters.BitsPerSample,
                         NewSurfaceParameters[PlayerIdx].PeriodParameters.ChannelCount, MixerPlayer[PlayerIdx].GetPcmPlayerConfigSurfaceParameters().PeriodParameters.ChannelCount,
                         NewSurfaceParameters[PlayerIdx].PeriodParameters.SampleRateHz, MixerPlayer[PlayerIdx].GetPcmPlayerConfigSurfaceParameters().PeriodParameters.SampleRateHz,
                         NewSurfaceParameters[PlayerIdx].ActualSampleRateHz, MixerPlayer[PlayerIdx].GetPcmPlayerConfigSurfaceParameters().ActualSampleRateHz,
                         NewSurfaceParameters[PlayerIdx].PeriodSize, MixerPlayer[PlayerIdx].GetPcmPlayerConfigSurfaceParameters().PeriodSize,
                         NewSurfaceParameters[PlayerIdx].NumPeriods, MixerPlayer[PlayerIdx].GetPcmPlayerConfigSurfaceParameters().NumPeriods);
                RedundantUpdate = false;
            }
        }
    }

    if (RedundantUpdate)
    {
        SE_DEBUG(group_mixer,  "Ignoring redundant attempt to update the audio parameters\n");
        return PlayerNoError;
    }

    // we need to update all the PCM players (a parameter update involves stopping
    // and restarting a player so we need to make sure all players are treated
    // equally)

    for (uint32_t CountOfAudioPlayerAttached(0), PlayerIdx(0); CountOfAudioPlayerAttached < NumberOfAudioPlayerAttached; PlayerIdx++)
    {
        if (true == MixerPlayer[PlayerIdx].IsPlayerObjectAttached())
        {
            CountOfAudioPlayerAttached++;

            if (true == MixerPlayer[PlayerIdx].HasPcmPlayer())
            {
                Status = MixerPlayer[PlayerIdx].GetPcmPlayer()->SetParameters(&NewSurfaceParameters[PlayerIdx]);

                if (Status != PlayerNoError)
                {
                    if (NULL != MixerPlayer[PlayerIdx].GetPcmPlayerMappedSamplesArea())
                    {
                        PcmPlayerNeedsParameterUpdate = true;
                        SE_INFO(group_mixer, "%s Unable to modify pcm player settings at that time\n", Name);
                        return PlayerNoError;
                    }
                    else
                    {
                        SE_ERROR(" Fatal error attempting to reconfigure the PCM player\n");

                        // Restore the original settings. It is required for class consistency that all the attached
                        // PCM players remain in a playable state (e.g. so we can safely call ksnd_pcm_wait() on them)

                        for (uint32_t CountOfAudioPlayerAttached(0), PlayerIdx(0); CountOfAudioPlayerAttached < NumberOfAudioPlayerAttached; PlayerIdx++)
                        {
                            if (true == MixerPlayer[PlayerIdx].IsPlayerObjectAttached())
                            {
                                CountOfAudioPlayerAttached++;

                                if (true == MixerPlayer[PlayerIdx].HasPcmPlayer())
                                {
                                    NewSurfaceParameters[PlayerIdx] = MixerPlayer[PlayerIdx].GetPcmPlayerConfigSurfaceParameters();
                                    Status = MixerPlayer[PlayerIdx].GetPcmPlayer()->SetParameters(&NewSurfaceParameters[PlayerIdx]);
                                    SE_ASSERT(PlayerNoError == Status);
                                }
                            }
                        }

                        return PlayerError;
                    }
                }
            }
        }
    }

    // successfully deployed the update (to all players) so record the new surface settings
    for (uint32_t CountOfAudioPlayerAttached(0), PlayerIdx(0); CountOfAudioPlayerAttached < NumberOfAudioPlayerAttached; PlayerIdx++)
    {
        if (true == MixerPlayer[PlayerIdx].IsPlayerObjectAttached())
        {
            CountOfAudioPlayerAttached++;

            if (true == MixerPlayer[PlayerIdx].HasPcmPlayer())
            {
                MixerPlayer[PlayerIdx].SetPcmPlayerConfigSurfaceParameters(NewSurfaceParameters[PlayerIdx]);
            }
        }
    }

    // Check all player configurations as far period size is concerned.
    for (uint32_t CountOfAudioPlayerAttached(0), PlayerIdx(0); CountOfAudioPlayerAttached < NumberOfAudioPlayerAttached; PlayerIdx++)
    {
        if (true == MixerPlayer[PlayerIdx].IsPlayerObjectAttached())
        {
            CountOfAudioPlayerAttached++;

            if (MixerPlayer[PlayerIdx].GetPcmPlayerConfigSurfaceParameters().PeriodSize != NominalMixerGranuleSize)
            {
                SE_ERROR("Invalid PCM player period size for %u!\n", PlayerIdx);
                return PlayerError;
            }
            else
            {
                SE_DEBUG(group_mixer, "PcmPlayer %u parameters - NumPeriods %u PeriodSize %u\n",
                         PlayerIdx,
                         MixerPlayer[PlayerIdx].GetPcmPlayerConfigSurfaceParameters().NumPeriods,
                         MixerPlayer[PlayerIdx].GetPcmPlayerConfigSurfaceParameters().PeriodSize);
            }
        }
    }

    UpdatePcmPlayersIec60958StatusBits();
    // Of interest in case Mixer thread is in Starting state and surface parameters are still not set by application.
    SE_DEBUG(group_mixer, "%s Set PcmPlayerSurfaceParametersUpdated Event\n", Name);
    OS_SetEvent(&PcmPlayerSurfaceParametersUpdated);
    return PlayerNoError;
}

////////////////////////////////////////////////////////////////////////////
///
/// AllocateInterMixerRings
///
/// This method allocate buffers and RingBuffers required for MS12 dual Mixer setup
///
/// *Creates 2 RingBuffer:
///     + InterMixerFreeBufRing: store allocated unused buffers
///     + InterMixerFillBufRing: store allocated used/filled buffers
/// *Allocates MME buffers
/// *Stores allocated MME buffers within InterMixerFreeBufRing
///
PlayerStatus_t Mixer_Mme_c::AllocateInterMixerRings(unsigned int numberOfBuffer, unsigned int bufferSize)
{
    MME_DataBuffer_t *MMEBuf;

    SE_DEBUG(group_mixer, ">\n");

    /* Ensure Rings are freed before overriding references */
    FreeInterMixerRings();

    InterMixerFreeBufRing = new RingGeneric_c(numberOfBuffer, "Mixer_Mme_c::InterMixerFreeBufRing");
    InterMixerFillBufRing = new RingGeneric_c(numberOfBuffer, "Mixer_Mme_c::InterMixerFillBufRing");

    if (InterMixerFreeBufRing == NULL || InterMixerFillBufRing == NULL)
    {
        SE_ERROR("Failed to allocate Rings\n");
        FreeInterMixerRings();
        return PlayerError;
    }

    for (int i = 0; i < numberOfBuffer; i++)
    {
        if (MME_AllocDataBuffer(MMEHandle, bufferSize, MME_ALLOCATION_PHYSICAL, &MMEBuf) != MME_SUCCESS)
        {
            SE_ERROR("Failed to allocate interMixer MME PCM Buffer bufIdx:%d\n", i);
            FreeInterMixerRings();
            return PlayerError;
        }
        if (InterMixerFreeBufRing->Insert((uintptr_t) MMEBuf) != RingNoError)
        {
            SE_ERROR("Failed to insert into InterMixerFreeBufRing\n");
            MME_FreeDataBuffer(MMEBuf);
            FreeInterMixerRings();
            return PlayerError;
        }
    }
    return PlayerNoError;
}

void Mixer_Mme_c::FreeInterMixerRing(RingGeneric_c *Ring)
{
    MME_DataBuffer_t *MMEBuf;

    if (Ring != NULL)
    {
        while (Ring->Extract((uintptr_t *) &MMEBuf, RING_NONE_BLOCKING) == RingNoError)
        {
            MME_FreeDataBuffer(MMEBuf);
        }
        delete Ring;
    }

}

void Mixer_Mme_c::FreeInterMixerRings()
{
    SE_DEBUG(group_mixer, ">\n");
    FreeInterMixerRing(InterMixerFreeBufRing);
    FreeInterMixerRing(InterMixerFillBufRing);
}

////////////////////////////////////////////////////////////////////////////
///
/// MMETransformerSanityCheck
///
/// This method queries the capabilities of the mixer transformer Name input argument
/// and makes sure the mixer supports sufficient features to be usefully employed.
///
PlayerStatus_t Mixer_Mme_c::MMETransformerSanityCheck(const char *transformerName)
{
    MME_ERROR MMEStatus;
    MME_TransformerCapability_t Capability = { 0 };
    MME_LxMixerTransformerInfo_t MixerInfo = { 0 };

    // Obtain the capabilities of the mixer
    Capability.StructSize = sizeof(Capability);
    Capability.TransformerInfo_p = &MixerInfo;
    Capability.TransformerInfoSize = sizeof(MixerInfo);
    MMEStatus = MME_GetTransformerCapability(transformerName, &Capability);

    if (MMEStatus != MME_SUCCESS)
    {
        SE_ERROR("%s - Unable to read capabilities (%d)\n", transformerName, MMEStatus);
        return PlayerError;
    }

    SE_INFO(group_mixer, "%s Found %s transformer (version %x)\n", Name, transformerName, Capability.Version);
    SE_VERBOSE(group_mixer, "MixerCapabilityFlags = %08x\n", MixerInfo.MixerCapabilityFlags);
    SE_VERBOSE(group_mixer, "PcmProcessorCapabilityFlags[0..1] = %08x %08x\n",
               MixerInfo.PcmProcessorCapabilityFlags[0], MixerInfo.PcmProcessorCapabilityFlags[1]);

    // Verify that is it fit for purpose
    if (MixerInfo.StructSize != sizeof(MixerInfo))
    {
        SE_ERROR("Detected structure size skew between firmware and driver\n");
        return PlayerError;
    }
    return PlayerNoError;
}

////////////////////////////////////////////////////////////////////////////
///
/// Initialize the mixer transformer.
///
/// This method queries the capabilities of the mixer and after making sure
/// the mixer support sufficient features to be usefully employed initialized
/// a transformer and configures it.
///
/// Typically it will be configured with a 'holding pattern' since at this
/// stage it is unlikely that the proper configuration is known.
///

PlayerStatus_t Mixer_Mme_c::InitializeMMETransformer(MME_TransformerInitParams_t *initParams, bool isFirstMixer)
{
    MME_ERROR MMEStatus;
    SE_DEBUG(group_mixer, ">%s\n", Name);

    if (initParams == NULL)
    {
        SE_ERROR("initParams pointer is NULL\n");
        return PlayerError;
    }

    // Check FW driver compatibility
    if (MMETransformerSanityCheck(MMETransformerName) != PlayerNoError)
    {
        return PlayerError;
    }

    //
    // Initialize the transformer
    //
    *initParams = (MME_TransformerInitParams_t) {0}; // Ensure that we clear down the MMEInitParams
    initParams->StructSize = sizeof(*initParams);
    initParams->Priority = MME_PRIORITY_ABOVE_NORMAL; // we are more important than a decode...
    initParams->Callback = isFirstMixer ? MMEFirstMixerCallbackStub : MMECallbackStub;
    initParams->CallbackUserData = static_cast<void *>(this);
    initParams->TransformerInitParamsSize = sizeof(MixerParams);
    initParams->TransformerInitParams_p = &MixerParams;

    MixerParams = (MME_LxMixerTransformerInitBDParams_Extended_t) {0}; // Ensure that we clear down the MixerParams
    MixerParams.StructSize = sizeof(MixerParams);
    MixerParams.CacheFlush = ACC_MME_ENABLED;
    {
        uint32_t PrimaryClientEnableAudioCRC(0);
        Client[PrimaryClient].LockTake();

        if (true == Client[PrimaryClient].ManagedByThread())
        {
            // This client is correctly stated, and its manifestor is well known.
            PrimaryClientEnableAudioCRC = Client[PrimaryClient].GetManifestor()->EnableAudioCRC;
        }

        Client[PrimaryClient].LockRelease();
        MixerParams.MemConfig.Crc.CRC_Main = AudioConfiguration.CRC = PrimaryClientEnableAudioCRC;
        MixerParams.MemConfig.Crc.CRC_ALL = (PrimaryClientEnableAudioCRC == 2);
    }
    MixerParams.NbInput       = isFirstMixer ? MIXER_MAX_PCM_INPUTS : ACC_MIXER_MAX_NB_INPUT;
    MixerParams.DisableMixing = 0;
    MixerParams.BlockWise     = 0; // transform will be processed in subblocks of 256 ; (64 * (2^Blockwise));
    MixerParams.Index         = 0;

    // the large this value to more memory we'll need on the co-processor (if we didn't consider the topology
    // here we'ld blow our memory budget on STi710x)
    if ((MixerSettings.MixerOptions.MasterGrain) && (MixerSettings.MixerOptions.MasterGrain <= SND_PSEUDO_MIXER_MAX_GRAIN))
    {
        MasterOutputGrain = SND_PSEUDO_MIXER_ADJUST_GRAIN(MixerSettings.MixerOptions.MasterGrain);
    }
    else
    {
        MasterOutputGrain = SND_PSEUDO_MIXER_DEFAULT_GRAIN;
    }

    MixerParams.MaxNbOutputSamplesPerTransform = LockClientAndLookupMixerGranuleSize(LookupPcmPlayersMaxSamplingFrequency());
    // The interleaving of the output buffer that the host will send to the mixer at each transform.
    // Note that we are using the structure in a 'backwards compatible' mode. If we need to be more
    // expressive we must look at MME_OutChan_t.
    MixerParams.OutputNbChannels.Card = (FirstActivePlayer != MIXER_MME_NO_ACTIVE_PLAYER) ? MixerPlayer[FirstActivePlayer].GetCardNumberOfChannels() : 0;

    if ((true == MixerPlayer[0].IsPlayerObjectAttached()) && MixerPlayer[0].GetCard().alsaname[0] != '\0')
    {
        MixerParams.OutputNbChannels.SubCard.NbChanMain = MixerPlayer[0].GetCardNumberOfChannels();
        MixerParams.OutputNbChannels.SubCard.StrideMain = MixerParams.OutputNbChannels.SubCard.NbChanMain;
    }

    if ((true == MixerPlayer[1].IsPlayerObjectAttached()) && MixerPlayer[1].GetCardNumberOfChannels() != '\0')
    {
        MixerParams.OutputNbChannels.SubCard.NbChanAux = MixerPlayer[1].GetCardNumberOfChannels();
        MixerParams.OutputNbChannels.SubCard.StrideAux = MixerParams.OutputNbChannels.SubCard.NbChanAux;
    }

    if ((true == MixerPlayer[2].IsPlayerObjectAttached()) && MixerPlayer[2].GetCard().alsaname[0] != '\0')
    {
        MixerParams.OutputNbChannels.SubCard.NbChanDig = MixerPlayer[2].GetCardNumberOfChannels();
        MixerParams.OutputNbChannels.SubCard.StrideDig = MixerParams.OutputNbChannels.SubCard.NbChanDig;
    }

    if ((true == MixerPlayer[3].IsPlayerObjectAttached()) && MixerPlayer[3].GetCard().alsaname[0] != '\0')
    {
        MixerParams.OutputNbChannels.SubCard.NbChanHdmi = MixerPlayer[3].GetCardNumberOfChannels();
        MixerParams.OutputNbChannels.SubCard.StrideHdmi = MixerParams.OutputNbChannels.SubCard.NbChanHdmi;
    }

    SE_INFO(group_mixer,  "%s MixerParams.OutputNbChannels = 0x%08x\n", Name, MixerParams.OutputNbChannels.Card);

    // choose the output mode (either the sampling frequency of the mixer or a 'floating' value that can
    // be specified later)

    // Check if mixer sampling frequency possible forced by STKPI control, i.e. STM_SE_FIXED_OUTPUT_FREQUENCY
    if (STM_SE_FIXED_OUTPUT_FREQUENCY == MixerSettings.MixerOptions.OutputSfreq.control)
    {
        MixerParams.OutputSamplingFreq = StmSeTranslateIntegerSamplingFrequencyToDiscrete(MixerSettings.MixerOptions.OutputSfreq.frequency);
        InitializedSamplingFrequency = StmSeTranslateDiscreteSamplingFrequencyToInteger(MixerParams.OutputSamplingFreq);
        SE_INFO(group_mixer, "%s Mixer sampling frequency currently initialized to %d\n", Name, InitializedSamplingFrequency);
    }
    else
    {
        MixerParams.OutputSamplingFreq = (eAccFsCode)AUDIOMIXER_OVERRIDE_OUTFS;
        InitializedSamplingFrequency = MIXER_SAMPLING_FREQUENCY_STKPI_UNSET;
    }

    {
        for (uint32_t ClientIdx(0); ClientIdx < MIXER_MAX_CLIENTS; ClientIdx++)
        {
            Client[ClientIdx].LockTake();
        }

        // provide sane default values (to ensure they are never zero when consumed)
        MixerSamplingFrequency = NominalOutputSamplingFrequency = LookupMixerSamplingFrequency();
        if (isFirstMixer)
        {
            // Force update of MixerSettings to get a fully initilialized GlobalParams struct
            MixerSettings.MixerOptions.GainUpdated = true;
            MixerSettings.MixerOptions.PanUpdated = true;
            MixerSettings.MixerOptions.PostMixGainUpdated = true;

            FillOutFirstMixerGlobalParameters(&MixerParams.GlobalParams);

            if (AllocateInterMixerRings(INTERMIXER_BUFFER_COUNT, MAX_INTER_BUFFER_SIZE) != PlayerNoError)
            {
                SE_ERROR("Failed to allocate InterMixerRings\n");
                for (uint32_t ClientIdx(0); ClientIdx < MIXER_MAX_CLIENTS; ClientIdx++)
                {
                    Client[ClientIdx].LockRelease();
                }
                return PlayerError;
            }
        }
        else
        {
            MMENeedsParameterUpdate = false;
            FillOutTransformerGlobalParameters(&MixerParams.GlobalParams);
        }

        for (uint32_t ClientIdx(0); ClientIdx < MIXER_MAX_CLIENTS; ClientIdx++)
        {
            Client[ClientIdx].LockRelease();
        }
    }

    //As dynamic, we need to fixup the sizes we've declared
    MixerParams.StructSize -= (sizeof(MixerParams.GlobalParams) - MixerParams.GlobalParams.StructSize);
    initParams->TransformerInitParamsSize = MixerParams.StructSize;
    if (isFirstMixer)
    {
        MMEStatus = MME_InitTransformer(MMETransformerName, initParams, &MMEFirstMixerHdl);
    }
    else
    {
        MMEStatus = MME_InitTransformer(MMETransformerName, initParams, &MMEHandle);
    }

    if (MMEStatus != MME_SUCCESS)
    {
        SE_ERROR("%s Failed to initialize %s (%08x)\n",
                 isFirstMixer ? "FirstMixer" : "", MMETransformerName, MMEStatus);
        return PlayerError;
    }

    if (!isFirstMixer)
    {
        MMEInitialized = true;
        MMECallbackThreadBoosted = false;
    }
    CheckAndStartClients();
    SE_DEBUG(group_mixer, "<\n\n");
    return PlayerNoError;
}


////////////////////////////////////////////////////////////////////////////
///
/// Attempt to terminate the MME mix transformer.
///
/// Keep attempting to terminate the mix transformer until it returns an
/// status code other than MME_COMMAND_STILL_EXECUTING or it a timeout
/// occurs.
///
PlayerStatus_t Mixer_Mme_c::TerminateMMETransformer(MME_TransformerHandle_t MMEHdl)
{
    MME_ERROR Status;
    SE_DEBUG(group_mixer, ">%s\n", Name);

    if (MMEInitialized && (MMEHdl != MME_INVALID_ARGUMENT))
    {
        //
        // Wait a reasonable time for all mme transactions to terminate
        //
        // CAUTION about signed int !!
        int32_t TimeToWait(MIXER_MAX_WAIT_FOR_MME_COMMAND_COMPLETION);

        for (;;)
        {
            Status = MME_TermTransformer(MMEHdl);

            if (Status != MME_COMMAND_STILL_EXECUTING)
            {
                break;
            }
            else
            {
                if (TimeToWait > 0)
                {
                    TimeToWait -= 10;
                    OS_SleepMilliSeconds(10);
                }
                else
                {
                    break;
                }
            }
        }

        if (Status == MME_SUCCESS)
        {
            MMEHdl = MME_INVALID_ARGUMENT;
        }
        else
        {
            return PlayerError;
        }
    }

    return PlayerNoError;
}

PlayerStatus_t Mixer_Mme_c::TerminateMMETransformers(void)
{
    PlayerStatus_t Status = PlayerNoError;
    PlayerStatus_t Status1, Status2;

    Status1 = TerminateMMETransformer(MMEHandle);
    Status2 = TerminateMMETransformer(MMEFirstMixerHdl);
    if (Status1 != PlayerNoError)
    {
        SE_ERROR("Cannot Terminate MME Transformer");
        Status = Status1;
    }

    if (Status2 != PlayerNoError)
    {
        SE_ERROR("Cannot Terminate First MME Transformer");
        Status = Status2;
    }
    MMEInitialized = false;
    FreeInterMixerRings();

    return Status;
}

////////////////////////////////////////////////////////////////////////////
///
/// Issue the (already populated) command to the mixer transformer.
///
PlayerStatus_t Mixer_Mme_c::SendMMEMixCommand()
{
    MME_ERROR ErrorCode;

#define MIXER_INPUT_TRACE(mixCommand, name, inputId)                                       \
    SE_VERBOSE(group_mixer, "> %s [%p]"name": %15s (%d ScatterPages; BufferFlags:0x%X)\n", \
            Name, &mixCommand.InputParams.InputParam[inputId],                             \
            LookupMixerCommand(mixCommand.InputParams.InputParam[inputId].Command),        \
            mixCommand.DataBuffers[inputId].NumberOfScatterPages,                          \
            mixCommand.DataBuffers[inputId].Flags);

    if (SE_IS_VERBOSE_ON(group_mixer))
    {
        MIXER_INPUT_TRACE(MixerCommand, "input0          ", 0)
        MIXER_INPUT_TRACE(MixerCommand, "input1          ", 1)
        MIXER_INPUT_TRACE(MixerCommand, "input2          ", 2)
        MIXER_INPUT_TRACE(MixerCommand, "input3          ", 3)
        MIXER_INPUT_TRACE(MixerCommand, "SPDIF CodedInput", MIXER_CODED_DATA_INPUT + MIXER_CODED_DATA_INPUT_SPDIF_IDX)
        MIXER_INPUT_TRACE(MixerCommand, "HDMI  CodedInput", MIXER_CODED_DATA_INPUT + MIXER_CODED_DATA_INPUT_HDMI_IDX)
        MIXER_INPUT_TRACE(MixerCommand, "Interactive     ", MIXER_INTERACTIVE_INPUT)

        MixCommandStartTime = OS_GetTimeInMicroSeconds();
    }
    ErrorCode = MME_SendCommand(MMEHandle, &MixerCommand.Command);
    if (ErrorCode != MME_SUCCESS)
    {
        SE_ERROR("Could not issue mixer command (%d)\n", ErrorCode);
        return PlayerError;
    }

    if (MMEFirstMixerHdl != MME_INVALID_ARGUMENT)
    {
        if (SE_IS_VERBOSE_ON(group_mixer))
        {
            MIXER_INPUT_TRACE(FirstMixerCommand, "FirstMixer input0", 0)
            MIXER_INPUT_TRACE(FirstMixerCommand, "FirstMixer input1", 1)
            MIXER_INPUT_TRACE(FirstMixerCommand, "FirstMixer input2", 2)
            MIXER_INPUT_TRACE(FirstMixerCommand, "FirstMixer input3", 3)
        }
        ErrorCode = MME_SendCommand(MMEFirstMixerHdl, &FirstMixerCommand.Command);
        if (ErrorCode != MME_SUCCESS)
        {
            SE_ERROR("Could not issue First mixer command (%d)\n", ErrorCode);
            return PlayerError;
        }
    }

#undef MIXER_INPUT_TRACE

    return PlayerNoError;
}

////////////////////////////////////////////////////////////////////////////
///
///
///
void Mixer_Mme_c::ReleaseNonQueuedBuffersOfStoppingClients()
{
    for (uint32_t clientIndex = 0; clientIndex < MIXER_MAX_CLIENTS; clientIndex++)
    {
        if (Client[clientIndex].GetState() == STOPPING)
        {
            Manifestor_AudioKsound_c *clientManifestor = Client[clientIndex].GetManifestor();

            if (clientManifestor != NULL)
            {
                clientManifestor->ReleaseProcessingBuffers();
            }
            else
            {
                SE_ERROR("Client[clientIndex].GetManifestor() is NULL\n");
            }
        }
    }
}

////////////////////////////////////////////////////////////////////////////
///
///
///
void Mixer_Mme_c::CleanUpOnError()
{
    SE_DEBUG(group_mixer, ">\n");
    ReleaseNonQueuedBuffersOfStoppingClients();
    SetStoppingClientsToStoppedState();
}

static inline const char *LookupMixerState(enum eMixerState state)
{
    switch (state)
    {
#define E(x) case x: return #x
        E(MIXER_INPUT_NOT_RUNNING);
        E(MIXER_INPUT_RUNNING);
        E(MIXER_INPUT_FADING_OUT);
        E(MIXER_INPUT_MUTED);
        E(MIXER_INPUT_PAUSED);
        E(MIXER_INPUT_FADING_IN);
#undef E
    default:
        return "INVALID";
    }
}

////////////////////////////////////////////////////////////////////////////
///
/// Block until the MME delivers its callback and update the client structures.
///
/// \todo Review this method for unmuted stops (I think we need an extra STOPPING state so we are
///       aware whether or not soft mute has been applied yet).
///
PlayerStatus_t Mixer_Mme_c::WaitForMMECallback()
{
    PlayerStatus_t Status;
    SE_VERBOSE(group_mixer, ">%s\n", Name);
    OS_SemaphoreWaitAuto(&MMECallbackSemaphore);
    if (SE_IS_VERBOSE_ON(group_mixer))
    {
        SE_VERBOSE(group_mixer, "%s: MME Mix Command duration:%6lldus (now:%lldus)",
                   Name, OS_GetTimeInMicroSeconds() - MixCommandStartTime, OS_GetTimeInMicroSeconds());
        SE_VERBOSE(group_mixer, "> Client 0                   : State:%25s BytesUsed:%8d NbInSplNextTransform:%6d\n",
                   LookupMixerState(MixerCommand.OutputParams.MixStatus.InputStreamStatus[0].State),
                   MixerCommand.OutputParams.MixStatus.InputStreamStatus[0].BytesUsed,
                   MixerCommand.OutputParams.MixStatus.InputStreamStatus[0].NbInSplNextTransform);
        SE_VERBOSE(group_mixer, "@ Client 1                   : State:%25s BytesUsed:%8d NbInSplNextTransform:%6d\n",
                   LookupMixerState(MixerCommand.OutputParams.MixStatus.InputStreamStatus[1].State),
                   MixerCommand.OutputParams.MixStatus.InputStreamStatus[1].BytesUsed,
                   MixerCommand.OutputParams.MixStatus.InputStreamStatus[1].NbInSplNextTransform);
        SE_VERBOSE(group_mixer, "@ MIXER_CODED_DATA_INPUT     : State:%25s BytesUsed:%8d NbInSplNextTransform:%6d\n",
                   LookupMixerState(MixerCommand.OutputParams.MixStatus.InputStreamStatus[MIXER_CODED_DATA_INPUT].State),
                   MixerCommand.OutputParams.MixStatus.InputStreamStatus[MIXER_CODED_DATA_INPUT].BytesUsed,
                   MixerCommand.OutputParams.MixStatus.InputStreamStatus[MIXER_CODED_DATA_INPUT].NbInSplNextTransform);
        SE_VERBOSE(group_mixer, "@ MIXER_CODED_DATA_INPUT + 1 : State:%25s BytesUsed:%8d NbInSplNextTransform:%6d\n",
                   LookupMixerState(MixerCommand.OutputParams.MixStatus.InputStreamStatus[MIXER_CODED_DATA_INPUT + 1].State),
                   MixerCommand.OutputParams.MixStatus.InputStreamStatus[MIXER_CODED_DATA_INPUT + 1].BytesUsed,
                   MixerCommand.OutputParams.MixStatus.InputStreamStatus[MIXER_CODED_DATA_INPUT + 1].NbInSplNextTransform);
    }
    if (MixerCommand.Command.CmdStatus.State != MME_COMMAND_COMPLETED)
    {
        SE_ERROR("MME command failed (State %d Error %d)\n",
                 MixerCommand.Command.CmdStatus.State, MixerCommand.Command.CmdStatus.Error);
    }

    Status = UpdateOutputBuffers();

    if (MMEFirstMixerHdl != MME_INVALID_ARGUMENT)
    {
        OS_SemaphoreWaitAuto(&MMEFirstMixerCallbackSemaphore);
        if (SE_IS_VERBOSE_ON(group_mixer))
        {
            SE_VERBOSE(group_mixer, "@ FIRST MIXER: Client 0      : State:%25s BytesUsed:%8d NbInSplNextTransform:%6d\n",
                       LookupMixerState(FirstMixerCommand.OutputParams.MixStatus.InputStreamStatus[0].State),
                       FirstMixerCommand.OutputParams.MixStatus.InputStreamStatus[0].BytesUsed,
                       FirstMixerCommand.OutputParams.MixStatus.InputStreamStatus[0].NbInSplNextTransform);
            SE_VERBOSE(group_mixer, "@ FIRST MIXER: Client 1      : State:%25s BytesUsed:%8d NbInSplNextTransform:%6d\n",
                       LookupMixerState(FirstMixerCommand.OutputParams.MixStatus.InputStreamStatus[1].State),
                       FirstMixerCommand.OutputParams.MixStatus.InputStreamStatus[1].BytesUsed,
                       FirstMixerCommand.OutputParams.MixStatus.InputStreamStatus[1].NbInSplNextTransform);
        }

        Status = UpdateFirstMixerOutputBuffers();
        UpdateSecondMixerInputBuffer();
    }

    // Whatever the status of OutputBuffer updation generally because of overrun at player stage,
    // we should update input buffers as
    // these data have been processed and if we don't update the same data will be resent to
    // mixer on next transform leading to "inconsistent" output.
    for (uint32_t ClientIdx(0); ClientIdx < MIXER_MAX_CLIENTS; ClientIdx++)
    {
        UpdateInputBuffer(ClientIdx);
    }

    for (uint32_t AudioGeneratorIdx = 0; AudioGeneratorIdx < MAX_AUDIO_GENERATORS; AudioGeneratorIdx++)
    {
        UpdateAudioGenBuffer(AudioGeneratorIdx);
    }

    for (uint32_t InteractiveAudioIdx = 0; InteractiveAudioIdx < MAX_INTERACTIVE_AUDIO; InteractiveAudioIdx++)
    {
        UpdateInteractiveAudioBuffer(InteractiveAudioIdx);
    }

    if (Status != PlayerNoError)
    {
        SE_ERROR("Failed to update the output buffer\n");
        return PlayerError;
    }

    if (AudioConfiguration.CRC == true)
    {
        union
        {
            MME_MixerFrameExtStatus_t *s;
            MME_MixerStatusTemplate_t *t;
            MME_CrcStatus_t            *crc;
            char                       *p;
        } status_u;
        int bytes_used, ssize;
        status_u.p = &((char *) &MixerCommand.OutputParams)[MixerCommand.OutputParams.MixStatus.StructSize];
        bytes_used = status_u.s->BytesUsed;
        status_u.t = & status_u.s->MixExtStatus;

        while (bytes_used >= (int)sizeof(MME_MixerStatusTemplate_t))
        {
            ssize = status_u.t->StructSize;

            if ((status_u.t->Id == ACC_CRC_ID) && (bytes_used >= ssize))
            {
                st_relayfs_write_se(ST_RELAY_TYPE_AUDIO_MIXER_CRC, ST_RELAY_SOURCE_SE,
                                    (unsigned char *) &status_u.crc->Crc[0], sizeof(status_u.crc->Crc), 0);
            }

            bytes_used -= ssize;
            status_u.p += ssize;
        }
    }

    return PlayerNoError;
}


////////////////////////////////////////////////////////////////////////////
///
/// Provide sensible defaults for the output configuration.
///
void Mixer_Mme_c::ResetOutputConfiguration()
{
    SE_DEBUG(group_mixer, "\n");
    NumberOfAudioPlayerAttached = 0;
    FirstActivePlayer = MIXER_MME_NO_ACTIVE_PLAYER;
    MixerSettings.Reset();
}


////////////////////////////////////////////////////////////////////////////
///
/// Reset the 'meta data' gain/pan configuration.
///
/// If the mixer is configured to honour secondary stream meta data then
/// reset the associated values.
///
void Mixer_Mme_c::ResetMixingMetadata()
{
    SE_DEBUG(group_mixer, ">%s\n", Name);

    if (MixerSettings.OutputConfiguration.metadata_update != SND_PSEUDO_MIXER_METADATA_UPDATE_NEVER)
    {
        for (uint32_t ChannelIdx(0); ChannelIdx < SND_PSEUDO_MIXER_CHANNELS; ChannelIdx++)
        {
            MixerSettings.MixerOptions.Gain[PrimaryClient][ChannelIdx] = Q3_13_UNITY;
        }

        for (uint32_t ChannelIdx(0); ChannelIdx < SND_PSEUDO_MIXER_CHANNELS; ChannelIdx++)
        {
            MixerSettings.MixerOptions.Gain[SecondaryClient][ChannelIdx] = Q3_13_UNITY;
            MixerSettings.MixerOptions.Pan[SecondaryClient][ChannelIdx] = Q3_13_UNITY;
        }

        if (MixerSettings.OutputConfiguration.metadata_update == SND_PSEUDO_MIXER_METADATA_UPDATE_ALWAYS)
        {
            MixerSettings.MixerOptions.PostMixGain = Q3_13_UNITY;
        }
    }

    for (uint32_t AudioGeneratorIdx(0); AudioGeneratorIdx < MAX_AUDIO_GENERATORS; AudioGeneratorIdx++)
    {
        for (uint32_t ChannelIdx(0); ChannelIdx < SND_PSEUDO_MIXER_CHANNELS; ChannelIdx++)
        {
            MixerSettings.MixerOptions.AudioGeneratorGain[AudioGeneratorIdx][ChannelIdx] = Q3_13_UNITY;
        }
    }

    // Populate the flags with value "true" during reset so that structures dependent on the below params
    // in function Mixer_Mme_c::FillOutTransformerGlobalParameters() are filled
    MixerSettings.MixerOptions.GainUpdated = true;
    MixerSettings.MixerOptions.PanUpdated = true;
    MixerSettings.MixerOptions.PostMixGainUpdated = true;
    MixerSettings.MixerOptions.AudioGeneratorGainUpdated = true;
    MixerSettings.MixerOptions.InteractiveGainUpdated = true;
    MixerSettings.MixerOptions.InteractivePanUpdated = true;
    SE_DEBUG(group_mixer, "<%s\n", Name);
}

////////////////////////////////////////////////////////////////////////////
//
// FillOutGlobalPcmInputMixerConfig
//
void Mixer_Mme_c::FillOutGlobalPcmInputMixerConfig(MME_LxMixerInConfig_t *InConfig)
{
    MME_MixerInputConfig_t *clientConfig;

    for (uint32_t ClientIdx(0); ClientIdx < MIXER_MAX_CLIENTS; ClientIdx++)
    {
        clientConfig = &InConfig->Config[ClientIdx];

        // b[7..4] :: Input Type | b[3..0] :: Input Number
        clientConfig->InputId = (ACC_MIXER_LINEARISER << 4) + ClientIdx;
        clientConfig->Alpha = ACC_ALPHA_DONT_CARE;  // Mixing coefficients are given "Per Channel" through the BD Gain Set of coefficient
        clientConfig->Mono2Stereo = ACC_MME_TRUE;  // [enum eAccBoolean] Mono 2 Stereo upmix of a mono input
        clientConfig->Lpcm.WordSize = ACC_WS32;     // Input Lpcm.WordSize : ACC_WS32 / ACC_WS16
        // To which output channel is mixer the 1st channel of this input stream
        clientConfig->FirstOutputChan = ACC_MAIN_LEFT;
        clientConfig->AutoFade = ACC_MME_TRUE;
        clientConfig->Config = 0;

        // Client Lock already taken by caller.
        if (Client[ClientIdx].ManagedByThread())
        {
            // This client is correctly stated, and its parameters are well known.
            ParsedAudioParameters_t AudioParameters = Client[ClientIdx].GetParameters();
            clientConfig->NbChannels = AudioParameters.Source.ChannelCount; // Interleaving of the input pcm buffers
            clientConfig->AudioMode = (eAccAcMode) AudioParameters.Organisation;    //  Channel Configuration
            clientConfig->SamplingFreq = StmSeTranslateIntegerSamplingFrequencyToDiscrete(AudioParameters.Source.SampleRateHz);
        }
        else
        {
            // workaround a bug in the mixer (it crashes on transition from 0 channels to 8 channels)
            clientConfig->NbChannels = 8;
            clientConfig->AudioMode = ACC_MODE20;
            clientConfig->SamplingFreq = StmSeTranslateIntegerSamplingFrequencyToDiscrete(MixerSamplingFrequency);
        }
        SE_DEBUG(group_mixer,  "Input %d: AudioMode %s (%d) SamplingFreq %s\n",
                 ClientIdx,
                 LookupAudioMode(clientConfig->AudioMode),
                 clientConfig->AudioMode,
                 LookupDiscreteSamplingFrequency(clientConfig->SamplingFreq));
    }
}

////////////////////////////////////////////////////////////////////////////
//
// FillOutGlobalCodedInputMixerConfig
//
void Mixer_Mme_c::FillOutGlobalCodedInputMixerConfig(MME_LxMixerInConfig_t *InConfig, bool *IsOutputBypassed)
{
    // Describe inputs corresponding to bypass coded data.
    for (uint32_t CodedIndex = 0; CodedIndex < MIXER_MAX_CODED_INPUTS; CodedIndex++)
    {
        MME_MixerInputConfig_t &CodedDataInConfig = InConfig->Config[MIXER_CODED_DATA_INPUT + CodedIndex];
        CodedDataInConfig.InputId         = MIXER_CODED_DATA_INPUT + CodedIndex;
        CodedDataInConfig.NbChannels      = 2;
        CodedDataInConfig.Alpha           = 0; // N/A
        CodedDataInConfig.Mono2Stereo     = ACC_MME_FALSE; // N/A
        CodedDataInConfig.Lpcm.WordSize   = ACC_WS32;
        CodedDataInConfig.AudioMode       = ACC_MODE20;
        CodedDataInConfig.SamplingFreq    = StmSeTranslateIntegerSamplingFrequencyToDiscrete(MixerSamplingFrequency);
        CodedDataInConfig.FirstOutputChan = ACC_MAIN_LEFT; // N/A
        CodedDataInConfig.AutoFade        = ACC_MME_FALSE; // N/A
        CodedDataInConfig.Config          = 0;
        CodedDataInConfig.SfcFilterSelect = 0;
    }

    // Reset SPDIF Idx and HDMI PLayerIdx as it will be reevaluated in the following loop.
    mSPDIFCardId = STM_SE_MIXER_NB_MAX_OUTPUTS;
    mHDMICardId  = STM_SE_MIXER_NB_MAX_OUTPUTS;

    // Look for all possible coded inputs of mixer according cards and players
    for (uint32_t CountOfAudioPlayerAttached(0), PlayerIdx(0); CountOfAudioPlayerAttached < NumberOfAudioPlayerAttached; PlayerIdx++)
    {
        if (true == MixerPlayer[PlayerIdx].IsPlayerObjectAttached())
        {
            CountOfAudioPlayerAttached++;
            bool IsConnectedToHdmi = MixerPlayer[PlayerIdx].IsConnectedToHdmi();
            bool IsConnectedToSpdifPlayer = MixerPlayer[PlayerIdx].IsConnectedToSpdif();
            PcmPlayer_c::OutputEncoding PlayerEncoding = MixerPlayer[PlayerIdx].LookupOutputEncoding(
                                                             PrimaryCodedDataType[MIXER_CODED_DATA_INPUT_SPDIF_IDX],
                                                             PrimaryCodedDataType[MIXER_CODED_DATA_INPUT_HDMI_IDX]);

            // Connected to SPDIF
            if (IsConnectedToSpdifPlayer)
            {
                mSPDIFCardId = PlayerIdx;
                PcmPlayer_c::OutputEncoding SPDIFEncoding(PlayerEncoding);

                // Is there some SPDIF bypass
                if (PcmPlayer_c::IsOutputBypassed(SPDIFEncoding))
                {
                    MME_MixerInputConfig_t &CodedDataInConfig = InConfig->Config[MIXER_CODED_DATA_INPUT + MIXER_CODED_DATA_INPUT_SPDIF_IDX];
                    CodedDataInConfig.InputId += ACC_MIXER_COMPRESSED_DATA << 4;
                    CodedDataInConfig.Config = mSPDIFCardId;
                    CodedDataInConfig.SamplingFreq = StmSeTranslateIntegerSamplingFrequencyToDiscrete(LookupIec60958FrameRate(SPDIFEncoding));
                    SE_INFO(group_mixer, "%s Applying coded data bypass to PCM chain %d, (%s) , Config 0x%x\n",
                            Name,
                            mSPDIFCardId,
                            PcmPlayer_c::LookupOutputEncoding(SPDIFEncoding),
                            CodedDataInConfig.Config);
                    *IsOutputBypassed = true;
                }
            }

            // Connected to HDMI Only.
            if (IsConnectedToHdmi && !IsConnectedToSpdifPlayer)
            {
                mHDMICardId = PlayerIdx;
                PcmPlayer_c::OutputEncoding HDMIEncoding(PlayerEncoding);

                // Is there some HDMI bypass
                if (PcmPlayer_c::IsOutputBypassed(HDMIEncoding))
                {
                    MME_MixerInputConfig_t &CodedDataInConfig = InConfig->Config[MIXER_CODED_DATA_INPUT + MIXER_CODED_DATA_INPUT_HDMI_IDX];
                    // Client Lock already taken by caller.
                    if (Client[PrimaryClient].ManagedByThread() && ((HDMIEncoding == PcmPlayer_c::BYPASS_SPDIFIN_PCM  || (HDMIEncoding == PcmPlayer_c::BYPASS_SPDIFIN_COMPRESSED))))
                    {
                        ParsedAudioParameters_t PrimaryClientAudioParameters;
                        // This client is correctly stated, and its parameters are well known.
                        PrimaryClientAudioParameters = Client[PrimaryClient].GetParameters();
                        CodedDataInConfig.NbChannels = PrimaryClientAudioParameters.SpdifInProperties.ChannelCount;
                        CodedDataInConfig.AudioMode  = (eAccAcMode) PrimaryClientAudioParameters.SpdifInProperties.Organisation;
                    }


                    CodedDataInConfig.InputId += ACC_MIXER_COMPRESSED_DATA << 4;
                    CodedDataInConfig.Config = mHDMICardId;
                    CodedDataInConfig.SamplingFreq = StmSeTranslateIntegerSamplingFrequencyToDiscrete(LookupIec60958FrameRate(HDMIEncoding));
                    SE_INFO(group_mixer, "%s Applying coded data bypass to PCM chain %d, (%s) , Config 0x%x\n",
                            Name,
                            mHDMICardId,
                            PcmPlayer_c::LookupOutputEncoding(HDMIEncoding),
                            CodedDataInConfig.Config);
                    *IsOutputBypassed = true;
                }
            }
        }
    }
}

////////////////////////////////////////////////////////////////////////////
//
// FillOutGlobalInteractiveInputMixerConfig
//
void Mixer_Mme_c::FillOutGlobalInteractiveInputMixerConfig(MME_LxMixerInConfig_t *InConfig)
{
    // Describe inputs corresponding to interactive audio clients.
    MME_MixerInputConfig_t &IAudioInConfig = InConfig->Config[MIXER_INTERACTIVE_INPUT];
    IAudioInConfig.InputId = (ACC_MIXER_IAUDIO << 4) + MIXER_INTERACTIVE_INPUT;
    IAudioInConfig.NbChannels = InConfig->Config[PrimaryClient].NbChannels; // same as primary audio
    IAudioInConfig.Alpha = 0xffff;
    IAudioInConfig.Mono2Stereo = ACC_MME_FALSE;
    IAudioInConfig.Lpcm.WordSize = ACC_WS16;
    IAudioInConfig.AudioMode = ACC_MODE_ID;
    {
        uint32_t InterractiveClientIdx;

        for (InterractiveClientIdx = 0; (InterractiveClientIdx < MAX_INTERACTIVE_AUDIO); InterractiveClientIdx++)
            if (InteractiveAudio[InterractiveClientIdx])
            {
                break;
            }

        if (InterractiveClientIdx != MAX_INTERACTIVE_AUDIO)
            IAudioInConfig.SamplingFreq = StmSeTranslateIntegerSamplingFrequencyToDiscrete(
                                              InteractiveAudio[InterractiveClientIdx]->GetSamplingFrequency() ?
                                              InteractiveAudio[InterractiveClientIdx]->GetSamplingFrequency() :
                                              MixerSamplingFrequency);
        else
            IAudioInConfig.SamplingFreq = StmSeTranslateIntegerSamplingFrequencyToDiscrete(
                                              MixerSamplingFrequency);
    }
    IAudioInConfig.FirstOutputChan = ACC_MAIN_LEFT;
    IAudioInConfig.AutoFade = ACC_MME_FALSE; // no point when it plays all the time
    IAudioInConfig.Config = 0;
    IAudioInConfig.SfcFilterSelect = 0;
}


////////////////////////////////////////////////////////////////////////////
//
// FillOutGlobalGeneratorInputMixerConfig
//
void Mixer_Mme_c::FillOutGlobalGeneratorInputMixerConfig(MME_LxMixerInConfig_t *InConfig)
{
    MME_MixerInputConfig_t *GeneratorCfg;
    for (uint32_t AudioGeneratorIdx = 0; AudioGeneratorIdx < MAX_AUDIO_GENERATORS; AudioGeneratorIdx++)
    {
        if (!Generator[AudioGeneratorIdx])
        {
            continue;
        }
        GeneratorCfg = &InConfig->Config[AudioGeneratorIdx + MIXER_AUDIOGEN_DATA_INPUT];

        stm_se_audio_generator_buffer_t audiogenerator_buffer;
        // b[7..4] :: Input Type | b[3..0] :: Input Number
        GeneratorCfg->InputId         = (ACC_MIXER_LINEARISER << 4) + (AudioGeneratorIdx + MIXER_AUDIOGEN_DATA_INPUT);
        GeneratorCfg->NbChannels      = Generator[AudioGeneratorIdx]->GetNumChannels(); // same as primary audio
        GeneratorCfg->Alpha           = ACC_ALPHA_DONT_CARE;
        GeneratorCfg->Mono2Stereo     = ACC_MME_FALSE;
        GeneratorCfg->AudioMode       = Generator[AudioGeneratorIdx]->GetAudioMode();
        GeneratorCfg->Lpcm.Definition = 1;

        if (Generator[AudioGeneratorIdx]->GetCompoundOption(STM_SE_CTRL_AUDIO_GENERATOR_BUFFER, &audiogenerator_buffer))
        {
            continue;
        }

        switch (audiogenerator_buffer.format)
        {
        case STM_SE_AUDIO_PCM_FMT_S32LE:
        default:
            GeneratorCfg->Lpcm.WordSize = ACC_LPCM_WS32;
            break;

        case STM_SE_AUDIO_PCM_FMT_S32BE:
            GeneratorCfg->Lpcm.WordSize = ACC_LPCM_WS32be;
            break;

        case STM_SE_AUDIO_PCM_FMT_S24LE:
            GeneratorCfg->Lpcm.WordSize = ACC_LPCM_WS24;
            break;

        case STM_SE_AUDIO_PCM_FMT_S24BE:
            GeneratorCfg->Lpcm.WordSize = ACC_LPCM_WS24be;
            break;

        case STM_SE_AUDIO_PCM_FMT_S16LE:
            GeneratorCfg->Lpcm.WordSize = ACC_LPCM_WS16le;
            break;

        case STM_SE_AUDIO_PCM_FMT_S16BE:
            GeneratorCfg->Lpcm.WordSize = ACC_LPCM_WS16;
            break;

        case STM_SE_AUDIO_PCM_FMT_U16LE:
            GeneratorCfg->Lpcm.WordSize = ACC_LPCM_WS16ule;
            break;

        case STM_SE_AUDIO_PCM_FMT_U16BE:
            GeneratorCfg->Lpcm.WordSize = ACC_LPCM_WS16u;
            break;

        case STM_SE_AUDIO_PCM_FMT_S8:
            GeneratorCfg->Lpcm.WordSize = ACC_LPCM_WS8s;
            break;

        case STM_SE_AUDIO_PCM_FMT_U8:
            GeneratorCfg->Lpcm.WordSize = ACC_LPCM_WS8u;
            break;

        case STM_SE_AUDIO_PCM_FMT_ALAW_8:
            GeneratorCfg->Lpcm.WordSize = ACC_LPCM_WS8A;
            break;

        case STM_SE_AUDIO_PCM_FMT_ULAW_8:
            GeneratorCfg->Lpcm.WordSize = ACC_LPCM_WS8Mu;
            break;
        }

        GeneratorCfg->SamplingFreq = StmSeTranslateIntegerSamplingFrequencyToDiscrete(
                                         Generator[AudioGeneratorIdx]->GetSamplingFrequency() ?
                                         Generator[AudioGeneratorIdx]->GetSamplingFrequency() :
                                         MixerSamplingFrequency);
        // To which output channel is mixer the 1st channel of
        // this input stream
        GeneratorCfg->FirstOutputChan = ACC_MAIN_LEFT;
        GeneratorCfg->AutoFade = ACC_MME_FALSE;
        GeneratorCfg->Config = 0;
        GeneratorCfg->SfcFilterSelect = 0;
    }
}

////////////////////////////////////////////////////////////////////////////
//
// FillOutGlobalInputGainConfig
//
void Mixer_Mme_c::FillOutGlobalInputGainConfig(MME_LxMixerGainSet_t *InGainConfig)
{
    for (uint32_t ClientIdx(0); ClientIdx < ACC_MIXER_MAX_NB_INPUT; ClientIdx++)
    {
        InGainConfig->GainSet[ClientIdx].InputId = ClientIdx;

        for (uint32_t ChannelIdx(0); ChannelIdx < SND_PSEUDO_MIXER_CHANNELS; ChannelIdx++)
        {
            U16 *AlphaPerChannel = &InGainConfig->GainSet[ClientIdx].AlphaPerChannel[ChannelIdx];
            if (ClientIdx < MIXER_MAX_CLIENTS)
            {
                *AlphaPerChannel = MixerSettings.MixerOptions.Gain[ClientIdx][ChannelIdx];
            }
            else if ((ClientIdx >= MIXER_AUDIOGEN_DATA_INPUT && ClientIdx < MIXER_AUDIOGEN_DATA_INPUT + MIXER_MAX_AUDIOGEN_INPUTS))
            {
                *AlphaPerChannel = MixerSettings.MixerOptions.AudioGeneratorGain[ClientIdx - MIXER_AUDIOGEN_DATA_INPUT][ChannelIdx];
            }
            else if (ClientIdx == MIXER_INTERACTIVE_INPUT)
            {
                *AlphaPerChannel = MixerSettings.MixerOptions.InteractiveGain[ChannelIdx];
            }
            else
            {
                *AlphaPerChannel = Q3_13_UNITY;
            }
        }
    }
}

////////////////////////////////////////////////////////////////////////////
//
// FillOutGlobalInputPanningConfig
//
void Mixer_Mme_c::FillOutGlobalInputPanningConfig(MME_LxMixerPanningSet_t *InPanningConfig)
{
    // Preset Panning for Application and System Sounds (injected through AudioGenerators)
    // The Panning Coefficients are applicable to mono input only and we expect that mono  Application sounds
    // be always mixed on Left and Right (panning for other channels have been set to 0 globally)
    // In order to equally diffuse the acoustical power of a mono signal over stereo speakers,
    // it should be panned with -3dB on left / right

    for (uint32_t InputIdx(MIXER_AUDIOGEN_DATA_INPUT);
         InputIdx < MIXER_AUDIOGEN_DATA_INPUT + MIXER_MAX_AUDIOGEN_INPUTS;
         InputIdx++)
    {
        InPanningConfig->PanningSet[InputIdx].InputId                = InputIdx;
        InPanningConfig->PanningSet[InputIdx].Panning[ACC_MAIN_LEFT] = Q3_13_M3DB;
        InPanningConfig->PanningSet[InputIdx].Panning[ACC_MAIN_RGHT] = Q3_13_M3DB;
    }

    // Set the Panning for PlayStream clients.
    for (uint32_t ClientIdx(0); ClientIdx < MIXER_MAX_CLIENTS; ClientIdx++)
    {
        InPanningConfig->PanningSet[ClientIdx].InputId = ClientIdx;

        for (uint32_t ChannelIdx(0); ChannelIdx < SND_PSEUDO_MIXER_CHANNELS; ChannelIdx++)
        {
            if ((ClientIdx == PrimaryClient) || (ClientIdx == MIXER_INTERACTIVE_INPUT))
            {
                InPanningConfig->PanningSet[ClientIdx].Panning[ChannelIdx] = Q3_13_UNITY;
            }
            else
            {
                // Case of secondary client.
                InPanningConfig->PanningSet[ClientIdx].Panning[ChannelIdx] =
                    MixerSettings.MixerOptions.Pan[ClientIdx][ChannelIdx];
            }
        }
    }
}

////////////////////////////////////////////////////////////////////////////
//
// FillOutGlobalInteractiveAudioConfig
//
void Mixer_Mme_c::FillOutGlobalInteractiveAudioConfig(MME_LxMixerInIAudioConfig_t *InIaudioConfig)
{
    MME_MixerIAudioInputConfig_t *clientConfig;
    for (uint32_t InterractiveClientIdx(0); InterractiveClientIdx < MAX_INTERACTIVE_AUDIO; InterractiveClientIdx++)
    {
        clientConfig = &InIaudioConfig->ConfigIAudio[InterractiveClientIdx];
        clientConfig->InputId = InterractiveClientIdx;

        if (InteractiveAudio[InterractiveClientIdx])
        {
            clientConfig->NbChannels = InteractiveAudio[InterractiveClientIdx]->GetNumChannels();
            clientConfig->Alpha = MixerSettings.MixerOptions.InteractiveGain[InterractiveClientIdx];

            for (uint32_t ChannelIdx(0); ChannelIdx < SND_PSEUDO_MIXER_CHANNELS; ChannelIdx++)
            {
                clientConfig->Panning[ChannelIdx] = MixerSettings.MixerOptions.InteractivePan[InterractiveClientIdx][ChannelIdx];
            }

            clientConfig->Play = ACC_MME_TRUE;
        }
        else
        {
            // we set the gain of unused inputs to zero in order to get a de-pop during startup
            // due to gain smoothing
            clientConfig->NbChannels = 1;
            clientConfig->Alpha = 0;

            for (uint32_t ChannelIdx(0); ChannelIdx < SND_PSEUDO_MIXER_CHANNELS; ChannelIdx++)
            {
                clientConfig->Panning[ChannelIdx] = 0;
            }

            clientConfig->Play = ACC_MME_FALSE;
        }
    }
}

////////////////////////////////////////////////////////////////////////////
//
// FillOutGlobalOutputTopology
//
void Mixer_Mme_c::FillOutGlobalOutputTopology(MME_LxMixerOutTopology_t *OutTopology)
{
    if (FirstActivePlayer != MIXER_MME_NO_ACTIVE_PLAYER)
    {
        OutTopology->Topology.NbChannels = MixerPlayer[FirstActivePlayer].GetCardNumberOfChannels();

        if ((true == MixerPlayer[0].IsPlayerObjectAttached()) && MixerPlayer[0].GetCard().alsaname[0] != '\0')
        {
            OutTopology->Topology.Output.NbChanMain = MixerPlayer[0].GetCardNumberOfChannels();
            OutTopology->Topology.Output.StrideMain = MixerPlayer[0].GetCardNumberOfChannels();
        }

        if ((true == MixerPlayer[1].IsPlayerObjectAttached()) && MixerPlayer[1].GetCardNumberOfChannels() != '\0')
        {
            OutTopology->Topology.Output.NbChanAux = MixerPlayer[1].GetCardNumberOfChannels();
            OutTopology->Topology.Output.StrideAux = MixerPlayer[1].GetCardNumberOfChannels();
        }

        if ((true == MixerPlayer[2].IsPlayerObjectAttached()) && MixerPlayer[2].GetCard().alsaname[0] != '\0')
        {
            OutTopology->Topology.Output.NbChanDig = MixerPlayer[2].GetCardNumberOfChannels();
            OutTopology->Topology.Output.StrideDig = MixerPlayer[2].GetCardNumberOfChannels();
        }

        if ((true == MixerPlayer[3].IsPlayerObjectAttached()) && MixerPlayer[3].GetCard().alsaname[0] != '\0')
        {
            OutTopology->Topology.Output.NbChanHdmi = MixerPlayer[3].GetCardNumberOfChannels();
            OutTopology->Topology.Output.StrideHdmi = MixerPlayer[3].GetCardNumberOfChannels();
        }
    }
    else
    {
        OutTopology->Topology.NbChannels = 0;
    }
}

////////////////////////////////////////////////////////////////////////////
//
// UpdateOutputTopology
//
void Mixer_Mme_c::UpdateOutputTopology(MME_LxMixerOutTopology_t *OutTopology, int BypassPlayerId, int BypassChannelCount)
{
    switch (BypassPlayerId)
    {
    case 0:
        OutTopology->Topology.Output.NbChanMain = BypassChannelCount;
        OutTopology->Topology.Output.StrideMain = BypassChannelCount;
        break;

    case 1:
        OutTopology->Topology.Output.NbChanAux = BypassChannelCount;
        OutTopology->Topology.Output.StrideAux = BypassChannelCount;
        break;

    case 2:
        OutTopology->Topology.Output.NbChanDig = BypassChannelCount;
        OutTopology->Topology.Output.StrideDig = BypassChannelCount;
        break;

    case 3:
        OutTopology->Topology.Output.NbChanHdmi = BypassChannelCount;
        OutTopology->Topology.Output.StrideHdmi = BypassChannelCount;
        break;

    default:
        SE_DEBUG(group_mixer, "@: Card %d: is not valid\n", BypassPlayerId);
    }

}

////////////////////////////////////////////////////////////////////////////
///
/// Populate a pointer to the mixer's global parameters structure.
///
void Mixer_Mme_c::FillOutTransformerGlobalParameters(MME_LxMixerBDTransformerGlobalParams_Extended_t *GlobalParams)
{
    SE_DEBUG(group_mixer, ">%s\n", Name);
    uint8_t *ptr = reinterpret_cast<uint8_t *>(GlobalParams);
    // Take PolicyValueAudioApplicationIso as default application type.
    int ApplicationType(PolicyValueAudioApplicationIso);
    int RegionType(PolicyValueRegionDVB);
    // Set the global reference for the actual mixer frequency and granule size.
    MixerSamplingFrequency = LookupMixerSamplingFrequency();
    MixerGranuleSize = LookupMixerGranuleSize(MixerSamplingFrequency);
    UpdateMixingMetadata();

    // Get PrimaryClient audio application type for later use
    if (Client[PrimaryClient].ManagedByThread())
    {
        Client[PrimaryClient].GetStream()->GetOption(PolicyAudioApplicationType, &ApplicationType);
        Client[PrimaryClient].GetStream()->GetOption(PolicyRegionType, &RegionType);
        SE_DEBUG(group_mixer, "PrimaryClient: ApplicationType=%s",
                 Mixer_Mme_c::LookupPolicyValueAudioApplicationType(ApplicationType));
    }

    if (MixerSamplingFrequencyHistory != MixerSamplingFrequency)
    {
        MixerSamplingFrequencyHistory = MixerSamplingFrequency;
        MixerSamplingFrequencyUpdated = true;
    }

    memset(GlobalParams, 0, sizeof(*GlobalParams));
    GlobalParams->StructSize = sizeof(GlobalParams->StructSize);
    // Update the pointer corresponding to the StructSize variable of the global param structure
    ptr = &ptr[sizeof(GlobalParams->StructSize)];
    {
        //Fill out ACC_RENDERER_MIXER_ID parameters for mixer inputs
        MME_LxMixerInConfig_t InConfig; // C.FENARD: Should take care about size of this object in stack.
        InConfig.Id         = ACC_RENDERER_MIXER_ID;
        InConfig.StructSize = sizeof(InConfig);
        memset(&InConfig.Config, 0, sizeof(InConfig.Config));

        //Flag to signal that outputs are bypassed, used below for global structure update decision
        bool IsOutputBypassed = false;

        FillOutGlobalPcmInputMixerConfig(&InConfig);
        FillOutGlobalCodedInputMixerConfig(&InConfig, &IsOutputBypassed);
        FillOutGlobalInteractiveInputMixerConfig(&InConfig);
        FillOutGlobalGeneratorInputMixerConfig(&InConfig);

        if (ClientAudioParamsUpdated || MixerSamplingFrequencyUpdated || IsOutputBypassed
            || AudioGeneratorUpdated || IAudioUpdated)
        {
            memcpy(ptr, &InConfig, InConfig.StructSize);
            GlobalParams->StructSize += InConfig.StructSize;
            ptr = &ptr[InConfig.StructSize];
            // Reset the flags here after the global structure is populated.
            // IAudioUpdated flag is set to true due to ongoing
            // work on IAudio code so the structures depending on this are filled always at present.
            // Code will be updated once the work is done and to be covered in separate bug.
            ClientAudioParamsUpdated = false;
            AudioGeneratorUpdated = false;
        }
    }

    if (MixerSettings.MixerOptions.GainUpdated || MixerSettings.MixerOptions.InteractiveGainUpdated  ||
        MixerSettings.MixerOptions.AudioGeneratorGainUpdated)
    {
        // Fill out ACC_RENDERER_MIXER_BD_GAIN_ID paramaters
        MME_LxMixerGainSet_t *InGainConfig = reinterpret_cast<MME_LxMixerGainSet_t *>(ptr);
        InGainConfig->Id = ACC_RENDERER_MIXER_BD_GAIN_ID;
        InGainConfig->StructSize = sizeof(*InGainConfig);
        memset(&InGainConfig->GainSet[0], 0, sizeof(InGainConfig->GainSet));

        FillOutGlobalInputGainConfig(InGainConfig);

        GlobalParams->StructSize += InGainConfig->StructSize;
        ptr = &ptr[InGainConfig->StructSize];
        // Reset the Gain flag here. InteractiveGain and AudioGeneratorGain flags are reset below
        // as other structures are also dependent on this flag.
        MixerSettings.MixerOptions.GainUpdated = false;
    }

    if (MixerSettings.MixerOptions.PanUpdated)
    {
        // Fill out ACC_RENDERER_MIXER_BD_PANNING_ID paramaters
        MME_LxMixerPanningSet_t *InPanningConfig = reinterpret_cast<MME_LxMixerPanningSet_t *>(ptr);
        InPanningConfig->Id = ACC_RENDERER_MIXER_BD_PANNING_ID;
        InPanningConfig->StructSize = sizeof(*InPanningConfig);
        memset(&InPanningConfig->PanningSet[0], 0, sizeof(InPanningConfig->PanningSet));

        FillOutGlobalInputPanningConfig(InPanningConfig);

        GlobalParams->StructSize += InPanningConfig->StructSize;
        ptr = &ptr[InPanningConfig->StructSize];
        //Reset the flag
        MixerSettings.MixerOptions.PanUpdated = false;
    }


    if (MixerSettings.MixerOptions.InteractiveGainUpdated || MixerSettings.MixerOptions.InteractivePanUpdated || IAudioUpdated)
    {
        // Fill out ACC_RENDERER_MIXER_BD_IAUDIO_ID parameters
        MME_LxMixerInIAudioConfig_t *InIaudioConfig = reinterpret_cast<MME_LxMixerInIAudioConfig_t *>(ptr);
        InIaudioConfig->Id = ACC_RENDERER_MIXER_BD_IAUDIO_ID;
        InIaudioConfig->StructSize = sizeof(*InIaudioConfig);
        InIaudioConfig->NbInteractiveAudioInput = MAX_INTERACTIVE_AUDIO;
        memset(&InIaudioConfig->ConfigIAudio[0], 0, sizeof(InIaudioConfig->ConfigIAudio));

        FillOutGlobalInteractiveAudioConfig(InIaudioConfig);

        GlobalParams->StructSize += InIaudioConfig->StructSize;
        ptr = &ptr[InIaudioConfig->StructSize];
        //Reset the flags
        MixerSettings.MixerOptions.InteractivePanUpdated = false;
    }

    // InteractiveGainUpdated is reset here after all the structures
    // dependent on this flag have been filled up
    MixerSettings.MixerOptions.InteractiveGainUpdated = false;
    MixerSettings.MixerOptions.AudioGeneratorGainUpdated = false;

    if (MixerSettings.MixerOptions.PostMixGainUpdated)
    {
        // Fill Out ACC_RENDERER_MIXER_BD_GENERAL_ID parameters
        MME_LxMixerBDGeneral_t &InBDGenConfig = *(reinterpret_cast<MME_LxMixerBDGeneral_t *>(ptr));
        InBDGenConfig.Id = ACC_RENDERER_MIXER_BD_GENERAL_ID;
        InBDGenConfig.StructSize = sizeof(InBDGenConfig);
        InBDGenConfig.PostMixGain = MixerSettings.MixerOptions.PostMixGain;
        InBDGenConfig.GainSmoothEnable = ACC_MME_TRUE;
        InBDGenConfig.OutputLimiter = LIMITER_DISABLED;
        GlobalParams->StructSize += InBDGenConfig.StructSize;
        ptr = &ptr[InBDGenConfig.StructSize];
        //Reset the flags
        MixerSettings.MixerOptions.PostMixGainUpdated = false;
    }

    if (MixerSamplingFrequencyUpdated)
    {
        //Fill out ACC_RENDERER_MIXER_OUTPUT_CONFIG_ID parameters
        MME_LxMixerOutConfig_t &OutConfig = *(reinterpret_cast<MME_LxMixerOutConfig_t *>(ptr));
        OutConfig.Id = ACC_RENDERER_MIXER_OUTPUT_CONFIG_ID;
        OutConfig.StructSize = sizeof(OutConfig);
        OutConfig.NbOutputSamplesPerTransform = MixerGranuleSize;
        OutConfig.MainFS = StmSeTranslateIntegerSamplingFrequencyToDiscrete(MixerSamplingFrequency);
        GlobalParams->StructSize += OutConfig.StructSize;
        ptr = &ptr[OutConfig.StructSize];
    }

    MixerSamplingFrequencyUpdated = false;

    bool UpdateOutputTopologyForPCMBypass = false;
    int  BypassPlarerId = 0;
    int  BypassChannelCount = 0;

    for (uint32_t CountOfAudioPlayerAttached(0), PlayerIdx(0); CountOfAudioPlayerAttached < NumberOfAudioPlayerAttached; PlayerIdx++)
    {
        if (true == MixerPlayer[PlayerIdx].IsPlayerObjectAttached())
        {
            CountOfAudioPlayerAttached++;

            if (true == MixerPlayer[PlayerIdx].HasPcmPlayer())
            {
                PcmPlayer_c::OutputEncoding OutputEncoding = MixerPlayer[PlayerIdx].LookupOutputEncoding(PrimaryCodedDataType[MIXER_CODED_DATA_INPUT_SPDIF_IDX],
                                                                                                         PrimaryCodedDataType[MIXER_CODED_DATA_INPUT_HDMI_IDX]);
                // it is required to specify "NonLinearPCM" channel status bits  when output is bypassed
                // Bypass also possible for SPDIFIN_PCM. Exclude that for channel status update
                if (Client[PrimaryClient].ManagedByThread() && ((OutputEncoding == PcmPlayer_c::BYPASS_SPDIFIN_PCM)  || (OutputEncoding == PcmPlayer_c::BYPASS_SPDIFIN_COMPRESSED)))
                {
                    ParsedAudioParameters_t PrimaryClientAudioParameters;
                    // This client is correctly stated, and its parameters are well known.
                    PrimaryClientAudioParameters = Client[PrimaryClient].GetParameters();
                    if (PrimaryClientAudioParameters.SpdifInProperties.ChannelCount != MixerPlayer[PlayerIdx].GetCardNumberOfChannels())
                    {
                        UpdateOutputTopologyForPCMBypass = true;
                        BypassPlarerId = PlayerIdx;
                        BypassChannelCount = PrimaryClientAudioParameters.SpdifInProperties.ChannelCount;
                        SE_DEBUG(group_mixer, "@: Card %d: %s is bypassed InputChannelCount %d CardChannelCount %d\n", PlayerIdx, MixerPlayer[PlayerIdx].GetCardName(), BypassChannelCount,
                                 MixerPlayer[PlayerIdx].GetCardNumberOfChannels());
                    }

                }
            }
        }
    }

    if ((OutputTopologyUpdated) || (UpdateOutputTopologyForPCMBypass))
    {
        //Fill out ACC_RENDERER_MIXER_OUTPUT_TOPOLOGY_ID parameters
        MME_LxMixerOutTopology_t *OutTopology = reinterpret_cast<MME_LxMixerOutTopology_t *>(ptr);
        OutTopology->Id         = ACC_RENDERER_MIXER_OUTPUT_TOPOLOGY_ID;
        OutTopology->StructSize = sizeof(*OutTopology);
        FillOutGlobalOutputTopology(OutTopology);
        if (UpdateOutputTopologyForPCMBypass)
        {
            UpdateOutputTopology(OutTopology, BypassPlarerId, BypassChannelCount);
        }

        GlobalParams->StructSize += OutTopology->StructSize;
        ptr = &ptr[OutTopology->StructSize];
        OutputTopologyUpdated = false;
    }

    {
        uint32_t HeaderSize;
        // Point the output pcm parameters.
        MME_LxPcmPostProcessingGlobalParameters_Frozen_t *GlobalOutputPcmParams = reinterpret_cast<MME_LxPcmPostProcessingGlobalParameters_Frozen_t *>(ptr);
        // Calculate the header size and pointer to the first parameter structure
        HeaderSize = offsetof(MME_LxPcmPostProcessingGlobalParameters_Frozen_t, AuxSplit) + sizeof(MME_LxPcmPostProcessingGlobalParameters_Frozen_t::AuxSplit);
        SE_DEBUG(group_mixer, "@%s HeaderSize: %d\n", Name, HeaderSize);
        // Populate the header for the pcm parameters to fill according to the outputs in presence.
        GlobalOutputPcmParams->Id = ACC_RENDERER_MIXER_POSTPROCESSING_ID;
        GlobalOutputPcmParams->StructSize = HeaderSize;
        GlobalOutputPcmParams->NbPcmProcessings = 0; // no longer used
        GlobalOutputPcmParams->AuxSplit = SPLIT_AUX; // split automatically
        GlobalParams->StructSize += HeaderSize;
        ptr = &ptr[HeaderSize];
        // Fill in the common chain PCM Parameters
        {
            uint32_t SizeOfParametersInBytes = 0;
            FillCommonChainPcmParameters(ptr, SizeOfParametersInBytes, ApplicationType);
            GlobalOutputPcmParams->StructSize += SizeOfParametersInBytes;
            GlobalParams->StructSize += SizeOfParametersInBytes;
            ptr = &ptr[SizeOfParametersInBytes];
        }
        for (uint32_t CountOfAudioPlayerAttached(0), PlayerIdx(0); CountOfAudioPlayerAttached < NumberOfAudioPlayerAttached; PlayerIdx++)
        {
            if (true == MixerPlayer[PlayerIdx].IsPlayerObjectAttached())
            {
                uint8_t *OriP;
                uint32_t SizeOfParametersInBytes;
                CountOfAudioPlayerAttached++;
                MixerPlayer[PlayerIdx].DebugDump();
                // Compute each output corresponding to the player.
                FillOutDevicePcmParameters(CurrentOutputPcmParams, PlayerIdx, ApplicationType, RegionType);
                OriP = reinterpret_cast<uint8_t *>(&CurrentOutputPcmParams);
                // Point just after header of the given CurrentOutputPcmParams element.
                OriP = &OriP[HeaderSize];
                // Compute corresponding size of data that follow this header of this CurrentOutputPcmParams element.
                SizeOfParametersInBytes = CurrentOutputPcmParams.StructSize - HeaderSize;
                SE_DEBUG(group_mixer, "@%s SizeOfParametersInBytes: %d\n", Name, SizeOfParametersInBytes);
                memcpy(ptr, OriP, SizeOfParametersInBytes);
                GlobalOutputPcmParams->StructSize += SizeOfParametersInBytes;
                GlobalParams->StructSize += SizeOfParametersInBytes;
                ptr = &ptr[SizeOfParametersInBytes];
            }
        }
        SE_DEBUG(group_mixer, "GlobalOutputPcmParams->StructSize=%d\n", GlobalOutputPcmParams->StructSize);
        if (SE_IS_VERBOSE_ON(group_mixer))
        {
            // Dump GlobalOutputPcmParams:
            uint8_t *dumpPtr  = reinterpret_cast<uint8_t *>(GlobalOutputPcmParams);
            struct MME_PcmProcParamsHeader_s *pcmProc;
            uint32_t index = HeaderSize;
            while (index < GlobalOutputPcmParams->StructSize)
            {
                pcmProc = reinterpret_cast<struct MME_PcmProcParamsHeader_s *>(&dumpPtr[index]);
                SE_VERBOSE(group_mixer, "index:%5d PCM PROC Id:%2d [chain:%2d size:%4d] ->%s\n",
                           index, ACC_PCMPROC_ID(pcmProc->Id), ACC_PCMPROC_SUBID(pcmProc->Id),
                           pcmProc->StructSize, pcmProc->Apply ? "Enable" : "Disable");
                index += pcmProc->StructSize;
            }
        }
    }

    SE_DEBUG(group_mixer, "@: InConfig.Config[MIXER_CODED_DATA_INPUT+0] InputId: 0x%08x Config: 0x%08x SamplingFreq: %s\n",
             GlobalParams->InConfig.Config[MIXER_CODED_DATA_INPUT].InputId,
             GlobalParams->InConfig.Config[MIXER_CODED_DATA_INPUT].Config,
             LookupDiscreteSamplingFrequency(GlobalParams->InConfig.Config[MIXER_CODED_DATA_INPUT].SamplingFreq));
    SE_DEBUG(group_mixer, "@: InConfig.Config[MIXER_CODED_DATA_INPUT+1] InputId: 0x%08x Config: 0x%08x SamplingFreq: %s\n",
             GlobalParams->InConfig.Config[MIXER_CODED_DATA_INPUT + 1].InputId,
             GlobalParams->InConfig.Config[MIXER_CODED_DATA_INPUT + 1].Config,
             LookupDiscreteSamplingFrequency(GlobalParams->InConfig.Config[MIXER_CODED_DATA_INPUT + 1].SamplingFreq));
    SE_DEBUG(group_mixer, "@: PcmParams[0].FatPipeOrSpdifOut.Config.Layout: 0x%08x\n",
             GlobalParams->PcmParams[0].FatPipeOrSpdifOut.Config.Layout);
    SE_DEBUG(group_mixer, "@: PcmParams[1].FatPipeOrSpdifOut.Config.Layout: 0x%08x\n",
             GlobalParams->PcmParams[1].FatPipeOrSpdifOut.Config.Layout);
    SE_DEBUG(group_mixer, "@: PcmParams[2].FatPipeOrSpdifOut.Config.Layout: 0x%08x\n",
             GlobalParams->PcmParams[2].FatPipeOrSpdifOut.Config.Layout);
    SE_DEBUG(group_mixer, "@: PcmParams[3].FatPipeOrSpdifOut.Config.Layout: 0x%08x\n",
             GlobalParams->PcmParams[3].FatPipeOrSpdifOut.Config.Layout);
    SE_DEBUG(group_mixer, "<\n");
}

////////////////////////////////////////////////////////////////////////////
///
/// Populate a pointer to the mixer's global parameters structure.
///
void Mixer_Mme_c::FillOutFirstMixerGlobalParameters(MME_LxMixerBDTransformerGlobalParams_Extended_t *GlobalParams)
{
    SE_DEBUG(group_mixer, ">%s\n", Name);
    uint8_t *ptr = reinterpret_cast<uint8_t *>(GlobalParams);

    // Set the global reference for the actual mixer frequency and granule size.
    MixerSamplingFrequency = LookupMixerSamplingFrequency();
    MixerGranuleSize = LookupMixerGranuleSize(MixerSamplingFrequency);
    UpdateMixingMetadata();

    if (FirstMixerSamplingFrequencyHistory != MixerSamplingFrequency)
    {
        FirstMixerSamplingFrequencyHistory = MixerSamplingFrequency;
        FirstMixerSamplingFrequencyUpdated = true;
    }

    memset(GlobalParams, 0, sizeof(*GlobalParams));
    GlobalParams->StructSize = sizeof(GlobalParams->StructSize);
    // Update the pointer corresponding to the StructSize variable of the global param structure
    ptr = &ptr[sizeof(GlobalParams->StructSize)];
    // Fill out parameters for inputs of mixer.
    {
        MME_LxMixerInConfig_t InConfig; // C.FENARD: Should take care about size of this object in stack.
        InConfig.Id         = ACC_RENDERER_MIXER_ID;
        InConfig.StructSize = sizeof(InConfig);
        memset(&InConfig.Config, 0, sizeof(InConfig.Config));

        FillOutGlobalPcmInputMixerConfig(&InConfig);

        //Check if Inconfig Update is required if either of the flags are set
        if (FirstMixerClientAudioParamsUpdated || FirstMixerSamplingFrequencyUpdated)
        {
            memcpy(ptr, &InConfig, InConfig.StructSize);
            GlobalParams->StructSize += InConfig.StructSize;
            ptr = &ptr[InConfig.StructSize];
            // Reset the flags here after the global structure is populated.
            FirstMixerClientAudioParamsUpdated = false;
        }
    }

    if (MixerSettings.MixerOptions.GainUpdated || MixerSettings.MixerOptions.InteractiveGainUpdated  ||
        MixerSettings.MixerOptions.AudioGeneratorGainUpdated)
    {
        MME_LxMixerGainSet_t &InGainConfig = *(reinterpret_cast<MME_LxMixerGainSet_t *>(ptr));
        InGainConfig.Id = ACC_RENDERER_MIXER_BD_GAIN_ID;
        InGainConfig.StructSize = sizeof(InGainConfig);
        memset(&InGainConfig.GainSet[0], 0, sizeof(InGainConfig.GainSet));

        for (uint32_t ClientIdx(0); ClientIdx < ACC_MIXER_MAX_NB_INPUT; ClientIdx++)
        {
            InGainConfig.GainSet[ClientIdx].InputId =   ClientIdx;

            for (uint32_t ChannelIdx(0); ChannelIdx < SND_PSEUDO_MIXER_CHANNELS; ChannelIdx++)
            {
                if (ClientIdx < MIXER_MAX_CLIENTS)
                {
                    InGainConfig.GainSet[ClientIdx].AlphaPerChannel[ChannelIdx] =
                        MixerSettings.MixerOptions.Gain[ClientIdx][ChannelIdx];
                }
                else
                {
                    InGainConfig.GainSet[ClientIdx].AlphaPerChannel[ChannelIdx] =    Q3_13_UNITY;
                }
            }
        }

        GlobalParams->StructSize += InGainConfig.StructSize;
        ptr = &ptr[InGainConfig.StructSize];
        // Reset the Gain flag here. The InteractiveGain flag is reset below as other structures
        // are also dependent on this flag.
        MixerSettings.MixerOptions.GainUpdated = false;
    }

    if (MixerSettings.MixerOptions.PanUpdated)
    {
        // Fill out ACC_RENDERER_MIXER_BD_PANNING_ID paramaters
        MME_LxMixerPanningSet_t *InPanningConfig = reinterpret_cast<MME_LxMixerPanningSet_t *>(ptr);
        InPanningConfig->Id = ACC_RENDERER_MIXER_BD_PANNING_ID;
        InPanningConfig->StructSize = sizeof(*InPanningConfig);
        memset(&InPanningConfig->PanningSet[0], 0, sizeof(InPanningConfig->PanningSet));

        FillOutGlobalInputPanningConfig(InPanningConfig);

        GlobalParams->StructSize += InPanningConfig->StructSize;
        ptr = &ptr[InPanningConfig->StructSize];
        //Reset the flag
        MixerSettings.MixerOptions.PanUpdated = false;
    }

    if (MixerSettings.MixerOptions.PostMixGainUpdated)
    {
        MME_LxMixerBDGeneral_t &InBDGenConfig = *(reinterpret_cast<MME_LxMixerBDGeneral_t *>(ptr));
        InBDGenConfig.Id = ACC_RENDERER_MIXER_BD_GENERAL_ID;
        InBDGenConfig.StructSize = sizeof(InBDGenConfig);
        InBDGenConfig.PostMixGain = MixerSettings.MixerOptions.PostMixGain;
        InBDGenConfig.GainSmoothEnable = ACC_MME_TRUE;
        InBDGenConfig.OutputLimiter = LIMITER_DISABLED;
        GlobalParams->StructSize += InBDGenConfig.StructSize;
        ptr = &ptr[InBDGenConfig.StructSize];
        //Reset the flags
        MixerSettings.MixerOptions.PostMixGainUpdated = false;
    }

    if (FirstMixerSamplingFrequencyUpdated)
    {
        MME_LxMixerOutConfig_t &OutConfig = *(reinterpret_cast<MME_LxMixerOutConfig_t *>(ptr));
        OutConfig.Id = ACC_RENDERER_MIXER_OUTPUT_CONFIG_ID;
        OutConfig.StructSize = sizeof(OutConfig);
        OutConfig.NbOutputSamplesPerTransform = MixerGranuleSize;
        OutConfig.MainFS = StmSeTranslateIntegerSamplingFrequencyToDiscrete(MixerSamplingFrequency);
        GlobalParams->StructSize += OutConfig.StructSize;
        ptr = &ptr[OutConfig.StructSize];
    }

    FirstMixerSamplingFrequencyUpdated = false;

    MME_LxMixerOutTopology_t &OutTopology = *(reinterpret_cast<MME_LxMixerOutTopology_t *>(ptr));
    OutTopology.Id         = ACC_RENDERER_MIXER_OUTPUT_TOPOLOGY_ID;
    OutTopology.StructSize = sizeof(OutTopology);

    if (FirstActivePlayer != MIXER_MME_NO_ACTIVE_PLAYER)
    {
        OutTopology.Topology.Output.NbChanMain = 8;
        OutTopology.Topology.Output.StrideMain = 8;
    }
    else
    {
        OutTopology.Topology.NbChannels = 0;
    }

    GlobalParams->StructSize += OutTopology.StructSize;
    ptr = &ptr[OutTopology.StructSize];

    SE_DEBUG(group_mixer, "<\n");
}

////////////////////////////////////////////////////////////////////////////
///
/// Determine the output sampling frequency to use for the specified device.
///
/// Basically we consider mixer output frequency and alter it (strictly
/// powers of 2) until the frequency lies within a suitable range for
/// the output topology.
///
/// \todo Strictly speaking there is a cyclic dependency between
///       Mixer_Mme_c::LookupOutputSamplingFrequency() and
///       Mixer_Mme_c::LookupOutputEncoding(). If the output has to
///       fallback to LPCM there is no need to clamp the frequency
///       below 48khz.
///
uint32_t Mixer_Mme_c::LookupPcmPlayerOutputSamplingFrequency(PcmPlayer_c::OutputEncoding OutputEncoding,
                                                             uint32_t MixerPlayerIndex) const
{
    SE_DEBUG(group_mixer, ">\n");
    SE_ASSERT(NominalOutputSamplingFrequency);
    uint32_t Iec60958FrameRate = LookupIec60958FrameRate(OutputEncoding);
    uint32_t Freq = MixerPlayer[MixerPlayerIndex].LookupOutputSamplingFrequency(OutputEncoding,
                                                                                NominalOutputSamplingFrequency,
                                                                                Iec60958FrameRate);
    SE_DEBUG(group_mixer, "<%s %d\n", Name, Freq);
    return Freq;
}

////////////////////////////////////////////////////////////////////////////
///
/// Configure the downmix processing appropriately.
///
/// Contract: The PcmParams structure is zeroed before population. At the time
/// it is handed to this method the CMC settings have already been stored and
/// the downmix settings are untouched (still zero).
///
/// This method is, semantically, part of FillOutDevicePcmParameters() (hence
/// it is inlined) but is quite complex enough to justify splitting it out into
/// a separate method.
///
inline void Mixer_Mme_c::FillOutDeviceDownmixParameters(MME_LxPcmPostProcessingGlobalParameters_Frozen_t &PcmParams,
                                                        uint32_t MixerPlayerIndex,
                                                        bool EnableDMix,
                                                        int RegionType)
{
    uint32_t min_index = 0, max_index = 0, mid_index = 0;
    uint64_t TargetSortValue = 0, CurrentSortValue = 0;
    bool TraceDownmixLookups;
    bool CodecDownmixTable = false;
    int DmixTableNo;
    ParsedAudioParameters_t PrimaryClientAudioParameters;
    // put these are the top because their values are very transient - don't expect them to maintain their
    // values between blocks...
    union
    {
        struct stm_se_audio_channel_assignment ca;
        uint32_t n;
    } TargetOutputId, TargetInputId, OutputId, InputId;
    // Initialize the donwmixer structure before population
    // PcmParams already initialized to null.
    MME_DMixGlobalParams_t &DMix   = PcmParams.Dmix;
    DMix.Id                 = PCMPROCESS_SET_ID(ACC_PCM_DMIX_ID, MixerPlayerIndex);
    DMix.Apply              = (EnableDMix ? ACC_MME_ENABLED : ACC_MME_DISABLED);
    DMix.StructSize             = sizeof(DMix);
    DMix.Config[DMIX_USER_DEFINED]  = ACC_MME_FALSE;
    DMix.Config[DMIX_STEREO_UPMIX]  = ACC_MME_FALSE;
    DMix.Config[DMIX_MONO_UPMIX]    = ACC_MME_FALSE;
    DMix.Config[DMIX_MEAN_SURROUND]     = ACC_MME_FALSE;
    DMix.Config[DMIX_SECOND_STEREO]     = ACC_MME_FALSE;
    DMix.Config[DMIX_MIX_LFE]       = ACC_MME_FALSE;
    if (RegionType == PolicyValueRegionARIB)
    {
        DMix.Config[DMIX_NORMALIZE]     = DMIX_HEADROOM;
    }
    else
    {
        DMix.Config[DMIX_NORMALIZE]     = DMIX_NORMALIZE_ON;
    }
    DMix.Config[DMIX_NORM_IDX]      = 0;
    DMix.Config[DMIX_DIALOG_ENHANCE]    = ACC_MME_FALSE;
    PcmParams.StructSize += sizeof(DMix);

    // if the downmixer isn't enabled we're done

    if (!EnableDMix)
    {
        return;
    }

    // get output and input audio modes
    enum eAccAcMode OutputMode  = (enum eAccAcMode) PcmParams.CMC.Config[CMC_OUTMODE_MAIN];
    enum eAccAcMode InputMode   = LookupMixerAudioMode();
    SE_DEBUG(group_mixer, "[Player %d] Input mode (or mixer mode)=%s  OutputMode=%s\n", MixerPlayerIndex, LookupAudioMode(InputMode), LookupAudioMode(OutputMode));
    SE_ASSERT(ACC_MODE_ID == PcmParams.CMC.Config[CMC_OUTMODE_AUX]); // no auxiliary output at the moment

    // Client Lock already taken by caller.
    if (Client[PrimaryClient].ManagedByThread())
    {
        // This client is correctly stated, and its parameters are well known.
        Client[PrimaryClient].GetParameters(&PrimaryClientAudioParameters);

        /*Check if we have some downmix table return from the codec
          and its outmode/input mode matches with the user out mode / mixer inmode then use that table for downmixing*/
        for (DmixTableNo = 0; DmixTableNo < MAX_SUPPORTED_DMIX_TABLE; DmixTableNo++)
        {
            if ((PrimaryClientAudioParameters.DownMixTable[DmixTableNo].IsTablePresent)
                && (PrimaryClientAudioParameters.DownMixTable[DmixTableNo].OutMode == OutputMode)
                && (PrimaryClientAudioParameters.DownMixTable[DmixTableNo].InMode == InputMode)) // Downmix Table is present and also input and output mode is matching use this table for downmixing
            {
                CodecDownmixTable = true;
                break;
            }
        }
    }

    if (CodecDownmixTable)
    {
        int nch_out = PrimaryClientAudioParameters.DownMixTable[DmixTableNo].NChOut;
        int nch_in  = PrimaryClientAudioParameters.DownMixTable[DmixTableNo].NChIn;
        int i, j;
        DMix.Config[DMIX_USER_DEFINED] = ACC_MME_TRUE;

        for (i = 0; i < nch_out; i++)
        {
            for (j = 0; j < nch_in; j++)
            {
                DMix.MainMixTable[i][j] = PrimaryClientAudioParameters.DownMixTable[DmixTableNo].DownMixTableCoeff[i][j];
            }
        }

        return; // We have configure the downmixing
    }

    /* STEREO INPUT AND ALL SPEAKER ENABLED */

    if ((InputMode == ACC_MODE20) && (MixerSettings.OutputConfiguration.all_speaker_stereo_enable))
    {
        DMix.Config[DMIX_USER_DEFINED] = ACC_MME_TRUE;
        DMix.MainMixTable[0][0] = ACC_UNITY;
        DMix.MainMixTable[1][1] = ACC_UNITY;
        DMix.MainMixTable[2][0] = ACC_UNITY / 2;
        DMix.MainMixTable[2][1] = ACC_UNITY / 2;
        DMix.MainMixTable[4][0] = ACC_UNITY;
        DMix.MainMixTable[5][1] = ACC_UNITY;

        // one single rear central channel

        if ((OutputMode == ACC_MODE21) || (OutputMode == ACC_MODE31) ||
            (OutputMode == ACC_MODE21_LFE) || (OutputMode == ACC_MODE31_LFE))
        {
            DMix.MainMixTable[4][0] = ACC_UNITY / 2;
            DMix.MainMixTable[4][1] = ACC_UNITY / 2;
        }

        SE_DEBUG(group_mixer, "Used custom UpMix table for %s to %s\n",
                 LookupAudioMode(InputMode), LookupAudioMode(OutputMode));

        for (uint32_t i = 0; i < DMIX_NB_IN_CHANNELS; i++)
            SE_DEBUG(group_mixer, "%04x %04x %04x %04x %04x %04x %04x %04x\n",
                     DMix.MainMixTable[i][0], DMix.MainMixTable[i][1],
                     DMix.MainMixTable[i][2], DMix.MainMixTable[i][3],
                     DMix.MainMixTable[i][4], DMix.MainMixTable[i][5],
                     DMix.MainMixTable[i][6], DMix.MainMixTable[i][7]);

        return;
    }

    /* OTHER CASES */

    // if we don't need to honor the downmix 'firmware' then we're done

    if (!DownmixFirmware)
    {
        return;
    }

    // find proper modes = consider speaker configuration
    TargetOutputId.ca = TranslateAudioModeToChannelAssignment(OutputMode);
    TargetInputId.ca = TranslateAudioModeToChannelAssignment(InputMode);
    TargetSortValue = ((uint64_t) TargetInputId.n << 32) + TargetOutputId.n;
    // update some values to use later
    TraceDownmixLookups = (OutputMode != MixerPlayer[MixerPlayerIndex].GetPcmPlayerConfig().LastOutputMode) || (InputMode != MixerPlayer[MixerPlayerIndex].GetPcmPlayerConfig().LastOutputMode);
    MixerPlayer[MixerPlayerIndex].GetPcmPlayerConfig().LastOutputMode = OutputMode;
    MixerPlayer[MixerPlayerIndex].GetPcmPlayerConfig().LastInputMode = InputMode;
    // find the index in the downmixer firmware table corresponding to the TargetSortValue
    // just found. In order to do that, the following comparison loop is necessary
    max_index = DownmixFirmware->header.num_index_entries;

    while (min_index < max_index)
    {
        // Note: (max_index + min_index) / 2 is not overflow-safe
        mid_index = min_index + ((max_index - min_index) / 2);
        InputId.ca       = DownmixFirmware->index[mid_index].input_id;
        OutputId.ca      = DownmixFirmware->index[mid_index].output_id;
        CurrentSortValue = ((uint64_t) InputId.n << 32) + OutputId.n;

        if (CurrentSortValue < TargetSortValue)
        {
            min_index = mid_index + 1;
        }
        else
        {
            max_index = mid_index;
        }
    }

    SE_ASSERT(min_index == max_index);
    // regenerate the sort value; it is state if CurrentSortValue < TargetSortValue during final iteration
    InputId.ca       = DownmixFirmware->index[min_index].input_id;
    OutputId.ca      = DownmixFirmware->index[min_index].output_id;
    CurrentSortValue = ((uint64_t) InputId.n << 32) + OutputId.n;

    // malleable surface handling (a malleable output surface can dynamically change its
    // channel topology depending on the data being presented).
    //
    // For a malleable surface the low order 32-bits of the search word are 0x80000000. This
    // is specified to matched by any entry in the firmware that has the top bit set (i.e.
    // the lowest 31 bits are ignored). The binary search above has been carefully tuned to
    // select the right value even with an inexact match (the tuning is mostly making the
    // malleable bit the most significant bit).
    //
    // Having got this far we need to detect if we are targeting malleable surface and update
    // the CMC config.
    if (TargetOutputId.ca.malleable && OutputId.ca.malleable)
    {
        OutputMode = TranslateChannelAssignmentToAudioMode(OutputId.ca);
        SE_INFO(group_mixer, "Downmix firmware requested %s as malleable output surface\n",
                LookupAudioMode(OutputMode));
        // update the CMC configuration with the value found in the firmware
        PcmParams.CMC.Config[CMC_OUTMODE_MAIN] = OutputMode;
        // update the target value since now we know what we were looking for
        TargetSortValue = CurrentSortValue;
    }

    // fill the rest of the dowmixer table with the proper coefficients

    if (CurrentSortValue == TargetSortValue)
    {
        struct snd_pseudo_mixer_downmix_index *index = DownmixFirmware->index + min_index;
        snd_pseudo_mixer_downmix_Q15 *data = (snd_pseudo_mixer_downmix_Q15 *)(DownmixFirmware->index +
                                                                              DownmixFirmware->header.num_index_entries);
        snd_pseudo_mixer_downmix_Q15 *table = data + index->offset;
        DMix.Config[DMIX_USER_DEFINED] = ACC_MME_TRUE;

        for (int x = 0; x < index->output_dimension; x++)
            for (int y = 0; y < index->input_dimension; y++)
            {
                DMix.MainMixTable[x][y] = table[x * index->input_dimension + y];
            }

        SE_DEBUG(group_mixer, "Found custom downmix table for %s to %s\n",
                 LookupAudioMode(InputMode), LookupAudioMode(OutputMode));

        for (uint32_t i = 0; i < DMIX_NB_IN_CHANNELS; i++)
            SE_DEBUG(group_mixer, "%04x %04x %04x %04x %04x %04x %04x %04x\n",
                     DMix.MainMixTable[i][0], DMix.MainMixTable[i][1],
                     DMix.MainMixTable[i][2], DMix.MainMixTable[i][3],
                     DMix.MainMixTable[i][4], DMix.MainMixTable[i][5],
                     DMix.MainMixTable[i][6], DMix.MainMixTable[i][7]);
    }
    else
    {
        // not an error but certainly worthy of note
        if (TraceDownmixLookups)
            SE_INFO(group_mixer, "Downmix firmware has no entry for %s to %s - using defaults\n",
                    LookupAudioMode(InputMode), LookupAudioMode(OutputMode));
    }
}



////////////////////////////////////////////////////////////////////////////
///
/// Configure the SpdifOut processing appropriately.
///
/// Contract: The PcmParams structure is zeroed before population. At the time
/// it is handed to this method it may already have been partially filled in.
///
/// This method is, semantically, part of FillOutDevicePcmParameters() (hence
/// it is inlined) but is quite complex enough to justify splitting it out into
/// a separate method.
///
/// The primary responsibility of the function is to set dynamically changing
/// fields (such as the sample rate), static fields are provided by userspace.
/// The general strategy taken to merge the two sources is that is that where
/// fields are supplied by the implementation (e.g. sample rate,
/// LPCM/other, emphasis) we ignore the values provided by userspace. Otherwise
/// we make every effort to reflect the userspace values, we make no effort
/// to validate their correctness.
///
inline void Mixer_Mme_c::FillOutDeviceSpdifParameters(MME_LxPcmPostProcessingGlobalParameters_Frozen_t &PcmParams,
                                                      uint32_t MixerPlayerIndex,
                                                      PcmPlayer_c::OutputEncoding OutputEncoding)
{
    SE_DEBUG(group_mixer, "> Card %d: %s is %s\n",
             MixerPlayerIndex,
             MixerPlayer[MixerPlayerIndex].GetCardName(),
             PcmPlayer_c::LookupOutputEncoding(OutputEncoding));

    AudioOriginalEncoding_t OriginalEncoding(AudioOriginalEncodingUnknown);
    // Client Lock already taken by caller.
    if (Client[MasterClientIndex].ManagedByThread())
    {
        // This client is correctly stated, and its parameters are well known.
        OriginalEncoding = Client[MasterClientIndex].GetParameters().OriginalEncoding;
    }

    if (OutputEncoding == PcmPlayer_c::OUTPUT_FATPIPE)
    {
        MMESetFatpipeConfig(PcmParams, MixerPlayerIndex, OutputEncoding, OriginalEncoding);
        return;
    }

    MMESetSpdifConfig(PcmParams, MixerPlayerIndex, OutputEncoding, OriginalEncoding);

    SpdifInProperties_t *SpdifInPropPtr = NULL;
    ParsedAudioParameters_t PrimaryClientAudioParameters;
    // Client Lock already taken by caller.
    if (Client[PrimaryClient].ManagedByThread())
    {
        // This client is correctly started, and its parameters are well known.
        PrimaryClientAudioParameters = Client[PrimaryClient].GetParameters();
        SpdifInPropPtr = &PrimaryClientAudioParameters.SpdifInProperties;
    }
    if (PlayerNoError != MixerPlayer[MixerPlayerIndex].CheckAndConfigureHDMICell(OutputEncoding, SpdifInPropPtr))
    {
        SE_ERROR("Error when configuring the hdmi cell\n");
    }
}

inline void Mixer_Mme_c::MMESetFatpipeConfig(MME_LxPcmPostProcessingGlobalParameters_Frozen_t &PcmParams,
                                             uint32_t MixerPlayerIndex,
                                             PcmPlayer_c::OutputEncoding OutputEncoding,
                                             AudioOriginalEncoding_t OriginalEncoding)
{
    //
    // Special case FatPipe which uses a different parameter structure and handles channel status
    // differently.
    //
    // TODO: check if these default values are correct.
    StreamMetadata_t MetaData =
    {
        0, // FrontMatrixEncoded
        0, // RearMatrixEncoded
        0, // MixLevel
        0, // DialogNorm
        0  // LfeGain
    };

    // Client Lock already taken by caller.
    if (Client[PrimaryClient].ManagedByThread())
    {
        // This client is correctly stated, and its parameters are well known.
        MetaData = Client[PrimaryClient].GetParameters().StreamMetadata;
    }

    MME_FatpipeGlobalParams_t &FatPipe = PcmParams.FatPipeOrSpdifOut;
    FatPipe.Id = PCMPROCESS_SET_ID(ACC_PCM_SPDIFOUT_ID, MixerPlayerIndex);
    FatPipe.StructSize = sizeof(FatPipe);
    PcmParams.StructSize += PcmParams.FatPipeOrSpdifOut.StructSize;
    FatPipe.Apply = ACC_MME_ENABLED;
    FatPipe.Config.Mode = 1; // FatPipe mode
    FatPipe.Config.UpdateSpdifControl = 1;
    FatPipe.Config.UpdateMetaData = 1;
    FatPipe.Config.Upsample4x = 1;
    FatPipe.Config.ForceChannelMap = 0;
    // metadata gotten from the userspace
    FatPipe.Config.FEC = ((MixerSettings.OutputConfiguration.fatpipe_metadata.md[0] >> 6) & 1);
    FatPipe.Config.Encryption = ((MixerSettings.OutputConfiguration.fatpipe_metadata.md[0] >> 5) & 1);
    FatPipe.Config.InvalidStream = ((MixerSettings.OutputConfiguration.fatpipe_metadata.md[0] >> 4) & 1);
    FatPipe.MetaData.LevelShiftValue = MixerSettings.OutputConfiguration.fatpipe_metadata.md[6] & 31;
    FatPipe.MetaData.Effects = MixerSettings.OutputConfiguration.fatpipe_metadata.md[14] & 0xffff;

    // metadata gotten from the primary stream itself (AC3/DTS)
    FatPipe.MetaData.Mixlevel = MetaData.MixLevel;
    FatPipe.MetaData.DialogHeadroom = MetaData.DialogNorm;
    FatPipe.MetaData.FrontMatrixEncoded = MetaData.FrontMatrixEncoded;
    FatPipe.MetaData.RearMatrixEncoded = MetaData.RearMatrixEncoded;
    FatPipe.MetaData.Lfe_Gain = MetaData.LfeGain;
    memset(FatPipe.Spdifout.Validity, 0xff, sizeof(FatPipe.Spdifout.Validity));
    // FatPipe is entirely prescriptitive w.r.t the channel status bits so we can ignore the userspace
    // provided values entirely
    /* consumer, not linear PCM, copyright is asserted, mode 00, pcm encoder,
     * no generation indication, channel 0, source 0, level II clock accuracy,
     * ?44.1KHz?, 24-bit
     */
    FatPipe.Spdifout.ChannelStatus[0] = 0x02020000;
    FatPipe.Spdifout.ChannelStatus[1] = 0x0B000000; // report 24bits word length
}

inline void Mixer_Mme_c::MMESetSpdifConfig(MME_LxPcmPostProcessingGlobalParameters_Frozen_t &PcmParams,
                                           uint32_t MixerPlayerIndex,
                                           PcmPlayer_c::OutputEncoding OutputEncoding,
                                           AudioOriginalEncoding_t OriginalEncoding)
{
    //
    // Handle, in a unified manner, all the IEC SPDIF formatings
    //
    MME_SpdifOutGlobalParams_t &SpdifOut = *(reinterpret_cast<MME_SpdifOutGlobalParams_t *>(&PcmParams.FatPipeOrSpdifOut));
    SpdifOut.Id = PCMPROCESS_SET_ID(ACC_PCM_SPDIFOUT_ID, MixerPlayerIndex);
    SpdifOut.StructSize = sizeof(MME_FatpipeGlobalParams_t);   // use fatpipe size (to match frozen structure)
    PcmParams.StructSize += sizeof(MME_FatpipeGlobalParams_t);   // use fatpipe size
    int layout = 0;

    // Set Layout
    if ((MixerPlayer[MixerPlayerIndex].GetCardNumberOfChannels() > 2) &&
        (MixerPlayer[MixerPlayerIndex].IsConnectedToHdmi()) &&
        (PcmParams.CMC.Config[CMC_OUTMODE_MAIN] > ACC_MODE20))
    {
        layout = 1;
    }

    SpdifOut.Config.Layout = 0;

    if ((OutputEncoding == PcmPlayer_c::OUTPUT_IEC60958) || (OutputEncoding == PcmPlayer_c::BYPASS_DTS_CDDA))
    {
        SpdifOut.Apply = ACC_MME_ENABLED;
        SpdifOut.Config.Mode = 0; // SPDIF mode
        SpdifOut.Config.UpdateSpdifControl = 1;
        SpdifOut.Config.SpdifCompressed = 0;
        SpdifOut.Config.AddIECPreamble = 0;
        SpdifOut.Config.ForcePC = 0; // useless in this case: for compressed mode only
        SpdifOut.Spdifout.ChannelStatus[0] = 0x00000000; /* consumer, linear PCM, mode 00 */

        if (OutputEncoding == PcmPlayer_c::OUTPUT_IEC60958)
        {
            SpdifOut.Spdifout.ChannelStatus[1] = 0x0b000000;    /* 24-bit word length */
        }
        else
        {
            SpdifOut.Spdifout.ChannelStatus[1] = 0x00000000;    /* word length not indicated */
        }

        if (OutputEncoding == PcmPlayer_c::OUTPUT_IEC60958)
        {
            SpdifOut.Config.Layout = layout;
        }
    }
    else if ((OutputEncoding == PcmPlayer_c::OUTPUT_AC3) ||
             (OutputEncoding == PcmPlayer_c::BYPASS_AC3) ||
             (OutputEncoding == PcmPlayer_c::BYPASS_DDPLUS) ||
             (OutputEncoding == PcmPlayer_c::BYPASS_AAC))
    {
        SpdifOut.Apply = ACC_MME_ENABLED;
        SpdifOut.Config.Mode = 0; // SPDIF mode
        SpdifOut.Config.UpdateSpdifControl = 1;
        SpdifOut.Config.UpdateMetaData = 1; // this is supposed to be FatPipe only but the examples set it...
        SpdifOut.Config.SpdifCompressed = 1;
        SpdifOut.Config.AddIECPreamble = (OutputEncoding == PcmPlayer_c::OUTPUT_AC3) ? 0 : 1;
        SpdifOut.Config.ForcePC = 1; // compressed mode: firmware will get the stream type from the to Preamble_PC below

        if ((OutputEncoding == PcmPlayer_c::OUTPUT_AC3) ||
            ((OutputEncoding == PcmPlayer_c::BYPASS_AC3) &&
             ((OriginalEncoding == AudioOriginalEncodingDdplus) || (OriginalEncoding == AudioOriginalEncodingDPulse)))) /* For HE-AAC  we do transcoding in little endian AC3 */
        {
            SpdifOut.Config.Endianness = 1; // little endian input frames
            SE_DEBUG(group_mixer, "@ %s Endianness changed for SpdifOut\n", PcmPlayer_c::LookupOutputEncoding(OutputEncoding));
        }

        memset(SpdifOut.Spdifout.Validity, 0xff, sizeof(SpdifOut.Spdifout.Validity));
        SpdifOut.Spdifout.ChannelStatus[0] = 0x02000000; /* consumer, not linear PCM, mode 00 */
        SpdifOut.Spdifout.ChannelStatus[1] = 0x02000000; /* 16-bit word length */
        SpdifOut.Spdifout.Preamble_PA = 0xf872;
        SpdifOut.Spdifout.Preamble_PB = 0x4e1f;
        SpdifOut.Spdifout.Preamble_PC = LookupSpdifPreamblePc(OutputEncoding);
    }
    else if ((OutputEncoding == PcmPlayer_c::OUTPUT_DTS) ||
             (OutputEncoding == PcmPlayer_c::BYPASS_DTS_512) ||
             (OutputEncoding == PcmPlayer_c::BYPASS_DTS_1024) ||
             (OutputEncoding == PcmPlayer_c::BYPASS_DTS_2048) ||
             ((OutputEncoding >= PcmPlayer_c::BYPASS_DTSHD_LBR) && (OutputEncoding <= PcmPlayer_c::BYPASS_DTSHD_DTS_8192)) ||
             (OutputEncoding == PcmPlayer_c::BYPASS_DTSHD_MA))
    {
        SpdifOut.Apply = ACC_MME_ENABLED;
        SpdifOut.Config.Mode = 0; // SPDIF mode
        SpdifOut.Config.UpdateSpdifControl = 1;
        SpdifOut.Config.UpdateMetaData = 1; // this is supposed to be FatPipe only but the examples set it...
        // in case of DTS encoding, the preambles are set by the encoder itself
        SpdifOut.Config.SpdifCompressed = 1;
        SpdifOut.Config.AddIECPreamble = (OutputEncoding == PcmPlayer_c::OUTPUT_DTS) ? 0 : 1;
        SpdifOut.Config.ForcePC = 1; // compressed mode: firmware will get the stream type from the to Preamble_PC below

        if (OutputEncoding == PcmPlayer_c::OUTPUT_DTS)
        {
            SpdifOut.Config.Endianness = 1; // little endian input frames
        }

        memset(SpdifOut.Spdifout.Validity, 0xff, sizeof(SpdifOut.Spdifout.Validity));
        SpdifOut.Spdifout.ChannelStatus[0] = 0x02000000; /* consumer, not linear PCM, mode 00 */
        SpdifOut.Spdifout.ChannelStatus[1] = 0x02000000; /* 16-bit word length */
        SpdifOut.Spdifout.Preamble_PA = 0xf872;
        SpdifOut.Spdifout.Preamble_PB = 0x4e1f;
        SpdifOut.Spdifout.Preamble_PC = LookupSpdifPreamblePc(OutputEncoding);
    }
    else if (OutputEncoding == PcmPlayer_c::BYPASS_TRUEHD)
    {
        /// TODO not supported yet by the firmware...
        SE_INFO(group_mixer, "TrueHD bypassing is not supported yet by the firmware\n");
        SpdifOut.Apply = ACC_MME_ENABLED;
        SpdifOut.Config.Mode = 0; // SPDIF mode
        SpdifOut.Config.UpdateSpdifControl = 1;
        SpdifOut.Config.UpdateMetaData = 1; // this is supposed to be FatPipe only but the examples set it...
        SpdifOut.Config.SpdifCompressed = 1;
        SpdifOut.Config.AddIECPreamble = 1;
        SpdifOut.Config.ForcePC = 1; // compressed mode: firmware will get the stream type from the to Preamble_PC below
        //
        SpdifOut.Config.Endianness = 1; // big endian input frames: check with the MAT engine
        memset(SpdifOut.Spdifout.Validity, 0xff, sizeof(SpdifOut.Spdifout.Validity));
        SpdifOut.Spdifout.ChannelStatus[0] = 0x02000000; /* consumer, not linear PCM, mode 00 */
        SpdifOut.Spdifout.ChannelStatus[1] = 0x02000000; /* 16-bit word length */
        SpdifOut.Spdifout.Preamble_PA = 0xf872;
        SpdifOut.Spdifout.Preamble_PB = 0x4e1f;
        SpdifOut.Spdifout.Preamble_PC = LookupSpdifPreamblePc(OutputEncoding);
    }
    else if (OutputEncoding == PcmPlayer_c::BYPASS_SPDIFIN_COMPRESSED)
    {
        SE_INFO(group_mixer, "Setting bypass for BYPASS_SPDIFIN_COMPRESSED\n");
        SpdifOut.Apply = ACC_MME_ENABLED;
        SpdifOut.Config.Mode = 0; // SPDIF mode
        SpdifOut.Config.UpdateSpdifControl = 0;
        SpdifOut.Config.SpdifCompressed = 1;
        SpdifOut.Config.AddIECPreamble = 0;
        SpdifOut.Config.ForcePC = 0;
        SpdifOut.Spdifout.ChannelStatus[0] = 0x02000000; /* consumer, linear PCM, mode 00 */
        SpdifOut.Spdifout.ChannelStatus[1] = 0x02000000; /* 16-bit word length */
        SpdifOut.Config.Endianness = 1; // little endian input frames
    }
    else if (OutputEncoding == PcmPlayer_c::BYPASS_SPDIFIN_PCM)
    {
        SE_INFO(group_mixer, "Setting bypass for BYPASS_SPDIFIN_PCM\n");
        SpdifOut.Apply = ACC_MME_ENABLED;
        SpdifOut.Config.Mode = 0; // SPDIF mode
        SpdifOut.Config.UpdateSpdifControl = 0;
        SpdifOut.Config.SpdifCompressed = 0;
        SpdifOut.Config.AddIECPreamble = 0;
        SpdifOut.Config.ForcePC = 0; // useless in this case: for compressed mode only
        SpdifOut.Spdifout.ChannelStatus[0] = 0x00000000; /* consumer, linear PCM, mode 00 */
        SpdifOut.Spdifout.ChannelStatus[1] = 0x00000000;    /* word length not indicated */
        SpdifOut.Config.Endianness = 1; // little endian input frames
        // No need to set layout. FW will takecare of the layout in SPDIFin bypass mode
    }
    else
    {
        SpdifOut.Apply = ACC_MME_DISABLED;
        SE_DEBUG(group_mixer, "@: SpdifOut.Apply->ACC_MME_DISABLED\n");
    }

    // A convenience macro to allow the entries below to be copied from the IEC
    // standards document. The sub-macro chops of the high bits (the ones that
    // select which word within the channel mask we need). The full macro then
    // performs an endian swap since the firmware expects big endian values.
#define B(x) (((_B(x) & 0xff) << 24) | ((_B(x) & 0xff00) << 8) | ((_B(x) >> 8)  & 0xff00) | ((_B(x) >> 24) & 0xff))
#define _B(x) (1 << ((x) % 32))
    const uint32_t use_of_channel_status_block = B(0);
    const uint32_t linear_pcm_identification = B(1);
    //const uint32_t copyright_information = B(2);
    const uint32_t additional_format_information = B(3) | B(4) | B(5);
    const uint32_t mode = B(6) | B(7);
    //const uint32_t category_code = 0x0000ff00;
    //const uint32_t source_number = B(16) | B(17) | B(18) | B(19);
    //const uint32_t channel_number = B(20) | B(21) | B(22) | B(23);
    const uint32_t sampling_frequency = B(24) | B(25) | B(26) | B(27);
    //const uint32_t clock_accuracy = B(28) | B(29);
    const uint32_t word_length = B(32) | B(33) | B(34) | B(35);
    //const uint32_t original_sampling_frequency = B(36) | B(37) | B(38) | B(39);
    //const uint32_t cgms_a = B(40) | B(41);
#undef B
#undef _B
    const uint32_t STSZ = 6;
    uint32_t ChannelStatusMask[STSZ];

    //
    // Copy the userspace mask for the channel status bits. This consists mostly
    // of set bits.
    //

    for (uint32_t i = 0; i < STSZ; i++)
    {
        ChannelStatusMask[i] = MixerSettings.OutputConfiguration.iec958_mask.status[i * 4 + 0] << 24 |
                               MixerSettings.OutputConfiguration.iec958_mask.status[i * 4 + 1] << 16 |
                               MixerSettings.OutputConfiguration.iec958_mask.status[i * 4 + 2] <<  8 |
                               MixerSettings.OutputConfiguration.iec958_mask.status[i * 4 + 3] <<  0;
    }

    // these should never be overlaid
    ChannelStatusMask[0] &= ~(use_of_channel_status_block |
                              linear_pcm_identification |
                              additional_format_information | /* auto fill in for PCM, 000 for coded data */
                              mode |
                              sampling_frequency);  /* auto fill in */
    ChannelStatusMask[1] &= ~(word_length);

    //
    // The mask is now complete so we can overlay the userspace supplied channel
    // status bits.
    //
    for (uint32_t i = 0; i < STSZ; i++)
    {
        SpdifOut.Spdifout.ChannelStatus[i] |= ChannelStatusMask[i] &
                                              ((MixerSettings.OutputConfiguration.iec958_metadata.status[i * 4 + 0] << 24) |
                                               (MixerSettings.OutputConfiguration.iec958_metadata.status[i * 4 + 1] << 16) |
                                               (MixerSettings.OutputConfiguration.iec958_metadata.status[i * 4 + 2] << 8) |
                                               (MixerSettings.OutputConfiguration.iec958_metadata.status[i * 4 + 3] << 0));
    }
}

////////////////////////////////////////////////////////////////////////////
///
/// Fill out the PCM post-processing required for the common processing chain.
///
PlayerStatus_t Mixer_Mme_c::FillCommonChainPcmParameters(uint8_t *ptr, uint32_t &size, int ApplicationType)
{
    size = 0;

    // Setup the DDRE/PcmRender Processing structure
    if (STM_SE_CTRL_VALUE_AUDIO_APPLICATION_MS11 == ApplicationType ||
        STM_SE_CTRL_VALUE_AUDIO_APPLICATION_MS12 == ApplicationType)
    {
        bool DV258Enabled = false; // Upon DV258 integration update this flag based upon alsa control for Dolby Volume.
        MME_METARENCGlobalParams_t *MetaRencode = reinterpret_cast<MME_METARENCGlobalParams_t *>(ptr);

        memset(MetaRencode, 0, sizeof(*MetaRencode));
        MetaRencode->Id                        = PCMPROCESS_SET_ID(ACC_PCM_METARENC_ID, ACC_MIX_COMMON);
        MetaRencode->StructSize                = sizeof(MME_METARENCGlobalParams_t);
        MetaRencode->Apply                     = ACC_MME_ENABLED;
        MetaRencode->MetadataType              = METADATA_TYPE_DOLBY;
        MetaRencode->MetaCommon.StartupDelay   = 0;
        MetaRencode->MetaCommon.TargetLevel    = 0; // Let Limiter take care of targetLevel Adjustments per chain

        // If DV258 enabled on Common chain before DDRE, set No Compression mode else Film standard
        MetaRencode->MetaDataDesc.DDRECfg.CompProfile = DV258Enabled
                                                        ? DDRE_COMPR_NO_COMPRESSION
                                                        : DDRE_COMPR_FILM_STANDARD;

        for (uint32_t PlayerAttached(0), PlayerIdx(0); PlayerAttached < NumberOfAudioPlayerAttached; PlayerIdx++)
        {
            if (true == MixerPlayer[PlayerIdx].IsPlayerObjectAttached())
            {
                stm_se_drc_t audioplayerdrcConfig;
                PlayerAttached++;
                MixerPlayer[PlayerIdx].DebugDump();
                MixerPlayer[PlayerIdx].GetCompoundOption(STM_SE_CTRL_AUDIO_DYNAMIC_RANGE_COMPRESSION, (void *) &audioplayerdrcConfig);

                // Fill the Common DRC/Compression params
                MetaRencode->MetaCommon.ComprModeMap    |= (audioplayerdrcConfig.mode == STM_SE_COMP_RF_MODE) << PlayerIdx;
                MetaRencode->MetaCommon.Cut[PlayerIdx]   = ConvQ8ToPercent(audioplayerdrcConfig.cut);
                MetaRencode->MetaCommon.Boost[PlayerIdx] = ConvQ8ToPercent(audioplayerdrcConfig.boost);
            }
        }
        size += MetaRencode->StructSize;
    }

    PrepareMixerPcmProcTuningCommand(&MixerSettings.MixerOptions.VolumeManagerParams,
                                     MixerSettings.MixerOptions.VolumeManagerAmount,
                                     ACC_MIX_COMMON, ptr, &size);
    PrepareMixerPcmProcTuningCommand(&MixerSettings.MixerOptions.VirtualizerParams,
                                     MixerSettings.MixerOptions.VirtualizerAmount,
                                     ACC_MIX_COMMON, ptr, &size);
    PrepareMixerPcmProcTuningCommand(&MixerSettings.MixerOptions.UpmixerParams,
                                     MixerSettings.MixerOptions.UpmixerAmount,
                                     ACC_MIX_COMMON, ptr, &size);
    PrepareMixerPcmProcTuningCommand(&MixerSettings.MixerOptions.DialogEnhancerParams,
                                     MixerSettings.MixerOptions.DialogEnhancerAmount,
                                     ACC_MIX_COMMON, ptr, &size);

    return PlayerNoError;
}

////////////////////////////////////////////////////////////////////////////
///
/// Fill out the PCM post-processing required for a single physical output.
///
PlayerStatus_t Mixer_Mme_c::FillOutDevicePcmParameters(MME_LxPcmPostProcessingGlobalParameters_Frozen_t &PcmParams,
                                                       uint32_t MixerPlayerIndex,
                                                       int ApplicationType,
                                                       int RegionType)
{
    const int PER_SPEAKER_DELAY_NULL = 0;
    PcmPlayer_c::OutputEncoding OutputEncoding = MixerPlayer[MixerPlayerIndex].LookupOutputEncoding(PrimaryCodedDataType[MIXER_CODED_DATA_INPUT_SPDIF_IDX],
                                                                                                    PrimaryCodedDataType[MIXER_CODED_DATA_INPUT_HDMI_IDX]);
    uint32_t SampleRateHz = LookupPcmPlayerOutputSamplingFrequency(OutputEncoding, MixerPlayerIndex);
    // we must send the sample rate here since we need the conditional disabling of the encoders...
    // So refine the output encoding with previously computed SampleRateHz.
    OutputEncoding = MixerPlayer[MixerPlayerIndex].LookupOutputEncoding(PrimaryCodedDataType[MIXER_CODED_DATA_INPUT_SPDIF_IDX],
                                                                        PrimaryCodedDataType[MIXER_CODED_DATA_INPUT_HDMI_IDX],
                                                                        SampleRateHz);
    //
    // Summarize the processing that are enabled
    //
    // Get bassmgt config from audio-player
    stm_se_bassmgt_t  audioplayerBassMgtConfig;
    MixerPlayer[MixerPlayerIndex].GetCompoundOption(STM_SE_CTRL_BASSMGT , (void *) &audioplayerBassMgtConfig);
    bool EnableBassMgt = (STM_SE_CTRL_VALUE_APPLY == audioplayerBassMgtConfig.apply) ? true : false;
    bool EnableDcRemove = false;
    MixerPlayer[MixerPlayerIndex].GetOption(STM_SE_CTRL_DCREMOVE, (int &)EnableDcRemove);
    SE_DEBUG(group_mixer, ">%s Card %d: EnableDcRemove=%d\n",
             Name,
             MixerPlayerIndex,
             EnableDcRemove);
    // the encoder choice complex - this is determined later
    stm_se_limiter_t  audioplayerLimiterConfig;
    MixerPlayer[MixerPlayerIndex].GetCompoundOption(STM_SE_CTRL_LIMITER, (void *) &audioplayerLimiterConfig);
    bool EnableLimiter = (STM_SE_CTRL_VALUE_APPLY == audioplayerLimiterConfig.apply) ? true : false;
    bool EnableDMix = (true == CheckDownmixBypass(MixerPlayer[MixerPlayerIndex].GetCard())) ? false : true;
    SE_DEBUG(group_mixer, ">%s Card %d: %s is %s\n",
             Name,
             MixerPlayerIndex,
             MixerPlayer[MixerPlayerIndex].GetCardName(),
             PcmPlayer_c::LookupOutputEncoding(OutputEncoding));
    //
    // Update the enables if we are in a bypassed mode
    //

    if (PcmPlayer_c::IsOutputBypassed(OutputEncoding))
    {
        EnableBassMgt = false;
        EnableDcRemove = false;
        EnableDMix = false;
        EnableLimiter = false;
    }

    memset(&PcmParams, 0, sizeof(PcmParams));
    PcmParams.Id = ACC_RENDERER_MIXER_POSTPROCESSING_ID;
    // These next two would be relevant if doing a single card
    PcmParams.NbPcmProcessings = 0; //!< NbPcmProcessings on main[0..3] and aux[4..7]
    PcmParams.AuxSplit = SPLIT_AUX;     //! Point of split between Main output and Aux output
    uint32_t HeaderSize = offsetof(MME_LxPcmPostProcessingGlobalParameters_Frozen_t, AuxSplit) + sizeof(MME_LxPcmPostProcessingGlobalParameters_Frozen_t::AuxSplit);
    SE_DEBUG(group_mixer, "@%s HeaderSize: %d\n", Name, HeaderSize);
    PcmParams.StructSize = HeaderSize;
    {
        MME_ExtBassMgtGlobalParams_t &ExtBassMgt = PcmParams.BassMgt;
        ExtBassMgt.Id = PCMPROCESS_SET_ID(ACC_PCM_BASSMGT_ID, MixerPlayerIndex);
        ExtBassMgt.StructSize = sizeof(ExtBassMgt);
        PcmParams.StructSize += ExtBassMgt.StructSize;

        if (!EnableBassMgt)
        {
            ExtBassMgt.Apply = ACC_MME_DISABLED;
        }
        else
        {
            //const int BASSMGT_ATTENUATION_MUTE = 9600;
            ExtBassMgt.Apply = ACC_MME_ENABLED;

            switch (audioplayerBassMgtConfig.type)
            {
            case STM_SE_BASSMGT_SPEAKER_BALANCE:
                ExtBassMgt.Type = BASSMGT_VOLUME_CTRL;
                break;

            case STM_SE_BASSMGT_DOLBY_1:
                ExtBassMgt.Type = BASSMGT_DOLBY_1;
                break;

            case STM_SE_BASSMGT_DOLBY_2:
                ExtBassMgt.Type = BASSMGT_DOLBY_2;
                break;

            case STM_SE_BASSMGT_DOLBY_3:
                ExtBassMgt.Type = BASSMGT_DOLBY_3;
                break;

            default:
                SE_ERROR("Unexpected BassMgt type\n");
            }

            ExtBassMgt.LfeOut      = ACC_MME_TRUE;
            ExtBassMgt.BoostOut    = (true == audioplayerBassMgtConfig.boost_out) ? ACC_MME_ENABLED : ACC_MME_DISABLED;
            ExtBassMgt.Prologic    = ACC_MME_TRUE;
            ExtBassMgt.WsOut       = audioplayerBassMgtConfig.ws_out;
            ExtBassMgt.Limiter     = 0;
            ExtBassMgt.InputRoute  = ACC_MIX_MAIN;
            ExtBassMgt.OutputRoute = ACC_MIX_MAIN;
            // this is rather naughty (the gain values are linear everywhere else but are mapped onto a logarithmic scale here)
            ExtBassMgt.VolumeInmB = 1; // attenuation in mB

            //memset(ExtBassMgt.Volume, BASSMGT_ATTENUATION_MUTE, sizeof(ExtBassMgt.Volume));
            for (uint32_t ChannelIdx(0); ChannelIdx < SND_PSEUDO_MIXER_CHANNELS; ChannelIdx++)
            {
                // remap ALSA channels definition to FW channels definition
                // STKPI uses a signed gain value whereas we are setting a positive attenuation (explains why we use a negative value below)
                ExtBassMgt.Volume[ChannelIdx] = -audioplayerBassMgtConfig.gain[remap_channels[ChannelIdx]]; // in mB
            }

            // Implementation choice: the delay is not applied to ExtMME_BassMgtGlobalParams_t FW component but
            // to MME_DelayGlobalParams_t FW component which is practically implemented in the FW as a BassMgt component dedicated to the delay
            ExtBassMgt.DelayEnable = ACC_MME_FALSE;
            // From ACC-BL023-3BD
            ExtBassMgt.CutOffFrequency = audioplayerBassMgtConfig.cut_off_frequency;
            ExtBassMgt.FilterOrder     = audioplayerBassMgtConfig.filter_order;
            SE_DEBUG(group_mixer, "%s Card %d %s : BassMgt enabled, Volume in mB (%d)(%d)(%d)(%d)-(%d)(%d)(%d)(%d)\n",
                     Name,
                     MixerPlayerIndex,
                     MixerPlayer[MixerPlayerIndex].GetCardName(),
                     ExtBassMgt.Volume[0], ExtBassMgt.Volume[1], ExtBassMgt.Volume[2], ExtBassMgt.Volume[3],
                     ExtBassMgt.Volume[4], ExtBassMgt.Volume[5], ExtBassMgt.Volume[6], ExtBassMgt.Volume[7]
                    );
        }
    }
    {
        MME_EqualizerGlobalParams_t &Equalizer = PcmParams.Equalizer;
        Equalizer.Id = PCMPROCESS_SET_ID(ACC_PCM_EQUALIZER_ID, MixerPlayerIndex);
        Equalizer.StructSize = sizeof(Equalizer);
        PcmParams.StructSize += Equalizer.StructSize;
        Equalizer.Apply = ACC_MME_DISABLED;
    }
    {
        MME_TempoGlobalParams_t &TempoControl = PcmParams.TempoControl;
        TempoControl.Id = PCMPROCESS_SET_ID(ACC_PCM_TEMPO_ID, MixerPlayerIndex);
        TempoControl.StructSize = sizeof(TempoControl);
        PcmParams.StructSize += TempoControl.StructSize;
        TempoControl.Apply = ACC_MME_DISABLED;
    }
    {
        MME_DCRemoveGlobalParams_t &DCRemove = PcmParams.DCRemove;
        DCRemove.Id = PCMPROCESS_SET_ID(ACC_PCM_DCREMOVE_ID, MixerPlayerIndex);
        DCRemove.StructSize = sizeof(DCRemove);
        PcmParams.StructSize += DCRemove.StructSize;
        DCRemove.Apply = (EnableDcRemove ? ACC_MME_ENABLED : ACC_MME_DISABLED);
    }
    {
        MME_DelayGlobalParams_t &Delay = PcmParams.Delay;
        Delay.Id = PCMPROCESS_SET_ID(ACC_PCM_DELAY_ID, MixerPlayerIndex);
        Delay.StructSize = sizeof(Delay);
        PcmParams.StructSize += Delay.StructSize;
        Delay.Apply = ACC_MME_ENABLED;
        Delay.DelayUpdate = ACC_MME_TRUE;
        memset(&Delay.Delay[0], PER_SPEAKER_DELAY_NULL, sizeof(Delay.Delay));

        // Implementation choice: this FW component is always instantiated but, by default, delay is set to 0 for all channels
        if (STM_SE_CTRL_VALUE_APPLY == audioplayerBassMgtConfig.delay_enable)
        {
            for (uint32_t ChannelIdx(0); ChannelIdx < SND_PSEUDO_MIXER_CHANNELS; ChannelIdx++)
            {
                // remap ALSA channels definition to FW channels definition
                Delay.Delay[ChannelIdx] = audioplayerBassMgtConfig.delay[remap_channels[ChannelIdx]]; // in msec
            }

            SE_DEBUG(group_mixer, ">%s Card %d %s : Delay enabled, delay in msec (%d)(%d)(%d)(%d)-(%d)(%d)(%d)(%d)\n",
                     Name,
                     MixerPlayerIndex,
                     MixerPlayer[MixerPlayerIndex].GetCardName(),
                     Delay.Delay[0], Delay.Delay[1], Delay.Delay[2], Delay.Delay[3],
                     Delay.Delay[4], Delay.Delay[5], Delay.Delay[6], Delay.Delay[7]
                    );
        }
    }
    {
        MME_EncoderPPGlobalParams_t &Encoder = PcmParams.Encoder;
        Encoder.Id = PCMPROCESS_SET_ID(ACC_PCM_ENCODER_ID, MixerPlayerIndex);
        Encoder.StructSize = sizeof(Encoder);
        PcmParams.StructSize += Encoder.StructSize;

        // maybe setup AC3/etc encoding if SPDIF device.
        switch (OutputEncoding)
        {
        case PcmPlayer_c::OUTPUT_PCM:
        case PcmPlayer_c::OUTPUT_IEC60958:
        case PcmPlayer_c::BYPASS_AC3:
        case PcmPlayer_c::BYPASS_DTS_512:
        case PcmPlayer_c::BYPASS_DTS_1024:
        case PcmPlayer_c::BYPASS_DTS_2048:
        case PcmPlayer_c::BYPASS_DTS_CDDA:
        case PcmPlayer_c::BYPASS_DTSHD_LBR:
        case PcmPlayer_c::BYPASS_DTSHD_HR:
        case PcmPlayer_c::BYPASS_DTSHD_DTS_4096:
        case PcmPlayer_c::BYPASS_DTSHD_DTS_8192:
        case PcmPlayer_c::BYPASS_DTSHD_MA:
        case PcmPlayer_c::BYPASS_DDPLUS:
        case PcmPlayer_c::BYPASS_TRUEHD:
        case PcmPlayer_c::BYPASS_AAC:
        case PcmPlayer_c::BYPASS_HEAAC_960:
        case PcmPlayer_c::BYPASS_HEAAC_1024:
        case PcmPlayer_c::BYPASS_HEAAC_1920:
        case PcmPlayer_c::BYPASS_HEAAC_2048:
        case PcmPlayer_c::BYPASS_SPDIFIN_COMPRESSED:
        case PcmPlayer_c::BYPASS_SPDIFIN_PCM:
        {
            SE_DEBUG(group_mixer, "@ Encoder.Apply->ACC_MME_DISABLED\n");
            Encoder.Apply = ACC_MME_DISABLED;
        }
        break;

        case PcmPlayer_c::OUTPUT_AC3:
        {
            SE_DEBUG(group_mixer, "@ OUTPUT_AC3: Encoder.Apply->ACC_MME_ENABLED\n");
            Encoder.Apply = ACC_MME_ENABLED;
            Encoder.Type = ACC_ENCODERPP_DDCE;  //type=bits0-3, subtype=bits4-7
            // let the encoder insert preambles
            ACC_ENCODERPP_SET_IECENABLE(((MME_EncoderPPGlobalParams_t *)&Encoder), 1);
            Encoder.BitRate = 448;           //DDCE:448000bps DTS:1536000bps
        }
        break;

        case PcmPlayer_c::OUTPUT_DTS:
        {
            SE_DEBUG(group_mixer, "@ OUTPUT_DTS: Encoder.Apply->ACC_MME_ENABLED\n");
            Encoder.Apply = ACC_MME_ENABLED;
            Encoder.Type = ACC_ENCODERPP_DTSE;  //type=bits0-3, subtype=bits4-7
            // let the encoder insert preambles
            ACC_ENCODERPP_SET_IECENABLE(((MME_EncoderPPGlobalParams_t *)&Encoder), 1);
            Encoder.BitRate = 1536000;          //DDCE:448000bps DTS:1536000bps
        }
        break;

        case PcmPlayer_c::OUTPUT_FATPIPE:
        {
        }
        break;

        default:
            SE_ASSERT(0);
        }
    }
    //
    {
        MME_SfcPPGlobalParams_t &Sfc = PcmParams.Sfc;
        Sfc.Id = PCMPROCESS_SET_ID(ACC_PCM_SFC_ID, MixerPlayerIndex);
        Sfc.StructSize = sizeof(Sfc);
        PcmParams.StructSize += Sfc.StructSize;
        Sfc.Apply = ACC_MME_DISABLED;
    }
    //
    {
        MME_Resamplex2GlobalParams_t &Resamplex2 = PcmParams.Resamplex2;
        Resamplex2.Id = PCMPROCESS_SET_ID(ACC_PCM_RESAMPLE_ID, MixerPlayerIndex);
        Resamplex2.StructSize = sizeof(Resamplex2);
        PcmParams.StructSize += Resamplex2.StructSize;
        SE_DEBUG(group_mixer, "## SampleRateHz %d MixerSamplingFrequency:%d ApplicationType: %s\n", SampleRateHz, MixerSamplingFrequency,
                 Mixer_Mme_c::LookupPolicyValueAudioApplicationType(ApplicationType));

        if (((SampleRateHz == MixerSamplingFrequency) && (OutputEncoding != PcmPlayer_c::OUTPUT_AC3) && (OutputEncoding != PcmPlayer_c::OUTPUT_DTS)) ||
            (true == PcmPlayer_c::IsOutputBypassed(OutputEncoding)))
        {
            Resamplex2.Apply = ACC_MME_DISABLED;
        }
        else
        {
            enum eFsRange FsRange;
            enum eAccProcessApply ProcessApply;
            FsRange = TranslateIntegerSamplingFrequencyToRange(SampleRateHz);

            if ((OutputEncoding == PcmPlayer_c::OUTPUT_AC3) || (OutputEncoding == PcmPlayer_c::OUTPUT_DTS))
            {
                // When the outputs are encoded then we have to deploy resampling at the beginning of the PCM
                // chain. Thus if the output rate differs from the mixers native rate then we need to deploy.
                ProcessApply = (SampleRateHz != MixerSamplingFrequency ? ACC_MME_ENABLED : ACC_MME_DISABLED);
            }
            else
            {
                // it is important to use _AUTO here (rather than conditionally selecting _ENABLE ourselves)
                // because _AUTO also leaves the firmware free to apply the resampling at the point it believes is
                // best.using _ENABLE causes the processing to unconditionally applied at the start of the chain.
                ProcessApply = ACC_MME_AUTO;
            }

            SE_DEBUG(group_mixer, "SampleRateHz %d  FsRange %d  ACC_FSRANGE_48k %d  ProcessApply %d\n",
                     SampleRateHz, FsRange, ACC_FSRANGE_48k, ProcessApply);
            Resamplex2.Apply = ProcessApply;
            Resamplex2.Range = FsRange;
        }

        SE_DEBUG(group_mixer,  "%s post-mix resampling (%s)\n",
                 (Resamplex2.Apply == ACC_MME_ENABLED ? "Enabled" :
                  Resamplex2.Apply == ACC_MME_AUTO ? "Automatic" : "Disabled"),
                 LookupDiscreteSamplingFrequencyRange((enum eFsRange) Resamplex2.Range));
    }
    //
    {
        // Get BTSC config from audio-player
        stm_se_btsc_t                               audioplayerBtscConfig;
        stm_se_ctrl_audio_player_hardware_mode_t    audioplayerHwMode;
        MixerPlayer[MixerPlayerIndex].GetCompoundOption(STM_SE_CTRL_BTSC, (void *) &audioplayerBtscConfig);
        MixerPlayer[MixerPlayerIndex].GetCompoundOption(STM_SE_CTRL_AUDIO_PLAYER_HARDWARE_MODE, (void *) &audioplayerHwMode);
        MME_BTSCGlobalParams_t &Btsc = PcmParams.Btsc;
        Btsc.Id = PCMPROCESS_SET_ID(ACC_PCMPRO_BTSC_ID, MixerPlayerIndex);
        Btsc.StructSize = sizeof(Btsc);
        PcmParams.StructSize += Btsc.StructSize;
        Btsc.DualSignal = ACC_MME_FALSE;
        Btsc.Apply = ACC_MME_DISABLED;
        Btsc.OutAt192K = ACC_MME_FALSE;

        if ((STM_SE_PLAYER_I2S == audioplayerHwMode.player_type) && (STM_SE_PLAY_BTSC_OUT == audioplayerHwMode.playback_mode))
        {
            Btsc.Apply = ACC_MME_ENABLED;
        }

        if (audioplayerBtscConfig.dual_signal)
        {
            Btsc.DualSignal = ACC_MME_TRUE;
        }

        Btsc.InputGain = audioplayerBtscConfig.input_gain;
        Btsc.TXGain = audioplayerBtscConfig.tx_gain;
        Btsc.GainInMB = ACC_MME_TRUE;

        if (MixerPlayer[MixerPlayerIndex].GetCardMaxFreq() >= 192000)
        {
            Btsc.OutAt192K = ACC_MME_TRUE;
        }

        SE_DEBUG(group_mixer, "BTSC Config Apply = %d, DualSignal = %d, InputGain = 0x%x, TxGain = 0x%x\n",
                 Btsc.Apply, Btsc.DualSignal, Btsc.InputGain, Btsc.TXGain);
    }
    //
    {
        MME_CMCGlobalParams_t &CMC = PcmParams.CMC;
        CMC.Id = PCMPROCESS_SET_ID(ACC_PCM_CMC_ID, MixerPlayerIndex);
        CMC.StructSize = sizeof(CMC);
        PcmParams.StructSize += CMC.StructSize;
        CMC.ExtendedParams.MixCoeffs.GlobalGainCoeff = ACC_UNITY;
        CMC.ExtendedParams.MixCoeffs.LfeMixCoeff = ACC_UNITY;

        // Setting DUAL_MODE configuration for this player
        stm_se_dual_mode_t player_dualmode;
        MixerPlayer[MixerPlayerIndex].GetOption(STM_SE_CTRL_DUALMONO, (int &)player_dualmode);

        switch (player_dualmode)
        {
        case STM_SE_STEREO_OUT:
            CMC.Config[CMC_DUAL_MODE] = ACC_DUAL_LR;
            break;

        case STM_SE_DUAL_LEFT_OUT:
            CMC.Config[CMC_DUAL_MODE] = ACC_DUAL_LEFT_MONO;
            break;

        case STM_SE_DUAL_RIGHT_OUT:
            CMC.Config[CMC_DUAL_MODE] = ACC_DUAL_RIGHT_MONO;
            break;

        case STM_SE_MONO_OUT:
            CMC.Config[CMC_DUAL_MODE] = ACC_DUAL_MIX_LR_MONO;
            break;

        default:
            SE_ERROR("Invalid Dual Mode from Audio_Player\n");
            break;
        }

        int isStreamDrivenDualMono;
        MixerPlayer[MixerPlayerIndex].GetOption(STM_SE_CTRL_STREAM_DRIVEN_DUALMONO, isStreamDrivenDualMono);

        if (isStreamDrivenDualMono)
        {
            CMC.Config[CMC_DUAL_MODE] = CMC.Config[CMC_DUAL_MODE] | ACC_DUAL_1p1_ONLY;
        }

        CMC.Config[CMC_PCM_DOWN_SCALED] = ACC_MME_FALSE;
        CMC.CenterMixCoeff = ACC_M3DB;
        CMC.SurroundMixCoeff = ACC_M3DB;

        switch (OutputEncoding)
        {
        case PcmPlayer_c::OUTPUT_FATPIPE:
        {
            /* FatPipe is a malleable output surface */
            CMC.Config[CMC_OUTMODE_MAIN] = LookupFatPipeOutputMode(LookupMixerAudioMode());
            CMC.Config[CMC_OUTMODE_AUX] = ACC_MODE_ID;
        }
        break;

        case PcmPlayer_c::OUTPUT_AC3:
        case PcmPlayer_c::OUTPUT_DTS:
        {
            CMC.Config[CMC_OUTMODE_MAIN] = ACC_MODE32_LFE; /* traditional 5.1 */
            CMC.Config[CMC_OUTMODE_AUX] = ACC_MODE_ID;
        }
        break;

        default:
        {
            CMC.Config[CMC_OUTMODE_MAIN] = TranslateDownstreamCardToMainAudioMode(MixerPlayer[MixerPlayerIndex].GetCard());
            CMC.Config[CMC_OUTMODE_AUX ] = ACC_MODE_ID;

            // handle rerouting of PAIRx on PAIR0 for DD+ certification ::
            if (CMC.Config[CMC_OUTMODE_MAIN] == ACC_MODE_ALL2)
            {
                uint32_t pair0 = MixerPlayer[MixerPlayerIndex].GetCard().channel_assignment.pair0;

                pair0 = pair0 - STM_SE_AUDIO_CHANNEL_PAIR_PAIR0;
                PcmParams.CMC.ExtendedParams.MainPcmConfig.ChannelPair0 = ACC_CHANNEL_PAIR_PAIR0 + pair0;
                SE_INFO(group_mixer,  "%s (Card%d) reroutes pair %d to its output\n", MixerPlayer[MixerPlayerIndex].GetCardName(), MixerPlayerIndex, pair0);
            }

#ifndef BUG_4518

            // The downmixer in the current firmware doesn't support 7.1 outputs (can't automatically generate
            // downmix tables for them). We therefore need to fall back to ACC_MODE32(_LFE) unless the decoder
            // is already chucking out samples in the correct mode.
            // However if we have custom downmix matrix then we won't have to do this.
            if (!DownmixFirmware && (ACC_MODE34 == CMC.Config[CMC_OUTMODE_MAIN] || ACC_MODE34_LFE == CMC.Config[CMC_OUTMODE_MAIN]))
            {
                eAccAcMode DecoderMode = LookupMixerAudioMode();

                if (CMC.Config[CMC_OUTMODE_MAIN] != DecoderMode)
                {
                    if (ACC_MODE34_LFE == CMC.Config[CMC_OUTMODE_MAIN])
                    {
                        if (ACC_MODE34 == DecoderMode)
                        {
                            CMC.Config[CMC_OUTMODE_MAIN] = ACC_MODE34;
                        }
                        else
                        {
                            CMC.Config[CMC_OUTMODE_MAIN] = ACC_MODE32_LFE;
                        }
                    }
                    else
                    {
                        CMC.Config[CMC_OUTMODE_MAIN] = ACC_MODE32;
                    }
                }
            }

#endif
#ifndef BUG_4518
            // The mixer doesn't support auxiliary outputs in the current firmware
            CMC.Config[CMC_OUTMODE_AUX] = ACC_MODE_ID;
#else
            CMC.Config[CMC_OUTMODE_AUX] = TranslateDownstreamCardToAuxAudioMode(&Player[MixerPlayerIndex]->card);
#endif
            break;
        } // end of default
        } // end of switch

        SE_DEBUG(group_mixer,  "CMC output mode (%d): %d channels  %s and %s (main and auxiliary) - Dual mode=%d (1p1_only=%d)\n",
                 MixerPlayerIndex, MixerPlayer[MixerPlayerIndex].GetCardNumberOfChannels(),
                 LookupAudioMode((eAccAcMode) CMC.Config[CMC_OUTMODE_MAIN]),
                 LookupAudioMode((eAccAcMode) CMC.Config[CMC_OUTMODE_AUX]),
                 (CMC.Config[CMC_DUAL_MODE] & 0x3),
                 ((CMC.Config[CMC_DUAL_MODE] & ACC_DUAL_1p1_ONLY) == ACC_DUAL_1p1_ONLY)
                );
    }
    //
    FillOutDeviceDownmixParameters(PcmParams, MixerPlayerIndex, EnableDMix, RegionType);
    //
    FillOutDeviceSpdifParameters(PcmParams, MixerPlayerIndex, OutputEncoding);
    //
    {
        // Get limiter config from audio-player
        int               gain, delay, softmute;
        MixerPlayer[MixerPlayerIndex].GetOption(STM_SE_CTRL_AUDIO_GAIN ,    gain);
        MixerPlayer[MixerPlayerIndex].GetOption(STM_SE_CTRL_AUDIO_DELAY,    delay);
        MixerPlayer[MixerPlayerIndex].GetOption(STM_SE_CTRL_AUDIO_SOFTMUTE, softmute);
        MME_LimiterGlobalParams_t  &Limiter = PcmParams.Limiter;
        Limiter.Id = PCMPROCESS_SET_ID(ACC_PCM_LIMITER_ID, MixerPlayerIndex);
        Limiter.StructSize = sizeof(Limiter);
        PcmParams.StructSize += Limiter.StructSize;
        Limiter.Apply         = (EnableLimiter ? ACC_MME_ENABLED : ACC_MME_DISABLED);
        Limiter.LimiterEnable = ACC_MME_TRUE;
        Limiter.SvcEnable     = ACC_MME_TRUE;
        // soft mute is triggered by a 0 to 1 transition, and unmute by a 1 to 0 transition
        Limiter.SoftMute = softmute;
        Limiter.GainInMilliBell = 1; //Gain is given in mB
        // Limiter Gain = MasterPlaybackVolume (general, applied to all audio-players) + specific audio-player gain
        Limiter.Gain            = MixerSettings.MixerOptions.MasterPlaybackVolume + gain;
        SE_DEBUG(group_mixer, ">%s Card%d %s :  LimiterGain (%d) = MasterPlaybackVolume (%d) + specific player gain (%d)\n",
                 Name,
                 MixerPlayerIndex,
                 MixerPlayer[MixerPlayerIndex].GetCardName(),
                 Limiter.Gain,
                 MixerSettings.MixerOptions.MasterPlaybackVolume,
                 gain
                );
        Limiter.DelayEnable     = 1;  // enable delay engine
        Limiter.MuteDuration    = MIXER_LIMITER_MUTE_RAMP_DOWN_PERIOD;
        Limiter.UnMuteDuration  = MIXER_LIMITER_MUTE_RAMP_UP_PERIOD;
        SE_DEBUG(group_mixer, "< Limiter Gain before target_level modificators= %d >\n", Limiter.Gain);

        if (MixerPlayer[MixerPlayerIndex].GetCard().target_level)
        {
            //when (target_level != 0), the target playback level uses the value provided by the control STM_SE_CTRL_AUDIO_PROGRAM_PLAYBACK_LEVEL
            Limiter.AdjustProgramRefLevel = ACC_MME_TRUE;       // The actual gain to apply will be adjusted wrt actual audio dialog-level
            Limiter.Gain += MixerPlayer[MixerPlayerIndex].GetCard().target_level; // Combine User-Requested volume and audio-player target-level
            SE_DEBUG(group_mixer, "card[%d] : target_level=%d, AdjustProgramRefLevel=%d\n",
                     MixerPlayerIndex, MixerPlayer[MixerPlayerIndex].GetCard().target_level, Limiter.AdjustProgramRefLevel);
        }
        else
        {
            //when (target_level = 0), the target playback level is driven by AUDIO_APPLICATION_TYPE
            if (ApplicationType < NB_AUDIO_APPLICATION_TYPES)
            {
                stm_se_player_sink_t playerSinkType = MixerPlayer[MixerPlayerIndex].GetSinkType();
                I32                  gainAdjust     = 0;
                Limiter.AdjustProgramRefLevel = LimiterGainAdjustLookupTable[ApplicationType].AdjustPRL;

                switch (playerSinkType)
                {
                case STM_SE_PLAYER_SINK_AUTO:
                {
                    bool IsConnectedToHdmi  = MixerPlayer[MixerPlayerIndex].IsConnectedToHdmi();
                    bool IsConnectedToSpdif = MixerPlayer[MixerPlayerIndex].IsConnectedToSpdif();

                    // Connected to HDMI Only: default output = TV
                    if (IsConnectedToHdmi)
                    {
                        gainAdjust = LimiterGainAdjustLookupTable[ApplicationType].PRL_TV;
                        SE_DEBUG(group_mixer, "Default HDMI PRL  ==> TV ");
                    }
                    // Connected to SPDIF : default output = AVR
                    else if (IsConnectedToSpdif)
                    {
                        gainAdjust = LimiterGainAdjustLookupTable[ApplicationType].PRL_AVR;
                        SE_DEBUG(group_mixer, "Default SPDIF PRL ==> AVR ");
                    }
                    // Connected to Analog: default output = TV
                    else
                    {
                        // Analog is considered as TVOuput (historical)
                        gainAdjust = LimiterGainAdjustLookupTable[ApplicationType].PRL_TV;
                        SE_DEBUG(group_mixer, "Default Analog PRL ==> TV ");
                    }

                    break;
                }

                case STM_SE_PLAYER_SINK_TV:
                    // Forced case : connected to TV
                    gainAdjust = LimiterGainAdjustLookupTable[ApplicationType].PRL_TV;
                    SE_DEBUG(group_mixer, "Apply PRL_TV ");
                    break;

                case STM_SE_PLAYER_SINK_AVR:
                    // Forced case : connected to AVR
                    gainAdjust = LimiterGainAdjustLookupTable[ApplicationType].PRL_AVR;
                    SE_DEBUG(group_mixer, "Apply PRL_AVR ");
                    break;

                case STM_SE_PLAYER_SINK_HEADPHONE:
                    // Forced case : connected to HeadPhone
                    gainAdjust = LimiterGainAdjustLookupTable[ApplicationType].PRL_HeadPhone;
                    SE_DEBUG(group_mixer, "Apply PRL_TV ");
                    break;

                default:
                    SE_ERROR("Invalid Player Sink Type: %d\n", playerSinkType);
                    break;
                }

                Limiter.Gain += gainAdjust;
                SE_DEBUG(group_mixer, "card[%d]: ApplicationType=%s, AdjustProgramRefLevel=%d, GainAdjust=%d\n",
                         MixerPlayerIndex ,
                         Mixer_Mme_c::LookupPolicyValueAudioApplicationType(ApplicationType),
                         Limiter.AdjustProgramRefLevel,
                         gainAdjust);
            }
        }

        // Limiter.Gain value should remains between [+3200 ,- 9600] mB
        if (Limiter.Gain < -9600)
        {
            // below down limit
            Limiter.Gain = -9600;
        }
        else if (Limiter.Gain > 3200)
        {
            // above up limit
            Limiter.Gain = 3200;
        }

        SE_DEBUG(group_mixer, "</card[%d]: Limiter Gain after target_level modificators= %d >\n\n", MixerPlayerIndex, Limiter.Gain);
        // During initialization (i.e. when MMEInitialized is false) we must set HardGain to 1. This
        // prevents the Limiter from setting the initial gain to -96db and applying gain smoothing
        // (which takes ~500ms to reach 0db) during startup.
        // After initialization, the HardGain is driven using the STKPI limiter control.
        Limiter.HardGain = (false == MMEInitialized) ? 1 : (uint32_t) audioplayerLimiterConfig.hard_gain;
        //  Limiter.Threshold = ;
        Limiter.DelayBuffer = NULL; // delay buffer will be allocated by firmware
        Limiter.DelayBufSize = 0;
        Limiter.Delay = delay; //in ms
    }
    {
        // DDRE is disabled on all outputs except the common chain
        MME_METARENCGlobalParams_t &MetaRencode = PcmParams.MetaRencode;
        MetaRencode.Id = PCMPROCESS_SET_ID(ACC_PCM_METARENC_ID, MixerPlayerIndex);
        MetaRencode.StructSize = sizeof(MetaRencode);
        PcmParams.StructSize += MetaRencode.StructSize;
        MetaRencode.Apply = ACC_MME_DISABLED;
    }

    int amount;
    struct PcmProcManagerParams_s  *VolumeManager = NULL;
    MixerPlayer[MixerPlayerIndex].GetCompoundOption(STM_SE_CTRL_VOLUME_MANAGER, (void *)&VolumeManager);
    MixerPlayer[MixerPlayerIndex].GetOption(STM_SE_CTRL_VOLUME_MANAGER_AMOUNT, amount);
    if (VolumeManager != NULL)
    {

        PrepareMixerPcmProcTuningCommand(VolumeManager,
                                         amount,
                                         (enum eAccMixOutput) MixerPlayerIndex,
                                         (uint8_t *) &PcmParams, (unsigned int *) &PcmParams.StructSize);
    }

    struct PcmProcManagerParams_s  *Virtualizer = NULL;
    MixerPlayer[MixerPlayerIndex].GetCompoundOption(STM_SE_CTRL_VIRTUALIZER, (void *)&Virtualizer);
    MixerPlayer[MixerPlayerIndex].GetOption(STM_SE_CTRL_VIRTUALIZER_AMOUNT, amount);
    if (Virtualizer != NULL)
    {
        PrepareMixerPcmProcTuningCommand(Virtualizer,
                                         amount,
                                         (enum eAccMixOutput) MixerPlayerIndex,
                                         (uint8_t *) &PcmParams, (unsigned int *) &PcmParams.StructSize);
    }

    struct PcmProcManagerParams_s  *Upmixer = NULL;
    MixerPlayer[MixerPlayerIndex].GetCompoundOption(STM_SE_CTRL_UPMIXER, (void *)&Upmixer);
    MixerPlayer[MixerPlayerIndex].GetOption(STM_SE_CTRL_UPMIXER_AMOUNT, amount);
    if (Upmixer != NULL && Upmixer->ProfileAddr != NULL)
    {
        PrepareMixerPcmProcTuningCommand(Upmixer,
                                         amount,
                                         (enum eAccMixOutput) MixerPlayerIndex,
                                         (uint8_t *) &PcmParams, (unsigned int *) &PcmParams.StructSize);
    }

    struct PcmProcManagerParams_s  *DialogEnhancer = NULL;
    MixerPlayer[MixerPlayerIndex].GetCompoundOption(STM_SE_CTRL_DIALOG_ENHANCER, (void *)&DialogEnhancer);
    MixerPlayer[MixerPlayerIndex].GetOption(STM_SE_CTRL_DIALOG_ENHANCER_AMOUNT, amount);
    if (DialogEnhancer != NULL && DialogEnhancer->ProfileAddr != NULL)
    {
        PrepareMixerPcmProcTuningCommand(DialogEnhancer,
                                         amount,
                                         (enum eAccMixOutput) MixerPlayerIndex,
                                         (uint8_t *) &PcmParams, (unsigned int *) &PcmParams.StructSize);
    }

    SE_DEBUG(group_mixer, "<%s PcmParams.StructSize: %d\n", Name, PcmParams.StructSize);
    return PlayerNoError;
}

////////////////////////////////////////////////////////////////////////////
///
/// ResetBufferTypeFlags resets all MME Buffer Flags to 0.
///
void Mixer_Mme_c::ResetBufferTypeFlags()
{
    unsigned int nb_buffers = MixerCommand.Command.NumberInputBuffers
                              + MixerCommand.Command.NumberOutputBuffers;

    for (int i = 0; i < nb_buffers; i++)
    {
        MixerCommand.Command.DataBuffers_p[i]->Flags = 0;
    }
}

////////////////////////////////////////////////////////////////////////////
///
/// Prepare the mixer command obtaining any required working buffers as we go.
///
/// The output buffers are obtained first since the this may involve waiting
/// for the output buffers to be ready. Waiting for output buffers is a blocking
/// operation (obviously).
///
/// The collection of input buffers is most definitely not a blocking operation
/// (since blocking due to starvation would cause a pop on the output).
///
PlayerStatus_t Mixer_Mme_c::FillOutMixCommand()
{
    ManifestorStatus_t Status;
    uint32_t InteractiveAudioIdx;
    bool FillInputWithSilentBuffer = false;
    SE_VERBOSE(group_mixer, ">\n");

    ResetBufferTypeFlags();

    if (MMEFirstMixerHdl != MME_INVALID_ARGUMENT)
    {
        Status = FillOutFirstMixerOutputBuffers();
        if (Status != PlayerNoError)
        {
            SE_ERROR("Failed to obtain/populate First Mixer output buffer\n");
            return Status;
        }
    }

    Status = FillOutOutputBuffers();

    if (Status != PlayerNoError)
    {
        SE_ERROR("Failed to obtain/populate output buffer\n");
        return Status;
    }

    if (WaitNCommandForAudioToAudioSync > 0)
    {
        bool AtLeastOneClientStarted   = ThereIsOneClientInState(STARTED);
        bool AllConnectedClientStarted = AllConnectedClientsAreInState(STARTED);

        if (AtLeastOneClientStarted && !AllConnectedClientStarted)
        {
            SE_DEBUG(group_mixer, "At least one client started but not all. Wait for other client to start by putting SilentBuffer. WaitNCommandForAudioToAudioSync = %d\n", WaitNCommandForAudioToAudioSync);
            FillInputWithSilentBuffer = true;
            WaitNCommandForAudioToAudioSync--;
        }
        else if (AllConnectedClientStarted)
        {
            SE_DEBUG(group_mixer, " All clients started. Main and Assosiated Audio(if present) will be in sync\n");
            WaitNCommandForAudioToAudioSync = 0;
        }
        else
        {
            SE_DEBUG(group_mixer, " No Client Started\n");
        }
    }

    if (MMEFirstMixerHdl != MME_INVALID_ARGUMENT)
    {
        for (uint32_t ClientIdx(0); ClientIdx < MIXER_MAX_CLIENTS; ClientIdx++)
        {
            Client[ClientIdx].LockTake();
            FillOutFirstMixerInputBuffer(ClientIdx, FillInputWithSilentBuffer);
            Client[ClientIdx].LockRelease();
        }

        FillOutSecondMixerInputBuffer();

    }
    else
    {
        for (uint32_t ClientIdx(0); ClientIdx < MIXER_MAX_CLIENTS; ClientIdx++)
        {
            Client[ClientIdx].LockTake();
            FillOutInputBuffer(ClientIdx, FillInputWithSilentBuffer);
            Client[ClientIdx].LockRelease();
        }
    }

    for (uint32_t AudioGeneratorIdx = 0; AudioGeneratorIdx < MAX_AUDIO_GENERATORS; AudioGeneratorIdx++)
    {
        FillOutAudioGenBuffer(AudioGeneratorIdx);
    }

    for (InteractiveAudioIdx = 0; InteractiveAudioIdx < MAX_INTERACTIVE_AUDIO; InteractiveAudioIdx++)
    {
        FillOutInteractiveAudioBuffer(MixerCommand.Command.DataBuffers_p[MIXER_INTERACTIVE_INPUT + InteractiveAudioIdx], InteractiveAudioIdx);
    }

    MixerCommand.InputParams.InputParam[MIXER_INTERACTIVE_INPUT].Command = MIXER_PLAY;
    MixerCommand.InputParams.InputParam[MIXER_INTERACTIVE_INPUT].StartOffset = 0;
    MixerCommand.InputParams.InputParam[MIXER_INTERACTIVE_INPUT].PTS = 0;
    MixerCommand.InputParams.InputParam[MIXER_INTERACTIVE_INPUT].PTSflags.U32 = 0;
    SE_VERBOSE(group_mixer, "<\n");
    return PlayerNoError;
}

PlayerStatus_t Mixer_Mme_c::FillOutFirstMixerOutputBuffers()
{
    //PlayerStatus_t Status;
    uint32_t OnePeriodSize = MixerPlayer[FirstActivePlayer].GetPcmPlayerConfigSurfaceParameters().PeriodSize;
    SE_VERBOSE(group_mixer, ">%s OnePeriodSize: %u\n", Name, OnePeriodSize);

    uint32_t OutBufIdx = FirstMixerCommand.Command.NumberInputBuffers;
    MME_DataBuffer_t *OutputDataBuffer;

    if (InterMixerFreeBufRing->Extract((uintptr_t *) &OutputDataBuffer, RING_NONE_BLOCKING) != RingNoError)
    {
        SE_ERROR("Cannot extract Buff from InterMixerFreeBufRing\n");
        return PlayerError;
    }

    FirstMixerCommand.DataBufferList[OutBufIdx]   = OutputDataBuffer;
    OutputDataBuffer->Flags                       = BUFFER_TYPE_AUDIO_IO;
    OutputDataBuffer->ScatterPages_p->FlagsIn     = 0;
    OutputDataBuffer->ScatterPages_p->BytesUsed   = 0;

    return PlayerNoError;
}

////////////////////////////////////////////////////////////////////////////
///
/// Populate a data buffer ready for it to contain output samples.
///
PlayerStatus_t Mixer_Mme_c::FillOutOutputBuffers()
{
    PlayerStatus_t Status;
    uint32_t OnePeriodSize = MixerPlayer[FirstActivePlayer].GetPcmPlayerConfigSurfaceParameters().PeriodSize;
    SE_VERBOSE(group_mixer, ">%s OnePeriodSize: %u\n", Name, OnePeriodSize);
    // This value cannot be pre-configured.
    MixerCommand.Command.NumberOutputBuffers = NumberOfAudioPlayerAttached;
    Status = MapPcmPlayersSamples(OnePeriodSize, false);

    if (Status != PlayerNoError)
    {
        SE_ERROR("Failed to map a period of samples (%u)\n", OnePeriodSize);
        DeployPcmPlayersUnderrunRecovery();
        return PlayerError;
    }

    for (uint32_t CountOfAudioPlayerAttached(0), PlayerIdx(0); CountOfAudioPlayerAttached < NumberOfAudioPlayerAttached; PlayerIdx++)
    {
        MME_DataBuffer_t *OutputDataBuffer = MixerCommand.Command.DataBuffers_p[MixerCommand.Command.NumberInputBuffers + CountOfAudioPlayerAttached];
        //By default, will be overwritten if needed.
        OutputDataBuffer->NumberOfScatterPages = 0;
        OutputDataBuffer->TotalSize = 0;

        if (true == MixerPlayer[PlayerIdx].IsPlayerObjectAttached())
        {
            CountOfAudioPlayerAttached++;

            if (true == MixerPlayer[PlayerIdx].HasPcmPlayer())
            {
                uPcmBufferFlags *PcmBufferFlags = (uPcmBufferFlags *)& OutputDataBuffer->Flags;
                OutputDataBuffer->ScatterPages_p->Page_p = MixerPlayer[PlayerIdx].GetPcmPlayerMappedSamplesArea();
                OutputDataBuffer->ScatterPages_p->Size = MixerPlayer[PlayerIdx].GetPcmPlayer()->SamplesToBytes(OnePeriodSize);
                OutputDataBuffer->ScatterPages_p->FlagsIn = 0;
                OutputDataBuffer->ScatterPages_p->BytesUsed = 0;
                OutputDataBuffer->NumberOfScatterPages = 1;
                OutputDataBuffer->TotalSize = OutputDataBuffer->ScatterPages_p->Size;
                /* Default Output buffer order is  MAIN(Analog0), AUX(Anolog1), DIGITALOUT(SPDIF), HDMIOUT(HDMI)
                   AudioFW can except any buffer order provided the Flag of the DataBuffer is set with the order of the buffer as above */
                PcmBufferFlags->bits.Type     = BUFFER_TYPE_AUDIO_IO | PlayerIdx;
                SE_VERBOSE(group_mixer, "@:%s %s OutputDataBuffer: %p ->TotalSize=%u\n", Name, MixerPlayer[PlayerIdx].GetCardName(), OutputDataBuffer, OutputDataBuffer->TotalSize);
            }
        }
    }

    return PlayerNoError;
}


void Mixer_Mme_c::SetMixerCodedInputStopCommand(bool updateRequired)
{
    for (uint32_t CodedIndex = 0; CodedIndex < MIXER_MAX_CODED_INPUTS; CodedIndex++)
    {
        uint32_t Idx(MIXER_CODED_DATA_INPUT + CodedIndex);
        MixerCommand.InputParams.InputParam[Idx].Command = MIXER_STOP;
        MixerCommand.InputParams.InputParam[Idx].StartOffset = 0;
        MixerCommand.InputParams.InputParam[Idx].PTSflags.U32 = 0;
        MixerCommand.InputParams.InputParam[Idx].PTS = 0;

        if (MIXER_CODED_DATA_INPUT_SPDIF_IDX == CodedIndex ||
            MIXER_CODED_DATA_INPUT_HDMI_IDX  == CodedIndex)
        {
            if (PrimaryCodedDataType[CodedIndex] != PcmPlayer_c::OUTPUT_IEC60958)
            {
                PrimaryCodedDataType[CodedIndex] = PcmPlayer_c::OUTPUT_IEC60958;
                // Moving from bypass to PCM mode update the PcmPlayerParameters
                if (updateRequired)
                {
                    PcmPlayerNeedsParameterUpdate = true;
                    MMENeedsParameterUpdate = true;
                    OutputTopologyUpdated   = true;
                }
            }
        }
    }
}

void Mixer_Mme_c::DumpMixerCodedInput()
{
    if (SE_IS_VERBOSE_ON(group_mixer))
    {
        SE_VERBOSE(group_mixer, "@: PrimaryCodedDataType[]: -> SPDIF:%s HDMI:%s\n",
                   PcmPlayer_c::LookupOutputEncoding(PrimaryCodedDataType[MIXER_CODED_DATA_INPUT_SPDIF_IDX]),
                   PcmPlayer_c::LookupOutputEncoding(PrimaryCodedDataType[MIXER_CODED_DATA_INPUT_HDMI_IDX]));
    }
}


void Mixer_Mme_c::FillOutFirstMixerInputBuffer(uint32_t Id, bool ForceSilentBuffer)
{
    PlayerStatus_t Status(PlayerNoError);
    SE_VERBOSE(group_mixer, ">: Client[%d].State: %s\n", Id, Mixer_c::LookupInputState(Client[Id].GetState()));

    if (((Client[Id].GetState() == STARTED) || (Client[Id].GetState() == STOPPING)) && !ForceSilentBuffer)
    {
        bool Muted = (Client[Id].IsMute() == true);
        //
        // Generate a ratio used by the manifestor to work out how many samples are required
        // for normal play and how many samples should be reserved for de-pop.
        //
        // both numbers should be divisible by 25
        // This client is correctly stated, and its parameters are well known.
        int sampling = MixerSamplingFrequency;

        if (0 == sampling)
        {
            SE_INFO(group_mixer, "I-MixerSamplingFrequency 0; forcing default\n");
            sampling = 48000; // default
        }

        Rational_c ResamplingFactor(Client[Id].GetParameters().Source.SampleRateHz, sampling, 25, false);

        // Primary client, no bypass.
        Status = Client[Id].GetManifestor()->FillOutInputBuffer(MixerGranuleSize,
                                                                ResamplingFactor,
                                                                Client[Id].GetState() == STOPPING,
                                                                FirstMixerCommand.Command.DataBuffers_p[Id],
                                                                &FirstMixerCommand.InputParams.InputParam[Id],
                                                                Muted);

        if (PlayerNoError == Status)
        {
            // All is correct, can return now.
            SE_VERBOSE(group_mixer, "<\n");
            return;
        }

    }
    else
    {
        // This client is not running or a request to force the silence buffer so fill out silence.
        // (STARTING, ...)
        FillOutSilentBuffer(FirstMixerCommand.Command.DataBuffers_p[Id],
                            &FirstMixerCommand.InputParams.InputParam[Id]);

        // Let return normally
    }

    if (PlayerNoError != Status)
    {
        SE_ERROR("Manifestor failed to populate its input buffer\n");
        // error recovery is provided by falling through and filling out a silent buffer
        FillOutSilentBuffer(FirstMixerCommand.Command.DataBuffers_p[Id],
                            &FirstMixerCommand.InputParams.InputParam[Id]);
    }

    SE_VERBOSE(group_mixer, "<\n");
}

void Mixer_Mme_c::FillOutSecondMixerInputBuffer()
{

    if (InterMixerFillBufRing->NonEmpty())
    {
        RingStatus_t err = InterMixerFillBufRing->Extract((uintptr_t *) &MixerCommand.DataBufferList[0], RING_NONE_BLOCKING);
        if (err != RingNoError)
        {
            SE_ERROR("Failed to extract from InterMixerFillBufRing (err:%d)\n", err);
            return;
        }
        SE_VERBOSE(group_mixer, "> Extract buf from InterMixerFillBufRing:%p [size:%d]\n",
                   MixerCommand.DataBufferList[0],
                   MixerCommand.DataBufferList[0]->ScatterPages_p[0].Size);
        MixerCommand.InputParams.InputParam[0].Command = MIXER_PLAY;
        MixerCommand.InputParams.InputParam[0].StartOffset = 0;

        st_relayfs_write_se(ST_RELAY_TYPE_AUDIO_INTERMIXER_PCM, ST_RELAY_SOURCE_SE,
                            (unsigned char *) MixerCommand.DataBufferList[0]->ScatterPages_p[0].Page_p,
                            MixerCommand.DataBufferList[0]->ScatterPages_p[0].Size, 0);
    }
    else
    {
        // This client is not running or a request to force the silence buffer so fill out silence.
        // (STARTING, ...)
        SE_VERBOSE(group_mixer, "> InterMixerFillBufRing is empty => FillOutSilentBuffer\n");
        FillOutSilentBuffer(MixerCommand.Command.DataBuffers_p[0],
                            &MixerCommand.InputParams.InputParam[0]);

    }
}

////////////////////////////////////////////////////////////////////////////
///
/// Populate the data buffer associated with a specific manifestor.
///
void Mixer_Mme_c::FillOutInputBuffer(uint32_t Id, bool ForceSilentBuffer)
{
    PlayerStatus_t Status(PlayerNoError);
    SE_VERBOSE(group_mixer, ">: Client[%d].State: %s\n", Id, Mixer_c::LookupInputState(Client[Id].GetState()));

    if (((Client[Id].GetState() == STARTED) || (Client[Id].GetState() == STOPPING)) && !ForceSilentBuffer)
    {
        bool Muted = (Client[Id].IsMute() == true);
        //
        // Generate a ratio used by the manifestor to work out how many samples are required
        // for normal play and how many samples should be reserved for de-pop.
        //
        // both numbers should be divisible by 25
        // This client is correctly stated, and its parameters are well known.
        int sampling = MixerSamplingFrequency;

        if (0 == sampling)
        {
            SE_INFO(group_mixer, "I-MixerSamplingFrequency 0; forcing default\n");
            sampling = 48000; // default
        }

        Rational_c ResamplingFactor(Client[Id].GetParameters().Source.SampleRateHz, sampling, 25, false);

        if (PrimaryClient == Id)
        {
            bool BypassSPDIF(false);
            bool BypassHDMI(false);
            bool BypassSDSPDIF(false);
            bool BypassSDHDMI(false);
            bool Bypassed = IsThisClientRequestedToBeBypassed(Id, BypassSPDIF, BypassHDMI, BypassSDSPDIF, BypassSDHDMI);
            SE_VERBOSE(group_mixer, "SPDIF=>Bypass:%s BypassSD:%s HDMI=>Bypass:%s BypassSD:%s\n",
                       BypassSPDIF ? "true" : "false", BypassSDSPDIF ? "true" : "false",
                       BypassHDMI  ? "true" : "false", BypassSDHDMI  ? "true" : "false");

            int ClientSamplingFrequency = Client[Id].GetParameters().Source.SampleRateHz;
            if ((Bypassed == true) && (ClientSamplingFrequency != sampling))
            {
                SE_DEBUG(group_mixer, "Bypass not possible due to mismatch in player frequency[%d] / client frequency[%d] moving back to PCM mode\n", sampling, ClientSamplingFrequency);
                Bypassed = false;
            }

            if (Bypassed)
            {
                PcmPlayer_c::OutputEncoding OutputEncodingSPDIF(PcmPlayer_c::OUTPUT_IEC60958);
                PcmPlayer_c::OutputEncoding OutputEncodingHDMI(PcmPlayer_c::OUTPUT_IEC60958);

                // Check if bypassed output should be muted
                int softmute;
                if (mSPDIFCardId < STM_SE_MIXER_NB_MAX_OUTPUTS)
                {
                    MixerPlayer[mSPDIFCardId].GetOption(STM_SE_CTRL_AUDIO_SOFTMUTE, softmute);
                    if (softmute)
                    {
                        OutputEncodingSPDIF = PcmPlayer_c::BYPASS_MUTE;
                    }
                }

                if (mHDMICardId < STM_SE_MIXER_NB_MAX_OUTPUTS)
                {
                    MixerPlayer[mHDMICardId].GetOption(STM_SE_CTRL_AUDIO_SOFTMUTE, softmute);
                    if (softmute)
                    {
                        OutputEncodingHDMI = PcmPlayer_c::BYPASS_MUTE;
                    }
                }

                // Primary client, bypass.
                Status = Client[Id].GetManifestor()->FillOutInputBuffer(MixerGranuleSize,
                                                                        ResamplingFactor,
                                                                        Client[Id].GetState() == STOPPING,
                                                                        MixerCommand.Command.DataBuffers_p[Id],
                                                                        &MixerCommand.InputParams.InputParam[Id],
                                                                        Muted,
                                                                        MixerCommand.Command.DataBuffers_p[MIXER_CODED_DATA_INPUT],
                                                                        &MixerCommand.InputParams.InputParam[MIXER_CODED_DATA_INPUT],
                                                                        BypassSPDIF,
                                                                        BypassHDMI,
                                                                        BypassSDSPDIF,
                                                                        BypassSDHDMI,
                                                                        &OutputEncodingSPDIF,
                                                                        &OutputEncodingHDMI);

                MixerCommand.Command.DataBuffers_p[Id]->Flags = BUFFER_TYPE_AUDIO_IO | Id;

                if (PlayerNoError == Status)
                {
                    if (OutputEncodingSPDIF != PrimaryCodedDataType[Mixer_Mme_c::MIXER_CODED_DATA_INPUT_SPDIF_IDX])
                    {
                        uint32_t IdxSpdif(MIXER_CODED_DATA_INPUT + MIXER_CODED_DATA_INPUT_SPDIF_IDX);
                        // Output encoding for SPDIF bypass is actually known, update this from mixer sub-system point of view.
                        PrimaryCodedDataType[MIXER_CODED_DATA_INPUT_SPDIF_IDX] = OutputEncodingSPDIF;
                        // encoding has changed, so the sample rate and the number of samples of the
                        // bypassed output may have also...
                        PcmPlayerNeedsParameterUpdate = true;
                        MMENeedsParameterUpdate = true;
                        // deliberately stop the compressed data input, since it is not configured yet as compressed data in the mixer...
                        MixerCommand.InputParams.InputParam[IdxSpdif].Command = MIXER_STOP;
                        MixerCommand.InputParams.InputParam[IdxSpdif].StartOffset = 0;
                    }

                    if (OutputEncodingHDMI != PrimaryCodedDataType[MIXER_CODED_DATA_INPUT_HDMI_IDX])
                    {
                        uint32_t IdxHdmi(MIXER_CODED_DATA_INPUT + MIXER_CODED_DATA_INPUT_HDMI_IDX);
                        // Output encoding for HDMI bypass is actually known, update this from mixer sub-system point of view.
                        PrimaryCodedDataType[MIXER_CODED_DATA_INPUT_HDMI_IDX] = OutputEncodingHDMI;
                        // encoding has changed, so the sample rate and the number of samples of the
                        // bypassed output may have also...
                        PcmPlayerNeedsParameterUpdate = true;
                        MMENeedsParameterUpdate = true;
                        // deliberately stop the compressed data input, since it is not configured yet as compressed data in the mixer...
                        MixerCommand.InputParams.InputParam[IdxHdmi].Command = MIXER_STOP;
                        MixerCommand.InputParams.InputParam[IdxHdmi].StartOffset = 0;
                    }

                    if (Client[Id].GetManifestor()->IsRunning() == false)
                    {
                        // if the client is not already being played (file playback startup)
                        // then stop also the injection of PCM data so that PCM and bypass remain synchron.
                        //
                        // when client is already playing, then the stop of the bypass corresponds
                        // to a dynamic switch of the bypass, thus the PCM output itself should be impacted
                        MixerCommand.InputParams.InputParam[Id].Command     = MIXER_STOP;
                        MixerCommand.InputParams.InputParam[Id].StartOffset = 0;
                    }

                    DumpMixerCodedInput();

                    // All is correct, can return now.
                    SE_VERBOSE(group_mixer, "<\n");
                    return;
                }
                else
                {
                    // There was an error.
                    // Stop input for coded data that correspond to the primary mixer input.
                    SetMixerCodedInputStopCommand(false);
                }
                // Let return normally: let fill out silence.
            }
            else
            {
                // Primary client, no bypass.
                Status = Client[Id].GetManifestor()->FillOutInputBuffer(MixerGranuleSize,
                                                                        ResamplingFactor,
                                                                        Client[Id].GetState() == STOPPING,
                                                                        MixerCommand.Command.DataBuffers_p[Id],
                                                                        &MixerCommand.InputParams.InputParam[Id],
                                                                        Muted,
                                                                        MixerCommand.Command.DataBuffers_p[MIXER_CODED_DATA_INPUT],
                                                                        &MixerCommand.InputParams.InputParam[MIXER_CODED_DATA_INPUT]);

                MixerCommand.Command.DataBuffers_p[Id]->Flags = BUFFER_TYPE_AUDIO_IO | Id;

                // Stop input for coded data that correspond to the primary mixer input.
                SetMixerCodedInputStopCommand(true);
                DumpMixerCodedInput();

                if (PlayerNoError == Status)
                {
                    // All is correct, can return now.
                    SE_VERBOSE(group_mixer, "<\n");
                    return;
                }

                // Let return normally: let fill out silence.
            }
        }
        else
        {
            // This client is NOT the primary one
            Status = Client[Id].GetManifestor()->FillOutInputBuffer(MixerGranuleSize,
                                                                    ResamplingFactor,
                                                                    Client[Id].GetState() == STOPPING,
                                                                    MixerCommand.Command.DataBuffers_p[Id],
                                                                    &MixerCommand.InputParams.InputParam[Id],
                                                                    Muted);
            MixerCommand.Command.DataBuffers_p[Id]->Flags = BUFFER_TYPE_AUDIO_IO | Id;

            if (PlayerNoError == Status)
            {
                // All is correct, can return now.
                SE_VERBOSE(group_mixer, "<\n");
                return;
            }
            // Let return normally: let fill out silence.
        }
    }
    else
    {
        // This client is not running or a request to force the silence buffer so fill out silence.
        // (STARTING, ...)
        FillOutSilentBuffer(MixerCommand.Command.DataBuffers_p[Id],
                            &MixerCommand.InputParams.InputParam[Id]);
        MixerCommand.Command.DataBuffers_p[Id]->Flags = BUFFER_TYPE_AUDIO_IO | Id;


        // Should stop also possible coded data being bypassed in case of the primary client.
        if (PrimaryClient == Id)
        {
            SetMixerCodedInputStopCommand(false);
            DumpMixerCodedInput();
        }

        // Let return normally
    }

    if (PlayerNoError != Status)
    {
        SE_ERROR("Manifestor failed to populate its input buffer\n");
        // error recovery is provided by falling through and filling out a silent buffer
        FillOutSilentBuffer(MixerCommand.Command.DataBuffers_p[Id],
                            &MixerCommand.InputParams.InputParam[Id]);
        MixerCommand.Command.DataBuffers_p[Id]->Flags = BUFFER_TYPE_AUDIO_IO | Id;
    }

    SE_VERBOSE(group_mixer, "<\n");
}

////////////////////////////////////////////////////////////////////////////
///
/// Populate the data buffer associated with a specific audio generator input.
///
void Mixer_Mme_c::FillOutAudioGenBuffer(uint32_t AudioGeneratorIdx)
{
    MME_DataBuffer_t *DataBuffer = MixerCommand.Command.DataBuffers_p[MIXER_AUDIOGEN_DATA_INPUT + AudioGeneratorIdx];

    if (Generator[AudioGeneratorIdx] &&
        (Generator[AudioGeneratorIdx]->GetState() == STM_SE_AUDIO_GENERATOR_STARTED))
    {
        stm_se_audio_generator_buffer_t audiogenerator_buffer;
        int sampling = MixerSamplingFrequency;

        if (0 == sampling)
        {
            SE_INFO(group_mixer, "O-MixerSamplingFrequency 0; forcing default\n");
            sampling = 48000; // default
        }

        Rational_c ResamplingFactor(Generator[AudioGeneratorIdx]->GetSamplingFrequency(), sampling, 25, false);
        Generator[AudioGeneratorIdx]->AudioGeneratorEventUnderflow(MixerGranuleSize, ResamplingFactor);
        DataBuffer->NumberOfScatterPages = 0;
        DataBuffer->TotalSize = 0;
        DataBuffer->StartOffset = 0;
        Generator[AudioGeneratorIdx]->GetCompoundOption(STM_SE_CTRL_AUDIO_GENERATOR_BUFFER,
                                                        &audiogenerator_buffer);
        unsigned char *Buffer = (unsigned char *) audiogenerator_buffer.audio_buffer;
        uint32_t BufferSize = audiogenerator_buffer.audio_buffer_size;
        uint32_t PlayPointer = Generator[AudioGeneratorIdx]->GetPlayPointer();
        DataBuffer->TotalSize = BufferSize;
        DataBuffer->ScatterPages_p[0].Page_p = Buffer + PlayPointer;
        DataBuffer->ScatterPages_p[0].Size = BufferSize - PlayPointer;
        DataBuffer->ScatterPages_p[0].FlagsIn = 0;

        if (0 == PlayPointer)
        {
            DataBuffer->NumberOfScatterPages = 1;
        }
        else
        {
            DataBuffer->ScatterPages_p[1].Page_p = Buffer;
            DataBuffer->ScatterPages_p[1].Size = PlayPointer;
            DataBuffer->ScatterPages_p[1].FlagsIn = 0;
            DataBuffer->NumberOfScatterPages = 2;
        }

        MixerCommand.InputParams.InputParam[MIXER_AUDIOGEN_DATA_INPUT + AudioGeneratorIdx].Command = MIXER_PLAY;
        MixerCommand.InputParams.InputParam[MIXER_AUDIOGEN_DATA_INPUT + AudioGeneratorIdx].StartOffset = 0;
        MixerCommand.InputParams.InputParam[MIXER_AUDIOGEN_DATA_INPUT + AudioGeneratorIdx].PTS = 0;
        MixerCommand.InputParams.InputParam[MIXER_AUDIOGEN_DATA_INPUT + AudioGeneratorIdx].PTSflags.U32 = 0;
    }
    else
    {
        FillOutSilentBuffer(DataBuffer, &MixerCommand.InputParams.InputParam[MIXER_AUDIOGEN_DATA_INPUT + AudioGeneratorIdx]);
    }

    DataBuffer->Flags = BUFFER_TYPE_AUDIO_IO | AudioGeneratorIdx;
}

////////////////////////////////////////////////////////////////////////////
///
/// Populate the data buffer associated with a specific interactive audio input.
///
void Mixer_Mme_c::FillOutInteractiveAudioBuffer(MME_DataBuffer_t *DataBuffer, int InteractiveAudioIdx)
{
    if (InteractiveAudio[InteractiveAudioIdx] &&
        ((InteractiveAudio[InteractiveAudioIdx])->GetState() == STM_SE_AUDIO_GENERATOR_STARTED))
    {
        stm_se_audio_generator_buffer_t interactiveaudio_buffer;
        DataBuffer->NumberOfScatterPages = 0;
        DataBuffer->TotalSize = 0;
        DataBuffer->StartOffset = 0;
        InteractiveAudio[InteractiveAudioIdx]->GetCompoundOption(STM_SE_CTRL_AUDIO_GENERATOR_BUFFER,
                                                                 &interactiveaudio_buffer);
        uint8_t *Buffer = static_cast<uint8_t *>(interactiveaudio_buffer.audio_buffer);
        uint32_t BufferSize = interactiveaudio_buffer.audio_buffer_size;
        uint32_t PlayPointer = InteractiveAudio[InteractiveAudioIdx]->GetPlayPointer();
        DataBuffer->TotalSize = BufferSize;
        DataBuffer->ScatterPages_p[0].Page_p = Buffer + PlayPointer;
        DataBuffer->ScatterPages_p[0].Size = BufferSize - PlayPointer;
        DataBuffer->ScatterPages_p[0].FlagsIn = 0;

        if (0 == PlayPointer)
        {
            DataBuffer->NumberOfScatterPages = 1;
        }
        else
        {
            DataBuffer->ScatterPages_p[1].Page_p = Buffer;
            DataBuffer->ScatterPages_p[1].Size = PlayPointer;
            DataBuffer->ScatterPages_p[1].FlagsIn = 0;
            DataBuffer->NumberOfScatterPages = 2;
        }

        DataBuffer->Flags = (InteractiveAudio[InteractiveAudioIdx]->GetNumChannels() == 1 ?
                             BUFFER_TYPE_IAUDIO_MONO_I :
                             BUFFER_TYPE_IAUDIO_STEREO_I);
    }
    else
    {
        FillOutSilentBuffer(DataBuffer);
        DataBuffer->Flags = BUFFER_TYPE_IAUDIO_MONO_I;
    }

    DataBuffer->Flags |= InteractiveAudioIdx;
}
////////////////////////////////////////////////////////////////////////////
///
/// Populate a data buffer such that it will be zero length (and therefore
/// silent).
///
/// If an mixer command structure is provided this will be configured to
/// stop the associated mixer output.
///
void Mixer_Mme_c::FillOutSilentBuffer(MME_DataBuffer_t *DataBuffer,
                                      tMixerFrameParams *MixerFrameParams)
{
    DataBuffer->NumberOfScatterPages = 0;
    DataBuffer->TotalSize = 0;
    DataBuffer->StartOffset = 0;
    DataBuffer->Flags = BUFFER_TYPE_AUDIO_IO;
    // zero-length, null pointer, no flags
    memset(&DataBuffer->ScatterPages_p[0], 0, sizeof(DataBuffer->ScatterPages_p[0]));

    if (MixerFrameParams)
    {
        MixerFrameParams->Command = MIXER_STOP;
        MixerFrameParams->StartOffset = 0;
        MixerFrameParams->PTS = 0;
        MixerFrameParams->PTSflags.U32 = 0;
    }
}

////////////////////////////////////////////////////////////////////////////
///
///
///
PlayerStatus_t Mixer_Mme_c::UpdateFirstMixerOutputBuffers()
{
    unsigned int OutBufIdx = FirstMixerCommand.Command.NumberInputBuffers;
    RingStatus_t rStatus;

    MME_DataBuffer_t *MMEBuf = FirstMixerCommand.DataBufferList[OutBufIdx++];
    SE_VERBOSE(group_mixer, ">%s insert MMEBuf:%p in InterMixerFillBufRing\n", Name, MMEBuf);

    MMEBuf->ScatterPages_p[0].Size = MMEBuf->ScatterPages_p[0].BytesUsed;
    MMEBuf->ScatterPages_p[0].BytesUsed = 0;

    rStatus = InterMixerFillBufRing->Insert((uintptr_t) MMEBuf);
    if (rStatus != RingNoError)
    {
        SE_ERROR("Cannot insert MMEBuf:%p in InterMixerFillBufRing\n", MMEBuf);
        return PlayerError;
    }
    return PlayerNoError;
}

////////////////////////////////////////////////////////////////////////////
///
///
///
PlayerStatus_t Mixer_Mme_c::UpdateOutputBuffers()
{
    PlayerStatus_t Status;
    SE_VERBOSE(group_mixer, ">%s\n", Name);
    Status = CommitPcmPlayersMappedSamples();

    if (PlayerNoError != Status)
    {
        SE_ERROR("Failed to commit\n");
        return Status;
    }

    Status = GetAndUpdateDisplayTimeOfNextCommit();

    if (PlayerNoError != Status)
    {
        SE_ERROR("Failed to update time of next commit\n");
        return Status;
    }

    for (uint32_t CountOfAudioPlayerAttached(0), PlayerIdx(0); CountOfAudioPlayerAttached < NumberOfAudioPlayerAttached; PlayerIdx++)
    {
        if (true == MixerPlayer[PlayerIdx].IsPlayerObjectAttached())
        {
            CountOfAudioPlayerAttached++;
            MME_DataBuffer_t *OutputDataBuffer = MixerCommand.Command.DataBuffers_p[MixerCommand.Command.NumberInputBuffers + PlayerIdx];
            OutputDataBuffer->TotalSize = 0;
            SE_VERBOSE(group_mixer, "@: %s OutputDataBuffer: %p ->TotalSize=%u\n", MixerPlayer[PlayerIdx].GetCardName(), OutputDataBuffer, OutputDataBuffer->TotalSize);
        }
    }

    return PlayerNoError;
}


////////////////////////////////////////////////////////////////////////////
///
/// Update input buffer for all Clients.
/// Set Client's ReleaseProcessingDecodedBuffer when requested.
///
void Mixer_Mme_c::UpdateInputBuffer(uint32_t Id, bool ReleaseProcessingBuffer)
{
    MME_DataBuffer_t       *PcmBuffer, *CodedBuffer;
    MME_MixerInputStatus_t *PcmStatus, *CodedStatus;

    Client[Id].LockTake();
    if (MMEFirstMixerHdl == MME_INVALID_ARGUMENT)
    {
        PcmBuffer   =  MixerCommand.Command.DataBuffers_p[Id];
        CodedBuffer =  MixerCommand.Command.DataBuffers_p[MIXER_CODED_DATA_INPUT];
        PcmStatus   = &MixerCommand.OutputParams.MixStatus.InputStreamStatus[Id];
        CodedStatus = &MixerCommand.OutputParams.MixStatus.InputStreamStatus[MIXER_CODED_DATA_INPUT];
    }
    else
    {
        PcmBuffer   =  FirstMixerCommand.Command.DataBuffers_p[Id];
        CodedBuffer =  FirstMixerCommand.Command.DataBuffers_p[MIXER_CODED_DATA_INPUT];
        PcmStatus   = &FirstMixerCommand.OutputParams.MixStatus.InputStreamStatus[Id];
        CodedStatus = &FirstMixerCommand.OutputParams.MixStatus.InputStreamStatus[MIXER_CODED_DATA_INPUT];
    }

    if ((Client[Id].GetState() == STARTED) || (Client[Id].GetState() == STOPPING))
    {
        PlayerStatus_t Status;

        Status = Client[Id].GetManifestor()->UpdateInputBuffer(PcmBuffer, PcmStatus, CodedBuffer, CodedStatus, ReleaseProcessingBuffer);

        if (Status != ManifestorNoError && Status != ManifestorNullQueued)
        {
            SE_ERROR("Failed to update the input buffer\n");
            // no real error recovery possible
        }
    }

    Client[Id].LockRelease();
}

////////////////////////////////////////////////////////////////////////////
///
///
///
void Mixer_Mme_c::UpdateSecondMixerInputBuffer()
{
    MME_DataBuffer_t *MMEBuf = MixerCommand.DataBufferList[0];
    if (MMEBuf->NumberOfScatterPages != 0)
    {
        RingStatus_t err = InterMixerFreeBufRing->Insert((uintptr_t) MixerCommand.DataBufferList[0]);
        if (err != RingNoError)
        {
            SE_ERROR("Failed to insert into InterMixerFreeBufRing (err:%d)\n", err);
            return;
        }
    }
}

////////////////////////////////////////////////////////////////////////////
///
///
///
void Mixer_Mme_c::UpdateAudioGenBuffer(uint32_t AudioGenId)
{
    if (Generator[AudioGenId] &&
        (Generator[AudioGenId]->GetState() == STM_SE_AUDIO_GENERATOR_STARTED))
    {
        uint32_t BytesConsumed;
        BytesConsumed = MixerCommand.OutputParams.MixStatus.InputStreamStatus[MIXER_AUDIOGEN_DATA_INPUT + AudioGenId].BytesUsed;
        Generator[AudioGenId]->SamplesConsumed(BytesConsumed / Generator[AudioGenId]->GetBytesPerSample());
    }
}

////////////////////////////////////////////////////////////////////////////
///
///
///
void Mixer_Mme_c::UpdateInteractiveAudioBuffer(uint32_t InteractiveAudioId)
{
    if (InteractiveAudio[InteractiveAudioId] &&
        (InteractiveAudio[InteractiveAudioId]->GetState() == STM_SE_AUDIO_GENERATOR_STARTED))
    {
        uint32_t SamplesConsumed;
        SamplesConsumed = MixerCommand.OutputParams.MixStatus.InputStreamStatus[MIXER_INTERACTIVE_INPUT].BytesUsed / (8 * 4);
        InteractiveAudio[InteractiveAudioId]->SamplesConsumed(SamplesConsumed);
    }
}

////////////////////////////////////////////////////////////////////////////
///
/// Check for any existing mixing metadata from the secondary stream, and
/// eventually update them
///
void Mixer_Mme_c::UpdateMixingMetadata()
{
    // Client Lock already taken by caller.
    if (Client[PrimaryClient].ManagedByThread() && Client[SecondaryClient].ManagedByThread())
    {
        // These clients are correctly stated, and their parameters are well known.
        // Read the code carefully here. We use lots of x = y = z statements.
        ParsedAudioParameters_t SecondaryClientAudioParameters = Client[SecondaryClient].GetParameters();
        int PrimaryClientAudioMode = Client[PrimaryClient].GetParameters().Organisation;

        if ((SecondaryClientAudioParameters.MixingMetadata.IsMixingMetadataPresent) &&
            (MixerSettings.OutputConfiguration.metadata_update != SND_PSEUDO_MIXER_METADATA_UPDATE_NEVER))
        {
            MixingMetadata_t *StreamMetadata = &SecondaryClientAudioParameters.MixingMetadata;
            SE_DEBUG(group_mixer, "Mixing metadata found (mix out configs: %d)!\n", StreamMetadata->NbOutMixConfig);
            // It is impossible for UpstreamConfiguration to be non-NULL. This is so because the reset value
            // of OutputConfiguration.metadata_update makes this code unreachable. The only means by which we
            // could end up executing this branch would also set UpstreamConfiguration to a non-NULL value.
            SE_ASSERT(UpstreamConfiguration);

            if (StreamMetadata->ADPESMetaData.ADInfoAvailable)
            {
                unsigned short  PanCoeff[8];
                unsigned char Panbyte = SecondaryClientAudioParameters.MixingMetadata.ADPESMetaData.ADPanValue;
                memset(PanCoeff, 0 , sizeof(PanCoeff));
                StreamMetadata->MixOutConfig[0].AudioMode = PrimaryClientAudioMode;
                StreamMetadata->NbOutMixConfig = 1;
                enum eAccAcMode AudioMode = (enum eAccAcMode)PrimaryClientAudioMode;

                switch (AudioMode)
                {
                case ACC_MODE20t:
                case ACC_MODE20:
                    Gen2chPanCoef(Panbyte, PanCoeff);
                    //Applying Fade
                    StreamMetadata->MixOutConfig[0].PrimaryAudioGain[ACC_MAIN_LEFT] = \
                                                                                      StreamMetadata->MixOutConfig[0].PrimaryAudioGain[ACC_MAIN_RGHT] = \
                                                                                              Fade_Mapping[(SecondaryClientAudioParameters.MixingMetadata.ADPESMetaData.ADFadeValue) & 0xFF];
                    //Applying pan
                    StreamMetadata->MixOutConfig[0].SecondaryAudioPanCoeff[ACC_MAIN_LEFT] = PanCoeff[ACC_MAIN_LEFT];
                    StreamMetadata->MixOutConfig[0].SecondaryAudioPanCoeff[ACC_MAIN_RGHT] = PanCoeff[ACC_MAIN_RGHT];
                    break;

                case  ACC_MODE30:
                    Gen4chPanCoef(Panbyte, PanCoeff);
                    //Applying Fade
                    StreamMetadata->MixOutConfig[0].PrimaryAudioGain[ACC_MAIN_LEFT] = \
                                                                                      StreamMetadata->MixOutConfig[0].PrimaryAudioGain[ACC_MAIN_RGHT] = \
                                                                                              StreamMetadata->MixOutConfig[0].PrimaryAudioGain[ACC_MAIN_CNTR] = \
                                                                                                      Fade_Mapping[(SecondaryClientAudioParameters.MixingMetadata.ADPESMetaData.ADFadeValue) & 0xFF];
                    //Applying pan
                    StreamMetadata->MixOutConfig[0].SecondaryAudioPanCoeff[ACC_MAIN_LEFT] = PanCoeff[ACC_MAIN_LEFT];
                    StreamMetadata->MixOutConfig[0].SecondaryAudioPanCoeff[ACC_MAIN_RGHT] = PanCoeff[ACC_MAIN_RGHT];
                    StreamMetadata->MixOutConfig[0].SecondaryAudioPanCoeff[ACC_MAIN_CNTR] = PanCoeff[ACC_MAIN_CNTR];
                    break;

                case  ACC_MODE32_LFE:
                    Gen6chPanCoef(Panbyte, PanCoeff);
                    //Applying Fade
                    StreamMetadata->MixOutConfig[0].PrimaryAudioGain[ACC_MAIN_LEFT] = \
                                                                                      StreamMetadata->MixOutConfig[0].PrimaryAudioGain[ACC_MAIN_RGHT] = \
                                                                                              StreamMetadata->MixOutConfig[0].PrimaryAudioGain[ACC_MAIN_CNTR] = \
                                                                                                      StreamMetadata->MixOutConfig[0].PrimaryAudioGain[ACC_MAIN_LSUR] = \
                                                                                                              StreamMetadata->MixOutConfig[0].PrimaryAudioGain[ACC_MAIN_RSUR] = \
                                                                                                                      Fade_Mapping[(SecondaryClientAudioParameters.MixingMetadata.ADPESMetaData.ADFadeValue) & 0xFF];
                    //Applying pan
                    StreamMetadata->MixOutConfig[0].SecondaryAudioPanCoeff[ACC_MAIN_LEFT] = PanCoeff[ACC_MAIN_LEFT];
                    StreamMetadata->MixOutConfig[0].SecondaryAudioPanCoeff[ACC_MAIN_RGHT] = PanCoeff[ACC_MAIN_RGHT];
                    StreamMetadata->MixOutConfig[0].SecondaryAudioPanCoeff[ACC_MAIN_CNTR] = PanCoeff[ACC_MAIN_CNTR];
                    StreamMetadata->MixOutConfig[0].SecondaryAudioPanCoeff[ACC_MAIN_LSUR] = PanCoeff[ACC_MAIN_LSUR];
                    StreamMetadata->MixOutConfig[0].SecondaryAudioPanCoeff[ACC_MAIN_RSUR] = PanCoeff[ACC_MAIN_RSUR];
                    break;

                default:
                    SE_INFO(group_mixer, "Audio Mode %u not Supported\n", AudioMode);
                    break;
                }
            }

            SE_DEBUG(group_mixer, "WARNING - Selecting first PrimaryAudioGain (which may not be correct)\n");

            for (uint32_t ChannelIdx(0); ChannelIdx < SND_PSEUDO_MIXER_CHANNELS; ChannelIdx++)
            {
                MixerSettings.MixerOptions.Gain[PrimaryClient][ChannelIdx] = StreamMetadata->MixOutConfig[0].PrimaryAudioGain[ChannelIdx];
                // Update the flag
                MixerSettings.MixerOptions.GainUpdated = true;
            }

            // To pan the secondary stream it is necessary that this stream is mono....
            if (SecondaryClientAudioParameters.Organisation == ACC_MODE10)
            {
                for (uint32_t MixConfig(0); MixConfig < StreamMetadata->NbOutMixConfig; MixConfig++)
                {
                    // check if this mix config is the same as the primary stream one,
                    // if so, the secondary panning coef metadata will be applied
                    if (StreamMetadata->MixOutConfig[MixConfig].AudioMode == PrimaryClientAudioMode)
                    {
                        for (uint32_t ChannelIdx(0); ChannelIdx < SND_PSEUDO_MIXER_CHANNELS; ChannelIdx++)
                        {
                            MixerSettings.MixerOptions.Gain[SecondaryClient][ChannelIdx] = Q3_13_UNITY;
                            MixerSettings.MixerOptions.Pan[SecondaryClient][ChannelIdx] = StreamMetadata->MixOutConfig[MixConfig].SecondaryAudioPanCoeff[ChannelIdx];
                            // Update the flag
                            MixerSettings.MixerOptions.GainUpdated = true;
                            MixerSettings.MixerOptions.PanUpdated = true;
                        }
                    }
                }
            }

            if (MixerSettings.OutputConfiguration.metadata_update == SND_PSEUDO_MIXER_METADATA_UPDATE_ALWAYS)
            {
                MixerSettings.MixerOptions.PostMixGain = StreamMetadata->PostMixGain;
                // Update the flag
                MixerSettings.MixerOptions.PostMixGainUpdated = true;
            }
        }
    }
}

////////////////////////////////////////////////////////////////////////////
///
/// Take a continuous sampling frequency and identify the eFsRange
/// for that frequency.
///
enum eFsRange Mixer_Mme_c::TranslateIntegerSamplingFrequencyToRange(uint32_t IntegerFrequency)
{
    if (IntegerFrequency < 64000)
    {
        if (IntegerFrequency >= 32000)
        {
            return ACC_FSRANGE_48k;
        }

        if (IntegerFrequency >= 16000)
        {
            return ACC_FSRANGE_24k;
        }

        return ACC_FSRANGE_12k;
    }

    if (IntegerFrequency < 128000)
    {
        return ACC_FSRANGE_96k;
    }

    if (IntegerFrequency < 256000)
    {
        return ACC_FSRANGE_192k;
    }

    return ACC_FSRANGE_384k;
}

////////////////////////////////////////////////////////////////////////////
///
/// Lookup a discrete sampling frequency and convert it to a string.
///
const char *Mixer_Mme_c::LookupDiscreteSamplingFrequency(enum eAccFsCode DiscreteFrequency)
{
    switch (DiscreteFrequency)
    {
#define E(x) case x: return #x
        E(ACC_FS48k);
        E(ACC_FS44k);
        E(ACC_FS32k);
        E(ACC_FS_reserved_48K);
        /* Range : 2^1  */
        E(ACC_FS96k);
        E(ACC_FS88k);
        E(ACC_FS64k);
        E(ACC_FS_reserved_96K);
        /* Range : 2^2  */
        E(ACC_FS192k);
        E(ACC_FS176k);
        E(ACC_FS128k);
        E(ACC_FS_reserved_192K);
        /* Range : 2^3 */
        E(ACC_FS384k);
        E(ACC_FS352k);
        E(ACC_FS256k);
        E(ACC_FS_reserved_384K);
        /* Range : 2^-2 */
        E(ACC_FS12k);
        E(ACC_FS11k);
        E(ACC_FS8k);
        E(ACC_FS_reserved_12K);
        /* Range : 2^-1 */
        E(ACC_FS24k);
        E(ACC_FS22k);
        E(ACC_FS16k);
        E(ACC_FS_reserved_24K);
        /* Range : 2^-1 */
        E(ACC_FS768k);
        E(ACC_FS705k);
        E(ACC_FS512k);
        E(ACC_FS_reserved_768K);
        /* Range : 2^-3 */
        E(ACC_FS6k);
        E(ACC_FS5k);
        E(ACC_FS4k);
        E(ACC_FS_reserved_6K);
        E(ACC_FS_reserved);  // Undefined
        E(ACC_FS_ID);        // Used by Mixer : if FS_ID then OutSFreq = InSFreq
#undef E

    default:

        /* AUDIOMIXER_OVERRIDE_OUTFS is not a component of enum eAccFsCode */
        if (DiscreteFrequency == AUDIOMIXER_OVERRIDE_OUTFS)
        {
            return "AUDIOMIXER_OVERRIDE_OUTFS";
        }

        return "INVALID";
    }
}

// For verbosity in case of debug.
const char *Mixer_Mme_c::LookupPolicyValueAudioApplicationType(int ApplicationType)
{
    switch (ApplicationType)
    {
#define E(x) case x: return #x
        E(PolicyValueAudioApplicationIso);
        E(PolicyValueAudioApplicationDvd);
        E(PolicyValueAudioApplicationDvb);
        E(PolicyValueAudioApplicationMS10);
        E(PolicyValueAudioApplicationMS11);
        E(PolicyValueAudioApplicationMS12);
        E(PolicyValueAudioApplicationHDMIRxGameMode);
#undef E

    default:
        return "INVALID";
    }
}

// For verbosity in case of debug.
const char *Mixer_Mme_c::LookupAudioOriginalEncoding(AudioOriginalEncoding_t AudioOriginalEncoding)
{
    switch (AudioOriginalEncoding)
    {
#define E(x) case x: return #x
        E(AudioOriginalEncodingUnknown);
        E(AudioOriginalEncodingAc3);
        E(AudioOriginalEncodingDdplus);
        E(AudioOriginalEncodingDts);
        E(AudioOriginalEncodingDtshd);
        E(AudioOriginalEncodingDtshdMA);
        E(AudioOriginalEncodingDtshdLBR);
        E(AudioOriginalEncodingTrueHD);
        E(AudioOriginalEncodingAAC);
        E(AudioOriginalEncodingHEAAC_960);
        E(AudioOriginalEncodingHEAAC_1024);
        E(AudioOriginalEncodingHEAAC_1920);
        E(AudioOriginalEncodingHEAAC_2048);
        E(AudioOriginalEncodingDPulse);
        E(AudioOriginalEncodingSPDIFIn_Compressed);
        E(AudioOriginalEncodingSPDIFIn_Pcm);
        E(AudioOriginalEncodingMax);
#undef E

    default:
        return "INVALID";
    }
}


////////////////////////////////////////////////////////////////////////////
///
/// Lookup a discrete sampling frequency range and convert it to a string.
///
const char *Mixer_Mme_c::LookupDiscreteSamplingFrequencyRange(enum eFsRange DiscreteRange)
{
    switch (DiscreteRange)
    {
#define E(x) case x: return #x
        E(ACC_FSRANGE_12k);
        E(ACC_FSRANGE_24k);
        E(ACC_FSRANGE_48k);
        E(ACC_FSRANGE_96k);
        E(ACC_FSRANGE_192k);
        E(ACC_FSRANGE_384k);
#undef E

    default:
        return "INVALID";
    }
}

uint32_t Mixer_Mme_c::LookupSpdifPreamblePc(PcmPlayer_c::OutputEncoding Encoding)
{
#if 0
    Value   Corresponding frame
    0     NULL data
    1     Ac3(1536 samples)
    3     Pause burst
    4     MPEG - 1 layer - 1
    5     MPEG - 1 layer - 2 or - 3 data or MPEG - 2 without extension
    6     MPEG - 2 data with extension
    7     MPEG - 2 AAC
    8     MPEG - 2, layer - 1 low sampling frequency
    9     MPEG - 2, layer - 2 low sampling frequency
    10     MPEG - 2, layer - 3 low sampling frequency
    11     DTS type I(512 samples)
    12     DTS type II(1024 samples)
    13     DTS type III(2048 samples)
    14     ATRAC(512 samples)
    15     ATRAC 2 / 3(1 024 samples)
    17     DTS Type 4
    21     DDPLUS
    22     Dolby TrueHD
    23     AAC / HE - AAC
#endif

    switch (Encoding)
    {
    case PcmPlayer_c::OUTPUT_AC3:
    case PcmPlayer_c::BYPASS_AC3:
        return 1;

    case PcmPlayer_c::BYPASS_AAC:
        return 7;

    case PcmPlayer_c::OUTPUT_DTS:
    case PcmPlayer_c::BYPASS_DTS_512:
        return 11;

    case PcmPlayer_c::BYPASS_DTS_1024:
        return 12;

    case PcmPlayer_c::BYPASS_DTS_2048:
        return 13;

    case PcmPlayer_c::BYPASS_DTSHD_HR:
        return (17 + MIXER_DTSHD_PC_REP_PERIOD_2048);

    case PcmPlayer_c::BYPASS_DTSHD_LBR:
    case PcmPlayer_c::BYPASS_DTSHD_DTS_4096:
        return (17 + MIXER_DTSHD_PC_REP_PERIOD_4096);

    case PcmPlayer_c::BYPASS_DTSHD_MA:
    case PcmPlayer_c::BYPASS_DTSHD_DTS_8192:
        return (17 + MIXER_DTSHD_PC_REP_PERIOD_8192);

    case PcmPlayer_c::BYPASS_DDPLUS:
        return 21;

    case PcmPlayer_c::BYPASS_TRUEHD:
        return 22;

    case PcmPlayer_c::BYPASS_HEAAC_960:
        return (23 + MIXER_HEAAC_PC_REP_PERIOD_960);

    case PcmPlayer_c::BYPASS_HEAAC_1024:
        return (23 + MIXER_HEAAC_PC_REP_PERIOD_1024);

    case PcmPlayer_c::BYPASS_HEAAC_1920:
        return (23 + MIXER_HEAAC_PC_REP_PERIOD_1920);

    case PcmPlayer_c::BYPASS_HEAAC_2048:
        return (23 + MIXER_HEAAC_PC_REP_PERIOD_2048);

    default:
        return 0;
    }
}

uint32_t Mixer_Mme_c::LookupRepetitionPeriod(PcmPlayer_c::OutputEncoding Encoding)
{
    switch (Encoding)
    {
    case PcmPlayer_c::OUTPUT_AC3:
    case PcmPlayer_c::BYPASS_AC3:
        return 1536;

    case PcmPlayer_c::OUTPUT_DTS:
    case PcmPlayer_c::BYPASS_DTS_512:
        return 512;

    case PcmPlayer_c::BYPASS_DTS_1024:
        return 1024;

    case PcmPlayer_c::BYPASS_DTS_2048:
        return 2048;

    case PcmPlayer_c::BYPASS_DTSHD_HR:
        return 2048;

    case PcmPlayer_c::BYPASS_DTSHD_LBR:
    case PcmPlayer_c::BYPASS_DTSHD_DTS_4096:
        return 4096;

    case PcmPlayer_c::BYPASS_DTSHD_MA:
    case PcmPlayer_c::BYPASS_DTSHD_DTS_8192:
        return 8192;

    case PcmPlayer_c::BYPASS_DDPLUS:
        return 1536 * 4;

    case PcmPlayer_c::BYPASS_TRUEHD:
        return 10 * 1536;

    case PcmPlayer_c::BYPASS_AAC:
        return 1024;

    case PcmPlayer_c::BYPASS_HEAAC_960:
        return 960;

    case PcmPlayer_c::BYPASS_HEAAC_1024:
        return 1024;

    case PcmPlayer_c::BYPASS_HEAAC_1920:
        return 1920;

    case PcmPlayer_c::BYPASS_HEAAC_2048:
        return 2048;

    default:
        return 1;
    }
}

uint32_t Mixer_Mme_c::LookupIec60958FrameRate(PcmPlayer_c::OutputEncoding Encoding) const
{
    ParsedAudioParameters_t PrimaryClientAudioParameters;
    AudioOriginalEncoding_t OriginalEncoding(AudioOriginalEncodingUnknown);
    uint32_t OriginalSamplingFreq;
    uint32_t SpdifInSamplingFrequency;

    // Client Lock already taken by caller.
    if (Client[PrimaryClient].ManagedByThread())
    {
        // This client is correctly stated, and its parameters are well known.
        PrimaryClientAudioParameters = Client[PrimaryClient].GetParameters();
        OriginalEncoding = PrimaryClientAudioParameters.OriginalEncoding;
        OriginalSamplingFreq = PrimaryClientAudioParameters.BackwardCompatibleProperties.SampleRateHz;
        SpdifInSamplingFrequency  = PrimaryClientAudioParameters.SpdifInProperties.SamplingFrequency;

        if (0 == OriginalSamplingFreq)
        {
            OriginalSamplingFreq = PrimaryClientAudioParameters.Source.SampleRateHz;
        }
    }
    else
    {
        SE_DEBUG(group_mixer, "<%s Forcing 48000\n", Name);
        // The client is de-registered, so return arbitrary value.
        return 48000;
    }

    SE_DEBUG(group_mixer, "@%s %s %u %s\n",
             Name,
             LookupAudioOriginalEncoding(OriginalEncoding),
             OriginalSamplingFreq,
             PcmPlayer_c::LookupOutputEncoding(Encoding));
    uint32_t Iec60958FrameRate;

    if ((Encoding == PcmPlayer_c::BYPASS_SPDIFIN_COMPRESSED) || (Encoding == PcmPlayer_c::BYPASS_SPDIFIN_PCM))
    {
        SE_DEBUG(group_mixer, "SPDIFin SamplingFrequency %u\n", SpdifInSamplingFrequency);
        Iec60958FrameRate = SpdifInSamplingFrequency;
    }
    else if (Encoding == PcmPlayer_c::BYPASS_DDPLUS)
    {
        Iec60958FrameRate = OriginalSamplingFreq * 4;
    }
    else if ((PcmPlayer_c::BYPASS_AC3 == Encoding) && (AudioOriginalEncodingDPulse == OriginalEncoding))
    {
        // bug29797: in case of DolbyPulse transcoded as AC3 is 48 kHz.
        // Take into account what was forced by STM_SE_FIXED_OUTPUT_FREQUENCY control
        SE_INFO(group_mixer, "%s %s FORCING Iec60958FrameRate to %d\n", Name, LookupAudioOriginalEncoding(OriginalEncoding), NominalOutputSamplingFrequency);
        Iec60958FrameRate = NominalOutputSamplingFrequency;
    }
    else if (Encoding <= PcmPlayer_c::BYPASS_DTSHD_LBR)   // C.FENARD: To be reworked cause can have some side effects ...
    {
        Iec60958FrameRate = OriginalSamplingFreq;
    }
    else if (Encoding <= PcmPlayer_c::BYPASS_DTSHD_MA)  // C.FENARD: To be reworked cause can have some side effects ...
    {
        Iec60958FrameRate = (PrimaryClientAudioParameters.Source.SampleRateHz * LookupRepetitionPeriod(Encoding)) / \
                            PrimaryClientAudioParameters.SampleCount;
    }
    else if (Encoding == PcmPlayer_c::BYPASS_TRUEHD)
    {
        Iec60958FrameRate = 4 * 192000;
    }
    else
    {
        Iec60958FrameRate = OriginalSamplingFreq;
    }

    SE_DEBUG(group_mixer, "<%s %d\n", Name, Iec60958FrameRate);
    return Iec60958FrameRate;
}

////////////////////////////////////////////////////////////////////////////
///
/// Simple lookup table used to convert between struct
/// ::stm_se_audio_channel_assignment and enum eAccAcMode.
///
static const struct
{
    enum eAccAcMode AccAcMode;
    const char *Text; // sneaky pre-processor trick used to convert the enumerations to textual equivalents
    struct stm_se_audio_channel_assignment ChannelAssignment;
    bool SuitableForDirectOutput;
}
ChannelAssignmentLookupTable[] =
{
#define E(mode, p0, p1, p2, p3) { mode, #mode, { STM_SE_AUDIO_CHANNEL_PAIR_ ## p0, \
    STM_SE_AUDIO_CHANNEL_PAIR_ ## p1, \
    STM_SE_AUDIO_CHANNEL_PAIR_ ## p2, \
    STM_SE_AUDIO_CHANNEL_PAIR_ ## p3, \
    STM_SE_AUDIO_CHANNEL_PAIR_NOT_CONNECTED }, true }
    // as E but mark the output unsuitable for direct output
#define XXX(mode, p0, p1, p2, p3) { mode, #mode, { STM_SE_AUDIO_CHANNEL_PAIR_ ## p0, \
    STM_SE_AUDIO_CHANNEL_PAIR_ ## p1, \
    STM_SE_AUDIO_CHANNEL_PAIR_ ## p2, \
    STM_SE_AUDIO_CHANNEL_PAIR_ ## p3, \
    STM_SE_AUDIO_CHANNEL_PAIR_NOT_CONNECTED }, false }
    // as XXX but for a different reason: there is already a better candidate for direct output, not a bug
#define DUPE(mode, p0, p1, p2, p3) XXX(mode, p0, p1, p2, p3)

    // Weird modes ;-)
    E(ACC_MODE10,           NOT_CONNECTED,  CNTR_0,     NOT_CONNECTED,  NOT_CONNECTED),     // Mad
    E(ACC_MODE20t,          LT_RT,      NOT_CONNECTED,  NOT_CONNECTED,  NOT_CONNECTED),
    //XXX( ACC_MODE53,              ...                                                             ), // Not 8ch
    //XXX( ACC_MODE53_LFE,          ...                                                             ), // Not 8ch

    // CEA-861 (A to D) modes (in numerical order)
    E(ACC_MODE20,           L_R,        NOT_CONNECTED,  NOT_CONNECTED,  NOT_CONNECTED),
    DUPE(ACC_HDMI_MODE20,         L_R,            NOT_CONNECTED,  NOT_CONNECTED,  NOT_CONNECTED),
    E(ACC_MODE20_LFE,           L_R,        0_LFE1,         NOT_CONNECTED,  NOT_CONNECTED),
    DUPE(ACC_HDMI_MODE20_LFE,       L_R,        0_LFE1,         NOT_CONNECTED,  NOT_CONNECTED),
    E(ACC_MODE30,               L_R,        CNTR_0,         NOT_CONNECTED,  NOT_CONNECTED),
    DUPE(ACC_HDMI_MODE30,           L_R,        CNTR_0,         NOT_CONNECTED,  NOT_CONNECTED),
    E(ACC_MODE30_LFE,           L_R,        CNTR_LFE1,      NOT_CONNECTED,  NOT_CONNECTED),
    DUPE(ACC_HDMI_MODE30_LFE,       L_R,        CNTR_LFE1,      NOT_CONNECTED,  NOT_CONNECTED),
    E(ACC_MODE21,           L_R,        NOT_CONNECTED,  CSURR_0,    NOT_CONNECTED),
    DUPE(ACC_HDMI_MODE21,           L_R,        NOT_CONNECTED,  CSURR_0,    NOT_CONNECTED),
    E(ACC_MODE21_LFE,           L_R,        0_LFE1,     CSURR_0,    NOT_CONNECTED),
    DUPE(ACC_HDMI_MODE21_LFE,       L_R,        0_LFE1,     CSURR_0,    NOT_CONNECTED),
    E(ACC_MODE31,               L_R,        CNTR_0,         CSURR_0,        NOT_CONNECTED),
    DUPE(ACC_HDMI_MODE31,           L_R,        CNTR_0,         CSURR_0,        NOT_CONNECTED),
    E(ACC_MODE31_LFE,           L_R,        CNTR_LFE1,      CSURR_0,        NOT_CONNECTED),
    DUPE(ACC_HDMI_MODE31_LFE,       L_R,        CNTR_LFE1,      CSURR_0,        NOT_CONNECTED),
    E(ACC_MODE22,           L_R,        NOT_CONNECTED,  LSUR_RSUR,  NOT_CONNECTED),
    DUPE(ACC_HDMI_MODE22,           L_R,        NOT_CONNECTED,  LSUR_RSUR,  NOT_CONNECTED),
    E(ACC_MODE22_LFE,           L_R,        0_LFE1,     LSUR_RSUR,  NOT_CONNECTED),
    DUPE(ACC_HDMI_MODE22_LFE,       L_R,        0_LFE1,     LSUR_RSUR,  NOT_CONNECTED),
    E(ACC_MODE32,           L_R,        CNTR_0,     LSUR_RSUR,  NOT_CONNECTED),
    DUPE(ACC_HDMI_MODE32,           L_R,        CNTR_0,     LSUR_RSUR,  NOT_CONNECTED),
    E(ACC_MODE32_LFE,           L_R,        CNTR_LFE1,  LSUR_RSUR,  NOT_CONNECTED),
    DUPE(ACC_HDMI_MODE32_LFE,       L_R,        CNTR_LFE1,  LSUR_RSUR,  NOT_CONNECTED),
    XXX(ACC_MODE23,         L_R,        NOT_CONNECTED,  LSUR_RSUR,  CSURR_0),       // Bug 4518
    DUPE(ACC_HDMI_MODE23,           L_R,        NOT_CONNECTED,  LSUR_RSUR,  CSURR_0),
    XXX(ACC_MODE23_LFE,         L_R,        0_LFE1,     LSUR_RSUR,  CSURR_0),       // Bug 4518
    DUPE(ACC_HDMI_MODE23_LFE,       L_R,        0_LFE1,     LSUR_RSUR,  CSURR_0),
    XXX(ACC_MODE33,         L_R,        CNTR_0,     LSUR_RSUR,  CSURR_0),       // Bug 4518
    DUPE(ACC_HDMI_MODE33,           L_R,        CNTR_0,     LSUR_RSUR,  CSURR_0),
    XXX(ACC_MODE33_LFE,         L_R,        CNTR_LFE1,  LSUR_RSUR,  CSURR_0),       // Bug 4518
    DUPE(ACC_HDMI_MODE33_LFE,       L_R,        CNTR_LFE1,  LSUR_RSUR,  CSURR_0),
    XXX(ACC_MODE24,         L_R,        NOT_CONNECTED,  LSUR_RSUR,  LSURREAR_RSURREAR),  //Bug 4518
    DUPE(ACC_HDMI_MODE24,           L_R,        NOT_CONNECTED,  LSUR_RSUR,  LSURREAR_RSURREAR),
    XXX(ACC_MODE24_LFE,         L_R,        0_LFE1,     LSUR_RSUR,  LSURREAR_RSURREAR),  //Bug 4518
    DUPE(ACC_HDMI_MODE24_LFE,   L_R,        0_LFE1,     LSUR_RSUR,  LSURREAR_RSURREAR),
    E(ACC_MODE34,           L_R,        CNTR_0,     LSUR_RSUR,  LSURREAR_RSURREAR),
    DUPE(ACC_HDMI_MODE34,           L_R,        CNTR_0,     LSUR_RSUR,  LSURREAR_RSURREAR),
    E(ACC_MODE34_LFE,           L_R,        CNTR_LFE1,  LSUR_RSUR,  LSURREAR_RSURREAR),
    DUPE(ACC_HDMI_MODE34_LFE,       L_R,        CNTR_LFE1,  LSUR_RSUR,  LSURREAR_RSURREAR),
    XXX(ACC_HDMI_MODE40,            L_R,        NOT_CONNECTED,  NOT_CONNECTED,  CNTRL_CNTRR),   // Bug 4518
    XXX(ACC_HDMI_MODE40_LFE,    L_R,        0_LFE1,     NOT_CONNECTED,  CNTRL_CNTRR),   // Bug 4518
    XXX(ACC_HDMI_MODE50,            L_R,        CNTR_0,     NOT_CONNECTED,  CNTRL_CNTRR),   // Bug 4518
    XXX(ACC_HDMI_MODE50_LFE,    L_R,        CNTR_LFE1,  NOT_CONNECTED,  CNTRL_CNTRR),   // Bug 4518
    XXX(ACC_HDMI_MODE41,            L_R,        NOT_CONNECTED,  CSURR_0,    CNTRL_CNTRR),   // Bug 4518
    XXX(ACC_HDMI_MODE41_LFE,        L_R,        0_LFE1,     CSURR_0,    CNTRL_CNTRR),   // Bug 4518
    XXX(ACC_HDMI_MODE51,            L_R,        CNTR_0,     CSURR_0,    CNTRL_CNTRR),   // Bug 4518
    XXX(ACC_HDMI_MODE51_LFE,    L_R,        CNTR_LFE1,  CSURR_0,    CNTRL_CNTRR),   // Bug 4518
    XXX(ACC_MODE42,         L_R,        NOT_CONNECTED,  LSUR_RSUR,  CNTRL_CNTRR),   // Bug 4518
    DUPE(ACC_HDMI_MODE42,           L_R,        NOT_CONNECTED,  LSUR_RSUR,  CNTRL_CNTRR),
    XXX(ACC_MODE42_LFE,         L_R,        0_LFE1,     LSUR_RSUR,  CNTRL_CNTRR),   // Bug 4518
    DUPE(ACC_HDMI_MODE42_LFE,   L_R,        0_LFE1,     LSUR_RSUR,  CNTRL_CNTRR),
    XXX(ACC_MODE52,         L_R,        CNTR_0,     LSUR_RSUR,  CNTRL_CNTRR),   // Bug 4518
    DUPE(ACC_HDMI_MODE52,           L_R,        CNTR_0,     LSUR_RSUR,  CNTRL_CNTRR),
    XXX(ACC_MODE52_LFE,         L_R,        CNTR_LFE1,  LSUR_RSUR,  CNTRL_CNTRR),   // Bug 4518
    DUPE(ACC_HDMI_MODE52_LFE,       L_R,        CNTR_LFE1,  LSUR_RSUR,  CNTRL_CNTRR),

    // CEA-861 (E) modes
    XXX(ACC_HDMI_MODE32_T100,   L_R,        CNTR_0,     LSUR_RSUR,      CHIGH_0),       // Bug 4518
    XXX(ACC_HDMI_MODE32_T100_LFE, L_R,       CNTR_LFE1,  LSUR_RSUR,      CHIGH_0),      // Bug 4518
    XXX(ACC_HDMI_MODE32_T010,   L_R,        CNTR_0,     LSUR_RSUR,      TOPSUR_0),      // Bug 4518
    XXX(ACC_HDMI_MODE32_T010_LFE, L_R,       CNTR_LFE1,  LSUR_RSUR,      TOPSUR_0),     // Bug 4518
    XXX(ACC_HDMI_MODE22_T200,   L_R,        NOT_CONNECTED,  LSUR_RSUR,      LHIGH_RHIGH),   // Bug 4518
    XXX(ACC_HDMI_MODE22_T200_LFE, L_R,       0_LFE1,     LSUR_RSUR,      LHIGH_RHIGH),  // Bug 4518
    XXX(ACC_HDMI_MODE42_WIDE,   L_R,        NOT_CONNECTED,  LSUR_RSUR,      LWIDE_RWIDE),   // Bug 4518
    XXX(ACC_HDMI_MODE42_WIDE_LFE, L_R,       0_LFE1,     LSUR_RSUR,      LWIDE_RWIDE),  // Bug 4518
    XXX(ACC_HDMI_MODE33_T010,   L_R,        CNTR_0,     LSUR_RSUR,      CSURR_TOPSUR),      // Bug 4518
    XXX(ACC_HDMI_MODE33_T010_LFE, L_R,       CNTR_LFE1,  LSUR_RSUR,      CSURR_TOPSUR),     // Bug 4518
    XXX(ACC_HDMI_MODE33_T100    , L_R,       CNTR_LFE1,  LSUR_RSUR,      CSURR_CHIGH),  // Bug 4518
    XXX(ACC_HDMI_MODE33_T100_LFE, L_R,       CNTR_LFE1,  LSUR_RSUR,      CSURR_CHIGH),  // Bug 4518
    XXX(ACC_HDMI_MODE32_T110,   L_R,        CNTR_0,     LSUR_RSUR,      CHIGH_TOPSUR),      // Bug 4518
    XXX(ACC_HDMI_MODE32_T110_LFE, L_R,       CNTR_LFE1,  LSUR_RSUR,      CHIGH_TOPSUR),     // Bug 4518
    XXX(ACC_HDMI_MODE32_T200,   L_R,        CNTR_0,     LSUR_RSUR,      LHIGH_RHIGH),   // Bug 4518
    XXX(ACC_HDMI_MODE32_T200_LFE, L_R,       CNTR_LFE1,  LSUR_RSUR,      LHIGH_RHIGH),  // Bug 4518
    XXX(ACC_HDMI_MODE52_WIDE,   L_R,        CNTR_0,     LSUR_RSUR,      LWIDE_RWIDE),   // Bug 4518
    XXX(ACC_HDMI_MODE52_WIDE_LFE, L_R,       CNTR_LFE1,  LSUR_RSUR,      LWIDE_RWIDE),  // Bug 4518

    // Special case where the mono is issued on a 1 or 2 channel output buffer.
    E(ACC_MODE_ALL1,        CNTR_0,     NOT_CONNECTED,  NOT_CONNECTED,  NOT_CONNECTED),

    // DD+ speaker topologies for certification
    E(ACC_MODE_ALL2,        PAIR0,      NOT_CONNECTED,  NOT_CONNECTED,  NOT_CONNECTED),
    E(ACC_MODE_ALL2,        PAIR1,      NOT_CONNECTED,  NOT_CONNECTED,  NOT_CONNECTED),
    E(ACC_MODE_ALL2,        PAIR2,      NOT_CONNECTED,  NOT_CONNECTED,  NOT_CONNECTED),
    E(ACC_MODE_ALL2,        PAIR3,      NOT_CONNECTED,  NOT_CONNECTED,  NOT_CONNECTED),

    // malleable surfaces
    E(ACC_MODE_ALL4,        PAIR0,      PAIR1,          NOT_CONNECTED,  NOT_CONNECTED),
    E(ACC_MODE_ALL6,        PAIR0,      PAIR1,          PAIR2,          NOT_CONNECTED),
    E(ACC_MODE_ALL8,        PAIR0,      PAIR1,          PAIR2,          PAIR3),

#if PCMPROCESSINGS_API_VERSION >= 0x100325 &&  PCMPROCESSINGS_API_VERSION < 0x101122
    // Unusual speaker topologies (not inclued in CEA-861 E)
    XXX(ACC_MODE30_T100,         L_R,            CNTR_0,         NOT_CONNECTED,  CHIGH_0),
    XXX(ACC_MODE30_T100_LFE,     L_R,            CNTR_LFE1,      NOT_CONNECTED,  CHIGH_0),
    XXX(ACC_MODE30_T200,         L_R,            CNTR_0,         NOT_CONNECTED,  LHIGH_RHIGH),
    XXX(ACC_MODE30_T200_LFE,     L_R,            CNTR_LFE1,      NOT_CONNECTED,  LHIGH_RHIGH),
    XXX(ACC_MODE22_T010,         L_R,            NOT_CONNECTED,  LSUR_RSUR,      TOPSUR_0),
    XXX(ACC_MODE22_T010_LFE,     L_R,            0_LFE1,         LSUR_RSUR,      TOPSUR_0),
    XXX(ACC_MODE32_T020,         L_R,            CNTR_0,         LSUR_RSUR,      LHIGHSIDE_RHIGHSIDE),
    XXX(ACC_MODE32_T020_LFE,     L_R,            CNTR_LFE1,      LSUR_RSUR,      LHIGHSIDE_RHIGHSIDE),
    XXX(ACC_MODE23_T100,         L_R,            NOT_CONNECTED,  LSUR_RSUR,      CHIGH_0),
    XXX(ACC_MODE23_T100_LFE,     L_R,            0_LFE1,         LSUR_RSUR,      CHIGH_0),
    XXX(ACC_MODE23_T010,         L_R,            NOT_CONNECTED,  LSUR_RSUR,      TOPSUR_0),
    XXX(ACC_MODE23_T010_LFE,     L_R,            0_LFE1,         LSUR_RSUR,      TOPSUR_0),
#endif

    // DTS-HD speaker topologies (not included in CEA-861)
    // These are all disabled at the moment because there is no matching entry in the AccAcMode
    // enumeration. The automatic fallback code (below) will select ACC_MODE32_LFE automatically
    // (by disconnecting pair3) if the user requests any of these modes.
    //XXX( ACC_MODE32_LFE,          L_R,        CNTR_LFE1,  LSUR_RSUR,  LSIDESURR_RSIDESURR ), No enum

    // delimiter
    E(ACC_MODE_ID,      NOT_CONNECTED,  NOT_CONNECTED,  NOT_CONNECTED,  NOT_CONNECTED)

#undef E
#undef XXX
};


////////////////////////////////////////////////////////////////////////////
///
/// Lookup a discrete audio mode (2.0, 5.1, etc) and convert it to a string.
///
const char *Mixer_Mme_c::LookupAudioMode(enum eAccAcMode DiscreteMode)
{
    uint32_t i;

    for (i = 0; ACC_MODE_ID != ChannelAssignmentLookupTable[i].AccAcMode; i++)
        if (DiscreteMode == ChannelAssignmentLookupTable[i].AccAcMode)
        {
            return ChannelAssignmentLookupTable[i].Text;
        }

    // cover the delimiter itself...
    if (DiscreteMode == ChannelAssignmentLookupTable[i].AccAcMode)
    {
        return ChannelAssignmentLookupTable[i].Text;
    }

    return "UNKNOWN";
}


////////////////////////////////////////////////////////////////////////////
///
/// Lookup the most appropriate ACC_MODE for the current topology.
///
/// This method assume that the channel assignment structure has been been
/// pre-filtered ready for main or auxiliary lookups. In other words the
/// forth pair is *always* disconnected (we will be called twice to handle the
/// forth pair).
///
/// This method excludes from the lookup any format for which the firmware
/// cannot automatically derive (correct) downmix coefficients.
///
enum eAccAcMode Mixer_Mme_c::TranslateChannelAssignmentToAudioMode(struct stm_se_audio_channel_assignment ChannelAssignment)
{
    int32_t pair, i;
    // we want to use memcmp() to compare the channel assignments so
    // we must explicitly zero the bits we don't care about
    ChannelAssignment.pair4 = STM_SE_AUDIO_CHANNEL_PAIR_NOT_CONNECTED;
    ChannelAssignment.reserved0 = 0;
    ChannelAssignment.malleable = 0;

    // CAUTION about signed int !!
    for (pair = 3; pair >= 0; pair--)
    {
        for (i = 0; ACC_MODE_ID != ChannelAssignmentLookupTable[i].AccAcMode; i++)
            if (0 == memcmp(&ChannelAssignmentLookupTable[i].ChannelAssignment,
                            &ChannelAssignment, sizeof(ChannelAssignment)) &&
                ChannelAssignmentLookupTable[i].SuitableForDirectOutput)
            {
                return ChannelAssignmentLookupTable[i].AccAcMode;
            }

        // cover the delimiter itself...
        if (ACC_MODE_ID == ChannelAssignmentLookupTable[i].AccAcMode)
            if (0 == memcmp(&ChannelAssignmentLookupTable[i].ChannelAssignment,
                            &ChannelAssignment, sizeof(ChannelAssignment)) &&
                ChannelAssignmentLookupTable[i].SuitableForDirectOutput)
            {
                return ChannelAssignmentLookupTable[i].AccAcMode;
            }

        // Progressively disconnect pairs of outputs until we find something that matches
        if (pair != 0)
        {
            switch (pair)
            {
            case 1:
                ChannelAssignment.pair1 = STM_SE_AUDIO_CHANNEL_PAIR_NOT_CONNECTED;
                break;

            case 2:
                ChannelAssignment.pair2 = STM_SE_AUDIO_CHANNEL_PAIR_NOT_CONNECTED;
                break;

            case 3:
                ChannelAssignment.pair3 = STM_SE_AUDIO_CHANNEL_PAIR_NOT_CONNECTED;
                break;
            }

            SE_INFO(group_mixer,  "Cannot find matching audio mode - disconnecting pair %d\n", pair);
        }
    }

    SE_INFO(group_mixer,  "Cannot find matching audio mode - falling back to ACC_MODE20t (Lt/Rt stereo)\n");
    return ACC_MODE20t;
}


////////////////////////////////////////////////////////////////////////////
///
/// Lookup the 'natural' channel assignment for an audio mode.
///
/// This method and Mixer_Mme_c::TranslateChannelAssignmentToAudioMode are *not* reversible for
/// audio modes that are not marked as suitable for output.
///
struct stm_se_audio_channel_assignment Mixer_Mme_c::TranslateAudioModeToChannelAssignment(enum eAccAcMode AudioMode)
{
    struct stm_se_audio_channel_assignment Zeros = { 0 };

    if (ACC_MODE_ID == AudioMode)
    {
        Zeros.malleable = 1;
        return Zeros;
    }

    for (uint32_t i = 0; ACC_MODE_ID != ChannelAssignmentLookupTable[i].AccAcMode; i++)
    {
        if (ChannelAssignmentLookupTable[i].AccAcMode == AudioMode)
        {
            return ChannelAssignmentLookupTable[i].ChannelAssignment;
        }
    }

    // no point in stringizing the mode - we know its not in the table
    SE_ERROR("Cannot find matching audio mode (%d)\n", AudioMode);
    return Zeros;
}


////////////////////////////////////////////////////////////////////////////
///
/// Lookup the most appropriate ACC_MODE for the primary output.
///
enum eAccAcMode Mixer_Mme_c::TranslateDownstreamCardToMainAudioMode(const struct snd_pseudo_mixer_downstream_card &DownstreamCard)
{
    struct stm_se_audio_channel_assignment ChannelAssignment = DownstreamCard.channel_assignment;

    // Disconnect any pair that is deselected by the number of channels
    switch (DownstreamCard.num_channels)
    {
    case 1:
    case 2:
        ChannelAssignment.pair1 = STM_SE_AUDIO_CHANNEL_PAIR_NOT_CONNECTED;

    //FALLTHRU
    case 3:
    case 4:
        ChannelAssignment.pair2 = STM_SE_AUDIO_CHANNEL_PAIR_NOT_CONNECTED;

    //FALLTHRU
    case 5:
    case 6:
        ChannelAssignment.pair3 = STM_SE_AUDIO_CHANNEL_PAIR_NOT_CONNECTED;

    //FALLTHRU
    default:
        // pair4 is unconditionally disconnected because it is only used for auxilliary output
        ChannelAssignment.pair4 = STM_SE_AUDIO_CHANNEL_PAIR_NOT_CONNECTED;
    }

    return TranslateChannelAssignmentToAudioMode(ChannelAssignment);
}

////////////////////////////////////////////////////////////////////////////
///
/// Verify if the downmix bypass is desired
///
bool Mixer_Mme_c::CheckDownmixBypass(const struct snd_pseudo_mixer_downstream_card &DownstreamCard)
{
    struct stm_se_audio_channel_assignment ChannelAssignment = DownstreamCard.channel_assignment;
    bool bypass = false;

    switch (DownstreamCard.num_channels)
    {
    case 1:
    case 2:
        if (ChannelAssignment.pair0 == STM_SE_AUDIO_CHANNEL_PAIR_PAIR0 &&
            ChannelAssignment.pair1 == STM_SE_AUDIO_CHANNEL_PAIR_NOT_CONNECTED &&
            ChannelAssignment.pair2 == STM_SE_AUDIO_CHANNEL_PAIR_NOT_CONNECTED &&
            ChannelAssignment.pair3 == STM_SE_AUDIO_CHANNEL_PAIR_NOT_CONNECTED)
        {
            bypass = true;
        }

        break;

    case 3:
    case 4:
        if (ChannelAssignment.pair0 == STM_SE_AUDIO_CHANNEL_PAIR_PAIR0 &&
            ChannelAssignment.pair1 == STM_SE_AUDIO_CHANNEL_PAIR_PAIR1 &&
            ChannelAssignment.pair2 == STM_SE_AUDIO_CHANNEL_PAIR_NOT_CONNECTED &&
            ChannelAssignment.pair3 == STM_SE_AUDIO_CHANNEL_PAIR_NOT_CONNECTED)
        {
            bypass = true;
        }

        break;

    case 5:
    case 6:
        if (ChannelAssignment.pair0 == STM_SE_AUDIO_CHANNEL_PAIR_PAIR0 &&
            ChannelAssignment.pair1 == STM_SE_AUDIO_CHANNEL_PAIR_PAIR1 &&
            ChannelAssignment.pair2 == STM_SE_AUDIO_CHANNEL_PAIR_PAIR2 &&
            ChannelAssignment.pair3 == STM_SE_AUDIO_CHANNEL_PAIR_NOT_CONNECTED)
        {
            bypass = true;
        }

        break;

    default:
        if (ChannelAssignment.pair0 == STM_SE_AUDIO_CHANNEL_PAIR_PAIR0 &&
            ChannelAssignment.pair1 == STM_SE_AUDIO_CHANNEL_PAIR_PAIR1 &&
            ChannelAssignment.pair2 == STM_SE_AUDIO_CHANNEL_PAIR_PAIR2 &&
            ChannelAssignment.pair3 == STM_SE_AUDIO_CHANNEL_PAIR_PAIR3)
        {
            bypass = true;
        }

        break;
    }

    return bypass;
}

////////////////////////////////////////////////////////////////////////////
///
/// Lookup the most appropriate ACC_MODE for the auxiliary output.
///
enum eAccAcMode Mixer_Mme_c::TranslateDownstreamCardToAuxAudioMode(const struct snd_pseudo_mixer_downstream_card &DownstreamCard)
{
    struct stm_se_audio_channel_assignment ChannelAssignment = DownstreamCard.channel_assignment;

    if ((DownstreamCard.num_channels < 10) || (STM_SE_AUDIO_CHANNEL_PAIR_NOT_CONNECTED == ChannelAssignment.pair4))
    {
        return ACC_MODE_ID;
    }

    ChannelAssignment.pair0 = ChannelAssignment.pair4;
    ChannelAssignment.pair1 = ChannelAssignment.pair2 = ChannelAssignment.pair3 = ChannelAssignment.pair4 = STM_SE_AUDIO_CHANNEL_PAIR_NOT_CONNECTED;
    return TranslateChannelAssignmentToAudioMode(ChannelAssignment);
}

////////////////////////////////////////////////////////////////////////////
///
/// Simple lookup table used to convert between incoming speaker topology and
/// the speaker topology that must be provide to FatPipe.
///
static const struct
{
    enum eAccAcMode AccAcModeInput;
    enum eAccAcMode AccAcModeOutput;
}
FatPipeOutputModeLookupTable[] =
{
#define EL(in, out) { in, out }, { in ## _LFE, out ## _LFE }
#define EX(in, out) { in, out }
    EX(ACC_MODE10, ACC_MODE10),
    EL(ACC_MODE20, ACC_MODE20),
    EL(ACC_MODE30, ACC_MODE30),
    //EL(ACC_MODE40, ACC_MODE30),
    //EL(ACC_MODE50, ACC_MODE30),
    EL(ACC_MODE21, ACC_MODE21),
    EL(ACC_MODE31, ACC_MODE31),
    //EL(ACC_MODE41, ACC_MODE31),
    //EL(ACC_MODE51, ACC_MODE31),
    EL(ACC_MODE22, ACC_MODE22),
    EL(ACC_MODE32, ACC_MODE32),
    EX(ACC_MODE42, ACC_MODE32), //EL(ACC_MODE42, ACC_MODE32),
    EX(ACC_MODE52, ACC_MODE32), //EL(ACC_MODE52, ACC_MODE32),
    EL(ACC_MODE23, ACC_MODE33),
    EL(ACC_MODE33, ACC_MODE33),
    EL(ACC_MODE24, ACC_MODE34),
    EL(ACC_MODE34, ACC_MODE34),
    EX(ACC_MODE_ID, ACC_MODE_ID),
#undef EX
#undef EL
};

////////////////////////////////////////////////////////////////////////////
///
/// Lookup the correct FatPipe speaker topology.
///
enum eAccAcMode Mixer_Mme_c::LookupFatPipeOutputMode(enum eAccAcMode InputMode)
{
    int i;

    for (i = 0; ACC_MODE_ID != FatPipeOutputModeLookupTable[i].AccAcModeInput; i++)
        if (InputMode == FatPipeOutputModeLookupTable[i].AccAcModeInput)
        {
            break;
        }

    // i is either the index of the correct entry or the last entry (which is crafted
    // to be the correct value for a lookup miss).
    return FatPipeOutputModeLookupTable[i].AccAcModeOutput;
}

////////////////////////////////////////////////////////////////////////////
///
///
/// This is a synchronous function.
/// This function locks, sets event and waits event so be sure that you
/// call it in a thread state that allows that: in particular DO NOT CALL IT
/// in a spin locked code.
///
PlayerStatus_t Mixer_Mme_c::SendAttachPlayer(Audio_Player_c *AudioPlayer)
{
    PlayerStatus_t status;
    MixerRequest_c request;
    request.SetAudioPlayer(AudioPlayer);
    request.SetFunctionToManageTheRequest(&Mixer_Mme_c::AttachPlayer);
    status = MixerRequestManager.SendRequest(request);

    if (status != PlayerNoError)
    {
        SE_ERROR("Error returned by MixerRequestManager.SendRequest()\n");
    }

    return status;
}

////////////////////////////////////////////////////////////////////////////
///
/// A wrapper to the Mixer_Mme_c::AttachPlayer(Audio_Player_c* Audio_Player)
/// method.
///
PlayerStatus_t Mixer_Mme_c::AttachPlayer(MixerRequest_c &aMixerRequest_c)
{
    PlayerStatus_t Status;
    Audio_Player_c *AudioPlayer = aMixerRequest_c.GetAudioPlayer();
    Status = AttachPlayer(AudioPlayer);
    return Status;
}

////////////////////////////////////////////////////////////////////////////
///
/// Attach audio player to the mixer.
///
PlayerStatus_t Mixer_Mme_c::AttachPlayer(Audio_Player_c *Audio_Player)
{
    SE_DEBUG(group_mixer, ">%s %p\n", Name, Audio_Player);

    if (mTopology.nb_max_players <= NumberOfAudioPlayerAttached)
    {
        SE_ERROR("Can't Attach: would exceed Topology.nb_max_players = %d \n", mTopology.nb_max_players);
        return PlayerNotSupported;
    }

    uint32_t PlayerIdx;

    for (PlayerIdx = 0; PlayerIdx < STM_SE_MIXER_NB_MAX_OUTPUTS; PlayerIdx++)
    {
        if (false == MixerPlayer[PlayerIdx].IsPlayerObjectAttached())
        {
            // Register the given audio player.
            MixerPlayer[PlayerIdx].AttachPlayerObject(Audio_Player);
            NumberOfAudioPlayerAttached++;
            break;
        }
    }

    if (PlayerIdx == STM_SE_MIXER_NB_MAX_OUTPUTS)
    {
        SE_ERROR("Can't Attach:Player table is full\n");
        return PlayerError;
    }

    if ((ThreadState == Idle) && (true == MMEInitialized))
    {
        if (ThereIsOneClientInState(UNCONFIGURED) || ThereIsOneAudioGenIAudioInputInState(STM_SE_AUDIO_GENERATOR_STOPPED)
            || ThereIsOneClientInState(STARTED)   || ThereIsOneAudioGenIAudioInputInState(STM_SE_AUDIO_GENERATOR_STARTED))
        {
            ThreadState = Starting;
            // Atleast 1 player is now attached.
            // Clients may be already started or to be started.
            // Ensure that the players are updated and started in the "Starting" state.
        }
    }

    // If the Mixer is Idle, the player shall not be open
    // If the Mixer is Starting, the player will be open in the InitializePcmPlayer() call
    // If the Mixer is in Playback state, the Player shall be started now.
    if ((ThreadState == Starting) || (ThreadState == Playback))
    {
        PlayerStatus_t Status = InitializePcmPlayer(PlayerIdx);

        if (Status != PlayerNoError)
        {
            MixerPlayer[PlayerIdx].DetachPlayerObject();
            NumberOfAudioPlayerAttached--;
            SE_ERROR("Player (%p) was attached but unable to initialize the pcm player so detaching this player\n", Audio_Player);
            return Status;
        }
    }

    FirstActivePlayer = (true == MixerPlayer[0].IsPlayerObjectAttached()) ? 0 : \
                        (true == MixerPlayer[1].IsPlayerObjectAttached()) ? 1 : \
                        (true == MixerPlayer[2].IsPlayerObjectAttached()) ? 2 : \
                        (true == MixerPlayer[3].IsPlayerObjectAttached()) ? 3 : MIXER_MME_NO_ACTIVE_PLAYER;
    PcmPlayerSurfaceParameters_t NewSurfaceParameters = {{0}};
    MixerPlayer[PlayerIdx].SetPcmPlayerConfigSurfaceParameters(NewSurfaceParameters);
    OutputTopologyUpdated         = true;
    // Re-configure the mixer to update the output topology
    MMENeedsParameterUpdate       = true;
    PcmPlayerNeedsParameterUpdate = true;

    SE_DEBUG(group_mixer, "<%s %d\n", Name, NumberOfAudioPlayerAttached);
    return PlayerNoError;
}

////////////////////////////////////////////////////////////////////////////
///
///
/// This is a synchronous function.
/// This function locks, sets event and waits event so be sure that you
/// call it in a thread state that allows that: in particular DO NOT CALL IT
/// in a spin locked code.
///
PlayerStatus_t Mixer_Mme_c::SendDetachPlayer(Audio_Player_c *AudioPlayer)
{
    PlayerStatus_t status;
    MixerRequest_c request;
    request.SetAudioPlayer(AudioPlayer);
    request.SetFunctionToManageTheRequest(&Mixer_Mme_c::DetachPlayer);
    status = MixerRequestManager.SendRequest(request);

    if (status != PlayerNoError)
    {
        SE_ERROR("Error returned by MixerRequestManager.SendRequest()\n");
    }

    return status;
}

////////////////////////////////////////////////////////////////////////////
///
/// A wrapper to the PlayerStatus_t Mixer_Mme_c::DetachPlayer(Audio_Player_c* Audio_Player)
/// method.
///
PlayerStatus_t Mixer_Mme_c::DetachPlayer(MixerRequest_c &aMixerRequest_c)
{
    PlayerStatus_t Status;
    Audio_Player_c *AudioPlayer = aMixerRequest_c.GetAudioPlayer();
    Status = DetachPlayer(AudioPlayer);
    return Status;
}
////////////////////////////////////////////////////////////////////////////
///
/// Detach audio player from the mixer.
///
PlayerStatus_t Mixer_Mme_c::DetachPlayer(Audio_Player_c *Audio_Player)
{
    SE_DEBUG(group_mixer, ">%s %p\n", Name, Audio_Player);
    uint32_t PlayerIdx;

    for (PlayerIdx = 0; PlayerIdx < STM_SE_MIXER_NB_MAX_OUTPUTS; PlayerIdx++)
    {
        if (true == MixerPlayer[PlayerIdx].IsMyPlayerObject(Audio_Player))
        {
            break;
        }
    }

    if (PlayerIdx == STM_SE_MIXER_NB_MAX_OUTPUTS)
    {
        SE_ERROR("Player not existent\n");
        return PlayerError;
    }

    if (true == MixerPlayer[PlayerIdx].HasPcmPlayer())
    {
        MixerPlayer[PlayerIdx].DeletePcmPlayer();
        NumberOfAudioPlayerInitialized--;
    }

    MixerPlayer[PlayerIdx].DetachPlayerObject();
    NumberOfAudioPlayerAttached--;

    // No need to reset mSPDIFCardId and mHDMICardId here because
    // it will be re-evaluated while processing the MMENeedsParameterUpdate

    FirstActivePlayer = (true == MixerPlayer[0].IsPlayerObjectAttached()) ? 0 : \
                        (true == MixerPlayer[1].IsPlayerObjectAttached()) ? 1 : \
                        (true == MixerPlayer[2].IsPlayerObjectAttached()) ? 2 : \
                        (true == MixerPlayer[3].IsPlayerObjectAttached()) ? 3 : MIXER_MME_NO_ACTIVE_PLAYER;
    OutputTopologyUpdated         = true;
    // Re-configure the mixer to update the output topology
    MMENeedsParameterUpdate       = true;
    PcmPlayerNeedsParameterUpdate = true;
    if ((FirstActivePlayer == MIXER_MME_NO_ACTIVE_PLAYER) && (ThreadState != Idle))
    {
        // We are moving mixer ThreadState to Idle. Update input buffer for all Clients and request to ReleaseProcessingDecodedBuffer.
        for (uint32_t ClientIdx(0); ClientIdx < MIXER_MAX_CLIENTS; ClientIdx++)
        {
            UpdateInputBuffer(ClientIdx, true);
        }
        // We have no player attached. Can't play samples move the Mixer ThreadState to Idle
        SE_WARNING("No player attached. Can't play samples so moving Mixer ThreadState to Idle\n");
        ThreadState = Idle;
    }
    SE_DEBUG(group_mixer, "<%s %d\n", Name, NumberOfAudioPlayerAttached);
    return PlayerNoError;
}

bool Mixer_Mme_c::AllAudioGenIAudioInputsAreDisconnected(void)
{
    bool allClientInTheState = true;

    for (uint32_t AudioGeneratorIdx = 0; AudioGeneratorIdx < MAX_AUDIO_GENERATORS; AudioGeneratorIdx++)
    {
        if (Generator[AudioGeneratorIdx] &&
            Generator[AudioGeneratorIdx]->GetState() != STM_SE_AUDIO_GENERATOR_DISCONNECTED)
        {
            allClientInTheState = false;
            break;
        }
    }

    for (uint32_t InteractiveAudioIdx = 0; InteractiveAudioIdx < MAX_INTERACTIVE_AUDIO; InteractiveAudioIdx++)
    {
        if (InteractiveAudio[InteractiveAudioIdx] &&
            InteractiveAudio[InteractiveAudioIdx]->GetState() != STM_SE_AUDIO_GENERATOR_DISCONNECTED)
        {
            allClientInTheState = false;
            break;
        }
    }

    return allClientInTheState;
}

////////////////////////////////////////////////////////////////////////////
///
/// Specification: it returns true if there is at least one audio generator
/// client in the aState state and false otherwise.
///
bool Mixer_Mme_c::ThereIsOneAudioGenIAudioInputInState(stm_se_audio_generator_state_t aState)
{
    bool allClientInTheState = false;

    for (uint32_t AudioGeneratorIdx = 0; AudioGeneratorIdx < MAX_AUDIO_GENERATORS; AudioGeneratorIdx++)
    {
        if (Generator[AudioGeneratorIdx] && Generator[AudioGeneratorIdx]->GetState() == aState)
        {
            allClientInTheState = true;
            break;
        }
    }

    for (int InteractiveAudioIdx = 0; InteractiveAudioIdx < MAX_INTERACTIVE_AUDIO; InteractiveAudioIdx++)
    {
        if (InteractiveAudio[InteractiveAudioIdx] && InteractiveAudio[InteractiveAudioIdx]->GetState() == aState)
        {
            allClientInTheState = true;
            break;
        }
    }

    return allClientInTheState;
}

////////////////////////////////////////////////////////////////////////////
///
/// Provide sensible defaults for the output configuration.
///
void Mixer_Mme_c::ResetMixerSettings()
{
    MixerSettings.MixerOptions.Reset();
    MixerSettings.MixerOptions.SpeakerConfig = default_speaker_config;
}
