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

Source file name : frame_parser_video_flv1.h
Author :           Julian

Definition of the frame parser video flv1 class implementation for player 2.


Date        Modification                                    Name
----        ------------                                    --------
19-May_08   Created                                         Julian

************************************************************************/

#ifndef H_FRAME_PARSER_VIDEO_FLV1
#define H_FRAME_PARSER_VIDEO_FLV1

// /////////////////////////////////////////////////////////////////////
//
//      Include any component headers

#include "h263.h"
#include "frame_parser_video_h263.h"

// /////////////////////////////////////////////////////////////////////////
//
// Locally defined structures
//


// /////////////////////////////////////////////////////////////////////////
//
// The class definition
//

/// Frame parser for Flv1
class FrameParser_VideoFlv1_c : public FrameParser_VideoH263_c
{
private:

    FrameParserStatus_t         FlvReadPictureHeader(           void );

public:

    FrameParser_VideoFlv1_c(                                    void );

    FrameParserStatus_t         ReadHeaders(                    void );

};

#endif

