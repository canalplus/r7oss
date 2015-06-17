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

#ifndef H_FRAME_PARSER_VIDEO_VP8
#define H_FRAME_PARSER_VIDEO_VP8

#include "vp8.h"
#include "frame_parser_video.h"

#undef TRACE_TAG
#define TRACE_TAG "FrameParser_VideoVp8_c"

#define VP8_REFERENCE_FRAMES_FOR_FORWARD_DECODE                 3

//  VP8 Data Format and Decoding Guide Chapter 7: Boolean Entropy Decoder
//  WebM Project Page 21,22,23 of 104
#if 1
//{{{  BoolDecoder_c
//{{{  Length table from dvbtest mkv.c
static const  unsigned char     VintLength[]    =
{
    0, 7, 6, 6,                                                /* 0 - 3 */
    5, 5, 5, 5,                                                /* 4 - 7 */
    4, 4, 4, 4, 4, 4, 4, 4,                                    /* 8 - 15 */
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,            /* 16 - 31 */
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,            /* 32 - 47 */
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,            /* 48 - 63 */
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,            /* 64 - 79 */
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,            /* 80 - 95 */
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,            /* 96 - 111 */
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,            /* 112 - 127 */
};
//}}}

//  Decoder state exactly parallels that of the encoder.
//  "value", together with the remaining input, equals the complete encoded
//  number x less the left endpoint of the current coding interval.
class BoolDecoder_c
{
public:
    // Call this function before reading any bools from the partition.
    void Init(unsigned char   *StartPartition)
    {
        Input                   = StartPartition;       // ptr to next byte to be read
        Value                   = 0;                    // value = first 24 input bytes
        Range                   = 255;                  // initial range is full
        BitCount                = -8;                   // have not yet shifted out any bits
        Fill();
    }

    // Convenience function reads a "literal", that is, a "num_bits" wide
    // unsigned value whose bits come high- to low-order, with each bit
    // encoded at probability 128 (i.e., 1/2).
    unsigned int GetBits(int NumBits)
    {
        unsigned int    RetVal  = 0;

        while (NumBits--)
        {
            RetVal              = (RetVal << 1) + ReadBool(128);
        }

        return RetVal;
    }

    inline unsigned int GetBit(void)
    {
        return ReadBool(128);
    }

private:
    unsigned char              *Input;          // pointer to next compressed data byte
    unsigned int                Range;          // always identical to encoder's range
    unsigned int                Value;          // contains at least 24 significant bits
    int                         BitCount;       // # of bits shifted out of value, at most 7

    void Fill(void)
    {
        for (int Shift = ((sizeof(unsigned int) * 8) - 8) - (BitCount + 8); Shift >= 0;)
        {
            BitCount       += 8;
            Value          |= (unsigned int) * Input++ << Shift;
            Shift          -= 8;
        }
    }

    // Main function reads a bool encoded at probability prob/256, which of
    // course must agree with the probability used when the bool was written.
    int ReadBool(unsigned int Probability)
    {
        // Range and split are identical to the corresponding values used
        // by the encoder when this bool was written
        unsigned int            Split           = 1 + (((Range - 1) * Probability) >> 8);
        unsigned int            BigSplit        = Split << ((sizeof(unsigned int) * 8) - 8);
        unsigned int            NewRange        = Split;
        int                     Bit             = 0;    // will be 0 or 1

        if (Value >= BigSplit)
        {
            // encoded a one
            NewRange            = Range - Split;        // reduce range
            Value              -= BigSplit;             // subtract left endpoint of interval
            Bit                 = 1;
        }

        if (NewRange < 128)
        {
            unsigned int        Shift;
            Shift               = VintLength[NewRange];
            NewRange          <<= Shift;
            Value             <<= Shift;
            BitCount           -= Shift;
        }

        Range                   = NewRange;

        if (BitCount < 0)
        {
            Fill();
        }

        return Bit;
    }
};
//}}}
#else
//{{{  BoolDecoder_c
typedef size_t VP8_BD_VALUE;

# define VP8_BD_VALUE_SIZE ((int)sizeof(VP8_BD_VALUE)*8)
static const unsigned char vp8dx_bitreader_norm[256] =
{
    0, 7, 6, 6, 5, 5, 5, 5, 4, 4, 4, 4, 4, 4, 4, 4, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};
#define VP8DX_BOOL_DECODER_FILL(_count,_value,_bufptr) \
    do \
    { \
        int shift; \
        for(shift = VP8_BD_VALUE_SIZE - 8 - ((_count) + 8); shift >= 0; ) \
        { \
            (_count) += 8; \
            (_value) |= (VP8_BD_VALUE)*(_bufptr)++ << shift; \
            shift -= 8; \
        } \
    } \
    while (0);

