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

Source file name : frame_parser_audio_vorbis.h
Author :

Definition of the frame parser video Ogg Vorbis Audio class implementation for player 2.


Date        Modification                                Name
----        ------------                                --------
10-Mar-09   Created                                     Julian

************************************************************************/

#ifndef H_FRAME_PARSER_AUDIO_VORBIS
#define H_FRAME_PARSER_AUDIO_VORBIS

// /////////////////////////////////////////////////////////////////////
//
//      Include any component headers

#include "vorbis_audio.h"
#include "frame_parser_audio.h"


// /////////////////////////////////////////////////////////////////////////
//
// Locally defined constants
//


// /////////////////////////////////////////////////////////////////////////
//
// Locally defined structures
//

typedef enum
{
    VorbisNoHeaders                     = 0x00,
    VorbisIdentificationHeader          = 0x01,
    VorbisCommentHeader                 = 0x02,
    VorbisSetupHeader                   = 0x04,
    VorbisAllHeaders                    = 0x07,
} VorbisHeader_t;

// /////////////////////////////////////////////////////////////////////////
//
// The class definition
//

/// Frame parser for Ogg Vorbis audio
class FrameParser_AudioVorbis_c : public FrameParser_Audio_c
{
private:

    // Data

    VorbisAudioParsedFrameHeader_t      ParsedFrameHeader;

    VorbisAudioStreamParameters_t*      StreamParameters;
    VorbisAudioStreamParameters_t       CurrentStreamParameters;
    VorbisAudioFrameParameters_t*       FrameParameters;

    unsigned int                        StreamHeadersRead;

    // Functions

    FrameParserStatus_t                 ReadStreamHeaders(              void );

public:

    // Constructor function

    FrameParser_AudioVorbis_c( void );
    ~FrameParser_AudioVorbis_c( void );

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

