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

Source file name : frame_parser.h
Author :           Nick

Definition of the frame parser class module for player 2.


Date        Modification                                    Name
----        ------------                                    --------
02-Nov-06   Created                                         Nick

************************************************************************/

#ifndef H_FRAME_PARSER
#define H_FRAME_PARSER

#include "player.h"

// ---------------------------------------------------------------------
//
// Local type definitions
//

enum
{
    FrameParserNoError				= PlayerNoError,
    FrameParserError				= PlayerError,

    FrameParserNoStreamParameters		= BASE_FRAME_PARSER,
    FrameParserPartialFrameParameters,
    FrameParserUnhandledHeader,
    FrameParserHeaderSyntaxError,
    FrameParserHeaderUnplayable,
    FrameParserStreamSyntaxError,

    FrameParserFailedToCreateReversePlayStacks,

    FrameParserFailedToAllocateBuffer,

    FrameParserReferenceListConstructionDeferred,
    FrameParserInsufficientReferenceFrames
};

typedef PlayerStatus_t	FrameParserStatus_t;

//

enum
{
    FrameParserFnRegisterOutputBufferRing	= BASE_FRAME_PARSER,
    FrameParserFnInput,
    FrameParserFnTranslatePlaybackTimeNativeToNormalized,
    FrameParserFnTranslatePlaybackTimeNormalizedToNative,
    FrameParserFnApplyCorrectiveNativeTimeWrap,

    FrameParserFnSetModuleParameters
};

//

enum
{
    FrameParserHeaderFlagPartitionPoint			= 0x0001,
    FrameParserHeaderFlagPossibleReversiblePoint	= 0x0002,
    FrameParserHeaderFlagConfirmReversiblePoint		= 0x0004,
};

typedef unsigned int    FrameParserHeaderFlag_t;

// ---------------------------------------------------------------------
//
// Class definition
//

class FrameParser_c : public BaseComponentClass_c
{
public:

    virtual FrameParserStatus_t   RegisterOutputBufferRing(	Ring_t			  Ring ) = 0;

    virtual FrameParserStatus_t   Input(			Buffer_t		  CodedBuffer ) = 0;

    virtual FrameParserStatus_t   TranslatePlaybackTimeNativeToNormalized(
								unsigned long long	  NativeTime,
								unsigned long long	 *NormalizedTime ) = 0;

    virtual FrameParserStatus_t   TranslatePlaybackTimeNormalizedToNative(
								unsigned long long	  NormalizedTime, 
								unsigned long long	 *NativeTime ) = 0;

    virtual FrameParserStatus_t   ApplyCorrectiveNativeTimeWrap( void ) = 0;

    //
    // Additions to support H264 framedecode (rather than slice), and reverible collation
    //

    virtual FrameParserStatus_t   ResetCollatedHeaderState(	void ) = 0;
    virtual unsigned int	  RequiredPresentationLength(	unsigned char		  StartCode ) = 0;
    virtual FrameParserStatus_t   PresentCollatedHeader(	unsigned char		  StartCode,
								unsigned char		 *HeaderBytes,
								FrameParserHeaderFlag_t	 *Flags ) = 0;
};

// ---------------------------------------------------------------------
//
// Docuentation
//

/*! \class FrameParser_c
\brief Responsible for parsing coded frames extract metadata from the frame.

The frame parser class is responsible for taking individual coded 
frames parsing them to extract parameters controlling the decode and 
manifestation of the frame, and passing the coded frame on to the 
output ring for passing to the codec for decoding. This is a list of 
its entrypoints, and a partial list of the calls it makes, and the data 
structures it accesses, these are in addition to the standard component 
class entrypoints, and the complete list of support entrypoints in the 
Player class.

The partial list of entrypoints used by this class:

- Empty list.

The partial list of meta data types used by this class:

- Attached to input buffers:
  - <b>CodedFrameParameters</b>, Describes the coded frame output.
  - <b>StartCodeList</b>, Optional output attachment for those collators generating a start code scan.

- Added to output buffers:
  - <b>ParsedFrameParameters</b>, Contains the parsed frame/stream parameters pointers, and the assigned frame index.
  - <b>Parsed[Video|Audio|Data]Parameters</b>, Optional output attachment containing the descriptive parameters for a frame used in manifestation/timing. 

*/

