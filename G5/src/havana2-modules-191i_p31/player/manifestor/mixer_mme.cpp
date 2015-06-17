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

Source file name : mixer_mme.cpp
Author :           Daniel

Concrete implementation of an MME mixer driver.


Date        Modification                                    Name
----        ------------                                    --------
29-Jun-07   Created                                         Daniel

************************************************************************/

#include "acc_mme.h"

#include "output_timer_audio.h"
#include "mixer_mme.h"
#include "../codec/codec_mme_base.h"
#include <include/stmdisplay.h>

// /////////////////////////////////////////////////////////////////////////
//
// Tuneable constants
//

#define MIXER_MME_TRANSFORMER_NAME "MME_TRANSFORMER_TYPE_AUDIO_MIXER"
#define MIXER_MAX_WAIT_FOR_MME_COMMAND_COMPLETION  100     /* Ms */
#define MIXER_MIN_FREQUENCY 32000

#define Q15_UNITY 0x7fff

//#define SPLIT_AUX  ACC_DONT_SPLIT 
#define SPLIT_AUX  ACC_SPLIT_AUTO
static const unsigned int SAMPLES_NEEDED_FOR_FADE_OUT = 128;

static OS_TaskEntry( PlaybackThreadStub )
{
    Mixer_Mme_c *Mixer = ( Mixer_Mme_c * ) Parameter;

    Mixer->PlaybackThread();
    OS_TerminateThread();
    return NULL;
}


////////////////////////////////////////////////////////////////////////////
///
/// Stub function with C linkage to connect the MME callback to the manifestor.
///
static void MMECallbackStub(    MME_Event_t      Event,
                                MME_Command_t   *CallbackData,
                                void            *UserData )
{
Mixer_Mme_c *Self = (Mixer_Mme_c *) UserData;

    Self->CallbackFromMME( Event, CallbackData );

    return;
}

////////////////////////////////////////////////////////////////////////////
/// 
///
///
Mixer_Mme_c::Mixer_Mme_c()
{
    InitializationStatus = PlayerNoError;
    
    if( OS_InitializeMutex( &ClientsStateManagementMutex ) != OS_NO_ERROR )
    {
        MIXER_ERROR ( "Unable to create the clients state management mutex\n" );
        return;
    }
    
    memset( &AudioConfiguration, 0, sizeof( AudioConfiguration ) );
    strcpy( AudioConfiguration.TransformName, MIXER_MME_TRANSFORMER_NAME);
    AudioConfiguration.MixerPriority = OS_MID_PRIORITY + 14;

//
    
    UpstreamConfiguration = NULL;
    ResetOutputConfiguration();
    DownmixFirmware = NULL;
    
    Reset();

    InitializationStatus = PlayerNoError;
    MIXER_DEBUG( "Sucessful construction\n" );
}


////////////////////////////////////////////////////////////////////////////
/// 
///
///
Mixer_Mme_c::~Mixer_Mme_c()
{
    OS_TerminateMutex( &ClientsStateManagementMutex );
}


////////////////////////////////////////////////////////////////////////////
/// 
///
///
PlayerStatus_t Mixer_Mme_c::Halt()
{
    if( PlaybackThreadId != OS_INVALID_THREAD )
        TerminatePlaybackThread();
    
    if( MMEInitialized )
    {
        if( PlayerNoError != TerminateMMETransformer() )
        {
            MIXER_ERROR("Failed to terminated mixer transformer\n");
            // no error recovery possible
        }
    }
    
    if( PcmPlayer[0] )
    {
        TerminatePcmPlayer();
    }

    return Mixer_c::Halt();
}


////////////////////////////////////////////////////////////////////////////
/// 
///
///
PlayerStatus_t Mixer_Mme_c::Reset()
{
    memset( &Clients, 0, sizeof( Clients ) );
    for( int i = 0; i < MIXER_MAX_CLIENTS; i++ )
        Clients[i].State = DISCONNECTED;
    
    memset( &InteractiveClients, 0, sizeof( InteractiveClients ) );
    for( int i = 0; i < MIXER_MAX_INTERACTIVE_CLIENTS; i++ )
        InteractiveClients[i].State = DISCONNECTED;

    PlaybackThreadRunning = false;
    PlaybackThreadId = OS_INVALID_THREAD;
    // PlaybackThreadTerminated is undefined

    for( int i = 0; i < MIXER_AUDIO_MAX_OUTPUT_BUFFERS; i++ )
    {
        PcmPlayer[i] = NULL;
        PcmPlayerMappedSamples[i] = NULL;
    }
    memset( &PcmPlayerSurfaceParameters, 0, sizeof( PcmPlayerSurfaceParameters ) );
    PcmPlayerNeedsParameterUpdate = false;

    // MMEHandle is undefined
    MMEInitialized = false;
    // MMECallbackSemaphore is undefined
    // MMEParamCallbackSemaphore is undefined
    // MMECallbackThreadBoosted is undefined
    MMENeedsParameterUpdate = false;

    InitializedSamplingFrequency = 0;
    MixerSamplingFrequency = 0;
    NominalOutputSamplingFrequency = 0;
    MixerGranuleSize = 0;
    
    PrimaryCodedDataType = PcmPlayer_c::OUTPUT_IEC60958;

    ResetMixingMetadata();
    // Do not touch other parts of the OutputConfiguration. These are immnue to reset.
    // Do not touch the DownmixFirmware. It is immune to reset.
    for( int i=0; i<MIXER_AUDIO_MAX_OUTPUT_BUFFERS; ++i )
    {
	LastInputMode[i] = ACC_MODE_ID; // an impossible (guaranteeing a message the first time round)
	LastOutputMode[i] = ACC_MODE_ID; // a legitimate state but this is OK because the former is impossible
    }

    memset( &ActualTopology, 0, sizeof( ActualTopology ) );

    PrimaryClient = 0;

    //
    // Pre-configure the mixer command
    //

    memset( &MixerCommand, 0, sizeof( MixerCommand ) );

    MixerCommand.Command.StructSize = sizeof( MixerCommand.Command );
    MixerCommand.Command.CmdCode = MME_TRANSFORM;
    MixerCommand.Command.CmdEnd = MME_COMMAND_END_RETURN_NOTIFY;
    MixerCommand.Command.DueTime = 0;	// no sorting by time
    MixerCommand.Command.NumberInputBuffers = MIXER_AUDIO_MAX_INPUT_BUFFERS;
    // MixerCommand.Command.NumberOutputBuffers is not constant and must be set later
    MixerCommand.Command.DataBuffers_p = MixerCommand.DataBufferList;
    MixerCommand.Command.CmdStatus.AdditionalInfoSize = sizeof( MME_LxMixerTransformerFrameMix_ExtendedParams_t);
    MixerCommand.Command.CmdStatus.AdditionalInfo_p = &MixerCommand.OutputParams;
    MixerCommand.Command.ParamSize = sizeof( MixerCommand.InputParams );
    MixerCommand.Command.Param_p = &MixerCommand.InputParams;

    for( unsigned int i = 0; i < MIXER_AUDIO_MAX_BUFFERS; i++ )
    {
	MixerCommand.DataBufferList[i] = &MixerCommand.DataBuffers[i];
	MixerCommand.DataBuffers[i].StructSize = sizeof( MME_DataBuffer_t );
	MixerCommand.DataBuffers[i].UserData_p = &MixerCommand.BufferIndex[i * MIXER_AUDIO_PAGES_PER_BUFFER];
	MixerCommand.DataBuffers[i].ScatterPages_p =
		&MixerCommand.ScatterPages[i * MIXER_AUDIO_PAGES_PER_BUFFER];

	//MixerCommand.StreamNumber = i;
    }

    for( unsigned int i = 0; i < MIXER_AUDIO_MAX_PAGES; i++ )
    {
	MixerCommand.BufferIndex[i] = INVALID_BUFFER_ID;
    }

    //
    // Pre-configure the parameter update command
    //

    memset( &ParamsCommand, 0, sizeof( ParamsCommand ) );

    ParamsCommand.Command.StructSize = sizeof( ParamsCommand.Command );
    ParamsCommand.Command.CmdCode = MME_SET_GLOBAL_TRANSFORM_PARAMS;
    ParamsCommand.Command.CmdEnd = MME_COMMAND_END_RETURN_NOTIFY;
    ParamsCommand.Command.DueTime = 0;	// no sorting by time
    ParamsCommand.Command.NumberInputBuffers = 0;
    ParamsCommand.Command.NumberOutputBuffers = 0;
    ParamsCommand.Command.DataBuffers_p = NULL;
    ParamsCommand.Command.CmdStatus.State = MME_COMMAND_COMPLETED;
    ParamsCommand.Command.CmdStatus.AdditionalInfoSize = sizeof( ParamsCommand.OutputParams );
    ParamsCommand.Command.CmdStatus.AdditionalInfo_p = &ParamsCommand.OutputParams;
    ParamsCommand.Command.ParamSize = sizeof( ParamsCommand.InputParams );
    ParamsCommand.Command.Param_p = &ParamsCommand.InputParams;

    return Mixer_c::Reset();
}


////////////////////////////////////////////////////////////////////////////
/// 
///
///
PlayerStatus_t Mixer_Mme_c::SetModuleParameters( unsigned int  ParameterBlockSize,
                                                 void         *ParameterBlock )
{
    PlayerStatus_t Status;
    
    MIXER_DEBUG("\n");
    
    if( ParameterBlockSize == sizeof(OutputConfiguration) )
    {
        struct snd_pseudo_mixer_settings *MixerSettings = (struct snd_pseudo_mixer_settings *) ParameterBlock;
        
        if( MixerSettings->magic == SND_PSEUDO_MIXER_MAGIC )
        {
            UpstreamConfiguration = MixerSettings;
            OutputConfiguration = *MixerSettings;
            MMENeedsParameterUpdate = true;

            Status = UpdatePlayerComponentsModuleParameters();
            if( Status != PlayerNoError )
            {
        	MIXER_ERROR( "Failed to update normalized time offset (a zero value will be used instead)\n" );
        	// no error recovery needed
            }
            
            return PlayerNoError;
        }
    }
    
    if( ParameterBlockSize > sizeof(struct snd_pseudo_mixer_downmix_header) &&
        ((char *) ParameterBlock)[0] == 'D')
    {
	struct snd_pseudo_mixer_downmix_header *Header = 
	    (struct snd_pseudo_mixer_downmix_header *) ParameterBlock;
	
	if ( SND_PSEUDO_MIXER_DOWNMIX_HEADER_MAGIC == Header->magic &&
             SND_PSEUDO_MIXER_DOWNMIX_HEADER_VERSION == Header->version &&
             SND_PSEUDO_MIXER_DOWNMIX_HEADER_SIZE(*Header) == ParameterBlockSize )
	{
	    // the firmware loader is responsible for validating the index section, by the time we reach
	    // this point it is known to be valid.
	    DownmixFirmware = (struct DownmixFirmwareStruct *) Header;
	    
	    return PlayerNoError;
	}
    }
    
    if( ParameterBlockSize == sizeof(struct snd_pseudo_transformer_name) )
    {
	struct snd_pseudo_transformer_name *TransformerName =
	    	(struct snd_pseudo_transformer_name *) ParameterBlock;
	
	if( TransformerName->magic == SND_PSEUDO_TRANSFORMER_NAME_MAGIC)
	{
	    strncpy(AudioConfiguration.TransformName, TransformerName->name,
		    sizeof(AudioConfiguration.TransformName) - 1);
	    AudioConfiguration.TransformName[sizeof(AudioConfiguration.TransformName) - 1] = '\0';
	    
	    return PlayerNoError;
	}
    }

    return Mixer_c::SetModuleParameters( ParameterBlockSize, ParameterBlock );
}

