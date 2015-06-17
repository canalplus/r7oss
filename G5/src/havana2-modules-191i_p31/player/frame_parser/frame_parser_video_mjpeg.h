/************************************************************************
Copyright (C) 2009 STMicroelectronics. All Rights Reserved.

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

Source file name : frame_parser_video_mjpeg.h
Author :           Andy

Definition of the frame parser video MJPEG class implementation for player 2.


Date        Modification                                    Name
----        ------------                                    --------

************************************************************************/

#ifndef H_FRAME_PARSER_VIDEO_MJPEG
#define H_FRAME_PARSER_VIDEO_MJPEG

// /////////////////////////////////////////////////////////////////////
//
//      Include any component headers

#include "mjpeg.h"
#include "frame_parser_video.h"

// /////////////////////////////////////////////////////////////////////////
//
// Locally defined structures
//


// /////////////////////////////////////////////////////////////////////////
//
// The class definition
//

class FrameParser_VideoMjpeg_c : public FrameParser_Video_c
{
private:

    // Data

    MjpegStreamParameters_t     CopyOfStreamParameters;
    MjpegStreamParameters_t*    StreamParameters;
    MjpegFrameParameters_t*     FrameParameters;

    // Functions

    FrameParserStatus_t         ReadStreamMetadata(                             void );
    FrameParserStatus_t         ReadStartOfFrame(                               void );
#if 0
    FrameParserStatus_t         ReadQuantizationMatrices(                       void );
    FrameParserStatus_t         ReadRestartInterval(                            void );
    FrameParserStatus_t         ReadHuffmanTables(                              void );
    FrameParserStatus_t         ReadStartOfScan(                                void );
#endif

    FrameParserStatus_t         CommitFrameForDecode(                           void );

public:

    // Constructor function
    FrameParser_VideoMjpeg_c( void );
    ~FrameParser_VideoMjpeg_c( void );

    // Overrides for component base class functions
    FrameParserStatus_t         Reset(                                          void );

    // FrameParser class functions
    FrameParserStatus_t         RegisterOutputBufferRing(                       Ring_t          Ring );

    // Stream specific functions
    FrameParserStatus_t         ReadHeaders(                                    void );
    //FrameParserStatus_t         RevPlayProcessDecodeStacks(                     void );
    //FrameParserStatus_t         RevPlayGeneratePostDecodeParameterSettings(     void );

};

#endif

