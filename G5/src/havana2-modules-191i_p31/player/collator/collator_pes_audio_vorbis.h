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

Source file name : collator_pes_audio_vorbis.h
Author :           Julian

Definition of the Ogg Vorbis collator pes class implementation for player 2.


Date        Modification                                    Name
----        ------------                                    --------
27-Jan-09   Created from existing collator_pes_video_wmv.h  Julian

************************************************************************/

#ifndef H_COLLATOR_PES_AUDIO_VORBIS
#define H_COLLATOR_PES_AUDIO_VORBIS

// /////////////////////////////////////////////////////////////////////
//
//      Include any component headers

#include "collator_pes_frame.h"

// /////////////////////////////////////////////////////////////////////////
//
// Locally defined structures
//

// /////////////////////////////////////////////////////////////////////////
//
// The class definition
//

/*
The Ogg Vorbis is very similar to the wmv collator because .ogg files
contain the audio in frame chunks.  The frame length is passed from the container parser
in a private structure. This is passed into the player using the private data
structure in the pes header.
*/

class Collator_PesAudioVorbis_c : public Collator_PesFrame_c
{
};

#endif /* H_COLLATOR_PES_AUDIO_VORBIS */

