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

Source file name : codec_dummy.h
Author :           Nick

Definition of the dummy codec class implementation for player 2.


Date        Modification                                    Name
----        ------------                                    --------
16-Jan-07   Created                                         Nick

************************************************************************/

#ifndef H_CODEC_DUMMY
#define H_CODEC_DUMMY

// /////////////////////////////////////////////////////////////////////
//
//	Include any component headers

#include "player.h"

// /////////////////////////////////////////////////////////////////////////
//
// Locally defined structures
//

// /////////////////////////////////////////////////////////////////////////
//
// The class definition
//

class Codec_Dummy_c : public Codec_c
{
public:

    CodecTrickModeParameters_t	  CodecTrickModeParameters;

    //
    // Constructor/Destructor methods
    //

    Codec_Dummy_c( 	void ){ report( severity_info, "Codec_Dummy_c - Called\n" ); }
    ~Codec_Dummy_c( 	void ){ report( severity_info, "~Codec_Dummy_c - Called\n" ); }

    //
    // Other functions
    //

    CodecStatus_t   GetTrickModeParameters(	CodecTrickModeParameters_t *TrickModeParameters )
	    {
		CodecTrickModeParameters.SubstandardDecodeSupported	= false;
		CodecTrickModeParameters.MaximumNormalDecodeRate	= 2;
		CodecTrickModeParameters.MaximumSubstandardDecodeRate	= 3;
		CodecTrickModeParameters.DefaultGroupSize		= 12;
		CodecTrickModeParameters.DefaultGroupReferenceFrameCount= 4;

		*TrickModeParameters	= CodecTrickModeParameters;
		return CodecNoError;
	    }

//

    CodecStatus_t   SetModuleParameters(	unsigned int   ParameterBlockSize,
						void          *ParameterBlock )
		{ report( severity_info, "Codec_Dummy_c::SetModuleParameters - Called\n" ); return CodecNoError; }

    CodecStatus_t   DiscardQueuedDecodes(       void )
		{ report( severity_info, "Codec_Dummy_c::DiscardQueuedDecodes - Called\n" ); return CodecNoError; }

    CodecStatus_t   CheckReferenceFrameList(    unsigned int              NumberOfReferenceFrameLists,
                                                ReferenceFrameList_t      ReferenceFrameList[] )
		{ report( severity_info, "Codec_Dummy_c::CheckReferenceFrameList - Called\n" ); return CodecNoError; }

    CodecStatus_t   RegisterOutputBufferRing(	Ring_t		  Ring )
		{ report( severity_info, "Codec_Dummy_c::RegisterOutputBufferRing - Called\n" ); return CodecNoError; }

    CodecStatus_t   OutputPartialDecodeBuffers(	void )
		{ report( severity_info, "Codec_Dummy_c::OutputPartialDecodeBuffers - Called\n" ); return CodecNoError; };

    CodecStatus_t   ReleaseReferenceFrame(	unsigned int	  ReferenceFrameDecodeIndex )
		{ report( severity_info, "Codec_Dummy_c::ReleaseReferenceFrame - Called\n" ); return CodecNoError; }

    CodecStatus_t   ReleaseDecodeBuffer(	Buffer_t	  Buffer )
		{ report( severity_info, "Codec_Dummy_c::ReleaseDecodeBuffer - Called\n" ); return CodecNoError; }

    CodecStatus_t   Input(			Buffer_t	  CodedBuffer )
		{ report( severity_info, "Codec_Dummy_c::Input - Called\n" ); /*BufferManager->Dump(DumpAll);*/ return CodecNoError; }
};

#endif

