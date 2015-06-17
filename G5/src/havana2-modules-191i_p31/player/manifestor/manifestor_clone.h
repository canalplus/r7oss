/************************************************************************
Copyright (C) 2010 STMicroelectronics. All Rights Reserved.

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

Source file name : manifestor_clone.h
Author :           Nick

Definition of the manifestor cloning class implementation for player 2.


Date        Modification                                    Name
----        ------------                                    --------
20-Jan-10   Created                                         Nick

************************************************************************/

#ifndef H_MANIFESTOR_CLONE
#define H_MANIFESTOR_CLONE

// /////////////////////////////////////////////////////////////////////
//
//      Include any component headers

#include "player.h"

// /////////////////////////////////////////////////////////////////////
//
//      defined values

// ---------------------------------------------------------------------
//
// Local type definitions
//

// ---------------------------------------------------------------------
//
// Class definition
//

/// Framework for implementing manifestors.
class Manifestor_Clone_c : public Manifestor_c
{
protected:

    // Data

    Manifestor_t		Original;
    Manifestor_t		CloneTo;

    OS_Thread_t			BufferReleaseThreadId;
    bool			BufferReleaseThreadRunning;

    Ring_t                      OriginalOutputRing;
    Ring_t                      CloneOutputRing;

public:

    //
    // Thread to handle release of cloned buffers
    //

    void   BufferReleaseThread(	void );

    //
    // Constructor/Destructor methods
    //

    Manifestor_Clone_c( Manifestor_t	Original );
    ~Manifestor_Clone_c( void );

    //
    // Overrides for component base class functions
    //

    PlayerStatus_t   Halt( 				void );
    PlayerStatus_t   Reset(				void );

    PlayerStatus_t   SetModuleParameters(		unsigned int		 ParameterBlockSize,
							void			*ParameterBlock);

    PlayerStatus_t   RegisterPlayer(			Player_t		 Player,
							PlayerPlayback_t	 Playback,
							PlayerStream_t		 Stream,
							Collator_t		 Collator       = NULL,
							FrameParser_t		 FrameParser    = NULL,
							Codec_t			 Codec          = NULL,
							OutputTimer_t		 OutputTimer    = NULL,
							Manifestor_t		 Manifestor     = NULL );

    //
    // Class functions
    //

    ManifestorStatus_t   GetDecodeBufferPool(           BufferPool_t             *Pool );

    ManifestorStatus_t   GetPostProcessControlBufferPool( BufferPool_t            *Pool );

    ManifestorStatus_t   RegisterOutputBufferRing(      Ring_t                    Ring );

    ManifestorStatus_t   GetSurfaceParameters(          void                    **SurfaceParameters );

    ManifestorStatus_t   GetNextQueuedManifestationTime( unsigned long long	 *Time);

    ManifestorStatus_t   ReleaseQueuedDecodeBuffers(    void );

    ManifestorStatus_t   InitialFrame(                  Buffer_t                  Buffer );

    ManifestorStatus_t   QueueDecodeBuffer(             Buffer_t                  Buffer );

    ManifestorStatus_t   QueueNullManifestation(        void );

    ManifestorStatus_t   QueueEventSignal(              PlayerEventRecord_t      *Event );

    ManifestorStatus_t   GetNativeTimeOfCurrentlyManifestedFrame( unsigned long long *Time );

    ManifestorStatus_t   GetDecodeBuffer(               BufferStructure_t        *RequestedStructure,
							Buffer_t                 *Buffer );

    ManifestorStatus_t   GetDecodeBufferCount(          unsigned int             *Count );

    ManifestorStatus_t   SynchronizeOutput(             void );

    ManifestorStatus_t   GetFrameCount(                 unsigned long long       *FrameCount );

    //
    // Extension function to support clone management
    //

    ManifestorStatus_t   SetCloneTo(			Manifestor_t		  CloneTo );
    ManifestorStatus_t   GetManifestors(		Manifestor_t		  *Original,
							Manifestor_t		  *CloneTo );
};
#endif

