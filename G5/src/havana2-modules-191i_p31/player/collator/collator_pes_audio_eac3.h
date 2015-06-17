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

Source file name : collator_pes_audio_eac3.h
Author :           Sylvain

Definition of the base collator pes class implementation for player 2.


Date        Modification                                    Name
----        ------------                                    --------
23-May-07   Created from existing collator_pes_audio_ac3.h  Sylvain

************************************************************************/

#ifndef H_COLLATOR_PES_AUDIO_EAC3
#define H_COLLATOR_PES_AUDIO_EAC3

// /////////////////////////////////////////////////////////////////////
//
//	Include any component headers

#include "collator_pes_audio_dvd.h"
#include "eac3_audio.h"

// /////////////////////////////////////////////////////////////////////////
//
// Locally defined structures
//

// /////////////////////////////////////////////////////////////////////////
//
// The class definition
//

class Collator_PesAudioEAc3_c : public Collator_PesAudioDvd_c
{
 private:
 
  bool             EightChannelsRequired; ///< Holds the output channel required configuration
  int              ProgrammeId;           ///< Identifier if the eac3 programme to be decoded
  int              NbAccumulatedSamples;  ///< The collator has to accumulate 1536 samples so that the eac3->ac3 transcoding is possible (for spdif output)
  

  /// \todo This variable should be updated according to the manifestor constraints via SetModuleParameters
  ///


protected:
  
  CollatorStatus_t FindNextSyncWord( int *CodeOffset );
  
  CollatorStatus_t DecideCollatorNextStateAndGetLength( unsigned int *FrameLength );
  void             GetStreamType( unsigned char * Header, EAc3AudioParsedFrameHeader_t * ParsedFrameHeader);
  void             SetPesPrivateDataLength(unsigned char SpecificCode);
  CollatorStatus_t HandlePesPrivateData(unsigned char *PesPrivateData);
    
public:

  Collator_PesAudioEAc3_c();
  
  CollatorStatus_t   Reset(			void );
};

#endif // H_COLLATOR_PES_AUDIO_EAC3

