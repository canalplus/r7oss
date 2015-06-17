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

Source file name : frame_parser_audio_eac3.h
Author :           Sylvain

Definition of the frame parser audio eac3 class implementation for player 2.


Date        Modification                                    Name
----        ------------                                    --------
23-May-07   Created (from frame_parser_audio_ac3.h)         Sylvain

************************************************************************/

#ifndef H_FRAME_PARSER_AUDIO_EAC3
#define H_FRAME_PARSER_AUDIO_EAC3

// /////////////////////////////////////////////////////////////////////
//
//	Include any component headers

#include "eac3_audio.h"
#include "frame_parser_audio.h"

// /////////////////////////////////////////////////////////////////////////
//
// Locally defined structures
//

// /////////////////////////////////////////////////////////////////////////
//
// The class definition
//

class FrameParser_AudioEAc3_c : public FrameParser_Audio_c
{
private:

    // Data
    
    EAc3AudioParsedFrameHeader_t ParsedFrameHeader;
    bool FirstTime;
    
    EAc3AudioStreamParameters_t	*StreamParameters;
    EAc3AudioStreamParameters_t CurrentStreamParameters;
    EAc3AudioFrameParameters_t *FrameParameters;

    // Functions
public:

    //
    // Constructor function
    //

    FrameParser_AudioEAc3_c( void );
    ~FrameParser_AudioEAc3_c( void );

    //
    // Overrides for component base class functions
    //

    FrameParserStatus_t   Reset(		void );

    //
    // FrameParser class functions
    //

    FrameParserStatus_t   RegisterOutputBufferRing(	Ring_t		Ring );

    //
    // Stream specific functions
    //

    FrameParserStatus_t   ReadHeaders( 					void );
    FrameParserStatus_t   ResetReferenceFrameList(			void );
    FrameParserStatus_t   PurgeQueuedPostDecodeParameterSettings(	void );
    FrameParserStatus_t   PrepareReferenceFrameList(			void );
    FrameParserStatus_t   ProcessQueuedPostDecodeParameterSettings(	void );
    FrameParserStatus_t   GeneratePostDecodeParameterSettings(		void );
    FrameParserStatus_t   UpdateReferenceFrameList(			void );

    FrameParserStatus_t   ProcessReverseDecodeUnsatisfiedReferenceStack(void );
    FrameParserStatus_t   ProcessReverseDecodeStack(			void );
    FrameParserStatus_t   PurgeReverseDecodeUnsatisfiedReferenceStack(	void );
    FrameParserStatus_t   PurgeReverseDecodeStack(			void );
    FrameParserStatus_t   TestForTrickModeFrameDrop(			void );

    FrameParserStatus_t ParseFrameHeader( unsigned char *FrameHeader, 
                                          EAc3AudioParsedFrameHeader_t *ParsedFrameHeader,
                                          int RemainingElementaryLength );

	static FrameParserStatus_t ParseSingleFrameHeader( unsigned char *FrameHeaderBytes, 
													   EAc3AudioParsedFrameHeader_t *ParsedFrameHeader,
													   bool SearchForConvSync);
};

#endif /* H_FRAME_PARSER_AUDIO_EAC3 */