//  Decoder state exactly parallels that of the encoder.
//  "value", together with the remaining input, equals the complete encoded
//  number x less the left endpoint of the current coding interval.
class BoolDecoder_c
{
private:
    unsigned char              *Input;          // pointer to next compressed data byte
    unsigned int                Range;          // always identical to encoder's range
    unsigned int                Value;          // contains at least 24 significant bits
    int                         BitCount;       // # of bits shifted out of value, at most 7


    void Fill(void)
    {
        unsigned char *bufptr;
        VP8_BD_VALUE         value;
        int                  count;
        bufptr = Input;
        value = Value;
        count = BitCount;
        VP8DX_BOOL_DECODER_FILL(count, value, bufptr);
        Input = bufptr;
        Value = value;
        BitCount = count;
    }

    // Main function reads a bool encoded at probability prob/256, which of
    // course must agree with the probability used when the bool was written.
    int ReadBool(unsigned int probability)
    {
        unsigned int bit = 0;
        VP8_BD_VALUE value;
        unsigned int split;
        VP8_BD_VALUE bigsplit;
        int count;
        unsigned int range;
        value = Value;
        count = BitCount;
        range = Range;
        split = 1 + (((range - 1) * probability) >> 8);
        bigsplit = (VP8_BD_VALUE)split << (VP8_BD_VALUE_SIZE - 8);
        range = split;

        if (value >= bigsplit)
        {
            range = Range - split;
            value = value - bigsplit;
            bit = 1;
        }

        {
            register unsigned int shift = vp8dx_bitreader_norm[range];
            range <<= shift;
            value <<= shift;
            count -= shift;
        }

        Value = value;
        BitCount = count;
        Range = range;

        if (count < 0)
        {
            Fill();
        }

        return bit;
    }

    DISALLOW_COPY_AND_ASSIGN(BoolDecoder_c);

public:
    // Call this function before reading any bools from the partition.
    void Init(unsigned char   *StartPartition)
    {
        Input                   = StartPartition;       // ptr to next byte to be read
        Value                   = 0;                    // value = first 24 input bytes
        Range                   = 255;                  // initial range is full
        BitCount                = -8;                   // have not yet shifted out any bits
        Fill();
    }

    // Convenience function reads a "literal", that is, a "num_bits" wide
    // unsigned value whose bits come high- to low-order, with each bit
    // encoded at probability 128 (i.e., 1/2).
    unsigned int GetBits(int NumBits)
    {
        int z = 0;
        int bit;

        for (bit = NumBits - 1; bit >= 0; bit--)
        {
            z |= (ReadBool(0x80) << bit);
        }

        return z;
    }

    inline unsigned int GetBit(void)
    {
        return ReadBool(128);
    }

};
//}}}
#endif

/// Frame parser for Vp8
class FrameParser_VideoVp8_c : public FrameParser_Video_c
{
public:
    FrameParser_VideoVp8_c(void);
    ~FrameParser_VideoVp8_c(void);

    //
    // FrameParser class functions
    //

    FrameParserStatus_t         Connect(Port_c *Port);

    //
    // Stream specific functions
    //

    FrameParserStatus_t         ReadHeaders(void);

    FrameParserStatus_t         PrepareReferenceFrameList(void);

    FrameParserStatus_t         ForPlayUpdateReferenceFrameList(void);

    FrameParserStatus_t         RevPlayProcessDecodeStacks(void);

    void                        CalculateFrameIndexAndPts(ParsedFrameParameters_t         *ParsedFrame, ParsedVideoParameters_t         *ParsedVideo);

private:
    class BoolDecoder_c         BoolDecoder;

    Vp8StreamParameters_t      *StreamParameters;
    Vp8FrameParameters_t       *FrameParameters;
    Vp8StreamParameters_t       CopyOfStreamParameters;

    Rational_t                  FrameRate;
    struct Vp8MetaData_s        MetaData;
    bool                        StreamMetadataValid;

    ReferenceFrameList_t        ReferenceFrameList[VP8_NUM_REF_FRAME_LISTS];

    FrameParserStatus_t         ReadStreamMetadata(void);
    FrameParserStatus_t         ReadPictureHeader(void);

    FrameParserStatus_t         CommitFrameForDecode(void);
    bool                        NewStreamParametersCheck(void);

    DISALLOW_COPY_AND_ASSIGN(FrameParser_VideoVp8_c);
};

#endif