////////////////////////////////////////////////////////////////////////////
/// 
///
///
PlayerStatus_t Mixer_Mme_c::RegisterManifestor( Manifestor_AudioKsound_c * Manifestor )
{
PlayerStatus_t Status;
int i;

    MIXER_DEBUG(">><<\n");

    i = LookupClient( Manifestor );
    if( -1 != i )
    {
	MIXER_ERROR( "Manifestor %p-%d is already registered\n", Manifestor, i );
	return PlayerError;
    }
    
    for( i = 0; i < MIXER_MAX_CLIENTS && Clients[i].State != DISCONNECTED; i++ )
        ; // do nothing
    if( i >= MIXER_MAX_CLIENTS )
    {
	MIXER_ERROR( "Too many manifestors registered\n" );
	return PlayerError;
    }

    Clients[i].State = STOPPED;
    Clients[i].Manifestor = Manifestor;
    memset( &Clients[i].Parameters, 0, sizeof( Clients[i].Parameters ) );
    Clients[i].Muted = false;

    Status = UpdateGlobalState();
    if( Status != PlayerNoError )
    {
        MIXER_ERROR( "Failed to update global state\n" );
        Clients[i].State = DISCONNECTED;
        return Status;
    }

    return PlayerNoError;
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
PlayerStatus_t Mixer_Mme_c::DeRegisterManifestor( Manifestor_AudioKsound_c * Manifestor )
{
int i;

    MIXER_DEBUG(">><<\n");
    
    i = LookupClient( Manifestor );

    if( i < 0 )
    {
	MIXER_ERROR( "Manifestor %p is not registered\n", Manifestor );
	return PlayerError;
    }
    
    if( Clients[i].State != STOPPED )
    {
        MIXER_ERROR( "Cannot deregister an input from state %s\n", LookupInputState( Clients[i].State ) );
        return PlayerError;
    }

    Clients[i].State = DISCONNECTED;
    Clients[i].Manifestor = NULL;
    // we need to reset the parameters otherwise the mixing metadata will remain applied
    memset(&Clients[i].Parameters, 0, sizeof(Clients[i].Parameters));

    return UpdateGlobalState();
}


////////////////////////////////////////////////////////////////////////////
/// 
///
///
PlayerStatus_t Mixer_Mme_c::EnableManifestor( Manifestor_AudioKsound_c * Manifestor )
{
PlayerStatus_t Status;
int i;

    i = LookupClient( Manifestor );
    if( i < 0 )
    {
        MIXER_ERROR( "Manifestor %p is not registered\n", Manifestor );
        return PlayerError;
    }

    MIXER_DEBUG("Enabling manifestor %d-%p\n", i, Manifestor);
    
    if( Clients[i].State != STOPPED )
    {
        MIXER_ERROR( "Cannot enable an input from state %s\n", LookupInputState( Clients[i].State ) );
        return PlayerError;
    }
    
    MIXER_ASSERT( PcmPlayer[0] );
    MIXER_ASSERT( MMEInitialized );
    
    // we must move out of the STOPPED state before calling UpdatePlayerComponentsModuleParameters()
    // otherwise the update will not take place.
    
    MIXER_DEBUG( "Moving manifestor %p-%d from %s to UNCONFIGURED\n",
	         Manifestor, i, LookupInputState( Clients[i].State ) );
    Clients[i].State = UNCONFIGURED;

    Status = UpdatePlayerComponentsModuleParameters();
    if( Status != PlayerNoError )
    {
	MIXER_ERROR( "Failed to update normalized time offset (a zero value will be used instead)\n" );
	// no error recovery needed
    }
    
    Status = UpdateGlobalState();
    if( Status != PlayerNoError )
    {
        MIXER_ERROR( "Failed to update global state\n" );
        Clients[i].State = STOPPED;
        return Status;
    }
    
    return PlayerNoError;

}


////////////////////////////////////////////////////////////////////////////
/// 
///
///
PlayerStatus_t Mixer_Mme_c::DisableManifestor( Manifestor_AudioKsound_c * Manifestor )
{
int i;

    i = LookupClient( Manifestor );
    if( i < 0 )
    {
        MIXER_ERROR( "Manifestor %p is not registered\n", Manifestor );
        return PlayerError;
    }
    
    MIXER_DEBUG("Disabling manifestor %p-%d\n", Manifestor, i);

    if( Clients[i].State == STOPPED || Clients[i].State == DISCONNECTED )
    {
        MIXER_ERROR( "Cannot disable an input from state %s\n", LookupInputState( Clients[i].State ) );
        return PlayerError;
    }
    
    // Note that whilst the writes we are about to do to the state variable are atomic we are not
    // permitted to enter the STOPPED or STOPPING state until it is safe to do so; all code that
    // makes it unsafe to enter this state owns the mutex.
    
    OS_LockMutex( &ClientsStateManagementMutex );    
    
    if( Clients[i].State == UNCONFIGURED )
    {
        // If we are in the UNCONFIGURED state then the mixer main thread has not yet started
        // interacting with the manifestor. this means we can (and, in fact, must) go straight
        // to the STOPPED state without waiting for a handshake from the main thread.

        MIXER_DEBUG( "Moving manifestor %p-%d from UNCONFIGURED to STOPPED\n", Manifestor, i );
        Clients[i].State = STOPPED;
        OS_UnLockMutex( &ClientsStateManagementMutex ); // match other branch
    }
    else
    {
	// When the playback thread observes a clien in the STOPPING state it will finalize the input
	// (request a swan song from the manifestor and then release all resources) before placing the
	// client into the STOPPED state.
	
	MIXER_DEBUG( "Moving manifestor %p-%d from %s to STOPPED\n",
		     Manifestor, i, LookupInputState( Clients[i].State ) );
	Clients[i].State = STOPPING;
	OS_UnLockMutex( &ClientsStateManagementMutex ); // must be released before the busy wait

	// The event is used to nudge the playback thread if it has never dispatched any samples
	// to the MME transformer (it has not entered its main loop yet).
	OS_SetEvent( &PcmPlayerSurfaceParametersUpdated );

	for( int l = 0; Clients[i].State == STOPPING; l++)
	{
	    if (l > 100)
	    {
		MIXER_ERROR( "Time out waiting for manifestor %p-%d to enter STOPPED state\n",
			     Manifestor, i );
		return PlayerError;
	    }

	    OS_SleepMilliSeconds( 10 );
	}
    }
    MIXER_ASSERT( Clients[i].State == STOPPED );
    
    return UpdateGlobalState();
}


////////////////////////////////////////////////////////////////////////////
/// 
///
///
PlayerStatus_t Mixer_Mme_c::UpdateManifestorParameters( Manifestor_AudioKsound_c *Manifestor,
                                                        ParsedAudioParameters_t *ParsedAudioParameters )
{
int i;

    //
    // Store the new audio parameters
    //
    
    i = LookupClient( Manifestor );
    if( i < 0 )
    {
        MIXER_ERROR( "Manifestor %p is not registered\n", Manifestor );
        return PlayerError;
    }
    Clients[i].Parameters = *ParsedAudioParameters;

    MIXER_ASSERT( Clients[i].State != DISCONNECTED );
    MIXER_ASSERT( Clients[i].State != STOPPED );

    if( Clients[i].State == UNCONFIGURED )
    {
        MIXER_DEBUG( "Moving manifestor %p-%d from UNCONFIGURED to STARTING\n", Manifestor, i );
        Clients[i].State = STARTING;
    }
    
    //
    // Re-configure the mixer
    //

    MMENeedsParameterUpdate = true;

    //
    // Reconfigure the PCM player
    //
    
    PcmPlayerNeedsParameterUpdate = true;
    OS_SetEvent( &PcmPlayerSurfaceParametersUpdated );
    
    return PlayerNoError;
}

////////////////////////////////////////////////////////////////////////////
/// 
/// Place the specified manifestor into the (emergency) muted state.
///
/// The emergency mute will be realized as specified by the mixer emergency mute
/// setting. This includes:
/// * Source only: Set the input gain of the manifestors input to zero
/// * Pre-mix: Globally set the input gain of the mixer to zero (affects *all* inputs)
/// * Post-mix: Set the normalization gain of each output to zero (affects *all* inputs)
/// * No mute: Do nothing (exists mostly for testing)
///
PlayerStatus_t Mixer_Mme_c::SetManifestorEmergencyMuteState( Manifestor_AudioKsound_c *Manifestor, bool Muted )
{
    int i = LookupClient( Manifestor );
    if( i < 0 )
    {
        MIXER_ERROR( "Manifestor %p is not registered\n", Manifestor );
        return PlayerError;
    }
    
    MIXER_TRACE( "Set mute state to %s (was %s)\n",
	         (Muted ? "true" : "false"), (Clients[i].Muted ? "true" : "false") );
    
    Clients[i].Muted = Muted;
    MMENeedsParameterUpdate = true;
    return PlayerNoError;
}

////////////////////////////////////////////////////////////////////////////
/// 
/// Provide a pointer to the ::MME_DataBuffer_t associated with a manifestor.
///
/// \todo This method is, arguably, evidence of the unacceptable level of
///       coupling between the audio manifestors and the mixer.
///
PlayerStatus_t Mixer_Mme_c::LookupDataBuffer( Manifestor_AudioKsound_c * Manifestor,
                                              MME_DataBuffer_t **DataBufferPP )
{
    int Index = LookupClient( Manifestor );
    
    if( Index < 0 )
    {
        MIXER_ERROR("Manifestor does not exist\n");
        return PlayerError;
    }
    
    *DataBufferPP = MixerCommand.Command.DataBuffers_p[Index];
    return PlayerNoError;
}


////////////////////////////////////////////////////////////////////////////
/// 
///
///
PlayerStatus_t Mixer_Mme_c::SetOutputRateAdjustment( int adjust )
{
int adjust_done;

    MIXER_DEBUG(">><<\n");
    
    for( unsigned int n = 0; n < ActualNumDownstreamCards; n++ )
    {
	if( PcmPlayer[n] )
	{
	    PcmPlayer[n]->SetOutputRateAdjustment(adjust, &adjust_done);
	}
    }

    return PlayerNoError;
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
PlayerStatus_t Status = PlayerNoError;
unsigned int Cycles;

    //
    // Wait until the PCM player is configured and we can inject the initial lump of silence
    //
    
    while( PlaybackThreadRunning )
    {
        while( PlaybackThreadRunning && !PcmPlayerSurfaceParameters.PeriodParameters.SampleRateHz )
        {
            MIXER_DEBUG( "Waiting for PCM player to be initialized\n" );
            OS_WaitForEvent( &PcmPlayerSurfaceParametersUpdated, 5 * 1000 );
            OS_ResetEvent( &PcmPlayerSurfaceParametersUpdated );
            
            // acknowledge any mixer clients that have stopped
            for( int i = 0; i < MIXER_MAX_CLIENTS; i++ )
                if( Clients[i].State == STOPPING )
                {
                    MIXER_DEBUG( "Moving manifestor %p-%d from STOPPING to STOPPED\n",
                                  Clients[i].Manifestor, i );
                    Clients[i].State = STOPPED;
                }
            for( int i = 0; i < MIXER_MAX_INTERACTIVE_CLIENTS; i++ )
                if( InteractiveClients[i].State == STOPPING )
                {
                    MIXER_DEBUG( "Moving interactive input %d from STOPPING to STOPPED\n", i );
                    InteractiveClients[i].State = STOPPED;
                }
            
            if (PcmPlayerNeedsParameterUpdate)
            {
                MIXER_DEBUG( "About to update PCM player parameters\n" );
                Status = UpdatePcmPlayerParameters();
                if (Status != PlayerNoError)
                {
                    MIXER_ERROR("Failed to update soundcard parameters\n");
                    continue;
                }
            }
        }
        
        if( PlaybackThreadRunning )
        {
            Status = StartPcmPlayer();
            if( Status == PlayerNoError )
            {
                break;
            }
            else
            {
                MIXER_ERROR( "Failed to startup PCM player.\n" );
                // no recovery possible (just wait for someone else to change the PCM parameters)
            }
        }
    }

    //
    // Now that the PCM player is running we can keep running the main loop (which injects silence
    // when there is nothing to play) until death.
    //

    for (Cycles = 0; PlaybackThreadRunning; Cycles++)
    {
        //MIXER_DEBUG("Mixer cycle %d\n", Cycles);
        
        if( Status != PlayerNoError )
        {
            // ensure we don't live-lock if the sound system goes mad...
            OS_SleepMilliSeconds(100);
        }
        
        if (PcmPlayerNeedsParameterUpdate)
        {
            MIXER_DEBUG( "About to update PCM player parameters\n" );
            Status = UpdatePcmPlayerParameters();
            if (Status != PlayerNoError)
            {
                MIXER_ERROR("Failed to update soundcard parameters\n");
                continue;
            }
        }
        
        if( MMENeedsParameterUpdate )
        {
            MIXER_DEBUG( "About to update parameters\n" );
            Status = UpdateMixerParameters();
            if( Status != PlayerNoError )
            {
                MIXER_ERROR( "Failed to update mixer parameters\n" );
                continue;
            }
            
            UpdateIec60958StatusBits();
        }
        
        Status = FillOutMixCommand();
        if( Status != PlayerNoError )
        {
            MIXER_ERROR( "Failed to populate mix command\n" );
            continue;
        }

        Status = SendMMEMixCommand();
        if( Status != PlayerNoError )
        {
            MIXER_ERROR( "Unable to issue mix command\n" );
            continue;
        }
        
        Status = WaitForMMECallback();
        if( Status != PlayerNoError )
        {
            MIXER_ERROR( "Waiting for MME callback returned error\n" );
            continue;
        }
    }

    MIXER_DEBUG( "Terminating\n" );
    OS_SetEvent( &PlaybackThreadTerminated );
}


////////////////////////////////////////////////////////////////////////////
/// 
/// Handle a callback from MME.
///
/// In addition to handling the callback this function also alters the
/// priority of the callback thread (created by MME) such that it has
/// the same priority as the mixer collation thread. This ensures that
/// the delivery of the callback will not be obcured by lower priority
/// work.
///
void Mixer_Mme_c::CallbackFromMME(MME_Event_t Event, MME_Command_t *Command)
{
    //MIXER_DEBUG(">><<\n");

    switch (Event)
    {
    case MME_COMMAND_COMPLETED_EVT:
        switch (Command->CmdCode)
        {
        case MME_TRANSFORM:
            OS_SemaphoreSignal(&MMECallbackSemaphore);
            break;

        case MME_SET_GLOBAL_TRANSFORM_PARAMS:
            OS_SemaphoreSignal(&MMEParamCallbackSemaphore);
            
            if (!MMECallbackThreadBoosted)
            {
                OS_SetPriority(AudioConfiguration.MixerPriority);
                MMECallbackThreadBoosted = true;
            }
            break;
            
        default:
            MIXER_ERROR("Unexpected MME CmdCode (%d)\n", Command->CmdCode);
            break;
        }
        break;
            
    default:
        MIXER_ERROR("Unexpected MME event (%d)\n", Event);
        break;
    }
}

////////////////////////////////////////////////////////////////////////////
///
/// Allocate an interactive input identifier and associated resources.
///
PlayerStatus_t Mixer_Mme_c::AllocInteractiveInput( int *InteractiveId )
{
PlayerStatus_t Status;
unsigned int i;

    for( i = 0; i < MIXER_MAX_INTERACTIVE_CLIENTS && InteractiveClients[i].State != DISCONNECTED; i++ )
        ; // do nothing
    if( i >= MIXER_MAX_INTERACTIVE_CLIENTS )
    {
        MIXER_ERROR("Too many interactive inputs allocated already\n");
        return PlayerError;
    }
    
    MIXER_DEBUG( "Moving interactive input %d from %s to UNCONFIGURED\n",
                 i, LookupInputState( InteractiveClients[i].State ));
    InteractiveClients[i].State = UNCONFIGURED;
    
    Status = UpdateGlobalState();
    if( Status != PlayerNoError )
    {
        MIXER_ERROR( "Failed to update global state\n" );
        InteractiveClients[i].State = DISCONNECTED;
        return Status;
    }
    
    InteractiveClients[i].PlayPointer = 0;
    memset( &InteractiveClients[i].Descriptor, 0, sizeof( InteractiveClients[i].Descriptor ) );

    *InteractiveId = i;
    return PlayerNoError;
}


////////////////////////////////////////////////////////////////////////////
///
/// Free an interative input identifier and associated resources.
///
/// If the input is still in use it will first be forced into the STOPPED
/// state to ensure that the playback thread has disclaimed all interest
/// in the data structures associated with the input.
///
PlayerStatus_t Mixer_Mme_c::FreeInteractiveInput( int InteractiveId )
{
PlayerStatus_t Status;
    
    if( InteractiveId > MIXER_MAX_INTERACTIVE_CLIENTS )
    {
        MIXER_ERROR( "Invalid interactive input identifier (%d)\n", InteractiveId );
        return PlayerError;
    }

    if( InteractiveClients[InteractiveId].State == STARTING ||
        InteractiveClients[InteractiveId].State == STARTED ||
        InteractiveClients[InteractiveId].State == STOPPING )
    {
        MIXER_DEBUG( "Preparing interactive input %d\n", InteractiveId );
        Status = PrepareInteractiveInput( InteractiveId );
        if( Status != PlayerNoError )
            return Status;
    }
      
    if( InteractiveClients[InteractiveId].State != UNCONFIGURED &&
        InteractiveClients[InteractiveId].State != STOPPED )
    {
        MIXER_ERROR( "Cannot free interactive input %d while in state %s\n",
                     InteractiveId,  LookupInputState( InteractiveClients[InteractiveId].State ) );
        return PlayerError;
    }

    MIXER_DEBUG( "Moving interactive input %d from %s to DISCONNECTED\n",
                 InteractiveId, LookupInputState( InteractiveClients[InteractiveId].State ));
    InteractiveClients[InteractiveId].State = DISCONNECTED;
    
    return UpdateGlobalState();
}


////////////////////////////////////////////////////////////////////////////
///
/// Update the configuration of the interactive input.
///
/// Can only be called while the input is not already configured or if it
/// is stopped (or stopping).
///
PlayerStatus_t Mixer_Mme_c::SetupInteractiveInput( int InteractiveId,
                                                   struct alsa_substream_descriptor *Descriptor )
{
PlayerStatus_t Status;

    if( InteractiveId > MIXER_MAX_INTERACTIVE_CLIENTS )
    {
        MIXER_ERROR( "Invalid interactive input identifier (%d)\n", InteractiveId );
        return PlayerError;
    }
    
    /* Wait for the input to stop if needs be. */
    if( InteractiveClients[InteractiveId].State == STOPPING )
    {
        MIXER_DEBUG( "Preparing interactive input %d\n", InteractiveId );
        Status = PrepareInteractiveInput( InteractiveId );
        if( Status != PlayerNoError )
            return Status;
    }
    
    if( InteractiveClients[InteractiveId].State != UNCONFIGURED &&
        InteractiveClients[InteractiveId].State != STOPPED )
    {
        MIXER_ERROR( "Cannot setup interactive input %d while in state %s\n",
                     InteractiveId,  LookupInputState( InteractiveClients[InteractiveId].State ) );
        return PlayerError;
    }
    
    
    MIXER_DEBUG( "Configuration for interactive input %d\n", InteractiveId );
    MIXER_DEBUG( "  hw_buffer %p (%d bytes)\n", Descriptor->hw_buffer, Descriptor->hw_buffer_size );
    MIXER_DEBUG( "  channels %u sampling freq %u bytes_per_sample %u\n",
                 Descriptor->channels, Descriptor->sampling_freq, Descriptor->bytes_per_sample );
    MIXER_DEBUG( "  callback %p(%p, ...)\n", Descriptor->callback, Descriptor->user_data );

    InteractiveClients[InteractiveId].Descriptor = *Descriptor;

    MIXER_DEBUG( "Moving interactive input %d from %s to STOPPED\n",
                 InteractiveId, LookupInputState( InteractiveClients[InteractiveId].State ));
    InteractiveClients[InteractiveId].State = STOPPED;

    Status = UpdateGlobalState();
    if( Status != PlayerNoError )
    {
        MIXER_ERROR( "Failed to update global state\n" );
        MIXER_DEBUG( "Moving interactive input %d from %s to UNCONFIGURED\n",
            InteractiveId, LookupInputState( InteractiveClients[InteractiveId].State ));
        InteractiveClients[InteractiveId].State = UNCONFIGURED;
        return Status;
    }

    //
    // Re-configure the mixer
    //
    
    MMENeedsParameterUpdate = true;

    //
    // Reconfigure the PCM player
    //

    PcmPlayerNeedsParameterUpdate = true;
    OS_SetEvent( &PcmPlayerSurfaceParametersUpdated );
    
    return PlayerNoError;
}


////////////////////////////////////////////////////////////////////////////
///
/// Prepare for the interactive input to be started to restarted.
///
/// In the normal startup case this method is pretty much a no-op, however
/// the method is much more important in the context of underrun recovery where
/// it is responsible for bringing the input into the stopped state ready
/// for it to be restarted.
///
PlayerStatus_t Mixer_Mme_c::PrepareInteractiveInput( int InteractiveId)
{
PlayerStatus_t Status;
    
    if( InteractiveId > MIXER_MAX_INTERACTIVE_CLIENTS )
    {
        MIXER_ERROR( "Invalid interactive input identifier (%d)\n", InteractiveId );
        return PlayerError;
    }

    /* if we are already STOPPED then this is a nop */
    if( InteractiveClients[InteractiveId].State == STOPPED ) {
        MIXER_DEBUG( "Preparing interactive input %d while in state %s is a no-op\n",
                     InteractiveId, LookupInputState( InteractiveClients[InteractiveId].State ) );
        return PlayerNoError;
    }
    
    if( InteractiveClients[InteractiveId].State != STARTING &&
        InteractiveClients[InteractiveId].State != STARTED &&
        InteractiveClients[InteractiveId].State != STOPPING )
    {
        MIXER_ERROR( "Cannot prepare interactive input %d while in state %s\n",
                     InteractiveId,  LookupInputState( InteractiveClients[InteractiveId].State ) );
        return PlayerError;
    }
    
    if( InteractiveClients[InteractiveId].State == STARTING ||
        InteractiveClients[InteractiveId].State == STARTED ) {
        MIXER_DEBUG( "Preparing interactive input %d\n", InteractiveId );
        Status = DisableInteractiveInput( InteractiveId );
        if( Status != PlayerNoError )
                return Status;
    }

    MIXER_DEBUG( "Waiting for interactive input %d to enter STOPPED state\n", InteractiveId );    
    for( int i = 0; InteractiveClients[InteractiveId].State == STOPPING; i++)
    {
        if (i > 1000)
        {
            MIXER_ERROR( "Time out waiting for interactive input %d to enter STOPPED state\n",
                         InteractiveId );
            
            // forcibly disconnect
            InteractiveClients[InteractiveId].State = STOPPED;
            break;
        }
        
        OS_SleepMilliSeconds( 10 );
    }
        
    MIXER_ASSERT( InteractiveClients[InteractiveId].State == STOPPED );
    return PlayerNoError;
}


////////////////////////////////////////////////////////////////////////////
///
/// Start playing from a particular input.
///
/// This method may be called with interrupts locked. This means that it may not
/// perform any action that requires owning a lock. In the context of the Player's
/// thread abstraction this means that it may not set events.
///
PlayerStatus_t Mixer_Mme_c::EnableInteractiveInput( int InteractiveId)
{
    if( InteractiveId > MIXER_MAX_INTERACTIVE_CLIENTS )
    {
        MIXER_ERROR( "Invalid interactive input identifier (%d)\n", InteractiveId );
        return PlayerError;
    }

    if( InteractiveClients[InteractiveId].State == STARTING ||
        InteractiveClients[InteractiveId].State == STARTED )
    {
        MIXER_DEBUG( "Starting interactive input %d again while in state %s\n",
                     InteractiveId, LookupInputState( InteractiveClients[InteractiveId].State ) );
        return PlayerNoError;
    }

    if( InteractiveClients[InteractiveId].State != STOPPED )
    {
        MIXER_ERROR( "Cannot start interactive input %d while in state %s\n",
                     InteractiveId,  LookupInputState( InteractiveClients[InteractiveId].State ) );
        return PlayerError;
    }

    MIXER_DEBUG( "Moving interactive input %d from %s to STARTING\n",
                 InteractiveId, LookupInputState( InteractiveClients[InteractiveId].State ));
    InteractiveClients[InteractiveId].State = STARTING;

    MMENeedsParameterUpdate = true;

    return PlayerNoError;
}


////////////////////////////////////////////////////////////////////////////
///
/// Stop playing from a particular input.
///
/// This method may be called with interrupts locked. This means that it may not
/// perform any action that requires owning a lock. In the context of the Player's
/// thread abstraction this means that it may not set events.
///
PlayerStatus_t Mixer_Mme_c::DisableInteractiveInput( int InteractiveId)
{
    if( InteractiveId > MIXER_MAX_INTERACTIVE_CLIENTS )
    {
        MIXER_ERROR( "Invalid interactive input identifier (%d)\n", InteractiveId );
        return PlayerError;
    }

    if( InteractiveClients[InteractiveId].State == STOPPING ||
        InteractiveClients[InteractiveId].State == STOPPED )
    {
        MIXER_DEBUG( "Stopping interactive input %d again while in state %s\n",
                     InteractiveId, LookupInputState( InteractiveClients[InteractiveId].State ) );
        return PlayerNoError;
    }

    if( InteractiveClients[InteractiveId].State != STARTING &&
        InteractiveClients[InteractiveId].State != STARTED )
    {
        MIXER_ERROR( "Cannot start interactive input %d while in state %s\n",
                     InteractiveId,  LookupInputState( InteractiveClients[InteractiveId].State ) );
        return PlayerError;
    }

    MIXER_DEBUG( "Moving interactive input %d from %s to STOPPING\n",
                 InteractiveId, LookupInputState( InteractiveClients[InteractiveId].State ));
    InteractiveClients[InteractiveId].State = STOPPING;

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
PlayerStatus_t Status;
MME_ERROR ErrorCode;
        
    MIXER_DEBUG("Waiting for parameter semaphore\n");
    OS_SemaphoreWait(&MMEParamCallbackSemaphore);
    MIXER_DEBUG("Got the parameter semaphore\n");
    
    MMENeedsParameterUpdate = false;
    
    // TODO: if the SamplingFrequency has changed we ought to terminate and re-init the transformer

    Status = FillOutTransformerGlobalParameters(&ParamsCommand.InputParams);
    if( Status != PlayerNoError )
    {
        MIXER_ERROR("Could not fill out the transformer global parameters\n");
        return Status;
    }
    
    ErrorCode = MME_SendCommand(MMEHandle, &ParamsCommand.Command);
    if( ErrorCode != MME_SUCCESS ) 
    {
        MIXER_ERROR("Could not issue parameter update command (%d)\n", ErrorCode);
        return PlayerError;
    }

    for( int i = 0; i < MIXER_MAX_CLIENTS; i++ )
        if( Clients[i].State == STARTING )
        {
            MIXER_DEBUG( "Moving manifestor %p-%d from STARTING to STARTED\n", Clients[i].Manifestor, i );
            Clients[i].State = STARTED;

            // place the buffer descriptor (especially NumberOfScatterPages) to the 'silent' state.            
            FillOutSilentBuffer( MixerCommand.Command.DataBuffers_p[i],
                                 MixerCommand.InputParams.InputParam + i );
        }
    
    for( int i = 0; i < MIXER_MAX_INTERACTIVE_CLIENTS; i++ )
        if( InteractiveClients[i].State == STARTING )
        {
            MIXER_DEBUG( "Moving interactive input %d from STARTING to STARTED\n", i );
            InteractiveClients[i].State = STARTED;
        }

    return PlayerNoError;
}


////////////////////////////////////////////////////////////////////////////
/// 
/// Lookup the index into Mixer_Mme_c::Clients of the supplied manifestor.
///
int Mixer_Mme_c::LookupClient( Manifestor_AudioKsound_c * Manifestor )
{
int i;

    for( i = 0; i < MIXER_MAX_CLIENTS && Clients[i].Manifestor != Manifestor; i++ )
	;			// do nothing

    if( i >= MIXER_MAX_CLIENTS )
	return -1;

    return i;
}


////////////////////////////////////////////////////////////////////////////
/// 
/// Examine the downstream topology and determine the maximum frequency we might be asked to
/// support.
///
unsigned int Mixer_Mme_c::LookupMaxMixerSamplingFrequency()
{
    unsigned int MaxFreq = MIXER_MIN_FREQUENCY;
    
    for( unsigned int i=0; i<ActualNumDownstreamCards; i++ )
	if( ActualTopology.card[i].max_freq > MaxFreq )
	    MaxFreq = ActualTopology.card[i].max_freq;
    
    return MaxFreq;
}


////////////////////////////////////////////////////////////////////////////
/// 
/// Examine the clients attached to the mixer and determine what sampling frequency to use.
///
unsigned int Mixer_Mme_c::LookupMixerSamplingFrequency()
{
unsigned int F = 0; // goto-avoiding state variable...

    if( InitializedSamplingFrequency )
    {
        F = InitializedSamplingFrequency;
        MIXER_DEBUG( "Mixer frequency %d (due to forcibly initialized frequency)\n", F );
        return F; // immune to low frequency boosting
    }
    
    if( !F ) {
	if( Clients[PrimaryClient].State == STARTING ||
		Clients[PrimaryClient].State == STARTED )
	{
	    F = Clients[PrimaryClient].Parameters.Source.SampleRateHz;
	    MIXER_DEBUG( "Mixer frequency %d (due to primary manifestor)\n", F );
	}
    }

    if( !F ) {
	for( int i = 0; i < MIXER_MAX_CLIENTS; i++ )
	{
	    if( Clients[i].State == STARTING ||
		    Clients[i].State == STARTED )
	    {
		F = Clients[i].Parameters.Source.SampleRateHz;
		MIXER_DEBUG( "Mixer frequency %d (due to secondary manifestor %d)\n", F, i );
		break;
	    }
	}
    }
    
    if( !F ) {
	for( int i = 0; i < MIXER_MAX_INTERACTIVE_CLIENTS; i++ )
	{
	    if( InteractiveClients[i].State != DISCONNECTED &&
		    InteractiveClients[i].State != UNCONFIGURED )
	    {
		F = InteractiveClients[i].Descriptor.sampling_freq;
		MIXER_DEBUG( "Mixer frequency %d (due to interactive input %d)\n", F, i );
		break;
	    }
	}
    }

    if( !F ) {
	if( MixerSamplingFrequency )
	{
	    MIXER_DEBUG( "No input dictates mixer frequency (falling back to previous value of %d)\n",
		         MixerSamplingFrequency );
	    return MixerSamplingFrequency;
	}
	
	MIXER_DEBUG( "No input dictates mixer frequency (falling back to 48KHz)\n" );
	return 48000;
    }

    unsigned int Funrefined = F;
    
    // determine the highest frequency the downstream output supports
    unsigned int mixer_max_frequency = LookupMaxMixerSamplingFrequency();
    
    // cap the sampling frequency to the largest of the downstream max. output frequencies.
    // this reduces the complexity of the mix and (if we are lucky) of the subsequent PCM
    // post-processing. this translates to (milli)watts...
    while( F > mixer_max_frequency )
    {
	unsigned int Fprime = F / 2;
	if (Fprime * 2 != F) {
	    MIXER_ASSERT(0);
	    break;
	}
	F = Fprime;
    }
    
    // boost low frequencies until they exceed the system minimum (if we are mixing we
    // don't want to drag the sample rate too low and harm the other sources).
    while( F < MIXER_MIN_FREQUENCY )
	F *= 2;
    
    if (Funrefined != F)
	MIXER_DEBUG("Refined mixer frequency to %d due to topological constraints\n", F);
	
    return F;
}


////////////////////////////////////////////////////////////////////////////
/// 
/// Examine the clients attached to the mixer and determine what audio mode the mixer will adopt
///
enum eAccAcMode Mixer_Mme_c::LookupMixerAudioMode()
{
    if( Clients[PrimaryClient].State == STARTING || Clients[PrimaryClient].State == STARTED )
	return (enum eAccAcMode) Clients[PrimaryClient].Parameters.Organisation;

    for( int i = 0; i < MIXER_MAX_CLIENTS; i++ )
	if( Clients[i].State == STARTING || Clients[i].State == STARTED )
	    return (enum eAccAcMode) Clients[i].Parameters.Organisation;
    
    for( int i = 0; i < MIXER_MAX_INTERACTIVE_CLIENTS; i++ ) {
	if( InteractiveClients[i].State != DISCONNECTED && InteractiveClients[i].State != UNCONFIGURED ) {
	    switch (OutputConfiguration.interactive_audio_mode) {
	    case SND_PSEUDO_MIXER_INTERACTIVE_AUDIO_MODE_2_0:
		return ACC_MODE20;

	    case SND_PSEUDO_MIXER_INTERACTIVE_AUDIO_MODE_3_2:
		return ACC_MODE32;

	    case SND_PSEUDO_MIXER_INTERACTIVE_AUDIO_MODE_3_4:
	    default:
		return ACC_MODE34;
	    }
	}
    }

    return ACC_MODE20;
    }


////////////////////////////////////////////////////////////////////////////
/// 
/// Calculate, from the specified sampling frequency, the size of the mixer granule.
///
unsigned int Mixer_Mme_c::LookupMixerGranuleSize(unsigned int Frequency)
{
    unsigned int Granule = MIXER_MAX_48K_GRANULE;

    if( Frequency > 48000)
	Granule *= 2;

#ifdef BUG_4184
    if( Frequency > 96000)
	Granule *= 2;
#endif
    
    MIXER_DEBUG( "Returned %d\n", Granule );

    return Granule;
}

////////////////////////////////////////////////////////////////////////////
/// 
/// Choose between normal and muted values based on the mute settings.
///
int Mixer_Mme_c::SelectGainBasedOnMuteSettings(int ClientIdOrMixingStage, int NormalValue, int MutedValue)
{
    MIXER_ASSERT( 0 <= ClientIdOrMixingStage && ClientIdOrMixingStage <= MIXER_STAGE_MAX );

    switch( OutputConfiguration.emergency_mute )
    {
    case SND_PSEUDO_MIXER_EMERGENCY_MUTE_SOURCE_ONLY:
	if( ClientIdOrMixingStage < MIXER_MAX_CLIENTS && Clients[ClientIdOrMixingStage].Muted ) {
	    MIXER_TRACE("Deploying source-only mute for client %d\n", ClientIdOrMixingStage);
	    return MutedValue;
	}
	break;
	
    case SND_PSEUDO_MIXER_EMERGENCY_MUTE_PRE_MIX:
    case SND_PSEUDO_MIXER_EMERGENCY_MUTE_POST_MIX:
	if( ClientIdOrMixingStage < MIXER_MAX_CLIENTS )
	    break;
	
	for( int i=0; i<MIXER_MAX_CLIENTS; i++)
	    if( DISCONNECTED != Clients[i].State && Clients[i].Muted )
		if( ( SND_PSEUDO_MIXER_EMERGENCY_MUTE_PRE_MIX == OutputConfiguration.emergency_mute &&
		      MIXER_STAGE_PRE_MIX == ClientIdOrMixingStage ) ||
		    ( SND_PSEUDO_MIXER_EMERGENCY_MUTE_POST_MIX == OutputConfiguration.emergency_mute &&
		      MIXER_STAGE_POST_MIX == ClientIdOrMixingStage ) ) {
		    MIXER_TRACE( "Deploying %s-mix mute due to client %d\n",
			         (MIXER_STAGE_PRE_MIX == ClientIdOrMixingStage ? "pre" : "post"), i );
		    return MutedValue;
		}
	break;
	
    case SND_PSEUDO_MIXER_EMERGENCY_MUTE_NO_MUTE:
    default:
	break;
    }
        
    return NormalValue;
}

////////////////////////////////////////////////////////////////////////////
/// 
/// Intialize and terminate the appropriate global resources based the state of the inputs.
///
/// Basically we examine the state of all the inputs (including interactive inputs)
/// and determine which global services should be running. Then we make it so.
///
PlayerStatus_t Mixer_Mme_c::UpdateGlobalState()
{
unsigned int c, i;
PlayerStatus_t Status;

    MIXER_DEBUG(">><<\n");
    
    //
    // If everything is disconnected then completely reset the class
    //
    
    for( c = 0; c < MIXER_MAX_CLIENTS && Clients[c].State == DISCONNECTED; c++ )
        ; // do nothing
    for( i = 0; i < MIXER_MAX_INTERACTIVE_CLIENTS && InteractiveClients[i].State == DISCONNECTED; i++ )
        ; // do nothing
    if( c >= MIXER_MAX_CLIENTS && i >= MIXER_MAX_INTERACTIVE_CLIENTS)
    {
        Status = Halt();
        if( Status != PlayerNoError )
        {
            MIXER_ERROR( "Failed to halt the mixer\n" );
            // no error recovery possible
        }
        
        Status = Reset();
        if( Status != PlayerNoError )
        {
            MIXER_ERROR( "Failed to reset the mixer\n" );
            // no error recovery possible
        }
        
        return PlayerNoError;
    }
    
    //
    // If we reach this point then something is definitely connected so we initialize the
    // PcmPlayer and the MME transformer.
    //
    
    if( !PcmPlayer[0] )
    {
        Status = InitializePcmPlayer();
        if( Status != PlayerNoError )
        {
            MIXER_ERROR( "Failed to initialize PCM player\n" );
            
            return Status;
        }
    }
    
    if( !MMEInitialized )
    {
        Status = InitializeMMETransformer();
        if( Status != PlayerNoError )
        {
            MIXER_ERROR( "Failed to initialize mixer transformer\n" );
            Clients[i].State = DISCONNECTED;
            return Status;
        }
    }

    //
    // If everything is disconnected or stopped the terminate the playback thread.
    // Interactive inputs have a state machine subtly different to non-interactive
    // inputs. For this reason they have a different set of states to be considered.
    //
    
    for( c = 0; c < MIXER_MAX_CLIENTS; c++ )
        if( Clients[c].State != DISCONNECTED && Clients[c].State != STOPPED )
            break;
    for( i = 0; i < MIXER_MAX_INTERACTIVE_CLIENTS; i++ )
        if( InteractiveClients[i].State != DISCONNECTED && InteractiveClients[i].State != UNCONFIGURED )
            break;
    if( c >= MIXER_MAX_CLIENTS && i >= MIXER_MAX_INTERACTIVE_CLIENTS )
    {
        MIXER_ASSERT( PcmPlayer );
        MIXER_ASSERT( MMEInitialized );
        
        if( PlaybackThreadRunning )
            TerminatePlaybackThread();

        return PlayerNoError;
    }

    //
    // If we reach this point then something is definitely ready to play so we
    // start up the playback thread.
    //
    
    if( !PlaybackThreadRunning )
    {
        Status = StartPlaybackThread();
        if( Status != PlayerNoError )
        {
            MIXER_ERROR( "Cannot start playback thread\n" );
            return Status;
        }
    }

    return PlayerNoError;
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
///          mixer controls. There is therefore absolutely no predictablity about
///          when in the startup/shutdown sequences is will be called.
///
PlayerStatus_t Mixer_Mme_c::UpdatePlayerComponentsModuleParameters()
{
    ManifestorStatus_t Status;
    PlayerStatus_t Result = PlayerNoError;
    
    OutputTimerParameterBlock_t ParameterBlockOT;
    CodecParameterBlock_t       ParameterBlockDRC;
    CodecParameterBlock_t       ParameterBlockDownmix;
   
    // Prepare the A/V offset command
    ParameterBlockOT.ParameterType = OutputTimerSetNormalizedTimeOffset;
    ParameterBlockOT.Offset.Value  = OutputConfiguration.master_latency * 1000; // Value is in microseconds
    MIXER_DEBUG( "Master A/V sync offset is %lld us\n", ParameterBlockOT.Offset.Value );

    // Prepare the DRC command
    ParameterBlockDRC.ParameterType = CodecSpecifyDRC;
    ParameterBlockDRC.DRC.Enable    = OutputConfiguration.drc_enable; // ON/OFF switch of DRC 
    ParameterBlockDRC.DRC.Type      = OutputConfiguration.drc_type;   // Line/RF/Custom mode
    ParameterBlockDRC.DRC.HDR       = OutputConfiguration.hdr;        // Boost factor
    ParameterBlockDRC.DRC.LDR       = OutputConfiguration.ldr;        // Cut   factor
    MIXER_DEBUG( "Master A/V  DRC is {Enable = %d / Type = %d / HDR = %d / LDR = %d} \n",
	         ParameterBlockDRC.DRC.Enable, ParameterBlockDRC.DRC.Type,
	         ParameterBlockDRC.DRC.HDR,  ParameterBlockDRC.DRC.LDR );

    // Prepare the downmix command
    ParameterBlockDownmix.ParameterType = CodecSpecifyDownmix;
    if( OutputConfiguration.downmix_promotion_enable )
    {
	eAccAcMode OutmodeMain = TranslateDownstreamCardToMainAudioMode( &ActualTopology.card[0] );	

	for( unsigned i=1; i<ActualNumDownstreamCards; i++ )
	{
	    eAccAcMode CurrentOutmode = TranslateDownstreamCardToMainAudioMode( &ActualTopology.card[i] );
	    if( CurrentOutmode != OutmodeMain )
	    {
		MIXER_ERROR( "Output topology precludes downmix promotion (%s versus %s)\n",
			     LookupAudioMode(OutmodeMain), LookupAudioMode(CurrentOutmode) );
		OutmodeMain = ACC_MODE_ID;
		break;
	    }
	}

	if( ACC_MODE_ID != OutmodeMain )
	    MIXER_TRACE( "Promoting %s downmix to codec (downmix ROM will be disabled)\n", LookupAudioMode(OutmodeMain) );
	ParameterBlockDownmix.Downmix.OutmodeMain = OutmodeMain;
    }
    else
    {	    
        ParameterBlockDownmix.Downmix.OutmodeMain = ACC_MODE_ID;
    }

    // Ensure that the client states don't change in a way that would alter the behavior of the
    // state check, below.
    OS_LockMutex( &ClientsStateManagementMutex );

    // Send the perpared commands to each applicable client
    for( int i = 0; i < MIXER_MAX_CLIENTS; i++ )
    {
	// There is no point in updating a STOPPED client since these parameters will be resent as soon as
	// the manifestor is enabled. In fact, updating STOPPED clients invites races during startup and
	// shutdown since this method is called, at arbitary times, from userspace threads. It would be
	// safe to update STOPPING clients but only it we knew the remained in the STOPPING state after
	// making this check. The mixer thread doesn't take the mutex to transition between STOPPING and
	// STOPPED (deliberately - we don't want the real time mixer thread to be blocked waiting for a
	// userspace thread to be scheduled)) so this mean we must avoid updating STOPPING clients as well.
	if( Clients[i].State != DISCONNECTED && Clients[i].State != STOPPED && Clients[i].State != STOPPING )
	{
	    Status = Clients[i].Manifestor->SetModuleParameters( sizeof(ParameterBlockOT),
		                                                 (void *) &ParameterBlockOT );
	    if( ManifestorNoError != Status )
	    {
		Result = PlayerError;
		break;
	    }

	    Status = Clients[i].Manifestor->SetModuleParameters( sizeof(ParameterBlockDRC),
		                                                 (void *) &ParameterBlockDRC );
	    if ( ManifestorNoError != Status )
	    {
		Result = PlayerError;
		break;
	    }

	    Status = Clients[i].Manifestor->SetModuleParameters( sizeof(ParameterBlockDownmix),
								 (void *) &ParameterBlockDownmix );
	    if ( ManifestorNoError != Status )
	    {
		Result = PlayerError;
		break;
	    }
	}
    }
    
    OS_UnLockMutex( &ClientsStateManagementMutex );
    
    return Result;
}


////////////////////////////////////////////////////////////////////////////
/// 
/// Update our PCM player's IEC 60958 status bits.
///
/// This is called when the PCM players are initialized and whenever the
/// mixer values change.
///
void Mixer_Mme_c::UpdateIec60958StatusBits()
{
    for( unsigned int n=0;n<ActualNumDownstreamCards;n++ )
    {
        (void) PcmPlayer[n]->SetIec60958StatusBits( &OutputConfiguration.iec958_metadata );
        
        if( (ActualTopology.card[n].flags & SND_PSEUDO_TOPOLOGY_FLAGS_ENABLE_HDMI_FORMATING) && 
            (!(ActualTopology.card[n].flags & SND_PSEUDO_TOPOLOGY_FLAGS_ENABLE_SPDIF_FORMATING)) &&
            PcmPlayer_c::IsOutputBypassed( LookupOutputEncoding(n) ) )
        {
            // in case of hdmi bypass not linked to a spdif player, specify a few other bits, 
            // since the device is the pcm player + i2s converter, and such a device 
            // does not support s/w formatting (channel status + validity)
            (void) PcmPlayer[n]->SetIec61937StatusBits( &OutputConfiguration.iec958_metadata );
        }
    }
}


////////////////////////////////////////////////////////////////////////////
/// 
///
///
PlayerStatus_t Mixer_Mme_c::StartPlaybackThread()
{
    MIXER_DEBUG( ">><<\n" );

    if( PlaybackThreadId != OS_INVALID_THREAD )
    {
	MIXER_ERROR( "Playback thread is already running\n" );
	return PlayerError;
    }

    if( OS_InitializeEvent( &PlaybackThreadTerminated ) != OS_NO_ERROR )
    {
	MIXER_ERROR( "Unable to create the thread terminated event\n" );
	return PlayerError;
    }

    PlaybackThreadRunning = true;

    if( OS_CreateThread( &PlaybackThreadId, PlaybackThreadStub, this, "Player Aud Mixer",
			 AudioConfiguration.MixerPriority ) != OS_NO_ERROR )
    {
	MIXER_ERROR( "Unable to create mixer playback thread\n" );
	PlaybackThreadRunning = false;
	OS_TerminateEvent( &PlaybackThreadTerminated );
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
    MIXER_DEBUG( ">><<\n" );

    if( PlaybackThreadId != OS_INVALID_THREAD )
    {
        MIXER_ASSERT( PcmPlayer[0] );
        
        // notify the thread it should exit
	PlaybackThreadRunning = false;
        
        // set any events the thread may be blocked waiting for
        OS_SetEvent( &PcmPlayerSurfaceParametersUpdated );
        
        // wait for the threaed to come to rest
	OS_WaitForEvent( &PlaybackThreadTerminated, OS_INFINITE );
        
        // tidy up
	OS_TerminateEvent( &PlaybackThreadTerminated );
	PlaybackThreadId = OS_INVALID_THREAD;
    }

    MIXER_ASSERT( !PlaybackThreadRunning );
}


////////////////////////////////////////////////////////////////////////////
///
/// Open the PCM player (or players) ready for them to be configured.
///
PlayerStatus_t Mixer_Mme_c::InitializePcmPlayer()
{
OS_Status_t Status;

    MIXER_DEBUG( ">><<\n" );
    
    Status = OS_InitializeEvent( &PcmPlayerSurfaceParametersUpdated );
    if( Status != OS_NO_ERROR )
    {
        MIXER_ERROR( "Failed to initialize surface parameter update event\n" );
        return PlayerError;
    }
    
    // copy the output topology
    ActualTopology = OutputConfiguration.downstream_topology;
    
    // calculate the number of cards described by the topology
    for( ActualNumDownstreamCards=0;
         ActualNumDownstreamCards<SND_PSEUDO_MAX_OUTPUTS;
         ActualNumDownstreamCards++)
    {
	if (ActualTopology.card[ActualNumDownstreamCards].name[0] == '\0')
	    break;
    }
    
    // instanciate based on the latched topology
    for(unsigned int n=0;n<ActualNumDownstreamCards;n++) {
	char *alsaname = ActualTopology.card[n].alsaname;
	unsigned int major, minor;
	
	// TODO: This is a hack version of the card name parsing (only accepts hw:X,Y form).
	//       The alsa name handling should be moved into ksound.
	if (6 != strlen(alsaname) || 0 != strncmp(alsaname, "hw:", 3) ||
	    alsaname[3] < '0' || alsaname[4] != ',' || alsaname[5] < '0')
	{
	    MIXER_ERROR("Cannot find '%s', try 'hw:X,Y' instead\n", alsaname);
	    return PlayerError;
	}
	
	major = alsaname[3] - '0';
	minor = alsaname[5] - '0';

        PcmPlayer[n] = new PcmPlayer_Ksound_c( major, minor, n, LookupOutputEncoding( n ) );
        if( PcmPlayer[n]->InitializationStatus != PlayerNoError )
        {
            TerminatePcmPlayer();
            MIXER_ERROR("Failed to manufacture PCM player\n");
            return PlayerError;
        }
    }
    
    return PlayerNoError;
}


////////////////////////////////////////////////////////////////////////////
/// 
/// Close the PCM player (or players).
///
void Mixer_Mme_c::TerminatePcmPlayer()
{
    MIXER_DEBUG( ">><<\n" );

    for(unsigned int n=0;n<ActualNumDownstreamCards;n++) {
	    if (PcmPlayer[n])
	    {
	        delete PcmPlayer[n];
	        PcmPlayer[n] = NULL;
	    }
    }
    (void) OS_TerminateEvent( &PcmPlayerSurfaceParametersUpdated );
}


////////////////////////////////////////////////////////////////////////////
/// 
/// 
///
PlayerStatus_t Mixer_Mme_c::MapSamples(unsigned int SampleCount, bool NonBlock)
{
PlayerStatus_t Status;
     
    for(unsigned int n=0;n<ActualNumDownstreamCards;n++) {
        Status = PcmPlayer[n]->MapSamples(SampleCount, NonBlock, &PcmPlayerMappedSamples[n]);
        if( Status != PlayerNoError )
        {
            MIXER_ERROR("PcmPlayer %d refused to map %d samples\n", n, SampleCount);
            return PlayerError;
        }
    }
    
    return PlayerNoError;
}


////////////////////////////////////////////////////////////////////////////
///
/// Commit any outstanding samples to the PCM player buffers and update the estimated display times.
///
PlayerStatus_t Mixer_Mme_c::CommitMappedSamples()
{
PlayerStatus_t Status;
unsigned long long DisplayTimeOfNextCommit;
/*
    //copy output to other PCM Players' memory
    if(ActualTopology.num_cards>1) {
        for(unsigned int n=1;n<ActualTopology.num_cards;n++) {
            memcpy(PcmPlayerMappedSamples[n],
                   PcmPlayerMappedSamples[0],
                   PcmPlayer[n]->SamplesToBytes(PcmPlayerSurfaceParameters.PeriodSize));
        }
    }
*/
    for(unsigned int n=0;n<ActualNumDownstreamCards;n++) {
        PcmPlayerMappedSamples[n] = NULL;

        Status = PcmPlayer[n]->CommitMappedSamples();
        if (Status != PlayerNoError)
        {
            MIXER_ERROR("PcmPlayer failed to commit mapped samples\n");
            return PlayerError;
        }
    }

    // 
    // Calculate when the next commit will be displayed and appriase the manifestors of the
    // situation. Just use the first PCM player for this.
    //
    
    Status = PcmPlayer[0]->GetTimeOfNextCommit( &DisplayTimeOfNextCommit );
    if (Status != PlayerNoError)
    {
        MIXER_ERROR("PCM player won't tell us its commit latency\n");
        return Status;
    }    
    
    for( int i = 0; i < MIXER_MAX_CLIENTS; i++ )
        if( Clients[i].State != DISCONNECTED )
            Clients[i].Manifestor->UpdateDisplayTimeOfNextCommit( DisplayTimeOfNextCommit );
    
    return PlayerNoError;
}


////////////////////////////////////////////////////////////////////////////
/// 
/// Populate the PCM player buffers to ensure clean startup.
///
/// Clean startup means startup without immediate startvation. If the PCM
/// player buffers are not seeded with a starting value they rapidly consume
/// the data in the decode buffers. When the decode buffers are consumed the
/// mixer enters the starved state and mutes itself.
///
/// If the PCM player buffers have a start threshhold of their own buffer
/// length then this method can simply mix in advance consuming samples from
/// the buffer queue. Alternatively, if the PCM player buffer is 'short' then
/// the buffer can simply be filled with silence.
///
PlayerStatus_t Mixer_Mme_c::StartPcmPlayer()
{
ManifestorStatus_t Status;
unsigned int BufferSize = PcmPlayerSurfaceParameters.PeriodSize * PcmPlayerSurfaceParameters.NumPeriods;
    
    Status = MapSamples(BufferSize, true); // non-blocking attempt to map
    if( Status != ManifestorNoError )
    {
        MIXER_ERROR("Failed to map all the PCM buffer samples (%u)\n", BufferSize);
        return ManifestorError;
    }

    MIXER_DEBUG( "Injecting %d muted samples\n", BufferSize );
         
    for(unsigned int n=0;n<ActualNumDownstreamCards;n++) {
        memset(PcmPlayerMappedSamples[n], 0, PcmPlayer[n]->SamplesToBytes(BufferSize));
    }

    Status = CommitMappedSamples();
    if( Status != ManifestorNoError )
    {
        MIXER_ERROR("Failed to commit samples to the player\n");
        return ManifestorError;
    }
    
    return ManifestorNoError;
}


////////////////////////////////////////////////////////////////////////////
/// 
/// Update soundcard parameters.
///
PlayerStatus_t Mixer_Mme_c::UpdatePcmPlayerParameters()
{
PlayerStatus_t Status;
PcmPlayerSurfaceParameters_t NewSurfaceParameters[MIXER_AUDIO_MAX_OUTPUT_BUFFERS] = {{{0}}};
    
    PcmPlayerNeedsParameterUpdate = false;
    
    // during an update the MixerSamplingFrequency becomes stale (it is updated after we have finished
    // updating the manifestor state). this method is called between these points and therefore must
    // perform a fresh lookup.
    NominalOutputSamplingFrequency = LookupMixerSamplingFrequency();
    unsigned int NominalMixerGranuleSize = LookupMixerGranuleSize(NominalOutputSamplingFrequency);
    
    if( PcmPlayerSurfaceParameters.PeriodParameters.SampleRateHz == NominalOutputSamplingFrequency )
    {
        MIXER_DEBUG( "Ingoring redundant attempt to update the audio parameters.\n" );
        return PlayerNoError;
    }   
    
    for(unsigned int n=0;n<ActualNumDownstreamCards;n++) 
    {
        unsigned int ActualSampleRateHz = LookupOutputSamplingFrequency( n );
        NewSurfaceParameters[n].PeriodParameters.BitsPerSample = 24;
        NewSurfaceParameters[n].PeriodParameters.ChannelCount = LookupOutputNumberOfChannels(n);
        NewSurfaceParameters[n].PeriodParameters.SampleRateHz = (PcmPlayer_c::IsOutputBypassed(LookupOutputEncoding(n)))?ActualSampleRateHz:NominalOutputSamplingFrequency;
        NewSurfaceParameters[n].ActualSampleRateHz = ActualSampleRateHz;
        NewSurfaceParameters[n].PeriodSize = NominalMixerGranuleSize; //LookupOutputNumberOfSamples( n, NominalMixerGranuleSize, ActualSampleRateHz, NominalOutputSamplingFrequency );
        NewSurfaceParameters[n].NumPeriods = MIXER_NUM_PERIODS;
        
        MIXER_DEBUG("Card %d: #Samples: %d Sampling Rate: %d, # channels: %d\n", n, 
                    NewSurfaceParameters[n].PeriodSize, 
                    NewSurfaceParameters[n].ActualSampleRateHz,
                    NewSurfaceParameters[n].PeriodParameters.ChannelCount);
    }

    for(unsigned int n=0;n<ActualNumDownstreamCards;n++) 
    {
        Status = PcmPlayer[n]->SetParameters(&NewSurfaceParameters[n]);

        if (Status != PlayerNoError)
        {
            if (PcmPlayerMappedSamples[n])
            {
                PcmPlayerNeedsParameterUpdate = true;
                MIXER_TRACE("Unable to modify pcm player settings at that time\n");

                return PlayerNoError;
            }
            else
            {
                MIXER_ERROR("Fatal error attempting to reconfigure the PCM player\n");
                
                // Restore the original settings. It is required for class consistancy that all the attached
                // PCM players remain in a playable state (e.g. so we can safely call ksnd_pcm_wait() on them)
                for(unsigned int m=0; m<=n; m++)
                {
                    NewSurfaceParameters[n] = PcmPlayerSurfaceParameters;
                    Status = PcmPlayer[n]->SetParameters(&NewSurfaceParameters[n]);
                    MIXER_ASSERT(Status == PlayerNoError);
                }
         
                return PlayerError;
            }
        }
    }
    
    PcmPlayerSurfaceParameters = NewSurfaceParameters[0];
    
    MIXER_DEBUG( "PcmPlayer parameters - NumPeriods %u PeriodSize %u\n",
                 PcmPlayerSurfaceParameters.NumPeriods, PcmPlayerSurfaceParameters.PeriodSize );
    
    if( PcmPlayerSurfaceParameters.PeriodSize != NominalMixerGranuleSize )
    {
        MIXER_ERROR("Invalid PCM player period size %u\n", PcmPlayerSurfaceParameters.PeriodSize);
        return PlayerError;
    }

    UpdateIec60958StatusBits();
    OS_SetEvent( &PcmPlayerSurfaceParametersUpdated );
    
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
PlayerStatus_t Mixer_Mme_c::InitializeMMETransformer( void )
{
PlayerStatus_t Status;
MME_ERROR MMEStatus;
MME_TransformerCapability_t Capability = { 0 };
MME_LxMixerTransformerInfo_t MixerInfo = { 0 };
MME_TransformerInitParams_t InitParams = { 0 };
MME_LxMixerTransformerInitBDParams_Extended_t MixerParams = { 0 };
ParsedAudioParameters_t &PrimaryAudioParameters = Clients[PrimaryClient].Parameters;

    //
    // Obtain the capabilities of the mixer
    //

    Capability.StructSize = sizeof( Capability );
    Capability.TransformerInfo_p = &MixerInfo;
    Capability.TransformerInfoSize = sizeof( MixerInfo );
    MMEStatus = MME_GetTransformerCapability( AudioConfiguration.TransformName, &Capability );
    if( MMEStatus != MME_SUCCESS )
    {
	MIXER_ERROR( "%s - Unable to read capabilities (%d)\n", AudioConfiguration.TransformName, MMEStatus );
	return PlayerError;
    }

    MIXER_TRACE( "Found %s transformer (version %x)\n", AudioConfiguration.TransformName, Capability.Version );

    // Dump the transformer capability structure
    MIXER_DEBUG( "\tMixerCapabilityFlags = %08x\n", MixerInfo.MixerCapabilityFlags );
    MIXER_DEBUG( "\tPcmProcessorCapabilityFlags[0..1] = %08x %08x\n",
		      MixerInfo.PcmProcessorCapabilityFlags[0], MixerInfo.PcmProcessorCapabilityFlags[1] );
    //
    // Verify that is it fit for purpose
    //

    if( MixerInfo.StructSize != sizeof( MixerInfo ) )
    {
	MIXER_ERROR( "Detected structure size skew between firmware and driver\n" );
	return PlayerError;
    }

    // TODO: should check that interactive audio is supported

//

    //
    // Initialize other components whose lifetime is linked to the transformer
    //

    Status = OS_SemaphoreInitialize( &MMECallbackSemaphore, 0 );
    if( Status != OS_NO_ERROR )
    {
	MIXER_ERROR( "Could initialize callback semaphore\n" );
	return PlayerError;
    }

    // initialize the param semaphore posted...
    Status = OS_SemaphoreInitialize( &MMEParamCallbackSemaphore, 1 );
    if( Status != OS_NO_ERROR )
    {
	MIXER_ERROR( "Could initialize parameter callback semaphore\n" );
	OS_SemaphoreTerminate( &MMECallbackSemaphore );
	return PlayerError;
    }
//

    //
    // Initialize the transformer
    //

    InitParams.StructSize = sizeof( InitParams );
    InitParams.Priority = MME_PRIORITY_ABOVE_NORMAL;	// we are more important than a decode...
    InitParams.Callback = MMECallbackStub;
    InitParams.CallbackUserData = ( void * ) this;
    InitParams.TransformerInitParamsSize = sizeof( MixerParams );
    InitParams.TransformerInitParams_p = &MixerParams;

    MixerParams.StructSize = sizeof( MixerParams );
    MixerParams.CacheFlush = ACC_MME_ENABLED;
    MixerParams.NbInput = MIXER_MAX_INPUTS;
    // the large this value to more memory we'll need on the co-processor (if we didn't consider the topology
    // here we'ld blow our memory budget on STi710x)
    MixerParams.MaxNbOutputSamplesPerTransform = LookupMixerGranuleSize( LookupMaxMixerSamplingFrequency() );

    // The interleaving of the output buffer that the host will send to the mixer at each transform.
    // Note that we are using the structure in a 'backwards compatible' mode. If we need to be more
    // expressive we must look at MME_OutChan_t.
    MixerParams.OutputNbChannels = ActualTopology.card[0].num_channels;

    // choose the output mode (either the sampling frequency of the mixer or a 'floating' value that can
    // be specified later)
    MixerParams.OutputSamplingFreq = ( OutputConfiguration.fixed_output_frequency == 1 ?
                                       ACC_FS48k : (eAccFsCode) AUDIOMIXER_OVERRIDE_OUTFS );
    InitializedSamplingFrequency = TranslateDiscreteSamplingFrequencyToInteger( MixerParams.OutputSamplingFreq );

    // provide sane default values (to ensure they are never zero when consumed)
    MixerSamplingFrequency = NominalOutputSamplingFrequency = LookupMixerSamplingFrequency();

    MIXER_DEBUG( "MixerParams.OutputSamplingFreq = %s (%d)\n",
                 LookupDiscreteSamplingFrequency( MixerParams.OutputSamplingFreq ),
                 PrimaryAudioParameters.Source.SampleRateHz );

    MMENeedsParameterUpdate = false;

    Status = FillOutTransformerGlobalParameters( &( MixerParams.GlobalParams ) );
    if( Status != PlayerNoError )
    {
	MIXER_ERROR( "Could not fill out the transformer global parameters\n" );
	OS_SemaphoreTerminate( &MMECallbackSemaphore );
	OS_SemaphoreTerminate( &MMEParamCallbackSemaphore );
	return Status;
    }

    //As dynamic, we need to fixup the sizes we've declared
    MixerParams.StructSize -= (sizeof(MixerParams.GlobalParams) - MixerParams.GlobalParams.StructSize);
    InitParams.TransformerInitParamsSize = MixerParams.StructSize;

    MMEStatus = MME_InitTransformer( AudioConfiguration.TransformName, &InitParams, &MMEHandle );
    if( MMEStatus != MME_SUCCESS )
    {
	MIXER_ERROR( "Failed to initialize %s (%08x).\n", AudioConfiguration.TransformName, MMEStatus );
	OS_SemaphoreTerminate( &MMECallbackSemaphore );
	OS_SemaphoreTerminate( &MMEParamCallbackSemaphore );
	return PlayerError;
    }

//

    MMEInitialized = true;
    MMECallbackThreadBoosted = false;
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
PlayerStatus_t Mixer_Mme_c::TerminateMMETransformer( void )
{
    MME_ERROR Status;
    int TimeToWait;

//

    if( MMEInitialized )
    {
	//
	// Wait a reasonable time for all mme transactions to terminate
	//

	TimeToWait = MIXER_MAX_WAIT_FOR_MME_COMMAND_COMPLETION;

	for( ;; )
	{
	    Status = MME_TermTransformer( MMEHandle );
	    if( Status != MME_COMMAND_STILL_EXECUTING )
	    {
		break;
	    }
	    else
	    {
		if( TimeToWait > 0 )
		{
		    TimeToWait -= 10;
		    OS_SleepMilliSeconds( 10 );
		}
		else
		{
		    break;
		}
	    }
	}

	if( Status == MME_SUCCESS )
	{
	    OS_SemaphoreTerminate( &MMECallbackSemaphore );
	    OS_SemaphoreTerminate( &MMEParamCallbackSemaphore );
	    MMEInitialized = false;
	    return PlayerNoError;

	}
    }

    return PlayerError;
}


////////////////////////////////////////////////////////////////////////////
/// 
/// Issue the (already populated) command to the mixer transformer.
///
PlayerStatus_t Mixer_Mme_c::SendMMEMixCommand()
{
MME_ERROR ErrorCode;
    
    ErrorCode = MME_SendCommand(MMEHandle, &MixerCommand.Command);
    if( ErrorCode != MME_SUCCESS ) 
    {
        MIXER_ERROR("Could not issue mixer command (%d)\n", ErrorCode);
        return PlayerError;
    }
    
    return PlayerNoError;
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
unsigned int OutputBufferIndex = MixerCommand.Command.NumberInputBuffers;
    
    OS_SemaphoreWait(&MMECallbackSemaphore);

    if( MixerCommand.Command.CmdStatus.State != MME_COMMAND_COMPLETED )
    {
        MIXER_ERROR( "MME command failed (State %d Error %d)\n",
                     MixerCommand.Command.CmdStatus.State, MixerCommand.Command.CmdStatus.Error );
    }

    Status = UpdateOutputBuffer( MixerCommand.Command.DataBuffers_p[OutputBufferIndex] );
    if( Status != PlayerNoError )
    {
        MIXER_ERROR( "Failed to update the output buffer\n" );
        return PlayerError;
    }
    
    for( int i = 0; i < MIXER_MAX_CLIENTS; i++ )
        UpdateInputBuffer(i);

    for( int i = 0; i < MIXER_MAX_INTERACTIVE_CLIENTS; i++ )
        UpdateInteractiveBuffer(i);

    return PlayerNoError;
}


////////////////////////////////////////////////////////////////////////////
/// 
/// Provide sensible defaults for the output configuration.
///
void Mixer_Mme_c::ResetOutputConfiguration()
{
    memset( &OutputConfiguration, 0, sizeof(OutputConfiguration ) );
    
    OutputConfiguration.magic = SND_PSEUDO_MIXER_MAGIC;
    
    /* pre-mix gain control */
    OutputConfiguration.post_mix_gain = Q3_13_UNITY;
    for( int i=0; i<SND_PSEUDO_MIXER_CHANNELS; i++ )
    {
	OutputConfiguration.primary_gain[i] = Q3_13_UNITY;
	OutputConfiguration.secondary_gain[i] = Q3_13_UNITY;
	OutputConfiguration.secondary_pan[i] = Q3_13_UNITY;
    }
    for( int i=0; i<SND_PSEUDO_MIXER_INTERACTIVE; i++ )
    {
	OutputConfiguration.interactive_gain[i] = Q3_13_UNITY;
	for( int j=0; j<SND_PSEUDO_MIXER_CHANNELS; j++ )
	    OutputConfiguration.interactive_pan[i][j] = Q3_13_UNITY;
    }
    OutputConfiguration.metadata_update = SND_PSEUDO_MIXER_METADATA_UPDATE_NEVER;

    /* post-mix gain control */
    for( int i=0; i<SND_PSEUDO_MIXER_CHANNELS; i++ )
	OutputConfiguration.master_volume[i] = 0;
    for( int i=0; i<SND_PSEUDO_MAX_OUTPUTS; i++ )
    {
	OutputConfiguration.chain_volume[i] = 0;
	OutputConfiguration.chain_enable[i] = 1;
	OutputConfiguration.chain_limiter_enable[i] = 1;
    }
    for ( int i=0; i<SND_PSEUDO_MAX_OUTPUTS; i++ )
        for ( int j=0; j<SND_PSEUDO_MIXER_CHANNELS; j++ )
            OutputConfiguration.chain_delay[i][j] = 0;

    /* emergency mute */
    OutputConfiguration.emergency_mute = SND_PSEUDO_MIXER_EMERGENCY_MUTE_SOURCE_ONLY;

    /* switches */
    OutputConfiguration.dc_remove_enable = 1; /* On */
    OutputConfiguration.bass_mgt_bypass = 0; /* Off */
    OutputConfiguration.fixed_output_frequency = 0; /* Off */
    OutputConfiguration.all_speaker_stereo_enable = 0; /* Off */
    OutputConfiguration.downmix_promotion_enable = 0; /* Off */

    /* routes */
    OutputConfiguration.spdif_encoding = SND_PSEUDO_MIXER_SPDIF_ENCODING_PCM;
    OutputConfiguration.hdmi_encoding = SND_PSEUDO_MIXER_SPDIF_ENCODING_PCM;
    OutputConfiguration.interactive_audio_mode = SND_PSEUDO_MIXER_INTERACTIVE_AUDIO_MODE_3_4;
    OutputConfiguration.virtualization_mode = 0; /* Off */
    OutputConfiguration.spacialization_mode = 0; /* Off */

    /* latency tuning */
    OutputConfiguration.master_latency = 0;
    for( int i=0; i<SND_PSEUDO_MAX_OUTPUTS; i++ )
	OutputConfiguration.chain_latency[i] = 0;

    /* generic spdif meta data */
    /* rely on memset */

    /* fatpipe meta data */
    /* rely on memset */
    
    /* topological structure */
    strcpy(OutputConfiguration.downstream_topology.card[0].name, "Default");
    strcpy(OutputConfiguration.downstream_topology.card[0].alsaname, "hw:0,0");
    OutputConfiguration.downstream_topology.card[0].flags = 0;
    OutputConfiguration.downstream_topology.card[0].max_freq = 48000;
    OutputConfiguration.downstream_topology.card[0].num_channels = 2;
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
    // Read the code carefully here. We use lots of x = y = z statements.

    if (OutputConfiguration.metadata_update != SND_PSEUDO_MIXER_METADATA_UPDATE_NEVER)
    {
        for( int i=0; i<SND_PSEUDO_MIXER_CHANNELS; i++ )
        {
            UpstreamConfiguration->primary_gain[i] = OutputConfiguration.primary_gain[i] = Q3_13_UNITY;
        }

        for( int i=0; i<SND_PSEUDO_MIXER_CHANNELS; i++ )
        {
            UpstreamConfiguration->secondary_gain[i] = OutputConfiguration.secondary_gain[i] = Q3_13_UNITY;
            UpstreamConfiguration->secondary_pan[i] = OutputConfiguration.secondary_pan[i] = Q3_13_UNITY;
        }
        
        if (OutputConfiguration.metadata_update == SND_PSEUDO_MIXER_METADATA_UPDATE_ALWAYS)
        {
            UpstreamConfiguration->post_mix_gain = OutputConfiguration.post_mix_gain = Q3_13_UNITY;
        }
    }
}
                                                                
////////////////////////////////////////////////////////////////////////////
/// 
/// Populate a pointer to the mixer's global parameters structure.
///
PlayerStatus_t Mixer_Mme_c::FillOutTransformerGlobalParameters( MME_LxMixerBDTransformerGlobalParams_Extended_t *
								GlobalParams )
{
    struct snd_pseudo_mixer_settings &Configuration = OutputConfiguration;
    
//
    
    // set the global reference for the actual mixer frequency and granule size
    MixerSamplingFrequency = LookupMixerSamplingFrequency();
    MixerGranuleSize = LookupMixerGranuleSize( MixerSamplingFrequency );
    UpdateMixingMetadata();

//

    memset( GlobalParams, 0, sizeof( *GlobalParams ) );
    GlobalParams->StructSize = sizeof( *GlobalParams );

//

    MME_LxMixerInConfig_t & InConfig = GlobalParams->InConfig;
    InConfig.Id = ACC_RENDERER_MIXER_ID;
    InConfig.StructSize = sizeof( InConfig );
    
    for( int i = 0; i < MIXER_MAX_CLIENTS; i++ )
    {
	// b[7..4] :: Input Type | b[3..0] :: Input Number
	InConfig.Config[i].InputId = ( ACC_MIXER_LINEARISER << 4 ) + i;

        InConfig.Config[i].Alpha = 0xffff;  // Mixing Coefficient for Each Input
        InConfig.Config[i].Mono2Stereo = ACC_MME_TRUE;  // [enum eAccBoolean] Mono 2 Stereo upmix of a mono input
        InConfig.Config[i].WordSize = ACC_WS32;     // Input WordSize : ACC_WS32 / ACC_WS16
        // To which output channel is mixer the 1st channel of
        // this input stream
        InConfig.Config[i].FirstOutputChan = ACC_MAIN_LEFT;
        InConfig.Config[i].AutoFade = ACC_MME_TRUE;
        InConfig.Config[i].Config = 0;
        
        if( ( Clients[i].State == STARTING ) || ( Clients[i].State == STARTED ) )
        {
            ParsedAudioParameters_t &AudioParameters = Clients[i].Parameters;

            InConfig.Config[i].NbChannels = AudioParameters.Source.ChannelCount; // Interleaving of the input pcm buffers
            InConfig.Config[i].AudioMode = ( eAccAcMode ) AudioParameters.Organisation;  //  Channel Configuration
            InConfig.Config[i].SamplingFreq =
                    TranslateIntegerSamplingFrequencyToDiscrete( AudioParameters.Source.SampleRateHz );

            MIXER_DEBUG( "Input %d: AudioMode %s (%d)  SamplingFreq %s (%d)\n",
                         i,
                         LookupAudioMode( InConfig.Config[i].AudioMode ),
                         InConfig.Config[i].AudioMode,
                         LookupDiscreteSamplingFrequency( InConfig.Config[i].SamplingFreq ),
                         AudioParameters.Source.SampleRateHz );
        }
        else
        {
            // workaround a bug in the mixer (it crashes on transition from 0 channels to 8 channels)
            InConfig.Config[i].NbChannels = 8;
            InConfig.Config[i].AudioMode = ACC_MODE20;
            InConfig.Config[i].SamplingFreq = TranslateIntegerSamplingFrequencyToDiscrete( 
                    MixerSamplingFrequency );;
        }
    }
    

    MME_MixerInputConfig_t & CodedDataInConfig = InConfig.Config[MIXER_CODED_DATA_INPUT];
    CodedDataInConfig.InputId = 2;
    CodedDataInConfig.NbChannels = 2;
    CodedDataInConfig.Alpha = 0; // N/A
    CodedDataInConfig.Mono2Stereo = ACC_MME_FALSE; // N/A
    CodedDataInConfig.WordSize = ACC_WS32;
    CodedDataInConfig.AudioMode = ACC_MODE20;
    CodedDataInConfig.SamplingFreq = TranslateIntegerSamplingFrequencyToDiscrete( MixerSamplingFrequency );
    CodedDataInConfig.FirstOutputChan = ACC_MAIN_LEFT; // N/A
    CodedDataInConfig.AutoFade = ACC_MME_FALSE; // N/A
    CodedDataInConfig.Config = 0;
    
    // figure out if any of our outputs are bypassed
    for( unsigned int i=0; i<ActualNumDownstreamCards; i++ )
    {
        PcmPlayer_c::OutputEncoding Encoding = LookupOutputEncoding(i);
        
        if ( PcmPlayer_c::IsOutputBypassed( Encoding ) )
        {
            MIXER_DEBUG("Applying coded data bypass to PCM chain %d\n", i);
            CodedDataInConfig.InputId += ACC_MIXER_COMPRESSED_DATA << 4;
            CodedDataInConfig.Config = i;
            CodedDataInConfig.SamplingFreq = TranslateIntegerSamplingFrequencyToDiscrete( LookupIec60958FrameRate( Encoding ) );
            break;
        }
    }
    
    MME_MixerInputConfig_t & IAudioInConfig = InConfig.Config[MIXER_INTERACTIVE_INPUT];
    IAudioInConfig.InputId = ( ACC_MIXER_IAUDIO << 4) + 3;
    IAudioInConfig.NbChannels = InConfig.Config[PrimaryClient].NbChannels; // same as primary audio
    IAudioInConfig.Alpha = 0xffff;
    IAudioInConfig.Mono2Stereo = ACC_MME_FALSE;
    IAudioInConfig.WordSize = ACC_WS16;
    IAudioInConfig.AudioMode = ACC_MODE_ID;
    IAudioInConfig.SamplingFreq = TranslateIntegerSamplingFrequencyToDiscrete( 
                InteractiveClients[0].Descriptor.sampling_freq ?
                InteractiveClients[0].Descriptor.sampling_freq :
                MixerSamplingFrequency );
    IAudioInConfig.FirstOutputChan = ACC_MAIN_LEFT;
    IAudioInConfig.AutoFade = ACC_MME_FALSE; // no point when it plays all the time
    IAudioInConfig.Config = 0;

//

    MME_LxMixerGainSet_t & InGainConfig = GlobalParams->InGainConfig;
    InGainConfig.Id = ACC_RENDERER_MIXER_BD_GAIN_ID;
    InGainConfig.StructSize = sizeof( InGainConfig );
    
    for( unsigned int i = 0; i < MIXER_MAX_INPUTS; i++ )
    {
	int MuteGainMultiplier = 1;
	
        InGainConfig.GainSet[i].InputId = i;

        if (i < MIXER_MAX_CLIENTS)
            MuteGainMultiplier = SelectGainBasedOnMuteSettings(i, MuteGainMultiplier, 0);
        MuteGainMultiplier = SelectGainBasedOnMuteSettings(MIXER_STAGE_PRE_MIX, MuteGainMultiplier, 0);
        
        for( int j = 0; j < SND_PSEUDO_MIXER_CHANNELS; j++ ) {
            if( i == PrimaryClient )
                InGainConfig.GainSet[i].AlphaPerChannel[j] = Configuration.primary_gain[j];
            else if( i != MIXER_INTERACTIVE_INPUT )
                InGainConfig.GainSet[i].AlphaPerChannel[j] = Configuration.secondary_gain[j];
            else
                InGainConfig.GainSet[i].AlphaPerChannel[j] = Q3_13_UNITY;
            
            InGainConfig.GainSet[i].AlphaPerChannel[j] *= MuteGainMultiplier;
        }
    }
    
//

    MME_LxMixerPanningSet_t & InPanningConfig = GlobalParams->InPanningConfig;
    InPanningConfig.Id = ACC_RENDERER_MIXER_BD_PANNING_ID;
    InPanningConfig.StructSize = sizeof( InPanningConfig );
    
    for( unsigned int i = 0; i < MIXER_MAX_INPUTS; i++ )
    {
        InPanningConfig.PanningSet[i].InputId = i;

        for( int j = 0; j < SND_PSEUDO_MIXER_CHANNELS; j++ )
            if( i == PrimaryClient || i == MIXER_INTERACTIVE_INPUT )
                InPanningConfig.PanningSet[i].Panning[j] = Q3_13_UNITY;
            else
                InPanningConfig.PanningSet[i].Panning[j] = Configuration.secondary_pan[j];
    }

//

    MME_LxMixerInIAudioConfig_t & InIaudioConfig = GlobalParams->InIaudioConfig;
    InIaudioConfig.Id = ACC_RENDERER_MIXER_BD_IAUDIO_ID;
    InIaudioConfig.StructSize = sizeof( InIaudioConfig );
    InIaudioConfig.NbInteractiveAudioInput = MIXER_MAX_INTERACTIVE_CLIENTS;
    
    for( unsigned int i = 0; i < MIXER_MAX_INTERACTIVE_CLIENTS; i++ )
    {
        InIaudioConfig.ConfigIAudio[i].InputId = i;   
        
        if( InteractiveClients[i].State == STARTING || InteractiveClients[i].State == STARTED )
        {
            InIaudioConfig.ConfigIAudio[i].NbChannels = InteractiveClients[i].Descriptor.channels;
            InIaudioConfig.ConfigIAudio[i].Alpha = OutputConfiguration.interactive_gain[i];
            for( int j = 0; j < SND_PSEUDO_MIXER_CHANNELS; j++ )
                InIaudioConfig.ConfigIAudio[i].Panning[j] = OutputConfiguration.interactive_pan[i][j];
            InIaudioConfig.ConfigIAudio[i].Play = ACC_MME_TRUE;
        }
        else
        {
            // we set the gain of unused inputs to zero in order to get a de-pop during startup
            // due to gain smoothing
            InIaudioConfig.ConfigIAudio[i].NbChannels = 1;
            InIaudioConfig.ConfigIAudio[i].Alpha = 0;
            for( int j = 0; j < SND_PSEUDO_MIXER_CHANNELS; j++ )
                InIaudioConfig.ConfigIAudio[i].Panning[j] = 0;
            InIaudioConfig.ConfigIAudio[i].Play = ACC_MME_FALSE;
        }
    }

//

    MME_LxMixerBDGeneral_t & InBDGenConfig = GlobalParams->InBDGenConfig;
    InBDGenConfig.Id = ACC_RENDERER_MIXER_BD_GENERAL_ID;
    InBDGenConfig.StructSize = sizeof( InBDGenConfig );
    InBDGenConfig.PostMixGain = Configuration.post_mix_gain;
    InBDGenConfig.GainSmoothEnable = ACC_MME_TRUE;
    InBDGenConfig.OutputLimiterEnable = ACC_MME_FALSE;

//

    MME_LxMixerOutConfig_t & OutConfig = GlobalParams->OutConfig;
    OutConfig.Id = ACC_RENDERER_MIXER_OUTPUT_CONFIG_ID;
    OutConfig.StructSize = sizeof( OutConfig );
    OutConfig.NbOutputSamplesPerTransform = MixerGranuleSize;
    OutConfig.MainFS = TranslateIntegerSamplingFrequencyToDiscrete( MixerSamplingFrequency );

//

    if(ActualNumDownstreamCards==1) {
        //don't bother if just the one card
        FillOutDevicePcmParameters(GlobalParams->PcmParams[0],0);
    } else {
        //other wise, generate for each card, then optimize
        for(unsigned int n=0;n<ActualNumDownstreamCards;n++) {
            FillOutDevicePcmParameters(NominalPcmParams[n],n);
        }
        AggregatePcmParameters(GlobalParams->PcmParams[0]);
    }

    //As dynamic, we need to fixup the sizes we've declared
    GlobalParams->StructSize -= (sizeof(GlobalParams->PcmParams) - GlobalParams->PcmParams[0].StructSize);

    return PlayerNoError;
}

////////////////////////////////////////////////////////////////////////////
/// 
/// Determine the output encoding to use for the specified device.
///
/// The process of determining the output encoding of a paricular device
/// involved the (slightly complex) conjuction of Mixer_Mme_c::ActualTopology
/// and Mixer_Mme_c::OutputConfiguration . The former describes the 
/// capabilities of an output (is is PCM, SPDIF or HDMI? does it like FatPipe?)
/// whilst the later is the configuration requested by the user (should SPDIF
/// output be AC3 encoded?).
///
/// Since joining the structures is 'quirky' we make damn sure to do it
/// only in one place.
///
/// The freq argument may legitimately be zero (typically when this method is
/// called from sites that don't know or care about the output frequency). In
/// this case the logic that disables certain encodings for unsupported sampling
/// frequencies will be disabled. This was safe at the time of writing but...
///
PcmPlayer_c::OutputEncoding Mixer_Mme_c::LookupOutputEncoding( int dev_num, unsigned int freq )
{
    // this boolean tells whether the output is connected to the hdmi cell
    bool IsConnectedToHdmi        = ActualTopology.card[dev_num].flags & SND_PSEUDO_TOPOLOGY_FLAGS_ENABLE_HDMI_FORMATING;
    // this boolean tells whether the output is connected to a spdif player
    bool IsConnectedToSpdifPlayer = ActualTopology.card[dev_num].flags & SND_PSEUDO_TOPOLOGY_FLAGS_ENABLE_SPDIF_FORMATING;
    // this boolean tells whether the output is a spdif output (i.e. connected to a spdif player, but not to hdmi)
    bool IsConnectedToSpdifOnly = IsConnectedToSpdifPlayer && !IsConnectedToHdmi;

    if( IsConnectedToHdmi || IsConnectedToSpdifPlayer )
    {
        if( PcmPlayer_c::IsOutputBypassed( PrimaryCodedDataType ) && 
            ( (OutputConfiguration.hdmi_bypass && IsConnectedToHdmi) || 
              (OutputConfiguration.spdif_bypass && IsConnectedToSpdifOnly) ) )
        {
            MIXER_DEBUG("Output %d (%s) is bypassed (%s)\n",
                        dev_num, ActualTopology.card[dev_num].alsaname,
                        PcmPlayer_c::LookupOutputEncoding( PrimaryCodedDataType ) );
            return PrimaryCodedDataType;
        }

        enum snd_pseudo_mixer_spdif_encoding RequiredEncoding = IsConnectedToSpdifOnly?OutputConfiguration.spdif_encoding:OutputConfiguration.hdmi_encoding;
        
        switch( RequiredEncoding )
        {
            case SND_PSEUDO_MIXER_SPDIF_ENCODING_PCM:
                MIXER_DEBUG("Output %d (%s) is PCM\n",
                            dev_num, ActualTopology.card[dev_num].alsaname);
                return PcmPlayer_c::OUTPUT_IEC60958;
                
            case SND_PSEUDO_MIXER_SPDIF_ENCODING_AC3:
                if( freq == 0 || ( freq >= MIXER_MIN_FREQ_AC3_ENCODER && freq <= MIXER_MAX_FREQ_AC3_ENCODER ) )
                {
                    MIXER_DEBUG("Output %d (%s) is AC3\n",
                                dev_num, ActualTopology.card[dev_num].alsaname);
                    return PcmPlayer_c::OUTPUT_AC3;
                }
                else
                {
                    MIXER_DEBUG("Output %d (%s) is PCM (because AC3 encoder doesn't support %d hz\n",
                                dev_num, ActualTopology.card[dev_num].alsaname, freq);
                    return PcmPlayer_c::OUTPUT_IEC60958;
                }
                
            case SND_PSEUDO_MIXER_SPDIF_ENCODING_DTS:
                if( freq == 0 || ( freq >= MIXER_MIN_FREQ_DTS_ENCODER && freq <= MIXER_MAX_FREQ_DTS_ENCODER ) )
                {
                    MIXER_DEBUG("Output %d (%s) is DTS\n",
                                dev_num, ActualTopology.card[dev_num].alsaname);
                    return PcmPlayer_c::OUTPUT_DTS;
                }	
                else
                {
                    MIXER_DEBUG("Output %d (%s) is PCM (because DTS encoder doesn't support %d hz\n",
                                dev_num, ActualTopology.card[dev_num].alsaname, freq);
                    return PcmPlayer_c::OUTPUT_IEC60958;
                }
                
            case SND_PSEUDO_MIXER_SPDIF_ENCODING_FATPIPE:
                if( ActualTopology.card[dev_num].flags & SND_PSEUDO_TOPOLOGY_FLAGS_FATPIPE ) {
                    MIXER_DEBUG("Output %d (%s) is FatPipe\n",
                                dev_num, ActualTopology.card[dev_num].alsaname);
                    return PcmPlayer_c::OUTPUT_FATPIPE;
                }
                /*FALLTHRU*/
	
            default:
                MIXER_DEBUG("Output %d (%s) has an invalid encoding (assuming PCM)\n",
                            dev_num, ActualTopology.card[dev_num].alsaname);
                return PcmPlayer_c::OUTPUT_IEC60958;
        }
    }

    // an equivalent if/switch will appear here once we properly
    // support encoded HDMI output...

    MIXER_DEBUG("PCM output %d (%s)\n", dev_num, ActualTopology.card[dev_num].alsaname);
    return PcmPlayer_c::OUTPUT_PCM;
}

////////////////////////////////////////////////////////////////////////////
/// 
/// Determine the output sampling frequency to use for the specified device.
///
/// Basically we consider mixer output frequency and alter it (strictly
/// powers of 2) until the frequency lies within a suitable range for
/// the output topology.
///
/// \todo Strictly speaking there is a cyclic dependancy between
///       Mixer_Mme_c::LookupOutputSamplingFrequency() and
///       Mixer_Mme_c::LookupOutputEncoding(). If the output has to
///       fallback to LPCM there is no need to clamp the frequency
///       below 48khz.
///
unsigned int Mixer_Mme_c::LookupOutputSamplingFrequency( int dev_num )
{
    unsigned int Freq, MixerFreq, MaxFreq;
    PcmPlayer_c::OutputEncoding OutputEncoding = LookupOutputEncoding(dev_num);
    
    MIXER_ASSERT( NominalOutputSamplingFrequency );
    Freq = MixerFreq = NominalOutputSamplingFrequency;

    // start with the contraint imposed by the settings for this output device
    MaxFreq = ActualTopology.card[dev_num].max_freq;

    // check to see if the output encoding imposes any constraints on the sampling frequency
    if (OutputEncoding == PcmPlayer_c::OUTPUT_AC3 && MaxFreq > MIXER_MAX_FREQ_AC3_ENCODER)
	MaxFreq = MIXER_MAX_FREQ_AC3_ENCODER;
    else if (OutputEncoding == PcmPlayer_c::OUTPUT_DTS && MaxFreq > MIXER_MAX_FREQ_DTS_ENCODER)
	MaxFreq = MIXER_MAX_FREQ_DTS_ENCODER;
    else if( PcmPlayer_c::IsOutputBypassed( OutputEncoding ) )
    {
        // the frequency we're calculating is the HDMI pcm player sampling frequency

        Freq = LookupIec60958FrameRate( OutputEncoding );

        if ( OutputEncoding >= PcmPlayer_c::BYPASS_HBRA_LOWEST )
        {
            Freq /= 4;
        }
            
        return (Freq);
    }
    
    // (conditionally) deploy post-mix resampling to meet our contraints
    while( Freq > MaxFreq && Freq > MIXER_MIN_FREQUENCY )
    {
	unsigned int Fprime = Freq / 2;
	if (Fprime * 2 != Freq) {
	    MIXER_ASSERT(0);
	    break;
	}
	Freq = Fprime;
    }

    // verify correct operation of the pre-mix resampling (and that the topology's
    // maximum frequency is not impossible to honour)
    if( Freq < MIXER_MIN_FREQUENCY )
	MIXER_ERROR( "Unexpected mixer output frequency %d (%s)\n",
		     Freq, ( MixerFreq < MIXER_MIN_FREQUENCY ? "pre-mix SRC did not deploy" :
			                                       "max_freq is too aggressive" ) );
    while( Freq < MIXER_MIN_FREQUENCY )
	Freq *= 2;
    
    if( Freq != MixerFreq )
	MIXER_DEBUG( "Requesting post-mix resampling (%d -> %d)\n", MixerFreq, Freq );
    
    return Freq;
}

////////////////////////////////////////////////////////////////////////////
/// 
/// Determine the output number of channels for the specified device.
///
///
unsigned int Mixer_Mme_c::LookupOutputNumberOfChannels( int dev_num )
{
    
    PcmPlayer_c::OutputEncoding Encoding = LookupOutputEncoding(dev_num);

    unsigned int ChannelCount;

    if (Encoding < PcmPlayer_c::BYPASS_LOWEST)
    {
        ChannelCount = ActualTopology.card[dev_num].num_channels;
    }
    else if ( Encoding < PcmPlayer_c::BYPASS_HBRA_LOWEST )
    {
        ChannelCount = 2;
    }
    else
    {
        // high bit rate audio case
        ChannelCount = 8;
    }

    return ChannelCount;
}

#if 0
////////////////////////////////////////////////////////////////////////////
/// 
/// Determine the number of output samples for the specified device.
///
/// This number of samples should be the mixer granule size,
/// except for the hdmi outputs, where it is determined by an 
/// arbitrary repetition period 
///
unsigned int Mixer_Mme_c::LookupOutputNumberOfSamples( int dev_num, unsigned int NominalMixerGranuleSize, 
                                                       unsigned int ActualSampleRateHz, unsigned int NominalOutputSamplingFrequency )
{
    PcmPlayer_c::OutputEncoding Encoding = LookupOutputEncoding(dev_num);

    unsigned int ClockRatio, NumberOfSamples;
    
    if( PcmPlayer_c::IsOutputBypassed( Encoding ) )
    {
        ClockRatio = ActualSampleRateHz/NominalOutputSamplingFrequency;
    }
    else
    {
        ClockRatio = 1;
    }
    
    NumberOfSamples = NominalMixerGranuleSize * ClockRatio;

    MIXER_DEBUG("# of output samples of card %d: %d\n", dev_num, NumberOfSamples);
    
    return NumberOfSamples;
}
#endif

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
/// a seperate method.
///
inline void Mixer_Mme_c::FillOutDeviceDownmixParameters(
	MME_LxPcmPostProcessingGlobalParameters_Frozen_t & PcmParams,  int dev_num, bool EnableDMix )
{
    unsigned int min_index = 0, max_index = 0, mid_index = 0;
    uint64_t TargetSortValue = 0, CurrentSortValue = 0;
    bool TraceDownmixLookups;

    // put these are the top because their values are very transient - don't expect them to maintain their
    // values between blocks...
    union {
	struct snd_pseudo_mixer_channel_assignment ca;
	uint32_t n;
    } TargetOutputId, TargetInputId, OutputId, InputId; 
 
    // Initialize the donwmixer structure before population

    MME_DMixGlobalParams_t & DMix 	= PcmParams.Dmix;
    DMix.Id 				= PCMPROCESS_SET_ID(ACC_PCM_DMIX_ID, dev_num);
    DMix.Apply 				= ( EnableDMix ? ACC_MME_ENABLED : ACC_MME_DISABLED );
    DMix.StructSize 			= sizeof( DMix );
    DMix.Config[DMIX_USER_DEFINED] 	= ACC_MME_FALSE;
    DMix.Config[DMIX_STEREO_UPMIX] 	= ACC_MME_FALSE;
    DMix.Config[DMIX_MONO_UPMIX] 	= ACC_MME_FALSE;
    DMix.Config[DMIX_MEAN_SURROUND] 	= ACC_MME_FALSE;
    DMix.Config[DMIX_SECOND_STEREO] 	= ACC_MME_FALSE;
    DMix.Config[DMIX_MIX_LFE] 		= ACC_MME_FALSE;
    DMix.Config[DMIX_NORMALIZE] 	= ACC_MME_TRUE;
    DMix.Config[DMIX_NORM_IDX] 		= 0;
    DMix.Config[DMIX_DIALOG_ENHANCE] 	= ACC_MME_FALSE;

    // table already initialized to null
    
    //for (int i=0; i<DMIX_NB_IN_CHANNELS; i++)
	//for (int j=0; j<DMIX_NB_IN_CHANNELS; j++)
	  //  DMix.MainMixTable[i][j] = 0000;

    PcmParams.StructSize += sizeof( DMix );

    // if the downmixer isn't enabled we're done
   
    if (!EnableDMix) return;

    // get output and input audio modes

    enum eAccAcMode OutputMode 	= (enum eAccAcMode) PcmParams.CMC.Config[CMC_OUTMODE_MAIN];
    enum eAccAcMode InputMode 	= LookupMixerAudioMode(); 
    
    MIXER_ASSERT(ACC_MODE_ID == PcmParams.CMC.Config[CMC_OUTMODE_AUX]); // no auxillary output at the moment

    /* STEREO INPUT AND ALL SPEAKER ENABLED */
    
    if ((InputMode == ACC_MODE20) && (OutputConfiguration.all_speaker_stereo_enable))
    {
	DMix.Config[DMIX_USER_DEFINED] = ACC_MME_TRUE;

	DMix.MainMixTable[0][0] = Q15_UNITY; 
	DMix.MainMixTable[1][1] = Q15_UNITY; 
	DMix.MainMixTable[2][0] = Q15_UNITY/2; 
	DMix.MainMixTable[2][1] = Q15_UNITY/2; 
	DMix.MainMixTable[4][0] = Q15_UNITY; 
	DMix.MainMixTable[5][1] = Q15_UNITY; 	

	// one single rear central channel 
	
	if ((OutputMode == ACC_MODE21) || (OutputMode == ACC_MODE31) || 
		(OutputMode == ACC_MODE21_LFE) || (OutputMode == ACC_MODE31_LFE)) 
	{
	    DMix.MainMixTable[4][0] = Q15_UNITY/2; 
	    DMix.MainMixTable[4][1] = Q15_UNITY/2; 
	}
	    
	MIXER_DEBUG("Used custom UpMix table for %s to %s\n",
		    LookupAudioMode(InputMode), LookupAudioMode(OutputMode));
	for (int i=0; i<DMIX_NB_IN_CHANNELS; i++)
	    MIXER_DEBUG("%04x %04x %04x %04x %04x %04x %04x %04x\n",
	            DMix.MainMixTable[i][0], DMix.MainMixTable[i][1],
	            DMix.MainMixTable[i][2], DMix.MainMixTable[i][3],
		    DMix.MainMixTable[i][4], DMix.MainMixTable[i][5],
		    DMix.MainMixTable[i][6], DMix.MainMixTable[i][7]);

	return;
    }
    
    /* OTHER CASES */
    
    // if we don't need to honour the downmix 'firmware' then we're done
    
    if (!DownmixFirmware) return;
    
    // find proper modes = consider speaker configuration

    TargetOutputId.ca = TranslateAudioModeToChannelAssignment(OutputMode);
    TargetInputId.ca = TranslateAudioModeToChannelAssignment(InputMode);
    TargetSortValue = ((uint64_t) TargetInputId.n << 32) + TargetOutputId.n;

    // update some values to use later

    TraceDownmixLookups = OutputMode != LastOutputMode[dev_num] || InputMode != LastOutputMode[dev_num];
    LastOutputMode[dev_num] = OutputMode;
    LastInputMode[dev_num] = InputMode;

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
	
	if( CurrentSortValue < TargetSortValue )
	    min_index = mid_index + 1;
	else
	    max_index = mid_index;
    }
    MIXER_ASSERT(min_index == max_index);
    
    // regenerate the sort value; it is state if CurrentSortValue < TargetSortValue during final iteration

    InputId.ca 	     = DownmixFirmware->index[min_index].input_id;
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
    if (TargetOutputId.ca.malleable && OutputId.ca.malleable) {
	OutputMode = TranslateChannelAssignmentToAudioMode(OutputId.ca);
	MIXER_TRACE("Downmix firmware requested %s as malleable output surface\n",
		  LookupAudioMode(OutputMode));

	// update the CMC configuration with the value found in the firmware
	PcmParams.CMC.Config[CMC_OUTMODE_MAIN] = OutputMode;

	// update the target value since now we know what we were looking for
	TargetSortValue = CurrentSortValue;
    }

    // fill the rest of the dowmixer table with the proper coefficients

    if( CurrentSortValue == TargetSortValue )
    {
	struct snd_pseudo_mixer_downmix_index *index = DownmixFirmware->index + min_index;
	snd_pseudo_mixer_downmix_Q15 *data = (snd_pseudo_mixer_downmix_Q15 *)(DownmixFirmware->index + 
		DownmixFirmware->header.num_index_entries);
	snd_pseudo_mixer_downmix_Q15 *table = data + index->offset;
	
	DMix.Config[DMIX_USER_DEFINED] = ACC_MME_TRUE;

	for (int x=0; x<index->output_dimension; x++)
		for (int y=0; y<index->input_dimension; y++)
		    	DMix.MainMixTable[x][y] = table[x * index->input_dimension + y];

	MIXER_DEBUG("Found custom downmix table for %s to %s\n",
		    LookupAudioMode(InputMode), LookupAudioMode(OutputMode));
	for (int i=0; i<DMIX_NB_IN_CHANNELS; i++)
	    MIXER_DEBUG("%04x %04x %04x %04x %04x %04x %04x %04x\n",
	            DMix.MainMixTable[i][0], DMix.MainMixTable[i][1],
	            DMix.MainMixTable[i][2], DMix.MainMixTable[i][3],
		    DMix.MainMixTable[i][4], DMix.MainMixTable[i][5],
		    DMix.MainMixTable[i][6], DMix.MainMixTable[i][7]);
    }
    else
    {
	// not an error but certainly worthy of note
	if( TraceDownmixLookups )
	    MIXER_TRACE("Downmix firmware has no entry for %s to %s - using defaults\n",
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
/// This method is, semanticlly, part of FillOutDevicePcmParameters() (hence
/// it is inlined) but is quite complex enough to justify spliting it out into
/// a seperate method.
///
/// The primary responsibility of the function is to set dynamically changing
/// fields (such as the sample rate), static fields are provided by userspace.
/// The general strategy taken to merge the two sources is that is that where 
/// fields are supplied by the implementation (e.g. sample rate,
/// LPCM/other, emphasis) we ignore the values provided by userspace. Otherwise
/// we make every effort to reflect the userspace values, we make no effort
/// to validate their correctness.
///
inline void Mixer_Mme_c::FillOutDeviceSpdifParameters(
			MME_LxPcmPostProcessingGlobalParameters_Frozen_t & PcmParams,
			int dev_num, PcmPlayer_c::OutputEncoding OutputEncoding )
{

// A convenience macro to allow the entries below to be copied from the IEC
// standards document. The sub-macro chops of the high bits (the ones that
// select which word within the channel mask we need). The full macro then
// performs an endian swap since the firmware expects big endian values.
#define B(x) (((_B(x) & 0xff) << 24) | ((_B(x) & 0xff00) << 8) | ((_B(x) >> 8)  & 0xff00) | ((_B(x) >> 24) & 0xff))
#define _B(x) (1 << ((x) % 32))

    const unsigned int use_of_channel_status_block = B(0);
    const unsigned int linear_pcm_identification = B(1);
    //const unsigned int copyright_information = B(2);
    const unsigned int additional_format_information = B(3) | B(4) | B(5);
    const unsigned int mode = B(6) | B(7);
    //const unsigned int category_code = 0x0000ff00;
    //const unsigned int source_number = B(16) | B(17) | B(18) | B(19);
    //const unsigned int channel_number = B(20) | B(21) | B(22) | B(23);
    const unsigned int sampling_frequency = B(24) | B(25) | B(26) | B(27);
    //const unsigned int clock_accuracy = B(28) | B(29);
    const unsigned int word_length = B(32) | B(33) | B(34) | B(35);
    //const unsigned int original_sampling_frequency = B(36) | B(37) | B(38) | B(39);
    //const unsigned int cgms_a = B(40) | B(41);
#undef B
#undef _B
    
    const int STSZ = 6;
    unsigned int ChannelStatusMask[STSZ];
    
    //
    // Special case FatPipe which uses a different parameter structure and handles channel status
    // differently.
    //
    
    if( OutputEncoding == PcmPlayer_c::OUTPUT_FATPIPE )
    {
        MME_FatpipeGlobalParams_t & FatPipe = PcmParams.FatPipeOrSpdifOut;
        StreamMetadata_t * MetaData = &Clients[PrimaryClient].Parameters.StreamMetadata;

        FatPipe.Id = PCMPROCESS_SET_ID(ACC_PCM_SPDIFOUT_ID, dev_num);
        FatPipe.StructSize = sizeof( MME_FatpipeGlobalParams_t );
        PcmParams.StructSize += sizeof( MME_FatpipeGlobalParams_t );
        FatPipe.Apply = ACC_MME_ENABLED;
        FatPipe.Config.Mode = 1; // FatPipe mode
        FatPipe.Config.UpdateSpdifControl = 1;
        FatPipe.Config.UpdateMetaData = 1;
        FatPipe.Config.Upsample4x = 1;
        FatPipe.Config.ForceChannelMap = 0;

        // metadata gotten from the userspace
        FatPipe.Config.FEC = ((OutputConfiguration.fatpipe_metadata.md[0] >> 6) & 1);
        FatPipe.Config.Encryption = ((OutputConfiguration.fatpipe_metadata.md[0] >> 5) & 1);
        FatPipe.Config.InvalidStream = ((OutputConfiguration.fatpipe_metadata.md[0] >> 4) & 1);
        FatPipe.MetaData.LevelShiftValue = OutputConfiguration.fatpipe_metadata.md[6] & 31;
#if PCMPROCESSINGS_API_VERSION >= 0x100415
	FatPipe.MetaData.Effects = OutputConfiguration.fatpipe_metadata.md[14] & 0xffff;
#endif

        // metadata gotten from the primary stream itself (AC3/DTS)
        FatPipe.MetaData.Mixlevel = MetaData->MixLevel;
        FatPipe.MetaData.DialogHeadroom = MetaData->DialogNorm;
        FatPipe.MetaData.FrontMatrixEncoded = MetaData->FrontMatrixEncoded;
        FatPipe.MetaData.RearMatrixEncoded= MetaData->RearMatrixEncoded;
        FatPipe.MetaData.Lfe_Gain = MetaData->LfeGain;

	memset( FatPipe.Spdifout.Validity, 0xff, sizeof( FatPipe.Spdifout.Validity ));
	
	// FatPipe is entirely prescriptitive w.r.t the channel status bits so we can ignore the userspace
	// provided values entirely
		
	/* consumer, not linear PCM, copyright is asserted, mode 00, pcm encoder,
	 * no generation indication, channel 0, source 0, level II clock accuracy,
	 * ?44.1KHz?, 24-bit
	 */
        FatPipe.Spdifout.ChannelStatus[0] = 0x02020000;
	FatPipe.Spdifout.ChannelStatus[1] = 0x0B000000; // report 24bits wordlength
        
        return;
    }

    //
    // Copy the userspace mask for the channel status bits. This consists mostly
    // of set bits.
    //
    
    for (int i=0; i<STSZ; i++)
	ChannelStatusMask[i] = OutputConfiguration.iec958_mask.status[i*4 + 0] << 24 |
	      		       OutputConfiguration.iec958_mask.status[i*4 + 1] << 16 |
	      		       OutputConfiguration.iec958_mask.status[i*4 + 2] <<  8|
	      		       OutputConfiguration.iec958_mask.status[i*4 + 3] <<  0;
    
    // these should never be overlaid
    ChannelStatusMask[0] &= ~( use_of_channel_status_block |
                               linear_pcm_identification |
                               additional_format_information | /* auto fill in for PCM, 000 for coded data */
                               mode |
                               sampling_frequency ); /* auto fill in */
    ChannelStatusMask[1] &= ~( word_length );
    
    
    //
    // Handle, in a unified manner, all the IEC SPDIF formatings
    //
    
    MME_SpdifOutGlobalParams_t & SpdifOut = *((MME_SpdifOutGlobalParams_t *) (&PcmParams.FatPipeOrSpdifOut));
    SpdifOut.Id = PCMPROCESS_SET_ID(ACC_PCM_SPDIFOUT_ID, dev_num);
    SpdifOut.StructSize = sizeof( MME_FatpipeGlobalParams_t ); // use fatpipe size (to match frozen structure)
    PcmParams.StructSize += sizeof( MME_FatpipeGlobalParams_t ); // use fatpipe size
    
    if( OutputEncoding == PcmPlayer_c::OUTPUT_IEC60958 || OutputEncoding == PcmPlayer_c::BYPASS_DTS_CDDA )
    {
        SpdifOut.Apply = ACC_MME_ENABLED;
        SpdifOut.Config.Mode = 0; // SPDIF mode
        SpdifOut.Config.UpdateSpdifControl = 1;
        SpdifOut.Config.SpdifCompressed = 0;
        SpdifOut.Config.AddIECPreamble = 0;
        SpdifOut.Config.ForcePC = 0; // useless in this case: for compressed mode only
        
        SpdifOut.Spdifout.ChannelStatus[0] = 0x00000000; /* consumer, linear PCM, mode 00 */
        if( OutputEncoding == PcmPlayer_c::OUTPUT_IEC60958 )
            SpdifOut.Spdifout.ChannelStatus[1] = 0x0b000000; /* 24-bit word length */
        else
            SpdifOut.Spdifout.ChannelStatus[1] = 0x00000000; /* word length not indiciated */

    }
    else if( OutputEncoding == PcmPlayer_c::OUTPUT_AC3 || OutputEncoding == PcmPlayer_c::BYPASS_AC3 || 
             OutputEncoding == PcmPlayer_c::BYPASS_DDPLUS )
    {       
        SpdifOut.Apply = ACC_MME_ENABLED;
        SpdifOut.Config.Mode = 0; // SPDIF mode
        SpdifOut.Config.UpdateSpdifControl = 1;
        SpdifOut.Config.UpdateMetaData = 1; // this is supposed to be FatPipe only but the examples set it...
        SpdifOut.Config.SpdifCompressed = 1;
        SpdifOut.Config.AddIECPreamble = (OutputEncoding == PcmPlayer_c::OUTPUT_AC3)?0:1;;
        SpdifOut.Config.ForcePC = 1; // compressed mode: firmware will get the stream type from the to Preamble_PC below
        
        if ((OutputEncoding == PcmPlayer_c::OUTPUT_AC3) || (Clients[dev_num].Parameters.OriginalEncoding == AudioOriginalEncodingDdplus))
        {
            SpdifOut.Config.Endianness = 1; // little endian input frames
        }
        
        memset( SpdifOut.Spdifout.Validity, 0xff, sizeof( SpdifOut.Spdifout.Validity ));

        SpdifOut.Spdifout.ChannelStatus[0] = 0x02000000; /* consumer, not linear PCM, mode 00 */
        SpdifOut.Spdifout.ChannelStatus[1] = 0x02000000; /* 16-bit word length */
        
        SpdifOut.Spdifout.Preamble_PA = 0xf872;
        SpdifOut.Spdifout.Preamble_PB = 0x4e1f;
        SpdifOut.Spdifout.Preamble_PC = LookupSpdifPreamblePc( OutputEncoding );
    }
    else if( OutputEncoding == PcmPlayer_c::OUTPUT_DTS || OutputEncoding == PcmPlayer_c::BYPASS_DTS_512 ||
             OutputEncoding == PcmPlayer_c::BYPASS_DTS_1024 || OutputEncoding == PcmPlayer_c::BYPASS_DTS_2048 ||
             (OutputEncoding >= PcmPlayer_c::BYPASS_DTSHD_LBR && OutputEncoding <= PcmPlayer_c::BYPASS_DTSHD_DTS_8192 ) ||
             OutputEncoding == PcmPlayer_c::BYPASS_DTSHD_MA )
    {
        SpdifOut.Apply = ACC_MME_ENABLED;
        SpdifOut.Config.Mode = 0; // SPDIF mode
        SpdifOut.Config.UpdateSpdifControl = 1;
        SpdifOut.Config.UpdateMetaData = 1; // this is supposed to be FatPipe only but the examples set it...
        // in case of DTS encoding, the preambles are set by the encoder itself
        SpdifOut.Config.SpdifCompressed = 1;
        SpdifOut.Config.AddIECPreamble = (OutputEncoding == PcmPlayer_c::OUTPUT_DTS)?0:1;
        SpdifOut.Config.ForcePC = 1; // compressed mode: firmware will get the stream type from the to Preamble_PC below
        if (OutputEncoding == PcmPlayer_c::OUTPUT_DTS)
        {
            SpdifOut.Config.Endianness = 1; // little endian input frames
        }
        
        memset( SpdifOut.Spdifout.Validity, 0xff, sizeof( SpdifOut.Spdifout.Validity ));

        SpdifOut.Spdifout.ChannelStatus[0] = 0x02000000; /* consumer, not linear PCM, mode 00 */
        SpdifOut.Spdifout.ChannelStatus[1] = 0x02000000; /* 16-bit word length */
        
        SpdifOut.Spdifout.Preamble_PA = 0xf872;
        SpdifOut.Spdifout.Preamble_PB = 0x4e1f;
        SpdifOut.Spdifout.Preamble_PC = LookupSpdifPreamblePc( OutputEncoding );
    }
    else if( OutputEncoding == PcmPlayer_c::BYPASS_TRUEHD )
    {       
        /// TODO not supported yet by the firmware...
        MIXER_TRACE("TrueHD bypassing is not supported yet by the firmware...\n");
        SpdifOut.Apply = ACC_MME_ENABLED;
        SpdifOut.Config.Mode = 0; // SPDIF mode
        SpdifOut.Config.UpdateSpdifControl = 1;
        SpdifOut.Config.UpdateMetaData = 1; // this is supposed to be FatPipe only but the examples set it...
        SpdifOut.Config.SpdifCompressed = 1;
        SpdifOut.Config.AddIECPreamble = 1;;
        SpdifOut.Config.ForcePC = 1; // compressed mode: firmware will get the stream type from the to Preamble_PC below
        
        //
        SpdifOut.Config.Endianness = 1; // big endian input frames: check with the MAT engine
        
        memset( SpdifOut.Spdifout.Validity, 0xff, sizeof( SpdifOut.Spdifout.Validity ));

        SpdifOut.Spdifout.ChannelStatus[0] = 0x02000000; /* consumer, not linear PCM, mode 00 */
        SpdifOut.Spdifout.ChannelStatus[1] = 0x02000000; /* 16-bit word length */
        
        SpdifOut.Spdifout.Preamble_PA = 0xf872;
        SpdifOut.Spdifout.Preamble_PB = 0x4e1f;
        SpdifOut.Spdifout.Preamble_PC = LookupSpdifPreamblePc( OutputEncoding );
    }
    else
    {
        SpdifOut.Apply = ACC_MME_DISABLED;
    }
    
    //
    // The mask is now complete so we can overlay the userspace supplied channel
    // status bits.
    //
    
    for (int i=0; i<STSZ; i++)
	SpdifOut.Spdifout.ChannelStatus[i] |=
	    ChannelStatusMask[i] &
	    ( OutputConfiguration.iec958_metadata.status[i*4 + 0] << 24 |
	      OutputConfiguration.iec958_metadata.status[i*4 + 1] << 16 |
	      OutputConfiguration.iec958_metadata.status[i*4 + 2] <<  8|
	      OutputConfiguration.iec958_metadata.status[i*4 + 3] <<  0 );
    if( ActualTopology.card[dev_num].flags & SND_PSEUDO_TOPOLOGY_FLAGS_ENABLE_HDMI_FORMATING )
    {
        if (ConfigureHDMICell(OutputEncoding, ActualTopology.card[dev_num].flags) != PlayerNoError)
        {
            MIXER_ERROR("Error when configuring the hdmi cell\n");
        }
    }
}


////////////////////////////////////////////////////////////////////////////
/// 
/// Fill out the PCM post-processing required for a single physical output.
///
PlayerStatus_t Mixer_Mme_c::FillOutDevicePcmParameters( MME_LxPcmPostProcessingGlobalParameters_Frozen_t &
								PcmParams , int dev_num )
{
    const int PER_SPEAKER_DELAY_NULL = 0;
    unsigned int SampleRateHz = LookupOutputSamplingFrequency( dev_num );
    // we must send the sample rate here since we need the conditional disabling of the encoders...
    PcmPlayer_c::OutputEncoding OutputEncoding = LookupOutputEncoding( dev_num, SampleRateHz );
    
    //
    // Summarise the processing that are enabled
    //
    
    bool EnableBassMgt = !OutputConfiguration.bass_mgt_bypass;
    bool EnableDcRemove = OutputConfiguration.dc_remove_enable;
    // the encoder choice complex - this is determined later
    bool EnableDMix = true;
    bool EnableLimiter = true;
    
    //
    // Update the enables if we are in a bypassed mode
    //
    
    if( PcmPlayer_c::IsOutputBypassed( OutputEncoding ) )
    {
        EnableBassMgt = false;
        EnableDcRemove = false;
        EnableDMix = false;
        EnableLimiter = false;
    }
    

    memset(&PcmParams, 0, sizeof( PcmParams ));

    PcmParams.Id = ACC_RENDERER_MIXER_POSTPROCESSING_ID;
    PcmParams.StructSize = 8; //sizeof( PcmParams ); Just the header - we'll increment as go along

//these next two would be relevant if doing a single card
    PcmParams.NbPcmProcessings = 0;	//!< NbPcmProcessings on main[0..3] and aux[4..7]
    PcmParams.AuxSplit = SPLIT_AUX;		//! Point of split between Main output and Aux output 

    MME_BassMgtGlobalParams_t & BassMgt = PcmParams.BassMgt;
    BassMgt.Id = PCMPROCESS_SET_ID(ACC_PCM_BASSMGT_ID, dev_num);
    BassMgt.StructSize = sizeof( BassMgt );
    PcmParams.StructSize += sizeof( BassMgt );
    if( !EnableBassMgt )
    {
        BassMgt.Apply = ACC_MME_DISABLED;
    }
    else
    {
        const int BASSMGT_ATTENUATION_MUTE = 70;

        BassMgt.Apply = ACC_MME_ENABLED;
        BassMgt.Config[BASSMGT_TYPE] = BASSMGT_VOLUME_CTRL;
        BassMgt.Config[BASSMGT_LFE_OUT] = ACC_MME_TRUE;
        BassMgt.Config[BASSMGT_BOOST_OUT] = ACC_MME_DISABLED;
        BassMgt.Config[BASSMGT_PROLOGIC_IN] = ACC_MME_TRUE;
        BassMgt.Config[BASSMGT_OUT_WS] = 32;
        BassMgt.Config[BASSMGT_LIMITER] = 0;
        BassMgt.Config[BASSMGT_INPUT_ROUTE] = ACC_MIX_MAIN;
        BassMgt.Config[BASSMGT_OUTPUT_ROUTE] = ACC_MIX_MAIN;
        // this is rather naughty (the gain values are linear everywhere else
        // but are mapped onto a logarithmic scale here)
        memset(BassMgt.Volume, BASSMGT_ATTENUATION_MUTE, sizeof(BassMgt.Volume));
        for( int i = 0; i < SND_PSEUDO_MIXER_CHANNELS; i++ )
            BassMgt.Volume[i] = -OutputConfiguration.master_volume[i];
        BassMgt.DelayUpdate = ACC_MME_FALSE;
        // From ACC-BL023-3BD
	BassMgt.CutOffFrequency = 100; // within [50, 200] Hz 
	BassMgt.FilterOrder = 2;       // could be 1 or 2 .
    }

    MME_EqualizerGlobalParams_t & Equalizer = PcmParams.Equalizer;
    Equalizer.Id = PCMPROCESS_SET_ID(ACC_PCM_EQUALIZER_ID, dev_num);
    Equalizer.StructSize = sizeof( Equalizer );
    PcmParams.StructSize += sizeof( Equalizer );
    Equalizer.Apply = ACC_MME_DISABLED;

    MME_TempoGlobalParams_t & TempoControl = PcmParams.TempoControl;
    TempoControl.Id = PCMPROCESS_SET_ID(ACC_PCM_TEMPO_ID, dev_num);
    TempoControl.StructSize = sizeof( TempoControl );
    PcmParams.StructSize += sizeof( TempoControl );
    TempoControl.Apply = ACC_MME_DISABLED;

    MME_DCRemoveGlobalParams_t & DCRemove = PcmParams.DCRemove;
    DCRemove.Id = PCMPROCESS_SET_ID(ACC_PCM_DCREMOVE_ID, dev_num);
    DCRemove.StructSize = sizeof( DCRemove );
    PcmParams.StructSize += sizeof( DCRemove );
    DCRemove.Apply = ( EnableDcRemove ? ACC_MME_ENABLED : ACC_MME_DISABLED );

    MME_DelayGlobalParams_t & Delay = PcmParams.Delay;
    Delay.Id = PCMPROCESS_SET_ID(ACC_PCM_DELAY_ID, dev_num);
    Delay.StructSize = sizeof( Delay );
    PcmParams.StructSize += sizeof( Delay );
    Delay.Apply = ACC_MME_ENABLED;
    Delay.DelayUpdate = ACC_MME_TRUE;
    memset(Delay.Delay, PER_SPEAKER_DELAY_NULL, sizeof(Delay.Delay));
    for( int i = 0; i < SND_PSEUDO_MIXER_CHANNELS; i++ )
    {
	Delay.Delay[i] = OutputConfiguration.chain_delay[dev_num][i] / 1000;
	MIXER_DEBUG("Delay.Delay[%d] = %d\n", i, Delay.Delay[i]);
    }
    
    MME_EncoderPPGlobalParams_t & Encoder = PcmParams.Encoder;
    Encoder.Id = PCMPROCESS_SET_ID(ACC_PCM_ENCODER_ID, dev_num);
    Encoder.StructSize = sizeof( Encoder );
    PcmParams.StructSize += sizeof( Encoder );

    // maybe setup AC3/etc encoding if SPDIF device.
    switch( OutputEncoding )
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
    	Encoder.Apply = ACC_MME_DISABLED;
	break;

    case PcmPlayer_c::OUTPUT_AC3:
	Encoder.Apply = ACC_MME_ENABLED;
	Encoder.Type = ACC_ENCODERPP_DDCE;	//type=bits0-3, subtype=bits4-7
    // let the encoder insert preambles
    ACC_ENCODERPP_SET_IECENABLE(((MME_EncoderPPGlobalParams_t*)&Encoder), 1);
	Encoder.BitRate = 448;           //DDCE:448000bps DTS:1536000bps
	break;

    case PcmPlayer_c::OUTPUT_DTS:
	Encoder.Apply = ACC_MME_ENABLED;
	Encoder.Type = ACC_ENCODERPP_DTSE;	//type=bits0-3, subtype=bits4-7
    // let the encoder insert preambles
    ACC_ENCODERPP_SET_IECENABLE(((MME_EncoderPPGlobalParams_t*)&Encoder), 1);
	Encoder.BitRate = 1536000;          //DDCE:448000bps DTS:1536000bps
	break;

    case PcmPlayer_c::OUTPUT_FATPIPE:
	Encoder.Apply = ACC_MME_DISABLED;
	break;

    default:
	MIXER_ASSERT(0);
    }

//

    MME_SfcPPGlobalParams_t & Sfc = PcmParams.Sfc;
    Sfc.Id = PCMPROCESS_SET_ID(ACC_PCM_SFC_ID, dev_num);
    Sfc.StructSize = sizeof( Sfc );
    PcmParams.StructSize += sizeof( Sfc );
    Sfc.Apply = ACC_MME_DISABLED;

//
    
    MME_Resamplex2GlobalParams_t & Resamplex2 = PcmParams.Resamplex2;
    Resamplex2.Id = PCMPROCESS_SET_ID(ACC_PCM_RESAMPLE_ID, dev_num);
    Resamplex2.StructSize = sizeof( Resamplex2 );
    PcmParams.StructSize += sizeof( Resamplex2 );
    if( ((SampleRateHz == MixerSamplingFrequency) &&
         (OutputEncoding != PcmPlayer_c::OUTPUT_AC3) && (OutputEncoding != PcmPlayer_c::OUTPUT_DTS)) ||
        PcmPlayer_c::IsOutputBypassed( OutputEncoding ) )
    {
        Resamplex2.Apply = ACC_MME_DISABLED;
    }
    else
    {
        enum eFsRange FsRange;
        enum eAccProcessApply ProcessApply;
        FsRange = TranslateIntegerSamplingFrequencyToRange( SampleRateHz );

        if ((OutputEncoding == PcmPlayer_c::OUTPUT_AC3) || (OutputEncoding == PcmPlayer_c::OUTPUT_DTS))
        {
            // When the outputs are encoded then we have to deploy resampling at the beginning of the PCM
            // chain. Thus if the output rate differs from the mixers native rate then we need to deploy.
            ProcessApply = ( SampleRateHz != MixerSamplingFrequency ? ACC_MME_ENABLED : ACC_MME_DISABLED );
        }
        else
        {
            // it is important to use _AUTO here (rather than conditionally selecting _ENABLE ourselves)
            // because _AUTO also leaves the firmware free to apply the resampling at the point it believes is
            // best.using _ENABLE causes the processing to unconditionally applied at the start of the chain.   
            ProcessApply = ACC_MME_AUTO;
        }

        MIXER_DEBUG("SampleRateHz %d  FsRange %d  ACC_FSRANGE_48k %d  ProcessApply %d\n",
        	    SampleRateHz, FsRange, ACC_FSRANGE_48k, ProcessApply);

        Resamplex2.Apply = ProcessApply;
        Resamplex2.Range = FsRange;
    }
    
    MIXER_DEBUG( "%s post-mix resampling (%s)\n",
                 (Resamplex2.Apply == ACC_MME_ENABLED ? "Enabled" :
                  Resamplex2.Apply == ACC_MME_AUTO ? "Automatic" : "Disabled"),
		 LookupDiscreteSamplingFrequencyRange( Resamplex2.Range ) );
    
//

    MME_CMCGlobalParams_t & CMC = PcmParams.CMC;
    CMC.Id = PCMPROCESS_SET_ID(ACC_PCM_CMC_ID, dev_num);
    CMC.StructSize = sizeof( CMC );
    PcmParams.StructSize += sizeof( CMC );

    CMC.Config[CMC_DUAL_MODE] = 0; /*DUAL_LR */ ;	// not applied
    CMC.Config[CMC_PCM_DOWN_SCALED] = ACC_MME_FALSE;
    CMC.CenterMixCoeff = ACC_M3DB;
    CMC.SurroundMixCoeff = ACC_M3DB;
    
    switch( OutputEncoding )
    {
    case PcmPlayer_c::OUTPUT_FATPIPE:
	/* FatPipe is a malleable output surface */
	CMC.Config[CMC_OUTMODE_MAIN] = LookupFatPipeOutputMode(LookupMixerAudioMode());
	CMC.Config[CMC_OUTMODE_AUX] = ACC_MODE_ID;
	break;

    case PcmPlayer_c::OUTPUT_AC3:
    case PcmPlayer_c::OUTPUT_DTS:
	CMC.Config[CMC_OUTMODE_MAIN] = ACC_MODE32_LFE; /* traditional 5.1 */
	CMC.Config[CMC_OUTMODE_AUX] = ACC_MODE_ID;
	break;

    default:
	CMC.Config[CMC_OUTMODE_MAIN] = TranslateDownstreamCardToMainAudioMode( &ActualTopology.card[dev_num] );
	
#ifndef BUG_4518
	// The downmixer in the current firmware doesn't support 7.1 outputs (can't automatically generate
	// downmix tables for them). We therefore need to fall back to ACC_MODE32(_LFE) unless the decoder
	// is already chucking out samples in the correct mode.
	if( ACC_MODE34 == CMC.Config[CMC_OUTMODE_MAIN] || ACC_MODE34_LFE == CMC.Config[CMC_OUTMODE_MAIN] )
	{
	    eAccAcMode DecoderMode = LookupMixerAudioMode();
	    
	    if( CMC.Config[CMC_OUTMODE_MAIN] != DecoderMode )
	    {
		if( ACC_MODE34_LFE == CMC.Config[CMC_OUTMODE_MAIN] )
		{
		    if( ACC_MODE34 == DecoderMode )
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
	// The mixer doesn't support auxilliary outputs in the current firmware 
	CMC.Config[CMC_OUTMODE_AUX] = ACC_MODE_ID;
#else
	CMC.Config[CMC_OUTMODE_AUX] = TranslateDownstreamCardToAuxAudioMode( &ActualTopology.card[dev_num] );
#endif
	break;
    }

    MIXER_DEBUG( "CMC output mode (%d): %d channels  %s and %s (main and auxilliary)\n",
	         dev_num, ActualTopology.card[dev_num].num_channels,
	         LookupAudioMode( (eAccAcMode) CMC.Config[CMC_OUTMODE_MAIN] ),
	         LookupAudioMode( (eAccAcMode) CMC.Config[CMC_OUTMODE_AUX] ) );

//
    
    FillOutDeviceDownmixParameters( PcmParams, dev_num, EnableDMix );

//
    
    FillOutDeviceSpdifParameters( PcmParams, dev_num, OutputEncoding );

//

    MME_LimiterGlobalParams_t&	Limiter = PcmParams.Limiter;
    Limiter.Id = PCMPROCESS_SET_ID(ACC_PCM_LIMITER_ID, dev_num);
    Limiter.StructSize = sizeof( Limiter );
    PcmParams.StructSize += sizeof( Limiter );
	Limiter.Apply = ( EnableLimiter ? ACC_MME_ENABLED : ACC_MME_DISABLED );
    if ((dev_num == 0) || (OutputConfiguration.chain_limiter_enable[dev_num] == false))
	Limiter.LimiterEnable = ACC_MME_FALSE;
    else
	Limiter.LimiterEnable = ACC_MME_TRUE;

    // soft mute is triggered by a 0 to 1 transition, and unmute by a 1 to 0 transition
    Limiter.SoftMute = (OutputConfiguration.chain_enable[dev_num] ? 0 : 1);
    Limiter.SoftMute = SelectGainBasedOnMuteSettings( MIXER_STAGE_POST_MIX, Limiter.SoftMute, 1 );

    Limiter.DelayEnable = 1;  // enable delay engine
    Limiter.MuteDuration = MIXER_LIMITER_MUTE_RAMP_DOWN_PERIOD;
    Limiter.UnMuteDuration = MIXER_LIMITER_MUTE_RAMP_UP_PERIOD;
    Limiter.Gain = OutputConfiguration.chain_volume[dev_num];
    // During initialization (i.e. when MMEInitialized is false) we must set HardGain to 1. This
    // prevents the Limiter from setting the initial gain to -96db and applying gain smoothing
    // (which takes ~500ms to reach 0db) during startup.
    Limiter.HardGain = (MMEInitialized ? 0 : 1);
	    
    //	Limiter.Threshold = ;
    Limiter.DelayBuffer = NULL; // delay buffer will be allocated by firmware
    Limiter.DelayBufSize = 0;
    Limiter.Delay = OutputConfiguration.chain_latency[dev_num]; //in ms

    return PlayerNoError;
}

////////////////////////////////////////////////////////////////////////////
/// 
/// Aggregate all the nominal PCM output chains into a single structure
/// describing all the outputs.
///
/// This method doesn't really do much useful at present.
///
PlayerStatus_t Mixer_Mme_c::AggregatePcmParameters( MME_LxPcmPostProcessingGlobalParameters_Frozen_t &
								PcmParams )
{
    unsigned int HeaderSize;
    unsigned char *DestP;
    unsigned int SizeOfParametersInBytes;

    //
    // Calculate the header size and pointer to the first parameter structure
    //
    
    HeaderSize = offsetof( MME_LxPcmPostProcessingGlobalParameters_Frozen_t, AuxSplit) +
                 sizeof( PcmParams.AuxSplit );
    
    DestP = ((unsigned char *) (&PcmParams)) + HeaderSize;

    //
    // Populate the header values
    //
    
    PcmParams.Id = ACC_RENDERER_MIXER_POSTPROCESSING_ID;
    PcmParams.StructSize = HeaderSize;
    PcmParams.NbPcmProcessings = 0; // no longer used
    PcmParams.AuxSplit = SPLIT_AUX; // split automatically 

    //
    // Copy data from the nominal structures into the collated structure
    //

    for( unsigned int n = 0; n < ActualNumDownstreamCards; n++ )
    {
        SizeOfParametersInBytes = NominalPcmParams[n].StructSize - HeaderSize;
        
        memcpy( DestP, ((unsigned char *) (&NominalPcmParams[n])) + HeaderSize, SizeOfParametersInBytes );
                
        DestP += SizeOfParametersInBytes;
        PcmParams.StructSize += SizeOfParametersInBytes;
    }

    return PlayerNoError;
}

////////////////////////////////////////////////////////////////////////////
/// 
/// Prepare the mixer command obtaining any required working buffers as we go.
///
/// The output buffers are obtained first since the this may involve waiting
/// for the output buffers to be ready. Waiting for output buffers is a blocking
/// operation (obviously).
///
/// The collection of input buffers is most definately not a blocking operation
/// (since blocking due to starvation would cause a pop on the output).
///
PlayerStatus_t Mixer_Mme_c::FillOutMixCommand()
{
ManifestorStatus_t Status;
unsigned int OutputBufferIndex = MixerCommand.Command.NumberInputBuffers;
    
    Status = FillOutOutputBuffer( MixerCommand.Command.DataBuffers_p[OutputBufferIndex] );
    if( Status != PlayerNoError )
    {
        MIXER_ERROR("Failed to obtain/populate output buffer\n");
        return Status;
    }

    for( int i = 0; i < MIXER_MAX_CLIENTS; i++ )
        FillOutInputBuffer(i);
        
    for( int i = 0; i < MIXER_MAX_INTERACTIVE_CLIENTS; i++ )
        FillOutInteractiveBuffer(i);
    
    // \todo Unconditionally plaing the interactive streams even when they are not present is harmless but
    //       theoretically may harm power consumption.
    MixerCommand.InputParams.InputParam[MIXER_INTERACTIVE_INPUT].Command = MIXER_PLAY;
    MixerCommand.InputParams.InputParam[MIXER_INTERACTIVE_INPUT].StartOffset = 0;
    MixerCommand.InputParams.InputParam[MIXER_INTERACTIVE_INPUT].PTS = 0;
    MixerCommand.InputParams.InputParam[MIXER_INTERACTIVE_INPUT].PTSflags = 0;

    return PlayerNoError;
}


////////////////////////////////////////////////////////////////////////////
///
/// Populate a data buffer ready for it to contain output samples.
///
PlayerStatus_t Mixer_Mme_c::FillOutOutputBuffer( MME_DataBuffer_t *DataBuffer )
{
PlayerStatus_t Status;
unsigned int PeriodSize = PcmPlayerSurfaceParameters.PeriodSize;;

    // this value cannot be pre-configured from Mixer_Mme_c::Reset because its
    // value can be changed by Mixer_Mme_c::SetModuleParameters.
    MixerCommand.Command.NumberOutputBuffers = ActualNumDownstreamCards;
    
    Status = MapSamples(PeriodSize);
    if( Status != PlayerNoError )
    {
        MIXER_ERROR("Failed to map a period of samples (%u)\n", PeriodSize);
        return PlayerError;
    }
    
    for(unsigned int n=0;n<ActualNumDownstreamCards;n++) {
        DataBuffer[n].ScatterPages_p->Page_p = PcmPlayerMappedSamples[n];
        DataBuffer[n].ScatterPages_p->Size = PcmPlayer[n]->SamplesToBytes(PeriodSize);
        DataBuffer[n].ScatterPages_p->FlagsIn = 0;
        DataBuffer[n].NumberOfScatterPages = 1;
        DataBuffer[n].TotalSize = DataBuffer[n].ScatterPages_p->Size;
    }
    
    return PlayerNoError;
}


////////////////////////////////////////////////////////////////////////////
///
/// Populate the data buffer associated with a specific manifestor.
///
void Mixer_Mme_c::FillOutInputBuffer( unsigned int Id )
{
PlayerStatus_t Status;

    if( Clients[Id].State == STARTED || Clients[Id].State == STOPPING )
    {
        PcmPlayer_c::OutputEncoding OutputEncoding = PcmPlayer_c::OUTPUT_IEC60958;
            
        //
        // Generate a ratio used by the manifestor to work out how many samples are required
        // for normal play and how many samples should be reserved for de-pop.
        //
        
        // both numbers should be divisible by 25
        Rational_c ResamplingFactor(Clients[Id].Parameters.Source.SampleRateHz, MixerSamplingFrequency, 25, false);
        
        if( 0 == Id )
        {
            if (OutputConfiguration.spdif_bypass || OutputConfiguration.hdmi_bypass)
                Status = Clients[Id].Manifestor->FillOutInputBuffer( MixerGranuleSize, ResamplingFactor,
                                                                 Clients[Id].State == STOPPING,
                                                                 MixerCommand.Command.DataBuffers_p[Id],
                                                                 MixerCommand.InputParams.InputParam + Id,
                                                                 MixerCommand.Command.DataBuffers_p[MIXER_CODED_DATA_INPUT],
                                                                 MixerCommand.InputParams.InputParam + MIXER_CODED_DATA_INPUT,
                                                                 &OutputEncoding,
                                                                 OutputConfiguration.spdif_bypass?Manifestor_AudioKsound_c::SPDIF:Manifestor_AudioKsound_c::HDMI);
            else                                     
                Status = Clients[Id].Manifestor->FillOutInputBuffer( MixerGranuleSize, ResamplingFactor,
                                                                 Clients[Id].State == STOPPING,
                                                                 MixerCommand.Command.DataBuffers_p[Id],
                                                                 MixerCommand.InputParams.InputParam + Id );
                                                                 
            if( PlayerNoError ==  Status && OutputEncoding != PrimaryCodedDataType )
            {
                PrimaryCodedDataType = OutputEncoding;
                // encoding has changed, so the sample rate and the number of samples of the 
                // bypassed ouput may have also...
                PcmPlayerNeedsParameterUpdate = true;
                MMENeedsParameterUpdate = true;

                // deliberately stop the compressed data input, since it is not configured yet as compressed data in the mixer...
                MixerCommand.InputParams.InputParam[MIXER_CODED_DATA_INPUT].Command = MixerCommand.InputParams.InputParam[Id].Command = MIXER_STOP;
                MixerCommand.InputParams.InputParam[MIXER_CODED_DATA_INPUT].StartOffset = MixerCommand.InputParams.InputParam[Id].StartOffset = 0;
            }
        }
        else                                     
            Status = Clients[Id].Manifestor->FillOutInputBuffer( MixerGranuleSize, ResamplingFactor,
                                                                 Clients[Id].State == STOPPING,
                                                                 MixerCommand.Command.DataBuffers_p[Id],
                                                                 MixerCommand.InputParams.InputParam + Id );
        if( Status == PlayerNoError )
            return;
            
        MIXER_ERROR("Manifestor failed to populate its input buffer\n");
        // error recovery is provided by falling through and filling out a silent buffer
    }

    FillOutSilentBuffer( MixerCommand.Command.DataBuffers_p[Id],
                         MixerCommand.InputParams.InputParam + Id );
}


////////////////////////////////////////////////////////////////////////////
///
/// Populate the data buffer associated with a specific interactive input.
///
void Mixer_Mme_c::FillOutInteractiveBuffer( unsigned int Id )
{
MME_DataBuffer_t *DataBuffer = MixerCommand.Command.DataBuffers_p[MIXER_INTERACTIVE_INPUT+Id];

    if( (InteractiveClients[Id].State == STARTED || InteractiveClients[Id].State == STOPPING) )
    {     
        //
        // We don't need to calculate how many samples are required. We can simply
        // map the entire hardware buffer into a databuffer and let the mixer have
        // fun.
        //
            
        DataBuffer->Flags = ( InteractiveClients[Id].Descriptor.channels == 1 ?
                              ACC_MIXER_DATABUFFER_FLAG_IAUDIO_MONO :
                              ACC_MIXER_DATABUFFER_FLAG_IAUDIO_STEREO );
        DataBuffer->NumberOfScatterPages = 0;
        DataBuffer->TotalSize = 0;
        DataBuffer->StartOffset = 0;
            
        unsigned char *Buffer = (unsigned char *) InteractiveClients[Id].Descriptor.hw_buffer;
        unsigned int BufferSize = InteractiveClients[Id].Descriptor.hw_buffer_size;
        unsigned int PlayPointer = InteractiveClients[Id].PlayPointer;

        DataBuffer->TotalSize = BufferSize;
            
        DataBuffer->ScatterPages_p[0].Page_p = Buffer + PlayPointer;
        DataBuffer->ScatterPages_p[0].Size = BufferSize - PlayPointer;
        DataBuffer->ScatterPages_p[0].FlagsIn = 0;
                        
        if( 0 == PlayPointer ) {
            DataBuffer->NumberOfScatterPages = 1;
        } else {
            DataBuffer->ScatterPages_p[1].Page_p = Buffer;
            DataBuffer->ScatterPages_p[1].Size = PlayPointer;
            DataBuffer->ScatterPages_p[1].FlagsIn = 0;
                
            DataBuffer->NumberOfScatterPages = 2;
        }
    }
    else
    {
        FillOutSilentBuffer( DataBuffer );
        DataBuffer->Flags = ACC_MIXER_DATABUFFER_FLAG_IAUDIO_MONO;
    }
}


////////////////////////////////////////////////////////////////////////////
///
/// Populate a data buffer such that it will be zero length (and therefore
/// silent).
///
/// If an mixer command structure is provided this will be configured to
/// stop the associated mixer output.
///
void Mixer_Mme_c::FillOutSilentBuffer( MME_DataBuffer_t *DataBuffer,
                                       tMixerFrameParams *MixerFrameParams )
{
    DataBuffer->NumberOfScatterPages = 0;
    DataBuffer->TotalSize = 0;
    DataBuffer->StartOffset = 0;
    DataBuffer->Flags = ACC_MIXER_DATABUFFER_FLAG_REGULAR;
    
    // zero-length, null pointer, no flags
    memset( &DataBuffer->ScatterPages_p[0], 0, sizeof( DataBuffer->ScatterPages_p[0] ) );

    if( MixerFrameParams )
    {
        MixerFrameParams->Command = MIXER_STOP;
        MixerFrameParams->StartOffset = 0;
        MixerFrameParams->PTS = 0;
        MixerFrameParams->PTSflags = 0;
    }
}


////////////////////////////////////////////////////////////////////////////
///
///
///
PlayerStatus_t Mixer_Mme_c::UpdateOutputBuffer( MME_DataBuffer_t *DataBuffer )
{
PlayerStatus_t Status;

    Status = CommitMappedSamples();
    if (Status != PlayerNoError)
        return Status;
        
    for(unsigned int n=0;n<ActualNumDownstreamCards;n++) {
        DataBuffer[n].TotalSize = 0;
    }
    return PlayerNoError;
}


////////////////////////////////////////////////////////////////////////////
///
///
///
void Mixer_Mme_c::UpdateInputBuffer( unsigned int Id )
{
PlayerStatus_t Status;

    if( Clients[Id].State == STARTED || Clients[Id].State == STOPPING )
    {
        if( 0 == Id && (OutputConfiguration.spdif_bypass || OutputConfiguration.hdmi_bypass) ) 
            Status = Clients[Id].Manifestor->UpdateInputBuffer(
                        MixerCommand.Command.DataBuffers_p[Id],
                        MixerCommand.OutputParams.MixStatus.InputStreamStatus + Id,
                        MixerCommand.Command.DataBuffers_p[MIXER_CODED_DATA_INPUT],
                        MixerCommand.OutputParams.MixStatus.InputStreamStatus + MIXER_CODED_DATA_INPUT );
        else
            Status = Clients[Id].Manifestor->UpdateInputBuffer(
                        MixerCommand.Command.DataBuffers_p[Id],
                        MixerCommand.OutputParams.MixStatus.InputStreamStatus + Id );
                
        if( Status != ManifestorNoError && Status != ManifestorNullQueued )
        {
            MIXER_ERROR("Failed to update the input buffer\n");
            // no real error recovery possible
        }
        
        // We really do want to check for *not* success here. NoError means that the update completed
        // normally (i.e. we haven't done a de-pop yet). NullQueued means that is safe to disconnect. Any
        // other error means the system is knackered... and refusing to stop the input is hardly going to
        // help at this point.
        if( Clients[Id].State == STOPPING && Status != ManifestorNoError )
        {
            MMENeedsParameterUpdate = true;
            MIXER_DEBUG( "Moving manifestor %p-%d from STOPPING to STOPPED\n", Clients[Id].Manifestor, Id );
            Clients[Id].State = STOPPED;
        }
    }
}


////////////////////////////////////////////////////////////////////////////
///
///
///
void Mixer_Mme_c::UpdateInteractiveBuffer( unsigned int Id )
{
    if( InteractiveClients[Id].State == STARTED )
    {
        unsigned int SamplesConsumed;
        unsigned int BytesConsumed;
        
        // In order to work out how many samples were consumed on each output it seems we have to figure out
        // how much the input will have been expanded during the interactive pre-mix and then scale down by
        // that value.
        SamplesConsumed = MixerCommand.OutputParams.MixStatus.InputStreamStatus[MIXER_INTERACTIVE_INPUT].BytesUsed;
        SamplesConsumed /= 8; /* 8 channels - nowhere to lookup in output status */
        SamplesConsumed /= 4; /* 32-bit samples - nowhere to lookup in output status */
        
        BytesConsumed = SamplesConsumed;
        BytesConsumed *= InteractiveClients[Id].Descriptor.channels;
        BytesConsumed *= InteractiveClients[Id].Descriptor.bytes_per_sample;

        // Update the play pointer based on the data consumed and wrap if required
        InteractiveClients[Id].PlayPointer += BytesConsumed;
        while( InteractiveClients[Id].PlayPointer >= InteractiveClients[Id].Descriptor.hw_buffer_size )
            InteractiveClients[Id].PlayPointer -= InteractiveClients[Id].Descriptor.hw_buffer_size;

        // Update the interactive client of the position of the play pointer
        InteractiveClients[Id].Descriptor.callback(
                InteractiveClients[Id].Descriptor.user_data, InteractiveClients[Id].PlayPointer );
    }
    else
    if( InteractiveClients[Id].State == STOPPING )
    {
        MMENeedsParameterUpdate = true;
        MIXER_DEBUG( "Moving interactive input %d from STOPPING to STOPPED\n", Id );
        InteractiveClients[Id].State = STOPPED;
    }
}


////////////////////////////////////////////////////////////////////////////
///
/// Check for any existing mixing metadata from the secondary stream, and
/// eventually update them
///
void Mixer_Mme_c::UpdateMixingMetadata()
{
    // Read the code carefully here. We use lots of x = y = z statements.
	
    // I bet the secondary client is at index 1...
    ParsedAudioParameters_t * SecondaryAudioParameters = &Clients[1].Parameters;

    if ((SecondaryAudioParameters->MixingMetadata.IsMixingMetadataPresent) &&
        (OutputConfiguration.metadata_update != SND_PSEUDO_MIXER_METADATA_UPDATE_NEVER))
    {
        MixingMetadata_t *StreamMetadata = &SecondaryAudioParameters->MixingMetadata;
        MIXER_DEBUG("Mixing metadata found (mix out configs: %d)!\n", StreamMetadata->NbOutMixConfig);

        // It is impossible for UpstreamConfiguration to be non-NULL. This is so because the reset value
        // of OutputConfiguration.metadata_update makes this code unreachable. The only means by which we
        // could end up executing this branch would also set UpstreamConfiguration to a non-NULL value.
        MIXER_ASSERT(UpstreamConfiguration);
        
	MIXER_TRACE("WARNING - Selecting first PrimaryAudioGain (which may not be correct)\n");
        for( int i=0; i<SND_PSEUDO_MIXER_CHANNELS; i++ )
        {
            UpstreamConfiguration->primary_gain[i] =
        	OutputConfiguration.primary_gain[i] = StreamMetadata->MixOutConfig[0].PrimaryAudioGain[i];
        }

        // To pan the secondary stream it is necessary that this stream is mono....
        if (SecondaryAudioParameters->Organisation == ACC_MODE10)
        {
            for ( int MixConfig = 0; MixConfig < StreamMetadata->NbOutMixConfig; MixConfig++ )
            {
                // check if this mix config is the same as the primary stream one,
                // if    so, the secondary panning coeef metadata will be applied
                if (StreamMetadata->MixOutConfig[MixConfig].AudioMode == Clients[PrimaryClient].Parameters.Organisation)
                {
                    for( int i=0; i<SND_PSEUDO_MIXER_CHANNELS; i++ )
                    {
                	UpstreamConfiguration->secondary_gain[i] =
                	    OutputConfiguration.secondary_gain[i] = Q3_13_UNITY;
                	UpstreamConfiguration->secondary_pan[i] =
                	    OutputConfiguration.secondary_pan[i] =
                	    	StreamMetadata->MixOutConfig[MixConfig].SecondaryAudioPanCoeff[i];
                    }
                }
            }
        }
        
        if (OutputConfiguration.metadata_update == SND_PSEUDO_MIXER_METADATA_UPDATE_ALWAYS)
             UpstreamConfiguration->post_mix_gain = 
        	 OutputConfiguration.post_mix_gain = StreamMetadata->PostMixGain;
    }
}


////////////////////////////////////////////////////////////////////////////
///
/// Simple lookup table used to convert between discrete and integer sampling frequencies.
///
/// The final zero value is very important. In both directions is prevents reads past the
/// end of the table. When indexed on eAccFsCode it also provides the return value when the
/// lookup fails.
///
static const struct
{
    enum eAccFsCode Discrete;
    unsigned int Integer;
}
SamplingFrequencyLookupTable[] =
{
    /* Range : 2^4  */ { ACC_FS768k, 768000 }, { ACC_FS705k, 705600 }, { ACC_FS512k, 512000 },
    /* Range : 2^3  */ { ACC_FS384k, 384000 }, { ACC_FS352k, 352800 }, { ACC_FS256k, 256000 },
    /* Range : 2^2  */ { ACC_FS192k, 192000 }, { ACC_FS176k, 176400 }, { ACC_FS128k, 128000 },
    /* Range : 2^1  */ {  ACC_FS96k,  96000 }, {  ACC_FS88k,  88200 }, {  ACC_FS64k,  64000 },
    /* Range : 2^0  */ {  ACC_FS48k,  48000 }, {  ACC_FS44k,  44100 }, {  ACC_FS32k,  32000 },
    /* Range : 2^-1 */ {  ACC_FS24k,  24000 }, {  ACC_FS22k,  22050 }, {  ACC_FS16k,  16000 },
    /* Range : 2^-2 */ {  ACC_FS12k,  12000 }, {  ACC_FS11k,  11025 }, {   ACC_FS8k,   8000 },
    /* Delimiter    */                                                 {   ACC_FS8k,      0 }
};


////////////////////////////////////////////////////////////////////////////
///
/// Take a continuous sampling frequency and identify the nearest eAccFsCode
/// for that frequency.
///
enum eAccFsCode Mixer_Mme_c::TranslateIntegerSamplingFrequencyToDiscrete( unsigned int IntegerFrequency )
{
    int i;
    
    for( i=0; IntegerFrequency < SamplingFrequencyLookupTable[i].Integer; i++ )
        ; // do nothing
    
    return SamplingFrequencyLookupTable[i].Discrete;
}


////////////////////////////////////////////////////////////////////////////
///
/// Take a discrete sampling frequency and convert that to an integer frequency.
///
/// Unexpected values (such as ACC_FS_ID) translate to zero.
///
unsigned int Mixer_Mme_c::TranslateDiscreteSamplingFrequencyToInteger( enum eAccFsCode DiscreteFrequency )
{
    int i;
    
    for (i=0; DiscreteFrequency != SamplingFrequencyLookupTable[i].Discrete; i++)
        if( 0 == SamplingFrequencyLookupTable[i].Integer )
            break;
    
    return SamplingFrequencyLookupTable[i].Integer;
}


////////////////////////////////////////////////////////////////////////////
///
/// Take a continuous sampling frequency and identify the eFsRange
/// for that frequency.
///
enum eFsRange Mixer_Mme_c::TranslateIntegerSamplingFrequencyToRange( unsigned int IntegerFrequency )
{
    if( IntegerFrequency < 64000 )
    {
	if( IntegerFrequency >= 32000 )
	    return ACC_FSRANGE_48k;
	
	if( IntegerFrequency >= 16000 )
	    return ACC_FSRANGE_24k;
	
	return ACC_FSRANGE_12k;
    }

    if( IntegerFrequency < 128000 )
	return ACC_FSRANGE_96k;
    
    if( IntegerFrequency < 256000 )
	return ACC_FSRANGE_192k;
    
    return ACC_FSRANGE_384k;
}


////////////////////////////////////////////////////////////////////////////
///
/// Lookup a discrete sampling frequency and convert it to a string.
///
const char * Mixer_Mme_c::LookupDiscreteSamplingFrequency( enum eAccFsCode DiscreteFrequency )
{
    switch( DiscreteFrequency )
    {
#define E(x) case x: return #x
    E(ACC_FS48k); 
    E(ACC_FS44k); 
    E(ACC_FS32k);
    E(ACC_FS_reserved_3); 
    /* Range : 2^1  */
    E(ACC_FS96k);
    E(ACC_FS88k);
    E(ACC_FS64k);
    E(ACC_FS_reserved_7); 
    /* Range : 2^2  */
    E(ACC_FS192k);
    E(ACC_FS176k);
    E(ACC_FS128k);
    E(ACC_FS_reserved_11);
    /* Range : 2^3 */
    E(ACC_FS384k);
    E(ACC_FS352k);
    E(ACC_FS256k);
    E(ACC_FS_reserved_15);
    /* Range : 2^-2 */
    E(ACC_FS12k);
    E(ACC_FS11k);
    E(ACC_FS8k);
    E(ACC_FS_reserved_19); 
    /* Range : 2^-1 */
    E(ACC_FS24k);
    E(ACC_FS22k);
    E(ACC_FS16k);
    E(ACC_FS_reserved_23); 
    /* Range : 2^-1 */
    E(ACC_FS768k);
    E(ACC_FS705k);
    E(ACC_FS512k);
    E(ACC_FS_reserved_27); 

    E(ACC_FS_reserved);  // Undefined
    E(ACC_FS_ID);        // Used by Mixer : if FS_ID then OutSFreq = InSFreq
    E(AUDIOMIXER_OVERRIDE_OUTFS);
#undef E
    default:
	return "INVALID";
    }
}


////////////////////////////////////////////////////////////////////////////
///
/// Lookup a discrete sampling frequency range and convert it to a string.
///
const char * Mixer_Mme_c::LookupDiscreteSamplingFrequencyRange( enum eFsRange DiscreteRange )
{
    switch( DiscreteRange )
    {
#define E(x) case x: return #x
    E( ACC_FSRANGE_12k );
    E( ACC_FSRANGE_24k );
    E( ACC_FSRANGE_48k );
    E( ACC_FSRANGE_96k );
    E( ACC_FSRANGE_192k );
    E( ACC_FSRANGE_384k );
#undef E
    default:
	return "INVALID";
    }
}

unsigned int Mixer_Mme_c::LookupSpdifPreamblePc( PcmPlayer_c::OutputEncoding Encoding )
{
   
#if 0     
Value   Corresponding frame
  0     NULL data
  1     Ac3 (1536 samples)
  3     Pause burst
  4     MPEG-1 layer-1
  5     MPEG-1 layer-2 or -3 data or MPEG-2 without extension
  6     MPEG-2 data with extension
  7     MPEG-2 AAC
  8     MPEG-2, layer-1 low sampling frequency
  9     MPEG-2, layer-2 low sampling frequency
 10     MPEG-2, layer-3 low sampling frequency
 11     DTS type I (512 samples)
 12     DTS type II (1024 samples)
 13     DTS type III (2048 samples)
 14     ATRAC (512 samples)
 15     ATRAC 2/3 (1 024 samples)
 17     DTS Type 4
 21     DDPLUS
 22     Dolby TrueHD
#endif

    switch( Encoding )
    {
    case PcmPlayer_c::OUTPUT_AC3:
    case PcmPlayer_c::BYPASS_AC3:
        return 1;
    
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

    default:
        return 0;
    }
}

unsigned int Mixer_Mme_c::LookupRepetitionPeriod( PcmPlayer_c::OutputEncoding Encoding )
{

    switch( Encoding )
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

    default:
        return 1;
    }
}

unsigned int Mixer_Mme_c::LookupIec60958FrameRate( PcmPlayer_c::OutputEncoding Encoding )
{
    ParsedAudioParameters_t * PrimaryAudioParameters = &Clients[PrimaryClient].Parameters;
    unsigned int OriginalSamplingFreq = PrimaryAudioParameters->BackwardCompatibleProperties.SampleRateHz;

    if (OriginalSamplingFreq == 0)
        OriginalSamplingFreq = MixerSamplingFrequency;
    
    unsigned int Iec60958FrameRate;

    if ( Encoding == PcmPlayer_c::BYPASS_AC3 )
    {
        Iec60958FrameRate = PrimaryAudioParameters->Source.SampleRateHz;
    }
    else if ( Encoding <= PcmPlayer_c::BYPASS_DTSHD_LBR )
    {
        Iec60958FrameRate = OriginalSamplingFreq;
    }
    else if ( Encoding <= PcmPlayer_c::BYPASS_DTSHD_MA)
    {
        Iec60958FrameRate = (PrimaryAudioParameters->Source.SampleRateHz * LookupRepetitionPeriod(Encoding)) / (PrimaryAudioParameters->SampleCount);
    }
    else if ( Encoding == PcmPlayer_c::BYPASS_TRUEHD)
    {
        Iec60958FrameRate = 4 * 192000;
    }
    else
    {
        Iec60958FrameRate = OriginalSamplingFreq;
    }

    return Iec60958FrameRate;
}

////////////////////////////////////////////////////////////////////////////
///
/// Simple lookup table used to convert between struct
/// ::snd_pseudo_mixer_channel_assignment and enum eAccAcMode.
///
static const struct
{
    enum eAccAcMode AccAcMode;
    const char *Text; // sneaky pre-processor trick used to convert the enumerations to textual equivalents
    struct snd_pseudo_mixer_channel_assignment ChannelAssignment;
    bool SuitableForDirectOutput;
}
ChannelAssignmentLookupTable[] =
{
#define E(mode, p0, p1, p2, p3) { mode, #mode, { SND_PSEUDO_MIXER_CHANNEL_PAIR_ ## p0, \
                                                 SND_PSEUDO_MIXER_CHANNEL_PAIR_ ## p1, \
                                                 SND_PSEUDO_MIXER_CHANNEL_PAIR_ ## p2, \
                                                 SND_PSEUDO_MIXER_CHANNEL_PAIR_ ## p3, \
			                         SND_PSEUDO_MIXER_CHANNEL_PAIR_NOT_CONNECTED }, true }
// as E but mark the output unsuitable for direct output
#define XXX(mode, p0, p1, p2, p3) { mode, #mode, { SND_PSEUDO_MIXER_CHANNEL_PAIR_ ## p0, \
                                                 SND_PSEUDO_MIXER_CHANNEL_PAIR_ ## p1, \
                                                 SND_PSEUDO_MIXER_CHANNEL_PAIR_ ## p2, \
                                                 SND_PSEUDO_MIXER_CHANNEL_PAIR_ ## p3, \
			                         SND_PSEUDO_MIXER_CHANNEL_PAIR_NOT_CONNECTED }, false }
// as XXX but for a different reason: there is already a better candidate for direct output, not a bug
#define DUPE(mode, p0, p1, p2, p3) XXX(mode, p0, p1, p2, p3)

    // Weird modes ;-)
  XXX( ACC_MODE10,	    	NOT_CONNECTED,	CNTR_0,		NOT_CONNECTED,	NOT_CONNECTED	), // Mad
    E( ACC_MODE20t,	    	LT_RT,		NOT_CONNECTED, 	NOT_CONNECTED, 	NOT_CONNECTED	),
//XXX( ACC_MODE53,              ...                                                             ), // Not 8ch
//XXX( ACC_MODE53_LFE,          ...                                                             ), // Not 8ch

    // CEA-861 (A to D) modes (in numerical order)
    E( ACC_MODE20,  	    	L_R,		NOT_CONNECTED,	NOT_CONNECTED,	NOT_CONNECTED	),
 DUPE( ACC_HDMI_MODE20,         L_R,            NOT_CONNECTED,  NOT_CONNECTED,  NOT_CONNECTED   ),
    E( ACC_MODE20_LFE,       	L_R,		0_LFE1,         NOT_CONNECTED,  NOT_CONNECTED   ),
 DUPE( ACC_HDMI_MODE20_LFE,    	L_R,		0_LFE1,         NOT_CONNECTED,  NOT_CONNECTED   ),
    E( ACC_MODE30,           	L_R,		CNTR_0,         NOT_CONNECTED,  NOT_CONNECTED   ),
 DUPE( ACC_HDMI_MODE30,        	L_R,		CNTR_0,         NOT_CONNECTED,  NOT_CONNECTED   ),
    E( ACC_MODE30_LFE,       	L_R,		CNTR_LFE1,      NOT_CONNECTED,  NOT_CONNECTED   ),
 DUPE( ACC_HDMI_MODE30_LFE,    	L_R,		CNTR_LFE1,      NOT_CONNECTED,  NOT_CONNECTED   ),
    E( ACC_MODE21,	    	L_R,		NOT_CONNECTED,	CSURR_0,	NOT_CONNECTED	),
 DUPE( ACC_HDMI_MODE21,	    	L_R,		NOT_CONNECTED,	CSURR_0,	NOT_CONNECTED	),
    E( ACC_MODE21_LFE,	    	L_R,		0_LFE1,		CSURR_0,	NOT_CONNECTED	),
 DUPE( ACC_HDMI_MODE21_LFE,    	L_R,		0_LFE1,		CSURR_0,	NOT_CONNECTED	),
    E( ACC_MODE31,           	L_R,		CNTR_0,         CSURR_0,        NOT_CONNECTED   ),
 DUPE( ACC_HDMI_MODE31,        	L_R,		CNTR_0,         CSURR_0,        NOT_CONNECTED   ),
    E( ACC_MODE31_LFE,       	L_R,		CNTR_LFE1,      CSURR_0,        NOT_CONNECTED   ),
 DUPE( ACC_HDMI_MODE31_LFE,    	L_R,		CNTR_LFE1,      CSURR_0,        NOT_CONNECTED   ),
    E( ACC_MODE22,	    	L_R,		NOT_CONNECTED,	LSUR_RSUR,	NOT_CONNECTED	),
 DUPE( ACC_HDMI_MODE22,	    	L_R,		NOT_CONNECTED,	LSUR_RSUR,	NOT_CONNECTED	),
    E( ACC_MODE22_LFE,	    	L_R,		0_LFE1,		LSUR_RSUR,	NOT_CONNECTED	),
 DUPE( ACC_HDMI_MODE22_LFE,   	L_R,		0_LFE1,		LSUR_RSUR,	NOT_CONNECTED	),
    E( ACC_MODE32,	    	L_R,		CNTR_0,		LSUR_RSUR,	NOT_CONNECTED	),
 DUPE( ACC_HDMI_MODE32,	    	L_R,		CNTR_0,		LSUR_RSUR,	NOT_CONNECTED	),
    E( ACC_MODE32_LFE,       	L_R,		CNTR_LFE1,	LSUR_RSUR,	NOT_CONNECTED	),
 DUPE( ACC_HDMI_MODE32_LFE,    	L_R,		CNTR_LFE1,	LSUR_RSUR,	NOT_CONNECTED	),
  XXX( ACC_MODE23,	    	L_R,		NOT_CONNECTED,	LSUR_RSUR,	CSURR_0		), // Bug 4518
 DUPE( ACC_HDMI_MODE23,	    	L_R,		NOT_CONNECTED,	LSUR_RSUR,	CSURR_0		),
  XXX( ACC_MODE23_LFE,	    	L_R,		0_LFE1,		LSUR_RSUR,	CSURR_0		), // Bug 4518
 DUPE( ACC_HDMI_MODE23_LFE,    	L_R,		0_LFE1,		LSUR_RSUR,	CSURR_0		),
  XXX( ACC_MODE33,	    	L_R,		CNTR_0,		LSUR_RSUR,	CSURR_0		), // Bug 4518
 DUPE( ACC_HDMI_MODE33,	    	L_R,		CNTR_0,		LSUR_RSUR,	CSURR_0		),
  XXX( ACC_MODE33_LFE,	    	L_R,		CNTR_LFE1,	LSUR_RSUR,	CSURR_0		), // Bug 4518
 DUPE( ACC_HDMI_MODE33_LFE,    	L_R,		CNTR_LFE1,	LSUR_RSUR,	CSURR_0		),
  XXX( ACC_MODE24,	    	L_R,		NOT_CONNECTED,	LSUR_RSUR,	LSURREAR_RSURREAR ),//Bug 4518
 DUPE( ACC_HDMI_MODE24,	    	L_R,		NOT_CONNECTED,	LSUR_RSUR,	LSURREAR_RSURREAR ),
  XXX( ACC_MODE24_LFE,	    	L_R,		0_LFE1,		LSUR_RSUR,	LSURREAR_RSURREAR ),//Bug 4518
 DUPE( ACC_HDMI_MODE24_LFE,	L_R,		0_LFE1,		LSUR_RSUR,	LSURREAR_RSURREAR ),
    E( ACC_MODE34,	    	L_R,		CNTR_0,		LSUR_RSUR,	LSURREAR_RSURREAR ),
 DUPE( ACC_HDMI_MODE34,	    	L_R,		CNTR_0,		LSUR_RSUR,	LSURREAR_RSURREAR ),
    E( ACC_MODE34_LFE,	    	L_R,		CNTR_LFE1,	LSUR_RSUR,	LSURREAR_RSURREAR ),
 DUPE( ACC_HDMI_MODE34_LFE,    	L_R,		CNTR_LFE1,	LSUR_RSUR,	LSURREAR_RSURREAR ),
  XXX( ACC_HDMI_MODE40,	    	L_R,		NOT_CONNECTED,	NOT_CONNECTED,	CNTRL_CNTRR	), // Bug 4518
  XXX( ACC_HDMI_MODE40_LFE,	L_R,		0_LFE1,		NOT_CONNECTED,	CNTRL_CNTRR	), // Bug 4518
  XXX( ACC_HDMI_MODE50,	    	L_R,		CNTR_0,		NOT_CONNECTED,	CNTRL_CNTRR	), // Bug 4518
  XXX( ACC_HDMI_MODE50_LFE,	L_R,		CNTR_LFE1,	NOT_CONNECTED,	CNTRL_CNTRR	), // Bug 4518
  XXX( ACC_HDMI_MODE41,	    	L_R,		NOT_CONNECTED,	CSURR_0,	CNTRL_CNTRR	), // Bug 4518
  XXX( ACC_HDMI_MODE41_LFE,    	L_R,		0_LFE1,		CSURR_0,	CNTRL_CNTRR	), // Bug 4518
  XXX( ACC_HDMI_MODE51,	    	L_R,		CNTR_0,		CSURR_0,	CNTRL_CNTRR	), // Bug 4518
  XXX( ACC_HDMI_MODE51_LFE,	L_R,		CNTR_LFE1,	CSURR_0,	CNTRL_CNTRR	), // Bug 4518
  XXX( ACC_MODE42,	    	L_R,		NOT_CONNECTED,	LSUR_RSUR,	CNTRL_CNTRR	), // Bug 4518
 DUPE( ACC_HDMI_MODE42,	    	L_R,		NOT_CONNECTED,	LSUR_RSUR,	CNTRL_CNTRR	),
  XXX( ACC_MODE42_LFE,	    	L_R,		0_LFE1,		LSUR_RSUR,	CNTRL_CNTRR	), // Bug 4518
 DUPE( ACC_HDMI_MODE42_LFE,	L_R,		0_LFE1,		LSUR_RSUR,	CNTRL_CNTRR	), 
  XXX( ACC_MODE52,	    	L_R,		CNTR_0,		LSUR_RSUR,	CNTRL_CNTRR	), // Bug 4518
 DUPE( ACC_HDMI_MODE52,	    	L_R,		CNTR_0,		LSUR_RSUR,	CNTRL_CNTRR	),
  XXX( ACC_MODE52_LFE,       	L_R,		CNTR_LFE1,	LSUR_RSUR,	CNTRL_CNTRR	), // Bug 4518
 DUPE( ACC_HDMI_MODE52_LFE,    	L_R,		CNTR_LFE1,	LSUR_RSUR,	CNTRL_CNTRR	),

    // CEA-861 (E) modes
  XXX( ACC_HDMI_MODE32_T100,	L_R,		CNTR_0,		LSUR_RSUR,      CHIGH_0		), // Bug 4518
  XXX( ACC_HDMI_MODE32_T100_LFE,L_R,		CNTR_LFE1,	LSUR_RSUR,      CHIGH_0		), // Bug 4518
  XXX( ACC_HDMI_MODE32_T010,	L_R,		CNTR_0,		LSUR_RSUR,      TOPSUR_0	), // Bug 4518
  XXX( ACC_HDMI_MODE32_T010_LFE,L_R,		CNTR_LFE1,	LSUR_RSUR,      TOPSUR_0	), // Bug 4518
  XXX( ACC_HDMI_MODE22_T200,	L_R,		NOT_CONNECTED,	LSUR_RSUR,      LHIGH_RHIGH	), // Bug 4518
  XXX( ACC_HDMI_MODE22_T200_LFE,L_R,		0_LFE1,		LSUR_RSUR,      LHIGH_RHIGH	), // Bug 4518
  XXX( ACC_HDMI_MODE42_WIDE,	L_R,		NOT_CONNECTED,	LSUR_RSUR,      LWIDE_RWIDE	), // Bug 4518
  XXX( ACC_HDMI_MODE42_WIDE_LFE,L_R,		0_LFE1,		LSUR_RSUR,      LWIDE_RWIDE	), // Bug 4518
  XXX( ACC_HDMI_MODE33_T010,	L_R,		CNTR_0,		LSUR_RSUR,      CSURR_TOPSUR	), // Bug 4518
  XXX( ACC_HDMI_MODE33_T010_LFE,L_R,		CNTR_LFE1,	LSUR_RSUR,      CSURR_TOPSUR	), // Bug 4518
  XXX( ACC_HDMI_MODE33_T100	,L_R,		CNTR_LFE1,	LSUR_RSUR,      CSURR_CHIGH	), // Bug 4518
  XXX( ACC_HDMI_MODE33_T100_LFE,L_R,		CNTR_LFE1,	LSUR_RSUR,      CSURR_CHIGH	), // Bug 4518
  XXX( ACC_HDMI_MODE32_T110,	L_R,		CNTR_0,		LSUR_RSUR,      CHIGH_TOPSUR	), // Bug 4518
  XXX( ACC_HDMI_MODE32_T110_LFE,L_R,		CNTR_LFE1,	LSUR_RSUR,      CHIGH_TOPSUR	), // Bug 4518
  XXX( ACC_HDMI_MODE32_T200,	L_R,		CNTR_0,		LSUR_RSUR,      LHIGH_RHIGH	), // Bug 4518
  XXX( ACC_HDMI_MODE32_T200_LFE,L_R,		CNTR_LFE1,	LSUR_RSUR,      LHIGH_RHIGH	), // Bug 4518
  XXX( ACC_HDMI_MODE52_WIDE,	L_R,		CNTR_0,		LSUR_RSUR,      LWIDE_RWIDE	), // Bug 4518
  XXX( ACC_HDMI_MODE52_WIDE_LFE,L_R,		CNTR_LFE1,	LSUR_RSUR,      LWIDE_RWIDE	), // Bug 4518

#if PCMPROCESSINGS_API_VERSION >= 0x100325
    // Unusual speaker topologies (not inclued in CEA-861 E)
  XXX( ACC_MODE30_T100,         L_R,            CNTR_0,         NOT_CONNECTED,  CHIGH_0             ),
  XXX( ACC_MODE30_T100_LFE,     L_R,            CNTR_LFE1,      NOT_CONNECTED,  CHIGH_0             ),
  XXX( ACC_MODE30_T200,         L_R,            CNTR_0,         NOT_CONNECTED,  LHIGH_RHIGH         ),
  XXX( ACC_MODE30_T200_LFE,     L_R,            CNTR_LFE1,      NOT_CONNECTED,  LHIGH_RHIGH         ),
  XXX( ACC_MODE22_T010,         L_R,            NOT_CONNECTED,  LSUR_RSUR,      TOPSUR_0            ),
  XXX( ACC_MODE22_T010_LFE,     L_R,            0_LFE1,         LSUR_RSUR,      TOPSUR_0            ),
  XXX( ACC_MODE32_T020,         L_R,            CNTR_0,         LSUR_RSUR,      LHIGHSIDE_RHIGHSIDE ),
  XXX( ACC_MODE32_T020_LFE,     L_R,            CNTR_LFE1,      LSUR_RSUR,      LHIGHSIDE_RHIGHSIDE ),
  XXX( ACC_MODE23_T100,         L_R,            NOT_CONNECTED,  LSUR_RSUR,      CHIGH_0             ),
  XXX( ACC_MODE23_T100_LFE,     L_R,            0_LFE1,         LSUR_RSUR,      CHIGH_0             ),
  XXX( ACC_MODE23_T010,         L_R,            NOT_CONNECTED,  LSUR_RSUR,      TOPSUR_0            ),
  XXX( ACC_MODE23_T010_LFE,     L_R,            0_LFE1,         LSUR_RSUR,      TOPSUR_0            ),
#endif

    // DTS-HD speaker topologies (not included in CEA-861)
    // These are all disabled at the moment because there is no matching entry in the AccAcMode
    // enumeration. The automatic fallback code (below) will select ACC_MODE32_LFE automatically
    // (by disconnecting pair3) if the user requests any of these modes.
//XXX( ACC_MODE32_LFE,       	L_R,		CNTR_LFE1,	LSUR_RSUR,	LSIDESURR_RSIDESURR ), No enum

    // delimiter
    E( ACC_MODE_ID,		NOT_CONNECTED,	NOT_CONNECTED,	NOT_CONNECTED,	NOT_CONNECTED	)

#undef E
#undef XXX
};


////////////////////////////////////////////////////////////////////////////
///
/// Lookup a discrete audio mode (2.0, 5.1, etc) and convert it to a string.
///
const char * Mixer_Mme_c::LookupAudioMode( enum eAccAcMode DiscreteMode )
{
    int i;
    
    for (i=0; ACC_MODE_ID != ChannelAssignmentLookupTable[i].AccAcMode; i++)
	if( DiscreteMode == ChannelAssignmentLookupTable[i].AccAcMode )
	    return ChannelAssignmentLookupTable[i].Text;
    
    // cover the delimiter itself...
    if( DiscreteMode == ChannelAssignmentLookupTable[i].AccAcMode )
	return ChannelAssignmentLookupTable[i].Text;
    
    return "UNKNOWN";
}


////////////////////////////////////////////////////////////////////////////
///
/// Lookup the most appropriate ACC_MODE for the current topology.
///
/// This method assume that the channel assignment structure has been been
/// pre-filtered ready for main or auxilliary lookups. In other words the
/// forth pair is *always* disconnected (we will be called twice to handle the
/// forth pair).
///
/// This method excludes from the lookup any format for which the firmware
/// cannot automatically derive (correct) downmix coefficients.
///
enum eAccAcMode Mixer_Mme_c::TranslateChannelAssignmentToAudioMode( struct snd_pseudo_mixer_channel_assignment ChannelAssignment )
{
    int pair, i;

    // we want to use memcmp() to compare the channel assignments so
    // we must explicitly zero the bits we don't care about
    ChannelAssignment.pair4 = SND_PSEUDO_MIXER_CHANNEL_PAIR_NOT_CONNECTED;
    ChannelAssignment.reserved0 = 0;
    ChannelAssignment.malleable = 0;

    for (pair=3; pair>=0; pair--)
    {
	for (i=0; ACC_MODE_ID != ChannelAssignmentLookupTable[i].AccAcMode; i++)
	    if (0 == memcmp(&ChannelAssignmentLookupTable[i].ChannelAssignment,
		            &ChannelAssignment, sizeof(ChannelAssignment)) &&
		ChannelAssignmentLookupTable[i].SuitableForDirectOutput)
		return ChannelAssignmentLookupTable[i].AccAcMode;
	
	// Progressively disconnect pairs of outputs until we find something that matches
	if (pair != 0) {
	    switch (pair) {
	    case 1: ChannelAssignment.pair1 = SND_PSEUDO_MIXER_CHANNEL_PAIR_NOT_CONNECTED; break;
	    case 2: ChannelAssignment.pair2 = SND_PSEUDO_MIXER_CHANNEL_PAIR_NOT_CONNECTED; break;
	    case 3: ChannelAssignment.pair3 = SND_PSEUDO_MIXER_CHANNEL_PAIR_NOT_CONNECTED; break;
	    }
	    
	    MIXER_TRACE( "Cannot find matching audio mode - disconnecting pair %d\n", pair );
	}
    }

    MIXER_TRACE( "Cannot find matching audio mode - falling back to ACC_MODE20t (Lt/Rt stereo)\n" );
    return ACC_MODE20t;
}


////////////////////////////////////////////////////////////////////////////
///
/// Lookup the 'natural' channel assignment for an audio mode.
///
/// This method and Mixer_Mme_c::TranslateChannelAssignmentToAudioMode are *not* reversible for
/// audio modes that are not marked as suitable for output.
///
struct snd_pseudo_mixer_channel_assignment Mixer_Mme_c::TranslateAudioModeToChannelAssignment(
	enum eAccAcMode AudioMode )
{
    struct snd_pseudo_mixer_channel_assignment Zeros = { 0 };

    if (ACC_MODE_ID == AudioMode) {
	    Zeros.malleable = 1;
	    return Zeros;
    }

    for (int i=0; ACC_MODE_ID != ChannelAssignmentLookupTable[i].AccAcMode; i++) {
	if (ChannelAssignmentLookupTable[i].AccAcMode == AudioMode) {
	    return ChannelAssignmentLookupTable[i].ChannelAssignment;
	}
    }
    
    // no point in stringizing the mode - we know its not in the table
    MIXER_ERROR("Cannot find matching audio mode (%d)\n", AudioMode);
    return Zeros;
}


////////////////////////////////////////////////////////////////////////////
///
/// Lookup the most appropriate ACC_MODE for the primary output.
///
enum eAccAcMode Mixer_Mme_c::TranslateDownstreamCardToMainAudioMode(
		struct snd_pseudo_mixer_downstream_card *DownstreamCard )
{
    struct snd_pseudo_mixer_channel_assignment ChannelAssignment = DownstreamCard->channel_assignment;
    
    // Disconnect any pair that is deselected by the number of channels
    switch( DownstreamCard->num_channels ) {
    case 1:
    case 2:
	ChannelAssignment.pair1 = SND_PSEUDO_MIXER_CHANNEL_PAIR_NOT_CONNECTED;
	//FALLTHRU
    case 3:
    case 4:
	ChannelAssignment.pair2 = SND_PSEUDO_MIXER_CHANNEL_PAIR_NOT_CONNECTED;
	//FALLTHRU
    case 5:
    case 6:
	ChannelAssignment.pair3 = SND_PSEUDO_MIXER_CHANNEL_PAIR_NOT_CONNECTED;
	//FALLTHRU
    default:
	// pair4 is unconditionally disconnected because it is only used for auxilliary output
	ChannelAssignment.pair4 = SND_PSEUDO_MIXER_CHANNEL_PAIR_NOT_CONNECTED;
    }
    
    return TranslateChannelAssignmentToAudioMode( ChannelAssignment );
}


////////////////////////////////////////////////////////////////////////////
///
/// Lookup the most appropriate ACC_MODE for the auxillary output.
///
enum eAccAcMode Mixer_Mme_c::TranslateDownstreamCardToAuxAudioMode(
		struct snd_pseudo_mixer_downstream_card *DownstreamCard )
{
    struct snd_pseudo_mixer_channel_assignment ChannelAssignment = DownstreamCard->channel_assignment;
 
    if( DownstreamCard->num_channels < 10 ||
	SND_PSEUDO_MIXER_CHANNEL_PAIR_NOT_CONNECTED == ChannelAssignment.pair4 )
	return ACC_MODE_ID;
    
    ChannelAssignment.pair0 = ChannelAssignment.pair4;
    ChannelAssignment.pair1 =
	ChannelAssignment.pair2 =
	    ChannelAssignment.pair3 =
		ChannelAssignment.pair4 = SND_PSEUDO_MIXER_CHANNEL_PAIR_NOT_CONNECTED;

    return TranslateChannelAssignmentToAudioMode( ChannelAssignment );
}

////////////////////////////////////////////////////////////////////////////
///
/// Simple lookup table used to convert between incoming speaker toplogy and
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
enum eAccAcMode Mixer_Mme_c::LookupFatPipeOutputMode( enum eAccAcMode InputMode )
{
    int i;
    
    for (i=0; ACC_MODE_ID != FatPipeOutputModeLookupTable[i].AccAcModeInput; i++)
	if( InputMode == FatPipeOutputModeLookupTable[i].AccAcModeInput )
	    break;

    // i is either the index of the correct entry or the last entry (which is crafted
    // to be the correct value for a lookup miss).

    return FatPipeOutputModeLookupTable[i].AccAcModeOutput;
}

////////////////////////////////////////////////////////////////////////////
///
/// Configure the HDMI audio cell
///
PlayerStatus_t Mixer_Mme_c::ConfigureHDMICell(PcmPlayer_c::OutputEncoding OutputEncoding, unsigned int CardFlags)
{
    // get a handle to the output device
    stm_display_device_t * pDev = stm_display_get_device(OutputConfiguration.display_device_id);
    if (pDev == NULL)
    {
        MIXER_ERROR("Unable to get display device %d\n", OutputConfiguration.display_device_id);
        return PlayerError;
    }
    stm_display_output_t *out = stm_display_get_output(pDev, OutputConfiguration.display_output_id);
    
    if (out == NULL)
    {
        MIXER_ERROR("Unable to get display output %d (device %d)\n", OutputConfiguration.display_output_id, OutputConfiguration.display_device_id);
        return PlayerError;
    }

    // check the current hdmi audio switches...
    {
        ULONG ctrlVal;
        
        if (!stm_display_output_get_control(out, STM_CTRL_AV_SOURCE_SELECT, &ctrlVal))
        {
            MIXER_DEBUG("Current HDMI AV source selection: 0x%x\n", ctrlVal);
        }
    
        stm_hdmi_audio_output_type_t OutputType;
        unsigned int AVSource = STM_AV_SOURCE_MAIN_INPUT;

        if ( (OutputEncoding == PcmPlayer_c::BYPASS_TRUEHD) ||
             (OutputEncoding == PcmPlayer_c::BYPASS_DTSHD_MA) )
        {
            // High Bit Rate audio is required...
            OutputType = STM_HDMI_AUDIO_TYPE_HBR;
            AVSource |= STM_AV_SOURCE_8CH_I2S_INPUT;
        }
        else
        {
            // hdmi layout 0 mode
            OutputType = STM_HDMI_AUDIO_TYPE_NORMAL;

            if (CardFlags & SND_PSEUDO_TOPOLOGY_FLAGS_ENABLE_SPDIF_FORMATING)
            {
                // we're connected to a spdif player...
                AVSource |= STM_AV_SOURCE_SPDIF_INPUT;
            }
            else
            {
                // we're connected to a pcm player
                AVSource |= STM_AV_SOURCE_2CH_I2S_INPUT;
            }
        }
        
        if (stm_display_output_set_control(out, STM_CTRL_AV_SOURCE_SELECT, AVSource))
        {
            MIXER_ERROR("Could not set control STM_CTRL_AV_SOURCE_SELECT\n");
            return PlayerError;
        }
        
        if (stm_display_output_set_control(out, STM_CTRL_HDMI_AUDIO_OUT_SELECT, OutputType))
        {
            MIXER_ERROR("Could not set control STM_CTRL_HDMI_AUDIO_OUT_SELECT\n");
            return PlayerError;
        }
    }
    return PlayerNoError;
}
