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

Source file name : collator_pes_audio_dtshd.h
Author :           Sylvain

Definition of the base collator pes class implementation for player 2.


Date        Modification                                    Name
----        ------------                                    --------
07-Jun-07   Ported from dts on Player 1                     Sylvain

************************************************************************/

#ifndef H_COLLATOR_PES_AUDIO_DTSHD
#define H_COLLATOR_PES_AUDIO_DTSHD

// /////////////////////////////////////////////////////////////////////
//
//	Include any component headers

#include "collator_pes_audio_dvd.h"
#include "dtshd.h"

// /////////////////////////////////////////////////////////////////////////
//
// Locally defined structures
//

// /////////////////////////////////////////////////////////////////////////
//
// The class definition
//

class Collator_PesAudioDtshd_c : public Collator_PesAudioDvd_c
{
 private:
  bool             EightChannelsRequired; ///< Holds the output channel required configuration
  /// \todo This variable should be updated according to the manifestor constraints via SetModuleParameters
  ///
  DtshdAudioSyncFrameHeader_t SyncFrameHeader;

  BitStreamClass_c Bits;                  ///< will be used to parse frame header

protected:
  
  CollatorStatus_t FindNextSyncWord( int *CodeOffset );
  CollatorStatus_t FindAnyNextSyncWord( int *CodeOffset, DtshdStreamType_t * Type );
  
  CollatorStatus_t DecideCollatorNextStateAndGetLength( unsigned int *FrameLength );
  void             GetStreamType( unsigned char * Header, DtshdAudioParsedFrameHeader_t * ParsedFrameHeader);
  void             SetPesPrivateDataLength(unsigned char SpecificCode);
  CollatorStatus_t HandlePesPrivateData(unsigned char *PesPrivateData);
  CollatorStatus_t GetSpecificFrameLength(unsigned int * FrameLength);
    
public:

  //  unsigned int     RealFrameSize;
  unsigned int     CoreFrameSize;
  bool             GotCoreFrameSize;

  Collator_PesAudioDtshd_c();
  
  CollatorStatus_t   Reset(			void );
};

#endif // H_COLLATOR_PES_AUDIO_DTSHD

