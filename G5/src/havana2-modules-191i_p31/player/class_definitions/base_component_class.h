/************************************************************************
Copyright (C) 2006 STMicroelectronics. All Rights Reserved.

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

Source file name : base_component_class.h
Author :           Nick

Definition of the base class for all of the component classes in player 2
module.


Date        Modification                                    Name
----        ------------                                    --------
12-Oct-06   Created                                         Nick

************************************************************************/


#ifndef H_BASE_COMPONENT_CLASS
#define H_BASE_COMPONENT_CLASS

#include "player_types.h"

//

typedef enum
{
    ComponentReset      = 1,
    ComponentRunning,
    ComponentHalted,
    ComponentInError
} PlayerComponentState_t;

//

#define AssertComponentState( f, s )            \
    if( ComponentState != s )                   \
    {                                           \
	report( severity_error, "%s - Component not in required state (in %d, required %d).\n", f, ComponentState, s ); \
	return PlayerError;                     \
    }

#define TestComponentState( s )                 (ComponentState == s)
#define SetComponentState( s )                  ComponentState = s

#define CrashWithStackTrace()                   (*((int*)0))++;

//

class BaseComponentClass_c
{
protected:

    Player_t                      Player;
    PlayerPlayback_t              Playback;
    PlayerStream_t                Stream;
    PlayerComponentState_t        ComponentState;
    PlayerEventMask_t             EventMask;
    void                         *EventUserData;

    Collator_t                    Collator;
    FrameParser_t                 FrameParser;
    Codec_t                       Codec;
    OutputTimer_t                 OutputTimer;
    Manifestor_t                  Manifestor;

//

public:

    PlayerStatus_t              InitializationStatus;

//

    BaseComponentClass_c( void )
    {
	InitializationStatus	= PlayerNoError;
	BaseComponentClass_c::Reset();
    }

//

    virtual ~BaseComponentClass_c( void ) {};

//

    virtual PlayerStatus_t      Halt( void )
    {
	SetComponentState( ComponentHalted );
	return  PlayerNoError;
    }

//

    virtual PlayerStatus_t      Reset( void )
    {
	Player          = NULL;
	Playback        = PlayerAllPlaybacks;
	Stream          = PlayerAllStreams;
	EventMask       = 0;
	EventUserData   = NULL;

	Collator        = NULL;
	FrameParser     = NULL;
	Codec           = NULL;
	OutputTimer     = NULL;
	Manifestor      = NULL;

	SetComponentState( ComponentReset );
	return  PlayerNoError;
    }           

//

    virtual PlayerStatus_t      RegisterPlayer( 
					Player_t                 Player,
					PlayerPlayback_t         Playback,
					PlayerStream_t           Stream,
					Collator_t		 Collator	= NULL,
					FrameParser_t		 FrameParser	= NULL,
					Codec_t			 Codec		= NULL,
					OutputTimer_t		 OutputTimer	= NULL,
					Manifestor_t		 Manifestor	= NULL )
    {
	//
	// In order to provide this data for early use, there is an assumption 
	// that this registratory function does no real work, and cannot fail.
	//

	this->Player    = Player;
	this->Playback  = Playback;
	this->Stream    = Stream;

	if( Collator != NULL )
	    this->Collator	= Collator;

	if( FrameParser != NULL )
	    this->FrameParser	= FrameParser;

	if( Codec != NULL )
	    this->Codec		= Codec;

	if( OutputTimer != NULL )
	    this->OutputTimer	= OutputTimer;

	if( Manifestor != NULL )
	    this->Manifestor	= Manifestor;

	return  PlayerNoError;
    }

//

    virtual PlayerStatus_t      SpecifySignalledEvents( 
					PlayerEventMask_t        EventMask,
					void                    *EventUserData )
    {
	this->EventMask         = EventMask;
	this->EventUserData     = EventUserData;

	return  PlayerNoError;
    }

//

    virtual PlayerStatus_t      SetModuleParameters(    
					unsigned int             ParameterBlockSize,
					void                    *ParameterBlock )
    {
	return  PlayerNotSupported;
    }

    virtual PlayerStatus_t      CreateAttributeEvents (void)
    {
	return  PlayerNoError;
    }

    virtual PlayerStatus_t      GetAttribute (
					const char                      *Attribute,
					PlayerAttributeDescriptor_t     *Value)
    {
	return  PlayerNoError;
    }

    virtual PlayerStatus_t      SetAttribute (
					const char                      *Attribute,
					PlayerAttributeDescriptor_t     *Value)
    {
	return  PlayerNoError;
    }
};

// ---------------------------------------------------------------------
//
// Docuentation
//

/*! \class BaseComponentClass_c
\brief Base class from which all component classes are derived.

All of the component classes, (apart from the buffer classes) have 
certain common components in terms of globally visible data,
entrypoints, and usage of player class support functions. Here we 
detail those common components that all these classes <b>must</b> 
have.
*/

/*! \var BaseComponentClass_c::InitializationStatus
\brief Status flag indicating how the intitialization of the class went.

In order to fit well into the Linux kernel environment the player is
compiled with exceptions and RTTI disabled. In C++ constructors cannot
return an error code since all errors should be reported using
exceptions. With exceptions disabled we need to use this alternative
means to communicate that the constructed object instance is not valid.
*/

/*! \fn PlayerStatus_t BaseComponentClass_c::Halt()
\brief Prepare for destruction or reset.

This function is used in preparation for destruction or a reset, it 
halts processing of all data, and de-registers any resources registered 
to the class. Halt should be called prior to destruction, or reset, of 
a component class.

\return Player status code, PlayerNoError indicates success.
*/

/*! \fn PlayerStatus_t BaseComponentClass_c::Reset()
\brief Completely reset a class.

This function is used to completely reset a class, it should be used 
when a class is to be re-used after channel or stream change. This 
function is called by the class constructor during class initialization.

\return Player status code, PlayerNoError indicates success.
*/

/*! \fn PlayerStatus_t BaseComponentClass_c::RegisterPlayer(Player_t Player, PlayerPlayback_t Playback, PlayerStream_t Stream )
\brief Register to player instantiation to which the class 'belongs'.

Takes as parameter a pointer to a Player class instance, a playback identifier, and a stream identifier. This function is used to register the Player instantiation with the component class instance, this is used when the component needs to access a player entrypoint. 

\param Player Pointer to a Player_c instance
\param Playback Playback identifier
\param Stream Stream identifier

\return Player status code, PlayerNoError indicates success.
*/

/*! \fn PlayerStatus_t BaseComponentClass_c::SpecifySignalledEvents(PlayerEventMask_t EventMask, void *EventUserData)
\brief Enable/disable ongoing event signalling.

\param EventMask A mask indicating which occurances should raise an event record
\param EventUserData An optional void * value to be associated with any event raised.

\return Player status code, PlayerNoError indicates success.
*/

/*! \fn PlayerStatus_t BaseComponentClass_c::SetModuleParameters(unsigned int ParameterBlockSize, void *ParameterBlock)
\brief Specify the module parameters.

\param ParameterBlockSize Size of the parameter block (in bytes)
\param ParameterBlock Pointer to the parameter block

\return Player status code, PlayerNoError indicates success.
*/

#endif

