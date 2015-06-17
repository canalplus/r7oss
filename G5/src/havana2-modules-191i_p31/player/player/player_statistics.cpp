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

Source file name : player_statistics.cpp
Author :           Nick

Implementation of the statistics related functions of the
generic class implementation of player 2


Date        Modification                                    Name
----        ------------                                    --------
02-Jul-07   Created                                         Nick

************************************************************************/

#include "player_generic.h"

// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Useful defines/macros that need not be user visible
//

#define Init( X )				\
{						\
    X.Count	= 0;				\
    X.Total	= 0;				\
    X.Longest	= 0;				\
    X.Shortest	= 0xffffffffffffffffULL;	\
}

//

#define Update( X, V )				\
{						\
    X.Count++;					\
    X.Total	+= V;				\
    X.Longest	 = max( X.Longest, V );		\
    X.Shortest	 = min( X.Shortest, V );	\
}

//

#define Report( S, X )				\
{						\
unsigned long long Frac;			\
						\
    Frac	= ((X.Total*1000)/X.Count);	\
    Frac	= Frac - (1000 * (Frac/1000));	\
    report( severity_info, "    %s - Max %12lld, Min %12lld, Average %12lld.%03lld\n", S, X.Longest, X.Shortest, X.Total/X.Count, Frac );	\
}

// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Set a playback speed.
//

void   Player_Generic_c::ProcessStatistics(	PlayerStream_t		  Stream,
						PlayerSequenceNumber_t	 *Record )
{
unsigned char	Policy;
unsigned int	CodedBufferCount,CodedBuffersUsed,DecodeBufferCount,DecodeBuffersUsed, PosssibleDecodeBuffers;

//

    Policy	= PolicyValue( Stream->Playback, Stream, (PlayerPolicy_t)((Stream->StreamType == StreamTypeAudio) ? PolicyStatisticsOnAudio : PolicyStatisticsOnVideo) );
    if( Policy != PolicyValueApply )
	return;

//

#if 0

    report( severity_info, "TimeEntryInProcess0 	= %016llx\n", Record->TimeEntryInProcess0 );
    report( severity_info, "TimeEntryInProcess1 	= %016llx\n", Record->TimeEntryInProcess1 );
    report( severity_info, "TimePassToCodec        	= %016llx\n", Record->TimePassToCodec );
    report( severity_info, "TimeEntryInProcess2 	= %016llx\n", Record->TimeEntryInProcess2 );
    report( severity_info, "TimePassToManifestor 	= %016llx\n", Record->TimePassToManifestor );
    report( severity_info, "TimeEntryInProcess3 	= %016llx\n", Record->TimeEntryInProcess3 );
#endif

//

    if( Stream->Statistics.Count == 0 )
    {
	Init( Stream->Statistics.DeltaEntryIntoProcess0 );
	Init( Stream->Statistics.DeltaEntryIntoProcess1 );
	Init( Stream->Statistics.DeltaEntryIntoProcess2 );
	Init( Stream->Statistics.DeltaEntryIntoProcess3 );

	Init( Stream->Statistics.Traverse0To1 );
	Init( Stream->Statistics.Traverse1To2 );
	Init( Stream->Statistics.Traverse2To3 );

	Init( Stream->Statistics.FrameTimeInProcess1 );
	Init( Stream->Statistics.FrameTimeInProcess2 );
	Init( Stream->Statistics.TotalTraversalTime );
    }

    if( Record->TimePassToCodec != INVALID_TIME )
	Update( Stream->Statistics.FrameTimeInProcess1, (Record->TimePassToCodec - Record->TimeEntryInProcess1) );

    if( Record->TimePassToManifestor != INVALID_TIME )
	Update( Stream->Statistics.FrameTimeInProcess2, (Record->TimePassToManifestor - Record->TimeEntryInProcess2) );

    Update( Stream->Statistics.Traverse0To1, 		(Record->TimeEntryInProcess1 - Record->TimeEntryInProcess0) );
    Update( Stream->Statistics.Traverse1To2, 		(Record->TimeEntryInProcess2 - Record->TimeEntryInProcess1) );
    Update( Stream->Statistics.Traverse2To3, 		(Record->TimeEntryInProcess3 - Record->TimeEntryInProcess2) );
    Update( Stream->Statistics.TotalTraversalTime, 	(Record->TimeEntryInProcess3 - Record->TimeEntryInProcess0) );
    Update( Stream->Statistics.DeltaEntryIntoProcess0, 	Record->DeltaEntryInProcess0 );
    Update( Stream->Statistics.DeltaEntryIntoProcess1, 	Record->DeltaEntryInProcess1 );
    Update( Stream->Statistics.DeltaEntryIntoProcess2, 	Record->DeltaEntryInProcess2 );
    Update( Stream->Statistics.DeltaEntryIntoProcess3, 	Record->DeltaEntryInProcess3 );

    Stream->Statistics.Count++;

    if( Stream->Statistics.Count == 1024 )
    {
	report( severity_info, "\n" );

	Stream->CodedFrameBufferPool->GetPoolUsage( &CodedBufferCount, &CodedBuffersUsed, NULL, NULL, NULL );
	Stream->DecodeBufferPool->GetPoolUsage( &DecodeBufferCount, &DecodeBuffersUsed, NULL, NULL, NULL );
	Stream->Manifestor->GetDecodeBufferCount( &PosssibleDecodeBuffers );
	report( severity_info, "    Coded data buffers %2d, %2d used - Decode buffers %2d (%2d), %2d used.\n", CodedBufferCount, CodedBuffersUsed, DecodeBufferCount, PosssibleDecodeBuffers, DecodeBuffersUsed );

	Report( "DeltaEntryIntoProcess0", Stream->Statistics.DeltaEntryIntoProcess0 );
	Report( "DeltaEntryIntoProcess1", Stream->Statistics.DeltaEntryIntoProcess1 );
	Report( "DeltaEntryIntoProcess2", Stream->Statistics.DeltaEntryIntoProcess2 );
	Report( "DeltaEntryIntoProcess3", Stream->Statistics.DeltaEntryIntoProcess3 );

	Report( "FrameTimeInProcess1   ", Stream->Statistics.FrameTimeInProcess1 );
	Report( "FrameTimeInProcess2   ", Stream->Statistics.FrameTimeInProcess2 );

	Report( "Traverse0To1          ", Stream->Statistics.Traverse0To1 );
	Report( "Traverse1To2          ", Stream->Statistics.Traverse1To2 );
	Report( "Traverse2To3          ", Stream->Statistics.Traverse2To3 );
	Report( "TotalTraversalTime    ", Stream->Statistics.TotalTraversalTime );
	report( severity_info, "\n" );

	Stream->Statistics.Count	= 0;
    }
}




