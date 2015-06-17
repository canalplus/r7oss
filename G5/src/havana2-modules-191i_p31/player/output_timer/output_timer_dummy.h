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

Source file name : output_timer_dummy.h
Author :           Nick

Dummy instance of the output timer class module for player 2.


Date        Modification                                    Name
----        ------------                                    --------
16-Jan-07   Created                                         Nick

************************************************************************/

#ifndef H_OUTPUT_TIMER_DUMMY
#define H_OUTPUT_TIMER_DUMMY

#include "player.h"

// ---------------------------------------------------------------------
//
// Local type definitions
//

// ---------------------------------------------------------------------
//
// Class definition
//

class OutputTimer_Dummy_c : public OutputTimer_c
{
public:

    OutputTimer_Dummy_c(	void )
    { report( severity_info, "OutputTimer_Dummy_c - Called\n" ); }

    ~OutputTimer_Dummy_c(	void )
    { report( severity_info, "~OutputTimer_Dummy_c - Called\n" ); }


    OutputTimerStatus_t   RegisterStream(		PlayerStream_t		  Stream,
							PlayerStreamType_t	  StreamType,
							TimerStreamContext_t	 *TimerContext )
		{ report( severity_info, "OutputTimer_Dummy_c::RegisterStream - Called\n" ); return OutputTimerNoError; }

    OutputTimerStatus_t   DeRegisterStream(		TimerStreamContext_t	  TimerContext )
		{ report( severity_info, "OutputTimer_Dummy_c::DeRegisterStream - Called\n" ); return OutputTimerNoError; }

    OutputTimerStatus_t   SetPlaybackSpeed(		UnsignedFixedPoint_t	  Speed,
							PlayDirection_t		  Direction )
		{ report( severity_info, "OutputTimer_Dummy_c::SetPlaybackSpeed - Called\n" ); return OutputTimerNoError; }

    OutputTimerStatus_t   ResetTimeMapping(		void )
		{ report( severity_info, "OutputTimer_Dummy_c::ResetTimeMapping - Called\n" ); return OutputTimerNoError; }

    OutputTimerStatus_t   EstablishTimeMapping(		unsigned long long	  NormalizedPlaybackTime,
							unsigned long long	  SystemTime = INVALID_TIME )
		{ report( severity_info, "OutputTimer_Dummy_c::EstablishTimeMapping - Called\n" ); return OutputTimerNoError; }

    OutputTimerStatus_t   TranslatePlaybackTimeToSystem(unsigned long long	  NormalizedPlaybackTime,
							unsigned long long	 *SystemTime )
		{ report( severity_info, "OutputTimer_Dummy_c::TranslatePlaybackTimeToSystem - Called\n" ); return OutputTimerNoError; }

    OutputTimerStatus_t   TranslateSystemTimeToPlayback(unsigned long long	  SystemTime,
							unsigned long long	 *NormalizedPlaybackTime )
		{ report( severity_info, "OutputTimer_Dummy_c::TranslateSystemTimeToPlayback - Called\n" ); return OutputTimerNoError; }

    OutputTimerStatus_t   GenerateFrameTiming(		TimerStreamContext_t	  TimerContext,
							Buffer_t		  Buffer,
							bool			  NonBlocking = false )
		{ report( severity_info, "OutputTimer_Dummy_c::GenerateFrameTiming - Called\n" ); return OutputTimerNoError; }

    OutputTimerStatus_t   RecordActualFrameTiming(	TimerStreamContext_t	  TimerContext,
							Buffer_t		  Buffer )
		{ report( severity_info, "OutputTimer_Dummy_c::RecordActualFrameTiming - Called\n" ); return OutputTimerNoError; }
};
#endif
