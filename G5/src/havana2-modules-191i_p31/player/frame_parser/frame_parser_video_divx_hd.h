/************************************************************************
Copyright (C) 2008 STMicroelectronics. All Rights Reserved.

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

Source file name : frame_parser_video_divx_hd.h
Author :           Chris

Definition of the DivX HD frame parser video class implementation for player 2.


Date        Modification                                    Name
----        ------------                                    --------
18-Jun-07   Created                                         Chris

************************************************************************/

#ifndef H_FRAME_PARSER_VIDEO_DIVX_HD
#define H_FRAME_PARSER_VIDEO_DIVX_HD

// /////////////////////////////////////////////////////////////////////
//
//      Include any component headers

#include "frame_parser_video_divx.h"

// /////////////////////////////////////////////////////////////////////////
//
// The class definition
//

class FrameParser_VideoDivxHd_c : public FrameParser_VideoDivx_c
{

protected:

    FrameParserStatus_t  ReadVopHeader(       Mpeg4VopHeader_t         *Vop );
    FrameParserStatus_t  ReadHeaders(         void );

};

#endif // H_FRAME_PARSER_VIDEO_DIVX_HD
