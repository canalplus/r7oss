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

Source file name : frame_parser_video_h263.h
Author :           Julian

Definition of the frame parser video vc1 class implementation for player 2.


Date        Modification                                    Name
----        ------------                                    --------
19-May_08   Created                                         Julian

************************************************************************/

#ifndef H_FRAME_PARSER_VIDEO_H263
#define H_FRAME_PARSER_VIDEO_H263

// /////////////////////////////////////////////////////////////////////
//
//      Include any component headers

#include "h263.h"
#include "frame_parser_video.h"

// /////////////////////////////////////////////////////////////////////////
//
// Locally defined structures
//

#define H263_REFERENCE_FRAMES_FOR_FORWARD_DECODE                1
#define H263_TEMPORAL_REFERENCE_INVALID                         0x1000


// /////////////////////////////////////////////////////////////////////////
//
// The class definition
//

/// Frame parser for H263
class FrameParser_VideoH263_c : public FrameParser_Video_c
{
private:

    FrameParserStatus_t         H263ReadPictureHeader(          void );

    bool                        NewStreamParametersCheck(       void );

protected:

    H263StreamParameters_t*     StreamParameters;
    H263FrameParameters_t*      FrameParameters;
    H263StreamParameters_t      CopyOfStreamParameters;

    unsigned int                TemporalReference;

    FrameParserStatus_t         CommitFrameForDecode(           void );

public:

    FrameParser_VideoH263_c(                                    void );
    ~FrameParser_VideoH263_c(                                   void );

    //
    // Overrides for component base class functions
    //

    FrameParserStatus_t   Reset(                                void );

    //
    // FrameParser class functions
    //

    FrameParserStatus_t   RegisterOutputBufferRing(             Ring_t          Ring );

    //
    // Stream specific functions
    //

    FrameParserStatus_t   ReadHeaders(                                          void );

    FrameParserStatus_t   ResetReferenceFrameList(                              void );
    FrameParserStatus_t   PrepareReferenceFrameList(                            void );

    FrameParserStatus_t   ForPlayUpdateReferenceFrameList(                      void );

    FrameParserStatus_t   RevPlayProcessDecodeStacks(                           void );
};

#endif

