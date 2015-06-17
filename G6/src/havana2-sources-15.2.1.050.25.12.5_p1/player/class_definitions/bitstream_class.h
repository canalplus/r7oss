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

#ifndef H_BITSTREAM_CLASS
#define H_BITSTREAM_CLASS

#include "osinline.h"

class BitStreamClass_c
{
public:
    BitStreamClass_c(void)
        : BitsAvailable(0)
        , BitsWord(0)
        , BitsData(NULL)
    {
    }

    // ////////////////////////////////////////////////////////////////////////
    //
    // Set the pointer
    //

    void    SetPointer(unsigned char     *Pointer)
    {
        unsigned int    Ptr = (unsigned int)Pointer;
        BitsData    = (unsigned int *)(Ptr & 0xfffffffc);
        BitsAvailable   = 32 - ((Ptr & 0x3) * 8);
        BitsWord    = __swapbw(*(BitsData++));
    }

    // ////////////////////////////////////////////////////////////////////////
    //
    // Get and flush some bits
    //

    unsigned int   Get(unsigned int   N)
    {
        //
        // Ensure we have enough data
        //
        if (BitsAvailable < N)
        {
            BitsWord        = (BitsWord << 32) | __swapbw(*(BitsData++));
            BitsAvailable  += 32;
        }

        //
        // return the appropriate field
        //
        BitsAvailable -= N;
        return (BitsWord >> BitsAvailable) & ((1ULL << N) - 1);
    }

    // ////////////////////////////////////////////////////////////////////////
    //
    // Get and flush some bits
    //

    int   SignedGet(unsigned int   N)
    {
        unsigned int    Value;
        Value   = Get(N);

        if (inrange(N, 2, 31) &&
            (Value >= (1u << (N - 1))))
        {
            Value |= (0xffffffff << (N - 1));
        }

        return (int)Value;
    }

    // ////////////////////////////////////////////////////////////////////////
    //
    // Get but do not flush some bits
    //

    unsigned int   Show(unsigned int  N)
    {
        //
        // Ensure we have enough data
        //
        if (BitsAvailable < N)
        {
            BitsWord        = (BitsWord << 32) | __swapbw(*(BitsData++));
            BitsAvailable  += 32;
        }

        //
        // return the appropriate field
        //
        return (BitsWord >> (BitsAvailable - N)) & ((1ULL << N) - 1);
    }

    // ////////////////////////////////////////////////////////////////////////
    //
    // Get but do not flush some bits
    //

    int   SignedShow(unsigned int   N)
    {
        unsigned int    Value;
        Value   = Show(N);

        if (inrange(N, 2, 31) &&
            (Value >= (1u << (N - 1))))
        {
            Value |= (0xffffffffu << (N - 1));
        }

        return (int)Value;
    }

    // ////////////////////////////////////////////////////////////////////////
    //
    // Flush previously shown bits
    //

    void   Flush(unsigned int   N)
    {
        BitsAvailable -= N;
    }

    // ////////////////////////////////////////////////////////////////////////
    //
    // Flush bits not previously seen
    //

    void   FlushUnseen(unsigned int N)
    {
        //
        // Ensure we have enough data
        //
        while (N > 32)
        {
            BitsWord        = (BitsWord << 32) | __swapbw(*(BitsData++));
            N          -= 32;
        }

        if (BitsAvailable < N)
        {
            BitsWord        = (BitsWord << 32) | __swapbw(*(BitsData++));
            BitsAvailable  += 32;
        }

        //
        // Discard the bits
        //
        BitsAvailable -= N;
    }

    // ////////////////////////////////////////////////////////////////////////
    //
    // Retrieve the bit position
    //

    void  GetPosition(unsigned char         **Pointer,
                      unsigned int           *BitsInByte)
    {
        *Pointer    = (unsigned char *)BitsData - ((BitsAvailable + 7) / 8);
        *BitsInByte = (BitsAvailable & 7) ? (BitsAvailable & 7) : 8;
    }

    // ////////////////////////////////////////////////////////////////////////
    //
    // H264 extension - standard described function for "MoreRsbpData"
    //

    bool  MoreRsbpData(void)
    {
        unsigned char   *Pointer;
        unsigned int     BitsInByte;
        GetPosition(&Pointer, &BitsInByte);
        BitsInByte  += 8;           // We know there should be a zero byte after the body
        // because we insert one in the collation phase
        return (Show(BitsInByte) != (1U << (BitsInByte - 1)));
    }

    // ////////////////////////////////////////////////////////////////////////
    //
    // H264 extension - standard described functions for reading
    //          unsigned and signed coded numbers.
    //

    unsigned int   GetUe(void)
    {
        unsigned int    LeadingZeros;
        LeadingZeros    = __lzcntw(Show(32));
        Flush(LeadingZeros + 1);
        return ((LeadingZeros != 0) ? ((1 << LeadingZeros) - 1 + Get(LeadingZeros)) : 0);
    }

    int   GetSe(void)
    {
        unsigned int    Code;
        //
        // Code is a local GetUe, this is then mapped to an Se in the return.
        //
        Code    = GetUe();
        return ((Code & 1) != 0) ? ((Code >> 1) + 1) : -(Code >> 1);
    }

protected:
    unsigned int        BitsAvailable;
    unsigned long long  BitsWord;
    unsigned int       *BitsData;
};

#endif