/*! \fn FrameParserStatus_t FrameParser_c::RegisterOutputBufferRing(Ring_t Ring)
\brief Register the ring on which parsed frame buffers are to be placed.

This function is used to register the ring on which parsed frame buffers are to be placed. 

\param Ring A pointer to a Ring_c instance.

\return Frame parser status code, FrameParserNoError indicates success.
*/

/*! \fn FrameParserStatus_t FrameParser_c::Input(Buffer_t CodedBuffer)
\brief Accept input from a collator.

This function accepts a coded frame buffer, as output from a collator.

\param CodedBuffer A pointer to a Buffer_c instance.

\return Frame parser status code, FrameParserNoError indicates success.
*/

/*! \fn FrameParserStatus_t FrameParser_c::TranslatePlaybackTimeNativeToNormalized(unsigned long long NativeTime, unsigned long long *NormalizedTime)
\brief Translate a stream time value to player intenal value.

This function translates a 'native' stream time (e.g. in STC/PTS units for PES)
to 'normalized' player internal time value (in microseconds).  See
\ref time for more information.

\param NativeTime     Native stream time.
\param NormalizedTime Pointer to a variable to hold the normalized time.

\return Frame parser status code, FrameParserNoError indicates success.
*/

/*! \fn FrameParserStatus_t FrameParser_c::TranslatePlaybackTimeNormalizedToNative(unsigned long long NormalizedTime, unsigned long long *NativeTime)
\brief 

Takes as parameters a long long normalized playback time and a pointer to a long long variable to hold the native time.
\brief Translate a stream time value to player intenal value.

This function translates a 'normalized' player internal time value (in 
microseconds) to 'native' stream time (e.g. in STC/PTS units for PES).
See \ref time for more information.


\param NormalizedTime Normalized player internal time.
\param NativeTime     Pointer to a variable to hold the native time.

\return Frame parser status code, FrameParserNoError indicates success.
*/

/*! \fn FrameParserStatus_t FrameParser_c::ApplyCorrectiveNativeTimeWrap()
\brief Pretend there has been a wrap of the PTS.

This function pretends there has been a wrap of the PTS, used to 
correct wrap state when some streams in a playback start off on one 
side of a PTS wrap point, while others start off on the opposite side.

\return Frame parser status code, FrameParserNoError indicates success.
*/

/*! \fn FrameParserStatus_t FrameParser_c::ResetCollatedHeaderState()
\brief Forget any previously seen headers, when presented with a new header

This function allows the header presentation mechansim to be reset

\return Frame parser status code, FrameParserNoError indicates success.
*/

/*! \fn unsigned int FrameParser_c::RequiredPresentationLength(unsigned char StartCode)
\brief How many bytes does the presentation function need to see with this start code

This function allows the collator to know how many bytes need to be shown along with the actual start code, to the header presentation routine.

\param StartCode    The startcode that we are talking about
\return number of header bytes required to be shown
*/

/*! \fn FrameParserStatus_tFrameParser_c::PresentCollatedHeader( unsigned char   StartCode, unsigned char   *HeaderBytes, FrameParserHeaderFlag_t   *Flags )
\brief What does this header mean

This function allows the collator to know whether or not this start code implies a new frame, or any other action within the collator

\param StartCode    The startcode that we are talking about
\param HeaderBytes  Additional header bytes (number as specified in RequiredPresentationLength()).
\param Flags        The returned indicator of this header's meaning
\return number of header bytes required to be shown
*/

#endif
