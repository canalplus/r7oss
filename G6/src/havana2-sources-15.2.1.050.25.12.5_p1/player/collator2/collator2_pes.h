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

#ifndef H_COLLATOR2_PES
#define H_COLLATOR2_PES

#include "pes.h"
#include "bitstream_class.h"
#include "collator2_base.h"

#undef  TRACE_TAG
#define TRACE_TAG "Collator2_Pes_c"

#ifndef H_COLLATOR_PES
#define SPANNING_HEADER_SIZE    3


#define IS_START_CODE(x)        (memcmp(RemainingData+x, "\x00\x00\x01", 3) == 0)
#define IS_NOT_START_CODE(x)    (memcmp(RemainingData+x, "\x00\x00\x01", 3) != 0)

#define IS_CONTROL_DATA(x)      IS_START_CODE(x)&&(RemainingData[x+3]==PES_START_CODE_CONTROL)
#define IS_NOT_CONTROL_DATA(x)  IS_START_CODE(x)&&(RemainingData[x+3]!=PES_START_CODE_CONTROL)

typedef enum
{
    HeaderZeroStartCode,
    HeaderPesStartCode,
    HeaderPaddingStartCode,
    HeaderControlStartCode,
    HeaderGenericStartCode
} PartialHeaderType_t;
#endif

typedef enum
{
    StartCodeCutAfterFirstByte,
    StartCodeCutAfterSecondByte,
    StartCodeCutAfterThirdByte
} ControlCuttingStartCodeType_t;

class Collator2_Pes_c : public Collator2_Base_c
{
public:
    Collator2_Pes_c(void);
    ~Collator2_Pes_c(void);

    CollatorStatus_t   Halt(void);

    //
    // Collator class functions
    //

    CollatorStatus_t   DiscardAccumulatedData(void);

    CollatorStatus_t   InputJump(bool                      SurplusDataInjected,
                                 bool                      ContinuousReverseJump,
                                 bool                      FromDiscontinuityControl);

protected:
    bool                  GotPartialHeader;         // New style partial header management

    PartialHeaderType_t   GotPartialType;
    PartialHeaderType_t   CutPartialType;
    unsigned int          GotPartialCurrentSize;
    unsigned int          GotPartialDesiredSize;
    unsigned int          CutGotPartialDesiredSize;
    unsigned int          CutGotPartialCurrentSize;
    unsigned char        *StoredPartialHeader;
    unsigned char         StoredPartialHeaderCopy[256];

    unsigned int          Skipping;

    unsigned int          RemainingLength;
    unsigned char        *RemainingData;

    unsigned int          RemainingSpanningControlData;
    unsigned char         SpanningControlData[PES_CONTROL_SIZE];

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

    BitStreamClass_c      Bits;

    CollatorStatus_t   ReadPesHeader(unsigned char  *PesHeader);

    // Control data functions

    CollatorStatus_t   ReadControlUserData(unsigned char  *PesHeader, bool *NeedToRemoveControlFromESBuffer);
    CollatorStatus_t   SearchForStartCodeCutByControlData(unsigned int AdditionalBytes);
    CollatorStatus_t   RemoveControlAndReposition(unsigned int AdditionalBytes, ControlCuttingStartCodeType_t cutcase);

    int                IsThereControlData(unsigned int  length);
    CollatorStatus_t   ReadAndJumpControlData(unsigned int ControlDataPos);
    CollatorStatus_t   ReadSpanningControlData(void);
    CollatorStatus_t   ReadFirstPartSpanningControlData(unsigned int ControlDataPos);
    unsigned int       CheckControlDatawithinStartCode(unsigned int CodeOffset);
    CollatorStatus_t   CheckBeginingStartCodeSpanningControlData(void);

    // Overridable functions

    virtual CollatorStatus_t   FindNextStartCode(unsigned int        *CodeOffset, unsigned int UpperLimit);
    virtual CollatorStatus_t   FindPreviousStartCode(unsigned int        *CodeOffset);
    virtual CollatorStatus_t   SetAlarmParsedPts(void);

private:
    DISALLOW_COPY_AND_ASSIGN(Collator2_Pes_c);
};

#endif

