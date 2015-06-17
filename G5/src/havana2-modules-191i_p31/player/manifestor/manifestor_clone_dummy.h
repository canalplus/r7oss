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

Source file name : manifestor_clone_dummy.h
Author :           Nick

Dummy instance of a manifestor clone module for player 2.


Date        Modification                                    Name
----        ------------                                    --------
22-Jan-10   Created                                         Nick

************************************************************************/

#ifndef H_MANIFESTOR_CLONE_DUMMY
#define H_MANIFESTOR_CLONE_DUMMY

#include "player.h"

// ---------------------------------------------------------------------
//
// Local type definitions
//

// ---------------------------------------------------------------------
//
// Class definition
//

class Manifestor_CloneDummy_c : public Manifestor_c
{
    Ring_t				 OutputRing;

public:

    Manifestor_CloneDummy_c(	void )
    { report( severity_info, "Manifestor_CloneDummy_c - Called\n" ); OutputRing = NULL;}

    ~Manifestor_CloneDummy_c(	void )
    { report( severity_info, "~Manifestor_CloneDummy_c - Called\n" ); }


    ManifestorStatus_t   GetDecodeBufferPool(           BufferPool_t             *Pool )
    { report( severity_fatal, "GetDecodeBufferPool - Called\n" ); return ManifestorNoError; }

    ManifestorStatus_t   GetPostProcessControlBufferPool( BufferPool_t            *Pool )
    { report( severity_fatal, "GetPostProcessControlBufferPool - Called\n" ); return ManifestorNoError; }

    ManifestorStatus_t   RegisterOutputBufferRing(      Ring_t                    Ring )
    { report( severity_info, "RegisterOutputBufferRing - Called\n" ); OutputRing = Ring; return ManifestorNoError; }

    ManifestorStatus_t   GetSurfaceParameters(          void                    **SurfaceParameters )
    { report( severity_fatal, "GetSurfaceParameters - Called\n" ); return ManifestorNoError; }

    ManifestorStatus_t   GetNextQueuedManifestationTime( unsigned long long	 *Time)
    { report( severity_fatal, "GetNextQueuedManifestationTime - Called\n" ); return ManifestorNoError; }

    ManifestorStatus_t   ReleaseQueuedDecodeBuffers(    void )
    { report( severity_info, "ReleaseQueuedDecodeBuffers - Called\n" ); return ManifestorNoError; }

    ManifestorStatus_t   InitialFrame(                  Buffer_t                  Buffer )
    { report( severity_info, "InitialFrame - Called\n" ); if( OutputRing != NULL ) OutputRing->Insert( (unsigned int)Buffer ); return ManifestorNoError; }

    ManifestorStatus_t   QueueDecodeBuffer(             Buffer_t                  Buffer )
    { report( severity_info, "QueueDecodeBuffer - Called\n" ); if( OutputRing != NULL ) OutputRing->Insert( (unsigned int)Buffer ); return ManifestorNoError; }

    ManifestorStatus_t   QueueNullManifestation(        void )
    { report( severity_info, "QueueNullManifestation - Called\n" ); return ManifestorNoError; }

    ManifestorStatus_t   QueueEventSignal(              PlayerEventRecord_t      *Event )
    { report( severity_fatal, "QueueEventSignal - Called\n" ); return ManifestorNoError; }

    ManifestorStatus_t   GetNativeTimeOfCurrentlyManifestedFrame( unsigned long long *Time )
    { report( severity_fatal, "GetNativeTimeOfCurrentlyManifestedFrame - Called\n" ); return ManifestorNoError; }

    ManifestorStatus_t   GetDecodeBuffer(               BufferStructure_t        *RequestedStructure,
							Buffer_t                 *Buffer )
    { report( severity_fatal, "GetDecodeBuffer - Called\n" ); return ManifestorNoError; }

    ManifestorStatus_t   GetDecodeBufferCount(          unsigned int             *Count )
    { report( severity_fatal, "GetDecodeBufferCount - Called\n" ); return ManifestorNoError; }

    ManifestorStatus_t   SynchronizeOutput(             void )
    { report( severity_info, "SynchronizeOutput - Called\n" ); return ManifestorNoError; }

    ManifestorStatus_t   GetFrameCount(                 unsigned long long       *FrameCount )
    { report( severity_fatal, "GetFrameCount - Called\n" ); return ManifestorNoError; }

};
#endif
