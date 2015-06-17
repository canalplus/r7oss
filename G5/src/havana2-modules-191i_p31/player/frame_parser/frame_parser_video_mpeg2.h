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

Source file name : frame_parser_video_mpeg2.h
Author :           Nick

Definition of the frame parser video mpeg2 class implementation for player 2.


Date        Modification                                    Name
----        ------------                                    --------
29-Nov-06   Created                                         Nick

************************************************************************/

#ifndef H_FRAME_PARSER_VIDEO_MPEG2
#define H_FRAME_PARSER_VIDEO_MPEG2

// /////////////////////////////////////////////////////////////////////
//
//	Include any component headers

#include "mpeg2.h"
#include "frame_parser_video.h"

// /////////////////////////////////////////////////////////////////////////
//
// Locally defined structures
//

// /////////////////////////////////////////////////////////////////////////
//
// The class definition
//

class FrameParser_VideoMpeg2_c : public FrameParser_Video_c
{
private:

    // Data

    Mpeg2StreamParameters_t		  CopyOfStreamParameters;

    Mpeg2StreamParameters_t		 *StreamParameters;
    Mpeg2FrameParameters_t		 *FrameParameters;

    int					  LastPanScanHorizontalOffset;
    int					  LastPanScanVerticalOffset;

    bool				  EverSeenRepeatFirstField;		// Support for PolicyMPEG2DoNotHonourProgressiveFrameFlag

    bool				  LastFirstFieldWasAnI;			// Support for self referencing IP field pairs

    // Functions

    FrameParserStatus_t   ReadSequenceHeader(				void );
    FrameParserStatus_t   ReadSequenceExtensionHeader(			void );
    FrameParserStatus_t   ReadSequenceDisplayExtensionHeader(		void );
    FrameParserStatus_t   ReadSequenceScalableExtensionHeader(		void );
    FrameParserStatus_t   ReadGroupOfPicturesHeader(			void );
    FrameParserStatus_t   ReadPictureHeader(				void );
    FrameParserStatus_t   ReadPictureCodingExtensionHeader(		void );
    FrameParserStatus_t   ReadQuantMatrixExtensionHeader(		void );
    FrameParserStatus_t   ReadPictureDisplayExtensionHeader(		void );
    FrameParserStatus_t   ReadPictureTemporalScalableExtensionHeader(	void );
    FrameParserStatus_t   ReadPictureSpatialScalableExtensionHeader(	void );

    bool		  NewStreamParametersCheck(			void );
    FrameParserStatus_t   CommitFrameForDecode(				void );

public:

    //
    // Constructor function
    //

    FrameParser_VideoMpeg2_c( void );
    ~FrameParser_VideoMpeg2_c( void );

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

    FrameParserStatus_t   ReadHeaders( 						void );
    FrameParserStatus_t   ResetReferenceFrameList(				void );
    FrameParserStatus_t   PrepareReferenceFrameList(				void );

    FrameParserStatus_t   ForPlayUpdateReferenceFrameList(			void );

    FrameParserStatus_t   RevPlayProcessDecodeStacks(				void );
    FrameParserStatus_t   RevPlayGeneratePostDecodeParameterSettings(		void );
    FrameParserStatus_t   RevPlayAppendToReferenceFrameList(			void );
    FrameParserStatus_t   RevPlayRemoveReferenceFrameFromList(			void );
    FrameParserStatus_t   RevPlayJunkReferenceFrameList(			void );
};

#endif

