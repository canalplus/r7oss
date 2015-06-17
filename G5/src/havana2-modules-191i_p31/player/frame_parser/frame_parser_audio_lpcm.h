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

Source file name : frame_parser_audio_lpcm.h
Author :           Sylvain

Definition of the frame parser audio lpcm class implementation for player 2.


Date        Modification                                    Name
----        ------------                                    --------
12-Jul-07   Created                                         Sylvain

************************************************************************/

#ifndef H_FRAME_PARSER_AUDIO_LPCM
#define H_FRAME_PARSER_AUDIO_LPCM

// /////////////////////////////////////////////////////////////////////
//
//	Include any component headers

#include "lpcm.h"
#include "frame_parser_audio.h"

// /////////////////////////////////////////////////////////////////////////
//
// Locally defined structures
//

// /////////////////////////////////////////////////////////////////////////
//
// The class definition
//

class FrameParser_AudioLpcm_c : public FrameParser_Audio_c
{
private:

    // Data
    
    LpcmAudioStreamParameters_t	*StreamParameters;
    LpcmAudioStreamParameters_t CurrentStreamParameters;
    LpcmAudioFrameParameters_t *FrameParameters;
    
    /// LPCM variant we are required to operate with. Copied from the collator during registration.
    LpcmStreamType_t StreamType;
    
    /// Number of samples of instrinic latency in the decoder.
    /// The output timer assumes that the decode latency any decode is smaller than the length of time that the
    /// frame will be displayed for. Decoders such as SPDIF have additional latency that the output time must be made
    /// aware of in the decode time stamp. This value contains that latency.
    unsigned int IndirectDecodeLatencyInSamples;

    // Functions
public:

    //
    // Constructor function
    //

    FrameParser_AudioLpcm_c( unsigned int DecodeLatencyInSamples = 0 );
    ~FrameParser_AudioLpcm_c( void );

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

    static FrameParserStatus_t  ParseFrameHeader( unsigned char *FrameHeader, 
						  LpcmAudioParsedFrameHeader_t *ParsedFrameHeader,
						  int RemainingElementaryLength );
    
};

#endif /* H_FRAME_PARSER_AUDIO_LPCM */

