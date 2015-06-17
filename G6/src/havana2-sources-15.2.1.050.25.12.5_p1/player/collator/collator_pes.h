/************************************************************************
Copyright (C) 2003-2014 STMicroelectronics. All Rights Reserved.

This file is part of the Streaming Engine.

Streaming Engine is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

Streaming Engine is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with Streaming Engine; see the file COPYING.  If not, write to the Free
Software Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307,
USA.

The Streaming Engine Library may alternatively be licensed under a
proprietary license from ST.
************************************************************************/

#ifndef H_COLLATOR_PES
#define H_COLLATOR_PES

#include "pes.h"
#include "bitstream_class.h"
#include "collator_base.h"

#undef  TRACE_TAG
#define TRACE_TAG "Collator_Pes_c"

#define SPANNING_HEADER_SIZE    3

typedef enum
{
    HeaderZeroStartCode,
    HeaderPesStartCode,
    HeaderPaddingStartCode,
    HeaderControlStartCode,
    HeaderGenericStartCode
} PartialHeaderType_t;

typedef struct AudioDescriptionInfo_s
{
    //AD related parameter
    unsigned int       ADFadeValue;
    unsigned int       ADPanValue;
    unsigned int       ADGainCenter ;
    unsigned int       ADGainFront ;
    unsigned int       ADGainSurround ;
    // Flag to check the validity of the AD information
    bool           ADValidFlag;
    //Flag to check whether AD content is available or not in the stream.
    bool                   ADInfoAvailable;
} AudioDescriptionInfo_t;

class Collator_Pes_c : public Collator_Base_c
{
public:
    Collator_Pes_c(void);
    ~Collator_Pes_c(void);

    CollatorStatus_t   Halt(void);

    //
    // Collator class functions
    //

    CollatorStatus_t   DiscardAccumulatedData(void);

    CollatorStatus_t   InputJump(bool                      SurplusDataInjected,
                                 bool                      ContinuousReverseJump,
                                 bool                      FromDiscontinuityControl);

protected:
    bool                  SeekingPesHeader;

    bool                  GotPartialHeader;         // New style partial header management
    PartialHeaderType_t   GotPartialType;
    unsigned int          GotPartialCurrentSize;
    unsigned int          GotPartialDesiredSize;
    unsigned char        *StoredPartialHeader;
    unsigned char         StoredPartialHeaderCopy[256];

    bool                  GotPartialZeroHeader;         // Following 3 partial wossnames are now for divx old code support only
    unsigned int          GotPartialZeroHeaderBytes;
    unsigned char        *StoredZeroHeader;

    bool                  GotPartialPesHeader;
    unsigned int          GotPartialPesHeaderBytes;
    unsigned char        *StoredPesHeader;

    bool                  GotPartialPaddingHeader;
    unsigned int          GotPartialPaddingHeaderBytes;
    unsigned char        *StoredPaddingHeader;
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
    unsigned int          ControlUserData[STM_SE_STREAM_CONTROL_USER_DATA_SIZE];

    BitStreamClass_c        Bits;
    AudioDescriptionInfo_t  AudioDescriptionInfo;
    bool                    IsMpeg4p2Stream;

    // Functions

    virtual CollatorStatus_t   FindNextStartCode(unsigned int             *CodeOffset);
    virtual CollatorStatus_t   SetAlarmParsedPts(void);
    CollatorStatus_t   ReadPesHeader(void);
    CollatorStatus_t   ReadControlUserData(void);

private:
    DISALLOW_COPY_AND_ASSIGN(Collator_Pes_c);
};

#endif

