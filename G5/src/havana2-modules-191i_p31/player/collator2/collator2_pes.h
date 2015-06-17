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

Source file name : collator2_pes.h
Author :           Nick

Definition of the base collator2 pes class implementation for player 2.


Date        Modification                                    Name
----        ------------                                    --------
20-Nov-06   Created                                         Nick

************************************************************************/

#ifndef H_COLLATOR2_PES
#define H_COLLATOR2_PES

// /////////////////////////////////////////////////////////////////////
//
//      Include any component headers

#include "pes.h"
#include "bitstream_class.h"
#include "collator2_base.h"

// /////////////////////////////////////////////////////////////////////////
//
// Locally defined structures
//

#ifndef H_COLLATOR_PES
#define SPANNING_HEADER_SIZE    3

typedef enum
{
    HeaderZeroStartCode,
    HeaderPesStartCode,
    HeaderPaddingStartCode,
    HeaderGenericStartCode
} PartialHeaderType_t;
#endif

// /////////////////////////////////////////////////////////////////////////
//
// The class definition
//

class Collator2_Pes_c : public Collator2_Base_c
{
protected:

    // Data

    bool                  GotPartialHeader;			// New style partial header mangement
    PartialHeaderType_t	  GotPartialType;
    unsigned int          GotPartialCurrentSize;
    unsigned int          GotPartialDesiredSize;
    unsigned char        *StoredPartialHeader;
    unsigned char	  StoredPartialHeaderCopy[256];

    unsigned int          Skipping;

    unsigned int          RemainingLength;
    unsigned char        *RemainingData;

    unsigned int          PesPacketLength;
    unsigned int          PesPayloadLength;

    bool                  PlaybackTimeValid;
    bool                  DecodeTimeValid;
    unsigned long long    PlaybackTime;
    unsigned long long    DecodeTime;
    bool                  UseSpanningTime;
    bool                  SpanningPlaybackTimeValid;
    unsigned long long    SpanningPlaybackTime;
    bool                  SpanningDecodeTimeValid;
    unsigned long long    SpanningDecodeTime;

    BitStreamClass_c      Bits;

    // Functions

    CollatorStatus_t   ReadPesHeader( unsigned char	*PesHeader );

    // Overridable functions

    virtual CollatorStatus_t   FindNextStartCode(	unsigned int		 *CodeOffset );
    virtual CollatorStatus_t   FindPreviousStartCode(	unsigned int		 *CodeOffset );

//

public:

    //
    // Constructor/Destructor methods
    //

    Collator2_Pes_c(     void );
    ~Collator2_Pes_c(    void );

    //
    // Base overrides
    //

    CollatorStatus_t   Halt(                    void );
    CollatorStatus_t   Reset(                   void );

    //
    // Collator class functions
    //

    CollatorStatus_t   DiscardAccumulatedData(  void );

    CollatorStatus_t   InputJump(               bool                      SurplusDataInjected,
						bool                      ContinuousReverseJump );
};

#endif

