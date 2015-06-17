/************************************************************************
Copyright (C) 2006 STMicroelectronics. All Rights Reserved.

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

Source file name : frame_parser_video_dvp.h
Author :           Chris

Definition of the DVP frame parser video class implementation for player 2.


Date        Modification                                    Name
----        ------------                                    --------
08-Aug-07   Created                                         Chris

************************************************************************/

#ifndef H_FRAME_PARSER_VIDEO_DVP
#define H_FRAME_PARSER_VIDEO_DVP

// /////////////////////////////////////////////////////////////////////
//
//	Include any component headers

#include "dvp.h"
#include "frame_parser_video.h"

// /////////////////////////////////////////////////////////////////////////
//
// Locally defined structures
//

typedef struct DvpStreamParameters_s
{
    int dummy;    
} DvpStreamParameters_t;

typedef struct DvpFrameParameters_s
{
    int dummy;    
} DvpFrameParameters_t;

// /////////////////////////////////////////////////////////////////////////
//
// The class definition
//

class FrameParser_VideoDvp_c : public FrameParser_Video_c
{
protected:

    // Data

    DvpStreamParameters_t	 *StreamParameters;
    DvpFrameParameters_t	 *FrameParameters;
    DvpRectangle_t		  InputWindow;
    DvpRectangle_t		  OutputWindow;

public:

    //
    // Constructor/Destructor methods
    //

    FrameParser_VideoDvp_c ( void );
    ~FrameParser_VideoDvp_c( void );

    //
    // Overrides for component base class functions
    //

    FrameParserStatus_t   Reset( void );

    //
    // FrameParser class functions
    //

    FrameParserStatus_t   RegisterOutputBufferRing( Ring_t Ring );

    //
    // Extensions to the class overriding the base implementations
    // NOTE in order to keep the names reasonably short, in the following
    // functions specifically for forward playback will be prefixed with 
    // ForPlay and functions specific to reverse playback will be prefixed with 
    // RevPlay.
    //

    virtual FrameParserStatus_t   ForPlayProcessFrame( void );
    virtual FrameParserStatus_t   ForPlayQueueFrameForDecode( void );

    virtual FrameParserStatus_t   RevPlayProcessFrame( void );
    virtual FrameParserStatus_t   RevPlayQueueFrameForDecode( void );
    virtual FrameParserStatus_t   RevPlayProcessDecodeStacks( void );
    virtual FrameParserStatus_t   RevPlayPurgeDecodeStacks( void );

    //
    // Extensions to the class to be fulfilled by my inheritors, 
    // these are required to support the process buffer override
    //

    FrameParserStatus_t   ReadHeaders( void );
	
    FrameParserStatus_t   PrepareReferenceFrameList( void );
    FrameParserStatus_t   ForPlayUpdateReferenceFrameList( void );

};

#endif // H_FRAME_PARSER_VIDEO_DIVX
