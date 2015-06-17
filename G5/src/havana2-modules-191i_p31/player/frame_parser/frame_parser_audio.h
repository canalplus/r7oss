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

Source file name : frame_parser_audio.h
Author :           Daniel

Definition of the frame parser video base class implementation for player 2.


Date        Modification                                    Name
----        ------------                                    --------
30-Mar-07   Created (from frame_parser_video.h)             Daniel

************************************************************************/

#ifndef H_FRAME_PARSER_AUDIO
#define H_FRAME_PARSER_AUDIO

// /////////////////////////////////////////////////////////////////////
//
//	Include any component headers

#include "frame_parser_base.h"

// /////////////////////////////////////////////////////////////////////////
//
// Locally defined structures
//

// /////////////////////////////////////////////////////////////////////////
//
// The class definition
//

/// Framework to unify the approach to audio frame parsing.
class FrameParser_Audio_c : public FrameParser_Base_c
{
protected:

    // Data

    ParsedAudioParameters_t	 *ParsedAudioParameters;

    /// The maximum value by which the actual PTS is permitted to differ from the synthesised PTS
    /// (use to identify bad streams or poor predictions).
    int PtsJitterTollerenceThreshold;
    
    /// Time at which that previous frame was to be presented.
    /// This is primarily used for debugging PTS sequencing problems (e.g. calculating deltas between
    /// 'then' and 'now'.
    unsigned long long LastNormalizedPlaybackTime;
    /// Expected time at which the following frame must be presented.
    unsigned long long  NextFrameNormalizedPlaybackTime;
    /// Error that is accumulated by calculating the PTS based on its last recorded value.
    unsigned long long NextFramePlaybackTimeAccumulatedError;
    
    bool UpdateStreamParameters; ///< True if we need to update the stream parameters on the next frame.
    
    // Functions

    FrameParserStatus_t HandleCurrentFrameNormalizedPlaybackTime();
    void HandleUpdateStreamParameters();
    void GenerateNextFrameNormalizedPlaybackTime(unsigned int SampleCount, unsigned SamplingFrequency);


public:

    FrameParser_Audio_c();
    
    //
    // Overrides for component base class functions
    //

    FrameParserStatus_t   Reset(		void );

    //
    // FrameParser class functions
    //

    FrameParserStatus_t   RegisterOutputBufferRing(	Ring_t		Ring );

    FrameParserStatus_t   Input(			Buffer_t		  CodedBuffer );

    //
    // Common portion of read headers
    //

    virtual FrameParserStatus_t   ReadHeaders(		void );

};

#endif

