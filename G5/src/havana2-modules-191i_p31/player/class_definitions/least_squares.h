/************************************************************************
Copyright (C) 2009 STMicroelectronics. All Rights Reserved.

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

Source file name : least_squares.h
Author :           Nick

Definition of the class supporting least squares approximation in player 2.
The algorithm used is detailed in :-

	http://en.wikipedia.org/wiki/Linear_least_squares


NOTE the individual points here are delta's from the last point pair


Date        Modification                                    Name
----        ------------                                    --------
18-Mar-07   Created                                         Nick

************************************************************************/

#include "longlonglong.h"
#include "rational.h"

#ifndef H_LEAST_SQUARES
#define H_LEAST_SQUARES

// -------------------------------------------------------------

class LeastSquares_c
{
private:
	unsigned int		Count;

	long long		CumulativeX;
	long long		CumulativeY;

        LongLongLong_t		SigmaXSquared;
	LongLongLong_t		SigmaX;
	LongLongLong_t		SigmaY;
	LongLongLong_t		SigmaYX;

public:

    //
    // Constructor just resets the accumulation
    //

    LeastSquares_c()
    {
	Reset();
    }

    //
    // Reset the accumulation
    //

    void   Reset( void )
    {
	Count		= 0;
	CumulativeX	= 0;
	CumulativeY	= 0;
        SigmaXSquared	= 0;
	SigmaX		= 0;
	SigmaY		= 0;
	SigmaYX		= 0;
    }

    //
    // Read out functions for X and Y
    //

    long long   Y( void )
    {
	return CumulativeY;
    }

    long long   X( void )
    {
	return CumulativeX;
    }

    //
    // Add a new pair of values
    //

    void   Add(	long long	DeltaY,
		long long	DeltaX )
    {
	CumulativeY	+= DeltaY;
	CumulativeX	+= DeltaX;

	SigmaY		+= CumulativeY;
	SigmaYX		+= CumulativeY * CumulativeX;
	SigmaXSquared	+= CumulativeX * CumulativeX;
	SigmaX		+= CumulativeX;
	Count++;
    }

    //
    // Read out the Gradient
    //

    Rational_t   Gradient( void )
    {
	LongLongLong_t	Top;
	LongLongLong_t	Bottom;
	long long	TopLong;
	long long	BottomLong;
	unsigned int	TopShift;
	unsigned int	BottomShift;

	if( Count < 2 )
	{
	    report( severity_error, "LeastSquares_c::Gradient - Attepmpt to obtain least squares fit with less than 2 values\n" );
	    return 0;
	}

	Top	= ((SigmaY * SigmaX) - (Count * SigmaYX));
	Bottom	= ((SigmaX*SigmaX) - (Count * SigmaXSquared));

	Top.Get( &TopLong, &TopShift );
	Bottom.Get( &BottomLong, &BottomShift );

	if( TopShift != BottomShift )
	{
	    if( TopShift > BottomShift )
		BottomLong	/= (1<<(TopShift - BottomShift));
	    else
		TopLong		/= (1<<(BottomShift - TopShift));
	}

	return Rational_t( TopLong, BottomLong );
    }

    //
    // Read out the intercept
    //

    Rational_t   Intercept( void )
    {
	LongLongLong_t	Top;
	LongLongLong_t	Bottom;
	long long	TopLong;
	long long	BottomLong;
	unsigned int	TopShift;
	unsigned int	BottomShift;

	if( Count < 2 )
	{
	    report( severity_error, "LeastSquares_c::Intercept - Attepmpt to obtain least squares fit with less than 2 values\n" );
	    return 0;
	}

	Top	= ((SigmaX * SigmaYX) - (SigmaY * SigmaXSquared));
	Bottom	= ((SigmaX*SigmaX) - (Count * SigmaXSquared));

	Top.Get( &TopLong, &TopShift );
	Bottom.Get( &BottomLong, &BottomShift );

	if( TopShift != BottomShift )
	{
	    if( TopShift > BottomShift )
		BottomLong	/= (1<<(TopShift - BottomShift));
	    else
		TopLong		/= (1<<(BottomShift - TopShift));
	}

	return Rational_t( TopLong, BottomLong );
    }
};

//

typedef LeastSquares_c LeastSquares_t;

#endif

