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

Source file name : codec_mme_video.h
Author :           Nick

Definition of the codec video class implementation for player 2.


Date        Modification                                    Name
----        ------------                                    --------
20-Jan-07   Created                                         Nick

************************************************************************/

#ifndef H_CODEC_MME_VIDEO
#define H_CODEC_MME_VIDEO

// /////////////////////////////////////////////////////////////////////
//
//      Include any component headers

#include "VideoCompanion.h"
#include "codec_mme_base.h"

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

class Codec_MmeVideo_c : public Codec_MmeBase_c
{
protected:

    // Data

    VideoOutputSurfaceDescriptor_t       *VideoOutputSurface;

    ParsedVideoParameters_t              *ParsedVideoParameters;
    bool                                  KnownLastSliceInFieldFrame;

    // Functions

public:

    //
    // Overrides for component base class functions
    //

    CodecStatus_t   Halt(       void );
    CodecStatus_t   Reset(      void );

    //
    // Codec class functions
    //

    CodecStatus_t   RegisterOutputBufferRing(   Ring_t            Ring );
    CodecStatus_t   Input(                      Buffer_t          CodedBuffer );

    //
    // Extension to base functions
    //

    CodecStatus_t   InitializeDataTypes(        void );

    //
    // Implementation of fill out function for generic video,
    // may be overidden if necessary.
    //

    virtual CodecStatus_t   FillOutDecodeBufferRequest(	BufferStructure_t	 *Request );
};
#endif
