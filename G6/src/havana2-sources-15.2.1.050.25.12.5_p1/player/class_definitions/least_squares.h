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

#ifndef H_LEAST_SQUARES
#define H_LEAST_SQUARES

#include "longlonglong.h"
#include "rational.h"

#undef TRACE_TAG
#define TRACE_TAG "LeastSquares_c"

class LeastSquares_c
{
public:
    LeastSquares_c()
        : Count(0)
        , CumulativeX(0)
        , CumulativeY(0)
        , SigmaXSquared(0)
        , SigmaX(0)
        , SigmaY(0)
        , SigmaYX(0)
    {
    }

    //
    // Reset the accumulation
    //

    void   Reset(void)
    {
        Count       = 0;
        CumulativeX = 0;
        CumulativeY = 0;
        SigmaXSquared = 0;
        SigmaX      = 0;
        SigmaY      = 0;
        SigmaYX     = 0;
    }

    //
    // Read out functions for X and Y
    //

    long long   Y(void)
    {
        return CumulativeY;
    }

    long long   X(void)
    {
        return CumulativeX;
    }

    //
    // Add a new pair of values
    //

    void   Add(long long   DeltaY,
               long long   DeltaX)
    {
        CumulativeY   += DeltaY;
        CumulativeX   += DeltaX;
        SigmaY        += CumulativeY;
        SigmaYX       += CumulativeY * CumulativeX;
        SigmaXSquared += CumulativeX * CumulativeX;
        SigmaX        += CumulativeX;
        Count++;
    }

    //
    // Read out the Gradient
    //

    Rational_t   Gradient(void)
    {
        LongLongLong_t  Top;
        LongLongLong_t  Bottom;
        long long       TopLong;
        long long       BottomLong;
        unsigned int    TopShift;
        unsigned int    BottomShift;

        if (Count < 2)
        {
            SE_WARNING("less than 2 values\n");
            return 0;
        }

        Top = ((SigmaY * SigmaX) - (Count * SigmaYX));
        Bottom  = ((SigmaX * SigmaX) - (Count * SigmaXSquared));
        Top.Get(&TopLong, &TopShift);
        Bottom.Get(&BottomLong, &BottomShift);

        if (TopShift != BottomShift)
        {
            if (TopShift > BottomShift)
            {
                BottomLong    /= (1 << (TopShift - BottomShift));
            }
            else
            {
                TopLong       /= (1 << (BottomShift - TopShift));
            }
        }

        if (0 == BottomLong)
        {
            SE_INFO(group_misc, "G-BottomLong 0; forcing 1\n");
            BottomLong = 1;
        }

        return Rational_t(TopLong, BottomLong);
    }

    //
    // Read out the intercept
    //

    Rational_t   Intercept(void)
    {
        LongLongLong_t  Top;
        LongLongLong_t  Bottom;
        long long       TopLong;
        long long       BottomLong;
        unsigned int    TopShift;
        unsigned int    BottomShift;

        if (Count < 2)
        {
            SE_WARNING("less than 2 values\n");
            return 0;
        }

        Top = ((SigmaX * SigmaYX) - (SigmaY * SigmaXSquared));
        Bottom  = ((SigmaX * SigmaX) - (Count * SigmaXSquared));
        Top.Get(&TopLong, &TopShift);
        Bottom.Get(&BottomLong, &BottomShift);

        if (TopShift != BottomShift)
        {
            if (TopShift > BottomShift)
            {
                BottomLong /= (1 << (TopShift - BottomShift));
            }
            else
            {
                TopLong    /= (1 << (BottomShift - TopShift));
            }
        }

        if (0 == BottomLong)
        {
            SE_INFO(group_misc, "I-BottomLong 0; forcing 1\n");
            BottomLong = 1;
        }

        return Rational_t(TopLong, BottomLong);
    }

private:
    unsigned int    Count;

    long long       CumulativeX;
    long long       CumulativeY;

    LongLongLong_t  SigmaXSquared;
    LongLongLong_t  SigmaX;
    LongLongLong_t  SigmaY;
    LongLongLong_t  SigmaYX;
};

typedef LeastSquares_c LeastSquares_t;

#endif

