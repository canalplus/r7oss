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

Source file name : collator_pes_audio_mlp.h
Author :           Sylvain

Definition of the base collator pes class implementation for player 2.


Date        Modification                                    Name
----        ------------                                    --------
08-Oct-07   Creation                                        Sylvain

************************************************************************/

#ifndef H_COLLATOR_PES_AUDIO_MLP
#define H_COLLATOR_PES_AUDIO_MLP

// /////////////////////////////////////////////////////////////////////
//
//	Include any component headers

#include "collator_pes_audio_dvd.h"

// /////////////////////////////////////////////////////////////////////////
//
// Locally defined structures
//

// /////////////////////////////////////////////////////////////////////////
//
// The class definition
//

class Collator_PesAudioMlp_c : public Collator_PesAudioDvd_c
{
protected:

    CollatorStatus_t FindNextSyncWord( int *CodeOffset );
    CollatorStatus_t DecideCollatorNextStateAndGetLength( unsigned int *FrameLength );
    void             SetPesPrivateDataLength( unsigned char SpecificCode );
    CollatorStatus_t HandlePesPrivateData( unsigned char *PesPrivateData );

    int AccumulatedFrameNumber;   ///< Holds the number of audio frames accumulated
    int NbFramesToGlob;           ///< Number of audio frames to glob (depends on sampling frequency)
    int StuffingBytesLength;      ///< Number of stuffing bytes in the pda (concerns MLP in DVD-Audio)
public:

    Collator_PesAudioMlp_c();

    CollatorStatus_t   Reset(			void );
};

#endif // H_COLLATOR_PES_AUDIO_MLP

