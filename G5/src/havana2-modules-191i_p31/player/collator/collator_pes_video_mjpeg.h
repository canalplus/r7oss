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

Source file name : collator_pes_video_mjpeg.h
Author :           Julian

Definition of the base collator pes class implementation for player 2.


Date        Modification                                    Name
----        ------------                                    --------
12-Jun-07   Created from existing collator_pes_video.h      Julian

************************************************************************/

#ifndef H_COLLATOR_PES_VIDEO_MJPEG
#define H_COLLATOR_PES_VIDEO_MJPEG

// /////////////////////////////////////////////////////////////////////
//
//      Include any component headers

#include "collator_pes_video.h"

// /////////////////////////////////////////////////////////////////////////
//
// Locally defined structures
//

// /////////////////////////////////////////////////////////////////////////
//
// The class definition
//

class Collator_PesVideoMjpeg_c : public Collator_PesVideo_c
{
protected:

    // Overrides for base class
    CollatorStatus_t   FindNextStartCode(       unsigned int*           CodeOffset);

public:

    Collator_PesVideoMjpeg_c();

    CollatorStatus_t   Reset(                   void );

    CollatorStatus_t    Input          (PlayerInputDescriptor_t  *Input,
                                        unsigned int              DataLength,
                                        void                     *Data,
					bool			  NonBlocking = false,
					unsigned int		 *DataLengthRemaining = NULL );

};

#endif // H_COLLATOR_PES_VIDEO_MJPEG

