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

Source file name : frame_parser_audio_rma.h
Author :

Definition of the frame parser video Real Media Audio class implementation for player 2.


Date        Modification                                Name
----        ------------                                --------
28-Jan-09   Created                                     Julian

************************************************************************/

#ifndef H_FRAME_PARSER_AUDIO_RMA
#define H_FRAME_PARSER_AUDIO_RMA

// /////////////////////////////////////////////////////////////////////
//
//      Include any component headers

#include "rma_audio.h"
#include "frame_parser_audio.h"


// /////////////////////////////////////////////////////////////////////////
//
// Locally defined constants
//


// /////////////////////////////////////////////////////////////////////////
//
// Locally defined structures
//

// /////////////////////////////////////////////////////////////////////////
//
// The class definition
//

/// Frame parser for Real Media Rma audio
class FrameParser_AudioRma_c : public FrameParser_Audio_c
{
private:

    // Data

    RmaAudioParsedFrameHeader_t         ParsedFrameHeader;

    RmaAudioStreamParameters_t*         StreamParameters;
    RmaAudioStreamParameters_t          CurrentStreamParameters;
    RmaAudioFrameParameters_t*          FrameParameters;

    bool                                StreamFormatInfoValid;
    unsigned int                        DataOffset;

    // Functions

    FrameParserStatus_t         ReadStreamParameters(                   void );

public:

    // Constructor function

    FrameParser_AudioRma_c( void );
    ~FrameParser_AudioRma_c( void );

    // Overrides for component base class functions

    FrameParserStatus_t   Reset(                                        void );

    // FrameParser class functions

    FrameParserStatus_t   RegisterOutputBufferRing(     Ring_t          Ring );

    // Stream specific functions

    FrameParserStatus_t   ReadHeaders(                                  void );

    FrameParserStatus_t   ResetReferenceFrameList(                      void );
    FrameParserStatus_t   PurgeQueuedPostDecodeParameterSettings(       void );
    FrameParserStatus_t   PrepareReferenceFrameList(                    void );
    FrameParserStatus_t   ProcessQueuedPostDecodeParameterSettings(     void );
    FrameParserStatus_t   GeneratePostDecodeParameterSettings(          void );
    FrameParserStatus_t   UpdateReferenceFrameList(                     void );

    FrameParserStatus_t   ProcessReverseDecodeUnsatisfiedReferenceStack(void );
    FrameParserStatus_t   ProcessReverseDecodeStack(                    void );
    FrameParserStatus_t   PurgeReverseDecodeUnsatisfiedReferenceStack(  void );
    FrameParserStatus_t   PurgeReverseDecodeStack(                      void );
    FrameParserStatus_t   TestForTrickModeFrameDrop(                    void );
};

#endif

