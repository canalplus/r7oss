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

#ifndef H_FRAME_PARSER_VIDEO_VP6
#define H_FRAME_PARSER_VIDEO_VP6

#include "vp6.h"
#include "frame_parser_video.h"

#undef TRACE_TAG
#define TRACE_TAG "FrameParser_VideoVp6_c"

#define VP6_REFERENCE_FRAMES_FOR_FORWARD_DECODE    1

#define VP6_MIN_FRAME_WIDTH                        16
#define VP6_MAX_FRAME_WIDTH                        640
#define VP6_MIN_FRAME_HEIGHT                       16
#define VP6_MAX_FRAME_HEIGHT                       480

/* Range code class for accessing arithmetically coded parts of the bitstream */
class RangeCoder_c
{
public:
    /* Initialise the pointer */
    void Init(unsigned char *Pointer)
    {
        High            = 0xff;
        Bits            = 8;
        Buffer          = Pointer;
        Data            = *Buffer++ << 8;
        Data           |= *Buffer++;
    }

    /* Retrieve a single bit */
    int GetBit(void)
    {
        int                 Low             = (High + 1) >> 1;
        unsigned int        LowShift        = Low << 8;
        int                 Bit             = Data >= LowShift;

        if (Bit)
        {
            High            = (High - Low) << 1;
            Data           -= LowShift;
        }
        else
        {
            High            = Low << 1;
        }

        /* normalize */
        Data              <<= 1;

        if (--Bits == 0)
        {
            Bits            = 8;
            Data           |= *Buffer++;
        }

        return Bit;
    }

    int GetBits(int Bits)
    {
        int Value   = 0;

        while (Bits--)
        {
            Value   = (Value << 1) | GetBit();
        }

        return Value;
    }

    int HighVal(void)
    {
        return High;
    }

    int BitsVal(void)
    {
        return Bits;
    }

    unsigned int DataVal(void)
    {
        return Data;
    }

    unsigned char *BufferVal(void)
    {
        return Buffer;
    }
protected:
    unsigned int                Data;
    int                         High;
    int                         Bits;
    unsigned char              *Buffer;
};

/// Frame parser for Vp6
class FrameParser_VideoVp6_c : public FrameParser_Video_c
{
public:
    FrameParser_VideoVp6_c(void);
    ~FrameParser_VideoVp6_c(void);

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

private:
    class RangeCoder_c          RangeDecoder;

    Vp6StreamParameters_t      *StreamParameters;
    Vp6FrameParameters_t       *FrameParameters;
    Vp6StreamParameters_t       CopyOfStreamParameters;

    struct Vp6MetaData_s        MetaData;
    bool                        StreamMetadataValid;
    Rational_t                  FrameRate;

    FrameParserStatus_t         ReadStreamMetadata(void);
    FrameParserStatus_t         ReadPictureHeader(void);

    FrameParserStatus_t         CommitFrameForDecode(void);
    bool                        NewStreamParametersCheck(void);

    DISALLOW_COPY_AND_ASSIGN(FrameParser_VideoVp6_c);
};

#endif

